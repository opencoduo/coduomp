#include "animation_private.h"

/* Sources: CoDUOMP.exe 0x00494960 and coduo_lnxded 0x080c665c.
 * Both bodies use the complete model index in the byte-offset calculation;
 * the former client uint8_t declaration was narrower than the machine ABI.
 * Name: exact same-version Mac symbol DObjGetMatrixArray. */
DObjSkelMat *DObjGetMatrixArray(const DObj *obj, int32_t modelIndex)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    return obj != NULL && obj->evaluationStorage != NULL &&
                   (uint32_t)modelIndex < obj->modelCount
               ? &obj->evaluationStorage
                      ->partSpans[obj->modelPartBaseIndices[modelIndex]].basePose
               : NULL;
}

/* Sources: CoDUOMP.exe 0x00494d40 and coduo_lnxded 0x080c6b72.
 * Name: exact same-version Mac symbol DObjGetBoneInfo. */
void DObjGetBoneInfo(const DObj *obj, XModelPartColl **partCollisions)
{
    for (int32_t modelIndex = 0; modelIndex < obj->modelCount;
         ++modelIndex) {
        const XModelPartsData *parts =
            obj->models[modelIndex]->info->parts->data.xmodelParts;
        int32_t partCount = XModelNumBones(obj->models[modelIndex]);

        for (int32_t partIndex = 0; partIndex < partCount; ++partIndex) {
            *partCollisions++ =
                &parts->partCollisions[partIndex];
        }
    }
}

/* Sources: CoDUOMP.exe 0x00495160 and coduo_lnxded 0x080c71e6.
 * The Windows body inlines XModelBad's default-model comparison while Linux
 * calls it; the reverse scan and returned qboolean are otherwise identical.
 * Name: exact same-version Mac symbol DObjBad. */
qboolean DObjBad(const DObj *obj)
{
    for (int32_t modelIndex = obj->modelCount - 1; modelIndex >= 0;
         --modelIndex) {
        if (XModelBad(obj->models[modelIndex]) != qfalse) {
            return qtrue;
        }
    }

    return qfalse;
}
