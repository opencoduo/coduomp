/*
 * Shared cgame/UI parser keyword handlers.
 *
 * Direct comparison of the original instruction streams proves that the 92
 * retained handlers are identical between uo_cgame_mp_x86.dll
 * 0x30058ef0..0x3005ab9b and uo_ui_mp_x86.dll
 * 0x4001aa60..0x4001c7eb after rebasing image-local references.  The two
 * original MenuParse_visible bodies are intentionally excluded: cgame sets
 * WINDOW_VISIBLE when the parsed value is nonzero, whereas UI only consumes
 * the integer.  MenuParse_itemDef also remains module-local because the two
 * DLLs use different allocation and Item_Parse calling boundaries.
 */

#include "ui_parse.h"
#include "ui_runtime.h"

#include <string.h>

void Com_Printf(const char *format, ...);

#define PARSE_ITEM_STRING(fn, field) \
    qboolean fn(itemDef_t *item, int handle) { return PC_String_Parse(handle, &item->field) != 0; }
#define PARSE_ITEM_SCRIPT(fn, field) \
    qboolean fn(itemDef_t *item, int handle) { return PC_Script_Parse(handle, &item->field) != 0; }
#define PARSE_ITEM_INT(fn, field) \
    qboolean fn(itemDef_t *item, int handle) { return PC_Int_Parse(handle, &item->field) != 0; }
#define PARSE_ITEM_FLOAT(fn, field) \
    qboolean fn(itemDef_t *item, int handle) { return PC_Float_Parse(handle, &item->field) != 0; }
#define PARSE_MENU_STRING(fn, field) \
    qboolean fn(menuDef_t *menu, int handle) { return PC_String_Parse(handle, &menu->field) != 0; }
#define PARSE_MENU_SCRIPT(fn, field) \
    qboolean fn(menuDef_t *menu, int handle) { return PC_Script_Parse(handle, &menu->field) != 0; }
#define PARSE_MENU_INT(fn, field) \
    qboolean fn(menuDef_t *menu, int handle) { return PC_Int_Parse(handle, &menu->field) != 0; }
#define PARSE_MENU_FLOAT(fn, field) \
    qboolean fn(menuDef_t *menu, int handle) { return PC_Float_Parse(handle, &menu->field) != 0; }

// Source RVA: 0x30058ef0
PARSE_ITEM_STRING(ItemParse_name, window.name)
// Source RVA: 0x30058f50
PARSE_ITEM_STRING(ItemParse_text, text)
// Source RVA: 0x30058ff0
PARSE_ITEM_STRING(ItemParse_group, window.group)
// Source RVA: 0x30059410
PARSE_ITEM_INT(ItemParse_style, window.style)
// Source RVA: 0x30059600
PARSE_ITEM_FLOAT(ItemParse_feeder, special)
// Source RVA: 0x30059740
PARSE_ITEM_INT(ItemParse_border, window.border)
// Source RVA: 0x30059760
PARSE_ITEM_FLOAT(ItemParse_bordersize, window.borderSize)
// Source RVA: 0x300597e0
PARSE_ITEM_INT(ItemParse_align, alignment)
// Source RVA: 0x30059800
PARSE_ITEM_INT(ItemParse_textalign, textalignment)
// Source RVA: 0x30059820
PARSE_ITEM_FLOAT(ItemParse_textalignx, textalignx)
// Source RVA: 0x30059840
PARSE_ITEM_FLOAT(ItemParse_textaligny, textaligny)
// Source RVA: 0x30059860
PARSE_ITEM_FLOAT(ItemParse_textscale, textscale)
// Source RVA: 0x30059880
PARSE_ITEM_INT(ItemParse_textstyle, textStyle)
// Source RVA: 0x300598a0
PARSE_ITEM_INT(ItemParse_textfont, font)
// Source RVA: 0x30059a20
PARSE_ITEM_STRING(ItemParse_cinematic, window.cinematicName)
// Source RVA: 0x30059aa0
PARSE_ITEM_SCRIPT(ItemParse_onFocus, onFocus)
// Source RVA: 0x30059ac0
PARSE_ITEM_SCRIPT(ItemParse_leaveFocus, leaveFocus)
// Source RVA: 0x30059ae0
PARSE_ITEM_SCRIPT(ItemParse_mouseEnter, mouseEnter)
// Source RVA: 0x30059b00
PARSE_ITEM_SCRIPT(ItemParse_mouseExit, mouseExit)
// Source RVA: 0x30059b20
PARSE_ITEM_SCRIPT(ItemParse_mouseEnterText, mouseEnterText)
// Source RVA: 0x30059b40
PARSE_ITEM_SCRIPT(ItemParse_mouseExitText, mouseExitText)
// Source RVA: 0x30059b60
PARSE_ITEM_SCRIPT(ItemParse_action, action)
// Source RVA: 0x30059b80
PARSE_ITEM_SCRIPT(ItemParse_accept, accept)
// Source RVA: 0x30059ba0
PARSE_ITEM_FLOAT(ItemParse_special, special)
// Source RVA: 0x30059bc0
PARSE_ITEM_STRING(ItemParse_cvarTest, cvarTest)

