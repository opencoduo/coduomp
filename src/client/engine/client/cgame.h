#ifndef CODUOMP_CLIENT_CGAME_H
#define CODUOMP_CLIENT_CGAME_H

#include <stddef.h>
#include <stdint.h>

#include "../networking/net_channel.h"
#include "../q_shared.h"
#include "../renderer/gl_state.h"
#include "../ui/ui_module_loader.h"
#include "qcommon/client_connection_types.h"
#include "qcommon/cgame_syscall_types.h"
#include "server_browser.h"
#include "qcommon/snapshot_types.h"

struct DObj_s;
struct client_debug_line_s;
struct client_debug_string_s;

enum {
    CODUO_MAP_BSP_NAME_SIZE = 64,
    CODUO_SERVER_NAME_SIZE = 256,
    CODUO_RELIABLE_COMMAND_COUNT = 64,
    CODUO_RELIABLE_COMMAND_CAPACITY = 1024,
    CODUO_USERCMD_BACKUP_COUNT = 128,
    CODUO_SNAPSHOT_BACKUP_COUNT = 32,
    CODUO_PARSE_RING_COUNT = 2048
};

/* Internal client snapshot retained in the 32-entry packet ring. */
typedef struct clSnapshot_s {
    qboolean valid;
    uint32_t snapFlags;
    int32_t serverTime;
    int32_t messageNum;
    int32_t deltaNum;
    int32_t ping;
    uint32_t unused018; /* +0x18: unused by CoDUOMP.exe. */
    playerState_t ps;
    int32_t numEntities;
    int32_t numClients;
    int32_t firstEntitySequence;
    int32_t firstClientSequence;
    int32_t serverCommandSequence;
} clSnapshot_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(clSnapshot_t, ps) == 0x1c,
               "i386 internal snapshot player-state offset changed");
_Static_assert(offsetof(clSnapshot_t, unused018) == 0x18,
               "i386 internal snapshot unused field moved");
_Static_assert(offsetof(clSnapshot_t, numEntities) == 0x4520,
               "i386 internal snapshot entity-count offset changed");
_Static_assert(offsetof(clSnapshot_t, firstEntitySequence) == 0x4528,
               "i386 internal snapshot entity-sequence offset changed");
_Static_assert(offsetof(clSnapshot_t, serverCommandSequence) == 0x4530,
               "i386 internal snapshot command-sequence offset changed");
_Static_assert(sizeof(clSnapshot_t) == 0x4534,
               "i386 internal snapshot size changed");
#endif

typedef struct clOutPacket_s {
    int32_t lastCommandNumber;
    int32_t lastCommandTime;
    int32_t sendRealTime;
} clOutPacket_t;

/* Engine key-button accumulator used by the +command/-command input pairs.
 * Two physical keys may hold one action simultaneously; downtime and
 * accumulatedMsec feed CL_KeyState's per-command fractional result. */
typedef struct clKeyButton_s {
    int32_t downKeys[2];
    int32_t downtime;
    int32_t accumulatedMsec;
    qboolean active;
    qboolean wasPressed;
} clKeyButton_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(clOutPacket_t) == 0x04,
               "i386 outgoing-packet alignment changed");
_Static_assert(offsetof(clOutPacket_t, lastCommandNumber) == 0x00,
               "i386 outgoing-packet command-number offset changed");
_Static_assert(sizeof(((clOutPacket_t *)0)->lastCommandNumber) == 0x04,
               "i386 outgoing-packet command-number extent changed");
_Static_assert(offsetof(clOutPacket_t, lastCommandTime) == 0x04,
               "i386 outgoing-packet server-time offset changed");
_Static_assert(sizeof(((clOutPacket_t *)0)->lastCommandTime) == 0x04,
               "i386 outgoing-packet server-time extent changed");
_Static_assert(offsetof(clOutPacket_t, sendRealTime) == 0x08,
               "i386 outgoing-packet realtime offset changed");
_Static_assert(sizeof(((clOutPacket_t *)0)->sendRealTime) == 0x04,
               "i386 outgoing-packet realtime extent changed");
_Static_assert(sizeof(clOutPacket_t) == 0x0c,
               "original i386 outgoing-packet size changed");

_Static_assert(_Alignof(clKeyButton_t) == 4,
               "original i386 client key-button alignment");
_Static_assert(offsetof(clKeyButton_t, downKeys) == 0x00,
               "original i386 client key-button keys offset");
_Static_assert(offsetof(clKeyButton_t, downtime) == 0x08,
               "original i386 client key-button downtime offset");
_Static_assert(offsetof(clKeyButton_t, accumulatedMsec) == 0x0c,
               "original i386 client key-button accumulated-time offset");
