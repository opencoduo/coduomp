/*
 * Source reconstruction for missile simulation.
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
#include "g_public.h"
#include "game_globals.h"
#include "g_syscalls.h"
#include "game_functions.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */
#include "level_locals.h"
#include "scr_vm.h"
#include "compat/libm/coduo_libm.h"

void G_MissileLandAngles(gentity_t *ent, trace_t *trace, float *angles, int forceAngles);
int G_BounceMissile(gentity_t *ent, trace_t *trace);
void G_MissileImpact(gentity_t *ent, trace_t *trace, float *dir);

#define MISSILE_WATER_CONTENTS CONTENTS_WATER
#define MISSILE_OWNER_TRACE_CONTENTS CONTENTS_BODY
#define MISSILE_CLIPMASK 0x02802091u
#define MISSILE_BOUNCE_FLAGS 0x03000000u
#define MISSILE_LOW_BOUNCE_FLAG 0x02000000u
#define MISSILE_OWNER_LEAVE_BOUNDS_FLAG 0x00002000u
#define MISSILE_VEHICLE_HIT_S_FLAG 0x00100000u
#define GRENADE_BOUNCE_FLAGS 0x03000000u
#define MISSILE_STOP_SPEED 20.0f
#define MISSILE_MIN_MOVE_DISTANCE 0.001f
#define MISSILE_WATER_TRACE_VERTICAL_SPEED 30.0 /* original float64 0x403e000000000000 */
#define MISSILE_GROUND_TRACE_DEPTH 16.0f
#define MISSILE_WATER_SURFACE_TRACE_HEIGHT 500.0f
#define MISSILE_SPLASH_TRACE_OFFSET 10.0f
#define MISSILE_IMPACT_TRACE_OFFSET 4.0f
#define MISSILE_BOUNCE_EVENT_SPEED_DELTA 100.0f
#define GENTITY_OVERLAP_LINKED_ENTITY_FLAG 0x10000u
#define GRENADE_DEFAULT_FUSE_MS 2500
#define GRENADE_OWNER_LEAVE_DELAY_MS 500
#define GRENADE_OWNER_LEAVE_RECHECK_MS 100
#define GRENADE_OWNER_LEAVE_SVFLAGS_MASK 0xfffffff9u
#define MISSILE_EXPLODED_LIFETIME_MS 15000
#define MISSILE_TRAJECTORY_BACKDATE_MS 50
#define MISSILE_DIE_DELAY_MS 10
#define ROCKET_LIFETIME_MS 15000
#define CONCUSSIVE_THINK_INTERVAL_MS 100
#define CONCUSSIVE_LIFETIME_MS 500.0f
#define DYNA_SINK_FREE_INTERVAL_MS 100
#define DYNA_SINK_INTERVAL_MS 50
#define GRENADE_DIE_THINK_MS 100
#define ARTILLERY_DROP_SVFLAGS 0x88u
#define ARTILLERY_PENDING_SVFLAGS 1u
#define ARTILLERY_DROP_LIFETIME_MS 15000
#define ARTILLERY_BARRAGE_THINK_MS 50
#define ARTILLERY_BARRAGE_WARNING_DELAY_MS 250
#define ARTILLERY_BARRAGE_FIRE_DELAY_MS 1750
#define ARTILLERY_FALL_HEIGHT 1000.0f
#define MISSILE_LAUNCH_S_FLAG 0x00008000u
#define PROJECTILE_EXPLOSION_TYPE_SMOKE 1

/* NOT_FROM_ORIGINAL_SOURCE: local vector helper factored from recovered missile bodies. */
static void game_compat_g_missile_vector_copy(const float *src, float *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: local vector helper factored from recovered missile bodies. */
static void game_compat_g_missile_round_vector(float *value)
{
    value[0] = (float)game_compat_int32_from_float_trunc(value[0]);
    value[1] = (float)game_compat_int32_from_float_trunc(value[1]);
    value[2] = (float)game_compat_int32_from_float_trunc(value[2]);
}

/* ------------------------------------------------------------------ */
/*  0x5eb72  G_ExplodeSmokeGrenade                                    */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5eb72, 6eb72_G_ExplodeSmokeGrenade.c, VERIFY-MISSILE-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
void G_ExplodeSmokeGrenade(gentity_t *ent)
{
    trace_t trace;
    vec3_t origin;
    vec3_t end;
    int byteDir;
    const weaponInfo_t *weaponInfo;

    if (ent->passEntityNum == ENTITYNUM_NONE) {
        ent->passEntityNum = ent->parentEntityNum;
    }

    BG_EvaluateTrajectory(&ent->s.pos, level.time, origin);
    origin[0] = (float)game_compat_int32_from_float_trunc(origin[0]);
    origin[1] = (float)game_compat_int32_from_float_trunc(origin[1]);
    origin[2] = (float)game_compat_int32_from_float_trunc(origin[2]);

    ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)MISSILE_EXPLODED_LIFETIME_MS);
    ent->think = G_FreeEntity;

    end[0] = ent->currentOrigin[0];
    end[1] = ent->currentOrigin[1];
    end[2] = ent->currentOrigin[2] - MISSILE_GROUND_TRACE_DEPTH;
    trap_Trace(&trace, ent->currentOrigin, vec3_origin, vec3_origin, end, ent->s.number, MASK_GRENADE_TRACE);

    byteDir = DirToByte(trace.normal);
    G_AddEvent(ent, EV_PROJECTILE_EXPLODE, byteDir);

    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(ent->s.weapon);
    ent->s.hintStringIndex = weaponInfo->projectileExplosionType;

    if (trap_PointContents(ent->currentOrigin, PASS_ENTITY_NONE, MISSILE_WATER_CONTENTS) == 0) {
        ent->s.surfType = (trace.surfaceFlags & (SURFACE_TYPE_MASK << SURFACE_TYPE_SHIFT)) >> SURFACE_TYPE_SHIFT;
    } else {
        ent->s.surfType = SURFACE_TYPE_WATER;
    }

    trap_LinkEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x5edae  G_MissileTrace                                           */
/* ------------------------------------------------------------------ */

/*
 * Perform a trace for missile movement.
 *
 * Wrapper around trap_LocationalTrace that handles special cases:
 * - If startsolid and trace hit something, reset fraction to 0 and
 *   compute normalized direction from start to end
 * - If startsolid but trace didn't hit anything, set fraction to 1.0
 *
 * RECOVERED(UO-GAME-UNK-0151): The trace result structure layout is
 * recovered. Field names/types are inferred from usage patterns.
 */
/* VERIFIED_DECOMPILER(0x5edae, 6edae_G_MissileTrace.c, VERIFY-MISSILE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void G_MissileTrace(trace_t *trace, float *start, float *end, int passEntityNum, int contentMask)
{
    vec3_t dir;

    trap_LocationalTrace(trace, start, end, passEntityNum, contentMask, bulletPriorityMap);

    if (trace->startsolid == 0 || (trace->contents & CONTENTS_SKY) == 0) {
        if (trace->startsolid != 0) {
            trace->fraction = 0.0f;
            dir[0] = start[0] - end[0];
            dir[1] = start[1] - end[1];
            dir[2] = start[2] - end[2];
            VectorNormalize2(dir, trace->normal);
        }
    } else {
        trace->startsolid = 0;
        trace->fraction = 1.0f;
    }
}

/* ------------------------------------------------------------------ */
/*  0x5ee7d  G_RunMissile                                             */
/* ------------------------------------------------------------------ */

