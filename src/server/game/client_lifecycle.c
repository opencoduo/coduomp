/*
 * Source reconstruction for client connect/begin lifecycle helpers.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "recovered_game.h"
#include "qcommon/info.h"
#include "game_globals.h"
#include "game_functions.h"
#include "scr_vm.h"

#define CLIENT_ARCHIVE_NONE -1
/* GENTITY_OFFSET_* replaced by struct field access:
 * CONNECT_FLAGS -> ent->svFlags, TOUCH -> ent->touch, USE -> ent->pain (at +0x224),
 * ENTITY_NUM -> ent->s.groundEntityNum, LINKED_STATE -> ent->linkedState,
 * BOUNDS_MIN -> ent->mins, BOUNDS_MAX -> ent->maxs, FLAGS -> ent->spawnflags,
 * CLIPMASK -> ent->clipmask, DIE -> ent->die.
 * MODEL_CLASSNAME offset (0x184) is ent->scriptClassname.
 * BYTE_17B/17C/17D -> ent->clientSpawnStateByte,
 * ent->clientSpawnResetByte, ent->takeDamage. */
#define PLAYERSTATE_CONNECT_FLAGS 0x10
#define USERINFO_BUFFER_SIZE 1024
#define CG_ATMOS_DEFAULT "-1"
#define PASSWORD_NONE "none"
#define INVALID_PASSWORD_MESSAGE "GAME_INVALIDPASSWORD"
/* GENTITY_OFFSET_ENTITY_NUM, LINKED_STATE, BOUNDS_MIN/MAX, MODEL_CLASSNAME,
 * FLAGS, CLIPMASK, DIE, BYTE_17B/17C/17D -- all replaced by struct fields. */
/* CLIENT_OFFSET_* replaced by gclient_t struct field access,
 * leanFraction -> client->ps.leanFraction, leanYaw -> client->ps.viewAngles[1]. */
#define CLIENT_SESSION_COPY_SIZE (offsetof(gclient_t, archiveClient) - offsetof(gclient_t, sessionState))
#define CLIENT_SPAWN_PRESERVED_PS_FLAGS 0x00010008u
#define CLIENT_SPAWN_ACTIVE_PS_FLAG 0x00000010u
#define CLIENT_SPAWN_TURRET_FLAGS 0x00006000u
#define CLIENT_SPAWN_ORIGIN_TELEPORT_BIT 0x00000008u
#define CLIENT_SPAWN_STANCE_FLAG 0x00000800u
#define CLIENT_SPAWN_ENTITY_FLAGS 0x00002000u
#define CLIENT_SPAWN_VIEWHEIGHT_MODE 8
#define CLIENT_SPAWN_LEVELTIME_BACKDATE_MS 100
#define CLIENT_SECONDS_TO_MS 1000
#define CLIENT_DISCONNECT_TAIL_CLEAR_SIZE (offsetof(gclient_t, spectatorActivityState) - offsetof(gclient_t, clientNum))
#define SERVER_COMMAND_RELIABLE 1
#define DISCONNECT_REASON "disconnect"
#define DISCONNECT_MENU_RESPONSE_VALUE "-1"
#define DISCONNECT_COMPLAINT_CANCEL "m -2"
#define PLAYER_SIZE_HALF_WIDTH 0.5f
#define LEAN_SIDE_SCALE 16.0f
#define LEAN_FORWARD_SCALE 20.0f

typedef char client_session_copy_size_check[(CLIENT_SESSION_COPY_SIZE == 276) ? 1 : -1];
typedef char client_disconnect_tail_clear_size_check[(CLIENT_DISCONNECT_TAIL_CLEAR_SIZE == 92) ? 1 : -1];

static const float CLIENT_WALK_SPEED_SCALE = 0.4f;
static const float CLIENT_RUN_SPEED_SCALE = 1.0f;
static const float CLIENT_SPRINT_SPEED_SCALE = 1.6f;
static const float CLIENT_PRONE_SPEED_SCALE = 0.15f;
static const float CLIENT_CROUCH_SPEED_SCALE = 0.65f;
static const float CLIENT_STRAFE_SPEED_SCALE = 0.8f;
static const float CLIENT_BACK_SPEED_SCALE = 0.7f;


