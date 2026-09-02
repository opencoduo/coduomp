#include "animation_private.h"

#include <stddef.h>

/* Sources: CoDUOMP.exe 0x00494200 and coduo_lnxded 0x080c5c36. */
qboolean DObjSkelAreBonesUpToDate(const DObj *obj,
                                  const uint32_t *partBits)
{
    const dobj_eval_storage_t *storage = obj->evaluationStorage;

    for (int32_t wordIndex = 0;
         wordIndex < DOBJ_PART_BITSET_WORD_COUNT; ++wordIndex) {
        if ((~storage->blockedPartBits[wordIndex] & partBits[wordIndex]) !=
            0) {
            return qfalse;
        }
    }
    return qtrue;
}

/* Sources: CoDUOMP.exe 0x004941e0 and coduo_lnxded 0x080c5c08. */
qboolean DObjSkelIsBoneUpToDate(const DObj *obj, int32_t boneIndex)
{
    const uint8_t *blockedPartBytes =
        (const uint8_t *)obj->evaluationStorage->blockedPartBits;
    uint8_t partBit = (uint8_t)(1U << (boneIndex & 7));

    return (blockedPartBytes[boneIndex >> 3] & partBit) != 0
               ? qtrue
               : qfalse;
}

/* Sources: CoDUOMP.exe 0x00493b50 and coduo_lnxded 0x080c54da. */
qboolean DObjChildSkipped(const DObj *obj, uint8_t modelIndex)
{
    return (obj->collisionSkipModelMask &
            (1U << (modelIndex & 31))) != 0
               ? qtrue
               : qfalse;
}

/* Sources: CoDUOMP.exe 0x00494c50 and coduo_lnxded 0x080c6a24.
 * Name: exact same-version Mac symbol DObjGetAllocSkelSize. */
int32_t DObjGetAllocSkelSize(const DObj *obj)
{
    return (int32_t)(sizeof(dobj_eval_storage_t) +
                     (size_t)obj->boneCount * sizeof(DObjAnimMat) * 3U);
}

/* Sources: CoDUOMP.exe 0x00494c60 and coduo_lnxded 0x080c6a4a. */
qboolean DObjRefreshEvalStorageKey(DObj *obj, int32_t key)
{
    if (obj->skeletonCacheKey == key) {
        return obj->evaluationStorage != NULL ? qtrue : qfalse;
    }

    obj->skeletonCacheKey = key;
    obj->unknownState10 = 0;
    obj->evaluationStorage = NULL;
    return qfalse;
}

/* Sources: CoDUOMP.exe 0x00494c90 and coduo_lnxded 0x080c6a96.
 * Name: exact same-version Mac symbol DObjSkelExists. */
qboolean DObjSkelExists(const DObj *obj, int32_t key)
{
    return obj->skeletonCacheKey == key && obj->evaluationStorage != NULL
               ? qtrue
               : qfalse;
}

/* Sources: CoDUOMP.exe 0x00494cb0 and coduo_lnxded 0x080c6ac4.
 * Name: exact same-version Mac symbol DObjCreateSkel. */
void DObjCreateSkel(DObj *obj, dobj_eval_storage_t *storage)
{
    obj->evaluationStorage = storage;
    for (int32_t wordIndex = 0;
         wordIndex < DOBJ_PART_BITSET_WORD_COUNT; ++wordIndex) {
        storage->evaluatedPartBits[wordIndex] = 0;
        storage->controlPartBits[wordIndex] = 0;
        storage->blockedPartBits[wordIndex] = 0;
    }
}

/* Sources: CoDUOMP.exe 0x00494ce0 and coduo_lnxded 0x080c6b1a.
 * Name: same-version Mac symbol DObjGetNumModels. */
uint8_t DObjGetNumModels(const DObj *obj)
{
    return obj->modelCount;
}

/* Sources: CoDUOMP.exe 0x00494cf0 and coduo_lnxded 0x080c6b26.
 * Name: same-version Mac symbol DObjGetModel. */
XModel *DObjGetModel(const DObj *obj, int32_t modelIndex)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (obj == NULL || (uint32_t)modelIndex >= obj->modelCount) {
        return NULL;
    }

    return obj->models[modelIndex];
}

/* Sources: CoDUOMP.exe 0x004959f0 and coduo_lnxded 0x080c7d10.
 * Name and signature: exact same-version Mac symbol DObjGetLodForDist. */
int32_t DObjGetLodForDist(const DObj *obj, int32_t modelIndex,
                          float distance)
{
    return XModelGetLodForDist(obj->models[modelIndex], distance);
}

/* Sources: CoDUOMP.exe 0x00495150 and coduo_lnxded 0x080c71dc. */
XAnimTree *DObjGetTree(const DObj *obj)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    return obj != NULL ? obj->runtimeTree : NULL;
}

/* Sources: CoDUOMP.exe 0x00495190 and coduo_lnxded 0x080c7232. */
int32_t DObjNumBones(const DObj *obj)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    return obj != NULL ? obj->boneCount : 0;
}

/* Sources: CoDUOMP.exe 0x00494d30 and coduo_lnxded 0x080c6b5a.
 * Name: exact same-version Mac symbol DObjGetRotTransArray. */
DObjAnimMat *DObjGetRotTransArray(const DObj *obj)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    return obj != NULL && obj->evaluationStorage != NULL
               ? &obj->evaluationStorage
                      ->partSpans[obj->boneCount].evalParts.parts[0]
               : NULL;
}

