#include "collision_terrain_trace.h"
#include "compat/coduo_x87emu.h"

#include <math.h>

/*
 * Point-through-triangle-soup trace:
 *
 *   CoDUOMP.exe  0x004247c0..0x0042492e
 *   coduo_lnxded 0x08055595..0x080558dc
 *
 * The complete authoritative bodies retain their original operation graphs.
 */
#if defined(WINDOWS_BEHAVIOR)
/* Exact original single-precision terrain-triangle tolerances:
 * 0x005b9b8c = 0xba83126f and 0x005b9cbc = 0x3f8020c5. */
static const float CM_TERRAIN_TRIANGLE_COORDINATE_MIN =
    -0.0010000000474974513f;
static const float CM_TERRAIN_TRIANGLE_COORDINATE_MAX =
    1.0010000467300415f;

/* Source: CoDUOMP.exe 0x004247c0..0x0042492e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004247c0_0042492e.mcode.
 * Name: exact same-module Mac symbol CM_TracePointThroughTerrainCollide.
 * A crossing of a triangle plane becomes a hit only when both stored
 * barycentric coordinates and their sum remain within the original
 * 0.001-unit edge tolerance. */
void CM_TracePointThroughTerrainCollide(
    traceWork_t *traceWork,
    const collisionTriangleSoup_t *terrainCollide)
{
    for (int32_t triangleIndex = 0;
         triangleIndex <
             (int32_t)terrainCollide->triangleCount;
         ++triangleIndex) {
        const collisionSoupTriangle_t *const
            triangle =
                &terrainCollide
                     ->triangles[triangleIndex];

        const long double endDistanceRaw =
            ((long double)triangle->plane.components.normal[1] *
                 traceWork->end[1] +
             (long double)triangle->plane.components.normal[0] *
                 traceWork->end[0]) +
            (long double)triangle->plane.components.normal[2] *
                traceWork->end[2] -
            triangle->plane.components.distance;
        const float endDistance =
            (float)endDistanceRaw;
        if (!(endDistanceRaw < 0.0f ||
              isunordered(endDistanceRaw, 0.0f))) {
            continue;
        }

        const long double startDistanceRaw =
            ((long double)triangle->plane.components.normal[2] *
                 traceWork->start[2] +
             (long double)triangle->plane.components.normal[1] *
                 traceWork->start[1]) +
            (long double)triangle->plane.components.normal[0] *
                traceWork->start[0] -
            triangle->plane.components.distance;
        const float startDistance =
            (float)startDistanceRaw;
        if (!(startDistanceRaw > 0.0f ||
              isunordered(startDistanceRaw, 0.0f))) {
            continue;
        }

        const long double distanceDelta =
            (long double)startDistance -
            endDistance;
        const long double enterFractionRaw =
            ((long double)startDistance - 0.125f) /
            distanceDelta;
        const float enterFraction =
            (float)enterFractionRaw;
        /* FST records the fraction as a float for a later hit, but the
         * immediately following FCOMP still sees the unrounded ST0 value. */
        if (!(enterFractionRaw <
                  traceWork->trace.fraction ||
              isunordered(
                  enterFractionRaw,
                  traceWork->trace.fraction))) {
            continue;
        }

        const long double planeFraction =
            (long double)startDistance /
            distanceDelta;
        vec3_t hitPoint;
        for (int32_t axis = 0; axis < 2;
             ++axis) {
            hitPoint[axis] = (float)(
                (long double)traceWork->start[axis] +
                planeFraction *
                    traceWork->delta[axis]);
        }
        const long double hitPointZRaw =
            (long double)traceWork->start[2] +
            planeFraction * traceWork->delta[2];
        hitPoint[2] = (float)hitPointZRaw;

        const long double sCoordinateRaw =
            hitPointZRaw *
                triangle->svec[2] +
            ((long double)hitPoint[1] *
                 triangle->svec[1] +
             (long double)hitPoint[0] *
                 triangle->svec[0]) -
            triangle->svec[3];
        const float sCoordinate =
            (float)sCoordinateRaw;
        /* The lower-bound comparison consumes the value retained by FST;
         * the upper-bound and later sum reload the rounded float. */
        if ((!isunordered(
                 sCoordinateRaw,
                 CM_TERRAIN_TRIANGLE_COORDINATE_MIN) &&
             sCoordinateRaw <
                 CM_TERRAIN_TRIANGLE_COORDINATE_MIN) ||
            (!isunordered(
                 sCoordinate,
                 CM_TERRAIN_TRIANGLE_COORDINATE_MAX) &&
             sCoordinate >
                 CM_TERRAIN_TRIANGLE_COORDINATE_MAX)) {
            continue;
        }

        const long double tCoordinate =
            (long double)hitPoint[2] *
                triangle->tvec[2] +
            ((long double)hitPoint[1] *
                 triangle->tvec[1] +
             (long double)hitPoint[0] *
                 triangle->tvec[0]) -
            triangle->tvec[3];
        if ((!isunordered(
                 tCoordinate,
                 CM_TERRAIN_TRIANGLE_COORDINATE_MIN) &&
             tCoordinate <
                 CM_TERRAIN_TRIANGLE_COORDINATE_MIN) ||
            (!isunordered(
                 (long double)sCoordinate +
                     tCoordinate,
                 CM_TERRAIN_TRIANGLE_COORDINATE_MAX) &&
             (long double)sCoordinate +
                     tCoordinate >
                 CM_TERRAIN_TRIANGLE_COORDINATE_MAX)) {
            continue;
        }

        traceWork->trace.fraction =
            enterFraction;
        traceWork->trace.normal[0] =
            triangle->plane.components.normal[0];
        traceWork->trace.normal[1] =
            triangle->plane.components.normal[1];
        traceWork->trace.normal[2] =
            triangle->plane.components.normal[2];
    }
}
#elif defined(LINUX_BEHAVIOR)

