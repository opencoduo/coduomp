#ifndef CODUO_CORE_MEMORY_PRIVATE_H
#define CODUO_CORE_MEMORY_PRIVATE_H

#include <stddef.h>
#include <stdint.h>

#include "server/standalone/bindings/coduo_engine_structs.h"
#include "qcommon/hunk.h"
#include "qcommon/q_memory.h"

void Com_Error(errorParm_t code, const char *format, ...);
void Com_Printf(const char *format, ...);
int32_t Sys_Milliseconds(void);
void *GetWeaponInfoMemory(int32_t bytes, int32_t *priorState, int32_t callerState);
void FreeWeaponInfoMemory(int32_t callerState, qboolean keepMemory);

extern uint8_t *weaponInfo_memory;
extern int32_t weaponInfo_memoryState;

#endif
