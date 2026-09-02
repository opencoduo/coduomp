#include "ui_globals.h"
#include "math/q_math.h"

#include <string.h>

// Source: uo_ui_mp_x86.dll 0x401c46b0.
int32_t ui_activeMenu;
// Source: uo_ui_mp_x86.dll 0x4021c9a8.
int32_t ui_gameTypeCount;
// Source: uo_ui_mp_x86.dll 0x4021c9ac..0x4021caab.
uiGameTypeInfo_t ui_gameTypes[UI_MAX_GAMETYPES];
// Source: uo_ui_mp_x86.dll 0x4021b798..0x4021bba0.
int32_t ui_teamCount;
// Source: uo_ui_mp_x86.dll 0x4021b7a0..0x4021bb9f.
uiTeamInfo_t ui_teams[UI_MAX_TEAMS];
// Source: uo_ui_mp_x86.dll 0x4021caac/0x4021cab0.
int32_t ui_joinGameTypeCount;
// Source: uo_ui_mp_x86.dll 0x4021cab0..0x4021cbaf.
uiGameTypeInfo_t ui_joinGameTypes[UI_MAX_GAMETYPES];
// Source: uo_ui_mp_x86.dll 0x402234e8..0x402238e7.
const char *ui_movieNames[UI_MAX_MOVIES];
// Source: uo_ui_mp_x86.dll 0x402238e8.
int32_t ui_movieCount;
// Source: uo_ui_mp_x86.dll 0x402238ec.
int32_t ui_movieIndex;
// Source: uo_ui_mp_x86.dll 0x402238f0.
int32_t ui_previewMovie;
// Source: uo_ui_mp_x86.dll 0x402234e4.
int32_t ui_demoIndex;
// Source: uo_ui_mp_x86.dll 0x402234e0/0x402230e0..0x402234df.
int32_t ui_demoCount;
// Source: uo_ui_mp_x86.dll 0x402230e0..0x402234df.
const char *ui_demoNames[UI_MAX_DEMOS];
// Source: uo_ui_mp_x86.dll 0x40237610; server-browser map preview shader.
qhandle_t ui_serverMapPreviewShader;
// Source: uo_ui_mp_x86.dll 0x40237614; server-browser map cinematic handle.
int32_t ui_serverMapCinematic;
// Source: uo_ui_mp_x86.dll 0x401f96ec; server-filter selection.
int32_t ui_serverFilterType;
/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
const uiServerFilter_t ui_serverFilters[UI_SERVER_FILTER_COUNT] = {{"EXE_ALL", ""}};
// Source: uo_ui_mp_x86.dll 0x40060400; owner-draw width scratch text.
char ui_ownerDrawWidthBuffer[64];
/* Source: uo_ui_mp_x86.dll 0x40060560..0x4006155f.  PC_SourceWarning's
 * module-local message storage; kept local because overflow adjacency differs
 * from cgame's retail data layout. */
char pc_sourceWarningMessage[4096];
// Source: uo_ui_mp_x86.dll 0x40239c50.
int32_t ui_activeFont;
// Source: uo_ui_mp_x86.dll 0x4021dcd0.
int32_t ui_mapCount;
// Source: uo_ui_mp_x86.dll 0x4021dcd4..0x40222ed3.
uiMapInfo_t ui_maps[UI_MAX_MAPS];
// Source: uo_ui_mp_x86.dll 0x400556d0..0x40055acf.
char ui_argvBuffer[MAX_STRING_CHARS];
// Source: uo_ui_mp_x86.dll 0x40055ad0..0x40055ecf.
char ui_menuFilesBuffer[MAX_STRING_CHARS];
// Source: uo_ui_mp_x86.dll 0x401c46ac.
qboolean ui_bypassMouseInput;
// Source: uo_ui_mp_x86.dll 0x401c46d8.
int32_t menuCount;
// Source: uo_ui_mp_x86.dll 0x401c46dc.
int32_t openMenuCount;
// Source: uo_ui_mp_x86.dll 0x401f9220..0x401f925f.
menuDef_t *menuStack[MAX_OPEN_MENUS];
// Source: uo_ui_mp_x86.dll 0x401c62e0..0x401f891f.
menuDef_t Menus[MAX_MENUS];
// Source: uo_ui_mp_x86.dll 0x401c46c0.
itemDef_t *captureItem;
// Source: uo_ui_mp_x86.dll 0x401c46b8.
ui_captureFunc_t captureFunc;
// Source: uo_ui_mp_x86.dll 0x401c46bc.
void *captureData;
// Source: uo_ui_mp_x86.dll 0x401c35f8..0x401c3617.
scrollInfo_t ui_scrollInfo;
/* Source: uo_ui_mp_x86.dll 0x401c3618..0x401c4617.  This buffer immediately
 * follows ui_scrollInfo in retail and precedes the key-name storage. */
