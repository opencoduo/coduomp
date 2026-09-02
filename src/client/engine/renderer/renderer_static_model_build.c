#include "backend.h"

#include "../animation/dobj.h"

#include <string.h>

enum {
    R_STATIC_MODEL_MAX_SOURCE_SURFACES = 1024,
    R_STATIC_MODEL_SHADER_USAGE = 7,
    R_STATIC_MODEL_LIGHTMAP_MODE = -1
};

/* Source: CoDUOMP.exe 0x00518c20..0x00518cc4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00518c20_00518cc5.mcode and direct
 * objdump of the original executable. Exact same-module Mac symbol
 * R_SetupDObjToStaticModel. The Windows body proves both optimization gates,
 * the first DObj model as owner, and the single allocation containing the
 * variable LOD-pointer array followed by the copied registration name. */
renderer_registered_static_model_t *R_SetupDObjToStaticModel(const char *name, const DObj *obj)
{
    XModel *model = NULL;
    int32_t lodCount = 0;

    if (r_optimizeBackend->integer != 0 && r_optimizeXModels->integer != 0) {
        model = obj->models[0];
        lodCount = model->info->lodCount;
    }

    const size_t nameBytes = strlen(name) + 1U;
    renderer_registered_static_model_t *staticModel =
        ri.Hunk_Alloc(sizeof(*staticModel) + (size_t)lodCount * sizeof(staticModel->lods[0]) + nameBytes);
    char *nameStorage = (char *)&staticModel->lods[lodCount];

    staticModel->name = nameStorage;
    memcpy(nameStorage, name, nameBytes);
    staticModel->lodCount = lodCount;
    staticModel->model = model;
    return staticModel;
}

/* Source: CoDUOMP.exe 0x00518cd0..0x0051920f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00518cd0_00519210.mcode and direct
 * objdump of the original executable. Exact same-module Mac symbol
 * R_FinishDObjToStaticModel. Windows proves surface discovery, shader-based
 * grouping, packed geometry allocation, DObj base-pose selection, index
 * rebasing, texture-sheet remapping, and backend optimizer priority. */
