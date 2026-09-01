#ifndef QCOMMON_XMODEL_TYPES_H
#define QCOMMON_XMODEL_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "file_data.h"
#include "q_vector_types.h"

enum {
    XMODEL_COLLISION_FACET_PLANE_COUNT = 3,
    XMODEL_LOD_COUNT = 3,
    XMODEL_PATH_BUFFER_SIZE = 1024,
    XSURFACE_TRIANGLE_INDEX_COUNT = 3,
    XSURFACE_BONE_USAGE_WORD_COUNT = 4
};

typedef enum xmodel_load_mode_e {
    XMODEL_LOAD_PARTS_ONLY = 0,
    XMODEL_LOAD_SURFACES = 1,
    XMODEL_LOAD_SURFACES_PREPROCESSED = 2
} xmodel_load_mode_t;

struct XSurfaceOptimizedDataARB_s;
struct XSurfaceOptimizedDataATI_s;
struct XSurfaceOptimizedDataNV_s;
struct xanim_int16_vec4_s;

typedef uint16_t XSurfaceTriangle[XSURFACE_TRIANGLE_INDEX_COUNT];
typedef vec2_t XSurfaceTexCoord;

/* The authoritative Windows client and Linux server agree on this counted
 * handle table and its one-element trailing-array allocation.  Windows
 * XModelNumBones/XModelBoneNames/XModelGetBoneIndex are at 0x0049e690,
 * 0x0049e6a0, and 0x0049e6b0; the Linux bodies are at 0x080c3f5e,
 * 0x080c3f76, and 0x080c3f8e. */
struct XModelPartNameTable_s {
    int16_t count;
    /* Pre-C99 one-element trailing-array idiom, not capacity one. */
    uint16_t handles[1];
};

/* Both model-parts loaders allocate this i386 record as eight bytes plus one
 * byte for each additional non-root-part parent delta. */
struct XModelPartNameTableSlot_s {
    XModelPartNameTable *partNameTable;
    /* Backward model-local parent delta for each non-root part. */
    uint8_t parentPartDeltas[1];
};

/* Exact engine record returned across the DObj part-collision boundary.  The
 * Windows engine, Windows cgame consumer, and Linux server agree on its four
 * fields and 0x28-byte i386 extent. */
struct XModelPartColl_s {
    vec3_t mins;
    vec3_t maxs;
    vec3_t center;
    float radiusSq;
};

/* CoDUOMP.exe XModelLoadCollData 0x0049e0c0 and XModelTraceLine 0x0049f0a0,
 * and coduo_lnxded 0x080c34b2 and 0x080c4892, agree on three 0x10-byte
 * planes per collision triangle and the 0x2c-byte i386 surface record. */
struct XModelCollTriPlane_s {
    vec3_t normal;
    float distance;
};

struct XModelCollTri_s {
    XModelCollTriPlane planes[XMODEL_COLLISION_FACET_PLANE_COUNT];
};

struct XModelCollSurf_s {
    XModelCollTri *collTris;
    int32_t numCollTris;
    vec3_t expandedMins; /* Serialized bounds expanded by 0.001. */
    vec3_t expandedMaxs;
    int32_t basePoseIndex;
    int32_t contents;
    int32_t surfaceFlags;
};

/* Exact original tag recovered from the Mac
 * XSurfaceUnstrip(XStripInfo_s *, ...) symbol.  The Windows client and Linux
 * server both consume this three-word triangle-strip carrier. */
struct XStripInfo_s {
    int32_t stripCount;
    const uint8_t *stripVertexCounts;
    const uint16_t *stripIndices;
};

/* Exact tag recovered from the Mac
 * ReadBlend(XSurface_s *, XSimpleBlendInfo_s *, ...) symbol.  The Windows
 * loader at 0x0049d090 and Linux loader at 0x080c1b12 agree on the position
 * and matrix-offset fields. */
struct XSimpleBlendInfo_s {
    vec3_t position;
    uint32_t boneMatrixOffset;
};

