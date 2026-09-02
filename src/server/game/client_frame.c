/*
 * Source reconstruction for client frame/contact helpers.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "recovered_game.h"
#include "game_globals.h"
#include "scr_vm.h"
#include "g_syscalls.h"
#include "game_functions.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */
#include "compat/coduo_native_x87.h"
#include "compat/libm/coduo_libm.h"
#include "compat/coduo_int32_bits.h"

/* GENTITY_OFFSET_* constants replaced by struct field access:
 * CLIENT_SOUND -> ent->s.clientSound, CLIENTINFO_FIELD_6C -> ent->s.clientInfo.leanAmount,
 * ENTITY_STATE_WEAPON -> ent->s.weapon, CLIENTINFO_FIELD_D8 -> ent->s.clientInfoLeanFraction,
 * LINKED_STATE_0C -> ent->s.pos.trType, LINKED_STATE_30 -> ent->s.apos.trType,
 * INTERMISSION_FLAGS -> ent->svFlags, VEHICLE_OWNER -> ent->passEntityNum,
 * BOUNDS_MIN -> ent->mins, BOUNDS_MAX -> ent->maxs, CONTENTS -> ent->scriptContents,
 * ABS_MIN_X/Y/Z -> ent->absMin[0/1/2], ABS_MAX_X/Y/Z -> ent->absMax[0/1/2],
 * PAIN_EVENT_TIME -> ent->painEventTime, TOUCH -> ent->touch,
 * CONTROLLER -> ent->controller, CLIENTINFO_DOBJ_DIRTY -> ent->dobjDirty,
 * INTERMISSION_BYTE -> ent->takeDamage. */
/* CLIENT_OFFSET_* constants replaced by struct field access:
 * DAMAGE_EVENT -> client->ps.damageEvent, DAMAGE_YAW_BYTE -> client->ps.damageYaw,
 * DAMAGE_PITCH_BYTE -> client->ps.damagePitch, DAMAGE_COUNT -> client->ps.damageCount,
 * DAMAGE_ALPHA -> client->ps.aimSpreadScale, DAMAGE_FEEDBACK_MAX_HEALTH -> client->normalMaxHealth,
 * EVENTS -> client->ps.events, EVENT_PARMS -> client->ps.eventParms,
 * CLIENTINFO_BASE_MODEL -> client->baseModelIndex,
 * CLIENTINFO_ATTACH_MODELS -> client->attachModelIndices,
 * CLIENTINFO_ATTACH_TAGS -> client->attachTagIndices,
 * CLIENTEND_NONPVS_FRIENDLY_CLIENT -> client->nonpvsFriendlyClient,
 * CLIENTEND_NONPVS_TANK_CLIENT -> client->nonpvsTankClient,
 * CLIENTEND_TURRET_ENTITY -> client->ps.viewLockedEntityNum,
 * SPECTATOR_BUTTONS_SOURCE -> client->command.buttons,
 * SPECTATOR_WBUTTONS_SOURCE -> client->command.wbuttons,
 * SPECTATOR_BUTTONS -> client->currentButtons,
 * SPECTATOR_OLD_BUTTONS -> client->oldButtons,
 * FLAME_ACTIVE_FLAGS/SPECTATOR_WBUTTONS -> client->spectatorWbuttons,
 * SPECTATOR_OLD_WBUTTONS -> client->oldWbuttons,
 * SPECTATE_TEAM_DENY_MASK -> client->spectateTeamDenyMask,
 * DAMAGE_TAKEN -> client->damageTaken, DAMAGE_FROM -> client->damageFrom,
 * DAMAGE_FROM_WORLD -> client->damageFromWorld, DAMAGE_TIME -> client->damageTime,
 * DAMAGE_ROLL -> client->damageRoll, DAMAGE_PITCH -> client->damagePitch,
 * EVENT_SEQUENCE -> client->ps.eventIndex. */
#define CLIENT_IMPACT_TRACE_MODE 1
#define TOUCH_TRIGGERS_TRACE_MODE 1
#define TOUCH_TRIGGERS_SCRIPT_SYSTEM 1
#define TOUCH_TRIGGERS_BOUNDS_XY 40.0f
#define TOUCH_TRIGGERS_BOUNDS_Z 52.0f
#define TOUCH_TRIGGERS_CONTENTS_MASK MASK_TRIGGER
#define DO_TOUCH_TRIGGERS_VEHICLE_CONTENTS_MASK CONTENTS_TRIGGER_TOUCH_VEHICLE
#define DO_TOUCH_TRIGGERS_CLIENT_CONTENTS_MASK CONTENTS_TRIGGER_TOUCH_CLIENT
#define DEBUG_LABEL(value) "debug-" value
#define RUNCLIENT_FIXED_LINK_MODE 2
#define INTERMISSION_ENTITY_FLAGS_CLEAR 0x00000002u
#define INTERMISSION_ENTITY_FLAGS_SET 0x00000001u
#define INTERMISSION_STANCE_FLAGS_CLEAR 0x00380000u
#define INTERMISSION_PS_FLAGS_CLEAR 0x00020200u
#define FOLLOW_PLAYERSTATE_COPY_SIZE offsetof(gclient_t, sessionState)
#define FOLLOW_PLAYERSTATE_CLEAR_OFFSET offsetof(gclient_t, ps.hudCurrent)
#define FOLLOW_PLAYERSTATE_CLEAR_SIZE \
    sizeof(((gclient_t *)0)->ps.hudCurrent)
#define ARCHIVE_CLIENT_NONE -1
#define SPECTATOR_ARCHIVE_STEP_MS 50
#define SPECTATOR_ARCHIVE_HUDELEM_UPDATE_FLAGS 2u
#define SPECTATOR_ARCHIVE_PRESERVE_PS_FLAG 0x00010000u
#define SPECTATOR_CAN_FOLLOW_FLAG 0x00100000u
#define SPECTATOR_CAN_FOLLOW_TEAM_FLAG 0x00200000u
#define SPECTATOR_ARCHIVE_FLAGS_CLEAR 0x00300000u
#define STUCK_CLIENT_PUSH_FLAG 0x00000100u
#define STUCK_ENTITY_CAPSULE_FLAG 0x00000200u
#define STUCK_ENTITY_SKIP_FLAG 0x00100000u
#define STUCK_PM_TIME 300
#define STUCK_SPEED_EPSILON 0.0001f /* original float32 0x38d1b717 */
#define CLIENTEND_ENTITY_FLAGS_NORMAL_CLEAR 0x00000001u
#define CLIENTEND_ENTITY_FLAGS_NORMAL_SET 0x00000002u
#define CLIENTEND_ENTITY_FLAGS_DEAD_CLEAR 0x00000002u
#define CLIENTEND_ENTITY_FLAGS_DEAD_SET 0x00000001u
#define CLIENTEND_STANCE_FLAGS_CLEAR 0x00300000u
#define CLIENTEND_PS_TIMED_FLAG 0x00040000u
#define CLIENTEND_PS_INACTIVE_FLAG 0x00000800u
#define CLIENTEND_PS_FRIENDLY_HAS_FLAG 0x00080000u
#define CLIENTEND_TURRET_PS_FLAGS 0x00006000u
#define CLIENTEND_VEHICLE_PITCH_THRESHOLD 20.0f
#define CLIENTEND_DAMAGE_ALPHA_SCALE 255.0 /* stock 0x432ae loads a QWORD double const */
#define CLIENTEND_INACTIVE_THRESHOLD_MS 1000
#define CLIENTEND_NONPVS_NONE ENTITYNUM_NONE
#define CLIENTEND_NONPVS_ENTITY_MASK 0x3f
#define CLIENTEND_FRIENDLY_ENTITY_FLAG 0x00040000u
#define CLIENTEND_NONPVS_SCAN_CLIENTS 64
#define CLIENTEND_NONPVS_OFFSET_LIMIT_POS 1024
#define CLIENTEND_NONPVS_OFFSET_LIMIT_NEG -1022
#define CLIENTEND_NONPVS_OFFSET_CENTER 255
#define CLIENTEND_NONPVS_OFFSET_MASK 0x1ffu
#define CLIENTEND_NONPVS_X_SHIFT 6
#define CLIENTEND_NONPVS_Y_SHIFT 15
#define CLIENTEND_NONPVS_YAW_SHIFT 24
#define CLIENTEND_NONPVS_YAW_SCALE 0.7111111f /* original float32 0x3f360b61 */
#define SPECTATOR_FOLLOW_TEAM 4
#define SPECTATOR_BUTTON_STOP_FOLLOW_MASK 0x10u
#define SPECTATOR_BUTTON_FOLLOW_NEXT_MASK 0x01u
#define SPECTATOR_BUTTON_FOLLOW_PREV_MASK 0x20u
#define SPECTATOR_PM_FLAGS_ALLOWED 400
#define SPECTATOR_PMOVE_TRACEMASK 0x800011
#define DEBUG_ARCHIVE_THROTTLE_MS 1000
#define SERVER_COMMAND_UNRELIABLE 0
#define INACTIVITY_GRACE_MS 60000
#define INACTIVITY_WARNING_MS 10000
#define INACTIVITY_SECONDS_TO_MS 1000
#define INACTIVITY_INPUT_BUTTON_MASK 0x01u
#define CLIENT_EVENTS_RING_MASK 0x03u
#define CLIENT_EVENTS_MAX_BACKLOG 4
#define CLIENT_EVENTS_DAMAGE_MIN 116
#define CLIENT_EVENTS_DAMAGE_MAX 138
#define CLIENT_EVENTS_DAMAGE_FULL_SCALE 1.1f
#define CLIENT_EVENTS_DAMAGE_PERCENT_SCALE 0.01f
#define CLIENT_EVENTS_PAIN_DELAY_MS 200
#define CLIENT_EVENTS_FALL_DAMAGE_MOD MOD_FALLING
#define CLIENT_EVENTS_WEAPON_RAISE 157
#define CLIENT_EVENTS_WEAPON_LOWER 158
#define CLIENT_EVENTS_FIRE_BEGIN 163
#define CLIENT_EVENTS_FIRE_LASTSHOT 164
#define CLIENT_EVENTS_FIRE_ALT 165
#define CLIENT_EVENTS_FIRE_LASTSHOT_ALT 166
#define CLIENT_EVENTS_MELEE 170
#define CLIENT_EVENTS_FIRE_MOUNTED 173
#define CLIENT_EVENTS_DETACH_WEAPON 202
#define CLIENT_EVENTS_DROP_WEAPON 203
#define CLIENT_EVENTS_SUICIDE 210
#define ENTITY_FLAG_GODMODE 0x00000001u
#define CLIENTEND_VEHICLE_STATE_ORIGIN_OFFSET \
    offsetof(vehicle_state_t, origin)
#define CLIENTEND_VEHICLE_STATE_NULL_SENTINEL \
    ((vehicle_state_t *)(intptr_t)(-((intptr_t)CLIENTEND_VEHICLE_STATE_ORIGIN_OFFSET)))
#define SUICIDE_DAMAGE 100000
#define DAMAGE_FEEDBACK_MAX_COUNT 127
#define DAMAGE_FEEDBACK_ALPHA_MAX 255.0f
#define DAMAGE_FEEDBACK_KICK_SCALE 0.2f
#define DAMAGE_FEEDBACK_KICK_MIN 5.0f
#define DAMAGE_FEEDBACK_KICK_MAX 90.0f
#define DAMAGE_FEEDBACK_BYTE_SCALE 256.0f
#define DAMAGE_FEEDBACK_DEGREES_PER_CIRCLE 360.0f
#define DAMAGE_FEEDBACK_EVENT_INTERVAL_MS 700
#define DAMAGE_FEEDBACK_TIME_BACKDATE_MS 20
#define FLAME_DAMAGE_WINDOW_MS 3000

