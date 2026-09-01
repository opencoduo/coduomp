#ifndef SCRIPT_SERIALIZATION_H
#define SCRIPT_SERIALIZATION_H

#include "script_value.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint16_t *script_variableToObjectId;
extern uint16_t *script_objectIdToVariable;
extern uint16_t script_savedObjectCount;
extern uint8_t *script_serializationCursor;

extern uint16_t script_levelHandle;
extern uint16_t script_gameHandle;
/* Variable whose value is the script language's special `anim` array. */
extern uint16_t script_animArrayHandle;
extern uint16_t script_pauseArrayHandle;

void WriteByte(uint8_t value);
uint8_t ReadByte(void);
void WriteShort(uint16_t value);
uint16_t ReadShort(void);
void WriteString(uint16_t string);
uint16_t ReadString(void);
void WriteFloat(float value);
float ReadFloat(void);

void ScriptSave_WriteOptionalString(uint16_t string);
uint16_t ScriptLoad_ReadOptionalString(void);
void ScriptSave_WriteVector(const vec3_t vector);
uintptr_t ScriptLoad_ReadVector(void);
void ScriptSave_WriteInt(int32_t value);
int32_t ScriptLoad_ReadInt(void);
void ScriptSave_WriteCodepos(const uint8_t *codePos);
script_codepos_t ScriptLoad_ReadCodepos(void);
void ScriptSave_WriteObject(uint16_t object);
uint16_t ScriptLoad_ReadObject(void);
void ScriptSave_WriteData(const void *data, size_t size);
void ScriptLoad_ReadData(void *data, size_t size);
void ScriptSave_WriteStack(VariableStackBuffer *frame);
VariableStackBuffer *ScriptLoad_ReadStack(void);

void ScriptSave_PrepareStack(VariableStackBuffer *frame);
void ScriptSave_PrepareValue(script_variable_type_t type,
                             coduo_script_value_payload_t payload);
void ScriptSave_PrepareValueObjectRefs(VariableValue *value);
void ScriptSave_WriteValue(script_variable_type_t type,
                           coduo_script_value_payload_t payload);
void ScriptSave_WriteChildValue(const VariableValue *value, uint32_t name,
                                qboolean parentIsArray);
void ScriptLoad_ReadValue(VariableValue *value);
uint32_t ScriptLoad_ReadChildValue(VariableValue *value);
void ScriptSave_PrepareObject(uint16_t object);
void ScriptSave_WriteObjectRecord(uint16_t object);
void ScriptLoad_ReadObjectRecord(uint16_t object);
void ScriptSave_WriteRootValue(void);
void ScriptLoad_ReadRootValue(void);

void Scr_SavePre(void);
void Scr_SavePost(void);
void Scr_SaveShutdown(void);
void Scr_LoadPre(void);
void Scr_LoadShutdown(void);
uint16_t Scr_ConvertThreadToSave(uint16_t thread);
uint16_t Scr_ConvertThreadFromLoad(uint16_t objectId);
void *CODUO_SCRIPT_CDECL Scr_LoadRead(uint32_t size);

#ifdef __cplusplus
}
#endif

#endif
