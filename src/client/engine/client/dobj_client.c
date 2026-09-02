#include "cgame.h"

#include "../animation/dobj.h"

/* Source: CoDUOMP.exe 0x00401c80..0x00401c84.
 * Name and arguments: exact same-module Mac symbol CL_DObjCalcAnim. MSVC
 * emits the body as a tail-call wrapper and separately inlines it into the
 * cgame syscall dispatcher. */
void CL_DObjCalcAnim(DObj *obj, const uint32_t *partBits)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (obj == NULL || obj->evaluationStorage == NULL || partBits == NULL) {
        return;
    }

    DObjCalcAnim(obj, partBits);
}

/* Source: CoDUOMP.exe 0x00401c50..0x00401c75.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00401c50_00401c76.mcode.
 * Name: exact same-module Mac symbol CL_DObjInvalidateSkels. The unsigned
 * addition preserves the original wrapping INC before zero is skipped. */
void CL_DObjInvalidateSkels(void)
{
    dobj_skelCacheKey =
        (int32_t)((uint32_t)dobj_skelCacheKey + 1u);
    if (dobj_skelCacheKey == 0)
        dobj_skelCacheKey = 1;

    Hunk_ClearTempMemory();
}

/* Source: CoDUOMP.exe 0x00401ca0..0x00401d29.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00401ca0_00401d2a.mcode.
 * Name: exact same-module Mac symbol CL_DObjCreateSkelForBone. */
qboolean CL_DObjCreateSkelForBone(DObj *obj, int32_t boneIndex)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (obj == NULL || (uint32_t)boneIndex >= obj->boneCount) {
        return qfalse;
    }

    if (DObjRefreshEvalStorageKey(
            obj, dobj_skelCacheKey) != qfalse) {
        return DObjSkelIsBoneUpToDate(obj, boneIndex);
    }

    dobj_eval_storage_t *const storage =
        Hunk_AllocateTempMemoryInternal(
            (size_t)DObjGetAllocSkelSize(obj));
    DObjCreateSkel(obj, storage);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00401d30..0x00401d9e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00401d30_00401d9f.mcode.
 * Name: exact same-module Mac symbol CL_DObjCreateSkelForBones. */
qboolean CL_DObjCreateSkelForBones(
    DObj *obj, const uint32_t *partBits)
{
    if (obj == NULL || partBits == NULL) {
        return qfalse;
    }

    if (DObjRefreshEvalStorageKey(
            obj, dobj_skelCacheKey) != qfalse) {
        return DObjSkelAreBonesUpToDate(obj, partBits);
    }

    dobj_eval_storage_t *const storage =
        Hunk_AllocateTempMemoryInternal(
            (size_t)DObjGetAllocSkelSize(obj));
    DObjCreateSkel(obj, storage);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00401da0..0x00401dab.
 * Name and arguments: exact same-module Mac symbol CL_DObjCalcSkel. The
 * Windows register-call wrapper moves obj into EAX and forwards partBits on
 * the stack; the core function has the ordinary portable signature below. */
void CL_DObjCalcSkel(DObj *obj, const uint32_t *partBits)
{
    if (obj == NULL || obj->evaluationStorage == NULL || partBits == NULL) {
        return;
    }

    DObjCalcSkel(obj, partBits);
}
