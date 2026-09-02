#ifndef QCOMMON_DOBJ_TYPES_H
#define QCOMMON_DOBJ_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "asset_type_names.h"
#include "math/q_matrix_types.h"

enum {
    DOBJ_MAX_MODELS = 8,
    DOBJ_POOL_COUNT = 1024,
    DOBJ_SERVER_HANDLE_COUNT = 1024,
    DOBJ_CLIENT_HANDLE_COUNT = 1152,
    DOBJ_NULL_HANDLE = 0,
    DOBJ_LAST_SEARCH_SLOT_INITIAL_VALUE = 1,
    DOBJ_ALLOC_STATE_RECORDS_PER_BYTE = 4,
    DOBJ_ALLOC_STATE_BITS_PER_RECORD = 2,
    DOBJ_ALLOC_STATE_BYTES = DOBJ_POOL_COUNT / DOBJ_ALLOC_STATE_RECORDS_PER_BYTE,
    DOBJ_CHILD_PARENT_NONE = 255,
    DOBJ_PART_REMAP_PREFIX_SIZE = 16,
    DOBJ_PART_REMAP_SENTINEL = 127,
    DOBJ_PART_BITSET_WORD_COUNT = 4,
    DOBJ_PART_BITSET_WORD_BITS = 32,
    DOBJ_MAX_BONES = DOBJ_PART_BITSET_WORD_COUNT * DOBJ_PART_BITSET_WORD_BITS,
    DOBJ_MAX_BONE_INDEX = DOBJ_MAX_BONES - 1,
    DOBJ_PART_INDEX_NOT_FOUND = -1
};

/* Compact surface reference used by the shared DObj tracing walk. */
typedef struct dobj_surface_ref_s {
    int16_t modelIndex;
    int16_t surfaceIndex;
} dobj_surface_ref_t;

/* CoDUOMP.exe and coduo_lnxded agree on the complete compact DObj trace
 * result.  This is distinct from the module-facing trace_t: the vec3 at +0x08
 * is a normal and the hit identifiers are packed at +0x14/+0x16. */
typedef struct dobj_trace_result_s {
    float fraction;
    int32_t surfaceFlags;
    vec3_t normal;
    uint16_t hitPartNameHandle;
    uint16_t hitPartStateIndex;
    uint8_t startsolid;
    uint8_t allsolid;
} dobj_trace_result_t;

/* DObj animation storage has three 16-byte part bitsets followed by two
 * DObjAnimMat lanes per bone.  A third lane per bone begins immediately after
 * the complete partSpans array.  Windows DObjGetAllocSkelSize/DObjCreateSkel
 * (CoDUOMP.exe 0x00494c50 and 0x00494cb0) and Linux
 * (coduo_lnxded 0x080c6a24 and 0x080c6ac4) agree on the 0x30-byte prefix,
 * 0x40-byte span, and total 0x30 + boneCount * 0x60 allocation size. */
typedef struct dobj_eval_part_span_s {
    DObjAnimMat parts[2];
} dobj_eval_part_span_t;

typedef union dobj_eval_storage_span_u {
    dobj_eval_part_span_t evalParts;
    DObjSkelMat basePose;
} dobj_eval_storage_span_t;

typedef struct dobj_eval_storage_s {
    uint32_t evaluatedPartBits[DOBJ_PART_BITSET_WORD_COUNT];
    uint32_t controlPartBits[DOBJ_PART_BITSET_WORD_COUNT];
    uint32_t blockedPartBits[DOBJ_PART_BITSET_WORD_COUNT];
    dobj_eval_storage_span_t partSpans[];
} dobj_eval_storage_t;

/* Exact retail type name from the Mac
 * XAnimCalcData(fileData_s *, XAnimToXModel *, float, DObjAnimMat *, float)
 * symbol.  Windows XAnimBuildPartRemap (0x00496e00) and Linux XAnimSetModel
 * (0x080b9752) agree on a 16-byte part mask followed by one byte per animation
 * part.  The mask remains byte-addressed because it is also interned as string
 * payload rather than being a naturally aligned uint32_t object. */
struct XAnimToXModel {
    uint8_t partBits[DOBJ_PART_REMAP_PREFIX_SIZE];
    uint8_t boneIndex[];
};

/* DObj duplicate-part tracing uses the same 16-byte prefix boundary, but its
 * payload is a zero-terminated list of one-based source/target byte pairs. */
struct DObjTracePartRemap_s {
    uint8_t sourcePartBits[DOBJ_PART_REMAP_PREFIX_SIZE];
    uint8_t duplicatePairs[];
};

/* Module-facing descriptor consumed by DObjCreate.  The Windows constructor
 * at CoDUOMP.exe 0x00494970 and Linux constructor at coduo_lnxded 0x080c667a
 * both walk 0x10-byte elements, consume pointers at +0x00/+0x04, copy the
 * signed halfword at +0x08, skip +0x0a, and test the dword at +0x0c.  Client
 * callers store registered model indices while Linux game callers may store
 * their negation; modelIndex names the shared carrier, not a sign policy. */
