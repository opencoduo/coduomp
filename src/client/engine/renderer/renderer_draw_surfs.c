#include "backend.h"

#include "../client/debug_lines.h"
#include "gl_state.h"

#include <math.h>
#include <stddef.h>

enum {
    R_FAST_SORT_CUTOFF = 8,
    R_FAST_SORT_STACK_DEPTH = 30,
    R_COMMAND_BUFFER_USABLE_BYTES = 262136
};

/* Source data: CoDUOMP.exe 0x005ce958. R_AddEntitySurfaces is its only PE
 * address reference, and RB_SurfaceEntity consumes only this type dword before
 * dispatching through backEnd.currentEntity. */
static renderer_surface_t rendererEntitySurface = {
    R_SURFACE_ENTITY
};

/* Source: CoDUOMP.exe 0x0051f6d0..0x0051f6eb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051f6d0_0051f6ec.mcode.
 * Name and two-argument signature: exact same-module Mac symbol
 * R_DObjGetSurfIndex. The Windows compiler also inlines this body into
 * R_AddXModelSurfaces at 0x0051ffb7 and 0x00520159. */
int32_t R_DObjGetSurfIndex(const DObj *obj, int32_t modelIndex)
{
    int32_t surfaceModelIndex = obj->modelIndices[modelIndex];

    if (surfaceModelIndex < 0)
        surfaceModelIndex = ri.CG_GetGameModel(
            (int16_t)-surfaceModelIndex);
    return surfaceModelIndex;
}

/* Source: CoDUOMP.exe 0x004e67a0..0x004e6827.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e67a0_004e6828.mcode.
 * Name and entity argument: same-module Mac symbol R_GetLodDist. */
float R_GetLodDist(const trRefEntity_t *entity)
{
    vec3_t delta;
    float lodDistance;

    delta[0] = tr.viewParms.orientation.origin[0] - entity->e.origin[0];
    delta[1] = tr.viewParms.orientation.origin[1] - entity->e.origin[1];
    delta[2] = tr.viewParms.orientation.origin[2] - entity->e.origin[2];
    /* The PE accumulates Z, then Y, then X on the x87 stack. Keep that
     * association explicit so host builds do not silently change rounding. */
    lodDistance = sqrtf((delta[2] * delta[2] +
                         delta[1] * delta[1]) +
                        delta[0] * delta[0]);
    lodDistance = lodDistance * tr.viewParms.lodScale +
                  tr.viewParms.lodBias;
    if (entity->e.nonNormalizedAxes != 0.0f)
        lodDistance /= entity->e.nonNormalizedAxes;
    return lodDistance;
}

/* NOT_FROM_ORIGINAL_SOURCE: typed source factoring shared by the two original
 * XModel debug routines. It composes the evaluated DObj matrix with the
 * refEntity axis/origin in the exact order used by both PE bodies. */
static void R_XModelDebugPointToWorld(
    const trRefEntity_t *entity,
    const DObjSkelMat *boneMatrix,
    const vec3_t localPoint, vec3_t worldPoint)
{
    vec3_t modelPoint;

    /* Both PE bodies accumulate Z, then Y, then X for each component. This
     * matters for exact float rounding when drawing a box corner. */
    modelPoint[0] =
        (localPoint[2] * boneMatrix->axis[2][0] +
         localPoint[1] * boneMatrix->axis[1][0]) +
        localPoint[0] * boneMatrix->axis[0][0] +
        boneMatrix->origin[0];
    modelPoint[1] =
        (localPoint[2] * boneMatrix->axis[2][1] +
         localPoint[1] * boneMatrix->axis[1][1]) +
        localPoint[0] * boneMatrix->axis[0][1] +
        boneMatrix->origin[1];
    modelPoint[2] =
        (localPoint[2] * boneMatrix->axis[2][2] +
         localPoint[1] * boneMatrix->axis[1][2]) +
        localPoint[0] * boneMatrix->axis[0][2] +
        boneMatrix->origin[2];
    worldPoint[0] =
        (modelPoint[2] * entity->e.axis[2][0] +
         modelPoint[1] * entity->e.axis[1][0]) +
        modelPoint[0] * entity->e.axis[0][0] +
        entity->e.origin[0];
    worldPoint[1] =
        (modelPoint[2] * entity->e.axis[2][1] +
         modelPoint[1] * entity->e.axis[1][1]) +
        modelPoint[0] * entity->e.axis[0][1] +
        entity->e.origin[1];
    worldPoint[2] =
        (modelPoint[2] * entity->e.axis[2][2] +
         modelPoint[1] * entity->e.axis[1][2]) +
        modelPoint[0] * entity->e.axis[0][2] +
        entity->e.origin[2];
}

/* Source: CoDUOMP.exe 0x0051f800..0x0051faca.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051f800_0051facb.mcode.
 * Name: same-module Mac symbol R_XModelDebugBoxes. */