void G_InitGentity(gentity_t *ent);
void G_FreeEntity(gentity_t *ent);
void G_ClientStopUsingTurret(gentity_t *ent);
void G_EntUnlink(gentity_t *ent);
void G_SetClientContents(gentity_t *ent);
void G_SetOrigin(gentity_t *ent, const float *origin);
void ClientUserinfoChanged(int clientNum);
void ClientEndFrame(gentity_t *ent);
void ClientThink_real(gentity_t *ent, usercmd_t *command);
void SetClientViewAngle(gentity_t *ent, const float *angle);
void trap_GetUserinfo(int clientNum, char *buffer, int bufferSize);
void trap_GetUsercmd(int clientNum, usercmd_t *command);
void trap_UnlinkEntity(gentity_t *ent);
void trap_Cvar_Set(const char *name, const char *value);
void Scr_PlayerConnect(gentity_t *ent);
void Scr_PlayerDisconnect(gentity_t *ent);
void Scr_AddString(const char *value);
void Scr_Notify(gentity_t *ent, uint16_t event, int paramCount);
void CalculateRanks(void);
void StopFollowing(gentity_t *ent);
void HudElem_ClientDisconnect(gentity_t *ent);
void trap_SendServerCommand(int clientNum, int reliable, const char *command);
void player_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath, int weapon, const float *dir,
                int hitLocation);

/* VERIFIED_DECOMPILER(0x44ab4, 54ab4_ClientConnect.c, VERIFY-WORKER-CLIENT-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED - client/BGS zeroing and preserved animTree, initial session fields, entity setup, userinfo/password rejection, connect script notify, and rank update checked. */
char *ClientConnect(int clientNum, uint16_t persistentValue)
{
    gentity_t *ent = &g_entities[clientNum];
    gclient_t *client = &level.clients[clientNum];
    clientInfo_t *clientInfo = &bgs.clientinfo[clientNum];
    XAnimTree *preservedAnimTree = clientInfo->animTree;
    char userinfo[USERINFO_BUFFER_SIZE];

    memset(client, 0, sizeof(*client));
    memset(clientInfo, 0, sizeof(*clientInfo));
    clientInfo->animTree = preservedAnimTree;
    clientInfo->infoValid = 1;
    clientInfo->moduleState.pmType = PM_TYPE_LINKED;

    client->connectedState = CON_CONNECTING;
    client->pers = persistentValue;
    client->sessionTeam = TEAM_SPECTATOR;
    client->sessionState = SESS_STATE_SPECTATOR;
    client->archiveClient = CLIENT_ARCHIVE_NONE;
    client->followClient = CLIENT_ARCHIVE_NONE;
    client->sessionSquad = SESS_SQUAD_NONE;

    G_InitGentity(ent);
    ent->touch = 0;
    ent->pain = 0;
    ent->client = client;
    client->clientNum = clientNum;
    client->ps.psClientNum = clientNum;
    client->ps.entityStateFlags = PLAYERSTATE_CONNECT_FLAGS;
    ent->svFlags = SVF_CAPSULE;
    client->pendingComplaintClient = CLIENT_ARCHIVE_NONE;
    client->pendingComplaintTime = CLIENT_ARCHIVE_NONE;
    client->maxSpeed = g_speed.integer;

    ClientUserinfoChanged(clientNum);

    trap_GetUserinfo(clientNum, userinfo, sizeof(userinfo));
    trap_Cvar_Set("cg_atmos", CG_ATMOS_DEFAULT);

    if (client->complaintDisabled == 0) {
        const char *password = Info_ValueForKey(userinfo, "password");
        const char *requiredPassword = g_password.string;

        if (requiredPassword[0] != '\0' && Q_stricmp(requiredPassword, PASSWORD_NONE) != 0 && strcmp(requiredPassword, password) != 0) {
            G_FreeEntity(ent);
            return INVALID_PASSWORD_MESSAGE;
        }
    }

    Scr_PlayerConnect(ent);
    CalculateRanks();
    return 0;
}

/* VERIFIED_DECOMPILER(0x44d82, 54d82_ClientBegin.c, VERIFY-WORKER-CLIENT-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED - connected-state store, CalculateRanks call, begin notify target/const/arg-count, and void return checked. */
void ClientBegin(int clientNum)
{
    level.clients[clientNum].connectedState = CON_CONNECTED;
    CalculateRanks();
    Scr_Notify(&g_entities[clientNum], scr_const_begin, 0);
}