_Static_assert(offsetof(clKeyButton_t, active) == 0x10,
               "original i386 client key-button active offset");
_Static_assert(offsetof(clKeyButton_t, wasPressed) == 0x14,
               "original i386 client key-button press-latch offset");
_Static_assert(sizeof(clKeyButton_t) == 0x18,
               "original i386 client key-button extent");
#endif

/* Engine input state followed by cgame-controlled command/mouse values. */
typedef struct clientCgameInputState_s {
    int32_t mouseDx[2];                 /* +0x00: double-buffered mouse X */
    int32_t mouseDy[2];                 /* +0x08: double-buffered mouse Y */
    int32_t mouseIndex;                 /* +0x10: active mouse accumulator */
    int32_t joystickAxis[6];            /* +0x14: axes 3..5 are unused by CoDUOMP.exe. */
    int32_t userCmdValue;               /* +0x2c: cgame syscall 85 */
    int32_t shellshockScreenBlur;       /* +0x30: cgame syscall 87 */
    int32_t flameDamage;                /* +0x34: cgame syscall 88 */
    float userCmdSensitivityScale;      /* +0x38: cgame syscall 85 */
    float shellshockMouseMaxPitchSpeed; /* +0x3c: cgame syscall 246 */
    float shellshockMouseMaxYawSpeed;   /* +0x40: cgame syscall 246 */
    vec3_t clientLerpOrigin;            /* +0x44: cgame syscall 212 */
    vec3_t userCmdAimValues;            /* +0x50: cgame syscall 86 */
    vec3_t viewAngles;                  /* +0x5c: set by cgame syscall 247 */
} clientCgameInputState_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(clientCgameInputState_t) == 104,
               "client cgame-input state size changed");
_Static_assert(offsetof(clientCgameInputState_t, userCmdValue) == 44,
               "cgame user-command value offset changed");
_Static_assert(offsetof(clientCgameInputState_t,
                        userCmdSensitivityScale) == 56,
               "cgame user-command scale offset changed");
_Static_assert(offsetof(clientCgameInputState_t,
                        shellshockMouseMaxPitchSpeed) == 60,
               "shellshock mouse-pitch limit offset changed");
_Static_assert(offsetof(clientCgameInputState_t,
                        shellshockMouseMaxYawSpeed) == 64,
               "shellshock mouse-yaw limit offset changed");
_Static_assert(offsetof(clientCgameInputState_t, clientLerpOrigin) == 68,
               "cgame client-lerp origin offset changed");
_Static_assert(offsetof(clientCgameInputState_t, userCmdAimValues) == 80,
               "cgame aim-value offset changed");
_Static_assert(
    offsetof(clientCgameInputState_t, viewAngles) == 92,
    "client view-angle offset changed");
#endif

/* Complete pointer-free clientActive_t storage cleared by CL_ClearState.
 * The cgame-owned input fields are typed above; its reserved spans remain live
 * client-input state whose consumers have not yet supplied field names. */
typedef struct clientActive_s {
    int32_t timeoutCount;
    clSnapshot_t snap;
    int32_t serverTime;
    int32_t oldServerTime;
    int32_t oldFrameServerTime;
    int32_t serverTimeDelta;
    int32_t previousSnapshotServerTime;
    qboolean extrapolatedSnapshot;
    qboolean newSnapshots;
    gameState_t gameState;
    char mapBspName[CODUO_MAP_BSP_NAME_SIZE];
    int32_t parseEntitySequence;
    int32_t parseClientSequence;
    clientCgameInputState_t inputState;
    int32_t serverId;
    vec4_t teamColorAllies;
    vec4_t teamColorAxis;
    usercmd_t cmds[CODUO_USERCMD_BACKUP_COUNT];
    int32_t cmdNumber;
    clOutPacket_t outPackets[CODUO_SNAPSHOT_BACKUP_COUNT];
    clSnapshot_t snapshots[CODUO_SNAPSHOT_BACKUP_COUNT];
    entityState_t entityBaselines[MAX_GENTITIES];
    entityState_t parseEntities[CODUO_PARSE_RING_COUNT];
    clientState_t parseClients[CODUO_PARSE_RING_COUNT];
    /* Original +0x17ba30..+0x17bb33; unused by CoDUOMP.exe except as part of
     * the complete clientActive_t clear extent. */
    uint8_t unused17BA30[260];
} clientActive_t;

typedef struct clientStaticDownload_s {
    char downloadName[256];
    char downloadTempName[256];
    char originalDownloadName[MAX_QPATH];
    qboolean downloadRestart;
} clientStaticDownload_t;

/* Complete clientConnection_t state cleared by CL_Disconnect. The last native
 * pointer was four bytes in the original i386 build and widens normally with
 * the rest of the host-side connection ABI. */