// Source RVA: 0x30058f10
qboolean ItemParse_focusSound(itemDef_t *item, int handle)
{
    const char *name;
    const char *soundName;
    displayContextDef_t *display;

    if (!PC_String_Parse(handle, &name)) return qfalse;
    soundName = name;
    display = DC;
    item->focusSound = display->registerAsset(soundName);
    return qtrue;
}

// Source RVA: 0x30059010
qboolean ItemParse_asset_model(itemDef_t *item, int handle)
{
    const char *name;

    Item_ValidateTypeData(item, handle);
    if (!PC_String_Parse(handle, &name)) return qfalse;
    if (item->asset == 0) {
        int32_t loadMode = item->loadMode;
        const char *modelName = name;
        displayContextDef_t *display = DC;

        item->asset = display->registerModel(modelName, loadMode);
    }
    return qtrue;
}

// Source RVA: 0x30059070
qboolean ItemParse_asset_shader(itemDef_t *item, int handle)
{
    const char *name;

    if (!PC_String_Parse(handle, &name)) return qfalse;
    {
        int32_t loadMode = item->loadMode;
        const char *shaderName = name;
        displayContextDef_t *display = DC;

        item->asset = display->registerShaderNoMip(shaderName, loadMode);
    }
    return qtrue;
}

// Source RVA: 0x30059370
qboolean ItemParse_rect(itemDef_t *item, int handle)
{
    return PC_Float_Parse(handle, &item->window.rectClient.x) &&
           PC_Float_Parse(handle, &item->window.rectClient.y) &&
           PC_Float_Parse(handle, &item->window.rectClient.w) &&
           PC_Float_Parse(handle, &item->window.rectClient.h);
}

// Source RVA: 0x30059780
qboolean ItemParse_visible(itemDef_t *item, int handle)
{
    int visible;
    if (!PC_Int_Parse(handle, &visible)) return qfalse;
    if (visible) item->window.flags |= WINDOW_VISIBLE;
    return qtrue;
}

// Source RVA: 0x300597b0
qboolean ItemParse_ownerdraw(itemDef_t *item, int handle)
{
    if (!PC_Int_Parse(handle, &item->window.ownerDraw)) return qfalse;
    item->type = ITEM_TYPE_OWNERDRAW;
    return qtrue;
}

