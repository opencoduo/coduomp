#ifndef QCOMMON_BSP_TYPES_H
#define QCOMMON_BSP_TYPES_H

#include "q_vector_types.h"

#include <stdint.h>

/* CoD's BSP format is a direct descendant of Quake III's qfiles records.
 * The shipped Windows collision/renderer loaders, the Linux dedicated-server
 * collision loader, and the independently named PPC Mac CMod_Load* and R_Load*
 * families agree on these complete layouts. CoD reverses the two lump words
 * and extends or compacts several records, but their original ownership and
 * inherited type/member identities remain. */
enum {
    BSP_VERSION = 59,
    HEADER_LUMPS = 33
};

typedef enum bspLumpIndex_e {
    BSP_LUMP_SHADERS = 0,
    BSP_LUMP_LIGHTMAPS = 1,
    BSP_LUMP_PLANES = 2,
    /* CoDUOMP.exe 0x0041d2f9 passes lump 3 as 8-byte brush-side records
     * and lump 4 as 4-byte brush records; the Linux loader agrees. */
    BSP_LUMP_BRUSH_SIDES = 3,
    BSP_LUMP_BRUSHES = 4,
    BSP_LUMP_SURFACES = 6,
    BSP_LUMP_DRAW_VERTICES = 7,
    BSP_LUMP_DRAW_INDICES = 8,
    BSP_LUMP_CULL_GROUPS = 9,
    BSP_LUMP_CULL_GROUP_INDICES = 10,
    BSP_LUMP_PORTAL_VERTICES = 11,
    BSP_LUMP_OCCLUDERS = 12,
    BSP_LUMP_OCCLUDER_PLANES = 13,
    BSP_LUMP_OCCLUDER_EDGES = 14,
    BSP_LUMP_OCCLUDER_INDICES = 15,
    BSP_LUMP_AABB_TREES = 16,
    BSP_LUMP_CELLS = 17,
    BSP_LUMP_PORTALS = 18,
    BSP_LUMP_LIGHT_INDICES = 19,
    BSP_LUMP_NODES = 20,
    BSP_LUMP_LEAFS = 21,
    BSP_LUMP_LEAF_BRUSHES = 22,
    BSP_LUMP_LEAF_SURFACES = 23,
    BSP_LUMP_TERRAIN_PATCHES = 24,
    BSP_LUMP_TERRAIN_VERTICES = 25,
    BSP_LUMP_TERRAIN_INDICES = 26,
    BSP_LUMP_MODELS = 27,
    BSP_LUMP_VISIBILITY = 28,
    BSP_LUMP_ENTITIES = 29,
    BSP_LUMP_LIGHTS = 30,
    BSP_LUMP_LIGHT_VIS_CACHE = 32
} bspLumpIndex_t;

typedef struct lump_s {
    int32_t filelen;
    int32_t fileofs;
} lump_t;

typedef struct dheader_s {
    int32_t ident; /* Preserved by CM_SaveLump; not validated by the loaders. */
    int32_t version;
    lump_t lumps[HEADER_LUMPS];
} dheader_t;

typedef struct dplane_s {
    vec3_t normal;
    float dist;
} dplane_t;

typedef struct dnode_s {
    int32_t planeNum;
    int32_t children[2];
    int32_t mins[3]; /* Format bounds; unused by the collision loaders. */
    int32_t maxs[3]; /* Format bounds; unused by the collision loaders. */
} dnode_t;

typedef struct dleaf_s {
    int32_t cluster;
    int32_t area;
    int32_t firstLeafTerrainPatch;
    int32_t numLeafTerrainPatches;
    int32_t firstLeafBrush;
    int32_t numLeafBrushes;
    int32_t cellNum;
    int32_t firstLightIndex;
    int32_t lightCount;
} dleaf_t;

typedef struct dmodel_s {
    vec3_t mins;
    vec3_t maxs;
    int32_t firstSurface;
    int32_t numSurfaces;
    int32_t firstLeafSurface;
    int32_t numLeafSurfaces;
    int32_t firstLeafBrush;
    int32_t numLeafBrushes;
} dmodel_t;

typedef struct dbrush_s {
    int16_t numSides;
    int16_t shaderNum;
} dbrush_t;

typedef struct dbrushside_s {
    union {
        float dist;
        int32_t planeNum;
    } plane;
    int32_t shaderNum;
} dbrushside_t;

/* CoD-specific records with no Q3 typedef counterpart. */
typedef struct dterrainPatch_s {
    int16_t shaderNum;
    uint8_t collisionMode;
    uint8_t padding03; /* Compiler padding before the 32-bit-aligned union. */
    union {
        struct {
            int16_t width;
            int16_t height;
            int32_t maxError;
            uint32_t firstVert;
        } curve;
        struct {
            int16_t numVerts;
            int16_t numIndexes;
            uint32_t firstVert;
            uint32_t firstIndex;
        } terrain;
    } data;
} dterrainPatch_t;

typedef struct dvis_s {
    int32_t numClusters;
    int32_t clusterBytes;
    uint8_t data[];
} dvis_t;

typedef struct dsurface_s {
    int16_t shaderNum;
    int16_t lightmapNum;
    int32_t firstVert;
    uint16_t numVerts;
    uint16_t numIndexes;
    uint32_t firstIndex;
} dsurface_t;

typedef struct drawVert_s {
    vec3_t xyz;
    vec2_t st;
    vec2_t lightmap;
    vec3_t normal;
    uint8_t color[4];
} drawVert_t;

#endif
