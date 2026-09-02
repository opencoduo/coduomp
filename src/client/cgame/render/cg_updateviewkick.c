// Source: uo_cgame_mp_x86.dll 0x3003f9f0..0x3003fb51
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003f9f0_3003fb51.mcode
//
// CG_UpdateViewKick — advance the persistent first-person view-kick spring.
//
// Naming: the .mcode header carried the size-guess "CG_FlameAdjustSpeed"
// (win size 0x161 vs matched 0x160). That name is REJECTED: the body never
// touches any flame/effect object. It centers a 3-component angular offset back
// toward zero using per-weapon "view-kick center speed" magnitudes and clamps
// the offset to +/-10 degrees. The weapon-info fields it multiplies by are the
// recovered-server weaponInfo_s::adsViewKickCenterSpeed (+0x404) and
// hipViewKickCenterSpeed (+0x44c); the name CG_UpdateViewKick is the proven
// role (exact original source name unproven).
//
// Machine-code facts pinned:
//   - dt substep: min(cg_frametime, 5) * 0.001f, cg_frametime an int (FILD).
//     Outer loop peels 5 ms at a time (SUB ESI,5 ; JG) until <= 0.
//   - inner loop over the 3 vec components (ECX from 0x3048b064 to <0x3048b070,
//     step 4), touching cg_viewKickAngles[i] at [ECX] and cg_viewKickVel[i] at
//     [ECX-0xc].
//   - constants from .rdata: 0.001f@0x3007bd94, 0.0f@0x3007bcec, 1.0f@0x3007bce0,
//     0.5f@0x3007bce8, 2400.0f@0x3007bdac, -1.0f@0x3007bdb0, 0.06f@0x3007bda8,
//     10.0f@0x3007bda4, -10.0f@0x3007bda0, 10.0(double)@0x3007bd30.
//   - FUCOMPP/FCOMP + FNSTSW AX + TEST AH,{0x44,0x41,0x05,0x01} encode the
//     comparisons. The two TEST-AH patterns that deliberately include unordered
//     results are modeled explicitly below rather than collapsed to ordinary C.

#include "client/cgame/client_recovered.h"

#include <math.h>

void CG_UpdateViewKick(void)
{
    /* ESI = cg_frametime (elapsed ms this client frame); return if <= 0. */
    int32_t remaining = cg_frametime;
    if (remaining <= 0) {
        return;
    }

    do {
        /* MOV [ESP+4],5 ; if (ESI <= 5) MOV [ESP+4],ESI  -> step = min(remaining,5).
         * FILD [ESP+4] ; FMUL 0.001f -> dt in seconds (this substep). */
        int32_t stepMs = (remaining > 5) ? 5 : remaining;
        const long double dt = (long double)stepMs * (long double)0.001f;

        /* Inner loop: ECX walks cg_viewKickAngles[0..2]; [ECX-0xc] is the
         * matching cg_viewKickVel[i] (the two vec3s are adjacent in memory). */
        for (int i = 0; i < 3; ++i) {
            float vel = cg_viewKickVel[i];
            float ang = cg_viewKickAngles[i];

            /* A==0 && B==0  -> nothing to do this component (JNP 0x3003fb32).
             * The velocity-integration block (0x3003fa50) runs only when the
             * angle is nonzero; when the angle is zero but velocity is nonzero
             * it is skipped straight to the position integrate (0x3003faab). */
            if (ang == 0.0f && vel == 0.0f) {
                continue;
            }

            if (ang != 0.0f) {
                /* signFactor = (ang > 0) ? -1.0f : 1.0f  (restore toward 0).
                 * FLD ang ; FCOMP 0.0 ; TEST AH,0x41 ; JNZ -> ang<0 branch. */
                float signFactor = (ang > 0.0f) ? -1.0f : 1.0f;

                /* Centering acceleration magnitude:
                 *   - no weapon selected            -> 2400.0f
                 *   - adsFraction >= 0.5f           -> weapon->adsViewKickCenterSpeed
                 *   - adsFraction <  0.5f           -> weapon->hipViewKickCenterSpeed */
                float accel;
                if (cg_predictedPlayerState.currentWeapon == 0) {
                    accel = signFactor * 2400.0f;
                } else {
                    /* MOV EAX,[0x30487980] is issued unconditionally before the
                     * ADS-fraction branch; the field selected is the only diff. */
                    const weaponInfo_t *w = cg_currentWeaponInfo;
                    /* 0x3003fa7b fcomp 0.5f / 0x3003fa83 test ah,0x41 / 0x3003fa8b jne
                     * -> the ADS field (+0x404) is reached (fall-through) only when
                     * adsFraction > 0.5 STRICTLY; at ==0.5 (C3 set) the jne is taken to
                     * the hip field (+0x44c). A prior pass used >= 0.5f. */
                    if (cg_predictedPlayerState.adsFraction > 0.5f) {
                        accel = signFactor * w->adsViewKickCenterSpeed;
                    } else {
                        accel = signFactor * w->hipViewKickCenterSpeed;
                    }
                }

                /* FMUL st,st(1) (dt) ; FADD vel ; FSTP vel. */
                vel = (float)((long double)accel * dt + (long double)vel);
                cg_viewKickVel[i] = vel;
            }

            /* Position integrate (0x3003faab).
             * step = dt * vel ; if (step * ang < 0) step *= 0.06f  (damp when the
             * motion is toward center; TEST AH,0x5 / JP keeps step when ordered
             * and >= 0). */
            long double angleStep = dt * (long double)vel;
            if (angleStep * (long double)ang < (long double)0.0f) {
                angleStep *= (long double)0.06f;
            }

            /* newAng = step + ang. If newAng * ang < 0 the offset crossed zero:
             * clamp both offset and velocity to 0 (0x3003fb2b). */
            const long double newAng = angleStep + (long double)ang;
            const long double crossingProduct = newAng * (long double)ang;
            /* TEST AH,0x01 takes the reset path for both a negative product
             * (C0) and unordered/NaN (C0|C2|C3). */
            if (isnan(crossingProduct) || crossingProduct < (long double)0.0f) {
                cg_viewKickAngles[i] = 0.0f;
                cg_viewKickVel[i] = 0.0f;
                continue;
            }

            /* Store newAng. If it is exactly 0, zero the velocity too and move on
             * (FUCOMPP 0.0 ; JNP 0x3003fb2f). */
            const float storedNewAng = (float)newAng;
            cg_viewKickAngles[i] = storedNewAng;
            if (storedNewAng == 0.0f) {
                cg_viewKickVel[i] = 0.0f;
                continue;
            }

            /* If |newAng| <= 10.0 leave the value and keep the velocity
             * (FABS ; FCOMP 10.0(double) ; TEST AH,0x41 ; JNZ 0x3003fb32).
             * Otherwise clamp to +/-10.0 and zero the velocity. */
            float mag = fabsf(storedNewAng);
            /* FCOMP 10.0 / TEST AH,0x41 leaves unordered values untouched as
             * well as values at or below the clamp. */
            if (isnan(mag) || mag <= 10.0) {
                continue;
            }

            /* |newAng| > 10.0: clamp. FLD newAng ; FCOMP 0.0 ; TEST AH,0x41 ; JNZ
             * -> negative branch stores -10.0, else +10.0. */
            if (storedNewAng > 0.0f) {
                cg_viewKickAngles[i] = 10.0f;
            } else {
                cg_viewKickAngles[i] = -10.0f;
            }
            cg_viewKickVel[i] = 0.0f;
        }

        remaining -= 5;
    } while (remaining > 0);
}
