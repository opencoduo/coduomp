// Source: uo_cgame_mp_x86.dll 0x3001d6d0..0x3001d965
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001d6d0_3001d965.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <math.h>
#include <stdint.h>

/*
 * CG_UpdateCompassOrientation (0x3001d6d0) — advance the compass/objective-pointer
 * reference yaw (cg_compassRefYaw) toward the animated effect spin angle
 * (cg_refdefViewAngles[1]) using a critically-damped angular spring, integrated in
 * fixed <= 5 ms substeps. The velocity/angle twin of CG_UpdateHudSpinAngle
 * (0x3001d3a0): same integrator shape, different tuning constants, plus a one-shot
 * initialization flag and a per-frame max chase-rate clamp of 10 deg.
 *
 * NAME ADJUDICATION: the .mcode header's pre-hint "VEH_CheckPushClients" (a pure
 * size match against game_mp_uo, the WRONG DLL — win 0x295 vs matched 0x296) is
 * REJECTED. This function touches no vehicle/client-push state; it reads cg_time and
 * compass-spin globals and writes cg_compassRefYaw. The Mac
 * CG_UpdateCompassOrientation has the corresponding angle-spring integration
 * call graph, resolving the source name.
 *
 * ABI (proven from this body): NO arguments, no return (bare RET). The stack frame
 * is just SUB ESP,0x8 for two float/int locals plus a saved ESI; no incoming
 * argument slot is ever read.
 *
 * Callees (all with existing client_recovered.h decls):
 *   AngleSubtract(a,b)              0x3004bd70 — signed shortest a-b in (-180,180]
 *   AngleNormalize180(x)    0x3004be60 — BAMS fold to (-180,180]
 *   AngleNormalize360(x)            0x3004be30 — SHORT2ANGLE(ANGLE2SHORT(x)&0xffff)
 *
 * State globals (all owned by this function):
 *   cg_hudCompassSpringyPointers_vmCvar.integer (0x30450fec, int)    — one-shot init flag
 *   cg_compassSpinPrevTime    (0x30134ce4, int ms)  — cg.time of prev advance
 *   cg_compassRefYaw          (0x3048b5d4, float deg)— output reference yaw
 *   cg_compassRefVel          (0x3048b5d8, float deg/s)— spring velocity
 * Inputs:
 *   cg_refdefViewAngles[1] (0x30487acc, float deg) — animated target angle
 *   cg_time            (0x304831b0, ms)        — current client time
 */

/* .rdata float/double constants used by this function (dumped at the exact
 * addresses via objdump -s -j .rdata). */
#define COMPASS_MAX_DELTA_MS (500.0f)   /* 0x3007beec: snap if elapsed > 500 ms */
#define COMPASS_MAX_STEP (10.0)     /* 0x3007bd30 (double): cap chased error to 10 deg */
#define COMPASS_STEP_NEG (-10.0f)   /* 0x3007bda0: clamped step, negative side */
#define COMPASS_STEP_POS (10.0f)    /* 0x3007bda4: clamped step, positive side */
#define MS_PER_SEC (0.001f)   /* 0x3007bd94: ms -> seconds */
#define SETTLE_DELTA (0.5)      /* 0x3007bd28 (double): settle if |step| < 0.5 */
#define SETTLE_VEL (2.0)      /* 0x3007bde8 (double): ... and |vel| < 2.0 */
#define SPRING_ACCEL (1500.0f)  /* 0x3007bee8: velocity push per second */
#define SPRING_DAMP (3.0f)     /* 0x3007be5c: base damping vel*(1 - 3*stepSec) */
#define OVERSHOOT_DAMP (5.0f)     /* 0x3007bde0: extra damping when vel,step share sign */
#define BIAS_DAMP (2.0f)     /* FADD ST0,ST0 doubles the stepSec bias term */
#define COMPASS_VEL_MAX (2000.0f)  /* 0x3007bee4 / imm 0x44fa0000 */
#define COMPASS_VEL_MIN (-2000.0f) /* 0x3007bee0 / imm 0xc4fa0000 */
#define FZERO (0.0f)     /* 0x3007bcec */

