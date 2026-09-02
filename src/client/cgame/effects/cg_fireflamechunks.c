// Source: uo_cgame_mp_x86.dll 0x30027d10..0x300291b4
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30027d10_300291b4.mcode
//
// CG_FireFlameChunks — the per-frame flame-chunk simulation tick (the SECOND
// flame giant, sibling of CG_EmitPlayerFlameChunks). Reconstructed via the proven
// REGION-SPLIT strategy. Phase 1 established the shared stack frame + entry ABI and
// fully reconstructed REGION A (setup) and REGION C (teardown). Phase 2 filled the
// three loop bodies (OUTER, INNER, INNERMOST) and their induction. PHASE 3 (this
// pass) reconstructs the remaining child-jitter population block (0x30028011 ..
// 0x30028eee) inside the inner loop: the per-kind rand jitter/clamp seed, the
// field_2c==3 fire long-life path (with its damage-refinement trace), the smoke
// path, the cg_flameInfo emit-basis copy + smoke curves, and the nested smoke
// sub-spawn. The whole function body is now proven against the machine code.
//
// ============================================================================
// STACK FRAME (SUB ESP,0x354 at entry, then PUSH EBX/EBP/ESI/EDI)
// ============================================================================
//   30027d10  SUB ESP,0x354        ; 0x354 (852) bytes of locals
//   30027d1c  PUSH EBX             ; callee-saved
//   30027d1d  PUSH EBP             ; callee-saved
//   30027d1e  PUSH ESI             ; callee-saved
//   30027d23  PUSH EDI             ; callee-saved
//   ...epilogue at 0x300291a9: POP EDI/ESI/EBP/EBX; ADD ESP,0x354; RET
//
// Entry register ABI: NONE — void(void), __cdecl. Reads all inputs from globals
// (cg_activeFlameChunks, cg_flameTime, cg_snap, the refdef view axes, cg.time).
//
// Register roles in the body:
//   EBP = 0 (XOR EBP,EBP at 0x30027d47) used as the constant null/zero comparand
//         AND, inside the inner loop, as the freshly-spawned child chunk pointer.
//   EDI = 1 (MOV EDI,1 at 0x30027e00 / re-set at 0x300290a9): the "spawned"/live
//         flag written to chunk->listMarker (+0x24) and compared against field_2c.
//   EBX = current chunk (mirrors [ESP+0x24]).
//   ESI = cur->next (mirrors [ESP+0x50]).
//
// ============================================================================
// .rdata FLOAT/DOUBLE POOL (values proven by objdump -s of .rdata)
// ============================================================================
//   Singles:  bce0=1.0f bce4=2.0f bce8=0.5f bcec=0.0f  bd10=32768.0f
//     bd50=180.0f bd54=360.0f  bda4=10.0f  bde0=5.0f  be04=20.0f  be10=0.2f
//     be38=0.75f be40=4.0f be58=0.25f be5c=3.0f be70=1.5f be98=25.0f bebc=290.0f
//     bec8=140.0f bef8=3.5f bf00=16.0f bf6c=0.1f bfc4=-0.3f bff0=60.0f c018=0.15f
//     c150=-2.0f c158=120.0f c174=0.45f c348=0.19f c350=1.85f c368=105.0f
//     c380=-2.25f c390=-0.35f c3a0=14.5f c1b0=0.55f
//   Doubles:  bcf0=0.0 bcf8=1.0 bd28=0.5 bde8=2.0 bdf8=3.0 c0a8=-1.0
//     c118=0.75 c278=1.5 c340=2600.0 c358=0.8(exact) c360=4.0
//     c370=(double)0.2f(0x3FC99999A0000000) c378=5.0
//     c388=800.0 c398=1600.0 c3a8=600.0 c3b0=2400.0 c3b8=400.0 c3c0=0.2(exact)
//     c3d0=1300.0 c3d8=(double)0.3f(0x3FD3333340000000)
//     c3e0=(double)0.8f(0x3FE99999A0000000)
//     c34c=0.9000004556728527 c348=-16777219.89125(dbl)
//   Singles used by the reconstructed code (re-verified from objdump -s):
//     c0d8=900.0f (field_94 divisor).
//   PHASE 3 UPDATE: the child-jitter block (0x30028011..0x30028eee) is now
//   reconstructed and EVERY constant it uses was re-dumped from `objdump -s -j
//   .rdata` and verified individually. Corrections vs. the phase-2 indicative
//   catalog above: c34c is the SINGLE -15.0f (not 0.9 double); c348 is the SINGLE
//   0.19f (a separate c348 DOUBLE -16777219.89 exists but is unused here);
//   c1fc=0.65f c0e4=0.35f c2b8=0.02f (singles). Doubles confirmed: c358=0.8
//   c360=4.0 c370=0.2 c378=5.0 c388=800.0 c398=1600.0 c3a8=600.0 c3b0=2400.0
//   c3b8=400.0 c3c0=0.2 c3c8=1300.0(single!) c3d0=1300.0(double) c3d8=0.3 c3e0=0.8
//   c340=2600.0. Every constant used by the RECONSTRUCTED body below was verified.
//   Immediates in-line: 3.0f(0x40400000) 40.0f(0x42200000) 1.0f(0x3f800000)
//     0.5f(0x3f000000) 60.0f(0x42700000) 120.0f(0x42f00000) 290.0f(0x43910000)
//     -15.0f(0xc1700000) -2.0f(0xc0000000) 2.0f(0x40000000) 0.15f(0x3e19999a).
//
// The single-float scale 32768.0f (bd10) is the rand()/32768 divisor: rand()
// returns [0,32767], so rand()/32768.0f is a uniform [0,1) fraction; the idiom
// `2*(rand/32768) - 1` maps it to [-1,1).  The m32 constant at c0d8 is 900.0f.

#include <math.h>       /* fabs: direct C form of the inline x87 FABS (d9 e1) in the jitter block */
#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"
#include "compat/coduo_native_x87.h"

/* Cvar flags used at the trap(7,...) call sites: PUSH 0x21 == CVAR_ARCHIVE|0x20. */
enum {
    CG_FIRE_CVAR_FLAGS = 0x21   /* CVAR_ARCHIVE | 0x20 (ROM?) */
};

/* Flame-chunk kind (flameChunk_t.kind) discriminants proven by this function.
 * kind==2 marks a spawned child (set at 0x30027fcb); ==3 selects the fire
 * long-life spawn path (0x30028194); ==5 marks a smoke chunk (set at 0x30028d36).
 * Exact source enum names unresolved; named provisionally by role. */
enum {
    FLAME_CHUNK_KIND_CHILD = 2,   /* spawned child chunk */
    FLAME_CHUNK_KIND_FIRE  = 3,   /* long-life fire chunk */
    FLAME_CHUNK_KIND_SMOKE = 5    /* smoke chunk */
};

/* rand()/32768.0f -> uniform [0,1). rand (0x3005b879) is the MSVC CRT PRNG. */
#define FLAME_RAND_DIVISOR 32768.0f

