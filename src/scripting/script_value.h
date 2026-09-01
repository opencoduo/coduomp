#ifndef SCRIPT_VALUE_H
#define SCRIPT_VALUE_H

#include "qcommon/q_vector_types.h"
#include "qcommon/script_runtime_types.h"
#include "qcommon/script_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t *script_vectorLocalPoolBase;
extern uint32_t script_parameterCount;
extern VariableValue script_valueStack[SCRIPT_VALUE_STACK_COUNT];
extern VariableValue *script_valueStackTop;
extern VariableValue *script_valueStackLimit;
extern uint32_t script_valueStackDepth;
extern const char *script_errorMessage;
extern const char *script_errorSource;
extern int32_t script_errorParameterIndex;
extern uint8_t script_forceErrorReport;
extern script_variable_type_t script_coerceLeftType;
extern script_variable_type_t script_coerceRightType;
extern uint16_t script_tempValueHandle;
extern uint16_t script_timeArrayHandle;
extern uint32_t script_currentTimeKey;
extern uint32_t script_loopWatchdogTick;
extern uint8_t *script_codeBase;

float *AllocVector(void);
float *AllocVectorCopy(const vec3_t value);
void AddRefToVector(const float *vector);
void RemoveRefToVector(const float *vector);

void AddRefToValueOfType(script_variable_type_t type, VariableUnion value);
void AddRefToValue(const VariableValue *value);
void RemoveRefToValueOfType(script_variable_type_t type,
                            VariableUnion value);
void RemoveRefToValue(VariableValue *value);

uint16_t VM_ConcatenateStrings(const VariableValue values[2]);
void GetSizeValue(VariableValue *value);
uint16_t CastFieldObject(VariableValue *value);
void EvalArray(VariableValue *index, VariableValue *container);
uint16_t FindArrayVariableByValue(uint16_t handle,
                                  VariableValue values[2]);
uint16_t EvalArrayRef(uint16_t handle, VariableValue values[2]);
void ClearArray(uint16_t handle, VariableValue values[2]);

void CastVector2(VariableValue values[3]);
void ClearVector(VariableValue values[3]);
void UnmatchingTypesError(VariableValue *left,
                                        VariableValue *right);
qboolean CastWeakerPair(VariableValue *left,
                                     VariableValue *right);
qboolean CastWeakerPairValues(VariableValue values[2]);
qboolean CheckEquality(VariableValue *left,
                                  VariableValue *right);
qboolean ScriptRuntime_StringStartsWithZeroLiteral(const char *text);
qboolean CastBool(VariableValue *value);
qboolean CastInt(VariableValue *value);
qboolean CastFloat(VariableValue *value);
qboolean CastString(VariableValue *value);
qboolean CastIString(VariableValue *value);
qboolean CastVector(VariableValue *value);
qboolean CastPointer(VariableValue *value);

qboolean Scr_GetBool(uint32_t index);
int32_t Scr_GetInt(uint32_t index);
float Scr_GetFloat(uint32_t index);
const char *Scr_GetString(uint32_t index);
uint16_t Scr_GetConstString(uint32_t index);
const char *Scr_GetDebugString(uint32_t index);
const char *Scr_GetIString(uint32_t index);
uint16_t Scr_GetConstIString(uint32_t index);
void Scr_GetVector(uint32_t index, vec3_t vector);
uint32_t Scr_GetFunc(uint32_t index);
#if defined(WINDOWS_BEHAVIOR)
uint16_t Scr_GetEntityNum(uint32_t index, int32_t *classNum);
#else
uint32_t Scr_GetEntityNum(uint32_t index, int32_t *classNum);
#endif
script_variable_type_t Scr_GetType(uint32_t index);
script_variable_type_t Scr_GetPointerType(uint32_t index);

void IncInParam(void);
void Scr_AddBool(qboolean value);
void Scr_AddInt(int32_t value);
void Scr_AddFloat(float value);
void Scr_AddAnim(uint32_t value);
void Scr_AddUndefined(void);
void Scr_AddObject(uint16_t object);
void Scr_AddEntityNum(int32_t entityNum, int32_t classNum);
void Scr_AddStruct(void);
void Scr_AddString(const char *value);
void Scr_AddIString(const char *value);
void Scr_AddConstString(uint16_t value);
void Scr_AddVector(const vec3_t vector);
void Scr_MakeArray(void);
void Scr_AddArray(void);
void Scr_AddArrayStringIndexed(uint16_t name);

scr_anim_t Scr_GetAnim(uint32_t index, XAnimTree *runtimeTree);
#if defined(WINDOWS_BEHAVIOR)
XAnim *Scr_GetAnimTree(uint32_t index);
#else
script_anim_tree_ref_t Scr_GetAnimTree(uint32_t index);
#endif
uint32_t CODUO_SCRIPT_CDECL Scr_GetAnimsIndex(XAnim *tree);
XAnim *Scr_GetAnims(uint32_t treeIndex);

void Scr_SetTime(uint32_t time);
void Scr_SetDynamicEntityField(int32_t entityNum, int32_t classNum,
                               uint16_t fieldName);
void Scr_Error(const char *message);
void Scr_TerminalError(const char *message);
void Scr_ErrorWithDialogMessage(const char *message, const char *source);
void Scr_ParamError(int32_t index, const char *message);
void Scr_ObjectError(const char *message);
#if defined(WINDOWS_BEHAVIOR)
qboolean Scr_IsSystemActive(qboolean unused);
#else
qboolean Scr_IsSystemActive(uint8_t unused);
#endif
uint32_t Scr_GetNumParam(void);
void Scr_RunCurrentThreads(void);
void Scr_ResetTimeout(void);

#ifdef __cplusplus
}
#endif

#endif