void CG_UpdateCompassOrientation(void)
{
    /* 0x3001d6d0..0x3001d6e9: first run — snap the reference yaw straight to the
     * target angle and return. The init flag is only tested here, never written by
     * this function (an external one-shot must set it). */
    if (cg_hudCompassSpringyPointers_vmCvar.integer == 0) {
        cg_compassRefYaw = cg_refdefViewAngles[1];
        return;
    }

    int32_t prev = coduo_int32_from_bits(cg_compassSpinPrevTime); /* 0x3001d6ea */
    int32_t now = coduo_int32_from_bits(cg_time);                /* 0x3001d6ef */

    /* 0x3001d6f5/0x3001d6f7: nothing to do if no time has passed this frame. */
    if (prev == now) {
        return;
    }

    float target = cg_refdefViewAngles[1];   /* 0x3001d6fd: saved for the whole update */

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    /* 0x3001d708: if the signed clock appears to run backwards, resync and snap. */
    if (prev > now) {
        /* 0x3001d947 reset path: prevTime := now; refYaw := target; vel := 0. */
        cg_compassSpinPrevTime = (uint32_t)now;
        cg_compassRefYaw = target;
        cg_compassRefVel = 0.0f;
        return;
    }

    int32_t elapsed = coduo_int32_from_bits((uint32_t)now - (uint32_t)prev);

    /* 0x3001d716..0x3001d725: direct signed FILD; unlike HUD spin, no float spill. */
    if ((long double)elapsed > (long double)COMPASS_MAX_DELTA_MS) {
        /* 0x3001d947 reset path (same as the backwards case). */
        cg_compassSpinPrevTime = (uint32_t)now;
        cg_compassRefYaw = target;
        cg_compassRefVel = 0.0f;
        return;
    }

    /* 0x3001d72b retains the target, 0x3001d72d publishes the new time, and only
     * then does 0x3001d733 load the current yaw for AngleSubtract. */
    cg_compassSpinPrevTime = (uint32_t)now;
    float currentYaw = cg_compassRefYaw;

    /* 0x3001d73b: delta = shortest signed rotation from the current yaw to target.
     * FAITHFULNESS NOTE: the DLL keeps this raw st0 return in an x87 register for the
     * whole update -- the settle test (0x3001d79c FLD ST0/FABS), the newStep chain
     * (0x3001d7d1 FADD ST0,ST1) and the finish (0x3001d911 FADD target) all read it
     * unstored. AngleSubtract's float result is already a binary32 value, but the
     * caller retains that returned value in ST0; `long double` is the raw-register
     * carrier and does not change the shared function's source return type. */
    long double step = (long double)AngleSubtract(currentYaw, target);

    /* 0x3001d740..0x3001d76e: cap the chased error to +-10 deg per update. Only when
     * |delta| > 10 is it replaced by the sign-matched clamp; otherwise it is kept. */
    if (fabsl(step) > (long double)COMPASS_MAX_STEP) {
        /* 0x3001d754..0x3001d768: sign taken from the raw float bit pattern (>= 0). */
        float signProbe = (float)step;
        step = (signProbe >= 0.0f) ? (long double)COMPASS_STEP_POS : (long double)COMPASS_STEP_NEG;
    }

    /* 0x3001d76e/0x3001d770: nothing to do if no time elapsed (elapsed > 0 here). */
    int32_t remaining = elapsed;
    if (remaining <= 0) {
        /* 0x3001d911 finish. */
        cg_compassRefYaw = AngleNormalize360((float)(step + (long double)target));
        return;
    }

    /* 0x3001d776..0x3001d90b: integrate the spring in <= 5 ms substeps. */
    while (remaining > 0) {
        /* 0x3001d776..0x3001d78c: step_ms = min(remaining, 5); remaining -= step_ms. */
        int32_t step_ms;
        if (remaining <= 5) {
            step_ms = remaining;
            remaining = 0;
        } else {
            step_ms = 5;
            remaining -= 5;
        }

        /* 0x3001d78e..0x3001d798: stepSec = step_ms / 1000. */
        float stepSec = (float)((long double)step_ms * (long double)MS_PER_SEC);

        /* 0x3001d79c..0x3001d7c0: settle test. If the pointer is essentially at rest
         * (|step| < 0.5 AND |vel| < 2.0), snap the yaw to the target and stop. */
        if (fabsl(step) < (long double)SETTLE_DELTA && fabsl((long double)cg_compassRefVel) < (long double)SETTLE_VEL) {
            /* 0x3001d92c: refYaw := target; vel := 0. */
            cg_compassRefYaw = target;
            cg_compassRefVel = 0.0f;
            return;
        }

        /* 0x3001d7c6..0x3001d7dd: advance the working error by vel*stepSec + step,
         * folded to (-180, 180]; this becomes the new working step. The argument IS
         * rounded (0x3001d7d3 FSTP float [ESP] at the call boundary), but the raw st0
         * return is NOT: it is FCOM'd against 0 unstored at 0x3001d7dd/0x3001d83c and
         * carried in ST0 for the next substep. */
        float normalizeArg = (float)((long double)cg_compassRefVel * (long double)stepSec + step);
        long double newStep = (long double)AngleNormalize180(normalizeArg);

        /* 0x3001d7dd..0x3001d81c: form the candidate velocity in a working register (the
         * global is not rewritten until the final store below); drive it toward closing
         * `newStep`. long double: the whole per-substep velocity chain (accel/decel at
         * 0x3001d7ed/0x3001d80c, base damping at 0x3001d824, overshoot damping at
         * 0x3001d84e/0x3001d890, and the 2*stepSec bias) stays in st registers and is
         * rounded to float exactly once, at the store to cg_compassRefVel
         * (0x3001d864 / 0x3001d8a6); a float local would round at every step. */
        long double vel;
        if (newStep > FZERO) {
            /* 0x3001d7ed..0x3001d7fd: still ahead of target -> decelerate. */
            vel = (long double)cg_compassRefVel - (long double)stepSec * (long double)SPRING_ACCEL;
        } else if (newStep < FZERO) {
            /* 0x3001d80c..0x3001d81c: behind target -> accelerate. */
            vel = (long double)cg_compassRefVel + (long double)stepSec * (long double)SPRING_ACCEL;
        } else {
            /* 0x3001d81e: newStep == 0 -> velocity unchanged. */
            vel = cg_compassRefVel;
        }

        /* 0x3001d824..0x3001d830: base damping vel *= (1 - 3*stepSec). */
        vel = vel - (long double)SPRING_DAMP * ((long double)stepSec * vel);

        /* 0x3001d832..0x3001d909: overshoot/zero-crossing handling, keyed on the signs
         * of the damped vel and newStep (both compared against 0). The vel > 0 branch is
         * taken only for strictly positive vel; vel == 0 falls to the else (0x3001d847
         * JNZ is taken when vel <= 0). */
        step = newStep;
        if (vel > FZERO) {
            /* 0x3001d84e..0x3001d85a: if vel and newStep share the (+) sign, apply
             * extra damping vel *= (1 - 5*stepSec). */
            if (newStep > FZERO) {
                vel = vel - (long double)OVERSHOOT_DAMP * ((long double)stepSec * vel);
            }
            /* 0x3001d85c..0x3001d872: nudge vel down by 2*stepSec; clamp to 0 on cross. */
            cg_compassRefVel = (float)(vel - (long double)BIAS_DAMP * (long double)stepSec);
            if (cg_compassRefVel < FZERO) {
                cg_compassRefVel = 0.0f;
            }
        } else {
            /* 0x3001d88b..0x3001d89c: if vel and newStep share the (-) sign, extra damping.
             * (Reached for vel <= 0; the newStep < 0 test gates the extra damping.) */
            if (newStep < FZERO) {
                vel = vel - (long double)OVERSHOOT_DAMP * ((long double)stepSec * vel);
            }
            /* 0x3001d89e..0x3001d8bf: nudge vel up by 2*stepSec; clamp to 0 on cross. */
            cg_compassRefVel = (float)(vel + (long double)BIAS_DAMP * (long double)stepSec);
            if (cg_compassRefVel > FZERO) {
                cg_compassRefVel = 0.0f;
            }
        }

        /* 0x3001d8cd..0x3001d909: clamp vel magnitude to [-2000, 2000]. */
        if (cg_compassRefVel > COMPASS_VEL_MAX) {
            cg_compassRefVel = COMPASS_VEL_MAX;
        } else if (cg_compassRefVel < COMPASS_VEL_MIN) {
            cg_compassRefVel = COMPASS_VEL_MIN;
        }
        /* 0x3001d909/0x3001d90b: loop while ms remain. */
    }

    /* 0x3001d911..0x3001d921 finish: requantize (step + target) through 16-bit BAMS
     * and store it as the new reference yaw. */
    cg_compassRefYaw = AngleNormalize360((float)(step + (long double)target));
}