/* Sources: CoDUOMP.exe 0x00494d00 and coduo_lnxded 0x080c6b36. */
void DObjGetBounds(const DObj *obj, vec3_t mins, vec3_t maxs)
{
    XModelGetBounds(obj->models[0], mins, maxs);
}

/* Sources: CoDUOMP.exe 0x00494da0 and coduo_lnxded 0x080c6bf4.
 * Name: exact same-version Mac symbol DObjSetRotTransIndex. */
qboolean DObjSetRotTransIndex(DObj *obj, const uint8_t *partBits,
                              int32_t boneIndex)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (obj == NULL || obj->evaluationStorage == NULL || partBits == NULL ||
        (uint32_t)boneIndex >= obj->boneCount) {
        return qfalse;
    }

    int32_t byteIndex = boneIndex >> 3;
    uint8_t bit = (uint8_t)(1U << (boneIndex & 7));
    uint8_t *blockedPartBytes =
        (uint8_t *)obj->evaluationStorage->blockedPartBits;

    if ((partBits[byteIndex] & bit) == 0 ||
        (blockedPartBytes[byteIndex] & bit) != 0) {
        return qfalse;
    }

    ((uint8_t *)obj->evaluationStorage->evaluatedPartBits)[byteIndex] |= bit;
    return qtrue;
}

/* Sources: CoDUOMP.exe 0x00494de0 and coduo_lnxded 0x080c6c76.
 * Name: exact same-version Mac symbol DObjSetControlRotTransIndex. */
qboolean DObjSetControlRotTransIndex(DObj *obj,
                                     const uint8_t *partBits,
                                     int32_t boneIndex)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (obj == NULL || obj->evaluationStorage == NULL || partBits == NULL ||
        (uint32_t)boneIndex >= obj->boneCount) {
        return qfalse;
    }

    int32_t byteIndex = boneIndex >> 3;
    uint8_t bit = (uint8_t)(1U << (boneIndex & 7));
    uint8_t *blockedPartBytes =
        (uint8_t *)obj->evaluationStorage->blockedPartBits;

    if ((partBits[byteIndex] & bit) == 0 ||
        (blockedPartBytes[byteIndex] & bit) != 0) {
        return qfalse;
    }

    ((uint8_t *)obj->evaluationStorage->controlPartBits)[byteIndex] |= bit;
    ((uint8_t *)obj->evaluationStorage->evaluatedPartBits)[byteIndex] |= bit;
    return qtrue;
}

/* Sources: CoDUOMP.exe 0x00493cf0 and coduo_lnxded 0x080c557a. */
int32_t DObjFindPartIndex(const DObj *obj, uint16_t partName)
{
    int32_t modelBoneBase = 0;

    for (int32_t modelIndex = 0; modelIndex < obj->modelCount;
         ++modelIndex) {
        const XModel *model = obj->models[modelIndex];
        int32_t modelBoneIndex = XModelGetBoneIndex(model, partName);

        if (modelBoneIndex >= 0) {
            return modelBoneBase + modelBoneIndex;
        }
        modelBoneBase += XModelNumBones(model);
    }
    return DOBJ_PART_INDEX_NOT_FOUND;
}

/* Sources: CoDUOMP.exe 0x004950d0 and coduo_lnxded 0x080c70f0.
 * Name: same-version Mac symbol DObjGetBoneIndex. */
int32_t DObjGetBoneIndex(const DObj *obj, const char *tagName)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (obj == NULL || tagName == NULL) {
        return DOBJ_PART_INDEX_NOT_FOUND;
    }

    uint16_t partName = SL_FindLowercaseString(tagName);

    if (partName == 0) {
        return DOBJ_PART_INDEX_NOT_FOUND;
    }
    return DObjFindPartIndex(obj, partName);
}

/* Sources: CoDUOMP.exe 0x004950f0 and coduo_lnxded 0x080c7136.
 * Name: same-version Mac symbol DObjGetBoneName. SL_ConvertToString returns
 * NULL for the zero handle, making the two original call lowerings equivalent. */
const char *DObjGetBoneName(const DObj *obj, int32_t boneIndex)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (obj == NULL || (uint32_t)boneIndex >= obj->boneCount) {
        return NULL;
    }

    int32_t modelBoneBase = 0;

    for (int32_t modelIndex = 0; modelIndex < obj->modelCount;
         ++modelIndex) {
        const XModel *model = obj->models[modelIndex];
        int32_t modelBoneCount = XModelNumBones(model);
        int32_t localBoneIndex = boneIndex - modelBoneBase;

        if (localBoneIndex < modelBoneCount) {
            uint16_t partName = XModelBoneNames(model)[localBoneIndex];
            return SL_ConvertToString(partName);
        }
        modelBoneBase += modelBoneCount;
    }
    return NULL;
}

/* Sources: CoDUOMP.exe 0x00495a00 and coduo_lnxded 0x080c7d32. */
qboolean DObjHasContents(const DObj *obj, int32_t contentsMask)
{
    for (int32_t modelIndex = 0; modelIndex < obj->modelCount;
         ++modelIndex) {
        if ((XModelGetContents(obj->models[modelIndex]) & contentsMask) !=
            0) {
            return qtrue;
        }
    }
    return qfalse;
}