void R_XModelDebugBoxes(trRefEntity_t *entity,
                        const uint32_t *partBits)
{
    /* Source: CoDUOMP.exe 0x005cec58..0x005ced77. The original table contains
     * one min/max selector for each axis of both endpoints of all 12 edges. */
    static const int32_t rendererBoxCornerSelectors[12][2][3] = { /* 0x005cec58 */
        {{0, 0, 0}, {1, 0, 0}}, {{0, 0, 0}, {0, 1, 0}},
        {{1, 1, 0}, {1, 0, 0}}, {{1, 1, 0}, {0, 1, 0}},
        {{0, 0, 1}, {1, 0, 1}}, {{0, 0, 1}, {0, 1, 1}},
        {{1, 1, 1}, {1, 0, 1}}, {{1, 1, 1}, {0, 1, 1}},
        {{0, 0, 0}, {0, 0, 1}}, {{1, 0, 0}, {1, 0, 1}},
        {{0, 1, 0}, {0, 1, 1}}, {{1, 1, 0}, {1, 1, 1}}
    };
    static const vec4_t boxColor = {1.0f, 1.0f, 1.0f, 0.0f};
    DObj *obj = entity->e.dobj;
    XModelPartColl **partCollisions = CODUOMP_ALLOCA(
        (size_t)obj->boneCount * sizeof(*partCollisions));
    DObjSkelMat *boneMatrices =
        &obj->evaluationStorage
             ->partSpans[obj->modelPartBaseIndices[0]].basePose;

    DObjGetBoneInfo(obj, partCollisions);
    for (int32_t boneIndex = 0;
         boneIndex < obj->boneCount; ++boneIndex) {
        const XModelPartColl *collision;

        if ((((const uint8_t *)partBits)[boneIndex >> 3] &
             (uint8_t)(1U << (boneIndex & 7))) == 0) {
            continue;
        }

        collision = partCollisions[boneIndex];
        for (int32_t edgeIndex = 0; edgeIndex < 12; ++edgeIndex) {
            vec3_t worldPoints[2];

            for (int32_t endpoint = 0; endpoint < 2; ++endpoint) {
                vec3_t localPoint;

                for (int32_t axis = 0; axis < 3; ++axis) {
                    localPoint[axis] =
                        rendererBoxCornerSelectors[edgeIndex][endpoint][axis]
                        ? collision->maxs[axis] : collision->mins[axis];
                }
                R_XModelDebugPointToWorld(
                    entity, &boneMatrices[boneIndex], localPoint,
                    worldPoints[endpoint]);
            }

            CL_AddDebugLine(worldPoints[0], worldPoints[1], boxColor,
                            qfalse, 0, qfalse);
        }
    }
}

/* Source: CoDUOMP.exe 0x0051fad0..0x0051fd6b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051fad0_0051fd6c.mcode.
 * Name: same-module Mac symbol R_XModelDebugAxes. Each selected bone emits
 * the transformed local X/Y/Z axes at the exact original length of 6.0f. */
void R_XModelDebugAxes(trRefEntity_t *entity,
                       const uint32_t *partBits)
{
    DObj *obj = entity->e.dobj;
    DObjSkelMat *boneMatrices =
        &obj->evaluationStorage
             ->partSpans[obj->modelPartBaseIndices[0]].basePose;

    for (int32_t boneIndex = 0;
         boneIndex < obj->boneCount; ++boneIndex) {
        if ((((const uint8_t *)partBits)[boneIndex >> 3] &
             (uint8_t)(1U << (boneIndex & 7))) == 0) {
            continue;
        }

        for (int32_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
            vec3_t localStart = {0.0f, 0.0f, 0.0f};
            vec3_t localEnd = {0.0f, 0.0f, 0.0f};
            vec3_t worldStart;
            vec3_t worldEnd;
            vec4_t color = {0.0f, 0.0f, 0.0f, 0.0f};

            localEnd[axisIndex] = 6.0f;
            color[axisIndex] = 1.0f;
            R_XModelDebugPointToWorld(
                entity, &boneMatrices[boneIndex], localStart, worldStart);
            R_XModelDebugPointToWorld(
                entity, &boneMatrices[boneIndex], localEnd, worldEnd);
            CL_AddDebugLine(worldStart, worldEnd, color,
                            qfalse, 0, qfalse);
        }
    }
}

/* Source: CoDUOMP.exe 0x004e5640..0x004e5684.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e5640_004e5685.mcode.
 * Name: same-module Mac symbol shortsort. This is the draw-surface-specific
 * selection sort used for partitions of at most eight entries. Its unsigned
 * sort-word comparison and choice of the first maximum match the PE. */
static void shortsort(drawSurf_t *lo, drawSurf_t *hi)
{
    while (hi > lo) {
        drawSurf_t *maximum = lo;

        for (drawSurf_t *entry = lo + 1; entry <= hi; ++entry) {
            if (entry->sort > maximum->sort)
                maximum = entry;
        }

        {
            const drawSurf_t temporary = *maximum;
            *maximum = *hi;
            *hi = temporary;
        }
        --hi;
    }
}

/* Source: CoDUOMP.exe 0x004e5690..0x004e57c2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e5690_004e57c3.mcode.
 * Name: same-module Mac symbol qsortFast. The explicit partition stack,
 * middle-element pivot, unsigned comparisons, and strict larger-partition
 * choice preserve the PE's ordering for duplicate sort words. The Windows
 * compiler carries sizeof(drawSurf_t) in EBX; typed pointer stepping gives
 * the same eight-byte stride on i386 and the correct native stride on 64-bit. */
