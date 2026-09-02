#include "script_runtime_host.h"

#include "script_callbacks.h"
#include "script_code_emit.h"
#include "script_compile_expr.h"
#include "script_source_positions.h"
#include "script_string.h"
#include "script_variable.h"

#include <string.h>

#define SCRIPT_FLOAT_SIGN_BIT UINT32_C(0x80000000)

enum {
    SCRIPT_CODEGEN_MODE_RECORD_STRING_FIXUPS = 1,
    SCRIPT_CODEGEN_MODE_RELEASE_STRINGS = 2,
    SCRIPT_STRING_USAGE_FUNCTION = 2,
    SCRIPT_FUNCTION_REFERENCE_COUNT_CHILD = 0,
    SCRIPT_FUNCTION_DEFINITION_CHILD = 1,
    SCRIPT_FUNCTION_REFERENCE_FIRST_CHILD = 2,
    SCRIPT_OPCODE_FUNCTION_REFERENCE = 0x10,
    SCRIPT_OPCODE_BOOL_AND = 0x37,
    SCRIPT_OPCODE_BOOL_OR = 0x38,
    SCRIPT_OPCODE_STORE_TEMP = 0x1b,
    SCRIPT_OPCODE_PRE_FUNCTION_CALL = 0x24,
    SCRIPT_OPCODE_SCRIPT_FUNCTION = 0x25,
    SCRIPT_OPCODE_SCRIPT_FUNCTION_POINTER = 0x26,
    SCRIPT_OPCODE_SCRIPT_FUNCTION_STATEMENT = 0x27,
    SCRIPT_OPCODE_SCRIPT_FUNCTION_POINTER_STATEMENT = 0x28,
    SCRIPT_OPCODE_SCRIPT_THREAD = 0x29,
    SCRIPT_OPCODE_SCRIPT_THREAD_POINTER = 0x2a,
    SCRIPT_OPCODE_SCRIPT_THREAD_STATEMENT = 0x2b,
    SCRIPT_OPCODE_SCRIPT_THREAD_POINTER_STATEMENT = 0x2c,
    SCRIPT_THREAD_LOCAL_DEPTH_STACK = 2,
    SCRIPT_OPCODE_SIZE = 0x4d,
    SCRIPT_OPCODE_VECTOR_FROM_LIST = 0x55,
    SCRIPT_OPCODE_DEVELOPER_COMMAND = 0x57,
    SCRIPT_OPCODE_BUILTIN_FUNCTION = 0x21,
    SCRIPT_OPCODE_BUILTIN_METHOD = 0x22,
    SCRIPT_CALL_ARGUMENT_LIMIT = 256
};

/* Source: CoDUOMP.exe 0x0047b900..0x0047b974.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047b900_0047b974.mcode. */
void EmitGetFunction(scr_ast_node_t *nameNode, uint32_t sourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_FUNCTION_REFERENCE, 1, 0);
    EmitFunction(nameNode, sourcePos);
}

/* Source: CoDUOMP.exe 0x0047b470..0x0047b496.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047b470_0047b497.mcode.
 * Name: exact same-module Mac symbol EmitArrayVariable. */
void EmitArrayVariable(scr_ast_node_t *objectNode, scr_ast_node_t *indexNode, uint32_t objectSourcePos, uint32_t indexSourcePos)
{
    EmitExpression(indexNode);
    EmitPrimitiveExpression(objectNode);
    EmitEvalArray(objectSourcePos, indexSourcePos);
}

/* Source: CoDUOMP.exe 0x0047b4a0..0x0047b4c9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047b4a0_0047b4ca.mcode.
 * Name: exact same-module Mac symbol EmitArrayVariableRef. */
void EmitArrayVariableRef(scr_ast_node_t *objectNode, scr_ast_node_t *indexNode, uint32_t objectSourcePos, uint32_t indexSourcePos)
{
    EmitExpression(indexNode);
    EmitArrayExpressionRef(objectNode, objectSourcePos);
    EmitEvalArrayRef(objectSourcePos, indexSourcePos);
}

/* Source: CoDUOMP.exe 0x0047b4d0..0x0047b4f9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047b4d0_0047b4fa.mcode.
 * Name: exact same-module Mac symbol EmitClearArrayVariable. */
void EmitClearArrayVariable(scr_ast_node_t *objectNode, scr_ast_node_t *indexNode, uint32_t objectSourcePos, uint32_t indexSourcePos)
{
    EmitExpression(indexNode);
    EmitArrayExpressionRef(objectNode, objectSourcePos);
    EmitClearArray(objectSourcePos, indexSourcePos);
}

/* Source: CoDUOMP.exe 0x0047b500..0x0047b55d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047b500_0047b55d.mcode. */
void EmitVariableExpression(scr_ast_node_t *node)
{
    if (node->kind == SCR_AST_KIND_OBJECT_INDEX_OBJECT_REF) {
        EmitArrayVariable(node->payload.objectIndexObjectRef.objectNode, node->payload.objectIndexObjectRef.indexNode,
                          node->payload.objectIndexObjectRef.objectSourcePos, node->payload.objectIndexObjectRef.indexSourcePos);
        return;
    }

    if (node->kind == SCR_AST_KIND_STRING_REF) {
        EmitLocalVariable(SCR_AST_STRING_HANDLE(node->payload.stringRef.stringHandle));
        return;
    }

    if (node->kind == SCR_AST_KIND_OBJECT_STRING_REF) {
        EmitFieldVariable(node->payload.objectStringRef.objectNode, SCR_AST_STRING_HANDLE(node->payload.objectStringRef.stringHandle),
                          node->payload.objectStringRef.sourcePos);
    }
}

/* Source: CoDUOMP.exe 0x0047b560..0x0047b58a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047b560_0047b58a.mcode. */
int32_t EmitExpressionList(scr_ast_list_t *list)
{
    int32_t count = 0;

    for (scr_ast_list_item_t *item = list->head; item != NULL; item = item->next) {
        EmitExpression(item->entry->node);
        count++;
    }

    return count;
}

/* Source: CoDUOMP.exe 0x0047b590..0x0047b5a9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047b590_0047b5a9.mcode. */
scr_ast_list_item_t *GetSingleParameter(scr_ast_list_t *list)
{
    scr_ast_list_item_t *item = list->head;

    if (item == NULL || item->next != NULL) {
        return NULL;
    }

    return item;
}

/* Source: CoDUOMP.exe 0x0047b5b0..0x0047b5db.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047b5b0_0047b5db.mcode. */
void AddExpressionListOpcodePos(scr_ast_list_t *list)
{
    if (script_runtimeDebugReportFlag == qfalse) {
        return;
    }

    for (scr_ast_list_item_t *item = list->head; item != NULL; item = item->next) {
        AddOpcodePos(item->entry->sourcePos);
    }
}

