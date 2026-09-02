// Source: uo_cgame_mp_x86.dll 0x3001db70..0x3001dc8e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001db70_3001dc8e.mcode
//
// CG_DebugCircleEx — draw a debug circle/arc as a 16-point fan connected by 15
// debug lines. Around `center`, it lays down 16 points on a circle of the given
// `radius`, sweeping from `startAngle` to `endAngle`, then issues one
// CG_ADD_DEBUG_LINE (cgame trap 202) per adjacent point pair in `color`.
//
// The mechanical pre-hint `script_func_radiusdamage` is REJECTED: that name is a
// pure size match (win 0x11e == matched 0x11e) against a game_mp_uo server script
// builtin, and this function does no damage — it computes sin/cos ring points and
// calls the debug-line trap. It sits immediately after CG_DebugBox (0x3001d970)
// in the same debug-draw cluster; the same-module PPC bank (cgame_mp.dll) lists
// CG_DebugArc / CG_DebugCircleEx / CG_DebugBox as neighbors. The explicit
// start/end angle range (the "Ex" arc bounds) plus the 16-point sin/cos ring name
// this the circle/arc variant CG_DebugCircleEx. Name is role-derived from the PPC
// bank + behavior; exact spelling (Circle vs CircleEx vs Arc) not binary-proven.
//
// Register-arg ABI (this i386 build): `center` arrives in EAX (moved to ESI and
// read as center[0..2]); `flag` arrives in EBX. Neither is written by this
// function — EBX is forwarded unchanged as the last trap argument (the caller,
// 0x3000cd9a, sets EBX=1 just before the call). The remaining arguments come on
// the stack (radius, startAngle, endAngle, color, param); the call site cleans 5
// dwords (ADD ESP,0x14). Modeled as ordinary leading register parameters, exactly
// as CG_DebugBox (0x3001d970) models its EAX/EDX/EBX/EDI register args.
//
// Angle-range normalization (0x3001db76..0x3001dbcf):
//   angleStep = (endAngle - startAngle) * (1/15)    // 0x3007bed8 = 0.06666667
//   FST saves angleStep, FCOMP vs 0.0 (0x3007bcec), FNSTSW/TEST AH,5/JP.
//   The JP (parity) branch is the compiler's `fcom` "not below" test: it SKIPS the
//   recompute when angleStep >= 0 (or unordered). So the recompute runs only when
//   angleStep < 0.0 — i.e. endAngle < startAngle. It then folds startAngle by one
//   turn (startAngle -= 360.0, 0x3007bd54) so the sweep goes positive:
//   angleStep = (endAngle - startAngle) * (1/15).
//
// Point-build loop (0x3001dbf0..0x3001dc57, EDI = i = 0..15, 16 points):
//   angle = ((i * angleStep) + startAngle) * PI * (1/180)
//           // FMUL 0x3007bd88 (PI), FMUL 0x3007bed4 (1/180)
//   FSINCOS gives cos and sin of `angle`; the machine stores cos to a scratch slot
//   and sin to another, then:
//   pts[i].x = center[0] + radius * cos(angle)   // FMUL [ESP+0xe8]=radius; FADD [ESI]
//   pts[i].y = center[1] + radius * sin(angle)   // FADD [ESI+4]
//   pts[i].z = center[2]                          // MOV [ESI+8] copied verbatim
//
// Line loop (0x3001dc59..0x3001dc82, EDI = 15 down to 1, 15 lines): for each
//   adjacent pair, cgame_syscall(CG_ADD_DEBUG_LINE, &pts[k], &pts[k+1], color,
//   param, flag) — six pushed dwords cleaned by ADD ESP,0x18, matching CG_DebugBox.
//
// Self-check vs .mcode: SUB ESP,0xd8 frame; 3 callee-saved (EBP/ESI/EDI) + the
// EBX/EBP pushes inside the line loop are call arguments not saves; FSINCOS x87
// order (cos stored first to [ESP+0x18]-scratch, sin to [ESP+0x14]-scratch);
// EDX walks the point array in 0xc (vec3) strides with the .z-then-.x-then-.y
// store order; both branch parities and all five .rdata float addresses verified
// against the disassembly.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "compat/coduo_native_x87.h"

