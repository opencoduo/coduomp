#include "backend.h"

#include "math/q_math.h"

#include <string.h>

typedef enum renderer_mark_plane_side_e {
    R_MARK_SIDE_FRONT = 0,
    R_MARK_SIDE_BACK = 1,
    R_MARK_SIDE_ON = 2,
    R_MARK_SIDE_COUNT = 3
} renderer_mark_plane_side_t;

enum renderer_box_plane_side_e {
    R_BOX_SIDE_FRONT = 1,
    R_BOX_SIDE_BACK = 2
};

enum renderer_mark_limits_e {
    R_MARK_MAX_WORLD_SURFACES = 4096,
    R_MARK_EXTRA_CLIP_PLANES = 2,
    R_MARK_SHADER_USAGE = 9
};

/* Original 0x0389c578..0x0389c587. RE_MarkFragments installs the negative
 * projection direction while gathering world surfaces, then leaves the plane
 * facing along the projection direction after the traversal. */
plane_t rendererMarkProjectionPlane;

/* Source: CoDUOMP.exe 0x005161c0..0x0051641b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005161c0_0051641c.mcode.
 * Same-module Mac R_ChopPolyBehindPlane independently proves the seven-argument
 * source signature and 20-byte vertex stride. Windows instructions prove the
 * 62-point guard, three-way epsilon classification, whole-polygon fast path,
 * cyclic edge walk, and interpolation of all five floats. */
void R_ChopPolyBehindPlane(
    int32_t inPointCount, const renderer_mark_clip_vertex_t *inPoints,
    int32_t *outPointCount, renderer_mark_clip_vertex_t *outPoints,
    const vec3_t planeNormal, float planeDistance, float epsilon)
{
    float distances[R_MARK_CLIP_MAX_VERTICES];
    renderer_mark_plane_side_t sides[R_MARK_CLIP_MAX_VERTICES];
    int32_t sideCounts[R_MARK_SIDE_COUNT] = {0, 0, 0};
    int32_t pointIndex;

    if (inPointCount >= R_MARK_CLIP_MAX_VERTICES - 2) {
        *outPointCount = 0;
        return;
    }

    if (inPointCount <= 0) {
        *outPointCount = 0;
        return;
    }

    for (pointIndex = 0; pointIndex < inPointCount; ++pointIndex) {
        const renderer_mark_clip_vertex_t *point = &inPoints[pointIndex];
        const long double distanceRaw =
            ((long double)point->xyz[2] * planeNormal[2] +
             (long double)point->xyz[0] * planeNormal[0]) +
            (long double)point->xyz[1] * planeNormal[1] -
            (long double)planeDistance;
        renderer_mark_plane_side_t side;

        distances[pointIndex] = (float)distanceRaw;
        if (distanceRaw > (long double)epsilon)
            side = R_MARK_SIDE_FRONT;
        else if (distanceRaw < -(long double)epsilon)
            side = R_MARK_SIDE_BACK;
        else
            side = R_MARK_SIDE_ON;

        sides[pointIndex] = side;
        ++sideCounts[side];
    }

    sides[inPointCount] = sides[0];
    distances[inPointCount] = distances[0];
    *outPointCount = 0;

    if (sideCounts[R_MARK_SIDE_FRONT] == 0)
        return;

    if (sideCounts[R_MARK_SIDE_BACK] == 0) {
        *outPointCount = inPointCount;
        memcpy(outPoints, inPoints,
               (size_t)inPointCount * sizeof(*outPoints));
        return;
    }

    for (pointIndex = 0; pointIndex < inPointCount; ++pointIndex) {
        const renderer_mark_plane_side_t side = sides[pointIndex];
        const renderer_mark_plane_side_t nextSide = sides[pointIndex + 1];
        const renderer_mark_clip_vertex_t *point = &inPoints[pointIndex];
        renderer_mark_clip_vertex_t *outPoint =
            &outPoints[*outPointCount];

        if (side == R_MARK_SIDE_ON) {
            *outPoint = *point;
            ++*outPointCount;
            continue;
        }

        if (side == R_MARK_SIDE_FRONT) {
            *outPoint = *point;
            ++*outPointCount;
            outPoint = &outPoints[*outPointCount];
        }

        if (nextSide != R_MARK_SIDE_ON && nextSide != side) {
            const renderer_mark_clip_vertex_t *nextPoint =
                &inPoints[(pointIndex + 1) % inPointCount];
            const float denominator =
                distances[pointIndex] - distances[pointIndex + 1];
            const float fraction = denominator == 0.0f
                ? 0.0f
                : distances[pointIndex] / denominator;
            int32_t component;

            for (component = 0; component < 3; ++component) {
                outPoint->xyz[component] = point->xyz[component] +
                    fraction *
                        (nextPoint->xyz[component] - point->xyz[component]);
            }
            for (component = 0; component < 2; ++component) {
                outPoint->lightmapCoords[component] =
                    point->lightmapCoords[component] +
                    fraction * (nextPoint->lightmapCoords[component] -
                                point->lightmapCoords[component]);
            }
            ++*outPointCount;
        }
    }
}

