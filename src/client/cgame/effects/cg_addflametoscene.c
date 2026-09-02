// Source: uo_cgame_mp_x86.dll 0x300268e0..0x300272ac
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300268e0_300272ac.mcode
//
// CG_AddFlameToScene builds and submits one four-vertex flame/smoke billboard.
// The same-module CG_AddFlameSpriteToScene name describes this routine too; the
// shorter name is retained because its reconstructed caller already uses it.

#include "../client_recovered.h"
#include "../globals.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

enum {
    FLAME_KIND_NO_SPRITE_AFTER_CLOCK = 1,
    FLAME_KIND_DAMAGE = 2,
    FLAME_KIND_NO_SPRITE = 3,
    FLAME_KIND_SMOKE = 5,
    FLAME_FIRE_FRAME_COUNT = 43,
    FLAME_POLY_VERTEX_COUNT = 4,
    FLAME_NEAR_SPRITE_LIMIT = 50
};

void CG_AddFlameToScene(flameChunk_t *chunk, float animationFraction, float alpha, int32_t finalFrame)
{
    if (chunk->kind == FLAME_KIND_NO_SPRITE) {
        return;
    }

    /* 0x30026904..0x3002692c: this function refreshes the shared flame clock.
     * FILD; FADD ST0,ST0; FSTP double [ESP] (the floor argument IS rounded to
     * double); CALL floor; CALL _ftol2 — the floor result goes RAW into _ftol2
     * with no float store, so no (float) cast here. */
    cg_flameTime = (uint32_t)coduo_fp_to_i32_extended(floor((double)coduo_int32_from_bits(cg_time) * 2.0));

    if (alpha < 0.0f || chunk->kind == FLAME_KIND_NO_SPRITE_AFTER_CLOCK) {
        return;
    }

    /* The damage probe is randomized and only runs during the first 90% of life. */
    if (!(chunk->kind == FLAME_KIND_DAMAGE && chunk->ownerSentinel != UINT32_MAX) && coduo_crt_rand() % 5 == 0 &&
        chunk->lifeFraction < 0.89999998f) {
        CG_FlameDamage(chunk->worldPos, chunk->ownerClientNum, chunk->radius, chunk);
    }

    const float selectedRadius = finalFrame == 1 ? chunk->radiusBaseA : chunk->radiusBaseB;
    const int32_t selectedRadiusInt = coduo_fp_to_i32_extended(selectedRadius);
    float halfExtent = chunk->radius * 0.5f;
    vec3_t center = {chunk->worldPos[0], chunk->worldPos[1], chunk->worldPos[2]};

    if ((chunk->centerOffset[0] != 0.0f || chunk->centerOffset[1] != 0.0f || chunk->centerOffset[2] != 0.0f) &&
        chunk->centerOffset[2] < 0.0f) {
        center[2] = (float)((long double)center[2] - (long double)fabsf(chunk->centerOffset[2]) * (long double)20.0f);
    }

    /* 0x30026a4b..0x30026aed: camera-facing displacement for the local owner.
     * The FLD [ESP+0x14] at 0x30026a4b executes BEFORE the PUSH ESI at
     * 0x30026a4f, so it reads the pre-push slot written at 0x300269e7
     * (center[0] = worldPos[0]) — an ordinary center - vieworg vector. */
    vec3_t facing = {center[0] - cg_refdef.vieworg[0], center[1] - cg_refdef.vieworg[1], center[2] - cg_refdef.vieworg[2]};
    const float facingDistance = VectorNormalize(facing);
    /* 0x30026a7f..0x30026a9e: both arms of the select (FLD halfExtent; FCHS /
     * FLD ST0; FCHS; FSUB 20.0f) leave the value RAW on the x87 stack, and
     * every consumer below (the three FMULs and the FDIV at 0x30026ad9) reads
     * that register — the DLL never stores it, so it is long double. */
    const long double placement = halfExtent < facingDistance ? -halfExtent : -(long double)facingDistance - 20.0f;

    if (chunk->ownerClientNum == cg_snap->ps.psClientNum) {
        center[0] += facing[0] * placement;
        center[1] += facing[1] * placement;
        center[2] += facing[2] * placement;
        halfExtent *= placement / facingDistance + 1.0f;
    }
    if (halfExtent < 0.5f) {
        halfExtent = 0.5f;
    }

    /* 0x30026b08..0x30026b31: the DLL subtracts ONCE (FLD lifeFraction; FSUB
     * 0.5f) and compares that difference against 0.0f (FCOM; TEST AH,0x41;
     * JNZ), storing the already-computed difference on the positive arm. */
    const long double lateLifeDelta = (long double)chunk->lifeFraction - (long double)0.5f;
    const float lateLife = lateLifeDelta > 0.0f ? (float)lateLifeDelta : 0.0f;
    /* 0x30026b31..0x30026b54: FLD lateLife; FADD ST0,ST0; FSUBR 1.0f; FLD
     * double 1.0; CALL pow; FMUL float 80.0f (0x3007bfb0); FADD float 20.0f
     * (0x3007be04); CALL _ftol2. The raw pow st0 result, the two FLOAT-width
     * constants and the _ftol2 argument never touch memory — powl keeps the
     * base (operand widened) and the result 80-bit; a (float)/double pow would
     * round where the DLL does not. */
    const int32_t colorLevel = coduo_fp_to_i32_extended(powl(1.0f - (long double)lateLife * 2.0f, 1.0L) * 80.0f + 20.0f);

    uint8_t rgba[4];
    if (chunk->kind == FLAME_KIND_SMOKE) {
        /* 0x30026b61 MOVZX EAX,AL: the smoke color base keeps only the low
         * byte of the _ftol2 result. No (float) cast on it: 0x30026b68 FILDs it
         * with no FSTP DWORD, and it stays RAW in st(1) across all three
         * channels (each does FLD const; FMUL ST1), so it enters every multiply
         * exact. */
        const int32_t smokeColorBase = (int32_t)(uint8_t)colorLevel;
        rgba[0] = (uint8_t)coduo_fp_to_i32_extended((long double)smokeColorBase * (long double)0.97647059f);
        rgba[1] = (uint8_t)coduo_fp_to_i32_extended((long double)smokeColorBase * (long double)0.93725491f);
        rgba[2] = (uint8_t)coduo_fp_to_i32_extended((long double)smokeColorBase * (long double)0.85882354f);
        rgba[3] = (uint8_t)coduo_fp_to_i32_extended((long double)alpha * (long double)76.5f);
    } else if (chunk->kind == FLAME_KIND_DAMAGE && chunk->ownerSentinel == UINT32_MAX) {
        /* 0x30026bc2..0x30026c31: FLD lifeFraction; FADD ST0,ST0 — the doubled
         * value is never stored, it lives in an x87 register across all three
         * channels, and each channel re-runs the FCOM 1.0f clamp on it before
         * FMUL 255.0f / _ftol2. A float local would round it once up front. */
        const long double doubledLife = (long double)chunk->lifeFraction + (long double)chunk->lifeFraction;
        /* Each channel repeats FCOM/_ftol2. TEST AH,5 selects 1.0 for greater,
         * equal, or unordered doubledLife; a shared conversion would lose two
         * original conversions and would propagate NaN instead of clamping it. */
        rgba[0] = (uint8_t)coduo_fp_to_i32_extended((!(doubledLife < 1.0f) ? 1.0L : doubledLife) * 255.0f);
        rgba[1] = (uint8_t)coduo_fp_to_i32_extended((!(doubledLife < 1.0f) ? 1.0L : doubledLife) * 255.0f);
        rgba[2] = (uint8_t)coduo_fp_to_i32_extended((!(doubledLife < 1.0f) ? 1.0L : doubledLife) * 255.0f);
        rgba[3] = UINT8_MAX;
    } else {
        rgba[0] = UINT8_MAX;
        rgba[1] = UINT8_MAX;
        rgba[2] = UINT8_MAX;
        rgba[3] = UINT8_MAX;
    }

    if (chunk->kind != FLAME_KIND_SMOKE && cg_rFullscreenCvar.integer != 0 && cg_rOverbrightBitsCvar.integer != 0) {
        rgba[0] >>= 1;
        rgba[1] >>= 1;
        rgba[2] >>= 1;
    }

    /* Project the center onto the camera-forward line and reject back/zero-depth
     * billboards. A bounded counter throttles sprites intersecting the camera. */
    vec3_t forwardEnd = {(float)((long double)cg_refdef.vieworg[0] + (long double)cg_refdef.viewaxis[0][0] * 1024.0L),
                         (float)((long double)cg_refdef.vieworg[1] + (long double)cg_refdef.viewaxis[0][1] * 1024.0L),
                         (float)((long double)cg_refdef.vieworg[2] + (long double)cg_refdef.viewaxis[0][2] * 1024.0L)};
    vec3_t projected;
    ProjectPointOnLine(projected, cg_refdef.vieworg, center, forwardEnd);
    vec3_t projectedDirection = {projected[0] - cg_refdef.vieworg[0], projected[1] - cg_refdef.vieworg[1],
                                 projected[2] - cg_refdef.vieworg[2]};
    const float projectedDistance = VectorNormalize(projectedDirection);
    if (projectedDistance == 0.0f) {
        return;
    }
    /* 0x30026d5b..0x30026d7d: the dot is summed z,y,x (FADDP pairs) and
     * FCOMP'd against 0.0f RAW — no store anywhere in the chain — so it is
     * long double, in the binary's summation order. */
    const long double forwardDot = (long double)cg_refdef.viewaxis[0][2] * (long double)projectedDirection[2] +
                                   (long double)cg_refdef.viewaxis[0][1] * (long double)projectedDirection[1] +
                                   (long double)cg_refdef.viewaxis[0][0] * (long double)projectedDirection[0];
    if (forwardDot < 0.0f) {
        return;
    }
    if (projectedDistance < chunk->radius) {
        const uint32_t previousBits = cg_flameDamageBillboardCount;
        const int32_t previous = coduo_int32_from_bits(previousBits);
        cg_flameDamageBillboardCount = previousBits + 1u; /* INC EAX */
        if (previous > FLAME_NEAR_SPRITE_LIMIT) {
            return;
        }
    }

    if (cg_flameSpriteViewRelative != 0) {
        vec3_t angles;
        vectoangles(cg_refdef.viewaxis[0], angles);
        angles[2] = (float)selectedRadiusInt;
        AngleVectors(angles, NULL, cg_flameSpriteRight, cg_flameSpriteUp);
    } else {
        cg_flameSpriteRight[0] = cg_flameSpriteSrcRight[0];
        cg_flameSpriteRight[1] = cg_flameSpriteSrcRight[1];
        cg_flameSpriteRight[2] = cg_flameSpriteSrcRight[2];
        cg_flameSpriteUp[0] = cg_flameSpriteSrcUp[0];
        cg_flameSpriteUp[1] = cg_flameSpriteSrcUp[1];
        cg_flameSpriteUp[2] = cg_flameSpriteSrcUp[2];
    }

    polyVert_t verts[FLAME_POLY_VERTEX_COUNT];
    for (int32_t i = 0; i < FLAME_POLY_VERTEX_COUNT; ++i) {
        verts[i].modulate[0] = rgba[0];
        verts[i].modulate[1] = rgba[1];
        verts[i].modulate[2] = rgba[2];
        verts[i].modulate[3] = rgba[3];
    }

    /* Canonical unit-quad texture coordinates, stored interleaved with the
     * position math in the machine code: v0 (0,0) at 0x30026e2f/0x30026e39,
     * v1 (0,1) at 0x30026e47/0x30026e51, v2 (1,1) at 0x30026e59/0x30026e64,
     * v3 (1,0) at 0x30026f95/0x30026fa0 (inside the SUB ESP,8 bracket). */
    verts[0].st[0] = 0.0f;
    verts[0].st[1] = 0.0f;
    verts[1].st[0] = 0.0f;
    verts[1].st[1] = 1.0f;
    verts[2].st[0] = 1.0f;
    verts[2].st[1] = 1.0f;
    verts[3].st[0] = 1.0f;
    verts[3].st[1] = 0.0f;

    /* 0x30026e2b..0x30026fca: the quad is ONE x87 chain with asymmetric
     * spills, transcribed store-for-store:
     *   - the up-displaced corner temp rounds through memory for Z only
     *     (FSTP [ESP+0x20]); its X and Y stay raw in st registers;
     *   - the Y chain (verts[0] -> verts[1] -> verts[2]) and the Z chain
     *     (verts[1] -> verts[2] -> verts[3]) carry the RAW st value into the
     *     next vertex (FADDP / FADD ST0,STn), while the X components (and
     *     verts[0].z -> verts[1].z) reload the ROUNDED stores;
     *   - verts[3] is NOT built from verts[0]: the DLL derives it from
     *     verts[2] by removing the up displacement (FMUL -2.0f @0x3007c150).
     * halfExtent+halfExtent / halfExtent*-2.0f are exact, so the doubled
     * extents can live in the float scratch the DLL spills them to. */
    {
        const long double negExtent = -halfExtent;               /* FCHS, raw */
        const long double upCornerX = negExtent * cg_flameSpriteUp[0] + center[0];         /* raw */
        const long double upCornerY = negExtent * cg_flameSpriteUp[1] + center[1];         /* raw */
        const float upCornerZ = (float)(negExtent * cg_flameSpriteUp[2] + center[2]); /* FSTP [ESP+0x20] */

        verts[0].xyz[0] = (float)(negExtent * cg_flameSpriteRight[0] + upCornerX);
        const long double corner0Y = negExtent * cg_flameSpriteRight[1] + upCornerY;      /* raw */
        verts[0].xyz[1] = (float)corner0Y;
        verts[0].xyz[2] = (float)(negExtent * cg_flameSpriteRight[2] + upCornerZ);

        const long double doubledExtentRaw = (long double)halfExtent + (long double)halfExtent;  /* retained ST0 */
        const float doubledExtent = (float)doubledExtentRaw;                            /* non-popping FST scratch */
        verts[1].xyz[0] = (float)(doubledExtentRaw * (long double)cg_flameSpriteUp[0] + (long double)verts[0].xyz[0]);
        const long double corner1Y = corner0Y + doubledExtent * cg_flameSpriteUp[1];      /* raw Y carry */
        verts[1].xyz[1] = (float)corner1Y;
        const long double corner1Z = doubledExtent * cg_flameSpriteUp[2] + verts[0].xyz[2];
        verts[1].xyz[2] = (float)corner1Z;

        verts[2].xyz[0] = (float)((long double)doubledExtent * (long double)cg_flameSpriteRight[0] + (long double)verts[1].xyz[0]);
        verts[2].xyz[1] = (float)(doubledExtent * cg_flameSpriteRight[1] + corner1Y); /* raw Y carry */
        const long double corner2Z = doubledExtent * cg_flameSpriteRight[2] + corner1Z;   /* raw Z carry */
        verts[2].xyz[2] = (float)corner2Z;

        const long double negDoubledExtentRaw = (long double)halfExtent * (long double)-2.0f;       /* retained ST0 */
        const float negDoubledExtent = (float)negDoubledExtentRaw; /* non-popping FST scratch */
        verts[3].xyz[0] = (float)(negDoubledExtentRaw * (long double)cg_flameSpriteUp[0] + (long double)verts[2].xyz[0]);
        verts[3].xyz[1] = (float)((long double)negDoubledExtent * (long double)cg_flameSpriteUp[1] + (long double)verts[2].xyz[1]);
        verts[3].xyz[2] = (float)(negDoubledExtent * cg_flameSpriteUp[2] + corner2Z); /* raw Z carry */
    }

    /* 0x30026fcc..0x30026fe4: FMUL float 43.0f (0x3007c218); FSTP double [ESP]
     * (the floor argument IS rounded to double); CALL floor; CALL _ftol2 — the
     * floor result feeds _ftol2 raw, so there is no (float) cast. */
    int32_t frame = coduo_fp_to_i32_extended(floor((double)((long double)animationFraction * (long double)43.0f)));
    if (frame < 0) {
        frame = 0;
    } else if (frame > FLAME_FIRE_FRAME_COUNT - 1) {
        frame = FLAME_FIRE_FRAME_COUNT - 1;
    }

    /* 0x30027005..0x300270d1: the lift folds into each vertex Z as ONE
     * per-vertex chain — FMUL 0.55f; FADD verts[i].z; FSUB 8.8f; FSTP — so the
     * association is (min*0.55f + z) - 8.8f with a single rounding per vertex;
     * the lift term itself is never rounded on its own and must not be
     * hoisted into a float. The min select carries an exact float value. */
    const float clampedExtent = halfExtent < 72.5f ? halfExtent : 72.5f;
    for (int32_t i = 0; i < FLAME_POLY_VERTEX_COUNT; ++i) {
        verts[i].xyz[2] =
            (float)((long double)clampedExtent * (long double)0.55000001f + (long double)verts[i].xyz[2] - (long double)8.8000002f);
    }

    if (finalFrame == 1) {
        qhandle_t material = chunk->overrideMaterial != 0 ? (qhandle_t)chunk->overrideMaterial : cg_flameFireMaterials[frame];
        cgame_syscall(CG_R_ADDPOLYTOSCENE, material, FLAME_POLY_VERTEX_COUNT, verts);
        return;
    }

    /* 0x3002714a..0x3002715a: FILD frame; FMUL double 0.7 (0x3007c208); FADD
     * double 20.0 (0x3007c128); CALL _ftol2 — all raw, no store. */
    const int32_t smokeFrame = coduo_fp_to_i32_extended((long double)frame * (long double)0.7 + (long double)20.0);
    if (smokeFrame < FLAME_FIRE_FRAME_COUNT) {
        /* 0x3002716a..0x3002717c: FLD 1.0f; FSUB lifeFraction; FSUB 0.5f;
         * FSTP — the binary computes (1.0f - lifeFraction) - 0.5f as one
         * chain, NOT 0.5f - lifeFraction (same algebra, different rounding). */
        const float smokeLife = (float)((long double)1.0f - (long double)chunk->lifeFraction - (long double)0.5f);
        float clampedSmokeLife = smokeLife;
        /* The two TEST-based selects map unordered smokeLife to zero. */
        if (!(clampedSmokeLife > 0.0f)) {
            clampedSmokeLife = 0.0f;
        } else if (!(clampedSmokeLife < 0.40000001f)) {
            clampedSmokeLife = 0.40000001f;
        }
        const float earlyLife = chunk->lifeFraction < 0.15000001f ? chunk->lifeFraction : 0.15000001f;
        /* 0x30027180..0x30027194: the raw CG_pow st0 result and the +0.5
         * (double @0x3007bd28) stay in st registers across the whole
         * per-vertex loop with no store, so smokeDensity is long double; powl
         * widens the base OPERAND and keeps the result 80-bit (a double pow
         * base/return would round where the DLL does not). */
        const long double smokeDensity = powl((long double)chunk->smokeDensityRate * 0.0071428572f, 1.0L) + 0.5;
        /* 0x300271a3..0x30027237: the lift is recomputed for every vertex as
         * ONE unstored chain whose only rounding is the verts[i].z store:
         * (clamped * 2.5f) FMULP (early * 6.6666665f), then *halfExtent,
         * *0.64999998f (0x3007c1fc), *smokeDensity, FADD z, FSTP z — the
         * grouping and the absence of a float smokeLift temp are part of the
         * rounding behavior. */
        for (int32_t i = 0; i < FLAME_POLY_VERTEX_COUNT; ++i) {
            verts[i].xyz[2] =
                (float)((long double)clampedSmokeLife * (long double)2.5f * ((long double)earlyLife * (long double)6.6666665f) *
                            (long double)halfExtent * (long double)0.64999998f * smokeDensity +
                        (long double)verts[i].xyz[2]);
        }
        cgame_syscall(CG_R_ADDPOLYTOSCENE, cg_flameFireMaterials[smokeFrame], FLAME_POLY_VERTEX_COUNT, verts);
    }

    cg_flameLastSpritePos[0] = chunk->worldPos[0];
    cg_flameLastSpritePos[1] = chunk->worldPos[1];
    cg_flameLastSpritePos[2] = chunk->worldPos[2];
}
