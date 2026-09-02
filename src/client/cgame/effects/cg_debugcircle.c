#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "compat/coduo_native_x87.h"

#include <math.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3001d9f0..0x3001db64
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001d9f0_3001db64.mcode
//
// CG_DebugCircle — draw a debug circle of `radius` around `center`, oriented in
// the plane PERPENDICULAR to a direction vector `dir`, as 16 CG_ADD_DEBUG_LINE
// (trap 0xca == 202) segments in `color`. Sits in the debug-draw cluster
// immediately after CG_DebugBox (0x3001d970) and before CG_DebugCircleEx
// (0x3001db70); unlike CG_DebugCircleEx (which sweeps a start..end ARC in a
// fixed plane), this variant builds an orthonormal frame around an arbitrary
// direction and draws the FULL closed 16-gon in the disc perpendicular to it.
//
// The assigned .mcode name "BG_CalculateView_DamageKick" is REJECTED: it is a
// SIZE-MATCH ONLY (win 0x174 == a same-size corpus function) with zero
// behavioral basis. The body touches NO view-kick / weapon-movement globals
// (cg_viewKickAngles / cg_viewKickVel / cg_weaponMovementAngles are never
// referenced), does no angle/velocity integration, and computes no view kick.
// It is pure vector-frame math feeding CG_ADD_DEBUG_LINE, exactly the debug-draw
// idiom of its two neighbors. Name role-derived from the same-module debug-draw
// cluster (CG_DebugBox / CG_DebugCircleEx neighbors) plus the observed behavior.
// Exact spelling (Circle vs. CircleAxis vs. DebugCircle) is not binary-proven.
//
// i386 register+stack ABI (proven from the caller at 0x30007238 in
// FUN_30006e10 and from this body):
//   EAX = dir     (input direction; LEA EAX,[ESP+0xe4] at the call site)
//   EBX = center  (circle center vec3; LEA EBX,[ESP+0x20] at the call site)
//   stack arg0 (entry+0x04, [ESP+0x108]) = radius   (caller PUSHes 0x40c00000 = 6.0f)
//   stack arg1 (entry+0x08, [ESP+0x114]) = color    (caller PUSHes 0x30072014 = {1,0,0.25,0.25})
//   stack arg2 (entry+0x0c, [ESP+0x110]) = param    (caller PUSHes [0x30452e4c])
//   stack arg3 (entry+0x10, [ESP+0x10c]=EBP)         = flag  (caller PUSHes 0x1)
// EBX (center) and the stack args are never modified; they are forwarded to
// every trap call. Caller cleans the 4 stack dwords (ADD ESP,0x10). Modeled
// here as ordinary leading register parameters, as with CG_DebugBox.
//
// param and flag are forwarded verbatim as the trailing two CG_ADD_DEBUG_LINE
// args and are opaque here (duration/depth flags). Typed as int to match the
// sibling CG_DebugBox/CG_DebugCircleEx `int param` / `int flag`.

