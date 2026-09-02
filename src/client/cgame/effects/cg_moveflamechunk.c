// Source: uo_cgame_mp_x86.dll 0x30025da0..0x300265bb
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30025da0_300265bb.mcode
//
// CG_MoveFlameChunk — per-frame finalize/advance of one flame chunk.
//
// ABI (proven from the machine code, matches the existing decl
//   void CG_MoveFlameChunk(flameChunk_t *chunk, int32_t flameTime)):
//   the two caller-cleaned stack dwords are chunk (arg0, held in EBP throughout —
//   EBP is used as a general register, NOT a frame pointer) and flameTime (arg1).
//   The prologue is `SUB ESP,0x44; PUSH EBX/EBP/ESI/EDI` and `MOV EBP,[ESP+0x50]`
//   reads arg0; [ESP+0x5c] is arg1. Plain RET (caller-cleaned). EBX/EBP/ESI/EDI are
//   callee-saved. No calling-convention attribute is added (syntax-only build).
//
// The .mcode header's size-matched guess "CG_PlayerTurretPositionAndBlend" is
// REJECTED: the body calls the flame-cluster helpers CG_ComputeFlameChunkOrigin
// (0x30025990), CG_FlamethrowerTrace (0x30025cd0) and CG_FlameDropDrip
// (0x30027ad0), indexes cg_flameInfo[chunk->ownerInfoIndex], and reads/writes flameChunk_t
// fields — it is flame-chunk finalize, not turret math.
// The Mac cgame symbol CG_MoveFlameChunk shares all five distinctive direct game
// callees; the only spelling difference is AngleNormalize180Accurate versus the recovered
// accuracy-qualified wrapper. This resolves the source name.
//
// This function calls CG_UpdateFlameChunk(0x30023c30) with EAX = the chunk
// pointer to advance the chunk's per-frame drift/size state. (Adjudicated: that
// helper was once mis-modelled on a provisional cg_adsAnimState_t as
// CG_PlayADSAnim; the machine code proves it operates on flameChunk_t, so it now
// takes flameChunk_t* directly and no cast is needed.)
//
// Every behavior-affecting statement below is proven against the .mcode; x87
// compare idioms (FCOMP/FUCOMPP + FNSTSW + TEST AH,{0x41,0x44,0x05} + Jcc) are
// translated to the equivalent scalar comparisons.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "compat/coduo_native_x87.h"

#include <math.h>

/* Statically-linked MSVC math helpers (FSQRT / single-precision fabs) are
 * expressed through the standard math interface. */

/* .rdata float/double pool constants used below (re-verified via
 * objdump -s -j .rdata; addresses in comments):
 *   0x3007bce0 = 1.0f          0x3007bcec = 0.0f
 *   0x3007c330 = 1800.0f       0x3007be6c = 0.05f
 *   0x3007be58 = 0.25f         0x3007c0f8 = 6000.0f
 *   0x3007bdd0 = 32.0f         0x3007c308 = 145.0f
 *   0x3007bec0 = 1/32768 (3.0517578e-05f)  0x3007bee4 = 2000.0f
 *   0x3007c174 = 0.45f
 *   0x3007bcf0 = 0.0  (double) 0x3007bcf8 = 1.0  (double)
 *   0x3007bd28 = 0.5  (double) 0x3007c338 = 0.001 (double)
 *   0x3007c328 = 30.5 (double) 0x3007c320 = (double)-6.8f (0xC01B333340000000)
 *   0x3007c318 = 1/(30.5+(double)6.8f) = 0.026809651337438734 (double, exact)
 *   0x3007c310 = (double)0.35f (0x3FD6666660000000)
 *   0x3007c128 = 20.0 (double) 0x3007c300 = 2000.0 (double)
 *   0x3007c2f8 = 0.3  (double) 0x3007c2f0 = 150.0 (double)
 * Trace bounds (.data): 0x300851d0 = maxs {2,2,2}, 0x300851c4 = mins {-2,-2,-2};
 * only maxs[0] (2.0f) is read as a scalar; both are passed by address to the trace. */

