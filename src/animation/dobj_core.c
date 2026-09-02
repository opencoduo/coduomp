#include "animation_private.h"
#include "xanim_compat.h"

#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */
#include "math/q_math.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

enum {
    DOBJ_DEFAULT_TRACE_REMAP_PAYLOAD_SIZE = 0x11,
    DOBJ_DEFAULT_TRACE_REMAP_STORAGE_BYTES = 0x14,
    DOBJ_DEFAULT_TRACE_REMAP_STRING_TYPE = 0x0c,
    DOBJ_PART_BIT_INDEX_SHIFT = 3,
    DOBJ_PART_BIT_INDEX_MASK = 7,
    DOBJ_PART_BIT_LOW_MASK = 0x01U,
    DOBJ_TRACE_REMAP_PAIR_STRIDE = 2,
    DOBJ_TRACE_PART_INHERIT_PRIORITY = 1,
    DOBJ_TRACE_PART_MIN_PRIORITY = 2,
    DOBJ_PART_QUAT_LANE_COUNT = 4
};

typedef struct dobj_compat_model_part_layout_s {
    XModelPartsData *payload;
    XModelPartNameTable *partNameTable;
    uint8_t *parentPartDeltas;
    vec3_t *baseTranslations;
} dobj_compat_model_part_layout_t;

/* NOT_FROM_ORIGINAL_SOURCE: resolves the nested XModel part-name payload. */
static dobj_compat_model_part_layout_t dobj_compat_model_part_layout(XModel *model)
{
    XModelInfo *collision;
    fileData_t *partsEntry;
    XModelPartsData *payload;
    XModelPartNameTableSlot *tableSlot;
    dobj_compat_model_part_layout_t layout;

    collision = model->info;
    partsEntry = collision->parts;
    payload = partsEntry->data.xmodelParts;
    tableSlot = payload->partNameTableSlot;

    layout.payload = payload;
    layout.partNameTable = tableSlot->partNameTable;
    layout.parentPartDeltas = tableSlot->parentPartDeltas;
    layout.baseTranslations = payload->baseTranslations;

    return layout;
}

/* NOT_FROM_ORIGINAL_SOURCE: byte-level access to packed DObj part bitsets. */
static uint8_t dobj_compat_part_bit_mask(int32_t partIndex)
{
    return (uint8_t)(1U << (partIndex & DOBJ_PART_BIT_INDEX_MASK));
}

