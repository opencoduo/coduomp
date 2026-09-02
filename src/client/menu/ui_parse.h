#ifndef CLIENT_UI_PARSE_H
#define CLIENT_UI_PARSE_H

#include "qcommon/q_shared_types.h"
#include "ui_display_context_types.h"
#include "ui_menu_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Each client module owns this pointer at its original data location. */
extern displayContextDef_t *DC;

void Init_Display(displayContextDef_t *displayContext);

qboolean Float_Parse(char **handle, float *destination);
qboolean Int_Parse(char **handle, int32_t *destination);
qboolean Color_Parse(char **handle, vec4_t destination);
qboolean Rect_Parse(char **handle, rectDef_t *destination);
qboolean String_Parse(char **handle, const char **destination);

qboolean PC_Float_Parse(int32_t sourceHandle, float *destination);
qboolean PC_Int_Parse(int32_t sourceHandle, int32_t *destination);
qboolean PC_Color_Parse(int32_t sourceHandle, vec4_t destination);
qboolean PC_Rect_Parse(int32_t sourceHandle, rectDef_t *destination);
qboolean PC_String_Parse(int32_t sourceHandle, const char **destination);
qboolean PC_Char_Parse(int32_t sourceHandle, char *destination);
qboolean PC_Script_Parse(int32_t sourceHandle, const char **destination);

void PC_SourceWarning(int32_t sourceHandle, const char *format, ...);
void PC_SourceError(int32_t sourceHandle, const char *format, ...);

extern keywordHash_t itemParseKeywords[];
extern menuKeywordHash_t menuParseKeywords[];
extern keywordHash_t *itemKeywordHashTable[KEYWORDHASH_SIZE];
extern menuKeywordHash_t *menuKeywordHashTable[KEYWORDHASH_SIZE];

void Item_SetupKeywordHash(void);
void Menu_SetupKeywordHash(void);
qboolean Item_Parse(int32_t sourceHandle, itemDef_t *item);
qboolean Menu_Parse(int32_t sourceHandle, menuDef_t *menu);
void Menu_New(int32_t sourceHandle, int32_t loadMode);

/* Canonical parser-trap boundary implemented separately by cgame and UI. */
int32_t trap_PC_LoadSource(const char *filename);
void trap_PC_FreeSource(int32_t sourceHandle);
qboolean trap_PC_ReadToken(int32_t sourceHandle, pc_token_t *token);
void trap_PC_SourceFileAndLine(int32_t sourceHandle, char *filename, int32_t *line);

/* Parser keyword handlers shared by the cgame and UI modules. */
qboolean ItemParse_name(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_focusSound(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_text(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_textfile(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_group(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_asset_model(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_asset_shader(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_model_origin(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_model_fovx(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_model_fovy(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_model_rotation(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_model_angle(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_model_animplay(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_rect(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_origin(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_style(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_decoration(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_notselectable(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_wrapped(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_autowrapped(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_horizontalscroll(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_type(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_elementwidth(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_elementheight(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_feeder(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_elementtype(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_columns(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_border(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_bordersize(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_visible(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_ownerdraw(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_align(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_textalign(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_textalignx(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_textaligny(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_textscale(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_textstyle(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_textfont(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_backcolor(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_forecolor(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_bordercolor(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_outlinecolor(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_background(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_cinematic(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_doubleClick(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_onFocus(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_leaveFocus(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_mouseEnter(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_mouseExit(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_mouseEnterText(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_mouseExitText(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_action(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_accept(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_special(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_cvarTest(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_cvar(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_maxChars(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_maxCharsGotoNext(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_maxPaintChars(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_cvarFloat(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_cvarStrList(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_cvarFloatList(itemDef_t *item, int32_t sourceHandle);
qboolean ParseColorRange(int32_t sourceHandle, int32_t rangeType, itemDef_t *item);
qboolean ItemParse_addColorRangeRel(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_addColorRange(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_ownerdrawFlag(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_enableCvar(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_disableCvar(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_showCvar(itemDef_t *item, int32_t sourceHandle);
qboolean ItemParse_hideCvar(itemDef_t *item, int32_t sourceHandle);

qboolean MenuParse_font(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_name(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_fullscreen(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_rect(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_style(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_onOpen(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_onClose(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_onESC(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_onAnyKey(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_border(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_borderSize(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_backcolor(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_forecolor(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_bordercolor(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_focuscolor(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_disablecolor(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_outlinecolor(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_background(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_cinematic(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_ownerdrawFlag(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_ownerdraw(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_popup(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_outOfBoundsClick(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_soundLoop(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_fadeClamp(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_fadeAmount(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_fadeInAmount(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_fadeCycle(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_execKey(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_execKeyInt(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_visible(menuDef_t *menu, int32_t sourceHandle);
qboolean MenuParse_itemDef(menuDef_t *menu, int32_t sourceHandle);

void Item_PostParse(itemDef_t *item);
void Menu_PostParse(menuDef_t *menu);

#ifdef __cplusplus
}
#endif

#endif
