#include "script_runtime_host.h"

#include "script_code_emit.h"
#include "script_compile_developer.h"
#include "script_compile_expr.h"
#include "script_compile_statements.h"
#include "script_error_reporting.h"
#include "script_source_positions.h"
#include "script_temp_memory.h"
#include "compat/crt/qsort_compat.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    SCRIPT_CODEGEN_MODE_RECORD_STRING_FIXUPS = 1,
    SCRIPT_CODEGEN_MODE_RELEASE_STRINGS = 2,
    SCRIPT_STRING_USAGE_CASE = 1,
    SCRIPT_OPCODE_RETURN = 0x00,
    SCRIPT_OPCODE_RETURN_VALUE = 0x01,
    SCRIPT_OPCODE_WAITTILL_END = 0x1e,
    SCRIPT_OPCODE_FUNCTION_PARAMETERS_DONE = 0x1f,
    SCRIPT_OPCODE_WAIT = 0x23,
    SCRIPT_OPCODE_JUMP_ON_FALSE = 0x35,
    SCRIPT_OPCODE_JUMP_ON_TRUE_BACK = 0x36,
    SCRIPT_OPCODE_JUMP_FORWARD = 0x39,
    SCRIPT_OPCODE_JUMP_BACK = 0x3a,
    SCRIPT_OPCODE_INC = 0x3b,
    SCRIPT_OPCODE_DEC = 0x3c,
    SCRIPT_OPCODE_WAITTILLMATCH = 0x4e,
    SCRIPT_OPCODE_WAITTILL = 0x4f,
    SCRIPT_OPCODE_NOTIFY = 0x50,
    SCRIPT_OPCODE_ENDON = 0x51,
    SCRIPT_OPCODE_PUSH_CODEPOS = 0x52,
    SCRIPT_OPCODE_SWITCH_JUMP = 0x53,
    SCRIPT_OPCODE_SWITCH_TABLE = 0x54,
    SCRIPT_WAITTILLMATCH_MATCH_COUNT_LIMIT = INT8_MAX,
    SCRIPT_SWITCH_TABLE_DISPLACEMENT_LIMIT = INT16_MAX,
    SCRIPT_SWITCH_CASE_COUNT_LIMIT = INT16_MAX,
    SCRIPT_CASE_INTEGER_TEST_BIAS = 0x7e0000u,
    SCRIPT_CASE_INTEGER_TEST_LIMIT = 0xfe0000u,
    SCRIPT_CASE_INTEGER_ENCODE_BIAS = 0x800000u,
    SCRIPT_CASE_INTEGER_ENCODE_MASK = 0xffffffu
};

/* NOT_FROM_ORIGINAL_SOURCE: readable grouping of the six independently
 * saved loop-control globals. CoDUOMP.exe has no aggregate ABI for this
 * source-level save/restore convenience. */
typedef struct script_loop_patch_state_s {
    uint8_t breakAllowed;
    uint8_t breakAllowedInDeveloperBlock;
    script_code_offset_patch_t *breakPatchList;
    uint8_t continueAllowed;
    uint8_t continueAllowedInDeveloperBlock;
    script_code_offset_patch_t *continuePatchList;
} script_loop_patch_state_t;

/* Source: CoDUOMP.exe 0x0047d310..0x0047d34c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d310_0047d34c.mcode. */
void EmitExpressionStatement(scr_ast_node_t *node)
{
    EmitCallExpression(node, qtrue);
}

/* Source: CoDUOMP.exe 0x0047d350..0x0047d3cd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d350_0047d3cd.mcode. */
void EmitReturnStatement(scr_ast_node_t *node)
{
    EmitExpression(node);
    EmitOpcode(SCRIPT_OPCODE_RETURN_VALUE, -1, 0);
}

/* Source: CoDUOMP.exe 0x0047d3d0..0x0047d435.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d3d0_0047d435.mcode. */
void EmitEndStatement(void)
{
    EmitOpcode(SCRIPT_OPCODE_RETURN, 0, 0);
}

/* Source: CoDUOMP.exe 0x0047d440..0x0047d4ef.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d440_0047d4ef.mcode. */
void EmitWaitStatement(scr_ast_node_t *timeNode, uint32_t timeSourcePos, uint32_t opcodeSourcePos)
{
    if (script_codegenMode != 0) {
        CompileError(opcodeSourcePos, "wait not allowed in /# ... #/ comment");
    }

    EmitExpression(timeNode);
    EmitOpcode(SCRIPT_OPCODE_WAIT, -1, 0);
    AddOpcodePos(opcodeSourcePos);
    AddOpcodePos(timeSourcePos);
}