/* NOT_FROM_ORIGINAL_SOURCE: byte-level access to packed DObj part bitsets. */
static qboolean dobj_compat_part_bit_is_set(const void *partBits, int32_t partIndex)
{
    const uint8_t *bytes = (const uint8_t *)partBits;

    return (bytes[partIndex >> DOBJ_PART_BIT_INDEX_SHIFT] & dobj_compat_part_bit_mask(partIndex)) != 0 ? qtrue : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: byte-level access to packed DObj part bitsets. */
static qboolean dobj_compat_part_bit_byte_has_any(const void *partBits, int32_t partIndex)
{
    const uint8_t *bytes = (const uint8_t *)partBits;

    return bytes[partIndex >> DOBJ_PART_BIT_INDEX_SHIFT] != 0 ? qtrue : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: byte-level access to packed DObj part bitsets. */
static void dobj_compat_part_bit_set(uint32_t *partBits, int32_t partIndex)
{
    uint8_t *bytes = (uint8_t *)partBits;
    int32_t byteIndex = partIndex >> DOBJ_PART_BIT_INDEX_SHIFT;

    bytes[byteIndex] = (uint8_t)(bytes[byteIndex] | dobj_compat_part_bit_mask(partIndex));
}

/* NOT_FROM_ORIGINAL_SOURCE: typed copy between eval-part quaternion lanes. */
static void dobj_compat_eval_part_load_quat(const DObjAnimMat *part, vec4_t quat)
{
    quat[0] = part->quat[0];
    quat[1] = part->quat[1];
    quat[2] = part->quat[2];
    quat[3] = part->quat[3];
}

/* NOT_FROM_ORIGINAL_SOURCE: typed copy between eval-part quaternion lanes. */
static void dobj_compat_eval_part_store_quat(DObjAnimMat *part, const vec4_t quat)
{
    part->quat[0] = quat[0];
    part->quat[1] = quat[1];
    part->quat[2] = quat[2];
    part->quat[3] = quat[3];
}

/* NOT_FROM_ORIGINAL_SOURCE: typed copy between eval-part quaternion lanes. */
static void dobj_compat_eval_part_copy_quat(DObjAnimMat *dest, const DObjAnimMat *source)
{
    dest->quat[0] = source->quat[0];
    dest->quat[1] = source->quat[1];
    dest->quat[2] = source->quat[2];
    dest->quat[3] = source->quat[3];
}

/* NOT_FROM_ORIGINAL_SOURCE: typed copy between eval-part translation lanes. */
static void dobj_compat_eval_part_load_translation(const DObjAnimMat *part, vec3_t translation)
{
    translation[0] = part->translation[0];
    translation[1] = part->translation[1];
    translation[2] = part->translation[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: typed copy between eval-part translation lanes. */
static void dobj_compat_eval_part_store_translation(DObjAnimMat *part, const vec3_t translation)
{
    part->translation[0] = translation[0];
    part->translation[1] = translation[1];
    part->translation[2] = translation[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: typed copy between eval-part translation lanes. */
static void dobj_compat_eval_part_copy_translation(DObjAnimMat *dest, const DObjAnimMat *source)
{
    dest->translation[0] = source->translation[0];
    dest->translation[1] = source->translation[1];
    dest->translation[2] = source->translation[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: preserves the original remap-pair scan shape. */
static int32_t dobj_compat_trace_remap_target_for_source(const DObjTracePartRemap *traceRemap, int32_t sourcePart)
{
    const uint8_t *pair = traceRemap->duplicatePairs;

    while (sourcePart != (int32_t)pair[0] - 1) {
        pair += DOBJ_TRACE_REMAP_PAIR_STRIDE;
    }

    return (int32_t)pair[1] - 1;
}

/* NOT_FROM_ORIGINAL_SOURCE: returns a duplicate target and advances on match. */
static qboolean dobj_compat_trace_remap_consume_source(const uint8_t **pairCursor, int32_t sourcePart, int32_t *targetPart)
{
    const uint8_t *pair = *pairCursor;

    if (sourcePart != (int32_t)pair[0] - 1) {
        return qfalse;
    }

    *targetPart = (int32_t)pair[1] - 1;
    *pairCursor = pair + DOBJ_TRACE_REMAP_PAIR_STRIDE;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: routes quaternion multiply through recovered funcs. */
static void dobj_compat_eval_part_multiply_into_self(DObjAnimMat *part, const DObjAnimMat *rhsPart)
{
    vec4_t quat;
    vec4_t rhs;

    dobj_compat_eval_part_load_quat(part, quat);
    dobj_compat_eval_part_load_quat(rhsPart, rhs);
    DObjQuatMultiplyIntoFirst(quat, rhs);
    dobj_compat_eval_part_store_quat(part, quat);
}

/* NOT_FROM_ORIGINAL_SOURCE: routes quaternion multiply through recovered funcs. */
static void dobj_compat_eval_part_multiply_into_rhs(const DObjAnimMat *lhsPart, DObjAnimMat *part)
{
    vec4_t lhs;
    vec4_t quat;

    dobj_compat_eval_part_load_quat(lhsPart, lhs);
    dobj_compat_eval_part_load_quat(part, quat);
    DObjQuatMultiplyIntoSecond(lhs, quat);
    dobj_compat_eval_part_store_quat(part, quat);
}

/* NOT_FROM_ORIGINAL_SOURCE: expands quaternion lanes into a base-pose matrix. */
static void dobj_compat_eval_part_build_matrix(const DObjAnimMat *part, DObjSkelMat *matrix)
{
    vec4_t quat;

    dobj_compat_eval_part_load_quat(part, quat);
    DObjQuatToMatrix43(quat, matrix);
}

/* NOT_FROM_ORIGINAL_SOURCE: transforms eval-part translation in place. */
static void dobj_compat_eval_part_transform_translation(DObjAnimMat *part, const DObjSkelMat *matrix)
{
    vec3_t translation;

    dobj_compat_eval_part_load_translation(part, translation);
    DObjMatrixTransformVector43InPlace(translation, matrix);
    dobj_compat_eval_part_store_translation(part, translation);
}

/* NOT_FROM_ORIGINAL_SOURCE: writes matrix origin from eval translation lanes. */
static void dobj_compat_base_pose_store_origin(DObjSkelMat *matrix, const DObjAnimMat *part)
{
    matrix->origin[0] = part->translation[0];
    matrix->origin[1] = part->translation[1];
    matrix->origin[2] = part->translation[2];
    matrix->origin[3] = 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: applies the model base translation for a child part. */
static void dobj_compat_eval_part_add_base_translation(DObjAnimMat *part, const vec3_t baseTranslation)
{
    part->translation[0] += baseTranslation[0];
    part->translation[1] += baseTranslation[1];
    part->translation[2] += baseTranslation[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: local stack-size calculation for trace remaps. */
static size_t dobj_compat_trace_remap_stack_size(DObj *state)
{
    size_t size;

    size = (size_t)state->boneCount * 2U + DOBJ_PART_REMAP_PREFIX_SIZE + 1U;
    return (size + 15U) & ~15U;
}

uint16_t xanimDefaultPartRemapHandle;

/* Sources: CoDUOMP.exe 0x00493840 and coduo_lnxded 0x080c5338.
 * Name: exact same-version Mac symbol DObjInit. */
void DObjInit(void)
{
    char defaultTraceRemap[DOBJ_DEFAULT_TRACE_REMAP_STORAGE_BYTES];

    Com_Memset(defaultTraceRemap, 0, DOBJ_DEFAULT_TRACE_REMAP_STORAGE_BYTES);
    xanimDefaultPartRemapHandle =
        SL_GetStringOfLen(defaultTraceRemap, 0, DOBJ_DEFAULT_TRACE_REMAP_PAYLOAD_SIZE, DOBJ_DEFAULT_TRACE_REMAP_STRING_TYPE);
}

/* Sources: CoDUOMP.exe 0x00493870 and coduo_lnxded 0x080c5384.
 * Name: exact same-version Mac symbol DObjShutdown. */
void DObjShutdown(void)
{
    if (xanimDefaultPartRemapHandle != 0) {
        SL_RemoveRefToStringOfLen(xanimDefaultPartRemapHandle, DOBJ_DEFAULT_TRACE_REMAP_PAYLOAD_SIZE);
        xanimDefaultPartRemapHandle = 0;
    }
}

/* Sources: CoDUOMP.exe 0x00493d50..0x00493ef2 and coduo_lnxded
 * 0x080c5608..0x080c5832. */
void DObjBuildTracePartRemap(DObj *state)
{
    DObj *record;
    size_t remapStackSize;
    uint32_t remapOffset;
    int32_t partBase;

    record = state;
    remapStackSize = dobj_compat_trace_remap_stack_size(state);
    uint8_t remap[remapStackSize];

    Com_Memset(remap, 0, DOBJ_PART_REMAP_PREFIX_SIZE);

    partBase = XModelNumBones(state->models[0]);
    remapOffset = DOBJ_PART_REMAP_PREFIX_SIZE;

    for (int32_t childIndex = 1; childIndex < state->modelCount; ++childIndex) {
        XModel *model;

        model = state->models[childIndex];
        if (record->modelParentPartIndices[childIndex] == DOBJ_CHILD_PARENT_NONE) {
            const uint16_t *partNames;
            int32_t partNameCount;
            qboolean remappedRootPart;

            partNames = XModelBoneNames(model);
            partNameCount = XModelNumBones(model);
            remappedRootPart = 0;

            for (int32_t localPart = 0; localPart < partNameCount; ++localPart) {
                int32_t remapSource;
                int32_t remapTarget;

                remapSource = localPart + partBase;
                remapTarget = DObjFindPartIndex(state, partNames[localPart]);
                if (remapTarget != remapSource) {
                    if (localPart == 0) {
                        remappedRootPart = 1;
                    }

                    remap[remapOffset] = (uint8_t)(remapSource + 1);
                    remap[remapSource >> 3] = (uint8_t)(remap[remapSource >> 3] | (1U << (remapSource & 7)));
                    ++remapOffset;
                    remap[remapOffset] = (uint8_t)(remapTarget + 1);
                    ++remapOffset;
                }
            }

            if (remappedRootPart == 0) {
                XModel *modelRecord = (XModel *)model;
                XModel *rootModelRecord = (XModel *)state->models[0];

                Com_Printf("WARNING: Attempting to meld model, but root part "
                           "'%s' of model '%s' not found in model '%s' or any "
                           "of its descendants\n",
                           SL_ConvertToString(partNames[0]), modelRecord->name, rootModelRecord->name);
            }
        }

        partBase += XModelNumBones(model);
    }

    if (remapOffset > DOBJ_PART_REMAP_PREFIX_SIZE) {
        remap[remapOffset] = 0;
        ++remapOffset;
        state->tracePartRemapHandle = SL_GetStringOfLen((const char *)remap, 0, remapOffset, DOBJ_DEFAULT_TRACE_REMAP_STRING_TYPE);
    } else {
        state->tracePartRemapHandle = xanimDefaultPartRemapHandle;
    }
}

/* Sources: CoDUOMP.exe 0x00493f00..0x0049406c and coduo_lnxded
 * 0x080c5832..0x080c5a16.  The shared interface is non-const because a
 * missing trace-remap table is built and attached on demand. */
void DObjGetHierarchyBits(DObj *state, int32_t boneIndex, uint32_t *partBits)
{
    DObj *record;
    const DObjTracePartRemap *traceRemap;
    int32_t childBases[DOBJ_MAX_MODELS];
    int32_t childIndex;

    if (partBits == NULL) {
        return;
    }

    for (int32_t wordIndex = 0; wordIndex < DOBJ_PART_BITSET_WORD_COUNT; ++wordIndex) {
        partBits[wordIndex] = 0;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (state == NULL || (uint32_t)boneIndex >= state->boneCount) {
        return;
    }

    record = state;

    if (state->tracePartRemapHandle == 0) {
        DObjBuildTracePartRemap(state);
    }

    traceRemap = (const DObjTracePartRemap *)SL_ConvertToString(state->tracePartRemapHandle);

    /* NOT_FROM_ORIGINAL_SOURCE: model loading and DObj construction validate
     * every bone count before these cumulative child bases are used. */
    childBases[0] = 0;
    childIndex = 0;
    while (childIndex < state->modelCount) {
        XModel *model = state->models[childIndex];
        dobj_compat_model_part_layout_t layout = dobj_compat_model_part_layout(model);
        int32_t childEnd = childBases[childIndex] + layout.partNameTable->count;

        if (boneIndex < childEnd) {
            break;
        }

        ++childIndex;
        if (childIndex == state->modelCount) {
            return;
        }

        childBases[childIndex] = childEnd;
    }

    dobj_compat_model_part_layout_t layout = dobj_compat_model_part_layout(state->models[childIndex]);
    while (qtrue) {
        int32_t localPart = boneIndex - childBases[childIndex];

        while (qtrue) {
            dobj_compat_part_bit_set(partBits, boneIndex);

            if (dobj_compat_part_bit_is_set(traceRemap->sourcePartBits, boneIndex) != qfalse) {
                boneIndex = dobj_compat_trace_remap_target_for_source(traceRemap, boneIndex);
            } else {
                int32_t parentDeltaIndex = localPart - layout.payload->rootPartCount;

                if (parentDeltaIndex >= 0) {
                    boneIndex = (int32_t)(boneIndex - layout.parentPartDeltas[parentDeltaIndex]);
                    break;
                }

                boneIndex = record->modelParentPartIndices[childIndex];
                if (boneIndex == DOBJ_CHILD_PARENT_NONE) {
                    return;
                }
            }

            do {
                --childIndex;
                if (childIndex < 0) {
                    return;
                }
                localPart = boneIndex - childBases[childIndex];
            } while (localPart < 0);

            layout = dobj_compat_model_part_layout(state->models[childIndex]);
        }
    }
}

/* Sources: CoDUOMP.exe 0x00494070..0x004941da and coduo_lnxded
 * 0x080c5a16..0x080c5c08.  Name: same-version Mac symbol
 * DObjCompleteHierarchyBits. */
void DObjCompleteHierarchyBits(DObj *state, uint32_t *partBits)
{
    DObj *record;
    const DObjTracePartRemap *traceRemap;
    int32_t childBases[DOBJ_MAX_MODELS];
    int32_t boneIndex;
    int32_t childIndex;

    record = state;
    boneIndex = state->boneCount - 1;

    if (state->tracePartRemapHandle == 0) {
        DObjBuildTracePartRemap(state);
    }

    traceRemap = (const DObjTracePartRemap *)SL_ConvertToString(state->tracePartRemapHandle);

    /* NOT_FROM_ORIGINAL_SOURCE: model loading and DObj construction validate
     * every authored count feeding these cumulative child bases. */
    childBases[0] = 0;
    childIndex = 0;
    while (qtrue) {
        XModel *model = state->models[childIndex];
        dobj_compat_model_part_layout_t layout = dobj_compat_model_part_layout(model);
        int32_t childEnd = childBases[childIndex] + layout.partNameTable->count;

        if (boneIndex < childEnd) {
            break;
        }

        ++childIndex;
        childBases[childIndex] = childEnd;
    }

    dobj_compat_model_part_layout_t layout = dobj_compat_model_part_layout(state->models[childIndex]);
    while (qtrue) {
        int32_t localPart;

        do {
            localPart = boneIndex - childBases[childIndex];
            if (localPart >= 0) {
                break;
            }

            --childIndex;
            if (childIndex < 0) {
                return;
            }
            layout = dobj_compat_model_part_layout(state->models[childIndex]);
        } while (qtrue);

        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (dobj_compat_part_bit_byte_has_any(partBits, boneIndex) == qfalse &&
            (dobj_compat_part_bit_mask(boneIndex) & DOBJ_PART_BIT_LOW_MASK) != 0) {
            --boneIndex;
            continue;
        }

        int32_t parentPart;
        if (dobj_compat_part_bit_is_set(traceRemap->sourcePartBits, boneIndex) != qfalse) {
            parentPart = dobj_compat_trace_remap_target_for_source(traceRemap, boneIndex);
        } else {
            int32_t parentDeltaIndex = localPart - layout.payload->rootPartCount;

            if (parentDeltaIndex >= 0) {
                parentPart = (int32_t)(boneIndex - layout.parentPartDeltas[parentDeltaIndex]);
            } else {
                parentPart = record->modelParentPartIndices[childIndex];
                if (parentPart == DOBJ_CHILD_PARENT_NONE) {
                    --boneIndex;
                    continue;
                }
            }
        }

        dobj_compat_part_bit_set(partBits, parentPart);
        --boneIndex;
    }
}

/* Sources: CoDUOMP.exe 0x00494240..0x00494952 and coduo_lnxded
 * 0x080c5c96..0x080c665c. */
void DObjCalcSkel(DObj *state, const uint32_t *partBits)
{
    DObj *record = state;
    dobj_eval_storage_t *storage = state->evaluationStorage;
    uint32_t blockedOrNotRequestedBits[DOBJ_PART_BITSET_WORD_COUNT];
    uint32_t buildPartBits[DOBJ_PART_BITSET_WORD_COUNT];
    uint32_t skipQuatBits[DOBJ_PART_BITSET_WORD_COUNT];
    qboolean allPartsAlreadyBlocked = qtrue;

    for (int32_t wordIndex = 0; wordIndex < DOBJ_PART_BITSET_WORD_COUNT; ++wordIndex) {
        blockedOrNotRequestedBits[wordIndex] = ~partBits[wordIndex] | storage->blockedPartBits[wordIndex];
        if (blockedOrNotRequestedBits[wordIndex] != UINT32_MAX) {
            allPartsAlreadyBlocked = qfalse;
        }
    }

    if (allPartsAlreadyBlocked != qfalse) {
        return;
    }

    if (state->tracePartRemapHandle == 0) {
        DObjBuildTracePartRemap(state);
    }

    const DObjTracePartRemap *traceRemap = (const DObjTracePartRemap *)SL_ConvertToString(state->tracePartRemapHandle);

    for (int32_t wordIndex = 0; wordIndex < DOBJ_PART_BITSET_WORD_COUNT; ++wordIndex) {
        uint32_t traceSourcePartBits;

        memcpy(&traceSourcePartBits, &traceRemap->sourcePartBits[(size_t)wordIndex * sizeof(traceSourcePartBits)],
               sizeof(traceSourcePartBits));
        storage->blockedPartBits[wordIndex] |= partBits[wordIndex];
        buildPartBits[wordIndex] = ~blockedOrNotRequestedBits[wordIndex] & storage->controlPartBits[wordIndex];
        skipQuatBits[wordIndex] = blockedOrNotRequestedBits[wordIndex] | buildPartBits[wordIndex] | traceSourcePartBits;
    }

    for (int32_t wordIndex = 0; wordIndex < DOBJ_PART_BITSET_WORD_COUNT; ++wordIndex) {
        buildPartBits[wordIndex] = ~skipQuatBits[wordIndex] | buildPartBits[wordIndex];
    }

    DObjSkelMat *matrices = &storage->partSpans[0].basePose;
    DObjAnimMat *evalParts = DObjGetRotTransArray(state);
    DObjAnimMat *evalPart = evalParts;
    int32_t globalPart = 0;
    const uint8_t *remapPair = traceRemap->duplicatePairs;

    for (int32_t childIndex = 0; childIndex < state->modelCount; ++childIndex) {
        dobj_compat_model_part_layout_t layout = dobj_compat_model_part_layout(state->models[childIndex]);
        uint8_t parentPart = record->modelParentPartIndices[childIndex];

        if (parentPart == DOBJ_CHILD_PARENT_NONE) {
            for (int32_t remaining = layout.payload->rootPartCount; remaining > 0; --remaining) {
                int32_t duplicatePart;

                if (dobj_compat_part_bit_is_set(buildPartBits, globalPart) == qfalse &&
                    dobj_compat_trace_remap_consume_source(&remapPair, globalPart, &duplicatePart) != qfalse &&
                    dobj_compat_part_bit_is_set(blockedOrNotRequestedBits, globalPart) == qfalse) {
                    dobj_compat_eval_part_copy_quat(evalPart, &evalParts[duplicatePart]);
                }

                ++evalPart;
                ++globalPart;
            }
        } else {
            DObjAnimMat *parentEvalPart = &evalParts[parentPart];

            for (int32_t remaining = layout.payload->rootPartCount; remaining > 0; --remaining) {
                if (dobj_compat_part_bit_is_set(skipQuatBits, globalPart) == qfalse) {
                    dobj_compat_eval_part_multiply_into_self(evalPart, parentEvalPart);
                } else if (dobj_compat_part_bit_is_set(buildPartBits, globalPart) != qfalse) {
                    dobj_compat_eval_part_multiply_into_rhs(parentEvalPart, evalPart);
                }

                ++evalPart;
                ++globalPart;
            }
        }

        uint8_t *parentDeltas = layout.parentPartDeltas;
        int32_t nonRootPartCount = layout.partNameTable->count - layout.payload->rootPartCount;

        /* NOT_FROM_ORIGINAL_SOURCE: loading proves this difference
         * nonnegative; retain a positive sink countdown. */
        int32_t localPart = 0;
        while (nonRootPartCount > 0) {
            uint8_t parentDelta = parentDeltas[localPart];
            int32_t duplicatePart;

            if (dobj_compat_part_bit_is_set(skipQuatBits, globalPart) == qfalse) {
                dobj_compat_eval_part_multiply_into_self(evalPart, &evalParts[globalPart - parentDelta]);
            } else if (dobj_compat_part_bit_is_set(buildPartBits, globalPart) == qfalse) {
                if (dobj_compat_trace_remap_consume_source(&remapPair, globalPart, &duplicatePart) != qfalse &&
                    dobj_compat_part_bit_is_set(blockedOrNotRequestedBits, globalPart) == qfalse) {
                    dobj_compat_eval_part_copy_quat(evalPart, &evalParts[duplicatePart]);
                }
            } else {
                dobj_compat_eval_part_multiply_into_rhs(&evalParts[globalPart - parentDelta], evalPart);
            }

            ++evalPart;
            ++globalPart;
            ++localPart;
            --nonRootPartCount;
        }
    }

    evalPart = evalParts;
    DObjSkelMat *matrix = matrices;
    globalPart = 0;
    remapPair = traceRemap->duplicatePairs;

    for (int32_t childIndex = 0; childIndex < state->modelCount; ++childIndex) {
        dobj_compat_model_part_layout_t layout = dobj_compat_model_part_layout(state->models[childIndex]);
        uint8_t parentPart = record->modelParentPartIndices[childIndex];

        if (parentPart == DOBJ_CHILD_PARENT_NONE) {
            for (int32_t remaining = layout.payload->rootPartCount; remaining > 0; --remaining) {
                int32_t duplicatePart;

                if (dobj_compat_part_bit_is_set(buildPartBits, globalPart) == qfalse) {
                    if (dobj_compat_trace_remap_consume_source(&remapPair, globalPart, &duplicatePart) != qfalse &&
                        dobj_compat_part_bit_is_set(blockedOrNotRequestedBits, globalPart) == qfalse) {
                        dobj_compat_eval_part_copy_translation(evalPart, &evalParts[duplicatePart]);
                        *matrix = matrices[duplicatePart];
                    }
                } else {
                    dobj_compat_eval_part_build_matrix(evalPart, matrix);
                    dobj_compat_base_pose_store_origin(matrix, evalPart);
                }

                ++evalPart;
                ++matrix;
                ++globalPart;
            }
        } else {
            DObjSkelMat *parentMatrix = &matrices[parentPart];

            for (int32_t remaining = layout.payload->rootPartCount; remaining > 0; --remaining) {
                if (dobj_compat_part_bit_is_set(buildPartBits, globalPart) != qfalse) {
                    dobj_compat_eval_part_build_matrix(evalPart, matrix);
                    dobj_compat_eval_part_transform_translation(evalPart, parentMatrix);
                    dobj_compat_base_pose_store_origin(matrix, evalPart);
                }

                ++evalPart;
                ++matrix;
                ++globalPart;
            }
        }

        uint8_t *parentDeltas = layout.parentPartDeltas;
        vec3_t *baseTranslations = layout.baseTranslations;
        int32_t nonRootPartCount = layout.partNameTable->count - layout.payload->rootPartCount;

        int32_t localPart = 0;
        while (nonRootPartCount > 0) {
            uint8_t parentDelta = parentDeltas[localPart];
            int32_t duplicatePart;

            if (dobj_compat_part_bit_is_set(buildPartBits, globalPart) == qfalse) {
                if (dobj_compat_trace_remap_consume_source(&remapPair, globalPart, &duplicatePart) != qfalse &&
                    dobj_compat_part_bit_is_set(blockedOrNotRequestedBits, globalPart) == qfalse) {
                    dobj_compat_eval_part_copy_translation(evalPart, &evalParts[duplicatePart]);
                    *matrix = matrices[duplicatePart];
                }
            } else {
                dobj_compat_eval_part_build_matrix(evalPart, matrix);
                dobj_compat_eval_part_add_base_translation(evalPart, baseTranslations[localPart]);
                dobj_compat_eval_part_transform_translation(evalPart, &matrices[globalPart - parentDelta]);
                dobj_compat_base_pose_store_origin(matrix, evalPart);
            }

            ++evalPart;
            ++matrix;
            ++globalPart;
            ++localPart;
            --nonRootPartCount;
        }
    }
}

/* Sources: CoDUOMP.exe 0x00494970..0x00494b98 and coduo_lnxded
 * 0x080c667a..0x080c6962.  Name: exact same-version Mac symbol DObjCreate.
 * Windows computes strlen + 1 at the call site and invokes
 * SL_FindStringOfLen; Linux invokes SL_FindString, whose complete body at
 * 0x080a47e8 computes the same length and forwards to SL_FindStringOfLen.
 * The common spelling below therefore preserves both original paths. */
void DObjCreate(const DObjModel *models, uint16_t modelCount, XAnimTree *runtimeTree, DObj *state, uint16_t gameId)
{
    uint8_t childCount;
    int32_t partCount;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (modelCount > DOBJ_MAX_MODELS) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "dobj has more than %d models",
                  DOBJ_MAX_MODELS);
        modelCount = DOBJ_MAX_MODELS;
    }

    state->runtimeTree = runtimeTree;
    state->evaluationStorage = 0;
    state->skeletonCacheKey = 0;
    state->rootHandle = gameId;
    state->tracePartRemapHandle = 0;
    state->collisionSkipModelMask = 0;

    if (runtimeTree != NULL) {
        XAnim *sourceTree;
        uint8_t *partRemapTableBytes;
        uint8_t *generation;

        sourceTree = runtimeTree->sourceTree;
        partRemapTableBytes = coduo_xanim_part_remap_table_bytes(runtimeTree, runtimeTree->partRemapTableSelector);
        state->partRemapTable = partRemapTableBytes;

        generation = coduo_xanim_part_remap_generation_bytes(partRemapTableBytes, sourceTree->nodeCount);
        ++*generation;
        if (*generation == 0) {
            *generation = 1;
            Com_Memset(generation + 1, 0, sourceTree->nodeCount);
        }

        runtimeTree->partRemapTableSelector = 1 - runtimeTree->partRemapTableSelector;
    } else {
        state->partRemapTable = 0;
    }

    childCount = 0;
    partCount = 0;
    for (int32_t modelIndex = 0; modelIndex < modelCount; ++modelIndex, ++models) {
        XModel *model;
        int32_t modelPartCount;

        model = models->model;
        state->models[childCount] = model;
        state->modelParentPartIndices[childCount] = DOBJ_CHILD_PARENT_NONE;
        state->modelIndices[childCount] = models->modelIndex;
        state->modelPartBaseIndices[childCount] = (uint8_t)partCount;

        if (models->ignoreCollision != 0) {
            state->collisionSkipModelMask |= 1U << modelIndex;
        }

        if (modelIndex != 0 && models->tagName != NULL) {
            const char *tagName;

            tagName = models->tagName;
            if (tagName[0] != '\0') {
                uint16_t tagHandle;

                tagHandle = SL_FindStringOfLen(tagName, strlen(tagName) + 1U);
                if (tagHandle != 0) {
                    for (int32_t parent = 0; parent < childCount; ++parent) {
                        int32_t localPart;

                        localPart = XModelGetBoneIndex(state->models[parent], tagHandle);
                        if (localPart >= 0) {
                            state->modelParentPartIndices[childCount] = (uint8_t)(localPart + state->modelPartBaseIndices[parent]);
                            goto update_part_count;
                        }
                    }
                }

                XModel *rootModelRecord = (XModel *)state->models[0];

                Com_Printf("WARNING: Part '%s' not found in model '%s' or any "
                           "of its descendants\n",
                           tagName, rootModelRecord->name);
            }
        }

    update_part_count:
        /* NOT_FROM_ORIGINAL_SOURCE: each model contributes a positive count to
         * the bounded cumulative DObj span. */
        modelPartCount = XModelNumBones(model);
        if (modelPartCount <= 0) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "dobj xmodel '%s' has an invalid bone count",
                      model->name);
            goto finish;
        }
        partCount += modelPartCount;
        if (partCount > DOBJ_MAX_BONE_INDEX) {
            XModel *rootModelRecord = (XModel *)state->models[0];

            Com_Error(ERR_DROP,
                      "\x15"
                      "dobj for xmodel '%s' has more than %d bones",
                      rootModelRecord->name, DOBJ_MAX_BONE_INDEX);
            goto finish;
        }

        ++childCount;
    }

finish:
    state->modelCount = childCount;
    state->boneCount = (uint8_t)partCount;
}

/* Sources: CoDUOMP.exe 0x00494ba0..0x00494c40 and coduo_lnxded
 * 0x080c6962..0x080c6a24.  Name: exact same-version Mac symbol DObjFree. */
void DObjFree(DObj *state, qboolean releaseRuntimeTree)
{
    XAnimTree *runtimeTree;

    runtimeTree = state->runtimeTree;
    if (runtimeTree != NULL) {
        if (releaseRuntimeTree != 0) {
            XAnimClearTree(runtimeTree);
        }

        if (state->partRemapTable == coduo_xanim_part_remap_table_bytes(runtimeTree, 0)) {
            runtimeTree->partRemapTableSelector = 0;
        } else {
            runtimeTree->partRemapTableSelector = 1;
        }

        state->partRemapTable = 0;
        state->runtimeTree = 0;
    }

    if (state->tracePartRemapHandle != 0) {
        if (state->tracePartRemapHandle != xanimDefaultPartRemapHandle) {
            const char *payload;

            payload = SL_ConvertToString(state->tracePartRemapHandle);
            SL_RemoveRefToStringOfLen(state->tracePartRemapHandle,
                                      (uint32_t)(strlen(payload + DOBJ_PART_REMAP_PREFIX_SIZE) + DOBJ_PART_REMAP_PREFIX_SIZE + 1U));
        }

        state->tracePartRemapHandle = 0;
    }
}

/* DObjTraceParts has a genuine floating-point source split.  The
 * Windows body retains several PC=53 x87 values across binary32 stores;
 * the Linux body uses different spill points and fold order under PC=64.
 * Keep the complete authoritative body selected by the existing target
 * behavior mode rather than interleaving arithmetic-level conditions. */
#if defined(WINDOWS_BEHAVIOR)
/* Source: CoDUOMP.exe 0x00495240..0x0049590c.
 * Name and collision flow match the machine-audited Linux engine function. */
void DObjTraceParts(const DObj *obj, const vec3_t start, const vec3_t end, const uint8_t *partState, dobj_trace_result_t *trace)
{
    vec3_t delta;
    delta[0] = end[0] - start[0];
    delta[1] = end[1] - start[1];
    /* 0x00495297 FST keeps the Z subtraction live for its square; X and Y
     * have already been rounded to their float stack slots. */
    const long double deltaZRaw = (long double)end[2] - (long double)start[2];
    delta[2] = (float)deltaZRaw;
    const long double lengthSquared =
        deltaZRaw * deltaZRaw + (long double)delta[1] * (long double)delta[1] + (long double)delta[0] * (long double)delta[0];
    float invLenSq = (float)((long double)1.0f / lengthSquared);
    DObjSkelMat *basePose = DObjGetMatrixArray(obj, 0);
    /* NOT_FROM_ORIGINAL_SOURCE: DObj construction proves the cumulative child
     * count fits this fixed remapping table before tracing can index it. */
    uint16_t remappedPartStateIndices[DOBJ_MAX_BONES];

    trace->surfaceFlags = 0;
    trace->startsolid = 0;
    trace->allsolid = 0;
    trace->hitPartNameHandle = 0;
    trace->hitPartStateIndex = 0;
    trace->normal[2] = 0.0f;
    trace->normal[1] = 0.0f;
    trace->normal[0] = 0.0f;

    uint32_t bestPriority = DOBJ_TRACE_PART_MIN_PRIORITY;
    const uint8_t *traceRemap = (const uint8_t *)SL_ConvertToString(obj->tracePartRemapHandle) + DOBJ_PART_REMAP_PREFIX_SIZE;
    int32_t globalPart = 0;

    for (int32_t modelIndex = 0; modelIndex < obj->modelCount; ++modelIndex) {
        dobj_compat_model_part_layout_t layout = dobj_compat_model_part_layout(obj->models[modelIndex]);
        const uint16_t *partNames = layout.partNameTable->handles;
        int32_t partCount = layout.partNameTable->count;
        const XModelPartColl *parts = layout.payload->partCollisions;
        const uint8_t *partStateMap = layout.payload->partStateIndices;
        int32_t rootPartCount = layout.payload->rootPartCount;
        uint32_t modelSkipped = (1U << (modelIndex & 31)) & obj->collisionSkipModelMask;

        for (int32_t localPart = 0; localPart < partCount; ++localPart, ++globalPart, ++basePose) {
            uint16_t partStateIndex = partStateMap[localPart];
            uint32_t partPriority = partState[partStateIndex];

            if (globalPart == (int32_t)traceRemap[0] - 1) {
                const uint8_t *remapEntry = traceRemap;
                traceRemap += 2;
                if (partPriority == DOBJ_TRACE_PART_INHERIT_PRIORITY) {
                    partStateIndex = remappedPartStateIndices[remapEntry[1] - 1];
                    partPriority = partState[partStateIndex];
                }
            } else if (partPriority == DOBJ_TRACE_PART_INHERIT_PRIORITY) {
                if (localPart < rootPartCount) {
                    uint8_t parentPart = obj->modelParentPartIndices[modelIndex];
                    partStateIndex = parentPart == DOBJ_CHILD_PARENT_NONE ? 0 : remappedPartStateIndices[parentPart];
                } else {
                    uint8_t parentBack = layout.parentPartDeltas[localPart - rootPartCount];
                    partStateIndex = remappedPartStateIndices[globalPart - parentBack];
                }
                partPriority = partState[partStateIndex];
            }

            remappedPartStateIndices[globalPart] = partStateIndex;
            if (modelSkipped != 0) {
                continue;
            }

            const XModelPartColl *part = &parts[localPart];
            if (part->radiusSq == 0.0f || bestPriority > partPriority) {
                continue;
            }

            vec3_t center;
            vec3_t startToCenter;
            DObjMatrixTransformVector43(part->center, basePose, center);
            startToCenter[0] = start[0] - center[0];
            startToCenter[1] = start[1] - center[1];
            /* 0x004954e1 retains the unrounded Z subtraction through the dot;
             * its float copy is reloaded only by the later nearest-point path. */
            const long double startToCenterZRaw = (long double)start[2] - (long double)center[2];
            startToCenter[2] = (float)startToCenterZRaw;
            const long double projectionRaw =
                -((startToCenterZRaw * (long double)delta[2] + (long double)startToCenter[1] * (long double)delta[1]) +
                  (long double)startToCenter[0] * (long double)delta[0]) *
                (long double)invLenSq;
            float projection = (float)projectionRaw;
            float distanceSq;

            /* 0x00495503 FST stores projection, while both bounds compare the
             * retained x87 result. */
            if (projectionRaw < (long double)1.0f) {
                if (projectionRaw > (long double)0.0f) {
                    vec3_t nearest = {startToCenter[0] + delta[0] * projection, startToCenter[1] + delta[1] * projection,
                                      startToCenter[2] + delta[2] * projection};
                    distanceSq = nearest[2] * nearest[2] + nearest[1] * nearest[1] + nearest[0] * nearest[0];
                } else {
                    distanceSq =
                        startToCenter[2] * startToCenter[2] + startToCenter[1] * startToCenter[1] + startToCenter[0] * startToCenter[0];
                }
            } else {
                vec3_t endToCenter = {end[0] - center[0], end[1] - center[1], end[2] - center[2]};
                distanceSq = endToCenter[2] * endToCenter[2] + endToCenter[1] * endToCenter[1] + endToCenter[0] * endToCenter[0];
            }

            float radiusDelta = part->radiusSq - distanceSq;
            if (radiusDelta <= 0.0f) {
                continue;
            }
            if (bestPriority == partPriority) {
                const long double sphereFractionRaw = (long double)projection - sqrtl((long double)radiusDelta * (long double)invLenSq);
                if (sphereFractionRaw >= (long double)trace->fraction) {
                    continue;
                }
            }

            vec3_t localStart;
            vec3_t localEnd;
            DObjMatrixInverseTransformVector43(start, basePose, localStart);
            DObjMatrixInverseTransformVector43(end, basePose, localEnd);
            float enter = 0.0f;
            float exit = trace->fraction;
            qboolean startInside = qtrue;
            qboolean endInside = qtrue;
            float hitSign = -1.0f;
            int32_t hitAxis = -1;
            float sign = -1.0f;
            const float *bounds = part->mins;

            for (;;) {
                for (int32_t axis = 0; axis < 3; ++axis) {
                    float startDist = (float)(((long double)localStart[axis] - (long double)bounds[axis]) * (long double)sign);
                    /* startDist is FSTP-rounded at 0x004956ef; endDist remains
                     * live through both zero tests and the subtraction. */
                    const long double endDistRaw = ((long double)localEnd[axis] - (long double)bounds[axis]) * (long double)sign;

                    if (startDist > 0.0f) {
                        if (endDistRaw > (long double)0.0f) {
                            goto next_part;
                        }
                        startInside = qfalse;
                        const long double denom = (long double)startDist - endDistRaw;
                        if ((long double)enter * denom < (long double)startDist) {
                            const long double enterCandidate = (long double)startDist / denom;
                            /* 0x00495742 FST rounds the saved candidate but
                             * leaves the wide quotient for this comparison. */
                            enter = (float)enterCandidate;
                            if ((long double)exit <= enterCandidate) {
                                goto next_part;
                            }
                            hitSign = sign;
                            hitAxis = axis;
                        }
                    } else if (endDistRaw > (long double)0.0f) {
                        endInside = qfalse;
                        const long double denom = (long double)startDist - endDistRaw;
                        if ((long double)exit * denom < (long double)startDist) {
                            exit = (float)((long double)startDist / denom);
                            if (exit <= enter) {
                                goto next_part;
                            }
                        }
                    }
                }

                if (sign == 1.0f) {
                    break;
                }
                sign = 1.0f;
                bounds = part->maxs;
            }

            if (!startInside) {
                if (bestPriority == partPriority) {
                    if (trace->fraction <= enter) {
                        goto next_part;
                    }
                } else {
                    bestPriority = partPriority;
                }

                trace->fraction = enter;
                trace->hitPartNameHandle = partNames[localPart];
                trace->hitPartStateIndex = partStateIndex;
                trace->normal[0] = basePose->axis[hitAxis][0] * hitSign;
                trace->normal[1] = basePose->axis[hitAxis][1] * hitSign;
                trace->normal[2] = basePose->axis[hitAxis][2] * hitSign;
            } else {
                trace->startsolid = qtrue;
                if (endInside) {
                    trace->allsolid = qtrue;
                    trace->fraction = 0.0f;
                    trace->hitPartNameHandle = partNames[localPart];
                    trace->hitPartStateIndex = partStateIndex;
                    trace->normal[2] = 0.0f;
                    trace->normal[1] = 0.0f;
                    trace->normal[0] = 0.0f;
                    return;
                }
            }

        next_part:;
        }
    }
}
#else
/* Source: coduo_lnxded 0x080c7394..0x080c7be8. */
void DObjTraceParts(const DObj *state, const vec3_t start, const vec3_t end, const uint8_t *partState, dobj_trace_result_t *trace)
{
    const DObj *record;
    DObjSkelMat *basePose;
    const uint8_t *traceRemap;
    /* NOT_FROM_ORIGINAL_SOURCE: DObj construction proves the cumulative child
     * count fits this fixed remapping table before tracing can index it. */
    uint16_t remappedPartStateIndices[DOBJ_MAX_BONES];
    vec3_t delta;
    float invLenSq;
    uint32_t bestPriority;
    int32_t globalPart;

    record = state;
    delta[0] = end[0] - start[0];
    delta[1] = end[1] - start[1];
    delta[2] = end[2] - start[2];
    /* 1.0 / (delta.delta) kept 80-bit, one store -> shim. */
#if EMULATE_X87
    invLenSq = x87f_store_f32(x87f_div(x87f_load_f32(1.0f), x87f_add(x87f_add(x87f_mul(x87f_load_f32(delta[0]), x87f_load_f32(delta[0])),
                                                                              x87f_mul(x87f_load_f32(delta[1]), x87f_load_f32(delta[1]))),
                                                                     x87f_mul(x87f_load_f32(delta[2]), x87f_load_f32(delta[2])))));
#else
    invLenSq = 1.0f / (delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
#endif
    basePose = DObjGetMatrixArray(state, 0);

    trace->surfaceFlags = 0;
    trace->startsolid = 0;
    trace->allsolid = 0;
    trace->hitPartNameHandle = 0;
    trace->hitPartStateIndex = 0;
    trace->normal[2] = 0.0f;
    trace->normal[1] = 0.0f;
    trace->normal[0] = 0.0f;

    bestPriority = DOBJ_TRACE_PART_MIN_PRIORITY;
    traceRemap = (const uint8_t *)SL_ConvertToString(state->tracePartRemapHandle) + DOBJ_PART_REMAP_PREFIX_SIZE;
    globalPart = 0;

    for (int32_t childIndex = 0; childIndex < state->modelCount; ++childIndex) {
        XModel *model;
        dobj_compat_model_part_layout_t layout;
        const uint16_t *partNames;
        XModelPartColl *parts;
        const uint8_t *partStateMap;
        int32_t partNameCount;
        int32_t parentThreshold;
        uint32_t childSkipped;

        model = state->models[childIndex];
        layout = dobj_compat_model_part_layout(model);
        /*
         * Original DObjTraceParts reads the i386 xmodelparts payload at
         * +0x00/+0x04/+0x08/+0x14. Resolve those semantic fields through the
         * host payload so widened pointers do not shift collision metadata.
         */
        partNames = layout.partNameTable->handles;
        partNameCount = layout.partNameTable->count;
        parts = layout.payload->partCollisions;
        partStateMap = layout.payload->partStateIndices;
        parentThreshold = layout.payload->rootPartCount;
        childSkipped = (1U << (childIndex & 31)) & state->collisionSkipModelMask;

        for (int32_t localPart = 0; localPart < partNameCount; ++localPart, ++globalPart, ++basePose) {
            uint16_t partStateIndex;
            uint32_t partPriority;

            partStateIndex = partStateMap[localPart];
            partPriority = partState[partStateIndex];

            if (globalPart == (int32_t)traceRemap[0] - 1) {
                const uint8_t *remapEntry;

                remapEntry = traceRemap;
                traceRemap += 2;
                if (partPriority == DOBJ_TRACE_PART_INHERIT_PRIORITY) {
                    partStateIndex = remappedPartStateIndices[remapEntry[1] - 1];
                    partPriority = partState[partStateIndex];
                }
            } else if (partPriority == DOBJ_TRACE_PART_INHERIT_PRIORITY) {
                if (localPart < parentThreshold) {
                    uint8_t parentPart;

                    parentPart = record->modelParentPartIndices[childIndex];
                    if (parentPart == DOBJ_CHILD_PARENT_NONE) {
                        partStateIndex = 0;
                    } else {
                        partStateIndex = remappedPartStateIndices[parentPart];
                    }
                } else {
                    uint8_t parentBack;

                    parentBack = layout.parentPartDeltas[localPart - parentThreshold];
                    partStateIndex = remappedPartStateIndices[globalPart - parentBack];
                }
                partPriority = partState[partStateIndex];
            }

            remappedPartStateIndices[globalPart] = partStateIndex;
            if (childSkipped != 0) {
                continue;
            }

            XModelPartColl *part = &parts[localPart];
            if (part->radiusSq == 0.0f || bestPriority > partPriority) {
                continue;
            }

            vec3_t center;
            vec3_t startToCenter;
            float projection;
            float distanceSq;
            float radiusDelta;

            DObjMatrixTransformVector43(part->center, basePose, center);
            startToCenter[0] = start[0] - center[0];
            startToCenter[1] = start[1] - center[1];
            startToCenter[2] = start[2] - center[2];
            /* -(startToCenter.delta) * invLenSq kept 80-bit, one store -> shim. */
#if EMULATE_X87
            projection =
                x87f_store_f32(x87f_mul(x87f_neg(x87f_add(x87f_add(x87f_mul(x87f_load_f32(startToCenter[0]), x87f_load_f32(delta[0])),
                                                                   x87f_mul(x87f_load_f32(startToCenter[1]), x87f_load_f32(delta[1]))),
                                                          x87f_mul(x87f_load_f32(startToCenter[2]), x87f_load_f32(delta[2])))),
                                        x87f_load_f32(invLenSq)));
#else
            projection = -(startToCenter[0] * delta[0] + startToCenter[1] * delta[1] + startToCenter[2] * delta[2]) * invLenSq;
#endif

            if (projection < 1.0f) {
                if (projection > 0.0f) {
                    vec3_t nearest;

                    /* nearest[k] = startToCenter[k] + delta[k]*projection MA;
                     * nearest.nearest dot -> shim. */
#if EMULATE_X87
                    for (int32_t k = 0; k < 3; k++) {
                        nearest[k] = x87f_store_f32(
                            x87f_add(x87f_mul(x87f_load_f32(delta[k]), x87f_load_f32(projection)), x87f_load_f32(startToCenter[k])));
                    }
                    distanceSq = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(nearest[0]), x87f_load_f32(nearest[0])),
                                                                  x87f_mul(x87f_load_f32(nearest[1]), x87f_load_f32(nearest[1]))),
                                                         x87f_mul(x87f_load_f32(nearest[2]), x87f_load_f32(nearest[2]))));
#else
                    nearest[0] = startToCenter[0] + delta[0] * projection;
                    nearest[1] = startToCenter[1] + delta[1] * projection;
                    nearest[2] = startToCenter[2] + delta[2] * projection;
                    distanceSq = nearest[0] * nearest[0] + nearest[1] * nearest[1] + nearest[2] * nearest[2];
#endif
                } else {
#if EMULATE_X87
                    distanceSq =
                        x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(startToCenter[0]), x87f_load_f32(startToCenter[0])),
                                                         x87f_mul(x87f_load_f32(startToCenter[1]), x87f_load_f32(startToCenter[1]))),
                                                x87f_mul(x87f_load_f32(startToCenter[2]), x87f_load_f32(startToCenter[2]))));
#else
                    distanceSq =
                        startToCenter[0] * startToCenter[0] + startToCenter[1] * startToCenter[1] + startToCenter[2] * startToCenter[2];
#endif
                }
            } else {
                vec3_t endToCenter;

                endToCenter[0] = end[0] - center[0];
                endToCenter[1] = end[1] - center[1];
                endToCenter[2] = end[2] - center[2];
#if EMULATE_X87
                distanceSq = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(endToCenter[0]), x87f_load_f32(endToCenter[0])),
                                                              x87f_mul(x87f_load_f32(endToCenter[1]), x87f_load_f32(endToCenter[1]))),
                                                     x87f_mul(x87f_load_f32(endToCenter[2]), x87f_load_f32(endToCenter[2]))));
#else
                distanceSq = endToCenter[0] * endToCenter[0] + endToCenter[1] * endToCenter[1] + endToCenter[2] * endToCenter[2];
#endif
            }

            radiusDelta = part->radiusSq - distanceSq;
            if (radiusDelta <= 0.0f) {
                continue;
            }

            if (bestPriority == partPriority) {
                /*
                 * 0x080c783b..0x080c784c: only the sqrt result is rounded to
                 * float; the projection - sqrt difference stays in an x87
                 * register for the compare, so it must not pass through a
                 * float local here.
                 */
#if EMULATE_X87
                if (x87f_le(x87f_load_f32(trace->fraction),
                            x87f_sub(x87f_load_f32(projection), x87f_load_f32((float)sqrt((double)(radiusDelta * invLenSq)))))) {
                    continue;
                }
#else
                if (projection - (float)sqrt((double)(radiusDelta * invLenSq)) >= trace->fraction) {
                    continue;
                }
#endif
            }

            vec3_t localStart;
            vec3_t localEnd;
            float enter;
            float exit;
            qboolean startInside;
            qboolean endInside;
            float hitSign;
            int32_t hitAxis;
            float sign;
            const float *bounds;

            DObjMatrixInverseTransformVector43(start, basePose, localStart);
            DObjMatrixInverseTransformVector43(end, basePose, localEnd);
            enter = 0.0f;
            exit = trace->fraction;
            startInside = 1;
            endInside = 1;
            hitSign = -1.0f;
            hitAxis = -1;
            sign = -1.0f;
            bounds = part->mins;

            while (1) {
                for (int32_t axis = 0; axis < 3; ++axis) {
                    float startDist;
                    float endDist;

                    startDist = (localStart[axis] - bounds[axis]) * sign;
                    endDist = (localEnd[axis] - bounds[axis]) * sign;
                    if (startDist > 0.0f) {
                        float denom;

                        if (endDist > 0.0f) {
                            goto next_part;
                        }
                        startInside = 0;
                        denom = startDist - endDist;
                        if (enter * denom < startDist) {
                            enter = startDist / denom;
                            if (exit <= enter) {
                                goto next_part;
                            }
                            hitSign = sign;
                            hitAxis = axis;
                        }
                    } else if (endDist > 0.0f) {
                        float denom;

                        endInside = 0;
                        denom = startDist - endDist;
                        if (exit * denom < startDist) {
                            exit = startDist / denom;
                            if (exit <= enter) {
                                goto next_part;
                            }
                        }
                    }
                }

                if (sign == 1.0f) {
                    break;
                }
                sign = 1.0f;
                bounds = part->maxs;
            }

            if (startInside == 0) {
                if (bestPriority == partPriority) {
                    if (trace->fraction <= enter) {
                        goto next_part;
                    }
                } else {
                    bestPriority = partPriority;
                }

                trace->fraction = enter;
                trace->hitPartNameHandle = partNames[localPart];
                trace->hitPartStateIndex = partStateIndex;
                trace->normal[0] = basePose->axis[hitAxis][0] * hitSign;
                trace->normal[1] = basePose->axis[hitAxis][1] * hitSign;
                trace->normal[2] = basePose->axis[hitAxis][2] * hitSign;
            } else {
                trace->startsolid = 1;
                if (endInside != 0) {
                    trace->allsolid = 1;
                    trace->fraction = 0.0f;
                    trace->hitPartNameHandle = partNames[localPart];
                    trace->hitPartStateIndex = partStateIndex;
                    trace->normal[2] = 0.0f;
                    trace->normal[1] = 0.0f;
                    trace->normal[0] = 0.0f;
                    return;
                }
            }

        next_part:;
        }
    }
}

