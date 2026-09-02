#include "ui_module_abi.h"

#include <string.h>

// Source: uo_ui_mp_x86.dll 0x4001d250..0x4001d25d
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d250_4001d25d.mcode
// The Com_Error caller at 0x40007760 independently proves syscall id 0.
void trap_Error(const char *message)
{
    ui_syscall(UI_ERROR, (intptr_t)message);
}

// Source: uo_ui_mp_x86.dll 0x4001d240..0x4001d24d
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d240_4001d24d.mcode
// The Com_Printf caller at 0x400077c0 independently proves syscall id 1.
void trap_Print(const char *message)
{
    ui_syscall(UI_PRINT, (intptr_t)message);
}

// Source: uo_ui_mp_x86.dll 0x4001d260..0x4001d26d
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d260_4001d26d.mcode
// Exact same-module PPC symbol: trap_GetLanguagename.
const char *trap_GetLanguagename(int32_t languageIndex)
{
    return (const char *)ui_syscall(UI_GET_LANGUAGE_NAME,
                                    (intptr_t)languageIndex);
}

// Source: uo_ui_mp_x86.dll 0x4001d270..0x4001d27d
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d270_4001d27d.mcode
// Exact same-module PPC symbol: trap_VerifyLanguageSelection.
int32_t trap_VerifyLanguageSelection(int32_t languageIndex)
{
    return (int32_t)ui_syscall(UI_VERIFY_LANGUAGE_SELECTION,
                               (intptr_t)languageIndex);
}

// Source: uo_ui_mp_x86.dll 0x4001d280..0x4001d28c
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d280_4001d28c.mcode
// Same-module PPC symbol and wrapper order: trap_Milliseconds.
int32_t trap_Milliseconds(void)
{
    return (int32_t)ui_syscall(UI_MILLISECONDS);
}

// Source: uo_ui_mp_x86.dll 0x4001d290..0x4001d2a4
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d290_4001d2a4.mcode
void trap_Cvar_Register(vmCvar_t *vmCvar, const char *name,
                        const char *defaultValue, int32_t flags)
{
    ui_syscall(UI_CVAR_REGISTER, (intptr_t)vmCvar, (intptr_t)name,
               (intptr_t)defaultValue, (intptr_t)flags);
}

// Source: uo_ui_mp_x86.dll 0x4001d2b0..0x4001d2bd
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d2b0_4001d2bd.mcode
void trap_Cvar_Update(vmCvar_t *vmCvar)
{
    ui_syscall(UI_CVAR_UPDATE, (intptr_t)vmCvar);
}

// Source: uo_ui_mp_x86.dll 0x4001d2c0..0x4001d2d6
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d2c0_4001d2d6.mcode
void trap_Cvar_Set(const char *name, const char *value)
{
    ui_syscall(UI_CVAR_SET, (intptr_t)name, (intptr_t)value);
}

