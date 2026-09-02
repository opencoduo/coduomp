#include "ui_runtime.h"

#include "client/common/client_legacy_crt.h"
#include "compat/coduo_fp_conversion.h"
#include "ui_parse.h"

/*
 * Complete list/automatic-update/script-response menu-command cluster.  The
 * authoritative Windows cgame/UI bodies are instruction-identical after
 * rebasing module-local strings, calls, and the display-context global:
 *
 *                                  cgame       UI
 * Script_AddListItem               0x30052570  0x400140c0
 * Script_GetAutoUpdate             0x30052610  0x40014160
 * Script_ScriptMenuResponse        0x30052620  0x40014170
 *
 * Script_ScriptMenuResponse uses the getConfigString callback at +0x74 in
 * both DLLs.  The former cgame spelling as a direct CG_ConfigString call was a
 * reconstruction error.  Both originals convert sv_serverId through the
 * common MSVC _ftol2 result path and consume its low dword.
 */

void Script_AddListItem(itemDef_t *item, char **arguments)
{
    const char *listName;
    const char *value;
    const char *name;
    itemDef_t *target;

    if (!String_Parse(arguments, &listName) || !String_Parse(arguments, &value) || !String_Parse(arguments, &name)) {
        return;
    }
    target = Menu_FindItemByName(item->parent, listName);
    if (target != NULL && target->special != 0.0f) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        DC->feederAddItem(target->special, name, coduo_crt_atoi(value));
    }
}

void Script_GetAutoUpdate(itemDef_t *item, char **arguments)
{
    (void)item;
    (void)arguments;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    DC->getAutoUpdate();
}

void Script_ScriptMenuResponse(itemDef_t *item, char **arguments)
{
    enum {
        SCRIPT_MENU_CONFIG_BASE = 1333,
        SCRIPT_MENU_CONFIG_COUNT = 32,
        SCRIPT_MENU_NOT_FOUND = -1,
        SCRIPT_MENU_NAME_COMPARE_LIMIT = 99999
    };
    const char *response;
    int32_t menuIndex;
    int32_t serverId;

    if (DC->getCVarValue("ui_scriptMenuAllowResponse") == 0.0f || !String_Parse(arguments, &response)) {
        return;
    }

    for (menuIndex = 0; menuIndex < SCRIPT_MENU_CONFIG_COUNT; ++menuIndex) {
        const char *menuName = DC->getConfigString(SCRIPT_MENU_CONFIG_BASE + menuIndex);

        if (menuName[0] != '\0' && item->parent->window.name != NULL &&
            Q_stricmpn(menuName, item->parent->window.name, SCRIPT_MENU_NAME_COMPARE_LIMIT) == 0) {
            break;
        }
    }
    if (menuIndex == SCRIPT_MENU_CONFIG_COUNT) {
        menuIndex = SCRIPT_MENU_NOT_FOUND;
    }

    serverId = coduo_fp_to_i32_extended(DC->getCVarValue("sv_serverId"));
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    DC->executeText(EXEC_APPEND, va("cmd mr %i %i %s\n", serverId, menuIndex, response));
}