struct DObjModel_s {
    XModel *model;
    const char *tagName;
    int16_t modelIndex;
    int16_t reserved_00a;
    int32_t ignoreCollision;
};

/* Fixed DObj pool record.  CoDUOMP.exe DObjCreate
 * (0x00494970..0x00494b98) and coduo_lnxded DObjCreate
 * (0x080c667a..0x080c6961) agree on every field, width, and eight-element
 * array at the original i386 ABI.  Mac DObj_s signatures and the
 * DObjGetNumModels, DObjGetModel, and DObjNumBones symbols support the retained
 * model/bone terminology.
 *
 * partRemapTable is deliberately byte-addressed.  Each packed table contains
 * nodeCount 16-bit handles followed by generation bytes, and the second table
 * can begin at an odd address.  The original x86 bodies perform unaligned
 * halfword accesses; maintained portable callers copy those halfwords without
 * asserting uint16_t alignment. */
struct DObj_s {
    XAnimTree *runtimeTree;
    dobj_eval_storage_t *evaluationStorage;
    int32_t skeletonCacheKey;
    uint8_t *partRemapTable;
    /* Explicitly zeroed as a 16-bit slot when the evaluation-storage key
     * changes; no other authoritative access proves its semantic role. */
    uint16_t unknownState10;
    uint16_t rootHandle;
    uint16_t tracePartRemapHandle;
    uint8_t modelCount;
    uint8_t boneCount;
    uint32_t collisionSkipModelMask;
    XModel *models[DOBJ_MAX_MODELS];
    int16_t modelIndices[DOBJ_MAX_MODELS];
    uint8_t modelParentPartIndices[DOBJ_MAX_MODELS];
    uint8_t modelPartBaseIndices[DOBJ_MAX_MODELS];
};

#if defined(_MSC_VER)
#define DOBJ_TYPES_ALIGNOF(type) __alignof(type)
#elif defined(__cplusplus)
#define DOBJ_TYPES_ALIGNOF(type) alignof(type)
#else
#define DOBJ_TYPES_ALIGNOF(type) __alignof__(type)
#endif
#define DOBJ_TYPES_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]

DOBJ_TYPES_ASSERT(dobj_eval_part_span_alignment, DOBJ_TYPES_ALIGNOF(dobj_eval_part_span_t) == 0x04);
DOBJ_TYPES_ASSERT(dobj_eval_part_span_second_part_offset, offsetof(dobj_eval_part_span_t, parts[1]) == 0x20);
DOBJ_TYPES_ASSERT(dobj_eval_part_span_size, sizeof(dobj_eval_part_span_t) == 0x40);
DOBJ_TYPES_ASSERT(dobj_eval_storage_span_alignment, DOBJ_TYPES_ALIGNOF(dobj_eval_storage_span_t) == 0x04);
DOBJ_TYPES_ASSERT(dobj_eval_storage_span_size, sizeof(dobj_eval_storage_span_t) == 0x40);
DOBJ_TYPES_ASSERT(dobj_eval_storage_control_bits_offset, offsetof(dobj_eval_storage_t, controlPartBits) == 0x10);
DOBJ_TYPES_ASSERT(dobj_eval_storage_blocked_bits_offset, offsetof(dobj_eval_storage_t, blockedPartBits) == 0x20);
DOBJ_TYPES_ASSERT(dobj_eval_storage_spans_offset, offsetof(dobj_eval_storage_t, partSpans) == 0x30);
DOBJ_TYPES_ASSERT(dobj_eval_storage_size, sizeof(dobj_eval_storage_t) == 0x30);
DOBJ_TYPES_ASSERT(xanim_to_xmodel_alignment, DOBJ_TYPES_ALIGNOF(XAnimToXModel) == 0x01);
DOBJ_TYPES_ASSERT(xanim_to_xmodel_bone_index_offset, offsetof(XAnimToXModel, boneIndex) == 0x10);
DOBJ_TYPES_ASSERT(xanim_to_xmodel_size, sizeof(XAnimToXModel) == 0x10);
DOBJ_TYPES_ASSERT(dobj_trace_part_remap_alignment, DOBJ_TYPES_ALIGNOF(DObjTracePartRemap) == 0x01);
DOBJ_TYPES_ASSERT(dobj_trace_part_remap_pairs_offset, offsetof(DObjTracePartRemap, duplicatePairs) == 0x10);
DOBJ_TYPES_ASSERT(dobj_trace_part_remap_size, sizeof(DObjTracePartRemap) == 0x10);
DOBJ_TYPES_ASSERT(dobj_surface_ref_alignment, DOBJ_TYPES_ALIGNOF(dobj_surface_ref_t) == 0x02);
DOBJ_TYPES_ASSERT(dobj_surface_ref_model_index_offset, offsetof(dobj_surface_ref_t, modelIndex) == 0x00);
DOBJ_TYPES_ASSERT(dobj_surface_ref_surface_index_offset, offsetof(dobj_surface_ref_t, surfaceIndex) == 0x02);
DOBJ_TYPES_ASSERT(dobj_surface_ref_size, sizeof(dobj_surface_ref_t) == 0x04);
DOBJ_TYPES_ASSERT(dobj_trace_result_alignment, DOBJ_TYPES_ALIGNOF(dobj_trace_result_t) == 0x04);
DOBJ_TYPES_ASSERT(dobj_trace_result_fraction_offset, offsetof(dobj_trace_result_t, fraction) == 0x00);
DOBJ_TYPES_ASSERT(dobj_trace_result_surface_flags_offset, offsetof(dobj_trace_result_t, surfaceFlags) == 0x04);
DOBJ_TYPES_ASSERT(dobj_trace_result_normal_offset, offsetof(dobj_trace_result_t, normal) == 0x08);
DOBJ_TYPES_ASSERT(dobj_trace_result_hit_part_offset, offsetof(dobj_trace_result_t, hitPartNameHandle) == 0x14);
DOBJ_TYPES_ASSERT(dobj_trace_result_part_state_offset, offsetof(dobj_trace_result_t, hitPartStateIndex) == 0x16);
DOBJ_TYPES_ASSERT(dobj_trace_result_startsolid_offset, offsetof(dobj_trace_result_t, startsolid) == 0x18);
DOBJ_TYPES_ASSERT(dobj_trace_result_allsolid_offset, offsetof(dobj_trace_result_t, allsolid) == 0x19);
DOBJ_TYPES_ASSERT(dobj_trace_result_size, sizeof(dobj_trace_result_t) == 0x1c);

