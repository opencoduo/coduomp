#include "script_runtime_host.h"

#include "script_code_emit.h"
#include "script_compile_developer.h"
#include "script_source_positions.h"
#include "script_string.h"

#include <string.h>

enum {
    SCRIPT_CODEGEN_MODE_INTERN_STRINGS = 0,
    SCRIPT_CODEGEN_MODE_RECORD_STRING_FIXUPS = 1,
    SCRIPT_CODEGEN_MODE_RELEASE_STRINGS = 2,
    SCRIPT_STRING_USAGE_RUNTIME = 1,
    SCRIPT_CODE_OPCODE_HASH_MULTIPLIER = 31,
    SCRIPT_OPCODE_GET_UNDEFINED = 0x02,
    SCRIPT_OPCODE_GET_INTEGER = 0x03,
    SCRIPT_OPCODE_GET_FLOAT = 0x04,
    SCRIPT_OPCODE_GET_STRING = 0x05,
    SCRIPT_OPCODE_GET_ISTRING = 0x06,
    SCRIPT_OPCODE_GET_SELF_OBJECT = 0x07,
    SCRIPT_OPCODE_GET_LEVEL_OBJECT = 0x08,
    SCRIPT_OPCODE_GET_GAME_OBJECT = 0x09,
    SCRIPT_OPCODE_GET_SELF = 0x0a,
    SCRIPT_OPCODE_GET_LEVEL = 0x0b,
    SCRIPT_OPCODE_GET_ANIM = 0x0c,
    SCRIPT_OPCODE_GET_GAME = 0x0d,
    SCRIPT_OPCODE_GET_ANIMATION = 0x0e,
    SCRIPT_OPCODE_SET_ANIM_OBJECT = 0x0f,
    SCRIPT_OPCODE_GET_LOCAL = 0x11,
    SCRIPT_OPCODE_SET_LOCAL_REF = 0x12,
    SCRIPT_OPCODE_CLEAR_LOCAL = 0x13,
    SCRIPT_OPCODE_EVAL_INDEX = 0x14,
    SCRIPT_OPCODE_SET_INDEXED_REF = 0x15,
    SCRIPT_OPCODE_CLEAR_INDEXED = 0x16,
    SCRIPT_OPCODE_NEW_ARRAY = 0x17,
    SCRIPT_OPCODE_GET_FIELD = 0x18,
    SCRIPT_OPCODE_SET_FIELD_REF = 0x19,
    SCRIPT_OPCODE_CLEAR_FIELD = 0x1a,
    SCRIPT_OPCODE_STORE_TEMP = 0x1b,
    SCRIPT_OPCODE_SET_LOCAL = 0x1c,
    SCRIPT_OPCODE_SET_LOCAL_AND_CLEAR = 0x1d,
    SCRIPT_OPCODE_STORE_REF = 0x20,
    SCRIPT_OPCODE_DROP_TOP = 0x2d,
    SCRIPT_OPCODE_CAST_OBJECT = 0x2e,
    SCRIPT_OPCODE_CAST_BOOL = 0x2f,
    SCRIPT_OPCODE_CAST_INT = 0x30,
    SCRIPT_OPCODE_CAST_FLOAT = 0x31,
    SCRIPT_OPCODE_CAST_STRING = 0x32,
    SCRIPT_OPCODE_BOOL_NOT = 0x33,
    SCRIPT_OPCODE_BIT_NOT = 0x34,
    SCRIPT_OPCODE_NOP = 0x56
};

/* The authoritative emitter clusters agree on opcode values, stack deltas,
 * allocation widths, string ownership, source-position calls, and call order:
 * CoDUOMP.exe 0x00479ac0..0x0047b464 and coduo_lnxded
 * 0x0809d2f8..0x0809dee1.  The three Linux field emitters at
 * 0x0809dd26, 0x0809dd74, and 0x0809ddb6 call the inner object dispatcher at
 * 0x0809fa00, matching the Windows/Mac EmitExpressionFieldObject identity;
 * the former Linux recovery had exchanged that name with its primitive-node
 * wrapper. */

/* Function identities below follow the same-module Mac traceback symbols
 * where available. */