/* Bone-local point used by additive skinning.  Consumers transform position
 * through the matrix selected by boneMatrixOffset, then apply weight while
 * accumulating the output position. */
struct XSurfaceWeightedPoint_s {
    XSimpleBlendInfo blend;
    float weight;
};

/* Rigid vertices store the normal first and position second.  The Windows and
 * Linux loaders and consumers agree on the two vec3 fields and 0x18 stride. */
struct XSurfaceRigidVert_s {
    vec3_t normal;
    vec3_t position;
};

/* A blended vertex begins with its normal and primary weighted point.
 * boneMatrixOffset is a byte offset into the 64-byte base-pose matrix array.
 * primaryWeight is present only when additiveWeightCount is nonzero. */
struct XSurfaceBlendVert_s {
    vec3_t normal;
    int32_t additiveWeightCount;
    XSimpleBlendInfo blend;
    float primaryWeight;
};

/* With no additive weights, the primary point has implicit weight one and the
 * stream omits primaryWeight.  Consumers select this 0x20-byte record or the
 * 0x24-byte record above from additiveWeightCount. */
struct XSurfaceBlendVertNoWeight_s {
    vec3_t normal;
    int32_t additiveWeightCount;
    XSimpleBlendInfo blend;
};

/* The single-bone sentinel selects one of two interpretations for XSurface
 * +0x20: fixed-stride rigid vertices or a variable-stride primary-blend
 * stream.  Additive blend points live in weightedPoints. */
union XSurfaceVertexData_u {
    XSurfaceRigidVert *rigidVertices;
    uint8_t *blendPrimaryStream;
};

/* Exact XSurface_s tag recovered from Mac XModelReadSurface and ReadBlend
 * signatures.  CoDUOMP.exe XSurfaceCloneSurface at 0x0049e760 and
 * coduo_lnxded at 0x080c40f0 both copy a 0x34-byte, 13-dword header.  Their
 * accessors agree on every scalar and pointer offset.  XSurfaceGetVerts at
 * 0x0049ea40 and 0x080c4342 also agrees on the rigid, primary-blend, additive,
 * triangle, and texcoord payload formats.  Only the pointed-to optimized
 * renderer records are platform-local, so their tags remain opaque here. */
struct XSurface_s {
    uint8_t tileMode;
    uint8_t padding01; /* ABI padding; neither authority uses it semantically. */
    int16_t vertexCount;
    int16_t triangleCount;
    int16_t boneIndex;
    uint32_t boneUsage[XSURFACE_BONE_USAGE_WORD_COUNT];
    XSurfaceWeightedPoint *weightedPoints;
    XSurfaceTriangle *triangles;
    XSurfaceVertexData vertexData;
    XSurfaceTexCoord *texCoords;
    struct XSurfaceOptimizedDataARB_s *optimizedDataARB;
    struct XSurfaceOptimizedDataATI_s *optimizedDataATI;
    struct XSurfaceOptimizedDataNV_s *optimizedDataNV;
};

/* The Mac XModelSurfsCloneSurfs symbol distinguishes the outer named asset
 * from this inner surface-list payload.  Windows and Linux agree on both
 * i386 record layouts. */
struct XModelSurfsData_s {
    XModelSurfsData *next;
    int16_t surfaceCount;
    uint8_t padding06[2];
    struct XSurface_s **surfaces;
};

struct XModelSurfs_s {
    const char *name;
    XModelSurfsData *surfs;
    fileDataFree_t freeData;
};

/* Shared xmodelparts payload.  The rotation pointee is kept by its common
 * record tag so this XModel header does not own the separate XAnim stream
 * type cluster. */
struct XModelPartsData_s {
    XModelPartNameTableSlot *partNameTableSlot;
    int16_t rootPartCount;
    uint8_t padding06[2];
    XModelPartColl *partCollisions;
    struct xanim_int16_vec4_s *baseRotations;
    vec3_t *baseTranslations;
    /* Model-local part to collision/trace state-slot mapping. */
    uint8_t *partStateIndices;
};

