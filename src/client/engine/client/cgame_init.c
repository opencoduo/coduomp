#include "cgame.h"

#include "client/common/client_legacy_crt.h"
#include "console.h"
#include "../animation/dobj.h"
#include "../animation/xanim_pool.h"
#include "../localization/string_ed_api.h"
#include "../q_shared.h"
#include "qcommon/q_string.h"
#include "../physics/cm_trace.h"
#include "../renderer/renderer_api.h"
#include "../scripting/script_runtime.h"
#include "sound/alias/sound_alias.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum {
    CL_SUBTITLE_DEFAULT_TIME_MSEC = 5000,
    CL_HUNK_USAGE_LINE_SIZE = 256
};

/* Registered as "sv_running"; original Win32 pointer is at 0x04927ed4. */
cvar_t *sv_running;

/* Source: CoDUOMP.exe 0x004019a0..0x00401b36.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004019a0_00401b37.mcode.
 * Name: exact same-module Mac symbol CL_SetExpectedHunkUsage. The Windows
 * compiler emitted the same source structure for SV_SetExpectedHunkUsage at
 * 0x0045f5c0, including the zeroed allocation and -1 fallback. */
void CL_SetExpectedHunkUsage(const char *mapBspPath)
{
    int32_t fileHandle;
    const int32_t fileLength = FS_FOpenFileByMode("hunkusage.dat", &fileHandle, FS_READ);
    if (fileLength < 0) {
        (void)Cvar_Set2("com_expectedhunkusage", "-1", qtrue);
        return;
    }

    char *fileBuffer = Z_MallocInternal((size_t)fileLength + 1);
    memset(fileBuffer, 0, (size_t)fileLength + 1);
    (void)FS_Read(fileBuffer, fileLength, fileHandle);
    FS_FCloseFile(fileHandle);

    char *parseData = fileBuffer;
    for (;;) {
        const char *token = Com_Parse(&parseData);
        if (token == NULL || token[0] == '\0') {
            Z_FreeInternal(fileBuffer);
            (void)Cvar_Set2("com_expectedhunkusage", "-1", qtrue);
            return;
        }

        if (Q_strcasecmp(token, mapBspPath) == 0) {
            token = Com_Parse(&parseData);
            if (token != NULL && token[0] != '\0') {
                (void)Cvar_Set2("com_expectedhunkusage", token, qtrue);
                Z_FreeInternal(fileBuffer);
                return;
            }
        }
    }
}

/* Source: CoDUOMP.exe 0x00401b40..0x00401b66.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00401b40_00401b67.mcode.
 * Name: exact same-module Mac symbol CL_CM_LoadMap. The cgame syscall
 * dispatcher at 0x00402746 proves the map-name argument. */
void CL_CM_LoadMap(const char *mapBspPath)
{
    int32_t checksum;

    if (sv_running->integer == 0)
        CL_SetExpectedHunkUsage(mapBspPath);
    CM_LoadMap(mapBspPath, qtrue, &checksum);
}

/* Source: CoDUOMP.exe 0x00413bb0..0x00413c29.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00413bb0_00413c2a.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_InitRenderer. Renderer registration supplies the active video dimensions;
 * the console input field then adopts the original 32-pixel horizontal margin
 * and the existing 8x16 glyph metrics. */
void CL_InitRenderer(void)
{
    enum {
        CL_RENDERER_SHADER_LOAD_MODE = 2,
        CL_CONSOLE_HORIZONTAL_MARGIN = 32
    };

    rendererExports.BeginRegistration(&cls.rendererConfig);
    coduomp_scr_reset_widescreen_backdrop_compat();
    cls.whiteShader = rendererExports.RegisterShader("white", CL_RENDERER_SHADER_LOAD_MODE);
    cls.consoleShader = rendererExports.RegisterShader("console", CL_RENDERER_SHADER_LOAD_MODE);

    con_fieldWidthPixels = cls.rendererConfig.vidWidth - CL_CONSOLE_HORIZONTAL_MARGIN;
    con_inputField.widthInPixels = con_fieldWidthPixels;
    con_inputField.charWidth = con_fieldCharWidth;
    con_inputField.charHeight = con_fieldCharHeight;
    StatMon_Reset();
    con_inputField.fixedSize = qtrue;
}

