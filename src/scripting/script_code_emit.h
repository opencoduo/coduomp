#ifndef SHARED_SCRIPT_CODE_EMIT_H
#define SHARED_SCRIPT_CODE_EMIT_H

#include "script_compile_types.h"
#include "qcommon/script_runtime_types.h"
#include "qcommon/script_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int32_t script_codegenMode;
extern int32_t script_codeStackDepth;
extern int32_t script_codeMaxStackDepth;
extern int32_t script_codeMaxLocalDepth;
extern uint8_t *script_codeEmitCursor;
extern uint32_t script_codeChecksum;
extern script_code_string_fixup_t *script_codeStringFixups;

/*
 * Original compiler-state bytes.  CoDUOMP.exe accesses 0x009d65ce and
 * 0x009d65cf exclusively with byte loads/stores; coduo_lnxded retains the
 * same one-byte objects.  Earlier client declarations as qboolean were a
 * reconstruction transcription error.
 */
extern uint8_t script_codeNeedsDeferredCheck;
extern uint8_t script_codeOwnsStrings;

void CompileRemoveRefToString(uint16_t string);
void CompileTransferRefToString(uint16_t string, uint8_t usage);
void EmitCanonicalString(uint16_t string);
void EmitOpcode(uint8_t opcode, int32_t stackDelta,
                int32_t localDepthMode);
void EmitNOP(void);
void EmitInteger(int32_t value);
void EmitShort(int16_t value);
void EmitByte(uint8_t value);
void EmitFloat(float value);
void EmitString(uint16_t value);
void EmitBlankPos(void);
void EmitCodepos(uint32_t i386CodePos);
void EmitBuiltinFunction(script_function_callback_t function);
void EmitBuiltinMethod(script_method_callback_t method);

/* Native reconstruction adapters for operands whose original i386 carrier is
 * narrower than the maintained host representation. */
void coduomp_script_emit_value_payload(
    coduo_script_value_payload_t value);
void coduomp_script_emit_float_bits(uint32_t value);

void EmitGetUndefined(void);
void EmitGetInteger(int32_t value);
void EmitGetFloat(float value);
void coduomp_script_emit_get_float_bits(uint32_t value);
void EmitFalse(void);
void EmitTrue(void);
void EmitAnimTree(uint32_t sourcePos);
void EmitGetString(uint16_t string);
void EmitGetIString(uint16_t string);
void EmitSafeSetWaittillVariableField(uint16_t string, uint32_t sourcePos);
void EmitSafeSetVariableField(uint16_t string, uint32_t sourcePos);
void EmitSetVariableField(uint32_t sourcePos);

void EmitCastBool(uint32_t sourcePos);
void EmitCastInt(uint32_t sourcePos);
void EmitCastFloat(uint32_t sourcePos);
void EmitCastString(uint32_t sourcePos);
void EmitBoolNot(uint32_t sourcePos);
void EmitBoolComplement(uint32_t sourcePos);
void EmitSelf(void);
void EmitLevel(void);
void EmitAnim(void);
void EmitGame(void);
void EmitSelfObject(void);
void EmitLevelObject(void);
void EmitGameRef(void);
void EmitAnimObject(void);
void EmitLocalVariable(uint16_t string);
void EmitLocalVariableRef(uint16_t string);
void EmitClearLocalVariable(uint16_t string);
void EmitEvalArray(uint32_t firstSourcePos, uint32_t secondSourcePos);
void EmitEvalArrayRef(uint32_t firstSourcePos,
                      uint32_t secondSourcePos);
void EmitClearArray(uint32_t firstSourcePos,
                    uint32_t secondSourcePos);
void EmitEmptyArray(void);
void EmitAnimation(uint16_t string, uint32_t sourcePos);
void EmitFieldVariable(scr_ast_node_t *objectNode,
                       uint16_t string, uint32_t sourcePos);
void EmitFieldVariableRef(scr_ast_node_t *objectNode,
                          uint16_t string, uint32_t sourcePos);
void EmitClearFieldVariable(scr_ast_node_t *objectNode,
                            uint16_t string,
                            uint32_t objectSourcePos,
                            uint32_t opcodeSourcePos);
void EmitCallRef(scr_ast_node_t *nameNode,
                 scr_ast_list_t *argsNode,
                 uint32_t callSourcePos);
void EmitMethodRef(scr_ast_node_t *objectNode,
                   scr_ast_node_t *nameNode,
                   scr_ast_list_t *argsNode,
                   uint32_t sourcePos);
void EmitDecTop(void);
void EmitCastFieldObject(uint32_t sourcePos);

#ifdef __cplusplus
}
#endif

#endif