#define PARSE_MODEL_FIELD(fn, parse, field) \
    qboolean fn(itemDef_t *item, int handle) \
    { \
        Item_ValidateTypeData(item, handle); \
        if (item->typeValidated != ITEM_TYPE_MODEL && \
            item->typeValidated != ITEM_TYPE_MENUMODEL) { \
            Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_MODEL, or ITEM_TYPE_MENUMODEL\n"); \
            return qfalse; \
        } \
        modelDef_t *model = (modelDef_t *)item->typeData; \
        if (model == NULL) return qfalse; \
        return parse(handle, &model->field) != 0; \
    }

// Source RVA: 0x300590b0
qboolean ItemParse_model_origin(itemDef_t *item, int handle)
{
    Item_ValidateTypeData(item, handle);
    if (item->typeValidated != ITEM_TYPE_MODEL &&
        item->typeValidated != ITEM_TYPE_MENUMODEL) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_MODEL, or ITEM_TYPE_MENUMODEL\n");
        return qfalse;
    }
    modelDef_t *model = (modelDef_t *)item->typeData;
    if (model == NULL) return qfalse;
    return PC_Float_Parse(handle, &model->origin[0]) &&
           PC_Float_Parse(handle, &model->origin[1]) &&
           PC_Float_Parse(handle, &model->origin[2]);
}

// Source RVA: 0x30059130
PARSE_MODEL_FIELD(ItemParse_model_fovx, PC_Float_Parse, fovX)
// Source RVA: 0x30059190
PARSE_MODEL_FIELD(ItemParse_model_fovy, PC_Float_Parse, fovY)
// Source RVA: 0x300591f0
PARSE_MODEL_FIELD(ItemParse_model_rotation, PC_Int_Parse, rotationSpeed)
// Source RVA: 0x30059250
PARSE_MODEL_FIELD(ItemParse_model_angle, PC_Int_Parse, angle)

#undef PARSE_MODEL_FIELD

// Source RVA: 0x30059440
qboolean ItemParse_notselectable(itemDef_t *item, int handle)
{
    Item_ValidateTypeData(item, handle);
    if (item->typeValidated != ITEM_TYPE_LISTBOX) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");
        return qfalse;
    }
    listBoxDef_t *list = (listBoxDef_t *)item->typeData;
    if (list == NULL) return qfalse;
    if (item->type == ITEM_TYPE_LISTBOX) list->notselectable = qtrue;
    return qtrue;
}

// Source RVA: 0x30059500
qboolean ItemParse_type(itemDef_t *item, int handle)
{
    if (!PC_Int_Parse(handle, &item->type)) return qfalse;
    Item_ValidateTypeData(item, handle);
    return qtrue;
}

#define PARSE_LISTBOX_FIELD(fn, parse, field) \
    qboolean fn(itemDef_t *item, int handle) \
    { \
        Item_ValidateTypeData(item, handle); \
        if (item->typeValidated != ITEM_TYPE_LISTBOX) { \
            Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n"); \
            return qfalse; \
        } \
        listBoxDef_t *list = (listBoxDef_t *)item->typeData; \
        if (list == NULL) return qfalse; \
        return parse(handle, &list->field) != 0; \
    }

// Source RVA: 0x30059540
PARSE_LISTBOX_FIELD(ItemParse_elementwidth, PC_Float_Parse, elementWidth)
// Source RVA: 0x300595a0
qboolean ItemParse_elementheight(itemDef_t *item, int handle)
{
    Item_ValidateTypeData(item, handle);
    if (item->typeValidated != ITEM_TYPE_LISTBOX) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");
        return qfalse;
    }
    listBoxDef_t *list = (listBoxDef_t *)item->typeData;
    if (list == NULL) return qfalse;
    return PC_Float_Parse(handle, &list->elementHeight) != 0;
}
#undef PARSE_LISTBOX_FIELD

// Source RVA: 0x30059620
// Unlike its elementwidth/elementheight siblings (which use the
// PARSE_LISTBOX_FIELD macro: typeValidated-first, print), ItemParse_elementtype checks
// typeData FIRST and returns qfalse SILENTLY when it is NULL (0x3005963b test / 0x3005963d
// je 0x30059655), only THEN testing typeValidated and printing the ITEM_TYPE_LISTBOX
// error (0x3005963f cmp / 0x30059648 push). A prior pass used the macro, so a non-listbox
// item with no typeData carrying an elementtype line wrongly printed the error.
qboolean ItemParse_elementtype(itemDef_t *item, int handle)
{
    Item_ValidateTypeData(item, handle);
    if (item->typeData == NULL) return qfalse;
    if (item->typeValidated != ITEM_TYPE_LISTBOX) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");
        return qfalse;
    }
    listBoxDef_t *list = (listBoxDef_t *)item->typeData;
    return PC_Int_Parse(handle, &list->elementStyle) != 0;
}