/* Windows and Linux model loaders use three identical 0x14-byte i386 LOD
 * records followed by the same collision payload fields through +0x68. */
struct XModelLodInfo_s {
    float distance;
    const char *name;
    int16_t surfaceCount;
    uint8_t padding0a[2];
    uint16_t *surfaceNameTable;
    XModelSurfs *surfs;
};

struct XModelInfo_s {
    fileData_t *parts; /* xmodelparts asset named by LOD zero. */
    XModelLodInfo lodRecords[XMODEL_LOD_COUNT];
    XModelCollSurf *collisionSurfaces;
    int32_t collisionSurfaceCount;
    int32_t contents;
    vec3_t mins;
    vec3_t maxs;
    int16_t lodCount;
    int16_t modelFileCount;
};

/* XModelConfig is the exact Mac XModelLoadConfigFile type name.  The Windows
 * body at 0x0049e000 and the retained Linux loader use the same three
 * 1024-byte LOD names and the same trailing bounds/count fields. */
struct XModelConfigLod_s {
    char name[XMODEL_PATH_BUFFER_SIZE];
    float distance;
};

struct XModelConfig {
    XModelConfigLod lods[XMODEL_LOD_COUNT];
    vec3_t mins;
    vec3_t maxs;
    int32_t modelFileCount;
};

/* Exact XModel_s tag recovered from Mac XModelCreateDefault. */
struct XModel_s {
    const char *name;
    XModelInfo *info;
    fileDataFree_t freeData;
};

#if UINTPTR_MAX == UINT32_MAX
#if defined(_MSC_VER)
#define XMODEL_TYPES_ALIGNOF(type) __alignof(type)
#elif defined(__cplusplus)
#define XMODEL_TYPES_ALIGNOF(type) alignof(type)
#else
#define XMODEL_TYPES_ALIGNOF(type) __alignof__(type)
#endif
#define XMODEL_TYPES_ASSERT(name, expression) \
    typedef char name[(expression) ? 1 : -1]

XMODEL_TYPES_ASSERT(xmodel_part_name_table_alignment,
                    XMODEL_TYPES_ALIGNOF(XModelPartNameTable) == 2);
XMODEL_TYPES_ASSERT(xmodel_part_name_table_count_offset,
                    offsetof(XModelPartNameTable, count) == 0x00);
XMODEL_TYPES_ASSERT(xmodel_part_name_table_handles_offset,
                    offsetof(XModelPartNameTable, handles) == 0x02);
XMODEL_TYPES_ASSERT(xmodel_part_name_table_handle_stride,
                    sizeof(((XModelPartNameTable *)0)->handles[0]) == 0x02);
XMODEL_TYPES_ASSERT(xmodel_part_name_table_size,
                    sizeof(XModelPartNameTable) == 0x04);
XMODEL_TYPES_ASSERT(xmodel_part_name_slot_alignment,
                    XMODEL_TYPES_ALIGNOF(XModelPartNameTableSlot) == 4);
XMODEL_TYPES_ASSERT(xmodel_part_name_slot_table_offset,
                    offsetof(XModelPartNameTableSlot, partNameTable) == 0x00);
XMODEL_TYPES_ASSERT(xmodel_part_name_slot_deltas_offset,
                    offsetof(XModelPartNameTableSlot, parentPartDeltas) == 0x04);
XMODEL_TYPES_ASSERT(xmodel_part_name_slot_size,
                    sizeof(XModelPartNameTableSlot) == 0x08);

XMODEL_TYPES_ASSERT(xmodel_part_collision_alignment,
                    XMODEL_TYPES_ALIGNOF(XModelPartColl) == 4);
XMODEL_TYPES_ASSERT(xmodel_part_collision_mins_offset,
                    offsetof(XModelPartColl, mins) == 0x00);
XMODEL_TYPES_ASSERT(xmodel_part_collision_maxs_offset,
                    offsetof(XModelPartColl, maxs) == 0x0c);
XMODEL_TYPES_ASSERT(xmodel_part_collision_center_offset,
                    offsetof(XModelPartColl, center) == 0x18);
