#ifndef CLIENT_UI_RUNTIME_H
#define CLIENT_UI_RUNTIME_H

#include "ui_display_context_types.h"
#include "ui_menu_types.h"
#include "qcommon/q_string.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Fade(int32_t *flags, float *alpha, float clamp, int32_t *nextTime,
          int32_t offsetTime, qboolean clearFlags, float fadeAmount,
          float fadeInAmount);
void Window_Init(windowDef_t *window);
void Menu_Init(menuDef_t *menu, int32_t loadMode);
void Item_Init(itemDef_t *item, int32_t loadMode);
void Item_ValidateTypeData(itemDef_t *item, int32_t sourceHandle);

extern bind_t g_bindings[CONTROL_BINDING_COUNT];
void Controls_GetKeyAssignment(const char *command, int32_t keys[2]);
void Controls_GetConfig(void);
void Controls_SetConfig(void);
void Controls_SetDefaults(void);
int32_t BindingIDFromName(const char *command);
const char *BindingFromName(const char *command, qboolean firstKeyOnly);
qboolean GetCommandHasBinding(const char *command);
int32_t Key_GetKeysForCommand(char **firstKeyName, char **secondKeyName,
                              const char *command);
int32_t UI_KeysStringForBinding(const char *command, char **bindingText);

void LerpColor(vec4_t output, const vec4_t from, const vec4_t to,
               float fraction);

int32_t KeywordHash_Key(const char *keyword);
void KeywordHash_Add(keywordHash_t **hashTable, keywordHash_t *keyword);
keywordHash_t *KeywordHash_Find(keywordHash_t *const *hashTable,
                                const char *keyword);

void Window_CacheContents(windowDef_t *window);
void Item_CacheContents(itemDef_t *item);
void Menu_CacheContents(menuDef_t *menu);
void Display_CacheAll(void);
qboolean Menu_OverActiveItem(menuDef_t *menu, float x, float y);

int32_t Item_Multi_CountSettings(itemDef_t *item);
int32_t Item_Multi_FindCvarByValue(itemDef_t *item);
const char *Item_Multi_Setting(itemDef_t *item);
qboolean Item_Multi_HandleKey(itemDef_t *item, int32_t key);
void Item_Multi_Paint(itemDef_t *item);

qboolean Item_YesNo_HandleKey(itemDef_t *item, int32_t key);
void Item_YesNo_Paint(itemDef_t *item);

long double Item_Slider_ThumbPosition(itemDef_t *item);
int32_t Item_Slider_OverSlider(itemDef_t *item, float x, float y);
qboolean Item_Slider_HandleKey(itemDef_t *item, int32_t key);
void Item_Slider_Paint(itemDef_t *item);

void Item_TextColor(itemDef_t *item, vec4_t color);
void Item_SetTextExtents(itemDef_t *item, int32_t *width, int32_t *height,
                         const char *text);
void Item_Text_AutoWrapped_Paint(itemDef_t *item, const char *text,
                                 const vec4_t color);
void Item_Text_Wrapped_Paint(itemDef_t *item, const char *text,
                             const vec4_t color);
void Item_Text_Paint(itemDef_t *item);
qboolean Item_TextField_HandleKey(itemDef_t *item, int32_t key);
void Item_TextField_Paint(itemDef_t *item);

qboolean Item_EnableShowViaCvar(itemDef_t *item, int32_t flag);
qboolean Item_OwnerDraw_HandleKey(itemDef_t *item, int32_t key);
void Item_OwnerDraw_Paint(itemDef_t *item);
void Item_Action(itemDef_t *item);
void Item_Asset_Paint(itemDef_t *item);
void Item_Model_Paint(itemDef_t *item);

int32_t Item_ListBox_MaxScroll(itemDef_t *item);
int32_t Item_ListBox_ThumbPosition(itemDef_t *item);
int32_t Item_ListBox_ThumbDrawPosition(itemDef_t *item);
int32_t Item_ListBox_OverLB(itemDef_t *item, float x, float y);
void Item_ListBox_MouseEnter(itemDef_t *item, float x, float y);
qboolean Item_ListBox_HandleKey(itemDef_t *item, int32_t key,
                                qboolean force);
void Item_ListBox_Paint(itemDef_t *item);

/* NOT_FROM_ORIGINAL_SOURCE: target boundary for the UI DLL's original
 * server-browser selection synchronization. Cgame has no corresponding work. */
void client_ui_compat_sync_server_list_selection(itemDef_t *item);

qboolean Item_Bind_HandleKey(itemDef_t *item, int32_t key,
                             qboolean down);