/*
 * Per-frame update for missile entities (type 4).
 *
 * Simulates missile movement by:
 *  1. Pre-trace: adjust Z by -1.5 and trace to detect immediate collision
 *  2. Evaluate trajectory to get new position
 *  3. Compute movement direction and normalize
 *  4. Perform main trace (with water splash check if needed)
 *  5. Handle surface effects (splashes, temp entities)
 *  6. Adjust trace normal for bounce flag
 *  7. Special grenade-vs-linked-entity handling
 *  8. Update position and link entity
 *  9. Handle impact or ground touch
 * 10. Call G_RunThink for think function execution
 *
 * RECOVERED(UO-GAME-UNK-0152): Many field offsets in the entity struct
 * are used without clear semantic names. The trajectory data is at
 * ent+0x0c (12 bytes), currentOrigin at +0x13c, and various missile-
 * specific fields are scattered throughout the entity struct.
 */
/* VERIFIED_DECOMPILER(0x5ee7d, 6ee7d_G_RunMissile.c, VERIFY-MISSILE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void G_RunMissile(gentity_t *ent)
{
    trace_t trace;
    vec3_t startOrigin;
    vec3_t endOrigin;
    vec3_t dir;
    float magnitude;
    weaponInfo_t *weaponInfo;
    gentity_t *tempEnt;
    int byteDir;

    /* Save current origin */
    startOrigin[0] = ent->currentOrigin[0];
    startOrigin[1] = ent->currentOrigin[1];
    startOrigin[2] = ent->currentOrigin[2];

    /* Pre-trace: adjust Z down by 1.5 and check for immediate collision */
    if (ent->s.pos.trType == TR_STATIONARY && ent->s.groundEntityNum != ENTITYNUM_WORLD) {
        endOrigin[0] = ent->currentOrigin[0];
        endOrigin[1] = ent->currentOrigin[1];
        endOrigin[2] = ent->currentOrigin[2] - 1.5f;
        G_MissileTrace(&trace, ent->currentOrigin, endOrigin, ent->passEntityNum, ent->clipmask);
        if (trace.fraction == 1.0f) {
            ent->s.pos.trType = TR_GRAVITY;
            ent->s.pos.trTime = level.time;
            ent->s.pos.trDuration = 0;
            ent->s.pos.trBase[0] = startOrigin[0];
            ent->s.pos.trBase[1] = startOrigin[1];
            ent->s.pos.trBase[2] = startOrigin[2];
            ent->s.pos.trDelta[0] = 0.0f;
            ent->s.pos.trDelta[1] = 0.0f;
            ent->s.pos.trDelta[2] = 0.0f;
        }
    }

    /* Save pre-trajectory origin */
    startOrigin[0] = ent->currentOrigin[0];
    startOrigin[1] = ent->currentOrigin[1];
    startOrigin[2] = ent->currentOrigin[2];

    /* Evaluate trajectory to get new position */
    BG_EvaluateTrajectory(&ent->s.pos, level.time, endOrigin);

    /* Compute movement direction */
    dir[0] = endOrigin[0] - ent->currentOrigin[0];
    dir[1] = endOrigin[1] - ent->currentOrigin[1];
    dir[2] = endOrigin[2] - ent->currentOrigin[2];
    magnitude = VectorNormalize(dir);

    /* Early exit if no movement */
    if (magnitude < MISSILE_MIN_MOVE_DISTANCE) {
        G_RunThink(ent);
        return;
    }

    /* Main trace: check for water splash if velocity is low */
    if (!(fabsf(ent->s.pos.trDelta[2]) > MISSILE_WATER_TRACE_VERTICAL_SPEED) ||
        trap_PointContents(ent->currentOrigin, PASS_ENTITY_NONE, MISSILE_WATER_CONTENTS) != 0) {
        G_MissileTrace(&trace, ent->currentOrigin, endOrigin, ent->passEntityNum, ent->clipmask);
    } else {
        G_MissileTrace(&trace, ent->currentOrigin, endOrigin, ent->passEntityNum, ent->clipmask | MISSILE_WATER_CONTENTS);
    }

    /* Surface effects: create splash temp entity if hit surface */
    if ((trace.surfaceFlags & (SURFACE_TYPE_MASK << SURFACE_TYPE_SHIFT)) == (SURFACE_TYPE_WATER << SURFACE_TYPE_SHIFT)) {
        vec3_t normal;

        VectorNormalize2(ent->s.pos.trDelta, normal);
        if (normal[2] < 0.0f) {
            normal[2] = -normal[2];
        }

        tempEnt = G_TempEntity(ent->currentOrigin, EV_BULLET_HIT);
        byteDir = DirToByte(trace.normal);
        tempEnt->s.tempEffectId = byteDir & 0xff;
        byteDir = DirToByte(normal);
        tempEnt->s.hintStringIndex = byteDir & 0xff;
        tempEnt->s.surfType = (trace.surfaceFlags & (SURFACE_TYPE_MASK << SURFACE_TYPE_SHIFT)) >> SURFACE_TYPE_SHIFT;
        tempEnt->s.weapon = ent->s.weapon;
        tempEnt->s.vehicleEntityNum = ent->s.number;

        weaponInfo = (weaponInfo_t *)BG_GetInfoForWeapon(ent->s.weapon);
        if (weaponInfo->projectileExplosionType == PROJECTILE_EXPLOSION_TYPE_SMOKE) {
            G_FreeEntity(ent);
            return;
        }

        G_MissileTrace(&trace, ent->currentOrigin, endOrigin, ent->passEntityNum, ent->clipmask);
    }

    /* Original G_RunMissile 0x5f24e..0x5f284 nudges sky-surface hits by
     * adding trace.normal * -2.0 to trace.endpos before currentOrigin is
     * copied. This lets falling artillery continue below sky brush faces. */
    if ((trace.surfaceFlags & SURF_SKY) != 0) {
        trace.endpos[0] = trace.endpos[0] + trace.normal[0] * -2.0f;
        trace.endpos[1] = trace.endpos[1] + trace.normal[1] * -2.0f;
        trace.endpos[2] = trace.endpos[2] + trace.normal[2] * -2.0f;
    }

    /* Special grenade-vs-linked-entity handling */
    if (ent->methodOfDeath == MOD_GRENADE) {
        gentity_t *hitEnt = &g_entities[trace.entityNum];
        if ((hitEnt->flags & GENTITY_OVERLAP_LINKED_ENTITY_FLAG) != 0) {
            int savedContents = hitEnt->scriptContents;
            hitEnt->scriptContents = 0;
            G_MissileTrace(&trace, ent->currentOrigin, endOrigin, ent->passEntityNum, ent->clipmask);
            hitEnt->scriptContents = savedContents;
        }
    }

    /* Update position */
    ent->currentOrigin[0] = trace.endpos[0];
    ent->currentOrigin[1] = trace.endpos[1];
    ent->currentOrigin[2] = trace.endpos[2];

    if (trace.startsolid != 0) {
        trace.fraction = 0.0f;
    }

    /* Special handling for certain entity flags */
    if (((ent->s.eFlags & MISSILE_BOUNCE_FLAGS) != 0) && (trace.fraction == 1.0f || (trace.fraction < 1.0f && trace.normal[2] > 0.7f))) {
        trace_t trace2;
        endOrigin[0] = ent->currentOrigin[0];
        endOrigin[1] = ent->currentOrigin[1];
        endOrigin[2] = ent->currentOrigin[2] - 1.5f;
        G_MissileTrace(&trace2, ent->currentOrigin, endOrigin, ent->passEntityNum, ent->clipmask);
        if (trace2.fraction != 1.0f && trace2.entityNum == ENTITYNUM_WORLD) {
            trace = trace2;
            /* Adjust entity position based on trace result */
#if EMULATE_X87
            ent->s.pos.trBase[2] = x87f_store_f32(
                x87f_add(x87f_load_f32(ent->s.pos.trBase[2]),
                         x87f_sub(x87f_add(x87f_load_f32(trace.endpos[2]), x87f_load_f32(1.5f)), x87f_load_f32(ent->currentOrigin[2]))));
#else
            ent->s.pos.trBase[2] += (trace.endpos[2] + 1.5f) - ent->currentOrigin[2];
#endif
            ent->currentOrigin[0] = trace.endpos[0];
            ent->currentOrigin[1] = trace.endpos[1];
            ent->currentOrigin[2] = trace.endpos[2];
            ent->currentOrigin[2] = ent->currentOrigin[2] + 1.5f;
        }
    }

    trap_LinkEntity(ent);

    /* Grenade trigger damage */
    if (ent->methodOfDeath == MOD_GRENADE) {
        G_GrenadeTouchTriggerDamage(ent, startOrigin, ent->currentOrigin, ent->splashDamage, ent->methodOfDeath);
    }

    /* Impact handling */
    if (trace.fraction != 1.0f) {
        if ((trace.surfaceFlags & SURF_SKY) == 0) {
            if ((trace.surfaceFlags & SURF_NOIMPACT) != 0) {
                G_FreeEntity(ent);
                return;
            }
            G_MissileImpact(ent, &trace, dir);
            if (ent->s.eType != ET_MISSILE) {
                return;
            }
        }
    } else {
        float velocityMag;
#if EMULATE_X87
        velocityMag = (float)CoduoLibm_Sqrt(
            x87f_store_f64(x87f_add(x87f_add(x87f_mul(x87f_load_f32(ent->s.pos.trDelta[0]), x87f_load_f32(ent->s.pos.trDelta[0])),
                                             x87f_mul(x87f_load_f32(ent->s.pos.trDelta[1]), x87f_load_f32(ent->s.pos.trDelta[1]))),
                                    x87f_mul(x87f_load_f32(ent->s.pos.trDelta[2]), x87f_load_f32(ent->s.pos.trDelta[2])))));
#else
        velocityMag =
            (float)CoduoLibm_Sqrt((double)(ent->s.pos.trDelta[0] * ent->s.pos.trDelta[0] + ent->s.pos.trDelta[1] * ent->s.pos.trDelta[1] +
                                           ent->s.pos.trDelta[2] * ent->s.pos.trDelta[2]));
#endif
        if (velocityMag != 0.0f) {
            ent->s.groundEntityNum = ENTITYNUM_NONE;
        }
    }

    G_RunThink(ent);
}

