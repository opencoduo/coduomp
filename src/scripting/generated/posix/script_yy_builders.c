#include <stddef.h>
#include <stdint.h>

#include "server/standalone/core_runtime/core_runtime_private.h"
#include "server/standalone/scripting/script_compile_private.h"
#include "scripting/script_memory.h"

#define AST_MEMBER_SIZE(member_) (offsetof(scr_ast_node_t, payload) + sizeof(((scr_ast_node_t *)0)->payload.member_))

/* NOT_FROM_ORIGINAL_SOURCE: native-width allocation for the original
 * fixed-dword AST constructors.  Kind-selected stores below retain 32-bit
 * source positions, opcodes, and string handles while widening only real
 * pointers on native 64-bit builds. */
static scr_ast_node_t *coduomp_script_ast_allocate(scr_ast_kind_t kind, size_t size)
{
    scr_ast_node_t *node = Hunk_AllocateTempMemoryHighInternal(size);
    node->kind = kind;
    return node;
}

sval_u *node1_(sval_u *out, coduo_script_yystype_word_t word)
{
    out->words[0] = word;
    return out;
}

sval_u *node_pos(sval_u *out, coduo_script_yystype_word_t word)
{
    out->words[0] = word;
    return out;
}

sval_u *node0(sval_u *out, coduo_script_yystype_word_t word0)
{
    uintptr_t *word = Hunk_AllocateTempMemoryHighInternal(sizeof(*word));

    *word = word0;
    out->words[0] = (coduo_script_yystype_word_t)word;
    return out;
}

sval_u *node1(sval_u *out, coduo_script_yystype_word_t word0, coduo_script_yystype_word_t word1)
{
    const scr_ast_kind_t kind = (scr_ast_kind_t)word0;
    scr_ast_node_t *node;

    switch (kind) {
    case SCR_AST_KIND_FOR_CONDITION:
    case SCR_AST_KIND_CALL_VALUE:
    case SCR_AST_KIND_CALL_STATEMENT:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(child));
        node->payload.child.node = (scr_ast_node_t *)word1;
        break;
    default:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(sourceOnlyStatement));
        node->payload.sourceOnlyStatement.sourcePos = (uint32_t)word1;
        break;
    }

    out->words[0] = (coduo_script_yystype_word_t)node;
    return out;
}

