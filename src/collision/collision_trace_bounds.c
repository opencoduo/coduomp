#include "collision_trace_bounds.h"

#include "compat/coduo_x87emu.h"

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "collision_trace_bounds.c requires a platform behavior mode"
#endif

/*
 * Complete collision broad-phase bounds-rejection cluster:
 *
 *   CoDUOMP.exe  CM_TraceWorkIntersectsBounds 0x00426820..0x004269ea
 *                CM_TraceLineSkipsBox         0x0042ad30..0x0042ae33
 *   coduo_lnxded CM_TraceWorkIntersectsBounds 0x08058b85..0x08058e2e
 *                CM_TraceLineSkipsBox         0x0805e387..0x0805e4fc
 *
 * The predicates and stored results agree.  The original Windows compiler
 * retains selected separating-axis and interval values in x87 registers,
 * while the Linux compiler spills those values to binary32.  Whole-function
 * behavior bodies preserve those proven realization boundaries without
 * splitting ownership of the subsystem.
 */

#if defined(WINDOWS_BEHAVIOR)
qboolean CM_TraceWorkIntersectsBounds(const traceWork_t *traceWork,
                                      const vec3_t mins,
                                      const vec3_t maxs)
{
    for (int32_t axis = 0; axis < 3; ++axis) {
#if EMULATE_X87
        if (x87f_lt_signaling(x87f_load_f32(maxs[axis]),
                              x87f_load_f32(traceWork->bounds[0][axis])) ||
            x87f_lt_signaling(x87f_load_f32(traceWork->bounds[1][axis]),
                              x87f_load_f32(mins[axis]))) {
#else
        if (maxs[axis] < traceWork->bounds[0][axis] ||
            traceWork->bounds[1][axis] < mins[axis]) {
#endif
            return qfalse;
        }
    }

    if (traceWork->isPoint == qfalse) {
        return qtrue;
    }

    vec3_t boundsSize;
    vec3_t boundsCenterSum;
    vec3_t traceCenterSum;
    vec3_t centerDelta;
    vec3_t traceBoundsSize;
    for (int32_t axis = 0; axis < 3; ++axis) {
#if EMULATE_X87
        boundsSize[axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(maxs[axis]), x87f_load_f32(mins[axis])));
        boundsCenterSum[axis] = x87f_store_f32(x87f_add(
            x87f_load_f32(maxs[axis]), x87f_load_f32(mins[axis])));
        traceCenterSum[axis] = x87f_store_f32(x87f_add(
            x87f_load_f32(traceWork->end[axis]),
            x87f_load_f32(traceWork->start[axis])));
        centerDelta[axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(traceCenterSum[axis]),
            x87f_load_f32(boundsCenterSum[axis])));
        traceBoundsSize[axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(traceWork->bounds[1][axis]),
            x87f_load_f32(traceWork->bounds[0][axis])));
#else
        boundsSize[axis] = maxs[axis] - mins[axis];
        boundsCenterSum[axis] = maxs[axis] + mins[axis];
        traceCenterSum[axis] = traceWork->end[axis] + traceWork->start[axis];
        centerDelta[axis] = traceCenterSum[axis] - boundsCenterSum[axis];
        traceBoundsSize[axis] =
            traceWork->bounds[1][axis] - traceWork->bounds[0][axis];
#endif
    }

#if EMULATE_X87
    const x87f crossX = x87f_sub(
        x87f_mul(x87f_load_f32(traceWork->delta[1]),
                 x87f_load_f32(centerDelta[2])),
        x87f_mul(x87f_load_f32(traceWork->delta[2]),
                 x87f_load_f32(centerDelta[1])));
    const x87f projectedX = x87f_add(
        x87f_mul(x87f_load_f32(boundsSize[1]),
                 x87f_load_f32(traceBoundsSize[2])),
        x87f_mul(x87f_load_f32(boundsSize[2]),
                 x87f_load_f32(traceBoundsSize[1])));
    if (x87f_lt_signaling(x87f_mul(projectedX, projectedX),
                          x87f_mul(crossX, crossX))) {
        return qfalse;
    }

    const x87f crossY = x87f_sub(
        x87f_mul(x87f_load_f32(traceWork->delta[2]),
                 x87f_load_f32(centerDelta[0])),
        x87f_mul(x87f_load_f32(traceWork->delta[0]),
                 x87f_load_f32(centerDelta[2])));
    const x87f projectedY = x87f_add(
        x87f_mul(x87f_load_f32(boundsSize[0]),
                 x87f_load_f32(traceBoundsSize[2])),
        x87f_mul(x87f_load_f32(boundsSize[2]),
                 x87f_load_f32(traceBoundsSize[0])));
    if (x87f_lt_signaling(x87f_mul(projectedY, projectedY),
                          x87f_mul(crossY, crossY))) {
        return qfalse;
    }

    const x87f crossZ = x87f_sub(
        x87f_mul(x87f_load_f32(traceWork->delta[0]),
                 x87f_load_f32(centerDelta[1])),
        x87f_mul(x87f_load_f32(traceWork->delta[1]),
                 x87f_load_f32(centerDelta[0])));
    const x87f projectedZ = x87f_add(
        x87f_mul(x87f_load_f32(boundsSize[0]),
                 x87f_load_f32(traceBoundsSize[1])),
        x87f_mul(x87f_load_f32(boundsSize[1]),
                 x87f_load_f32(traceBoundsSize[0])));
    if (x87f_lt_signaling(x87f_mul(projectedZ, projectedZ),
                          x87f_mul(crossZ, crossZ))) {
        return qfalse;
    }
