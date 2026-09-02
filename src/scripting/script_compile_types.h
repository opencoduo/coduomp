#ifndef SHARED_SCRIPT_COMPILE_TYPES_H
#define SHARED_SCRIPT_COMPILE_TYPES_H

#include "qcommon/script_runtime_types.h"

#include <stddef.h>
#include <stdint.h>

/* The parser and compiler use the same numeric node-kind domain in
 * CoDUOMP.exe and coduo_lnxded.  Names below follow the emitted VM operation
 * or the compiler behavior, including the independently proved anim/game and
 * notify/endon identities. */
typedef enum scr_ast_kind_e {
    SCR_AST_KIND_SCRIPT_ROOT = 0x01,
    SCR_AST_KIND_ASSIGNMENT_STATEMENT = 0x02,
    SCR_AST_KIND_STRING_REF = 0x03,
    SCR_AST_KIND_PRIMITIVE_EXPRESSION = 0x04,
    SCR_AST_KIND_INTEGER_LITERAL = 0x05,
    SCR_AST_KIND_FLOAT_LITERAL = 0x06,
    SCR_AST_KIND_NEGATED_INTEGER_LITERAL = 0x07,
    SCR_AST_KIND_NEGATED_FLOAT_LITERAL = 0x08,
    SCR_AST_KIND_STRING = 0x09,
    SCR_AST_KIND_ISTRING = 0x0a,
    SCR_AST_KIND_OBJECT_INDEX_OBJECT_REF = 0x0b,
    SCR_AST_KIND_OBJECT_STRING_REF = 0x0c,
    SCR_AST_KIND_REFERENCE_EXPRESSION = 0x0d,
    SCR_AST_KIND_SCRIPT_FUNCTION_NAME = 0x0e,
    SCR_AST_KIND_CALL_VALUE = 0x0f,
    SCR_AST_KIND_FUNCTION_REF = 0x10,
    SCR_AST_KIND_SCRIPT_FUNCTION_REF = 0x11,
    SCR_AST_KIND_FUNCTION_POINTER_CALL = 0x12,
    SCR_AST_KIND_FUNCTION_CALL_VALUE = 0x13,
    SCR_AST_KIND_METHOD_CALL_VALUE = 0x14,
    SCR_AST_KIND_CALL_STATEMENT = 0x15,
    SCR_AST_KIND_FUNCTION_CALL = 0x16,
    SCR_AST_KIND_RETURN_VALUE_STATEMENT = 0x17,
    SCR_AST_KIND_RETURN_STATEMENT = 0x18,
    SCR_AST_KIND_WAIT_STATEMENT = 0x19,
    SCR_AST_KIND_METHOD_CALL = 0x1a,
    SCR_AST_KIND_UNDEFINED = 0x1b,
    SCR_AST_KIND_SELF = 0x1c,
    SCR_AST_KIND_LEVEL = 0x1d,
    SCR_AST_KIND_ANIM = 0x1e,
    SCR_AST_KIND_GAME = 0x1f,
    SCR_AST_KIND_IF_STATEMENT = 0x20,
    SCR_AST_KIND_IF_ELSE_STATEMENT = 0x21,
    SCR_AST_KIND_WHILE_STATEMENT = 0x22,
    SCR_AST_KIND_DO_WHILE_STATEMENT = 0x23,
    SCR_AST_KIND_FOR_STATEMENT = 0x24,
    SCR_AST_KIND_INC_STATEMENT = 0x25,
    SCR_AST_KIND_DEC_STATEMENT = 0x26,
    SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT = 0x27,
    SCR_AST_KIND_STATEMENT_BLOCK = 0x28,
    SCR_AST_KIND_DEVELOPER_STATEMENT_BLOCK = 0x29,
    SCR_AST_KIND_EXPRESSION_LIST = 0x2a,
    SCR_AST_KIND_BOOL_OR = 0x2b,
    SCR_AST_KIND_BOOL_AND = 0x2c,
    SCR_AST_KIND_BINARY_OPERATOR = 0x2d,
    SCR_AST_KIND_CAST_BOOL = 0x2e,
    SCR_AST_KIND_CAST_INT = 0x2f,
    SCR_AST_KIND_CAST_FLOAT = 0x30,
    SCR_AST_KIND_CAST_STRING = 0x31,
    SCR_AST_KIND_BOOL_NOT = 0x32,
    SCR_AST_KIND_BOOL_COMPLEMENT = 0x33,
    SCR_AST_KIND_SIZE = 0x34,
    SCR_AST_KIND_WAITTILL_STATEMENT = 0x36,
    SCR_AST_KIND_WAITTILLMATCH_STATEMENT = 0x37,
    SCR_AST_KIND_NOTIFY_STATEMENT = 0x38,
    SCR_AST_KIND_ENDON_STATEMENT = 0x39,
    SCR_AST_KIND_SWITCH_STATEMENT = 0x3a,
    SCR_AST_KIND_CASE_STATEMENT = 0x3b,
    SCR_AST_KIND_DEFAULT_STATEMENT = 0x3c,
    SCR_AST_KIND_BREAK_STATEMENT = 0x3d,
    SCR_AST_KIND_CONTINUE_STATEMENT = 0x3e,
    SCR_AST_KIND_FOR_CONDITION = 0x3f,
    SCR_AST_KIND_EMPTY_ARRAY = 0x40,
    SCR_AST_KIND_ANIMATION = 0x41,
    SCR_AST_KIND_FUNCTION_DEFINITION = 0x42,
    SCR_AST_KIND_DEVELOPER_FUNCTION_DEFINITION = 0x43,
    SCR_AST_KIND_USING_ANIMTREE = 0x44,
    SCR_AST_KIND_FALSE = 0x45,
    SCR_AST_KIND_TRUE = 0x46,
    SCR_AST_KIND_ANIMTREE = 0x47
} scr_ast_kind_t;

