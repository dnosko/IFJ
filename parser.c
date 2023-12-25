/*
*********************************************
    Projekt:    IFJ 2019
    Tým:        086
    Varianta:   II
    Členové:    Antonín Hubík    (xhubik03)
                Daša Nosková     (xnosko05)
                David Holas      (xholas11)
                Kateřina Mušková (xmusko00)

    Soubor:     parser.c
    Autor:      Daša Nosková     (xnosko05)
    Úpravy:
*********************************************
*/

#define S_A "Syntax Analyzer"

#include "parser.h"
#include <stdbool.h>

#define LINES_GT 101
#define LINES_LT 23
#define LINES_TT 13

#define CHECK_RESULT(result) if (result != OK) return result
#define GETnCHECK_TOKEN() if(actual_token != NULL) {free_token(actual_token); actual_token = NULL;} result = get_new_token(); if (result != OK) return result
#define ERR_MSG(text,err_type) if (actual_token != NULL) free_token(actual_token); actual_token = NULL; error(S_A, err_type, "%s",text); return err_type
#define ERR_EXPR -2

token actual_token = NULL;
symtab* global_tab = NULL;
symtab* local_tab = NULL;
symtab* tmp_tab = NULL; // tmp tab to store used global vars locally
symt_item*  global_item = NULL;

fun_par* f_param = NULL;
unsigned param_index = 0;

bool inFunction = false;
bool defining_func = false;
bool check_param = false;
bool inCondition = false;

int counter_conditions = 0;
int counter_while = 0;


int main() {
    int result;
    //initialization
    scanner_init();
    generator_init();
    f_param = (fun_par*) malloc(sizeof(struct fun_par));
    if (!f_param) { ERR_MSG("Allocation failed.",ERR_INTERN); }

    global_tab = symtab_init(LINES_GT);
    if (global_tab == NULL) { ERR_MSG("Could't init global table.",ERR_INTERN); }

    local_tab = symtab_init(LINES_LT);
    if (local_tab == NULL) { ERR_MSG("Could't init local table.",ERR_INTERN); }

    tmp_tab = symtab_init(LINES_TT);
    if (tmp_tab == NULL) { ERR_MSG("Could't init temporary table.",ERR_INTERN); }

    //start SA
    result = program();

    if (result == OK) {  // if not OK token should be already freed
        sa_debug (actual_token->lexeme, "end of main");
        all_fun_defined();
    }
    //free everything
    if(f_param != NULL) {
        for (unsigned i = 0; i < param_index; i++) {
            str_destroy(f_param[i].par_id);
        }
        free(f_param);
    }

    gen_end();
    symtab_destroy(global_tab);
    symtab_destroy(local_tab);
    symtab_destroy(tmp_tab);
    generator_destroy();
    scanner_destroy();

    // Free just in case
    if(actual_token != NULL) {
        free_token(actual_token);
    } 

    return ERR_FLAG;
}

int get_new_token() {
    actual_token = get_token();
    if ((actual_token == NULL) || (actual_token->lexeme == T_LEX_UNKNOWN)) {
        ERR_MSG("",ERR_LEX_AN);
    }
    return OK;
}

void all_fun_defined() {
   symt_item *item = symtab_first_item(global_tab);
   if (item == NULL) { // no item in table
       return;
   }
   unsigned int is_def;

   while (1) {
       str_debug(item->id);
       if (get_is_def(item, &is_def) == 0 && is_def == 0) { //function exists
           error(S_A, ERR_SEM_DEF, "Function is used, but never defined.");
           return;
       }
       item = symtab_next_item(global_tab, item);
       if(item == NULL) {
           debug("done looping through table.");
           break;
       }
   }
}

int program() {
    int result;

    if (get_new_token() != OK) return -1;

    gen_begin();

    while(actual_token->lexeme != T_EOF) {
        switch (actual_token->lexeme) {
            case T_EOL: //comment at the beginning
                break;
            case T_DEF:
                result = f_definition(); // idem dnu actual_tok = def
                CHECK_RESULT(result);
                inFunction = false;
                symtab_reset(local_tab);
                symtab_reset(tmp_tab);
                debug("inFunction changed to false : %d",inFunction); // false = 0
                break;
            case T_IF:
            case T_ID:
            case T_WHILE:
            case T_INPUT_I:
            case T_INPUT_S:
            case T_INPUT_F:
            case T_PRINT:
            case T_LEN:
            case T_SUBSTR:
            case T_ORD:
            case T_CHR:
            case T_RETURN:
            case T_PASS:
                result = statement();
                CHECK_RESULT(result);
                break;
            case T_INDENT:
                ERR_MSG("Check indent",ERR_SYN_AN);
            case T_DEDENT:
                if(counter_while != 0) {
                    GETnCHECK_TOKEN();
                    if (actual_token->lexeme != T_DEDENT) {
                        ERR_MSG("Dedent expected",ERR_SYN_AN);
                    }
                }
                break;
            case T_INT:
            case T_STRING:
            case T_NONE:
            case T_DOUBLE:
            case T_L_BRACKET:
                result = eval_expression(global_tab,local_tab,tmp_tab,actual_token,NULL,&actual_token,NULL);
                if (result != 0) {
                    actual_token = NULL;
                    return ERR_EXPR;
                }
                break;
            default:
                ERR_MSG("Something is wrong",ERR_SYN_AN);
        }
        if (actual_token->lexeme == T_EOF) break;
        GETnCHECK_TOKEN();
        sa_debug (actual_token->lexeme, "program loop");
    }
    return OK;
}