/* Source: CoDUOMP.exe 0x00401e50..0x00401f2e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00401e50_00401f2f.mcode.
 * Name and signature: exact same-module Mac symbol CL_SubtitlePrint. The
 * Win32 optimizer inlines the outer CL_ConsolePrint checks before calling
 * CL_ConsolePrint_AddLine; the Mac call graph retains CL_ConsolePrint. */
void CL_SubtitlePrint(const char *reference, int32_t timeMs, int32_t lineWidth)
{
    const char *text = reference;

    if (cl_languagetranslate != NULL && cl_languagetranslate->integer != 0 && reference[0] != '\0' && reference[1] != '\0') {
        text = SEH_StringEd_GetString(reference);
    }

    if (text == NULL) {
        if (cl_languagewarnings->integer != 0) {
            if (cl_languagewarningsaserrors->integer != 0) {
                Com_Error(ERR_LOCALIZATION, "Could not translate subtitle text: \"%s\"", reference);
            } else {
                Com_Printf("^3WARNING: Could not translate subtitle text: \"%s\"\n", reference);
            }
            text = va("^1UNLOCALIZED(^7%s^1)^7", reference);
        } else {
            text = reference;
        }
    }

    if (cl_noprint != NULL && cl_noprint->integer != 0) {
        return;
    }
    if (con.initialized == qfalse) {
        Con_OneTimeInit();
    }

    if (timeMs == 0) {
        timeMs = CL_SUBTITLE_DEFAULT_TIME_MSEC;
    } else if (timeMs < 0) {
        timeMs = 0;
    }

    CL_ConsolePrint(text, CON_DEST_SUBTITLE, timeMs, lineWidth);
}

/* Source: CoDUOMP.exe 0x00404ee0..0x00405306.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00404ee0_00405307.mcode.
 * Name: exact same-module Mac symbol CL_UpdateLevelHunkUsage. Existing entries
 * are copied as map/value pairs, except for the current map; an unchanged
 * current-map value returns without rewriting the file. */
void CL_UpdateLevelHunkUsage(void)
{
    static const char hunkUsagePath[] = "hunkusage.dat";
    int32_t fileHandle;
    const int32_t fileLength = FS_FOpenFileByMode(hunkUsagePath, &fileHandle, FS_READ);

    if (fileLength >= 0) {
        const int32_t bufferSize = fileLength + 1;
        char *fileBuffer = Z_MallocInternal((size_t)bufferSize);
        memset(fileBuffer, 0, (size_t)bufferSize);
        char *outputBuffer = Z_MallocInternal((size_t)bufferSize);
        memset(outputBuffer, 0, (size_t)bufferSize);

        (void)FS_Read(fileBuffer, fileLength, fileHandle);
        FS_FCloseFile(fileHandle);

        char *parseData = fileBuffer;
        outputBuffer[0] = '\0';
        for (;;) {
            const char *mapToken = Com_Parse(&parseData);
            if (mapToken == NULL || mapToken[0] == '\0') {
                break;
            }

            if (Q_strcasecmp(mapToken, cl.mapBspName) == 0) {
                const char *usageToken = Com_Parse(&parseData);
                if (usageToken != NULL && usageToken[0] != '\0' && coduo_crt_atoi(usageToken) == hunk_used) {
                    Z_FreeInternal(fileBuffer);
                    Z_FreeInternal(outputBuffer);
                    return;
                }
                continue;
            }

            Q_strcat(outputBuffer, bufferSize, mapToken);
            Q_strcat(outputBuffer, bufferSize, " ");

            const char *usageToken = Com_Parse(&parseData);
            if (usageToken == NULL || usageToken[0] == '\0') {
                Com_Error(ERR_DROP, "EXE_ERR_HUNGUSAGE_CORRUPT");
                continue;
            }
            Q_strcat(outputBuffer, bufferSize, usageToken);
            Q_strcat(outputBuffer, bufferSize, "\n");
        }

        fileHandle = FS_FOpenFileWrite(hunkUsagePath);
        if (fileHandle < 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            Com_Error(ERR_DROP, "EXE_ERR_CANT_CREATE\x15%s", hunkUsagePath);
        }

        const int32_t outputLength = (int32_t)strlen(outputBuffer);
        if (FS_Write(outputBuffer, outputLength, fileHandle) != outputLength) {
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            Com_Error(ERR_DROP, "EXE_ERR_CANT_WRITE\x15%s", hunkUsagePath);
        }
        FS_FCloseFile(fileHandle);
        Z_FreeInternal(fileBuffer);
        Z_FreeInternal(outputBuffer);
    }

    if (FS_FOpenFileByMode(hunkUsagePath, &fileHandle, FS_APPEND) < 0) {
        Com_Error(ERR_DROP, "EXE_ERR_HUNKUSAGE_CANT_WRITE");
    }

    char currentUsageLine[CL_HUNK_USAGE_LINE_SIZE];
    Com_sprintf(currentUsageLine, sizeof(currentUsageLine), "%s %i\n", cl.mapBspName, hunk_used);
    (void)FS_Write(currentUsageLine, (int32_t)strlen(currentUsageLine), fileHandle);
    FS_FCloseFile(fileHandle);

    if (FS_FOpenFileByMode(hunkUsagePath, &fileHandle, FS_READ) >= 0) {
        FS_FCloseFile(fileHandle);
    }
}

