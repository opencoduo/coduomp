// Source: uo_cgame_mp_x86.dll 0x3002a9e0..0x3002aa2f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002a9e0_3002aa2f.mcode
//
// CG_InitLocalEntities — reset the cgame local-entity subsystem at cgame startup.
//
// The .mcode header name AngleNormalize180 is REJECTED: it is a pure
// size guess (win size 0x4f ~= matched size 0x50) and the body contains no x87,
// no float constants, and no 360/180 wrap arithmetic — it is not an angle helper.
// The machine code is the canonical Quake3/CoD CG_InitLocalEntities:
//   - zeroes the whole cg_localEntities pool,
//   - resets the active list to an empty circular doubly-linked list (sentinel
//     cg_activeLocalEntities self-linked),
//   - points cg_freeLocalEntities at cg_localEntities[0],
//   - chains cg_localEntities[i].next = &cg_localEntities[i+1] for the free list,
//   - resets cg_numLocalEntities to 0.
// This is proven by the sibling consumers that touch the same globals:
// CG_AllocLocalEntity (0x3002aa70) pops from cg_freeLocalEntities and increments
// cg_numLocalEntities; CG_AddLocalEntities (0x3002ad00) walks cg_activeLocalEntities;
// FUN_3002ac20 builds a refEntity_t inside a localEntity_t. The PPC cgame_mp.dll
// name bank confirms a CG_InitLocalEntities exists in this module.
//
// ABI: no arguments, no return value (RET, no stack cleanup). Only EDI is saved.
// The `MOV EDI,EDI` at 0x3002aa0e is a 2-byte no-op (hot-patch padding), not source.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

#include <string.h>

void CG_InitLocalEntities(void)
{
    int i;

    /* memset(cg_localEntities, 0, sizeof(cg_localEntities)):
     *   XOR EAX,EAX; MOV ECX,0x1d80; MOV EDI,0x30530780; rep stosd.
     * 0x1d80 = 7552 dwords = 30208 bytes = MAX_LOCAL_ENTITIES * sizeof(localEntity_t)
     * (128 * 236 at the target's 4-byte pointer width). sizeof preserves that exact
     * i386 extent and clears the naturally widened pointer fields on native 64-bit
     * builds. */
    memset(cg_localEntities, 0, sizeof(cg_localEntities));

    /* cg_activeLocalEntities.next = cg_activeLocalEntities.prev = &cg_activeLocalEntities:
     *   MOV EAX,0x30537da0
     *   MOV [0x30537da4],EAX   ; .next (+0x4)
     *   MOV [0x30537da0],EAX   ; .prev (+0x0)
     * An empty circular doubly-linked list — both links point at the sentinel. */
    cg_activeLocalEntities.next = &cg_activeLocalEntities;
    cg_activeLocalEntities.prev = &cg_activeLocalEntities;

    /* cg_freeLocalEntities = &cg_localEntities[0]:
     *   MOV dword ptr [0x30537d80],0x30530780  (0x30530780 == &cg_localEntities[0]). */
    cg_freeLocalEntities = &cg_localEntities[0];

    /* Chain the singly-linked free list through ->next:
     *   for ( i = 0 ; i < MAX_LOCAL_ENTITIES - 1 ; i++ )
     *       cg_localEntities[i].next = &cg_localEntities[i + 1];
     *
     * Machine-code form: EAX = 0x30530784 (== &cg_localEntities[0].next, since .next is
     * at +0x4), and each iteration writes [EAX] = EAX + 0xe8 then EAX += 0xec (the entity
     * stride). EAX + 0xe8 == the .next field address minus 4 plus 0xec == the base of the
     * next entity, so cg_localEntities[i].next receives &cg_localEntities[i+1]. The loop
     * continues while EAX < 0x30537c98 (== &cg_localEntities[127].next), i.e. it links
     * entries 0..126 and leaves cg_localEntities[127].next as NULL (already zeroed),
     * terminating the free list. */
    for (i = 0; i < MAX_LOCAL_ENTITIES - 1; ++i) {
        cg_localEntities[i].next = &cg_localEntities[i + 1];
    }

    /* cg_numLocalEntities = 0:  MOV dword ptr [0x30134cfc],0x0. */
    cg_numLocalEntities = 0;
}