/* CG_UpdateFlameChunk (0x30023c30) advances the drift/size state of the chunk.
 * Declared in client_recovered.h; no local extern needed. */

void CG_MoveFlameChunk(flameChunk_t *chunk, int32_t flameTime)
{
    /* ---- Block A (0x30025da0): advance the chunk's anim/fade timer field_f8 ----
     * Only when field_94 > 1.0f AND field_bc != 0.0f. (FCOMP field_94 vs 1.0f;
     * skip when field_94 <= 1.0. FUCOMPP field_bc vs 0.0f; skip when == 0.0f.) */
    if (chunk->driftSpeed > 1.0f && chunk->alpha != 0.0f) {
        /* The target does not read +0x34 or form this row address until both
         * leading float gates have passed (0x30025ddf). */
        cgFlameInfo_t *info = &cg_flameInfo[chunk->ownerInfoIndex];
        double elapsedF8 = (double)(((long double)flameTime - (long double)chunk->endTimeCopy3) * (long double)0.001); /* [ESP+0x30] */
        /* 0x30025e0e is FMUL m32 (c330 = float 1800.0f), so the product is built
         * from a float operand and only widened by the FST double [ESP+0x28] at
         * 0x30025e14. (1800 is exact in both widths; form follows the bytes.) */
        long double bcScaledRaw = (long double)chunk->alpha * (long double)1800.0f;
        double bcScaled = (double)bcScaledRaw; /* [ESP+0x28] */
        /* x is NEVER stored: both paths keep it in st0 through the optional
         * 0.35 multiply and the elapsedF8 multiply, and the only rounding is
         * the FSTP float of -x as the call argument (0x30025fa7). A double
         * local would insert per-step roundings the DLL does not perform. */
        long double x;
        flameChunk_t *ownerChunk = info->ownerChunk; /* [EAX+0x300ab790], saved [ESP+0x58] */

        if (ownerChunk != NULL && chunk->emitCounter == info->emitCounter && /* [EBP+0x128] == info+0x90 */
            info->clientFrame >= coduo_int32_from_bits((uint32_t)cg_clientFrame - 1u)) /* DEC EDX; current/recent emitter */
        {
            /* Emit-basis (field_100/104/108) delta between the owner chunk and this
             * chunk, each component folded to (-180,180] via AngleNormalize180;
             * distA = |normalized angle delta|. AngleVectors is called on the owner's
             * emit basis (its `forward` output at [ESP+0x38] is immediately overwritten
             * by the delta below — reproduced faithfully). */
            float fwd[3]; /* [ESP+0x38] */
            float angInput[3]; /* [ESP+0x18] */
            /* distA, posDiff and distB are NEVER stored: the whole
             * squares/FADDP/FSQRT chain (0x30025eb9..0x30025f0f) runs on the
             * x87 stack, so these stay long double (BoxOnPlaneSide precedent).
             * Only the distB*0.05f product gets a ROUNDED float copy (FST
             * [ESP+0x58] at 0x30025f1d), reloaded when it wins the max. */
            long double distA, distB, metricFull, metric;
            long double posDiff0, posDiff1, posDiff2;
            float metricMem; /* the rounded [ESP+0x58] copy */
            double ramp; /* genuinely spilled to a double slot
                                      * ([ESP+0x18], FSTP at 0x30025f72) */
            long double result; /* _CIpow leaves ST0 raw; the FSUBR 1.0 /
                                      * FMUL bcScaled below consume it with no
                                      * store, so result stays 80-bit (powl) */
            int i;

            angInput[0] = ownerChunk->emitBasis[0];
            angInput[1] = ownerChunk->emitBasis[1];
            angInput[2] = ownerChunk->emitBasis[2];
            AngleVectors(angInput, fwd, NULL, NULL); /* forward output is dead */

            fwd[0] = ownerChunk->emitBasis[0] - chunk->emitBasis[0];
            fwd[1] = ownerChunk->emitBasis[1] - chunk->emitBasis[1];
            fwd[2] = ownerChunk->emitBasis[2] - chunk->emitBasis[2];
            for (i = 0; i < 3; i++)
                fwd[i] = AngleNormalize180(fwd[i]); /* in place (push offsets the FSTP) */
            /* Inline FSQRT chains; FADDP order is (z^2 + y^2) + x^2. */
            distA = coduo_x87_sqrtl((long double)fwd[2] * fwd[2] + (long double)fwd[1] * fwd[1] + (long double)fwd[0] * fwd[0]);

            posDiff0 = (long double)chunk->emitOrigin[0] - ownerChunk->emitOrigin[0];
            posDiff1 = (long double)chunk->emitOrigin[1] - ownerChunk->emitOrigin[1];
            posDiff2 = (long double)chunk->emitOrigin[2] - ownerChunk->emitOrigin[2];
            distB = coduo_x87_sqrtl(posDiff2 * posDiff2 + posDiff1 * posDiff1 + posDiff0 * posDiff0);

            /* metric = max(distA, distB * 0.05f). The FCOMPP at 0x30025f23
             * compares the UNROUNDED distA and product; but when the product
             * wins, the code reloads the ROUNDED float copy from [ESP+0x58]. */
            metricFull = distB * 0.05f;
            metricMem = (float)metricFull; /* FST float [ESP+0x58] @0x30025f1d */
            if (distA > metricFull)
                metric = distA;
            else
                metric = metricMem;

            /* Piecewise ramp on metric: 0.0 above 30.5, 1.0 below -6.8, else a linear
             * fall 1.0 -> 0.0 across [-6.8, 30.5]. The -6.8 pool double is the
             * float-widened value (double)-6.8f, not exact -6.8:
             * 0x3007c320 = 0xC01B333340000000 = -6.800000190734863. */
            /* TEST AH,5 / JP also selects this arm for an unordered metric. */
            if (!(metric < 30.5)) {
                ramp = 0.0; /* FLD 0.0 (0x3007bcf0) */
            } else if (metric < (double)-6.8f) { /* 0x3007c320 */
                ramp = 1.0; /* FLD 1.0 (0x3007bcf8) */
            } else {
                /* FSUB/FMUL/FSUBR run on the st0 metric with no narrowing;
                 * the one rounding is the FSTP double into ramp (0x30025f72). */
                ramp = 1.0 - (metric - (double)-6.8f) * 0.026809651337438734;
            }

            /* result = CG_pow(ramp, 1.0): base ramp is a genuine double (FSTP
             * double [ESP+0x18]; FLD double before the CALL) so it is not widened,
             * but the raw ST0 result is consumed by FSUBR/FMUL with no store, so
             * powl (long double result) is required -- a double pow would round. */
            result = powl(ramp, 1.0L);
            x = (1.0 - result) * bcScaled;
        } else {
            /* No matching owner chunk: x = field_bc*1800, halved-ish by 0.35 when
             * field_2c == 0. */
            x = bcScaledRaw;
            if (chunk->kind == 0)
                x *= (double)0.35f; /* 0x3007c310 = 0x3FD6666660000000 = (double)0.35f */
        }

        x *= elapsedF8;
        /* Advance the chunk (dt = -x), then stamp field_f8 = (double)flameTime. */
        CG_UpdateFlameChunk(chunk, (float)(-x)); /* EAX=chunk, [ESP]=-x */
        chunk->endTimeCopy3 = (double)flameTime;
    }

    /* ---- Block B (0x30025fbc): free-chunk (boneHandle == 0) damage/drift update ----
     * When boneHandle != 0 the whole block is skipped straight to the tail. */
    if (chunk->boneHandle == 0) {
        /* Gate 1: proceed to the drift update only when the chunk is either still
         * growing (field_94 > 1.0f) or has a nonzero expansion rate (field_a8 != 0). */
        if (chunk->driftSpeed > 1.0f || chunk->expansionRate != 0.0f) {
            /* Gate 2: elapsed = flameTime - field_80 must exceed 20.0. The
             * difference is never stored (FILD/FSUB feed FCOMP 20.0 directly at
             * 0x30026001), so it stays at register precision. */
            long double elapsed80 = (long double)flameTime - (long double)chunk->driftStartTime;
            if (elapsed80 > 20.0) {
                float origin[3]; /* [ESP+0x44] */

                CG_ComputeFlameChunkOrigin(chunk, flameTime, origin); /* into [ESP+0x44] */

                /* When the chunk's own end-time (field_d0) has already passed
                 * (field_d0 >= flameTime) or it is a smoke chunk (field_2c == 5),
                 * just copy the computed origin into field_70/74/78 and stamp
                 * field_80 = flameTime (0x30026572 epilogue); otherwise run the
                 * damage-source / drift update. */
                /* 0x30026023..35 also takes this path when field_d0 is NaN. */
                if (!(chunk->endTimeCopy < (double)flameTime) || chunk->kind == 5) {
                    chunk->driftStartTime = (double)flameTime;
                    chunk->localPos[0] = origin[0];
                    chunk->localPos[1] = origin[1];
                    chunk->localPos[2] = origin[2];
                } else {
                    /* Extrapolate a FUTURE origin (flameTime + 350) and measure it
                     * against the current best flame-damage source's hit point.
                     * futureTime is also latched into stack slot [ESP+0x58] at
                     * 0x30026051 (MOV [ESP+0x60],EAX with two args pushed, EAX =
                     * flameTime+0x15e from 0x30026049 LEA EAX,[EDI+0x15e]); both
                     * later FILD [ESP+0x58] reads (0x30026093, 0x3002613a) are
                     * dominated by that store, so the end-time updates below use
                     * this same future timestamp. */
                    float futureOrigin[3]; /* [ESP+0x44] */
                    float dist;
                    int32_t futureTime = coduo_int32_from_bits((uint32_t)flameTime + 350u); /* LEA EAX,[EDI+350] */
                    CG_ComputeFlameChunkOrigin(chunk, futureTime, futureOrigin);
                    dist = VectorDistance(futureOrigin, cg_flameDamageTrace.endpos);

                    if (dist < 2.0f && cg_flameDamageTrace.fraction == 1.0f) {
                        /* The stored best source is still valid (fraction == 1.0)
                         * and close: reuse it. field_c0..c8 <- endpos, field_d0 <-
                         * (double)futureTime, then recompute the chunk origin. */
                        chunk->posCopy[0] = cg_flameDamageTrace.endpos[0];
                        chunk->posCopy[1] = cg_flameDamageTrace.endpos[1];
                        chunk->posCopy[2] = cg_flameDamageTrace.endpos[2];
                        chunk->endTimeCopy = (double)futureTime; /* FILD [ESP+0x58] @0x30026093 */
                        CG_ComputeFlameChunkOrigin(chunk, flameTime, &chunk->localPos[0]);
                        chunk->driftStartTime = (double)flameTime;
                    } else {
                        /* Run a fresh flame-damage trace into cg_flameDamageTrace.
                         * CG_FlamethrowerTrace register args: EAX=contentMask,
                         * ECX=maxs(&0x300851d0), EDX=mins(&0x300851c4). Stack:
                         * out=&cg_flameDamageTrace, arg1=&field_c0, origin=[ESP+0x44],
                         * entityNum=field_34. (ECX/EDX are the trace bounds pointers
                         * passed where the caller-observed decl models flags/arg2 ints;
                         * reproduced faithfully.)
                         *
                         * [ESP+0x44] is one shared buffer that CG_ComputeFlameChunkOrigin
                         * last wrote as futureOrigin at 0x30026055 (LEA ECX,[ESP+0x44]);
                         * nothing rewrites it before this trace (VectorDistance only
                         * reads it), so the DLL passes the futureOrigin value here.
                         * The recon splits that one slot into `origin`/`futureOrigin`,
                         * so this call must pass `futureOrigin` to stay byte-faithful. */
                        CG_FlamethrowerTrace((int32_t)FLAME_DAMAGE_TRACE_CONTENTMASK, cg_flameTraceMaxs, cg_flameTraceMins,
                                             &cg_flameDamageTrace, chunk->posCopy, futureOrigin, chunk->ownerInfoIndex);

                        if (cg_flameDamageTrace.startsolid != 0) {
                            /* Fresh hit: seed the trace record's fraction/endpos from
                             * the chunk's stored contact point (field_c0..c8). */
                            cg_flameDamageTrace.fraction = 0.0f; /* MOV [..b8],0 */
                            cg_flameDamageTrace.endpos[0] = chunk->posCopy[0];
                            cg_flameDamageTrace.endpos[1] = chunk->posCopy[1];
                            cg_flameDamageTrace.endpos[2] = chunk->posCopy[2];
                        }

                        /* Interpolate field_d0 toward futureTime by fraction, copy the
                         * endpos into field_c0..c8, recompute the origin. */
                        chunk->posCopy[0] = cg_flameDamageTrace.endpos[0];
                        chunk->posCopy[1] = cg_flameDamageTrace.endpos[1];
                        chunk->posCopy[2] = cg_flameDamageTrace.endpos[2];
                        chunk->endTimeCopy = (double)((long double)chunk->endTimeCopy +
                                                      ((long double)futureTime - (long double)chunk->endTimeCopy) * /* FILD @0x3002613a */
                                                          (long double)cg_flameDamageTrace.fraction);
                        CG_ComputeFlameChunkOrigin(chunk, flameTime, origin);

                        if ((double)flameTime < chunk->endTimeCopy) {
                            /* Not yet at end time: adopt the computed origin. */
                            chunk->driftStartTime = (double)flameTime;
                            chunk->localPos[0] = origin[0];
                            chunk->localPos[1] = origin[1];
                            chunk->localPos[2] = origin[2];
                        } else {
                            /* At/after end time. If the trace surface has a nonzero
                             * normal (any of normal[0..2] != 0) copy it into the
                             * chunk's sprite-offset field_118/11c/120. */
                            if (cg_flameDamageTrace.fraction < 1.0f) {
                                if (cg_flameDamageTrace.normal[0] != 0.0f || cg_flameDamageTrace.normal[1] != 0.0f ||
                                    cg_flameDamageTrace.normal[2] != 0.0f) {
                                    chunk->centerOffset[0] = cg_flameDamageTrace.normal[0];
                                    chunk->centerOffset[1] = cg_flameDamageTrace.normal[1];
                                    chunk->centerOffset[2] = cg_flameDamageTrace.normal[2];
                                }
                            }

                            if (cg_flameDamageTrace.startsolid != 0) {
                                /* A fresh source was latched: kill the chunk's drift
                                 * (field_88/8c/90/94 <- 0) and finish. */
                                chunk->driftSpeed = 0.0f;
                                chunk->driftDir[2] = 0.0f;
                                chunk->driftDir[1] = 0.0f;
                                chunk->driftDir[0] = 0.0f;
                            } else if ((cg_flameDamageTrace.surfaceFlags & 0x10u) != 0) {
                                /* Surface flagged 0x10 (CONTENTS bit): nothing to do,
                                 * fall to the common tail (0x30026593). */
                            } else {
                                /* No fresh source and no 0x10 flag: reproject the drift.
                                 * field_70/74/78 = field_d0=field_80=flameTime origin
                                 * seeded from field_c0..c8, unless the current source is
                                 * exactly at fraction==1.0. */
                                chunk->endTimeCopy = (double)flameTime; /* FST field_d0 */
                                chunk->driftStartTime = (double)flameTime; /* FST field_80 */
                                chunk->localPos[0] = chunk->posCopy[0];
                                chunk->localPos[1] = chunk->posCopy[1];
                                chunk->localPos[2] = chunk->posCopy[2];

                                if (cg_flameDamageTrace.fraction == 1.0f) {
                                    /* fraction == 1.0: nothing more (0x30026593 tail). */
                                } else {
                                    /* Reproject: origin <- endpos, drift velocity
                                     * v = field_94*(field_88,8c,90) clipped against the
                                     * surface normal (overbounce 1.1), renormalized, and
                                     * field_94 = 0.25*|v|; field_bc = 0.5. */
                                    float drift[3];
                                    float len;
                                    chunk->localPos[0] = cg_flameDamageTrace.endpos[0];
                                    chunk->localPos[1] = cg_flameDamageTrace.endpos[1];
                                    chunk->localPos[2] = cg_flameDamageTrace.endpos[2];
                                    chunk->driftStartTime = (double)flameTime;
                                    chunk->worldPos[0] = cg_flameDamageTrace.endpos[0];
                                    chunk->worldPos[1] = cg_flameDamageTrace.endpos[1];
                                    chunk->worldPos[2] = cg_flameDamageTrace.endpos[2];

                                    drift[0] = chunk->driftSpeed * chunk->driftDir[0];
                                    drift[1] = chunk->driftDir[1] * chunk->driftSpeed;
                                    drift[2] = chunk->driftDir[2] * chunk->driftSpeed;
                                    PM_ClipVelocity(drift, cg_flameDamageTrace.normal, &chunk->driftDir[0], 1.1f);
                                    len = VectorNormalize(&chunk->driftDir[0]);
                                    chunk->driftSpeed = len * 0.25f;
                                    chunk->alpha = 0.5f;

                                    /* ---- Flame-damage spawn gate (0x3002632d) ----
                                     * Attempted only when field_124 == 0 AND the trace's hit
                                     * entity is a world (non-client) entity AND the surface
                                     * kind is not one of the three excluded ({0x13,0x14,0xc}
                                     * << 20). Any of those surface/entity gates failing skips
                                     * straight to stamping field_124 (0x30026565), leaving
                                     * field_14 untouched. Once past those gates (0x3002637b),
                                     * field_14 is ALWAYS cleared (0x3002655e) whatever the
                                     * sub-outcome. */
                                    if (chunk->damageFrameStamp == 0 && (int32_t)cg_flameDamageTrace.entityNum >= cgs_maxclients &&
                                        (cg_flameDamageTrace.surfaceFlags & FLAME_SURF_KIND_MASK) != FLAME_SURF_KIND_1300 &&
                                        (cg_flameDamageTrace.surfaceFlags & FLAME_SURF_KIND_MASK) != FLAME_SURF_KIND_1400 &&
                                        (cg_flameDamageTrace.surfaceFlags & FLAME_SURF_KIND_MASK) != FLAME_SURF_KIND_0C00) {
                                        /* field_2c != 0 or field_14 == 0 -> just clear
                                         * field_14 (no-op if already 0) and stamp. */
                                        if (chunk->kind == 0 && chunk->ownerSentinel != 0) {
                                            /* 0x30026391 reloads +0x34 and forms a fresh
                                             * owner-info row; it does not retain the row
                                             * used by block A above. */
                                            cgFlameInfo_t *damageInfo = &cg_flameInfo[chunk->ownerInfoIndex];
                                            int32_t flameClock = coduo_int32_from_bits(cg_flameTime);
                                            int32_t lastStamp = damageInfo->lastFlameStamp; /* [ESP+0x58] */
                                            int doSpawn = 0;

                                            /* Rate-limit: last source must be > 200 ms old. */
                                            if (lastStamp < coduo_int32_from_bits((uint32_t)flameClock - 200u)) {
                                                /* If the last stamp is in the future
                                                 * (lastStamp > cg_flameTime) or older than
                                                 * 6000 ms, spawn unconditionally; otherwise
                                                 * require the new source > 32 units away. */
                                                if (lastStamp > flameClock || (double)flameClock - 6000.0f > (double)lastStamp) {
                                                    doSpawn = 1;
                                                } else {
                                                    float d = VectorDistance(&chunk->worldPos[0], damageInfo->lastFlamePos);
                                                    if (d > 32.0f)
                                                        doSpawn = 1;
                                                }
                                            }

                                            if (doSpawn) {
                                                flameChunk_t *child;
                                                float spawnA;

                                                damageInfo->lastFlameStamp = coduo_int32_from_bits(cg_flameTime);
                                                damageInfo->lastFlamePos[0] = chunk->worldPos[0];
                                                damageInfo->lastFlamePos[1] = chunk->worldPos[1];
                                                damageInfo->lastFlamePos[2] = chunk->worldPos[2];

                                                /* a = min(field_e4, 145.0f), b = -0.2f. */
                                                spawnA = (chunk->radius < 145.0f) ? chunk->radius : 145.0f;
                                                child = CG_FlameDropDrip(chunk, spawnA, -0.2f);
                                                if (child != NULL) {
                                                    /* r0/r1/r3 are NEVER stored: each FILD feeds
                                                     * its FMUL directly (0x3002648f, 0x300264d0,
                                                     * 0x3002653a -- no intervening store, so no
                                                     * (float) cast on rand()) and each chain runs
                                                     * unbroken to a single FSTP. double locals
                                                     * would insert roundings the DLL lacks. */
                                                    long double r0, r1, r3;
                                                    child->spawnTime = (double)coduo_int32_from_bits(cg_flameTime);

                                                    /* end time field_50 = field_48 + r0 +
                                                     *   2000*(1 + 0.5*(1 - |normal[2]|)),
                                                     * r0 = rand()*(1/32768) * 2000.
                                                     * NB the DLL uses BOTH widths of 2000 here:
                                                     * r0's scale is FMUL m32 float 2000.0f
                                                     * (0x3007bee4 @0x30026499), the sibling term
                                                     * is FMUL m64 double 2000.0 (0x3007c300
                                                     * @0x300264b9). */
                                                    r0 = (long double)coduo_crt_rand() * (long double)3.0517578125e-05f *
                                                         (long double)2000.0f;
                                                    child->endTime =
                                                        (double)(r0 +
                                                                 ((long double)1.0 +
                                                                  (long double)0.5 *
                                                                      ((long double)1.0 -
                                                                       __builtin_fabsl((long double)cg_flameDamageTrace.normal[2]))) *
                                                                     (long double)2000.0 +
                                                                 (long double)child->spawnTime);

                                                    /* With p < 0.3, seed field_18 =
                                                     * Q_rint(lerp(field_48, field_50, r2)),
                                                     * r2 = rand()/32768*0.45 + 0.25. */
                                                    r1 = (long double)coduo_crt_rand() * (long double)3.0517578125e-05f;
                                                    if (r1 < 0.3) {
                                                        /* FMUL 0x3007c174 / FADD 0x3007be58 are FLOAT
                                                         * (DWORD) loads: 0.45f and 0.25f, not the double
                                                         * literals (0.45f != 0.45). */
                                                        long double r2 = (long double)coduo_crt_rand() * (long double)3.0517578125e-05f *
                                                                             (long double)0.45f +
                                                                         (long double)0.25f;
                                                        /* The _ftol2 at 0x30026517 converts the RAW st0
                                                         * sum -- nothing stores it, so no (float) cast.
                                                         * (The FST float [ESP+0x58] at 0x30026511 is a
                                                         * dead store of spawnTime into the rand scratch
                                                         * slot; the FADDP at 0x30026515 runs on the
                                                         * unrounded st0 and no float read of that slot
                                                         * follows.) */
                                                        child->emitScatterIndex = (uint32_t)coduo_fp_to_i32_extended(
                                                            (long double)child->spawnTime +
                                                            ((long double)child->endTime - (long double)child->spawnTime) * r2);
                                                    }

                                                    /* field_138 = 150.0 / (field_50 - field_48). */
                                                    child->lifeRate =
                                                        (double)(150.0L / ((long double)child->endTime - (long double)child->spawnTime));
                                                    /* field_130 = cg_flameTime +
                                                     *   r3*(field_50-field_48)*field_138. */
                                                    r3 = (long double)coduo_crt_rand() * (long double)3.0517578125e-05f;
                                                    child->lifeStartTime =
                                                        (double)(r3 * ((long double)child->endTime - (long double)child->spawnTime) *
                                                                     (long double)child->lifeRate +
                                                                 (long double)coduo_int32_from_bits(cg_flameTime));
                                                }
                                            }
                                        }

                                        chunk->ownerSentinel = 0; /* 0x3002655e */
                                    }
                                    chunk->damageFrameStamp = coduo_int32_from_bits(cg_flameTime); /* 0x30026565 */
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /* ---- Common tail (0x3002659b) ----
     * Recompute the chunk's world origin into field_d8/dc/e0 and record this chunk
     * as the last one processed this frame. */
    CG_ComputeFlameChunkOrigin(chunk, flameTime, &chunk->worldPos[0]);
    cg_lastFlameChunkProcessed = chunk;
}