#else
    const long double crossX =
        (long double)traceWork->delta[1] * centerDelta[2] -
        (long double)traceWork->delta[2] * centerDelta[1];
    const long double projectedX =
        (long double)boundsSize[1] * traceBoundsSize[2] +
        (long double)boundsSize[2] * traceBoundsSize[1];
    if (projectedX * projectedX < crossX * crossX) {
        return qfalse;
    }

    const long double crossY =
        (long double)traceWork->delta[2] * centerDelta[0] -
        (long double)traceWork->delta[0] * centerDelta[2];
    const long double projectedY =
        (long double)boundsSize[0] * traceBoundsSize[2] +
        (long double)boundsSize[2] * traceBoundsSize[0];
    if (projectedY * projectedY < crossY * crossY) {
        return qfalse;
    }

    const long double crossZ =
        (long double)traceWork->delta[0] * centerDelta[1] -
        (long double)traceWork->delta[1] * centerDelta[0];
    const long double projectedZ =
        (long double)boundsSize[0] * traceBoundsSize[1] +
        (long double)boundsSize[1] * traceBoundsSize[0];
    if (projectedZ * projectedZ < crossZ * crossZ) {
        return qfalse;
    }
#endif

    return qtrue;
}

qboolean CM_TraceLineSkipsBox(const vec3_t start, const vec3_t end,
                              const vec3_t mins, const vec3_t maxs,
                              float fraction)
{
    float enterFraction = 0.0f;
    float leaveFraction = fraction;
    float sideScale = -1.0f;
    const float *bounds = mins;

    for (;;) {
        for (int32_t axis = 0; axis < 3; ++axis) {
#if EMULATE_X87
            const float startOffset = x87f_store_f32(x87f_mul(
                x87f_sub(x87f_load_f32(start[axis]),
                         x87f_load_f32(bounds[axis])),
                x87f_load_f32(sideScale)));
            const x87f endOffset = x87f_mul(
                x87f_sub(x87f_load_f32(end[axis]),
                         x87f_load_f32(bounds[axis])),
                x87f_load_f32(sideScale));

            if (x87f_lt_signaling(x87f_load_f32(0.0f),
                                  x87f_load_f32(startOffset))) {
                if (x87f_lt_signaling(x87f_load_f32(0.0f), endOffset)) {
                    return qtrue;
                }
                const x87f offsetDelta = x87f_sub(
                    x87f_load_f32(startOffset), endOffset);
                if (x87f_lt_signaling(
                        x87f_mul(x87f_load_f32(enterFraction), offsetDelta),
                        x87f_load_f32(startOffset))) {
                    enterFraction = x87f_store_f32(x87f_div(
                        x87f_load_f32(startOffset), offsetDelta));
                    if (x87f_le_signaling(x87f_load_f32(leaveFraction),
                                          x87f_load_f32(enterFraction))) {
                        return qtrue;
                    }
                }
            } else if (x87f_lt_signaling(x87f_load_f32(0.0f), endOffset)) {
                const x87f offsetDelta = x87f_sub(
                    x87f_load_f32(startOffset), endOffset);
                if (x87f_lt_signaling(
                        x87f_mul(x87f_load_f32(leaveFraction), offsetDelta),
                        x87f_load_f32(startOffset))) {
                    leaveFraction = x87f_store_f32(x87f_div(
                        x87f_load_f32(startOffset), offsetDelta));
                    if (x87f_le_signaling(x87f_load_f32(leaveFraction),
                                          x87f_load_f32(enterFraction))) {
                        return qtrue;
                    }
                }
            }
#else
            const float startOffset = (float)(
                ((long double)start[axis] - bounds[axis]) * sideScale);
            const long double endOffset =
                ((long double)end[axis] - bounds[axis]) * sideScale;

            if (startOffset > 0.0f) {
                if (endOffset > 0.0L) {
                    return qtrue;
                }
                const long double offsetDelta =
                    (long double)startOffset - endOffset;
                if ((long double)enterFraction * offsetDelta < startOffset) {
                    enterFraction = (float)(
                        (long double)startOffset / offsetDelta);
                    if (leaveFraction <= enterFraction) {
                        return qtrue;
                    }
                }
            } else if (endOffset > 0.0L) {
                const long double offsetDelta =
                    (long double)startOffset - endOffset;
                if ((long double)leaveFraction * offsetDelta < startOffset) {
                    leaveFraction = (float)(
                        (long double)startOffset / offsetDelta);
                    if (leaveFraction <= enterFraction) {
                        return qtrue;
                    }
                }
            }
#endif
        }

#if EMULATE_X87
        if (x87f_eq(x87f_load_f32(sideScale), x87f_load_f32(1.0f))) {
#else
        if (sideScale == 1.0f) {
#endif
            return qfalse;
        }
        sideScale = 1.0f;
        bounds = maxs;
    }
}
#else
qboolean CM_TraceWorkIntersectsBounds(const traceWork_t *traceWork,
                                      const vec3_t mins,
                                      const vec3_t maxs)
{
    for (int32_t axis = 0; axis < 3; ++axis) {
#if EMULATE_X87
        if (x87f_lt(x87f_load_f32(maxs[axis]),
                    x87f_load_f32(traceWork->bounds[0][axis])) ||
            x87f_lt(x87f_load_f32(traceWork->bounds[1][axis]),
                    x87f_load_f32(mins[axis]))) {
#else
        if (maxs[axis] < traceWork->bounds[0][axis] ||
            traceWork->bounds[1][axis] < mins[axis]) {
#endif
            return qfalse;
        }
    }

    if (traceWork->isPoint == qfalse) {
        return qtrue;
    }

    vec3_t boundsSize;
    vec3_t boundsCenterSum;
    vec3_t traceCenterSum;
    vec3_t centerDelta;
    vec3_t traceBoundsSize;
    for (int32_t axis = 0; axis < 3; ++axis) {
#if EMULATE_X87
        boundsSize[axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(maxs[axis]), x87f_load_f32(mins[axis])));
        boundsCenterSum[axis] = x87f_store_f32(x87f_add(
            x87f_load_f32(maxs[axis]), x87f_load_f32(mins[axis])));
        traceCenterSum[axis] = x87f_store_f32(x87f_add(
            x87f_load_f32(traceWork->end[axis]),
            x87f_load_f32(traceWork->start[axis])));
        centerDelta[axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(traceCenterSum[axis]),
            x87f_load_f32(boundsCenterSum[axis])));
        traceBoundsSize[axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(traceWork->bounds[1][axis]),
            x87f_load_f32(traceWork->bounds[0][axis])));
#else
        boundsSize[axis] = maxs[axis] - mins[axis];
        boundsCenterSum[axis] = maxs[axis] + mins[axis];
        traceCenterSum[axis] = traceWork->end[axis] + traceWork->start[axis];
        centerDelta[axis] = traceCenterSum[axis] - boundsCenterSum[axis];
        traceBoundsSize[axis] =
            traceWork->bounds[1][axis] - traceWork->bounds[0][axis];
#endif
    }

