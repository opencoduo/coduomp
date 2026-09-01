#include "collision_trace_entry.h"

#include "collision_brush_traces.h"
#include "collision_capsule_traces.h"
#include "collision_leaf_traces.h"
#include "collision_patch_dispatch.h"
#include "collision_patch_trace.h"
#include "collision_queries.h"
#include "collision_terrain_dispatch.h"
#include "collision_trace_bounds.h"
#include "collision_tree_traces.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern collisionBrush_t *cm_boxBrush;
extern collisionBrush_t *cm_brushes;
extern collisionTerrainPatch_t *cm_terrainPatches;
extern int32_t cm_checkcount;
extern int32_t cm_numBrushes;
extern int32_t cm_numTerrainPatches;

void *Com_Memset(void *destination, int value, size_t count);
float CM_TraceFloatAbs(float value);
/*
 * Complete collision trace entrypoint pair:
 *
 *   CoDUOMP.exe  CM_Trace      0x00427ed0..0x00428569
 *                CM_SightTrace 0x00429b20..0x0042a175
 *   coduo_lnxded CM_Trace      0x0805a9d5..0x0805b144
 *                CM_SightTrace 0x0805cd62..0x0805d458
 *
 * The same dispatch and result contracts are preserved. Platform bodies keep
 * their proven x87 expression graphs, spills, and cached-hit realization.
 */
#if defined(WINDOWS_BEHAVIOR)
/* Source: CoDUOMP.exe 0x00427ed0..0x00428569.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00427ed0_0042856a.mcode.
 * Name: exact same-module Mac symbol CM_Trace. Build the centered moving
 * trace record, derive its corner offsets and swept bounds, dispatch either
 * the stationary or moving collision path, and copy the final trace out. */
