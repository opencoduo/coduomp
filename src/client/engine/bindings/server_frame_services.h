#ifndef CODUOMP_SERVER_FRAME_SERVICES_H
#define CODUOMP_SERVER_FRAME_SERVICES_H

#include "client/engine/client/debug_lines.h"

#include <stdint.h>

uint32_t Sys_Milliseconds(void);

/* CoDUOMP.exe 0x0046356f flushes client-owned debug geometry immediately
 * before each server game frame.  This client presentation edge is absent
 * from the dedicated engine. */
#define SERVER_FRAME_PRE_GAME_VM() CL_FlushDebugData(qtrue)
#define SERVER_FRAME_MILLISECONDS() ((int32_t)Sys_Milliseconds())

#endif