/* Source: CoDUOMP.exe 0x0047d4f0..0x0047d5db.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d4f0_0047d5db.mcode. */
void EmitIfStatement(scr_ast_node_t *conditionNode, scr_ast_node_t *bodyNode, uint32_t sourcePos)
{
    EmitExpression(conditionNode);
    EmitOpcode(SCRIPT_OPCODE_JUMP_ON_FALSE, -1, 0);
    AddOpcodePos(sourcePos);
    EmitInteger(0);
    char *falsePatch = (char *)script_codeEmitCursor;

    EmitStatement(bodyNode);
    EmitNOP();

    char *current = (char *)TempMalloc(0);
    const int32_t falseOffset = (int32_t)(current - falsePatch) - (int32_t)sizeof(falseOffset);
    memcpy(falsePatch, &falseOffset, sizeof(falseOffset));
}

/* Source: CoDUOMP.exe 0x0047d5e0..0x0047d774.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d5e0_0047d774.mcode. */
void EmitIfElseStatement(scr_ast_node_t *conditionNode, scr_ast_node_t *thenNode, scr_ast_node_t *elseNode, uint32_t sourcePos)
{
    EmitExpression(conditionNode);
    EmitOpcode(SCRIPT_OPCODE_JUMP_ON_FALSE, -1, 0);
    AddOpcodePos(sourcePos);
    EmitInteger(0);
    char *falsePatch = (char *)script_codeEmitCursor;

    EmitStatement(thenNode);
    EmitOpcode(SCRIPT_OPCODE_JUMP_FORWARD, 0, 0);
    EmitInteger(0);
    char *endPatch = (char *)script_codeEmitCursor;

    char *current = (char *)TempMalloc(0);
    const int32_t falseOffset = (int32_t)(current - falsePatch) - (int32_t)sizeof(falseOffset);
    memcpy(falsePatch, &falseOffset, sizeof(falseOffset));

    EmitStatement(elseNode);
    EmitNOP();

    current = (char *)TempMalloc(0);
    const int32_t endOffset = (int32_t)(current - endPatch);
    memcpy(endPatch, &endOffset, sizeof(endOffset));
}

/* Source: CoDUOMP.exe 0x0047d780..0x0047da10.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d780_0047da10.mcode. */
void EmitWhileStatement(scr_ast_node_t *conditionNode, scr_ast_node_t *bodyNode, uint32_t conditionSourcePos, uint32_t loopSourcePos)
{
    EmitNOP();

    script_loop_patch_state_t saved = {script_breakAllowed,    script_breakAllowedInDeveloperBlock,    script_breakPatchList,
                                       script_continueAllowed, script_continueAllowedInDeveloperBlock, script_continuePatchList};
    script_breakAllowed = qfalse;
    script_breakAllowedInDeveloperBlock = qfalse;
    script_continueAllowed = qfalse;
    script_continueAllowedInDeveloperBlock = qfalse;

    uint8_t *loopStart = TempMalloc(0);
    EmitExpression(conditionNode);
    EmitOpcode(SCRIPT_OPCODE_JUMP_ON_FALSE, -1, 0);
    AddOpcodePos(conditionSourcePos);
    EmitInteger(0);
    char *falsePatch = (char *)script_codeEmitCursor;

    script_breakAllowed = qtrue;
    script_breakAllowedInDeveloperBlock = script_codegenMode != 0;
    script_breakPatchList = NULL;
    script_continueAllowed = qtrue;
    script_continueAllowedInDeveloperBlock = script_codegenMode != 0;
    script_continuePatchList = NULL;
    EmitStatement(bodyNode);
    script_breakAllowed = qfalse;
    script_breakAllowedInDeveloperBlock = qfalse;
    script_continueAllowed = qfalse;
    script_continueAllowedInDeveloperBlock = qfalse;
    ConnectContinueStatements();

    EmitOpcode(SCRIPT_OPCODE_JUMP_BACK, 0, 0);
    AddOpcodePos(loopSourcePos);
    uint8_t *jumpPos = TempMalloc(0);
    EmitInteger((int32_t)(loopStart - jumpPos));

    char *current = (char *)TempMalloc(0);
    const int32_t falseOffset = (int32_t)(current - falsePatch) - (int32_t)sizeof(falseOffset);
    memcpy(falsePatch, &falseOffset, sizeof(falseOffset));
    ConnectBreakStatements();

    script_breakAllowed = saved.breakAllowed;
    script_breakAllowedInDeveloperBlock = saved.breakAllowedInDeveloperBlock;
    script_breakPatchList = saved.breakPatchList;
    script_continueAllowed = saved.continueAllowed;
    script_continueAllowedInDeveloperBlock = saved.continueAllowedInDeveloperBlock;
    script_continuePatchList = saved.continuePatchList;
}

