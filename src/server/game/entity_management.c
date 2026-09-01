/*
 * Source reconstruction for entity management functions.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string.h>
#include "recovered_game.h"
#include "g_public.h"
#include "game_globals.h"
#include "g_syscalls.h"
#include "game_functions.h"
#include "level_locals.h"
#include "scr_vm.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */
#include "compat/coduo_native_x87.h"

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

/* World and none occupy the top entity numbers; spawned entities stop before world. */
#define MAX_SPAWNED_ENTITIES ENTITYNUM_WORLD
#define PLAYER_CLONE_TOGGLE_S_FLAG 0x00000008u
#define CLIENT_ORIGIN_Z_SNAP_OFFSET 1.0f
#define CLIENT_TELEPORT_BIT 0x00000008u
#define TELEPORT_EXIT_EVENT 206
#define TELEPORT_ENTER_EVENT 207
#define TELEPORT_LAUNCH_SPEED 400.0f
#define TELEPORT_PM_TIME 160
#define TELEPORT_NO_LAUNCH_FLAGS EF_FORCED_STANCE_MASK
#define TELEPORT_KNOCKBACK_FLAG 0x00000200u
#define SOUND_BLEND_PITCH_SCALE 100.0f
#ifndef M_PI
#define M_PI 3.14159265358979323846 /* original double64 0x400921fb54442d18 */
#endif
#define SOUND_BLEND_PITCH_MAX 0.99f
#define TURRET_TAG_FLASH "tag_flash"
#define TURRET_TAG_PLAYER "tag_player"
#define TURRET_TAG_WEAPON "tag_weapon"
#define TURRET_TAG_AIM "tag_aim"
#define TURRET_TAG_AIM_ANIMATED "tag_aim_animated"
#define TURRET_TAG_BUTT "tag_butt"
#define TURRET_BULLET_MISS_EVENT_PARM 128
#define TURRET_STATE_REST_PITCH_RECOVERING_FLAG 0x00000100u
#define TURRET_STATE_REST_PITCH_CLAMP_FLAG 0x00000200u
#define TURRET_STATE_REST_PITCH_CLAMP_UP_FLAG 0x00000400u
#define TURRET_STATE_CLIENT_REFRESH_FLAG 0x00000800u
#define TURRET_CLIENT_STATE_TOGGLE_FLAG 0x00000008u
#define TURRET_CLIENT_FIRING_FLAG 0x00000200u
#define TURRET_CLIENT_CLEAR_PS_FLAGS EF_FORCED_STANCE_MASK
#define TURRET_CLIENT_THINK_MSEC 50
#define TURRET_THINK_MSEC 50
#define TURRET_DEFAULT_TURN_RATE 200.0f
#define TURRET_FAST_REST_PITCH_RATE 360.0f
#define TURRET_OVERHEAT_THRESHOLD 1.0f
#define TURRET_OVERHEAT_CLEAR_THRESHOLD 0.5f
#define TURRET_INIT_PITCH_STEPS 30
#define TURRET_INIT_PITCH_RANGE -90.0f
#define TURRET_INIT_TRACE_CONTENTS 0x00000011
#define TARGET_LOCATION_CONFIGSTRING_BASE 53
#define TARGET_LOCATION_LINK_DELAY 200
#define TURRET_USE_MODE_DEFAULT 0
#define TURRET_USE_MODE_ALT 1
#define TURRET_USE_MODE_GUNNER 2
#define TURRET_USE_PS_FLAG_ALT 0x00004000u
#define TURRET_USE_PS_FLAG_GUNNER 0x00002000u
#define TURRET_USE_PS_FLAG_MASK \
    (TURRET_USE_PS_FLAG_ALT | TURRET_USE_PS_FLAG_GUNNER)
#define TURRET_STOP_USE_SELECTOR_ALT_BUTTON_FLAG 0x00000002u
#define TURRET_STOP_USE_SELECTOR_GUNNER_BUTTON_FLAG 0x00000001u
#define TURRET_STOP_USE_EVENT_DISABLED -1
#define TURRET_STOP_USE_EVENT_ALT 1
#define TURRET_STOP_USE_EVENT_GUNNER 2
#define TURRET_POSITION_BLEND_MSEC 1000.0f
#define TURRET_TRACE_CONTENTS 0x02000001
#define TURRET_MAX_COUNT MAX_TURRETS
#define TURRET_DEFAULT_HEALTH 100
#define TURRET_STATE_INITIAL_FLAGS 3u
#define TURRET_CLIPMASK 1
#define TURRET_SCRIPT_CONTENTS 0x00200004u
#define TURRET_SV_FLAGS 0x00000080u
#define TURRET_ENTITY_LINK_USE_FLAG 0x00002000u
#define TURRET_BOUND_XY 32.0f
#define TURRET_BOUND_Z 56.0f
#define MISC_SPAWNER_THINK_MSEC 100

GAME_I386_LAYOUT_ASSERT(bg_static_animation_table_anim_tree_offset,
                      offsetof(bg_static_animation_table_t, animTreeHandle) ==
                      0x0a7ac8u);

int trap_XAnimGetNumChildren(uint32_t anim);
uint32_t *trap_XAnimGetChildAt(uint32_t *out, uint32_t anim, int childIndex);
void trap_XAnimClearTreeGoalWeightsStrict(XAnimTree *tree, uint32_t anim,
                                          float blendTime);
void trap_XAnimSetGoalWeight(XAnimTree *tree, uint32_t anim, float weight,
                             float goalTime, float rate,
                             uint16_t notifyName, qboolean restart);
void trap_XAnimCalcAbsDelta(XAnimTree *tree, uint32_t anim, float *rot,
                            float *trans);

/* ------------------------------------------------------------------ */
/*  0x7a17a  G_SetOrigin                                              */
/* ------------------------------------------------------------------ */

/*
 * Set entity origin and reset position trajectory.
 *
 * Updates both the trajectory base (pos.trBase) and current position
 * (currentOrigin). Zeros out the trajectory type, time, and delta
 * to make the entity stationary at the new position.
 */
/* VERIFIED_DECOMPILER(0x7a17a, 8a17a_G_SetOrigin.c, VERIFY-ENTITYMGMT-ORIGIN-VIS-2026-06-17): DATAFLOW_VERIFIED - trajectory base, type/time/duration clears, delta clears, currentOrigin mirror, and void return checked against current decompiler output. */
void G_SetOrigin(gentity_t *ent, const float *origin)
{
    /* Set trajectory base */
    ent->s.pos.trBase[0] = origin[0];
    ent->s.pos.trBase[1] = origin[1];
    ent->s.pos.trBase[2] = origin[2];
    
    /* Reset trajectory to stationary */
    ent->s.pos.trType = TR_STATIONARY;
    ent->s.pos.trTime = 0;
    ent->s.pos.trDuration = 0;
    ent->s.pos.trDelta[0] = 0.0f;
    ent->s.pos.trDelta[1] = 0.0f;
    ent->s.pos.trDelta[2] = 0.0f;
    
    /* Update current position */
    ent->currentOrigin[0] = origin[0];
    ent->currentOrigin[1] = origin[1];
    ent->currentOrigin[2] = origin[2];
}

/* ------------------------------------------------------------------ */
/*  0x7a21b  G_SetAngle                                               */
/* ------------------------------------------------------------------ */

/*
 * Set entity angles and reset angle trajectory.
 *
 * Updates both the angle trajectory base (apos.trBase) and current
 * angles (currentAngles). Zeros out the trajectory type, time, and
 * delta to make the entity stationary at the new angles.
 */
/* VERIFIED_DECOMPILER(0x7a21b, 8a21b_G_SetAngle.c, VERIFY-ENTITYMGMT-ORIGIN-VIS-2026-06-17): DATAFLOW_VERIFIED - angle trajectory base, type/time/duration clears, delta clears, currentAngles mirror, and void return checked against current decompiler output. */
void G_SetAngle(gentity_t *ent, const float *angles)
{
    /* Set angle trajectory base */
    ent->s.apos.trBase[0] = angles[0];
    ent->s.apos.trBase[1] = angles[1];
    ent->s.apos.trBase[2] = angles[2];
    
    /* Reset angle trajectory to stationary */
    ent->s.apos.trType = TR_STATIONARY;
    ent->s.apos.trTime = 0;
    ent->s.apos.trDuration = 0;
    ent->s.apos.trDelta[0] = 0.0f;
    ent->s.apos.trDelta[1] = 0.0f;
    ent->s.apos.trDelta[2] = 0.0f;
    
    /* Update current angles */
    ent->currentAngles[0] = angles[0];
    ent->currentAngles[1] = angles[1];
    ent->currentAngles[2] = angles[2];
}

/* ------------------------------------------------------------------ */
/*  0x7975b  G_Spawn                                                  */
/* ------------------------------------------------------------------ */

/*
 * Allocate a new entity from the free list or entity array.
 *
 * First tries to reuse an entity from the free list. If the free list
 * is empty, allocates a new entity by extending the entity array.
 * Fails with G_Error if MAX_ENTITIES is reached.
 *
 * Returns pointer to the newly allocated and initialized entity.
 */
void G_InitGentity(gentity_t *ent);

/* VERIFIED_DECOMPILER(0x7975b, 8975b_G_Spawn.c, VERIFY-P1-SPAWN-2026-06-17): DATAFLOW_VERIFIED - allocation path, free-list path, limit dump, trap_LocateGameData arguments, and G_InitGentity call checked against the current decompiler output. */
gentity_t *G_Spawn(void)
{
    gentity_t *ent;
    
    /* Try to allocate from free list */
    if (level.freeListHead == NULL) {
        /* Free list is empty, extend entity array */
        if (level.num_entities == MAX_SPAWNED_ENTITIES) {
            /* Entity limit reached - dump all entities for debugging */
            int i;
            for (i = 0; i < level.num_entities; i++) {
                gentity_t *e = &g_entities[i];
                const char *classname;
                
                if (e->scriptClassname == 0) {
                    classname = "";
                } else {
                    classname = SL_ConvertToString(e->scriptClassname);
                }
                
                G_Printf("%4i: '%s', origin: %f %f %f\n",
                        i, classname,
                        e->currentOrigin[0],
                        e->currentOrigin[1],
                        e->currentOrigin[2]);
            }
            G_Error("G_Spawn: no free entities");
        }
        
        /* Allocate next entity from array */
        ent = &level.gentities[level.num_entities];
        level.num_entities++;
        
        /* Notify engine of new entity count */
        trap_LocateGameData(level.gentities, level.num_entities,
                            sizeof(level.gentities[0]), &level.clients[0].ps,
                            sizeof(level.clients[0]));
    } else {
        /* Take entity from free list */
        ent = level.freeListHead;

        if (ent->nextFree == NULL) {
            level.freeListTail = NULL;
        }

        /* Update free list head */
        level.freeListHead = ent->nextFree;

        /* Clear nextFree pointer */
        ent->nextFree = NULL;
    }
    
    /* Initialize the entity */
    G_InitGentity(ent);

    return ent;
}

/* ------------------------------------------------------------------ */
/*  0x5a2d4  SP_info_camp                                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5a2d4, 6a2d4_SP_info_camp.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - G_SetOrigin argument order, currentOrigin offset 0x13c, void return, and no other side effects checked against current decompiler output. */
void SP_info_camp(gentity_t *ent)
{
    G_SetOrigin(ent, ent->currentOrigin);
}