/* Source: CoDUOMP.exe 0x00479c10..0x00479caa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479c10_00479caa.mcode. */
void EmitOpcode(uint8_t opcode, int32_t stackDelta,
                int32_t localDepthMode)
{
    if (script_codeNeedsDeferredCheck != qfalse &&
        script_codegenMode == SCRIPT_CODEGEN_MODE_INTERN_STRINGS) {
        script_codeNeedsDeferredCheck = qfalse;
        Scr_TransferStatementListToDeveloperBuffer();
    }

    script_codeStackDepth += stackDelta;
    if (script_codeMaxStackDepth < script_codeStackDepth) {
        script_codeMaxStackDepth = script_codeStackDepth;
    }

    if (localDepthMode != 0 &&
        script_codeMaxLocalDepth < script_codeStackDepth) {
        script_codeMaxLocalDepth = script_codeStackDepth;
        if (localDepthMode == 3) {
            script_codeMaxLocalDepth = script_codeStackDepth + 1;
        }
    }

    script_codeEmitCursor = TempMalloc(sizeof(opcode));
    script_codeLastOpcodePos = script_codeEmitCursor;
    *script_codeEmitCursor = opcode;
    script_codeChecksum =
        script_codeChecksum * SCRIPT_CODE_OPCODE_HASH_MULTIPLIER + opcode;
}

/* Source: CoDUOMP.exe 0x00479ac0..0x00479b07.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479ac0_00479b07.mcode. */
void CompileRemoveRefToString(uint16_t string)
{
    if (script_codeOwnsStrings == qfalse) {
        SL_RemoveRefToString(string);
    }
}

/* Source: CoDUOMP.exe 0x00479b10..0x00479bc7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479b10_00479bc7.mcode. */
void EmitCanonicalString(uint16_t string)
{
    script_codeEmitCursor = TempMalloc(sizeof(uint16_t));
    char *out = (char *)script_codeEmitCursor;

    if (script_codegenMode == SCRIPT_CODEGEN_MODE_RELEASE_STRINGS) {
        CompileRemoveRefToString(string);
        return;
    }

    if (script_codeOwnsStrings != qfalse) {
        SL_AddRefToString(string);
    }

    if (script_codegenMode == SCRIPT_CODEGEN_MODE_INTERN_STRINGS) {
        const uint16_t canonical = SL_TransferToCanonicalString(string);
        memcpy(out, &canonical, sizeof(canonical));
        return;
    }

    memcpy(out, &string, sizeof(string));
    script_code_string_fixup_t *fixup = Z_MallocInternal(sizeof(*fixup));
    fixup->codePos =
        (char *)(script_codeEmitCursor +
                 (script_codeRelocationEnd - script_codeRelocationStart));
    const uint16_t emptyString = 0;
    memcpy(fixup->codePos, &emptyString, sizeof(emptyString));
    fixup->next = script_codeStringFixups;
    script_codeStringFixups = fixup;
}

/* Source: CoDUOMP.exe 0x00479bd0..0x00479c0f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479bd0_00479c0f.mcode. */
void CompileTransferRefToString(uint16_t string, uint8_t usage)
{
    if (script_codegenMode == SCRIPT_CODEGEN_MODE_RELEASE_STRINGS) {
        CompileRemoveRefToString(string);
        return;
    }

    if (script_codeOwnsStrings != qfalse) {
        SL_AddRefToString(string);
    }

    SL_TransferRefToString(string, usage);
}

/* Source: CoDUOMP.exe 0x00479cb0..0x00479d18.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479cb0_00479d18.mcode. */
void EmitNOP(void)
{
    EmitOpcode(SCRIPT_OPCODE_NOP, 0, 0);
}

/* Source: CoDUOMP.exe 0x00479d20..0x00479d48.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479d20_00479d48.mcode. */
void EmitInteger(int32_t value)
{
    char *out = (char *)TempMalloc(sizeof(value));
    script_codeEmitCursor = (uint8_t *)out;
    memcpy(out, &value, sizeof(value));
}

/* Source: CoDUOMP.exe 0x00479d50..0x00479d7a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479d50_00479d7a.mcode. */
void EmitShort(int16_t value)
{
    char *out = (char *)TempMalloc(sizeof(value));
    script_codeEmitCursor = (uint8_t *)out;
    memcpy(out, &value, sizeof(value));
}

/* Source: CoDUOMP.exe 0x00479d80..0x00479da6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479d80_00479da6.mcode. */
void EmitByte(uint8_t value)
{
    uint8_t *out = TempMalloc(sizeof(*out));
    script_codeEmitCursor = out;
    *out = value;
}