#endif

/* Sources: CoDUOMP.exe 0x00495910..0x004959ec and coduo_lnxded
 * 0x080c7be8..0x080c7d10. */
void DObjTraceModelParts(const DObj *state, const vec3_t start, const vec3_t end, int32_t contentMask, dobj_trace_result_t *trace)
{
    DObjSkelMat *basePose;
    trace_t traceState;

    basePose = DObjGetMatrixArray(state, 0);
    trace->hitPartNameHandle = 0;
    trace->hitPartStateIndex = 0;
    trace->startsolid = 0;
    trace->allsolid = 0;

    traceState.fraction = trace->fraction;
    traceState.normal[0] = 0.0f;
    traceState.normal[1] = 0.0f;
    traceState.normal[2] = 0.0f;
    traceState.surfaceFlags = 0;

    for (int32_t childIndex = 0; childIndex < state->modelCount; ++childIndex) {
        XModel *model;
        const uint16_t *partNames;
        int32_t hitPart;

        model = state->models[childIndex];
        partNames = XModelBoneNames(model);
        hitPart = XModelTraceLine(model, &traceState, basePose, start, end, contentMask);
        if (hitPart >= 0) {
            trace->hitPartNameHandle = partNames[hitPart];
        }

        basePose += XModelNumBones(model);
    }

    trace->fraction = traceState.fraction;
    trace->surfaceFlags = traceState.surfaceFlags;
    trace->normal[0] = traceState.normal[0];
    trace->normal[1] = traceState.normal[1];
    trace->normal[2] = traceState.normal[2];
}
