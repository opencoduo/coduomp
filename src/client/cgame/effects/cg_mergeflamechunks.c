// Source: uo_cgame_mp_x86.dll 0x300257e0..0x30025988
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300257e0_30025988.mcode
//
// CG_MergeFlameChunks — fuse two consecutive flame-chunk nodes. f1 is the leading
// chunk (EDI, "into"); f2 is the trailing chunk (ESI, "from"). f2 must immediately
// follow f1 on the forward parent chain, i.e. f1->parent == f2. The routine copies
// f2's render/physics state fields into f1 (so f1 absorbs f2's evolved state), then
// relinks f1's forward chain past f2 and frees f2.
//
// Name: the .mcode header size-guess "CG_MapRestart" (assigned only because win size
// 0x1a8 == matched size 0x1a8) is REJECTED. The function resets no per-map cg state
// and clears no timers/effects; it is pure flameChunk_t field surgery over two nodes
// plus a call to CG_FreeFlameChunk. The error string it emits is the ground truth:
// global_3007773c = "CG_MergeFlameChunks: f2 doesn't follow f1, cannot merge\n"
// (dumped at .rdata 0x3007773c). The PPC bank corroborates: cgame_mp.dll has
// CG_MergeFlameChunks (0x64920) immediately preceding CG_FreeFlameChunk (0x64b20),
// mirroring this binary's 0x300257e0 preceding 0x300256e0.
//
// ABI: f1 arrives in EDI and f2 in ESI (register-passed by the caller; there are no
// stack arguments — the function's PUSH ECX / POP ECX at 0x300257e0 / 0x30025986
// only reserve/discard a scratch slot, and RET has no immediate). Modelled as a
// normal two-parameter C function; the register-only entry is an ABI detail.
//
// float note: the +0xe4 discriminant is a 32-bit float (FLD/FCOMP float ptr); every
// other floating copy is a 64-bit double (FLD/FSTP double ptr, or the double 0.0 at
// 0x3007bcf0). The zero used by the +0x130/+0x140 tests is the .rdata double 0.0 at
// 0x3007bcf0 (exact address dumped: 0x3007bcf0 = 00000000 00000000).

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* .rdata double 0.0 @ 0x3007bcf0, used as the FUCOMPP operand for the +0x130/+0x140
 * "override only when f2 is zero and f1 is nonzero" tests. */
#define FLAME_MERGE_ZERO 0.0

