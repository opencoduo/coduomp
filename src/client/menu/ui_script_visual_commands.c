#include "ui_runtime.h"

#include "ui_parse.h"

#include <string.h>

enum { UI_SCRIPT_COLOR_FIELD_COMPARE_LIMIT = 99999 };

/*
 * Complete visual-property menu-command cluster.  The authoritative Windows
 * cgame/UI bodies are instruction-identical after rebasing module-local
 * strings, calls, and the display-context global:
 *
 *                                  cgame       UI
 * Script_SetColor                  0x300513f0  0x40012f10
 * Script_SetAsset                  0x300514c0  0x40012fe0
 * Script_SetBackground             0x300514e0  0x40013000
 * Script_SetTeamColor              0x30051590  0x400130b0
 * Script_SetItemColor              0x300515e0  0x40013100
 *
 * Both supporting Mac modules export all five canonical Script_* names.  In
 * particular, Script_SetAsset is not an Item_HandleSetAsset function in the
 * UI module: both Mac symbol tables name it Script_SetAsset, both Windows
 * command tables point directly at the body, and both Windows bodies merely
 * return with String_Parse's otherwise ignored value still in EAX.
 */

void Script_SetColor(itemDef_t *item, char **arguments)
{
    const char *fieldName;
    vec4_t *target = NULL;
    int32_t component;

    if (!String_Parse(arguments, &fieldName) || fieldName == NULL) {
        return;
    }
    if (Q_stricmpn("backcolor", fieldName,
                   UI_SCRIPT_COLOR_FIELD_COMPARE_LIMIT) == 0) {
        item->window.flags |= WINDOW_BACKCOLOR_SET;
        target = &item->window.backColor;
    } else if (Q_stricmpn("forecolor", fieldName,
                          UI_SCRIPT_COLOR_FIELD_COMPARE_LIMIT) == 0) {
        item->window.flags |= WINDOW_FORECOLORSET;
        target = &item->window.foreColor;
    } else if (Q_stricmpn("bordercolor", fieldName,
                          UI_SCRIPT_COLOR_FIELD_COMPARE_LIMIT) == 0) {
        target = &item->window.borderColor;
    }
    if (target == NULL) {
        return;
    }

    for (component = 0; component < 4; ++component) {
        if (!Float_Parse(arguments, &(*target)[component])) {
            return;
        }
    }
}

void Script_SetAsset(itemDef_t *item, char **arguments)
{
    const char *discardedAsset;

    (void)item;
    (void)String_Parse(arguments, &discardedAsset);
}

void Script_SetBackground(itemDef_t *item, char **arguments)
{
    const char *background;

    if (!String_Parse(arguments, &background)) {
        return;
    }
    item->window.background =
        DC->registerShaderNoMip(background, item->loadMode);
}

void Script_SetTeamColor(itemDef_t *item, char **arguments)
{
    ui_getTeamColor_t getTeamColor = DC->getTeamColor;

    (void)arguments;
    if (getTeamColor != NULL) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        vec4_t color = { 0.0f, 0.0f, 0.0f, 0.0f };

        getTeamColor(color);
        item->window.backColor[0] = color[0];
        memcpy(&item->window.backColor[1], &color[1],
               3u * sizeof(color[0]));
    }
}

void Script_SetItemColor(itemDef_t *item, char **arguments)
{
    const char *name;
    const char *fieldName;
    vec4_t color;
    int32_t count;
    int32_t index;

    if (!String_Parse(arguments, &name) ||
        !String_Parse(arguments, &fieldName)) {
        return;
    }
    count = Menu_ItemsMatchingGroup(item->parent, name);
    if (!Color_Parse(arguments, color)) {
        return;
    }

    for (index = 0; index < count; ++index) {
        itemDef_t *matched = Menu_GetMatchingItemByNumber(
            item->parent, name, index);
        vec4_t *target = NULL;

        if (matched == NULL || fieldName == NULL) {
            continue;
        }
        if (Q_stricmpn("backcolor", fieldName,
                       UI_SCRIPT_COLOR_FIELD_COMPARE_LIMIT) == 0) {
            target = &matched->window.backColor;
        } else if (Q_stricmpn("forecolor", fieldName,
                              UI_SCRIPT_COLOR_FIELD_COMPARE_LIMIT) == 0) {
            matched->window.flags |= WINDOW_FORECOLORSET;
            target = &matched->window.foreColor;
        } else if (Q_stricmpn("bordercolor", fieldName,
                              UI_SCRIPT_COLOR_FIELD_COMPARE_LIMIT) == 0) {
            target = &matched->window.borderColor;
        }
        if (target != NULL) {
            (*target)[0] = color[0];
            (*target)[1] = color[1];
            (*target)[2] = color[2];
            (*target)[3] = color[3];
        }
    }
}