sval_u *node2(sval_u *out, coduo_script_yystype_word_t word0, coduo_script_yystype_word_t word1, coduo_script_yystype_word_t word2)
{
    const scr_ast_kind_t kind = (scr_ast_kind_t)word0;
    scr_ast_node_t *node;

    switch (kind) {
    case SCR_AST_KIND_PRIMITIVE_EXPRESSION:
    case SCR_AST_KIND_REFERENCE_EXPRESSION:
    case SCR_AST_KIND_SCRIPT_FUNCTION_NAME:
    case SCR_AST_KIND_CAST_BOOL:
    case SCR_AST_KIND_CAST_INT:
    case SCR_AST_KIND_CAST_FLOAT:
    case SCR_AST_KIND_CAST_STRING:
    case SCR_AST_KIND_BOOL_NOT:
    case SCR_AST_KIND_BOOL_COMPLEMENT:
    case SCR_AST_KIND_SIZE:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(sourceChild));
        node->payload.sourceChild.node = (scr_ast_node_t *)word1;
        node->payload.sourceChild.sourcePos = (uint32_t)word2;
        break;
    case SCR_AST_KIND_FUNCTION_REF:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(functionRef));
        node->payload.functionRef.nameHandle = (uint32_t)word1;
        node->payload.functionRef.sourcePos = (uint32_t)word2;
        break;
    case SCR_AST_KIND_FUNCTION_POINTER_CALL:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(pointerCall));
        node->payload.pointerCall.functionExpression = (scr_ast_node_t *)word1;
        node->payload.pointerCall.sourcePos = (uint32_t)word2;
        break;
    case SCR_AST_KIND_FUNCTION_CALL:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(namedCall));
        node->payload.namedCall.functionNode = (scr_ast_node_t *)word1;
        node->payload.namedCall.sourcePos = (uint32_t)word2;
        break;
    case SCR_AST_KIND_EXPRESSION_LIST:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(expressionList));
        node->payload.expressionList.list = (scr_ast_list_t *)word1;
        node->payload.expressionList.sourcePos = (uint32_t)word2;
        break;
    case SCR_AST_KIND_RETURN_VALUE_STATEMENT:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(sourceChild));
        node->payload.sourceChild.node = (scr_ast_node_t *)word1;
        node->payload.sourceChild.sourcePos = (uint32_t)word2;
        break;
    case SCR_AST_KIND_INC_STATEMENT:
    case SCR_AST_KIND_DEC_STATEMENT:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(incDecStatement));
        node->payload.incDecStatement.refNode = (scr_ast_node_t *)word1;
        node->payload.incDecStatement.sourcePos = (uint32_t)word2;
        break;
    case SCR_AST_KIND_STATEMENT_BLOCK:
    case SCR_AST_KIND_DEVELOPER_STATEMENT_BLOCK:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(developerStatementBlock));
        node->payload.developerStatementBlock.block = (scr_ast_statement_block_t *)word1;
        node->payload.developerStatementBlock.sourcePos = (uint32_t)word2;
        break;
    case SCR_AST_KIND_CASE_STATEMENT:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(caseStatement));
        node->payload.caseStatement.valueNode = (scr_ast_node_t *)word1;
        node->payload.caseStatement.sourcePos = (uint32_t)word2;
        break;
    case SCR_AST_KIND_USING_ANIMTREE:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(usingAnimTree));
        node->payload.usingAnimTree.nameHandle = (uint32_t)word1;
        node->payload.usingAnimTree.sourcePos = (uint32_t)word2;
        break;
    default:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(sourceString));
        node->payload.sourceString.stringHandle = (uint32_t)word1;
        node->payload.sourceString.sourcePos = (uint32_t)word2;
        break;
    }

    out->words[0] = (coduo_script_yystype_word_t)node;
    return out;
}

sval_u *node2_(sval_u *out, coduo_script_yystype_word_t word0, coduo_script_yystype_word_t word1)
{
    void *value;

#if UINTPTR_MAX > UINT32_MAX
    if (word0 <= UINT16_MAX) {
        scr_ast_string_entry_t *entry = Hunk_AllocateTempMemoryHighInternal(sizeof(*entry));
        entry->stringHandle = (uint32_t)word0;
        entry->sourcePos = (uint32_t)word1;
        value = entry;
    } else {
        scr_ast_expression_entry_t *entry = Hunk_AllocateTempMemoryHighInternal(sizeof(*entry));
        entry->node = (scr_ast_node_t *)word0;
        entry->sourcePos = (uint32_t)word1;
        value = entry;
    }
#else
    {
        scr_ast_string_entry_t *entry = Hunk_AllocateTempMemoryHighInternal(sizeof(*entry));
        entry->stringHandle = (uint32_t)word0;
        entry->sourcePos = (uint32_t)word1;
        value = entry;
    }
#endif

    out->words[0] = (coduo_script_yystype_word_t)value;
    return out;
}

/* NOT_FROM_ORIGINAL_SOURCE: typed rule-1 use of the original two-word
 * constructor, separated from node2_ so a legitimate string handle equal to
 * the root-kind value cannot be misclassified on native 64-bit builds. */
sval_u *coduomp_script_ast_new_script_root(sval_u *out, coduo_script_yystype_word_t entries)
{
    scr_ast_node_t *root = coduomp_script_ast_allocate(SCR_AST_KIND_SCRIPT_ROOT, AST_MEMBER_SIZE(scriptRoot));
    root->payload.scriptRoot.entries = (scr_ast_script_entry_block_t *)entries;
    out->words[0] = (coduo_script_yystype_word_t)root;
    return out;
}

