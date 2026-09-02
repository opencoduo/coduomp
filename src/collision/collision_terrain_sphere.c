#include "collision_terrain_trace.h"
#include "compat/coduo_x87emu.h"

#include <math.h>
#include <stdint.h>

extern int32_t cm_checkcount;

/*
 * Swept sphere/capsule trace against triangle soup:
 *
 *   CoDUOMP.exe  0x00424930..0x00425117
 *   coduo_lnxded 0x080558dd..0x08056772
 *
 * The complete authoritative bodies retain their original operation graphs.
 */
#if defined(WINDOWS_BEHAVIOR)
/* Source: CoDUOMP.exe 0x00424930..0x00425117.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00424930_00425117.mcode.
 * Name: exact same-module Mac symbol CM_TraceSphereThroughTerrainCollide.
 * Triangle-face crossings use the expanded radius; barycentric misses select
 * deduplicated finite edges or vertices for quadratic swept-sphere tests. */
void CM_TraceSphereThroughTerrainCollide(traceWork_t *traceWork, const collisionTriangleSoup_t *terrainCollide)
{
    const long double cylinderHalfHeightRaw = (long double)traceWork->sphere.halfheight - traceWork->sphere.radius;
    float verticalSpan;
    if (terrainCollide->negativeZOnly != 0) {
        verticalSpan = (float)(-2.0L * cylinderHalfHeightRaw);
        traceWork->start[2] = (float)((long double)traceWork->start[2] + cylinderHalfHeightRaw);
        traceWork->end[2] = (float)((long double)traceWork->end[2] + cylinderHalfHeightRaw);
    } else {
        verticalSpan = (float)(2.0L * cylinderHalfHeightRaw);
        traceWork->start[2] = (float)((long double)traceWork->start[2] - cylinderHalfHeightRaw);
        traceWork->end[2] = (float)((long double)traceWork->end[2] - cylinderHalfHeightRaw);
    }

    const float expandedRadius = traceWork->sphere.radius + 0.125f;
    const float negativeExpandedRadius = -expandedRadius;
    const float fractionEpsilon = 0.000009999999747378752f;

    for (int32_t triangleIndex = 0; triangleIndex < (int32_t)terrainCollide->triangleCount; ++triangleIndex) {
        const collisionSoupTriangle_t *const triangle = &terrainCollide->triangles[triangleIndex];
        const long double endDistanceRaw = ((long double)triangle->plane.components.normal[2] * traceWork->end[2] +
                                            (long double)triangle->plane.components.normal[1] * traceWork->end[1]) +
                                           (long double)triangle->plane.components.normal[0] * traceWork->end[0] -
                                           triangle->plane.components.distance;
        if (!(endDistanceRaw < expandedRadius || isunordered(endDistanceRaw, expandedRadius)))
            continue;

        const long double startDistanceRaw = ((long double)triangle->plane.components.normal[1] * traceWork->start[1] +
                                              (long double)triangle->plane.components.normal[0] * traceWork->start[0]) +
                                             (long double)triangle->plane.components.normal[2] * traceWork->start[2] -
                                             triangle->plane.components.distance;
        const float startDistance = (float)startDistanceRaw;
        const float distanceDelta = (float)(startDistanceRaw - endDistanceRaw);
        if (!(distanceDelta > 0.0f || isunordered(distanceDelta, 0.0f)))
            continue;

        if (!(startDistance > negativeExpandedRadius || isunordered(startDistance, negativeExpandedRadius))) {
            const long double oppositeCapDistance = (long double)verticalSpan * triangle->plane.components.normal[2] + startDistance;
            if (!(oppositeCapDistance > negativeExpandedRadius || isunordered(oppositeCapDistance, negativeExpandedRadius))) {
                continue;
            }

            const float inverseNormalZ = 1.0f / triangle->plane.components.normal[2];
            const float sAtStart =
                (float)(((long double)triangle->svec[1] * traceWork->start[1] + (long double)triangle->svec[0] * traceWork->start[0]) +
                        (long double)triangle->svec[2] * traceWork->start[2] - triangle->svec[3]);
            const float tAtStart =
                (float)(((long double)triangle->tvec[1] * traceWork->start[1] + (long double)triangle->tvec[0] * traceWork->start[0]) +
                        (long double)triangle->tvec[2] * traceWork->start[2] - triangle->tvec[3]);

            const long double firstZOffset = ((long double)negativeExpandedRadius - startDistance) * inverseNormalZ;
            const long double firstSRaw = firstZOffset * triangle->svec[2] + sAtStart;
            const float firstS = (float)firstSRaw;
            const long double firstT = firstZOffset * triangle->tvec[2] + tAtStart;
            if (firstSRaw >= 0.0f && firstT >= 0.0f && (long double)firstS + firstT <= 1.0f) {
                traceWork->trace.normal[0] = triangle->plane.components.normal[0];
                traceWork->trace.normal[1] = triangle->plane.components.normal[1];
                traceWork->trace.normal[2] = triangle->plane.components.normal[2];
                traceWork->trace.fraction = 0.0f;
                traceWork->trace.startsolid = qtrue;
                return;
            }

            const long double secondZOffset = !isunordered(oppositeCapDistance, expandedRadius) && oppositeCapDistance < expandedRadius
                                                  ? (long double)verticalSpan
                                                  : ((long double)expandedRadius - startDistance) * inverseNormalZ;
            const long double secondSRaw = secondZOffset * triangle->svec[2] + sAtStart;
            const float secondS = (float)secondSRaw;
            const long double secondT = secondZOffset * triangle->tvec[2] + tAtStart;
            /* 0x00424b45..0x00424b81 uses the asymmetric x87 status
             * tests from the original: the second-cap candidate is rejected
             * only by ordered s < 0, t < 0, or s+t > 1 comparisons. */
            if (!(secondSRaw < 0.0f) && !(secondT < 0.0f) && !((long double)secondS + secondT > 1.0f)) {
                traceWork->trace.normal[0] = triangle->plane.components.normal[0];
                traceWork->trace.normal[1] = triangle->plane.components.normal[1];
                traceWork->trace.normal[2] = triangle->plane.components.normal[2];
                traceWork->trace.fraction = 0.0f;
                traceWork->trace.startsolid = qtrue;
                return;
            }
            continue;
        }

        long double faceFraction;
        long double facePoint[3];
        if (!isunordered(startDistance, expandedRadius) && startDistance < expandedRadius) {
            faceFraction = 0.0L;
            facePoint[0] = traceWork->start[0];
            facePoint[1] = traceWork->start[1];
            facePoint[2] = traceWork->start[2];
        } else {
            faceFraction = ((long double)startDistance - expandedRadius) / distanceDelta;
            if (!isunordered(faceFraction, traceWork->trace.fraction) && faceFraction > traceWork->trace.fraction) {
                continue;
            }
            for (int32_t axis = 0; axis < 3; ++axis) {
                facePoint[axis] = (long double)traceWork->start[axis] + faceFraction * traceWork->delta[axis];
            }
        }

        const float faceS = (float)(((long double)facePoint[1] * triangle->svec[1] + (long double)facePoint[0] * triangle->svec[0]) +
                                    (long double)facePoint[2] * triangle->svec[2] - triangle->svec[3]);
        const long double faceT = ((long double)facePoint[2] * triangle->tvec[2] + (long double)facePoint[1] * triangle->tvec[1]) +
                                  (long double)facePoint[0] * triangle->tvec[0] - triangle->tvec[3];

        int32_t outsideMask = 0;
        if ((long double)faceS + faceT > 1.0f)
            outsideMask |= 1;
        if (faceS < 0.0f)
            outsideMask |= 2;
        if (faceT < 0.0f)
            outsideMask |= 4;

        if (outsideMask == 0) {
            traceWork->trace.normal[0] = triangle->plane.components.normal[0];
            traceWork->trace.normal[1] = triangle->plane.components.normal[1];
            traceWork->trace.normal[2] = triangle->plane.components.normal[2];
            if (faceFraction <= fractionEpsilon) {
                traceWork->trace.fraction = 0.0f;
                traceWork->trace.startsolid = qtrue;
                return;
            }
            traceWork->trace.fraction = (float)(faceFraction - fractionEpsilon);
            continue;
        }

        const long double radiusSquared = (long double)expandedRadius * expandedRadius;
        for (int32_t corner = 0; corner < CM_TRIANGLE_VERTEX_COUNT; ++corner) {
            long double featureFraction;
            vec3_t featureNormal;

            if ((outsideMask & (1 << corner)) != 0) {
                collisionSoupEdge_t *const edge = triangle->oppositeEdges[corner];
                if (edge == NULL || edge->checkcount == cm_checkcount) {
                    continue;
                }
                edge->checkcount = cm_checkcount;

                vec3_t relativeStart;
                for (int32_t axis = 0; axis < 3; ++axis) {
                    relativeStart[axis] = traceWork->start[axis] - edge->origin[axis];
                }
                const float radialStart0 = (float)(((long double)relativeStart[0] * edge->radialAxes[0][0] +
                                                    (long double)relativeStart[2] * edge->radialAxes[0][2]) +
                                                   (long double)relativeStart[1] * edge->radialAxes[0][1]);
                const float radialStart1 = (float)(((long double)relativeStart[0] * edge->radialAxes[1][0] +
                                                    (long double)relativeStart[2] * edge->radialAxes[1][2]) +
                                                   (long double)relativeStart[1] * edge->radialAxes[1][1]);
                const float edgeStart = (float)(((long double)relativeStart[2] * edge->unitDirection[2] +
                                                 (long double)relativeStart[1] * edge->unitDirection[1]) +
                                                (long double)relativeStart[0] * edge->unitDirection[0]);
                const long double radialConstantRaw =
                    (long double)radialStart1 * radialStart1 + (long double)radialStart0 * radialStart0 - radiusSquared;
                const float radialConstant = (float)radialConstantRaw;

                if (radialConstantRaw <= 0.0f) {
                    if (edgeStart < 0.0f || edgeStart > edge->length) {
                        continue;
                    }
                    traceWork->trace.normal[0] = triangle->plane.components.normal[0];
                    traceWork->trace.normal[1] = triangle->plane.components.normal[1];
                    traceWork->trace.normal[2] = triangle->plane.components.normal[2];
                    traceWork->trace.fraction = 0.0f;
                    traceWork->trace.startsolid = qtrue;
                    return;
                }

                const float radialDelta0 = (float)(((long double)traceWork->delta[0] * edge->radialAxes[0][0] +
                                                    (long double)traceWork->delta[1] * edge->radialAxes[0][1]) +
                                                   (long double)traceWork->delta[2] * edge->radialAxes[0][2]);
                const float radialDelta1 = (float)(((long double)traceWork->delta[1] * edge->radialAxes[1][1] +
                                                    (long double)traceWork->delta[0] * edge->radialAxes[1][0]) +
                                                   (long double)traceWork->delta[2] * edge->radialAxes[1][2]);
                const long double radialLinearRaw = (long double)radialStart1 * radialDelta1 + (long double)radialStart0 * radialDelta0;
                const float radialLinear = (float)radialLinearRaw;
                if (!(radialLinearRaw < 0.0f || isunordered(radialLinearRaw, 0.0f)))
                    continue;

                const long double radialQuadratic = (long double)radialDelta1 * radialDelta1 + (long double)radialDelta0 * radialDelta0;
                const long double discriminant = (long double)radialLinear * radialLinear - radialQuadratic * radialConstant;
                if (!(discriminant > 0.0L || isunordered(discriminant, 0.0L)))
                    continue;

                const long double featureFractionRaw = (-sqrtl(discriminant) - radialLinear) / radialQuadratic;
                featureFraction = (float)featureFractionRaw;
                if (!(featureFractionRaw < traceWork->trace.fraction)) {
                    continue;
                }

                const long double edgeAtHit = (((long double)traceWork->delta[2] * edge->unitDirection[2] +
                                                (long double)traceWork->delta[0] * edge->unitDirection[0]) +
                                               (long double)traceWork->delta[1] * edge->unitDirection[1]) *
                                                  featureFraction +
                                              edgeStart;
                if (edgeAtHit < 0.0L || edgeAtHit > edge->length) {
                    continue;
                }

                const long double inverseRadius = 1.0L / expandedRadius;
                const long double normalAxis0 = ((long double)radialDelta0 * featureFraction + radialStart0) * inverseRadius;
                const float normalAxis1 = (float)(((long double)radialDelta1 * featureFraction + radialStart1) * inverseRadius);
                for (int32_t axis = 0; axis < 3; ++axis) {
                    const float axis0Term = (float)(normalAxis0 * edge->radialAxes[0][axis]);
                    featureNormal[axis] = (float)((long double)axis0Term + (long double)normalAxis1 * edge->radialAxes[1][axis]);
                }
            } else {
                collisionSoupVertex_t *const vertex = triangle->vertices[corner];
                if (vertex == NULL || vertex->checkcount == cm_checkcount) {
                    continue;
                }
                vertex->checkcount = cm_checkcount;

                vec3_t relativeStart;
                for (int32_t axis = 0; axis < 3; ++axis) {
                    relativeStart[axis] = traceWork->start[axis] - vertex->position[axis];
                }
                const long double vertexConstantRaw =
                    ((long double)relativeStart[2] * relativeStart[2] + (long double)relativeStart[1] * relativeStart[1]) +
                    (long double)relativeStart[0] * relativeStart[0] - radiusSquared;
                const float vertexConstant = (float)vertexConstantRaw;
                if (vertexConstantRaw <= 0.0f) {
                    traceWork->trace.normal[0] = triangle->plane.components.normal[0];
                    traceWork->trace.normal[1] = triangle->plane.components.normal[1];
                    traceWork->trace.normal[2] = triangle->plane.components.normal[2];
                    traceWork->trace.fraction = 0.0f;
                    traceWork->trace.startsolid = qtrue;
                    return;
                }

                const long double vertexLinearRaw =
                    ((long double)relativeStart[1] * traceWork->delta[1] + (long double)relativeStart[0] * traceWork->delta[0]) +
                    (long double)relativeStart[2] * traceWork->delta[2];
                const float vertexLinear = (float)vertexLinearRaw;
                if (!(vertexLinearRaw < 0.0f || isunordered(vertexLinearRaw, 0.0f)))
                    continue;

                const long double discriminant =
                    (long double)vertexLinear * vertexLinear - (long double)traceWork->deltaLengthSquared * vertexConstant;
                if (!(discriminant >= 0.0L || isunordered(discriminant, 0.0L)))
                    continue;

                featureFraction = (-sqrtl(discriminant) - vertexLinear) / traceWork->deltaLengthSquared;
                if (!(featureFraction < traceWork->trace.fraction)) {
                    continue;
                }

                const long double inverseRadius = 1.0L / expandedRadius;
                for (int32_t axis = 0; axis < 3; ++axis) {
                    featureNormal[axis] =
                        (float)(((long double)traceWork->delta[axis] * featureFraction + relativeStart[axis]) * inverseRadius);
                }
            }

            traceWork->trace.normal[0] = featureNormal[0];
            traceWork->trace.normal[1] = featureNormal[1];
            traceWork->trace.normal[2] = featureNormal[2];
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (traceWork->trace.fraction <= fractionEpsilon) {
                traceWork->trace.fraction = 0.0f;
                traceWork->trace.startsolid = qtrue;
                return;
            }
            traceWork->trace.fraction = (float)(featureFraction - fractionEpsilon);
        }
    }
}
#elif defined(LINUX_BEHAVIOR)