char pc_sourceErrorMessage[4096];
// Source: uo_ui_mp_x86.dll 0x401c46c8.
int32_t g_waitingForKey;
// Source: uo_ui_mp_x86.dll 0x401c46d0.
itemDef_t *g_bindItem;
// Source: uo_ui_mp_x86.dll 0x401c46cc.
int32_t g_editingField;
// Source: uo_ui_mp_x86.dll 0x401c46d4.
itemDef_t *g_editItem;
// Source: uo_ui_mp_x86.dll 0x401c46f0.
int32_t inHandleKey;
// Source: uo_ui_mp_x86.dll 0x401c46e4; listbox double-click deadline.
int32_t lastListBoxClickTime;
// Source: uo_ui_mp_x86.dll 0x40055ed0/0x40055fd0.
char *ui_arenaInfos[UI_MAX_ARENA_INFOS];
// Source: uo_ui_mp_x86.dll 0x40055fd0.
int32_t ui_arenaInfoCount;
// Source: uo_ui_mp_x86.dll 0x401fd3a0..0x4021b797.
uiDisplayContextStorage_t ui_displayContextStorage;
// Source: uo_ui_mp_x86.dll 0x401c46c4; UI_Init installs 0x401fd3a0.
displayContextDef_t *DC;
// Source: uo_ui_mp_x86.dll 0x4005fbf8.
int32_t ui_frameSampleCount;
// Source: uo_ui_mp_x86.dll 0x40060444..0x40060453.
int32_t ui_frameSamples[4];
// Source: uo_ui_mp_x86.dll 0x401c46e0.
int32_t debugMode;
// Source: uo_ui_mp_x86.dll 0x40223d74.
qboolean ui_serverRefreshActive;
// Source: uo_ui_mp_x86.dll 0x40223d60.
int32_t ui_serverRefreshTime;
// Source: uo_ui_mp_x86.dll 0x40237608.
int32_t ui_nextDisplayRefresh;
// Source: uo_ui_mp_x86.dll 0x40237600; last observed LAN server count.
int32_t ui_serverCount;
// Source: uo_ui_mp_x86.dll 0x402375fc.
int32_t ui_displayServerCount;
// Source: uo_ui_mp_x86.dll 0x40237604.
int32_t ui_numPlayers;
// Source: uo_ui_mp_x86.dll 0x40060458; accepted positive-ping count.
int32_t ui_filteredServerCount;
// Source: uo_ui_mp_x86.dll 0x40223d78.
int32_t ui_currentServer;
// Source: uo_ui_mp_x86.dll 0x40237618/0x4023761c/0x40237630.
int32_t ui_motdLength;
// Source: uo_ui_mp_x86.dll 0x4023761c.
int32_t ui_motdOffset;
// Source: uo_ui_mp_x86.dll 0x40237630..0x40237a2f.
char ui_motd[MAX_STRING_CHARS];
// Source: uo_ui_mp_x86.dll vmCvar mirrors; listed addresses are integer +0x0c.
// Source: uo_ui_mp_x86.dll 0x40223d68/0x40223d6c.
int32_t ui_serverSortKey;
// Source: uo_ui_mp_x86.dll 0x40223d6c.
int32_t ui_serverSortDirection;
// Source: uo_ui_mp_x86.dll 0x40223d7c..0x402375fb.
int32_t ui_displayServers[UI_MAX_DISPLAY_SERVERS];
// Source: uo_ui_mp_x86.dll 0x4021cbb4.
int32_t ui_playerCount;
// Source: uo_ui_mp_x86.dll 0x4021cbc0; player-list refresh deadline.
int32_t ui_playerRefreshDeadline;
// Source: uo_ui_mp_x86.dll 0x4021cbc4.
int32_t ui_playerIndex;
// Source: uo_ui_mp_x86.dll 0x4021bba0; init-only store.
int32_t ui_teamListReservedMarker;
// Source: uo_ui_mp_x86.dll 0x4021bea4; init-only store.
int32_t ui_gameTypeListReservedMarker;
// Source: uo_ui_mp_x86.dll 0x40238774; server-status retry deadline.
int32_t ui_serverStatusNextRefresh;
// Source: uo_ui_mp_x86.dll 0x40239c44; find-player refresh deadline.
int32_t ui_findPlayerNextRefresh;
// Source: uo_ui_mp_x86.dll 0x40237a30..0x40237a6f.
char ui_serverStatusAddress[64];
// Source: uo_ui_mp_x86.dll 0x40237a70..0x40238773.
uiServerStatusInfo_t ui_serverStatusInfo;
// Source: uo_ui_mp_x86.dll 0x4021cbc8.
int32_t ui_myClientNum;
// Source: uo_ui_mp_x86.dll 0x4021cbd0..0x4021d3cf.
char ui_playerNames[UI_MAX_PLAYER_NAMES][UI_PLAYER_NAME_SIZE];
// Source: uo_ui_mp_x86.dll 0x4021cbb8/0x4021d3d0.
int32_t ui_teamPlayerCount;
// Source: uo_ui_mp_x86.dll 0x4021d3d0..0x4021dbcf.
char ui_teamPlayerNames[UI_MAX_TEAM_PLAYER_NAMES][UI_PLAYER_NAME_SIZE];
// Source: uo_ui_mp_x86.dll 0x40239c40.
int32_t ui_foundPlayerServerCount;
/* Source: uo_ui_mp_x86.dll 0x4023903c..0x402397fb.
 * The 1024-byte find-player syscall destination overlaps the first 64-byte
 * address slot at offset 960; result counting starts at one, leaving that
 * overlapping slot reserved exactly as the machine-code indexing requires. */