/* Source: CoDUOMP.exe 0x00479db0..0x00479dd8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479db0_00479dd8.mcode.
 * Same-module Mac name/signature: EmitFloat(float). */
void EmitFloat(float value)
{
    char *out = (char *)TempMalloc(sizeof(value));
    script_codeEmitCursor = (uint8_t *)out;
    memcpy(out, &value, sizeof(value));
}

/* Source: CoDUOMP.exe 0x00479de0..0x00479e0a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479de0_00479e0a.mcode. */
void EmitString(uint16_t value)
{
    char *out = (char *)TempMalloc(sizeof(value));
    script_codeEmitCursor = (uint8_t *)out;
    memcpy(out, &value, sizeof(value));
}

/* Source: CoDUOMP.exe 0x00479e10..0x00479e38.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479e10_00479e38.mcode. */
void EmitBlankPos(void)
{
    const uint32_t value = 0;
    char *out = (char *)TempMalloc(sizeof(value));
    script_codeEmitCursor = (uint8_t *)out;
    memcpy(out, &value, sizeof(value));
}

/* Source: CoDUOMP.exe 0x00479e40..0x00479e68.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479e40_00479e68.mcode.
 * Same-module Mac name: EmitCodepos. The fixed-dword carrier is the animation
 * reference encoded in the bytecode, not a native host pointer. */
void EmitCodepos(uint32_t value)
{
    char *out = (char *)TempMalloc(sizeof(value));
    script_codeEmitCursor = (uint8_t *)out;
    memcpy(out, &value, sizeof(value));
}

/* NOT_FROM_ORIGINAL_SOURCE: the i386 binary's value payload is one dword;
 * native reconstruction keeps script value payloads at host pointer width. */
void coduomp_script_emit_value_payload(
    coduo_script_value_payload_t value)
{
    char *out = (char *)TempMalloc(sizeof(value));
    script_codeEmitCursor = (uint8_t *)out;
    memcpy(out, &value, sizeof(value));
}

/* Source: CoDUOMP.exe 0x00479e70..0x00479e98.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479e70_00479e98.mcode.
 * Name/signature: exact same-module Mac symbol EmitBuiltinFunction. The
 * original i386 callback is four bytes; native bytecode naturally carries the
 * host callback width consumed by ScriptInterpreter_ReadBuiltinFunction. */
void EmitBuiltinFunction(script_function_callback_t function)
{
    char *out = (char *)TempMalloc(sizeof(function));
    script_codeEmitCursor = (uint8_t *)out;
    memcpy(out, &function, sizeof(function));
}

/* Source: CoDUOMP.exe 0x00479ea0..0x00479ec8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479ea0_00479ec8.mcode.
 * Name/signature: exact same-module Mac symbol EmitBuiltinMethod. The original
 * i386 callback is four bytes; native bytecode naturally carries the host
 * callback width consumed by ScriptInterpreter_ReadBuiltinMethod. */
void EmitBuiltinMethod(script_method_callback_t method)
{
    char *out = (char *)TempMalloc(sizeof(method));
    script_codeEmitCursor = (uint8_t *)out;
    memcpy(out, &method, sizeof(method));
}

/* Source: CoDUOMP.exe 0x00479ed0..0x00479f40.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479ed0_00479f40.mcode. */
void EmitGetUndefined(void)
{
    EmitOpcode(SCRIPT_OPCODE_GET_UNDEFINED, 1, 0);
}

/* Source: CoDUOMP.exe 0x00479f40..0x00479fcf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479f40_00479fcf.mcode. */
void EmitGetInteger(int32_t value)
{
    EmitOpcode(SCRIPT_OPCODE_GET_INTEGER, 1, 0);
    EmitInteger(value);
}

/* Source: CoDUOMP.exe 0x00479fd0..0x0047a05f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479fd0_0047a05f.mcode.
 * Same-module Mac name/signature: EmitGetFloat(float). */
void EmitGetFloat(float value)
{
    EmitOpcode(SCRIPT_OPCODE_GET_FLOAT, 1, 0);
    EmitFloat(value);
}

/* NOT_FROM_ORIGINAL_SOURCE: retained raw-bit path for recovered AST literals.
 * It avoids a host floating-point argument boundary changing a NaN payload or
 * signed zero that the original i386 emitters copied as one dword. */