/* ------------------------------------------------------------------ */
/*  0x5d444  G_MissileLandAngles                                      */
/* ------------------------------------------------------------------ */

/* NOT_FROM_ORIGINAL_SOURCE: helper extracted from the machine-code-backed
 * impact-time expression in G_MissileLandAngles. */
static int game_compat_g_missile_trace_time(const trace_t *trace)
{
    int32_t frameMsec = coduo_int32_from_bits((uint32_t)level.time - (uint32_t)level.previousTime);
    int32_t elapsed;

#if EMULATE_X87
    /* (frameMsec * fraction) in x87 width (long double is 64-bit on arm64),
     * truncated to int, then + previousTime. */
    elapsed = x87f_store_i32_trunc(x87f_mul(x87f_load_i32(frameMsec), x87f_load_f32(trace->fraction)));
#else
    elapsed = game_compat_int32_from_long_double_trunc((long double)frameMsec * (long double)trace->fraction);
#endif

    return coduo_int32_from_bits((uint32_t)elapsed + (uint32_t)level.previousTime);
}

/* VERIFIED_DECOMPILER(0x29304, 29304_G_MissileLandAngles.c, VERIFY-MISSILE-REMAINING-2026-06-17): DATAFLOW_VERIFIED - generated file is a self-call thunk/alias; implementation body verified against 6d444_G_MissileLandAngles.c. */
void G_MissileLandAngles(gentity_t *ent, trace_t *trace, float *angles, int forceAngles)
{
    int impactTime;

    impactTime = game_compat_g_missile_trace_time(trace);
    BG_EvaluateTrajectory(&ent->s.apos, impactTime, angles);

    if (trace->normal[2] > 0.1f) {
        float landPitch;
        float pitchDelta;
        float pitchDeltaAbs;

        landPitch = PitchForYawOnNormal(angles[1], trace->normal);
        pitchDelta = AngleSubtract(landPitch, angles[0]);
        pitchDeltaAbs = fabsf(pitchDelta);

        if (forceAngles == 0) {
            ent->s.apos.trBase[0] = angles[0];
            ent->s.apos.trBase[1] = angles[1];
            ent->s.apos.trBase[2] = angles[2];
            ent->s.apos.trTime = impactTime;
            /* 0x5d5a3/0x5d5de: the 0.3/0.85 constants are DWORD float
             * loads (.rodata 0xa1778 = 0.3f, 0xa177c = 0.85f), not long
             * double literals; the arithmetic itself stays 80-bit. */
            if (pitchDeltaAbs < 80.0f) {
#if EMULATE_X87
                /* rand/2^31 is exact; *0.3f + 0.85f and the trDelta multiply
                 * stay 80-bit (0.3/0.85 are DWORD float loads). */
                double r = coduo_server_rand_unit();
                ent->s.apos.trDelta[0] =
                    x87f_store_f32(x87f_mul(x87f_load_f32(ent->s.apos.trDelta[0]),
                                            x87f_neg(x87f_add(x87f_mul(x87f_load_f64(r), x87f_load_f32(0.3f)), x87f_load_f32(0.85f)))));
#else
                ent->s.apos.trDelta[0] =
                    (float)((long double)ent->s.apos.trDelta[0] * -((long double)coduo_server_rand_unit() * 0.3f + 0.85f));
#endif
            } else {
#if EMULATE_X87
                double r = coduo_server_rand_unit();
                ent->s.apos.trDelta[0] =
                    x87f_store_f32(x87f_mul(x87f_load_f32(ent->s.apos.trDelta[0]),
                                            x87f_add(x87f_mul(x87f_load_f64(r), x87f_load_f32(0.3f)), x87f_load_f32(0.85f))));
#else
                ent->s.apos.trDelta[0] =
                    (float)((long double)ent->s.apos.trDelta[0] * ((long double)coduo_server_rand_unit() * 0.3f + 0.85f));
#endif
            }
        }

        angles[0] = AngleNormalize180(angles[0]);
        if (forceAngles != 0 || pitchDeltaAbs < 45.0f) {
            if (fabsf(angles[0]) > 90.0f) {
                angles[0] = AngleNormalize360(landPitch + 180.0f);
            } else {
                angles[0] = AngleNormalize360(landPitch);
            }
        } else if (pitchDeltaAbs < 80.0f) {
            angles[0] = AngleNormalize360(angles[0] + pitchDelta * 0.25f);
        } else {
            angles[0] = AngleNormalize360(angles[0]);
        }
    } else if (forceAngles == 0) {
        /* 0x5d6c8: fild feeds the add directly; the only rounding is the
         * float argument store (value-identical either way: the int is
         * 7-bit, but keep the structure faithful). */
        ent->s.apos.trDelta[0] = AngleNormalize360((long double)((coduo_server_rand() & 0x7f) - 0x3f) + ent->s.apos.trDelta[0]);
    }
}

