// Source: uo_cgame_mp_x86.dll 0x30048a00..0x30048b5f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30048a00_30048b5f.mcode
//
// CG_SpawnMovingTracer — build the moving/animated bullet-tracer geometry for one
// shot, from the muzzle point (startOrigin, register EDI/EAX) toward the bullet
// impact point (endOrigin, register ECX), and hand two computed vec3 endpoints plus
// a width to the tracer polygon builder CG_DrawMovingTracerPoly (0x30048460).
//
// NAME ADJUDICATION: the .mcode's mechanical `# name G_RunItem` is a broad-corpus
// SIZE-GUESS (win 0x15f ~ matched 0x160 from game_mp.dll) and is REJECTED — this is a
// cgame client tracer builder, not a server item-run entity think. The name
// CG_SpawnMovingTracer comes from its sole caller CG_SpawnTracer (0x30048d60, already
// reconstructed) — the direct-surface branch of that function calls this at 0x30048e28 —
// and is corroborated by the behavior: it reuses the same tracer-mode selection
// (bg_weaponInfos[idx]->ammoType in {3,4,5} -> mode A) and the same
// width/length twins as CG_AddMovingTracer, and it feeds CG_DrawMovingTracerPoly.
// Exact original CoD symbol unproven; role name kept.
//
// ABI (proven from this body + the caller's call site 0x30048e20..0x30048e2b, which is
// `XOR EDX,EDX; MOV ECX,impactOrigin; LEA EAX,&muzzle; PUSH weaponIndex; CALL; ADD ESP,4`):
//   register EAX -> startOrigin     (vec3*, muzzle point; copied into EDI at entry)
//   register ECX -> endOrigin       (vec3*, bullet impact point)
//   register EDX -> weaponInfoIndex (index into bg_weaponInfos[] used ONLY for tracer-mode
//                                    selection; the sole caller passes 0)
//   stack  arg0  -> weaponIndex     (int; only consulted in the near-shot gate below)
// caller-cleaned (RET, no imm; caller does ADD ESP,4). The register/stack split is an
// i386 calling-convention detail; recovered here as ordered C parameters with no
// calling-convention attribute (the syntax-only build does not require one).

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

/*
 * Exact .rdata float constants read by this function (dumped from the binary; do NOT
 * infer from neighbors):
 *   0x3007bff4 = 9.9999997e-07f  (1e-6 width/length lower bound)
 *   0x300715ec = 100.0f          (near-shot distance threshold)
 *   0x3007bec0 = 3.0517578e-05f  (1.0f/32768.0f, rand() -> [0,1) normalizer)
 *   0x3007bff0 = 60.0f           (distance bias in the tail-position scalar)
 *   0x300715c8 = 50.0f           (tail-position scalar base)
 * These are immutable .rdata pool constants; written here as natural float literals
 * with the source address noted, per the constant-form rules.
 */