// 4. def <f_name> : eol indent <stat> <stat_list> dedent
int f_definition() {
    int result;

    inFunction = true;
    defining_func = true;
    symtab_reset(local_tab);

    result = f_name();
    CHECK_RESULT(result);

    // get args for generator
    unsigned int count = get_args_cnt(global_item);
    fun_par* p = malloc(sizeof(fun_par)*count);
    if(p == NULL) {
        ERR_MSG("Allocation failed.",ERR_INTERN);
    }
    // initialization
    for(unsigned int i = 0; i < count; i++) {
        p[i].par_id = NULL;
        p[i].par_type = DT_UNDEF;
    }
    count = get_args(global_item,count,p);
    string* param_names[count];
    for (unsigned i = 0; i < count; i++) {
        param_names[i] = str_init();
        str_cpy(param_names[i],p[i].par_id);
    }
    gen_fnc_def_beg(global_item->id,param_names,count);
    //free string*
    for (unsigned int i = 0; i < count; i++) {
        str_destroy(param_names[i]);
        str_destroy(p[i].par_id);
    }
    free(p);

    if(set_is_def(global_item, 1) == -1) {
        ERR_MSG("Function could not be defined",ERR_INTERN);
    }

    defining_func = false;

    result = help_conditions(T_DEF);
    CHECK_RESULT(result);

    gen_fnc_def_end();
    return result;
}

int f_name() {
    int result;

    GETnCHECK_TOKEN();
    sa_debug (actual_token->lexeme, "f_name");
    if (actual_token->lexeme != T_ID) {
        ERR_MSG("Unexpected lexeme. ID expected",ERR_SYN_AN);
    }
    result = func_table_control(actual_token);
    CHECK_RESULT(result);

    GETnCHECK_TOKEN(); // (
    sa_debug (actual_token->lexeme, "f_name");
    if (actual_token->lexeme != T_L_BRACKET) {
        ERR_MSG("Unexpected lexeme. Left bracket expected.",ERR_SYN_AN);
    }

    result = func_check_param(actual_token);
    CHECK_RESULT(result);

    return OK;
}

int func_table_control(token token_f) {
    id_type type;
    unsigned int def;

    //searches for func in global table
    global_item = symtab_find(global_tab, token_f->value.id_key);

    // check if global variable with the same name isn't already used
    if (get_item_type(global_item,&type) == 0) {
        if (type != ID_F) {
            ERR_MSG("Function can't have the same name as variable.", ERR_SEM_DEF);
        }
    }

    // CHECK IF DEFINED OR NOT, IN TABLE OR NOT, DEFINING RN OR NOT,

    // DEFINING FUNCTION RN
    int is_def_res = get_is_def(global_item,&def);
    if (is_def_res == 0) { //function is in table already
        if (def == 1 && defining_func == true) { // function is already defined
            ERR_MSG("Redefinition of function isn't allowed.",ERR_SEM_DEF);
        }
        check_param = true;
    }
    else if (is_def_res == -1) { // function isnt in table yet -> dont check param, just insert item to table
        if (inFunction == true) { // function can be called from different function even if it isn't defined yet
            global_item = symtab_find_insert(global_tab,token_f->value.id_key); // adds function to table -> we know it hasnt been yet in table
            int set_def_res = (defining_func == true) ? set_is_def(global_item,1) : set_is_def(global_item,0); // if we are defining function set it to def, else not def yet
            if (set_def_res == -1) {
                ERR_MSG("Couldn't set if func is defined.",ERR_INTERN);
            }
            if (set_ret_t(global_item,DT_UNDEF) == -1) {
                ERR_MSG("Setting return type of function failed.",ERR_INTERN);
            }
            str_debug(global_item->id);
        }
        else { //function hasn't been defined
            ERR_MSG("Function hasn't been defined.",ERR_SEM_DEF);
        }
        check_param = false;
    }

    return OK;
}

int func_check_param() {

    param_index = 0;

    if (param() != OK) {
        return -1;
    }

    unsigned param_cnt = param_index;

    // first time in table so no need to check parameters, just add them to function
    if (check_param == false) {
        if (set_args(global_item,param_cnt,f_param) == -1) {
            error(S_A, ERR_INTERN,"Couldn't add arguments of function to a table.");
            free_token(actual_token); // not using macro ERR_MSG cause we need to destroy f_param at the end
            actual_token = NULL;
        }
    }
    else {
        // checking only if the number of params is the same
        debug("param_index: %d, argc_cnt: %d\n",param_cnt,get_args_cnt(global_item));
        if (param_cnt != get_args_cnt(global_item)) {
            error(S_A, ERR_SEM_PAR,"Number of parameters isn't the same.");
            free_token(actual_token);
            actual_token = NULL;
        }
    }

    for (unsigned i = 0; i < param_cnt; i++) {
        // destroy only allocated ids, if param is constant par_id = NULL
        if (f_param[i].par_id != NULL)
            str_destroy(f_param[i].par_id);
    }
    param_index = 0;

    return ERR_FLAG;
}

