#ifndef CODUO_UI_FUNCTIONS_H
#define CODUO_UI_FUNCTIONS_H

#ifndef EMULATE_X87
#define EMULATE_X87 0
#endif

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_native_x87.h"
#include "qcommon/com_parse.h"
#include "qcommon/com_sprintf.h"
#include "qcommon/info.h"
#include "qcommon/q_endian.h"
#include "qcommon/q_bits.h"
#include "math/q_math.h"
#include "client/common/client_common.h"
#include "client/math/client_math.h"
#include "qcommon/q_path.h"
#include "qcommon/q_shared_misc.h"
#include "qcommon/q_string.h"
#include "client/menu/ui_parse.h"
#include "client/menu/ui_memory.h"
#include "client/menu/ui_runtime.h"
#include "../abi/ui_module_abi.h"
#include "ui_globals.h"

#include <math.h>
#include <string.h>

/* NOT_FROM_ORIGINAL_SOURCE: always-inline recovered-host adapters. */
#if defined(_MSC_VER)
#define UI_RECOVERY_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define UI_RECOVERY_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define UI_RECOVERY_ALWAYS_INLINE inline
#endif

static UI_RECOVERY_ALWAYS_INLINE uint32_t UI_FloatBits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static UI_RECOVERY_ALWAYS_INLINE float UI_FloatFromBits(uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

#undef UI_RECOVERY_ALWAYS_INLINE

void UI_Init(void);
void ui_compat_extend_graphics_menu(void);
void ui_compat_extend_console_binding_menu(void);
void ui_compat_extend_advanced_menu(void);
void ui_compat_brand_main_menu_version(void);
void ui_compat_remove_single_player_menu_item(void);
void UI_RunMenuScript(char **arguments);
void UI_Shutdown(void);
void UI_KeyEvent(int32_t key, qboolean down);
void UI_MouseEvent(int32_t deltaX, int32_t deltaY);
void UI_Refresh(int32_t realtime);
qboolean UI_IsFullscreen(void);
int32_t UI_SetActiveMenu(int32_t menu);
int32_t _UI_GetActiveMenu(void);
const char *UI_GetMapDisplayName(const char *mapName);
const char *UI_GetGameTypeDisplayName(const char *gameType);
qboolean UI_ConsoleCommand(int32_t realtime);
void UI_DrawConnectScreen(qboolean overlay);
qhandle_t trap_R_RegisterModel(const char *name, int32_t loadMode);
void trap_R_ModelBounds(qhandle_t model, vec3_t minimums, vec3_t maximums);
int32_t trap_R_Text_Height(int32_t font, float scale);
void trap_R_ClearScene(void);
void trap_R_AddRefEntity(const refEntity_t *entity);
void trap_R_RenderScene(const refdef_t *refdef);
void trap_R_Text_PaintWithCursor(float x, float y, int32_t font,
                                 float scale, const vec4_t color,
                                 const char *text, int32_t cursorPosition,
                                 int8_t cursorCharacter, int32_t limit,
                                 int32_t textStyle);
void trap_Key_SetOverstrikeMode(qboolean overstrike);
qboolean trap_Key_GetOverstrikeMode(void);
void trap_Key_KeynumToStringBuf(int32_t keynum, char *buffer,
                                int32_t bufferSize);
void trap_Key_GetBindingBuf(int32_t keynum, char *buffer,
                            int32_t bufferSize);
void trap_Key_SetBinding(int32_t keynum, const char *binding);
void trap_GetAutoUpdate(void);
qboolean trap_RunningGame(void);
long double ui_compat_display_cvar_value(const char *name);
void ui_compat_lan_load_cached_servers(void);
qboolean ui_compat_lan_server_is_punkbuster(int32_t source, int32_t server);
void ui_compat_lan_remove_server(int32_t source, const char *address);
void ui_compat_set_pb_client_status(int32_t status);
qboolean ui_compat_verify_cd_key(const char *key, const char *checksum);
void ui_compat_set_cd_key(const char *key, const char *checksum);
void UI_Test_f(void);

/* vmMain commands 13..16 are identified by the executable callers and the
 * module dispatch targets: unique-CD-key query, exec-key check, script-menu
 * load, and font selection. */

void Com_Printf(const char *format, ...);
void Com_DPrintf(const char *format, ...);
void Com_Error(errorParm_t level, const char *format, ...);
void UI_AdjustFrom640(float *x, float *y, float *width, float *height);
const char *UI_SafeTranslateString(const char *reference);
const char *UI_Argv(int32_t index);
const char *UI_Cvar_VariableString(const char *name);
const char *UI_ConfigString(int32_t index);
void UI_ShowPostGame(qboolean newHighScore);
void UI_Report(void);
void String_Report(void);
void UI_GetFontInfo(void);
fontInfo_t *Text_GetFont(int32_t font, float scale);
void Text_SetActiveFont(int32_t font);
void UI_DrawHandicap(const rectDef_t *rect, int32_t font, float scale,
                     const vec4_t color, int32_t textStyle);
void UI_DrawGameType(const rectDef_t *rect, int32_t font, float scale,
                     const vec4_t color, int32_t textStyle);
void UI_DrawNetGameType(const rectDef_t *rect, int32_t font, float scale,
                        const vec4_t color, int32_t textStyle);
void UI_DrawJoinGameType(const rectDef_t *rect, int32_t font, float scale,
                         const vec4_t color, int32_t textStyle);
void UI_DrawPreviewCinematic(const rectDef_t *rect);
void UI_DrawMapPreview(const rectDef_t *rect, qboolean netMap);
void UI_ValidateMapPreviewSelection(const rectDef_t *rect, qboolean netMap);
void UI_DrawNetMapPreview(const rectDef_t *rect);
void UI_DrawNetMapCinematic(const rectDef_t *rect);
int32_t UI_OwnerDrawWidth(int32_t ownerDraw, int32_t font, float scale);
void UI_BuildPlayerList(void);
void UI_DrawServerRefreshDate(const rectDef_t *rect, int32_t font,
                              float scale, const vec4_t color,
                              int32_t textStyle);
void UI_DrawServerRefreshTotals(const rectDef_t *rect, int32_t font,
                                float scale, const vec4_t color,
                                int32_t textStyle);
void UI_DrawKeyBindStatus(const rectDef_t *rect, int32_t font, float scale,
                          const vec4_t color, int32_t textStyle);
void UI_DrawGLInfo(const rectDef_t *rect, int32_t font, float scale,
                   const vec4_t color, int32_t textStyle);
void UI_OwnerDraw(float x, float y, float width, float height,
                  float textX, float textY, int32_t ownerDraw,
                  int32_t ownerDrawFlags, int32_t alignment, float special,
                  int32_t font, float textScale, vec4_t color,
                  qhandle_t background, int32_t textStyle);
qboolean UI_OwnerDrawVisible(int32_t ownerDrawFlags);
qboolean UI_Handicap_HandleKey(int32_t flags, float *special, int32_t key);
int32_t UI_MapCountByGameType(void);
void UI_SelectCurrentGameType(void);
void UI_SelectCurrentMap(void);
const char *UI_SelectedMap(int32_t index, int32_t *actual);
const char *UI_FileText(const char *filename);
void UI_Pause(qboolean pause);
void UI_DrawCinematic(int32_t handle, float x, float y, float width,
                      float height);
void UI_RunCinematicFrame(int32_t handle);
int32_t UI_PlayCinematic(const char *name, float x, float y, float width,
                         float height);
void UI_StopCinematic(int32_t handle);
qboolean UI_GameType_HandleKey(int32_t flags, float *special, int32_t key);
qboolean UI_JoinGameType_HandleKey(int32_t flags, float *special,
                                   int32_t key);
qboolean UI_NetSource_HandleKey(int32_t flags, float *special, int32_t key);
qboolean UI_NetFilter_HandleKey(int32_t flags, float *special, int32_t key);
qboolean UI_NetGameType_HandleKey(int32_t flags, float *special,
                                  int32_t key);
void UI_FeederSelection(float feeder, int32_t index);
void UI_SelectCurrentMap(void);
qboolean UI_OwnerDrawHandleKey(int32_t ownerDraw, int32_t flags,
                               float *special, int32_t key);
float UI_GetValue(int32_t ownerDraw, int32_t colorRangeType);
int32_t UI_ServersQsortCompare(const void *left, const void *right);
void UI_ServersSort(int32_t column, qboolean force);
void UI_LoadMods(void);
void UI_LoadMovies(void);
void UI_LoadDemos(void);
void UI_DrawNetSource(const rectDef_t *rect, int32_t font, float scale,
                      const vec4_t color, int32_t textStyle);
void UI_DrawNetFilter(const rectDef_t *rect, int32_t font, float scale,
                      const vec4_t color, int32_t textStyle);
void UI_Load(void);
void UI_Cache_f(void);
void Menu_SetItemBackground(const char *itemName, const char *backgroundName);
void Menu_SetItemVisible(const char *itemName, qboolean visible);
qboolean UI_CheckExecKey(int32_t key);
void UI_Update(const char *name);
void UI_VerifyLanguage(void);
void ui_compat_controls_get_config(void);
void ui_compat_controls_set_defaults(void);
/* The common menu feeder controls are shared by ui_runtime.h. */
void UI_GetGameTypesList(void);
void UI_LoadArenas(void);
void UI_LoadMenus(const char *menuFile, qboolean reset, int32_t loadMode);
qboolean Load_Menu(int32_t sourceHandle, int32_t loadMode);
qboolean Load_ScriptMenu(const char *menuName, int32_t loadMode);
qboolean UI_ParseMenu(const char *filename, int32_t loadMode);
qboolean Asset_Parse(int32_t sourceHandle, int32_t loadMode);
/* Common parser keyword handlers are declared by ui_parse.h. */
/* Cvar visibility and owner-draw item runtime are shared by ui_runtime.h. */
/* Item_Action and Item_Asset_Paint are shared by ui_runtime.h. */
void UI_SyncServerListSelection(itemDef_t *item);
/* Item_RunScript and the module-owned Script_FadeIn interface are declared by
 * ui_runtime.h. */
/* The complete menu-script navigation cluster is shared by ui_runtime.h. */
/* The cvar/exec/play command family is shared by ui_runtime.h. */
/* The list/automatic-update/response commands and Script_SetFocus are shared
 * by ui_runtime.h. */
/* The visual-property command cluster is shared by ui_runtime.h. */
void gunrandom(float *x, float *y);
void UI_GetTeamColor(vec4_t color);
void UI_FeederAddItem(float feeder, const char *name, int32_t value);
void UI_StartSkirmish(void);
void UI_DrawTextBox(int32_t x, int32_t y, int32_t width, int32_t lines);
const char *stristr(const char *string, const char *substring);
int32_t UI_FeederCount(float feeder);
const char *UI_FeederItemText(float feeder, int32_t index, int32_t column,
                              int32_t *imageHandle);
qhandle_t UI_FeederItemImage(float feeder, int32_t index);
qboolean trap_LAN_ServerStatus(const char *address, char *status,
                               int32_t statusSize);
void UI_SortServerStatusInfo(uiServerStatusInfo_t *statusInfo);
qboolean UI_GetServerStatusInfo(const char *address,
                                uiServerStatusInfo_t *statusInfo);
void UI_ReadableSize(char *buffer, int32_t bufferSize, int32_t value);
void UI_PrintTime(char *buffer, int32_t bufferSize, int32_t time);
void Text_PaintCenter(float x, float y, const char *text, float scale,
                      const vec4_t color, int32_t font);
void UI_DisplayDownloadInfo(int32_t font, const char *downloadName,
                            float centerPoint, float yStart, float scale);
int32_t Menu_Count(void);
void Menu_Reset(void);
displayContextDef_t *Display_GetContext(void);
/* Shared display input dispatch is declared by ui_runtime.h. */
void UI_UpdateCvars(void);
void UI_RegisterCvars(void);
void UI_DoServerRefresh(void);
void UI_StartServerRefresh(qboolean full);
void UI_UpdatePendingPings(void);
void UI_ClearDisplayedServers(void);
void UI_UpdateDisplayServers(void);
void UI_AddServerToFavoritesList(const char *name, const char *address);
void UI_BinaryServerInsertion(int32_t index, int32_t server);
void UI_RemoveServerFromDisplayList(int32_t server);
void UI_InsertServerIntoDisplayList(int32_t server);
void UI_BuildServerDisplayList(int32_t force);
void UI_StopServerRefresh(void);
void UI_BuildServerStatus(qboolean force);
void UI_BuildFindPlayerList(qboolean force);
void UI_SetColor(const vec4_t rgba);
void UI_DrawHandlePic(float x, float y, float width, float height,
                      qhandle_t shader);
void UI_DrawNamedPic(float x, float y, float width, float height,
                     const char *name, int32_t loadMode);
void UI_DrawCenteredPic(qhandle_t shader, int32_t width, int32_t height);
void UI_AssetCache(void);
void UI_FillRect(float x, float y, float width, float height,
                 const vec4_t color);
void UI_DrawSides(float x, float y, float width, float height);
void UI_DrawTopBottom(float x, float y, float width, float height);
void UI_DrawRect(float x, float y, float width, float height,
                 const vec4_t color);
void UI_UpdateScreen(void);
void UI_DrawSidesWithSize(float x, float y, float width, float height,
                          float size);
void UI_DrawTopBottomWithSize(float x, float y, float width, float height,
                              float size);
void UI_DrawRectWithSize(float x, float y, float width, float height,
                         float size, const vec4_t color);
qboolean UI_CursorInRect(int32_t x, int32_t y, int32_t width,
                         int32_t height);
int32_t UI_ParseInfos(char *buffer, int32_t maxInfos, char **infos);
void UI_LoadArenasFromFile(const char *filename);
char *UI_LoadMenuTextFile(const char *filename);

#endif
