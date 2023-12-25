/*
*********************************************
    Projekt:    IFJ 2019
    Tým:        086
    Varianta:   II
    Členové:    Antonín Hubík    (xhubik03)
                Daša Nosková     (xnosko05)
                David Holas      (xholas11)
                Kateřina Mušková (xmusko00)
    
    Soubor:     parser.h
    Autor:      Daša Nosková     (xnosko05)
    Úpravy:     
*********************************************
*/

#ifndef PARSER_H
#define PARSER_H


#include "token.h"
#include "scanner.h"
#include "error.h"
#include "expressions.h"
#include "symtable.h"
#include "token.h"
#include "generator.h"

int get_new_token();
// function checks the global table if all used functions have been also defined
void all_fun_defined();
int program();
int f_definition();
int f_name();
// function checks if function token_f is already in table or not and does sematics controls related to function
// sets bool check_param
int func_table_control(token token_f);
// based on value of bool check_param either sets or checks parameters of function
int func_check_param();
int statement();
int stat_list();
// help with redundant code
// enum lexeme type - type of first terminal from used non-terminal
int help_conditions(enum lexeme type);
int if_else();
int return_tail();
int func();
// checks type of parameters given to called build-in function.
// enum lexeme type is type of expected parameter
int check_build_in_func_param(enum lexeme type);
int help_id();
int assign(symt_item* assign_to);
int func_or_exp(symt_item* assign_to);
int param();
int param_list();
int term();
void sa_debug (enum lexeme lex, char *func);

#endif