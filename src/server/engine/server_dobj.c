#include "server_dobj.h"

#include "qcommon/qcommon_runtime_types.h"
#include "animation/xanim.h"
#include "animation/xanim_eval.h"

#include <stddef.h>

extern cvar_t *com_developer;

void Com_Printf(const char *format, ...);
void *Hunk_AllocateTempMemoryInternal(size_t size);

/* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */

/* Source: CoDUOMP.exe 0x0045cee0..0x0045cf20.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045cee0_0045cf21.mcode.
 * Name: exact same-module Mac symbol SV_DObjDumpInfo. */
void SV_DObjDumpInfo(int32_t entityNum)
{
    if (com_developer->integer == 0) {
        return;
    }

    DObj *obj = Com_GetServerDObj(entityNum);
    if (obj == NULL) {
        Com_Printf("no model.\n");
    } else {
        DObjDumpInfo(obj);
    }
}

/* Source: CoDUOMP.exe 0x0045cf30..0x0045cfd7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045cf30_0045cfd8.mcode.
 * Name: exact same-module Mac symbol SV_DObjCreateSkelForBone. */
qboolean SV_DObjCreateSkelForBone(int32_t entityNum, int32_t boneIndex)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (obj == NULL || (uint32_t)boneIndex >= obj->boneCount) {
        return qfalse;
    }

    if (DObjRefreshEvalStorageKey(obj, dobj_skelCacheKey) != qfalse) {
        return DObjSkelIsBoneUpToDate(obj, boneIndex);
    }

    dobj_eval_storage_t *storage =
        Hunk_AllocateTempMemoryInternal((size_t)DObjGetAllocSkelSize(obj));
    DObjCreateSkel(obj, storage);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x0045cfe0..0x0045d06c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045cfe0_0045d06d.mcode.
 * Name: exact same-module Mac symbol SV_DObjCreateSkelForBones. */
qboolean SV_DObjCreateSkelForBones(int32_t entityNum,
                                   const uint32_t *partBits)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    if (obj == NULL) {
        return qfalse;
    }

    if (DObjRefreshEvalStorageKey(obj, dobj_skelCacheKey) != qfalse) {
        return DObjSkelAreBonesUpToDate(obj, partBits);
    }

    dobj_eval_storage_t *storage =
        Hunk_AllocateTempMemoryInternal((size_t)DObjGetAllocSkelSize(obj));
    DObjCreateSkel(obj, storage);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x0045d070..0x0045d09c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045d070_0045d09d.mcode.
 * Name: exact same-module Mac symbol SV_DObjUpdateServerTime. */
qboolean SV_DObjUpdateServerTime(int32_t entityNum, float serverTime,
                                 qboolean notify)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    return obj != NULL ? DObjUpdateServerInfo(obj, serverTime, notify)
                       : qfalse;
}

/* Source: CoDUOMP.exe 0x0045d0a0..0x0045d0dc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045d0a0_0045d0dd.mcode.
 * Name: exact same-module Mac symbol SV_DObjInitServerTime. */
void SV_DObjInitServerTime(int32_t entityNum, float serverTime)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    if (obj != NULL) {
        DObjInitServerTime(obj, serverTime);
    }
}

/* Source: CoDUOMP.exe 0x0045d0e0..0x0045d117.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045d0e0_0045d118.mcode.
 * Name: exact same-module Mac symbol SV_DObjGetHierarchyBits. */
void SV_DObjGetHierarchyBits(int32_t entityNum, int32_t boneIndex,
                             uint32_t *partBits)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    if (obj == NULL || (uint32_t)boneIndex >= obj->boneCount) {
        if (partBits != NULL) {
            for (int32_t wordIndex = 0;
                 wordIndex < DOBJ_PART_BITSET_WORD_COUNT; ++wordIndex) {
                partBits[wordIndex] = 0;
            }
        }
        return;
    }

    DObjGetHierarchyBits(obj, boneIndex, partBits);
}

/* Source: CoDUOMP.exe 0x0045d120..0x0045d143.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045d120_0045d144.mcode.
 * Name: exact same-module Mac symbol SV_DObjCalcAnim. */
void SV_DObjCalcAnim(int32_t entityNum, const uint32_t *partBits)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    if (obj != NULL && obj->evaluationStorage != NULL) {
        DObjCalcAnim(obj, partBits);
    }
}

/* Source: CoDUOMP.exe 0x0045d150..0x0045d181.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045d150_0045d182.mcode.
 * Name: exact same-module Mac symbol SV_DObjCalcSkel. */