/* ------------------------------------------------------------------ */
/*  0x5d6e5  G_BounceMissile                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5d6e5, 6d6e5_G_BounceMissile.c, VERIFY-MISSILE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
int G_BounceMissile(gentity_t *ent, trace_t *trace)
{
    vec3_t velocity;
    vec3_t delta;
    float dot;
    vec3_t nudge;
    int pointContents;
    int impactTime;

    pointContents = trap_PointContents(ent->currentOrigin, PASS_ENTITY_NONE, MISSILE_WATER_CONTENTS);
    impactTime = game_compat_g_missile_trace_time(trace);
    BG_EvaluateTrajectoryDelta(&ent->s.pos, impactTime, velocity);

#if EMULATE_X87
    dot = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(velocity[0]), x87f_load_f32(trace->normal[0])),
                                           x87f_mul(x87f_load_f32(velocity[1]), x87f_load_f32(trace->normal[1]))),
                                  x87f_mul(x87f_load_f32(velocity[2]), x87f_load_f32(trace->normal[2]))));
    for (int i = 0; i < 3; i++) {
        ent->s.pos.trDelta[i] = x87f_store_f32(x87f_sub(
            x87f_load_f32(velocity[i]), x87f_mul(x87f_mul(x87f_load_f32(dot), x87f_load_f32(2.0f)), x87f_load_f32(trace->normal[i]))));
    }
#else
    dot = velocity[0] * trace->normal[0] + velocity[1] * trace->normal[1] + velocity[2] * trace->normal[2];

    ent->s.pos.trDelta[0] = velocity[0] - dot * 2.0f * trace->normal[0];
    ent->s.pos.trDelta[1] = velocity[1] - dot * 2.0f * trace->normal[1];
    ent->s.pos.trDelta[2] = velocity[2] - dot * 2.0f * trace->normal[2];
#endif

    if (trace->normal[2] > 0.7f) {
        ent->s.groundEntityNum = trace->entityNum;
    }

    if ((ent->s.eFlags & MISSILE_LOW_BOUNCE_FLAG) != 0) {
        float scale;

        if (pointContents == 0 && (trace->contents & CONTENTS_BODY) == 0) {
            scale = 0.4f;
        } else {
            scale = 0.1f;
        }
        ent->s.pos.trDelta[0] *= scale;
        ent->s.pos.trDelta[1] *= scale;
        ent->s.pos.trDelta[2] *= scale;

        /* 0x5d91c: this second compare loads a QWORD double 0.7 (.rodata
         * 0xa17a8), unlike the float 0.7f compare at 0x5d804 above. */
        if (trace->normal[2] > 0.7) {
            float speed;

#if EMULATE_X87
            speed = (float)CoduoLibm_Sqrt(
                x87f_store_f64(x87f_add(x87f_add(x87f_mul(x87f_load_f32(ent->s.pos.trDelta[0]), x87f_load_f32(ent->s.pos.trDelta[0])),
                                                 x87f_mul(x87f_load_f32(ent->s.pos.trDelta[1]), x87f_load_f32(ent->s.pos.trDelta[1]))),
                                        x87f_mul(x87f_load_f32(ent->s.pos.trDelta[2]), x87f_load_f32(ent->s.pos.trDelta[2])))));
#else
            speed = (float)CoduoLibm_Sqrt((double)(ent->s.pos.trDelta[0] * ent->s.pos.trDelta[0] +
                                                   ent->s.pos.trDelta[1] * ent->s.pos.trDelta[1] +
                                                   ent->s.pos.trDelta[2] * ent->s.pos.trDelta[2]));
#endif
            if (speed < MISSILE_STOP_SPEED) {
                vec3_t angles;

                G_SetOrigin(ent, ent->currentOrigin);
                G_MissileLandAngles(ent, trace, angles, qtrue);
                G_SetAngle(ent, angles);
                return qfalse;
            }
        }
    }

    nudge[0] = trace->normal[0] * 0.1f;
    nudge[1] = trace->normal[1] * 0.1f;
    nudge[2] = trace->normal[2] * 0.1f;
    if (nudge[2] > 0.0f) {
        nudge[2] = 0.0f;
    }

    ent->currentOrigin[0] += nudge[0];
    ent->currentOrigin[1] += nudge[1];
    ent->currentOrigin[2] += nudge[2];
    ent->s.pos.trBase[0] = ent->currentOrigin[0];
    ent->s.pos.trBase[1] = ent->currentOrigin[1];
    ent->s.pos.trBase[2] = ent->currentOrigin[2];
    ent->s.pos.trTime = level.time;

    {
        vec3_t landAngles;

        G_MissileLandAngles(ent, trace, landAngles, qfalse);
        ent->s.apos.trBase[0] = landAngles[0];
        ent->s.apos.trBase[1] = landAngles[1];
        ent->s.apos.trBase[2] = landAngles[2];
    }
    ent->s.apos.trTime = level.time;

    if (pointContents == 0) {
        delta[0] = ent->s.pos.trDelta[0] - velocity[0];
        delta[1] = ent->s.pos.trDelta[1] - velocity[1];
        delta[2] = ent->s.pos.trDelta[2] - velocity[2];
#if EMULATE_X87
        if ((float)CoduoLibm_Sqrt(x87f_store_f64(x87f_add(x87f_add(x87f_mul(x87f_load_f32(delta[0]), x87f_load_f32(delta[0])),
                                                                   x87f_mul(x87f_load_f32(delta[1]), x87f_load_f32(delta[1]))),
                                                          x87f_mul(x87f_load_f32(delta[2]), x87f_load_f32(delta[2]))))) >
            MISSILE_BOUNCE_EVENT_SPEED_DELTA) {
            return qtrue;
        }
#else
        if ((float)CoduoLibm_Sqrt((double)(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2])) >
            MISSILE_BOUNCE_EVENT_SPEED_DELTA) {
            return qtrue;
        }
#endif
    }

    return qfalse;
}

/* ------------------------------------------------------------------ */
/*  0x5db67  G_MissileImpact                                          */
/* ------------------------------------------------------------------ */

/*
 * Handle missile collision with world or entity.
 *
 * Processes missile impact including:
 *  - Bounce logic for non-damage missiles
 *  - Damage calculation with falloff
 *  - Radius damage for explosive missiles
 *  - Impact events and effects
 *  - Hit tracking for accuracy
 *
 * RECOVERED(UO-GAME-UNK-0157): The trace result structure layout is
 * recovered. Field names/types are inferred from missile usage patterns.
 */
void G_Damage(gentity_t *target, gentity_t *inflictor, gentity_t *attacker, const float *dir, const float *point, int damage, int flags,
              int meansOfDeath, int hitLocation);
void G_CheckHitTriggerDamage(gentity_t *trigger, const float *start, const float *end, int damage, int mod);