void coduomp_script_emit_float_bits(uint32_t value)
{
    char *out = (char *)TempMalloc(sizeof(value));
    script_codeEmitCursor = (uint8_t *)out;
    memcpy(out, &value, sizeof(value));
}

/* NOT_FROM_ORIGINAL_SOURCE: raw-bit counterpart of EmitGetFloat for recovered
 * AST literal storage. */
void coduomp_script_emit_get_float_bits(uint32_t value)
{
    EmitOpcode(SCRIPT_OPCODE_GET_FLOAT, 1, 0);
    coduomp_script_emit_float_bits(value);
}

/* Source: CoDUOMP.exe 0x0047a060..0x0047a0ef.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a060_0047a0ef.mcode. */
void EmitFalse(void)
{
    EmitOpcode(SCRIPT_OPCODE_GET_INTEGER, 1, 0);
    EmitInteger(0);
}

/* Source: CoDUOMP.exe 0x0047a0f0..0x0047a17f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a0f0_0047a17f.mcode. */
void EmitTrue(void)
{
    EmitOpcode(SCRIPT_OPCODE_GET_INTEGER, 1, 0);
    EmitInteger(1);
}

/* Source: CoDUOMP.exe 0x0047a180..0x0047a1d6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a180_0047a1d6.mcode. */
void EmitAnimTree(uint32_t sourcePos)
{
    if (script_activeAnimTreeHandle == 0) {
        CompileError(sourcePos, "#using_animtree was not specified");
        return;
    }

    EmitOpcode(SCRIPT_OPCODE_GET_INTEGER, 1, 0);
    EmitInteger(script_activeAnimTreeHandle);
}

/* Source: CoDUOMP.exe 0x0047a260..0x0047a2e2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a260_0047a2e2.mcode. */
void EmitSafeSetVariableField(uint16_t string, uint32_t sourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_SET_LOCAL, 0, 0);
    AddOpcodePos(sourcePos);
    EmitCanonicalString(string);
}

/* Source: CoDUOMP.exe 0x0047a2f0..0x0047a372.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a2f0_0047a372.mcode. */
void EmitSafeSetWaittillVariableField(uint16_t string, uint32_t sourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_SET_LOCAL_AND_CLEAR, 0, 0);
    AddOpcodePos(sourcePos);
    EmitCanonicalString(string);
}

/* Source: CoDUOMP.exe 0x0047a380..0x0047a44b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a380_0047a44b.mcode. */
void EmitGetString(uint16_t string)
{
    EmitOpcode(SCRIPT_OPCODE_GET_STRING, 1, 0);
    EmitString(string);
    CompileTransferRefToString(string, SCRIPT_STRING_USAGE_RUNTIME);
}

/* Source: CoDUOMP.exe 0x0047a450..0x0047a51b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a450_0047a51b.mcode. */
void EmitGetIString(uint16_t string)
{
    EmitOpcode(SCRIPT_OPCODE_GET_ISTRING, 1, 0);
    EmitString(string);
    CompileTransferRefToString(string, SCRIPT_STRING_USAGE_RUNTIME);
}

/* Source: CoDUOMP.exe 0x0047a1e0..0x0047a254.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a1e0_0047a254.mcode. */
void EmitSetVariableField(uint32_t sourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_STORE_REF, -1, 0);
    AddOpcodePos(sourcePos);
}

/* Source: CoDUOMP.exe 0x0047a520..0x0047a58c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a520_0047a58c.mcode. */
void EmitCastBool(uint32_t sourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_CAST_BOOL, 0, 0);
    AddOpcodePos(sourcePos);
}

/* Source: CoDUOMP.exe 0x0047a590..0x0047a5fc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a590_0047a5fc.mcode. */
void EmitCastInt(uint32_t sourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_CAST_INT, 0, 0);
    AddOpcodePos(sourcePos);
}

/* Source: CoDUOMP.exe 0x0047a600..0x0047a66c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a600_0047a66c.mcode. */
void EmitCastFloat(uint32_t sourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_CAST_FLOAT, 0, 0);
    AddOpcodePos(sourcePos);
}

/* Source: CoDUOMP.exe 0x0047a670..0x0047a6dc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a670_0047a6dc.mcode. */
void EmitCastString(uint32_t sourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_CAST_STRING, 0, 0);
    AddOpcodePos(sourcePos);
}

