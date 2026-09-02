#include "ui_runtime.h"

#include "ui_parse.h"

#include <string.h>

enum {
    UI_SCRIPT_COMMAND_COUNT = 30,
    UI_SCRIPT_COMMAND_COMPARE_LIMIT = 99999
};

/*
 * Complete menu-script dispatch subsystem.  The authoritative Windows bodies
 * are instruction-identical after rebasing calls, the display context, the
 * command table, the command count, and the compiler's security cookie:
 *
 *                                  cgame       UI
 * Item_RunScript                   0x300526e0  0x40014230
 * command table                    0x3008acc0  0x400405b8
 * command count                    0x3008adb0  0x400406a8
 *
 * Both supporting Mac modules export Item_RunScript and the same Script_*
 * handlers.  The table and its separately loaded count remain writable
 * initialized data, as in both original PE32 modules.
 */
static commandDef_t scriptCommandList[UI_SCRIPT_COMMAND_COUNT] = {
    { "fadein", Script_FadeIn },
    { "fadeout", Script_FadeOut },
    { "show", Script_Show },
    { "hide", Script_Hide },
    { "setcolor", Script_SetColor },
    { "open", Script_Open },
    { "openforgametype", Script_OpenForGameType },
    { "closeforgametype", Script_CloseForGameType },
    { "conditionalopen", Script_ConditionalOpen },
    { "close", Script_Close },
    { "ingameopen", Script_InGameOpen },
    { "ingameclose", Script_InGameClose },
    { "setasset", Script_SetAsset },
    { "setbackground", Script_SetBackground },
    { "setitemcolor", Script_SetItemColor },
    { "setteamcolor", Script_SetTeamColor },
    { "setfocus", Script_SetFocus },
    { "setplayermodel", Script_SetPlayerModel },
    { "setplayerhead", Script_SetPlayerHead },
    { "transition", Script_Transition },
    { "setcvar", Script_SetCvar },
    { "exec", Script_Exec },
    { "execOnCvarStringValue", Script_ExecOnCvarStringValue },
    { "execOnCvarIntValue", Script_ExecOnCvarIntValue },
    { "execOnCvarFloatValue", Script_ExecOnCvarFloatValue },
    { "play", Script_Play },
    { "orbit", Script_Orbit },
    { "addlistitem", Script_AddListItem },
    { "getautoupdate", Script_GetAutoUpdate },
    { "scriptmenuresponse", Script_ScriptMenuResponse }
};
static int32_t scriptCommandCount = UI_SCRIPT_COMMAND_COUNT;

void Item_RunScript(itemDef_t *item, const char *script)
{
    char buffer[MAX_STRING_CHARS];
    char *cursor;
    const char *token;

    /* Both originals clear the complete 0x400-byte work buffer before testing
     * either argument. */
    memset(buffer, 0, sizeof(buffer));
    if (item == NULL || script == NULL || script[0] == '\0') {
        return;
    }

    Q_strcat(buffer, MAX_STRING_CHARS, script);
    cursor = buffer;
    if (!String_Parse(&cursor, &token)) {
        return;
    }

    do {
        qboolean matched = qfalse;
        int32_t index;

        if (token[0] == ';' && token[1] == '\0') {
            continue;
        }
        for (index = 0; index < scriptCommandCount; ++index) {
            const char *name = scriptCommandList[index].name;

            if (name != NULL &&
                Q_stricmpn(name, token,
                           UI_SCRIPT_COMMAND_COMPARE_LIMIT) == 0) {
                scriptCommandList[index].handler(item, &cursor);
                matched = qtrue;
                break;
            }
        }
        if (matched == qfalse) {
            DC->runScript(&cursor);
        }
    } while (String_Parse(&cursor, &token));
}
