#include "ui_runtime.h"

#include "ui_parse.h"
#include "client/common/client_format_validation.h"

#include <stddef.h>

void Com_Printf(const char *format, ...);

/* NOT_FROM_ORIGINAL_SOURCE: mounted menu scripts provide the format used by
 * both cgame and UI navigation. Invalid templates are logged and treated as
 * literal menu names rather than consuming nonexistent variadic arguments. */
static const char *client_menu_compat_format_game_type(const char *format, const char *gameType)
{
    if (client_compat_validate_format_signature(format, "s") == qfalse) {
        Com_Printf("WARNING: rejected invalid game-type menu format\n");
        return format;
    }
    return va(format, gameType);
}

/*
 * Complete menu-script navigation cluster.  Each authoritative Windows
 * cgame/UI pair is instruction-identical after rebasing image-local calls,
 * globals, constants, and the compiler's security-cookie storage:
 *
 *                              cgame       UI
 * Script_Open                  0x30051b20  0x40013670
 * Script_OpenForGameType       0x30051b50  0x400136a0
 * Script_CloseForGameType      0x30051bd0  0x40013720
 * Script_ConditionalOpen       0x30051c60  0x400137b0
 * Script_Close                 0x30051ce0  0x40013830
 * Script_InGameOpen            0x30051d10  0x40013860
 * Script_InGameClose           0x30051d50  0x400138a0
 *
 * The same-target bodies use the same branches, display-context callback
 * offsets, item/menu fields, token order, and 1,024-byte game-type buffer.
 */

void Script_Open(itemDef_t *item, char **arguments)
{
    const char *name;
    menuDef_t *menu;

    (void)item;
    if (!String_Parse(arguments, &name)) {
        return;
    }
    menu = Menus_FindByName(name);
    if (menu != NULL) {
        Menus_Open(menu);
    }
}

void Script_OpenForGameType(itemDef_t *item, char **arguments)
{
    const char *format;
    char gameType[MAX_STRING_CHARS];
    menuDef_t *menu;

    if (!String_Parse(arguments, &format)) {
        return;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (item->cvar == NULL) {
        return;
    }
    DC->getCVarString(item->cvar, gameType, sizeof(gameType));
    /* NOT_FROM_ORIGINAL_SOURCE: apply the mounted-menu one-string formatting
     * contract before resolving the menu name. */
    menu = Menus_FindByName(client_menu_compat_format_game_type(format, gameType));
    if (menu != NULL) {
        Menus_Open(menu);
    }
}

void Script_CloseForGameType(itemDef_t *item, char **arguments)
{
    const char *format;
    char gameType[MAX_STRING_CHARS];
    menuDef_t *menu;

    if (!String_Parse(arguments, &format)) {
        return;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (item->cvar == NULL) {
        return;
    }
    DC->getCVarString(item->cvar, gameType, sizeof(gameType));
    menu = Menus_FindByName(client_menu_compat_format_game_type(format, gameType));
    if (menu != NULL) {
        Menus_Close(menu);
    }
}

void Script_ConditionalOpen(itemDef_t *item, char **arguments)
{
    const char *cvarName;
    const char *menuIfNonzero;
    const char *menuIfZero;
    const char *menuName;
    menuDef_t *menu;

    (void)item;
    if (!String_Parse(arguments, &cvarName) || !String_Parse(arguments, &menuIfNonzero) || !String_Parse(arguments, &menuIfZero)) {
        return;
    }

    menuName = menuIfZero;
    if (DC->getCVarValue(cvarName) != 0.0f) {
        menuName = menuIfNonzero;
    }
    menu = Menus_FindByName(menuName);
    if (menu != NULL) {
        Menus_Open(menu);
    }
}

void Script_Close(itemDef_t *item, char **arguments)
{
    const char *name;
    menuDef_t *menu;

    (void)item;
    if (!String_Parse(arguments, &name)) {
        return;
    }
    menu = Menus_FindByName(name);
    if (menu != NULL) {
        Menus_Close(menu);
    }
}

void Script_InGameOpen(itemDef_t *item, char **arguments)
{
    const char *name;
    menuDef_t *menu;

    (void)item;
    if (!String_Parse(arguments, &name)) {
        return;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (!DC->runningGame()) {
        return;
    }
    menu = Menus_FindByName(name);
    if (menu != NULL) {
        Menus_Open(menu);
    }
}

void Script_InGameClose(itemDef_t *item, char **arguments)
{
    const char *name;
    menuDef_t *menu;

    (void)item;
    if (!String_Parse(arguments, &name)) {
        return;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (!DC->runningGame()) {
        return;
    }
    menu = Menus_FindByName(name);
    if (menu != NULL) {
        Menus_Close(menu);
    }
}
