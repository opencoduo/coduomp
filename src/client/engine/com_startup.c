#include "q_shared.h"

#include "client/common/client_branding.h"
#include "client/common/client_legacy_crt.h"
#include "com_startup.h"

#include "animation/xanim_pool.h"
#include "client/cgame.h"
#include "filesystem/filesystem.h"
#include "filesystem/server_namespace.h"
#include "qcommon/hunk.h"
#include "networking/net_address.h"
#include "networking/net_channel.h"
#include "scripting/script_runtime.h"
#include "server/server.h"
#include "system_console.h"
#include "system_event.h"
#include "system_info.h"
#include "system_platform.h"
#include "system_process_lock.h"
#include "platform/hardware_profile.h"
#include "ui/ui_module_loader.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

enum {
    COM_CONFIGURE_CHECKSUM_MULTIPLIER = 31337,
    COM_CONFIGURE_CHECKSUM_MASK = 268435455,
    COM_RECOMMENDED_CVAR_LIMIT = 256,
    COM_RECOMMENDED_CVAR_NAME_BYTES = 128,
    COM_RECOMMENDED_CVAR_VALUE_BYTES = 256,
    COM_RECOMMENDED_MINIMUM_SYSTEM_MB = 128,
    COM_RECOMMENDED_SYSTEM_ALLOWANCE_MB = 8,
    COM_RECOMMENDED_MINIMUM_VIDEO_MB = 32,
    COM_BUILD_VERSION_CAPACITY = 64,
    COM_QPORT_MASK = 65535,
    COM_CONSOLE_VISIBLE = 1
};

static char comBuildVersion[COM_BUILD_VERSION_CAPACITY];
                                        /* original 0x04e3d2e0 */

/* Source: CoDUOMP.exe 0x00401150..0x00401176.
 * Evidence: the PE body formats the build number, compile date, and compile
 * time into the static buffer at 0x04e3d2e0 and returns that buffer. The exact
 * source symbol is absent from the Mac binary; the role name follows the
 * common-system naming used by the surrounding source. MSVC retained this
 * out-of-line definition while also inlining it into Com_Init.
 * INTENTIONAL_OVERRIDE: the improved client does not maintain a numeric build
 * sequence, so its local display identity retains only the date and time. */
const char *Com_GetBuildVersion(void)
{
    (void)sprintf(comBuildVersion, "%s %s",
                  CODUOMP_DISPLAY_BUILD_DATE, "20:33:18");
    return comBuildVersion;
}

/* Source: CoDUOMP.exe 0x0043b2c0..0x0043b2e8. Exact symbol is absent from
 * the Mac traceback table; the same multiplier, signed-byte input, mask, and
 * nonzero increment are inlined into Com_ConfigureChecksum. */
static uint32_t Com_ConfigureChecksumValue(
    const int8_t *data, int32_t length)
{
    uint32_t checksum = 0;

    for (int32_t index = 0; index < length; ++index) {
        checksum =
            checksum * (uint32_t)COM_CONFIGURE_CHECKSUM_MULTIPLIER +
            (uint32_t)(int32_t)data[index];
    }
    checksum &= (uint32_t)COM_CONFIGURE_CHECKSUM_MASK;
    return checksum + 1u;
}

#define CODUOMP_APPLE_SILICON_RECOMMENDED_VIDEO_MODE "20"

/* NOT_FROM_ORIGINAL_SOURCE: the retail recommendation table predates unified
 * Apple GPUs and tops out below the quality that Apple Silicon can sustain.
 * This override is called only from the existing first-run recommendation
 * path; it is not a config migration and never rewrites an established
 * installation merely because the executable version changed. */