uiFindPlayerStorage_t ui_findPlayerStorage;
// Source: uo_ui_mp_x86.dll 0x402397fc..0x40239bfb.
char ui_foundPlayerServerNames[UI_MAX_FOUND_PLAYER_SERVERS][UI_FOUND_PLAYER_SERVER_TEXT_SIZE];
// Source: uo_ui_mp_x86.dll 0x402238f4..0x4022390b.
qhandle_t ui_serverHardwareShaders[UI_SERVER_HARDWARE_SHADER_COUNT];
// Source: uo_ui_mp_x86.dll 0x4022390c.
qhandle_t ui_punkbusterShader;
// Source: uo_ui_mp_x86.dll 0x40040434/0x401c46a8/0x40055fd8/0x4005fbd8.
int32_t ui_cachedServerInfoColumn = -1;
// Source: uo_ui_mp_x86.dll 0x401c46a8.
int32_t ui_cachedServerInfoTime;
// Source: uo_ui_mp_x86.dll 0x40055fd8..0x400563d7.
char ui_cachedServerInfo[MAX_STRING_CHARS];
// Source: uo_ui_mp_x86.dll 0x4005fbd8..0x4005fbf7.
char ui_serverClientText[32];
// Source: uo_ui_mp_x86.dll 0x40238778..0x4023903b.
int32_t ui_findPlayerServerIndex;
// Source: uo_ui_mp_x86.dll 0x4023877c..0x4023903b.
uiPendingServerStatus_t ui_pendingServerStatus[UI_MAX_FOUND_PLAYER_SERVERS];
// Source: uo_ui_mp_x86.dll 0x40060440/0x40060454.
int32_t ui_findPlayerCompletedCount;
// Source: uo_ui_mp_x86.dll 0x40060454.
int32_t ui_findPlayerRequestCount;
// Source: uo_ui_mp_x86.dll 0x4021cbcc.
int32_t ui_teamLeader;
// Source: uo_ui_mp_x86.dll 0x4021dbd0..0x4021dccf.
int32_t ui_playerNumbers[UI_MAX_PLAYER_NAMES];
// Source: uo_ui_mp_x86.dll 0x40239c4c.
const char *ui_newHighScoreSound;
// Source: uo_ui_mp_x86.dll 0x401c46b4/0x40040470..0x400405af.
int32_t ui_downloadEstimateIndex;
// Source: uo_ui_mp_x86.dll 0x40040470..0x400405af.
int32_t ui_downloadEstimates[UI_DOWNLOAD_ESTIMATE_SAMPLES] = {
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60};
// Source: uo_ui_mp_x86.dll 0x402230d8/0x40222ed8..0x402230d7.
int32_t ui_modCount;
// Source: uo_ui_mp_x86.dll 0x40222ed8..0x402230d7.
uiModInfo_t ui_mods[UI_MAX_MODS];
// Source: uo_ui_mp_x86.dll 0x402230dc.
int32_t ui_modIndex;
// Source: uo_ui_mp_x86.dll 0x4021cbbc.
int32_t ui_teamPlayerIndex;
// Source: uo_ui_mp_x86.dll 0x40239c3c.
int32_t ui_foundPlayerServerIndex;
// Source: uo_ui_mp_x86.dll 0x400573d8..0x400577d7.
char ui_selectedServerInfo[MAX_STRING_CHARS];
// Source: uo_ui_mp_x86.dll 0x401fc080..0x401fc18f.
vmCvar_t g_arenasFileCvar;
// Source: uo_ui_mp_x86.dll 0x401f95c0..0x401f96cf.
vmCvar_t g_allowvoteCvar;
// Source: uo_ui_mp_x86.dll 0x401fb1e0..0x401fb2ef.
vmCvar_t g_allowVoteMapRestartCvar;
// Source: uo_ui_mp_x86.dll 0x401fab20..0x401fac2f.
vmCvar_t g_allowVoteMapRotateCvar;
// Source: uo_ui_mp_x86.dll 0x401f9800..0x401f990f.
vmCvar_t g_allowVoteTypeMapCvar;
// Source: uo_ui_mp_x86.dll 0x4023af80..0x4023b08f.
vmCvar_t g_allowVoteMapCvar;
// Source: uo_ui_mp_x86.dll 0x401fc3e0..0x401fc4ef.
vmCvar_t g_allowVoteGameTypeCvar;
// Source: uo_ui_mp_x86.dll 0x401f9c80..0x401f9d8f.
vmCvar_t g_allowVoteKickCvar;
// Source: uo_ui_mp_x86.dll 0x40239c60..0x40239d6f.
vmCvar_t g_allowVoteClientKickCvar;
// Source: uo_ui_mp_x86.dll 0x4023b1c0..0x4023b2cf.
vmCvar_t g_allowVoteTempBanUserCvar;
// Source: uo_ui_mp_x86.dll 0x401fb9c0..0x401fbacf.
vmCvar_t g_allowVoteTempBanClientCvar;
// Source: uo_ui_mp_x86.dll 0x401fa8e0..0x401fa9ef.
vmCvar_t cg_brassCvar;
// Source: uo_ui_mp_x86.dll 0x401fa340..0x401fa44f.
vmCvar_t cg_marksCvar;
// Source: uo_ui_mp_x86.dll 0x401fc500..0x401fc60f.
vmCvar_t server1Cvar;
// Source: uo_ui_mp_x86.dll 0x401f9ec0..0x401f9fcf.
vmCvar_t server2Cvar;
// Source: uo_ui_mp_x86.dll 0x401fb540..0x401fb64f.
vmCvar_t server3Cvar;
// Source: uo_ui_mp_x86.dll 0x401f94a0..0x401f95af.
vmCvar_t server4Cvar;
// Source: uo_ui_mp_x86.dll 0x4023a9e0..0x4023aaef.
vmCvar_t server5Cvar;
// Source: uo_ui_mp_x86.dll 0x4023ae60..0x4023af6f.
vmCvar_t server6Cvar;
// Source: uo_ui_mp_x86.dll 0x401fb0c0..0x401fb1cf.
vmCvar_t server7Cvar;
// Source: uo_ui_mp_x86.dll 0x4023a7a0..0x4023a8af.
vmCvar_t server8Cvar;
// Source: uo_ui_mp_x86.dll 0x401fbd20..0x401fbe2f.
vmCvar_t server9Cvar;
// Source: uo_ui_mp_x86.dll 0x4023a200..0x4023a30f.
vmCvar_t server10Cvar;
// Source: uo_ui_mp_x86.dll 0x4023a320..0x4023a42f.
vmCvar_t server11Cvar;
// Source: uo_ui_mp_x86.dll 0x4023ad40..0x4023ae4f.
vmCvar_t server12Cvar;
// Source: uo_ui_mp_x86.dll 0x401fc740..0x401fc84f.
vmCvar_t server13Cvar;
// Source: uo_ui_mp_x86.dll 0x401fbc00..0x401fbd0f.
vmCvar_t server14Cvar;
// Source: uo_ui_mp_x86.dll 0x401fd160..0x401fd26f.
vmCvar_t server15Cvar;
// Source: uo_ui_mp_x86.dll 0x401fc2c0..0x401fc3cf.
vmCvar_t server16Cvar;
// Source: uo_ui_mp_x86.dll 0x40239d80..0x40239e8f.
vmCvar_t ui_dedicatedCvar;
// Source: uo_ui_mp_x86.dll 0x40239ea0..0x40239faf.
vmCvar_t ui_smallFontCvar;
// Source: uo_ui_mp_x86.dll 0x401fad60..0x401fae6f.
vmCvar_t ui_bigFontCvar;
// Source: uo_ui_mp_x86.dll 0x401fb420..0x401fb52f.
vmCvar_t ui_extraBigFontCvar;
// Source: uo_ui_mp_x86.dll 0x401fd040..0x401fd14f.
vmCvar_t ui_cdkeycheckedCvar;
// Source: uo_ui_mp_x86.dll 0x401fb300..0x401fb40f.
vmCvar_t cg_selectedPlayerCvar;
// Source: uo_ui_mp_x86.dll 0x401fa220..0x401fa32f.
vmCvar_t ui_netSourceCvar;
// Source: uo_ui_mp_x86.dll 0x4023a8c0..0x4023a9cf.
vmCvar_t ui_menuFilesCvar;
// Source: uo_ui_mp_x86.dll 0x401fafa0..0x401fb0af.
vmCvar_t ui_gametypeCvar;
// Source: uo_ui_mp_x86.dll 0x401fcce0..0x401fcdef.
vmCvar_t ui_joinGametypeCvar;
// Source: uo_ui_mp_x86.dll 0x401fcbc0..0x401fcccf.
vmCvar_t ui_netGametypeCvar;
// Source: uo_ui_mp_x86.dll 0x401faa00..0x401fab0f.
vmCvar_t ui_netGametypeNameCvar;
// Source: uo_ui_mp_x86.dll 0x401f9380..0x401f948f.
vmCvar_t ui_newScriptMenuCvar;
// Source: uo_ui_mp_x86.dll 0x401f9da0..0x401f9eaf.
vmCvar_t ui_newScriptMenuIndexCvar;
// Source: uo_ui_mp_x86.dll 0x401fa580..0x401fa68f.
vmCvar_t ui_scriptMenuCvar;
// Source: uo_ui_mp_x86.dll 0x401fae80..0x401faf8f.
vmCvar_t ui_scriptMenuIndexCvar;
// Source: uo_ui_mp_x86.dll 0x401fac40..0x401fad4f.
vmCvar_t ui_scriptMenuAllowResponseCvar;
// Source: uo_ui_mp_x86.dll 0x401fa460..0x401fa56f.
vmCvar_t ui_waitingScriptMenuCvar;
// Source: uo_ui_mp_x86.dll 0x4023b2e0..0x4023b3ef.
vmCvar_t ui_waitingScriptMenuIndexCvar;
// Source: uo_ui_mp_x86.dll 0x40239fc0..0x4023a0cf.
vmCvar_t ui_waitingScriptMenuNoMouseCvar;
// Source: uo_ui_mp_x86.dll 0x401fbf60..0x401fc06f.
vmCvar_t ui_mapIndexCvar;
// Source: uo_ui_mp_x86.dll 0x401fcaa0..0x401fcbaf.
vmCvar_t ui_currentMapCvar;
// Source: uo_ui_mp_x86.dll 0x401fbae0..0x401fbbef.
vmCvar_t ui_currentNetMapCvar;
// Source: uo_ui_mp_x86.dll 0x401fc860..0x401fc96f.
vmCvar_t ui_browserMasterCvar;
// Source: uo_ui_mp_x86.dll 0x401f9a40..0x401f9b4f.
vmCvar_t ui_browserGameTypeCvar;
// Source: uo_ui_mp_x86.dll 0x401fc620..0x401fc72f.
vmCvar_t ui_browserSortKeyCvar;
// Source: uo_ui_mp_x86.dll 0x4023a680..0x4023a78f.
vmCvar_t ui_browserShowFullCvar;
// Source: uo_ui_mp_x86.dll 0x401fcf20..0x401fd02f.
vmCvar_t ui_browserShowEmptyCvar;
// Source: uo_ui_mp_x86.dll 0x4023b0a0..0x4023b1af.
vmCvar_t ui_browserShowPasswordCvar;
// Source: uo_ui_mp_x86.dll 0x401fce00..0x401fcf0f.
vmCvar_t ui_browserShowNoPasswordCvar;
// Source: uo_ui_mp_x86.dll 0x401fb8a0..0x401fb9af.
vmCvar_t ui_browserShowPureCvar;
// Source: uo_ui_mp_x86.dll 0x401fd280..0x401fd38f.
vmCvar_t ui_browserShowDedicatedCvar;
// Source: uo_ui_mp_x86.dll 0x401fc1a0..0x401fc2af.
vmCvar_t ui_browserShowJeepsCvar;
// Source: uo_ui_mp_x86.dll 0x401f9b60..0x401f9c6f.
vmCvar_t ui_browserShowTanksCvar;
// Source: uo_ui_mp_x86.dll 0x4023a560..0x4023a66f.
vmCvar_t ui_browserModCvar;
// Source: uo_ui_mp_x86.dll 0x4023a440..0x4023a54f.
vmCvar_t ui_browserFriendlyfireCvar;
// Source: uo_ui_mp_x86.dll 0x401fbe40..0x401fbf4f.
vmCvar_t ui_browserKillcamCvar;
// Source: uo_ui_mp_x86.dll 0x401f9fe0..0x401fa0ef.
vmCvar_t ui_browserShowPunkBusterCvar;
// Source: uo_ui_mp_x86.dll 0x4023a0e0..0x4023a1ef.
vmCvar_t ui_serverStatusTimeOutCvar;
// Source: uo_ui_mp_x86.dll 0x401fc980..0x401fca8f.
vmCvar_t ui_cmdCvar;
// Source: uo_ui_mp_x86.dll 0x401fb780..0x401fb88f.
vmCvar_t ui_isSpectatorCvar;
// Source: uo_ui_mp_x86.dll 0x4023ab00..0x4023ac0f.
vmCvar_t cg_hudAlphaCvar;
// Source: uo_ui_mp_x86.dll 0x401fa7c0..0x401fa8cf.
vmCvar_t cl_languageWarningsCvar;
// Source: uo_ui_mp_x86.dll 0x401fa6a0..0x401fa7af.
vmCvar_t cl_languageWarningsAsErrorsCvar;

