#include "collision_brush_traces.h"

extern collisionBrush_t *cm_brushes;

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "collision_sight_trace_through_brush.c requires a platform behavior mode"
#endif

/*
 * Complete sight-segment brush clipping primitive:
 *
 *   CoDUOMP.exe  0x00428ae0..0x00428ffc
 *   coduo_lnxded 0x0805b67d..0x0805bdac
 *
 * The interval decisions and one-based brush result agree.  Windows retains
 * selected end distances and quotient comparisons at PC=53; Linux stores the
 * corresponding values as binary32 under PC=64.
 */

#if defined(WINDOWS_BEHAVIOR)
int32_t CM_SightTraceThroughBrush(
    const traceWork_t *traceWork,
    const collisionBrush_t *brush)
{
    float enterFraction = 0.0f;
    float leaveFraction = 1.0f;
    float sign = -1.0f;
    const float *brushBounds = brush->mins;
    qboolean maxsPass = qfalse;

    if (traceWork->sphere.use != qfalse) {
        for (;;) {
            for (int32_t axis = 0;
                 axis < 3; ++axis) {
                const float startDistance =
                    (traceWork->start[axis] -
                     brushBounds[axis]) *
                        sign -
                    traceWork->sphereExtents[axis];
                const long double endDistanceRaw =
                    ((long double)traceWork->end[axis] -
                     brushBounds[axis]) *
                        sign -
                    traceWork->sphereExtents[axis];

                if (startDistance > 0.0f) {
                    const float fractionDelta = (float)(
                        (long double)startDistance -
                        endDistanceRaw);
                    if (endDistanceRaw > 0.0L)
                        return 0;

                    if (enterFraction *
                            fractionDelta <
                        startDistance) {
                        const long double enterCandidate =
                            (long double)startDistance /
                            (long double)fractionDelta;
                        enterFraction = (float)enterCandidate;
                        if ((long double)leaveFraction <=
                            enterCandidate) {
                            return 0;
                        }
                    }
                } else if (endDistanceRaw > 0.0L) {
                    const long double
                        fractionDeltaRaw =
                            (long double)startDistance -
                            endDistanceRaw;
                    if ((long double)leaveFraction *
                            fractionDeltaRaw <
                        startDistance) {
                        leaveFraction = (float)(
                            (long double)startDistance /
                            fractionDeltaRaw);
                        if (leaveFraction <=
                            enterFraction) {
                            return 0;
                        }
                    }
                }
            }

            if (maxsPass != qfalse)
                break;
            sign = 1.0f;
            brushBounds = brush->maxs;
            maxsPass = qtrue;
        }

        const collisionBrushSide_t *side =
            brush->nonAxialSides;
        for (int32_t sideCount =
                 brush->nonAxialSideCount;
             sideCount != 0;
             --sideCount, ++side) {
            const cplane_t *const plane =
                side->plane;
            const long double planeDistanceRaw =
                (long double)plane->dist +
                traceWork->sphere.radius;
            const long double sphereOffsetRaw =
                ((long double)plane->normal[0] *
                      traceWork->sphere.offset[0] +
                  (long double)plane->normal[1] *
                      traceWork->sphere.offset[1]) +
                (long double)plane->normal[2] *
                    traceWork->sphere.offset[2];
            long double localStart[3];
            vec3_t localEnd;

            if (sphereOffsetRaw > 0.0L) {
                for (int32_t axis = 0;
                     axis < 3; ++axis) {
                    localStart[axis] =
                        (long double)traceWork->start[axis] -
                        traceWork->sphere
                            .offset[axis];
                    localEnd[axis] =
                        traceWork->end[axis] -
                        traceWork->sphere
                            .offset[axis];
                }
            } else {
                for (int32_t axis = 0;
                     axis < 3; ++axis) {
                    localStart[axis] =
                        (long double)traceWork->start[axis] +
                        traceWork->sphere
                            .offset[axis];
                    localEnd[axis] =
                        traceWork->end[axis] +
                        traceWork->sphere
                            .offset[axis];
                }
            }

            const float startDistance = (float)(
                (localStart[0] *
                      plane->normal[0] +
                 localStart[2] *
                      plane->normal[2]) +
                localStart[1] *
                    plane->normal[1] -
                planeDistanceRaw);
            const long double endDistanceRaw =
                ((long double)localEnd[0] *
                      plane->normal[0] +
                 (long double)localEnd[2] *
                      plane->normal[2]) +
                (long double)localEnd[1] *
                    plane->normal[1] -
                planeDistanceRaw;

            if (startDistance > 0.0f) {
                const float fractionDelta = (float)(
                    (long double)startDistance -
                    endDistanceRaw);
                if (endDistanceRaw > 0.0L)
                    return 0;

                if (enterFraction *
                        fractionDelta <
                    startDistance) {
                    const long double enterCandidate =
                        (long double)startDistance /
                        (long double)fractionDelta;
                    enterFraction = (float)enterCandidate;
                    if ((long double)leaveFraction <=
                        enterCandidate) {
                        return 0;
                    }
                }
            } else if (endDistanceRaw > 0.0L) {
                const long double fractionDeltaRaw =
                    (long double)startDistance -
                    endDistanceRaw;
                if ((long double)leaveFraction *
                        fractionDeltaRaw <
                    startDistance) {
                    leaveFraction = (float)(
                        (long double)startDistance /
                        fractionDeltaRaw);
                    if (leaveFraction <=
                        enterFraction) {
                        return 0;
                    }
                }
            }
        }
    } else {
        for (;;) {
            for (int32_t axis = 0;
                 axis < 3; ++axis) {
                const float startDistance =
                    (traceWork->start[axis] -
                     brushBounds[axis]) *
                        sign -
                    traceWork->maxs[axis];
                const long double endDistanceRaw =
                    ((long double)traceWork->end[axis] -
                     brushBounds[axis]) *
                        sign -
                    traceWork->maxs[axis];

                if (startDistance > 0.0f) {
                    const float fractionDelta = (float)(
                        (long double)startDistance -
                        endDistanceRaw);
                    if (endDistanceRaw > 0.0L)
                        return 0;

                    if (enterFraction *
                            fractionDelta <
                        startDistance) {
                        const long double enterCandidate =
                            (long double)startDistance /
                            (long double)fractionDelta;
                        enterFraction = (float)enterCandidate;
                        if ((long double)leaveFraction <=
                            enterCandidate) {
                            return 0;
                        }
                    }
                } else if (endDistanceRaw > 0.0L) {
                    const long double
                        fractionDeltaRaw =
                            (long double)startDistance -
                            endDistanceRaw;
                    if ((long double)leaveFraction *
                            fractionDeltaRaw <
                        startDistance) {
                        leaveFraction = (float)(
                            (long double)startDistance /
                            fractionDeltaRaw);
                        if (leaveFraction <=
                            enterFraction) {
                            return 0;
                        }
                    }
                }
            }

            if (maxsPass != qfalse)
                break;
            sign = 1.0f;
            brushBounds = brush->maxs;
            maxsPass = qtrue;
        }

        const collisionBrushSide_t *side =
            brush->nonAxialSides;
        for (int32_t sideCount =
                 brush->nonAxialSideCount;
             sideCount != 0;
             --sideCount, ++side) {
            const cplane_t *const plane =
                side->plane;
            const float *const cornerOffset =
                traceWork->offsets[
                    plane->signbits];
            const long double planeDistanceRaw =
                (long double)plane->dist -
                (((long double)cornerOffset[2] *
                      plane->normal[2] +
                  (long double)cornerOffset[1] *
                      plane->normal[1]) +
                 (long double)cornerOffset[0] *
                     plane->normal[0]);
            const float startDistance = (float)(
                ((long double)traceWork->start[0] *
                      plane->normal[0] +
                 (long double)traceWork->start[2] *
                      plane->normal[2]) +
                (long double)traceWork->start[1] *
                    plane->normal[1] -
                planeDistanceRaw);
            const long double endDistanceRaw =
                ((long double)traceWork->end[0] *
                      plane->normal[0] +
                 (long double)traceWork->end[2] *
                      plane->normal[2]) +
                (long double)traceWork->end[1] *
                    plane->normal[1] -
                planeDistanceRaw;

            if (startDistance > 0.0f) {
                const float fractionDelta = (float)(
                    (long double)startDistance -
                    endDistanceRaw);
                if (endDistanceRaw > 0.0L)
                    return 0;

                if (enterFraction *
                        fractionDelta <
                    startDistance) {
                    const long double enterCandidate =
                        (long double)startDistance /
                        (long double)fractionDelta;
                    enterFraction = (float)enterCandidate;
                    if ((long double)leaveFraction <=
                        enterCandidate) {
                        return 0;
                    }
                }
            } else if (endDistanceRaw > 0.0L) {
                const long double fractionDeltaRaw =
                    (long double)startDistance -
                    endDistanceRaw;
                if ((long double)leaveFraction *
                        fractionDeltaRaw <
                    startDistance) {
                    leaveFraction = (float)(
                        (long double)startDistance /
                        fractionDeltaRaw);
                    if (leaveFraction <=
                        enterFraction) {
                        return 0;
                    }
                }
            }
        }
    }

    return (int32_t)(brush - cm_brushes) + 1;
}
#else
int32_t
CM_SightTraceThroughBrush(const traceWork_t *traceWork,
                          const collisionBrush_t *brush)
{
    float enterFraction = 0.0f;
    float leaveFraction = 1.0f;
    float sign = -1.0f;
    const float *brushBounds = brush->mins;
    qboolean maxsPass = 0;

    if (traceWork->sphere.use != 0) {
        for (;;) {
            for (int32_t axis = 0; axis <= 2; ++axis) {
                const float startDistance =
                    (traceWork->start[axis] - brushBounds[axis]) * sign -
                    traceWork->sphereExtents[axis];
                const float endDistance =
                    (traceWork->end[axis] - brushBounds[axis]) * sign -
                    traceWork->sphereExtents[axis];

                if (startDistance > 0.0f) {
                    const float fractionDelta =
                        startDistance - endDistance;

                    if (endDistance > 0.0f) {
                        return 0;
                    }

                    if (enterFraction * fractionDelta < startDistance) {
                        enterFraction = startDistance / fractionDelta;
                        if (leaveFraction <= enterFraction) {
                            return 0;
                        }
                    }
                } else if (endDistance > 0.0f) {
                    const float fractionDelta =
                        startDistance - endDistance;

                    if (leaveFraction * fractionDelta < startDistance) {
                        leaveFraction = startDistance / fractionDelta;
                        if (leaveFraction <= enterFraction) {
                            return 0;
                        }
                    }
                }
            }

            if (maxsPass != 0) {
                break;
            }

            sign = 1.0f;
            brushBounds = brush->maxs;
            maxsPass = 1;
        }

        const collisionBrushSide_t *side = brush->nonAxialSides;
        for (int32_t sideCount = brush->nonAxialSideCount; sideCount != 0;
             --sideCount, ++side) {
            const cplane_t *plane = side->plane;
            const float planeDist = plane->dist + traceWork->sphere.radius;
            const float sphereOffset =
                ((plane->normal[0] * traceWork->sphere.offset[0]) +
                 (plane->normal[1] * traceWork->sphere.offset[1])) +
                (plane->normal[2] * traceWork->sphere.offset[2]);
            vec3_t start;
            vec3_t end;

            if (sphereOffset > 0.0f) {
                start[0] =
                    traceWork->start[0] - traceWork->sphere.offset[0];
                start[1] =
                    traceWork->start[1] - traceWork->sphere.offset[1];
                start[2] =
                    traceWork->start[2] - traceWork->sphere.offset[2];
                end[0] = traceWork->end[0] - traceWork->sphere.offset[0];
                end[1] = traceWork->end[1] - traceWork->sphere.offset[1];
                end[2] = traceWork->end[2] - traceWork->sphere.offset[2];
            } else {
                start[0] =
                    traceWork->start[0] + traceWork->sphere.offset[0];
                start[1] =
                    traceWork->start[1] + traceWork->sphere.offset[1];
                start[2] =
                    traceWork->start[2] + traceWork->sphere.offset[2];
                end[0] = traceWork->end[0] + traceWork->sphere.offset[0];
                end[1] = traceWork->end[1] + traceWork->sphere.offset[1];
                end[2] = traceWork->end[2] + traceWork->sphere.offset[2];
            }

            const float startDistance =
                (((start[0] * plane->normal[0]) +
                  (start[1] * plane->normal[1])) +
                 (start[2] * plane->normal[2])) -
                planeDist;
            const float endDistance =
                (((end[0] * plane->normal[0]) +
                  (end[1] * plane->normal[1])) +
                 (end[2] * plane->normal[2])) -
                planeDist;

            if (startDistance > 0.0f) {
                const float fractionDelta = startDistance - endDistance;

                if (endDistance > 0.0f) {
                    return 0;
                }

                if (enterFraction * fractionDelta < startDistance) {
                    enterFraction = startDistance / fractionDelta;
                    if (leaveFraction <= enterFraction) {
                        return 0;
                    }
                }
            } else if (endDistance > 0.0f) {
                const float fractionDelta = startDistance - endDistance;

                if (leaveFraction * fractionDelta < startDistance) {
                    leaveFraction = startDistance / fractionDelta;
                    if (leaveFraction <= enterFraction) {
                        return 0;
                    }
                }
            }
        }
    } else {
        for (;;) {
            for (int32_t axis = 0; axis <= 2; ++axis) {
                const float startDistance =
                    (traceWork->start[axis] - brushBounds[axis]) * sign -
                    traceWork->maxs[axis];
                const float endDistance =
                    (traceWork->end[axis] - brushBounds[axis]) * sign -
                    traceWork->maxs[axis];

                if (startDistance > 0.0f) {
                    const float fractionDelta =
                        startDistance - endDistance;

                    if (endDistance > 0.0f) {
                        return 0;
                    }

                    if (enterFraction * fractionDelta < startDistance) {
                        enterFraction = startDistance / fractionDelta;
                        if (leaveFraction <= enterFraction) {
                            return 0;
                        }
                    }
                } else if (endDistance > 0.0f) {
                    const float fractionDelta =
                        startDistance - endDistance;

                    if (leaveFraction * fractionDelta < startDistance) {
                        leaveFraction = startDistance / fractionDelta;
                        if (leaveFraction <= enterFraction) {
                            return 0;
                        }
                    }
                }
            }

            if (maxsPass != 0) {
                break;
            }

            sign = 1.0f;
            brushBounds = brush->maxs;
            maxsPass = 1;
        }

        const collisionBrushSide_t *side = brush->nonAxialSides;
        for (int32_t sideCount = brush->nonAxialSideCount; sideCount != 0;
             --sideCount, ++side) {
            const cplane_t *plane = side->plane;
            const float *offset = traceWork->offsets[plane->signbits];
            const float planeDist =
                plane->dist -
                (((offset[0] * plane->normal[0]) +
                  (offset[1] * plane->normal[1])) +
                 (offset[2] * plane->normal[2]));
            const float startDistance =
                (((traceWork->start[0] * plane->normal[0]) +
                  (traceWork->start[1] * plane->normal[1])) +
                 (traceWork->start[2] * plane->normal[2])) -
                planeDist;
            const float endDistance =
                (((traceWork->end[0] * plane->normal[0]) +
                  (traceWork->end[1] * plane->normal[1])) +
                 (traceWork->end[2] * plane->normal[2])) -
                planeDist;

            if (startDistance > 0.0f) {
                const float fractionDelta = startDistance - endDistance;

                if (endDistance > 0.0f) {
                    return 0;
                }

                if (enterFraction * fractionDelta < startDistance) {
                    enterFraction = startDistance / fractionDelta;
                    if (leaveFraction <= enterFraction) {
                        return 0;
                    }
                }
            } else if (endDistance > 0.0f) {
                const float fractionDelta = startDistance - endDistance;

                if (leaveFraction * fractionDelta < startDistance) {
                    leaveFraction = startDistance / fractionDelta;
                    if (leaveFraction <= enterFraction) {
                        return 0;
                    }
                }
            }
        }
    }

    return (int32_t)(brush - cm_brushes) + 1;
}
#endif
