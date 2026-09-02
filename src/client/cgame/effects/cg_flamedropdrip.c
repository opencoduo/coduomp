// Source: uo_cgame_mp_x86.dll 0x30027ad0..0x30027d0b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30027ad0_30027d0b.mcode
//
// CG_FlameDropDrip — spawn a fresh flame chunk
// derived from `parent`, clone the parent's render/physics state into it while
// preserving the new node's own list links, apply a small random jitter to the
// chunk's drift direction and renormalize it, then seed the new chunk's size,
// life span, and per-frame rate scalars from the two float parameters.
//
// Name adjudication: the .mcode header's "G_ExplodeSmokeGrenade" is a pure size
// match (win size 0x23b == corpus size 0x23b) and is REJECTED per the
// no-size-matching rule. The behavior proves cgame flame-chunk work: it calls
// CG_SpawnFlameChunk (0x30025600) to pop a pool node, clones a flameChunk_t
// (0x54 dwords == 336 bytes == sizeof(flameChunk_t)), VectorNormalize()s the
// chunk's drift direction (field_88..field_90), and calls CG_FlameGetSizeRate
// (0x30023b70). The Mac cgame symbol CG_FlameDropDrip has the identical three
// named direct callees, resolving the source name.
//
// ABI (cdecl, proven from the frame): plain SUB ESP frame + PUSHes, plain RET
// (no immediate), caller cleans. Three stack args at the original call frame:
//   parent  = [origEsp+4]   (loaded into EBP)
//   a       = [origEsp+8]   ([esp+0x170] after the prologue) — a float
//   b       = [origEsp+0xc] ([esp+0x174] after the prologue) — a float
// Returns the new flameChunk_t* in EAX (MOV EAX,EBX at 0x30027cd1), or NULL on
// the spawn-failure path (XOR EAX,EAX at 0x30027d01). The shared decl previously
// modeled this as `void`; the machine code proves a flameChunk_t* return.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

// .rdata single/double constants consumed by this function (re-verified against
// objdump -s -j .rdata):
//   0x3007bcec = 0.0f          (drift-gate compare bound)
//   0x3007be0c = 0.6f          (drift-speed decay factor)
//   0x3007bec0 = 1/32768       (rand() -> unit-interval scale; 0x38000000)
//   0x3007bce0 = 1.0f          (the "* 2 - 1" symmetric-jitter recentre)
//   0x3007be6c = 0.05f         (drift-jitter amplitude)
//   0x3007bebc = 290.0f        (size clamp maximum)
//   0x3007bcf0 = 0.0  (double) (field_140 seed; bytes 00000000 00000000. The
//                               1.0 double is the next slot, 0x3007bcf8.)
//   0x3007beb8 = 1.4f          (life-span rand bias)
//   0x3007bdf8 = 3.0  (double) (life-rate lower bound on `a`)
//   0x3007beb0 = 1.0/3.0 (double)
//   0x3007bea8 = 25.0 (double)
//   0x3da3d70a = 0.08f         (field_bc seed)
/* The DLL scales rand() by MULTIPLYING the folded reciprocal 1/32768 held at
 * 0x3007bec0 (FMUL m32) -- it never loads 32768.0f and never divides on this
 * path. Same idiom as cg_calcviewshake.c's CG_SHAKE_RAND_NORM. (Contrast
 * cg_poissondisksample.c / cg_fireflamechunks.c, which genuinely FDIV by
 * 32768.0f at 0x3007bd10 -- both idioms exist in this DLL, so they are per-site.)
 * Value-identical either way (1/32768 is an exact power of two); this matches
 * the operation and the constant the bytes actually use. */
#define FLAME_DRIP_RAND_NORM 3.0517578125e-05f   /* 0x3007bec0 == 1/32768 */