void CM_Trace(trace_t *trace,
              const vec3_t start, const vec3_t end,
              const vec3_t mins, const vec3_t maxs,
              int32_t modelHandle, int32_t contentMask,
              qboolean capsule,
              const cmTraceSphereRecord_t *sphere)
{
    const collisionModel_t *const collisionModel =
        CM_ClipHandleToModel(modelHandle);
    traceWork_t traceWork;
    long double deltaZRaw = 0.0L;

    cm_checkcount++;
    Com_Memset(&traceWork, 0, sizeof(traceWork));
    traceWork.trace.fraction = trace->fraction;

    const float *const traceMins =
        mins != NULL ? mins : vec3_origin;
    const float *const traceMaxs =
        maxs != NULL ? maxs : vec3_origin;

    for (int32_t axis = 0; axis < 3; ++axis) {
        /* 0x427f3f..0x427fed: the centered offset and end remain in
         * x87 precision while their float copies are written.  The delta
         * subtracts the rounded centered start from the retained end. */
        const long double offsetRaw =
            ((long double)traceMins[axis] +
             (long double)traceMaxs[axis]) *
            0.5L;
        traceWork.mins[axis] = (float)(
            (long double)traceMins[axis] - offsetRaw);
        traceWork.maxs[axis] = (float)(
            (long double)traceMaxs[axis] - offsetRaw);
        traceWork.start[axis] = (float)(
            (long double)start[axis] + offsetRaw);

        const long double endRaw =
            (long double)end[axis] + offsetRaw;
        traceWork.end[axis] = (float)endRaw;
        const long double deltaRaw =
            endRaw - (long double)traceWork.start[axis];
        traceWork.delta[axis] = (float)deltaRaw;
        if (axis == 2)
            deltaZRaw = deltaRaw;
    }
    /* 0x427fed..0x428009: only the Z delta remains live after its float
     * store; the Y and X terms are reloaded from their rounded copies. */
    traceWork.deltaLengthSquared = (float)(
        deltaZRaw * (long double)traceWork.delta[2] +
        (long double)traceWork.delta[1] *
            (long double)traceWork.delta[1] +
        (long double)traceWork.delta[0] *
            (long double)traceWork.delta[0]);

    if (sphere != NULL) {
        /* The original copies this complete 36-byte record as nine dwords.
         * A raw copy also permits transformed traces to leave the extents
         * unspecified: moving sphere traces replace them before use. */
        memcpy(&traceWork.sphere, &sphere->sphere,
               sizeof(traceWork.sphere));
        memcpy(traceWork.sphereExtents,
               sphere->extents,
               sizeof(traceWork.sphereExtents));
    } else {
        traceWork.sphere.use = capsule;
        traceWork.sphere.radius =
            traceWork.maxs[2] <
                    traceWork.maxs[0]
                ? traceWork.maxs[2]
                : traceWork.maxs[0];
        traceWork.sphere.halfheight =
            traceWork.maxs[2];
        traceWork.sphere.offset[0] = 0.0f;
        traceWork.sphere.offset[1] = 0.0f;
        traceWork.sphere.offset[2] =
            traceWork.maxs[2] -
            traceWork.sphere.radius;
    }

    /* 0x428087..0x4280af leaves this raw sum on the x87 stack until the
     * moving-trace point test at 0x4283fd..0x428417. */
    const long double maxsSumRaw =
        ((long double)traceWork.maxs[1] +
         (long double)traceWork.maxs[2]) +
        (long double)traceWork.maxs[0];
    traceWork.maxsSum = (float)maxsSumRaw;
    traceWork.contents = contentMask;

    traceWork.offsets[0][0] =
        traceWork.mins[0];
    traceWork.offsets[0][1] =
        traceWork.mins[1];
    traceWork.offsets[0][2] =
        traceWork.mins[2];
    traceWork.offsets[1][0] =
        traceWork.maxs[0];
    traceWork.offsets[1][1] =
        traceWork.mins[1];
    traceWork.offsets[1][2] =
        traceWork.mins[2];
    traceWork.offsets[2][0] =
        traceWork.mins[0];
    traceWork.offsets[2][1] =
        traceWork.maxs[1];
    traceWork.offsets[2][2] =
        traceWork.mins[2];
    traceWork.offsets[3][0] =
        traceWork.maxs[0];
    traceWork.offsets[3][1] =
        traceWork.maxs[1];
    traceWork.offsets[3][2] =
        traceWork.mins[2];
    traceWork.offsets[4][0] =
        traceWork.mins[0];
    traceWork.offsets[4][1] =
        traceWork.mins[1];
    traceWork.offsets[4][2] =
        traceWork.maxs[2];
    traceWork.offsets[5][0] =
        traceWork.maxs[0];
    traceWork.offsets[5][1] =
        traceWork.mins[1];
    traceWork.offsets[5][2] =
        traceWork.maxs[2];
    traceWork.offsets[6][0] =
        traceWork.mins[0];
    traceWork.offsets[6][1] =
        traceWork.maxs[1];
    traceWork.offsets[6][2] =
        traceWork.maxs[2];
    traceWork.offsets[7][0] =
        traceWork.maxs[0];
    traceWork.offsets[7][1] =
        traceWork.maxs[1];
    traceWork.offsets[7][2] =
        traceWork.maxs[2];

    if (traceWork.sphere.use != qfalse) {
        for (int32_t axis = 0; axis < 3; ++axis) {
            const float sphereOffset =
                fabsf(traceWork.sphere.offset[axis]);
            if (traceWork.start[axis] <
                traceWork.end[axis]) {
                traceWork.bounds[0][axis] =
                    traceWork.start[axis] -
                    sphereOffset -
                    traceWork.sphere.radius;
                traceWork.bounds[1][axis] =
                    traceWork.end[axis] +
                    sphereOffset +
                    traceWork.sphere.radius;
            } else {
                traceWork.bounds[0][axis] =
                    traceWork.end[axis] -
                    sphereOffset -
                    traceWork.sphere.radius;
                traceWork.bounds[1][axis] =
                    traceWork.start[axis] +
                    sphereOffset +
                    traceWork.sphere.radius;
            }
        }
    } else {
        for (int32_t axis = 0; axis < 3; ++axis) {
            if (traceWork.start[axis] <
                traceWork.end[axis]) {
                traceWork.bounds[0][axis] =
                    traceWork.start[axis] +
                    traceWork.mins[axis];
                traceWork.bounds[1][axis] =
                    traceWork.end[axis] +
                    traceWork.maxs[axis];
            } else {
                traceWork.bounds[0][axis] =
                    traceWork.end[axis] +
                    traceWork.mins[axis];
                traceWork.bounds[1][axis] =
                    traceWork.start[axis] +
                    traceWork.maxs[axis];
            }
        }
    }

    if (start[0] == end[0] &&
        start[1] == end[1] &&
        start[2] == end[2]) {
        if (modelHandle == CM_WORLD_MODEL) {
            CM_PositionTest(&traceWork);
        } else if (
            modelHandle ==
            CM_TEMP_CAPSULE_MODEL_HANDLE) {
            if ((cm_boxBrush->contents &
                 contentMask) != 0) {
                if (traceWork.sphere.use !=
                    qfalse) {
                    CM_TestCapsuleInCapsule(
                        &traceWork);
                } else {
                    CM_TestBoundingBoxInCapsule(
                        &traceWork);
                }
            }
        } else {
            CM_TestInLeaf(
                &traceWork,
                &collisionModel->leaf);
        }
    } else {
        traceWork.isPoint = maxsSumRaw == 0.0L;

        if (traceWork.sphere.use != qfalse) {
            for (int32_t axis = 0;
                 axis < 3; ++axis) {
                traceWork.sphereExtents[axis] =
                    traceWork.sphere.radius +
                    fabsf(
                        traceWork.sphere
                            .offset[axis]);
            }
        }

        if (modelHandle == CM_WORLD_MODEL) {
            CM_TraceThroughTree(
                &traceWork, 0, 0.0f,
                traceWork.trace.fraction,
                traceWork.start, traceWork.end);
        } else if (
            modelHandle ==
            CM_TEMP_CAPSULE_MODEL_HANDLE) {
            if ((cm_boxBrush->contents &
                 traceWork.contents) != 0) {
                if (traceWork.sphere.use !=
                    qfalse) {
                    CM_TraceCapsuleThroughCapsule(
                        &traceWork);
                } else {
                    CM_TraceBoundingBoxThroughCapsule(
                        &traceWork);
                }
            }
        } else {
            CM_TraceThroughLeaf(
                &traceWork,
                &collisionModel->leaf);
        }
    }

    for (int32_t axis = 0; axis < 3; ++axis) {
        traceWork.trace.endpos[axis] =
            start[axis] +
            traceWork.delta[axis] *
                traceWork.trace.fraction;
    }
    *trace = traceWork.trace;
}

