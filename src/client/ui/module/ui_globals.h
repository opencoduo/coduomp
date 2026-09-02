#ifndef CODUO_UI_GLOBALS_H
#define CODUO_UI_GLOBALS_H

#include <stddef.h>
#include <stdint.h>

#include "qcommon/com_parse.h"
#include "qcommon/qcommon_limits.h"
#include "qcommon/q_renderer_types.h"
#include "qcommon/q_key_types.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/server_browser_types.h"
#include "client/menu/ui_display_context_types.h"
#include "client/menu/ui_memory.h"
#include "ui_memory_config.h"
#include "client/menu/ui_menu_globals.h"
#include "client/menu/ui_menu_types.h"
#include "client/menu/ui_parse.h"

/* Source: uo_ui_mp_x86.dll 0x401c46b0 (.data/.bss).
 * Read directly by vmMain command 8. */
extern int32_t ui_activeMenu;

enum {
    UI_MAX_GAMETYPES = 32,
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    UI_MAX_MAPS = 2048
};

/* NOT_FROM_ORIGINAL_SOURCE: transient handshake allowing the engine to send
 * its normally-reserved console key to this module during that row's capture. */
#define UI_COMPAT_CONSOLE_BIND_CAPTURE_CVAR "ui_coduomp_consoleBindCapture"

enum {
    UI_MAX_MOVIES = 256
};

enum {
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    UI_MAX_ARENA_INFOS = UI_MAX_MAPS
};

extern int32_t ui_arenaInfoCount;
extern char *ui_arenaInfos[UI_MAX_ARENA_INFOS];

/* The common rectDef_t/windowDef_t/menuDef_t record family is shared by both
 * client DLLs through ui_menu_types.h. */

_Static_assert(offsetof(windowDef_t, name) == 0x20, "original windowDef_t name offset is 32 bytes");
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(windowDef_t, group) == 0x28, "original windowDef_t group offset is 40 bytes");
_Static_assert(offsetof(windowDef_t, cinematicName) == 0x2c, "original windowDef_t cinematic-name offset is 44 bytes");
_Static_assert(offsetof(windowDef_t, cinematic) == 0x30, "original windowDef_t cinematic offset is 48 bytes");
_Static_assert(offsetof(windowDef_t, style) == 0x34, "original windowDef_t style offset is 52 bytes");
_Static_assert(offsetof(windowDef_t, border) == 0x38, "original windowDef_t border offset is 56 bytes");
_Static_assert(offsetof(windowDef_t, ownerDraw) == 0x3c, "original windowDef_t owner-draw offset is 60 bytes");
_Static_assert(offsetof(windowDef_t, ownerDrawFlags) == 0x40, "original windowDef_t owner-draw flags offset is 64 bytes");
_Static_assert(offsetof(windowDef_t, borderSize) == 0x44, "original windowDef_t border-size offset is 68 bytes");
_Static_assert(offsetof(windowDef_t, flags) == 0x48, "original windowDef_t flags offset is 72 bytes");
_Static_assert(offsetof(windowDef_t, nextTime) == 0x70, "original windowDef_t next-time offset is 112 bytes");
_Static_assert(offsetof(windowDef_t, rectEffects) == 0x4c, "original windowDef_t target rectangle offset is 76 bytes");
_Static_assert(offsetof(windowDef_t, rectEffects2) == 0x5c, "original windowDef_t transition-step rectangle offset is 92 bytes");
_Static_assert(offsetof(windowDef_t, offsetTime) == 0x6c, "original windowDef_t effect interval offset is 108 bytes");
_Static_assert(offsetof(windowDef_t, foreColor) == 0x74, "original windowDef_t foreground-color offset is 116 bytes");
_Static_assert(offsetof(windowDef_t, backColor) == 0x84, "original windowDef_t background-color offset is 132 bytes");
_Static_assert(offsetof(windowDef_t, borderColor) == 0x94, "original windowDef_t border-color offset is 148 bytes");
_Static_assert(offsetof(windowDef_t, outlineColor) == 0xa4, "original windowDef_t outline-color offset is 164 bytes");
_Static_assert(offsetof(windowDef_t, background) == 0xb4, "original windowDef_t background handle offset is 180 bytes");
_Static_assert(sizeof(windowDef_t) == 184, "original windowDef_t size is 184 bytes");
#endif

