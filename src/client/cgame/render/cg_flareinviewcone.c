// Source: uo_cgame_mp_x86.dll 0x3001e510..0x3001e676
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001e510_3001e676.mcode
//
// CG_FlareInViewCone (provisional-by-role name; exact CoD source symbol
// unresolved). Boolean predicate: given a moving effect entity (its position
// trajectory) and the current view (origin + view angles), decide whether the
// effect's evaluated world position is (a) within 255 world units of the view
// point and (b) inside a forward-facing view cone whose half-angle *widens with
// distance*. Returns qtrue when both hold, qfalse otherwise. This is the classic
// id-Tech/CoD corona / lens-flare visibility gate (a flare is drawn only when it
// lies ahead of the eye, within range, and its angular offset from the view
// forward is small enough for the current range).
//
// The mechanical .mcode name "script_func_playfx" is REJECTED: that symbol is a
// script VM builtin with signature `void(void)` and no arguments, whereas this
// function uses a custom register ABI (ECX = entity, EDI = view, EAX = atTime),
// reads floating-point struct fields, and returns a boolean in EAX. The name was
// a pure size match (win 0x166 vs corpus 0x165), which the recovery rules forbid.
//
// Custom REGISTER ABI (matches the sibling trajectory helpers in this module):
//   ECX = entity object (its position trajectory_t is embedded at +0x0c)
//   EDI = playerState_t (psOrigin at +0x14, viewAngles at +0xe8,
//         viewHeightCurrent at +0xf8)
//   EAX = atTime (forwarded untouched into BG_EvaluateTrajectory)
// Plain-ish frame: SUB ESP,0x4c then PUSH EBX/ESI; body [ESP+..] offsets are
// relative to the post-push stack pointer. RET (no stack args; callee-register
// ABI). Expressed as source-shaped C below; the register plumbing is an ABI
// detail, not source behavior.

#include <math.h>
#include "client/cgame/client_recovered.h"
#include "compat/coduo_native_x87.h"

/*
 * Degrees-to-radians factor. The machine code multiplies each view angle by the
 * .rdata constant at 0x3007bd70 == 0x3c8efa35 == 0.0174532924f, i.e. PI/180.
 */
#define CG_DEG2RAD 0.0174532924f

/*
 * Distance-scaled cone threshold coefficients (all exact .rdata dword reads):
 *   0x3007be24 = 0x3b808081 = 0.00392156886f  (== 1/255)
 *   0x3007c024 = 0x3d6978d5 = 0.05700000003f  (~0.057)
 *   0x3007c020 = 0xbf70a3d7 = -0.939999998f   (~-0.94)
 * threshold = -0.94f - dist * (1/255) * 0.057f  (a negative cosine bound that
 * grows toward -1 as distance grows, i.e. a cone that narrows with range).
 */
#define CG_FLARE_INV_MAX_DIST 0.00392156886f   /* 1/255, 0x3007be24 */
#define CG_FLARE_CONE_SLOPE   0.057f           /* 0x3007c024, 0x3d6978d5 */
#define CG_FLARE_CONE_BASE    (-0.939999998f)  /* 0x3007c020 */

/* Max flare range in world units (FCOMP against 0x3007bd64 == 255.0f). */
#define CG_FLARE_MAX_DIST 255.0f

qboolean CG_FlareInViewCone(const centity_t *entity /* ECX */,
                            const playerState_t *ps /* EDI */,
                            int32_t atTime /* EAX */)
{
    vec3_t origin;

    /* Evaluate the effect entity's position trajectory at atTime.
     * BG_EvaluateTrajectory(tr=EBX=entity+0x0c, atTime=EAX, result=ECX). */
    BG_EvaluateTrajectory(&entity->currentState.pos, atTime, origin);

    /* dir = (view eye point) - (effect origin). The eye point is the view
     * origin, with its z component pushed up by view->eyeHeight (+0xf8). */
    vec3_t dir;
    /* 0x3001e534/0x3001e546: (view->origin[2] + view->eyeHeight) is rounded to a
     * float slot and reloaded at 0x3001e562 before the origin[2] subtraction
     * (Class 1) -- an explicit float temp forces that intermediate rounding. */
    float eyePointZ = (float)((long double)ps->psOrigin[2] +
                              (long double)ps->viewHeightCurrent);
    dir[0] = (float)((long double)ps->psOrigin[0] - (long double)origin[0]);
    dir[1] = (float)((long double)ps->psOrigin[1] - (long double)origin[1]);
    dir[2] = (float)((long double)eyePointZ - (long double)origin[2]);

    /* Normalize in place; VectorNormalize returns the original length. */
    float dist = VectorNormalize(dir);

    /* Out of range: reject when dist > 255. (FCOMP; taken JZ path when ST0 is
     * strictly greater than the constant, i.e. neither equal nor less.) */
    if (dist > CG_FLARE_MAX_DIST) {
        return qfalse;
    }

    /*
     * Build the view forward vector from the two view angles via FSINCOS, in the
     * exact component order the machine code uses:
     *   a0 = angle0 * DEG2RAD  -> c0 = cos(a0), s0 = sin(a0)   (EDI+0xe8)
     *   a1 = angle1 * DEG2RAD  -> c1 = cos(a1), s1 = sin(a1)   (EDI+0xec)
     *   forward = ( c0*c1, c0*s1, -s0 )
     * FSINCOS produces cos on top of the stack (ST0) and sin in ST1; the stores
     * (FSTP [cos_slot]; FSTP [sin_slot]) confirm cos-first / sin-second.
     */
    float yawRadians = (float)((long double)ps->viewAngles[1] *
                               (long double)CG_DEG2RAD);
    float s1;
    float c1;
    coduo_x87_sincosf(yawRadians, &s1, &c1);
    float pitchRadians = (float)((long double)ps->viewAngles[0] *
                                 (long double)CG_DEG2RAD);
    float s0;
    float c0;
    coduo_x87_sincosf(pitchRadians, &s0, &c0);

    float forward0 = (float)((long double)c0 * (long double)c1);
    float forward1 = (float)((long double)c0 * (long double)s1);
    float forward2 = -s0;       /* [esp+0x38] via FCHS */

    /* dot = forward . dir (dir is unit) == cos(angle between view forward and the
     * direction from the effect toward the eye). Accumulated in machine order:
     * (-s0)*dir[2] + (c0*s1)*dir[1] + (c0*c1)*dir[0]. */
    /* 0x3001e61a..0x3001e636: one unbroken FMUL/FADDP chain with a single trailing
     * FSTP -- the dot product rounds once, not once per term (Class 2). */
    float dot = (float)(
        (long double)forward2 * (long double)dir[2] +
        (long double)forward1 * (long double)dir[1] +
        (long double)forward0 * (long double)dir[0]);

    /* Distance-scaled cone bound; a flare passes when dot <= threshold (the
     * direction toward the eye is sufficiently anti-parallel to view forward,
     * i.e. the effect is ahead of the eye within the cone). */
    float threshold = (float)(
        (long double)CG_FLARE_CONE_BASE -
        ((long double)dist * (long double)CG_FLARE_INV_MAX_DIST) *
            (long double)CG_FLARE_CONE_SLOPE);

    /* TEST AH,0x41/JNZ accepts less, equal, and unordered; only ordered greater
     * reaches the false return. `!(>)` preserves that unordered edge. */
    if (!(dot > threshold)) {
        return qtrue;
    }
    return qfalse;
}