XMODEL_TYPES_ASSERT(xmodel_part_collision_radius_offset,
                    offsetof(XModelPartColl, radiusSq) == 0x24);
XMODEL_TYPES_ASSERT(xmodel_part_collision_size,
                    sizeof(XModelPartColl) == 0x28);

XMODEL_TYPES_ASSERT(xmodel_collision_plane_alignment,
                    XMODEL_TYPES_ALIGNOF(XModelCollTriPlane) == 4);
XMODEL_TYPES_ASSERT(xmodel_collision_plane_normal_offset,
                    offsetof(XModelCollTriPlane, normal) == 0x00);
XMODEL_TYPES_ASSERT(xmodel_collision_plane_distance_offset,
                    offsetof(XModelCollTriPlane, distance) == 0x0c);
XMODEL_TYPES_ASSERT(xmodel_collision_plane_size,
                    sizeof(XModelCollTriPlane) == 0x10);
XMODEL_TYPES_ASSERT(xmodel_collision_triangle_alignment,
                    XMODEL_TYPES_ALIGNOF(XModelCollTri) == 4);
XMODEL_TYPES_ASSERT(xmodel_collision_triangle_planes_offset,
                    offsetof(XModelCollTri, planes) == 0x00);
XMODEL_TYPES_ASSERT(xmodel_collision_triangle_second_plane_offset,
                    offsetof(XModelCollTri, planes[1]) == 0x10);
XMODEL_TYPES_ASSERT(xmodel_collision_triangle_third_plane_offset,
                    offsetof(XModelCollTri, planes[2]) == 0x20);
XMODEL_TYPES_ASSERT(xmodel_collision_triangle_size,
                    sizeof(XModelCollTri) == 0x30);
XMODEL_TYPES_ASSERT(xmodel_collision_surface_alignment,
                    XMODEL_TYPES_ALIGNOF(XModelCollSurf) == 4);
XMODEL_TYPES_ASSERT(xmodel_collision_surface_triangles_offset,
                    offsetof(XModelCollSurf, collTris) == 0x00);
XMODEL_TYPES_ASSERT(xmodel_collision_surface_count_offset,
                    offsetof(XModelCollSurf, numCollTris) == 0x04);
XMODEL_TYPES_ASSERT(xmodel_collision_surface_mins_offset,
                    offsetof(XModelCollSurf, expandedMins) == 0x08);
XMODEL_TYPES_ASSERT(xmodel_collision_surface_maxs_offset,
                    offsetof(XModelCollSurf, expandedMaxs) == 0x14);
XMODEL_TYPES_ASSERT(xmodel_collision_surface_pose_offset,
                    offsetof(XModelCollSurf, basePoseIndex) == 0x20);
XMODEL_TYPES_ASSERT(xmodel_collision_surface_contents_offset,
                    offsetof(XModelCollSurf, contents) == 0x24);
XMODEL_TYPES_ASSERT(xmodel_collision_surface_flags_offset,
                    offsetof(XModelCollSurf, surfaceFlags) == 0x28);
XMODEL_TYPES_ASSERT(xmodel_collision_surface_size,
                    sizeof(XModelCollSurf) == 0x2c);

XMODEL_TYPES_ASSERT(xsurface_strip_info_alignment,
                    XMODEL_TYPES_ALIGNOF(XStripInfo) == 4);
XMODEL_TYPES_ASSERT(xsurface_strip_info_count_offset,
                    offsetof(XStripInfo, stripCount) == 0x00);
XMODEL_TYPES_ASSERT(xsurface_strip_info_counts_offset,
                    offsetof(XStripInfo, stripVertexCounts) == 0x04);
XMODEL_TYPES_ASSERT(xsurface_strip_info_indices_offset,
                    offsetof(XStripInfo, stripIndices) == 0x08);
XMODEL_TYPES_ASSERT(xsurface_strip_info_size,
                    sizeof(XStripInfo) == 0x0c);