/* The complete item/type-data and capture-state families are shared with
 * cgame through ui_menu_types.h and ui_menu_globals.h. */

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(itemDef_t, textRect) == 0xb8, "original itemDef_t text rectangle offset is 184 bytes");
_Static_assert(offsetof(itemDef_t, textRect.w) == 0xc0, "original itemDef_t text rectangle width offset is 192 bytes");
_Static_assert(offsetof(itemDef_t, textRect.h) == 0xc4, "original itemDef_t text rectangle height offset is 196 bytes");
_Static_assert(offsetof(itemDef_t, type) == 0xc8, "original itemDef_t type offset is 200 bytes");
_Static_assert(offsetof(itemDef_t, typeValidated) == 0xcc, "original itemDef_t validated-type offset is 204 bytes");
_Static_assert(offsetof(itemDef_t, alignment) == 0xd0, "original itemDef_t owner-draw alignment offset is 208 bytes");
_Static_assert(offsetof(itemDef_t, font) == 0xd4, "original itemDef_t font offset is 212 bytes");
_Static_assert(offsetof(itemDef_t, textalignment) == 0xd8, "original itemDef_t text-alignment offset is 216 bytes");
_Static_assert(offsetof(itemDef_t, textalignx) == 0xdc, "original itemDef_t text-alignment X offset is 220 bytes");
_Static_assert(offsetof(itemDef_t, textaligny) == 0xe0, "original itemDef_t text-alignment Y offset is 224 bytes");
_Static_assert(offsetof(itemDef_t, textscale) == 0xe4, "original itemDef_t text-scale offset is 228 bytes");
_Static_assert(offsetof(itemDef_t, textStyle) == 0xe8, "original itemDef_t text-style offset is 232 bytes");
_Static_assert(offsetof(itemDef_t, text) == 0xec, "original itemDef_t text offset is 236 bytes");
_Static_assert(offsetof(itemDef_t, parent) == 0xf0, "original itemDef_t parent offset is 240 bytes");
_Static_assert(offsetof(itemDef_t, asset) == 0xf4, "original itemDef_t asset handle offset is 244 bytes");
_Static_assert(offsetof(itemDef_t, mouseEnterText) == 0xf8, "original itemDef_t mouse-enter-text script offset is 248 bytes");
_Static_assert(offsetof(itemDef_t, mouseExitText) == 0xfc, "original itemDef_t mouse-exit-text script offset is 252 bytes");
_Static_assert(offsetof(itemDef_t, mouseEnter) == 0x100, "original itemDef_t mouse-enter script offset is 256 bytes");
_Static_assert(offsetof(itemDef_t, mouseExit) == 0x104, "original itemDef_t mouse-exit script offset is 260 bytes");
_Static_assert(offsetof(itemDef_t, action) == 0x108, "original itemDef_t action script offset is 264 bytes");
_Static_assert(offsetof(itemDef_t, accept) == 0x10c, "original itemDef_t accept script offset is 268 bytes");
_Static_assert(offsetof(itemDef_t, onFocus) == 0x110, "original itemDef_t focus script offset is 272 bytes");
_Static_assert(offsetof(itemDef_t, leaveFocus) == 0x114, "original itemDef_t leave-focus script offset is 276 bytes");
_Static_assert(offsetof(itemDef_t, cvar) == 0x118, "original itemDef_t cvar offset is 280 bytes");
_Static_assert(offsetof(itemDef_t, cvarTest) == 0x11c, "original itemDef_t cvar-test offset is 284 bytes");
_Static_assert(offsetof(itemDef_t, enableCvar) == 0x120, "original itemDef_t enable-cvar script offset is 288 bytes");
_Static_assert(offsetof(itemDef_t, cvarFlags) == 0x124, "original itemDef_t cvar flags offset is 292 bytes");
_Static_assert(offsetof(itemDef_t, focusSound) == 0x128, "original itemDef_t focus-sound offset is 296 bytes");
_Static_assert(offsetof(itemDef_t, numColors) == 0x12c, "original itemDef_t color-range count offset is 300 bytes");
_Static_assert(offsetof(itemDef_t, colorRanges) == 0x130, "original itemDef_t color-range array offset is 304 bytes");
_Static_assert(offsetof(itemDef_t, colorRangeType) == 0x248, "original itemDef_t color-range type offset is 584 bytes");
_Static_assert(offsetof(itemDef_t, special) == 0x24c, "original itemDef_t feeder/special offset is 588 bytes");
_Static_assert(offsetof(itemDef_t, cursorPos) == 0x250, "original itemDef_t cursor offset is 592 bytes");
_Static_assert(offsetof(itemDef_t, typeData) == 0x254, "original itemDef_t type-data offset is 596 bytes");
_Static_assert(offsetof(itemDef_t, loadMode) == 0x258, "original itemDef_t load-mode offset is 600 bytes");
_Static_assert(sizeof(itemDef_t) == 604, "original itemDef_t size is 604 bytes");
#endif