sval_u *node3(sval_u *out, coduo_script_yystype_word_t word0, coduo_script_yystype_word_t word1, coduo_script_yystype_word_t word2,
              coduo_script_yystype_word_t word3)
{
    const scr_ast_kind_t kind = (scr_ast_kind_t)word0;
    scr_ast_node_t *node;

    switch (kind) {
    case SCR_AST_KIND_SCRIPT_FUNCTION_REF:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(scriptFunctionRef));
        node->payload.scriptFunctionRef.filenameHandle = (uint32_t)word1;
        node->payload.scriptFunctionRef.nameHandle = (uint32_t)word2;
        node->payload.scriptFunctionRef.sourcePos = (uint32_t)word3;
        break;
    case SCR_AST_KIND_METHOD_CALL:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(call));
        node->payload.call.callee = (scr_ast_node_t *)word1;
        node->payload.call.callSourcePos = (uint32_t)word2;
        node->payload.call.methodSourcePos = (uint32_t)word3;
        break;
    case SCR_AST_KIND_FUNCTION_CALL_VALUE:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(functionCallValue));
        node->payload.functionCallValue.callee = (scr_ast_node_t *)word1;
        node->payload.functionCallValue.args = (scr_ast_list_t *)word2;
        node->payload.functionCallValue.callSourcePos = (uint32_t)word3;
        break;
    case SCR_AST_KIND_ASSIGNMENT_STATEMENT:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(assignmentStatement));
        node->payload.assignmentStatement.refNode = (scr_ast_node_t *)word1;
        node->payload.assignmentStatement.valueNode = (scr_ast_node_t *)word2;
        node->payload.assignmentStatement.sourcePos = (uint32_t)word3;
        break;
    case SCR_AST_KIND_WAIT_STATEMENT:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(waitStatement));
        node->payload.waitStatement.timeNode = (scr_ast_node_t *)word1;
        node->payload.waitStatement.timeSourcePos = (uint32_t)word2;
        node->payload.waitStatement.opcodeSourcePos = (uint32_t)word3;
        break;
    case SCR_AST_KIND_IF_STATEMENT:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(ifStatement));
        node->payload.ifStatement.conditionNode = (scr_ast_node_t *)word1;
        node->payload.ifStatement.bodyNode = (scr_ast_node_t *)word2;
        node->payload.ifStatement.sourcePos = (uint32_t)word3;
        break;
    default:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(switchStatement));
        node->payload.switchStatement.valueNode = (scr_ast_node_t *)word1;
        node->payload.switchStatement.bodyNode = (scr_ast_node_t *)word2;
        node->payload.switchStatement.sourcePos = (uint32_t)word3;
        break;
    }

    out->words[0] = (coduo_script_yystype_word_t)node;
    return out;
}

sval_u *ScriptAst_NewTriple(sval_u *out, coduo_script_yystype_word_t word0, coduo_script_yystype_word_t word1,
                            coduo_script_yystype_word_t word2)
{
    uintptr_t *words = Hunk_AllocateTempMemoryHighInternal(3 * sizeof(words[0]));

    words[0] = word0;
    words[1] = word1;
    words[2] = word2;
    out->words[0] = (coduo_script_yystype_word_t)words;
    return out;
}