static void qsortFast(drawSurf_t *base, size_t count)
{
    drawSurf_t *lowStack[R_FAST_SORT_STACK_DEPTH];
    drawSurf_t *highStack[R_FAST_SORT_STACK_DEPTH];
    int32_t stackDepth = 0;
    drawSurf_t *lo;
    drawSurf_t *hi;

    if (count < 2)
        return;

    lo = base;
    hi = base + count - 1;

    for (;;) {
        const size_t partitionCount = (size_t)(hi - lo) + 1;

        if (partitionCount <= R_FAST_SORT_CUTOFF) {
            shortsort(lo, hi);
            if (stackDepth == 0)
                return;

            --stackDepth;
            lo = lowStack[stackDepth];
            hi = highStack[stackDepth];
            continue;
        }

        {
            drawSurf_t *middle = lo + partitionCount / 2;
            drawSurf_t *lowCursor = lo;
            drawSurf_t *highCursor = hi + 1;
            drawSurf_t temporary = *middle;

            *middle = *lo;
            *lo = temporary;

            for (;;) {
                do {
                    ++lowCursor;
                } while (lowCursor <= hi && lowCursor->sort <= lo->sort);

                do {
                    --highCursor;
                } while (highCursor > lo && highCursor->sort >= lo->sort);

                if (highCursor < lowCursor)
                    break;

                temporary = *lowCursor;
                *lowCursor = *highCursor;
                *highCursor = temporary;
            }

            temporary = *lo;
            *lo = *highCursor;
            *highCursor = temporary;

            /* The PE compares byte spans as (highCursor - 1 - lo) against
             * (hi - lowCursor). With fixed-size records this makes the left
             * side larger only when it contains strictly more elements. */
            if ((highCursor - lo) > (hi - lowCursor)) {
                if (lo + 1 < highCursor) {
                    lowStack[stackDepth] = lo;
                    highStack[stackDepth] = highCursor - 1;
                    ++stackDepth;
                }
                if (lowCursor < hi) {
                    lo = lowCursor;
                    continue;
                }
            } else {
                if (lowCursor < hi) {
                    lowStack[stackDepth] = lowCursor;
                    highStack[stackDepth] = hi;
                    ++stackDepth;
                }
                if (lo + 1 < highCursor) {
                    hi = highCursor - 1;
                    continue;
                }
            }
        }

        if (stackDepth == 0)
            return;
        --stackDepth;
        lo = lowStack[stackDepth];
        hi = highStack[stackDepth];
    }
}

/* Source: CoDUOMP.exe 0x004e57d0..0x004e5829.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e57d0_004e582a.mcode.
 * Name and source parameter order: same-module Mac symbol R_AddDrawSurf. The
 * sort-word construction is written from the shifts proved by the PE rather
 * than relying on the host layout of drawSurf_t. */
void R_AddDrawSurf(renderer_surface_t *surface, int32_t storageMode,
                   shader_t *shader, uint32_t batchFlag0,
                   uint32_t batchFlag2, uint32_t worldEntity)
{
    drawSurf_t *drawSurf;

    if (tr.refdef.numDrawSurfs >= R_MAX_DRAW_SURFS)
        return;

    drawSurf = &tr.refdef.drawSurfs[tr.refdef.numDrawSurfs];
    drawSurf->sort =
        ((uint32_t)storageMode << R_SORT_STORAGE_SHIFT) |
        ((uint32_t)shader->sortedIndex << R_SORT_SHADER_SHIFT) |
        (batchFlag2 << 2) |
        (worldEntity << 1) |
        tr.shiftedEntityNumber |
        batchFlag0;
    drawSurf->surface = surface;
    ++tr.refdef.numDrawSurfs;
}

/* Source: CoDUOMP.exe 0x004e5830..0x004e5885, exporter-gap recovery.
 * Name and parameter order: exact same-module Mac symbol R_DecomposeSort.
 * The PPC stores r3's fields through r4..r8 in the order represented below;
 * the Windows optimizer gives the emitted helper a private register convention
 * and inlines the same extraction into its live callers. */
void R_DecomposeSort(
    uint32_t sort, renderer_static_vertex_memory_source_t *storageMode,
    int32_t *entityNumber, shader_t **shader, uint32_t *batchFlag0,
    uint32_t *batchFlag2)
{
    *storageMode = (renderer_static_vertex_memory_source_t)(
        (sort >> R_SORT_STORAGE_SHIFT) & R_SORT_STORAGE_MASK);
    *entityNumber =
        (int32_t)((sort >> R_SORT_ENTITY_SHIFT) & R_SORT_ENTITY_MASK);
    *shader = tr.sortedShaders[
        (sort >> R_SORT_SHADER_SHIFT) & R_SORT_SHADER_MASK];
    *batchFlag0 = sort & R_SORT_BATCH_FLAG0;
    *batchFlag2 = (sort & R_SORT_BATCH_FLAG2) != 0;

    if ((sort & R_SORT_WORLD_ENTITY) != 0)
        *entityNumber = R_WORLD_ENTITY_NUMBER;
}

