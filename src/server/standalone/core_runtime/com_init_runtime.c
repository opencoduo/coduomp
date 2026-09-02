#define _POSIX_C_SOURCE 200809L

#include <setjmp.h>
#include <stdint.h>

#include "qcommon/vm_runtime.h"
#include "../animation/xanim_private.h"
#include "qcommon/q_command.h"
#include "../core_cvar/cvar_private.h"
#include "../filesystem/fs_private.h"
#include "../networking/netchan_private.h"
#include "../server/server_private.h"
#include "core_runtime_private.h"

#define COM_INIT_LONGJMP_SAVE_MASK 0
#define COM_INIT_CVAR_AUTOWRITE_MASK 0x00000001
#define COM_INIT_RECOMMENDED_FLAGS CVAR_ARCHIVE
#define COM_INIT_DEDICATED_FLAGS CVAR_ROM
#define COM_INIT_ARCHIVE_FLAGS CVAR_ARCHIVE
#define COM_INIT_DEVELOPER_FLAGS CVAR_TEMP
#define COM_INIT_LATCH_FLAGS CVAR_LATCH
#define COM_INIT_PAUSED_FLAGS CVAR_ROM
#define COM_INIT_VERSION_FLAGS CVAR_ROM
#define COM_INIT_SHORTVERSION_FLAGS (CVAR_ROM | CVAR_SERVERINFO)
#define COM_INIT_DEDICATED_DEFAULT "2"
#define COM_INIT_MAXFPS_DEFAULT "85"
#define COM_INIT_ZERO "0"
#define COM_INIT_ONE "1"
#define COM_INIT_QPORT_MASK 0xffff
#if UINTPTR_MAX > UINT32_MAX
#define COM_INIT_ARCH "x86_64"
#else
#define COM_INIT_ARCH "i386"
#endif
/*
 * INTENTIONAL_OVERRIDE: identify the recovered host build in logs and cvars.
 * These strings intentionally differ from the original dedicated binary.
 */
#if defined(_WIN32)
#define COM_INIT_PLATFORM "windows-" COM_INIT_ARCH
#else
#define COM_INIT_PLATFORM "linux-" COM_INIT_ARCH
#endif
#define COM_INIT_BUILD_DATE "Jul 01 2026"
#define COM_INIT_VERSION_STRING "1.51"
#define COM_INIT_PRODUCT_STRING "CoD:UO Recovered Dedicated Engine"
#define COM_INIT_SAFE_MODE_CONFIG "exec safemode_mp_server.cfg\n"
#define COM_INIT_DEFAULT_CONFIG "exec default_mp.cfg\n"
#define COM_INIT_LANGUAGE_CONFIG "exec language.cfg\n"
#define COM_INIT_UO_CONFIG "exec uoconfig_mp_server.cfg\n"
#define COM_INIT_AUTOEXEC_CONFIG "exec autoexec_mp.cfg\n"
#define COM_INIT_INTRO_CINEMATIC "cinematic atvi.bik\n"
#define COM_INIT_NEXTMAP_GMI_LOGO "cinematic gmi_logo.roq"
#define COM_INIT_NEXTMAP_IW_LOGO "cinematic iw_logo.roq"

cvar_t *dedicated;
cvar_t *com_developer;
cvar_t *com_logfile;
cvar_t *host_cvar_developer_script_pointer_slot;
cvar_t *host_cvar_com_recommendedSet_pointer_slot;
cvar_t *host_cvar_com_maxfps_pointer_slot;
cvar_t *com_timescale;
cvar_t *host_cvar_viewlog_pointer_slot;
cvar_t *com_fixedtime;
cvar_t *com_speeds;
cvar_t *sv_paused;
cvar_t *cl_paused;
cvar_t *host_cvar_com_introplayed_pointer_slot;
cvar_t *host_cvar_com_animCheck_pointer_slot;
cvar_t *cvar_version;
cvar_t *cvar_shortversion;
int32_t hunk_used;
/* Original 0x0848871c: the Com_EventLoop result consumed by server IDs. */
int32_t com_frameTime;