/* Source: CoDUOMP.exe 0x0047da10..0x0047dbdd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047da10_0047dbdd.mcode. */
void EmitDoWhileStatement(scr_ast_node_t *bodyNode, scr_ast_node_t *conditionNode, uint32_t conditionSourcePos, uint32_t loopSourcePos)
{
    EmitNOP();

    script_loop_patch_state_t saved = {script_breakAllowed,    script_breakAllowedInDeveloperBlock,    script_breakPatchList,
                                       script_continueAllowed, script_continueAllowedInDeveloperBlock, script_continuePatchList};
    script_breakAllowed = qtrue;
    script_breakAllowedInDeveloperBlock = script_codegenMode != 0;
    script_breakPatchList = NULL;
    script_continueAllowed = qtrue;
    script_continueAllowedInDeveloperBlock = script_codegenMode != 0;
    script_continuePatchList = NULL;

    uint8_t *loopStart = TempMalloc(0);
    EmitStatement(bodyNode);
    script_breakAllowed = qfalse;
    script_breakAllowedInDeveloperBlock = qfalse;
    script_continueAllowed = qfalse;
    script_continueAllowedInDeveloperBlock = qfalse;
    ConnectContinueStatements();
    EmitExpression(conditionNode);
    EmitOpcode(SCRIPT_OPCODE_JUMP_ON_TRUE_BACK, -1, 0);
    AddOpcodePos(loopSourcePos);
    AddOpcodePos(conditionSourcePos);
    uint8_t *jumpPos = TempMalloc(0);
    EmitInteger((int32_t)(loopStart - jumpPos));
    ConnectBreakStatements();

    script_breakAllowed = saved.breakAllowed;
    script_breakAllowedInDeveloperBlock = saved.breakAllowedInDeveloperBlock;
    script_breakPatchList = saved.breakPatchList;
    script_continueAllowed = saved.continueAllowed;
    script_continueAllowedInDeveloperBlock = saved.continueAllowedInDeveloperBlock;
    script_continuePatchList = saved.continuePatchList;
}

/* Source: CoDUOMP.exe 0x0047dbe0..0x0047de4b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047dbe0_0047de4b.mcode. */
void EmitForStatement(scr_ast_node_t *initNode, scr_ast_node_t *conditionNode, scr_ast_node_t *incrementNode, scr_ast_node_t *bodyNode,
                      uint32_t conditionSourcePos, uint32_t loopSourcePos)
{
    script_loop_patch_state_t saved = {script_breakAllowed,    script_breakAllowedInDeveloperBlock,    script_breakPatchList,
                                       script_continueAllowed, script_continueAllowedInDeveloperBlock, script_continuePatchList};
    script_breakAllowed = qfalse;
    script_breakAllowedInDeveloperBlock = qfalse;
    script_continueAllowed = qfalse;
    script_continueAllowedInDeveloperBlock = qfalse;

    EmitStatement(initNode);
    EmitNOP();
    uint8_t *loopStart = TempMalloc(0);

    char *falsePatch = NULL;
    if (conditionNode->kind == SCR_AST_KIND_FOR_CONDITION) {
        EmitExpression(conditionNode->payload.child.node);
        EmitOpcode(SCRIPT_OPCODE_JUMP_ON_FALSE, -1, 0);
        AddOpcodePos(conditionSourcePos);
        EmitInteger(0);
        falsePatch = (char *)script_codeEmitCursor;
    }

    script_breakAllowed = qtrue;
    script_breakAllowedInDeveloperBlock = script_codegenMode != 0;
    script_breakPatchList = NULL;
    script_continueAllowed = qtrue;
    script_continueAllowedInDeveloperBlock = script_codegenMode != 0;
    script_continuePatchList = NULL;
    EmitStatement(bodyNode);
    script_breakAllowed = qfalse;
    script_breakAllowedInDeveloperBlock = qfalse;
    script_continueAllowed = qfalse;
    script_continueAllowedInDeveloperBlock = qfalse;
    ConnectContinueStatements();
    EmitStatement(incrementNode);
    EmitOpcode(SCRIPT_OPCODE_JUMP_BACK, 0, 0);
    /* The original records only loopSourcePos after the back-jump (single
     * CALL 0x00481280 at 0x0047dda0, arg [ESP+0x34]=loopSourcePos). The extra
     * RecordSourcePosition(conditionSourcePos) here pushed a spurious entry
     * into the opcode->source-position stream, desyncing runtime error line
     * numbers for all code after any for-loop. */
    AddOpcodePos(loopSourcePos);
    uint8_t *jumpPos = TempMalloc(0);
    EmitInteger((int32_t)(loopStart - jumpPos));

    if (falsePatch != NULL) {
        char *current = (char *)TempMalloc(0);
        const int32_t falseOffset = (int32_t)(current - falsePatch) - (int32_t)sizeof(falseOffset);
        memcpy(falsePatch, &falseOffset, sizeof(falseOffset));
    }

    ConnectBreakStatements();
    script_breakAllowed = saved.breakAllowed;
    script_breakAllowedInDeveloperBlock = saved.breakAllowedInDeveloperBlock;
    script_breakPatchList = saved.breakPatchList;
    script_continueAllowed = saved.continueAllowed;
    script_continueAllowedInDeveloperBlock = saved.continueAllowedInDeveloperBlock;
    script_continuePatchList = saved.continuePatchList;
}

