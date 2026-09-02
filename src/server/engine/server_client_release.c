#include "server_client_release.h"

#include "qcommon/game_module_abi_types.h"
#include "qcommon/qcommon_runtime_types.h"
#include "scripting/script_variable.h"
#include "server_download.h"
#include "server_game_bridge.h"
#include "qcommon/vm_runtime.h"

#include <stdint.h>
#include <stdlib.h>

extern serverHeader_t sv;
extern serverStatic_t svs;
extern cvar_t *sv_maxclients;
extern vm_t *sv_gameVM;

/*
 * Complete shared client-resource release cluster:
 *
 * Function                    Windows       Linux
 * SV_FreeClient               0x00459a60    0x0808abb3
 * SV_FreeClients              0x00459ad0    0x0808ac2b
 * SV_FreeClientScriptPers     0x0045a5e0    0x0808b9b5
 * SV_FreeClientScriptId       0x00462e60    0x08094de7
 *
 * The original bodies agree on client-state thresholds, release ordering,
 * game-VM notification, userinfo clearing, script-object ownership, array
 * replacement, client stride, and final svs.clients free. Linux's recovered
 * PB_DropClient(client_t *) and FUN_0808ac2b spellings described the first
 * two bodies incorrectly; Windows and the Quake server API supply the
 * canonical SV_FreeClient and SV_FreeClients identities.
 */

void SV_FreeClientScriptId(client_t *client)
{
    Scr_FreeValue(client->scriptId);
    client->scriptId = 0;
}

void SV_FreeClient(client_t *client)
{
    const int32_t clientNum = (int32_t)(client - svs.clients);

    SV_CloseDownload(client);
    if (sv.state == SS_GAME) {
        (void)VM_Call(sv_gameVM, GAME_CLIENT_DISCONNECT, clientNum, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    SV_SetUserinfo(clientNum, "");
    SV_FreeClientScriptId(client);
}

void SV_FreeClients(void)
{
    client_t *client = svs.clients;
    for (int32_t clientNum = 0; clientNum < sv_maxclients->integer; ++clientNum, ++client) {
        if (client->state >= CS_CONNECTED) {
            SV_FreeClient(client);
        }
    }
    free(svs.clients);
}

void SV_FreeClientScriptPers(void)
{
    client_t *client = svs.clients;
    for (int32_t clientNum = 0; clientNum < sv_maxclients->integer; ++clientNum, ++client) {
        if (client->state >= CS_CONNECTED) {
            SV_FreeClientScriptId(client);
            client->scriptId = Scr_AllocArray();
        }
    }
}
