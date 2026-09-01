#ifndef CODUOMP_SYSTEM_PROCESS_LOCK_H
#define CODUOMP_SYSTEM_PROCESS_LOCK_H

#include "q_shared.h"

#if defined(_WIN32)
qboolean Sys_ProcessMatchesExecutable(uint32_t processId);
void Sys_InitProcessLockFile(void);
qboolean Sys_CheckProcessLock(void);
void Sys_DeleteProcessLockFile(void);
#endif

#endif