// Source RVA: 0x30059c60
qboolean ItemParse_maxChars(itemDef_t *item, int handle)
{
    int value;
    Item_ValidateTypeData(item, handle);
    if (item->typeData == NULL || !PC_Int_Parse(handle, &value)) return qfalse;
    editFieldDef_t *edit = (editFieldDef_t *)Item_GetEditFieldDef(item);
    if (edit == NULL) return qfalse;
    edit->maxChars = value;
    return qtrue;
}

// Source RVA: 0x30059cb0
qboolean ItemParse_maxCharsGotoNext(itemDef_t *item, int handle)
{
    Item_ValidateTypeData(item, handle);
    editFieldDef_t *edit = (editFieldDef_t *)item->typeData;
    if (edit == NULL) return qfalse;
    if (Item_GetEditFieldDef(item) == NULL) return qfalse;
    edit->maxCharsGotoNext = qtrue;
    return qtrue;
}

// Source RVA: 0x30059d20
qboolean ItemParse_maxPaintChars(itemDef_t *item, int handle)
{
    int value;
    Item_ValidateTypeData(item, handle);
    if (item->typeData == NULL || !PC_Int_Parse(handle, &value)) return qfalse;
    editFieldDef_t *edit = (editFieldDef_t *)Item_GetEditFieldDef(item);
    if (edit == NULL) return qfalse;
    edit->maxPaintChars = value;
    return qtrue;
}

// Source RVA: 0x30059a40
qboolean ItemParse_doubleClick(itemDef_t *item, int handle)
{
    Item_ValidateTypeData(item, handle);
    if (item->typeData == NULL) return qfalse;
    if (item->typeValidated != ITEM_TYPE_LISTBOX) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");
        return qfalse;
    }
    listBoxDef_t *list = (listBoxDef_t *)item->typeData;
    return PC_Script_Parse(handle, &list->doubleClick) != 0;
}

// Source RVA: 0x30059be0
qboolean ItemParse_cvar(itemDef_t *item, int handle)
{
    Item_ValidateTypeData(item, handle);
    if (!PC_String_Parse(handle, &item->cvar)) return qfalse;
    if (item->typeData != NULL) {
        /* 0x30059c15-3b: INLINE typeValidated check with NO print. Item_GetEditFieldDef
         * is reached only for an edit-field-family type (or the fall-through TEXT/0);
         * any OTHER nonzero type is skipped SILENTLY (test eax,eax / jne 0x30059c51).
         * A prior pass called Item_GetEditFieldDef unconditionally, which itself prints
         * "Expecting type: ITEM_TYPE_EDITFIELD" for a listbox/model item that carries a
         * cvar. The binary also does not null-check the result (0x30059c44 uses it
         * directly), since a valid type with typeData != NULL always returns typeData. */
        int32_t tv = item->typeValidated;
        if (tv == ITEM_TYPE_TEXT || tv == ITEM_TYPE_EDITFIELD ||
            tv == ITEM_TYPE_NUMERICFIELD || tv == ITEM_TYPE_SLIDER ||
            tv == ITEM_TYPE_YESNO || tv == ITEM_TYPE_BIND ||
            tv == ITEM_TYPE_UPREDITFIELD) {
            editFieldDef_t *edit = (editFieldDef_t *)Item_GetEditFieldDef(item);
            edit->minVal = -1.0f;
            edit->maxVal = -1.0f;
            edit->defVal = -1.0f;
        }
    }
    return qtrue;
}

// Source RVA: 0x30059d70
qboolean ItemParse_cvarFloat(itemDef_t *item, int handle)
{
    Item_ValidateTypeData(item, handle);
    if (item->typeData == NULL) return qfalse;
    editFieldDef_t *edit = (editFieldDef_t *)Item_GetEditFieldDef(item);
    if (edit == NULL) return qfalse;
    return PC_String_Parse(handle, &item->cvar) &&
           PC_Float_Parse(handle, &edit->defVal) &&
           PC_Float_Parse(handle, &edit->minVal) &&
           PC_Float_Parse(handle, &edit->maxVal);
}

