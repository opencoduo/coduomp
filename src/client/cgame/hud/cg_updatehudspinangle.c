// Source: uo_cgame_mp_x86.dll 0x3001d3a0..0x3001d6c6
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001d3a0_3001d6c6.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <math.h>
#include <stdint.h>

/*
 * CG_UpdateHudSpinAngle (0x3001d3a0) — advance the angle of the time-animated
 * spinning HUD element (cg_hudSpinAngle, 0x3048b5cc) toward a target angle using a
 * critically-damped angular spring, integrated in fixed <= 5 ms substeps.
 *
 * NAME ADJUDICATION: the .mcode header's pre-hint "PM_BeginWeaponChange" (a pure
 * size match, win 0x326 vs matched 0x325) is REJECTED. This function touches no
 * pmove/playerState/weapon state; it reads cg_time and HUD spin globals and writes
 * cg_hudSpinAngle. Role name CG_UpdateHudSpinAngle (already in client_recovered.h).
 *
 * ABI (proven from this body, superseding the earlier caller-observed guess): NO
 * arguments. The sole caller CG_DrawSpinningPic (0x3002f910) pre-stores four scaled
 * floats in the outgoing stack frame, but this function reads NONE of the incoming
 * argument slots — verified exhaustively: every [ESP + n] access resolves to one of
 * this function's own four locals (see the entry/prologue map below), never to the
 * caller args at entry+4/+8/+0xc. The return value in EAX is undefined and ignored.
 * Bare RET (no callee stack cleanup).
 *
 * Frame: SUB ESP,0x10 reserves four float locals; the function then PUSHes ESI/EBP/
 * EDI as saved registers around the AngleSubtract call. Mapping the reserved slots
 * to entry-relative offsets E-0x4, E-0x8, E-0xc, E-0x10:
 *   E-0x4  = `target`     : SHORT2ANGLE(ANGLE2SHORT(cg_refdefViewAngles[1] - baseTime))
 *   E-0x8  = `delta`      : signed shortest angle from current angle to `target`
 *   E-0xc  = `stepSec`    : substep length in seconds (step_ms / 1000)
 *   E-0x10 = `foldedDelta`: `delta` re-folded to (-180, 180] after each integration
 *
 * State globals (all exclusively owned by this function):
 *   cg_hudSpinAngle   (0x3048b5cc, float deg)  — output angle
 *   cg_hudSpinVel     (0x3048b5d0, float deg/s) — angular velocity being integrated
 *   cg_hudSpinPrevTime(0x30134ce0, int32 ms)    — cg_time of the previous advance
 * Inputs:
 *   cg_refdefViewAngles[1]  (0x30487acc, float deg) — animated source angle
 *   cg_hudSpinBaseTime  (0x3048b5c8, float)     — angle offset subtracted from it
 *   cg_time             (0x304831b0, ms)        — current client time
 */

/* .rdata float constants used by this function (dumped at the exact addresses). */
#define ANGLE2SHORT (182.04445f)      /* 0x3007bd60 = 65536/360 */
#define SHORT2ANGLE (0.0054931641f)   /* 0x3007bd5c = 360/65536 */
#define HUDSPIN_MAX_DELTA_MS (500.0f)   /* 0x3007beec: snap if elapsed > 500 ms */
#define MS_PER_SEC (0.001f)   /* 0x3007bd94: ms -> seconds */
#define ANGLE_HALF (180.0f)   /* 0x3007bd50 */
#define ANGLE_FULL (360.0f)   /* 0x3007bd54 */
#define SETTLE_DELTA (0.25)     /* 0x3007be30 (double): settle if |delta| < 0.25 */
#define SETTLE_VEL (1.0)      /* 0x3007bcf8 (double): ... and |vel| < 1.0 */
#define MS_PER_SEC_INV (1000.0f)  /* 0x3007be88: seconds -> ms (== step_ms) */
#define SPRING_DAMP (2.0f)     /* FADD ST0,ST0 doubles the vel*sec term */
#define OVERSHOOT_DAMP (3.5f)     /* 0x3007bef8: extra damping when vel and delta share sign */
#define HUDSPIN_VEL_MAX (30000.0f) /* 0x3007bef4 / +0x46ea6000 */
#define HUDSPIN_VEL_MIN (-30000.0f)/* 0x3007bef0 / +0xc6ea6000 */