XMODEL_TYPES_ASSERT(xsurface_triangle_size,
                    sizeof(XSurfaceTriangle) == 0x06);
XMODEL_TYPES_ASSERT(xsurface_texcoord_size,
                    sizeof(XSurfaceTexCoord) == 0x08);
XMODEL_TYPES_ASSERT(xsurface_simple_blend_alignment,
                    XMODEL_TYPES_ALIGNOF(XSimpleBlendInfo) == 4);
XMODEL_TYPES_ASSERT(xsurface_simple_blend_position_offset,
                    offsetof(XSimpleBlendInfo, position) == 0x00);
XMODEL_TYPES_ASSERT(xsurface_simple_blend_matrix_offset,
                    offsetof(XSimpleBlendInfo, boneMatrixOffset) == 0x0c);
XMODEL_TYPES_ASSERT(xsurface_simple_blend_size,
                    sizeof(XSimpleBlendInfo) == 0x10);
XMODEL_TYPES_ASSERT(xsurface_weighted_point_alignment,
                    XMODEL_TYPES_ALIGNOF(XSurfaceWeightedPoint) == 4);
XMODEL_TYPES_ASSERT(xsurface_weighted_point_position_offset,
                    offsetof(XSurfaceWeightedPoint, blend.position) == 0x00);
XMODEL_TYPES_ASSERT(xsurface_weighted_point_matrix_offset,
                    offsetof(XSurfaceWeightedPoint,
                             blend.boneMatrixOffset) == 0x0c);
XMODEL_TYPES_ASSERT(xsurface_weighted_point_weight_offset,
                    offsetof(XSurfaceWeightedPoint, weight) == 0x10);
XMODEL_TYPES_ASSERT(xsurface_weighted_point_size,
                    sizeof(XSurfaceWeightedPoint) == 0x14);
XMODEL_TYPES_ASSERT(xsurface_rigid_vertex_alignment,
                    XMODEL_TYPES_ALIGNOF(XSurfaceRigidVert) == 4);
XMODEL_TYPES_ASSERT(xsurface_rigid_vertex_normal_offset,
                    offsetof(XSurfaceRigidVert, normal) == 0x00);
XMODEL_TYPES_ASSERT(xsurface_rigid_vertex_position_offset,
                    offsetof(XSurfaceRigidVert, position) == 0x0c);
XMODEL_TYPES_ASSERT(xsurface_rigid_vertex_size,
                    sizeof(XSurfaceRigidVert) == 0x18);
XMODEL_TYPES_ASSERT(xsurface_blend_vertex_alignment,
                    XMODEL_TYPES_ALIGNOF(XSurfaceBlendVert) == 4);
XMODEL_TYPES_ASSERT(xsurface_blend_vertex_normal_offset,
                    offsetof(XSurfaceBlendVert, normal) == 0x00);
XMODEL_TYPES_ASSERT(xsurface_blend_vertex_count_offset,
                    offsetof(XSurfaceBlendVert,
                             additiveWeightCount) == 0x0c);
XMODEL_TYPES_ASSERT(xsurface_blend_vertex_position_offset,
                    offsetof(XSurfaceBlendVert, blend.position) == 0x10);
XMODEL_TYPES_ASSERT(xsurface_blend_vertex_matrix_offset,
                    offsetof(XSurfaceBlendVert,
                             blend.boneMatrixOffset) == 0x1c);
XMODEL_TYPES_ASSERT(xsurface_blend_vertex_weight_offset,
                    offsetof(XSurfaceBlendVert, primaryWeight) == 0x20);
XMODEL_TYPES_ASSERT(xsurface_blend_vertex_size,
                    sizeof(XSurfaceBlendVert) == 0x24);
XMODEL_TYPES_ASSERT(xsurface_compact_blend_vertex_alignment,
                    XMODEL_TYPES_ALIGNOF(XSurfaceBlendVertNoWeight) == 4);
XMODEL_TYPES_ASSERT(xsurface_compact_blend_vertex_normal_offset,
                    offsetof(XSurfaceBlendVertNoWeight, normal) == 0x00);