void R_FinishDObjToStaticModel(renderer_registered_static_model_t *staticModel, const DObj *obj)
{
    int32_t lodIndices[DOBJ_MAX_MODELS] = {0};

    for (int32_t lodIndex = 0; lodIndex < staticModel->model->info->lodCount; ++lodIndex) {
        dobj_surface_ref_t surfaceRefs[R_STATIC_MODEL_MAX_SOURCE_SURFACES];
        uint32_t partBits[DOBJ_PART_BITSET_WORD_COUNT];
        shader_t *sourceShaders[R_STATIC_MODEL_MAX_SOURCE_SURFACES];
        shader_t *effectiveShaders[R_STATIC_MODEL_MAX_SOURCE_SURFACES];
        int32_t surfaceGroups[R_STATIC_MODEL_MAX_SOURCE_SURFACES];
        int32_t indexCounts[R_STATIC_MODEL_MAX_SOURCE_SURFACES];
        int32_t vertexCounts[R_STATIC_MODEL_MAX_SOURCE_SURFACES];
        int32_t groupCount = 0;

        lodIndices[0] = lodIndex;
        const int32_t sourceSurfaceCount = DObjGetNumSurfaces(obj, lodIndices);
        if (sourceSurfaceCount > R_STATIC_MODEL_MAX_SOURCE_SURFACES) {
            ri.Error(ERR_DROP, "\x15model '%s' has %i surfaces, which is more than %i\n", staticModel->name, sourceSurfaceCount,
                     R_STATIC_MODEL_MAX_SOURCE_SURFACES);
        }

        DObjGetSurfaces(obj, surfaceRefs, partBits, lodIndices);

        for (int32_t refIndex = 0; refIndex < sourceSurfaceCount; ++refIndex) {
            const dobj_surface_ref_t *ref = &surfaceRefs[refIndex];
            XSurface *sourceSurface = DObjGetSurface(obj, ref->modelIndex, ref->surfaceIndex, lodIndices);
            const char *surfaceName = DObjGetSurfaceName(obj, (uint8_t)ref->modelIndex, ref->surfaceIndex, lodIndices);
            shader_t *sourceShader =
                R_FindShader(va("skins/%s", surfaceName), R_STATIC_MODEL_LIGHTMAP_MODE, qtrue, R_STATIC_MODEL_SHADER_USAGE);
            shader_t *effectiveShader = sourceShader;

            sourceShaders[refIndex] = sourceShader;
            if ((sourceShader->flags & SHADER_FLAG_REMAPPED) != 0)
                effectiveShader = sourceShader->remappedShader;
            /*
             * 0x00518e41 loads the compact group-array byte offset, not the
             * source-surface index. The candidate occupies groupCount while
             * the search compares the established [0, groupCount) prefix.
             * A duplicate candidate is overwritten by the next source
             * surface; a new candidate becomes the next retained group.
             */
            effectiveShaders[groupCount] = effectiveShader;

            const int32_t sourceIndexCount = sourceSurface->triangleCount * 3;
            const int32_t sourceVertexCount = sourceSurface->vertexCount;
            int32_t groupIndex;

            for (groupIndex = 0; groupIndex < groupCount; ++groupIndex) {
                if (effectiveShaders[groupIndex] == effectiveShader)
                    break;
            }

            surfaceGroups[refIndex] = groupIndex;
            if (groupIndex == groupCount) {
                /*
                 * The same compact offset is used by the stores at
                 * 0x00518ea0/0x00518ea7 and increments only when this is a
                 * new group (0x00518eae). Duplicate surfaces branch through
                 * 0x00519002 and leave it unchanged.
                 */
                indexCounts[groupIndex] = sourceIndexCount;
                vertexCounts[groupIndex] = sourceVertexCount;
                ++groupCount;
            } else {
                indexCounts[groupIndex] += sourceIndexCount;
                vertexCounts[groupIndex] += sourceVertexCount;
            }
        }

        renderer_static_model_lod_t *lod = ri.Hunk_Alloc(sizeof(*lod) + (size_t)groupCount * sizeof(lod->surfaces[0]));
        lod->surfaceCount = groupCount;

        for (int32_t groupIndex = 0; groupIndex < groupCount; ++groupIndex) {
            renderer_static_model_surface_t *surface = &lod->surfaces[groupIndex];
            const int32_t indexCount = indexCounts[groupIndex];
            const int32_t vertexCount = vertexCounts[groupIndex];

            surface->surfaceType = R_SURFACE_STATIC_MODEL;
            surface->storageSource = tr.defaultStorageMode;
            surface->shader = effectiveShaders[groupIndex];
            surface->cachedShader = R_CacheableStaticModelShader(surface->shader);
            surface->indexCount = (uint16_t)indexCount;
            surface->vertexCount = (uint16_t)vertexCount;

            if ((int32_t)surface->indexCount != indexCount || (int32_t)surface->vertexCount != vertexCount) {
                ri.Error(ERR_DROP,
                         "\x15model %s surface %s has more than 65,535 vertices "
                         "or more than 21,845 triangles",
                         staticModel->name, sourceShaders[groupIndex]->name);
            }

            const size_t texCoordBytes = (size_t)surface->vertexCount * sizeof(surface->texCoords[0]);
            const size_t normalBytes = (size_t)surface->vertexCount * sizeof(surface->normals[0]);
            const size_t vertexBytes = (size_t)surface->vertexCount * sizeof(surface->vertices[0]);
            const size_t indexBytes = (size_t)surface->indexCount * sizeof(surface->indices[0]);
            uint8_t *geometry = ri.Hunk_Alloc(texCoordBytes + normalBytes + vertexBytes + indexBytes);

            surface->texCoords = (vec2_t *)geometry;
            geometry += texCoordBytes;
            surface->normals = (vec3_t *)geometry;
            geometry += normalBytes;
            surface->vertices = (vec3_t *)geometry;
            geometry += vertexBytes;
            surface->indices = (uint16_t *)geometry;

            indexCounts[groupIndex] = 0;
            vertexCounts[groupIndex] = 0;
        }

        for (int32_t refIndex = 0; refIndex < sourceSurfaceCount; ++refIndex) {
            const dobj_surface_ref_t *ref = &surfaceRefs[refIndex];
            renderer_static_model_surface_t *surface = &lod->surfaces[surfaceGroups[refIndex]];
            XSurface *sourceSurface = DObjGetSurface(obj, ref->modelIndex, ref->surfaceIndex, lodIndices);
            const int32_t sourceIndexCount = sourceSurface->triangleCount * 3;
            const int32_t sourceVertexCount = sourceSurface->vertexCount;
            const int32_t baseIndex = indexCounts[surfaceGroups[refIndex]];
            const int32_t baseVertex = vertexCounts[surfaceGroups[refIndex]];
            uint16_t *destinationIndices = surface->indices + baseIndex;
            vec2_t *destinationTexCoords = surface->texCoords + baseVertex;
            vec3_t *destinationNormals = surface->normals + baseVertex;
            vec3_t *destinationVertices = surface->vertices + baseVertex;
            const DObjSkelMat *basePose = &obj->evaluationStorage->partSpans[obj->modelPartBaseIndices[ref->modelIndex]].basePose;

            memcpy(destinationIndices, sourceSurface->triangles, (size_t)sourceIndexCount * sizeof(destinationIndices[0]));
            XSurfaceGetVerts(sourceSurface, basePose, destinationVertices, destinationTexCoords, destinationNormals);

            if (baseVertex != 0) {
                for (int32_t sourceIndex = 0; sourceIndex < sourceIndexCount; ++sourceIndex) {
                    destinationIndices[sourceIndex] = (uint16_t)(destinationIndices[sourceIndex] + baseVertex);
                }
            }

            if ((sourceShaders[refIndex]->flags & SHADER_FLAG_REMAPPED) != 0) {
                R_RemapTextureCoordinatesForSheet(sourceShaders[refIndex], sourceVertexCount, destinationTexCoords);
            }

            indexCounts[surfaceGroups[refIndex]] += sourceIndexCount;
            vertexCounts[surfaceGroups[refIndex]] += sourceVertexCount;
        }

        for (int32_t groupIndex = 0; groupIndex < groupCount; ++groupIndex) {
            renderer_static_model_surface_t *surface = &lod->surfaces[groupIndex];

            if (glConfig.vertexBufferObjectAvailable) {
                if (!R_OptimizeSModelSurfARB(surface))
                    R_OptimizeSModelSurfGeneric(surface);
            } else if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE) {
                R_OptimizeSModelSurfNV(surface);
            } else if (glConfig.vertexArrayObjectATIAvailable) {
                if (!R_OptimizeSModelSurfATI(surface))
                    R_OptimizeSModelSurfGeneric(surface);
            } else {
                R_OptimizeSModelSurfGeneric(surface);
            }
        }

        staticModel->lods[lodIndex] = lod;
    }
}