typedef struct clientConnection_s {
    int32_t clientNum;
    int32_t lastPacketSentTime;
    int32_t lastPacketTime;
    netadr_t serverAddress;
    int32_t connectTime;
    int32_t connectPacketCount;
    char serverMessage[256];
    int32_t challenge;
    int32_t checksumFeed;
    qboolean onlyVisibleClients;
    int32_t reliableSequence;
    int32_t reliableAcknowledge;
    char reliableCommands[CODUO_RELIABLE_COMMAND_COUNT]
                         [CODUO_RELIABLE_COMMAND_CAPACITY];
    int32_t serverMessageSequence;
    int32_t serverCommandSequence;
    int32_t lastExecutedServerCommand;
    char serverCommands[CODUO_RELIABLE_COMMAND_COUNT]
                       [CODUO_RELIABLE_COMMAND_CAPACITY];
    int32_t downloadFile;
    /* Original +0x02014c; unused by CoDUOMP.exe except as part of the complete
     * clientConnection_t clear extent. */
    int32_t unused2014C;
    int32_t downloadBlock;
    int32_t downloadCount;
    int32_t downloadSize;
    int32_t downloadFlags;
    char downloadList[MAX_STRING_CHARS];
    qboolean wwwDownloadActive;
    qboolean wwwDownloadAborting;
    char redirectedList[MAX_STRING_CHARS];
    char badChecksumList[MAX_STRING_CHARS];
    char demoName[MAX_QPATH];
    qboolean demoRecording;
    qboolean demoPlayback;
    qboolean demoWaiting;
    qboolean demoFirstFrameSkipped;
    int32_t demoFile;
    int32_t timeDemoLogFile;
    int32_t timeDemoFrameCount;
    uint32_t timeDemoStartTime;
    uint32_t timeDemoPreviousFrameTime;
    int32_t timeDemoBaseTime;
    int32_t voiceChatTime;
    int32_t voiceChatRepeatCount;
    char voiceChatText[32];
    netchan_t netchan;
    netProfileInfo_t *netProfile;
} clientConnection_t;

#if UINTPTR_MAX == UINT32_MAX
/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): every field following the
 * embedded gameState_t moves by the selected client/cgame pool extension.
 * The stock source selects zero and therefore retains every retail offset. */
enum {
    CLIENT_ACTIVE_GAMESTATE_EXTENSION_BYTES =
        MAX_GAMESTATE_CHARS - MAX_GAMESTATE_CHARS_RETAIL
};

_Static_assert(offsetof(clientActive_t, snap) == 0x000004,
               "i386 client-active current snapshot moved");
_Static_assert(offsetof(clientActive_t, serverTime) == 0x004538,
               "i386 client-active server time moved");
_Static_assert(offsetof(clientActive_t, gameState) == 0x004554,
               "i386 client-active game state moved");
_Static_assert(offsetof(clientActive_t, parseEntitySequence) ==
                   0x00b598 + CLIENT_ACTIVE_GAMESTATE_EXTENSION_BYTES,
               "i386 client-active parse sequence moved");
_Static_assert(offsetof(clientActive_t, inputState) ==
                   0x00b5a0 + CLIENT_ACTIVE_GAMESTATE_EXTENSION_BYTES,
               "i386 cgame input-state offset changed");
_Static_assert(offsetof(clientActive_t, serverId) ==
                   0x00b608 + CLIENT_ACTIVE_GAMESTATE_EXTENSION_BYTES,
               "i386 client server-id offset changed");
_Static_assert(offsetof(clientActive_t, teamColorAllies) ==
                   0x00b60c + CLIENT_ACTIVE_GAMESTATE_EXTENSION_BYTES,
               "i386 client-active team colors moved");
_Static_assert(offsetof(clientActive_t, cmds) ==
                   0x00b62c + CLIENT_ACTIVE_GAMESTATE_EXTENSION_BYTES,
               "i386 client-active user-command ring moved");
_Static_assert(offsetof(clientActive_t, snapshots) ==
                   0x00c3b0 + CLIENT_ACTIVE_GAMESTATE_EXTENSION_BYTES,
               "i386 client-active snapshot ring moved");
_Static_assert(offsetof(clientActive_t, entityBaselines) ==
                   0x096a30 + CLIENT_ACTIVE_GAMESTATE_EXTENSION_BYTES,
               "i386 client-active entity baselines moved");
_Static_assert(offsetof(clientActive_t, parseEntities) ==
                   0x0d3a30 + CLIENT_ACTIVE_GAMESTATE_EXTENSION_BYTES,
               "i386 client-active parsed entities moved");