/* VERIFIED_DECOMPILER(0x44df4, 54df4_ClientSpawn.c, VERIFY-WORKER-CLIENT-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED - turret/vehicle unlink gates, entity reset, session copy/restore, client memset and preserved health/flags, usercmd, viewheight/speed constants, origin/view angle setup, inactivity timers, and end-frame/think/state calls checked. */
void ClientSpawn(gentity_t *ent, const float *origin, const float *angles)
{
    int clientNum = (int)(ent - g_entities);
    gclient_t *client = ent->client;
    uint8_t savedSession[CLIENT_SESSION_COPY_SIZE];
    uint32_t preservedPsFlags = client->ps.entityStateFlags & CLIENT_SPAWN_PRESERVED_PS_FLAGS;
    int32_t preservedSpawnCount = client->ps.stats[STAT_SPAWN_COUNT];

    if ((client->ps.playerStateFlags & PSF_ACTIVE_PLAYER) != 0 && (client->ps.entityStateFlags & CLIENT_SPAWN_TURRET_FLAGS) != 0) {
        G_ClientStopUsingTurret(&level.gentities[client->ps.viewLockedEntityNum]);
    }

    if ((client->ps.playerStateFlags & PSF_ACTIVE_PLAYER) != 0 && (client->ps.entityStateFlags & EF_IN_VEHICLE) != 0) {
        VEH_UnlinkPlayer(ent, 0);
    }

    G_EntUnlink(ent);
    if (ent->linkedState != 0) {
        trap_UnlinkEntity(ent);
    }

    ent->s.groundEntityNum = ENTITYNUM_NONE;
    Scr_SetString(&ent->scriptClassname, scr_const_player);
    ent->clipmask = MASK_PLAYERSOLID;
    ent->svFlags |= 1u;
    ent->takeDamage = 0;
    G_SetClientContents(ent);
    ent->die = player_die;
    ent->clientSpawnStateByte = 0;
    ent->clientSpawnResetByte = 0;
    ent->flags = CLIENT_SPAWN_ENTITY_FLAGS;
    memcpy(ent->mins, playerMins, 3 * sizeof(float));
    memcpy(ent->maxs, playerMaxs, 3 * sizeof(float));

    memcpy(savedSession, &client->sessionState, sizeof(savedSession));
    memset(client, 0, sizeof(*client));
    memcpy(&client->sessionState, savedSession, sizeof(savedSession));

    client->archiveClient = CLIENT_ARCHIVE_NONE;
    client->ps.stats[STAT_SPAWN_COUNT] = coduo_int32_from_bits((uint32_t)preservedSpawnCount + UINT32_C(1));
    client->ps.stats[STAT_MAX_HEALTH] = client->normalMaxHealth;
    client->ps.entityStateFlags = preservedPsFlags | CLIENT_SPAWN_ACTIVE_PS_FLAG;
    client->clientNum = clientNum;
    client->ps.psClientNum = clientNum;

    /* 0x45149..0x45162 recomputes this index from the client-array base,
     * rather than reusing the entity-array index captured at entry. */
    trap_GetUsercmd((int)(client - level.clients), &client->command);
    client->ps.entityStateFlags ^= CLIENT_SPAWN_ORIGIN_TELEPORT_BIT;

    memcpy(client->ps.playerMins, ent->mins, 3 * sizeof(float));
    memcpy(client->ps.playerMaxs, ent->maxs, 3 * sizeof(float));
    /* The binary stores the raw cvar integers (mov at 0x451fa..0x45218);
     * only ps.viewHeightCurrent below goes through fild/fstp. */
    client->ps.proneViewHeight = bg_viewheight_prone.integer;
    client->ps.crouchViewHeight = bg_viewheight_crouched.integer;
    client->ps.standViewHeight = bg_viewheight_standing.integer;
    client->ps.deadViewHeight = CLIENT_SPAWN_VIEWHEIGHT_MODE;
    client->ps.viewHeightTarget = bg_viewheight_standing.integer;
    client->ps.viewHeightCurrent = (float)bg_viewheight_standing.integer;
    client->ps.viewHeightLerpTime = 0;
    client->ps.viewHeightLerpPosAdj = 0.0f;

    client->ps.walkSpeedScale = CLIENT_WALK_SPEED_SCALE;
    client->ps.runSpeedScale = CLIENT_RUN_SPEED_SCALE;
    client->ps.sprintSpeedScale = CLIENT_SPRINT_SPEED_SCALE;
    client->ps.proneSpeedScale = CLIENT_PRONE_SPEED_SCALE;
    client->ps.crouchSpeedScale = CLIENT_CROUCH_SPEED_SCALE;
    client->ps.strafeSpeedScale = CLIENT_STRAFE_SPEED_SCALE;
    client->ps.backSpeedScale = CLIENT_BACK_SPEED_SCALE;
    client->ps.leanSpeedScale = CLIENT_WALK_SPEED_SCALE;
    client->ps.friction = CLIENT_RUN_SPEED_SCALE;
    client->ps.fatigueScale = CLIENT_RUN_SPEED_SCALE;

    G_SetOrigin(ent, origin);
    client->ps.psOrigin[0] = origin[0];
    client->ps.psOrigin[1] = origin[1];
    client->ps.psOrigin[2] = origin[2];
    client->ps.playerStateFlags |= CLIENT_SPAWN_STANCE_FLAG;
    SetClientViewAngle(ent, angles);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    client->inactivityTime = coduo_int32_from_bits((uint32_t)g_inactivity.integer * (uint32_t)CLIENT_SECONDS_TO_MS + (uint32_t)level.time);
    client->spectatorInactivityTime =
        coduo_int32_from_bits((uint32_t)g_inactivityspectator.integer * (uint32_t)CLIENT_SECONDS_TO_MS + (uint32_t)level.time);
    client->inactivityWarningSent = 0;
    client->spectatorInactivityWarning = 0;
    client->latchedButtons = 0;
    client->latchedWbuttons = 0;
    client->command.commandTime = level.time;
    client->ps.commandTime = coduo_int32_from_bits((uint32_t)level.time - (uint32_t)CLIENT_SPAWN_LEVELTIME_BACKDATE_MS);

    ClientEndFrame(ent);
    ClientThink_real(ent, &client->command);
    BG_PlayerStateToEntityState(&client->ps, &ent->s, qtrue);
}

