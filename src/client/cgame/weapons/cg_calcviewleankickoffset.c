// Source: uo_cgame_mp_x86.dll 0x30045230..0x3004547e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30045230_3004547e.mcode
//
// CG_CalcViewLeanKickOffset(vec3_t out) — build the per-frame first-person
// view/weapon positional offset into the caller's scratch vec3 `out` (passed in
// EAX -> EDI). The offset is the sum of three contributions, all evaluated in the
// view-effect spin frame, then the aim-down-sight (ADS) view offset is derived
// from the finished point:
//   1. a procedural lean sway (envelope of the interpolated player lean fraction,
//      faded out as ADS zoom increases),
//   2. the current weapon-movement view angles (via CG_ApplyWeaponMovementAngles), plus a fixed
//      per-frame Y/Z view-kick nudge, and
//   3. a time-based recoil bump on Z (rise over the first 150 ms after the last
//      impact event, then decay over the following 300 ms).
// Finally CG_CalcAdsViewOffset projects the finished point to cg_adsViewOffset.
//
// NAME ADJUDICATION: the .mcode pre-hint "Info_SetValueForKey_Big" is a pure
// size match (win 0x24e ~= corpus 0x24d) and is REJECTED per the naming rules.
// The body contains zero info-string / key-value work: no strings, no BIG_INFO_
// STRING buffer, no character scanning. It is dense x87 float math (three FSINCOS
// rotations, a vec3 accumulation, three CG view-math callees, a recoil time
// envelope) writing a vec3 through EDI. The name here is provisional-by-role
// (derived from the dataflow: lean-sway + weapon-movement + recoil into a view
// offset); the exact original CoD symbol is unproven.
//
// ABI: the output vec3 arrives in EAX (the sole caller FUN_30046570 at 0x300466a2
// does `LEA EAX,[ESP+0xfc]` over a stack vec3 it just zeroed with STOSD.REP, then
// CALL). The function copies EAX->EDI and uses EDI as the in/out vec3 for the whole
// body; it also re-zeroes [EDI]/[EDI+4]/[EDI+8] at entry. Plain RET, no stack-arg
// cleanup (register-arg ABI). Modeled here as a normal vec3_t out-parameter.
//
// -------------------------------------------------------------------------------
// Machine-code trace (behavior-affecting), constants dumped from .rdata via
// `objdump -s -j .rdata` (exact):
//   0x3007bce0 = 0x3f800000 =   1.0f
//   0x3007bce4 = 0x40000000 =   2.0f    (envelope: 2.0 - fabs)
//   0x3007bcec = 0x00000000 =   0.0f    (== / seed compares)
//   0x3007bd70 = 0x3c8efa35 =   0.017453292f  (DEG2RAD = PI/180)
//   0x3007bdb0 = 0xbf800000 =  -1.0f    (dir sign flips)
//   0x3007be58 = 0x3e800000 =   0.25f   (recoil scale)
//   0x3007c084 = 0x43160000 = 150.0f    (recoil rise divisor)
//   0x3007c088 = 0x43960000 = 300.0f    (recoil decay divisor)
//   0x3007c08c = 0x3fcccccd =   1.6f    (lean-sway push scale)
//
// Globals:
//   0x30483208  leanFraction  (float bits; interpolated playerState lean fraction,
//                              written by CG_InterpolatePlayerState 0x3003564c as
//                              u32_from_f — read here as a float)
//   0x304832a4  cg_predictedPlayerState.adsFraction  (ADS zoom fraction in [0,1])
//   0x30487af0/af4  the fixed per-frame Y/Z view-kick nudge (floats; reset to 0
//                   each frame by 0x30034d40, accumulated elsewhere)
//   0x304879e0  the last impact-event kick magnitude (float; set by CG_EntityEvent)
//   0x304879e4  the cg.time stamp paired with that kick magnitude
//   0x304831b0  cg_time
//
//   30045230 FLD 0.0                                  ; (seed, popped by first FUCOMPP)
//   3004523e MOV [EDI+8]=[EDI+4]=[EDI]=0              ; zero the output vec3
//   30045246 FLD lean ; FUCOMPP 0.0 ; JNP skip        ; if (lean == 0.0) skip math
//   30045259 FLD ads ; FCOMP 1.0 ; JP skip            ; if (ads >= 1.0) skip math   [do-math iff ads<1.0]
//   30045270 EAX=lean ; AND 0x7fffffff -> fabs(lean)
//   30045284 FLD 2.0 ; FSUB fabs(lean) ; FMUL lean    ; envelope=(2-|lean|)*lean -> [+20]
//   300452a2 FLD env ; FADD ST0 ; FSUBR 0.0           ; -2*env -> [+34]  (== ang for sincos #2, in "deg")
//   300452b2 FLD 1.0 ; FSUB ads ; FMUL env ; FMUL 1.6 ; push=(1-ads)*env*1.6 -> [+20] (overwrites env slot)
//   300452cc FLD 0.0 ; FMUL DEG2RAD -> [+1c]          ; ang #? seed 0
//   ... three FSINCOS blocks over [+24],[+24],[+24] producing sin/cos into scratch
//   300453a8 out[0] += push * dirx                    ; FADD [EDI]  / FSTP [EDI]
//   300453b4 out[1] += push * diry                    ; FADD [EDI+4]/ FSTP [EDI+4]
//   300453c2 out[2] += push * dirz                    ; FADD [EDI+8]/ FSTP [EDI+8]
//   300453d0 CALL 0x30045070  CG_ApplyWeaponMovementAngles(out)      ; out += weapon-movement angles
//   300453d5 out[1] -= af0 ; out[2] += af4            ; fixed per-frame view-kick nudge
//   300453de PUSH EDI ; CALL 0x300450e0               ; CG_SpinEffectPointToWorld(out)
//   300453f3 EAX = cg_time - kickTime                 ; ms since last impact event
//   30045401 CMP EAX,0x96 ; JGE                        ; 150 ms rise window
//   3004540c out[2] += (float)elapsed * kick * 0.25 / 150.0 ; then CG_CalcAdsViewOffset(out); RET
//   3004543c CMP EAX,0x1c2 ; JGE 0x30045472            ; 450 ms total window
//   30045443 out[2] += (float)(450-elapsed)*kick*0.25/300.0 ; decay
//   30045472 MOV ECX,EDI ; CALL 0x300451a0             ; CG_CalcAdsViewOffset(out); RET
//
// BOTH the yaw (block #1) and mid (block #2) sincos phases are a literal 0 (they
// reload slot [esp+0x1c] which was set to 0.0*DEG2RAD at 0x300452cc-d8 and copied
// via edx at 0x300452fa); only the pitch phase (-2*envelope*DEG2RAD, 0x3004533a)
// is nonzero. So the emitted direction collapses to (0, -cos(pitch), -sin(pitch)) --
// a pure Y/Z-plane rotation -- exactly as in the sibling AddLeanToPosition
// (0x3004f370): the two share the identical (2-|x|)*x envelope + sincos-direction
// idiom, differing only in phase/scale source and the -Y-forward sign convention.
// -------------------------------------------------------------------------------