_Static_assert(offsetof(clientActive_t, parseClients) ==
                   0x14da30 + CLIENT_ACTIVE_GAMESTATE_EXTENSION_BYTES,
               "i386 client-active parsed clients moved");
_Static_assert(offsetof(clientActive_t, unused17BA30) ==
                   0x17ba30 + CLIENT_ACTIVE_GAMESTATE_EXTENSION_BYTES,
               "i386 client-active unused tail moved");
_Static_assert(sizeof(clientActive_t) ==
                   0x17bb34 + CLIENT_ACTIVE_GAMESTATE_EXTENSION_BYTES,
               "i386 client-active clear extent changed");
_Static_assert(offsetof(clientStaticDownload_t, downloadTempName) == 256,
               "i386 static-download temporary name moved");
_Static_assert(offsetof(clientStaticDownload_t, originalDownloadName) == 512,
               "i386 static-download original name moved");
_Static_assert(offsetof(clientStaticDownload_t, downloadRestart) == 576,
               "i386 static-download restart flag moved");
_Static_assert(sizeof(clientStaticDownload_t) == 580,
               "i386 static-download state size changed");
_Static_assert(offsetof(clientConnection_t, reliableSequence) == 0x000134,
               "i386 client-connection reliable sequence moved");
_Static_assert(offsetof(clientConnection_t, serverMessageSequence) ==
                   0x01013c,
               "i386 client-connection server-message sequence moved");
_Static_assert(offsetof(clientConnection_t, serverCommands) == 0x010148,
               "i386 client-connection server-command ring moved");
_Static_assert(offsetof(clientConnection_t, downloadFile) == 0x020148,
               "i386 client-connection download state moved");
_Static_assert(offsetof(clientConnection_t, unused2014C) == 0x02014c,
               "i386 client-connection unused download slot moved");
_Static_assert(offsetof(clientConnection_t, redirectedList) == 0x020568,
               "i386 client redirect history moved");
_Static_assert(offsetof(clientConnection_t, badChecksumList) == 0x020968,
               "i386 client bad-checksum history moved");
_Static_assert(offsetof(clientConnection_t, demoName) == 0x020d68,
               "i386 client-connection demo name moved");
_Static_assert(offsetof(clientConnection_t, voiceChatTime) == 0x020dd0,
               "i386 client-connection voice-chat state moved");
_Static_assert(offsetof(clientConnection_t, netchan) == 0x020df8,
               "i386 client-connection net channel moved");
_Static_assert(offsetof(clientConnection_t, netProfile) == 0x030e38,
               "i386 client-connection profile pointer moved");
_Static_assert(sizeof(clientConnection_t) == 0x030e3c,
               "i386 client-connection clear extent changed");
#endif

enum {
    CL_NETWORK_PROTOCOL_VERSION = 22
};

/* Complete client-static state cleared by CL_Shutdown. The original i386
 * object occupies 0x04ad3d60..0x04dc464b. Pointer-bearing debug allocation
 * fields widen naturally on modern hosts; the i386 assertions below retain
 * the proven executable layout. */
typedef struct clientStatic_s {
    connstate_t state;
    int32_t keyCatchers;
    qboolean cdDialogRequested;
    char serverName[CODUO_SERVER_NAME_SIZE];
    qboolean rendererStarted;
    qboolean soundStarted;
    qboolean uiStarted;
    int32_t frameCount;
    int32_t frameTime;
    int32_t realtime;
    int32_t realTime;
    int32_t realFrametime;
    int32_t logoStartTime;
    int32_t logoTotalDuration;
    int32_t logoFadeInDuration;
    int32_t logoFadeOutDuration;
    int32_t logoShaderTop;
    int32_t logoShaderBottom;
    int32_t numLocalServers;
    lan_server_info_t localServers[LAN_LOCAL_SERVER_CAPACITY];
    qboolean waitingForMasterResponse;
    int32_t numGlobalServers;
    lan_server_info_t globalServers[LAN_GLOBAL_SERVER_CAPACITY];
    int32_t numFavoriteServers;
    lan_server_info_t favoriteServers[LAN_FAVORITE_SERVER_CAPACITY];
    lan_server_source_t pingUpdateSource;
    int32_t numGlobalServerAddresses;
    netadr_t updateServer;
    char updateChallenge[MAX_STRING_CHARS];
    char updateInfoString[MAX_STRING_CHARS];
    netadr_t cdAuthorizeAddress;
    char autoUpdateServerNames[5][64];
    netadr_t autoUpdateServer;
    glconfig_t rendererConfig;
    int32_t whiteShader;
    int32_t consoleShader;
    int32_t debugStringCapacity;
    int32_t debugStringCount;
    struct client_debug_string_s *debugStrings;
    uint8_t *debugStringFromServer;
    int32_t debugLineCapacity;
    int32_t debugLineCount;
    struct client_debug_line_s *debugLines;
    uint8_t *debugLineFromServer;
    int32_t *debugLineDurations;
    qboolean wwwDownloadDisconnected;
    clientStaticDownload_t staticDownload;
} clientStatic_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(clientStatic_t, keyCatchers) == 0x04,
               "i386 client-static key-catcher offset changed");