/* ------------------------------------------------------------------ */
/*  0x5a303  SP_info_null                                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5a303, 6a303_SP_info_null.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - direct G_FreeEntity call, argument forwarding, void return, and no other side effects checked against current decompiler output. */
void SP_info_null(gentity_t *ent)
{
    G_FreeEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x5a326  SP_info_notnull                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5a326, 6a326_SP_info_notnull.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - G_SetOrigin argument order, currentOrigin offset 0x13c, void return, and no other side effects checked against current decompiler output. */
void SP_info_notnull(gentity_t *ent)
{
    G_SetOrigin(ent, ent->currentOrigin);
}

/* ------------------------------------------------------------------ */
/*  0x5a355  SP_light                                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5a355, 6a355_SP_light.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - direct G_FreeEntity call, argument forwarding, void return, and no other side effects checked against current decompiler output. */
void SP_light(gentity_t *ent)
{
    G_FreeEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x5a5e2  SP_misc_teleporter_dest                                  */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5a5e2, 6a5e2_SP_misc_teleporter_dest.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - empty body, ignored parameter, void return, and no side effects checked against current decompiler output. */
void SP_misc_teleporter_dest(gentity_t *ent)
{
    (void)ent;
}

/* ------------------------------------------------------------------ */
/*  0x5a5e7  SP_sound_blend                                           */
/* ------------------------------------------------------------------ */

/* NOT_FROM_ORIGINAL_SOURCE: sound-blend pitch payload accessor; extracted during reconstruction of 0x5a5e7. */
/* VERIFIED_DECOMPILER(0x5a5e7, 6a5e7_SP_sound_blend.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - extracted accessor preserves the decompiler store to entity offset 0xd8, used by sound-blend pitch payload writes. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original SP_sound_blend (0x5a5e7); no standalone original body. */
static void game_compat_g_set_sound_blend_pitch_payload(gentity_t *ent, float encodedPitch)
{
    ent->s.clientInfoLeanFraction = encodedPitch;
}

/* VERIFIED_DECOMPILER(0x5a5e7, 6a5e7_SP_sound_blend.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - scriptContents clear, ET_SOUND_BLEND, stationary trajectories, alias/pitch payload clears, ENTITYNUM_NONE store, svFlags OR 8, and void return checked against current decompiler output. */
void SP_sound_blend(gentity_t *ent)
{
    ent->scriptContents = 0;
    ent->s.eType = ET_SOUND_BLEND;
    ent->s.pos.trType = TR_STATIONARY;
    ent->s.apos.trType = TR_STATIONARY;
    ent->s.tempEffectId = 0;
    ent->s.hintStringIndex = 0;
    game_compat_g_set_sound_blend_pitch_payload(ent, 0.0f);
    ent->s.vehicleEntityNum = ENTITYNUM_NONE;
    ent->svFlags |= SVF_LOOPED_FX;
}

/* ------------------------------------------------------------------ */
/*  0x5a65d  G_SpawnSoundBlend                                        */
/* ------------------------------------------------------------------ */


/* VERIFIED_DECOMPILER(0x5a65d, 6a65d_G_SpawnSoundBlend.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - G_Spawn result, scriptClassname Scr_SetString at offset 0x184, SP_sound_blend call, and return value checked against current decompiler output. */
gentity_t *G_SpawnSoundBlend(void)
{
    gentity_t *ent = G_Spawn();

    Scr_SetString(&ent->scriptClassname,
                  scr_const_sound_blend);
    SP_sound_blend(ent);
    return ent;
}

/* ------------------------------------------------------------------ */
/*  0x5a6a9  G_SetSoundBlend                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5a6a9, 6a6a9_G_SetSoundBlend.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - both-aliases-zero unlink branch, byte-narrowed alias stores, repeat-delay store at 0x6c, link call, and void return checked against current decompiler output. */
void G_SetSoundBlend(gentity_t *ent, int soundAlias0, int soundAlias1,
                     float repeatDelay)
{
    if (soundAlias0 == 0 && soundAlias1 == 0) {
        trap_UnlinkEntity(ent);
        return;
    }

    ent->s.tempEffectId = (uint8_t)soundAlias0;
    ent->s.hintStringIndex = (uint8_t)soundAlias1;
    ent->s.loopedFx.repeatDelayMs = repeatDelay;
    trap_LinkEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x5a708  G_SetSoundBlendAndPitch                                  */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5a708, 6a708_G_SetSoundBlendAndPitch.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - both-aliases-zero unlink branch, byte-narrowed alias stores, floor(pitch*100) negation, 0.99 volume clamp, pitch payload store at 0xd8, link call, and void return checked against current decompiler output. */
void G_SetSoundBlendAndPitch(gentity_t *ent, int soundAlias0,
                             int soundAlias1, float volume, float pitch)
{
    double pitchProduct;
    double encodedPitch;

    if (soundAlias0 == 0 && soundAlias1 == 0) {
        trap_UnlinkEntity(ent);
        return;
    }

    ent->s.tempEffectId = (uint8_t)soundAlias0;
    ent->s.hintStringIndex = (uint8_t)soundAlias1;

    /* 0x5a753: pitch*100.0f is a single float mul stored to double (exact, 48
     * bits); floor()+sign-flip and the final (float) cast are native.  The two
     * (encodedPitch - x) subtracts stay 80-bit and round to double -> shim.
     * PITCH_MAX loads as 0.99f-widened-to-double (0x3FEFAE1480000000), which
     * (long double)0.99f / x87f_load_f32(0.99f) reproduce exactly. */
    pitchProduct = (double)((long double)pitch *
                            (long double)SOUND_BLEND_PITCH_SCALE);
    encodedPitch = -floor(pitchProduct);
    if (volume < SOUND_BLEND_PITCH_MAX) {
#if EMULATE_X87
        encodedPitch = x87f_store_f64(x87f_sub(
            x87f_load_f64(encodedPitch), x87f_load_f32(volume)));
#else
        encodedPitch = (double)((long double)encodedPitch -
                                (long double)volume);
#endif
    } else {
#if EMULATE_X87
        encodedPitch = x87f_store_f64(x87f_sub(
            x87f_load_f64(encodedPitch),
            x87f_load_f32(SOUND_BLEND_PITCH_MAX)));
#else
        encodedPitch = (double)((long double)encodedPitch -
                                (long double)SOUND_BLEND_PITCH_MAX);
#endif
    }
    game_compat_g_set_sound_blend_pitch_payload(ent, (float)encodedPitch);

    trap_LinkEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x5a7b7  SP_misc_model                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5a7b7, 6a7b7_SP_misc_model.c, VERIFY-ENTITYMGMT-ORIGIN-VIS-2026-06-17): DATAFLOW_VERIFIED - direct G_FreeEntity call, argument forwarding, and void return checked against current decompiler output. */
void SP_misc_model(gentity_t *ent)
{
    G_FreeEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x5a7da  use_corona                                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5a7da, 6a7da_use_corona.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - linkedState branch at offset 0xf4, activeState byte clear at 0x17e before link, unlink else path, ignored extra callback parameters, and void return checked against current decompiler output. */
void use_corona(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
    (void)other;
    (void)activator;

    if (ent->linkedState == 0) {
        ent->activeState = 0;
        trap_LinkEntity(ent);
    } else {
        trap_UnlinkEntity(ent);
    }
}

/* ------------------------------------------------------------------ */
/*  0x5a820  SP_corona                                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5a820, 6a820_SP_corona.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - direct G_FreeEntity call, argument forwarding, void return, and no other side effects checked against current decompiler output. */
void SP_corona(gentity_t *ent)
{
    G_FreeEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x75a34  target_location_linkup                                   */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x75a34, 85a34_target_location_linkup.c, VERIFY-ENTITYMGMT-ORIGIN-VIS-2026-06-17): DATAFLOW_VERIFIED - one-shot link guard, configstring base 53, entity scan bounds, classname test, config index store at +0x240, message conversion, linked-list insertion, and void return checked against current decompiler output. */
void target_location_linkup(gentity_t *ent)
{
    int targetLocationConfigIndex;
    int entityIndex;

    (void)ent;

    if (level.targetLocationsLinked != 0) {
        return;
    }

    level.targetLocationsLinked = 1;
    level.targetLocationHead = NULL;
    trap_SetConfigstring(TARGET_LOCATION_CONFIGSTRING_BASE, "unknown");

    targetLocationConfigIndex = 1;
    for (entityIndex = 0; entityIndex < level.num_entities; ++entityIndex) {
        gentity_t *candidate =
            &g_entities[entityIndex];

        if (candidate->scriptClassname == scr_const_target_location) {
            /* target_location reuses gentity +0x240, the generic health slot,
             * as its configstring index. */
            candidate->health = targetLocationConfigIndex;
            trap_SetConfigstring(
                TARGET_LOCATION_CONFIGSTRING_BASE +
                    targetLocationConfigIndex,
                SL_ConvertToString(candidate->targetLocationMessage));

            ++targetLocationConfigIndex;
            candidate->targetLocationNext = level.targetLocationHead;
            level.targetLocationHead = candidate;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x75b43  SP_target_location                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x75b43, 85b43_SP_target_location.c, VERIFY-ENTITYMGMT-ORIGIN-VIS-2026-06-17): DATAFLOW_VERIFIED - classname set, think callback assignment, nextthink level.time+200, G_SetOrigin argument order, and void return checked against current decompiler output. */
void SP_target_location(gentity_t *ent)
{
    Scr_SetString(&ent->scriptClassname, scr_const_target_location);
    ent->think = target_location_linkup;
    ent->nextthink = coduo_int32_from_bits(
        (uint32_t)level.time + (uint32_t)TARGET_LOCATION_LINK_DELAY);
    G_SetOrigin(ent, ent->currentOrigin);
}

/* ------------------------------------------------------------------ */
/*  0x5a843  G_IsInMatchTimeout                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5a843, 6a843_G_IsInMatchTimeout.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - bool/int return sourced from level frameTime zero test and no side effects checked against current decompiler output. */
int G_IsInMatchTimeout(void)
{
    return level.frameTime == 0;
}

/* ------------------------------------------------------------------ */
/*  0x5a868  G_InitTurrets                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5a868, 6a868_G_InitTurrets.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - 32-slot loop, 0x48 turret-state stride, inUse zero store, and void return checked against current decompiler output. */
void G_InitTurrets(void)
{
    int slot;

    for (slot = 0; slot < MAX_TURRETS; ++slot) {
        g_turretStates[slot].inUse = 0;
    }
}

/* ------------------------------------------------------------------ */
/*  0x5a8b0  G_CalcTurretMuzzlePoints                                 */
/* ------------------------------------------------------------------ */

/* NOT_FROM_ORIGINAL_SOURCE: three-float subtract for muzzle reconstruction; extracted during reconstruction of 0x5a8b0. */
/* VERIFIED_DECOMPILER(0x5a8b0, 6a8b0_FUN_0006a8b0.c, VERIFY-NEXT-001-TURRET-FIRE-2026-06-17): DATAFLOW_VERIFIED - extracted flash-minus-player origin vector stores preserve the decompiler assignments to the three local vector components. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_CalcTurretMuzzlePoints (0x5a8b0); no standalone original body. */
static void game_compat_g_turret_vector_subtract(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: three-float multiply-add for muzzle reconstruction; extracted during reconstruction of 0x5a8b0. */
/* VERIFIED_DECOMPILER(0x5a8b0, 6a8b0_FUN_0006a8b0.c, VERIFY-NEXT-001-TURRET-FIRE-2026-06-17): DATAFLOW_VERIFIED - extracted player-origin plus normalized distance scaled by forward vector stores preserve muzzle origin writes at output indices 9..11. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_CalcTurretMuzzlePoints (0x5a8b0); no standalone original body. */
static void game_compat_g_turret_vector_ma(const float *start, float scale, const float *dir,
                             float *out)
{
    out[0] = start[0] + dir[0] * scale;
    out[1] = start[1] + dir[1] * scale;
    out[2] = start[2] + dir[2] * scale;
}

/* NOT_FROM_ORIGINAL_SOURCE: turret pitch/yaw pair accessor; extracted during reconstruction of 0x5bedf. */
/* VERIFIED_DECOMPILER(0x5bedf, 6bedf_FUN_0006bedf.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - extracted accessor preserves decompiler reads and writes at entityStateAngles2.turret.pitch/yaw offsets 0x68 and 0x6c. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_TurretAimAtAngles (0x5bedf); no standalone original body. */
static float *game_compat_g_turret_pitch_yaw(gentity_t *turret)
{
    return &turret->s.turret.pitch;
}

/* NOT_FROM_ORIGINAL_SOURCE: turret pitch carry accessor; extracted during reconstruction of 0x5bedf. */
/* VERIFIED_DECOMPILER(0x5bedf, 6bedf_FUN_0006bedf.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - extracted accessor preserves the decompiler pitch carry store/subtract at entityStateAngles2.turret.pitchCarry offset 0x70. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_TurretAimAtAngles (0x5bedf); no standalone original body. */
static float *game_compat_g_turret_pitch_carry(gentity_t *turret)
{
    return &turret->s.turret.pitchCarry;
}

/* NOT_FROM_ORIGINAL_SOURCE: bottom-arc payload accessor; extracted during reconstruction of 0x5b950. */
/* VERIFIED_DECOMPILER(0x5b950, 6b950_FUN_0006b950.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - extracted accessor preserves the decompiler bottomArc payload store through entity offset 0xd8. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_UpdateTurretClientAiming (0x5b950); no standalone original body. */
static float *game_compat_g_turret_bottom_arc_payload(gentity_t *turret)
{
    return &turret->s.clientInfoLeanFraction;
}

/* NOT_FROM_ORIGINAL_SOURCE: axis minimum arc selector; extracted during reconstruction of 0x5c919. */
/* VERIFIED_DECOMPILER(0x5c919, 6c919_turret_use.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - extracted selector preserves min clamp source offsets 0x0c/0x10 for pitch/yaw axes in turret_use. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original turret_use (0x5c919); no standalone original body. */
static float game_compat_g_turret_min_arc_for_axis(const turret_state_t *turretState,
                                   int axisIndex)
{
    return axisIndex == 0 ? turretState->topArc : turretState->rightArc;
}

/* NOT_FROM_ORIGINAL_SOURCE: axis maximum arc selector; extracted during reconstruction of 0x5c919. */
/* VERIFIED_DECOMPILER(0x5c919, 6c919_turret_use.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - extracted selector preserves max clamp source offsets 0x14/0x18 for pitch/yaw axes in turret_use. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original turret_use (0x5c919); no standalone original body. */
static float game_compat_g_turret_max_arc_for_axis(const turret_state_t *turretState,
                                   int axisIndex)
{
    return axisIndex == 0 ? turretState->bottomArc : turretState->leftArc;
}

/* VERIFIED_DECOMPILER(0x5a8b0, 6a8b0_FUN_0006a8b0.c, VERIFY-NEXT-001-TURRET-FIRE-2026-06-17): DATAFLOW_VERIFIED - tag lookup failure paths, classname diagnostics, angle composition from angles2/currentAngles, AngleVectors outputs, forward copy, normalized flash-player distance, muzzle origin stores, and qboolean return checked against current decompiler output. */
qboolean G_CalcTurretMuzzlePoints(gentity_t *turret, gentity_t *fireEnt,
                                  weapon_muzzle_t *muzzle)
{
    DObjSkelMat flashMatrix;
    DObjSkelMat playerMatrix;
    vec3_t angles;
    vec3_t flashToPlayer;
    float distance;

    (void)fireEnt;

    if (G_DObjGetWorldTagMatrix(turret, TURRET_TAG_FLASH, &flashMatrix) == 0) {
        Com_Printf("Couldn't find %s on turret (entity %d, classname '%s').\n",
                   TURRET_TAG_FLASH, turret->s.number,
                   SL_ConvertToString(turret->scriptClassname));
        return qfalse;
    }

    if (G_DObjGetWorldTagMatrix(turret, TURRET_TAG_PLAYER, &playerMatrix) == 0) {
        Com_Printf("Couldn't find %s on turret (entity %d, classname '%s').\n",
                   TURRET_TAG_PLAYER, turret->s.number,
                   SL_ConvertToString(turret->scriptClassname));
        return qfalse;
    }

    angles[0] = turret->s.turret.pitch +
                turret->currentAngles[0];
    angles[1] = turret->s.turret.yaw + turret->currentAngles[1];
    angles[2] = 0.0f;
    AngleVectors(angles, muzzle->forward, muzzle->right, muzzle->up);

    muzzle->extraVector[0] = muzzle->forward[0];
    muzzle->extraVector[1] = muzzle->forward[1];
    muzzle->extraVector[2] = muzzle->forward[2];

    game_compat_g_turret_vector_subtract(flashMatrix.origin,
                           playerMatrix.origin,
                           flashToPlayer);
    distance = VectorNormalize(flashToPlayer);
    game_compat_g_turret_vector_ma(playerMatrix.origin,
                     distance, muzzle->forward, muzzle->origin);

    return qtrue;
}

/* ------------------------------------------------------------------ */
/*  0x5aaa9  G_FireTurret                                             */
/* ------------------------------------------------------------------ */

/* NOT_FROM_ORIGINAL_SOURCE: turret/player active byte accessor; extracted during reconstruction of 0x5bcc8. */
/* VERIFIED_DECOMPILER(0x5bcc8, 6bcc8_G_ClientStopUsingTurret.c, VERIFY-ENTITYMGMT-EARLY-SPAWN-SOUND-TURRET-2026-06-17): DATAFLOW_VERIFIED - extracted accessor preserves active byte reads/writes at gentity offset 0x17e. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_FireTurret (0x5aaa9); no standalone original body. */
static uint8_t *game_compat_g_turret_active_byte(gentity_t *ent)
{
    return &ent->activeState;
}

/* NOT_FROM_ORIGINAL_SOURCE: ENTITYNUM_NONE to world fire entity normalization; extracted during reconstruction of 0x5aaa9. */
/* VERIFIED_DECOMPILER(0x5aaa9, 6aaa9_FUN_0006aaa9.c, VERIFY-NEXT-001-TURRET-FIRE-2026-06-17): DATAFLOW_VERIFIED - ENTITYNUM_NONE pointer comparison and fallback to ENTITYNUM_WORLD match the decompiler branch and assignment before muzzle calculation. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_FireTurret (0x5aaa9); no standalone original body. */
static gentity_t *game_compat_g_turret_normalize_fire_entity(gentity_t *fireEnt)
{
    if (fireEnt == &g_entities[ENTITYNUM_NONE]) {
        return &g_entities[ENTITYNUM_WORLD];
    }
    return fireEnt;
}

/* VERIFIED_DECOMPILER(0x5aaa9, 6aaa9_FUN_0006aaa9.c, VERIFY-NEXT-001-TURRET-FIRE-2026-06-17): DATAFLOW_VERIFIED - match-timeout short-circuit, turretState overheating guard, fire entity normalization, muzzle calculation gate, weapon-info lookup/store, bullet-vs-rocket branch, heat increment, miss event parameter, and void return checked against current decompiler output. */
void G_FireTurret(gentity_t *turret, gentity_t *fireEnt, int damage)
{
    level_locals_t *lvl = &level;
    turret_state_t *turretState;
    gentity_t *normalizedFireEnt;
    weapon_muzzle_t muzzle;
    const weaponInfo_t *weaponInfo;
    qboolean hit;

    if (lvl->matchTimeoutDuration != 0 ||
        lvl->matchTimeoutRecoveryEndTime != 0) {
        return;
    }

    turretState = turret->turretState;
    if (turretState->overheating != 0) {
        return;
    }

    normalizedFireEnt = game_compat_g_turret_normalize_fire_entity(fireEnt);
    if (G_CalcTurretMuzzlePoints(turret, normalizedFireEnt, &muzzle) == 0) {
        return;
    }

    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(turret->s.weapon);
    muzzle.weaponInfo = weaponInfo;

    if (weaponInfo->weaponType == WEAPTYPE_BULLET) {
        hit = Bullet_Fire(normalizedFireEnt, 0.0f, damage, &muzzle, turret);
        turretState->heat += weaponInfo->turretHeatPerShot;
        G_AddEvent(turret, EV_FIRE_WEAPON_MG42,
                   hit == 0 ? TURRET_BULLET_MISS_EVENT_PARM : 0);
    } else {
        Weapon_RocketLauncher_Fire(turret, 0.0f, &muzzle);
    }
}

/* ------------------------------------------------------------------ */
/*  0x5ac06  G_PlayerTurretPositionAndBlend                             */
/* ------------------------------------------------------------------ */

/* NOT_FROM_ORIGINAL_SOURCE: player animation id builder; extracted during reconstruction of 0x5ac06. */
/* VERIFIED_DECOMPILER(0x5ac06, 6ac06_FUN_0006ac06.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; Scr_GetAnimsIndex, low animation word, shift, and 0xfffffdff mask match the root animation expression. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_PlayerTurretPositionAndBlend (0x5ac06); no standalone original body. */
static uint32_t game_compat_g_turret_player_anim(uint32_t animationWord)
{
    uint32_t animTreeIndex = (uint32_t)Scr_GetAnimsIndex(bgAnimStaticTable->animTreeHandle);

    return ((animTreeIndex << SCR_ANIM_TREE_INDEX_SHIFT) |
            (animationWord & 0xffffu)) &
           ~ANIM_TOGGLEBIT;
}

/* NOT_FROM_ORIGINAL_SOURCE: weight blend-time expression; extracted during reconstruction of 0x5ac06. */
/* VERIFIED_DECOMPILER(0x5ac06, 6ac06_FUN_0006ac06.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; current weight delta, 1000/frameTime blend frame calculation, positive reciprocal, and zero-time path match. */
/* targetWeight is long double so call sites pass their weight EXPRESSION at
 * full 80-bit precision, as the stock inline code does (e.g. 0x5b0fe
 * subtracts the unrounded 1.0-secondWeight, not the float argument). */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_PlayerTurretPositionAndBlend (0x5ac06); no standalone original body. */
static float game_compat_g_turret_xanim_goal_time_for_weight(XAnimTree *animTree, uint32_t anim,
                                            long double targetWeight)
{
    /* 0x5b0f9..0x5b12d: |weight - targetWeight| kept 80-bit -> float (0x5b0fe);
     * (BLEND_MSEC / fild(frameTime)) * weightDelta -> float (0x5b111); 1.0f /
     * blendFrames (0x5b148).  targetWeight carries an exact-float value (1.0,
     * selectedWeight, or the float-exact 1.0-selectedWeight), so load it f64. */
#if EMULATE_X87
    float weight = trap_XAnimGetWeight(animTree, anim);
    float weightDelta = x87f_store_f32(x87f_abs(x87f_sub(
        x87f_load_f32(weight), x87f_load_f64((double)targetWeight))));
    float blendFrames = x87f_store_f32(x87f_mul(
        x87f_div(x87f_load_f32(TURRET_POSITION_BLEND_MSEC),
                 x87f_load_i32(level.frameTime)),
        x87f_load_f32(weightDelta)));

    if (blendFrames > 0.0f) {
        return x87f_store_f32(
            x87f_div(x87f_load_f32(1.0f), x87f_load_f32(blendFrames)));
    }
    return 0.0f;
#else
    float weightDelta = fabsl(trap_XAnimGetWeight(animTree, anim) -
                              targetWeight);
    float blendFrames = (TURRET_POSITION_BLEND_MSEC /
                         level.frameTime) *
                        weightDelta;

    if (blendFrames > 0.0f) {
        return 1.0f / blendFrames;
    }
    return 0.0f;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: goal-weight wrapper; extracted during reconstruction of 0x5ac06. */
/* VERIFIED_DECOMPILER(0x5ac06, 6ac06_FUN_0006ac06.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; trap_XAnimSetGoalWeight argument order, computed goal time, fixed rate, notify, and restart fields match. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_PlayerTurretPositionAndBlend (0x5ac06); no standalone original body. */
static void game_compat_g_turret_set_anim_weight(XAnimTree *animTree, uint32_t anim,
                                  long double targetWeight)
{
    trap_XAnimSetGoalWeight(animTree, anim, targetWeight,
                            game_compat_g_turret_xanim_goal_time_for_weight(animTree, anim,
                                                           targetWeight),
                            1.0f,
                            0, 0);
}

/* NOT_FROM_ORIGINAL_SOURCE: animation child-position clamp; extracted during reconstruction of 0x5ac06. */
/* VERIFIED_DECOMPILER(0x5ac06, 6ac06_FUN_0006ac06.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; negative clamp, childCount-1 upper clamp, and pass-through path match. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_PlayerTurretPositionAndBlend (0x5ac06); no standalone original body. */
static float game_compat_g_turret_clamp_child_position(float childPosition, int childCount)
{
    if (childPosition < 0.0f) {
        return 0.0f;
    }

    if ((float)(childCount - 1) <= childPosition) {
        return (float)(childCount - 1);
    }

    return childPosition;
}

/* NOT_FROM_ORIGINAL_SOURCE: two-child blend helper; extracted during reconstruction of 0x5ac06. */
/* VERIFIED_DECOMPILER(0x5ac06, 6ac06_FUN_0006ac06.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; previous top-animation child lookup, fractional split, NaN-inclusive second child path, and weighted goal calls match. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_PlayerTurretPositionAndBlend (0x5ac06); no standalone original body. */
static void game_compat_g_turret_set_child_pair_weight(XAnimTree *animTree, uint32_t parentAnim,
                                       int childIndex, float secondWeight,
                                       float weightScale)
{
    uint32_t childAnim;

    trap_XAnimGetChildAt(&childAnim, parentAnim, childIndex);
    game_compat_g_turret_set_anim_weight(animTree, childAnim,
                          (1.0f - secondWeight) * weightScale);

    if (secondWeight != 0.0f || isnan(secondWeight)) {
        trap_XAnimGetChildAt(&childAnim, parentAnim, childIndex + 1);
        game_compat_g_turret_set_anim_weight(animTree, childAnim,
                              secondWeight * weightScale);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: two-child initial weight helper; extracted during reconstruction of 0x5ac06. */
/* VERIFIED_DECOMPILER(0x5ac06, 6ac06_FUN_0006ac06.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; search-loop child lookup outputs, immediate 1.0-rate priming, fractional split, and NaN-inclusive second child path match. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_PlayerTurretPositionAndBlend (0x5ac06); no standalone original body. */
static float game_compat_g_turret_prime_child_pair(XAnimTree *animTree, uint32_t parentAnim,
                                    float childPosition,
                                    int *childIndexOut,
                                    uint32_t *firstChildAnim,
                                    uint32_t *secondChildAnim)
{
#if EMULATE_X87
    int childIndex = x87f_store_i32_trunc(x87f_load_f32(childPosition));
#elif defined(__i386__) || defined(__x86_64__)
    int childIndex =
        CODUO_X87_TRUNCATE_I32((long double)childPosition);
#else
    int childIndex = game_compat_int32_from_float_trunc(childPosition);
#endif
    float secondWeight = childPosition - (float)childIndex;

    *childIndexOut = childIndex;
    *secondChildAnim = 0;
    trap_XAnimGetChildAt(firstChildAnim, parentAnim, childIndex);
    trap_XAnimSetGoalWeight(animTree, *firstChildAnim, 1.0f - secondWeight,
                            1.0f, 1.0f, 0, 0);

    if (secondWeight != 0.0f || isnan(secondWeight)) {
        trap_XAnimGetChildAt(secondChildAnim, parentAnim, childIndex + 1);
        trap_XAnimSetGoalWeight(animTree, *secondChildAnim, secondWeight,
                                1.0f, 1.0f, 0, 0);
    }

    return secondWeight;
}

/* NOT_FROM_ORIGINAL_SOURCE: aiming animation search loop; extracted during reconstruction of 0x5ac06. */
/* VERIFIED_DECOMPILER(0x5ac06, 6ac06_FUN_0006ac06.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; top-child iteration, no-child errors, child-position math, priming side effects, delta-Z selection, previous sample capture, and loop exit match. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_PlayerTurretPositionAndBlend (0x5ac06); no standalone original body. */
static int game_compat_g_turret_select_aiming_anim(XAnimTree *animTree, uint32_t rootAnim,
                                    float targetForwardOffset,
                                    float turretYaw,
                                    float horizontalRotateIncrement,
                                    uint32_t *selectedAnim,
                                    uint32_t *selectedFirstChildAnim,
                                    uint32_t *selectedSecondChildAnim,
                                    float *selectedSecondWeight,
                                    int *previousChildIndex,
                                    float *previousSecondWeight,
                                    float *previousDeltaZ,
                                    float *currentDeltaZ,
                                    int *topChildCountOut)
{
    int topChildCount = trap_XAnimGetNumChildren(rootAnim);
    int topIndex;
    vec3_t deltaRotation;
    vec3_t deltaTranslation;

    if (topChildCount == 0) {
        Com_Error(1, COM_ERROR_MARKER "Player anim '%s' has no children",
                  trap_XAnimGetAnimName(rootAnim));
    }

    *topChildCountOut = topChildCount;
    *previousDeltaZ = 0.0f;
    *selectedFirstChildAnim = 0;
    *selectedSecondChildAnim = 0;
    *selectedSecondWeight = 0.0f;
    *previousChildIndex = 0;
    *previousSecondWeight = 0.0f;
    *currentDeltaZ = 0.0f;

    for (topIndex = 0; topIndex < topChildCount; ++topIndex) {
        uint32_t topAnim;
        uint32_t firstChildAnim;
        uint32_t secondChildAnim;
        int childCount;
        int childIndex;
        float childPosition;
        float secondWeight;

        trap_XAnimGetChildAt(&topAnim, rootAnim, topIndex);
        trap_XAnimSetGoalWeight(animTree, topAnim, 1.0f, 1.0f, 1.0f, 0, 0);

        childCount = trap_XAnimGetNumChildren(topAnim);
        if (childCount == 0) {
            Com_Error(1, COM_ERROR_MARKER "Player anim '%s' has no children",
                      trap_XAnimGetAnimName(topAnim));
        }

        childPosition = game_compat_g_turret_clamp_child_position(
            (float)childCount * 0.5f - turretYaw / horizontalRotateIncrement,
            childCount);
        secondWeight = game_compat_g_turret_prime_child_pair(animTree, topAnim,
                                              childPosition, &childIndex,
                                              &firstChildAnim,
                                              &secondChildAnim);

        trap_XAnimCalcAbsDelta(animTree, topAnim, deltaRotation,
                               deltaTranslation);
        (void)deltaRotation;

        *selectedAnim = topAnim;
        *selectedFirstChildAnim = firstChildAnim;
        *selectedSecondChildAnim = secondChildAnim;
        *selectedSecondWeight = secondWeight;
        *currentDeltaZ = deltaTranslation[2];

        if (targetForwardOffset <= deltaTranslation[2]) {
            break;
        }

        *previousDeltaZ = deltaTranslation[2];
        *previousChildIndex = childIndex;
        *previousSecondWeight = secondWeight;
    }

    return topIndex;
}

/* VERIFIED_DECOMPILER(0x5ac06, 6ac06_FUN_0006ac06.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; animation gates, tag lookups, root animation setup, aiming search, post-clear child/top weights, delta transform, trace adjustment, player state copy, angles, and link side effects match. */
void G_PlayerTurretPositionAndBlend(gentity_t *player, gentity_t *turret)
{
    clientInfo_t *clientInfo = &bgs.clientinfo[player->s.clientNum];
    gclient_t *client = player->client;
    const weaponInfo_t *weaponInfo;
    uint32_t animationReference;
    uint32_t animationWord;
    uint32_t rootAnim;
    uint32_t selectedAnim = 0;
    uint32_t selectedFirstChildAnim = 0;
    uint32_t selectedSecondChildAnim = 0;
    XAnimTree *animTree;
    const DObjSkelMat *weaponTagMatrix;
    matrix43_t turretAxis;
    matrix43_t yawTransform;
    matrix43_t combinedAxis;
    vec3_t deltaRotation;
    vec3_t deltaTranslation;
    vec3_t viewDelta;
    float tagForwardOffset;
    float targetForwardOffset;
    float turretYaw;
    float selectedSecondWeight = 0.0f;
    int previousChildIndex;
    float previousSecondWeight;
    float previousDeltaZ;
    float currentDeltaZ;
    int selectedTopIndex;
    int topChildCount;
    trace_t trace;
    vec3_t traceStart;
    vec3_t traceEnd;

    /* Read the animation word and entry reference from the flattened
     * legs-animation slot. The reference is an entry pointer in the original
     * i386 modules and a table-relative byte offset in native 64-bit builds. */
    animationWord = game_compat_bg_anim_slot_animation_word(clientInfo, offsetof(clientInfo_t, legsYawAngle));
    animationReference = game_compat_bg_anim_slot_animation_reference(clientInfo, offsetof(clientInfo_t, legsYawAngle));
    if (animationWord == 0 ||
        animationReference == 0 ||
        (game_compat_bg_static_animation_flags_from_reference(bgAnimStaticTable, animationReference) & BG_ANIM_ENTRY_TURRET) == 0) {
        return;
    }

    weaponTagMatrix = G_DObjGetLocalTagMatrix(turret, TURRET_TAG_WEAPON);
    if (weaponTagMatrix == NULL) {
        Com_Printf("WARNING: aborting player positioning on turret since 'tag_weapon' does not exist\n");
        return;
    }

    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(turret->s.weapon);
    animTree = clientInfo->animTree;
    rootAnim = game_compat_g_turret_player_anim(animationWord);
    turretYaw = vectosignedyaw(weaponTagMatrix->axis[0]);

    AnglesToAxis(turret->currentAngles, turretAxis.axis);
    turretAxis.origin[0] = turret->currentOrigin[0];
    turretAxis.origin[1] = turret->currentOrigin[1];
    turretAxis.origin[2] = turret->currentOrigin[2];

    viewDelta[0] = player->currentOrigin[0] - turret->currentOrigin[0];
    viewDelta[1] = player->currentOrigin[1] - turret->currentOrigin[1];
    viewDelta[2] = player->currentOrigin[2] - turret->currentOrigin[2];
    tagForwardOffset = viewDelta[0] * turretAxis.axis[2][0] +
                       viewDelta[1] * turretAxis.axis[2][1] +
                       viewDelta[2] * turretAxis.axis[2][2];
    targetForwardOffset = tagForwardOffset -
                          weaponTagMatrix->origin[2];

    trap_XAnimClearTreeGoalWeightsStrict(animTree, rootAnim, 0.0f);
    selectedTopIndex = game_compat_g_turret_select_aiming_anim(
        animTree, rootAnim, targetForwardOffset, turretYaw,
        weaponInfo->animHorRotateInc,
        &selectedAnim, &selectedFirstChildAnim, &selectedSecondChildAnim,
        &selectedSecondWeight, &previousChildIndex, &previousSecondWeight,
        &previousDeltaZ,
        &currentDeltaZ, &topChildCount);

    trap_XAnimClearTreeGoalWeightsStrict(animTree, rootAnim, 0.0f);
    game_compat_g_turret_set_anim_weight(animTree, selectedFirstChildAnim,
                          1.0f - selectedSecondWeight);
    if (selectedSecondWeight != 0.0f || isnan(selectedSecondWeight)) {
        game_compat_g_turret_set_anim_weight(animTree, selectedSecondChildAnim,
                              selectedSecondWeight);
    }

    if (selectedTopIndex == 0 || selectedTopIndex == topChildCount) {
        if (G_DObjGetLocalTagMatrix(turret, TURRET_TAG_AIM) == NULL) {
            Com_Printf("WARNING: aborting player positioning on turret since 'tag_aim' does not exist\n");
            return;
        }
        game_compat_g_turret_set_anim_weight(animTree, selectedAnim, 1.0f);
    } else {
        uint32_t previousAnim;
        float selectedWeight =
            (targetForwardOffset - previousDeltaZ) /
            (currentDeltaZ - previousDeltaZ);

        game_compat_g_turret_set_anim_weight(animTree, selectedAnim, selectedWeight);
        trap_XAnimGetChildAt(&previousAnim, rootAnim, selectedTopIndex - 1);
        game_compat_g_turret_set_anim_weight(animTree, previousAnim, 1.0f - selectedWeight);
        game_compat_g_turret_set_child_pair_weight(animTree, previousAnim,
                                   previousChildIndex, previousSecondWeight,
                                   1.0f);
    }

    trap_XAnimCalcAbsDelta(animTree, rootAnim, deltaRotation, deltaTranslation);
    VectorAngleMultiply(deltaTranslation, turretYaw);

    yawTransform.origin[0] = deltaTranslation[0] +
                             weaponTagMatrix->origin[0];
    yawTransform.origin[1] = deltaTranslation[1] +
                             weaponTagMatrix->origin[1];
    yawTransform.origin[2] = tagForwardOffset;
    YawToAxis(RotationToYaw(deltaRotation) + turretYaw, yawTransform.axis);
    MatrixMultiply43(&yawTransform, &turretAxis, &combinedAxis);

    client->ps.psOrigin[0] = combinedAxis.origin[0];
    client->ps.psOrigin[1] = combinedAxis.origin[1];
    client->ps.psOrigin[2] = combinedAxis.origin[2];

    traceStart[0] = client->ps.psOrigin[0];
    traceStart[1] = client->ps.psOrigin[1];
    traceStart[2] = client->ps.psOrigin[2];
    traceEnd[0] = client->ps.psOrigin[0];
    traceEnd[1] = client->ps.psOrigin[1];
    traceEnd[2] = turret->currentOrigin[2];
    trap_Trace(&trace, traceStart, vec3_origin, vec3_origin, traceEnd,
               player->s.number, TURRET_TRACE_CONTENTS);
    if (trace.fraction < 1.0f) {
        client->ps.psOrigin[2] = trace.endpos[2];
    }

    BG_PlayerStateToEntityState(&client->ps, &player->s, qtrue);
    player->currentOrigin[0] = client->ps.psOrigin[0];
    player->currentOrigin[1] = client->ps.psOrigin[1];
    player->currentOrigin[2] = client->ps.psOrigin[2];
    /* C99 multidimensional-array qualifier bridge; AxisToAngles retains a
     * read-only view of the composed axis. */
    AxisToAngles((const vec_t (*)[3])combinedAxis.axis,
                 player->currentAngles);
    trap_LinkEntity(player);
}

/* ------------------------------------------------------------------ */
/*  0x5b950  G_UpdateTurretClientAiming                               */
/* ------------------------------------------------------------------ */

/* NOT_FROM_ORIGINAL_SOURCE: client aiming clamp expression; extracted during reconstruction of 0x5b950. */
/* VERIFIED_DECOMPILER(0x5b950, 6b950_FUN_0006b950.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; lower clamp, upper clamp, and pass-through return match the pitch/yaw clamp expression. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_UpdateTurretClientAiming (0x5b950); no standalone original body. */
static float game_compat_g_turret_clamp_angle(float angle, float minAngle, float maxAngle)
{
    if (angle < minAngle) {
        return minAngle;
    }

    if (angle > maxAngle) {
        return maxAngle;
    }

    return angle;
}

/* VERIFIED_DECOMPILER(0x5b950, 6b950_FUN_0006b950.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; weapon-info lookup, stop-follow clip write, AnglesSubtract arguments, pitch/yaw clamps, pitchCarry clear, arc payload stores, refresh flag clear, and s_flags toggle match. */
void G_UpdateTurretClientAiming(gentity_t *turret, gentity_t *player)
{
    turret_state_t *turretState = turret->turretState;
    vec3_t deltaAngles;
    const weaponInfo_t *weaponInfo =
        (const weaponInfo_t *)BG_GetInfoForWeapon(turret->s.weapon);

    (void)weaponInfo;

    player->client->ps.viewLockedEntityNum = turret->s.number;
    AnglesSubtract(player->client->ps.viewAngles, turret->currentAngles,
                   deltaAngles);

    turret->s.turret.pitch =
        game_compat_g_turret_clamp_angle(deltaAngles[0], turretState->topArc,
                           turretState->bottomArc);
    turret->s.turret.yaw =
        game_compat_g_turret_clamp_angle(deltaAngles[1], turretState->rightArc,
                           turretState->leftArc);
    turret->s.turret.pitchCarry = 0.0f;

    turret->s.loopedFxForward[0] = turretState->leftArc;
    turret->s.loopedFxForward[1] = turretState->topArc;
    turret->s.loopedFxForward[2] = turretState->rightArc;
    *game_compat_g_turret_bottom_arc_payload(turret) = turretState->bottomArc;

    if ((turretState->flags & TURRET_STATE_CLIENT_REFRESH_FLAG) != 0) {
        turretState->flags &= ~TURRET_STATE_CLIENT_REFRESH_FLAG;
        turret->s.eFlags ^= TURRET_CLIENT_STATE_TOGGLE_FLAG;
    }
}

/* ------------------------------------------------------------------ */
/*  0x5bad3  G_FireTurretFromClient                                   */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5bad3, 6bad3_FUN_0006bad3.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; fire sound timer derives from weapon fireTime*3, client-null gate, damage argument, and G_FireTurret call path match. */
void G_FireTurretFromClient(gentity_t *turret, gentity_t *player)
{
    turret_state_t *turretState = turret->turretState;
    const weaponInfo_t *weaponInfo =
        (const weaponInfo_t *)BG_GetInfoForWeapon(turret->s.weapon);

    turretState->fireSoundTime = coduo_int32_from_bits(
        (uint32_t)weaponInfo->fireTime * UINT32_C(3));
    if (player->client != NULL) {
        G_FireTurret(turret, player, turret->damage);
    }
}

/* ------------------------------------------------------------------ */
/*  0x5bb49  G_RunClientTurret                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5bb49, 6bb49_FUN_0006bb49.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; aiming/position calls, weapon lookup, firing flag clear, fire timer decrement/reset, attack and overheat gates, refire timer, fire call, and firing flag set match. */
void G_RunClientTurret(gentity_t *turret, gentity_t *player)
{
    turret_state_t *turretState = turret->turretState;
    const weaponInfo_t *weaponInfo;

    G_UpdateTurretClientAiming(turret, player);
    G_PlayerTurretPositionAndBlend(player, turret);

    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(turret->s.weapon);
    turret->s.eFlags &= ~TURRET_CLIENT_FIRING_FLAG;

    turretState->fireTimeRemaining = coduo_int32_from_bits(
        (uint32_t)turretState->fireTimeRemaining -
        (uint32_t)TURRET_CLIENT_THINK_MSEC);
    if (turretState->fireTimeRemaining < 1) {
        turretState->fireTimeRemaining = 0;
        if ((player->client->currentButtons & PM_BUTTON_FIRE) != 0 &&
            turretState->overheating == 0) {
            turretState->fireTimeRemaining = (int)weaponInfo->fireTime;
            G_FireTurretFromClient(turret, player);
            turret->s.eFlags |= TURRET_CLIENT_FIRING_FLAG;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x5bc29  G_UpdateTurretSound                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5bc29, 6bc29_FUN_0006bc29.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; clientSound clear/loop alias set, 50 ms countdown, stop-alias or timeout condition, final clear, and sound alias call match. */
void G_UpdateTurretSound(gentity_t *turret)
{
    turret_state_t *turretState = turret->turretState;

    turret->s.clientSound = 0;
    if (turretState->fireSoundTime > 0) {
        turret->s.clientSound = turretState->sustainedFireLoopSound;
        turretState->fireSoundTime = coduo_int32_from_bits(
            (uint32_t)turretState->fireSoundTime -
            (uint32_t)TURRET_CLIENT_THINK_MSEC);
        if ((turretState->fireSoundTime < 1 &&
             turretState->sustainedFireStopSound != 0) ||
            G_IsInMatchTimeout() != 0) {
            turret->s.clientSound = 0;
            G_PlaySoundAlias(turret, turretState->sustainedFireStopSound);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x5bcc8  G_ClientStopUsingTurret                                  */
/* ------------------------------------------------------------------ */

/* NOT_FROM_ORIGINAL_SOURCE: stop-use event selector emission; extracted during reconstruction of 0x5bcc8. */
/* VERIFIED_DECOMPILER(0x5bcc8, 6bcc8_G_ClientStopUsingTurret.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; disabled-event return, gunner/alt/default event IDs, event arguments, and selector reset match. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_ClientStopUsingTurret (0x5bcc8); no standalone original body. */
static void game_compat_g_turret_emit_stop_use_event(gentity_t *player,
                                     turret_state_t *turretState)
{
    if (turretState->stopUseEventType == TURRET_STOP_USE_EVENT_DISABLED) {
        return;
    }

    if (turretState->stopUseEventType == TURRET_STOP_USE_EVENT_GUNNER) {
        G_AddEvent(player, EV_STANCE_FORCE_PRONE, 0);
    } else if (turretState->stopUseEventType == TURRET_STOP_USE_EVENT_ALT) {
        G_AddEvent(player, EV_STANCE_FORCE_CROUCH, 0);
    } else {
        G_AddEvent(player, EV_STANCE_FORCE_STAND, 0);
    }
    turretState->stopUseEventType = TURRET_STOP_USE_EVENT_DISABLED;
}

/* VERIFIED_DECOMPILER(0x5bcc8, 6bcc8_G_ClientStopUsingTurret.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; rider lookup, sound timer/clientSound clears, stop-use event dispatch, teleport origin/angles, ps flag clears, follow fields, active bytes, vehicle/pass entity clears, refresh flag clear, and deactivated notify match. */
void G_ClientStopUsingTurret(gentity_t *turret)
{
    turret_state_t *turretState = turret->turretState;
    gentity_t *player = &g_entities[turret->passEntityNum];

    turretState->fireSoundTime = 0;
    turret->s.clientSound = 0;
    game_compat_g_turret_emit_stop_use_event(player, turretState);

    TeleportPlayer(player, turretState->stopUseOrigin, player->currentAngles);
    player->client->ps.entityStateFlags &= ~TURRET_CLIENT_CLEAR_PS_FLAGS;
    player->client->ps.viewLocked = 0;
    player->client->ps.viewLockedEntityNum = ENTITYNUM_NONE;
    *game_compat_g_turret_active_byte(player) = 0;
    player->s.vehicleEntityNum = 0;

    *game_compat_g_turret_active_byte(turret) = 0;
    turret->passEntityNum = ENTITYNUM_NONE;
    turretState->flags &= ~TURRET_STATE_CLIENT_REFRESH_FLAG;
    Scr_Notify(turret, scr_const_deactivated, 0);
}

/* ------------------------------------------------------------------ */
/*  0x5be63  turret_think_client                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5be63, 6be63_turret_think_client.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; passEntity rider lookup, active byte and session-state gates, run/sound calls, and stop-using fallback match. */
void turret_think_client(gentity_t *turret)
{
    gentity_t *player = &g_entities[turret->passEntityNum];

    if (*game_compat_g_turret_active_byte(player) == 1 &&
        player->client->sessionState == SESS_STATE_PLAYING) {
        G_RunClientTurret(turret, player);
        G_UpdateTurretSound(turret);
    } else {
        G_ClientStopUsingTurret(turret);
    }
}

/* ------------------------------------------------------------------ */
/*  0x5bedf  G_TurretAimAtAngles                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5bedf, 6bedf_FUN_0006bedf.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; pitch carry fold-in, default/weapon turn rates, rest-pitch fast clamp, two-axis AngleSubtract loop, arrival flag, rest clamp/recovery flags, final pitch step, carry residual, and return value match. */
qboolean G_TurretAimAtAngles(gentity_t *turret, const float *targetAngles,
                             qboolean useWeaponTurnRates)
{
    turret_state_t *turretState = turret->turretState;
    const weaponInfo_t *weaponInfo;
    float turnRates[2];
    float previousPitch;
    float targetPitch;
    float delta;
    qboolean arrived;
    int axisIndex;

    previousPitch = turret->s.turret.pitch;
    turret->s.turret.pitch +=
        turret->s.turret.pitchCarry;
    arrived = qtrue;

    if (useWeaponTurnRates == qfalse) {
        turnRates[0] = TURRET_DEFAULT_TURN_RATE;
        turnRates[1] = TURRET_DEFAULT_TURN_RATE;
    } else {
        weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(
            turret->s.weapon);
        turnRates[0] = weaponInfo->turretPitchRate;
        weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(
            turret->s.weapon);
        turnRates[1] = weaponInfo->turretYawRate;
    }

    if ((turretState->flags & TURRET_STATE_REST_PITCH_CLAMP_FLAG) != 0 &&
        (turretState->flags & TURRET_STATE_REST_PITCH_RECOVERING_FLAG) != 0 &&
        turnRates[0] < TURRET_FAST_REST_PITCH_RATE) {
        turnRates[0] = TURRET_FAST_REST_PITCH_RATE;
    }

    for (axisIndex = 0; axisIndex < 2; axisIndex++) {
        float *turretAngle = &game_compat_g_turret_pitch_yaw(turret)[axisIndex];

        /* 0x5bfb9: stock multiplies by the single float constant 0x3d4ccccd
         * (rodata 0xa1584); the outer (float) cast pins the folded 50*0.001
         * product to that float value -- without it c99 excess precision
         * would multiply by the wider long double product. */
        turnRates[axisIndex] *= (float)((float)TURRET_THINK_MSEC * 0.001f);
        delta = AngleSubtract(targetAngles[axisIndex], *turretAngle);
        if (turnRates[axisIndex] < delta) {
            arrived = qfalse;
            delta = turnRates[axisIndex];
        } else if (delta < -turnRates[axisIndex]) {
            arrived = qfalse;
            delta = -turnRates[axisIndex];
        }
        *turretAngle += delta;
    }

    targetPitch = turret->s.turret.pitch;
    *game_compat_g_turret_pitch_carry(turret) = targetPitch;

    if ((turretState->flags & TURRET_STATE_REST_PITCH_CLAMP_FLAG) != 0) {
        if ((turretState->flags &
             TURRET_STATE_REST_PITCH_CLAMP_UP_FLAG) == 0) {
            if (turretState->restPitchClamp <
                turret->s.turret.pitch) {
                targetPitch = turretState->restPitchClamp;
            } else {
                turretState->flags &=
                    ~TURRET_STATE_REST_PITCH_RECOVERING_FLAG;
            }
        } else if (turret->s.turret.pitch <
                   turretState->restPitchClamp) {
            targetPitch = turretState->restPitchClamp;
        } else {
            turretState->flags &=
                ~TURRET_STATE_REST_PITCH_RECOVERING_FLAG;
        }
    }

    delta = AngleSubtract(targetPitch, previousPitch);
    if (turnRates[0] < delta) {
        arrived = qfalse;
        delta = turnRates[0];
    } else if (delta < -turnRates[0]) {
        arrived = qfalse;
        delta = -turnRates[0];
    }
    turret->s.turret.pitch = previousPitch + delta;
    *game_compat_g_turret_pitch_carry(turret) -= turret->s.turret.pitch;

    return arrived;
}

/* ------------------------------------------------------------------ */
/*  0x5c18e  G_TurretReturnToRest                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5c18e, 6c18e_FUN_0006c18e.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; rest-pitch versus zero pitch target selection, zero yaw/roll stores, useWeaponTurnRates passthrough, and aim call match. */
void G_TurretReturnToRest(gentity_t *turret, qboolean useWeaponTurnRates)
{
    turret_state_t *turretState = turret->turretState;
    vec3_t targetAngles;

    if (useWeaponTurnRates == qfalse) {
        targetAngles[0] = turretState->restPitch;
    } else {
        targetAngles[0] = 0.0f;
    }
    targetAngles[1] = 0.0f;
    targetAngles[2] = 0.0f;
    G_TurretAimAtAngles(turret, targetAngles, useWeaponTurnRates);
}

/* ------------------------------------------------------------------ */
/*  0x5c1f2  turret_think                                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5c1f2, 6c1f2_turret_think.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; nextthink, optional general link, overheat set/notify/event, overheat clear threshold, heat decay/clamp, rider-client absence, sound update, firing flag clear, and return-to-rest call match. */
void turret_think(gentity_t *turret)
{
    turret_state_t *turretState = turret->turretState;
    const weaponInfo_t *weaponInfo;
    int riderEntityNum = turret->passEntityNum;

    turret->nextthink = coduo_int32_from_bits(
        (uint32_t)level.time + (uint32_t)TURRET_THINK_MSEC);
    if (turret->linkInfo != NULL) {
        G_GeneralLink(turret);
    }

    if (turretState->heat >= TURRET_OVERHEAT_THRESHOLD) {
        turretState->overheating = 1;
        G_AddEvent(turret, EV_OVERHEATING, 0);
        Scr_Notify(turret, scr_const_overheating, 0);
        turret->s.turretOverheatState = 1;
    } else if (turretState->overheating != 0 &&
               turretState->heat <= TURRET_OVERHEAT_CLEAR_THRESHOLD) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        turretState->overheating = 0;
        turret->s.turretOverheatState = 0;
    }

    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(
        turret->s.weapon);
    if (turretState->heat > 0.0f) {
        turretState->heat -= weaponInfo->turretHeatDecay;
    } else {
        turretState->heat = 0.0f;
    }

    if (g_entities[riderEntityNum].client == NULL) {
        G_UpdateTurretSound(turret);
        turret->s.eFlags &= ~TURRET_CLIENT_FIRING_FLAG;
        G_TurretReturnToRest(turret, qfalse);
    }
}

/* ------------------------------------------------------------------ */
/*  0x5c381  turret_think_init                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5c381, 6c381_turret_think_init.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; think/nextthink setup, tag_aim/tag_butt gates, entity axis/origin stores, butt-aim delta, pitch sweep bounds, trace arguments, and restPitch hit update match. */
void turret_think_init(gentity_t *turret)
{
    turret_state_t *turretState = turret->turretState;
    matrix43_t entityAxis;
    vec3_t traceStart;
    vec3_t traceEnd;
    vec3_t tagDelta;
    vec3_t rotatedDelta;
    vec3_t localEnd;
    vec3_t testAngles;
    axis_t pitchAxis;
    const DObjSkelMat *aimTag;
    const DObjSkelMat *buttTag;
    trace_t trace;
    int pitchStep;

    turret->think = turret_think;
    turret->nextthink = coduo_int32_from_bits(
        (uint32_t)level.time + (uint32_t)TURRET_THINK_MSEC);

    aimTag = G_DObjGetLocalTagMatrix(turret, TURRET_TAG_AIM);
    if (aimTag == NULL) {
        return;
    }

    buttTag = G_DObjGetLocalTagMatrix(turret, TURRET_TAG_BUTT);
    if (buttTag == NULL) {
        return;
    }

    AnglesToAxis(turret->currentAngles, entityAxis.axis);
    entityAxis.origin[0] = turret->currentOrigin[0];
    entityAxis.origin[1] = turret->currentOrigin[1];
    entityAxis.origin[2] = turret->currentOrigin[2];

    tagDelta[0] = buttTag->origin[0] - aimTag->origin[0];
    tagDelta[1] = buttTag->origin[1] - aimTag->origin[1];
    tagDelta[2] = buttTag->origin[2] - aimTag->origin[2];
    MatrixTransformVector43(aimTag->origin, &entityAxis, traceStart);

    for (pitchStep = 0; pitchStep <= TURRET_INIT_PITCH_STEPS;
         pitchStep++) {
        testAngles[0] = (float)pitchStep *
                        (TURRET_INIT_PITCH_RANGE /
                         (float)TURRET_INIT_PITCH_STEPS);
        testAngles[1] = 0.0f;
        testAngles[2] = 0.0f;
        AnglesToAxis(testAngles, pitchAxis);
        /* C99 multidimensional-array qualifier bridge; the pitch axis remains
         * read-only in MatrixTransformVector. */
        MatrixTransformVector(tagDelta,
                              (const vec_t (*)[3])pitchAxis,
                              rotatedDelta);

        localEnd[0] = rotatedDelta[0] + aimTag->origin[0];
        localEnd[1] = rotatedDelta[1] + aimTag->origin[1];
        localEnd[2] = rotatedDelta[2] + aimTag->origin[2];
        MatrixTransformVector43(localEnd, &entityAxis, traceEnd);

        trap_LocationalTrace(&trace, traceStart, traceEnd, turret->s.number,
                             TURRET_INIT_TRACE_CONTENTS,
                             bulletPriorityMap);
        if (trace.fraction < 1.0f) {
            turretState->restPitch = testAngles[0];
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x5c5e9  turret_controller                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5c5e9, 6c5e9_turret_controller.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; aim and aim_animated control tag pitch/yaw/zero-roll writes plus flash pitchCarry/zero-yaw reuse match. */
void turret_controller(gentity_t *turret, uint32_t *partBits)
{
    vec3_t tagAngles;

    tagAngles[0] = turret->s.turret.pitch;
    tagAngles[1] = turret->s.turret.yaw;
    tagAngles[2] = 0.0f;
    G_DObjSetControlTagAngles(turret, partBits, TURRET_TAG_AIM,
                              tagAngles);
    G_DObjSetControlTagAngles(turret, partBits, TURRET_TAG_AIM_ANIMATED,
                              tagAngles);

    tagAngles[0] = turret->s.turret.pitchCarry;
    tagAngles[1] = 0.0f;
    G_DObjSetControlTagAngles(turret, partBits, TURRET_TAG_FLASH,
                              tagAngles);
}

/* ------------------------------------------------------------------ */
/*  0x5c693  G_TurretPlayerFacingUseArc                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5c693, 6c693_FUN_0006c693.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; yaw center math, fabs arc half-width, yaw vector normalization, player delta, height/use-mode gate, horizontal normalize, acos degree conversion, and arc comparison match. */
static qboolean G_TurretPlayerFacingUseArc(gentity_t *turret,
                                           gentity_t *player)
{
    turret_state_t *turretState = turret->turretState;
    vec3_t forward;
    vec3_t delta;
    float yawBase;
    float yawCenter;
    float halfUseArc;
    float angleToPlayer;

    /* 0x5c6c0/0x5c6ef: stock rounds currentAngles[1]+rightArc and the
     * half-arc product to float locals before summing them for the
     * AngleNormalize180 argument, and reuses the SAME stored half-arc for
     * the final comparison (computed once, not twice). */
    yawBase = turret->currentAngles[1] + turretState->rightArc;
    /* Stock 0x5c6e2: (|rightArc|+|leftArc|)*0.5 kept 80-bit, one store -> shim;
     * yawBase and yawBase+halfUseArc are single adds (native). */
#if EMULATE_X87
    halfUseArc = x87f_store_f32(x87f_mul(
        x87f_add(x87f_load_f32(fabsf(turretState->rightArc)),
                 x87f_load_f32(fabsf(turretState->leftArc))),
        x87f_load_f32(0.5f)));
#else
    halfUseArc = 0.5f *
                 (fabsf(turretState->rightArc) + fabsf(turretState->leftArc));
#endif
    yawCenter = AngleNormalize180(yawBase + halfUseArc);
    YawVectors(yawCenter, forward, NULL);
    VectorNormalize(forward);

    delta[0] = turret->currentOrigin[0] - player->currentOrigin[0];
    delta[1] = turret->currentOrigin[1] - player->currentOrigin[1];

    if (turret->currentOrigin[2] < player->currentOrigin[2] &&
        turretState->useMode != TURRET_USE_MODE_GUNNER &&
        turretState->useMode != TURRET_USE_MODE_ALT) {
        return qfalse;
    }

    delta[2] = 0.0f;
    VectorNormalize(delta);
    /* Stock 0x5c7aa: forward.delta 3-mul/2-add dot kept 80-bit, stored to float
     * (the Q_acos arg); 0x5c7ce: (Q_acos * 180.0f) / M_PI(QWORD double), one
     * store.  Q_acos is external (stock calls Q_acos@plt) -> faithful. */
#if EMULATE_X87
    float facingDot = x87f_store_f32(x87f_add(x87f_add(
        x87f_mul(x87f_load_f32(forward[0]), x87f_load_f32(delta[0])),
        x87f_mul(x87f_load_f32(forward[1]), x87f_load_f32(delta[1]))),
        x87f_mul(x87f_load_f32(forward[2]), x87f_load_f32(delta[2]))));
    angleToPlayer = x87f_store_f32(x87f_div(
        x87f_mul(x87f_load_f32(Q_acos(facingDot)), x87f_load_f32(180.0f)),
        x87f_load_f64(M_PI)));
#else
    angleToPlayer =
        (float)(((long double)180.0f *
                 (long double)Q_acos(forward[0] * delta[0] +
                                      forward[1] * delta[1] +
                                      forward[2] * delta[2])) /
                (long double)M_PI);
#endif

    /* Stock branches to false only for an ordered angle greater than the
     * half-arc; an unordered Q_acos result follows the true path. */
    return !(angleToPlayer > halfUseArc);
}

/* ------------------------------------------------------------------ */
/*  0x5c80b  G_FreeTurret                                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5c80b, 6c80b_G_FreeTurret.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; occupied-rider client gate, stop-using call, turret active byte clear, turret-state pool inUse clear, and pointer null match. */
void G_FreeTurret(gentity_t *turret)
{
    if (g_entities[turret->passEntityNum].client != NULL) {
        G_ClientStopUsingTurret(turret);
    }

    *game_compat_g_turret_active_byte(turret) = 0;
    turret->turretState->inUse = 0;
    turret->turretState = NULL;
}

/* ------------------------------------------------------------------ */
/*  0x5c888  G_IsTurretUsable                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5c888, 6c888_G_IsTurretUsable.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; active/state/usable gates, use-arc call, grenadeTimeLeft and ground-entity rejection, and qboolean return match. */
qboolean G_IsTurretUsable(gentity_t *turret, gentity_t *player)
{
    gclient_t *client = player->client;

    if (*game_compat_g_turret_active_byte(turret) != 0 ||
        turret->turretState == NULL ||
        turret->takeDamage == 0) {
        return qfalse;
    }

    if (G_TurretPlayerFacingUseArc(turret, player) == qfalse) {
        return qfalse;
    }

    if (client->ps.grenadeTimeLeft != 0 ||
        client->ps.groundEntityNum == ENTITYNUM_NONE) {
        return qfalse;
    }

    return qtrue;
}

/* ------------------------------------------------------------------ */
/*  0x5c919  turret_use                                               */
/* ------------------------------------------------------------------ */

/* NOT_FROM_ORIGINAL_SOURCE: player-state use-mode flag update; extracted during reconstruction of 0x5c919. */
/* VERIFIED_DECOMPILER(0x5c919, 6c919_turret_use.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; gunner/alt/default ps flag masks, set/clear order, and default OR-only path match. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original turret_use (0x5c919); no standalone original body. */
static void game_compat_g_turret_set_use_mode_flags(gclient_t *client,
                                    const turret_state_t *turretState)
{
    if (turretState->useMode == TURRET_USE_MODE_GUNNER) {
        client->ps.entityStateFlags |= TURRET_USE_PS_FLAG_GUNNER;
        client->ps.entityStateFlags &= ~TURRET_USE_PS_FLAG_ALT;
    } else if (turretState->useMode == TURRET_USE_MODE_ALT) {
        client->ps.entityStateFlags |= TURRET_USE_PS_FLAG_ALT;
        client->ps.entityStateFlags &= ~TURRET_USE_PS_FLAG_GUNNER;
    } else {
        client->ps.entityStateFlags |= TURRET_USE_PS_FLAG_MASK;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: stop-use event selector capture; extracted during reconstruction of 0x5c919. */
/* VERIFIED_DECOMPILER(0x5c919, 6c919_turret_use.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; playerStateFlags gunner-first, alt-second, default-zero selector writes match. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original turret_use (0x5c919); no standalone original body. */
static void game_compat_g_turret_select_stop_use_event(turret_state_t *turretState,
                                       const gclient_t *client)
{
    if ((client->ps.playerStateFlags & TURRET_STOP_USE_SELECTOR_GUNNER_BUTTON_FLAG) != 0) {
        turretState->stopUseEventType = TURRET_STOP_USE_EVENT_GUNNER;
    } else if ((client->ps.playerStateFlags &
                TURRET_STOP_USE_SELECTOR_ALT_BUTTON_FLAG) != 0) {
        turretState->stopUseEventType = TURRET_STOP_USE_EVENT_ALT;
    } else {
        turretState->stopUseEventType = 0;
    }
}

/* VERIFIED_DECOMPILER(0x5c919, 6c919_turret_use.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; active byte writes, pass/follow fields, refresh flag, stop origin, vehicle entity, stop event selector, use-mode flags, saved angles, two-axis clamp loop, SetClientViewAngle, Scr_AddEntity, and activated notify match. */
void turret_use(gentity_t *turret, gentity_t *other, gentity_t *activator)
{
    turret_state_t *turretState = turret->turretState;
    gclient_t *client = other->client;
    vec3_t angles;
    int axisIndex;

    (void)activator;

    *game_compat_g_turret_active_byte(other) = 1;
    *game_compat_g_turret_active_byte(turret) = 1;
    turret->passEntityNum = other->s.number;
    client->ps.viewLockedEntityNum = turret->s.number;
    turretState->flags |= TURRET_STATE_CLIENT_REFRESH_FLAG;

    turretState->stopUseOrigin[0] = other->currentOrigin[0];
    turretState->stopUseOrigin[1] = other->currentOrigin[1];
    turretState->stopUseOrigin[2] = other->currentOrigin[2];
    other->s.vehicleEntityNum = turret->s.number;

    game_compat_g_turret_select_stop_use_event(turretState, client);
    game_compat_g_turret_set_use_mode_flags(client, turretState);

    turret->scriptMoverAngleTarget[0] = turret->currentAngles[0];
    turret->scriptMoverAngleTarget[1] = turret->currentAngles[1];
    turret->scriptMoverAngleTarget[2] = turret->currentAngles[2];

    for (axisIndex = 0; axisIndex < 2; axisIndex++) {
        float *turretAngle = &game_compat_g_turret_pitch_yaw(turret)[axisIndex];
        float delta = AngleSubtract(client->ps.viewAngles[axisIndex],
                                    turret->currentAngles[axisIndex]);
        float minAngle = game_compat_g_turret_min_arc_for_axis(turretState, axisIndex);
        float maxAngle = game_compat_g_turret_max_arc_for_axis(turretState, axisIndex);

        if (maxAngle < delta) {
            delta = maxAngle;
        } else if (delta < minAngle) {
            delta = minAngle;
        }
        *turretAngle = delta;
    }

    angles[0] = turret->s.turret.pitch +
                turret->currentAngles[0];
    angles[1] = turret->s.turret.yaw + turret->currentAngles[1];
    angles[2] = 0.0f;
    SetClientViewAngle(other, angles);

    Scr_AddEntity(activator);
    Scr_Notify(turret, scr_const_activated, 1);
}

/* ------------------------------------------------------------------ */
/*  0x5cc27  G_SpawnTurret                                            */
/* ------------------------------------------------------------------ */

/* NOT_FROM_ORIGINAL_SOURCE: turret pool allocation loop; extracted during reconstruction of 0x5cc27. */
/* VERIFIED_DECOMPILER(0x5cc27, 6cc27_G_SpawnTurret.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; 32-slot pool scan, 0x48-byte clear, inUse set, return pointer, and max-turret Com_Error match. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_SpawnTurret (0x5cc27); no standalone original body. */
static turret_state_t *game_compat_g_alloc_turret_state(void)
{
    int slot;

    for (slot = 0; slot < TURRET_MAX_COUNT; slot++) {
        if (g_turretStates[slot].inUse == 0) {
            memset(&g_turretStates[slot], 0, sizeof(g_turretStates[slot]));
            g_turretStates[slot].inUse = 1;
            return &g_turretStates[slot];
        }
    }

    Com_Error(1,
              COM_ERROR_MARKER
              "G_SpawnTurret: max number of turrets (%d) exceeded",
              TURRET_MAX_COUNT);
    return NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: optional turret sound alias resolver; extracted during reconstruction of 0x5cc27. */
/* VERIFIED_DECOMPILER(0x5cc27, 6cc27_G_SpawnTurret.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; null/empty string zero path and G_SoundAliasIndex byte result match both sustained fire alias fields. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_SpawnTurret (0x5cc27); no standalone original body. */
static uint8_t game_compat_g_turret_sound_alias_for_name(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    return G_SoundAliasIndex(name);
}

/* NOT_FROM_ORIGINAL_SOURCE: spawn/default turret arc parser; extracted during reconstruction of 0x5cc27. */
/* VERIFIED_DECOMPILER(0x5cc27, 6cc27_G_SpawnTurret.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; G_SpawnFloat defaulting, right/top negation and nonpositive clamp, left/bottom nonnegative clamp, and destination stores match. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original G_SpawnTurret (0x5cc27); no standalone original body. */
static void game_compat_g_spawn_turret_arc_float(const char *key, float weaponDefault,
                                  qboolean negate, float *out)
{
    if (G_SpawnFloat(key, "", out) == 0) {
        *out = weaponDefault;
    }

    if (negate) {
        *out = -*out;
        if (*out > 0.0f) {
            *out = 0.0f;
        }
    } else if (*out < 0.0f) {
        *out = 0.0f;
    }
}

/* VERIFIED_DECOMPILER(0x5cc27, 6cc27_G_SpawnTurret.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; state allocation, weapon index/mask/error, precache gate, item register, state defaults, sound aliases, arc parsing, health/damage defaults and clamps, entity flags/contents/type, bounds, origin/angle reset, callbacks, trajectory type, usable byte, and link match. */
void G_SpawnTurret(gentity_t *turret, const char *weaponName)
{
    turret_state_t *turretState = game_compat_g_alloc_turret_state();
    const weaponInfo_t *weaponInfo;
    int weaponIndex;

    turret->turretState = turretState;

    weaponIndex = BG_GetWeaponIndexForName(weaponName);
    turret->s.weapon = weaponIndex & 0xff;
    if (turret->s.weapon == 0) {
        Com_Error(1,
                  COM_ERROR_MARKER
                  "bad weaponinfo '%s' specified for turret",
                  weaponName);
    }

    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(turret->s.weapon);
    if (level.spawning == 0 && !IsItemRegistered(turret->s.weapon)) {
        Scr_Error(va("turret '%s' not precached", weaponName));
    }
    RegisterItem(turret->s.weapon, qtrue);

    turretState->fireTimeRemaining = 0;
    turretState->useMode = weaponInfo->stance;
    turretState->stopUseEventType = TURRET_STOP_USE_EVENT_DISABLED;
    turretState->fireSoundTime = 0;
    turretState->sustainedFireLoopSound =
        game_compat_g_turret_sound_alias_for_name(weaponInfo->loopFireSound);
    turretState->sustainedFireStopSound =
        game_compat_g_turret_sound_alias_for_name(weaponInfo->stopFireSound);

    game_compat_g_spawn_turret_arc_float("rightarc", weaponInfo->turretRightArcDefault,
                          qtrue, &turretState->rightArc);
    game_compat_g_spawn_turret_arc_float("leftarc", weaponInfo->turretLeftArcDefault,
                          qfalse, &turretState->leftArc);
    game_compat_g_spawn_turret_arc_float("toparc", weaponInfo->turretTopArcDefault,
                          qtrue, &turretState->topArc);
    game_compat_g_spawn_turret_arc_float("bottomarc", weaponInfo->turretBottomArcDefault,
                          qfalse, &turretState->bottomArc);
    turretState->restPitch = TURRET_INIT_PITCH_RANGE;

    if (turret->health == 0) {
        turret->health = TURRET_DEFAULT_HEALTH;
    }
    if (G_SpawnInt("damage", "0", &turret->damage) == 0) {
        turret->damage = weaponInfo->flameDamage;
    }
    if (turret->damage < 0) {
        turret->damage = 0;
    }

    turretState->flags = TURRET_STATE_INITIAL_FLAGS;
    turret->clipmask = TURRET_CLIPMASK;
    turret->scriptContents = TURRET_SCRIPT_CONTENTS;
    turret->svFlags = TURRET_SV_FLAGS;
    turret->s.eType = ET_TURRET;
    turret->flags |= TURRET_ENTITY_LINK_USE_FLAG;

    G_DObjUpdate(turret);
    turret->mins[0] = -TURRET_BOUND_XY;
    turret->mins[1] = -TURRET_BOUND_XY;
    turret->mins[2] = 0.0f;
    turret->maxs[0] = TURRET_BOUND_XY;
    turret->maxs[1] = TURRET_BOUND_XY;
    turret->maxs[2] = TURRET_BOUND_Z;
    G_SetOrigin(turret, turret->currentOrigin);
    G_SetAngle(turret, turret->currentAngles);

    turret->s.turret.pitchCarry = 0.0f;
    turret->s.turret.yaw = 0.0f;
    turret->s.turret.pitch = 0.0f;
    turret->think = turret_think_init;
    turret->nextthink = coduo_int32_from_bits(
        (uint32_t)level.time + (uint32_t)MISC_SPAWNER_THINK_MSEC);
    turret->controller = turret_controller;
    turret->use = turret_use;
    turret->s.apos.trType = TR_LINEAR_STOP;
    turret->takeDamage = 1;
    trap_LinkEntity(turret);
}

/* ------------------------------------------------------------------ */
/*  0x5d16b  SP_turret                                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5d16b, 6d16b_SP_turret.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; weaponinfo spawn-string lookup, missing weapon Com_Error, and G_SpawnTurret argument order match. */
void SP_turret(gentity_t *ent)
{
    const char *weaponName;

    if (G_SpawnString("weaponinfo", "", &weaponName) == 0) {
        Com_Error(1, COM_ERROR_MARKER "no weaponinfo specified for turret");
    }
    G_SpawnTurret(ent, weaponName);
}

/* ------------------------------------------------------------------ */
/*  0x5d1ce  misc_spawner_think                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5d1ce, 6d1ce_misc_spawner_think.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; spawnItem string conversion, item lookup, Drop_Item zero arguments, null-drop warning strings, vtos origin argument, and void return match. */
void misc_spawner_think(gentity_t *ent)
{
    const char *spawnItemName = SL_ConvertToString(ent->spawnItem);
    gitem_t *item = BG_FindItem(spawnItemName);
    gentity_t *dropped = Drop_Item(ent, item, 0.0f, 0);

    if (dropped == NULL) {
        G_Printf("-----> WARNING <-------\n");
        G_Printf("misc_spawner used at %s failed to drop!\n",
                 vtos(ent->currentOrigin));
    }
}

/* ------------------------------------------------------------------ */
/*  0x5d265  misc_spawner_use                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5d265, 6d265_misc_spawner_use.c, VERIFY-ENTITY-MGMT-TURRET-2026-06-17): DATAFLOW_VERIFIED; think callback assignment, nextthink level.time+100, link call, and unused other/activator behavior match. */
void misc_spawner_use(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
    (void)other;
    (void)activator;

    ent->think = misc_spawner_think;
    ent->nextthink = coduo_int32_from_bits(
        (uint32_t)level.time + (uint32_t)MISC_SPAWNER_THINK_MSEC);
    trap_LinkEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x5d2af  SP_misc_spawner                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5d2af, 6d2af_SP_misc_spawner.c, VERIFY-P1-ENTITYMGMT-2026-06-17): DATAFLOW_VERIFIED - missing-spawnitem warning path, use callback assignment, and link side effect checked against current decompiler output. */
void SP_misc_spawner(gentity_t *ent)
{
    if (ent->spawnItem == 0) {
        G_Printf("-----> WARNING <-------\n");
        G_Printf("misc_spawner at loc %s has no spawnitem!\n",
                 vtos(ent->currentOrigin));
        return;
    }

    ent->use = misc_spawner_use;
    trap_LinkEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x5d320  miscGunnerEnemyScan                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5d320, 6d320_miscGunnerEnemyScan.c, VERIFY-P1-ENTITYMGMT-2026-06-17): DATAFLOW_VERIFIED - client entity scan bounds, linked/health/radius guards, angle calculation side effect, attacker assignment, and fallthrough checked against current decompiler output. */
void miscGunnerEnemyScan(gentity_t *ent)
{
    gentity_t *candidate = g_entities;
    gentity_t *end = &g_entities[level.maxclients];

    while (candidate < end) {
        /* 0x5d39b..0x5d3b0 compares the float distance with an exact fild of
         * enemyScanRadius and rejects only an ordered distance above it;
         * unordered distances therefore remain eligible. */
        if (candidate->linked != 0 &&
            candidate->health >= 0 &&
            !((long double)VectorDistance(ent->currentOrigin,
                                          candidate->currentOrigin) >
              (long double)ent->enemyScanRadius)) {
            vec3_t delta;
            vec3_t angles;

            delta[0] = candidate->currentOrigin[0] - ent->currentOrigin[0];
            delta[1] = candidate->currentOrigin[1] - ent->currentOrigin[1];
            delta[2] = candidate->currentOrigin[2] - ent->currentOrigin[2];
            vectoangles(delta, angles);
            ent->attacker = candidate;
            return;
        }

        candidate = &candidate[1];
    }
}

/* ------------------------------------------------------------------ */
/*  0x79937  G_SpawnPlayerClone                                       */
/* ------------------------------------------------------------------ */

/*
 * Allocate one of the eight reserved player clone entities.
 *
 * Clone slots occupy entity indexes 64..71. The cursor is advanced modulo 8.
 * If the selected clone is currently in use, it is freed before being
 * reinitialized. The preserved state flag bit is toggled to force clients to
 * observe a fresh clone state on slot reuse.
 */
/* VERIFIED_DECOMPILER(0x79937, 89937_G_SpawnPlayerClone.c, VERIFY-P1-ENTITYMGMT-2026-06-17): DATAFLOW_VERIFIED - clone cursor advance, reserved slot selection, linked-slot free, G_InitGentity call, toggle flag write, and return checked against current decompiler output. */
gentity_t *G_SpawnPlayerClone(void)
{
    int slot = level.playerCloneCursor;
    int nextSlot = slot + 1;
    gentity_t *clone =
        &level.gentities[PLAYER_CLONE_ENTITYNUM_BASE + slot];
    uint32_t oldFlags = clone->s.eFlags;

    level.playerCloneCursor = nextSlot % PLAYER_CLONE_COUNT;

    if (clone->linked != 0) {
        G_FreeEntity(clone);
    }

    G_InitGentity(clone);
    clone->s.eFlags = (oldFlags & PLAYER_CLONE_TOGGLE_S_FLAG) ^
                     PLAYER_CLONE_TOGGLE_S_FLAG;

    return clone;
}

/* ------------------------------------------------------------------ */
/*  0x796ce  G_InitGentity                                            */
/* ------------------------------------------------------------------ */

/*
 * Initialize a newly allocated entity to default state.
 *
 * Sets the entity number, clears classname, sets passEntityNum to
 * ENTITYNUM_NONE, and initializes other default values.
 *
 * RECOVERED(UO-GAME-UNK-0169): Entity number is calculated from pointer
 * arithmetic against g_entities base. The classname is set to an empty
 * string.
 */
/* VERIFIED_DECOMPILER(0x796ce, 896ce_G_InitGentity.c, VERIFY-P1-ENTITYMGMT-2026-06-17): DATAFLOW_VERIFIED - entity index calculation, s_number, linked byte, classname, passEntityNum +0x154, lastThinkTime, skipTypeDispatch, and parentEntityNum stores checked against current decompiler output. */
void G_InitGentity(gentity_t *ent)
{
    int entityIndex;

    ent->linked = 1;

    /* Set classname to empty string */
    Scr_SetString(&ent->scriptClassname, scr_const_noclass);

    /* Calculate entity index from pointer */
    entityIndex = (int)(ent - g_entities);

    /* Set entity number */
    ent->s.number = entityIndex;

    ent->passEntityNum = ENTITYNUM_NONE;

    /* Clear lastThinkTime */
    ent->lastThinkTime = 0;

    /* Clear skipTypeDispatch */
    ent->skipTypeDispatch = 0;

    ent->parentEntityNum = ENTITYNUM_NONE;
}

/* ------------------------------------------------------------------ */
/*  0x79cba  G_TempEntity                                             */
/* ------------------------------------------------------------------ */

/*
 * Create a temporary entity for visual effects.
 *
 * Allocates a new entity, sets its type to ET_EVENTS + event, sets
 * classname to "tempEntity", and initializes timing fields. The entity
 * will be automatically freed after its lifetime expires.
 *
 * RECOVERED(UO-GAME-UNK-0170): Temporary entities use event types offset
 * by the event entity-type base. The classname is set to "tempEntity".
 * Origin coordinates are truncated to integers.
 */
/* VERIFIED_DECOMPILER(0x79cba, 89cba_G_TempEntity.c, VERIFY-FIRST-FUNCTIONS-2026-06-17): DATAFLOW_VERIFIED; spawn, event type, tempEntity classname, timing/skip fields, rounded origin, G_SetOrigin, link, and return checked against current decompiler output. */
gentity_t *G_TempEntity(const float *origin, int event)
{
    gentity_t *ent;
    vec3_t roundedOrigin;

    /* Allocate new entity */
    ent = G_Spawn();

    /* Set entity type to event type */
    ent->s.eType = coduo_int32_from_bits((uint32_t)event +
                                         (uint32_t)ET_EVENTS);

    /* Set classname to "tempEntity" */
    Scr_SetString(&ent->scriptClassname, scr_const_tempEntity);

    /* Set timing fields to current time */
    ent->lastThinkTime = level.time;
    ent->eventTime2 = level.time;

    /* Set skipTypeDispatch flag */
    ent->skipTypeDispatch = 1;

    /* Truncate origin coordinates to integers, matching stock x87 fistp conversion. */
    roundedOrigin[0] = (float)game_compat_int32_from_float_trunc(origin[0]);
    roundedOrigin[1] = (float)game_compat_int32_from_float_trunc(origin[1]);
    roundedOrigin[2] = (float)game_compat_int32_from_float_trunc(origin[2]);

    /* Set entity origin */
    G_SetOrigin(ent, roundedOrigin);

    /* Link entity to world */
    trap_LinkEntity(ent);

    return ent;
}

/* ------------------------------------------------------------------ */
/*  0x5a378  TeleportPlayer                                           */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5a378, 6a378_TeleportPlayer.c, VERIFY-P1-ENTITYMGMT-2026-06-17): DATAFLOW_VERIFIED - temp entities, unlink, origin snap, binary OR launch guard, teleport bit toggle, view angle/entity-state update, and relink guard checked against current decompiler output. */
void TeleportPlayer(gentity_t *ent, const float *origin, const float *angles)
{
    gclient_t *client = ent->client;
    int wasLinked = ent->linkedState;
    gentity_t *temp;

    if (client->sessionState == 0) {
        temp = G_TempEntity(client->ps.psOrigin, TELEPORT_ENTER_EVENT);
        temp->s.clientNum = ent->s.clientNum;

        temp = G_TempEntity(origin, TELEPORT_EXIT_EVENT);
        temp->s.clientNum = ent->s.clientNum;
    }

    trap_UnlinkEntity(ent);

    client->ps.psOrigin[0] = origin[0];
    client->ps.psOrigin[1] = origin[1];
    client->ps.psOrigin[2] = origin[2] + CLIENT_ORIGIN_Z_SNAP_OFFSET;

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    uint32_t noLaunchFlags = TELEPORT_NO_LAUNCH_FLAGS;
    if ((client->ps.entityStateFlags | noLaunchFlags) == 0) {
        AngleVectors(angles, client->ps.velocity, NULL, NULL);
        client->ps.velocity[0] *= TELEPORT_LAUNCH_SPEED;
        client->ps.velocity[1] *= TELEPORT_LAUNCH_SPEED;
        client->ps.velocity[2] *= TELEPORT_LAUNCH_SPEED;
        client->ps.pmTime = TELEPORT_PM_TIME;
        client->ps.playerStateFlags |= TELEPORT_KNOCKBACK_FLAG;
    }

    client->ps.entityStateFlags ^= CLIENT_TELEPORT_BIT;
    SetClientViewAngle(ent, angles);
    BG_PlayerStateToEntityState(&client->ps, &ent->s, qtrue);

    ent->currentOrigin[0] = client->ps.psOrigin[0];
    ent->currentOrigin[1] = client->ps.psOrigin[1];
    ent->currentOrigin[2] = client->ps.psOrigin[2];

    if (wasLinked != 0) {
        trap_LinkEntity(ent);
    }
}
