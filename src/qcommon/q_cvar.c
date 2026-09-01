#include "q_cvar.h"
#include "q_cvar_services.h"

#include "compat/coduo_ctype_compat.h"
#include "compat/coduo_fp_conversion.h"
#include "compat/crt/format_compat.h"
#include "com_parse.h"
#include "com_sprintf.h"
#include "info.h"
#include "q_command.h"
#include "q_string.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    CVAR_INFO_STRING_SIZE = 1024,
    CVAR_BIG_INFO_STRING_SIZE = 8192,
    CVAR_WRITE_BUFFER_SIZE = 1024,
    CVAR_SET_VALUE_BUFFER_SIZE = 4096,
    PB_CVAR_VALIDATE_OUTPUT_CAPACITY = 256,
    CVAR_NAME_COMPARE_LIMIT = 99999,
    COM_CVAR_BUFFER_TRACKED_COUNT = 65536,
    CVAR_SET_PRINT_CHANNEL_DEVELOPER = 4,
    CVAR_LIST_SERVERINFO_FLAGS =
        CVAR_SERVERINFO | CVAR_SCRIPT_SETCVAR_SERVERINFO,
    CVAR_WRITE_DEFAULTS_EXCLUDED_FLAGS =
        CVAR_ROM | CVAR_USER_CREATED |
        CVAR_CHEAT | CVAR_SCRIPT_SETCVAR
};

static cvar_t *cvarHashTable[CVAR_HASH_BUCKET_COUNT];
static cvar_t cvarTable[CVAR_MAX_COUNT]; /* original 0x009827a0 */
static char cvarInfoString[CVAR_INFO_STRING_SIZE]; /* original 0x00998ba0 */
static char cvarBigInfoString[CVAR_BIG_INFO_STRING_SIZE]; /* 0x00998fa0 */
static int32_t cvar_numIndexes;           /* original 0x04927ea0 */
static cvar_t *cvarExportCursor;           /* original 0x0389fd48 */
cvar_t *sv_console_lockout;                 /* CoDUOMP.exe 0x00982794 */
cvar_t *sv_cheats;                          /* CoDUOMP.exe 0x00982798 */
cvar_t *cvar_vars;
uint32_t cvar_modifiedFlags;

/*
 * Sources: CoDUOMP.exe 0x0043e140..0x0043e173 and coduo_lnxded's
 * corresponding Cvar initialization body.  Both register the same two cvars
 * with the same flags, then install the cvar commands.  The former Linux
 * `Cvar_ResetScriptInfo` name was a reconstruction-only mismatch; the Mac
 * symbol and Windows client identify the canonical original name Cvar_Init.
 */
void Cvar_Init(void)
{
    sv_cheats = Cvar_Get("sv_cheats", "0", CVAR_ROM | CVAR_SYSTEMINFO);
    sv_console_lockout = Cvar_Get(
        "sv_console_lockout", "0", CVAR_ROM | CVAR_SYSTEMINFO);
    Cvar_AddCommands();
}

/*
 * The Windows client and Linux dedicated bodies are the same forced-set
 * wrapper:
 *
 *   CoDUOMP.exe 0x0043df50
 *   coduo_lnxded 0x08073798
 *
 * The Linux EAX value after the call is merely the Cvar_Set2 result left in
 * the return register.  No Linux caller consumes it, and the Windows import
 * boundary plus the supporting Mac Cvar_Set symbol retain the canonical void
 * interface.
 */
void Cvar_Set(const char *name, const char *value)
{
    (void)Cvar_Set2(name, value, qtrue);
}

/* Source: CoDUOMP.exe 0x0043d860..0x0043d8ad.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043d860_0043d8ae.mcode.
 * Name and algorithm: same-family recovered Linux engine
 * generateHashValue. This cvar-local hash is distinct from the renderer's
 * same-named, translation-unit-local image-path hash. */
static uint32_t generateHashValue(const char *name)
{
    enum { CVAR_HASH_CHARACTER_BASE_INDEX = 119 };
    uint32_t hash = 0;

    if (name == NULL)
        Com_Error(ERR_DROP,
                  "\x15null name in generateHashValue");

    for (int32_t index = 0; name[index] != '\0'; ++index) {
        const int32_t lower =
            tolower(coduo_ctype_signed_byte_arg(name[index]));
        hash += ((uint32_t)index +
                 (uint32_t)CVAR_HASH_CHARACTER_BASE_INDEX) *
                (uint32_t)lower;
    }

    return hash & (CVAR_HASH_BUCKET_COUNT - 1);
}

/* Source: CoDUOMP.exe 0x0043d8b0..0x0043d8e5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043d8b0_0043d8e6.mcode.
 * Name: exact same-module Mac symbol Cvar_ValidateString. */
qboolean Cvar_ValidateString(const char *name)
{
    if (name == NULL)
        return qfalse;

    return strchr(name, '\\') == NULL &&
                   strchr(name, '"') == NULL &&
                   strchr(name, ';') == NULL
               ? qtrue
               : qfalse;
}

/* Source: CoDUOMP.exe 0x0043d8f0..0x0043d932.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043d8f0_0043d933.mcode.
 * Name: exact same-module Mac symbol Cvar_FindVar. */
cvar_t *Cvar_FindVar(const char *name)
{
    const uint32_t hash = generateHashValue(name);

    for (cvar_t *cvar = cvarHashTable[hash];
         cvar != NULL;
         cvar = cvar->hashNext) {
        if (Q_stricmp(name, cvar->name) == 0)
            return cvar;
    }

    return NULL;
}

