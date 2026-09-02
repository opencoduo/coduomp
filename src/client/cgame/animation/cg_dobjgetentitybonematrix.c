// Source: uo_cgame_mp_x86.dll 0x3001fda0..0x3001fde2
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001fda0_3001fde2.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
DObjSkelMat *CG_DObjGetEntityBoneMatrix(struct DObj_s *self, const char *tagName, centity_t *part)
{
    /* 3001fda5: EDI = trap(0xb2, self, tagName) -> the named DObj bone/tag index. */
    int32_t boneHandle = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_DOBJ_GET_BONE_INDEX, (intptr_t)self, (intptr_t)tagName));

    /* 3001fdb6: JGE keeps handle >= 0; a negative handle means no such tag / no
     * DObj -> return NULL. */
    if (boneHandle < 0) {
        return NULL;
    }

    /* 3001fdc3: calculate this bone hierarchy and run the entity controllers.
     * The helper takes self in ESI, the bone index in EDI, and the owning entity
     * as its stack argument. */
    CG_DObjCalcBone(self, boneHandle, part);

    /* 3001fdc9: EAX = trap(0xa0, self, 0) -> base of the per-bone matrix table;
     * 3001fdd7/3001fddd: index it by handle*0x40 (SHL 6) to select this bone's
     * 64-byte (4x4 float) matrix. No NULL guard in the machine code. */
    DObjSkelMat *boneMatrixTable = (DObjSkelMat *)(intptr_t)cgame_syscall(CG_DOBJ_GET_BONE_MATRICES, (intptr_t)self, 0);

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (boneMatrixTable == NULL) {
        return NULL;
    }

    /* This is an explicit engine-pointer boundary: the retail ADD is integer
     * address arithmetic. Use the native address carrier so the 64-bit build
     * preserves the valid-base arithmetic without ISO C pointer-type drift. */
    return (DObjSkelMat *)((uintptr_t)boneMatrixTable + ((uint32_t)boneHandle << 6));
}