sval_u *node4(sval_u *out, coduo_script_yystype_word_t word0, coduo_script_yystype_word_t word1, coduo_script_yystype_word_t word2,
              coduo_script_yystype_word_t word3, coduo_script_yystype_word_t word4)
{
    const scr_ast_kind_t kind = (scr_ast_kind_t)word0;
    scr_ast_node_t *node;

    switch (kind) {
    case SCR_AST_KIND_BINARY_OPERATOR:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(binaryOperator));
        node->payload.binaryOperator.left = (scr_ast_node_t *)word1;
        node->payload.binaryOperator.right = (scr_ast_node_t *)word2;
        node->payload.binaryOperator.opcode = (uint32_t)word3;
        node->payload.binaryOperator.sourcePos = (uint32_t)word4;
        break;
    case SCR_AST_KIND_OBJECT_STRING_REF:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(objectStringRef));
        node->payload.objectStringRef.objectNode = (scr_ast_node_t *)word1;
        node->payload.objectStringRef.stringHandle = (uint32_t)word2;
        node->payload.objectStringRef.sourcePos = (uint32_t)word3;
        node->payload.objectStringRef.opcodeSourcePos = (uint32_t)word4;
        break;
    case SCR_AST_KIND_OBJECT_INDEX_OBJECT_REF:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(objectIndexObjectRef));
        node->payload.objectIndexObjectRef.objectNode = (scr_ast_node_t *)word1;
        node->payload.objectIndexObjectRef.indexNode = (scr_ast_node_t *)word2;
        node->payload.objectIndexObjectRef.objectSourcePos = (uint32_t)word3;
        node->payload.objectIndexObjectRef.indexSourcePos = (uint32_t)word4;
        break;
    case SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(refAssignmentStatement));
        node->payload.refAssignmentStatement.refNode = (scr_ast_node_t *)word1;
        node->payload.refAssignmentStatement.valueNode = (scr_ast_node_t *)word2;
        node->payload.refAssignmentStatement.opcode = (uint32_t)word3;
        node->payload.refAssignmentStatement.sourcePos = (uint32_t)word4;
        break;
    case SCR_AST_KIND_IF_ELSE_STATEMENT:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(ifElseStatement));
        node->payload.ifElseStatement.conditionNode = (scr_ast_node_t *)word1;
        node->payload.ifElseStatement.thenNode = (scr_ast_node_t *)word2;
        node->payload.ifElseStatement.elseNode = (scr_ast_node_t *)word3;
        node->payload.ifElseStatement.sourcePos = (uint32_t)word4;
        break;
    case SCR_AST_KIND_WHILE_STATEMENT:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(whileStatement));
        node->payload.whileStatement.conditionNode = (scr_ast_node_t *)word1;
        node->payload.whileStatement.bodyNode = (scr_ast_node_t *)word2;
        node->payload.whileStatement.conditionSourcePos = (uint32_t)word3;
        node->payload.whileStatement.loopSourcePos = (uint32_t)word4;
        break;
    case SCR_AST_KIND_DO_WHILE_STATEMENT:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(doWhileStatement));
        node->payload.doWhileStatement.bodyNode = (scr_ast_node_t *)word1;
        node->payload.doWhileStatement.conditionNode = (scr_ast_node_t *)word2;
        node->payload.doWhileStatement.conditionSourcePos = (uint32_t)word3;
        node->payload.doWhileStatement.loopSourcePos = (uint32_t)word4;
        break;
    case SCR_AST_KIND_FUNCTION_DEFINITION:
    case SCR_AST_KIND_DEVELOPER_FUNCTION_DEFINITION:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(functionDefinition));
        node->payload.functionDefinition.nameHandle = (uint32_t)word1;
        node->payload.functionDefinition.parameters = (scr_ast_list_t *)word2;
        node->payload.functionDefinition.body = (scr_ast_statement_block_t *)word3;
        node->payload.functionDefinition.sourcePos = (uint32_t)word4;
        break;
    case SCR_AST_KIND_ENDON_STATEMENT:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(endonStatement));
        node->payload.endonStatement.objectNode = (scr_ast_node_t *)word1;
        node->payload.endonStatement.eventNode = (scr_ast_node_t *)word2;
        node->payload.endonStatement.objectSourcePos = (uint32_t)word3;
        node->payload.endonStatement.opcodeSourcePos = (uint32_t)word4;
        break;
    default:
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(waittillStatement));
        node->payload.waittillStatement.objectNode = (scr_ast_node_t *)word1;
        node->payload.waittillStatement.list = (scr_ast_list_t *)word2;
        node->payload.waittillStatement.objectSourcePos = (uint32_t)word3;
        node->payload.waittillStatement.opcodeSourcePos = (uint32_t)word4;
        break;
    }

    out->words[0] = (coduo_script_yystype_word_t)node;
    return out;
}

sval_u *ScriptAst_NewQuad(sval_u *out, coduo_script_yystype_word_t word0, coduo_script_yystype_word_t word1,
                          coduo_script_yystype_word_t word2, coduo_script_yystype_word_t word3)
{
    uintptr_t *words = Hunk_AllocateTempMemoryHighInternal(4 * sizeof(words[0]));

    words[0] = word0;
    words[1] = word1;
    words[2] = word2;
    words[3] = word3;
    out->words[0] = (coduo_script_yystype_word_t)words;
    return out;
}