/* Source: CoDUOMP.exe 0x00429b20..0x0042a175.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00429b20_0042a176.mcode.
 * Name: exact same-module Mac symbol CM_SightTrace. Build the centered
 * sight-work record, optionally recheck a prior encoded hit, and dispatch
 * world, inline-model, or temporary-capsule collision. */
int32_t CM_SightTrace(
    int32_t oldHitNum,
    const vec3_t start, const vec3_t end,
    const vec3_t mins, const vec3_t maxs,
    int32_t modelHandle, const vec3_t origin,
    int32_t contentMask, qboolean capsule,
    const cmTraceSphereRecord_t *sphere)
{
    (void)origin;

    const collisionModel_t *const
        collisionModel =
            CM_ClipHandleToModel(modelHandle);
    traceWork_t traceWork;
    long double deltaZRaw = 0.0L;

    cm_checkcount++;
    Com_Memset(
        &traceWork, 0, sizeof(traceWork));
    traceWork.trace.fraction = 1.0f;

    const float *const traceMins =
        mins != NULL ? mins : vec3_origin;
    const float *const traceMaxs =
        maxs != NULL ? maxs : vec3_origin;
    traceWork.contents = contentMask;

    for (int32_t axis = 0; axis < 3;
         ++axis) {
        /* 0x429b81..0x429c38 mirrors CM_Trace: the centering offset and
         * centered end remain wide while their float copies are stored. */
        const long double offsetRaw =
            ((long double)traceMins[axis] +
             (long double)traceMaxs[axis]) *
            0.5L;
        traceWork.mins[axis] = (float)(
            (long double)traceMins[axis] - offsetRaw);
        traceWork.maxs[axis] = (float)(
            (long double)traceMaxs[axis] - offsetRaw);
        traceWork.start[axis] = (float)(
            (long double)start[axis] + offsetRaw);
        const long double endRaw =
            (long double)end[axis] + offsetRaw;
        traceWork.end[axis] = (float)endRaw;
        const long double deltaRaw =
            endRaw - (long double)traceWork.start[axis];
        traceWork.delta[axis] = (float)deltaRaw;
        if (axis == 2)
            deltaZRaw = deltaRaw;
    }
    traceWork.deltaLengthSquared = (float)(
        deltaZRaw * (long double)traceWork.delta[2] +
        (long double)traceWork.delta[1] *
            (long double)traceWork.delta[1] +
        (long double)traceWork.delta[0] *
            (long double)traceWork.delta[0]);

    if (sphere != NULL) {
        memcpy(
            &traceWork.sphere,
            &sphere->sphere,
            sizeof(traceWork.sphere));
        memcpy(
            traceWork.sphereExtents,
            sphere->extents,
            sizeof(traceWork.sphereExtents));
    } else {
        traceWork.sphere.use = capsule;
        traceWork.sphere.radius =
            traceWork.maxs[2] <
                    traceWork.maxs[0]
                ? traceWork.maxs[2]
                : traceWork.maxs[0];
        traceWork.sphere.halfheight =
            traceWork.maxs[2];
        traceWork.sphere.offset[0] = 0.0f;
        traceWork.sphere.offset[1] = 0.0f;
        traceWork.sphere.offset[2] =
            traceWork.maxs[2] -
            traceWork.sphere.radius;
    }

    const long double maxsSumRaw =
        ((long double)traceWork.maxs[1] +
         (long double)traceWork.maxs[2]) +
        (long double)traceWork.maxs[0];
    traceWork.maxsSum = (float)maxsSumRaw;

    traceWork.offsets[0][0] =
        traceWork.mins[0];
    traceWork.offsets[0][1] =
        traceWork.mins[1];
    traceWork.offsets[0][2] =
        traceWork.mins[2];
    traceWork.offsets[1][0] =
        traceWork.maxs[0];
    traceWork.offsets[1][1] =
        traceWork.mins[1];
    traceWork.offsets[1][2] =
        traceWork.mins[2];
    traceWork.offsets[2][0] =
        traceWork.mins[0];
    traceWork.offsets[2][1] =
        traceWork.maxs[1];
    traceWork.offsets[2][2] =
        traceWork.mins[2];
    traceWork.offsets[3][0] =
        traceWork.maxs[0];
    traceWork.offsets[3][1] =
        traceWork.maxs[1];
    traceWork.offsets[3][2] =
        traceWork.mins[2];
    traceWork.offsets[4][0] =
        traceWork.mins[0];
    traceWork.offsets[4][1] =
        traceWork.mins[1];
    traceWork.offsets[4][2] =
        traceWork.maxs[2];
    traceWork.offsets[5][0] =
        traceWork.maxs[0];
    traceWork.offsets[5][1] =
        traceWork.mins[1];
    traceWork.offsets[5][2] =
        traceWork.maxs[2];
    traceWork.offsets[6][0] =
        traceWork.mins[0];
    traceWork.offsets[6][1] =
        traceWork.maxs[1];
    traceWork.offsets[6][2] =
        traceWork.maxs[2];
    traceWork.offsets[7][0] =
        traceWork.maxs[0];
    traceWork.offsets[7][1] =
        traceWork.maxs[1];
    traceWork.offsets[7][2] =
        traceWork.maxs[2];

    if (traceWork.sphere.use != qfalse) {
        for (int32_t axis = 0; axis < 3;
             ++axis) {
            const float sphereOffset =
                fabsf(
                    traceWork.sphere
                        .offset[axis]);
            if (traceWork.start[axis] <
                traceWork.end[axis]) {
                traceWork.bounds[0][axis] =
                    traceWork.start[axis] -
                    sphereOffset -
                    traceWork.sphere.radius;
                traceWork.bounds[1][axis] =
                    traceWork.end[axis] +
                    sphereOffset +
                    traceWork.sphere.radius;
            } else {
                traceWork.bounds[0][axis] =
                    traceWork.end[axis] -
                    sphereOffset -
                    traceWork.sphere.radius;
                traceWork.bounds[1][axis] =
                    traceWork.start[axis] +
                    sphereOffset +
                    traceWork.sphere.radius;
            }
        }
    } else {
        for (int32_t axis = 0; axis < 3;
             ++axis) {
            if (traceWork.start[axis] <
                traceWork.end[axis]) {
                traceWork.bounds[0][axis] =
                    traceWork.start[axis] +
                    traceWork.mins[axis];
                traceWork.bounds[1][axis] =
                    traceWork.end[axis] +
                    traceWork.maxs[axis];
            } else {
                traceWork.bounds[0][axis] =
                    traceWork.end[axis] +
                    traceWork.mins[axis];
                traceWork.bounds[1][axis] =
                    traceWork.start[axis] +
                    traceWork.maxs[axis];
            }
        }
    }

    traceWork.isPoint = maxsSumRaw == 0.0L;
    if (traceWork.sphere.use != qfalse) {
        for (int32_t axis = 0; axis < 3;
             ++axis) {
            traceWork.sphereExtents[axis] =
                traceWork.sphere.radius +
                fabsf(
                    traceWork.sphere
                        .offset[axis]);
        }
    }

    if (modelHandle != CM_WORLD_MODEL) {
        if (modelHandle ==
            CM_TEMP_CAPSULE_MODEL_HANDLE) {
            if ((cm_boxBrush->contents &
                 traceWork.contents) == 0) {
                return 0;
            }
            if (traceWork.sphere.use !=
                qfalse) {
                return
                    CM_SightTraceCapsuleThroughCapsule(
                        &traceWork);
            }
            return
                CM_SightTraceBoundingBoxThroughCapsule(
                    &traceWork);
        }

        return CM_SightTraceThroughLeaf(
            &traceWork,
            &collisionModel->leaf);
    }

    int32_t sightHit = 0;
    if (oldHitNum > 0) {
        const int32_t hitIndex =
            oldHitNum - 1;
        if (hitIndex < cm_numBrushes) {
            sightHit =
                CM_SightTraceThroughBrush(
                    &traceWork,
                    &cm_brushes[hitIndex]);
        } else {
            const int32_t terrainPatchIndex =
                hitIndex - cm_numBrushes;
            if (terrainPatchIndex <
                cm_numTerrainPatches) {
                collisionTerrainPatch_t *const
                    terrainPatch =
                        &cm_terrainPatches[
                            terrainPatchIndex];
                /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                if (CM_TraceWorkIntersectsBounds(
                        &traceWork,
                        terrainPatch->bounds[0],
                        terrainPatch->bounds[1]) ==
                    qfalse) {
                    return qtrue;
                }

                if (terrainPatch->curveCollide !=
                    NULL) {
                    sightHit =
                        CM_SightTraceThroughPatchCollide(
                            &traceWork,
                            terrainPatch
                                ->curveCollide);
                } else {
                    sightHit =
                        CM_SightTraceThroughTerrainCollide(
                            &traceWork,
                            terrainPatch
                                ->terrainCollide);
                }
            }
        }
    }

    if (sightHit != 0)
        return sightHit;

    return CM_SightTraceThroughTree(
        &traceWork, 0, 0.0f, 1.0f,
        traceWork.start, traceWork.end);
}
#else



