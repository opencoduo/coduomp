#include "collision_static_model_trace.h"

#include "compat/coduo_x87emu.h"
#include "collision_trace_bounds.h"
#include "math/q_math.h"
#include "animation/xmodel.h"

#include <stdint.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "collision_static_model_trace.c requires a platform behavior mode"
#endif

/*
 * Complete static-model trace subsystem shared by the Windows client/listen
 * server and Linux dedicated server:
 *
 *   CM_TraceStaticModel
 *       CoDUOMP.exe   0x004237a0..0x00423990
 *       coduo_lnxded  0x08053d9b..0x08053fa1
 *   CM_PointTraceStaticModels_r / CM_PointTraceStaticModels
 *       CoDUOMP.exe   0x0042ae40..0x0042b0ce
 *       coduo_lnxded  0x0805e4fd..0x0805e8e4
 *   CM_SightTraceStaticModels_r / CM_SightTraceStaticModels
 *       CoDUOMP.exe   0x0042b930..0x0042bb97
 *       coduo_lnxded  0x0805f5ce..0x0805f985
 *
 * The ownership, masks, traversal order, broad-phase tests, model calls, and
 * result contract agree.  Whole platform bodies retain genuine compiler/x87
 * differences: Windows keeps recursive split values live at PC=53 and inlines
 * model transforms with its observed product order; Linux stores recursive
 * distances/fractions as binary32 at PC=64 and calls the common matrix helpers.
 */

#if defined(WINDOWS_BEHAVIOR)
void CM_TraceStaticModel(worldSectorAreaLink_t *areaLink,
                         trace_t *trace, const vec3_t start,
                         const vec3_t end, int32_t contentsMask)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    trace_t localTrace;
    localTrace.fraction = trace->fraction;
    localTrace.material = NULL;
    localTrace.partName = 0;
    localTrace.partGroup = 0;

    const int32_t partNameCount = XModelNumBones(areaLink->model);
    DObjSkelMat basePose[partNameCount];
    XModelGetBasePose(areaLink->model, basePose);

    vec3_t offsetStart;
    vec3_t localStart;
    vec3_t offsetEnd;
    vec3_t localEnd;

#if EMULATE_X87
    for (int32_t axis = 0; axis < 3; ++axis) {
        offsetStart[axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(start[axis]),
            x87f_load_f32(areaLink->origin[axis])));
        offsetEnd[axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(end[axis]),
            x87f_load_f32(areaLink->origin[axis])));
    }
    for (int32_t lane = 0; lane < 3; ++lane) {
        localStart[lane] = x87f_store_f32(x87f_add(
            x87f_add(
                x87f_mul(x87f_load_f32(offsetStart[2]),
                         x87f_load_f32(areaLink->inverseAxis[2][lane])),
                x87f_mul(x87f_load_f32(offsetStart[1]),
                         x87f_load_f32(areaLink->inverseAxis[1][lane]))),
            x87f_mul(x87f_load_f32(offsetStart[0]),
                     x87f_load_f32(areaLink->inverseAxis[0][lane]))));
        localEnd[lane] = x87f_store_f32(x87f_add(
            x87f_add(
                x87f_mul(x87f_load_f32(offsetEnd[2]),
                         x87f_load_f32(areaLink->inverseAxis[2][lane])),
                x87f_mul(x87f_load_f32(offsetEnd[1]),
                         x87f_load_f32(areaLink->inverseAxis[1][lane]))),
            x87f_mul(x87f_load_f32(offsetEnd[0]),
                     x87f_load_f32(areaLink->inverseAxis[0][lane]))));
    }
#else
    for (int32_t axis = 0; axis < 3; ++axis) {
        offsetStart[axis] = (float)((long double)start[axis] -
                                    areaLink->origin[axis]);
        offsetEnd[axis] = (float)((long double)end[axis] -
                                  areaLink->origin[axis]);
    }
    for (int32_t lane = 0; lane < 3; ++lane) {
        localStart[lane] = (float)(
            ((long double)offsetStart[2] *
                 areaLink->inverseAxis[2][lane] +
             (long double)offsetStart[1] *
                 areaLink->inverseAxis[1][lane]) +
            (long double)offsetStart[0] *
                areaLink->inverseAxis[0][lane]);
        localEnd[lane] = (float)(
            ((long double)offsetEnd[2] *
                 areaLink->inverseAxis[2][lane] +
             (long double)offsetEnd[1] *
                 areaLink->inverseAxis[1][lane]) +
            (long double)offsetEnd[0] *
                areaLink->inverseAxis[0][lane]);
    }
