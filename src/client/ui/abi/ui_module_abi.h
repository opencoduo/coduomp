#ifndef CODUO_UI_MODULE_ABI_H
#define CODUO_UI_MODULE_ABI_H

#include <stdint.h>

#include "qcommon/filesystem_types.h"
#include "qcommon/precompiler_types.h"
#include "qcommon/q_renderer_types.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/qtime_types.h"
#include "qcommon/ui_module_abi_types.h"

#if defined(_WIN32)
#define UI_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define UI_EXPORT __attribute__((visibility("default")))
#else
#define UI_EXPORT
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
#define UI_ABI_CDECL __cdecl
#elif defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
#define UI_ABI_CDECL __attribute__((cdecl))
#else
#define UI_ABI_CDECL
#endif

#if UINTPTR_MAX == UINT32_MAX
typedef intptr_t(UI_ABI_CDECL *ui_syscall_t)(intptr_t command, ...);
#else
/*
 * NOT_FROM_ORIGINAL_SOURCE: native-width module syscalls use the argument
 * vector already consumed by CL_UISystemCalls. This replaces the original
 * i386 command-plus-adjacent-stack-dwords ABI without guessing vararg counts.
 */
typedef intptr_t (*ui_syscall_t)(intptr_t *arguments);

#define UI_NATIVE_SYSCALL_1(a0) ui_syscall_vector((intptr_t[]){(intptr_t)(a0)})
#define UI_NATIVE_SYSCALL_2(a0, a1) ui_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1)})
#define UI_NATIVE_SYSCALL_3(a0, a1, a2) ui_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2)})
#define UI_NATIVE_SYSCALL_4(a0, a1, a2, a3) ui_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3)})
#define UI_NATIVE_SYSCALL_5(a0, a1, a2, a3, a4) \
    ui_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4)})
#define UI_NATIVE_SYSCALL_6(a0, a1, a2, a3, a4, a5) \
    ui_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5)})
#define UI_NATIVE_SYSCALL_7(a0, a1, a2, a3, a4, a5, a6) \
    ui_syscall_vector( \
        (intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6)})
#define UI_NATIVE_SYSCALL_8(a0, a1, a2, a3, a4, a5, a6, a7) \
    ui_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), \
                                   (intptr_t)(a6), (intptr_t)(a7)})
#define UI_NATIVE_SYSCALL_9(a0, a1, a2, a3, a4, a5, a6, a7, a8) \
    ui_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), \
                                   (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8)})
#define UI_NATIVE_SYSCALL_10(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
    ui_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), \
                                   (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9)})
#define UI_NATIVE_SYSCALL_11(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
    ui_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), \
                                   (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10)})
#define UI_NATIVE_SYSCALL_12(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
    ui_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), \
                                   (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), (intptr_t)(a11)})
#define UI_NATIVE_SYSCALL_13(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) \
    ui_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), \
                                   (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), (intptr_t)(a11), \
                                   (intptr_t)(a12)})
#define UI_NATIVE_SYSCALL_SELECT(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, name, ...) name
#define ui_syscall(...) \
    UI_NATIVE_SYSCALL_SELECT(__VA_ARGS__, UI_NATIVE_SYSCALL_13, UI_NATIVE_SYSCALL_12, UI_NATIVE_SYSCALL_11, UI_NATIVE_SYSCALL_10, \
                             UI_NATIVE_SYSCALL_9, UI_NATIVE_SYSCALL_8, UI_NATIVE_SYSCALL_7, UI_NATIVE_SYSCALL_6, UI_NATIVE_SYSCALL_5, \
                             UI_NATIVE_SYSCALL_4, UI_NATIVE_SYSCALL_3, UI_NATIVE_SYSCALL_2, UI_NATIVE_SYSCALL_1)(__VA_ARGS__)
#endif

#if UINTPTR_MAX == UINT32_MAX
extern ui_syscall_t ui_syscall;
#else
extern ui_syscall_t ui_syscall_vector;
#endif

UI_EXPORT void UI_ABI_CDECL dllEntry(ui_syscall_t systemCall);
UI_EXPORT intptr_t UI_ABI_CDECL vmMain(int32_t command, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4,
                                       intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, intptr_t arg9, intptr_t arg10,
                                       intptr_t arg11);