_Static_assert(offsetof(clientStatic_t, cdDialogRequested) == 0x08,
               "i386 client-static CD-dialog flag moved");
_Static_assert(offsetof(clientStatic_t, serverName) == 0x0c,
               "i386 client-static server-name offset changed");
_Static_assert(offsetof(clientStatic_t, rendererStarted) == 0x10c,
               "i386 client-static renderer flag moved");
_Static_assert(offsetof(clientStatic_t, frameCount) == 0x118,
               "i386 client-static frame count moved");
_Static_assert(offsetof(clientStatic_t, realtime) == 0x120,
               "i386 client-static scaled time moved");
_Static_assert(offsetof(clientStatic_t, logoStartTime) == 0x12c,
               "i386 client-static logo state moved");
_Static_assert(offsetof(clientStatic_t, localServers) == 0x148,
               "i386 client-static local-server list moved");
_Static_assert(offsetof(clientStatic_t, globalServers) == 0x4d50,
               "i386 client-static global-server list moved");
_Static_assert(offsetof(clientStatic_t, favoriteServers) == 0x2eb054,
               "i386 client-static favorite-server list moved");
_Static_assert(offsetof(clientStatic_t, updateServer) == 0x2efc5c,
               "i386 client-static update server moved");
_Static_assert(offsetof(clientStatic_t, updateInfoString) == 0x2f0070,
               "i386 client-static update info moved");
_Static_assert(offsetof(clientStatic_t, cdAuthorizeAddress) == 0x2f0470,
               "i386 client-static authorize server moved");
_Static_assert(offsetof(clientStatic_t, autoUpdateServer) == 0x2f05c4,
               "i386 client-static auto-update server moved");
_Static_assert(offsetof(clientStatic_t, rendererConfig) == 0x2f05d8,
               "i386 client-static renderer config moved");
_Static_assert(offsetof(clientStatic_t, debugStringCapacity) == 0x2f0680,
               "i386 client-static debug state moved");
_Static_assert(offsetof(clientStatic_t, wwwDownloadDisconnected) == 0x2f06a4,
               "i386 client-static disconnected WWW-download flag moved");
_Static_assert(offsetof(clientStatic_t, staticDownload) == 0x2f06a8,
               "i386 client-static download state moved");
_Static_assert(sizeof(clientStatic_t) == 0x2f08ec,
               "i386 client-static clear extent changed");
#endif

