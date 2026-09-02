#include "collision_terrain_trace.h"

#include <math.h>

/*
 * Capsule-cap position test against triangle soup:
 *
 *   CoDUOMP.exe  0x00425120..0x0042550f
 *   coduo_lnxded 0x08056773..0x08056f56
 *
 * The complete authoritative bodies retain their original operation graphs.
 */
#if defined(WINDOWS_BEHAVIOR)
/* Source: CoDUOMP.exe 0x00425120..0x0042550f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00425120_0042550f.mcode.
 * Name: exact same-module Mac symbol
 * CM_PositionTestSphereWithTerrainCollide. The selected capsule cap is tested
 * against each triangle face; projections outside the face are reduced to
 * the implicated edge or vertex before the radius test. */
qboolean CM_PositionTestSphereWithTerrainCollide(traceWork_t *traceWork, const collisionTriangleSoup_t *terrainCollide)
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

    const float radius = traceWork->sphere.radius;
    const float negativeRadius = -radius;

    for (int32_t triangleIndex = 0; triangleIndex < (int32_t)terrainCollide->triangleCount; ++triangleIndex) {
        const collisionSoupTriangle_t *const triangle = &terrainCollide->triangles[triangleIndex];
        const long double planeDistanceRaw = ((long double)traceWork->start[2] * triangle->plane.components.normal[2] +
                                              (long double)traceWork->start[1] * triangle->plane.components.normal[1]) +
                                             (long double)traceWork->start[0] * triangle->plane.components.normal[0] -
                                             triangle->plane.components.distance;
        const float planeDistance = (float)planeDistanceRaw;

        if (!(planeDistanceRaw < radius || isunordered(planeDistanceRaw, radius)))
            continue;

        if (!(planeDistance > negativeRadius || isunordered(planeDistance, negativeRadius))) {
            const long double oppositeCapDistance = (long double)verticalSpan * triangle->plane.components.normal[2] + planeDistance;
            if (!(oppositeCapDistance > negativeRadius || isunordered(oppositeCapDistance, negativeRadius))) {
                continue;
            }

            const float inverseNormalZ = 1.0f / triangle->plane.components.normal[2];
            const float sAtCap =
                (float)(((long double)triangle->svec[2] * traceWork->start[2] + (long double)triangle->svec[1] * traceWork->start[1]) +
                        (long double)triangle->svec[0] * traceWork->start[0] - triangle->svec[3]);
            const float tAtCap =
                (float)(((long double)triangle->tvec[2] * traceWork->start[2] + (long double)triangle->tvec[1] * traceWork->start[1]) +
                        (long double)triangle->tvec[0] * traceWork->start[0] - triangle->tvec[3]);

            const long double firstZOffset = ((long double)negativeRadius - planeDistance) * inverseNormalZ;
            const long double firstSRaw = firstZOffset * triangle->svec[2] + sAtCap;
            const float firstS = (float)firstSRaw;
            const long double firstT = firstZOffset * triangle->tvec[2] + tAtCap;
            if (firstSRaw >= 0.0f && firstT >= 0.0f && (long double)firstS + firstT <= 1.0f) {
                return qtrue;
            }

            /* The second sample is clipped at the positive-radius plane when
             * the opposite cap reaches or crosses it; only a shorter segment
             * uses the cap endpoint itself (0x0042529c..0x004252b9). */
            const long double secondZOffset = !isunordered(oppositeCapDistance, radius) && oppositeCapDistance < radius
                                                  ? (long double)verticalSpan
                                                  : ((long double)radius - planeDistance) * inverseNormalZ;
            const long double secondSRaw = secondZOffset * triangle->svec[2] + sAtCap;
            const float secondS = (float)secondSRaw;
            const long double secondT = secondZOffset * triangle->tvec[2] + tAtCap;
            if (secondSRaw >= 0.0f && secondT >= 0.0f && (long double)secondS + secondT <= 1.0f) {
                return qtrue;
            }
            continue;
        }

        vec3_t projectedPoint;
        for (int32_t axis = 0; axis < 2; ++axis) {
            projectedPoint[axis] =
                (float)((long double)traceWork->start[axis] - (long double)planeDistance * triangle->plane.components.normal[axis]);
        }
        const long double projectedPointZRaw =
            (long double)traceWork->start[2] - (long double)planeDistance * triangle->plane.components.normal[2];
        projectedPoint[2] = (float)projectedPointZRaw;

        const long double sCoordinate = (projectedPointZRaw * triangle->svec[2] + (long double)projectedPoint[1] * triangle->svec[1]) +
                                        (long double)projectedPoint[0] * triangle->svec[0] - triangle->svec[3];
        const long double tCoordinateRaw =
            ((long double)projectedPoint[2] * triangle->tvec[2] + (long double)projectedPoint[1] * triangle->tvec[1]) +
            (long double)projectedPoint[0] * triangle->tvec[0] - triangle->tvec[3];
        const float tCoordinate = (float)tCoordinateRaw;

        int32_t outsideMask = 0;
        if (sCoordinate + tCoordinateRaw > 1.0f) {
            outsideMask |= 1;
        }
        if (sCoordinate < 0.0f)
            outsideMask |= 2;
        if (tCoordinate < 0.0f)
            outsideMask |= 4;

        if (outsideMask == 0)
            return qtrue;

        const long double radiusSquared = (long double)radius * radius;
        for (int32_t corner = 0; corner < CM_TRIANGLE_VERTEX_COUNT; ++corner) {
            if ((outsideMask & (1 << corner)) != 0) {
                const collisionSoupEdge_t *const edge = triangle->oppositeEdges[corner];
                if (edge == NULL)
                    continue;

                vec3_t edgeDelta;
                for (int32_t axis = 0; axis < 3; ++axis) {
                    edgeDelta[axis] = traceWork->start[axis] - edge->origin[axis];
                }
                const long double edgeDistanceRaw =
                    ((long double)edgeDelta[2] * edge->unitDirection[2] + (long double)edgeDelta[1] * edge->unitDirection[1]) +
                    (long double)edgeDelta[0] * edge->unitDirection[0];
                const float edgeDistance = (float)edgeDistanceRaw;
                if (edgeDistanceRaw < 0.0f || edgeDistance > edge->length) {
                    continue;
                }

                const long double closestX = (long double)edgeDelta[0] - (long double)edgeDistance * edge->unitDirection[0];
                const long double closestY = (long double)edgeDelta[1] - (long double)edgeDistance * edge->unitDirection[1];
                const long double closestZ = (long double)edgeDelta[2] - (long double)edgeDistance * edge->unitDirection[2];
                if (closestX * closestX + closestY * closestY + closestZ * closestZ < radiusSquared) {
                    return qtrue;
                }
            } else {
                const collisionSoupVertex_t *const vertex = triangle->vertices[corner];
                if (vertex == NULL)
                    continue;

                const long double deltaX = (long double)traceWork->start[0] - vertex->position[0];
                const long double deltaY = (long double)traceWork->start[1] - vertex->position[1];
                const long double deltaZ = (long double)traceWork->start[2] - vertex->position[2];
                if (deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ < radiusSquared) {
                    return qtrue;
                }
            }
        }
    }

    return qfalse;
}
#elif defined(LINUX_BEHAVIOR)