XMODEL_TYPES_ASSERT(xsurface_compact_blend_vertex_count_offset,
                    offsetof(XSurfaceBlendVertNoWeight,
                             additiveWeightCount) == 0x0c);
XMODEL_TYPES_ASSERT(xsurface_compact_blend_vertex_position_offset,
                    offsetof(XSurfaceBlendVertNoWeight,
                             blend.position) == 0x10);
XMODEL_TYPES_ASSERT(xsurface_compact_blend_vertex_matrix_offset,
                    offsetof(XSurfaceBlendVertNoWeight,
                             blend.boneMatrixOffset) == 0x1c);
XMODEL_TYPES_ASSERT(xsurface_compact_blend_vertex_size,
                    sizeof(XSurfaceBlendVertNoWeight) == 0x20);
XMODEL_TYPES_ASSERT(xsurface_vertex_data_alignment,
                    XMODEL_TYPES_ALIGNOF(XSurfaceVertexData) == 4);
XMODEL_TYPES_ASSERT(xsurface_vertex_data_rigid_offset,
                    offsetof(XSurfaceVertexData, rigidVertices) == 0x00);
XMODEL_TYPES_ASSERT(xsurface_vertex_data_blend_offset,
                    offsetof(XSurfaceVertexData, blendPrimaryStream) == 0x00);
XMODEL_TYPES_ASSERT(xsurface_vertex_data_size,
                    sizeof(XSurfaceVertexData) == 0x04);
XMODEL_TYPES_ASSERT(xsurface_alignment,
                    XMODEL_TYPES_ALIGNOF(XSurface) == 4);
XMODEL_TYPES_ASSERT(xsurface_tile_mode_offset,
                    offsetof(XSurface, tileMode) == 0x00);
XMODEL_TYPES_ASSERT(xsurface_padding_offset,
                    offsetof(XSurface, padding01) == 0x01);
XMODEL_TYPES_ASSERT(xsurface_vertex_count_offset,
                    offsetof(XSurface, vertexCount) == 0x02);
XMODEL_TYPES_ASSERT(xsurface_triangle_count_offset,
                    offsetof(XSurface, triangleCount) == 0x04);
XMODEL_TYPES_ASSERT(xsurface_bone_index_offset,
                    offsetof(XSurface, boneIndex) == 0x06);
XMODEL_TYPES_ASSERT(xsurface_bone_usage_offset,
                    offsetof(XSurface, boneUsage) == 0x08);
XMODEL_TYPES_ASSERT(xsurface_bone_usage_size,
                    sizeof(((XSurface *)0)->boneUsage) == 0x10);
XMODEL_TYPES_ASSERT(xsurface_weighted_points_offset,
                    offsetof(XSurface, weightedPoints) == 0x18);
XMODEL_TYPES_ASSERT(xsurface_triangles_offset,
                    offsetof(XSurface, triangles) == 0x1c);
XMODEL_TYPES_ASSERT(xsurface_vertex_data_offset,
                    offsetof(XSurface, vertexData) == 0x20);
XMODEL_TYPES_ASSERT(xsurface_texcoords_offset,
                    offsetof(XSurface, texCoords) == 0x24);
XMODEL_TYPES_ASSERT(xsurface_optimized_arb_offset,
                    offsetof(XSurface, optimizedDataARB) == 0x28);
XMODEL_TYPES_ASSERT(xsurface_optimized_ati_offset,
                    offsetof(XSurface, optimizedDataATI) == 0x2c);
XMODEL_TYPES_ASSERT(xsurface_optimized_nv_offset,
                    offsetof(XSurface, optimizedDataNV) == 0x30);
XMODEL_TYPES_ASSERT(xsurface_size,
                    sizeof(XSurface) == 0x34);

XMODEL_TYPES_ASSERT(xmodel_surfs_data_alignment,
                    XMODEL_TYPES_ALIGNOF(XModelSurfsData) == 4);