static const vec3_t cm_trace_zero_vec = {0.0f, 0.0f, 0.0f};

void CM_Trace(trace_t *trace,
              const vec3_t start,
              const vec3_t end,
              const vec3_t mins,
              const vec3_t maxs,
              int32_t model,
              int32_t brushMask,
              qboolean capsule,
              const cmTraceSphereRecord_t *sphere)
{
    const collisionModel_t *collisionModel =
        CM_ClipHandleToModel(model);
    traceWork_t traceWork;
    vec3_t offset;

    cm_checkcount++;
    Com_Memset(&traceWork, 0, sizeof(traceWork));
    traceWork.trace.fraction = trace->fraction;

    const float *traceMins = mins;
    if (traceMins == NULL) {
        traceMins = cm_trace_zero_vec;
    }

    const float *traceMaxs = maxs;
    if (traceMaxs == NULL) {
        traceMaxs = cm_trace_zero_vec;
    }

#if EMULATE_X87
    /* x87 (DLL 0x0805a9d5): each setup value is one 80-bit chain rounded to
     * its float slot (offset = (mins+maxs)*0.5f via fld;fadd;fld 0.5f;fmulp;
     * fstp, then the mins/maxs/start/end/delta fld;fsub|fadd;fstp pairs), and
     * deltaLengthSquared is an 80-bit 3-term dot rounded once. */
    for (int32_t axis = 0; axis < 3; ++axis) {
        offset[axis] = x87f_store_f32(
            x87f_mul(x87f_add(x87f_load_f32(traceMins[axis]),
                              x87f_load_f32(traceMaxs[axis])),
                     x87f_load_f32(0.5f)));
        traceWork.mins[axis] = x87f_store_f32(
            x87f_sub(x87f_load_f32(traceMins[axis]),
                     x87f_load_f32(offset[axis])));
        traceWork.maxs[axis] = x87f_store_f32(
            x87f_sub(x87f_load_f32(traceMaxs[axis]),
                     x87f_load_f32(offset[axis])));
        traceWork.start[axis] = x87f_store_f32(
            x87f_add(x87f_load_f32(start[axis]),
                     x87f_load_f32(offset[axis])));
        traceWork.end[axis] = x87f_store_f32(
            x87f_add(x87f_load_f32(end[axis]), x87f_load_f32(offset[axis])));
        traceWork.delta[axis] = x87f_store_f32(
            x87f_sub(x87f_load_f32(traceWork.end[axis]),
                     x87f_load_f32(traceWork.start[axis])));
    }

    traceWork.deltaLengthSquared = x87f_store_f32(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(traceWork.delta[0]),
                          x87f_load_f32(traceWork.delta[0])),
                 x87f_mul(x87f_load_f32(traceWork.delta[1]),
                          x87f_load_f32(traceWork.delta[1]))),
        x87f_mul(x87f_load_f32(traceWork.delta[2]),
                 x87f_load_f32(traceWork.delta[2]))));
