#ifndef SHARED_SCRIPT_COMPILE_EXPR_H
#define SHARED_SCRIPT_COMPILE_EXPR_H

#include "script_compile_types.h"
#include "qcommon/q_shared_types.h"

#include <stdint.h>

void EmitGetFunction(scr_ast_node_t *nameNode, uint32_t sourcePos);
void EmitArrayVariable(scr_ast_node_t *objectNode,
                       scr_ast_node_t *indexNode,
                       uint32_t objectSourcePos,
                       uint32_t indexSourcePos);
void EmitArrayVariableRef(scr_ast_node_t *objectNode,
                          scr_ast_node_t *indexNode,
                          uint32_t objectSourcePos,
                          uint32_t indexSourcePos);
void EmitClearArrayVariable(scr_ast_node_t *objectNode,
                            scr_ast_node_t *indexNode,
                            uint32_t objectSourcePos,
                            uint32_t indexSourcePos);
void EmitVariableExpression(scr_ast_node_t *node);
int32_t EmitExpressionList(scr_ast_list_t *list);
scr_ast_list_item_t *GetSingleParameter(scr_ast_list_t *list);
void AddExpressionListOpcodePos(scr_ast_list_t *list);
uint16_t AddFilePrecache(uint16_t filename, uint32_t sourcePos);
void EmitFunction(scr_ast_node_t *node, uint32_t sourcePos);

void EmitPostScriptFunction(scr_ast_node_t *functionNode,
                            int32_t argumentCount,
                            qboolean hasMethodContext,
                            uint32_t sourcePos);
void EmitPostScriptFunctionPointer(scr_ast_node_t *functionExpression,
                                   int32_t argumentCount,
                                   qboolean hasMethodContext,
                                   uint32_t callSourcePos,
                                   uint32_t functionSourcePos);
void EmitPostScriptThread(scr_ast_node_t *functionNode,
                          int32_t argumentCount,
                          qboolean hasMethodContext,
                          uint32_t sourcePos);
void EmitPostScriptThreadPointer(scr_ast_node_t *functionExpression,
                                 int32_t argumentCount,
                                 qboolean hasMethodContext,
                                 uint32_t sourcePos);
void EmitPostScriptFunctionCall(scr_ast_node_t *callee,
                                int32_t argumentCount,
                                qboolean hasMethodContext,
                                uint32_t callSourcePos);
void EmitPostScriptThreadCall(scr_ast_node_t *callee,
                              int32_t argumentCount,
                              qboolean hasMethodContext,
                              uint32_t callSourcePos,
                              uint32_t functionSourcePos);
void EmitPreFunctionCall(scr_ast_node_t *call);
void EmitPostFunctionCall(scr_ast_node_t *call,
                          int32_t argumentCount,
                          qboolean hasMethodContext);
uint16_t GetBuiltin(scr_ast_node_t *call);
void EmitCall(scr_ast_node_t *call, scr_ast_list_t *arguments,
              qboolean isStatement);
void EmitMethod(scr_ast_node_t *object, scr_ast_node_t *call,
                scr_ast_list_t *arguments, uint32_t objectSourcePos,
                qboolean isStatement);

void AdjustFunctionAddresses(void);
void SpecifyThreadPosition(uint16_t functionObject, uint32_t sourcePos);
void SpecifyDeveloperThreadPosition(uint16_t functionObject,
                                    uint32_t sourcePos);
void SetThreadPosition(uint16_t functionObject, uint32_t sourcePos);
void SetDeveloperThreadPosition(uint16_t functionObject,
                                uint32_t sourcePos);

void EmitSize(scr_ast_node_t *node, uint32_t sourcePos);
void EmitCallExpression(scr_ast_node_t *node, qboolean emitDropTop);
void EmitCallExpressionRef(scr_ast_node_t *node);
void EmitCallExpressionFieldObject(scr_ast_node_t *node);
void EmitArrayExpressionListRef(scr_ast_list_t *list,
                                uint32_t sourcePos);
void EmitExpressionListFieldObject(scr_ast_list_t *list,
                                   uint32_t sourcePos);
void EmitPrimitiveExpressionList(scr_ast_list_t *list,
                                 uint32_t sourcePos);
void EmitPrimitiveExpression(scr_ast_node_t *node);
void EmitBoolOrExpression(scr_ast_node_t *leftNode,
                          uint32_t leftSourcePos,
                          scr_ast_node_t *rightNode,
                          uint32_t rightSourcePos);
void EmitBoolAndExpression(scr_ast_node_t *leftNode,
                           uint32_t leftSourcePos,
                           scr_ast_node_t *rightNode,
                           uint32_t rightSourcePos);
void EmitBinaryOperatorExpression(scr_ast_node_t *leftNode,
                                  scr_ast_node_t *rightNode,
                                  uint8_t opcode,
                                  uint32_t sourcePos);
void EmitBinaryEqualsOperatorExpression(scr_ast_node_t *refNode,
                                        scr_ast_node_t *valueNode,
                                        uint8_t opcode,
                                        uint32_t sourcePos);
void EmitExpression(scr_ast_node_t *node);
void EmitVariableExpressionRef(scr_ast_node_t *node);
void EmitArrayExpressionRef(scr_ast_node_t *node, uint32_t sourcePos);
void EmitExpressionFieldObject(scr_ast_node_t *node, uint32_t sourcePos);
void EmitArrayPrimitiveExpressionRef(scr_ast_node_t *node,
                                     uint32_t sourcePos);
void EmitPrimitiveExpressionFieldObject(scr_ast_node_t *node,
                                        uint32_t sourcePos);
void ConnectBreakStatements(void);
void ConnectContinueStatements(void);
void EmitClearVariableExpression(scr_ast_node_t *node);
qboolean IsUndefinedPrimitiveExpression(scr_ast_node_t *node);
qboolean IsUndefinedExpression(scr_ast_node_t *node);
void EmitAssignmentStatement(scr_ast_node_t *refNode,
                             scr_ast_node_t *valueNode,
                             uint32_t sourcePos);

#endif