static int flameWeaponIndex = -1; /* DAT_000bcadc */

static vmCvar_t sv_privateClients; /* DAT_000e0de0 */
/* NOT_FROM_ORIGINAL_SOURCE: throttle for g_debugArchiveCheck diagnostics. */
static int debugArchiveLastGetFollowLog[MAX_CLIENTS];
/* NOT_FROM_ORIGINAL_SOURCE: throttle for g_debugArchiveCheck diagnostics. */
static int debugArchiveLastSpectatorLog[MAX_CLIENTS];

int BG_PlayerTouchesItem(gclient_t *client, gentity_t *itemEnt, int time);
qboolean Cmd_FollowCycle_f(gentity_t *ent, int direction);
qboolean G_ClientCanSpectateTeam(gclient_t *client, int team);
void HudElem_UpdateClient(gclient_t *client, int clientNum,
                                 uint32_t updateFlags);
void G_Damage(gentity_t *target, gentity_t *inflictor,
                     gentity_t *attacker, const float *dir, const float *point,
                     int damage, int dflags, int mod, int hitLoc);
qboolean G_EntAttach(gentity_t *ent, const char *modelName,
                            const char *tagName, qboolean ignoreCollision);
qboolean G_EntDetach(gentity_t *ent, const char *modelName,
                            const char *tagName);
void G_SetFixedLink(gentity_t *ent, int mode);
void G_SetOrigin(gentity_t *ent, const float *origin);
int G_TagIndex(const char *tagName);
void G_SafeDObjFree(gentity_t *ent);
void BG_UpdatePlayerDObj(gentity_t *ent, const gentity_t *entState,
                         clientInfo_t *clientInfo,
                         uint8_t *dObjVersion);
void G_CheckForPreventFriendlyFire(gentity_t *ent);
void G_CheckForCursorHints(gentity_t *ent);
void G_AddEvent(gentity_t *ent, int event, int eventParm);