/* Source: CoDUOMP.exe 0x0047b5e0..0x0047b62a.
 * Name/signature: exact same-module Mac symbol
 * AddFilePrecache(unsigned short, unsigned int). */
uint16_t AddFilePrecache(uint16_t filename, uint32_t sourcePos)
{
    SL_AddRefToString(filename);
    script_pendingScriptLoadCursor->filenameHandle = filename;
    script_pendingScriptLoadCursor->sourcePos = sourcePos;
    ++script_pendingScriptLoadCursor;

    const uint16_t scriptSlot = GetVariable(script_loadScriptHandleRoot, filename);
    return GetObject(scriptSlot);
}

/* Source: CoDUOMP.exe 0x0047b630..0x0047b8f9.
 * Name/signature: exact same-module Mac symbol EmitFunction(sval_u, sval_u).
 * The maintained parameters expose the two proven union members directly:
 * an AST function-reference node and its source position. */
void EmitFunction(scr_ast_node_t *node, uint32_t sourcePos)
{
    if (script_codegenMode == SCRIPT_CODEGEN_MODE_RELEASE_STRINGS) {
        CompileRemoveRefToString(SCR_AST_STRING_HANDLE(node->payload.functionRef.nameHandle));
        if (node->kind == SCR_AST_KIND_SCRIPT_FUNCTION_REF) {
            CompileRemoveRefToString(SCR_AST_STRING_HANDLE(node->payload.scriptFunctionRef.nameHandle));
            --script_pendingScriptLoadCount;
        }
        return;
    }

    uint16_t functionObject = 0;
    if (node->kind == SCR_AST_KIND_FUNCTION_REF) {
        const uint16_t functionName = SCR_AST_STRING_HANDLE(node->payload.functionRef.nameHandle);
        const uint16_t functionHandle = FindVariable(script_currentFunctionRoot, functionName);
        CompileTransferRefToString(functionName, SCRIPT_STRING_USAGE_FUNCTION);
        if (functionHandle == 0) {
            CompileError(sourcePos, "unknown function");
            return;
        }
        functionObject = FindObject(functionHandle);
    } else if (node->kind == SCR_AST_KIND_SCRIPT_FUNCTION_REF) {
        const uint16_t scriptFilename = SCR_AST_STRING_HANDLE(node->payload.scriptFunctionRef.filenameHandle);
        const uint16_t functionName = SCR_AST_STRING_HANDLE(node->payload.scriptFunctionRef.nameHandle);
        const uint16_t canonicalFilename = Scr_CreateCanonicalFilename(SL_ConvertToString(scriptFilename));

        CompileRemoveRefToString(scriptFilename);
        const uint16_t loadedScript = FindVariable(script_loadScriptCodeRoot, canonicalFilename);
        const uint16_t scriptRoot = AddFilePrecache(canonicalFilename, sourcePos);
        CompileRemoveRefToString(canonicalFilename);

        uint16_t functionHandle;
        if (loadedScript == 0) {
            functionHandle = GetVariable(scriptRoot, functionName);
            CompileTransferRefToString(functionName, SCRIPT_STRING_USAGE_FUNCTION);
        } else {
            functionHandle = FindVariable(scriptRoot, functionName);
            CompileTransferRefToString(functionName, SCRIPT_STRING_USAGE_FUNCTION);
            if (functionHandle == 0) {
                CompileError(sourcePos, "unknown function");
                return;
            }
        }

        VariableValue functionValue;
        GetVariableValue(functionHandle, &functionValue);
        if (functionValue.type == SCRIPT_VAR_CODEPOS) {
            coduomp_script_emit_value_payload(functionValue.payload);
            return;
        }
        if (functionValue.type == SCRIPT_VAR_INT) {
            if (script_codegenMode != SCRIPT_CODEGEN_MODE_RECORD_STRING_FIXUPS) {
                CompileError(sourcePos, "normal script cannot reference /# ... #/ comment");
            }
            coduomp_script_emit_value_payload(functionValue.payload);
            return;
        }
        functionObject = GetObject(functionHandle);
    }

    coduomp_script_emit_value_payload(0);
    if (script_codegenMode == SCRIPT_CODEGEN_MODE_RELEASE_STRINGS)
        return;

    const uint16_t countSlot = GetVariable(functionObject, SCRIPT_FUNCTION_REFERENCE_COUNT_CHILD);
    VariableValue countValue;
    GetVariableValue(countSlot, &countValue);
    if (countValue.type == SCRIPT_VAR_UNDEFINED) {
        countValue.payload = 0;
        countValue.type = SCRIPT_VAR_INT;
    }

    const uint16_t referenceSlot = GetVariable(functionObject, (uint32_t)countValue.payload + SCRIPT_FUNCTION_REFERENCE_FIRST_CHILD);
    VariableValue referenceValue = {.payload = (uintptr_t)script_codeEmitCursor, .type = SCRIPT_VAR_CODEPOS};
    if (script_codegenMode == SCRIPT_CODEGEN_MODE_RECORD_STRING_FIXUPS) {
        referenceValue.type = SCRIPT_VAR_INT;
        referenceValue.payload += (uintptr_t)(script_codeRelocationEnd - script_codeRelocationStart);
    }
    SetNewVariableValue(referenceSlot, &referenceValue);

    ++countValue.payload;
    SetVariableValue(countSlot, &countValue);
    AddOpcodePos(sourcePos);
}

/* Source: CoDUOMP.exe 0x0047b980..0x0047b9d8.
 * Name/signature: exact same-module Mac symbol EmitPostScriptFunction. */
void EmitPostScriptFunction(scr_ast_node_t *functionNode, int32_t argumentCount, qboolean hasMethodContext, uint32_t sourcePos)
{
    if (hasMethodContext == qfalse) {
        EmitOpcode(SCRIPT_OPCODE_SCRIPT_FUNCTION, -1 - argumentCount, 3);
    } else {
        EmitOpcode(SCRIPT_OPCODE_SCRIPT_FUNCTION_STATEMENT, -2 - argumentCount, 3);
    }
    EmitFunction(functionNode, sourcePos);
    EmitInteger(argumentCount);
}

/* Source: CoDUOMP.exe 0x0047b9e0..0x0047ba50.
 * Name/signature: exact same-module Mac symbol
 * EmitPostScriptFunctionPointer. */