/* Source: CoDUOMP.exe 0x0043d9e0..0x0043dc44.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043d9e0_0043dc45.mcode.
 * Name: exact same-module Mac symbol Cvar_Get. The fixed cvar table, sorted
 * next chain, hash chain, placeholder replacement, and reset/latched value
 * ownership are all independently visible in the Windows instructions. */
cvar_t *Cvar_Get(const char *name, const char *defaultValue,
                 uint32_t flags)
{
    if (name == NULL || defaultValue == NULL)
        Com_Error(ERR_FATAL, "\x15" "Cvar_Get: NULL parameter");

    if (Cvar_ValidateString(name) == qfalse) {
        /* NOT_FROM_ORIGINAL_SOURCE: keep the invalid name as formatter data. */
        Com_Error(ERR_FATAL, "invalid cvar name string: %s", name);
    }

    cvar_t *cvar = Cvar_FindVar(name);
    if (cvar != NULL) {
        if ((cvar->flags & CVAR_PLACEHOLDER_CREATED_MASK) != 0 &&
            (flags & CVAR_PLACEHOLDER_CREATED_MASK) == 0 &&
            (defaultValue[0] != '\0' ||
             (flags & CVAR_CHEAT) != 0)) {
            cvar->flags &= ~CVAR_PLACEHOLDER_CREATED_MASK;
            Z_FreeInternal(cvar->resetString);
            cvar->resetString = CopyStringInternal(defaultValue);
            cvar_modifiedFlags |= flags;
        }

        cvar->flags |= flags;
        if (cvar->resetString[0] == '\0') {
            Z_FreeInternal(cvar->resetString);
            cvar->resetString = CopyStringInternal(defaultValue);
        } else if (defaultValue[0] != '\0' &&
                   strcmp(cvar->resetString, defaultValue) != 0) {
            Com_DPrintf(
                "Warning: cvar \"%s\" given initial values: \"%s\" and \"%s\"\n",
                name, cvar->resetString, defaultValue);
        }

        if (cvar->latchedString != NULL) {
            char *const latchedValue = cvar->latchedString;
            cvar->latchedString = NULL;
            (void)Cvar_Set2(name, latchedValue, qtrue);
            Z_FreeInternal(latchedValue);
        }

        if ((cvar->flags & CVAR_CHEAT) != 0 &&
            sv_cheats != NULL && sv_cheats->integer == 0) {
            (void)Cvar_Set2(name, defaultValue, qtrue);
        }
        return cvar;
    }

    if (cvar_numIndexes >= CVAR_MAX_COUNT)
        Com_Error(ERR_FATAL, "MAX_CVARS");

    cvar = &cvarTable[cvar_numIndexes++];
    cvar->name = CopyStringInternal(name);
    cvar->string = CopyStringInternal(defaultValue);
    cvar->modified = qtrue;
    cvar->modificationCount = 1;
    cvar->value = (float)atof(cvar->string);
    cvar->integer = atoi(cvar->string);
    cvar->resetString = CopyStringInternal(defaultValue);

    cvar_t **sortedLink = &cvar_vars;
    while (*sortedLink != NULL &&
           Q_stricmp(cvar->name, (*sortedLink)->name) >= 0) {
        sortedLink = &(*sortedLink)->next;
    }
    cvar->next = *sortedLink;
    *sortedLink = cvar;

    cvar->flags = flags;
    const uint32_t hash = generateHashValue(name);
    cvar->hashNext = cvarHashTable[hash];
    cvarHashTable[hash] = cvar;
    return cvar;
}

/* Source: CoDUOMP.exe 0x0043dc50..0x0043df47.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043dc50_0043df48.mcode.
 * Name: exact same-module Mac symbol Cvar_Set2. The original force path
 * bypasses write/cheat/console-lockout gates and clears any latched value;
 * the ordinary path preserves the restart latch behavior. */
cvar_t *Cvar_Set2(const char *name, const char *value, qboolean force)
{
    Com_PrintMessage(
        CVAR_SET_PRINT_CHANNEL_DEVELOPER,
        va("      cvar set %s %s\n", name, value));

    if (Cvar_ValidateString(name) == qfalse) {
        /* NOT_FROM_ORIGINAL_SOURCE: keep the invalid name as formatter data. */
        Com_Error(ERR_FATAL, "invalid cvar name string: %s", name);
    }

    cvar_t *cvar = Cvar_FindVar(name);
    if (cvar == NULL) {
        if (value == NULL)
            return NULL;
        return Cvar_Get(
            name, value,
            force == qfalse ? CVAR_USER_CREATED : 0);
    }

    if (value == NULL)
        value = cvar->resetString;

    if (strcmp(value, cvar->string) == 0) {
        if ((cvar->flags & CVAR_LATCH) != 0 &&
            cvar->latchedString != NULL) {
            Z_FreeInternal(cvar->latchedString);
            cvar->latchedString = NULL;
        }
        return cvar;
    }

    cvar_modifiedFlags |= cvar->flags;
    if (force == qfalse) {
        if ((cvar->flags & CVAR_ROM) != 0) {
            Com_Printf("%s is read only.\n", name);
            return cvar;
        }
        if ((cvar->flags & CVAR_INIT) != 0) {
            Com_Printf("%s is write protected.\n", name);
            return cvar;
        }
        if ((cvar->flags & CVAR_CHEAT) != 0 &&
            sv_cheats->integer == 0) {
            Com_Printf("%s is cheat protected.\n", name);
            return cvar;
        }
        if (sv_console_lockout->integer != 0 &&
            sv_running->integer != 0) {
            Com_Printf(
                "Consol is locked out, cannot change cvars.\n");
            return cvar;
        }
        if ((cvar->flags & CVAR_LATCH) != 0) {
            if (cvar->latchedString != NULL) {
                if (strcmp(value, cvar->latchedString) == 0)
                    return cvar;
                Z_FreeInternal(cvar->latchedString);
            } else if (strcmp(value, cvar->string) == 0) {
                return cvar;
            }

            Com_Printf(
                "%s will be changed upon restarting.\n", name);
            cvar->latchedString = CopyStringInternal(value);
            cvar->modified = qtrue;
            return cvar;
        }
    } else if (cvar->latchedString != NULL) {
        Z_FreeInternal(cvar->latchedString);
        cvar->latchedString = NULL;
    }

    if (strcmp(value, cvar->string) != 0) {
        cvar->modified = qtrue;
        ++cvar->modificationCount;
        Z_FreeInternal(cvar->string);
        cvar->string = CopyStringInternal(value);
        cvar->value = (float)atof(cvar->string);
        cvar->integer = atoi(cvar->string);
    }
    return cvar;
}

