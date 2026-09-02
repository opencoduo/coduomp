#include "xmodel.h"

#include "animation_private.h"

#include <string.h>

enum {
    XMODEL_BONE_INDEX_NOT_FOUND = -1,
    XMODEL_LOD_NOT_FOUND = -1,
    XMODEL_TRIANGLE_INDEX_COUNT = XSURFACE_TRIANGLE_INDEX_COUNT
};

const char xmodel_defaultName[] = "DEFAULT";

/* This state is owned exclusively by the three public LOD-test accessors.
 * CoDUOMP.exe stores it at 0x008b1f60..0x008b1f9f; coduo_lnxded stores the
 * corresponding state at 0x08456980..0x084569c3. */
static qboolean xmodel_testLodsEnabled;
static float xmodel_testLodDistanceOverride;
static XModelLodInfo xmodel_testLodRecords[XMODEL_LOD_COUNT];

/*
 * Complete common XModel/XSurface accessor cluster.
 *
 * Windows authority: CoDUOMP.exe 0x0049e690..0x0049ee01, with
 * XSurfaceGetTris at 0x0049e7e0 and XModelGetName at 0x0049e8c0.
 * Linux authority: coduo_lnxded 0x080c3f5e..0x080c4657.
 * The different optimizer output retains the same field walks, signed count
 * domains, comparisons, and returned values.
 */

const char *XModelGetName(const XModel *model)
{
    return model->name;
}

int32_t XModelNumBones(const XModel *model)
{
    return model->info->parts->data.xmodelParts->partNameTableSlot->partNameTable->count;
}

const uint16_t *XModelBoneNames(const XModel *model)
{
    return model->info->parts->data.xmodelParts->partNameTableSlot->partNameTable->handles;
}

int32_t XModelGetBoneIndex(const XModel *model, uint16_t partName)
{
    const XModelPartNameTable *table = model->info->parts->data.xmodelParts->partNameTableSlot->partNameTable;

    for (int32_t index = table->count - 1; index >= 0; --index) {
        if (table->handles[index] == partName) {
            return index;
        }
    }
    return XMODEL_BONE_INDEX_NOT_FOUND;
}

void XModelGetBounds(const XModel *model, vec3_t mins, vec3_t maxs)
{
    mins[0] = model->info->mins[0];
    mins[1] = model->info->mins[1];
    mins[2] = model->info->mins[2];
    maxs[0] = model->info->maxs[0];
    maxs[1] = model->info->maxs[1];
    maxs[2] = model->info->maxs[2];
}

int32_t XModelGetSurfaces(const XModel *model, XSurface ***surfacesOut, int32_t lodIndex)
{
    const XModelSurfsData *surfs = model->info->lodRecords[lodIndex].surfs->surfs;

    *surfacesOut = surfs->surfaces;
    return surfs->surfaceCount;
}

const char *XModelGetSurfaceName(const XModel *model, int32_t surfaceIndex, int32_t lodIndex)
{
    const uint16_t name = model->info->lodRecords[lodIndex].surfaceNameTable[surfaceIndex];

    return name != 0 ? SL_ConvertToString(name) : xmodel_defaultName;
}

XSurface *XSurfaceCloneSurface(const XSurface *surface, xmodel_asset_alloc_fn alloc)
{
    XSurface *clone = alloc(sizeof(*clone));

    *clone = *surface;
    clone->optimizedDataATI = NULL;
    clone->optimizedDataNV = NULL;
    clone->texCoords = alloc((size_t)((uint32_t)(int32_t)surface->vertexCount * (uint32_t)sizeof(surface->texCoords[0])));
    memcpy(clone->texCoords, surface->texCoords,
           (size_t)((uint32_t)(int32_t)surface->vertexCount * (uint32_t)sizeof(surface->texCoords[0])));
    return clone;
}

int32_t XSurfaceGetNumVerts(const XSurface *surface)
{
    return surface->vertexCount;
}

int32_t XSurfaceGetNumTris(const XSurface *surface)
{
    return surface->triangleCount;
}

int32_t XSurfaceTileMode(const XSurface *surface)
{
    return surface->tileMode;
}