void EmitPostScriptFunctionPointer(scr_ast_node_t *functionExpression, int32_t argumentCount, qboolean hasMethodContext,
                                   uint32_t callSourcePos, uint32_t functionSourcePos)
{
    EmitExpression(functionExpression);
    if (hasMethodContext == qfalse) {
        EmitOpcode(SCRIPT_OPCODE_SCRIPT_FUNCTION_POINTER, -2 - argumentCount, 3);
    } else {
        EmitOpcode(SCRIPT_OPCODE_SCRIPT_FUNCTION_POINTER_STATEMENT, -3 - argumentCount, 3);
    }
    AddOpcodePos(functionSourcePos);
    AddOpcodePos(callSourcePos);
    EmitInteger(argumentCount);
}

/* Source: CoDUOMP.exe 0x0047ba50..0x0047bb7d.
 * Name/signature: exact same-module Mac symbol EmitPostScriptThread. */
/* Both authoritative implementations maintain the current operand-stack depth
 * here rather than adding another local. Windows performs the equivalent
 * update inline at 0x0047ba50..0x0047bb7c; Linux passes mode 2 to EmitOpcode.
 * The former client recovery's mode 0 was a transcription error. */
void EmitPostScriptThread(scr_ast_node_t *functionNode, int32_t argumentCount, qboolean hasMethodContext, uint32_t sourcePos)
{
    if (hasMethodContext == qfalse) {
        EmitOpcode(SCRIPT_OPCODE_SCRIPT_THREAD, 1 - argumentCount, SCRIPT_THREAD_LOCAL_DEPTH_STACK);
    } else {
        EmitOpcode(SCRIPT_OPCODE_SCRIPT_THREAD_STATEMENT, -argumentCount, SCRIPT_THREAD_LOCAL_DEPTH_STACK);
    }
    EmitFunction(functionNode, sourcePos);
    EmitInteger(argumentCount);
}

/* Source: CoDUOMP.exe 0x0047bb80..0x0047bcb7.
 * Name/signature: exact same-module Mac symbol EmitPostScriptThreadPointer. */
void EmitPostScriptThreadPointer(scr_ast_node_t *functionExpression, int32_t argumentCount, qboolean hasMethodContext, uint32_t sourcePos)
{
    EmitExpression(functionExpression);
    if (hasMethodContext == qfalse) {
        EmitOpcode(SCRIPT_OPCODE_SCRIPT_THREAD_POINTER, -argumentCount, SCRIPT_THREAD_LOCAL_DEPTH_STACK);
    } else {
        EmitOpcode(SCRIPT_OPCODE_SCRIPT_THREAD_POINTER_STATEMENT, -1 - argumentCount, SCRIPT_THREAD_LOCAL_DEPTH_STACK);
    }
    AddOpcodePos(sourcePos);
    EmitInteger(argumentCount);
}

/* Source: CoDUOMP.exe 0x0047bcc0..0x0047bcfe, recovered from the executable
 * gap at 0x0047bcb7. Name/signature: exact same-module Mac symbol
 * EmitPostScriptFunctionCall. The Windows compiler also inlines this dispatch
 * into EmitPostFunctionCall. */
void EmitPostScriptFunctionCall(scr_ast_node_t *callee, int32_t argumentCount, qboolean hasMethodContext, uint32_t callSourcePos)
{
    if (callee->kind == SCR_AST_KIND_SCRIPT_FUNCTION_NAME) {
        EmitPostScriptFunction(callee->payload.namedCall.functionNode, argumentCount, hasMethodContext, callSourcePos);
    } else if (callee->kind == SCR_AST_KIND_FUNCTION_POINTER_CALL) {
        EmitPostScriptFunctionPointer(callee->payload.pointerCall.functionExpression, argumentCount, hasMethodContext, callSourcePos,
                                      callee->payload.pointerCall.sourcePos);
    }
}

/* Source: CoDUOMP.exe 0x0047bd00..0x0047bd53.
 * Name/signature: exact same-module Mac symbol EmitPostScriptThreadCall. */
void EmitPostScriptThreadCall(scr_ast_node_t *callee, int32_t argumentCount, qboolean hasMethodContext, uint32_t callSourcePos,
                              uint32_t functionSourcePos)
{
    if (callee->kind == SCR_AST_KIND_SCRIPT_FUNCTION_NAME) {
        EmitPostScriptThread(callee->payload.namedCall.functionNode, argumentCount, hasMethodContext, functionSourcePos);
    } else if (callee->kind == SCR_AST_KIND_FUNCTION_POINTER_CALL) {
        EmitPostScriptThreadPointer(callee->payload.pointerCall.functionExpression, argumentCount, hasMethodContext,
                                    callee->payload.pointerCall.sourcePos);
    }
    AddOpcodePos(callSourcePos);
}

/* Source: CoDUOMP.exe 0x0047bd60..0x0047bd78, recovered from the executable
 * gap at 0x0047bd53. Name/signature: exact same-module Mac symbol
 * EmitPreFunctionCall. */
void EmitPreFunctionCall(scr_ast_node_t *call)
{
    if (call->kind == SCR_AST_KIND_FUNCTION_CALL) {
        EmitOpcode(SCRIPT_OPCODE_PRE_FUNCTION_CALL, 2, 0);
    }
}

/* Source: CoDUOMP.exe 0x0047bd80..0x0047bde9.
 * Name/signature: exact same-module Mac symbol EmitPostFunctionCall. */
void EmitPostFunctionCall(scr_ast_node_t *call, int32_t argumentCount, qboolean hasMethodContext)
{
    if (call->kind == SCR_AST_KIND_FUNCTION_CALL) {
        EmitPostScriptFunctionCall(call->payload.call.callee, argumentCount, hasMethodContext, call->payload.call.callSourcePos);
    } else if (call->kind == SCR_AST_KIND_METHOD_CALL) {
        EmitPostScriptThreadCall(call->payload.call.callee, argumentCount, hasMethodContext, call->payload.call.callSourcePos,
                                 call->payload.call.methodSourcePos);
    }
}

/* Source: CoDUOMP.exe 0x0047bdf0..0x0047be38, recovered from the executable
 * gap at 0x0047bde9. Name/signature: exact same-module Mac symbol GetBuiltin. */
uint16_t GetBuiltin(scr_ast_node_t *call)
{
    if (call->kind != SCR_AST_KIND_FUNCTION_CALL)
        return 0;

    scr_ast_node_t *callee = call->payload.call.callee;
    if (callee->kind != SCR_AST_KIND_SCRIPT_FUNCTION_NAME)
        return 0;

    scr_ast_node_t *functionNode = callee->payload.namedCall.functionNode;
    if (functionNode->kind != SCR_AST_KIND_FUNCTION_REF)
        return 0;

    const uint16_t name = SCR_AST_STRING_HANDLE(functionNode->payload.functionRef.nameHandle);
    const uint16_t functionHandle = FindVariable(script_currentFunctionRoot, name);
    return functionHandle == 0 ? name : 0;
}