/* Source: CoDUOMP.exe 0x0043dfa0..0x0043dfac, recovered from an exporter
 * function-boundary gap.
 * Name: exact same-module Mac symbol Cvar_SetLatched. The original wrapper
 * forwards name and value unchanged with force set to false. */
void Cvar_SetLatched(const char *name, const char *value)
{
    (void)Cvar_Set2(name, value, qfalse);
}

/* Source: CoDUOMP.exe 0x0043e040..0x0043e04d, recovered from an exporter
 * function-boundary gap.
 * Name: exact same-module Mac symbol Cvar_Reset. A NULL value selects the
 * cvar's resetString inside Cvar_Set2; force remains false. */
void Cvar_Reset(const char *name)
{
    (void)Cvar_Set2(name, NULL, qfalse);
}

/* Source: CoDUOMP.exe 0x0043eb80..0x0043ebac.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043eb80_0043ebad.mcode.
 * Name and behavior: same-family recovered Linux engine Cvar_SetExisting.
 * Existing latch cvars use the ordinary Set2 path so their values remain
 * pending until restart; other existing cvars are changed immediately
 * through the force path. */
void Cvar_SetExisting(const char *name, const char *value)
{
    cvar_t *const cvar = Cvar_FindVar(name);
    if (cvar == NULL)
        return;

    (void)Cvar_Set2(
        name, value,
        (cvar->flags & CVAR_LATCH) != 0 ? qfalse : qtrue);
}

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
void Cvar_Set_f(void)
{
    const int32_t argumentCount = Cmd_Argc();
    if (argumentCount < 3) {
        Com_Printf("usage: set <variable> <value>\n");
        return;
    }

    char value[CVAR_SET_VALUE_BUFFER_SIZE] = "";
    size_t valueLength = 0;
    for (int32_t argumentIndex = 2;
         argumentIndex < argumentCount;
         ++argumentIndex) {
        const char *const argument = Cmd_Argv(argumentIndex);
        const size_t argumentLength = strlen(argument);
        const size_t separatorLength =
            argumentIndex != argumentCount - 1 ? 1U : 0U;
        const size_t availableLength =
            sizeof(value) - 1U - valueLength;

        /* NOT_FROM_ORIGINAL_SOURCE: charge the complete argument, optional
         * separator, and final NUL before extending the applied prefix. */
        if (argumentLength > availableLength ||
            separatorLength > availableLength - argumentLength) {
            break;
        }

        memcpy(value + valueLength, argument, argumentLength);
        valueLength += argumentLength;
        if (separatorLength != 0U) {
            value[valueLength++] = ' ';
        }
        value[valueLength] = '\0';
    }

    (void)Cvar_Set2(Cmd_Argv(1), value, qfalse);
}

/* Source: CoDUOMP.exe 0x0043e360..0x0043e4cd, recovered from the executable
 * gap between aligned Ghidra functions.
 * Name: exact same-module Mac symbol Cvar_Toggle_f. */
void Cvar_Toggle_f(void)
{
    const int32_t argumentCount = Cmd_Argc();
    if (argumentCount < 2) {
        Com_Printf(
            "usage: toggle <variable> <optional value sequence>\n");
        return;
    }

    const char *const name = Cmd_Argv(1);
    if (argumentCount == 2) {
        const int32_t toggledValue =
            coduo_fp_to_i32_extended(
                (long double)Cvar_VariableValue(name)) == 0;
        (void)Cvar_Set2(
            name, va("%i", toggledValue), qfalse);
        return;
    }

    const char *const currentValue = Cvar_VariableString(name);
    for (int32_t argumentIndex = 2;
         argumentIndex < argumentCount - 1;
         ++argumentIndex) {
        if (strcmp(currentValue, Cmd_Argv(argumentIndex)) == 0) {
            (void)Cvar_Set2(
                name, Cmd_Argv(argumentIndex + 1), qfalse);
            return;
        }
    }
    (void)Cvar_Set2(name, Cmd_Argv(2), qfalse);
}

/* Source: CoDUOMP.exe 0x0043e610..0x0043e650, recovered from an executable
 * gap. Name: exact same-module Mac symbol Cvar_SetU_f. */
void Cvar_SetU_f(void)
{
    if (Cmd_Argc() != 3) {
        Com_Printf("usage: setu <variable> <value>\n");
        return;
    }

    Cvar_Set_f();
    cvar_t *const cvar = Cvar_FindVar(Cmd_Argv(1));
    if (cvar != NULL)
        cvar->flags |= CVAR_USERINFO;
}