void CG_UpdateHudSpinAngle(void)
{
    /* 0x3001d3a3..0x3001d3e3: target = BAMS-fold of (cg_refdefViewAngles[1] - baseTime). */
    long double targetRaw = ((long double)cg_refdefViewAngles[1] - (long double)cg_hudSpinBaseTime) * (long double)ANGLE2SHORT;
    int32_t targetInteger = coduo_fp_to_i32_extended(targetRaw);
    int32_t now = coduo_int32_from_bits(cg_time); /* 0x3001d3bb MOV ECX,[cg_time] */
    uint32_t targetPacked = (uint32_t)targetInteger & 0xffffu;
    long double targetPackedCarrier = (long double)(int32_t)targetPacked;
    int32_t prev = cg_hudSpinPrevTime;       /* 0x3001d3ce MOV EAX,[prevTime] */
    qboolean clockAppearsBackwards = prev > now;
    float targetPackedFloat = (float)targetPackedCarrier;
    float target = (float)((long double)targetPackedFloat * (long double)SHORT2ANGLE);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    /* 0x3001d3d3/0x3001d3e7: if the signed clock appears to run backwards, snap. */
    if (clockAppearsBackwards) {
        /* 0x3001d6a7 reset path: prevTime := now, angle := target, vel := 0. */
        cg_hudSpinPrevTime = now;
        cg_hudSpinAngle = target;
        cg_hudSpinVel = 0.0f;
        return;
    }

    int32_t elapsed = coduo_int32_from_bits((uint32_t)now - (uint32_t)prev);

    /* 0x3001d3f1..0x3001d40c: FILD then FSTP/reload as binary32 before comparing. */
    float elapsedFloat = (float)elapsed;
    if (elapsedFloat > HUDSPIN_MAX_DELTA_MS) {
        cg_hudSpinPrevTime = now;
        cg_hudSpinAngle = target;
        cg_hudSpinVel = 0.0f;
        return;
    }

    /* 0x3001d412..0x3001d425: both call arguments are copied before the time store. */
    float currentAngle = cg_hudSpinAngle;
    float deltaTarget = target;
    cg_hudSpinPrevTime = now;
    float delta = AngleSubtract(currentAngle, deltaTarget);

    /* 0x3001d433/0x3001d435: nothing to do if no time elapsed. */
    int32_t remaining = elapsed;
    if (remaining <= 0) {
        goto finish;
    }

    /* 0x3001d440..0x3001d653: integrate the spring in <= 5 ms substeps. */
    while (remaining > 0) {
        /* 0x3001d440..0x3001d456: step_ms = min(remaining, 5); remaining -= step. */
        int32_t step_ms;
        if (remaining <= 5) {
            step_ms = remaining;
            remaining = 0;
        } else {
            step_ms = 5;
            remaining -= 5;
        }

        /* 0x3001d458..0x3001d46a: the integer is rounded to float before scaling. */
        float stepMsFloat = (float)step_ms;
        float stepSec = (float)((long double)stepMsFloat * (long double)MS_PER_SEC);

        /* 0x3001d46e..0x3001d494: settle test. If the element is essentially at rest
         * (|delta| < 0.25 AND |vel| < 1.0), snap the angle to the target and stop. */
        if (fabsl((long double)delta) < (long double)SETTLE_DELTA && fabsl((long double)cg_hudSpinVel) < (long double)SETTLE_VEL) {
            /* 0x3001d694: vel := 0; angle := target (EBP held the entry `target`). */
            cg_hudSpinVel = 0.0f;
            cg_hudSpinAngle = target;
            return;
        }

        /* 0x3001d49a..0x3001d4ef: advance the angle by vel*stepSec + delta, folded to
         * (-180, 180]; this becomes the new working delta. */
        long double foldedRaw = (((long double)cg_hudSpinVel * (long double)stepSec) + (long double)delta) * (long double)ANGLE2SHORT;
        uint32_t foldedPacked = (uint32_t)coduo_fp_to_i32_extended(foldedRaw) & 0xffffu;
        float foldedPackedFloat = (float)(int32_t)foldedPacked;
        float foldedDelta = (float)((long double)foldedPackedFloat * (long double)SHORT2ANGLE);
        if (foldedDelta > ANGLE_HALF) {
            foldedDelta -= ANGLE_FULL;       /* 0x3001d4e3..0x3001d4ed */
        }
        delta = foldedDelta;                 /* 0x3001d4ff: delta := foldedDelta */

        /* 0x3001d4f1..0x3001d555: drive vel toward closing `delta`, then apply the
         * symmetric spring damping vel *= (1 - 2*stepSec). */
        if (foldedDelta > 0.0f) {
            /* 0x3001d50a..0x3001d51a: still ahead of target -> decelerate. */
            cg_hudSpinVel = (float)((long double)cg_hudSpinVel - (long double)stepSec * (long double)MS_PER_SEC_INV);
        } else if (foldedDelta < 0.0f) {
            /* 0x3001d51c/0x3001d52d..0x3001d537: behind target -> accelerate. */
            cg_hudSpinVel = (float)((long double)cg_hudSpinVel + (long double)stepSec * (long double)MS_PER_SEC_INV);
        }
        /* else foldedDelta == 0: 0x3001d52b JP skips the +/- adjustment. */

        /* 0x3001d543..0x3001d555: vel -= 2 * (vel * stepSec)  ==  vel*(1 - 2*stepSec). */
        cg_hudSpinVel =
            (float)((long double)cg_hudSpinVel - (long double)SPRING_DAMP * ((long double)cg_hudSpinVel * (long double)stepSec));

        /* 0x3001d55b..0x3001d613: overshoot/zero-crossing handling, keyed on the signs
         * of vel and delta (both compared against 0). */
        if (cg_hudSpinVel > 0.0f) {
            /* 0x3001d57a..0x3001d595: if vel and delta share the (+) sign, apply extra
             * damping vel *= (1 - 3.5*stepSec). */
            if (foldedDelta > 0.0f) {
                cg_hudSpinVel =
                    (float)((long double)cg_hudSpinVel - (long double)OVERSHOOT_DAMP * ((long double)cg_hudSpinVel * (long double)stepSec));
            }
            /* 0x3001d59b..0x3001d5c4: nudge vel down by stepSec; clamp to 0 on cross. */
            cg_hudSpinVel = (float)((long double)cg_hudSpinVel - (long double)stepSec);
            if (cg_hudSpinVel < 0.0f) {
                cg_hudSpinVel = 0.0f;
            }
        } else {
            /* 0x3001d5c9..0x3001d5e4: if vel and delta share the (-) sign, extra damping. */
            if (foldedDelta < 0.0f) {
                cg_hudSpinVel =
                    (float)((long double)cg_hudSpinVel - (long double)OVERSHOOT_DAMP * ((long double)cg_hudSpinVel * (long double)stepSec));
            }
            /* 0x3001d5ea..0x3001d613: nudge vel up by stepSec; clamp to 0 on cross. */
            cg_hudSpinVel = (float)((long double)cg_hudSpinVel + (long double)stepSec);
            if (cg_hudSpinVel > 0.0f) {
                cg_hudSpinVel = 0.0f;
            }
        }
        /* else vel == 0: 0x3001d5c9 path with delta >= 0 falls straight to the clamps. */

        /* 0x3001d615..0x3001d64f: clamp vel to [-30000, 30000]. */
        if (cg_hudSpinVel > HUDSPIN_VEL_MAX) {
            cg_hudSpinVel = HUDSPIN_VEL_MAX;
        } else if (cg_hudSpinVel < HUDSPIN_VEL_MIN) {
            cg_hudSpinVel = HUDSPIN_VEL_MIN;
        }
        /* 0x3001d651/0x3001d653: loop while ms remain. */
    }

finish:
    /* 0x3001d659..0x3001d68a: no store separates FADD/FMUL from Q_rint. */
    {
        long double finalRaw = ((long double)delta + (long double)target) * (long double)ANGLE2SHORT;
        uint32_t finalPacked = (uint32_t)coduo_fp_to_i32_extended(finalRaw) & 0xffffu;
        float finalPackedFloat = (float)(int32_t)finalPacked;
        cg_hudSpinAngle = (float)((long double)finalPackedFloat * (long double)SHORT2ANGLE);
    }
}