//just one line, ends with EOL except IF and WHILE
int statement() {
    int result;

    switch (actual_token->lexeme) {
        case T_ID: // rule 9. and 10 where <func> = ID
            result = help_id();
            CHECK_RESULT(result);
            if ((actual_token->lexeme == T_EOL) || (actual_token->lexeme == T_EOF)) return OK;
            break; // check EOL at the end
        case T_IF:
            result = help_conditions(T_IF);
            CHECK_RESULT(result);


            GETnCHECK_TOKEN();
            sa_debug (actual_token->lexeme, "if between else");
            if (actual_token->lexeme != T_ELSE) {
                ERR_MSG("Unexpected lexeme. Else expected.",ERR_SYN_AN);
            }

            result = if_else(); //else
            gen_if_end();
            CHECK_RESULT(result);
            counter_conditions--;
            if (counter_conditions == 0) {
                debug("inCondition = false");
                inCondition = false;
            }
            return OK;
        case T_WHILE: // while EXPR : eol indent <stat> <stat_list> dedent
            counter_while++;
            result = help_conditions(T_WHILE);
            gen_while_end();
            CHECK_RESULT(result);
            counter_while--;
            counter_conditions--;
            if (counter_conditions == 0) {
                debug("inCondition = false");
                inCondition =false;
            }
            return OK;
        case T_RETURN: // <stat> -> return <return_tail> eol
            if (inFunction == false) {
                ERR_MSG("Return used outside of function.",ERR_SEM_OTH);
            }
            result = return_tail();
            CHECK_RESULT(result); // is always return <expression> EOL or EOF, dedent is followed after
            if (actual_token->lexeme == T_EOL || actual_token->lexeme == T_EOF)
                return OK;
            else ERR_MSG("Expected EOL or EOF after return statement.",ERR_SYN_AN); // dedent is after eol or eof
        case T_PASS: // <stat> -> pass eol
            break;
        case T_INPUT_S: // <stat> -> <func> eol
        case T_INPUT_F:
        case T_INPUT_I:
        case T_PRINT:
        case T_LEN:
        case T_SUBSTR:
        case T_ORD:
        case T_CHR:
            result = func();
            CHECK_RESULT(result);
            break;
        case T_STRING:
        case T_INT:
        case T_DOUBLE:
        case T_NONE:
        case T_L_BRACKET:
            result = eval_expression(global_tab,local_tab,tmp_tab,actual_token,NULL,&actual_token,NULL);
            if (result != 0) {
                actual_token = NULL;
                return ERR_EXPR;
            }
            gen_non_assign();
            if ((actual_token->lexeme == T_EOL) || (actual_token->lexeme == T_EOF)) return OK;
            else { ERR_MSG("End of line or end of file expected.",ERR_SYN_AN); }
        default:
        ERR_MSG("Unexpected lexeme.",ERR_SYN_AN);
    }

    //ends with EOL or EOF
    
    GETnCHECK_TOKEN();
    sa_debug (actual_token->lexeme, "end stat");
    if ((actual_token->lexeme == T_EOL) || (actual_token->lexeme == T_EOF)) return OK;
    else { ERR_MSG("End of line or end of file expected.",ERR_SYN_AN); }
}

int stat_list() {
    int result;

    if ((actual_token->lexeme == T_DEDENT) || (actual_token->lexeme == T_EOF))
        return OK;

    while (1) {
        switch (actual_token->lexeme) {
            case T_IF:
            case T_ID:
            case T_WHILE:
            case T_INPUT_I:
            case T_INPUT_S:
            case T_INPUT_F:
            case T_PRINT:
            case T_LEN:
            case T_SUBSTR:
            case T_ORD:
            case T_CHR:
            case T_RETURN:
            case T_PASS:
                result = statement();
                CHECK_RESULT(result);
                // if statement ends with EOF, and we try to get next token its ERROR
                if (actual_token->lexeme == T_EOF) return OK;
                GETnCHECK_TOKEN();
                sa_debug (actual_token->lexeme, "stat_list");
                if (actual_token->lexeme == T_DEDENT || actual_token->lexeme == T_EOF)
                    return OK;
                break;
            default:
            ERR_MSG("Unexpected lexeme.", ERR_SYN_AN);
        }
    }

}