/* Source: CoDUOMP.exe 0x0043e660..0x0043e6a0, recovered from an executable
 * gap. Name: exact same-module Mac symbol Cvar_SetS_f. */
void Cvar_SetS_f(void)
{
    if (Cmd_Argc() != 3) {
        Com_Printf("usage: sets <variable> <value>\n");
        return;
    }

    Cvar_Set_f();
    cvar_t *const cvar = Cvar_FindVar(Cmd_Argv(1));
    if (cvar != NULL)
        cvar->flags |= CVAR_SERVERINFO;
}

/* Source: CoDUOMP.exe 0x0043e6b0..0x0043e6f0, recovered from an executable
 * gap. Name: exact same-module Mac symbol Cvar_SetA_f. */
void Cvar_SetA_f(void)
{
    if (Cmd_Argc() != 3) {
        Com_Printf("usage: seta <variable> <value>\n");
        return;
    }

    Cvar_Set_f();
    cvar_t *const cvar = Cvar_FindVar(Cmd_Argv(1));
    if (cvar != NULL)
        cvar->flags |= CVAR_ARCHIVE;
}

/* Source: CoDUOMP.exe 0x0043e700..0x0043e753, recovered from an executable
 * gap. Name: exact same-module Mac symbol Cvar_SetFromCvar_f. */
void Cvar_SetFromCvar_f(void)
{
    if (Cmd_Argc() != 3) {
        Com_Printf(
            "usage: setfromcvar <variable> <variablein>\n");
        return;
    }

    const cvar_t *const source = Cvar_FindVar(Cmd_Argv(2));
    const char *const value =
        source != NULL ? source->string : "";
    (void)Cvar_Set2(Cmd_Argv(1), value, qfalse);
}

/* Source: CoDUOMP.exe 0x0043e760..0x0043e789, recovered from an executable
 * gap. Name: exact same-module Mac symbol Cvar_Reset_f. */
void Cvar_Reset_f(void)
{
    if (Cmd_Argc() != 2) {
        Com_Printf("usage: reset <variable>\n");
        return;
    }

    (void)Cvar_Set2(Cmd_Argv(1), NULL, qfalse);
}

/* Source: CoDUOMP.exe 0x0043e8d0..0x0043ea1c, recovered from the executable
 * gap between Cvar_WriteDefaults and Cvar_DumpToChannel.
 * Name: exact same-module Mac symbol Cvar_List_f. */
void Cvar_List_f(void)
{
    const char *const match =
        Cmd_Argc() > 1 ? Cmd_Argv(1) : NULL;

    int32_t count = 0;
    for (cvar_t *cvar = cvar_vars;
         cvar != NULL;
         cvar = cvar->next) {
        if (match == NULL ||
            Com_Filter(match, cvar->name, qfalse) != qfalse) {
            Com_Printf(
                (cvar->flags & CVAR_LIST_SERVERINFO_FLAGS) != 0
                    ? "S"
                    : " ");
            Com_Printf(
                (cvar->flags & CVAR_USERINFO) != 0 ? "U" : " ");
            Com_Printf(
                (cvar->flags & CVAR_ROM) != 0 ? "R" : " ");
            Com_Printf(
                (cvar->flags & CVAR_INIT) != 0 ? "I" : " ");
            Com_Printf(
                (cvar->flags & CVAR_ARCHIVE) != 0 ? "A" : " ");
            Com_Printf(
                (cvar->flags & CVAR_LATCH) != 0 ? "L" : " ");
            Com_Printf(
                (cvar->flags & CVAR_CHEAT) != 0 ? "C" : " ");
            Com_Printf(" %s \"%s\"\n", cvar->name, cvar->string);
        }
        ++count;
    }

    Com_Printf("\n%i total cvars\n", count);
    Com_Printf("%i cvar indexes\n", cvar_numIndexes);
}

/* Source: CoDUOMP.exe 0x0043ea20..0x0043ea28.
 * Name and call target: same-family recovered Linux engine Cvar_Dump_f. */
void Cvar_Dump_f(void)
{
    Cvar_DumpToChannel(0);
}

/* Source: CoDUOMP.exe 0x0043ebb0..0x0043ebf1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043ebb0_0043ebf2.mcode.
 * Name and output roles: same-family recovered Linux engine
 * Cvar_NextExport. This is the engine side of the PunkBuster cvar enumerator,
 * not part of PunkBuster's statically linked implementation. */