#else
    for (int32_t axis = 0; axis < 3; ++axis) {
        offset[axis] = (traceMins[axis] + traceMaxs[axis]) * 0.5f;
        traceWork.mins[axis] = traceMins[axis] - offset[axis];
        traceWork.maxs[axis] = traceMaxs[axis] - offset[axis];
        traceWork.start[axis] = start[axis] + offset[axis];
        traceWork.end[axis] = end[axis] + offset[axis];
        traceWork.delta[axis] =
            traceWork.end[axis] - traceWork.start[axis];
    }

    traceWork.deltaLengthSquared =
        (traceWork.delta[0] * traceWork.delta[0]) +
        (traceWork.delta[1] * traceWork.delta[1]) +
        (traceWork.delta[2] * traceWork.delta[2]);
#endif

    if (sphere != NULL) {
        traceWork.sphere = sphere->sphere;
        traceWork.sphereExtents[0] = sphere->extents[0];
        traceWork.sphereExtents[1] = sphere->extents[1];
        traceWork.sphereExtents[2] = sphere->extents[2];
    } else {
        traceWork.sphere.use = capsule;
        if (traceWork.maxs[2] < traceWork.maxs[0]) {
            traceWork.sphere.radius = traceWork.maxs[2];
        } else {
            traceWork.sphere.radius = traceWork.maxs[0];
        }
        traceWork.sphere.halfheight = traceWork.maxs[2];
        traceWork.sphere.offset[0] = 0.0f;
        traceWork.sphere.offset[1] = 0.0f;
#if EMULATE_X87
        traceWork.sphere.offset[2] =
            x87f_store_f32(x87f_sub(x87f_load_f32(traceWork.maxs[2]),
                                    x87f_load_f32(traceWork.sphere.radius)));
#else
        traceWork.sphere.offset[2] =
            traceWork.maxs[2] - traceWork.sphere.radius;
#endif
    }

    /* 0x805ac11..0x805ac23: stock sums maxs into the (never-read) work
     * field at +0xa0 with one float rounding. */
#if EMULATE_X87
    traceWork.maxsSum = x87f_store_f32(
        x87f_add(x87f_add(x87f_load_f32(traceWork.maxs[0]),
                          x87f_load_f32(traceWork.maxs[1])),
                 x87f_load_f32(traceWork.maxs[2])));
#else
    traceWork.maxsSum =
        traceWork.maxs[0] + traceWork.maxs[1] + traceWork.maxs[2];
