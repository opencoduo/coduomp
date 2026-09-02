#include "server_lifecycle.h"

#include "qcommon/q_cvar.h"
#include "qcommon/q_string.h"
#include "qcommon/qcommon_runtime_types.h"
#include "server_client_release.h"
#include "server_commands.h"
#include "server_game_lifecycle.h"
#include "server_master.h"
#include "server_operator_runtime.h"
#include "qcommon/server_runtime_types.h"
#include "server_snapshot_archive.h"
#include "server_snapshot_send.h"
#include "server_startup.h"

#include <stdint.h>
#include <string.h>

enum {
    SV_FINAL_MESSAGE_PASSES = 2
};

extern serverStatic_t svs;
extern cvar_t *sv_running;
extern cvar_t *g_gametype;
extern cvar_t *scr_allow_jeeps;
extern cvar_t *scr_allow_tanks;
extern cvar_t *sv_mapname;
extern cvar_t *sv_privateClients;
extern cvar_t *sv_hostname;
extern cvar_t *sv_maxclients;
extern cvar_t *sv_punkbuster;
extern cvar_t *sv_maxRate;
extern cvar_t *sv_minPing;
extern cvar_t *sv_maxPing;
extern cvar_t *sv_floodProtect;
extern cvar_t *sv_allowAnonymous;
extern cvar_t *sv_showCommands;
extern cvar_t *sv_disableClientConsole;
extern cvar_t *sv_serverid;
extern cvar_t *sv_pure;
extern cvar_t *rconPassword;
extern cvar_t *sv_privatePassword;
extern cvar_t *sv_fps;
extern cvar_t *sv_timeout;
extern cvar_t *sv_zombietime;
extern cvar_t *sv_allowDownload;
extern cvar_t *sv_reconnectlimit;
extern cvar_t *sv_showloss;
extern cvar_t *sv_padPackets;
extern cvar_t *sv_killserver;
extern cvar_t *sv_onlyVisibleClients;
extern cvar_t *sv_packet_info;
extern cvar_t *sv_showAverageBPS;
extern cvar_t *sv_kickBanTime;
extern cvar_t *sv_mapRotation;
extern cvar_t *sv_mapRotationCurrent;
extern cvar_t *sv_wwwDownload;
extern cvar_t *sv_wwwBaseURL;
extern cvar_t *sv_wwwDlDisconnected;
extern int32_t com_errorEntered;

void Com_Printf(const char *format, ...);
void CL_Disconnect(qboolean showMainMenu);

/*
 * Complete server initialization/final-message/shutdown lifecycle shared by
 * the Windows client engine and Linux dedicated engine:
 *
 *   Function          CoDUOMP.exe                coduo_lnxded
 *   SV_Init           0x004604f0..0x004608fb     0x08092211..0x08092802
 *   SV_FinalMessage   0x00460900..0x00460991     0x08092803..0x080928c1
 *   SV_Shutdown       0x004609a0..0x00460a54     0x080928c2..0x0809297e
 *
 * Both targets register the same cvars in the same order, perform two final
 * snapshot passes, release the same server-owned resources, clear the complete
 * serverStatic_t, and disconnect the local client boundary.  Windows inlines
 * SV_MasterShutdown and the server-static clear.  Linux retains a call to the
 * empty SV_RemoveOperatorCommands; its absence here has no observable effect.
 */
