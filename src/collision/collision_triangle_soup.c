#include "collision_triangle_soup.h"

#include "compat/coduo_x87emu.h"
#include "math/q_math.h"
#include "qcommon/hunk.h"
#include "qcommon/q_shared_types.h"

#include <string.h>

void Com_Error(errorParm_t code, const char *format, ...);

#if !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "collision_triangle_soup.c requires a target behavior"
#endif

enum {
    CM_TRIANGLE_SOUP_HUNK_ALIGNMENT = 32,
};

typedef struct collisionSoupBuildEdge_s {
    int16_t vertexIndices[2];
} collisionSoupBuildEdge_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(collisionSoupBuildEdge_t) == 2, "i386 soup-build edge alignment changed");
_Static_assert(offsetof(collisionSoupBuildEdge_t, vertexIndices[0]) == 0x00, "i386 soup-build edge first vertex index moved");
_Static_assert(offsetof(collisionSoupBuildEdge_t, vertexIndices[1]) == 0x02, "i386 soup-build edge second vertex index moved");
_Static_assert(sizeof(collisionSoupBuildEdge_t) == 0x04, "i386 soup-build edge size changed");
#endif

/* Exact original single-precision constants:
 * 0x005b9eac = 0x31dbe6ff, the coplanar-edge scale;
 * 0x005b9b44/0x005b9b8c = +/-0x3a83126f, approximately +/-0.001. */
static const float CM_TRIANGLE_SOUP_COPLANAR_EDGE_SCALE = 6.4000000854491645e-09f;
static const float CM_TRIANGLE_SOUP_POSITIVE_Z_EPSILON = 0.0010000000474974513f;
static const float CM_TRIANGLE_SOUP_NEGATIVE_Z_EPSILON = -0.0010000000474974513f;

/* Original build workspace:
 *   0x008e49b8 collisionSoupBuildEdges
 *   0x009249b8 collisionSoupBuildOppositeVertices
 *   0x009449b8 collisionSoupBuildVertexRemap
 * It is scratch state used only while one terrain triangle soup is built. */
static collisionSoupBuildEdge_t collisionSoupBuildEdges[CM_TRIANGLE_SOUP_EDGE_LIMIT]; /* 0x008e49b8 */
static int16_t collisionSoupBuildOppositeVertices[CM_TRIANGLE_SOUP_EDGE_LIMIT]; /* 0x009249b8 */
static int16_t collisionSoupBuildVertexRemap[CM_TRIANGLE_SOUP_VERTEX_LIMIT]; /* 0x009449b8 */

/* Source: CoDUOMP.exe 0x004239a0..0x004239ca, recovered from the executable
 * gap. The descriptive name follows its proven role. The same search is also
 * inlined at 0x00423ac0 and 0x00424120..0x004241dd. */
static int32_t CM_FindTriangleSoupEdge(int32_t edgeCount, const collisionSoupBuildEdge_t *edges, int16_t first, int16_t second)
{
    for (int32_t edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex) {
        if ((edges[edgeIndex].vertexIndices[0] == first && edges[edgeIndex].vertexIndices[1] == second) ||
            (edges[edgeIndex].vertexIndices[0] == second && edges[edgeIndex].vertexIndices[1] == first)) {
            return edgeIndex;
        }
    }

    return -1;
}

/* NOT_FROM_ORIGINAL_SOURCE: defensive validation for authored terrain
 * indices before either platform body uses them as vertex or remap-table
 * subscripts. The map-load trust boundary also validates their source span. */
static qboolean coduo_compat_terrain_indices_are_valid(int32_t indexCount, const int16_t *indices, uint32_t vertexCount)
{
    if (indexCount < 0 || (indexCount != 0 && indices == NULL))
        return qfalse;

    for (int32_t index = 0; index < indexCount; ++index) {
        if (indices[index] < 0 || (uint32_t)indices[index] >= vertexCount)
            return qfalse;
    }
    return qtrue;
}

#if defined(WINDOWS_BEHAVIOR)
/* Source: CoDUOMP.exe 0x004239d0..0x004247b1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004239d0_004247b2.mcode.
 * Name: exact same-module Mac symbol CM_GenerateTerrainCollide.
 *
 * The PE was link-time registerized: the caller carries vertices in EBX while
 * pushing the other five source arguments. The maintained interface expresses
 * the ordinary source-level signature. Long-double expressions preserve each
 * Windows x87 chain through its next explicit float store. */