extern clientActive_t cl;
extern clientConnection_t clc;
extern clientStatic_t cls;
extern qboolean cl_connectedToPureServer;
extern cvar_t *cl_activeAction;
extern cvar_t *cl_freezeDemo;
extern cvar_t *cl_showTimeDelta;
extern cvar_t *cl_showServerCommands;
extern cvar_t *cl_timeNudge;
extern cvar_t *cl_timedemo;
extern cvar_t *cl_paused;
extern cvar_t *sv_paused;
extern cvar_t *cl_autocmd;
extern cvar_t *cl_timeout;
extern cvar_t *cl_shownuments;
extern cvar_t *cl_visibleClients;
extern cvar_t *cl_showSend;
extern cvar_t *rcon_client_password;
extern cvar_t *cl_avidemo;
extern cvar_t *cl_forceavidemo;
extern cvar_t *rcon_client_address;
extern cvar_t *cl_yawspeed;
extern cvar_t *cl_pitchspeed;
extern cvar_t *cl_anglespeedkey;
extern cvar_t *cl_maxpackets;
extern cvar_t *cl_packetdup;
extern cvar_t *cl_run;
extern cvar_t *cl_stance;
extern cvar_t *cl_stanceTemp;
extern cvar_t *cl_goStandJumpTime;
extern cvar_t *sensitivity;
extern cvar_t *cl_mouseAccel;
extern cvar_t *cl_freelook;
extern cvar_t *cl_showmouserate;
extern cvar_t *cl_allowDownload;
extern cvar_t *cl_serverAllowDownload;
extern cvar_t *cl_wwwDownload;
extern cvar_t *cl_conXOffset;
extern cvar_t *cl_viewPitchCompensate;
extern cvar_t *cl_viewYawCompensate;
extern cvar_t *cl_bypassMouseInput;
extern cvar_t *m_pitch;
extern cvar_t *m_yaw;
extern cvar_t *m_forward;
extern cvar_t *m_side;
extern cvar_t *m_filter;
extern cvar_t *cl_motdString;
extern cvar_t *cl_ingame;
extern cvar_t *cl_waitForFire;
extern cvar_t *cl_updateAvailable;
extern cvar_t *cl_updateFiles;
extern cvar_t *cl_updateOldVersion;
extern cvar_t *cl_updateVersion;
extern cvar_t *cl_serverLoadMap;
extern cvar_t *cl_serverLoadGameType;
extern cvar_t *cl_serverLoadWaiting;
extern cvar_t *cg_announcerSounds;
extern cvar_t *cl_executeString;
extern cvar_t *cl_nodelta;
extern cvar_t *cl_debugMove;
extern uint32_t cl_commandFrameMsec;
extern clKeyButton_t in_left;
extern clKeyButton_t in_right;
extern clKeyButton_t in_forward;
extern clKeyButton_t in_back;
extern clKeyButton_t in_lookup;
extern clKeyButton_t in_lookdown;
extern clKeyButton_t in_moveleft;
extern clKeyButton_t in_moveright;
extern clKeyButton_t in_strafe;
extern clKeyButton_t in_speed;
extern clKeyButton_t in_up;
extern clKeyButton_t in_down;
extern clKeyButton_t in_stanceUp;
extern clKeyButton_t in_attack;
extern clKeyButton_t in_commandButton1;
extern clKeyButton_t in_dropWeapon;
extern clKeyButton_t in_sprint;
extern clKeyButton_t in_commandButton4;
extern clKeyButton_t in_melee;
extern clKeyButton_t in_activate;
extern clKeyButton_t in_commandButton7;
extern clKeyButton_t in_commandButton8;
extern clKeyButton_t in_commandButton9;
extern clKeyButton_t in_reload;
extern clKeyButton_t in_leanLeft;
extern clKeyButton_t in_leanRight;
extern clKeyButton_t in_prone;
extern qboolean in_mlooking;
extern cvar_t *scr_timegraph;
extern cvar_t *scr_debuggraph;
extern cvar_t *scr_graphheight;
extern cvar_t *scr_graphscale;
extern cvar_t *scr_graphshift;
extern int32_t cl_entityLastVisibleTime[MAX_CLIENTS];
extern char cl_updateFileName[MAX_QPATH];

intptr_t CL_CgameSystemCalls(intptr_t *arguments);
void CL_SetUserCmdValue(int32_t value, float sensitivityScale);
void CL_SetUserCmdAimValues(const vec3_t aimValues);
void CL_SetUserCmdInShellshock(int32_t shellshockScreenBlur);
void CL_SetUserCmdFlameDamage(int32_t flameDamage);
void CL_SetClientLerpOrigin(float x, float y, float z);
void CL_AddCgameCommand(const char *commandName);
void CL_InitCGame(void);
qboolean CL_GameCommand(void);
void CL_CGameRendering(int32_t stereoView, qboolean drawFrame);
void CL_UpdateColor(void);
void CL_SetExpectedHunkUsage(const char *mapBspPath);
void CL_CM_LoadMap(const char *mapBspPath);
void CL_SubtitlePrint(const char *reference, int32_t timeMs,
                      int32_t lineWidth);
surfaceType_t CL_SurfaceTypeFromName(const char *name);
const char *CL_SurfaceTypeToName(int32_t surfaceType);
void CL_DObjInvalidateSkels(void);
qboolean CL_DObjCreateSkelForBone(
    struct DObj_s *obj, int32_t boneIndex);
qboolean CL_DObjCreateSkelForBones(
    struct DObj_s *obj, const uint32_t *partBits);
void CL_UpdateColorInternal(const char *cvarName, vec4_t color);
void CL_LookupColor(uint8_t colorCode, vec4_t color);
void CL_DrawString(int32_t x, int32_t y, const char *text,
                   int32_t mode, int32_t charWidth,
                   int32_t charHeight, int32_t textStyle);
int32_t CL_SaveCgameState(int32_t bufferSize, uint8_t *buffer);
int32_t CL_RestoreCgameState(int32_t bufferSize, uint8_t *buffer);
void CL_GetGameState(gameState_t *gameState);
const char *ConcatArgs(int32_t start);
void CL_Vsay_f(void);
void CL_PlayVoiceChat(void);
void CL_ClearState(void);
void CL_ClearStaticDownload(void);
char *CL_TimeDemoLogBaseName(void);
void CL_UpdateTimeDemo(void);
void CL_FirstSnapshot(void);
void CL_GetCurrentSnapshotNumber(int32_t *snapshotNumber,
                                 int32_t *serverTime);
