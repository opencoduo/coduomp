/*
 * Source reconstruction for damage system functions.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "recovered_game.h"
#include "g_public.h"
#include "game_globals.h"
#include "g_syscalls.h"
#include "game_functions.h"
#include "scr_vm.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */
#include "compat/libm/coduo_libm.h"

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

/* MOD enum and damage flags are defined in recovered_game.h */
#define DAMAGE_VISIBILITY_TRACE_MASK 0x00802091u
#define DAMAGE_VISIBILITY_SAMPLE_RADIUS 15.0f
#define DAMAGE_VISIBILITY_EPSILON 0.00001f /* original float32 0x3727c5ac */
#define RADIUS_DAMAGE_VERTICAL_OFFSET 24.0f /* original float32 0x41c00000 */
#define RADIUS_DAMAGE_VEHICLE_BOUNDS_SCALE 0.75f /* original float32 0x3f400000 */
#define RADIUS_DAMAGE_PARTIAL_TRACE_MASK MASK_GRENADE_TRACE
#define RADIUS_DAMAGE_PARTIAL_RADIUS_SCALE 0.2f
#define RADIUS_DAMAGE_PARTIAL_DAMAGE_SCALE 0.1f
#define DAMAGECLIENT_VEHICLE_OCCUPANT_PSF 0x00100000u

/* ------------------------------------------------------------------ */
/*  0x4fedb  G_Damage                                                 */
/* ------------------------------------------------------------------ */

/*
 * Apply damage to an entity.
 *
 * This is the core damage application function. It handles:
 *  - Client vs non-client entity routing
 *  - Vehicle damage scaling
 *  - Script damage callbacks
 *  - Health reduction and death detection
 *  - Pain and death callbacks
 *
 * VERIFIED_DECOMPILER(0x4fedb, 5fedb_G_Damage.c, VERIFY-DAMAGE-FIRST-FUNCTIONS-2026-06-17): DATAFLOW_VERIFIED; client dispatch before non-client takeDamage gate, vehicle immunity/radius gates, truncating vehicle scale, mover trigger/use path, mover_alt/general notify order, health/death/pain side effects checked against current decompiler output.
 *
 * RECOVERED(UO-GAME-UNK-0171): Entity damage callback is at offset +0x220,
 * pain callback at +0x224, death callback at +0x228. Entities with a
 * vehicle state pointer use Scr_Vehicle_DamageScale for damage modification.
 */