#if EMULATE_X87
    float cross = x87f_store_f32(x87f_sub(
        x87f_mul(x87f_load_f32(traceWork->delta[1]),
                 x87f_load_f32(centerDelta[2])),
        x87f_mul(x87f_load_f32(traceWork->delta[2]),
                 x87f_load_f32(centerDelta[1]))));
    float projected = x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(boundsSize[1]),
                 x87f_load_f32(traceBoundsSize[2])),
        x87f_mul(x87f_load_f32(boundsSize[2]),
                 x87f_load_f32(traceBoundsSize[1]))));
    if (x87f_lt(x87f_mul(x87f_load_f32(projected),
                         x87f_load_f32(projected)),
                x87f_mul(x87f_load_f32(cross),
                         x87f_load_f32(cross)))) {
        return qfalse;
    }

    cross = x87f_store_f32(x87f_sub(
        x87f_mul(x87f_load_f32(traceWork->delta[2]),
                 x87f_load_f32(centerDelta[0])),
        x87f_mul(x87f_load_f32(traceWork->delta[0]),
                 x87f_load_f32(centerDelta[2]))));
    projected = x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(boundsSize[0]),
                 x87f_load_f32(traceBoundsSize[2])),
        x87f_mul(x87f_load_f32(boundsSize[2]),
                 x87f_load_f32(traceBoundsSize[0]))));
    if (x87f_lt(x87f_mul(x87f_load_f32(projected),
                         x87f_load_f32(projected)),
                x87f_mul(x87f_load_f32(cross),
                         x87f_load_f32(cross)))) {
        return qfalse;
    }

    cross = x87f_store_f32(x87f_sub(
        x87f_mul(x87f_load_f32(traceWork->delta[0]),
                 x87f_load_f32(centerDelta[1])),
        x87f_mul(x87f_load_f32(traceWork->delta[1]),
                 x87f_load_f32(centerDelta[0]))));
    projected = x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(boundsSize[0]),
                 x87f_load_f32(traceBoundsSize[1])),
        x87f_mul(x87f_load_f32(boundsSize[1]),
                 x87f_load_f32(traceBoundsSize[0]))));
    if (x87f_lt(x87f_mul(x87f_load_f32(projected),
                         x87f_load_f32(projected)),
                x87f_mul(x87f_load_f32(cross),
                         x87f_load_f32(cross)))) {
        return qfalse;
    }
