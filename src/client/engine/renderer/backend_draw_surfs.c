#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"

/* Source: CoDUOMP.exe 0x004bec70..0x004bf1f0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bec70_004bf1f1.mcode.
 * Name: same-module Mac symbol RB_RenderDrawSurfList.
 *
 * The Windows sort word is proved directly by the shifts and masks at
 * 0x004becfb..0x004bed2c: shader[29:18], storage[31:30], entity[17:8],
 * two batch-separation bits at 0 and 2, and bit 1 selecting the renderer's
 * world entity. The four-entry depth-range behavior is proved by the jump and
 * byte tables at 0x004bf1f4..0x004bf21c. */
void RB_RenderDrawSurfList(const drawSurf_t *drawSurfs,
                           int32_t drawSurfCount)
{
    const float originalFloatTime = backEnd.refdef.floatTime;
    shader_t *previousShader = NULL;
    int32_t previousEntityNumber = -1;
    renderer_static_vertex_memory_source_t previousStorageMode =
        R_STATIC_VERTEX_MEMORY_NONE;
    uint32_t previousBatchFlag0 = 0;
    uint32_t previousBatchFlag2 = UINT32_MAX;
    uint32_t previousDepthRangeFlags = 0;
    uint32_t currentDepthRangeFlags = 0;
    uint32_t shaderFlagChanges = 0;
    int32_t drawSurfIndex;

    RB_BeginDrawingView();
    backEnd.pc.surfaceCount += drawSurfCount;
    backEnd.currentEntity = &tr.worldEntity;

    for (drawSurfIndex = 0; drawSurfIndex < drawSurfCount;
         ++drawSurfIndex) {
        const drawSurf_t *drawSurf = &drawSurfs[drawSurfIndex];
        const uint32_t sort = drawSurf->sort;
        shader_t *shader;
        int32_t entityNumber;
        renderer_static_vertex_memory_source_t storageMode;
        uint32_t batchFlag0;
        uint32_t batchFlag2;
        qboolean entityChanged;
        qboolean batchChanged;

        if (sort == UINT32_MAX) {
            const int32_t surfaceType = *(const int32_t *)drawSurf->surface;

            rb_surfaceTable[surfaceType](drawSurf->surface);
            continue;
        }

        shader = tr.sortedShaders[
            (sort >> R_SORT_SHADER_SHIFT) & R_SORT_SHADER_MASK];
        storageMode = (renderer_static_vertex_memory_source_t)(
            (sort >> R_SORT_STORAGE_SHIFT) & R_SORT_STORAGE_MASK);
        entityNumber = (int32_t)((sort >> R_SORT_ENTITY_SHIFT) &
                                 R_SORT_ENTITY_MASK);
        batchFlag0 = sort & R_SORT_BATCH_FLAG0;
        batchFlag2 = (sort & R_SORT_BATCH_FLAG2) != 0;
        if ((sort & R_SORT_WORLD_ENTITY) != 0)
            entityNumber = R_WORLD_ENTITY_NUMBER;

        entityChanged = entityNumber != previousEntityNumber;
        batchChanged = shader != previousShader ||
                       storageMode != previousStorageMode ||
                       batchFlag0 != previousBatchFlag0 ||
                       batchFlag2 != previousBatchFlag2;

        if (batchChanged ||
            (entityChanged &&
             (shader->flags & SHADER_FLAG_ENTITY_MERGABLE) == 0)) {
            if (previousShader != NULL) {
                RB_EndSurface();
                shaderFlagChanges = previousShader->flags ^ shader->flags;
            }

            if (entityNumber == R_WORLD_ENTITY_NUMBER) {
                backEnd.currentEntity = &tr.worldEntity;
            } else {
                backEnd.currentEntity = &backEnd.refdef.entities[entityNumber];
            }

            if (storageMode != glState.currentStorageMode) {
                if (glConfig.vertexArrayRangeMode !=
                    R_VERTEX_ARRAY_RANGE_NONE) {
                    RB_SelectStorageNV(storageMode);
                } else if (glConfig.vertexArrayObjectATIAvailable) {
                    RB_SelectStorageATI(storageMode);
                }
                glState.currentStorageMode = storageMode;
            }

            RB_BeginSurface(shader, R_DYNAMIC_TESS_STORAGE);
            previousShader = shader;
            previousStorageMode = storageMode;
            previousBatchFlag0 = batchFlag0;
            previousBatchFlag2 = batchFlag2;
        }

        if (entityChanged) {
            trRefEntity_t *entity;

            currentDepthRangeFlags = 0;
            if (entityNumber != R_WORLD_ENTITY_NUMBER) {
                entity = &backEnd.refdef.entities[entityNumber];
                backEnd.currentEntity = entity;
                /* 0x4bee0c..0x4bee4e stores the entity-relative time but
                 * retains it for the shader offset subtraction. */
                const long double entityTimeRaw =
                    (long double)originalFloatTime -
                    (long double)entity->e.shaderTime;
                backEnd.refdef.floatTime = (float)entityTimeRaw;
                tess.shaderTime = (float)(
                    entityTimeRaw -
                    (long double)shader->timeOffset);

                if ((shader->flags & SHADER_FLAG_ENTITY_MERGABLE) != 0) {
                    backEnd.orientation = backEnd.viewParms.world;
                } else if (entity->e.reType >= RT_BRUSH_MODEL &&
                           entity->e.reType <= RT_STATIC_MODEL) {
                    R_RotateForEntity(entity, &backEnd.viewParms,
                                      &backEnd.orientation);
                } else {
                    backEnd.orientation = backEnd.viewParms.world;
                }

                if (entity->dlightBits != 0U) {
                    R_TransformDlights(backEnd.refdef.entityDlightCount,
                                       backEnd.refdef.dlights,
                                       &backEnd.orientation);
                }

                currentDepthRangeFlags =
                    (uint32_t)entity->e.renderfx &
                    RF_DEPTH_RANGE_FLAGS;

                /* 0x4beec6 TEST [shader+0x54],0x18; JE else; CALL 0x4bea30
                 * (RB_EnableHWLights): the shader-lighting conditional belongs to the
                 * NON-world arm only. A prior pass hoisted it below the if/else so it
                 * also ran for the world entity; the DLL's world arm instead does an
                 * unconditional GL_Normalize(0) (0x4bef9c) -- no lightingFlags test and
                 * no HW-lights. */
                if ((shader->lightingFlags &
                     SHADER_LIGHTING_ENTITY_MASK) != 0) {
                    RB_EnableHWLights();
                    GL_Normalize(entity->normalizationTarget);
                } else {
                    GL_Normalize(0);
                }
            } else {
                entity = &tr.worldEntity;
                backEnd.refdef.floatTime = originalFloatTime;
                backEnd.currentEntity = entity;
                backEnd.orientation = backEnd.viewParms.world;
                tess.shaderTime = originalFloatTime - shader->timeOffset;
                R_TransformDlights(backEnd.refdef.num_dlights,
                                   backEnd.refdef.dlights,
                                   &backEnd.orientation);
                GL_Normalize(0);
            }

            qglLoadMatrixf(backEnd.orientation.modelMatrix);

            if (currentDepthRangeFlags != previousDepthRangeFlags) {
                double depthRangeFar;

                switch (currentDepthRangeFlags) {
                case 0:
                    depthRangeFar = 1.0;
                    break;
                case RF_DEPTHHACK:
                    qglMatrixMode(GL_PROJECTION);
                    qglLoadMatrixf(
                        backEnd.viewParms.depthHackProjectionMatrix);
                    qglMatrixMode(GL_MODELVIEW);
                    depthRangeFar = 0.20000000298023224;
                    break;
                case RF_CROSSHAIR:
                case RF_DEPTHHACK | RF_CROSSHAIR:
                    depthRangeFar = 0.5;
                    break;
                default:
                    /* Masking above makes this unreachable. */
                    depthRangeFar = 1.0;
                    break;
                }

                qglDepthRange(0.0, depthRangeFar);
                if (previousDepthRangeFlags == RF_DEPTHHACK) {
                    qglMatrixMode(GL_PROJECTION);
                    qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
                    qglMatrixMode(GL_MODELVIEW);
                }
                previousDepthRangeFlags = currentDepthRangeFlags;
            }

            previousEntityNumber = entityNumber;
        } else {
            trRefEntity_t *entity = backEnd.currentEntity;

            if ((shader->lightingFlags &
                 SHADER_LIGHTING_ENTITY_MASK) != 0) {
                if (RB_EnableHWLights())
                    qglLoadMatrixf(backEnd.orientation.modelMatrix);
                GL_Normalize(entity->normalizationTarget);
            } else {
                GL_Normalize(0);
            }

            if (entityNumber != R_WORLD_ENTITY_NUMBER &&
                (shaderFlagChanges & SHADER_FLAG_ENTITY_MERGABLE) != 0) {
                if ((shader->flags &
                     SHADER_FLAG_ENTITY_MERGABLE) != 0) {
                    backEnd.orientation = backEnd.viewParms.world;
                } else if (entity->e.reType >= RT_BRUSH_MODEL &&
                           entity->e.reType <= RT_STATIC_MODEL) {
                    R_RotateForEntity(entity, &backEnd.viewParms,
                                      &backEnd.orientation);
                } else {
                    backEnd.orientation = backEnd.viewParms.world;
                }
                qglLoadMatrixf(backEnd.orientation.modelMatrix);
            }
        }

        {
            const int32_t surfaceType = *(const int32_t *)drawSurf->surface;

            rb_surfaceTable[surfaceType](drawSurf->surface);
        }
    }

    if (previousShader != NULL)
        RB_EndSurface();

    if (tr.defaultStorageMode != glState.currentStorageMode) {
        if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE) {
            RB_SelectStorageNV(tr.defaultStorageMode);
        } else if (glConfig.vertexArrayObjectATIAvailable) {
            RB_SelectStorageATI(tr.defaultStorageMode);
        }
        glState.currentStorageMode = tr.defaultStorageMode;
    }

    backEnd.refdef.floatTime = originalFloatTime;
    backEnd.currentEntity = &tr.worldEntity;
    backEnd.orientation = backEnd.viewParms.world;
    R_TransformDlights(backEnd.refdef.num_dlights,
                       backEnd.refdef.dlights,
                       &backEnd.orientation);
    qglLoadMatrixf(backEnd.viewParms.world.modelMatrix);
    if (currentDepthRangeFlags != 0)
        qglDepthRange(0.0, 1.0);

    RB_ShadowFinish();
    RB_RenderFlares();
}