#define UI_CVAR_SLOT_0 g_arenasFileCvar
#define UI_CVAR_SLOT_1 g_allowvoteCvar
#define UI_CVAR_SLOT_2 g_allowVoteMapRestartCvar
#define UI_CVAR_SLOT_3 g_allowVoteMapRotateCvar
#define UI_CVAR_SLOT_4 g_allowVoteTypeMapCvar
#define UI_CVAR_SLOT_5 g_allowVoteMapCvar
#define UI_CVAR_SLOT_6 g_allowVoteGameTypeCvar
#define UI_CVAR_SLOT_7 g_allowVoteKickCvar
#define UI_CVAR_SLOT_8 g_allowVoteClientKickCvar
#define UI_CVAR_SLOT_9 g_allowVoteTempBanUserCvar
#define UI_CVAR_SLOT_10 g_allowVoteTempBanClientCvar
#define UI_CVAR_SLOT_11 cg_brassCvar
#define UI_CVAR_SLOT_12 cg_marksCvar
#define UI_CVAR_SLOT_13 server1Cvar
#define UI_CVAR_SLOT_14 server2Cvar
#define UI_CVAR_SLOT_15 server3Cvar
#define UI_CVAR_SLOT_16 server4Cvar
#define UI_CVAR_SLOT_17 server5Cvar
#define UI_CVAR_SLOT_18 server6Cvar
#define UI_CVAR_SLOT_19 server7Cvar
#define UI_CVAR_SLOT_20 server8Cvar
#define UI_CVAR_SLOT_21 server9Cvar
#define UI_CVAR_SLOT_22 server10Cvar
#define UI_CVAR_SLOT_23 server11Cvar
#define UI_CVAR_SLOT_24 server12Cvar
#define UI_CVAR_SLOT_25 server13Cvar
#define UI_CVAR_SLOT_26 server14Cvar
#define UI_CVAR_SLOT_27 server15Cvar
#define UI_CVAR_SLOT_28 server16Cvar
#define UI_CVAR_SLOT_29 ui_dedicatedCvar
#define UI_CVAR_SLOT_30 ui_smallFontCvar
#define UI_CVAR_SLOT_31 ui_bigFontCvar
#define UI_CVAR_SLOT_32 ui_extraBigFontCvar
#define UI_CVAR_SLOT_33 ui_cdkeycheckedCvar
#define UI_CVAR_SLOT_34 cg_selectedPlayerCvar
#define UI_CVAR_SLOT_35 ui_netSourceCvar
#define UI_CVAR_SLOT_36 ui_menuFilesCvar
#define UI_CVAR_SLOT_37 ui_gametypeCvar
#define UI_CVAR_SLOT_38 ui_joinGametypeCvar
#define UI_CVAR_SLOT_39 ui_netGametypeCvar
#define UI_CVAR_SLOT_40 ui_netGametypeNameCvar
#define UI_CVAR_SLOT_41 ui_newScriptMenuCvar
#define UI_CVAR_SLOT_42 ui_newScriptMenuIndexCvar
#define UI_CVAR_SLOT_43 ui_scriptMenuCvar
#define UI_CVAR_SLOT_44 ui_scriptMenuIndexCvar
#define UI_CVAR_SLOT_45 ui_scriptMenuAllowResponseCvar
#define UI_CVAR_SLOT_46 ui_waitingScriptMenuCvar
#define UI_CVAR_SLOT_47 ui_waitingScriptMenuIndexCvar
#define UI_CVAR_SLOT_48 ui_waitingScriptMenuNoMouseCvar
#define UI_CVAR_SLOT_49 ui_mapIndexCvar
#define UI_CVAR_SLOT_50 ui_currentMapCvar
#define UI_CVAR_SLOT_51 ui_currentNetMapCvar
#define UI_CVAR_SLOT_52 ui_browserMasterCvar
#define UI_CVAR_SLOT_53 ui_browserGameTypeCvar
#define UI_CVAR_SLOT_54 ui_browserSortKeyCvar
#define UI_CVAR_SLOT_55 ui_browserShowFullCvar
#define UI_CVAR_SLOT_56 ui_browserShowEmptyCvar
#define UI_CVAR_SLOT_57 ui_browserShowPasswordCvar
#define UI_CVAR_SLOT_58 ui_browserShowNoPasswordCvar
#define UI_CVAR_SLOT_59 ui_browserShowPureCvar
#define UI_CVAR_SLOT_60 ui_browserShowDedicatedCvar
#define UI_CVAR_SLOT_61 ui_browserShowJeepsCvar
#define UI_CVAR_SLOT_62 ui_browserShowTanksCvar
#define UI_CVAR_SLOT_63 ui_browserModCvar
#define UI_CVAR_SLOT_64 ui_browserFriendlyfireCvar
#define UI_CVAR_SLOT_65 ui_browserKillcamCvar
#define UI_CVAR_SLOT_66 ui_browserShowPunkBusterCvar
#define UI_CVAR_SLOT_67 ui_serverStatusTimeOutCvar
#define UI_CVAR_SLOT_68 ui_cmdCvar
#define UI_CVAR_SLOT_69 ui_isSpectatorCvar
#define UI_CVAR_SLOT_70 cg_hudAlphaCvar
#define UI_CVAR_SLOT_71 cl_languageWarningsCvar
#define UI_CVAR_SLOT_72 cl_languageWarningsAsErrorsCvar