/* Source: CoDUOMP.exe 0x0047de50..0x0047dee2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047de50_0047dee2.mcode. */
void EmitIncStatement(scr_ast_node_t *refNode, uint32_t sourcePos)
{
    EmitVariableExpressionRef(refNode);
    EmitOpcode(SCRIPT_OPCODE_INC, 1, 0);
    AddOpcodePos(sourcePos);
    EmitSetVariableField(sourcePos);
}

/* Source: CoDUOMP.exe 0x0047def0..0x0047df82.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047def0_0047df82.mcode. */
void EmitDecStatement(scr_ast_node_t *refNode, uint32_t sourcePos)
{
    EmitVariableExpressionRef(refNode);
    EmitOpcode(SCRIPT_OPCODE_DEC, 1, 0);
    AddOpcodePos(sourcePos);
    EmitSetVariableField(sourcePos);
}

/* Source: CoDUOMP.exe 0x0047df90..0x0047dfb2.
 * Evidence:
 * coduomp/mcode/CoDUOMP/FUN_0047df90_0047dfb2.mcode.
 * Name: exact same-module Mac symbol
 * EmitFormalParameterListRefInternal. The Windows compiler separately emitted
 * this helper and also inlined it into EmitFormalParameterListRef. */
void EmitFormalParameterListRefInternal(scr_ast_list_item_t *listItem)
{
    for (scr_ast_list_item_t *item = listItem->next; item != NULL; item = item->next) {
        EmitSafeSetVariableField(SCR_AST_STRING_HANDLE(item->stringEntry->stringHandle), item->stringEntry->sourcePos);
    }
}

/* Source: CoDUOMP.exe 0x0047dfc0..0x0047dfe2.
 * Evidence:
 * coduomp/mcode/CoDUOMP/FUN_0047dfc0_0047dfe2.mcode.
 * Name: exact same-module Mac symbol
 * EmitFormalWaittillParameterListRefInternal. The Windows compiler separately
 * emitted this helper and also inlined it into EmitWaittillStatement. */
void EmitFormalWaittillParameterListRefInternal(scr_ast_list_item_t *listItem)
{
    for (scr_ast_list_item_t *item = listItem->next; item != NULL; item = item->next) {
        EmitSafeSetWaittillVariableField(SCR_AST_STRING_HANDLE(item->stringEntry->stringHandle), item->stringEntry->sourcePos);
    }
}

/* Source: CoDUOMP.exe 0x0047dff0..0x0047e147.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047dff0_0047e147.mcode. */
void EmitWaittillStatement(scr_ast_node_t *objectNode, scr_ast_list_t *list, uint32_t objectSourcePos, uint32_t opcodeSourcePos)
{
    if (script_codegenMode != 0) {
        CompileError(opcodeSourcePos, "waittill not allowed in developer script");
    }

    scr_ast_list_item_t *eventItem = list->head->next;
    EmitExpression(eventItem->entry->node);
    EmitPrimitiveExpression(objectNode);
    EmitOpcode(SCRIPT_OPCODE_WAITTILL, -2, 0);
    AddOpcodePos(opcodeSourcePos);
    AddOpcodePos(eventItem->entry->sourcePos);
    AddOpcodePos(objectSourcePos);
    EmitFormalWaittillParameterListRefInternal(eventItem);
    EmitOpcode(SCRIPT_OPCODE_WAITTILL_END, 0, 0);
}

/* Source: CoDUOMP.exe 0x0047e150..0x0047e2ee.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047e150_0047e2ee.mcode. */
void EmitWaittillmatchStatement(scr_ast_node_t *objectNode, scr_ast_list_t *list, uint32_t objectSourcePos, uint32_t opcodeSourcePos)
{
    if (script_codegenMode != 0) {
        CompileError(opcodeSourcePos, "waittillmatch not allowed in developer script");
    }

    scr_ast_list_item_t *eventItem = list->head->next;
    uint32_t matchCount = 0;
    for (scr_ast_list_item_t *item = eventItem->next; item != NULL; item = item->next) {
        matchCount++;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: the bytecode stores this expression count in a
     * signed byte. Reject an unrepresentable count before emitting any part of
     * the statement. */
    if (matchCount > SCRIPT_WAITTILLMATCH_MATCH_COUNT_LIMIT) {
        CompileError(opcodeSourcePos, "waittillmatch has too many match expressions (maximum %i)", SCRIPT_WAITTILLMATCH_MATCH_COUNT_LIMIT);
        return;
    }

    for (scr_ast_list_item_t *item = eventItem->next; item != NULL; item = item->next) {
        EmitExpression(item->entry->node);
    }

    EmitExpression(eventItem->entry->node);
    EmitPrimitiveExpression(objectNode);
    EmitOpcode(SCRIPT_OPCODE_WAITTILLMATCH, -2 - (int32_t)matchCount, 0);
    AddOpcodePos(opcodeSourcePos);
    AddOpcodePos(eventItem->entry->sourcePos);
    AddOpcodePos(objectSourcePos);
    for (scr_ast_list_item_t *item = eventItem->next; item != NULL; item = item->next) {
        AddOpcodePos(item->entry->sourcePos);
    }
    EmitByte((uint8_t)matchCount);
    EmitOpcode(SCRIPT_OPCODE_WAITTILL_END, 0, 0);
}

/* Source: CoDUOMP.exe 0x0047e2f0..0x0047e46d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047e2f0_0047e46d.mcode. */
void EmitNotifyStatement(scr_ast_node_t *objectNode, scr_ast_list_t *list, uint32_t objectSourcePos, uint32_t opcodeSourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_PUSH_CODEPOS, 1, 0);

    int32_t argumentCount = 0;
    scr_ast_list_item_t *lastItem = NULL;
    for (scr_ast_list_item_t *item = list->head; item != NULL; item = item->next) {
        lastItem = item;
        EmitExpression(item->entry->node);
        argumentCount++;
    }

    EmitPrimitiveExpression(objectNode);
    EmitOpcode(SCRIPT_OPCODE_NOTIFY, -2 - argumentCount, 0);
    AddOpcodePos(opcodeSourcePos);
    AddOpcodePos(lastItem->entry->sourcePos);
    AddOpcodePos(objectSourcePos);
}

