#include "qcommon/precompiler.h"
#include "ui_module_loader.h"

#include "../client/cgame.h"
#include "../client/cinematic.h"
#include "../client/console.h"
#include "../client/server_browser.h"
#include "filesystem/filesystem.h"
#include "../localization/string_ed_api.h"
#include "qcommon/hunk.h"
#include "../renderer/renderer_api.h"
#include "../sound/sound_system.h"
#include "sound/alias/sound_alias.h"
#include "ui_client_state.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define UI_ARG(index) (arguments[(index)])
#define UI_INT(index) ((int32_t)UI_ARG(index))
#define UI_SIZE(index) ((size_t)UI_ARG(index))
#define UI_PTR(type, index) ((type *)(uintptr_t)UI_ARG(index))
#define UI_CONST_PTR(type, index) \
    ((const type *)(uintptr_t)UI_ARG(index))
#define UI_STRING(index) ((const char *)(uintptr_t)UI_ARG(index))

#define UI_SOUND_ALIAS_SELECTOR (-1.0f)
#define UI_TEXT_FIXED_ADVANCE (0.0f)

/* NOT_FROM_ORIGINAL_SOURCE: explicit source expression of the UI VM ABI's
 * four-byte float-bit transport. memcpy preserves the bit pattern without
 * depending on a compiler's inactive-union-member extension. */
_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 &&
                   FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128,
               "UI syscall float transport requires IEEE binary32");