/* VERIFIED_DECOMPILER(0x45433, 55433_ClientDisconnect.c, VERIFY-WORKER-CLIENT-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED - disconnect menu notify, spectator-follow cleanup, single complaint cancel path, vehicle unlink gate, HUD/script/free teardown, tail clear, and rank update checked. */
void ClientDisconnect(int clientNum)
{
    gclient_t *client = &level.clients[clientNum];
    gentity_t *ent = &g_entities[clientNum];

    Scr_AddString(DISCONNECT_REASON);
    Scr_AddString(DISCONNECT_MENU_RESPONSE_VALUE);
    Scr_Notify(ent, scr_const_menuresponse, 2);

    for (int index = 0; index < level.maxclients; index++) {
        gclient_t *other = &level.clients[index];

        if (other->connectedState != 0 && other->sessionState == SESS_STATE_SPECTATOR && other->archiveClient == clientNum) {
            StopFollowing(&g_entities[index]);
        }
    }

    for (int index = 0; index < level.maxclients; index++) {
        gclient_t *other = &level.clients[index];

        if (other->connectedState != 0 && other->pendingComplaintClient == clientNum) {
            other->pendingComplaintClient = CLIENT_ARCHIVE_NONE;
            other->pendingComplaintTime = 0;
            trap_SendServerCommand(index, SERVER_COMMAND_RELIABLE, DISCONNECT_COMPLAINT_CANCEL);
            break;
        }
    }

    if ((client->ps.entityStateFlags & EF_IN_VEHICLE) != 0) {
        VEH_UnlinkPlayer(ent, 0);
    }

    HudElem_ClientDisconnect(ent);
    Scr_PlayerDisconnect(ent);
    G_FreeEntity(ent);
    client->connectedState = 0;
    memset(&client->clientNum, 0, CLIENT_DISCONNECT_TAIL_CLEAR_SIZE);
    CalculateRanks();
}

/* VERIFIED_DECOMPILER(0x45692, 55692_G_SetPlayerSize.c, VERIFY-WORKER-CLIENT-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED - width half-scale, XY mins/maxs, standing-height max Z, and void return checked. */
void G_SetPlayerSize(void)
{
    /* The binary computes width * -0.5f and width * 0.5f as two separate
     * products (0x456b6/0x456de), storing element [1] before [0] each time;
     * value-identical to negating one product, form kept for
     * instruction-stream parity. */
    playerMins[0] = playerMins[1] = g_bounds_width.value * -PLAYER_SIZE_HALF_WIDTH;
    playerMaxs[0] = playerMaxs[1] = g_bounds_width.value * PLAYER_SIZE_HALF_WIDTH;
    playerMaxs[2] = g_bounds_height_standing.value;
}

/* VERIFIED_DECOMPILER(0x4570c, 5570c_G_AddLean.c, VERIFY-WORKER-CLIENT-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED - AddLeanToPosition argument order, view yaw/lean fraction offsets, 16.0/20.0 constants, and void return checked. */
void G_AddLean(gentity_t *ent, float *origin)
{
    gclient_t *client = ent->client;

    AddLeanToPosition(origin, client->ps.viewAngles[1], /* leanYaw at +0xec */
                      client->ps.leanFraction, /* lean fraction at +0x44 */
                      LEAN_SIDE_SCALE, LEAN_FORWARD_SCALE);
}