_Static_assert(offsetof(menuDef_t, window.rect) == 0x00, "original menuDef_t rectangle offset is 0");
_Static_assert(offsetof(menuDef_t, window.name) == 0x20, "original menuDef_t name offset is 32 bytes");
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(menuDef_t, font) == 0xb8, "original menuDef_t font offset is 184 bytes");
_Static_assert(offsetof(menuDef_t, fullScreen) == 0xbc, "original menuDef_t fullscreen offset is 188 bytes");
_Static_assert(offsetof(menuDef_t, cursorItem) == 0xc8, "original menuDef_t cursor-item offset is 200 bytes");
_Static_assert(offsetof(menuDef_t, itemCount) == 0xc0, "original menuDef_t item-count offset is 192 bytes");
_Static_assert(offsetof(menuDef_t, fontIndex) == 0xc4, "original menuDef_t font-index offset is 196 bytes");
_Static_assert(offsetof(menuDef_t, fadeCycle) == 0xcc, "original menuDef_t fade-cycle offset is 204 bytes");
_Static_assert(offsetof(menuDef_t, onOpen) == 0xdc, "original menuDef_t onOpen offset is 220 bytes");
_Static_assert(offsetof(menuDef_t, onClose) == 0xe0, "original menuDef_t onClose offset is 224 bytes");
_Static_assert(offsetof(menuDef_t, onESC) == 0xe4, "original menuDef_t onESC offset is 228 bytes");
_Static_assert(offsetof(menuDef_t, onKey) == 0xe8, "original menuDef_t key-script array offset is 232 bytes");
_Static_assert(offsetof(menuDef_t, onKey[255]) == 0x4e4, "original menuDef_t any-key script offset is 1252 bytes");
_Static_assert(offsetof(menuDef_t, soundName) == 0x4e8, "original menuDef_t sound-loop offset is 1256 bytes");
_Static_assert(offsetof(menuDef_t, loadMode) == 0x4ec, "original menuDef_t load-mode offset is 1260 bytes");
_Static_assert(offsetof(menuDef_t, focusColor) == 0x4f0, "original menuDef_t focus-color offset is 1264 bytes");
_Static_assert(offsetof(menuDef_t, disableColor) == 0x500, "original menuDef_t disable-color offset is 1280 bytes");
_Static_assert(offsetof(menuDef_t, items) == 0x510, "original menuDef_t item-pointer array offset is 1296 bytes");
_Static_assert(sizeof(menuDef_t) == 2064, "original menuDef_t stride is 2064 bytes");
#endif

typedef struct {
    const char *gameType;
    const char *displayName;
} uiGameTypeInfo_t;

enum {
    UI_MAX_TEAMS = 64
};