/* Source: CoDUOMP.exe 0x0047e470..0x0047e514.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047e470_0047e514.mcode. */
void EmitEndOnStatement(scr_ast_node_t *objectNode, scr_ast_node_t *eventNode, uint32_t objectSourcePos, uint32_t opcodeSourcePos)
{
    EmitExpression(eventNode);
    EmitPrimitiveExpression(objectNode);
    EmitOpcode(SCRIPT_OPCODE_ENDON, -2, 0);
    AddOpcodePos(opcodeSourcePos);
    AddOpcodePos(objectSourcePos);
}

/* NOT_FROM_ORIGINAL_SOURCE: readable grouping of the six independently
 * saved switch-control globals. CoDUOMP.exe has no aggregate ABI for this
 * source-level save/restore convenience. */
typedef struct script_switch_patch_state_s {
    uint8_t caseAllowed;
    uint8_t caseAllowedInDeveloperBlock;
    script_switch_case_record_t *caseList;
    uint8_t breakAllowed;
    uint8_t breakAllowedInDeveloperBlock;
    script_code_offset_patch_t *breakPatchList;
} script_switch_patch_state_t;

/* Source: CoDUOMP.exe 0x0047e520..0x0047e539.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047e520_0047e539.mcode. */
int CompareCaseInfo(const void *left, const void *right)
{
    const script_switch_case_table_entry_t *leftCase = left;
    const script_switch_case_table_entry_t *rightCase = right;

    if (rightCase->value < leftCase->value) {
        return -1;
    }
    if (leftCase->value < rightCase->value) {
        return 1;
    }
    return 0;
}

/* Source: CoDUOMP.exe 0x0047e8d0..0x0047e941.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047e8d0_0047e941.mcode. */
void EmitCaseStatementInfo(uint32_t value, uint32_t sourcePos)
{
    if (script_codegenMode == SCRIPT_CODEGEN_MODE_RELEASE_STRINGS) {
        return;
    }

    script_switch_case_record_t *record = Hunk_AllocateTempMemoryHighInternal(sizeof(*record));
    record->value = value;
    record->codePos = TempMalloc(0);
    if (script_codegenMode == SCRIPT_CODEGEN_MODE_RECORD_STRING_FIXUPS) {
        record->codePos =
            (uint8_t *)((uintptr_t)record->codePos + ((uintptr_t)script_codeRelocationEnd - (uintptr_t)script_codeRelocationStart));
    }
    record->sourcePos = sourcePos;
    record->next = script_caseRecordList;
    script_caseRecordList = record;
}

/* Source: CoDUOMP.exe 0x0047e950..0x0047ea1a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047e950_0047ea1a.mcode. */
void EmitCaseStatement(scr_ast_node_t *valueNode, uint32_t sourcePos)
{
    uint32_t caseValue;

    if (valueNode->kind == SCR_AST_KIND_INTEGER_LITERAL) {
        int32_t integerValue = (int32_t)valueNode->payload.literal.value;
        if ((uint32_t)integerValue + SCRIPT_CASE_INTEGER_TEST_BIAS >= SCRIPT_CASE_INTEGER_TEST_LIMIT) {
            CompileError(sourcePos, va("case index %d out of range", integerValue));
            return;
        }
        /* The original subtracts the bias (ADD ESI,0xff800000 = -0x800000 at
         * 0x0047e9bb) then masks — the entity-key normalization form, matching
         * GetInternalVariableIndex. Adding +0x800000 gives the same
         * masked result but the instruction subtracts; match it. */
        caseValue = ((uint32_t)integerValue - SCRIPT_CASE_INTEGER_ENCODE_BIAS) & SCRIPT_CASE_INTEGER_ENCODE_MASK;
    } else {
        if (valueNode->kind != SCR_AST_KIND_STRING) {
            CompileError(sourcePos, "case expression must be an int or string");
            EmitPrimitiveExpression(valueNode);
            return;
        }

        caseValue = SCR_AST_STRING_HANDLE(valueNode->payload.sourceString.stringHandle);
        CompileTransferRefToString((uint16_t)caseValue, SCRIPT_STRING_USAGE_CASE);
    }

    if (script_caseAllowed == qfalse) {
        CompileError(sourcePos, "illegal case statement");
    } else if (script_caseAllowedInDeveloperBlock == qfalse && script_codegenMode != 0) {
        CompileError(sourcePos, "cannot use /# ... #/ comments directly around a case statement");
    } else {
        EmitCaseStatementInfo(caseValue, sourcePos);
    }
}