/* NOT_FROM_ORIGINAL_SOURCE: gated archive/follow diagnostic for live repros. */
static qboolean game_compat_client_frame_should_log_archive(int minLevel, int *lastLogTime)
{
    if (g_debugArchiveCheck.integer < minLevel) {
        return qfalse;
    }
    if (g_debugArchiveCheck.integer >= 5) {
        return qtrue;
    }
    if (*lastLogTime == 0 ||
        level.time - *lastLogTime >= DEBUG_ARCHIVE_THROTTLE_MS) {
        *lastLogTime = level.time;
        return qtrue;
    }
    return qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: gated archive/follow diagnostic for live repros. */
static void game_compat_client_frame_log_archive_client_state(const char *label, int clientNum,
                                              const gclient_t *client,
                                              int result)
{
    G_Printf("archive_debug %s time=%d client=%d result=%d "
             "connected=%d session=%d psClient=%d pmType=%d "
             "psFlags=0x%x esFlags=0x%x follow=%d archive=%d "
             "archiveTime=%d lastCmd=%d lastUsercmd=%d "
             "origin=(%.1f %.1f %.1f)\n",
             label, level.time, clientNum, result,
             client->connectedState, client->sessionState,
             client->ps.psClientNum, client->ps.pmType,
             client->ps.playerStateFlags, client->ps.entityStateFlags,
             client->followClient, client->archiveClient, client->archiveTime,
             client->ps.commandTime, client->lastUsercmdTime,
             client->ps.psOrigin[0], client->ps.psOrigin[1],
             client->ps.psOrigin[2]);
}

/* NOT_FROM_ORIGINAL_SOURCE: gated archive/follow diagnostic for live repros. */
static void game_compat_client_frame_log_archive_player_state(const char *label, int clientNum,
                                              const playerState_t *ps)
{
    G_Printf("archive_debug %s time=%d client=%d psClient=%d pmType=%d "
             "psFlags=0x%x esFlags=0x%x lastCmd=%d "
             "origin=(%.1f %.1f %.1f)\n",
             label, level.time, clientNum, ps->psClientNum, ps->pmType,
             ps->playerStateFlags, ps->entityStateFlags, ps->commandTime,
             ps->psOrigin[0], ps->psOrigin[1], ps->psOrigin[2]);
}
void FireWeapon(gentity_t *ent);
void FireWeaponMelee(gentity_t *ent);
gentity_t *Drop_Weapon(gentity_t *ent, int weapon, const char *tagName);
void turret_think_client(gentity_t *ent);
void G_DObjCalcPose(gentity_t *ent);
void StopFollowing(gentity_t *ent);
void ClientThink_real(gentity_t *ent, usercmd_t *command);

static const char CLIENT_EVENTS_EMPTY_ATTACH_TAG[] = "";
static const char CLIENT_EVENTS_DROP_TAG_RIGHT[] = "tag_weapon_right";

/* NOT_FROM_ORIGINAL_SOURCE: local float absolute-value helper for stuck-client shape tests. */
static float game_compat_abs_float(float value)
{
    return value < 0.0f ? -value : value;
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring for non-PVS client ring wrap. */
static int game_compat_client_end_frame_wrap_non_pvs_client_index(int clientNum)
{
    int wrapped = clientNum % CLIENTEND_NONPVS_SCAN_CLIENTS;

    return wrapped < 0 ? wrapped + CLIENTEND_NONPVS_SCAN_CLIENTS : wrapped;
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring for non-PVS offset clamp. */
static int game_compat_client_end_frame_clamp_non_pvs_offset(int value)
{
    if (value > CLIENTEND_NONPVS_OFFSET_LIMIT_POS) {
        return CLIENTEND_NONPVS_OFFSET_LIMIT_POS;
    }
    if (value < CLIENTEND_NONPVS_OFFSET_LIMIT_NEG) {
        return CLIENTEND_NONPVS_OFFSET_LIMIT_NEG;
    }
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring for non-PVS packed offset fields. */
static uint32_t game_compat_client_end_frame_pack_non_pvs_offset(int value, uint32_t shift)
{
    int quarterOffset = (value + 2) / 4;

    return ((uint32_t)(quarterOffset + CLIENTEND_NONPVS_OFFSET_CENTER) &
            CLIENTEND_NONPVS_OFFSET_MASK)
           << shift;
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring for StuckInClient contents predicate. */
static qboolean game_compat_stuck_client_contents(const gentity_t *ent)
{
    int contents = ent->scriptContents;

    return contents == CONTENTS_BODY || contents == CONTENTS_STUCK_ALT;
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring for StuckInClient AABB overlap test. */
static qboolean game_compat_stuck_bounds_overlap(const gentity_t *a, const gentity_t *b)
{
    /* Each stock rejection is an ordered `ja`; unordered inputs fall through
     * as overlapping rather than behaving like a source-level `<=`. */
    return !(b->absMin[0] > a->absMax[0]) &&
           !(a->absMin[0] > b->absMax[0]) &&
           !(b->absMin[1] > a->absMax[1]) &&
           !(a->absMin[1] > b->absMax[1]) &&
           !(b->absMin[2] > a->absMax[2]) &&
           !(a->absMin[2] > b->absMax[2]);
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring for StuckInClient capsule flag test. */
static qboolean game_compat_stuck_entity_uses_capsule(const gentity_t *ent)
{
    return (ent->svFlags & STUCK_ENTITY_CAPSULE_FLAG) != 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring for StuckInClient capsule-vs-box test. */
static qboolean game_compat_stuck_capsule_box_overlap(const gentity_t *capsule,
                                       const gentity_t *box,
                                       float deltaX,
                                       float deltaY)
{
    float normal[2];
    float adjustedX;
    float adjustedY;

    normal[0] = deltaX;
    normal[1] = deltaY;
    VectorNormalize2D(normal);

    /* The binary computes maxs[0] * -1.0f * normal[k] + delta (0x42863,
     * 0x4287f); value-identical to delta - maxs[0]*normal[k], form kept for
     * instruction-stream parity. */
    /* Stock 0x42863..0x42879: (maxs[0]*-1.0)*normal[k] + delta kept 80-bit, one
     * store -> shim. */
#if EMULATE_X87
    adjustedX = x87f_store_f32(x87f_add(
        x87f_mul(x87f_mul(x87f_load_f32(capsule->maxs[0]), x87f_load_f32(-1.0f)),
                 x87f_load_f32(normal[0])),
        x87f_load_f32(deltaX)));
    adjustedY = x87f_store_f32(x87f_add(
        x87f_mul(x87f_mul(x87f_load_f32(capsule->maxs[0]), x87f_load_f32(-1.0f)),
                 x87f_load_f32(normal[1])),
        x87f_load_f32(deltaY)));
#else
    adjustedX = (float)(((long double)capsule->maxs[0] * -1.0L) *
                        (long double)normal[0] + (long double)deltaX);
    adjustedY = (float)(((long double)capsule->maxs[0] * -1.0L) *
                        (long double)normal[1] + (long double)deltaY);
#endif

    /* Stock rejects only when both ordered greater-than tests succeed. */
    return !(game_compat_abs_float(adjustedX) > box->maxs[0]) ||
           !(game_compat_abs_float(adjustedY) > box->maxs[1]);
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring for StuckInClient shape dispatch. */
static qboolean game_compat_stuck_shapes_overlap(const gentity_t *ent, const gentity_t *other)
{
    qboolean entCapsule = game_compat_stuck_entity_uses_capsule(ent);
    qboolean otherCapsule = game_compat_stuck_entity_uses_capsule(other);
    float dx;
    float dy;

    if (!entCapsule && !otherCapsule) {
        return 1;
    }

    dx = other->currentOrigin[0] - ent->currentOrigin[0];
    dy = other->currentOrigin[1] - ent->currentOrigin[1];

    if (entCapsule && otherCapsule) {
        float radius = ent->maxs[0] + other->maxs[0];

        /* dx*dx+dy*dy vs radius*radius kept 80-bit, full-width compare -> shim */
#if EMULATE_X87
        return x87f_le(
            x87f_add(x87f_mul(x87f_load_f32(dx), x87f_load_f32(dx)),
                     x87f_mul(x87f_load_f32(dy), x87f_load_f32(dy))),
            x87f_mul(x87f_load_f32(radius), x87f_load_f32(radius)));
#else
        long double distanceSquared =
            (long double)dx * (long double)dx +
            (long double)dy * (long double)dy;
        long double radiusSquared =
            (long double)radius * (long double)radius;

        return !(distanceSquared > radiusSquared);
#endif
    }

    if (entCapsule) {
        return game_compat_stuck_capsule_box_overlap(ent, other, dx, dy);
    }

    return game_compat_stuck_capsule_box_overlap(other, ent, -dx, -dy);
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring for StuckInClient speed selection. */
static float game_compat_stuck_client_move_speed(gclient_t *client)
{
    /* The binary tests (float)CoduoLibm_Sqrt((double)(vx*vx + vy*vy)) > 0.0f
     * (0x42a34..0x42a6a): the planar speed rounds to double at the sqrt call
     * boundary and to float at the result store. */
#if EMULATE_X87
    float speed2D = (float)CoduoLibm_Sqrt(x87f_store_f64(x87f_add(
        x87f_mul(x87f_load_f32(client->ps.velocity[0]),
                 x87f_load_f32(client->ps.velocity[0])),
        x87f_mul(x87f_load_f32(client->ps.velocity[1]),
                 x87f_load_f32(client->ps.velocity[1])))));
#else
    long double speedSquared =
        (long double)client->ps.velocity[0] *
            (long double)client->ps.velocity[0] +
        (long double)client->ps.velocity[1] *
            (long double)client->ps.velocity[1];
    float speed2D = (float)CoduoLibm_Sqrt((double)speedSquared);
#endif

    if (speed2D > 0.0f) {
        return client->ps.speed;
    }

    return 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring for StuckInClient velocity/pmTime writes. */
static void game_compat_stuck_apply_push(gclient_t *client, const float *dir, float speed)
{
    client->ps.velocity[0] = dir[0] * speed;
    client->ps.velocity[1] = dir[1] * speed;
    client->ps.pmTime = STUCK_PM_TIME;
    client->ps.playerStateFlags |= STUCK_CLIENT_PUSH_FLAG;
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring for spectator inactivity cvar refresh. */
static void game_compat_update_sv_private_clients(void)
{
    if (sv_privateClients.handle == 0) {
        trap_Cvar_Register(&sv_privateClients, "sv_privateClients", "0",
                           CVAR_SERVERINFO);
    } else {
        trap_Cvar_Update(&sv_privateClients);
    }
}

/* 0x3f65a P_DamageFeedback */
/* VERIFIED_DECOMPILER(0x3f65a, 4f65a_P_DamageFeedback.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - pmType/damage/max-health gates, percent clamp, damage alpha clamp, kick min/max, vectoangles/axis stores, yaw byte at +0x110, pitch byte at +0x114, world fallback, pain event gate, and counter clears checked. */
void P_DamageFeedback(gentity_t *ent)
{
    gclient_t *client = ent->client;
    int damageTaken = client->damageTaken;

    /*
     * RECOVERED(UO-GAME-UNK-0058): damage feedback now uses typed client fields; the
     * event id range and feedback constants remain preserved from static evidence.
     */
    if (client->ps.pmType >= PM_TYPE_DEAD || damageTaken <= 0 ||
        client->normalMaxHealth <= 0) {
        return;
    }

    damageTaken = coduo_int32_from_bits((uint32_t)damageTaken * UINT32_C(100)) /
                  client->normalMaxHealth;
    if (damageTaken > DAMAGE_FEEDBACK_MAX_COUNT) {
        damageTaken = DAMAGE_FEEDBACK_MAX_COUNT;
    }

    client->ps.aimSpreadScale =
        (float)((long double)damageTaken +
                (long double)client->ps.aimSpreadScale);
    if (client->ps.aimSpreadScale > DAMAGE_FEEDBACK_ALPHA_MAX) {
        client->ps.aimSpreadScale = DAMAGE_FEEDBACK_ALPHA_MAX;
    }

    float kick = (float)((long double)damageTaken *
                         (long double)DAMAGE_FEEDBACK_KICK_SCALE);
    if (kick < DAMAGE_FEEDBACK_KICK_MIN) {
        kick = DAMAGE_FEEDBACK_KICK_MIN;
    } else if (kick > DAMAGE_FEEDBACK_KICK_MAX) {
        kick = DAMAGE_FEEDBACK_KICK_MAX;
    }

    if (client->damageFromWorld == 0) {
        const float *damageFrom = client->damageFrom;
        vec3_t damageAngles;
        axis_t axis;

        vectoangles(damageFrom, damageAngles);
        AnglesToAxis(client->ps.viewAngles, axis);

        /* Stock 0x3f7f4..0x3f823: the damageFrom.axis dot is a 3-mul/2-add chain
         * kept 80-bit, multiplied by (-)kick, one store -> shim. */
#if EMULATE_X87
        client->damageRoll = x87f_store_f32(x87f_mul(
            x87f_neg(x87f_load_f32(kick)),
            x87f_add(x87f_add(x87f_mul(x87f_load_f32(damageFrom[0]), x87f_load_f32(axis[1][0])),
                              x87f_mul(x87f_load_f32(damageFrom[1]), x87f_load_f32(axis[1][1]))),
                     x87f_mul(x87f_load_f32(damageFrom[2]), x87f_load_f32(axis[1][2])))));
        client->damagePitch = x87f_store_f32(x87f_mul(
            x87f_load_f32(kick),
            x87f_add(x87f_add(x87f_mul(x87f_load_f32(damageFrom[0]), x87f_load_f32(axis[0][0])),
                              x87f_mul(x87f_load_f32(damageFrom[1]), x87f_load_f32(axis[0][1]))),
                     x87f_mul(x87f_load_f32(damageFrom[2]), x87f_load_f32(axis[0][2])))));
#else
        long double rollDot =
            ((long double)damageFrom[0] * (long double)axis[1][0] +
             (long double)damageFrom[1] * (long double)axis[1][1]) +
            (long double)damageFrom[2] * (long double)axis[1][2];
        long double pitchDot =
            ((long double)damageFrom[0] * (long double)axis[0][0] +
             (long double)damageFrom[1] * (long double)axis[0][1]) +
            (long double)damageFrom[2] * (long double)axis[0][2];

        client->damageRoll = (float)(-(long double)kick * rollDot);
        client->damagePitch = (float)((long double)kick * pitchDot);
#endif
        /* Stock 0x3f86b..0x3f884: (angle/circle)*scale kept 80-bit, fistp with
         * RC=truncate DIRECTLY (no float rounding of the scaled angle). */
#if EMULATE_X87
        client->ps.damagePitch = x87f_store_i32_trunc(x87f_mul(
            x87f_div(x87f_load_f32(damageAngles[0]),
                     x87f_load_f32(DAMAGE_FEEDBACK_DEGREES_PER_CIRCLE)),
            x87f_load_f32(DAMAGE_FEEDBACK_BYTE_SCALE)));
        client->ps.damageYaw = x87f_store_i32_trunc(x87f_mul(
            x87f_div(x87f_load_f32(damageAngles[1]),
                     x87f_load_f32(DAMAGE_FEEDBACK_DEGREES_PER_CIRCLE)),
            x87f_load_f32(DAMAGE_FEEDBACK_BYTE_SCALE)));
#else
        client->ps.damagePitch =
            game_compat_int32_from_long_double_trunc(
                ((long double)damageAngles[0] /
                 (long double)DAMAGE_FEEDBACK_DEGREES_PER_CIRCLE) *
                (long double)DAMAGE_FEEDBACK_BYTE_SCALE);
        client->ps.damageYaw =
            game_compat_int32_from_long_double_trunc(
                ((long double)damageAngles[1] /
                 (long double)DAMAGE_FEEDBACK_DEGREES_PER_CIRCLE) *
                (long double)DAMAGE_FEEDBACK_BYTE_SCALE);
#endif
    } else {
        client->damageRoll = 0.0f;
        client->damagePitch = -kick;
        client->ps.damagePitch = 0xff;
        client->ps.damageYaw = 0xff;
        client->damageFromWorld = 0;
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (ent->painEventTime < level.time &&
        (ent->flags & ENTITY_FLAG_GODMODE) == 0) {
        /* The binary truncates (fistp with RC=truncate at 0x3f90c) straight
         * from the x87 register; the fild operands feed the divide with no
         * intermediate float store. */
#if EMULATE_X87
        int healthPercent = x87f_store_i32_trunc(x87f_mul(
            x87f_div(x87f_load_i32(client->ps.stats[STAT_HEALTH]),
                     x87f_load_i32(client->ps.stats[STAT_MAX_HEALTH])),
            x87f_load_f32(100.0f)));
#else
        int healthPercent =
            game_compat_int32_from_long_double_trunc(
                ((long double)client->ps.stats[STAT_HEALTH] /
                 (long double)client->ps.stats[STAT_MAX_HEALTH]) * 100.0L);
#endif

        if (healthPercent < 0) {
            healthPercent = 0;
        } else if (healthPercent > 100) {
            healthPercent = 100;
        }
        G_AddEvent(ent, EV_PAIN, healthPercent);
        ent->painEventTime = coduo_int32_from_bits(
            (uint32_t)level.time + DAMAGE_FEEDBACK_EVENT_INTERVAL_MS);
    }

    client->ps.damageEvent = coduo_int32_from_bits(
        (uint32_t)client->ps.damageEvent + UINT32_C(1));
    client->damageTime = coduo_int32_from_bits(
        (uint32_t)level.time - DAMAGE_FEEDBACK_TIME_BACKDATE_MS);
    client->ps.damageCount = damageTaken;
    client->damageTaken = 0;
}

/* 0x3f4d4 G_CheckFlameDamage */
/* VERIFIED_DECOMPILER(0x3f4d4, 4f4d4_G_CheckFlameDamage.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - cached flamethrower weapon index, active/window gates, inflictor client gate, direction normalize, flame damage field, self/nonself scale, and G_Damage arguments checked. */
void G_CheckFlameDamage(gentity_t *ent)
{
    if (flameWeaponIndex < 0) {
        flameWeaponIndex = BG_GetWeaponIndexForName("flamethrower_mp") & 0xff;
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((ent->client->spectatorWbuttons & 1) != 0 &&
        ent->client->flameDamageTime > coduo_int32_from_bits(
            (uint32_t)level.time - FLAME_DAMAGE_WINDOW_MS)) {
        gentity_t *inflictor = &g_entities[ent->client->flameDamageInflictor];

        if (inflictor->client != 0) {
            vec3_t dir;
            const weaponInfo_t *weaponInfo;
            int damage;

            dir[0] = ent->client->ps.psOrigin[0] - inflictor->client->ps.psOrigin[0];
            dir[1] = ent->client->ps.psOrigin[1] - inflictor->client->ps.psOrigin[1];
            dir[2] = ent->client->ps.psOrigin[2] - inflictor->client->ps.psOrigin[2];
            VectorNormalize(dir);

            weaponInfo = BG_GetInfoForWeapon(flameWeaponIndex);
            /*
             * RECOVERED(UO-GAME-UNK-0022): client flame active bit and weapon-info
             * flame damage offset remain source-name recovered.
             */
            damage = weaponInfo->flameDamage;
            if (ent != inflictor) {
                damage = coduo_int32_from_bits((uint32_t)damage << 2U);
            }

            G_Damage(ent, inflictor, inflictor, dir, inflictor->client->ps.psOrigin,
                     damage, 0, MOD_FLAME, 0);
        }
    }
}

/* 0x3f9a8 G_SetClientSound */
/* VERIFIED_DECOMPILER(0x3f9a8, 4f9a8_G_SetClientSound.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - single clientSound word clear at gentity +0x84 checked. */
void G_SetClientSound(gentity_t *ent)
{
    ent->s.clientSound = 0;
}

/* 0x3f9ba ClientImpacts */
/* VERIFIED_DECOMPILER(0x3f9ba, 4f9ba_ClientImpacts.c, VERIFY-WAVE2-CLIENT-FRAME-STATE-2026-06-17): DATAFLOW_VERIFIED - pmove touch-count/list offsets, duplicate suppression, script notify order, and both entity touch callbacks match the generated dataflow. */
void ClientImpacts(gentity_t *ent, const pmove_t *trace)
{
    int impactCount = trace->numtouch;

    for (int impactIndex = 0; impactIndex < impactCount; impactIndex++) {
        int entityNum = trace->impactEntityNums[impactIndex];
        int previous;

        for (previous = 0;
             previous < impactIndex &&
             trace->impactEntityNums[previous] != entityNum;
             previous++) {
        }

        if (previous == impactIndex) {
            gentity_t *other = &g_entities[entityNum];

            if (Scr_IsSystemActive(1) != 0) {
                Scr_AddEntity(other);
                Scr_Notify(ent, scr_const_touch, 1);
                Scr_AddEntity(ent);
                Scr_Notify(other, scr_const_touch, 1);
            }

            if (other->touch != 0) {
                other->touch(other, ent, CLIENT_IMPACT_TRACE_MODE);
            }
            if (ent->touch != 0) {
                ent->touch(ent, other, CLIENT_IMPACT_TRACE_MODE);
            }
        }
    }
}

/* 0x3fb15 G_TouchTriggers */
/* VERIFIED_DECOMPILER(0x3fb15, 4fb15_G_TouchTriggers.c, VERIFY-WAVE2-CLIENT-FRAME-STATE-2026-06-17): DATAFLOW_VERIFIED - live-client/pmType gate, inflated trigger query, item/contact tests, script notify order, and other-entity touch callback match generated output. */
void G_TouchTriggers(gentity_t *ent)
{
    gclient_t *client = ent->client;
    vec3_t queryMins;
    vec3_t queryMaxs;
    vec3_t contactMins;
    vec3_t contactMaxs;
    int touchList[MAX_GENTITIES];

    if (client == 0 || client->ps.pmType > PM_TYPE_LINKED) {
        return;
    }

    /*
     * RECOVERED(UO-GAME-UNK-0059): trigger query inflation and content mask are
     * preserved from the binary until broader collision/content definitions are
     * recovered.
     */
    queryMins[0] = client->ps.psOrigin[0] - TOUCH_TRIGGERS_BOUNDS_XY;
    queryMins[1] = client->ps.psOrigin[1] - TOUCH_TRIGGERS_BOUNDS_XY;
    queryMins[2] = client->ps.psOrigin[2] - TOUCH_TRIGGERS_BOUNDS_Z;
    queryMaxs[0] = client->ps.psOrigin[0] + TOUCH_TRIGGERS_BOUNDS_XY;
    queryMaxs[1] = client->ps.psOrigin[1] + TOUCH_TRIGGERS_BOUNDS_XY;
    queryMaxs[2] = client->ps.psOrigin[2] + TOUCH_TRIGGERS_BOUNDS_Z;

    int touchCount = trap_EntitiesInBox(queryMins, queryMaxs, touchList,
                                        (int)MAX_GENTITIES,
                                        TOUCH_TRIGGERS_CONTENTS_MASK);

    contactMins[0] = client->ps.psOrigin[0] + ent->mins[0];
    contactMins[1] = client->ps.psOrigin[1] + ent->mins[1];
    contactMins[2] = client->ps.psOrigin[2] + ent->mins[2];
    contactMaxs[0] = client->ps.psOrigin[0] + ent->maxs[0];
    contactMaxs[1] = client->ps.psOrigin[1] + ent->maxs[1];
    contactMaxs[2] = client->ps.psOrigin[2] + ent->maxs[2];

    for (int touchIndex = 0; touchIndex < touchCount; touchIndex++) {
        gentity_t *other = &g_entities[touchList[touchIndex]];
        int touched;

        if (other->touch == 0 && ent->touch == 0) {
            continue;
        }

        if (other->s.eType == ET_ITEM) {
            touched = BG_PlayerTouchesItem(client, other, level.time);
        } else {
            touched = trap_EntityContact(contactMins, contactMaxs, other);
        }

        if (touched == 0) {
            continue;
        }

        if (Scr_IsSystemActive(TOUCH_TRIGGERS_SCRIPT_SYSTEM) != 0) {
            Scr_AddEntity(ent);
            Scr_Notify(other, scr_const_touch, 1);
            Scr_AddEntity(other);
            Scr_Notify(ent, scr_const_touch, 1);
        }

        if (other->touch != 0) {
            other->touch(other, ent, TOUCH_TRIGGERS_TRACE_MODE);
        }
    }
}

/* 0x40543 G_DoTouchTriggers */
/* VERIFIED_DECOMPILER(0x40543, 50543_G_DoTouchTriggers.c, VERIFY-WAVE2-CLIENT-FRAME-STATE-2026-06-17): DATAFLOW_VERIFIED - vehicle/client contents mask selection, explicit-origin query bounds, item client gate, capsule contact, and other-entity touch callback match generated output. */
void G_DoTouchTriggers(gentity_t *ent, const float *origin)
{
    vec3_t queryMins;
    vec3_t queryMaxs;
    vec3_t contactMins;
    vec3_t contactMaxs;
    int touchList[MAX_GENTITIES];
    int contentMask;

    if (ent->vehicle != 0) {
        contentMask = DO_TOUCH_TRIGGERS_VEHICLE_CONTENTS_MASK;
    } else if (ent->client != 0) {
        contentMask = DO_TOUCH_TRIGGERS_CLIENT_CONTENTS_MASK;
    } else {
        return;
    }

    /*
     * RECOVERED(UO-GAME-UNK-0059): this explicit-origin variant shares the trigger
     * query inflation and bounds offsets with `G_TouchTriggers`, but uses a different
     * contents mask based on vehicle/client ownership and capsule contact for
     * non-item candidates.
     */
    queryMins[0] = origin[0] - TOUCH_TRIGGERS_BOUNDS_XY;
    queryMins[1] = origin[1] - TOUCH_TRIGGERS_BOUNDS_XY;
    queryMins[2] = origin[2] - TOUCH_TRIGGERS_BOUNDS_Z;
    queryMaxs[0] = origin[0] + TOUCH_TRIGGERS_BOUNDS_XY;
    queryMaxs[1] = origin[1] + TOUCH_TRIGGERS_BOUNDS_XY;
    queryMaxs[2] = origin[2] + TOUCH_TRIGGERS_BOUNDS_Z;

    int touchCount = trap_EntitiesInBox(queryMins, queryMaxs, touchList,
                                        (int)MAX_GENTITIES, contentMask);

    contactMins[0] = origin[0] + ent->mins[0];
    contactMins[1] = origin[1] + ent->mins[1];
    contactMins[2] = origin[2] + ent->mins[2];
    contactMaxs[0] = origin[0] + ent->maxs[0];
    contactMaxs[1] = origin[1] + ent->maxs[1];
    contactMaxs[2] = origin[2] + ent->maxs[2];

    for (int touchIndex = 0; touchIndex < touchCount; touchIndex++) {
        gentity_t *other = &g_entities[touchList[touchIndex]];
        int touched;

        if (other->touch == 0 && ent->touch == 0) {
            continue;
        }

        if (other->s.eType == ET_ITEM) {
            if (ent->client == 0) {
                continue;
            }
            touched = BG_PlayerTouchesItem(ent->client, other, level.time);
        } else {
            touched = trap_EntityContactCapsule(contactMins, contactMaxs, other);
        }

        if (touched == 0) {
            continue;
        }

        if (Scr_IsSystemActive(TOUCH_TRIGGERS_SCRIPT_SYSTEM) != 0) {
            Scr_AddEntity(ent);
            Scr_Notify(other, scr_const_touch, 1);
            Scr_AddEntity(other);
            Scr_Notify(ent, scr_const_touch, 1);
        }

        if (other->touch != 0) {
            other->touch(other, ent, TOUCH_TRIGGERS_TRACE_MODE);
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring for ClientEvents attach/detach cases. */
static void game_compat_client_events_handle_weapon_view_model(gentity_t *ent, int weapon,
                                              qboolean attach)
{
    const char *modelName =
        ((weaponInfo_t *)BG_GetInfoForWeapon(weapon))->viewModel;

    if (modelName == 0 || *modelName == '\0') {
        return;
    }

    if (attach) {
        G_EntAttach(ent, modelName, CLIENT_EVENTS_EMPTY_ATTACH_TAG, 0);
    } else {
        G_EntDetach(ent, modelName, CLIENT_EVENTS_EMPTY_ATTACH_TAG);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring for ClientEvents damage range cases. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original ClientEvents (0x40860); no standalone original body. */
static void game_compat_client_events_handle_damage_event(gentity_t *ent, gclient_t *client,
                                          int eventParm)
{
    float damageScale;
    if (eventParm < 100) {
        damageScale = (float)eventParm * CLIENT_EVENTS_DAMAGE_PERCENT_SCALE;
    } else {
        damageScale = CLIENT_EVENTS_DAMAGE_FULL_SCALE;
    }

    if (damageScale == 0.0f) {
        return;
    }

    /* Stock stores the fild(maxHealth) * damageScale product to float before
     * reloading it for the truncating fistp argument conversion. */
    float damage = (float)((long double)damageScale *
                           (long double)client->ps.stats[STAT_MAX_HEALTH]);
    ent->painEventTime = coduo_int32_from_bits(
        (uint32_t)level.time + CLIENT_EVENTS_PAIN_DELAY_MS);
    /* VERIFIED_DECOMPILER(0x40860, 50860_ClientEvents.c, VERIFY-WAVE2-CLIENT-FRAME-STATE-2026-06-17): DATAFLOW_VERIFIED - fall-damage scale, pain delay, max-health conversion, and MOD_FALLING call arguments match generated output. Binary uses x87 fistp with RC=truncate for the scaled fall damage. */
    G_Damage(ent, 0, 0, 0, 0,
             game_compat_int32_from_float_trunc(damage), 0,
             CLIENT_EVENTS_FALL_DAMAGE_MOD, 0);
}

/* 0x40860 ClientEvents */
/* VERIFIED_DECOMPILER(0x40860, 50860_ClientEvents.c, VERIFY-WAVE2-CLIENT-FRAME-STATE-2026-06-17): DATAFLOW_VERIFIED - event backlog clamp, ring dispatch, damage return path, weapon attach/detach/fire/drop cases, and suicide stores/call arguments match generated output. */
void ClientEvents(gentity_t *ent, uint32_t oldEventSequence)
{
    gclient_t *client = ent->client;
    int32_t eventSequence = client->ps.eventIndex;
    int32_t sequence = coduo_int32_from_bits(oldEventSequence);
    int32_t backlogStart = coduo_int32_from_bits(
        (uint32_t)eventSequence - CLIENT_EVENTS_MAX_BACKLOG);

    /*
     * RECOVERED(UO-GAME-UNK-0062): player-state event ring offsets and event ids are
     * preserved from the binary until the shared event enum/player-state layout is
     * recovered.
     */
    if (sequence < backlogStart) {
        sequence = backlogStart;
    }

    while (sequence < eventSequence) {
        int event = (int)((const uint32_t *)(const void *)client->ps.events)
            [(uint32_t)sequence & CLIENT_EVENTS_RING_MASK];
        int eventParm =
            client->ps.eventParms
                [(uint32_t)sequence & CLIENT_EVENTS_RING_MASK];

        if (event >= CLIENT_EVENTS_DAMAGE_MIN &&
            event <= CLIENT_EVENTS_DAMAGE_MAX) {
            if (ent->s.eType != ET_PLAYER) {
                return;
            }
            game_compat_client_events_handle_damage_event(ent, client, eventParm);
            sequence = coduo_int32_from_bits(
                (uint32_t)sequence + UINT32_C(1));
            continue;
        }

        switch (event) {
        case CLIENT_EVENTS_WEAPON_RAISE:
            game_compat_client_events_handle_weapon_view_model(
                ent, ent->s.weapon, 1);
            break;

        case CLIENT_EVENTS_WEAPON_LOWER:
            game_compat_client_events_handle_weapon_view_model(
                ent, ent->s.weapon, 0);
            break;

        case CLIENT_EVENTS_FIRE_BEGIN:
        case CLIENT_EVENTS_FIRE_LASTSHOT:
        case CLIENT_EVENTS_FIRE_ALT:
        case CLIENT_EVENTS_FIRE_LASTSHOT_ALT:
        case CLIENT_EVENTS_FIRE_MOUNTED:
            FireWeapon(ent);
            break;

        case CLIENT_EVENTS_MELEE:
            FireWeaponMelee(ent);
            break;

        case CLIENT_EVENTS_DETACH_WEAPON:
            game_compat_client_events_handle_weapon_view_model(ent, eventParm, 0);
            break;

        case CLIENT_EVENTS_DROP_WEAPON:
            if (client->ps.currentWeapon != 0 &&
                Com_BitCheck(client->ps.weaponBits, client->ps.currentWeapon) != 0) {
                Drop_Weapon(ent, client->ps.currentWeapon,
                            CLIENT_EVENTS_DROP_TAG_RIGHT);
            }
            break;

        case CLIENT_EVENTS_SUICIDE:
            if (ent->client != 0 && (ent->flags & ENTITY_FLAG_GODMODE) == 0) {
                ent->health = 0;
                ent->client->ps.stats[STAT_HEALTH] = 0;
                player_die(ent, ent, ent, SUICIDE_DAMAGE, MOD_SUICIDE, 0, 0, 0);
            }
            break;

        default:
            break;
        }

        sequence = coduo_int32_from_bits((uint32_t)sequence + UINT32_C(1));
    }
}

/* 0x40c1c G_SetClientContents */
/* VERIFIED_DECOMPILER(0x40c1c, 50c1c_G_SetClientContents.c, VERIFY-WAVE2-CLIENT-FRAME-STATE-2026-06-17): DATAFLOW_VERIFIED - noclip, ufo, dead-session gates clear contents; otherwise body contents write matches generated output. */
void G_SetClientContents(gentity_t *ent)
{
    gclient_t *client = ent->client;

    /*
     * Client contents are cleared for non-colliding client modes and otherwise use
     * the recovered body contents mask.
     */
    if (client->noclip != 0 || client->ufo != 0 ||
        client->sessionState == SESS_STATE_DEAD) {
        ent->scriptContents = 0;
    } else {
        ent->scriptContents = CONTENTS_BODY;
    }
}

/* 0x41d13 ClientThink */
/* VERIFIED_DECOMPILER(0x41d13, 51d13_ClientThink.c, VERIFY-WAVE2-CLIENT-FRAME-STATE-2026-06-17): DATAFLOW_VERIFIED - gentity/client indexing, six-word command preservation, usercmd fetch, last-usercmd time store, and synchronous gate match generated output. */
void ClientThink(int clientNum)
{
    gentity_t *ent = &g_entities[clientNum];
    gclient_t *client = ent->client;

    /* The original six dword loads/stores are the compiler's complete
     * usercmd_t assignment, including its two padding bytes. */
    client->oldPmoveCommand = client->command;

    trap_GetUsercmd(clientNum, &client->command);
    client->lastUsercmdTime = level.time;

    if (g_synchronousClients.integer == 0) {
        ClientThink_real(ent, &client->command);
    }
}

/* 0x41dfc G_RunClient */
/* VERIFIED_DECOMPILER(0x41dfc, 51dfc_G_RunClient.c, VERIFY-WAVE2-CLIENT-FRAME-STATE-2026-06-17): DATAFLOW_VERIFIED - synchronous ClientThink_real path, noclip exit, linkInfo fixed-link setup, trajectory/link stores, psOrigin copy, and linked pmType decrement match generated output. */
void G_RunClient(gentity_t *ent)
{
    gclient_t *client = ent->client;

    if (g_synchronousClients.integer != 0) {
        client->command.commandTime = level.time;
        ClientThink_real(ent, &client->command);
    }

    if (client->noclip != 0) {
        return;
    }

    /*
     * RECOVERED(UO-GAME-UNK-0065): linked clients are forced through fixed-link mode 2
     * and temporary pmType values 1/7 until the full entity link/client run loop is
     * recovered and these source-level names can be confirmed.
     */
    if (ent->linkInfo != 0) {
        client->ps.pmType = client->sessionState == SESS_STATE_DEAD
                             ? PM_TYPE_LINKED_DEAD
                             : PM_TYPE_LINKED;
        G_SetFixedLink(ent, RUNCLIENT_FIXED_LINK_MODE);
        G_SetOrigin(ent, ent->currentOrigin);
        ent->s.pos.trType = TR_INTERPOLATE;
        ent->s.apos.trType = TR_INTERPOLATE;
        trap_LinkEntity(ent);
        client->ps.psOrigin[0] = ent->currentOrigin[0];
        client->ps.psOrigin[1] = ent->currentOrigin[1];
        client->ps.psOrigin[2] = ent->currentOrigin[2];
    } else if (client->ps.pmType == PM_TYPE_LINKED ||
               client->ps.pmType == PM_TYPE_LINKED_DEAD) {
        client->ps.pmType--;
    }
}

/* 0x41f6b IntermissionClientEndFrame */
/* VERIFIED_DECOMPILER(0x41f6b, 51f6b_IntermissionClientEndFrame.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - svFlags clear/set stores, takeDamage/scriptContents clears, stance/ps flag masks, pmType, end-frame reset, and intermission eType checked. */
void IntermissionClientEndFrame(gentity_t *ent)
{
    gclient_t *client = ent->client;

    /*
     * RECOVERED(UO-GAME-UNK-0066): intermission end-frame clears several
     * still-unnamed entity/client flags while forcing the client into a passive
     * intermission presentation state.
     */
    ent->svFlags =
        (ent->svFlags & ~INTERMISSION_ENTITY_FLAGS_CLEAR) |
        INTERMISSION_ENTITY_FLAGS_SET;
    ent->takeDamage = 0;
    ent->scriptContents = 0;

    client->ps.playerStateFlags &= ~INTERMISSION_STANCE_FLAGS_CLEAR;
    client->ps.pmType = PM_TYPE_INTERMISSION;
    client->ps.entityStateFlags &= ~INTERMISSION_PS_FLAGS_CLEAR;
    client->ps.viewModelIndex = 0;
    ent->s.eType = ET_INVISIBLE;
}

/* 0x42020 SpectatorClientEndFrame */
/* VERIFIED_DECOMPILER(0x42020, 52020_SpectatorClientEndFrame.c, VERIFY-WAVE2-CLIENT-FRAME-STATE-2026-06-17): DATAFLOW_VERIFIED - spectator presentation reset, archive rewind/fallback, team gates, follow flag updates, archived prefix copy, and preserved entityStateFlags match generated output. */
void SpectatorClientEndFrame(gentity_t *ent)
{
    gclient_t *client = ent->client;
    playerState_t archivedPlayerState;
    clientState_t archiveMeta = { 0 };
    int archivedClient;
    int clientNum = (int)(ent - g_entities);

    if (game_compat_client_frame_should_log_archive(1,
            &debugArchiveLastSpectatorLog[(uint32_t)clientNum])) {
        game_compat_client_frame_log_archive_client_state(DEBUG_LABEL("spectator_begin"), clientNum, client, 0);
    }

    /*
     * RECOVERED(UO-GAME-UNK-0068): spectator end-frame shares intermission-style entity
     * presentation setup, then may replace the client/player-state prefix with archived
     * follow data. Archive payload layout remains recovered.
     */
    ent->svFlags =
        (ent->svFlags & ~INTERMISSION_ENTITY_FLAGS_CLEAR) |
        INTERMISSION_ENTITY_FLAGS_SET;
    ent->takeDamage = 0;
    ent->scriptContents = 0;
    client->ps.playerStateFlags &= ~PSF_ACTIVE_PLAYER;
    ent->s.eType = ET_INVISIBLE;
    client->ps.viewModelIndex = 0;
    client->spectatorSnapshotAngle0 = 0;
    client->spectatorSnapshotAngle1 = 0;

    if (client->followClient >= 0) {
        archivedClient = client->followClient;
        client->archiveClient = archivedClient;

        for (;;) {
            if (client->archiveTime < 0) {
                client->archiveTime = 0;
            }

            int archiveResult =
                trap_GetArchivedClientInfo(client->followClient,
                                           &client->archiveTime,
                                           &archivedPlayerState, &archiveMeta);
            int canSpectate =
                archiveResult != 0 &&
                G_ClientCanSpectateTeam(client, archiveMeta.team) != 0;
            if (g_debugArchiveCheck.integer >= 5) {
                G_Printf("archive_debug spectator_follow_probe time=%d "
                         "client=%d follow=%d archiveTime=%d result=%d "
                         "team=%d canSpectate=%d\n",
                         level.time, clientNum, client->followClient,
                         client->archiveTime, archiveResult, archiveMeta.team,
                         canSpectate);
            }
            if (canSpectate) {
                goto useArchivedState;
            }

            if (client->archiveTime == 0) {
                client->followClient = ARCHIVE_CLIENT_NONE;
                client->archiveClient = ARCHIVE_CLIENT_NONE;
                break;
            }

            client->archiveTime -= SPECTATOR_ARCHIVE_STEP_MS;
        }
    }

    if (client->archiveClient < 0 &&
        G_ClientCanSpectateTeam(client, SPECTATOR_FOLLOW_TEAM) == 0) {
        Cmd_FollowCycle_f(ent, 1);
    }

    archivedClient = client->archiveClient;
    int archiveResult =
        archivedClient < 0 ? 0 :
        trap_GetArchivedClientInfo(archivedClient, &client->archiveTime,
                                   &archivedPlayerState, &archiveMeta);
    int canSpectate =
        archiveResult != 0 &&
        G_ClientCanSpectateTeam(client, archiveMeta.team) != 0;
    if (g_debugArchiveCheck.integer >= 5) {
        G_Printf("archive_debug spectator_archive_probe time=%d client=%d "
                 "archive=%d archiveTime=%d result=%d team=%d "
                 "canSpectate=%d\n",
                 level.time, clientNum, archivedClient, client->archiveTime,
                 archiveResult, archiveMeta.team, canSpectate);
    }
    if (archivedClient < 0 || archiveResult == 0 || canSpectate == 0) {
        StopFollowing(ent);
        client->ps.playerStateFlags &= ~SPECTATOR_CAN_FOLLOW_TEAM_FLAG;
        if (G_ClientCanSpectateTeam(client, TEAM_ALLIES) == 0 &&
            G_ClientCanSpectateTeam(client, TEAM_AXIS) == 0 &&
            G_ClientCanSpectateTeam(client, TEAM_FREE) == 0) {
            client->ps.playerStateFlags &= ~SPECTATOR_CAN_FOLLOW_FLAG;
        } else {
            client->ps.playerStateFlags |= SPECTATOR_CAN_FOLLOW_FLAG;
        }
        return;
    }

    useArchivedState:
    {
        if (g_debugArchiveCheck.integer >= 5) {
            game_compat_client_frame_log_archive_player_state(
                DEBUG_LABEL("spectator_archive_buffer"), clientNum,
                &archivedPlayerState);
        }

        uint32_t preservedPsFlags =
            (client->ps.entityStateFlags & SPECTATOR_ARCHIVE_PRESERVE_PS_FLAG) |
            (archivedPlayerState.entityStateFlags &
             ~SPECTATOR_ARCHIVE_PRESERVE_PS_FLAG);

        memcpy(&client->ps, &archivedPlayerState, sizeof(archivedPlayerState));
        HudElem_UpdateClient(client, ent->s.number,
                             SPECTATOR_ARCHIVE_HUDELEM_UPDATE_FLAGS);
        client->ps.entityStateFlags = preservedPsFlags;
        client->ps.playerStateFlags &= ~PSF_ACTIVE_PLAYER;
        client->ps.playerStateFlags |= PSF_FOLLOWING;

        if (client->followClient < 0) {
            client->ps.playerStateFlags |= SPECTATOR_CAN_FOLLOW_FLAG;
            if (G_ClientCanSpectateTeam(client, SPECTATOR_FOLLOW_TEAM) == 0) {
                client->ps.playerStateFlags &= ~SPECTATOR_CAN_FOLLOW_TEAM_FLAG;
            } else {
                client->ps.playerStateFlags |= SPECTATOR_CAN_FOLLOW_TEAM_FLAG;
            }
        } else {
            client->ps.playerStateFlags &= ~SPECTATOR_ARCHIVE_FLAGS_CLEAR;
        }

        if (g_debugArchiveCheck.integer >= 5) {
            game_compat_client_frame_log_archive_client_state(DEBUG_LABEL("spectator_after_archive_copy"),
                                              clientNum, client, 1);
        }
    }
}

/* 0x424b0 G_ClientCanSpectateTeam */
/* VERIFIED_DECOMPILER(0x424b0, 524b0_G_ClientCanSpectateTeam.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - deny-mask shift by team & 0x1f and inverted low-bit return checked. */
qboolean G_ClientCanSpectateTeam(gclient_t *client, int team)
{
    /*
     * RESOLVED(UO-GAME-UNK-0012): spectateTeamDenyMask stores deny bits; a clear bit
     * means the client may spectate that archived-info team value.
     */
    return ((client->spectateTeamDenyMask >> ((uint32_t)team & 0x1fu)) ^ 1u) & 1u;
}

/* 0x424ca GetFollowPlayerState */
/* VERIFIED_DECOMPILER(0x424ca, 524ca_GetFollowPlayerState.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - follow-flag gate, gclient prefix copy through +0x4503 including decompiler alignment loop, and output tail clear at +0x7f8 size 0x1e84 checked. */
qboolean GetFollowPlayerState(int clientNum, void *playerState)
{
    gclient_t *client = g_entities[clientNum].client;
    qboolean result;

    /*
     * RECOVERED(UO-GAME-UNK-0067): the follow-player snapshot copies the leading
     * player-state-sized client prefix, then clears a large output-only tail region.
     */
    if ((client->ps.playerStateFlags & PSF_ACTIVE_PLAYER) == 0) {
        if (game_compat_client_frame_should_log_archive(1,
                &debugArchiveLastGetFollowLog[(uint32_t)clientNum])) {
            game_compat_client_frame_log_archive_client_state(DEBUG_LABEL("get_follow_state_blocked"),
                                              clientNum, client, 0);
        }
        return 0;
    }

    memcpy(playerState, client, FOLLOW_PLAYERSTATE_COPY_SIZE);
    memset((uint8_t *)playerState + FOLLOW_PLAYERSTATE_CLEAR_OFFSET, 0,
           FOLLOW_PLAYERSTATE_CLEAR_SIZE);
    result = 1;
    if (game_compat_client_frame_should_log_archive(2,
            &debugArchiveLastGetFollowLog[(uint32_t)clientNum])) {
        game_compat_client_frame_log_archive_client_state(DEBUG_LABEL("get_follow_state_ok"),
                                          clientNum, client, result);
    }
    return result;
}

/* 0x4258c StuckInClient */
/* VERIFIED_DECOMPILER(0x4258c, 5258c_StuckInClient.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - initial contents/session gates, client scan bounds, AABB and capsule/box overlap tests, random push jitter, speed fallback, reciprocal velocity writes, pmTime, and push flag stores checked. */
qboolean StuckInClient(gentity_t *ent)
{
    gclient_t *client = ent->client;

    /*
     * RECOVERED(UO-GAME-UNK-0069): this stuck-client resolver uses recovered collision
     * bounds and capsule flags; broader entity collision naming remains tied to that
     * evidence record.
     */
    if ((client->ps.playerStateFlags & PSF_ACTIVE_PLAYER) == 0 ||
        client->sessionState != SESS_STATE_PLAYING ||
        game_compat_stuck_client_contents(ent) == 0) {
        return 0;
    }

    for (int entityNum = 0; entityNum < level.maxclients; entityNum++) {
        gentity_t *other = &g_entities[entityNum];
        gclient_t *otherClient = other->client;

        if (other->linked == 0 || other == ent || otherClient == 0 ||
            (otherClient->ps.playerStateFlags & PSF_ACTIVE_PLAYER) == 0 ||
            otherClient->sessionState != SESS_STATE_PLAYING ||
            other->health <= 0 ||
            game_compat_stuck_client_contents(other) == 0 ||
            (other->s.eFlags & STUCK_ENTITY_SKIP_FLAG) != 0 ||
            game_compat_stuck_bounds_overlap(ent, other) == 0 ||
            game_compat_stuck_shapes_overlap(ent, other) == 0) {
            continue;
        }

        float dir[2];
        float otherSpeed;
        float entSpeed;

        /* The binary rounds each origin delta to float (fstp at
         * 0x429ab/0x429c0) before adding the extended-precision jitter. */
        dir[0] = other->currentOrigin[0] - ent->currentOrigin[0];
        dir[1] = other->currentOrigin[1] - ent->currentOrigin[1];
        /* Stock 0x429ab rounds dir[k] to float before adding a fresh signed
         * random jitter for each component. */
#if EMULATE_X87
        for (int k = 0; k < 2; k++) {
            dir[k] = x87f_store_f32(x87f_add(
                x87f_load_f32(dir[k]),
                x87f_load_f64(coduo_server_rand_signed_unit())));
        }
#else
        dir[0] = (float)((long double)dir[0] +
                         (long double)coduo_server_rand_signed_unit());
        dir[1] = (float)((long double)dir[1] +
                         (long double)coduo_server_rand_signed_unit());
#endif
        VectorNormalize2D(dir);

        otherSpeed = game_compat_stuck_client_move_speed(otherClient);
        entSpeed = game_compat_stuck_client_move_speed(client);
        if (otherSpeed < STUCK_SPEED_EPSILON &&
            entSpeed < STUCK_SPEED_EPSILON) {
            otherSpeed = otherClient->ps.speed;
            entSpeed = client->ps.speed;
        }

        game_compat_stuck_apply_push(otherClient, dir, otherSpeed);
        dir[0] = -dir[0];
        dir[1] = -dir[1];
        game_compat_stuck_apply_push(client, dir, entSpeed);
        return 1;
    }

    return 0;
}

/* 0x42c26 G_PlayerController */
/* VERIFIED_DECOMPILER(0x42c26, 52c26_FUN_00052c26.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - controller callback forwards ent, ent, DObj part bits, and bgs client-info anim view for ent->s.clientNum checked. */
void G_PlayerController(gentity_t *ent, uint32_t *partBits)
{
    /*
     * RECOVERED(UO-GAME-UNK-0070): the typed BGS client-info overlay is shared with
     * G_UpdateClientInfo for model strings, tags, DObj setup, animation, and
     * controller output. DObj passes its four-word part-bit set through this
     * entity callback slot.
     */
    BG_Player_DoControllers(ent, &ent->s, partBits,
                            &bgs.clientinfo[ent->s.clientNum]);
}

/* 0x42c81 G_UpdateClientInfo */
/* VERIFIED_DECOMPILER(0x42c81, 52c81_G_UpdateClientInfo.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - BGS client-info stride, lean/view-angle stores, base model mirror, six attach model/tag slots, changed-model DObj free, BG_UpdatePlayerDObj, and BG_PlayerAnimation calls checked. */
void G_UpdateClientInfo(gentity_t *ent)
{
    gclient_t *client = ent->client;
    clientInfo_t *clientInfo = &bgs.clientinfo[ent->s.clientNum];
    const char *modelName;
    qboolean modelChanged;

    /*
     * RECOVERED(UO-GAME-UNK-0070): this mirrors entity/client model and view state into
     * the bgs client-info record consumed by BG_UpdatePlayerDObj, BG_PlayerAnimation,
     * and G_PlayerController. The entity source words remain recovered because
     * multiple temp-entity/sound paths reuse the same offsets.
     */
    clientInfo->leanAmount =
        ent->s.clientInfo.leanAmount;
    clientInfo->leanFraction = ent->s.clientInfoLeanFraction;
    clientInfo->viewPitch = client->ps.viewAngles[0];
    clientInfo->viewYaw = client->ps.viewAngles[1];
    clientInfo->viewRoll = client->ps.viewAngles[2];

    modelName = G_ModelName(ent->modelIndex);
    client->baseModelIndex = ent->modelIndex;
    modelChanged = strcmp(clientInfo->modelName, modelName) != 0;
    if (modelChanged) {
        Q_strncpyz(clientInfo->modelName, modelName,
                   CLIENT_INFO_MODEL_NAME_SIZE);
    }

    for (int slot = 0; slot < CLIENT_INFO_ATTACHMENT_COUNT; slot++) {
        if (ent->attachModelIndex[slot] == 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (clientInfo->attachModelNames[slot][0] != '\0' ||
                clientInfo->attachTagNames[slot][0] != '\0') {
                modelChanged = 1;
            }
            clientInfo->attachModelNames[slot][0] = '\0';
            clientInfo->attachTagNames[slot][0] = '\0';
            client->attachModelIndices[slot] = 0;
            client->attachTagIndices[slot] = 0;
            continue;
        }

        modelName = G_ModelName(ent->attachModelIndex[slot]);
        client->attachModelIndices[slot] = ent->attachModelIndex[slot];
        if (strcmp(clientInfo->attachModelNames[slot], modelName) != 0) {
            modelChanged = 1;
            Q_strncpyz(clientInfo->attachModelNames[slot], modelName,
                       CLIENT_INFO_MODEL_NAME_SIZE);
        }

        const char *tagName = SL_ConvertToString(ent->attachTagIndex[slot]);
        client->attachTagIndices[slot] = G_TagIndex(tagName);
        if (strcmp(clientInfo->attachTagNames[slot], tagName) != 0) {
            modelChanged = 1;
            Q_strncpyz(clientInfo->attachTagNames[slot], tagName,
                       CLIENT_INFO_MODEL_NAME_SIZE);
        }
    }

    if (modelChanged) {
        G_SafeDObjFree(ent);
    }

    BG_UpdatePlayerDObj(ent, ent, clientInfo, &ent->dobjDirty);
    BG_PlayerAnimation(&ent->s, clientInfo);
}

/* 0x45762 G_GetNonPVSFriendlyInfo */
/* VERIFIED_DECOMPILER(0x45762, 55762_G_GetNonPVSFriendlyInfo.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - team gates, 64-client ring scan, linked/live/team/snapshot filters, initial ROUND(delta + 0.5), scale clamp, signed quarter-offset packing, yaw packing, and entity-number bits checked. */
int G_GetNonPVSFriendlyInfo(gentity_t *ent, const float *origin, int lastClient)
{
    int team = ent->client->sessionTeam;

    if (team == TEAM_FREE ||
        team == TEAM_SPECTATOR) {
        return 0;
    }

    int startClient =
        lastClient == CLIENTEND_NONPVS_NONE
            ? 0
            : coduo_int32_from_bits((uint32_t)lastClient + UINT32_C(1));

    for (int scan = 0; scan < CLIENTEND_NONPVS_SCAN_CLIENTS; scan++) {
        int clientNum = game_compat_client_end_frame_wrap_non_pvs_client_index(
            coduo_int32_from_bits((uint32_t)startClient + (uint32_t)scan));
        gentity_t *candidate = &g_entities[clientNum];
        gclient_t *candidateClient = candidate->client;

        if (candidate->linked == 0 || candidateClient == 0 ||
            candidateClient->sessionState != 0 ||
            candidateClient->sessionTeam != team || candidate == ent ||
            trap_InSnapshot(origin, candidate->s.number) != 0) {
            continue;
        }

        /* Stock 0x458a7/0x458bb: each planar delta is a single sub rounded to
         * float (native); 0x458db/0x458ef: the biased sum (delta+0.5f) is kept
         * 80-bit and truncated straight from the x87 register (fistp direct, no
         * float store) -> shim. */
        float deltaXf = candidate->currentOrigin[0] - origin[0];
        float deltaYf = candidate->currentOrigin[1] - origin[1];
#if EMULATE_X87
        int deltaX = x87f_store_i32_trunc(
            x87f_add(x87f_load_f32(deltaXf), x87f_load_f32(0.5f)));
        int deltaY = x87f_store_i32_trunc(
            x87f_add(x87f_load_f32(deltaYf), x87f_load_f32(0.5f)));
#else
        int deltaX = game_compat_int32_from_long_double_trunc(
            (long double)deltaXf + 0.5L);
        int deltaY = game_compat_int32_from_long_double_trunc(
            (long double)deltaYf + 0.5L);
#endif
        float scaleX = 1.0f;
        float scaleY = 1.0f;

        /* Stock 0x4590c..0x45960: scale = (float)LIMIT / (fild)delta; the divide
         * is kept 80-bit and rounded to float on store -> shim. */
        if (deltaX > CLIENTEND_NONPVS_OFFSET_LIMIT_POS) {
#if EMULATE_X87
            scaleX = x87f_store_f32(x87f_div(
                x87f_load_f32((float)CLIENTEND_NONPVS_OFFSET_LIMIT_POS),
                x87f_load_i32(deltaX)));
#else
            scaleX = (float)((long double)CLIENTEND_NONPVS_OFFSET_LIMIT_POS /
                             (long double)deltaX);
#endif
        } else if (deltaX < CLIENTEND_NONPVS_OFFSET_LIMIT_NEG) {
#if EMULATE_X87
            scaleX = x87f_store_f32(x87f_div(
                x87f_load_f32((float)CLIENTEND_NONPVS_OFFSET_LIMIT_NEG),
                x87f_load_i32(deltaX)));
#else
            scaleX = (float)((long double)CLIENTEND_NONPVS_OFFSET_LIMIT_NEG /
                             (long double)deltaX);
#endif
        }

        if (deltaY > CLIENTEND_NONPVS_OFFSET_LIMIT_POS) {
#if EMULATE_X87
            scaleY = x87f_store_f32(x87f_div(
                x87f_load_f32((float)CLIENTEND_NONPVS_OFFSET_LIMIT_POS),
                x87f_load_i32(deltaY)));
#else
            scaleY = (float)((long double)CLIENTEND_NONPVS_OFFSET_LIMIT_POS /
                             (long double)deltaY);
#endif
        } else if (deltaY < CLIENTEND_NONPVS_OFFSET_LIMIT_NEG) {
#if EMULATE_X87
            scaleY = x87f_store_f32(x87f_div(
                x87f_load_f32((float)CLIENTEND_NONPVS_OFFSET_LIMIT_NEG),
                x87f_load_i32(deltaY)));
#else
            scaleY = (float)((long double)CLIENTEND_NONPVS_OFFSET_LIMIT_NEG /
                             (long double)deltaY);
#endif
        }

        if (scaleX < 1.0f || scaleY < 1.0f) {
            /* Stock 0x4599d/0x459bd: (fild)delta * scale kept 80-bit, fistp
             * truncate straight from the register (no float store) -> shim. */
            if (scaleX < scaleY) {
#if EMULATE_X87
                deltaY = x87f_store_i32_trunc(
                    x87f_mul(x87f_load_i32(deltaY), x87f_load_f32(scaleX)));
#else
                deltaY = game_compat_int32_from_long_double_trunc(
                    (long double)deltaY * (long double)scaleX);
#endif
            } else if (scaleY < scaleX) {
#if EMULATE_X87
                deltaX = x87f_store_i32_trunc(
                    x87f_mul(x87f_load_i32(deltaX), x87f_load_f32(scaleY)));
#else
                deltaX = game_compat_int32_from_long_double_trunc(
                    (long double)deltaX * (long double)scaleY);
#endif
            }
        }

        deltaX = game_compat_client_end_frame_clamp_non_pvs_offset(deltaX);
        deltaY = game_compat_client_end_frame_clamp_non_pvs_offset(deltaY);

        /* Stock 0x45a93: (int)(currentAngles[1] * YAW_SCALE) kept 80-bit, fistp
         * truncate straight from the register (no float store) -> shim. */
#if EMULATE_X87
        int yawByte = x87f_store_i32_trunc(x87f_mul(
            x87f_load_f32(candidate->currentAngles[1]),
            x87f_load_f32(CLIENTEND_NONPVS_YAW_SCALE)));
#else
        int yawByte = game_compat_int32_from_long_double_trunc(
            (long double)candidate->currentAngles[1] *
            (long double)CLIENTEND_NONPVS_YAW_SCALE);
#endif
        return (int)(
            (((uint32_t)yawByte & 0xffu)
             << CLIENTEND_NONPVS_YAW_SHIFT) |
            game_compat_client_end_frame_pack_non_pvs_offset(deltaY, CLIENTEND_NONPVS_Y_SHIFT) |
            game_compat_client_end_frame_pack_non_pvs_offset(deltaX, CLIENTEND_NONPVS_X_SHIFT) |
            ((uint32_t)candidate->s.number & CLIENTEND_NONPVS_ENTITY_MASK));
    }

    return 0;
}

/* 0x42f59 ClientEndFrame */
/* VERIFIED_DECOMPILER(0x42f59, 52f59_ClientEndFrame.c, VERIFY-WAVE2-CLIENT-FRAME-STATE-2026-06-17): DATAFLOW_VERIFIED - connection/session dispatch, respawn path, contents/pmType/state exports, vehicle pitch, state replication, stuck contents, non-PVS info, and player/turret/vehicle hooks match generated output. */
void ClientEndFrame(gentity_t *ent)
{
    gclient_t *client = ent->client;

    /*
     * RECOVERED(UO-GAME-UNK-0071): the normal client end-frame path still uses several
     * player-state/client offsets whose final source names depend on broader
     * ClientEndFrame and ClientThink_real recovery.
     */
    ent->controller = NULL;
    client->ps.deltaTime = 0;

    if (client->connectedState != CON_CONNECTED) {
        return;
    }

    if (client->sessionState == SESS_STATE_INTERMISSION) {
        IntermissionClientEndFrame(ent);
        return;
    }

    if (client->sessionState == SESS_STATE_SPECTATOR) {
        SpectatorClientEndFrame(ent);
        return;
    }

    if (client->ps.psClientNum != ent->s.number) {
        vec3_t spawnOrigin;
        vec3_t spawnAngles;

        spawnOrigin[0] = client->ps.psOrigin[0];
        spawnOrigin[1] = client->ps.psOrigin[1];
        spawnOrigin[2] = client->ps.psOrigin[2];
        spawnAngles[0] = 0.0f;
        spawnAngles[1] = client->ps.viewAngles[1];
        spawnAngles[2] = 0.0f;
        ClientSpawn(ent, spawnOrigin, spawnAngles);
        return;
    }

    ent->svFlags =
        (ent->svFlags | CLIENTEND_ENTITY_FLAGS_NORMAL_SET) &
        ~CLIENTEND_ENTITY_FLAGS_NORMAL_CLEAR;
    ent->takeDamage = 1;
    client->ps.playerStateFlags =
        (client->ps.playerStateFlags | PSF_ACTIVE_PLAYER) &
        ~CLIENTEND_STANCE_FLAGS_CLEAR;
    client->ps.viewModelIndex = client->viewModelIndex;

    G_SetClientContents(ent);

    if ((client->ps.entityStateFlags & EF_IN_VEHICLE) != 0) {
        gentity_t *owner = &g_entities[ent->passEntityNum];
        vehicle_state_t *vehicleState = (vehicle_state_t *)owner->vehicle;

        if (vehicleState != NULL &&
            vehicleState != CLIENTEND_VEHICLE_STATE_NULL_SENTINEL) {
            float vehiclePitchSignal = vehicleState->angularVelocity[0];

            if (game_compat_abs_float(vehiclePitchSignal) < CLIENTEND_VEHICLE_PITCH_THRESHOLD) {
                client->ps.vehicleMotion = 1;
            } else if (vehiclePitchSignal > 0.0f) {
                client->ps.vehicleMotion = 2;
            } else if (vehiclePitchSignal < 0.0f) {
                client->ps.vehicleMotion = 3;
            }
        }
    }

    client->endFrameTransient46a4 = 0;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (client->pingEndTime <= level.time) {
        client->ps.entityStateFlags &= ~CLIENTEND_PS_TIMED_FLAG;
    }

    if (client->noclip != 0) {
        client->ps.pmType = PM_TYPE_NOCLIP;
    } else if (client->ufo != 0) {
        client->ps.pmType = PM_TYPE_UFO;
    } else if (client->sessionState == SESS_STATE_DEAD) {
        client->ps.pmType = ent->linkInfo != 0 ? PM_TYPE_LINKED_DEAD
                                            : PM_TYPE_DEAD;
        ent->svFlags =
            (ent->svFlags | CLIENTEND_ENTITY_FLAGS_DEAD_SET) &
            ~CLIENTEND_ENTITY_FLAGS_DEAD_CLEAR;
        ent->takeDamage = 0;
    } else {
        client->ps.pmType = ent->linkInfo != 0 ? PM_TYPE_LINKED
                                            : PM_TYPE_NORMAL;
    }

    /* VERIFIED_DECOMPILER(0x42f59, 52f59_ClientEndFrame.c, VERIFY-WAVE2-CLIENT-FRAME-STATE-2026-06-17): DATAFLOW_VERIFIED - gravity cvar assignment uses generated ROUND before speed and damage-alpha stores. */
#if EMULATE_X87
    client->ps.gravity =
        x87f_store_i32_trunc(x87f_load_f32(g_gravity.value));
#elif defined(__i386__) || defined(__x86_64__)
    client->ps.gravity =
        CODUO_X87_TRUNCATE_I32((long double)g_gravity.value);
#else
    client->ps.gravity =
        game_compat_int32_from_float_trunc(g_gravity.value);
#endif
    client->ps.speed = client->maxSpeed;
    /* Stock 0x432a8: aimSpreadScale / 255.0 kept 80-bit, rounded to float on
     * store -> shim (divide). */
#if EMULATE_X87
    client->damageAlphaFraction = x87f_store_f32(x87f_div(
        x87f_load_f32(client->ps.aimSpreadScale),
        x87f_load_f64(CLIENTEND_DAMAGE_ALPHA_SCALE)));
#else
    client->damageAlphaFraction =
        (float)((long double)client->ps.aimSpreadScale /
                (long double)CLIENTEND_DAMAGE_ALPHA_SCALE);
#endif

    G_CheckForPreventFriendlyFire(ent);
    G_CheckForCursorHints(ent);
    G_CheckFlameDamage(ent);
    P_DamageFeedback(ent);

    if (coduo_int32_from_bits((uint32_t)level.time -
                              (uint32_t)client->lastUsercmdTime) >
        CLIENTEND_INACTIVE_THRESHOLD_MS) {
        ent->s.eFlags |= CLIENTEND_PS_INACTIVE_FLAG;
    } else {
        ent->s.eFlags &= ~CLIENTEND_PS_INACTIVE_FLAG;
    }

    client->ps.stats[STAT_HEALTH] = ent->health;
    G_SetClientSound(ent);

    if (g_smoothClients.integer == 0) {
        BG_PlayerStateToEntityState(&client->ps, &ent->s, qtrue);
    } else {
        BG_PlayerStateToEntityStateExtrapolate(
            &client->ps, &ent->s, client->ps.commandTime, qtrue);
    }

    if (ent->health > 0 && StuckInClient(ent) != 0) {
        ent->scriptContents = CONTENTS_STUCK_ALT;
    }

    vec3_t leanOrigin;
    leanOrigin[0] = client->ps.psOrigin[0];
    leanOrigin[1] = client->ps.psOrigin[1];
    leanOrigin[2] = client->ps.psOrigin[2] + client->ps.viewHeightCurrent;
    G_AddLean(ent, leanOrigin);

    int nonPvsFriendly =
        G_GetNonPVSFriendlyInfo(
            ent, leanOrigin,
            client->nonpvsFriendlyClient);
    client->ps.compassFriendInfo = nonPvsFriendly;
    if (nonPvsFriendly == 0) {
        client->nonpvsFriendlyClient = CLIENTEND_NONPVS_NONE;
    } else {
        int friendlyClient = nonPvsFriendly & CLIENTEND_NONPVS_ENTITY_MASK;
        client->nonpvsFriendlyClient = friendlyClient;
        if ((g_entities[friendlyClient].s.eFlags &
             CLIENTEND_FRIENDLY_ENTITY_FLAG) != 0) {
            client->ps.entityStateFlags |= CLIENTEND_PS_FRIENDLY_HAS_FLAG;
        } else {
            client->ps.entityStateFlags &= ~CLIENTEND_PS_FRIENDLY_HAS_FLAG;
        }
    }

    int nonPvsTank =
        G_GetNonPVSTankInfo(
            ent, leanOrigin,
            client->nonpvsTankClient);
    client->ps.compassTankInfo = nonPvsTank;
    if (nonPvsTank == 0) {
        client->nonpvsTankClient = CLIENTEND_NONPVS_NONE;
    } else {
        client->nonpvsTankClient = nonPvsTank & CLIENTEND_NONPVS_ENTITY_MASK;
    }

    if (ent->s.eType == ET_PLAYER) {
        ent->controller = G_PlayerController;
        G_UpdateClientInfo(ent);

        if ((client->ps.playerStateFlags & PSF_ACTIVE_PLAYER) != 0 &&
            (client->ps.entityStateFlags & CLIENTEND_TURRET_PS_FLAGS) != 0) {
            turret_think_client(
                &level.gentities[client->ps.viewLockedEntityNum]);
        }

        if ((client->ps.playerStateFlags & PSF_ACTIVE_PLAYER) != 0 &&
            (client->ps.entityStateFlags & EF_IN_VEHICLE) != 0) {
            G_PlayerVehiclePositionAndBlend(ent);
        }

        if (g_debugLocDamage.integer != 0 && trap_DObjExists(ent) != 0) {
            G_DObjCalcPose(ent);
            trap_XModelDebugBoxes(ent);
        }
    }
}

/* 0x3fe31 G_EntityType */
/* VERIFIED_DECOMPILER(0x3fe31, 4fe31_G_EntityType.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - gentity index lookup, linked-byte gate, -1 fallback, and s_eType return checked. */
int G_EntityType(int entityNum)
{
    gentity_t *ent = &g_entities[entityNum];

    if (ent->linked == 0) {
        return -1;
    }
    return ent->s.eType;
}

/* 0x3fe8a SpectatorThink */
/* VERIFIED_DECOMPILER(0x3fe8a, 4fe8a_SpectatorThink.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - spectator button mirrors, follow stop/next/prev edges, pmType/speed setup, pmove stack initialization, callback slots, Pmove call, origin copy-back, and unlink checked. */
void SpectatorThink(gentity_t *ent, const usercmd_t *command)
{
    gclient_t *client = ent->client;

    client->oldButtons = client->currentButtons;
    client->currentButtons = client->command.buttons;
    client->oldWbuttons = client->spectatorWbuttons;
    client->spectatorWbuttons = client->command.wbuttons;

    if (client->followClient < 0 &&
        G_ClientCanSpectateTeam(client, SPECTATOR_FOLLOW_TEAM) != 0 &&
        client->archiveClient >= 0 &&
        /* VERIFIED_DECOMPILER(0x3fe8a, 4fe8a_SpectatorThink.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - stop-follow edge compares regular button byte gclient+0x462c against previous gclient+0x4630. */
        ((client->currentButtons & SPECTATOR_BUTTON_STOP_FOLLOW_MASK) !=
         (client->oldButtons & SPECTATOR_BUTTON_STOP_FOLLOW_MASK))) {
        StopFollowing(ent);
    }

    if ((client->currentButtons & SPECTATOR_BUTTON_FOLLOW_NEXT_MASK) != 0 &&
        ((client->oldButtons & SPECTATOR_BUTTON_FOLLOW_NEXT_MASK) == 0)) {
        Cmd_FollowCycle_f(ent, 1);
    } else if ((client->currentButtons & SPECTATOR_BUTTON_FOLLOW_PREV_MASK) != 0 &&
               ((client->oldButtons & SPECTATOR_BUTTON_FOLLOW_PREV_MASK) == 0)) {
        Cmd_FollowCycle_f(ent, -1);
    }

    if ((client->ps.playerStateFlags & PSF_FOLLOWING) == 0) {
        pmove_t pmove;

        client->ps.pmType = PM_TYPE_SPECTATOR;
        client->ps.speed = G_ClientCanSpectateTeam(client, SPECTATOR_FOLLOW_TEAM) != 0
                            ? SPECTATOR_PM_FLAGS_ALLOWED
                            : 0;

        memset(&pmove, 0, sizeof(pmove));
        pmove.ps = &client->ps;
        pmove.command = *command;
        pmove.traceMask = SPECTATOR_PMOVE_TRACEMASK;
        pmove.trace = trap_TraceCapsule;
        pmove.trace2 = trap_TraceCapsule;
        pmove.trace3 = trap_TraceCapsule;
        pmove.pointContents = trap_PointContents;
        pmove.entityType = G_EntityType;
        pmove.adsInputBlocked = G_IsInMatchTimeout();

        Pmove(&pmove);

        ent->currentOrigin[0] = client->ps.psOrigin[0];
        ent->currentOrigin[1] = client->ps.psOrigin[1];
        ent->currentOrigin[2] = client->ps.psOrigin[2];
        trap_UnlinkEntity(ent);
    }
}

/* 0x4013c ClientInactivityTimer */
/* VERIFIED_DECOMPILER(0x4013c, 5013c_ClientInactivityTimer.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - inactivity cvar zero path, movement/button activity reset, no-kick gate, drop command, warning timing, and server-command arguments checked. */
qboolean ClientInactivityTimer(gclient_t *client)
{
    /*
     * RECOVERED(UO-GAME-UNK-0061): input byte offsets and inactivity deadline fields are
     * modeled from this call site until the full client input/session layout is named.
     */
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (g_inactivity.integer == 0) {
        client->inactivityTime = coduo_int32_from_bits(
            (uint32_t)level.time + (uint32_t)INACTIVITY_GRACE_MS);
        client->inactivityWarningSent = 0;
    } else if (client->command.forwardmove != 0 ||
               client->command.rightmove != 0 ||
               client->command.upmove != 0 ||
               (client->command.buttons &
                INACTIVITY_INPUT_BUTTON_MASK) != 0) {
        client->inactivityTime = coduo_int32_from_bits(
            (uint32_t)level.time +
            (uint32_t)g_inactivity.integer *
                (uint32_t)INACTIVITY_SECONDS_TO_MS);
        client->inactivityWarningSent = 0;
    } else if (client->complaintDisabled == 0) {
        if (client->inactivityTime < level.time) {
            trap_DropClient((int)(client - level.clients), "GAME_DROPPEDFORINACTIVITY");
            return 0;
        }

        if (coduo_int32_from_bits(
                (uint32_t)client->inactivityTime -
                (uint32_t)INACTIVITY_WARNING_MS) < level.time &&
            client->inactivityWarningSent == 0) {
            client->inactivityWarningSent = 1;
            trap_SendServerCommand((int)(client - level.clients), SERVER_COMMAND_UNRELIABLE,
                                   "c \"GAME_INACTIVEDROPWARNING\"");
        }
    }

    return 1;
}

/* 0x402c0 ClientSpectatorInactivityTimer */
/* VERIFIED_DECOMPILER(0x402c0, 502c0_ClientSpectatorInactivityTimer.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - initial negative-time reset, sv_privateClients register/update helper, spectator/private-client reset gates, activity reset, no-kick gate, drop command, and warning command checked. */
qboolean ClientSpectatorInactivityTimer(gclient_t *client)
{
    /*
     * RECOVERED(UO-GAME-UNK-0061): spectator inactivity uses a parallel deadline/warning
     * pair and treats `spectatorActivityState` as additional spectator activity.
     */
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (client->spectatorInactivityTime < 0) {
        client->spectatorInactivityTime = coduo_int32_from_bits(
            (uint32_t)level.time + (uint32_t)INACTIVITY_GRACE_MS);
        client->spectatorInactivityWarning = 0;
    }

    game_compat_update_sv_private_clients();

    if (g_inactivityspectator.integer == 0 ||
        client->sessionState != SESS_STATE_SPECTATOR ||
        client->ps.psClientNum < sv_privateClients.integer) {
        client->spectatorInactivityTime = coduo_int32_from_bits(
            (uint32_t)level.time + (uint32_t)INACTIVITY_GRACE_MS);
        client->spectatorInactivityWarning = 0;
    } else if (client->command.forwardmove != 0 ||
               client->command.rightmove != 0 ||
               client->command.upmove != 0 ||
               (client->command.buttons &
                INACTIVITY_INPUT_BUTTON_MASK) != 0 ||
               client->spectatorActivityState != 0) {
        client->spectatorInactivityTime = coduo_int32_from_bits(
            (uint32_t)level.time +
            (uint32_t)g_inactivityspectator.integer *
                (uint32_t)INACTIVITY_SECONDS_TO_MS);
        client->spectatorInactivityWarning = 0;
    } else if (client->complaintDisabled == 0) {
        if (client->spectatorInactivityTime < level.time) {
            trap_DropClient((int)(client - level.clients), "GAME_DROPPEDFORINACTIVITY");
            return 0;
        }

        if (coduo_int32_from_bits(
                (uint32_t)client->spectatorInactivityTime -
                (uint32_t)INACTIVITY_WARNING_MS) < level.time &&
            client->spectatorInactivityWarning == 0) {
            client->spectatorInactivityWarning = 1;
            trap_SendServerCommand((int)(client - level.clients), SERVER_COMMAND_UNRELIABLE,
                                   "c \"GAME_INACTIVEDROPWARNING\"");
        }
    }

    return 1;
}

/* 0x404e5 ClientIntermissionThink */
/* VERIFIED_DECOMPILER(0x404e5, 504e5_ClientIntermissionThink.c, VERIFY-WAVE4-CLIENT-FRAME-LOCAL-2026-06-17): DATAFLOW_VERIFIED - old/current button and wbutton mirror stores checked. */
void ClientIntermissionThink(gentity_t *ent,
                             const usercmd_t *command)
{
    gclient_t *client = ent->client;
    (void)command;

    /*
     * RECOVERED(UO-GAME-UNK-0060): intermission uses the same current/old input mirrors
     * as spectator movement, but only updates the mirrors and performs no edge actions.
     */
    client->oldButtons = client->currentButtons;
    client->currentButtons = client->command.buttons;
    client->oldWbuttons = client->spectatorWbuttons;
    client->spectatorWbuttons = client->command.wbuttons;
}
