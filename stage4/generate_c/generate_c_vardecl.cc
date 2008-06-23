/*
 * (c) 2003 Mario de Sousa
 *
 * Offered to the public under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * This code is made available on the understanding that it will not be
 * used in safety-critical situations without a full and competent review.
 */

/*
 * An IEC 61131-3 IL and ST compiler.
 *
 * Based on the
 * FINAL DRAFT - IEC 61131-3, 2nd Ed. (2001-12-10)
 *
 */


/*
 * Conversion of variable declaration constructs.
 *
 * This is part of the 4th stage that generates
 * a c++ source program equivalent to the IL and ST
 * code.
 */




//#include <stdio.h>  /* required for NULL */
//#include <string>
//#include <iostream>

//#include "../../util/symtable.hh"




class generate_c_array_initialization_c: public generate_c_typedecl_c {

  public:
    typedef enum {
      none_am,
      dimensioncount_am,
      initializationvalue_am,
      arrayassignment_am,
      varlistparse_am
    } arrayinitialization_mode_t;

    arrayinitialization_mode_t current_mode;
    
    symbol_c* array_default_value;

  private:
    int dimension_number, current_dimension, array_size;

  public:
    generate_c_array_initialization_c(stage4out_c *s4o_ptr): generate_c_typedecl_c(s4o_ptr) {}
    ~generate_c_array_initialization_c(void) {}

    void init_array(symbol_c *var1_list, symbol_c *array_specification, symbol_c *array_initialization) {
      int i;
      
      dimension_number = 0;
      current_dimension = 0;
      array_size = 1;
      array_default_value = NULL;
      
      current_mode = dimensioncount_am;
      array_specification->accept(*this);
      
      current_mode = initializationvalue_am;
      s4o.print("\n");
      s4o.print(s4o.indent_spaces + "{\n");
      s4o.indent_right();
      s4o.print(s4o.indent_spaces + "int index[");
      print_integer(dimension_number);
      s4o.print("];\n");
      s4o.print(s4o.indent_spaces);
      array_specification->accept(*this);
      s4o.print(" temp = ");
      array_initialization->accept(*this);
      s4o.print(";\n");
      
      current_mode = arrayassignment_am;
      array_specification->accept(*this);
      
      current_mode = varlistparse_am;
      var1_list->accept(*this);
      
      current_mode = arrayassignment_am;
      for (i = 0; i < dimension_number; i++) {
        s4o.indent_left();
        s4o.print(s4o.indent_spaces + "}\n");
      }
      s4o.indent_left();
      s4o.print(s4o.indent_spaces + "}");
    }

    void *visit(identifier_c *type_name) {
      symbol_c *type_decl;
      switch (current_mode) {
        case dimensioncount_am:
        case arrayassignment_am:
          /* look up the type declaration... */
          type_decl = type_symtable.find_value(type_name);
          if (type_decl == type_symtable.end_value())
            /* Type declaration not found!! */
            ERROR;
          type_decl->accept(*this);
          break;
        default:
          print_token(type_name);
          break;
      }
      return NULL;
    }

    void *visit(var1_list_c *symbol) {
      int i, j;
      
      for (i = 0; i < symbol->n; i++) {
        s4o.print(s4o.indent_spaces);
        print_variable_prefix();
        symbol->elements[i]->accept(*this);
        for (j = 0; j < dimension_number; j++) {
          s4o.print("[index[");
          print_integer(j);
          s4o.print("]]");
        }
        s4o.print(" = temp");
        for (j = 0; j < dimension_number; j++) {
          s4o.print("[index[");
          print_integer(j);
          s4o.print("]]");
        }
        s4o.print(";\n");
      }
      return NULL;
    }

/********************************/
/* B 1.3.3 - Derived data types */
/********************************/
    
    /* ARRAY '[' array_subrange_list ']' OF non_generic_type_name */
    void *visit(array_specification_c *symbol) {
      switch (current_mode) {
        case dimensioncount_am:
          symbol->array_subrange_list->accept(*this);
          array_default_value = (symbol_c *)symbol->non_generic_type_name->accept(*type_initial_value_c::instance());;
          break;
        default:
          symbol->array_subrange_list->accept(*this);
          break;
      } 
      return NULL;
    }
    
    /*  signed_integer DOTDOT signed_integer */
    //SYM_REF2(subrange_c, lower_limit, upper_limit)
    void *visit(subrange_c *symbol) {
      switch (current_mode) {
        case dimensioncount_am:
          dimension_number++;
          array_size *= extract_integer(symbol->upper_limit) - extract_integer(symbol->lower_limit) + 1;
          break;
        case arrayassignment_am:
          s4o.print(s4o.indent_spaces + "for (index[");
          print_integer(current_dimension);
          s4o.print("] = 0; index[");
          print_integer(current_dimension);
          s4o.print("] <= ");
          print_integer(extract_integer(symbol->upper_limit) - extract_integer(symbol->lower_limit));
          s4o.print("; index[");
          print_integer(current_dimension);
          s4o.print("]++) {\n");
          s4o.indent_right();
          current_dimension++;
          break;
        default:
          break;
      }
      return NULL;
    }
    
    /* helper symbol for array_initialization */
    /* array_initial_elements_list ',' array_initial_elements */
    void *visit(array_initial_elements_list_c *symbol) {
      switch (current_mode) {
        case initializationvalue_am:
          int i;
      
          s4o.print("{");
          for (i = 0; i < symbol->n; i++) {
            if (i > 0)
              s4o.print(", ");
            symbol->elements[i]->accept(*this);
            array_size--;
          }
          if (array_size > 0) {
            if (symbol->n > 0)
              s4o.print(", ");
            for (i = 0; i < array_size; i++) {
              if (i > 0)
                s4o.print(", ");
              array_default_value->accept(*this);
            }
          }
          s4o.print("}");
          break;
        default:
          break;
      }
      return NULL;
    }
    
    /* integer '(' [array_initial_element] ')' */
    /* array_initial_element may be NULL ! */
    void *visit(array_initial_elements_c *symbol) {
      int initial_element_number;
      
      switch (current_mode) {
        case initializationvalue_am:
          initial_element_number = extract_integer(symbol->integer);
          
          for (int i = 0; i < initial_element_number; i++) {
            if (i > 0)
              s4o.print(", ");
            if (symbol->array_initial_element != NULL)
              symbol->array_initial_element->accept(*this);
            else if (array_default_value != NULL)
              array_default_value->accept(*this);
          }
          if (initial_element_number > 1)
            array_size -= initial_element_number - 1;
          break;
        default:
          break;
      }
      return NULL;
    }

};

/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/




class generate_c_vardecl_c: protected generate_c_typedecl_c {

  /* A Helper class to the main class... */
  /* print a string, except the first time it is called */
  /* used to print the separator "," before each variable
   * declaration, except the first...
   *
   * Needs to be done in a seperate class because this is called
   * from within various locations within the code.
   */
  class next_var_c {
    private:
      bool print_flag;
      std::string str1, str2;

      next_var_c *embedded_scope;

