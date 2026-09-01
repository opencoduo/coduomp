// Source: uo_cgame_mp_x86.dll 0x300272b0..0x3002783f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300272b0_3002783f.mcode
//
// CG_AddFlameChunks (0x300272b0) — per-frame processing of one flame's chunk
// chain. Walks a flame owner's parent-linked flameChunk_t chain (f = f->parent),
// updates the shared flame-time base, accumulates the per-owner looping-sound
// envelope in cg_flameSoundLoops[], culls/merges adjacent chunks, and adds each
// surviving chunk's sprite to the render scene via CG_AddFlameToScene.
//
// NAME ADJUDICATION — the mechanical pre-hint "G_RadiusDamage" (a game_mp.dll
// SERVER combat function, matched only by byte size 0x59c) is REJECTED: this is a
// cgame (client) routine that touches the flame-chunk subsystem globals
// (cg_flameTime 0x300ab718, cg_flameSoundLoops 0x300a8718, cg_flameInfo
// 0x300ab750, cg_clientFrame 0x30459140, cg_snap 0x30459160,
// cg_refdef.vieworg 0x30487a90) and calls the flame chunk merger
// (CG_MergeFlameChunks 0x300257e0, whose "CG_MergeFlameChunks: f2 doesn't follow
// f1" string anchors the subsystem), the recursive chunk free (CG_FreeFlameChunk
// 0x300256e0) and the per-chunk scene renderer (CG_AddFlameToScene 0x300268e0).
// Behavior + call-graph name taken from the same-module PPC bank
// (CG_AddFlameChunks). The globals.h note that attributes the name to 0x300240a3
// refers to a different function (entry 0x30024056); it is a first-toucher label.
//
// Callee-cleanup: RET (no immediate); the one argument (the flame owner chunk)
// arrives in EAX (register ABI) and is moved to EDI. EBX/EBP/ESI/EDI save and the
// 0x74-byte frame are i386 calling-convention plumbing, not source behavior.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "compat/coduo_native_x87.h"

#include <math.h>

/* 0x3006bb20 is the MSVC x87 pow intrinsic (base ST1, exp ST0, raw 80-bit
 * result in ST0). Portable code uses powl so the base (operand widened) and the
 * result stay 80-bit where the DLL keeps them; a double pow would round both. */

/*
 * CG_AddFlameToScene (0x300268e0, provisional) — build and add one flame chunk's
 * render sprite(s) to the scene (invokes the renderer import at 0x30085e9c,
 * normalizes flame direction, etc.). Called ONLY by CG_AddFlameChunks.
 * Caller-cleaned (ADD ESP,0x10 after four dwords pushed). Observed push order at
 * 0x30027805..0x30027819 (right-to-left): PUSH 1, PUSH <float alpha>, PUSH
 * chunk->lifeFraction (as a dword), PUSH chunk. arity/types UNPROVEN — verify at its
 * own .mcode; the alpha slot is a float and field_e8 is pushed as its raw dword.
 */
/* CG_AddFlameToScene is now declared in client_recovered.h (single source of truth);
 * the local extern was removed to avoid decl drift. */

/*
 * CG_FlameGetSizeRate == 0x30023b70 — flame chunk per-frame size-rate helper
 * (declared in client_recovered.h). ADJUDICATED: this address was once also
 * modelled as CG_ADSAnim_ComputeRate(cg_adsAnimState_t*), but the machine code
 * operates entirely on flameChunk_t fields (+0x2c=kind, +0x34=ownerInfoIndex,
 * +0x48=spawnTime, +0x50=endTime, +0x5c=startSpeedBits, +0x94=driftSpeed,
 * +0xb8=soundAmpRate) and all 7 callers pass a flame chunk (0x30023d3a, 0x30025c73,
 * this site 0x30027825, 0x30027c42, 0x3002843d, 0x30028566, 0x30028ea4). The ADS
 * struct/decl were removed; the helper takes flameChunk_t* directly. Result
 * returns raw on ST(0) with no store (Class 7), so the shared decl returns
 * long double; here it feeds chunk->sizeRate (a float store rounds it) or a
 * further FMUL that consumes the raw value.
 */