#endif

    traceWork.contents = brushMask;

    traceWork.offsets[0][0] = traceWork.mins[0];
    traceWork.offsets[0][1] = traceWork.mins[1];
    traceWork.offsets[0][2] = traceWork.mins[2];
    traceWork.offsets[1][0] = traceWork.maxs[0];
    traceWork.offsets[1][1] = traceWork.mins[1];
    traceWork.offsets[1][2] = traceWork.mins[2];
    traceWork.offsets[2][0] = traceWork.mins[0];
    traceWork.offsets[2][1] = traceWork.maxs[1];
    traceWork.offsets[2][2] = traceWork.mins[2];
    traceWork.offsets[3][0] = traceWork.maxs[0];
    traceWork.offsets[3][1] = traceWork.maxs[1];
    traceWork.offsets[3][2] = traceWork.mins[2];
    traceWork.offsets[4][0] = traceWork.mins[0];
    traceWork.offsets[4][1] = traceWork.mins[1];
    traceWork.offsets[4][2] = traceWork.maxs[2];
    traceWork.offsets[5][0] = traceWork.maxs[0];
    traceWork.offsets[5][1] = traceWork.mins[1];
    traceWork.offsets[5][2] = traceWork.maxs[2];
    traceWork.offsets[6][0] = traceWork.mins[0];
    traceWork.offsets[6][1] = traceWork.maxs[1];
    traceWork.offsets[6][2] = traceWork.maxs[2];
    traceWork.offsets[7][0] = traceWork.maxs[0];
    traceWork.offsets[7][1] = traceWork.maxs[1];
    traceWork.offsets[7][2] = traceWork.maxs[2];

    /*
     * These bounds drive the leaf/patch culling, so an ULP here changes which
     * brushes and patches get traced at all — not just the resulting fraction.
     * Each is one 80-bit chain rounded once to its float slot.
     */
    if (traceWork.sphere.use != 0) {
        for (int32_t axis = 0; axis < 3; ++axis) {
#if EMULATE_X87
            const float sphereOffset =
                x87f_store_f32(x87f_abs(
                    x87f_load_f32(traceWork.sphere.offset[axis])));

            if (traceWork.start[axis] < traceWork.end[axis]) {
                traceWork.bounds[0][axis] = x87f_store_f32(x87f_sub(
                    x87f_sub(x87f_load_f32(traceWork.start[axis]),
                             x87f_load_f32(sphereOffset)),
                    x87f_load_f32(traceWork.sphere.radius)));
                traceWork.bounds[1][axis] = x87f_store_f32(x87f_add(
                    x87f_add(x87f_load_f32(sphereOffset),
                             x87f_load_f32(traceWork.end[axis])),
                    x87f_load_f32(traceWork.sphere.radius)));
            } else {
                traceWork.bounds[0][axis] = x87f_store_f32(x87f_sub(
                    x87f_sub(x87f_load_f32(traceWork.end[axis]),
                             x87f_load_f32(sphereOffset)),
                    x87f_load_f32(traceWork.sphere.radius)));
                traceWork.bounds[1][axis] = x87f_store_f32(x87f_add(
                    x87f_add(x87f_load_f32(sphereOffset),
                             x87f_load_f32(traceWork.start[axis])),
                    x87f_load_f32(traceWork.sphere.radius)));
            }
#else
            /* 0x805ad8b: stock inlines the fabs here (no helper call). */
            const float sphereOffset =
                fabsf(traceWork.sphere.offset[axis]);

            if (traceWork.start[axis] < traceWork.end[axis]) {
                traceWork.bounds[0][axis] =
                    traceWork.start[axis] - sphereOffset -
                    traceWork.sphere.radius;
                traceWork.bounds[1][axis] =
                    sphereOffset + traceWork.end[axis] +
                    traceWork.sphere.radius;
            } else {
                traceWork.bounds[0][axis] =
                    traceWork.end[axis] - sphereOffset -
                    traceWork.sphere.radius;
                traceWork.bounds[1][axis] =
                    sphereOffset + traceWork.start[axis] +
                    traceWork.sphere.radius;
            }
#endif
        }
    } else {
        for (int32_t axis = 0; axis < 3; ++axis) {
#if EMULATE_X87
            if (traceWork.start[axis] < traceWork.end[axis]) {
                traceWork.bounds[0][axis] = x87f_store_f32(
                    x87f_add(x87f_load_f32(traceWork.start[axis]),
                             x87f_load_f32(traceWork.mins[axis])));
                traceWork.bounds[1][axis] = x87f_store_f32(
                    x87f_add(x87f_load_f32(traceWork.end[axis]),
                             x87f_load_f32(traceWork.maxs[axis])));
            } else {
                traceWork.bounds[0][axis] = x87f_store_f32(
                    x87f_add(x87f_load_f32(traceWork.end[axis]),
                             x87f_load_f32(traceWork.mins[axis])));
                traceWork.bounds[1][axis] = x87f_store_f32(
                    x87f_add(x87f_load_f32(traceWork.start[axis]),
                             x87f_load_f32(traceWork.maxs[axis])));
            }
#else
            if (traceWork.start[axis] < traceWork.end[axis]) {
                traceWork.bounds[0][axis] =
                    traceWork.start[axis] + traceWork.mins[axis];
                traceWork.bounds[1][axis] =
                    traceWork.end[axis] + traceWork.maxs[axis];
            } else {
                traceWork.bounds[0][axis] =
                    traceWork.end[axis] + traceWork.mins[axis];
                traceWork.bounds[1][axis] =
                    traceWork.start[axis] + traceWork.maxs[axis];
            }
#endif
        }
    }

    if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2]) {
        if (model == 0) {
            CM_PositionTest(&traceWork);
        } else if (model == CM_TEMP_CAPSULE_MODEL_HANDLE) {
            if ((cm_boxBrush->contents & brushMask) != 0) {
                if (traceWork.sphere.use != 0) {
                    CM_TestCapsuleInCapsule(&traceWork);
                } else {
                    CM_TestBoundingBoxInCapsule(&traceWork);
                }
            }
        } else {
            CM_TestInLeaf(&traceWork, &collisionModel->leaf);
        }
    } else {
#if EMULATE_X87
        /* The maxs sum is formed and compared against zero in 80-bit (no float
         * store); sphereExtents = radius + |offset| is one chain per axis. */
        traceWork.isPoint =
            x87f_eq(x87f_add(x87f_add(x87f_load_f32(traceWork.maxs[0]),
                                      x87f_load_f32(traceWork.maxs[1])),
                             x87f_load_f32(traceWork.maxs[2])),
                    x87f_load_f32(0.0f));

        if (traceWork.sphere.use != 0) {
            for (int32_t axis = 0; axis < 3; ++axis) {
                traceWork.sphereExtents[axis] = x87f_store_f32(x87f_add(
                    x87f_load_f32(traceWork.sphere.radius),
                    x87f_abs(x87f_load_f32(traceWork.sphere.offset[axis]))));
            }
        }
#else
        traceWork.isPoint =
            (traceWork.maxs[0] + traceWork.maxs[1] + traceWork.maxs[2] ==
             0.0f);

        if (traceWork.sphere.use != 0) {
            for (int32_t axis = 0; axis < 3; ++axis) {
                /* 0x805b020: stock calls the CM_TraceFloatAbs helper here
                 * (exact value either way; form matches the call). */
                traceWork.sphereExtents[axis] =
                    traceWork.sphere.radius +
                    CM_TraceFloatAbs(traceWork.sphere.offset[axis]);
            }
        }
#endif

        if (model == 0) {
            CM_TraceThroughTree(&traceWork, 0, 0.0f,
                                traceWork.trace.fraction, traceWork.start,
                                traceWork.end);
        } else if (model == CM_TEMP_CAPSULE_MODEL_HANDLE) {
            if ((cm_boxBrush->contents & traceWork.contents) != 0) {
                if (traceWork.sphere.use != 0) {
                    CM_TraceCapsuleThroughCapsule(&traceWork);
                } else {
                    CM_TraceBoundingBoxThroughCapsule(&traceWork);
                }
            }
        } else {
            CM_TraceThroughLeaf(&traceWork, &collisionModel->leaf);
        }
    }