qboolean Cvar_NextExport(const char **name, const char **string,
                         uint32_t *flags, const char **resetString)
{
    if (cvarExportCursor == NULL)
        cvarExportCursor = cvar_vars;
    else
        cvarExportCursor = cvarExportCursor->next;

    if (cvarExportCursor == NULL)
        return qfalse;

    *name = cvarExportCursor->name;
    *string = cvarExportCursor->string;
    *flags = cvarExportCursor->flags;
    *resetString = cvarExportCursor->resetString;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x0043ec00..0x0043ec5e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043ec00_0043ec5f.mcode.
 * Name: same-family recovered Linux engine PbCvarValidate. PunkBuster calls
 * this engine-owned consistency check through its host interface. */
char *PbCvarValidate(char *buffer)
{
    buffer[0] = '\0';
    for (cvar_t *cvar = cvar_vars;
         cvar != NULL;
         cvar = cvar->next) {
        const float parsedValue = (float)atof(cvar->string);
        const int32_t parsedInteger = atoi(cvar->string);
        if (parsedValue != cvar->value ||
            parsedInteger != cvar->integer) {
            /* NOT_FROM_ORIGINAL_SOURCE: the callback result must fit the
             * dispatcher's fixed terminated output contract. */
            Q_strncpyz(buffer, cvar->name, PB_CVAR_VALIDATE_OUTPUT_CAPACITY);
            break;
        }
    }
    return buffer;
}

/* Source: CoDUOMP.exe 0x0043dfb0..0x0043e030 and coduo_lnxded
 * 0x0807380c..0x080738a1. Integral floats use the integer spelling; other
 * values retain the original "%f" formatting. The Linux body performs the
 * conversion twice on its integer arm and compares the first integer in the
 * live x87 domain. Windows converts once through _ftol2 and narrows the
 * integer back to binary32 for its comparison. Every binary32 input produces
 * the same output text; the Linux port's redundant conversion and floating-
 * point status effects are compiler artifacts, so the main-platform Windows
 * source behavior is canonical. */
void Cvar_SetValue(const char *name, float value)
{
    char text[32];
    const int32_t integerValue =
        coduo_fp_to_i32_extended((long double)value);

    if (value == (float)integerValue)
        Com_sprintf(text, sizeof(text), "%i", integerValue);
    else
        Com_sprintf(text, sizeof(text), "%f", value);

    (void)Cvar_Set2(name, text, qtrue);
}

/* Source: CoDUOMP.exe 0x0043e1a0..0x0043e1b7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043e1a0_0043e1b8.mcode.
 * Name: exact same-module Mac symbol Cvar_VariableValue. */
float Cvar_VariableValue(const char *name)
{
    const cvar_t *const cvar = Cvar_FindVar(name);
    return cvar != NULL ? cvar->value : 0.0f;
}

/* Source: CoDUOMP.exe 0x0043e1c0..0x0043e1d1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043e1c0_0043e1d2.mcode.
 * Name: exact same-module Mac symbol Cvar_VariableIntegerValue. */
int32_t Cvar_VariableIntegerValue(const char *name)
{
    const cvar_t *const cvar = Cvar_FindVar(name);
    return cvar != NULL ? cvar->integer : 0;
}

/* Source: CoDUOMP.exe 0x0043e200..0x0043e226.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043e200_0043e227.mcode.
 * Name: exact same-module Mac symbol Cvar_VariableStringBuffer. */
void Cvar_VariableStringBuffer(const char *name, char *buffer,
                               int32_t bufferLength)
{
    const cvar_t *const cvar = Cvar_FindVar(name);

    if (cvar == NULL) {
        buffer[0] = '\0';
        return;
    }

    Q_strncpyz(buffer, cvar->string, bufferLength);
}

/* Source: CoDUOMP.exe 0x0043e260..0x0043e2c5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043e260_0043e2c6.mcode.
 * Name: exact same-module Mac symbol Cvar_SetCheatState. Cheat-protected
 * cvars whose current values differ are restored to their reset strings. */
void Cvar_SetCheatState(void)
{
    for (cvar_t *cvar = cvar_vars;
         cvar != NULL;
         cvar = cvar->next) {
        if ((cvar->flags & CVAR_CHEAT) != 0 &&
            strcmp(cvar->resetString, cvar->string) != 0) {
            (void)Cvar_Set2(cvar->name, cvar->resetString, qtrue);
        }
    }
}

/* Source: CoDUOMP.exe 0x0043e180..0x0043e19a, recovered from the executable
 * gap. Name and flag role: same-family recovered Linux engine
 * Cvar_ClearScriptSetServerinfoFlags. */
void Cvar_ClearScriptSetServerinfoFlags(void)
{
    for (cvar_t *cvar = cvar_vars;
         cvar != NULL;
         cvar = cvar->next) {
        cvar->flags &= ~CVAR_SCRIPT_SETCVAR_SERVERINFO;
    }
}

/* Source: CoDUOMP.exe 0x0043df70..0x0043df90.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043df70_0043df91.mcode.
 * Name and signature: same-family recovered Linux engine Cvar_VMSet. */
void Cvar_VMSet(vmCvar_t *vmCvar, const char *value)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((uint32_t)vmCvar->handle >= (uint32_t)cvar_numIndexes) {
        Com_Error(ERR_DROP,
                  "\x15" "Cvar_VMSet: handle out of range");
        return;
    }

    (void)Cvar_Set2(cvarTable[vmCvar->handle].name, value, qtrue);
    Cvar_Update(vmCvar);
}

/* Source: CoDUOMP.exe 0x0043e050..0x0043e08b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043e050_0043e08c.mcode.
 * Name: exact same-module Mac symbol Cvar_Register. */
void Cvar_Register(vmCvar_t *vmCvar, const char *name,
                   const char *defaultValue, uint32_t flags)
{
    cvar_t *const cvar = Cvar_Get(name, defaultValue, flags);

    if (vmCvar != NULL) {
        vmCvar->handle = (int32_t)(cvar - cvarTable);
        vmCvar->modificationCount = -1;
        Cvar_Update(vmCvar);
    }
}

/* Source: CoDUOMP.exe 0x0043e090..0x0043e131.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043e090_0043e132.mcode.
 * Name: exact same-module Mac symbol Cvar_Update. */