/* VERIFIED_DECOMPILER(0x5db67, 6db67_G_MissileImpact.c, VERIFY-MISSILE-PACKET-2026-06-17): DATAFLOW_VERIFIED - 27ee4_G_MissileImpact.c is the PLT/import thunk to this body. */
void G_MissileImpact(gentity_t *ent, trace_t *trace, float *dir)
{
    (void)dir; /* Parameter not used in original code */
    weaponInfo_t *weaponInfo;
    gentity_t *other;
    gentity_t *inflictor;
    gentity_t *attacker;
    float distance;
    float damage;
    vec3_t velocity;
    float speed;
    int hitLogged;
    int impactEvent;
    int surfaceType;
    int isVehicle;

    hitLogged = 0;
    weaponInfo = (weaponInfo_t *)BG_GetInfoForWeapon(ent->s.weapon);
    other = &g_entities[trace->entityNum];
    inflictor = NULL;
    attacker = NULL;

    /* Handle non-damage missile (bounce only) */
    if (!game_compat_gentity_can_take_damage(other) && (ent->s.eFlags & MISSILE_BOUNCE_FLAGS) != 0) {
        if (G_BounceMissile(ent, trace) != qfalse && trace->startsolid == 0 && ent->scriptClassname != scr_const_no_bounce_missile) {
            if (ent->scriptClassname == scr_const_flamebarrel) {
                G_AddEvent(ent, EV_FLAMEBARREL_BOUNCE, 0);
            } else {
                G_AddEvent(ent, EV_GRENADE_BOUNCE, (trace->surfaceFlags & (SURFACE_TYPE_MASK << SURFACE_TYPE_SHIFT)) >> SURFACE_TYPE_SHIFT);
            }
        }
        return;
    }

    /* Determine inflictor and attacker entities */
    if (ent->passEntityNum != ENTITYNUM_NONE) {
        gentity_t *parent = &g_entities[ent->passEntityNum];
        inflictor = ent;
        if (parent->s.eType == ET_TURRET && parent->passEntityNum != ENTITYNUM_NONE) {
            inflictor = parent;
            attacker = &g_entities[parent->passEntityNum];
        } else {
            attacker = parent;
            if (parent->s.eType == ET_VEHICLE && ent->parentEntityNum != ENTITYNUM_NONE) {
                attacker = &g_entities[ent->parentEntityNum];
                inflictor = parent;
            }
        }
    }

    /* Calculate damage with falloff */
    if (game_compat_gentity_can_take_damage(other)) {
        if (ent->damage == 0) {
            G_BounceMissile(ent, trace);
            return;
        }
        distance = VectorDistance(ent->s.pos.trBase, ent->currentOrigin);
        if (weaponInfo == NULL) {
            damage = (float)ent->damage;
        } else {
            damage = Damage_Falloff(distance, (float)ent->damage, (float)weaponInfo->damageFalloffMinDamagePercent,
                                    weaponInfo->damageFalloffMinRange, weaponInfo->damageFalloffMaxRange);
        }
        if (damage > 0.0f) {
            gentity_t *accuracyAttacker = &g_entities[ent->passEntityNum];

            if (LogAccuracyHit(other, accuracyAttacker) != 0) {
                hitLogged = 1;
            }
            BG_EvaluateTrajectoryDelta(&ent->s.pos, level.time, velocity);
#if EMULATE_X87
            speed =
                (float)CoduoLibm_Sqrt(x87f_store_f64(x87f_add(x87f_add(x87f_mul(x87f_load_f32(velocity[0]), x87f_load_f32(velocity[0])),
                                                                       x87f_mul(x87f_load_f32(velocity[1]), x87f_load_f32(velocity[1]))),
                                                              x87f_mul(x87f_load_f32(velocity[2]), x87f_load_f32(velocity[2])))));
#else
            speed = (float)CoduoLibm_Sqrt((double)(velocity[0] * velocity[0] + velocity[1] * velocity[1] + velocity[2] * velocity[2]));
#endif
            if (speed == 0.0f) {
                velocity[2] = 1.0f;
            }
            G_Damage(other, inflictor, attacker, velocity, ent->currentOrigin, game_compat_int32_from_float_trunc(damage), 0,
                     ent->methodOfDeath, 0);
        }
    }

    /* Check for trigger damage */
    if (ent->damage != 0) {
        gentity_t *trigger = attacker;
        if (attacker == NULL) {
            trigger = &g_entities[ENTITYNUM_WORLD];
        }
        G_CheckHitTriggerDamage(trigger, ent->currentOrigin, trace->endpos, ent->damage, ent->methodOfDeath);
    }

    /* Determine impact event type */
    isVehicle = (hitLogged != 0 || trace->partName != 0) ? 1 : 0;
    impactEvent = DirToByte(trace->normal);
    if (isVehicle == 0) {
        G_AddEvent(ent, EV_PROJECTILE_EXPLODE, impactEvent);
    } else {
        G_AddEvent(ent, EV_PROJECTILE_EXPLODE_NOMARKS, impactEvent);
    }
    ent->skipTypeDispatch = 1;

    /* Determine surface type */
    surfaceType = trap_PointContents(ent->currentOrigin, PASS_ENTITY_NONE, MISSILE_WATER_CONTENTS);
    if (surfaceType == 0) {
        ent->s.surfType = (trace->surfaceFlags & (SURFACE_TYPE_MASK << SURFACE_TYPE_SHIFT)) >> SURFACE_TYPE_SHIFT;
    } else {
        ent->s.surfType = SURFACE_TYPE_WATER;
    }

    /* Store weapon-specific data */
    weaponInfo = (weaponInfo_t *)BG_GetInfoForWeapon(ent->s.weapon);
    ent->s.hintStringIndex = weaponInfo->projectileExplosionType;

    /* Mark if hit a vehicle */
    if (other != NULL && other->s.eType == ET_VEHICLE) {
        ent->s.eFlags |= MISSILE_VEHICLE_HIT_S_FLAG;
    }

    /* Update position and handle radius damage */
    ent->s.eType = ET_GENERAL;
    SnapVectorTowards(trace->endpos, ent->s.pos.trBase);
    G_SetOrigin(ent, trace->endpos);

    if (ent->splashDamage != 0) {
        vec3_t splashOrigin;
        vec3_t splashEnd;
        gentity_t *splashInflictor;

        splashOrigin[0] = trace->endpos[0];
        splashOrigin[1] = trace->endpos[1];
        splashOrigin[2] = trace->endpos[2];
        splashEnd[0] = trace->endpos[0] + trace->normal[0] * MISSILE_IMPACT_TRACE_OFFSET;
        splashEnd[1] = trace->endpos[1] + trace->normal[1] * MISSILE_IMPACT_TRACE_OFFSET;
        splashEnd[2] = trace->endpos[2] + trace->normal[2] * MISSILE_IMPACT_TRACE_OFFSET;

        trap_Trace(trace, splashOrigin, vec3_origin, vec3_origin, splashEnd, ent->s.number, MASK_GRENADE_TRACE);

        splashInflictor = inflictor;
        if (inflictor == NULL) {
            splashInflictor = ent;
        }
        G_RadiusDamage(trace->endpos, splashInflictor, attacker, (float)ent->splashDamage, (float)ent->splashMinDamage,
                       (float)ent->splashRadius, other, ent->splashMethodOfDeath);
    }

    trap_LinkEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x5e1c8  Concussive_think                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5e1c8, 6e1c8_Concussive_think.c, VERIFY-MISSILE-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
void Concussive_think(gentity_t *ent)
{
    /* 0x5e1dd: level.time is fild'd straight into the compare with no
     * float-rounding store, so no (float) cast of level.time here. */
    if (ent->concussiveFxEndTime < (long double)level.time) {
        ent->think = G_FreeEntity;
    }
    ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)CONCUSSIVE_THINK_INTERVAL_MS);
}

/* ------------------------------------------------------------------ */
/*  0x5e221  Concussive_fx                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5e221, 6e221_Concussive_fx.c, VERIFY-MISSILE-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
void Concussive_fx(float *origin)
{
    gentity_t *ent;

    ent = G_Spawn();
    ent->currentOrigin[0] = origin[0];
    ent->currentOrigin[1] = origin[1];
    ent->currentOrigin[2] = origin[2];
    ent->think = Concussive_think;
    ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)CONCUSSIVE_THINK_INTERVAL_MS);
    /* 0x5e29b: fild level.time feeds the add directly (one rounding at
     * the final store), so level.time is not rounded to float first. */
    ent->concussiveFxEndTime = (float)((long double)level.time + CONCUSSIVE_LIFETIME_MS);
}