void G_Damage(gentity_t *target, gentity_t *inflictor, gentity_t *attacker, const float *dir, const float *point, int damage, int flags,
              int mod, int hitLocation)
{
    vec3_t normalizedDir = {0.0f, 0.0f, 0.0f};

    /* Client entity - delegate to G_DamageClient */
    if (target->client != NULL) {
        G_DamageClient(target, inflictor, attacker, dir, point, damage, flags, mod, hitLocation);
        return;
    }

    /* Check if non-client target takes damage */
    if (!game_compat_gentity_can_take_damage(target)) {
        return;
    }

    /* Non-client entity damage path */

    /* Vehicle damage scaling */
    if (target->vehicle != NULL) {
        /* Check vehicle immunity */
        if (G_IsVehicleImmune(target, mod)) {
            return;
        }

        /* Radius damage with less than 100 damage is ignored for vehicles */
        if ((flags & DAMAGE_RADIUS) != 0 && damage < 100) {
            return;
        }

        /* Scale damage based on vehicle type */
        float scale = Scr_Vehicle_DamageScale(target, attacker, inflictor, point, mod);
        /* NO shim: a single float*float product is exact in double (<=48 sig
         * bits), so the (long double) intermediate is identical in 64-bit and
         * 80-bit and the (int) truncation matches (rule 6). Applies to the other
         * (int)((ld)damage*(ld)scale/multiplier) casts here too. */
        damage = (int32_t)((long double)(float)damage * (long double)scale);
    }

    /* Default inflictor/attacker to world if NULL */
    if (inflictor == NULL) {
        inflictor = &g_entities[ENTITYNUM_WORLD];
    }
    if (attacker == NULL) {
        attacker = &g_entities[ENTITYNUM_WORLD];
    }

    /* Handle mover entities (type 5) */
    if (target->s.eType == ET_MOVER) {
        if (target->use != NULL && target->moverState == 0) {
            /* VERIFIED_DECOMPILER(0x4fedb, 5fedb_G_Damage.c, VERIFY-DAMAGE-FIRST-FUNCTIONS-2026-06-17): DATAFLOW_VERIFIED - ET_MOVER use path notifies "trigger" before invoking use callback. */
            Scr_AddEntity(attacker);
            Scr_Notify(target, scr_const_trigger, 1);
            target->use(target, inflictor, attacker);
        }
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (dir != NULL) {
        VectorNormalize2(dir, normalizedDir);
    }

    /* Handle mover_alt entities (type 8) - script notify with 6 params */
    if (target->s.eType == ET_SCRIPTMOVER) {
        Scr_AddEntity(inflictor);
        Scr_AddString(modNames[mod]);
        Scr_AddVector(point != NULL ? point : target->currentOrigin);
        Scr_AddVector(normalizedDir);
        Scr_AddEntity(attacker);
        Scr_AddInt(damage);
        Scr_Notify(target, scr_const_damage, 6);
        return;
    }

    /* General entity damage */

    /* Check invulnerability flag */
    if ((target->flags & 1) != 0) {
        return;
    }

    /* Clamp minimum damage to 1 */
    if (damage < 1) {
        damage = 1;
    }

    /* Debug damage output */
    if (g_debugDamage.integer != 0) {
        G_Printf("target:%i health:%i damage:%i\n", target->s.number, target->health, damage);
    }

    /* Check for special classname immunity */
    if (target->scriptClassname != scr_const_misc_mg42 && target->scriptClassname != scr_const_misc_turret) {
        /* Apply damage */
        target->health -= damage;
    }

    /* Send script damage notify with 6 parameters */
    Scr_AddEntity(inflictor);
    Scr_AddString(modNames[mod]);
    Scr_AddVector(point != NULL ? point : target->currentOrigin);
    Scr_AddVector(normalizedDir);
    Scr_AddEntity(attacker);
    Scr_AddInt(damage);
    Scr_Notify(target, scr_const_damage, 6);

    /* Check for death */
    if (target->health < 1) {
        /* Clamp health to minimum -999 */
        if (target->health < -999) {
            target->health = -999;
        }

        /* Send death notify */
        Scr_AddEntity(attacker);
        Scr_Notify(target, scr_const_death, 1);

        /* Store attacker reference */
        target->attacker = attacker;

        /* Call death callback if present */
        if (target->die != NULL) {
            target->die(target, inflictor, attacker, damage, mod, inflictor->s.weapon, normalizedDir, hitLocation);
        }
    } else {
        /* Call pain callback if present and health still positive */
        if (target->pain != NULL) {
            if (dir == NULL) {
                /* Clear damage direction fields */
                target->damageDir[0] = 0.0f;
                target->damageDir[1] = 0.0f;
                target->damageDir[2] = 0.0f;
                target->damagePoint[0] = 0.0f;
                target->damagePoint[1] = 0.0f;
                target->damagePoint[2] = 0.0f;
            } else {
                /* Store damage direction and point */
                target->damageDir[0] = normalizedDir[0];
                target->damageDir[1] = normalizedDir[1];
                target->damageDir[2] = normalizedDir[2];
                target->damagePoint[0] = point[0];
                target->damagePoint[1] = point[1];
                target->damagePoint[2] = point[2];
            }
            target->pain(target, attacker, damage, point, mod, normalizedDir, hitLocation);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x5098b  G_RadiusDamage                                           */
/* ------------------------------------------------------------------ */

/*
 * Apply radius damage to all entities within blast radius.
 *
 * Calculates damage falloff based on distance from explosion origin,
 * checks line of sight, and applies damage to affected entities.
 *
 * VERIFIED_DECOMPILER(0x5098b, 6098b_G_RadiusDamage.c, VERIFY-DAMAGE-SYSTEM-PACKET-2026-06-17): DATAFLOW_VERIFIED
 * - entity selection, distance falloff, visibility, partial trace, and
 *   return value match generated decompiler output.
 *
 * RECOVERED(UO-GAME-UNK-0172): Uses trap_EntitiesInBox with bounding box
 * expanded by radius * CoduoLibm_Sqrt(2). Damage scales linearly from maxDamage
 * at origin to minDamage at radius distance.
 */
int G_RadiusDamage(const float *origin, gentity_t *inflictor, gentity_t *attacker, float maxDamage, float minDamage, float radius,
                   gentity_t *ignore, int mod)
{
    vec3_t mins, maxs;
    int entityList[1024];
    int numEntities;
    int i;
    gentity_t *ent;
    vec3_t delta;
    float distance;
    float damage;
    vec3_t dir;
    int hitAny;

    /* Validate parameters */
    if (attacker == NULL) {
        return 0;
    }

    /* Clamp minimum radius */
    if (radius < 1.0f) {
        radius = 1.0f;
    }

    /* Calculate bounding box (expanded by CoduoLibm_Sqrt(2) for diagonal coverage) */
    float expandRadius = radius * 1.4142135f; /* CoduoLibm_Sqrt(2), original float32 0x3fb504f3 */
    mins[0] = origin[0] - expandRadius;
    mins[1] = origin[1] - expandRadius;
    mins[2] = origin[2] - expandRadius;
    maxs[0] = origin[0] + expandRadius;
    maxs[1] = origin[1] + expandRadius;
    maxs[2] = origin[2] + expandRadius;

    /* Find all entities in bounding box */
    numEntities = trap_EntitiesInBox(mins, maxs, entityList, 1024, MASK_ALL);

    hitAny = 0;

    /* Process each entity */
    for (i = 0; i < numEntities; i++) {
        ent = &g_entities[entityList[i]];

        /* Skip ignored entity */
        if (ent == ignore) {
            continue;
        }

        /* Skip entities that don't take damage */
        if (!game_compat_gentity_can_take_damage(ent)) {
            continue;
        }

        if (ent->contents == 0) {
            delta[0] = ent->currentOrigin[0] - origin[0];
            delta[1] = ent->currentOrigin[1] - origin[1];
            delta[2] = ent->currentOrigin[2] - origin[2];
        } else if (ent->vehicle == NULL) {
            for (int axis = 0; axis < 3; axis++) {
                if (origin[axis] < ent->absMin[axis]) {
                    delta[axis] = ent->absMin[axis] - origin[axis];
                } else if (ent->absMax[axis] < origin[axis]) {
                    delta[axis] = origin[axis] - ent->absMax[axis];
                } else {
                    delta[axis] = 0.0f;
                }
            }
        } else {
            for (int axis = 0; axis < 3; axis++) {
                /* 0x50b8d/0x50bcc/0x50c20/0x50c64: stock never rounds the
                 * absMin-mins*0.75 bound to float -- it compares the 80-bit
                 * value and recomputes it inside the taken branch, so delta
                 * gets exactly one rounding.  Do not hoist into a float
                 * temporary. */
#if EMULATE_X87
                {
                    x87f loBound = x87f_sub(x87f_load_f32(ent->absMin[axis]),
                                            x87f_mul(x87f_load_f32(ent->mins[axis]), x87f_load_f32(RADIUS_DAMAGE_VEHICLE_BOUNDS_SCALE)));
                    x87f hiBound = x87f_sub(x87f_load_f32(ent->absMax[axis]),
                                            x87f_mul(x87f_load_f32(ent->maxs[axis]), x87f_load_f32(RADIUS_DAMAGE_VEHICLE_BOUNDS_SCALE)));
                    if (x87f_lt(x87f_load_f32(origin[axis]), loBound)) {
                        delta[axis] = x87f_store_f32(x87f_sub(loBound, x87f_load_f32(origin[axis])));
                    } else if (x87f_lt(hiBound, x87f_load_f32(origin[axis]))) {
                        delta[axis] = x87f_store_f32(x87f_sub(x87f_load_f32(origin[axis]), hiBound));
                    } else {
                        delta[axis] = 0.0f;
                    }
                }
#else
                if (origin[axis] < ent->absMin[axis] - ent->mins[axis] * RADIUS_DAMAGE_VEHICLE_BOUNDS_SCALE) {
                    delta[axis] = (ent->absMin[axis] - ent->mins[axis] * RADIUS_DAMAGE_VEHICLE_BOUNDS_SCALE) - origin[axis];
                } else if (ent->absMax[axis] - ent->maxs[axis] * RADIUS_DAMAGE_VEHICLE_BOUNDS_SCALE < origin[axis]) {
                    delta[axis] = origin[axis] - (ent->absMax[axis] - ent->maxs[axis] * RADIUS_DAMAGE_VEHICLE_BOUNDS_SCALE);
                } else {
                    delta[axis] = 0.0f;
                }
#endif
            }
        }

#if EMULATE_X87
        /* double dot summed in x87 width (FLT_EVAL_METHOD=2), rounded to the
         * double distanceSquared, then sqrt. */
        double distanceSquared = x87f_store_f64(x87f_add(x87f_add(x87f_mul(x87f_load_f32(delta[0]), x87f_load_f32(delta[0])),
                                                                  x87f_mul(x87f_load_f32(delta[1]), x87f_load_f32(delta[1]))),
                                                         x87f_mul(x87f_load_f32(delta[2]), x87f_load_f32(delta[2]))));
#else
        double distanceSquared =
            (double)delta[0] * (double)delta[0] + (double)delta[1] * (double)delta[1] + (double)delta[2] * (double)delta[2];
#endif
        distance = (float)CoduoLibm_Sqrt(distanceSquared);

        /* Skip entities outside radius */
        if (distance >= radius) {
            continue;
        }
        if (ent->client != NULL && level.radiusDamageIgnorePlayersActive != 0) {
            continue;
        }

        /* Calculate damage with linear falloff */
#if EMULATE_X87
        /* minDamage + (1 - distance/radius) * (maxDamage - minDamage), all in
         * x87 width. */
        damage = x87f_store_f32(x87f_add(x87f_load_f32(minDamage),
                                         x87f_mul(x87f_sub(x87f_load_f32(1.0f), x87f_div(x87f_load_f32(distance), x87f_load_f32(radius))),
                                                  x87f_sub(x87f_load_f32(maxDamage), x87f_load_f32(minDamage)))));
#else
        damage = (float)((long double)minDamage +
                         ((1.0L - (long double)distance / (long double)radius) * ((long double)maxDamage - (long double)minDamage)));
#endif

        /* Check line of sight */
        float damageScale = CanDamage(ent, origin);
        if (damageScale > 0.0f) {
            if (LogAccuracyHit(ent, attacker)) {
                hitAny = 1;
            }

            dir[0] = ent->currentOrigin[0] - origin[0];
            dir[1] = ent->currentOrigin[1] - origin[1];
            /* 0x50eb5: stock rounds the z subtract to float, then adds the
             * 24.0 offset in a second rounding. */
            dir[2] = ent->currentOrigin[2] - origin[2];
            dir[2] += RADIUS_DAMAGE_VERTICAL_OFFSET;
            /* Direct line of sight - apply full damage */
            G_Damage(ent, inflictor, attacker, dir, origin, (int32_t)((long double)damage * (long double)damageScale), DAMAGE_RADIUS, mod,
                     0);
        } else {
            /* No direct line of sight - check for partial damage through geometry */
            /* Trace to entity midpoint */
            trace_t trace;
            vec3_t midpoint;
            vec3_t midpointDir;
            float midpointDistance;

            /* 0x50f43..0x50fbb: stock rounds each absMin+absMax sum to
             * float, then scales by the DOUBLE constant 0.5 (fld QWORD
             * 0x9fbd0) in a second rounding. */
            midpoint[0] = ent->absMin[0] + ent->absMax[0];
            midpoint[1] = ent->absMin[1] + ent->absMax[1];
            midpoint[2] = ent->absMin[2] + ent->absMax[2];
            midpoint[0] *= 0.5;
            midpoint[1] *= 0.5;
            midpoint[2] *= 0.5;
            trap_Trace(&trace, origin, vec3_origin, vec3_origin, midpoint, ENTITYNUM_NONE, RADIUS_DAMAGE_PARTIAL_TRACE_MASK);

            if (trace.fraction < 1.0f) {
                midpointDir[0] = midpoint[0] - origin[0];
                midpointDir[1] = midpoint[1] - origin[1];
                midpointDir[2] = midpoint[2] - origin[2];
                /* 0x51079: machine sums x*x + y*y + z*z in that order. */
#if EMULATE_X87
                double midpointDistanceSquared =
                    x87f_store_f64(x87f_add(x87f_add(x87f_mul(x87f_load_f32(midpointDir[0]), x87f_load_f32(midpointDir[0])),
                                                     x87f_mul(x87f_load_f32(midpointDir[1]), x87f_load_f32(midpointDir[1]))),
                                            x87f_mul(x87f_load_f32(midpointDir[2]), x87f_load_f32(midpointDir[2]))));
#else
                double midpointDistanceSquared = (double)midpointDir[0] * (double)midpointDir[0] +
                                                 (double)midpointDir[1] * (double)midpointDir[1] +
                                                 (double)midpointDir[2] * (double)midpointDir[2];
#endif
                midpointDistance = (float)CoduoLibm_Sqrt(midpointDistanceSquared);

                /* Apply 10% damage if the blocked midpoint is within 20% of
                 * radius. radius*0.2 is a full-width compare (0x3c603 stock:
                 * fmulp; fcomip, no spill) and 0.2 is not a power of two, so
                 * emulate. */
#if EMULATE_X87
                if (x87f_lt(x87f_load_f32(midpointDistance),
                            x87f_mul(x87f_load_f32(radius), x87f_load_f32(RADIUS_DAMAGE_PARTIAL_RADIUS_SCALE)))) {
#else
                if (midpointDistance < radius * RADIUS_DAMAGE_PARTIAL_RADIUS_SCALE) {
#endif
                    /* VERIFIED_DECOMPILER(0x5098b, 6098b_G_RadiusDamage.c, VERIFY-DAMAGE-SYSTEM-PACKET-2026-06-17): DATAFLOW_VERIFIED - partial branch recomputes raised dir. */
                    if (LogAccuracyHit(ent, attacker)) {
                        hitAny = 1;
                    }

                    dir[0] = ent->currentOrigin[0] - origin[0];
                    dir[1] = ent->currentOrigin[1] - origin[1];
                    /* 0x5112a: same two-step z rounding as above. */
                    dir[2] = ent->currentOrigin[2] - origin[2];
                    dir[2] += RADIUS_DAMAGE_VERTICAL_OFFSET;
                    G_Damage(ent, inflictor, attacker, dir, origin,
                             (int32_t)((long double)damage * (long double)RADIUS_DAMAGE_PARTIAL_DAMAGE_SCALE), DAMAGE_RADIUS, mod, 0);
                }
            }
        }
    }

    return hitAny;
}

/* ------------------------------------------------------------------ */
/*  0x4fd4b  G_DamageClient                                           */
/* ------------------------------------------------------------------ */

/*
 * Apply damage to a client entity with hit location multipliers.
 *
 * VERIFIED_DECOMPILER(0x4fd4b, 5fd4b_G_DamageClient.c, VERIFY-DAMAGE-SYSTEM-PACKET-2026-06-17): DATAFLOW_VERIFIED
 * - client gates, vehicle occupant scaling, weapon selection, hitloc
 *   multiplier, and invulnerability exception match generated decompiler
 *   output and binary disassembly at RVA 0x4fd4b.
 *
 * RECOVERED(UO-GAME-UNK-0177): Client damage checks vehicle occupancy,
 * invulnerability, and applies hit location damage multipliers from
 * g_fHitLocDamageMult table.
 */
void G_DamageClient(gentity_t *target, gentity_t *inflictor, gentity_t *attacker, const float *dir, const float *point, int damage,
                    int flags, int mod, int hitLocation)
{
    int weapon;

    /* Check if client can take damage */
    if (!game_compat_gentity_can_take_damage(target) || target->client->noclip != 0 || target->client->ufo != 0 ||
        target->client->connectedState != 2) { /* Not connected/active */
        return;
    }

    /* Handle vehicle occupant damage */
    if (target->client != NULL && (target->client->ps.entityStateFlags & DAMAGECLIENT_VEHICLE_OCCUPANT_PSF) != 0) {
        /* Scale radius damage for vehicle occupants */
        if ((flags & DAMAGE_RADIUS) != 0) {
            float scale = G_VehicleOccupantRadiusDamageScale(target);
            damage = (int32_t)((long double)(float)damage * (long double)scale);
        }

        /* Check vehicle occupant invulnerability */
        if (G_IsVehicleOccupantInvulnerable(target) != 0) {
            /* Allow damage from vehicle itself */
            if (inflictor != NULL && target->passEntityNum != inflictor->s.number) {
                return;
            }
        }
    }

    /* Apply hit location damage multiplier */
    float multiplier = g_fHitLocDamageMult[hitLocation];
    int32_t scaledDamage = (int32_t)((long double)damage * (long double)multiplier);

    /* Determine hit location type from inflictor */
    if (inflictor == NULL) {
        if (attacker == NULL) {
            weapon = 0;
        } else {
            weapon = attacker->s.weapon;
        }
    } else {
        weapon = inflictor->s.weapon;
    }

    /* Call script damage handler */
    Scr_PlayerDamage(target, inflictor, attacker, scaledDamage, flags, mod, weapon, point, dir, hitLocation);
}

/* ------------------------------------------------------------------ */
/*  0x503f8  CanDamage                                                */
/* ------------------------------------------------------------------ */

/*
 * Check if a target entity can be damaged from a given origin.
 * Returns a fractional visibility scale used by radius damage.
 *
 * RECOVERED(UO-GAME-UNK-0178): Uses multiple locational traces from
 * different points on the target to check line of sight. Uses trace
 * mask 0x802091 for damage visibility checks.
 */
/* NOT_FROM_ORIGINAL_SOURCE: helper extracted from repeated CanDamage trace checks.
 * VERIFIED_DECOMPILER(0x503f8, 603f8_CanDamage.c, VERIFY-DAMAGE-SYSTEM-PACKET-2026-06-17): DATAFLOW_VERIFIED;
 * preserves trap_LocationalTrace argument order and fraction equality test.
 */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original CanDamage (0x503f8); no standalone original body. */
static int game_compat_can_damage_trace_point(gentity_t *target, const float *point, const float *origin)
{
    trace_t trace;

    trap_LocationalTrace(&trace, point, origin, target->s.number, DAMAGE_VISIBILITY_TRACE_MASK, bulletPriorityMap);
    return trace.fraction == 1.0f;
}

float CanDamage(gentity_t *target, const float *origin)
{
    /* VERIFIED_DECOMPILER(0x503f8, 603f8_CanDamage.c, VERIFY-DAMAGE-SYSTEM-PACKET-2026-06-17): DATAFLOW_VERIFIED - full visibility sampling body. */
    vec3_t center;
    float tracePoints[5][3];
    int hitCount;

    if (target->client == NULL) {
        /* 0x507b9..0x50819: stock rounds each absMin+absMax sum to float,
         * then scales by 0.5f in a second rounding (VectorAdd/VectorScale
         * two-step) -- keep the intermediate float rounding. */
        center[0] = target->absMin[0] + target->absMax[0];
        center[1] = target->absMin[1] + target->absMax[1];
        center[2] = target->absMin[2] + target->absMax[2];
        center[0] *= 0.5f;
        center[1] *= 0.5f;
        center[2] *= 0.5f;

        tracePoints[0][0] = center[0];
        tracePoints[0][1] = center[1];
        tracePoints[0][2] = center[2];
        tracePoints[1][0] = center[0] + DAMAGE_VISIBILITY_SAMPLE_RADIUS;
        tracePoints[1][1] = center[1] + DAMAGE_VISIBILITY_SAMPLE_RADIUS;
        tracePoints[1][2] = center[2];
        tracePoints[2][0] = center[0] + DAMAGE_VISIBILITY_SAMPLE_RADIUS;
        tracePoints[2][1] = center[1] - DAMAGE_VISIBILITY_SAMPLE_RADIUS;
        tracePoints[2][2] = center[2];
        tracePoints[3][0] = center[0] - DAMAGE_VISIBILITY_SAMPLE_RADIUS;
        tracePoints[3][1] = center[1] + DAMAGE_VISIBILITY_SAMPLE_RADIUS;
        tracePoints[3][2] = center[2];
        tracePoints[4][0] = center[0] - DAMAGE_VISIBILITY_SAMPLE_RADIUS;
        tracePoints[4][1] = center[1] - DAMAGE_VISIBILITY_SAMPLE_RADIUS;
        tracePoints[4][2] = center[2];

        if (VectorDistanceSquared(center, origin) < DAMAGE_VISIBILITY_EPSILON) {
            return 1.0f;
        }

        for (int index = 0; index < 5; index++) {
            if (game_compat_can_damage_trace_point(target, tracePoints[index], origin)) {
                return 1.0f;
            }
        }

        return 0.0f;
    }

    vec3_t viewOrigin;
    vec3_t toOrigin;
    vec3_t side;
    float halfHeight;

    viewOrigin[0] = target->currentOrigin[0];
    viewOrigin[1] = target->currentOrigin[1];
    viewOrigin[2] = target->currentOrigin[2] + target->client->ps.viewHeightCurrent;
    G_AddLean(target, viewOrigin);

    halfHeight = (viewOrigin[2] - target->currentOrigin[2]) * 0.5f;

    toOrigin[0] = origin[0] - target->currentOrigin[0];
    toOrigin[1] = origin[1] - target->currentOrigin[1];
    toOrigin[2] = 0.0f;
    VectorNormalize(toOrigin);

    side[0] = -toOrigin[1];
    side[1] = toOrigin[0];
    side[2] = toOrigin[2];

    /* 0x50523..0x50580: stock rounds each viewOrigin+currentOrigin sum to
     * float, then scales by 0.5f in a second rounding -- keep both. */
    center[0] = viewOrigin[0] + target->currentOrigin[0];
    center[1] = viewOrigin[1] + target->currentOrigin[1];
    center[2] = viewOrigin[2] + target->currentOrigin[2];
    center[0] *= 0.5f;
    center[1] *= 0.5f;
    center[2] *= 0.5f;

    tracePoints[0][0] = center[0];
    tracePoints[0][1] = center[1];
    tracePoints[0][2] = center[2];
    /* 0x505b6/0x505f5/0x5065e/0x506c7: stock stores each z point as
     * center+side*radius (one float rounding), then adds/subtracts
     * halfHeight in a second rounding (VectorMA then z adjust). */
#if EMULATE_X87
    for (int j = 0; j < 3; j++) {
        float plus = x87f_store_f32(
            x87f_add(x87f_load_f32(center[j]), x87f_mul(x87f_load_f32(side[j]), x87f_load_f32(DAMAGE_VISIBILITY_SAMPLE_RADIUS))));
        float minus = x87f_store_f32(
            x87f_sub(x87f_load_f32(center[j]), x87f_mul(x87f_load_f32(side[j]), x87f_load_f32(DAMAGE_VISIBILITY_SAMPLE_RADIUS))));
        tracePoints[1][j] = plus;
        tracePoints[2][j] = plus;
        tracePoints[3][j] = minus;
        tracePoints[4][j] = minus;
    }
    /* halfHeight z adjusts are single adds/subs (a second rounding). */
    tracePoints[1][2] += halfHeight;
    tracePoints[2][2] -= halfHeight;
    tracePoints[3][2] += halfHeight;
    tracePoints[4][2] -= halfHeight;
#else
    tracePoints[1][0] = center[0] + side[0] * DAMAGE_VISIBILITY_SAMPLE_RADIUS;
    tracePoints[1][1] = center[1] + side[1] * DAMAGE_VISIBILITY_SAMPLE_RADIUS;
    tracePoints[1][2] = center[2] + side[2] * DAMAGE_VISIBILITY_SAMPLE_RADIUS;
    tracePoints[1][2] += halfHeight;
    tracePoints[2][0] = center[0] + side[0] * DAMAGE_VISIBILITY_SAMPLE_RADIUS;
    tracePoints[2][1] = center[1] + side[1] * DAMAGE_VISIBILITY_SAMPLE_RADIUS;
    tracePoints[2][2] = center[2] + side[2] * DAMAGE_VISIBILITY_SAMPLE_RADIUS;
    tracePoints[2][2] -= halfHeight;
    tracePoints[3][0] = center[0] - side[0] * DAMAGE_VISIBILITY_SAMPLE_RADIUS;
    tracePoints[3][1] = center[1] - side[1] * DAMAGE_VISIBILITY_SAMPLE_RADIUS;
    tracePoints[3][2] = center[2] - side[2] * DAMAGE_VISIBILITY_SAMPLE_RADIUS;
    tracePoints[3][2] += halfHeight;
    tracePoints[4][0] = center[0] - side[0] * DAMAGE_VISIBILITY_SAMPLE_RADIUS;
    tracePoints[4][1] = center[1] - side[1] * DAMAGE_VISIBILITY_SAMPLE_RADIUS;
    tracePoints[4][2] = center[2] - side[2] * DAMAGE_VISIBILITY_SAMPLE_RADIUS;
    tracePoints[4][2] -= halfHeight;
#endif

    hitCount = 0;
    for (int index = 0; index < 5; index++) {
        if (game_compat_can_damage_trace_point(target, tracePoints[index], origin)) {
            hitCount++;
        }
    }

    if (hitCount == 0) {
        return 0.0f;
    }

    if (hitCount < 4) {
#if EMULATE_X87
        return x87f_store_f32(x87f_div(x87f_load_i32(hitCount), x87f_load_f32(3.0f)));
#else
        return (float)hitCount / 3.0f;
#endif
    }

    return 1.0f;
}
