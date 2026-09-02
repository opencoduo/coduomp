// Source: uo_cgame_mp_x86.dll 0x30015610..0x3001575e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30015610_3001575e.mcode
//
// One integration step of a bounded, damped 1-D spring that returns a value
// (*pValue) toward zero using a companion rate (*pRate). Not a UI parse handler.
//
// The mechanical `.mcode` name `ItemParse_cvarFloatList` is REJECTED. It is a
// size-only guess (win size 0x14e vs matched 0x150) from the cgame corpus, and
// the machine code disproves it outright: this function takes NO text-parse
// handle and NO itemDef pointer. Its ABI (proven at both call sites in
// FUN_30015760, e.g. 0x3001589b / 0x300158c2) is __fastcall with the value
// pointer in EDX and the rate pointer in ECX, plus six 32-bit float arguments
// pushed on the stack; the caller reclaims all six slots with `add esp,0x30`
// after two back-to-back calls. The body is pure x87 float integration/clamping
// over two adjacent float fields (caller passes edx+0x2c & edx+0x38, and
// eax+0x30 & eax+0x3c — value/rate pairs of some cgame object), returning a
// qboolean.
//
// Behavior (spring toward 0 with symmetric limits, per-step):
//   1. Settle test: if |value| < 0.25 and |rate| < 1.0, snap both to 0 and
//      report settled (return qtrue).
//   2. Integrate position: value += dt*rate, then clamp to [-posLimit,posLimit];
//      when the clamp is hit while the rate drives further into the bound, the
//      rate is zeroed.
//   3. Restoring accel: push the rate toward 0 by dt*springAccel, opposing the
//      sign of value (value==0 leaves the rate unchanged).
//   4. Damping: rate *= (1 - dt*dampCoef), then a further dt*dampAccel pull
//      toward 0; if that pull crosses zero the rate is clamped to 0.
//   5. Clamp rate to [-velLimit, velLimit].
// All non-settled paths return qfalse.
//
// Name is role-derived and UNPROVEN as the exact original symbol — the two
// callers (FUN_30015760, itself unreconstructed) are the only evidence and were
// not fully reconstructed here. Kept descriptive rather than address-shaped.
//
// x87 compare idioms used below (after FCOMP/FCOM; FNSTSW AX):
//   TEST AH,0x41; JNZ  -> branch taken when ST0 <= mem   (C3|C0)
//   TEST AH,0x41; JZ   -> branch taken when ST0 >  mem
//   TEST AH,0x05; JP   -> branch taken when ST0 >= mem   (C0|C2, ordered)
//   TEST AH,0x05; JNP  -> branch taken when ST0 <  mem
//
// Float constants (exact .rdata addresses; do not infer from neighbors):
//   0.25 : double @0x3007be30
//   1.0  : double @0x3007bcf8
//   0.0f : float  @0x3007bcec

#include "bg_weapon_position.h"
#include "bg_bob.h"
#include "bg_bob_binding.h"
#include "bg_vehicle.h"
#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"
#include <math.h>

