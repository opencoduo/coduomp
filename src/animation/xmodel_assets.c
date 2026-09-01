#include "animation_private.h"
#include "collision/collision_trace_bounds.h"
#include "filesystem/filesystem.h"
#include "math/q_math.h"
#include "qcommon/com_sprintf.h"
#include "xanim_asset_load.h"
#include "xmodel.h"

#include <float.h>
#include <stdio.h>
#include <string.h>

#define XMODEL_COLLISION_CONTENTS_MASK UINT32_C(0xdfff7ffb)

/* Runtime objects use pointer-typed XModelPartsData. This carrier supplies
 * only the original i386 header width when preserving the stock allocation. */
typedef struct xmodel_parts_data_stock_i386_layout_s {
    uint32_t partNameTableSlotAddress;
    int16_t rootPartCount;
    uint8_t padding06[2];
    uint32_t partCollisionsAddress;
    uint32_t baseRotationsAddress;
    uint32_t baseTranslationsAddress;
    uint32_t partStateIndicesAddress;
} xmodel_parts_data_stock_i386_layout_t;

enum {
    XMODEL_ASSET_VERSION = 14,
    XMODEL_DEFAULT_BONE_COUNT = 1,
    XMODEL_DEFAULT_LOD_COUNT = 1,
    XMODEL_DEFAULT_SURFACE_COUNT = 1,
    XMODEL_DEFAULT_VERTEX_COUNT = 3,
    XMODEL_NO_SINGLE_BONE = -1,
    XMODEL_TRACE_NO_HIT = -1,
    XMODEL_BONE_INDEX_SHIFT = 6,
    XMODEL_BONE_USAGE_SHIFT = 3,
    XMODEL_BONE_USAGE_MASK = 7,
    XMODEL_BONE_USAGE_CAPACITY =
        XSURFACE_BONE_USAGE_WORD_COUNT * 32,
    XMODEL_TRIANGLE_INDEX_COUNT = XSURFACE_TRIANGLE_INDEX_COUNT,
    XMODEL_MAX_BONES = 127,
    XMODEL_PART_NAME_STRING_TYPE = 10,
    XMODEL_SURFACE_NAME_STRING_TYPE = 8,
    XMODEL_BOUNDS_CORNER_COUNT = 8
};

XModelInfo xmodel_defaultCollision;
static const char xmodel_emptyName[] = "";
static XModelPartNameTable xmodel_defaultPartNameTable;
static XModelPartNameTableSlot xmodel_defaultPartNameSlot;
/* CoDUOMP.exe stores the root-only default part-state byte independently from the
 * part-name slot header (0x00b8d4fc versus 0x00b8d37c). */
static uint8_t xmodel_defaultPartStateIndices[XMODEL_DEFAULT_BONE_COUNT];
static XModelPartsData xmodel_defaultParts;
static fileData_t xmodel_defaultPartsEntry;
static XModelPartColl xmodel_defaultPartCollision;
static XSurface xmodel_defaultSurface;
static XSurfaceTriangle
    xmodel_defaultTriangles[XMODEL_DEFAULT_SURFACE_COUNT];
static XSurfaceRigidVert
    xmodel_defaultRigidVertices[XMODEL_DEFAULT_VERTEX_COUNT];
static vec2_t xmodel_defaultTexCoords[XMODEL_DEFAULT_VERTEX_COUNT];
static XSurface *xmodel_defaultSurfacePtrs[XMODEL_DEFAULT_SURFACE_COUNT];
static XModelSurfsData xmodel_defaultSurfs;
static XModelSurfs xmodel_defaultSurfsEntry;
static uint16_t xmodel_defaultSurfaceNames[XMODEL_DEFAULT_SURFACE_COUNT];
static XModel xmodel_defaultModelEntry;
static XModelSurfsData *xmodel_surfsCloneHead;
/* Windows CoDUOMP.exe 0x00b8d4f8 and Linux coduo_lnxded
 * RVA 0x000af330 both initialize this missing-asset policy latch to true. */
qboolean xmodel_enforceExist = qtrue;
static qboolean xmodel_optimizeEnabled;

/* NOT_FROM_ORIGINAL_SOURCE: portable unaligned reads for the packed little-
 * endian XModel stream. All supported target architectures are little-endian. */