#endif

    if (XModelTraceLine(areaLink->model, &localTrace, basePose,
                        localStart, localEnd, contentsMask) < 0) {
        return;
    }

    localTrace.entityNum = ENTITYNUM_WORLD;
    vec3_t delta;
#if EMULATE_X87
    for (int32_t axis = 0; axis < 3; ++axis) {
        delta[axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(end[axis]), x87f_load_f32(start[axis])));
        localTrace.endpos[axis] = x87f_store_f32(x87f_add(
            x87f_mul(x87f_load_f32(delta[axis]),
                     x87f_load_f32(localTrace.fraction)),
            x87f_load_f32(start[axis])));
    }

    vec3_t worldNormal;
    for (int32_t row = 0; row < 3; ++row) {
        worldNormal[row] = x87f_store_f32(x87f_add(
            x87f_add(
                x87f_mul(x87f_load_f32(localTrace.normal[2]),
                         x87f_load_f32(areaLink->inverseAxis[row][2])),
                x87f_mul(x87f_load_f32(localTrace.normal[1]),
                         x87f_load_f32(areaLink->inverseAxis[row][1]))),
            x87f_mul(x87f_load_f32(localTrace.normal[0]),
                     x87f_load_f32(areaLink->inverseAxis[row][0]))));
    }
#else
    for (int32_t axis = 0; axis < 3; ++axis) {
        delta[axis] = (float)((long double)end[axis] - start[axis]);
        localTrace.endpos[axis] = (float)(
            (long double)delta[axis] * localTrace.fraction + start[axis]);
    }

    vec3_t worldNormal;
    for (int32_t row = 0; row < 3; ++row) {
        worldNormal[row] = (float)(
            ((long double)localTrace.normal[2] *
                 areaLink->inverseAxis[row][2] +
             (long double)localTrace.normal[1] *
                 areaLink->inverseAxis[row][1]) +
            (long double)localTrace.normal[0] *
                areaLink->inverseAxis[row][0]);
    }
#endif
    (void)VectorNormalize(worldNormal);
    localTrace.normal[0] = worldNormal[0];
    localTrace.normal[1] = worldNormal[1];
    localTrace.normal[2] = worldNormal[2];
    memcpy(trace, &localTrace, sizeof(localTrace));
}
#else
void CM_TraceStaticModel(worldSectorAreaLink_t *areaLink,
                         trace_t *trace, const vec3_t start,
                         const vec3_t end, int32_t contentsMask)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    trace_t localTrace;
    localTrace.fraction = trace->fraction;
    localTrace.material = NULL;
    localTrace.partName = 0;
    localTrace.partGroup = 0;

    const int32_t partNameCount = XModelNumBones(areaLink->model);
    DObjSkelMat basePose[partNameCount];
    XModelGetBasePose(areaLink->model, basePose);

    vec3_t offsetStart;
    vec3_t offsetEnd;
#if EMULATE_X87
    for (int32_t axis = 0; axis < 3; ++axis) {
        offsetStart[axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(start[axis]),
            x87f_load_f32(areaLink->origin[axis])));
        offsetEnd[axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(end[axis]),
            x87f_load_f32(areaLink->origin[axis])));
    }
#else
    for (int32_t axis = 0; axis < 3; ++axis) {
        offsetStart[axis] = (float)((long double)start[axis] -
                                    areaLink->origin[axis]);
        offsetEnd[axis] = (float)((long double)end[axis] -
                                  areaLink->origin[axis]);
    }
#endif

    vec3_t localStart;
    vec3_t localEnd;
    MatrixTransformVector(offsetStart,
                          (const float (*)[3])areaLink->inverseAxis,
                          localStart);
    MatrixTransformVector(offsetEnd,
                          (const float (*)[3])areaLink->inverseAxis,
                          localEnd);

    if (XModelTraceLine(areaLink->model, &localTrace, basePose,
                        localStart, localEnd, contentsMask) < 0) {
        return;
    }

    localTrace.entityNum = ENTITYNUM_WORLD;
    vec3_t delta;
