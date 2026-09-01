#include "collision_brush_traces.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "collision_test_box_in_brush.c requires a platform behavior mode"
#endif

/*
 * Complete position-versus-brush primitive:
 *
 *   CoDUOMP.exe  0x00426000..0x004261bb
 *   coduo_lnxded 0x08057fe3..0x080582c7
 *
 * Bounds, plane selection, side traversal, and result stores agree.  Windows
 * retains its non-axial plane expressions in x87 registers; Linux stores the
 * intermediate plane distance, offset, test point, and final distance as
 * binary32.  The complete platform bodies preserve those proven spill points.
 */

#if defined(WINDOWS_BEHAVIOR)
void CM_TestBoxInBrush(
    traceWork_t *traceWork,
    const collisionBrush_t *brush)
{
    for (int32_t axis = 0; axis < 3; ++axis) {
        if (traceWork->bounds[0][axis] >
            brush->maxs[axis]) {
            return;
        }
    }
    for (int32_t axis = 0; axis < 3; ++axis) {
        if (brush->mins[axis] >
            traceWork->bounds[1][axis]) {
            return;
        }
    }

    const collisionBrushSide_t *side = brush->nonAxialSides;
    int32_t sideCount = brush->nonAxialSideCount;

    if (traceWork->sphere.use != qfalse) {
        while (sideCount != 0) {
            const cplane_t *const plane = side->plane;
            const long double planeDistRaw =
                (long double)plane->dist +
                traceWork->sphere.radius;
            const long double sphereOffsetRaw =
                ((long double)plane->normal[1] *
                     traceWork->sphere.offset[1] +
                 (long double)plane->normal[0] *
                     traceWork->sphere.offset[0]) +
                (long double)plane->normal[2] *
                    traceWork->sphere.offset[2];
            long double testPoint[3];

            if (sphereOffsetRaw > 0.0L) {
                for (int32_t axis = 0;
                     axis < 3;
                     ++axis) {
                    testPoint[axis] =
                        (long double)traceWork->start[axis] -
                        traceWork->sphere.offset[axis];
                }
            } else {
                for (int32_t axis = 0;
                     axis < 3;
                     ++axis) {
                    testPoint[axis] =
                        (long double)traceWork->start[axis] +
                        traceWork->sphere.offset[axis];
                }
            }

            const long double distanceRaw =
                (testPoint[0] * plane->normal[0] +
                 testPoint[2] * plane->normal[2]) +
                testPoint[1] * plane->normal[1] -
                planeDistRaw;
            if (distanceRaw > 0.0L)
                return;

            ++side;
            --sideCount;
        }
    } else {
        while (sideCount != 0) {
            const cplane_t *const plane = side->plane;
            const float *const offset =
                traceWork->offsets[plane->signbits];
            const long double planeDistRaw =
                (long double)plane->dist -
                (((long double)offset[2] *
                      plane->normal[2] +
                  (long double)offset[0] *
                      plane->normal[0]) +
                 (long double)offset[1] *
                     plane->normal[1]);
            const long double distanceRaw =
                ((long double)traceWork->start[1] *
                     plane->normal[1] +
                 (long double)traceWork->start[0] *
                     plane->normal[0]) +
                (long double)traceWork->start[2] *
                    plane->normal[2] -
                planeDistRaw;
            if (distanceRaw > 0.0L)
                return;

            ++side;
            --sideCount;
        }
    }

    traceWork->trace.allsolid = qtrue;
    traceWork->trace.startsolid = qtrue;
    traceWork->trace.fraction = 0.0f;
    traceWork->trace.contents = brush->contents;
}
#else
void CM_TestBoxInBrush(traceWork_t *traceWork,
                        const collisionBrush_t *brush)
{
    if (traceWork->bounds[0][0] > brush->maxs[0]) {
        return;
    }
    if (traceWork->bounds[0][1] > brush->maxs[1]) {
        return;
    }
    if (traceWork->bounds[0][2] > brush->maxs[2]) {
        return;
    }
    if (brush->mins[0] > traceWork->bounds[1][0]) {
        return;
    }
    if (brush->mins[1] > traceWork->bounds[1][1]) {
        return;
    }
    if (brush->mins[2] > traceWork->bounds[1][2]) {
        return;
    }

    const collisionBrushSide_t *side = brush->nonAxialSides;
    int32_t sideCount = brush->nonAxialSideCount;

    if (traceWork->sphere.use != 0) {
        for (; sideCount != 0; --sideCount, ++side) {
            const cplane_t *plane = side->plane;
            const float planeDist = plane->dist + traceWork->sphere.radius;
            const float sphereOffset =
                ((plane->normal[0] * traceWork->sphere.offset[0]) +
                 (plane->normal[1] * traceWork->sphere.offset[1])) +
                (plane->normal[2] * traceWork->sphere.offset[2]);
            vec3_t testPoint;

            if (sphereOffset > 0.0f) {
                testPoint[0] =
                    traceWork->start[0] - traceWork->sphere.offset[0];
                testPoint[1] =
                    traceWork->start[1] - traceWork->sphere.offset[1];
                testPoint[2] =
                    traceWork->start[2] - traceWork->sphere.offset[2];
            } else {
                testPoint[0] =
                    traceWork->start[0] + traceWork->sphere.offset[0];
                testPoint[1] =
                    traceWork->start[1] + traceWork->sphere.offset[1];
                testPoint[2] =
                    traceWork->start[2] + traceWork->sphere.offset[2];
            }

            const float distance =
                (((testPoint[0] * plane->normal[0]) +
                  (testPoint[1] * plane->normal[1])) +
                 (testPoint[2] * plane->normal[2])) -
                planeDist;
            if (distance > 0.0f) {
                return;
            }
        }
    } else {
        for (; sideCount != 0; --sideCount, ++side) {
            const cplane_t *plane = side->plane;
            const float *offset = traceWork->offsets[plane->signbits];
            const float planeDist =
                plane->dist -
                (((offset[0] * plane->normal[0]) +
                  (offset[1] * plane->normal[1])) +
                 (offset[2] * plane->normal[2]));
            const float distance =
                (((traceWork->start[0] * plane->normal[0]) +
                  (traceWork->start[1] * plane->normal[1])) +
                 (traceWork->start[2] * plane->normal[2])) -
                planeDist;
            if (distance > 0.0f) {
                return;
            }
        }
    }

    traceWork->trace.allsolid = 1;
    traceWork->trace.startsolid = 1;
    traceWork->trace.fraction = 0.0f;
    traceWork->trace.contents = brush->contents;
}
#endif
