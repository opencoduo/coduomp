// Source: uo_cgame_mp_x86.dll 0x3002e490..0x3002e51c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002e490_3002e51c.mcode
//
// CG_AllocMark — allocate a mark poly (transient decal) from the cgame mark
// pool. Pops a node off the free list cg_freeMarkPolys; if that list is empty,
// reclaims the run of oldest active marks that share the current oldest mark's
// markTime, pushing each back onto the free list, then pops. Zeroes the node and
// appends it at the newest (nextMark) end of the cg_activeMarkPolys circular
// doubly-linked list, then returns it.
//
// Naming: the .mcode size-matched guess "Com_ScriptError" is REJECTED. The
// globals it drives are the mark-poly free list (0x303b5d20 = cg_freeMarkPolys),
// the mark-poly pool (0x303b5d40 = cg_markPolys), and the active-list sentinel
// (0x30412d40 = cg_activeMarkPolys, with its nextMark link at 0x30412d44), all
// set up by CG_InitMarkPolys (0x3002e400) and consumed by CG_ImpactMark
// (0x3002e520, the caller that fills the returned node) and CG_AddMarks
// (0x3002e8c0). The "CG_FreeLocalEntity: not active" string handed to
// Com_ErrorMessage on the reclaim path is a copy-pasted list-integrity assert
// shared with the local-entity free routine (a different pool) and does not name
// this allocator.
//
// ABI: no arguments, returns the node in EAX (RET, caller-cleaned / cdecl).

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <string.h>

markPoly_t *CG_AllocMark(void)
{
    // 0x3002e490: EDX = cg_freeMarkPolys.
    markPoly_t *mark = cg_freeMarkPolys;

    // 0x3002e49a: JNZ alloc — if the free list is non-empty, skip reclamation.
    if (mark == 0) {
        // Free list exhausted: reclaim the oldest active marks. cg_activeMarkPolys
        // is a sentinel node; its prevMark (+0x0) is the oldest active mark. The
        // loop frees every consecutive oldest mark whose markTime (+0x08) equals
        // the first oldest mark's markTime, then pops the last one reclaimed.
        //
        // 0x3002e49c: ESI = cg_activeMarkPolys.prevMark (oldest).
        markPoly_t *oldest = cg_activeMarkPolys.prevMark;
        // 0x3002e4a2: EDI = oldest->markTime — the reference time for the run.
        //             EDI is NOT reloaded across iterations, so the loop keeps
        //             comparing subsequent heads against this first value.
        int32_t groupMarkTime = oldest->markTime;

        for (;;) {
            // 0x3002e4a5: CMP EDI,[ESI+8]; JNZ alloc — stop once the current
            // oldest mark's markTime differs from the run's reference time. (On
            // the first pass ESI == oldest, so this is always equal.)
            if (groupMarkTime != oldest->markTime)
                break;

            // 0x3002e4aa: CMP [ESI],0 — a mark on the active list must have a
            // non-null prevMark. If it is null the list is corrupt: fatal error.
            if (oldest->prevMark == 0) {
                // 0x3002e4af/0x3002e4b4: Com_ErrorMessage("CG_FreeLocalEntity: not active").
                Com_ErrorMessage("CG_FreeLocalEntity: not active");
                // 0x3002e4b9: EDX (mark) = cg_freeMarkPolys reloaded after the call.
                mark = cg_freeMarkPolys;
            }

            // 0x3002e4c2..0x3002e4cf: unlink oldest from the active list.
            //   oldest->prevMark->nextMark = oldest->nextMark;
            //   oldest->nextMark->prevMark = oldest->prevMark;
            oldest->prevMark->nextMark = oldest->nextMark;
            oldest->nextMark->prevMark = oldest->prevMark;

            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            oldest->prevMark = NULL;

            // 0x3002e4d1: push oldest onto the free list (oldest->nextMark = head).
            // 0x3002e4d4/0x3002e4de: cg_freeMarkPolys = oldest; mark = oldest.
            oldest->nextMark = mark;
            mark = oldest;
            cg_freeMarkPolys = mark;

            // 0x3002e4d6: reload the new oldest active mark.
            oldest = cg_activeMarkPolys.prevMark;
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (oldest == NULL || oldest == &cg_activeMarkPolys)
                break;
        }
    }

    // alloc (0x3002e4e6): pop `mark` off the free list.
    // 0x3002e4e6/0x3002e4ec: cg_freeMarkPolys = mark->nextMark.
    cg_freeMarkPolys = mark->nextMark;

    // 0x3002e4f1..0x3002e4fa: zero the whole node (0x5d dwords == 0x174 bytes).
    memset(mark, 0, sizeof(*mark));

    // 0x3002e4fc..0x3002e512: append at the newest (nextMark) end of the active
    // list. The zeroing above ran before these link stores.
    //   mark->nextMark = cg_activeMarkPolys.nextMark;   (0x3002e4fc/0x3002e502)
    //   mark->prevMark = &cg_activeMarkPolys;           (0x3002e504)
    //   cg_activeMarkPolys.nextMark->prevMark = mark;   (0x3002e50a/0x3002e50f)
    //   cg_activeMarkPolys.nextMark = mark;             (0x3002e512)
    mark->nextMark = cg_activeMarkPolys.nextMark;
    mark->prevMark = &cg_activeMarkPolys;
    cg_activeMarkPolys.nextMark->prevMark = mark;
    cg_activeMarkPolys.nextMark = mark;

    // 0x3002e518/0x3002e51b: return the node.
    return mark;
}
