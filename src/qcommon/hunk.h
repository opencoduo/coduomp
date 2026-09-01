#ifndef QCOMMON_HUNK_H
#define QCOMMON_HUNK_H

#include "hunk_types.h"
#include "q_shared_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern hunk_state_t hunk;
extern uint8_t *hunk_data;
extern uint8_t *hunk_allocData;
extern int32_t hunk_used;

void *Hunk_AllocAlignInternal(size_t size, size_t alignment);
void *Hunk_AllocLowAlignInternal(size_t size, size_t alignment);
void *Hunk_AllocInternal(size_t size);
void *Hunk_AllocLowInternal(size_t size);
void *Hunk_AllocXAnimCreateTree(size_t size);
void *Hunk_AllocXModelPrecache(size_t size);
void *Hunk_AllocXModelPrecacheMesh(size_t size);
void *Hunk_AllocateTempMemoryHighInternal(size_t size);
void *Hunk_AllocateTempMemoryInternal(size_t size);
void *Hunk_ReallocateTempMemory(size_t size);
void Hunk_ClearTempMemoryHigh(void);
void Hunk_CommitTempMemory(void);
void Hunk_FreeTempMemory(void *memory);
void Hunk_ClearTempMemory(void);
size_t Hunk_ConvertTempToPermLowInternal(void);
void Hunk_SetLowUsedInternal(size_t lowUsed);

size_t Hunk_MemoryRemaining(void);
void Hunk_ClearData(void);
void Hunk_SetMark2(void);
void Hunk_SetHighTempMark(void);
qboolean Hunk_CheckHighMark(const void *pointer);
void Hunk_ClearToMark2(void);
void Hunk_ClearToHighTempMark(void);
qboolean Hunk_HighMarkIsSet(void);
void Hunk_SetMarkLow(void);
void Hunk_ClearToMarkLow(void);
void Hunk_Clear(void);
void Hunk_Shutdown(void);

void Hunk_ClearToStart(void);
void Hunk_SetMarkTemp(void);
void Hunk_ClearToMarkTemp(void);
void Com_InitHunkMemory(void);
void Com_InitZoneMemory(void);
void Com_Meminfo_f(void);
void Com_TouchMemory(void);
void Hunk_Log(void);
void Hunk_SmallLog(void);

#ifdef __cplusplus
}
#endif

#endif