void CG_DebugCircle(const vec3_t dir, const vec3_t center, float radius, const float color[4], int param, int flag)
{
    /*
     * 0x3001da00..0x3001da13: build an orthonormal frame around `dir`.
     *   forward = normalize(dir)            (VectorNormalize2; length discarded)
     *   right   = PerpendicularVector(forward)   (unit vector perpendicular to forward)
     * VectorNormalize2 writes `forward` through ESI (LEA ESI,[ESP+0x24]) from the
     * input in EDI (=EAX=dir); its returned length is FSTP ST0 (discarded).
     * PerpendicularVector then takes forward as src (EDI=ESI) and writes `right`
     * through EDX (LEA EDX,[ESP+0x18]).
     */
    vec3_t forward;
    vec3_t right;
    (void)VectorNormalize2(dir, forward);
    PerpendicularVector(right, forward);

    /*
     * 0x3001da18..0x3001da70: up = forward x right  (the third basis vector,
     * completing a right-handed orthonormal frame). Each component is a single
     * FLD*FMUL - FLD*FMUL - FSUBP cross-product term, all in float precision:
     *   up.x = forward.y*right.z - forward.z*right.y
     *   up.y = forward.z*right.x - forward.x*right.z
     *   up.z = forward.x*right.y - forward.y*right.x
     * Stored at [ESP+0x34/0x38/0x3c].
     */
    vec3_t up;
    up[0] = (float)((long double)forward[1] * (long double)right[2] - (long double)forward[2] * (long double)right[1]);
    up[1] = (float)((long double)forward[2] * (long double)right[0] - (long double)forward[0] * (long double)right[2]);
    up[2] = (float)((long double)forward[0] * (long double)right[1] - (long double)forward[1] * (long double)right[0]);

    /*
     * 0x3001da74..0x3001db10: generate 16 points evenly spaced around the circle.
     * For step i (ESI runs 1..16; the stored index [ESP+0x14] advances 1..16):
     *   angle  = i * 0.39269909f            (== PI/8, .rdata float @0x3007bedc; 16 steps = 2*PI)
     *   FSINCOS(angle) -> cos(angle), sin(angle)
     *   c = cos(angle) * radius             (radius = [ESP+0x108])
     *   s = sin(angle) * radius
     *   points[i-1] = center + c*right + s*up   (per-component, +[EBX+4k])
     * The x87 order is preserved: cos/right paired first, sin/up second, FADDP,
     * then FADD center[k]. Points are written to a 16-entry vec3 array on the
     * stack (EDX walks [ESP+0x48]+0xc per step, storing at EDX-0x10/-0xc/-0x8;
     * i.e. points[i] at [ESP+0x44 + 12*i]).
     *
     * NOTE: the index used for `angle` (INC ESI at 0x3001daa4, CMP ESI,0x10)
     * starts at 1 and the FILD reads the pre-increment value, so the first point
     * uses angle 0 (cos=1,sin=0) and the loop produces points for i=0..15.
     */
    vec3_t points[16];
    {
        int i;
        for (i = 0; i < 16; i++) {
            float indexAsFloat = (float)i;          /* FILD/FSTP at 0x3001da74 */
            float angle = (float)((long double)indexAsFloat * (long double)0.39269909f);
            float sine;
            float cosine;
            coduo_x87_sincosf(angle, &sine, &cosine);
            /* 0x3001daa0..0x3001dac5 scales and stores sine first, then cosine. */
            float s = (float)((long double)sine * (long double)radius);
            float c = (float)((long double)cosine * (long double)radius);
            /* FADDP sums the two products FIRST, then FADD center[k]
             * (0x3001dac9..0x3001db0d) — the association is part of the
             * rounding behavior, so center[k] is added last. */
            points[i][0] = (float)((long double)right[0] * (long double)c + (long double)up[0] * (long double)s + (long double)center[0]);
            points[i][1] = (float)((long double)right[1] * (long double)c + (long double)up[1] * (long double)s + (long double)center[1]);
            points[i][2] = (float)((long double)right[2] * (long double)c + (long double)up[2] * (long double)s + (long double)center[2]);
        }
    }

    /*
     * 0x3001db16..0x3001db58: connect the ring into a closed 16-gon. For each i
     * (ESI runs 1..16), draw a debug line from points[i] to points[(i+1) & 0xf]
     * (EDI walks &points[0]+0xc per step; the neighbor index is (i+1)&0xf, which
     * wraps 15->0 to close the circle). The six trap args are pushed in reverse
     * and cleaned by ADD ESP,0x18:
     *   PUSH flag(EBP); PUSH param([ESP+0x110]); PUSH color([ESP+0x114]);
     *   PUSH &points[next]; PUSH &points[i]; PUSH 0xca.
     */
    {
        int i;
        for (i = 0; i < 16; i++) {
            int next = (i + 1) & 0xf;
            cgame_syscall(CG_ADD_DEBUG_LINE, (intptr_t)points[i], (intptr_t)points[next], (intptr_t)color, (int32_t)param, (int32_t)flag);
        }
    }
}