void CL_Disconnect(qboolean showMainMenu);
void coduomp_client_complete_server_mod_teardown(void);
void CL_Disconnect_f(void);
void CL_Init(void);
void CL_Shutdown(void);
void CL_ShutdownAll(void);
void CL_SetupForNewServerMap(void);
void CL_ShutdownUI(void);
void CL_ShutdownCGame(void);
void CL_ShutdownRef(void);
void CL_ShutdownDebugData(void);
void CL_ShutdownKeyCommands(void);
void CL_InitKeyCommands(void);
void CL_AdjustTimeDelta(void);
void CL_SetCGameTime(void);
void CL_ReadDemoMessage(void);
void CL_UpdateLevelHunkUsage(void);
void CL_CheckForResend(void);
void SCR_UpdateScreen(void);
void SCR_Init(void);
/* NOT_FROM_ORIGINAL_SOURCE: forget a widescreen backdrop pre-queued into a
 * renderer command buffer that a renderer restart has discarded. */
void coduomp_scr_reset_widescreen_backdrop_compat(void);
void CL_CheckAutoUpdate(void);
void CL_GetAutoUpdate(void);
void CL_AddReliableCommand(const char *command);
void CL_MakeMonkeyDoLaundry(void);
void CL_ChangeReliableCommand(void);
void CL_CDDialog(void);
void CL_SendPureChecksums(void);
void CL_ResetPureClientAtServer(void);
void CL_Snd_Restart_f(void);
void CL_Vid_Restart_f(void);
void CL_InitRef(void);
void CL_RefPrintf(int32_t printLevel, const char *format, ...);
int32_t CG_GetGameModel(int16_t modelIndex);
void CG_DObjCalcPose(void *owner, struct DObj_s *obj,
                     uint32_t *partBits);
void CL_DObjCalcAnim(struct DObj_s *obj, const uint32_t *partBits);
void CL_DObjCalcSkel(struct DObj_s *obj, const uint32_t *partBits);
fontInfo_t *CL_GetFontInfo(int32_t fontHandle, float scale);
void CL_startSingleplayer_f(void);
void set_cl_punkbuster(const char *value);
void CL_DrawLogo(void);
void CL_StopLogo(void);
void CL_PlayLogo_f(void);
void CL_ForwardToServer_f(void);
void CL_Configstrings_f(void);
void CL_Clientinfo_f(void);
void CL_PlayDemo_f(void);
void CL_Setenv_f(void);
void CL_Reconnect_f(void);
void CL_Connect_f(void);
void CL_Rcon_f(void);
void CL_OpenedPK3List_f(void);
void CL_ReferencedPK3List_f(void);
void CL_ShowIP_f(void);
void CL_SetRecommended_f(void);
void CL_UpdateScreen_f(void);
void CL_CubemapShotUsage(void);
void CL_CubemapShot_f(void);
void Com_WriteLocalizedSoundAliasFiles(void);
void CL_InitRenderer(void);
void CL_StartHunkUsers(void);
float CL_KeyState(clKeyButton_t *button);
void CL_AdjustAngles(void);
void CL_CmdButtons(usercmd_t *command);
void CL_KeyMove(usercmd_t *command);
qboolean CL_IsInMatchTimeout(void);
void CL_MouseMove(usercmd_t *command);
void CL_JoystickMove(usercmd_t *command);
void CL_FinishMove(usercmd_t *command);
usercmd_t CL_CreateCmd(void);
void CL_CreateNewCommands(void);
qboolean CL_ReadyToSendPacket(void);
void CL_WritePacket(void);
void CL_SendCmd(void);
void CL_ForwardCommandToServer(const char *command);
void CL_MouseEvent(int32_t deltaX, int32_t deltaY);
void CL_JoystickEvent(int32_t axis, int32_t value);
void CL_PacketEvent(netadr_t from, msg_t *message, int32_t time);
void CL_Netchan_Encode(uint8_t *data, int32_t length);
void CL_Netchan_Decode(uint8_t *data, int32_t length);
void CL_Netchan_TransmitNextFragment(netchan_t *channel);
void CL_Netchan_Transmit(netchan_t *channel, uint8_t *data,
                         int32_t length);
void CL_Netchan_AddOOBProfilePacket(int32_t length);
void CL_Netchan_UpdateProfileStats(void);
qboolean CL_CDKeyValidate(const char *key, const char *checksum);
void CL_RequestAuthorization(void);
extern qboolean cl_frameRunning;      /* original 0x0389fcf0 */
void CL_WriteDemoMessage(const msg_t *message, int32_t headerBytes);
void CL_StopRecord_f(void);
void CL_DemoFilename(int32_t number, char *fileName);
void CL_Record_f(void);
void CL_DemoCompleted(void);
void CL_StartDemoLoop(void);
void CL_NextDemo(void);
void CL_ReadDemoMessage(void);
qboolean isEntVisible(const entityState_t *entity);
void SHOWNET(const msg_t *message, const char *label);
void CL_DeltaEntity(msg_t *message, clSnapshot_t *frame,
                    int32_t newNumber, const entityState_t *oldEntity,
                    qboolean unchanged);