enum {
    CM_TERRAIN_OUTSIDE_SUM = 1,
    CM_TERRAIN_OUTSIDE_EDGE0 = 2,
    CM_TERRAIN_OUTSIDE_EDGE1 = 4,
    CM_TERRAIN_EDGE_COUNT = 3
};

qboolean CM_PositionTestSphereWithTerrainCollide(traceWork_t *traceWork, const collisionTriangleSoup_t *terrainCollide)
{
    const float radius = traceWork->sphere.radius;
    const float negativeRadius = -radius;
    const float capOffset = traceWork->sphere.halfheight - radius;
    float capDelta;

    if (terrainCollide->negativeZOnly == 0) {
        capDelta = capOffset + capOffset;
        traceWork->start[2] -= capOffset;
        traceWork->end[2] -= capOffset;
    } else {
        capDelta = capOffset * -2.0f;
        traceWork->start[2] += capOffset;
        traceWork->end[2] += capOffset;
    }

    for (int32_t facetIndex = 0; facetIndex < (int32_t)terrainCollide->triangleCount; ++facetIndex) {
        const collisionSoupTriangle_t *facet = &terrainCollide->triangles[facetIndex];
        const plane_t *surfacePlane = &facet->plane;

        const float surfaceDistance =
            ((traceWork->start[0] * surfacePlane->components.normal[0]) + (traceWork->start[1] * surfacePlane->components.normal[1])) +
            (traceWork->start[2] * surfacePlane->components.normal[2]) - surfacePlane->components.distance;

        if (!(surfaceDistance >= radius)) {
            if (surfaceDistance <= negativeRadius) {
                const float capDistance = surfaceDistance + (capDelta * surfacePlane->components.normal[2]);

                if (!(negativeRadius >= capDistance)) {
                    const float enterOffset = (negativeRadius - surfaceDistance) / surfacePlane->components.normal[2];
                    const vec4_t *edgePlane = &facet->svec;
                    const float baseEdge0Distance = ((traceWork->start[0] * (*edgePlane)[0]) + (traceWork->start[1] * (*edgePlane)[1])) +
                                                    (traceWork->start[2] * (*edgePlane)[2]) - (*edgePlane)[3];

                    edgePlane = &facet->tvec;
                    const float baseEdge1Distance = ((traceWork->start[0] * (*edgePlane)[0]) + (traceWork->start[1] * (*edgePlane)[1])) +
                                                    (traceWork->start[2] * (*edgePlane)[2]) - (*edgePlane)[3];

                    edgePlane = &facet->svec;
                    const float enterEdge0Distance = baseEdge0Distance + (enterOffset * (*edgePlane)[2]);
                    if (enterEdge0Distance >= 0.0f) {
                        edgePlane = &facet->tvec;
                        const float enterEdge1Distance = baseEdge1Distance + (enterOffset * (*edgePlane)[2]);

                        if (enterEdge1Distance >= 0.0f && enterEdge0Distance + enterEdge1Distance <= 1.0f) {
                            return 1;
                        }
                    }

                    float leaveOffset;
                    if (radius > capDistance) {
                        leaveOffset = capDelta;
                    } else {
                        leaveOffset = (radius - surfaceDistance) / surfacePlane->components.normal[2];
                    }

                    edgePlane = &facet->svec;
                    const float leaveEdge0Distance = baseEdge0Distance + (leaveOffset * (*edgePlane)[2]);

                    if (!(0.0f > leaveEdge0Distance)) {
                        edgePlane = &facet->tvec;
                        const float leaveEdge1Distance = baseEdge1Distance + (leaveOffset * (*edgePlane)[2]);

                        if (!(0.0f > leaveEdge1Distance) && !(leaveEdge0Distance + leaveEdge1Distance > 1.0f)) {
                            return 1;
                        }
                    }
                }
            } else {
                const float projectedX = traceWork->start[0] - (surfaceDistance * surfacePlane->components.normal[0]);
                const float projectedY = traceWork->start[1] - (surfaceDistance * surfacePlane->components.normal[1]);
                const float projectedZ = traceWork->start[2] - (surfaceDistance * surfacePlane->components.normal[2]);

                const vec4_t *edgePlane = &facet->svec;
                const float edge0Distance =
                    ((projectedX * (*edgePlane)[0]) + (projectedY * (*edgePlane)[1])) + (projectedZ * (*edgePlane)[2]) - (*edgePlane)[3];

                edgePlane = &facet->tvec;
                const float edge1Distance =
                    ((projectedX * (*edgePlane)[0]) + (projectedY * (*edgePlane)[1])) + (projectedZ * (*edgePlane)[2]) - (*edgePlane)[3];

                uint32_t outsideFlags = 0;
                if (edge0Distance + edge1Distance > 1.0f) {
                    outsideFlags |= CM_TERRAIN_OUTSIDE_SUM;
                }
                if (edge0Distance < 0.0f) {
                    outsideFlags |= CM_TERRAIN_OUTSIDE_EDGE0;
                }
                if (edge1Distance < 0.0f) {
                    outsideFlags |= CM_TERRAIN_OUTSIDE_EDGE1;
                }

                if (outsideFlags == 0) {
                    return 1;
                }

                for (int32_t edgeIndex = 0; edgeIndex < CM_TERRAIN_EDGE_COUNT; ++edgeIndex) {
                    if (((outsideFlags >> edgeIndex) & 1U) != 0) {
                        const collisionSoupEdge_t *edgeCylinder = facet->oppositeEdges[edgeIndex];

                        if (edgeCylinder != NULL) {
                            float deltaX = traceWork->start[0] - edgeCylinder->origin[0];
                            float deltaY = traceWork->start[1] - edgeCylinder->origin[1];
                            float deltaZ = traceWork->start[2] - edgeCylinder->origin[2];
                            const float axisDistance =
                                ((deltaX * edgeCylinder->unitDirection[0]) + (deltaY * edgeCylinder->unitDirection[1])) +
                                (deltaZ * edgeCylinder->unitDirection[2]);

                            if (axisDistance >= 0.0f && axisDistance <= edgeCylinder->length) {
                                deltaX -= axisDistance * edgeCylinder->unitDirection[0];
                                deltaY -= axisDistance * edgeCylinder->unitDirection[1];
                                deltaZ -= axisDistance * edgeCylinder->unitDirection[2];

                                if (((deltaX * deltaX) + (deltaY * deltaY)) + (deltaZ * deltaZ) < radius * radius) {
                                    return 1;
                                }
                            }
                        }
                    } else {
                        const collisionSoupVertex_t *vertexSphere = facet->vertices[edgeIndex];

                        if (vertexSphere != NULL) {
                            const float deltaX = traceWork->start[0] - vertexSphere->position[0];
                            const float deltaY = traceWork->start[1] - vertexSphere->position[1];
                            const float deltaZ = traceWork->start[2] - vertexSphere->position[2];

                            if (((deltaX * deltaX) + (deltaY * deltaY)) + (deltaZ * deltaZ) < radius * radius) {
                                return 1;
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}
#else
#error "collision_terrain_position.c requires a target behavior"
#endif