/* Source: CoDUOMP.exe 0x0047be40..0x0047c0c4.
 * Name/signature: exact same-module Mac symbol EmitCall(sval_u, sval_u, bool).
 * The Windows compiler inlines GetBuiltin, the bytecode operand emitters, and
 * the pre/post script-call dispatchers recovered above. */
void EmitCall(scr_ast_node_t *call, scr_ast_list_t *arguments, qboolean isStatement)
{
    const uint16_t builtinName = GetBuiltin(call);
    if (builtinName != 0) {
        const char *builtinNameText = SL_ConvertToString(builtinName);
        int32_t developerCommand = 0;
        script_function_callback_t builtinFunction = Scr_GetFunction(&builtinNameText, &developerCommand);
        const uint32_t sourcePos = call->payload.call.callSourcePos;
        const uint32_t savedChecksum = script_codeChecksum;
        uint8_t *savedTempPos = TempMalloc(0);

        if (developerCommand != 0) {
            if (script_codegenMode != 0) {
                developerCommand = 0;
            } else {
                if (isStatement == qfalse) {
                    CompileError(sourcePos, "developer command can only be used as a statement "
                                            "if not in a /# ... #/ comment");
                }

                if (script_runtimeDeveloperScriptFlag == qfalse) {
                    script_codegenMode = SCRIPT_CODEGEN_MODE_RELEASE_STRINGS;
                } else {
                    if (script_codeNeedsDeferredCheck == qfalse) {
                        EmitOpcode(SCRIPT_OPCODE_DEVELOPER_COMMAND, 0, 0);
                        /* Windows EmitCall at 0x0047bf48..0x0047bf5c and
                         * EmitDeveloperStatementList at
                         * 0x0047f0a5..0x0047f0ae both snapshot the code-emission
                         * cursor here, matching Linux. The former client source
                         * bound this store to the developer-buffer base. */
                        script_codeRelocationStart = script_codeEmitCursor;
                    }
                    script_codegenMode = SCRIPT_CODEGEN_MODE_RECORD_STRING_FIXUPS;
                }
            }
        }

        const int32_t argumentCount = EmitExpressionList(arguments);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (argumentCount >= SCRIPT_CALL_ARGUMENT_LIMIT) {
            CompileRemoveRefToString(builtinName);
            CompileError(sourcePos, "parameter count exceeds 256");
        }

        if (builtinFunction == NULL) {
            CompileError(sourcePos, "unknown (builtin) function '%s'", builtinNameText);
            CompileRemoveRefToString(builtinName);
        } else {
            CompileRemoveRefToString(builtinName);
            EmitOpcode(SCRIPT_OPCODE_BUILTIN_FUNCTION, 1 - argumentCount, 1);
            EmitByte((uint8_t)argumentCount);
            EmitBuiltinFunction(builtinFunction);
            AddOpcodePos(sourcePos);
        }

        AddExpressionListOpcodePos(arguments);
        if (isStatement != qfalse) {
            EmitDecTop();
        }

        if (developerCommand != 0) {
            script_codegenMode = 0;
            if (script_runtimeDeveloperScriptFlag != qfalse) {
                script_codeNeedsDeferredCheck = qtrue;
                script_codeChecksum = savedChecksum;
            } else {
                TempMemorySetPos(savedTempPos);
                script_codeChecksum = savedChecksum;
            }
        }
        return;
    }

    EmitPreFunctionCall(call);
    const int32_t argumentCount = EmitExpressionList(arguments);
    EmitPostFunctionCall(call, argumentCount, qfalse);
    AddExpressionListOpcodePos(arguments);
    if (isStatement != qfalse) {
        EmitDecTop();
    }
}

/* Source: CoDUOMP.exe 0x0047c0d0..0x0047c383.
 * Name/signature: exact same-module Mac symbol EmitMethod.
 * The first AST value is the receiver expression; the second is the same
 * function-call descriptor consumed by GetBuiltin and EmitPostFunctionCall. */
void EmitMethod(scr_ast_node_t *object, scr_ast_node_t *call, scr_ast_list_t *arguments, uint32_t objectSourcePos, qboolean isStatement)
{
    const uint16_t builtinName = GetBuiltin(call);
    if (builtinName != 0) {
        const char *builtinNameText = SL_ConvertToString(builtinName);
        int32_t developerCommand = 0;
        script_method_callback_t builtinMethod = Scr_GetMethod(&builtinNameText, &developerCommand);
        const uint32_t sourcePos = call->payload.call.callSourcePos;
        const uint32_t savedChecksum = script_codeChecksum;
        uint8_t *savedTempPos = TempMalloc(0);

        if (developerCommand != 0) {
            if (script_codegenMode != 0) {
                developerCommand = 0;
            } else {
                if (isStatement == qfalse) {
                    CompileError(sourcePos, "developer command can only be used as a statement "
                                            "if not in a /# ... #/ comment");
                }

                if (script_runtimeDeveloperScriptFlag == qfalse) {
                    script_codegenMode = SCRIPT_CODEGEN_MODE_RELEASE_STRINGS;
                } else {
                    if (script_codeNeedsDeferredCheck == qfalse) {
                        EmitOpcode(SCRIPT_OPCODE_DEVELOPER_COMMAND, 0, 0);
                        /* Same relocation-start snapshot as the builtin
                         * function path above. */
                        script_codeRelocationStart = script_codeEmitCursor;
                    }
                    script_codegenMode = SCRIPT_CODEGEN_MODE_RECORD_STRING_FIXUPS;
                }
            }
        }

        const int32_t argumentCount = EmitExpressionList(arguments);
        EmitPrimitiveExpression(object);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (argumentCount >= SCRIPT_CALL_ARGUMENT_LIMIT) {
            CompileRemoveRefToString(builtinName);
            CompileError(sourcePos, "parameter count exceeds 256");
        }

        if (builtinMethod == NULL) {
            CompileError(sourcePos, "unknown (builtin) method '%s'", builtinNameText);
            CompileRemoveRefToString(builtinName);
        } else {
            CompileRemoveRefToString(builtinName);
            EmitOpcode(SCRIPT_OPCODE_BUILTIN_METHOD, -argumentCount, 1);
            EmitByte((uint8_t)argumentCount);
            EmitBuiltinMethod(builtinMethod);
            AddOpcodePos(sourcePos);
        }

        AddOpcodePos(objectSourcePos);
        AddExpressionListOpcodePos(arguments);
        if (isStatement != qfalse) {
            EmitDecTop();
        }

        if (developerCommand != 0) {
            script_codegenMode = 0;
            if (script_runtimeDeveloperScriptFlag != qfalse) {
                script_codeNeedsDeferredCheck = qtrue;
                script_codeChecksum = savedChecksum;
            } else {
                TempMemorySetPos(savedTempPos);
                script_codeChecksum = savedChecksum;
            }
        }
        return;
    }

    EmitPreFunctionCall(call);
    const int32_t argumentCount = EmitExpressionList(arguments);
    EmitPrimitiveExpression(object);
    EmitPostFunctionCall(call, argumentCount, qtrue);
    AddOpcodePos(objectSourcePos);
    AddExpressionListOpcodePos(arguments);
    if (isStatement != qfalse) {
        EmitDecTop();
    }
}