#include <math.h>

enum {
    CM_TERRAIN_OUTSIDE_SUM = 1,
    CM_TERRAIN_OUTSIDE_EDGE0 = 2,
    CM_TERRAIN_OUTSIDE_EDGE1 = 4,
    CM_TERRAIN_EDGE_COUNT = 3
};

#define CM_TERRAIN_SPHERE_HIT_EPSILON 0.00001f

#if EMULATE_X87
/* NOT_FROM_ORIGINAL_SOURCE:
 * Every plane distance in the stock function (DLL 0x08055595 pattern) is one
 * 80-bit x87 chain — three products, two adds and the trailing -dist, rounded
 * once at its float slot (fmul;fmul;faddp;fmul;faddp;fsub dist;fstps). This
 * helper reproduces that exact chain so the six
 * identical dot-minus-dist sites below cannot drift by a transcription slip.
 */
static inline float coduo_collision_x87_plane_dist(float x, float y, float z, float nx, float ny, float nz, float d)
{
    return x87f_store_f32(
        x87f_sub(x87f_add(x87f_add(x87f_mul(x87f_load_f32(x), x87f_load_f32(nx)), x87f_mul(x87f_load_f32(y), x87f_load_f32(ny))),
                          x87f_mul(x87f_load_f32(z), x87f_load_f32(nz))),
                 x87f_load_f32(d)));
}
#endif