/* Source: CoDUOMP.exe 0x0050ef60..0x0050efce.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050ef60_0050efcf.mcode.
 * Name and record roles: same-module Mac symbol R_AddWorldSurfaceNoCull. For
 * surface kinds below 24 the PE has inlined R_DlightSurface's proved result
 * of zero; kinds 24 and above retain only lights overlapping their bounds. */
void R_AddWorldSurfaceNoCull(msurface_t *worldSurface,
                             uint32_t dlightBits)
{
    renderer_surface_t *surface = worldSurface->data;
    int32_t storageMode = tr.defaultStorageMode;
    uint32_t hasDlights = 0;

    worldSurface->viewCount = tr.viewCount;

    if (surface->surfaceType >= R_SURFACE_INDEXED_POSITION_FIRST) {
        renderer_lit_surface_t *litSurface =
            (renderer_lit_surface_t *)surface;

        storageMode = litSurface->storageMode;
        if (dlightBits != 0) {
            litSurface->dlightBits = R_CullDlightsForBox(
                litSurface->boundsMin, litSurface->boundsMax, dlightBits);
            hasDlights = litSurface->dlightBits != 0;
        }
    }

    R_AddDrawSurf(surface, storageMode, worldSurface->shader,
                  hasDlights, 0, 0);
}

/* Source: CoDUOMP.exe 0x0051f590..0x0051f5ac.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051f590_0051f5ad.mcode.
 * Name: exact same-module Mac symbol R_CullModel. Disabling XModel drawing
 * deliberately reports CULL_OUT; otherwise DPVS supplied the stored result. */
cull_result_t R_CullModel(const trRefEntity_t *entity)
{
    return r_drawXModels->integer == 0
        ? CULL_OUT
        : entity->cullState;
}

/* Source: CoDUOMP.exe 0x0051ca00..0x0051ca25.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051ca00_0051ca26.mcode.
 * Exact source name is absent from the Mac symbol set; the role-proven name
 * describes the separately emitted helper that LTCG inlines into
 * R_AddWorldSurface. Only lit world-surface kinds have the bounds prefix. */
qboolean R_CullWorldSurface(const renderer_surface_t *surface)
{
    if (r_nocull->integer != 0 ||
        surface->surfaceType < R_SURFACE_INDEXED_POSITION_FIRST) {
        return qfalse;
    }

    const renderer_lit_surface_t *litSurface =
        (const renderer_lit_surface_t *)surface;
    return R_CullLocalBox(
               (const vec3_t *)(const void *)&litSurface->boundsMin) ==
           CULL_OUT;
}

/* Source: CoDUOMP.exe 0x0051cd40..0x0051cdd6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051cd40_0051cdd7.mcode.
 * Exact source name is absent from the Mac symbol set; this is the proved
 * culled counterpart to R_AddWorldSurfaceNoCull. The Windows body inlines
 * R_CullWorldSurface and R_DlightSurface and retains the original input-mask
 * boolean as the draw-surface batch flag. */
void R_AddWorldSurface(msurface_t *worldSurface,
                       uint32_t dlightBits)
{
    renderer_surface_t *surface;
    int32_t storageMode = tr.defaultStorageMode;
    qboolean hasDlights = qfalse;

    if (worldSurface->viewCount == tr.viewCount)
        return;
    worldSurface->viewCount = tr.viewCount;

    surface = worldSurface->data;
    if (R_CullWorldSurface(surface) != qfalse)
        return;

    if (surface->surfaceType >= R_SURFACE_INDEXED_POSITION_FIRST) {
        renderer_lit_surface_t *litSurface =
            (renderer_lit_surface_t *)surface;
        storageMode = litSurface->storageMode;
        if (dlightBits != 0)
            hasDlights = R_DlightSurface(worldSurface, dlightBits);
    }

    R_AddDrawSurf(surface, storageMode, worldSurface->shader,
                  (uint32_t)hasDlights, 0, 0);
}

/* Source: CoDUOMP.exe 0x0051ced0..0x0051cff1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051ced0_0051cff2.mcode.
 * Exact source name is absent from the Mac symbol set. The behavior-proven
 * name distinguishes this unreferenced all-surfaces path from
 * R_AddWorldSurfacesDPVS: it disables world dynamic lights, submits every
 * world surface through R_AddWorldSurface, then installs every static model
 * linked from every world cell once per view. */
void R_AddWorldSurfaces(void)
{
    if (r_drawworld->integer == 0 ||
        (tr.refdef.rdflags & RDF_NOWORLDMODEL) != 0) {
        return;
    }

    tr.currentEntityNumber = R_WORLD_ENTITY_NUMBER;
    tr.shiftedEntityNumber =
        (uint32_t)R_WORLD_ENTITY_NUMBER << R_SORT_ENTITY_SHIFT;
    tr.refdef.num_dlights = 0;

    for (int32_t surfaceIndex = 0;
         surfaceIndex < tr.world->numsurfaces;
         ++surfaceIndex) {
        R_AddWorldSurface(&tr.world->surfaces[surfaceIndex], 0);
    }

    for (int32_t cellIndex = 0;
         cellIndex < tr.world->cellCount;
         ++cellIndex) {
        renderer_world_cell_t *cell = &tr.world->cells[cellIndex];
        for (renderer_cell_model_link_t *link = cell->modelLinks;
             link != NULL;
             link = link->next) {
            renderer_static_model_t *model = link->model;
            if (model->viewCount == tr.viewCount)
                continue;

            model->viewCount = tr.viewCount;
            RE_AddRefEntityToScene(&model->entity, model);
        }
    }
}