void Cvar_Update(vmCvar_t *vmCvar)
{
    if ((uint32_t)vmCvar->handle >= (uint32_t)cvar_numIndexes) {
        Com_Error(ERR_DROP,
                  "\x15" "Cvar_Update: handle out of range");
    }

    const cvar_t *const cvar = &cvarTable[vmCvar->handle];
    if (cvar->modificationCount == vmCvar->modificationCount ||
        cvar->string == NULL) {
        return;
    }

    vmCvar->modificationCount = cvar->modificationCount;
    const size_t stringLength = strlen(cvar->string);
    if (stringLength + 1 > sizeof(vmCvar->string)) {
        Com_Error(
            ERR_DROP,
            "\x15" "Cvar_Update: src %s length %d exceeds "
            "MAX_CVAR_VALUE_STRING",
            cvar->string, (int32_t)stringLength,
            (int32_t)sizeof(vmCvar->string));
    }

    Q_strncpyz(vmCvar->string, cvar->string,
               (int32_t)sizeof(vmCvar->string));
    vmCvar->value = cvar->value;
    vmCvar->integer = cvar->integer;
}

/* Each requested cvar is written as one quoted assignment line and a missing
 * cvar supplies an empty value. The authoritative bodies differ at the
 * platform formatter boundary: the Windows legacy bounded formatter returns
 * negative on truncation, while Linux C99 snprintf returns the required
 * length. See docs/platform-discrepancies/
 * cvar-save-buffer-truncation.md. */
#if defined(WINDOWS_BEHAVIOR)
/* Source: CoDUOMP.exe 0x0043d4b0..0x0043d521.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043d4b0_0043d522.mcode.
 * Name: exact same-module Mac symbol Com_SaveCvarsToBuffer. */
qboolean Com_SaveCvarsToBuffer(const char *const *cvarNames,
                               int32_t cvarCount, char *buffer,
                               size_t bufferSize)
{
    char *cursor = buffer;
    size_t remaining = bufferSize;

    for (int32_t index = 0; index < cvarCount; ++index) {
        const cvar_t *const cvar = Cvar_FindVar(cvarNames[index]);
        const char *const value =
            cvar != NULL ? cvar->string : "";
        const int32_t written = coduo_crt_snprintf(
            cursor, remaining, "%s \"%s\"\n", cvarNames[index], value);

        if (written < 0)
            return qfalse;

        cursor += written;
        remaining -= (size_t)written;
    }

    return qtrue;
}
#else
/* Source: coduo_lnxded 0x08072b9e..0x08072c3d. */
qboolean Com_SaveCvarsToBuffer(const char *const *cvarNames,
                               int32_t cvarCount, char *buffer,
                               size_t bufferSize)
{
    size_t remaining = bufferSize;

    for (int32_t index = 0; index < cvarCount; ++index) {
        const char *const name = cvarNames[index];
        const char *const value = Cvar_VariableString(name);
        const int32_t written = snprintf(
            buffer, (size_t)remaining, "%s \"%s\"\n", name, value);

        /* NOT_FROM_ORIGINAL_SOURCE: advance only after the complete serialized
         * entry fits the remaining output extent. */
        if (written < 0 || (size_t)written >= remaining)
            return qfalse;
        buffer += written;
        remaining -= (size_t)written;
    }

    return qtrue;
}
#endif

/* Source: CoDUOMP.exe 0x0043d530..0x0043d757.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043d530_0043d758.mcode.
 * Name: exact same-module Mac symbol Com_LoadCvarsFromBuffer. MSVC inlines
 * Com_Parse, Com_ParseOnLine, Com_SkipRestOfLine, and Com_EndParseSession
 * around their surviving Com_ParseExt calls. The byte array is the original
 * fixed 65536-entry stack allocation used to report omitted requested cvars. */
qboolean Com_LoadCvarsFromBuffer(const char *const *cvarNames,
                                 int32_t cvarCount, char *buffer,
                                 const char *fileName)
{
    uint8_t found[COM_CVAR_BUFFER_TRACKED_COUNT];
    int32_t foundCount = 0;

    /* NOT_FROM_ORIGINAL_SOURCE: validate the requested cvar count against the
     * fixed tracking array before clearing or indexing it. */
    if (cvarCount < 0 || cvarCount > COM_CVAR_BUFFER_TRACKED_COUNT) {
        Com_Printf("Com_LoadCvarsFromBuffer: invalid cvar count %i\n",
                   cvarCount);
        return qfalse;
    }

    memset(found, 0, (size_t)cvarCount);
    Com_BeginParseSession(fileName);

    for (;;) {
        const char *const name = Com_Parse(&buffer);
        if (name[0] == '\0')
            break;

        int32_t index;
        for (index = 0; index < cvarCount; ++index) {
            if (Q_stricmp(name, cvarNames[index]) == 0)
                break;
        }

        if (index == cvarCount) {
            Com_Printf(
                "^3WARNING: unknown cvar '%s' in file '%s'\n",
                name, fileName);
            Com_SkipRestOfLine(&buffer);
            continue;
        }

        const char *const value = Com_ParseOnLine(&buffer);
        (void)Cvar_Set2(cvarNames[index], value, qtrue);
        if (found[index] == 0) {
            found[index] = 1;
            ++foundCount;
        }
    }

    Com_EndParseSession();
    if (foundCount == cvarCount)
        return qtrue;

    Com_Printf(
        "^1ERROR: the following cvars were not specified in file '%s'\n",
        fileName);
    for (int32_t index = 0; index < cvarCount; ++index) {
        if (found[index] == 0)
            Com_Printf("^1  %s\n", cvarNames[index]);
    }
    return qfalse;
}

/* Source: CoDUOMP.exe 0x0043e1e0..0x0043e1f6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043e1e0_0043e1f7.mcode.
 * Name and behavior: established id-engine cvar interface. The original
 * returns the shared empty string when the named cvar does not exist. */