collisionTriangleSoup_t *CM_GenerateTerrainCollide(int32_t indexCount, const int16_t *indices, uint32_t vertexCount, const vec3_t *vertices,
                                                   vec3_t bounds[2])
{
    if (indexCount % CM_TRIANGLE_VERTEX_COUNT != 0) {
        Com_Error(ERR_DROP, "\x15"
                            "CM_GenerateTerrainCollide: numIndexes % 3 != 0, corrupt bsp?");
    }
    if (vertexCount >= CM_TRIANGLE_SOUP_VERTEX_LIMIT) {
        Com_Error(ERR_DROP, "\x15"
                            "CM_GenerateTerrainCollide: too many vertices");
    }

    /* NOT_FROM_ORIGINAL_SOURCE: every authored index must belong to the
     * patch-local vertex domain before the first indexed read. */
    if (coduo_compat_terrain_indices_are_valid(indexCount, indices, vertexCount) == qfalse) {
        Com_Error(ERR_DROP, "\x15"
                            "CM_GenerateTerrainCollide: bad vertex index, corrupt bsp?");
    }

    int32_t edgeCount = 0;
    for (int32_t triangleStart = 0; triangleStart < indexCount; triangleStart += CM_TRIANGLE_VERTEX_COUNT) {
        for (int32_t corner = 0; corner < CM_TRIANGLE_VERTEX_COUNT; ++corner) {
            const int16_t first = indices[triangleStart + corner];
            const int16_t second = indices[triangleStart + ((corner + 1) % CM_TRIANGLE_VERTEX_COUNT)];
            const int16_t opposite = indices[triangleStart + ((corner + 2) % CM_TRIANGLE_VERTEX_COUNT)];

            if (first == second) {
                Com_Error(ERR_DROP, "\x15"
                                    "CM_GenerateTerrainCollide: degenerate triangle, corrupt bsp?");
            }

            const int32_t edgeIndex = CM_FindTriangleSoupEdge(edgeCount, collisionSoupBuildEdges, first, second);
            if (edgeIndex < 0) {
                if (edgeCount >= CM_TRIANGLE_SOUP_EDGE_LIMIT) {
                    Com_Error(ERR_DROP, "\x15"
                                        "CM_GenerateTerrainCollide: too many edges");
                }
                collisionSoupBuildEdges[edgeCount].vertexIndices[0] = first;
                collisionSoupBuildEdges[edgeCount].vertexIndices[1] = second;
                collisionSoupBuildOppositeVertices[edgeCount] = opposite;
                edgeCount++;
                continue;
            }

            const int16_t previousOpposite = collisionSoupBuildOppositeVertices[edgeIndex];
            vec3_t firstDelta;
            vec3_t secondDelta;
            vec3_t previousDelta;
            for (int32_t component = 0; component < CM_TRIANGLE_SOUP_VECTOR_COMPONENT_COUNT; ++component) {
                firstDelta[component] = vertices[first][component] - vertices[opposite][component];
                secondDelta[component] = vertices[second][component] - vertices[opposite][component];
                previousDelta[component] = vertices[previousOpposite][component] - vertices[opposite][component];
            }

            vec3_t cross;
            CrossProduct(secondDelta, firstDelta, cross);
            const float concavity = (float)((long double)cross[2] * previousDelta[2] + (long double)cross[1] * previousDelta[1] +
                                            (long double)cross[0] * previousDelta[0]);
            if (!(concavity > 0.0f))
                continue;

            const float firstDistanceSquared = VectorDistanceSquared(vertices[first], vertices[previousOpposite]);
            const float secondDistanceSquared = VectorDistanceSquared(vertices[second], vertices[previousOpposite]);
            const float maximumDistanceSquared =
                secondDistanceSquared < firstDistanceSquared ? firstDistanceSquared : secondDistanceSquared;
            const long double crossLengthSquared =
                (long double)cross[2] * cross[2] + (long double)cross[1] * cross[1] + (long double)cross[0] * cross[0];
            const long double coplanarThreshold = (long double)maximumDistanceSquared * CM_TRIANGLE_SOUP_COPLANAR_EDGE_SCALE;

            if (crossLengthSquared < coplanarThreshold) {
                edgeCount--;
                collisionSoupBuildEdges[edgeIndex] = collisionSoupBuildEdges[edgeCount];
                collisionSoupBuildOppositeVertices[edgeIndex] = collisionSoupBuildOppositeVertices[edgeCount];
            }
        }
    }

    memset(collisionSoupBuildVertexRemap, -1, sizeof(collisionSoupBuildVertexRemap));
    int32_t usedVertexCount = 0;
    for (int32_t edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex) {
        const int16_t first = collisionSoupBuildEdges[edgeIndex].vertexIndices[0];
        const int16_t second = collisionSoupBuildEdges[edgeIndex].vertexIndices[1];

        if (collisionSoupBuildVertexRemap[first] < 0)
            collisionSoupBuildVertexRemap[first] = (int16_t)usedVertexCount++;
        if (collisionSoupBuildVertexRemap[second] < 0)
            collisionSoupBuildVertexRemap[second] = (int16_t)usedVertexCount++;
    }

    const int32_t triangleCount = indexCount / CM_TRIANGLE_VERTEX_COUNT;
    collisionTriangleSoup_t *soup =
        Hunk_AllocAlignInternal(sizeof(*soup) + (size_t)triangleCount * sizeof(soup->triangles[0]), CM_TRIANGLE_SOUP_HUNK_ALIGNMENT);
    collisionSoupVertex_t *collisionVertices =
        Hunk_AllocAlignInternal((size_t)usedVertexCount * sizeof(collisionVertices[0]), CM_TRIANGLE_SOUP_HUNK_ALIGNMENT);
    collisionSoupEdge_t *collisionEdges =
        Hunk_AllocAlignInternal((size_t)edgeCount * sizeof(collisionEdges[0]), CM_TRIANGLE_SOUP_HUNK_ALIGNMENT);

    qboolean hasNegativeZPlane = qfalse;
    qboolean hasPositiveZPlane = qfalse;
    soup->triangleCount = (uint16_t)triangleCount;

    /* The PE reloads the published uint16 count from soup rather than retaining
     * the full quotient. Preserve that truncation at this loop boundary. */
    for (int32_t triangleIndex = 0; triangleIndex < (int32_t)soup->triangleCount; ++triangleIndex) {
        const int32_t triangleStart = triangleIndex * CM_TRIANGLE_VERTEX_COUNT;
        const int16_t first = indices[triangleStart];
        const int16_t second = indices[triangleStart + 1];
        const int16_t third = indices[triangleStart + 2];
        collisionSoupTriangle_t *triangle = &soup->triangles[triangleIndex];

        (void)PlaneFromPoints(triangle->plane.equation, vertices[first], vertices[second], vertices[third]);
        if (triangle->plane.components.normal[2] < CM_TRIANGLE_SOUP_NEGATIVE_Z_EPSILON) {
            hasNegativeZPlane = qtrue;
        } else if (triangle->plane.components.normal[2] > CM_TRIANGLE_SOUP_POSITIVE_Z_EPSILON) {
            hasPositiveZPlane = qtrue;
        }

        vec3_t edge01;
        vec3_t edge02;
        for (int32_t component = 0; component < CM_TRIANGLE_SOUP_VECTOR_COMPONENT_COUNT; ++component) {
            edge01[component] = vertices[second][component] - vertices[first][component];
            edge02[component] = vertices[third][component] - vertices[first][component];
        }
        const float edge01Length = VectorNormalize(edge01);
        const float edge02Length = VectorNormalize(edge02);
        const float dot =
            (float)((long double)edge01[2] * edge02[2] + (long double)edge01[1] * edge02[1] + (long double)edge01[0] * edge02[0]);
        const float inverseDot = (float)(1.0L / (1.0L - (long double)dot * dot));

        for (int32_t component = 0; component < CM_TRIANGLE_SOUP_VECTOR_COMPONENT_COUNT; ++component) {
            edge01[component] = (float)((long double)edge01[component] * inverseDot);
            edge02[component] = (float)((long double)edge02[component] * inverseDot);
        }

        const float negativeDot = -dot;
        const float inverseEdge01Length = (float)(1.0L / (long double)edge01Length);
        const float inverseEdge02Length = (float)(1.0L / (long double)edge02Length);

        /* Unlike the Linux server build, the Windows client keeps each
         * numerator in x87 through the inverse-length multiplication and
         * performs only the final float store (0x00423fc2..0x00424008 and
         * 0x0042403c..0x0042408b). */
        for (int32_t component = 0; component < CM_TRIANGLE_SOUP_VECTOR_COMPONENT_COUNT; ++component) {
            triangle->svec[component] = (float)(((long double)negativeDot * edge02[component] + edge01[component]) * inverseEdge01Length);
            triangle->tvec[component] = (float)(((long double)negativeDot * edge01[component] + edge02[component]) * inverseEdge02Length);
        }
        triangle->svec[3] =
            (float)((long double)triangle->svec[2] * vertices[first][2] + (long double)triangle->svec[1] * vertices[first][1] +
                    (long double)triangle->svec[0] * vertices[first][0]);
        triangle->tvec[3] =
            (float)((long double)triangle->tvec[2] * vertices[first][2] + (long double)triangle->tvec[1] * vertices[first][1] +
                    (long double)triangle->tvec[0] * vertices[first][0]);

        int16_t remap = collisionSoupBuildVertexRemap[first];
        triangle->vertices[0] = remap < 0 ? NULL : &collisionVertices[remap];
        remap = collisionSoupBuildVertexRemap[second];
        triangle->vertices[1] = remap < 0 ? NULL : &collisionVertices[remap];
        remap = collisionSoupBuildVertexRemap[third];
        triangle->vertices[2] = remap < 0 ? NULL : &collisionVertices[remap];

        int32_t edgeIndex = CM_FindTriangleSoupEdge(edgeCount, collisionSoupBuildEdges, second, third);
        triangle->oppositeEdges[0] = edgeIndex < 0 ? NULL : &collisionEdges[edgeIndex];
        edgeIndex = CM_FindTriangleSoupEdge(edgeCount, collisionSoupBuildEdges, third, first);
        triangle->oppositeEdges[1] = edgeIndex < 0 ? NULL : &collisionEdges[edgeIndex];
        edgeIndex = CM_FindTriangleSoupEdge(edgeCount, collisionSoupBuildEdges, first, second);
        triangle->oppositeEdges[2] = edgeIndex < 0 ? NULL : &collisionEdges[edgeIndex];
    }

    soup->negativeZOnly = hasNegativeZPlane != qfalse && hasPositiveZPlane == qfalse;

    ClearBounds(bounds[0], bounds[1]);
    for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        AddPointToBounds(vertices[vertexIndex], bounds[0], bounds[1]);

        const int16_t remap = collisionSoupBuildVertexRemap[vertexIndex];
        if (remap >= 0) {
            collisionVertices[remap].checkcount = 0;
            collisionVertices[remap].position[0] = vertices[vertexIndex][0];
            collisionVertices[remap].position[1] = vertices[vertexIndex][1];
            collisionVertices[remap].position[2] = vertices[vertexIndex][2];
        }
    }

    for (int32_t edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex) {
        collisionSoupEdge_t *edge = &collisionEdges[edgeIndex];
        const int16_t first = collisionSoupBuildEdges[edgeIndex].vertexIndices[0];
        const int16_t second = collisionSoupBuildEdges[edgeIndex].vertexIndices[1];

        edge->checkcount = 0;
        edge->origin[0] = vertices[first][0];
        edge->origin[1] = vertices[first][1];
        edge->origin[2] = vertices[first][2];
        edge->unitDirection[0] = vertices[second][0] - vertices[first][0];
        edge->unitDirection[1] = vertices[second][1] - vertices[first][1];
        edge->unitDirection[2] = vertices[second][2] - vertices[first][2];
        edge->length = VectorNormalize(edge->unitDirection);
        PerpendicularVector(edge->radialAxes[0], edge->unitDirection);
        CrossProduct(edge->unitDirection, edge->radialAxes[0], edge->radialAxes[1]);
    }

    return soup;
}
#elif defined(LINUX_BEHAVIOR)