/* Source: CoDUOMP.exe 0x00401b70..0x00401c41.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00401b70_00401c42.mcode.
 * Name: exact same-module Mac symbol CL_ShutdownCGame. The optimized Win32
 * body inlines Com_UnloadSoundAliases and VM_Free after the two calls shown
 * by the Mac call graph. */
void CL_ShutdownCGame(void)
{
    Com_UnloadSoundAliases(SND_ALIAS_BANK_CGAME);
    cls.keyCatchers &= ~KEYCATCH_CGAME;

    if (coduo_cgameVm == NULL) {
        return;
    }

    (void)VM_Call(coduo_cgameVm, CGVM_SHUTDOWN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    VM_Free(coduo_cgameVm);
    coduo_cgameVm = NULL;
}

/* Source: CoDUOMP.exe 0x00405310..0x00405500.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00405310_00405501.mcode.
 * Name: exact same-module Mac symbol CL_InitCGame. The Mac call graph also
 * identifies the Win32-inlined XAnimSetUser and console-clear operations. */
void CL_InitCGame(void)
{
    const uint32_t startTime = Sys_Milliseconds();

    if (sv_running->integer == 0) {
        Com_InitDObj();
    }
    XAnimSetUser(XANIM_USER_CLIENT);
    Con_Close();

    const char *serverInfo = &cl.gameState.stringData[cl.gameState.stringOffsets[CS_SERVERINFO]];
    const char *mapName = Info_ValueForKey(serverInfo, "mapname");
    Com_sprintf(cl.mapBspName, sizeof(cl.mapBspName), "maps/mp/%s.bsp", mapName);

    coduo_cgameVm = VM_Create("cgame", CL_CgameSystemCalls);
    if (coduo_cgameVm == NULL) {
        Com_Error(ERR_DROP, "\x15VM_Create on cgame failed");
    }

    intptr_t apiVersion = VM_Call(coduo_cgameVm, CGVM_GET_API_VERSION, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (apiVersion != CGVM_API_VERSION) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "cgame is version %d, expected %d",
                  (int32_t)apiVersion, CGVM_API_VERSION);
    }

    cls.state = CA_LOADING;
    (void)VM_Call(coduo_cgameVm, CGVM_SCRIPT_FAR_HOOK, (intptr_t)Scr_NearHook(NULL), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    (void)VM_Call(coduo_cgameVm, CGVM_INIT, clc.serverMessageSequence, clc.lastExecutedServerCommand, clc.clientNum, 0, 0, 0, 0, 0, 0, 0, 0,
                  0);
    cls.state = CA_PRIMED;

    /* Win32 stores the wrapping subtraction in a dword and loads it with FILD,
     * so the elapsed value is interpreted as signed before conversion. */
    const int32_t elapsedMilliseconds = (int32_t)(Sys_Milliseconds() - startTime);
    Com_Printf("CL_InitCGame: %5.2f seconds\n", (double)elapsedMilliseconds * 0.001);

    rendererExports.EndRegistration();
    if (Sys_LowPhysicalMemory() == qfalse) {
        Com_TouchMemory();
    }
    Con_ClearNotify();
    Con_ClearSubtitles();
    CL_UpdateLevelHunkUsage();
}
