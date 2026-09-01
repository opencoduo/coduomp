#include "ui_parse.h"

#include "qcommon/q_string.h"
#include "ui_memory.h"
#include "ui_menu_globals.h"
#include "ui_runtime.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    UI_KEYWORD_COMPARE_LIMIT = 99999,
    UI_FULLSCREEN_WIDTH = 640,
    UI_FULLSCREEN_HEIGHT = 480
};

/*
 * Complete shared keyword-table setup and parser-dispatch cluster. The cgame
 * and UI PE32 bodies make the same decisions and invoke the same ordered
 * handlers. Their instruction shapes differ where the UI compiler inlines
 * KeywordHash_Key/Q_stricmpn and where cgame retains calls; these are not
 * behavioral differences. Mac traceback symbols retain the canonical names.
 *
 *                                  cgame       UI
 * Item_SetupKeywordHash            0x3005a300  0x4001be70
 * Item_Parse                       0x3005a350  0x4001bf00
 * MenuParse_itemDef                0x3005aa40  0x4001c650
 * Menu_SetupKeywordHash            0x3005aba0  0x4001c7f0
 * Menu_Parse                       0x3005abf0  0x4001c880
 * Menu_New                         0x3005ad40  0x4001ca60
 */

void Item_SetupKeywordHash(void)
{
    keywordHash_t *keyword;

    memset(itemKeywordHashTable, 0, sizeof(itemKeywordHashTable));
    for (keyword = itemParseKeywords; keyword->keyword != NULL; ++keyword) {
        const int32_t hash = KeywordHash_Key(keyword->keyword);

        keyword->next = itemKeywordHashTable[hash];
        itemKeywordHashTable[hash] = keyword;
    }
}

void Menu_SetupKeywordHash(void)
{
    menuKeywordHash_t *keyword;

    memset(menuKeywordHashTable, 0, sizeof(menuKeywordHashTable));
    for (keyword = menuParseKeywords; keyword->keyword != NULL; ++keyword) {
        const int32_t hash = KeywordHash_Key(keyword->keyword);

        keyword->next = menuKeywordHashTable[hash];
        menuKeywordHashTable[hash] = keyword;
    }
}

qboolean Item_Parse(int32_t sourceHandle, itemDef_t *item)
{
    pc_token_t token;

    if (!trap_PC_ReadToken(sourceHandle, &token) || token.string[0] != '{') {
        return qfalse;
    }

    while (trap_PC_ReadToken(sourceHandle, &token)) {
        keywordHash_t *keyword;

        if (token.string[0] == '}') {
            return qtrue;
        }

        keyword = itemKeywordHashTable[KeywordHash_Key(token.string)];
        while (keyword != NULL && keyword->keyword != NULL &&
               Q_stricmpn(token.string, keyword->keyword,
                          UI_KEYWORD_COMPARE_LIMIT) != 0) {
            keyword = keyword->next;
        }

        if (keyword == NULL || keyword->keyword == NULL) {
            PC_SourceError(sourceHandle, "unknown menu item keyword %s",
                           token.string);
            continue;
        }
        if (!keyword->func(item, sourceHandle)) {
            PC_SourceError(sourceHandle,
                           "couldn't parse menu item keyword %s",
                           token.string);
            return qfalse;
        }
    }

    PC_SourceError(sourceHandle, "end of file inside menu item\n");
    return qfalse;
}

qboolean Menu_Parse(int32_t sourceHandle, menuDef_t *menu)
{
    pc_token_t token;

    if (!trap_PC_ReadToken(sourceHandle, &token) || token.string[0] != '{') {
        return qfalse;
    }

    for (;;) {
        menuKeywordHash_t *keyword;

        memset(&token, 0, sizeof(token));
        if (!trap_PC_ReadToken(sourceHandle, &token)) {
            PC_SourceError(sourceHandle, "end of file inside menu\n");
            return qfalse;
        }
        if (token.string[0] == '}') {
            return qtrue;
        }

        keyword = menuKeywordHashTable[KeywordHash_Key(token.string)];
        while (keyword != NULL && keyword->keyword != NULL &&
               Q_stricmpn(token.string, keyword->keyword,
                          UI_KEYWORD_COMPARE_LIMIT) != 0) {
            keyword = keyword->next;
        }

        if (keyword == NULL || keyword->keyword == NULL) {
            PC_SourceError(sourceHandle, "unknown menu keyword %s",
                           token.string);
            continue;
        }
        if (!keyword->func(menu, sourceHandle)) {
            PC_SourceError(sourceHandle, "couldn't parse menu keyword %s",
                           token.string);
            return qfalse;
        }
    }
}

qboolean MenuParse_itemDef(menuDef_t *menu, int32_t sourceHandle)
{
    itemDef_t *item;

    if (menu->itemCount >= MAX_MENUITEMS) {
        return qtrue;
    }

    item = UI_Alloc(sizeof(*item));
    menu->items[menu->itemCount] = item;
    memset(item, 0, sizeof(*item));
    item->textscale = 0.55f;
    item->loadMode = menu->loadMode;

    memset(&item->window, 0, sizeof(item->window));
    item->window.borderSize = 1.0f;
    item->window.foreColor[0] = 1.0f;
    item->window.foreColor[1] = 1.0f;
    item->window.foreColor[2] = 1.0f;
    item->window.foreColor[3] = 1.0f;
    item->window.cinematic = -1;

    if (!Item_Parse(sourceHandle, item)) {
        return qfalse;
    }
    Item_PostParse(item);
    item->parent = menu;
    ++menu->itemCount;
    return qtrue;
}

void Menu_New(int32_t sourceHandle, int32_t loadMode)
{
    menuDef_t *menu;

    if (menuCount >= MAX_MENUS) {
        return;
    }

    menu = &Menus[menuCount];
    Menu_Init(menu, loadMode);
    if (!Menu_Parse(sourceHandle, menu)) {
        return;
    }

    if (menu->fullScreen != 0) {
        menu->window.rect.x = 0.0f;
        menu->window.rect.y = 0.0f;
        menu->window.rect.w = UI_FULLSCREEN_WIDTH;
        menu->window.rect.h = UI_FULLSCREEN_HEIGHT;
    }
    Menu_PostParse(menu);
    ++menuCount;
}