/* Source: CoDUOMP.exe 0x0051f5b0..0x0051f6ce.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051f5b0_0051f6cf.mcode.
 * Name and five-argument source order: same-module Mac symbol
 * R_AddEntityDrawSurf. The surface-type choices match the seven
 * RB_SurfaceXModel* symbols in the Mac renderer. */
void R_AddEntityDrawSurf(trRefEntity_t *entity, DObj *obj,
                         XSurface *surface, shader_t *shader,
                         int32_t modelIndex)
{
    renderer_entity_surface_t *entitySurface =
        &tr.refdef.entitySurfaces[tr.refdef.entitySurfaceCount];
    int32_t storageMode = tr.defaultStorageMode;

    if (XSurfaceGetBoneIndex(surface) == -1) {
        entitySurface->base.surfaceType = sysSseSupported != qfalse
            ? R_SURFACE_XMODEL_WEIGHT_SSE
            : R_SURFACE_XMODEL_WEIGHT;
    } else {
        entitySurface->base.surfaceType = sysSseSupported != qfalse
            ? R_SURFACE_XMODEL_RIGID_SSE
            : R_SURFACE_XMODEL_RIGID;

        if ((shader->surfaceFlags &
             SHADER_XMODEL_OPTIMIZATION_BLOCK_MASK) == 0) {
            if (surface->optimizedDataARB != NULL) {
                entitySurface->base.surfaceType =
                    R_SURFACE_XMODEL_RIGID_ARB;
            } else if (surface->optimizedDataNV != NULL) {
                entitySurface->base.surfaceType =
                    R_SURFACE_XMODEL_RIGID_NV;
                storageMode = surface->optimizedDataNV->memorySource;
            } else if (surface->optimizedDataATI != NULL) {
                entitySurface->base.surfaceType =
                    R_SURFACE_XMODEL_RIGID_ATI;
                storageMode = surface->optimizedDataATI->memorySource;
            }
        }
    }

    entitySurface->obj = obj;
    entitySurface->surface = surface;
    entitySurface->modelIndex = modelIndex;
    R_AddDrawSurf(&entitySurface->base, storageMode, shader, 0, 1, 0);
    ++tr.refdef.entitySurfaceCount;

    if (cg_skybox->integer == 2 &&
        (entity->e.renderfx & RF_DOBJ_MODEL) != 0) {
        R_AddDrawSurf(&entitySurface->base, tr.defaultStorageMode,
                      tr.stencilShadowShader, 0, 0, 0);
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    }
}

/* Source: CoDUOMP.exe 0x0051fd70..0x0051fe2f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051fd70_0051fe30.mcode.
 * Name and the boxes/axes command modes: same-module Mac symbol
 * R_XModelDebug and its two named callees. */
void R_XModelDebug(trRefEntity_t *entity, const uint32_t *partBits)
{
    qboolean drewBoxes = qfalse;

    if (Q_stricmp(r_xdebug->string, "boxes") == 0 ||
        Q_stricmp(r_xdebug->string, "both") == 0) {
        R_XModelDebugBoxes(entity, partBits);
        drewBoxes = qtrue;
    }

    if (Q_stricmp(r_xdebug->string, "axes") == 0 ||
        Q_stricmp(r_xdebug->string, "both") == 0) {
        R_XModelDebugAxes(entity, partBits);
        return;
    }

    if (drewBoxes == qfalse) {
        Cvar_Set("r_xdebug", "");
        Com_Printf("boxes - show bounding boxes\n");
        Com_Printf("axes - show axes\n");
        Com_Printf("both - show bounding boxes and axes\n");
    }
}

/* Source: CoDUOMP.exe 0x0051fe30..0x0052028b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051fe30_0052028c.mcode.
 * Name and source structure: same-module Mac R_AddXModelSurfaces. Windows
 * inlines R_CullModel, R_DObjGetSurfIndex, R_GetShaderByHandle, and the first
 * DObjBad test; their exact data dependencies remain explicit below. */