/* Source: CoDUOMP.exe 0x0047c390..0x0047c533.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047c390_0047c533.mcode.
 * Name/signature: exact same-module Mac symbol AdjustFunctionAddresses().
 * Each unresolved call operand is stored as the payload of a child beginning
 * at key 2; child 0 holds the count and child 1 holds the definition. */
void AdjustFunctionAddresses(void)
{
    for (uint16_t functionSlot = FindNextSibling(script_currentFunctionRoot); functionSlot != 0;
         functionSlot = FindNextSibling(functionSlot)) {
        const uint16_t functionObject = FindObject(functionSlot);
        const uint16_t definitionSlot = FindVariable(functionObject, SCRIPT_FUNCTION_DEFINITION_CHILD);
        VariableValue definitionValue = {.payload = 0, .type = SCRIPT_VAR_UNDEFINED};

        if (definitionSlot != 0) {
            GetVariableValue(definitionSlot, &definitionValue);
        }

        const uint16_t referenceCountSlot = FindVariable(functionObject, SCRIPT_FUNCTION_REFERENCE_COUNT_CHILD);
        if (referenceCountSlot != 0) {
            VariableValue referenceCountValue;
            GetVariableValue(referenceCountSlot, &referenceCountValue);
            const int32_t referenceCount = (int32_t)(uint32_t)referenceCountValue.payload;

            for (int32_t referenceIndex = 0; referenceIndex < referenceCount; ++referenceIndex) {
                const uint16_t referenceSlot =
                    FindVariable(functionObject, (uint32_t)referenceIndex + SCRIPT_FUNCTION_REFERENCE_FIRST_CHILD);
                VariableValue *referenceValue = GetVariableValueAddress(referenceSlot);

                if (definitionValue.type == SCRIPT_VAR_INT && GetVarType(referenceSlot) == SCRIPT_VAR_CODEPOS) {
                    CompileError2((uint8_t *)referenceValue->payload, "normal script cannot reference /# ... #/ comment");
                } else if (definitionSlot == 0) {
                    CompileError2((uint8_t *)referenceValue->payload, "unknown function");
                } else {
                    /* The original stores one i386 payload dword. Native
                     * bytecode emits host-width code-position operands, so
                     * patch the same logical operand without assuming its
                     * alignment. */
                    memcpy((void *)referenceValue->payload, &definitionValue.payload, sizeof(definitionValue.payload));
                }
            }
        }

        if (definitionSlot != 0) {
            SetVariableValue(functionSlot, &definitionValue);
        }
    }
}

/* Source: CoDUOMP.exe 0x0047c540..0x0047c599.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047c540_0047c599.mcode.
 * Name/signature: exact same-module Mac symbol
 * SpecifyThreadPosition(unsigned short, sval_u). */
void SpecifyThreadPosition(uint16_t functionObject, uint32_t sourcePos)
{
    const uint16_t definitionSlot = GetVariable(functionObject, SCRIPT_FUNCTION_DEFINITION_CHILD);
    if (GetVarType(definitionSlot) != SCRIPT_VAR_UNDEFINED) {
        CompileError(sourcePos, "function already defined");
        return;
    }

    const VariableValue definitionValue = {.payload = 0, .type = SCRIPT_VAR_CODEPOS};
    SetNewVariableValue(definitionSlot, &definitionValue);
}

/* Source: CoDUOMP.exe 0x0047c5a0..0x0047c606.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047c5a0_0047c606.mcode.
 * Name/signature: exact same-module Mac symbol
 * SpecifyDeveloperThreadPosition(unsigned short, sval_u). */
void SpecifyDeveloperThreadPosition(uint16_t functionObject, uint32_t sourcePos)
{
    if (script_runtimeDeveloperScriptFlag == qfalse) {
        return;
    }

    const uint16_t definitionSlot = GetVariable(functionObject, SCRIPT_FUNCTION_DEFINITION_CHILD);
    if (GetVarType(definitionSlot) != SCRIPT_VAR_UNDEFINED) {
        CompileError(sourcePos, "function already defined");
        return;
    }

    const VariableValue definitionValue = {.payload = 0, .type = SCRIPT_VAR_INT};
    SetNewVariableValue(definitionSlot, &definitionValue);
}

/* Source: CoDUOMP.exe 0x0047c610..0x0047c653, recovered from the executable
 * gap at 0x0047c606. Name/signature: exact same-module Mac symbol
 * SetThreadPosition(unsigned short, sval_u). */
void SetThreadPosition(uint16_t functionObject, uint32_t sourcePos)
{
    (void)sourcePos;

    const uint16_t definitionSlot = FindVariable(functionObject, SCRIPT_FUNCTION_DEFINITION_CHILD);
    GetVariableValueAddress(definitionSlot)->payload = (uintptr_t)TempMalloc(0);
}

/* Source: CoDUOMP.exe 0x0047c660..0x0047c6b3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047c660_0047c6b3.mcode.
 * Name/signature: exact same-module Mac symbol
 * SetDeveloperThreadPosition(unsigned short, sval_u). */
void SetDeveloperThreadPosition(uint16_t functionObject, uint32_t sourcePos)
{
    (void)sourcePos;

    const uint16_t definitionSlot = FindVariable(functionObject, SCRIPT_FUNCTION_DEFINITION_CHILD);
    GetVariableValueAddress(definitionSlot)->payload = (uintptr_t)(TempMalloc(0) + (script_codeRelocationEnd - script_codeRelocationStart));
}

/* Source: CoDUOMP.exe 0x0047a7c0..0x0047a846.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a7c0_0047a846.mcode. */
void EmitSize(scr_ast_node_t *node, uint32_t sourcePos)
{
    EmitPrimitiveExpression(node);
    EmitOpcode(SCRIPT_OPCODE_SIZE, 0, 0);
    AddOpcodePos(sourcePos);
}

/* Source: CoDUOMP.exe 0x0047c6c0..0x0047c6fa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047c6c0_0047c6fa.mcode.
 * The Windows optimizer carries emitDropTop in DL on the internal tail-call
 * path; it remains an ordinary qboolean source parameter here. */
