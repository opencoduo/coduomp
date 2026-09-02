#ifndef CODUOMP_SYSTEM_INFO_H
#define CODUOMP_SYSTEM_INFO_H

#include "q_shared.h"

typedef struct sys_info_s {
    double cpuFrequencyMHz;
    int32_t physicalMemoryMB;
    int32_t videoMemoryMB;
} sys_info_t;

void Sys_GetInfo(sys_info_t *info);
void Sys_InitHardwareInfo(void);
void Sys_RegisterInfoCvars(void);
qboolean Sys_UpdateForConfigChange(void);
qboolean Sys_ConfigureChecksumChanged(int32_t checksum);
void Sys_ArchiveInfo(int32_t checksum);
qboolean Sys_UpdateForInfoChange(void);
qboolean Sys_InfoChanged(void);

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(sys_info_t, cpuFrequencyMHz) == 0x00, "original i386 system-info CPU frequency offset");
_Static_assert(offsetof(sys_info_t, physicalMemoryMB) == 0x08, "original i386 system-info RAM field offset");
_Static_assert(offsetof(sys_info_t, videoMemoryMB) == 0x0c, "original i386 system-info video-memory field offset");
_Static_assert(sizeof(sys_info_t) == 0x10, "original i386 system-info record size");
#endif

#endif