void R_AddXModelSurfaces(trRefEntity_t *entity)
{
    DObj *obj = entity->e.dobj;
    int32_t modelCount;
    int32_t *lodIndices;
    uint32_t partBits[DOBJ_PART_BITSET_WORD_COUNT];
    dobj_surface_ref_t *surfaceRefs;
    float lodDistance;
    int32_t surfaceCount;
    int32_t triangleCount = 0;
    int32_t vertexCount = 0;
    qboolean needsLighting = qfalse;
    qboolean thirdPersonOnly =
        (entity->e.renderfx & RF_THIRD_PERSON) != 0 &&
        tr.viewParms.isPortal == qfalse;

    if (R_CullModel(entity) == CULL_OUT) {
        return;
    }

    modelCount = DObjGetNumModels(obj);
    lodIndices = modelCount != 0
        ? CODUOMP_ALLOCA((size_t)modelCount * sizeof(*lodIndices))
        : NULL;
    lodDistance = R_GetLodDist(entity);
    for (int32_t modelIndex = 0;
         modelIndex < modelCount; ++modelIndex) {
        lodIndices[modelIndex] =
            DObjGetLodForDist(obj, modelIndex, lodDistance);
    }

    surfaceCount = DObjGetNumSurfaces(obj, lodIndices);
    /* Each accepted model surface owns one entity-surface record. Its
     * optional skybox shadow draw references that same record. */
    if (tr.refdef.entitySurfaceCount + surfaceCount >
        R_MAX_ENTITY_SURFACES) {
        if (com_developer->integer != 0)
            ri.Printf(R_PRINT_ALL, "WARNING: MAX_ENTSURFS exceeded\n");
        return;
    }

    surfaceRefs = surfaceCount != 0
        ? CODUOMP_ALLOCA((size_t)surfaceCount * sizeof(*surfaceRefs))
        : NULL;
    DObjGetSurfaces(obj, surfaceRefs, partBits, lodIndices);

    if (entity->e.owner != NULL) {
        DObjCompleteHierarchyBits(obj, partBits);
        ri.CG_DObjCalcPose(entity->e.owner, obj, partBits);
    }

    if (DObjBad(obj) != qfalse) {
        if (com_developer->integer == 0)
            return;
        (void)DObjBad(obj);
        R_XModelDebugBoxes(entity, partBits);
        R_XModelDebugAxes(entity, partBits);
        return;
    }

    for (int32_t surfaceRefIndex = 0;
         surfaceRefIndex < surfaceCount; ++surfaceRefIndex) {
        const dobj_surface_ref_t *surfaceRef =
            &surfaceRefs[surfaceRefIndex];
        int32_t surfaceModelIndex =
            R_DObjGetSurfIndex(obj, surfaceRef->modelIndex);
        model_t *rendererModel;
        int32_t shaderHandle;
        shader_t *shader;

        if (surfaceModelIndex == 0)
            continue;

        rendererModel = tr.models[surfaceModelIndex];
        shaderHandle = rendererModel
            ->shaderHandles[lodIndices[surfaceRef->modelIndex]]
                          [surfaceRef->surfaceIndex];
        if (shaderHandle < 0 || shaderHandle >= tr.numShaders) {
            ri.Printf(R_PRINT_WARNING,
                      "R_GetShaderByHandle: out of range hShader '%d'\n",
                      shaderHandle);
            shader = tr.defaultShader;
        } else {
            shader = tr.shaders[shaderHandle];
        }

        if ((shader->lightingFlags & SHADER_LIGHTING_ENTITY_MASK) != 0)
            needsLighting = qtrue;

        if (thirdPersonOnly != qfalse)
            continue;

        XSurface *surface = DObjGetSurface(
            obj, surfaceRef->modelIndex, surfaceRef->surfaceIndex,
            lodIndices);
        R_AddEntityDrawSurf(entity, obj, surface, shader,
                            surfaceRef->modelIndex);
        triangleCount += surface->triangleCount;
        vertexCount += surface->vertexCount;
    }

    if ((entity->e.renderfx & RF_DEPTH_RANGE_FLAGS) == 0) {
        if (r_showtricounts->integer != 0) {
            int32_t count = r_showtricounts->integer == 2
                ? vertexCount
                : triangleCount;
            R_AddScaledDebugString(entity->e.origin, colorWhite,
                                   va("%i", count));
        } else if (r_showsurfcounts->integer != 0) {
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            int32_t uniqueShaderCount = 1;

            for (int32_t currentIndex = 1;
                 currentIndex < surfaceCount; ++currentIndex) {
                const dobj_surface_ref_t *currentRef =
                    &surfaceRefs[currentIndex];
                int32_t currentModelIndex =
                    R_DObjGetSurfIndex(obj, currentRef->modelIndex);
                int32_t previousIndex;

                if (currentModelIndex == 0)
                    continue;

                uint16_t currentShader = tr.models[currentModelIndex]
                    ->shaderHandles[lodIndices[currentRef->modelIndex]]
                                  [currentRef->surfaceIndex];
                for (previousIndex = 0;
                     previousIndex < currentIndex; ++previousIndex) {
                    const dobj_surface_ref_t *previousRef =
                        &surfaceRefs[previousIndex];
                    int32_t previousModelIndex =
                        R_DObjGetSurfIndex(obj, previousRef->modelIndex);

                    if (previousModelIndex == 0)
                        continue;

                    uint16_t previousShader = tr.models[previousModelIndex]
                        ->shaderHandles[lodIndices[previousRef->modelIndex]]
                                      [previousRef->surfaceIndex];
                    if (currentShader == previousShader)
                        break;
                }
                if (previousIndex == currentIndex)
                    ++uniqueShaderCount;
            }

            R_AddScaledDebugString(
                entity->e.origin, colorWhite,
                va("%i/%i", uniqueShaderCount, surfaceCount));
        }
    }

    if (r_xdebug->string[0] != '\0')
        R_XModelDebug(entity, partBits);

    if ((thirdPersonOnly == qfalse || cg_skybox->integer > 1) &&
        needsLighting != qfalse) {
        R_SetupEntityLighting(&tr.refdef, entity);
    }
}