typedef struct scr_ast_node_s scr_ast_node_t;
typedef struct scr_ast_list_s scr_ast_list_t;
typedef struct scr_ast_list_item_s scr_ast_list_item_t;

/* Script string handles occupy the low word of an original parser dword. */
#define SCR_AST_STRING_HANDLE(value) ((uint16_t)(value))

typedef struct scr_ast_statement_item_s {
    scr_ast_node_t *node;
    struct scr_ast_statement_item_s *next;
} scr_ast_statement_item_t;

typedef struct scr_ast_statement_list_s {
    /* Parser-allocated zero sentinel; neither compiler dereferences it. */
    uintptr_t *sentinelValue;
    scr_ast_statement_item_t *head;
} scr_ast_statement_list_t;

typedef struct scr_ast_statement_block_s {
    /* Partial view of the generic head/tail list allocation. */
    scr_ast_statement_list_t *sentinel;
} scr_ast_statement_block_t;

typedef struct scr_ast_script_entry_list_s {
    uintptr_t *sentinelValue;
    scr_ast_list_item_t *head;
} scr_ast_script_entry_list_t;

typedef struct scr_ast_script_entry_block_s {
    scr_ast_script_entry_list_t *sentinel;
} scr_ast_script_entry_block_t;

typedef struct scr_ast_expression_entry_s {
    scr_ast_node_t *node;
    uint32_t sourcePos;
} scr_ast_expression_entry_t;

typedef struct scr_ast_string_entry_s {
    uint32_t stringHandle;
    uint32_t sourcePos;
} scr_ast_string_entry_t;

struct scr_ast_list_item_s {
    union {
        uintptr_t value;
        scr_ast_expression_entry_t *entry;
        scr_ast_string_entry_t *stringEntry;
    };
    scr_ast_list_item_t *next;
};

struct scr_ast_list_s {
    scr_ast_list_item_t *head;
    scr_ast_list_item_t *tail;
};

