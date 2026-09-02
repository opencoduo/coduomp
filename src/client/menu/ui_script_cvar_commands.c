#include "ui_runtime.h"

#include "client/common/client_legacy_crt.h"
#include "ui_parse.h"

#include <math.h>
#include <stdlib.h>

enum {
    UI_SCRIPT_CVAR_VALUE_SIZE = MAX_STRING_CHARS,
    UI_SCRIPT_CVAR_STRING_COMPARE_LIMIT = 99999
};

/*
 * Complete cvar/exec/play menu-script command family.  Every authoritative
 * Windows cgame/UI pair is instruction-identical after rebasing image-local
 * storage, strings, and calls:
 *
 *                                  cgame       UI
 * Script_SetPlayerModel            0x30052230  0x40013d80
 * Script_SetPlayerHead             0x30052260  0x40013db0
 * Script_SetCvar                   0x30052290  0x40013de0
 * Script_Exec                      0x300522e0  0x40013e30
 * Script_ExecOnCvarStringValue     0x30052320  0x40013e70
 * Script_ExecOnCvarIntValue        0x300523e0  0x40013f30
 * Script_ExecOnCvarFloatValue      0x300524a0  0x40013ff0
 * Script_Play                      0x30052540  0x40014090
 *
 * Both float handlers spill getCVarValue to binary64, call the same MSVC
 * atof body, take x87 FABS, and compare against the same binary64 widening of
 * the binary32 literal 0.00001f (bits 0x3ee4f8b580000000).
 */

void Script_SetPlayerModel(itemDef_t *item, char **arguments)
{
    const char *value;

    (void)item;
    if (String_Parse(arguments, &value)) {
        DC->setCVar("team_model", value);
    }
}

void Script_SetPlayerHead(itemDef_t *item, char **arguments)
{
    const char *value;

    (void)item;
    if (String_Parse(arguments, &value)) {
        DC->setCVar("team_headmodel", value);
    }
}

void Script_SetCvar(itemDef_t *item, char **arguments)
{
    const char *name;
    const char *value;

    (void)item;
    if (!String_Parse(arguments, &name) ||
        !String_Parse(arguments, &value)) {
        return;
    }
    DC->setCVar(name, value);
}

void Script_Exec(itemDef_t *item, char **arguments)
{
    const char *command;

    (void)item;
    if (!String_Parse(arguments, &command)) {
        return;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    DC->executeText(EXEC_APPEND, va("%s ; ", command));
}

void Script_ExecOnCvarStringValue(itemDef_t *item, char **arguments)
{
    const char *cvarName;
    const char *compareValue;
    const char *command;
    char currentValue[UI_SCRIPT_CVAR_VALUE_SIZE];

    (void)item;
    if (!String_Parse(arguments, &cvarName) ||
        !String_Parse(arguments, &compareValue) ||
        !String_Parse(arguments, &command)) {
        return;
    }
    DC->getCVarString(cvarName, currentValue, sizeof(currentValue));
    if (compareValue != NULL &&
        Q_stricmpn(compareValue, currentValue,
                   UI_SCRIPT_CVAR_STRING_COMPARE_LIMIT) == 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        DC->executeText(EXEC_APPEND, va("%s\n", command));
    }
}

void Script_ExecOnCvarIntValue(itemDef_t *item, char **arguments)
{
    const char *cvarName;
    const char *compareValue;
    const char *command;
    char currentValue[UI_SCRIPT_CVAR_VALUE_SIZE];

    (void)item;
    if (!String_Parse(arguments, &cvarName) ||
        !String_Parse(arguments, &compareValue) ||
        !String_Parse(arguments, &command)) {
        return;
    }
    DC->getCVarString(cvarName, currentValue, sizeof(currentValue));
    if (coduo_crt_atoi(currentValue) == coduo_crt_atoi(compareValue)) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        DC->executeText(EXEC_APPEND, va("%s\n", command));
    }
}

void Script_ExecOnCvarFloatValue(itemDef_t *item, char **arguments)
{
    const char *cvarName;
    const char *compareValue;
    const char *command;
    double currentValue;

    (void)item;
    if (!String_Parse(arguments, &cvarName) ||
        !String_Parse(arguments, &compareValue) ||
        !String_Parse(arguments, &command)) {
        return;
    }
    currentValue = (double)DC->getCVarValue(cvarName);
    if (fabs(currentValue - atof(compareValue)) < (double)0.00001f) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        DC->executeText(EXEC_APPEND, va("%s\n", command));
    }
}

void Script_Play(itemDef_t *item, char **arguments)
{
    const char *name;

    (void)item;
    if (String_Parse(arguments, &name)) {
        DC->startLocalSound(name);
    }
}
