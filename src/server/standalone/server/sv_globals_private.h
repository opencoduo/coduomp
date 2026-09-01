#ifndef CODUO_SV_GLOBALS_PRIVATE_H
#define CODUO_SV_GLOBALS_PRIVATE_H

#include <stdint.h>

#include "server/standalone/bindings/coduo_engine_structs.h"
#include "server/engine/server_game_data.h"
#include "server/engine/server_snapshot_send.h"

extern serverHeader_t sv;
extern serverStatic_t svs;
extern vm_t *sv_gameVM;
extern char *sv_configstrings[MAX_CONFIGSTRINGS];
extern char *sv_entityParsePoint;
extern char sv_gametypeNormalizeBuffer[MAX_QPATH];
extern cvar_t *sv_maxclients;
extern cvar_t *cl_paused;
extern cvar_t *com_speeds;
extern cvar_t *dedicated;
extern cvar_t *g_gametype;
extern cvar_t *sv_mapname;
extern cvar_t *rconPassword;
extern cvar_t *scr_allow_jeeps;
extern cvar_t *scr_allow_tanks;
extern cvar_t *sv_allowAnonymous;
extern cvar_t *sv_allowDownload;
extern cvar_t *sv_disableClientConsole;
extern cvar_t *sv_floodProtect;
extern cvar_t *sv_hostname;
extern cvar_t *sv_kickBanTime;
extern cvar_t *sv_killserver;
extern cvar_t *sv_mapRotation;
extern cvar_t *sv_mapRotationCurrent;
extern cvar_t *sv_maxPing;
extern cvar_t *sv_maxRate;
extern cvar_t *sv_minPing;
extern cvar_t *sv_onlyVisibleClients;
extern cvar_t *sv_packet_info;
extern cvar_t *sv_paused;
extern cvar_t *sv_padPackets;
extern cvar_t *sv_privateClients;
extern cvar_t *sv_privatePassword;
extern cvar_t *sv_punkbuster;
extern cvar_t *sv_pure;
extern cvar_t *sv_reconnectlimit;
extern cvar_t *sv_running;
extern cvar_t *sv_showAverageBPS;
extern cvar_t *sv_showCommands;
extern cvar_t *sv_timeout;
extern cvar_t *sv_wwwBaseURL;
extern cvar_t *sv_wwwDlDisconnected;
extern cvar_t *sv_wwwDownload;
extern cvar_t *sv_zombietime;
extern cvar_t *sv_serverid;
extern cvar_t *sv_showloss;
extern cvar_t *sv_fps;
extern int32_t com_errorEntered;
extern int32_t sv_serverId;
extern qboolean sv_frameRunning;
extern int32_t sv_timeResidual;
extern int32_t sv_reconnectSequence;
#endif