void CG_SpawnMovingTracer(vec3_t startOrigin, vec3_t endOrigin,
                          int32_t weaponInfoIndex, int32_t weaponIndex)
{
    /* 30048a06..30048a44: select the tracer width/length pair by the weapon's
     * ammo type. bg_weaponInfos[weaponInfoIndex] (0x30134cd8[EDX*4]); if non-null and
     * its ammoType (+0x90) is LMG(3)/HMG(4)/UMG(5), use the "mode A"
     * width/length, otherwise the "mode B" defaults. */
    float tracerWidth;   /* [ESP+0xc] : mode-selected tracer poly width  */
    float tracerLength;  /* ST0/[ESP+0x10] : mode-selected tracer length */
    {
        const weaponInfo_t *weapon = bg_weaponInfos[weaponInfoIndex];
        if (weapon != 0 &&
            (weapon->ammoType == WEAPON_AMMO_TYPE_LMG ||
             weapon->ammoType == WEAPON_AMMO_TYPE_HMG ||
             weapon->ammoType == WEAPON_AMMO_TYPE_UMG)) {
            tracerWidth  = cg_tracerwidthlmg_vmCvar.value;   /* 0x304407c8 */
            tracerLength = cg_tracerlengthlmg_vmCvar.value;  /* 0x304506e8 */
        } else {
            tracerWidth  = cg_tracerwidth_vmCvar.value;   /* 0x3048c268 */
            tracerLength = cg_tracerlength_vmCvar.value;  /* 0x30530668 */
        }
    }

    /* 30048a48..30048a6c: bail unless BOTH the mode-selected length and width are at
     * least ~1e-6 (a disabled/zero tracer pair). FCOMP ...,[1e-6 @0x3007bff4] with the
     * TEST AH,0x5 / JNP idiom skips (returns) when the value is strictly < 1e-6.
     * The length is also parked in [ESP+0x10] here for reuse below. */
    if (tracerLength < 9.9999997e-07f) {   /* 0x3007bff4 */
        return;
    }
    if (tracerWidth < 9.9999997e-07f) {    /* 0x3007bff4 */
        return;
    }

    /* 30048a72..30048a93: dir = endOrigin - startOrigin, then VectorNormalize(dir) which
     * normalizes dir in place and returns the pre-normalization length (the shot span
     * distance) in ST0. ECX=endOrigin, EDI=startOrigin. */
    vec3_t dir;   /* [ESP+0x14..0x1c] : unit direction from muzzle toward impact */
    dir[0] = endOrigin[0] - startOrigin[0];
    dir[1] = endOrigin[1] - startOrigin[1];
    dir[2] = endOrigin[2] - startOrigin[2];
    float dist = VectorNormalize(dir);   /* 0x30049700; also stored to [ESP+0x8] */

    /* 30048a9d..30048ab0: near-shot cull. If the shot span is short (< 100.0) AND the
     * caller's weaponIndex stack arg is zero, draw nothing.
     * FCOMP dist,[100.0 @0x300715ec]; TEST AH,0x5; JP -> proceed when dist >= 100.0;
     * otherwise fall through to the weaponIndex test; JZ -> return when it is 0. */
    if (dist < 100.0f && weaponIndex == 0) {   /* 0x300715ec */
        return;
    }

    /* 30048ab6..30048ae1: tailScalar = 50.0 + (rand()/32768) * (dist - 60.0).
     * rand() (0x3005b879, returns 0..0x7fff) is normalized by
     * (1/32768 @0x3007bec0); dist biased by (60.0 @0x3007bff0); base (50.0 @0x300715c8).
     * This is the animated tracer's tail offset along the ray, jittered per frame. */
    int32_t r = coduo_crt_rand();   /* 0x3005b879 */
    /* r enters via a bare FILD fed straight into FMUL 3.05e-5f (0x30048abf FILD;
     * 0x30048ac3 FMUL) with no FSTP DWORD between, so drop the (float) cast (Class 4). */
    long double tailScalarWide =
        50.0L +
        ((long double)r * (long double)3.0517578e-05f) *
        ((long double)dist - 60.0L);
    float tailScalar = (float)tailScalarWide; /* 0x30048adb FST, value remains live */
    /*                  0x300715c8          0x3007bec0            0x3007bff0 */

    /* 30048adf..30048af2: headScalar = min(tailScalar + tracerLength, dist) — the head
     * offset is the tail offset extended by the tracer length, clamped to the shot span
     * so the tracer never overshoots the end point.
     * FADD length; FCOM dist; TEST AH,0x41; JNZ keeps (tail+length) when it is <= dist,
     * else replaces it with dist. */
    long double headScalar =
        tailScalarWide + (long double)tracerLength;
    if (headScalar > (long double)dist) {
        headScalar = (long double)dist;
    }

    /* 30048af4..30048b4e: place the two tracer endpoints along the ray starting at
     * startOrigin (EDI), stepping by dir. tail = startOrigin + tailScalar*dir,
     * head = startOrigin + headScalar*dir. Each component is computed and FSTP'd into
     * its stack slot. */
    vec3_t tail;   /* [ESP+0x2c..0x34] : passed to the builder in EDI */
    vec3_t head;   /* [ESP+0x20..0x28] : passed to the builder as a pushed pointer */
    tail[0] = (float)((long double)tailScalar * (long double)dir[0] +
                      (long double)startOrigin[0]);
    tail[1] = (float)((long double)tailScalar * (long double)dir[1] +
                      (long double)startOrigin[1]);
    tail[2] = (float)((long double)tailScalar * (long double)dir[2] +
                      (long double)startOrigin[2]);
    head[0] = (float)((long double)dir[0] * headScalar +
                      (long double)startOrigin[0]);
    head[1] = (float)((long double)dir[1] * headScalar +
                      (long double)startOrigin[1]);
    head[2] = (float)((long double)dir[2] * headScalar +
                      (long double)startOrigin[2]);

    /* 30048b00..30048b57: CG_DrawMovingTracerPoly(tail, head, tracerWidth).
     * PUSH tracerWidth (as its int slot); PUSH &head; EDI=&tail; CALL 0x30048460;
     * ADD ESP,8 (caller cleans the two pushed args). */
    CG_DrawMovingTracerPoly(tail, head, tracerWidth);
}