void SV_DObjCalcSkel(int32_t entityNum, const uint32_t *partBits)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    if (obj != NULL && obj->evaluationStorage != NULL) {
        DObjCalcSkel(obj, partBits);
    }
}

/* Source: CoDUOMP.exe 0x0045d190..0x0045d1b3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045d190_0045d1b4.mcode.
 * Name: exact same-module Mac symbol SV_DObjNumBones. */
int32_t SV_DObjNumBones(int32_t entityNum)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    return obj != NULL ? DObjNumBones(obj) : 0;
}

/* Source: CoDUOMP.exe 0x0045d1c0..0x0045d202.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045d1c0_0045d203.mcode.
 * Name: exact same-module Mac symbol SV_DObjGetBoneIndex. */
int32_t SV_DObjGetBoneIndex(int32_t entityNum, const char *tagName)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    return obj != NULL ? DObjGetBoneIndex(obj, tagName) : -1;
}

/* Source: CoDUOMP.exe 0x0045d210..0x0045d247.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045d210_0045d248.mcode.
 * Name: exact same-module Mac symbol SV_DObjGetMatrixArray. */
DObjSkelMat *SV_DObjGetMatrixArray(int32_t entityNum)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    return obj != NULL && obj->evaluationStorage != NULL
               ? DObjGetMatrixArray(obj, 0)
               : NULL;
}

/* Source: CoDUOMP.exe 0x0045d250..0x0045d295.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045d250_0045d296.mcode.
 * Name: exact same-module Mac symbol SV_DObjDisplayAnim. */
void SV_DObjDisplayAnim(int32_t entityNum)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    if (obj != NULL) {
        DObjDisplayAnim(obj);
    }
}

/* Source: CoDUOMP.exe 0x0045d2a0..0x0045d2d7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045d2a0_0045d2d8.mcode.
 * Name: exact same-module Mac symbol SV_DObjGetRotTransArray. */
DObjAnimMat *SV_DObjGetRotTransArray(int32_t entityNum)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    return obj != NULL && obj->evaluationStorage != NULL
               ? DObjGetRotTransArray(obj)
               : NULL;
}

/* Source: CoDUOMP.exe 0x0045d2e0..0x0045d332.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045d2e0_0045d333.mcode.
 * Name: exact same-module Mac symbol SV_DObjSetRotTransIndex. */
qboolean SV_DObjSetRotTransIndex(int32_t entityNum,
                                 const uint8_t *partBits,
                                 int32_t boneIndex)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    return obj != NULL && obj->evaluationStorage != NULL &&
                   (uint32_t)boneIndex < obj->boneCount
               ? DObjSetRotTransIndex(obj, partBits, boneIndex)
               : qfalse;
}

/* Source: CoDUOMP.exe 0x0045d340..0x0045d396.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045d340_0045d397.mcode.
 * Name: exact same-module Mac symbol SV_DObjSetControlRotTransIndex. */
qboolean SV_DObjSetControlRotTransIndex(int32_t entityNum,
                                        const uint8_t *partBits,
                                        int32_t boneIndex)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    return obj != NULL && obj->evaluationStorage != NULL &&
                   (uint32_t)boneIndex < obj->boneCount
               ? DObjSetControlRotTransIndex(obj, partBits, boneIndex)
               : qfalse;
}

/* Source: CoDUOMP.exe 0x0045d3a0..0x0045d3e6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045d3a0_0045d3e7.mcode.
 * Name: exact same-module Mac symbol SV_DObjGetBounds. */
void SV_DObjGetBounds(int32_t entityNum, vec3_t mins, vec3_t maxs)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    if (obj == NULL) {
        if (mins != NULL) {
            mins[0] = 0.0f;
            mins[1] = 0.0f;
            mins[2] = 0.0f;
        }
        if (maxs != NULL) {
            maxs[0] = 0.0f;
            maxs[1] = 0.0f;
            maxs[2] = 0.0f;
        }
        return;
    }

    DObjGetBounds(obj, mins, maxs);
}

/* Source: CoDUOMP.exe 0x0045d3f0..0x0045d40f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045d3f0_0045d410.mcode.
 * Name: exact same-module Mac symbol SV_DObjGetTree. */
XAnimTree *SV_DObjGetTree(int32_t entityNum)
{
    DObj *obj = Com_GetServerDObj(entityNum);
    return obj != NULL ? DObjGetTree(obj) : NULL;
}