XMODEL_TYPES_ASSERT(xmodel_surfs_data_next_offset,
                    offsetof(XModelSurfsData, next) == 0x00);
XMODEL_TYPES_ASSERT(xmodel_surfs_data_count_offset,
                    offsetof(XModelSurfsData, surfaceCount) == 0x04);
XMODEL_TYPES_ASSERT(xmodel_surfs_data_padding_offset,
                    offsetof(XModelSurfsData, padding06) == 0x06);
XMODEL_TYPES_ASSERT(xmodel_surfs_data_surfaces_offset,
                    offsetof(XModelSurfsData, surfaces) == 0x08);
XMODEL_TYPES_ASSERT(xmodel_surfs_data_size,
                    sizeof(XModelSurfsData) == 0x0c);
XMODEL_TYPES_ASSERT(xmodel_surfs_name_offset,
                    offsetof(XModelSurfs, name) == 0x00);
XMODEL_TYPES_ASSERT(xmodel_surfs_payload_offset,
                    offsetof(XModelSurfs, surfs) == 0x04);
XMODEL_TYPES_ASSERT(xmodel_surfs_callback_offset,
                    offsetof(XModelSurfs, freeData) == 0x08);
XMODEL_TYPES_ASSERT(xmodel_surfs_size, sizeof(XModelSurfs) == 0x0c);

XMODEL_TYPES_ASSERT(xmodel_parts_data_alignment,
                    XMODEL_TYPES_ALIGNOF(XModelPartsData) == 4);
XMODEL_TYPES_ASSERT(xmodel_parts_data_table_offset,
                    offsetof(XModelPartsData, partNameTableSlot) == 0x00);
XMODEL_TYPES_ASSERT(xmodel_parts_data_root_count_offset,
                    offsetof(XModelPartsData, rootPartCount) == 0x04);
XMODEL_TYPES_ASSERT(xmodel_parts_data_padding_offset,
                    offsetof(XModelPartsData, padding06) == 0x06);
XMODEL_TYPES_ASSERT(xmodel_parts_data_collision_offset,
                    offsetof(XModelPartsData, partCollisions) == 0x08);
XMODEL_TYPES_ASSERT(xmodel_parts_data_rotations_offset,
                    offsetof(XModelPartsData, baseRotations) == 0x0c);
XMODEL_TYPES_ASSERT(xmodel_parts_data_translations_offset,
                    offsetof(XModelPartsData, baseTranslations) == 0x10);
XMODEL_TYPES_ASSERT(xmodel_parts_data_state_indices_offset,
                    offsetof(XModelPartsData, partStateIndices) == 0x14);
XMODEL_TYPES_ASSERT(xmodel_parts_data_size,
                    sizeof(XModelPartsData) == 0x18);

XMODEL_TYPES_ASSERT(xmodel_lod_info_alignment,
                    XMODEL_TYPES_ALIGNOF(XModelLodInfo) == 4);
XMODEL_TYPES_ASSERT(xmodel_lod_info_distance_offset,
                    offsetof(XModelLodInfo, distance) == 0x00);
XMODEL_TYPES_ASSERT(xmodel_lod_info_name_offset,
                    offsetof(XModelLodInfo, name) == 0x04);
XMODEL_TYPES_ASSERT(xmodel_lod_info_surface_count_offset,
                    offsetof(XModelLodInfo, surfaceCount) == 0x08);
XMODEL_TYPES_ASSERT(xmodel_lod_info_padding_offset,
                    offsetof(XModelLodInfo, padding0a) == 0x0a);
XMODEL_TYPES_ASSERT(xmodel_lod_info_name_table_offset,
                    offsetof(XModelLodInfo, surfaceNameTable) == 0x0c);
XMODEL_TYPES_ASSERT(xmodel_lod_info_surfs_offset,
                    offsetof(XModelLodInfo, surfs) == 0x10);
XMODEL_TYPES_ASSERT(xmodel_lod_info_size,
                    sizeof(XModelLodInfo) == 0x14);