// help function, because conditions if, while and if_else have almost identical syntax
// added also definition of function
int help_conditions(enum lexeme type) {
    int result;

    GETnCHECK_TOKEN();
    sa_debug (actual_token->lexeme, "help_condi");

    switch (type) {
        case T_IF:
            result = eval_expression(global_tab,local_tab,tmp_tab,actual_token,NULL, &actual_token,NULL);
            gen_if_beg();
            if (result != 0) {
                actual_token = NULL;
                return ERR_EXPR;
            }
            inCondition = true;
            counter_conditions++;
            break;
        case T_WHILE:
            gen_while_beg();
            result = eval_expression(global_tab,local_tab,tmp_tab,actual_token,NULL, &actual_token,NULL);
            gen_while_cond();
            if (result != 0) {
                actual_token = NULL;
                return ERR_EXPR;
            }
            inCondition = true;
            counter_conditions++;
            break;
        case T_ELSE:
            gen_else();
            break;
        case T_DEF:
            break;
        default:
        ERR_MSG("Something is wrong",ERR_SYN_AN);
    }

    if (actual_token->lexeme != T_COLON) { ERR_MSG("Colon expected.",ERR_SYN_AN);}

    GETnCHECK_TOKEN(); //eol
    sa_debug (actual_token->lexeme, "help_condi");
    if (actual_token->lexeme != T_EOL) {ERR_MSG("EOL expected.",ERR_SYN_AN);}

    GETnCHECK_TOKEN(); //indent
    sa_debug (actual_token->lexeme, "help_condi");
    if (actual_token->lexeme != T_INDENT) {ERR_MSG("Indent expected.",ERR_SYN_AN);}

    GETnCHECK_TOKEN(); // stat
    sa_debug (actual_token->lexeme, "help_condi");
    result = statement(); // stat
    CHECK_RESULT(result);
    if (actual_token->lexeme == T_EOF) return OK;

    GETnCHECK_TOKEN();
    sa_debug(actual_token->lexeme, "help condi before stat_list");
    result = stat_list();
    CHECK_RESULT(result);

    if (actual_token->lexeme != T_DEDENT
        && actual_token->lexeme != T_EOF) {
        ERR_MSG("Dedent or EOF expected.",ERR_SYN_AN);
    }

    return OK;
}

// 7. <if_else> -> else : indent <stat> <stat_list> dedent
int if_else() {

   if (actual_token->lexeme != T_ELSE) {
       ERR_MSG("Unexpected lexeme. ELSE expected.",ERR_SYN_AN);
   }

   return help_conditions(T_ELSE);
}

// rules 15. , 16.
int return_tail() {
   int result;

   GETnCHECK_TOKEN();
   sa_debug (actual_token->lexeme, "ret_tail");
   if (actual_token->lexeme != T_EOL && actual_token->lexeme != T_EOF) { // return_tail -> eps
       result = eval_expression(global_tab,local_tab,tmp_tab,actual_token,NULL, &actual_token,NULL);
       gen_return(0); // not NULL
       if (result != 0) {
           actual_token = NULL;
           return ERR_EXPR;
       }
       if (actual_token->lexeme == T_EOF) return OK;
   }
   else gen_return(1); // is NULL

   return OK;
}

// rules 17. - 24.
int func() {
   int result;

   enum lexeme name_of_func = actual_token->lexeme;
   GETnCHECK_TOKEN();
   sa_debug (actual_token->lexeme, "func");
   if (actual_token->lexeme != T_L_BRACKET) { ERR_MSG("Left bracket expected.",ERR_SYN_AN); }

   switch (name_of_func) {
       case T_INPUT_S: // inputs()
           gen_inputs();
           break;
       case T_INPUT_I: // inputi()
           gen_inputi();
           break;
       case T_INPUT_F: // inputf()
           gen_inputf();
           break;
       case T_PRINT: // print(<param>)
           param_index = 0;
           if (param() != OK) return -1;
           for (unsigned i = 0; i < param_index; i++) {
               // destroy only allocated ids, if param is constant par_id = NULL
               if (f_param[i].par_id != NULL)
                   str_destroy(f_param[i].par_id);
           }
           param_index = 0; 
           gen_print();
           return result;
       case T_LEN:
           GETnCHECK_TOKEN(); // string
           result = check_build_in_func_param(T_STRING);
           CHECK_RESULT(result);
           gen_len();
           break;
       case T_SUBSTR: // substr(string, int, int)
           GETnCHECK_TOKEN(); //string
           result = check_build_in_func_param(T_STRING);
           CHECK_RESULT(result);

           GETnCHECK_TOKEN(); // comma
           if (actual_token->lexeme != T_COMMA) { ERR_MSG("Unexpected lexeme. Comma expected.",ERR_SYN_AN);}

           GETnCHECK_TOKEN(); // int
           result = check_build_in_func_param(T_INT);
           CHECK_RESULT(result);

           GETnCHECK_TOKEN(); // comma
           if (actual_token->lexeme != T_COMMA) { ERR_MSG("Unexpected lexeme. Comma expected",ERR_SYN_AN);}

           GETnCHECK_TOKEN(); // int
           result = check_build_in_func_param(T_INT);
           CHECK_RESULT(result);

           gen_substr();
           break;
       case T_ORD: // ord(string, int)
           GETnCHECK_TOKEN(); //string
           result = check_build_in_func_param(T_STRING);
           CHECK_RESULT(result);

           GETnCHECK_TOKEN(); //comma
           if (actual_token->lexeme != T_COMMA) { ERR_MSG("Unexpected lexeme. Comma expected",ERR_SYN_AN);}

           GETnCHECK_TOKEN(); //int
           result = check_build_in_func_param(T_INT);
           CHECK_RESULT(result);

           gen_ord();
           break;
       case T_CHR: // chr(int) [0 - 255]
           GETnCHECK_TOKEN(); // int
           result = check_build_in_func_param(T_INT);
           CHECK_RESULT(result);
           gen_chr();
           break;
       default:
           ERR_MSG("Unexpected lexeme.",ERR_SYN_AN);
   }

   GETnCHECK_TOKEN();
   sa_debug (actual_token->lexeme, "end func");
   if (actual_token->lexeme != T_R_BRACKET) { ERR_MSG("Unexpected lexeme. Bracket expected.",ERR_SYN_AN); }

   return OK;
}

