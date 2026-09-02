// Source: uo_cgame_mp_x86.dll 0x30032c20..0x30032d99
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30032c20_30032d99.mcode

#include "client/cgame/client_recovered.h"

/*
 * CG_PlayerShadow (0x30032c20) — project the drop/blob ground shadow for a model
 * (player or usable-vehicle) and report the shadow plane Z back to the caller.
 *
 * The .mcode-assigned name "G_IsVehicleUsable" is REJECTED: it is a pure size
 * match (win size 0x179 == a game_mp_uo G_* symbol) with no behavioral basis, and
 * this is cgame client code that WRITES an out struct (it does not return a
 * vehicle-usable bool). The proven behavior is the classic idTech/CoD
 * CG_PlayerShadow: a downward trace from the entity origin, and when the shadow
 * cvar is set to the blob-shadow mode, a `markShadow` decal projected onto the
 * ground and faded by distance to the local player camera. The out float the
 * caller reads is the shadow plane height. (The prior caller-observed provisional
 * name `CG_AddPlayerModelEffect` named the same 0x30032c20 entry; superseded here
 * by the behavioral name.)
 *
 * Custom register ABI (proven from the CG_Player call site at 0x300345e5, which
 * leaves `cent` live in ESI and passes the out pointer in EAX):
 *   EAX = shadowPlane  (float *, the out slot; first store is *shadowPlane = 0)
 *   ESI = cent         (centity_t *, the entity, an incoming register arg)
 * Returns qboolean in EAX (EBX is loaded with 1 on the success paths and moved to
 * EAX at the shared epilogue; the early-out paths XOR EAX,EAX). EBX/EDI are
 * callee-saved.
 *
 * The gating global cg_shadows_vmCvar.integer (0x3044fdec) is the shadow
 * mode flag (idTech cg_shadows): nonzero to attempt a shadow at all, and exactly
 * == 1 selects the blob-decal path taken here (higher values would select the
 * stencil path, which this build does not implement). Its owner label
 * "g_isvehicleusable" is likewise a size-match guess and is not its real name.
 *
 * cent+0x208 (lerpOrigin) is the entity's interpolated world position used as
 * the trace start; cent+0x8 EF_DEAD suppresses the blob shadow.
 *
 * .rdata constants (dumped exact via objdump -s -j .rdata):
 *   0x3007bce0 = 1.0f    (floatOne)      — shadow-plane bias
 *   0x3007bcec = 0.0f    (floatZero)   — fraction==0 reject
 *   0x3007c000 = 64.0f   (g_const_float_64)       — trace start Z drop
 *   0x3007bcf8 = 1.0     (doubleOne)           — trace-fraction sentinel
 *   0x3007c1d8 = 250.0   (g_double_250)           — near fade radius
 *   0x3007c1d0 = 512.0   (g_double_512)           — mid fade radius
 *   0x3007c1c0 = 1024.0  (g_double_1024)          — far cull radius
 *   0x3007c1c8 = 0.0038167939 (g_double_0_0038167939...)   — 1/262 (ramp 250->512)
 *   0x3007c1b8 = 0.001953125  (g_double_0_001953125...)    — 1/512 (ramp 512->1024)
 *
 * Machine-code proof of each behavior-affecting step:
 *   30032c26  *shadowPlane = 0                              (MOV [EBX],0)
 *   30032c2c  if (g_isvehicleusable == 0) return 0          (TEST EAX,EAX; JZ)
 *   30032c3c..30032c7f  trace 0x26 (CG_CM_BOX_TRACE) with origin =
 *             { cent->lerpOrigin[0], cent->lerpOrigin[1],
 *               cent->lerpOrigin[2] - 64.0f }                (trace straight down)
 *             mask arg = 0x2810011, out = &traceResult.
 *   30032c85  if (traceResult.fraction == 1.0) return 0     (FLD; FUCOMPP; JNP)
 *   30032c9f  if (traceResult.fraction == 0.0) return 0     (FLD 0.0; FUCOMPP; JNP)
 *   30032cb6  *shadowPlane = traceResult.endpos[2] + 1.0    (FADD 1.0; FSTP [EBX])
 *   30032cc2  if (g_isvehicleusable != 1) return 1          (CMP EAX,1; JNZ epilogue)
 *   30032cd4  if (cent->currentState.eFlags & 1)      return 0           (TEST [ESI+8],BL; JNZ)
 *   30032cdd  alpha0 = 1.0 - traceResult.fraction           (FSUBR 1.0)
 *   30032cf5  dist = VectorDistance(cg_snap->ps.psOrigin,
 *                                   cent->lerpOrigin)         (EAX=a, ECX=b)
 *   30032cfe  if (dist <= 250.0) return 1                    (FCOMP 250; TEST AH,41; JNZ)
 *   30032d0b  if (dist < 512.0) scale = (dist-250)*(1/262)   (ramp up 0..1)
 *   30032d2e  else if (dist >= 1024.0) return 0              (cull)
 *   30032d3b  else scale = 1.0 - (dist-512)*(1/512)          (ramp down 1..0)
 *   30032d51  scale *= alpha0                                (FMUL [ESP+0xc])
 *   30032d55..30032d81  CG_ImpactMark(cgs.media.markShadow,
 *               traceResult.endpos, traceResult+0x10 (surface dir),
 *               orientation=0, r=scale, g=scale, b=scale, a=1.0,
 *               alphaFade=0, radius=16.0, markLifeTime=1, temporary=-1)
 *   30032d89  return 1
 *
 * Callees (provisional decls; reuse name only, arity/types re-derived here):
 *   0x300495b0 VectorDistance(a, b)      — fastcall EAX=a, ECX=b, x87 float return.
 *   0x3002e520 CG_ImpactMark(...)        — ECX = dir; all other args cdecl on stack.
 *
 * Stack-offset correction: at 0x30032c85 the eight live syscall pushes make
 * [ESP+0x3c] the trace buffer's +0x00 fraction, not +0x20. After cleanup,
 * [ESP+0x1c] addresses that same fraction and [ESP+0x28] is endpos.z.
 */