#if UINTPTR_MAX == UINT32_MAX
DOBJ_TYPES_ASSERT(dobj_model_alignment, DOBJ_TYPES_ALIGNOF(DObjModel) == 0x04);
DOBJ_TYPES_ASSERT(dobj_model_model_offset, offsetof(DObjModel, model) == 0x00);
DOBJ_TYPES_ASSERT(dobj_model_tag_name_offset, offsetof(DObjModel, tagName) == 0x04);
DOBJ_TYPES_ASSERT(dobj_model_index_offset, offsetof(DObjModel, modelIndex) == 0x08);
DOBJ_TYPES_ASSERT(dobj_model_reserved_offset, offsetof(DObjModel, reserved_00a) == 0x0a);
DOBJ_TYPES_ASSERT(dobj_model_ignore_collision_offset, offsetof(DObjModel, ignoreCollision) == 0x0c);
DOBJ_TYPES_ASSERT(dobj_model_size, sizeof(DObjModel) == 0x10);
DOBJ_TYPES_ASSERT(dobj_alignment, DOBJ_TYPES_ALIGNOF(DObj) == 0x04);
DOBJ_TYPES_ASSERT(dobj_runtime_tree_offset, offsetof(DObj, runtimeTree) == 0x00);
DOBJ_TYPES_ASSERT(dobj_evaluation_storage_offset, offsetof(DObj, evaluationStorage) == 0x04);
DOBJ_TYPES_ASSERT(dobj_skeleton_cache_key_offset, offsetof(DObj, skeletonCacheKey) == 0x08);
DOBJ_TYPES_ASSERT(dobj_part_remap_table_offset, offsetof(DObj, partRemapTable) == 0x0c);
DOBJ_TYPES_ASSERT(dobj_unknown_state_offset, offsetof(DObj, unknownState10) == 0x10);
DOBJ_TYPES_ASSERT(dobj_root_handle_offset, offsetof(DObj, rootHandle) == 0x12);
DOBJ_TYPES_ASSERT(dobj_trace_part_remap_handle_offset, offsetof(DObj, tracePartRemapHandle) == 0x14);
DOBJ_TYPES_ASSERT(dobj_model_count_offset, offsetof(DObj, modelCount) == 0x16);
DOBJ_TYPES_ASSERT(dobj_bone_count_offset, offsetof(DObj, boneCount) == 0x17);
DOBJ_TYPES_ASSERT(dobj_collision_skip_model_mask_offset, offsetof(DObj, collisionSkipModelMask) == 0x18);
DOBJ_TYPES_ASSERT(dobj_models_offset, offsetof(DObj, models) == 0x1c);
DOBJ_TYPES_ASSERT(dobj_model_indices_offset, offsetof(DObj, modelIndices) == 0x3c);
DOBJ_TYPES_ASSERT(dobj_model_parent_part_indices_offset, offsetof(DObj, modelParentPartIndices) == 0x4c);
DOBJ_TYPES_ASSERT(dobj_model_part_base_indices_offset, offsetof(DObj, modelPartBaseIndices) == 0x54);
DOBJ_TYPES_ASSERT(dobj_size, sizeof(DObj) == 0x5c);
#endif

#undef DOBJ_TYPES_ASSERT
#undef DOBJ_TYPES_ALIGNOF

#endif