void EmitCallExpression(scr_ast_node_t *node, qboolean emitDropTop)
{
    if (node->kind == SCR_AST_KIND_FUNCTION_CALL_VALUE) {
        EmitCall(node->payload.functionCallValue.callee, node->payload.functionCallValue.args, emitDropTop);
    } else if (node->kind == SCR_AST_KIND_METHOD_CALL_VALUE) {
        EmitMethod(node->payload.methodCallValue.objectNode, node->payload.methodCallValue.callee, node->payload.methodCallValue.args,
                   node->payload.methodCallValue.methodSourcePos, emitDropTop);
    }
}

/* Source: CoDUOMP.exe 0x0047c700..0x0047c73c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047c700_0047c73c.mcode. */
void EmitCallExpressionRef(scr_ast_node_t *node)
{
    if (node->kind == SCR_AST_KIND_FUNCTION_CALL_VALUE) {
        EmitCallRef(node->payload.functionCallValue.callee, node->payload.functionCallValue.args,
                    node->payload.functionCallValue.callSourcePos);
    } else if (node->kind == SCR_AST_KIND_METHOD_CALL_VALUE) {
        EmitMethodRef(node->payload.methodCallValue.objectNode, node->payload.methodCallValue.callee, node->payload.methodCallValue.args,
                      node->payload.methodCallValue.methodSourcePos);
    }
}

/* Source: CoDUOMP.exe 0x0047c740..0x0047c797.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047c740_0047c797.mcode. */
void EmitCallExpressionFieldObject(scr_ast_node_t *node)
{
    if (node->kind == SCR_AST_KIND_FUNCTION_CALL_VALUE) {
        EmitCall(node->payload.functionCallValue.callee, node->payload.functionCallValue.args, qfalse);
        EmitCastFieldObject(node->payload.functionCallValue.callSourcePos);
    } else if (node->kind == SCR_AST_KIND_METHOD_CALL_VALUE) {
        EmitMethod(node->payload.methodCallValue.objectNode, node->payload.methodCallValue.callee, node->payload.methodCallValue.args,
                   node->payload.methodCallValue.methodSourcePos, qfalse);
        EmitCastFieldObject(node->payload.methodCallValue.objectSourcePos);
    }
}

/* Source: CoDUOMP.exe 0x0047c800..0x0047c889.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047c800_0047c889.mcode. */
void EmitArrayExpressionListRef(scr_ast_list_t *list, uint32_t sourcePos)
{
    scr_ast_list_item_t *item = GetSingleParameter(list);

    if (item != NULL) {
        EmitArrayPrimitiveExpressionRef(item->entry->node, item->entry->sourcePos);
        return;
    }

    int32_t count = EmitExpressionList(list);
    if (count == 3) {
        EmitOpcode(SCRIPT_OPCODE_VECTOR_FROM_LIST, -2, 0);
        AddExpressionListOpcodePos(list);
        EmitOpcode(SCRIPT_OPCODE_STORE_TEMP, -1, 0);
        return;
    }

    CompileError(sourcePos, "not an array, string, or vector");
}

/* Source: CoDUOMP.exe 0x0047c890..0x0047c8f0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047c890_0047c8f0.mcode.
 * Both Windows and Linux route the singleton through
 * EmitPrimitiveExpressionFieldObject; Linux 0x0809f30a calls its wrapper at
 * 0x0809faa6. The former Linux recovery's direct inner-dispatch call was a
 * transcription error. */
void EmitExpressionListFieldObject(scr_ast_list_t *list, uint32_t sourcePos)
{
    scr_ast_list_item_t *item = GetSingleParameter(list);

    if (item != NULL) {
        EmitPrimitiveExpressionFieldObject(item->entry->node, item->entry->sourcePos);
        return;
    }

    CompileError(sourcePos, "not an object");
    EmitExpressionList(list);
}

/* Source: CoDUOMP.exe 0x0047c7a0..0x0047c7fb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047c7a0_0047c7fb.mcode. */
void EmitPrimitiveExpressionList(scr_ast_list_t *list, uint32_t sourcePos)
{
    int32_t count = EmitExpressionList(list);

    if (count == 1) {
        return;
    }

    if (count == 3) {
        EmitOpcode(SCRIPT_OPCODE_VECTOR_FROM_LIST, -2, 0);
        AddExpressionListOpcodePos(list);
        return;
    }

    CompileError(sourcePos, "expression list must have 1 or 3 parameters");
}

/* Source: CoDUOMP.exe 0x0047c8f0..0x0047ca00.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047c8f0_0047ca00.mcode. */
void EmitPrimitiveExpression(scr_ast_node_t *node)
{
    switch (node->kind) {
    case SCR_AST_KIND_INTEGER_LITERAL:
        EmitGetInteger((int32_t)node->payload.literal.value);
        break;
    case SCR_AST_KIND_FLOAT_LITERAL:
        coduomp_script_emit_get_float_bits(node->payload.literal.value);
        break;
    case SCR_AST_KIND_NEGATED_INTEGER_LITERAL:
        EmitGetInteger(-(int32_t)node->payload.literal.value);
        break;
    case SCR_AST_KIND_NEGATED_FLOAT_LITERAL:
        coduomp_script_emit_get_float_bits(node->payload.literal.value ^ SCRIPT_FLOAT_SIGN_BIT);
        break;
    case SCR_AST_KIND_STRING:
        EmitGetString(SCR_AST_STRING_HANDLE(node->payload.sourceString.stringHandle));
        break;
    case SCR_AST_KIND_ISTRING:
        EmitGetIString(SCR_AST_STRING_HANDLE(node->payload.sourceString.stringHandle));
        break;
    case SCR_AST_KIND_REFERENCE_EXPRESSION:
        EmitVariableExpression(node->payload.child.node);
        break;
    case SCR_AST_KIND_SCRIPT_FUNCTION_NAME:
        EmitGetFunction(node->payload.sourceChild.node, node->payload.sourceChild.sourcePos);
        break;
    case SCR_AST_KIND_CALL_VALUE:
        EmitCallExpression(node->payload.child.node, qfalse);
        break;
    case SCR_AST_KIND_UNDEFINED:
        EmitGetUndefined();
        break;
    case SCR_AST_KIND_SELF:
        EmitSelf();
        break;
    case SCR_AST_KIND_LEVEL:
        EmitLevel();
        break;
    case SCR_AST_KIND_ANIM:
        EmitAnim();
        break;
    case SCR_AST_KIND_GAME:
        EmitGame();
        break;
    case SCR_AST_KIND_EXPRESSION_LIST:
        EmitPrimitiveExpressionList(node->payload.expressionList.list, node->payload.expressionList.sourcePos);
        break;
    case SCR_AST_KIND_SIZE:
        EmitSize(node->payload.sourceChild.node, node->payload.sourceChild.sourcePos);
        break;
    case SCR_AST_KIND_EMPTY_ARRAY:
        EmitEmptyArray();
        break;
    case SCR_AST_KIND_ANIMATION:
        EmitAnimation(SCR_AST_STRING_HANDLE(node->payload.sourceString.stringHandle), node->payload.sourceString.sourcePos);
        break;
    case SCR_AST_KIND_FALSE:
        EmitFalse();
        break;
    case SCR_AST_KIND_TRUE:
        EmitTrue();
        break;
    case SCR_AST_KIND_ANIMTREE:
        EmitAnimTree(node->payload.sourceOnlyStatement.sourcePos);
        break;
    default:
        break;
    }
}