qboolean BG_CalculateWeaponPosition_GunRecoil_SingleAngle(float *pValue /*EDX*/, float *pRate /*ECX*/, float dt,          /* [esp+0xc]  */
                                                          float posLimit,    /* [esp+0x10] */
                                                          float springAccel, /* [esp+0x14] */
                                                          float velLimit,    /* [esp+0x18] */
                                                          float dampCoef,    /* [esp+0x1c] */
                                                          float dampAccel)   /* [esp+0x20] */
{
#if defined(WINDOWS_BEHAVIOR)
    /* [esp+0x4] scratch: the float-rounded integrated value, reused by the
     * lower-bound compare at 0x3001567e. The upper-bound compare at 0x30015650
     * instead consumes the still-live x87 value before that value is popped. */
    float integrated;

    /* --- 0x30015610: settle test (|value| < 0.25 && |rate| < 1.0) --- */
    /* FABS |value| >= 0.25 -> skip to integrate (JP at 0x30015623). */
    if (fabsf(*pValue) < 0.25 /* double @0x3007be30 */) {
        /* FABS |rate| >= 1.0 -> skip to integrate (JP at 0x30015634). */
        if (fabsf(*pRate) < 1.0 /* double @0x3007bcf8 */) {
            *pValue = 0.0f;              /* MOV [EDX],ESI (ESI==0) */
            *pRate = 0.0f;               /* MOV [ECX],ESI */
            return qtrue;                /* MOV EAX,1 */
        }
    }

    /* --- 0x30015642: integrate position and clamp to [-posLimit, posLimit] --- */
    long double integratedWide = (long double)dt * (*pRate) + (*pValue); /* FLD dt; FMUL [ECX]; FADD [EDX] */
    integrated = (float)integratedWide;          /* FST [esp+0x4] */
    *pValue = (float)integratedWide;             /* FST [EDX], x87 value retained */

    if (integratedWide > posLimit) {        /* FCOMP posLimit; JNZ==le -> below */
        /* 0x3001565b: overshot upper bound */
        *pValue = posLimit;                 /* MOV [EDX],posLimit */
        if (*pRate > 0.0f) {                /* FCOMP 0.0f @0x3007bcec; le skips */
            *pRate = 0.0f;                  /* 0x30015670 */
        }
    } else {
        /* 0x30015678: integrated <= posLimit; check lower bound -posLimit.
         * Compares the retained integrated value against -posLimit. */
        if (integrated < -posLimit) {       /* FLD posLimit;FCHS;FLD int;FCOMP; JP==ge skips */
            *pValue = -posLimit;            /* FSTP [EDX] stores the -posLimit on stack */
            if (*pRate < 0.0f) {            /* FCOMP 0.0f; JP==ge skips */
                *pRate = 0.0f;              /* 0x3001569c */
            }
        }
        /* 0x300156a4: else path discards -posLimit (FSTP ST0). */
    }

    /* --- 0x300156a6: restoring acceleration toward 0, opposing sign(value) --- */
    if (*pValue > 0.0f) {                    /* FLD [EDX];FCOMP 0.0f; JNZ==le -> 0x300156c1 */
        *pRate = *pRate - dt * springAccel; /* FLD dt;FMUL springAccel;FSUBR [ECX];FSTP [ECX] */
    } else if (*pValue < 0.0f) {             /* 0x300156c1: FLD [EDX];FCOMP 0.0f; JP==ge -> skip */
        *pRate = *pRate + dt * springAccel; /* FADD [ECX];FSTP [ECX] */
    }
    /* *pValue == 0.0f leaves *pRate unchanged (0x300156dc reached directly). */

    /* --- 0x300156dc: damping --- */
    {
        /* damped = *pRate - dt*(*pRate)*dampCoef  == *pRate * (1 - dt*dampCoef) */
        long double damped = *pRate - (long double)dt * (*pRate) * dampCoef; /* FLD dt;FMUL [ECX];FMUL dampCoef;FSUBR [ECX] */
        long double extra = (long double)dt * dampAccel; /* FLD dt;FMUL dampAccel (pushed after FCOM) */
        *pRate = (float)damped; /* FST [ECX], x87 value retained */

        /* Sign of `damped` (FCOM 0.0f at 0x300156e8, read at 0x300156fa) selects
         * whether the additional dampAccel pull subtracts or adds; either way it
         * pulls toward 0, and if it overshoots 0 the rate snaps to 0. */
        if (damped > 0.0f) { /* JNZ==le -> 0x30015712 (add branch) */
            long double r = damped - extra; /* FSUBP; FST [ECX] */
            *pRate = (float)r;
            if (r < 0.0f) { /* FCOMP 0.0f; JP==ge keeps, else fall */
                *pRate = 0.0f; /* 0x30015723 */
            }
        } else {
            long double r = damped + extra; /* FADDP; FST [ECX] */
            *pRate = (float)r;
            if (r > 0.0f) { /* FCOMP 0.0f; JNZ==le keeps, else fall */
                *pRate = 0.0f; /* 0x30015723 */
            }
        }
    }

    /* --- 0x30015729: clamp rate to [-velLimit, velLimit], return qfalse --- */
    if (*pRate > velLimit) { /* FLD [ECX];FCOMP velLimit; JNZ==le -> 0x30015741 */
        *pRate = velLimit; /* MOV [ECX],velLimit; 0x30015736 */
        return qfalse; /* MOV EAX,ESI (0) */
    }
    /* 0x30015741: compare -velLimit against *pRate (FLD velLimit;FCHS;FCOM [ECX]). */
    if (-velLimit > *pRate) { /* (-velLimit) <= *pRate -> in range (JNZ) */
        *pRate = -velLimit; /* FSTP [ECX]; 0x30015752 */
    }
    /* 0x30015757 in-range path discards -velLimit (FSTP ST0). */
    return qfalse; /* MOV EAX,ESI (0) */
#else
    if (fabsf(*pValue) < 0.25f && fabsf(*pRate) < 1.0f) {
        *pValue = 0.0f;
        *pRate = 0.0f;
        return qtrue;
    }

#if EMULATE_X87
    *pValue = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(*pRate), x87f_load_f32(dt)), x87f_load_f32(*pValue)));
#else
    *pValue += *pRate * dt;
#endif
    if (*pValue > posLimit) {
        *pValue = posLimit;
        if (*pRate > 0.0f) {
            *pRate = 0.0f;
        }
    } else if (*pValue < -posLimit) {
        *pValue = -posLimit;
        if (*pRate < 0.0f) {
            *pRate = 0.0f;
        }
    }

    if (*pValue > 0.0f) {
#if EMULATE_X87
        *pRate = x87f_store_f32(x87f_sub(x87f_load_f32(*pRate), x87f_mul(x87f_load_f32(springAccel), x87f_load_f32(dt))));
#else
        *pRate -= springAccel * dt;
#endif
    } else if (*pValue < 0.0f) {
#if EMULATE_X87
        *pRate = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(springAccel), x87f_load_f32(dt)), x87f_load_f32(*pRate)));
#else
        *pRate += springAccel * dt;
#endif
    }

#if EMULATE_X87
    *pRate = x87f_store_f32(
        x87f_sub(x87f_load_f32(*pRate), x87f_mul(x87f_mul(x87f_load_f32(*pRate), x87f_load_f32(dampCoef)), x87f_load_f32(dt))));
#else
    *pRate -= *pRate * dampCoef * dt;
#endif
    if (*pRate > 0.0f) {
#if EMULATE_X87
        *pRate = x87f_store_f32(x87f_sub(x87f_load_f32(*pRate), x87f_mul(x87f_load_f32(dampAccel), x87f_load_f32(dt))));
#else
        *pRate -= dampAccel * dt;
#endif
        if (*pRate < 0.0f) {
            *pRate = 0.0f;
        }
    } else {
#if EMULATE_X87
        *pRate = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(dampAccel), x87f_load_f32(dt)), x87f_load_f32(*pRate)));
#else
        *pRate += dampAccel * dt;
#endif
        if (*pRate > 0.0f) {
            *pRate = 0.0f;
        }
    }

    if (*pRate > velLimit) {
        *pRate = velLimit;
    } else if (*pRate < -velLimit) {
        *pRate = -velLimit;
    }
    return qfalse;
#endif
}
