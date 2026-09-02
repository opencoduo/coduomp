#include "../module/ui_functions.h"
#include "client/common/client_legacy_crt.h"

#include <stdlib.h>

enum {
    UI_SERVERINFO_CONFIG_STRING = 0,
    UI_SERVERINFO_BUFFER_SIZE = 1024,
    UI_PLAYER_NAME_COPY_SIZE = 31
};

// Source: uo_ui_mp_x86.dll 0x4000a3f0..0x4000a4cf
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000a3f0_4000a4cf.mcode
// Exact same-module PPC symbol: UI_BuildPlayerList.
void UI_BuildPlayerList(void)
{
    uiClientState_t state;
    char serverInfo[UI_SERVERINFO_BUFFER_SIZE];
    char clientName[UI_PLAYER_NAME_SIZE];
    int32_t maxClients;
    int32_t clientNum;

    trap_GetClientState(&state);
    ui_myClientNum = state.clientNum;

    trap_GetConfigString(UI_SERVERINFO_CONFIG_STRING, serverInfo, sizeof(serverInfo));
    maxClients = coduo_crt_atoi(Info_ValueForKey(serverInfo, "sv_maxclients"));
    ui_playerCount = 0;

    for (clientNum = 0; clientNum < maxClients; ++clientNum) {
        char *playerName;

        if (!trap_GetClientName(clientNum, clientName, sizeof(clientName))) {
            continue;
        }

        playerName = ui_playerNames[ui_playerCount];
        /* strncpy(dst, src, 0x1f) + dst[0x1f] = 0: 31 characters survive. */
        Q_strncpyz(playerName, clientName, UI_PLAYER_NAME_COPY_SIZE + 1);
        Q_CleanStr(playerName);
        ++ui_playerCount;
    }
}