#if EMULATE_X87
    for (int32_t axis = 0; axis < 3; ++axis) {
        delta[axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(end[axis]), x87f_load_f32(start[axis])));
        localTrace.endpos[axis] = x87f_store_f32(x87f_add(
            x87f_mul(x87f_load_f32(delta[axis]),
                     x87f_load_f32(localTrace.fraction)),
            x87f_load_f32(start[axis])));
    }
#else
    for (int32_t axis = 0; axis < 3; ++axis) {
        delta[axis] = (float)((long double)end[axis] - start[axis]);
        localTrace.endpos[axis] = (float)(
            (long double)delta[axis] * localTrace.fraction + start[axis]);
    }
#endif

    vec3_t worldNormal;
    MatrixTransposeTransformVector(localTrace.normal,
                                   areaLink->inverseAxis, worldNormal);
    (void)VectorNormalize(worldNormal);
    localTrace.normal[0] = worldNormal[0];
    localTrace.normal[1] = worldNormal[1];
    localTrace.normal[2] = worldNormal[2];
    memcpy(trace, &localTrace, sizeof(localTrace));
}
#endif

#if defined(WINDOWS_BEHAVIOR)
void CM_PointTraceStaticModels_r(cmPointTraceStaticModelsWork_t *work,
                                 worldSector_t *sector,
                                 float startFraction, float endFraction,
                                 const vec3_t start, const vec3_t end)
{
#if EMULATE_X87
    if (x87f_le_signaling(x87f_load_f32(work->trace.fraction),
                          x87f_load_f32(startFraction)) ||
        (work->contentsMask & sector->staticModelContentsMask) == 0) {
        return;
    }

    const int32_t axis = sector->axis;
    const x87f startDistance = x87f_sub(
        x87f_load_f32(start[axis]), x87f_load_f32(sector->dist));
    const float endDistance = x87f_store_f32(x87f_sub(
        x87f_load_f32(end[axis]), x87f_load_f32(sector->dist)));
    const x87f zero = x87f_load_f32(0.0f);
    const x87f endDistanceValue = x87f_load_f32(endDistance);

    if (!x87f_le_signaling(zero, startDistance) ||
        !x87f_le_signaling(zero, endDistanceValue)) {
        if (!x87f_le_signaling(startDistance, zero) ||
            !x87f_le_signaling(endDistanceValue, zero)) {
            const x87f splitFraction = x87f_div(
                startDistance,
                x87f_sub(startDistance, x87f_load_f32(endDistance)));
            const float middleFraction = x87f_store_f32(x87f_add(
                x87f_load_f32(startFraction),
                x87f_mul(x87f_sub(x87f_load_f32(endFraction),
                                  x87f_load_f32(startFraction)),
                         splitFraction)));
            vec3_t middle;
            for (int32_t lane = 0; lane < 3; ++lane) {
                middle[lane] = x87f_store_f32(x87f_add(
                    x87f_load_f32(start[lane]),
                    x87f_mul(x87f_sub(x87f_load_f32(end[lane]),
                                      x87f_load_f32(start[lane])),
                             splitFraction)));
            }
            const int32_t side =
                x87f_lt_signaling(startDistance, zero) ? 1 : 0;
            CM_PointTraceStaticModels_r(work, sector->children[side],
                                        startFraction, middleFraction,
                                        start, middle);
            CM_PointTraceStaticModels_r(work, sector->children[1 - side],
                                        middleFraction, endFraction,
                                        middle, end);
        } else {
            CM_PointTraceStaticModels_r(work, sector->children[1],
                                        startFraction, endFraction,
                                        start, end);
        }
    } else {
        CM_PointTraceStaticModels_r(work, sector->children[0],
                                    startFraction, endFraction, start, end);
    }
#else
    if (work->trace.fraction <= startFraction ||
        (work->contentsMask & sector->staticModelContentsMask) == 0) {
        return;
    }

    const int32_t axis = sector->axis;
    const long double startDistance =
        (long double)start[axis] - sector->dist;
    const float endDistance =
        (float)((long double)end[axis] - sector->dist);
    if (!(startDistance >= 0.0L) || !(endDistance >= 0.0f)) {
        if (!(startDistance <= 0.0L) || !(endDistance <= 0.0f)) {
            const long double splitFraction =
                startDistance / (startDistance - endDistance);
            const float middleFraction = (float)(
                (long double)startFraction +
                ((long double)endFraction - startFraction) * splitFraction);
            vec3_t middle;
            for (int32_t lane = 0; lane < 3; ++lane) {
                middle[lane] = (float)(
                    (long double)start[lane] +
                    ((long double)end[lane] - start[lane]) * splitFraction);
            }
            const int32_t side = startDistance < 0.0L ? 1 : 0;
            CM_PointTraceStaticModels_r(work, sector->children[side],
                                        startFraction, middleFraction,
                                        start, middle);
            CM_PointTraceStaticModels_r(work, sector->children[1 - side],
                                        middleFraction, endFraction,
                                        middle, end);
        } else {
            CM_PointTraceStaticModels_r(work, sector->children[1],
                                        startFraction, endFraction,
                                        start, end);
        }
    } else {
        CM_PointTraceStaticModels_r(work, sector->children[0],
                                    startFraction, endFraction, start, end);
    }
#endif

    for (worldSectorAreaLink_t *areaLink = sector->staticModelLinkHead;
         areaLink != NULL; areaLink = areaLink->nextInWorldSector) {
        if ((XModelGetContents(areaLink->model) & work->contentsMask) == 0 ||
            CM_TraceLineSkipsBox(work->start, work->end,
                                 areaLink->linkMins, areaLink->linkMaxs,
                                 work->trace.fraction) != qfalse) {
            continue;
        }
        CM_TraceStaticModel(areaLink, &work->trace, work->start, work->end,
                            work->contentsMask);
    }
}