#include <math.h>
#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "compat/coduo_native_x87.h"

/* PI/180 — the DEG2RAD constant at 0x3007bd70 (0x3c8efa35). */
#define CG_DEG2RAD 0.017453292f

void CG_CalcViewLeanKickOffset(vec3_t out /* EAX -> EDI: in/out view offset vec3 */)
{
    /* 0x3004523e: zero the output vector. */
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = 0.0f;

    const float leanFraction = cg_predictedPlayerState.leanFraction;
    const float ads = cg_predictedPlayerState.adsFraction;

    /* 0x30045246 / 0x30045259: the lean-sway block runs only when there is a
     * non-zero lean AND the player is not fully aimed down sight. (Verified x87
     * flag semantics: FUCOMPP/TEST 0x44/JNP => skip iff lean==0.0; FCOMP 1.0/
     * TEST 0x5/JP => do-math iff ads < 1.0.) */
    if (leanFraction != 0.0f && ads < 1.0f) {
        /* 0x30045284: smooth odd envelope over the lean fraction, peaking at +/-1
         * for lean == +/-1 and vanishing at 0 and +/-2. */
        const float envelope = (2.0f - fabsf(leanFraction)) * leanFraction;

        /* 0x300452b2: sway push magnitude, faded out by ADS zoom. */
        const float push = (1.0f - ads) * envelope * 1.6f;

        /* Three FSINCOS angles (radians). Each block loads its phase from the
         * SAME stack slot base+0x1c (`FLD [esp+0x24]` after the two register
         * pushes). That slot holds:
         *   block #1 (yaw): 0x300452cc-d8 `FLD 0.0; FMUL DEG2RAD; FSTP [esp+0x1c]`
         *                   -> 0, NOT envelope. Only cos survives (cosYaw=1,sinYaw=0).
         *   block #2 (mid): 0x300452fa MOV [esp+0x1c],edx (edx = the same 0 bits) -> 0.
         *   block #3 (pitch): 0x3004533a FSTP [esp+0x1c] = -2*envelope*DEG2RAD.
         * A prior pass read block #1's phase as envelope*DEG2RAD (a generic
         * AngleVectors assumption), giving the sway a spurious X component. */
        const float angYaw = 0.0f * CG_DEG2RAD;            /* 0x300452cc: [esp+0x1c] = 0 */
        const float angMid = 0.0f * CG_DEG2RAD;            /* 0x300452fa: [esp+0x1c] = 0 */
        const float angPitch = (-2.0f * envelope) * CG_DEG2RAD; /* 0x3004533a: [esp+0x1c] */

        float sinYaw, cosYaw;
        float sinMid, cosMid;
        float sinPitch, cosPitch;

        coduo_x87_sincosf(angYaw, &sinYaw, &cosYaw);
        coduo_x87_sincosf(angMid, &sinMid, &cosMid);
        coduo_x87_sincosf(angPitch, &sinPitch, &cosPitch);

        /* 0x30045358..0x300453a4: the spherical forward direction as emitted; the
         * sinMid (== 0) cross terms vanish, matching a yaw/pitch forward vector in
         * the -Y-forward axis convention. */
        const float sinPitchSinMid = sinPitch * sinMid;
        vec3_t dir;
        dir[0] = cosPitch * sinYaw - sinPitchSinMid * cosYaw;
        dir[1] = -(cosPitch * cosYaw) - sinPitchSinMid * sinYaw;
        dir[2] = -(sinPitch * cosMid);

        /* 0x300453a8..0x300453cd: accumulate the scaled sway direction. */
        out[0] += push * dir[0];
        out[1] += push * dir[1];
        out[2] += push * dir[2];
    }

    /* 0x300453d0: add this frame's weapon-movement view angles into the offset. */
    CG_ApplyWeaponMovementAngles(out);

    /* 0x300453d5..0x300453eb: fixed per-frame view-kick nudge (Y down, Z up). The
     * two globals hold float bits and are reset to 0 each frame by 0x30034d40. */
    out[1] -= cg_weaponSwayOffset[1];
    out[2] += cg_weaponSwayOffset[2];

    /* 0x300453ee: rotate the accumulated point through the view-effect spin frame
     * into world space, anchored at the current view origin. */
    CG_SpinEffectPointToWorld(out);

    /* 0x300453f3: elapsed ms since the last impact-event kick was recorded. */
    const int32_t elapsed = coduo_int32_from_bits((uint32_t)cg_time - (uint32_t)cg_impactViewKickTime);
    const float kick = cg_impactViewKick;

    if (elapsed < 150) {
        /* 0x3004540c: recoil rise — linear ramp up over the first 150 ms. */
        out[2] += (float)elapsed * kick * 0.25f / 150.0f;
    } else if (elapsed < 450) {
        /* 0x30045443: recoil decay — linear ramp down over the next 300 ms. */
        out[2] += (float)coduo_int32_from_bits(450u - (uint32_t)elapsed) * kick * 0.25f / 300.0f;
    }
    /* elapsed >= 450: recoil has fully settled, no Z bump. */

    /* 0x30045432 / 0x30045474: derive the ADS view offset from the finished point
     * (ECX = EDI = out). */
    CG_CalcAdsViewOffset(out);
}