typedef struct {
    const char *name;
    const char *imageName;
    qhandle_t imageShader;
    qboolean selected;
} uiTeamInfo_t;

typedef struct {
    const char *displayName;
    const char *mapName;
    const char *imageName;
    /* Names and ordering inherited unchanged from Q3 mapInfo. */
    const char *opponentName;
    int32_t teamMembers;
    int32_t typeBits;
    int32_t cinematic;
    int32_t timeToBeat[UI_MAX_GAMETYPES];
    qhandle_t imageShader;
    qboolean active;
} uiMapInfo_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(uiGameTypeInfo_t) == 8, "original uiGameTypeInfo_t stride is 8 bytes");
_Static_assert(sizeof(uiTeamInfo_t) == 16, "original uiTeamInfo_t stride is 16 bytes");
_Static_assert(sizeof(uiMapInfo_t) == 164, "original uiMapInfo_t stride is 164 bytes");
_Static_assert(offsetof(uiMapInfo_t, imageName) == 0x08, "original map image-name offset is 8 bytes");
_Static_assert(offsetof(uiMapInfo_t, opponentName) == 0x0c, "original map opponent-name offset is 12 bytes");
_Static_assert(offsetof(uiMapInfo_t, teamMembers) == 0x10, "original map team-members offset is 16 bytes");
_Static_assert(offsetof(uiMapInfo_t, typeBits) == 0x14, "original map gametype-bit offset is 20 bytes");
_Static_assert(offsetof(uiMapInfo_t, cinematic) == 0x18, "original map cinematic offset is 24 bytes");
_Static_assert(offsetof(uiMapInfo_t, imageShader) == 0x9c, "original map image shader offset is 156 bytes");
_Static_assert(offsetof(uiMapInfo_t, active) == 0xa0, "original map active flag offset is 160 bytes");
#endif

extern int32_t ui_gameTypeCount;
extern uiGameTypeInfo_t ui_gameTypes[UI_MAX_GAMETYPES];
extern int32_t ui_teamCount;
extern uiTeamInfo_t ui_teams[UI_MAX_TEAMS];
extern int32_t ui_joinGameTypeCount;
extern uiGameTypeInfo_t ui_joinGameTypes[UI_MAX_GAMETYPES];
extern const char *ui_movieNames[UI_MAX_MOVIES];
extern int32_t ui_movieCount;
extern int32_t ui_movieIndex;
extern int32_t ui_previewMovie;
extern int32_t ui_demoIndex;

enum {
    UI_MAX_DEMOS = 256
};

extern int32_t ui_demoCount;
extern const char *ui_demoNames[UI_MAX_DEMOS];
extern qhandle_t ui_serverMapPreviewShader;
extern int32_t ui_serverMapCinematic;
extern int32_t ui_serverFilterType;

typedef struct uiServerFilter_s {
    const char *label;
    const char *gameName;
} uiServerFilter_t;

enum {
    UI_SERVER_FILTER_COUNT = 1
};

extern const uiServerFilter_t ui_serverFilters[UI_SERVER_FILTER_COUNT];
extern const char *const ui_handicapLabels[22];

extern char ui_ownerDrawWidthBuffer[64];
extern int32_t ui_mapCount;
extern uiMapInfo_t ui_maps[UI_MAX_MAPS];

extern char ui_argvBuffer[MAX_STRING_CHARS];
extern char ui_menuFilesBuffer[MAX_STRING_CHARS];
extern qboolean ui_bypassMouseInput;
extern int32_t ui_activeFont;

/*
 * The UI DLL extends the common displayContextDef_t storage with six
 * front-end-only words.  Keeping this as an owning wrapper preserves the
 * original contiguous 0x1e3f8-byte allocation without extending cgame's
 * shorter instance into its next independent global.
 */
