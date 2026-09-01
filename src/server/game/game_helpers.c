/*
 * Source reconstruction for small game-module helpers.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "recovered_game.h"
#include "game_globals.h"
#include "game_functions.h"
#include "g_syscalls.h"
#include "qcommon/info.h"
#include "compat/crt/qsort_compat.h"
#include "level_locals.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */

#define EVENT_RING_MASK 3
#define ACTIVATE_MAX_CANDIDATES 1024
#define ACTIVATE_SEARCH_XY 192.0f
#define ACTIVATE_SEARCH_Z 96.0f
#define ACTIVATE_MAX_DISTANCE 128.0f
#define ACTIVATE_MIN_DOT 0.76f
#define ACTIVATE_UNUSABLE_ITEM_PENALTY 10000.0f
#define ACTIVATE_OCCLUDED_PENALTY 100000.0f
#define ACTIVATE_ENTITY_CONTENTS 0x00200000u
#define ACTIVATE_VEHICLE_TRACE_MASK 0x02810011u
#define ACTIVATE_VIS_TRACE_MASK 0x00800011u
#define CURSOR_MOUNT_CONTENTS 0x00404000u
#define CURSOR_FLAG_WORLD_HINT 0x00000008u
#define CURSOR_PLAYERSTATE_WORLD_HINT_BLOCKED 0x00000010u
#define CURSOR_STANCE_FLAG 0x00000020u
#define CURSOR_HINT_OWNED_WEAPON_BASE 138
#define CURSOR_HINT_STRING_INHERIT 255
#define PREVENT_FRIENDLY_FIRE_MASK 0x20000001u
#define PREVENT_FRIENDLY_FIRE_CONFIRM_MASK 0x22802001u
#define PLAYER_RECOIL_VIEWKICK_EVENT_MIN EV_FIRE_WEAPON
#define PLAYER_RECOIL_VIEWKICK_EVENT_MAX EV_FIRE_WEAPON_LASTSHOT

typedef struct activate_candidate_s {
    gentity_t *ent;        /* +0x0 */
    float score;           /* +0x4 */
} activate_candidate_t;

#if UINTPTR_MAX == 0xffffffffu
GAME_STATIC_ASSERT(activate_candidate_size,
                   sizeof(activate_candidate_t) == 0x8);
GAME_STATIC_ASSERT(activate_candidate_score_offset,
                   offsetof(activate_candidate_t, score) == 0x4);
#endif

void *trap_Hunk_AllocAlignInternal(size_t size, int alignment);
qboolean G_IsVehicleUsable(gentity_t *vehicle, gentity_t *player);
qboolean G_IsTurretUsable(gentity_t *turret, gentity_t *player);
void Use_BinaryMover(gentity_t *ent, gentity_t *other,
                            gentity_t *activator);
int G_CheckPointInsideTriggerMount(gentity_t *ent, const float *point,
                                          int *mountHintData);

