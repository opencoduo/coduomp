#ifndef CODUO_SCRIPT_COMPILE_PRIVATE_H
#define CODUO_SCRIPT_COMPILE_PRIVATE_H

#include <stdint.h>
#include <stdio.h>

#include "server/standalone/bindings/coduo_engine_structs.h"
#include "scripting/script_anim.h"
#include "scripting/script_compile_types.h"
#include "scripting/script_compile_developer.h"
#include "scripting/script_code_emit.h"
#include "scripting/script_compile_expr.h"
#include "scripting/script_compile_load.h"
#include "scripting/script_compile_statements.h"
#include "scripting/script_yy_runtime.h"
#include "scripting/script_yy_tokens.h"
#include "script_runtime_private.h"

/* Static yacc stack and highest parse-table index in coduo_lnxded. */
enum {
    SCRIPT_YY_STACK_COUNT = 500,
    SCRIPT_YY_PARSE_TABLE_INDEX_MAX = 0x5f1
};

extern int32_t script_parseSourceDone;
extern char *script_parseSource;
extern scr_ast_node_t *script_parseRoot;
extern const int16_t script_yyAccept[];
extern const int16_t script_yyBase[];
extern const int16_t script_yyChk[];
extern char *script_yyCBufferPosition;
extern const int16_t script_yyDef[];
extern int32_t script_yyDidBufferSwitchOnEof;
extern const uint32_t script_yyEc[];
extern uint8_t script_yyHoldChar;
extern int32_t script_yyInit;
extern char *script_yyInputCursor;
extern int32_t script_yyLastAcceptingState;
extern char *script_yyLastAcceptingCpos;
extern int32_t script_yyLength;
extern const uint32_t script_yyMeta[];
extern int32_t script_yyNChars;
extern const int16_t script_yyNxt[];
extern uint32_t script_yyCurrentSourcePos;
extern uint32_t script_yyPreviousSourcePos;
extern FILE *script_yyInputFile;
extern FILE *script_yyOutputFile;
extern int32_t script_yyStart;
extern char *script_yyText;
extern const int16_t script_yycheck[];
extern int32_t script_yychar;
extern script_yy_buffer_t *script_yyCurrentBuffer;
extern const int16_t script_yydef[];
extern const int16_t script_yydgoto[];
extern int32_t script_yyerrflag;
extern const int16_t script_yygindex[];
extern const int16_t script_yylhs[];
extern sval_u script_yylval;
extern const int16_t script_yylen[];
extern int16_t script_yyss[];
extern int16_t *script_yyssp;
extern const int16_t script_yyrindex[];
extern const int16_t script_yysindex[];
extern const int16_t script_yytable[];
extern sval_u script_yyval;
extern sval_u script_yyvs[];
extern sval_u *script_yyvsp;
extern int32_t script_yynerrs;

sval_u *
node1_(sval_u *out,
       coduo_script_yystype_word_t word);
sval_u *
node_pos(sval_u *out,
         coduo_script_yystype_word_t word);
sval_u *
node0(sval_u *out,
                      coduo_script_yystype_word_t word0);
sval_u *
node1(sval_u *out,
                   coduo_script_yystype_word_t word0,
                   coduo_script_yystype_word_t word1);
sval_u *
node2(sval_u *out,
                   coduo_script_yystype_word_t word0,
                   coduo_script_yystype_word_t word1,
                   coduo_script_yystype_word_t word2);
sval_u *
node2_(sval_u *out,
                  coduo_script_yystype_word_t word0,
                  coduo_script_yystype_word_t word1);
sval_u *coduomp_script_ast_new_script_root(
    sval_u *out, coduo_script_yystype_word_t entries);
sval_u *
node3(sval_u *out,
                   coduo_script_yystype_word_t word0,
                   coduo_script_yystype_word_t word1,
                   coduo_script_yystype_word_t word2,
                   coduo_script_yystype_word_t word3);
sval_u *
node4(sval_u *out,
                   coduo_script_yystype_word_t word0,
                   coduo_script_yystype_word_t word1,
                   coduo_script_yystype_word_t word2,
                   coduo_script_yystype_word_t word3,
                   coduo_script_yystype_word_t word4);
sval_u *
node5(sval_u *out,
                   coduo_script_yystype_word_t word0,
                   coduo_script_yystype_word_t word1,
                   coduo_script_yystype_word_t word2,
                   coduo_script_yystype_word_t word3,
                   coduo_script_yystype_word_t word4,
                   coduo_script_yystype_word_t word5);
sval_u *
node6(sval_u *out,
                   coduo_script_yystype_word_t word0,
                   coduo_script_yystype_word_t word1,
                   coduo_script_yystype_word_t word2,
                   coduo_script_yystype_word_t word3,
                   coduo_script_yystype_word_t word4,
                   coduo_script_yystype_word_t word5,
                   coduo_script_yystype_word_t word6);
sval_u *
linked_list_end(sval_u *out,
                      coduo_script_yystype_word_t value);
sval_u *
prepend_node(sval_u *out,
                         coduo_script_yystype_word_t value,
                         coduo_script_yystype_word_t headValue);
sval_u *
append_node(sval_u *out,
                        coduo_script_yystype_word_t listValue,
                        coduo_script_yystype_word_t value);
#endif