typedef struct uiDisplayContextStorage_s {
    displayContextDef_t context;
    int32_t newHighScoreTime; /* +0x1e3e0 */
    int32_t newBestTime; /* +0x1e3e4 */
    int32_t showPostGameTime; /* +0x1e3e8 */
    qboolean newHighScore; /* +0x1e3ec */
    qboolean demoAvailable; /* +0x1e3f0 */
    qboolean soundHighScore; /* +0x1e3f4 */
} uiDisplayContextStorage_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(uiDisplayContextStorage_t) == 0x1e3f8, "original UI display-context storage size is 123896 bytes");
_Static_assert(offsetof(uiDisplayContextStorage_t, newHighScoreTime) == 0x1e3e0, "original UI high-score deadline offset is 123872 bytes");
_Static_assert(offsetof(uiDisplayContextStorage_t, soundHighScore) == 0x1e3f4, "original UI high-score sound flag offset is 123892 bytes");
#endif

extern uiDisplayContextStorage_t ui_displayContextStorage;
extern int32_t ui_frameSampleCount;
extern int32_t ui_frameSamples[4];

extern qboolean ui_serverRefreshActive;
extern int32_t ui_serverRefreshTime;
extern int32_t ui_nextDisplayRefresh;
extern int32_t ui_serverCount;
extern int32_t ui_displayServerCount;
extern int32_t ui_numPlayers;
extern int32_t ui_filteredServerCount;
extern int32_t ui_currentServer;
extern int32_t ui_motdLength;
extern int32_t ui_motdOffset;
extern char ui_motd[MAX_STRING_CHARS];

enum {
    UI_MAX_DISPLAY_SERVERS = 20000
};

extern int32_t ui_serverSortKey;
extern int32_t ui_serverSortDirection;
extern int32_t ui_displayServers[UI_MAX_DISPLAY_SERVERS];

enum {
    UI_MAX_PLAYER_NAMES = 64,
    UI_PLAYER_NAME_SIZE = 32
};

enum {
    UI_MAX_FOUND_PLAYER_SERVERS = 16,
    UI_FOUND_PLAYER_SERVER_TEXT_SIZE = 64,
    UI_MAX_TEAM_PLAYER_NAMES = 64,
    UI_SERVER_HARDWARE_SHADER_COUNT = 6
};

enum {
    UI_SERVER_STATUS_MAX_LINES = MAX_CLIENTS * 2,
    UI_SERVER_STATUS_TEXT_SIZE = 1024,
    UI_SERVER_STATUS_PLAYER_NUMBER_SIZE = 5
};

_Static_assert(MAX_CLIENTS > 0 && MAX_CLIENTS <= 9999, "server-status player numbers require at most four digits");

typedef struct {
    const char *column[4];
} uiServerStatusLine_t;

typedef struct {
    char address[64];
    uiServerStatusLine_t lines[UI_SERVER_STATUS_MAX_LINES];
    char text[UI_SERVER_STATUS_TEXT_SIZE];
    char numberText[MAX_CLIENTS][UI_SERVER_STATUS_PLAYER_NUMBER_SIZE];
    int32_t numLines;
} uiServerStatusInfo_t;

typedef struct {
    char address[64];
    char name[64];
    int32_t startTime;
    /* Q3 pendingServer_t name and exact position. */
    int32_t serverNum;
    qboolean active;
} uiPendingServerStatus_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(uiServerStatusLine_t) == 16, "uiServerStatusLine_t target layout");
_Static_assert(offsetof(uiServerStatusInfo_t, lines) == 0x40, "uiServerStatusInfo_t lines target offset");
_Static_assert(offsetof(uiServerStatusInfo_t, text) == 0x840, "uiServerStatusInfo_t text target offset");
_Static_assert(offsetof(uiServerStatusInfo_t, numberText) == 0xc40, "uiServerStatusInfo_t numberText target offset");
_Static_assert(offsetof(uiServerStatusInfo_t, numLines) ==
                   offsetof(uiServerStatusInfo_t, numberText) + sizeof(((uiServerStatusInfo_t *)0)->numberText),
               "uiServerStatusInfo_t number storage precedes line count");
