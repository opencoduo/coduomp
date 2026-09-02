// Source: uo_cgame_mp_x86.dll 0x300063e0..0x3000654f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300063e0_3000654f.mcode
//
// ConstrainVectorTowardForward (name provisional-by-ROLE; exact CoD source
// symbol unresolved). Rejected the .mcode's assigned name "PM_Weapon_StartFiring":
// that name was matched only by byte size (win 0x16f vs corpus 0x170), which the
// naming rules forbid, and the machine code has no weapon/firing state, no engine
// calls, and no side effects beyond writing one vec3 — it is a pure geometric
// helper, not a weapon-fire routine.
//
// Behavior (proven from the i386 machine code):
//   Inputs arrive in registers (custom register-argument ABI; the prologue is
//   only `SUB ESP,0x28` and neither EDI, ECX nor EBX is saved, so they are
//   caller-supplied arguments, and ESI is the one genuine callee-saved temp):
//     EDI -> const vec3_t forwardDir   (a direction, normally unit-length)
//     ECX -> const vec3_t reference    (a reference direction to bend from)
//     EBX -> vec3_t       out          (result written back)
//   `RET` with no immediate (caller cleans nothing; register args).
//
//   1. Sanitize the forward direction into a local `forward`:
//        if (length(forwardDir) >= 1.0)  forward = forwardDir;
//        else                            forward = (0,0,1);
//      (length compared against the shared .rdata double 1.0 at 0x3007bcf8; the
//       FCOMP/FNSTSW/TEST AH,0x5/JP idiom takes the JP branch — i.e. copies
//       forwardDir — when length >= 1.0 or is unordered, and falls through to the
//       (0,0,1) default only when length < 1.0.)
//   2. Start the working vector from the negated, normalized reference:
//        v = normalize( -reference )
//   3. Choose the cone threshold from the forward pitch (forwardDir[2]):
//        threshold = (forwardDir[2] > 0.8f) ? 0.7f : 0.3f
//      (forwardDir[2] compared against the shared .rdata float 0.8f at 0x3007bdf0.)
//   4. Rotate v toward forward in fixed half-forward steps until it lies inside
//      the cone, then output it:
//        step = 0.5f * forward;              // fixed increment
//        while (dot(v, forward) < threshold) {
//            v = normalize(v + step);
//        }
//        out = v;
//      (dot(v,forward) compared against `threshold`; same TEST AH,0x5 idiom —
//       loop continues while dot < threshold, exits when dot >= threshold.)
//
// Callees:
//   VectorNormalize(vec3_t) @ 0x30049700 — in-place 3D normalize returning length
//   in ST0; the returned length is discarded here (FSTP ST0 after each call).
//   Length in step 1 is computed inline (FLD/FMUL/FADDP/FSQRT), not via a call.
//
// Shared .rdata float/double constants (natural-form values verified from the
// binary): 0x3007bcf8 = 1.0 (double), 0x3007bdf0 = 0.8f, 0x3007bce8 = 0.5f.
// Immediates: 0x3e99999a = 0.3f, 0x3f333333 = 0.7f, 0x3f800000 = 1.0f.

#include "client/cgame/client_recovered.h"

#include <math.h> /* sqrtf: direct C form of the inline x87 FSQRT at 0x30006402 */

/*
 * Register-argument ABI: forwardDir=EDI, reference=ECX, out=EBX (see header
 * comment). Modeled here as plain pointer parameters; the register mapping is a
 * calling-convention detail, not source-level behavior.
 */
void ConstrainVectorTowardForward(const vec3_t forwardDir, const vec3_t reference, vec3_t out)
{
    /*
     * Inline length of forwardDir: sqrt(x*x + y*y + z*z), computed on the x87
     * stack at 0x300063e3..0x30006402 and compared against the double 1.0 at
     * 0x3007bcf8 (0x3000640a). The whole chain -- squares, sum, FSQRT, FCOMP --
     * stays in 80-bit registers with NO float store, so the length local is
     * long double and the comparison constant is the double 1.0.
     */
    long double forwardLen = sqrtl((long double)forwardDir[0] * forwardDir[0] + (long double)forwardDir[1] * forwardDir[1] +
                                   (long double)forwardDir[2] * forwardDir[2]);

    vec3_t forward;
    if (forwardLen < 1.0) {
        /* length < 1.0: default forward = (0,0,1). 0x30006417..0x30006427 */
        forward[0] = 0.0f;
        forward[1] = 0.0f;
        forward[2] = 1.0f;
    } else {
        /* length >= 1.0 (or unordered): forward = forwardDir. 0x30006431 */
        forward[0] = forwardDir[0];
        forward[1] = forwardDir[1];
        forward[2] = forwardDir[2];
    }

    /* v = normalize(-reference). Negations at 0x30006445..0x30006460,
     * VectorNormalize call at 0x30006464 (length discarded, FSTP ST0). */
    vec3_t v;
    v[0] = -reference[0];
    v[1] = -reference[1];
    v[2] = -reference[2];
    VectorNormalize(v);

    /* threshold = (forwardDir[2] > 0.8f) ? 0.7f : 0.3f.
     * forwardDir[2] compared against 0.8f (0x3007bdf0) at 0x3000646e; the 0.3f
     * default is pre-loaded at 0x300063e6 and only overwritten with 0.7f when
     * forwardDir[2] > 0.8f (0x30006479 JNZ / 0x3000647b). */
    float threshold = 0.3f;
    if (forwardDir[2] > 0.8f) {
        threshold = 0.7f;
    }

    /*
     * dot(v, forward) at 0x30006483..0x3000649f. If already inside the cone
     * (dot >= threshold) skip the loop entirely (JP 0x30006536).
     * The FADDP chain sums the z, y, x terms IN THAT ORDER and the FCOMP
     * consumes the unrounded 80-bit dot (no float store), so the local is
     * long double and the C term order mirrors the instruction stream.
     */
    long double dot = (long double)v[2] * forward[2] + v[1] * forward[1] + v[0] * forward[0];
    if (dot < threshold) {
        /* Fixed step increment = 0.5f * forward. 0x300064ae..0x300064d4. */
        vec3_t step;
        step[0] = forward[0] * 0.5f;
        step[1] = forward[1] * 0.5f;
        step[2] = forward[2] * 0.5f;

        /* Rotate v toward forward one step at a time, renormalizing, until the
         * dot reaches the threshold. Loop body 0x300064e0..0x30006534. */
        do {
            v[0] = v[0] + step[0];
            v[1] = v[1] + step[1];
            v[2] = v[2] + step[2];
            VectorNormalize(v);
            /* 0x3000650f..0x3000652b: same z,y,x FADDP order and unrounded
             * 80-bit FCOMP as the pre-loop dot. */
            dot = (long double)v[2] * forward[2] + v[1] * forward[1] + v[0] * forward[0];
        } while (dot < threshold);
    }

    /* out = v. 0x30006536..0x30006547 (moved as raw dwords through ECX/EDX/EAX). */
    out[0] = v[0];
    out[1] = v[1];
    out[2] = v[2];
}