static float CL_UISyscallFloatArgument(intptr_t argument)
{
    const uint32_t bits = (uint32_t)argument;
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: inverse of CL_UISyscallFloatArgument for the
 * math and cvar services that return a float through the integer VM word. */
static intptr_t CL_UISyscallFloatResult(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return (intptr_t)bits;
}

/* Source: CoDUOMP.exe 0x0041b6c0..0x0041c05a, with the compiler-generated
 * command jump table at 0x0041c05c..0x0041c228.
 * Name and signature: exact same-module Mac symbol CL_UISystemCalls. */
intptr_t CL_UISystemCalls(intptr_t *arguments)
{
    const uiImport_t syscall = (uiImport_t)UI_ARG(0);

    switch (syscall) {
    case UI_ERROR:
        Com_Error(ERR_DROP, "\x15%s", UI_STRING(1));
        return 0;
    case UI_PRINT:
        Com_Printf("%s", UI_STRING(1));
        return 0;
    case UI_GET_LANGUAGE_NAME:
        return (intptr_t)SEH_GetLanguageName(UI_INT(1));
    case UI_VERIFY_LANGUAGE_SELECTION:
        return SEH_UpdateCurrentLanguage(UI_INT(1));
    case UI_MILLISECONDS:
        return (intptr_t)Sys_Milliseconds();
    case UI_CVAR_REGISTER:
        Cvar_Register(UI_PTR(vmCvar_t, 1), UI_STRING(2),
                      UI_STRING(3), (uint32_t)UI_ARG(4));
        return 0;
    case UI_CVAR_UPDATE:
        Cvar_Update(UI_PTR(vmCvar_t, 1));
        return 0;
    case UI_CVAR_SET:
        Cvar_Set(UI_STRING(1), UI_STRING(2));
        return 0;
    case UI_CVAR_VARIABLE_VALUE:
        return CL_UISyscallFloatResult(Cvar_VariableValue(UI_STRING(1)));
    case UI_CVAR_VARIABLE_STRING_BUFFER:
        Cvar_VariableStringBuffer(
            UI_STRING(1), UI_PTR(char, 2), UI_INT(3));
        return 0;
    case UI_CVAR_SET_VALUE:
        Cvar_SetValue(
            UI_STRING(1), CL_UISyscallFloatArgument(UI_ARG(2)));
        return 0;
    case UI_CVAR_RESET:
        (void)Cvar_Set2(UI_STRING(1), NULL, qfalse);
        return 0;
    case UI_CVAR_CREATE:
        (void)Cvar_Get(UI_STRING(1), UI_STRING(2),
                       (uint32_t)UI_ARG(3));
        return 0;
    case UI_CVAR_INFO_STRING_BUFFER:
        Cvar_InfoStringBuffer(
            (uint32_t)UI_ARG(1), UI_PTR(char, 2), UI_INT(3));
        return 0;
    case UI_ARGC:
        return Cmd_Argc();
    case UI_ARGV:
        Cmd_ArgvBuffer(UI_INT(1), UI_PTR(char, 2), UI_INT(3));
        return 0;
    case UI_CMD_EXECUTE_TEXT:
        Cbuf_ExecuteText((cbufExec_t)UI_ARG(1), UI_STRING(2));
        return 0;
    case UI_FS_FOPEN_FILE:
        return FS_FOpenFileByMode(
            UI_STRING(1), UI_PTR(int32_t, 2), (fsMode_t)UI_INT(3));
    case UI_FS_READ:
        (void)FS_Read(UI_PTR(void, 1), UI_INT(2), UI_INT(3));
        return 0;
    case UI_FS_SEEK:
        (void)FS_Seek(UI_INT(1), UI_INT(2), UI_INT(3));
        return 0;
    case UI_FS_WRITE:
        (void)FS_Write(UI_CONST_PTR(void, 1), UI_INT(2), UI_INT(3));
        return 0;
    case UI_FS_FCLOSE_FILE:
        FS_FCloseFile(UI_INT(1));
        return 0;
    case UI_FS_GET_FILE_LIST:
        return FS_GetFileList(
            UI_STRING(1), UI_STRING(2), UI_PTR(char, 3), UI_INT(4));
    case UI_FS_DELETE:
        return FS_Delete(UI_STRING(1));
    case UI_R_REGISTER_MODEL:
        return rendererExports.RegisterModel(UI_STRING(1), UI_INT(2));
    case UI_R_REGISTER_SHADER_NO_MIP:
        /* The original command-25 jump-table target at 0x0041b8eb calls
         * renderer export slot 5 (0x049580b4), RegisterShaderNoMip. */
        return rendererExports.RegisterShaderNoMip(UI_STRING(1), UI_INT(2));
    case UI_R_CLEAR_SCENE:
        rendererExports.ClearScene();
        return 0;
    case UI_R_ADD_REF_ENTITY:
        rendererExports.AddRefEntityToScene(
            UI_CONST_PTR(refEntity_t, 1), NULL);
        return 0;
    case UI_R_ADD_POLY_TO_SCENE:
        rendererExports.AddPolyToScene(
            UI_INT(1), UI_INT(2), UI_CONST_PTR(polyVert_t, 3));
        return 0;
    case UI_R_ADD_POLYS_TO_SCENE:
        rendererExports.AddPolysToScene(
            UI_INT(1), UI_INT(2), UI_CONST_PTR(polyVert_t, 3),
            UI_INT(4));
        return 0;
    case UI_R_ADD_LIGHT_TO_SCENE:
        rendererExports.AddLightToScene(
            UI_CONST_PTR(float, 1),
            CL_UISyscallFloatArgument(UI_ARG(2)),
            CL_UISyscallFloatArgument(UI_ARG(3)),
            CL_UISyscallFloatArgument(UI_ARG(4)),
            CL_UISyscallFloatArgument(UI_ARG(5)));
        return 0;
    case UI_R_ADD_CORONA_TO_SCENE:
        rendererExports.AddCoronaToScene(
            UI_CONST_PTR(float, 1),
            CL_UISyscallFloatArgument(UI_ARG(2)),
            CL_UISyscallFloatArgument(UI_ARG(3)),
            CL_UISyscallFloatArgument(UI_ARG(4)),
            CL_UISyscallFloatArgument(UI_ARG(5)), UI_INT(6),
            qtrue);
        return 0;
    case UI_R_RENDER_SCENE:
        rendererExports.RenderScene(UI_CONST_PTR(refdef_t, 1));
        return 0;
    case UI_R_SET_COLOR:
        rendererExports.SetColor(UI_CONST_PTR(float, 1));
        return 0;
    case UI_R_DRAW_STRETCH_PIC:
        rendererExports.StretchPic(
            CL_UISyscallFloatArgument(UI_ARG(1)),
            CL_UISyscallFloatArgument(UI_ARG(2)),
            CL_UISyscallFloatArgument(UI_ARG(3)),
            CL_UISyscallFloatArgument(UI_ARG(4)),
            CL_UISyscallFloatArgument(UI_ARG(5)),
            CL_UISyscallFloatArgument(UI_ARG(6)),
            CL_UISyscallFloatArgument(UI_ARG(7)),
            CL_UISyscallFloatArgument(UI_ARG(8)), UI_INT(9));
        return 0;
    case UI_R_MODEL_BOUNDS:
        rendererExports.ModelBounds(
            UI_INT(1), UI_PTR(float, 2), UI_PTR(float, 3));
        return 0;
    case UI_UPDATE_SCREEN:
        SCR_UpdateScreen();
        return 0;
    case UI_S_REGISTER_SOUND: {
        snd_alias_t *const alias = Com_FindSoundAlias(
            UI_STRING(1), SND_ALIAS_BANK_COMMON,
            UI_SOUND_ALIAS_SELECTOR);
        return alias != NULL ? (intptr_t)alias->aliasName : 0;
    }
    case UI_MSS_PLAY_LOCAL_SOUND_ALIAS:
        (void)MSS_PlayLocalSoundAlias(
            UI_STRING(1), SND_ALIAS_BANK_COMMON);
        return 0;
    case UI_MSS_FADE_ALL_SOUNDS:
        MSS_FadeAllSounds(
            CL_UISyscallFloatArgument(UI_ARG(1)), UI_INT(2));
        return 0;
    case UI_KEY_KEYNUM_TO_STRING_BUF:
        Key_KeynumToStringBuf(
            UI_INT(1), UI_PTR(char, 2), UI_INT(3));
        return 0;
    case UI_KEY_GET_BINDING_BUF:
        Key_GetBindingBuf(
            UI_INT(1), UI_PTR(char, 2), UI_INT(3));
        return 0;
    case UI_KEY_SET_BINDING:
        Key_SetBinding(UI_INT(1), UI_STRING(2));
        return 0;
    case UI_KEY_IS_DOWN:
        return Key_IsDown(UI_INT(1));
    case UI_KEY_GET_OVERSTRIKE_MODE:
        return key_overstrikeMode;
    case UI_KEY_SET_OVERSTRIKE_MODE:
        key_overstrikeMode = (qboolean)UI_ARG(1);
        return 0;
    case UI_KEY_CLEAR_STATES:
        Key_ClearStates();
        return 0;
    case UI_KEY_GET_CATCHER:
        return cls.keyCatchers;
    case UI_KEY_SET_CATCHER:
        Key_SetCatcher(UI_INT(1));
        return 0;
    case UI_GET_CLIPBOARD_DATA:
        GetClipboardDataUI(UI_PTR(char, 1), UI_INT(2));
        return 0;
    case UI_GET_CLIENT_STATE:
        GetClientState(UI_PTR(uiClientState_t, 1));
        return 0;
    case UI_GET_GLCONFIG:
        CL_GetGlconfig(UI_PTR(glconfig_t, 1));
        return 0;
    case UI_GET_CONFIG_STRING:
        return GetConfigString(
            UI_INT(1), UI_PTR(char, 2), UI_INT(3));
    case UI_GET_CLIENT_NAME:
        return GetClientname(
            UI_INT(1), UI_PTR(char, 2), UI_INT(3));
    case UI_LAN_GET_PING_QUEUE_COUNT:
        return LAN_GetPingQueueCount();
    case UI_LAN_CLEAR_PING:
        LAN_ClearPing(UI_INT(1));
        return 0;
    case UI_LAN_GET_PING:
        LAN_GetPing(
            UI_INT(1), UI_PTR(char, 2), UI_INT(3),
            UI_PTR(int32_t, 4));
        return 0;
    case UI_LAN_GET_PING_INFO:
        LAN_GetPingInfo(
            UI_INT(1), UI_PTR(char, 2), UI_INT(3));
        return 0;
    case UI_MEMORY_REMAINING:
        return (intptr_t)Hunk_MemoryRemaining();
    case UI_GET_CD_KEY:
        CLUI_GetCDKey(
            UI_PTR(char, 1), UI_INT(2), UI_PTR(char, 3));
        return 0;
    case UI_SET_CD_KEY:
        CLUI_SetCDKey(UI_STRING(1), UI_STRING(2));
        return 0;
    case UI_R_REGISTER_FONT:
        rendererExports.RegisterFont(
            UI_STRING(1), UI_INT(2), UI_PTR(fontInfo_t, 3),
            UI_INT(4));
        return 0;
    case UI_R_TEXT_WIDTH:
        return rendererExports.TextWidth(
            UI_STRING(1), UI_INT(2),
            CL_UISyscallFloatArgument(UI_ARG(3)),
            UI_TEXT_FIXED_ADVANCE, UI_INT(4));
    case UI_R_TEXT_HEIGHT:
        return rendererExports.TextHeight(
            UI_INT(1), CL_UISyscallFloatArgument(UI_ARG(2)));
    case UI_R_TEXT_PAINT:
        rendererExports.TextPaint(
            CL_UISyscallFloatArgument(UI_ARG(1)),
            CL_UISyscallFloatArgument(UI_ARG(2)), UI_INT(3),
            CL_UISyscallFloatArgument(UI_ARG(4)),
            UI_CONST_PTR(float, 5), UI_STRING(6),
            CL_UISyscallFloatArgument(UI_ARG(7)), UI_INT(8),
            UI_INT(9));
        return 0;
    case UI_R_TEXT_PAINT_WITH_CURSOR:
        rendererExports.TextPaintWithCursor(
            CL_UISyscallFloatArgument(UI_ARG(1)),
            CL_UISyscallFloatArgument(UI_ARG(2)), UI_INT(3),
            CL_UISyscallFloatArgument(UI_ARG(4)),
            UI_CONST_PTR(float, 5), UI_STRING(6), UI_INT(7),
            (uint8_t)UI_ARG(8), UI_TEXT_FIXED_ADVANCE, UI_INT(9),
            UI_INT(10));
        return 0;
    case UI_SE_TRANSLATE_REFERENCE:
        return (intptr_t)SEH_StringEd_GetString(UI_STRING(1));
    case UI_SE_LOCALIZE_MESSAGE:
        return (intptr_t)SEH_LocalizeTextMessage(
            UI_STRING(1), UI_STRING(2), LOCMSG_SAFE);
    case UI_PC_ADD_GLOBAL_DEFINE:
        return PC_AddGlobalDefine(UI_STRING(1));
    case UI_PC_LOAD_SOURCE:
        return PC_LoadSourceHandle(UI_STRING(1));
    case UI_PC_FREE_SOURCE:
        return PC_FreeSourceHandle(UI_INT(1));
    case UI_PC_READ_TOKEN:
        return PC_ReadTokenHandle(
            UI_INT(1), UI_PTR(pc_token_t, 2));
    case UI_PC_SOURCE_FILE_AND_LINE:
        return PC_SourceFileAndLine(
            UI_INT(1), UI_PTR(char, 2), UI_PTR(int32_t, 3));
    case UI_REAL_TIME:
        return (intptr_t)Com_RealTime(UI_PTR(qtime_t, 1));
    case UI_LAN_GET_SERVER_COUNT:
        return LAN_GetServerCount((lan_server_source_t)UI_ARG(1));
    case UI_LAN_WAIT_SERVER_RESPONSE:
        return LAN_WaitServerResponse(
            (lan_server_source_t)UI_ARG(1));
    case UI_LAN_GET_SERVER_ADDRESS_STRING:
        LAN_GetServerAddressString(
            (lan_server_source_t)UI_ARG(1), UI_INT(2),
            UI_PTR(char, 3), UI_INT(4));
        return 0;
    case UI_LAN_GET_SERVER_INFO:
        LAN_GetServerInfo(
            (lan_server_source_t)UI_ARG(1), UI_INT(2),
            UI_PTR(char, 3), UI_INT(4));
        return 0;
    case UI_LAN_MARK_SERVER_DIRTY:
        LAN_MarkServerDirty(
            (lan_server_source_t)UI_ARG(1), UI_INT(2),
            (qboolean)UI_ARG(3));
        return 0;
    case UI_LAN_UPDATE_DIRTY_PINGS:
        return LAN_UpdateDirtyPings(
            (lan_server_source_t)UI_ARG(1));
    case UI_LAN_RESET_PINGS:
        LAN_ResetPings((lan_server_source_t)UI_ARG(1));
        return 0;
    case UI_LAN_LOAD_CACHED_SERVERS:
        LAN_LoadCachedServers();
        return 0;
    case UI_LAN_SAVE_CACHED_SERVERS:
        LAN_SaveServersToCache();
        return 0;
    case UI_LAN_ADD_SERVER:
        return LAN_AddServer(
            (lan_server_source_t)UI_ARG(1), UI_STRING(2),
            UI_STRING(3));
    case UI_LAN_REMOVE_SERVER:
        LAN_RemoveServer(
            (lan_server_source_t)UI_ARG(1), UI_STRING(2));
        return 0;
    case UI_CIN_PLAY_CINEMATIC:
        Com_DPrintf("UI_CIN_PlayCinematic\n");
        return CIN_PlayCinematic(
            UI_STRING(1), UI_INT(2), UI_INT(3), UI_INT(4),
            UI_INT(5), UI_INT(6));
    case UI_CIN_STOP_CINEMATIC:
        return CIN_StopCinematic(UI_INT(1));
    case UI_CIN_RUN_CINEMATIC:
        return CIN_RunCinematic(UI_INT(1));
    case UI_CIN_DRAW_CINEMATIC:
        CIN_DrawCinematic(UI_INT(1));
        return 0;
    case UI_CIN_SET_EXTENTS:
        CIN_SetExtents(
            UI_INT(1), UI_INT(2), UI_INT(3), UI_INT(4), UI_INT(5));
        return 0;
    case UI_VERIFY_CD_KEY:
        return CL_CDKeyValidate(UI_STRING(1), UI_STRING(2));
    case UI_LAN_SERVER_STATUS:
        return LAN_GetServerStatus(
            UI_STRING(1), UI_PTR(char, 2), UI_INT(3));
    case UI_LAN_GET_SERVER_PING:
        return LAN_GetServerPing(
            (lan_server_source_t)UI_ARG(1), UI_INT(2));
    case UI_LAN_SERVER_IS_PUNKBUSTER:
        return LAN_GetServerPunkBuster(
            (lan_server_source_t)UI_ARG(1), UI_INT(2));
    case UI_LAN_SERVER_IS_DIRTY:
        return LAN_ServerIsDirty(
            (lan_server_source_t)UI_ARG(1), UI_INT(2));
    case UI_LAN_COMPARE_SERVERS:
        return LAN_CompareServers(
            (lan_server_source_t)UI_ARG(1),
            (lan_server_sort_key_t)UI_ARG(2),
            (qboolean)UI_ARG(3), UI_INT(4), UI_INT(5));
    case UI_SET_PB_CLIENT_STATUS:
        CLUI_SetPbClStatus((qboolean)UI_ARG(1));
        return 0;
    case UI_GET_AUTO_UPDATE:
        CL_GetAutoUpdate();
        return 0;
    case UI_RUNNING_GAME:
        return cls.state == CA_ACTIVE ? qtrue : qfalse;
    case UI_MEMSET:
        return (intptr_t)memset(
            UI_PTR(void, 1), UI_INT(2), UI_SIZE(3));
    case UI_MEMCPY:
        return (intptr_t)memcpy(
            UI_PTR(void, 1), UI_CONST_PTR(void, 2), UI_SIZE(3));
    case UI_STRNCPY:
        return (intptr_t)strncpy(
            UI_PTR(char, 1), UI_STRING(2), UI_SIZE(3));
    case UI_SIN:
        return CL_UISyscallFloatResult(
            sinf(CL_UISyscallFloatArgument(UI_ARG(1))));
    case UI_COS:
        return CL_UISyscallFloatResult(
            cosf(CL_UISyscallFloatArgument(UI_ARG(1))));
    case UI_ATAN2:
        return CL_UISyscallFloatResult(
            atan2f(CL_UISyscallFloatArgument(UI_ARG(1)),
                   CL_UISyscallFloatArgument(UI_ARG(2))));
    case UI_SQRT:
        return CL_UISyscallFloatResult(
            sqrtf(CL_UISyscallFloatArgument(UI_ARG(1))));
    case UI_FLOOR:
        return CL_UISyscallFloatResult((float)floor(
            (double)CL_UISyscallFloatArgument(UI_ARG(1))));
    case UI_CEIL:
        return CL_UISyscallFloatResult((float)ceil(
            (double)CL_UISyscallFloatArgument(UI_ARG(1))));
    case UI_MALLOC:
        return (intptr_t)Z_MallocInternal(UI_SIZE(1));
    case UI_FREE:
        Z_FreeInternal(UI_PTR(void, 1));
        return 0;
    case UI_RESERVED_37:
    case UI_RESERVED_55:
    case UI_RESERVED_56:
    case UI_RESERVED_57:
    case UI_RESERVED_58:
    default:
        Com_Error(ERR_DROP, "\x15" "Bad UI system trap: %i",
                  (int32_t)syscall);
        return 0;
    }
}