static int16_t XModelReadStreamInt16(const uint8_t **position)
{
    int16_t value;
    memcpy(&value, *position, sizeof(value));
    *position += sizeof(value);
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: portable unaligned float read corresponding to
 * the original x86 dword loads from the packed XModel stream. */
static float XModelReadStreamFloat(const uint8_t **position)
{
    float value;
    memcpy(&value, *position, sizeof(value));
    *position += sizeof(value);
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: portable unaligned dword read corresponding to
 * the original x86 packed-stream load. */
static uint32_t XModelReadStreamUint32(const uint8_t **position)
{
    uint32_t value;
    memcpy(&value, *position, sizeof(value));
    *position += sizeof(value);
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: bounded validation cursor used before the retail
 * XModel parsers consume packed files through their original pointer-only
 * interfaces. Keeping validation as a prepass preserves every operation of a
 * valid stream while preventing malformed counts from reaching allocations,
 * stack scratch storage, or downstream model consumers. */
typedef struct coduo_xmodel_validation_stream_s {
    const uint8_t *cursor;
    const uint8_t *end;
} coduo_xmodel_validation_stream_t;

static size_t coduo_xmodel_validation_remaining(
    const coduo_xmodel_validation_stream_t *stream)
{
    return (size_t)(stream->end - stream->cursor);
}

static qboolean coduo_xmodel_validation_take(
    coduo_xmodel_validation_stream_t *stream, size_t size,
    const uint8_t **bytes)
{
    if (size > coduo_xmodel_validation_remaining(stream)) {
        return qfalse;
    }
    if (bytes != NULL) {
        *bytes = stream->cursor;
    }
    stream->cursor += size;
    return qtrue;
}

static qboolean coduo_xmodel_validation_u8(
    coduo_xmodel_validation_stream_t *stream, uint8_t *value)
{
    const uint8_t *bytes;
    if (coduo_xmodel_validation_take(stream, sizeof(*value), &bytes) ==
        qfalse) {
        return qfalse;
    }
    *value = bytes[0];
    return qtrue;
}

static qboolean coduo_xmodel_validation_i16(
    coduo_xmodel_validation_stream_t *stream, int16_t *value)
{
    const uint8_t *bytes;
    if (coduo_xmodel_validation_take(stream, sizeof(*value), &bytes) ==
        qfalse) {
        return qfalse;
    }
    memcpy(value, bytes, sizeof(*value));
    return qtrue;
}

static qboolean coduo_xmodel_validation_u32(
    coduo_xmodel_validation_stream_t *stream, uint32_t *value)
{
    const uint8_t *bytes;
    if (coduo_xmodel_validation_take(stream, sizeof(*value), &bytes) ==
        qfalse) {
        return qfalse;
    }
    memcpy(value, bytes, sizeof(*value));
    return qtrue;
}

static qboolean coduo_xmodel_validation_c_string(
    coduo_xmodel_validation_stream_t *stream, size_t *length)
{
    const size_t remaining = coduo_xmodel_validation_remaining(stream);
    const uint8_t *terminator = memchr(stream->cursor, '\0', remaining);
    if (terminator == NULL) {
        return qfalse;
    }
    if (length != NULL) {
        *length = (size_t)(terminator - stream->cursor);
    }
    stream->cursor = terminator + 1;
    return qtrue;
}

static uint16_t coduo_xmodel_validation_index(const uint8_t *bytes,
                                               size_t index)
{
    uint16_t value;
    memcpy(&value, bytes + index * sizeof(value), sizeof(value));
    return value;
}

static qboolean coduo_xmodel_validate_blend(
    coduo_xmodel_validation_stream_t *stream)
{
    int16_t boneIndex;
    if (coduo_xmodel_validation_i16(stream, &boneIndex) == qfalse ||
        boneIndex < 0 || boneIndex >= XMODEL_MAX_BONES) {
        return qfalse;
    }
    return coduo_xmodel_validation_take(stream, sizeof(vec3_t), NULL);
}

static qboolean coduo_xmodel_validate_surface(
    coduo_xmodel_validation_stream_t *stream)
{
    uint8_t tileMode;
    int16_t vertexCount;
    int16_t triangleCount;
    int16_t stripCount;
    int16_t singleBoneIndex;
    int16_t weightedPointCount = 0;
    int16_t compactWeightedVertexCount = 0;

    if (coduo_xmodel_validation_u8(stream, &tileMode) == qfalse ||
        coduo_xmodel_validation_i16(stream, &vertexCount) == qfalse ||
        coduo_xmodel_validation_i16(stream, &triangleCount) == qfalse ||
        coduo_xmodel_validation_i16(stream, &stripCount) == qfalse ||
        coduo_xmodel_validation_i16(stream, &singleBoneIndex) == qfalse) {
        return qfalse;
    }
    (void)tileMode;

    if (vertexCount < 0 || triangleCount < 0 || stripCount < 0 ||
        (triangleCount == INT16_MAX && (triangleCount & 1) != 0)) {
        return qfalse;
    }

    if (singleBoneIndex == XMODEL_NO_SINGLE_BONE) {
        if (coduo_xmodel_validation_i16(stream, &weightedPointCount) ==
                qfalse ||
            coduo_xmodel_validation_i16(
                stream, &compactWeightedVertexCount) == qfalse ||
            weightedPointCount < 0 || compactWeightedVertexCount < 0 ||
            compactWeightedVertexCount > vertexCount) {
            return qfalse;
        }
    } else if (singleBoneIndex < 0 ||
               singleBoneIndex >= XMODEL_MAX_BONES) {
        return qfalse;
    }

    int32_t producedTriangles = 0;
    for (int32_t strip = 0; strip < stripCount; ++strip) {
        uint8_t stripVertexCount;
        const uint8_t *indices;
        if (coduo_xmodel_validation_u8(stream, &stripVertexCount) == qfalse ||
            stripVertexCount < XMODEL_TRIANGLE_INDEX_COUNT ||
            coduo_xmodel_validation_take(
                stream, (size_t)stripVertexCount * sizeof(uint16_t),
                &indices) == qfalse) {
            return qfalse;
        }

        for (size_t index = 0; index < stripVertexCount; ++index) {
            if (coduo_xmodel_validation_index(indices, index) >=
                (uint16_t)vertexCount) {
                return qfalse;
            }
        }

        uint16_t previousA = coduo_xmodel_validation_index(indices, 0);
        uint16_t previousB = coduo_xmodel_validation_index(indices, 1);
        uint16_t current = coduo_xmodel_validation_index(indices, 2);
        if (previousA != previousB && previousA != current &&
            previousB != current) {
            ++producedTriangles;
        }
        for (int32_t vertex = XMODEL_TRIANGLE_INDEX_COUNT;
             vertex < stripVertexCount; vertex += 2) {
            uint16_t next = coduo_xmodel_validation_index(
                indices, (size_t)vertex);
            if (current != previousB && current != next &&
                previousB != next) {
                ++producedTriangles;
            }
            if (stripVertexCount <= vertex + 1) {
                break;
            }
            uint16_t next2 = coduo_xmodel_validation_index(
                indices, (size_t)vertex + 1u);
            if (current != next && current != next2 && next != next2) {
                ++producedTriangles;
            }
            current = next2;
            previousB = next;
        }
    }
    if (producedTriangles != triangleCount) {
        return qfalse;
    }

    if (singleBoneIndex == XMODEL_NO_SINGLE_BONE) {
        int32_t zeroAdditiveVertices = 0;
        int32_t totalAdditivePoints = 0;
        for (int32_t vertex = 0; vertex < vertexCount; ++vertex) {
            int16_t additiveWeightCount;
            if (coduo_xmodel_validation_take(
                    stream, sizeof(vec3_t) + sizeof(vec2_t), NULL) == qfalse ||
                coduo_xmodel_validation_i16(
                    stream, &additiveWeightCount) == qfalse ||
                additiveWeightCount < 0 ||
                coduo_xmodel_validate_blend(stream) == qfalse) {
                return qfalse;
            }
            if (additiveWeightCount == 0) {
                ++zeroAdditiveVertices;
            } else if (coduo_xmodel_validation_take(
                           stream, sizeof(float), NULL) == qfalse) {
                return qfalse;
            }
            if (totalAdditivePoints >
                weightedPointCount - additiveWeightCount) {
                return qfalse;
            }
            totalAdditivePoints += additiveWeightCount;
        }
        if (zeroAdditiveVertices != compactWeightedVertexCount ||
            totalAdditivePoints != weightedPointCount) {
            return qfalse;
        }
        for (int32_t point = 0; point < weightedPointCount; ++point) {
            if (coduo_xmodel_validate_blend(stream) == qfalse ||
                coduo_xmodel_validation_take(
                    stream, sizeof(float), NULL) == qfalse) {
                return qfalse;
            }
        }
    } else if (coduo_xmodel_validation_take(
                   stream,
                   (size_t)vertexCount *
                       (sizeof(XSurfaceRigidVert) + sizeof(vec2_t)),
                   NULL) == qfalse) {
        return qfalse;
    }

    return qtrue;
}

static qboolean coduo_xmodel_validate_surfs_file(
    const uint8_t *data, size_t size)
{
    if (data == NULL) {
        return qfalse;
    }
    coduo_xmodel_validation_stream_t stream = {data, data + size};
    int16_t version;
    int16_t surfaceCount;
    if (coduo_xmodel_validation_i16(&stream, &version) == qfalse ||
        version != XMODEL_ASSET_VERSION ||
        coduo_xmodel_validation_i16(&stream, &surfaceCount) == qfalse ||
        surfaceCount < 0) {
        return qfalse;
    }
    for (int32_t surface = 0; surface < surfaceCount; ++surface) {
        if (coduo_xmodel_validate_surface(&stream) == qfalse) {
            return qfalse;
        }
    }
    return qtrue;
}

static qboolean coduo_xmodel_validate_parts_file(
    const uint8_t *data, size_t size)
{
    if (data == NULL) {
        return qfalse;
    }
    coduo_xmodel_validation_stream_t stream = {data, data + size};
    int16_t version;
    int16_t childPartCount;
    int16_t rootPartCount;
    if (coduo_xmodel_validation_i16(&stream, &version) == qfalse ||
        version != XMODEL_ASSET_VERSION ||
        coduo_xmodel_validation_i16(&stream, &childPartCount) == qfalse ||
        coduo_xmodel_validation_i16(&stream, &rootPartCount) == qfalse ||
        childPartCount < 0 || rootPartCount < 0) {
        return qfalse;
    }

    const int32_t totalPartCount =
        (int32_t)childPartCount + (int32_t)rootPartCount;
    if (totalPartCount <= 0 || totalPartCount > XMODEL_MAX_BONES) {
        return qfalse;
    }

    for (int32_t partIndex = rootPartCount;
         partIndex < totalPartCount; ++partIndex) {
        uint8_t parentIndex;
        if (coduo_xmodel_validation_u8(&stream, &parentIndex) == qfalse ||
            parentIndex >= partIndex ||
            coduo_xmodel_validation_take(
                &stream, sizeof(vec3_t) + 3u * sizeof(int16_t), NULL) ==
                qfalse) {
            return qfalse;
        }
    }

    for (int32_t partIndex = 0;
         partIndex < totalPartCount; ++partIndex) {
        if (coduo_xmodel_validation_c_string(&stream, NULL) == qfalse ||
            coduo_xmodel_validation_take(
                &stream, 2u * sizeof(vec3_t), NULL) == qfalse) {
            return qfalse;
        }
    }
    return coduo_xmodel_validation_take(
        &stream, (size_t)totalPartCount, NULL);
}

static qboolean coduo_xmodel_validate_model_file(
    const uint8_t *data, size_t size)
{
    if (data == NULL) {
        return qfalse;
    }
    coduo_xmodel_validation_stream_t stream = {data, data + size};
    int16_t version;
    qboolean lodPresent[XMODEL_LOD_COUNT];
    if (coduo_xmodel_validation_i16(&stream, &version) == qfalse ||
        version != XMODEL_ASSET_VERSION ||
        coduo_xmodel_validation_take(
            &stream, 2u * sizeof(vec3_t), NULL) == qfalse) {
        return qfalse;
    }

    for (int32_t lod = 0; lod < XMODEL_LOD_COUNT; ++lod) {
        size_t nameLength;
        if (coduo_xmodel_validation_take(
                &stream, sizeof(float), NULL) == qfalse ||
            coduo_xmodel_validation_c_string(
                &stream, &nameLength) == qfalse ||
            nameLength >= XMODEL_PATH_BUFFER_SIZE) {
            return qfalse;
        }
        lodPresent[lod] = nameLength != 0 ? qtrue : qfalse;
    }

    uint32_t modelFileCount;
    uint32_t collisionSurfaceCount;
    if (coduo_xmodel_validation_u32(&stream, &modelFileCount) == qfalse ||
        coduo_xmodel_validation_u32(
            &stream, &collisionSurfaceCount) == qfalse ||
        collisionSurfaceCount > INT32_MAX) {
        return qfalse;
    }
    (void)modelFileCount;

    for (uint32_t surface = 0;
         surface < collisionSurfaceCount; ++surface) {
        uint32_t triangleCount;
        uint32_t basePoseIndex;
        uint32_t contents;
        uint32_t surfaceFlags;
        if (coduo_xmodel_validation_u32(&stream, &triangleCount) == qfalse ||
            triangleCount > INT32_MAX ||
            triangleCount >
                coduo_xmodel_validation_remaining(&stream) /
                    sizeof(XModelCollTri) ||
            coduo_xmodel_validation_take(
                &stream, (size_t)triangleCount * sizeof(XModelCollTri),
                NULL) == qfalse ||
            coduo_xmodel_validation_take(
                &stream, 2u * sizeof(vec3_t), NULL) == qfalse ||
            coduo_xmodel_validation_u32(
                &stream, &basePoseIndex) == qfalse ||
            coduo_xmodel_validation_u32(&stream, &contents) == qfalse ||
            coduo_xmodel_validation_u32(
                &stream, &surfaceFlags) == qfalse ||
            basePoseIndex >= XMODEL_MAX_BONES) {
            return qfalse;
        }
        (void)contents;
        (void)surfaceFlags;
    }

    for (int32_t lod = 0; lod < XMODEL_LOD_COUNT; ++lod) {
        if (lodPresent[lod] == qfalse) {
            continue;
        }
        int16_t surfaceCount;
        if (coduo_xmodel_validation_i16(&stream, &surfaceCount) == qfalse ||
            surfaceCount < 0) {
            return qfalse;
        }
        for (int32_t surface = 0; surface < surfaceCount; ++surface) {
            /* Some stock-compatible exporters leave declared but unused
             * trailing surface-name slots out of the file. Retail walks past
             * EOF and obtains empty strings from FS_ReadFile's allocation
             * padding. Preserve that result explicitly without permitting an
             * unterminated partial string or an out-of-bounds read. */
            if (coduo_xmodel_validation_remaining(&stream) == 0) {
                continue;
            }
            if (coduo_xmodel_validation_c_string(&stream, NULL) == qfalse) {
                return qfalse;
            }
        }
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: cross-file validation for references consumed
 * after the stock loader has independently accepted the three asset files.
 * Geometry indexes the surface-name table, so fewer names than geometry
 * surfaces is unsafe; extra authored names are unused and are stock-valid. */
static qboolean coduo_xmodel_loaded_references_fit(
    const XModelInfo *model, const XModelLodInfo *lod,
    const XModelSurfsData *surfs)
{
    const int32_t boneCount =
        model->parts->data.xmodelParts->partNameTableSlot
            ->partNameTable->count;
    if (lod->surfaceCount < surfs->surfaceCount) {
        return qfalse;
    }
    for (int32_t surfaceIndex = 0;
         surfaceIndex < surfs->surfaceCount; ++surfaceIndex) {
        const XSurface *surface = surfs->surfaces[surfaceIndex];
        for (int32_t boneIndex = boneCount;
             boneIndex < XMODEL_BONE_USAGE_CAPACITY; ++boneIndex) {
            if ((surface->boneUsage[boneIndex >> 5] &
                 (UINT32_C(1) << (boneIndex & 31))) != 0) {
                return qfalse;
            }
        }
    }
    return qtrue;
}

static qboolean coduo_xmodel_loaded_collision_fits(
    const XModelInfo *model)
{
    const int32_t boneCount =
        model->parts->data.xmodelParts->partNameTableSlot
            ->partNameTable->count;
    for (int32_t surfaceIndex = 0;
         surfaceIndex < model->collisionSurfaceCount; ++surfaceIndex) {
        const int32_t basePoseIndex =
            model->collisionSurfaces[surfaceIndex].basePoseIndex;
        if (basePoseIndex < 0 || basePoseIndex >= boneCount) {
            return qfalse;
        }
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x0049d090..0x0049d0ee, exporter-gap recovery. */
void ReadBlend(XSurface *surface, XSimpleBlendInfo *blend,
               const uint8_t **position)
{
    const uint8_t *cursor = *position;
    int16_t boneIndex;

    memcpy(&boneIndex, cursor, sizeof(boneIndex));
    cursor += sizeof(boneIndex);
    /* NOT_FROM_ORIGINAL_SOURCE: retain a sink check for the prevalidated bone
     * index before updating the fixed mask. */
    if (boneIndex < 0 || boneIndex >= XMODEL_MAX_BONES) {
        Com_Error(ERR_DROP, "\x15" "XModel blend has an invalid bone index");
        return;
    }
    ((uint8_t *)surface->boneUsage)[boneIndex >> XMODEL_BONE_USAGE_SHIFT] |=
        (uint8_t)(1U << (boneIndex & XMODEL_BONE_USAGE_MASK));
    blend->boneMatrixOffset =
        (uint32_t)(int32_t)boneIndex << XMODEL_BONE_INDEX_SHIFT;
    memcpy(&blend->position[0], cursor, sizeof(blend->position[0]));
    cursor += sizeof(blend->position[0]);
    memcpy(&blend->position[1], cursor, sizeof(blend->position[1]));
    cursor += sizeof(blend->position[1]);
    memcpy(&blend->position[2], cursor, sizeof(blend->position[2]));
    cursor += sizeof(blend->position[2]);
    *position = cursor;
}

/* Source: CoDUOMP.exe 0x0049d0f0..0x0049d1dd. */
void XSurfaceUnstrip(const XStripInfo *strips,
                     XSurfaceTriangle *triangles)
{
    const uint16_t *stripIndex = strips->stripIndices;

    for (int32_t strip = 0; strip < strips->stripCount; ++strip) {
        uint8_t vertexCount = strips->stripVertexCounts[strip];
        /* NOT_FROM_ORIGINAL_SOURCE: retain the prepass's minimum strip length
         * at the sink before loading the first three indexes. */
        if (vertexCount < XMODEL_TRIANGLE_INDEX_COUNT) {
            stripIndex += vertexCount;
            continue;
        }
        uint16_t previousA = stripIndex[0];
        uint16_t previousB = stripIndex[1];
        uint16_t current = stripIndex[2];

        stripIndex += XMODEL_TRIANGLE_INDEX_COUNT;
        if (previousA != previousB && previousA != current &&
            previousB != current) {
            triangles[0][0] = previousA;
            triangles[0][1] = previousB;
            triangles[0][2] = current;
            ++triangles;
        }

        for (int32_t vertex = XMODEL_TRIANGLE_INDEX_COUNT;
             vertex < vertexCount;
             vertex += 2) {
            uint16_t next = stripIndex[0];
            if (current != previousB && current != next &&
                previousB != next) {
                triangles[0][0] = current;
                triangles[0][1] = previousB;
                triangles[0][2] = next;
                ++triangles;
            }

            if (vertexCount <= vertex + 1) {
                ++stripIndex;
                break;
            }

            uint16_t next2 = stripIndex[1];
            stripIndex += 2;
            if (current != next && current != next2 && next != next2) {
                triangles[0][0] = current;
                triangles[0][1] = next;
                triangles[0][2] = next2;
                ++triangles;
            }
            current = next2;
            previousB = next;
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: consume validated serialized strip records
 * directly and preserve the triangle order without temporary variable-sized
 * scratch arrays. */
static void coduo_xmodel_unstrip_serialized(
    const uint8_t *stripData, int32_t stripCount,
    XSurfaceTriangle *triangles)
{
    for (int32_t strip = 0; strip < stripCount; ++strip) {
        const uint8_t vertexCount = *stripData++;
        const uint8_t *indices = stripData;
        stripData += (size_t)vertexCount * sizeof(uint16_t);

        uint16_t previousA = coduo_xmodel_validation_index(indices, 0);
        uint16_t previousB = coduo_xmodel_validation_index(indices, 1);
        uint16_t current = coduo_xmodel_validation_index(indices, 2);
        if (previousA != previousB && previousA != current &&
            previousB != current) {
            triangles[0][0] = previousA;
            triangles[0][1] = previousB;
            triangles[0][2] = current;
            ++triangles;
        }

        for (int32_t vertex = XMODEL_TRIANGLE_INDEX_COUNT;
             vertex < vertexCount; vertex += 2) {
            uint16_t next = coduo_xmodel_validation_index(
                indices, (size_t)vertex);
            if (current != previousB && current != next &&
                previousB != next) {
                triangles[0][0] = current;
                triangles[0][1] = previousB;
                triangles[0][2] = next;
                ++triangles;
            }
            if (vertexCount <= vertex + 1) {
                break;
            }

            uint16_t next2 = coduo_xmodel_validation_index(
                indices, (size_t)vertex + 1u);
            if (current != next && current != next2 && next != next2) {
                triangles[0][0] = current;
                triangles[0][1] = next;
                triangles[0][2] = next2;
                ++triangles;
            }
            current = next2;
            previousB = next;
        }
    }
}

/* Sources: CoDUOMP.exe 0x0049d1e0..0x0049d769;
 * coduo_lnxded 0x080c1d6c..0x080c23bc. */
void XModelReadSurface(XSurface *surface, const uint8_t **position,
                       void *(*alloc)(size_t size))
{
    /* NOT_FROM_ORIGINAL_SOURCE: XModelSurfsPrecache validates the complete
     * packed record graph before this retained pointer-only parser is entered. */
    const uint8_t *cursor = *position;
    int16_t weightedPointCount = 0;
    int16_t compactWeightedVertexCount = 0;

    surface->tileMode = *cursor++;
    surface->vertexCount = XModelReadStreamInt16(&cursor);
    surface->triangleCount = XModelReadStreamInt16(&cursor);
    int16_t stripCount = XModelReadStreamInt16(&cursor);
    int16_t singleBoneIndex = XModelReadStreamInt16(&cursor);
    memset(surface->boneUsage, 0, sizeof(surface->boneUsage));

    if (singleBoneIndex == XMODEL_NO_SINGLE_BONE) {
        weightedPointCount = XModelReadStreamInt16(&cursor);
        compactWeightedVertexCount = XModelReadStreamInt16(&cursor);
        surface->boneIndex = XMODEL_NO_SINGLE_BONE;
        uint32_t fullBlendVertexCount =
            (uint32_t)(int32_t)surface->vertexCount -
            (uint32_t)(int32_t)compactWeightedVertexCount;
        uint32_t blendPrimaryBytes =
            fullBlendVertexCount * (uint32_t)sizeof(XSurfaceBlendVert) +
            (uint32_t)(int32_t)compactWeightedVertexCount *
                (uint32_t)sizeof(XSurfaceBlendVertNoWeight);
        surface->vertexData.blendPrimaryStream = alloc(
            (size_t)blendPrimaryBytes);
        surface->weightedPoints = alloc(
            (size_t)((uint32_t)(int32_t)weightedPointCount *
                     (uint32_t)sizeof(surface->weightedPoints[0])));
        surface->texCoords = alloc(
            (size_t)((uint32_t)(int32_t)surface->vertexCount *
                     (uint32_t)sizeof(surface->texCoords[0])));
    } else {
        surface->boneIndex =
            (int16_t)((uint32_t)(int32_t)singleBoneIndex <<
                      XMODEL_BONE_INDEX_SHIFT);
        /* The prepass and ReadBlend-equivalent index policy prove this mask
         * lane before the retained original store. */
        ((uint8_t *)surface->boneUsage)
            [singleBoneIndex >> XMODEL_BONE_USAGE_SHIFT] |=
            (uint8_t)(1U <<
                      (singleBoneIndex & XMODEL_BONE_USAGE_MASK));
        surface->vertexData.rigidVertices = alloc(
            (size_t)((uint32_t)(int32_t)surface->vertexCount *
                     (uint32_t)sizeof(
                         surface->vertexData.rigidVertices[0])));
        surface->weightedPoints = NULL;
        surface->texCoords = alloc(
            (size_t)((uint32_t)(int32_t)surface->vertexCount *
                     (uint32_t)sizeof(surface->texCoords[0])));
    }

    const uint8_t *stripData = cursor;
    for (int32_t strip = 0; strip < stripCount; ++strip) {
        uint8_t vertexCount = *cursor++;
        cursor += (size_t)vertexCount * sizeof(uint16_t);
    }

    uint8_t *weightedVertexCursor = surface->vertexData.blendPrimaryStream;
    XSurfaceRigidVert *rigidVertex =
        surface->vertexData.rigidVertices;
    for (int32_t vertex = 0; vertex < surface->vertexCount; ++vertex) {
        if (singleBoneIndex == XMODEL_NO_SINGLE_BONE) {
            XSurfaceBlendVert *weightedVertex =
                (XSurfaceBlendVert *)(void *)weightedVertexCursor;
            XSimpleBlendInfo primaryBlend;

            weightedVertex->normal[0] = XModelReadStreamFloat(&cursor);
            weightedVertex->normal[1] = XModelReadStreamFloat(&cursor);
            weightedVertex->normal[2] = XModelReadStreamFloat(&cursor);
            surface->texCoords[vertex][0] = XModelReadStreamFloat(&cursor);
            surface->texCoords[vertex][1] = XModelReadStreamFloat(&cursor);
            weightedVertex->additiveWeightCount =
                XModelReadStreamInt16(&cursor);
            ReadBlend(surface, &primaryBlend, &cursor);
            weightedVertex->blend = primaryBlend;

            if (weightedVertex->additiveWeightCount == 0) {
                weightedVertexCursor +=
                    sizeof(XSurfaceBlendVertNoWeight);
            } else {
                weightedVertex->primaryWeight =
                    XModelReadStreamFloat(&cursor);
                weightedVertexCursor += sizeof(*weightedVertex);
            }
        } else {
            rigidVertex->normal[0] = XModelReadStreamFloat(&cursor);
            rigidVertex->normal[1] = XModelReadStreamFloat(&cursor);
            rigidVertex->normal[2] = XModelReadStreamFloat(&cursor);
            surface->texCoords[vertex][0] = XModelReadStreamFloat(&cursor);
            surface->texCoords[vertex][1] = XModelReadStreamFloat(&cursor);
            rigidVertex->position[0] = XModelReadStreamFloat(&cursor);
            rigidVertex->position[1] = XModelReadStreamFloat(&cursor);
            rigidVertex->position[2] = XModelReadStreamFloat(&cursor);
            ++rigidVertex;
        }
    }

    for (int32_t point = 0; point < weightedPointCount; ++point) {
        ReadBlend(surface, &surface->weightedPoints[point].blend, &cursor);
        surface->weightedPoints[point].weight =
            XModelReadStreamFloat(&cursor);
    }

    qboolean appendPaddingTriangle =
        (surface->triangleCount & 1) != 0 ? qtrue : qfalse;
    if (appendPaddingTriangle != qfalse) {
        /* 0x0049d6cc..0x0049d6e4 updates the stored word before deriving the
         * signed allocation operand and before calling XSurfaceUnstrip. */
        surface->triangleCount = (int16_t)(uint16_t)(
            (uint16_t)surface->triangleCount + 1u);
    }
    int32_t triangleAllocCount = surface->triangleCount;
    surface->triangles = alloc(
        (size_t)((uint32_t)triangleAllocCount *
                 (uint32_t)sizeof(surface->triangles[0])));
    coduo_xmodel_unstrip_serialized(
        stripData, stripCount, surface->triangles);

    if (appendPaddingTriangle != qfalse) {
        uint16_t repeated = surface->triangles[triangleAllocCount - 2][2];
        surface->triangles[triangleAllocCount - 1][0] = repeated;
        surface->triangles[triangleAllocCount - 1][1] = repeated;
        surface->triangles[triangleAllocCount - 1][2] = repeated;
    }
    *position = cursor;
}

/* Sources: CoDUOMP.exe 0x0049ee10..0x0049f099;
 * coduo_lnxded 0x080c4658..0x080c4892.
 * Name: same-module Mac symbol XModelGetBasePose. The scale literal is the
 * exact float stored at original Windows .rdata bits 0x38000100.
 *
 * Windows inlines the quaternion and point transforms while Linux calls the
 * stock shared helpers. Both sides store the packed rotations to binary32
 * before quaternion multiplication and store every helper result to the same
 * binary32 DObjSkelMat fields. Calling the behavior-selected shared helpers
 * therefore preserves each platform's finite arithmetic without duplicating
 * this common control flow. */
void XModelGetBasePose(const XModel *model,
                       DObjSkelMat *basePose)
{
    const XModelPartsData *parts =
        model->info->parts->data.xmodelParts;
    const XModelPartNameTableSlot *slot = parts->partNameTableSlot;
    const int32_t totalPartCount = slot->partNameTable->count;
    const int32_t rootPartCount = parts->rootPartCount;
    const float packedQuatScale =
        3.0518509447574615e-05f; /* It is effectively the binary32 representation of 1.0 / 32767.0. */

    /* NOT_FROM_ORIGINAL_SOURCE: the parts-file prepass proves nonnegative
     * counts with a total no greater than XMODEL_MAX_BONES; retain positive
     * sink countdowns. */
    int32_t remaining = rootPartCount;
    int32_t partIndex = 0;
    if (remaining > 0) {
        do {
            basePose[partIndex].axis[0][0] = 0.0f;
            basePose[partIndex].axis[0][1] = 0.0f;
            basePose[partIndex].axis[0][2] = 0.0f;
            basePose[partIndex].axis[0][3] = 1.0f;
            ++partIndex;
            --remaining;
        } while (remaining > 0);
    }

    remaining = totalPartCount - rootPartCount;
    partIndex = rootPartCount;
    int32_t childIndex = 0;
    if (remaining > 0) {
        do {
            vec4_t localRotation;

            /* Both targets feed each signed int16 through FILD, multiply by
             * the binary32 scale, and store the lane to binary32. */
            localRotation[0] =
                parts->baseRotations[childIndex].components[0] *
                packedQuatScale;
            localRotation[1] =
                parts->baseRotations[childIndex].components[1] *
                packedQuatScale;
            localRotation[2] =
                parts->baseRotations[childIndex].components[2] *
                packedQuatScale;
            localRotation[3] =
                parts->baseRotations[childIndex].components[3] *
                packedQuatScale;

            QuatMultiply(localRotation,
                         basePose[partIndex -
                                  slot->parentPartDeltas[childIndex]]
                             .axis[0],
                         basePose[partIndex].axis[0]);
            ++childIndex;
            ++partIndex;
            --remaining;
        } while (remaining > 0);
    }

    remaining = rootPartCount;
    partIndex = 0;
    if (remaining > 0) {
        do {
            basePose[partIndex].axis[0][0] = 1.0f;
            basePose[partIndex].axis[0][1] = 0.0f;
            basePose[partIndex].axis[0][2] = 0.0f;
            basePose[partIndex].axis[1][0] = 0.0f;
            basePose[partIndex].axis[1][1] = 1.0f;
            basePose[partIndex].axis[1][2] = 0.0f;
            basePose[partIndex].axis[2][0] = 0.0f;
            basePose[partIndex].axis[2][1] = 0.0f;
            basePose[partIndex].axis[2][2] = 1.0f;
            basePose[partIndex].origin[0] = 0.0f;
            basePose[partIndex].origin[1] = 0.0f;
            basePose[partIndex].origin[2] = 0.0f;
            ++partIndex;
            --remaining;
        } while (remaining > 0);
    }

    remaining = totalPartCount - rootPartCount;
    partIndex = rootPartCount;
    childIndex = 0;
    if (remaining > 0) {
        do {
            DObjSkelMat *pose = &basePose[partIndex];
            const DObjSkelMat *parent =
                &basePose[partIndex -
                          slot->parentPartDeltas[childIndex]];

            XModelExpandQuatToAxis(pose->axis[0]);
            XSurfaceTransformPoint43(parts->baseTranslations[childIndex],
                                     parent, pose->origin);
            ++childIndex;
            ++partIndex;
            --remaining;
        } while (remaining > 0);
    }
}

/* Sources: CoDUOMP.exe 0x0049f0a0..0x0049f40f;
 * coduo_lnxded 0x080c4892..0x080c4cb2.
 * Name: same-module Mac symbol XModelTraceLine. Constants preserve the exact
 * original float encodings 0x3e000000, 0xba83126f, and 0x3f8020c5. */
#if defined(WINDOWS_BEHAVIOR)
int32_t XModelTraceLine(const XModel *model,
                        trace_t *trace,
                        const DObjSkelMat *basePose,
                        const vec3_t start, const vec3_t end,
                        int32_t contentsMask)
{
    const XModelInfo *collision = model->info;
    const float planeOffsetEpsilon = 0.125f;
    const float barycentricMin = -0.0010000000474974513f;
    const float barycentricMax = 1.0010000467300415f;
    int32_t hitPart = XMODEL_TRACE_NO_HIT;

    for (int32_t surfaceIndex = 0;
         surfaceIndex < collision->collisionSurfaceCount;
         ++surfaceIndex) {
        const XModelCollSurf *surface =
            &collision->collisionSurfaces[surfaceIndex];
        if ((contentsMask & surface->contents) == 0) {
            continue;
        }

        const DObjSkelMat *pose =
            &basePose[surface->basePoseIndex];
        vec3_t offsetStart = {
            start[0] - pose->origin[0],
            start[1] - pose->origin[1],
            start[2] - pose->origin[2],
        };
        vec3_t offsetEnd = {
            end[0] - pose->origin[0],
            end[1] - pose->origin[1],
            end[2] - pose->origin[2],
        };
        vec3_t localStart;
        vec3_t localEnd;
        XSurfaceTransformVectorRows43(offsetStart, pose, localStart);
        XSurfaceTransformVectorRows43(offsetEnd, pose, localEnd);

        if (CM_TraceLineSkipsBox(
                localStart, localEnd, surface->expandedMins,
                surface->expandedMaxs, trace->fraction) != qfalse) {
            continue;
        }

        vec3_t delta = {
            localEnd[0] - localStart[0],
            localEnd[1] - localStart[1],
            localEnd[2] - localStart[2],
        };
        for (int32_t facetIndex = 0;
             facetIndex < surface->numCollTris;
             ++facetIndex) {
            const XModelCollTri *facet =
                &surface->collTris[facetIndex];
            const XModelCollTriPlane *plane0 =
                &facet->planes[0];
            /* 0x0049f200..0x0049f224 accumulates X + Z + Y, stores a float
             * copy, and tests the retained x87 distance against zero. */
            const long double endDistanceRaw =
                ((long double)localEnd[0] *
                     (long double)plane0->normal[0] +
                 (long double)localEnd[2] *
                     (long double)plane0->normal[2]) +
                    (long double)localEnd[1] *
                        (long double)plane0->normal[1] -
                (long double)plane0->distance;
            float endDistance = (float)endDistanceRaw;
            if (endDistanceRaw >= (long double)0.0f) {
                continue;
            }

            /* 0x0049f235..0x0049f254 repeats the same retained chain for the
             * start point. */
            const long double startDistanceRaw =
                ((long double)localStart[0] *
                     (long double)plane0->normal[0] +
                 (long double)localStart[2] *
                     (long double)plane0->normal[2]) +
                    (long double)localStart[1] *
                        (long double)plane0->normal[1] -
                (long double)plane0->distance;
            float startDistance = (float)startDistanceRaw;
            if (startDistanceRaw <= (long double)0.0f) {
                continue;
            }

            const long double distanceDenominator =
                (long double)startDistance - (long double)endDistance;
            /* 0x0049f265..0x0049f284 stores hitFraction, but compares its
             * retained quotient against the current trace fraction. */
            const long double hitFractionRaw =
                ((long double)startDistance -
                 (long double)planeOffsetEpsilon) /
                distanceDenominator;
            float hitFraction = (float)hitFractionRaw;
            if (hitFractionRaw >= (long double)trace->fraction) {
                continue;
            }

            /* After the hit-fraction compare, 0x0049f28a retains the shared
             * denominator and produces an unrounded plane fraction. X and Y
             * hit coordinates are rounded; Z remains live at 0x0049f2b2. */
            const long double planeFractionRaw =
                (long double)startDistance / distanceDenominator;
            vec3_t hitPoint;
            hitPoint[0] = (float)(
                (long double)delta[0] * planeFractionRaw +
                (long double)localStart[0]);
            hitPoint[1] = (float)(
                (long double)delta[1] * planeFractionRaw +
                (long double)localStart[1]);
            const long double hitPointZRaw =
                (long double)delta[2] * planeFractionRaw +
                (long double)localStart[2];
            hitPoint[2] = (float)hitPointZRaw;
            const XModelCollTriPlane *plane1 =
                &facet->planes[1];
            /* 0x0049f2b6..0x0049f2f2 uses the retained Z for this plane,
             * tests the retained result against the lower bound, then reloads
             * the rounded float copy for the upper bound. */
            const long double barycentric0Raw =
                ((hitPointZRaw * (long double)plane1->normal[2] +
                  (long double)hitPoint[1] *
                      (long double)plane1->normal[1]) +
                 (long double)hitPoint[0] *
                     (long double)plane1->normal[0]) -
                (long double)plane1->distance;
            float barycentric0 = (float)barycentric0Raw;
            if (barycentric0Raw < (long double)barycentricMin ||
                barycentric0 > barycentricMax) {
                continue;
            }

            const XModelCollTriPlane *plane2 =
                &facet->planes[2];
            /* This second plane reloads the rounded Z copy. Its result stays
             * wide through both the lower-bound and barycentric-sum tests. */
            const long double barycentric1Raw =
                (((long double)hitPoint[2] *
                      (long double)plane2->normal[2] +
                  (long double)hitPoint[1] *
                      (long double)plane2->normal[1]) +
                 (long double)hitPoint[0] *
                     (long double)plane2->normal[0]) -
                (long double)plane2->distance;
            if (barycentric1Raw < (long double)barycentricMin ||
                (long double)barycentric0 + barycentric1Raw >
                    (long double)barycentricMax) {
                continue;
            }

            hitPart = surface->basePoseIndex;
            trace->allsolid = 0;
            trace->startsolid = 0;
            trace->fraction = hitFraction;
            trace->surfaceFlags = surface->surfaceFlags;
            trace->contents = surface->contents;
            trace->normal[0] = plane0->normal[0];
            trace->normal[1] = plane0->normal[1];
            trace->normal[2] = plane0->normal[2];
        }
    }

    if (hitPart < 0) {
        return XMODEL_TRACE_NO_HIT;
    }
    vec3_t worldNormal;
    XSurfaceTransformNormal43(
        trace->normal, &basePose[hitPart], worldNormal);
    trace->normal[0] = worldNormal[0];
    trace->normal[1] = worldNormal[1];
    trace->normal[2] = worldNormal[2];
    return hitPart;
}
#else
/*
 * The Linux compiler retains source-level calls to the shared math
 * primitives and its binary32 spill points affect finite inputs.  Keep this
 * complete body separate from the Windows inlined x87 operation graph.
 */
int32_t
XModelTraceLine(const XModel *model,
                trace_t *traceState,
                const DObjSkelMat *basePose, const vec3_t start,
                const vec3_t end, int32_t contentMask)
{
    const XModel *entry =
        model;
    const XModelInfo *collision = entry->info;
    int32_t hitPart = XMODEL_TRACE_NO_HIT;
    const float planeOffsetEpsilon = 0.125f;
    const float barycentricMin = -0.0010000000474974513f;
    const float barycentricMax = 1.0010000467300415f;

    for (int32_t surfaceIndex = 0;
         surfaceIndex < collision->collisionSurfaceCount; ++surfaceIndex) {
        const XModelCollSurf *surface =
            &collision->collisionSurfaces[surfaceIndex];

        if ((contentMask & surface->contents) == 0) {
            continue;
        }

        const DObjSkelMat *pose =
            &basePose[surface->basePoseIndex];
        vec3_t offsetStart;
        vec3_t offsetEnd;
        vec3_t localStart;
        vec3_t localEnd;

        offsetStart[0] = start[0] - pose->origin[0];
        offsetStart[1] = start[1] - pose->origin[1];
        offsetStart[2] = start[2] - pose->origin[2];
        XSurfaceTransformVectorRows43(offsetStart, pose, localStart);

        offsetEnd[0] = end[0] - pose->origin[0];
        offsetEnd[1] = end[1] - pose->origin[1];
        offsetEnd[2] = end[2] - pose->origin[2];
        XSurfaceTransformVectorRows43(offsetEnd, pose, localEnd);

        if (CM_TraceLineSkipsBox(localStart, localEnd, surface->expandedMins,
                                 surface->expandedMaxs,
                                 traceState->fraction) != qfalse) {
            continue;
        }

        vec3_t delta = {
            localEnd[0] - localStart[0],
            localEnd[1] - localStart[1],
            localEnd[2] - localStart[2],
        };

        for (int32_t facetIndex = 0; facetIndex < surface->numCollTris;
             ++facetIndex) {
            const XModelCollTri *facet =
                &surface->collTris[facetIndex];
            const XModelCollTriPlane *plane0 =
                &facet->planes[0];

            float endDist = localEnd[0] * plane0->normal[0] +
                            localEnd[1] * plane0->normal[1] +
                            localEnd[2] * plane0->normal[2] -
                            plane0->distance;

            if (endDist >= 0.0f) {
                continue;
            }

            float startDist =
                localStart[0] * plane0->normal[0] +
                localStart[1] * plane0->normal[1] +
                localStart[2] * plane0->normal[2] - plane0->distance;

            if (startDist <= 0.0f) {
                continue;
            }

            float hitFraction =
                (startDist - planeOffsetEpsilon) / (startDist - endDist);
            if (hitFraction >= traceState->fraction) {
                continue;
            }

            float planeFraction = startDist / (startDist - endDist);
            vec3_t hitPoint = {
                localStart[0] + delta[0] * planeFraction,
                localStart[1] + delta[1] * planeFraction,
                localStart[2] + delta[2] * planeFraction,
            };

            const XModelCollTriPlane *plane1 =
                &facet->planes[1];
            float bary0 = hitPoint[0] * plane1->normal[0] +
                          hitPoint[1] * plane1->normal[1] +
                          hitPoint[2] * plane1->normal[2] - plane1->distance;
            if (bary0 < barycentricMin || bary0 > barycentricMax) {
                continue;
            }

            const XModelCollTriPlane *plane2 =
                &facet->planes[2];
            float bary1 = hitPoint[0] * plane2->normal[0] +
                          hitPoint[1] * plane2->normal[1] +
                          hitPoint[2] * plane2->normal[2] - plane2->distance;
            if (bary1 < barycentricMin ||
                bary0 + bary1 > barycentricMax) {
                continue;
            }

            hitPart = surface->basePoseIndex;
            traceState->allsolid = 0;
            traceState->startsolid = 0;
            traceState->fraction = hitFraction;
            traceState->surfaceFlags = surface->surfaceFlags;
            traceState->contents = surface->contents;
            traceState->normal[0] = plane0->normal[0];
            traceState->normal[1] = plane0->normal[1];
            traceState->normal[2] = plane0->normal[2];
        }
    }

    if (hitPart < 0) {
        return XMODEL_TRACE_NO_HIT;
    }

    const DObjSkelMat *pose = &basePose[hitPart];
    vec3_t worldNormal;

    XSurfaceTransformNormal43(traceState->normal, pose, worldNormal);
    traceState->normal[0] = worldNormal[0];
    traceState->normal[1] = worldNormal[1];
    traceState->normal[2] = worldNormal[2];

    return hitPart;
}
#endif

/* Sources: CoDUOMP.exe 0x0049f410..0x0049f56f;
 * coduo_lnxded 0x080c4cb2..0x080c4e86.
 * Name: same-module Mac symbol XModelGetStaticBounds. */
#if defined(WINDOWS_BEHAVIOR)
qboolean XModelGetStaticBounds(const XModel *model,
                               axis_t transform,
                               vec3_t mins, vec3_t maxs)
{
    const XModelInfo *collision = model->info;
    if (collision->collisionSurfaceCount == 0) {
        return qfalse;
    }

    mins[0] = FLT_MAX;
    mins[1] = FLT_MAX;
    mins[2] = FLT_MAX;
    maxs[0] = -FLT_MAX;
    maxs[1] = -FLT_MAX;
    maxs[2] = -FLT_MAX;

    for (int32_t surfaceIndex = 0;
         surfaceIndex < collision->collisionSurfaceCount;
         ++surfaceIndex) {
        const XModelCollSurf *surface =
            &collision->collisionSurfaces[surfaceIndex];
        for (int32_t corner = 0;
             corner < XMODEL_BOUNDS_CORNER_COUNT;
             ++corner) {
            vec3_t localCorner = {
                (corner & 1) == 0
                    ? surface->expandedMaxs[0]
                    : surface->expandedMins[0],
                (corner & 2) == 0
                    ? surface->expandedMaxs[1]
                    : surface->expandedMins[1],
                (corner & 4) == 0
                    ? surface->expandedMaxs[2]
                    : surface->expandedMins[2],
            };
            /* 0x0049f4af..0x0049f503 stores all three transformed corners
             * as binary32 before either bounds comparison. */
            vec3_t transformedCorner = {
                (float)(
                    ((long double)localCorner[0] * transform[0][0] +
                     (long double)localCorner[2] * transform[2][0]) +
                    (long double)localCorner[1] * transform[1][0]),
                (float)(
                    ((long double)localCorner[0] * transform[0][1] +
                     (long double)localCorner[2] * transform[2][1]) +
                    (long double)localCorner[1] * transform[1][1]),
                (float)(
                    ((long double)localCorner[2] * transform[2][2] +
                     (long double)localCorner[1] * transform[1][2]) +
                    (long double)localCorner[0] * transform[0][2]),
            };

            for (int32_t axis = 0; axis < 3; ++axis) {
                if (transformedCorner[axis] < mins[axis]) {
                    mins[axis] = transformedCorner[axis];
                }
                if (maxs[axis] < transformedCorner[axis]) {
                    maxs[axis] = transformedCorner[axis];
                }
            }
        }
    }
    return qtrue;
}
#else
/*
 * The Linux compiler retains source-level calls to the shared math
 * primitives and its binary32 spill points affect finite inputs.  Keep this
 * complete body separate from the Windows inlined x87 operation graph.
 */
qboolean XModelGetStaticBounds(const XModel *model,
                               axis_t transform,
                               vec3_t mins, vec3_t maxs)
{
    const XModel *entry =
        model;
    const XModelInfo *collision = entry->info;

    if (collision->collisionSurfaceCount == 0) {
        return qfalse;
    }

    mins[0] = FLT_MAX;
    mins[1] = FLT_MAX;
    mins[2] = FLT_MAX;
    maxs[0] = -FLT_MAX;
    maxs[1] = -FLT_MAX;
    maxs[2] = -FLT_MAX;

    for (int32_t surfaceIndex = 0;
         surfaceIndex < collision->collisionSurfaceCount; ++surfaceIndex) {
        const XModelCollSurf *surface =
            &collision->collisionSurfaces[surfaceIndex];

        for (int32_t corner = 0; corner < XMODEL_BOUNDS_CORNER_COUNT;
             ++corner) {
            vec3_t localCorner;
            vec3_t transformedCorner;

            localCorner[0] =
                (corner & 1) == 0 ? surface->expandedMaxs[0]
                                  : surface->expandedMins[0];
            localCorner[1] =
                (corner & 2) == 0 ? surface->expandedMaxs[1]
                                  : surface->expandedMins[1];
            localCorner[2] =
                (corner & 4) == 0 ? surface->expandedMaxs[2]
                                  : surface->expandedMins[2];

            /* C11 cannot add element qualification through the axis_t array
             * typedef implicitly; the matrix remains read-only. */
            MatrixTransformVector(localCorner,
                                  (const vec3_t *)transform,
                                  transformedCorner);

            for (int32_t axis = 0; axis < 3; ++axis) {
                if (transformedCorner[axis] < mins[axis]) {
                    mins[axis] = transformedCorner[axis];
                }
                if (maxs[axis] < transformedCorner[axis]) {
                    maxs[axis] = transformedCorner[axis];
                }
            }
        }
    }

    return qtrue;
}
#endif

/* Sources: CoDUOMP.exe 0x0049ea40..0x0049ece6;
 * coduo_lnxded 0x080c4342..0x080c4512.
 * Name: same-module Mac symbol XSurfaceGetVerts. */
#if defined(WINDOWS_BEHAVIOR)
void XSurfaceGetVerts(const XSurface *surface,
                      const DObjSkelMat *basePose,
                      vec3_t *outVerts, vec2_t *outTexCoords,
                      vec3_t *outNormals)
{
    /* NOT_FROM_ORIGINAL_SOURCE: keep the sink a no-op for every nonpositive
     * vertex count before copying or starting positive countdowns. */
    if (surface->vertexCount <= 0) {
        return;
    }

    if (outTexCoords != NULL) {
        /* Retail turns a negative authored vertex count into a wrapping
         * target-dword byte count. The nonpositive sink guard above prevents
         * that malformed operand from reaching this retained copy. */
        uint32_t texCoordBytes =
            (uint32_t)(int32_t)surface->vertexCount *
            (uint32_t)sizeof(surface->texCoords[0]);
        memcpy(outTexCoords, surface->texCoords,
               (size_t)texCoordBytes);
    }

    if (surface->boneIndex == XMODEL_NO_SINGLE_BONE) {
        const uint8_t *primaryVertexCursor =
            surface->vertexData.blendPrimaryStream;
        const XSurfaceWeightedPoint *additivePoint =
            surface->weightedPoints;
        vec3_t *outVert = outVerts;
        vec3_t *outNormal = outNormals;

        /* Retail tests the signed count only for zero and then uses a dword
         * countdown. The positive sink guard above preserves every valid
         * iteration while excluding the wrapping negative-count case. */
        int32_t verticesRemaining = surface->vertexCount;
        while (verticesRemaining > 0) {
            const XSurfaceBlendVert *primary =
                (const XSurfaceBlendVert *)(const void *)
                    primaryVertexCursor;
            const DObjSkelMat *primaryPose =
                (const DObjSkelMat *)(const void *)(
                    (const uint8_t *)basePose +
                    primary->blend.boneMatrixOffset);

            if (outNormal != NULL) {
                (*outNormal)[0] = (float)(
                    ((long double)primary->normal[2] *
                         primaryPose->axis[2][0] +
                     (long double)primary->normal[1] *
                         primaryPose->axis[1][0]) +
                    (long double)primary->normal[0] *
                        primaryPose->axis[0][0]);
                (*outNormal)[1] = (float)(
                    ((long double)primary->normal[2] *
                         primaryPose->axis[2][1] +
                     (long double)primary->normal[0] *
                         primaryPose->axis[0][1]) +
                    (long double)primary->normal[1] *
                        primaryPose->axis[1][1]);
                (*outNormal)[2] = (float)(
                    ((long double)primary->normal[2] *
                         primaryPose->axis[2][2] +
                     (long double)primary->normal[0] *
                         primaryPose->axis[0][2]) +
                    (long double)primary->normal[1] *
                        primaryPose->axis[1][2]);
                ++outNormal;
            }
            const float *primaryPoint = primary->blend.position;
            (*outVert)[0] = (float)(
                (((long double)primaryPoint[2] *
                      primaryPose->axis[2][0] +
                  (long double)primaryPoint[1] *
                      primaryPose->axis[1][0]) +
                 (long double)primaryPoint[0] *
                     primaryPose->axis[0][0]) +
                (long double)primaryPose->origin[0]);
            (*outVert)[1] = (float)(
                (((long double)primaryPoint[0] *
                      primaryPose->axis[0][1] +
                  (long double)primaryPoint[2] *
                      primaryPose->axis[2][1]) +
                 (long double)primaryPoint[1] *
                     primaryPose->axis[1][1]) +
                (long double)primaryPose->origin[1]);
            (*outVert)[2] = (float)(
                (((long double)primaryPoint[0] *
                      primaryPose->axis[0][2] +
                  (long double)primaryPoint[2] *
                      primaryPose->axis[2][2]) +
                 (long double)primaryPoint[1] *
                     primaryPose->axis[1][2]) +
                (long double)primaryPose->origin[2]);

            if (primary->additiveWeightCount > 0) {
                (*outVert)[0] *= primary->primaryWeight;
                (*outVert)[1] *= primary->primaryWeight;
                (*outVert)[2] *= primary->primaryWeight;

                int32_t additiveWeightsRemaining =
                    primary->additiveWeightCount;
                /* The original loop is a nonzero dword countdown. Loader
                 * validation proves this authored count nonnegative before
                 * the retained do-while is entered. */
                do {
                    const DObjSkelMat *additivePose =
                        (const DObjSkelMat *)(const void *)(
                            (const uint8_t *)basePose +
                            additivePoint->blend.boneMatrixOffset);
                    const vec3_t point = {
                        additivePoint->blend.position[0],
                        additivePoint->blend.position[1],
                        additivePoint->blend.position[2]
                    };
                    const long double weight = additivePoint->weight;
                    (*outVert)[0] = (float)(
                        ((((long double)point[1] *
                               additivePose->axis[1][0] +
                           (long double)point[2] *
                               additivePose->axis[2][0]) +
                          (long double)point[0] *
                              additivePose->axis[0][0]) +
                         (long double)additivePose->origin[0]) *
                            weight +
                        (long double)(*outVert)[0]);
                    (*outVert)[1] = (float)(
                        ((((long double)point[1] *
                               additivePose->axis[1][1] +
                           (long double)point[0] *
                               additivePose->axis[0][1]) +
                          (long double)point[2] *
                              additivePose->axis[2][1]) +
                         (long double)additivePose->origin[1]) *
                            weight +
                        (long double)(*outVert)[1]);
                    (*outVert)[2] = (float)(
                        ((((long double)point[1] *
                               additivePose->axis[1][2] +
                           (long double)point[0] *
                               additivePose->axis[0][2]) +
                          (long double)point[2] *
                              additivePose->axis[2][2]) +
                         (long double)additivePose->origin[2]) *
                            weight +
                        (long double)(*outVert)[2]);
                    ++additivePoint;
                    --additiveWeightsRemaining;
                } while (additiveWeightsRemaining > 0);
                primaryVertexCursor += sizeof(*primary);
            } else {
                primaryVertexCursor +=
                    sizeof(XSurfaceBlendVertNoWeight);
            }
            ++outVert;
            --verticesRemaining;
        }
        return;
    }

    const DObjSkelMat *rigidPose =
        (const DObjSkelMat *)(const void *)(
            (const uint8_t *)basePose + surface->boneIndex);
    const XSurfaceRigidVert *rigidVertex =
        surface->vertexData.rigidVertices;
    vec3_t *outVert = outVerts;
    vec3_t *outNormal = outNormals;
    /* The rigid retail stream uses the same nonzero dword countdown. The
     * positive function-entry guard bounds it without changing valid input. */
    int32_t verticesRemaining = surface->vertexCount;
    while (verticesRemaining > 0) {
        if (outNormal != NULL) {
            const float *normal = rigidVertex->normal;
            (*outNormal)[0] = (float)(
                ((long double)normal[1] * rigidPose->axis[1][0] +
                 (long double)normal[0] * rigidPose->axis[0][0]) +
                (long double)normal[2] * rigidPose->axis[2][0]);
            (*outNormal)[1] = (float)(
                ((long double)normal[2] * rigidPose->axis[2][1] +
                 (long double)normal[1] * rigidPose->axis[1][1]) +
                (long double)normal[0] * rigidPose->axis[0][1]);
            (*outNormal)[2] = (float)(
                ((long double)normal[2] * rigidPose->axis[2][2] +
                 (long double)normal[1] * rigidPose->axis[1][2]) +
                (long double)normal[0] * rigidPose->axis[0][2]);
            ++outNormal;
        }
        const float *point = rigidVertex->position;
        (*outVert)[0] = (float)(
            (((long double)point[1] * rigidPose->axis[1][0] +
              (long double)point[2] * rigidPose->axis[2][0]) +
             (long double)point[0] * rigidPose->axis[0][0]) +
            (long double)rigidPose->origin[0]);
        (*outVert)[1] = (float)(
            (((long double)point[0] * rigidPose->axis[0][1] +
              (long double)point[2] * rigidPose->axis[2][1]) +
             (long double)point[1] * rigidPose->axis[1][1]) +
            (long double)rigidPose->origin[1]);
        (*outVert)[2] = (float)(
            (((long double)point[0] * rigidPose->axis[0][2] +
              (long double)point[2] * rigidPose->axis[2][2]) +
             (long double)point[1] * rigidPose->axis[1][2]) +
            (long double)rigidPose->origin[2]);
        ++rigidVertex;
        ++outVert;
        --verticesRemaining;
    }
}
#else
/*
 * The Linux compiler retains source-level calls to the shared math
 * primitives and its binary32 spill points affect finite inputs.  Keep this
 * complete body separate from the Windows inlined x87 operation graph.
 */
void XSurfaceGetVerts(const XSurface *surface,
                      const DObjSkelMat *basePose,
                      vec3_t *outVerts, vec2_t *outTexCoords,
                      vec3_t *outNormals)
{
    /* NOT_FROM_ORIGINAL_SOURCE: keep the sink a no-op for every nonpositive
     * vertex count before copying or starting positive countdowns. */
    if (surface->vertexCount <= 0) {
        return;
    }

    if (outTexCoords != NULL) {
        /* The original target-dword multiplication remains safe because the
         * nonpositive sink guard has excluded its wrapping input domain. */
        memcpy(outTexCoords, surface->texCoords,
               (size_t)((uint32_t)(int32_t)surface->vertexCount *
                        (uint32_t)sizeof(surface->texCoords[0])));
    }

    /* Both Linux vertex paths and every nonzero additive-weight path use
     * signed nonzero dword countdowns. Loader validation and the function-
     * entry guard bound those retained loops. */
    if (XSurfaceGetBoneIndex(surface) == XMODEL_NO_SINGLE_BONE) {
        const uint8_t *primaryVertexCursor =
            surface->vertexData.blendPrimaryStream;
        const XSurfaceWeightedPoint *additiveVerts =
            (const XSurfaceWeightedPoint *)
                surface->weightedPoints;

        int32_t remainingVertices = surface->vertexCount;
        int32_t vertexIndex = 0;
        while (remainingVertices > 0) {
            const XSurfaceBlendVert *primary =
                (const XSurfaceBlendVert *)
                    primaryVertexCursor;
            const DObjSkelMat *primaryPose =
                (const DObjSkelMat
                     *)((const uint8_t *)basePose +
                        primary->blend.boneMatrixOffset);

            if (outNormals != NULL) {
                XSurfaceTransformNormal43(primary->normal, primaryPose,
                                          outNormals[vertexIndex]);
            }

            XSurfaceTransformPoint43(primary->blend.position, primaryPose,
                                     outVerts[vertexIndex]);

            if (primary->additiveWeightCount > 0) {
                outVerts[vertexIndex][0] *= primary->primaryWeight;
                outVerts[vertexIndex][1] *= primary->primaryWeight;
                outVerts[vertexIndex][2] *= primary->primaryWeight;

                int32_t remainingWeights = primary->additiveWeightCount;
                do {
                    const XSurfaceWeightedPoint *additive =
                        additiveVerts++;
                    const DObjSkelMat *additivePose =
                        (const DObjSkelMat *)(
                            (const uint8_t *)basePose +
                            additive->blend.boneMatrixOffset);

                    XSurfaceAccumulateWeightedPoint43(
                        additive->blend.position, additive->weight,
                        additivePose, outVerts[vertexIndex]);
                    --remainingWeights;
                } while (remainingWeights > 0);

                primaryVertexCursor +=
                    sizeof(XSurfaceBlendVert);
            } else {
                primaryVertexCursor +=
                    sizeof(XSurfaceBlendVertNoWeight);
            }

            ++vertexIndex;
            --remainingVertices;
        }

        return;
    }

    const DObjSkelMat *rigidPose =
        (const DObjSkelMat *)((const uint8_t *)basePose +
                                           surface->boneIndex);
    const XSurfaceRigidVert *rigidVerts =
        surface->vertexData.rigidVertices;

    int32_t remainingVertices = surface->vertexCount;
    int32_t vertexIndex = 0;
        while (remainingVertices > 0) {
        if (outNormals != NULL) {
            XSurfaceTransformNormal43(rigidVerts[vertexIndex].normal,
                                      rigidPose, outNormals[vertexIndex]);
        }

        XSurfaceTransformPoint43(rigidVerts[vertexIndex].position,
                                 rigidPose, outVerts[vertexIndex]);
        ++vertexIndex;
        --remainingVertices;
    }
}
#endif

/* Source: CoDUOMP.exe 0x0049cd50..0x0049cd55.
 * Name: exact same-module Mac symbol XModelEnforceExist. The destination is
 * the cl_xmodelcheck value read by XModel precache paths. */
void XModelEnforceExist(qboolean enforce)
{
    xmodel_enforceExist = enforce;
}

/* Source: CoDUOMP.exe 0x0049cd60..0x0049cd6e.
 * Name: exact same-module Mac symbol XModelBad. The standalone body compares
 * model->info with the process-wide default collision payload at
 * 0x00b8d3bc. */
qboolean XModelBad(const XModel *model)
{
    return model->info == &xmodel_defaultCollision
               ? qtrue
               : qfalse;
}

/* Source: CoDUOMP.exe 0x0049cd70..0x0049cd75.
 * Name: same-module Mac symbol XModelSetOptimize.  XModelPrecache consumes the
 * stored flag before preprocessing loaded surface data. */
void XModelSetOptimize(qboolean enabled)
{
    xmodel_optimizeEnabled = enabled;
}

/* Source: CoDUOMP.exe 0x0049d770..0x0049d7fe.
 * Name: exact same-module Mac symbol XModelCreateDefaultSurface. */
void XModelCreateDefaultSurface(XSurface *surface)
{
    surface->vertexCount = XMODEL_DEFAULT_VERTEX_COUNT;
    surface->triangleCount = XMODEL_DEFAULT_SURFACE_COUNT;
    surface->boneIndex = 0;
    memset(surface->boneUsage, 0, sizeof(surface->boneUsage));
    surface->weightedPoints = NULL;
    surface->triangles = xmodel_defaultTriangles;
    surface->vertexData.rigidVertices = xmodel_defaultRigidVertices;
    surface->texCoords = xmodel_defaultTexCoords;

    xmodel_defaultTriangles[0][0] = 0;
    xmodel_defaultTriangles[0][1] = 1;
    xmodel_defaultTriangles[0][2] = 2;

    for (int32_t vertex = 0; vertex < surface->vertexCount; ++vertex) {
        surface->vertexData.rigidVertices[vertex].normal[0] = 1.0f;
        surface->vertexData.rigidVertices[vertex].normal[1] = 0.0f;
        surface->vertexData.rigidVertices[vertex].normal[2] = 0.0f;
        surface->texCoords[vertex][0] = 0.0f;
        surface->texCoords[vertex][1] = 0.0f;
        surface->vertexData.rigidVertices[vertex].position[0] = 0.0f;
        surface->vertexData.rigidVertices[vertex].position[1] = 0.0f;
        surface->vertexData.rigidVertices[vertex].position[2] = 0.0f;
    }
}

/* Source: CoDUOMP.exe 0x0049d800..0x0049d8ac.
 * Name: exact same-module Mac symbol XModelCreateDefaultParts. */
fileData_t *XModelCreateDefaultParts(void)
{
    xmodel_defaultPartNameTable.count = XMODEL_DEFAULT_BONE_COUNT;
    xmodel_defaultPartNameTable.handles[0] = 0;
    xmodel_defaultPartNameSlot.partNameTable = &xmodel_defaultPartNameTable;
    xmodel_defaultPartStateIndices[0] = 0;

    xmodel_defaultParts.partNameTableSlot = &xmodel_defaultPartNameSlot;
    xmodel_defaultParts.rootPartCount = XMODEL_DEFAULT_BONE_COUNT;
    xmodel_defaultParts.baseRotations = NULL;
    xmodel_defaultParts.baseTranslations = NULL;
    xmodel_defaultParts.partStateIndices =
        xmodel_defaultPartStateIndices;

    xmodel_defaultPartCollision.mins[0] = -16.0f;
    xmodel_defaultPartCollision.mins[1] = -16.0f;
    xmodel_defaultPartCollision.mins[2] = -16.0f;
    xmodel_defaultPartCollision.maxs[0] = 16.0f;
    xmodel_defaultPartCollision.maxs[1] = 16.0f;
    xmodel_defaultPartCollision.maxs[2] = 16.0f;
    xmodel_defaultParts.partCollisions = &xmodel_defaultPartCollision;

    xmodel_defaultPartsEntry.name = xmodel_defaultName;
    xmodel_defaultPartsEntry.data.xmodelParts = &xmodel_defaultParts;
    xmodel_defaultPartsEntry.freeData = NULL;
    return &xmodel_defaultPartsEntry;
}

/* Source: CoDUOMP.exe 0x0049d8b0..0x0049d8f8, exporter-gap recovery.
 * Name: exact same-module Mac symbol XModelCreateDefaultSurfs. */
XModelSurfs *XModelCreateDefaultSurfs(void)
{
    xmodel_defaultSurfs.surfaceCount = XMODEL_DEFAULT_SURFACE_COUNT;
    xmodel_defaultSurfacePtrs[0] = &xmodel_defaultSurface;
    xmodel_defaultSurfs.surfaces = xmodel_defaultSurfacePtrs;
    XModelCreateDefaultSurface(&xmodel_defaultSurface);

    xmodel_defaultSurfsEntry.name = xmodel_defaultName;
    xmodel_defaultSurfsEntry.surfs = &xmodel_defaultSurfs;
    xmodel_defaultSurfsEntry.freeData = NULL;
    return &xmodel_defaultSurfsEntry;
}

/* Source: CoDUOMP.exe 0x0049d900..0x0049d9a8.
 * Name: same-module Mac overload XModelCreateDefault(XModel_s *). */
void XModelCreateDefault(XModel *model)
{
    xmodel_defaultCollision.parts = XModelCreateDefaultParts();

    for (int32_t lodIndex = 0; lodIndex < XMODEL_LOD_COUNT; ++lodIndex) {
        XModelLodInfo *lod =
            &xmodel_defaultCollision.lodRecords[lodIndex];
        lod->surfs = NULL;
        lod->name = xmodel_emptyName;
        lod->distance = 0.0f;
        lod->surfaceCount = XMODEL_DEFAULT_SURFACE_COUNT;
        lod->surfaceNameTable = xmodel_defaultSurfaceNames;
        xmodel_defaultSurfaceNames[0] = 0;
    }

    xmodel_defaultCollision.lodRecords[0].surfs =
        XModelCreateDefaultSurfs();
    model->info = &xmodel_defaultCollision;
    model->freeData = NULL;
    xmodel_defaultCollision.lodCount = XMODEL_DEFAULT_LOD_COUNT;
    xmodel_defaultCollision.modelFileCount = 0;
}

/* Source: CoDUOMP.exe 0x0049d9b0..0x0049d9cc, exporter-gap recovery.
 * Name: same-module Mac overload XModelCreateDefault(void). The trailing
 * underscore is the C spelling that distinguishes it from the pointer-taking
 * overload above; it is not a compatibility alias. */
XModel *XModelCreateDefault_(void)
{
    XModelCreateDefault(&xmodel_defaultModelEntry);
    xmodel_defaultModelEntry.name = xmodel_defaultName;
    return &xmodel_defaultModelEntry;
}

/* Source: CoDUOMP.exe 0x0049d9d0..0x0049da26.
 * Name and clone ownership match the reconstructed Linux engine function. */
XModelSurfs *XModelSurfsCloneSurfs(const XModelSurfs *modelSurfs,
                                   void *(*alloc)(size_t size))
{
    const XModelSurfsData *source = modelSurfs->surfs;
    XModelSurfsData *clone = alloc(sizeof(*clone));

    clone->surfaceCount = source->surfaceCount;
    clone->surfaces = alloc(
        (size_t)((uint32_t)(int32_t)clone->surfaceCount *
                 (uint32_t)sizeof(clone->surfaces[0])));
    memcpy(clone->surfaces, source->surfaces,
           (size_t)((uint32_t)(int32_t)clone->surfaceCount *
                    (uint32_t)sizeof(clone->surfaces[0])));
    clone->next = xmodel_surfsCloneHead;
    xmodel_surfsCloneHead = clone;

    XModelSurfs *entry = alloc(sizeof(*entry));
    entry->name = modelSurfs->name;
    entry->surfs = clone;
    entry->freeData = XModelSurfsFree;
    return entry;
}

/* Source: CoDUOMP.exe 0x0049da30..0x0049dc2e. */
XModelSurfs *XModelSurfsPrecache(const char *name,
                                 void *(*alloc)(size_t size))
{
    XModelSurfs *entry =
        (XModelSurfs *)FS_GetDataForFile("xmodelsurfs", name, "");

    if (entry == NULL) {
        if (xmodel_enforceExist == qfalse) {
            Com_Printf("ERROR: Cannot precache 'xmodelsurfs/%s'", name);
        } else {
            Com_Error(ERR_DROP,
                      "\x15" "Cannot precache 'xmodelsurfs/%s'.\n"
                      "you may need to get latest and run converter to fix",
                      name);
        }
        return NULL;
    }

    if (entry->surfs != NULL) {
        return XModelSurfsCloneSurfs(entry, alloc);
    }

    char path[XMODEL_PATH_BUFFER_SIZE];
    /* NOT_FROM_ORIGINAL_SOURCE: require the complete surface path and NUL to
     * fit; never substitute a truncated asset name. */
    if (strlen(name) > sizeof(path) - sizeof("xmodelsurfs/")) {
        Com_Error(ERR_DROP, "\x15" "XModel surface path is too long");
        return NULL;
    }
    Com_sprintf(path, sizeof(path), "xmodelsurfs/%s", name);
    void *fileBuffer;
    const int32_t fileLength = FS_ReadFile(path, &fileBuffer);
    if (fileLength < 0) {
        Com_Error(ERR_DROP,
                  "\x15" "Cannot find 'xmodelsurfs/%s'.", name);
        return NULL;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate the complete packed surface stream
     * before entering the retained pointer-only parser. */
    if (coduo_xmodel_validate_surfs_file(
            fileBuffer, (size_t)fileLength) == qfalse) {
        FS_FreeFile(fileBuffer);
        Com_Error(ERR_DROP,
                  "\x15" "Malformed xmodelsurfs asset '%s'", name);
        return NULL;
    }

    const uint8_t *cursor = fileBuffer;
    int16_t version = XModelReadStreamInt16(&cursor);
    if (version != XMODEL_ASSET_VERSION) {
        FS_FreeFile(fileBuffer);
        Com_Error(ERR_DROP,
                  "\x15" "xmodelsurfs '%s' out of date (version %d, "
                  "expecting %d)",
                  name, version, XMODEL_ASSET_VERSION);
        return NULL;
    }

    int16_t surfaceCount = XModelReadStreamInt16(&cursor);
    XModelSurfsData *surfs = alloc(sizeof(*surfs));
    surfs->surfaceCount = surfaceCount;
    surfs->surfaces = alloc(
        (size_t)((uint32_t)(int32_t)surfaceCount *
                 (uint32_t)sizeof(surfs->surfaces[0])));
    XSurface *surfaces =
        alloc((size_t)((uint32_t)(int32_t)surfaceCount *
                       (uint32_t)sizeof(surfaces[0])));

    for (int32_t surfaceIndex = 0;
         surfaceIndex < surfaceCount;
         ++surfaceIndex) {
        surfs->surfaces[surfaceIndex] = &surfaces[surfaceIndex];
        XModelReadSurface(&surfaces[surfaceIndex], &cursor, alloc);
    }

    FS_FreeFile(fileBuffer);
    entry->surfs = surfs;
    entry->freeData = XModelSurfsFree;
    surfs->next = xmodel_surfsCloneHead;
    xmodel_surfsCloneHead = surfs;
    return entry;
}

/* Source: CoDUOMP.exe 0x0049dc30..0x0049dff4. */
fileData_t *XModelPartsPrecache(const char *name,
                                void *(*alloc)(size_t size))
{
    fileData_t *entry =
        FS_GetDataForFile("xmodelparts", name, "");

    if (entry == NULL) {
        if (xmodel_enforceExist == qfalse) {
            Com_Printf("ERROR: Cannot precache 'xmodelparts/%s'", name);
        } else {
            Com_Error(ERR_DROP,
                      "\x15" "Cannot precache 'xmodelparts/%s'.\n"
                      "you may need to get latest and run converter to fix",
                      name);
        }
        return NULL;
    }
    if (entry->data.generic != NULL) {
        return entry;
    }

    char path[XMODEL_PATH_BUFFER_SIZE];
    /* NOT_FROM_ORIGINAL_SOURCE: require the complete parts path and NUL to
     * fit; never substitute a truncated asset name. */
    if (strlen(name) > sizeof(path) - sizeof("xmodelparts/")) {
        Com_Error(ERR_DROP, "\x15" "XModel parts path is too long");
        return NULL;
    }
    Com_sprintf(path, sizeof(path), "xmodelparts/%s", name);
    void *fileBuffer;
    const int32_t fileLength = FS_ReadFile(path, &fileBuffer);
    if (fileLength < 0) {
        Com_Error(ERR_DROP,
                  "\x15" "Cannot find 'xmodelparts/%s'.", name);
        return NULL;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate the complete parts stream, count
     * domains, links, names, collision records, and state tail before the
     * retained pointer-only parser. */
    if (coduo_xmodel_validate_parts_file(
            fileBuffer, (size_t)fileLength) == qfalse) {
        FS_FreeFile(fileBuffer);
        Com_Error(ERR_DROP,
                  "\x15" "Malformed xmodelparts asset '%s'", name);
        return NULL;
    }

    const uint8_t *cursor = fileBuffer;
    int16_t version = XModelReadStreamInt16(&cursor);
    if (version != XMODEL_ASSET_VERSION) {
        FS_FreeFile(fileBuffer);
        Com_Error(ERR_DROP,
                  "\x15" "xmodelparts '%s' out of date (version %d, "
                  "expecting %d)",
                  name, version, XMODEL_ASSET_VERSION);
        return NULL;
    }

    int16_t childPartCount = XModelReadStreamInt16(&cursor);
    int16_t rootPartCount = XModelReadStreamInt16(&cursor);
    uint16_t totalPartCountBits =
        (uint16_t)((uint16_t)rootPartCount + (uint16_t)childPartCount);
    int16_t totalPartCount;
    memcpy(&totalPartCount, &totalPartCountBits, sizeof(totalPartCount));
    uint32_t partNameTableBytes =
        (uint32_t)(sizeof(XModelPartNameTable) -
                   sizeof(((XModelPartNameTable *)0)->handles[0])) +
        (uint32_t)(int32_t)totalPartCount *
            (uint32_t)sizeof(((XModelPartNameTable *)0)->handles[0]);
    XModelPartNameTable *partNameTable =
        alloc((size_t)partNameTableBytes);
    partNameTable->count = totalPartCount;

    if (totalPartCount > XMODEL_MAX_BONES) {
        Com_Error(ERR_DROP,
                  "\x15" "xmodel '%s' has more than %d bones",
                  name, XMODEL_MAX_BONES);
        return NULL;
    }

    uint32_t partNameSlotBytes =
        (uint32_t)(sizeof(XModelPartNameTableSlot) -
                   sizeof(((XModelPartNameTableSlot *)0)
                              ->parentPartDeltas[0])) +
        (uint32_t)(int32_t)childPartCount *
            (uint32_t)sizeof(((XModelPartNameTableSlot *)0)
                                 ->parentPartDeltas[0]);
    XModelPartNameTableSlot *slot = alloc((size_t)partNameSlotBytes);
    slot->partNameTable = partNameTable;

    uint32_t childPartCountBits = (uint32_t)(int32_t)childPartCount;
    uint32_t rotationBytes =
        childPartCountBits * (uint32_t)sizeof(xanim_int16_vec4_t);
    uint32_t translationBytes =
        childPartCountBits * (uint32_t)sizeof(vec3_t);
    /* CoDUOMP.exe 0x0049dddd, coduo_lnxded 0x080c2d3e, and the Mac client
     * 0x000f5148 all request 0x68 + childCount * 0x18 bytes. The loaders write
     * only the 0x18-byte header, childCount packed rotations, and childCount
     * translations. Audited free, base-pose, XAnim, and DObj consumers reach
     * those arrays solely through header pointers and never address the
     * remainder. It is therefore a stock allocation tail unused by this
     * object, not hidden XModelPartsData fields. Preserve it because the
     * allocator cursor is observable, while letting the real pointer-bearing
     * header grow natively. */
    uint32_t stockI386UnusedAllocationBytes =
        (uint32_t)(0x68 -
                   sizeof(xmodel_parts_data_stock_i386_layout_t)) +
        childPartCountBits *
            (uint32_t)(0x18 - sizeof(xanim_int16_vec4_t) -
                       sizeof(vec3_t));
    uint32_t partsAllocationBytes =
        (uint32_t)sizeof(XModelPartsData) + rotationBytes +
        translationBytes + stockI386UnusedAllocationBytes;
    XModelPartsData *parts = alloc((size_t)partsAllocationBytes);

    parts->partNameTableSlot = slot;
    parts->rootPartCount = rootPartCount;
    parts->baseRotations = (xanim_int16_vec4_t *)(parts + 1);
    parts->baseTranslations = childPartCount != 0
        ? (vec3_t *)((uint8_t *)parts->baseRotations +
                     (intptr_t)childPartCount *
                         (intptr_t)sizeof(parts->baseRotations[0]))
        : NULL;
    parts->partStateIndices =
        alloc((size_t)(uint32_t)(int32_t)totalPartCount);
    parts->partCollisions = alloc(
        (size_t)((uint32_t)(int32_t)totalPartCount *
                 (uint32_t)sizeof(parts->partCollisions[0])));

    for (int32_t partIndex = rootPartCount;
         partIndex < totalPartCount;
         ++partIndex) {
        int32_t childIndex = partIndex - rootPartCount;
        slot->parentPartDeltas[childIndex] =
            (uint8_t)(partIndex - *cursor++);
        parts->baseTranslations[childIndex][0] =
            XModelReadStreamFloat(&cursor);
        parts->baseTranslations[childIndex][1] =
            XModelReadStreamFloat(&cursor);
        parts->baseTranslations[childIndex][2] =
            XModelReadStreamFloat(&cursor);

        int16_t packedRotation[3];
        packedRotation[0] = XModelReadStreamInt16(&cursor);
        packedRotation[1] = XModelReadStreamInt16(&cursor);
        packedRotation[2] = XModelReadStreamInt16(&cursor);
        ReadQuat(
            packedRotation, &parts->baseRotations[childIndex]);
    }

    for (int32_t partIndex = 0;
         partIndex < totalPartCount;
         ++partIndex) {
        size_t partNameSize = strlen((const char *)cursor) + 1U;
        partNameTable->handles[partIndex] = SL_GetStringOfLen(
            (const char *)cursor, 0, partNameSize,
            XMODEL_PART_NAME_STRING_TYPE);
        cursor += partNameSize;

        XModelPartColl *collision =
            &parts->partCollisions[partIndex];
        collision->mins[0] = XModelReadStreamFloat(&cursor);
        collision->mins[1] = XModelReadStreamFloat(&cursor);
        collision->mins[2] = XModelReadStreamFloat(&cursor);
        collision->maxs[0] = XModelReadStreamFloat(&cursor);
        collision->maxs[1] = XModelReadStreamFloat(&cursor);
        collision->maxs[2] = XModelReadStreamFloat(&cursor);

        const float centerSumX = (float)(
            (long double)collision->maxs[0] + collision->mins[0]);
        const float centerSumY = (float)(
            (long double)collision->maxs[1] + collision->mins[1]);
        const float centerSumZ = (float)(
            (long double)collision->maxs[2] + collision->mins[2]);
        collision->center[0] = (float)(
            (long double)centerSumX * (long double)0.5f);
        collision->center[1] = (float)(
            (long double)centerSumY * (long double)0.5f);
        collision->center[2] = (float)(
            (long double)centerSumZ * (long double)0.5f);
        const float deltaX = (float)(
            (long double)collision->maxs[0] - collision->center[0]);
#if defined(WINDOWS_BEHAVIOR)
        /* Windows stores deltaX as binary32 but retains deltaY and deltaZ in
         * the PC=53 x87 stack through the squared-radius expression. */
        const long double deltaY =
            (long double)collision->maxs[1] - collision->center[1];
        const long double deltaZ =
            (long double)collision->maxs[2] - collision->center[2];
        /* CoDUOMP.exe 0x0049df7a..0x0049df95 folds Z, Y, X. */
        collision->radiusSq = (float)(
            (deltaZ * deltaZ + deltaY * deltaY) +
            (long double)deltaX * (long double)deltaX);
#else
        /* Linux stores all three deltas as binary32 before folding X, Y, Z;
         * coduo_lnxded 0x080c320b..0x080c3287. */
        const float deltaY = (float)(
            (long double)collision->maxs[1] - collision->center[1]);
        const float deltaZ = (float)(
            (long double)collision->maxs[2] - collision->center[2]);
        collision->radiusSq = (float)(
            ((long double)deltaX * (long double)deltaX +
             (long double)deltaY * (long double)deltaY) +
            (long double)deltaZ * (long double)deltaZ);
#endif
    }

    memcpy(parts->partStateIndices, cursor,
           (size_t)(uint32_t)(int32_t)totalPartCount);
    FS_FreeFile(fileBuffer);
    entry->data.xmodelParts = parts;
    entry->freeData = XModelPartsFree;
    return entry;
}

/* Source: CoDUOMP.exe 0x0049e000..0x0049e0b6.
 * Name: exact same-module Mac symbol XModelLoadConfigFile. */
const uint8_t *XModelLoadConfigFile(const char *name,
                                    const uint8_t *cursor,
                                    XModelConfig *header)
{
    int16_t version = XModelReadStreamInt16(&cursor);
    if (version != XMODEL_ASSET_VERSION) {
        Com_Error(ERR_DROP,
                  "\x15" "xmodel '%s' out of date (version %d, "
                  "expecting %d)",
                  name, version, XMODEL_ASSET_VERSION);
        return NULL;
    }

    header->mins[0] = XModelReadStreamFloat(&cursor);
    header->mins[1] = XModelReadStreamFloat(&cursor);
    header->mins[2] = XModelReadStreamFloat(&cursor);
    header->maxs[0] = XModelReadStreamFloat(&cursor);
    header->maxs[1] = XModelReadStreamFloat(&cursor);
    header->maxs[2] = XModelReadStreamFloat(&cursor);

    for (int32_t lodIndex = 0; lodIndex < XMODEL_LOD_COUNT; ++lodIndex) {
        header->lods[lodIndex].distance = XModelReadStreamFloat(&cursor);
        /* NOT_FROM_ORIGINAL_SOURCE: the model-file prepass proves a terminator
         * within the fixed LOD-name destination before this copy. */
        strcpy(header->lods[lodIndex].name, (const char *)cursor);
        cursor += strlen((const char *)cursor) + 1U;
    }
    header->modelFileCount = (int32_t)XModelReadStreamUint32(&cursor);
    return cursor;
}

/* Sources: CoDUOMP.exe 0x0049e0c0..0x0049e24d;
 * coduo_lnxded 0x080c34b2..0x080c37f6. The decimal epsilon is the exact float
 * encoded by original .rdata bits 0x3a83126f.
 * Name: exact same-module Mac symbol XModelLoadCollData. */
const uint8_t *XModelLoadCollData(
    const uint8_t *cursor, XModelInfo *collision,
    void *(*alloc)(size_t size))
{
    /* NOT_FROM_ORIGINAL_SOURCE: XModelPrecache validates the complete
     * collision graph and count-derived extents before this pointer-only body. */
    const float boundsEpsilon = 0.0010000000474974513f;

    collision->collisionSurfaceCount =
        (int32_t)XModelReadStreamUint32(&cursor);
    collision->contents = 0;
    if (collision->collisionSurfaceCount == 0) {
        collision->collisionSurfaces = NULL;
        return cursor;
    }

    collision->collisionSurfaces = alloc(
        (size_t)((uint32_t)collision->collisionSurfaceCount *
                 (uint32_t)sizeof(collision->collisionSurfaces[0])));
    for (int32_t surfaceIndex = 0;
         surfaceIndex < collision->collisionSurfaceCount;
         ++surfaceIndex) {
        XModelCollSurf *surface =
            &collision->collisionSurfaces[surfaceIndex];
        surface->numCollTris = (int32_t)XModelReadStreamUint32(&cursor);
        surface->collTris = alloc(
            (size_t)((uint32_t)surface->numCollTris *
                     (uint32_t)sizeof(surface->collTris[0])));

        for (int32_t facetIndex = 0;
             facetIndex < surface->numCollTris;
             ++facetIndex) {
            XModelCollTri *facet =
                &surface->collTris[facetIndex];
            for (int32_t planeIndex = 0;
                 planeIndex < XMODEL_COLLISION_FACET_PLANE_COUNT;
                 ++planeIndex) {
                facet->planes[planeIndex].normal[0] =
                    XModelReadStreamFloat(&cursor);
                facet->planes[planeIndex].normal[1] =
                    XModelReadStreamFloat(&cursor);
                facet->planes[planeIndex].normal[2] =
                    XModelReadStreamFloat(&cursor);
                facet->planes[planeIndex].distance =
                    XModelReadStreamFloat(&cursor);
            }
        }

        surface->expandedMins[0] = (float)(
            (long double)XModelReadStreamFloat(&cursor) - boundsEpsilon);
        surface->expandedMins[1] = (float)(
            (long double)XModelReadStreamFloat(&cursor) - boundsEpsilon);
        surface->expandedMins[2] = (float)(
            (long double)XModelReadStreamFloat(&cursor) - boundsEpsilon);
        surface->expandedMaxs[0] = (float)(
            (long double)XModelReadStreamFloat(&cursor) + boundsEpsilon);
        surface->expandedMaxs[1] = (float)(
            (long double)XModelReadStreamFloat(&cursor) + boundsEpsilon);
        surface->expandedMaxs[2] = (float)(
            (long double)XModelReadStreamFloat(&cursor) + boundsEpsilon);
        surface->basePoseIndex =
            (int32_t)XModelReadStreamUint32(&cursor);
        surface->contents = (int32_t)(
            XModelReadStreamUint32(&cursor) &
            XMODEL_COLLISION_CONTENTS_MASK);
        surface->surfaceFlags =
            (int32_t)XModelReadStreamUint32(&cursor);
        collision->contents |= surface->contents;
    }
    return cursor;
}

/* Source: CoDUOMP.exe 0x0049e250..0x0049e26c. */
qboolean XModelExists(const char *name)
{
    return FS_GetDataForFile("xmodel", name, "") != NULL
               ? qtrue
               : qfalse;
}

/* Source: CoDUOMP.exe 0x0049e270..0x0049e614. */
XModel *XModelPrecache(const char *name, xmodel_load_mode_t loadMode,
                      void *(*alloc)(size_t size),
                      void *(*optionalAlloc)(size_t size))
{
    XModel *entry = (XModel *)FS_GetDataForFile("xmodel", name, "");
    if (entry == NULL) {
        if (xmodel_enforceExist != qfalse) {
            /* 0x0049e2b1 push 0x5a1c90: the full .rdata string is
             * "\x15Cannot precache 'xmodel/%s'.\nyou may need to get latest and run
             * converter to fix" -- a prior pass truncated it after 'xmodel/%s' (the
             * sibling xmodelsurfs/xmodelparts errors keep the suffix). */
            Com_Error(ERR_DROP,
                      "\x15" "Cannot precache 'xmodel/%s'.\n"
                      "you may need to get latest and run converter to fix",
                      name);
            return NULL;
        }
        Com_Printf("ERROR: Cannot precache 'xmodel/%s'", name);
        return XModelCreateDefault_();
    }

    XModelInfo *collision = entry->info;
    if (collision == NULL) {
        char path[XMODEL_PATH_BUFFER_SIZE];
        void *fileBuffer;
        XModelConfig header;

        /* NOT_FROM_ORIGINAL_SOURCE: require the complete model path and NUL to
         * fit; never substitute a truncated registry name. */
        if (strlen(name) > sizeof(path) - sizeof("xmodel/")) {
            Com_Error(ERR_DROP, "\x15" "XModel path is too long");
            return NULL;
        }
        Com_sprintf(path, sizeof(path), "xmodel/%s", name);
        const int32_t fileLength = FS_ReadFile(path, &fileBuffer);
        if (fileLength < 0) {
            Com_Error(ERR_DROP,
                      "\x15" "Cannot find 'xmodel/%s'.", name);
            return NULL;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: prove every serialized model record,
         * string, and count before calling retained pointer-only loaders. */
        if (coduo_xmodel_validate_model_file(
                fileBuffer, (size_t)fileLength) == qfalse) {
            FS_FreeFile(fileBuffer);
            Com_Error(ERR_DROP, "\x15" "Malformed xmodel asset '%s'", name);
            return NULL;
        }

        const uint8_t *cursor =
            XModelLoadConfigFile(name, fileBuffer, &header);
        if (cursor == NULL) {
            FS_FreeFile(fileBuffer);
            return NULL;
        }
        const uint8_t *fileEnd =
            (const uint8_t *)fileBuffer + (size_t)fileLength;

        uint32_t lodNameBytes = 0;
        for (int32_t lodIndex = 0;
             lodIndex < XMODEL_LOD_COUNT;
             ++lodIndex) {
            lodNameBytes +=
                (uint32_t)strlen(header.lods[lodIndex].name) + 1u;
        }

        uint32_t modelInfoBytes =
            (uint32_t)sizeof(*collision) + lodNameBytes;
        collision = alloc((size_t)modelInfoBytes);
        cursor = XModelLoadCollData(cursor, collision, alloc);
        char *lodNameStorage = (char *)(collision + 1);
        collision->lodCount = 0;

        for (int32_t lodIndex = 0;
             lodIndex < XMODEL_LOD_COUNT;
             ++lodIndex) {
            XModelLodInfo *lod = &collision->lodRecords[lodIndex];
            strcpy(lodNameStorage, header.lods[lodIndex].name);
            lod->name = lodNameStorage;
            lodNameStorage += strlen(lodNameStorage) + 1U;
            /* The construction loop stores the name, optional surface count
             * and name table, then distance. The three surfs slots are cleared
             * together at 0x0049e54f..0x0049e557 after the entry is committed
             * below. */

            if (lod->name[0] == '\0') {
                lod->surfaceNameTable = NULL;
                lod->distance = header.lods[lodIndex].distance;
                continue;
            }

            ++collision->lodCount;
            lod->surfaceCount = XModelReadStreamInt16(&cursor);
            lod->surfaceNameTable = alloc(
                (size_t)((uint32_t)(int32_t)lod->surfaceCount *
                         (uint32_t)sizeof(lod->surfaceNameTable[0])));
            for (int32_t surfaceIndex = 0;
                 surfaceIndex < lod->surfaceCount;
                 ++surfaceIndex) {
                /* CoDUOMP.exe 0x0049e460..0x0049e4af reads every declared
                 * name without retaining the file extent. For an omitted
                 * trailing slot, reproduce the empty string retail obtains
                 * from read-buffer padding while keeping the cursor at EOF. */
                const char *surfaceName;
                if (cursor == fileEnd) {
                    surfaceName = xmodel_emptyName;
                } else {
                    surfaceName = (const char *)cursor;
                    cursor += strlen(surfaceName) + 1U;
                }
                lod->surfaceNameTable[surfaceIndex] =
                    SL_GetString_(
                        surfaceName, 0, XMODEL_SURFACE_NAME_STRING_TYPE);
            }
            lod->distance = header.lods[lodIndex].distance;
        }

        FS_FreeFile(fileBuffer);
        memcpy(collision->mins, header.mins, sizeof(collision->mins));
        memcpy(collision->maxs, header.maxs, sizeof(collision->maxs));
        collision->modelFileCount = (int16_t)header.modelFileCount;
        entry->info = collision;
        entry->freeData = XModelFree;
        collision->lodRecords[0].surfs = NULL;
        collision->lodRecords[1].surfs = NULL;
        collision->lodRecords[2].surfs = NULL;
    } else if (collision == &xmodel_defaultCollision) {
        return entry;
    }

    collision->parts =
        XModelPartsPrecache(collision->lodRecords[0].name, alloc);
    if (collision->parts == NULL) {
        XModelFree((fileData_t *)(void *)entry);
        XModelCreateDefault(entry);
        return entry;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: the model and parts files are separate assets;
     * collision-pose indexes must fit the loaded parts record. */
    if (coduo_xmodel_loaded_collision_fits(collision) == qfalse) {
        Com_Error(ERR_DROP,
                  "\x15" "XModel '%s' has an invalid collision bone", name);
        return NULL;
    }

    if (loadMode == XMODEL_LOAD_PARTS_ONLY) {
        return entry;
    }

    for (int32_t lodIndex = 0;
         lodIndex < XMODEL_LOD_COUNT &&
             collision->lodRecords[lodIndex].name[0] != '\0';
         ++lodIndex) {
        XModelLodInfo *lod = &collision->lodRecords[lodIndex];
        lod->surfs = XModelSurfsPrecache(lod->name, alloc);
        if (lod->surfs == NULL) {
            XModelFree((fileData_t *)(void *)entry);
            XModelCreateDefault(entry);
            return entry;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: require enough names for loaded geometry
         * and every serialized bone reference to fit the independent parts
         * table; unused trailing names remain accepted. */
        if (coduo_xmodel_loaded_references_fit(
                collision, lod, lod->surfs->surfs) == qfalse) {
            Com_Error(ERR_DROP,
                      "\x15" "XModel '%s' has invalid surface references",
                      name);
            return NULL;
        }

        if (xmodel_optimizeEnabled != qfalse &&
            loadMode == XMODEL_LOAD_SURFACES_PREPROCESSED) {
            xmodel_compat_optimize_loaded_surfs(
                lod->surfs->surfs, optionalAlloc);
        }
    }
    return entry;
}

/* Source: CoDUOMP.exe 0x0049e620..0x0049e68a.
 * Name: exact same-module Mac symbol XModelClearData. Allocator-range pointer
 * comparisons are intentionally expressed as integer addresses. */
void XModelClearData(void *rangeStart, void *rangeEnd)
{
    uintptr_t first = (uintptr_t)rangeStart;
    uintptr_t limit = (uintptr_t)rangeEnd;
    XModelSurfsData **link = &xmodel_surfsCloneHead;

    while (*link != NULL) {
        XModelSurfsData *surfs = *link;
        uintptr_t surfsAddress = (uintptr_t)surfs;
        if (surfsAddress >= first && surfsAddress < limit) {
            *link = surfs->next;
            continue;
        }

        for (int32_t surfaceIndex = 0;
             surfaceIndex < surfs->surfaceCount;
             ++surfaceIndex) {
            XSurface *surface = surfs->surfaces[surfaceIndex];
            if (surface == NULL) {
                continue;
            }

            uintptr_t nvAddress = (uintptr_t)surface->optimizedDataNV;
            if (nvAddress >= first && nvAddress < limit) {
                surface->optimizedDataNV = NULL;
            }
            uintptr_t atiAddress = (uintptr_t)surface->optimizedDataATI;
            if (atiAddress >= first && atiAddress < limit) {
                surface->optimizedDataATI = NULL;
            }
        }
        link = &surfs->next;
    }
}