/* Source: CoDUOMP.exe 0x00516810..0x005168cd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00516810_005168ce.mcode.
 * Mac R_AddMarkFragment proves the eight source arguments. The Windows body
 * proves the alternating 64-vertex clip buffers, 0.5 epsilon, capacity check,
 * fragment count write, and 20-byte-to-32-byte output copy. */
qboolean R_AddMarkFragment(
    int32_t pointCount,
    renderer_mark_clip_vertex_t
        clipPoints[R_MARK_CLIP_BUFFER_COUNT][R_MARK_CLIP_MAX_VERTICES],
    int32_t clipPlaneCount, const vec3_t *clipPlaneNormals,
    const float *clipPlaneDistances, int32_t maxPoints,
    polyVert_t *pointBuffer, markFragment_t *fragment)
{
    int32_t activeBuffer = 0;
    int32_t planeIndex;
    int32_t pointIndex;

    for (planeIndex = 0; planeIndex < clipPlaneCount; ++planeIndex) {
        R_ChopPolyBehindPlane(
            pointCount, clipPoints[activeBuffer], &pointCount,
            clipPoints[activeBuffer == 0 ? 1 : 0],
            clipPlaneNormals[planeIndex], clipPlaneDistances[planeIndex],
            0.5f);
        activeBuffer ^= 1;
        if (pointCount == 0)
            return qfalse;
    }

    if (pointCount > maxPoints)
        return qfalse;

    fragment->numPoints = pointCount;
    for (pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
        const renderer_mark_clip_vertex_t *clipPoint =
            &clipPoints[activeBuffer][pointIndex];
        polyVert_t *point = &pointBuffer[pointIndex];

        memcpy(point->xyz, clipPoint->xyz, sizeof(point->xyz));
        memcpy(point->lightmapCoords, clipPoint->lightmapCoords,
               sizeof(clipPoint->lightmapCoords));
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x00516420..0x0051659a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00516420_0051659b.mcode.
 * Mac R_AABBTreeSurfaces_r independently confirms the six-argument source
 * signature. Windows instructions prove all bounds tests, the 40-byte child
 * stride, terminal 12-byte surface stride, shader mark exclusion, and output
 * capacity behavior. */
void R_AABBTreeSurfaces_r(
    const renderer_aabb_tree_t *tree, const vec3_t mins, const vec3_t maxs,
    msurface_t **surfaceBuffer, int32_t maxSurfaces,
    int32_t *surfaceCount)
{
    int32_t index;

    if (tree->maxs[0] < mins[0] || tree->mins[0] > maxs[0] ||
        tree->maxs[1] < mins[1] || tree->mins[1] > maxs[1] ||
        tree->maxs[2] < mins[2] || tree->mins[2] > maxs[2]) {
        return;
    }

    if (tree->childCount != 0) {
        for (index = 0; index < tree->childCount; ++index) {
            R_AABBTreeSurfaces_r(&tree->children[index], mins, maxs,
                                 surfaceBuffer, maxSurfaces, surfaceCount);
        }
        return;
    }

    for (index = 0; index < tree->surfaceCount; ++index) {
        msurface_t *worldSurface;
        const renderer_lit_surface_t *surface;

        if (*surfaceCount >= maxSurfaces)
            break;

        worldSurface = &tree->surfaces[index];
        if ((worldSurface->shader->surfaceParmFlags &
             SHADER_MARKS_DISABLED) != 0)
            continue;

        surface = (const renderer_lit_surface_t *)worldSurface->data;
        if (surface->boundsMax[0] < mins[0] ||
            surface->boundsMin[0] > maxs[0] ||
            surface->boundsMax[1] < mins[1] ||
            surface->boundsMin[1] > maxs[1] ||
            surface->boundsMax[2] < mins[2] ||
            surface->boundsMin[2] > maxs[2]) {
            continue;
        }

        surfaceBuffer[*surfaceCount] = worldSurface;
        ++*surfaceCount;
    }
}

/* Source: CoDUOMP.exe 0x005165a0..0x0051676c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005165a0_0051676d.mcode.
 * The Windows body proves the per-view cell stamp, cull-group list,
 * duplicate suppression, surface bounds filtering, and final root-tree walk;
 * the Mac R_CellSurfaces implementation independently agrees. */
void R_CellSurfaces(
    renderer_world_cell_t *cell, const vec3_t mins, const vec3_t maxs,
    msurface_t **surfaceBuffer, int32_t maxSurfaces,
    int32_t *surfaceCount)
{
    int32_t groupIndex;

    if (cell->markViewCount == tr.viewCount)
        return;

    cell->markViewCount = tr.viewCount;
    for (groupIndex = 0; groupIndex < cell->cullGroupCount; ++groupIndex) {
        const renderer_cull_group_t *group = cell->cullGroups[groupIndex];
        int32_t surfaceIndex;

        if (group->maxs[0] < mins[0] || group->mins[0] > maxs[0] ||
            group->maxs[1] < mins[1] || group->mins[1] > maxs[1] ||
            group->maxs[2] < mins[2] || group->mins[2] > maxs[2]) {
            continue;
        }

        for (surfaceIndex = 0; surfaceIndex < group->surfaceCount;
             ++surfaceIndex) {
            msurface_t *worldSurface;
            const renderer_lit_surface_t *surface;
            int32_t existingIndex;

            if (*surfaceCount >= maxSurfaces)
                break;

            worldSurface = &group->surfaces[surfaceIndex];
            if ((worldSurface->shader->surfaceParmFlags &
                 SHADER_MARKS_DISABLED) != 0)
                continue;

            surface = (const renderer_lit_surface_t *)worldSurface->data;
            if (surface->boundsMax[0] < mins[0] ||
                surface->boundsMin[0] > maxs[0] ||
                surface->boundsMax[1] < mins[1] ||
                surface->boundsMin[1] > maxs[1] ||
                surface->boundsMax[2] < mins[2] ||
                surface->boundsMin[2] > maxs[2]) {
                continue;
            }

            for (existingIndex = 0; existingIndex < *surfaceCount;
                 ++existingIndex) {
                if (surfaceBuffer[existingIndex] == worldSurface)
                    break;
            }
            if (existingIndex == *surfaceCount) {
                surfaceBuffer[*surfaceCount] = worldSurface;
                ++*surfaceCount;
            }
        }
    }

    R_AABBTreeSurfaces_r(cell->aabbTree, mins, maxs, surfaceBuffer,
                         maxSurfaces, surfaceCount);
}

/* Source: CoDUOMP.exe 0x00516770..0x00516801.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00516770_00516802.mcode.
 * The Windows body proves the cellIndex -2/-1 sentinels, BoxOnPlaneSide branch
 * meanings, recursive/tail traversal, 64-byte cell stride, and world ownership;
 * Mac R_BoxSurfaces_r independently proves the six source arguments. */
void R_BoxSurfaces_r(
    const mnode_t *node, const vec3_t mins, const vec3_t maxs,
    msurface_t **surfaceBuffer, int32_t maxSurfaces,
    int32_t *surfaceCount)
{
    while (node->cellIndex == R_WORLD_NODE_INTERNAL) {
        const int32_t side =
            BoxOnPlaneSide(mins, maxs, node->data.node.plane);

        if (side == R_BOX_SIDE_FRONT) {
            node = node->data.node.children[0];
            continue;
        }
        if (side == R_BOX_SIDE_BACK) {
            node = node->data.node.children[1];
            continue;
        }

        R_BoxSurfaces_r(node->data.node.children[0], mins, maxs,
                        surfaceBuffer, maxSurfaces, surfaceCount);
        node = node->data.node.children[1];
    }

    if (node->cellIndex != R_WORLD_NODE_NO_CELL) {
        R_CellSurfaces(&tr.world->cells[node->cellIndex], mins, maxs,
                       surfaceBuffer, maxSurfaces, surfaceCount);
    }
}

/* Source: CoDUOMP.exe 0x005168d0..0x005171e2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005168d0_005171e3.mcode.
 * The same-module Mac RE_MarkFragments independently proves the ten-argument
 * public renderer ABI and high-level call graph. Windows instructions remain
 * authoritative for the 64-source-point clamp, -20-radius search bounds,
 * 4096-surface candidate limit, clipping-plane construction, triangle facing
 * gate, lightmap-coordinate interpolation, shader variant selection, and both
 * output-capacity exits. */
int32_t RE_MarkFragments(int32_t pointCount, const vec3_t *points,
                         const vec3_t projectionOrigin,
                         const axis_t projectionAxis,
                         float projectionRadius, int32_t maxPoints,
                         polyVert_t *pointBuffer, int32_t maxFragments,
                         markFragment_t *fragmentBuffer,
                         int32_t shaderHandle)
{
    msurface_t
        *surfaceBuffer[R_MARK_MAX_WORLD_SURFACES];
    renderer_mark_clip_vertex_t
        clipPoints[R_MARK_CLIP_BUFFER_COUNT][R_MARK_CLIP_MAX_VERTICES];
    vec3_t clipPlaneNormals[
        R_MARK_CLIP_MAX_VERTICES + R_MARK_EXTRA_CLIP_PLANES];
    float clipPlaneDistances[
        R_MARK_CLIP_MAX_VERTICES + R_MARK_EXTRA_CLIP_PLANES];
    /* 0x00516906 MOV EAX,0x48800000 (+262144.0f) -> boundsMin; 0x00516928
     * MOV EAX,0xc8800000 (-262144.0f) -> boundsMax. A prior pass used 65536.0f
     * (0x47800000), a sentinel too small for a world reaching +/-131072. */
    vec3_t boundsMin = {262144.0f, 262144.0f, 262144.0f};
    vec3_t boundsMax = {-262144.0f, -262144.0f, -262144.0f};
    int32_t clippedSourcePointCount;
    int32_t clipPlaneCount;
    int32_t surfaceCount = 0;
    int32_t fragmentCount = 0;
    int32_t outputPointCount = 0;
    shader_t *requestedShader;
    int32_t index;
    int32_t component;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (pointCount <= 0 || pointCount > R_MARK_CLIP_MAX_VERTICES ||
        points == NULL || projectionOrigin == NULL ||
        projectionAxis == NULL || maxPoints <= 0 || pointBuffer == NULL ||
        maxFragments <= 0 || fragmentBuffer == NULL) {
        return 0;
    }

    ++tr.viewCount;
    for (component = 0; component < 3; ++component)
        rendererMarkProjectionPlane.components.normal[component] =
            -projectionAxis[0][component];
    rendererMarkProjectionPlane.components.distance = 0.0f;

    for (index = 0; index < pointCount; ++index) {
        vec3_t projectedPoint;
        vec3_t extendedPoint;

        for (component = 0; component < 3; ++component) {
            const float pointComponent = points[index][component];
            const float directionComponent = projectionAxis[0][component];

            if (pointComponent < boundsMin[component])
                boundsMin[component] = pointComponent;
            if (pointComponent > boundsMax[component])
                boundsMax[component] = pointComponent;

            projectedPoint[component] = pointComponent +
                projectionRadius * directionComponent;
            extendedPoint[component] = pointComponent +
                (projectionRadius * -20.0f) * directionComponent;
        }

        for (component = 0; component < 3; ++component) {
            if (projectedPoint[component] < boundsMin[component])
                boundsMin[component] = projectedPoint[component];
            if (projectedPoint[component] > boundsMax[component])
                boundsMax[component] = projectedPoint[component];
            if (extendedPoint[component] < boundsMin[component])
                boundsMin[component] = extendedPoint[component];
            if (extendedPoint[component] > boundsMax[component])
                boundsMax[component] = extendedPoint[component];
        }
    }

    clippedSourcePointCount = pointCount;
    if (clippedSourcePointCount > R_MARK_CLIP_MAX_VERTICES)
        clippedSourcePointCount = R_MARK_CLIP_MAX_VERTICES;

    for (index = 0; index < clippedSourcePointCount; ++index) {
        const int32_t nextIndex =
            (index + 1) % clippedSourcePointCount;
        vec3_t edge;

        for (component = 0; component < 3; ++component)
            edge[component] = points[nextIndex][component] -
                              points[index][component];

        clipPlaneNormals[index][0] =
            edge[1] * projectionAxis[0][2] -
            edge[2] * projectionAxis[0][1];
        clipPlaneNormals[index][1] =
            edge[2] * projectionAxis[0][0] -
            edge[0] * projectionAxis[0][2];
        clipPlaneNormals[index][2] =
            edge[0] * projectionAxis[0][1] -
            edge[1] * projectionAxis[0][0];
        (void)VectorNormalize(clipPlaneNormals[index]);

        clipPlaneDistances[index] =
            points[index][0] * clipPlaneNormals[index][0] +
            points[index][1] * clipPlaneNormals[index][1] +
            points[index][2] * clipPlaneNormals[index][2];
    }

    memcpy(clipPlaneNormals[clippedSourcePointCount], projectionAxis[0],
           sizeof(vec3_t));
    clipPlaneDistances[clippedSourcePointCount] =
        points[0][0] * projectionAxis[0][0] +
        points[0][1] * projectionAxis[0][1] +
        points[0][2] * projectionAxis[0][2] - projectionRadius;

    for (component = 0; component < 3; ++component) {
        clipPlaneNormals[clippedSourcePointCount + 1][component] =
            -projectionAxis[0][component];
    }
    clipPlaneDistances[clippedSourcePointCount + 1] =
        points[0][0] * clipPlaneNormals[clippedSourcePointCount + 1][0] +
        points[0][1] * clipPlaneNormals[clippedSourcePointCount + 1][1] +
        points[0][2] * clipPlaneNormals[clippedSourcePointCount + 1][2] -
        projectionRadius;
    clipPlaneCount = clippedSourcePointCount + R_MARK_EXTRA_CLIP_PLANES;

    R_BoxSurfaces_r(tr.world->nodes, boundsMin, boundsMax, surfaceBuffer,
                    R_MARK_MAX_WORLD_SURFACES, &surfaceCount);

    for (component = 0; component < 3; ++component)
        rendererMarkProjectionPlane.components.normal[component] =
            -rendererMarkProjectionPlane.components.normal[component];

    if (shaderHandle < 0 || shaderHandle >= tr.numShaders) {
        ri.Printf(R_PRINT_WARNING,
                  "R_GetShaderByHandle: out of range hShader '%d'\n",
                  shaderHandle);
        requestedShader = tr.defaultShader;
    } else {
        requestedShader = tr.shaders[shaderHandle];
    }

    for (index = 0; index < surfaceCount; ++index) {
        msurface_t *worldSurface = surfaceBuffer[index];
        renderer_world_mesh_surface_t *surface =
            (renderer_world_mesh_surface_t *)worldSurface->data;
        shader_t *fragmentShader;
        uint16_t baseIndex;
        int32_t firstTriangleIndex;

        if (surface->surfaceType < R_SURFACE_INDEXED_POSITION_FIRST)
            continue;

        baseIndex = surface->indices[0];

        if (requestedShader != tr.defaultShader &&
            worldSurface->shader->lightmapIndex >= 0) {
            fragmentShader = R_FindShader(
                requestedShader->name, worldSurface->shader->lightmapIndex,
                qtrue, R_MARK_SHADER_USAGE);
        } else {
            fragmentShader = requestedShader;
        }

        for (firstTriangleIndex = 0;
             firstTriangleIndex < surface->indexCount;
             firstTriangleIndex += 3) {
            const uint16_t vertexIndices[3] = {
                surface->indices[firstTriangleIndex],
                surface->indices[firstTriangleIndex + 1],
                surface->indices[firstTriangleIndex + 2]
            };
            const vec3_t *trianglePoints[3];
            const vec2_t *triangleLightmapCoords[3];
            vec3_t edge0;
            vec3_t edge1;
            vec3_t triangleNormal;
            float facing;
            int32_t corner;
            markFragment_t *fragment;

            for (corner = 0; corner < 3; ++corner) {
                const int32_t vertexOffset =
                    (int32_t)vertexIndices[corner] - (int32_t)baseIndex;

                trianglePoints[corner] = &surface->positions[vertexOffset];
                triangleLightmapCoords[corner] =
                    &surface->lightmapCoords[vertexOffset];
            }

            for (component = 0; component < 3; ++component) {
                edge0[component] = (*trianglePoints[0])[component] -
                                   (*trianglePoints[1])[component];
                edge1[component] = (*trianglePoints[2])[component] -
                                   (*trianglePoints[1])[component];
            }
            triangleNormal[0] =
                edge0[1] * edge1[2] - edge0[2] * edge1[1];
            triangleNormal[1] =
                edge0[2] * edge1[0] - edge0[0] * edge1[2];
            triangleNormal[2] =
                edge0[0] * edge1[1] - edge0[1] * edge1[0];
            (void)VectorNormalize(triangleNormal);

            facing = triangleNormal[0] * projectionAxis[0][0] +
                     triangleNormal[1] * projectionAxis[0][1] +
                     triangleNormal[2] * projectionAxis[0][2];
            if (!(facing > 0.5f))
                continue;

            for (corner = 0; corner < 3; ++corner) {
                memcpy(clipPoints[0][corner].xyz, *trianglePoints[corner],
                       sizeof(vec3_t));
                memcpy(clipPoints[0][corner].lightmapCoords,
                       *triangleLightmapCoords[corner], sizeof(vec2_t));
            }

            fragment = &fragmentBuffer[fragmentCount];
            if (R_AddMarkFragment(
                    3, clipPoints, clipPlaneCount, clipPlaneNormals,
                    clipPlaneDistances, maxPoints - outputPointCount,
                    &pointBuffer[outputPointCount], fragment) == qfalse) {
                continue;
            }

            if (fragment->numPoints > 0) {
                const float textureScale = 0.5f / projectionRadius;

                for (corner = 0; corner < fragment->numPoints; ++corner) {
                    polyVert_t *outputPoint =
                        &pointBuffer[outputPointCount + corner];
                    vec3_t relativePoint;

                    for (component = 0; component < 3; ++component) {
                        relativePoint[component] =
                            outputPoint->xyz[component] -
                            projectionOrigin[component];
                    }
                    outputPoint->st[0] = 0.5f + textureScale *
                        ((relativePoint[2] * projectionAxis[1][2] +
                          relativePoint[1] * projectionAxis[1][1]) +
                         relativePoint[0] * projectionAxis[1][0]);
                    outputPoint->st[1] = 0.5f + textureScale *
                        ((relativePoint[0] * projectionAxis[2][0] +
                          relativePoint[2] * projectionAxis[2][2]) +
                         relativePoint[1] * projectionAxis[2][1]);
                }
            }

            fragment->shaderHandle = fragmentShader->index;
            fragment->firstPoint = outputPointCount;
            outputPointCount += fragment->numPoints;
            ++fragmentCount;

            if (fragmentCount == maxFragments ||
                outputPointCount > maxPoints - 3) {
                return fragmentCount;
            }
        }
    }

    return fragmentCount;
}
