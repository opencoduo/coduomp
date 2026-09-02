#include "client/engine/scripting/script_compile.h"

#include <stddef.h>

#define AST_MEMBER_SIZE(member_) (offsetof(scr_ast_node_t, payload) + sizeof(((scr_ast_node_t *)0)->payload.member_))

/* NOT_FROM_ORIGINAL_SOURCE: native-width allocation and common kind store for
 * the original fixed-word parser constructors. Each public constructor below
 * selects a typed payload before assigning fields, so 32-bit source positions
 * and opcodes do not become pointer-width slots on 64-bit hosts. */
static scr_ast_node_t *coduomp_script_ast_allocate(scr_ast_kind_t kind, size_t size)
{
    scr_ast_node_t *node = coduomp_script_parse_allocate(size);
    node->kind = kind;
    return node;
}

/* Source: CoDUOMP.exe 0x00481c60..0x00481c60.
 * Same-module Mac identity: node1_. The retained Windows body is a bare RET:
 * all parser uses are inlined, and their proved action is the dword identity
 * expressed here. */
uint32_t(node1_)(uint32_t word)
{
    return word;
}

/* Source: CoDUOMP.exe 0x00481c70..0x00481c70.
 * Same-module Mac identity: node_pos. Its inlined parser uses prove the
 * identical source-position dword operation. */
uint32_t(node_pos)(uint32_t word)
{
    return word;
}

/* Source: CoDUOMP.exe 0x00481c80..0x00481cd2. */
uintptr_t *node0(uintptr_t word0)
{
    uintptr_t *word = coduomp_script_parse_allocate(sizeof(*word));
    *word = word0;
    return word;
}

/* Source: CoDUOMP.exe 0x00481ce0..0x00481d39. */
scr_ast_node_t *node1(uintptr_t word0, uintptr_t word1)
{
    scr_ast_kind_t kind = (scr_ast_kind_t)word0;
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
    return node;
}

/* Source: CoDUOMP.exe 0x00481d40..0x00481da0. */
scr_ast_node_t *node2(uintptr_t word0, uintptr_t word1, uintptr_t word2)
{
    scr_ast_kind_t kind = (scr_ast_kind_t)word0;
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
    return node;
}

/* Source: CoDUOMP.exe 0x00481db0..0x00481e09.
 *
 * The original pair is two untyped 32-bit grammar words. On native 64-bit
 * hosts, list entries require either pointer/source-position or
 * string/source-position records. Select that honest typed layout here
 * instead of widening both original words indiscriminately. */
void *node2_(uintptr_t word0, uintptr_t word1)
{
    void *pair;

#if UINTPTR_MAX > UINT32_MAX
    if (word0 <= UINT16_MAX) {
        scr_ast_string_entry_t *entry = coduomp_script_parse_allocate(sizeof(*entry));
        entry->stringHandle = (uint32_t)word0;
        entry->sourcePos = (uint32_t)word1;
        pair = entry;
    } else {
        scr_ast_expression_entry_t *entry = coduomp_script_parse_allocate(sizeof(*entry));
        entry->node = (scr_ast_node_t *)word0;
        entry->sourcePos = (uint32_t)word1;
        pair = entry;
#else
    {
        scr_ast_string_entry_t *entry = coduomp_script_parse_allocate(sizeof(*entry));
        entry->stringHandle = (uint32_t)word0;
        entry->sourcePos = (uint32_t)word1;
        pair = entry;
#endif
    }

    return pair;
}

/* NOT_FROM_ORIGINAL_SOURCE: typed rule-1 use of the original pair
 * constructor. The compiler consumes the second word as scriptRoot.entries;
 * the first word is the grammar-provided root kind. */
scr_ast_node_t *coduomp_script_ast_new_script_root(uintptr_t kind, uintptr_t entries)
{
    scr_ast_node_t *root = coduomp_script_ast_allocate((scr_ast_kind_t)kind, AST_MEMBER_SIZE(scriptRoot));
    root->payload.scriptRoot.entries = (scr_ast_script_entry_block_t *)entries;
    return root;
}

/* Source: CoDUOMP.exe 0x00481e10..0x00481e77. */
scr_ast_node_t *node3(uintptr_t word0, uintptr_t word1, uintptr_t word2, uintptr_t word3)
{
    scr_ast_kind_t kind = (scr_ast_kind_t)word0;
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
    return node;
}

/* Source: CoDUOMP.exe 0x00481e80..0x00481ee0.
 * The `node3_` identity follows the proved `node2_` raw-record overload and
 * the node0..node6 constructor family. The same-module Mac linker discarded
 * this unused overload. */
uintptr_t *node3_(uintptr_t word0, uintptr_t word1, uintptr_t word2)
{
    uintptr_t *record = coduomp_script_parse_allocate(3 * sizeof(record[0]));

    record[0] = word0;
    record[1] = word1;
    record[2] = word2;
    return record;
}

/* Source: CoDUOMP.exe 0x00481ef0..0x00481f5e. */
scr_ast_node_t *node4(uintptr_t word0, uintptr_t word1, uintptr_t word2, uintptr_t word3, uintptr_t word4)
{
    scr_ast_kind_t kind = (scr_ast_kind_t)word0;
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
    return node;
}

/* Source: CoDUOMP.exe 0x00481f60..0x00481fc7.
 * Four-word counterpart to node3_. The Windows linker kept
 * this unused out-of-line body even though all live parser uses were inlined. */
uintptr_t *node4_(uintptr_t word0, uintptr_t word1, uintptr_t word2, uintptr_t word3)
{
    uintptr_t *record = coduomp_script_parse_allocate(4 * sizeof(record[0]));

    record[0] = word0;
    record[1] = word1;
    record[2] = word2;
    record[3] = word3;
    return record;
}

/* Source: CoDUOMP.exe 0x00481fd0..0x00482045. */
scr_ast_node_t *node5(uintptr_t word0, uintptr_t word1, uintptr_t word2, uintptr_t word3, uintptr_t word4, uintptr_t word5)
{
    scr_ast_kind_t kind = (scr_ast_kind_t)word0;
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
    return node;
}