/* Source: CoDUOMP.exe 0x0047e540..0x0047e8c4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047e540_0047e8c4.mcode. */
void EmitSwitchStatement(scr_ast_node_t *valueNode, scr_ast_node_t *bodyNode, uint32_t sourcePos)
{
    script_switch_patch_state_t saved = {script_caseAllowed,  script_caseAllowedInDeveloperBlock,  script_caseRecordList,
                                         script_breakAllowed, script_breakAllowedInDeveloperBlock, script_breakPatchList};
    script_caseAllowed = qfalse;
    script_caseAllowedInDeveloperBlock = qfalse;
    script_breakAllowed = qfalse;
    script_breakAllowedInDeveloperBlock = qfalse;

    EmitExpression(valueNode);
    EmitOpcode(SCRIPT_OPCODE_SWITCH_JUMP, -1, 0);
    EmitInteger(0);
    char *tablePatch = (char *)script_codeEmitCursor;

    script_caseAllowed = qtrue;
    script_caseAllowedInDeveloperBlock = script_codegenMode != 0;
    script_caseRecordList = NULL;
    script_breakAllowed = qtrue;
    script_breakAllowedInDeveloperBlock = script_codegenMode != 0;
    script_breakPatchList = NULL;
    EmitStatement(bodyNode);
    script_caseAllowed = qfalse;
    script_caseAllowedInDeveloperBlock = qfalse;
    script_breakAllowed = qfalse;
    script_breakAllowedInDeveloperBlock = qfalse;

    EmitOpcode(SCRIPT_OPCODE_SWITCH_TABLE, 0, 0);
    AddOpcodePos(sourcePos);
    EmitShort(0);
    script_switch_case_table_entry_t *table = (script_switch_case_table_entry_t *)TempMalloc(0);
    const ptrdiff_t tableOffset = (char *)table - tablePatch;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (tableOffset < 0 || tableOffset > SCRIPT_SWITCH_TABLE_DISPLACEMENT_LIMIT) {
        CompileError(sourcePos, "switch body exceeds maximum bytecode span of %i bytes", SCRIPT_SWITCH_TABLE_DISPLACEMENT_LIMIT);
        return;
    }

    const int32_t encodedTableOffset = (int32_t)tableOffset;
    memcpy(tablePatch, &encodedTableOffset, sizeof(encodedTableOffset));

    uint32_t caseCount = 0;
    for (script_switch_case_record_t *record = script_caseRecordList; record != NULL; record = record->next) {
        caseCount++;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: the bytecode stores this case count in a signed
     * word. Reject an unrepresentable count before emitting any table row. */
    if (caseCount > SCRIPT_SWITCH_CASE_COUNT_LIMIT) {
        CompileError(sourcePos, "switch has too many cases (maximum %i)", SCRIPT_SWITCH_CASE_COUNT_LIMIT);
        return;
    }

    for (script_switch_case_record_t *record = script_caseRecordList; record != NULL; record = record->next) {
        EmitInteger(record->value);
        coduomp_script_emit_value_payload((coduo_script_value_payload_t)(uintptr_t)record->codePos);
    }

    uint8_t *tableBytes = (uint8_t *)table;
    const uint16_t emittedCaseCount = (uint16_t)caseCount;
    memcpy(tableBytes - sizeof(emittedCaseCount), &emittedCaseCount, sizeof(emittedCaseCount));
    coduo_qsort(table, caseCount, sizeof(*table), CompareCaseInfo);

    for (uint32_t index = 1; index < caseCount; index++) {
        if (table[index - 1].value != table[index].value) {
            continue;
        }

        for (script_switch_case_record_t *record = script_caseRecordList; record != NULL; record = record->next) {
            if (record->value == table[index - 1].value) {
                CompileError(record->sourcePos, "duplicate case expression");
            }
        }
    }

    ConnectBreakStatements();
    script_caseAllowed = saved.caseAllowed;
    script_caseAllowedInDeveloperBlock = saved.caseAllowedInDeveloperBlock;
    script_caseRecordList = saved.caseList;
    script_breakAllowed = saved.breakAllowed;
    script_breakAllowedInDeveloperBlock = saved.breakAllowedInDeveloperBlock;
    script_breakPatchList = saved.breakPatchList;
}

/* Source: CoDUOMP.exe 0x0047ea20..0x0047ea4c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047ea20_0047ea4c.mcode. */
void EmitDefaultStatement(uint32_t sourcePos)
{
    if (script_caseAllowed == qfalse) {
        CompileError(sourcePos, "illegal default statement");
        return;
    }

    EmitCaseStatementInfo(0, sourcePos);
}

/* Source: CoDUOMP.exe 0x0047ea50..0x0047eae7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047ea50_0047eae7.mcode. */
void EmitBreakStatement(uint32_t sourcePos)
{
    if (script_breakAllowed == qfalse) {
        CompileError(sourcePos, "illegal break statement");
        return;
    }
    if (script_breakAllowedInDeveloperBlock == qfalse && script_codegenMode != 0) {
        CompileError(sourcePos, "cannot use /# ... #/ comments directly around a break statement");
        return;
    }

    EmitOpcode(SCRIPT_OPCODE_JUMP_FORWARD, 0, 0);
    EmitInteger(0);

    script_code_offset_patch_t *patch = Hunk_AllocateTempMemoryHighInternal(sizeof(*patch));
    patch->patch = (char *)script_codeEmitCursor;
    patch->next = script_breakPatchList;
    script_breakPatchList = patch;
}

/* Source: CoDUOMP.exe 0x0047eaf0..0x0047eb87.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047eaf0_0047eb87.mcode. */
void EmitContinueStatement(uint32_t sourcePos)
{
    if (script_continueAllowed == qfalse) {
        CompileError(sourcePos, "illegal continue statement");
        return;
    }
    if (script_continueAllowedInDeveloperBlock == qfalse && script_codegenMode != 0) {
        CompileError(sourcePos, "cannot use /# ... #/ comments directly around a continue statement");
        return;
    }

    EmitOpcode(SCRIPT_OPCODE_JUMP_FORWARD, 0, 0);
    EmitInteger(0);

    script_code_offset_patch_t *patch = Hunk_AllocateTempMemoryHighInternal(sizeof(*patch));
    patch->patch = (char *)script_codeEmitCursor;
    patch->next = script_continuePatchList;
    script_continuePatchList = patch;
}

/* Source: CoDUOMP.exe 0x0047eb90..0x0047ed93.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047eb90_0047ed93.mcode. */
void EmitStatement(scr_ast_node_t *node)
{
    switch (node->kind) {
    case SCR_AST_KIND_ASSIGNMENT_STATEMENT:
        EmitAssignmentStatement(node->payload.assignmentStatement.refNode, node->payload.assignmentStatement.valueNode,
                                node->payload.assignmentStatement.sourcePos);
        break;
    case SCR_AST_KIND_CALL_STATEMENT:
        EmitExpressionStatement(node->payload.child.node);
        break;
    case SCR_AST_KIND_RETURN_VALUE_STATEMENT:
        EmitReturnStatement(node->payload.returnValueStatement.valueNode);
        break;
    case SCR_AST_KIND_RETURN_STATEMENT:
        EmitEndStatement();
        break;
    case SCR_AST_KIND_WAIT_STATEMENT:
        EmitWaitStatement(node->payload.waitStatement.timeNode, node->payload.waitStatement.timeSourcePos,
                          node->payload.waitStatement.opcodeSourcePos);
        break;
    case SCR_AST_KIND_IF_STATEMENT:
        EmitIfStatement(node->payload.ifStatement.conditionNode, node->payload.ifStatement.bodyNode, node->payload.ifStatement.sourcePos);
        break;
    case SCR_AST_KIND_IF_ELSE_STATEMENT:
        EmitIfElseStatement(node->payload.ifElseStatement.conditionNode, node->payload.ifElseStatement.thenNode,
                            node->payload.ifElseStatement.elseNode, node->payload.ifElseStatement.sourcePos);
        break;
    case SCR_AST_KIND_WHILE_STATEMENT:
        EmitWhileStatement(node->payload.whileStatement.conditionNode, node->payload.whileStatement.bodyNode,
                           node->payload.whileStatement.conditionSourcePos, node->payload.whileStatement.loopSourcePos);
        break;
    case SCR_AST_KIND_DO_WHILE_STATEMENT:
        EmitDoWhileStatement(node->payload.doWhileStatement.bodyNode, node->payload.doWhileStatement.conditionNode,
                             node->payload.doWhileStatement.conditionSourcePos, node->payload.doWhileStatement.loopSourcePos);
        break;
    case SCR_AST_KIND_FOR_STATEMENT:
        EmitForStatement(node->payload.forStatement.initNode, node->payload.forStatement.conditionNode,
                         node->payload.forStatement.incrementNode, node->payload.forStatement.bodyNode,
                         node->payload.forStatement.conditionSourcePos, node->payload.forStatement.loopSourcePos);
        break;
    case SCR_AST_KIND_INC_STATEMENT:
        EmitIncStatement(node->payload.incDecStatement.refNode, node->payload.incDecStatement.sourcePos);
        break;
    case SCR_AST_KIND_DEC_STATEMENT:
        EmitDecStatement(node->payload.incDecStatement.refNode, node->payload.incDecStatement.sourcePos);
        break;
    case SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT:
        EmitBinaryEqualsOperatorExpression(node->payload.refAssignmentStatement.refNode, node->payload.refAssignmentStatement.valueNode,
                                           (uint8_t)node->payload.refAssignmentStatement.opcode,
                                           node->payload.refAssignmentStatement.sourcePos);
        break;
    case SCR_AST_KIND_STATEMENT_BLOCK:
        EmitStatementList(node->payload.statementBlock.block);
        break;
    case SCR_AST_KIND_DEVELOPER_STATEMENT_BLOCK:
        EmitDeveloperStatementList(node->payload.developerStatementBlock.block, node->payload.developerStatementBlock.sourcePos);
        break;
    case SCR_AST_KIND_WAITTILL_STATEMENT:
        EmitWaittillStatement(node->payload.waittillStatement.objectNode, node->payload.waittillStatement.list,
                              node->payload.waittillStatement.objectSourcePos, node->payload.waittillStatement.opcodeSourcePos);
        break;
    case SCR_AST_KIND_WAITTILLMATCH_STATEMENT:
        EmitWaittillmatchStatement(node->payload.waittillStatement.objectNode, node->payload.waittillStatement.list,
                                   node->payload.waittillStatement.objectSourcePos, node->payload.waittillStatement.opcodeSourcePos);
        break;
    case SCR_AST_KIND_NOTIFY_STATEMENT:
        EmitNotifyStatement(node->payload.notifyStatement.objectNode, node->payload.notifyStatement.list,
                            node->payload.notifyStatement.objectSourcePos, node->payload.notifyStatement.opcodeSourcePos);
        break;
    case SCR_AST_KIND_ENDON_STATEMENT:
        EmitEndOnStatement(node->payload.endonStatement.objectNode, node->payload.endonStatement.eventNode,
                           node->payload.endonStatement.objectSourcePos, node->payload.endonStatement.opcodeSourcePos);
        break;
    case SCR_AST_KIND_SWITCH_STATEMENT:
        EmitSwitchStatement(node->payload.switchStatement.valueNode, node->payload.switchStatement.bodyNode,
                            node->payload.switchStatement.sourcePos);
        break;
    case SCR_AST_KIND_CASE_STATEMENT:
        EmitCaseStatement(node->payload.caseStatement.valueNode, node->payload.caseStatement.sourcePos);
        break;
    case SCR_AST_KIND_DEFAULT_STATEMENT:
        EmitDefaultStatement(node->payload.sourceOnlyStatement.sourcePos);
        break;
    case SCR_AST_KIND_BREAK_STATEMENT:
        EmitBreakStatement(node->payload.sourceOnlyStatement.sourcePos);
        break;
    case SCR_AST_KIND_CONTINUE_STATEMENT:
        EmitContinueStatement(node->payload.sourceOnlyStatement.sourcePos);
        break;
    default:
        break;
    }
}

/* Source: CoDUOMP.exe 0x0047f2c0..0x0047f35e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047f2c0_0047f35e.mcode. */
void EmitFormalParameterListRef(scr_ast_list_t *parameters, uint32_t sourcePos)
{
    EmitFormalParameterListRefInternal(parameters->head);
    EmitOpcode(SCRIPT_OPCODE_FUNCTION_PARAMETERS_DONE, 0, 0);
    AddOpcodePos(sourcePos);
}

/* Source: CoDUOMP.exe 0x0047f360..0x0047f3dc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047f360_0047f3dc.mcode. */
void SpecifyThread(scr_ast_node_t *node)
{
    if (node->kind == SCR_AST_KIND_FUNCTION_DEFINITION) {
        uint16_t functionName = SCR_AST_STRING_HANDLE(node->payload.functionDefinition.nameHandle);
        uint16_t functionObject = GetObject(GetVariable(script_currentFunctionRoot, functionName));
        SpecifyThreadPosition(functionObject, node->payload.functionDefinition.sourcePos);
        return;
    }

    if (node->kind == SCR_AST_KIND_DEVELOPER_FUNCTION_DEFINITION) {
        uint16_t functionName = SCR_AST_STRING_HANDLE(node->payload.functionDefinition.nameHandle);
        uint16_t functionObject = GetObject(GetVariable(script_currentFunctionRoot, functionName));
        SpecifyDeveloperThreadPosition(functionObject, node->payload.functionDefinition.sourcePos);
    }
}
