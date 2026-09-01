#include <stdint.h>
#include <math.h>
#include <string.h>

#include "compat/coduo_int32_bits.h"
#include "script_error_exception.hpp"
#include "script_error_reporting.h"
#include "script_notify.h"
#include "script_runtime_host.h"
#include "script_string.h"
#include "script_usage.h"
#include "script_value.h"
#include "script_variable.h"
#include "script_vm.h"

#define SCRIPT_INTERPRETER_FLOAT_EQUAL_EPSILON 1.0e-6f

enum script_interpreter_opcode_e {
    SCRIPT_OP_END = 0x00,
    SCRIPT_OP_RETURN = 0x01,
    SCRIPT_OP_GET_UNDEFINED = 0x02,
    SCRIPT_OP_GET_INTEGER = 0x03,
    SCRIPT_OP_GET_FLOAT = 0x04,
    SCRIPT_OP_GET_STRING = 0x05,
    SCRIPT_OP_GET_ISTRING = 0x06,
    SCRIPT_OP_GET_SELF_OBJECT = 0x07,
    SCRIPT_OP_GET_LEVEL_OBJECT = 0x08,
    SCRIPT_OP_GET_GAME_OBJECT = 0x09,
    SCRIPT_OP_GET_SELF = 0x0a,
    SCRIPT_OP_GET_LEVEL = 0x0b,
    SCRIPT_OP_GET_ANIM = 0x0c,
    SCRIPT_OP_GET_GAME = 0x0d,
    SCRIPT_OP_GET_ANIMATION = 0x0e,
    SCRIPT_OP_SET_ANIM_OBJECT = 0x0f,
    SCRIPT_OP_GET_FUNCTION = 0x10,
    SCRIPT_OP_GET_LOCAL = 0x11,
    SCRIPT_OP_SET_LOCAL_REF = 0x12,
    SCRIPT_OP_CLEAR_LOCAL = 0x13,
    SCRIPT_OP_EVAL_INDEX = 0x14,
    SCRIPT_OP_SET_INDEXED_REF = 0x15,
    SCRIPT_OP_CLEAR_INDEXED = 0x16,
    SCRIPT_OP_NEW_ARRAY = 0x17,
    SCRIPT_OP_GET_FIELD = 0x18,
    SCRIPT_OP_SET_FIELD_REF = 0x19,
    SCRIPT_OP_CLEAR_FIELD = 0x1a,
    SCRIPT_OP_STORE_TEMP = 0x1b,
    SCRIPT_OP_SET_LOCAL = 0x1c,
    SCRIPT_OP_SET_LOCAL_AND_CLEAR = 0x1d,
    SCRIPT_OP_CLEAR_PARAMS_UNTIL_MARKER = 0x1e,
    SCRIPT_OP_FUNCTION_PARAMETERS_DONE = 0x1f,
    SCRIPT_OP_STORE_REF = 0x20,
    SCRIPT_OP_BUILTIN_FUNCTION = 0x21,
    SCRIPT_OP_BUILTIN_METHOD = 0x22,
    SCRIPT_OP_WAIT = 0x23,
    SCRIPT_OP_THREAD_MARKER = 0x24,
    SCRIPT_OP_CALL_FUNCTION = 0x25,
    SCRIPT_OP_CALL_POINTER = 0x26,
    SCRIPT_OP_CALL_FUNCTION_METHOD_CONTEXT = 0x27,
    SCRIPT_OP_CALL_POINTER_METHOD_CONTEXT = 0x28,
    SCRIPT_OP_THREAD_FUNCTION = 0x29,
    SCRIPT_OP_THREAD_POINTER = 0x2a,
    SCRIPT_OP_THREAD_FUNCTION_METHOD_CONTEXT = 0x2b,
    SCRIPT_OP_THREAD_POINTER_METHOD_CONTEXT = 0x2c,
    SCRIPT_OP_DROP_TOP = 0x2d,
    SCRIPT_OP_CAST_OBJECT = 0x2e,
    SCRIPT_OP_CAST_BOOL = 0x2f,
    SCRIPT_OP_CAST_INT = 0x30,
    SCRIPT_OP_CAST_FLOAT = 0x31,
    SCRIPT_OP_CAST_STRING = 0x32,
    SCRIPT_OP_BOOL_NOT = 0x33,
    SCRIPT_OP_BIT_NOT = 0x34,
    SCRIPT_OP_JUMP_ON_FALSE = 0x35,
    SCRIPT_OP_JUMP_ON_TRUE_BACK = 0x36,
    SCRIPT_OP_SHORT_CIRCUIT_AND = 0x37,
    SCRIPT_OP_SHORT_CIRCUIT_OR = 0x38,
    SCRIPT_OP_JUMP = 0x39,
    SCRIPT_OP_JUMP_BACK = 0x3a,
    SCRIPT_OP_INC = 0x3b,
    SCRIPT_OP_DEC = 0x3c,
    SCRIPT_OP_BIT_OR = 0x3d,
    SCRIPT_OP_BIT_XOR = 0x3e,
    SCRIPT_OP_BIT_AND = 0x3f,
    SCRIPT_OP_EQUAL = 0x40,
    SCRIPT_OP_NOT_EQUAL = 0x41,
    SCRIPT_OP_LESS = 0x42,
    SCRIPT_OP_GREATER = 0x43,
    SCRIPT_OP_LESS_EQUAL = 0x44,
    SCRIPT_OP_GREATER_EQUAL = 0x45,
    SCRIPT_OP_SHIFT_LEFT = 0x46,
    SCRIPT_OP_SHIFT_RIGHT = 0x47,
    SCRIPT_OP_PLUS = 0x48,
    SCRIPT_OP_MINUS = 0x49,
    SCRIPT_OP_MULTIPLY = 0x4a,
    SCRIPT_OP_DIVIDE = 0x4b,
    SCRIPT_OP_MODULO = 0x4c,
    SCRIPT_OP_SIZE = 0x4d,
    SCRIPT_OP_WAITTILLMATCH = 0x4e,
    SCRIPT_OP_WAITTILL = 0x4f,
    SCRIPT_OP_NOTIFY = 0x50,
    SCRIPT_OP_ENDON = 0x51,
    SCRIPT_OP_PUSH_CODEPOS = 0x52,
    SCRIPT_OP_SWITCH_JUMP = 0x53,
    SCRIPT_OP_SWITCH_TABLE = 0x54,
    SCRIPT_OP_VECTOR = 0x55,
    SCRIPT_OP_NOP = 0x56,
    SCRIPT_OP_DEVELOPER_COMMAND = 0x57,
    SCRIPT_OP_DEFERRED_DEVELOPER_CHECK = 0x58
};

enum {
    SCRIPT_INTERPRETER_MAX_CALL_DEPTH = 32,
    SCRIPT_INTERPRETER_TIME_MASK = 0x00ffffff,
    SCRIPT_INTERPRETER_MAX_WAIT_SECONDS = 16777,
    SCRIPT_INTERPRETER_WAIT_SCALE = 1000,
    SCRIPT_INTERPRETER_SHIFT_MASK = 0x1f,
    SCRIPT_INTERPRETER_STACK_SENTINEL_VALUE = 0,
    SCRIPT_INTERPRETER_ENDON_NOTIFY_NAME = 0x20000,
    SCRIPT_INTERPRETER_NOTIFY_STACK_NAME = 0x20001,
    SCRIPT_INTERPRETER_CODEPOS_OPERAND_BYTES =
        sizeof(uintptr_t),
    SCRIPT_INTERPRETER_FUNCTION_CALL_OPERAND_BYTES =
        sizeof(uintptr_t) + sizeof(uint32_t),
    SCRIPT_INTERPRETER_POINTER_CALL_OPERAND_BYTES = sizeof(uint32_t)
};

/* NOT_FROM_ORIGINAL_SOURCE: preserves the 32-bit script integer payload in
 * native-width value storage. */
static uintptr_t ScriptInterpreter_U32Payload(uint32_t value)
{
    return (uintptr_t)value;
}