    public:
      next_var_c(std::string s1, std::string s2) {
        str1 = s1;
        str2 = s2;
        print_flag = false;
        embedded_scope = NULL;
      }

      std::string get(void) {
        if (NULL != embedded_scope)
          return embedded_scope->get();

        bool old_print_flag = print_flag;
        print_flag = true;
        if (!old_print_flag)
          return str1;
        else
          return str2;
      }

      /* Create a new next_var_c for an embedded scope.
       * From now on, every call to get() will return the
       * inner-most scope...!
       */
      void push(std::string s1, std::string s2) {
        if (NULL != embedded_scope)
          return embedded_scope->push(s1, s2);

        embedded_scope = new next_var_c(s1, s2);
        if (NULL == embedded_scope)
          ERROR;
        return;
      }

      /* Remove the inner-most scope... */
      void pop(void) {
        if (NULL != embedded_scope)
          return embedded_scope->pop();

        delete embedded_scope;
        embedded_scope = NULL;
      }
  };

  private:
    /* used to generate the ',' separating the parameters in a function call */
    next_var_c *nv;


  public:
    /* the types of variables that need to be processed... */
    static const unsigned int none_vt	  = 0x0000;
    static const unsigned int input_vt	  = 0x0001;  // VAR_INPUT
    static const unsigned int output_vt	  = 0x0002;  // VAR_OUTPUT
    static const unsigned int inoutput_vt = 0x0004;  // VAR_IN_OUT
    static const unsigned int private_vt  = 0x0008;  // VAR
    static const unsigned int temp_vt	  = 0x0010;  // VAR_TEMP
    static const unsigned int external_vt = 0x0020;  // VAR_EXTERNAL
    static const unsigned int global_vt   = 0x0040;  // VAR_GLOBAL
						     //    Globals declared inside a resource will not be declared
						     //    unless global_vt is acompanied by resource_vt
    static const unsigned int located_vt  = 0x0080;  // VAR <var_name> AT <location>
    static const unsigned int program_vt  = 0x0100;  // PROGRAM (inside a configuration!)
						     //    Programs declared inside a resource will not be declared
						     //    unless program_vt is acompanied by resource_vt

    static const unsigned int resource_vt = 0x8000;  // RESOURCE (inside a configuration!)
                                                     //    This, just of itself, will not print out any declarations!!
						     //    It must be acompanied by either program_vt and/or global_vt

    /* How variables should be declared: as local variables or
     * variables within a function call interface.
     *
     * This will define the format of the output generated
     * by this class.
     *
     * finterface_vf: function interface parameters
     *               e.g.  f(  int a, long b, real c );
     *                         ---------------------
     *               This class/function will produce the
     *               underlined code of the above examples,
     *               and no more!!
     *
     * localinit_vf: local declaration. Will include variable
     *           initialisation if it is not a located variable.
     *           e.g.
     *                int a = 9;
     *                long b = 99;
     *                real c = 99.9;
     *
     * local_vf: local declaration. Will NOT include variable
     *           initialisation.
     *           e.g.
     *                int a;
     *                long b;
     *                real c;
     *
     * init_vf: local initialisation without declaration.
     *           e.g.
     *                a = 9;
     *                b = 99;
     *                c = 99.9;
     *
     * constructorinit_vf: initialising of member variables...
     *                e.g. for a constructor...
     *                class_name_c(void)
     *                : a(9), b(99), c(99.9)  { // code... }
     *                  --------------------
     *               This class/function will produce the
     *               underlined code of the above examples,
     *               and no more!!
     *
     * globalinit_vf: initialising of static c++ variables. These
     *                variables may have been declared as static inside
     *                a class, in which case the scope within which they were
     *                previously delcared must be passed as the second parameter
     *                to the print() function.
     *
     *                e.g.
     *                __plc_pt_c<INT, 8*sizeof(INT)> START_P::loc = __plc_pt_c<INT, 8*sizeof(INT)>("I2");
     */
    typedef enum {finterface_vf,
                    local_vf,
                    localstatic_vf,
		    localinit_vf,
		    init_vf,
		    constructorinit_vf,
		    globalinit_vf
		   } varformat_t;


  private:
    generate_c_array_initialization_c *generate_c_array_initialization;
    
    /* variable used to store the types of variables that need to be processed... */
    /* Only set in the constructor...! */
    /* Will contain a set of values of generate_c_vardecl_c::XXXX_vt */
    unsigned int wanted_vartype;
    /* variable used to store the type of variable currently being processed... */
    /* Will contain a single value of generate_c_vardecl_c::XXXX_vt */
    unsigned int current_vartype;

    /* How variables should be declared: as local variables or
     * variables within a function interface...
     */
    /* Only set in the constructor...! */
    varformat_t wanted_varformat;

    /* The number of variables already declared. */
    /* Used to declare 'void' in case no variables are declared in a function interface... */
    int finterface_var_count;



    /* Holds the references to the type and initial value
     * of the variables currently being declared.
     * Please read the comment under var1_init_decl_c for further
     * details...
     *
     * We make an effort to keep these pointers pointing to NULL
     * whenever we are outside the scope of variable declaration
     * (i.e. when we are traversing a part of the parse tree which
     * is not part of variable declaration) in order tio try to catch
     * any bugs as soon as possible.
     */
    symbol_c *current_var_type_symbol;
    symbol_c *current_var_init_symbol;
    void update_type_init(symbol_c *symbol /* a spec_init_c, subrange_spec_init_c, etc... */ ) {
      this->current_var_type_symbol = spec_init_sperator_c::get_spec(symbol);
      this->current_var_init_symbol = spec_init_sperator_c::get_init(symbol);
      if (NULL == this->current_var_type_symbol)
        ERROR;
      if (NULL == this->current_var_init_symbol) {
        /* We try to find the data type's default value... */
        this->current_var_init_symbol = (symbol_c *)this->current_var_type_symbol->accept(*type_initial_value_c::instance());
      /* Note that Function Block 'data types' do not have a default value, so we cannot abort if no default value is found! */
      /*
      if (NULL == this->current_var_init_symbol)
        ERROR;
      */
      }
    }

    void void_type_init(void) {
      this->current_var_type_symbol = NULL;
      this->current_var_init_symbol = NULL;
    }

    /* Only used when wanted_varformat == globalinit_vf
     * Holds a pointer to an identifier_c, which in turns contains
     * the identifier of the scope within which the static member was
     * declared.
     */
    symbol_c *globalnamespace;