qboolean CG_PlayerShadow(float *shadowPlane, centity_t *cent)
{
    trace_t traceResult;
    vec3_t traceStart;
    float alpha0;
    float dist;
    /* long double: neither fade ramp is stored. Both legs run from the FLD of
     * dist ([ESP+0x8]) straight through the FMUL by alpha0 at 0x30032d51 with
     * no intermediate float store; the single rounding is the FSTP at
     * 0x30032d63 that materializes the CG_ImpactMark red/green/blue argument. */
    long double scale;

    *shadowPlane = 0.0f;

    if (cg_shadows_vmCvar.integer == 0)
        return qfalse;

    /* Trace straight down from the entity origin (Z dropped by 64) to find ground. */
    traceStart[0] = cent->lerpOrigin[0];
    traceStart[1] = cent->lerpOrigin[1];
    traceStart[2] = cent->lerpOrigin[2] - 64.0f;

    cgame_syscall(CG_CM_BOX_TRACE, (intptr_t)&traceResult, (intptr_t)cent->lerpOrigin, (intptr_t)traceStart, 0, 0, 0, 0x2810011);

    /* No hit at fraction 1.0, and reject the degenerate zero-fraction result. */
    if (traceResult.fraction == doubleOne)
        return qfalse;
    if (traceResult.fraction == 0.0f)
        return qfalse;

    /* Report the shadow plane one unit above the ground hit. */
    *shadowPlane = traceResult.endpos[2] + 1.0f;

    /* Only the blob-shadow mode (== 1) draws a decal; higher modes fall through. */
    if (cg_shadows_vmCvar.integer != 1)
        return qtrue;

    /* EF_DEAD suppresses the blob shadow for this entity. */
    if ((cent->currentState.eFlags & EF_DEAD) != 0)
        return qfalse;

    alpha0 = 1.0f - traceResult.fraction;

    dist = VectorDistance(cg_snap->ps.psOrigin, cent->lerpOrigin);

    /* Distance fade to the local player camera: invisible past 1024, full-strength
     * ramp between the near/mid radii, ramp back down to the cull radius. */
    /* 0x30032cfe..0x30032d09 tests C0|C3 and returns on any nonzero
     * result.  That is ordered <= and unordered, not an ordinary C <=
     * comparison (which would reject NaN). */
    if (!(dist > 250.0f)) {
        return qtrue;
    } else if (dist < 512.0) {
        scale = ((long double)dist - 250.0L) * (long double)0.0038167938931297709;
    } else if (dist > 1024.0) {
        /* 0x30032d36 test ah,0x41 / 0x30032d39 je: the far cull is taken only when
         * C0=0 AND C3=0, i.e. dist STRICTLY > 1024.0. At dist == 1024.0 (C3=1) the
         * binary falls through to the ramp-down leg (scale = 1 - (1024-512)/512 = 0)
         * and still draws the mark / returns qtrue. A prior pass used >= 1024.0. */
        return qfalse;
    } else {
        scale = 1.0L - ((long double)dist - 512.0L) * 0.001953125L;
    }

    scale = scale * alpha0;

    CG_ImpactMark(cgs_media_markShadowShader, traceResult.endpos, traceResult.normal, 0.0f, scale, scale, scale, 1.0f, qfalse, 16.0f, 1,
                  -1);

    return qtrue;
}
