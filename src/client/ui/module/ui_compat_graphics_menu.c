#include "ui_functions.h"
#include "ui_globals.h"
#include "client/common/client_branding.h"
#include "client/common/client_legacy_crt.h"

#include <string.h>

/* This translation unit is an isolated improved compatibility interface. */

typedef struct ui_compat_multi_value_s {
    const char *label;
    float value;
} ui_compat_multi_value_t;

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: labels and stable renderer mode
 * numbers used to replace the retail seven-entry resolution selector. */
static const ui_compat_multi_value_t uiCompatResolutions[] = {{"@CODUOMP_GRAPHICS_CURRENT_DISPLAY", -2.0f},
                                                              {"640 x 480 (4:3)", 3.0f},
                                                              {"800 x 600 (4:3)", 4.0f},
                                                              {"1024 x 768 (4:3)", 6.0f},
                                                              {"1152 x 864 (4:3)", 7.0f},
                                                              {"1280 x 720 (16:9)", 13.0f},
                                                              {"1280 x 800 (16:10)", 14.0f},
                                                              {"1280 x 1024 (5:4)", 8.0f},
                                                              {"1366 x 768 (16:9)", 15.0f},
                                                              {"1440 x 900 (16:10)", 16.0f},
                                                              {"1600 x 900 (16:9)", 17.0f},
                                                              {"1600 x 1200 (4:3)", 9.0f},
                                                              {"1680 x 1050 (16:10)", 18.0f},
                                                              {"1920 x 1080 (16:9)", 19.0f},
                                                              {"1920 x 1200 (16:10)", 12.0f},
                                                              {"2560 x 1440 (16:9)", 20.0f},
                                                              {"2560 x 1600 (16:10)", 21.0f},
                                                              {"2880 x 1800 (16:10)", 22.0f},
                                                              {"3024 x 1964 (16:10)", 23.0f},
                                                              {"3456 x 2234 (16:10)", 24.0f},
                                                              {"3840 x 2160 (16:9)", 25.0f}};

/* NOT_FROM_ORIGINAL_SOURCE: joins persistent parsed-menu scripts without
 * depending on a particular localized retail asset's existing contents. */
static const char *ui_compat_prepend_menu_script(const char *prefix, const char *script)
{
    const size_t prefixLength = strlen(prefix);
    const size_t scriptLength = script != NULL ? strlen(script) : 0;
    char *const joined = UI_Alloc(prefixLength + scriptLength + 1);

    memcpy(joined, prefix, prefixLength);
    if (scriptLength != 0)
        memcpy(joined + prefixLength, script, scriptLength);
    joined[prefixLength + scriptLength] = '\0';
    return joined;
}

/* NOT_FROM_ORIGINAL_SOURCE: adds the compatibility cvar commit to every
 * matching confirmation control in one parsed popup menu. */