flameChunk_t *CG_FlameDropDrip(flameChunk_t *parent /* [esp+4] */,
                                       float a /* [esp+0x170] */,
                                       float b /* [esp+0x174] */)
{
    // 30027ae1/30027ae3: ESI=0 (root parent), CALL CG_SpawnFlameChunk. The pool
    // spawner takes its `parent` arg in ESI; XOR ESI,ESI means a ROOT chunk.
    flameChunk_t *chunk = CG_SpawnFlameChunk(NULL);

    // 30027aea/30027aec: if the pool is exhausted, return NULL immediately.
    if (chunk == NULL) {
        return NULL;   // 30027d01: XOR EAX,EAX
    }

    // 30027af2/30027afe: evaluate the drift-speed gate (b <= 0.0f or unordered) early via
    // FLD b / FCOMP 0.0f; the FNSTSW/TEST is deferred until after the clone.
    // FCOMP sets C3/C2/C0; TEST AH,0x41 is nonzero for below, equal, or unordered.
    qboolean driftGateZero = !(b > 0.0f);

    // 30027b04..30027b1d: two 0x54-dword (336-byte == sizeof(flameChunk_t))
    // block copies.
    //   (1) save the freshly spawned node's own bytes into a stack buffer, then
    //   (2) clone the entire parent over the new node.
    // The saved link fields are then written back so the new node keeps its own
    // place on the pool free/active list and the secondary flame list rather
    // than inheriting the parent's links.
    flameChunk_t savedLinks = *chunk;   // 30027b06/30027b0a: rep movsd chunk -> [esp+0x18]
    *chunk = *parent;                   // 30027b19/30027b1d: rep movsd parent -> chunk

    // 30027b1f..30027b4c: restore the new node's own list links from the saved copy.
    chunk->next     = savedLinks.next;      // 30027b1f/30027b2d
    chunk->prev     = savedLinks.prev;      // 30027b26/30027b2a
    chunk->parent   = savedLinks.parent;    // 30027b0c/30027b23
    chunk->listNext = savedLinks.listNext;  // 30027b10/30027b49
    chunk->listPrev = savedLinks.listPrev;  // 30027b2f/30027b4c

    // 30027b38/30027b3f: overwrite two cloned-from-parent fields with fresh values.
    chunk->kind  = 3;             // provisional flame-chunk kind == 3
    chunk->emitCounter = 0xffffffff;    // emit counter reset to -1

    // 30027b4f..30027b6d: seed the drift speed. When b is not ordered above 0.0
    // the drift speed is zeroed; otherwise the cloned parent's value decays by 0.6.
    if (driftGateZero) {
        chunk->driftSpeed = 0.0f;               // 30027b65
    } else {
        chunk->driftSpeed = (float)(
            (long double)chunk->driftSpeed * (long double)0.6f);
    }

    // 30027b6f..30027bee: perturb the drift direction (field_88/8c/90) by an
    // independent symmetric jitter per component:
    //   d += ((rand()/32768.0f) * 2.0f - 1.0f) * 0.05f   in [-0.05, +0.05].
    // ESI is left pointing at &field_88 for the VectorNormalize call below.
    /* FILD feeds FMUL directly at 0x30027b7e/b82, 0x30027ba3/ba7 and 0x30027bd0/bd4
     * (no intervening store), so rand() stays exact in 80-bit -- no (float) cast. */
    chunk->driftDir[0] = (float)(
        (long double)chunk->driftDir[0] +
        (((long double)coduo_crt_rand() *
              (long double)FLAME_DRIP_RAND_NORM * 2.0L) -
         1.0L) * (long double)0.05f);
    chunk->driftDir[1] = (float)(
        (long double)chunk->driftDir[1] +
        (((long double)coduo_crt_rand() *
              (long double)FLAME_DRIP_RAND_NORM * 2.0L) -
         1.0L) * (long double)0.05f);
    chunk->driftDir[2] = (float)(
        (long double)chunk->driftDir[2] +
        (((long double)coduo_crt_rand() *
              (long double)FLAME_DRIP_RAND_NORM * 2.0L) -
         1.0L) * (long double)0.05f);

    // 30027bf4/30027c00: renormalize the drift direction in place (ESI = &field_88).
    // The returned length (left on ST0) is discarded (FSTP ST0).
    VectorNormalize(&chunk->driftDir[0]);

    // 30027bf9..30027c1e: stash `a` and `b` (raw bits) and the fixed 0.08f seed.
    chunk->radius = a;         // 30027c12: field_e4 = a (radius/expansion base)
    chunk->expansionRate = b;         // 30027c18: field_a8 = b (drift rate)
    chunk->alpha = 0.08f;     // 30027c1e: 0x3da3d70a

    // 30027c02..30027c3f: field_5c = min(a * 2.0f, 290.0f). FLD a; FADD ST,ST
    // gives a*2; FCOM 290.0f; when a*2 >= 290 or is unordered the value is
    // replaced by 290.0f before the store.
    {
        long double sizeTarget = (long double)a + (long double)a;
        if (!(sizeTarget < (long double)290.0f)) {
            sizeTarget = (long double)290.0f;
        }
        // 30027c3f FSTP float [EBX+0x5c]: the dword receives the FLOAT BITS of
        // sizeTarget (startSpeedBits is the dual-role uint32_t field).
        chunk->startSpeedBits = (uint32_t)CG_FloatBits((float)sizeTarget);
    }

    // 30027c3d/30027c42/30027c47: field_64 = CG_FlameGetSizeRate(parent). The
    // helper takes `this` in ESI (= EBP = parent) and returns a float on ST0.
    chunk->sizeRate = CG_FlameGetSizeRate(parent);

    // 30027c4a..30027c5c: fixed double seeds.
    // 30027c4a FLD double [0x3007bcf0]; c50 FSTP double [EBX+0x140]. The pool
    // double at 0x3007bcf0 is 0.0 (bytes 00000000 00000000); the 1.0 double is
    // the NEXT slot, 0x3007bcf8.
    chunk->lifeStartTime2 = 0.0;                       // 30027c4a/c50: 0x3007bcf0
    chunk->spawnTime =
        (double)coduo_int32_from_bits(cg_flameTime); // 30027c56/c5c: FILD timestamp

    // 30027c5f..30027c85: seed the chunk end timestamp.
    //   field_50 = (rand()*(1/32768) * 2.0f + 1.4f)
    //              * (parent->endTime - parent->spawnTime) + field_48.
    // ONE x87 chain, ONE rounding (the FST double at 30027c85). FST (not FSTP)
    // keeps the UNROUNDED value on the stack; the lifeRate division below
    // consumes that retained register value, not the stored double.
    // FILD 30027c68 feeds FMUL 30027c6c with no store, and nothing narrows the
    // left factor before the FMULP at 30027c80 -- so neither the (float) nor the
    // (double) cast the recon used belongs here.
    long double endTimeFull =
        ((long double)coduo_crt_rand() *
             (long double)FLAME_DRIP_RAND_NORM * 2.0L +
         (long double)1.4f) *
            ((long double)parent->endTime -
             (long double)parent->spawnTime) +
        (long double)chunk->spawnTime;
    chunk->endTime = (double)endTimeFull;

    // 30027c88..30027cbe: field_138 = (max(a, 3.0) * 25.0 / 3.0)
    //                                 / (field_50 - field_48).
    // FLD a; FCOMP 3.0 => when a <= 3.0 or unordered use 3.0, else use a.
    {
        long double aClamped =
            !(a > 3.0f) ? 3.0L : (long double)a;   // includes unordered
        // 30027cb7 FXCH/FSUB/FDIVP: the denominator uses the UNROUNDED endTime
        // retained on the x87 stack by the FST above, not the stored double.
        chunk->lifeRate = (double)(
            (aClamped * (long double)(1.0 / 3.0) * 25.0L) /
            (endTimeFull - (long double)chunk->spawnTime));
    }

    // 30027cc4..30027ced: field_130 = cg_flameTime
    //     + rand()*(1/32768) * (field_50 - field_48) * field_138.
    // FIADD adds the (signed) integer cg_flameTime last.
    // FILD 30027ccd feeds FMUL 30027cd3 with no store, and the chain runs
    // unbroken to the single FSTP double at 30027ced -- no (float)/(double)
    // narrowing of the rand term. (30027cd9 reloads the STORED endTime double.)
    chunk->lifeStartTime = (double)(
        (long double)coduo_crt_rand() *
            (long double)FLAME_DRIP_RAND_NORM *
            ((long double)chunk->endTime -
             (long double)chunk->spawnTime) *
            (long double)chunk->lifeRate +
        (long double)coduo_int32_from_bits(cg_flameTime));

    // 30027cd1: MOV EAX,EBX — return the new chunk.
    return chunk;
}
