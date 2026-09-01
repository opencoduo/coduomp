#ifndef QCOMMON_COLLISION_MAP_TYPES_H
#define QCOMMON_COLLISION_MAP_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "q_collision_types.h"
#include "q_shared_types.h"
#include "q_vector_types.h"

/* Collision-map limits shared by the engine and its cgame boundary. */
enum {
    /* CoDUOMP CMod_LoadSubmodels compares the BSP model count with 0x200 at
     * 0x0041c4de, and the cgame inline-model handle interval contains the same
     * 512 rows. */
    MAX_SUBMODELS = 512,

    /* Fixed-width name field in the Quake III BSP dshader_t record. This is
     * an on-disk format extent, not the configurable engine path limit. */
    BSP_SHADER_NAME_FIELD_SIZE = 64,

    CM_PATCH_VECTOR_AXIS_COUNT = 3,
    CM_PATCH_PLANE_LIMIT = 4096,
    CM_PATCH_POINT_GRID_SIZE = 129,
    CM_PATCH_PLANE_GRID_SIZE = 129,
    CM_PATCH_FACET_BORDER_PLANE_LIMIT = 26,
    /* CM_AddFacetBevels always appends the surface plane after the bevels. */
    CM_PATCH_FACET_BEVEL_BORDER_LIMIT =
        CM_PATCH_FACET_BORDER_PLANE_LIMIT - 1,
    CM_PATCH_MAX_FACETS = 1024,

    CM_TRIANGLE_SOUP_VECTOR_COMPONENT_COUNT = 3,
    CM_TRIANGLE_VERTEX_COUNT = 3,
    CM_TRIANGLE_SOUP_EDGE_LIMIT = 65536,
    CM_TRIANGLE_SOUP_VERTEX_LIMIT = 65536
};

typedef struct patchPlane_s {
    vec3_t normal;
    float dist;
    /* Low three bits record negative x/y/z normal components. */
    uint32_t signbits;
} patchPlane_t;

/* Quake III's anonymous facet_t; facet_s is only the local tag. */
typedef struct facet_s {
    /* Plane fields are indexes into the owning patchCollide_t plane array. */
    int32_t surfacePlane;
    int32_t numBorders;
    int32_t borderPlanes[CM_PATCH_FACET_BORDER_PLANE_LIMIT];
    qboolean borderInward[CM_PATCH_FACET_BORDER_PLANE_LIMIT];
    /* CoD writes and copies this inherited lane but does not consume it. */
    qboolean borderNoAdjust[CM_PATCH_FACET_BORDER_PLANE_LIMIT];
} facet_t;

/* Quake III's anonymous cGrid_t; cGrid_s is only the local tag. */
typedef struct cGrid_s {
    int32_t width;
    int32_t height;
    qboolean wrapWidth;
    qboolean wrapHeight;
    vec3_t points[CM_PATCH_POINT_GRID_SIZE][CM_PATCH_POINT_GRID_SIZE];
} cGrid_t;

typedef int32_t patchPlaneGrid_t[CM_PATCH_PLANE_GRID_SIZE]
                                [CM_PATCH_PLANE_GRID_SIZE][2];

typedef struct patchCollide_s {
    int32_t numPlanes;
    patchPlane_t *planes;
    int32_t numFacets;
    facet_t *facets;
} patchCollide_t;

/* Quake III's anonymous winding_t; winding_s is only the local tag. */
typedef struct winding_s {
    int32_t numpoints;
    vec3_t p[];
} winding_t;

/* Indexed triangle-soup collision backend used for terrain and static models. */
typedef struct collisionSoupVertex_s {
    int32_t checkcount;
    vec3_t position;
} collisionSoupVertex_t;

typedef struct collisionSoupEdge_s {
    int32_t checkcount;
    vec3_t origin;
    vec3_t radialAxes[2];
    vec3_t unitDirection;
    float length;
} collisionSoupEdge_t;

typedef struct collisionSoupTriangle_s {
    plane_t plane;
    /* dot(position, xyz) - w yields the two barycentric coordinates. */
    vec4_t svec;
    vec4_t tvec;
    collisionSoupVertex_t *vertices[CM_TRIANGLE_VERTEX_COUNT];
    /* Entry i is the edge opposite vertices[i]. */
    collisionSoupEdge_t *oppositeEdges[CM_TRIANGLE_VERTEX_COUNT];
} collisionSoupTriangle_t;

typedef struct collisionTriangleSoup_s {
    uint16_t triangleCount;
    /* True when downward-facing planes exist and upward-facing planes do not. */
    uint8_t negativeZOnly;
    uint8_t padding03;
    collisionSoupTriangle_t triangles[];
} collisionTriangleSoup_t;

