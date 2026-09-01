#include "bg_pmove.h"

#include "bg_weapon.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <math.h>
#include <stdint.h>

/*
 * The authoritative Windows cgame/game bodies are instruction-identical apart
 * from relocations and their dependency addresses:
 *
 *   uo_cgame_mp_x86.dll  PM_UpdateLean 0x3000c560
 *   uo_game_mp_x86.dll   PM_UpdateLean 0x2000c320
 *
 * Linux game RVA 0x0002ac45 retains the same button gates, stance selection,
 * easing state machine, trace arguments, inverse-lean clearance curve, and
 * final sign restoration.  Its unoptimized body retains calls to the shared
 * helpers and spills named binary32 locals that MSVC inlines or keeps live.
 * Those compiler choices do not require separate recovered source bodies.
 */
void PM_UpdateLean(playerState_t *ps, const usercmd_t *command,
                   pm_trace_fn_t traceFunc)
{
    int32_t leanInput = 0;
    float maxLean;
    float leanFraction;

    if ((command->wbuttons & (PM_WBUTTON_LEAN_LEFT |
                              PM_WBUTTON_LEAN_RIGHT)) != 0 &&
        (ps->playerStateFlags & PMF_FOLLOW) == 0 &&
        ps->pmType < PM_TYPE_DEAD &&
        (ps->groundEntityNum != ENTITYNUM_NONE ||
         ps->pmType == PM_TYPE_LINKED)) {
        if ((command->wbuttons & PM_WBUTTON_LEAN_LEFT) != 0) {
            leanInput = -1;
        }
        if ((command->wbuttons & PM_WBUTTON_LEAN_RIGHT) != 0) {
            leanInput++;
        }
    }

    if ((ps->entityStateFlags & EF_RESTRICTED_MASK) != 0 ||
        ((ps->playerStateFlags & PMF_ADS) != 0 &&
         BG_GetInfoForWeapon(ps->currentWeapon)->weaponClass ==
             WEAPCLASS_LMG)) {
        leanInput = 0;
    }

    maxLean = PM_GetEffectiveStance(ps) == EFFECTIVE_STANCE_PRONE
                  ? 0.25f
                  : 0.5f;
    leanFraction = ps->leanFraction;

    if (leanInput == 0) {
        if (leanFraction > 0.0f) {
#if EMULATE_X87
            leanFraction = x87f_store_f32(x87f_sub(
                x87f_load_f32(leanFraction),
                x87f_mul(
                    x87f_div(x87f_load_i32(pml.msec),
                             x87f_load_f32(280.0f)),
                    x87f_load_f32(maxLean))));
#else
            leanFraction = (float)(
                (long double)leanFraction -
                (long double)pml.msec / 280.0L *
                    (long double)maxLean);
#endif
            if (leanFraction < 0.0f) {
                leanFraction = 0.0f;
            }
        } else if (leanFraction < 0.0f) {
#if EMULATE_X87
            leanFraction = x87f_store_f32(x87f_add(
                x87f_load_f32(leanFraction),
                x87f_mul(
                    x87f_div(x87f_load_i32(pml.msec),
                             x87f_load_f32(280.0f)),
                    x87f_load_f32(maxLean))));
#else
            leanFraction = (float)(
                (long double)leanFraction +
                (long double)pml.msec / 280.0L *
                    (long double)maxLean);
#endif
            if (leanFraction > 0.0f) {
                leanFraction = 0.0f;
            }
        }
    } else if (leanInput < 0) {
        if (leanFraction > -maxLean) {
#if EMULATE_X87
            leanFraction = x87f_store_f32(x87f_sub(
                x87f_load_f32(leanFraction),
                x87f_mul(
                    x87f_div(x87f_load_i32(pml.msec),
                             x87f_load_f32(350.0f)),
                    x87f_load_f32(maxLean))));
#else
            leanFraction = (float)(
                (long double)leanFraction -
                (long double)pml.msec / 350.0L *
                    (long double)maxLean);
#endif
        }
        if (leanFraction < -maxLean) {
            leanFraction = -maxLean;
        }
    } else {
        if (leanFraction < maxLean) {
#if EMULATE_X87
            leanFraction = x87f_store_f32(x87f_add(
                x87f_load_f32(leanFraction),
                x87f_mul(
                    x87f_div(x87f_load_i32(pml.msec),
                             x87f_load_f32(350.0f)),
                    x87f_load_f32(maxLean))));
#else
            leanFraction = (float)(
                (long double)leanFraction +
                (long double)pml.msec / 350.0L *
                    (long double)maxLean);
#endif
        }
        if (leanFraction > maxLean) {
            leanFraction = maxLean;
        }
    }

    ps->leanFraction = leanFraction;
    if (ps->leanFraction != 0.0f || isnan(ps->leanFraction)) {
        trace_t leanTrace;
        vec3_t start;
        vec3_t end;
        const vec3_t mins = {-8.0f, -8.0f, -8.0f};
        const vec3_t maxs = {8.0f, 8.0f, 8.0f};
        float blockedFraction;

        end[0] = ps->psOrigin[0];
        end[1] = ps->psOrigin[1];
#if EMULATE_X87
        end[2] = x87f_store_f32(x87f_add(
            x87f_load_f32(ps->psOrigin[2]),
            x87f_load_f32(ps->viewHeightCurrent)));
#else
        end[2] = ps->psOrigin[2] + ps->viewHeightCurrent;
#endif
        start[0] = end[0];
        start[1] = end[1];
        start[2] = end[2];
        AddLeanToPosition(end, ps->viewAngles[1],
                          (float)PM_FloatSign(ps->leanFraction),
                          16.0f, 20.0f);

        traceFunc(&leanTrace, start, mins, maxs, end,
                  ps->psClientNum, MASK_PLAYERSOLID);
        blockedFraction = UnGetLeanFraction(leanTrace.fraction);
        if (blockedFraction < PM_FloatAbs(ps->leanFraction)) {
#if EMULATE_X87
            ps->leanFraction = x87f_store_f32(x87f_mul(
                x87f_load_i32(PM_FloatSign(ps->leanFraction)),
                x87f_load_f32(blockedFraction)));
#else
            ps->leanFraction = (float)(
                (long double)PM_FloatSign(ps->leanFraction) *
                (long double)blockedFraction);
#endif
        }
    }
}