void Item_Bind_Paint(itemDef_t *item);
void Item_Paint(itemDef_t *item);
void Menu_PaintAll(void);

/* NOT_FROM_ORIGINAL_SOURCE: target boundaries for UI's optional console-key
 * binding row. Cgame implements the stock/no-extra-row side of each boundary. */
bind_t *client_ui_compat_extra_binding_for_name(const char *command);
void client_ui_compat_remove_key_from_extra_bindings(int32_t key);
void client_ui_compat_controls_set_config(void);
const char *client_ui_compat_binding_from_name(const char *command,
                                               qboolean firstKeyOnly);
void client_ui_compat_bind_capture_started(itemDef_t *item);
void client_ui_compat_bind_capture_finished(void);
qboolean client_ui_compat_bind_key_is_ignored(itemDef_t *item,
                                               int32_t key);

/* NOT_FROM_ORIGINAL_SOURCE: target boundary for cgame's optional passive-HUD
 * translation around the common original item painter. UI implements no work. */
float client_ui_compat_begin_item_paint(itemDef_t *item);
void client_ui_compat_end_item_paint(itemDef_t *item, float offset);

/* NOT_FROM_ORIGINAL_SOURCE: target boundary for cgame's optional widescreen
 * projection around the shared original per-menu paint body. */
qboolean client_ui_compat_should_skip_menu_paint(
    menuDef_t *menu, qboolean passiveHudPass);
void client_ui_compat_begin_menu_paint(
    menuDef_t *menu, qboolean passiveHudPass, float *passiveHudOffset,
    float *openMenuPreviousXScale);
void client_ui_compat_finish_menu_window_paint(
    menuDef_t *menu, qboolean passiveHudPass, float passiveHudOffset);
void client_ui_compat_finish_menu_items(
    menuDef_t *menu, qboolean passiveHudPass);
void client_ui_compat_end_menu_paint(
    menuDef_t *menu, qboolean passiveHudPass, float openMenuPreviousXScale);

listBoxDef_t *Item_GetListBoxDef(itemDef_t *item);
qboolean Item_IsEditFieldDef(itemDef_t *item);
editFieldDef_t *Item_GetEditFieldDef(itemDef_t *item);
multiDef_t *Item_GetMultiDef(itemDef_t *item);
modelDef_t *Item_GetModelDef(itemDef_t *item);

qboolean Rect_ContainsPoint(const rectDef_t *rect, float x, float y);
qboolean IsVisible(int32_t flags);

void Item_SetScreenCoords(itemDef_t *item, float x, float y);
void Item_UpdatePosition(itemDef_t *item);
void Menu_UpdatePosition(menuDef_t *menu);
rectDef_t *Item_CorrectedTextRect(itemDef_t *item);
void ToWindowCoords(float *x, float *y, const windowDef_t *window);
void Rect_ToWindowCoords(rectDef_t *rect, const windowDef_t *window);

itemDef_t *Menu_FindItemByName(menuDef_t *menu, const char *name);
int32_t Menu_ItemsMatchingGroup(menuDef_t *menu, const char *name);
itemDef_t *Menu_GetMatchingItemByNumber(menuDef_t *menu, const char *name,
                                        int32_t index);
itemDef_t *Menu_GetFocusedItem(menuDef_t *menu);
qboolean Menus_MenuIsInStack(menuDef_t *menu);
menuDef_t *Menus_FindByName(const char *name);
menuDef_t *Menu_GetFocused(void);
qboolean Menus_AnyFullScreenVisible(void);
menuDef_t *Menu_GetAtPoint(int32_t x, int32_t y);

void Menu_RunCloseScript(menuDef_t *menu);
qboolean Menus_RemoveFromStack(menuDef_t *menu);
void Menus_AddToStack(menuDef_t *menu);
void Menus_Close(menuDef_t *menu);
void Menus_CloseByName(const char *name);
void Menus_CloseAll(void);
void Window_CloseCinematic(windowDef_t *window);
void Menu_CloseCinematics(menuDef_t *menu);
void Display_CloseCinematics(void);
void Menus_Open(menuDef_t *menu);
qboolean Menus_OpenByName(const char *name);
int32_t Menus_VisibleCount(void);

void Menu_ShowItemByName(menuDef_t *menu, const char *name, qboolean show);
void Menu_FadeItemByName(menuDef_t *menu, const char *name,
                         qboolean fadeOut);
