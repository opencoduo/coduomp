// Source: uo_cgame_mp_x86.dll 0x3002aa70..0x3002aaf5
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002aa70_3002aaf5.mcode
//
// CG_AllocLocalEntity — allocate a transient cgame local entity.
//
// Rejected .mcode header name PM_WeaponUseAmmo: that is a pure size guess
// (win size 0x85 == matched size 0x85) and the behavior is unrelated to weapons
// or ammo. The function is proven to be the cgame local-entity allocator:
//   - it pops a node off the cg_freeLocalEntities free list [0x30537d80],
//   - memset()s it to zero (rep stosd, 0x3b=59 dwords = 236 = sizeof(localEntity_t)),
//   - links it at the head of the cg_activeLocalEntities [0x30537da0] active list,
//   - when the free list is empty it recycles the oldest active entity by inlining
//     CG_FreeLocalEntity, which raises the CG_ERROR string
//     "CG_FreeLocalEntity: not active" (global_30077908) via Com_ErrorMessage
//     (0x3002b300) when that entity's ->prev link is null.
// This is the canonical Quake3/CoD CG_AllocLocalEntity; the sibling initializer at
// 0x3002a9e0 (CG_InitLocalEntities) proves the list/free-list layout and the
// 236-byte entity size, matching this function's rep-stosd count.
//
// ABI: __cdecl-ish, no arguments, returns the new localEntity_t * in EAX (RET, no
// stack cleanup). ESI/EDI saved/restored by the prologue/epilogue.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <string.h>

localEntity_t *CG_AllocLocalEntity(void)
{
    localEntity_t *le;

    /* EDX = cg_freeLocalEntities; TEST/JNZ: only recycle when the free list is empty. */
    le = cg_freeLocalEntities;

    if (le == 0) {
        /* Free list exhausted: recycle the oldest active entity, i.e.
         * cg_activeLocalEntities.prev (MOV ESI,[0x30537da0], the .prev field at +0).
         * Inlined CG_FreeLocalEntity(cg_activeLocalEntities.prev). */
        localEntity_t *oldest = cg_activeLocalEntities.prev;

        /* if ( !oldest->prev ) CG_Error("CG_FreeLocalEntity: not active");
         * (CMP dword ptr [ESI],0x0 tests oldest->prev at +0.) */
        if (oldest->prev == 0) {
            Com_ErrorMessage("CG_FreeLocalEntity: not active");
            /* Only the returning error path reloads EDX/freeHead. */
            le = cg_freeLocalEntities;
        }

        /* --- inlined CG_FreeLocalEntity(oldest) --- */
        uint32_t countBits = (uint32_t)cg_numLocalEntities; /* 0x3002aa9a */
        localEntity_t *linkNext = oldest->next;             /* 0x3002aa9f */
        localEntity_t *linkPrev;

        /* cg_numLocalEntities--  (DEC EAX; store) */
        cg_numLocalEntities = coduo_int32_from_bits(countBits - 1u);
        /* Unlink from the active list:
         *   oldest->prev->next = oldest->next;   ([EAX+4]=ECX, EAX=oldest->prev, ECX=oldest->next)
         *   oldest->next->prev = oldest->prev;   ([EAX]=ECX,   EAX=oldest->next, ECX=oldest->prev) */
        linkPrev = oldest->prev;                             /* 0x3002aaa8 */
        linkPrev->next = linkNext;                           /* 0x3002aaaa */
        linkNext = oldest->next;                             /* 0x3002aaad */
        linkPrev = oldest->prev;                             /* 0x3002aab0 */
        linkNext->prev = linkPrev;                           /* 0x3002aab2 */
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        oldest->prev = NULL;
        /* Push onto the free list: oldest->next = cg_freeLocalEntities;
         * cg_freeLocalEntities = oldest.  (le carries the value store below.) */
        oldest->next = le;
        le = oldest;
    }

    /* cg_numLocalEntities++  (INC dword ptr [0x30134cfc]) */
    cg_numLocalEntities = coduo_int32_from_bits((uint32_t)cg_numLocalEntities + 1u);

    /* Pop le off the free list: cg_freeLocalEntities = le->next.
     * (MOV EAX,[EDX+4]; LEA ESI,[EDX+4]; store to [0x30537d80]; ESI holds &le->next.) */
    cg_freeLocalEntities = le->next;

    /* XOR EAX,EAX; MOV ECX,0x3b; MOV EDI,EDX; REP STOSD clears the whole
     * 236-byte PE32 localEntity_t. Clear the complete naturally widened native
     * representation as CG_InitLocalEntities does for the pool. */
    memset(le, 0, sizeof(*le));

    /* Link le at the head of the active list (after the sentinel):
     *   le->next = cg_activeLocalEntities.next;   (MOV ECX,[0x30537da4]; MOV [ESI],ECX; ESI=&le->next)
     *   le->prev = &cg_activeLocalEntities;        (MOV dword ptr [EDX],0x30537da0)
     *   cg_activeLocalEntities.next->prev = le;    (EAX=[0x30537da4]; MOV [EAX],EDX)
     *   cg_activeLocalEntities.next = le;          (MOV [0x30537da4],EDX) */
    le->next = cg_activeLocalEntities.next;
    le->prev = &cg_activeLocalEntities;
    cg_activeLocalEntities.next->prev = le;
    cg_activeLocalEntities.next = le;

    /* MOV EAX,EDX; RET — return the new entity. */
    return le;
}