_Static_assert(sizeof(uiServerStatusInfo_t) == offsetof(uiServerStatusInfo_t, numLines) + sizeof(((uiServerStatusInfo_t *)0)->numLines),
               "uiServerStatusInfo_t line count terminates the record");
_Static_assert(sizeof(uiPendingServerStatus_t) == 140, "uiPendingServerStatus_t target size");
_Static_assert(offsetof(uiPendingServerStatus_t, serverNum) == 0x84, "uiPendingServerStatus_t server-number target offset");
_Static_assert(offsetof(uiPendingServerStatus_t, active) == 0x88, "uiPendingServerStatus_t active target offset");
#endif

extern int32_t ui_playerCount;
extern int32_t ui_playerRefreshDeadline;
extern int32_t ui_playerIndex;
extern int32_t ui_teamListReservedMarker;
extern int32_t ui_gameTypeListReservedMarker;
extern int32_t ui_serverStatusNextRefresh;
extern int32_t ui_findPlayerNextRefresh;
extern char ui_serverStatusAddress[64];
extern uiServerStatusInfo_t ui_serverStatusInfo;
extern int32_t ui_myClientNum;
extern char ui_playerNames[UI_MAX_PLAYER_NAMES][UI_PLAYER_NAME_SIZE];
extern int32_t ui_teamPlayerCount;
extern char ui_teamPlayerNames[UI_MAX_TEAM_PLAYER_NAMES][UI_PLAYER_NAME_SIZE];
extern int32_t ui_foundPlayerServerCount;

typedef union {
    char findPlayerName[MAX_STRING_CHARS];
    struct {
        unsigned char overlapPrefix[960];
        char serverAddresses[UI_MAX_FOUND_PLAYER_SERVERS][UI_FOUND_PLAYER_SERVER_TEXT_SIZE];
    } addressView;
} uiFindPlayerStorage_t;

_Static_assert(offsetof(uiFindPlayerStorage_t, addressView.serverAddresses) == 960, "retail find-player/address overlap offset");
_Static_assert(sizeof(uiFindPlayerStorage_t) == 1984, "retail find-player overlap carrier size");

extern uiFindPlayerStorage_t ui_findPlayerStorage;
#define ui_findPlayerName ui_findPlayerStorage.findPlayerName
#define ui_foundPlayerServerAddresses ui_findPlayerStorage.addressView.serverAddresses
extern char ui_foundPlayerServerNames[UI_MAX_FOUND_PLAYER_SERVERS][UI_FOUND_PLAYER_SERVER_TEXT_SIZE];
extern qhandle_t ui_serverHardwareShaders[UI_SERVER_HARDWARE_SHADER_COUNT];
extern qhandle_t ui_punkbusterShader;
extern int32_t ui_cachedServerInfoColumn;
extern int32_t ui_cachedServerInfoTime;
extern char ui_cachedServerInfo[MAX_STRING_CHARS];
extern char ui_serverClientText[32];
extern int32_t ui_findPlayerServerIndex;
extern uiPendingServerStatus_t ui_pendingServerStatus[UI_MAX_FOUND_PLAYER_SERVERS];
extern int32_t ui_findPlayerCompletedCount;
extern int32_t ui_findPlayerRequestCount;
extern int32_t ui_teamLeader;
extern int32_t ui_playerNumbers[UI_MAX_PLAYER_NAMES];
extern const char *ui_newHighScoreSound;

enum {
    UI_DOWNLOAD_ESTIMATE_SAMPLES = 80
};

extern int32_t ui_downloadEstimateIndex;
extern int32_t ui_downloadEstimates[UI_DOWNLOAD_ESTIMATE_SAMPLES];

enum {
    UI_MAX_MODS = 64
};

typedef struct {
    const char *directory;
    const char *description;
} uiModInfo_t;

extern int32_t ui_modCount;
extern uiModInfo_t ui_mods[UI_MAX_MODS];
extern int32_t ui_modIndex;
extern int32_t ui_teamPlayerIndex;
extern int32_t ui_foundPlayerServerIndex;
extern char ui_selectedServerInfo[MAX_STRING_CHARS];