typedef struct collisionLeaf_s {
    int16_t cluster;
    int16_t area;
    int32_t firstLeafBrush;
    uint16_t numLeafBrushes;
    uint16_t firstLeafTerrainPatch;
    uint16_t numLeafTerrainPatches;
    int16_t cellNum;
} collisionLeaf_t;

typedef struct collisionArea_s {
    int32_t floodNum;
    int32_t floodValid;
} collisionArea_t;

typedef struct collisionTerrainPatch_s {
    int32_t checkcount;
    int32_t materialIndex;
    int32_t contents;
    vec3_t bounds[2]; /* [0] mins, [1] maxs; one contiguous generator output */
    patchCollide_t *curveCollide;
    collisionTriangleSoup_t *terrainCollide;
} collisionTerrainPatch_t;

typedef struct collisionNode_s {
    const cplane_t *plane;
    int16_t children[2];
} collisionNode_t;

typedef struct collisionBrushSide_s {
    const cplane_t *plane;
    int32_t materialIndex;
} collisionBrushSide_t;

typedef struct collisionBrush_s {
    int32_t contents;
    vec3_t mins;
    vec3_t maxs;
    int32_t nonAxialSideCount;
    collisionBrushSide_t *nonAxialSides;
    int32_t checkcount;
    int16_t axialMaterialIndices[6];
} collisionBrush_t;

typedef struct collisionModel_s {
    vec3_t mins;
    vec3_t maxs;
    collisionLeaf_t leaf;
} collisionModel_t;

/* Quake III BSP material record retained by collision and renderer loaders. */
typedef struct dshader_s {
    char shader[BSP_SHADER_NAME_FIELD_SIZE];
    int32_t surfaceFlags;
    int32_t contentFlags;
} dshader_t;

typedef struct cmLeafQueryWork_s cmLeafQueryWork_t;
typedef void (*cmLeafQueryStoreFn_t)(cmLeafQueryWork_t *work,
                                     int32_t nodeNum);

/* Generic callback-driven BSP query record.  items stores either leaf indices
 * or collisionBrush_t pointers, matching the original weakly typed carrier. */
struct cmLeafQueryWork_s {
    int32_t count;
    int32_t maxCount;
    qboolean overflowed;
    void *items;
    vec3_t mins;
    vec3_t maxs;
    int32_t lastLeaf;
    cmLeafQueryStoreFn_t storeLeafs;
};

#if defined(__cplusplus)
#define COLLISION_MAP_STATIC_ASSERT(expression, message) \
    static_assert((expression), message)
#define COLLISION_MAP_ALIGNOF(type) alignof(type)
#else
#define COLLISION_MAP_STATIC_ASSERT(expression, message) \
    _Static_assert((expression), message)
#define COLLISION_MAP_ALIGNOF(type) _Alignof(type)
#endif