/* Source: CoDUOMP.exe 0x004f0080..0x004f00e4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f0080_004f00e5.mcode.
 * Name: same-module Mac symbol R_AddDrawSurfCmd. The command buffer ends with
 * eight reserved bytes in the original 256 KiB allocation, hence the proved
 * 262136-byte usable limit. */
void R_AddDrawSurfCmd(drawSurf_t *drawSurfs, int32_t drawSurfCount)
{
    drawSurfsCommand_t *command;
    const uint32_t newCommandUsed =
        (uint32_t)rendererBackendData->commandUsed +
        (uint32_t)sizeof(*command);

    if (newCommandUsed > R_COMMAND_BUFFER_USABLE_BYTES)
        return;

    command = (drawSurfsCommand_t *)&rendererBackendData->commandBuffer[
        rendererBackendData->commandUsed];
    rendererBackendData->commandUsed = (int32_t)newCommandUsed;

    command->drawSurfs = drawSurfs;
    command->commandId = RC_DRAW_SURFS;
    command->numDrawSurfs = drawSurfCount;
    command->refdef = tr.refdef;
    command->viewParms = tr.viewParms;
}

/* Source: CoDUOMP.exe 0x004e5890..0x004e5985.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e5890_004e5986.mcode.
 * Name: same-module Mac symbol R_SortDrawSurfs. Only shaders through the
 * portal sort participate in the portal-view scan; later sorts terminate it. */
void R_SortDrawSurfs(drawSurf_t *drawSurfs, int32_t drawSurfCount)
{
    if (drawSurfCount < 1) {
        R_AddDrawSurfCmd(drawSurfs, drawSurfCount);
        return;
    }

    if (drawSurfCount > R_MAX_DRAW_SURFS)
        drawSurfCount = R_MAX_DRAW_SURFS;

    qsortFast(drawSurfs, (size_t)drawSurfCount);

    for (int32_t drawSurfIndex = 0;
         drawSurfIndex < drawSurfCount;
         ++drawSurfIndex) {
        const drawSurf_t *drawSurf = &drawSurfs[drawSurfIndex];
        const uint32_t sort = drawSurf->sort;
        shader_t *shader = tr.sortedShaders[
            (sort >> R_SORT_SHADER_SHIFT) & R_SORT_SHADER_MASK];
        int32_t entityNumber =
            (int32_t)((sort >> R_SORT_ENTITY_SHIFT) & R_SORT_ENTITY_MASK);

        if ((sort & R_SORT_WORLD_ENTITY) != 0)
            entityNumber = R_WORLD_ENTITY_NUMBER;

        if (shader->sort > SHADER_SORT_PORTAL)
            break;

        if (shader->sort == SHADER_SORT_BAD) {
            ri.Error(ERR_DROP,
                     "\x15" "Shader '%s'with sort == SS_BAD",
                     shader->name);
        }

        if (shader->sort == SHADER_SORT_PORTAL &&
            R_MirrorViewBySurface(drawSurf, entityNumber) != qfalse &&
            r_portalOnly->integer != 0) {
            return;
        }
    }

    R_AddDrawSurfCmd(drawSurfs, drawSurfCount);
}

/* Source: CoDUOMP.exe 0x004e5990..0x004e5aad.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e5990_004e5aae.mcode, including the
 * exact jump and byte tables at 0x004e5ab0/0x004e5ac8.
 * Name: the PE fatal string and same-module Mac symbol R_AddEntitySurfaces.
 * Model types 0..2 select their dedicated surface producers, type 11 is the
 * portal marker, and the remaining accepted types use the fixed entity
 * surface that RB_SurfaceEntity expands according to the current entity. */
void R_AddEntitySurfaces(void)
{
    if (r_drawentities->integer == 0)
        return;

    tr.currentEntityNumber = 0;
    while (tr.currentEntityNumber < tr.refdef.num_entities) {
        trRefEntity_t *entity =
            &tr.refdef.entities[tr.currentEntityNumber];
        const refEntityType_t entityType = entity->e.reType;

        tr.currentEntity = entity;
        entity->dlightBits = 0;
        tr.shiftedEntityNumber =
            (uint32_t)tr.currentEntityNumber << R_SORT_ENTITY_SHIFT;

        if ((entity->e.renderfx & RF_FIRST_PERSON) != 0 &&
            tr.viewParms.isPortal != qfalse) {
            ++tr.currentEntityNumber;
            continue;
        }

        switch (entityType) {
        case RT_BRUSH_MODEL:
            R_AddBrushModelSurfaces(entity);
            break;

        case RT_MODEL:
            R_AddXModelSurfaces(entity);
            break;

        case RT_STATIC_MODEL:
            R_AddStaticModelSurfaces(entity);
            break;

        case RT_PORTALSURFACE:
            break;

        case RT_INVALID:
            ri.Error(ERR_DROP,
                     "\x15" "R_AddEntitySurfaces: Bad reType");
            break;

        default:
            if (entityType < RT_SPRITE ||
                entityType > RT_CYLINDER) {
                ri.Error(ERR_DROP,
                         "\x15" "R_AddEntitySurfaces: Bad reType");
                break;
            }

            if ((entity->e.renderfx & RF_THIRD_PERSON) != 0 &&
                tr.viewParms.isPortal == qfalse) {
                break;
            }

            {
                const int32_t shaderHandle =
                    entity->e.spriteShaderHandle;
                shader_t *shader;

                if (shaderHandle < 0 || shaderHandle >= tr.numShaders) {
                    ri.Printf(
                        R_PRINT_WARNING,
                        "R_GetShaderByHandle: out of range hShader '%d'\n",
                        shaderHandle);
                    shader = tr.defaultShader;
                } else {
                    shader = tr.shaders[shaderHandle];
                }

                R_AddDrawSurf(&rendererEntitySurface,
                              tr.defaultStorageMode, shader,
                              0, 0, 0);
            }
            break;
        }

        ++tr.currentEntityNumber;
    }
}