/* NOT_FROM_ORIGINAL_SOURCE: bytecode reader for packed script VM operands. */
static uint16_t ScriptInterpreter_ReadU16(const uint8_t *codePos)
{
    uint16_t value;
    memcpy(&value, codePos, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: bytecode reader for packed script VM operands. */
static int16_t ScriptInterpreter_ReadI16(const uint8_t *codePos)
{
    int16_t value;
    memcpy(&value, codePos, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: bytecode reader for packed script VM operands. */
static int32_t ScriptInterpreter_ReadI32(const uint8_t *codePos)
{
    int32_t value;
    memcpy(&value, codePos, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: bytecode reader for packed script VM operands. */
static uint32_t ScriptInterpreter_ReadU32(const uint8_t *codePos)
{
    uint32_t value;
    memcpy(&value, codePos, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: bytecode reader for host-width script payloads. */
static uintptr_t
ScriptInterpreter_ReadValuePayload(const uint8_t *codePos)
{
    uintptr_t value;
    memcpy(&value, codePos, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: converts script code payloads at VM ABI edges. */
static uintptr_t
ScriptInterpreter_CodeposPayload(const uint8_t *codePos)
{
    return (uintptr_t)codePos;
}

/* NOT_FROM_ORIGINAL_SOURCE: converts script code payloads at VM ABI edges. */
static uint8_t *
ScriptInterpreter_CodeposFromPayload(uintptr_t payload)
{
    return (uint8_t *)payload;
}

/* NOT_FROM_ORIGINAL_SOURCE: bytecode reader for packed callback pointers. */
static script_function_callback_t
ScriptInterpreter_ReadBuiltinFunction(const uint8_t *codePos)
{
    script_function_callback_t value;
    memcpy(&value, codePos, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: bytecode reader for packed callback pointers. */
static script_method_callback_t
ScriptInterpreter_ReadBuiltinMethod(const uint8_t *codePos)
{
    script_method_callback_t value;
    memcpy(&value, codePos, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: preserves script value float payload encoding. */
static uintptr_t ScriptInterpreter_FloatPayload(float value)
{
    uintptr_t payload = 0;

    memcpy(&payload, &value, sizeof(value));
    return payload;
}

/* NOT_FROM_ORIGINAL_SOURCE: preserves script value float payload encoding. */
static float ScriptInterpreter_PayloadFloat(uintptr_t payload)
{
    float value;
    memcpy(&value, &payload, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: the DLL stores the subtraction back to float,
 * then clears its IEEE-754 sign bit before comparing it with the epsilon. */
static float ScriptInterpreter_AbsFloat(float value)
{
    return fabsf(value);
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for script VM stack pushes. */
static VariableValue *
ScriptInterpreter_Push(VariableValue *stackTop,
                       script_variable_type_t type,
                       uintptr_t payload)
{
    ++stackTop;
    stackTop->payload = payload;
    stackTop->type = type;
    return stackTop;
}

/* NOT_FROM_ORIGINAL_SOURCE: pushes a VM slot while preserving the stale
 * payload dword, as the DLL does for undefined values and bare markers. */
static VariableValue *
ScriptInterpreter_PushTypeOnly(VariableValue *stackTop,
                               script_variable_type_t type)
{
    ++stackTop;
    stackTop->type = type;
    return stackTop;
}

/* NOT_FROM_ORIGINAL_SOURCE: typed access to script vector payload storage. */
static float *ScriptInterpreter_Vector(uintptr_t payload)
{
    return (float *)payload;
}

/* NOT_FROM_ORIGINAL_SOURCE: shared call-depth guard for bytecode dispatch. */
static void ScriptInterpreter_CheckCallDepth(qboolean setErrorParameterIndex)
{
    if (script_callStackDepth >= SCRIPT_INTERPRETER_MAX_CALL_DEPTH) {
        /* The original sets script_errorParameterIndex = 1 on this overflow for
         * every call/thread opcode EXCEPT the direct named-function forms
         * CALL_FUNCTION (0x25, no write at 0x00489fc3) and
         * CALL_FUNCTION_METHOD_CONTEXT (0x27, no write at 0x0048a262); all six
         * other forms write 1 (e.g. 0x26 at 0x0048a0e4). */
        if (setErrorParameterIndex != qfalse) {
            script_errorParameterIndex = 1;
        }
        Scr_Error("script stack overflow (too many embedded function calls)");
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: shared live-object validator for VM dispatch. */
static uint16_t
ScriptInterpreter_RequireObject(VariableValue *value,
                                int32_t parameterIndex)
{
    if (value->type != SCRIPT_VAR_OBJECT) {
        script_errorParameterIndex = parameterIndex;
        Scr_Error(
            va("%s is not an object", script_variableTypeNames[value->type]));
    }

    uint16_t object =
        (uint16_t)value->payload;
    if (IsFieldObject(object) == qfalse) {
        script_errorParameterIndex = parameterIndex;
        script_variable_type_t type = GetVarType(object);
        Scr_Error(
            va("%s is not an object", script_variableTypeNames[type]));
    }

    return object;
}

/* NOT_FROM_ORIGINAL_SOURCE: installs a nested script-call frame. */
static void ScriptInterpreter_SaveCallCodepos(uint8_t *codePos)
{
    script_callStackCodepos[script_callStackDepth] = codePos;
    script_callStackDepth++;
}

/* NOT_FROM_ORIGINAL_SOURCE: prepares a child script function call frame. */
static void ScriptInterpreter_EnterFunctionCall(
    VariableValue **stackTop, uint8_t **codePos,
    uint16_t *thread,
    uint16_t parent, int32_t argumentCount,
    uint8_t *targetCodePos, uint8_t *returnCodePos,
    qboolean addParentRef, qboolean setErrorParameterIndex)
{
    ScriptInterpreter_CheckCallDepth(setErrorParameterIndex);
    ScriptInterpreter_SaveCallCodepos(*codePos);

    if (addParentRef != qfalse) {
        AddRefToObject(parent);
    }
    *thread = AllocThread(parent);

    VariableValue *marker = *stackTop - argumentCount;
    marker->payload = ScriptInterpreter_CodeposPayload(returnCodePos);
    marker->type = SCRIPT_VAR_CODEPOS;
    *codePos = targetCodePos;
}

/* NOT_FROM_ORIGINAL_SOURCE: runs a bytecode thread call and releases it. */
static VariableValue *ScriptInterpreter_RunThreadCall(
    VariableValue *callTop, uint8_t *targetCodePos,
    uint16_t threadParent,
    VariableValue *marker)
{
    script_variable_type_t savedType =
        (script_variable_type_t)marker->type;

    marker->type = SCRIPT_VAR_CODEPOS;
    uint16_t thread =
        AllocThread(threadParent);
    uint16_t result =
        VM_Execute(callTop, targetCodePos, thread, thread, marker);
    RemoveRefToObject(result);
    marker->type = savedType;

    return marker + 1;
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for script VM stack cleanups. */
static void ScriptInterpreter_ReleaseActiveParams(void)
{
    while (script_parameterCount != 0) {
        RemoveRefToValue(script_valueStackTop);
        --script_valueStackTop;
        --script_parameterCount;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: common return-through-call-frame cleanup. */
static qboolean ScriptInterpreter_ReturnFromFrame(
    VariableValue **stackTop, uint8_t **codePos,
    uint16_t *thread,
    VariableValue *stackBase, VariableValue *returnValue,
    qboolean copyPayloadUnconditionally)
{
    VariableValue *top = *stackTop;

    while (top->type != SCRIPT_VAR_CODEPOS) {
        RemoveRefToValue(top);
        --top;
    }

    /* END (0x00) writes the result slot type-only when the return is
     * undefined (0x00489005 / 0x00489070). RETURN (0x01) copies payload+type
     * unconditionally (0x004891be / 0x0048922e) with no type test, so a
     * `return <undefinedValue>;` still writes the payload dword. */
    --script_callStackDepth;
    if (top == stackBase) {
        if (copyPayloadUnconditionally == qfalse &&
            returnValue->type == SCRIPT_VAR_UNDEFINED) {
            stackBase[1].type = SCRIPT_VAR_UNDEFINED;
        } else {
            stackBase[1] = *returnValue;
        }
        *stackTop = top;
        return qtrue;
    }

    RemoveRefToObject(*thread);
    *codePos = (uint8_t *)top->payload;
    --top;
    *thread = (uint16_t)top->payload;
    if (copyPayloadUnconditionally == qfalse &&
        returnValue->type == SCRIPT_VAR_UNDEFINED) {
        top->type = SCRIPT_VAR_UNDEFINED;
    } else {
        *top = *returnValue;
    }

    uint16_t parent =
        GetSelf(*thread);
    if (IsFieldObject(parent) == qfalse) {
        while (true) {
            KillThread(*thread);
            while (top->type != SCRIPT_VAR_CODEPOS) {
                RemoveRefToValue(top);
                --top;
            }
            --script_callStackDepth;
            if (top == stackBase) {
                stackBase[1].type = SCRIPT_VAR_UNDEFINED;
                *stackTop = top;
                return qtrue;
            }

            RemoveRefToObject(*thread);
            --top;
            *thread = (uint16_t)top->payload;
            top->type = SCRIPT_VAR_UNDEFINED;
        }
    }

    *stackTop = top;
    return qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: restores after a script error throw. */
static void ScriptInterpreter_CleanupAfterError(
    uint8_t opcode, VariableValue **stackTop, uint8_t **codePos,
    uint16_t *fieldRef, int32_t switchCaseCount)
{
    switch ((script_interpreter_opcode_e)opcode) {
    case SCRIPT_OP_EVAL_INDEX:
        RemoveRefToValue(*stackTop);
        RemoveRefToValue(*stackTop + 1);
        (*stackTop)->type = SCRIPT_VAR_UNDEFINED;
        break;

    case SCRIPT_OP_SET_INDEXED_REF:
        ClearVariableValue(script_tempValueHandle);
        *fieldRef = script_tempValueHandle;
        /* Fall through. */

    case SCRIPT_OP_CLEAR_INDEXED:
        if (script_errorParameterIndex < 0) {
            script_errorParameterIndex = 1;
        }
        RemoveRefToValue(*stackTop);
        *stackTop -= 1;
        break;

    case SCRIPT_OP_GET_FIELD:
    case SCRIPT_OP_CLEAR_FIELD:
        script_errorParameterIndex = 0;
        *codePos += sizeof(uint16_t);
        break;

    case SCRIPT_OP_SET_LOCAL:
    case SCRIPT_OP_SET_LOCAL_AND_CLEAR:
    case SCRIPT_OP_STORE_REF:
        ScriptInterpreter_ReleaseActiveParams();
        script_errorParameterIndex = 0;
        *stackTop -= 1;
        if (opcode == SCRIPT_OP_SET_LOCAL ||
            opcode == SCRIPT_OP_SET_LOCAL_AND_CLEAR) {
            *codePos += sizeof(uint16_t);
        }
        break;

    case SCRIPT_OP_FUNCTION_PARAMETERS_DONE:
        while ((*stackTop)->type != SCRIPT_VAR_CODEPOS) {
            RemoveRefToValue(*stackTop);
            *stackTop -= 1;
        }
        break;

    case SCRIPT_OP_BUILTIN_FUNCTION:
        if (script_errorParameterIndex > 0) {
            script_errorParameterIndex =
                (script_parameterCount - script_errorParameterIndex) + 1;
        }
        *codePos += sizeof(uint8_t) + sizeof(script_function_callback_t);
        ScriptInterpreter_ReleaseActiveParams();
        *stackTop = ScriptInterpreter_PushTypeOnly(
            script_valueStackTop, SCRIPT_VAR_UNDEFINED);
        break;

    case SCRIPT_OP_BUILTIN_METHOD:
        if (script_errorParameterIndex < 0) {
            script_errorParameterIndex = 1;
        } else if (script_errorParameterIndex > 0) {
            script_errorParameterIndex =
                (script_parameterCount - script_errorParameterIndex) + 2;
        }
        *codePos += sizeof(uint8_t) + sizeof(script_method_callback_t);
        ScriptInterpreter_ReleaseActiveParams();
        *stackTop = ScriptInterpreter_PushTypeOnly(
            script_valueStackTop, SCRIPT_VAR_UNDEFINED);
        break;

    case SCRIPT_OP_WAIT:
        script_errorParameterIndex = 1;
        *stackTop -= 1;
        break;

    case SCRIPT_OP_CALL_FUNCTION:
    case SCRIPT_OP_CALL_FUNCTION_METHOD_CONTEXT: {
        int32_t argumentCount =
            ScriptInterpreter_ReadI32(*codePos +
                                      SCRIPT_INTERPRETER_CODEPOS_OPERAND_BYTES);
        for (int32_t index = 0; index < argumentCount; ++index) {
            RemoveRefToValue(*stackTop);
            *stackTop -= 1;
        }
        if (opcode == SCRIPT_OP_CALL_FUNCTION_METHOD_CONTEXT) {
            RemoveRefToValue(*stackTop);
            *stackTop -= 1;
        }
        /* The shared cleanup tail at 0x0048d25d decrements stackTop once more
         * (SUB EAX,8) before writing UNDEFINED at the resulting slot. */
        *stackTop -= 1;
        (*stackTop)->type = SCRIPT_VAR_UNDEFINED;
        *codePos += SCRIPT_INTERPRETER_FUNCTION_CALL_OPERAND_BYTES;
        break;
    }

    case SCRIPT_OP_CALL_POINTER:
    case SCRIPT_OP_CALL_POINTER_METHOD_CONTEXT: {
        /* The pointer-call cleanup routes through the shared decrementing tail
         * at 0x0048d25d (SUB EAX,8), not the incrementing thread tail at
         * 0x0048d37d. After releasing argumentCount+1 stack values the original
         * decrements stackTop once more and writes UNDEFINED at that slot,
         * unlike the thread-pointer forms below which push a new slot. */
        int32_t argumentCount = ScriptInterpreter_ReadI32(*codePos);
        int32_t valueCount = argumentCount + 1;
        for (int32_t index = 0; index < valueCount; ++index) {
            RemoveRefToValue(*stackTop);
            *stackTop -= 1;
        }
        *stackTop -= 1;
        (*stackTop)->type = SCRIPT_VAR_UNDEFINED;
        *codePos += SCRIPT_INTERPRETER_POINTER_CALL_OPERAND_BYTES;
        break;
    }

    case SCRIPT_OP_THREAD_POINTER:
    case SCRIPT_OP_THREAD_POINTER_METHOD_CONTEXT: {
        int32_t argumentCount = ScriptInterpreter_ReadI32(*codePos);
        int32_t valueCount = argumentCount + 1;
        for (int32_t index = 0; index < valueCount; ++index) {
            RemoveRefToValue(*stackTop);
            *stackTop -= 1;
        }
        *stackTop = ScriptInterpreter_PushTypeOnly(
            *stackTop, SCRIPT_VAR_UNDEFINED);
        *codePos += SCRIPT_INTERPRETER_POINTER_CALL_OPERAND_BYTES;
        break;
    }

    case SCRIPT_OP_THREAD_FUNCTION_METHOD_CONTEXT: {
        int32_t argumentCount =
            ScriptInterpreter_ReadI32(*codePos +
                                      SCRIPT_INTERPRETER_CODEPOS_OPERAND_BYTES);
        int32_t valueCount = argumentCount + 1;
        for (int32_t index = 0; index < valueCount; ++index) {
            RemoveRefToValue(*stackTop);
            *stackTop -= 1;
        }
        *stackTop = ScriptInterpreter_PushTypeOnly(
            *stackTop, SCRIPT_VAR_UNDEFINED);
        *codePos += SCRIPT_INTERPRETER_FUNCTION_CALL_OPERAND_BYTES;
        break;
    }

    case SCRIPT_OP_CAST_OBJECT:
        RemoveRefToValue(*stackTop);
        ClearVariableValue(script_tempValueHandle);
        *fieldRef = GetObject(script_tempValueHandle);
        *stackTop -= 1;
        break;

    case SCRIPT_OP_BIT_NOT:
    case SCRIPT_OP_SIZE:
        RemoveRefToValue(*stackTop);
        (*stackTop)->type = SCRIPT_VAR_UNDEFINED;
        break;

    case SCRIPT_OP_JUMP_ON_FALSE:
    case SCRIPT_OP_JUMP_ON_TRUE_BACK:
    case SCRIPT_OP_SHORT_CIRCUIT_AND:
    case SCRIPT_OP_SHORT_CIRCUIT_OR:
        *stackTop -= 1;
        *codePos += sizeof(int32_t);
        break;

    case SCRIPT_OP_INC:
    case SCRIPT_OP_DEC:
        script_errorParameterIndex = 0;
        break;

    case SCRIPT_OP_WAITTILL:
    case SCRIPT_OP_WAITTILLMATCH:
    case SCRIPT_OP_ENDON:
        if (opcode == SCRIPT_OP_WAITTILLMATCH) {
            *codePos += sizeof(uint8_t);
        }
        RemoveRefToValue(*stackTop);
        RemoveRefToValue(*stackTop - 1);
        *stackTop -= 2;
        break;

    case SCRIPT_OP_NOTIFY:
        while ((*stackTop)->type != SCRIPT_VAR_CODEPOS) {
            RemoveRefToValue(*stackTop);
            *stackTop -= 1;
        }
        RemoveRefToValue(*stackTop);
        *stackTop -= 1;
        break;

    case SCRIPT_OP_SWITCH_JUMP:
        RemoveRefToValue(*stackTop);
        *stackTop -= 1;
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        *codePos = reinterpret_cast<uint8_t *>(
            reinterpret_cast<uintptr_t>(*codePos) +
            static_cast<uintptr_t>(
                static_cast<intptr_t>(switchCaseCount)) *
                sizeof(script_switch_case_table_entry_t));
        break;

    case SCRIPT_OP_VECTOR:
        ClearVector(*stackTop - 2);
        *stackTop -= 2;
        break;

    default:
        break;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: reports a recovered C++ script exception. */
static void ScriptInterpreter_ReportAndContinue(uint8_t *codePos)
{
    RuntimeError(
        codePos, script_errorParameterIndex, script_errorMessage,
        script_errorSource);
    script_errorMessage = NULL;
    script_errorSource = NULL;
    script_errorParameterIndex = 0;
}

/* Source: CoDUOMP.exe 0x00488e70..0x0048d4ef.
 * Evidence: FUN_00488e70_00488e9a.mcode (MSVC SEH prologue),
 * FUN_00488ea0_0048d4bb.mcode (dispatch/body), and
 * FUN_0048cf38_0048d4f0.mcode (fragmented cleanup/tail continuation).
 * FUN_0048c81e_0048c824.mcode is the compiler-generated catch funclet.
 * The corresponding Linux function is coduo_lnxded
 * 0x080ab56a..0x080ae052. */
extern "C" uint16_t
VM_Execute(VariableValue *stackTop, uint8_t *codePos,
           uint16_t thread,
           uint16_t currentObject,
           VariableValue *stackBase)
{
    uint16_t fieldRef = 0;
    int32_t developerDepth = 0;
    int32_t switchCaseCount = 0;
    uint8_t opcode = SCRIPT_OP_NOP;
    qboolean opcodeAlreadyLoaded = qfalse;

    for (;;) {
        try {
            if (opcodeAlreadyLoaded == qfalse) {
                opcode = *codePos++;
            } else {
                opcodeAlreadyLoaded = qfalse;
            }

            switch ((script_interpreter_opcode_e)opcode) {
            case SCRIPT_OP_END: {
                KillThread(thread);
                VariableValue returnValue;
                returnValue.payload = SCRIPT_INTERPRETER_STACK_SENTINEL_VALUE;
                returnValue.type = SCRIPT_VAR_UNDEFINED;
                if (ScriptInterpreter_ReturnFromFrame(
                        &stackTop, &codePos, &thread, stackBase,
                        &returnValue, qfalse) != qfalse) {
                    return thread;
                }
                break;
            }

            case SCRIPT_OP_RETURN: {
                KillThread(thread);
                VariableValue returnValue = *stackTop;
                --stackTop;
                /* RETURN copies payload+type unconditionally (0x004891be). */
                if (ScriptInterpreter_ReturnFromFrame(
                        &stackTop, &codePos, &thread, stackBase,
                        &returnValue, qtrue) != qfalse) {
                    return thread;
                }
                break;
            }

            case SCRIPT_OP_GET_UNDEFINED:
                stackTop = ScriptInterpreter_PushTypeOnly(
                    stackTop, SCRIPT_VAR_UNDEFINED);
                break;

            case SCRIPT_OP_GET_INTEGER:
                stackTop = ScriptInterpreter_Push(
                    stackTop, SCRIPT_VAR_INT,
                    ScriptInterpreter_ReadU32(codePos));
                codePos += sizeof(uint32_t);
                break;

            case SCRIPT_OP_GET_FLOAT:
                stackTop = ScriptInterpreter_Push(
                    stackTop, SCRIPT_VAR_FLOAT,
                    ScriptInterpreter_ReadU32(codePos));
                codePos += sizeof(uint32_t);
                break;

            case SCRIPT_OP_GET_STRING:
            case SCRIPT_OP_GET_ISTRING: {
                uint16_t string =
                    ScriptInterpreter_ReadU16(codePos);
                codePos += sizeof(uint16_t);
                SL_AddRefToString(string);
                stackTop = ScriptInterpreter_Push(
                    stackTop,
                    opcode == SCRIPT_OP_GET_STRING
                        ? SCRIPT_VAR_STRING
                        : SCRIPT_VAR_LOCALIZED_STRING,
                    string);
                break;
            }

            case SCRIPT_OP_GET_SELF_OBJECT:
                fieldRef = GetSelf(thread);
                break;

            case SCRIPT_OP_GET_LEVEL_OBJECT:
                fieldRef = script_levelHandle;
                break;

            case SCRIPT_OP_GET_GAME_OBJECT:
                fieldRef = script_gameHandle;
                break;

            case SCRIPT_OP_GET_SELF: {
                uint16_t parent =
                    GetSelf(thread);
                AddRefToObject(parent);
                stackTop =
                    ScriptInterpreter_Push(stackTop, SCRIPT_VAR_OBJECT,
                                           parent);
                break;
            }

            case SCRIPT_OP_GET_LEVEL:
                AddRefToObject(script_levelHandle);
                stackTop = ScriptInterpreter_Push(
                    stackTop, SCRIPT_VAR_OBJECT, script_levelHandle);
                break;

            case SCRIPT_OP_GET_ANIM: {
                VariableValue value;
                GetVariableValue(script_animArrayHandle, &value);
                ++stackTop;
                *stackTop = value;
                AddRefToValue(stackTop);
                break;
            }

            case SCRIPT_OP_GET_GAME:
                AddRefToObject(script_gameHandle);
                stackTop = ScriptInterpreter_Push(stackTop,
                                                  SCRIPT_VAR_OBJECT,
                                                  script_gameHandle);
                break;

            case SCRIPT_OP_GET_ANIMATION:
                stackTop = ScriptInterpreter_Push(
                    stackTop, SCRIPT_VAR_ANIMATION,
                    ScriptInterpreter_ReadU32(codePos));
                codePos += sizeof(uint32_t);
                break;

            case SCRIPT_OP_SET_ANIM_OBJECT:
                fieldRef = script_animArrayHandle;
                break;

            case SCRIPT_OP_GET_FUNCTION:
                stackTop = ScriptInterpreter_Push(
                    stackTop, SCRIPT_VAR_FUNCTION,
                    ScriptInterpreter_ReadValuePayload(codePos));
                codePos += SCRIPT_INTERPRETER_CODEPOS_OPERAND_BYTES;
                break;

            case SCRIPT_OP_GET_LOCAL: {
                uint16_t child =
                    GetVariable(thread,
                                            ScriptInterpreter_ReadU16(codePos));
                codePos += sizeof(uint16_t);
                ++stackTop;
                GetVariableValue(child, stackTop);
                AddRefToValue(stackTop);
                break;
            }

            case SCRIPT_OP_SET_LOCAL_REF:
                fieldRef = GetVariable(
                    thread, ScriptInterpreter_ReadU16(codePos));
                codePos += sizeof(uint16_t);
                break;

            case SCRIPT_OP_CLEAR_LOCAL:
                SafeRemoveVariable(
                    thread, ScriptInterpreter_ReadU16(codePos));
                codePos += sizeof(uint16_t);
                break;

            case SCRIPT_OP_EVAL_INDEX:
                --stackTop;
                EvalArray(stackTop, stackTop + 1);
                break;

            case SCRIPT_OP_SET_INDEXED_REF:
                fieldRef = EvalArrayRef(fieldRef, stackTop);
                --stackTop;
                break;

            case SCRIPT_OP_CLEAR_INDEXED:
                ClearArray(fieldRef, stackTop);
                --stackTop;
                break;

            case SCRIPT_OP_NEW_ARRAY:
                ++stackTop;
                GetEmptyArray(stackTop);
                break;

            case SCRIPT_OP_GET_FIELD: {
                uint16_t child =
                    GetVariableField(
                        fieldRef, ScriptInterpreter_ReadU16(codePos));
                codePos += sizeof(uint16_t);
                ++stackTop;
                GetVariableFieldValue(child, stackTop);
                break;
            }

            case SCRIPT_OP_SET_FIELD_REF:
                fieldRef = GetVariableField(
                    fieldRef, ScriptInterpreter_ReadU16(codePos));
                codePos += sizeof(uint16_t);
                break;

            case SCRIPT_OP_CLEAR_FIELD:
                ClearVariableField(
                    fieldRef, ScriptInterpreter_ReadU16(codePos));
                codePos += sizeof(uint16_t);
                break;

            case SCRIPT_OP_STORE_TEMP:
                SetVariableValue(script_tempValueHandle, stackTop);
                fieldRef = script_tempValueHandle;
                --stackTop;
                break;

            case SCRIPT_OP_SET_LOCAL:
            case SCRIPT_OP_SET_LOCAL_AND_CLEAR:
                if (stackTop->type == SCRIPT_VAR_CODEPOS) {
                    if (opcode == SCRIPT_OP_SET_LOCAL_AND_CLEAR) {
                        SafeRemoveVariable(
                            thread, ScriptInterpreter_ReadU16(codePos));
                    }
                } else {
                    uint16_t child =
                        GetVariable(
                            thread, ScriptInterpreter_ReadU16(codePos));
                    SetVariableFieldValue(child, stackTop);
                    --stackTop;
                }
                codePos += sizeof(uint16_t);
                break;

            case SCRIPT_OP_CLEAR_PARAMS_UNTIL_MARKER:
                while (stackTop->type != SCRIPT_VAR_CODEPOS) {
                    RemoveRefToValue(stackTop);
                    --stackTop;
                }
                break;

            case SCRIPT_OP_FUNCTION_PARAMETERS_DONE:
                if (stackTop->type != SCRIPT_VAR_CODEPOS) {
                    Scr_Error("function called with too many parameters");
                }
                break;

            case SCRIPT_OP_STORE_REF:
                SetVariableFieldValue(fieldRef, stackTop);
                --stackTop;
                break;

            case SCRIPT_OP_BUILTIN_FUNCTION: {
                script_parameterCount = *codePos;
                script_valueStackTop = stackTop;
                script_function_callback_t callback =
                    ScriptInterpreter_ReadBuiltinFunction(codePos + 1);
                callback();
                stackTop = script_valueStackTop;
                ScriptInterpreter_ReleaseActiveParams();
                stackTop = script_valueStackTop;
                codePos += sizeof(uint8_t) + sizeof(callback);
                if (script_valueStackDepth == 0) {
                    stackTop++;
                    stackTop->type = SCRIPT_VAR_UNDEFINED;
                } else {
                    script_valueStackDepth = 0;
                }
                break;
            }

            case SCRIPT_OP_BUILTIN_METHOD: {
                script_parameterCount = *codePos;
                script_valueStackTop = stackTop - 1;
                if (stackTop->type != SCRIPT_VAR_OBJECT) {
                    script_variable_type_t type =
                        (script_variable_type_t)stackTop->type;
                    RemoveRefToValue(stackTop);
                    script_errorParameterIndex = -1;
                    Scr_Error(va("%s is not an entity",
                                          script_variableTypeNames[type]));
                }

                uint16_t object =
                    (uint16_t)stackTop->payload;
                script_variable_type_t type = GetVarType(object);
                if (type != SCRIPT_VAR_ENTITY) {
                    RemoveRefToObject(object);
                    script_errorParameterIndex = -1;
                    Scr_Error(
                        va("%s is not an entity", script_variableTypeNames[type]));
                }

                int32_t objectNum =
                    (int32_t)GetSelf(object);
                RemoveRefToObject(object);
                script_method_callback_t callback =
                    ScriptInterpreter_ReadBuiltinMethod(codePos + 1);
                callback(objectNum);
                stackTop = script_valueStackTop;
                ScriptInterpreter_ReleaseActiveParams();
                stackTop = script_valueStackTop;
                codePos += sizeof(uint8_t) + sizeof(callback);
                if (script_valueStackDepth == 0) {
                    stackTop++;
                    stackTop->type = SCRIPT_VAR_UNDEFINED;
                } else {
                    script_valueStackDepth = 0;
                }
                break;
            }

            case SCRIPT_OP_WAIT:
                if (developerDepth != 0) {
                    Scr_Error(
                        "wait not allowed in /# ... #/ comment (call as a thread to fix)");
                }
                if (CastFloat(stackTop) == qfalse) {
                    ScriptRuntime_RaiseError();
                } else {
                    float waitSeconds =
                        ScriptInterpreter_PayloadFloat(stackTop->payload);
                    /* The original tests C0 after FCOMP vs 0.0 (TEST AH,0x1;
                     * JNZ at 0x00489d22), which is set for both an ordered
                     * negative AND an unordered NaN compare. Expressing this as
                     * `waitSeconds < 0.0f` misses NaN (that comparison is false
                     * for NaN), letting a NaN wait fall through to a garbage
                     * resume time instead of erroring. Use !(x >= 0.0f) so NaN
                     * takes the negative-wait error path as the DLL does. */
                    if (!(waitSeconds >= 0.0f)) {
                        Scr_Error(
                            va("negative wait of %g is not allowed",
                               (double)waitSeconds));
                    } else if (waitSeconds >=
                        (float)SCRIPT_INTERPRETER_MAX_WAIT_SECONDS) {
                        Scr_Error(
                            va("wait of %.0f seconds is too long",
                               (double)waitSeconds));
                        Scr_Error(
                            va("negative wait of %g is not allowed",
                               (double)waitSeconds));
                    } else {
                        int32_t waitMsec = FastRound(
                            waitSeconds * SCRIPT_INTERPRETER_WAIT_SCALE);
                        uint32_t resumeTime =
                            (script_currentTimeKey + (uint32_t)waitMsec) &
                            SCRIPT_INTERPRETER_TIME_MASK;
                        uint16_t timeSlot =
                            GetVariable(script_timeArrayHandle,
                                                    resumeTime);
                        uint16_t timeArray =
                            GetArray(timeSlot);
                        GetObjectVariable(timeArray,
                                                      currentObject);
                        VM_ArchiveStack(
                            (int32_t)(stackTop - stackBase - 1),
                            codePos, thread, currentObject, stackBase,
                            resumeTime);
                        stackBase[1].type = SCRIPT_VAR_UNDEFINED;
                        return currentObject;
                    }
                }
                /* fall through */

            case SCRIPT_OP_THREAD_MARKER:
                AddRefToObject(thread);
                stackTop = ScriptInterpreter_Push(stackTop,
                                                  SCRIPT_VAR_OBJECT,
                                                  thread);
                stackTop++;
                stackTop->type = SCRIPT_VAR_UNDEFINED;
                break;

            case SCRIPT_OP_CALL_FUNCTION:
            case SCRIPT_OP_CALL_FUNCTION_METHOD_CONTEXT: {
                uint8_t *targetCodePos = ScriptInterpreter_CodeposFromPayload(
                    ScriptInterpreter_ReadValuePayload(codePos));
                int32_t argumentCount =
                    ScriptInterpreter_ReadI32(
                        codePos + SCRIPT_INTERPRETER_CODEPOS_OPERAND_BYTES);
                uint8_t *returnCodePos =
                    codePos + SCRIPT_INTERPRETER_FUNCTION_CALL_OPERAND_BYTES;
                uint16_t parent;

                RemoveRefToObject(thread);
                if (opcode == SCRIPT_OP_CALL_FUNCTION) {
                    parent = GetSelf(thread);
                } else {
                    uint16_t object =
                        ScriptInterpreter_RequireObject(stackTop, 1);
                    parent = object;
                    --stackTop;
                }

                ScriptInterpreter_EnterFunctionCall(
                    &stackTop, &codePos, &thread, parent, argumentCount,
                    targetCodePos, returnCodePos,
                    opcode == SCRIPT_OP_CALL_FUNCTION ? qtrue : qfalse,
                    /* 0x25/0x27 do not set errorParameterIndex on overflow. */
                    qfalse);
                break;
            }

            case SCRIPT_OP_CALL_POINTER:
            case SCRIPT_OP_CALL_POINTER_METHOD_CONTEXT: {
                int32_t argumentCount = ScriptInterpreter_ReadI32(codePos);

                RemoveRefToObject(thread);
                if (stackTop->type != SCRIPT_VAR_FUNCTION) {
                    script_variable_type_t badType =
                        (script_variable_type_t)stackTop->type;
                    if (opcode == SCRIPT_OP_CALL_POINTER_METHOD_CONTEXT) {
                        RemoveRefToValue(stackTop);
                        --stackTop;
                    }
                    Scr_Error(
                        va("%s is not a function pointer",
                           script_variableTypeNames[badType]));
                }

                uint8_t *targetCodePos =
                    ScriptInterpreter_CodeposFromPayload(stackTop->payload);
                --stackTop;

                uint16_t parent;
                if (opcode == SCRIPT_OP_CALL_POINTER) {
                    parent = GetSelf(thread);
                } else {
                    uint16_t object =
                        ScriptInterpreter_RequireObject(stackTop, 2);
                    parent = object;
                    --stackTop;
                }

                ScriptInterpreter_EnterFunctionCall(
                    &stackTop, &codePos, &thread, parent, argumentCount,
                    targetCodePos,
                    codePos + SCRIPT_INTERPRETER_POINTER_CALL_OPERAND_BYTES,
                    opcode == SCRIPT_OP_CALL_POINTER ? qtrue : qfalse,
                    /* 0x26/0x28 set errorParameterIndex on overflow. */
                    qtrue);
                break;
            }

            case SCRIPT_OP_THREAD_FUNCTION:
            case SCRIPT_OP_THREAD_FUNCTION_METHOD_CONTEXT: {
                uint8_t *targetCodePos = ScriptInterpreter_CodeposFromPayload(
                    ScriptInterpreter_ReadValuePayload(codePos));
                int32_t argumentCount =
                    ScriptInterpreter_ReadI32(
                        codePos + SCRIPT_INTERPRETER_CODEPOS_OPERAND_BYTES);
                VariableValue *marker;
                uint16_t parent;

                if (opcode == SCRIPT_OP_THREAD_FUNCTION) {
                    ScriptInterpreter_CheckCallDepth(qtrue);
                    ScriptInterpreter_SaveCallCodepos(codePos);
                    parent = GetSelf(thread);
                    AddRefToObject(parent);
                    marker = stackTop - argumentCount;
                    stackTop = ScriptInterpreter_RunThreadCall(
                        stackTop, targetCodePos, parent, marker);
                } else {
                    parent = ScriptInterpreter_RequireObject(stackTop, 2);
                    ScriptInterpreter_CheckCallDepth(qtrue);
                    ScriptInterpreter_SaveCallCodepos(codePos);
                    marker = stackTop - argumentCount - 1;
                    stackTop = ScriptInterpreter_RunThreadCall(
                        stackTop - 1, targetCodePos, parent, marker);
                }

                codePos += SCRIPT_INTERPRETER_FUNCTION_CALL_OPERAND_BYTES;
                break;
            }

            case SCRIPT_OP_THREAD_POINTER:
            case SCRIPT_OP_THREAD_POINTER_METHOD_CONTEXT: {
                if (stackTop->type != SCRIPT_VAR_FUNCTION) {
                    script_variable_type_t badType =
                        (script_variable_type_t)stackTop->type;
                    if (opcode == SCRIPT_OP_THREAD_POINTER_METHOD_CONTEXT) {
                        RemoveRefToValue(stackTop);
                        --stackTop;
                    }
                    Scr_Error(
                        va("%s is not a function pointer",
                           script_variableTypeNames[badType]));
                }

                int32_t argumentCount = ScriptInterpreter_ReadI32(codePos);
                uint8_t *targetCodePos =
                    ScriptInterpreter_CodeposFromPayload(stackTop->payload);
                VariableValue *marker;

                if (opcode == SCRIPT_OP_THREAD_POINTER) {
                    ScriptInterpreter_CheckCallDepth(qtrue);
                    ScriptInterpreter_SaveCallCodepos(codePos);
                    uint16_t parent =
                        GetSelf(thread);
                    AddRefToObject(parent);
                    marker = stackTop - argumentCount - 1;
                    stackTop = ScriptInterpreter_RunThreadCall(
                        stackTop - 1, targetCodePos, parent, marker);
                } else {
                    --stackTop;
                    uint16_t parent =
                        ScriptInterpreter_RequireObject(stackTop, 2);
                    ScriptInterpreter_CheckCallDepth(qtrue);
                    ScriptInterpreter_SaveCallCodepos(codePos);
                    marker = stackTop - argumentCount - 1;
                    stackTop = ScriptInterpreter_RunThreadCall(
                        stackTop - 1, targetCodePos, parent, marker);
                }

                codePos += SCRIPT_INTERPRETER_POINTER_CALL_OPERAND_BYTES;
                break;
            }

            case SCRIPT_OP_DROP_TOP:
                RemoveRefToValue(stackTop);
                --stackTop;
                break;

            case SCRIPT_OP_CAST_OBJECT:
                fieldRef = CastFieldObject(stackTop);
                --stackTop;
                break;

            case SCRIPT_OP_CAST_BOOL:
                if (stackTop->type == SCRIPT_VAR_INT) {
                    stackTop->payload = stackTop->payload != 0 ? qtrue : qfalse;
                } else if (CastBool(stackTop) == qfalse) {
                    ScriptRuntime_RaiseError();
                }
                break;

            case SCRIPT_OP_CAST_INT:
                if (CastInt(stackTop) == qfalse) {
                    ScriptRuntime_RaiseError();
                }
                break;

            case SCRIPT_OP_CAST_FLOAT:
                if (CastFloat(stackTop) == qfalse) {
                    ScriptRuntime_RaiseError();
                }
                break;

            case SCRIPT_OP_CAST_STRING:
                if (CastString(stackTop) == qfalse) {
                    ScriptRuntime_RaiseError();
                }
                break;

            case SCRIPT_OP_BOOL_NOT:
                if (stackTop->type != SCRIPT_VAR_INT &&
                    CastBool(stackTop) == qfalse) {
                    ScriptRuntime_RaiseError();
                }
                stackTop->payload = stackTop->payload == 0 ? qtrue : qfalse;
                break;

            case SCRIPT_OP_BIT_NOT:
                if (stackTop->type != SCRIPT_VAR_INT) {
                    Scr_Error(
                        va("~ cannot be applied to \"%s\"",
                           script_variableTypeNames[stackTop->type]));
                }
                stackTop->payload =
                    ScriptInterpreter_U32Payload(
                        ~(uint32_t)stackTop->payload);
                break;

            case SCRIPT_OP_JUMP_ON_FALSE:
            case SCRIPT_OP_SHORT_CIRCUIT_AND:
            case SCRIPT_OP_SHORT_CIRCUIT_OR: {
                int32_t offset = ScriptInterpreter_ReadI32(codePos);
                if (stackTop->type != SCRIPT_VAR_INT &&
                    CastBool(stackTop) == qfalse) {
                    ScriptRuntime_RaiseError();
                }

                qboolean value = stackTop->payload != 0 ? qtrue : qfalse;
                if (opcode == SCRIPT_OP_JUMP_ON_FALSE) {
                    if (value == qfalse) {
                        codePos += offset + sizeof(int32_t);
                    } else {
                        codePos += sizeof(int32_t);
                    }
                    --stackTop;
                } else if ((opcode == SCRIPT_OP_SHORT_CIRCUIT_OR &&
                            value != qfalse) ||
                           (opcode == SCRIPT_OP_SHORT_CIRCUIT_AND &&
                            value == qfalse)) {
                    codePos += offset + sizeof(int32_t);
                } else {
                    --stackTop;
                    codePos += sizeof(int32_t);
                }
                break;
            }

            case SCRIPT_OP_JUMP_ON_TRUE_BACK: {
                int32_t offset = ScriptInterpreter_ReadI32(codePos);
                if (stackTop->type != SCRIPT_VAR_INT &&
                    CastBool(stackTop) == qfalse) {
                    script_errorParameterIndex = 1;
                    ScriptRuntime_RaiseError();
                }
                if (stackTop->payload == 0) {
                    --stackTop;
                    codePos += sizeof(int32_t);
                } else {
                    --stackTop;
                    if ((int32_t)(rdtsc() -
                                  script_loopWatchdogTick) >= 0) {
                        codePos += offset;
                    } else if (script_loopWatchdogWarningFlag != 0) {
                        Com_Printf(
                            "script runtime warning: potential infinite loop in script.\n");
#if defined(WINDOWS_BEHAVIOR)
                        RuntimeError(0, 0, NULL, NULL);
#else
                        Scr_PrintPrevCodePos(0, codePos, 0);
#endif
                        script_loopWatchdogTick =
                            rdtsc();
                        codePos += offset;
                    } else if (script_runtimeDeveloperFlag == 0) {
                        Com_Printf(
                            "script runtime error: potential infinite loop in script - killing thread.\n");
#if defined(WINDOWS_BEHAVIOR)
                        RuntimeError(0, 0, NULL, NULL);
#else
                        Scr_PrintPrevCodePos(0, codePos, 0);
#endif
                        script_loopWatchdogTick =
                            rdtsc();
                        while (true) {
                            VariableValue returnValue;
                            returnValue.payload =
                                SCRIPT_INTERPRETER_STACK_SENTINEL_VALUE;
                            returnValue.type = SCRIPT_VAR_UNDEFINED;
                            if (ScriptInterpreter_ReturnFromFrame(
                                    &stackTop, &codePos, &thread, stackBase,
                                    &returnValue, qfalse) != qfalse) {
                                return thread;
                            }
                        }
                    } else {
#if defined(WINDOWS_BEHAVIOR)
                        Scr_DumpScriptThreads();
#endif
                        Scr_TerminalError("potential infinite loop in script");
                    }
                }
                break;
            }

            case SCRIPT_OP_JUMP:
                codePos += ScriptInterpreter_ReadI32(codePos);
                break;

            case SCRIPT_OP_JUMP_BACK:
                if ((int32_t)(rdtsc() -
                              script_loopWatchdogTick) >= 0) {
                    codePos += ScriptInterpreter_ReadI32(codePos);
                } else if (script_loopWatchdogWarningFlag != 0) {
                    Com_Printf(
                        "script runtime warning: potential infinite loop in script.\n");
#if defined(WINDOWS_BEHAVIOR)
                    RuntimeError(0, 0, NULL, NULL);
#else
                    Scr_PrintPrevCodePos(0, codePos, 0);
#endif
                    script_loopWatchdogTick = rdtsc();
                    codePos += ScriptInterpreter_ReadI32(codePos);
                } else if (script_runtimeDeveloperFlag == 0) {
                    Com_Printf(
                        "script runtime error: potential infinite loop in script - killing thread.\n");
#if defined(WINDOWS_BEHAVIOR)
                    RuntimeError(0, 0, NULL, NULL);
#else
                    Scr_PrintPrevCodePos(0, codePos, 0);
#endif
                    script_loopWatchdogTick = rdtsc();
                    while (true) {
                        VariableValue returnValue;
                        returnValue.payload =
                            SCRIPT_INTERPRETER_STACK_SENTINEL_VALUE;
                        returnValue.type = SCRIPT_VAR_UNDEFINED;
                        if (ScriptInterpreter_ReturnFromFrame(
                                &stackTop, &codePos, &thread, stackBase,
                                &returnValue, qfalse) != qfalse) {
                            return thread;
                        }
                    }
                } else {
#if defined(WINDOWS_BEHAVIOR)
                    Scr_DumpScriptThreads();
#endif
                    Scr_TerminalError("potential infinite loop in script");
                }
                break;

            case SCRIPT_OP_INC:
            case SCRIPT_OP_DEC: {
                ++stackTop;
                GetVariableFieldValue(fieldRef, stackTop);
                if (stackTop->type != SCRIPT_VAR_INT) {
                    Scr_Error(
                        va(opcode == SCRIPT_OP_INC
                               ? "++ must be applied to an int (applied to %s)"
                               : "-- must be applied to an int (applied to %s)",
                           script_variableTypeNames[stackTop->type]));
                }
                stackTop->payload = ScriptInterpreter_U32Payload(
                    (uint32_t)stackTop->payload +
                    (opcode == SCRIPT_OP_INC ? 1u : (uint32_t)-1));
                break;
            }

            case SCRIPT_OP_BIT_OR:
            case SCRIPT_OP_BIT_XOR:
            case SCRIPT_OP_BIT_AND:
            case SCRIPT_OP_SHIFT_LEFT:
            case SCRIPT_OP_SHIFT_RIGHT: {
                VariableValue *left = stackTop - 1;
                if (CastWeakerPair(left, stackTop) == qfalse) {
                    ScriptRuntime_RaiseError();
                }
                if (left->type != SCRIPT_VAR_INT) {
                    UnmatchingTypesError(left, stackTop);
                    ScriptRuntime_RaiseError();
                }
                uint32_t leftValue = (uint32_t)left->payload;
                uint32_t right = (uint32_t)stackTop->payload;
                switch (opcode) {
                case SCRIPT_OP_BIT_OR:
                    left->payload = ScriptInterpreter_U32Payload(leftValue | right);
                    break;
                case SCRIPT_OP_BIT_XOR:
                    left->payload = ScriptInterpreter_U32Payload(leftValue ^ right);
                    break;
                case SCRIPT_OP_BIT_AND:
                    left->payload = ScriptInterpreter_U32Payload(leftValue & right);
                    break;
                case SCRIPT_OP_SHIFT_LEFT:
                    left->payload = ScriptInterpreter_U32Payload(
                        leftValue << (right & SCRIPT_INTERPRETER_SHIFT_MASK));
                    break;
                case SCRIPT_OP_SHIFT_RIGHT:
                    left->payload = ScriptInterpreter_U32Payload(
                        coduo_int32_sar_bits(
                            leftValue,
                            right & SCRIPT_INTERPRETER_SHIFT_MASK));
                    break;
                default:
                    break;
                }
                stackTop = left;
                break;
            }

            case SCRIPT_OP_EQUAL:
                --stackTop;
                if (CheckEquality(stackTop, stackTop + 1) == qfalse) {
                    ScriptRuntime_RaiseError();
                }
                break;

            case SCRIPT_OP_NOT_EQUAL:
                --stackTop;
                if (CastWeakerPair(stackTop, stackTop + 1) == qfalse) {
                    ScriptRuntime_RaiseError();
                }
                switch (stackTop->type) {
                case SCRIPT_VAR_UNDEFINED:
                    stackTop->type = SCRIPT_VAR_INT;
                    stackTop->payload = qfalse;
                    break;
                case SCRIPT_VAR_STRING:
                case SCRIPT_VAR_LOCALIZED_STRING: {
                    uint16_t leftString =
                        (uint16_t)stackTop->payload;
                    uint16_t rightString =
                        (uint16_t)stackTop[1].payload;
                    stackTop->type = SCRIPT_VAR_INT;
                    SL_RemoveRefToString(leftString);
                    SL_RemoveRefToString(rightString);
                    stackTop->payload =
                        leftString != rightString ? qtrue : qfalse;
                    break;
                }
                case SCRIPT_VAR_VECTOR: {
                    float *leftVector =
                        ScriptInterpreter_Vector(stackTop->payload);
                    float *rightVector =
                        ScriptInterpreter_Vector(stackTop[1].payload);
                    qboolean notEqual =
                        leftVector[0] != rightVector[0] ||
                        leftVector[1] != rightVector[1] ||
                        leftVector[2] != rightVector[2] ? qtrue : qfalse;
                    stackTop->type = SCRIPT_VAR_INT;
                    RemoveRefToVector(
                        (const float *)stackTop->payload);
                    RemoveRefToVector(
                        (const float *)stackTop[1].payload);
                    stackTop->payload = notEqual;
                    break;
                }
                case SCRIPT_VAR_FLOAT: {
                    float leftFloat =
                        ScriptInterpreter_PayloadFloat(stackTop->payload);
                    float rightFloat =
                        ScriptInterpreter_PayloadFloat(stackTop[1].payload);
                    stackTop->type = SCRIPT_VAR_INT;
                    stackTop->payload =
                        ScriptInterpreter_AbsFloat(leftFloat - rightFloat) >=
                                SCRIPT_INTERPRETER_FLOAT_EQUAL_EPSILON
                            ? qtrue
                            : qfalse;
                    break;
                }
                case SCRIPT_VAR_INT:
                    stackTop->payload =
                        (uint32_t)stackTop->payload !=
                                (uint32_t)stackTop[1].payload
                            ? qtrue
                            : qfalse;
                    break;
                case SCRIPT_VAR_OBJECT: {
                    uint16_t leftObject =
                        (uint16_t)stackTop->payload;
                    uint16_t rightObject =
                        (uint16_t)stackTop[1].payload;
                    stackTop->type = SCRIPT_VAR_INT;
                    RemoveRefToObject(leftObject);
                    RemoveRefToObject(rightObject);
                    stackTop->payload =
                        leftObject != rightObject ? qtrue : qfalse;
                    break;
                }
                case SCRIPT_VAR_ANIMATION:
                    stackTop->type = SCRIPT_VAR_INT;
                    stackTop->payload =
                        stackTop->payload != stackTop[1].payload ? qtrue
                                                                 : qfalse;
                    break;
                default:
                    UnmatchingTypesError(stackTop, stackTop + 1);
                    ScriptRuntime_RaiseError();
                    break;
                }
                break;

            case SCRIPT_OP_LESS:
            case SCRIPT_OP_GREATER:
            case SCRIPT_OP_LESS_EQUAL:
            case SCRIPT_OP_GREATER_EQUAL: {
                VariableValue *left = stackTop - 1;
                if (CastWeakerPair(left, stackTop) == qfalse) {
                    ScriptRuntime_RaiseError();
                }

                qboolean result = qfalse;
                if (left->type == SCRIPT_VAR_FLOAT) {
                    float lhs = ScriptInterpreter_PayloadFloat(left->payload);
                    float rhs = ScriptInterpreter_PayloadFloat(stackTop->payload);
                    if (opcode == SCRIPT_OP_LESS) {
                        result = lhs < rhs ? qtrue : qfalse;
                    } else if (opcode == SCRIPT_OP_GREATER) {
                        result = rhs < lhs ? qtrue : qfalse;
                    } else if (opcode == SCRIPT_OP_LESS_EQUAL) {
                        result = lhs <= rhs ? qtrue : qfalse;
                    } else {
                        result = rhs <= lhs ? qtrue : qfalse;
                    }
                } else if (left->type == SCRIPT_VAR_INT) {
                    int32_t lhs = (int32_t)left->payload;
                    int32_t rhs = (int32_t)stackTop->payload;
                    if (opcode == SCRIPT_OP_LESS) {
                        result = lhs < rhs ? qtrue : qfalse;
                    } else if (opcode == SCRIPT_OP_GREATER) {
                        result = rhs < lhs ? qtrue : qfalse;
                    } else if (opcode == SCRIPT_OP_LESS_EQUAL) {
                        result = lhs <= rhs ? qtrue : qfalse;
                    } else {
                        result = rhs <= lhs ? qtrue : qfalse;
                    }
                } else {
                    UnmatchingTypesError(left, stackTop);
                    ScriptRuntime_RaiseError();
                }
                left->type = SCRIPT_VAR_INT;
                left->payload = result;
                stackTop = left;
                break;
            }

            case SCRIPT_OP_PLUS:
            case SCRIPT_OP_MINUS:
            case SCRIPT_OP_MULTIPLY:
            case SCRIPT_OP_DIVIDE:
            case SCRIPT_OP_MODULO: {
                VariableValue *left = stackTop - 1;
                if (CastWeakerPair(left, stackTop) == qfalse) {
                    ScriptRuntime_RaiseError();
                }

                if (left->type == SCRIPT_VAR_INT) {
                    int32_t lhs = (int32_t)left->payload;
                    int32_t rhs = (int32_t)stackTop->payload;
                    switch (opcode) {
                    case SCRIPT_OP_PLUS:
                        left->payload = ScriptInterpreter_U32Payload(
                            (uint32_t)lhs + (uint32_t)rhs);
                        break;
                    case SCRIPT_OP_MINUS:
                        left->payload = ScriptInterpreter_U32Payload(
                            (uint32_t)lhs - (uint32_t)rhs);
                        break;
                    case SCRIPT_OP_MULTIPLY:
                        left->payload = ScriptInterpreter_U32Payload(
                            (uint32_t)lhs * (uint32_t)rhs);
                        break;
                    case SCRIPT_OP_DIVIDE:
                        if (rhs == 0) {
                            left->payload = 0;
                            Scr_Error("divide by 0");
                        }
                        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                        if (lhs == INT32_MIN && rhs == -1) {
                            left->payload = 0;
                            Scr_Error("integer division overflow");
                        }
                        left->payload = ScriptInterpreter_U32Payload(lhs / rhs);
                        break;
                    case SCRIPT_OP_MODULO:
                        if (rhs == 0) {
                            left->payload = 0;
                            Scr_Error("divide by 0");
                        }
                        /* IDIV raises the same exception before producing the
                         * mathematically representable zero remainder. Keep
                         * division and modulo behavior consistent. */
                        if (lhs == INT32_MIN && rhs == -1) {
                            left->payload = 0;
                            Scr_Error("integer division overflow");
                        }
                        left->payload = ScriptInterpreter_U32Payload(lhs % rhs);
                        break;
                    default:
                        break;
                    }
                } else if (left->type == SCRIPT_VAR_FLOAT) {
                    float lhs = ScriptInterpreter_PayloadFloat(left->payload);
                    float rhs = ScriptInterpreter_PayloadFloat(stackTop->payload);
                    float result = 0.0f;
                    switch (opcode) {
                    case SCRIPT_OP_PLUS:
                        result = lhs + rhs;
                        break;
                    case SCRIPT_OP_MINUS:
                        result = lhs - rhs;
                        break;
                    case SCRIPT_OP_MULTIPLY:
                        result = lhs * rhs;
                        break;
                    case SCRIPT_OP_DIVIDE:
                        if (rhs == 0.0f && rhs == rhs) {
                            left->payload = ScriptInterpreter_FloatPayload(0.0f);
                            Scr_Error("divide by 0");
                        }
                        result = lhs / rhs;
                        break;
                    case SCRIPT_OP_MODULO:
                        UnmatchingTypesError(left, stackTop);
                        ScriptRuntime_RaiseError();
                        break;
                    default:
                        break;
                    }
                    left->payload = ScriptInterpreter_FloatPayload(result);
                } else if (left->type == SCRIPT_VAR_VECTOR &&
                           (opcode == SCRIPT_OP_PLUS ||
                            opcode == SCRIPT_OP_MINUS)) {
                    float *leftVector = ScriptInterpreter_Vector(left->payload);
                    float *rightVector = ScriptInterpreter_Vector(stackTop->payload);
                    float *result = AllocVector();
                    uintptr_t resultPayload =
                        (uintptr_t)result;

                    for (int32_t axis = 0; axis < 3; ++axis) {
                        result[axis] =
                            opcode == SCRIPT_OP_PLUS
                                ? leftVector[axis] + rightVector[axis]
                                : leftVector[axis] - rightVector[axis];
                    }

                    RemoveRefToVector((const float *)left->payload);
                    RemoveRefToVector(
                        (const float *)stackTop->payload);
                    left->payload = resultPayload;
                } else if (opcode == SCRIPT_OP_PLUS &&
                           left->type == SCRIPT_VAR_STRING) {
                    uint16_t leftString =
                        (uint16_t)left->payload;
                    uint16_t rightString =
                        (uint16_t)stackTop->payload;
                    left->payload = VM_ConcatenateStrings(left);
                    SL_RemoveRefToString(leftString);
                    SL_RemoveRefToString(rightString);
                } else {
                    UnmatchingTypesError(left, stackTop);
                    ScriptRuntime_RaiseError();
                }
                stackTop = left;
                break;
            }

            case SCRIPT_OP_SIZE:
                GetSizeValue(stackTop);
                break;

            case SCRIPT_OP_WAITTILL:
            case SCRIPT_OP_WAITTILLMATCH: {
                if (developerDepth != 0) {
                    Scr_Error(
                        "waittill not allowed in /# ... #/ comment (call as a thread to fix)");
                }

                uint16_t object =
                    ScriptInterpreter_RequireObject(stackTop, 2);
                if ((stackTop - 1)->type != SCRIPT_VAR_STRING) {
                    script_errorParameterIndex = 1;
                    Scr_Error(
                        "first parameter of waittill must evaluate to a string");
                }

                uint16_t notifyName =
                    (uint16_t)(stackTop - 1)->payload;
                uint16_t waitSlot =
                    GetVariable(object,
                                            SCRIPT_INTERPRETER_ENDON_NOTIFY_NAME);
                uint16_t waitRoot =
                    GetArray(waitSlot);
                uint16_t notifySlot =
                    GetVariable(waitRoot, notifyName);
                uint16_t notifyBucket =
                    GetArray(notifySlot);
                GetObjectVariable(notifyBucket, currentObject);

                VariableValue objectValue;
                objectValue.type = SCRIPT_VAR_OBJECT;
                objectValue.payload = object;

                uint16_t parent =
                    GetSelf(currentObject);
                uint16_t pauseSlot =
                    GetObjectVariable(script_pauseArrayHandle,
                                                  parent);
                uint16_t pauseBucket =
                    GetArray(pauseSlot);
                uint16_t pauseThreadSlot =
                    GetObjectVariable(pauseBucket, currentObject);
                SetNewVariableValue(pauseThreadSlot, &objectValue);

                SetThreadNotifyName(currentObject, notifyName);
                SL_RemoveRefToString(notifyName);
                VM_ArchiveStack(
                    (int32_t)(stackTop - stackBase - 2),
                    codePos, thread, currentObject, stackBase,
                    SCRIPT_INTERPRETER_STACK_SENTINEL_VALUE);
                stackBase[1].type = SCRIPT_VAR_UNDEFINED;
                return currentObject;
            }

            case SCRIPT_OP_NOTIFY: {
                uint16_t object =
                    ScriptInterpreter_RequireObject(stackTop, 2);
                if ((stackTop - 1)->type != SCRIPT_VAR_STRING) {
                    script_errorParameterIndex = 1;
                    Scr_Error(
                        "first parameter of notify must evaluate to a string");
                }

                uint16_t notifyName =
                    (uint16_t)(stackTop - 1)->payload;
                VM_Notify(object, notifyName, stackTop - 2);
                while (stackTop->type != SCRIPT_VAR_CODEPOS) {
                    RemoveRefToValue(stackTop);
                    --stackTop;
                }
                --stackTop;
                break;
            }

            case SCRIPT_OP_ENDON: {
                uint16_t object =
                    ScriptInterpreter_RequireObject(stackTop, 1);
                if ((stackTop - 1)->type != SCRIPT_VAR_STRING) {
                    Scr_Error(
                        "first parameter of notify must evaluate to a string");
                }

                uint16_t notifyName =
                    (uint16_t)(stackTop - 1)->payload;
                AddRefToObject(currentObject);
                uint16_t endonThread =
                    AllocThread(currentObject);

                uint16_t waitSlot =
                    GetVariable(object,
                                            SCRIPT_INTERPRETER_ENDON_NOTIFY_NAME);
                uint16_t waitRoot =
                    GetArray(waitSlot);
                uint16_t notifySlot =
                    GetVariable(waitRoot, notifyName);
                uint16_t notifyBucket =
                    GetArray(notifySlot);
                GetObjectVariable(notifyBucket, endonThread);
                RemoveRefToObject(endonThread);

                VariableValue objectValue;
                objectValue.type = SCRIPT_VAR_OBJECT;
                objectValue.payload = object;

                uint16_t pauseSlot =
                    GetObjectVariable(script_pauseArrayHandle,
                                                  currentObject);
                uint16_t pauseBucket =
                    GetArray(pauseSlot);
                uint16_t pauseThreadSlot =
                    GetObjectVariable(pauseBucket, endonThread);
                SetNewVariableValue(pauseThreadSlot, &objectValue);

                SetThreadNotifyName(endonThread, notifyName);
                SL_RemoveRefToString(notifyName);
                stackTop -= 2;
                break;
            }

            case SCRIPT_OP_PUSH_CODEPOS:
                stackTop = ScriptInterpreter_PushTypeOnly(
                    stackTop, SCRIPT_VAR_CODEPOS);
                break;

            case SCRIPT_OP_SWITCH_JUMP:
            {
                /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                uint8_t *tableBytes =
                    codePos + ScriptInterpreter_ReadI16(codePos);
                int32_t caseCount = ScriptInterpreter_ReadI16(
                    tableBytes - sizeof(int16_t));
                switchCaseCount = caseCount;
                uint32_t switchValue;
                script_switch_case_table_entry_t *table =
                    reinterpret_cast<script_switch_case_table_entry_t *>(
                        tableBytes);
                script_switch_case_table_entry_t *entry = table;
                codePos = tableBytes;

                if (stackTop->type == SCRIPT_VAR_STRING) {
                    switchValue = (uint32_t)stackTop->payload;
                    SL_RemoveRefToString(
                        (uint16_t)stackTop->payload);
                } else if (stackTop->type == SCRIPT_VAR_INT) {
                    if (IsValidArrayIndex((int32_t)stackTop->payload) ==
                        qfalse) {
                        Scr_Error(
                            va("switch index %d out of range",
                               (int32_t)stackTop->payload));
                    }
                    switchValue =
                        GetInternalVariableIndex(
                            (int32_t)stackTop->payload);
                } else {
                    Scr_Error(
                        va("cannot switch on %s",
                           script_variableTypeNames[stackTop->type]));
                }

                --stackTop;
                int32_t remaining = caseCount;
                qboolean selected = qfalse;
                while (remaining != 0) {
                    uint32_t caseValue = entry->value;

                    if (caseValue == switchValue) {
                        codePos = ScriptInterpreter_CodeposFromPayload(
                            entry->codePos);
                        selected = qtrue;
                        break;
                    }

                    entry =
                        reinterpret_cast<script_switch_case_table_entry_t *>(
                            reinterpret_cast<uintptr_t>(entry) +
                            sizeof(*entry));
                    codePos = reinterpret_cast<uint8_t *>(entry);
                    --remaining;
                }

                if (selected == qfalse && caseCount != 0) {
                    const script_switch_case_table_entry_t *lastEntry =
                        reinterpret_cast<
                            const script_switch_case_table_entry_t *>(
                            reinterpret_cast<uintptr_t>(entry) -
                            sizeof(*entry));
                    if (lastEntry->value == 0) {
                        codePos = ScriptInterpreter_CodeposFromPayload(
                            lastEntry->codePos);
                    }
                }
                break;
            }

            case SCRIPT_OP_SWITCH_TABLE:
                codePos +=
                    sizeof(int16_t) +
                    ScriptInterpreter_ReadI16(codePos) *
                        sizeof(script_switch_case_table_entry_t);
                break;

            case SCRIPT_OP_VECTOR:
                CastVector2(stackTop - 2);
                stackTop -= 2;
                break;

            case SCRIPT_OP_NOP:
                break;

            case SCRIPT_OP_DEVELOPER_COMMAND: {
                script_frameBackupCodepos[script_callStackDepth] = codePos;
                int32_t codeOffset = (int32_t)(codePos - script_codeBase) - 1;
                uint8_t *developerCode = script_developerOpcodePatchTable[codeOffset];
                /* Linux 0x080adaed..0x080adb37 agrees with Windows: load the
                 * table entry, save its first byte, then advance past it. */
                script_frameBackupOpcode[script_callStackDepth] =
                    *developerCode++;
                developerDepth++;
                codePos = developerCode;
                break;
            }

            case SCRIPT_OP_DEFERRED_DEVELOPER_CHECK:
                codePos = script_frameBackupCodepos[script_callStackDepth];
                opcode = script_frameBackupOpcode[script_callStackDepth];
                script_frameBackupCodepos[script_callStackDepth] = 0;
                developerDepth--;
                opcodeAlreadyLoaded = qtrue;
                break;
            }
        } catch (const ScriptErrorClass &) {
            if (script_forceErrorReport != 0) {
                RuntimeError(
                    codePos, script_errorParameterIndex, script_errorMessage,
                    script_errorSource);
            }
            ScriptInterpreter_CleanupAfterError(opcode, &stackTop, &codePos,
                                                &fieldRef,
                                                switchCaseCount);
            ScriptInterpreter_ReportAndContinue(codePos);
        }
    }
}