void CG_AddFlameChunks(flameChunk_t *ownerChunk)
{
    flameChunk_t *f;        /* EDI: chunk currently being processed */
    flameChunk_t *next;     /* EBX: f->parent for this iteration */
    flameChunk_t *prev;     /* ESI: previously-processed chunk (merge/compare) */
    flameChunk_t *lastKept; /* EBP: last chunk added to the scene */

    int32_t ownerIsLocal;    /* [ESP+0x10] */
    int32_t flameInfoActive; /* [ESP+0x28] */
    int32_t chunkAlive;      /* [ESP+0x2c] */
    double  refTime;         /* [ESP+0x48]: per-flame reference time (double) */
    double  age;             /* [ESP+0x40]: refTime - f->spawnTime (double) */

    /* cg_flameTime = _ftol2(floor(2.0 * cg_time)) — the inlined
     * CG_UpdateFlameTime (0x30023af0). 0x300272b3: FILD cg_time; FSTP double
     * [ESP+0x44]; FLD double [ESP+0x48] (the same slot, across the PUSH EDI at
     * 0x300272c0); FADD ST0,ST0; FSTP double [ESP]; CALL floor (0x3005bcd0);
     * CALL _ftol2 (0x3006be3c). The int32->double round-trip is exact, so the
     * spill is modelled by the (double) cast alone. Spelled the same way as the
     * byte-for-byte equivalent chain in cg_addflametoscene.c (0x30026904),
     * which lacks only the spill/reload pair. */
    cg_flameTime = (uint32_t)coduo_fp_to_i32_extended(
        floor((double)coduo_int32_from_bits(cg_time) * 2.0));

    f = ownerChunk;
    prev = (flameChunk_t *)0; /* XOR ESI,ESI */
    next = (flameChunk_t *)0; /* XOR EBX,EBX */
    lastKept = (flameChunk_t *)0; /* XOR EBP,EBP */

    /* Per-owner flame-info bookkeeping (index = f->ownerInfoIndex, stride 0xb8). */
    {
        int32_t ownerIndex = f->ownerInfoIndex;                       /* [EDI+0x34] */
        cgFlameInfo_t *info = &cg_flameInfo[ownerIndex];

        /* ownerIsLocal (1 iff this info entry owns f AND its stored frame stamp
         * has reached cg_clientFrame): the machine code sets [ESP+0x10]=1 only
         * inside the (info->field_40 == f) arm, then JGE skips the [ESP+0x10]=0
         * store when info->clientFrame >= cg_clientFrame; every other path leaves
         * it 0. (0x300ab790 == cg_flameInfo base + 0x40; 0x300ab750 == base + 0x00.) */
        if (info->ownerChunk == f                              /* CMP EDI,[EAX+0x300ab790] */
            && info->clientFrame >= cg_clientFrame)          /* CMP [+0x300ab750],cg_clientFrame; JGE */
            ownerIsLocal = 1;
        else
            ownerIsLocal = 0;

        /* flameInfoActive = (ownerIndex == cg_snap->ps.psClientNum). SETZ DL on
         * CMP ownerIndex,[cg_snap+0xe0]. */
        flameInfoActive = (ownerIndex == cg_snap->ps.psClientNum) ? 1 : 0;
    }

    /* Pick the per-flame reference time. */
    if (ownerIsLocal != 0) {           /* CMP [ESP+0x10],0; JZ else */
        refTime = f->spawnTime;         /* FLD [EDI+0x48] (double) */
        chunkAlive = 1;                /* [ESP+0x2c]=1 */
    } else {
        refTime = (double)coduo_int32_from_bits(cg_flameTime); /* FILD cg_flameTime (signed) */
        chunkAlive = 0;                /* [ESP+0x2c]=0 */
    }

    /* ---- outer walk over the parent chain: for (; f; f = f->parent) ---- */
    for (;;) {
        /* 0x30027357: free a stale child chain hanging off f->parent. */
        next = f->parent;                          /* MOV EAX,[EDI+0x8] */
        if (next != (flameChunk_t *)0 && next->listMarker != 0) { /* [EAX+0x24] */
            CG_FreeFlameChunk(next);
            f->parent = (flameChunk_t *)0;         /* MOV [EDI+0x8],EBX(0) */
        }
        next = f->parent;                          /* MOV EBX,[EDI+0x8] */

        /* age = refTime - f->spawnTime (double). */
        age = refTime - f->spawnTime;

        /* toView = f->origin - cg_refdef.vieworg; distToView = |toView|. */
        {
            float vx = f->worldPos[0] - cg_refdef.vieworg[0];
            float vy = f->worldPos[1] - cg_refdef.vieworg[1];
            float vz = f->worldPos[2] - cg_refdef.vieworg[2];
            /* 0x300273ad..0x300273ce: the z,y,x square sum is handed to the CRT
             * sqrt helper RAW in st0 (no float store of the argument), so the
             * argument must not round to float: sqrtl on the 80-bit chain, one
             * float rounding at the result store ([ESP+0x1c]). */
            float distToView = (float)coduo_x87_sqrtl(
                (long double)vz * vz +
                (long double)vy * vy +
                (long double)vx * vx);

            /* 0x300273da: run the sound-envelope accumulation only when
             * (double)f->birthTime > age AND prev != NULL. FILD [EDI+0xa4]; FCOMP age;
             * TEST AH,0x41; JNZ skip => skip when field_a4 <= age (or unordered). */
            if ((double)f->birthTime > age && prev != (flameChunk_t *)0) {
                float *envA = &cg_flameSoundLoops[f->ownerInfoIndex].envA; /* base 0x300a8718 */

                /* Proceed only when envA < 1.0f (FLD envA; FCOMP 1.0f; TEST AH,0x5;
                 * JP skip => skip when envA >= 1.0f or unordered). */
                if (*envA < 1.0f) {
                    /* dot = f->axisRow · prev->axisRow (+0xac/+0xb0/+0xb4). */
                    float dot = (float)(
                        (long double)f->axisDir[2] * prev->axisDir[2] +
                        (long double)f->axisDir[1] * prev->axisDir[1] +
                        (long double)f->axisDir[0] * prev->axisDir[0]);
                    /* Proceed only when dot < 1.0f. */
                    if (dot < 1.0f) {
                        /* Proceed only when distToView < 1024.0f. */
                        if (distToView < 1024.0f) {
                            /* envA += (1 - distToView/1024) * (1 - dot) * 500;
                             * machine: FLD dist/1024; FSUBR 1.0; FLD 1.0; FSUB dot;
                             * FMULP; FMUL 500.0; FADD envA; FSTP envA. */
                            /* 0x3002746b..0x30027494 is ONE chain whose only
                             * rounding is the envA store, so no float temp. */
                            *envA = (float)(
                                ((long double)1.0f -
                                 (long double)distToView / 1024.0f) *
                                ((long double)1.0f - (long double)dot) *
                                (long double)500.0f + (long double)*envA);
                            /* Clamp to 1.0f: FCOMP 1.0f; TEST AH,0x41; JNZ skip =>
                             * skip (keep) when envA <= 1.0f (or unordered); set 1.0f
                             * only when envA > 1.0f. */
                            if (*envA > 1.0f)
                                *envA = 1.0f;
                        }
                    }
                }
            }
        }

        /* 0x300274c0: remember f as the previous chunk for the next iteration, and
         * bail out of the per-chunk body when the chunk is inactive (field_a0). */
        prev = f;                          /* MOV [ESP+0x18],EDI (restored as ESI) */
        if (f->deadFlag != 0)              /* MOV EAX,[EDI+0xa0]; TEST; JNZ 0x3002782d */
            goto step_chain;

        /* 0x300274d2: cull by age. cutoff = f->birthTime * 0.2f. FLD cutoff;
         * FCOMP age; TEST AH,0x5; JP 0x3002782d => skip when cutoff >= age (or
         * unordered); proceed only when cutoff < age. */
        {
            /* cutoff = (float)f->birthTime * 0.2f. The machine truncates age to a
             * float ([ESP+0x1c]) and compares cutoff(ST0) vs that float; TEST AH,0x5;
             * JP 0x3002782d => skip when cutoff >= (float)age (or unordered), i.e.
             * proceed only when cutoff < (float)age. The FILD'd birthTime IS
             * stored as a float (0x300274d8), but the *0.2f product stays in st0
             * UNSTORED for the compare, so cutoff is long double. */
            long double cutoff =
                (long double)(float)f->birthTime * (long double)0.2f;
            if (!(cutoff < (float)age))
                goto step_chain;
        }

        /* 0x300274fd: merge pass over the parent chain (needs a parent, and both
         * chunks must have field_2c == 0). The TEST EBX,EBX; JZ 0x3002761e at
         * 0x300274fd is EBX = f->parent, i.e. the merge loop's own entry test
         * (next != NULL) -- NOT a chunkAlive guard. chunkAlive is first read at
         * 0x30027632 (below). A prior pass wrapped this loop in
         * `if (chunkAlive != 0)`, which the binary does not do. */
        {
            next = f->parent;
            while (next != (flameChunk_t *)0) {
                float ratioA;  /* [ESP+0x20] */
                float weight;  /* [ESP+0x24] */

                if (f->kind != 0)          /* MOV EAX,[EDI+0x2c]; JNZ 0x3002761e */
                    break;
                if (next->kind != 0)       /* MOV EAX,[EBX+0x2c]; JNZ 0x3002761e */
                    break;

                /* ratioA = min(f->radius / 72.5f, 1.0f). FDIV 72.5; FCOMP 1.0f;
                 * TEST AH,0x5; JP => use 1.0f when quotient >= 1.0f. */
                {
                    float q = f->radius / 72.5f;
                    ratioA = (q < 1.0f) ? q : 1.0f;
                }

                /* weight = flameInfoActive ? 0.1f : 0.25f. */
                weight = (flameInfoActive != 0) ? 0.1f : 0.25f;

                /* sep = next->origin - f->origin; sepLen = |sep|. */
                {
                    float sx = next->worldPos[0] - f->worldPos[0];
                    float sy = next->worldPos[1] - f->worldPos[1];
                    float sz = next->worldPos[2] - f->worldPos[2];
                    /* Square sum passed RAW in st0 to the CRT sqrt helper
                     * (0x300275b2, no argument store), one float rounding at
                     * the result store. */
                    float sepLen = (float)coduo_x87_sqrtl(
                        (long double)sz * sz +
                        (long double)sy * sy +
                        (long double)sx * sx);

                    /* thr = pow((ratioA + 1) * 0.5, 1.5) * f->radius * weight;
                     * thr += thr (FADD ST0,ST0). Then FCOMPP thr,sepLen;
                     * TEST AH,0x41; JNZ 0x3002761e => stop merging when
                     * thr <= sepLen (or unordered); continue only when thr > sepLen.
                     * The raw CG_pow st0 result, both float multiplies, the
                     * doubling and the FCOMPP all stay in st registers with no
                     * store, so thr is long double; powl widens the base OPERAND
                     * and keeps the raw result 80-bit. */
                    long double thr = powl(((long double)ratioA + 1.0f) * 0.5f, 1.5L)
                                      * f->radius * weight;
                    thr = thr + thr;
                    if (!(thr > sepLen))
                        break;
                }

                /* Time gap: |f->spawnTime - next->spawnTime| vs 50.0. The double
                 * subtraction is stored as a FLOAT (FSTP float [ESP+0x1c] at
                 * 0x300275f5) before the FABS; FCOMP double 50.0; TEST AH,0x5;
                 * JP 0x3002761e => stop when >= 50.0 (or unordered); continue
                 * only when < 50.0. */
                {
                    float dt = (float)(
                        (long double)f->spawnTime -
                        (long double)next->spawnTime);
                    dt = fabsf(dt); /* FABS clears the sign bit, including NaNs. */
                    if (!(dt < 50.0))
                        break;
                }

                /* Fuse next into f, then re-read the (relinked) parent. */
                prev = next;                   /* MOV ESI,EBX */
                CG_MergeFlameChunks(f, next);  /* EDI=f, ESI=next */
                next = f->parent;              /* MOV EBX,[EDI+0x8] */
            }
        }

        /* 0x3002761e: decide whether to add this chunk to the scene. */
        if (ownerIsLocal != 0 && flameInfoActive != 0)   /* JZ 0x30027632 / JNZ 0x300277a9 */
            goto add_to_scene;
        if (chunkAlive == 0)                             /* JZ 0x300277a9 */
            goto add_to_scene;
        if (lastKept == (flameChunk_t *)0)               /* TEST EBP,EBP; JZ 0x300277a9 */
            goto add_to_scene;

        /* 0x30027646: radius-adjacency test against the last kept chunk. Proceed to
         * the angle test only when |lastKept->radius - f->radius| < f->radius/10.
         * FLD lastKept->e4; FSUB f->e4; FABS; FLD f->e4; FDIV 10.0f; FCOMPP;
         * TEST AH,0x41; JNZ 0x300277a9 => add when (f->e4/10) <= |diff| (or
         * unordered). The difference, FABS and quotient stay in st registers
         * (no store) up to the FCOMPP, so radDiff is long double. */
        {
            long double radDiff = fabsl(
                (long double)lastKept->radius - (long double)f->radius);
            if (!((long double)f->radius / (long double)10.0f > radDiff))
                goto add_to_scene;
        }

        /* 0x3002766d: angle/coverage test between (f - lastKept) and
         * (cg_refdef.vieworg - lastKept). */
        {
            float toF[3];    /* [ESP+0x68..0x70]: f->origin - lastKept->origin */
            float toView[3]; /* [ESP+0x74..0x7c]: cg_refdef.vieworg - lastKept->origin */
            float lenF;      /* [ESP+0x40]: length of toF, from the first normalize */
            float lenView;   /* [ESP+0x38]: length of toView, from the second */
            float sizeCap;   /* [ESP+0x30] */
            float sizeRatio; /* [ESP+0x34] */
            /* The whole cosang/score/term computation (0x30027741..0x300277a0)
             * runs in st registers with ZERO memory stores, from the dot
             * product through the FCOMPP against term — float/double locals
             * would each insert a rounding the DLL does not perform, so all
             * three are long double. */
            long double cosang;
            long double score, term;

            toView[0] = cg_refdef.vieworg[0] - lastKept->worldPos[0];
            toView[1] = cg_refdef.vieworg[1] - lastKept->worldPos[1];
            toView[2] = cg_refdef.vieworg[2] - lastKept->worldPos[2];
            toF[0] = f->worldPos[0] - lastKept->worldPos[0];
            toF[1] = f->worldPos[1] - lastKept->worldPos[1];
            toF[2] = f->worldPos[2] - lastKept->worldPos[2];

            /* VectorNormalize both in place (ESI = vector base), capturing each
             * length. First call (0x300276d1) normalizes toF (ESI=[ESP+0x68]),
             * length spilled to [ESP+0x40]; second call (0x300276de) normalizes
             * toView (ESI=[ESP+0x74]), length to [ESP+0x38]. */
            lenF = VectorNormalize(toF);
            lenView = VectorNormalize(toView);

            /* sizeCap = min(f->radius, 30.0f). 0x300276e7: FLD f->radius;
             * FCOMP 30.0f (0x3007becc); TEST AH,0x5; JP 0x30027706 => the
             * PARITY branch stores the constant 30.0f (MOV [ESP+0x30],
             * 0x41f00000) and is taken when radius >= 30.0f (or unordered);
             * only the fall-through (radius < 30.0f) copies f->radius. Same
             * branch shape as the sizeRatio clamp below, which likewise takes
             * the constant on JP. */
            sizeCap = (f->radius < 30.0f) ? f->radius : 30.0f;

            /* sizeRatio = min(f->radius / 30.0f, 1.0f). */
            {
                float q = f->radius / 30.0f;
                sizeRatio = (q < 1.0f) ? q : 1.0f;
            }

            /* cosang = toF · toView (both normalized). */
            cosang =
                (long double)toF[2] * toView[2] +
                (long double)toF[1] * toView[1] +
                (long double)toF[0] * toView[0];

            /* score = ((1.0 - |cosang|) * 0.9 + 0.1) * (lenView / sizeCap * lenF).
             * Machine (0x3002775d..): FABS; FSUBR 1.0(0x3007bcf8); FMUL 0.9(0x3007c268);
             * FADD 0.1(0x3007c260); FLD lenView([ESP+0x38]); FDIV sizeCap([ESP+0x30]);
             * FMUL lenF([ESP+0x40]); FMULP — the FMULP pairs the 0.9x+0.1 term
             * with the ALREADY-FORMED (lenView/sizeCap)*lenF product, so that
             * grouping is kept. */
            score = 1.0 - fabsl(cosang);                                 /* FABS; FSUBR 1.0 */
            score = score * 0.8999999761581421 + 0.10000000149011612;   /* 0x3007c268, c260 */
            score = score *
                ((long double)lenView / (long double)sizeCap *
                 (long double)lenF);

            /* term = pow(1.0 - f->lifeFraction, 3.0) * sizeRatio * 32.0.
             * FLD f->lifeFraction; FSUBR 1.0(0x3007bcf8); FLD 3.0(0x3007bdf8); CALL pow;
             * FMUL sizeRatio([ESP+0x34]); FMUL 32.0(0x3007c258); raw st0 result
             * consumed straight by the FCOMPP -- powl keeps the base (1.0 - e8,
             * built raw via FSUBR double 1.0) and the result 80-bit. */
            term = powl(1.0 - (long double)f->lifeFraction, 3.0L);
            term = term * sizeRatio * 32.0;

            /* FCOMPP compares term(ST0) vs score(ST1); TEST AH,0x41; JZ 0x30027823
             * => skip the add (finalize only) when term > score; otherwise add. */
            if (term > score)
                goto finalize;
            goto add_to_scene;
        }

    add_to_scene:
        {
            /* 0x300277a9: life fraction from f->lifeFraction through the 0.5 knee.
             * FLD e8; FCOMP 0.5f; FLD e8; TEST AH,0x5; JP else => when e8 >= 0.5f
             * take the else; proceed (e8 < 0.5f) with e8/0.5f. */
            float e8 = f->lifeFraction;
            float lifeFrac;

            if (e8 < 0.5f) {
                lifeFrac = e8 / 0.5f;              /* FDIV 0.5f */
            } else {
                /* FCOMP 0.5f (reloaded e8); TEST AH,0x41; JNZ 0x300277f9 => when
                 * e8 <= 0.5f (== 0.5f, or unordered) use 1.0f; else falloff. */
                if (e8 > 0.5f)
                    lifeFrac = (float)(
                        (long double)1.0f -
                        ((long double)e8 - (long double)0.5f) /
                            (long double)0.5f);              /* FSUB; FDIV; FSUBR */
                else
                    lifeFrac = 1.0f;                        /* 0x3f800000 */
            }

            /* alpha = lifeFrac * 0.5f; add the sprite. */
            {
                float alpha = lifeFrac * 0.5f;    /* FMUL 0.5f */
                CG_AddFlameToScene(f, f->lifeFraction, alpha, 1);
            }
            lastKept = f;                          /* MOV EBP,EDI */
        }
        /* falls into finalize */

    finalize:
        /* 0x30027823: prev = f; f->sizeRate = CG_FlameGetSizeRate(f). */
        prev = f;                                  /* MOV ESI,EDI */
        /* f->sizeRate is a float; the x87 result is stored straight in. */
        f->sizeRate = CG_FlameGetSizeRate(f); /* CALL 0x30023b70; FSTP [EDI+0x64] */

    step_chain:
        /* 0x3002782d: advance to f->parent. EBX holds f->parent (last [EDI+0x8]
         * read); MOV EDI,EBX; if EBX != 0 loop with prev restored from [ESP+0x18]
         * (== the f we just processed). */
        if (next == (flameChunk_t *)0)
            break;
        prev = f;              /* MOV ESI,[ESP+0x18] at 0x30027351 restores saved f */
        f = next;              /* MOV EDI,EBX */
    }
}
