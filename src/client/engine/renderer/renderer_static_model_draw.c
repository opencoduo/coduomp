#include "backend.h"

#include "../math/vector_math.h"

/* Source data: CoDUOMP.exe 0x0058fc28. R_AddStaticModelSurfaces passes this
 * exact cyan RGBA value to both optional model-count debug strings. */
static const vec4_t staticModelCountColor = {
    0.0f, 1.0f, 1.0f, 1.0f
};

/* Source: CoDUOMP.exe 0x00519d60..0x0051a069.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00519d60_0051a06a.mcode.
 * Name and source-level loop shape: exact same-module Mac symbol
 * R_AddStaticModelSurfaces. Windows behavior is authoritative for cache
 * eligibility, lighting setup, draw-surface selection, and the two developer
 * counters. */
void R_AddStaticModelSurfaces(trRefEntity_t *entity)
{
    renderer_registered_static_model_t *registration =
        entity->e.staticModelRegistration;
    renderer_static_model_t *lighting = entity->staticModelLighting;
    const int32_t lodIndex = XModelGetLodForDist(
        registration->model, R_GetLodDist(entity));
    renderer_static_model_lod_t *lod;
    axis_t inverseAxis;
    int32_t firstSurfaceIndex = 0;
    int32_t totalIndexCount = 0;
    qboolean lightingInitialized = qfalse;
    qboolean needsLighting = qfalse;
    int32_t surfaceIndex;

    if (lodIndex < 0)
        return;

    for (int32_t earlierLod = 0; earlierLod < lodIndex; ++earlierLod)
        firstSurfaceIndex += registration->lods[earlierLod]->surfaceCount;

    lod = registration->lods[lodIndex];
    for (surfaceIndex = 0;
         surfaceIndex < lod->surfaceCount;
         ++surfaceIndex) {
        renderer_static_model_surface_t *surface =
            &lod->surfaces[surfaceIndex];
        const int32_t globalSurfaceIndex =
            firstSurfaceIndex + surfaceIndex;
        renderer_cached_static_model_surface_t **cacheSlot =
            &lighting->surfaceLightingCache[globalSurfaceIndex];
        renderer_cached_static_model_surface_t *cached = *cacheSlot;

        if (cached == NULL && surface->cachedShader != NULL) {
            if (lightingInitialized == qfalse) {
                MatrixInverse(entity->e.axis, inverseAxis);
                R_SetupStaticModelLighting(&tr.refdef, entity);
                lightingInitialized = qtrue;
            }

            if (entity->hasDynamicLights == 0) {
                cached = R_CacheStaticModelSurface(
                    surface, lighting, globalSurfaceIndex,
                    entity, inverseAxis);
                *cacheSlot = cached;
            }
        }

        if (cached == NULL) {
            R_AddDrawSurf((renderer_surface_t *)surface,
                          surface->storageSource, surface->shader,
                          0, 0, 0);
            if ((surface->shader->lightingFlags &
                 SHADER_LIGHTING_ENTITY_MASK) != 0) {
                needsLighting = qtrue;
            }
        } else {
            R_UsedCachedStaticModelSurface(cached);

            if ((surface->shader->lightingFlags &
                 SHADER_LIGHTING_ENTITY_MASK) == 0) {
                R_AddDrawSurf((renderer_surface_t *)cached,
                              tr.cachedStaticModelStorageSource,
                              surface->cachedShader, 0, 0, 0);
            } else {
                R_SetupStaticModelLighting(&tr.refdef, entity);
                if (entity->hasDynamicLights == 0) {
                    R_AddDrawSurf((renderer_surface_t *)cached,
                                  tr.cachedStaticModelStorageSource,
                                  surface->cachedShader, 0, 0, 0);
                } else {
                    R_AddDrawSurf((renderer_surface_t *)surface,
                                  surface->storageSource, surface->shader,
                                  0, 0, 0);
                }
            }
        }

        totalIndexCount += surface->indexCount;
    }

    if ((entity->e.renderfx & RF_DEPTHHACK) == 0) {
        if (r_showtricounts->integer != 0) {
            R_AddScaledDebugString(entity->e.origin,
                                   staticModelCountColor,
                                   va("%i", totalIndexCount / 3));
        } else if (r_showsurfcounts->integer != 0) {
            R_AddScaledDebugString(entity->e.origin,
                                   staticModelCountColor,
                                   va("%i", lod->surfaceCount));
        }
    }

    if (needsLighting != qfalse)
        R_SetupStaticModelLighting(&tr.refdef, entity);
}