/* Source: CoDUOMP.exe 0x0047caa0..0x0047cb90.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047caa0_0047cb90.mcode. */
void EmitBoolOrExpression(scr_ast_node_t *leftNode, uint32_t leftSourcePos, scr_ast_node_t *rightNode, uint32_t rightSourcePos)
{
    EmitExpression(leftNode);
    EmitOpcode(SCRIPT_OPCODE_BOOL_OR, -1, 0);
    AddOpcodePos(leftSourcePos);
    EmitInteger(0);

    char *patch = (char *)script_codeEmitCursor;
    EmitExpression(rightNode);
    EmitCastBool(rightSourcePos);

    char *end = (char *)TempMalloc(0);
    const int32_t offset = (int32_t)(end - patch) - (int32_t)sizeof(offset);
    memcpy(patch, &offset, sizeof(offset));
}

/* Source: CoDUOMP.exe 0x0047cb90..0x0047cc80.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047cb90_0047cc80.mcode. */
void EmitBoolAndExpression(scr_ast_node_t *leftNode, uint32_t leftSourcePos, scr_ast_node_t *rightNode, uint32_t rightSourcePos)
{
    EmitExpression(leftNode);
    EmitOpcode(SCRIPT_OPCODE_BOOL_AND, -1, 0);
    AddOpcodePos(leftSourcePos);
    EmitInteger(0);

    char *patch = (char *)script_codeEmitCursor;
    EmitExpression(rightNode);
    EmitCastBool(rightSourcePos);

    char *end = (char *)TempMalloc(0);
    const int32_t offset = (int32_t)(end - patch) - (int32_t)sizeof(offset);
    memcpy(patch, &offset, sizeof(offset));
}

/* Source: CoDUOMP.exe 0x0047cc80..0x0047cd1d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047cc80_0047cd1d.mcode. */
void EmitBinaryOperatorExpression(scr_ast_node_t *leftNode, scr_ast_node_t *rightNode, uint8_t opcode, uint32_t sourcePos)
{
    EmitExpression(leftNode);
    EmitExpression(rightNode);
    EmitOpcode(opcode, -1, 0);
    AddOpcodePos(sourcePos);
}

/* Source: CoDUOMP.exe 0x0047cd20..0x0047cdd9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047cd20_0047cdd9.mcode. */
void EmitBinaryEqualsOperatorExpression(scr_ast_node_t *refNode, scr_ast_node_t *valueNode, uint8_t opcode, uint32_t sourcePos)
{
    script_codeOwnsStrings = qtrue;
    EmitVariableExpression(refNode);
    script_codeOwnsStrings = qfalse;
    EmitExpression(valueNode);
    EmitOpcode(opcode, -1, 0);
    AddOpcodePos(sourcePos);
    EmitVariableExpressionRef(refNode);
    EmitSetVariableField(sourcePos);
}

/* Source: CoDUOMP.exe 0x0047cde0..0x0047ceee.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047cde0_0047ceee.mcode. */
void EmitExpression(scr_ast_node_t *node)
{
    switch (node->kind) {
    case SCR_AST_KIND_PRIMITIVE_EXPRESSION:
        EmitPrimitiveExpression(node->payload.child.node);
        break;
    case SCR_AST_KIND_BOOL_OR:
        EmitBoolOrExpression(node->payload.shortCircuit.left, node->payload.shortCircuit.leftSourcePos, node->payload.shortCircuit.right,
                             node->payload.shortCircuit.rightSourcePos);
        break;
    case SCR_AST_KIND_BOOL_AND:
        EmitBoolAndExpression(node->payload.shortCircuit.left, node->payload.shortCircuit.leftSourcePos, node->payload.shortCircuit.right,
                              node->payload.shortCircuit.rightSourcePos);
        break;
    case SCR_AST_KIND_BINARY_OPERATOR:
        EmitBinaryOperatorExpression(node->payload.binaryOperator.left, node->payload.binaryOperator.right,
                                     (uint8_t)node->payload.binaryOperator.opcode, node->payload.binaryOperator.sourcePos);
        break;
    case SCR_AST_KIND_CAST_BOOL:
        EmitExpression(node->payload.sourceChild.node);
        EmitCastBool(node->payload.sourceChild.sourcePos);
        break;
    case SCR_AST_KIND_CAST_INT:
        EmitExpression(node->payload.sourceChild.node);
        EmitCastInt(node->payload.sourceChild.sourcePos);
        break;
    case SCR_AST_KIND_CAST_FLOAT:
        EmitExpression(node->payload.sourceChild.node);
        EmitCastFloat(node->payload.sourceChild.sourcePos);
        break;
    case SCR_AST_KIND_CAST_STRING:
        EmitExpression(node->payload.sourceChild.node);
        EmitCastString(node->payload.sourceChild.sourcePos);
        break;
    case SCR_AST_KIND_BOOL_NOT:
        EmitExpression(node->payload.sourceChild.node);
        EmitBoolNot(node->payload.sourceChild.sourcePos);
        break;
    case SCR_AST_KIND_BOOL_COMPLEMENT:
        EmitExpression(node->payload.sourceChild.node);
        EmitBoolComplement(node->payload.sourceChild.sourcePos);
        break;
    default:
        break;
    }
}

/* Source: CoDUOMP.exe 0x0047cf50..0x0047cfae.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047cf50_0047cfae.mcode. */
void EmitVariableExpressionRef(scr_ast_node_t *node)
{
    if (node->kind == SCR_AST_KIND_OBJECT_INDEX_OBJECT_REF) {
        EmitArrayVariableRef(node->payload.objectIndexObjectRef.objectNode, node->payload.objectIndexObjectRef.indexNode,
                             node->payload.objectIndexObjectRef.objectSourcePos, node->payload.objectIndexObjectRef.indexSourcePos);
        return;
    }

    if (node->kind == SCR_AST_KIND_STRING_REF) {
        EmitLocalVariableRef(SCR_AST_STRING_HANDLE(node->payload.stringRef.stringHandle));
        return;
    }

    if (node->kind == SCR_AST_KIND_OBJECT_STRING_REF) {
        EmitFieldVariableRef(node->payload.objectStringRef.objectNode, SCR_AST_STRING_HANDLE(node->payload.objectStringRef.stringHandle),
                             node->payload.objectStringRef.sourcePos);
    }
}