const char *Cvar_VariableString(const char *name)
{
    const cvar_t *const cvar = Cvar_FindVar(name);
    return cvar != NULL ? cvar->string : "";
}

/* Source: CoDUOMP.exe 0x0043e2d0..0x0043e353.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043e2d0_0043e354.mcode.
 * Name and behavior: exact same-module Mac symbol Cvar_Command. */
qboolean Cvar_Command(void)
{
    cvar_t *const cvar = Cvar_FindVar(Cmd_Argv(0));
    if (cvar == NULL)
        return qfalse;

    if (Cmd_Argc() == 1) {
        Com_Printf("\"%s\" is:\"%s^7\" default:\"%s^7\"\n",
                   cvar->name, cvar->string, cvar->resetString);
        if (cvar->latchedString != NULL)
            Com_Printf("latched: \"%s\"\n", cvar->latchedString);
    } else {
        (void)Cvar_Set2(cvar->name, Cmd_Argv(1), qfalse);
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x0043e230..0x0043e250 and coduo_lnxded
 * Cvar_CommandCompletion. Both enumerate the sorted cvar chain. */
void Cvar_CommandCompletion(void (*callback)(const char *name))
{
    for (cvar_t *cvar = cvar_vars; cvar != NULL; cvar = cvar->next)
        callback(cvar->name);
}

/* Source: CoDUOMP.exe 0x0043ecf0..0x0043ed22.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043ecf0_0043ed23.mcode.
 * Name and argument roles: same-family Linux server symbol
 * Cvar_SetConfigstringValues. */
void Cvar_SetConfigstringValues(int32_t base, int32_t count,
                                uint32_t flags)
{
    for (cvar_t *cvar = cvar_vars;
         cvar != NULL;
         cvar = cvar->next) {
        if ((cvar->flags & flags) != 0) {
            SV_SetConfigValueForKey(base, count,
                                    cvar->name, cvar->string);
        }
    }
}

/* Source: CoDUOMP.exe 0x0043ed30..0x0043ed67.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043ed30_0043ed68.mcode.
 * Name: exact same-module Mac symbol Cvar_InfoString. */
const char *Cvar_InfoString(uint32_t flags)
{
    cvarInfoString[0] = '\0';
    for (cvar_t *cvar = cvar_vars;
         cvar != NULL;
         cvar = cvar->next) {
        if ((cvar->flags & flags) != 0) {
            Info_SetValueForKey(
                cvarInfoString, cvar->name, cvar->string);
        }
    }
    return cvarInfoString;
}

/* Source: CoDUOMP.exe 0x0043ed70..0x0043eda7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043ed70_0043eda8.mcode.
 * Name: exact same-module Mac symbol Cvar_InfoString_Big. */
const char *Cvar_InfoString_Big(uint32_t flags)
{
    cvarBigInfoString[0] = '\0';
    for (cvar_t *cvar = cvar_vars;
         cvar != NULL;
         cvar = cvar->next) {
        if ((cvar->flags & flags) != 0) {
            Info_SetValueForKey_Big(
                cvarBigInfoString, cvar->name, cvar->string);
        }
    }
    return cvarBigInfoString;
}

/* Source: CoDUOMP.exe 0x0043edb0..0x0043edc8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043edb0_0043edc9.mcode.
 * Name: exact same-module Mac symbol Cvar_InfoStringBuffer. */
void Cvar_InfoStringBuffer(uint32_t flags, char *buffer,
                           int32_t bufferLength)
{
    Q_strncpyz(buffer, Cvar_InfoString(flags), bufferLength);
}

/* Source: CoDUOMP.exe 0x0043e790..0x0043e82a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043e790_0043e82b.mcode.
 * Name: exact same-module Mac symbol Cvar_WriteVariables. The active latched
 * value is written when present, and cl_cdkey is deliberately excluded. */
void Cvar_WriteVariables(int32_t fileHandle)
{
    char line[CVAR_WRITE_BUFFER_SIZE];

    for (cvar_t *cvar = cvar_vars;
         cvar != NULL;
         cvar = cvar->next) {
        if (cvar->name != NULL &&
            Q_stricmpn("cl_cdkey", cvar->name,
                       CVAR_NAME_COMPARE_LIMIT) == 0) {
            continue;
        }
        if ((cvar->flags & CVAR_ARCHIVE) == 0)
            continue;

        const char *const value =
            cvar->latchedString != NULL
                ? cvar->latchedString
                : cvar->string;
        Com_sprintf(line, sizeof(line),
                    "seta %s \"%s\"\n", cvar->name, value);
        FS_Printf(fileHandle, "%s", line);
    }
}

/* Source: CoDUOMP.exe 0x0043e830..0x0043e8c1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043e830_0043e8c2.mcode.
 * Name: exact same-module Mac symbol Cvar_WriteDefaults. Read-only,
 * user-created, cheat, and script-set cvars are not written as defaults. */
void Cvar_WriteDefaults(int32_t fileHandle)
{
    char line[CVAR_WRITE_BUFFER_SIZE];

    for (cvar_t *cvar = cvar_vars;
         cvar != NULL;
         cvar = cvar->next) {
        if (cvar->name != NULL &&
            Q_stricmpn("cl_cdkey", cvar->name,
                       CVAR_NAME_COMPARE_LIMIT) == 0) {
            continue;
        }
        if ((cvar->flags & CVAR_WRITE_DEFAULTS_EXCLUDED_FLAGS) != 0)
            continue;

        Com_sprintf(line, sizeof(line),
                    "set %s \"%s\"\n",
                    cvar->name, cvar->resetString);
        FS_Printf(fileHandle, "%s", line);
    }
}

/* Source: CoDUOMP.exe 0x0043ea30..0x0043eb72.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043ea30_0043eb73.mcode.
 * Name: same-family recovered Linux engine Cvar_DumpToChannel. A zero-channel
 * dump is suppressed unless logfile is active; an optional command argument
 * filters printed names without changing the reported total count. */
void Cvar_DumpToChannel(int32_t channel)
{
    const char *const match =
        Cmd_Argc() > 1 ? Cmd_Argv(1) : NULL;

    if (channel == 0 &&
        (com_logfile == NULL || com_logfile->integer == 0)) {
        return;
    }

    Com_PrintMessage(
        channel,
        "=============================== CVAR DUMP "
        "========================================\n");

    char line[CVAR_BIG_INFO_STRING_SIZE];
    int32_t count = 0;
    for (cvar_t *cvar = cvar_vars;
         cvar != NULL;
         cvar = cvar->next) {
        if (match == NULL ||
            Com_Filter(match, cvar->name, qfalse) != qfalse) {
            if (cvar->latchedString != NULL) {
                Com_sprintf(
                    line, sizeof(line),
                    "      %s \"%s\" -- latched \"%s\"\n",
                    cvar->name, cvar->string,
                    cvar->latchedString);
            } else {
                Com_sprintf(
                    line, sizeof(line),
                    "      %s \"%s\"\n",
                    cvar->name, cvar->string);
            }
            Com_PrintMessage(channel, line);
        }
        ++count;
    }

    Com_sprintf(
        line, sizeof(line),
        "\n%i total cvars\n%i cvar indexes\n",
        count, cvar_numIndexes);
    Com_PrintMessage(channel, line);
    Com_PrintMessage(
        channel,
        "=============================== END CVAR DUMP "
        "====================================\n");
}

/* Source: CoDUOMP.exe 0x0043ec60..0x0043ece7, recovered from the executable
 * gap between PbCvarValidate and Cvar_SetConfigstringValues.
 * Name: exact same-module Mac symbol Cvar_Restart_f. */
void Cvar_Restart_f(void)
{
    cvar_t **link = &cvar_vars;
    while (*link != NULL) {
        cvar_t *const cvar = *link;
        if ((cvar->flags & CVAR_RESTART_PRESERVE_MASK) != 0) {
            link = &cvar->next;
            continue;
        }

        if ((cvar->flags & CVAR_USER_CREATED) != 0) {
            *link = cvar->next;

            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            const uint32_t hash = generateHashValue(cvar->name);
            cvar_t **hashLink = &cvarHashTable[hash];
            while (*hashLink != NULL && *hashLink != cvar)
                hashLink = &(*hashLink)->hashNext;
            if (*hashLink == cvar)
                *hashLink = cvar->hashNext;

            if (cvar->name != NULL)
                Z_FreeInternal(cvar->name);
            if (cvar->string != NULL)
                Z_FreeInternal(cvar->string);
            if (cvar->latchedString != NULL)
                Z_FreeInternal(cvar->latchedString);
            if (cvar->resetString != NULL)
                Z_FreeInternal(cvar->resetString);
            memset(cvar, 0, sizeof(*cvar));
            continue;
        }

        (void)Cvar_Set2(cvar->name, cvar->resetString, qtrue);
        link = &cvar->next;
    }
}

/* Source: CoDUOMP.exe 0x0043edd0..0x0043ee6c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043edd0_0043ee6d.mcode.
 * Name: exact same-module Mac symbol Cvar_AddCommands. */
void Cvar_AddCommands(void)
{
    Cmd_AddCommand("toggle", Cvar_Toggle_f);
    Cmd_AddCommand("set", Cvar_Set_f);
    Cmd_AddCommand("sets", Cvar_SetS_f);
    Cmd_AddCommand("setu", Cvar_SetU_f);
    Cmd_AddCommand("seta", Cvar_SetA_f);
    Cmd_AddCommand("setfromcvar", Cvar_SetFromCvar_f);
    Cmd_AddCommand("reset", Cvar_Reset_f);
    Cmd_AddCommand("cvarlist", Cvar_List_f);
    Cmd_AddCommand("cvardump", Cvar_Dump_f);
    Cmd_AddCommand("cvar_restart", Cvar_Restart_f);
}

/* Source: CoDUOMP.exe 0x0043d940..0x0043d9df.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043d940_0043d9e0.mcode.
 * Name and field ownership: same-family recovered Linux engine symbol
 * Cvar_Shutdown; all four freed pointer offsets and hashNext are independently
 * proved by the Windows body. */
void Cvar_Shutdown(void)
{
    for (int32_t bucket = 0; bucket < CVAR_HASH_BUCKET_COUNT; ++bucket) {
        for (cvar_t *cvar = cvarHashTable[bucket];
             cvar != NULL;
             cvar = cvar->hashNext) {
            if (cvar->latchedString != NULL) {
                Z_FreeInternal(cvar->latchedString);
                cvar->latchedString = NULL;
            }
            if (cvar->string != NULL) {
                Z_FreeInternal(cvar->string);
                cvar->string = NULL;
            }
            if (cvar->resetString != NULL) {
                Z_FreeInternal(cvar->resetString);
                cvar->resetString = NULL;
            }
            Z_FreeInternal(cvar->name);
            cvar->name = NULL;
        }
        cvarHashTable[bucket] = NULL;
    }

    cvar_numIndexes = 0;
    cvar_vars = NULL;
    sv_console_lockout = NULL;
    sv_cheats = NULL;
    cvar_modifiedFlags = 0;
}