int check_build_in_func_param(enum lexeme type) {
    int is_loc = (inFunction == true) ? 0 : 1 ;
    string* value = NULL; // string to copy int or double value to string*
    symt_item* tmp_item = NULL;
    data_type var_type;

    if (actual_token->lexeme == T_ID) {
        if (inFunction == true) {
            tmp_item = symtab_find(local_tab,actual_token->value.id_key);
        }
        if (inFunction == false || tmp_item == NULL) {
            tmp_item = symtab_find(global_tab,actual_token->value.id_key);
            // if we are in function and we use global var, put it in tmp_table for possible later control because its value cant be changed locally
            if (inFunction == true) {
                symtab_find_insert(tmp_tab,tmp_item->id);
            }
        }
        if (get_var_t(tmp_item,&var_type) == -1) {
            ERR_MSG("Could't get type of variable.",ERR_INTERN);
        }
    }

    switch (type) {
        case T_STRING:
            if (actual_token->lexeme == T_STRING) {
                gen_const_param(DT_STRING,actual_token->value.string_struct);
                debug("ok its constant string");
                return OK;
            }
            else if (actual_token->lexeme == T_ID) {
                gen_id_param(actual_token->value.id_key,is_loc);

                if (var_type == DT_STRING) {
                    debug("ok its id string");
                }
                else if (var_type == DT_UNDEF) {
                    gen_param_ID_check(DT_STRING);
                    debug("undef string");
                }
                else
                    break;

                return OK;
            }
            break;
        case T_INT:
            if (actual_token->lexeme == T_INT) {
                value = str_init_fmt("%d",actual_token->value.integer); //int to string*
                gen_const_param(DT_INT,value);
                str_destroy(value);
                debug("ok its constant int");
                return OK;
            }
            else if (actual_token->lexeme == T_ID) {
                gen_id_param(actual_token->value.id_key,is_loc);

                if (var_type == DT_INT) {
                    debug("ok its id int");
                }
                else if (var_type == DT_UNDEF) {
                    gen_param_ID_check(DT_INT);
                    debug("undef int");
                }
                else
                    break;

                return OK;
            }
            break;
        default:
        ERR_MSG("Definitely should't be here.", ERR_INTERN);
    }

    ERR_MSG("Unexpected parameter in build-in function.", ERR_SEM_RUN);

}
//help function to decide how should be ID treated next
// ID + 5
// ID = <assign>
// ID(param) => funkcia
int help_id(){
    int result;

    symtab* helptab = (inFunction == true) ? local_tab : global_tab;

    symt_item* tmp_item = NULL;

    token first_token_id = actual_token; // ID
    actual_token = get_token();
    if (actual_token == NULL) {
        free_token(first_token_id);
        ERR_MSG("Could't get new token.",ERR_INTERN);
    }
    sa_debug (actual_token->lexeme, "help ID.");
    switch (actual_token->lexeme) {
        case T_ASSIGNMENT: // ID = <assign>
            // check if we havent already used global variable locally -> cant change its value
            if (inFunction == true) {
                if (symtab_find(tmp_tab,first_token_id->value.id_key) != NULL) {
                    free_token(first_token_id);
                    ERR_MSG("Can't change value of global variable locally.", ERR_SEM_DEF);
                }
            }

            // CALL GENERATOR IF VAR IS IN TABLE FOR THE FIRST TIME
            tmp_item = symtab_find(helptab,first_token_id->value.id_key);
            if (tmp_item == NULL)
                gen_def_var(first_token_id->value.id_key);

            // check global table for function with possibly the same name
            tmp_item = symtab_find(global_tab,first_token_id->value.id_key);
            id_type type;
            if (!get_item_type(tmp_item,&type) && type == ID_F) {
                free_token(first_token_id);
                ERR_MSG("Variable can't have the same name as function.",ERR_SEM_DEF);
            }

            tmp_item = symtab_find_insert(helptab, first_token_id->value.id_key);
            // will be DT_UNDEF unless we assign something else in func_or_exp()
            if (add_var_at(tmp_item,1,DT_UNDEF) == -1) {
                free_token(first_token_id);
                ERR_MSG("Setting attributes of variable failed.",ERR_INTERN);
            }

            debug("ASSIGN");
            str_debug(tmp_item->id);

            result = assign(tmp_item);
            /************************ debug only *******************************/
            data_type test;
            get_var_t(tmp_item,&test);
            debug("inCondition: %d, type: %d ",inCondition,test);
            /***************************************************************/
            (inFunction == true) ? gen_assign(tmp_item->id,0): gen_assign(tmp_item->id,1);
            free_token(first_token_id);
            return result;
        case T_EOL: // just ID
        case T_EOF:
        case T_GE:
        case T_GT:
        case T_LE:
        case T_LT:
        case T_EQUAL:
        case T_N_EQUAL:
        case T_MINUS:
        case T_PLUS:
        case T_DIVISION:
        case T_F_DIVISION:
        case T_MUL: // ID  * ID for example
            result = eval_expression(global_tab,local_tab,tmp_tab,first_token_id,actual_token, &actual_token,NULL);
            if (result != 0) {
                actual_token = NULL;
                return ERR_EXPR;
            }
            gen_non_assign();
            break;
        case T_L_BRACKET: // ID()


            // check if function can be named like this
            if (func_table_control(first_token_id) != OK) {
                free_token(first_token_id);
                return -1;
            }

            // check param
            if (func_check_param() != OK) {
                free_token(first_token_id);
                return -1;
            }

            gen_fnc_call(first_token_id->value.id_key);
            free_token(first_token_id);

            if (actual_token->lexeme != T_R_BRACKET) {
                ERR_MSG("Right bracket expected",ERR_SYN_AN);
            }

            gen_non_assign();

            return OK;
        default:
            free_token(first_token_id);
            ERR_MSG("Something is wrong.",ERR_SYN_AN);
    }
    return OK;
}
// 25. <assign> -> id = <func_or_exp>
int assign(symt_item* assign_to) {

   if (actual_token->lexeme != T_ASSIGNMENT) { ERR_MSG("Unexpected lexeme. Assigment expected.",ERR_SYN_AN);}
   sa_debug (actual_token->lexeme, "assign");

   return func_or_exp(assign_to);
}
// 26. <func_or_expr> -> id rest (decides based on semantic action)
int func_or_exp(symt_item* assign_to) {
   int result;
   data_type result_type;

   GETnCHECK_TOKEN();
   sa_debug (actual_token->lexeme, "func_or_ex. ");
   token first_token_id;
   switch(actual_token->lexeme) {
       case T_INPUT_S:
       case T_INPUT_I:
       case T_INPUT_F:
       case T_PRINT:
       case T_LEN:
       case T_SUBSTR:
       case T_ORD:
       case T_CHR:
           result = func();
           return result;
       // expression starts with id ==> ID rest
       case T_ID:
          first_token_id = actual_token; //assign the value, cause we need another token

          actual_token = get_token();
          if (actual_token == NULL) {
             free_token(first_token_id);
             ERR_MSG("Could't get new token.",ERR_INTERN);
          }
          sa_debug (actual_token->lexeme, "func_or_ex");
          switch (actual_token->lexeme) {
              case T_L_BRACKET: //ID()
                  if (func_table_control(first_token_id) != OK) {
                      free_token(first_token_id);
                      return -1;
                  }
                  // check param
                  if (func_check_param() != OK) {
                      free_token(first_token_id);
                      return -1;
                  }

                  gen_fnc_call(first_token_id->value.id_key);

                  free_token(first_token_id);
                  
                  if(actual_token->lexeme != T_R_BRACKET) {
                      ERR_MSG("Unexpected lexeme. Right bracket expected",ERR_SYN_AN);
                  }

                  return OK;
              //assigning ID
              case T_EOL: // ID
              case T_EOF:
              // assigning expression for example: ID + 5
              case T_PLUS:
              case T_MINUS:
              case T_MUL:
              case T_DIVISION:
              case T_F_DIVISION:
              case T_GE:
              case T_GT:
              case T_LE:
              case T_LT:
              case T_EQUAL:
              case T_N_EQUAL:
                  result = eval_expression(global_tab,local_tab,tmp_tab,first_token_id,actual_token, &actual_token,&result_type);
                 
                 if (result != 0) {
                     actual_token = NULL;
                     return ERR_EXPR;
                 }

                  if (inCondition == false) {
                      if (add_var_at(assign_to, 1, result_type) == -1) {
                          error(S_A, ERR_INTERN,
                                "Couldn't add type."); //actual_token freed in eval therefor ERR_MSG not used
                          return -1;
                      }
                  }
                  return OK;
              default:
                  free_token(first_token_id);
                  ERR_MSG("Unexpected lexeme.",ERR_SYN_AN);
          }
       // expression starts with term or bracket
       case T_INT:
       case T_DOUBLE:
       case T_STRING:
       case T_NONE:
       case T_L_BRACKET:
           result = eval_expression(global_tab,local_tab,tmp_tab,actual_token,NULL, &actual_token,&result_type);
           if (result != 0) {
               actual_token = NULL;
               return ERR_EXPR;
           }
           // set type only if we know for sure it will have this type so not inside of conditions
           if (inCondition == false) {
                if (add_var_at(assign_to, 1, result_type) == -1) {
                    error(S_A, ERR_INTERN, "Couldn't add type."); //actual_token freed in eval therefor ERR_MSG not used
                    return -1;
                }
           }

           return OK;
       default:
           break;
   }
   ERR_MSG("Unexpected lexeme.",ERR_SYN_AN);
}