/* Source: CoDUOMP.exe 0x0047a6e0..0x0047a74c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a6e0_0047a74c.mcode. */
void EmitBoolNot(uint32_t sourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_BOOL_NOT, 0, 0);
    AddOpcodePos(sourcePos);
}

/* Source: CoDUOMP.exe 0x0047a750..0x0047a7bc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a750_0047a7bc.mcode. */
void EmitBoolComplement(uint32_t sourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_BIT_NOT, 0, 0);
    AddOpcodePos(sourcePos);
}

/* Source: CoDUOMP.exe 0x0047a850..0x0047a8c0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a850_0047a8c0.mcode. */
void EmitSelf(void)
{
    EmitOpcode(SCRIPT_OPCODE_GET_SELF, 1, 0);
}

/* Source: CoDUOMP.exe 0x0047a8c0..0x0047a930.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a8c0_0047a930.mcode. */
void EmitLevel(void)
{
    EmitOpcode(SCRIPT_OPCODE_GET_LEVEL, 1, 0);
}

/* Source: CoDUOMP.exe 0x0047a930..0x0047a9a0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a930_0047a9a0.mcode. */
void EmitAnim(void)
{
    EmitOpcode(SCRIPT_OPCODE_GET_ANIM, 1, 0);
}

/* Source: CoDUOMP.exe 0x0047a9a0..0x0047aa10.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047a9a0_0047aa10.mcode. */
void EmitGame(void)
{
    EmitOpcode(SCRIPT_OPCODE_GET_GAME, 1, 0);
}

/* Source: CoDUOMP.exe 0x0047aa10..0x0047aa78.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047aa10_0047aa78.mcode. */
void EmitSelfObject(void)
{
    EmitOpcode(SCRIPT_OPCODE_GET_SELF_OBJECT, 0, 0);
}

/* Source: CoDUOMP.exe 0x0047aa80..0x0047aae8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047aa80_0047aae8.mcode. */
void EmitLevelObject(void)
{
    EmitOpcode(SCRIPT_OPCODE_GET_LEVEL_OBJECT, 0, 0);
}

/* Source: CoDUOMP.exe 0x0047aaf0..0x0047ab58.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047aaf0_0047ab58.mcode. */
void EmitGameRef(void)
{
    EmitOpcode(SCRIPT_OPCODE_GET_GAME_OBJECT, 0, 0);
}

/* Source: CoDUOMP.exe 0x0047ac60..0x0047acc8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047ac60_0047acc8.mcode. */
void EmitAnimObject(void)
{
    EmitOpcode(SCRIPT_OPCODE_SET_ANIM_OBJECT, 0, 0);
}

/* Source: CoDUOMP.exe 0x0047ab60..0x0047abdd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047ab60_0047abdd.mcode. */
void EmitLocalVariable(uint16_t string)
{
    EmitOpcode(SCRIPT_OPCODE_GET_LOCAL, 1, 0);
    EmitCanonicalString(string);
}

/* Source: CoDUOMP.exe 0x0047abe0..0x0047ac55.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047abe0_0047ac55.mcode. */
void EmitLocalVariableRef(uint16_t string)
{
    EmitOpcode(SCRIPT_OPCODE_SET_LOCAL_REF, 0, 0);
    EmitCanonicalString(string);
}

/* Source: CoDUOMP.exe 0x0047acd0..0x0047ad45.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047acd0_0047ad45.mcode. */
void EmitClearLocalVariable(uint16_t string)
{
    EmitOpcode(SCRIPT_OPCODE_CLEAR_LOCAL, 0, 0);
    EmitCanonicalString(string);
}

/* Source: CoDUOMP.exe 0x0047ad50..0x0047add9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047ad50_0047add9.mcode. */
void EmitEvalArray(uint32_t firstSourcePos, uint32_t secondSourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_EVAL_INDEX, -1, 0);
    AddOpcodePos(secondSourcePos);
    AddOpcodePos(firstSourcePos);
}

/* Source: CoDUOMP.exe 0x0047ade0..0x0047ae69.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047ade0_0047ae69.mcode. */
void EmitEvalArrayRef(uint32_t firstSourcePos,
                      uint32_t secondSourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_SET_INDEXED_REF, -1, 0);
    AddOpcodePos(secondSourcePos);
    AddOpcodePos(firstSourcePos);
}