/* NOT_FROM_ORIGINAL_SOURCE: local vector copy helper for activation recovery. */
static void game_compat_g_vector_copy3(const float *src, float *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: local vector subtract helper for activation recovery. */
static void game_compat_g_vector_subtract3(const float *lhs, const float *rhs, float *out)
{
    out[0] = lhs[0] - rhs[0];
    out[1] = lhs[1] - rhs[1];
    out[2] = lhs[2] - rhs[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: local dot-product helper for activation recovery.
 * Stock inlines the dot at the call site (0x5721d: 3-mul/2-add kept 80-bit,
 * stored to float); the caller consumes it as a stored float, so rounding here
 * on return is equivalent -> shim. */
static float game_compat_g_dot_product3(const float *lhs, const float *rhs)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_add(x87f_add(
        x87f_mul(x87f_load_f32(lhs[0]), x87f_load_f32(rhs[0])),
        x87f_mul(x87f_load_f32(lhs[1]), x87f_load_f32(rhs[1]))),
        x87f_mul(x87f_load_f32(lhs[2]), x87f_load_f32(rhs[2]))));
#else
    return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: local entity bounds-center helper. */
static void game_compat_gentity_abs_center(const gentity_t *ent, vec3_t center)
{
    /* 0x57051-0x570d5 / 0x57413-0x5748b: the abs-bounds sums are stored
     * to center first, then scaled by 0.5f as a second float32 store. */
    center[0] = ent->absMin[0] + ent->absMax[0];
    center[1] = ent->absMin[1] + ent->absMax[1];
    center[2] = ent->absMin[2] + ent->absMax[2];
    center[0] *= 0.5f;
    center[1] *= 0.5f;
    center[2] *= 0.5f;
}

/* NOT_FROM_ORIGINAL_SOURCE: local predicate for weapon hint-string presence. */
static int game_compat_g_weapon_info_has_hint_string(const weaponInfo_t *weaponInfo)
{
    const char *hintString = weaponInfo->hintString;

    return hintString != NULL && hintString[0] != '\0';
}

/* NOT_FROM_ORIGINAL_SOURCE: names the binary mover in-use byte. */
static uint8_t *game_compat_g_binary_mover_in_use_byte(gentity_t *ent)
{
    return &ent->activeState;
}

/* NOT_FROM_ORIGINAL_SOURCE: factored repeated event-time stores. */
static void game_compat_g_set_entity_event_times(gentity_t *ent)
{
    ent->lastThinkTime = level.time;
    ent->eventTime2 = level.time;
}

/* VERIFIED_DECOMPILER(0x56d04, 66d04_G_Printf.c, VERIFY-GAMEHELPERS-2026-06-17): DATAFLOW_VERIFIED; varargs buffer and trap_Printf call match. */
void G_Printf(const char *format, ...)
{
    char buffer[MAX_STRING_CHARS];
    va_list args;

    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted diagnostic to its fixed
     * destination. */
    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    trap_Printf(buffer);
}

/* VERIFIED_DECOMPILER(0x56d52, 66d52_G_DPrintf.c, VERIFY-GAMEHELPERS-2026-06-17): DATAFLOW_VERIFIED; developer-gated varargs buffer and trap_Printf call match. */
void G_DPrintf(const char *format, ...)
{
    char buffer[MAX_STRING_CHARS];
    va_list args;

    if (g_developer.integer == 0) {
        return;
    }

    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted diagnostic to its fixed
     * destination. */
    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    trap_Printf(buffer);
}

/* VERIFIED_DECOMPILER(0x56dae, 66dae_G_Error.c, VERIFY-GAMEHELPERS-2026-06-17): DATAFLOW_VERIFIED; varargs buffer and trap_Error call match. */
void G_Error(const char *format, ...)
{
    char buffer[MAX_STRING_CHARS];
    va_list args;

    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted diagnostic to its fixed
     * destination. */
    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    trap_Error(buffer);
}

/* VERIFIED_DECOMPILER(0x56dfc, 66dfc_G_Error_Localized.c, VERIFY-GAMEHELPERS-2026-06-17): DATAFLOW_VERIFIED; varargs buffer and localized trap error match. */
void G_Error_Localized(const char *format, ...)
{
    char buffer[MAX_STRING_CHARS];
    va_list args;

    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted diagnostic to its fixed
     * destination. */
    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    trap_Error_Localized(buffer);
}

/* VERIFIED_DECOMPILER(0x58c90, 68c90_Com_Error.c, VERIFY-GAMEHELPERS-2026-06-17): DATAFLOW_VERIFIED; ignores code and forwards formatted text to G_Error("%s", buffer). */
void Com_Error(errorParm_t code, const char *format, ...)
{
    char buffer[MAX_STRING_CHARS];
    va_list args;

    (void)code;

    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted diagnostic to its fixed
     * destination. */
    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    G_Error("%s", buffer);
}

/* VERIFIED_DECOMPILER(0x58ce8, 68ce8_Com_Printf.c, VERIFY-GAMEHELPERS-2026-06-17): DATAFLOW_VERIFIED; formats and forwards through G_Printf("%s", buffer). */
void Com_Printf(const char *format, ...)
{
    char buffer[MAX_STRING_CHARS];
    va_list args;

    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted diagnostic to its fixed
     * destination. */
    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    G_Printf("%s", buffer);
}

/* VERIFIED_DECOMPILER(0x58d40, 68d40_Com_DPrintf.c, VERIFY-GAMEHELPERS-2026-06-17): DATAFLOW_VERIFIED; developer-gated formatted G_Printf("%s", buffer). */
void Com_DPrintf(const char *format, ...)
{
    char buffer[MAX_STRING_CHARS];
    va_list args;

    if (g_developer.integer == 0) {
        return;
    }

    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted diagnostic to its fixed
     * destination. */
    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    G_Printf("%s", buffer);
}

/* 0x56a7d FUN_00066a7d */
/* VERIFIED_DECOMPILER(0x56a7d, 66a7d_FUN_00066a7d.c, VERIFY-GAME-HELPERS-PARSE-INFO-2026-06-17): DATAFLOW_VERIFIED; float-plus-half integer conversion. */
int Game_RoundFloatPlusHalf(float value)
{
    /* 0x56a8f-0x56aac: the +0.5f sum is truncated straight from the x87
     * register (fistp direct), with no float32 rounding of the sum -> shim. */
#if EMULATE_X87
    return x87f_store_i32_trunc(
        x87f_add(x87f_load_f32(value), x87f_load_f32(0.5f)));
#else
    return game_compat_int32_from_long_double_trunc(
        (long double)value + (long double)0.5f);
#endif
}

/* 0x56e4a FUN_00066e4a */
/* VERIFIED_DECOMPILER(0x56e4a, 66e4a_FUN_00066e4a.c, VERIFY-GAME-HELPERS-ACTIVATE-PACKET-2026-06-17): DATAFLOW_VERIFIED - qsort comparator signature, score offset, float subtraction, and truncate-to-int return checked. */
int G_CompareActivateEntScores(const void *lhs, const void *rhs)
{
    const activate_candidate_t *a = (const activate_candidate_t *)lhs;
    const activate_candidate_t *b = (const activate_candidate_t *)rhs;

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
#if EMULATE_X87
    return x87f_store_i32_trunc(
        x87f_sub(x87f_load_f32(a->score), x87f_load_f32(b->score)));
#else
    return game_compat_int32_from_long_double_trunc(
        (long double)a->score - (long double)b->score);
#endif
}

/* ------------------------------------------------------------------ */
/*  0x56e85 G_GetActivateEnt                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x56e85, 66e85_G_GetActivateEnt.c, VERIFY-GAME-HELPERS-ACTIVATE-PACKET-2026-06-17): DATAFLOW_VERIFIED - activation box query, candidate filters, vehicle/entity targeting traces, scoring penalties, two qsorts, occlusion pruning, and return count checked. */
int G_GetActivateEnt(gentity_t *ent, activate_candidate_t *candidates)
{
    gclient_t *client = ent->client;
    vec3_t forward;
    vec3_t muzzle;
    vec3_t searchMins;
    vec3_t searchMaxs;
    int entityNums[ACTIVATE_MAX_CANDIDATES];
    int entityCount;
    int candidateCount = 0;
    int unusableItemCount = 0;
    int occludedCount = 0;
    int index;

    AngleVectors(client->ps.viewAngles, forward, NULL, NULL);
    CalcMuzzlePoint(ent, muzzle);

    searchMins[0] = muzzle[0] - ACTIVATE_SEARCH_XY;
    searchMins[1] = muzzle[1] - ACTIVATE_SEARCH_XY;
    searchMins[2] = muzzle[2] - ACTIVATE_SEARCH_Z;
    searchMaxs[0] = muzzle[0] + ACTIVATE_SEARCH_XY;
    searchMaxs[1] = muzzle[1] + ACTIVATE_SEARCH_XY;
    searchMaxs[2] = muzzle[2] + ACTIVATE_SEARCH_Z;

    entityCount = trap_EntitiesInBox(searchMins, searchMaxs, entityNums,
                                     ACTIVATE_MAX_CANDIDATES,
                                     ACTIVATE_ENTITY_CONTENTS);
    for (index = 0; index < entityCount; index++) {
        gentity_t *candidate = &g_entities[entityNums[index]];
        vec3_t center;
        vec3_t direction;
        float distance;
        float dot;
        float baseScore;

        if (candidate == ent ||
            (candidate->s.eType != ET_ITEM &&
             (candidate->scriptContents & ACTIVATE_ENTITY_CONTENTS) == 0)) {
            continue;
        }

        game_compat_gentity_abs_center(candidate, center);
        if (candidate->s.eType == ET_VEHICLE) {
            trace_t trace;

            /* Stock 0x570xx..0x5712a: forward[k]*DIST kept 80-bit, + muzzle[k],
             * one store -> shim (mul+add). */
#if EMULATE_X87
            for (int k = 0; k < 3; k++) {
                center[k] = x87f_store_f32(x87f_add(
                    x87f_mul(x87f_load_f32(forward[k]),
                             x87f_load_f32(ACTIVATE_MAX_DISTANCE)),
                    x87f_load_f32(muzzle[k])));
            }
#else
            center[0] = muzzle[0] + forward[0] * ACTIVATE_MAX_DISTANCE;
            center[1] = muzzle[1] + forward[1] * ACTIVATE_MAX_DISTANCE;
            center[2] = muzzle[2] + forward[2] * ACTIVATE_MAX_DISTANCE;
#endif
            trap_Trace(&trace, muzzle, vec3_origin, vec3_origin, center,
                       client->ps.psClientNum, ACTIVATE_VEHICLE_TRACE_MASK);
            if (trace.entityNum != candidate->s.number) {
                continue;
            }
            game_compat_g_vector_subtract3(trace.endpos, muzzle, direction);
        } else {
            game_compat_g_vector_subtract3(center, muzzle, direction);
        }

        distance = VectorNormalize(direction);
        dot = game_compat_g_dot_product3(direction, forward);
        if (distance > ACTIVATE_MAX_DISTANCE || dot <= 0.0f ||
            dot < ACTIVATE_MIN_DOT) {
            continue;
        }

        /* Stock 0x5727e..0x572a0: 1.0 - (dot-MIN)/(1.0-MIN), the divide kept
         * 80-bit through the outer subtract, one store -> shim. */
#if EMULATE_X87
        baseScore = x87f_store_f32(x87f_sub(
            x87f_load_f32(1.0f),
            x87f_div(
                x87f_sub(x87f_load_f32(dot), x87f_load_f32(ACTIVATE_MIN_DOT)),
                x87f_sub(x87f_load_f32(1.0f), x87f_load_f32(ACTIVATE_MIN_DOT)))));
#else
        baseScore = 1.0f - (dot - ACTIVATE_MIN_DOT) /
                    (1.0f - ACTIVATE_MIN_DOT);
#endif
        /* 0x572a0/0x572c2: the falloff factor is stored to a float32 slot
         * before the distance-scale multiply, which rounds again. */
        baseScore *= ACTIVATE_MAX_DISTANCE + ACTIVATE_MAX_DISTANCE;
        if (candidate->s.eType == ET_ITEM &&
            BG_CanItemBeGrabbed(&candidate->s, &client->ps, 0) == 0) {
            baseScore += ACTIVATE_UNUSABLE_ITEM_PENALTY;
            unusableItemCount++;
        }

        candidates[candidateCount].ent = candidate;
        candidates[candidateCount].score = baseScore + distance;
        candidateCount++;
    }

    coduo_qsort(candidates, (size_t)candidateCount, sizeof(candidates[0]),
                G_CompareActivateEntScores);
    candidateCount -= unusableItemCount;

    for (index = 0; index < candidateCount; index++) {
        gentity_t *candidate = candidates[index].ent;
        vec3_t center;
        trace_t trace;

        if (candidate->s.eType == ET_VEHICLE) {
            break;
        }

        game_compat_gentity_abs_center(candidate, center);
        if (candidate->s.eType == ET_TURRET) {
            DObjSkelMat tagMatrix;

            if (G_DObjGetWorldTagMatrix(candidate, "tag_aim",
                                        &tagMatrix) != 0) {
                game_compat_g_vector_copy3(tagMatrix.origin, center);
            }
        }

        trap_Trace(&trace, muzzle, vec3_origin, vec3_origin, center,
                   client->ps.psClientNum, ACTIVATE_VIS_TRACE_MASK);
        if (trace.entityNum != ENTITYNUM_WORLD) {
            break;
        }

        candidates[index].score += ACTIVATE_OCCLUDED_PENALTY;
        occludedCount++;
    }

    coduo_qsort(candidates, (size_t)candidateCount, sizeof(candidates[0]),
                G_CompareActivateEntScores);
    return candidateCount - occludedCount;
}

/* ------------------------------------------------------------------ */
/*  0x651ac G_Activate                                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x651ac, 751ac_G_Activate.c, VERIFY-GAME-HELPERS-ACTIVATE-PACKET-2026-06-17): DATAFLOW_VERIFIED - stationary and lock gates, active byte stores, low-word team token test, team-master redirect, and Use_BinaryMover argument order checked. */
void G_Activate(gentity_t *ent, gentity_t *activator)
{
    gentity_t *target;

    if (ent->s.apos.trType != TR_STATIONARY ||
        ent->s.pos.trType != TR_STATIONARY ||
        (*game_compat_g_binary_mover_in_use_byte(ent)) != 0 ||
        ent->doorLocked != 0) {
        return;
    }

    target = ent;
    if (ent->teamMaster != NULL &&
        (uint16_t)ent->teamName != 0 &&
        ent != ent->teamMaster) {
        target = ent->teamMaster;
    }

    (*game_compat_g_binary_mover_in_use_byte(target)) = 1;
    Use_BinaryMover(target, activator, activator);
}

/* ------------------------------------------------------------------ */
/*  0x575bb G_CheckForCursorHints                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x575bb, 675bb_G_CheckForCursorHints.c, VERIFY-GAME-HELPERS-ACTIVATE-PACKET-2026-06-17): DATAFLOW_VERIFIED - cursor reset stores, weapon-class gates, mount probe, activate candidate loop, hint/string/entity fields, item/mover/turret/vehicle branches, and early returns checked. */
void G_CheckForCursorHints(gentity_t *ent)
{
    activate_candidate_t candidates[ACTIVATE_MAX_CANDIDATES];
    gclient_t *client = ent->client;
    int hint = CURSOR_HINT_OFF;
    int hintData = 0;
    int hintString = -1;
    int candidateCount = 0;
    const weaponInfo_t *weaponInfo;
    int weaponClass;
    int index;

    client->ps.serverCursorHint = CURSOR_HINT_OFF;
    client->ps.serverCursorHintVal = 0;
    client->ps.cursorHintEntNum = ENTITYNUM_NONE;

    if (ent->health <= 0 ||
        (*game_compat_g_binary_mover_in_use_byte(ent)) != 0) {
        return;
    }

    client->ps.serverCursorHintString = -1;
    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(client->ps.currentWeapon);
    weaponClass = weaponInfo->weaponClass;
    if (((client->ps.playerStateFlags & CURSOR_STANCE_FLAG) != 0 &&
         weaponClass == WEAPCLASS_LMG) ||
        (client->ps.entityStateFlags & EF_IN_VEHICLE) != 0) {
        return;
    }

    candidateCount = G_GetActivateEnt(ent, candidates);
    if (weaponClass == WEAPCLASS_LMG) {
        vec3_t yawAngles = {0.0f, client->ps.viewAngles[1], 0.0f};
        vec3_t forward;
        vec3_t mountPoint;
        int mountHintData = 0;
        int contents;

        AngleVectors(yawAngles, forward, NULL, NULL);
        /* Stock 0x577ca..0x577e0: forward[k]*15.0 kept 80-bit, + currentOrigin[k],
         * one store -> shim (mul+add). */
#if EMULATE_X87
        for (int k = 0; k < 3; k++) {
            mountPoint[k] = x87f_store_f32(x87f_add(
                x87f_mul(x87f_load_f32(forward[k]), x87f_load_f32(15.0f)),
                x87f_load_f32(ent->currentOrigin[k])));
        }
#else
        mountPoint[0] = ent->currentOrigin[0] + forward[0] * 15.0f;
        mountPoint[1] = ent->currentOrigin[1] + forward[1] * 15.0f;
        mountPoint[2] = ent->currentOrigin[2] + forward[2] * 15.0f;
#endif
        /* 0x577e6-0x577f4: the +1.0f lift is a second float32 store on
         * the already-rounded mountPoint[2] (single add -> native). */
        mountPoint[2] += 1.0f;

        contents = trap_PointContents(mountPoint, PASS_ENTITY_NONE,
                                      CURSOR_MOUNT_CONTENTS);
        if (contents != 0 ||
            G_CheckPointInsideTriggerMount(ent, mountPoint, &mountHintData) != 0) {
            hint = CURSOR_HINT_LMG;
            hintData = mountHintData;
            if (game_compat_g_weapon_info_has_hint_string(weaponInfo)) {
                hintString = weaponInfo->hintStringIndex;
            }
        }

        client->ps.serverCursorHint = hint;
        client->ps.serverCursorHintVal = hintData;
        client->ps.serverCursorHintString = hintString;
    }

    for (index = 0; index < candidateCount; index++) {
        gentity_t *candidate = candidates[index].ent;

        client->ps.cursorHintEntNum = (uint16_t)candidate->s.number;
        if (candidate == NULL) {
            break;
        }

        if (candidate->s.number == ENTITYNUM_WORLD) {
            if ((client->ps.cursorHintFlags & CURSOR_FLAG_WORLD_HINT) != 0 &&
                (client->ps.playerStateFlags &
                 CURSOR_PLAYERSTATE_WORLD_HINT_BLOCKED) == 0) {
                hint = CURSOR_HINT_LADDER;
            }
            break;
        }

        if (candidate->client != NULL) {
            break;
        }

        if (candidate->s.eType == ET_GENERAL) {
            if (candidate->scriptClassname == scr_const_trigger_use) {
                hint = candidate->s.cursorHint;
                if (candidate->s.cursorHint != 0 &&
                    candidate->s.hintStringIndex != CURSOR_HINT_STRING_INHERIT) {
                    hintString = candidate->s.hintStringIndex;
                }
            }
        } else if (candidate->s.eType == ET_TURRET) {
            if (G_IsTurretUsable(candidate, ent) != 0) {
                const weaponInfo_t *turretWeaponInfo =
                    (const weaponInfo_t *)BG_GetInfoForWeapon(
                        candidate->s.weapon);

                hint = CURSOR_HINT_MG42;
                if (game_compat_g_weapon_info_has_hint_string(turretWeaponInfo)) {
                    hintString = turretWeaponInfo->hintStringIndex;
                }
            } else {
                continue;
            }
        } else if (candidate->s.eType == ET_VEHICLE) {
            if (G_IsVehicleUsable(candidate, ent) != 0) {
                const vehicle_state_t *vehicle =
                    (const vehicle_state_t *)candidate->vehicle;

                hint = CURSOR_HINT_ACTIVATE;
                hintString = vehicle->hintStringIndex;
            } else {
                continue;
            }
        } else if (candidate->s.eType == ET_ITEM) {
            const gitem_t *itemInfo = (const gitem_t *)candidate->itemInfo;
            itemType_t itemType = itemInfo->type;
            int itemWeapon = itemInfo->weapon;

            if (itemType == IT_WEAPON) {
                if (Com_BitCheck(client->ps.weaponBits, itemWeapon) == 0) {
                    hint = itemWeapon + CURSOR_HINT_WEAPON_BASE;
                } else {
                    hint = itemWeapon + CURSOR_HINT_OWNED_WEAPON_BASE;
                }
            } else if (itemType == IT_AMMO) {
                hint = itemWeapon + CURSOR_HINT_OWNED_WEAPON_BASE;
            } else if (itemType == IT_HEALTH) {
                hint = CURSOR_HINT_HEALTH;
            }
        } else if (candidate->s.eType == ET_MOVER) {
            uint16_t classname = candidate->scriptClassname;
            uint8_t moverState =
                candidate->moverState;
            uint8_t moverFlags =
                (uint8_t)candidate->flags;

            if (classname == scr_const_func_door_rotating) {
                if (moverState == 7 ||
                    (moverState == 8 && (moverFlags & 0x80u) != 0)) {
                    hint = CURSOR_HINT_DOOR;
                    if (candidate->doorLocked != 0) {
                        hint = CURSOR_HINT_DOOR_LOCKED;
                    }
                }
            } else if (classname == scr_const_func_door) {
                if (moverState == 0 ||
                    (moverState == 1 && (moverFlags & 0x80u) != 0)) {
                    hint = CURSOR_HINT_DOOR;
                    if (candidate->doorLocked != 0) {
                        hint = CURSOR_HINT_DOOR_LOCKED;
                    }
                }
            }
        }

        if (candidate->s.cursorHint > CURSOR_HINT_OFF &&
            hint != CURSOR_HINT_OFF) {
            hint = candidate->s.cursorHint;
        }
        break;
    }

    client->ps.serverCursorHint = hint;
    client->ps.serverCursorHintVal = hintData;
    client->ps.serverCursorHintString = hintString;
    if (client->ps.serverCursorHint == CURSOR_HINT_OFF) {
        client->ps.cursorHintEntNum = ENTITYNUM_NONE;
    }
}

/* ------------------------------------------------------------------ */
/*  0x57c8b G_CheckForPreventFriendlyFire                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x57c8b, 67c8b_G_CheckForPreventFriendlyFire.c, VERIFY-GAME-HELPERS-ACTIVATE-PACKET-2026-06-17): DATAFLOW_VERIFIED - lookAtEntity clear, active-state gate, muzzle/end vectors, priority-map short-circuit, trace masks, trigger_friendlyfire classname gate, and side effects checked. */
void G_CheckForPreventFriendlyFire(gentity_t *ent)
{
    weapon_muzzle_t muzzle;
    const weaponInfo_t *weaponInfo;
    int currentWeapon;
    const uint8_t *priorityMap;
    vec3_t end;
    trace_t trace;

    ent->client->lookAtEntity = NULL;
    if ((*game_compat_g_binary_mover_in_use_byte(ent)) != 0) {
        return;
    }

    CalcMuzzlePoints(ent, &muzzle);
    currentWeapon = ent->client->ps.currentWeapon;
    if (currentWeapon == 0) {
        priorityMap = bulletPriorityMap;
    } else {
        weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(currentWeapon);
        if (weaponInfo->ricochet == 0) {
            priorityMap = bulletPriorityMap;
        } else {
            priorityMap = riflePriorityMap;
        }
    }

    /* Stock 0x57d25..0x57d64: forward[k]*8192.0 kept 80-bit, + origin[k],
     * one store -> shim (mul+add). */
#if EMULATE_X87
    for (int k = 0; k < 3; k++) {
        end[k] = x87f_store_f32(x87f_add(
            x87f_mul(x87f_load_f32(muzzle.forward[k]), x87f_load_f32(8192.0f)),
            x87f_load_f32(muzzle.origin[k])));
    }
#else
    end[0] = muzzle.origin[0] + muzzle.forward[0] * 8192.0f;
    end[1] = muzzle.origin[1] + muzzle.forward[1] * 8192.0f;
    end[2] = muzzle.origin[2] + muzzle.forward[2] * 8192.0f;
#endif

    trap_LocationalTrace(&trace, muzzle.origin, end, ent->s.number,
                         PREVENT_FRIENDLY_FIRE_MASK, priorityMap);
    if (trace.entityNum >= ENTITYNUM_WORLD) {
        return;
    }

    trap_LocationalTrace(&trace, muzzle.origin, end, ent->s.number,
                         PREVENT_FRIENDLY_FIRE_CONFIRM_MASK, priorityMap);
    if (trace.entityNum < ENTITYNUM_WORLD) {
        gentity_t *hitEnt = &g_entities[trace.entityNum];

        if (hitEnt->scriptClassname == scr_const_trigger_friendlyfire) {
            ent->client->lookAtEntity = hitEnt;
            G_Trigger(hitEnt, ent);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x435cc  G_PlayerEvent                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x435cc, 535cc_G_PlayerEvent.c, VERIFY-GAMEHELPERS-EVENTS-2026-06-17): DATAFLOW_VERIFIED; recoil/FxOnTag event gate and BG_WeaponFireRecoil arguments match. */
void G_PlayerEvent(int entityNum, int event)
{
    vec3_t viewKick;
    gentity_t *ent;

    if ((event >= PLAYER_RECOIL_VIEWKICK_EVENT_MIN &&
         event <= PLAYER_RECOIL_VIEWKICK_EVENT_MAX) ||
        event == EV_FIRE_WEAPON_MG42) {
        ent = &g_entities[entityNum];
        BG_WeaponFireRecoil(&ent->client->ps,
                            ent->client->fireRecoilVelocity, viewKick);
    }
}

/* ------------------------------------------------------------------ */
/*  0x79f7a  G_AddPredictableEvent                                    */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x79f7a, 89f7a_G_AddPredictableEvent.c, VERIFY-GAMEHELPERS-EVENTS-2026-06-17): DATAFLOW_VERIFIED; client-gated predictable event append. */
void G_AddPredictableEvent(gentity_t *ent, int event, int eventParm)
{
    if (ent->client != NULL) {
        BG_AddPredictableEventToPlayerstate(event, eventParm, &ent->client->ps);
    }
}

/* ------------------------------------------------------------------ */
/*  0x79fbf  G_AddEvent                                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x79fbf, 89fbf_G_AddEvent.c, VERIFY-GAMEHELPERS-EVENTS-2026-06-17): DATAFLOW_VERIFIED; entity/client event rings and event-time stores via game_compat_g_set_entity_event_times helper. */
void G_AddEvent(gentity_t *ent, int event, int eventParm)
{
    int slot;

    if (ent->client == NULL) {
        slot = ent->s.eventCount & EVENT_RING_MASK;
        ent->s.events[slot] = event;
        ent->s.eventParms[slot] = eventParm;
        ent->s.eventCount = coduo_int32_from_bits(
            (uint32_t)ent->s.eventCount + UINT32_C(1));
    } else {
        slot = ent->client->ps.eventIndex & EVENT_RING_MASK;
        ent->client->ps.events[slot] = event;
        ent->client->ps.eventParms[slot] = eventParm;
        ent->client->ps.eventIndex = coduo_int32_from_bits(
            (uint32_t)ent->client->ps.eventIndex + UINT32_C(1));
    }

    game_compat_g_set_entity_event_times(ent);
}

/* ------------------------------------------------------------------ */
/*  0x7a09d  G_PlaySoundAliasAtPoint                                  */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7a09d, 8a09d_G_PlaySoundAliasAtPoint.c, VERIFY-GAMEHELPERS-EVENTS-2026-06-17): DATAFLOW_VERIFIED; nonzero sound alias temp entity. */
gentity_t *G_PlaySoundAliasAtPoint(const float *origin, uint8_t soundAlias)
{
    gentity_t *ent;

    if (soundAlias == 0) {
        return NULL;
    }

    ent = G_TempEntity(origin, EV_SOUND_ALIAS);
    ent->s.tempEffectId = soundAlias;
    return ent;
}

/* ------------------------------------------------------------------ */
/*  0x7a0f6  G_PlaySoundAlias                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7a0f6, 8a0f6_G_PlaySoundAlias.c, VERIFY-GAMEHELPERS-EVENTS-2026-06-17): DATAFLOW_VERIFIED; nonzero sound alias event. */
/*
 * Play a sound alias on an entity by adding a sound event.
 *
 * RECOVERED(UO-GAME-UNK-0176): Sound alias is added as event EV_SOUND_ALIAS (0xb1).
 */
void G_PlaySoundAlias(gentity_t *ent, uint8_t soundAlias)
{
    if (soundAlias != 0) {
        G_AddEvent(ent, EV_SOUND_ALIAS, soundAlias);
    }
}

/* ------------------------------------------------------------------ */
/*  0x7a135  G_AnimScriptSound                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7a135, 8a135_G_AnimScriptSound.c, VERIFY-GAMEHELPERS-EVENTS-2026-06-17): DATAFLOW_VERIFIED; entity lookup, sound alias index, and playback. */
void G_AnimScriptSound(int entityNum, const char *soundAliasName)
{
    gentity_t *ent = &g_entities[entityNum];

    G_PlaySoundAlias(ent, G_SoundAliasIndex(soundAliasName));
}

/* ------------------------------------------------------------------ */
/*  0x7a387 DebugLine                                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7a387, 8a387_DebugLine.c, VERIFY-GAMEHELPERS-EVENTS-2026-06-17): DATAFLOW_VERIFIED; constant zero return. */
int DebugLine(void)
{
    return 0;
}

/* ------------------------------------------------------------------ */
/* 0x77a8c G_LocalizedStringIndex */
/* VERIFIED_DECOMPILER(0x77a8c, 87a8c_G_LocalizedStringIndex.c, VERIFY-GAMEHELPERS-EVENTS-2026-06-17): DATAFLOW_VERIFIED; empty-string guard and localized configstring lookup. */
int G_LocalizedStringIndex(const char *value)
{
    if (value[0] == '\0') {
        return 0;
    }

    return G_FindConfigstringIndex(
        value,
        CS_LOCALIZED_STRINGS,
        CS_LOCALIZED_STRINGS_COUNT,
        level.spawning != 0,
        "localized string");
}

/* 0x77aed G_ShaderIndex */
/* VERIFIED_DECOMPILER(0x77aed, 87aed_G_ShaderIndex.c, VERIFY-GAMEHELPERS-EVENTS-2026-06-17): DATAFLOW_VERIFIED; shader configstring lookup; Ghidra's void inference drops the propagated return value. */
int G_ShaderIndex(const char *name)
{
    return G_FindConfigstringIndex(
        name,
        CS_SHADERS,
        CS_SHADERS_COUNT,
        level.spawning != 0,
        "shader");
}

/* 0x77c72 G_TagIndex */
/* VERIFIED_DECOMPILER(0x77c72, 87c72_G_TagIndex.c, VERIFY-GAMEHELPERS-EVENTS-2026-06-17): DATAFLOW_VERIFIED; tag configstring lookup with forced create; Ghidra's void inference drops the propagated return value. */
int G_TagIndex(const char *tagName)
{
    return G_FindConfigstringIndex(
        tagName,
        CS_TAGS,
        CS_TAGS_COUNT,
        qtrue,
        NULL);
}

/* 0x77cb5 G_EffectIndex */
/* VERIFIED_DECOMPILER(0x77cb5, 87cb5_G_EffectIndex.c, VERIFY-GAMEHELPERS-EVENTS-2026-06-17): DATAFLOW_VERIFIED; effect configstring lookup; Ghidra's void inference drops the propagated return value. */
int G_EffectIndex(const char *name)
{
    /* NOT_FROM_ORIGINAL_SOURCE: require an effect key that fits the client
     * scheduler field before its configstring is published. */
    if (name != NULL &&
        strcspn(name, ".") >= FX_EFFECT_TEMPLATE_NAME_CAPACITY) {
        G_Error("G_EffectIndex: effect name is too long");
    }

    return G_FindConfigstringIndex(
        name,
        CS_EFFECTS,
        CS_EFFECTS_COUNT,
        level.spawning != 0,
        "effect");
}

/* 0x77cff G_ShellShockIndex */
/* VERIFIED_DECOMPILER(0x77cff, 87cff_G_ShellShockIndex.c, VERIFY-GAMEHELPERS-EVENTS-2026-06-17): DATAFLOW_VERIFIED; shellshock configstring lookup with forced create; Ghidra's void inference drops the propagated return value. */
int G_ShellShockIndex(const char *name)
{
    return G_FindConfigstringIndex(
        name,
        CS_SHELLSHOCKS,
        CS_SHELLSHOCKS_COUNT,
        qtrue,
        NULL);
}

/*  0x7a391 G_SetConstString                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7a391, 8a391_G_SetConstString.c, VERIFY-GAMEHELPERS-EVENTS-2026-06-17): DATAFLOW_VERIFIED; clear old string and assign SL_GetString result. */
void G_SetConstString(uint16_t *slot, const char *value)
{
    Scr_SetString(slot, 0);
    *slot = SL_GetString(value, 0);
}

/* ------------------------------------------------------------------ */
/*  0x7a3d7 G_BackupSpawnVars                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7a3d7, 8a3d7_G_BackupSpawnVars.c, VERIFY-GAME-HELPERS-PARSE-INFO-2026-06-17): DATAFLOW_VERIFIED; hunk-copy spawn text and remap pair pointers. */
void G_BackupSpawnVars(gentity_t *ent)
{
    level_locals_t *lvl = &level;
    char *spawnText = lvl->spawnText;
    char **spawnVarPairs = lvl->spawnVarPairSlots;

    ent->savedSpawnText = trap_Hunk_AllocAlignInternal((size_t)lvl->spawnTextLength, 1);
    ent->savedSpawnTextLength = lvl->spawnTextLength;
    memcpy(ent->savedSpawnText, spawnText, (size_t)lvl->spawnTextLength);

    ent->savedSpawnVarPairs =
        trap_Hunk_AllocAlignInternal((size_t)lvl->spawnVarCount * 2u * sizeof(char *), 1);
    ent->savedSpawnVarCount = lvl->spawnVarCount;

    for (int pairIndex = 0; pairIndex < lvl->spawnVarCount; pairIndex++) {
        for (int slot = 0; slot < 2; slot++) {
            int index = pairIndex * 2 + slot;
            ent->savedSpawnVarPairs[index] =
                &ent->savedSpawnText[spawnVarPairs[index] - spawnText];
        }
    }
}