#if EMULATE_X87
    /* endpos = start + delta*fraction: one 80-bit chain per axis, rounded once. */
    for (int32_t axis = 0; axis < 3; ++axis) {
        traceWork.trace.endpos[axis] = x87f_store_f32(
            x87f_add(x87f_load_f32(start[axis]),
                     x87f_mul(x87f_load_f32(traceWork.delta[axis]),
                              x87f_load_f32(traceWork.trace.fraction))));
    }
#else
    traceWork.trace.endpos[0] =
        start[0] + traceWork.delta[0] * traceWork.trace.fraction;
    traceWork.trace.endpos[1] =
        start[1] + traceWork.delta[1] * traceWork.trace.fraction;
    traceWork.trace.endpos[2] =
        start[2] + traceWork.delta[2] * traceWork.trace.fraction;
#endif

    *trace = traceWork.trace;
}


static const vec3_t cm_sight_trace_zero_vec = {0.0f, 0.0f, 0.0f};

int32_t
CM_SightTrace(int32_t result,
              const vec3_t start,
              const vec3_t end,
              const vec3_t mins,
              const vec3_t maxs,
              int32_t model,
              const vec3_t origin,
              int32_t brushMask,
              qboolean capsule,
              const cmTraceSphereRecord_t *sphere)
{
    const collisionModel_t *collisionModel =
        CM_ClipHandleToModel(model);
    traceWork_t traceWork;
    vec3_t offset;

    (void)origin;

    cm_checkcount++;
    Com_Memset(&traceWork, 0, sizeof(traceWork));
    traceWork.trace.fraction = 1.0f;

    const float *traceMins = mins;
    if (traceMins == NULL) {
        traceMins = cm_sight_trace_zero_vec;
    }

    const float *traceMaxs = maxs;
    if (traceMaxs == NULL) {
        traceMaxs = cm_sight_trace_zero_vec;
    }

    traceWork.contents = brushMask;
    for (int32_t axis = 0; axis < 3; ++axis) {
        offset[axis] = (traceMins[axis] + traceMaxs[axis]) * 0.5f;
        traceWork.mins[axis] = traceMins[axis] - offset[axis];
        traceWork.maxs[axis] = traceMaxs[axis] - offset[axis];
        traceWork.start[axis] = start[axis] + offset[axis];
        traceWork.end[axis] = end[axis] + offset[axis];
        traceWork.delta[axis] =
            traceWork.end[axis] - traceWork.start[axis];
    }

    traceWork.deltaLengthSquared =
        (traceWork.delta[0] * traceWork.delta[0]) +
        (traceWork.delta[1] * traceWork.delta[1]) +
        (traceWork.delta[2] * traceWork.delta[2]);

    if (sphere != NULL) {
        traceWork.sphere = sphere->sphere;
        traceWork.sphereExtents[0] = sphere->extents[0];
        traceWork.sphereExtents[1] = sphere->extents[1];
        traceWork.sphereExtents[2] = sphere->extents[2];
    } else {
        traceWork.sphere.use = capsule;
        if (traceWork.maxs[2] < traceWork.maxs[0]) {
            traceWork.sphere.radius = traceWork.maxs[2];
        } else {
            traceWork.sphere.radius = traceWork.maxs[0];
        }
        traceWork.sphere.halfheight = traceWork.maxs[2];
        traceWork.sphere.offset[0] = 0.0f;
        traceWork.sphere.offset[1] = 0.0f;
        traceWork.sphere.offset[2] =
            traceWork.maxs[2] - traceWork.sphere.radius;
    }

    /* 0x805cf96..0x805cfa8: stock sums maxs into the (never-read) work
     * field at +0xa0 with one float rounding. */
    traceWork.maxsSum =
        traceWork.maxs[0] + traceWork.maxs[1] + traceWork.maxs[2];

    traceWork.offsets[0][0] = traceWork.mins[0];
    traceWork.offsets[0][1] = traceWork.mins[1];
    traceWork.offsets[0][2] = traceWork.mins[2];
    traceWork.offsets[1][0] = traceWork.maxs[0];
    traceWork.offsets[1][1] = traceWork.mins[1];
    traceWork.offsets[1][2] = traceWork.mins[2];
    traceWork.offsets[2][0] = traceWork.mins[0];
    traceWork.offsets[2][1] = traceWork.maxs[1];
    traceWork.offsets[2][2] = traceWork.mins[2];
    traceWork.offsets[3][0] = traceWork.maxs[0];
    traceWork.offsets[3][1] = traceWork.maxs[1];
    traceWork.offsets[3][2] = traceWork.mins[2];
    traceWork.offsets[4][0] = traceWork.mins[0];
    traceWork.offsets[4][1] = traceWork.mins[1];
    traceWork.offsets[4][2] = traceWork.maxs[2];
    traceWork.offsets[5][0] = traceWork.maxs[0];
    traceWork.offsets[5][1] = traceWork.mins[1];
    traceWork.offsets[5][2] = traceWork.maxs[2];
    traceWork.offsets[6][0] = traceWork.mins[0];
    traceWork.offsets[6][1] = traceWork.maxs[1];
    traceWork.offsets[6][2] = traceWork.maxs[2];
    traceWork.offsets[7][0] = traceWork.maxs[0];
    traceWork.offsets[7][1] = traceWork.maxs[1];
    traceWork.offsets[7][2] = traceWork.maxs[2];

    if (traceWork.sphere.use != 0) {
        for (int32_t axis = 0; axis < 3; ++axis) {
            /* 0x805d114: stock inlines the fabs here (no helper call). */
            const float sphereOffset =
                fabsf(traceWork.sphere.offset[axis]);

            if (traceWork.start[axis] < traceWork.end[axis]) {
                traceWork.bounds[0][axis] =
                    traceWork.start[axis] - sphereOffset -
                    traceWork.sphere.radius;
                traceWork.bounds[1][axis] =
                    sphereOffset + traceWork.end[axis] +
                    traceWork.sphere.radius;
            } else {
                traceWork.bounds[0][axis] =
                    traceWork.end[axis] - sphereOffset -
                    traceWork.sphere.radius;
                traceWork.bounds[1][axis] =
                    sphereOffset + traceWork.start[axis] +
                    traceWork.sphere.radius;
            }
        }
    } else {
        for (int32_t axis = 0; axis < 3; ++axis) {
            if (traceWork.start[axis] < traceWork.end[axis]) {
                traceWork.bounds[0][axis] =
                    traceWork.start[axis] + traceWork.mins[axis];
                traceWork.bounds[1][axis] =
                    traceWork.end[axis] + traceWork.maxs[axis];
            } else {
                traceWork.bounds[0][axis] =
                    traceWork.end[axis] + traceWork.mins[axis];
                traceWork.bounds[1][axis] =
                    traceWork.start[axis] + traceWork.maxs[axis];
            }
        }
    }

    traceWork.isPoint =
        (traceWork.maxs[0] + traceWork.maxs[1] + traceWork.maxs[2] ==
         0.0f);

    if (traceWork.sphere.use != 0) {
        for (int32_t axis = 0; axis < 3; ++axis) {
            /* 0x805d2c3: stock calls the CM_TraceFloatAbs helper here
             * (exact value either way; form matches the call). */
            traceWork.sphereExtents[axis] =
                traceWork.sphere.radius +
                CM_TraceFloatAbs(traceWork.sphere.offset[axis]);
        }
    }

    int32_t sightHit;
    if (model == 0) {
        sightHit = 0;
        if (result > 0) {
            const int32_t hitIndex = result - 1;

            if (hitIndex < cm_numBrushes) {
                sightHit =
                    CM_SightTraceThroughBrush(&traceWork,
                                              &cm_brushes[hitIndex]);
            } else if (hitIndex - cm_numBrushes < cm_numTerrainPatches) {
                /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                sightHit = CM_SightTraceThroughPatch(
                    &traceWork,
                    &cm_terrainPatches[hitIndex - cm_numBrushes]);
            }
        }

        if (sightHit == 0) {
            sightHit = CM_SightTraceThroughTree(&traceWork, 0, 0.0f, 1.0f,
                                                traceWork.start,
                                                traceWork.end);
        }
    } else if (model == CM_TEMP_CAPSULE_MODEL_HANDLE) {
        if ((cm_boxBrush->contents & traceWork.contents) == 0) {
            sightHit = 0;
        } else if (traceWork.sphere.use == 0) {
            sightHit = CM_SightTraceBoundingBoxThroughCapsule(&traceWork);
        } else {
            sightHit = CM_SightTraceCapsuleThroughCapsule(&traceWork);
        }
    } else {
        sightHit = CM_SightTraceThroughLeaf(&traceWork,
                                            &collisionModel->leaf);
    }

    return sightHit;
}
#endif