/* Source: coduo_lnxded 0x0805403e..0x08055594.  The adjacent edge
 * search at 0x08053fa4..0x0805403d is the common helper above. */
collisionTriangleSoup_t *CM_GenerateTerrainCollide(int32_t indexCount, const int16_t *indices, uint32_t vertexCount, const vec3_t *vertices,
                                                   vec3_t bounds[2])
{
    if (indexCount % CM_TRIANGLE_VERTEX_COUNT != 0) {
        Com_Error(ERR_DROP, "\x15"
                            "CM_GenerateTerrainCollide: numIndexes % 3 != 0, corrupt bsp?");
    }
    if (CM_TRIANGLE_SOUP_VERTEX_LIMIT - 1U < vertexCount) {
        Com_Error(ERR_DROP, "\x15"
                            "CM_GenerateTerrainCollide: too many vertices");
    }

    /* NOT_FROM_ORIGINAL_SOURCE: apply the same index-domain validation to the
     * Linux operation graph before its first access. */
    if (coduo_compat_terrain_indices_are_valid(indexCount, indices, vertexCount) == qfalse) {
        Com_Error(ERR_DROP, "\x15"
                            "CM_GenerateTerrainCollide: bad vertex index, corrupt bsp?");
    }

    uint32_t edgeCount = 0;
    for (int32_t triangleIndex = 0; triangleIndex < indexCount; triangleIndex += CM_TRIANGLE_VERTEX_COUNT) {
        for (int32_t corner = 0; corner < CM_TRIANGLE_VERTEX_COUNT; ++corner) {
            int16_t first = indices[triangleIndex + corner];
            int16_t second = indices[triangleIndex + ((corner + 1) % CM_TRIANGLE_VERTEX_COUNT)];
            int16_t opposite = indices[triangleIndex + ((corner + 2) % CM_TRIANGLE_VERTEX_COUNT)];

            if (first == second) {
                Com_Error(ERR_DROP, "\x15"
                                    "CM_GenerateTerrainCollide: degenerate triangle, corrupt bsp?");
            }

            int32_t edgeIndex = CM_FindTriangleSoupEdge((int32_t)edgeCount, collisionSoupBuildEdges, first, second);
            if (edgeIndex < 0) {
                if (CM_TRIANGLE_SOUP_EDGE_LIMIT - 1U < edgeCount) {
                    Com_Error(ERR_DROP, "\x15"
                                        "CM_GenerateTerrainCollide: too many edges");
                }
                collisionSoupBuildEdges[edgeCount].vertexIndices[0] = first;
                collisionSoupBuildEdges[edgeCount].vertexIndices[1] = second;
                collisionSoupBuildOppositeVertices[edgeCount] = opposite;
                edgeCount++;
                continue;
            }

            vec3_t firstDelta = {
                vertices[first][0] - vertices[opposite][0],
                vertices[first][1] - vertices[opposite][1],
                vertices[first][2] - vertices[opposite][2],
            };
            vec3_t secondDelta = {
                vertices[second][0] - vertices[opposite][0],
                vertices[second][1] - vertices[opposite][1],
                vertices[second][2] - vertices[opposite][2],
            };
            int16_t previousOpposite = collisionSoupBuildOppositeVertices[edgeIndex];
            vec3_t previousDelta = {
                vertices[previousOpposite][0] - vertices[opposite][0],
                vertices[previousOpposite][1] - vertices[opposite][1],
                vertices[previousOpposite][2] - vertices[opposite][2],
            };

            vec3_t cross;
            CrossProduct(secondDelta, firstDelta, cross);
            float concavity = (cross[0] * previousDelta[0]) + (cross[1] * previousDelta[1]) + (cross[2] * previousDelta[2]);
            if (concavity <= 0.0f) {
                continue;
            }

            float firstDistance = VectorDistanceSquared(vertices[first], vertices[previousOpposite]);
            float secondDistance = VectorDistanceSquared(vertices[second], vertices[previousOpposite]);
            float maxDistance = secondDistance < firstDistance ? firstDistance : secondDistance;
            /* The original never stores the cross-length dot: it is compared
             * in 80-bit straight against maxDistance*epsilon (0x805452a..
             * 0x8054544, no fstp), so it must stay inline in the condition. */
            if (((cross[0] * cross[0]) + (cross[1] * cross[1]) + (cross[2] * cross[2])) <
                maxDistance * CM_TRIANGLE_SOUP_COPLANAR_EDGE_SCALE) {
                edgeCount--;
                collisionSoupBuildEdges[edgeIndex] = collisionSoupBuildEdges[edgeCount];
                collisionSoupBuildOppositeVertices[edgeIndex] = collisionSoupBuildOppositeVertices[edgeCount];
            }
        }
    }

    memset(collisionSoupBuildVertexRemap, -1, sizeof(collisionSoupBuildVertexRemap));
    int32_t usedVertexCount = 0;
    for (int32_t edgeIndex = 0; edgeIndex < (int32_t)edgeCount; ++edgeIndex) {
        int16_t first = collisionSoupBuildEdges[edgeIndex].vertexIndices[0];
        int16_t second = collisionSoupBuildEdges[edgeIndex].vertexIndices[1];

        if (collisionSoupBuildVertexRemap[first] < 0) {
            collisionSoupBuildVertexRemap[first] = (int16_t)usedVertexCount;
            usedVertexCount++;
        }
        if (collisionSoupBuildVertexRemap[second] < 0) {
            collisionSoupBuildVertexRemap[second] = (int16_t)usedVertexCount;
            usedVertexCount++;
        }
    }

    int32_t triangleCount = indexCount / CM_TRIANGLE_VERTEX_COUNT;
    uint32_t soupBytes = (uint32_t)sizeof(collisionTriangleSoup_t) + (uint32_t)triangleCount * (uint32_t)sizeof(collisionSoupTriangle_t);
    collisionTriangleSoup_t *soup = Hunk_AllocInternal(soupBytes);
    collisionSoupVertex_t *collisionVertices = Hunk_AllocInternal((size_t)usedVertexCount * sizeof(collisionVertices[0]));
    collisionSoupEdge_t *collisionEdges = Hunk_AllocInternal((size_t)edgeCount * sizeof(collisionEdges[0]));

    qboolean hasPositiveZPlane = qfalse;
    qboolean hasNegativeZPlane = qfalse;
    soup->triangleCount = (uint16_t)triangleCount;
    const int32_t publishedTriangleCount = (int32_t)soup->triangleCount;

    for (int32_t triangleIndex = 0; triangleIndex < publishedTriangleCount; ++triangleIndex) {
        int16_t first = indices[triangleIndex * CM_TRIANGLE_VERTEX_COUNT];
        int16_t second = indices[triangleIndex * CM_TRIANGLE_VERTEX_COUNT + 1];
        int16_t third = indices[triangleIndex * CM_TRIANGLE_VERTEX_COUNT + 2];
        collisionSoupTriangle_t *triangle = &soup->triangles[triangleIndex];

        PlaneFromPoints(triangle->plane.equation, vertices[first], vertices[second], vertices[third]);
        if (triangle->plane.components.normal[2] < CM_TRIANGLE_SOUP_NEGATIVE_Z_EPSILON) {
            hasNegativeZPlane = qtrue;
        } else if (CM_TRIANGLE_SOUP_POSITIVE_Z_EPSILON < triangle->plane.components.normal[2]) {
            hasPositiveZPlane = qtrue;
        }

#if EMULATE_X87
        /* x87-faithful transcription of the svec/tvec basis (0x0805403e):
         * every product/sum/difference is an 80-bit chain rounded to its float
         * slot. VectorNormalize is the emulated variant; edge subtracts round
         * to float; dot is an 80-bit 3-term dot; inverseDot = 1/(1-dot*dot);
         * edge01/edge02 are scaled by inverseDot in place BEFORE svec/tvec;
         * each svec/tvec component is (edgeA - dot*edgeB) * (1/edgeLength);
         * svec[3]/tvec[3] are dots with vertices[first]. */
        vec3_t edge01;
        vec3_t edge02;
        for (int32_t k = 0; k < CM_PATCH_VECTOR_AXIS_COUNT; ++k) {
            edge01[k] = x87f_store_f32(x87f_sub(x87f_load_f32(vertices[second][k]), x87f_load_f32(vertices[first][k])));
        }
        float edge01Length = (float)VectorNormalize(edge01);
        for (int32_t k = 0; k < CM_PATCH_VECTOR_AXIS_COUNT; ++k) {
            edge02[k] = x87f_store_f32(x87f_sub(x87f_load_f32(vertices[third][k]), x87f_load_f32(vertices[first][k])));
        }
        float edge02Length = (float)VectorNormalize(edge02);

        float dot = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(edge01[0]), x87f_load_f32(edge02[0])),
                                                     x87f_mul(x87f_load_f32(edge01[1]), x87f_load_f32(edge02[1]))),
                                            x87f_mul(x87f_load_f32(edge01[2]), x87f_load_f32(edge02[2]))));
        /* inverseDot = 1 / (1 - dot*dot) */
        float inverseDot =
            x87f_store_f32(x87f_div(x87f_load_f32(1.0f), x87f_sub(x87f_load_f32(1.0f), x87f_mul(x87f_load_f32(dot), x87f_load_f32(dot)))));

        for (int32_t axis = 0; axis < CM_PATCH_VECTOR_AXIS_COUNT; ++axis) {
            edge01[axis] = x87f_store_f32(x87f_mul(x87f_load_f32(edge01[axis]), x87f_load_f32(inverseDot)));
            edge02[axis] = x87f_store_f32(x87f_mul(x87f_load_f32(edge02[axis]), x87f_load_f32(inverseDot)));
        }

        /* Each svec/tvec component is TWO stores: the numerator
         * edgeA[k] + (-dot)*edgeB[k] is rounded to a float slot (fchs;fmul;
         * fld;faddp;fstp), then that reloaded float is multiplied by 1/length
         * (fld1;fdiv length; fmulp; fstp) — the 1/length is formed inline per
         * component. */
        for (int32_t k = 0; k < CM_PATCH_VECTOR_AXIS_COUNT; ++k) {
            float numerator =
                x87f_store_f32(x87f_add(x87f_mul(x87f_neg(x87f_load_f32(dot)), x87f_load_f32(edge02[k])), x87f_load_f32(edge01[k])));
            triangle->svec[k] =
                x87f_store_f32(x87f_mul(x87f_load_f32(numerator), x87f_div(x87f_load_f32(1.0f), x87f_load_f32(edge01Length))));
        }
        triangle->svec[3] = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(triangle->svec[0]), x87f_load_f32(vertices[first][0])),
                                                             x87f_mul(x87f_load_f32(triangle->svec[1]), x87f_load_f32(vertices[first][1]))),
                                                    x87f_mul(x87f_load_f32(triangle->svec[2]), x87f_load_f32(vertices[first][2]))));

        for (int32_t k = 0; k < CM_PATCH_VECTOR_AXIS_COUNT; ++k) {
            float numerator =
                x87f_store_f32(x87f_add(x87f_mul(x87f_neg(x87f_load_f32(dot)), x87f_load_f32(edge01[k])), x87f_load_f32(edge02[k])));
            triangle->tvec[k] =
                x87f_store_f32(x87f_mul(x87f_load_f32(numerator), x87f_div(x87f_load_f32(1.0f), x87f_load_f32(edge02Length))));
        }
        triangle->tvec[3] = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(triangle->tvec[0]), x87f_load_f32(vertices[first][0])),
                                                             x87f_mul(x87f_load_f32(triangle->tvec[1]), x87f_load_f32(vertices[first][1]))),
                                                    x87f_mul(x87f_load_f32(triangle->tvec[2]), x87f_load_f32(vertices[first][2]))));