/* ------------------------------------------------------------------ */
/*  0x5e2b5  G_ExplodeMissile                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5e2b5, 6e2b5_G_ExplodeMissile.c, VERIFY-MISSILE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void G_ExplodeMissile(gentity_t *ent)
{
    gentity_t *attacker;
    trace_t trace;
    vec3_t origin;
    vec3_t end;
    vec3_t splashEnd;
    int byteDir;
    const weaponInfo_t *weaponInfo;

    attacker = NULL;
    if (ent->passEntityNum == ENTITYNUM_NONE) {
        ent->passEntityNum = ent->parentEntityNum;
    }
    attacker = ent->entityRef;
    if (attacker != NULL && attacker->s.eType == ET_TURRET && attacker->passEntityNum != ENTITYNUM_NONE) {
        attacker = &g_entities[attacker->passEntityNum];
    }

    BG_EvaluateTrajectory(&ent->s.pos, level.time, origin);
    origin[0] = (float)game_compat_int32_from_float_trunc(origin[0]);
    origin[1] = (float)game_compat_int32_from_float_trunc(origin[1]);
    origin[2] = (float)game_compat_int32_from_float_trunc(origin[2]);
    G_SetOrigin(ent, origin);

    ent->s.eType = ET_GENERAL;
    ent->s.eFlags |= EF_NODRAW;
    ent->flags |= FL_NOCLIENT;
    ent->svFlags |= SVF_LOOPED_FX;
    ent->scriptContents = 0;

    if (ent->scriptClassname == scr_const_flamebarrel) {
        ent->skipTypeDispatch = 1;
        trap_LinkEntity(ent);
        return;
    }

    end[0] = ent->currentOrigin[0];
    end[1] = ent->currentOrigin[1];
    end[2] = ent->currentOrigin[2] - MISSILE_GROUND_TRACE_DEPTH;
    trap_Trace(&trace, ent->currentOrigin, vec3_origin, vec3_origin, end, ent->s.number, MASK_GRENADE_TRACE);

    byteDir = DirToByte(trace.normal);
    G_AddEvent(ent, EV_PROJECTILE_EXPLODE, byteDir);

    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(ent->s.weapon);
    ent->s.hintStringIndex = weaponInfo->projectileExplosionType;

    if (trap_PointContents(ent->currentOrigin, PASS_ENTITY_NONE, MISSILE_WATER_CONTENTS) == 0) {
        ent->s.surfType = (trace.surfaceFlags & (SURFACE_TYPE_MASK << SURFACE_TYPE_SHIFT)) >> SURFACE_TYPE_SHIFT;
    } else {
        splashEnd[0] = ent->currentOrigin[0];
        splashEnd[1] = ent->currentOrigin[1];
        splashEnd[2] = ent->currentOrigin[2] + MISSILE_WATER_SURFACE_TRACE_HEIGHT;
        trap_Trace(&trace, splashEnd, vec3_origin, vec3_origin, end, ent->s.number, MISSILE_WATER_CONTENTS);
        if (trace.fraction < 1.0f) {
            G_SetOrigin(ent, trace.endpos);
        }
        ent->s.surfType = SURFACE_TYPE_WATER;
    }

    ent->skipTypeDispatch = 1;
    if (ent->splashDamage != 0) {
        /* 0x5e6d0..0x5e700: the original adds a (0, 0, 10) offset vector
         * component-wise, storing an fadd result for all three components;
         * the + 0.0f adds are kept because they normalize -0.0 to +0.0. */
        splashEnd[0] = ent->currentOrigin[0] + 0.0f;
        splashEnd[1] = ent->currentOrigin[1] + 0.0f;
        splashEnd[2] = ent->currentOrigin[2] + MISSILE_SPLASH_TRACE_OFFSET;
        trap_Trace(&trace, ent->currentOrigin, vec3_origin, vec3_origin, splashEnd, ent->s.number, MASK_GRENADE_TRACE);
        G_RadiusDamage(trace.endpos, ent, attacker, (float)ent->splashDamage, (float)ent->splashMinDamage, (float)ent->splashRadius, ent,
                       ent->splashMethodOfDeath);
    }

    trap_LinkEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x5e7b5  G_BarrageThink                                           */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5e7b5, 6e7b5_G_BarrageThink.c, VERIFY-MISSILE-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
void G_BarrageThink(gentity_t *ent)
{
    const weaponInfo_t *weaponInfo;
    gentity_t *tempEnt;
    vec3_t angles;
    vec3_t forward;
    vec3_t origin;
    int delay;

    angles[0] = 0.0f;
    angles[1] = 0.0f;
    angles[2] = 0.0f;

    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(ent->s.weapon);
    ent->nextthink = coduo_int32_from_bits((uint32_t)ent->nextthink + (uint32_t)ARTILLERY_BARRAGE_THINK_MS);

    if (ent->itemCount < weaponInfo->artilleryBarrageCount && ent->splashDamage < level.time) {
        ent->itemCount = coduo_int32_from_bits((uint32_t)ent->itemCount + 1u);
        delay = irand(weaponInfo->artilleryBarrageDelayMin, weaponInfo->artilleryBarrageDelayMax);
        /* NO shim: delay*0.5 is exact (delay/2, a power-of-two scale) and
         * (float)splashDamage is an exact small integer; their sum is exact in
         * both 64-bit and 80-bit, so the (int) truncation is identical (rule 6).
         */
        ent->splashDamage =
            game_compat_int32_from_long_double_trunc((long double)delay * (long double)0.5f + (long double)(float)ent->splashDamage);
        tempEnt = G_TempEntity(ent->currentOrigin, EV_PROJECTILE_LAUNCH);
        tempEnt->s.tempEffectId = weaponInfo->projectileExplosionType;
    }

    if (ent->droppedClipCount < weaponInfo->artilleryBarrageCount && ent->splashMinDamage < level.time) {
        ent->droppedClipCount = coduo_int32_from_bits((uint32_t)ent->droppedClipCount + 1u);
        delay = irand(weaponInfo->artilleryBarrageDelayMin, weaponInfo->artilleryBarrageDelayMax);
        ent->splashMinDamage = coduo_int32_from_bits((uint32_t)ent->splashMinDamage + (uint32_t)delay);

        angles[1] = (float)irand(0, 360);
        AngleVectors(angles, forward, NULL, NULL);

#if EMULATE_X87
        /* origin[i] = currentOrigin[i] + spread * signedRand * forward[i], the
         * whole chain in x87 width after a fresh normalized sample per axis. */
        for (int i = 0; i < 3; i++) {
            double sr = coduo_server_rand_signed_unit();
            origin[i] = x87f_store_f32(x87f_add(
                x87f_load_f32(ent->currentOrigin[i]),
                x87f_mul(x87f_mul(x87f_load_i32(weaponInfo->artilleryBarrageSpread), x87f_load_f64(sr)), x87f_load_f32(forward[i]))));
        }
#else
        origin[0] =
            (float)((long double)ent->currentOrigin[0] + (long double)weaponInfo->artilleryBarrageSpread *
                                                             (long double)coduo_server_rand_signed_unit() * (long double)forward[0]);
        origin[1] =
            (float)((long double)ent->currentOrigin[1] + (long double)weaponInfo->artilleryBarrageSpread *
                                                             (long double)coduo_server_rand_signed_unit() * (long double)forward[1]);
        origin[2] =
            (float)((long double)ent->currentOrigin[2] + (long double)weaponInfo->artilleryBarrageSpread *
                                                             (long double)coduo_server_rand_signed_unit() * (long double)forward[2]);
#endif

        tempEnt = G_TempEntity(origin, EV_PROJECTILE_INCOMING);
        tempEnt->s.tempEffectId = weaponInfo->projectileExplosionType;

        origin[2] += ARTILLERY_FALL_HEIGHT;
        fire_artillery(ent, origin, ARTILLERY_BARRAGE_FIRE_DELAY_MS);
    }

    if (weaponInfo->artilleryBarrageCount <= ent->itemCount && weaponInfo->artilleryBarrageCount <= ent->droppedClipCount) {
        ent->think = G_FreeEntity;
    }
}