/* Source: CoDUOMP.exe 0x0047ae70..0x0047aef9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047ae70_0047aef9.mcode. */
void EmitClearArray(uint32_t firstSourcePos,
                    uint32_t secondSourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_CLEAR_INDEXED, -1, 0);
    AddOpcodePos(secondSourcePos);
    AddOpcodePos(firstSourcePos);
}

/* Source: CoDUOMP.exe 0x0047af00..0x0047af70.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047af00_0047af70.mcode. */
void EmitEmptyArray(void)
{
    EmitOpcode(SCRIPT_OPCODE_NEW_ARRAY, 1, 0);
}

/* Source: CoDUOMP.exe 0x0047af70..0x0047b087.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047af70_0047b087.mcode. */
void EmitAnimation(uint16_t string, uint32_t sourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_GET_ANIMATION, 1, 0);
    EmitCodepos(UINT32_MAX);
    AddOpcodePos(sourcePos);
    Scr_EmitAnimation((char *)script_codeEmitCursor, string, sourcePos);
    CompileRemoveRefToString(string);
}

/* Source: CoDUOMP.exe 0x0047b090..0x0047b12c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047b090_0047b12c.mcode. */
void EmitFieldVariable(scr_ast_node_t *objectNode,
                       uint16_t string, uint32_t sourcePos)
{
    EmitExpressionFieldObject(objectNode, sourcePos);
    EmitOpcode(SCRIPT_OPCODE_GET_FIELD, 1, 0);
    AddOpcodePos(sourcePos);
    EmitCanonicalString(string);
}

/* Source: CoDUOMP.exe 0x0047b130..0x0047b1b9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047b130_0047b1b9.mcode. */
void EmitFieldVariableRef(scr_ast_node_t *objectNode,
                          uint16_t string, uint32_t sourcePos)
{
    EmitExpressionFieldObject(objectNode, sourcePos);
    EmitOpcode(SCRIPT_OPCODE_SET_FIELD_REF, 0, 0);
    EmitCanonicalString(string);
}

/* Source: CoDUOMP.exe 0x0047b1c0..0x0047b256.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047b1c0_0047b256.mcode. */
void EmitClearFieldVariable(scr_ast_node_t *objectNode,
                            uint16_t string,
                            uint32_t objectSourcePos,
                            uint32_t opcodeSourcePos)
{
    EmitExpressionFieldObject(objectNode, objectSourcePos);
    EmitOpcode(SCRIPT_OPCODE_CLEAR_FIELD, 0, 0);
    AddOpcodePos(opcodeSourcePos);
    EmitCanonicalString(string);
}

/* Source: CoDUOMP.exe 0x0047b260..0x0047b2e8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047b260_0047b2e8.mcode. */
void EmitCallRef(scr_ast_node_t *nameNode,
                 scr_ast_list_t *argsNode,
                 uint32_t callSourcePos)
{
    /* The third sval_u parameter is part of the original source signature but
     * the Windows body at 0x0047b260 never reads it. */
    (void)callSourcePos;
    EmitCall(nameNode, argsNode, qfalse);
    EmitOpcode(SCRIPT_OPCODE_STORE_TEMP, -1, 0);
}

/* Source: CoDUOMP.exe 0x0047b2f0..0x0047b380.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047b2f0_0047b380.mcode. */
void EmitMethodRef(scr_ast_node_t *objectNode,
                   scr_ast_node_t *nameNode,
                   scr_ast_list_t *argsNode,
                   uint32_t sourcePos)
{
    EmitMethod(objectNode, nameNode, argsNode, sourcePos, qfalse);
    EmitOpcode(SCRIPT_OPCODE_STORE_TEMP, -1, 0);
}

/* Source: CoDUOMP.exe 0x0047b380..0x0047b3f0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047b380_0047b3f0.mcode. */
void EmitDecTop(void)
{
    EmitOpcode(SCRIPT_OPCODE_DROP_TOP, -1, 0);
}

/* Source: CoDUOMP.exe 0x0047b3f0..0x0047b464.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047b3f0_0047b464.mcode. */
void EmitCastFieldObject(uint32_t sourcePos)
{
    EmitOpcode(SCRIPT_OPCODE_CAST_OBJECT, -1, 0);
    AddOpcodePos(sourcePos);
}