#else
        vec3_t edge01 = {
            vertices[second][0] - vertices[first][0],
            vertices[second][1] - vertices[first][1],
            vertices[second][2] - vertices[first][2],
        };
        float edge01Length = (float)VectorNormalize(edge01);
        vec3_t edge02 = {
            vertices[third][0] - vertices[first][0],
            vertices[third][1] - vertices[first][1],
            vertices[third][2] - vertices[first][2],
        };
        float edge02Length = (float)VectorNormalize(edge02);

        float dot = (edge01[0] * edge02[0]) + (edge01[1] * edge02[1]) + (edge01[2] * edge02[2]);
        float inverseDot = 1.0f / (1.0f - (dot * dot));

        for (int32_t axis = 0; axis < CM_PATCH_VECTOR_AXIS_COUNT; ++axis) {
            edge01[axis] *= inverseDot;
            edge02[axis] *= inverseDot;
        }

        /* The original rounds the (edgeA - dot*edgeB) numerator to a float
         * before multiplying by 1/length (it spills it to the svec/tvec slot
         * and reloads). A natural `(edgeA - dot*edgeB) * (1/length)` keeps the
         * numerator in an 80-bit register on both x87 and -mfpmath=387 at -O0
         * and does NOT match the stock binary, so use explicit float temps. */
        for (int32_t axis = 0; axis < CM_PATCH_VECTOR_AXIS_COUNT; ++axis) {
            float numerator = edge01[axis] - (dot * edge02[axis]);
            triangle->svec[axis] = numerator * (1.0f / edge01Length);
        }
        triangle->svec[3] =
            (triangle->svec[0] * vertices[first][0]) + (triangle->svec[1] * vertices[first][1]) + (triangle->svec[2] * vertices[first][2]);

        for (int32_t axis = 0; axis < CM_PATCH_VECTOR_AXIS_COUNT; ++axis) {
            float numerator = edge02[axis] - (dot * edge01[axis]);
            triangle->tvec[axis] = numerator * (1.0f / edge02Length);
        }
        triangle->tvec[3] =
            (triangle->tvec[0] * vertices[first][0]) + (triangle->tvec[1] * vertices[first][1]) + (triangle->tvec[2] * vertices[first][2]);