/* ------------------------------------------------------------------ */
/*  0x5eabe  G_DropArtillery                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5eabe, 6eabe_G_DropArtillery.c, VERIFY-MISSILE-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
void G_DropArtillery(gentity_t *ent)
{
    ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)ARTILLERY_DROP_LIFETIME_MS);
    ent->think = G_ExplodeMissile;
    ent->svFlags = ARTILLERY_DROP_SVFLAGS;
    ent->s.pos.trType = TR_GRAVITY;
    ent->s.pos.trTime = coduo_int32_from_bits((uint32_t)level.time - (uint32_t)MISSILE_TRAJECTORY_BACKDATE_MS);
}

/* ------------------------------------------------------------------ */
/*  0x5eb25  G_MissileDie                                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5eb25, 6eb25_G_MissileDie.c, VERIFY-MISSILE-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
void G_MissileDie(gentity_t *ent, gentity_t *inflictor, gentity_t *attacker, int damage, int mod, int weapon, const float *dir,
                  int hitLocation)
{
    (void)attacker;
    (void)damage;
    (void)mod;
    (void)weapon;
    (void)dir;
    (void)hitLocation;

    if (inflictor != ent) {
        ent->takeDamage = 0;
        ent->think = G_ExplodeMissile;
        ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)MISSILE_DIE_DELAY_MS);
    }
}

/* ------------------------------------------------------------------ */
/*  0x5f5eb  DynaSink                                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5f5eb, 6f5eb_DynaSink.c, VERIFY-MISSILE-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
void DynaSink(gentity_t *ent)
{
    ent->clipmask = 0;
    ent->scriptContents = 0;
    if (ent->dynaSinkEndTime < level.time) {
        ent->think = G_FreeEntity;
        ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)DYNA_SINK_FREE_INTERVAL_MS);
    } else {
        ent->s.pos.trBase[2] -= 0.5f;
        ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)DYNA_SINK_INTERVAL_MS);
    }
}

/* ------------------------------------------------------------------ */
/*  0x5f683  G_GrenadeDie                                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5f683, 6f683_G_GrenadeDie.c, VERIFY-MISSILE-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
void G_GrenadeDie(gentity_t *ent, gentity_t *inflictor, gentity_t *attacker, int damage, int mod, int weapon, const float *dir,
                  int hitLocation)
{
    (void)inflictor;
    (void)attacker;
    (void)damage;
    (void)mod;
    (void)weapon;
    (void)dir;
    (void)hitLocation;

    ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)GRENADE_DIE_THINK_MS);
    ent->takeDamage = 0;
}

/* ------------------------------------------------------------------ */
/*  0x5f6b7  G_GrenadeLeaveOwnerThink                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5f6b7, 6f6b7_G_GrenadeLeaveOwnerThink.c, VERIFY-MISSILE-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
void G_GrenadeLeaveOwnerThink(gentity_t *ent)
{
    trace_t trace;
    const weaponInfo_t *weaponInfo;

    G_MissileTrace(&trace, ent->currentOrigin, ent->currentOrigin, ENTITYNUM_NONE, MISSILE_OWNER_TRACE_CONTENTS);
    if (level.time < coduo_int32_from_bits((uint32_t)ent->missileFuseTime - (uint32_t)GRENADE_OWNER_LEAVE_RECHECK_MS) &&
        (trace.allsolid != 0 || trace.startsolid != 0)) {
        ent->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)GRENADE_OWNER_LEAVE_RECHECK_MS);
        return;
    }

    ent->nextthink = ent->missileFuseTime;
    ent->passEntityNum = ent->s.number;
    trap_LinkEntity(ent);

    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(ent->s.weapon);
    if (weaponInfo->projectileExplosionType == PROJECTILE_EXPLOSION_TYPE_SMOKE) {
        ent->think = G_ExplodeSmokeGrenade;
    } else {
        ent->think = G_ExplodeMissile;
    }
}

/* ------------------------------------------------------------------ */
/*  0x5f7ad  fire_grenade                                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5f7ad, 6f7ad_fire_grenade.c, VERIFY-MISSILE-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
gentity_t *fire_grenade(gentity_t *self, float *start, float *dir, int weapon)
{
    const weaponInfo_t *weaponInfo;
    gentity_t *grenade;

    grenade = G_Spawn();
    if (self->client == NULL || self->client->ps.grenadeTimeLeft == 0) {
        grenade->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)GRENADE_DEFAULT_FUSE_MS);
    } else {
        grenade->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)self->client->ps.grenadeTimeLeft);
    }
    grenade->missileFuseTime = grenade->nextthink;
    if (self->client != NULL) {
        self->client->ps.grenadeTimeLeft = 0;
    }

    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(weapon);
    if (weaponInfo->projectileExplosionType == PROJECTILE_EXPLOSION_TYPE_SMOKE) {
        grenade->think = G_ExplodeSmokeGrenade;
    } else {
        grenade->think = G_ExplodeMissile;
    }

    grenade->s.eType = ET_MISSILE;
    grenade->svFlags = ARTILLERY_DROP_SVFLAGS;
    grenade->s.weapon = weapon;
    grenade->passEntityNum = self->s.number;
    grenade->entityRef = self;
    Scr_SetString(&grenade->scriptClassname, scr_const_grenade_projectile);

    grenade->damage = weaponInfo->flameDamage;
    grenade->splashDamage = weaponInfo->missileSplashDamage;
    grenade->splashMinDamage = weaponInfo->missileSplashMinDamage;
    grenade->splashRadius = weaponInfo->missileSplashRadius;
    grenade->methodOfDeath = MOD_GRENADE;
    grenade->splashMethodOfDeath = MOD_GRENADE_SPLASH;
    grenade->s.eFlags = GRENADE_BOUNCE_FLAGS;
    grenade->clipmask = MISSILE_CLIPMASK;

    grenade->s.pos.trType = TR_GRAVITY;
    grenade->s.pos.trTime = level.time;
    game_compat_g_missile_vector_copy(start, grenade->s.pos.trBase);
    game_compat_g_missile_vector_copy(dir, grenade->s.pos.trDelta);
    game_compat_g_missile_round_vector(grenade->s.pos.trDelta);

    grenade->s.apos.trType = TR_LINEAR;
    grenade->s.apos.trTime = level.time;
    vectoangles(dir, grenade->s.apos.trBase);
    grenade->s.apos.trBase[0] = AngleNormalize360(grenade->s.apos.trBase[0] - 120.0f);
    grenade->s.apos.trDelta[0] = 720.0f + flrand(-45.0f, 45.0f);
    grenade->s.apos.trDelta[1] = 0.0f;
    grenade->s.apos.trDelta[2] = 360.0f + flrand(-45.0f, 45.0f);

    game_compat_g_missile_vector_copy(start, grenade->currentOrigin);
    game_compat_g_missile_vector_copy(grenade->s.apos.trBase, grenade->currentAngles);

    grenade->takeDamage = (uint8_t)weaponInfo->grenadeTouchDamageEnabled;
    if (grenade->takeDamage != 0 &&
        coduo_int32_from_bits((uint32_t)level.time + (uint32_t)GRENADE_OWNER_LEAVE_DELAY_MS) < grenade->missileFuseTime) {
        grenade->mins[0] = -6.0f;
        grenade->mins[1] = -6.0f;
        grenade->mins[2] = 0.0f;
        grenade->maxs[0] = 6.0f;
        grenade->maxs[1] = 6.0f;
        grenade->maxs[2] = 6.0f;
        grenade->scriptContents = MISSILE_OWNER_LEAVE_BOUNDS_FLAG;
        grenade->svFlags |= SVF_CAPSULE;
        grenade->svFlags &= GRENADE_OWNER_LEAVE_SVFLAGS_MASK;
        grenade->s.eFlags |= EF_CAPSULE;
        grenade->health = 1;
        grenade->die = G_GrenadeDie;
        grenade->parentEntityNum = grenade->passEntityNum;
        grenade->think = G_GrenadeLeaveOwnerThink;
        grenade->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)GRENADE_OWNER_LEAVE_DELAY_MS);
        trap_LinkEntity(grenade);
    }

    return grenade;
}

/* ------------------------------------------------------------------ */
/*  0x5fc74  fire_rocket                                              */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5fc74, 6fc74_fire_rocket.c, VERIFY-MISSILE-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
gentity_t *fire_rocket(gentity_t *self, float *start, float *dir)
{
    const weaponInfo_t *weaponInfo;
    gentity_t *rocket;

    VectorNormalize(dir);
    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(self->s.weapon);
    rocket = G_Spawn();

    Scr_SetString(&rocket->scriptClassname, scr_const_rocket_projectile);
    rocket->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)ROCKET_LIFETIME_MS);
    rocket->think = G_ExplodeMissile;
    rocket->s.eType = ET_MISSILE;
    rocket->s.eFlags |= MISSILE_LAUNCH_S_FLAG;
    rocket->svFlags = ARTILLERY_DROP_SVFLAGS;
    rocket->s.weapon = self->s.weapon;
    if (self->s.eType == ET_VEHICLE) {
        rocket->parentEntityNum = self->passEntityNum;
    } else {
        rocket->parentEntityNum = ENTITYNUM_NONE;
    }
    rocket->passEntityNum = self->s.number;
    rocket->entityRef = self;
    rocket->damage = weaponInfo->flameDamage;
    rocket->splashDamage = weaponInfo->missileSplashDamage;
    rocket->splashMinDamage = weaponInfo->missileSplashMinDamage;
    rocket->splashRadius = weaponInfo->missileSplashRadius;
    rocket->methodOfDeath = MOD_PROJECTILE;
    rocket->splashMethodOfDeath = MOD_PROJECTILE_SPLASH;
    rocket->clipmask = MISSILE_CLIPMASK;

    rocket->s.pos.trType = TR_LINEAR;
    rocket->s.pos.trTime = coduo_int32_from_bits((uint32_t)level.time - (uint32_t)MISSILE_TRAJECTORY_BACKDATE_MS);
    game_compat_g_missile_vector_copy(start, rocket->s.pos.trBase);
    /* 0x5fe32: fild missileSpeed feeds the multiply directly; the integer
     * is never rounded to float, only the final store rounds. */
    rocket->s.pos.trDelta[0] = dir[0] * (long double)weaponInfo->missileSpeed;
    rocket->s.pos.trDelta[1] = dir[1] * (long double)weaponInfo->missileSpeed;
    rocket->s.pos.trDelta[2] = dir[2] * (long double)weaponInfo->missileSpeed;
    game_compat_g_missile_round_vector(rocket->s.pos.trDelta);

    game_compat_g_missile_vector_copy(start, rocket->currentOrigin);
    vectoangles(rocket->s.pos.trDelta, rocket->currentAngles);
    G_SetAngle(rocket, rocket->currentAngles);
    return rocket;
}