void CL_DeltaClient(msg_t *message, clSnapshot_t *frame,
                    int32_t newNumber, const clientState_t *oldClient,
                    qboolean unchanged);
void CL_ParsePacketEntities(msg_t *message,
                            const clSnapshot_t *oldFrame,
                            clSnapshot_t *newFrame);
void CL_ParsePacketClients(msg_t *message,
                           const clSnapshot_t *oldFrame,
                           clSnapshot_t *newFrame);
void CL_ParseCommandString(msg_t *message);
void CL_ParseDownload(msg_t *message);
void CL_ParseGamestate(msg_t *message);
void CL_ParseSnapshot(msg_t *message);
void CL_ParseServerMessage(msg_t *message);
void CL_BeginDownload(const char *localName, const char *remoteName);
void CL_NextDownload(void);
void CL_DownloadsComplete(void);
void CL_InitDownloads(void);
void CL_CheckForResend(void);
void CL_CheckTimeout(void);
void CL_CheckUserinfo(void);
void CL_UpdateInGameState(void);
void CL_WWWDownload(void);
void CL_Frame(int32_t msec, int32_t realMsec);
int32_t CL_ScaledMilliseconds(void);
qboolean CL_GetServerCommand(int32_t serverCommandNumber);
int32_t CL_GetCurrentCmdNumber(void);
qboolean CL_GetUserCmd(int32_t commandNumber, usercmd_t *command);
qboolean CL_GetSnapshot(int32_t snapshotNumber, snapshot_t *snapshot);
void CL_ConfigstringModified(void);
void CL_SystemInfoChanged(void);
void SCR_AdjustTo640(float *x, float *y, float *width, float *height);
void SCR_AdjustFrom640(float *x, float *y, float *width, float *height);
void SCR_DrawPic(float x, float y, float width, float height,
                 int32_t shaderHandle);
void SCR_DrawNamedPic(float x, float y, float width, float height,
                      const char *name, int32_t shaderUsage);
void SCR_FillRect(float x, float y, float width, float height,
                  const vec4_t color);
void SCR_DrawSmallChar(int32_t x, int32_t y, int32_t character);
void SCR_DrawSmallStringExt(int32_t x, int32_t y, const char *text,
                            const vec4_t color);
void SCR_DrawConsoleString(int32_t x, int32_t y,
                           const uint16_t *encodedText,
                           int32_t encodedCount, const vec4_t color);
void SCR_DrawDemoRecording(void);
void SCR_DrawDebugGraph(void);
void SCR_DebugGraph(float value, int32_t color);

void IN_CenterView(void);
void IN_UpDown(void);
void IN_UpUp(void);
void IN_DownDown(void);
void IN_DownUp(void);
void IN_LeftDown(void);
void IN_LeftUp(void);
void IN_RightDown(void);
void IN_RightUp(void);
void IN_ForwardDown(void);
void IN_ForwardUp(void);
void IN_BackDown(void);
void IN_BackUp(void);
void IN_LookupDown(void);
void IN_LookupUp(void);
void IN_LookdownDown(void);
void IN_LookdownUp(void);
void IN_StrafeDown(void);
void IN_StrafeUp(void);
void IN_MoveleftDown(void);
void IN_MoveleftUp(void);
void IN_MoverightDown(void);
void IN_MoverightUp(void);
void IN_SpeedDown(void);
void IN_SpeedUp(void);
void IN_Button0Down(void);
void IN_Button0Up(void);
void IN_Button5Down(void);
void IN_Button5Up(void);
void IN_ActivateDown(void);
void IN_ActivateUp(void);
void IN_ReloadDown(void);
void IN_ReloadUp(void);
void IN_LeanLeftDown(void);
void IN_LeanLeftUp(void);
void IN_LeanRightDown(void);
void IN_LeanRightUp(void);
void IN_MP_DropWeaponDown(void);
void IN_MP_DropWeaponUp(void);
void IN_Wbutton6Down(void);
void IN_Wbutton6Up(void);
void IN_SprintDown(void);
void IN_SprintUp(void);
void IN_MLookDown(void);
void IN_MLookUp(void);
void IN_LowerStance(void);
void IN_RaiseStance(void);
void IN_ToggleCrouch(void);
void IN_ToggleProne(void);
void IN_GoProne(void);
void IN_GoCrouch(void);
void IN_GoStandDown(void);
void IN_GoStandUp(void);

#endif
