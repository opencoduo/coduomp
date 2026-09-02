#ifndef CODUOMP_SCRIPT_COMPILE_H
#define CODUOMP_SCRIPT_COMPILE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../q_shared.h"
#include "script_runtime.h"
#include "scripting/script_code_emit.h"
#include "scripting/script_compile_expr.h"
#include "scripting/script_compile_statements.h"
#include "scripting/script_compile_types.h"
#include "scripting/script_compile_developer.h"
#include "scripting/script_compile_load.h"
#include "scripting/script_yy_runtime.h"
#include "scripting/script_yy_tokens.h"

enum {
    SCRIPT_YYSTACK_INITIAL_COUNT = 200,
    SCRIPT_YYSTACK_MAX_COUNT = 10000,
    SCRIPT_YYTABLE_INDEX_MAX = 1066
};

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(script_yy_buffer_t) == 40, "i386 Flex buffer layout changed");
_Static_assert(offsetof(sval_u, source.sourcePos) == 4, "i386 yacc source-position word moved");
_Static_assert(sizeof(sval_u) == 8, "i386 yacc value record size changed");
#endif

scr_ast_list_t *linked_list_end(void *entry);
scr_ast_list_item_t **prepend_node(void *entry, scr_ast_list_item_t **headLink);
scr_ast_list_t *append_node(scr_ast_list_t *list, void *entry);
scr_ast_list_t *ScriptParse_ConcatLists(scr_ast_list_t *first, const scr_ast_list_t *second);
void *coduomp_script_parse_allocate(size_t size);

void AddOpcodePos(uint32_t sourcePos);
uint16_t GetVariable(uint16_t parentId, uint32_t name);
uint16_t GetObject(uint16_t variableId);
uint16_t FindVariable(uint16_t parentId, uint32_t name);
uint16_t FindObject(uint16_t variableId);
const char *SL_ConvertToString(uint16_t string);
uint16_t Scr_CreateCanonicalFilename(const char *filename);
uint16_t SL_FindLowercaseString(const char *text);

extern char *script_yyInputCursor;
extern uint32_t script_yyCurrentSourcePos;
extern uint32_t script_yyPreviousSourcePos;
extern qboolean script_yyInit;
extern script_yy_buffer_t *script_yyCurrentBuffer;
extern int32_t script_yyStart;
extern scr_ast_node_t *script_parseRoot;
extern const int16_t script_yyAccept[];
extern const int16_t script_yyBase[];
extern const int16_t script_yyChk[];
extern char *script_yyCBufferPosition;
extern const int16_t script_yyDef[];
extern qboolean script_yyDidBufferSwitchOnEof;
extern const uint32_t script_yyEc[];
extern uint8_t script_yyHoldChar;
extern FILE *script_yyInputFile;
extern FILE *script_yyOutputFile;
extern int32_t script_yyLastAcceptingState;
extern char *script_yyLastAcceptingCpos;
extern int32_t script_yyLength;
extern const uint32_t script_yyMeta[];
extern int32_t script_yyNChars;
extern const int16_t script_yyNxt[];
extern char *script_yyText;
extern int32_t script_yychar;
extern sval_u script_yylval;
extern int32_t script_yynerrs;
extern const uint8_t script_yytranslate[];
extern const int16_t script_yyr1[];
extern const int16_t script_yyr2[];
extern const int16_t script_yydefact[];
extern const int16_t script_yydefgoto[];
extern const int16_t script_yypact[];
extern const int16_t script_yypgoto[];
extern const int16_t script_yytable[];
extern const int16_t script_yycheck[];

uint32_t(node1_)(uint32_t word);
uint32_t(node_pos)(uint32_t word);
/* NOT_FROM_ORIGINAL_SOURCE: the yacc reconstruction names the destination
 * semantic-value temporary explicitly. The original node1_/node_pos helpers
 * are dword identities; their calls were inlined into the parser actions. */
#define node1_(out_, word_) ((out_)->source.value = (node1_)((uint32_t)(word_)))
#define node_pos(out_, word_) ((out_)->source.value = (node_pos)((uint32_t)(word_)))
uintptr_t *node0(uintptr_t word0);
scr_ast_node_t *node1(uintptr_t word0, uintptr_t word1);
scr_ast_node_t *node2(uintptr_t word0, uintptr_t word1, uintptr_t word2);
void *node2_(uintptr_t word0, uintptr_t word1);
scr_ast_node_t *coduomp_script_ast_new_script_root(uintptr_t kind, uintptr_t entries);
scr_ast_node_t *node3(uintptr_t word0, uintptr_t word1, uintptr_t word2, uintptr_t word3);
uintptr_t *node3_(uintptr_t word0, uintptr_t word1, uintptr_t word2);
scr_ast_node_t *node4(uintptr_t word0, uintptr_t word1, uintptr_t word2, uintptr_t word3, uintptr_t word4);
uintptr_t *node4_(uintptr_t word0, uintptr_t word1, uintptr_t word2, uintptr_t word3);
scr_ast_node_t *node5(uintptr_t word0, uintptr_t word1, uintptr_t word2, uintptr_t word3, uintptr_t word4, uintptr_t word5);
scr_ast_node_t *node6(uintptr_t word0, uintptr_t word1, uintptr_t word2, uintptr_t word3, uintptr_t word4, uintptr_t word5,
                      uintptr_t word6);
#endif