static void coduomp_apply_apple_silicon_first_run_profile(void)
{
    static const struct {
        const char *name;
        const char *value;
    } settings[] = {
        { "com_maxfps", "250" },
        { "r_mode", CODUOMP_APPLE_SILICON_RECOMMENDED_VIDEO_MODE },
        { "r_fullscreen", "1" },
        { "r_aspectMode", "0" },
        { "r_picmip", "0" },
        { "r_picmip2", "0" },
        { "r_textureMode", "GL_LINEAR_MIPMAP_LINEAR" },
        { "r_texturebits", "32" },
        { "r_colorbits", "32" },
        { "r_depthbits", "24" },
        { "r_stencilbits", "8" },
        { "r_ext_compressed_textures", "0" },
        { "r_vertexLight", "0" },
        { "r_lodbias", "0" },
        { "r_lodscale", "0" },
        { "r_subdivisions", "4" },
        { "r_dynamiclight", "1" },
        { "r_dlightQuality", "1" },
        { "r_flareOcclusionQuery", "1" },
        { "cg_marks", "1" },
        { "cg_brass", "1" },
        { "cg_blood", "1" },
        { "cg_shellshockblur", "1" },
        { "cg_vehicletrails", "1" },
        { "ai_corpseCount", "64" },
        { "fx_cullscale", "1" },
        { "fx_cullbias", "0" }
    };

    Com_Printf(
        "Applying Apple Silicon first-run graphics/performance profile\n");
    for (size_t index = 0;
         index < sizeof(settings) / sizeof(settings[0]);
         ++index) {
        cvar_t *const cvar =
            Cvar_Set2(settings[index].name, settings[index].value, qtrue);
        cvar->flags |= CVAR_ARCHIVE;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: distinguishes an explicit user preference from
 * the retail default_mp.cfg assignment.  Generated configs use seta, while
 * the token scan also recognizes equivalent hand-written set/direct forms. */
static qboolean coduomp_config_sets_cvar(const char *path,
                                         const char *cvarName)
{
    void *fileBuffer = NULL;
    const int32_t fileLength = FS_ReadFile(path, &fileBuffer);
    qboolean found = qfalse;

    if (fileLength <= 0 || fileBuffer == NULL)
        return qfalse;

    char *parseCursor = fileBuffer;
    Com_BeginParseSession(path);
    for (;;) {
        const char *const token = Com_Parse(&parseCursor);

        if (token[0] == '\0')
            break;
        if (Q_stricmp(token, cvarName) == 0) {
            found = qtrue;
            break;
        }
    }
    Com_EndParseSession();
    FS_FreeFile(fileBuffer);
    return found;
}

/* Source: CoDUOMP.exe 0x0043bba0..0x0043bc0f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043bba0_0043bc10.mcode.
 * Name: exact same-version Mac symbol Script_Init. Role and argument order
 * independently match the recovered Linux engine initialization. The Windows
 * optimizer inlines Scr_Init, accounting for the complete body after
 * the three cvar reads. */
void Script_Init(void)
{
    const qboolean debugReport =
        com_developer->integer != 0 ||
        com_logfile->integer != 0;

    Scr_Init(debugReport, com_developerScript->integer,
             com_developer->integer);
}

/* Source: CoDUOMP.exe 0x0043b2f0..0x0043b359.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043b2f0_0043b35a.mcode and the
 * original strings at 0x005986c8..0x005986fa.
 * Name and return role: exact same-module Mac symbol
 * Com_ConfigureChecksum. Input bytes are sign-extended before the original
 * 32-bit wrapping multiply/add; the final mask reserves zero as "unset". */
qboolean Com_ConfigureChecksum(void)
{
    void *fileBuffer;
    const int32_t fileLength =
        FS_ReadFile("configure_mp.csv", &fileBuffer);
    uint32_t checksum;

    if (fileLength < 0) {
        Com_Error(ERR_FATAL,
                  "EXE_ERR_NOT_FOUND\x15" "configure_mp.csv");
    }

    checksum = Com_ConfigureChecksumValue(
        (const int8_t *)fileBuffer, fileLength);
    FS_FreeFile(fileBuffer);
    return Sys_ConfigureChecksumChanged((int32_t)checksum);
}

/* Source: CoDUOMP.exe 0x0043b360..0x0043bb98.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043b360_0043bb99.mcode and exact
 * strings 0x00598300..0x005986fa.
 * Name: exact same-module Mac symbol Com_SetRecommended. The Windows linker
 * inlines the parser-session transitions and command-buffer append paths.
 *
 * configure_mp.csv is ordered from lower to higher hardware requirements.
 * The PE therefore replaces the selected values for each qualifying row while
 * the selected CPU threshold remains below the detected CPU allowance. The
 * exact tie path compares the already selected video/system thresholds with
 * the detected allowances; it is retained even though comparing against the
 * current row would look more conventional. */
void Com_SetRecommended(qboolean restartSound)
{
    char cvarNames[COM_RECOMMENDED_CVAR_LIMIT]
                  [COM_RECOMMENDED_CVAR_NAME_BYTES];
    char cvarValues[COM_RECOMMENDED_CVAR_LIMIT]
                   [COM_RECOMMENDED_CVAR_VALUE_BYTES];
    void *fileBuffer;

    Com_Printf("========= autoconfigure\n");

    sys_info_t hardware;
    Sys_GetInfo(&hardware);
    const double cpuAllowance =
        hardware.cpuFrequencyMHz * 1.0200000000000000178;
    const int32_t systemMemoryAllowance =
        hardware.physicalMemoryMB < COM_RECOMMENDED_MINIMUM_SYSTEM_MB
            ? COM_RECOMMENDED_MINIMUM_SYSTEM_MB
            : hardware.physicalMemoryMB +
                  COM_RECOMMENDED_SYSTEM_ALLOWANCE_MB;
    const int32_t videoMemoryAllowance =
        hardware.videoMemoryMB < COM_RECOMMENDED_MINIMUM_VIDEO_MB
            ? COM_RECOMMENDED_MINIMUM_VIDEO_MB
            : hardware.videoMemoryMB;

    const int32_t fileLength =
        FS_ReadFile("configure_mp.csv", &fileBuffer);
    if (fileLength < 0) {
        Com_Error(ERR_FATAL,
                  "EXE_ERR_NOT_FOUND\x15"
                  "configure_mp.csv");
    }

    Com_BeginParseSession("configure_mp.csv");
    Com_SetCSV(qtrue);
    char *parseCursor = fileBuffer;
    int32_t cvarCount = 0;

    char *token;
    for (;;) {
        token = Com_Parse(&parseCursor);
        if (parseCursor == NULL)
            goto parsing_complete;
        if (token[0] != '\0' && token[0] != '#')
            break;
        Com_SkipRestOfLine(&parseCursor);
    }

    if (strcmp(token, "cpu mhz") != 0) {
        Com_Error(
            ERR_FATAL,
            "\x15"
            "configure_mp.csv: \"cpu mhz\" should be the first column\n");
    }

    token = Com_ParseOnLine(&parseCursor);
    if (strcmp(token, "sys mb") != 0) {
        Com_Error(
            ERR_FATAL,
            "\x15"
            "configure_mp.csv: \"sys mb\" should be the second column\n");
    }

    token = Com_ParseOnLine(&parseCursor);
    if (strcmp(token, "vid mb") != 0) {
        Com_Error(
            ERR_FATAL,
            "\x15"
            "configure_mp.csv: \"vid mb\" should be the third column\n");
    }

    for (;;) {
        token = Com_ParseOnLine(&parseCursor);
        if (parseCursor == NULL) {
            Com_Error(ERR_FATAL,
                      "\x15"
                      "configure_mp.csv: unexpected end-of-file");
        }
        if (token[0] == '\0')
            break;

        const size_t nameLength = strlen(token);
        if (nameLength >= COM_RECOMMENDED_CVAR_NAME_BYTES) {
            Com_Error(
                ERR_FATAL,
                "\x15"
                "configure_mp.csv: cvar name \"%s\" longer than %i\n",
                token, COM_RECOMMENDED_CVAR_NAME_BYTES - 1);
        }
        if (cvarCount >= COM_RECOMMENDED_CVAR_LIMIT) {
            Com_Error(ERR_FATAL,
                      "\x15"
                      "configure_mp.csv: more than %i cvars\n",
                      COM_RECOMMENDED_CVAR_LIMIT);
        }

        memcpy(cvarNames[cvarCount], token, nameLength + 1);
        ++cvarCount;
    }

    double selectedCpuMHz = -1.0;
    int32_t selectedSystemMB = 0;
    int32_t selectedVideoMB = 0;

    for (;;) {
        token = Com_Parse(&parseCursor);
        if (parseCursor == NULL)
            break;

        if (token[0] == '\0' || token[0] == '#') {
            Com_SkipRestOfLine(&parseCursor);
            continue;
        }

        const double rowCpuMHz = atof(token);
        if (rowCpuMHz < 0.0) {
            Com_Error(
                ERR_FATAL,
                "\x15"
                "configure_mp.csv: cpu mhz %g not allowed to be less than 0\n",
                rowCpuMHz);
        }

        const int32_t rowSystemMB =
            coduo_crt_atoi(Com_ParseOnLine(&parseCursor));
        if (rowSystemMB < COM_RECOMMENDED_MINIMUM_SYSTEM_MB) {
            Com_Error(
                ERR_FATAL,
                "\x15"
                "configure_mp.csv: sys mb %i not allowed to be less than 128\n",
                rowSystemMB);
        }

        const int32_t rowVideoMB =
            coduo_crt_atoi(Com_ParseOnLine(&parseCursor));
        if (rowVideoMB < COM_RECOMMENDED_MINIMUM_VIDEO_MB) {
            Com_Error(
                ERR_FATAL,
                "\x15"
                "configure_mp.csv: vid mb %i not allowed to be less than 32\n",
                rowVideoMB);
        }

        qboolean selectRow = qfalse;
        if (cpuAllowance >= rowCpuMHz &&
            systemMemoryAllowance >= rowSystemMB &&
            videoMemoryAllowance >= rowVideoMB) {
            if (selectedCpuMHz < cpuAllowance ||
                (selectedCpuMHz == cpuAllowance &&
                 (selectedVideoMB < videoMemoryAllowance ||
                  (selectedVideoMB == videoMemoryAllowance &&
                   selectedSystemMB < systemMemoryAllowance)))) {
                selectRow = qtrue;
                selectedCpuMHz = rowCpuMHz;
                selectedSystemMB = rowSystemMB;
                selectedVideoMB = rowVideoMB;
            }
        }

        for (int32_t cvarIndex = 0;
             cvarIndex < cvarCount; ++cvarIndex) {
            token = Com_ParseOnLine(&parseCursor);
            if (parseCursor == NULL) {
                Com_Error(ERR_FATAL,
                          "\x15"
                          "configure_mp.csv: unexpected end-of-file");
            }
            if (token[0] == '\0') {
                Com_Error(
                    ERR_FATAL,
                    "\x15"
                    "configure_mp.csv: missing entry for cvar '%s' in row %lg %i %i\n",
                    cvarNames[cvarIndex], rowCpuMHz, rowSystemMB,
                    rowVideoMB);
            }

            const size_t valueLength = strlen(token);
            if (valueLength >= COM_RECOMMENDED_CVAR_VALUE_BYTES) {
                Com_Error(
                    ERR_FATAL,
                    "\x15"
                    "configure_mp.csv: entry '%s' for cvar '%s' in row %lg %i %i is longer than %i\n",
                    token, cvarNames[cvarIndex], rowCpuMHz, rowSystemMB,
                    rowVideoMB, COM_RECOMMENDED_CVAR_VALUE_BYTES - 1);
            }

            if (selectRow != qfalse) {
                memcpy(cvarValues[cvarIndex], token, valueLength + 1);
            }
        }

        token = Com_ParseOnLine(&parseCursor);
        if (token[0] != '\0') {
            Com_Error(
                ERR_FATAL,
                "\x15"
                "configure_mp.csv: extra cvar value column(s) in row %lg %i %i\n",
                rowCpuMHz, rowSystemMB, rowVideoMB);
        }
    }

parsing_complete:
    Com_EndParseSession();

    uint32_t checksum = 0;
    for (int32_t index = 0; index < fileLength; ++index) {
        checksum =
            checksum * (uint32_t)COM_CONFIGURE_CHECKSUM_MULTIPLIER +
            (uint32_t)(int32_t)((const int8_t *)fileBuffer)[index];
    }
    checksum &= (uint32_t)COM_CONFIGURE_CHECKSUM_MASK;
    ++checksum;
    FS_FreeFile(fileBuffer);

    if (selectedCpuMHz < 0.0 ||
        selectedSystemMB == 0 || selectedVideoMB == 0) {
        Com_Error(
            ERR_FATAL,
            "\x15"
            "configure_mp.csv: \x14"
            "EXE_ERR_COULDNT_CONFIGURE\x15"
            " %.0f cpu MHz %i sys MB %i vid MB\n",
            hardware.cpuFrequencyMHz, hardware.physicalMemoryMB,
            hardware.videoMemoryMB);
    }

    Com_Printf(
        "configure_mp.csv: using configuration %.0f cpu MHz %i sys MB %i vid MB\n",
        selectedCpuMHz, selectedSystemMB, selectedVideoMB);
    Cbuf_AddText("exec configure_mp.cfg");
    Cbuf_Execute();

    for (int32_t cvarIndex = 0; cvarIndex < cvarCount; ++cvarIndex) {
        (void)Cvar_Set2(cvarNames[cvarIndex],
                        cvarValues[cvarIndex], qtrue);
        cvar_t *const cvar = Cvar_FindVar(cvarNames[cvarIndex]);
        cvar->flags |= CVAR_ARCHIVE;
    }

    if (coduomp_is_apple_silicon() != qfalse)
        coduomp_apply_apple_silicon_first_run_profile();

    Sys_ArchiveInfo((int32_t)checksum);
    if (restartSound != qfalse)
        Cbuf_AddText("snd_restart\n");
}

/* Source: CoDUOMP.exe 0x0043bc10..0x0043c253.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043bc10_0043c254.mcode, exact PE
 * strings and cvar operands, and the same-module Mac symbol Com_Init.
 * The source-level calls below retain the original order while replacing
 * MSVC's inlined Cvar_Init, Cbuf_Init, Key_Init, SL_Init, and VM_Init bodies
 * with their recovered boundaries. The two stock logo assignments are
 * intentionally consecutive: the Windows executable performs both. */
void Com_Init(char *commandLine)
{
    Com_Printf("%s %s build %s %s\n",
               CODUOMP_DISPLAY_PRODUCT, CODUOMP_DISPLAY_VERSION,
               CODUOMP_PLATFORM_TAG,
               CODUOMP_DISPLAY_BUILD_DATE);

    if (setjmp(com_abortFrame) != 0) {
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
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): establish the improved
     * reset/default value before default_mp.cfg creates this cvar with the
     * retail disabled value. Config and command-line selections still replace
     * the live value below. */
    qboolean allowDownloadConfigured =
        Cvar_FindVar("cl_allowDownload") != NULL ? qtrue : qfalse;
    (void)Cvar_Get("cl_allowDownload", "1", CVAR_ARCHIVE);
    Key_Init();
    /* A prior process may have crashed while a server mod was active. The
     * selected provider resets only transient in-memory ownership here; no
     * server-cache directory is enumerated or mounted during normal startup. */
    coduomp_server_namespace_reset_for_startup();
    FS_InitFilesystem();
    Com_InitJournaling();

    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): preserve an explicit
     * user choice, but do not let the retail default_mp.cfg silently disable
     * downloads when the saved config and autoexec omit the setting. */
    allowDownloadConfigured =
        allowDownloadConfigured ||
        coduomp_config_sets_cvar("uoconfig_mp.cfg", "cl_allowDownload") ||
        coduomp_config_sets_cvar("autoexec_mp.cfg", "cl_allowDownload");

    Cbuf_AddText("exec default_mp.cfg\n");
    Cbuf_AddText("exec language.cfg\n");
    Cbuf_AddText("exec uoconfig_mp.cfg\n");
    Cbuf_AddText("exec autoexec_mp.cfg\n");
    if (Com_SafeMode() != qfalse)
        Cbuf_AddText("exec safemode_mp.cfg\n");
    Cbuf_Execute();

    com_recommendedSet =
        Cvar_Get("com_recommendedSet", "0", CVAR_ARCHIVE);
    if (com_recommendedSet->integer == 0 ||
        Com_ConfigureChecksum() != qfalse) {
        Com_SetRecommended(qfalse);
        (void)Cvar_Set2("com_recommendedSet", "1", qtrue);
    }

    if (Sys_InfoChanged() != qfalse)
        Com_SetRecommended(qfalse);

    if (allowDownloadConfigured == qfalse) {
        cvar_t *const allowDownload =
            Cvar_Set2("cl_allowDownload", "1", qtrue);
        allowDownload->flags |= CVAR_ARCHIVE;
    }

    Com_StartupVariable(NULL);
    SEH_UpdateLanguageInfo();

    dedicated = Cvar_Get("dedicated", "0", CVAR_LATCH);
    if (dedicated->integer != 0) {
        Sys_HideSplashWindow();
        Sys_ShowConsole(COM_CONSOLE_VISIBLE, qtrue);
#if defined(_WIN32)
        Sys_DeleteProcessLockFile();
#endif
    }

    Com_InitHunkMemory();
    cvar_modifiedFlags &= ~(uint32_t)CVAR_ARCHIVE;

    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): use the high-rate
     * frame-rate default only on Apple-Silicon hosts. This is the
     * missing-cvar path; the first-run recommendation profile above supplies
     * the same value after retail configure_mp.cfg sets 85. Intel Macs and
     * every non-Apple-Silicon target retain the retail default. */
    com_maxfps = Cvar_Get(
        "com_maxfps",
        coduomp_is_apple_silicon() != qfalse ? "250" : "85",
        CVAR_ARCHIVE);
    com_developer =
        Cvar_Get("developer", "0", CVAR_TEMP);
    com_developerScript =
        Cvar_Get("developer_script", "0", CVAR_TEMP);
    com_logfile = Cvar_Get("logfile", "0", 0);
    com_statmon = Cvar_Get("com_statmon", "0", 0);
    com_timescale =
        Cvar_Get("timescale", "1",
                 CVAR_CHEAT |
                     CVAR_SCRIPT_MAKE_SERVERINFO);
    com_fixedtime =
        Cvar_Get("fixedtime", "0", CVAR_CHEAT);
    com_viewlog =
        Cvar_Get("viewlog", "0", CVAR_CHEAT);
    com_speeds = Cvar_Get("com_speeds", "0", 0);
    sv_paused = Cvar_Get("sv_paused", "0", CVAR_ROM);
    cl_paused = Cvar_Get("cl_paused", "0", CVAR_ROM);
    sv_running = Cvar_Get("sv_running", "0", CVAR_ROM);
    cl_running = Cvar_Get("cl_running", "0", CVAR_ROM);
    com_introPlayed =
        Cvar_Get("com_introplayed", "0", CVAR_ARCHIVE);
    com_animCheck = Cvar_Get("com_animCheck", "0", 0);
    hunk_used = 0;

    if (dedicated->integer != 0 && com_viewlog->integer == 0)
        (void)Cvar_Set2("viewlog", "1", qtrue);

    if (com_developer->integer != 0) {
        Cmd_AddCommand("error", Com_Error_f);
        Cmd_AddCommand("crash", Com_Crash_f);
        Cmd_AddCommand("freeze", Com_Freeze_f);
    }
    Cmd_AddCommand("quit", Com_Quit_f);
    Cmd_AddCommand("writeconfig", Com_WriteConfig_f);
    Cmd_AddCommand("writedefaults", Com_WriteDefaults_f);
    coduomp_server_namespace_register_commands();

    com_version =
        Cvar_Get("version",
                 va("%s %s build %s %s",
                    CODUOMP_DISPLAY_PRODUCT, CODUOMP_DISPLAY_VERSION,
                    Com_GetBuildVersion(), CODUOMP_PLATFORM_TAG),
                 CVAR_ROM);
    com_shortVersion =
        Cvar_Get("shortversion", "1.51",
                 CVAR_ROM | CVAR_SERVERINFO);

    Sys_Init();
    Netchan_Init((int32_t)(Com_Milliseconds() & COM_QPORT_MASK));
    Script_Init();
    XAnimInit();
    DObjInit();
    VM_Init();
    SV_Init();
    Sys_InitNetworking();
    dedicated->modified = qfalse;

    if (dedicated->integer == 0) {
        CL_Init();
        Sys_ShowConsole(com_viewlog->integer, qfalse);
    }

    com_frameTime = (int32_t)Com_Milliseconds();
    (void)Com_AddStartupCommands();
    (void)Cvar_Set2("r_uiFullScreen", "1", qtrue);
    CL_StartHunkUsers();

    if (dedicated->integer == 0) {
        Sys_ShowConsole(com_viewlog->integer, qfalse);
        (void)Cvar_Set2("cl_movieplaying", "0", qtrue);

        if (com_introPlayed->integer == 0) {
            (void)Cvar_Set2(
                com_introPlayed->name, "1", qtrue);
            Cbuf_AddText("cinematic atvi.bik\n");
            (void)Cvar_Set2(
                "nextmap", "cinematic gmi_logo.roq", qtrue);
            (void)Cvar_Set2(
                "nextmap", "cinematic iw_logo.roq", qtrue);
        }
    }

    (void)Cvar_Set2("com_statmon", "0", qtrue);
    com_configAutowriteEnabled = qtrue;
    Com_Printf("--- Common Initialization Complete ---\n");
}