static void ui_compat_prepend_named_item_actions(menuDef_t *menu, const char *itemName, const char *prefix)
{
    if (menu == NULL)
        return;

    for (int32_t index = 0; index < menu->itemCount; ++index) {
        itemDef_t *const item = menu->items[index];

        if (item != NULL && item->window.name != NULL && Q_stricmp(item->window.name, itemName) == 0) {
            item->action = ui_compat_prepend_menu_script(prefix, item->action);
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: locates a parsed menu control by cvar so the
 * adaptation remains independent of localized item names and labels. */
static itemDef_t *ui_compat_find_cvar_item(menuDef_t *menu, const char *cvar, int32_t *itemIndex)
{
    for (int32_t index = 0; index < menu->itemCount; ++index) {
        itemDef_t *const item = menu->items[index];

        if (item != NULL && item->cvar != NULL && Q_stricmp(item->cvar, cvar) == 0) {
            if (itemIndex != NULL)
                *itemIndex = index;
            return item;
        }
    }
    return NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: locates a parsed menu control by a stable command
 * fragment when retail gives every right-side options entry the same name. */
static itemDef_t *ui_compat_find_action_item(menuDef_t *menu, const char *command, int32_t *itemIndex)
{
    for (int32_t index = 0; index < menu->itemCount; ++index) {
        itemDef_t *const item = menu->items[index];

        if (item != NULL && item->action != NULL && strstr(item->action, command) != NULL) {
            if (itemIndex != NULL)
                *itemIndex = index;
            return item;
        }
    }
    return NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: duplicates one parsed item into the persistent UI
 * memory pool and assigns ownership to its code-created menu. */
static itemDef_t *ui_compat_clone_menu_item(const itemDef_t *source, menuDef_t *parent)
{
    itemDef_t *const item = UI_Alloc(sizeof(*item));

    memcpy(item, source, sizeof(*item));
    item->parent = parent;
    item->window.flags &= ~(WINDOW_MOUSEOVER | WINDOW_HASFOCUS | WINDOW_MOUSEOVERTEXT);
    return item;
}

/* NOT_FROM_ORIGINAL_SOURCE: initializes a numeric multi selector with bounded
 * arrays owned by the parsed-menu memory pool. */
static void ui_compat_set_numeric_multi(itemDef_t *item, const ui_compat_multi_value_t *values, int32_t count)
{
    multiDef_t *multi = UI_Alloc(sizeof(*multi));

    memset(multi, 0, sizeof(*multi));
    item->type = ITEM_TYPE_MULTI;
    item->typeValidated = ITEM_TYPE_MULTI;
    item->typeData = multi;
    multi->strDef = 0;
    multi->count = count;
    for (int32_t index = 0; index < count; ++index) {
        multi->cvarList[index] = String_Alloc(values[index].label);
        multi->cvarValue[index] = values[index].value;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: extends the parsed retail graphics menu in memory
 * so no proprietary menu asset is copied into the source distribution. */
void ui_compat_extend_graphics_menu(void)
{
    static const char stageCompatibilityCvars[] = "exec \"setfromcvar ui_r_aspectMode r_aspectMode\"; ";
    static const char applyCompatibilityCvars[] = "exec \"setfromcvar r_aspectMode ui_r_aspectMode\"; ";
    static const ui_compat_multi_value_t displayModes[] = {
        {"@CODUOMP_GRAPHICS_WINDOWED", 0.0f}, {"@CODUOMP_GRAPHICS_FULLSCREEN", 1.0f}, {"@CODUOMP_GRAPHICS_BORDERLESS", 2.0f}};
    static const ui_compat_multi_value_t aspectModes[] = {{"@CODUOMP_GRAPHICS_FILL_SCREEN", 0.0f}, {"@CODUOMP_GRAPHICS_LETTERBOX", 1.0f}};
    menuDef_t *const menu = Menus_FindByName("options_graphics");
    menuDef_t *const restartMenu = Menus_FindByName("vid_restart_popmenu");
    menuDef_t *const listenRestartMenu = Menus_FindByName("vid_restart_popmenu_listen");
    itemDef_t *resolutionItem;
    itemDef_t *displayModeItem;
    itemDef_t *aspectItem;
    ui_compat_multi_value_t resolutionModes[sizeof(uiCompatResolutions) / sizeof(uiCompatResolutions[0])];
    int32_t resolutionModeCount = 0;
    int32_t displayModeIndex;
    const uint32_t availableModes = (uint32_t)coduo_crt_atoi(UI_Cvar_VariableString("r_availableModes"));

    if (menu == NULL)
        return;

    resolutionItem = ui_compat_find_cvar_item(menu, "ui_r_mode", NULL);
    displayModeItem = ui_compat_find_cvar_item(menu, "ui_r_fullscreen", &displayModeIndex);
    if (resolutionItem == NULL || displayModeItem == NULL || menu->itemCount > MAX_MENUITEMS - 1) {
        return;
    }

    resolutionModes[resolutionModeCount++] = uiCompatResolutions[0];
    for (size_t index = 1; index < sizeof(uiCompatResolutions) / sizeof(uiCompatResolutions[0]); ++index) {
        const int32_t mode = (int32_t)uiCompatResolutions[index].value;

        if ((availableModes & (UINT32_C(1) << mode)) != 0)
            resolutionModes[resolutionModeCount++] = uiCompatResolutions[index];
    }
    ui_compat_set_numeric_multi(resolutionItem, resolutionModes, resolutionModeCount);

    ui_compat_set_numeric_multi(displayModeItem, displayModes, (int32_t)(sizeof(displayModes) / sizeof(displayModes[0])));
    displayModeItem->text = String_Alloc("@CODUOMP_GRAPHICS_DISPLAY_MODE");

    /* The stock graphics page stages every restart-sensitive control in a
     * ui_r_* cvar, reveals Apply on change, and commits only in the restart
     * confirmation popup. Keep the compatibility control in that flow. */
    menu->onOpen = ui_compat_prepend_menu_script(stageCompatibilityCvars, menu->onOpen);
    ui_compat_prepend_named_item_actions(restartMenu, "yes", applyCompatibilityCvars);
    ui_compat_prepend_named_item_actions(listenRestartMenu, "ok", applyCompatibilityCvars);
    if (listenRestartMenu != NULL) {
        listenRestartMenu->onESC = ui_compat_prepend_menu_script(applyCompatibilityCvars, listenRestartMenu->onESC);
    }

    /* Make one control row below Fullscreen, preserving the stock Apply and
     * language rows at the bottom of the panel. */
    for (int32_t index = 0; index < menu->itemCount; ++index) {
        itemDef_t *const item = menu->items[index];

        if (item->window.rectClient.y >= 110.0f && item->window.rectClient.y <= 200.0f) {
            item->window.rectClient.y += 15.0f;
        }
    }

    aspectItem = UI_Alloc(sizeof(*aspectItem));
    memcpy(aspectItem, displayModeItem, sizeof(*aspectItem));
    aspectItem->window.name = String_Alloc("coduomp_aspect_mode");
    aspectItem->window.rectClient.y = 110.0f;
    aspectItem->text = String_Alloc("@CODUOMP_GRAPHICS_GAMEPLAY_VIEW");
    aspectItem->cvar = String_Alloc("ui_r_aspectMode");
    aspectItem->action = String_Alloc("play \"mouse_click\" ; show graphicsapply ; ");
    aspectItem->parent = menu;
    ui_compat_set_numeric_multi(aspectItem, aspectModes, (int32_t)(sizeof(aspectModes) / sizeof(aspectModes[0])));

    for (int32_t index = menu->itemCount; index > displayModeIndex + 1; --index) {
        menu->items[index] = menu->items[index - 1];
    }
    menu->items[displayModeIndex + 1] = aspectItem;
    menu->itemCount += 1;
    Menu_UpdatePosition(menu);
}

/* NOT_FROM_ORIGINAL_SOURCE: adds the console binding contemplated by the
 * retail options_misc asset without copying or replacing that proprietary
 * menu.  The screenshot row supplies the established multiplayer control
 * styling, and the adjacent single-player quick-save row is hidden because
 * ui_multiplayer is a read-only one in this module. */
void ui_compat_extend_console_binding_menu(void)
{
    menuDef_t *const menu = Menus_FindByName("options_misc");
    itemDef_t *screenshotItem;
    itemDef_t *consoleItem;
    int32_t screenshotIndex;

    if (menu == NULL || ui_compat_find_cvar_item(menu, "toggleconsole", NULL) != NULL || menu->itemCount >= MAX_MENUITEMS) {
        return;
    }

    screenshotItem = ui_compat_find_cvar_item(menu, "screenshotjpeg", &screenshotIndex);
    if (screenshotItem == NULL)
        return;

    consoleItem = UI_Alloc(sizeof(*consoleItem));
    memcpy(consoleItem, screenshotItem, sizeof(*consoleItem));
    consoleItem->window.name = String_Alloc("coduomp_open_console");
    consoleItem->window.rectClient.y += 15.0f;
    consoleItem->text = String_Alloc("Open Console");
    consoleItem->cvar = String_Alloc("toggleconsole");
    consoleItem->parent = menu;

    for (int32_t index = menu->itemCount; index > screenshotIndex + 1; --index) {
        menu->items[index] = menu->items[index - 1];
    }
    menu->items[screenshotIndex + 1] = consoleItem;
    ++menu->itemCount;
    Menu_UpdatePosition(menu);
}

/* NOT_FROM_ORIGINAL_SOURCE: creates an Advanced System page and navigation
 * entry entirely from parsed menu records. This keeps the server-cache option
 * independent of, and absent from, the user's proprietary game assets. */
void ui_compat_extend_advanced_menu(void)
{
    static const char closeAdvancedMenu[] = "close options_advanced; ";
    static const char openAdvancedMenu[] = "play \"mouse_click\"; "
                                           "close options_shoot; close options_move; close options_misc; "
                                           "close options_look; close options_graphics; close options_sound; "
                                           "close options_performance; close options_view; "
                                           "close options_defaults; close options_driverinfo; "
                                           "close options_credits; close options_multi; "
                                           "close options_graphics_defaults; "
                                           "close options_control_defaults; open options_advanced; ";
    enum {
        UI_COMPAT_ADVANCED_TEMPLATE_ITEM_COUNT = 4
    };
    menuDef_t *const optionsMenu = Menus_FindByName("options_menu");
    menuDef_t *const performanceMenu = Menus_FindByName("options_performance");
    itemDef_t *performanceNavigation;
    itemDef_t *serverCacheItem;
    itemDef_t *advancedNavigation;
    itemDef_t *controlTemplate;
    menuDef_t *advancedMenu;
    int32_t performanceNavigationIndex;

    if (optionsMenu == NULL || performanceMenu == NULL || Menus_FindByName("options_advanced") != NULL || menuCount >= MAX_MENUS ||
        optionsMenu->itemCount >= MAX_MENUITEMS || performanceMenu->itemCount < UI_COMPAT_ADVANCED_TEMPLATE_ITEM_COUNT) {
        return;
    }

    performanceNavigation = ui_compat_find_action_item(optionsMenu, "open options_performance", &performanceNavigationIndex);
    controlTemplate = ui_compat_find_cvar_item(performanceMenu, "r_swapinterval", NULL);
    if (performanceNavigation == NULL || controlTemplate == NULL)
        return;

    advancedMenu = &Menus[menuCount];
    memcpy(advancedMenu, performanceMenu, sizeof(*advancedMenu));
    advancedMenu->window.name = String_Alloc("options_advanced");
    advancedMenu->window.flags &= ~(WINDOW_MOUSEOVER | WINDOW_HASFOCUS | WINDOW_VISIBLE | WINDOW_MOUSEOVERTEXT);
    advancedMenu->itemCount = 0;
    advancedMenu->cursorItem = -1;
    advancedMenu->onOpen = NULL;
    advancedMenu->onClose = NULL;
    memset(advancedMenu->items, 0, sizeof(advancedMenu->items));

    for (int32_t index = 0; index < UI_COMPAT_ADVANCED_TEMPLATE_ITEM_COUNT; ++index) {
        advancedMenu->items[index] = ui_compat_clone_menu_item(performanceMenu->items[index], advancedMenu);
        ++advancedMenu->itemCount;
    }
    advancedMenu->items[UI_COMPAT_ADVANCED_TEMPLATE_ITEM_COUNT - 1]->text = String_Alloc("@CODUOMP_ADVANCED");

    serverCacheItem = ui_compat_clone_menu_item(controlTemplate, advancedMenu);
    serverCacheItem->window.name = String_Alloc("coduomp_server_cache");
    serverCacheItem->window.rectClient.y = 40.0f;
    serverCacheItem->text = String_Alloc("@CODUOMP_SERVER_CACHE");
    serverCacheItem->cvar = String_Alloc("cl_serverCache");
    serverCacheItem->action = String_Alloc("play \"mouse_click\"; ");
    advancedMenu->items[advancedMenu->itemCount] = serverCacheItem;
    ++advancedMenu->itemCount;
    Menu_UpdatePosition(advancedMenu);
    ++menuCount;

    /* Existing navigation scripts know only the retail pages. Close the new
     * panel before any of them opens its destination, and close it when the
     * parent options menu itself leaves the stack. */
    for (int32_t index = 0; index < optionsMenu->itemCount; ++index) {
        itemDef_t *const item = optionsMenu->items[index];

        if (item != NULL && item->action != NULL && strstr(item->action, "open options_") != NULL) {
            item->action = ui_compat_prepend_menu_script(closeAdvancedMenu, item->action);
        }
    }
    optionsMenu->onClose = ui_compat_prepend_menu_script(closeAdvancedMenu, optionsMenu->onClose);

    /* Reset System Defaults and the developer-only Driver Info entry occupy
     * the two rows after Performance in the retail menu. Make room for the
     * requested Advanced row without moving the fixed Back entry. */
    for (int32_t index = 0; index < optionsMenu->itemCount; ++index) {
        itemDef_t *const item = optionsMenu->items[index];

        if (item != NULL && item->action != NULL &&
            (strstr(item->action, "open options_graphics_defaults") != NULL || strstr(item->action, "open options_driverinfo") != NULL)) {
            item->window.rectClient.y += 15.0f;
        }
    }

    advancedNavigation = ui_compat_clone_menu_item(performanceNavigation, optionsMenu);
    advancedNavigation->window.name = String_Alloc("coduomp_advanced_options");
    advancedNavigation->window.rectClient.y += 15.0f;
    advancedNavigation->text = String_Alloc("@CODUOMP_ADVANCED");
    advancedNavigation->action = String_Alloc(openAdvancedMenu);

    for (int32_t index = optionsMenu->itemCount; index > performanceNavigationIndex + 1; --index) {
        optionsMenu->items[index] = optionsMenu->items[index - 1];
    }
    optionsMenu->items[performanceNavigationIndex + 1] = advancedNavigation;
    ++optionsMenu->itemCount;
    Menu_UpdatePosition(optionsMenu);
}

/* NOT_FROM_ORIGINAL_SOURCE: replaces the parsed retail main-menu version
 * text in memory while retaining shortversion=1.51 for network compatibility
 * and leaving the proprietary menu asset unchanged. */
void ui_compat_brand_main_menu_version(void)
{
    menuDef_t *const menu = Menus_FindByName("main");

    if (menu == NULL)
        return;

    for (int32_t index = 0; index < menu->itemCount; ++index) {
        itemDef_t *const item = menu->items[index];

        if (item != NULL && item->window.name != NULL && item->cvar != NULL &&
            Q_stricmp(item->window.name, "background_version_display") == 0 && Q_stricmp(item->cvar, "shortversion") == 0) {
            item->text = String_Alloc(CODUOMP_DISPLAY_LABEL);
            item->textRect.w = 0.0f;
            item->textRect.h = 0.0f;
            break;
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: removes the retail Single Player launcher from
 * the normal recovered client's parsed main menu. The replacement client has
 * no single-player executable to launch, and the proprietary menu asset stays
 * unchanged. */
void ui_compat_remove_single_player_menu_item(void)
{
    menuDef_t *const menu = Menus_FindByName("main");
    float vacatedRowY = 0.0f;
    qboolean removed = qfalse;

    if (menu == NULL)
        return;

    for (int32_t index = 0; index < menu->itemCount; ++index) {
        itemDef_t *const item = menu->items[index];

        if (item == NULL || item->text == NULL || Q_stricmp(item->text, "@MENU_SINGLE_PLAYER") != 0) {
            continue;
        }

        vacatedRowY = item->window.rectClient.y;
        for (int32_t next = index + 1; next < menu->itemCount; ++next)
            menu->items[next - 1] = menu->items[next];
        --menu->itemCount;
        menu->items[menu->itemCount] = NULL;
        removed = qtrue;
        break;
    }

    if (removed == qfalse)
        return;

    for (int32_t index = 0; index < menu->itemCount; ++index) {
        itemDef_t *const item = menu->items[index];

        if (item != NULL && item->text != NULL && Q_stricmp(item->text, "@MENU_QUIT") == 0 && item->window.rectClient.y > vacatedRowY) {
            item->window.rectClient.y = vacatedRowY;
            break;
        }
    }
    Menu_UpdatePosition(menu);
}
