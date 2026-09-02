#include "backend.h"

#include "../animation/dobj.h"
#include "../math/vector_math.h"
#include "../platform/crt_boundary.h"
#include "gl_api.h"

#include <math.h>
#include <string.h>

enum {
    R_MAX_REGISTERED_STATIC_MODELS = 2048,
    R_STATIC_MODEL_SHADER_USAGE = 7,
    R_STATIC_MODEL_LIGHTMAP_MODE = -1
};

#define R_AUTOSPRITE_BOUNDS_SCALE 0.550000011920929f /* 0x3f0ccccd */
#define R_AUTOSPRITE_BOUNDS_PAD 2.0f              /* 0x40000000 */
#define R_STATIC_MODEL_CENTER_SCALE 0.5f             /* 0x3f000000 */
#define R_STATIC_MODEL_COLOR_SCALE 255.0f           /* 0x437f0000 */

static renderer_static_model_instance_t *rendererStaticModelInstances; /* 0x0389fec4 */

enum {
    R_STATIC_MODEL_REFRESH_VERTEX_CAPACITY = UINT16_MAX + 1U
};

/* NOT_FROM_ORIGINAL_SOURCE: shared scratch carrier introduced by the
 * factored refresh helper for its mutually exclusive interleaved formats. */
typedef union renderer_static_model_refresh_staging_u {
    renderer_static_model_t2v3_vertex_t t2v3[R_STATIC_MODEL_REFRESH_VERTEX_CAPACITY];
    renderer_static_model_t2n3v3_vertex_t t2n3v3[R_STATIC_MODEL_REFRESH_VERTEX_CAPACITY];
} renderer_static_model_refresh_staging_t;

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the identical ARB
 * buffer refresh bodies in R_RefreshStaticModels_ARB and
 * R_IncrementalRefreshStaticModels_ARB. The public functions retain their
 * original traversal, eligibility, and buffer-orphaning behavior. */
static void R_RefreshStaticModelSurface_ARB(renderer_static_model_surface_t *surface, renderer_vbo_refresh_components_t refreshComponents,
                                            qboolean orphanBuffers)
{
    if ((refreshComponents & R_VBO_REFRESH_INDEXES) != 0) {
        const size_t indexBytes = (size_t)surface->indexCount * sizeof(surface->indices[0]);

        qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, surface->backend.arb.indexBuffer);
        if (orphanBuffers) {
            qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, indexBytes, NULL, GL_STATIC_DRAW_ARB);
        }
        qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, indexBytes, surface->indices, GL_STATIC_DRAW_ARB);
        qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
    }

    if ((refreshComponents & R_VBO_REFRESH_VERTICES) != 0) {
        renderer_static_model_refresh_staging_t staging;
        size_t vertexBytes;

        if (surface->surfaceType == R_SURFACE_STATIC_MODEL_T2V3_ARB) {
            for (uint16_t vertexIndex = 0; vertexIndex < surface->vertexCount; ++vertexIndex) {
                memcpy(staging.t2v3[vertexIndex].texCoord, surface->texCoords[vertexIndex], sizeof(staging.t2v3[vertexIndex].texCoord));
                memcpy(staging.t2v3[vertexIndex].position, surface->vertices[vertexIndex], sizeof(staging.t2v3[vertexIndex].position));
            }
            vertexBytes = (size_t)surface->vertexCount * sizeof(staging.t2v3[0]);
        } else {
            for (uint16_t vertexIndex = 0; vertexIndex < surface->vertexCount; ++vertexIndex) {
                memcpy(staging.t2n3v3[vertexIndex].texCoord, surface->texCoords[vertexIndex], sizeof(staging.t2n3v3[vertexIndex].texCoord));
                memcpy(staging.t2n3v3[vertexIndex].normal, surface->normals[vertexIndex], sizeof(staging.t2n3v3[vertexIndex].normal));
                memcpy(staging.t2n3v3[vertexIndex].position, surface->vertices[vertexIndex], sizeof(staging.t2n3v3[vertexIndex].position));
            }
            vertexBytes = (size_t)surface->vertexCount * sizeof(staging.t2n3v3[0]);
        }

        qglBindBufferARB(GL_ARRAY_BUFFER_ARB, surface->optimized.vertexBuffer);
        if (orphanBuffers) {
            qglBufferDataARB(GL_ARRAY_BUFFER_ARB, vertexBytes, NULL, GL_STATIC_DRAW_ARB);
        }
        qglBufferDataARB(GL_ARRAY_BUFFER_ARB, vertexBytes, &staging, GL_STATIC_DRAW_ARB);
        qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    }
}

