// Source: uo_cgame_mp_x86.dll 0x30025600..0x300256d2
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30025600_300256d2.mcode
//
// CG_SpawnFlameChunk — pop one node off the flame-chunk free list
// (cg_freeFlameChunks), push it onto the active list (cg_activeFlameChunks), and
// splice it onto the secondary cg_flameChunkList, inheriting the `parent` chunk's
// place on that list when parent != NULL. Returns the new node, or NULL if the
// free list is exhausted.
//
// The free list built by CG_ClearFlameChunks (0x30025570) is the source of nodes.
// The .mcode name-guess "NormalizeColor" is REJECTED: it was assigned purely by a
// 0xd2==0xd2 win/PPC size match (the .mcode header even says so), and this body
// touches no color/float-normalize math whatsoever — it is pure intrusive
// doubly-linked-list surgery over flameChunk_t nodes and the flame-pool list-head
// globals (cg_freeFlameChunks/cg_activeFlameChunks/cg_flameChunkList/
// cg_numActiveFlameChunks), which is CG_SpawnFlameChunk by call graph and behavior.
//
// ABI note: the sole argument `parent` arrives in ESI (every caller sets ESI just
// before the CALL — XOR ESI,ESI for a root chunk, MOV ESI,<chunk> for a child) and
// the result is returned in EAX. RET (no imm) — caller-cleanup register argument.
// PUSH EDI / PUSH EBX in the body are callee-saved-register spills, not stack args.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

flameChunk_t *CG_SpawnFlameChunk(flameChunk_t *parent)
{
    /* 0x30025600: EAX = cg_freeFlameChunks; 0x30025607..0d: NULL free list -> 0 */
    flameChunk_t *node = cg_freeFlameChunks;
    if (node == 0) {
        return 0;
    }

    /* 0x3002560e..1e: validate the current secondary-list head. EDX is retained
     * when the head is NULL or its listMarker (+0x24) is zero. A non-NULL head
     * with a nonzero marker is discarded before the splice. */
    flameChunk_t *listHead = cg_flameChunkList;
    if (listHead != NULL && listHead->listMarker != 0) {
        listHead = NULL;
    }

    /* 0x30025620..2c: pop `node` off the head of the free list.
     *   cg_freeFlameChunks = node->next; if new head exists, clear its .prev. */
    flameChunk_t *freeNext = node->next;                /* 0x30025620 MOV EDI,[EAX] */
    cg_freeFlameChunks = freeNext;                      /* 0x30025624 */
    if (freeNext != 0) {                                /* 0x30025622/2a */
        freeNext->prev = 0;                             /* 0x3002562c */
    }

    /* 0x3002562f..3b: push `node` onto the head of the active list.
     *   node->next = cg_activeFlameChunks; if old head exists, old->prev = node. */
    flameChunk_t *activeHead = cg_activeFlameChunks;    /* 0x3002562f */
    node->next = activeHead;                            /* 0x30025637 MOV [EAX],EDI */
    if (activeHead != 0) {                              /* 0x30025635/39 */
        activeHead->prev = node;                        /* 0x3002563b */
    }
    /* 0x30025640..75: finish active-list link and reset per-node spawn state. */
    cg_activeFlameChunks = node;                        /* 0x30025640 */
    node->prev = 0;                                     /* 0x30025645 */
    node->liveFlag = 1;                                 /* 0x30025648 */
    node->listMarker = 0;                               /* 0x3002564f */
    node->ownerSentinel = 0;                                 /* 0x30025652 */
    node->emitScatterIndex = 0;                                 /* 0x30025655 */
    node->unresolvedField_1c = 0;                        /* 0x30025658 */
    node->centerOffset[2] = 0;                                /* 0x3002565b */
    node->centerOffset[1] = 0;                                /* 0x30025661 */
    node->centerOffset[0] = 0;                                /* 0x30025667 */
    node->overrideMaterial = 0;                                 /* 0x3002566d */
    node->spawnScale = 1.0f;                             /* 0x30025670 MOV [EAX+0x148],0x3f800000 */

    /* 0x3002567a: the CMP ESI,ECX at 0x3002563e set these flags — if parent == NULL
     * skip the secondary-list unlink and go straight to the head insert. */
    if (parent != 0) {
        if (parent == listHead) {
            /* 0x30025680..8a: parent is the (validated) head; the new node takes
             * its slot, so drop the head and hand its successor to `listHead`. */
            listHead = listHead->listNext;             /* 0x30025680 MOV EDX,[EDX+0xc] */
            if (listHead != 0) {                       /* 0x30025683/85 */
                listHead->listPrev = 0;                /* 0x30025687 */
            }
        } else {
            /* 0x3002568c..a7: unlink parent from the middle of the secondary list.
             *   if (parent->listNext) parent->listNext->listPrev = parent->listPrev;
             *   if (parent->listPrev) parent->listPrev->listNext = parent->listNext; */
            flameChunk_t *pNext = parent->listNext;    /* 0x3002568c */
            if (pNext != 0) {                          /* 0x3002568f/92 */
                pNext->listPrev = parent->listPrev;    /* 0x30025694/97 */
            }
            flameChunk_t *pPrev = parent->listPrev;    /* 0x3002569a */
            if (pPrev != 0) {                          /* 0x3002569d/9f */
                pPrev->listNext = parent->listNext;    /* 0x300256a1/a4 */
            }
        }
        /* 0x300256a8..ab: clear the parent's own secondary-list links. */
        parent->listPrev = 0;                          /* 0x300256a8 */
        parent->listNext = 0;                          /* 0x300256ab */
    }

    /* 0x300256ae..d1: insert `node` at the head of the secondary cg_flameChunkList
     * in front of `listHead` (the surviving successor), then bump the active count
     * and record the parent. */
    if (listHead != 0) {                               /* 0x300256ae/b1 */
        listHead->listPrev = node;                     /* 0x300256b3 */
    }
    node->listPrev = 0;                                /* 0x300256b6 */
    const int32_t nextActiveCount = coduo_int32_from_bits(
        (uint32_t)cg_numActiveFlameChunks + 1u);        /* 0x300256b9/bf */
    node->listNext = listHead;                         /* 0x300256c0 MOV [EAX+0xc],EDX */
    cg_flameChunkList = node;                           /* 0x300256c3 */
    node->parent = parent;                             /* 0x300256c8 MOV [EAX+0x8],ESI */
    cg_numActiveFlameChunks = nextActiveCount;         /* 0x300256cb */

    return node;                                        /* EAX = node */
}
