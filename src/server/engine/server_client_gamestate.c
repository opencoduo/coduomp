#include "server_client_gamestate.h"

#include "qcommon/game_module_abi_types.h"
#include "qcommon/game_state_types.h"
#include "qcommon/msg.h"
#include "qcommon/msg_delta.h"
#include "qcommon/q_cvar.h"
#include "server_commands.h"
#include "server_game_data.h"
#include "server_netchan.h"
#include "server_snapshot_send.h"
#include "qcommon/vm_runtime.h"

#include <stdint.h>
#include <string.h>

extern serverHeader_t sv;
extern serverStatic_t svs;
extern char *sv_configstrings[MAX_CONFIGSTRINGS];
extern vm_t *sv_gameVM;

void Com_DPrintf(const char *format, ...);
void Com_Error(errorParm_t code, const char *format, ...);

/*
 * Complete server gamestate-delivery and client-activation transition shared
 * by the Windows client engine and Linux dedicated engine:
 *
 * Function                    Windows       Linux
 * SV_SendClientGameState      0x0045a8f0    0x0808bc48
 * SV_ClientEnterWorld         0x0045ac40    0x0808bf31
 *
 * Both originals serialize the same opcodes, configstrings, baselines,
 * client number, checksum feed, and terminal markers, then publish the same
 * active-client state before GAME_CLIENT_BEGIN. The supporting Mac client
 * exports both canonical names.
 */

/* Source: CoDUOMP.exe 0x0045a8f0..0x0045ac35.
 * Name: same-module Mac symbol SV_SendClientGameState. The opcode and payload
 * sequence is the serialized server-to-client ABI and therefore remains
 * explicit even though the backing baseline storage is normalized above. */
void SV_SendClientGameState(client_t *client)
{
    while (client->state != CS_FREE &&
           client->netchan.unsentFragments != qfalse) {
        SV_Netchan_TransmitNextFragment(&client->netchan);
    }

    Com_DPrintf("SV_SendClientGameState() for %s\n", client->name);
    Com_DPrintf("Going from CS_CONNECTED to CS_PRIMED for %s\n",
                client->name);

    client->state = CS_PRIMED;
    client->pureAuthState = SERVER_CLIENT_PURE_STATE_PENDING;
    client->gamestateMessageNum = client->netchan.outgoingSequence;

    uint8_t messageData[MAX_MSGLEN];
    msg_t message;
    MSG_Init(&message, messageData, sizeof(messageData));
    MSG_WriteLong(&message, client->lastClientCommand);
    SV_UpdateServerCommandsToClient(client, &message);

    MSG_WriteByte(&message, SERVER_SVC_GAMESTATE);
    MSG_WriteLong(&message, client->reliableSequence);

    for (int32_t configstringNum = 0;
         configstringNum < MAX_CONFIGSTRINGS;
         ++configstringNum) {
        if (sv_configstrings[configstringNum][0] != '\0') {
            MSG_WriteByte(&message, SERVER_SVC_CONFIGSTRING);
            MSG_WriteShort(&message, configstringNum);
            MSG_WriteBigString(&message,
                               sv_configstrings[configstringNum]);
        }
    }

    entityState_t nullEntityState;
    memset(&nullEntityState, 0, sizeof(nullEntityState));
    for (int32_t entityNum = 0;
         entityNum < MAX_GENTITIES;
         ++entityNum) {
        entityState_t *const baseline =
            &sv_entities[entityNum].baseline.state;
        if (baseline->number != 0) {
            MSG_WriteByte(&message, SERVER_SVC_BASELINE);
            MSG_WriteDeltaEntity(&message, &nullEntityState,
                                 baseline, qtrue);
        }
    }

    MSG_WriteByte(&message, SERVER_SVC_EOF);
    const int32_t clientNum = (int32_t)(client - svs.clients);
    MSG_WriteLong(&message, clientNum);
    MSG_WriteLong(&message, sv.gamestateChecksumFeed);
    MSG_WriteByte(&message, SERVER_SVC_EOF);

    /* NOT_FROM_ORIGINAL_SOURCE: the extended MAX_GAMESTATE_CHARS budget can
     * compose more than one MAX_MSGLEN message holds once per-string framing,
     * baselines, and pending server commands ride along. The message writers
     * only latch message.overflowed, so fail loudly here instead of handing
     * the netchan a silently truncated gamestate. */
    if (message.overflowed != qfalse) {
        Com_Error(ERR_DROP,
                  "\x15" "SV_SendClientGameState: gamestate for client %i "
                  "exceeds the %i-byte message budget",
                  clientNum, MAX_MSGLEN);
    }

    Com_DPrintf("Sending %i bytes in gamestate to client: %i\n",
                message.cursize, clientNum);
    SV_SendMessageToClient(&message, client);

    if (com_timescale->integer == 0 &&
        client->state != CS_FREE &&
        client->netchan.unsentFragments != qfalse) {
        SV_Netchan_TransmitNextFragment(&client->netchan);
    }
}

/* Source: CoDUOMP.exe 0x0045ac40..0x0045acf5.
 * Name: same-module Mac symbol SV_ClientEnterWorld. */
void SV_ClientEnterWorld(client_t *client, const usercmd_t *command)
{
    const int32_t clientNum = (int32_t)(client - svs.clients);

    Com_DPrintf("Going from CS_PRIMED to CS_ACTIVE for %s\n",
                client->name);
    client->state = CS_ACTIVE;

    sharedEntity_t *const gentity = SV_GentityNum(clientNum);
    gentity->entityState.number = clientNum;
    client->gentity = gentity;
    client->deltaMessage = SERVER_DELTA_MESSAGE_NONE;
    client->nextSnapshotTime = svs.realTime;
    client->lastUsercmd = *command;

    (void)VM_Call(sv_gameVM, GAME_CLIENT_BEGIN,
                  clientNum, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}