void CM_TraceSphereThroughTerrainCollide(traceWork_t *traceWork, const collisionTriangleSoup_t *terrainCollide)
{
#if EMULATE_X87
    const float capOffset = x87f_store_f32(x87f_sub(x87f_load_f32(traceWork->sphere.halfheight), x87f_load_f32(traceWork->sphere.radius)));
#else
    const float capOffset = traceWork->sphere.halfheight - traceWork->sphere.radius;
#endif
    float capDelta;

    if (terrainCollide->negativeZOnly == 0) {
#if EMULATE_X87
        capDelta = x87f_store_f32(x87f_add(x87f_load_f32(capOffset), x87f_load_f32(capOffset)));
        traceWork->start[2] = x87f_store_f32(x87f_sub(x87f_load_f32(traceWork->start[2]), x87f_load_f32(capOffset)));
        traceWork->end[2] = x87f_store_f32(x87f_sub(x87f_load_f32(traceWork->end[2]), x87f_load_f32(capOffset)));
#else
        capDelta = capOffset + capOffset;
        traceWork->start[2] -= capOffset;
        traceWork->end[2] -= capOffset;
#endif
    } else {
#if EMULATE_X87
        /* stock multiplies by the .rodata -2.0f (0x80dc924). */
        capDelta = x87f_store_f32(x87f_mul(x87f_load_f32(capOffset), x87f_load_f32(-2.0f)));
        traceWork->start[2] = x87f_store_f32(x87f_add(x87f_load_f32(traceWork->start[2]), x87f_load_f32(capOffset)));
        traceWork->end[2] = x87f_store_f32(x87f_add(x87f_load_f32(traceWork->end[2]), x87f_load_f32(capOffset)));
#else
        capDelta = capOffset * -2.0f;
        traceWork->start[2] += capOffset;
        traceWork->end[2] += capOffset;
#endif
    }

#if EMULATE_X87
    const float radius = x87f_store_f32(x87f_add(x87f_load_f32(traceWork->sphere.radius), x87f_load_f32(CM_TERRAIN_POINT_ENTER_EPSILON)));
#else
    const float radius = traceWork->sphere.radius + CM_TERRAIN_POINT_ENTER_EPSILON;
#endif
    /* stock flips the sign bit (xor 0x80000000): an exact bitwise negate. */
    const float negativeRadius = -radius;

    for (int32_t facetIndex = 0; facetIndex < (int32_t)terrainCollide->triangleCount; ++facetIndex) {
        const collisionSoupTriangle_t *facet = &terrainCollide->triangles[facetIndex];
        const plane_t *surfacePlane = &facet->plane;

#if EMULATE_X87
        const float endDistance = coduo_collision_x87_plane_dist(traceWork->end[0], traceWork->end[1], traceWork->end[2],
                                                                 surfacePlane->components.normal[0], surfacePlane->components.normal[1],
                                                                 surfacePlane->components.normal[2], surfacePlane->components.distance);
#else
        const float endDistance =
            ((traceWork->end[0] * surfacePlane->components.normal[0]) + (traceWork->end[1] * surfacePlane->components.normal[1])) +
            (traceWork->end[2] * surfacePlane->components.normal[2]) - surfacePlane->components.distance;
#endif

        /* The stock comparisons fall through on unordered operands; the negated
         * forms below preserve that branch behavior. */
        if (!(endDistance >= radius)) {
#if EMULATE_X87
            const float startDistance = coduo_collision_x87_plane_dist(
                traceWork->start[0], traceWork->start[1], traceWork->start[2], surfacePlane->components.normal[0],
                surfacePlane->components.normal[1], surfacePlane->components.normal[2], surfacePlane->components.distance);
            const float surfaceDelta = x87f_store_f32(x87f_sub(x87f_load_f32(startDistance), x87f_load_f32(endDistance)));
#else
            const float startDistance =
                ((traceWork->start[0] * surfacePlane->components.normal[0]) + (traceWork->start[1] * surfacePlane->components.normal[1])) +
                (traceWork->start[2] * surfacePlane->components.normal[2]) - surfacePlane->components.distance;
            const float surfaceDelta = startDistance - endDistance;
#endif

            if (!(0.0f >= surfaceDelta)) {
                if (startDistance <= negativeRadius) {
#if EMULATE_X87
                    const float capDistance =
                        x87f_store_f32(x87f_add(x87f_load_f32(startDistance),
                                                x87f_mul(x87f_load_f32(capDelta), x87f_load_f32(surfacePlane->components.normal[2]))));
#else
                    const float capDistance = startDistance + (capDelta * surfacePlane->components.normal[2]);
#endif

                    if (negativeRadius < capDistance) {
#if EMULATE_X87
                        float capTraceOffset =
                            x87f_store_f32(x87f_div(x87f_sub(x87f_load_f32(negativeRadius), x87f_load_f32(startDistance)),
                                                    x87f_load_f32(surfacePlane->components.normal[2])));
                        const float edge0StartDistance =
                            coduo_collision_x87_plane_dist(traceWork->start[0], traceWork->start[1], traceWork->start[2], facet->svec[0],
                                                           facet->svec[1], facet->svec[2], facet->svec[3]);
                        const float edge1StartDistance =
                            coduo_collision_x87_plane_dist(traceWork->start[0], traceWork->start[1], traceWork->start[2], facet->tvec[0],
                                                           facet->tvec[1], facet->tvec[2], facet->tvec[3]);
                        const float edge0EnterDistance = x87f_store_f32(x87f_add(
                            x87f_load_f32(edge0StartDistance), x87f_mul(x87f_load_f32(capTraceOffset), x87f_load_f32(facet->svec[2]))));
#else
                        float capTraceOffset = (negativeRadius - startDistance) / surfacePlane->components.normal[2];
                        const float edge0StartDistance = ((traceWork->start[0] * facet->svec[0]) + (traceWork->start[1] * facet->svec[1])) +
                                                         (traceWork->start[2] * facet->svec[2]) - facet->svec[3];
                        const float edge1StartDistance = ((traceWork->start[0] * facet->tvec[0]) + (traceWork->start[1] * facet->tvec[1])) +
                                                         (traceWork->start[2] * facet->tvec[2]) - facet->tvec[3];
                        const float edge0EnterDistance = edge0StartDistance + (capTraceOffset * facet->svec[2]);
#endif

                        /* 0x8055c40/0x8055c53: edge1's enter distance is
                         * computed (and rounded) only after edge0 passes. */
                        if (0.0f <= edge0EnterDistance) {
#if EMULATE_X87
                            const float edge1EnterDistance = x87f_store_f32(x87f_add(
                                x87f_load_f32(edge1StartDistance), x87f_mul(x87f_load_f32(capTraceOffset), x87f_load_f32(facet->tvec[2]))));

                            /* edge0EnterDistance + edge1EnterDistance is formed
                             * and compared in 80-bit (fadds; fld1; fucompp)
                             * with no intervening store. */
                            if (0.0f <= edge1EnterDistance &&
                                x87f_le(x87f_add(x87f_load_f32(edge0EnterDistance), x87f_load_f32(edge1EnterDistance)),
                                        x87f_load_f32(1.0f))) {
#else
                            const float edge1EnterDistance = edge1StartDistance + (capTraceOffset * facet->tvec[2]);

                            if (0.0f <= edge1EnterDistance && edge0EnterDistance + edge1EnterDistance <= 1.0f) {
#endif
                                traceWork->trace.normal[0] = surfacePlane->components.normal[0];
                                traceWork->trace.normal[1] = surfacePlane->components.normal[1];
                                traceWork->trace.normal[2] = surfacePlane->components.normal[2];
                                traceWork->trace.fraction = 0.0f;
                                traceWork->trace.startsolid = 1;
                                return;
                            }
                        }

#if EMULATE_X87
                        if (capDistance < radius) {
                            capTraceOffset = capDelta;
                        } else {
                            capTraceOffset = x87f_store_f32(x87f_div(x87f_sub(x87f_load_f32(radius), x87f_load_f32(startDistance)),
                                                                     x87f_load_f32(surfacePlane->components.normal[2])));
                        }

                        const float edge0LeaveDistance = x87f_store_f32(x87f_add(
                            x87f_load_f32(edge0StartDistance), x87f_mul(x87f_load_f32(capTraceOffset), x87f_load_f32(facet->svec[2]))));
#else
                        if (capDistance < radius) {
                            capTraceOffset = capDelta;
                        } else {
                            capTraceOffset = (radius - startDistance) / surfacePlane->components.normal[2];
                        }

                        const float edge0LeaveDistance = edge0StartDistance + (capTraceOffset * facet->svec[2]);
#endif

                        /* 0x8055d77..0x8055dcb: edge1's leave distance is
                         * computed only after edge0 passes, and the stock
                         * ja-to-next-facet tests continue on unordered, so
                         * the predicates must be the negated > forms. */
                        if (!(0.0f > edge0LeaveDistance)) {
#if EMULATE_X87
                            const float edge1LeaveDistance = x87f_store_f32(x87f_add(
                                x87f_load_f32(edge1StartDistance), x87f_mul(x87f_load_f32(capTraceOffset), x87f_load_f32(facet->tvec[2]))));

                            /* edge0LeaveDistance + edge1LeaveDistance in 80-bit
                             * (fadds; fld1; fucompp), no store. */
                            if (!(0.0f > edge1LeaveDistance) &&
                                !x87f_lt(x87f_load_f32(1.0f),
                                         x87f_add(x87f_load_f32(edge0LeaveDistance), x87f_load_f32(edge1LeaveDistance)))) {
#else
                            const float edge1LeaveDistance = edge1StartDistance + (capTraceOffset * facet->tvec[2]);

                            if (!(0.0f > edge1LeaveDistance) && !(edge0LeaveDistance + edge1LeaveDistance > 1.0f)) {
#endif
                                traceWork->trace.normal[0] = surfacePlane->components.normal[0];
                                traceWork->trace.normal[1] = surfacePlane->components.normal[1];
                                traceWork->trace.normal[2] = surfacePlane->components.normal[2];
                                traceWork->trace.fraction = 0.0f;
                                traceWork->trace.startsolid = 1;
                                return;
                            }
                        }
                    }
                } else {
                    float hitFraction;
                    vec3_t hitPoint;

                    if (startDistance < radius) {
                        hitFraction = 0.0f;
                        hitPoint[0] = traceWork->start[0];
                        hitPoint[1] = traceWork->start[1];
                        hitPoint[2] = traceWork->start[2];
                    } else {
#if EMULATE_X87
                        hitFraction = x87f_store_f32(
                            x87f_div(x87f_sub(x87f_load_f32(startDistance), x87f_load_f32(radius)), x87f_load_f32(surfaceDelta)));
#else
                        hitFraction = (startDistance - radius) / surfaceDelta;
#endif
                        if (traceWork->trace.fraction < hitFraction) {
                            goto nextFacet;
                        }
#if EMULATE_X87
                        hitPoint[0] = x87f_store_f32(x87f_add(x87f_load_f32(traceWork->start[0]),
                                                              x87f_mul(x87f_load_f32(traceWork->delta[0]), x87f_load_f32(hitFraction))));
                        hitPoint[1] = x87f_store_f32(x87f_add(x87f_load_f32(traceWork->start[1]),
                                                              x87f_mul(x87f_load_f32(traceWork->delta[1]), x87f_load_f32(hitFraction))));
                        hitPoint[2] = x87f_store_f32(x87f_add(x87f_load_f32(traceWork->start[2]),
                                                              x87f_mul(x87f_load_f32(traceWork->delta[2]), x87f_load_f32(hitFraction))));
#else
                        hitPoint[0] = traceWork->start[0] + (traceWork->delta[0] * hitFraction);
                        hitPoint[1] = traceWork->start[1] + (traceWork->delta[1] * hitFraction);
                        hitPoint[2] = traceWork->start[2] + (traceWork->delta[2] * hitFraction);
#endif
                    }

#if EMULATE_X87
                    const float edge0Distance = coduo_collision_x87_plane_dist(hitPoint[0], hitPoint[1], hitPoint[2], facet->svec[0],
                                                                               facet->svec[1], facet->svec[2], facet->svec[3]);
                    const float edge1Distance = coduo_collision_x87_plane_dist(hitPoint[0], hitPoint[1], hitPoint[2], facet->tvec[0],
                                                                               facet->tvec[1], facet->tvec[2], facet->tvec[3]);

                    /* 1.0f < edge0Distance + edge1Distance: the sum is 80-bit
                     * (fadds; fld1; fucompp; seta), no store. */
                    uint32_t outsideFlags =
                        (uint32_t)x87f_lt(x87f_load_f32(1.0f), x87f_add(x87f_load_f32(edge0Distance), x87f_load_f32(edge1Distance)));
#else
                    const float edge0Distance =
                        ((hitPoint[0] * facet->svec[0]) + (hitPoint[1] * facet->svec[1])) + (hitPoint[2] * facet->svec[2]) - facet->svec[3];
                    const float edge1Distance =
                        ((hitPoint[0] * facet->tvec[0]) + (hitPoint[1] * facet->tvec[1])) + (hitPoint[2] * facet->tvec[2]) - facet->tvec[3];

                    uint32_t outsideFlags = (uint32_t)(1.0f < edge0Distance + edge1Distance);
#endif
                    if (edge0Distance < 0.0f) {
                        outsideFlags |= CM_TERRAIN_OUTSIDE_EDGE0;
                    }
                    if (edge1Distance < 0.0f) {
                        outsideFlags |= CM_TERRAIN_OUTSIDE_EDGE1;
                    }

                    if (outsideFlags == 0U) {
                        traceWork->trace.normal[0] = surfacePlane->components.normal[0];
                        traceWork->trace.normal[1] = surfacePlane->components.normal[1];
                        traceWork->trace.normal[2] = surfacePlane->components.normal[2];
                        if (hitFraction <= CM_TERRAIN_SPHERE_HIT_EPSILON) {
                            traceWork->trace.fraction = 0.0f;
                            traceWork->trace.startsolid = 1;
                            return;
                        }
#if EMULATE_X87
                        traceWork->trace.fraction =
                            x87f_store_f32(x87f_sub(x87f_load_f32(hitFraction), x87f_load_f32(CM_TERRAIN_SPHERE_HIT_EPSILON)));
#else
                        traceWork->trace.fraction = hitFraction - CM_TERRAIN_SPHERE_HIT_EPSILON;
#endif
                    } else {
                        for (int32_t edgeIndex = 0; edgeIndex < CM_TERRAIN_EDGE_COUNT; ++edgeIndex) {
                            if (((outsideFlags >> edgeIndex) & 1U) == 0U) {
                                collisionSoupVertex_t *vertexSphere = facet->vertices[edgeIndex];

                                if (vertexSphere != NULL && vertexSphere->checkcount != cm_checkcount) {
                                    vertexSphere->checkcount = cm_checkcount;

#if EMULATE_X87
                                    const float deltaX = x87f_store_f32(
                                        x87f_sub(x87f_load_f32(traceWork->start[0]), x87f_load_f32(vertexSphere->position[0])));
                                    const float deltaY = x87f_store_f32(
                                        x87f_sub(x87f_load_f32(traceWork->start[1]), x87f_load_f32(vertexSphere->position[1])));
                                    const float deltaZ = x87f_store_f32(
                                        x87f_sub(x87f_load_f32(traceWork->start[2]), x87f_load_f32(vertexSphere->position[2])));
                                    /* (dx*dx + dy*dy) + dz*dz - radius*radius,
                                     * one 80-bit chain (fsubrp for the -r*r). */
                                    const float radiusDelta =
                                        x87f_store_f32(x87f_sub(x87f_add(x87f_add(x87f_mul(x87f_load_f32(deltaX), x87f_load_f32(deltaX)),
                                                                                  x87f_mul(x87f_load_f32(deltaY), x87f_load_f32(deltaY))),
                                                                         x87f_mul(x87f_load_f32(deltaZ), x87f_load_f32(deltaZ))),
                                                                x87f_mul(x87f_load_f32(radius), x87f_load_f32(radius))));
#else
                                    const float deltaX = traceWork->start[0] - vertexSphere->position[0];
                                    const float deltaY = traceWork->start[1] - vertexSphere->position[1];
                                    const float deltaZ = traceWork->start[2] - vertexSphere->position[2];
                                    const float radiusDelta =
                                        ((deltaX * deltaX) + (deltaY * deltaY)) + (deltaZ * deltaZ) - (radius * radius);
#endif

                                    if (radiusDelta <= 0.0f) {
                                        traceWork->trace.normal[0] = surfacePlane->components.normal[0];
                                        traceWork->trace.normal[1] = surfacePlane->components.normal[1];
                                        traceWork->trace.normal[2] = surfacePlane->components.normal[2];
                                        traceWork->trace.fraction = 0.0f;
                                        traceWork->trace.startsolid = 1;
                                        return;
                                    }

#if EMULATE_X87
                                    const float velocityDot = x87f_store_f32(
                                        x87f_add(x87f_add(x87f_mul(x87f_load_f32(traceWork->delta[0]), x87f_load_f32(deltaX)),
                                                          x87f_mul(x87f_load_f32(traceWork->delta[1]), x87f_load_f32(deltaY))),
                                                 x87f_mul(x87f_load_f32(traceWork->delta[2]), x87f_load_f32(deltaZ))));
#else
                                    const float velocityDot =
                                        ((traceWork->delta[0] * deltaX) + (traceWork->delta[1] * deltaY)) + (traceWork->delta[2] * deltaZ);
#endif

                                    if (velocityDot < 0.0f) {
#if EMULATE_X87
                                        /* velocityDot*velocityDot -
                                         * deltaLengthSquared*radiusDelta,
                                         * one 80-bit chain. */
                                        const float discriminant = x87f_store_f32(
                                            x87f_sub(x87f_mul(x87f_load_f32(velocityDot), x87f_load_f32(velocityDot)),
                                                     x87f_mul(x87f_load_f32(traceWork->deltaLengthSquared), x87f_load_f32(radiusDelta))));
#else
                                        const float discriminant =
                                            (velocityDot * velocityDot) - (traceWork->deltaLengthSquared * radiusDelta);
#endif

                                        if (0.0f <= discriminant) {
#if EMULATE_X87
                                            /* sqrt takes (double)discriminant
                                             * (fstpl), returns in st0; the
                                             * negate/subtract/divide then run
                                             * in 80-bit (fchs;fsubrp;fdivrp). */
                                            const float vertexFraction = x87f_store_f32(x87f_div(
                                                x87f_sub(x87f_neg(x87f_load_f64(sqrt(x87f_store_f64(x87f_load_f32(discriminant))))),
                                                         x87f_load_f32(velocityDot)),
                                                x87f_load_f32(traceWork->deltaLengthSquared)));
#else
                                            const float vertexFraction =
                                                (-sqrt(discriminant) - velocityDot) / traceWork->deltaLengthSquared;
#endif

                                            if (vertexFraction < traceWork->trace.fraction) {
#if EMULATE_X87
                                                traceWork->trace.normal[0] = x87f_store_f32(
                                                    x87f_add(x87f_load_f32(deltaX),
                                                             x87f_mul(x87f_load_f32(traceWork->delta[0]), x87f_load_f32(vertexFraction))));
                                                traceWork->trace.normal[1] = x87f_store_f32(
                                                    x87f_add(x87f_load_f32(deltaY),
                                                             x87f_mul(x87f_load_f32(traceWork->delta[1]), x87f_load_f32(vertexFraction))));
                                                traceWork->trace.normal[2] = x87f_store_f32(
                                                    x87f_add(x87f_load_f32(deltaZ),
                                                             x87f_mul(x87f_load_f32(traceWork->delta[2]), x87f_load_f32(vertexFraction))));
                                                /* stock computes the reciprocal
                                                 * 1.0f/radius, then multiplies
                                                 * each component (fld1;fdivs;
                                                 * fmulp;fstps) — one store. */
                                                traceWork->trace.normal[0] =
                                                    x87f_store_f32(x87f_mul(x87f_div(x87f_load_f32(1.0f), x87f_load_f32(radius)),
                                                                            x87f_load_f32(traceWork->trace.normal[0])));
                                                traceWork->trace.normal[1] =
                                                    x87f_store_f32(x87f_mul(x87f_div(x87f_load_f32(1.0f), x87f_load_f32(radius)),
                                                                            x87f_load_f32(traceWork->trace.normal[1])));
                                                traceWork->trace.normal[2] =
                                                    x87f_store_f32(x87f_mul(x87f_div(x87f_load_f32(1.0f), x87f_load_f32(radius)),
                                                                            x87f_load_f32(traceWork->trace.normal[2])));
#else
                                                traceWork->trace.normal[0] = deltaX + (traceWork->delta[0] * vertexFraction);
                                                traceWork->trace.normal[1] = deltaY + (traceWork->delta[1] * vertexFraction);
                                                traceWork->trace.normal[2] = deltaZ + (traceWork->delta[2] * vertexFraction);
                                                traceWork->trace.normal[0] *= 1.0f / radius;
                                                traceWork->trace.normal[1] *= 1.0f / radius;
                                                traceWork->trace.normal[2] *= 1.0f / radius;
#endif

                                                if (traceWork->trace.fraction <= CM_TERRAIN_SPHERE_HIT_EPSILON) {
                                                    traceWork->trace.fraction = 0.0f;
                                                    traceWork->trace.startsolid = 1;
                                                    return;
                                                }
#if EMULATE_X87
                                                traceWork->trace.fraction = x87f_store_f32(
                                                    x87f_sub(x87f_load_f32(vertexFraction), x87f_load_f32(CM_TERRAIN_SPHERE_HIT_EPSILON)));
#else
                                                traceWork->trace.fraction = vertexFraction - CM_TERRAIN_SPHERE_HIT_EPSILON;
#endif
                                            }
                                        }
                                    }
                                }
                            } else {
                                collisionSoupEdge_t *edgeCylinder = facet->oppositeEdges[edgeIndex];

                                if (edgeCylinder != NULL && edgeCylinder->checkcount != cm_checkcount) {
                                    edgeCylinder->checkcount = cm_checkcount;

#if EMULATE_X87
                                    const float deltaX = x87f_store_f32(
                                        x87f_sub(x87f_load_f32(traceWork->start[0]), x87f_load_f32(edgeCylinder->origin[0])));
                                    const float deltaY = x87f_store_f32(
                                        x87f_sub(x87f_load_f32(traceWork->start[1]), x87f_load_f32(edgeCylinder->origin[1])));
                                    const float deltaZ = x87f_store_f32(
                                        x87f_sub(x87f_load_f32(traceWork->start[2]), x87f_load_f32(edgeCylinder->origin[2])));
                                    const float radial0 = x87f_store_f32(
                                        x87f_add(x87f_add(x87f_mul(x87f_load_f32(deltaX), x87f_load_f32(edgeCylinder->radialAxes[0][0])),
                                                          x87f_mul(x87f_load_f32(deltaY), x87f_load_f32(edgeCylinder->radialAxes[0][1]))),
                                                 x87f_mul(x87f_load_f32(deltaZ), x87f_load_f32(edgeCylinder->radialAxes[0][2]))));
                                    const float radial1 = x87f_store_f32(
                                        x87f_add(x87f_add(x87f_mul(x87f_load_f32(deltaX), x87f_load_f32(edgeCylinder->radialAxes[1][0])),
                                                          x87f_mul(x87f_load_f32(deltaY), x87f_load_f32(edgeCylinder->radialAxes[1][1]))),
                                                 x87f_mul(x87f_load_f32(deltaZ), x87f_load_f32(edgeCylinder->radialAxes[1][2]))));
                                    float axial = x87f_store_f32(
                                        x87f_add(x87f_add(x87f_mul(x87f_load_f32(deltaX), x87f_load_f32(edgeCylinder->unitDirection[0])),
                                                          x87f_mul(x87f_load_f32(deltaY), x87f_load_f32(edgeCylinder->unitDirection[1]))),
                                                 x87f_mul(x87f_load_f32(deltaZ), x87f_load_f32(edgeCylinder->unitDirection[2]))));
                                    /* (radial0*radial0 + radial1*radial1) -
                                     * radius*radius, one 80-bit chain. */
                                    const float radiusDelta =
                                        x87f_store_f32(x87f_sub(x87f_add(x87f_mul(x87f_load_f32(radial0), x87f_load_f32(radial0)),
                                                                         x87f_mul(x87f_load_f32(radial1), x87f_load_f32(radial1))),
                                                                x87f_mul(x87f_load_f32(radius), x87f_load_f32(radius))));
#else
                                    const float deltaX = traceWork->start[0] - edgeCylinder->origin[0];
                                    const float deltaY = traceWork->start[1] - edgeCylinder->origin[1];
                                    const float deltaZ = traceWork->start[2] - edgeCylinder->origin[2];
                                    const float radial0 =
                                        ((deltaX * edgeCylinder->radialAxes[0][0]) + (deltaY * edgeCylinder->radialAxes[0][1])) +
                                        (deltaZ * edgeCylinder->radialAxes[0][2]);
                                    const float radial1 =
                                        ((deltaX * edgeCylinder->radialAxes[1][0]) + (deltaY * edgeCylinder->radialAxes[1][1])) +
                                        (deltaZ * edgeCylinder->radialAxes[1][2]);
                                    float axial = ((deltaX * edgeCylinder->unitDirection[0]) + (deltaY * edgeCylinder->unitDirection[1])) +
                                                  (deltaZ * edgeCylinder->unitDirection[2]);
                                    const float radiusDelta = ((radial0 * radial0) + (radial1 * radial1)) - (radius * radius);
#endif

                                    if (radiusDelta <= 0.0f) {
                                        if (0.0f <= axial && axial <= edgeCylinder->length) {
                                            traceWork->trace.normal[0] = surfacePlane->components.normal[0];
                                            traceWork->trace.normal[1] = surfacePlane->components.normal[1];
                                            traceWork->trace.normal[2] = surfacePlane->components.normal[2];
                                            traceWork->trace.fraction = 0.0f;
                                            traceWork->trace.startsolid = 1;
                                            return;
                                        }
                                    } else {
#if EMULATE_X87
                                        const float velocity0 = x87f_store_f32(x87f_add(
                                            x87f_add(
                                                x87f_mul(x87f_load_f32(traceWork->delta[0]), x87f_load_f32(edgeCylinder->radialAxes[0][0])),
                                                x87f_mul(x87f_load_f32(traceWork->delta[1]),
                                                         x87f_load_f32(edgeCylinder->radialAxes[0][1]))),
                                            x87f_mul(x87f_load_f32(traceWork->delta[2]), x87f_load_f32(edgeCylinder->radialAxes[0][2]))));
                                        const float velocity1 = x87f_store_f32(x87f_add(
                                            x87f_add(
                                                x87f_mul(x87f_load_f32(traceWork->delta[0]), x87f_load_f32(edgeCylinder->radialAxes[1][0])),
                                                x87f_mul(x87f_load_f32(traceWork->delta[1]),
                                                         x87f_load_f32(edgeCylinder->radialAxes[1][1]))),
                                            x87f_mul(x87f_load_f32(traceWork->delta[2]), x87f_load_f32(edgeCylinder->radialAxes[1][2]))));
                                        /* velocity0*radial0 +
                                         * velocity1*radial1, one 80-bit
                                         * chain. */
                                        const float velocityDot =
                                            x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(velocity0), x87f_load_f32(radial0)),
                                                                    x87f_mul(x87f_load_f32(velocity1), x87f_load_f32(radial1))));
#else
                                        const float velocity0 = ((traceWork->delta[0] * edgeCylinder->radialAxes[0][0]) +
                                                                 (traceWork->delta[1] * edgeCylinder->radialAxes[0][1])) +
                                                                (traceWork->delta[2] * edgeCylinder->radialAxes[0][2]);
                                        const float velocity1 = ((traceWork->delta[0] * edgeCylinder->radialAxes[1][0]) +
                                                                 (traceWork->delta[1] * edgeCylinder->radialAxes[1][1])) +
                                                                (traceWork->delta[2] * edgeCylinder->radialAxes[1][2]);
                                        const float velocityDot = (velocity0 * radial0) + (velocity1 * radial1);
#endif

                                        if (velocityDot < 0.0f) {
#if EMULATE_X87
                                            const float velocityLengthSquared =
                                                x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(velocity0), x87f_load_f32(velocity0)),
                                                                        x87f_mul(x87f_load_f32(velocity1), x87f_load_f32(velocity1))));
                                            /* velocityDot*velocityDot -
                                             * velocityLengthSquared*radiusDelta,
                                             * one 80-bit chain. */
                                            const float discriminant = x87f_store_f32(
                                                x87f_sub(x87f_mul(x87f_load_f32(velocityDot), x87f_load_f32(velocityDot)),
                                                         x87f_mul(x87f_load_f32(velocityLengthSquared), x87f_load_f32(radiusDelta))));
#else
                                            const float velocityLengthSquared = (velocity0 * velocity0) + (velocity1 * velocity1);
                                            const float discriminant = (velocityDot * velocityDot) - (velocityLengthSquared * radiusDelta);
#endif

                                            if (0.0f < discriminant) {
#if EMULATE_X87
                                                const float cylinderFraction = x87f_store_f32(x87f_div(
                                                    x87f_sub(x87f_neg(x87f_load_f64(sqrt(x87f_store_f64(x87f_load_f32(discriminant))))),
                                                             x87f_load_f32(velocityDot)),
                                                    x87f_load_f32(velocityLengthSquared)));
#else
                                                const float cylinderFraction = (-sqrt(discriminant) - velocityDot) / velocityLengthSquared;
#endif

                                                if (cylinderFraction < traceWork->trace.fraction) {
#if EMULATE_X87
                                                    const float axialVelocity = x87f_store_f32(
                                                        x87f_add(x87f_add(x87f_mul(x87f_load_f32(traceWork->delta[0]),
                                                                                   x87f_load_f32(edgeCylinder->unitDirection[0])),
                                                                          x87f_mul(x87f_load_f32(traceWork->delta[1]),
                                                                                   x87f_load_f32(edgeCylinder->unitDirection[1]))),
                                                                 x87f_mul(x87f_load_f32(traceWork->delta[2]),
                                                                          x87f_load_f32(edgeCylinder->unitDirection[2]))));
                                                    axial = x87f_store_f32(
                                                        x87f_add(x87f_load_f32(axial),
                                                                 x87f_mul(x87f_load_f32(cylinderFraction), x87f_load_f32(axialVelocity))));
#else
                                                    const float axialVelocity = ((traceWork->delta[0] * edgeCylinder->unitDirection[0]) +
                                                                                 (traceWork->delta[1] * edgeCylinder->unitDirection[1])) +
                                                                                (traceWork->delta[2] * edgeCylinder->unitDirection[2]);
                                                    axial += cylinderFraction * axialVelocity;
#endif

                                                    if (0.0f <= axial && axial <= edgeCylinder->length) {
#if EMULATE_X87
                                                        /* ((cf*velocity0) +
                                                         * radial0) / radius, one
                                                         * 80-bit chain (fdivs,
                                                         * not a reciprocal). */
                                                        const float normal0 = x87f_store_f32(x87f_div(
                                                            x87f_add(x87f_mul(x87f_load_f32(cylinderFraction), x87f_load_f32(velocity0)),
                                                                     x87f_load_f32(radial0)),
                                                            x87f_load_f32(radius)));
                                                        const float normal1 = x87f_store_f32(x87f_div(
                                                            x87f_add(x87f_mul(x87f_load_f32(cylinderFraction), x87f_load_f32(velocity1)),
                                                                     x87f_load_f32(radial1)),
                                                            x87f_load_f32(radius)));

                                                        traceWork->trace.normal[0] = x87f_store_f32(x87f_mul(
                                                            x87f_load_f32(edgeCylinder->radialAxes[0][0]), x87f_load_f32(normal0)));
                                                        traceWork->trace.normal[1] = x87f_store_f32(x87f_mul(
                                                            x87f_load_f32(edgeCylinder->radialAxes[0][1]), x87f_load_f32(normal0)));
                                                        traceWork->trace.normal[2] = x87f_store_f32(x87f_mul(
                                                            x87f_load_f32(edgeCylinder->radialAxes[0][2]), x87f_load_f32(normal0)));
                                                        traceWork->trace.normal[0] =
                                                            x87f_store_f32(x87f_add(x87f_load_f32(traceWork->trace.normal[0]),
                                                                                    x87f_mul(x87f_load_f32(edgeCylinder->radialAxes[1][0]),
                                                                                             x87f_load_f32(normal1))));
                                                        traceWork->trace.normal[1] =
                                                            x87f_store_f32(x87f_add(x87f_load_f32(traceWork->trace.normal[1]),
                                                                                    x87f_mul(x87f_load_f32(edgeCylinder->radialAxes[1][1]),
                                                                                             x87f_load_f32(normal1))));
                                                        traceWork->trace.normal[2] =
                                                            x87f_store_f32(x87f_add(x87f_load_f32(traceWork->trace.normal[2]),
                                                                                    x87f_mul(x87f_load_f32(edgeCylinder->radialAxes[1][2]),
                                                                                             x87f_load_f32(normal1))));
#else
                                                        const float normal0 = ((cylinderFraction * velocity0) + radial0) / radius;
                                                        const float normal1 = ((cylinderFraction * velocity1) + radial1) / radius;

                                                        traceWork->trace.normal[0] = edgeCylinder->radialAxes[0][0] * normal0;
                                                        traceWork->trace.normal[1] = edgeCylinder->radialAxes[0][1] * normal0;
                                                        traceWork->trace.normal[2] = edgeCylinder->radialAxes[0][2] * normal0;
                                                        traceWork->trace.normal[0] += edgeCylinder->radialAxes[1][0] * normal1;
                                                        traceWork->trace.normal[1] += edgeCylinder->radialAxes[1][1] * normal1;
                                                        traceWork->trace.normal[2] += edgeCylinder->radialAxes[1][2] * normal1;
#endif

                                                        if (traceWork->trace.fraction <= CM_TERRAIN_SPHERE_HIT_EPSILON) {
                                                            traceWork->trace.fraction = 0.0f;
                                                            traceWork->trace.startsolid = 1;
                                                            return;
                                                        }
#if EMULATE_X87
                                                        traceWork->trace.fraction = x87f_store_f32(x87f_sub(
                                                            x87f_load_f32(cylinderFraction), x87f_load_f32(CM_TERRAIN_SPHERE_HIT_EPSILON)));
#else
                                                        traceWork->trace.fraction = cylinderFraction - CM_TERRAIN_SPHERE_HIT_EPSILON;
#endif
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

    nextFacet:;
    }
}
#else
#error "collision_terrain_sphere.c requires a target behavior"
#endif