#endif

        int16_t remap = collisionSoupBuildVertexRemap[first];
        triangle->vertices[0] = remap < 0 ? NULL : &collisionVertices[remap];
        remap = collisionSoupBuildVertexRemap[second];
        triangle->vertices[1] = remap < 0 ? NULL : &collisionVertices[remap];
        remap = collisionSoupBuildVertexRemap[third];
        triangle->vertices[2] = remap < 0 ? NULL : &collisionVertices[remap];

        int32_t edgeIndex = CM_FindTriangleSoupEdge((int32_t)edgeCount, collisionSoupBuildEdges, second, third);
        triangle->oppositeEdges[0] = edgeIndex < 0 ? NULL : &collisionEdges[edgeIndex];
        edgeIndex = CM_FindTriangleSoupEdge((int32_t)edgeCount, collisionSoupBuildEdges, third, first);
        triangle->oppositeEdges[1] = edgeIndex < 0 ? NULL : &collisionEdges[edgeIndex];
        edgeIndex = CM_FindTriangleSoupEdge((int32_t)edgeCount, collisionSoupBuildEdges, first, second);
        triangle->oppositeEdges[2] = edgeIndex < 0 ? NULL : &collisionEdges[edgeIndex];
    }

    soup->negativeZOnly = hasNegativeZPlane != qfalse && hasPositiveZPlane == qfalse ? qtrue : qfalse;

    ClearBounds(bounds[0], bounds[1]);
    for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        AddPointToBounds(vertices[vertexIndex], bounds[0], bounds[1]);

        int16_t remap = collisionSoupBuildVertexRemap[vertexIndex];
        if (remap >= 0) {
            collisionVertices[remap].checkcount = 0;
            collisionVertices[remap].position[0] = vertices[vertexIndex][0];
            collisionVertices[remap].position[1] = vertices[vertexIndex][1];
            collisionVertices[remap].position[2] = vertices[vertexIndex][2];
        }
    }

    for (int32_t edgeIndex = 0; edgeIndex < (int32_t)edgeCount; ++edgeIndex) {
        collisionSoupEdge_t *edge = &collisionEdges[edgeIndex];
        int16_t first = collisionSoupBuildEdges[edgeIndex].vertexIndices[0];
        int16_t second = collisionSoupBuildEdges[edgeIndex].vertexIndices[1];

        edge->checkcount = 0;
        edge->origin[0] = vertices[first][0];
        edge->origin[1] = vertices[first][1];
        edge->origin[2] = vertices[first][2];
        edge->unitDirection[0] = vertices[second][0] - vertices[first][0];
        edge->unitDirection[1] = vertices[second][1] - vertices[first][1];
        edge->unitDirection[2] = vertices[second][2] - vertices[first][2];
        edge->length = (float)VectorNormalize(edge->unitDirection);
        PerpendicularVector(edge->radialAxes[0], edge->unitDirection);
        CrossProduct(edge->unitDirection, edge->radialAxes[0], edge->radialAxes[1]);
    }

    return soup;
}
#endif