struct scr_ast_node_s {
    scr_ast_kind_t kind;
    /* Constructors write a kind-selected sequence of original dwords.  The
     * typed views keep pointers host-width while source positions, opcodes,
     * and string handles retain their original widths on native 64-bit. */
    union {
        struct {
            uint32_t value;
        } literal;
        struct {
            scr_ast_node_t *node;
        } child;
        struct {
            scr_ast_node_t *node;
            uint32_t sourcePos;
        } sourceChild;
        struct {
            uint32_t stringHandle;
            uint32_t sourcePos;
        } sourceString;
        struct {
            uint32_t nameHandle;
            scr_ast_list_t *parameters;
            scr_ast_statement_block_t *body;
            uint32_t sourcePos;
        } functionDefinition;
        struct {
            uint32_t nameHandle;
            uint32_t sourcePos;
        } usingAnimTree;
        struct {
            scr_ast_script_entry_block_t *entries;
        } scriptRoot;
        struct {
            scr_ast_list_t *list;
            uint32_t sourcePos;
        } expressionList;
        struct {
            scr_ast_node_t *left;
            uint32_t leftSourcePos;
            scr_ast_node_t *right;
            uint32_t rightSourcePos;
            uint32_t operatorSourcePos;
        } shortCircuit;
        struct {
            scr_ast_node_t *left;
            scr_ast_node_t *right;
            uint32_t opcode;
            uint32_t sourcePos;
        } binaryOperator;
        struct {
            scr_ast_node_t *objectNode;
            scr_ast_node_t *indexNode;
            uint32_t objectSourcePos;
            uint32_t indexSourcePos;
        } objectIndexObjectRef;
        struct {
            uint32_t stringHandle;
        } stringRef;
        struct {
            scr_ast_node_t *objectNode;
            uint32_t stringHandle;
            uint32_t sourcePos;
            uint32_t opcodeSourcePos;
        } objectStringRef;
        struct {
            scr_ast_node_t *refNode;
            scr_ast_node_t *valueNode;
            uint32_t sourcePos;
        } assignmentStatement;
        struct {
            scr_ast_node_t *refNode;
            scr_ast_node_t *valueNode;
            uint32_t opcode;
            uint32_t sourcePos;
        } refAssignmentStatement;
        struct {
            scr_ast_node_t *valueNode;
        } returnValueStatement;
        struct {
            scr_ast_node_t *timeNode;
            uint32_t timeSourcePos;
            uint32_t opcodeSourcePos;
        } waitStatement;
        struct {
            scr_ast_node_t *conditionNode;
            scr_ast_node_t *bodyNode;
            uint32_t sourcePos;
        } ifStatement;
        struct {
            scr_ast_node_t *conditionNode;
            scr_ast_node_t *thenNode;
            scr_ast_node_t *elseNode;
            uint32_t sourcePos;
        } ifElseStatement;
        struct {
            scr_ast_node_t *conditionNode;
            scr_ast_node_t *bodyNode;
            uint32_t conditionSourcePos;
            uint32_t loopSourcePos;
        } whileStatement;
        struct {
            scr_ast_node_t *bodyNode;
            scr_ast_node_t *conditionNode;
            uint32_t conditionSourcePos;
            uint32_t loopSourcePos;
        } doWhileStatement;
        struct {
            scr_ast_node_t *initNode;
            scr_ast_node_t *conditionNode;
            scr_ast_node_t *incrementNode;
            scr_ast_node_t *bodyNode;
            uint32_t conditionSourcePos;
            uint32_t loopSourcePos;
        } forStatement;
        struct {
            scr_ast_node_t *refNode;
            uint32_t sourcePos;
        } incDecStatement;
        struct {
            scr_ast_statement_block_t *block;
        } statementBlock;
        struct {
            scr_ast_statement_block_t *block;
            uint32_t sourcePos;
        } developerStatementBlock;
        struct {
            scr_ast_node_t *objectNode;
            scr_ast_list_t *list;
            uint32_t objectSourcePos;
            uint32_t opcodeSourcePos;
        } waittillStatement;
        struct {
            scr_ast_node_t *objectNode;
            scr_ast_list_t *list;
            uint32_t objectSourcePos;
            uint32_t opcodeSourcePos;
        } notifyStatement;
        struct {
            scr_ast_node_t *objectNode;
            scr_ast_node_t *eventNode;
            uint32_t objectSourcePos;
            uint32_t opcodeSourcePos;
        } endonStatement;
        struct {
            scr_ast_node_t *valueNode;
            scr_ast_node_t *bodyNode;
            uint32_t sourcePos;
        } switchStatement;
        struct {
            scr_ast_node_t *valueNode;
            uint32_t sourcePos;
        } caseStatement;
        struct {
            uint32_t sourcePos;
        } sourceOnlyStatement;
        struct {
            scr_ast_node_t *callee;
            scr_ast_list_t *args;
            uint32_t callSourcePos;
        } functionCallValue;
        struct {
            scr_ast_node_t *objectNode;
            scr_ast_node_t *callee;
            scr_ast_list_t *args;
            uint32_t methodSourcePos;
            uint32_t objectSourcePos;
        } methodCallValue;
        struct {
            uint32_t nameHandle;
            uint32_t sourcePos;
        } functionRef;
        struct {
            uint32_t filenameHandle;
            uint32_t nameHandle;
            uint32_t sourcePos;
        } scriptFunctionRef;
        struct {
            scr_ast_node_t *functionNode;
            uint32_t sourcePos;
        } namedCall;
        struct {
            scr_ast_node_t *functionExpression;
            uint32_t sourcePos;
        } pointerCall;
        struct {
            scr_ast_node_t *callee;
            uint32_t callSourcePos;
            uint32_t methodSourcePos;
        } call;
    } payload;
};

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(scr_ast_node_t, payload) == 4, "i386 AST payload offset changed");
_Static_assert(sizeof(scr_ast_node_t) == 28, "i386 AST node size changed");
_Static_assert(sizeof(scr_ast_list_item_t) == 8, "i386 AST list-item size changed");
_Static_assert(sizeof(scr_ast_list_t) == 8, "i386 AST list header size changed");
#endif

#endif