// Source RVA: 0x3005a1a0
qboolean ItemParse_addColorRangeRel(itemDef_t *item, int handle) { return ParseColorRange(handle, 1, item); }
// Source RVA: 0x3005a1c0
qboolean ItemParse_addColorRange(itemDef_t *item, int handle) { return ParseColorRange(handle, 0, item); }

// Source RVA: 0x3005a1e0
qboolean ItemParse_ownerdrawFlag(itemDef_t *item, int handle)
{
    int flag;
    if (!PC_Int_Parse(handle, &flag)) return qfalse;
    item->window.ownerDrawFlags |= flag;
    return qtrue;
}

// Source RVA: 0x3005a210
qboolean ItemParse_enableCvar(itemDef_t *item, int handle)
{
    if (!PC_Script_Parse(handle, &item->enableCvar)) return qfalse;
    item->cvarFlags = ITEM_CVAR_ENABLE;
    return qtrue;
}

// Source RVA: 0x3005a240
qboolean ItemParse_disableCvar(itemDef_t *item, int handle)
{
    if (!PC_Script_Parse(handle, &item->enableCvar)) return qfalse;
    item->cvarFlags = ITEM_CVAR_DISABLE;
    return qtrue;
}

// Source RVA: 0x3005a280
qboolean ItemParse_showCvar(itemDef_t *item, int handle)
{
    if (!PC_Script_Parse(handle, &item->enableCvar)) return qfalse;
    item->cvarFlags = ITEM_CVAR_SHOW;
    return qtrue;
}

// Source RVA: 0x3005a2c0
qboolean ItemParse_hideCvar(itemDef_t *item, int handle)
{
    if (!PC_Script_Parse(handle, &item->enableCvar)) return qfalse;
    item->cvarFlags = ITEM_CVAR_HIDE;
    return qtrue;
}

// Source RVA: 0x300598c0
qboolean ItemParse_backcolor(itemDef_t *item, int handle)
{
    float *color = item->window.backColor;

    for (int component = 0; component < 4; ++component) {
        float value;
        if (!PC_Float_Parse(handle, &value)) return qfalse;
        /* 0x300598e2..e6 copies the parsed scalar with a raw dword MOV. */
        memcpy(&color[component], &value, sizeof(value));
    }
    return qtrue;
}
// Source RVA: 0x30059970
qboolean ItemParse_bordercolor(itemDef_t *item, int handle)
{
    float *color = item->window.borderColor;

    for (int component = 0; component < 4; ++component) {
        float value;
        if (!PC_Float_Parse(handle, &value)) return qfalse;
        /* 0x30059992..96 copies the parsed scalar with a raw dword MOV. */
        memcpy(&color[component], &value, sizeof(value));
    }
    return qtrue;
}
// Source RVA: 0x300599c0
qboolean ItemParse_outlinecolor(itemDef_t *item, int handle) { return PC_Color_Parse(handle, item->window.outlineColor) != 0; }

// Source RVA: 0x30059910
qboolean ItemParse_forecolor(itemDef_t *item, int handle)
{
    float *color = item->window.foreColor;

    for (int i = 0; i < 4; ++i) {
        float value;
        if (!PC_Float_Parse(handle, &value)) return qfalse;
        /* 0x3005992f..33 copies the local with a raw dword MOV before the flag. */
        memcpy(&color[i], &value, sizeof(value));
        item->window.flags |= WINDOW_FORECOLORSET;
    }
    return qtrue;
}

// Source RVA: 0x300599e0
qboolean ItemParse_background(itemDef_t *item, int handle)
{
    const char *name;
    if (!PC_String_Parse(handle, &name)) return qfalse;
    const int32_t loadMode = item->loadMode;                 /* 0x300599fa */
    displayContextDef_t *context = DC;                     /* 0x30059a04 */
    item->window.background = context->registerShaderNoMip(name, loadMode);
    return qtrue;
}