/* ------------------------------------------------------------------ */
/*  0x5ff4a  fire_artillery                                           */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5ff4a, 6ff4a_fire_artillery.c, VERIFY-MISSILE-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
gentity_t *fire_artillery(gentity_t *self, float *origin, int delay)
{
    const weaponInfo_t *weaponInfo;
    gentity_t *artillery;
    vec3_t down;
    vec3_t start;

    down[0] = 0.0f;
    down[1] = 0.0f;
    down[2] = -1.0f;

    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(self->s.weapon);
    artillery = G_Spawn();
    Scr_SetString(&artillery->scriptClassname, scr_const_rocket_projectile);
    artillery->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)delay);
    artillery->think = G_DropArtillery;
    artillery->s.eType = ET_MISSILE;
    artillery->s.eFlags |= MISSILE_LAUNCH_S_FLAG;
    artillery->svFlags = ARTILLERY_PENDING_SVFLAGS;
    artillery->s.weapon = self->s.weapon;

    if (g_entities[self->s.number].s.eType == ET_GENERAL) {
        artillery->passEntityNum = self->passEntityNum;
        artillery->entityRef = &g_entities[self->passEntityNum];
    } else {
        artillery->passEntityNum = self->s.number;
        artillery->entityRef = self;
    }

    artillery->damage = weaponInfo->flameDamage;
    artillery->splashDamage = weaponInfo->missileSplashDamage;
    artillery->splashMinDamage = weaponInfo->missileSplashMinDamage;
    artillery->splashRadius = weaponInfo->missileSplashRadius;
    artillery->methodOfDeath = MOD_ARTILLERY;
    artillery->splashMethodOfDeath = MOD_ARTILLERY_SPLASH;
    artillery->clipmask = MISSILE_CLIPMASK;
    artillery->s.groundEntityNum = ENTITYNUM_WORLD;

    artillery->s.pos.trType = TR_STATIONARY;
    start[0] = origin[0];
    start[1] = origin[1];
    start[2] = origin[2] + ARTILLERY_FALL_HEIGHT;
    game_compat_g_missile_vector_copy(start, artillery->s.pos.trBase);
    /* 0x60147: fild missileSpeed feeds the multiply directly; the integer
     * is never rounded to float, only the final store rounds. */
    artillery->s.pos.trDelta[0] = down[0] * (long double)weaponInfo->missileSpeed;
    artillery->s.pos.trDelta[1] = down[1] * (long double)weaponInfo->missileSpeed;
    artillery->s.pos.trDelta[2] = down[2] * (long double)weaponInfo->missileSpeed;

    game_compat_g_missile_vector_copy(start, artillery->currentOrigin);
    vectoangles(down, artillery->currentAngles);
    G_SetAngle(artillery, artillery->currentAngles);
    G_SetModel(artillery, weaponInfo->clipModel);
    G_DObjUpdate(artillery);
    return artillery;
}

/* ------------------------------------------------------------------ */
/*  0x601fb  fire_artillery_barrage                                   */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x601fb, 701fb_fire_artillery_barrage.c, VERIFY-MISSILE-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
void fire_artillery_barrage(gentity_t *self, float *origin, int weapon)
{
    const weaponInfo_t *weaponInfo;
    gentity_t *barrage;

    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(weapon);
    barrage = G_Spawn();
    Scr_SetString(&barrage->scriptClassname, scr_const_rocket_projectile);
    barrage->nextthink = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)ARTILLERY_BARRAGE_THINK_MS);
    barrage->think = G_BarrageThink;
    barrage->s.eType = ET_GENERAL;
    barrage->svFlags = ARTILLERY_PENDING_SVFLAGS;
    barrage->s.weapon = weapon;
    barrage->passEntityNum = self->s.number;
    barrage->entityRef = self;
    game_compat_g_missile_vector_copy(origin, barrage->currentOrigin);
    barrage->itemCount = 0;
    barrage->droppedClipCount = 0;
    barrage->splashDamage = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)ARTILLERY_BARRAGE_WARNING_DELAY_MS);
    barrage->splashMinDamage = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)weaponInfo->artilleryBarrageFirstDelay);

    Scr_AddVector(origin);
    Scr_Notify(self, scr_const_artillery_impact, 1);
}