enum {
    UI_MAX_CVARS = 73
};

extern vmCvar_t g_arenasFileCvar;
extern vmCvar_t ui_dedicatedCvar;
extern vmCvar_t ui_smallFontCvar;
extern vmCvar_t ui_bigFontCvar;
extern vmCvar_t ui_extraBigFontCvar;
extern vmCvar_t cg_selectedPlayerCvar;
extern vmCvar_t ui_netSourceCvar;
extern vmCvar_t ui_gametypeCvar;
extern vmCvar_t ui_joinGametypeCvar;
extern vmCvar_t ui_netGametypeCvar;
extern vmCvar_t ui_mapIndexCvar;
extern vmCvar_t ui_currentMapCvar;
extern vmCvar_t ui_currentNetMapCvar;
extern vmCvar_t ui_browserShowFullCvar;
extern vmCvar_t ui_browserShowEmptyCvar;
extern vmCvar_t ui_browserShowPasswordCvar;
extern vmCvar_t ui_browserShowNoPasswordCvar;
extern vmCvar_t ui_browserShowPureCvar;
extern vmCvar_t ui_browserShowDedicatedCvar;
extern vmCvar_t ui_browserShowJeepsCvar;
extern vmCvar_t ui_browserShowTanksCvar;
extern vmCvar_t ui_browserModCvar;
extern vmCvar_t ui_browserFriendlyfireCvar;
extern vmCvar_t ui_browserKillcamCvar;
extern vmCvar_t ui_browserShowPunkBusterCvar;
extern vmCvar_t ui_serverStatusTimeOutCvar;
extern vmCvar_t cl_languageWarningsCvar;
extern vmCvar_t cl_languageWarningsAsErrorsCvar;

#define ui_dedicated ui_dedicatedCvar
#define ui_smallFontThreshold ui_smallFontCvar.value
#define ui_bigFontThreshold ui_bigFontCvar.value
#define ui_extraBigFontThreshold ui_extraBigFontCvar.value
#define ui_netSource ui_netSourceCvar.integer
#define ui_gameType ui_gametypeCvar.integer
#define ui_joinGameType ui_joinGametypeCvar.integer
#define ui_netGameType ui_netGametypeCvar.integer
#define ui_currentMap ui_currentMapCvar.integer
#define ui_currentNetMap ui_currentNetMapCvar.integer
#define ui_browserShowFull ui_browserShowFullCvar
#define ui_browserShowEmpty ui_browserShowEmptyCvar
#define ui_browserShowPassword ui_browserShowPasswordCvar
#define ui_browserShowNoPassword ui_browserShowNoPasswordCvar
#define ui_browserShowPure ui_browserShowPureCvar
#define ui_browserShowDedicated ui_browserShowDedicatedCvar
#define ui_browserShowJeeps ui_browserShowJeepsCvar
#define ui_browserShowTanks ui_browserShowTanksCvar
#define ui_browserMod ui_browserModCvar
#define ui_browserFriendlyfire ui_browserFriendlyfireCvar
#define ui_browserKillcam ui_browserKillcamCvar
#define ui_browserShowPunkBuster ui_browserShowPunkBusterCvar
#define ui_serverStatusTimeOut ui_serverStatusTimeOutCvar
#define cl_languageWarnings cl_languageWarningsCvar
#define cl_languageWarningsAsErrors cl_languageWarningsAsErrorsCvar

extern cvarTable_t ui_cvarTable[UI_MAX_CVARS];
extern int32_t ui_cvarCount;
extern qboolean ui_menuLoadActive;
/* NOT_FROM_ORIGINAL_SOURCE: native VM-image lifecycle reset helpers. */
void ui_compat_reset_module_load_state(void);
void ui_compat_reset_control_binding_state(void);
void ui_compat_reset_memory_pool_state(void);
void ui_compat_reset_random_geometry_state(void);
void ui_compat_reset_temp_runtime_state(void);
void ui_compat_reset_window_state(void);

#endif