void CG_MergeFlameChunks(flameChunk_t *f1, flameChunk_t *f2)
{
    /* 0x300257e1..07f0: integrity assert. f2 must be f1's forward-chain successor.
     * If f1->parent != f2, emit the merge error (Com_ErrorMessage is the cgame
     * error emitter at 0x3002b300; the format string carries its own newline). */
    if (f1->parent != f2) {                          /* 0x300257e1/e4 */
        Com_ErrorMessage("CG_MergeFlameChunks: f2 doesn't follow f1, cannot merge\n");
    }

    /* 0x300257f3..07ff: splice f1's forward chain past f2 (f1->parent := f2->parent)
     * and clear f2's forward link so the later free does not recurse through it. */
    f1->parent = f2->parent;                         /* 0x300257f3/f6 */
    /* 0x300257f9: preload f2->radius (single) for the discriminant compare below,
     * done here in the machine code before f2->parent is overwritten with 0. */
    f2->parent = NULL;                               /* 0x300257ff: MOV [ESI+8],0 */

    /* 0x30025806..0811: discriminant. Copy f2's state into f1 only when
     * f2->radius > f1->radius (strict, ordered). FCOMP sets C3 (equal) / C0
     * (less); TEST AH,0x41 is nonzero when equal-or-less-or-unordered, and JNZ then
     * skips the whole copy block — so the block runs on the strict-greater case. */
    if (f2->radius > f1->radius) {               /* 0x30025806..0811 */
        /* 0x30025817..08e9: bulk state copy f2 -> f1. Widths follow the machine
         * code exactly: dword MOVs for the uint32_t fields, FLD/FSTP double for the
         * 64-bit fields. Order preserved as emitted. */
        f1->radius = f2->radius;                 /* 0x30025817/1d (dword copy) */
        f1->startSpeedBits = f2->startSpeedBits;                 /* 0x30025823/26 */
        f1->smokeDensityRate = f2->smokeDensityRate;                 /* 0x30025829/2c */
        f1->spawnTimeCopy = f2->spawnTimeCopy;                 /* 0x3002582f/32 (double) */
        f1->spawnTime = f2->spawnTime;                 /* 0x30025835/38 (double) */
        f1->endTime = f2->endTime;                 /* 0x3002583b/3e (double) */
        f1->radiusBaseA = f2->radiusBaseA;                 /* 0x30025841/47 */
        f1->radiusBaseB = f2->radiusBaseB;                 /* 0x3002584d/53 */
        f1->localPos[0] = f2->localPos[0];                 /* 0x30025859/5c */
        f1->localPos[1] = f2->localPos[1];                 /* 0x3002585f/62 */
        f1->localPos[2] = f2->localPos[2];                 /* 0x30025865/68 */
        f1->driftStartTime = f2->driftStartTime;                 /* 0x3002586b/71 (double) */
        f1->worldPos[0] = f2->worldPos[0];                 /* 0x30025877/7d */
        f1->worldPos[1] = f2->worldPos[1];                 /* 0x30025883/89 */
        f1->worldPos[2] = f2->worldPos[2];                 /* 0x3002588f/95 */
        f1->expansionRate = f2->expansionRate;                 /* 0x3002589b/a1 */
        f1->driftDir[0] = f2->driftDir[0];                 /* 0x300258a7/ad */
        f1->driftDir[1] = f2->driftDir[1];                 /* 0x300258b3/b9 */
        f1->driftDir[2] = f2->driftDir[2];                 /* 0x300258bf/c5 */
        f1->driftSpeed = f2->driftSpeed;                 /* 0x300258cb/d1 */
        f1->endTimeCopy3 = f2->endTimeCopy3;                 /* 0x300258d7/dd (double) */
        f1->lifeFraction = f2->lifeFraction;                 /* 0x300258e3/e9 */
    }

    /* 0x300258ef..0900: copy f2->emitScatterIndex into f1->emitScatterIndex only when f2's is
     * nonzero and f1's is still zero (fill an unset slot). */
    if (f2->emitScatterIndex != 0 && f1->emitScatterIndex == 0) {    /* 0x300258ef..08fb */
        f1->emitScatterIndex = f2->emitScatterIndex;                 /* 0x300258fd */
    }

    /* 0x30025900..0936: field_130 fill. TEST AH,0x44 plus JNP skips when
     * f2 compares equal to zero; nonzero or unordered f2 values continue. The
     * second TEST/JP permits the copy only when f1 compares equal to zero. */
    if (f2->lifeStartTime != FLAME_MERGE_ZERO &&
        f1->lifeStartTime == FLAME_MERGE_ZERO) {
        f1->lifeStartTime = f2->lifeStartTime;               /* 0x3002592a/30 (double) */
    }

    /* 0x30025936..096c: field_140 override, identical fill-if-zero rule. */
    if (f2->lifeStartTime2 != FLAME_MERGE_ZERO &&
        f1->lifeStartTime2 == FLAME_MERGE_ZERO) {
        f1->lifeStartTime2 = f2->lifeStartTime2;               /* 0x30025960/66 (double) */
    }

    /* 0x3002596c..097d: same fill-if-unset rule for field_14. */
    if (f2->ownerSentinel != 0 && f1->ownerSentinel == 0) {    /* 0x3002596c..0978 */
        f1->ownerSentinel = f2->ownerSentinel;                 /* 0x3002597a */
    }

    /* 0x3002597d..0986: free the now-absorbed trailing chunk. CALL 0x300256e0 with
     * f2 pushed; ADD ESP,4 is the cdecl cleanup of the single argument. */
    CG_FreeFlameChunk(f2);                            /* 0x3002597d/7e */
}
