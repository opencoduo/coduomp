#include "collision_brush_traces.h"

#include "compat/coduo_x87emu.h"

extern dshader_t *cm_materials;

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "collision_trace_through_brush.c requires a platform behavior mode"
#endif

/*
 * Complete swept-volume brush clipping primitive:
 *
 *   CoDUOMP.exe  0x00426a80..0x0042721b
 *   coduo_lnxded 0x08058f0a..0x08059874
 *
 * Both bodies clip the same enter/leave interval and select the same side.
 * Windows retains selected end distances and quotient comparisons at PC=53;
 * Linux stores those intermediates as binary32 under PC=64.  The separate
 * complete bodies preserve the original spill boundaries.
 */

#if defined(WINDOWS_BEHAVIOR)
void CM_TraceThroughBrush(
    traceWork_t *traceWork,
    const collisionBrush_t *brush)
{
    const float enterEpsilon = 0.125f;
    float enterFraction = 0.0f;
    float leaveFraction =
        traceWork->trace.fraction;
    qboolean allSolid = qtrue;
    const collisionBrushSide_t *hitSide = NULL;
    cplane_t axialPlane = {0};
    collisionBrushSide_t axialSide = {
        &axialPlane, 0
    };

    for (int32_t boundsPass = 0;
         boundsPass < 2;
         ++boundsPass) {
        const float sign =
            boundsPass == 0 ? -1.0f : 1.0f;
        const float *const brushBounds =
            boundsPass == 0
                ? brush->mins
                : brush->maxs;

        for (int32_t axis = 0; axis < 3; ++axis) {
            const float workOffset =
                traceWork->sphere.use != qfalse
                    ? traceWork->sphereExtents[axis]
                    : traceWork->maxs[axis];
            const float startDistance =
                (traceWork->start[axis] -
                 brushBounds[axis]) *
                    sign -
                workOffset;
            const long double endDistanceRaw =
                ((long double)traceWork->end[axis] -
                 brushBounds[axis]) *
                    sign -
                workOffset;

            if (startDistance > 0.0f) {
                const float fractionDelta = (float)(
                    (long double)startDistance -
                    endDistanceRaw);

                if (endDistanceRaw > 0.0L) {
                    if (fractionDelta <= 0.0f ||
                        endDistanceRaw >=
                            (long double)enterEpsilon) {
                        return;
                    }
                    allSolid = qfalse;
                }

                const long double enterLimitRaw =
                    (long double)startDistance -
                    enterEpsilon;
                if ((long double)enterFraction *
                        fractionDelta <
                    enterLimitRaw) {
                    /* 0x00426b8d/0x00426de4 stores the quotient as the next
                     * enter fraction, but compares the retained x87 value. */
                    const long double enterCandidate =
                        enterLimitRaw /
                        fractionDelta;
                    enterFraction = (float)enterCandidate;
                    if ((long double)leaveFraction <=
                        enterCandidate) {
                        return;
                    }
                } else if (hitSide != NULL) {
                    continue;
                }

                axialPlane.normal[0] = 0.0f;
                axialPlane.normal[1] = 0.0f;
                axialPlane.normal[2] = 0.0f;
                axialPlane.normal[axis] = sign;
                axialSide.materialIndex =
                    brush->axialMaterialIndices[
                        boundsPass * 3 + axis];
                hitSide = &axialSide;
            } else if (endDistanceRaw > 0.0L) {
                const long double fractionDeltaRaw =
                    (long double)startDistance -
                    endDistanceRaw;

                allSolid = qfalse;
                if ((long double)leaveFraction *
                        fractionDeltaRaw <
                    startDistance) {
                    leaveFraction = (float)(
                        (long double)startDistance /
                        fractionDeltaRaw);
                    if (leaveFraction <=
                        enterFraction) {
                        return;
                    }
                }
            }
        }
    }

    const collisionBrushSide_t *side =
        brush->nonAxialSides;
    for (int32_t sideCount = brush->nonAxialSideCount;
         sideCount != 0;
         --sideCount, ++side) {
        const cplane_t *const plane = side->plane;
        float startDistance;
        long double endDistanceRaw;

        if (traceWork->sphere.use != qfalse) {
            const long double planeDistanceRaw =
                (long double)plane->dist +
                traceWork->sphere.radius;
            const long double sphereOffsetRaw =
                ((long double)plane->normal[2] *
                      traceWork->sphere.offset[2] +
                  (long double)plane->normal[0] *
                      traceWork->sphere.offset[0]) +
                (long double)plane->normal[1] *
                    traceWork->sphere.offset[1];
            long double start[3];
            vec3_t end;

            if (sphereOffsetRaw > 0.0L) {
                for (int32_t axis = 0;
                     axis < 3;
                     ++axis) {
                    start[axis] =
                        (long double)traceWork->start[axis] -
                        traceWork->sphere.offset[axis];
                    end[axis] =
                        traceWork->end[axis] -
                        traceWork->sphere.offset[axis];
                }
            } else {
                for (int32_t axis = 0;
                     axis < 3;
                     ++axis) {
                    start[axis] =
                        (long double)traceWork->start[axis] +
                        traceWork->sphere.offset[axis];
                    end[axis] =
                        traceWork->end[axis] +
                        traceWork->sphere.offset[axis];
                }
            }

            startDistance = (float)(
                (start[2] * plane->normal[2] +
                 start[1] * plane->normal[1]) +
                start[0] * plane->normal[0] -
                planeDistanceRaw);
            endDistanceRaw =
                ((long double)end[2] *
                     plane->normal[2] +
                 (long double)end[1] *
                     plane->normal[1]) +
                (long double)end[0] *
                    plane->normal[0] -
                planeDistanceRaw;
        } else {
            const float *const offset =
                traceWork->offsets[plane->signbits];
            const long double planeDistanceRaw =
                (long double)plane->dist -
                (((long double)offset[2] *
                      plane->normal[2] +
                  (long double)offset[0] *
                      plane->normal[0]) +
                 (long double)offset[1] *
                     plane->normal[1]);
            startDistance = (float)(
                ((long double)traceWork->start[1] *
                     plane->normal[1] +
                 (long double)traceWork->start[0] *
                     plane->normal[0]) +
                (long double)traceWork->start[2] *
                    plane->normal[2] -
                planeDistanceRaw);
            endDistanceRaw =
                ((long double)traceWork->end[1] *
                     plane->normal[1] +
                 (long double)traceWork->end[2] *
                     plane->normal[2]) +
                (long double)traceWork->end[0] *
                    plane->normal[0] -
                planeDistanceRaw;
        }

        if (startDistance > 0.0f) {
            const float fractionDelta = (float)(
                (long double)startDistance -
                endDistanceRaw);

            if (endDistanceRaw > 0.0L) {
                if (fractionDelta <= 0.0f ||
                    endDistanceRaw >=
                        (long double)enterEpsilon) {
                    return;
                }
                allSolid = qfalse;
            }

            const long double enterLimitRaw =
                (long double)startDistance -
                enterEpsilon;
            if ((long double)enterFraction *
                    fractionDelta <
                enterLimitRaw) {
                /* 0x00426f17/0x004270e4 follows the same retained quotient
                 * rule for arbitrary brush sides. */
                const long double enterCandidate =
                    enterLimitRaw /
                    fractionDelta;
                enterFraction = (float)enterCandidate;
                if ((long double)leaveFraction <=
                    enterCandidate) {
                    return;
                }
            } else if (hitSide != NULL) {
                continue;
            }

            hitSide = side;
        } else if (endDistanceRaw > 0.0L) {
            const long double fractionDeltaRaw =
                (long double)startDistance -
                endDistanceRaw;

            allSolid = qfalse;
            if ((long double)leaveFraction *
                    fractionDeltaRaw <
                startDistance) {
                leaveFraction = (float)(
                    (long double)startDistance /
                    fractionDeltaRaw);
                if (leaveFraction <=
                    enterFraction) {
                    return;
                }
            }
        }
    }

    traceWork->trace.contents = brush->contents;
    if (hitSide == NULL) {
        traceWork->trace.startsolid = qtrue;
        if (allSolid != qfalse) {
            traceWork->trace.allsolid = qtrue;
            traceWork->trace.fraction = 0.0f;
        }
        return;
    }

    traceWork->trace.fraction = enterFraction;
    for (int32_t axis = 0; axis < 3; ++axis) {
        traceWork->trace.normal[axis] =
            hitSide->plane->normal[axis];
    }
    dshader_t *const material =
        &cm_materials[hitSide->materialIndex];
    traceWork->trace.surfaceFlags =
        material->surfaceFlags;
    traceWork->trace.material =
        material->shader;
}
#else
void CM_TraceThroughBrush(traceWork_t *traceWork,
                          const collisionBrush_t *brush)
{
    float enterFraction = 0.0f;
    float leaveFraction = traceWork->trace.fraction;
    qboolean allSolid = 1;
    const collisionBrushSide_t *hitSide = NULL;
    cplane_t axialPlane = {{0.0f, 0.0f, 0.0f}, 0.0f, 0, 0, {0, 0}};
    collisionBrushSide_t axialSide = {&axialPlane, 0};

    for (int32_t boundsPass = 0; boundsPass <= 1; ++boundsPass) {
        const float sign = boundsPass == 0 ? -1.0f : 1.0f;
        const float *brushBounds =
            boundsPass == 0 ? brush->mins : brush->maxs;

        for (int32_t axis = 0; axis <= 2; ++axis) {
            float workOffset;

            if (traceWork->sphere.use != 0) {
                workOffset = traceWork->sphereExtents[axis];
            } else {
                workOffset = traceWork->maxs[axis];
            }

#if EMULATE_X87
            /* x87 (DLL 0x08058f0a): each distance is one 80-bit chain
             * (fld start; fsub bounds; fmul sign; fsub workOffset; fstp). */
            const float startDistance = x87f_store_f32(x87f_sub(
                x87f_mul(x87f_sub(x87f_load_f32(traceWork->start[axis]),
                                  x87f_load_f32(brushBounds[axis])),
                         x87f_load_f32(sign)),
                x87f_load_f32(workOffset)));
            const float endDistance = x87f_store_f32(x87f_sub(
                x87f_mul(x87f_sub(x87f_load_f32(traceWork->end[axis]),
                                  x87f_load_f32(brushBounds[axis])),
                         x87f_load_f32(sign)),
                x87f_load_f32(workOffset)));
#else
            const float startDistance =
                (traceWork->start[axis] - brushBounds[axis]) * sign -
                workOffset;
            const float endDistance =
                (traceWork->end[axis] - brushBounds[axis]) * sign -
                workOffset;
#endif

            if (startDistance > 0.0f) {
#if EMULATE_X87
                const float fractionDelta = x87f_store_f32(
                    x87f_sub(x87f_load_f32(startDistance),
                             x87f_load_f32(endDistance)));
#else
                const float fractionDelta = startDistance - endDistance;
#endif

                if (endDistance > 0.0f) {
                    if (fractionDelta <= 0.0f) {
                        return;
                    }
                    if (endDistance >= 0.125f) {
                        return;
                    }
                    allSolid = 0;
                }

                /* The original computes startDistance - 0.125f ONCE into a
                 * float slot and uses that same rounded value for both the
                 * compare and the divide (fld;fld 0.125f;fsubp;fstp, then
                 * fld tmp;fmul/fucompp and fld tmp;fdiv), so keep it in an
                 * explicit temp rather than writing the expression twice. */
#if EMULATE_X87
                const float enterLimit = x87f_store_f32(
                    x87f_sub(x87f_load_f32(startDistance),
                             x87f_load_f32(0.125f)));

                if (x87f_lt(x87f_mul(x87f_load_f32(enterFraction),
                                     x87f_load_f32(fractionDelta)),
                            x87f_load_f32(enterLimit))) {
                    enterFraction = x87f_store_f32(
                        x87f_div(x87f_load_f32(enterLimit),
                                 x87f_load_f32(fractionDelta)));
                    if (leaveFraction <= enterFraction) {
                        return;
                    }
                } else if (hitSide != NULL) {
                    continue;
                }
#else
                const float enterLimit = startDistance - 0.125f;

                if (enterFraction * fractionDelta < enterLimit) {
                    enterFraction = enterLimit / fractionDelta;
                    if (leaveFraction <= enterFraction) {
                        return;
                    }
                } else if (hitSide != NULL) {
                    continue;
                }
#endif

                axialPlane.normal[0] = 0.0f;
                axialPlane.normal[1] = 0.0f;
                axialPlane.normal[2] = 0.0f;
                axialPlane.normal[axis] = sign;
                axialSide.materialIndex =
                    brush->axialMaterialIndices[boundsPass * 3 + axis];
                hitSide = &axialSide;
            } else if (endDistance > 0.0f) {
#if EMULATE_X87
                const float fractionDelta = x87f_store_f32(
                    x87f_sub(x87f_load_f32(startDistance),
                             x87f_load_f32(endDistance)));

                allSolid = 0;
                if (x87f_lt(x87f_mul(x87f_load_f32(leaveFraction),
                                     x87f_load_f32(fractionDelta)),
                            x87f_load_f32(startDistance))) {
                    leaveFraction = x87f_store_f32(
                        x87f_div(x87f_load_f32(startDistance),
                                 x87f_load_f32(fractionDelta)));
                    if (leaveFraction <= enterFraction) {
                        return;
                    }
                }
#else
                const float fractionDelta = startDistance - endDistance;

                allSolid = 0;
                if (leaveFraction * fractionDelta < startDistance) {
                    leaveFraction = startDistance / fractionDelta;
                    if (leaveFraction <= enterFraction) {
                        return;
                    }
                }
#endif
            }
        }
    }

    const collisionBrushSide_t *side = brush->nonAxialSides;
    for (int32_t sideCount = brush->nonAxialSideCount; sideCount != 0;
         --sideCount, ++side) {
        const cplane_t *plane = side->plane;
        float startDistance;
        float endDistance;

        if (traceWork->sphere.use != 0) {
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

            startDistance =
                (((start[0] * plane->normal[0]) +
                  (start[1] * plane->normal[1])) +
                 (start[2] * plane->normal[2])) -
                planeDist;
            endDistance =
                (((end[0] * plane->normal[0]) +
                  (end[1] * plane->normal[1])) +
                 (end[2] * plane->normal[2])) -
                planeDist;
        } else {
            const float *offset = traceWork->offsets[plane->signbits];
#if EMULATE_X87
            /* planeDist and each distance are single 80-bit chains rounded
             * once at their float slots. */
            const float planeDist = x87f_store_f32(x87f_sub(
                x87f_load_f32(plane->dist),
                x87f_add(x87f_add(x87f_mul(x87f_load_f32(offset[0]),
                                           x87f_load_f32(plane->normal[0])),
                                  x87f_mul(x87f_load_f32(offset[1]),
                                           x87f_load_f32(plane->normal[1]))),
                         x87f_mul(x87f_load_f32(offset[2]),
                                  x87f_load_f32(plane->normal[2])))));

            startDistance = x87f_store_f32(x87f_sub(
                x87f_add(x87f_add(x87f_mul(x87f_load_f32(traceWork->start[0]),
                                           x87f_load_f32(plane->normal[0])),
                                  x87f_mul(x87f_load_f32(traceWork->start[1]),
                                           x87f_load_f32(plane->normal[1]))),
                         x87f_mul(x87f_load_f32(traceWork->start[2]),
                                  x87f_load_f32(plane->normal[2]))),
                x87f_load_f32(planeDist)));
            endDistance = x87f_store_f32(x87f_sub(
                x87f_add(x87f_add(x87f_mul(x87f_load_f32(traceWork->end[0]),
                                           x87f_load_f32(plane->normal[0])),
                                  x87f_mul(x87f_load_f32(traceWork->end[1]),
                                           x87f_load_f32(plane->normal[1]))),
                         x87f_mul(x87f_load_f32(traceWork->end[2]),
                                  x87f_load_f32(plane->normal[2]))),
                x87f_load_f32(planeDist)));
#else
            const float planeDist =
                plane->dist -
                (((offset[0] * plane->normal[0]) +
                  (offset[1] * plane->normal[1])) +
                 (offset[2] * plane->normal[2]));

            startDistance =
                (((traceWork->start[0] * plane->normal[0]) +
                  (traceWork->start[1] * plane->normal[1])) +
                 (traceWork->start[2] * plane->normal[2])) -
                planeDist;
            endDistance =
                (((traceWork->end[0] * plane->normal[0]) +
                  (traceWork->end[1] * plane->normal[1])) +
                 (traceWork->end[2] * plane->normal[2])) -
                planeDist;
#endif
        }

        if (startDistance > 0.0f) {
#if EMULATE_X87
            const float fractionDelta = x87f_store_f32(
                x87f_sub(x87f_load_f32(startDistance),
                         x87f_load_f32(endDistance)));
#else
            const float fractionDelta = startDistance - endDistance;
#endif

            if (endDistance > 0.0f) {
                if (fractionDelta <= 0.0f) {
                    return;
                }
                if (endDistance >= 0.125f) {
                    return;
                }
                allSolid = 0;
            }

            /* As in the axial pass: startDistance - 0.125f is computed once
             * into a float slot and reused by both the compare and the divide. */
#if EMULATE_X87
            const float enterLimit = x87f_store_f32(
                x87f_sub(x87f_load_f32(startDistance), x87f_load_f32(0.125f)));

            if (x87f_lt(x87f_mul(x87f_load_f32(enterFraction),
                                 x87f_load_f32(fractionDelta)),
                        x87f_load_f32(enterLimit))) {
                enterFraction = x87f_store_f32(
                    x87f_div(x87f_load_f32(enterLimit),
                             x87f_load_f32(fractionDelta)));
                if (leaveFraction <= enterFraction) {
                    return;
                }
            } else if (hitSide != NULL) {
                continue;
            }
#else
            const float enterLimit = startDistance - 0.125f;

            if (enterFraction * fractionDelta < enterLimit) {
                enterFraction = enterLimit / fractionDelta;
                if (leaveFraction <= enterFraction) {
                    return;
                }
            } else if (hitSide != NULL) {
                continue;
            }
#endif

            hitSide = side;
        } else if (endDistance > 0.0f) {
#if EMULATE_X87
            const float fractionDelta = x87f_store_f32(
                x87f_sub(x87f_load_f32(startDistance),
                         x87f_load_f32(endDistance)));

            allSolid = 0;
            if (x87f_lt(x87f_mul(x87f_load_f32(leaveFraction),
                                 x87f_load_f32(fractionDelta)),
                        x87f_load_f32(startDistance))) {
                leaveFraction = x87f_store_f32(
                    x87f_div(x87f_load_f32(startDistance),
                             x87f_load_f32(fractionDelta)));
                if (leaveFraction <= enterFraction) {
                    return;
                }
            }
#else
            const float fractionDelta = startDistance - endDistance;

            allSolid = 0;
            if (leaveFraction * fractionDelta < startDistance) {
                leaveFraction = startDistance / fractionDelta;
                if (leaveFraction <= enterFraction) {
                    return;
                }
            }
#endif
        }
    }

    traceWork->trace.contents = brush->contents;
    if (hitSide == NULL) {
        traceWork->trace.startsolid = 1;
        if (allSolid != 0) {
            traceWork->trace.allsolid = 1;
            traceWork->trace.fraction = 0.0f;
        }
        return;
    }

    traceWork->trace.fraction = enterFraction;
    traceWork->trace.normal[0] = hitSide->plane->normal[0];
    traceWork->trace.normal[1] = hitSide->plane->normal[1];
    traceWork->trace.normal[2] = hitSide->plane->normal[2];
    traceWork->trace.surfaceFlags =
        cm_materials[hitSide->materialIndex].surfaceFlags;
    traceWork->trace.material =
        cm_materials[hitSide->materialIndex].shader;
}
#endif