void CM_TracePointThroughTerrainCollide(traceWork_t *traceWork,
                                      const collisionTriangleSoup_t *terrainCollide)
{
    for (int32_t facetIndex = 0;
         facetIndex < (int32_t)terrainCollide->triangleCount; ++facetIndex) {
        const collisionSoupTriangle_t *facet =
            &terrainCollide->triangles[facetIndex];
        const plane_t *surfacePlane =
            &facet->plane;

#if EMULATE_X87
        /* x87 (DLL 0x08055595): each plane distance is one 80-bit chain — three
         * products, two adds and the -dist — rounded once at its float slot
         * (fmul;fmul;faddp;fmul;faddp;fsub dist;fstp). The comparisons are
         * fucompp against fldz / the float epsilons, and fall through on
         * unordered operands, which the negated `!(a >= b)` forms preserve. */
        const float endDistance = x87f_store_f32(x87f_sub(
            x87f_add(x87f_add(x87f_mul(x87f_load_f32(traceWork->end[0]),
                                       x87f_load_f32(surfacePlane->components.normal[0])),
                              x87f_mul(x87f_load_f32(traceWork->end[1]),
                                       x87f_load_f32(surfacePlane->components.normal[1]))),
                     x87f_mul(x87f_load_f32(traceWork->end[2]),
                              x87f_load_f32(surfacePlane->components.normal[2]))),
            x87f_load_f32(surfacePlane->components.distance)));
#else
        const float endDistance =
            ((traceWork->end[0] * surfacePlane->components.normal[0]) +
             (traceWork->end[1] * surfacePlane->components.normal[1])) +
            (traceWork->end[2] * surfacePlane->components.normal[2]) -
            surfacePlane->components.distance;
#endif

        /*
         * The original x87 comparisons fall through on unordered operands; the
         * negated forms below preserve that branch behavior.
         */
        if (!(endDistance >= 0.0f)) {
#if EMULATE_X87
            const float startDistance = x87f_store_f32(x87f_sub(
                x87f_add(
                    x87f_add(x87f_mul(x87f_load_f32(traceWork->start[0]),
                                      x87f_load_f32(surfacePlane->components.normal[0])),
                             x87f_mul(x87f_load_f32(traceWork->start[1]),
                                      x87f_load_f32(surfacePlane->components.normal[1]))),
                    x87f_mul(x87f_load_f32(traceWork->start[2]),
                             x87f_load_f32(surfacePlane->components.normal[2]))),
                x87f_load_f32(surfacePlane->components.distance)));
#else
            const float startDistance =
                ((traceWork->start[0] * surfacePlane->components.normal[0]) +
                 (traceWork->start[1] * surfacePlane->components.normal[1])) +
                (traceWork->start[2] * surfacePlane->components.normal[2]) -
                surfacePlane->components.distance;
#endif

            if (!(0.0f >= startDistance)) {
#if EMULATE_X87
                const float enterFraction = x87f_store_f32(x87f_div(
                    x87f_sub(x87f_load_f32(startDistance),
                             x87f_load_f32(CM_TERRAIN_POINT_ENTER_EPSILON)),
                    x87f_sub(x87f_load_f32(startDistance),
                             x87f_load_f32(endDistance))));
#else
                const float enterFraction =
                    (startDistance - CM_TERRAIN_POINT_ENTER_EPSILON) /
                    (startDistance - endDistance);
#endif

                if (!(enterFraction >= traceWork->trace.fraction)) {
#if EMULATE_X87
                    const float hitFraction = x87f_store_f32(x87f_div(
                        x87f_load_f32(startDistance),
                        x87f_sub(x87f_load_f32(startDistance),
                                 x87f_load_f32(endDistance))));
                    const float hitX = x87f_store_f32(x87f_add(
                        x87f_load_f32(traceWork->start[0]),
                        x87f_mul(x87f_load_f32(traceWork->delta[0]),
                                 x87f_load_f32(hitFraction))));
                    const float hitY = x87f_store_f32(x87f_add(
                        x87f_load_f32(traceWork->start[1]),
                        x87f_mul(x87f_load_f32(traceWork->delta[1]),
                                 x87f_load_f32(hitFraction))));
                    const float hitZ = x87f_store_f32(x87f_add(
                        x87f_load_f32(traceWork->start[2]),
                        x87f_mul(x87f_load_f32(traceWork->delta[2]),
                                 x87f_load_f32(hitFraction))));
                    const vec4_t *edgePlane =
                        &facet->svec;

                    const float edge0Distance = x87f_store_f32(x87f_sub(
                        x87f_add(x87f_add(x87f_mul(x87f_load_f32(hitX),
                                                   x87f_load_f32(
                                                       (*edgePlane)[0])),
                                          x87f_mul(x87f_load_f32(hitY),
                                                   x87f_load_f32(
                                                       (*edgePlane)[1]))),
                                 x87f_mul(x87f_load_f32(hitZ),
                                          x87f_load_f32((*edgePlane)[2]))),
                        x87f_load_f32((*edgePlane)[3])));
#else
                    const float hitFraction =
                        startDistance / (startDistance - endDistance);
                    const float hitX =
                        traceWork->start[0] +
                        (traceWork->delta[0] * hitFraction);
                    const float hitY =
                        traceWork->start[1] +
                        (traceWork->delta[1] * hitFraction);
                    const float hitZ =
                        traceWork->start[2] +
                        (traceWork->delta[2] * hitFraction);
                    const vec4_t *edgePlane =
                        &facet->svec;

                    const float edge0Distance =
                        ((hitX * (*edgePlane)[0]) +
                         (hitY * (*edgePlane)[1])) +
                        (hitZ * (*edgePlane)[2]) - (*edgePlane)[3];
#endif

                    if (!(CM_TERRAIN_BARYCENTRIC_MIN > edge0Distance) &&
                        !(edge0Distance > CM_TERRAIN_BARYCENTRIC_MAX)) {
                        edgePlane = &facet->tvec;
#if EMULATE_X87
                        const float edge1Distance = x87f_store_f32(x87f_sub(
                            x87f_add(
                                x87f_add(x87f_mul(x87f_load_f32(hitX),
                                                  x87f_load_f32(
                                                      (*edgePlane)[0])),
                                         x87f_mul(x87f_load_f32(hitY),
                                                  x87f_load_f32(
                                                      (*edgePlane)[1]))),
                                x87f_mul(x87f_load_f32(hitZ),
                                         x87f_load_f32((*edgePlane)[2]))),
                            x87f_load_f32((*edgePlane)[3])));

                        /* edge0Distance + edge1Distance is formed and compared
                         * in 80-bit (fadd; fld max; fucompp) with no store. */
                        if (!(CM_TERRAIN_BARYCENTRIC_MIN > edge1Distance) &&
                            !x87f_lt(
                                x87f_load_f32(CM_TERRAIN_BARYCENTRIC_MAX),
                                x87f_add(x87f_load_f32(edge0Distance),
                                         x87f_load_f32(edge1Distance)))) {
#else
                        const float edge1Distance =
                            ((hitX * (*edgePlane)[0]) +
                             (hitY * (*edgePlane)[1])) +
                            (hitZ * (*edgePlane)[2]) - (*edgePlane)[3];

                        if (!(CM_TERRAIN_BARYCENTRIC_MIN > edge1Distance) &&
                            !((edge0Distance + edge1Distance) >
                              CM_TERRAIN_BARYCENTRIC_MAX)) {
#endif
                            traceWork->trace.fraction = enterFraction;
                            traceWork->trace.normal[0] =
                                surfacePlane->components.normal[0];
                            traceWork->trace.normal[1] =
                                surfacePlane->components.normal[1];
                            traceWork->trace.normal[2] =
                                surfacePlane->components.normal[2];
                        }
                    }
                }
            }
        }
    }
}
#else
#error "collision_terrain_point.c requires a target behavior"
#endif