XMODEL_TYPES_ASSERT(xmodel_info_alignment,
                    XMODEL_TYPES_ALIGNOF(XModelInfo) == 4);
XMODEL_TYPES_ASSERT(xmodel_info_parts_offset,
                    offsetof(XModelInfo, parts) == 0x00);
XMODEL_TYPES_ASSERT(xmodel_info_lods_offset,
                    offsetof(XModelInfo, lodRecords) == 0x04);
XMODEL_TYPES_ASSERT(xmodel_info_collision_surfaces_offset,
                    offsetof(XModelInfo, collisionSurfaces) == 0x40);
XMODEL_TYPES_ASSERT(xmodel_info_collision_count_offset,
                    offsetof(XModelInfo, collisionSurfaceCount) == 0x44);
XMODEL_TYPES_ASSERT(xmodel_info_contents_offset,
                    offsetof(XModelInfo, contents) == 0x48);
XMODEL_TYPES_ASSERT(xmodel_info_mins_offset,
                    offsetof(XModelInfo, mins) == 0x4c);
XMODEL_TYPES_ASSERT(xmodel_info_maxs_offset,
                    offsetof(XModelInfo, maxs) == 0x58);
XMODEL_TYPES_ASSERT(xmodel_info_lod_count_offset,
                    offsetof(XModelInfo, lodCount) == 0x64);
XMODEL_TYPES_ASSERT(xmodel_info_file_count_offset,
                    offsetof(XModelInfo, modelFileCount) == 0x66);
XMODEL_TYPES_ASSERT(xmodel_info_size, sizeof(XModelInfo) == 0x68);

XMODEL_TYPES_ASSERT(xmodel_config_lod_alignment,
                    XMODEL_TYPES_ALIGNOF(XModelConfigLod) == 4);
XMODEL_TYPES_ASSERT(xmodel_config_lod_name_offset,
                    offsetof(XModelConfigLod, name) == 0x000);
XMODEL_TYPES_ASSERT(xmodel_config_lod_distance_offset,
                    offsetof(XModelConfigLod, distance) == 0x400);
XMODEL_TYPES_ASSERT(xmodel_config_lod_size,
                    sizeof(XModelConfigLod) == 0x404);
XMODEL_TYPES_ASSERT(xmodel_config_alignment,
                    XMODEL_TYPES_ALIGNOF(XModelConfig) == 4);
XMODEL_TYPES_ASSERT(xmodel_config_lods_offset,
                    offsetof(XModelConfig, lods) == 0x000);
XMODEL_TYPES_ASSERT(xmodel_config_second_lod_offset,
                    offsetof(XModelConfig, lods[1]) == 0x404);
XMODEL_TYPES_ASSERT(xmodel_config_third_lod_offset,
                    offsetof(XModelConfig, lods[2]) == 0x808);
XMODEL_TYPES_ASSERT(xmodel_config_mins_offset,
                    offsetof(XModelConfig, mins) == 0xc0c);
XMODEL_TYPES_ASSERT(xmodel_config_maxs_offset,
                    offsetof(XModelConfig, maxs) == 0xc18);
XMODEL_TYPES_ASSERT(xmodel_config_file_count_offset,
                    offsetof(XModelConfig, modelFileCount) == 0xc24);
XMODEL_TYPES_ASSERT(xmodel_config_size, sizeof(XModelConfig) == 0xc28);

XMODEL_TYPES_ASSERT(xmodel_alignment, XMODEL_TYPES_ALIGNOF(XModel) == 4);
XMODEL_TYPES_ASSERT(xmodel_name_offset, offsetof(XModel, name) == 0x00);
XMODEL_TYPES_ASSERT(xmodel_info_offset, offsetof(XModel, info) == 0x04);
XMODEL_TYPES_ASSERT(xmodel_callback_offset,
                    offsetof(XModel, freeData) == 0x08);
XMODEL_TYPES_ASSERT(xmodel_size, sizeof(XModel) == 0x0c);

#undef XMODEL_TYPES_ASSERT
#undef XMODEL_TYPES_ALIGNOF
#endif

#endif
