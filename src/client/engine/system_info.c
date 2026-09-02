#include "system_info.h"

#include "system_localization.h"

#if defined(_WIN32)
#include <windows.h>
#endif

enum {
    SYS_INFO_CVAR_FLAGS = 65,
    SYS_INFO_MEMORY_TOLERANCE_MB = 32,
    SYS_INFO_VIDEO_MEMORY_TOLERANCE_MB = 16,
    SYS_RECONFIGURE_DIALOG_FLAGS = 68,
    SYS_DIALOG_RESULT_YES = 6
};

static cvar_t *sys_cpuMHz; /* original 0x009cf2ec */
static cvar_t *sys_sysMB;  /* original 0x009cf1d0 */
static cvar_t *sys_vidMB;  /* original 0x009cf2f0 */

/* NOT_FROM_ORIGINAL_SOURCE: portable factoring of the two identical direct
 * MessageBoxA calls in Sys_UpdateForConfigChange and
 * Sys_UpdateForInfoChange. */
static qboolean coduomp_show_reconfigure_dialog(const char *body,
                                                const char *title)
{
#if defined(_WIN32)
    return MessageBoxA(NULL, body, title,
                       SYS_RECONFIGURE_DIALOG_FLAGS) ==
                   SYS_DIALOG_RESULT_YES
               ? qtrue
               : qfalse;
#else
    /* NOT_FROM_ORIGINAL_SOURCE: a native platform question-dialog backend
     * will replace the Win32 MessageBox operation. Until then, decline the
     * optional automatic reconfiguration. */
    (void)body;
    (void)title;
    return qfalse;
#endif
}

/* Source: CoDUOMP.exe 0x0046a960..0x0046a983, recovered from an executable
 * gap after repairing the missing Ghidra function entry. The same-version Mac
 * Sys_GetInfo at code+0x00003be0 independently stores the double at +0x00 and
 * the two memory dwords at +0x08/+0x0c. */
void Sys_GetInfo(sys_info_t *info)
{
    info->cpuFrequencyMHz = sysCpuFrequencyMHz;
    info->physicalMemoryMB = sysPhysicalMemoryMB;
    info->videoMemoryMB = sysVideoMemoryMB;
}

/* Source: CoDUOMP.exe 0x0046a990..0x0046a9be.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046a990_0046a9bf.mcode.
 * Name: exact same-module Mac symbol Sys_UpdateForConfigChange. */
qboolean Sys_UpdateForConfigChange(void)
{
    const char *title =
        Sys_LocalizeString("WIN_CONFIGURE_UPDATED_TITLE");
    const char *body =
        Sys_LocalizeString("WIN_CONFIGURE_UPDATED_BODY");
    return coduomp_show_reconfigure_dialog(body, title);
}

/* Source: CoDUOMP.exe 0x0046a9c0..0x0046aa3e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046a9c0_0046aa3f.mcode.
 * Name: exact same-module Mac symbol Sys_ConfigureChecksumChanged. The
 * original compiler carries checksum in EBX. */
qboolean Sys_ConfigureChecksumChanged(int32_t checksum)
{
    qboolean reconfigure = qfalse;
    cvar_t *const archivedChecksum =
        Cvar_Get("sys_configSum", "0", SYS_INFO_CVAR_FLAGS);

    if (archivedChecksum->integer != 0 &&
        archivedChecksum->integer != checksum) {
        reconfigure = Sys_UpdateForConfigChange();
    }

    if (archivedChecksum->integer == 0 ||
        archivedChecksum->integer != checksum) {
        (void)Cvar_Set2("sys_configSum", va("%i", checksum), qtrue);
    }

    return reconfigure;
}

/* Source: CoDUOMP.exe 0x0046aa40..0x0046aa85, recovered from an executable
 * gap after repairing the missing Ghidra function entry.
 * Name: exact same-module Mac symbol Sys_RegisterInfoCvars. */
void Sys_RegisterInfoCvars(void)
{
    sys_cpuMHz = Cvar_Get("sys_cpuMHz", "0", SYS_INFO_CVAR_FLAGS);
    sys_sysMB = Cvar_Get("sys_sysMB", "0", SYS_INFO_CVAR_FLAGS);
    sys_vidMB = Cvar_Get("sys_vidMB", "0", SYS_INFO_CVAR_FLAGS);
}

/* Source: CoDUOMP.exe 0x0046aa90..0x0046aabb, recovered from an executable
 * gap after repairing the missing Ghidra function entry.
 * Role name: gathers the four platform hardware values later consumed by
 * Sys_GetInfo, Sys_InfoChanged, and renderer setup. */
void Sys_InitHardwareInfo(void)
{
    sysCpuFrequencyMHz = Sys_GetCpuFrequencyMHz();
    sysPhysicalMemoryMB = Sys_GetPhysicalMemoryMB();
    sysVideoMemoryMB = Sys_GetVideoMemoryMB();
    sysSseSupported = Sys_DetectSSESupport();
}

/* Source: CoDUOMP.exe 0x0046aac0..0x0046aaf8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046aac0_0046aaf9.mcode.
 * Name: exact same-module Mac symbol Sys_UpdateForInfoChange. */
qboolean Sys_UpdateForInfoChange(void)
{
    Sys_ArchiveInfo(0);
    const char *title =
        Sys_LocalizeString("WIN_COMPUTER_CHANGE_TITLE");
    const char *body =
        Sys_LocalizeString("WIN_COMPUTER_CHANGE_BODY");
    return coduomp_show_reconfigure_dialog(body, title);
}

/* Source: CoDUOMP.exe 0x0046ab00..0x0046abf3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046ab00_0046abf4.mcode.
 * Name: exact same-module Mac symbol Sys_InfoChanged. */
qboolean Sys_InfoChanged(void)
{
    /* Exact double constants at 0x005b9e98 and 0x005b9ea0. */
    const double minimumCpuRatio = 0.8999999761581421;
    const double maximumCpuRatio = 1.100000023841858;

    Sys_RegisterInfoCvars();


    if ((double)sys_cpuMHz->value >
            sysCpuFrequencyMHz * maximumCpuRatio ||
        (double)sys_cpuMHz->value <
            sysCpuFrequencyMHz * minimumCpuRatio) {
        return Sys_UpdateForInfoChange();
    }

    if (sys_sysMB->integer >
            sysPhysicalMemoryMB + SYS_INFO_MEMORY_TOLERANCE_MB ||
        sys_sysMB->integer <
            sysPhysicalMemoryMB - SYS_INFO_MEMORY_TOLERANCE_MB) {
        return Sys_UpdateForInfoChange();
    }

    if (sys_vidMB->integer >
            sysVideoMemoryMB + SYS_INFO_VIDEO_MEMORY_TOLERANCE_MB ||
        sys_vidMB->integer <
            sysVideoMemoryMB - SYS_INFO_VIDEO_MEMORY_TOLERANCE_MB) {
        return Sys_UpdateForInfoChange();
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x0046ac00..0x0046acc2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046ac00_0046acc3.mcode.
 * Name: exact same-module Mac symbol Sys_ArchiveInfo. */
void Sys_ArchiveInfo(int32_t checksum)
{
    Sys_RegisterInfoCvars();
    (void)Cvar_Set2("sys_cpuMHz", va("%lg", sysCpuFrequencyMHz), qtrue);
    (void)Cvar_Set2("sys_sysMB", va("%i", sysPhysicalMemoryMB), qtrue);
    (void)Cvar_Set2("sys_vidMB", va("%i", sysVideoMemoryMB), qtrue);
    (void)Cvar_Set2("sys_configSum", va("%i", checksum), qtrue);
}