/* Source: CoDUOMP.exe 0x00519210..0x00519338.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00519210_00519339.mcode and direct
 * objdump of the original executable. Exact same-module Mac symbol
 * R_NeedsBoundsAdjustment. The Windows body proves both sources of shader
 * state: optimized static-model LOD zero when a registration exists, or the
 * entity DObj's LOD-zero surface names before registration has completed. */
qboolean R_NeedsBoundsAdjustment(const refEntity_t *entity)
{
    const renderer_registered_static_model_t *registration = entity->staticModelRegistration;

    if (registration != NULL) {
        const renderer_static_model_lod_t *lod = registration->lods[0];

        for (int32_t surfaceIndex = 0; surfaceIndex < lod->surfaceCount; ++surfaceIndex) {
            if ((lod->surfaces[surfaceIndex].shader->surfaceFlags & SHADER_SURFACE_DEFORMED_POSITIONS) != 0) {
                return qtrue;
            }
        }
        return qfalse;
    }

    const DObj *obj = entity->dobj;
    if (obj == NULL)
        return qfalse;

    int32_t lodIndex = 0;
    uint32_t partBits[DOBJ_PART_BITSET_WORD_COUNT];
    const int32_t surfaceCount = DObjGetNumSurfaces(obj, &lodIndex);
    dobj_surface_ref_t *surfaceRefs = CODUOMP_ALLOCA((size_t)surfaceCount * sizeof(surfaceRefs[0]));

    DObjGetSurfaces(obj, surfaceRefs, partBits, &lodIndex);

    for (int32_t surfaceIndex = 0; surfaceIndex < surfaceCount; ++surfaceIndex) {
        const dobj_surface_ref_t *surfaceRef = &surfaceRefs[surfaceIndex];
        const char *surfaceName = DObjGetSurfaceName(obj, (uint8_t)surfaceRef->modelIndex, surfaceRef->surfaceIndex, &lodIndex);
        shader_t *shader = R_FindShader(va("skins/%s", surfaceName), R_STATIC_MODEL_LIGHTMAP_MODE, qtrue, R_STATIC_MODEL_SHADER_USAGE);

        if ((shader->surfaceFlags & SHADER_SURFACE_DEFORMED_POSITIONS) != 0)
            return qtrue;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x00519340..0x005193e6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00519340_005193e7.mcode and direct
 * objdump of the original executable. Exact same-module Mac symbol
 * R_AdjustBoundsForAutosprite. The constants retain the original float bit
 * patterns; the radius uses the full opposite-corner distance. */
void R_AdjustBoundsForAutosprite(const refEntity_t *entity, vec3_t mins, vec3_t maxs)
{
    if (R_NeedsBoundsAdjustment(entity) == qfalse)
        return;

    const float centerX = (float)(((long double)mins[0] + maxs[0]) * 0.5L);
    const long double centerY = ((long double)mins[1] + maxs[1]) * 0.5L;
    const float centerZSum = (float)((long double)mins[2] + maxs[2]);
    const long double centerZ = (long double)centerZSum * 0.5L;
    const long double diagonalX = (long double)maxs[0] - mins[0];
    const long double diagonalY = (long double)maxs[1] - mins[1];
    const long double diagonalZ = (long double)maxs[2] - mins[2];
    const long double diagonalLength = sqrtl((diagonalZ * diagonalZ + diagonalY * diagonalY) + diagonalX * diagonalX);
    const long double radius = diagonalLength * (long double)R_AUTOSPRITE_BOUNDS_SCALE + (long double)R_AUTOSPRITE_BOUNDS_PAD;

    mins[0] = (float)((long double)centerX - radius);
    mins[1] = (float)(centerY - radius);
    mins[2] = (float)(centerZ - radius);
    maxs[0] = (float)((long double)centerX + radius);
    maxs[1] = (float)(centerY + radius);
    maxs[2] = (float)(centerZ + radius);
}

/* Source: CoDUOMP.exe 0x005193f0..0x005194d6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005193f0_005194d7.mcode and direct
 * objdump of the original executable. Exact same-module Mac symbol
 * R_RegisterStaticModel. Windows proves the case-insensitive cache lookup,
 * hard 2048-entry limit, delayed-image group lifetime, and the rule that an
 * unoptimized registration is cached but returned to callers as NULL. */
renderer_registered_static_model_t *R_RegisterStaticModel(const char *name, const DObj *obj)
{
    for (int32_t modelIndex = 0; modelIndex < tr.registeredStaticModelCount; ++modelIndex) {
        renderer_registered_static_model_t *registration = tr.registeredStaticModels[modelIndex];

        if (registration->name != NULL && name != NULL && Q_stricmp(registration->name, name) == 0) {
            return registration->model != NULL ? registration : NULL;
        }
    }

    if (tr.registeredStaticModelCount == R_MAX_REGISTERED_STATIC_MODELS) {
        ri.Printf(R_PRINT_WARNING,
                  "R_RegisterStaticModel failed for '%s' -- more than %i unique "
                  "static models\n",
                  name, tr.registeredStaticModelCount);
        return NULL;
    }

    R_BeginDelayedImageGroup(name);
    renderer_registered_static_model_t *registration = R_SetupDObjToStaticModel(name, obj);
    R_EndDelayedImageGroup();

    tr.registeredStaticModels[tr.registeredStaticModelCount++] = registration;
    return registration->model != NULL ? registration : NULL;
}

/* Source: CoDUOMP.exe 0x005194e0..0x00519788.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005194e0_00519789.mcode and direct
 * objdump of the original executable. Exact same-module Mac symbol
 * R_CreateStaticModel. Windows proves the five source arguments, stripped
 * xmodel/ name storage, temporary one-model DObj evaluation, scaled axis and
 * translated bounds, conditional optimized registration, and list prepend. */
void R_CreateStaticModel(const char *name, const vec3_t origin, const vec3_t angles, const vec3_t scale, const vec3_t lightingPrecalc)
{
    enum {
        R_STATIC_MODEL_PREFIX_LENGTH = 6,
        R_STATIC_MODEL_PREFIX_AND_SEPARATOR_LENGTH = 7,
        R_STATIC_MODEL_MAX_PATH_LENGTH = 57
    };
    static const char xmodelPrefix[] = "xmodel";

    if (coduo_crt_strnicmp(name, xmodelPrefix, R_STATIC_MODEL_PREFIX_LENGTH) != 0 ||
        (name[R_STATIC_MODEL_PREFIX_LENGTH] != '/' && name[R_STATIC_MODEL_PREFIX_LENGTH] != '\\')) {
        Com_Printf("Model '%s' is not an xmodel\n", name);
        return;
    }

    if (strlen(name) >= R_STATIC_MODEL_MAX_PATH_LENGTH) {
        Com_Printf("Model '%s' has a name longer than %i characters\n", name, R_STATIC_MODEL_MAX_PATH_LENGTH);
        return;
    }

    R_SyncRenderThread();
    renderer_static_model_instance_t *instance = ri.Z_Malloc(sizeof(*instance));
    strcpy(instance->name, name + R_STATIC_MODEL_PREFIX_AND_SEPARATOR_LENGTH);

    const int16_t modelHandle = (int16_t)RE_RegisterModel(name, R_STATIC_MODEL_SHADER_USAGE);
    DObjModel model = {0};
    DObj obj;

    model.model = R_GetModelByHandle(modelHandle)->xmodel;
    /* 0x00519573 stores the renderer handle in the DObj descriptor's signed
     * model-index word. R_DObjGetSurfIndex later uses this value to recover
     * the renderer model and its per-LOD shader handles. */
    model.modelIndex = modelHandle;
    DObjCreate(&model, 1, NULL, &obj, 0);

    dobj_eval_storage_t *storage = CODUOMP_ALLOCA((size_t)DObjGetAllocSkelSize(&obj));
    DObjCreateSkel(&obj, storage);

    uint32_t partBits[DOBJ_PART_BITSET_WORD_COUNT];
    Com_Memset(partBits, 0xff, sizeof(partBits));
    DObjCalcAnim(&obj, partBits);
    DObjCalcSkel(&obj, partBits);

    instance->origin[0] = origin[0];
    instance->origin[1] = origin[1];
    instance->origin[2] = origin[2];
    AnglesToAxis(angles, instance->axis);
    instance->nonNormalizedAxes = 0.0f;

    if (scale[0] != 1.0f || scale[1] != 1.0f || scale[2] != 1.0f) {
        for (int32_t axis = 0; axis < 3; ++axis) {
            instance->axis[axis][0] *= scale[axis];
            instance->axis[axis][1] *= scale[axis];
            instance->axis[axis][2] *= scale[axis];
        }
        instance->nonNormalizedAxes = VectorMax(scale);
    }

    R_GetXModelBounds(&obj, instance->axis, instance->mins, instance->maxs);
    for (int32_t axis = 0; axis < 3; ++axis) {
        instance->mins[axis] += instance->origin[axis];
        instance->maxs[axis] += instance->origin[axis];
    }

    instance->lightingPrecalc[0] = lightingPrecalc[0];
    instance->lightingPrecalc[1] = lightingPrecalc[1];
    instance->lightingPrecalc[2] = lightingPrecalc[2];
    instance->registration = NULL;

    if (XModelBad(model.model) == qfalse)
        instance->registration = R_RegisterStaticModel(instance->name, &obj);

    instance->next = rendererStaticModelInstances;
    rendererStaticModelInstances = instance;
}

/* Source: CoDUOMP.exe 0x00519790..0x00519b6b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00519790_00519b6c.mcode and direct
 * objdump of the original executable. Exact same-module Mac symbol
 * R_AddStaticModelToWorld. Windows proves the optimized-registration and
 * fallback-DObj paths, variable surface-cache allocation, renderer-entity
 * construction, autosprite bounds expansion, precomputed color conversion,
 * BSP-cell insertion, and static-light candidate setup. */
void R_AddStaticModelToWorld(renderer_static_model_instance_t *instance)
{
    int32_t surfaceCacheCount = 0;
    DObj *obj = NULL;
    refEntityType_t entityType;

    if (instance->registration != NULL) {
        entityType = RT_STATIC_MODEL;
        for (int32_t lodIndex = 0; lodIndex < instance->registration->lodCount; ++lodIndex) {
            surfaceCacheCount += instance->registration->lods[lodIndex]->surfaceCount;
        }
    } else {
        entityType = RT_MODEL;
        obj = ri.Hunk_Alloc(sizeof(*obj));

        const int16_t modelHandle = (int16_t)RE_RegisterModel(va("xmodel/%s", instance->name), R_STATIC_MODEL_SHADER_USAGE);
        DObjModel model = {0};
        model.model = R_GetModelByHandle(modelHandle)->xmodel;
        /* 0x005197fa stores this handle in the descriptor. Leaving the word
         * zero makes R_AddXModelSurfaces reject every fallback surface as
         * belonging to renderer model zero (MODEL_BAD). */
        model.modelIndex = modelHandle;
        DObjCreate(&model, 1, NULL, obj, 0);

        dobj_eval_storage_t *storage = ri.Hunk_Alloc((size_t)DObjGetAllocSkelSize(obj));
        DObjCreateSkel(obj, storage);

        uint32_t partBits[DOBJ_PART_BITSET_WORD_COUNT];
        Com_Memset(partBits, 0xff, sizeof(partBits));
        DObjCalcAnim(obj, partBits);
        DObjCalcSkel(obj, partBits);
    }

    renderer_static_model_t *worldModel =
        ri.Hunk_Alloc(sizeof(*worldModel) + (size_t)surfaceCacheCount * sizeof(worldModel->surfaceLightingCache[0]));

    /* The original clears exactly the 0x9c-byte refEntity prefix. The light
     * arrays and variable surface cache remain owned, lazily filled storage. */
    Com_Memset(&worldModel->entity, 0, sizeof(worldModel->entity));
    worldModel->entity.reType = entityType;
    worldModel->entity.renderfx = RF_LIGHTING_ORIGIN;
    worldModel->entity.dobj = obj;
    worldModel->entity.staticModelRegistration = instance->registration;

    for (int32_t axis = 0; axis < 3; ++axis) {
        worldModel->entity.origin[axis] = instance->origin[axis];
        for (int32_t component = 0; component < 3; ++component) {
            worldModel->entity.axis[axis][component] = instance->axis[axis][component];
        }
    }
    worldModel->entity.nonNormalizedAxes = instance->nonNormalizedAxes;

    R_AdjustBoundsForAutosprite(&worldModel->entity, instance->mins, instance->maxs);
    for (int32_t axis = 0; axis < 3; ++axis) {
        worldModel->entity.lightingOrigin[axis] = (instance->mins[axis] + instance->maxs[axis]) * R_STATIC_MODEL_CENTER_SCALE;
    }

    for (int32_t component = 0; component < 3; ++component) {
        float color = tr.identityLight * instance->lightingPrecalc[component] * R_STATIC_MODEL_COLOR_SCALE;
        if (color > R_STATIC_MODEL_COLOR_SCALE) {
            color = R_STATIC_MODEL_COLOR_SCALE;
        } else if (color < 0.0f) {
            color = 0.0f;
        }
        worldModel->entity.shaderRGBA[component] = (uint8_t)(int32_t)color;
    }
    worldModel->entity.shaderRGBA[3] = UINT8_MAX;

    for (int32_t axis = 0; axis < 3; ++axis) {
        worldModel->mins[axis] = instance->mins[axis];
        worldModel->maxs[axis] = instance->maxs[axis];
    }
    worldModel->viewCount = 0;

    R_FilterStaticModelIntoCells_r(tr.world, tr.world->nodes, worldModel, worldModel->mins, worldModel->maxs);
    worldModel->lightCount = R_GetStaticLightContributions(worldModel->entity.lightingOrigin, worldModel->contributions,
                                                           &worldModel->diffuseSunContribution, worldModel->lights);
}

/* Source: CoDUOMP.exe 0x00519b70..0x00519d16.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00519b70_00519d17.mcode and direct
 * objdump of the original executable. Exact same-module Mac symbol
 * R_FinishLoadingStaticModels. Registrations whose first LOD has not yet been
 * built are finished through a temporary evaluated DObj; every pending world
 * instance is then installed and released from the renderer zone. */
void R_FinishLoadingStaticModels(void)
{
    for (int32_t modelIndex = 0; modelIndex < tr.registeredStaticModelCount; ++modelIndex) {
        renderer_registered_static_model_t *registration = tr.registeredStaticModels[modelIndex];

        if (registration->lods[0] != NULL)
            continue;

        const int16_t modelHandle = (int16_t)RE_RegisterModel(va("xmodel/%s", registration->name), R_STATIC_MODEL_SHADER_USAGE);
        DObjModel model = {0};
        DObj obj;

        model.model = R_GetModelByHandle(modelHandle)->xmodel;
        /* Exact descriptor word store at 0x00519bd2. */
        model.modelIndex = modelHandle;
        DObjCreate(&model, 1, NULL, &obj, 0);

        dobj_eval_storage_t *storage = ri.Hunk_AllocateTempMemory((size_t)DObjGetAllocSkelSize(&obj));
        DObjCreateSkel(&obj, storage);

        uint32_t partBits[DOBJ_PART_BITSET_WORD_COUNT];
        Com_Memset(partBits, 0xff, sizeof(partBits));
        DObjCalcAnim(&obj, partBits);
        DObjCalcSkel(&obj, partBits);
        R_FinishDObjToStaticModel(registration, &obj);
        ri.Hunk_FreeTempMemory(storage);
    }

    renderer_static_model_instance_t *instance = rendererStaticModelInstances;
    while (instance != NULL) {
        renderer_static_model_instance_t *next = instance->next;
        R_AddStaticModelToWorld(instance);
        ri.Z_Free(instance);
        instance = next;
    }
    rendererStaticModelInstances = NULL;
}

/* Source: CoDUOMP.exe 0x0051b5c0..0x0051ba0b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051b5c0_0051ba0c.mcode.
 * Exact same-module Mac symbol R_RefreshStaticModels_ARB. Windows proves the
 * registry/LOD/surface traversal, the requirement that both ARB buffer names
 * exist, the independent index/vertex flag bits, and the single upload used
 * for each selected buffer in the full refresh path. */
void R_RefreshStaticModels_ARB(renderer_vbo_refresh_components_t refreshComponents)
{
    for (int32_t modelIndex = 0; modelIndex < tr.registeredStaticModelCount; ++modelIndex) {
        renderer_registered_static_model_t *registration = tr.registeredStaticModels[modelIndex];

        for (int32_t lodIndex = 0; lodIndex < registration->lodCount; ++lodIndex) {
            renderer_static_model_lod_t *lod = registration->lods[lodIndex];

            for (int32_t surfaceIndex = 0; surfaceIndex < lod->surfaceCount; ++surfaceIndex) {
                renderer_static_model_surface_t *surface = &lod->surfaces[surfaceIndex];

                if (surface->optimized.vertexBuffer == 0 || surface->backend.arb.indexBuffer == 0) {
                    continue;
                }

                R_RefreshStaticModelSurface_ARB(surface, refreshComponents, qfalse);
            }
        }
    }
}

/* Source: CoDUOMP.exe 0x0051ba10..0x0051be85.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051ba10_0051be86.mcode.
 * Exact same-module Mac symbol R_IncrementalRefreshStaticModels_ARB. The
 * persistent cursor advances its surface component before selection, skips
 * surfaces without both ARB buffers, and refreshes at most one surface per
 * call. Pending zone-owned static-model instances suppress the pass. Unlike
 * the full refresh, every selected buffer is orphaned before it is refilled.
 * Windows addresses the independent static-model cursor at
 * 0x04887b08/0x04887b0c/0x04887b10; the XModel cursor is the preceding three
 * words at 0x04887afc/0x04887b00/0x04887b04. */
void R_IncrementalRefreshStaticModels_ARB(renderer_vbo_refresh_components_t refreshComponents)
{
    const int32_t initialModelIndex = tr.staticModelRefreshModelIndex;

    if (tr.registeredStaticModelCount == 0 || rendererStaticModelInstances != NULL) {
        return;
    }

    renderer_static_model_surface_t *surface;
    do {
        ++tr.staticModelRefreshSurfaceIndex;

        renderer_registered_static_model_t *registration = tr.registeredStaticModels[tr.staticModelRefreshModelIndex];
        if (registration->model == NULL)
            return;

        renderer_static_model_lod_t *lod = registration->lods[tr.staticModelRefreshLodIndex];
        if (tr.staticModelRefreshSurfaceIndex >= lod->surfaceCount) {
            tr.staticModelRefreshSurfaceIndex = 0;
            ++tr.staticModelRefreshLodIndex;

            if (tr.staticModelRefreshLodIndex >= registration->lodCount) {
                tr.staticModelRefreshLodIndex = 0;
                ++tr.staticModelRefreshModelIndex;

                if (tr.staticModelRefreshModelIndex >= tr.registeredStaticModelCount) {
                    tr.staticModelRefreshModelIndex = 0;
                }

                if (tr.staticModelRefreshModelIndex == initialModelIndex) {
                    return;
                }
            }

            registration = tr.registeredStaticModels[tr.staticModelRefreshModelIndex];
            lod = registration->lods[tr.staticModelRefreshLodIndex];
        }

        surface = &lod->surfaces[tr.staticModelRefreshSurfaceIndex];
    } while (surface->optimized.vertexBuffer == 0 || surface->backend.arb.indexBuffer == 0);

    R_RefreshStaticModelSurface_ARB(surface, refreshComponents, qtrue);
}

/* Source: CoDUOMP.exe 0x00519d20..0x00519d51.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00519d20_00519d52.mcode and direct
 * objdump of the original executable. Exact same-module Mac symbol
 * R_ShutdownStaticModels. Only not-yet-installed zone instances remain owned
 * here; persistent registrations and world records belong to the hunk. */
void R_ShutdownStaticModels(void)
{
    renderer_static_model_instance_t *instance = rendererStaticModelInstances;
    while (instance != NULL) {
        renderer_static_model_instance_t *next = instance->next;
        ri.Z_Free(instance);
        instance = next;
    }
    rendererStaticModelInstances = NULL;
}