#include <math.h>

/* Exact .rdata floats used by the point loop. The first divides the requested
 * angle range across 15 segments; the latter pair converts degrees to radians. */
#define CG_DEBUG_CIRCLE_STEP_SCALE 0.06666667014360428f          /* 0x3007bed8 = 1/15 */
#define CG_DEBUG_CIRCLE_ANGLE_SCALE_A 3.1415927410125732f        /* 0x3007bd88 = PI */
#define CG_DEBUG_CIRCLE_ANGLE_SCALE_B 0.0055555556900799274f     /* 0x3007bed4 = 1/180 */

enum {
    CG_DEBUG_CIRCLE_POINTS = 16   /* CMP EDI,0x10 in the point-build loop */
};

void CG_DebugCircleEx(const vec3_t center, int flag, float radius,
                      float startAngle, float endAngle,
                      const float color[4], int param)
{
    vec3_t pts[CG_DEBUG_CIRCLE_POINTS];
    float angleStep;
    int i;

    /* 0x3001db76..0x3001dbcf: compute the per-index step, normalizing the sweep
     * direction so it is non-negative. FST (0x3001db96) stores the rounded
     * float angleStep but KEEPS the 80-bit chain on st0, and the FCOMP against
     * 0.0f tests that unrounded value — so the sign test reads the long double
     * chain, not the float store. */
    long double angleStepFull =
        ((long double)endAngle - (long double)startAngle) *
        (long double)CG_DEBUG_CIRCLE_STEP_SCALE;
    angleStep = (float)angleStepFull;
    if (angleStepFull < 0.0f) {             /* FCOMP 0.0; TEST AH,5; JP-not-taken */
        startAngle = (float)((long double)startAngle - (long double)360.0f);
        angleStep = (float)(
            ((long double)endAngle - (long double)startAngle) *
            (long double)CG_DEBUG_CIRCLE_STEP_SCALE);
    }

    /* 0x3001dbf0..0x3001dc57: build the 16 ring points. No (float) cast on i:
     * 0x3001dbf0 FILDs it straight into the FMUL at 0x3001dbf4 with no FSTP
     * DWORD, so it enters the chain exact. (Contrast cg_debugcircle.c, whose
     * 0x3001da74 FILD IS followed by FSTP float / FLD float — the cast is
     * correct there. Two functions apart, opposite answers.) */
    for (i = 0; i < CG_DEBUG_CIRCLE_POINTS; i++) {
        float angle = (float)(
            ((long double)i * (long double)angleStep +
             (long double)startAngle) *
            (long double)CG_DEBUG_CIRCLE_ANGLE_SCALE_A *
            (long double)CG_DEBUG_CIRCLE_ANGLE_SCALE_B);
        /* FSINCOS stores cosine first and sine second into binary32 scratch. */
        {
            float sine;
            float cosine;
            coduo_x87_sincosf(angle, &sine, &cosine);
            long double xOffset = (long double)radius * (long double)cosine;
            /* 0x3001dc30..0x3001dc43 copies Z while xOffset remains live. */
            pts[i][2] = center[2];
            pts[i][0] = (float)(xOffset + (long double)center[0]);
            pts[i][1] = (float)((long double)radius * (long double)sine +
                                (long double)center[1]);
        }
    }

    /* 0x3001dc59..0x3001dc82: connect consecutive points with debug lines.
     * EDI runs 15 down to 1 (DEC EDI; JNZ), i.e. 15 segments pts[k]->pts[k+1].
     * Six pushed dwords cleaned by ADD ESP,0x18 — matches CG_DebugBox's use of
     * the same trap. The last argument is the register-arg `flag` (EBX). */
    for (i = 0; i < CG_DEBUG_CIRCLE_POINTS - 1; i++) {
        cgame_syscall(CG_ADD_DEBUG_LINE,
                      (intptr_t)pts[i],
                      (intptr_t)pts[i + 1],
                      (intptr_t)color,
                      (int32_t)param,
                      (int32_t)flag);
    }
}
