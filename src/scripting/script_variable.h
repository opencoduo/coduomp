#ifndef SCRIPT_VARIABLE_H
#define SCRIPT_VARIABLE_H

#include "qcommon/script_runtime_types.h"
#include "qcommon/script_types.h"
#include "script_value.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t script_runtimeActive;
extern uint16_t script_entityTypeClassMapRoot;
extern uint16_t script_classMapRoot;
extern script_class_map_entry_t *script_entityTypeUsageRecords;
extern uint32_t script_entityTypeUsageCount;
extern script_variable_node_t script_variableNodes[SCRIPT_VARIABLE_NODE_COUNT];
extern Variable script_variableIndirections[SCRIPT_VARIABLE_NODE_COUNT];
extern const char *script_variableTypeNames[SCRIPT_VAR_COUNT];

void InitVariables(void);
void Var_Init(void);

uint32_t GetVariableKeyObject(uint16_t handle);
uint32_t GetEntityType(uint16_t handle);
uint32_t GetVariableName(uint16_t handle);
uint32_t GetInternalVariableIndex(uint32_t name);
qboolean IsValidArrayIndex(uint32_t name);

uint16_t FindVariableIndexInternal(uint16_t parent, uint32_t name);
uint16_t GetVariableIndexInternal(uint16_t parent, uint32_t name);
uint16_t FindArrayVariableIndex(uint16_t parent, uint32_t name);
uint16_t GetArrayVariableIndex(uint16_t parent, uint32_t name);
uint16_t FindVariable(uint16_t parent, uint32_t name);
uint16_t GetVariable(uint16_t parent, uint32_t name);
uint16_t FindObjectVariable(uint16_t parent, uint16_t object);
uint16_t GetObjectVariable(uint16_t parent, uint16_t object);
uint16_t FindArrayVariable(uint16_t parent, int32_t name);
uint16_t GetArrayVariable(uint16_t parent, int32_t name);
uint16_t GetArrayVariableUnsigned(uint16_t parent, uint32_t name);
uint16_t GetVariableField(uint16_t parent, uint16_t name);

void ClearVariableField(uint16_t parent, uint16_t name);
void RemoveVariable(uint16_t parent, uint32_t name);
void RemoveObjectVariable(uint16_t parent, uint16_t object);
void RemoveArrayVariable(uint16_t parent, int32_t entityNum);
void SafeRemoveVariable(uint16_t parent, uint32_t name);
void SafeRemoveArrayVariable(uint16_t parent, int32_t entityNum);

script_variable_node_t *AllocVariable(void);
uint16_t AllocValue(void);
uint16_t AllocObject(void);
uint16_t AllocEntity(int32_t classNum, uint16_t entityNum);
uint16_t AllocThread(uint16_t parent);
uint16_t Scr_AllocArray(void);
void FreeVariable(uint16_t handle);
void FreeValueInternal(script_variable_node_t *node);
void FreeValue(uint16_t handle);

void MakeVariableExternal(Variable *slot, script_variable_node_t *parentNode);
void ClearObjectInternal(uint16_t object);
void ClearObject(uint16_t object);
void ClearVariableValue(uint16_t handle);
void AddRefToObject(uint16_t object);
void RemoveRefToObject(uint16_t object);
void Scr_FreeValue(uint16_t handle);

uint16_t GetThreadNotifyName(uint16_t thread);
void SetThreadNotifyName(uint16_t thread, uint16_t name);
void ClearThreadNotifyName(uint16_t thread);
qboolean Scr_IsThreadAlive(uint16_t handle);

uint16_t GetSelf(uint16_t object);
uint16_t GetEntnum(uint16_t handle);
script_variable_type_t GetVarType(uint16_t handle);
uint16_t GetArraySize(uint16_t handle);
uint16_t FindNextSibling(uint16_t handle);
/* windows.h aliases the unrelated GDI GetObject entry point to GetObjectA.
 * Keep that platform macro from rewriting the original script API name. */
#if defined(_WIN32) && defined(GetObject)
#undef GetObject
#endif
uint16_t GetObject(uint16_t handle);
uint16_t GetArray(uint16_t handle);
uint16_t FindObject(uint16_t handle);
qboolean IsFieldObject(uint16_t handle);

VariableValue *GetVariableValueAddress(uint16_t handle);
void GetVariableValue(uint16_t handle, VariableValue *value);
void GetVariableFieldValue(uint16_t handle, VariableValue *value);
void SetVariableValue(uint16_t handle, const VariableValue *value);
void SetNewVariableValue(uint16_t handle, const VariableValue *value);
void SetVariableFieldValue(uint16_t handle, VariableValue *value);
void GetEmptyArray(VariableValue *value);
void SetEmptyArray(uint16_t handle);

void Scr_FreeEntityNum(int32_t entityNum, int32_t classNum);
void Scr_SetClassMap(script_class_map_entry_t *records, uint32_t count);
void Scr_RemoveClassMap(void);
void Scr_AddClassField(uint16_t classRoot, const char *name, uint16_t offset);
uint32_t Scr_GetOffset(uint16_t classRoot, const char *name);
uint16_t Scr_GetEntityId(int32_t entityNum, int32_t classNum);
uint16_t FindEntityId(int32_t entityNum, int32_t classNum);
void CopyEntity(uint16_t source, uint16_t dest);
void CopyArray(uint16_t source, uint16_t dest);
void Scr_CopyEntityNum(int32_t sourceEntityNum, int32_t destEntityNum, int32_t classNum);

#ifdef __cplusplus
}
#endif

#endif
