/*
 * Source reconstruction for script cvar builtins.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <string.h>

#include "recovered_game.h"
#include "scr_vm.h"

void trap_Cvar_VariableStringBuffer(const char *name, char *buffer, int size);
int trap_Cvar_VariableIntegerValue(const char *name);
float trap_Cvar_VariableValue(const char *name);
void trap_Cvar_Register(vmCvar_t *cvar, const char *name, const char *value, int flags);
void trap_Cvar_Set(const char *name, const char *value);

static const char *game_compat_script_cvar_get_value_arg(char messageBuffer[MAX_STRING_CHARS])
{
    /* NOT_FROM_ORIGINAL_SOURCE: factored from setcvar/makecvarserverinfo bodies. */
    if (Scr_GetType(1) == SCRIPT_VAR_LOCALIZED_STRING) {
        Scr_ConstructMessageString(1, messageBuffer, MAX_STRING_CHARS, SCRIPT_MESSAGE_MODE_CVAR_VALUE);
        return messageBuffer;
    }

    return Scr_GetString(1);
}

static qboolean game_compat_script_cvar_build_observed_clean_buffer(const char *value, char cleaned[MAX_STRING_CHARS])
{
    /* NOT_FROM_ORIGINAL_SOURCE: shared by setcvar and makecvarserverinfo;
     * preserve a complete cleaned value when it fits and report otherwise. */
    memset(cleaned, 0, MAX_STRING_CHARS);

    int32_t index = 0;
    while (index < MAX_STRING_CHARS - 1 && value[index] != '\0') {
        cleaned[index] = Q_CleanCharacter((int)value[index]);
        if (cleaned[index] == '"') {
            cleaned[index] = '\'';
        }
        index++;
    }
    return value[index] == '\0' ? qtrue : qfalse;
}

/* VERIFIED_DECOMPILER(0x6721f, 7721f_script_func_getcvar.c, VERIFY-SCRIPT-CVAR-PACKET-2026-06-17): DATAFLOW_VERIFIED - cvar name fetch, 1024-byte buffer lookup, Scr_AddString argument, and void return checked against current decompiler output. */
void GScr_GetCvar(void)
{
    const char *name = Scr_GetString(0);
    char value[MAX_STRING_CHARS];

    trap_Cvar_VariableStringBuffer(name, value, MAX_STRING_CHARS);
    Scr_AddString(value);
}

/* VERIFIED_DECOMPILER(0x67277, 77277_script_func_getcvarint.c, VERIFY-SCRIPT-CVAR-PACKET-2026-06-17): DATAFLOW_VERIFIED - cvar name fetch, integer lookup, Scr_AddInt argument, and void return checked against current decompiler output. */
void GScr_GetCvarInt(void)
{
    const char *name = Scr_GetString(0);

    Scr_AddInt(trap_Cvar_VariableIntegerValue(name));
}

/* VERIFIED_DECOMPILER(0x672b1, 772b1_script_func_getcvarfloat.c, VERIFY-SCRIPT-CVAR-PACKET-2026-06-17): DATAFLOW_VERIFIED - cvar name fetch, float lookup, Scr_AddFloat argument, and void return checked against current decompiler output. */
void GScr_GetCvarFloat(void)
{
    const char *name = Scr_GetString(0);

    Scr_AddFloat(trap_Cvar_VariableValue(name));
}

/* VERIFIED_DECOMPILER(0x672eb, 772eb_script_func_setcvar.c, VERIFY-SCRIPT-CVAR-PACKET-2026-06-17): DATAFLOW_VERIFIED - string/message value argument, strlen side effect, observed clean-buffer loop, optional serverinfo flag, register call, set call, and void return checked against current decompiler output. */
void GScr_SetCvar(void)
{
    const char *name = Scr_GetString(0);
    char messageValue[MAX_STRING_CHARS];
    char observedCleaned[MAX_STRING_CHARS];
    const char *value = game_compat_script_cvar_get_value_arg(messageValue);
    int flags = CVAR_SCRIPT_SETCVAR;

    (void)strlen(value);
    /* The retail clean pass has no consumer in this function. Bound it while
     * retaining the original cvar value, including values longer than the
     * dead local array. */
    (void)game_compat_script_cvar_build_observed_clean_buffer(value, observedCleaned);

    if (Scr_GetNumParam() > 2 && Scr_GetBool(2)) {
        flags |= CVAR_SCRIPT_SETCVAR_SERVERINFO;
    }

    trap_Cvar_Register(0, name, value, flags);
    trap_Cvar_Set(name, value);
}

/* VERIFIED_DECOMPILER(0x6e4ae, 7e4ae_script_func_makecvarserverinfo.c, VERIFY-SCRIPT-CVAR-PACKET-2026-06-17): DATAFLOW_VERIFIED - string/message value argument, strlen side effect, observed clean-buffer loop, serverinfo registration flags, and void return checked against current decompiler output. */
void GScr_MakeCvarServerInfo(void)
{
    const char *name = Scr_GetString(0);
    char messageValue[MAX_STRING_CHARS];
    char observedCleaned[MAX_STRING_CHARS];
    const char *value = game_compat_script_cvar_get_value_arg(messageValue);

    (void)strlen(value);
    /* As in GScr_SetCvar, this result is unobserved; the original value remains
     * the registration value. */
    (void)game_compat_script_cvar_build_observed_clean_buffer(value, observedCleaned);

    trap_Cvar_Register(0, name, value, CVAR_SCRIPT_MAKE_SERVERINFO);
}