// Source: uo_ui_mp_x86.dll 0x4001d2e0..0x4001d2fa
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d2e0_4001d2fa.mcode
float trap_Cvar_VariableValue(const char *name)
{
    int32_t bits = (int32_t)ui_syscall(UI_CVAR_VARIABLE_VALUE, (intptr_t)name);
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

// Source: uo_ui_mp_x86.dll 0x4001d300..0x4001d31b
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d300_4001d31b.mcode
void trap_Cvar_VariableStringBuffer(const char *name, char *buffer,
                                    int32_t bufferSize)
{
    ui_syscall(UI_CVAR_VARIABLE_STRING_BUFFER, (intptr_t)name,
               (intptr_t)buffer, (intptr_t)bufferSize);
}

// Source: uo_ui_mp_x86.dll 0x4001d320..0x4001d338
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d320_4001d338.mcode
void trap_Cvar_SetValue(const char *name, float value)
{
    ui_syscall(UI_CVAR_SET_VALUE, (intptr_t)name, (intptr_t)PASSFLOAT(value));
}

// Source: uo_ui_mp_x86.dll 0x4001d390..0x4001d3a6
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d390_4001d3a6.mcode
// Same-module PPC symbol: trap_Cmd_ExecuteText.
void trap_Cmd_ExecuteText(cbufExec_t execWhen, const char *text)
{
    ui_syscall(UI_CMD_EXECUTE_TEXT, (intptr_t)execWhen, (intptr_t)text);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d3b0..0x4001d3bf,
// command 0x11.
int32_t trap_FS_FOpenFile(const char *filename, int32_t *handle, fsMode_t mode)
{
    return (int32_t)ui_syscall(UI_FS_FOPEN_FILE, (intptr_t)filename,
                               (intptr_t)handle, (intptr_t)mode);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d3c0..0x4001d3cf,
// command 0x12.
void trap_FS_Read(void *buffer, int32_t length, int32_t handle)
{
    ui_syscall(UI_FS_READ, (intptr_t)buffer, (intptr_t)length,
               (intptr_t)handle);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d3f0..0x4001d3fd,
// command 0x15.
void trap_FS_FCloseFile(int32_t handle)
{
    ui_syscall(UI_FS_FCLOSE_FILE, (intptr_t)handle);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d400..0x4001d414,
// command 0x16.
int32_t trap_FS_GetFileList(const char *path, const char *extension,
                            char *buffer, int32_t bufferSize)
{
    return (int32_t)ui_syscall(UI_FS_GET_FILE_LIST, (intptr_t)path,
                               (intptr_t)extension, (intptr_t)buffer,
                               (intptr_t)bufferSize);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d5c0..0x4001d5d6,
// command 0x19. Exact same-module PPC symbol:
// trap_R_RegisterShaderNoMip.
qhandle_t trap_R_RegisterShaderNoMip(const char *name, int32_t loadMode)
{
    return (qhandle_t)ui_syscall(UI_R_REGISTER_SHADER_NO_MIP,
                                 (intptr_t)name, (intptr_t)loadMode);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d7a0..0x4001d7b1,
// command 0x26.
const char *trap_S_RegisterSound(const char *name)
{
    return (const char *)ui_syscall(UI_S_REGISTER_SOUND, (intptr_t)name);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d450..0x4001d470,
// command 0x42.
void trap_R_RegisterFont(const char *name, int32_t pointSize,
                         fontInfo_t *fontStorage,
                         intptr_t context)
{
    ui_syscall(UI_R_REGISTER_FONT, (intptr_t)name, (intptr_t)pointSize,
               (intptr_t)fontStorage, context);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d470..0x4001d496,
// command 0x43. The scale travels as its raw 32-bit float word.
int32_t trap_R_Text_Width(const char *text, int32_t font, float scale,
                          int32_t limit)
{
    return (int32_t)ui_syscall(UI_R_TEXT_WIDTH, (intptr_t)text,
                               (intptr_t)font, (intptr_t)PASSFLOAT(scale),
                               (intptr_t)limit);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d4c0..0x4001d519,
// command 0x45. Float arguments are transported as raw 32-bit words.
void trap_R_Text_Paint(float x, float y, int32_t font, float scale,
                       const vec4_t color, const char *text,
                       float fixedAdvance, int32_t limit,
                       int32_t textStyle)
{
    ui_syscall(UI_R_TEXT_PAINT, (intptr_t)PASSFLOAT(x),
               (intptr_t)PASSFLOAT(y), (intptr_t)font,
               (intptr_t)PASSFLOAT(scale), (intptr_t)color,
               (intptr_t)text, (intptr_t)PASSFLOAT(fixedAdvance),
               (intptr_t)limit,
               (intptr_t)textStyle);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d8e0..0x4001d8ed,
// command 0x33. Exact same-module PPC symbol: trap_GetClientState.
void trap_GetClientState(uiClientState_t *state)
{
    ui_syscall(UI_GET_CLIENT_STATE, (intptr_t)state);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d8f0..0x4001d8fd,
// command 0x34. Exact same-module PPC symbol: trap_GetGlconfig.
void trap_GetGlconfig(glconfig_t *config)
{
    ui_syscall(UI_GET_GLCONFIG, (intptr_t)config);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d900..0x4001d90f,
// command 0x35. Exact same-module PPC symbol: trap_GetConfigString.
qboolean trap_GetConfigString(int32_t index, char *buffer,
                              int32_t bufferSize)
{
    return (qboolean)ui_syscall(UI_GET_CONFIG_STRING, (intptr_t)index,
                                (intptr_t)buffer, (intptr_t)bufferSize);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d910..0x4001d91f,
// command 0x36. Exact same-module PPC symbol: trap_GetClientName.
qboolean trap_GetClientName(int32_t clientNum, char *buffer,
                            int32_t bufferSize)
{
    return (qboolean)ui_syscall(UI_GET_CLIENT_NAME, (intptr_t)clientNum,
                                (intptr_t)buffer, (intptr_t)bufferSize);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d7c0..0x4001d7d1,
// command 0x27. Exact same-module PPC symbol: trap_MSS_PlayLocalSoundAlias.
void trap_MSS_PlayLocalSoundAlias(const char *aliasName)
{
    ui_syscall(UI_MSS_PLAY_LOCAL_SOUND_ALIAS, (intptr_t)aliasName);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d9e0..0x4001d9f9,
// command 0x64. Exact same-module PPC symbol: trap_LAN_CompareServers.
int32_t trap_LAN_CompareServers(int32_t source, int32_t sortKey,
                                int32_t sortDirection, int32_t server1,
                                int32_t server2)
{
    return (int32_t)ui_syscall(UI_LAN_COMPARE_SERVERS, (intptr_t)source,
                               (intptr_t)sortKey, (intptr_t)sortDirection,
                               (intptr_t)server1, (intptr_t)server2);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001db90..0x4001dbae,
// command 0x5a.
int32_t trap_CIN_PlayCinematic(const char *name, int32_t x, int32_t y,
                               int32_t width, int32_t height, int32_t flags)
{
    return (int32_t)ui_syscall(UI_CIN_PLAY_CINEMATIC, (intptr_t)name,
                               (intptr_t)x, (intptr_t)y, (intptr_t)width,
                               (intptr_t)height, (intptr_t)flags);
}

// Source: uo_ui_mp_x86.dll 0x4001dbb0..0x4001dbbd
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001dbb0_4001dbbd.mcode
// Exact same-module PPC symbol: trap_CIN_StopCinematic.
void trap_CIN_StopCinematic(int32_t handle)
{
    ui_syscall(UI_CIN_STOP_CINEMATIC, (intptr_t)handle);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001dbc0..0x4001dbcd,
// command 0x5c.
void trap_CIN_RunCinematic(int32_t handle)
{
    ui_syscall(UI_CIN_RUN_CINEMATIC, (intptr_t)handle);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001dbd0..0x4001dbdd,
// command 0x5d.
void trap_CIN_DrawCinematic(int32_t handle)
{
    ui_syscall(UI_CIN_DRAW_CINEMATIC, (intptr_t)handle);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001dbe0..0x4001dbf9,
// command 0x5e.
void trap_CIN_SetExtents(int32_t handle, int32_t x, int32_t y,
                         int32_t width, int32_t height)
{
    ui_syscall(UI_CIN_SET_EXTENTS, (intptr_t)handle, (intptr_t)x,
               (intptr_t)y, (intptr_t)width, (intptr_t)height);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d6e0..0x4001d6ed,
// command 0x21.
void trap_R_SetColor(const vec4_t rgba)
{
    ui_syscall(UI_R_SET_COLOR, (intptr_t)rgba);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d6f0..0x4001d769,
// command 0x22. The adapter transports all floats as raw 32-bit words.
void trap_R_DrawStretchPic(float x, float y, float width, float height,
                           float s0, float t0, float s1, float t1,
                           qhandle_t shader)
{
    ui_syscall(UI_R_DRAW_STRETCH_PIC,
               (intptr_t)PASSFLOAT(x), (intptr_t)PASSFLOAT(y),
               (intptr_t)PASSFLOAT(width), (intptr_t)PASSFLOAT(height),
               (intptr_t)PASSFLOAT(s0), (intptr_t)PASSFLOAT(t0),
               (intptr_t)PASSFLOAT(s1), (intptr_t)PASSFLOAT(t1),
               (intptr_t)shader);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d790..0x4001d79a,
// command 0x24.
void trap_UpdateScreen(void)
{
    ui_syscall(UI_UPDATE_SCREEN);
}

// Source: uo_ui_mp_x86.dll 0x4001d580..0x4001d591
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d580_4001d591.mcode
// Exact same-module PPC symbol: trap_SE_TranslateReference.
const char *trap_SE_TranslateReference(const char *reference)
{
    return (const char *)ui_syscall(UI_SE_TRANSLATE_REFERENCE,
                                    (intptr_t)reference);
}

// Engine syscall adapter: uo_ui_mp_x86.dll 0x4001d5a0..0x4001d5b6,
// command 0x48. Exact same-module PPC symbol: trap_SE_LocalizeMessage.
const char *trap_SE_LocalizeMessage(const char *message,
                                    const char *context)
{
    return (const char *)ui_syscall(UI_SE_LOCALIZE_MESSAGE,
                                    (intptr_t)message,
                                    (intptr_t)context);
}

// Source: uo_ui_mp_x86.dll 0x4001db40..0x4001db4d
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001db40_4001db4d.mcode
// Exact same-module PPC symbol: trap_PC_LoadSource.
int32_t trap_PC_LoadSource(const char *filename)
{
    return (int32_t)ui_syscall(UI_PC_LOAD_SOURCE, (intptr_t)filename);
}

// Source: uo_ui_mp_x86.dll 0x4001db50..0x4001db5d
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001db50_4001db5d.mcode
// Exact same-module PPC symbol: trap_PC_FreeSource.
void trap_PC_FreeSource(int32_t handle)
{
    ui_syscall(UI_PC_FREE_SOURCE, (intptr_t)handle);
}

// Source: uo_ui_mp_x86.dll 0x4001db60..0x4001db6e
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001db60_4001db6e.mcode
// Exact same-module PPC symbol: trap_PC_ReadToken.
qboolean trap_PC_ReadToken(int32_t handle, pc_token_t *token)
{
    return (qboolean)ui_syscall(UI_PC_READ_TOKEN, (intptr_t)handle,
                                (intptr_t)token);
}

// Source: uo_ui_mp_x86.dll 0x4001db70..0x4001db7f
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001db70_4001db7f.mcode
// Exact same-module PPC symbol: trap_PC_SourceFileAndLine.
void trap_PC_SourceFileAndLine(int32_t handle, char *filename,
                              int32_t *line)
{
    ui_syscall(UI_PC_SOURCE_FILE_AND_LINE, (intptr_t)handle,
               (intptr_t)filename, (intptr_t)line);
}

// Source: uo_ui_mp_x86.dll 0x4001d9b0..0x4001d9bd
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d9b0_4001d9bd.mcode
// Same-module PPC symbol: trap_LAN_UpdateDirtyPings.
qboolean trap_LAN_UpdateDirtyPings(int32_t source)
{
    return (qboolean)ui_syscall(UI_LAN_UPDATE_DIRTY_PINGS,
                                (intptr_t)source);
}

// Source: uo_ui_mp_x86.dll 0x4001d9c0..0x4001d9cd
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d9c0_4001d9cd.mcode
// Same-module PPC symbol: trap_LAN_GetServerCount.
int32_t trap_LAN_GetServerCount(int32_t source)
{
    return (int32_t)ui_syscall(UI_LAN_GET_SERVER_COUNT, (intptr_t)source);
}

// Source: uo_ui_mp_x86.dll 0x4001da90..0x4001da9f
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001da90_4001da9f.mcode
// Exact same-module PPC symbol: trap_LAN_ServerStatus.
qboolean trap_LAN_ServerStatus(const char *address, char *status,
                               int32_t statusSize)
{
    return (qboolean)ui_syscall(UI_LAN_SERVER_STATUS, (intptr_t)address,
                                (intptr_t)status, (intptr_t)statusSize);
}

// NOT_FROM_ORIGINAL_SOURCE: typed adapter for the direct command-82 call sites.
void ui_compat_lan_get_server_info(int32_t source, int32_t server,
                                   char *buffer, int32_t bufferSize)
{
    ui_syscall(UI_LAN_GET_SERVER_INFO, (intptr_t)source, (intptr_t)server,
               (intptr_t)buffer, (intptr_t)bufferSize);
}

// NOT_FROM_ORIGINAL_SOURCE: typed adapter for the direct command-81 call sites.
void ui_compat_lan_get_server_address(int32_t source, int32_t server,
                                      char *buffer, int32_t bufferSize)
{
    ui_syscall(UI_LAN_GET_SERVER_ADDRESS_STRING, (intptr_t)source,
               (intptr_t)server, (intptr_t)buffer, (intptr_t)bufferSize);
}

// Source: uo_ui_mp_x86.dll 0x4001da40..0x4001da4f
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001da40_4001da4f.mcode
// Exact same-module PPC symbol: trap_LAN_AddServer.
int32_t trap_LAN_AddServer(int32_t source, const char *name,
                           const char *address)
{
    return (int32_t)ui_syscall(UI_LAN_ADD_SERVER, (intptr_t)source,
                               (intptr_t)name, (intptr_t)address);
}

// Source: uo_ui_mp_x86.dll 0x4001d9d0..0x4001d9dd
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d9d0_4001d9dd.mcode
// Same-module PPC symbol: trap_LAN_WaitServerResponse.
qboolean trap_LAN_WaitServerResponse(int32_t source)
{
    return (qboolean)ui_syscall(UI_LAN_WAIT_SERVER_RESPONSE,
                                (intptr_t)source);
}

// Source: uo_ui_mp_x86.dll 0x4001d8a0..0x4001d8aa
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d8a0_4001d8aa.mcode
// Exact same-module PPC symbol: trap_Key_ClearStates.
void trap_Key_ClearStates(void)
{
    ui_syscall(UI_KEY_CLEAR_STATES);
}

// Source: uo_ui_mp_x86.dll 0x4001d8b0..0x4001d8bc
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d8b0_4001d8bc.mcode
// Exact same-module PPC symbol: trap_Key_GetCatcher.
int32_t trap_Key_GetCatcher(void)
{
    return (int32_t)ui_syscall(UI_KEY_GET_CATCHER);
}

// Source: uo_ui_mp_x86.dll 0x4001d8c0..0x4001d8cd
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d8c0_4001d8cd.mcode
// Exact same-module PPC symbol: trap_Key_SetCatcher.
void trap_Key_SetCatcher(int32_t catcher)
{
    ui_syscall(UI_KEY_SET_CATCHER, (intptr_t)catcher);
}

// Source: uo_ui_mp_x86.dll 0x4001dae0..0x4001daed
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001dae0_4001daed.mcode
// Same-module PPC symbol: trap_LAN_ResetPings.
void trap_LAN_ResetPings(int32_t source)
{
    ui_syscall(UI_LAN_RESET_PINGS, (intptr_t)source);
}

// Source: uo_ui_mp_x86.dll 0x4001dac0..0x4001dacf
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001dac0_4001dacf.mcode
// Same-module PPC symbol: trap_LAN_MarkServerDirty.
void trap_LAN_MarkServerDirty(int32_t source, int32_t server,
                              qboolean dirty)
{
    ui_syscall(UI_LAN_MARK_SERVER_DIRTY, (intptr_t)source,
               (intptr_t)server, (intptr_t)dirty);
}

// Source: uo_ui_mp_x86.dll 0x4001da60..0x4001da6e
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001da60_4001da6e.mcode
// Exact same-module PPC symbol: trap_LAN_GetServerPing.
int32_t trap_LAN_GetServerPing(int32_t source, int32_t server)
{
    return (int32_t)ui_syscall(UI_LAN_GET_SERVER_PING, (intptr_t)source,
                               (intptr_t)server);
}

// Source: uo_ui_mp_x86.dll 0x4001da80..0x4001da8e
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001da80_4001da8e.mcode
// Exact same-module PPC symbol: trap_LAN_ServerIsDirty.
qboolean trap_LAN_ServerIsDirty(int32_t source, int32_t server)
{
    return (qboolean)ui_syscall(UI_LAN_SERVER_IS_DIRTY,
                                (intptr_t)source, (intptr_t)server);
}

// Source: uo_ui_mp_x86.dll 0x4001db80..0x4001db8d
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001db80_4001db8d.mcode
// Same-module PPC symbol: trap_RealTime.
int32_t trap_RealTime(qtime_t *time)
{
    return (int32_t)ui_syscall(UI_REAL_TIME, (intptr_t)time);
}

// Source: uo_ui_mp_x86.dll 0x4001daa0..0x4001daaa
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001daa0_4001daaa.mcode
// Same-module PPC symbol: trap_LAN_SaveCachedServers.
void trap_LAN_SaveCachedServers(void)
{
    ui_syscall(UI_LAN_SAVE_CACHED_SERVERS);
}

// Source: uo_ui_mp_x86.dll 0x4001d380..0x4001d38f
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d380_4001d38f.mcode
// The UI_Argv caller at 0x40007870 independently proves syscall id 15 and the
// index, buffer, buffer-size argument order; PPC supplies the trap_Argv name.
void trap_Argv(int32_t index, char *buffer, int32_t bufferSize)
{
    ui_syscall(UI_ARGV, (intptr_t)index, (intptr_t)buffer,
               (intptr_t)bufferSize);
}