#else
    float cross = traceWork->delta[1] * centerDelta[2] -
                  traceWork->delta[2] * centerDelta[1];
    float projected = boundsSize[1] * traceBoundsSize[2] +
                      boundsSize[2] * traceBoundsSize[1];
    if (projected * projected < cross * cross) {
        return qfalse;
    }

    cross = traceWork->delta[2] * centerDelta[0] -
            traceWork->delta[0] * centerDelta[2];
    projected = boundsSize[0] * traceBoundsSize[2] +
                boundsSize[2] * traceBoundsSize[0];
    if (projected * projected < cross * cross) {
        return qfalse;
    }

    cross = traceWork->delta[0] * centerDelta[1] -
            traceWork->delta[1] * centerDelta[0];
    projected = boundsSize[0] * traceBoundsSize[1] +
                boundsSize[1] * traceBoundsSize[0];
    if (projected * projected < cross * cross) {
        return qfalse;
    }
#endif

    return qtrue;
}

qboolean CM_TraceLineSkipsBox(const vec3_t start, const vec3_t end,
                              const vec3_t mins, const vec3_t maxs,
                              float fraction)
{
    float enterFraction = 0.0f;
    float leaveFraction = fraction;
    float sideScale = -1.0f;
    const float *bounds = mins;

    for (;;) {
        for (int32_t axis = 0; axis < 3; ++axis) {
#if EMULATE_X87
            const float startOffset = x87f_store_f32(x87f_mul(
                x87f_sub(x87f_load_f32(start[axis]),
                         x87f_load_f32(bounds[axis])),
                x87f_load_f32(sideScale)));
            const float endOffset = x87f_store_f32(x87f_mul(
                x87f_sub(x87f_load_f32(end[axis]),
                         x87f_load_f32(bounds[axis])),
                x87f_load_f32(sideScale)));
#else
            const float startOffset =
                (start[axis] - bounds[axis]) * sideScale;
            const float endOffset =
                (end[axis] - bounds[axis]) * sideScale;
#endif

            if (0.0f < startOffset) {
                if (0.0f < endOffset) {
                    return qtrue;
                }
#if EMULATE_X87
                const float offsetDelta = x87f_store_f32(x87f_sub(
                    x87f_load_f32(startOffset), x87f_load_f32(endOffset)));
                if (x87f_lt(
                        x87f_mul(x87f_load_f32(enterFraction),
                                 x87f_load_f32(offsetDelta)),
                        x87f_load_f32(startOffset))) {
                    enterFraction = x87f_store_f32(x87f_div(
                        x87f_load_f32(startOffset),
                        x87f_load_f32(offsetDelta)));
#else
                const float offsetDelta = startOffset - endOffset;
                if (enterFraction * offsetDelta < startOffset) {
                    enterFraction = startOffset / offsetDelta;
#endif
                    if (leaveFraction <= enterFraction) {
                        return qtrue;
                    }
                }
            } else if (0.0f < endOffset) {
#if EMULATE_X87
                const float offsetDelta = x87f_store_f32(x87f_sub(
                    x87f_load_f32(startOffset), x87f_load_f32(endOffset)));
                if (x87f_lt(
                        x87f_mul(x87f_load_f32(leaveFraction),
                                 x87f_load_f32(offsetDelta)),
                        x87f_load_f32(startOffset))) {
                    leaveFraction = x87f_store_f32(x87f_div(
                        x87f_load_f32(startOffset),
                        x87f_load_f32(offsetDelta)));
#else
                const float offsetDelta = startOffset - endOffset;
                if (leaveFraction * offsetDelta < startOffset) {
                    leaveFraction = startOffset / offsetDelta;
#endif
                    if (leaveFraction <= enterFraction) {
                        return qtrue;
                    }
                }
            }
        }

        if (sideScale == 1.0f) {
            return qfalse;
        }
        sideScale = 1.0f;
        bounds = maxs;
    }
}
#endif