// Source RVA: 0x30059430
qboolean ItemParse_decoration(itemDef_t *item, int handle) { (void)handle; item->window.flags |= WINDOW_DECORATION; return qtrue; }
// Source RVA: 0x300594a0
qboolean ItemParse_wrapped(itemDef_t *item, int handle) { (void)handle; item->window.flags |= WINDOW_WRAPPED; return qtrue; }
// Source RVA: 0x300594c0
qboolean ItemParse_autowrapped(itemDef_t *item, int handle) { (void)handle; item->window.flags |= WINDOW_AUTOWRAPPED; return qtrue; }
// Source RVA: 0x300594e0
qboolean ItemParse_horizontalscroll(itemDef_t *item, int handle) { (void)handle; item->window.flags |= WINDOW_HORIZONTAL; return qtrue; }

// Source RVA: 0x3005a550
PARSE_MENU_STRING(MenuParse_name, window.name)
// Source RVA: 0x3005a570
PARSE_MENU_INT(MenuParse_fullscreen, fullScreen)
// Source RVA: 0x3005a5e0
PARSE_MENU_INT(MenuParse_style, window.style)
// Source RVA: 0x3005a630
PARSE_MENU_SCRIPT(MenuParse_onOpen, onOpen)
// Source RVA: 0x3005a650
PARSE_MENU_SCRIPT(MenuParse_onClose, onClose)
// Source RVA: 0x3005a670
PARSE_MENU_SCRIPT(MenuParse_onESC, onESC)
// Source RVA: 0x3005a690
PARSE_MENU_SCRIPT(MenuParse_onAnyKey, onKey[255])
// Source RVA: 0x3005a6b0
PARSE_MENU_INT(MenuParse_border, window.border)
// Source RVA: 0x3005a6d0
PARSE_MENU_FLOAT(MenuParse_borderSize, window.borderSize)
// Source RVA: 0x3005a8f0
PARSE_MENU_STRING(MenuParse_cinematic, window.cinematicName)
// Source RVA: 0x3005a940
PARSE_MENU_INT(MenuParse_ownerdraw, window.ownerDraw)
// Source RVA: 0x3005a9a0
PARSE_MENU_STRING(MenuParse_soundLoop, soundName)
// Source RVA: 0x3005a9c0
PARSE_MENU_FLOAT(MenuParse_fadeClamp, fadeClamp)
// Source RVA: 0x3005a9e0
PARSE_MENU_FLOAT(MenuParse_fadeAmount, fadeAmount)
// Source RVA: 0x3005aa00
PARSE_MENU_FLOAT(MenuParse_fadeInAmount, fadeInAmount)
// Source RVA: 0x3005aa20
PARSE_MENU_INT(MenuParse_fadeCycle, fadeCycle)

// Source RVA: 0x3005a6f0
qboolean MenuParse_backcolor(menuDef_t *menu, int handle)
{
    float *color = menu->window.backColor;

    for (int component = 0; component < 4; ++component) {
        float value;
        if (!PC_Float_Parse(handle, &value)) return qfalse;
        /* 0x3005a711..0x3005a715: copy the parsed scalar as one raw dword. */
        memcpy(&color[component], &value, sizeof(value));
    }
    return qtrue;
}
// Source RVA: 0x3005a7a0
qboolean MenuParse_bordercolor(menuDef_t *menu, int handle)
{
    float *color = menu->window.borderColor;

    for (int component = 0; component < 4; ++component) {
        float value;
        if (!PC_Float_Parse(handle, &value)) return qfalse;
        /* 0x3005a7c1..0x3005a7c5: copy the parsed scalar as one raw dword. */
        memcpy(&color[component], &value, sizeof(value));
    }
    return qtrue;
}
// Source RVA: 0x3005a7f0
qboolean MenuParse_focuscolor(menuDef_t *menu, int handle)
{
    float *color = menu->focusColor;

    for (int component = 0; component < 4; ++component) {
        float value;
        if (!PC_Float_Parse(handle, &value)) return qfalse;
        /* 0x3005a811..0x3005a815: copy the parsed scalar as one raw dword. */
        memcpy(&color[component], &value, sizeof(value));
    }
    return qtrue;
}
// Source RVA: 0x3005a840
qboolean MenuParse_disablecolor(menuDef_t *menu, int handle)
{
    float *color = menu->disableColor;

    for (int component = 0; component < 4; ++component) {
        float value;
        if (!PC_Float_Parse(handle, &value)) return qfalse;
        /* 0x3005a861..0x3005a865: copy the parsed scalar as one raw dword. */
        memcpy(&color[component], &value, sizeof(value));
    }
    return qtrue;
}
// Source RVA: 0x3005a890
qboolean MenuParse_outlinecolor(menuDef_t *menu, int handle) { return PC_Color_Parse(handle, menu->window.outlineColor) != 0; }

