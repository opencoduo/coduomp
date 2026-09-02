/*
 * Source reconstruction for entity dispatch and lifecycle.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "recovered_game.h"
#include "entity_dispatch_private.h"
#include "g_public.h"
#include "game_globals.h"
#include "g_syscalls.h"
#include "game_functions.h"
#include "level_locals.h"
#include "scr_vm.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */
#include "compat/coduo_native_x87.h"
#include "compat/libm/coduo_libm.h"
#include "math/q_math.h"

#define ENTITY_TIMEOUT_MS 300
#define ENTITY_LAST_RESERVED_INDEX (PLAYER_CLONE_ENTITYNUM_BASE + PLAYER_CLONE_COUNT - 1)
#define DROPPED_WEAPON_SLOT_COUNT 32
#define FUNC_ROTATING_UNLINKED_FLAG 0x04u

/* ------------------------------------------------------------------ */
/*  0x59862  G_RunThink                                               */
/* ------------------------------------------------------------------ */

/*
 * Execute an entity's think function if its nextthink time has arrived.
 *
 * Called by G_RunFrameForEntity for entities that have a think function
 * and whose nextthink time has passed. Also called directly by G_RunMover
 * and G_GeneralLink for linked entities.
 */
/* VERIFIED_DECOMPILER(0x59862, 69862_G_RunThink.c, VERIFY-FIRST-FUNCTIONS-2026-06-17): DATAFLOW_VERIFIED; nextthink positive/time gate, nextthink clear, null-think error, and callback dispatch checked against current decompiler output. */
void G_RunThink(gentity_t *ent)
{
    if (ent->nextthink > 0 && ent->nextthink <= level.time) {
        ent->nextthink = 0;
        if (ent->think == NULL) {
            G_Error("NULL ent->think");
        }
        ent->think(ent);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: frame counter accessor for G_RunFrameForEntity; extracted during reconstruction of 0x59a35. */
/* VERIFIED_DECOMPILER(0x59a35, 69a35_FUN_00069a35.c, VERIFY-ENTITY-DISPATCH-FRAME-LINK-MOVER-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against DAT_0024b708 frame-number reads in parent dispatcher. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_RunFrameForEntity (0x59a35); no standalone original body. */
static int game_compat_entity_dispatch_frame_num(void)
{
    return level.framenum;
}

/* ------------------------------------------------------------------ */
/*  0x78bc8  G_GeneralLink                                            */
/* ------------------------------------------------------------------ */

/*
 * Apply full matrix link transform to a linked entity.
 *
 * Called by G_RunFrameForEntity for linked entities (those with linkInfo)
 * and by Think_GeneralLink (the periodic link maintenance think function).
 *
 * Uses G_SetFixedLink mode 0 (full matrix link) to compute the entity's
 * world position from its parent's transform and the stored relative
 * transform in linkInfo.
 */
/* VERIFIED_DECOMPILER(0x78bc8, 88bc8_G_GeneralLink.c, VERIFY-ENTITY-DISPATCH-FRAME-LINK-MOVER-2026-06-17): DATAFLOW_VERIFIED - SetFixedLink mode, origin/angle argument pointers, interpolate trajectory writes, link call, and void return checked. */
void G_GeneralLink(gentity_t *ent)
{
    G_SetFixedLink(ent, 0);
    G_SetOrigin(ent, ent->currentOrigin);
    G_SetAngle(ent, ent->currentAngles);
    ent->s.pos.trType = TR_INTERPOLATE;
    ent->s.apos.trType = TR_INTERPOLATE;
    trap_LinkEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x59a35  G_RunFrameForEntity (FUN_00069a35)                       */
/* ------------------------------------------------------------------ */

/*
 * Per-entity dispatch function called by G_RunFrame for each linked entity.
 *
 * This is the heart of the per-frame entity simulation. It:
 *  1. Checks the per-entity frame counter to prevent double-processing
 *  2. Syncs the FL_NOCLIENT flag for non-client entities
 *  3. Handles entity timeout (300ms threshold) — frees or unlinks
 *  4. Syncs the SVF_CAPSULE flag
 *  5. Dispatches by entity type to type-specific runners:
 *     - Type 4 (missile): G_RunMissile
 *     - Type 3 (item): G_RunItem or G_GeneralLink + G_RunThink
 *     - Type 5/8 (mover): G_RunMover
 *     - Client entities: G_RunClient
 *     - Other: G_RunThink
 *
 * RECOVERED(UO-GAME-UNK-0145): The original source-level name for this
 * function is not known. The vmMain per-frame command calls G_RunFrame,
 * which reaches this per-entity dispatcher during the frame loop.
 */
/* VERIFIED_DECOMPILER(0x59a35, 69a35_FUN_00069a35.c, VERIFY-ENTITY-DISPATCH-FRAME-LINK-MOVER-2026-06-17): DATAFLOW_VERIFIED - frame guard, no-client/sv capsule flag sync, timeout free/unlink side effects, eType dispatch, linked item path, linkedByte16d fallback, call order, and early returns checked. */
void G_RunFrameForEntity(gentity_t *ent)
{
    /* Prevent double-processing within the same frame */
    if (ent->lastFrameNum == game_compat_entity_dispatch_frame_num()) {
        return;
    }
    ent->lastFrameNum = game_compat_entity_dispatch_frame_num();

    /* Sync FL_NOCLIENT flag for non-client entities */
    if (ent->client == NULL) {
        if ((ent->flags & FL_NOCLIENT) == 0) {
            ent->s.eFlags = ent->s.eFlags & ~EF_NODRAW;
        } else {
            ent->s.eFlags = ent->s.eFlags | EF_NODRAW;
        }
    }

    /* Entity timeout check. */
    if (level.time - ent->lastThinkTime > ENTITY_TIMEOUT_MS) {
        if (ent->skipTypeDispatch != 0) {
            /* Entity has custom think — free it on timeout */
            G_FreeEntity(ent);
            return;
        }
        if (ent->unlinkOnTimeout != 0) {
            /* No-respawn item pickup: clear the one-shot unlink flag. */
            ent->unlinkOnTimeout = 0;
            trap_UnlinkEntity(ent);
        }
    }

    /* Sync SVF_CAPSULE flag from entity flags */
    if ((ent->s.eFlags & EF_CAPSULE) == 0) {
        ent->svFlags = ent->svFlags & ~SVF_CAPSULE;
    } else {
        ent->svFlags = ent->svFlags | SVF_CAPSULE;
    }

    /* Type-based dispatch (skipped if skipTypeDispatch is set). */
    if (ent->skipTypeDispatch == 0) {
        if (ent->s.eType == ET_MISSILE) {
            G_RunMissile(ent);
        } else if (ent->s.eType == ET_ITEM) {
            if (ent->linkInfo == NULL) {
                G_RunItem(ent);
            } else {
                G_GeneralLink(ent);
                G_RunThink(ent);
            }
        } else if (ent->linkedByte16d == 0) {
            if (ent->s.eType == ET_MOVER || ent->s.eType == ET_SCRIPTMOVER) {
                G_RunMover(ent);
            } else if (ent->client == NULL) {
                G_RunThink(ent);
            } else {
                G_RunClient(ent);
            }
        } else {
            G_RunItem(ent);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x79a06  G_FreeEntityRefs                                         */
/* ------------------------------------------------------------------ */

/*
 * Clear references to an entity that is being freed.
 *
 * The cleanup covers active entity references, ownership/pass-entity links,
 * ground links, client look-at targets, vehicle-owned references, and the
 * dropped item slot table.
 */
/* VERIFIED_DECOMPILER(0x79a06, 89a06_G_FreeEntityRefs.c, VERIFY-P1-FREEPATH-2026-06-17): DATAFLOW_VERIFIED - entity-ref scan, passEntityNum/groundEntityNum clearing, ET_TURRET activeState guard, client lookAtEntity clearing, vehicle refs, and dropped slot cleanup checked against current decompiler output. */
void G_FreeEntityRefs(gentity_t *ent)
{
    int entNum = ent->s.number;

    for (int i = 0; i < level.num_entities; i++) {
        gentity_t *check = &g_entities[i];

        if (check->linked == 0) {
            continue;
        }

        if (check->entityRef == ent) {
            check->entityRef = NULL;
        }

        if (check->passEntityNum == entNum) {
            check->passEntityNum = ENTITYNUM_NONE;
            if (check->s.eType == ET_TURRET) {
                check->activeState = 0;
            }
        }

        if (check->s.groundEntityNum == entNum) {
            check->s.groundEntityNum = ENTITYNUM_NONE;
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        gentity_t *player = &g_entities[i];

        if (player->linked != 0 && player->client->lookAtEntity == ent) {
            player->client->lookAtEntity = NULL;
        }
    }

    G_FreeVehicleRefs(ent);

    for (int i = 0; i < DROPPED_WEAPON_SLOT_COUNT; i++) {
        if (level.droppedWeaponSlots[i] == ent) {
            level.droppedWeaponSlots[i] = NULL;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x79b77  G_FreeEntity                                             */
/* ------------------------------------------------------------------ */

/*
 * Free a game entity, returning it to the free list if eligible.
 *
 * Unlinks the entity from its parent and children, unlinks from the world,
 * frees its DObj, entity refs, turret/vehicle subsystems, and script data.
 * The entity struct is zeroed except for the generation counter (validationToken)
 * which is incremented to invalidate stale references.
 *
 * Entities beyond the reserved MAX_CLIENTS+8 slots are
 * added to the free list for reuse by G_Spawn.
 */
/* VERIFIED_DECOMPILER(0x79b77, 89b77_G_FreeEntity.c, VERIFY-P1-FREEPATH-2026-06-17): DATAFLOW_VERIFIED - unlink order, DObj free, reference cleanup, turret/vehicle/script cleanup, 0x34c zeroing, free-list append threshold, and validationToken increment checked against current decompiler output. */
void G_FreeEntity(gentity_t *ent)
{
    int generation;

    /* Unlink from parent and all children */
    G_EntUnlink(ent);
    while (ent->firstChild != NULL) {
        G_EntUnlink(ent->firstChild);
    }

    /* Unlink from world and free engine resources */
    trap_UnlinkEntity(ent);
    trap_SafeDObjFree(ent->s.number, 1);
    G_FreeEntityRefs(ent);

    /* Free subsystem-specific data */
    if (ent->turretState != NULL) {
        G_FreeTurret(ent);
    }
    if (ent->vehicle != NULL) {
        G_FreeVehicle(ent);
    }
    Scr_FreeEntity(ent);

    /* Preserve generation counter, zero the rest */
    generation = ent->validationToken;
    memset(ent, 0, sizeof(*ent));

    /* Add to free list if beyond reserved slots */
    {
        int entityIndex = (int)(ent - level.gentities);
        if (entityIndex > ENTITY_LAST_RESERVED_INDEX) {
            if (level.freeListHead == NULL) {
                level.freeListHead = ent;
            } else {
                level.freeListTail->nextFree = ent;
            }
            level.freeListTail = ent;
            ent->nextFree = NULL;
        }
    }

    /* Increment generation to invalidate stale references */
    ent->validationToken = generation + 1;
}

/* ------------------------------------------------------------------ */
/*  0x616c5  G_RunMover                                               */
/* ------------------------------------------------------------------ */

/*
 * Per-frame update for mover entities (types 5 and 8).
 *
 * Movers are doors, platforms, rotators, and other brush-model entities.
 * If the mover has a link record, it applies the link transform.
 * Otherwise, if it has trajectory data, it calls G_MoverTeam to simulate
 * the movement. Always falls through to G_RunThink at the end.
 *
 * The entity-flag bit 2 path uses the engine link-state field and classname
 * const-string IDs to keep start-unlinked func_tramcar and func_rotating
 * movers out of the world.
 */
void G_MoverTeam(gentity_t *ent);

/* VERIFIED_DECOMPILER(0x616c5, 716c5_G_RunMover.c, VERIFY-ENTITY-DISPATCH-FRAME-LINK-MOVER-2026-06-17): DATAFLOW_VERIFIED - linkInfo split, flag-4 unlink/return paths, func_tramcar and func_rotating const-string compares, trajectory mover-team gate, linked transform path, think fallthrough, and early returns checked. */
void G_RunMover(gentity_t *ent)
{
    if (ent->linkInfo == NULL) {
        /* Special case: flag at +0x18c bit 2 with model/index checks */
        if ((ent->flags & FUNC_ROTATING_UNLINKED_FLAG) != 0) {
            if (ent->linkedState != 0 && ent->scriptClassname == scr_const_func_tramcar) {
                trap_UnlinkEntity(ent);
                return;
            }
            if (ent->linkedState == 0) {
                return;
            }
            if (ent->scriptClassname != scr_const_func_rotating) {
                return;
            }
            trap_UnlinkEntity(ent);
            return;
        }
        /* Normal mover: simulate trajectory if present */
        if (ent->s.pos.trType != TR_STATIONARY || ent->s.apos.trType != TR_STATIONARY) {
            G_MoverTeam(ent);
        }
    } else {
        /* Linked mover: apply link transform */
        G_GeneralLink(ent);
    }
    G_RunThink(ent);
}

/* ------------------------------------------------------------------ */
/*  0x612f9  G_MoverTeam                                              */
/* ------------------------------------------------------------------ */

/*
 * Trajectory-based movement for mover entities (doors, platforms, etc).
 *
 * Evaluates position and angle trajectories, performs collision detection,
 * and updates entity positions. Handles linked entities and calls think
 * functions when trajectory times expire.
 *
 * RECOVERED(UO-GAME-UNK-0154): Entity offsets used:
 * - +0x00c: position trajectory
 * - +0x010: position trajectory time
 * - +0x030: angle trajectory
 * - +0x034: angle trajectory time
 * - +0x13c-+0x144: currentOrigin
 * - +0x148-+0x150: currentAngles
 * - +0x018-+0x020: s.pos.trBase (position base)
 * - +0x270: next linked entity
 * - +0x214: think function pointer
 * - +0x218: callback function pointer
 *
 * RECOVERED(UO-GAME-UNK-0155): G_MoverPush handles one mover push step,
 * including swept bounds collection, candidate filtering, and the special
 * crush-damage path for trajectory mode 4 movers.
 */
int G_SpawnVector(const char *key, const char *defaultValue, float *out);
void SetMoverState(gentity_t *ent, int moverState, int time);
void Reached_BinaryMover(gentity_t *ent);
void ReturnToPos1(gentity_t *ent);
void ReturnToPos1Rotate(gentity_t *ent);
void InitMover(gentity_t *ent);
void Use_BinaryMover(gentity_t *ent, gentity_t *other, gentity_t *activator);
void Use_Func_Rotate(gentity_t *ent, gentity_t *other, gentity_t *activator);
void Blocked_Door(gentity_t *ent, gentity_t *blocker);
void Blocked_DoorRotate(gentity_t *ent, gentity_t *blocker);
void G_Damage(gentity_t *target, gentity_t *inflictor, gentity_t *attacker, const float *dir, const float *point, int damage, int flags,
              int mod, int hitLocation);
gentity_t *G_Spawn(void);
void MatchTeam(gentity_t *ent, int moverState, int time);

#define MOVER_PUSH_MAX_ENTITIES 1024
#define MOVER_PUSH_QUERY_MASK MASK_MOVER_PUSH
#define MOVER_CRUSH_DAMAGE 99999
#define MOVER_ROTATE_GROUND_ONLY_FLAG 0x04000000u
#define MOVER_TRACE_DEFAULT_CONTENTS MASK_GRENADE_TRACE
#define MOVER_PUSH_STEP 4.0f
#define MOVER_ANGLE_SHORT_SCALE 182.04445f /* original float32 0x43360b61 */
#define BINARY_MOVER_SVFLAGS 0x80u
#define BINARY_MOVER_TEAM_SLAVE_FLAG 0x04u
#define BINARY_MOVER_TOGGLE_FLAG 0x80u
#define BINARY_MOVER_QUIET_FLAG 0x100u
#define BINARY_MOVER_SKIP_LINK_FLAG 0x01u
#define BINARY_MOVER_SPAWNFLAG_NO_REACHED 0x40u
#define BINARY_MOVER_START_DELAY_MS 50
#define BINARY_MOVER_SOUND_STARTUP_MS 4000
#define BINARY_MOVER_NO_AUTORETURN_WAIT -1000.0f
#define BINARY_MOVER_LINK_WAIT_SENTINEL -1.0f
#define BINARY_MOVER_LINK_ITEM_COUNT 1
#define DOOR_SPAWNFLAG_CRUSHER 0x04u
#define DOOR_BLOCKED_TEMP_EVENT 205
#define DOOR_ROTATE_FALLBACK_DAMAGE 99999
#define DOOR_TRIGGER_EXPAND_DISTANCE 120.0f
#define AUTO_DOOR_SPAWNFLAG_INIT_OPEN 0x01u
#define KEYED_MOVER_SPAWNFLAG_TRIGGER 0x08u
#define KEYED_MOVER_COMMAND_BLOCKED_FLAG 0x04u
#define KEYED_MOVER_FINISH_DELAY_MS 100
#define BINARY_MOVER_DEFAULT_SPEED 100.0f
#define DOOR_DEFAULT_SPEED 400.0f
#define DOOR_DEFAULT_WAIT_SECONDS 2.0f
#define DOOR_WAIT_SCALE 1000.0f
#define DOOR_DEFAULT_LIP 8.0f
#define DOOR_SPAWNFLAG_START_OPEN 0x01u
#define DOOR_SPAWNFLAG_TOGGLE 0x02u
#define DOOR_MOVER_HINT_FLAG 0x80u
#define DOOR_MOVER_STICKY_FLAG 0x100u
#define DOOR_HEALTH_DEFAULT "0"
#define DOOR_LOCKED_DEFAULT "0"
#define DOOR_DMG_DEFAULT "2"
#define STATIC_PAIN_DAMAGE_POINT_FLAG 0x04u
#define STATIC_SPAWNFLAG_START_UNLINKED 0x01u
#define STATIC_SPAWNFLAG_PAIN_TIMER 0x02u
#define STATIC_DEFAULT_HEALTH 9999
#define STATIC_DEFAULT_ITEM_COUNT 4
#define STATIC_DEFAULT_PAIN_DELAY_MS 1000.0f
#define STATIC_PAIN_RANDOM_MS 1000
#define STATIC_PAIN_BASE_DELAY_MS 500.0f
#define STATIC_HEALTH_DEFAULT "0"
#define FUNC_ROTATING_SPAWNFLAG_START_ON 0x01u
#define FUNC_ROTATING_SPAWNFLAG_START_UNLINKED 0x02u
#define FUNC_ROTATING_SPAWNFLAG_Z_AXIS 0x04u
#define FUNC_ROTATING_SPAWNFLAG_X_AXIS 0x08u
#define FUNC_ROTATING_DEFAULT_SPEED 100.0f
#define FUNC_ROTATING_DEFAULT_DAMAGE 2
#define FUNC_BOBBING_SPAWNFLAG_X_AXIS 0x01u
#define FUNC_BOBBING_SPAWNFLAG_Y_AXIS 0x02u
#define FUNC_BOBBING_DEFAULT_SPEED "4"
#define FUNC_BOBBING_DEFAULT_HEIGHT "32"
#define FUNC_BOBBING_DEFAULT_DAMAGE "2"
#define FUNC_BOBBING_DEFAULT_PHASE "0"
#define FUNC_PENDULUM_DEFAULT_SPEED "30"
#define FUNC_PENDULUM_DEFAULT_DAMAGE "2"
#define FUNC_PENDULUM_DEFAULT_PHASE "0"
#define FUNC_PENDULUM_MIN_LENGTH 8.0f
#define FUNC_PENDULUM_GRAVITY_SCALE 3.0f
#define FUNC_PENDULUM_FREQ_SCALE 0.15915494309189535 /* original double 0x3fc45f306dc9c883 */
#define FUNC_DOOR_ROTATING_SPAWNFLAG_START_OPEN 0x01u
#define FUNC_DOOR_ROTATING_SPAWNFLAG_TOGGLE 0x02u
#define FUNC_DOOR_ROTATING_SPAWNFLAG_Z_AXIS 0x04u
#define FUNC_DOOR_ROTATING_SPAWNFLAG_X_AXIS 0x08u
#define FUNC_DOOR_ROTATING_SPAWNFLAG_REVERSE 0x10u
#define FUNC_DOOR_ROTATING_SPAWNFLAG_SKIP_BLOCK_CHECK 0x20u
#define FUNC_DOOR_ROTATING_SPAWNFLAG_AXIS_MASK 0x0cu
#define FUNC_DOOR_ROTATING_SPAWNFLAG_NO_REACHED 0x40u
#define FUNC_DOOR_ROTATING_DEFAULT_SPEED 1000.0f
#define FUNC_DOOR_ROTATING_DEFAULT_YAW 90.0f
#define FUNC_DOOR_ROTATING_DEFAULT_WAIT 2.0f
#define FUNC_DOOR_ROTATING_WAIT_SCALE 1000.0f
#define FUNC_DOOR_ROTATING_LOCKED_DEFAULT "0"
#define FUNC_DOOR_ROTATING_HEALTH_DEFAULT "0"
#define FUNC_DOOR_ROTATING_AXIS_ERROR \
    "Too many axis marked in func_door_rotating entity.  Only choose one axis of rotation. (defaulting to standard " \
    "door rotation)"
#define TRIGGER_USE_HINTSTRING_INHERIT 255
#define TRIGGER_USE_HINTSTRING_LIMIT CS_HINTSTRINGS_COUNT
#define TRIGGER_USE_HINTSTRING_LIMIT_ERROR \
    COM_ERROR_MARKER "Too many different hintstring key values on trigger_use entities. Max allowed is %i"
#define TRIGGER_USE_HINT_TABLE_LIMIT 12
#define TRIGGER_USE_ACTIVATE_CONTENTS 0x00200000u
#define TRIGGER_USE_SPAWNFLAG_TOGGLE 0x01u
#define KILLBOX_MAX_ENTITIES 1024
#define KILLBOX_DAMAGE 100000
#define TRIGGER_SVFLAGS 0x00000001u
#define TRIGGER_ENTITY_STATE_FLAG 0x00000002u
#define TRIGGER_FREE_DELAY_MS 100
#define TRIGGER_SPAWNFLAG_NO_PLAYER 0x08u
#define TRIGGER_SPAWNFLAG_SENTIENT_1 0x01u
#define TRIGGER_SPAWNFLAG_SENTIENT_2 0x02u
#define TRIGGER_SPAWNFLAG_SENTIENT_3 0x04u
#define TRIGGER_SPAWNFLAG_VEHICLE 0x10u
#define CONTENTS_TRIGGER_SENTIENT_1 0x00040000u
#define CONTENTS_TRIGGER_SENTIENT_2 0x00080000u
#define CONTENTS_TRIGGER_SENTIENT_3 0x00100000u
#define TRIGGER_DEFAULT_WAIT "0.5"
#define TRIGGER_DEFAULT_RANDOM "0"

mover_push_record_t pushed[MAX_GENTITIES];

#if UINTPTR_MAX == 0xffffffffu
GAME_STATIC_ASSERT(mover_push_record_size, sizeof(mover_push_record_t) == 0x20);
GAME_STATIC_ASSERT(mover_push_record_angles_offset, offsetof(mover_push_record_t, angles) == 0x10);
GAME_STATIC_ASSERT(mover_push_record_yaw_offset, offsetof(mover_push_record_t, yawDelta) == 0x1c);
#endif

/* NOT_FROM_ORIGINAL_SOURCE: angle short conversion shared by mover push/rollback; extracted during reconstruction of 0x605e5. */
/* VERIFIED_DECOMPILER(0x605e5, 705e5_G_TryPushingEntity.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against pushed client deltaAngles[1] update using x87 truncate of yaw * 182.04445 masked to 16 bits. */
static int game_compat_g_mover_angle_to_short(float angle)
{
    /* 0x6080d/0x60ab3: the product feeds truncating fistp directly. */
#if EMULATE_X87
    int32_t packed = x87f_store_i32_trunc(x87f_mul(x87f_load_f32(angle), x87f_load_f32(MOVER_ANGLE_SHORT_SCALE)));
#else
    int32_t packed = game_compat_int32_from_long_double_trunc((long double)angle * MOVER_ANGLE_SHORT_SCALE);
#endif
    return packed & 0xffff;
}

/* NOT_FROM_ORIGINAL_SOURCE: three-float copy helper used by mover/trigger reconstructions; extracted during reconstruction of 0x64f76. */
/* VERIFIED_DECOMPILER(0x64f76, 74f76_trigger_use.c, VERIFY-TRIGGER-TOUCH-PACKET-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against trigger_use currentOrigin[0..2] stores into pos.trBase[0..2] with no extra side effects. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original trigger_use (0x64f76); no standalone original body. */
static void game_compat_g_copy_vector3(const float *src, float *dest)
{
    dest[0] = src[0];
    dest[1] = src[1];
    dest[2] = src[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: mover speed payload accessor; extracted during reconstruction of 0x62e2e. */
/* VERIFIED_DECOMPILER(0x62e2e, 72e2e_InitMover.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against InitMover and mover spawn bodies using float payload at gentity +0x1f0. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original InitMover (0x62e2e); no standalone original body. */
static float *game_compat_g_binary_mover_speed(gentity_t *ent)
{
    return (float *)(void *)&ent->maxSpeed;
}

/* NOT_FROM_ORIGINAL_SOURCE: shared pushed-entity origin update block; extracted during reconstruction of 0x605e5. */
/* VERIFIED_DECOMPILER(0x605e5, 705e5_G_TryPushingEntity.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against groundEntityNum reset, currentOrigin/pos.trBase stores, client deltaAngles[1] add, and client psOrigin stores. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_TryPushingEntity (0x605e5); no standalone original body. */
static void game_compat_g_mover_set_pushed_origin(gentity_t *ent, const float *origin, int moverEntityNum, float yawDelta)
{
    if (ent->s.groundEntityNum != moverEntityNum) {
        ent->s.groundEntityNum = ENTITYNUM_NONE;
    }

    ent->currentOrigin[0] = origin[0];
    ent->currentOrigin[1] = origin[1];
    ent->currentOrigin[2] = origin[2];
    ent->s.pos.trBase[0] = origin[0];
    ent->s.pos.trBase[1] = origin[1];
    ent->s.pos.trBase[2] = origin[2];

    if (ent->client != NULL) {
        ent->client->ps.deltaAngles[1] += game_compat_g_mover_angle_to_short(yawDelta);
        ent->client->ps.psOrigin[0] = origin[0];
        ent->client->ps.psOrigin[1] = origin[1];
        ent->client->ps.psOrigin[2] = origin[2];
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: pushed stack cursor advance; extracted during reconstruction of 0x605e5. */
/* VERIFIED_DECOMPILER(0x605e5, 705e5_G_TryPushingEntity.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against DAT_000e1c40 += 0x20 pushed-record cursor advance. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_TryPushingEntity (0x605e5); no standalone original body. */
static void game_compat_g_mover_push_record_advance(void)
{
    moverPushStackCursor++;
}

/* ------------------------------------------------------------------ */
/*  0x76218  G_Trigger                                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x76218, 86218_G_Trigger.c, VERIFY-TRIGGER-TOUCH-PACKET-2026-06-17): DATAFLOW_VERIFIED - system-active guard, full-watch immediate notify path, watch slot indexing, entity numbers, validation-token stores, and return behavior checked. */
void G_Trigger(gentity_t *ent, gentity_t *activator)
{
    level_trigger_notify_watch_entry_t *watchBase;
    level_trigger_notify_watch_entry_t *watch;
    int watchCount;

    if (Scr_IsSystemActive(1) == 0) {
        return;
    }

    watchCount = level.notifyWatchCount;
    if (watchCount == LEVEL_TRIGGER_NOTIFY_WATCH_LIMIT) {
        Scr_AddEntity(activator);
        Scr_Notify(ent, scr_const_trigger, 1);
        return;
    }

    watchBase = level.triggerNotifyWatch;
    watch = &watchBase[watchCount];
    level.notifyWatchCount = watchCount + 1;

    watch->entNum1 = (uint16_t)ent->s.number;
    watch->entNum2 = (uint16_t)activator->s.number;
    watch->token1 = ent->validationToken;
    watch->token2 = activator->validationToken;
}

/* ------------------------------------------------------------------ */
/*  0x762f6  InitTrigger                                              */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x762f6, 862f6_InitTrigger.c, VERIFY-TRIGGER-TOUCH-PACKET-2026-06-17): DATAFLOW_VERIFIED - nonzero-angle movedir branch, brush model call, contents/svFlags/s_flags stores, and void return checked. */
void InitTrigger(gentity_t *ent)
{
    if (ent->currentAngles[0] != 0.0f || ent->currentAngles[1] != 0.0f || ent->currentAngles[2] != 0.0f) {
        G_SetMovedir(ent->currentAngles, ent->moverDir);
    }

    trap_SetBrushModel(ent);
    ent->scriptContents = MASK_TRIGGER;
    ent->svFlags = TRIGGER_SVFLAGS;
    ent->s.eFlags |= TRIGGER_ENTITY_STATE_FLAG;
}

/* ------------------------------------------------------------------ */
/*  0x763b6  InitSentientTrigger                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x763b6, 863b6_InitSentientTrigger.c, VERIFY-TRIGGER-TOUCH-PACKET-2026-06-17): DATAFLOW_VERIFIED - scriptContents reset, no-player inversion, sentient spawnflag masks, vehicle mask, and void return checked. */
void InitSentientTrigger(gentity_t *ent)
{
    ent->scriptContents = 0;

    if ((ent->spawnflags & TRIGGER_SPAWNFLAG_NO_PLAYER) == 0) {
        ent->scriptContents |= CONTENTS_TRIGGER_TOUCH_CLIENT;
    }
    if ((ent->spawnflags & TRIGGER_SPAWNFLAG_SENTIENT_1) != 0) {
        ent->scriptContents |= CONTENTS_TRIGGER_SENTIENT_1;
    }
    if ((ent->spawnflags & TRIGGER_SPAWNFLAG_SENTIENT_2) != 0) {
        ent->scriptContents |= CONTENTS_TRIGGER_SENTIENT_2;
    }
    if ((ent->spawnflags & TRIGGER_SPAWNFLAG_SENTIENT_3) != 0) {
        ent->scriptContents |= CONTENTS_TRIGGER_SENTIENT_3;
    }
    if ((ent->spawnflags & TRIGGER_SPAWNFLAG_VEHICLE) != 0) {
        ent->scriptContents |= CONTENTS_TRIGGER_TOUCH_VEHICLE;
    }
}

/* ------------------------------------------------------------------ */
/*  0x7648d  multi_wait                                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7648d, 8648d_multi_wait.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - nextthink store to gentity +0x20c and void return checked. */
void multi_wait(gentity_t *ent)
{
    ent->nextthink = 0;
}

/* ------------------------------------------------------------------ */
/*  0x7649f  multi_trigger                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7649f, 8649f_multi_trigger.c, VERIFY-TRIGGER-TOUCH-PACKET-2026-06-17): DATAFLOW_VERIFIED - activator store, Think_GeneralLink/nextthink gate, wait/random schedule expression, free-delay path, touch clear, think assignments, and return behavior checked. */
void multi_trigger(gentity_t *ent, gentity_t *activator)
{
    ent->triggerActivator = activator;

    if (ent->think == Think_GeneralLink || ent->nextthink != 0) {
        return;
    }

    if (ent->itemWait > 0.0f) {
        ent->think = multi_wait;
        /* Apply a signed normalized sample to itemRandom, then preserve the
         * remaining wait and millisecond conversion in the x87 carrier. */
#if EMULATE_X87
        {
            x87f randSigned = x87f_load_f64(coduo_server_rand_signed_unit());
            ent->nextthink =
                level.time +
                x87f_store_i32_trunc(x87f_mul(x87f_add(x87f_load_f32(ent->itemWait), x87f_mul(randSigned, x87f_load_f32(ent->itemRandom))),
                                              x87f_load_f32(1000.0f)));
        }
#else
        ent->nextthink =
            level.time +
            game_compat_int32_from_long_double_trunc(
                ((long double)ent->itemWait + (long double)coduo_server_rand_signed_unit() * (long double)ent->itemRandom) * 1000.0f);
#endif
    } else {
        ent->touch = NULL;
        ent->nextthink = level.time + TRIGGER_FREE_DELAY_MS;
        ent->think = G_FreeEntity;
    }
}

/* ------------------------------------------------------------------ */
/*  0x765c7  Use_Multi                                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x765c7, 865c7_Use_Multi.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - callback signature adaptation, ignored other arg, multi_trigger(ent, activator) call order, and void return checked. */
void Use_Multi(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
    (void)other;

    multi_trigger(ent, activator);
}

/* ------------------------------------------------------------------ */
/*  0x765f1  Touch_Multi                                              */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x765f1, 865f1_Touch_Multi.c, VERIFY-TRIGGER-TOUCH-PACKET-2026-06-17): DATAFLOW_VERIFIED - G_Trigger then multi_trigger call order and argument order checked; source keeps ABI touch traceMode parameter ignored because Ghidra infers only used args. */
void Touch_Multi(gentity_t *ent, gentity_t *other, int traceMode)
{
    (void)traceMode;

    G_Trigger(ent, other);
    multi_trigger(ent, other);
}

/* ------------------------------------------------------------------ */
/*  0x7662d  SP_trigger_multiple                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7662d, 8662d_SP_trigger_multiple.c, VERIFY-WAVE2-ENTITY-DISPATCH-COLLISION-2026-06-17): DATAFLOW_VERIFIED - wait/random spawn floats, random>=wait clamp/print, Touch_Multi/Use_Multi installs, trigger init, sentient contents, and link call checked. */
void SP_trigger_multiple(gentity_t *ent)
{
    G_SpawnFloat("wait", TRIGGER_DEFAULT_WAIT, &ent->itemWait);
    G_SpawnFloat("random", TRIGGER_DEFAULT_RANDOM, &ent->itemRandom);

    if (ent->itemWait >= 0.0f && ent->itemWait <= ent->itemRandom) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        ent->itemRandom = ent->itemWait - 100.0f;
        G_Printf("trigger_multiple has random >= wait\n");
    }

    ent->touch = Touch_Multi;
    ent->use = Use_Multi;

    InitTrigger(ent);
    InitSentientTrigger(ent);
    trap_LinkEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x64ed2  use_trigger_use                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x64ed2, 74ed2_use_trigger_use.c, VERIFY-WAVE2-ENTITY-DISPATCH-COLLISION-2026-06-17): DATAFLOW_VERIFIED - wait gate, delay payload at +0x284, non-client spawnflag bit toggle, and ignored activator callback arg checked. */
void use_trigger_use(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
    (void)activator;

    /* 0x64eea/0x64f0a: stock fild keeps level.time exact in the 80-bit
     * register for both the compare and the add -- a (float) cast would
     * insert an extra rounding under -std=c99. */
    if ((long double)ent->itemWait < (long double)level.time) {
        ent->itemWait = (float)((long double)level.time + (long double)ent->concussiveFxEndTime);
        if (other->client == NULL) {
            if ((ent->spawnflags & TRIGGER_USE_SPAWNFLAG_TOGGLE) == 0) {
                ent->spawnflags |= TRIGGER_USE_SPAWNFLAG_TOGGLE;
            } else {
                ent->spawnflags &= ~TRIGGER_USE_SPAWNFLAG_TOGGLE;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x64f76  trigger_use                                              */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x64f76, 74f76_trigger_use.c, VERIFY-WAVE2-ENTITY-DISPATCH-COLLISION-2026-06-17): DATAFLOW_VERIFIED - brush/link order, stationary origin copy, activate contents, delay scale, cursorhint/hintstring loops, configstring writes, and Com_Error path checked. */
void trigger_use(gentity_t *ent)
{
    const char *value;
    char hintString[MAX_STRING_CHARS];
    unsigned int index;

    trap_SetBrushModel(ent);
    trap_LinkEntity(ent);

    ent->s.pos.trType = TR_STATIONARY;
    game_compat_g_copy_vector3(ent->currentOrigin, ent->s.pos.trBase);
    ent->scriptContents = TRIGGER_USE_ACTIVATE_CONTENTS;
    ent->svFlags = TRIGGER_SVFLAGS;
    ent->concussiveFxEndTime *= 1000.0f;
    ent->use = use_trigger_use;
    ent->s.cursorHint = CURSOR_HINT_ACTIVATE;

    if (G_SpawnString("cursorhint", "", &value) != 0) {
        if (Q_strcasecmp(value, "HINT_INHERIT") == 0) {
            ent->s.cursorHint = CURSOR_HINT_INHERIT;
        } else {
            for (index = 1; index < TRIGGER_USE_HINT_TABLE_LIMIT; index++) {
                if (Q_strcasecmp(value, hintStrings[index]) == 0) {
                    ent->s.cursorHint = (int)index;
                    break;
                }
            }
        }
    }

    ent->s.hintStringIndex = TRIGGER_USE_HINTSTRING_INHERIT;
    if (G_SpawnString("hintstring", "", &value) != 0) {
        for (index = 0; index < TRIGGER_USE_HINTSTRING_LIMIT; index++) {
            trap_GetConfigstring(CS_HINTSTRINGS + (int)index, hintString, MAX_STRING_CHARS);
            if (hintString[0] == '\0') {
                trap_SetConfigstring(CS_HINTSTRINGS + (int)index, value);
                ent->s.hintStringIndex = (int)(index & 0xffu);
                break;
            }
            if (strcmp(value, hintString) == 0) {
                ent->s.hintStringIndex = (int)(index & 0xffu);
                break;
            }
        }
        if (index == TRIGGER_USE_HINTSTRING_LIMIT) {
            Com_Error(1, TRIGGER_USE_HINTSTRING_LIMIT_ERROR, TRIGGER_USE_HINTSTRING_LIMIT);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x60378  G_TestEntityPosition                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x60378, 70378_G_TestEntityPosition.c, VERIFY-WAVE2-ENTITY-DISPATCH-COLLISION-2026-06-17): DATAFLOW_VERIFIED - clipmask fallback, stuck-alt early return, missile passEntityNum selection, trace args, startsolid test, and entity lookup checked. */
gentity_t *G_TestEntityPosition(gentity_t *ent, const float *origin)
{
    trace_t trace;
    int contentMask;
    int passEntityNum;

    if (ent->clipmask == 0) {
        contentMask = MOVER_TRACE_DEFAULT_CONTENTS;
    } else {
        if (ent->scriptContents == CONTENTS_STUCK_ALT) {
            return NULL;
        }
        contentMask = ent->clipmask;
    }

    if (ent->s.eType == ET_MISSILE) {
        passEntityNum = ent->passEntityNum;
    } else {
        passEntityNum = ent->s.number;
    }

    trap_Trace(&trace, origin, ent->mins, ent->maxs, origin, passEntityNum, contentMask);
    if (trace.startsolid == 0) {
        return NULL;
    }

    return &g_entities[trace.entityNum];
}

/* ------------------------------------------------------------------ */
/*  0x60487  G_CreateRotationMatrix                                   */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x60487, 70487_G_CreateRotationMatrix.c, VERIFY-WAVE2-ENTITY-DISPATCH-COLLISION-2026-06-17): DATAFLOW_VERIFIED - AngleVectors row destinations and right-vector inversion checked. */
void G_CreateRotationMatrix(const float *angles, float matrix[3][3])
{
    AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
    VectorInverse(matrix[1]);
}

/* ------------------------------------------------------------------ */
/*  0x604d3  G_TransposeMatrix                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x604d3, 704d3_G_TransposeMatrix.c, VERIFY-WAVE2-ENTITY-DISPATCH-COLLISION-2026-06-17): DATAFLOW_VERIFIED - 3x3 loop bounds and transposed store indexing checked. */
void G_TransposeMatrix(float matrix[3][3], float transpose[3][3])
{
    for (int column = 0; column < 3; column++) {
        for (int row = 0; row < 3; row++) {
            transpose[column][row] = matrix[row][column];
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x6053e  G_RotatePoint                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x6053e, 7053e_G_RotatePoint.c, VERIFY-WAVE2-ENTITY-DISPATCH-COLLISION-2026-06-17): DATAFLOW_VERIFIED - original point temporaries and row-major matrix dot products checked. */
void G_RotatePoint(float *point, float matrix[3][3])
{
    float x = point[0];
    float y = point[1];
    float z = point[2];

    /* Stock 0x60568..0x60587: each row is a ((m0*x + m1*y) + m2*z) dot kept
     * 80-bit until the point[i] store -> shim. */
#if EMULATE_X87
    for (int i = 0; i < 3; i++) {
        point[i] = x87f_store_f32(x87f_add(
            x87f_add(x87f_mul(x87f_load_f32(matrix[i][0]), x87f_load_f32(x)), x87f_mul(x87f_load_f32(matrix[i][1]), x87f_load_f32(y))),
            x87f_mul(x87f_load_f32(matrix[i][2]), x87f_load_f32(z))));
    }
#else
    point[0] = matrix[0][0] * x + matrix[0][1] * y + matrix[0][2] * z;
    point[1] = matrix[1][0] * x + matrix[1][1] * y + matrix[1][2] * z;
    point[2] = matrix[2][0] * x + matrix[2][1] * y + matrix[2][2] * z;
#endif
}

/* ------------------------------------------------------------------ */
/*  0x605e5  G_TryPushingEntity                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x605e5, 705e5_G_TryPushingEntity.c, VERIFY-WAVE2-ENTITY-DISPATCH-COLLISION-2026-06-17): DATAFLOW_VERIFIED - rotate-ground guard, translated/rotated origin math, collision probes, 4-unit fallback search, pushed-origin/client stores, stack advance, and unblocked-current-origin fallback checked. */
int G_TryPushingEntity(gentity_t *ent, gentity_t *mover, float *posDelta, float *angDelta)
{
    vec3_t pushedOrigin;
    float rotationMatrix[3][3];
    float transposeMatrix[3][3];
    vec3_t relativeOrigin;
    vec3_t originalRelativeOrigin;
    vec3_t rotateDelta;
    vec3_t trialOrigin;

    if ((mover->s.eFlags & MOVER_ROTATE_GROUND_ONLY_FLAG) != 0 && ent->s.groundEntityNum != mover->s.number) {
        return 0;
    }

    pushedOrigin[0] = ent->currentOrigin[0] + posDelta[0];
    pushedOrigin[1] = ent->currentOrigin[1] + posDelta[1];
    pushedOrigin[2] = ent->currentOrigin[2] + posDelta[2];

    G_CreateRotationMatrix(angDelta, rotationMatrix);
    G_TransposeMatrix(rotationMatrix, transposeMatrix);

    relativeOrigin[0] = pushedOrigin[0] - mover->currentOrigin[0];
    relativeOrigin[1] = pushedOrigin[1] - mover->currentOrigin[1];
    relativeOrigin[2] = pushedOrigin[2] - mover->currentOrigin[2];
    originalRelativeOrigin[0] = relativeOrigin[0];
    originalRelativeOrigin[1] = relativeOrigin[1];
    originalRelativeOrigin[2] = relativeOrigin[2];

    G_RotatePoint(relativeOrigin, transposeMatrix);
    /* 0x606eb..0x6072d: stock rounds the rotation delta to a float vector
     * first, then adds it to pushedOrigin -- two roundings per component. */
    rotateDelta[0] = relativeOrigin[0] - originalRelativeOrigin[0];
    rotateDelta[1] = relativeOrigin[1] - originalRelativeOrigin[1];
    rotateDelta[2] = relativeOrigin[2] - originalRelativeOrigin[2];
    pushedOrigin[0] += rotateDelta[0];
    pushedOrigin[1] += rotateDelta[1];
    pushedOrigin[2] += rotateDelta[2];

    if (G_TestEntityPosition(ent, pushedOrigin) == NULL) {
        game_compat_g_mover_set_pushed_origin(ent, pushedOrigin, mover->s.number, angDelta[1]);
        game_compat_g_mover_push_record_advance();
        return 1;
    }

    /* 0x60876/0x608c9/0x6091f/0x60975: the search bound is ent->maxs[0]
     * (not maxs[2]) divided by the DOUBLE constant 2.0, recomputed in the
     * 80-bit register for every comparison and never stored to a float --
     * do not hoist it into a float temporary. */
    if (MOVER_PUSH_STEP < ent->maxs[0] / 2.0) {
        for (float zStep = 0.0f; zStep < ent->maxs[0] / 2.0; zStep += MOVER_PUSH_STEP) {
            for (float zOffset = -zStep; zOffset <= zStep; zOffset += zStep + zStep) {
                for (float xStep = MOVER_PUSH_STEP; xStep < ent->maxs[0] / 2.0; xStep += MOVER_PUSH_STEP) {
                    for (float xOffset = -xStep; xOffset <= xStep; xOffset += xStep + xStep) {
                        for (float yStep = MOVER_PUSH_STEP; yStep < ent->maxs[0] / 2.0; yStep += MOVER_PUSH_STEP) {
                            for (float yOffset = -yStep; yOffset <= yStep; yOffset += yStep + yStep) {
                                trialOrigin[0] = pushedOrigin[0] + xOffset;
                                trialOrigin[1] = pushedOrigin[1] + yOffset;
                                trialOrigin[2] = pushedOrigin[2] + zOffset;

                                if (G_TestEntityPosition(ent, trialOrigin) == NULL) {
                                    game_compat_g_mover_set_pushed_origin(ent, trialOrigin, mover->s.number, angDelta[1]);
                                    game_compat_g_mover_push_record_advance();
                                    return 1;
                                }
                            }
                        }
                    }
                }
                if (zOffset == 0.0f) {
                    break;
                }
            }
        }
    }

    if (G_TestEntityPosition(ent, ent->currentOrigin) == NULL) {
        ent->s.groundEntityNum = ENTITYNUM_NONE;
        return 1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  0x79dd9  G_KillBox                                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x79dd9, 89dd9_G_KillBox.c, VERIFY-WAVE2-ENTITY-DISPATCH-COLLISION-2026-06-17): DATAFLOW_VERIFIED - client-origin bounds, body contents box query, linked client filter, and telefrag damage tuple checked. */
void G_KillBox(gentity_t *ent)
{
    int entityNums[KILLBOX_MAX_ENTITIES];
    vec3_t mins;
    vec3_t maxs;
    int count;

    mins[0] = ent->client->ps.psOrigin[0] + ent->mins[0];
    mins[1] = ent->client->ps.psOrigin[1] + ent->mins[1];
    mins[2] = ent->client->ps.psOrigin[2] + ent->mins[2];
    maxs[0] = ent->client->ps.psOrigin[0] + ent->maxs[0];
    maxs[1] = ent->client->ps.psOrigin[1] + ent->maxs[1];
    maxs[2] = ent->client->ps.psOrigin[2] + ent->maxs[2];

    count = trap_EntitiesInBox(mins, maxs, entityNums, KILLBOX_MAX_ENTITIES, CONTENTS_BODY);
    for (int i = 0; i < count; i++) {
        gentity_t *hit = &g_entities[entityNums[i]];

        if (hit->client != NULL && hit->linkedState != 0) {
            G_Damage(hit, ent, ent, NULL, NULL, KILLBOX_DAMAGE, DAMAGE_NO_PROTECTION, MOD_TELEFRAG, HITLOC_NONE);
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: mover push candidate type predicate; extracted during reconstruction of 0x60c1b. */
/* VERIFIED_DECOMPILER(0x60c1b, 70c1b_FUN_00070c1b.c, VERIFY-WAVE2-ENTITY-DISPATCH-COLLISION-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against parent mover-push candidate test for missile/item/player/linked byte. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_MoverPush (0x60c1b); no standalone original body. */
static int game_compat_g_mover_push_entity_type(gentity_t *ent)
{
    return ent->s.eType == ET_MISSILE || ent->s.eType == ET_ITEM || ent->s.eType == ET_PLAYER || ent->linkedByte16d != 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: mover push bounds predicate; extracted during reconstruction of 0x60c1b. */
/* VERIFIED_DECOMPILER(0x60c1b, 70c1b_FUN_00070c1b.c, VERIFY-WAVE2-ENTITY-DISPATCH-COLLISION-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against parent strict absMin/absMax overlap chain. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_MoverPush (0x60c1b); no standalone original body. */
static int game_compat_g_mover_push_bounds_overlap(const gentity_t *ent, const float *mins, const float *maxs)
{
    return ent->absMin[0] < maxs[0] && ent->absMin[1] < maxs[1] && ent->absMin[2] < maxs[2] && mins[0] < ent->absMax[0] &&
           mins[1] < ent->absMax[1] && mins[2] < ent->absMax[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: pushed stack record writer; extracted during reconstruction of 0x60c1b. */
/* VERIFIED_DECOMPILER(0x60c1b, 70c1b_FUN_00070c1b.c, VERIFY-WAVE2-ENTITY-DISPATCH-COLLISION-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against parent push-stack stores for ent, currentOrigin, and yaw delta. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_MoverPush (0x60c1b); no standalone original body. */
static void game_compat_g_mover_push_record(gentity_t *ent, const float *angDelta)
{
    mover_push_record_t *record;

    record = moverPushStackCursor;
    record->ent = ent;
    record->origin[0] = ent->currentOrigin[0];
    record->origin[1] = ent->currentOrigin[1];
    record->origin[2] = ent->currentOrigin[2];
    record->yawDelta = angDelta[1];
}

/* VERIFIED_DECOMPILER(0x60c1b, 70c1b_FUN_00070c1b.c, VERIFY-WAVE2-ENTITY-DISPATCH-COLLISION-2026-06-17): DATAFLOW_VERIFIED - blocker clear, rotating/swept bounds, mover unlink/link ordering, candidate filtering, push-stack record writes, TryPushing result paths, item relink, sine crush damage, blocker return, and final relink checked. */
int G_MoverPush(gentity_t *mover, float *posDelta, float *angDelta, gentity_t **blocker)
{
    int entityList[MOVER_PUSH_MAX_ENTITIES];
    int pushList[MOVER_PUSH_MAX_ENTITIES];
    int listedEntities;
    int pushCount;
    vec3_t finalMins;
    vec3_t finalMaxs;
    vec3_t queryMins;
    vec3_t queryMaxs;
    int rotates;

    *blocker = 0;

    rotates = mover->currentAngles[0] != 0.0f || mover->currentAngles[1] != 0.0f || mover->currentAngles[2] != 0.0f ||
              angDelta[0] != 0.0f || angDelta[1] != 0.0f || angDelta[2] != 0.0f;

    if (rotates) {
        float radius = RadiusFromBounds(mover->mins, mover->maxs);

        for (int axis = 0; axis < 3; axis++) {
            finalMins[axis] = mover->currentOrigin[axis] - radius + posDelta[axis];
            finalMaxs[axis] = mover->currentOrigin[axis] + radius + posDelta[axis];
            queryMins[axis] = mover->currentOrigin[axis] - radius;
            queryMaxs[axis] = mover->currentOrigin[axis] + radius;
        }
    } else {
        for (int axis = 0; axis < 3; axis++) {
            finalMins[axis] = mover->absMin[axis] + posDelta[axis];
            finalMaxs[axis] = mover->absMax[axis] + posDelta[axis];
            queryMins[axis] = mover->absMin[axis];
            queryMaxs[axis] = mover->absMax[axis];
        }
    }

    for (int axis = 0; axis < 3; axis++) {
        if (posDelta[axis] > 0.0f) {
            queryMaxs[axis] += posDelta[axis];
        } else {
            queryMins[axis] += posDelta[axis];
        }
    }

    trap_UnlinkEntity(mover);
    listedEntities = trap_EntitiesInBox(queryMins, queryMaxs, entityList, MOVER_PUSH_MAX_ENTITIES, MOVER_PUSH_QUERY_MASK);

    for (int axis = 0; axis < 3; axis++) {
        mover->currentOrigin[axis] += posDelta[axis];
        mover->currentAngles[axis] += angDelta[axis];
    }
    trap_LinkEntity(mover);

    pushCount = 0;
    for (int index = 0; index < listedEntities; index++) {
        gentity_t *check = &g_entities[entityList[index]];

        if (!game_compat_g_mover_push_entity_type(check)) {
            continue;
        }

        if (check->s.groundEntityNum != mover->s.number) {
            if (!game_compat_g_mover_push_bounds_overlap(check, finalMins, finalMaxs)) {
                continue;
            }
            if (G_TestEntityPosition(check, check->currentOrigin) != mover) {
                continue;
            }
        }

        pushList[pushCount] = entityList[index];
        pushCount++;
    }

    for (int index = 0; index < pushCount; index++) {
        trap_UnlinkEntity(&g_entities[pushList[index]]);
    }

    for (int index = 0; index < pushCount; index++) {
        gentity_t *check = &g_entities[pushList[index]];

        game_compat_g_mover_push_record(check, angDelta);
        if (G_TryPushingEntity(check, mover, posDelta, angDelta) == 0) {
            if (check->s.eType == ET_ITEM) {
                trap_LinkEntity(check);
            } else if (mover->s.pos.trType != TR_SINE && mover->s.apos.trType != TR_SINE) {
                *blocker = check;
                return 0;
            } else {
                G_Damage(check, mover, mover, NULL, NULL, MOVER_CRUSH_DAMAGE, 0, MOD_CRUSH, 0);
            }
        } else {
            trap_LinkEntity(check);
        }
    }

    for (int index = 0; index < pushCount; index++) {
        trap_LinkEntity(&g_entities[pushList[index]]);
    }

    return 1;
}

/* VERIFIED_DECOMPILER(0x612f9, 712f9_G_MoverTeam.c, VERIFY-NEXT-006-BINARY-MOVER-TEAM-2026-06-17): DATAFLOW_VERIFIED - push-stack reset, team trajectory evaluation, G_MoverPush argument order, reached-callback gates, rollback record stores/client yaw delta, frame-time rewind, relinks, and blocker callback checked. */
void G_MoverTeam(gentity_t *ent)
{
    gentity_t *blocker;
    gentity_t *current;
    vec3_t posEval;
    vec3_t angEval;
    vec3_t posDelta;
    vec3_t angDelta;
    int result;

    blocker = NULL;
    moverPushStackCursor = pushed;
    current = ent;

    while (1) {
        if (current == NULL) {
            for (current = ent; current != NULL; current = current->teamChain) {
                /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                if (current->s.pos.trType != TR_STATIONARY &&
                    coduo_int32_from_bits((uint32_t)current->s.pos.trTime + (uint32_t)current->s.pos.trDuration) <= level.time &&
                    current->moverReached != NULL) {
                    current->moverReached(current);
                }
                if (current->s.apos.trType != TR_STATIONARY &&
                    coduo_int32_from_bits((uint32_t)current->s.apos.trTime + (uint32_t)current->s.apos.trDuration) <= level.time &&
                    current->moverReached != NULL) {
                    current->moverReached(current);
                }
            }
            return;
        }

        BG_EvaluateTrajectory(&current->s.pos, level.time, posEval);
        BG_EvaluateTrajectory(&current->s.apos, level.time, angEval);

        posDelta[0] = posEval[0] - current->currentOrigin[0];
        posDelta[1] = posEval[1] - current->currentOrigin[1];
        posDelta[2] = posEval[2] - current->currentOrigin[2];
        angDelta[0] = angEval[0] - current->currentAngles[0];
        angDelta[1] = angEval[1] - current->currentAngles[1];
        angDelta[2] = angEval[2] - current->currentAngles[2];

        result = G_MoverPush(current, posDelta, angDelta, &blocker);
        if (result == 0)
            break;

        current = current->teamChain;
    }

    while (moverPushStackCursor > pushed) {
        mover_push_record_t *record;
        gentity_t *pushed;

        moverPushStackCursor--;
        record = moverPushStackCursor;
        pushed = record->ent;

        pushed->currentOrigin[0] = record->origin[0];
        pushed->currentOrigin[1] = record->origin[1];
        pushed->currentOrigin[2] = record->origin[2];
        pushed->s.pos.trBase[0] = record->origin[0];
        pushed->s.pos.trBase[1] = record->origin[1];
        pushed->s.pos.trBase[2] = record->origin[2];

        if (pushed->client != NULL) {
            pushed->client->ps.deltaAngles[1] -= game_compat_g_mover_angle_to_short(record->yawDelta);
            pushed->client->ps.psOrigin[0] = record->origin[0];
            pushed->client->ps.psOrigin[1] = record->origin[1];
            pushed->client->ps.psOrigin[2] = record->origin[2];
        }

        trap_LinkEntity(pushed);
    }

    for (current = ent; current != NULL; current = current->teamChain) {
        uint32_t frameDelta = (uint32_t)level.time - (uint32_t)level.previousTime;

        current->s.pos.trTime = coduo_int32_from_bits((uint32_t)current->s.pos.trTime + frameDelta);
        current->s.apos.trTime = coduo_int32_from_bits((uint32_t)current->s.apos.trTime + frameDelta);
        BG_EvaluateTrajectory(&current->s.pos, level.time, current->currentOrigin);
        BG_EvaluateTrajectory(&current->s.apos, level.time, current->currentAngles);
        trap_LinkEntity(current);
    }

    if (ent->moverBlocked != NULL) {
        ent->moverBlocked(ent, blocker);
    }
}

/* ------------------------------------------------------------------ */
/*  0x631c0  InitMoverRotate                                          */
/* ------------------------------------------------------------------ */

/* NOT_FROM_ORIGINAL_SOURCE: constant-light component clamp; extracted during reconstruction of 0x62e2e. */
/* VERIFIED_DECOMPILER(0x631c0, 731c0_InitMoverRotate.c, VERIFY-ENTITY-DISPATCH-MOVER-PACKET-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against x87 truncate, upper clamp, no lower clamp, and packed constant-light component use. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original InitMoverRotate (0x631c0); no standalone original body. */
static uint32_t game_compat_g_mover_light_component(float value, float scale)
{
    int32_t component = game_compat_int32_from_long_double_trunc((long double)value * (long double)scale);

    if (component > 255) {
        component = 255;
    }

    return (uint32_t)component;
}

/* NOT_FROM_ORIGINAL_SOURCE: alternate door speed predicate; extracted during reconstruction of 0x61791. */
/* VERIFIED_DECOMPILER(0x61791, 71791_SetMoverState.c, VERIFY-ENTITY-DISPATCH-MOVER-PACKET-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against the nonzero-or-NaN doorAltSpeed branch that selects moverAltDuration. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original SetMoverState (0x61791); no standalone original body. */
static int game_compat_g_binary_mover_has_alt_speed(const gentity_t *ent)
{
    return ent->doorAltSpeed != 0.0f || isnan(ent->doorAltSpeed);
}

/* NOT_FROM_ORIGINAL_SOURCE: linear position trajectory setup; extracted during reconstruction of 0x61791. */
/* VERIFIED_DECOMPILER(0x61791, 71791_SetMoverState.c, VERIFY-ENTITY-DISPATCH-MOVER-PACKET-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against pos.trBase, trDuration, trDelta, and TR_LINEAR_STOP stores for door move cases 3 through 6. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original SetMoverState (0x61791); no standalone original body. */
static void game_compat_g_binary_mover_set_pos_move(gentity_t *ent, const float *start, const float *end, int duration)
{
    vec3_t delta;
    float scale;

    game_compat_g_copy_vector3(start, ent->s.pos.trBase);
    ent->s.pos.trDuration = duration;
    /* 0x618e7..0x61952: stock rounds end-start to a float vector first,
     * then multiplies by scale -- two roundings per component.  The fild
     * keeps duration exact: no (float)duration cast rounding. */
    delta[0] = end[0] - start[0];
    delta[1] = end[1] - start[1];
    delta[2] = end[2] - start[2];
    /* Stock 0x61923: 1000.0f / duration, duration via fild (int->80, exact, no
     * (float)duration round) -> single divide, shim. delta subs and the
     * delta*scale trDelta muls are single ops stored to floats (native). */
#if EMULATE_X87
    scale = x87f_store_f32(x87f_div(x87f_load_f32(1000.0f), x87f_load_i32(duration)));
#else
    scale = (float)((long double)1000.0f / (long double)duration);
#endif
    ent->s.pos.trDelta[0] = delta[0] * scale;
    ent->s.pos.trDelta[1] = delta[1] * scale;
    ent->s.pos.trDelta[2] = delta[2] * scale;
    ent->s.pos.trType = TR_LINEAR_STOP;
}

/* NOT_FROM_ORIGINAL_SOURCE: stationary angle trajectory setup; extracted during reconstruction of 0x61791. */
/* VERIFIED_DECOMPILER(0x61791, 71791_SetMoverState.c, VERIFY-NEXT-006-BINARY-MOVER-TEAM-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against rotate states 7 and 8 copying currentAngles (+0x148) to apos.trBase and setting TR_STATIONARY. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original SetMoverState (0x61791); no standalone original body. */
static void game_compat_g_binary_mover_set_angle_stationary(gentity_t *ent)
{
    game_compat_g_copy_vector3(ent->currentAngles, ent->s.apos.trBase);
    ent->s.apos.trType = TR_STATIONARY;
}

/* NOT_FROM_ORIGINAL_SOURCE: rotate-door opening trajectory setup; extracted during reconstruction of 0x61791. */
/* VERIFIED_DECOMPILER(0x61791, 71791_SetMoverState.c, VERIFY-NEXT-006-BINARY-MOVER-TEAM-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against rotate state 9 zero base, quiet flag timing branch, damageDir/doorYawOffset delta math, and TR_LINEAR_STOP store. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original SetMoverState (0x61791); no standalone original body. */
static void game_compat_g_binary_mover_set_rotate_move_to_pos2(gentity_t *ent, int slow)
{
    float scale;

    ent->s.apos.trBase[0] = 0.0f;
    ent->s.apos.trBase[1] = 0.0f;
    ent->s.apos.trBase[2] = 0.0f;

    /* 0x61c6c/0x61c93: fild keeps moverDuration exact -- no (float) cast
     * rounding on the divisor. */
    /* Stock 0x61c6c/0x61c93: const / moverDuration, moverDuration via fild
     * (int->80, exact) -> single divide, shim. */
    if (slow == 0) {
#if EMULATE_X87
        scale = x87f_store_f32(x87f_div(x87f_load_f32(1000.0f), x87f_load_i32(ent->moverDuration)));
#else
        scale = (float)((long double)1000.0f / (long double)ent->moverDuration);
#endif
        ent->s.apos.trDuration = ent->moverDuration;
    } else {
#if EMULATE_X87
        scale = x87f_store_f32(x87f_div(x87f_load_f32(500.0f), x87f_load_i32(ent->moverDuration)));
#else
        scale = (float)((long double)500.0f / (long double)ent->moverDuration);
#endif
        ent->s.apos.trDuration = coduo_int32_from_bits((uint32_t)ent->moverDuration * UINT32_C(2));
    }

    /* 0x61cbc..0x61d07: stock multiplies scale*doorYawOffset first, then by
     * damageDir -- keep that 80-bit grouping (fmul is not associative) -> shim
     * each (scale*doorYawOffset)*damageDir[i] 2-mul cascade. */
#if EMULATE_X87
    for (int i = 0; i < 3; i++) {
        ent->s.apos.trDelta[i] =
            x87f_store_f32(x87f_mul(x87f_mul(x87f_load_f32(scale), x87f_load_f32(ent->doorYawOffset)), x87f_load_f32(ent->damageDir[i])));
    }
#else
    ent->s.apos.trDelta[0] = scale * ent->doorYawOffset * ent->damageDir[0];
    ent->s.apos.trDelta[1] = scale * ent->doorYawOffset * ent->damageDir[1];
    ent->s.apos.trDelta[2] = scale * ent->doorYawOffset * ent->damageDir[2];
#endif
    ent->s.apos.trType = TR_LINEAR_STOP;
}

/* NOT_FROM_ORIGINAL_SOURCE: rotate-door closing trajectory setup; extracted during reconstruction of 0x61791. */
/* VERIFIED_DECOMPILER(0x61791, 71791_SetMoverState.c, VERIFY-NEXT-006-BINARY-MOVER-TEAM-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against rotate state 10 scaled base, quiet flag duration doubling/0.5 scale, negative trDelta stores, and TR_LINEAR_STOP store. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original SetMoverState (0x61791); no standalone original body. */
static void game_compat_g_binary_mover_set_rotate_move_to_pos1(gentity_t *ent, int slow)
{
    /* Stock 0x61d64: 1000.0f / moverDuration, moverDuration via fild (int->80,
     * exact) -> single divide, shim. trBase = damageDir*doorYawOffset,
     * scale*=0.5 (exact) and trDelta = -trBase*scale (negate + single mul) all
     * native. */
#if EMULATE_X87
    float scale = x87f_store_f32(x87f_div(x87f_load_f32(1000.0f), x87f_load_i32(ent->moverDuration)));
#else
    float scale = (float)((long double)1000.0f / (long double)ent->moverDuration);
#endif

    ent->s.apos.trBase[0] = ent->damageDir[0] * ent->doorYawOffset;
    ent->s.apos.trBase[1] = ent->damageDir[1] * ent->doorYawOffset;
    ent->s.apos.trBase[2] = ent->damageDir[2] * ent->doorYawOffset;
    ent->s.apos.trDuration = ent->moverDuration;

    if (slow != 0) {
        ent->s.apos.trDuration = coduo_int32_from_bits((uint32_t)ent->s.apos.trDuration * UINT32_C(2));
        scale *= 0.5f;
    }

    ent->s.apos.trDelta[0] = -ent->s.apos.trBase[0] * scale;
    ent->s.apos.trDelta[1] = -ent->s.apos.trBase[1] * scale;
    ent->s.apos.trDelta[2] = -ent->s.apos.trBase[2] * scale;
    ent->s.apos.trType = TR_LINEAR_STOP;
}

/* ------------------------------------------------------------------ */
/*  0x61791  SetMoverState                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x61791, 71791_SetMoverState.c, VERIFY-ENTITY-DISPATCH-MOVER-PACKET-2026-06-17): DATAFLOW_VERIFIED - state byte/time stores, all 11 switch cases, position/angle trajectory writes, NaN alt-speed branch, quiet rotate timing, activeState clears, BG_EvaluateTrajectory call, and link guard checked. */
void SetMoverState(gentity_t *ent, int moverState, int time)
{
    int slowRotate = (ent->flags & BINARY_MOVER_QUIET_FLAG) != 0;

    ent->moverState = (uint8_t)((binary_mover_state_t)moverState);
    ent->s.pos.trTime = time;
    ent->s.apos.trTime = time;

    switch ((binary_mover_state_t)moverState) {
    case MOVER_STATE_DOOR_POS1:
        game_compat_g_copy_vector3(ent->moverPos1, ent->s.pos.trBase);
        ent->s.pos.trType = TR_STATIONARY;
        ent->activeState = 0;
        break;
    case MOVER_STATE_DOOR_POS2:
        game_compat_g_copy_vector3(ent->moverPos2, ent->s.pos.trBase);
        ent->s.pos.trType = TR_STATIONARY;
        break;
    case MOVER_STATE_DOOR_POS3:
        game_compat_g_copy_vector3(ent->damagePoint, ent->s.pos.trBase);
        ent->s.pos.trType = TR_STATIONARY;
        break;
    case MOVER_STATE_DOOR_MOVING_TO_POS2:
        game_compat_g_binary_mover_set_pos_move(ent, ent->moverPos1, ent->moverPos2, ent->moverDuration);
        break;
    case MOVER_STATE_DOOR_MOVING_TO_POS1:
        game_compat_g_binary_mover_set_pos_move(ent, ent->moverPos2, ent->moverPos1,
                                                game_compat_g_binary_mover_has_alt_speed(ent) ? ent->moverAltDuration : ent->moverDuration);
        break;
    case MOVER_STATE_DOOR_MOVING_TO_POS3:
        game_compat_g_binary_mover_set_pos_move(ent, ent->moverPos2, ent->damagePoint, ent->s.pos.trDuration);
        break;
    case MOVER_STATE_DOOR_MOVING_TO_POS2_FROM_POS3:
        game_compat_g_binary_mover_set_pos_move(ent, ent->damagePoint, ent->moverPos2, ent->s.pos.trDuration);
        break;
    case MOVER_STATE_ROTATE_POS1:
    case MOVER_STATE_ROTATE_POS2:
        game_compat_g_binary_mover_set_angle_stationary(ent);
        break;
    case MOVER_STATE_ROTATE_MOVING_TO_POS2:
        game_compat_g_binary_mover_set_rotate_move_to_pos2(ent, slowRotate);
        break;
    case MOVER_STATE_ROTATE_MOVING_TO_POS1:
        game_compat_g_binary_mover_set_rotate_move_to_pos1(ent, slowRotate);
        ent->activeState = 0;
        break;
    }

    BG_EvaluateTrajectory(&ent->s.pos, level.time, ent->currentOrigin);
    if ((ent->svFlags & BINARY_MOVER_SKIP_LINK_FLAG) == 0 || ent->scriptContents != 0) {
        trap_LinkEntity(ent);
    }
}

/* ------------------------------------------------------------------ */
/*  0x61e5f  MatchTeam                                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x61e5f, 71e5f_MatchTeam.c, VERIFY-ENTITY-DISPATCH-MOVER-PACKET-2026-06-17): DATAFLOW_VERIFIED - team-chain loop, original-entity quiet flag propagation, SetMoverState argument order, and void return checked. */
void MatchTeam(gentity_t *ent, int moverState, int time)
{
    int propagateQuiet = (ent->flags & BINARY_MOVER_QUIET_FLAG) != 0;

    for (gentity_t *current = ent; current != NULL; current = current->teamChain) {
        if (propagateQuiet != 0) {
            current->flags |= BINARY_MOVER_QUIET_FLAG;
        }
        SetMoverState(current, moverState, time);
    }
}

/* ------------------------------------------------------------------ */
/*  0x61ed6  MatchTeamReverseAngleOnSlaves                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x61ed6, 71ed6_MatchTeamReverseAngleOnSlaves.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - teamChain loop, per-slave yaw negation, quiet flag propagation from master, SetMoverState args, and void return checked. */
static void MatchTeamReverseAngleOnSlaves(gentity_t *ent, int moverState, int time)
{
    int propagateQuiet = (ent->flags & BINARY_MOVER_QUIET_FLAG) != 0;

    for (gentity_t *current = ent; current != NULL; current = current->teamChain) {
        current->doorYawOffset = -current->doorYawOffset;
        if (propagateQuiet != 0) {
            current->flags |= BINARY_MOVER_QUIET_FLAG;
        }
        SetMoverState(current, moverState, time);
    }
}

/* ------------------------------------------------------------------ */
/*  0x61f67  ReturnToPos1                                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x61f67, 71f67_ReturnToPos1.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - MatchTeam state/time, closing sound alias, close-loop clientSound byte zero-extension, and void return checked. */
void ReturnToPos1(gentity_t *ent)
{
    MatchTeam(ent, MOVER_STATE_DOOR_MOVING_TO_POS1, level.time);
    G_PlaySoundAlias(ent, ent->doorSoundClosing);
    ent->s.clientSound = ent->doorSoundCloseLoop;
}

/* NOT_FROM_ORIGINAL_SOURCE: PVS sound gate extracted from rotate-door return; extracted during reconstruction of 0x620c3. */
/* VERIFIED_DECOMPILER(0x620c3, 720c3_ReturnToPos1Rotate.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against G_Find(NULL, scriptClassname, player), trap_InPVS(player currentOrigin, ent currentOrigin), and zero return when no player. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original ReturnToPos1Rotate (0x620c3); no standalone original body. */
static int game_compat_g_binary_mover_first_player_in_pvs(gentity_t *ent)
{
    gentity_t *player = G_Find(NULL, offsetof(gentity_t, scriptClassname), scr_const_player);

    return player != NULL && trap_InPVS(player->currentOrigin, ent->currentOrigin) != qfalse;
}

/* ------------------------------------------------------------------ */
/*  0x620c3  ReturnToPos1Rotate                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x620c3, 720c3_ReturnToPos1Rotate.c, VERIFY-ENTITY-DISPATCH-MOVER-PACKET-2026-06-17): DATAFLOW_VERIFIED - MatchTeam state/time, first-player PVS gate, quiet closing sound selection, close-loop clientSound store, and void return checked. */
void ReturnToPos1Rotate(gentity_t *ent)
{
    MatchTeam(ent, MOVER_STATE_ROTATE_MOVING_TO_POS1, level.time);
    if (game_compat_g_binary_mover_first_player_in_pvs(ent) != 0) {
        if ((ent->flags & BINARY_MOVER_QUIET_FLAG) == 0) {
            G_PlaySoundAlias(ent, ent->doorSoundClosing);
        } else {
            G_PlaySoundAlias(ent, ent->doorSoundClosingQuiet);
        }
    }
    ent->s.clientSound = ent->doorSoundCloseLoop;
}

/* NOT_FROM_ORIGINAL_SOURCE: area portal master guard; extracted during reconstruction of 0x6282e. */
/* VERIFIED_DECOMPILER(0x6282e, 7282e_Use_BinaryMover.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against teamMaster self/null guard and trap_AdjustAreaPortalState(ent, open) calls. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original Use_BinaryMover (0x6282e); no standalone original body. */
static void game_compat_g_binary_mover_adjust_area_portal(gentity_t *ent, qboolean open)
{
    if (ent->teamMaster == ent || ent->teamMaster == NULL) {
        trap_AdjustAreaPortalState(ent, open);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: binary mover auto-return scheduling; extracted during reconstruction of 0x621ae. */
/* VERIFIED_DECOMPILER(0x621ae, 721ae_Reached_BinaryMover.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against toggle activeState clear, think install, nextthink zero, and non-toggle wait sentinel/x87 truncate wait+level.time scheduling. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original Reached_BinaryMover (0x621ae); no standalone original body. */
static void game_compat_g_binary_mover_schedule_return(gentity_t *ent, void (*think)(gentity_t *))
{
    if ((ent->flags & BINARY_MOVER_TOGGLE_FLAG) == 0) {
        if (ent->itemWait != BINARY_MOVER_NO_AUTORETURN_WAIT) {
            int32_t wait = game_compat_int32_from_long_double_trunc((long double)ent->itemWait);

            ent->think = think;
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)wait);
        }
    } else {
        ent->activeState = 0;
        ent->think = think;
        ent->nextthink = 0;
    }
}

/* ------------------------------------------------------------------ */
/*  0x621ae  Reached_BinaryMover                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x621ae, 721ae_Reached_BinaryMover.c, VERIFY-ENTITY-DISPATCH-MOVER-PACKET-2026-06-17): DATAFLOW_VERIFIED - clientSound clear, four mover-state branches, sound aliases, triggerActivator defaulting, auto-return scheduling including NaN wait behavior, quiet flag clear, portal guard, and error path checked. */
void Reached_BinaryMover(gentity_t *ent)
{
    int normalSound = (ent->flags & BINARY_MOVER_QUIET_FLAG) == 0;

    ent->s.clientSound = 0;

    if (ent->moverState == MOVER_STATE_DOOR_MOVING_TO_POS2) {
        SetMoverState(ent, MOVER_STATE_DOOR_POS2, level.time);
        G_PlaySoundAlias(ent, normalSound ? ent->doorSoundOpenEnd : ent->doorSoundOpenQuietEnd);
        if (ent->triggerActivator == NULL) {
            ent->triggerActivator = ent;
        }
        game_compat_g_binary_mover_schedule_return(ent, ReturnToPos1);
    } else if (ent->moverState == MOVER_STATE_DOOR_MOVING_TO_POS1) {
        SetMoverState(ent, MOVER_STATE_DOOR_POS1, level.time);
        G_PlaySoundAlias(ent, normalSound ? ent->doorSoundCloseEnd : ent->doorSoundCloseQuietEnd);
        game_compat_g_binary_mover_adjust_area_portal(ent, qfalse);
    } else if (ent->moverState == MOVER_STATE_ROTATE_MOVING_TO_POS2) {
        SetMoverState(ent, MOVER_STATE_ROTATE_POS2, level.time);
        G_PlaySoundAlias(ent, normalSound ? ent->doorSoundOpenEnd : ent->doorSoundOpenQuietEnd);
        if (ent->triggerActivator == NULL) {
            ent->triggerActivator = ent;
        }
        game_compat_g_binary_mover_schedule_return(ent, ReturnToPos1Rotate);
    } else if (ent->moverState == MOVER_STATE_ROTATE_MOVING_TO_POS1) {
        SetMoverState(ent, MOVER_STATE_ROTATE_POS1, level.time);
        if (game_compat_g_binary_mover_first_player_in_pvs(ent) != 0) {
            G_PlaySoundAlias(ent, normalSound ? ent->doorSoundCloseEnd : ent->doorSoundCloseQuietEnd);
        }
        ent->flags &= ~BINARY_MOVER_QUIET_FLAG;
        game_compat_g_binary_mover_adjust_area_portal(ent, qfalse);
    } else {
        G_Error("Reached_BinaryMover: bad moverState");
    }
}

/* ------------------------------------------------------------------ */
/*  0x62608  IsBinaryMoverBlocked                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x62608, 72608_IsBinaryMoverBlocked.c, VERIFY-ENTITY-DISPATCH-MOVER-PACKET-2026-06-17): DATAFLOW_VERIFIED - classname/spawnflag gates, center and angle vectors, NaN-aware axis selection, AngleVectors/VectorNormalize calls, dot-product comparison, and return values checked. */
static int IsBinaryMoverBlocked(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
    vec3_t center;
    vec3_t toCenter;
    vec3_t angles;
    vec3_t forward;
    vec3_t toActivator;

    (void)other;

    if (ent->scriptClassname != scr_const_func_door_rotating) {
        return 0;
    }
    if ((ent->spawnflags & FUNC_DOOR_ROTATING_SPAWNFLAG_SKIP_BLOCK_CHECK) != 0) {
        return 0;
    }

    /* 0x62659..0x626b9: stock rounds each absMin+absMax sum to float,
     * then scales by 0.5f in a second rounding. */
    center[0] = ent->absMin[0] + ent->absMax[0];
    center[1] = ent->absMin[1] + ent->absMax[1];
    center[2] = ent->absMin[2] + ent->absMax[2];
    center[0] *= 0.5f;
    center[1] *= 0.5f;
    center[2] *= 0.5f;

    toCenter[0] = center[0] - ent->currentOrigin[0];
    toCenter[1] = center[1] - ent->currentOrigin[1];
    toCenter[2] = center[2] - ent->currentOrigin[2];
    vectoangles(toCenter, angles);

    if (ent->damageDir[1] != 0.0f || isnan(ent->damageDir[1])) {
        angles[1] += ent->doorYawOffset;
    } else if (ent->damageDir[0] != 0.0f || isnan(ent->damageDir[0])) {
        angles[0] += ent->doorYawOffset;
    } else if (ent->damageDir[2] != 0.0f || isnan(ent->damageDir[2])) {
        angles[2] += ent->doorYawOffset;
    }

    AngleVectors(angles, forward, NULL, NULL);

    toActivator[0] = activator->currentOrigin[0] - center[0];
    toActivator[1] = activator->currentOrigin[1] - center[1];
    toActivator[2] = activator->currentOrigin[2] - center[2];
    VectorNormalize(toActivator);

    /* Stock 0x627dc..0x62800: the dot is a 3-mul/2-add chain kept 80-bit, stored
     * to a float local (fstp DWORD 0x627f2), then compared >= 0 (fucompp on the
     * stored float) -> shim the dot, native compare. center = absMin+absMax
     * (single add stored) then *0.5 (exact power-of-two scale) stay native. */
    {
#if EMULATE_X87
        float dot = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(toActivator[0]), x87f_load_f32(forward[0])),
                                                     x87f_mul(x87f_load_f32(toActivator[1]), x87f_load_f32(forward[1]))),
                                            x87f_mul(x87f_load_f32(toActivator[2]), x87f_load_f32(forward[2]))));
#else
        float dot = toActivator[0] * forward[0] + toActivator[1] * forward[1] + toActivator[2] * forward[2];
#endif

        return dot >= 0.0f;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: quiet/normal opening sound selection; extracted during reconstruction of 0x6282e. */
/* VERIFIED_DECOMPILER(0x6282e, 7282e_Use_BinaryMover.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against normal opening alias at +0x16f and quiet opening alias at +0x176. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original Use_BinaryMover (0x6282e); no standalone original body. */
static void game_compat_g_binary_mover_start_opening_sound(gentity_t *ent, int quiet)
{
    if (quiet == 0) {
        G_PlaySoundAlias(ent, ent->doorSoundOpening);
    } else {
        G_PlaySoundAlias(ent, ent->doorSoundOpeningQuiet);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: open-loop sound and portal setup; extracted during reconstruction of 0x6282e. */
/* VERIFIED_DECOMPILER(0x6282e, 7282e_Use_BinaryMover.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against clientSound clear, optional open-loop byte at +0x173, and area portal open guard. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original Use_BinaryMover (0x6282e); no standalone original body. */
static void game_compat_g_binary_mover_start_open_loop(gentity_t *ent, int playSounds)
{
    ent->s.clientSound = 0;
    if (playSounds != 0) {
        ent->s.clientSound = ent->doorSoundOpenLoop;
    }
    game_compat_g_binary_mover_adjust_area_portal(ent, qtrue);
}

/* ------------------------------------------------------------------ */
/*  0x6282e  Use_BinaryMover                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x6282e, 7282e_Use_BinaryMover.c, VERIFY-WAVE3-ENTITY-DISPATCH-C08-2026-06-17): DATAFLOW_VERIFIED - slave recursion/quiet propagation, blocked rotate opening path, target-location sentinel clear, state-specific MatchTeam/Blocked calls, wait scheduling, sound aliases, loop sound, portal adjustment helper, and toggle branches checked. */
void Use_BinaryMover(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
    int quiet = (ent->flags & BINARY_MOVER_QUIET_FLAG) != 0;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    int playSounds = level.time > BINARY_MOVER_SOUND_STARTUP_MS;
    int blocked = 0;

    if ((ent->flags & BINARY_MOVER_TEAM_SLAVE_FLAG) != 0) {
        if (quiet != 0) {
            ent->teamMaster->flags |= BINARY_MOVER_QUIET_FLAG;
        }
        Use_BinaryMover(ent->teamMaster, other, activator);
        return;
    }

    if (ent->moverState == MOVER_STATE_DOOR_POS1 || ent->moverState == MOVER_STATE_ROTATE_POS1) {
        blocked = IsBinaryMoverBlocked(ent, other, activator);
    }

    if (blocked != 0) {
        MatchTeamReverseAngleOnSlaves(ent, MOVER_STATE_ROTATE_MOVING_TO_POS2,
                                      coduo_int32_from_bits((uint32_t)level.time + BINARY_MOVER_START_DELAY_MS));
        if (playSounds != 0) {
            game_compat_g_binary_mover_start_opening_sound(ent, quiet);
        }
        game_compat_g_binary_mover_start_open_loop(ent, playSounds);
        return;
    }

    ent->triggerActivator = activator;
    if (ent->targetLocationNext != NULL && ent->targetLocationNext->itemWait == BINARY_MOVER_LINK_WAIT_SENTINEL &&
        ent->targetLocationNext->itemCount == BINARY_MOVER_LINK_ITEM_COUNT) {
        ent->targetLocationNext->itemCount = 0;
        return;
    }

    if (ent->moverState == MOVER_STATE_DOOR_POS1) {
        MatchTeam(ent, MOVER_STATE_DOOR_MOVING_TO_POS2, coduo_int32_from_bits((uint32_t)level.time + BINARY_MOVER_START_DELAY_MS));
        if (playSounds != 0) {
            G_PlaySoundAlias(ent, ent->doorSoundOpening);
        }
        game_compat_g_binary_mover_start_open_loop(ent, playSounds);
    } else if (ent->moverState == MOVER_STATE_ROTATE_POS1) {
        MatchTeam(ent, MOVER_STATE_ROTATE_MOVING_TO_POS2, coduo_int32_from_bits((uint32_t)level.time + BINARY_MOVER_START_DELAY_MS));
        if (playSounds != 0) {
            game_compat_g_binary_mover_start_opening_sound(ent, quiet);
        }
        game_compat_g_binary_mover_start_open_loop(ent, playSounds);
    } else if (ent->moverState == MOVER_STATE_DOOR_POS2) {
        if ((ent->flags & BINARY_MOVER_TOGGLE_FLAG) == 0) {
            if (ent->itemWait != BINARY_MOVER_NO_AUTORETURN_WAIT) {
                int32_t wait = game_compat_int32_from_long_double_trunc((long double)ent->itemWait);

                ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)wait);
            }
        } else {
            ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + BINARY_MOVER_START_DELAY_MS);
        }
    } else if (ent->moverState == MOVER_STATE_ROTATE_POS2) {
        if ((ent->flags & BINARY_MOVER_TOGGLE_FLAG) == 0) {
            int32_t wait = game_compat_int32_from_long_double_trunc((long double)ent->itemWait);

            ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)wait);
        } else {
            ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + BINARY_MOVER_START_DELAY_MS);
        }
    } else if (ent->moverState == MOVER_STATE_DOOR_MOVING_TO_POS1) {
        Blocked_Door(ent, NULL);
        if (playSounds != 0) {
            G_PlaySoundAlias(ent, ent->doorSoundOpening);
        }
    } else if (ent->moverState == MOVER_STATE_DOOR_MOVING_TO_POS2) {
        Blocked_Door(ent, NULL);
        if (playSounds != 0) {
            G_PlaySoundAlias(ent, ent->doorSoundClosing);
        }
    } else if (ent->moverState == MOVER_STATE_ROTATE_MOVING_TO_POS1) {
        Blocked_DoorRotate(ent, NULL);
        if (playSounds != 0) {
            G_PlaySoundAlias(ent, ent->doorSoundOpening);
        }
    } else if (ent->moverState == MOVER_STATE_ROTATE_MOVING_TO_POS2) {
        Blocked_DoorRotate(ent, NULL);
        if (playSounds != 0) {
            if (quiet != 0) {
                G_PlaySoundAlias(ent, ent->doorSoundClosingQuiet);
            } else {
                G_PlaySoundAlias(ent, ent->doorSoundClosing);
            }
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: func_rotating trajectory predicate; extracted during reconstruction of 0x62e2e. */
/* VERIFIED_DECOMPILER(0x62e2e, 72e2e_InitMover.c, VERIFY-WAVE3-ENTITY-DISPATCH-C08-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against InitMover's scriptClassname compare to DAT_00449e98, loaded as scr_const[28] for func_rotating. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original InitMover (0x62e2e); no standalone original body. */
static int game_compat_g_binary_mover_is_func_rotating(gentity_t *ent)
{
    return ent->scriptClassname == scr_const_func_rotating;
}

/* ------------------------------------------------------------------ */
/*  0x62e2e  InitMover                                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x62e2e, 72e2e_InitMover.c, VERIFY-WAVE3-ENTITY-DISPATCH-C08-2026-06-17): DATAFLOW_VERIFIED - noise spawn/sound alias byte, packed constant light clamps, func_rotating use/reached dispatch, mover state byte, entity state/link setup, position trajectory base, distance duration, default speed, and alt-speed duration checked. */
void InitMover(gentity_t *ent)
{
    const char *noise;
    float light;
    vec3_t color;
    int hasLight;
    int hasColor;
    vec3_t delta;
    float distance;
    int duration;

    if (G_SpawnString("noise", "100", &noise) != 0) {
        ent->s.clientSound = (uint8_t)G_SoundAliasIndex(noise);
    }

    hasLight = G_SpawnFloat("light", "100", &light);
    hasColor = G_SpawnVector("color", "1 1 1", color);
    if (hasLight != 0 || hasColor != 0) {
        uint32_t red = game_compat_g_mover_light_component(color[0], 255.0f);
        uint32_t green = game_compat_g_mover_light_component(color[1], 255.0f);
        uint32_t blue = game_compat_g_mover_light_component(color[2], 255.0f);
        uint32_t intensity = game_compat_g_mover_light_component(light, 0.25f);

        ent->s.constantLight = (intensity << 24) | (blue << 16) | (green << 8) | red;
    }

    if (game_compat_g_binary_mover_is_func_rotating(ent) != 0) {
        ent->use = Use_Func_Rotate;
        ent->moverReached = NULL;
    } else {
        ent->use = Use_BinaryMover;
        ent->moverReached = Reached_BinaryMover;
    }

    ent->moverState = (uint8_t)(MOVER_STATE_DOOR_POS1);
    ent->svFlags = BINARY_MOVER_SVFLAGS;
    ent->s.eType = ET_MOVER;
    game_compat_g_copy_vector3(ent->moverPos1, ent->currentOrigin);
    trap_LinkEntity(ent);

    ent->s.pos.trType = TR_STATIONARY;
    game_compat_g_copy_vector3(ent->moverPos1, ent->s.pos.trBase);

    delta[0] = ent->moverPos2[0] - ent->moverPos1[0];
    delta[1] = ent->moverPos2[1] - ent->moverPos1[1];
    delta[2] = ent->moverPos2[2] - ent->moverPos1[2];
    /* Stock 0x630bd..0x630d6: the delta.delta dot is computed in 80 bits
     * (fmul;faddp chain, no per-term double rounding) and stored to a QWORD once
     * for sqrt -> shim (store_f64), then (float)sqrt. */
#if EMULATE_X87
    distance = (float)CoduoLibm_Sqrt(x87f_store_f64(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(delta[0]), x87f_load_f32(delta[0])), x87f_mul(x87f_load_f32(delta[1]), x87f_load_f32(delta[1]))),
        x87f_mul(x87f_load_f32(delta[2]), x87f_load_f32(delta[2])))));
#else
    distance = (float)CoduoLibm_Sqrt((double)delta[0] * (double)delta[0] + (double)delta[1] * (double)delta[1] +
                                     (double)delta[2] * (double)delta[2]);
#endif

    if ((*game_compat_g_binary_mover_speed(ent)) == 0.0f) {
        (*game_compat_g_binary_mover_speed(ent)) = BINARY_MOVER_DEFAULT_SPEED;
    }

    /* Stock 0x63113..0x63130: distance*1000 then /speed kept 80-bit, fistp
     * DIRECTLY on the quotient (no float spill) -> truncate the 80-bit quotient.
     * The #else forces the 80-bit product/quotient with a long double multiply
     * (1000 exact) so the reference build also fistp's directly. */
#if EMULATE_X87
    duration = x87f_store_i32_trunc(
        x87f_div(x87f_mul(x87f_load_f32(distance), x87f_load_f32(1000.0f)), x87f_load_f32(*game_compat_g_binary_mover_speed(ent))));
#else
    duration = game_compat_int32_from_long_double_trunc((long double)distance * 1000.0f / (*game_compat_g_binary_mover_speed(ent)));
#endif
    if (duration < 1) {
        duration = 1;
    }

    ent->s.pos.trDuration = duration;
    ent->moverDuration = duration;
    ent->moverAltDuration = duration;

    if (game_compat_g_binary_mover_has_alt_speed(ent) != 0) {
        /* Same as the primary duration: distance*1000 / doorAltSpeed kept 80-bit,
         * fistp directly on the quotient. */
#if EMULATE_X87
        duration =
            x87f_store_i32_trunc(x87f_div(x87f_mul(x87f_load_f32(distance), x87f_load_f32(1000.0f)), x87f_load_f32(ent->doorAltSpeed)));
#else
        duration = game_compat_int32_from_long_double_trunc((long double)distance * 1000.0f / ent->doorAltSpeed);
#endif
        if (duration < 1) {
            duration = 1;
        }
        ent->moverAltDuration = duration;
    }
}

/* VERIFIED_DECOMPILER(0x631c0, 731c0_InitMoverRotate.c, VERIFY-WAVE3-ENTITY-DISPATCH-C08-2026-06-17): DATAFLOW_VERIFIED - light/color packing, Use_BinaryMover install, no-reached spawnflag, rotate state byte, sv/eType/link writes, stationary origin base, default speed, and duration stores checked. */
void InitMoverRotate(gentity_t *ent)
{
    float light;
    vec3_t color;
    int hasLight;
    int hasColor;
    int duration;

    hasLight = G_SpawnFloat("light", "100", &light);
    hasColor = G_SpawnVector("color", "1 1 1", color);
    if (hasLight != 0 || hasColor != 0) {
        uint32_t red = game_compat_g_mover_light_component(color[0], 255.0f);
        uint32_t green = game_compat_g_mover_light_component(color[1], 255.0f);
        uint32_t blue = game_compat_g_mover_light_component(color[2], 255.0f);
        uint32_t intensity = game_compat_g_mover_light_component(light, 0.25f);

        ent->s.constantLight = (intensity << 24) | (blue << 16) | (green << 8) | red;
    }

    ent->use = Use_BinaryMover;
    if ((ent->spawnflags & BINARY_MOVER_SPAWNFLAG_NO_REACHED) == 0) {
        ent->moverReached = Reached_BinaryMover;
    }

    ent->moverState = (uint8_t)(MOVER_STATE_ROTATE_POS1);
    ent->svFlags = BINARY_MOVER_SVFLAGS;
    ent->s.eType = ET_MOVER;
    trap_LinkEntity(ent);

    ent->s.pos.trType = TR_STATIONARY;
    ent->s.pos.trBase[0] = ent->currentOrigin[0];
    ent->s.pos.trBase[1] = ent->currentOrigin[1];
    ent->s.pos.trBase[2] = ent->currentOrigin[2];

    if ((*game_compat_g_binary_mover_speed(ent)) == 0.0f) {
        (*game_compat_g_binary_mover_speed(ent)) = BINARY_MOVER_DEFAULT_SPEED;
    }

    duration = game_compat_int32_from_float_trunc(*game_compat_g_binary_mover_speed(ent));
    if (duration < 1) {
        duration = 1;
    }

    ent->s.apos.trDuration = duration;
    ent->moverAltDuration = duration;
    ent->moverDuration = duration;
}

/* ------------------------------------------------------------------ */
/*  0x633f9  Blocked_Door                                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x633f9, 733f9_Blocked_Door.c, VERIFY-WAVE3-ENTITY-DISPATCH-C08-2026-06-17): DATAFLOW_VERIFIED - zero-client blocker temp event/free, crush damage tuple, crusher spawnflag guard, team-chain reverse time, state flip, and relink checked. */
void Blocked_Door(gentity_t *ent, gentity_t *blocker)
{
    if (blocker != NULL) {
        if (blocker->client == NULL) {
            G_TempEntity(blocker->currentOrigin, DOOR_BLOCKED_TEMP_EVENT);
            G_FreeEntity(blocker);
            return;
        }

        if (ent->damage != 0) {
            G_Damage(blocker, ent, ent, NULL, NULL, ent->damage, 0, MOD_CRUSH, HITLOC_NONE);
        }
    }

    if ((ent->spawnflags & DOOR_SPAWNFLAG_CRUSHER) == 0) {
        for (gentity_t *current = ent; current != NULL; current = current->teamChain) {
            int reverseTime = coduo_int32_from_bits((uint32_t)level.time * UINT32_C(2) - (uint32_t)current->s.pos.trTime -
                                                    (uint32_t)current->s.pos.trDuration);

            if (current->moverState == MOVER_STATE_DOOR_MOVING_TO_POS2) {
                SetMoverState(current, MOVER_STATE_DOOR_MOVING_TO_POS1, reverseTime);
            } else {
                SetMoverState(current, MOVER_STATE_DOOR_MOVING_TO_POS2, reverseTime);
            }
            trap_LinkEntity(current);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x63562  Blocked_DoorRotate                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x63562, 73562_Blocked_DoorRotate.c, VERIFY-WAVE3-ENTITY-DISPATCH-C08-2026-06-17): DATAFLOW_VERIFIED - zero-client blocker temp event/free, health<1 fallback crush damage, entity damage tuple, team-chain apos reverse time, rotate state flip, and relink checked. */
void Blocked_DoorRotate(gentity_t *ent, gentity_t *blocker)
{
    if (blocker != NULL) {
        if (blocker->client == NULL) {
            G_TempEntity(blocker->currentOrigin, DOOR_BLOCKED_TEMP_EVENT);
            G_FreeEntity(blocker);
            return;
        }

        if (blocker->health < 1) {
            G_Damage(blocker, ent, ent, NULL, NULL, DOOR_ROTATE_FALLBACK_DAMAGE, 0, MOD_CRUSH, HITLOC_NONE);
        }
        if (ent->damage != 0) {
            G_Damage(blocker, ent, ent, NULL, NULL, ent->damage, 0, MOD_CRUSH, HITLOC_NONE);
        }
    }

    for (gentity_t *current = ent; current != NULL; current = current->teamChain) {
        int reverseTime = coduo_int32_from_bits((uint32_t)level.time * UINT32_C(2) - (uint32_t)current->s.apos.trTime -
                                                (uint32_t)current->s.apos.trDuration);

        if (current->moverState == MOVER_STATE_ROTATE_MOVING_TO_POS2) {
            SetMoverState(current, MOVER_STATE_ROTATE_MOVING_TO_POS1, reverseTime);
        } else {
            SetMoverState(current, MOVER_STATE_ROTATE_MOVING_TO_POS2, reverseTime);
        }
        trap_LinkEntity(current);
    }
}

/* ------------------------------------------------------------------ */
/*  0x6370b  Touch_DoorTrigger                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x6370b, 7370b_Touch_DoorTrigger.c, VERIFY-WAVE3-ENTITY-DISPATCH-C08-2026-06-17): DATAFLOW_VERIFIED - direct entityRef dereference, doorLocked gate, pos1/rotate-pos1/rotate-moving-to-pos1 state test, Use_BinaryMover argument order, and ignored trace mode checked. */
void Touch_DoorTrigger(gentity_t *trigger, gentity_t *other, int traceMode)
{
    gentity_t *door = trigger->entityRef;
    uint8_t state;

    (void)traceMode;

    if (door->doorLocked != 0) {
        return;
    }

    state = door->moverState;
    if (state == MOVER_STATE_DOOR_POS1 || state == MOVER_STATE_ROTATE_POS1 || state == MOVER_STATE_ROTATE_MOVING_TO_POS1) {
        Use_BinaryMover(door, trigger, other);
    }
}

/* ------------------------------------------------------------------ */
/*  0x6378c  Think_SpawnNewDoorTriggerInternal                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x6378c, 7378c_FUN_0007378c.c, VERIFY-WAVE3-ENTITY-DISPATCH-C08-2026-06-17): DATAFLOW_VERIFIED - team takeDamage byte, aggregate abs bounds, narrow-axis expansion, trigger spawn bounds/entityRef/scriptContents/touch stores, link call, MatchTeam state/time, and return checked. */
static gentity_t *Think_SpawnNewDoorTriggerInternal(gentity_t *ent)
{
    gentity_t *trigger;
    vec3_t mins;
    vec3_t maxs;
    int narrowAxis;

    for (gentity_t *current = ent; current != NULL; current = current->teamChain) {
        current->takeDamage = 1;
    }

    mins[0] = ent->absMin[0];
    mins[1] = ent->absMin[1];
    mins[2] = ent->absMin[2];
    maxs[0] = ent->absMax[0];
    maxs[1] = ent->absMax[1];
    maxs[2] = ent->absMax[2];

    for (gentity_t *current = ent->teamChain; current != NULL; current = current->teamChain) {
        AddPointToBounds(current->absMin, mins, maxs);
        AddPointToBounds(current->absMax, mins, maxs);
    }

    narrowAxis = 0;
    for (int axis = 1; axis < 3; axis++) {
        if (maxs[axis] - mins[axis] < maxs[narrowAxis] - mins[narrowAxis]) {
            narrowAxis = axis;
        }
    }

    maxs[narrowAxis] += DOOR_TRIGGER_EXPAND_DISTANCE;
    mins[narrowAxis] -= DOOR_TRIGGER_EXPAND_DISTANCE;

    trigger = G_Spawn();
    trigger->mins[0] = mins[0];
    trigger->mins[1] = mins[1];
    trigger->mins[2] = mins[2];
    trigger->maxs[0] = maxs[0];
    trigger->maxs[1] = maxs[1];
    trigger->maxs[2] = maxs[2];
    trigger->entityRef = ent;
    trigger->scriptContents = CONTENTS_TRIGGER_TOUCH_CLIENT;
    trigger->touch = Touch_DoorTrigger;
    trap_LinkEntity(trigger);

    MatchTeam(ent, ent->moverState, level.time);
    return trigger;
}

/* ------------------------------------------------------------------ */
/*  0x63993  Think_SpawnNewDoorTrigger                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x63993, 73993_Think_SpawnNewDoorTrigger.c, VERIFY-WAVE4-ENTITY-DISPATCH-C09-2026-06-17): DATAFLOW_VERIFIED - direct Think_SpawnNewDoorTriggerInternal wrapper checked against generated FUN_0007378c call and return. */
void Think_SpawnNewDoorTrigger(gentity_t *ent)
{
    Think_SpawnNewDoorTriggerInternal(ent);
}

/* ------------------------------------------------------------------ */
/*  0x639a6  DoorRotateStartOpen                                      */
/* ------------------------------------------------------------------ */

/* The owning Mac traceback table identifies this cross-platform body as
 * DoorRotateStartOpen; its PPC body has the same stores and call graph. */
/* VERIFIED_DECOMPILER(0x639a6, 739a6_FUN_000739a6.c, VERIFY-WAVE4-ENTITY-DISPATCH-C09-2026-06-17): DATAFLOW_VERIFIED - think store, yaw add, SetMoverState state/time, and area portal self/null master gate checked. */
static void DoorRotateStartOpen(gentity_t *ent)
{
    ent->think = ReturnToPos1Rotate;
    ent->currentAngles[1] += ent->doorYawOffset;
    SetMoverState(ent, MOVER_STATE_ROTATE_POS2, level.time);

    if (ent->teamMaster == ent || ent->teamMaster == NULL) {
        trap_AdjustAreaPortalState(ent, qtrue);
    }
}

/* ------------------------------------------------------------------ */
/*  0x63a38  Think_SpawnNewAutoDoorTrigger                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x63a38, 73a38_Think_SpawnNewAutoDoorTrigger.c, VERIFY-WAVE4-ENTITY-DISPATCH-C09-2026-06-17): DATAFLOW_VERIFIED - spawnflags bit 1 gate and auto-door open initializer call checked. */
void Think_SpawnNewAutoDoorTrigger(gentity_t *ent)
{
    if ((ent->spawnflags & AUTO_DOOR_SPAWNFLAG_INIT_OPEN) != 0) {
        DoorRotateStartOpen(ent);
    }
}

/* ------------------------------------------------------------------ */
/*  0x63a5b  Think_MatchTeam                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x63a5b, 73a5b_Think_MatchTeam.c, VERIFY-WAVE4-ENTITY-DISPATCH-C09-2026-06-17): DATAFLOW_VERIFIED - MatchTeam argument order, mover state byte, and level.time use checked. */
void Think_MatchTeam(gentity_t *ent)
{
    MatchTeam(ent, ent->moverState, level.time);
}

/* ------------------------------------------------------------------ */
/*  0x63a9c  finishSpawningKeyedMover                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x63a9c, 73a9c_finishSpawningKeyedMover.c, VERIFY-WAVE4-ENTITY-DISPATCH-C09-2026-06-17): DATAFLOW_VERIFIED - nextthink delay, command-blocked gate, think selection branches, and doorLocked team-chain propagation checked. */
void finishSpawningKeyedMover(gentity_t *ent)
{
    ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + KEYED_MOVER_FINISH_DELAY_MS);

    if ((ent->flags & KEYED_MOVER_COMMAND_BLOCKED_FLAG) == 0) {
        if (ent->takeDamage == 0) {
            if (ent->scriptClassname == scr_const_func_door_rotating) {
                ent->think = Think_SpawnNewAutoDoorTrigger;
            } else if ((ent->spawnflags & KEYED_MOVER_SPAWNFLAG_TRIGGER) == 0) {
                ent->think = Think_MatchTeam;
            } else {
                ent->think = Think_SpawnNewDoorTrigger;
            }
        } else {
            ent->think = Think_MatchTeam;
        }

        for (gentity_t *current = ent; current != NULL; current = current->teamChain) {
            if (current != ent) {
                current->doorLocked = ent->doorLocked;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x63b8c  Door_reverse_sounds                                      */
/* ------------------------------------------------------------------ */

/* NOT_FROM_ORIGINAL_SOURCE: door sound byte swap; extracted during reconstruction of 0x63b8c. */
/* VERIFIED_DECOMPILER(0x63b8c, 73b8c_Door_reverse_sounds.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against each byte temp-swap in Door_reverse_sounds. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original Door_reverse_sounds (0x63b8c); no standalone original body. */
static void game_compat_door_swap_sound_slots(uint8_t *first, uint8_t *second)
{
    uint8_t sound = *first;

    *first = *second;
    *second = sound;
}

/* VERIFIED_DECOMPILER(0x63b8c, 73b8c_Door_reverse_sounds.c, VERIFY-WAVE4-ENTITY-DISPATCH-C09-2026-06-17): DATAFLOW_VERIFIED - all five byte swaps checked against offsets 0x16f/0x170, 0x16e/0x171, 0x173/0x174, 0x176/0x178, and 0x177/0x179. */
void Door_reverse_sounds(gentity_t *ent)
{
    game_compat_door_swap_sound_slots(&ent->doorSoundOpening, &ent->doorSoundClosing);
    game_compat_door_swap_sound_slots(&ent->doorSoundCloseEnd, &ent->doorSoundOpenEnd);
    game_compat_door_swap_sound_slots(&ent->doorSoundOpenLoop, &ent->doorSoundCloseLoop);
    game_compat_door_swap_sound_slots(&ent->doorSoundOpeningQuiet, &ent->doorSoundClosingQuiet);
    game_compat_door_swap_sound_slots(&ent->doorSoundOpenQuietEnd, &ent->doorSoundCloseQuietEnd);
}

/* ------------------------------------------------------------------ */
/*  0x63c75  DoorSetSounds                                            */
/* ------------------------------------------------------------------ */

/* NOT_FROM_ORIGINAL_SOURCE: door sound alias assignment; extracted during reconstruction of 0x63c75. */
/* VERIFIED_DECOMPILER(0x63c75, 73c75_DoorSetSounds.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against G_SoundAliasIndex result stored as one byte into door sound slots. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original DoorSetSounds (0x63c75); no standalone original body. */
static void game_compat_door_set_sound_alias(uint8_t *slot, const char *alias)
{
    *slot = G_SoundAliasIndex(alias);
}

/* VERIFIED_DECOMPILER(0x63c75, 73c75_DoorSetSounds.c, VERIFY-WAVE4-ENTITY-DISPATCH-C10-2026-06-17): DATAFLOW_VERIFIED - two-argument caller ABI with unused mode argument checked at door call sites; body reads ent only and all eleven alias stores were checked against sound byte offsets 0x16e..0x179. */
void DoorSetSounds(gentity_t *ent, int isRotating)
{
    (void)isRotating;

    game_compat_door_set_sound_alias(&ent->doorSoundOpening, "door_opening");
    game_compat_door_set_sound_alias(&ent->doorSoundOpenEnd, "door_open_end");
    game_compat_door_set_sound_alias(&ent->doorSoundClosing, "door_closing");
    game_compat_door_set_sound_alias(&ent->doorSoundCloseEnd, "door_close_end");
    game_compat_door_set_sound_alias(&ent->doorSoundOpenLoop, "door_open_loop");
    game_compat_door_set_sound_alias(&ent->doorSoundCloseLoop, "door_close_loop");
    game_compat_door_set_sound_alias(&ent->doorSoundLocked, "door_locked");
    game_compat_door_set_sound_alias(&ent->doorSoundOpeningQuiet, "door_opening_quiet");
    game_compat_door_set_sound_alias(&ent->doorSoundOpenQuietEnd, "door_open_quiet_end");
    game_compat_door_set_sound_alias(&ent->doorSoundClosingQuiet, "door_closing_quiet");
    game_compat_door_set_sound_alias(&ent->doorSoundCloseQuietEnd, "door_close_quiet_end");
}

/* ------------------------------------------------------------------ */
/*  0x63d8c  G_TryDoor                                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x63d8c, 73d8c_G_TryDoor.c, VERIFY-WAVE4-ENTITY-DISPATCH-C10-2026-06-17): DATAFLOW_VERIFIED - stationary pos/apos and in-use gates, doorLocked sound path, trigger notify arguments, team-master selection, activeState write, sticky flag propagation from original ent, and Use_BinaryMover argument order checked. */
void G_TryDoor(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
    uint32_t moverFlags = ent->flags;
    gentity_t *door = ent;

    (void)other;

    if (ent->s.apos.trType != TR_STATIONARY || ent->s.pos.trType != TR_STATIONARY || ent->activeState != 0) {
        return;
    }

    if (ent->doorLocked != 0) {
        G_PlaySoundAlias(ent, ent->doorSoundLocked);
        return;
    }

    Scr_AddEntity(activator);
    Scr_Notify(ent, scr_const_trigger, 1);

    if (ent->teamMaster != NULL && (uint16_t)ent->teamName != 0 && ent != ent->teamMaster) {
        door = ent->teamMaster;
    }

    door->activeState = 1;
    if ((moverFlags & DOOR_MOVER_STICKY_FLAG) != 0) {
        door->flags |= DOOR_MOVER_STICKY_FLAG;
    }
    Use_BinaryMover(door, activator, activator);
}

/* NOT_FROM_ORIGINAL_SOURCE: door vector copy to entity storage; extracted during reconstruction of 0x63f01. */
/* VERIFIED_DECOMPILER(0x63f01, 73f01_SP_func_door.c, VERIFY-WAVE4-ENTITY-DISPATCH-C10-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against parent moverPos1/moverPos2/currentOrigin three-float stores. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original SP_func_door (0x63f01); no standalone original body. */
static void game_compat_door_copy_vector_to_ent(vec3_t dest, const vec3_t value)
{
    dest[0] = value[0];
    dest[1] = value[1];
    dest[2] = value[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: door vector copy from entity storage; extracted during reconstruction of 0x63f01. */
/* VERIFIED_DECOMPILER(0x63f01, 73f01_SP_func_door.c, VERIFY-WAVE4-ENTITY-DISPATCH-C10-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against parent moverPos2 temporary three-float loads. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original SP_func_door (0x63f01); no standalone original body. */
static void game_compat_door_copy_vector_from_ent(const vec3_t src, vec3_t value)
{
    value[0] = src[0];
    value[1] = src[1];
    value[2] = src[2];
}

/* ------------------------------------------------------------------ */
/*  0x63f01  SP_func_door                                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x63f01, 73f01_SP_func_door.c, VERIFY-WAVE4-ENTITY-DISPATCH-C10-2026-06-17): DATAFLOW_VERIFIED - DoorSetSounds(ent, 0) caller use, Blocked_Door install, speed/wait defaults and scaling, locked-key presence behavior, lip/dmg spawns, brush model before movedir-size math, moverPos1/moverPos2 stores, start-open swap including alt-speed NaN branch and sound reversal, toggle hint flag, InitMover, health/takeDamage gate, and finish-spawn think scheduling checked. */
void SP_func_door(gentity_t *ent)
{
    int locked;
    int health;
    float lip = DOOR_DEFAULT_LIP;
    vec3_t size;
    float moveDistance;
    vec3_t pos1;
    vec3_t pos2;
    vec3_t temp;

    DoorSetSounds(ent, 0);
    ent->moverBlocked = Blocked_Door;

    if ((*game_compat_g_binary_mover_speed(ent)) == 0.0f) {
        (*game_compat_g_binary_mover_speed(ent)) = DOOR_DEFAULT_SPEED;
    }
    if (ent->itemWait == 0.0f) {
        ent->itemWait = DOOR_DEFAULT_WAIT_SECONDS;
    }
    ent->itemWait *= DOOR_WAIT_SCALE;

    if (G_SpawnInt("key", DOOR_LOCKED_DEFAULT, &locked) == 0) {
        ent->doorLocked = 0;
    } else {
        ent->doorLocked = 1;
    }

    G_SpawnFloat("lip", "8", &lip);
    G_SpawnInt("dmg", DOOR_DMG_DEFAULT, &ent->damage);

    pos1[0] = ent->currentOrigin[0];
    pos1[1] = ent->currentOrigin[1];
    pos1[2] = ent->currentOrigin[2];
    game_compat_door_copy_vector_to_ent(ent->moverPos1, pos1);

    trap_SetBrushModel(ent);
    G_SetMovedir(ent->currentAngles, ent->moverDir);

    size[0] = ent->maxs[0] - ent->mins[0];
    size[1] = ent->maxs[1] - ent->mins[1];
    size[2] = ent->maxs[2] - ent->mins[2];
    /* Stock 0x640e9..0x64102: fabsf(moverDir[i]) and size[i] are pre-stored to
     * floats (native), then the dot minus lip is kept 80-bit until the store ->
     * shim ((m0+m1)+m2) - lip. */
#if EMULATE_X87
    moveDistance = x87f_store_f32(x87f_sub(x87f_add(x87f_add(x87f_mul(x87f_load_f32(fabsf(ent->moverDir[0])), x87f_load_f32(size[0])),
                                                             x87f_mul(x87f_load_f32(fabsf(ent->moverDir[1])), x87f_load_f32(size[1]))),
                                                    x87f_mul(x87f_load_f32(fabsf(ent->moverDir[2])), x87f_load_f32(size[2]))),
                                           x87f_load_f32(lip)));
#else
    moveDistance = fabsf(ent->moverDir[0]) * size[0] + fabsf(ent->moverDir[1]) * size[1] + fabsf(ent->moverDir[2]) * size[2] - lip;
#endif

    /* pos2[i] = pos1[i] + moverDir[i]*moveDistance: mul then add, 80-bit, one
     * store per component -> shim. */
#if EMULATE_X87
    for (int i = 0; i < 3; i++) {
        pos2[i] = x87f_store_f32(x87f_add(x87f_load_f32(pos1[i]), x87f_mul(x87f_load_f32(ent->moverDir[i]), x87f_load_f32(moveDistance))));
    }
#else
    pos2[0] = pos1[0] + ent->moverDir[0] * moveDistance;
    pos2[1] = pos1[1] + ent->moverDir[1] * moveDistance;
    pos2[2] = pos1[2] + ent->moverDir[2] * moveDistance;
#endif
    game_compat_door_copy_vector_to_ent(ent->moverPos2, pos2);

    if ((ent->spawnflags & DOOR_SPAWNFLAG_START_OPEN) != 0) {
        game_compat_door_copy_vector_from_ent(ent->moverPos2, temp);
        game_compat_door_copy_vector_to_ent(ent->moverPos2, ent->currentOrigin);
        game_compat_door_copy_vector_to_ent(ent->moverPos1, temp);

        if (ent->doorAltSpeed != 0.0f || isnan(ent->doorAltSpeed)) {
            float speed = (*game_compat_g_binary_mover_speed(ent));

            (*game_compat_g_binary_mover_speed(ent)) = ent->doorAltSpeed;
            ent->doorAltSpeed = speed;
        }
        Door_reverse_sounds(ent);
    }

    if ((ent->spawnflags & DOOR_SPAWNFLAG_TOGGLE) != 0) {
        ent->flags |= DOOR_MOVER_HINT_FLAG;
    }

    InitMover(ent);

    if ((ent->flags & KEYED_MOVER_COMMAND_BLOCKED_FLAG) == 0) {
        G_SpawnInt("health", DOOR_HEALTH_DEFAULT, &health);
        if (health != 0) {
            ent->takeDamage = 1;
        }
    }

    ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + KEYED_MOVER_FINISH_DELAY_MS);
    ent->think = finishSpawningKeyedMover;
}

/* ------------------------------------------------------------------ */
/*  0x642e7  Use_Static                                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x642e7, 742e7_Use_Static.c, VERIFY-WAVE4-ENTITY-DISPATCH-C10-2026-06-17): DATAFLOW_VERIFIED - linkedState gate, link/unlink branch targets, and ignored use-callback parameters checked. */
void Use_Static(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
    (void)other;
    (void)activator;

    if (ent->linkedState == 0) {
        trap_LinkEntity(ent);
    } else {
        trap_UnlinkEntity(ent);
    }
}

/* ------------------------------------------------------------------ */
/*  0x64323  Static_Pain                                              */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x64323, 74323_Static_Pain.c, VERIFY-WAVE4-ENTITY-DISPATCH-C10-2026-06-17): DATAFLOW_VERIFIED - callback signature adaptation, pain-delay timing expression, rand modulo/base delay, level.time comparison, itemWait update, spawnflag bit 4 split, and damagePoint currentOrigin save/copy/restore side effects checked. */
void Static_Pain(gentity_t *ent, gentity_t *attacker, int damage, const float *point, int mod, const float *dir, int hitLocation)
{
    float painDelay;
    float nextPainTime;

    (void)attacker;
    (void)damage;
    (void)point;
    (void)mod;
    (void)dir;
    (void)hitLocation;

    painDelay = ent->concussiveFxEndTime;
    /* 0x6436a/0x64399..0x643a9: stock rounds itemWait+painDelay to a float,
     * then keeps the +rand+500 sum in the 80-bit register straight into the
     * comparison (rand result enters via fild, exact -- no (float) cast). */
    nextPainTime = ent->itemWait + painDelay;

    /* Stock 0x6434f..0x643a9: (float)level.time is materialised first (fild;
     * fstp DWORD -> a rounded float), then the sum nextPainTime + rand%1000 +
     * 500 is kept 80-bit (rand via fild, exact; 500 a DWORD float const) and
     * compared FULL-WIDTH (fucompp). The (float) cast must go through a float
     * VARIABLE -- an in-expression (float)level.time is elided by
     * -fexcess-precision=fast (fild direct, no round) and would deviate. */
    {
        const float levelTimeFloat = (float)level.time;
#if EMULATE_X87
        if (x87f_lt(x87f_add(x87f_add(x87f_load_f32(nextPainTime), x87f_load_i32(coduo_server_randrange(0, STATIC_PAIN_RANDOM_MS))),
                             x87f_load_f32(STATIC_PAIN_BASE_DELAY_MS)),
                    x87f_load_f32(levelTimeFloat))) {
#else
        if (nextPainTime + coduo_server_randrange(0, STATIC_PAIN_RANDOM_MS) + STATIC_PAIN_BASE_DELAY_MS < levelTimeFloat) {
#endif
            ent->itemWait = (float)level.time;

            if ((ent->spawnflags & STATIC_PAIN_DAMAGE_POINT_FLAG) != 0) {
                vec3_t savedOrigin;

                /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                game_compat_g_copy_vector3(ent->currentOrigin, savedOrigin);
                game_compat_g_copy_vector3(ent->damagePoint, ent->currentOrigin);
                game_compat_g_copy_vector3(savedOrigin, ent->currentOrigin);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x644ce  SP_func_leaky                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x644ce, 744ce_SP_func_leaky.c, VERIFY-WAVE4-ENTITY-DISPATCH-C10-2026-06-17): DATAFLOW_VERIFIED - brush model setup, link call, stationary pos.trType, and currentOrigin to pos.trBase stores checked. */
void SP_func_leaky(gentity_t *ent)
{
    trap_SetBrushModel(ent);
    trap_LinkEntity(ent);
    ent->s.pos.trType = TR_STATIONARY;
    game_compat_g_copy_vector3(ent->currentOrigin, ent->s.pos.trBase);
}

/* ------------------------------------------------------------------ */
/*  0x64533  SP_func_static                                           */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x64533, 74533_SP_func_static.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - brush model, InitMover, pos.trBase copy, Use_Static install, start-unlinked branch, health parse/value gate, pain setup, delay scaling/default, health/itemCount stores, and void return checked. */
void SP_func_static(gentity_t *ent)
{
    int health;

    trap_SetBrushModel(ent);
    InitMover(ent);
    game_compat_g_copy_vector3(ent->currentOrigin, ent->s.pos.trBase);
    ent->use = Use_Static;

    if ((ent->spawnflags & STATIC_SPAWNFLAG_START_UNLINKED) != 0) {
        trap_UnlinkEntity(ent);
    }

    if ((ent->flags & KEYED_MOVER_COMMAND_BLOCKED_FLAG) == 0) {
        G_SpawnInt("health", STATIC_HEALTH_DEFAULT, &health);
        if (health != 0) {
            ent->takeDamage = 1;
        }
    }

    if ((ent->spawnflags & (STATIC_SPAWNFLAG_PAIN_TIMER | STATIC_PAIN_DAMAGE_POINT_FLAG)) != 0) {
        float *painDelay = &ent->concussiveFxEndTime;

        ent->pain = Static_Pain;
        if (*painDelay == 0.0f) {
            *painDelay = STATIC_DEFAULT_PAIN_DELAY_MS;
        } else {
            *painDelay *= 1000.0f;
        }

        ent->takeDamage = 1;
        ent->health = STATIC_DEFAULT_HEALTH;
        if (ent->itemCount == 0) {
            ent->itemCount = STATIC_DEFAULT_ITEM_COUNT;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x646a0  Use_Func_Rotate                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x646a0, 746a0_Use_Func_Rotate.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - axis spawnflag priority, apos.trDelta speed stores, unlink flag clear, link call, and ignored callback args checked. */
void Use_Func_Rotate(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
    (void)other;
    (void)activator;

    if ((ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_Z_AXIS) != 0) {
        ent->s.apos.trDelta[2] = (*game_compat_g_binary_mover_speed(ent));
    } else if ((ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_X_AXIS) != 0) {
        ent->s.apos.trDelta[0] = (*game_compat_g_binary_mover_speed(ent));
    } else {
        ent->s.apos.trDelta[1] = (*game_compat_g_binary_mover_speed(ent));
    }

    if ((ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_START_UNLINKED) != 0) {
        ent->flags &= ~FUNC_ROTATING_UNLINKED_FLAG;
    }
    trap_LinkEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x64739  SP_func_rotating                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x64739, 74739_SP_func_rotating.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - speed default, TR_LINEAR setup, start-on axis deltas, damage default, brush/model mover init, pos.trBase copy, link/unlink flag branch, and void return checked. */
void SP_func_rotating(gentity_t *ent)
{
    if ((*game_compat_g_binary_mover_speed(ent)) == 0.0f) {
        (*game_compat_g_binary_mover_speed(ent)) = FUNC_ROTATING_DEFAULT_SPEED;
    }

    ent->s.apos.trType = TR_LINEAR;

    if ((ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_START_ON) != 0) {
        if ((ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_Z_AXIS) != 0) {
            ent->s.apos.trDelta[2] = (*game_compat_g_binary_mover_speed(ent));
        } else if ((ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_X_AXIS) != 0) {
            ent->s.apos.trDelta[0] = (*game_compat_g_binary_mover_speed(ent));
        } else {
            ent->s.apos.trDelta[1] = (*game_compat_g_binary_mover_speed(ent));
        }
    }

    if (ent->damage == 0) {
        ent->damage = FUNC_ROTATING_DEFAULT_DAMAGE;
    }

    trap_SetBrushModel(ent);
    InitMover(ent);
    game_compat_g_copy_vector3(ent->currentOrigin, ent->s.pos.trBase);

    if ((ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_START_UNLINKED) == 0) {
        trap_LinkEntity(ent);
    } else {
        ent->flags |= FUNC_ROTATING_UNLINKED_FLAG;
        trap_UnlinkEntity(ent);
    }
}

/* ------------------------------------------------------------------ */
/*  0x6487e  SP_func_bobbing                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x6487e, 7487e_SP_func_bobbing.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - speed/height/dmg/phase spawns, brush/model mover init, pos.trBase copy, sine duration/time, axis delta priority, and void return checked. */
void SP_func_bobbing(gentity_t *ent)
{
    float height;
    float phase;

    G_SpawnFloat("speed", FUNC_BOBBING_DEFAULT_SPEED, game_compat_g_binary_mover_speed(ent));
    G_SpawnFloat("height", FUNC_BOBBING_DEFAULT_HEIGHT, &height);
    G_SpawnInt("dmg", FUNC_BOBBING_DEFAULT_DAMAGE, &ent->damage);
    G_SpawnFloat("phase", FUNC_BOBBING_DEFAULT_PHASE, &phase);

    trap_SetBrushModel(ent);
    InitMover(ent);
    game_compat_g_copy_vector3(ent->currentOrigin, ent->s.pos.trBase);

    /* Stock 0x6496b/0x6498b: speed*1000 (80-bit, 1000 a DWORD float) and
     * trDuration(fild)*phase (80-bit) each feed fistp DIRECTLY (no float spill)
     * -> truncate the 80-bit product; the #else long double casts force the
     * 80-bit product so the ref build fistp's directly too. */
#if EMULATE_X87
    ent->s.pos.trDuration = x87f_store_i32_trunc(x87f_mul(x87f_load_f32(*game_compat_g_binary_mover_speed(ent)), x87f_load_f32(1000.0f)));
    ent->s.pos.trTime = x87f_store_i32_trunc(x87f_mul(x87f_load_i32(ent->s.pos.trDuration), x87f_load_f32(phase)));
#else
    ent->s.pos.trDuration =
        game_compat_int32_from_long_double_trunc((long double)(*game_compat_g_binary_mover_speed(ent)) * (long double)1000.0f);
    ent->s.pos.trTime = game_compat_int32_from_long_double_trunc((long double)ent->s.pos.trDuration * (long double)phase);
#endif
    ent->s.pos.trType = TR_SINE;

    if ((ent->spawnflags & FUNC_BOBBING_SPAWNFLAG_X_AXIS) != 0) {
        ent->s.pos.trDelta[0] = height;
    } else if ((ent->spawnflags & FUNC_BOBBING_SPAWNFLAG_Y_AXIS) != 0) {
        ent->s.pos.trDelta[1] = height;
    } else {
        ent->s.pos.trDelta[2] = height;
    }
}

/* ------------------------------------------------------------------ */
/*  0x649e9  SP_func_pendulum                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x649e9, 749e9_SP_func_pendulum.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - speed/dmg/phase spawns, brush model, abs/min length, gravity frequency constants, duration stores, mover init, base copies, sine angular trajectory, and void return checked. */
void SP_func_pendulum(gentity_t *ent)
{
    float speed;
    float phase;
    float length;
    float gravityFrequency;
    long double gravityRatio;
    float frequency;
    int duration;

    G_SpawnFloat("speed", FUNC_PENDULUM_DEFAULT_SPEED, &speed);
    G_SpawnInt("dmg", FUNC_PENDULUM_DEFAULT_DAMAGE, &ent->damage);
    G_SpawnFloat("phase", FUNC_PENDULUM_DEFAULT_PHASE, &phase);

    trap_SetBrushModel(ent);

    length = fabsf(ent->mins[2]);
    if (length < FUNC_PENDULUM_MIN_LENGTH) {
        length = FUNC_PENDULUM_MIN_LENGTH;
    }

    /* Stock 0x64a95..0x64adc: value / (length*3.0) computed 80-bit and stored to
     * a QWORD for CoduoLibm_Sqrt (0x64aa5) -> gravityFrequency float; frequency =
     * gravityFrequency(float) * FREQ_SCALE(QWORD double) kept 80-bit -> float;
     * duration = 1000.0/frequency, fistp DIRECTLY on the 80-bit quotient. */
#if EMULATE_X87
    gravityFrequency = (float)CoduoLibm_Sqrt(x87f_store_f64(
        x87f_div(x87f_load_f32(g_gravity.value), x87f_mul(x87f_load_f32(length), x87f_load_f32(FUNC_PENDULUM_GRAVITY_SCALE)))));
    frequency = x87f_store_f32(x87f_mul(x87f_load_f32(gravityFrequency), x87f_load_f64(FUNC_PENDULUM_FREQ_SCALE)));
    duration = x87f_store_i32_trunc(x87f_div(x87f_load_f32(1000.0f), x87f_load_f32(frequency)));
    (void)gravityRatio;
#elif defined(__i386__) || defined(__x86_64__)
    gravityRatio = (long double)g_gravity.value / ((long double)length * (long double)FUNC_PENDULUM_GRAVITY_SCALE);
    gravityFrequency = (float)CoduoLibm_Sqrt((double)gravityRatio);
    frequency = (float)((long double)gravityFrequency * (long double)FUNC_PENDULUM_FREQ_SCALE);
    {
        static const float numerator = 1000.0f;
        coduo_x87_truncation_control_t durationControl;
        duration = CODUO_X87_TRUNCATE_I32_FIRST(&durationControl, (long double)numerator / (long double)frequency);
    }
#else
    gravityRatio = (long double)g_gravity.value / ((long double)length * (long double)FUNC_PENDULUM_GRAVITY_SCALE);
    gravityFrequency = (float)CoduoLibm_Sqrt((double)gravityRatio);
    frequency = (float)((long double)gravityFrequency * (long double)FUNC_PENDULUM_FREQ_SCALE);
    duration = game_compat_int32_from_long_double_trunc((long double)1000.0f / (long double)frequency);
#endif

    ent->s.pos.trDuration = duration;
    InitMover(ent);
    game_compat_g_copy_vector3(ent->currentOrigin, ent->s.pos.trBase);
    game_compat_g_copy_vector3(ent->currentAngles, ent->s.apos.trBase);

    /* 0x64b65..0x64b7a: duration enters through bare fild and the product
     * feeds truncating fistp directly. */
#if EMULATE_X87
    ent->s.apos.trDuration = x87f_store_i32_trunc(x87f_div(x87f_load_f32(1000.0f), x87f_load_f32(frequency)));
    ent->s.apos.trTime = x87f_store_i32_trunc(x87f_mul(x87f_load_i32(ent->s.apos.trDuration), x87f_load_f32(phase)));
#elif defined(__i386__) || defined(__x86_64__)
    {
        static const float numerator = 1000.0f;
        coduo_x87_truncation_control_t durationControl;
        ent->s.apos.trDuration = CODUO_X87_TRUNCATE_I32_FIRST(&durationControl, (long double)numerator / (long double)frequency);
        ent->s.apos.trTime = CODUO_X87_TRUNCATE_I32_NEXT(&durationControl, (long double)ent->s.apos.trDuration * (long double)phase);
    }
#else
    ent->s.apos.trDuration = game_compat_int32_from_long_double_trunc((long double)1000.0f / (long double)frequency);
    ent->s.apos.trTime = game_compat_int32_from_long_double_trunc((long double)ent->s.apos.trDuration * (long double)phase);
#endif
    ent->s.apos.trType = TR_SINE;
    ent->s.apos.trDelta[2] = speed;
}

/* ------------------------------------------------------------------ */
/*  0x64b99  SP_func_door_rotating                                    */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x64b99, 74b99_SP_func_door_rotating.c, VERIFY-WORKER-MOVER-STATIC-2026-06-17): DATAFLOW_VERIFIED - start-open/toggle spawnflags, no-reached clear, sounds, speed/yaw defaults, reverse yaw, toggle flag, locked parse, axis vector priority/error fallback, wait scaling, brush/model mover init, health gate, finish think, blocked callback, and void return checked. */
void SP_func_door_rotating(gentity_t *ent)
{
    int locked;
    int health;
    float axisLength;

    if ((ent->spawnflags & FUNC_DOOR_ROTATING_SPAWNFLAG_START_OPEN) != 0) {
        ent->spawnflags |= FUNC_DOOR_ROTATING_SPAWNFLAG_TOGGLE;
    }
    ent->spawnflags &= ~FUNC_DOOR_ROTATING_SPAWNFLAG_NO_REACHED;

    DoorSetSounds(ent, 1);

    if ((*game_compat_g_binary_mover_speed(ent)) == 0.0f) {
        (*game_compat_g_binary_mover_speed(ent)) = FUNC_DOOR_ROTATING_DEFAULT_SPEED;
    }
    if (ent->doorYawOffset == 0.0f) {
        ent->doorYawOffset = FUNC_DOOR_ROTATING_DEFAULT_YAW;
    }
    if ((ent->spawnflags & FUNC_DOOR_ROTATING_SPAWNFLAG_REVERSE) != 0) {
        ent->doorYawOffset = -ent->doorYawOffset;
    }
    if ((ent->spawnflags & FUNC_DOOR_ROTATING_SPAWNFLAG_TOGGLE) != 0) {
        ent->flags |= DOOR_MOVER_HINT_FLAG;
    }

    if (G_SpawnInt("key", FUNC_DOOR_ROTATING_LOCKED_DEFAULT, &locked) == 0) {
        ent->doorLocked = 0;
    } else {
        ent->doorLocked = 1;
    }

    ent->damageDir[0] = 0.0f;
    ent->damageDir[1] = 0.0f;
    ent->damageDir[2] = 0.0f;
    if ((ent->spawnflags & FUNC_DOOR_ROTATING_SPAWNFLAG_AXIS_MASK) == FUNC_DOOR_ROTATING_SPAWNFLAG_AXIS_MASK) {
        ent->damageDir[1] = 1.0f;
    } else if ((ent->spawnflags & FUNC_DOOR_ROTATING_SPAWNFLAG_Z_AXIS) == 0) {
        if ((ent->spawnflags & FUNC_DOOR_ROTATING_SPAWNFLAG_X_AXIS) == 0) {
            ent->damageDir[1] = 1.0f;
        } else {
            ent->damageDir[0] = 1.0f;
        }
    } else {
        ent->damageDir[2] = 1.0f;
    }

    /* 0x64d6e..0x64daa: stock rounds the 80-bit sum to DOUBLE at the
     * CoduoLibm_Sqrt() call boundary, then the result to float -- sqrtf() would
     * round the argument to float instead. */
#if EMULATE_X87
    axisLength = (float)CoduoLibm_Sqrt(
        x87f_store_f64(x87f_add(x87f_add(x87f_mul(x87f_load_f32(ent->damageDir[0]), x87f_load_f32(ent->damageDir[0])),
                                         x87f_mul(x87f_load_f32(ent->damageDir[1]), x87f_load_f32(ent->damageDir[1]))),
                                x87f_mul(x87f_load_f32(ent->damageDir[2]), x87f_load_f32(ent->damageDir[2])))));
#else
    axisLength = (float)CoduoLibm_Sqrt((double)ent->damageDir[0] * (double)ent->damageDir[0] +
                                       (double)ent->damageDir[1] * (double)ent->damageDir[1] +
                                       (double)ent->damageDir[2] * (double)ent->damageDir[2]);
#endif
    if (axisLength > 1.0f) {
        G_Error(FUNC_DOOR_ROTATING_AXIS_ERROR);
        ent->damageDir[0] = 0.0f;
        ent->damageDir[1] = 1.0f;
        ent->damageDir[2] = 0.0f;
    }

    if (ent->itemWait == 0.0f) {
        ent->itemWait = FUNC_DOOR_ROTATING_DEFAULT_WAIT;
    }
    ent->itemWait *= FUNC_DOOR_ROTATING_WAIT_SCALE;

    trap_SetBrushModel(ent);
    InitMoverRotate(ent);

    if ((ent->flags & KEYED_MOVER_COMMAND_BLOCKED_FLAG) == 0) {
        G_SpawnInt("health", FUNC_DOOR_ROTATING_HEALTH_DEFAULT, &health);
        if (health != 0) {
            ent->takeDamage = 1;
        }
    }

    ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + KEYED_MOVER_FINISH_DELAY_MS);
    ent->think = finishSpawningKeyedMover;
    ent->moverBlocked = Blocked_DoorRotate;
}