sval_u *node5(sval_u *out, coduo_script_yystype_word_t word0, coduo_script_yystype_word_t word1, coduo_script_yystype_word_t word2,
              coduo_script_yystype_word_t word3, coduo_script_yystype_word_t word4, coduo_script_yystype_word_t word5)
{
    const scr_ast_kind_t kind = (scr_ast_kind_t)word0;
    scr_ast_node_t *node;

    if (kind == SCR_AST_KIND_METHOD_CALL_VALUE) {
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(methodCallValue));
        node->payload.methodCallValue.objectNode = (scr_ast_node_t *)word1;
        node->payload.methodCallValue.callee = (scr_ast_node_t *)word2;
        node->payload.methodCallValue.args = (scr_ast_list_t *)word3;
        node->payload.methodCallValue.methodSourcePos = (uint32_t)word4;
        node->payload.methodCallValue.objectSourcePos = (uint32_t)word5;
    } else {
        node = coduomp_script_ast_allocate(kind, AST_MEMBER_SIZE(shortCircuit));
        node->payload.shortCircuit.left = (scr_ast_node_t *)word1;
        node->payload.shortCircuit.leftSourcePos = (uint32_t)word2;
        node->payload.shortCircuit.right = (scr_ast_node_t *)word3;
        node->payload.shortCircuit.rightSourcePos = (uint32_t)word4;
        node->payload.shortCircuit.operatorSourcePos = (uint32_t)word5;
    }

    out->words[0] = (coduo_script_yystype_word_t)node;
    return out;
}

sval_u *node6(sval_u *out, coduo_script_yystype_word_t word0, coduo_script_yystype_word_t word1, coduo_script_yystype_word_t word2,
              coduo_script_yystype_word_t word3, coduo_script_yystype_word_t word4, coduo_script_yystype_word_t word5,
              coduo_script_yystype_word_t word6)
{
    scr_ast_node_t *node = coduomp_script_ast_allocate((scr_ast_kind_t)word0, AST_MEMBER_SIZE(forStatement));

    node->payload.forStatement.initNode = (scr_ast_node_t *)word1;
    node->payload.forStatement.conditionNode = (scr_ast_node_t *)word2;
    node->payload.forStatement.incrementNode = (scr_ast_node_t *)word3;
    node->payload.forStatement.bodyNode = (scr_ast_node_t *)word4;
    node->payload.forStatement.conditionSourcePos = (uint32_t)word5;
    node->payload.forStatement.loopSourcePos = (uint32_t)word6;
    out->words[0] = (coduo_script_yystype_word_t)node;
    return out;
}

sval_u *linked_list_end(sval_u *out, coduo_script_yystype_word_t value)
{
    scr_ast_list_item_t *item = Hunk_AllocateTempMemoryHighInternal((size_t)sizeof(*item));
    item->value = value;
    item->next = NULL;

    scr_ast_list_t *list = Hunk_AllocateTempMemoryHighInternal((size_t)sizeof(*list));
    list->head = item;
    list->tail = item;

    out->words[0] = (coduo_script_yystype_word_t)list;
    return out;
}

sval_u *prepend_node(sval_u *out, coduo_script_yystype_word_t value, coduo_script_yystype_word_t headValue)
{
    scr_ast_list_item_t **head = (scr_ast_list_item_t **)headValue;
    scr_ast_list_item_t *item = Hunk_AllocateTempMemoryHighInternal((size_t)sizeof(*item));

    item->value = value;
    item->next = *head;
    *head = item;
    out->words[0] = (coduo_script_yystype_word_t)head;
    return out;
}

sval_u *append_node(sval_u *out, coduo_script_yystype_word_t listValue, coduo_script_yystype_word_t value)
{
    scr_ast_list_t *list = (scr_ast_list_t *)listValue;
    scr_ast_list_item_t *item = Hunk_AllocateTempMemoryHighInternal((size_t)sizeof(*item));

    item->value = value;
    item->next = NULL;
    list->tail->next = item;
    list->tail = item;
    out->words[0] = (coduo_script_yystype_word_t)list;
    return out;
}

sval_u *ScriptYyList_Concat(sval_u *out, scr_ast_list_t *left, scr_ast_list_t *right)
{
    left->tail->next = right->head;
    left->tail = right->tail;
    out->words[0] = (coduo_script_yystype_word_t)left;
    return out;
}