// Source RVA: 0x3005a740
qboolean MenuParse_forecolor(menuDef_t *menu, int handle)
{
    float *color = menu->window.foreColor;

    for (int i = 0; i < 4; ++i) {
        float value;
        if (!PC_Float_Parse(handle, &value)) return qfalse;
        /* 0x3005a75f..0x3005a763: the flag store follows this raw dword copy. */
        memcpy(&color[i], &value, sizeof(value));
        menu->window.flags |= WINDOW_FORECOLORSET;
    }
    return qtrue;
}

// Source RVA: 0x3005a8b0
qboolean MenuParse_background(menuDef_t *menu, int handle)
{
    const char *name;
    if (!PC_String_Parse(handle, &name)) return qfalse;
    const int32_t loadMode = menu->loadMode;                 /* 0x3005a8ca */
    displayContextDef_t *context = DC;                     /* 0x3005a8d4 */
    menu->window.background = context->registerShaderNoMip(name, loadMode);
    return qtrue;
}

// Source RVA: 0x3005a590
qboolean MenuParse_rect(menuDef_t *menu, int handle)
{
    return PC_Float_Parse(handle, &menu->window.rect.x) &&
           PC_Float_Parse(handle, &menu->window.rect.y) &&
           PC_Float_Parse(handle, &menu->window.rect.w) &&
           PC_Float_Parse(handle, &menu->window.rect.h);
}

// Source RVA: 0x3005a910
qboolean MenuParse_ownerdrawFlag(menuDef_t *menu, int handle)
{
    int flag;
    if (!PC_Int_Parse(handle, &flag)) return qfalse;
    menu->window.ownerDrawFlags |= flag;
    return qtrue;
}

// Source RVA: 0x3005ab20
qboolean MenuParse_execKey(menuDef_t *menu, int handle)
{
    char key;
    if (!PC_Char_Parse(handle, &key)) return qfalse;
    return PC_Script_Parse(handle, &menu->onKey[(unsigned char)key]) != 0;
}

// Source RVA: 0x3005ab60
qboolean MenuParse_execKeyInt(menuDef_t *menu, int handle)
{
    int key;
    if (!PC_Int_Parse(handle, &key)) return qfalse;

    /* NOT_FROM_ORIGINAL_SOURCE: a parsed key must name an onKey element
     * before its destination pointer is formed. */
    if (key < 0 || key >= MAX_KEYS) {
        PC_SourceError(handle, "execKeyInt key %d is outside 0..%d\n",
                       key, MAX_KEYS - 1);
        return qfalse;
    }

    return PC_Script_Parse(handle, &menu->onKey[key]) != 0;
}

// Source RVA: 0x3005a960
qboolean MenuParse_popup(menuDef_t *menu, int handle) { (void)handle; menu->window.flags |= WINDOW_MODAL; return qtrue; }
// Source RVA: 0x3005a980
qboolean MenuParse_outOfBoundsClick(menuDef_t *menu, int handle) { (void)handle; menu->window.flags |= WINDOW_OOB_CLICK; return qtrue; }

#undef PARSE_ITEM_STRING
#undef PARSE_ITEM_SCRIPT
#undef PARSE_ITEM_INT
#undef PARSE_ITEM_FLOAT
#undef PARSE_MENU_STRING
#undef PARSE_MENU_SCRIPT
#undef PARSE_MENU_INT
#undef PARSE_MENU_FLOAT