/* Source: CoDUOMP.exe 0x0051cde0..0x0051ce58.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051cde0_0051ce59.mcode.
 * Name and model lookup: same-module Mac symbols R_AddBrushModelSurfaces and
 * R_GetModelByHandle. The latter is a direct model-registry array lookup and
 * is inlined by the Windows compiler. */
void R_AddBrushModelSurfaces(trRefEntity_t *entity)
{
    bmodel_t *bmodel;

    if (entity->cullState == CULL_OUT ||
        r_drawBModels->integer == 0) {
        return;
    }

    R_RotateForModelEntity(entity, &tr.viewParms, &tr.orientation);
    bmodel = tr.models[entity->e.hModel]->bmodel;
    R_DlightBmodel(bmodel);

    for (int32_t surfaceIndex = 0;
         surfaceIndex < bmodel->numSurfaces;
         ++surfaceIndex) {
        R_AddWorldSurfaceNoCull(&bmodel->firstSurface[surfaceIndex],
                                tr.currentEntity->dlightBits);
    }
}

/* Source: CoDUOMP.exe 0x004e5cf0..0x004e5dad.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e5cf0_004e5dae.mcode.
 * Name: same-module Mac symbol R_AddPolygonSurfaces. The polygon index is the
 * synthetic entity number, masked to the ten sort-word entity bits, while bit
 * 1 selects the renderer's world entity during back-end expansion. */
void R_AddPolygonSurfaces(void)
{
    for (int32_t polyIndex = 0;
         polyIndex < tr.refdef.numPolys;
         ++polyIndex) {
        srfPoly_t *poly = &tr.refdef.polys[polyIndex];
        shader_t *shader;

        tr.currentEntityNumber = polyIndex & R_SORT_ENTITY_MASK;
        tr.shiftedEntityNumber =
            (uint32_t)tr.currentEntityNumber << R_SORT_ENTITY_SHIFT;

        if (poly->hShader < 0 ||
            poly->hShader >= tr.numShaders) {
            ri.Printf(R_PRINT_WARNING,
                      "R_GetShaderByHandle: out of range hShader '%d'\n",
                      poly->hShader);
            shader = tr.defaultShader;
        } else {
            shader = tr.shaders[poly->hShader];
        }

        R_AddDrawSurf((renderer_surface_t *)poly,
                      tr.defaultStorageMode, shader,
                      0, 0, 1);
    }
}

/* Source: CoDUOMP.exe 0x004e5ae0..0x004e5b21.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e5ae0_004e5b22.mcode.
 * Name: same-module Mac symbol R_GenerateDrawSurfs. The world pointer guard,
 * front-end producer order, and optional light-visibility debug overlay all
 * follow the PE call sequence. */
void R_GenerateDrawSurfs(void)
{
    R_SetupProjection();

    if (tr.world != NULL && tr.world->cells != NULL)
        R_AddWorldSurfacesDPVS();

    R_AddPolygonSurfaces();
    R_AddEntitySurfaces();

    if (r_vc_showlog->integer != 0)
        R_ShowLightVisCachePoints();
}

/* Source: CoDUOMP.exe 0x004e5b30..0x004e5c68.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e5b30_004e5c69.mcode.
 * Name: same-module Mac symbol R_RenderView. The four LOD assignments are the
 * PE-inlined bodies of XModelSetTestLods/XModelSetTestLodDist: using those
 * recovered source functions preserves their exact negative-distance clamps
 * without exposing their private state here. */
void R_RenderView(const viewParms_t *viewParms)
{
    int32_t firstDrawSurf;
    int32_t drawSurfCount;

    if (viewParms->viewportWidth <= 0 || viewParms->viewportHeight <= 0)
        return;

    ++tr.viewCount;
    tr.viewParms = *viewParms;

    tr.viewParms.frameSceneNum = tr.frameSceneNum;
    firstDrawSurf = tr.refdef.numDrawSurfs;
    ++tr.viewCount;
    tr.viewParms.frameCount = tr.frameCount;

    XModelSetTestLods(0, r_highLodDist->value);
    XModelSetTestLods(1, r_mediumLodDist->value);
    XModelSetTestLods(2, r_lowLodDist->value);
    XModelSetTestLodDist(r_lodViewDist->value);

    R_RotateForViewer();
    R_SetupFrustum();
    R_GenerateDrawSurfs();

    drawSurfCount = (int32_t)(
        (uint32_t)tr.refdef.numDrawSurfs - (uint32_t)firstDrawSurf);
    R_SortDrawSurfs(&tr.refdef.drawSurfs[firstDrawSurf],
                    drawSurfCount);
}
