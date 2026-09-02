// Source: uo_cgame_mp_x86.dll 0x30014ea0..0x3001518d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30014ea0_3001518d.mcode
//
// BG_CalculateWeaponPosition_BasePosition_angles — derive the stance/sprint
// weapon-rotation target from horizontal speed, ease state->moveOffset toward it,
// and add the hip-fire portion of that offset to the running weapon angles.
//
// The .mcode header's VEH_InitVehicle assignment is rejected: it came from a
// cross-module size match. This function reads weaponInfo_t's sprint/stand/ducked/
// prone rotation vectors and smoothing rates. The same-module PPC bank contains
// BG_CalculateWeaponPosition_BasePosition_angles, and the isolated server
// weaponInfo_t layout explicitly identifies these fields as that routine's inputs.
//
// Original i386 ABI: state in EDX and angles as one caller-cleaned stack argument.

#include "bg_weapon_position.h"
#include "bg_bob.h"
#include "bg_bob_binding.h"
#include "bg_vehicle.h"
#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

void BG_CalculateWeaponPosition_BasePosition_angles(pm_weapon_angle_state_t *state, vec3_t angles)
{
#if defined(WINDOWS_BEHAVIOR)
    weaponInfo_t **weaponTable = bg_weaponInfos;
    playerState_t *ps = state->ps;
    const weaponInfo_t *weapon = weaponTable[ps->currentWeapon];
    vec3_t *moveOffset = &state->moveOffset;
    const vec3_t *rotation;
    float threshold;
    /* fraction is never spilled by the binary: both the speed fraction
     * (0x30014f17 FDIVP) and the ADS fade (0x3001513e..0x30015140) stay on
     * the x87 stack through their compares and multiplies, so it is long
     * double (a float local would insert roundings the DLL does not have). */
    long double fraction;
    float target[3] = {0.0f, 0.0f, 0.0f};
    int i;

    /* 0x30014ebb..0x30014ef2: select the speed threshold for sprint or stance. */
    if ((ps->playerStateFlags & PMF_SPRINTING) != 0) {
        threshold = weapon->sprintRotMinSpeed;
        rotation = &weapon->sprintRot;
    } else if ((ps->entityStateFlags & EF_PRONE) != 0) {
        threshold = weapon->proneRotMinSpeed;
        rotation = &weapon->proneRot;
    } else if ((ps->entityStateFlags & EF_CROUCHING) != 0) {
        threshold = weapon->duckedRotMinSpeed;
        rotation = &weapon->duckedRot;
    } else {
        threshold = weapon->standRotMinSpeed;
        rotation = &weapon->standRot;
    }

    /* 0x30014ef2..0x30015013: the target is zero unless speed is above the
     * selected minimum and the weapon is not in state 5. Otherwise normalize
     * speed across [threshold, ps->speed], clamp to [0,1], and scale the selected
     * rotation vector. ADS removes this base-position rotation linearly. */
    if (state->speed > threshold && ps->weaponState != WEAPON_STATE_RELOADING) {
        fraction = ((long double)state->speed - (long double)threshold) / ((long double)ps->speed - (long double)threshold);
        if (fraction < 0.0f) {
            fraction = 0.0f;
        } else if (fraction > 1.0f) {
            fraction = 1.0f;
        }

        target[0] = (*rotation)[0] * fraction;
        target[1] = (*rotation)[1] * fraction;
        target[2] = (*rotation)[2] * fraction;
    }

    if (ps->adsFraction != 0.0f) {
        /* 0x30015028..0x3001502e: 1-ads stays in ST0 unrounded across the
         * three FMUL/FSTP pairs -- long double, not float. */
        long double hipFraction = 1.0f - (long double)ps->adsFraction;
        target[0] *= hipFraction;
        target[1] *= hipFraction;
        target[2] *= hipFraction;
    }

    /* 0x30015050..0x30015108: ease each persistent moveOffset component toward
     * its target. Prone viewheight uses the prone rate; every other height uses
     * posRotRate. Each direction enforces a MINIMUM per-frame step of
     * +/-(0.1*frameTime) -- the FCOM at 0x300150af/0x300150db replaces delta
     * with the limit only when delta is SMALLER in magnitude -- and overshoot
     * past the target snaps to the target. */
    for (i = 0; i < 3; ++i) {
        float current = (*moveOffset)[i];
        float rate;
        long double delta;
        float limit;

        if (current == target[i]) {
            continue;
        }

        rate = ((long double)ps->viewHeightCurrent == (long double)ps->proneViewHeight) ? weapon->posProneRotRate : weapon->posRotRate;
        /* delta stays on the x87 stack unrounded from the FMUL chain at
         * 0x3001507b..0x30015092 through the FADD at 0x300150c0/0x300150ec;
         * only the limit is spilled to a float slot ([ESP+0x10]). */
        delta = ((long double)target[i] - current) * rate * state->frameTime;

        /* 0x3001509e/0x300150a0: the direction branch keys on current vs
         * target (unordered joins the negative path, as `<` does here). */
        if (current < target[i]) {
            limit = state->frameTime * 0.1f;      /* 0x3007bf6c, FSTP [ESP+0x10] */
            if (delta < limit) {
                delta = limit;                   /* 0x300150ba: min step up */
            }
            long double currentWide = (long double)current + delta;
            current = (float)currentWide;        /* FADD [ECX]; FST [ECX] */
            if (currentWide > (long double)target[i]) {
                current = target[i];             /* 0x300150fb: overshoot snap */
            }
        } else {
            limit = state->frameTime * -0.1f;     /* 0x3007bf68 = -0.1f */
            if (delta > limit) {
                delta = limit;                   /* 0x300150e6: min step down */
            }
            long double currentWide = (long double)current + delta;
            current = (float)currentWide;
            if (currentWide < (long double)target[i]) {
                current = target[i];
            }
        }
        (*moveOffset)[i] = current;
    }

    /* 0x3001510e..0x30015183: full contribution at hip, fade to zero over the
     * first half of ADS, and no contribution once adsFraction reaches 0.5. */
    if (ps->adsFraction == 0.0f) {
        angles[0] += (*moveOffset)[0];
        angles[1] += (*moveOffset)[1];
        angles[2] += (*moveOffset)[2];
    } else if (ps->adsFraction < 0.5f) {
        /* 0x3001513e FADD ST0,ST0: the doubling is ads+ads (no 2.0 constant),
         * and the fade stays in ST0 unrounded across the three FMULs. */
        fraction = 1.0f - ((long double)ps->adsFraction + ps->adsFraction);
        angles[0] += (*moveOffset)[0] * fraction;
        angles[1] += (*moveOffset)[1] * fraction;
        angles[2] += (*moveOffset)[2] * fraction;
    }
#else
    playerState_t *ps = state->ps;
    const weaponInfo_t *weapon = BG_GetInfoForWeapon(ps->currentWeapon);
    const vec3_t *rotation;
    float threshold;
    vec3_t target = {0.0f, 0.0f, 0.0f};
    int32_t axis;

    if ((ps->playerStateFlags & PMF_SPRINTING) != 0) {
        threshold = weapon->sprintRotMinSpeed;
        rotation = &weapon->sprintRot;
    } else if ((ps->entityStateFlags & EF_PRONE) != 0) {
        threshold = weapon->proneRotMinSpeed;
        rotation = &weapon->proneRot;
    } else if ((ps->entityStateFlags & EF_CROUCHING) != 0) {
        threshold = weapon->duckedRotMinSpeed;
        rotation = &weapon->duckedRot;
    } else {
        threshold = weapon->standRotMinSpeed;
        rotation = &weapon->standRot;
    }

    if (state->speed > threshold && ps->weaponState != WEAPON_STATE_RELOADING) {
        float fraction;

#if EMULATE_X87
        fraction = x87f_store_f32(x87f_div(x87f_sub(x87f_load_f32(state->speed), x87f_load_f32(threshold)),
                                           x87f_sub(x87f_load_i32(ps->speed), x87f_load_f32(threshold))));
#else
        fraction = (state->speed - threshold) / ((long double)ps->speed - threshold);
#endif
        if (fraction < 0.0f) {
            fraction = 0.0f;
        } else if (fraction > 1.0f) {
            fraction = 1.0f;
        }

        target[0] = (*rotation)[0] * fraction;
        target[1] = (*rotation)[1] * fraction;
        target[2] = (*rotation)[2] * fraction;
    }

    if (ps->adsFraction != 0.0f) {
        const float hipFraction = 1.0f - ps->adsFraction;

        target[0] *= hipFraction;
        target[1] *= hipFraction;
        target[2] *= hipFraction;
    }

    for (axis = 0; axis < 3; ++axis) {
        if (state->moveOffset[axis] != target[axis]) {
            float step;
            const float rate = ps->viewHeightCurrent == (long double)ps->proneViewHeight ? weapon->posProneRotRate : weapon->posRotRate;

#if EMULATE_X87
            step = x87f_store_f32(x87f_mul(
                x87f_mul(x87f_sub(x87f_load_f32(target[axis]), x87f_load_f32(state->moveOffset[axis])), x87f_load_f32(state->frameTime)),
                x87f_load_f32(rate)));
#else
            step = (target[axis] - state->moveOffset[axis]) * state->frameTime * rate;
#endif

            if (state->moveOffset[axis] < target[axis]) {
#if EMULATE_X87
                if (x87f_lt(x87f_load_f32(step), x87f_mul(x87f_load_f32(state->frameTime), x87f_load_f32(0.1f)))) {
                    step = state->frameTime * 0.1f;
                }
#else
                if (step < state->frameTime * 0.1f) {
                    step = state->frameTime * 0.1f;
                }
#endif
                state->moveOffset[axis] += step;
                if (target[axis] < state->moveOffset[axis]) {
                    state->moveOffset[axis] = target[axis];
                }
            } else {
#if EMULATE_X87
                if (x87f_lt(x87f_mul(x87f_load_f32(state->frameTime), x87f_load_f32(-0.1f)), x87f_load_f32(step))) {
                    step = state->frameTime * -0.1f;
                }
#else
                if (state->frameTime * -0.1f < step) {
                    step = state->frameTime * -0.1f;
                }
#endif
                state->moveOffset[axis] += step;
                if (state->moveOffset[axis] < target[axis]) {
                    state->moveOffset[axis] = target[axis];
                }
            }
        }
    }

    if (ps->adsFraction == 0.0f) {
        angles[0] += state->moveOffset[0];
        angles[1] += state->moveOffset[1];
        angles[2] += state->moveOffset[2];
    } else if (ps->adsFraction < 0.5f) {
#if EMULATE_X87
        const float fraction =
            x87f_store_f32(x87f_sub(x87f_load_f32(1.0f), x87f_add(x87f_load_f32(ps->adsFraction), x87f_load_f32(ps->adsFraction))));
        for (axis = 0; axis < 3; ++axis) {
            angles[axis] = x87f_store_f32(
                x87f_add(x87f_mul(x87f_load_f32(state->moveOffset[axis]), x87f_load_f32(fraction)), x87f_load_f32(angles[axis])));
        }
#else
        const float fraction = 1.0f - (ps->adsFraction + ps->adsFraction);
        angles[0] += state->moveOffset[0] * fraction;
        angles[1] += state->moveOffset[1] * fraction;
        angles[2] += state->moveOffset[2] * fraction;
#endif
    }
#endif
}
