#ifndef SHARED_SCRIPT_COMPILE_STATEMENTS_H
#define SHARED_SCRIPT_COMPILE_STATEMENTS_H

#include "script_compile_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern script_code_offset_patch_t *script_breakPatchList;
extern script_code_offset_patch_t *script_continuePatchList;
extern uint8_t script_caseAllowed;
extern uint8_t script_caseAllowedInDeveloperBlock;
extern script_switch_case_record_t *script_caseRecordList;
extern uint8_t script_breakAllowed;
extern uint8_t script_breakAllowedInDeveloperBlock;
extern uint8_t script_continueAllowed;
extern uint8_t script_continueAllowedInDeveloperBlock;

void EmitExpressionStatement(scr_ast_node_t *node);
void EmitReturnStatement(scr_ast_node_t *node);
void EmitEndStatement(void);
void EmitWaitStatement(scr_ast_node_t *timeNode, uint32_t timeSourcePos, uint32_t opcodeSourcePos);
void EmitIfStatement(scr_ast_node_t *conditionNode, scr_ast_node_t *bodyNode, uint32_t sourcePos);
void EmitIfElseStatement(scr_ast_node_t *conditionNode, scr_ast_node_t *thenNode, scr_ast_node_t *elseNode, uint32_t sourcePos);
void EmitWhileStatement(scr_ast_node_t *conditionNode, scr_ast_node_t *bodyNode, uint32_t conditionSourcePos, uint32_t loopSourcePos);
void EmitDoWhileStatement(scr_ast_node_t *bodyNode, scr_ast_node_t *conditionNode, uint32_t conditionSourcePos, uint32_t loopSourcePos);
void EmitForStatement(scr_ast_node_t *initNode, scr_ast_node_t *conditionNode, scr_ast_node_t *incrementNode, scr_ast_node_t *bodyNode,
                      uint32_t conditionSourcePos, uint32_t loopSourcePos);
void EmitIncStatement(scr_ast_node_t *refNode, uint32_t sourcePos);
void EmitDecStatement(scr_ast_node_t *refNode, uint32_t sourcePos);
void EmitFormalParameterListRefInternal(scr_ast_list_item_t *listItem);
void EmitFormalWaittillParameterListRefInternal(scr_ast_list_item_t *listItem);
void EmitWaittillStatement(scr_ast_node_t *objectNode, scr_ast_list_t *list, uint32_t objectSourcePos, uint32_t opcodeSourcePos);
void EmitWaittillmatchStatement(scr_ast_node_t *objectNode, scr_ast_list_t *list, uint32_t objectSourcePos, uint32_t opcodeSourcePos);
void EmitNotifyStatement(scr_ast_node_t *objectNode, scr_ast_list_t *list, uint32_t objectSourcePos, uint32_t opcodeSourcePos);
void EmitEndOnStatement(scr_ast_node_t *objectNode, scr_ast_node_t *eventNode, uint32_t objectSourcePos, uint32_t opcodeSourcePos);
int CompareCaseInfo(const void *left, const void *right);
void EmitCaseStatementInfo(uint32_t value, uint32_t sourcePos);
void EmitSwitchStatement(scr_ast_node_t *valueNode, scr_ast_node_t *bodyNode, uint32_t sourcePos);
void EmitCaseStatement(scr_ast_node_t *valueNode, uint32_t sourcePos);
void EmitDefaultStatement(uint32_t sourcePos);
void EmitBreakStatement(uint32_t sourcePos);
void EmitContinueStatement(uint32_t sourcePos);
void EmitStatement(scr_ast_node_t *node);
void EmitFormalParameterListRef(scr_ast_list_t *parameters, uint32_t sourcePos);
void SpecifyThread(scr_ast_node_t *node);

#ifdef __cplusplus
}
#endif

#endif
