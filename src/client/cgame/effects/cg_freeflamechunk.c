// Source: uo_cgame_mp_x86.dll 0x300256e0..0x300257c8
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300256e0_300257c8.mcode
//
// CG_FreeFlameChunk — recursively free one flame-chunk node and its parent chain,
// unlink it from every list it lives on, then push it back onto the flame-chunk
// free list (cg_freeFlameChunks) and decrement cg_numActiveFlameChunks.
//
// This is the inverse of CG_SpawnFlameChunk (0x30025600): a flameChunk_t node is
// simultaneously a member of the active list (next/prev, +0x0/+0x4, headed by
// cg_activeFlameChunks) and of the secondary cg_flameChunkList (listNext/listPrev,
// +0xc/+0x10, headed by cg_flameChunkList 0x300d9750). This routine detaches the
// node from both intrusive doubly-linked lists, then re-initialises the node as a
// clean free-list node.
//
// Name: the .mcode header size-guess name "CG_GetWeapReticleZoom" (assigned purely
// by win size 0xe8 == matched size 0xe8) is REJECTED. This body reads no weapon /
// reticle / zoom state and does no float math; it is pure intrusive linked-list
// surgery over flameChunk_t nodes and the flame-pool list-head globals
// (cg_activeFlameChunks / cg_flameChunkList / cg_freeFlameChunks /
// cg_numActiveFlameChunks) plus a recursive self-call over the parent chain — which
// is CG_FreeFlameChunk by call graph and behaviour. The matching PPC bank entry is
// cgame_mp.dll!CG_FreeFlameChunk.
//
// ABI: the sole argument `chunk` arrives on the stack ([ESP+0x15c] after the frame
// setup + PUSH EBX resolves to the first arg slot). RET with no immediate: cdecl,
// caller-cleaned. PUSH EBX/ESI/EDI in the body are callee-saved-register spills.
//
// The tail of the machine code performs the node re-init as a REP MOVSD copy of the
// whole 0x150-byte node into a stack temp, a REP STOSD zero of the node, then
// selective restores of the five link fields (next/prev/parent/listNext/listPrev)
// from the temp. The net effect is: zero the node except its five link pointers.
// The link fields are then set to their final free-list values (next :=
// cg_freeFlameChunks, prev := 0, parent := 0, listNext := 0, listPrev := 0), so the
// copy/zero/restore dance leaves the node holding exactly those five values with
// every other byte zeroed — modelled here directly as field assignments.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_FreeFlameChunk(flameChunk_t *chunk)
{
    /* 0x300256ee..0704: recursively free the parent chain first, then drop the
     * parent link. CALL 0x300256e0 is a self-call; ADD ESP,4 is the cdecl cleanup
     * of the single pushed argument. */
    if (chunk->parent != NULL) {                 /* 0x300256ee/f4/f7 */
        CG_FreeFlameChunk(chunk->parent);        /* 0x300256f9/fa */
        chunk->parent = NULL;                    /* 0x30025702 */
    }

    /* 0x30025705..0715: unlink from the active/free list (next/prev). The +0x20 and
     * +0x24 stores here are proven writes that are redundant with the full node
     * zero below, but the machine code performs them, so they are represented. */
    flameChunk_t *next = chunk->next;            /* 0x30025705 */
    chunk->liveFlag = 0;                         /* 0x30025709 */
    chunk->listMarker = 0;                       /* 0x3002570c */
    if (next != NULL) {                          /* 0x30025707/0f */
        next->prev = chunk->prev;                /* 0x30025711/14 */
    }
    flameChunk_t *prev = chunk->prev;            /* 0x30025717 */
    if (prev != NULL) {                          /* 0x3002571a/1c */
        prev->next = chunk->next;                /* 0x3002571e/20 */
    }

    /* 0x30025722..0730: if this node headed the active list, advance the head. */
    if (chunk == cg_activeFlameChunks) {         /* 0x30025722/28 */
        cg_activeFlameChunks = chunk->next;      /* 0x3002572a/2c */
    }

    /* 0x30025731..0741: if this node headed the secondary list, advance that head. */
    if (chunk == cg_flameChunkList) {            /* 0x30025731/37 */
        cg_flameChunkList = chunk->listNext;     /* 0x30025739/3c */
    }

    /* 0x30025742..075b: unlink from the secondary list (listNext/listPrev). */
    flameChunk_t *listNext = chunk->listNext;    /* 0x30025742 */
    if (listNext != NULL) {                      /* 0x30025745/47 */
        listNext->listPrev = chunk->listPrev;    /* 0x30025749/4c */
    }
    flameChunk_t *listPrev = chunk->listPrev;    /* 0x3002574f */
    if (listPrev != NULL) {                      /* 0x30025752/54 */
        listPrev->listNext = chunk->listNext;    /* 0x30025756/59 */
    }

    /* 0x3002575c..0773: splice the node onto the head of the free list. Read the
     * current free-list head first; the +0xc/+0x10/+0x4 stores clear the node's
     * remaining links; next is set to the old free-list head. */
    flameChunk_t *freeHead = cg_freeFlameChunks; /* 0x3002575c */
    chunk->listNext = NULL;                      /* 0x30025763 */
    chunk->listPrev = NULL;                      /* 0x30025766 */
    chunk->prev = NULL;                          /* 0x30025769 */
    chunk->next = freeHead;                      /* 0x3002576c */
    if (freeHead != NULL) {                      /* 0x30025761/6e */
        freeHead->prev = chunk;                  /* 0x30025770 */
    }

    /* 0x30025773..07c1: zero the remainder of the node (everything except the five
     * link pointers), publish the free-list head, and drop the active count. In the
     * machine code this is a REP MOVSD save of the node to a stack temp, a REP STOSD
     * zero of the whole node, then restores of next/prev/parent/listNext/listPrev
     * from the temp — the five values just assigned above are preserved, all other
     * fields become 0. Modelled as a whole-node zero followed by re-establishing the
     * five links. */
    flameChunk_t savedLinks = *chunk;            /* 0x30025782 temp[0]=next; +others */
    *chunk = (flameChunk_t){0};                /* 0x3002578d REP STOSD zero */
    chunk->prev = savedLinks.prev;               /* 0x30025797 (temp[1]) */
    chunk->listPrev = savedLinks.listPrev;       /* 0x3002579e (temp[4]) */
    const int32_t activeCount = cg_numActiveFlameChunks; /* 0x300257a1 */
    chunk->next = savedLinks.next;               /* 0x300257a6 */
    const int32_t nextActiveCount = coduo_int32_from_bits((uint32_t)activeCount - 1u); /* 0x300257ad DEC */
    cg_freeFlameChunks = chunk; /* 0x300257af */
    chunk->parent = savedLinks.parent; /* 0x300257b5 (temp[2]) */
    chunk->listNext = savedLinks.listNext; /* 0x300257b8 (temp[3]) */
    cg_numActiveFlameChunks = nextActiveCount; /* 0x300257bb */
}