#if UINTPTR_MAX == UINT32_MAX
#define COLLISION_MAP_ASSERT_FIELD(type, field, offset)                    \
    COLLISION_MAP_STATIC_ASSERT(offsetof(type, field) == (offset),         \
                                #type "." #field " moved")
#define COLLISION_MAP_ASSERT_SIZE(type, size)                              \
    COLLISION_MAP_STATIC_ASSERT(sizeof(type) == (size), #type " size changed")

COLLISION_MAP_ASSERT_SIZE(patchPlane_t, 0x14);
COLLISION_MAP_ASSERT_FIELD(patchPlane_t, signbits, 0x10);
COLLISION_MAP_ASSERT_SIZE(facet_t, 0x140);
COLLISION_MAP_ASSERT_FIELD(facet_t, borderPlanes, 0x08);
COLLISION_MAP_ASSERT_FIELD(facet_t, borderInward, 0x70);
COLLISION_MAP_ASSERT_FIELD(facet_t, borderNoAdjust, 0xd8);
COLLISION_MAP_ASSERT_SIZE(cGrid_t, 0x30c1c);
COLLISION_MAP_ASSERT_FIELD(cGrid_t, points, 0x10);
COLLISION_MAP_ASSERT_SIZE(patchCollide_t, 0x10);
COLLISION_MAP_ASSERT_FIELD(patchCollide_t, planes, 0x04);
COLLISION_MAP_ASSERT_FIELD(patchCollide_t, facets, 0x0c);
COLLISION_MAP_ASSERT_SIZE(winding_t, 0x04);
COLLISION_MAP_ASSERT_FIELD(winding_t, p, 0x04);

COLLISION_MAP_ASSERT_SIZE(collisionSoupVertex_t, 0x10);
COLLISION_MAP_ASSERT_FIELD(collisionSoupVertex_t, position, 0x04);
COLLISION_MAP_ASSERT_SIZE(collisionSoupEdge_t, 0x38);
COLLISION_MAP_ASSERT_FIELD(collisionSoupEdge_t, radialAxes, 0x10);
COLLISION_MAP_ASSERT_FIELD(collisionSoupEdge_t, unitDirection, 0x28);
COLLISION_MAP_ASSERT_FIELD(collisionSoupEdge_t, length, 0x34);
COLLISION_MAP_ASSERT_SIZE(collisionSoupTriangle_t, 0x48);
COLLISION_MAP_ASSERT_FIELD(collisionSoupTriangle_t, plane, 0x00);
COLLISION_MAP_ASSERT_FIELD(collisionSoupTriangle_t, svec, 0x10);
COLLISION_MAP_ASSERT_FIELD(collisionSoupTriangle_t, tvec, 0x20);
COLLISION_MAP_ASSERT_FIELD(collisionSoupTriangle_t, vertices, 0x30);
COLLISION_MAP_ASSERT_FIELD(collisionSoupTriangle_t, oppositeEdges, 0x3c);
COLLISION_MAP_ASSERT_SIZE(collisionTriangleSoup_t, 0x04);
COLLISION_MAP_ASSERT_FIELD(collisionTriangleSoup_t, triangles, 0x04);

COLLISION_MAP_ASSERT_SIZE(collisionLeaf_t, 0x10);
COLLISION_MAP_ASSERT_FIELD(collisionLeaf_t, firstLeafBrush, 0x04);
COLLISION_MAP_ASSERT_FIELD(collisionLeaf_t, cellNum, 0x0e);
COLLISION_MAP_ASSERT_SIZE(collisionArea_t, 0x08);
COLLISION_MAP_ASSERT_SIZE(collisionTerrainPatch_t, 0x2c);
COLLISION_MAP_ASSERT_FIELD(collisionTerrainPatch_t, bounds, 0x0c);
COLLISION_MAP_ASSERT_FIELD(collisionTerrainPatch_t, curveCollide, 0x24);
COLLISION_MAP_ASSERT_FIELD(collisionTerrainPatch_t, terrainCollide, 0x28);
COLLISION_MAP_ASSERT_SIZE(collisionNode_t, 0x08);
COLLISION_MAP_ASSERT_FIELD(collisionNode_t, children, 0x04);
COLLISION_MAP_ASSERT_SIZE(collisionBrushSide_t, 0x08);
COLLISION_MAP_ASSERT_SIZE(collisionBrush_t, 0x34);
COLLISION_MAP_ASSERT_FIELD(collisionBrush_t, nonAxialSideCount, 0x1c);
COLLISION_MAP_ASSERT_FIELD(collisionBrush_t, nonAxialSides, 0x20);
COLLISION_MAP_ASSERT_FIELD(collisionBrush_t, checkcount, 0x24);
COLLISION_MAP_ASSERT_FIELD(collisionBrush_t, axialMaterialIndices, 0x28);
COLLISION_MAP_ASSERT_SIZE(collisionModel_t, 0x28);
COLLISION_MAP_ASSERT_FIELD(collisionModel_t, leaf, 0x18);
COLLISION_MAP_ASSERT_SIZE(dshader_t, 0x48);
COLLISION_MAP_ASSERT_FIELD(dshader_t, surfaceFlags, 0x40);
COLLISION_MAP_ASSERT_FIELD(dshader_t, contentFlags, 0x44);

COLLISION_MAP_ASSERT_SIZE(cmLeafQueryWork_t, 0x30);
COLLISION_MAP_ASSERT_FIELD(cmLeafQueryWork_t, count, 0x00);
COLLISION_MAP_ASSERT_FIELD(cmLeafQueryWork_t, maxCount, 0x04);
COLLISION_MAP_ASSERT_FIELD(cmLeafQueryWork_t, overflowed, 0x08);
COLLISION_MAP_ASSERT_FIELD(cmLeafQueryWork_t, items, 0x0c);
COLLISION_MAP_ASSERT_FIELD(cmLeafQueryWork_t, mins, 0x10);
COLLISION_MAP_ASSERT_FIELD(cmLeafQueryWork_t, maxs, 0x1c);
COLLISION_MAP_ASSERT_FIELD(cmLeafQueryWork_t, lastLeaf, 0x28);
COLLISION_MAP_ASSERT_FIELD(cmLeafQueryWork_t, storeLeafs, 0x2c);

#undef COLLISION_MAP_ASSERT_FIELD
#undef COLLISION_MAP_ASSERT_SIZE
#endif

#undef COLLISION_MAP_ALIGNOF
#undef COLLISION_MAP_STATIC_ASSERT

#endif