void CM_PointTraceStaticModels(trace_t *trace, const vec3_t start,
                               const vec3_t end, int32_t contentsMask)
{
    cmPointTraceStaticModelsWork_t work;
    memset(&work.trace, 0, sizeof(work.trace));
    work.trace.fraction = trace->fraction;
    work.contentsMask = contentsMask;
    memcpy(work.start, start, sizeof(work.start));
    memcpy(work.end, end, sizeof(work.end));

    CM_PointTraceStaticModels_r(&work, &cm_worldSectorRoot, 0.0f,
                                work.trace.fraction, work.start, work.end);
    if (
#if EMULATE_X87
        x87f_lt_signaling(x87f_load_f32(work.trace.fraction),
                          x87f_load_f32(trace->fraction))
#else
        work.trace.fraction < trace->fraction
#endif
    ) {
#if EMULATE_X87
        const x87f deltaX = x87f_sub(x87f_load_f32(end[0]),
                                     x87f_load_f32(start[0]));
        const x87f deltaY = x87f_sub(x87f_load_f32(end[1]),
                                     x87f_load_f32(start[1]));
        const float deltaZ = x87f_store_f32(x87f_sub(
            x87f_load_f32(end[2]), x87f_load_f32(start[2])));
        work.trace.endpos[0] = x87f_store_f32(x87f_add(
            x87f_load_f32(start[0]),
            x87f_mul(deltaX, x87f_load_f32(work.trace.fraction))));
        work.trace.endpos[1] = x87f_store_f32(x87f_add(
            x87f_load_f32(start[1]),
            x87f_mul(deltaY, x87f_load_f32(work.trace.fraction))));
        work.trace.endpos[2] = x87f_store_f32(x87f_add(
            x87f_load_f32(start[2]),
            x87f_mul(x87f_load_f32(deltaZ),
                     x87f_load_f32(work.trace.fraction))));
#else
        const long double deltaX = (long double)end[0] - start[0];
        const long double deltaY = (long double)end[1] - start[1];
        const float deltaZ = (float)((long double)end[2] - start[2]);
        work.trace.endpos[0] = (float)((long double)start[0] +
                                      deltaX * work.trace.fraction);
        work.trace.endpos[1] = (float)((long double)start[1] +
                                      deltaY * work.trace.fraction);
        work.trace.endpos[2] = (float)((long double)start[2] +
                                      (long double)deltaZ *
                                          work.trace.fraction);
#endif
        *trace = work.trace;
    }
}
#else
void CM_PointTraceStaticModels_r(cmPointTraceStaticModelsWork_t *work,
                                 worldSector_t *sector,
                                 float startFraction, float endFraction,
                                 const vec3_t start, const vec3_t end)
{
#if EMULATE_X87
    if (x87f_le(x87f_load_f32(work->trace.fraction),
                x87f_load_f32(startFraction)) ||
#else
    if (work->trace.fraction <= startFraction ||
#endif
        (work->contentsMask & sector->staticModelContentsMask) == 0) {
        return;
    }

    const int32_t axis = sector->axis;
    float startDistance;
    float endDistance;
#if EMULATE_X87
    startDistance = x87f_store_f32(x87f_sub(
        x87f_load_f32(start[axis]), x87f_load_f32(sector->dist)));
    endDistance = x87f_store_f32(x87f_sub(
        x87f_load_f32(end[axis]), x87f_load_f32(sector->dist)));
#else
    startDistance = (float)((long double)start[axis] - sector->dist);
    endDistance = (float)((long double)end[axis] - sector->dist);
#endif

#if EMULATE_X87
    const x87f startDistanceValue = x87f_load_f32(startDistance);
    const x87f endDistanceValue = x87f_load_f32(endDistance);
    const x87f zero = x87f_load_f32(0.0f);
    if (!x87f_le(zero, startDistanceValue) ||
        !x87f_le(zero, endDistanceValue)) {
        if (!x87f_le(startDistanceValue, zero) ||
            !x87f_le(endDistanceValue, zero)) {
#else
    if (!(startDistance >= 0.0f) || !(endDistance >= 0.0f)) {
        if (!(startDistance <= 0.0f) || !(endDistance <= 0.0f)) {
#endif
            float splitFraction;
            float middleFraction;
            vec3_t middle;
#if EMULATE_X87
            splitFraction = x87f_store_f32(x87f_div(
                x87f_load_f32(startDistance),
                x87f_sub(x87f_load_f32(startDistance),
                         x87f_load_f32(endDistance))));
            middleFraction = x87f_store_f32(x87f_add(
                x87f_load_f32(startFraction),
                x87f_mul(x87f_sub(x87f_load_f32(endFraction),
                                  x87f_load_f32(startFraction)),
                         x87f_load_f32(splitFraction))));
            for (int32_t lane = 0; lane < 3; ++lane) {
                middle[lane] = x87f_store_f32(x87f_add(
                    x87f_load_f32(start[lane]),
                    x87f_mul(x87f_sub(x87f_load_f32(end[lane]),
                                      x87f_load_f32(start[lane])),
                             x87f_load_f32(splitFraction))));
            }
#else
            splitFraction = (float)(
                (long double)startDistance /
                ((long double)startDistance - endDistance));
            middleFraction = (float)(
                (long double)startFraction +
                ((long double)endFraction - startFraction) * splitFraction);
            for (int32_t lane = 0; lane < 3; ++lane) {
                middle[lane] = (float)(
                    (long double)start[lane] +
                    ((long double)end[lane] - start[lane]) * splitFraction);
            }
#endif
            const int32_t side =
#if EMULATE_X87
                x87f_lt(startDistanceValue, zero) ? 1 : 0;
#else
                startDistance < 0.0f ? 1 : 0;
#endif
            CM_PointTraceStaticModels_r(work, sector->children[side],
                                        startFraction, middleFraction,
                                        start, middle);
            CM_PointTraceStaticModels_r(work, sector->children[1 - side],
                                        middleFraction, endFraction,
                                        middle, end);
        } else {
            CM_PointTraceStaticModels_r(work, sector->children[1],
                                        startFraction, endFraction,
                                        start, end);
        }
    } else {
        CM_PointTraceStaticModels_r(work, sector->children[0],
                                    startFraction, endFraction, start, end);
    }

    for (worldSectorAreaLink_t *areaLink = sector->staticModelLinkHead;
         areaLink != NULL; areaLink = areaLink->nextInWorldSector) {
        if ((XModelGetContents(areaLink->model) & work->contentsMask) == 0 ||
            CM_TraceLineSkipsBox(work->start, work->end,
                                 areaLink->linkMins, areaLink->linkMaxs,
                                 work->trace.fraction) != qfalse) {
            continue;
        }
        CM_TraceStaticModel(areaLink, &work->trace, work->start, work->end,
                            work->contentsMask);
    }
}

void CM_PointTraceStaticModels(trace_t *trace, const vec3_t start,
                               const vec3_t end, int32_t contentsMask)
{
    cmPointTraceStaticModelsWork_t work;
    memset(&work.trace, 0, sizeof(work.trace));
    work.trace.fraction = trace->fraction;
    work.contentsMask = contentsMask;
    memcpy(work.start, start, sizeof(work.start));
    memcpy(work.end, end, sizeof(work.end));

    CM_PointTraceStaticModels_r(&work, &cm_worldSectorRoot, 0.0f,
                                work.trace.fraction, work.start, work.end);
    if (
#if EMULATE_X87
        x87f_lt(x87f_load_f32(work.trace.fraction),
                x87f_load_f32(trace->fraction))
#else
        work.trace.fraction < trace->fraction
#endif
    ) {
        vec3_t delta;
#if EMULATE_X87
        for (int32_t axis = 0; axis < 3; ++axis) {
            delta[axis] = x87f_store_f32(x87f_sub(
                x87f_load_f32(end[axis]), x87f_load_f32(start[axis])));
            work.trace.endpos[axis] = x87f_store_f32(x87f_add(
                x87f_load_f32(start[axis]),
                x87f_mul(x87f_load_f32(delta[axis]),
                         x87f_load_f32(work.trace.fraction))));
        }
#else
        for (int32_t axis = 0; axis < 3; ++axis) {
            delta[axis] = (float)((long double)end[axis] - start[axis]);
            work.trace.endpos[axis] = (float)(
                (long double)start[axis] +
                (long double)delta[axis] * work.trace.fraction);
        }
#endif
        *trace = work.trace;
    }
}
#endif

#if defined(WINDOWS_BEHAVIOR)
qboolean CM_SightTraceStaticModels_r(
    cmSightTraceStaticModelsWork_t *work, worldSector_t *sector,
    float startFraction, float endFraction,
    const vec3_t start, const vec3_t end)
{
    if ((work->contentsMask & sector->sightTraceStaticModelContentsMask) == 0) {
        return qtrue;
    }

    const int32_t axis = sector->axis;
#if EMULATE_X87
    const x87f startDistance = x87f_sub(
        x87f_load_f32(start[axis]), x87f_load_f32(sector->dist));
    const float endDistance = x87f_store_f32(x87f_sub(
        x87f_load_f32(end[axis]), x87f_load_f32(sector->dist)));
    const x87f zero = x87f_load_f32(0.0f);
    const x87f endDistanceValue = x87f_load_f32(endDistance);
    if (x87f_le_signaling(zero, startDistance) &&
        x87f_le_signaling(zero, endDistanceValue)) {
        if (CM_SightTraceStaticModels_r(work, sector->children[0],
                                        startFraction, endFraction,
                                        start, end) == qfalse) {
            return qfalse;
        }
    } else if (x87f_le_signaling(startDistance, zero) &&
               x87f_le_signaling(endDistanceValue, zero)) {
        if (CM_SightTraceStaticModels_r(work, sector->children[1],
                                        startFraction, endFraction,
                                        start, end) == qfalse) {
            return qfalse;
        }
    } else {
        const x87f splitFraction = x87f_div(
            startDistance,
            x87f_sub(startDistance, x87f_load_f32(endDistance)));
        const float middleFraction = x87f_store_f32(x87f_add(
            x87f_load_f32(startFraction),
            x87f_mul(x87f_sub(x87f_load_f32(endFraction),
                              x87f_load_f32(startFraction)),
                     splitFraction)));
        vec3_t middle;
        for (int32_t lane = 0; lane < 3; ++lane) {
            middle[lane] = x87f_store_f32(x87f_add(
                x87f_load_f32(start[lane]),
                x87f_mul(x87f_sub(x87f_load_f32(end[lane]),
                                  x87f_load_f32(start[lane])),
                         splitFraction)));
        }
        const int32_t side =
            x87f_lt_signaling(startDistance, zero) ? 1 : 0;
        if (CM_SightTraceStaticModels_r(work, sector->children[side],
                                        startFraction, middleFraction,
                                        start, middle) == qfalse ||
            CM_SightTraceStaticModels_r(work, sector->children[1 - side],
                                        middleFraction, endFraction,
                                        middle, end) == qfalse) {
            return qfalse;
        }
    }
#else
    const long double startDistance =
        (long double)start[axis] - sector->dist;
    const float endDistance =
        (float)((long double)end[axis] - sector->dist);
    if (startDistance >= 0.0L && endDistance >= 0.0f) {
        if (CM_SightTraceStaticModels_r(work, sector->children[0],
                                        startFraction, endFraction,
                                        start, end) == qfalse) {
            return qfalse;
        }
    } else if (startDistance <= 0.0L && endDistance <= 0.0f) {
        if (CM_SightTraceStaticModels_r(work, sector->children[1],
                                        startFraction, endFraction,
                                        start, end) == qfalse) {
            return qfalse;
        }
    } else {
        const long double splitFraction =
            startDistance / (startDistance - endDistance);
        const float middleFraction = (float)(
            (long double)startFraction +
            ((long double)endFraction - startFraction) * splitFraction);
        vec3_t middle;
        for (int32_t lane = 0; lane < 3; ++lane) {
            middle[lane] = (float)(
                (long double)start[lane] +
                ((long double)end[lane] - start[lane]) * splitFraction);
        }
        const int32_t side = startDistance < 0.0L ? 1 : 0;
        if (CM_SightTraceStaticModels_r(work, sector->children[side],
                                        startFraction, middleFraction,
                                        start, middle) == qfalse ||
            CM_SightTraceStaticModels_r(work, sector->children[1 - side],
                                        middleFraction, endFraction,
                                        middle, end) == qfalse) {
            return qfalse;
        }
    }
#endif

    trace_t trace;
    trace.fraction = 1.0f;
    for (worldSectorAreaLink_t *areaLink = sector->staticModelLinkHead;
         areaLink != NULL; areaLink = areaLink->nextInWorldSector) {
        if (areaLink->sightTraceEligible == qfalse ||
            (XModelGetContents(areaLink->model) & work->contentsMask) == 0 ||
            CM_TraceLineSkipsBox(work->start, work->end,
                                 areaLink->linkMins, areaLink->linkMaxs,
                                 1.0f) != qfalse) {
            continue;
        }
        CM_TraceStaticModel(areaLink, &trace, work->start, work->end,
                            work->contentsMask);
        if (
#if EMULATE_X87
            !x87f_eq(x87f_load_f32(trace.fraction),
                     x87f_load_f32(1.0f))
#else
            trace.fraction != 1.0f
#endif
        ) {
            return qfalse;
        }
    }
    return qtrue;
}
#else
qboolean CM_SightTraceStaticModels_r(
    cmSightTraceStaticModelsWork_t *work, worldSector_t *sector,
    float startFraction, float endFraction,
    const vec3_t start, const vec3_t end)
{
    if ((work->contentsMask & sector->sightTraceStaticModelContentsMask) == 0) {
        return qtrue;
    }

    const int32_t axis = sector->axis;
    float startDistance;
    float endDistance;
#if EMULATE_X87
    startDistance = x87f_store_f32(x87f_sub(
        x87f_load_f32(start[axis]), x87f_load_f32(sector->dist)));
    endDistance = x87f_store_f32(x87f_sub(
        x87f_load_f32(end[axis]), x87f_load_f32(sector->dist)));
#else
    startDistance = (float)((long double)start[axis] - sector->dist);
    endDistance = (float)((long double)end[axis] - sector->dist);
#endif

#if EMULATE_X87
    const x87f startDistanceValue = x87f_load_f32(startDistance);
    const x87f endDistanceValue = x87f_load_f32(endDistance);
    const x87f zero = x87f_load_f32(0.0f);
    if (x87f_le(zero, startDistanceValue) &&
        x87f_le(zero, endDistanceValue)) {
#else
    if (startDistance >= 0.0f && endDistance >= 0.0f) {
#endif
        if (CM_SightTraceStaticModels_r(work, sector->children[0],
                                        startFraction, endFraction,
                                        start, end) == qfalse) {
            return qfalse;
        }
#if EMULATE_X87
    } else if (x87f_le(startDistanceValue, zero) &&
               x87f_le(endDistanceValue, zero)) {
#else
    } else if (startDistance <= 0.0f && endDistance <= 0.0f) {
#endif
        if (CM_SightTraceStaticModels_r(work, sector->children[1],
                                        startFraction, endFraction,
                                        start, end) == qfalse) {
            return qfalse;
        }
    } else {
        float splitFraction;
        float middleFraction;
        vec3_t middle;
#if EMULATE_X87
        splitFraction = x87f_store_f32(x87f_div(
            x87f_load_f32(startDistance),
            x87f_sub(x87f_load_f32(startDistance),
                     x87f_load_f32(endDistance))));
        middleFraction = x87f_store_f32(x87f_add(
            x87f_load_f32(startFraction),
            x87f_mul(x87f_sub(x87f_load_f32(endFraction),
                              x87f_load_f32(startFraction)),
                     x87f_load_f32(splitFraction))));
        for (int32_t lane = 0; lane < 3; ++lane) {
            middle[lane] = x87f_store_f32(x87f_add(
                x87f_load_f32(start[lane]),
                x87f_mul(x87f_sub(x87f_load_f32(end[lane]),
                                  x87f_load_f32(start[lane])),
                         x87f_load_f32(splitFraction))));
        }
#else
        splitFraction = (float)(
            (long double)startDistance /
            ((long double)startDistance - endDistance));
        middleFraction = (float)(
            (long double)startFraction +
            ((long double)endFraction - startFraction) * splitFraction);
        for (int32_t lane = 0; lane < 3; ++lane) {
            middle[lane] = (float)(
                (long double)start[lane] +
                ((long double)end[lane] - start[lane]) * splitFraction);
        }
#endif
        const int32_t side =
#if EMULATE_X87
            x87f_lt(startDistanceValue, zero) ? 1 : 0;
#else
            startDistance < 0.0f ? 1 : 0;
#endif
        if (CM_SightTraceStaticModels_r(work, sector->children[side],
                                        startFraction, middleFraction,
                                        start, middle) == qfalse ||
            CM_SightTraceStaticModels_r(work, sector->children[1 - side],
                                        middleFraction, endFraction,
                                        middle, end) == qfalse) {
            return qfalse;
        }
    }

    trace_t trace;
    trace.fraction = 1.0f;
    for (worldSectorAreaLink_t *areaLink = sector->staticModelLinkHead;
         areaLink != NULL; areaLink = areaLink->nextInWorldSector) {
        if (areaLink->sightTraceEligible == qfalse ||
            (XModelGetContents(areaLink->model) & work->contentsMask) == 0 ||
            CM_TraceLineSkipsBox(work->start, work->end,
                                 areaLink->linkMins, areaLink->linkMaxs,
                                 1.0f) != qfalse) {
            continue;
        }
        CM_TraceStaticModel(areaLink, &trace, work->start, work->end,
                            work->contentsMask);
        if (
#if EMULATE_X87
            !x87f_eq(x87f_load_f32(trace.fraction),
                     x87f_load_f32(1.0f))
#else
            trace.fraction != 1.0f
#endif
        ) {
            return qfalse;
        }
    }
    return qtrue;
}
#endif

qboolean CM_SightTraceStaticModels(const vec3_t start, const vec3_t end,
                                   int32_t contentsMask)
{
    cmSightTraceStaticModelsWork_t work;
    work.contentsMask = contentsMask;
    memcpy(work.start, start, sizeof(work.start));
    memcpy(work.end, end, sizeof(work.end));
    return CM_SightTraceStaticModels_r(&work, &cm_worldSectorRoot,
                                       0.0f, 1.0f,
                                       work.start, work.end);
}