int param() {
   int result;
   symt_item* tmp_item = NULL;
   GETnCHECK_TOKEN();
   sa_debug (actual_token->lexeme, "param");
   unsigned is_init;

   //init f_param
   if (!param_index) {
       f_param[param_index].par_id = NULL;
       f_param[param_index].par_type = DT_UNDEF;
   }

   if (actual_token->lexeme == T_R_BRACKET) // 28. <param> -> eps,
       return OK;


   result = term(); // 27. <param> -> <term> <param_list>
   CHECK_RESULT(result);
   // ADD PARAMS TO ARRAY
    //realloc
    fun_par *a_realloc;
    if (param_index) {
        a_realloc = realloc(f_param, (param_index +1) * sizeof(struct fun_par));
        if (a_realloc == NULL) {
            ERR_MSG("Reallocation failed.",ERR_INTERN);
        }
        else {
            f_param = a_realloc;
            f_param[param_index].par_id = NULL;
            f_param[param_index].par_type = DT_UNDEF;
        }
    }
    string* value = NULL; // value to string

    switch (actual_token->lexeme) {
        case T_INT:
            // Constant parameters should not be allowed when defining_func
            if (defining_func == false) {
                f_param[param_index].par_type = DT_INT;
                debug("int\n");
                f_param[param_index].par_id = NULL;
                value = str_init_fmt("%d",actual_token->value.integer); //int to string*
                gen_const_param(DT_INT, value);
                str_destroy(value);
            } else {
                ERR_MSG("Parameters can't be constants.",ERR_SYN_AN);
            }
            break;
        case T_STRING:
            if (defining_func == false) {
                f_param[param_index].par_type = DT_STRING;
                debug("string\n");
                f_param[param_index].par_id = NULL;
                gen_const_param(DT_STRING, actual_token->value.string_struct);
            } else {
                ERR_MSG("Parameters can't be constants.",ERR_SYN_AN);
            }
            break;
        case T_DOUBLE:
            if (defining_func == false) {
                f_param[param_index].par_type = DT_DOUBLE;
                debug("double\n");
                f_param[param_index].par_id = NULL;
                value = str_init_fmt("%a",actual_token->value.floating_point);
                gen_const_param(DT_DOUBLE, value);
                str_destroy(value);
            } else {
                ERR_MSG("Parameters can't be constants.",ERR_SYN_AN);
            } 
            break;
        case T_ID:
            //debug("undef\n");
            if (defining_func == true) {
                // Check if there isn't already parameter with same name
                if (symtab_find(local_tab,actual_token->value.id_key) != NULL) {
                    ERR_MSG("Function definition already contains a parameter with this ID.",ERR_SEM_DEF);
                }
                tmp_item = symtab_find_insert(local_tab,actual_token->value.id_key);
                debug("ADDING PARAM LOCAL TAB");
                if(tmp_item == NULL) {
                    ERR_MSG("Could't add parameter to the local table.",ERR_INTERN);
                }
                str_debug(tmp_item->id);
                if (add_var_at(tmp_item,1,DT_UNDEF) == -1) { // var is init because if we call the function we give a value
                    ERR_MSG("Could't add parameter to the local table.",ERR_INTERN);
                }
                // All parameters should be already marked as inited, as when function is called they will all have a value
                if(set_is_init(tmp_item, 1) == -1) {
                    ERR_MSG("Could't add parameters attributes.",ERR_INTERN);
                }
                f_param[param_index].par_type = DT_UNDEF;
            }
            else {
                int is_global = 0;
                if (inFunction == true) { // is var def in local table?
                    tmp_item = symtab_find(local_tab, actual_token->value.id_key);
                    is_global = 0;
                }
                if (inFunction == false || tmp_item == NULL) { // searching if var is defined in global table
                    tmp_item = symtab_find(global_tab, actual_token->value.id_key);
                    if (tmp_item == NULL) {
                        ERR_MSG("Variable used was not found.", ERR_SEM_DEF);
                    }
                    // if we are in function and we use global var, put it in tmp_table for possible later control because its value cant be changed locally
                    if (inFunction == true) {
                        symtab_find_insert(tmp_tab,tmp_item->id);
                    }
                    is_global = 1;
                }
                // All IDs passed to function when calling it must be already inited (same as in expressions, etc)
                if ((get_is_init(tmp_item, &is_init) == -1 || is_init == 0)) {
                    ERR_MSG("Variable hasn't been initialized.", ERR_SEM_DEF);
                }

                data_type type;
                if (get_var_t(tmp_item, &type) == -1) {
                    //error(S_A, ERR_INTERN, "Item has no type.");
                    ERR_MSG("Item has no type.",ERR_INTERN);
                }

                if (defining_func == false) {
                    if (type == DT_UNDEF) {
                        gen_isdef_check(tmp_item->id,is_global);
                    }
                }

                f_param[param_index].par_type = type;

                // Only when calling function, because in definition, params have to be passed all at once to generator, for some reason
                gen_id_param(tmp_item->id, is_global);
            }
                f_param[param_index].par_id = str_init(); //just init
                if (str_cpy(f_param[param_index].par_id, tmp_item->id) == -1) {
                    debug("ERROR");
                    free_token(actual_token);
                    actual_token = NULL;
                    return -1;
                }
                break;
                case T_NONE:
                    if (defining_func == false) {
                        f_param[param_index].par_type = DT_NONE;
                        debug("none\n");
                        f_param[param_index].par_id = NULL;
                        value = str_init_fmt("nil");
                        gen_const_param(DT_NONE, value);
                        str_destroy(value);
                    } else {
                        ERR_MSG("Parameters can't be constants.",ERR_SYN_AN);
                    }
                break;
                default:
                    ERR_MSG("Unexpected lexeme.",ERR_SYN_AN);

    }

    param_index++; // actually number of parameters at the end

   result = param_list();
   CHECK_RESULT(result);

   return OK;
}