#define UI_CVAR_ENTRY(index_, name_, default_, flags_) {&UI_CVAR_SLOT_##index_, (name_), (default_), (flags_)}

/* Source: uo_ui_mp_x86.dll data 0x4003fe18..0x400402a7.
 * PE_RELOCATION_VALUES_VERIFIED: all 219 storage/name/default pointers resolve
 * to the original 73 records in order; flags and literal strings match. */
cvarTable_t ui_cvarTable[UI_MAX_CVARS] = {UI_CVAR_ENTRY(0, "g_arenasFile", "", CVAR_ROM | CVAR_INIT),
                                          UI_CVAR_ENTRY(1, "g_allowvote", "1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(2, "g_allowVoteMapRestart", "1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(3, "g_allowVoteMapRotate", "1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(4, "g_allowVoteTypeMap", "1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(5, "g_allowVoteMap", "1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(6, "g_allowVoteGameType", "1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(7, "g_allowVoteKick", "0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(8, "g_allowVoteClientKick", "0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(9, "g_allowVoteTempBanUser", "0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(10, "g_allowVoteTempBanClient", "0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(11, "cg_brass", "1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(12, "cg_marks", "1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(13, "server1", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(14, "server2", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(15, "server3", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(16, "server4", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(17, "server5", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(18, "server6", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(19, "server7", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(20, "server8", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(21, "server9", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(22, "server10", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(23, "server11", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(24, "server12", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(25, "server13", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(26, "server14", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(27, "server15", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(28, "server16", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(29, "ui_dedicated", "0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(30, "ui_smallFont", "0.25", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(31, "ui_bigFont", "0.4", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(32, "ui_extraBigFont", "0.55", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(33, "ui_cdkeychecked", "0", CVAR_ROM),
                                          UI_CVAR_ENTRY(34, "cg_selectedPlayer", "0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(35, "ui_netSource", "0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(36, "ui_menuFiles", "ui_mp/menus.txt", CVAR_NONE),
                                          UI_CVAR_ENTRY(37, "ui_gametype", "3", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(38, "ui_joinGametype", "0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(39, "ui_netGametype", "0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(40, "ui_netGametypeName", "", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(41, "ui_newScriptMenu", "", CVAR_ROM),
                                          UI_CVAR_ENTRY(42, "ui_newScriptMenuIndex", "-1", CVAR_ROM),
                                          UI_CVAR_ENTRY(43, "ui_scriptMenu", "", CVAR_ROM),
                                          UI_CVAR_ENTRY(44, "ui_scriptMenuIndex", "-1", CVAR_ROM),
                                          UI_CVAR_ENTRY(45, "ui_scriptMenuAllowResponse", "1", CVAR_ROM),
                                          UI_CVAR_ENTRY(46, "ui_waitingScriptMenu", "", CVAR_ROM),
                                          UI_CVAR_ENTRY(47, "ui_waitingScriptMenuIndex", "-1", CVAR_ROM),
                                          UI_CVAR_ENTRY(48, "ui_waitingScriptMenuNoMouse", "0", CVAR_ROM),
                                          UI_CVAR_ENTRY(49, "ui_mapIndex", "0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(50, "ui_currentMap", "0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(51, "ui_currentNetMap", "0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(52, "ui_browserMaster", "0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(53, "ui_browserGameType", "0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(54, "ui_browserSortKey", "4", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(55, "ui_browserShowFull", "1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(56, "ui_browserShowEmpty", "1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(57, "ui_browserShowPassword", "1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(58, "ui_browserShowNoPassword", "1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(59, "ui_browserShowPure", "1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(60, "ui_browserShowDedicated", "0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(61, "ui_browserShowJeeps", "-1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(62, "ui_browserShowTanks", "-1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(63, "ui_browserMod", "-1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(64, "ui_browserFriendlyfire", "-1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(65, "ui_browserKillcam", "-1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(66, "ui_browserShowPunkBuster", "-1", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(67, "ui_serverStatusTimeOut", "7000", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(68, "ui_cmd", "", CVAR_NONE),
                                          UI_CVAR_ENTRY(69, "ui_isSpectator", "1", CVAR_NONE),
                                          UI_CVAR_ENTRY(70, "cg_hudAlpha", "1.0", CVAR_ARCHIVE),
                                          UI_CVAR_ENTRY(71, "cl_languagewarnings", "0", CVAR_NONE),
                                          UI_CVAR_ENTRY(72, "cl_languagewarningsaserrors", "0", CVAR_NONE)};

// Source: uo_ui_mp_x86.dll data 0x400402a8; value 73.
int32_t ui_cvarCount = UI_MAX_CVARS;

#undef UI_CVAR_ENTRY
// Source: uo_ui_mp_x86.dll 0x4023ac2c.
qboolean ui_menuLoadActive;

/* NOT_FROM_ORIGINAL_SOURCE: retail reaches dllEntry through a freshly mapped
 * PE image. Restore the behavior-bearing load state that UI_Init does not
 * reconstruct so a retained native module behaves like a fresh image on every
 * platform. Scratch payload bytes whose cursors are reset or which are fully
 * overwritten before every read do not need clearing. */
void ui_compat_reset_module_load_state(void)
{
    int32_t index;
    keywordHash_t *itemKeyword;
    menuKeywordHash_t *menuKeyword;

#define UI_ZERO_LOAD_OBJECT(object_) memset(&(object_), 0, sizeof(object_))
#include "../recovered_initializers/ui_module_load_reset.inc"
#undef UI_ZERO_LOAD_OBJECT

    com_parseSessions[0].line = 1;
    com_parseSessions[0].spaceDelimited = qtrue;
    com_parseSessions[0].parseNegativeNumbers = qtrue;
    com_parseSessions[0].savedLine = 1;
    com_parseSession = &com_parseSessions[0];
    ui_cachedServerInfoColumn = -1;
    for (index = 0; index < UI_DOWNLOAD_ESTIMATE_SAMPLES; ++index)
        ui_downloadEstimates[index] = 60;
    for (index = 0; index < UI_MAX_CVARS; ++index)
        memset(ui_cvarTable[index].vmCvar, 0, sizeof(*ui_cvarTable[index].vmCvar));
    ui_cvarCount = UI_MAX_CVARS;
    sharedRandSeed = UINT32_C(0x89abcdef);

    for (itemKeyword = itemParseKeywords;; ++itemKeyword) {
        itemKeyword->next = NULL;
        if (itemKeyword->keyword == NULL)
            break;
    }
    for (menuKeyword = menuParseKeywords;; ++menuKeyword) {
        menuKeyword->next = NULL;
        if (menuKeyword->keyword == NULL)
            break;
    }

    ui_compat_reset_control_binding_state();
    ui_compat_reset_memory_pool_state();
    ui_compat_reset_random_geometry_state();
    ui_compat_reset_temp_runtime_state();
    ui_compat_reset_window_state();
}