void CG_FireFlameChunks(void)
{
    flameChunk_t *cur;   /* [ESP+0x24] / EBX — OUTER loop iterator */
    flameChunk_t *next;  /* [ESP+0x50] / ESI — cur->next latched at loop top */

    /* ---------------------------------------------------------------------
     * REGION A — SETUP (0x30027d10 .. 0x30027e05)
     * ------------------------------------------------------------------- */

    /* 0x30027d16..0x30027d3d: cg_flameTime = ftol(floor(2*cg.time)) — the ftol
     * at 0x30027d38 consumes floor's ST0 return directly: there is no store
     * between CALL floor (0x30027d30) and CALL _ftol2 (0x30027d38), so the raw
     * unrounded ST0 is what gets converted (coduo_fp_to_i32_extended takes
     * long double for exactly this). */
    {
        /* FILD m32 is signed: preserve the target's Win32 clock value before
         * widening it to the double argument passed to floor. */
        double timeD = (double)coduo_int32_from_bits(cg_time);
        cg_flameTime = (uint32_t)coduo_fp_to_i32_extended(floor(timeD + timeD));
    }

    /* 0x30027d42..0x30027da1: register (first call) or refresh the r_fullscreen /
     * r_overbrightbits cvar mirrors. */
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (cg_fireFlameChunksCvarsRegistered == 0) {
        cgame_syscall(CG_CVAR_REGISTER, (intptr_t)&cg_rFullscreenCvar,
                      (intptr_t)r_fullscreenCvarName,
                      (intptr_t)cvarEnabledValue,
                      CG_FIRE_CVAR_FLAGS);
        cgame_syscall(CG_CVAR_REGISTER, (intptr_t)&cg_rOverbrightBitsCvar,
                      (intptr_t)r_overbrightBitsCvarName,
                      (intptr_t)cvarEnabledValue,
                      CG_FIRE_CVAR_FLAGS);
    } else {
        cgame_syscall(CG_CVAR_UPDATE, (intptr_t)&cg_rFullscreenCvar);
        cgame_syscall(CG_CVAR_UPDATE, (intptr_t)&cg_rOverbrightBitsCvar);
    }

    /* 0x30027da4..0x30027dea: cache the world-flame billboard basis rows. */
    cg_flameSpriteSrcRight[0] = cg_refdef.viewaxis[1][0];
    cg_flameSpriteSrcRight[1] = cg_refdef.viewaxis[1][1];
    cg_flameSpriteSrcRight[2] = cg_refdef.viewaxis[1][2];
    cg_flameSpriteSrcUp[0] = cg_refdef.viewaxis[2][0];
    cg_flameSpriteSrcUp[1] = cg_refdef.viewaxis[2][1];
    cg_flameSpriteSrcUp[2] = cg_refdef.viewaxis[2][2];

    /* 0x30027db5 / 0x30027df0: cg_flameDamageBillboardCount reset to 0. */
    cg_flameDamageBillboardCount = 0;

    /* 0x30027db5,0x30027df6: cur = cg_activeFlameChunks. */
    cur = cg_activeFlameChunks;

    /* 0x30027dfa: if the active list is empty, skip straight to teardown. */
    if (cur != (flameChunk_t *)0) {
        /* -----------------------------------------------------------------
         * OUTER LOOP  (head 0x30027e05, back-edge 0x300290c9)
         * Walks cg_activeFlameChunks via ->next until NULL. `next` is latched
         * at the loop top so a node freed mid-body does not corrupt the walk.
         * ----------------------------------------------------------------- */
        while (cur != (flameChunk_t *)0) {
            /* 0x30027e05: skip nodes already processed this frame. */
            next = cur->next;                       /* 0x30027e08: latch cur->next */
            if (cur->listMarker != 0) {             /* 0x30027e05 CMP; JNE advance */
                goto outer_advance;
            }

            {
                /* 0x30027e14: dtmp = (double)cg_flameTime. */
                double flameTimeD = (double)coduo_int32_from_bits(cg_flameTime);

                /* 0x30027e1e..0x30027e2a: if (flameTimeD > field_50) the chunk has
                 * expired past its end time -> free/expire tail at 0x300290b0. */
                if (flameTimeD > cur->endTime) {
                    goto expire_chunk;              /* 0x300290b0 */
                }

                /* 0x30027e30..0x30027e3c: FLD flameTimeD; FCOMP field_48; TEST AH,5;
                 * JNP 0x300290b0 -> jump to the expire tail when flameTimeD < field_48
                 * (the chunk has not yet reached its spawn/start time). */
                if (flameTimeD < cur->spawnTime) {
                    goto expire_chunk;              /* 0x300290b0 */
                }

                /* 0x30027e42..0x30027e69: the field_a0 "dead" latch. When field_a0 is
                 * nonzero AND the elapsed-since-spawn (flameTimeD - field_48) has passed
                 * the birth stamp field_a4, mark the node processed and free it. */
                if (cur->deadFlag != 0) {           /* 0x30027e42 CMP [+0xa0],0 */
                    /* 0x30027e4a: fieldA4D = (double)field_a4 (FILD int32). */
                    double fieldA4D = (double)cur->birthTime;
                    /* 0x30027e54..0x30027e64: if ((flameTimeD - field_48) > fieldA4D)
                     * the chunk is past its display window -> mark + skip. TEST AH,0x41;
                     * JNE keeps going only when (delta) <= fieldA4D. */
                    if ((flameTimeD - cur->spawnTime) > fieldA4D) {
                        cur->listMarker = 1;        /* 0x30027e66 MOV [+0x24],EDI(=1) */
                        goto outer_advance;         /* 0x30027e69 */
                    }
                }

                /* 0x30027e6e..0x30027eb1: the field_140 child-spawn gate. When
                 * field_140 > 0.0 (0x30027e6e FCOMP 0.0; TEST AH,0x41; JNE skips when
                 * field_140 <= 0.0) and field_140 < flameTimeD (0x30027e87 FCOMP
                 * flameTimeD; TEST AH,5; JP skips when field_140 >= flameTimeD), spawn a
                 * burst of child chunks and reset field_140 to 0.0. */
                if (cur->lifeStartTime2 > 0.0) {
                    if (cur->lifeStartTime2 < flameTimeD) {
                        /* 0x30027e92..0x30027eae: spawn children then clear the gate. */
                        CG_FlameDropDrip(cur, 3.0f, 40.0f);
                        cur->lifeStartTime2 = 0.0;       /* 0x30027ea2 FLD 0.0; FSTP [+0x140] */
                    }
                }

                /* 0x30027eb1..0x30027f1c: for a NON-child chunk (field_2c == 0),
                 * recompute field_148 from a pow() curve of field_94 and field_e8. */
                if (cur->kind == 0) {           /* 0x30027eb1 CMP [+0x2c],0; JNE 0x30027f22 */
                    /* 0x30027eb6..0x30027ece: curve = pow(1.0f - field_94/900.0f, 0.8).
                     * FLD field_94; FDIV 900.0f(0x3007c0d8, single); FSUBR 1.0f(0x3007bce0)
                     * => (1.0f - field_94/900.0f) held on ST0; FLD 0.8(0x3007c3e0, double)
                     * => exponent on ST0, base on ST1; CALL CG_pow(base=ST1, exp=ST0). */
                    /* The base is NOT stored before the call (0x30027eb6..0x30027ec2 feed
                     * CALL directly), so it stays 80-bit; pow's ST0 return IS rounded, to
                     * FLOAT (FSTP/FLD m32 @0x30027ed3/ed7) — a double `curve` would round
                     * twice. */
                    long double curveBase = (long double)1.0f
                        - (long double)cur->driftSpeed / (long double)900.0f;
                    /* base (curveBase) kept 80-bit into _CIpow (no store @0x30027eb6..ec8);
                     * powl keeps base+result 80-bit as the bytes require. Exponent is the
                     * DOUBLE (double)0.8f (0x3007c3e0=0x3FE99999A0000000); (long double)0.8f
                     * preserves exactly that value (0.8f widens exact). Result IS rounded to
                     * float (FSTP m32 @0x30027ed3), so the single (float) narrowing stays. */
                    float curve = (float)powl(curveBase, (long double)0.8f);
                    /* 0x30027ed3..0x30027ee9: t = curve + (1.0f - field_e8), single. */
                    float t = curve + (1.0f - cur->lifeFraction);
                    /* 0x30027eed..0x30027f1c: field_148 = clamp01(t) with the two-sided
                     * branch: if (t < 0.0f) 0; else if (t > 1.0f) 1.0f; else t. */
                    float clamped;
                    if (t < 0.0f) {                 /* 0x30027ef1 FCOMP 0.0; TEST AH,5; JP -> t>=0 */
                        clamped = 0.0f;             /* 0x30027efe XOR EAX,EAX -> +0.0f bits */
                    } else if (t > 1.0f) {          /* 0x30027f06 FCOMP 1.0; TEST AH,0x41; JE -> t>1 */
                        clamped = 1.0f;             /* 0x30027f11 MOV EAX,0x3f800000 */
                    } else {
                        clamped = t;                /* 0x30027f18 keep t */
                    }
                    cur->spawnScale = clamped;       /* 0x30027f1c MOV [+0x148],EAX */
                }

                /* 0x30027f22..0x30027f33: the INNER-loop entry gate #1.
                 * FLD field_130; FCOMP 0.0(double); TEST AH,0x41; JNE 0x30029069.
                 * TEST AH,0x41;JNE => jump (to the final tail) when field_130 <= 0.0.
                 * So the inner spawn loop runs only when field_130 > 0.0. */
                if (cur->lifeStartTime > 0.0) {
                    /* -------------------------------------------------------------
                     * INNER LOOP  (head 0x30027f40, back-edge 0x30029063)
                     * do-while child-spawn loop; each iteration spawns a child,
                     * advances field_130 by a per-step increment, and re-enters
                     * while field_130 remains positive. The head also requires
                     * field_130 to remain below the current flame time.
                     * ----------------------------------------------------------- */
                    for (;;) {
                        /* stepAccum lives in [ESP+0x28] across the whole iteration; child
                         * is EBP. Declared at iteration scope to match. */
                        double stepAccum;
                        flameChunk_t *child;
                        /* ESI: reloaded with the parent's field_2c at 0x30028f27 on the
                         * child path; the gate that reads it (0x30028fd8) is only ever
                         * reached on that path (child==NULL jumps past it, see below). */
                        int32_t stepKind = 0;

                        /* 0x30027f40..0x30027f53: gate #2 — FLD 0.0; FLD
                         * field_138; FUCOMPP; TEST AH,0x44; JNP.  The jump is
                         * taken only for ordered equality.  Unordered input does
                         * not exit here. */
                        if (cur->lifeRate == 0.0) {
                            goto inner_final_tail;      /* 0x30029069 */
                        }

                        /* 0x30027f59..0x30027f72: gate #3 — recompute flameTimeD and
                         * enter the iteration only for an ordered field_130 <
                         * flameTimeD comparison.  TEST AH,5; JP exits for greater,
                         * equal, and unordered. */
                        {
                            double ftD = (double)coduo_int32_from_bits(cg_flameTime); /* 0x30027f59 refresh */
                            if (!(cur->lifeStartTime < ftD)) {
                                goto inner_final_tail;  /* 0x30029069 */
                            }
                        }

                        /* 0x30027f78..0x30027f86: step0 accumulator seed:
                         * stepAccum = (field_50 - field_48) * field_138. */
                        stepAccum = (cur->endTime - cur->spawnTime) * cur->lifeRate;

                        /* 0x30027f7b XOR ESI,ESI; 0x30027f8a CALL 0x30025600:
                         * CG_SpawnFlameChunk takes `parent` in ESI, so this spawn
                         * passes NULL — a root-chunk allocation, NOT a splice into
                         * the parent's list position. */
                        child = CG_SpawnFlameChunk((flameChunk_t *)0); /* 0x30027f8a, ESI=0 */
                        if (child != (flameChunk_t *)0) {

                            /* 0x30027f99..0x30027fb8: child->lifeFraction =
                             * (field_130 - field_48) / (field_50 - field_48). Computed
                             * on the PARENT's fields into the parent's field_e8. */
                            cur->lifeFraction = (float)((cur->lifeStartTime - cur->spawnTime)
                                                    / (cur->endTime - cur->spawnTime));

                            /* 0x30027fa7 MOV ESI,EBP; 0x30027fbe: REP MOVSD 0x54 dwords
                             * CHILD(EBP)->staging [ESP+0xc0] — the 0x150-byte snapshot
                             * captures the child's PRE-overwrite state (its own pool
                             * link fields), not the parent.
                             * 0x30027fc5 MOV ESI,EBX; 0x30027fc9: REP MOVSD 0x54 dwords
                             * parent(EBX)->child(EBP). The child is a full copy of the
                             * parent chunk. */
                            flameChunk_t staging = *child; /* [ESP+0xc0] @0x30027fbe */
                            *child = *cur;              /* 0x30027fc9 REP MOVSD parent->child */

                            /* 0x30027fcb: child->kind = 2 (spawned-child kind). */
                            child->kind = FLAME_CHUNK_KIND_CHILD;

                            /* 0x30027fd2..0x30027fe3: child->lifeFraction =
                             * (field_130 - child->spawnTime) / (child->endTime -
                             * child->spawnTime). */
                            child->lifeFraction = (float)((cur->lifeStartTime - child->spawnTime)
                                                      / (child->endTime - child->spawnTime));

                            /* 0x30027fe9..0x30028011: first size-advance + finalize pass
                             * on the child at the parent's field_130 time. Two separate
                             * callees (NOT the pool merge helper 0x300257e0):
                             *   PUSH Q_rint(field_130); MOV EAX,child; CALL 0x30025c60
                             *     -> CG_AdvanceFlameChunkSize(child, flameTime).
                             *   PUSH Q_rint(field_130); PUSH child; CALL 0x30025da0
                             *     -> CG_MoveFlameChunk(child, flameTime). */
                            {
                                int32_t rt = coduo_fp_to_i32_extended(
                                    (long double)cur->lifeStartTime); /* 0x30027fe9: FLD m64; no m32 spill */
                                CG_AdvanceFlameChunkSize(child, rt);        /* 0x30027ff7, EAX=child */
                                rt = coduo_fp_to_i32_extended(
                                    (long double)cur->lifeStartTime);       /* 0x30027ffc: second FLD m64 */
                                CG_MoveFlameChunk(child, rt);       /* 0x3002800c */
                            }

                            /* ===============================================================
                             * CHILD-JITTER POPULATION BLOCK — 0x30028011 .. 0x30028eee.
                             * Reconstructed (phase 3, x87-dense). At entry EBX=parent (`cur`),
                             * EBP=child; [ESP+0x24] mirrors the parent; [ESP+0xc0] holds the
                             * 0x150-byte parent snapshot copied at 0x30027fbe (modeled here as
                             * `staging`). All .rdata constants re-verified via objdump -s -j
                             * .rdata; each carries its pool address. All FP arithmetic is x87
                             * (double/extended); single/double load widths preserved per the
                             * FLD/FADD (m32/DWORD vs m64/QWORD) encodings.
                             * =============================================================== */
                            {
                                double sizeDelta;              /* [ESP+0x18] per-kind size seed */

                                /* 0x30028011: for a real emitter (field_2c==0) run the rand
                                 * jitter/clamp; otherwise the seed is 0.0 (FLD 0.0 @0x3002801b). */
                                if (cur->kind == 0) {
                                    /* jitter() sample (all x87 double):
                                     *   rf = (float)rand() round-tripped through m32;   0x30028026..37
                                     *   a  = (2*(rf/32768.0f) - 1) * 0.15f;             (bd10 / c018)
                                     *   p  = pow(1.0f - field_e8, 3.0);                 (bdf8)
                                     *   v  = (a + p*1.5 - (field_94/900.0f)*0.0f) * 0.3;(c278/c0d8/bcec/c3d8)
                                     * APPARENT SOURCE ODDITY: the (field_94/900)*0.0f term is a
                                     * no-op (coefficient tuned to 0.0 in the original source) but
                                     * the machine computes it; preserved. */
                                    /* a_ and p_ are NEVER stored: the *0.15f at 0x30028049 feeds
                                     * FLD [bce0] at 0x3002804f directly, and pow's ST0 return at
                                     * 0x30028061 feeds FMUL double [c278] at 0x30028066 directly.
                                     * long double keeps both unrounded, as the DLL does. */
                                    #define FLAME_JITTER_SAMPLE(dst)                                    \
                                        do {                                                            \
                                            float rf_ = (float)coduo_crt_rand();                                  \
                                            long double a_ = (2.0f * (rf_ / FLAME_RAND_DIVISOR)         \
                                                              - 1.0f) * 0.15f;                          \
                                            long double p_ = powl(1.0L - cur->lifeFraction, 3.0L); \
                                            (dst) = (a_ + p_ * 1.5L                                      \
                                                     - ((long double)cur->driftSpeed / 900.0f) * 0.0f)   \
                                                    * (long double)(double)0.3f; /* c3d8 */               \
                                        } while (0)
                                    /* s1/s2 are never stored: each jitter chain feeds its
                                     * FCOMP directly (0x30028088 / 0x30028102) at register
                                     * precision. Only the third chain (or the selected
                                     * constant) is rounded, by the FSTP double at 0x30028179
                                     * into sizeDelta. */
                                    long double s1;
                                    FLAME_JITTER_SAMPLE(s1);                 /* chain 1 @0x30028026 */
                                    if (s1 < 0.0) {                          /* 0x30028088 FCOMP 0.0(bcf0) */
                                        sizeDelta = 0.0;                     /* 0x30028095 */
                                    } else {
                                        long double s2;
                                        FLAME_JITTER_SAMPLE(s2);             /* chain 2 @0x300280a0 */
                                        if (s2 > (double)0.8f) {             /* 0x30028102 FCOMP c3e0=(double)0.8f */
                                            sizeDelta = (double)0.8f;        /* 0x3002810f */
                                        } else {
                                            FLAME_JITTER_SAMPLE(sizeDelta);  /* chain 3 @0x30028117 */
                                        }
                                    }
                                    #undef FLAME_JITTER_SAMPLE
                                    /* 0x3002817d..0x30028186: seed *= (field_50 - field_130). */
                                    sizeDelta = (cur->endTime - cur->lifeStartTime) * sizeDelta;
                                } else {
                                    sizeDelta = 0.0;
                                }
                                /* 0x3002818d: [ESP+0x18] = sizeDelta. */

                                /* 0x30028191: CMP field_2c,3 — the fire vs smoke split. */
                                if (cur->kind == FLAME_CHUNK_KIND_FIRE) {
                                    /* --- FIRE PATH (0x3002819a .. 0x30028845) --- */
                                    child->boneHandle = 0;                   /* 0x3002819a */
                                    child->ownerInfoIndex = ENTITYNUM_WORLD;       /* 0x300281a1 = 1022 */
                                    child->ownerSentinel = 0xffffffffu;           /* 0x300281a8 -1 */

                                    /* 0x300281af: FCOMP field_94 vs 1.0f; TEST AH,0x5; JP
                                     * 0x30028321 skips when field_94 >= 1.0f (or NaN), so the
                                     * whole trace+turbulence block runs only when
                                     * field_94 < 1.0f. */
                                    if (cur->driftSpeed < 1.0f) {              /* 0x300281af FCOMP 1.0f(bce0) */
                                        /* 0x300281c6..0x300282b2: damage-refinement trace, additionally
                                         * gated on parent free (boneHandle==0, else -> 0x300282b5) and
                                         * projected end (cg_flameTime + 1300.0(c3d0)) < field_50, i.e.
                                         * the chunk still has >1300ms to live (0x300281f0 TEST AH,0x5;
                                         * JP skips when >=). Both those skips fall to the turbulence
                                         * below. */
                                        if (cur->boneHandle == 0             /* 0x300281ca TEST [+0x40] */
                                            && ((double)coduo_int32_from_bits(cg_flameTime) + 1300.0)
                                                   < cur->endTime) {        /* 0x300281eb FCOMP field_50 */
                                            vec3_t computeOut;                /* [E0+0x54] */
                                            vec3_t hit;                       /* [E0+0x78] */
                                            trace_t traceOut;       /* [E0+0x90] */
                                            int32_t rt;
                                            /* 0x300281fb..0x30028219: rt = Q_rint((float)field_130 + 1300.0f(c3c8)). */
                                            rt = coduo_fp_to_i32_extended((float)cur->lifeStartTime + 1300.0f);
                                            /* 0x3002821a: CG_ComputeFlameChunkOrigin(parent, rt, computeOut). */
                                            CG_ComputeFlameChunkOrigin(cur, rt, computeOut);
                                            /* 0x3002821f..0x30028285: hit[i] = computeOut[i] - offset[i]*4.0f(be40). */
                                            hit[0] = computeOut[0] - cur->centerOffset[0] * 4.0f;
                                            hit[1] = computeOut[1] - cur->centerOffset[1] * 4.0f;
                                            hit[2] = computeOut[2] - cur->centerOffset[2] * 4.0f;
                                            /* 0x3002828c: CG_FlamethrowerTrace (custom ABI, args proven
                                             * from the push/LEA/esp trace). */
                                            CG_FlamethrowerTrace(0x2810011,
                                                cg_flameTraceMaxs,
                                                cg_flameTraceMins,
                                                &traceOut, computeOut, hit,
                                                cur->ownerInfoIndex);
                                            /* 0x30028291..0x300282b2: keep the projected end only when
                                             * the trace neither hit (+0x2f byte) nor was blocked
                                             * (fraction==1.0f) -> clamp field_50 up to cg_flameTime. */
                                            if (traceOut.startsolid == 0 && traceOut.fraction == 1.0f) {
                                                cur->endTime = (double)coduo_int32_from_bits(cg_flameTime); /* 0x300282ac */
                                            }
                                        }

                                        /* 0x300282b5..0x30028321: turbulence pre-roll of field_a8 when
                                         * field_d0 != 0.0 AND |field_120| < 0.2(c3c0) (0x300282de TEST
                                         * AH,0x5; JP skips when >=) AND |field_a8| >= 0.0 (0x300282f3
                                         * TEST AH,0x1; JNZ skips only when < 0.0 or unordered).
                                         * Since FABS cannot produce a negative ordered value, the
                                         * last comparison is specifically a NaN rejection gate; it
                                         * is not a zero test. */
                                        if (cur->endTimeCopy != 0.0                         /* 0x300282b5 FUCOMPP 0.0 */
                                            && fabs((double)cur->centerOffset[2]) < 0.2        /* 0x300282ce fabs; c3c0 */
                                            && fabs((double)cur->expansionRate) >= 0.0) {     /* 0x300282e3 fabs; bcf0 */
                                            float rf = (float)coduo_crt_rand();                     /* 0x300282f8 */
                                            cur->expansionRate = (float)((double)(-0.3f)       /* bfc4 */
                                                - 2.0 * ((double)rf / FLAME_RAND_DIVISOR));
                                        }
                                    }

                                    /* 0x30028321..0x300285df: field_130-driven size/life build. EDI=parent. */
                                    child->spawnTime = cur->lifeStartTime;                     /* 0x30028325 */
                                    {
                                        double base = cur->lifeStartTime + 400.0;             /* 0x3002832e c3b8 */
                                        child->endTime = base;                           /* 0x30028342 */
                                        if (cur->boneHandle == 0) {                       /* 0x30028345 */
                                            /* 0x30028350..0x30028393: free-chunk timing jitter.
                                             * base+2400.0 is STORED to endTime (rounding 1,
                                             * FSTP @0x3002835a) BEFORE the rand() call; the
                                             * stored value is then reloaded and j subtracted
                                             * (rounding 2, FSTP @0x30028389). Writing
                                             * (base+2400.0)-j as one expression would skip
                                             * the intermediate rounding. */
                                            child->endTime = base + 2400.0;              /* c3b0 */
                                            double j = ((double)(float)coduo_crt_rand() / FLAME_RAND_DIVISOR)
                                                       * 600.0;                           /* c3a8 */
                                            child->endTime = child->endTime - j;
                                            child->spawnTime = child->spawnTime - j;
                                            /* 0x30028396..0x30028437: field_5c = clamp(field_e4*1.5f
                                             * to [60,120]) + (rand/32768)*25.0f, clamped <=290;
                                             * field_e4 = field_5c / 2.0f. The pre-clamp value is
                                             * stored to field_5c at 0x3002841b, then overwritten
                                             * with 290.0f at 0x30028425 if it exceeds 290. */
                                            {
                                                float sel = cur->radius * 1.5f;         /* 0x30028396 be70 */
                                                float pick = (sel < 60.0f) ? 60.0f        /* bff0 */
                                                           : (sel > 120.0f) ? 120.0f      /* c158 */
                                                           : sel;
                                                /* All-float chain 0x300283ed..0x30028405, ONE
                                                 * rounding (FSTP float [ESP+0x10] @0x30028409). */
                                                float fv = (float)coduo_crt_rand()       /* 0x300283e4 */
                                                           / FLAME_RAND_DIVISOR * 25.0f   /* be98 */
                                                           + pick;                        /* 0x30028405 */
                                                child->startSpeedBits = (uint32_t)CG_FloatBits(fv); /* 0x3002841b */
                                                if (fv > 290.0f) {                        /* 0x30028415 bebc */
                                                    fv = 290.0f;                          /* 0x30028425 0x43910000 */
                                                    child->startSpeedBits = 0x43910000u;
                                                }
                                                child->radius = fv / 2.0f;              /* 0x30028431 bce4 */
                                            }
                                            /* 0x3002843d: field_64 = CG_FlameGetSizeRate(child)*14.5f(c3a0). */
                                            child->sizeRate = CG_FlameGetSizeRate(child) * 14.5f;
                                            /* 0x3002844b..0x3002846b: field_a8 =
                                             *   (parent->expansionRate - 20.0f) - (1.0f - parent->centerOffset[2])*5.0f.
                                             * One x87 chain, ONE rounding (FSTP @0x3002846b);
                                             * no sub-expression is narrowed. */
                                            child->expansionRate = (cur->expansionRate - 20.0f)   /* be04 */
                                                - (1.0f - cur->centerOffset[2]) * 5.0f;            /* bde0 */
                                            /* 0x30028471..0x3002849c: field_148 =
                                             *   pow(1.0f-parent->lifeFraction, 0.75(c118))*0.5f(bce8) + 0.1f(bf6c).
                                             * The pow result is round-tripped through a FLOAT
                                             * slot (FSTP/FLD m32 @0x30028488) before the
                                             * multiply — the float temp is load-bearing. */
                                            {
                                                float powF = (float)powl(
                                                    1.0L - cur->lifeFraction, 0.75L);
                                                child->spawnScale = powF * 0.5f + 0.1f;
                                            }
                                            /* jmp 0x300285e1 */
                                        } else {
                                            /* 0x300284a7..0x300285df: bone-attached fire chunk. */
                                            child->endTime = cur->lifeStartTime + 1600.0;    /* 0x300284a7 c398 */
                                            if (cur->boneHandle != 0) {                   /* 0x300284b6 (true here) */
                                                child->startSpeedBits = (uint32_t)CG_FloatBits(cur->radius); /* 0x300284bd */
                                            } else {
                                                float sel = cur->radius * 1.5f;         /* 0x300284c8 be70 */
                                                float pick = (sel < 60.0f) ? 60.0f
                                                           : (sel > 120.0f) ? 120.0f : sel;
                                                /* All-float chain 0x3002851f..0x30028537, ONE
                                                 * rounding (FSTP float @0x3002853b). */
                                                float fv = (float)coduo_crt_rand()
                                                           / FLAME_RAND_DIVISOR * 25.0f + pick;
                                                child->startSpeedBits = (uint32_t)CG_FloatBits(fv); /* 0x3002853b */
                                            }
                                            {
                                                float fv = CG_FloatFromBits(child->startSpeedBits); /* 0x3002853e */
                                                if (fv > 290.0f) {                        /* 0x30028541 bebc */
                                                    child->startSpeedBits = 0x43910000u;        /* 0x3002854e */
                                                    fv = 290.0f;
                                                }
                                                child->radius = fv / 10.0f;             /* 0x3002855a bda4 */
                                            }
                                            /* 0x30028566: field_64 = CG_FlameGetSizeRate(child). */
                                            child->sizeRate = CG_FlameGetSizeRate(child);
                                            /* 0x3002856e: field_a8 = parent->radius * -0.35f(c390). */
                                            child->expansionRate = cur->radius * -0.35f;
                                            /* 0x30028580..0x300285ab: field_148 =
                                             *   (pow(1.0f-parent->lifeFraction, 0.75(c118)) + 1.0f)*0.5f.
                                             * The pow result is round-tripped through a FLOAT
                                             * slot (FSTP/FLD m32 @0x30028597) before the add —
                                             * the float temp is load-bearing. */
                                            {
                                                float powF = (float)powl(
                                                    1.0L - cur->lifeFraction, 0.75L);
                                                child->spawnScale = (powF + 1.0f) * 0.5f;
                                            }
                                            /* 0x300285b1..0x300285df: for a tagged world entity bias times. */
                                            if (cg_entities[cur->ownerInfoIndex].currentState.eType ==
                                                ET_VEHICLE) {
                                                child->endTime = child->endTime + 800.0;   /* c388 */
                                                child->expansionRate = (float)((double)child->expansionRate * 1.5); /* c278 */
                                            }
                                        }
                                    }

                                    /* 0x300285e1..0x30028843: build the child's emit velocity from the
                                     * parent's offset direction, jitter it, copy the world position. */
                                    {
                                        vec3_t ang;                          /* [esp+0x84] */
                                        vec3_t forward;                      /* [esp+0x60] (AngleVectors ESI) */
                                        vec3_t right;                        /* [esp+0x6c] (AngleVectors EDI) */
                                        /* 0x300285e1..0x300285f2: vectoangles(&parent->centerOffset[0], ang). */
                                        vectoangles(&cur->centerOffset[0], ang);
                                        /* 0x300285f7..0x30028608: AngleVectors(ang, forward, right, up=NULL).
                                         * EBX is XOR'd to 0 so the `up` output is skipped. */
                                        AngleVectors(ang, forward, right, (float *)0);
                                        /* 0x30028613..0x30028690: field_88/8c/90 =
                                         *   (2*(rand/32768) - 1) * right[i]. */
                                        child->driftDir[0] = (float)((2.0f * ((float)coduo_crt_rand() / FLAME_RAND_DIVISOR)
                                                          - 1.0f) * right[0]);            /* 0x30028636 [esp+0x6c] */
                                        child->driftDir[1] = (float)((2.0f * ((float)coduo_crt_rand() / FLAME_RAND_DIVISOR)
                                                          - 1.0f) * right[1]);            /* 0x3002865f [esp+0x70] */
                                        child->driftDir[2] = (float)((2.0f * ((float)coduo_crt_rand() / FLAME_RAND_DIVISOR)
                                                          - 1.0f) * right[2]);            /* 0x3002868c [esp+0x74] */
                                        /* 0x30028696..0x3002877d: field_88/8c/90 +=
                                         *   ((rand/32768)*0.45f(c174) + 0.25f(be58))
                                         *   * (1.0f - |parent->centerOffset[2]|) * forward[i].
                                         * The s chain is NEVER stored (it stays in st across
                                         * the FABS spill), and 1.0f-|off_z| is computed in a
                                         * register (only |off_z| itself is round-tripped
                                         * through m32, which is exact) — the ONLY rounding is
                                         * the final FSTP into driftDir[i]. */
                                        {
                                            long double s = (float)coduo_crt_rand() / FLAME_RAND_DIVISOR
                                                      * 0.45f + 0.25f;                     /* 0x30028696 */
                                            child->driftDir[0] = child->driftDir[0]
                                                + s * (1.0f - fabsf(cur->centerOffset[2])) * forward[0]; /* [esp+0x60] */
                                        }
                                        {
                                            long double s = (float)coduo_crt_rand() / FLAME_RAND_DIVISOR
                                                      * 0.45f + 0.25f;                     /* 0x300286e1 */
                                            child->driftDir[1] = child->driftDir[1]
                                                + s * (1.0f - fabsf(cur->centerOffset[2])) * forward[1]; /* [esp+0x64] */
                                        }
                                        {
                                            long double s = (float)coduo_crt_rand() / FLAME_RAND_DIVISOR
                                                      * 0.45f + 0.25f;                     /* 0x30028730 */
                                            child->driftDir[2] = child->driftDir[2]
                                                + s * (1.0f - fabsf(cur->centerOffset[2])) * forward[2]; /* [esp+0x68] */
                                        }
                                        /* 0x3002877f: VectorNormalize(&child->driftDir[0]) (result discarded). */
                                        (void)VectorNormalize(&child->driftDir[0]);
                                        /* 0x30028786..0x300287cb: field_94 =
                                         *   ((rand/32768)*|parent->expansionRate|)*(1.0f-parent->lifeFraction) + 2.0f(bce4).
                                         *   |field_a8| round-tripped through m32 (fstp/fld @0x300287a9). */
                                        /* One chain, one rounding (the FSTP @0x300287cb):
                                         * only |field_a8| is round-tripped through m32
                                         * (exact); (1.0f - lifeFraction) stays in a
                                         * register — no (double) narrowing of it. */
                                        child->driftSpeed =
                                            (float)coduo_crt_rand() / FLAME_RAND_DIVISOR
                                            * fabsf(cur->expansionRate)
                                            * (1.0f - cur->lifeFraction) + 2.0f;
                                        child->alpha = 0.0f;              /* 0x300287bd mov [ebp+0xbc],ebx(=0) */
                                        /* 0x300287d1..0x3002880a: copy parent world pos into +0x70..+0x78
                                         * and +0xd8..+0xe0. */
                                        child->localPos[0] = cur->worldPos[0];     /* 0x300287d1 */
                                        child->localPos[1] = cur->worldPos[1];
                                        child->localPos[2] = cur->worldPos[2];
                                        child->worldPos[0] = cur->worldPos[0];     /* 0x300287ec */
                                        child->worldPos[1] = cur->worldPos[1];
                                        child->worldPos[2] = cur->worldPos[2];
                                        /* 0x30028810..0x30028822: field_80 = parent->lifeStartTime; field_130 = -1.0(c0a8). */
                                        child->driftStartTime = cur->lifeStartTime;    /* 0x30028810 */
                                        child->lifeStartTime = -1.0;             /* 0x3002881c */
                                        /* 0x30028828..0x3002883d: field_98 = (float)(rand() % 360). */
                                        child->radiusBaseA = (float)(coduo_crt_rand() % 360); /* 0x30028828 IDIV 360 */
                                        /* 0x30028843: EBX restored to parent (EDI). */
                                    }
                                } else {
                                    /* --- SMOKE PATH (0x30028847 .. 0x300288a8) --- */
                                    if (cur->ownerInfoIndex != cg_snap->ps.psClientNum) {    /* 0x30028847..0x30028855 */
                                        child->spawnTime = child->spawnTime - sizeDelta;       /* 0x30028857 */
                                        child->endTime = child->endTime - sizeDelta * 2.0;  /* 0x30028861..0x300288a8 */
                                    } else {
                                        /* local owner: bias by pow(field_e8, 0.5(bd28)) + 1.0(bcf8).
                                         * Each (pow+1)*sizeDelta chain feeds its FSUBR directly
                                         * (no store), so each statement is ONE rounding — a
                                         * double temp would insert a rounding the DLL lacks. */
                                        child->spawnTime = child->spawnTime
                                            - (powl((long double)cur->lifeFraction, 0.5L) + 1.0) * sizeDelta; /* 0x30028884; base FLD m32 @0x30028869, result raw (FADD @0x3002887a) */
                                        child->endTime = child->endTime
                                            - (powl((long double)cur->lifeFraction, 0.5L) + 1.0) * sizeDelta; /* 0x300288a5; base FLD m32 @0x3002888a, result raw (FADD @0x3002889b) */
                                    }
                                }

                                /* 0x300288ab..0x30028906: restore the child's OWN link fields
                                 * from its pre-overwrite snapshot (staging = the child's state
                                 * captured at 0x30027fbe, undoing the wholesale parent copy)
                                 * and stamp spawn defaults. */
                                child->parent    = staging.parent;           /* [esp+0xc8] */
                                child->lifeStartTime = -1.0;                     /* 0x300288b2 c0a8 */
                                child->next      = staging.next;             /* [esp+0xc0] */
                                child->lifeStartTime2 = 0.0;                      /* 0x300288cc bcf0 */
                                child->listNext  = staging.listNext;         /* [esp+0xcc] */
                                child->prev      = staging.prev;             /* [esp+0xc4] */
                                child->listPrev  = staging.listPrev;         /* [esp+0xd0] */
                                child->emitCounter = -1;                       /* 0x300288ec */
                                child->emitScatterIndex  = 0;                        /* 0x300288ff */

                                /* 0x30028906..0x30028bed: for a field_2c==0 parent, build the child's
                                 * emit basis from cg_flameInfo[field_34] + the smoke curves. */
                                if (cur->kind == 0) {
                                    cgFlameInfo_t *info = &cg_flameInfo[cur->ownerInfoIndex];
                                    /* 0x30028911..0x3002898f: emitDir/prevEmitOrigin -> field_100..114. */
                                    child->emitBasis[0] = info->emitDir[0];         /* +0x34 -> +0x100 */
                                    child->emitBasis[1] = info->emitDir[1];         /* +0x38 -> +0x104 */
                                    child->emitBasis[2] = info->emitDir[2];         /* +0x3c -> +0x108 */
                                    child->emitOrigin[0] = info->prevEmitOrigin[0];  /* +0x10 -> +0x10c */
                                    child->emitOrigin[1] = info->prevEmitOrigin[1];  /* +0x14 -> +0x110 */
                                    child->emitOrigin[2] = info->prevEmitOrigin[2];  /* +0x18 -> +0x114 */
                                    /* 0x3002898f..0x30028a45: field_a8 build.
                                     *   z0   = ((1.0f - pow(field_94/900,2.0)) + 1.0f) * 0.0f;
                                     *          (APPARENT SOURCE ODDITY: *0.0f no-op term, computed
                                     *           by the machine; preserved)
                                     *   acc  = (rand/32768)*3.0f + z0;                            (be5c)
                                     *   acc += pow(field_e4/290, 2.0)*16.0f;                      (bebc/bde8/bf00)
                                     *   acc += pow(field_e8, 1.0)*10.0f;                          (bcf8/bda4)
                                     *   field_a8 = acc * (parent->expansionRate/60.0f) * -2.25f;       (bff0/c380) */
                                    {
                                        /* pInv IS stored, to a FLOAT slot (FSTP m32 @0x300289c0),
                                         * and each pow return is likewise round-tripped through m32
                                         * (0x300289a6/0x30028a00/0x30028a21). But `acc` itself is
                                         * NEVER stored: 0x300289e5 FADD leaves it in st and the
                                         * FADDP/FADDP/FMULP/FMUL run on to the single
                                         * FSTP float [EBP+0xa8] @0x30028a45 — ONE rounding.
                                         * All operands are `float ptr`, incl. -2.25f (c380). */
                                        float pInv = (1.0f - (float)powl(
                                                          (long double)cur->driftSpeed / 900.0f, 2.0L)
                                                          + 1.0f) * 0.0f;                     /* [esp+0x14] */
                                        float rf = (float)coduo_crt_rand();
                                        float radiusPow = (float)powl(
                                            (long double)child->radius / 290.0f, 2.0L);
                                        float lifePow = (float)powl(
                                            (long double)cur->lifeFraction, 1.0L);
                                        long double acc =
                                            (long double)rf / FLAME_RAND_DIVISOR * 3.0f + pInv;
                                        acc += (long double)radiusPow * 16.0f;
                                        acc += (long double)lifePow * 10.0f;
                                        child->expansionRate = (float)(acc
                                            * ((long double)cur->expansionRate / 60.0f)
                                            * -2.25f);                                         /* c380 */
                                    }
                                    /* 0x30028a4b..0x30028ab0: field_60 =
                                     *   (((rand/32768)*0.0(bcf0)) + pow(1.0f-field_e8,5.0(c378))*0.5f(bce8)
                                     *    + 0.2(c370)) * (parent->smokeDensityRate*0.25f(be58) + 105.0f(c368)) * 0.75(be38). */
                                    {
                                        /* pow result round-tripped through m32 (fstp/fld DWORD @0x30028a83).
                                         * APPARENT SOURCE ODDITY: the rand() sample is multiplied by the
                                         * 0.0 pool double (bcf0) — a no-op term the machine still computes
                                         * (rand() is called, advancing the PRNG); preserved.
                                         * Neither the left factor nor the right is ever stored: the left
                                         * stays in st(1) (FADD double c370 @0x30028a93) while the right is
                                         * built in st(0), and FMULP @0x30028aae feeds the single
                                         * FSTP float [EBP+0x60] @0x30028ab0 — ONE rounding for the lot. */
                                        float rf = (float)coduo_crt_rand();
                                        float powF = (float)powl(
                                            1.0L - cur->lifeFraction, 5.0L);
                                        long double left =
                                            ((long double)rf / FLAME_RAND_DIVISOR) * 0.0
                                            + (long double)powF * 0.5f
                                            + (long double)(double)0.2f;
                                        long double right =
                                            (long double)child->smokeDensityRate * 0.25f
                                            + 105.0f;
                                        child->smokeDensityRate = (float)(
                                            left * right * 0.75f);                    /* be38 */
                                    }
                                    /* 0x30028ab3..0x30028b04: when the parent's owner (field_38) flame-info
                                     * clientFrame >= cg_clientFrame-1, bias field_e4 by
                                     * (1.0f - pow(1.0f-field_e8, 4.0(c360)))*20.0f(be04) + field_e4. */
                                    {
                                        cgFlameInfo_t *owner = &cg_flameInfo[cur->ownerClientNum];
                                        int32_t previousFrame = coduo_int32_from_bits(
                                            (uint32_t)cg_clientFrame - 1u);
                                        if (owner->clientFrame >= previousFrame) {
                                            /* pow's ST0 return is rounded to FLOAT (FSTP/FLD m32
                                             * @0x30028ae4/ae8) — a double `g` would round twice.
                                             * After the reload it is ONE chain to the single
                                             * FSTP float [EBP+0xe4] @0x30028afe. */
                                            float g = (float)powl(1.0L - cur->lifeFraction, 4.0L);
                                            child->radius = (1.0f - g) * 20.0f      /* be04 */
                                                              + child->radius;
                                        }
                                    }
                                    /* 0x30028b04: field_68 = parent->lifeStartTime. */
                                    child->spawnTimeCopy = cur->lifeStartTime;
                                    /* 0x30028b0d..0x30028b54: fv = clamp(field_60 to [0,290]);
                                     *   field_60 = fv (raw bits, 0x30028b47); field_bc = 0.15f;
                                     *   field_5c = 140.0f(bec8) - fv (0x30028b54). */
                                    {
                                        float fv;
                                        if (!(child->smokeDensityRate < 0.0f)) {           /* 0x30028b10 bcec */
                                            fv = (child->smokeDensityRate > 290.0f) ? 290.0f /* 0x30028b24 bebc */
                                                                            : child->smokeDensityRate;
                                        } else {
                                            fv = 0.0f;                             /* 0x30028b1d */
                                        }
                                        child->smokeDensityRate = fv;                      /* 0x30028b47 */
                                        child->alpha = 0.15f;                   /* 0x30028b4a 0x3e19999a */
                                        {
                                            float t = 140.0f - fv;                 /* 0x30028b54 bec8 */
                                            child->startSpeedBits = (uint32_t)CG_FloatBits(t); /* stored as float bits */
                                        }
                                    }
                                    /* 0x30028b57..0x30028b7a: field_98 += (rand() % 15). */
                                    child->radiusBaseA = child->radiusBaseA + (float)(coduo_crt_rand() % 15);
                                    /* 0x30028b80..0x30028bab: field_148 =
                                     *   pow(1.0f-parent->lifeFraction, 2.0(bde8))*0.65f(c1fc) + 0.35f(c0e4). */
                                    {
                                        float powF = (float)powl(
                                            1.0L - cur->lifeFraction, 2.0L);
                                        child->spawnScale = (float)(
                                            (long double)powF * 0.65f + 0.35f);
                                    }
                                    /* 0x30028bb1..0x30028be7: if the parent belongs to the local player
                                     * (field_34 == cg_snap->ps.psClientNum) field_94 =
                                     *   (child->lifeFraction*0.2f(be10) + 0.1f(bf6c)) * field_94; else *0.25f(be58). */
                                    if (cur->ownerInfoIndex == cg_snap->ps.psClientNum) {
                                        /* One chain (0x30028bc1..0x30028bd3), one rounding: both
                                         * branches converge on FSTP float [EBP+0x94] @0x30028be7. */
                                        child->driftSpeed = (child->lifeFraction * 0.2f + 0.1f)
                                                          * child->driftSpeed;
                                    } else {
                                        child->driftSpeed = child->driftSpeed * 0.25f;
                                    }
                                }

                                /* 0x30028bed..0x30028eee: SMOKE SUB-SPAWN. Only for a free
                                 * (boneHandle==0), non-child (field_2c!=2) parent whose field_e8 is in
                                 * (0.02(c2b8), 0.8(c358)) and passes a rand probability gate; spawns a
                                 * SECOND chunk (field_2c==5) that copies the child and gets its own
                                 * jittered smoke position/size. */
                                if (cur->boneHandle == 0                                     /* 0x30028bed */
                                    && cur->kind != FLAME_CHUNK_KIND_CHILD              /* 0x30028bf8 CMP,2 */
                                    && cur->lifeFraction > 0.02f                                 /* 0x30028c04 c2b8 */
                                    && cur->lifeFraction < 0.8) {                                /* 0x30028c1b c358 */
                                    /* 0x30028c32/0x30028c47: kind and owner weights. */
                                    float w4c = (cur->kind == FLAME_CHUNK_KIND_FIRE) ? 1.0f : 0.5f;
                                    float w3c = (cur->ownerInfoIndex == cg_snap->ps.psClientNum) ? 1.0f : 0.5f;
                                    /* 0x30028c68..0x30028cb1: probability gate —
                                     *   spawn only if pow(field_94/900,2.0)*w3c*w4c > rand/32768. */
                                    /* The target calls rand first and leaves its
                                     * divided threshold live below the later
                                     * _CIpow operands on the x87 stack. */
                                    float gateRand = (float)coduo_crt_rand();
                                    long double gateThreshold =
                                        (long double)gateRand / FLAME_RAND_DIVISOR;
                                    float gate = (float)powl(
                                        (long double)cur->driftSpeed / 900.0f, 2.0L);
                                    if ((long double)gate * w3c * w4c > gateThreshold) {
                                        int32_t smokeSeed = coduo_crt_rand() % 6;   /* 0x30028cb7 idiv 6 -> [esp+0x18] */
                                        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                                        flameChunk_t *smoke = CG_SpawnFlameChunk((flameChunk_t *)0);
                                        if (smoke == NULL) {
                                            goto finalize_child;
                                        }
                                        flameChunk_t smokeStaging = *smoke; /* [esp+0x210] REP MOVSD */
                                        *smoke = *child;                    /* REP MOVSD child(ebp)->smoke */
                                        smoke->lifeStartTime = -1.0;            /* 0x30028d00 c0a8 */
                                        smoke->lifeStartTime2 = 0.0;             /* 0x30028d06 bcf0 */
                                        /* 0x30028d12..0x30028d33: restore link fields from smokeStaging. */
                                        smoke->next     = smokeStaging.next;      /* [esp+0x210] */
                                        smoke->parent   = smokeStaging.parent;    /* [esp+0x218] */
                                        smoke->prev     = smokeStaging.prev;      /* [esp+0x214] */
                                        smoke->listNext = smokeStaging.listNext;  /* [esp+0x21c] */
                                        smoke->listPrev = smokeStaging.listPrev;  /* [esp+0x220] */
                                        smoke->kind  = FLAME_CHUNK_KIND_SMOKE; /* 0x30028d36 =5 */
                                        smoke->emitCounter = -1;               /* 0x30028d3d */
                                        smoke->emitScatterIndex  = 0;                /* 0x30028d4a */
                                        smoke->ownerInfoIndex = ENTITYNUM_WORLD; /* 0x30028d51 = 1022 */
                                        /* 0x30028d58..0x30028da6: field_a8 = clamp(
                                         *   (parent->lifeFraction*1.85f(c350) + 0.55f(c1b0)) * smoke->expansionRate,
                                         *   to [-15.0f(c34c), -2.0f(c150)]). EDI still parent via [esp+0x24]. */
                                        {
                                            /* One chain 0x30028d58..0x30028d6a, ONE rounding
                                             * (FSTP float [ESP+0x10] @0x30028d70); all operands
                                             * are `float ptr`. */
                                            float v = (cur->lifeFraction * 1.85f + 0.55f)
                                                      * smoke->expansionRate;
                                            float clamped = (v < -15.0f) ? -15.0f           /* c34c */
                                                          : (v > -2.0f) ? -2.0f             /* c150 */
                                                          : v;
                                            smoke->expansionRate = clamped;       /* 0x30028dac */
                                        }
                                        /* 0x30028da6..0x30028df4: field_5c = clamp(field_e4*3.5f(bef8)
                                         *   + 20.0f(be04) to <=290 else 290); field_e4 = field_5c/20.0f(be04). */
                                        {
                                            /* One chain 0x30028da6..0x30028db8, ONE rounding
                                             * (FSTP float [ESP+0x10] @0x30028dbe). */
                                            float fv = smoke->radius * 3.5f + 20.0f;
                                            smoke->startSpeedBits = (uint32_t)CG_FloatBits(fv);  /* 0x30028dd0 */
                                            if (fv > 290.0f) {                          /* 0x30028dca bebc */
                                                smoke->startSpeedBits = 0x43910000u;          /* 0x30028dda */
                                                fv = 290.0f;
                                            }
                                            smoke->radius = fv / 20.0f;               /* 0x30028de8 be04 */
                                        }
                                        /* 0x30028df4..0x30028e23: field_68 = parent->lifeStartTime;
                                         *   field_e0 = field_78 = parent->radius*0.19f(c348)
                                         *                          + parent->worldPos[2] + 6.0f(bddc). */
                                        smoke->spawnTimeCopy = cur->lifeStartTime;              /* 0x30028df4 */
                                        {
                                            /* One chain 0x30028dfd..0x30028e0f, ONE rounding
                                             * (FSTP float [ESP+0x14] @0x30028e15). */
                                            float z = cur->radius * 0.19f              /* c348 single */
                                                      + smoke->worldPos[2] + 6.0f;     /* [ebx+0xe0]; bddc */
                                            smoke->worldPos[2] = z;                        /* 0x30028e1d */
                                            smoke->localPos[2] = z;                        /* 0x30028e23 */
                                        }
                                        /* 0x30028e26..0x30028e59: field_98 =
                                         *   (2*(rand/32768) - 1) * 25.0f(be98) + 180.0f(bd50). */
                                        {
                                            float rf = (float)coduo_crt_rand();
                                            long double roll = (2.0f
                                                * ((long double)rf / FLAME_RAND_DIVISOR) - 1.0f)
                                                * 25.0f + 180.0f;
                                            smoke->radiusBaseA = (float)roll;
                                        }
                                        /* 0x30028e5f..0x30028e66: field_30 = cg_flameSmokeMaterials[smokeSeed]. */
                                        smoke->overrideMaterial = (uint32_t)cg_flameSmokeMaterials[smokeSeed];
                                        /* 0x30028e69..0x30028e6f: field_48 = parent->lifeStartTime. */
                                        smoke->spawnTime = cur->lifeStartTime;
                                        /* 0x30028e72..0x30028ea1: field_50 =
                                         *   (rand/32768)*2600.0(c340) + parent->lifeStartTime + 1600.0(c398). */
                                        {
                                            float rf = (float)coduo_crt_rand();
                                            long double endTime =
                                                ((long double)rf / FLAME_RAND_DIVISOR)
                                                * 2600.0
                                                + (long double)cur->lifeStartTime
                                                + 1600.0;
                                            smoke->endTime = (double)endTime;
                                        }
                                        /* 0x30028ea4..0x30028ea9: field_64 = CG_FlameGetSizeRate(smoke). */
                                        smoke->sizeRate = CG_FlameGetSizeRate(smoke);
                                        /* 0x30028eac..0x30028ebd: field_e8 =
                                         *   (parent->lifeStartTime - field_48) / (field_50 - field_48). */
                                        smoke->lifeFraction = (float)((cur->lifeStartTime - smoke->spawnTime)
                                            / (smoke->endTime - smoke->spawnTime));
                                        /* 0x30028ec3..0x30028ed4: CG_AdvanceFlameChunkSize(smoke,
                                         *   _ftol2(parent->lifeStartTime)).  Each site loads
                                         *   the double directly into ST0; there is no m32 spill. */
                                        {
                                            int32_t st = coduo_fp_to_i32_extended(
                                                (long double)cur->lifeStartTime); /* 0x30028ec3 */
                                            CG_AdvanceFlameChunkSize(smoke, st);        /* 0x30028ed1 */
                                            st = coduo_fp_to_i32_extended(
                                                (long double)cur->lifeStartTime); /* 0x30028ed6 */
                                            CG_MoveFlameChunk(smoke, st);       /* 0x30028ee6 */
                                        }
                                    }
                                }
                            }

finalize_child:
                            /* 0x30028ef0..0x30028f27: after the jitter block, re-finalize the
                             * child at the CURRENT cg_flameTime and read the parent's field_2c
                             * for the step-advance dispatch. child->lifeFraction recomputed from
                             * the current flame time. (ebx is restored to the parent here.) */
                            {
                                int32_t ft = coduo_int32_from_bits(cg_flameTime); /* 0x30028ef0 */
                                child->lifeFraction = (float)(((double)ft - child->spawnTime)
                                                          / (child->endTime - child->spawnTime));
                                CG_AdvanceFlameChunkSize(child, ft);        /* 0x30028f18, EAX=child */
                                CG_MoveFlameChunk(child, ft);       /* 0x30028f22 */
                            }

                            /* 0x30028f27: stepKind = parent->kind (into ESI). */
                            stepKind = cur->kind;

                            /* 0x30028f2d..0x30028f5e: for a short-life parent (field_2c < 2),
                             * grow the life-rate and rebuild the step accumulator. */
                            if (stepKind < 2) {
                                cur->lifeRate = (2.0 * cur->lifeFraction + 1.0) * cur->lifeRate;
                                stepAccum = (cur->endTime - cur->spawnTime) * cur->lifeRate;
                            }

                            /* 0x30028f62..0x30029017: view-distance step scaling. This
                             * whole block (the dist computation, its 2.0f clamp, and the
                             * stepKind==0 stepAccum scale) executes ONLY on this
                             * child!=NULL path — a pool-exhausted spawn (0x30027f93
                             * JZ 0x3002901b) jumps straight to the rand() step advance
                             * below, past all of it. */
                            /* 0x30028f62..0x30028fb3: dist = |cg_refdef.vieworg - parent
                             * world pos (field_d8/dc/e0)|. */
                            float ddx = cg_refdef.vieworg[0] - cur->worldPos[0]; /* 0x30028f62 */
                            float ddy = cg_refdef.vieworg[1] - cur->worldPos[1]; /* 0x30028f72 */
                            float ddz = cg_refdef.vieworg[2] - cur->worldPos[2]; /* 0x30028f82 */
                            /* The square sum is never stored: the FADDP at 0x30028fac
                             * hands an inline FSQRT its argument raw in ST0, followed by
                             * one float rounding at 0x30028fb3. */
                            long double distSquared =
                                (long double)ddz * (long double)ddz
                                + (long double)ddy * (long double)ddy
                                + (long double)ddx * (long double)ddx;
                            float dist = (float)coduo_x87_sqrtl(distSquared);

                            /* 0x30028fb7..0x30028fd8: clamp dist up to 2.0f. */
                            if (dist < 2.0f) {
                                dist = 2.0f;                        /* 0x30028fd0 0x40000000 */
                            }

                            /* 0x30028fd8..0x30029017: scale stepAccum by the near/far speed
                             * factor, but only for a non-child parent whose owner index is not
                             * the local client and whose radius*4 exceeds the view distance. */
                            if (stepKind == 0                                  /* 0x30028fd8 TEST ESI */
                                && cur->ownerInfoIndex != cg_snap->ps.psClientNum          /* 0x30028fe4 */
                                && cur->radius * 4.0f > dist) {  /* 0x30028fec, all-float FCOMP */
                                /* NB the DLL uses BOTH widths of 4 here: the compare above
                                 * takes float 4.0f (be40), this scale takes double 4.0
                                 * (c360, FMUL m64 @0x30029009). One chain, one FSTP double
                                 * @0x30029017; dist enters as a float OPERAND (FDIV m32
                                 * @0x3002900f), which is not a result rounding. */
                                stepAccum = (double)cur->radius * 4.0 / dist
                                          * stepAccum;              /* 0x30029003..0x30029013 */
                            }
                        }
                        /* When child == NULL (0x30027f93 JZ 0x3002901b) everything since
                         * the spawn — including the view-distance scaling above — is
                         * skipped and control joins here with stepAccum still holding
                         * the seed. */

                        /* 0x3002901b..0x3002904c: field_130 += (rand()/32768)*stepAccum
                         * + stepAccum/2.0. rand() is FILD'd then round-tripped through a
                         * single stack slot (fstp/fld m32) so it is rounded to float; the
                         * subsequent /32768.0f and the two products stay in x87 width. */
                        {
                            float rf = (float)coduo_crt_rand();               /* 0x3002901b..0x3002902c */
                            cur->lifeStartTime += (double)rf / FLAME_RAND_DIVISOR * stepAccum
                                            + stepAccum / 2.0;      /* fdiv 2.0(double bde8) */
                        }

                        /* 0x30029052..0x30029063: back-edge — re-enter the head while the
                         * parent's field_130 is still strictly above 0.0 (the true loop exit
                         * is gate #3 at the head: field_130 >= flameTime). FCOMP field_130 vs
                         * 0.0(double); TEST AH,0x41; JE head -> continue while field_130 > 0. */
                        if (!(cur->lifeStartTime > 0.0)) {
                            break;                      /* fall to 0x30029069 */
                        }
                    }
                }

            inner_final_tail:
                /* 0x30029069..0x300290a9: final size-advance + finalize + relink for this
                 * chunk.
                 *   field_e8 = ((double)cg_flameTime - field_48)/(field_50 - field_48);
                 *   CG_AdvanceFlameChunkSize(cur, (int)cg_flameTime);
                 *   CG_MoveFlameChunk(cur, (int)cg_flameTime);
                 *   ESI restored from [ESP+0x58] (2 pushes deep = base+0x50, the
                 *   loop-top latch written once at 0x30027e0a) — the machine reuses
                 *   the LATCHED next pointer rather than re-reading cur->next, so no
                 *   re-read is done here; `next` already holds it. EBP=0; EDI=1. */
                {
                    int32_t ft = coduo_int32_from_bits(cg_flameTime);       /* 0x30029069 FILD/MOV */
                    cur->lifeFraction = (float)(((double)ft - cur->spawnTime)
                                            / (cur->endTime - cur->spawnTime));
                    CG_AdvanceFlameChunkSize(cur, ft);                  /* 0x30029091, EAX=cur */
                    CG_MoveFlameChunk(cur, ft);                 /* 0x3002909b */
                }
                goto outer_advance;
            }

        expire_chunk:
            /* 0x300290b0..0x300290be: mark the chunk processed (listMarker=1, stored
             * unconditionally), then free it only when field_2c > 1.
             *   CMP [+0x2c],EDI(=1); MOV [+0x24],EDI; JLE 0x300290c1 (field_2c<=1 skip);
             *   else PUSH EBX; CALL CG_FreeFlameChunk. */
            cur->listMarker = 1;                    /* 0x300290b3 */
            if (cur->kind > 1) {                /* 0x300290b6 JLE -> advance if <=1 */
                CG_FreeFlameChunk(cur);             /* 0x300290b8 */
            }
            /* fall through to advance */

        outer_advance:
            /* 0x300290c1..0x300290c9: cur = next; loop while next != NULL. */
            cur = next;
        }
    }

    /* ---------------------------------------------------------------------
     * REGION C — TEARDOWN (0x300290cf .. 0x300291b3)
     * ------------------------------------------------------------------- */

    /* 0x300290cf..0x300290ed: clear the once-per-frame flame-damage flag if the
     * local owner's flame-damage window has lapsed. */
    if (cg_flameInfo[cg_snap->ps.psClientNum].activeUntil
        < coduo_int32_from_bits(cg_flameTime)) {
        cg_flameDamageTakenThisFrame = 0;
    }

    /* 0x300290f5..0x30029166: INNERMOST loop over cg_flameChunkList (->listNext). */
    {
        flameChunk_t *node = cg_flameChunkList;
        while (node != (flameChunk_t *)0) {
            flameChunk_t *snext = node->listNext;   /* +0x0c latched at top (0x30029103) */

            if (node->listMarker != 0) {            /* 0x30029100 CMP [+0x24],0; JZ 0x30029142 */
                /* dead node: if it is still the registered secondary head for its
                 * owner index, null out that head + the paired cg_flameInfo entry,
                 * then free it. */
                cgFlameInfo_t *info = &cg_flameInfo[node->ownerInfoIndex];
                if (info->ownerChunk == node) { /* 0x30029111 */
                    info->ownerChunk = NULL;        /* 0x30029122 [+0x300ab790]=0 */
                    info->clientFrame = 0;     /* 0x30029131 [+0x300ab750]=0 */
                }
                CG_FreeFlameChunk(node);            /* 0x30029138 */
            } else {
                /* live node: render it unless it is a dead-but-not-owner remnant.
                 * if (field_a0 == 0 OR cgFlameInfo[field_34].field_40 == node)
                 *    CG_AddFlameChunks(node). */
                cgFlameInfo_t *info = &cg_flameInfo[node->ownerInfoIndex];
                if (node->deadFlag == 0                              /* 0x30029142 */
                    || info->ownerChunk == node) { /* 0x30029153 */
                    CG_AddFlameChunks(node);        /* 0x3002915b */
                }
            }

            node = snext;                           /* 0x30029160..0x30029164 */
        }
    }

    /* 0x30029166..0x30029190: arm the owner's flame-damage window if past. */
    {
        int32_t nowTime = coduo_int32_from_bits(cg_flameTime);
        int32_t *armed = &cg_flameInfo[cg_snap->ps.psClientNum].activeUntil;
        if (*armed < nowTime) {
            *armed = coduo_int32_from_bits((uint32_t)nowTime + 100u);
        }
    }

    /* 0x30029192..0x300291a1: report this frame's flame-damage-taken flag. */
    cgame_syscall(0x58, cg_flameDamageTakenThisFrame);

    /* 0x300291a4: CG_UpdateFlamethrowerSounds(). */
    CG_UpdateFlamethrowerSounds();
}