/* Source: CoDUOMP.exe 0x0047cfb0..0x0047d01c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047cfb0_0047d01c.mcode. */
void EmitArrayExpressionRef(scr_ast_node_t *node, uint32_t sourcePos)
{
    if (node->kind == SCR_AST_KIND_CALL_VALUE) {
        EmitCallExpressionRef(node->payload.child.node);
        return;
    }

    if (node->kind == SCR_AST_KIND_REFERENCE_EXPRESSION) {
        EmitVariableExpressionRef(node->payload.child.node);
        return;
    }

    if (node->kind == SCR_AST_KIND_ANIM) {
        EmitAnimObject();
        return;
    }

    if (node->kind == SCR_AST_KIND_EXPRESSION_LIST) {
        EmitArrayExpressionListRef(node->payload.expressionList.list, sourcePos);
        return;
    }

    CompileError(sourcePos, "not an array, string, or vector");
    EmitPrimitiveExpression(node);
}

/* Source: CoDUOMP.exe 0x0047d090..0x0047d111.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d090_0047d111.mcode. */
void EmitExpressionFieldObject(scr_ast_node_t *node, uint32_t sourcePos)
{
    switch (node->kind) {
    case SCR_AST_KIND_REFERENCE_EXPRESSION:
        EmitVariableExpression(node->payload.child.node);
        EmitCastFieldObject(node->payload.sourceChild.sourcePos);
        break;
    case SCR_AST_KIND_CALL_VALUE:
        EmitCallExpressionFieldObject(node->payload.child.node);
        break;
    case SCR_AST_KIND_SELF:
        EmitSelfObject();
        break;
    case SCR_AST_KIND_LEVEL:
        EmitLevelObject();
        break;
    case SCR_AST_KIND_GAME:
        EmitGameRef();
        break;
    case SCR_AST_KIND_EXPRESSION_LIST:
        EmitExpressionListFieldObject(node->payload.expressionList.list, sourcePos);
        break;
    default:
        CompileError(sourcePos, "not an object");
        EmitPrimitiveExpression(node);
        break;
    }
}

/* Source: CoDUOMP.exe 0x0047d050..0x0047d086.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d050_0047d086.mcode. */
void EmitArrayPrimitiveExpressionRef(scr_ast_node_t *node, uint32_t sourcePos)
{
    if (node->kind == SCR_AST_KIND_PRIMITIVE_EXPRESSION) {
        EmitArrayExpressionRef(node->payload.sourceChild.node, node->payload.sourceChild.sourcePos);
        return;
    }

    CompileError(sourcePos, "not an array, string, or vector");
    EmitExpression(node);
}

/* Source: CoDUOMP.exe 0x0047d150..0x0047d186.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d150_0047d186.mcode. */
void EmitPrimitiveExpressionFieldObject(scr_ast_node_t *node, uint32_t sourcePos)
{
    if (node->kind == SCR_AST_KIND_PRIMITIVE_EXPRESSION) {
        EmitExpressionFieldObject(node->payload.sourceChild.node, node->payload.sourceChild.sourcePos);
        return;
    }

    CompileError(sourcePos, "not an object");
    EmitExpression(node);
}

/* Source: CoDUOMP.exe 0x0047d190..0x0047d1c9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d190_0047d1c9.mcode. */
void ConnectBreakStatements(void)
{
    char *current = (char *)TempMalloc(0);

    for (script_code_offset_patch_t *patch = script_breakPatchList; patch != NULL; patch = patch->next) {
        const int32_t offset = (int32_t)(current - patch->patch);
        memcpy(patch->patch, &offset, sizeof(offset));
    }
}

/* Source: CoDUOMP.exe 0x0047d1d0..0x0047d212.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d1d0_0047d212.mcode. */
void ConnectContinueStatements(void)
{
    EmitNOP();
    char *current = (char *)TempMalloc(0);

    for (script_code_offset_patch_t *patch = script_continuePatchList; patch != NULL; patch = patch->next) {
        const int32_t offset = (int32_t)(current - patch->patch);
        memcpy(patch->patch, &offset, sizeof(offset));
    }
}

/* Source: CoDUOMP.exe 0x0047d250..0x0047d2b2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d250_0047d2b2.mcode. */
void EmitClearVariableExpression(scr_ast_node_t *node)
{
    switch (node->kind) {
    case SCR_AST_KIND_STRING_REF:
        EmitClearLocalVariable(SCR_AST_STRING_HANDLE(node->payload.stringRef.stringHandle));
        break;
    case SCR_AST_KIND_OBJECT_INDEX_OBJECT_REF:
        EmitClearArrayVariable(node->payload.objectIndexObjectRef.objectNode, node->payload.objectIndexObjectRef.indexNode,
                               node->payload.objectIndexObjectRef.objectSourcePos, node->payload.objectIndexObjectRef.indexSourcePos);
        break;
    case SCR_AST_KIND_OBJECT_STRING_REF:
        EmitClearFieldVariable(node->payload.objectStringRef.objectNode, SCR_AST_STRING_HANDLE(node->payload.objectStringRef.stringHandle),
                               node->payload.objectStringRef.sourcePos, node->payload.objectStringRef.opcodeSourcePos);
        break;
    default:
        break;
    }
}

/* Source: CoDUOMP.exe 0x0047d220..0x0047d22f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d220_0047d22f.mcode. */
qboolean IsUndefinedPrimitiveExpression(scr_ast_node_t *node)
{
    return node->kind == SCR_AST_KIND_UNDEFINED ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x0047d230..0x0047d246.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d230_0047d246.mcode. */
qboolean IsUndefinedExpression(scr_ast_node_t *node)
{
    if (node->kind == SCR_AST_KIND_PRIMITIVE_EXPRESSION) {
        return IsUndefinedPrimitiveExpression(node->payload.child.node);
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x0047d2c0..0x0047d302.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047d2c0_0047d302.mcode. */
void EmitAssignmentStatement(scr_ast_node_t *refNode, scr_ast_node_t *valueNode, uint32_t sourcePos)
{
    if (IsUndefinedExpression(valueNode) == qfalse) {
        EmitExpression(valueNode);
        EmitVariableExpressionRef(refNode);
        EmitSetVariableField(sourcePos);
        return;
    }

    EmitClearVariableExpression(refNode);
}