void Com_Init(char *commandLine)
{
    Com_Printf("%s %s (%s) build %s\n", COM_INIT_PRODUCT_STRING, COM_INIT_VERSION_STRING, COM_INIT_PLATFORM, COM_INIT_BUILD_DATE);

    if (CODUO_SETJMP(com_frameAbortContext, COM_INIT_LONGJMP_SAVE_MASK) != 0) {
        Com_ErrorCleanup();
        Sys_Error("Error during initialization");
    }

    Com_ClearPushEventsForStartup();
    Cvar_Init();
    Com_ParseCommandLine(commandLine);
    Swap_Init();
    Cbuf_Init();
    Com_InitZoneMemory();
    Cmd_Init();
    Com_StartupVariable(NULL);
    Com_StartupVariable("developer");
    CL_InitKeyCommands();
    FS_InitFilesystem();
    Com_InitJournaling();

    Cbuf_AddText(COM_INIT_DEFAULT_CONFIG);
    Cbuf_AddText(COM_INIT_LANGUAGE_CONFIG);
    Cbuf_AddText(COM_INIT_UO_CONFIG);
    Cbuf_AddText(COM_INIT_AUTOEXEC_CONFIG);
    if (Com_SafeMode() != qfalse) {
        Cbuf_AddText(COM_INIT_SAFE_MODE_CONFIG);
    }
    Cbuf_Execute();

    host_cvar_com_recommendedSet_pointer_slot = Cvar_Get("com_recommendedSet", COM_INIT_ZERO, COM_INIT_RECOMMENDED_FLAGS);
    if (host_cvar_com_recommendedSet_pointer_slot->integer == 0 || Com_ApplyConfigureFileChecksum() != 0) {
        Com_SetRecommended();
        Cvar_Set("com_recommendedSet", COM_INIT_ONE);
    }

    if (Sys_InfoChanged() != qfalse) {
        Com_SetRecommended();
    }

    Com_StartupVariable(NULL);
    dedicated = Cvar_Get("dedicated", COM_INIT_DEDICATED_DEFAULT, COM_INIT_DEDICATED_FLAGS);

    Com_InitHunkMemory();
    cvar_modifiedFlags &= ~COM_INIT_CVAR_AUTOWRITE_MASK;

    host_cvar_com_maxfps_pointer_slot = Cvar_Get("com_maxfps", COM_INIT_MAXFPS_DEFAULT, COM_INIT_ARCHIVE_FLAGS);
    com_developer = Cvar_Get("developer", COM_INIT_ZERO, COM_INIT_DEVELOPER_FLAGS);
    host_cvar_developer_script_pointer_slot = Cvar_Get("developer_script", COM_INIT_ZERO, COM_INIT_DEVELOPER_FLAGS);
    com_logfile = Cvar_Get("logfile", COM_INIT_ZERO, 0);
    com_timescale = Cvar_Get("timescale", COM_INIT_ONE, CVAR_CHEAT | CVAR_SCRIPT_MAKE_SERVERINFO);
    com_fixedtime = Cvar_Get("fixedtime", COM_INIT_ZERO, COM_INIT_LATCH_FLAGS);
    host_cvar_viewlog_pointer_slot = Cvar_Get("viewlog", COM_INIT_ZERO, COM_INIT_LATCH_FLAGS);
    com_speeds = Cvar_Get("com_speeds", COM_INIT_ZERO, 0);
    sv_paused = Cvar_Get("sv_paused", COM_INIT_ZERO, COM_INIT_PAUSED_FLAGS);
    cl_paused = Cvar_Get("cl_paused", COM_INIT_ZERO, COM_INIT_PAUSED_FLAGS);
    sv_running = Cvar_Get("sv_running", COM_INIT_ZERO, COM_INIT_PAUSED_FLAGS);
    cl_running = Cvar_Get("cl_running", COM_INIT_ZERO, COM_INIT_PAUSED_FLAGS);
    host_cvar_com_introplayed_pointer_slot = Cvar_Get("com_introplayed", COM_INIT_ZERO, COM_INIT_ARCHIVE_FLAGS);
    host_cvar_com_animCheck_pointer_slot = Cvar_Get("com_animCheck", COM_INIT_ZERO, 0);

    hunk_used = 0;
    if (dedicated->integer != 0 && host_cvar_viewlog_pointer_slot->integer == 0) {
        Cvar_Set("viewlog", COM_INIT_ONE);
    }

    if (com_developer != NULL && com_developer->integer != 0) {
        Cmd_AddCommand("error", Com_Error_f);
        Cmd_AddCommand("crash", Com_Crash_f);
        Cmd_AddCommand("freeze", Com_Freeze_f);
    }
    Cmd_AddCommand("quit", Com_Quit_f);
    Cmd_AddCommand("writeconfig", Com_WriteConfig_f);
    Cmd_AddCommand("writedefaults", Com_WriteDefaults_f);

    cvar_version = Cvar_Get(
        "version", va("%s %s build %s %s", COM_INIT_PRODUCT_STRING, COM_INIT_VERSION_STRING, Com_BuildVersionString(), COM_INIT_PLATFORM),
        COM_INIT_VERSION_FLAGS);
    cvar_shortversion = Cvar_Get("shortversion", COM_INIT_VERSION_STRING, COM_INIT_SHORTVERSION_FLAGS);

    Sys_Init();
    Netchan_Init(Com_Milliseconds() & COM_INIT_QPORT_MASK);
    Com_InitScriptRuntime();
    XAnimInit();
    DObjInit();
    VM_Init();
    SV_Init();
    NET_Init();
    dedicated->modified = qfalse;

    if (dedicated->integer == 0) {
        CL_Init();
        Sys_ShowConsole(host_cvar_viewlog_pointer_slot->integer, qfalse);
    }

    com_frameTime = Com_Milliseconds();
    Com_AddStartupCommands();
    Cvar_Set("r_uiFullScreen", COM_INIT_ONE);
    CL_StartHunkUsers();

    if (dedicated->integer == 0) {
        Sys_ShowConsole(host_cvar_viewlog_pointer_slot->integer, qfalse);
    }

    if (dedicated->integer == 0) {
        Cvar_Set("cl_movieplaying", COM_INIT_ZERO);
        if (host_cvar_com_introplayed_pointer_slot->integer == 0) {
            Cvar_Set(host_cvar_com_introplayed_pointer_slot->name, COM_INIT_ONE);
            Cbuf_AddText(COM_INIT_INTRO_CINEMATIC);
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            Cvar_Set("nextmap", COM_INIT_NEXTMAP_GMI_LOGO);
            Cvar_Set("nextmap", COM_INIT_NEXTMAP_IW_LOGO);
        }
    }

    Cvar_Set("com_statmon", COM_INIT_ZERO);
    com_configAutowriteEnabled = qtrue;
    Com_Printf("--- Common Initialization Complete ---\n");
}