    /* Actually produce the output where variables are declared... */
    /* Note that located variables are the exception, they
     * being declared in the located_var_decl_c visitor...
     */
    void *declare_variables(symbol_c *symbol, bool is_fb = false) {
      list_c *list = dynamic_cast<list_c *>(symbol);
      /* should NEVER EVER occur!! */
      if (list == NULL) ERROR;

      /* now to produce the c equivalent... */
      if ((wanted_varformat == local_vf) ||
          (wanted_varformat == init_vf) ||
          (wanted_varformat == localinit_vf)) {
        for(int i = 0; i < list->n; i++) {
          s4o.print(s4o.indent_spaces);
          if (wanted_varformat != init_vf) {
            this->current_var_type_symbol->accept(*this);
            s4o.print(" ");
          }
          print_variable_prefix();
          list->elements[i]->accept(*this);
          if (wanted_varformat != local_vf) {
            if (this->current_var_init_symbol != NULL) {
              s4o.print(" = ");
              this->current_var_init_symbol->accept(*this);
            }
          }
          s4o.print(";\n");
        }
      }

      if (wanted_varformat == finterface_vf) {
        for(int i = 0; i < list->n; i++) {
          finterface_var_count++;
          s4o.print(nv->get());
          s4o.print("\n" + s4o.indent_spaces);
          this->current_var_type_symbol->accept(*this);
          if ((current_vartype & (output_vt | inoutput_vt)) != 0)
            s4o.print(" &");
          else
            s4o.print(" ");
          list->elements[i]->accept(*this);
          /* We do not print the initial value at function declaration!
           * It is up to the caller to pass the correct default value
           * if none is specified in the ST source code
           */
          /* if (this->current_var_init_symbol != NULL) {
               s4o.print(" = "); this->current_var_init_symbol->accept(*this);}
           */
        }
      }

      if (wanted_varformat == constructorinit_vf) {
        for(int i = 0; i < list->n; i++) {
          if (is_fb) {
            s4o.print(nv->get());
            this->current_var_type_symbol->accept(*this);
            s4o.print(FB_INIT_SUFFIX);
            s4o.print("(&");
            this->print_variable_prefix();
            list->elements[i]->accept(*this);
            s4o.print(");");
          }
          else if (this->current_var_init_symbol != NULL) {
            s4o.print(nv->get());
            this->print_variable_prefix();
            list->elements[i]->accept(*this);
            s4o.print(" = ");
            this->current_var_init_symbol->accept(*this);
            s4o.print(";");
          }
        }
      }

      return NULL;
    }




  public:
    generate_c_vardecl_c(stage4out_c *s4o_ptr, varformat_t varformat, unsigned int vartype)
    : generate_c_typedecl_c(s4o_ptr) {
      generate_c_array_initialization = new generate_c_array_initialization_c(s4o_ptr);
      wanted_varformat = varformat;
      wanted_vartype   = vartype;
      current_vartype  = none_vt;
      current_var_type_symbol = NULL;
      current_var_init_symbol = NULL;
      globalnamespace         = NULL;
      nv = NULL;
    }

    ~generate_c_vardecl_c(void) {
      delete generate_c_array_initialization;
    }


    void print(symbol_c *symbol, symbol_c *scope = NULL, const char *variable_prefix = NULL) {
      this->set_variable_prefix(variable_prefix);
      this->generate_c_array_initialization->set_variable_prefix(variable_prefix);
      if (globalinit_vf == wanted_varformat)
        globalnamespace = scope;

      finterface_var_count = 0;

      switch (wanted_varformat) {
        case constructorinit_vf: nv = new next_var_c("", "\n"+s4o.indent_spaces); break;
        case finterface_vf:      /* fall through... */
        case localinit_vf:       /* fall through... */
        case local_vf:           nv = new next_var_c("", ", "); break;
        default:                 nv = NULL;
      } /* switch() */

      symbol->accept(*this);

      /* special case... */
      if (wanted_varformat == finterface_vf)
        if (finterface_var_count == 0)
          s4o.print("void");

      delete nv;
      nv = NULL;
      globalnamespace = NULL;
    }


