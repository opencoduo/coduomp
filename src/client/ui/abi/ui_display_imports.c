#include "../module/ui_functions.h"

#include "ui_module_abi.h"

// Source: uo_ui_mp_x86.dll 0x4001d430..0x4001d446.
qhandle_t trap_R_RegisterModel(const char *name, int32_t loadMode)
{
    return (qhandle_t)ui_syscall(UI_R_REGISTER_MODEL, (intptr_t)name,
                                 (intptr_t)loadMode);
}

// Source: uo_ui_mp_x86.dll 0x4001d770..0x4001d78b.
void trap_R_ModelBounds(qhandle_t model, vec3_t minimums, vec3_t maximums)
{
    ui_syscall(UI_R_MODEL_BOUNDS, (intptr_t)model, (intptr_t)minimums,
               (intptr_t)maximums);
}

// Source: uo_ui_mp_x86.dll 0x4001d4a0..0x4001d4bc.
int32_t trap_R_Text_Height(int32_t font, float scale)
{
    return (int32_t)ui_syscall(UI_R_TEXT_HEIGHT, (intptr_t)font,
                               (intptr_t)PASSFLOAT(scale));
}

// Source: uo_ui_mp_x86.dll 0x4001d5e0..0x4001d5ea.
void trap_R_ClearScene(void)
{
    ui_syscall(UI_R_CLEAR_SCENE);
}

// Source: uo_ui_mp_x86.dll 0x4001d5f0..0x4001d601.
void trap_R_AddRefEntity(const refEntity_t *entity)
{
    ui_syscall(UI_R_ADD_REF_ENTITY, (intptr_t)entity);
}

// Source: uo_ui_mp_x86.dll 0x4001d6c0..0x4001d6d1.
void trap_R_RenderScene(const refdef_t *refdef)
{
    ui_syscall(UI_R_RENDER_SCENE, (intptr_t)refdef);
}

// Source: uo_ui_mp_x86.dll 0x4001d520..0x4001d577.
void trap_R_Text_PaintWithCursor(float x, float y, int32_t font,
                                 float scale, const vec4_t color,
                                 const char *text, int32_t cursorPosition,
                                 int8_t cursorCharacter, int32_t limit,
                                 int32_t textStyle)
{
    ui_syscall(UI_R_TEXT_PAINT_WITH_CURSOR, (intptr_t)PASSFLOAT(x),
               (intptr_t)PASSFLOAT(y), (intptr_t)font,
               (intptr_t)PASSFLOAT(scale), (intptr_t)color, (intptr_t)text,
               (intptr_t)cursorPosition, (intptr_t)cursorCharacter,
               (intptr_t)limit, (intptr_t)textStyle);
}

// Source: uo_ui_mp_x86.dll 0x4001d880..0x4001d891.
void trap_Key_SetOverstrikeMode(qboolean overstrike)
{
    ui_syscall(UI_KEY_SET_OVERSTRIKE_MODE, (intptr_t)overstrike);
}

// Source: uo_ui_mp_x86.dll 0x4001d870..0x4001d87c.
qboolean trap_Key_GetOverstrikeMode(void)
{
    return (qboolean)ui_syscall(UI_KEY_GET_OVERSTRIKE_MODE);
}

// Source: uo_ui_mp_x86.dll 0x4001d800..0x4001d81b.
void trap_Key_KeynumToStringBuf(int32_t keynum, char *buffer,
                                int32_t bufferSize)
{
    ui_syscall(UI_KEY_KEYNUM_TO_STRING_BUF, (intptr_t)keynum,
               (intptr_t)buffer,
               (intptr_t)bufferSize);
}

// Source: uo_ui_mp_x86.dll 0x4001d820..0x4001d83b.
void trap_Key_GetBindingBuf(int32_t keynum, char *buffer,
                            int32_t bufferSize)
{
    (void)ui_syscall(UI_KEY_GET_BINDING_BUF, (intptr_t)keynum,
                     (intptr_t)buffer, (intptr_t)bufferSize);
}

// Source: uo_ui_mp_x86.dll 0x4001d840..0x4001d856.
void trap_Key_SetBinding(int32_t keynum, const char *binding)
{
    ui_syscall(UI_KEY_SET_BINDING, (intptr_t)keynum, (intptr_t)binding);
}

// Source: uo_ui_mp_x86.dll 0x4001dc30..0x4001dc3a.
void trap_GetAutoUpdate(void)
{
    ui_syscall(UI_GET_AUTO_UPDATE);
}

// Source: uo_ui_mp_x86.dll 0x4001dc40..0x4001dc4c.
qboolean trap_RunningGame(void)
{
    return (qboolean)ui_syscall(UI_RUNNING_GAME);
}

/*
 * NOT_FROM_ORIGINAL_SOURCE
 *
 * The authoritative Win32 UI trap loads the syscall's binary32 result into
 * x87 ST0.  The cgame callback assigned to the same display-context slot loads
 * atof's binary64 result into ST0 and returns without narrowing it; some cgame
 * callers subsequently store that result as binary64.
 *
 * The original i386 ABI uses ST0 for both returns.  This adapter widens UI's
 * already-rounded binary32 value into the portable carrier used by the shared
 * callback declaration; it is not evidence of an original long-double type.
 */
long double ui_compat_display_cvar_value(const char *name)
{
    return (long double)trap_Cvar_VariableValue(name);
}

// NOT_FROM_ORIGINAL_SOURCE: typed adapter for the direct command-86 call site
// at uo_ui_mp_x86.dll 0x4000fd14.
void ui_compat_lan_load_cached_servers(void)
{
    ui_syscall(UI_LAN_LOAD_CACHED_SERVERS);
}

// NOT_FROM_ORIGINAL_SOURCE: typed adapter for the direct command-98 call site
// at uo_ui_mp_x86.dll 0x4000cabb.
qboolean ui_compat_lan_server_is_punkbuster(int32_t source, int32_t server)
{
    return (qboolean)ui_syscall(UI_LAN_SERVER_IS_PUNKBUSTER,
                                (intptr_t)source, (intptr_t)server);
}

// NOT_FROM_ORIGINAL_SOURCE: typed adapter for the direct command-89 call site
// at uo_ui_mp_x86.dll 0x4000cffc.
void ui_compat_lan_remove_server(int32_t source, const char *address)
{
    ui_syscall(UI_LAN_REMOVE_SERVER, (intptr_t)source, (intptr_t)address);
}

// NOT_FROM_ORIGINAL_SOURCE: typed adapter for the command-101 call site at
// uo_ui_mp_x86.dll 0x4000d421.
void ui_compat_set_pb_client_status(int32_t status)
{
    ui_syscall(UI_SET_PB_CLIENT_STATUS, (intptr_t)status);
}

// NOT_FROM_ORIGINAL_SOURCE: typed adapter for the command-95 call site at
// uo_ui_mp_x86.dll 0x4000c495.
qboolean ui_compat_verify_cd_key(const char *key, const char *checksum)
{
    return (qboolean)ui_syscall(UI_VERIFY_CD_KEY, (intptr_t)key,
                                (intptr_t)checksum);
}

// NOT_FROM_ORIGINAL_SOURCE: typed adapter for the command-65 call site at
// uo_ui_mp_x86.dll 0x4000c4c1.
void ui_compat_set_cd_key(const char *key, const char *checksum)
{
    ui_syscall(UI_SET_CD_KEY, (intptr_t)key, (intptr_t)checksum);
}