void SV_Init(void)
{
    SV_AddOperatorCommands();

    g_gametype = Cvar_Get("g_gametype", "dm", CVAR_LATCH | CVAR_SERVERINFO);
    scr_allow_jeeps = Cvar_Get("scr_allow_jeeps", "1", CVAR_LATCH | CVAR_SERVERINFO);
    scr_allow_tanks = Cvar_Get("scr_allow_tanks", "1", CVAR_LATCH | CVAR_SERVERINFO);
    (void)Cvar_Get("sv_keywords", "", CVAR_SERVERINFO);

    const char *const protocol = va("%i", SERVER_PROTOCOL_VERSION);
    (void)Cvar_Get("protocol", protocol, CVAR_ROM | CVAR_SERVERINFO);
    (void)Cvar_Set2("protocol", protocol, qtrue);

    sv_mapname = Cvar_Get("mapname", "nomap", CVAR_ROM | CVAR_SERVERINFO);
    sv_privateClients = Cvar_Get("sv_privateClients", "0", CVAR_SERVERINFO);
    sv_hostname = Cvar_Get("sv_hostname", "CoDHost", CVAR_ARCHIVE | CVAR_SERVERINFO);
    sv_maxclients = Cvar_Get("sv_maxclients", "20", CVAR_LATCH | CVAR_SERVERINFO);
    sv_punkbuster = Cvar_Get("sv_punkbuster", "0", CVAR_ARCHIVE | CVAR_ROM | CVAR_SERVERINFO);
    sv_maxRate = Cvar_Get("sv_maxRate", "0", CVAR_ARCHIVE | CVAR_SERVERINFO);
    sv_minPing = Cvar_Get("sv_minPing", "0", CVAR_ARCHIVE | CVAR_SERVERINFO);
    sv_maxPing = Cvar_Get("sv_maxPing", "0", CVAR_ARCHIVE | CVAR_SERVERINFO);
    sv_floodProtect = Cvar_Get("sv_floodProtect", "1", CVAR_ARCHIVE | CVAR_SERVERINFO);
    sv_allowAnonymous = Cvar_Get("sv_allowAnonymous", "0", CVAR_SERVERINFO);
    sv_showCommands = Cvar_Get("sv_showCommands", "0", CVAR_NONE);
    sv_disableClientConsole = Cvar_Get("sv_disableClientConsole", "0", CVAR_SYSTEMINFO);
    (void)Cvar_Get("sv_cheats", "1", CVAR_ROM | CVAR_SYSTEMINFO);
    sv_serverid = Cvar_Get("sv_serverid", "0", CVAR_ROM | CVAR_SYSTEMINFO);
    sv_pure = Cvar_Get("sv_pure", "1", CVAR_SYSTEMINFO | CVAR_SERVERINFO);
    (void)Cvar_Get("sv_paks", "", CVAR_ROM | CVAR_SYSTEMINFO);
    (void)Cvar_Get("sv_pakNames", "", CVAR_ROM | CVAR_SYSTEMINFO);
    (void)Cvar_Get("sv_referencedPaks", "", CVAR_ROM | CVAR_SYSTEMINFO);
    (void)Cvar_Get("sv_referencedPakNames", "", CVAR_ROM | CVAR_SYSTEMINFO);

    rconPassword = Cvar_Get("rconPassword", "", CVAR_TEMP);
    sv_privatePassword = Cvar_Get("sv_privatePassword", "", CVAR_TEMP);
    sv_fps = Cvar_Get("sv_fps", "20", CVAR_TEMP);
    sv_timeout = Cvar_Get("sv_timeout", "240", CVAR_TEMP);
    sv_zombietime = Cvar_Get("sv_zombietime", "2", CVAR_TEMP);
    (void)Cvar_Get("nextmap", "", CVAR_TEMP);
    sv_allowDownload = Cvar_Get("sv_allowDownload", "1", CVAR_ARCHIVE | CVAR_SYSTEMINFO);
    sv_reconnectlimit = Cvar_Get("sv_reconnectlimit", "3", CVAR_NONE);
    sv_showloss = Cvar_Get("sv_showloss", "0", CVAR_NONE);
    sv_padPackets = Cvar_Get("sv_padPackets", "0", CVAR_NONE);
    sv_killserver = Cvar_Get("sv_killserver", "0", CVAR_NONE);
    sv_onlyVisibleClients = Cvar_Get("sv_onlyVisibleClients", "0", CVAR_NONE);
    sv_packet_info = Cvar_Get("sv_packet_info", "0", CVAR_NONE);
    sv_showAverageBPS = Cvar_Get("sv_showAverageBPS", "0", CVAR_NONE);
    sv_kickBanTime = Cvar_Get("sv_kickBanTime", "300", CVAR_NONE);
    (void)Cvar_Get("g_complaintlimit", "3", CVAR_ARCHIVE);
    sv_mapRotation = Cvar_Get("sv_mapRotation", "", CVAR_NONE);
    sv_mapRotationCurrent = Cvar_Get("sv_mapRotationCurrent", "", CVAR_NONE);
    (void)Cvar_Get("fs_game", "", CVAR_SYSTEMINFO | CVAR_SERVERINFO);
    sv_wwwDownload = Cvar_Get("sv_wwwDownload", "0", CVAR_ARCHIVE);
    sv_wwwBaseURL = Cvar_Get("sv_wwwBaseURL", "", CVAR_ARCHIVE);
    sv_wwwDlDisconnected = Cvar_Get("sv_wwwDlDisconnected", "0", CVAR_ARCHIVE);
}

void SV_FinalMessage(const char *message)
{
    for (int32_t pass = 0; pass < SV_FINAL_MESSAGE_PASSES; ++pass) {
        client_t *client = svs.clients;
        for (int32_t clientNum = 0; clientNum < sv_maxclients->integer; ++clientNum, ++client) {
            if (client->state < CS_CONNECTED) {
                continue;
            }

            if (client->netchan.remoteAddress.type != NA_LOOPBACK) {
                SV_SendServerCommand(client, qfalse, "e \"%s\"", message);
                SV_SendServerCommand(client, qtrue, "w");
            }
            client->nextSnapshotTime = -1;
            SV_SendClientSnapshot(client);
        }
    }
}

void SV_Shutdown(const char *finalMessage)
{
    if (sv_running == NULL || sv_running->integer == 0) {
        return;
    }

    Com_Printf("----- Server Shutdown -----\n");
    if (svs.clients != NULL && com_errorEntered == qfalse) {
        SV_FinalMessage(finalMessage);
    }

    SV_MasterShutdown();
    SV_ShutdownGameProgs();
    SV_ClearServer();
    if (svs.clients != NULL) {
        SV_FreeClients();
    }
    SV_FreeArchivedSnapshot();
    memset(&svs, 0, sizeof(svs));
    (void)Cvar_Set2("sv_running", "0", qtrue);
    Com_Printf("---------------------------\n");
    CL_Disconnect(qfalse);
}