void XSurfaceGetTris(const XSurface *surface, XSurfaceTriangle *triangles, int16_t baseIndex)
{
    const int32_t triangleCount = surface->triangleCount;

    /* NOT_FROM_ORIGINAL_SOURCE: zero triangles are an empty copy; the packed
     * pair path below handles only complete pairs and a separate odd tail. */
    if (triangleCount <= 0) {
        return;
    }

    if (baseIndex == 0) {
        const uint32_t triangleBytes = (uint32_t)triangleCount * (uint32_t)sizeof(surface->triangles[0]);
        memcpy(triangles, surface->triangles, (size_t)triangleBytes);
        return;
    }

    const uint16_t truncatedBaseIndex = (uint16_t)baseIndex;
    const uint32_t packedBaseIndex = (uint32_t)truncatedBaseIndex | ((uint32_t)truncatedBaseIndex << 16);
    const uint8_t *sourceBytes = (const uint8_t *)surface->triangles;
    uint8_t *destinationBytes = (uint8_t *)triangles;
    uint32_t trianglePairCount = (uint32_t)triangleCount >> 1;

    while (trianglePairCount != 0) {
        for (int32_t packedLane = 0; packedLane < XMODEL_TRIANGLE_INDEX_COUNT; ++packedLane) {
            uint32_t packedIndices;

            memcpy(&packedIndices, sourceBytes, sizeof(packedIndices));
            packedIndices += packedBaseIndex;
            memcpy(destinationBytes, &packedIndices, sizeof(packedIndices));
            sourceBytes += sizeof(packedIndices);
            destinationBytes += sizeof(packedIndices);
        }
        --trianglePairCount;
    }

    if ((triangleCount & 1) != 0) {
        for (int32_t index = 0; index < XMODEL_TRIANGLE_INDEX_COUNT; ++index) {
            uint16_t vertexIndex;

            memcpy(&vertexIndex, sourceBytes, sizeof(vertexIndex));
            vertexIndex = (uint16_t)((uint32_t)vertexIndex + (uint32_t)truncatedBaseIndex);
            memcpy(destinationBytes, &vertexIndex, sizeof(vertexIndex));
            sourceBytes += sizeof(vertexIndex);
            destinationBytes += sizeof(vertexIndex);
        }
    }
}

uint8_t *XSurfaceGetBlendInfoArray(const XSurface *surface)
{
    /* Windows returns this pointer in EAX.  Linux returns a one-pointer
     * aggregate through a hidden result slot (0x080c426a); the observable
     * payload is the same field and the retained Linux engine has no caller
     * that depends on the compiler-specific aggregate ABI. */
    return surface->vertexData.blendPrimaryStream;
}

vec2_t *XSurfaceGetTexCoordArray(const XSurface *surface)
{
    return surface->texCoords;
}

XSurfaceWeightedPoint *XSurfaceGetVertexInfoArray(const XSurface *surface)
{
    return surface->weightedPoints;
}

int32_t XSurfaceGetBoneIndex(const XSurface *surface)
{
    return surface->boneIndex;
}

void XSurfaceRemapTextureCoordinates(XSurface *surface, const vec2_t scale, const vec2_t offset, int32_t sourceUIndex, int32_t sourceVIndex)
{
    int32_t remainingVertices = surface->vertexCount;
    vec2_t *texCoord = surface->texCoords;

    /* NOT_FROM_ORIGINAL_SOURCE: process only the positive in-memory vertex
     * domain established by model loading. */
    while (remainingVertices > 0) {
        const float sourceV = (*texCoord)[sourceVIndex];
        const float sourceU = (*texCoord)[sourceUIndex];

        (*texCoord)[0] = offset[0] + scale[0] * sourceU;
        (*texCoord)[1] = offset[1] + scale[1] * sourceV;
        ++texCoord;
        --remainingVertices;
    }
}

int32_t XModelGetContents(const XModel *model)
{
    return model->info->contents;
}

int32_t XModelGetNumLods(const XModel *model)
{
    return model->info->lodCount;
}

int16_t XModelGetModelFileCount(const XModel *model)
{
    return model->info->modelFileCount;
}

void XModelSetTestLods(int32_t lodIndex, float distance)
{
    if (lodIndex == 0) {
        xmodel_testLodsEnabled = distance >= 0.0f ? qtrue : qfalse;
    }
    if (distance < 0.0f) {
        distance = 0.0f;
    }
    xmodel_testLodRecords[lodIndex].distance = distance;
}

void XModelSetTestLodDist(float distance)
{
    xmodel_testLodDistanceOverride = distance > 0.0f ? distance : 0.0f;
}

int32_t XModelGetLodForDist(const XModel *model, float distance)
{
    const XModelInfo *info = model->info;
    const XModelLodInfo *lodRecords = xmodel_testLodsEnabled ? xmodel_testLodRecords : info->lodRecords;

    if (xmodel_testLodDistanceOverride != 0.0f) {
        distance = xmodel_testLodDistanceOverride;
    }

    for (int32_t lodIndex = 0; lodIndex < info->lodCount; ++lodIndex) {
        if (lodRecords[lodIndex].distance == 0.0f || distance < lodRecords[lodIndex].distance) {
            return lodIndex;
        }
    }
    return XMODEL_LOD_NOT_FOUND;
}
