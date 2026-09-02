#ifndef SHARED_ANIMATION_PRIVATE_H
#define SHARED_ANIMATION_PRIVATE_H

#include "dobj.h"
#include "qcommon/q_memory.h"
#include "xanim.h"
#include "xmodel.h"

#include <stddef.h>

uint16_t SL_FindLowercaseString(const char *text);
uint16_t SL_FindStringOfLen(const char *text, size_t size);
uint16_t SL_GetString_(const char *text, int32_t user, int32_t type);
const char *SL_ConvertToString(uint16_t string);
void SL_RemoveRefToString(uint16_t string);
void SL_RemoveRefToStringOfLen(uint16_t string, uint32_t size);
void SL_AddRefToString(uint16_t string);
void Com_Printf(const char *format, ...);
void Com_Error(int32_t level, const char *format, ...);
fileData_t *FS_GetDataForFile(const char *base, const char *path, const char *extension);
void xmodel_compat_optimize_loaded_surfs(XModelSurfsData *surfs, xmodel_asset_alloc_fn alloc);

extern int32_t xanim_poolHighWaterCount;
extern int32_t xanim_poolUsedCount;
extern XAnimInfo xanim_pool[XANIM_POOL_NODE_COUNT];
extern uint16_t xanim_endNotifyHandle;
extern XAnimTree *xanim_currentTree;
extern float xanim_evalCurrentTime;
extern int16_t xanim_evalCurrentFrame;
extern uint16_t xanim_evalRootHandle;
extern int16_t xanim_evalStartFrame;
extern float xanim_evalStartTime;
extern float xanim_evalTimeStep;
extern float xanim_evalTime;
extern int16_t xanim_evalWindowFrame;
extern float xanim_evalWindowTime;
extern int32_t xanim_deferredNotifyCount;
extern xanim_deferred_notify_t xanim_deferredNotifies[XANIM_DEFERRED_NOTIFY_CAPACITY];
extern int32_t xanim_evalPartCount;
extern uint32_t xanim_evalPartBits[DOBJ_PART_BITSET_WORD_COUNT];
extern uint32_t xanim_evalSkipBits[DOBJ_PART_BITSET_WORD_COUNT];
extern uint8_t xanim_evalLeafOutputMode;
extern int32_t xanim_evalPoolWeightSelector;
extern DObj *xanim_currentEvalState;
extern int32_t xanim_evalChildCount;
extern XModel **xanim_evalChildRefs;
extern size_t xanim_evalPartBytes;
extern uint16_t xanim_rootTreeHandle;
extern uint16_t xanimDefaultPartRemapHandle;

int32_t FS_ReadFile(const char *path, void **buffer);
void FS_FreeFile(void *buffer);
uint16_t SL_GetStringOfLen(const char *text, uint8_t user, size_t size, int32_t type);
uint16_t SL_GetLowercaseString_(const char *text, int32_t user, int32_t type);
uint16_t Scr_AllocArray(void);
uint16_t FindNextSibling(uint16_t handle);
uint32_t GetVariableName(uint16_t handle);
uint16_t GetVariable(uint16_t parent, uint32_t name);
void RemoveRefToObject(uint16_t handle);

void WriteByte(uint8_t value);
void WriteShort(uint16_t value);
void WriteString(uint16_t string);
void WriteFloat(float value);
void ScriptSave_WriteData(const void *data, size_t size);
uint8_t ReadByte(void);
uint16_t ReadShort(void);
uint16_t ReadString(void);
#if defined(WINDOWS_BEHAVIOR)
float ReadFloat(void);
#else
long double ReadFloat(void);
#endif
void ScriptLoad_ReadData(void *data, size_t size);
void Scr_AddConstString(uint16_t value);
void Scr_NotifyId(uint16_t objectHandle, uint16_t notifyName, uint32_t argumentCount);

#endif
