#include "collision_patch_trace.h"

#include "compat/coduo_x87emu.h"
#include "qcommon/qcommon_runtime_types.h"

#include <math.h>
#include <string.h>

extern cvar_t *cm_playerCurveClip;

/*
 * Complete curved-patch position, trace, and sight cluster:
 *
 *   CoDUOMP.exe  0x00420390..0x0042179b
 *   coduo_lnxded 0x0804f85b..0x08051405
 *
 * Function ordering differs between the two binaries, but the six operation
 * contracts agree. Platform bodies retain their proven x87 graphs and spills.
 */
#if defined(WINDOWS_BEHAVIOR)
/* NOT_FROM_ORIGINAL_SOURCE: precompute the two invariant capsule support points used by every tested patch plane. */
static qboolean coduomp_position_test_sphere_patch(const traceWork_t *traceWork, const patchCollide_t *patchCollide)
{
    if (patchCollide->numFacets <= 0)
        return qfalse;

    const vec3_t negativeSupportPoint = {
        traceWork->start[0] - traceWork->sphere.offset[0],
        traceWork->start[1] - traceWork->sphere.offset[1],
        traceWork->start[2] - traceWork->sphere.offset[2]
    };
    const vec3_t positiveSupportPoint = {
        traceWork->start[0] + traceWork->sphere.offset[0],
        traceWork->start[1] + traceWork->sphere.offset[1],
        traceWork->start[2] + traceWork->sphere.offset[2]
    };
    const float *const sphereOffset = traceWork->sphere.offset;
    const patchPlane_t *const planes = patchCollide->planes;
    const facet_t *facet = patchCollide->facets;
    const facet_t *const facetEnd = facet + patchCollide->numFacets;

    for (; facet != facetEnd; ++facet) {
        qboolean outside = qfalse;

        for (int32_t facetPlaneIndex = -1; facetPlaneIndex < facet->numBorders; ++facetPlaneIndex) {
            const int32_t planeIndex = facetPlaneIndex < 0 ? facet->surfacePlane : facet->borderPlanes[facetPlaneIndex];
            const qboolean inward = facetPlaneIndex < 0 ? qfalse : facet->borderInward[facetPlaneIndex];
            const patchPlane_t *const sourcePlane = &planes[planeIndex];
            float planeX = sourcePlane->normal[0];
            float planeY = sourcePlane->normal[1];
            float planeZ = sourcePlane->normal[2];
            float planeDist = sourcePlane->dist;

            if (inward != qfalse) {
                planeX = -planeX;
                planeY = -planeY;
                planeZ = -planeZ;
                planeDist = -planeDist;
            }

            const long double offsetDistance = ((long double)planeX * sphereOffset[0] +
                                                (long double)planeZ * sphereOffset[2]) +
                                               (long double)planeY * sphereOffset[1];
            const float *const supportPoint = offsetDistance > 0.0L ? negativeSupportPoint : positiveSupportPoint;
            const long double distance = ((long double)planeZ * supportPoint[2] + (long double)planeY * supportPoint[1]) +
                                         (long double)planeX * supportPoint[0] -
                                         ((long double)planeDist + traceWork->sphere.radius);

            if (distance > 0.0L) {
                outside = qtrue;
                break;
            }
        }

        if (outside == qfalse)
            return qtrue;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x00421510..0x0042179b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00421510_0042179b.mcode.
 * Name: exact same-module Mac symbol CM_PositionTestInPatchCollide. A point
 * trace has no volume to test. Box and capsule traces expand every facet
 * plane by their support point, then accept the first facet whose surface
 * and inward-facing border planes all contain the trace start. */
qboolean CM_PositionTestInPatchCollide(
    traceWork_t *traceWork,
    const patchCollide_t *patchCollide)
{
    if (traceWork->isPoint != qfalse)
        return qfalse;

    if (traceWork->sphere.use != qfalse)
        return coduomp_position_test_sphere_patch(traceWork, patchCollide);

    for (int32_t facetIndex = 0;
         facetIndex < patchCollide->numFacets;
         ++facetIndex) {
        const facet_t *const facet =
            &patchCollide->facets[facetIndex];
        qboolean outside = qfalse;

        for (int32_t facetPlaneIndex = -1;
             facetPlaneIndex <
                 facet->numBorders;
             ++facetPlaneIndex) {
            int32_t planeIndex;
            qboolean inward;
            if (facetPlaneIndex < 0) {
                planeIndex = facet->surfacePlane;
                inward = qfalse;
            } else {
                planeIndex =
                    facet->borderPlanes[facetPlaneIndex];
                inward =
                    facet->borderInward[facetPlaneIndex];
            }

            const patchPlane_t *const sourcePlane =
                &patchCollide->planes[planeIndex];
            vec4_t plane;
            if (inward != qfalse) {
                plane[0] = -sourcePlane->normal[0];
                plane[1] = -sourcePlane->normal[1];
                plane[2] = -sourcePlane->normal[2];
                plane[3] = -sourcePlane->dist;
            } else {
                plane[0] = sourcePlane->normal[0];
                plane[1] = sourcePlane->normal[1];
                plane[2] = sourcePlane->normal[2];
                plane[3] = sourcePlane->dist;
            }

            long double distance;
            if (traceWork->sphere.use != qfalse) {
                const long double offsetDistance =
                    ((long double)plane[0] *
                         traceWork->sphere.offset[0] +
                     (long double)plane[2] *
                         traceWork->sphere.offset[2]) +
                    (long double)plane[1] *
                        traceWork->sphere.offset[1];
                vec3_t supportPoint;
                for (int32_t axis = 0; axis < 3;
                     ++axis) {
                    supportPoint[axis] =
                        offsetDistance > 0.0L
                            ? traceWork->start[axis] -
                                  traceWork->sphere.offset[axis]
                            : traceWork->start[axis] +
                                  traceWork->sphere.offset[axis];
                }
                distance =
                    ((long double)plane[2] *
                         supportPoint[2] +
                     (long double)plane[1] *
                         supportPoint[1]) +
                    (long double)plane[0] *
                        supportPoint[0] -
                    ((long double)plane[3] +
                     traceWork->sphere.radius);
            } else {
                const vec3_t supportOffset = {
                    traceWork
                        ->offsets[sourcePlane->signbits][0],
                    traceWork
                        ->offsets[sourcePlane->signbits][1],
                    traceWork
                        ->offsets[sourcePlane->signbits][2]
                };
                const long double supportDistance =
                    ((long double)plane[2] *
                         supportOffset[2] +
                     (long double)plane[1] *
                         supportOffset[1]) +
                    (long double)plane[0] *
                        supportOffset[0];
                const long double expandedDistance =
                    facetPlaneIndex < 0
                        ? (long double)plane[3] -
                              supportDistance
                        : (long double)plane[3] +
                              fabsl(supportDistance);
                distance =
                    ((long double)plane[2] *
                         traceWork->start[2] +
                     (long double)plane[1] *
                         traceWork->start[1]) +
                    (long double)plane[0] *
                        traceWork->start[0] -
                    expandedDistance;
            }

            if (distance > 0.0L) {
                outside = qtrue;
                break;
            }
        }

        if (outside == qfalse)
            return qtrue;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x00420390..0x00420709.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00420390_00420709.mcode.
 * Role name: point-trace specialization called by
 * CM_TraceThroughPatchCollide. It precomputes every patch-plane crossing,
 * then accepts a surface crossing only when all oriented facet borders
 * contain the same point on the segment. */
void CM_TracePointThroughPatchCollide(
    traceWork_t *traceWork,
    const patchCollide_t *patchCollide)
{
    if (cm_playerCurveClip->integer == 0 ||
        traceWork->isPoint == qfalse) {
        return;
    }

    float planeIntersections[CM_PATCH_PLANE_LIMIT];
    qboolean planeFrontFacing[CM_PATCH_PLANE_LIMIT];

    for (int32_t planeIndex = 0;
         planeIndex < patchCollide->numPlanes;
         ++planeIndex) {
        const patchPlane_t *const plane =
            &patchCollide->planes[planeIndex];
        const vec3_t supportOffset = {
            traceWork->offsets[plane->signbits][0],
            traceWork->offsets[plane->signbits][1],
            traceWork->offsets[plane->signbits][2]
        };
        const long double offsetDistance =
            ((long double)plane->normal[2] *
                 supportOffset[2] +
             (long double)plane->normal[0] *
                 supportOffset[0]) +
            (long double)plane->normal[1] *
                supportOffset[1];
        const float startDistance = (float)(
            ((long double)plane->normal[0] *
                 traceWork->start[0] +
             (long double)plane->normal[1] *
                 traceWork->start[1]) +
            (long double)plane->normal[2] *
                traceWork->start[2] -
            plane->dist +
            offsetDistance);
        const long double endDistance =
            ((long double)plane->normal[2] *
                 traceWork->end[2] +
             (long double)plane->normal[0] *
                 traceWork->end[0]) +
            (long double)plane->normal[1] *
                traceWork->end[1] -
            plane->dist +
            offsetDistance;

        planeFrontFacing[planeIndex] =
            startDistance > 0.0f ||
            isunordered(startDistance, 0.0f);
        if ((long double)startDistance ==
            endDistance) {
            planeIntersections[planeIndex] =
                99999.0f;
        } else {
            const long double intersection =
                (long double)startDistance /
                ((long double)startDistance -
                 endDistance);
            planeIntersections[planeIndex] =
                intersection > 0.0L ||
                        isunordered(intersection, 0.0L)
                    ? (float)intersection
                    : 99999.0f;
        }
    }

    for (int32_t facetIndex = 0;
         facetIndex < patchCollide->numFacets;
         ++facetIndex) {
        const facet_t *const facet =
            &patchCollide->facets[facetIndex];
        const int32_t surfacePlaneIndex =
            facet->surfacePlane;
        if (planeFrontFacing[surfacePlaneIndex] ==
            qfalse) {
            continue;
        }

        const float surfaceIntersection =
            planeIntersections[surfacePlaneIndex];
        if (!(surfaceIntersection > 0.0f ||
              isunordered(surfaceIntersection, 0.0f)) ||
            surfaceIntersection >
                traceWork->trace.fraction) {
            continue;
        }

        qboolean inside = qtrue;
        for (int32_t borderIndex = 0;
             borderIndex <
                 facet->numBorders;
             ++borderIndex) {
            const int32_t borderPlaneIndex =
                facet->borderPlanes[borderIndex];
            const qboolean facesInward =
                planeFrontFacing[borderPlaneIndex] ^
                facet->borderInward[borderIndex];
            const float borderIntersection =
                planeIntersections[borderPlaneIndex];
            if ((facesInward != qfalse &&
                 borderIntersection >
                     surfaceIntersection) ||
                (facesInward == qfalse &&
                 borderIntersection <
                     surfaceIntersection)) {
                inside = qfalse;
                break;
            }
        }
        if (inside == qfalse)
            continue;

        const patchPlane_t *const surfacePlane =
            &patchCollide
                 ->planes[surfacePlaneIndex];
        const vec3_t supportOffset = {
            traceWork
                ->offsets[surfacePlane->signbits][0],
            traceWork
                ->offsets[surfacePlane->signbits][1],
            traceWork
                ->offsets[surfacePlane->signbits][2]
        };
        const long double offsetDistance =
            ((long double)surfacePlane->normal[2] *
                 supportOffset[2] +
             (long double)surfacePlane->normal[0] *
                 supportOffset[0]) +
            (long double)surfacePlane->normal[1] *
                supportOffset[1];
        const long double startDistance =
            ((long double)surfacePlane->normal[1] *
                 traceWork->start[1] +
             (long double)surfacePlane->normal[2] *
                 traceWork->start[2]) +
            (long double)surfacePlane->normal[0] *
                traceWork->start[0] -
            surfacePlane->dist +
            offsetDistance;
        const long double endDistance =
            ((long double)surfacePlane->normal[0] *
                 traceWork->end[0] +
             (long double)surfacePlane->normal[1] *
                 traceWork->end[1]) +
            (long double)surfacePlane->normal[2] *
                traceWork->end[2] -
            surfacePlane->dist +
            offsetDistance;
        const long double fraction =
            (startDistance - 0.125f) /
            (startDistance - endDistance);

        traceWork->trace.fraction =
            fraction >= 0.0L ||
                    isunordered(fraction, 0.0L)
                ? (float)fraction
                : 0.0f;
        traceWork->trace.normal[0] =
            surfacePlane->normal[0];
        traceWork->trace.normal[1] =
            surfacePlane->normal[1];
        traceWork->trace.normal[2] =
            surfacePlane->normal[2];
    }
}

/* Source: CoDUOMP.exe 0x00420710..0x00420a62.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00420710_00420a62.mcode.
 * Role name: point-sight specialization called by
 * CM_SightTraceThroughPatchCollide. Its cached plane/facet test mirrors the
 * point trace, but it reports obstruction without modifying trace output. */
qboolean CM_SightTracePointThroughPatchCollide(
    const traceWork_t *traceWork,
    const patchCollide_t *patchCollide)
{
    if (cm_playerCurveClip->integer == 0 ||
        traceWork->isPoint == qfalse) {
        return qtrue;
    }

    float planeIntersections[CM_PATCH_PLANE_LIMIT];
    qboolean planeFrontFacing[CM_PATCH_PLANE_LIMIT];

    for (int32_t planeIndex = 0;
         planeIndex < patchCollide->numPlanes;
         ++planeIndex) {
        const patchPlane_t *const plane =
            &patchCollide->planes[planeIndex];
        const vec3_t supportOffset = {
            traceWork->offsets[plane->signbits][0],
            traceWork->offsets[plane->signbits][1],
            traceWork->offsets[plane->signbits][2]
        };
        const long double offsetDistance =
            ((long double)plane->normal[2] *
                 supportOffset[2] +
             (long double)plane->normal[0] *
                 supportOffset[0]) +
            (long double)plane->normal[1] *
                supportOffset[1];
        const float startDistance = (float)(
            ((long double)plane->normal[0] *
                 traceWork->start[0] +
             (long double)plane->normal[1] *
                 traceWork->start[1]) +
            (long double)plane->normal[2] *
                traceWork->start[2] -
            plane->dist +
            offsetDistance);
        const long double endDistance =
            ((long double)plane->normal[2] *
                 traceWork->end[2] +
             (long double)plane->normal[0] *
                 traceWork->end[0]) +
            (long double)plane->normal[1] *
                traceWork->end[1] -
            plane->dist +
            offsetDistance;

        planeFrontFacing[planeIndex] =
            startDistance > 0.0f ||
            isunordered(startDistance, 0.0f);
        if ((long double)startDistance ==
            endDistance) {
            planeIntersections[planeIndex] =
                99999.0f;
        } else {
            const long double intersection =
                (long double)startDistance /
                ((long double)startDistance -
                 endDistance);
            planeIntersections[planeIndex] =
                intersection > 0.0L ||
                        isunordered(intersection, 0.0L)
                    ? (float)intersection
                    : 99999.0f;
        }
    }

    for (int32_t facetIndex = 0;
         facetIndex < patchCollide->numFacets;
         ++facetIndex) {
        const facet_t *const facet =
            &patchCollide->facets[facetIndex];
        const int32_t surfacePlaneIndex =
            facet->surfacePlane;
        if (planeFrontFacing[surfacePlaneIndex] ==
            qfalse) {
            continue;
        }

        const float surfaceIntersection =
            planeIntersections[surfacePlaneIndex];
        if (!(surfaceIntersection > 0.0f ||
              isunordered(surfaceIntersection, 0.0f)))
            continue;

        qboolean inside = qtrue;
        for (int32_t borderIndex = 0;
             borderIndex <
                 facet->numBorders;
             ++borderIndex) {
            const int32_t borderPlaneIndex =
                facet->borderPlanes[borderIndex];
            const qboolean facesInward =
                planeFrontFacing[borderPlaneIndex] ^
                facet->borderInward[borderIndex];
            const float borderIntersection =
                planeIntersections[borderPlaneIndex];
            if ((facesInward != qfalse &&
                 borderIntersection >
                     surfaceIntersection) ||
                (facesInward == qfalse &&
                 borderIntersection <
                     surfaceIntersection)) {
                inside = qfalse;
                break;
            }
        }
        if (inside == qfalse)
            continue;

        const patchPlane_t *const surfacePlane =
            &patchCollide
                 ->planes[surfacePlaneIndex];
        const vec3_t supportOffset = {
            traceWork
                ->offsets[surfacePlane->signbits][0],
            traceWork
                ->offsets[surfacePlane->signbits][1],
            traceWork
                ->offsets[surfacePlane->signbits][2]
        };
        const long double offsetDistance =
            ((long double)surfacePlane->normal[2] *
                 supportOffset[2] +
             (long double)surfacePlane->normal[0] *
                 supportOffset[0]) +
            (long double)surfacePlane->normal[1] *
                supportOffset[1];
        const float startDistance = (float)(
            ((long double)surfacePlane->normal[1] *
                 traceWork->start[1] +
             (long double)surfacePlane->normal[2] *
                 traceWork->start[2]) +
            (long double)surfacePlane->normal[0] *
                traceWork->start[0] -
            surfacePlane->dist +
            offsetDistance);
        const long double endDistance =
            ((long double)surfacePlane->normal[0] *
                 traceWork->end[0] +
             (long double)surfacePlane->normal[1] *
                 traceWork->end[1]) +
            (long double)surfacePlane->normal[2] *
                traceWork->end[2] -
            surfacePlane->dist +
            offsetDistance;
        const long double distanceDelta =
            (long double)startDistance -
            endDistance;
        if (!(distanceDelta > 0.0L))
            continue;

        const long double fraction =
            ((long double)startDistance - 0.125f) /
            distanceDelta;
        if (fraction < 1.0L)
            return qfalse;
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x00420ba0..0x00421088.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00420ba0_00421088.mcode.
 * Name: exact same-module Mac symbol CM_TraceThroughPatchCollide. Each
 * non-point facet is clipped as an expanded convex volume. The final border
 * is its back plane, so entering through that plane is not a visible surface
 * collision. */
void CM_TraceThroughPatchCollide(
    traceWork_t *traceWork,
    const patchCollide_t *patchCollide)
{
    if (traceWork->isPoint != qfalse) {
        CM_TracePointThroughPatchCollide(
            traceWork, patchCollide);
        return;
    }

    for (int32_t facetIndex = 0;
         facetIndex < patchCollide->numFacets;
         ++facetIndex) {
        const facet_t *const facet =
            &patchCollide->facets[facetIndex];
        float enterFraction = -1.0f;
        float leaveFraction =
            traceWork->trace.fraction;
        int32_t hitBorder = -1;
        vec3_t hitNormal;
        qboolean facetAccepted = qtrue;

        for (int32_t facetPlaneIndex = -1;
             facetPlaneIndex <
                 facet->numBorders;
             ++facetPlaneIndex) {
            int32_t planeIndex;
            qboolean inward;
            if (facetPlaneIndex < 0) {
                planeIndex = facet->surfacePlane;
                inward = qfalse;
            } else {
                planeIndex =
                    facet->borderPlanes[facetPlaneIndex];
                inward =
                    facet->borderInward[facetPlaneIndex];
            }

            const patchPlane_t *const sourcePlane =
                &patchCollide->planes[planeIndex];
            vec4_t plane;
            if (inward != qfalse) {
                plane[0] = -sourcePlane->normal[0];
                plane[1] = -sourcePlane->normal[1];
                plane[2] = -sourcePlane->normal[2];
                plane[3] = -sourcePlane->dist;
            } else {
                plane[0] = sourcePlane->normal[0];
                plane[1] = sourcePlane->normal[1];
                plane[2] = sourcePlane->normal[2];
                plane[3] = sourcePlane->dist;
            }

            vec3_t expandedStart;
            vec3_t expandedEnd;
            if (traceWork->sphere.use != qfalse) {
                plane[3] =
                    (float)((long double)plane[3] +
                            traceWork->sphere.radius);
                const long double offsetDistance =
                    ((long double)plane[0] *
                         traceWork->sphere.offset[0] +
                     (long double)plane[2] *
                         traceWork->sphere.offset[2]) +
                    (long double)plane[1] *
                        traceWork->sphere.offset[1];
                for (int32_t axis = 0; axis < 3;
                     ++axis) {
                    if (offsetDistance > 0.0L) {
                        expandedStart[axis] =
                            traceWork->start[axis] -
                            traceWork->sphere.offset[axis];
                        expandedEnd[axis] =
                            traceWork->end[axis] -
                            traceWork->sphere.offset[axis];
                    } else {
                        expandedStart[axis] =
                            traceWork->start[axis] +
                            traceWork->sphere.offset[axis];
                        expandedEnd[axis] =
                            traceWork->end[axis] +
                            traceWork->sphere.offset[axis];
                    }
                }
            } else {
                const vec3_t supportOffset = {
                    traceWork
                        ->offsets[sourcePlane->signbits][0],
                    traceWork
                        ->offsets[sourcePlane->signbits][1],
                    traceWork
                        ->offsets[sourcePlane->signbits][2]
                };
                const long double supportDistance =
                    ((long double)plane[2] *
                         supportOffset[2] +
                     (long double)plane[1] *
                         supportOffset[1]) +
                    (long double)plane[0] *
                        supportOffset[0];
                plane[3] = (float)(
                    facetPlaneIndex < 0
                        ? (long double)plane[3] -
                              supportDistance
                        : (long double)plane[3] +
                              fabsl(supportDistance));
                memcpy(expandedStart, traceWork->start,
                       sizeof(expandedStart));
                memcpy(expandedEnd, traceWork->end,
                       sizeof(expandedEnd));
            }

            qboolean hit;
            if (CM_CheckFacetPlane(
                    plane, expandedStart, expandedEnd,
                    &enterFraction, &leaveFraction,
                    &hit) == qfalse) {
                facetAccepted = qfalse;
                break;
            }
            if (hit != qfalse) {
                hitNormal[0] = plane[0];
                hitNormal[1] = plane[1];
                hitNormal[2] = plane[2];
                if (facetPlaneIndex >= 0)
                    hitBorder = facetPlaneIndex;
            }
        }

        if (facetAccepted == qfalse ||
            hitBorder ==
                facet->numBorders - 1 ||
            !(enterFraction < leaveFraction) ||
            enterFraction < 0.0f ||
            !(enterFraction <
              traceWork->trace.fraction)) {
            continue;
        }

        traceWork->trace.fraction =
            enterFraction;
        traceWork->trace.normal[0] =
            hitNormal[0];
        traceWork->trace.normal[1] =
            hitNormal[1];
        traceWork->trace.normal[2] =
            hitNormal[2];
    }
}

/* Source: CoDUOMP.exe 0x00421090..0x00421509.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00421090_00421509.mcode.
 * Name: exact same-module Mac symbol CM_SightTraceThroughPatchCollide. This
 * uses the same expanded facet clipping as the full trace, but returns as
 * soon as an accepted front/side crossing obstructs the segment. */
qboolean CM_SightTraceThroughPatchCollide(
    const traceWork_t *traceWork,
    const patchCollide_t *patchCollide)
{
    if (traceWork->isPoint != qfalse) {
        return CM_SightTracePointThroughPatchCollide(
            traceWork, patchCollide);
    }

    for (int32_t facetIndex = 0;
         facetIndex < patchCollide->numFacets;
         ++facetIndex) {
        const facet_t *const facet =
            &patchCollide->facets[facetIndex];
        float enterFraction = -1.0f;
        float leaveFraction = 1.0f;
        int32_t hitBorder = -1;
        qboolean facetAccepted = qtrue;

        for (int32_t facetPlaneIndex = -1;
             facetPlaneIndex <
                 facet->numBorders;
             ++facetPlaneIndex) {
            int32_t planeIndex;
            qboolean inward;
            if (facetPlaneIndex < 0) {
                planeIndex = facet->surfacePlane;
                inward = qfalse;
            } else {
                planeIndex =
                    facet->borderPlanes[facetPlaneIndex];
                inward =
                    facet->borderInward[facetPlaneIndex];
            }

            const patchPlane_t *const sourcePlane =
                &patchCollide->planes[planeIndex];
            vec4_t plane;
            if (inward != qfalse) {
                plane[0] = -sourcePlane->normal[0];
                plane[1] = -sourcePlane->normal[1];
                plane[2] = -sourcePlane->normal[2];
                plane[3] = -sourcePlane->dist;
            } else {
                plane[0] = sourcePlane->normal[0];
                plane[1] = sourcePlane->normal[1];
                plane[2] = sourcePlane->normal[2];
                plane[3] = sourcePlane->dist;
            }

            vec3_t expandedStart;
            vec3_t expandedEnd;
            if (traceWork->sphere.use != qfalse) {
                plane[3] =
                    (float)((long double)plane[3] +
                            traceWork->sphere.radius);
                const long double offsetDistance =
                    ((long double)plane[0] *
                         traceWork->sphere.offset[0] +
                     (long double)plane[2] *
                         traceWork->sphere.offset[2]) +
                    (long double)plane[1] *
                        traceWork->sphere.offset[1];
                for (int32_t axis = 0; axis < 3;
                     ++axis) {
                    if (offsetDistance > 0.0L) {
                        expandedStart[axis] =
                            traceWork->start[axis] -
                            traceWork->sphere.offset[axis];
                        expandedEnd[axis] =
                            traceWork->end[axis] -
                            traceWork->sphere.offset[axis];
                    } else {
                        expandedStart[axis] =
                            traceWork->start[axis] +
                            traceWork->sphere.offset[axis];
                        expandedEnd[axis] =
                            traceWork->end[axis] +
                            traceWork->sphere.offset[axis];
                    }
                }
            } else {
                const vec3_t supportOffset = {
                    traceWork
                        ->offsets[sourcePlane->signbits][0],
                    traceWork
                        ->offsets[sourcePlane->signbits][1],
                    traceWork
                        ->offsets[sourcePlane->signbits][2]
                };
                const long double supportDistance =
                    ((long double)plane[2] *
                         supportOffset[2] +
                     (long double)plane[1] *
                         supportOffset[1]) +
                    (long double)plane[0] *
                        supportOffset[0];
                plane[3] = (float)(
                    facetPlaneIndex < 0
                        ? (long double)plane[3] -
                              supportDistance
                        : (long double)plane[3] +
                              fabsl(supportDistance));
                memcpy(expandedStart, traceWork->start,
                       sizeof(expandedStart));
                memcpy(expandedEnd, traceWork->end,
                       sizeof(expandedEnd));
            }

            qboolean hit;
            if (CM_CheckFacetPlane(
                    plane, expandedStart, expandedEnd,
                    &enterFraction, &leaveFraction,
                    &hit) == qfalse) {
                facetAccepted = qfalse;
                break;
            }
            if (hit != qfalse &&
                facetPlaneIndex >= 0) {
                hitBorder = facetPlaneIndex;
            }
        }

        if (facetAccepted != qfalse &&
            hitBorder !=
                facet->numBorders - 1 &&
            enterFraction < leaveFraction &&
            enterFraction >= 0.0f) {
            return qfalse;
        }
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x00420a70..0x00420b9b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00420a70_00420b9b.mcode.
 * Name: exact same-module Mac symbol CM_CheckFacetPlane. The routine clips
 * one segment against a facet plane, tightening its entering or leaving
 * fraction with the original 0.125-unit surface epsilon. */
qboolean CM_CheckFacetPlane(
    const vec4_t plane, const vec3_t start,
    const vec3_t end, float *enterFraction,
    float *leaveFraction, qboolean *hit)
{
    *hit = qfalse;

    const float startDistance = (float)(
        ((long double)plane[2] * start[2] +
         (long double)plane[1] * start[1]) +
        (long double)plane[0] * start[0] -
        plane[3]);
    const float endDistance = (float)(
        ((long double)plane[2] * end[2] +
         (long double)plane[1] * end[1]) +
        (long double)plane[0] * end[0] -
        plane[3]);

    if (startDistance > 0.0f &&
        (endDistance >= 0.125f ||
         endDistance >= startDistance)) {
        return qfalse;
    }

    if (startDistance <= 0.0f &&
        endDistance <= 0.0f) {
        return qtrue;
    }

    if (startDistance > endDistance) {
        long double fraction =
            ((long double)startDistance - 0.125f) /
            ((long double)startDistance - endDistance);
        if (fraction < 0.0L)
            fraction = 0.0L;
        if (fraction > *enterFraction) {
            *enterFraction = (float)fraction;
            *hit = qtrue;
        }
    } else {
        long double fraction =
            ((long double)startDistance + 0.125f) /
            ((long double)startDistance - endDistance);
        if (fraction > 1.0L)
            fraction = 1.0L;
        if (fraction < *leaveFraction)
            *leaveFraction = (float)fraction;
    }

    return qtrue;
}

#else

/* NOT_FROM_ORIGINAL_SOURCE: Linux-behavior counterpart that precomputes the invariant capsule support points. */
static qboolean coduomp_linux_position_test_sphere_patch(const traceWork_t *traceWork, const patchCollide_t *patchCollide)
{
    const int32_t facetCount = patchCollide->numFacets;
    if (facetCount <= 0)
        return qfalse;

    const float sphereRadius = traceWork->sphere.radius;
    const vec3_t negativeSupportPoint = {
        traceWork->start[0] - traceWork->sphere.offset[0],
        traceWork->start[1] - traceWork->sphere.offset[1],
        traceWork->start[2] - traceWork->sphere.offset[2]
    };
    const vec3_t positiveSupportPoint = {
        traceWork->start[0] + traceWork->sphere.offset[0],
        traceWork->start[1] + traceWork->sphere.offset[1],
        traceWork->start[2] + traceWork->sphere.offset[2]
    };
    const float *const sphereOffset = traceWork->sphere.offset;
    const patchPlane_t *const planes = patchCollide->planes;
    const facet_t *facet = patchCollide->facets;
    const facet_t *const facetEnd = facet + facetCount;

    for (; facet != facetEnd; ++facet) {
        const patchPlane_t *plane = &planes[facet->surfacePlane];
        vec3_t planeNormal = {plane->normal[0], plane->normal[1], plane->normal[2]};
        float planeDist = plane->dist;
        planeDist += sphereRadius;

        const float sphereSupportDot = ((planeNormal[0] * sphereOffset[0]) + (planeNormal[1] * sphereOffset[1])) +
                                       (planeNormal[2] * sphereOffset[2]);
        const float *testPoint = sphereSupportDot > 0.0f ? negativeSupportPoint : positiveSupportPoint;

        if (!((((planeNormal[0] * testPoint[0]) + (planeNormal[1] * testPoint[1])) +
               (planeNormal[2] * testPoint[2])) - planeDist > 0.0f)) {
            int32_t childIndex;

            for (childIndex = 0; childIndex < facet->numBorders; ++childIndex) {
                plane = &planes[facet->borderPlanes[childIndex]];

                if (facet->borderInward[childIndex] == 0) {
                    planeNormal[0] = plane->normal[0];
                    planeNormal[1] = plane->normal[1];
                    planeNormal[2] = plane->normal[2];
                    planeDist = plane->dist;
                } else {
                    planeNormal[0] = -plane->normal[0];
                    planeNormal[1] = -plane->normal[1];
                    planeNormal[2] = -plane->normal[2];
                    planeDist = -plane->dist;
                }

                planeDist += sphereRadius;
                const float borderSupportDot = ((planeNormal[0] * sphereOffset[0]) + (planeNormal[1] * sphereOffset[1])) +
                                               (planeNormal[2] * sphereOffset[2]);
                testPoint = borderSupportDot > 0.0f ? negativeSupportPoint : positiveSupportPoint;

                if ((((planeNormal[0] * testPoint[0]) + (planeNormal[1] * testPoint[1])) +
                     (planeNormal[2] * testPoint[2])) - planeDist > 0.0f) {
                    break;
                }
            }

            if (childIndex >= facet->numBorders)
                return qtrue;
        }
    }

    return qfalse;
}


qboolean CM_PositionTestInPatchCollide(
    traceWork_t *traceWork,
    const patchCollide_t *patchCollide)
{
    if (traceWork->isPoint != 0) {
        return 0;
    }

    if (traceWork->sphere.use != 0)
        return coduomp_linux_position_test_sphere_patch(traceWork, patchCollide);

    const facet_t *facet = patchCollide->facets;
    for (int32_t facetIndex = 0; facetIndex < patchCollide->numFacets;
         ++facetIndex, ++facet) {
        const patchPlane_t *plane =
            &patchCollide->planes[facet->surfacePlane];

        vec3_t planeNormal = {plane->normal[0], plane->normal[1],
                              plane->normal[2]};
        float planeDist = plane->dist;
        vec3_t testPoint;

        if (traceWork->sphere.use != 0) {
            planeDist += traceWork->sphere.radius;

            const float sphereOffset =
                ((planeNormal[0] * traceWork->sphere.offset[0]) +
                 (planeNormal[1] * traceWork->sphere.offset[1])) +
                (planeNormal[2] * traceWork->sphere.offset[2]);

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
        } else {
            const float *offset = traceWork->offsets[plane->signbits];
            const float planeOffset =
                ((offset[0] * planeNormal[0]) +
                 (offset[1] * planeNormal[1])) +
                (offset[2] * planeNormal[2]);

            planeDist -= planeOffset;
            testPoint[0] = traceWork->start[0];
            testPoint[1] = traceWork->start[1];
            testPoint[2] = traceWork->start[2];
        }

        /* 0x8051188..0x80511aa: the root distance is compared against zero
         * in the 80-bit register chain with no intermediate float store. */
        if (!((((planeNormal[0] * testPoint[0]) +
                (planeNormal[1] * testPoint[1])) +
               (planeNormal[2] * testPoint[2])) -
                  planeDist >
              0.0f)) {
            int32_t childIndex;

            for (childIndex = 0; childIndex < facet->numBorders;
                 ++childIndex) {
                plane =
                    &patchCollide->planes[facet->borderPlanes[childIndex]];

                if (facet->borderInward[childIndex] == 0) {
                    planeNormal[0] = plane->normal[0];
                    planeNormal[1] = plane->normal[1];
                    planeNormal[2] = plane->normal[2];
                    planeDist = plane->dist;
                } else {
                    planeNormal[0] = -plane->normal[0];
                    planeNormal[1] = -plane->normal[1];
                    planeNormal[2] = -plane->normal[2];
                    planeDist = -plane->dist;
                }

                if (traceWork->sphere.use != 0) {
                    planeDist += traceWork->sphere.radius;

                    const float sphereOffset =
                        ((planeNormal[0] * traceWork->sphere.offset[0]) +
                         (planeNormal[1] * traceWork->sphere.offset[1])) +
                        (planeNormal[2] * traceWork->sphere.offset[2]);

                    if (sphereOffset > 0.0f) {
                        testPoint[0] =
                            traceWork->start[0] -
                            traceWork->sphere.offset[0];
                        testPoint[1] =
                            traceWork->start[1] -
                            traceWork->sphere.offset[1];
                        testPoint[2] =
                            traceWork->start[2] -
                            traceWork->sphere.offset[2];
                    } else {
                        testPoint[0] =
                            traceWork->start[0] +
                            traceWork->sphere.offset[0];
                        testPoint[1] =
                            traceWork->start[1] +
                            traceWork->sphere.offset[1];
                        testPoint[2] =
                            traceWork->start[2] +
                            traceWork->sphere.offset[2];
                    }
                } else {
                    const float *offset =
                        traceWork->offsets[plane->signbits];
                    const float planeOffset =
                        ((offset[0] * planeNormal[0]) +
                         (offset[1] * planeNormal[1])) +
                        (offset[2] * planeNormal[2]);

                    planeDist += fabsf(planeOffset);
                    testPoint[0] = traceWork->start[0];
                    testPoint[1] = traceWork->start[1];
                    testPoint[2] = traceWork->start[2];
                }

                /* 0x80513a3..0x80513c5: the child distance is compared
                 * against zero in the 80-bit register chain with no
                 * intermediate float store. */
                if ((((planeNormal[0] * testPoint[0]) +
                      (planeNormal[1] * testPoint[1])) +
                     (planeNormal[2] * testPoint[2])) -
                        planeDist >
                    0.0f) {
                    break;
                }
            }

            if (childIndex >= facet->numBorders) {
                return 1;
            }
        }
    }

    return 0;
}


void CM_TracePointThroughPatchCollide(
    traceWork_t *traceWork,
    const patchCollide_t *patchCollide)
{
    float planeFractions[CM_PATCH_PLANE_LIMIT];
    qboolean planeSides[CM_PATCH_PLANE_LIMIT];

    if (cm_playerCurveClip->integer == 0 || traceWork->isPoint == 0) {
        return;
    }

    const patchPlane_t *plane = patchCollide->planes;
    for (int32_t planeIndex = 0; planeIndex < patchCollide->numPlanes;
         ++planeIndex, ++plane) {
        const float *offset = traceWork->offsets[plane->signbits];
#if EMULATE_X87
        /* x87 (DLL 0x0804f85b): planeOffset is a 3-term dot rounded to its own
         * float slot; each distance is then ONE 80-bit chain — the dot, the
         * -dist and the +planeOffset — rounded once at the store
         * (fmul;fmul;faddp;fmul;faddp;fsub dist;fadd planeOffset;fstp). */
        const float planeOffset = x87f_store_f32(x87f_add(
            x87f_add(x87f_mul(x87f_load_f32(offset[0]),
                              x87f_load_f32(plane->normal[0])),
                     x87f_mul(x87f_load_f32(offset[1]),
                              x87f_load_f32(plane->normal[1]))),
            x87f_mul(x87f_load_f32(offset[2]),
                     x87f_load_f32(plane->normal[2]))));
        const float startDistance = x87f_store_f32(x87f_add(
            x87f_sub(
                x87f_add(x87f_add(x87f_mul(x87f_load_f32(traceWork->start[0]),
                                           x87f_load_f32(plane->normal[0])),
                                  x87f_mul(x87f_load_f32(traceWork->start[1]),
                                           x87f_load_f32(plane->normal[1]))),
                         x87f_mul(x87f_load_f32(traceWork->start[2]),
                                  x87f_load_f32(plane->normal[2]))),
                x87f_load_f32(plane->dist)),
            x87f_load_f32(planeOffset)));
        const float endDistance = x87f_store_f32(x87f_add(
            x87f_sub(
                x87f_add(x87f_add(x87f_mul(x87f_load_f32(traceWork->end[0]),
                                           x87f_load_f32(plane->normal[0])),
                                  x87f_mul(x87f_load_f32(traceWork->end[1]),
                                           x87f_load_f32(plane->normal[1]))),
                         x87f_mul(x87f_load_f32(traceWork->end[2]),
                                  x87f_load_f32(plane->normal[2]))),
                x87f_load_f32(plane->dist)),
            x87f_load_f32(planeOffset)));
#else
        const float planeOffset =
            ((offset[0] * plane->normal[0]) +
             (offset[1] * plane->normal[1])) +
            (offset[2] * plane->normal[2]);
        const float startDistance =
            (((traceWork->start[0] * plane->normal[0]) +
              (traceWork->start[1] * plane->normal[1])) +
             (traceWork->start[2] * plane->normal[2])) -
            plane->dist + planeOffset;
        const float endDistance =
            (((traceWork->end[0] * plane->normal[0]) +
              (traceWork->end[1] * plane->normal[1])) +
             (traceWork->end[2] * plane->normal[2])) -
            plane->dist + planeOffset;
#endif

        if (startDistance <= 0.0f) {
            planeSides[planeIndex] = 0;
        } else {
            planeSides[planeIndex] = 1;
        }

        if (startDistance == endDistance) {
            planeFractions[planeIndex] = 99999.0f;
        } else {
#if EMULATE_X87
            planeFractions[planeIndex] = x87f_store_f32(
                x87f_div(x87f_load_f32(startDistance),
                         x87f_sub(x87f_load_f32(startDistance),
                                  x87f_load_f32(endDistance))));
#else
            planeFractions[planeIndex] =
                startDistance / (startDistance - endDistance);
#endif
            if (planeFractions[planeIndex] <= 0.0f) {
                planeFractions[planeIndex] = 99999.0f;
            }
        }
    }

    const facet_t *facet = patchCollide->facets;
    for (int32_t facetIndex = 0;
         facetIndex < patchCollide->numFacets; ++facetIndex, ++facet) {
        const int32_t rootPlaneIndex = facet->surfacePlane;

        if (planeSides[rootPlaneIndex] == 0) {
            continue;
        }

        const float rootFraction = planeFractions[rootPlaneIndex];
        if (rootFraction < 0.0f || traceWork->trace.fraction < rootFraction) {
            continue;
        }

        int32_t childIndex;
        for (childIndex = 0; childIndex < facet->numBorders;
             ++childIndex) {
            const int32_t childPlaneIndex =
                facet->borderPlanes[childIndex];

            if (planeSides[childPlaneIndex] ==
                facet->borderInward[childIndex]) {
                if (planeFractions[childPlaneIndex] < rootFraction) {
                    break;
                }
            } else if (rootFraction < planeFractions[childPlaneIndex]) {
                break;
            }
        }

        if (childIndex == facet->numBorders) {
            plane = &patchCollide->planes[rootPlaneIndex];

            const float *offset = traceWork->offsets[plane->signbits];
#if EMULATE_X87
            /* Same structure as the per-plane pass above; this is the site that
             * actually sets trace.fraction for curved terrain. */
            const float planeOffset = x87f_store_f32(x87f_add(
                x87f_add(x87f_mul(x87f_load_f32(offset[0]),
                                  x87f_load_f32(plane->normal[0])),
                         x87f_mul(x87f_load_f32(offset[1]),
                                  x87f_load_f32(plane->normal[1]))),
                x87f_mul(x87f_load_f32(offset[2]),
                         x87f_load_f32(plane->normal[2]))));
            const float startDistance = x87f_store_f32(x87f_add(
                x87f_sub(
                    x87f_add(
                        x87f_add(x87f_mul(x87f_load_f32(traceWork->start[0]),
                                          x87f_load_f32(plane->normal[0])),
                                 x87f_mul(x87f_load_f32(traceWork->start[1]),
                                          x87f_load_f32(plane->normal[1]))),
                        x87f_mul(x87f_load_f32(traceWork->start[2]),
                                 x87f_load_f32(plane->normal[2]))),
                    x87f_load_f32(plane->dist)),
                x87f_load_f32(planeOffset)));
            const float endDistance = x87f_store_f32(x87f_add(
                x87f_sub(
                    x87f_add(
                        x87f_add(x87f_mul(x87f_load_f32(traceWork->end[0]),
                                          x87f_load_f32(plane->normal[0])),
                                 x87f_mul(x87f_load_f32(traceWork->end[1]),
                                          x87f_load_f32(plane->normal[1]))),
                        x87f_mul(x87f_load_f32(traceWork->end[2]),
                                 x87f_load_f32(plane->normal[2]))),
                    x87f_load_f32(plane->dist)),
                x87f_load_f32(planeOffset)));

            traceWork->trace.fraction = x87f_store_f32(x87f_div(
                x87f_sub(x87f_load_f32(startDistance), x87f_load_f32(0.125f)),
                x87f_sub(x87f_load_f32(startDistance),
                         x87f_load_f32(endDistance))));
#else
            const float planeOffset =
                ((offset[0] * plane->normal[0]) +
                 (offset[1] * plane->normal[1])) +
                (offset[2] * plane->normal[2]);
            const float startDistance =
                (((traceWork->start[0] * plane->normal[0]) +
                  (traceWork->start[1] * plane->normal[1])) +
                 (traceWork->start[2] * plane->normal[2])) -
                plane->dist + planeOffset;
            const float endDistance =
                (((traceWork->end[0] * plane->normal[0]) +
                  (traceWork->end[1] * plane->normal[1])) +
                 (traceWork->end[2] * plane->normal[2])) -
                plane->dist + planeOffset;

            traceWork->trace.fraction =
                (startDistance - 0.125f) / (startDistance - endDistance);
#endif
            if (traceWork->trace.fraction < 0.0f) {
                traceWork->trace.fraction = 0.0f;
            }

            traceWork->trace.normal[0] = plane->normal[0];
            traceWork->trace.normal[1] = plane->normal[1];
            traceWork->trace.normal[2] = plane->normal[2];
        }
    }
}

qboolean CM_SightTracePointThroughPatchCollide(
    const traceWork_t *traceWork,
    const patchCollide_t *patchCollide)
{
    float planeFractions[CM_PATCH_PLANE_LIMIT];
    qboolean planeSides[CM_PATCH_PLANE_LIMIT];

    if (cm_playerCurveClip->integer == 0 || traceWork->isPoint == 0) {
        return 1;
    }

    const patchPlane_t *plane = patchCollide->planes;
    for (int32_t planeIndex = 0; planeIndex < patchCollide->numPlanes;
         ++planeIndex, ++plane) {
        const float *offset = traceWork->offsets[plane->signbits];
        const float planeOffset =
            ((offset[0] * plane->normal[0]) +
             (offset[1] * plane->normal[1])) +
            (offset[2] * plane->normal[2]);
        const float startDistance =
            (((traceWork->start[0] * plane->normal[0]) +
              (traceWork->start[1] * plane->normal[1])) +
             (traceWork->start[2] * plane->normal[2])) -
            plane->dist + planeOffset;
        const float endDistance =
            (((traceWork->end[0] * plane->normal[0]) +
              (traceWork->end[1] * plane->normal[1])) +
             (traceWork->end[2] * plane->normal[2])) -
            plane->dist + planeOffset;

        if (startDistance <= 0.0f) {
            planeSides[planeIndex] = 0;
        } else {
            planeSides[planeIndex] = 1;
        }

        if (startDistance == endDistance) {
            planeFractions[planeIndex] = 99999.0f;
        } else {
            planeFractions[planeIndex] =
                startDistance / (startDistance - endDistance);
            if (planeFractions[planeIndex] <= 0.0f) {
                planeFractions[planeIndex] = 99999.0f;
            }
        }
    }

    const facet_t *facet = patchCollide->facets;
    for (int32_t facetIndex = 0;
         facetIndex < patchCollide->numFacets; ++facetIndex, ++facet) {
        const int32_t rootPlaneIndex = facet->surfacePlane;

        if (planeSides[rootPlaneIndex] == 0) {
            continue;
        }

        const float rootFraction = planeFractions[rootPlaneIndex];
        if (rootFraction < 0.0f) {
            continue;
        }

        int32_t childIndex;
        for (childIndex = 0; childIndex < facet->numBorders;
             ++childIndex) {
            const int32_t childPlaneIndex =
                facet->borderPlanes[childIndex];

            if (planeSides[childPlaneIndex] ==
                facet->borderInward[childIndex]) {
                if (planeFractions[childPlaneIndex] < rootFraction) {
                    break;
                }
            } else if (rootFraction < planeFractions[childPlaneIndex]) {
                break;
            }
        }

        if (childIndex == facet->numBorders) {
            plane = &patchCollide->planes[rootPlaneIndex];

            const float *offset = traceWork->offsets[plane->signbits];
            const float planeOffset =
                ((offset[0] * plane->normal[0]) +
                 (offset[1] * plane->normal[1])) +
                (offset[2] * plane->normal[2]);
            const float startDistance =
                (((traceWork->start[0] * plane->normal[0]) +
                  (traceWork->start[1] * plane->normal[1])) +
                 (traceWork->start[2] * plane->normal[2])) -
                plane->dist + planeOffset;
            const float endDistance =
                (((traceWork->end[0] * plane->normal[0]) +
                  (traceWork->end[1] * plane->normal[1])) +
                 (traceWork->end[2] * plane->normal[2])) -
                plane->dist + planeOffset;

            if (startDistance - endDistance > 0.0f &&
                (startDistance - 0.125f) /
                        (startDistance - endDistance) <
                    1.0f) {
                return 0;
            }
        }
    }

    return 1;
}



void CM_TraceThroughPatchCollide(
    traceWork_t *traceWork,
    const patchCollide_t *patchCollide)
{
    if (traceWork->isPoint != 0) {
        CM_TracePointThroughPatchCollide(traceWork, patchCollide);
        return;
    }

    const facet_t *facet = patchCollide->facets;
    for (int32_t facetIndex = 0; facetIndex < patchCollide->numFacets;
         ++facetIndex, ++facet) {
        float enterFraction = -1.0f;
        float leaveFraction = traceWork->trace.fraction;
        int32_t hitChildIndex = -1;

        const patchPlane_t *plane =
            &patchCollide->planes[facet->surfacePlane];
        vec4_t clipPlane = {plane->normal[0], plane->normal[1],
                            plane->normal[2], plane->dist};
        vec3_t start;
        vec3_t end;

        if (traceWork->sphere.use == 0) {
            const float *offset = traceWork->offsets[plane->signbits];
#if EMULATE_X87
            /* planeOffset is a 3-term dot rounded to its float slot; the
             * clipPlane[3] adjust is then its own rounded store. */
            const float planeOffset = x87f_store_f32(x87f_add(
                x87f_add(x87f_mul(x87f_load_f32(offset[0]),
                                  x87f_load_f32(clipPlane[0])),
                         x87f_mul(x87f_load_f32(offset[1]),
                                  x87f_load_f32(clipPlane[1]))),
                x87f_mul(x87f_load_f32(offset[2]),
                         x87f_load_f32(clipPlane[2]))));
            clipPlane[3] = x87f_store_f32(x87f_sub(
                x87f_load_f32(clipPlane[3]), x87f_load_f32(planeOffset)));
#else
            const float planeOffset =
                ((offset[0] * clipPlane[0]) +
                 (offset[1] * clipPlane[1])) +
                (offset[2] * clipPlane[2]);
            clipPlane[3] -= planeOffset;
#endif
            start[0] = traceWork->start[0];
            start[1] = traceWork->start[1];
            start[2] = traceWork->start[2];
            end[0] = traceWork->end[0];
            end[1] = traceWork->end[1];
            end[2] = traceWork->end[2];
        } else {
#if EMULATE_X87
            clipPlane[3] = x87f_store_f32(
                x87f_add(x87f_load_f32(clipPlane[3]),
                         x87f_load_f32(traceWork->sphere.radius)));
            /* Stock sums the sphere dot ascending -- cp0*off0 + cp1*off1
             * + cp2*off2 (VA 0x8050475..0x805049a). */
            const float sphereOffset = x87f_store_f32(x87f_add(
                x87f_add(x87f_mul(x87f_load_f32(clipPlane[0]),
                                  x87f_load_f32(traceWork->sphere.offset[0])),
                         x87f_mul(x87f_load_f32(clipPlane[1]),
                                  x87f_load_f32(traceWork->sphere.offset[1]))),
                x87f_mul(x87f_load_f32(clipPlane[2]),
                         x87f_load_f32(traceWork->sphere.offset[2]))));
#else
            clipPlane[3] += traceWork->sphere.radius;
            /* Stock sums the sphere dot ascending -- cp0*off0 + cp1*off1
             * + cp2*off2 (VA 0x8050475..0x805049a). */
            const float sphereOffset =
                ((clipPlane[0] * traceWork->sphere.offset[0]) +
                 (clipPlane[1] * traceWork->sphere.offset[1])) +
                (clipPlane[2] * traceWork->sphere.offset[2]);
#endif

            if (sphereOffset > 0.0f) {
                start[0] = traceWork->start[0] - traceWork->sphere.offset[0];
                start[1] = traceWork->start[1] - traceWork->sphere.offset[1];
                start[2] = traceWork->start[2] - traceWork->sphere.offset[2];
                end[0] = traceWork->end[0] - traceWork->sphere.offset[0];
                end[1] = traceWork->end[1] - traceWork->sphere.offset[1];
                end[2] = traceWork->end[2] - traceWork->sphere.offset[2];
            } else {
                start[0] = traceWork->start[0] + traceWork->sphere.offset[0];
                start[1] = traceWork->start[1] + traceWork->sphere.offset[1];
                start[2] = traceWork->start[2] + traceWork->sphere.offset[2];
                end[0] = traceWork->end[0] + traceWork->sphere.offset[0];
                end[1] = traceWork->end[1] + traceWork->sphere.offset[1];
                end[2] = traceWork->end[2] + traceWork->sphere.offset[2];
            }
        }

        qboolean hitPlaneChanged;
        if (CM_CheckFacetPlane(clipPlane, start, end, &enterFraction,
                                        &leaveFraction,
                                        &hitPlaneChanged) == 0) {
            continue;
        }

        vec3_t hitNormal;
        if (hitPlaneChanged != 0) {
            hitNormal[0] = clipPlane[0];
            hitNormal[1] = clipPlane[1];
            hitNormal[2] = clipPlane[2];
        }

        int32_t childIndex;
        for (childIndex = 0; childIndex < facet->numBorders;
             ++childIndex) {
            plane = &patchCollide->planes[facet->borderPlanes[childIndex]];

            if (facet->borderInward[childIndex] == 0) {
                clipPlane[0] = plane->normal[0];
                clipPlane[1] = plane->normal[1];
                clipPlane[2] = plane->normal[2];
                clipPlane[3] = plane->dist;
            } else {
                clipPlane[0] = -plane->normal[0];
                clipPlane[1] = -plane->normal[1];
                clipPlane[2] = -plane->normal[2];
                clipPlane[3] = -plane->dist;
            }

            if (traceWork->sphere.use == 0) {
                const float *offset =
                    traceWork->offsets[plane->signbits];
#if EMULATE_X87
                const float planeOffset = x87f_store_f32(x87f_add(
                    x87f_add(x87f_mul(x87f_load_f32(offset[0]),
                                      x87f_load_f32(clipPlane[0])),
                             x87f_mul(x87f_load_f32(offset[1]),
                                      x87f_load_f32(clipPlane[1]))),
                    x87f_mul(x87f_load_f32(offset[2]),
                             x87f_load_f32(clipPlane[2]))));
                clipPlane[3] = x87f_store_f32(
                    x87f_add(x87f_load_f32(clipPlane[3]),
                             x87f_abs(x87f_load_f32(planeOffset))));
#else
                const float planeOffset =
                    ((offset[0] * clipPlane[0]) +
                     (offset[1] * clipPlane[1])) +
                    (offset[2] * clipPlane[2]);
                clipPlane[3] += fabsf(planeOffset);
#endif
                start[0] = traceWork->start[0];
                start[1] = traceWork->start[1];
                start[2] = traceWork->start[2];
                end[0] = traceWork->end[0];
                end[1] = traceWork->end[1];
                end[2] = traceWork->end[2];
            } else {
#if EMULATE_X87
                clipPlane[3] = x87f_store_f32(
                    x87f_add(x87f_load_f32(clipPlane[3]),
                             x87f_load_f32(traceWork->sphere.radius)));
                /* Stock sums the sphere dot ascending -- cp0*off0 +
                 * cp1*off1 + cp2*off2 (VA 0x8050734..0x8050759). */
                const float sphereOffset = x87f_store_f32(x87f_add(
                    x87f_add(
                        x87f_mul(x87f_load_f32(clipPlane[0]),
                                 x87f_load_f32(traceWork->sphere.offset[0])),
                        x87f_mul(x87f_load_f32(clipPlane[1]),
                                 x87f_load_f32(traceWork->sphere.offset[1]))),
                    x87f_mul(x87f_load_f32(clipPlane[2]),
                             x87f_load_f32(traceWork->sphere.offset[2]))));
#else
                clipPlane[3] += traceWork->sphere.radius;
                /* Stock sums the sphere dot ascending -- cp0*off0 +
                 * cp1*off1 + cp2*off2 (VA 0x8050734..0x8050759). */
                const float sphereOffset =
                    ((clipPlane[0] * traceWork->sphere.offset[0]) +
                     (clipPlane[1] * traceWork->sphere.offset[1])) +
                    (clipPlane[2] * traceWork->sphere.offset[2]);
#endif

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
            }

            if (CM_CheckFacetPlane(clipPlane, start, end,
                                            &enterFraction, &leaveFraction,
                                            &hitPlaneChanged) == 0) {
                break;
            }

            if (hitPlaneChanged != 0) {
                hitChildIndex = childIndex;
                hitNormal[0] = clipPlane[0];
                hitNormal[1] = clipPlane[1];
                hitNormal[2] = clipPlane[2];
            }
        }

        if (childIndex >= facet->numBorders &&
            hitChildIndex != facet->numBorders - 1 &&
            enterFraction < leaveFraction && enterFraction >= 0.0f &&
            enterFraction < traceWork->trace.fraction) {
            if (enterFraction < 0.0f) {
                enterFraction = 0.0f;
            }

            traceWork->trace.fraction = enterFraction;
            traceWork->trace.normal[0] = hitNormal[0];
            traceWork->trace.normal[1] = hitNormal[1];
            traceWork->trace.normal[2] = hitNormal[2];
        }
    }
}


qboolean CM_SightTraceThroughPatchCollide(
    const traceWork_t *traceWork,
    const patchCollide_t *patchCollide)
{
    if (traceWork->isPoint != 0) {
        return CM_SightTracePointThroughPatchCollide(traceWork,
                                                       patchCollide);
    }

    const facet_t *facet = patchCollide->facets;
    for (int32_t facetIndex = 0; facetIndex < patchCollide->numFacets;
         ++facetIndex, ++facet) {
        float enterFraction = -1.0f;
        float leaveFraction = 1.0f;
        int32_t hitChildIndex = -1;

        const patchPlane_t *plane =
            &patchCollide->planes[facet->surfacePlane];
        vec4_t clipPlane = {plane->normal[0], plane->normal[1],
                            plane->normal[2], plane->dist};
        vec3_t start;
        vec3_t end;

        if (traceWork->sphere.use == 0) {
            const float *offset = traceWork->offsets[plane->signbits];
            const float planeOffset =
                ((offset[0] * clipPlane[0]) +
                 (offset[1] * clipPlane[1])) +
                (offset[2] * clipPlane[2]);
            clipPlane[3] -= planeOffset;
            start[0] = traceWork->start[0];
            start[1] = traceWork->start[1];
            start[2] = traceWork->start[2];
            end[0] = traceWork->end[0];
            end[1] = traceWork->end[1];
            end[2] = traceWork->end[2];
        } else {
            clipPlane[3] += traceWork->sphere.radius;
            const float sphereOffset =
                ((clipPlane[0] * traceWork->sphere.offset[0]) +
                 (clipPlane[1] * traceWork->sphere.offset[1])) +
                (clipPlane[2] * traceWork->sphere.offset[2]);

            if (sphereOffset > 0.0f) {
                start[0] = traceWork->start[0] - traceWork->sphere.offset[0];
                start[1] = traceWork->start[1] - traceWork->sphere.offset[1];
                start[2] = traceWork->start[2] - traceWork->sphere.offset[2];
                end[0] = traceWork->end[0] - traceWork->sphere.offset[0];
                end[1] = traceWork->end[1] - traceWork->sphere.offset[1];
                end[2] = traceWork->end[2] - traceWork->sphere.offset[2];
            } else {
                start[0] = traceWork->start[0] + traceWork->sphere.offset[0];
                start[1] = traceWork->start[1] + traceWork->sphere.offset[1];
                start[2] = traceWork->start[2] + traceWork->sphere.offset[2];
                end[0] = traceWork->end[0] + traceWork->sphere.offset[0];
                end[1] = traceWork->end[1] + traceWork->sphere.offset[1];
                end[2] = traceWork->end[2] + traceWork->sphere.offset[2];
            }
        }

        qboolean hitPlaneChanged;
        if (CM_CheckFacetPlane(clipPlane, start, end, &enterFraction,
                                        &leaveFraction,
                                        &hitPlaneChanged) == 0) {
            continue;
        }

        int32_t childIndex;
        for (childIndex = 0; childIndex < facet->numBorders;
             ++childIndex) {
            plane = &patchCollide->planes[facet->borderPlanes[childIndex]];

            if (facet->borderInward[childIndex] == 0) {
                clipPlane[0] = plane->normal[0];
                clipPlane[1] = plane->normal[1];
                clipPlane[2] = plane->normal[2];
                clipPlane[3] = plane->dist;
            } else {
                clipPlane[0] = -plane->normal[0];
                clipPlane[1] = -plane->normal[1];
                clipPlane[2] = -plane->normal[2];
                clipPlane[3] = -plane->dist;
            }

            if (traceWork->sphere.use == 0) {
                const float *offset =
                    traceWork->offsets[plane->signbits];
                const float planeOffset =
                    ((offset[0] * clipPlane[0]) +
                     (offset[1] * clipPlane[1])) +
                    (offset[2] * clipPlane[2]);
                clipPlane[3] += fabsf(planeOffset);
                start[0] = traceWork->start[0];
                start[1] = traceWork->start[1];
                start[2] = traceWork->start[2];
                end[0] = traceWork->end[0];
                end[1] = traceWork->end[1];
                end[2] = traceWork->end[2];
            } else {
                clipPlane[3] += traceWork->sphere.radius;
                const float sphereOffset =
                    ((clipPlane[0] * traceWork->sphere.offset[0]) +
                     (clipPlane[1] * traceWork->sphere.offset[1])) +
                    (clipPlane[2] * traceWork->sphere.offset[2]);

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
            }

            if (CM_CheckFacetPlane(clipPlane, start, end,
                                            &enterFraction, &leaveFraction,
                                            &hitPlaneChanged) == 0) {
                break;
            }

            if (hitPlaneChanged != 0) {
                hitChildIndex = childIndex;
            }
        }

        if (childIndex >= facet->numBorders &&
            hitChildIndex != facet->numBorders - 1 &&
            enterFraction < leaveFraction && enterFraction >= 0.0f) {
            return 0;
        }
    }

    return 1;
}


qboolean CM_CheckFacetPlane(
    const vec4_t plane,
    const vec3_t start,
    const vec3_t end,
    float *enterFraction,
    float *leaveFraction,
    qboolean *enterPlaneChanged)
{
    *enterPlaneChanged = 0;

#if EMULATE_X87
    /* Each distance is one 80-bit chain — three products, two adds and the
     * -plane[3] — rounded once at its float slot. */
    const float startDistance = x87f_store_f32(x87f_sub(
        x87f_add(x87f_add(x87f_mul(x87f_load_f32(start[0]),
                                   x87f_load_f32(plane[0])),
                          x87f_mul(x87f_load_f32(start[1]),
                                   x87f_load_f32(plane[1]))),
                 x87f_mul(x87f_load_f32(start[2]), x87f_load_f32(plane[2]))),
        x87f_load_f32(plane[3])));
    const float endDistance = x87f_store_f32(x87f_sub(
        x87f_add(x87f_add(x87f_mul(x87f_load_f32(end[0]),
                                   x87f_load_f32(plane[0])),
                          x87f_mul(x87f_load_f32(end[1]),
                                   x87f_load_f32(plane[1]))),
                 x87f_mul(x87f_load_f32(end[2]), x87f_load_f32(plane[2]))),
        x87f_load_f32(plane[3])));
#else
    const float startDistance =
        (((start[0] * plane[0]) + (start[1] * plane[1])) +
         (start[2] * plane[2])) -
        plane[3];
    const float endDistance =
        (((end[0] * plane[0]) + (end[1] * plane[1])) +
         (end[2] * plane[2])) -
        plane[3];
#endif

    qboolean hit;

    if (!(startDistance > 0.0f) ||
        (!(endDistance >= 0.125f) && !(endDistance >= startDistance))) {
        if (!(0.0f >= startDistance) || !(0.0f >= endDistance)) {
            if (endDistance < startDistance) {
#if EMULATE_X87
                float fraction = x87f_store_f32(x87f_div(
                    x87f_sub(x87f_load_f32(startDistance),
                             x87f_load_f32(0.125f)),
                    x87f_sub(x87f_load_f32(startDistance),
                             x87f_load_f32(endDistance))));
#else
                float fraction =
                    (startDistance - 0.125f) / (startDistance - endDistance);
#endif
                if (fraction < 0.0f) {
                    fraction = 0.0f;
                }

                if (*enterFraction < fraction) {
                    *enterFraction = fraction;
                    *enterPlaneChanged = 1;
                }
            } else {
#if EMULATE_X87
                float fraction = x87f_store_f32(x87f_div(
                    x87f_add(x87f_load_f32(startDistance),
                             x87f_load_f32(0.125f)),
                    x87f_sub(x87f_load_f32(startDistance),
                             x87f_load_f32(endDistance))));
#else
                float fraction =
                    (startDistance + 0.125f) / (startDistance - endDistance);
#endif
                if (fraction > 1.0f) {
                    fraction = 1.0f;
                }

                if (fraction < *leaveFraction) {
                    *leaveFraction = fraction;
                }
            }

            hit = 1;
        } else {
            hit = 1;
        }
    } else {
        hit = 0;
    }

    return hit;
}
#endif