  protected:
/***************************/
/* B 0 - Programming Model */
/***************************/
  /* leave for derived classes... */

/*************************/
/* B.1 - Common elements */
/*************************/
/*******************************************/
/* B 1.1 - Letters, digits and identifiers */
/*******************************************/
  /* done in base class(es) */

/*********************/
/* B 1.2 - Constants */
/*********************/
  /* originally empty... */

/******************************/
/* B 1.2.1 - Numeric Literals */
/******************************/
  /* done in base class(es) */

/*******************************/
/* B.1.2.2   Character Strings */
/*******************************/
  /* done in base class(es) */

/***************************/
/* B 1.2.3 - Time Literals */
/***************************/
/************************/
/* B 1.2.3.1 - Duration */
/************************/
  /* done in base class(es) */

/************************************/
/* B 1.2.3.2 - Time of day and Date */
/************************************/
  /* done in base class(es) */

/**********************/
/* B.1.3 - Data types */
/**********************/
/***********************************/
/* B 1.3.1 - Elementary Data Types */
/***********************************/
  /* done in base class(es) */

/********************************/
/* B.1.3.2 - Generic data types */
/********************************/
  /* originally empty... */

/********************************/
/* B 1.3.3 - Derived data types */
/********************************/
  /* done in base class(es) */

/*********************/
/* B 1.4 - Variables */
/*********************/
  /* done in base class(es) */

/********************************************/
/* B.1.4.1   Directly Represented Variables */
/********************************************/
  /* done in base class(es) */

/*************************************/
/* B.1.4.2   Multi-element Variables */
/*************************************/
  /* done in base class(es) */

/******************************************/
/* B 1.4.3 - Declaration & Initialisation */
/******************************************/
void *visit(constant_option_c *symbol) {s4o.print("CONSTANT"); return NULL;}
void *visit(retain_option_c *symbol) {s4o.print("RETAIN"); return NULL;}
void *visit(non_retain_option_c *symbol) {s4o.print("NON_RETAIN"); return NULL;}

void *visit(input_declarations_c *symbol) {
  TRACE("input_declarations_c");
  if ((wanted_vartype & input_vt) != 0) {
/*
    // TO DO ...
    if (symbol->option != NULL)
      symbol->option->accept(*this);
*/
    //s4o.indent_right();
    current_vartype = input_vt;
    symbol->input_declaration_list->accept(*this);
    current_vartype = none_vt;
    //s4o.indent_left();
  }
  return NULL;
}

/* helper symbol for input_declarations */
void *visit(input_declaration_list_c *symbol) {
  TRACE("input_declaration_list_c");
  print_list(symbol);
  return NULL;
}

void *visit(edge_declaration_c *symbol) {
  TRACE("edge_declaration_c");
  // TO DO ...
  symbol->var1_list->accept(*this);
  s4o.print(" : BOOL ");
  symbol->edge->accept(*this);
  return NULL;
}

void *visit(raising_edge_option_c *symbol) {
  // TO DO ...
  s4o.print("R_EDGE");
  return NULL;
}

/*  var1_list ':' array_spec_init */
// SYM_REF2(array_var_init_decl_c, var1_list, array_spec_init)
void *visit(array_var_init_decl_c *symbol) {
  TRACE("array_var_init_decl_c");
  /* Please read the comments inside the var1_init_decl_c
   * visitor, as they apply here too.
   */

  /* Start off by setting the current_var_type_symbol and
   * current_var_init_symbol private variables...
   */
  update_type_init(symbol->array_spec_init);

  /* now to produce the c equivalent... */
  if (wanted_varformat == constructorinit_vf)
    generate_c_array_initialization->init_array(symbol->var1_list, this->current_var_type_symbol, this->current_var_init_symbol);
  else
    symbol->var1_list->accept(*this);

  /* Values no longer in scope, and therefore no longer used.
   * Make an effort to keep them set to NULL when not in use
   * in order to catch bugs as soon as possible...
   */
  void_type_init();

  return NULL;
}

/*  var1_list ':' initialized_structure */
// SYM_REF2(structured_var_init_decl_c, var1_list, initialized_structure)
void *visit(structured_var_init_decl_c *symbol) {
  TRACE("structured_var_init_decl_c");
  /* Please read the comments inside the var1_init_decl_c
   * visitor, as they apply here too.
   */

  /* Start off by setting the current_var_type_symbol and
   * current_var_init_symbol private variables...
   */
  update_type_init(symbol->initialized_structure);

  /* now to produce the c equivalent... */
  symbol->var1_list->accept(*this);

  /* Values no longer in scope, and therefore no longer used.
   * Make an effort to keep them set to NULL when not in use
   * in order to catch bugs as soon as possible...
   */
  void_type_init();

  return NULL;
}

/* fb_name_list ':' function_block_type_name ASSIGN structure_initialization */
/* structure_initialization -> may be NULL ! */
void *visit(fb_name_decl_c *symbol) {
  TRACE("fb_name_decl_c");
  /* Please read the comments inside the var1_init_decl_c
   * visitor, as they apply here too.
   */

  /* Start off by setting the current_var_type_symbol and
   * current_var_init_symbol private variables...
   */
  update_type_init(symbol);

  /* now to produce the c equivalent... */
  symbol->fb_name_list->accept(*this);

  /* Values no longer in scope, and therefore no longer used.
   * Make an effort to keep them set to NULL when not in use
   * in order to catch bugs as soon as possible...
   */
  void_type_init();

  return NULL;
}

/* fb_name_list ',' fb_name */
void *visit(fb_name_list_c *symbol) {
  TRACE("fb_name_list_c");
  declare_variables(symbol, true);
  return NULL;
}


/* VAR_OUTPUT [RETAIN | NON_RETAIN] var_init_decl_list END_VAR */
/* option -> may be NULL ! */
void *visit(output_declarations_c *symbol) {
  TRACE("output_declarations_c");
  if ((wanted_vartype & output_vt) != 0) {
/*
    // TO DO ...
    if (symbol->option != NULL)
      symbol->option->accept(*this);
*/
    //s4o.indent_right();
    current_vartype = output_vt;
    symbol->var_init_decl_list->accept(*this);
    current_vartype = none_vt;
    //s4o.indent_left();
  }
  return NULL;
}

/*  VAR_IN_OUT var_declaration_list END_VAR */
void *visit(input_output_declarations_c *symbol) {
  TRACE("input_output_declarations_c");
  if ((wanted_vartype & inoutput_vt) != 0) {
    //s4o.indent_right();
    current_vartype = inoutput_vt;
    symbol->var_declaration_list->accept(*this);
    current_vartype = none_vt;
    //s4o.indent_left();
  }
  return NULL;
}

/* helper symbol for input_output_declarations */
/* var_declaration_list var_declaration ';' */
void *visit(var_declaration_list_c *symbol) {
  TRACE("var_declaration_list_c");
  print_list(symbol);
  return NULL;
}

#if 0
/*  var1_list ':' array_specification */
SYM_REF2(array_var_declaration_c, var1_list, array_specification)
#endif

void *visit(array_initial_elements_list_c *symbol) {
  s4o.print(";// array initialisation");
  return NULL;
}

/*  var1_list ':' structure_type_name */
//SYM_REF2(structured_var_declaration_c, var1_list, structure_type_name)
void *visit(structured_var_declaration_c *symbol) {
  TRACE("structured_var_declaration_c");
  /* Please read the comments inside the var1_init_decl_c
   * visitor, as they apply here too.
   */

  /* Start off by setting the current_var_type_symbol and
   * current_var_init_symbol private variables...
   */
  update_type_init(symbol->structure_type_name);

  /* now to produce the c equivalent... */
  symbol->var1_list->accept(*this);

  /* Values no longer in scope, and therefore no longer used.
   * Make an effort to keep them set to NULL when not in use
   * in order to catch bugs as soon as possible...
   */
  void_type_init();

  return NULL;
}


/* VAR [CONSTANT] var_init_decl_list END_VAR */
/* option -> may be NULL ! */
/* helper symbol for input_declarations */
void *visit(var_declarations_c *symbol) {
  TRACE("var_declarations_c");
  if ((wanted_vartype & private_vt) != 0) {
/*
    // TO DO ...
    if (symbol->option != NULL)
      symbol->option->accept(*this);
*/
    current_vartype = private_vt;
    symbol->var_init_decl_list->accept(*this);
    current_vartype = none_vt;
  }
  return NULL;
}

/*  VAR RETAIN var_init_decl_list END_VAR */
void *visit(retentive_var_declarations_c *symbol) {
  TRACE("retentive_var_declarations_c");
  if ((wanted_vartype & private_vt) != 0) {
    current_vartype = private_vt;
    symbol->var_init_decl_list->accept(*this);
    current_vartype = none_vt;
  }
  return NULL;
}

/*  VAR [CONSTANT|RETAIN|NON_RETAIN] located_var_decl_list END_VAR */
/* option -> may be NULL ! */
//SYM_REF2(located_var_declarations_c, option, located_var_decl_list)
void *visit(located_var_declarations_c *symbol) {
  TRACE("located_var_declarations_c");
  if ((wanted_vartype & located_vt) != 0) {
/*
    // TO DO ...
    if (symbol->option != NULL)
      symbol->option->accept(*this);
*/
    current_vartype = located_vt;
    symbol->located_var_decl_list->accept(*this);
    current_vartype = none_vt;
  }
  return NULL;
}

/* helper symbol for located_var_declarations */
/* located_var_decl_list located_var_decl ';' */
//SYM_LIST(located_var_decl_list_c)
void *visit(located_var_decl_list_c *symbol) {
  TRACE("located_var_decl_list_c");
  print_list(symbol);
  return NULL;
}


/*  [variable_name] location ':' located_var_spec_init */
/* variable_name -> may be NULL ! */
//SYM_REF4(located_var_decl_c, variable_name, location, located_var_spec_init, unused)
void *visit(located_var_decl_c *symbol) {
  TRACE("located_var_decl_c");
  /* Please read the comments inside the var1_init_decl_c
   * visitor, as they apply here too.
   */

  /* Start off by setting the current_var_type_symbol and
   * current_var_init_symbol private variables...
   */
  update_type_init(symbol->located_var_spec_init);

  /* now to produce the c equivalent... */
  switch(wanted_varformat) {
    case local_vf:
      s4o.print(s4o.indent_spaces);
      this->current_var_type_symbol->accept(*this);
      s4o.print(" *");
      if (symbol->variable_name != NULL)
        symbol->variable_name->accept(*this);
      else
        symbol->location->accept(*this);
      s4o.print(";\n");
      break;

    case constructorinit_vf:
      s4o.print(nv->get());
      s4o.print("{extern ");
      this->current_var_type_symbol->accept(*this);
      s4o.print("* ");
      symbol->location->accept(*this);
      s4o.print("; ");
      print_variable_prefix();
      if (symbol->variable_name != NULL)
        symbol->variable_name->accept(*this);
      else
        symbol->location->accept(*this);
      s4o.print(" = ");
      symbol->location->accept(*this);
      s4o.print(";}");
      break;

    case globalinit_vf:
      s4o.print(s4o.indent_spaces + "__plc_pt_c<");
      this->current_var_type_symbol->accept(*this);
      s4o.print(", 8*sizeof(");
      this->current_var_type_symbol->accept(*this);
      s4o.print(")> ");
      if (this->globalnamespace != NULL) {
        this->globalnamespace->accept(*this);
        s4o.print("::");
      }
      if (symbol->variable_name != NULL)
        symbol->variable_name->accept(*this);
      else
        symbol->location->accept(*this);

      s4o.print(" = ");

      s4o.print(s4o.indent_spaces + "__plc_pt_c<");
      this->current_var_type_symbol->accept(*this);
      s4o.print(", 8*sizeof(");
      this->current_var_type_symbol->accept(*this);
      s4o.print(")>(\"");
      symbol->location->accept(*this);
      s4o.print("\"");
      if (this->current_var_init_symbol != NULL) {
        s4o.print(", ");
        this->current_var_init_symbol->accept(*this);
      }
      s4o.print(");\n");
      break;

    default:
      ERROR;
  } /* switch() */

  /* Values no longer in scope, and therefore no longer used.
   * Make an effort to keep them set to NULL when not in use
   * in order to catch bugs as soon as possible...
   */
  void_type_init();

  return NULL;
}




/*| VAR_EXTERNAL [CONSTANT] external_declaration_list END_VAR */
/* option -> may be NULL ! */
//SYM_REF2(external_var_declarations_c, option, external_declaration_list)
void *visit(external_var_declarations_c *symbol) {
  TRACE("external_var_declarations_c");
  if ((wanted_vartype & external_vt) != 0) {
/*
    // TODO ...
    if (symbol->option != NULL)
      symbol->option->accept(*this);
*/
    //s4o.indent_right();
    current_vartype = external_vt;
    symbol->external_declaration_list->accept(*this);
    current_vartype = none_vt;
    //s4o.indent_left();
  }
  return NULL;
}

/* helper symbol for external_var_declarations */
/*| external_declaration_list external_declaration';' */
//SYM_LIST(external_declaration_list_c)
/* helper symbol for input_declarations */
void *visit(external_declaration_list_c *symbol) {
  TRACE("external_declaration_list_c");
  print_list(symbol);
  return NULL;
}


/*  global_var_name ':' (simple_specification|subrange_specification|enumerated_specification|array_specification|prev_declared_structure_type_name|function_block_type_name */
//SYM_REF2(external_declaration_c, global_var_name, specification)
void *visit(external_declaration_c *symbol) {
  TRACE("external_declaration_c");
  /* Please read the comments inside the var1_init_decl_c
   * visitor, as they apply here too.
   */

  /* Start off by setting the current_var_type_symbol and
   * current_var_init_symbol private variables...
   */
  this->current_var_type_symbol = symbol->specification;
  this->current_var_init_symbol = NULL;

  /* now to produce the c equivalent... */
  switch (wanted_varformat) {
    case local_vf:
    case localinit_vf:
      s4o.print(s4o.indent_spaces);
      this->current_var_type_symbol->accept(*this);
      s4o.print(" *");
      symbol->global_var_name->accept(*this);
      if ((wanted_varformat == localinit_vf) &&
          (this->current_var_init_symbol != NULL)) {
        s4o.print(" = ");
        this->current_var_init_symbol->accept(*this);
      }
      s4o.print(";\n");
      break;

    case constructorinit_vf:
      s4o.print(nv->get());
      s4o.print("{extern ");
      this->current_var_type_symbol->accept(*this);
      s4o.print(" *");
      symbol->global_var_name->accept(*this);
      s4o.print("; ");
      print_variable_prefix();
      symbol->global_var_name->accept(*this);
      s4o.print(" = ");
      symbol->global_var_name->accept(*this);
      s4o.print(";}");
      break;

    case finterface_vf:
      finterface_var_count++;
      s4o.print(nv->get());
      this->current_var_type_symbol->accept(*this);
      s4o.print(" *");
      symbol->global_var_name->accept(*this);
      break;

    default:
      ERROR;
  }

  /* Values no longer in scope, and therefore no longer used.
   * Make an effort to keep them set to NULL when not in use
   * in order to catch bugs as soon as possible...
   */
  void_type_init();

  return NULL;
}



/*| VAR_GLOBAL [CONSTANT|RETAIN] global_var_decl_list END_VAR */
/* option -> may be NULL ! */
// SYM_REF2(global_var_declarations_c, option, global_var_decl_list)
void *visit(global_var_declarations_c *symbol) {
  TRACE("global_var_declarations_c");
  if ((wanted_vartype & global_vt) != 0) {
/*
    // TODO ...
    if (symbol->option != NULL)
      symbol->option->accept(*this);
*/
    //s4o.indent_right();
    unsigned int previous_vartype = current_vartype;
      // previous_vartype will be either none_vt, or resource_vt
    current_vartype = global_vt;
    symbol->global_var_decl_list->accept(*this);
    current_vartype = previous_vartype;
    //s4o.indent_left();
  }
  return NULL;
}

/* helper symbol for global_var_declarations */
/*| global_var_decl_list global_var_decl ';' */
//SYM_LIST(global_var_decl_list_c)
void *visit(global_var_decl_list_c *symbol) {
  TRACE("global_var_decl_list_c");
  print_list(symbol);
  return NULL;
}



/*| global_var_spec ':' [located_var_spec_init|function_block_type_name] */
/* type_specification ->may be NULL ! */
// SYM_REF2(global_var_decl_c, global_var_spec, type_specification)
void *visit(global_var_decl_c *symbol) {
  TRACE("global_var_decl_c");
  /* Please read the comments inside the var1_init_decl_c
   * visitor, as they apply here too.
   */

  /* Start off by setting the current_var_type_symbol and
   * current_var_init_symbol private variables...
   */
  update_type_init(symbol->type_specification);

  /* now to produce the c equivalent... */
  symbol->global_var_spec->accept(*this);

  /* Values no longer in scope, and therefore no longer used.
   * Make an effort to keep them set to NULL when not in use
   * in order to catch bugs as soon as possible...
   */
  void_type_init();

  return NULL;
}


/*| global_var_name location */
// SYM_REF2(global_var_spec_c, global_var_name, location)
void *visit(global_var_spec_c *symbol) {
  TRACE("global_var_spec_c");

  /* now to produce the c equivalent... */
  switch(wanted_varformat) {
    case local_vf:
    case localstatic_vf:
      /* NOTE: located variables must be declared static, as the connection to the
       * MatPLC point must be initialised at program startup, and not whenever
       * a new function block is instantiated!
       * Nevertheless, this construct never occurs inside a Function Block, but
       * only inside a configuration. In this case, only a single instance will
       * be created, directly at startup, so it is not necessary that the variables
       * be declared static.
       */
      s4o.print(s4o.indent_spaces);
      if (symbol->global_var_name != NULL) {
        s4o.print("extern ");
        this->current_var_type_symbol->accept(*this);
        s4o.print("* ");
        symbol->location->accept(*this);
        s4o.print(";\n");
        if (wanted_varformat == localstatic_vf)
          s4o.print("static ");
        this->current_var_type_symbol->accept(*this);
        s4o.print(" *");
        symbol->global_var_name->accept(*this);
        s4o.print(" = ");
        symbol->location->accept(*this);
        s4o.print(";\n");
      }
      break;

    case constructorinit_vf:
      s4o.print(nv->get());
      
      if (symbol->global_var_name != NULL) {
        s4o.print("*");
        symbol->global_var_name->accept(*this);
      }
      else
        symbol->location->accept(*this);
      s4o.print(" = ");
      if (this->current_var_init_symbol != NULL) {
        this->current_var_init_symbol->accept(*this);
      }
      s4o.print(";");
      break;

    default:
      ERROR;
  } /* switch() */

  return NULL;
}



/*  AT direct_variable */
// SYM_REF2(location_c, direct_variable, unused)
void *visit(location_c *symbol) {
  TRACE("location_c");
  return symbol->direct_variable->accept(*this);
}


/*| global_var_list ',' global_var_name */
//SYM_LIST(global_var_list_c)
void *visit(global_var_list_c *symbol) {
  TRACE("global_var_list_c");
  list_c *list = dynamic_cast<list_c *>(symbol);
  /* should NEVER EVER occur!! */
  if (list == NULL) ERROR;

  /* now to produce the c equivalent... */
  switch (wanted_varformat) {
    case local_vf:
    case localinit_vf:
      for(int i = 0; i < list->n; i++) {
        s4o.print(s4o.indent_spaces);
        this->current_var_type_symbol->accept(*this);
        s4o.print(" __");
        list->elements[i]->accept(*this);
        s4o.print(";\n");
        this->current_var_type_symbol->accept(*this);
        s4o.print(" *");
        list->elements[i]->accept(*this);
        s4o.print(" = &__");
        list->elements[i]->accept(*this);
#if 0
        if (wanted_varformat == localinit_vf) {
          if (this->current_var_init_symbol != NULL) {
            s4o.print(" = ");
            this->current_var_init_symbol->accept(*this);
          }
        }
#endif
        s4o.print(";\n");
      }
      break;

    case constructorinit_vf:
      if (this->current_var_init_symbol != NULL) {
        for(int i = 0; i < list->n; i++) {
          s4o.print(nv->get());

          s4o.print("*");
          list->elements[i]->accept(*this);
          s4o.print(" = ");
          this->current_var_init_symbol->accept(*this);
          s4o.print(";");
#if 0
 	  /* The following code would be for globalinit_vf !!
	   * But it is not currently required...
	   */
	  s4o.print(s4o.indent_spaces + "__ext_element_c<");
          this->current_var_type_symbol->accept(*this);
          s4o.print("> ");
          if (this->globalnamespace != NULL) {
            this->globalnamespace->accept(*this);
            s4o.print("::");
          }
          list->elements[i]->accept(*this);

          if (this->current_var_init_symbol != NULL) {
            s4o.print(" = ");
            s4o.print("__ext_element_c<");
            this->current_var_type_symbol->accept(*this);
            s4o.print(">(");
            this->current_var_init_symbol->accept(*this);
            s4o.print(")");
	  }
          s4o.print(";\n");
#endif
        }
      }
      break;

    default:
      ERROR; /* not supported, and not needed either... */
  }

  return NULL;
}


#if 0
/*  var1_list ':' single_byte_string_spec */
SYM_REF2(single_byte_string_var_declaration_c, var1_list, single_byte_string_spec)

/*  STRING ['[' integer ']'] [ASSIGN single_byte_character_string] */
/* integer ->may be NULL ! */
/* single_byte_character_string ->may be NULL ! */
SYM_REF2(single_byte_string_spec_c, integer, single_byte_character_string)

/*  var1_list ':' double_byte_string_spec */
SYM_REF2(double_byte_string_var_declaration_c, var1_list, double_byte_string_spec)

/*  WSTRING ['[' integer ']'] [ASSIGN double_byte_character_string] */
/* integer ->may be NULL ! */
/* double_byte_character_string ->may be NULL ! */
SYM_REF2(double_byte_string_spec_c, integer, double_byte_character_string)

/*| VAR [RETAIN|NON_RETAIN] incompl_located_var_decl_list END_VAR */
/* option ->may be NULL ! */
SYM_REF2(incompl_located_var_declarations_c, option, incompl_located_var_decl_list)

/* helper symbol for incompl_located_var_declarations */
/*| incompl_located_var_decl_list incompl_located_var_decl ';' */
SYM_LIST(incompl_located_var_decl_list_c)

/*  variable_name incompl_location ':' var_spec */
SYM_REF4(incompl_located_var_decl_c, variable_name, incompl_location, var_spec, unused)

/*  AT incompl_location_token */
SYM_TOKEN(incompl_location_c)
#endif


void *visit(falling_edge_option_c *symbol) {
  // TO DO ...
  s4o.print("F_EDGE");
  return NULL;
}


void *visit(var1_init_decl_c *symbol) {
  TRACE("var1_init_decl_c");
  /* We have several implementation alternatives here...
   *
   * The issue is that generation of c code
   * based on the abstract syntax tree requires the reversal
   * of the symbol order compared to st. For e.g.:
   * (ST) a, b, c: INT := 98;
   * (C ) int a=98, b=98, c=98;
   * The spec_init contains the references to 'INT' and '98'.
   * The var1_list contains the references to 'a', 'b', and 'c'.
   * The var1_init_decl_c contains the references to spec_init and var1_list.
   *
   * For c code generation, the var1_init_decl_c visitor
   * would need to access the internals of other classes
   * (e.g. the simple_spec_init_c).
   *
   * We can do this using one of three methods:
   *   1) Create the abstract syntax tree differently;
   *   2) Access other classes from within the var1_init_decl_c;
   *   3) Pass info between the visitors using global variables
   *       only acessible by this class (private vars)
   *
   * In 1), the abstract syntax tree would be built so that
   * var1_init_decl_c would contain direct references to
   * var1_list_c, to the var type 'INT' and to the initialiser '98'
   * (as per the example above).
   *
   * 2) would have several sub-options to obtain the references
   * to 'INT' and '98' from within var1_init_decl_c.
   *  In 2a), the var1_init_decl_c would use dynamic casts to determine
   * the class of spec_init (simple_spec_init_c, subrange_spec_init_c or
   * enumerated_spec_init_c), and from there obtain the 'INT' and '98'
   *  In 2b) var1_init_decl_c would have one reference for each
   * simple_spec_init_c, subrange_spec_init_c and enumerated_spec_init_c,
   * the apropriate one being initialised by the var1_init_decl_c constructor.
   * Note that the constructor would use dynamic casts. In essence, we
   * would merely be changing the location of the code with the
   * dynamic casts.
   *  In 2c) we would use three overloaded constructors for var1_init_decl_c
   * one each for simple_spec_init_c, etc... This implies
   * use type specific pointers to each symbol in the bison
   * parser, instead of simply using a symbol_c * for all symbols;
   *
   * In 3), we use two global but private variables with references to
   * 'INT' and '98', that would be initiliased by the visitors to
   * simple_spec_init_c, subrange_spec_init_c and enumerated_spec_init_c.
   * The visitor to var1_list_c would then use the references in the global
   * variables.
   *
   * I (Mario) have chosen to use 3).
   */

  /* Start off by setting the current_var_type_symbol and
   * current_var_init_symbol private variables...
   */
  update_type_init(symbol->spec_init);

  /* now to produce the c equivalent... */
  symbol->var1_list->accept(*this);

  /* Values no longer in scope, and therefore no longer used.
   * Make an effort to keep them set to NULL when not in use
   * in order to catch bugs as soon as possible...
   */
  void_type_init();

  return NULL;
}



void *visit(var1_list_c *symbol) {
  TRACE("var1_list_c");
  declare_variables(symbol);
  return NULL;
}




/* intermediate helper symbol for:
 *  - non_retentive_var_decls
 *  - output_declarations
 */
void *visit(var_init_decl_list_c *symbol) {
  TRACE("var_init_decl_list_c");
  return print_list(symbol);
  return NULL;
}


/**************************************/
/* B.1.5 - Program organization units */
/**************************************/
/***********************/
/* B 1.5.1 - Functions */
/***********************/
/* The missing function_declaration_c
 * is handled in derived classes
 */



/* intermediate helper symbol for function_declaration */
void *visit(var_declarations_list_c *symbol) {
  TRACE("var_declarations_list_c");
  return print_list(symbol);
}


void *visit(function_var_decls_c *symbol) {
  TRACE("function_var_decls_c");

  if ((wanted_vartype & private_vt) != 0) {
/*
    // TO DO ...
    if (symbol->option != NULL)
      symbol->option->accept(*this);
*/
    current_vartype = private_vt;
    symbol->decl_list->accept(*this);
    current_vartype = none_vt;
  }
  return NULL;
}


/* intermediate helper symbol for function_var_decls */
void *visit(var2_init_decl_list_c *symbol) {
  TRACE("var2_init_decl_list_c");
  print_list(symbol);
  return NULL;
}


/*****************************/
/* B 1.5.2 - Function Blocks */
/*****************************/

/* The missing function_block_declaration_c
 * is handled in derived classes
 */


/*  VAR_TEMP temp_var_decl_list END_VAR */
void *visit(temp_var_decls_c *symbol) {
  TRACE("temp_var_decls_c");
  if ((wanted_vartype & temp_vt) != 0) {
    current_vartype = temp_vt;
    symbol->var_decl_list->accept(*this);
    current_vartype = none_vt;
  }
  return NULL;
}

/* intermediate helper symbol for temp_var_decls */
void *visit(temp_var_decls_list_c *symbol) {
  TRACE("temp_var_decls_list_c");
  return print_list(symbol);
}

/*  VAR NON_RETAIN var_init_decl_list END_VAR */
void *visit(non_retentive_var_decls_c *symbol) {
  TRACE("non_retentive_var_decls_c");
  // TODO ... guarantee the non-retain semantics!
  if ((wanted_vartype & private_vt) != 0) {
    current_vartype = private_vt;
    symbol->var_decl_list->accept(*this);
    current_vartype = none_vt;
  }
  return NULL;
}



/**********************/
/* B 1.5.3 - Programs */
/**********************/
  /* leave for derived classes... */

/*********************************************/
/* B.1.6  Sequential function chart elements */
/*********************************************/

/********************************/
/* B 1.7 Configuration elements */
/********************************/
  /* Programs instantiated inside configurations are declared as variables!! */

/*
CONFIGURATION configuration_name
   optional_global_var_declarations
   (resource_declaration_list | single_resource_declaration)
   optional_access_declarations
   optional_instance_specific_initializations
END_CONFIGURATION
*/
/*
SYM_REF6(configuration_declaration_c, configuration_name, global_var_declarations, resource_declarations, access_declarations, instance_specific_initializations, unused)
*/
void *visit(configuration_declaration_c *symbol) {
  TRACE("configuration_declaration_c");

  if(symbol->global_var_declarations)
    symbol->global_var_declarations->accept(*this); // will contain VAR_GLOBAL declarations!!
  symbol->resource_declarations->accept(*this);   // will contain PROGRAM declarations!!
  return NULL;
}


/* helper symbol for configuration_declaration */
// SYM_LIST(resource_declaration_list_c)
void *visit(resource_declaration_list_c *symbol) {
  TRACE("resource_declaration_list_c");

  return print_list(symbol);
}

/*
RESOURCE resource_name ON resource_type_name
   optional_global_var_declarations
   single_resource_declaration
END_RESOURCE
*/
// SYM_REF4(resource_declaration_c, resource_name, resource_type_name, global_var_declarations, resource_declaration)
void *visit(resource_declaration_c *symbol) {
  TRACE("resource_declaration_c");

  if ((wanted_vartype & resource_vt) != 0) {
    s4o.print(s4o.indent_spaces + "struct {\n");
    s4o.indent_right();

    current_vartype = resource_vt;
    if (NULL != symbol->global_var_declarations)
      symbol->global_var_declarations->accept(*this); // will contain VAR_GLOBAL declarations!!
    if (NULL != symbol->resource_declaration)
      symbol->resource_declaration->accept(*this);    // will contain PROGRAM declarations!!
    current_vartype = none_vt;

    s4o.indent_left();
    s4o.print(s4o.indent_spaces + "} ");
    symbol->resource_name->accept(*this);
    s4o.print(";\n");
  }
  return NULL;
}

/* task_configuration_list program_configuration_list */
// SYM_REF2(single_resource_declaration_c, task_configuration_list, program_configuration_list)
void *visit(single_resource_declaration_c *symbol) {
  TRACE("single_resource_declaration_c");

  if ((wanted_vartype & program_vt) != 0) {
    unsigned int previous_vartype = current_vartype;
      // previous_vartype will be resource_vt
    current_vartype = program_vt;
    symbol->program_configuration_list->accept(*this);
    current_vartype = previous_vartype;
  }
  return NULL;
}


/* helper symbol for single_resource_declaration */
// SYM_LIST(task_configuration_list_c)


/* helper symbol for single_resource_declaration */
/* | program_configuration_list program_configuration ';' */
// SYM_LIST(program_configuration_list_c)
void *visit(program_configuration_list_c *symbol) {
  TRACE("program_configuration_list_c");

  return print_list(symbol);
}

/* helper symbol for
 *  - access_path
 *  - instance_specific_init
 */
// SYM_LIST(any_fb_name_list_c)

/*  [resource_name '.'] global_var_name ['.' structure_element_name] */
// SYM_REF4(global_var_reference_c, resource_name, global_var_name, structure_element_name, unused)

/*  prev_declared_program_name '.' symbolic_variable */
// SYM_REF2(program_output_reference_c, program_name, symbolic_variable)

/*  TASK task_name task_initialization */
// SYM_REF2(task_configuration_c, task_name, task_initialization)

/*  '(' [SINGLE ASSIGN data_source ','] [INTERVAL ASSIGN data_source ','] PRIORITY ASSIGN integer ')' */
// SYM_REF4(task_initialization_c, single_data_source, interval_data_source, priority_data_source, unused)

/*  PROGRAM [RETAIN | NON_RETAIN] program_name [WITH task_name] ':' program_type_name ['(' prog_conf_elements ')'] */
// SYM_REF6(program_configuration_c, retain_option, program_name, task_name, program_type_name, prog_conf_elements, unused)
private:
  /* a helper function to the program_configuration_c visitor... */
  void program_constructor_call(program_configuration_c *symbol) {
  program_declaration_c *p_decl = program_type_symtable.find_value(symbol->program_type_name);

  if (p_decl == program_type_symtable.end_value())
    /* should never occur. The program being called MUST be in the symtable... */
    ERROR;

  symbol->program_name->accept(*this);
  s4o.print("(");

  /* loop through each function parameter, find the value we should pass
   * to it, and then output the c equivalent...
   */
  function_param_iterator_c fp_iterator(p_decl);
  function_call_param_iterator_c function_call_param_iterator(symbol);
  identifier_c *param_name;
  nv->push("", ", ");
  for(int i = 1; (param_name = fp_iterator.next()) != NULL; i++) {

    symbol_c *param_type = fp_iterator.param_type();
    if (param_type == NULL) ERROR;

    function_param_iterator_c::param_direction_t param_direction = fp_iterator.param_direction();

#if 0
    /* Get the value from a foo(<param_name> = <param_value>) style call */
    symbol_c *param_value = function_call_param_iterator.search(param_name);
#endif

    switch (param_direction) {
      case function_param_iterator_c::direction_in:
      case function_param_iterator_c::direction_out:
      case function_param_iterator_c::direction_inout:
        /* ignore them all!! */
        break;

      case function_param_iterator_c::direction_extref:
#if 0
        if (param_value == NULL)
	  /* This is illegal in ST and IL languages.
	   * All variables declared in a VAR_EXTERNAL __must__
	   * be initialised to reference a specific VAR_GLOBAL variable!!
	   *
	   * The semantic checker should have caught this, we check again just the
	   * same (especially since the semantic checker has not yet been written!).
	   */
	  ERROR;
        s4o.print(nv->get());
        s4o.print("&");
        param_value->accept(*this);
#endif
	break;
#if 0
        if (param_value == NULL) {
	  /* no parameter value given, so we pass a previously declared temporary variable. */
          std::string *temp_var_name = temp_var_name_factory.new_name();
          s4o.print(*temp_var_name);
          delete temp_var_name;
	} else {
          param_value->accept(*this);
	}
#endif
	break;
    } /* switch */
  } /* for(...) */

  // symbol->parameter_assignment->accept(*this);
  s4o.print(")");
  nv->pop();
  return;
}


public:
void *visit(program_configuration_c *symbol) {
  TRACE("program_configuration_c");

  /* now to produce the c equivalent... */
  switch (wanted_varformat) {
    case local_vf:
    case localinit_vf:
      s4o.print(s4o.indent_spaces);
      symbol->program_type_name->accept(*this);
      s4o.print(" ");
      symbol->program_name->accept(*this);
      if (wanted_varformat == localinit_vf) {
        // TODO...
        // program_call(symbol);
      }
      s4o.print(";\n");
      break;

    case constructorinit_vf:
      s4o.print(nv->get());
      program_constructor_call(symbol);
/*
      symbol->program_name->accept(*this);
      s4o.print("(");
      symbol->prog_conf_elements->accept(*this);
      nv->pop();
      s4o.print(")");
*/
      break;

    default:
      ERROR; /* not supported, and not needed either... */
  }

  return NULL;
}



/* prog_conf_elements ',' prog_conf_element */
//SYM_LIST(prog_conf_elements_c)
void *visit(prog_conf_elements_c *symbol) {
  TRACE("prog_conf_elements_c");

  return print_list(symbol);
}

/*  fb_name WITH task_name */
//SYM_REF2(fb_task_c, fb_name, task_name)
void *visit(fb_task_c *symbol) {
  TRACE("fb_task_c");

  /* TODO...
   *
   * NOTE: Not yet supported...
   *       We do not support allocating specific function blocks declared
   *       inside a program to be executed by a different task from the one
   *       already executing the program itself.
   *       This is mostly because I (Mario) simply do not understand the
   *       semantics the standard expects us to implement in this case. It is all
   *       very confusing, and very poorly defined in the standard!
   */
  ERROR;
  return NULL;
}



























/*  any_symbolic_variable ASSIGN prog_data_source */
// SYM_REF2(prog_cnxn_assign_c, symbolic_variable, prog_data_source)
void *visit(prog_cnxn_assign_c *symbol) {
  TRACE("prog_cnxn_assign_c");

  /* TODO... */
  return NULL;
}

/* any_symbolic_variable SENDTO data_sink */
// SYM_REF2(prog_cnxn_sendto_c, symbolic_variable, prog_data_source)
void *visit(prog_cnxn_sendto_c *symbol) {
  TRACE("prog_cnxn_sendto_c");

  /* TODO... */
  return NULL;
}

#if 0
/* VAR_CONFIG instance_specific_init_list END_VAR_BOGUS */
SYM_REF2(instance_specific_initializations_c, instance_specific_init_list, unused)

/* helper symbol for instance_specific_initializations */
SYM_LIST(instance_specific_init_list_c)

/* resource_name '.' program_name '.' {fb_name '.'}
    ((variable_name [location] ':' located_var_spec_init) | (fb_name ':' fb_initialization))
*/
SYM_REF6(instance_specific_init_c, resource_name, program_name, any_fb_name_list, variable_name, location, initialization)

/* helper symbol for instance_specific_init */
/* function_block_type_name ':=' structure_initialization */
SYM_REF2(fb_initialization_c, function_block_type_name, structure_initialization)

#endif




/****************************************/
/* B.2 - Language IL (Instruction List) */
/****************************************/
/***********************************/
/* B 2.1 Instructions and Operands */
/***********************************/
  /* leave for derived classes... */

/*******************/
/* B 2.2 Operators */
/*******************/
  /* leave for derived classes... */


/***************************************/
/* B.3 - Language ST (Structured Text) */
/***************************************/
/***********************/
/* B 3.1 - Expressions */
/***********************/
  /* leave for derived classes... */

/********************/
/* B 3.2 Statements */
/********************/
  /* leave for derived classes... */

/*********************************/
/* B 3.2.1 Assignment Statements */
/*********************************/
  /* leave for derived classes... */

/*****************************************/
/* B 3.2.2 Subprogram Control Statements */
/*****************************************/
  /* leave for derived classes... */

/********************************/
/* B 3.2.3 Selection Statements */
/********************************/
  /* leave for derived classes... */

/********************************/
/* B 3.2.4 Iteration Statements */
/********************************/
  /* leave for derived classes... */



}; /* generate_c_vardecl_c */