int param_list() {
   int result;
   GETnCHECK_TOKEN();
   sa_debug (actual_token->lexeme, "param_list");
   // 29. <param_list> -> , <param>
   if (actual_token->lexeme == T_COMMA)
   {
       result = param();
       CHECK_RESULT(result);
       return OK;
   } // 30. <param_list> -> eps
   else if (actual_token->lexeme == T_R_BRACKET) {
       return OK;
   }
   else {
       ERR_MSG("Unexpected lexeme.",ERR_SYN_AN);
   }
}

// rules 31. 32. 33. 34.
int term() {
  // int result;

   switch (actual_token->lexeme) {
       case T_ID:
           return OK;
       case T_INT:
           return OK;
       case T_DOUBLE:
           return OK;
       case T_STRING:
           return OK;
       case T_NONE:
           return OK;
       default:
           ERR_MSG("Unexpected lexeme. ID, INT, FLOAT, STRING expected.",ERR_SYN_AN);
   }
}

void sa_debug (enum lexeme lex, char *func) {
    switch (lex) {
        case  T_DEF:
            debug("Actual token DEF, func %s\n", func);
            break;
        case T_IF:
            debug("Actual token IF, func %s\n", func);
            break;
        case T_ELSE:
            debug("Actual token ELSE, func %s\n", func);
            break;
        case T_WHILE:
            debug("Actual token WHILE, func %s\n", func);
            break;
        case T_RETURN:
            debug("Actual token RETURN, func %s\n", func);
            break;
        case T_INDENT:
            debug("Actual token INDENT, func %s\n", func);
            break;
        case T_DEDENT:
            debug("Actual token DEDENT, func %s\n", func);
            break;
        case T_PASS:
            debug("Actual token PASS, func %s\n", func);
            break;
        case T_INPUT_S:
            debug("Actual token INPUTS, func %s\n", func);
            break;
        case T_INPUT_I:
            debug("Actual token INPUTI, func %s\n", func);
            break;
        case T_INPUT_F:
            debug("Actual token INPUTF, func %s\n", func);
            break;
        case T_PRINT:
            debug("Actual token PRINT, func %s\n", func);
            break;
        case T_LEN:
            debug("Actual token LEN, func %s\n", func);
            break;
        case T_SUBSTR:
            debug("Actual token SUBSTR, func %s\n", func);
            break;
        case T_ORD:
            debug("Actual token ORD, func %s\n", func);
            break;
        case T_CHR:
            debug("Actual token CHR, func %s\n", func);
            break;
        case T_ID:
            debug("Actual token ID, func %s\n", func);
            break;
        case T_NONE:
            debug("Actual token NONE, func %s\n", func);
            break;
        case T_INT:
            debug("Actual token INT, func %s\n", func);
            break;
        case T_STRING:
            debug("Actual token STRING, func %s\n", func);
            break;
        case T_DOUBLE:
            debug("Actual token DOUBLE, func %s\n", func);
            break;
        case T_PLUS:
            debug("Actual token PLUS, func %s\n", func);
            break;
        case T_MINUS:
            debug("Actual token MINUS, func %s\n", func);
            break;
        case T_MUL:
            debug("Actual token MUL, func %s\n", func);
            break;
        case T_DIVISION:
            debug("Actual token DIVISION, func %s\n", func);
            break;
        case T_F_DIVISION:
            debug("Actual token F_DIVISION, func %s\n", func);
            break;
        case T_GE:
            debug("Actual token GE, func %s\n", func);
            break;
        case T_GT:
            debug("Actual token GT, func %s\n", func);
            break;
        case T_LE:
            debug("Actual token LE, func %s\n", func);
            break;
        case T_LT:
            debug("Actual token LT, func %s\n", func);
            break;
        case T_EQUAL:
            debug("Actual token EQUAL, func %s\n", func);
            break;
        case T_N_EQUAL:
            debug("Actual token N_EQUAL, func %s\n", func);
            break;
        case T_L_BRACKET:
            debug("Actual token L_BRAC, func %s\n", func);
            break;
        case T_R_BRACKET:
            debug("Actual token R_BRAC, func %s\n", func);
            break;
        case T_ASSIGNMENT:
            debug("Actual token ASSIGNMENT, func %s\n", func);
            break;
        case T_COLON:
            debug("Actual token COLON, func %s\n", func);
            break;
        case T_COMMA:
            debug("Actual token COMMA, func %s\n", func);
            break;
        case T_EOF:
            debug("Actual token EOF, func %s\n", func);
            break;
        case T_EOL:
            debug("Actual token EOL, func %s\n", func);
            break;
        case T_LEX_UNKNOWN:
            debug("Actual token UNKNOWN, func %s\n", func);
            break;
        default:
            break;
    }
    (void)func; // Silence unused variable warning
}
