#include "../module/ui_functions.h"

#include <string.h>

enum { UI_MENU_KEYWORD_COMPARE_LIMIT = 99999 };

/* NOT_FROM_ORIGINAL_SOURCE: remove connection presets from each newly parsed
 * menu, including reloads and localized menus, without writing network cvars.
 * Unlink the controls so neither mouse nor keyboard input can activate them. */
static void ui_compat_remove_connection_menu_items(menuDef_t *menu)
{
    for (int32_t index = 0; index < menu->itemCount;) {
        const itemDef_t *const item = menu->items[index];

        if (item == NULL || item->type != ITEM_TYPE_MULTI ||
            item->cvar == NULL || Q_stricmp(item->cvar, "rate") != 0) {
            ++index;
            continue;
        }

        for (int32_t next = index + 1; next < menu->itemCount; ++next)
            menu->items[next - 1] = menu->items[next];
        --menu->itemCount;
        menu->items[menu->itemCount] = NULL;
    }
}

// Source: uo_ui_mp_x86.dll 0x40009300..0x400093f4
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40009300_400093f4.mcode
// Exact same-module PPC symbol and call graph: UI_ParseMenu.
qboolean UI_ParseMenu(const char *filename, int32_t loadMode)
{
    int32_t sourceHandle;
    pc_token_t token;

    Com_DPrintf("Parsing menu file:%s\n", filename);
    sourceHandle = trap_PC_LoadSource(filename);
    if (sourceHandle == 0) {
        return qfalse;
    }

    memset(&token, 0, sizeof(token));
    while (trap_PC_ReadToken(sourceHandle, &token)) {
        if (token.string[0] == '}') {
            break;
        }
        if (Q_stricmpn("assetGlobalDef", token.string,
                       UI_MENU_KEYWORD_COMPARE_LIMIT) == 0) {
            if (!Asset_Parse(sourceHandle, loadMode)) {
                break;
            }
        } else if (Q_stricmpn("menudef", token.string,
                              UI_MENU_KEYWORD_COMPARE_LIMIT) == 0) {
            const int32_t previousMenuCount = menuCount;

            Menu_New(sourceHandle, loadMode);
            if (menuCount > previousMenuCount)
                ui_compat_remove_connection_menu_items(&Menus[previousMenuCount]);
        }
        memset(&token, 0, sizeof(token));
    }

    trap_PC_FreeSource(sourceHandle);
    return qtrue;
}