int32_t PASSFLOAT(float value);
void trap_Error(const char *message);
void trap_Print(const char *message);
const char *trap_GetLanguagename(int32_t languageIndex);
int32_t trap_VerifyLanguageSelection(int32_t languageIndex);
int32_t trap_Milliseconds(void);
void trap_Cvar_Register(vmCvar_t *vmCvar, const char *name, const char *defaultValue, int32_t flags);
void trap_Cvar_Update(vmCvar_t *vmCvar);
void trap_Cvar_Set(const char *name, const char *value);
float trap_Cvar_VariableValue(const char *name);
void trap_Cvar_VariableStringBuffer(const char *name, char *buffer, int32_t bufferSize);
void trap_Cvar_SetValue(const char *name, float value);
void trap_Cmd_ExecuteText(cbufExec_t execWhen, const char *text);
int32_t trap_FS_FOpenFile(const char *filename, int32_t *handle, fsMode_t mode);
void trap_FS_Read(void *buffer, int32_t length, int32_t handle);
void trap_FS_FCloseFile(int32_t handle);
int32_t trap_FS_GetFileList(const char *path, const char *extension, char *buffer, int32_t bufferSize);
qhandle_t trap_R_RegisterShaderNoMip(const char *name, int32_t loadMode);
/* Despite its historical "sound handle" spelling, the Win32 word returned
 * here is the canonical sound-alias name pointer. */
const char *trap_S_RegisterSound(const char *name);
void trap_Key_ClearStates(void);
int32_t trap_Key_GetCatcher(void);
void trap_Key_SetCatcher(int32_t catcher);
void trap_R_RegisterFont(const char *name, int32_t pointSize, fontInfo_t *fontStorage, intptr_t context);
int32_t trap_R_Text_Width(const char *text, int32_t font, float scale, int32_t limit);
void trap_R_Text_Paint(float x, float y, int32_t font, float scale, const vec4_t color, const char *text, float fixedAdvance, int32_t limit,
                       int32_t textStyle);
void trap_GetClientState(uiClientState_t *state);
void trap_GetGlconfig(glconfig_t *config);
qboolean trap_GetConfigString(int32_t index, char *buffer, int32_t bufferSize);
qboolean trap_GetClientName(int32_t clientNum, char *buffer, int32_t bufferSize);
void trap_MSS_PlayLocalSoundAlias(const char *aliasName);
int32_t trap_LAN_CompareServers(int32_t source, int32_t sortKey, int32_t sortDirection, int32_t server1, int32_t server2);
int32_t trap_CIN_PlayCinematic(const char *name, int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags);
void trap_CIN_StopCinematic(int32_t handle);
void trap_CIN_RunCinematic(int32_t handle);
void trap_CIN_DrawCinematic(int32_t handle);
void trap_CIN_SetExtents(int32_t handle, int32_t x, int32_t y, int32_t width, int32_t height);
void trap_R_SetColor(const vec4_t rgba);
void trap_R_DrawStretchPic(float x, float y, float width, float height, float s0, float t0, float s1, float t1, qhandle_t shader);
void trap_UpdateScreen(void);
int32_t trap_RealTime(qtime_t *time);
const char *trap_SE_TranslateReference(const char *reference);
const char *trap_SE_LocalizeMessage(const char *message, const char *context);
int32_t trap_LAN_GetServerCount(int32_t source);
void ui_compat_lan_get_server_info(int32_t source, int32_t server, char *buffer, int32_t bufferSize);
void ui_compat_lan_get_server_address(int32_t source, int32_t server, char *buffer, int32_t bufferSize);
int32_t trap_LAN_AddServer(int32_t source, const char *name, const char *address);
qboolean trap_LAN_WaitServerResponse(int32_t source);
qboolean trap_LAN_UpdateDirtyPings(int32_t source);
void trap_LAN_MarkServerDirty(int32_t source, int32_t server, qboolean dirty);
int32_t trap_LAN_GetServerPing(int32_t source, int32_t server);
qboolean trap_LAN_ServerIsDirty(int32_t source, int32_t server);
void trap_LAN_ResetPings(int32_t source);
void trap_LAN_SaveCachedServers(void);
void trap_Argv(int32_t index, char *buffer, int32_t bufferSize);

#endif