void Script_Show(itemDef_t *item, char **arguments);
void Script_Hide(itemDef_t *item, char **arguments);
void Script_FadeOut(itemDef_t *item, char **arguments);
void Script_Open(itemDef_t *item, char **arguments);
void Script_OpenForGameType(itemDef_t *item, char **arguments);
void Script_CloseForGameType(itemDef_t *item, char **arguments);
void Script_ConditionalOpen(itemDef_t *item, char **arguments);
void Script_Close(itemDef_t *item, char **arguments);
void Script_InGameOpen(itemDef_t *item, char **arguments);
void Script_InGameClose(itemDef_t *item, char **arguments);
void Script_SetPlayerModel(itemDef_t *item, char **arguments);
void Script_SetPlayerHead(itemDef_t *item, char **arguments);
void Script_SetCvar(itemDef_t *item, char **arguments);
void Script_Exec(itemDef_t *item, char **arguments);
void Script_ExecOnCvarStringValue(itemDef_t *item, char **arguments);
void Script_ExecOnCvarIntValue(itemDef_t *item, char **arguments);
void Script_ExecOnCvarFloatValue(itemDef_t *item, char **arguments);
void Script_Play(itemDef_t *item, char **arguments);
void Script_AddListItem(itemDef_t *item, char **arguments);
void Script_GetAutoUpdate(itemDef_t *item, char **arguments);
void Script_ScriptMenuResponse(itemDef_t *item, char **arguments);
void Script_SetColor(itemDef_t *item, char **arguments);
void Script_SetAsset(itemDef_t *item, char **arguments);
void Script_SetBackground(itemDef_t *item, char **arguments);
void Script_SetTeamColor(itemDef_t *item, char **arguments);
void Script_SetItemColor(itemDef_t *item, char **arguments);
/* Script_FadeIn remains module-owned pending adjudication of its unlike
 * Windows bodies; the common command table still requires its interface. */
void Script_FadeIn(itemDef_t *item, char **arguments);
void Item_RunScript(itemDef_t *item, const char *script);

itemDef_t *Menu_ClearFocus(menuDef_t *menu);
void Script_SetFocus(itemDef_t *item, char **arguments);
qboolean Item_SetFocus(itemDef_t *item, float x, float y);
itemDef_t *Menu_GetItemUnderCursor(menuDef_t *menu, float x, float y);
void Item_SetMouseOver(itemDef_t *item, qboolean mouseOver);
void Item_MouseEnter(itemDef_t *item, float x, float y);
void Item_MouseLeave(itemDef_t *item);
qboolean Menu_HandleMouseMove(menuDef_t *menu, float x, float y);
itemDef_t *Menu_SetPrevCursorItem(menuDef_t *menu);
itemDef_t *Menu_SetNextCursorItem(menuDef_t *menu);

void Menu_TransitionItemByName(menuDef_t *menu, const char *name,
                               rectDef_t rectFrom, rectDef_t rectTo,
                               int32_t time, float amount);
void Script_Transition(itemDef_t *item, char **arguments);
void Menu_OrbitItemByName(menuDef_t *menu, const char *name,
                          float startX, float startY, float centerX,
                          float centerY, int32_t time);
void Script_Orbit(itemDef_t *item, char **arguments);

void Scroll_ListBox_AutoFunc(void *captureData);
void Scroll_ListBox_ThumbFunc(void *captureData);
void Scroll_Slider_ThumbFunc(void *captureData);
void Item_StartCapture(itemDef_t *item, int32_t key);
void Item_StopCapture(void);
qboolean Item_HandleKey(itemDef_t *item, int32_t key, qboolean down);
void Menus_HandleOOBClick(menuDef_t *menu, int32_t key, qboolean down);
void Menu_HandleKey(menuDef_t *menu, int32_t key, qboolean down);
qboolean Display_MouseMove(menuDef_t *menu, int32_t cursorX,
                           int32_t cursorY);
uiCursorType_t Display_CursorType(int32_t cursorX, int32_t cursorY);
void Display_HandleKey(int32_t x, int32_t y, int32_t key, qboolean down);
void Menu_ScrollFeeder(menuDef_t *menu, int32_t feeder, qboolean down);
void Menu_SetFeederSelection(menuDef_t *menu, const char *menuName,
                             int32_t feeder, int32_t index);

void GradientBar_Paint(const rectDef_t *rect, const vec4_t color);
void Window_Paint(windowDef_t *window, float fadeAmount, float fadeInAmount,
                  float fadeClamp, float fadeCycle);
void Menu_Paint(menuDef_t *menu, qboolean forcePaint);

void Copy40Bytes(void *destination, const void *source);
int32_t HexDigitValue(char character);
char LocaleDecimalSeparator(language_t locale);
qboolean Display_KeyBindPending(void);
void AdjustFrom640(float *x, float *y, float *width, float *height);

#ifdef __cplusplus
}
#endif

#endif
