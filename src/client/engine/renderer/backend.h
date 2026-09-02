#ifndef CODUOMP_RENDERER_BACKEND_H
#define CODUOMP_RENDERER_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#include "qcommon/asset_type_names.h"
#include "qcommon/q_renderer_types.h"
#include "../q_shared.h"
#include "qcommon/bsp_types.h"
#include "../effects/fx_render_types.h"
#include "gl_state.h"
#include "renderer_entity.h"
#include "renderer_cvars.h"
#include "renderer_fog.h"
#include "renderer_orientation.h"
#include "renderer_refdef.h"
#include "renderer_vbo.h"
#include "renderer_view_parms.h"

typedef struct shader_s shader_t;
typedef struct image_s image_t;
typedef struct shaderStage_s shaderStage_t;
typedef struct mnode_s mnode_t;
typedef struct renderer_sky_vertex_storage_s
    renderer_sky_vertex_storage_t;
typedef union renderer_cached_static_model_surface_u
    renderer_cached_static_model_surface_t;
struct client_debug_line_s;
struct client_debug_string_s;

/* Serialized BSP AABB-tree record. Child records follow their parent in
 * depth-first order; the runtime loader replaces the implicit ordering with
 * a native pointer to the contiguous child span. */
typedef struct renderer_disk_aabb_tree_s {
    int32_t firstSurface;
    int32_t surfaceCount;
    int32_t childCount;
} renderer_disk_aabb_tree_t;

/* Serialized BSP cell record. The loader resolves its table indexes into
 * native pointers while retaining the six authored bounds floats verbatim. */
typedef struct renderer_disk_cell_s {
    vec3_t mins;
    vec3_t maxs;
    int32_t aabbTreeIndex;
    int32_t firstPortal;
    int32_t portalCount;
    int32_t firstCullGroup;
    int32_t cullGroupCount;
    int32_t firstOccluder;
    int32_t occluderCount;
} renderer_disk_cell_t;

/* Serialized portal record. The loader resolves the plane, destination cell,
 * and contiguous portal-vertex span into a runtime renderer_portal_t. */
typedef struct renderer_disk_portal_s {
    int32_t planeIndex;
    int32_t cellIndex;
    int32_t firstVertex;
    int32_t vertexCount;
} renderer_disk_portal_t;

/* Serialized cull group: authored bounds plus a contiguous surface span. */
typedef struct renderer_disk_cull_group_s {
    vec3_t mins;
    vec3_t maxs;
    int32_t firstSurface;
    int32_t surfaceCount;
} renderer_disk_cull_group_t;

/* Each serialized occluder selects contiguous spans from the separate plane,
 * edge, and portal-vertex lumps. The two bytes after vertexCount are ordinary
 * trailing compiler padding in the 20-byte on-disk record; neither shipped
 * loader consumes them. */
typedef struct renderer_disk_occluder_s {
    int32_t firstPlane;
    int16_t planeCount;
    int16_t edgeCount;
    int32_t firstEdge;
    int32_t firstVertex;
    int16_t vertexCount;
} renderer_disk_occluder_t;

/* Four authored byte indexes select the edge's two planes and two vertices. */
typedef struct renderer_disk_occluder_edge_s {
    uint8_t planeIndices[2];
    uint8_t vertexIndices[2];
} renderer_disk_occluder_edge_t;

/* One output record per original 512x512 lightmap. atlasIndex is followed by
 * ordinary float alignment; the two intervening bytes are compiler padding,
 * not an unknown source field. */
typedef struct renderer_lightmap_placement_s {
    int16_t atlasIndex; /* original +0x00 */
    float sOffset;      /* original +0x04 */
    float tOffset;      /* original +0x08 */
    float sScale;       /* original +0x0c */
    float tScale;       /* original +0x10 */
} renderer_lightmap_placement_t;

#if UINTPTR_MAX == UINT32_MAX
#if defined(__cplusplus)
#define RENDERER_ORIGINAL_ALIGNOF(type_) alignof(type_)
#else
#define RENDERER_ORIGINAL_ALIGNOF(type_) _Alignof(type_)
#endif

_Static_assert(RENDERER_ORIGINAL_ALIGNOF(lump_t) == 4,
               "renderer BSP lump alignment changed");
_Static_assert(offsetof(lump_t, filelen) == 0,
               "renderer BSP lump length moved");
_Static_assert(offsetof(lump_t, fileofs) == 4,
               "renderer BSP lump offset moved");
_Static_assert(sizeof(lump_t) == 8,
               "renderer BSP lump layout changed");

_Static_assert(RENDERER_ORIGINAL_ALIGNOF(dshader_t) == 4,
               "renderer disk-shader alignment changed");
_Static_assert(offsetof(dshader_t, shader) == 0,
               "renderer disk-shader name moved");
_Static_assert(offsetof(dshader_t, surfaceFlags) == 64,
               "renderer disk-shader surface flags moved");
_Static_assert(offsetof(dshader_t, contentFlags) == 68,
               "renderer disk-shader content flags moved");
_Static_assert(sizeof(dshader_t) == 72,
               "renderer disk-shader layout changed");

_Static_assert(sizeof(dsurface_t) == 16,
               "renderer disk-surface layout changed");
_Static_assert(RENDERER_ORIGINAL_ALIGNOF(dsurface_t) == 4,
               "renderer disk-surface alignment changed");
_Static_assert(offsetof(dsurface_t, shaderNum) == 0,
               "renderer disk-surface shader index moved");
_Static_assert(offsetof(dsurface_t, lightmapNum) == 2,
               "renderer disk-surface lightmap index moved");
_Static_assert(offsetof(dsurface_t, firstVert) == 4,
               "renderer disk-surface first vertex moved");
_Static_assert(offsetof(dsurface_t, numVerts) == 8,
               "renderer disk-surface vertex count moved");
_Static_assert(offsetof(dsurface_t, numIndexes) == 10,
               "renderer disk-surface index count moved");
_Static_assert(offsetof(dsurface_t, firstIndex) == 12,
               "renderer disk-surface first index moved");

_Static_assert(RENDERER_ORIGINAL_ALIGNOF(renderer_disk_aabb_tree_t) == 4,
               "renderer disk AABB-tree alignment changed");
_Static_assert(offsetof(renderer_disk_aabb_tree_t, firstSurface) == 0,
               "renderer disk AABB-tree first surface moved");
_Static_assert(offsetof(renderer_disk_aabb_tree_t, surfaceCount) == 4,
               "renderer disk AABB-tree surface count moved");
_Static_assert(offsetof(renderer_disk_aabb_tree_t, childCount) == 8,
               "renderer disk AABB-tree child count moved");
_Static_assert(sizeof(renderer_disk_aabb_tree_t) == 12,
               "renderer disk AABB-tree layout changed");

_Static_assert(RENDERER_ORIGINAL_ALIGNOF(renderer_disk_cell_t) == 4,
               "renderer disk-cell alignment changed");
_Static_assert(offsetof(renderer_disk_cell_t, mins) == 0,
               "renderer disk-cell minimum bounds moved");
_Static_assert(offsetof(renderer_disk_cell_t, maxs) == 12,
               "renderer disk-cell maximum bounds moved");
_Static_assert(offsetof(renderer_disk_cell_t, aabbTreeIndex) == 24,
               "renderer disk-cell AABB-tree index moved");
_Static_assert(offsetof(renderer_disk_cell_t, firstPortal) == 28,
               "renderer disk-cell first portal moved");
_Static_assert(offsetof(renderer_disk_cell_t, portalCount) == 32,
               "renderer disk-cell portal count moved");
_Static_assert(offsetof(renderer_disk_cell_t, firstCullGroup) == 36,
               "renderer disk-cell first cull group moved");
_Static_assert(offsetof(renderer_disk_cell_t, cullGroupCount) == 40,
               "renderer disk-cell cull-group count moved");
_Static_assert(offsetof(renderer_disk_cell_t, firstOccluder) == 44,
               "renderer disk-cell first occluder moved");
_Static_assert(offsetof(renderer_disk_cell_t, occluderCount) == 48,
               "renderer disk-cell occluder count moved");
_Static_assert(sizeof(renderer_disk_cell_t) == 52,
               "renderer disk-cell layout changed");

_Static_assert(RENDERER_ORIGINAL_ALIGNOF(renderer_disk_portal_t) == 4,
               "renderer disk-portal alignment changed");
_Static_assert(offsetof(renderer_disk_portal_t, planeIndex) == 0,
               "renderer disk-portal plane index moved");
_Static_assert(offsetof(renderer_disk_portal_t, cellIndex) == 4,
               "renderer disk-portal cell index moved");
_Static_assert(offsetof(renderer_disk_portal_t, firstVertex) == 8,
               "renderer disk-portal first vertex moved");
_Static_assert(offsetof(renderer_disk_portal_t, vertexCount) == 12,
               "renderer disk-portal vertex count moved");
_Static_assert(sizeof(renderer_disk_portal_t) == 16,
               "renderer disk-portal layout changed");

_Static_assert(RENDERER_ORIGINAL_ALIGNOF(renderer_disk_cull_group_t) == 4,
               "renderer disk cull-group alignment changed");
_Static_assert(offsetof(renderer_disk_cull_group_t, mins) == 0,
               "renderer disk cull-group minimum bounds moved");
_Static_assert(offsetof(renderer_disk_cull_group_t, maxs) == 12,
               "renderer disk cull-group maximum bounds moved");
_Static_assert(offsetof(renderer_disk_cull_group_t, firstSurface) == 24,
               "renderer disk cull-group first surface moved");
_Static_assert(offsetof(renderer_disk_cull_group_t, surfaceCount) == 28,
               "renderer disk cull-group surface count moved");
_Static_assert(sizeof(renderer_disk_cull_group_t) == 32,
               "renderer disk cull-group layout changed");

_Static_assert(RENDERER_ORIGINAL_ALIGNOF(renderer_disk_occluder_t) == 4,
               "renderer disk-occluder alignment changed");
_Static_assert(offsetof(renderer_disk_occluder_t, firstPlane) == 0,
               "renderer disk-occluder first plane moved");
_Static_assert(offsetof(renderer_disk_occluder_t, planeCount) == 4,
               "renderer disk-occluder plane count moved");
_Static_assert(offsetof(renderer_disk_occluder_t, edgeCount) == 6,
               "renderer disk-occluder edge count moved");
_Static_assert(offsetof(renderer_disk_occluder_t, firstEdge) == 8,
               "renderer disk-occluder first edge moved");
_Static_assert(offsetof(renderer_disk_occluder_t, firstVertex) == 12,
               "renderer disk-occluder first vertex moved");
_Static_assert(offsetof(renderer_disk_occluder_t, vertexCount) == 16,
               "renderer disk-occluder vertex count moved");
_Static_assert(sizeof(renderer_disk_occluder_t) == 20,
               "renderer disk-occluder layout changed");

_Static_assert(RENDERER_ORIGINAL_ALIGNOF(renderer_disk_occluder_edge_t) == 1,
               "renderer disk occluder-edge alignment changed");
_Static_assert(offsetof(renderer_disk_occluder_edge_t, planeIndices) == 0,
               "renderer disk occluder-edge plane indexes moved");
_Static_assert(offsetof(renderer_disk_occluder_edge_t, vertexIndices) == 2,
               "renderer disk occluder-edge vertex indexes moved");
_Static_assert(sizeof(renderer_disk_occluder_edge_t) == 4,
               "renderer disk occluder-edge layout changed");

_Static_assert(RENDERER_ORIGINAL_ALIGNOF(renderer_lightmap_placement_t) == 4,
               "renderer lightmap-placement alignment changed");
_Static_assert(offsetof(renderer_lightmap_placement_t, atlasIndex) == 0,
               "renderer lightmap-placement atlas index moved");
_Static_assert(offsetof(renderer_lightmap_placement_t, sOffset) == 4,
               "renderer lightmap-placement alignment changed");
_Static_assert(offsetof(renderer_lightmap_placement_t, tOffset) == 8,
               "renderer lightmap-placement t offset moved");
_Static_assert(offsetof(renderer_lightmap_placement_t, sScale) == 12,
               "renderer lightmap-placement s scale moved");
_Static_assert(offsetof(renderer_lightmap_placement_t, tScale) == 16,
               "renderer lightmap-placement t scale moved");
_Static_assert(sizeof(renderer_lightmap_placement_t) == 20,
               "renderer lightmap-placement layout changed");

#undef RENDERER_ORIGINAL_ALIGNOF
#endif

enum {
    R_MAX_ENTITY_LIGHT_CANDIDATES = 50,
    R_MAX_DRAW_SURFS = 65536,
    R_MAX_ENTITY_SURFACES = 4096,
    R_MAX_SCENE_ENTITIES = 1024,
    R_MAX_DLIGHTS = 32,
    R_MAX_CORONAS = 32,
    R_BASE_SCENE_POLYS = 4096,
    R_BASE_SCENE_POLY_VERTICES = 16384,
    R_MAX_TESS_VERTICES = 65536,
    R_MAX_TESS_INDEXES = 393216,
    R_MAX_OPTIMIZED_TESS_INDEXES = 131072,
    R_MAX_TESS_XYZ_COMPONENTS = 4,
    R_TESS_TEXCOORD_SET_COUNT = 4,
    R_TESS_BASE_TEXCOORD_SET = 0,
    R_TESS_LIGHTMAP_TEXCOORD_SET = 2,
    R_MAX_SHADER_STAGES = 8,
    R_MAX_SHADER_TEXMODS = 4,
    R_DYNAMIC_TESS_STORAGE = 3,
    R_LIGHT_VIS_BUCKET_COUNT = 8192,
    R_LIGHT_VIS_ENTRIES_PER_BUCKET = 32,
    R_LIGHT_VIS_HISTORY_MAX_ENTRIES = 4194304,
    R_LIGHT_VIS_MAX_LEAF_LIGHTS = 16,
    RENDERER_ENTITY_FORCE_MIN_LIGHT = 0x20
};

typedef enum renderer_primitive_mode_e {
    R_PRIMITIVES_NONE = -1,
    R_PRIMITIVES_AUTOMATIC = 0,
    R_PRIMITIVES_ARRAY_ELEMENTS = 1,
    R_PRIMITIVES_DRAW_ELEMENTS = 2,
    R_PRIMITIVES_IMMEDIATE = 3
} renderer_primitive_mode_t;

enum renderer_draw_sort_e {
    R_SORT_SHADER_SHIFT = 18,
    R_SORT_SHADER_MASK = 4095,
    R_SORT_ENTITY_SHIFT = 8,
    R_SORT_ENTITY_MASK = 1023,
    R_SORT_STORAGE_SHIFT = 30,
    R_SORT_STORAGE_MASK = 3,
    R_SORT_BATCH_FLAG0 = 0x01,
    R_SORT_WORLD_ENTITY = 0x02,
    R_SORT_BATCH_FLAG2 = 0x04
};

enum {
    R_WORLD_ENTITY_NUMBER = 1022
};

typedef enum renderer_show_leaf_lights_mode_e {
    R_SHOW_LEAF_LIGHTS_GRID = 2,
    R_SHOW_LEAF_LIGHTS_VIEW_CONE = 3,
    R_SHOW_LEAF_LIGHTS_NEARBY = 4
} renderer_show_leaf_lights_mode_t;

enum renderer_light_vis_sample_state_e {
    R_LIGHT_VIS_SAMPLE_EMPTY = 0,
    R_LIGHT_VIS_SAMPLE_VALID = 1,
    R_LIGHT_VIS_SAMPLE_BLOCKED = 2
};

enum {
    R_DEBUG_STRING_TEXT_SIZE = 96,
    R_DEBUG_POLYGON_VERTEX_CAPACITY = 4096,
    R_DEBUG_POLYGON_CAPACITY = 512,
    R_DEBUG_STRING_CAPACITY = 4096,
    R_DEBUG_LINE_CAPACITY = 16384,
    R_PLUME_CAPACITY = 4096,
    RB_IMMEDIATE_VERTEX_CAPACITY = 8192,
    RB_IMMEDIATE_VERTEX_COMPONENTS = 3
};

typedef struct renderer_debug_string_s {
    vec3_t origin;                         /* original +0x00 */
    vec4_t color;                          /* original +0x0c */
    float scale;                           /* original +0x1c */
    char text[R_DEBUG_STRING_TEXT_SIZE];   /* original +0x20 */
} renderer_debug_string_t;

typedef struct renderer_debug_polygon_s {
    vec4_t color;                            /* original +0x00 */
    int32_t firstVertex;                    /* original +0x10 */
    int32_t vertexCount;                    /* original +0x14 */
} renderer_debug_polygon_t;

typedef struct renderer_debug_line_s {
    vec3_t start;                            /* original +0x00 */
    vec3_t end;                              /* original +0x0c */
    vec4_t color;                            /* original +0x18 */
    qboolean depthTest;                      /* original +0x28 */
} renderer_debug_line_t;

typedef struct renderer_plume_s {
    vec3_t origin;                           /* original +0x00 */
    vec4_t color;                            /* +0x0c: RGB input; live fade alpha */
    int32_t labelValue;                      /* +0x1c: formatted decimal label */
    int32_t startTime;                       /* original +0x20 */
    int32_t duration;                        /* original +0x24 */
} renderer_plume_t;

typedef struct rb_immediate_vertex_s {
    vec3_t xyz;                              /* original +0x00 */
    vec2_t texCoord;                         /* original +0x0c */
    uint8_t unused14[8];                     /* CoDUOMP +0x14..+0x1b: zeroed at
                                              * allocation; no original writer,
                                              * reader, or GL array accesses it. */
    uint8_t color[4];                        /* original +0x1c */
} rb_immediate_vertex_t;

/* Original contiguous state at 0x04898b78..0x04898bf7. Pointer members are
 * native-width source fields; only the recovered Windows i386 layout is
 * required to retain these exact offsets. */
typedef struct renderer_debug_state_s {
    int32_t polygonVertexCapacity;           /* original +0x00 */
    int32_t polygonVertexCount;              /* original +0x04 */
    vec3_t *polygonVertices;                 /* original +0x08 */
    int32_t polygonCapacity;                 /* original +0x0c */
    int32_t polygonCount;                    /* original +0x10 */
    renderer_debug_polygon_t *polygons;      /* original +0x14 */
    int32_t stringCapacity;                  /* original +0x18 */
    int32_t stringCount;                     /* original +0x1c */
    renderer_debug_string_t *strings;        /* original +0x20 */
    fontInfo_t *font;                          /* original +0x24 */
    int32_t locatedStringCount;              /* original +0x28 */
    const struct client_debug_string_s *locatedStrings; /* +0x2c */
    int32_t lineCapacity;                    /* original +0x30 */
    int32_t lineCount;                       /* original +0x34 */
    renderer_debug_line_t *lines;            /* original +0x38 */
    int32_t locatedLineCount;                /* original +0x3c */
    const struct client_debug_line_s *locatedLines; /* original +0x40 */
    int32_t plumeCount;                      /* original +0x44 */
    int32_t plumeCapacity;                   /* original +0x48 */
    renderer_plume_t *plumes;                /* original +0x4c */
    qboolean immediateModeActive;            /* original +0x50 */
    uint32_t immediatePrimitiveMode;         /* original +0x54 */
    float immediateLineWidth;                /* original +0x58 */
    vec2_t immediateTexCoord;                /* original +0x5c */
    uint8_t immediateColor[4];               /* original +0x64 */
    uint32_t unusedImmediateState[3];        /* original +0x68..+0x73; unused by CoDUOMP.exe */
    int32_t immediateVertexCount;            /* original +0x74 */
    int32_t immediateVertexCapacity;         /* original +0x78 */
    rb_immediate_vertex_t *immediateVertices; /* original +0x7c */
} renderer_debug_state_t;

extern renderer_debug_state_t rendererDebugState;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(renderer_debug_string_t) == 4,
               "i386 renderer debug-string alignment changed");
_Static_assert(offsetof(renderer_debug_string_t, origin) == 0x00,
               "i386 renderer debug-string origin moved");
_Static_assert(offsetof(renderer_debug_string_t, color) == 0x0c,
               "i386 renderer debug-string color moved");
_Static_assert(offsetof(renderer_debug_string_t, scale) == 0x1c,
               "i386 renderer debug-string scale moved");
_Static_assert(offsetof(renderer_debug_string_t, text) == 0x20,
               "i386 renderer debug-string text moved");
_Static_assert(sizeof(renderer_debug_string_t) == 0x80,
               "i386 renderer debug-string size changed");
_Static_assert(_Alignof(renderer_debug_polygon_t) == 4,
               "i386 renderer debug-polygon alignment changed");
_Static_assert(offsetof(renderer_debug_polygon_t, color) == 0x00,
               "i386 renderer debug-polygon color moved");
_Static_assert(offsetof(renderer_debug_polygon_t, firstVertex) == 0x10,
               "i386 renderer debug-polygon first vertex moved");
_Static_assert(offsetof(renderer_debug_polygon_t, vertexCount) == 0x14,
               "i386 renderer debug-polygon vertex count moved");
_Static_assert(sizeof(renderer_debug_polygon_t) == 0x18,
               "renderer debug-polygon record size changed");
_Static_assert(_Alignof(renderer_debug_line_t) == 4,
               "i386 renderer debug-line alignment changed");
_Static_assert(offsetof(renderer_debug_line_t, start) == 0x00,
               "i386 renderer debug-line start moved");
_Static_assert(offsetof(renderer_debug_line_t, end) == 0x0c,
               "i386 renderer debug-line end moved");
_Static_assert(offsetof(renderer_debug_line_t, color) == 0x18,
               "i386 renderer debug-line color moved");
_Static_assert(offsetof(renderer_debug_line_t, depthTest) == 0x28,
               "i386 renderer debug-line depth flag moved");
_Static_assert(sizeof(renderer_debug_line_t) == 0x2c,
               "renderer debug-line record size changed");
_Static_assert(_Alignof(renderer_plume_t) == 4,
               "i386 renderer plume alignment changed");
_Static_assert(offsetof(renderer_plume_t, origin) == 0x00,
               "i386 renderer plume origin moved");
_Static_assert(offsetof(renderer_plume_t, color) == 0x0c,
               "i386 renderer plume color moved");
_Static_assert(offsetof(renderer_plume_t, labelValue) == 0x1c,
               "i386 renderer plume label value moved");
_Static_assert(offsetof(renderer_plume_t, startTime) == 0x20,
               "i386 renderer plume start time moved");
_Static_assert(offsetof(renderer_plume_t, duration) == 0x24,
               "i386 renderer plume duration moved");
_Static_assert(sizeof(renderer_plume_t) == 0x28,
               "renderer plume record size changed");
_Static_assert(offsetof(rb_immediate_vertex_t, xyz) == 0x00,
               "renderer immediate vertex position moved");
_Static_assert(offsetof(rb_immediate_vertex_t, texCoord) == 0x0c,
               "renderer immediate vertex texture coordinate moved");
_Static_assert(offsetof(rb_immediate_vertex_t, unused14) == 0x14,
               "renderer immediate vertex unused region moved");
_Static_assert(offsetof(rb_immediate_vertex_t, color) == 0x1c,
               "renderer immediate vertex color moved");
_Static_assert(sizeof(rb_immediate_vertex_t) == 32,
               "renderer immediate vertex size changed");

_Static_assert(offsetof(renderer_debug_state_t, polygonVertices) == 0x08,
               "i386 renderer debug polygon-vertex pointer moved");
_Static_assert(offsetof(renderer_debug_state_t, strings) == 0x20,
               "i386 renderer debug-string pointer moved");
_Static_assert(offsetof(renderer_debug_state_t, plumes) == 0x4c,
               "i386 renderer plume pointer moved");
_Static_assert(offsetof(renderer_debug_state_t, immediateModeActive) == 0x50,
               "i386 renderer immediate state moved");
_Static_assert(offsetof(renderer_debug_state_t, immediateVertices) == 0x7c,
               "i386 renderer immediate-vertex pointer moved");
_Static_assert(sizeof(renderer_debug_state_t) == 0x80,
               "i386 renderer debug-state size changed");
#endif

typedef struct renderer_corona_s {
    vec3_t origin;                          /* original +0x00 */
    vec3_t color;                           /* original +0x0c */
    vec3_t transformed;                     /* original +0x18; RTCW-lineage
                                              * local-space origin slot, unused by
                                              * CoDUOMP's producer and consumer */
    float scale;                            /* original +0x24 */
    int32_t id;                             /* original +0x28 */
    int32_t flags;                          /* original +0x2c */
} renderer_corona_t;

enum {
    /* RE_AddCoronaToScene stores the caller's final integer unchanged.
     * RB_AddCoronaFlares uses this bit to select the built-in spotLight
     * material instead of flareShader. */
    R_CORONA_FLAG_SPOT_LIGHT_SHADER = 0x2
};

/* Persistent sun-sprite, flare, blindness, and glare state beginning at
 * original 0x0389c4d8. R_SetSunFromCvars fills the configuration prefix;
 * R_ClearSun and RB_CalcSunBlind own the three live values at the tail.
 * Pointer fields are native-width source fields. */
typedef struct renderer_sun_state_s {
    shader_t *spriteShader;                  /* original +0x00 */
    vec4_t spriteVertices[4];                /* original +0x04 */
    float spriteSize;                        /* original +0x44 */
    shader_t *flareShader;                   /* original +0x48 */
    float flareMinHalfSize;                  /* original +0x4c */
    float flareMinCosAngle;                  /* original +0x50 */
    float flareMaxHalfSize;                  /* original +0x54 */
    float flareMaxCosAngle;                  /* original +0x58 */
    float flareMaxAlpha;                     /* original +0x5c */
    int32_t flareFadeInMsec;                 /* original +0x60 */
    int32_t flareFadeOutMsec;                /* original +0x64 */
    float blindMinCosAngle;                  /* original +0x68 */
    float blindMaxCosAngle;                  /* original +0x6c */
    float blindMaxDarken;                    /* original +0x70 */
    int32_t blindFadeInMsec;                 /* original +0x74 */
    int32_t blindFadeOutMsec;                /* original +0x78 */
    float glareMinCosAngle;                  /* original +0x7c */
    float glareMaxCosAngle;                  /* original +0x80 */
    float glareMaxLighten;                   /* original +0x84 */
    int32_t glareFadeInMsec;                 /* original +0x88 */
    int32_t glareFadeOutMsec;                /* original +0x8c */
    float currentBlindFraction;              /* original +0x90 */
    float currentGlareFraction;              /* original +0x94 */
    int32_t lastUpdateTime;                  /* original +0x98 */
} renderer_sun_state_t;

/* Input record passed to RB_AddFlare. The optional direction argument lets
 * ordinary flares modulate their color by view direction; sun flares pass
 * NULL. */
typedef struct renderer_flare_source_s {
    int32_t id;                              /* original +0x00 */
    shader_t *shader;                        /* original +0x04 */
    vec3_t origin;                           /* original +0x08 */
    float depthOffset;                       /* original +0x14 */
    vec4_t color;                            /* original +0x18; color[3] is unused by CoDUOMP.exe */
    float size;                              /* original +0x28 */
    int32_t screenRadius;                    /* original +0x2c */
    int32_t fadeInMsec;                      /* original +0x30 */
    int32_t fadeOutMsec;                     /* original +0x34 */
    qboolean active;                         /* original +0x38 */
} renderer_flare_source_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(renderer_sun_state_t, spriteVertices) == 0x04,
               "i386 renderer sun sprite vertices moved");
_Static_assert(offsetof(renderer_sun_state_t, spriteSize) == 0x44,
               "i386 renderer sun sprite size moved");
_Static_assert(offsetof(renderer_sun_state_t, flareShader) == 0x48,
               "i386 renderer sun flare shader moved");
_Static_assert(offsetof(renderer_sun_state_t, lastUpdateTime) == 0x98,
               "i386 renderer sun update time moved");
_Static_assert(sizeof(renderer_sun_state_t) == 0x9c,
               "i386 renderer sun-state size changed");
_Static_assert(sizeof(renderer_flare_source_t) == 0x3c,
               "i386 renderer flare-source size changed");
#endif

/* R_LoadNodesAndLeafs stores internal BSP nodes and terminal leaves in one
 * array with a shared prefix and a role-dependent 16-byte tail.  The Windows
 * loader, R_SetParentAndCell, R_PointInLeaf, and R_BoxSurfaces_r prove every
 * member below. */
struct mnode_s {
    int32_t contents;                    /* original +0x00; R_WORLD_NODE_NO_CELL (-1) for a node */
    mnode_t *parent;                     /* original +0x04 */
    int32_t cellIndex;                   /* original +0x08 */
    union {
        struct {
            cplane_t *plane;               /* original +0x0c */
            mnode_t *children[2];         /* original +0x10 */
        } node;
        struct {
            int32_t cluster;             /* original +0x0c */
            qboolean hasSunLight;        /* original +0x10 */
            int32_t firstLightIndex;     /* original +0x14 */
            int32_t lightCount;          /* original +0x18 */
        } leaf;
    } data;
};

enum renderer_world_node_cell_e {
    R_WORLD_NODE_NO_CELL = -1,
    R_WORLD_NODE_INTERNAL = -2
};

typedef struct renderer_light_vis_cache_entry_s {
    uint32_t key;                        /* original +0x00 */
    uint8_t sampleState;                 /* original +0x04 */
    uint8_t diffuseSunVisibility;        /* original +0x05 */
    uint16_t visibleLightBits;           /* original +0x06 */
} renderer_light_vis_cache_entry_t;

typedef struct renderer_light_vis_history_entry_s {
    int32_t gridX;     /* original +0x00 */
    int32_t gridY;     /* original +0x04 */
    int32_t gridZ;     /* original +0x08 */
    vec3_t traceTarget; /* original +0x0c */
} renderer_light_vis_history_entry_t;

/* The sorted lookup stores three signed grid coordinates in a record whose
 * original stride is eight bytes. The final two bytes are copied by the
 * Windows insert path but are never read, and their source meaning is not
 * proved. */
typedef struct renderer_light_vis_sort_entry_s {
    int16_t gridX;               /* original +0x00 */
    int16_t gridY;               /* original +0x02 */
    int16_t gridZ;               /* original +0x04 */
    uint8_t unconsumed006[2];    /* +0x06; unused by CoDUOMP.exe. */
} renderer_light_vis_sort_entry_t;

typedef struct renderer_light_vis_disk_entry_s {
    uint32_t key;                    /* disk +0x00 */
    uint8_t sampleState;             /* disk +0x04 */
    uint8_t diffuseSunVisibility[5]; /* disk +0x05 */
    uint16_t visibleLightBits;       /* disk +0x0a */
} renderer_light_vis_disk_entry_t;

enum {
    R_MAX_STATIC_LIGHT_CANDIDATES = 49
};

/* Persistent world-static-model record built by R_AddStaticModelToWorld. It
 * owns the renderer entity, world bounds, precomputed light candidates, and a
 * variable surface-lighting cache sized from all registered LOD surfaces. */
struct renderer_static_model_s {
    refEntity_t entity;                    /* original +0x000 */
    vec3_t mins;                            /* original +0x09c */
    vec3_t maxs;                            /* original +0x0a8 */
    int32_t viewCount;                      /* original +0x0b4 */
    int32_t lightCount;                    /* original +0x0b8 */
    float diffuseSunContribution;          /* original +0x0bc */
    float contributions[R_MAX_STATIC_LIGHT_CANDIDATES]; /* +0x0c0 */
    renderer_light_t *lights[R_MAX_STATIC_LIGHT_CANDIDATES]; /* +0x184 */
    renderer_cached_static_model_surface_t
        *surfaceLightingCache[];            /* original +0x248 */
};

typedef struct renderer_static_model_t2v3_vertex_s {
    vec2_t texCoord; /* original +0x00 */
    vec3_t position; /* original +0x08 */
} renderer_static_model_t2v3_vertex_t;

typedef struct renderer_static_model_t2n3v3_vertex_s {
    vec2_t texCoord; /* original +0x00 */
    vec3_t normal;   /* original +0x08 */
    vec3_t position; /* original +0x14 */
} renderer_static_model_t2n3v3_vertex_t;

/* The optimizer tail is backend-specific: generic/NV retain a host pointer,
 * ARB stores its vertex-buffer name, and ATI stores its object-buffer name. */
typedef union renderer_static_model_optimized_data_u {
    uint8_t *vertices;        /* original +0x00 */
    uint32_t vertexBuffer;    /* original +0x00, ARB */
    uint32_t atiObjectBuffer; /* original +0x00, ATI */
} renderer_static_model_optimized_data_t;

typedef union renderer_static_model_backend_data_u {
    struct {
        uint32_t indexBuffer; /* original +0x00 */
    } arb;
    struct {
        uint32_t vertexOffset; /* original +0x00 */
        uint32_t indexOffset;  /* original +0x04 */
    } ati;
} renderer_static_model_backend_data_t;

/* Per-shader surface assembled by R_FinishDObjToStaticModel. The backend
 * optimizers replace surfaceType/storageSource and build their interleaved
 * vertex representation from the four source arrays. */
typedef struct renderer_static_model_surface_s {
    renderer_surface_type_t surfaceType;          /* original +0x00 */
    renderer_static_vertex_memory_source_t storageSource; /* +0x04 */
    shader_t *shader;                             /* original +0x08 */
    shader_t *cachedShader;                       /* original +0x0c */
    uint16_t indexCount;                          /* original +0x10 */
    uint16_t vertexCount;                         /* original +0x12 */
    vec2_t *texCoords;                            /* original +0x14 */
    vec3_t *normals;                              /* original +0x18 */
    vec3_t *vertices;                             /* original +0x1c */
    uint16_t *indices;                            /* original +0x20 */
    renderer_static_model_optimized_data_t optimized; /* original +0x24 */
    renderer_static_model_backend_data_t backend;     /* original +0x28 */
} renderer_static_model_surface_t;

/* Intrusive cache-list prefix shared by page LRU entries and free surface
 * blocks. The Windows cache uses the same two-pointer representation for both
 * roles; native builds retain that relationship with host-width pointers. */
typedef struct renderer_static_model_cache_link_s {
    struct renderer_static_model_cache_link_s *next;     /* original +0x00 */
    struct renderer_static_model_cache_link_s *previous; /* original +0x04 */
} renderer_static_model_cache_link_t;

/* One allocated static-model lighting surface, or the free-list link occupying
 * the same storage while unused. The first word is also the renderer surface
 * dispatch selector, which is why the four cached-static-model backend values
 * are shared with shader_optimized_backend_t. */
union renderer_cached_static_model_surface_u {
    renderer_static_model_cache_link_t freeLink;
    struct {
        renderer_surface_type_t surfaceType;          /* original +0x00 */
        renderer_static_model_surface_t *source;      /* original +0x04 */
        int32_t vertexOffset;                         /* original +0x08 */
        int32_t surfaceIndex;                         /* original +0x0c */
        renderer_static_model_t *owner;               /* original +0x10 */
    } cached;
};

typedef struct renderer_static_model_lod_s {
    int32_t surfaceCount;                       /* original +0x00 */
    renderer_static_model_surface_t surfaces[]; /* original +0x04 */
} renderer_static_model_lod_t;

/* Renderer registration record for one named DObj-derived static model. The
 * variable-length LOD pointer array is followed by the owned name string in
 * the same hunk allocation. */
typedef struct renderer_registered_static_model_s {
    char *name;                                  /* original +0x00 */
    int32_t lodCount;                            /* original +0x04 */
    XModel *model;                             /* original +0x08 */
    renderer_static_model_lod_t *lods[];         /* original +0x0c */
} renderer_registered_static_model_t;

/* Persistent misc-model instance parsed before the BSP world is finalized.
 * R_CreateStaticModel prepends these records to a private list; the finish
 * pass later converts each one into a renderer_static_model_t. */
typedef struct renderer_static_model_instance_s {
    char name[MAX_QPATH];                         /* original +0x00 */
    vec3_t origin;                               /* original +0x40 */
    axis_t axis;                                 /* original +0x4c */
    /* Zero for unit axes; otherwise the maximum authored scale, used to
     * correct the LOD distance after the scale is folded into axis. */
    float nonNormalizedAxes;                     /* original +0x70 */
    vec3_t lightingPrecalc;                      /* original +0x74 */
    renderer_registered_static_model_t *registration; /* original +0x80 */
    vec3_t mins;                                 /* original +0x84 */
    vec3_t maxs;                                 /* original +0x90 */
    struct renderer_static_model_instance_s *next; /* original +0x9c */
} renderer_static_model_instance_t;

enum renderer_shader_lighting_flags_e {
    SHADER_LIGHTING_AMBIENT = 0x08,
    SHADER_LIGHTING_DIFFUSE = 0x10,
    SHADER_LIGHTING_ENTITY_MASK =
        SHADER_LIGHTING_AMBIENT | SHADER_LIGHTING_DIFFUSE
};

/* Every renderer surface begins with this dispatch selector. Some surface
 * kinds, including the fixed entity surface, contain no other front-end data. */
typedef struct renderer_surface_s {
    renderer_surface_type_t surfaceType;    /* original +0x00 */
} renderer_surface_t;

/* World surface kinds 24 and above share this front-end prefix. The bounds
 * are tested against model-space dynamic lights before the surviving mask is
 * stored back into the surface. */
typedef struct renderer_lit_surface_s {
    renderer_surface_type_t surfaceType;    /* original +0x00 */
    renderer_static_vertex_memory_source_t storageMode; /* original +0x04 */
    uint32_t dlightBits;                    /* original +0x08 */
    vec3_t boundsMin;                       /* original +0x0c */
    vec3_t boundsMax;                       /* original +0x18 */
} renderer_lit_surface_t;

/* World mesh surfaces use the same pointer-rich representation for the
 * unoptimized and four optimized back ends. BuildOptimizedSurface allocates
 * and assigns all seven vertex arrays plus the flexible index tail; later
 * back ends select which arrays they consume. */
typedef struct renderer_world_mesh_surface_s {
    renderer_surface_type_t surfaceType;     /* original +0x00 */
    renderer_static_vertex_memory_source_t storageMode; /* original +0x04 */
    uint32_t dlightBits;                     /* original +0x08 */
    vec3_t boundsMin;                        /* original +0x0c */
    vec3_t boundsMax;                        /* original +0x18 */
    int32_t vertexCount;                     /* original +0x24 */
    vec3_t *tangents;                        /* original +0x28, optional */
    vec3_t *bitangents;                      /* original +0x2c, optional */
    vec3_t *normals;                         /* original +0x30, optional */
    vec2_t *texCoords;                       /* original +0x34 */
    vec2_t *lightmapCoords;                  /* original +0x38 */
    uint8_t (*colors)[4];                    /* original +0x3c */
    vec3_t *positions;                       /* original +0x40 */
    int32_t indexCount;                      /* original +0x44 */
    uint16_t indices[];                      /* original +0x48 */
} renderer_world_mesh_surface_t;

/* The optimized world stream interleaves two texture-coordinate pairs, the
 * four color bytes, and a three-float position into the original 32 bytes. */
typedef struct renderer_world_interleaved_vertex_s {
    vec2_t texCoord;       /* original +0x00 */
    vec2_t lightmapCoord;  /* original +0x08 */
    uint8_t color[4];      /* original +0x10 */
    vec3_t position;       /* original +0x14 */
} renderer_world_interleaved_vertex_t;

/* Shared static-model cache stream. Unlike optimized world geometry it has
 * one texture-coordinate pair and therefore occupies 24 bytes per vertex. */
typedef struct renderer_cached_static_model_vertex_s {
    vec2_t texCoord;  /* original +0x00 */
    uint8_t color[4]; /* original +0x08 */
    vec3_t position;  /* original +0x0c */
} renderer_cached_static_model_vertex_t;

/* Per-shader cursor and storage selection shared by BeginShaderSurfaces and
 * each BuildOptimizedSurface call made while loading that shader's meshes. */
typedef struct renderer_shader_surface_build_s {
    qboolean optimized;                              /* original +0x00 */
    size_t vertexBytes;                              /* original +0x04 */
    int32_t firstVertex;                             /* original +0x08 */
    renderer_static_vertex_memory_source_t storageMode; /* original +0x0c */
    shader_t *shader;                                /* original +0x10 */
} renderer_shader_surface_build_t;

/* BSP and brush-model lists use the same 12-byte world-surface record. */
typedef struct msurface_s {
    int32_t viewCount;                      /* original +0x00 */
    shader_t *shader;                       /* original +0x04 */
    renderer_surface_t *data;               /* original +0x08 */
} msurface_t;

enum renderer_shader_mark_flags_e {
    /* R_AABBTreeSurfaces_r and R_CellSurfaces exclude a world surface when
     * this shader word contains bit 0x20. Exact original flag name unproved. */
    SHADER_MARKS_DISABLED = 0x20
};

/* Mark projection traverses a 40-byte bounds tree. Internal records own a
 * contiguous child array; terminal records own 12-byte world-surface entries. */
typedef struct renderer_aabb_tree_s renderer_aabb_tree_t;
struct renderer_aabb_tree_s {
    vec3_t mins;                            /* original +0x00 */
    vec3_t maxs;                            /* original +0x0c */
    msurface_t *surfaces;                   /* original +0x18 */
    int32_t surfaceCount;                   /* original +0x1c */
    renderer_aabb_tree_t *children;         /* original +0x20 */
    int32_t childCount;                     /* original +0x24 */
};

/* Cull groups are authored bounds around contiguous world-surface spans.
 * R_AddCellCullGroups compares the runtime-only tail against tr.viewCount so
 * each group is submitted at most once in a view. */
typedef struct renderer_cull_group_s {
    vec3_t mins;                            /* original +0x00 */
    vec3_t maxs;                            /* original +0x0c */
    msurface_t *surfaces;                   /* original +0x18 */
    int32_t surfaceCount;                   /* original +0x1c */
    int32_t viewCount;                      /* original +0x20 */
} renderer_cull_group_t;

/* R_PortalClipPlanes owns two fixed banks of this many vec3_t vertices. */
enum { R_PORTAL_CLIP_VERTEX_CAPACITY = 1024 };

/* Both portal and occluder planes use three precomputed byte offsets that
 * select the appropriate mins/maxs component during DPVS clipping. */
typedef struct renderer_portal_s {
    renderer_dpvs_plane_t plane;            /* original +0x00 */
    struct renderer_world_cell_s *cell;     /* original +0x14 */
    vec3_t *vertices;                       /* original +0x18 */
    int32_t vertexCount;                    /* original +0x1c */
    qboolean recursionActive;               /* original +0x20 */
} renderer_portal_t;

/* Runtime form of a disk occluder edge after its four byte indexes have been
 * resolved against the owning occluder's plane and vertex spans. */
typedef struct renderer_occluder_edge_s {
    renderer_dpvs_plane_t *planes[2];       /* original +0x00 */
    vec3_t *vertices[2];                    /* original +0x08 */
} renderer_occluder_edge_t;

/* Runtime occluder. The first six fields own the authored plane, edge, and
 * vertex spans; DPVS owns the cull limit and active-plane work state. */
typedef struct renderer_occluder_s {
    renderer_dpvs_plane_t *planes;          /* original +0x00 */
    int32_t planeCount;                     /* original +0x04 */
    int32_t edgeCount;                      /* original +0x08 */
    renderer_occluder_edge_t *edges;        /* original +0x0c */
    int32_t vertexCount;                    /* original +0x10 */
    vec3_t *vertices;                       /* original +0x14 */
    /* Portal traversal initializes this to INT_MAX and lowers it to the first
     * plane position that culls the occluder. */
    int32_t cullPlaneLimit;                 /* original +0x18 */
    int32_t activePlaneCount;               /* original +0x1c */
    renderer_dpvs_plane_t *activePlanes;    /* original +0x20 */
} renderer_occluder_t;

/* Cells are loaded before portals. The original i386 loader temporarily puts
 * firstPortal*sizeof(renderer_portal_t) in the pointer slot, then divides it
 * back to an index after the portal allocation. This explicit union preserves
 * that two-phase role without manufacturing a pointer from an integer on
 * native 64-bit targets. */
typedef union renderer_cell_portal_reference_u {
    renderer_portal_t *portals;
    ptrdiff_t firstPortal;
} renderer_cell_portal_reference_t;

/* Per-cell membership link built while a static model is filtered through the
 * world BSP. Cells retain model identity only; the static-model record owns
 * its geometry and lighting state. */
typedef struct renderer_cell_model_link_s {
    renderer_static_model_t *model;          /* original +0x00 */
    struct renderer_cell_model_link_s *next; /* original +0x04 */
} renderer_cell_model_link_t;

/* Per-view model filtering builds a separate bounded-entity list in each
 * cell. R_AddModelToCell merges the bounds when the same renderer entity is
 * encountered again and otherwise prepends one of these transient records. */
typedef struct renderer_cell_entity_link_s {
    trRefEntity_t *entity;                    /* original +0x00 */
    vec3_t mins;                              /* original +0x04 */
    vec3_t maxs;                              /* original +0x10 */
    struct renderer_cell_entity_link_s *next; /* original +0x1c */
} renderer_cell_entity_link_t;

/* World cells are 64-byte i386 records. BSP loading proves the bounds and all
 * six authored list fields; mark projection and static-model filtering prove
 * the runtime-only visit stamp and model-link head. */
typedef struct renderer_world_cell_s {
    vec3_t mins;                            /* original +0x00 */
    vec3_t maxs;                            /* original +0x0c */
    renderer_aabb_tree_t *aabbTree;         /* original +0x18 */
    renderer_cell_portal_reference_t portalReference; /* original +0x1c */
    int32_t portalCount;                    /* original +0x20 */
    renderer_cull_group_t **cullGroups;     /* original +0x24 */
    int32_t cullGroupCount;                 /* original +0x28 */
    renderer_occluder_t **occluders;        /* original +0x2c */
    int32_t occluderCount;                  /* original +0x30 */
    int32_t markViewCount;                  /* original +0x34 */
    renderer_cell_model_link_t *modelLinks; /* original +0x38 */
    renderer_cell_entity_link_t *entityLinks; /* original +0x3c */
} renderer_world_cell_t;

/* Five-float temporary vertex clipped by R_ChopPolyBehindPlane. The final two
 * floats are interpolated world-surface lightmap coordinates and are copied to
 * polyVert_t before RE_MarkFragments computes the projected mark ST. */
typedef struct renderer_mark_clip_vertex_s {
    vec3_t xyz;            /* original +0x00 */
    vec2_t lightmapCoords; /* original +0x0c */
} renderer_mark_clip_vertex_t;

enum renderer_mark_clip_limits_e {
    R_MARK_CLIP_BUFFER_COUNT = 2,
    R_MARK_CLIP_MAX_VERTICES = 64
};

/* Transient front-end record expanded by the RB_SurfaceXModel* dispatch
 * family. R_AddEntityDrawSurf selects the first word and fills the remaining
 * three inputs before queueing the record as a draw surface. */
typedef struct renderer_entity_surface_s {
    renderer_surface_t base;                /* original +0x00 */
    DObj *obj;                            /* original +0x04 */
    XSurface *surface;              /* original +0x08 */
    int32_t modelIndex;                     /* original +0x0c */
} renderer_entity_surface_t;

/* Quake III's renderer-local bmodel_t survives unchanged: authored bounds
 * followed by its contiguous world-surface span. Windows and PPC Mac loaders
 * both build the exact inherited 0x20-byte record. */
typedef struct bmodel_s {
    vec3_t bounds[2];                       /* original +0x00 */
    msurface_t *firstSurface;               /* original +0x18 */
    int32_t numSurfaces;                    /* original +0x1c */
} bmodel_t;

enum shader_flags_e {
    /* Any of these surface flags prevents selection of an optimized rigid
     * XSurface backend in R_AddEntityDrawSurf. Exact source name unresolved. */
    SHADER_XMODEL_OPTIMIZATION_BLOCK_MASK = 0x3ff7fc00
};

enum shader_surface_flags_e {
    /* Set by every time-dependent shader input proved so far: waveform
     * rgb/alpha generators and the turb, scroll, stretch, and rotate texture
     * modifiers. Static scale/transform modifiers do not set it. */
    SHADER_SURFACE_TIME_DEPENDENT = 0x1,
    SHADER_SURFACE_REQUIRES_VERTEX_BASIS = 0x2,
    SHADER_SURFACE_REQUIRES_TANGENT = 0x4,
    SHADER_SURFACE_REQUIRES_BITANGENT = 0x8,
    SHADER_SURFACE_TANGENT_SPACE_INPUT_MASK =
        SHADER_SURFACE_REQUIRES_TANGENT |
        SHADER_SURFACE_REQUIRES_BITANGENT,
    SHADER_SURFACE_BASE_TEXCOORDS = 0x10,
    SHADER_SURFACE_LIGHTMAP_TEXCOORDS = 0x20,
    SHADER_SURFACE_VERTEX_COLORS = 0x40
};

typedef struct renderer_water_complex_s {
    float real;      /* original +0x00 */
    float imaginary; /* original +0x04 */
} renderer_water_complex_t;

/* Water-map configuration parsed by ParseWaterMap and completed by
 * R_GetWaterTexture. The parser strings prove the public parameter names;
 * the simulation bodies prove the four renderer-owned fields at +0x00..+0x0f.
 */
typedef struct shader_water_map_s {
    image_t *image;                              /* original +0x00 */
    int32_t uploadFrame;                         /* original +0x04 */
    renderer_water_complex_t *initialFrequencies; /* original +0x08 */
    float *angularFrequencies;                   /* original +0x0c */
    int32_t textureWidth;                        /* original +0x10 */
    int32_t textureHeight;                       /* original +0x14 */
    float horizontalWorldLength;                 /* original +0x18 */
    float verticalWorldLength;                   /* original +0x1c */
    float gravity;                               /* original +0x20 */
    float windVelocity;                          /* original +0x24 */
    vec2_t windDirection;                        /* original +0x28 */
    float amplitude;                             /* original +0x30 */
} shader_water_map_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(shader_water_map_t, initialFrequencies) == 0x08,
               "i386 water initial-frequency pointer moved");
_Static_assert(offsetof(shader_water_map_t, textureWidth) == 0x10,
               "i386 water texture dimensions moved");
_Static_assert(offsetof(shader_water_map_t, windDirection) == 0x28,
               "i386 water wind direction moved");
_Static_assert(sizeof(shader_water_map_t) == 0x34,
               "i386 water-map record size changed");
#endif

enum {
    R_VERTEX_PROGRAM_NAME_SIZE = 64,
    R_MAX_VERTEX_PROGRAMS = 128
};

typedef struct renderer_vertex_program_s {
    char name[R_VERTEX_PROGRAM_NAME_SIZE]; /* original +0x00 */
    uint32_t glProgramName;                /* original +0x40 */
} renderer_vertex_program_t;

typedef struct renderer_combiner_input_s {
    uint32_t input;
    uint32_t mapping;
    uint32_t componentUsage;
} renderer_combiner_input_t;

typedef struct renderer_nv_register_definition_s {
    const char *token;                     /* original +0x00 */
    uint32_t glRegister;                   /* original +0x04 */
    uint32_t componentUsage;               /* original +0x08 */
} renderer_nv_register_definition_t;

enum {
    R_NV_REGISTER_COMBINERS1_GENERAL_LIMIT = 2,
    R_NV_COMBINER_PORTION_COUNT = 2,
    R_NV_COMBINER_INPUT_COUNT = 4,
    R_NV_GENERAL_COMBINER_LIMIT = 8,
    R_NV_FINAL_COMBINER_INPUT_COUNT = 7,
    R_NV_FINAL_ALPHA_INPUT_INDEX = 6
};

typedef struct renderer_combiner_output_s {
    uint32_t abOutput;
    uint32_t cdOutput;
    uint32_t sumOutput;
    uint32_t scale;
    uint32_t bias;
    uint8_t abDotProduct;
    uint8_t cdDotProduct;
    uint8_t muxSum; /* original +0x16; +0x17 is CoDUOMP-unused tail padding */
} renderer_combiner_output_t;

typedef struct renderer_combiner_portion_s {
    renderer_combiner_input_t inputs[R_NV_COMBINER_INPUT_COUNT];
    renderer_combiner_output_t output;
} renderer_combiner_portion_t;

typedef struct renderer_general_combiner_s {
    vec4_t constantColors[2];
    renderer_combiner_portion_t rgb;
    renderer_combiner_portion_t alpha;
} renderer_general_combiner_t;

typedef struct renderer_final_combiner_s {
    renderer_combiner_input_t inputs[R_NV_FINAL_COMBINER_INPUT_COUNT];
    uint8_t clampColorSum; /* original +0x54; +0x55..+0x57 are CoDUOMP-unused tail padding */
} renderer_final_combiner_t;

typedef struct renderer_register_combiners_s {
    vec4_t constantColors[2];                /* original +0x000 */
    renderer_general_combiner_t
        general[R_NV_GENERAL_COMBINER_LIMIT]; /* original +0x020 */
    renderer_final_combiner_t final;         /* original +0x5a0 */
    int32_t generalCombinerCount;            /* original +0x5f8 */
    qboolean perStageConstants;              /* original +0x5fc */
} renderer_register_combiners_t;

typedef struct renderer_atifs_constant_definition_s {
    vec4_t value;
    qboolean defined;
} renderer_atifs_constant_definition_t;

enum {
    R_ATIFS_TEMP_REGISTER_COUNT = 6,
    R_ATIFS_OPERATION_PAIR_COUNT = 8
};

typedef struct renderer_atifs_texture_read_s {
    qboolean sampleMap;
    uint32_t source;
    uint32_t swizzle;
} renderer_atifs_texture_read_t;

typedef struct renderer_atifs_argument_s {
    uint32_t source;
    uint32_t componentUsage;
    uint32_t modifier;
} renderer_atifs_argument_t;

typedef struct renderer_atifs_instruction_s {
    uint32_t operation;
    uint32_t destination;
    uint32_t destinationMask;
    uint32_t destinationModifier;
    renderer_atifs_argument_t arguments[3];
} renderer_atifs_instruction_t;

typedef struct renderer_atifs_operation_pair_s {
    renderer_atifs_instruction_t color;
    renderer_atifs_instruction_t alpha;
} renderer_atifs_operation_pair_t;

typedef struct renderer_atifs_phase_s {
    renderer_atifs_texture_read_t
        textureReads[R_ATIFS_TEMP_REGISTER_COUNT];
    renderer_atifs_operation_pair_t
        operationPairs[R_ATIFS_OPERATION_PAIR_COUNT];
} renderer_atifs_phase_t;

/* ParseATIFS_ConstDefs accepts const8 even though ATI_fragment_shader exposes
 * only eight uploadable constants. In the original 1,968-byte stack record,
 * that ninth parser slot aliases the first 20 bytes of phases[0] and is
 * overwritten as the phase is parsed. The union preserves that exact,
 * machine-proven overlap without an out-of-bounds C access. */
typedef union renderer_atifs_program_u {
    renderer_atifs_constant_definition_t constantDefinitions[9];
    struct {
        renderer_atifs_constant_definition_t uploadConstants[8];
        renderer_atifs_phase_t phases[2];
    } parsed;
} renderer_atifs_program_t;

typedef struct shader_texture_combine_args_s {
    uint32_t operation;             /* subrecord +0x00 */
    uint32_t sources[3];            /* subrecord +0x04 */
    uint32_t operands[3];           /* subrecord +0x10 */
    float scale;                    /* subrecord +0x1c */
} shader_texture_combine_args_t;

typedef struct shader_texture_combine_s {
    vec4_t environmentColor;             /* original +0x00 */
    shader_texture_combine_args_t rgb;   /* original +0x10 */
    shader_texture_combine_args_t alpha; /* original +0x30 */
} shader_texture_combine_t;

typedef enum shader_requirement_operand_type_e {
    SHADER_REQUIREMENT_OPERAND_NONE = 0,
    SHADER_REQUIREMENT_OPERAND_STRING = 1,
    SHADER_REQUIREMENT_OPERAND_NUMBER = 2
} shader_requirement_operand_type_t;

/* Dependency indices are also the indices of the original requirement-byte
 * bank at 0x03880468. Slots 0..5 represent required active-texture counts
 * three through eight; named extension dependencies begin at slot 6. */
typedef enum shader_requirement_dependency_e {
    SHADER_REQUIREMENT_MAX_TEXTURE_UNITS_3 = 0,
    SHADER_REQUIREMENT_MAX_TEXTURE_UNITS_4 = 1,
    SHADER_REQUIREMENT_MAX_TEXTURE_UNITS_5 = 2,
    SHADER_REQUIREMENT_MAX_TEXTURE_UNITS_6 = 3,
    SHADER_REQUIREMENT_MAX_TEXTURE_UNITS_7 = 4,
    SHADER_REQUIREMENT_MAX_TEXTURE_UNITS_8 = 5,
    SHADER_REQUIREMENT_MAX_TEXTURE_UNITS_ARB =
        SHADER_REQUIREMENT_MAX_TEXTURE_UNITS_3,
    SHADER_REQUIREMENT_TEXTURE_CUBE_MAP_ARB = 6,
    SHADER_REQUIREMENT_TEXTURE_ENV_ADD_ARB = 7,
    SHADER_REQUIREMENT_TEXTURE_ENV_COMBINE_ARB = 8,
    SHADER_REQUIREMENT_TEXTURE_ENV_DOT3_ARB = 9,
    SHADER_REQUIREMENT_VERTEX_PROGRAM_ARB = 10,
    SHADER_REQUIREMENT_REGISTER_COMBINERS_NV = 11,
    SHADER_REQUIREMENT_REGISTER_COMBINERS2_NV = 12,
    SHADER_REQUIREMENT_TEXTURE_SHADER_NV = 13,
    SHADER_REQUIREMENT_FRAGMENT_SHADER_ATI = 14,
    SHADER_REQUIREMENT_COUNT = 15
} shader_requirement_dependency_t;

typedef enum shader_requirement_operator_e {
    SHADER_REQUIREMENT_EQUAL = 0,
    SHADER_REQUIREMENT_NOT_EQUAL = 1,
    SHADER_REQUIREMENT_LESS = 2,
    SHADER_REQUIREMENT_LESS_OR_EQUAL = 3,
    SHADER_REQUIREMENT_GREATER = 4,
    SHADER_REQUIREMENT_GREATER_OR_EQUAL = 5
} shader_requirement_operator_t;

/* ParseStageRequirementsOperand writes this exact 256-byte temporary. The
 * dependency is -1 for an ordinary literal/cvar value; leadingNotCount records
 * every consumed '!' so dependency tracking can distinguish positive from
 * negated feature requirements. */
typedef struct shader_requirement_operand_s {
    shader_requirement_operand_type_t type; /* original +0x00 */
    int16_t dependency;                     /* original +0x04 */
    uint16_t leadingNotCount;               /* original +0x06 */
    char value[248];                        /* original +0x08 */
} shader_requirement_operand_t;

typedef struct shader_texture_shader_s {
    uint32_t operation;             /* original +0x00 */
    uint32_t previousTextureInput;  /* original +0x04 */
    uint32_t dotProductMapping;     /* original +0x08 */
    union {
        vec4_t floats;
        uint32_t cullModes[4];
    } parameters;                   /* original +0x0c */
} shader_texture_shader_t;

/* Original-layout guards for this record and its anonymous union are kept
 * with the other original-i386 assertions after all dependent definitions;
 * no native-width layout contract is implied by this binary recovery. */


typedef enum nv_texture_shader_argument_type_e {
    NV_TEXTURE_SHADER_ARG_TEXTURE_INPUT = 0,
    NV_TEXTURE_SHADER_ARG_EXPANDABLE_TEXTURE_INPUT = 1,
    NV_TEXTURE_SHADER_ARG_CULL_MODE_0 = 2,
    NV_TEXTURE_SHADER_ARG_CULL_MODE_1 = 3,
    NV_TEXTURE_SHADER_ARG_CULL_MODE_2 = 4,
    NV_TEXTURE_SHADER_ARG_CULL_MODE_3 = 5,
    NV_TEXTURE_SHADER_ARG_FLOAT_0 = 6,
    NV_TEXTURE_SHADER_ARG_FLOAT_1 = 7,
    NV_TEXTURE_SHADER_ARG_FLOAT_2 = 8,
    NV_TEXTURE_SHADER_ARG_FLOAT_3 = 9
} nv_texture_shader_argument_type_t;

typedef enum shader_texcoord_gen_e {
    TCGEN_BAD = 0,
    TCGEN_IDENTITY = 1,
    TCGEN_LIGHTMAP = 2,
    TCGEN_TEXTURE = 3,
    TCGEN_ENVIRONMENT_MAPPED = 4,
    TCGEN_FOG = 5,
    TCGEN_VECTOR = 6,
    TCGEN_NORMAL = 7,
    TCGEN_TANGENT = 8,
    TCGEN_BITANGENT = 9,
    TCGEN_TBN_S = 10,
    TCGEN_TBN_T = 11,
    TCGEN_TBN_R = 12,
    TCGEN_CUBEMAP_VERTEX_TO_EYE = 13,
    TCGEN_CUBEMAP_EYE_TO_VERTEX = 14,
    TCGEN_CUBEMAP_REFLECTION = 15,
    TCGEN_CUBEMAP_LIGHT_VECTOR = 16,
    TCGEN_CUBEMAP_LIGHT_HALF_ANGLE = 17,
    TCGEN_CUBEMAP_SUN_HALF_ANGLE = 18,
    TCGEN_CUBEMAP_DOT3_REFLECT_S = 19,
    TCGEN_CUBEMAP_DOT3_REFLECT_T = 20,
    TCGEN_CUBEMAP_DOT3_REFLECT_R = 21
} shader_texcoord_gen_t;

typedef enum shader_texmod_type_e {
    TMOD_NONE = 0,
    TMOD_TRANSFORM = 1,
    TMOD_TURBULENT = 2,
    TMOD_SCROLL = 3,
    TMOD_SCALE = 4,
    TMOD_STRETCH = 5,
    TMOD_ROTATE = 6,
    TMOD_ENTITY_TRANSLATE = 7,
    TMOD_SWAP = 8,
    TMOD_CUBEMAP_NEGATE = 9,
    TMOD_CUBEMAP_BUMPMAP_FRAME = 10
} shader_texmod_type_t;

typedef enum shader_wave_func_e {
    SHADER_WAVE_SIN = 1,
    SHADER_WAVE_SQUARE = 2,
    SHADER_WAVE_TRIANGLE = 3,
    SHADER_WAVE_SAWTOOTH = 4,
    SHADER_WAVE_INVERSE_SAWTOOTH = 5,
    SHADER_WAVE_NOISE = 6
} shader_wave_func_t;

/* These five aggregate identities and their surviving member spellings are
 * inherited from Quake III. CoD expands the waveform consumers, texture
 * modifier modes, bundle bank, and stage GPU-program state, but Windows and
 * Mac producer/consumer pairs prove that the original ownership and common
 * lanes remain intact. */
typedef struct waveForm_s {
    shader_wave_func_t func;
    float base;
    float amplitude;
    float phase;
    float frequency;
} waveForm_t;

typedef struct texModInfo_s {
    shader_texmod_type_t type;   /* original +0x00 */
    waveForm_t wave;             /* original +0x04 */
    float matrix[2][2];             /* original +0x18, column-major */
    vec2_t translate;               /* original +0x28 */
    vec2_t scale;                   /* original +0x30 */
    vec2_t scroll;                  /* original +0x38 */
    float rotateSpeed;              /* original +0x40 */
} texModInfo_t;

typedef enum shader_deform_type_e {
    DEFORM_WAVE = 1,
    DEFORM_FLAP_S = 2,
    DEFORM_FLAP_T = 3,
    DEFORM_NORMALS = 4,
    DEFORM_SYNC_NORMALS = 5,
    DEFORM_BULGE = 6,
    DEFORM_MOVE = 7,
    DEFORM_PROJECTION_SHADOW = 8,
    DEFORM_AUTOSPRITE = 9,
    DEFORM_AUTOSPRITE2 = 10,
    DEFORM_TEXT0 = 11,
    DEFORM_TEXT1 = 12,
    DEFORM_TEXT2 = 13,
    DEFORM_TEXT3 = 14,
    DEFORM_TEXT4 = 15,
    DEFORM_TEXT5 = 16,
    DEFORM_TEXT6 = 17,
    DEFORM_TEXT7 = 18
} shader_deform_type_t;

enum {
    R_MAX_SHADER_DEFORMS = 3
};

typedef struct deformStage_s {
    shader_deform_type_t deformation; /* original +0x00 */
    vec3_t moveVector;                 /* original +0x04 */
    waveForm_t deformationWave;        /* original +0x10 */
    float deformationSpread;        /* original +0x24 */
    float bulgeWidth;               /* original +0x28 */
    float bulgeHeight;              /* original +0x2c */
    float bulgeSpeed;               /* original +0x30 */
} deformStage_t;

typedef struct textureBundle_s {
    image_t *image[32];                     /* original +0x00 */
    int32_t numImageAnimations;             /* original +0x80 */
    float imageAnimationSpeed;              /* original +0x84 */
    struct shader_water_map_s *waterMap;    /* original +0x88 */
    int32_t texCoordComponentCount;         /* original +0x8c */
    uint32_t textureEnvMode;                /* original +0x90 */
    shader_texture_combine_t *textureCombine; /* original +0x94 */
    shader_texture_shader_t *textureShader;   /* original +0x98 */
    shader_texcoord_gen_t tcGen;            /* original +0x9c */
    vec3_t tcGenVectors[2];                 /* original +0xa0 */
    int32_t numTexMods;                     /* original +0xb8 */
    texModInfo_t *texMods;                  /* original +0xbc */
    int32_t videoMapHandle;                 /* original +0xc0 */
    uint8_t isLightmap;                     /* original +0xc4 */
    uint8_t isVideoMap;                     /* original +0xc5 */
    uint8_t clampAnimation;                 /* original +0xc6 */
    uint8_t padding0c7; /* original +0xc7; unused by CoDUOMP.exe */
} textureBundle_t;

typedef enum shader_color_gen_e {
    CGEN_BAD = 0,
    CGEN_IDENTITY_LIGHTING = 1,
    CGEN_IDENTITY = 2,
    CGEN_ENTITY = 3,
    CGEN_ONE_MINUS_ENTITY = 4,
    CGEN_EXACT_VERTEX = 5,
    CGEN_VERTEX = 6,
    CGEN_ONE_MINUS_VERTEX = 7,
    CGEN_WAVEFORM = 8,
    CGEN_LIGHTING_AMBIENT = 9,
    CGEN_LIGHTING_DIFFUSE = 10,
    CGEN_LIGHTING_PRECALC = 11,
    CGEN_CONSTANT = 12,
    /* Internal renderer mode used to color surfaces by their index count
     * when the high-detail r_showtris diagnostics are active. */
    CGEN_DEBUG_SURFACE_COUNT = 13
} shader_color_gen_t;

typedef enum shader_alpha_gen_e {
    AGEN_UNSPECIFIED = 0,
    AGEN_IDENTITY = 1,
    AGEN_ENTITY = 2,
    AGEN_ONE_MINUS_ENTITY = 3,
    AGEN_VERTEX = 4,
    AGEN_ONE_MINUS_VERTEX = 5,
    AGEN_LIGHTING_SPECULAR = 6,
    AGEN_WAVEFORM = 7,
    AGEN_PORTAL = 8,
    AGEN_CONSTANT = 9,
    AGEN_DOT = 10,
    AGEN_ONE_MINUS_DOT = 11,
    AGEN_ONE_PLUS_DOT = 12,
    AGEN_NEGATIVE_DOT = 13
} shader_alpha_gen_t;

struct shaderStage_s {
    uint32_t flags;                              /* original +0x000 */
    textureBundle_t bundle[R_MAX_TEXTURE_UNITS]; /* +0x004 */
    uint32_t fragmentShaderATI;             /* original +0x644 */
    struct renderer_register_combiners_s *registerCombiners; /* +0x648 */
    renderer_vertex_program_t *vertexProgram;                 /* +0x64c */
    waveForm_t rgbWave;                    /* original +0x650 */
    shader_color_gen_t rgbGen;             /* original +0x664 */
    waveForm_t alphaWave;                  /* original +0x668 */
    shader_alpha_gen_t alphaGen;           /* original +0x67c */
    uint8_t constantColor[4];               /* original +0x680 */
    uint32_t stateBits;                     /* original +0x684 */
};

enum renderer_shader_stage_flag_e {
    /* ParseStage sets this on entry; a false `requires` result prevents the
     * caller from copying the otherwise parsed stage into the live shader. */
    SHADER_STAGE_ACTIVE = 0x01,
    SHADER_STAGE_DETAIL = 0x02,
    SHADER_STAGE_FOG = 0x04,
    /* Normal stage iteration skips this bit; the entity-light tail replays
     * exactly these stages once for each applicable light. */
    SHADER_STAGE_PER_LIGHT = 0x80,
    /* RB_SingleStageGenericATI/NV upload a generated texture-coordinate array
     * for unit n when bit (SHADER_STAGE_TEXCOORD_ARRAY0 << n) is present. */
    SHADER_STAGE_TEXCOORD_ARRAY0 = 0x100,
    SHADER_STAGE_COLOR_ARRAY = 0x10000,
    SHADER_STAGE_NORMAL_ARRAY = 0x20000,
    /* ComputeHardwareNeeds has derived all stage- and shader-wide input
     * requirements for this active stage. */
    SHADER_STAGE_HARDWARE_NEEDS_COMPUTED = 0x40000,
    SHADER_STAGE_DYNAMIC_ARRAY_MASK = 0x3ff00
};

enum renderer_shader_lighting_flag_e {
    /* Enables the renderer-entity light list at +0xcc after ordinary and
     * projected-dynamic-light stage processing. */
    SHADER_LIGHTING_PER_ENTITY = 0x80
};

enum renderer_shader_dlight_flag_e {
    /* Either bit suppresses ProjectDlightTexture* and leaves only the
     * per-entity light-stage tail. */
    SHADER_DLIGHT_PROJECTION_BLOCK_MASK = 0x20004
};

/* Values consumed by RB_EndSurface_Optimized. The intervening 16..23 values
 * deliberately have no end-surface dispatch in the Windows switch. */
typedef enum shader_optimized_backend_e {
    SHADER_BACKEND_CACHED_STATIC_MODEL_GENERIC = 12,
    SHADER_BACKEND_CACHED_STATIC_MODEL_ARB = 13,
    SHADER_BACKEND_CACHED_STATIC_MODEL_ATI = 14,
    SHADER_BACKEND_CACHED_STATIC_MODEL_NV = 15,
    SHADER_BACKEND_UNOPTIMIZED = 24,
    SHADER_BACKEND_OPTIMIZED_GENERIC = 25,
    SHADER_BACKEND_OPTIMIZED_ARB = 26,
    SHADER_BACKEND_OPTIMIZED_ATI = 27,
    SHADER_BACKEND_OPTIMIZED_NV = 28
} shader_optimized_backend_t;

enum {
    R_SKYBOX_FACE_COUNT = 6
};

enum renderer_shader_flag_e {
    SHADER_FLAG_DEFAULTED = 0x0001,
    SHADER_FLAG_EXPLICITLY_DEFINED = 0x0002,
    SHADER_FLAG_ENTITY_MERGABLE = 0x0004,
    SHADER_FLAG_SKY = 0x0008,
    SHADER_FLAG_POLYGON_OFFSET = 0x0010,
    SHADER_FLAG_POLYGON_OFFSET_DOUBLE = 0x0020,
    SHADER_FLAG_POLYGON_OFFSET_CONSTANT = 0x0040,
    SHADER_FLAG_POLYGON_OFFSET_MASK =
        SHADER_FLAG_POLYGON_OFFSET |
        SHADER_FLAG_POLYGON_OFFSET_DOUBLE |
        SHADER_FLAG_POLYGON_OFFSET_CONSTANT,
    /* ParseImage derives the initial image-upload policy from these four
     * shader properties before applying per-image options. */
    SHADER_FLAG_NO_MIPMAPS = 0x0080,
    SHADER_FLAG_NO_PICMIP = 0x0100,
    SHADER_FLAG_USE_PICMIP2 = 0x0200,
    SHADER_FLAG_NO_FOG = 0x0400,
    /* Marks shaders whose image references still require delayed-load
     * processing before their permanent renderer form is ready. */
    SHADER_FLAG_DELAYED_IMAGES = 0x0800,
    SHADER_FLAG_REMAPPED = 0x1000,
    SHADER_FLAG_NO_IMAGE_OVERBRIGHT = 0x2000
};

enum renderer_shader_surface_flag_e {
    /* The generic fixed-function stage iterator installs tess.stageNormals as
     * a GL normal array when this shader property is present. */
    SHADER_SURFACE_REQUIRES_NORMAL_ARRAY = 0x00000100,
    /* ComputeHardwareNeeds assigns one bit per live texture bundle. */
    SHADER_SURFACE_TEXTURE_UNIT0 = 0x00000200,
    /* A color generator consumes authored or computed color input. */
    SHADER_SURFACE_COLOR_INPUT = 0x00020000,
    SHADER_SURFACE_DEFORMED_NORMALS = 0x00040000,
    SHADER_SURFACE_ANIMATED_TEXTURES = 0x00080000,
    /* One bit per bundle whose texture coordinates cannot use an authored
     * identity/lightmap/base coordinate stream unchanged. */
    SHADER_SURFACE_GENERATED_TEXCOORD0 = 0x00100000,
    SHADER_SURFACE_DYNAMIC_COLORS = 0x10000000,
    /* Every deformation except DEFORM_NORMALS changes vertex positions.
     * R_NeedsBoundsAdjustment consumes this property for model bounds. */
    SHADER_SURFACE_DEFORMED_POSITIONS = 0x20000000
};

/* Named shader-sort values accepted by ParseSort. shader_t.sort remains a
 * float because shader text may provide an arbitrary numeric sort value. */
typedef enum shader_sort_e {
    SHADER_SORT_BAD = 0,
    SHADER_SORT_PORTAL = 1,
    SHADER_SORT_SKY = 2,
    SHADER_SORT_OCEAN = 3,
    SHADER_SORT_BOAT_HULL = 4,
    SHADER_SORT_OPAQUE = 5,
    SHADER_SORT_DECAL = 6,
    SHADER_SORT_SEE_THROUGH = 7,
    SHADER_SORT_BANNER = 8,
    SHADER_SORT_FOG = 9,
    SHADER_SORT_UNDERWATER = 10,
    SHADER_SORT_WATER = 11,
    SHADER_SORT_CORONA = 12,
    SHADER_SORT_INNER_BLEND = 13,
    SHADER_SORT_OUTER_BLEND = 14,
    SHADER_SORT_BLEND = 15,
    SHADER_SORT_BLEND_2 = 16,
    SHADER_SORT_BLEND_3 = 17,
    SHADER_SORT_BLEND_4 = 18,
    SHADER_SORT_ADDITIVE = 19,
    SHADER_SORT_STENCIL_SHADOW = 20,
    SHADER_SORT_NEAREST = 22
} shader_sort_t;

/* Negative lightmap selectors accepted by the renderer shader registry.
 * Nonnegative values index tr.lightmaps[]. */
enum {
    LIGHTMAP_NONE = -1,
    LIGHTMAP_WHITEIMAGE = -2,
    LIGHTMAP_BY_VERTEX = -3,
    LIGHTMAP_2D = -4
};

struct shader_s {
    char name[MAX_QPATH];                    /* original +0x000 */
    int32_t lightmapIndex;                    /* original +0x040 */
    int32_t index;                           /* original +0x044 */
    int32_t sortedIndex;                     /* original +0x048 */
    uint32_t flags;                         /* original +0x04c */
    uint32_t surfaceFlags;                  /* original +0x050 */
    uint32_t lightingFlags;                 /* original +0x054 */
    float sort;                             /* original +0x058 */
    uint32_t surfaceParmFlags;              /* original +0x05c */
    float skyCloudHeight;                   /* original +0x060 */
    image_t *skyOuterBox[R_SKYBOX_FACE_COUNT]; /* original +0x064 */
    image_t *skyInnerBox[R_SKYBOX_FACE_COUNT]; /* original +0x07c */
    vec3_t fogColor;                        /* original +0x094 */
    float fogDepthForOpaque;                /* original +0x0a0 */
    float portalRange;                       /* original +0x0a4 */
    cullType_t cullType;                     /* original +0x0a8 */
    int32_t numDeforms;                     /* original +0x0ac */
    deformStage_t deforms[R_MAX_SHADER_DEFORMS]; /* original +0x0b0 */
    float boundsExpansion;                   /* original +0x14c */
    int32_t numUnfoggedPasses;               /* original +0x150 */
    shaderStage_t *stages[R_MAX_SHADER_STAGES]; /* original +0x154 */
    renderer_stage_iterator_fn_t optimalStageIteratorFunc; /* original +0x174 */
    float clampTime;                        /* original +0x178 */
    float timeOffset;                       /* original +0x17c */
    struct shader_s *remappedShader;         /* original +0x180 */
    image_t *primaryImage;                   /* original +0x184 */
    shader_optimized_backend_t optimizedBackend; /* original +0x188 */
    /* BeginShaderSurfaces selects the active member by optimizedBackend. */
    renderer_static_vertex_memory_base_t optimizedVertexStorage; /* +0x18c */
    size_t optimizedVertexStorageOffset;                          /* +0x190 */
    struct shader_s *next;                                        /* +0x194 */
};

/* RGBA is a byte sequence, not a canonical-endian 32-bit integer. Both the
 * Windows and PowerPC Mac clients write components at +0..+3 and use native
 * word loads only as optimized four-byte copies. */
typedef struct renderer_rgba8_s {
    uint8_t components[4];
} renderer_rgba8_t;

/* CoD's continuation of Quake III backEndCounters_t. Windows
 * R_PerformanceCounters clears 13 dwords at backEnd + 0x464; the PowerPC Mac
 * function clears the same member with memset(..., 52). */
typedef struct backEndCounters_s {
    int32_t surfaceCount;                   /* original +0x00 */
    int32_t shaderCount;                    /* original +0x04 */
    int32_t vertexCount;                    /* original +0x08 */
    int32_t indexCount;                     /* original +0x0c */
    int32_t drawnIndexCount;                /* original +0x10 */
    int32_t drawCallCount;                  /* original +0x14 */
    float overdrawSum;                      /* original +0x18 */
    int32_t dlightVertexCount;              /* original +0x1c */
    int32_t dlightIndexCount;               /* original +0x20 */
    int32_t flareAddCount;                  /* original +0x24 */
    int32_t flareTestCount;                 /* original +0x28 */
    int32_t flareRenderCount;               /* original +0x2c */
    int32_t commandMsec;                    /* original +0x30 */
} backEndCounters_t;

/* CoD's continuation of Quake III shaderCommands_t. CoDUOMP.exe and the
 * PowerPC Mac client use the same naturally aligned
 * 32-bit layout for this renderer-private global. It is neither serialized
 * nor passed across an ABI boundary, so native 64-bit builds intentionally
 * use their compiler's wider pointer layout; the shipped-layout assertions
 * below apply only when pointers are 32 bits. */
typedef struct shaderCommands_s {
    uint16_t indexes[R_MAX_TESS_INDEXES];
    /* XYZ storage is packed with vertexComponentCount (three components for
     * the recovered 2D path) inside a four-components-per-vertex capacity. */
    float xyz[R_MAX_TESS_VERTICES * R_MAX_TESS_XYZ_COMPONENTS];
    vec2_t texCoords[R_TESS_TEXCOORD_SET_COUNT][R_MAX_TESS_VERTICES];
    uint32_t vertexColors[R_MAX_TESS_VERTICES];
    /* Back-end stage arrays established by R_ShutdownAllocators and the
     * storage-selection paths. RB_MakeNormalVectors fills the tangent and
     * bitangent arrays from stageNormals before tangent-space projection. */
    vec3_t stageNormals[R_MAX_TESS_VERTICES];       /* original +0x400000 */
    vec3_t stageBitangents[R_MAX_TESS_VERTICES];    /* original +0x4c0000 */
    vec3_t stageTangents[R_MAX_TESS_VERTICES];      /* original +0x580000 */
    uint32_t stageVertexColors[R_MAX_TESS_VERTICES]; /* original +0x640000 */
    const void *activeTexCoords[R_MAX_TEXTURE_UNITS]; /* original +0x680000 */
    vec4_t generatedTexCoords[R_MAX_TEXTURE_UNITS][R_MAX_TESS_VERTICES];
    /* R_Init fills all 65,536 packed colors with RGBA 255. This is the
     * renderer-lineage constantColor255 array. The Windows client has no data
     * consumer after initialization; PowerPC Mac confirms the same fill. */
    uint8_t constantColor255[R_MAX_TESS_VERTICES][4]; /* original +0xe80020 */
    int32_t vertexComponentCount;            /* state tail +0x20 */
    qboolean requiresVertexBasis;           /* state tail +0x24 */
    qboolean stageTangentsValid;            /* state tail +0x28 */
    qboolean stageBitangentsValid;          /* state tail +0x2c */
    shader_t *shader;                       /* state tail +0x30 */
    /* This is a real four-byte source-layout member, not compiler padding.
     * No Windows instruction addresses +0x34, and PowerPC Mac RB_BeginSurface
     * skips the same slot. Renderer lineage places a fog-number member near
     * shader/shaderTime, but CoDUOMP's ordering does not prove that identity. */
    uint8_t unrecoveredState034[4];
    float shaderTime;                       /* state tail +0x38 */
    trRefEntity_t *entity;                  /* state tail +0x3c */
    uint32_t dlightBits;                    /* state tail +0x40 */
    int32_t indexCount;                     /* state tail +0x44 */
    int32_t vertexCount;                    /* state tail +0x48 */
    int32_t optimizedFirstVertex;           /* state tail +0x4c */
    int32_t optimizedVertexEnd;             /* state tail +0x50 */
    uint16_t
        optimizedIndexes[R_MAX_OPTIMIZED_TESS_INDEXES]; /* tail +0x54 */
    int32_t renderedIndexCount;             /* original +0xf00054 */
    int32_t renderedVertexCount;            /* original +0xf00058 */
    int32_t activeStageCount;                /* original +0xf0005c */
    renderer_stage_iterator_fn_t stageIterator; /* original +0xf00060 */
    shaderStage_t **activeStages;          /* original +0xf00064 */
} shaderCommands_t;

/* Quake III drawSurf_t: the same packed sort word and polymorphic renderer
 * surface pointer remain the complete CoD record. */
typedef struct drawSurf_s {
    uint32_t sort;
    renderer_surface_t *surface;
} drawSurf_t;

/* Every rb_surfaceTable target receives the draw-surface pointer as its one
 * machine argument.  Keep that shared signature at the indirect-call boundary;
 * each target recovers its concrete surface type after entry. */
typedef void (*renderer_surface_fn_t)(renderer_surface_t *surface);

/* Fixed fog save-state record written by RE_SaveFogState. This is a byte
 * format, not the in-memory placement of rendererCurrentFogIndex: the Windows
 * function copies all nine fog records and appends the selected index. */
typedef struct renderer_fog_saved_state_s {
    renderer_fog_t fogs[R_FOG_SLOT_COUNT];
    int32_t currentFogIndex;
} renderer_fog_saved_state_t;

enum {
    R_DYNAMIC_BUFFER_ALLOCATION_COUNT = 512
};

typedef struct renderer_dynamic_buffer_allocation_s {
    int32_t offset;
    int32_t size;
} renderer_dynamic_buffer_allocation_t;

/* Original +0x00 is backend-dependent: a native CPU address for NV storage,
 * an ATI object-buffer name, or an ARB buffer-object name. */
typedef union renderer_dynamic_buffer_storage_u {
    uint8_t *address;
    uint32_t glBuffer;
    uint32_t atiObjectBuffer;
} renderer_dynamic_buffer_storage_t;

/* Ring allocator used by the renderer's dynamic vertex buffer. Allocation
 * records at original 0x0489ab54 are indexed modulo 512 by the allocator at
 * 0x0051d6f0. CoDUOMP.exe and the PowerPC Mac client use the same naturally
 * aligned 32-bit layout. This is renderer-private state, so native 64-bit
 * builds intentionally widen the storage union's CPU pointer and shift the
 * remaining fields; the shipped-layout assertions below are 32-bit-only. */
typedef struct renderer_dynamic_buffer_s {
    renderer_dynamic_buffer_storage_t storage; /* original +0x00 */
    int32_t capacity;                       /* original +0x04 */
    int32_t currentOffset;                  /* original +0x08 */
    int32_t frameSerial;                    /* original +0x0c; current ARB
                                              * buffer-object name serial */
    uint8_t unused010[4];                   /* original +0x10: real four-byte
                                              * source-layout slot, not required
                                              * alignment padding; only whole-
                                              * backend zeroing covers it, while
                                              * all field-specific CoDUOMP and
                                              * PowerPC Mac accesses skip it */
    uint32_t allocationSequence;            /* original +0x14; last issued NV
                                              * fence name */
    uint32_t reclaimSequence;               /* original +0x18; next NV fence
                                              * name to reclaim */
    int32_t freeBytes;                      /* original +0x1c */
    renderer_dynamic_buffer_allocation_t
        allocations[R_DYNAMIC_BUFFER_ALLOCATION_COUNT]; /* original +0x20 */
} renderer_dynamic_buffer_t;

/* CoD's continuation of Quake III backEndState_t, beginning at original
 * address 0x0489a100. Windows
 * R_Init clears 0x695 dwords here, proving the complete 0x1a54-byte extent.
 * The refdef and viewParms blocks are copied wholesale by RB_DrawSurfs.
 * Windows and PowerPC Mac use this same naturally aligned 32-bit layout;
 * native 64-bit builds intentionally widen its renderer-private pointers. */
typedef struct backEndState_s {
    trRefdef_t refdef;                      /* original +0x000 */
    viewParms_t viewParms;                  /* original +0x188 */
    orientationr_t orientation;             /* original +0x3e8 */
    backEndCounters_t pc;                   /* original +0x464 */
    qboolean isHyperspace;                  /* original +0x498 */
    trRefEntity_t *currentEntity;           /* original +0x49c */
    renderer_light_t *currentLight;         /* original +0x4a0 */
    float currentLightScale;                /* original +0x4a4 */
    qboolean skyRenderedThisView;           /* original +0x4a8 */
    qboolean endFramePending;               /* original +0x4ac */
    qboolean projection2D;                  /* original +0x4b0 */
    renderer_rgba8_t color2D;               /* original +0x4b4 */
    renderer_rgba8_t colorCode8;            /* original +0x4b8 */
    renderer_rgba8_t colorCode9;            /* original +0x4bc */
    /* Quake III/RTCW lineage names this source-layout member vertexes2D.
     * CoDUOMP retains the slot, but only whole-backend zeroing covers it;
     * every field-specific Windows and PowerPC Mac access skips +0x4c0. */
    qboolean vertexes2D;                    /* original +0x4c0; unused */
    trRefEntity_t entity2D;                 /* original +0x4c4 */
    /* RB_DrawDebugStrings writes &backEnd.debugEntity to currentEntity. The
     * original address 0x0489a87c is exactly one trRefEntity_t after
     * entity2D, proving that this range is a second embedded entity rather
     * than part of the unrecovered backend tail. */
    trRefEntity_t debugEntity;              /* original +0x77c */
    renderer_dynamic_buffer_t dynamicBuffer; /* original +0xa34 */
} backEndState_t;

/* Original scene-frame counters at 0x04884dac..0x04884dd3. The scene
 * producers, RE_ClearScene, and RE_RenderScene prove every counter and
 * first-item boundary. */
typedef struct renderer_scene_frame_state_s {
    int32_t entityCount;                    /* original +0x00 */
    int32_t coronaCount;                    /* original +0x04 */
    int32_t dlightCount;                    /* original +0x08 */
    int32_t firstDlight;                    /* original +0x0c */
    int32_t polyVertexCount;                /* original +0x10 */
    int32_t firstPoly;                      /* original +0x14 */
    int32_t drawSurfCount;                  /* original +0x18 */
    int32_t firstCorona;                    /* original +0x1c */
    int32_t firstEntity;                    /* original +0x20 */
    int32_t polyCount;                      /* original +0x24 */
} renderer_scene_frame_state_t;

/* CoD's continuation of Quake III srfPoly_t. Type-2 surfaces are immediate
 * scene polygons, not a separate grid format.
 * The Windows and PowerPC Mac RE_AddPolyToScene producers and
 * RB_SurfacePolychain consumers prove all four fields and the 32-byte public
 * polyVert_t stream. CoD removes Q3's fogIndex lane, leaving the complete
 * original record at 16 bytes. The pointer remains naturally aligned on native
 * 64-bit builds; the original 16-byte record layout is an i386 ABI property. */
typedef struct srfPoly_s {
    renderer_surface_type_t surfaceType;    /* original +0x00 */
    int32_t hShader;                        /* original +0x04 */
    int32_t numVerts;                       /* original +0x08 */
    polyVert_t *verts;                      /* original +0x0c */
} srfPoly_t;

/* CoD's continuation of Quake III backEndData_t: the fixed prefix of the hunk
 * allocation created by R_Init. R_Init appends
 * configurable polygon-sized bytes, but CoDUOMP.exe uses the fixed arrays
 * below. No original instruction addresses the tail, so it is unused
 * allocation slack rather than backing storage. */
typedef struct backEndData_s {
    drawSurf_t drawSurfs[R_MAX_DRAW_SURFS];              /* +0x000000 */
    renderer_light_t dlights[R_MAX_DLIGHTS];               /* +0x080000 */
    renderer_corona_t coronas[R_MAX_CORONAS];              /* +0x081100 */
    trRefEntity_t sceneEntities[R_MAX_SCENE_ENTITIES];     /* +0x81700 */
    renderer_entity_surface_t
        entitySurfaces[R_MAX_ENTITY_SURFACES];             /* +0x12f700 */
    srfPoly_t polys[R_BASE_SCENE_POLYS];     /* +0x13f700 */
    polyVert_t
        polyVertices[R_BASE_SCENE_POLY_VERTICES];          /* +0x14f700 */
    uint8_t commandBuffer[262144];          /* original +0x1cf700 */
    int32_t commandUsed;                    /* original +0x20f700 */
    uint8_t unusedAllocationTail[];         /* original +0x20f704; unused by
                                              * CoDUOMP.exe */
} backEndData_t;

/* The original command buffer advances in four-byte units and includes
 * four-byte records, so a following command may begin at an address that is
 * only four-byte aligned. Preserve that proved stream contract on i386. On
 * 64-bit hosts the embedded pointers widen but the stream does not gain
 * padding records; capping these command aggregates at four-byte packing makes
 * their producer/consumer layout agree and lets MSVC, GCC, and Clang generate
 * valid unaligned accesses for the widened pointer members. */
#pragma pack(push, 4)

/* CoD continuation of Quake III's drawSurfsCommand_t. */
typedef struct drawSurfsCommand_s {
    int32_t commandId;
    trRefdef_t refdef;
    viewParms_t viewParms;
    drawSurf_t *drawSurfs;
    int32_t numDrawSurfs;
} drawSurfsCommand_t;

typedef struct drawBufferCommand_s {
    int32_t commandId;
    uint32_t buffer;
} drawBufferCommand_t;

typedef struct save_screen_command_s {
    int32_t commandId;
} save_screen_command_t;

typedef struct blend_saved_screen_command_s {
    int32_t commandId;
    int32_t duration;
} blend_saved_screen_command_t;

typedef struct swapBuffersCommand_s {
    int32_t commandId;
} swapBuffersCommand_t;

/* CoD keeps the inherited command identity but packs Q3's float[4] color as
 * the four-byte RGBA value proved by both shipped clients. */
typedef struct setColorCommand_s {
    int32_t commandId;
    renderer_rgba8_t color;
} setColorCommand_t;

typedef struct stretchPicCommand_s {
    int32_t commandId;
    shader_t *shader;
    float x;
    float y;
    float w;
    float h;
    float s1;
    float t1;
    float s2;
    float t2;
} stretchPicCommand_t;

typedef struct stretch_pic_gradient_command_s {
    int32_t commandId;
    shader_t *shader;
    float x;
    float y;
    float width;
    float height;
    float s1;
    float t1;
    float s2;
    float t2;
    renderer_rgba8_t gradientColor;
    int32_t gradientType; /* carried by the command; backend does not inspect it */
} stretch_pic_gradient_command_t;

typedef struct stretch_pic_rotate_command_s {
    int32_t commandId;
    shader_t *shader;
    float x;
    float y;
    float width;
    float height;
    float s1;
    float t1;
    float s2;
    float t2;
    float angleDegrees;
} stretch_pic_rotate_command_t;

typedef struct draw_quad_pic_command_s {
    int32_t commandId;
    shader_t *shader;
    vec2_t positions[4];
    vec2_t texCoords[4];
} draw_quad_pic_command_t;

typedef enum renderer_command_id_e {
    RC_END_OF_LIST = 0,
    RC_SET_COLOR = 1,
    RC_STRETCH_PIC = 2,
    RC_STRETCH_PIC_GRADIENT = 3,
    RC_STRETCH_PIC_ROTATE = 4,
    RC_DRAW_QUAD_PIC = 5,
    RC_TEXT_PAINT_WITH_CURSOR = 6,
    RC_DRAW_SURFS = 7,
    RC_DRAW_BUFFER = 8,
    RC_SAVE_SCREEN = 9,
    RC_BLEND_SAVED_SCREEN = 10,
    RC_SWAP_BUFFERS = 11,
} renderer_command_id_t;

/* Variable-length command. The text begins immediately after the one-byte
 * cursor glyph in the original command stream; each complete command is then
 * rounded up to a four-byte boundary. */
typedef struct text_paint_command_s {
    int32_t commandId;                      /* original +0x00 */
    float x;                                /* original +0x04 */
    float y;                                /* original +0x08 */
    int32_t fontHandle;                     /* original +0x0c */
    float scale;                            /* original +0x10 */
    renderer_rgba8_t color;                 /* original +0x14 */
    float fixedAdvance;                     /* original +0x18 */
    int32_t textStyle;                      /* original +0x1c */
    int32_t cursorPosition;                 /* original +0x20 */
    uint8_t cursorCharacter;                /* original +0x24 */
    char text[];                            /* original +0x25 */
} text_paint_command_t;


#pragma pack(pop)

extern backEndState_t backEnd;
extern renderer_scene_frame_state_t rendererSceneFrameState;
extern renderer_fog_t rendererFogs[R_FOG_SLOT_COUNT];
extern int32_t rendererFogCount;
extern int32_t rendererCurrentFogIndex;
extern qboolean rendererSkyboxPortalActive;
extern shaderCommands_t tess;
extern renderer_surface_fn_t rb_surfaceTable[];
extern shader_t rendererParsedShader;
/* Original shader-parser requirement bank at 0x03880468..0x03880476. */
extern uint8_t
    rendererShaderRequirements[SHADER_REQUIREMENT_COUNT];
extern shaderStage_t
    rendererParsedShaderStages[R_MAX_SHADER_STAGES];
extern glyphInfo_t rendererAsianGlyph;
extern renderer_sun_state_t rendererSunState;
extern renderer_vertex_program_t *rendererCurrentVertexProgram;
extern renderer_vertex_program_t
    rendererVertexPrograms[R_MAX_VERTEX_PROGRAMS];
extern int32_t rendererVertexProgramCount;
extern backEndData_t *rendererBackendData;
extern int32_t rendererMaxPolys;
extern int32_t rendererMaxPolyVerts;
extern image_t *imageHashTable[4096];
/* Original loader state at 0x0388bed8 and 0x0388bee0. The first points at a
 * serialized BSP byte stream, where lump byte offsets are the correct
 * representation rather than typed object indexing. */
extern uint8_t *rendererWorldFileBase;
extern world_t rendererWorldData;
extern uint32_t rendererTextureMinFilter;
extern uint32_t rendererTextureMagFilter;
extern uint8_t rendererGammaTable[256];
extern uint8_t rendererGammaOverbrightTable[256];
extern uint8_t rendererOverbrightTable[256];
extern uint8_t rendererInverseOverbrightTable[256];
extern uint8_t rendererIntensityTable[256];
extern uint32_t rendererCurrentFragmentShader;
extern plane_t rendererMarkProjectionPlane;
extern int32_t rendererRegisteredFontCount;
extern qboolean rendererAsianFontLoaded;
extern qboolean rbSetArraysOnce;
extern int32_t rendererVisibleModelWarningView;
extern renderer_light_vis_cache_entry_t
    rendererLightVisCache[R_LIGHT_VIS_BUCKET_COUNT]
                         [R_LIGHT_VIS_ENTRIES_PER_BUCKET];
extern int32_t rendererLightVisMaxAssociativity;
extern int32_t rendererLightVisUsedEntryCount;
extern int32_t rendererLightVisFlushedEntryCount;
extern int32_t rendererLightVisRuntimeFillCount;
extern renderer_light_vis_history_entry_t *rendererLightVisHistory;
extern int32_t rendererLightVisHistoryCount;
extern renderer_light_vis_sort_entry_t *rendererLightVisSortedHistory;
extern int32_t rendererLightVisSortedHistoryCount;
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(renderer_aabb_tree_t) == 0x04,
               "i386 renderer AABB-tree alignment changed");
_Static_assert(offsetof(renderer_aabb_tree_t, mins) == 0x00,
               "i386 renderer AABB-tree minimum-bounds offset changed");
_Static_assert(sizeof(((renderer_aabb_tree_t *)0)->mins) == 0x0c,
               "i386 renderer AABB-tree minimum-bounds extent changed");
_Static_assert(offsetof(renderer_aabb_tree_t, maxs) == 0x0c,
               "i386 renderer AABB-tree maximum-bounds offset changed");
_Static_assert(sizeof(((renderer_aabb_tree_t *)0)->maxs) == 0x0c,
               "i386 renderer AABB-tree maximum-bounds extent changed");
_Static_assert(offsetof(renderer_aabb_tree_t, surfaces) == 0x18,
               "i386 renderer AABB-tree surface-pointer offset changed");
_Static_assert(sizeof(((renderer_aabb_tree_t *)0)->surfaces) == 0x04,
               "i386 renderer AABB-tree surface-pointer extent changed");
_Static_assert(offsetof(renderer_aabb_tree_t, surfaceCount) == 0x1c,
               "i386 renderer AABB-tree surface-count offset changed");
_Static_assert(sizeof(((renderer_aabb_tree_t *)0)->surfaceCount) == 0x04,
               "i386 renderer AABB-tree surface-count extent changed");
_Static_assert(offsetof(renderer_aabb_tree_t, children) == 0x20,
               "i386 renderer AABB-tree child-pointer offset changed");
_Static_assert(sizeof(((renderer_aabb_tree_t *)0)->children) == 0x04,
               "i386 renderer AABB-tree child-pointer extent changed");
_Static_assert(offsetof(renderer_aabb_tree_t, childCount) == 0x24,
               "i386 renderer AABB-tree child-count offset changed");
_Static_assert(sizeof(((renderer_aabb_tree_t *)0)->childCount) == 0x04,
               "i386 renderer AABB-tree child-count extent changed");
_Static_assert(sizeof(renderer_aabb_tree_t) == 0x28,
               "original i386 renderer AABB-tree size changed");

_Static_assert(_Alignof(renderer_cull_group_t) == 0x04,
               "i386 renderer cull-group alignment changed");
_Static_assert(offsetof(renderer_cull_group_t, mins) == 0x00,
               "i386 renderer cull-group minimum-bounds offset changed");
_Static_assert(sizeof(((renderer_cull_group_t *)0)->mins) == 0x0c,
               "i386 renderer cull-group minimum-bounds extent changed");
_Static_assert(offsetof(renderer_cull_group_t, maxs) == 0x0c,
               "i386 renderer cull-group maximum-bounds offset changed");
_Static_assert(sizeof(((renderer_cull_group_t *)0)->maxs) == 0x0c,
               "i386 renderer cull-group maximum-bounds extent changed");
_Static_assert(offsetof(renderer_cull_group_t, surfaces) == 0x18,
               "i386 renderer cull-group surface-pointer offset changed");
_Static_assert(sizeof(((renderer_cull_group_t *)0)->surfaces) == 0x04,
               "i386 renderer cull-group surface-pointer extent changed");
_Static_assert(offsetof(renderer_cull_group_t, surfaceCount) == 0x1c,
               "i386 renderer cull-group surface-count offset changed");
_Static_assert(sizeof(((renderer_cull_group_t *)0)->surfaceCount) == 0x04,
               "i386 renderer cull-group surface-count extent changed");
_Static_assert(offsetof(renderer_cull_group_t, viewCount) == 0x20,
               "i386 renderer cull-group view-stamp offset changed");
_Static_assert(sizeof(((renderer_cull_group_t *)0)->viewCount) == 0x04,
               "i386 renderer cull-group view-stamp extent changed");
_Static_assert(sizeof(renderer_cull_group_t) == 0x24,
               "original i386 renderer cull-group size changed");
_Static_assert(_Alignof(renderer_portal_t) == 0x04,
               "i386 renderer portal alignment changed");
_Static_assert(offsetof(renderer_portal_t, plane) == 0x00,
               "i386 renderer portal plane offset changed");
_Static_assert(sizeof(((renderer_portal_t *)0)->plane) == 0x14,
               "i386 renderer portal plane extent changed");
_Static_assert(offsetof(renderer_portal_t, cell) == 0x14,
               "i386 renderer portal cell-pointer offset changed");
_Static_assert(sizeof(((renderer_portal_t *)0)->cell) == 0x04,
               "i386 renderer portal cell-pointer extent changed");
_Static_assert(offsetof(renderer_portal_t, vertices) == 0x18,
               "i386 renderer portal vertex-pointer offset changed");
_Static_assert(sizeof(((renderer_portal_t *)0)->vertices) == 0x04,
               "i386 renderer portal vertex-pointer extent changed");
_Static_assert(offsetof(renderer_portal_t, vertexCount) == 0x1c,
               "i386 renderer portal vertex-count offset changed");
_Static_assert(sizeof(((renderer_portal_t *)0)->vertexCount) == 0x04,
               "i386 renderer portal vertex-count extent changed");
_Static_assert(offsetof(renderer_portal_t, recursionActive) == 0x20,
               "i386 renderer portal recursion-guard offset changed");
_Static_assert(sizeof(((renderer_portal_t *)0)->recursionActive) == 0x04,
               "i386 renderer portal recursion-guard extent changed");
_Static_assert(sizeof(renderer_portal_t) == 0x24,
               "original i386 renderer portal size changed");

_Static_assert(_Alignof(renderer_occluder_edge_t) == 0x04,
               "i386 renderer occluder-edge alignment changed");
_Static_assert(offsetof(renderer_occluder_edge_t, planes) == 0x00,
               "i386 renderer occluder-edge planes offset changed");
_Static_assert(sizeof(((renderer_occluder_edge_t *)0)->planes) == 0x08,
               "i386 renderer occluder-edge planes extent changed");
_Static_assert(offsetof(renderer_occluder_edge_t, vertices) == 0x08,
               "i386 renderer occluder-edge vertices offset changed");
_Static_assert(sizeof(((renderer_occluder_edge_t *)0)->vertices) == 0x08,
               "i386 renderer occluder-edge vertices extent changed");
_Static_assert(sizeof(renderer_occluder_edge_t) == 0x10,
               "original i386 renderer occluder-edge size changed");

_Static_assert(_Alignof(renderer_occluder_t) == 0x04,
               "i386 renderer occluder alignment changed");
_Static_assert(offsetof(renderer_occluder_t, planes) == 0x00,
               "i386 renderer occluder plane-pointer offset changed");
_Static_assert(sizeof(((renderer_occluder_t *)0)->planes) == 0x04,
               "i386 renderer occluder plane-pointer extent changed");
_Static_assert(offsetof(renderer_occluder_t, planeCount) == 0x04,
               "i386 renderer occluder plane-count offset changed");
_Static_assert(sizeof(((renderer_occluder_t *)0)->planeCount) == 0x04,
               "i386 renderer occluder plane-count extent changed");
_Static_assert(offsetof(renderer_occluder_t, edgeCount) == 0x08,
               "i386 renderer occluder edge-count offset changed");
_Static_assert(sizeof(((renderer_occluder_t *)0)->edgeCount) == 0x04,
               "i386 renderer occluder edge-count extent changed");
_Static_assert(offsetof(renderer_occluder_t, edges) == 0x0c,
               "i386 renderer occluder edge-pointer offset changed");
_Static_assert(sizeof(((renderer_occluder_t *)0)->edges) == 0x04,
               "i386 renderer occluder edge-pointer extent changed");
_Static_assert(offsetof(renderer_occluder_t, vertexCount) == 0x10,
               "i386 renderer occluder vertex-count offset changed");
_Static_assert(sizeof(((renderer_occluder_t *)0)->vertexCount) == 0x04,
               "i386 renderer occluder vertex-count extent changed");
_Static_assert(offsetof(renderer_occluder_t, vertices) == 0x14,
               "i386 renderer occluder vertex-pointer offset changed");
_Static_assert(sizeof(((renderer_occluder_t *)0)->vertices) == 0x04,
               "i386 renderer occluder vertex-pointer extent changed");
_Static_assert(offsetof(renderer_occluder_t, cullPlaneLimit) == 0x18,
               "i386 renderer occluder cull-plane limit offset changed");
_Static_assert(sizeof(((renderer_occluder_t *)0)->cullPlaneLimit) == 0x04,
               "i386 renderer occluder cull-plane limit extent changed");
_Static_assert(offsetof(renderer_occluder_t, activePlaneCount) == 0x1c,
               "i386 renderer occluder active-plane count offset changed");
_Static_assert(sizeof(((renderer_occluder_t *)0)->activePlaneCount) == 0x04,
               "i386 renderer occluder active-plane count extent changed");
_Static_assert(offsetof(renderer_occluder_t, activePlanes) == 0x20,
               "i386 renderer occluder active-plane pointer offset changed");
_Static_assert(sizeof(((renderer_occluder_t *)0)->activePlanes) == 0x04,
               "i386 renderer occluder active-plane pointer extent changed");
_Static_assert(sizeof(renderer_occluder_t) == 0x24,
               "original i386 renderer occluder size changed");
_Static_assert(_Alignof(renderer_cell_portal_reference_t) == 0x04,
               "i386 renderer cell portal-reference alignment changed");
_Static_assert(offsetof(renderer_cell_portal_reference_t, portals) == 0x00,
               "i386 renderer cell portal-pointer offset changed");
_Static_assert(sizeof(((renderer_cell_portal_reference_t *)0)->portals) ==
                   0x04,
               "i386 renderer cell portal-pointer extent changed");
_Static_assert(offsetof(renderer_cell_portal_reference_t, firstPortal) ==
                   0x00,
               "i386 renderer cell first-portal offset changed");
_Static_assert(sizeof(((renderer_cell_portal_reference_t *)0)->firstPortal) ==
                   0x04,
               "i386 renderer cell first-portal extent changed");
_Static_assert(sizeof(renderer_cell_portal_reference_t) == 0x04,
               "original i386 renderer cell portal-reference size changed");

_Static_assert(_Alignof(renderer_cell_model_link_t) == 0x04,
               "i386 renderer cell-model link alignment changed");
_Static_assert(offsetof(renderer_cell_model_link_t, model) == 0x00,
               "i386 renderer cell-model link model offset changed");
_Static_assert(sizeof(((renderer_cell_model_link_t *)0)->model) == 0x04,
               "i386 renderer cell-model link model extent changed");
_Static_assert(offsetof(renderer_cell_model_link_t, next) == 0x04,
               "i386 renderer cell-model link next offset changed");
_Static_assert(sizeof(((renderer_cell_model_link_t *)0)->next) == 0x04,
               "i386 renderer cell-model link next extent changed");
_Static_assert(sizeof(renderer_cell_model_link_t) == 0x08,
               "original i386 renderer cell-model link size changed");

_Static_assert(_Alignof(renderer_cell_entity_link_t) == 0x04,
               "i386 renderer cell-entity link alignment changed");
_Static_assert(offsetof(renderer_cell_entity_link_t, entity) == 0x00,
               "i386 renderer cell-entity link entity offset changed");
_Static_assert(sizeof(((renderer_cell_entity_link_t *)0)->entity) == 0x04,
               "i386 renderer cell-entity link entity extent changed");
_Static_assert(offsetof(renderer_cell_entity_link_t, mins) == 0x04,
               "i386 renderer cell-entity link minimum-bounds offset changed");
_Static_assert(sizeof(((renderer_cell_entity_link_t *)0)->mins) == 0x0c,
               "i386 renderer cell-entity link minimum-bounds extent changed");
_Static_assert(offsetof(renderer_cell_entity_link_t, maxs) == 0x10,
               "i386 renderer cell-entity link maximum-bounds offset changed");
_Static_assert(sizeof(((renderer_cell_entity_link_t *)0)->maxs) == 0x0c,
               "i386 renderer cell-entity link maximum-bounds extent changed");
_Static_assert(offsetof(renderer_cell_entity_link_t, next) == 0x1c,
               "i386 renderer cell-entity link next offset changed");
_Static_assert(sizeof(((renderer_cell_entity_link_t *)0)->next) == 0x04,
               "i386 renderer cell-entity link next extent changed");
_Static_assert(sizeof(renderer_cell_entity_link_t) == 0x20,
               "original i386 renderer cell-entity link size changed");

_Static_assert(_Alignof(renderer_world_cell_t) == 0x04,
               "i386 renderer world-cell alignment changed");
_Static_assert(offsetof(renderer_world_cell_t, mins) == 0x00,
               "i386 renderer world-cell minimum-bounds offset changed");
_Static_assert(sizeof(((renderer_world_cell_t *)0)->mins) == 0x0c,
               "i386 renderer world-cell minimum-bounds extent changed");
_Static_assert(offsetof(renderer_world_cell_t, maxs) == 0x0c,
               "i386 renderer world-cell maximum-bounds offset changed");
_Static_assert(sizeof(((renderer_world_cell_t *)0)->maxs) == 0x0c,
               "i386 renderer world-cell maximum-bounds extent changed");
_Static_assert(offsetof(renderer_world_cell_t, aabbTree) == 0x18,
               "i386 renderer world-cell AABB-tree offset changed");
_Static_assert(sizeof(((renderer_world_cell_t *)0)->aabbTree) == 0x04,
               "i386 renderer world-cell AABB-tree extent changed");
_Static_assert(offsetof(renderer_world_cell_t, portalReference) == 0x1c,
               "i386 renderer world-cell portal-reference offset changed");
_Static_assert(sizeof(((renderer_world_cell_t *)0)->portalReference) == 0x04,
               "i386 renderer world-cell portal-reference extent changed");
_Static_assert(offsetof(renderer_world_cell_t, portalCount) == 0x20,
               "i386 renderer world-cell portal-count offset changed");
_Static_assert(sizeof(((renderer_world_cell_t *)0)->portalCount) == 0x04,
               "i386 renderer world-cell portal-count extent changed");
_Static_assert(offsetof(renderer_world_cell_t, cullGroups) == 0x24,
               "i386 renderer world-cell cull-group list offset changed");
_Static_assert(sizeof(((renderer_world_cell_t *)0)->cullGroups) == 0x04,
               "i386 renderer world-cell cull-group list extent changed");
_Static_assert(offsetof(renderer_world_cell_t, cullGroupCount) == 0x28,
               "i386 renderer world-cell cull-group count offset changed");
_Static_assert(sizeof(((renderer_world_cell_t *)0)->cullGroupCount) == 0x04,
               "i386 renderer world-cell cull-group count extent changed");
_Static_assert(offsetof(renderer_world_cell_t, occluders) == 0x2c,
               "i386 renderer world-cell occluder list offset changed");
_Static_assert(sizeof(((renderer_world_cell_t *)0)->occluders) == 0x04,
               "i386 renderer world-cell occluder list extent changed");
_Static_assert(offsetof(renderer_world_cell_t, occluderCount) == 0x30,
               "i386 renderer world-cell occluder count offset changed");
_Static_assert(sizeof(((renderer_world_cell_t *)0)->occluderCount) == 0x04,
               "i386 renderer world-cell occluder count extent changed");
_Static_assert(offsetof(renderer_world_cell_t, markViewCount) == 0x34,
               "i386 renderer world-cell mark-view stamp offset changed");
_Static_assert(sizeof(((renderer_world_cell_t *)0)->markViewCount) == 0x04,
               "i386 renderer world-cell mark-view stamp extent changed");
_Static_assert(offsetof(renderer_world_cell_t, modelLinks) == 0x38,
               "i386 renderer world-cell model-link root offset changed");
_Static_assert(sizeof(((renderer_world_cell_t *)0)->modelLinks) == 0x04,
               "i386 renderer world-cell model-link root extent changed");
_Static_assert(offsetof(renderer_world_cell_t, entityLinks) == 0x3c,
               "i386 renderer world-cell entity-link root offset changed");
_Static_assert(sizeof(((renderer_world_cell_t *)0)->entityLinks) == 0x04,
               "i386 renderer world-cell entity-link root extent changed");
_Static_assert(sizeof(renderer_world_cell_t) == 0x40,
               "original i386 renderer world-cell size changed");
_Static_assert(_Alignof(renderer_static_model_t2v3_vertex_t) == 0x04,
               "i386 static-model T2V3 vertex alignment changed");
_Static_assert(offsetof(renderer_static_model_t2v3_vertex_t, texCoord) == 0x00,
               "i386 static-model T2V3 texture offset changed");
_Static_assert(sizeof(((renderer_static_model_t2v3_vertex_t *)0)->texCoord) ==
                   0x08,
               "i386 static-model T2V3 texture extent changed");
_Static_assert(offsetof(renderer_static_model_t2v3_vertex_t, position) == 0x08,
               "i386 static-model T2V3 position offset changed");
_Static_assert(sizeof(((renderer_static_model_t2v3_vertex_t *)0)->position) ==
                   0x0c,
               "i386 static-model T2V3 position extent changed");
_Static_assert(sizeof(renderer_static_model_t2v3_vertex_t) == 0x14,
               "original i386 static-model T2V3 vertex size changed");

_Static_assert(_Alignof(renderer_static_model_t2n3v3_vertex_t) == 0x04,
               "i386 static-model T2N3V3 vertex alignment changed");
_Static_assert(offsetof(renderer_static_model_t2n3v3_vertex_t, texCoord) ==
                   0x00,
               "i386 static-model T2N3V3 texture offset changed");
_Static_assert(sizeof(((renderer_static_model_t2n3v3_vertex_t *)0)->texCoord) ==
                   0x08,
               "i386 static-model T2N3V3 texture extent changed");
_Static_assert(offsetof(renderer_static_model_t2n3v3_vertex_t, normal) == 0x08,
               "i386 static-model T2N3V3 normal offset changed");
_Static_assert(sizeof(((renderer_static_model_t2n3v3_vertex_t *)0)->normal) ==
                   0x0c,
               "i386 static-model T2N3V3 normal extent changed");
_Static_assert(offsetof(renderer_static_model_t2n3v3_vertex_t, position) == 0x14,
               "i386 static-model T2N3V3 position offset changed");
_Static_assert(sizeof(((renderer_static_model_t2n3v3_vertex_t *)0)->position) ==
                   0x0c,
               "i386 static-model T2N3V3 position extent changed");
_Static_assert(sizeof(renderer_static_model_t2n3v3_vertex_t) == 0x20,
               "original i386 static-model T2N3V3 vertex size changed");
_Static_assert(_Alignof(renderer_static_model_optimized_data_t) == 0x04,
               "i386 static-model optimized-data alignment changed");
_Static_assert(offsetof(renderer_static_model_optimized_data_t, vertices) ==
                   0x00,
               "i386 static-model optimized vertex-pointer offset changed");
_Static_assert(sizeof(((renderer_static_model_optimized_data_t *)0)
                          ->vertices) == 0x04,
               "i386 static-model optimized vertex-pointer extent changed");
_Static_assert(offsetof(renderer_static_model_optimized_data_t,
                        vertexBuffer) == 0x00,
               "i386 static-model ARB vertex-buffer offset changed");
_Static_assert(sizeof(((renderer_static_model_optimized_data_t *)0)
                          ->vertexBuffer) == 0x04,
               "i386 static-model ARB vertex-buffer extent changed");
_Static_assert(offsetof(renderer_static_model_optimized_data_t,
                        atiObjectBuffer) == 0x00,
               "i386 static-model ATI object-buffer offset changed");
_Static_assert(sizeof(((renderer_static_model_optimized_data_t *)0)
                          ->atiObjectBuffer) == 0x04,
               "i386 static-model ATI object-buffer extent changed");
_Static_assert(sizeof(renderer_static_model_optimized_data_t) == 0x04,
               "original i386 static-model optimized-data size changed");

_Static_assert(_Alignof(renderer_static_model_backend_data_t) == 0x04,
               "i386 static-model backend-data alignment changed");
_Static_assert(offsetof(renderer_static_model_backend_data_t,
                        arb.indexBuffer) == 0x00,
               "i386 static-model ARB index-buffer offset changed");
_Static_assert(sizeof(((renderer_static_model_backend_data_t *)0)
                          ->arb.indexBuffer) == 0x04,
               "i386 static-model ARB index-buffer extent changed");
_Static_assert(offsetof(renderer_static_model_backend_data_t,
                        ati.vertexOffset) == 0x00,
               "i386 static-model ATI vertex-offset changed");
_Static_assert(sizeof(((renderer_static_model_backend_data_t *)0)
                          ->ati.vertexOffset) == 0x04,
               "i386 static-model ATI vertex-offset extent changed");
_Static_assert(offsetof(renderer_static_model_backend_data_t,
                        ati.indexOffset) == 0x04,
               "i386 static-model ATI index-offset changed");
_Static_assert(sizeof(((renderer_static_model_backend_data_t *)0)
                          ->ati.indexOffset) == 0x04,
               "i386 static-model ATI index-offset extent changed");
_Static_assert(sizeof(((renderer_static_model_backend_data_t *)0)->arb) ==
                   0x04,
               "i386 static-model ARB backend arm extent changed");
_Static_assert(sizeof(((renderer_static_model_backend_data_t *)0)->ati) ==
                   0x08,
               "i386 static-model ATI backend arm extent changed");
_Static_assert(sizeof(renderer_static_model_backend_data_t) == 0x08,
               "original i386 static-model backend-data size changed");

_Static_assert(_Alignof(renderer_static_model_surface_t) == 0x04,
               "i386 static-model surface alignment changed");
_Static_assert(offsetof(renderer_static_model_surface_t, surfaceType) == 0x00,
               "i386 static-model surface type offset changed");
_Static_assert(sizeof(((renderer_static_model_surface_t *)0)->surfaceType) ==
                   0x04,
               "i386 static-model surface type extent changed");
_Static_assert(offsetof(renderer_static_model_surface_t, storageSource) ==
                   0x04,
               "i386 static-model surface storage-source offset changed");
_Static_assert(sizeof(((renderer_static_model_surface_t *)0)->storageSource) ==
                   0x04,
               "i386 static-model surface storage-source extent changed");
_Static_assert(offsetof(renderer_static_model_surface_t, shader) == 0x08,
               "i386 static-model surface shader offset changed");
_Static_assert(sizeof(((renderer_static_model_surface_t *)0)->shader) == 0x04,
               "i386 static-model surface shader-pointer extent changed");
_Static_assert(offsetof(renderer_static_model_surface_t, cachedShader) ==
                   0x0c,
               "i386 static-model surface cached-shader offset changed");
_Static_assert(sizeof(((renderer_static_model_surface_t *)0)->cachedShader) ==
                   0x04,
               "i386 static-model surface cached-shader extent changed");
_Static_assert(offsetof(renderer_static_model_surface_t, indexCount) == 0x10,
               "i386 static-model surface index-count offset changed");
_Static_assert(sizeof(((renderer_static_model_surface_t *)0)->indexCount) ==
                   0x02,
               "i386 static-model surface index-count extent changed");
_Static_assert(offsetof(renderer_static_model_surface_t, vertexCount) == 0x12,
               "i386 static-model surface vertex-count offset changed");
_Static_assert(sizeof(((renderer_static_model_surface_t *)0)->vertexCount) ==
                   0x02,
               "i386 static-model surface vertex-count extent changed");
_Static_assert(offsetof(renderer_static_model_surface_t, texCoords) == 0x14,
               "i386 static-model surface texcoord offset changed");
_Static_assert(sizeof(((renderer_static_model_surface_t *)0)->texCoords) ==
                   0x04,
               "i386 static-model surface texcoord-pointer extent changed");
_Static_assert(offsetof(renderer_static_model_surface_t, normals) == 0x18,
               "i386 static-model surface normal offset changed");
_Static_assert(sizeof(((renderer_static_model_surface_t *)0)->normals) == 0x04,
               "i386 static-model surface normal-pointer extent changed");
_Static_assert(offsetof(renderer_static_model_surface_t, vertices) == 0x1c,
               "i386 static-model surface vertex offset changed");
_Static_assert(sizeof(((renderer_static_model_surface_t *)0)->vertices) ==
                   0x04,
               "i386 static-model surface vertex-pointer extent changed");
_Static_assert(offsetof(renderer_static_model_surface_t, indices) == 0x20,
               "i386 static-model surface index offset changed");
_Static_assert(sizeof(((renderer_static_model_surface_t *)0)->indices) == 0x04,
               "i386 static-model surface index-pointer extent changed");
_Static_assert(offsetof(renderer_static_model_surface_t, optimized) == 0x24,
               "i386 static-model optimized-data offset changed");
_Static_assert(sizeof(((renderer_static_model_surface_t *)0)->optimized) ==
                   0x04,
               "i386 static-model optimized-data extent changed");
_Static_assert(offsetof(renderer_static_model_surface_t, backend) == 0x28,
               "i386 static-model backend-data offset changed");
_Static_assert(sizeof(((renderer_static_model_surface_t *)0)->backend) == 0x08,
               "i386 static-model backend-data extent changed");
_Static_assert(sizeof(renderer_static_model_surface_t) == 0x30,
               "original i386 static-model surface size changed");
_Static_assert(_Alignof(renderer_static_model_lod_t) == 0x04,
               "i386 static-model LOD alignment changed");
_Static_assert(offsetof(renderer_static_model_lod_t, surfaceCount) == 0x00,
               "i386 static-model LOD surface-count offset changed");
_Static_assert(sizeof(((renderer_static_model_lod_t *)0)->surfaceCount) ==
                   0x04,
               "i386 static-model LOD surface-count extent changed");
_Static_assert(offsetof(renderer_static_model_lod_t, surfaces) == 0x04,
               "i386 static-model LOD surface-array offset changed");
_Static_assert(sizeof(renderer_static_model_lod_t) == 0x04,
               "original i386 static-model LOD header size changed");
_Static_assert(_Alignof(renderer_registered_static_model_t) == 0x04,
               "i386 registered static-model alignment changed");
_Static_assert(offsetof(renderer_registered_static_model_t, name) == 0x00,
               "i386 registered static-model name offset changed");
_Static_assert(sizeof(((renderer_registered_static_model_t *)0)->name) ==
                   0x04,
               "i386 registered static-model name-pointer extent changed");
_Static_assert(offsetof(renderer_registered_static_model_t, lodCount) ==
                   0x04,
               "i386 registered static-model LOD-count offset changed");
_Static_assert(sizeof(((renderer_registered_static_model_t *)0)->lodCount) ==
                   0x04,
               "i386 registered static-model LOD-count extent changed");
_Static_assert(offsetof(renderer_registered_static_model_t, model) == 0x08,
               "i386 registered static-model source-model offset changed");
_Static_assert(sizeof(((renderer_registered_static_model_t *)0)->model) ==
                   0x04,
               "i386 registered static-model source-model extent changed");
_Static_assert(offsetof(renderer_registered_static_model_t, lods) == 0x0c,
               "i386 registered static-model LOD-array offset changed");
_Static_assert(sizeof(renderer_registered_static_model_t) == 0x0c,
               "original i386 registered static-model header size changed");
_Static_assert(_Alignof(renderer_static_model_instance_t) == 0x04,
               "i386 static-model instance alignment changed");
_Static_assert(offsetof(renderer_static_model_instance_t, name) == 0x00,
               "i386 static-model instance name offset changed");
_Static_assert(sizeof(((renderer_static_model_instance_t *)0)->name) == 0x40,
               "i386 static-model instance name extent changed");
_Static_assert(offsetof(renderer_static_model_instance_t, origin) == 0x40,
               "i386 static-model instance origin offset changed");
_Static_assert(sizeof(((renderer_static_model_instance_t *)0)->origin) ==
                   0x0c,
               "i386 static-model instance origin extent changed");
_Static_assert(offsetof(renderer_static_model_instance_t, axis) == 0x4c,
               "i386 static-model instance axis offset changed");
_Static_assert(sizeof(((renderer_static_model_instance_t *)0)->axis) == 0x24,
               "i386 static-model instance axis extent changed");
_Static_assert(offsetof(renderer_static_model_instance_t,
                        nonNormalizedAxes) == 0x70,
               "i386 static-model instance axis-marker offset changed");
_Static_assert(sizeof(((renderer_static_model_instance_t *)0)
                          ->nonNormalizedAxes) == 0x04,
               "i386 static-model instance axis-marker extent changed");
_Static_assert(offsetof(renderer_static_model_instance_t, lightingPrecalc) ==
                   0x74,
               "i386 static-model instance lighting offset changed");
_Static_assert(sizeof(((renderer_static_model_instance_t *)0)
                          ->lightingPrecalc) == 0x0c,
               "i386 static-model instance lighting extent changed");
_Static_assert(offsetof(renderer_static_model_instance_t, registration) ==
                   0x80,
               "i386 static-model instance registration offset changed");
_Static_assert(sizeof(((renderer_static_model_instance_t *)0)->registration) ==
                   0x04,
               "i386 static-model instance registration extent changed");
_Static_assert(offsetof(renderer_static_model_instance_t, mins) == 0x84,
               "i386 static-model instance minimum-bounds offset changed");
_Static_assert(sizeof(((renderer_static_model_instance_t *)0)->mins) == 0x0c,
               "i386 static-model instance minimum-bounds extent changed");
_Static_assert(offsetof(renderer_static_model_instance_t, maxs) == 0x90,
               "i386 static-model instance maximum-bounds offset changed");
_Static_assert(sizeof(((renderer_static_model_instance_t *)0)->maxs) == 0x0c,
               "i386 static-model instance maximum-bounds extent changed");
_Static_assert(offsetof(renderer_static_model_instance_t, next) == 0x9c,
               "i386 static-model instance link offset changed");
_Static_assert(sizeof(((renderer_static_model_instance_t *)0)->next) == 0x04,
               "i386 static-model instance link extent changed");
_Static_assert(sizeof(renderer_static_model_instance_t) == 0xa0,
               "original i386 static-model instance size changed");
_Static_assert(_Alignof(renderer_mark_clip_vertex_t) == 0x04,
               "i386 renderer mark clip-vertex alignment changed");
_Static_assert(offsetof(renderer_mark_clip_vertex_t, xyz) == 0x00,
               "i386 renderer mark clip-vertex position offset changed");
_Static_assert(sizeof(((renderer_mark_clip_vertex_t *)0)->xyz) == 0x0c,
               "i386 renderer mark clip-vertex position extent changed");
_Static_assert(offsetof(renderer_mark_clip_vertex_t, lightmapCoords) == 0x0c,
               "i386 renderer mark clip-vertex lightmap offset changed");
_Static_assert(sizeof(((renderer_mark_clip_vertex_t *)0)->lightmapCoords) ==
                   0x08,
               "i386 renderer mark clip-vertex lightmap extent changed");
_Static_assert(sizeof(renderer_mark_clip_vertex_t) == 0x14,
               "original i386 renderer mark clip-vertex size changed");
_Static_assert(sizeof(renderer_rgba8_t) == 0x04,
               "renderer RGBA8 size changed");
_Static_assert(_Alignof(renderer_rgba8_t) == 0x01,
               "renderer RGBA8 alignment changed");
_Static_assert(offsetof(backEndCounters_t, surfaceCount) == 0x00,
               "i386 backend surface counter moved");
_Static_assert(offsetof(backEndCounters_t, shaderCount) == 0x04,
               "i386 backend shader counter moved");
_Static_assert(offsetof(backEndCounters_t, vertexCount) == 0x08,
               "i386 backend vertex counter moved");
_Static_assert(offsetof(backEndCounters_t, indexCount) == 0x0c,
               "i386 backend index counter moved");
_Static_assert(offsetof(backEndCounters_t, drawnIndexCount) ==
                   0x10,
               "i386 backend drawn-index counter moved");
_Static_assert(offsetof(backEndCounters_t, drawCallCount) == 0x14,
               "i386 backend draw-call counter moved");
_Static_assert(offsetof(backEndCounters_t, overdrawSum) == 0x18,
               "i386 backend overdraw sum moved");
_Static_assert(offsetof(backEndCounters_t, dlightVertexCount) ==
                   0x1c,
               "i386 backend dlight-vertex counter moved");
_Static_assert(offsetof(backEndCounters_t, dlightIndexCount) ==
                   0x20,
               "i386 backend dlight-index counter moved");
_Static_assert(offsetof(backEndCounters_t, flareAddCount) == 0x24,
               "i386 backend flare-add counter moved");
_Static_assert(offsetof(backEndCounters_t, flareTestCount) == 0x28,
               "i386 backend flare-test counter moved");
_Static_assert(offsetof(backEndCounters_t, flareRenderCount) ==
                   0x2c,
               "i386 backend flare-render counter moved");
_Static_assert(offsetof(backEndCounters_t, commandMsec) == 0x30,
               "i386 backend command-time counter moved");
_Static_assert(sizeof(backEndCounters_t) == 0x34,
               "i386 backend counter block size changed");
_Static_assert(offsetof(backEndState_t, refdef) == 0x000,
               "i386 backend refdef offset changed");
_Static_assert(offsetof(backEndState_t, viewParms) == 0x188,
               "i386 backend view-parms offset changed");
_Static_assert(offsetof(backEndState_t, orientation) == 0x3e8,
               "i386 backend orientation offset changed");
_Static_assert(offsetof(backEndState_t, pc) == 0x464,
               "i386 backend counters offset changed");
_Static_assert(offsetof(backEndState_t, pc.overdrawSum) == 0x47c,
               "i386 backend overdraw-sum offset changed");
_Static_assert(offsetof(backEndState_t, pc.dlightVertexCount) == 0x480,
               "i386 backend dlight-vertex counter offset changed");
_Static_assert(offsetof(backEndState_t, pc.dlightIndexCount) == 0x484,
               "i386 backend dlight-index counter offset changed");
_Static_assert(offsetof(backEndState_t, pc.flareAddCount) == 0x488,
               "i386 backend flare-add counter offset changed");
_Static_assert(offsetof(backEndState_t, pc.flareTestCount) == 0x48c,
               "i386 backend flare-test counter offset changed");
_Static_assert(offsetof(backEndState_t, pc.flareRenderCount) == 0x490,
               "i386 backend flare-render counter offset changed");
_Static_assert(offsetof(backEndState_t, pc.commandMsec) == 0x494,
               "i386 backend command-time offset changed");
_Static_assert(offsetof(backEndState_t, isHyperspace) == 0x498,
               "i386 backend hyperspace offset changed");
_Static_assert(offsetof(backEndState_t, currentEntity) == 0x49c,
               "i386 backend current-entity offset changed");
_Static_assert(offsetof(backEndState_t, currentLight) == 0x4a0,
               "i386 backend current-light offset changed");
_Static_assert(offsetof(backEndState_t, currentLightScale) == 0x4a4,
               "i386 backend current-light-scale offset changed");
_Static_assert(offsetof(backEndState_t, skyRenderedThisView) == 0x4a8,
               "i386 backend sky-rendered marker offset changed");
_Static_assert(offsetof(backEndState_t, endFramePending) == 0x4ac,
               "i386 backend end-frame marker offset changed");
_Static_assert(offsetof(backEndState_t, projection2D) == 0x4b0,
               "i386 backend projection-2D offset changed");
_Static_assert(offsetof(backEndState_t, color2D) == 0x4b4,
               "i386 backend 2D-color offset changed");
_Static_assert(offsetof(backEndState_t, colorCode8) == 0x4b8,
               "i386 backend custom color-code 8 offset changed");
_Static_assert(offsetof(backEndState_t, colorCode9) == 0x4bc,
               "i386 backend custom color-code 9 offset changed");
_Static_assert(offsetof(backEndState_t, vertexes2D) == 0x4c0,
               "i386 backend dormant 2D-vertex marker offset changed");
_Static_assert(offsetof(backEndState_t, entity2D) == 0x4c4,
               "i386 backend 2D-entity offset changed");
_Static_assert(offsetof(backEndState_t, debugEntity) == 0x77c,
               "i386 backend debug-entity offset changed");
_Static_assert(offsetof(backEndState_t, dynamicBuffer) == 0xa34,
               "i386 backend dynamic-buffer offset changed");
_Static_assert(sizeof(backEndState_t) == 0x1a54,
               "i386 backend-state size changed");
_Static_assert(_Alignof(renderer_lit_surface_t) == 0x04,
               "i386 lit-surface prefix alignment changed");
_Static_assert(offsetof(renderer_lit_surface_t, surfaceType) == 0x00,
               "i386 lit-surface type offset changed");
_Static_assert(sizeof(((renderer_lit_surface_t *)0)->surfaceType) == 0x04,
               "i386 lit-surface type extent changed");
_Static_assert(offsetof(renderer_lit_surface_t, storageMode) == 0x04,
               "i386 lit-surface storage-mode offset changed");
_Static_assert(sizeof(((renderer_lit_surface_t *)0)->storageMode) == 0x04,
               "i386 lit-surface storage-mode extent changed");
_Static_assert(offsetof(renderer_lit_surface_t, dlightBits) == 0x08,
               "i386 lit-surface dlight-mask offset changed");
_Static_assert(sizeof(((renderer_lit_surface_t *)0)->dlightBits) == 0x04,
               "i386 lit-surface dlight-mask extent changed");
_Static_assert(offsetof(renderer_lit_surface_t, boundsMin) == 0x0c,
               "i386 lit-surface minimum-bounds offset changed");
_Static_assert(sizeof(((renderer_lit_surface_t *)0)->boundsMin) == 0x0c,
               "i386 lit-surface minimum-bounds extent changed");
_Static_assert(offsetof(renderer_lit_surface_t, boundsMax) == 0x18,
               "i386 lit-surface maximum-bounds offset changed");
_Static_assert(sizeof(((renderer_lit_surface_t *)0)->boundsMax) == 0x0c,
               "i386 lit-surface maximum-bounds extent changed");
_Static_assert(sizeof(renderer_lit_surface_t) == 0x24,
               "original i386 lit-surface prefix size changed");

_Static_assert(_Alignof(renderer_world_mesh_surface_t) == 0x04,
               "i386 world-mesh surface alignment changed");
_Static_assert(offsetof(renderer_world_mesh_surface_t, surfaceType) == 0x00,
               "i386 world-mesh surface type offset changed");
_Static_assert(sizeof(((renderer_world_mesh_surface_t *)0)->surfaceType) ==
                   0x04,
               "i386 world-mesh surface type extent changed");
_Static_assert(offsetof(renderer_world_mesh_surface_t, storageMode) == 0x04,
               "i386 world-mesh storage-mode offset changed");
_Static_assert(sizeof(((renderer_world_mesh_surface_t *)0)->storageMode) ==
                   0x04,
               "i386 world-mesh storage-mode extent changed");
_Static_assert(offsetof(renderer_world_mesh_surface_t, dlightBits) == 0x08,
               "i386 world-mesh dlight-mask offset changed");
_Static_assert(sizeof(((renderer_world_mesh_surface_t *)0)->dlightBits) ==
                   0x04,
               "i386 world-mesh dlight-mask extent changed");
_Static_assert(offsetof(renderer_world_mesh_surface_t, boundsMin) == 0x0c,
               "i386 world-mesh minimum-bounds offset changed");
_Static_assert(sizeof(((renderer_world_mesh_surface_t *)0)->boundsMin) == 0x0c,
               "i386 world-mesh minimum-bounds extent changed");
_Static_assert(offsetof(renderer_world_mesh_surface_t, boundsMax) == 0x18,
               "i386 world-mesh maximum-bounds offset changed");
_Static_assert(sizeof(((renderer_world_mesh_surface_t *)0)->boundsMax) == 0x0c,
               "i386 world-mesh maximum-bounds extent changed");
_Static_assert(offsetof(renderer_world_mesh_surface_t, vertexCount) == 0x24,
               "i386 world-mesh vertex-count offset changed");
_Static_assert(sizeof(((renderer_world_mesh_surface_t *)0)->vertexCount) ==
                   0x04,
               "i386 world-mesh vertex-count extent changed");
_Static_assert(offsetof(renderer_world_mesh_surface_t, tangents) == 0x28,
               "i386 world-mesh tangent-pointer offset changed");
_Static_assert(sizeof(((renderer_world_mesh_surface_t *)0)->tangents) == 0x04,
               "i386 world-mesh tangent-pointer extent changed");
_Static_assert(offsetof(renderer_world_mesh_surface_t, bitangents) == 0x2c,
               "i386 world-mesh bitangent-pointer offset changed");
_Static_assert(sizeof(((renderer_world_mesh_surface_t *)0)->bitangents) ==
                   0x04,
               "i386 world-mesh bitangent-pointer extent changed");
_Static_assert(offsetof(renderer_world_mesh_surface_t, normals) == 0x30,
               "i386 world-mesh normal-pointer offset changed");
_Static_assert(sizeof(((renderer_world_mesh_surface_t *)0)->normals) == 0x04,
               "i386 world-mesh normal-pointer extent changed");
_Static_assert(offsetof(renderer_world_mesh_surface_t, texCoords) == 0x34,
               "i386 world-mesh texture-pointer offset changed");
_Static_assert(sizeof(((renderer_world_mesh_surface_t *)0)->texCoords) == 0x04,
               "i386 world-mesh texture-pointer extent changed");
_Static_assert(offsetof(renderer_world_mesh_surface_t, lightmapCoords) == 0x38,
               "i386 world-mesh lightmap-pointer offset changed");
_Static_assert(sizeof(((renderer_world_mesh_surface_t *)0)->lightmapCoords) ==
                   0x04,
               "i386 world-mesh lightmap-pointer extent changed");
_Static_assert(offsetof(renderer_world_mesh_surface_t, colors) == 0x3c,
               "i386 world-mesh color-pointer offset changed");
_Static_assert(sizeof(((renderer_world_mesh_surface_t *)0)->colors) == 0x04,
               "i386 world-mesh color-pointer extent changed");
_Static_assert(offsetof(renderer_world_mesh_surface_t, positions) == 0x40,
               "i386 world-mesh position-pointer offset changed");
_Static_assert(sizeof(((renderer_world_mesh_surface_t *)0)->positions) == 0x04,
               "i386 world-mesh position-pointer extent changed");
_Static_assert(offsetof(renderer_world_mesh_surface_t, indexCount) == 0x44,
               "i386 world-mesh index-count offset changed");
_Static_assert(sizeof(((renderer_world_mesh_surface_t *)0)->indexCount) ==
                   0x04,
               "i386 world-mesh index-count extent changed");
_Static_assert(offsetof(renderer_world_mesh_surface_t, indices) == 0x48,
               "i386 world-mesh flexible-index offset changed");
_Static_assert(sizeof(renderer_world_mesh_surface_t) == 0x48,
               "original i386 world-mesh surface header size changed");
_Static_assert(_Alignof(renderer_world_interleaved_vertex_t) == 0x04,
               "i386 optimized world-vertex alignment changed");
_Static_assert(offsetof(renderer_world_interleaved_vertex_t, texCoord) == 0x00,
               "i386 optimized world-vertex texture offset changed");
_Static_assert(sizeof(((renderer_world_interleaved_vertex_t *)0)->texCoord) ==
                   0x08,
               "i386 optimized world-vertex texture extent changed");
_Static_assert(offsetof(renderer_world_interleaved_vertex_t, lightmapCoord) ==
                   0x08,
               "i386 optimized world-vertex lightmap offset changed");
_Static_assert(sizeof(((renderer_world_interleaved_vertex_t *)0)
                          ->lightmapCoord) == 0x08,
               "i386 optimized world-vertex lightmap extent changed");
_Static_assert(offsetof(renderer_world_interleaved_vertex_t, color) == 0x10,
               "i386 optimized world-vertex color offset changed");
_Static_assert(sizeof(((renderer_world_interleaved_vertex_t *)0)->color) ==
                   0x04,
               "i386 optimized world-vertex color extent changed");
_Static_assert(offsetof(renderer_world_interleaved_vertex_t, position) == 0x14,
               "i386 optimized world-vertex position offset changed");
_Static_assert(sizeof(((renderer_world_interleaved_vertex_t *)0)->position) ==
                   0x0c,
               "i386 optimized world-vertex position extent changed");
_Static_assert(sizeof(renderer_world_interleaved_vertex_t) == 0x20,
               "original i386 optimized world-vertex size changed");

_Static_assert(_Alignof(drawVert_t) == 0x04,
               "i386 BSP world-vertex alignment changed");
_Static_assert(offsetof(drawVert_t, xyz) == 0x00,
               "i386 BSP world-vertex position offset changed");
_Static_assert(sizeof(((drawVert_t *)0)->xyz) == 0x0c,
               "i386 BSP world-vertex position extent changed");
_Static_assert(offsetof(drawVert_t, st) == 0x0c,
               "i386 BSP world-vertex texture offset changed");
_Static_assert(sizeof(((drawVert_t *)0)->st) == 0x08,
               "i386 BSP world-vertex texture extent changed");
_Static_assert(offsetof(drawVert_t, lightmap) == 0x14,
               "i386 BSP world-vertex lightmap offset changed");
_Static_assert(sizeof(((drawVert_t *)0)->lightmap) == 0x08,
               "i386 BSP world-vertex lightmap extent changed");
_Static_assert(offsetof(drawVert_t, normal) == 0x1c,
               "i386 BSP world-vertex normal offset changed");
_Static_assert(sizeof(((drawVert_t *)0)->normal) == 0x0c,
               "i386 BSP world-vertex normal extent changed");
_Static_assert(offsetof(drawVert_t, color) == 0x28,
               "i386 BSP world-vertex color offset changed");
_Static_assert(sizeof(((drawVert_t *)0)->color) == 0x04,
               "i386 BSP world-vertex color extent changed");
_Static_assert(sizeof(drawVert_t) == 0x2c,
               "original i386 BSP world-vertex size changed");
_Static_assert(_Alignof(renderer_shader_surface_build_t) == 0x04,
               "i386 shader-surface build alignment changed");
_Static_assert(offsetof(renderer_shader_surface_build_t, optimized) == 0x00,
               "i386 shader-surface optimized flag offset changed");
_Static_assert(sizeof(((renderer_shader_surface_build_t *)0)->optimized) ==
                   0x04,
               "i386 shader-surface optimized flag extent changed");
_Static_assert(offsetof(renderer_shader_surface_build_t, vertexBytes) == 0x04,
               "i386 shader-surface byte-count offset changed");
_Static_assert(sizeof(((renderer_shader_surface_build_t *)0)->vertexBytes) ==
                   0x04,
               "i386 shader-surface byte-count extent changed");
_Static_assert(offsetof(renderer_shader_surface_build_t, firstVertex) == 0x08,
               "i386 shader-surface first-vertex offset changed");
_Static_assert(sizeof(((renderer_shader_surface_build_t *)0)->firstVertex) ==
                   0x04,
               "i386 shader-surface first-vertex extent changed");
_Static_assert(offsetof(renderer_shader_surface_build_t, storageMode) == 0x0c,
               "i386 shader-surface storage-mode offset changed");
_Static_assert(sizeof(((renderer_shader_surface_build_t *)0)->storageMode) ==
                   0x04,
               "i386 shader-surface storage-mode extent changed");
_Static_assert(offsetof(renderer_shader_surface_build_t, shader) == 0x10,
               "i386 shader-surface shader offset changed");
_Static_assert(sizeof(((renderer_shader_surface_build_t *)0)->shader) == 0x04,
               "i386 shader-surface shader-pointer extent changed");
_Static_assert(sizeof(renderer_shader_surface_build_t) == 0x14,
               "original i386 shader-surface build size changed");
_Static_assert(_Alignof(drawSurf_t) == 0x04,
               "i386 draw-surface alignment changed");
_Static_assert(offsetof(drawSurf_t, sort) == 0x00,
               "i386 draw-surface sort offset changed");
_Static_assert(sizeof(((drawSurf_t *)0)->sort) == 0x04,
               "i386 draw-surface sort extent changed");
_Static_assert(offsetof(drawSurf_t, surface) == 0x04,
               "i386 draw-surface pointer offset changed");
_Static_assert(sizeof(((drawSurf_t *)0)->surface) == 0x04,
               "i386 draw-surface pointer extent changed");
_Static_assert(sizeof(drawSurf_t) == 0x08,
               "original i386 draw-surface size changed");
_Static_assert(_Alignof(renderer_fog_saved_state_t) == 0x04,
               "i386 renderer fog save-state alignment changed");
_Static_assert(offsetof(renderer_fog_saved_state_t, fogs) == 0x000,
               "i386 renderer fog save-state array offset changed");
_Static_assert(sizeof(((renderer_fog_saved_state_t *)0)->fogs) == 0x240,
               "i386 renderer fog save-state array extent changed");
_Static_assert(offsetof(renderer_fog_saved_state_t, currentFogIndex) == 0x240,
               "i386 renderer fog save-state index offset changed");
_Static_assert(sizeof(((renderer_fog_saved_state_t *)0)->currentFogIndex) ==
                   0x04,
               "i386 renderer fog save-state index extent changed");
_Static_assert(sizeof(renderer_fog_saved_state_t) == 0x244,
               "original i386 renderer fog save-state size changed");
_Static_assert(offsetof(renderer_corona_t, origin) == 0x00,
               "renderer corona origin moved");
_Static_assert(offsetof(renderer_corona_t, color) == 0x0c,
               "renderer corona color moved");
_Static_assert(offsetof(renderer_corona_t, transformed) == 0x18,
               "renderer corona transformed origin moved");
_Static_assert(offsetof(renderer_corona_t, scale) == 0x24,
               "renderer corona scale moved");
_Static_assert(offsetof(renderer_corona_t, id) == 0x28,
               "renderer corona id moved");
_Static_assert(offsetof(renderer_corona_t, flags) == 0x2c,
               "renderer corona flags moved");
_Static_assert(sizeof(renderer_corona_t) == 0x30,
               "i386 renderer-corona size changed");
_Static_assert(_Alignof(renderer_flare_source_t) == 0x04,
               "i386 renderer flare-source alignment changed");
_Static_assert(offsetof(renderer_flare_source_t, id) == 0x00,
               "i386 renderer flare-source identifier offset changed");
_Static_assert(sizeof(((renderer_flare_source_t *)0)->id) == 0x04,
               "i386 renderer flare-source identifier extent changed");
_Static_assert(offsetof(renderer_flare_source_t, shader) == 0x04,
               "i386 renderer flare-source shader offset changed");
_Static_assert(sizeof(((renderer_flare_source_t *)0)->shader) == 0x04,
               "i386 renderer flare-source shader extent changed");
_Static_assert(offsetof(renderer_flare_source_t, origin) == 0x08,
               "i386 renderer flare-source origin offset changed");
_Static_assert(sizeof(((renderer_flare_source_t *)0)->origin) == 0x0c,
               "i386 renderer flare-source origin extent changed");
_Static_assert(offsetof(renderer_flare_source_t, depthOffset) == 0x14,
               "i386 renderer flare-source depth offset changed");
_Static_assert(sizeof(((renderer_flare_source_t *)0)->depthOffset) == 0x04,
               "i386 renderer flare-source depth extent changed");
_Static_assert(offsetof(renderer_flare_source_t, color) == 0x18,
               "i386 renderer flare-source color offset changed");
_Static_assert(sizeof(((renderer_flare_source_t *)0)->color) == 0x10,
               "i386 renderer flare-source color extent changed");
_Static_assert(offsetof(renderer_flare_source_t, size) == 0x28,
               "i386 renderer flare-source size offset changed");
_Static_assert(sizeof(((renderer_flare_source_t *)0)->size) == 0x04,
               "i386 renderer flare-source size extent changed");
_Static_assert(offsetof(renderer_flare_source_t, screenRadius) == 0x2c,
               "i386 renderer flare-source screen-radius offset changed");
_Static_assert(sizeof(((renderer_flare_source_t *)0)->screenRadius) == 0x04,
               "i386 renderer flare-source screen-radius extent changed");
_Static_assert(offsetof(renderer_flare_source_t, fadeInMsec) == 0x30,
               "i386 renderer flare-source fade-in offset changed");
_Static_assert(sizeof(((renderer_flare_source_t *)0)->fadeInMsec) == 0x04,
               "i386 renderer flare-source fade-in extent changed");
_Static_assert(offsetof(renderer_flare_source_t, fadeOutMsec) == 0x34,
               "i386 renderer flare-source fade-out offset changed");
_Static_assert(sizeof(((renderer_flare_source_t *)0)->fadeOutMsec) == 0x04,
               "i386 renderer flare-source fade-out extent changed");
_Static_assert(offsetof(renderer_flare_source_t, active) == 0x38,
               "i386 renderer flare-source active offset changed");
_Static_assert(sizeof(((renderer_flare_source_t *)0)->active) == 0x04,
               "i386 renderer flare-source active extent changed");
_Static_assert(_Alignof(renderer_water_complex_t) == 0x04,
               "i386 renderer water-complex alignment changed");
_Static_assert(offsetof(renderer_water_complex_t, real) == 0x00,
               "i386 renderer water-complex real offset changed");
_Static_assert(sizeof(((renderer_water_complex_t *)0)->real) == 0x04,
               "i386 renderer water-complex real extent changed");
_Static_assert(offsetof(renderer_water_complex_t, imaginary) == 0x04,
               "i386 renderer water-complex imaginary offset changed");
_Static_assert(sizeof(((renderer_water_complex_t *)0)->imaginary) == 0x04,
               "i386 renderer water-complex imaginary extent changed");
_Static_assert(sizeof(renderer_water_complex_t) == 0x08,
               "original i386 renderer water-complex size changed");

_Static_assert(_Alignof(shader_water_map_t) == 0x04,
               "i386 shader water-map alignment changed");
_Static_assert(offsetof(shader_water_map_t, image) == 0x00,
               "i386 shader water-map image offset changed");
_Static_assert(sizeof(((shader_water_map_t *)0)->image) == 0x04,
               "i386 shader water-map image extent changed");
_Static_assert(offsetof(shader_water_map_t, uploadFrame) == 0x04,
               "i386 shader water-map upload-frame offset changed");
_Static_assert(sizeof(((shader_water_map_t *)0)->uploadFrame) == 0x04,
               "i386 shader water-map upload-frame extent changed");
_Static_assert(sizeof(((shader_water_map_t *)0)->initialFrequencies) == 0x04,
               "i386 shader water-map initial-frequency extent changed");
_Static_assert(offsetof(shader_water_map_t, angularFrequencies) == 0x0c,
               "i386 shader water-map angular-frequency offset changed");
_Static_assert(sizeof(((shader_water_map_t *)0)->angularFrequencies) == 0x04,
               "i386 shader water-map angular-frequency extent changed");
_Static_assert(sizeof(((shader_water_map_t *)0)->textureWidth) == 0x04,
               "i386 shader water-map width extent changed");
_Static_assert(offsetof(shader_water_map_t, textureHeight) == 0x14,
               "i386 shader water-map height offset changed");
_Static_assert(sizeof(((shader_water_map_t *)0)->textureHeight) == 0x04,
               "i386 shader water-map height extent changed");
_Static_assert(offsetof(shader_water_map_t, horizontalWorldLength) == 0x18,
               "i386 shader water-map horizontal-length offset changed");
_Static_assert(sizeof(((shader_water_map_t *)0)->horizontalWorldLength) ==
                   0x04,
               "i386 shader water-map horizontal-length extent changed");
_Static_assert(offsetof(shader_water_map_t, verticalWorldLength) == 0x1c,
               "i386 shader water-map vertical-length offset changed");
_Static_assert(sizeof(((shader_water_map_t *)0)->verticalWorldLength) == 0x04,
               "i386 shader water-map vertical-length extent changed");
_Static_assert(offsetof(shader_water_map_t, gravity) == 0x20,
               "i386 shader water-map gravity offset changed");
_Static_assert(sizeof(((shader_water_map_t *)0)->gravity) == 0x04,
               "i386 shader water-map gravity extent changed");
_Static_assert(offsetof(shader_water_map_t, windVelocity) == 0x24,
               "i386 shader water-map wind-velocity offset changed");
_Static_assert(sizeof(((shader_water_map_t *)0)->windVelocity) == 0x04,
               "i386 shader water-map wind-velocity extent changed");
_Static_assert(sizeof(((shader_water_map_t *)0)->windDirection) == 0x08,
               "i386 shader water-map wind-direction extent changed");
_Static_assert(offsetof(shader_water_map_t, amplitude) == 0x30,
               "i386 shader water-map amplitude offset changed");
_Static_assert(sizeof(((shader_water_map_t *)0)->amplitude) == 0x04,
               "i386 shader water-map amplitude extent changed");
_Static_assert(_Alignof(mnode_t) == 0x04,
               "i386 renderer world-node alignment changed");
_Static_assert(offsetof(mnode_t, contents) == 0x00,
               "i386 renderer world-node contents offset changed");
_Static_assert(sizeof(((mnode_t *)0)->contents) == 0x04,
               "i386 renderer world-node contents extent changed");
_Static_assert(offsetof(mnode_t, parent) == 0x04,
               "i386 renderer world-node parent offset changed");
_Static_assert(sizeof(((mnode_t *)0)->parent) == 0x04,
               "i386 renderer world-node parent extent changed");
_Static_assert(offsetof(mnode_t, cellIndex) == 0x08,
               "i386 renderer world-node cell-index offset changed");
_Static_assert(sizeof(((mnode_t *)0)->cellIndex) == 0x04,
               "i386 renderer world-node cell-index extent changed");
_Static_assert(offsetof(mnode_t, data) == 0x0c,
               "i386 renderer world-node role-data offset changed");
_Static_assert(sizeof(((mnode_t *)0)->data) == 0x10,
               "i386 renderer world-node role-data extent changed");
_Static_assert(offsetof(mnode_t, data.node) == 0x0c,
               "i386 renderer internal-node data offset changed");
_Static_assert(sizeof(((mnode_t *)0)->data.node) == 0x0c,
               "i386 renderer internal-node data extent changed");
_Static_assert(offsetof(mnode_t, data.node.plane) == 0x0c,
               "i386 renderer world-node plane offset changed");
_Static_assert(sizeof(((mnode_t *)0)->data.node.plane) == 0x04,
               "i386 renderer world-node plane extent changed");
_Static_assert(offsetof(mnode_t, data.node.children) == 0x10,
               "i386 renderer world-node children offset changed");
_Static_assert(sizeof(((mnode_t *)0)->data.node.children) ==
                   0x08,
               "i386 renderer world-node children extent changed");
_Static_assert(offsetof(mnode_t, data.leaf) == 0x0c,
               "i386 renderer world-leaf data offset changed");
_Static_assert(sizeof(((mnode_t *)0)->data.leaf) == 0x10,
               "i386 renderer world-leaf data extent changed");
_Static_assert(offsetof(mnode_t, data.leaf.cluster) == 0x0c,
               "i386 renderer world-leaf cluster offset changed");
_Static_assert(sizeof(((mnode_t *)0)->data.leaf.cluster) ==
                   0x04,
               "i386 renderer world-leaf cluster extent changed");
_Static_assert(offsetof(mnode_t, data.leaf.hasSunLight) == 0x10,
               "i386 renderer world-leaf sun marker offset changed");
_Static_assert(sizeof(((mnode_t *)0)->data.leaf.hasSunLight) ==
                   0x04,
               "i386 renderer world-leaf sun marker extent changed");
_Static_assert(offsetof(mnode_t, data.leaf.firstLightIndex) ==
                   0x14,
               "i386 renderer world-leaf light-index offset changed");
_Static_assert(sizeof(((mnode_t *)0)
                          ->data.leaf.firstLightIndex) == 0x04,
               "i386 renderer world-leaf light-index extent changed");
_Static_assert(offsetof(mnode_t, data.leaf.lightCount) == 0x18,
               "i386 renderer world-leaf light-count offset changed");
_Static_assert(sizeof(((mnode_t *)0)->data.leaf.lightCount) ==
                   0x04,
               "i386 renderer world-leaf light-count extent changed");
_Static_assert(sizeof(mnode_t) == 0x1c,
               "original i386 renderer world-node size changed");
_Static_assert(_Alignof(renderer_light_vis_cache_entry_t) == 4,
               "i386 light-vis cache-entry alignment changed");
_Static_assert(offsetof(renderer_light_vis_cache_entry_t, key) == 0x00,
               "i386 light-vis cache key moved");
_Static_assert(offsetof(renderer_light_vis_cache_entry_t, sampleState) == 0x04,
               "i386 light-vis cache sample state moved");
_Static_assert(offsetof(renderer_light_vis_cache_entry_t,
                        diffuseSunVisibility) == 0x05,
               "i386 light-vis cache diffuse-sun visibility moved");
_Static_assert(offsetof(renderer_light_vis_cache_entry_t,
                        visibleLightBits) == 0x06,
               "i386 light-vis cache visible-light bits moved");
_Static_assert(sizeof(renderer_light_vis_cache_entry_t) == 0x08,
               "i386 renderer light-visibility entry size changed");
_Static_assert(_Alignof(renderer_light_vis_history_entry_t) == 4,
               "i386 light-vis history-entry alignment changed");
_Static_assert(offsetof(renderer_light_vis_history_entry_t, gridX) == 0x00,
               "i386 light-vis history X coordinate moved");
_Static_assert(offsetof(renderer_light_vis_history_entry_t, gridY) == 0x04,
               "i386 light-vis history Y coordinate moved");
_Static_assert(offsetof(renderer_light_vis_history_entry_t, gridZ) == 0x08,
               "i386 light-vis history Z coordinate moved");
_Static_assert(offsetof(renderer_light_vis_history_entry_t, traceTarget) ==
                   0x0c,
               "i386 light-vis history trace target moved");
_Static_assert(sizeof(renderer_light_vis_history_entry_t) == 0x18,
               "i386 renderer light-visibility history size changed");
_Static_assert(_Alignof(renderer_light_vis_sort_entry_t) == 2,
               "i386 light-vis sorted-entry alignment changed");
_Static_assert(offsetof(renderer_light_vis_sort_entry_t, gridX) == 0x00,
               "i386 light-vis sorted X coordinate moved");
_Static_assert(offsetof(renderer_light_vis_sort_entry_t, gridY) == 0x02,
               "i386 light-vis sorted Y coordinate moved");
_Static_assert(offsetof(renderer_light_vis_sort_entry_t, gridZ) == 0x04,
               "i386 light-vis sorted Z coordinate moved");
_Static_assert(offsetof(renderer_light_vis_sort_entry_t, unconsumed006) ==
                   0x06,
               "i386 light-vis sorted unconsumed lane moved");
_Static_assert(sizeof(renderer_light_vis_sort_entry_t) == 0x08,
               "i386 light-vis sorted-entry size changed");
_Static_assert(_Alignof(renderer_light_vis_disk_entry_t) == 4,
               "i386 light-vis disk-entry alignment changed");
_Static_assert(offsetof(renderer_light_vis_disk_entry_t, key) == 0x00,
               "i386 light-vis disk key moved");
_Static_assert(offsetof(renderer_light_vis_disk_entry_t, sampleState) == 0x04,
               "i386 light-vis disk sample state moved");
_Static_assert(offsetof(renderer_light_vis_disk_entry_t,
                        diffuseSunVisibility) == 0x05,
               "i386 light-vis disk diffuse-sun samples moved");
_Static_assert(offsetof(renderer_light_vis_disk_entry_t,
                        visibleLightBits) == 0x0a,
               "i386 light-vis disk visible-light bits moved");
_Static_assert(sizeof(renderer_light_vis_disk_entry_t) == 0x0c,
               "i386 light-vis disk-entry size changed");
_Static_assert(_Alignof(renderer_static_model_t) == 0x04,
               "i386 static-model alignment changed");
_Static_assert(offsetof(renderer_static_model_t, entity) == 0x000,
               "i386 static-model entity offset changed");
_Static_assert(sizeof(((renderer_static_model_t *)0)->entity) == 0x09c,
               "i386 static-model entity extent changed");
_Static_assert(offsetof(renderer_static_model_t, mins) == 0x09c,
               "i386 static-model minimum-bounds offset changed");
_Static_assert(sizeof(((renderer_static_model_t *)0)->mins) == 0x00c,
               "i386 static-model minimum-bounds extent changed");
_Static_assert(offsetof(renderer_static_model_t, maxs) == 0x0a8,
               "i386 static-model maximum-bounds offset changed");
_Static_assert(sizeof(((renderer_static_model_t *)0)->maxs) == 0x00c,
               "i386 static-model maximum-bounds extent changed");
_Static_assert(offsetof(renderer_static_model_t, viewCount) == 0x0b4,
               "i386 static-model view-count offset changed");
_Static_assert(sizeof(((renderer_static_model_t *)0)->viewCount) == 0x004,
               "i386 static-model view-count extent changed");
_Static_assert(offsetof(renderer_static_model_t, lightCount) == 0x0b8,
               "i386 static-model light-count offset changed");
_Static_assert(sizeof(((renderer_static_model_t *)0)->lightCount) == 0x004,
               "i386 static-model light-count extent changed");
_Static_assert(offsetof(renderer_static_model_t,
                        diffuseSunContribution) == 0x0bc,
               "i386 static-model diffuse-sun offset changed");
_Static_assert(sizeof(((renderer_static_model_t *)0)
                          ->diffuseSunContribution) == 0x004,
               "i386 static-model diffuse-sun extent changed");
_Static_assert(offsetof(renderer_static_model_t, contributions) == 0x0c0,
               "i386 static-model contribution offset changed");
_Static_assert(sizeof(((renderer_static_model_t *)0)->contributions) ==
                   0x0c4,
               "i386 static-model contribution-array extent changed");
_Static_assert(offsetof(renderer_static_model_t, lights) == 0x184,
               "i386 static-model light-pointer offset changed");
_Static_assert(sizeof(((renderer_static_model_t *)0)->lights) == 0x0c4,
               "i386 static-model light-pointer-array extent changed");
_Static_assert(offsetof(renderer_static_model_t,
                        surfaceLightingCache) == 0x248,
               "i386 static-model surface-cache offset changed");
_Static_assert(sizeof(renderer_static_model_t) == 0x248,
               "original i386 static-model header size changed");
_Static_assert(_Alignof(renderer_static_model_cache_link_t) == 0x04,
               "renderer_static_model_cache_link_t original alignment");
_Static_assert(offsetof(renderer_static_model_cache_link_t, next) == 0x00,
               "renderer_static_model_cache_link_t next offset");
_Static_assert(sizeof(((renderer_static_model_cache_link_t *)0)->next) == 0x04,
               "renderer_static_model_cache_link_t next extent");
_Static_assert(offsetof(renderer_static_model_cache_link_t, previous) == 0x04,
               "renderer_static_model_cache_link_t previous offset");
_Static_assert(sizeof(((renderer_static_model_cache_link_t *)0)->previous) ==
                   0x04,
               "renderer_static_model_cache_link_t previous extent");
_Static_assert(sizeof(renderer_static_model_cache_link_t) == 0x08,
               "renderer_static_model_cache_link_t original size");
_Static_assert(_Alignof(renderer_cached_static_model_surface_t) == 0x04,
               "renderer_cached_static_model_surface_t original alignment");
_Static_assert(offsetof(renderer_cached_static_model_surface_t, freeLink) ==
                   0x00,
               "renderer_cached_static_model_surface_t freeLink offset");
_Static_assert(sizeof(((renderer_cached_static_model_surface_t *)0)->freeLink) ==
                   0x08,
               "renderer_cached_static_model_surface_t freeLink extent");
_Static_assert(offsetof(renderer_cached_static_model_surface_t,
                        cached.surfaceType) == 0x00,
               "renderer_cached_static_model_surface_t surfaceType offset");
_Static_assert(sizeof(((renderer_cached_static_model_surface_t *)0)
                          ->cached.surfaceType) == 0x04,
               "renderer_cached_static_model_surface_t surfaceType extent");
_Static_assert(offsetof(renderer_cached_static_model_surface_t,
                        cached.source) == 0x04,
               "renderer_cached_static_model_surface_t source offset");
_Static_assert(sizeof(((renderer_cached_static_model_surface_t *)0)
                          ->cached.source) == 0x04,
               "renderer_cached_static_model_surface_t source extent");
_Static_assert(offsetof(renderer_cached_static_model_surface_t,
                        cached.vertexOffset) == 0x08,
               "renderer_cached_static_model_surface_t vertexOffset offset");
_Static_assert(sizeof(((renderer_cached_static_model_surface_t *)0)
                          ->cached.vertexOffset) == 0x04,
               "renderer_cached_static_model_surface_t vertexOffset extent");
_Static_assert(offsetof(renderer_cached_static_model_surface_t,
                        cached.surfaceIndex) == 0x0c,
               "renderer_cached_static_model_surface_t surfaceIndex offset");
_Static_assert(sizeof(((renderer_cached_static_model_surface_t *)0)
                          ->cached.surfaceIndex) == 0x04,
               "renderer_cached_static_model_surface_t surfaceIndex extent");
_Static_assert(offsetof(renderer_cached_static_model_surface_t,
                        cached.owner) == 0x10,
               "renderer_cached_static_model_surface_t owner offset");
_Static_assert(sizeof(((renderer_cached_static_model_surface_t *)0)
                          ->cached.owner) == 0x04,
               "renderer_cached_static_model_surface_t owner extent");
_Static_assert(sizeof(((renderer_cached_static_model_surface_t *)0)->cached) ==
                   0x14,
               "renderer_cached_static_model_surface_t cached extent");
_Static_assert(sizeof(renderer_cached_static_model_surface_t) == 0x14,
               "renderer_cached_static_model_surface_t original size");
_Static_assert(_Alignof(renderer_cached_static_model_vertex_t) == 0x04,
               "renderer_cached_static_model_vertex_t original alignment");
_Static_assert(offsetof(renderer_cached_static_model_vertex_t, texCoord) ==
                   0x00,
               "renderer_cached_static_model_vertex_t texCoord offset");
_Static_assert(sizeof(((renderer_cached_static_model_vertex_t *)0)->texCoord) ==
                   0x08,
               "renderer_cached_static_model_vertex_t texCoord extent");
_Static_assert(offsetof(renderer_cached_static_model_vertex_t, color) == 0x08,
               "renderer_cached_static_model_vertex_t color offset");
_Static_assert(sizeof(((renderer_cached_static_model_vertex_t *)0)->color) ==
                   0x04,
               "renderer_cached_static_model_vertex_t color extent");
_Static_assert(offsetof(renderer_cached_static_model_vertex_t, position) ==
                   0x0c,
               "renderer_cached_static_model_vertex_t position offset");
_Static_assert(sizeof(((renderer_cached_static_model_vertex_t *)0)->position) ==
                   0x0c,
               "renderer_cached_static_model_vertex_t position extent");
_Static_assert(sizeof(renderer_cached_static_model_vertex_t) == 0x18,
               "renderer_cached_static_model_vertex_t original size");
_Static_assert(_Alignof(bmodel_t) == 0x04,
               "i386 renderer bmodel alignment changed");
_Static_assert(offsetof(bmodel_t, bounds) == 0x00,
               "i386 renderer bmodel minimum-bounds offset changed");
_Static_assert(sizeof(((bmodel_t *)0)->bounds) == 0x18,
               "i386 renderer bmodel bounds extent changed");
_Static_assert(offsetof(bmodel_t, firstSurface) == 0x18,
               "i386 renderer bmodel surface-pointer offset changed");
_Static_assert(sizeof(((bmodel_t *)0)->firstSurface) == 0x04,
               "i386 renderer bmodel surface-pointer extent changed");
_Static_assert(offsetof(bmodel_t, numSurfaces) == 0x1c,
               "i386 renderer bmodel surface-count offset changed");
_Static_assert(sizeof(((bmodel_t *)0)->numSurfaces) == 0x04,
               "i386 renderer bmodel surface-count extent changed");
_Static_assert(sizeof(bmodel_t) == 0x20,
               "original i386 renderer bmodel size changed");
_Static_assert(_Alignof(renderer_surface_t) == 0x04,
               "i386 renderer surface-prefix alignment changed");
_Static_assert(offsetof(renderer_surface_t, surfaceType) == 0x00,
               "i386 renderer surface-prefix type offset changed");
_Static_assert(sizeof(((renderer_surface_t *)0)->surfaceType) == 0x04,
               "i386 renderer surface-prefix type extent changed");
_Static_assert(sizeof(renderer_surface_t) == 0x04,
               "original i386 renderer surface-prefix size changed");

_Static_assert(_Alignof(msurface_t) == 0x04,
               "i386 renderer world-surface alignment changed");
_Static_assert(offsetof(msurface_t, viewCount) == 0x00,
               "i386 renderer world-surface view-count offset changed");
_Static_assert(sizeof(((msurface_t *)0)->viewCount) == 0x04,
               "i386 renderer world-surface view-count extent changed");
_Static_assert(offsetof(msurface_t, shader) == 0x04,
               "i386 renderer world-surface shader offset changed");
_Static_assert(sizeof(((msurface_t *)0)->shader) == 0x04,
               "i386 renderer world-surface shader-pointer extent changed");
_Static_assert(offsetof(msurface_t, data) == 0x08,
               "i386 renderer world-surface data offset changed");
_Static_assert(sizeof(((msurface_t *)0)->data) == 0x04,
               "i386 renderer world-surface data-pointer extent changed");
_Static_assert(sizeof(msurface_t) == 0x0c,
               "original i386 renderer world-surface size changed");
_Static_assert(_Alignof(renderer_entity_surface_t) == 0x04,
               "i386 renderer entity-surface alignment changed");
_Static_assert(offsetof(renderer_entity_surface_t, base) == 0x00,
               "i386 renderer entity-surface base offset changed");
_Static_assert(sizeof(((renderer_entity_surface_t *)0)->base) == 0x04,
               "i386 renderer entity-surface base extent changed");
_Static_assert(offsetof(renderer_entity_surface_t, obj) == 0x04,
               "i386 renderer entity-surface object offset changed");
_Static_assert(sizeof(((renderer_entity_surface_t *)0)->obj) == 0x04,
               "i386 renderer entity-surface object-pointer extent changed");
_Static_assert(offsetof(renderer_entity_surface_t, surface) == 0x08,
               "i386 renderer entity-surface payload offset changed");
_Static_assert(sizeof(((renderer_entity_surface_t *)0)->surface) == 0x04,
               "i386 renderer entity-surface payload-pointer extent changed");
_Static_assert(offsetof(renderer_entity_surface_t, modelIndex) == 0x0c,
               "i386 renderer entity-surface model-index offset changed");
_Static_assert(sizeof(((renderer_entity_surface_t *)0)->modelIndex) == 0x04,
               "i386 renderer entity-surface model-index extent changed");
_Static_assert(sizeof(renderer_entity_surface_t) == 0x10,
               "original i386 renderer entity-surface size changed");
_Static_assert(_Alignof(shader_t) == 0x04,
               "i386 shader alignment changed");
_Static_assert(offsetof(shader_t, name) == 0x000,
               "i386 shader name offset changed");
_Static_assert(sizeof(((shader_t *)0)->name) == 0x40,
               "i386 shader name extent changed");
_Static_assert(offsetof(shader_t, lightmapIndex) == 0x040,
               "i386 shader lightmap-mode offset changed");
_Static_assert(sizeof(((shader_t *)0)->lightmapIndex) == 0x04,
               "i386 shader lightmap-mode extent changed");
_Static_assert(offsetof(shader_t, index) == 0x044,
               "i386 shader index offset changed");
_Static_assert(sizeof(((shader_t *)0)->index) == 0x04,
               "i386 shader index extent changed");
_Static_assert(offsetof(shader_t, sortedIndex) == 0x048,
               "i386 shader sorted-index offset changed");
_Static_assert(sizeof(((shader_t *)0)->sortedIndex) == 0x04,
               "i386 shader sorted-index extent changed");
_Static_assert(offsetof(shader_t, flags) == 0x04c,
               "i386 shader flags offset changed");
_Static_assert(sizeof(((shader_t *)0)->flags) == 0x04,
               "i386 shader flags extent changed");
_Static_assert(offsetof(shader_t, surfaceFlags) == 0x050,
               "i386 shader surface-flags offset changed");
_Static_assert(sizeof(((shader_t *)0)->surfaceFlags) == 0x04,
               "i386 shader surface-flags extent changed");
_Static_assert(offsetof(shader_t, lightingFlags) == 0x054,
               "i386 shader lighting-flags offset changed");
_Static_assert(sizeof(((shader_t *)0)->lightingFlags) == 0x04,
               "i386 shader lighting-flags extent changed");
_Static_assert(offsetof(shader_t, sort) == 0x058,
               "i386 shader sort offset changed");
_Static_assert(sizeof(((shader_t *)0)->sort) == 0x04,
               "i386 shader sort extent changed");
_Static_assert(offsetof(shader_t, surfaceParmFlags) == 0x05c,
               "i386 shader surface-parm flags offset changed");
_Static_assert(sizeof(((shader_t *)0)->surfaceParmFlags) == 0x04,
               "i386 shader surface-parm flags extent changed");
_Static_assert(offsetof(shader_t, skyCloudHeight) == 0x060,
               "i386 shader sky cloud-height offset changed");
_Static_assert(sizeof(((shader_t *)0)->skyCloudHeight) == 0x04,
               "i386 shader sky cloud-height extent changed");
_Static_assert(offsetof(shader_t, skyOuterBox) == 0x064,
               "i386 shader outer-skybox offset changed");
_Static_assert(sizeof(((shader_t *)0)->skyOuterBox) == 0x18,
               "i386 shader outer-skybox extent changed");
_Static_assert(offsetof(shader_t, skyInnerBox) == 0x07c,
               "i386 shader inner-skybox offset changed");
_Static_assert(sizeof(((shader_t *)0)->skyInnerBox) == 0x18,
               "i386 shader inner-skybox extent changed");
_Static_assert(offsetof(shader_t, fogColor) == 0x094,
               "i386 shader fog-color offset changed");
_Static_assert(sizeof(((shader_t *)0)->fogColor) == 0x0c,
               "i386 shader fog-color extent changed");
_Static_assert(offsetof(shader_t, fogDepthForOpaque) == 0x0a0,
               "i386 shader opaque-fog-depth offset changed");
_Static_assert(sizeof(((shader_t *)0)->fogDepthForOpaque) == 0x04,
               "i386 shader opaque-fog-depth extent changed");
_Static_assert(offsetof(shader_t, portalRange) == 0x0a4,
               "i386 shader portal-range offset changed");
_Static_assert(sizeof(((shader_t *)0)->portalRange) == 0x04,
               "i386 shader portal-range extent changed");
_Static_assert(offsetof(shader_t, cullType) == 0x0a8,
               "i386 shader cull-type offset changed");
_Static_assert(sizeof(((shader_t *)0)->cullType) == 0x04,
               "i386 shader cull-type extent changed");
_Static_assert(offsetof(shader_t, numDeforms) == 0x0ac,
               "i386 shader deform-count offset changed");
_Static_assert(sizeof(((shader_t *)0)->numDeforms) == 0x04,
               "i386 shader deform-count extent changed");
_Static_assert(offsetof(shader_t, deforms) == 0x0b0,
               "i386 shader deform-array offset changed");
_Static_assert(sizeof(((shader_t *)0)->deforms) == 0x9c,
               "i386 shader deform-array extent changed");
_Static_assert(offsetof(shader_t, boundsExpansion) == 0x14c,
               "i386 shader bounds-expansion offset changed");
_Static_assert(sizeof(((shader_t *)0)->boundsExpansion) == 0x04,
               "i386 shader bounds-expansion extent changed");
_Static_assert(offsetof(shader_t, numUnfoggedPasses) == 0x150,
               "i386 shader pass-count offset changed");
_Static_assert(sizeof(((shader_t *)0)->numUnfoggedPasses) == 0x04,
               "i386 shader pass-count extent changed");
_Static_assert(offsetof(shader_t, stages) == 0x154,
               "i386 shader stage-array offset changed");
_Static_assert(sizeof(((shader_t *)0)->stages) == 0x20,
               "i386 shader stage-array extent changed");
_Static_assert(offsetof(shader_t, optimalStageIteratorFunc) == 0x174,
               "i386 shader stage-iterator offset changed");
_Static_assert(sizeof(((shader_t *)0)->optimalStageIteratorFunc) == 0x04,
               "i386 shader stage-iterator extent changed");
_Static_assert(offsetof(shader_t, clampTime) == 0x178,
               "i386 shader clamp-time offset changed");
_Static_assert(sizeof(((shader_t *)0)->clampTime) == 0x04,
               "i386 shader clamp-time extent changed");
_Static_assert(offsetof(shader_t, timeOffset) == 0x17c,
               "i386 shader time-offset offset changed");
_Static_assert(sizeof(((shader_t *)0)->timeOffset) == 0x04,
               "i386 shader time-offset extent changed");
_Static_assert(offsetof(shader_t, remappedShader) == 0x180,
               "i386 shader remapped-shader offset changed");
_Static_assert(sizeof(((shader_t *)0)->remappedShader) == 0x04,
               "i386 shader remapped-shader extent changed");
_Static_assert(offsetof(shader_t, primaryImage) == 0x184,
               "i386 shader primary-image offset changed");
_Static_assert(sizeof(((shader_t *)0)->primaryImage) == 0x04,
               "i386 shader primary-image extent changed");
_Static_assert(offsetof(shader_t, optimizedBackend) == 0x188,
               "i386 shader optimized-backend offset changed");
_Static_assert(sizeof(((shader_t *)0)->optimizedBackend) == 0x04,
               "i386 shader optimized-backend extent changed");
_Static_assert(offsetof(shader_t, optimizedVertexStorage) == 0x18c,
               "i386 shader optimized vertex-storage offset changed");
_Static_assert(sizeof(((shader_t *)0)->optimizedVertexStorage) == 0x04,
               "i386 shader optimized vertex-storage extent changed");
_Static_assert(offsetof(shader_t, optimizedVertexStorageOffset) == 0x190,
               "i386 shader optimized vertex-storage-base offset changed");
_Static_assert(sizeof(((shader_t *)0)->optimizedVertexStorageOffset) == 0x04,
               "i386 shader optimized vertex-storage-base extent changed");
_Static_assert(offsetof(shader_t, next) == 0x194,
               "i386 shader hash-link offset changed");
_Static_assert(sizeof(((shader_t *)0)->next) == 0x04,
               "i386 shader hash-link extent changed");
_Static_assert(sizeof(shader_t) == 0x198,
               "original i386 shader size changed");
_Static_assert(_Alignof(deformStage_t) == 0x04,
               "i386 shader-deform alignment changed");
_Static_assert(offsetof(deformStage_t, deformation) == 0x00,
               "i386 shader-deform type offset changed");
_Static_assert(sizeof(((deformStage_t *)0)->deformation) == 0x04,
               "i386 shader-deform type extent changed");
_Static_assert(offsetof(deformStage_t, moveVector) == 0x04,
               "i386 shader-deform move-vector offset changed");
_Static_assert(sizeof(((deformStage_t *)0)->moveVector) == 0x0c,
               "i386 shader-deform move-vector extent changed");
_Static_assert(offsetof(deformStage_t, deformationWave) == 0x10,
               "i386 shader-deform waveform offset changed");
_Static_assert(sizeof(((deformStage_t *)0)->deformationWave) == 0x14,
               "i386 shader-deform waveform extent changed");
_Static_assert(offsetof(deformStage_t, deformationSpread) == 0x24,
               "i386 shader-deform spread offset changed");
_Static_assert(sizeof(((deformStage_t *)0)->deformationSpread) == 0x04,
               "i386 shader-deform spread extent changed");
_Static_assert(offsetof(deformStage_t, bulgeWidth) == 0x28,
               "i386 shader-deform bulge-width offset changed");
_Static_assert(sizeof(((deformStage_t *)0)->bulgeWidth) == 0x04,
               "i386 shader-deform bulge-width extent changed");
_Static_assert(offsetof(deformStage_t, bulgeHeight) == 0x2c,
               "i386 shader-deform bulge-height offset changed");
_Static_assert(sizeof(((deformStage_t *)0)->bulgeHeight) == 0x04,
               "i386 shader-deform bulge-height extent changed");
_Static_assert(offsetof(deformStage_t, bulgeSpeed) == 0x30,
               "i386 shader-deform bulge-speed offset changed");
_Static_assert(sizeof(((deformStage_t *)0)->bulgeSpeed) == 0x04,
               "i386 shader-deform bulge-speed extent changed");
_Static_assert(sizeof(deformStage_t) == 0x34,
               "original i386 shader-deform size changed");
_Static_assert(_Alignof(shaderStage_t) == 0x04,
               "i386 shader-stage alignment changed");
_Static_assert(offsetof(shaderStage_t, flags) == 0x000,
               "i386 shader-stage flags offset changed");
_Static_assert(sizeof(((shaderStage_t *)0)->flags) == 0x04,
               "i386 shader-stage flags extent changed");
_Static_assert(offsetof(shaderStage_t, bundle) == 0x004,
               "i386 shader-stage bundle offset changed");
_Static_assert(sizeof(((shaderStage_t *)0)->bundle) == 0x640,
               "i386 shader-stage bundle extent changed");
_Static_assert(offsetof(shaderStage_t, fragmentShaderATI) == 0x644,
               "i386 shader-stage ATI fragment-shader offset changed");
_Static_assert(sizeof(((shaderStage_t *)0)->fragmentShaderATI) == 0x04,
               "i386 shader-stage ATI fragment-shader extent changed");
_Static_assert(offsetof(shaderStage_t, registerCombiners) == 0x648,
               "i386 shader-stage register-combiner offset changed");
_Static_assert(sizeof(((shaderStage_t *)0)->registerCombiners) == 0x04,
               "i386 shader-stage register-combiner extent changed");
_Static_assert(offsetof(shaderStage_t, vertexProgram) == 0x64c,
               "i386 shader-stage vertex-program offset changed");
_Static_assert(sizeof(((shaderStage_t *)0)->vertexProgram) == 0x04,
               "i386 shader-stage vertex-program extent changed");
_Static_assert(offsetof(shaderStage_t, rgbWave) == 0x650,
               "i386 shader-stage color-waveform offset changed");
_Static_assert(sizeof(((shaderStage_t *)0)->rgbWave) == 0x14,
               "i386 shader-stage color-waveform extent changed");
_Static_assert(offsetof(shaderStage_t, rgbGen) == 0x664,
               "i386 shader-stage color-generator offset changed");
_Static_assert(sizeof(((shaderStage_t *)0)->rgbGen) == 0x04,
               "i386 shader-stage color-generator extent changed");
_Static_assert(offsetof(shaderStage_t, alphaWave) == 0x668,
               "i386 shader-stage alpha-waveform offset changed");
_Static_assert(sizeof(((shaderStage_t *)0)->alphaWave) == 0x14,
               "i386 shader-stage alpha-waveform extent changed");
_Static_assert(offsetof(shaderStage_t, alphaGen) == 0x67c,
               "i386 shader-stage alpha-generator offset changed");
_Static_assert(sizeof(((shaderStage_t *)0)->alphaGen) == 0x04,
               "i386 shader-stage alpha-generator extent changed");
_Static_assert(offsetof(shaderStage_t, constantColor) == 0x680,
               "i386 shader-stage constant-color offset changed");
_Static_assert(sizeof(((shaderStage_t *)0)->constantColor) == 0x04,
               "i386 shader-stage constant-color extent changed");
_Static_assert(offsetof(shaderStage_t, stateBits) == 0x684,
               "i386 shader-stage state-bits offset changed");
_Static_assert(sizeof(((shaderStage_t *)0)->stateBits) == 0x04,
               "i386 shader-stage state-bits extent changed");
_Static_assert(sizeof(shaderStage_t) == 0x688,
               "i386 shader-stage size changed");
_Static_assert(_Alignof(textureBundle_t) == 0x04,
               "i386 shader texture-bundle alignment changed");
_Static_assert(offsetof(textureBundle_t, image) == 0x00,
               "i386 texture-bundle image-array offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->image) == 0x80,
               "i386 texture-bundle image-array extent changed");
_Static_assert(offsetof(textureBundle_t, numImageAnimations) == 0x80,
               "i386 texture-bundle animation-count offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->numImageAnimations) ==
                   0x04,
               "i386 texture-bundle animation-count extent changed");
_Static_assert(offsetof(textureBundle_t, imageAnimationSpeed) == 0x84,
               "i386 texture-bundle animation-speed offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->imageAnimationSpeed) ==
                   0x04,
               "i386 texture-bundle animation-speed extent changed");
_Static_assert(offsetof(textureBundle_t, waterMap) == 0x88,
               "i386 texture-bundle water-map offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->waterMap) == 0x04,
               "i386 texture-bundle water-map pointer extent changed");
_Static_assert(offsetof(textureBundle_t, texCoordComponentCount) ==
                   0x8c,
               "i386 texture-bundle coordinate-count offset changed");
_Static_assert(
    sizeof(((textureBundle_t *)0)->texCoordComponentCount) == 0x04,
    "i386 texture-bundle coordinate-count extent changed");
_Static_assert(offsetof(textureBundle_t, textureEnvMode) == 0x90,
               "i386 texture-bundle environment-mode offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->textureEnvMode) == 0x04,
               "i386 texture-bundle environment-mode extent changed");
_Static_assert(offsetof(textureBundle_t, textureCombine) == 0x94,
               "i386 texture-combine pointer offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->textureCombine) == 0x04,
               "i386 texture-combine pointer extent changed");
_Static_assert(offsetof(textureBundle_t, textureShader) == 0x98,
               "i386 texture-shader pointer offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->textureShader) == 0x04,
               "i386 texture-shader pointer extent changed");
_Static_assert(offsetof(textureBundle_t, tcGen) == 0x9c,
               "i386 texture-coordinate generator offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->tcGen) == 0x04,
               "i386 texture-coordinate generator extent changed");
_Static_assert(offsetof(textureBundle_t, tcGenVectors) == 0xa0,
               "i386 texture-coordinate vectors offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->tcGenVectors) ==
                   0x18,
               "i386 texture-coordinate vectors extent changed");
_Static_assert(offsetof(textureBundle_t, numTexMods) == 0xb8,
               "i386 texture-modifier count offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->numTexMods) == 0x04,
               "i386 texture-modifier count extent changed");
_Static_assert(offsetof(textureBundle_t, texMods) == 0xbc,
               "i386 texture-modifier pointer offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->texMods) == 0x04,
               "i386 texture-modifier pointer extent changed");
_Static_assert(offsetof(textureBundle_t, videoMapHandle) == 0xc0,
               "i386 texture-bundle cinematic-handle offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->videoMapHandle) == 0x04,
               "i386 texture-bundle cinematic-handle extent changed");
_Static_assert(offsetof(textureBundle_t, isLightmap) == 0xc4,
               "i386 texture-bundle lightmap flag offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->isLightmap) == 0x01,
               "i386 texture-bundle lightmap flag extent changed");
_Static_assert(offsetof(textureBundle_t, isVideoMap) == 0xc5,
               "i386 texture-bundle video-map flag offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->isVideoMap) == 0x01,
               "i386 texture-bundle video-map flag extent changed");
_Static_assert(offsetof(textureBundle_t, clampAnimation) == 0xc6,
               "i386 texture-bundle animation-clamp flag offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->clampAnimation) == 0x01,
               "i386 texture-bundle animation-clamp flag extent changed");
_Static_assert(offsetof(textureBundle_t, padding0c7) == 0xc7,
               "i386 texture-bundle tail-padding offset changed");
_Static_assert(sizeof(((textureBundle_t *)0)->padding0c7) == 0x01,
               "i386 texture-bundle tail-padding extent changed");
_Static_assert(sizeof(textureBundle_t) == 0xc8,
               "original i386 shader texture-bundle size changed");
_Static_assert(_Alignof(waveForm_t) == 0x04,
               "i386 shader waveform alignment changed");
_Static_assert(offsetof(waveForm_t, func) == 0x00,
               "i386 shader waveform function offset changed");
_Static_assert(sizeof(((waveForm_t *)0)->func) == 0x04,
               "i386 shader waveform function extent changed");
_Static_assert(offsetof(waveForm_t, base) == 0x04,
               "i386 shader waveform base offset changed");
_Static_assert(sizeof(((waveForm_t *)0)->base) == 0x04,
               "i386 shader waveform base extent changed");
_Static_assert(offsetof(waveForm_t, amplitude) == 0x08,
               "i386 shader waveform amplitude offset changed");
_Static_assert(sizeof(((waveForm_t *)0)->amplitude) == 0x04,
               "i386 shader waveform amplitude extent changed");
_Static_assert(offsetof(waveForm_t, phase) == 0x0c,
               "i386 shader waveform phase offset changed");
_Static_assert(sizeof(((waveForm_t *)0)->phase) == 0x04,
               "i386 shader waveform phase extent changed");
_Static_assert(offsetof(waveForm_t, frequency) == 0x10,
               "i386 shader waveform frequency offset changed");
_Static_assert(sizeof(((waveForm_t *)0)->frequency) == 0x04,
               "i386 shader waveform frequency extent changed");
_Static_assert(sizeof(waveForm_t) == 0x14,
               "original i386 shader waveform size changed");
_Static_assert(_Alignof(texModInfo_t) == 0x04,
               "i386 shader texture-modifier alignment changed");
_Static_assert(offsetof(texModInfo_t, type) == 0x00,
               "i386 shader texture-modifier type offset changed");
_Static_assert(sizeof(((texModInfo_t *)0)->type) == 0x04,
               "i386 shader texture-modifier type extent changed");
_Static_assert(offsetof(texModInfo_t, wave) == 0x04,
               "i386 shader texture-modifier waveform offset changed");
_Static_assert(sizeof(((texModInfo_t *)0)->wave) == 0x14,
               "i386 shader texture-modifier waveform extent changed");
_Static_assert(offsetof(texModInfo_t, matrix) == 0x18,
               "i386 shader texture-modifier matrix offset changed");
_Static_assert(sizeof(((texModInfo_t *)0)->matrix) == 0x10,
               "i386 shader texture-modifier matrix extent changed");
_Static_assert(offsetof(texModInfo_t, translate) == 0x28,
               "i386 shader texture-modifier translation offset changed");
_Static_assert(sizeof(((texModInfo_t *)0)->translate) == 0x08,
               "i386 shader texture-modifier translation extent changed");
_Static_assert(offsetof(texModInfo_t, scale) == 0x30,
               "i386 shader texture-modifier scale offset changed");
_Static_assert(sizeof(((texModInfo_t *)0)->scale) == 0x08,
               "i386 shader texture-modifier scale extent changed");
_Static_assert(offsetof(texModInfo_t, scroll) == 0x38,
               "i386 shader texture-modifier scroll offset changed");
_Static_assert(sizeof(((texModInfo_t *)0)->scroll) == 0x08,
               "i386 shader texture-modifier scroll extent changed");
_Static_assert(offsetof(texModInfo_t, rotateSpeed) == 0x40,
               "i386 shader texture-modifier rotation-speed offset changed");
_Static_assert(sizeof(((texModInfo_t *)0)->rotateSpeed) == 0x04,
               "i386 shader texture-modifier rotation-speed extent changed");
_Static_assert(sizeof(texModInfo_t) == 0x44,
               "original i386 shader texture-modifier size changed");
_Static_assert(_Alignof(renderer_combiner_input_t) == 0x04,
               "i386 register-combiner input alignment changed");
_Static_assert(offsetof(renderer_combiner_input_t, input) == 0x00,
               "i386 register-combiner input register offset changed");
_Static_assert(sizeof(((renderer_combiner_input_t *)0)->input) == 0x04,
               "i386 register-combiner input register extent changed");
_Static_assert(offsetof(renderer_combiner_input_t, mapping) == 0x04,
               "i386 register-combiner input mapping offset changed");
_Static_assert(sizeof(((renderer_combiner_input_t *)0)->mapping) == 0x04,
               "i386 register-combiner input mapping extent changed");
_Static_assert(offsetof(renderer_combiner_input_t, componentUsage) == 0x08,
               "i386 register-combiner component-usage offset changed");
_Static_assert(sizeof(((renderer_combiner_input_t *)0)->componentUsage) ==
                   0x04,
               "i386 register-combiner component-usage extent changed");
_Static_assert(sizeof(renderer_combiner_input_t) == 0x0c,
               "original i386 register-combiner input size changed");
_Static_assert(_Alignof(renderer_nv_register_definition_t) == 4,
               "i386 NV register-definition alignment changed");
_Static_assert(offsetof(renderer_nv_register_definition_t, token) == 0x00,
               "i386 NV register-definition token moved");
_Static_assert(offsetof(renderer_nv_register_definition_t, glRegister) ==
                   0x04,
               "i386 NV register-definition GL register moved");
_Static_assert(offsetof(renderer_nv_register_definition_t, componentUsage) ==
                   0x08,
               "i386 NV register-definition component usage moved");
_Static_assert(sizeof(renderer_nv_register_definition_t) == 0x0c,
               "i386 NV register-definition size changed");
_Static_assert(_Alignof(renderer_combiner_output_t) == 0x04,
               "i386 register-combiner output alignment changed");
_Static_assert(offsetof(renderer_combiner_output_t, abOutput) == 0x00,
               "i386 register-combiner AB-output offset changed");
_Static_assert(sizeof(((renderer_combiner_output_t *)0)->abOutput) == 0x04,
               "i386 register-combiner AB-output extent changed");
_Static_assert(offsetof(renderer_combiner_output_t, cdOutput) == 0x04,
               "i386 register-combiner CD-output offset changed");
_Static_assert(sizeof(((renderer_combiner_output_t *)0)->cdOutput) == 0x04,
               "i386 register-combiner CD-output extent changed");
_Static_assert(offsetof(renderer_combiner_output_t, sumOutput) == 0x08,
               "i386 register-combiner sum-output offset changed");
_Static_assert(sizeof(((renderer_combiner_output_t *)0)->sumOutput) == 0x04,
               "i386 register-combiner sum-output extent changed");
_Static_assert(offsetof(renderer_combiner_output_t, scale) == 0x0c,
               "i386 register-combiner scale offset changed");
_Static_assert(sizeof(((renderer_combiner_output_t *)0)->scale) == 0x04,
               "i386 register-combiner scale extent changed");
_Static_assert(offsetof(renderer_combiner_output_t, bias) == 0x10,
               "i386 register-combiner bias offset changed");
_Static_assert(sizeof(((renderer_combiner_output_t *)0)->bias) == 0x04,
               "i386 register-combiner bias extent changed");
_Static_assert(offsetof(renderer_combiner_output_t, abDotProduct) == 0x14,
               "i386 register-combiner AB-dot flag offset changed");
_Static_assert(sizeof(((renderer_combiner_output_t *)0)->abDotProduct) ==
                   0x01,
               "i386 register-combiner AB-dot flag extent changed");
_Static_assert(offsetof(renderer_combiner_output_t, cdDotProduct) == 0x15,
               "i386 register-combiner CD-dot flag offset changed");
_Static_assert(sizeof(((renderer_combiner_output_t *)0)->cdDotProduct) ==
                   0x01,
               "i386 register-combiner CD-dot flag extent changed");
_Static_assert(offsetof(renderer_combiner_output_t, muxSum) == 0x16,
               "i386 register-combiner mux flag offset changed");
_Static_assert(sizeof(((renderer_combiner_output_t *)0)->muxSum) == 0x01,
               "i386 register-combiner mux flag extent changed");
_Static_assert(sizeof(renderer_combiner_output_t) == 0x18,
               "original i386 register-combiner output size changed");
_Static_assert(_Alignof(renderer_combiner_portion_t) == 0x04,
               "i386 register-combiner portion alignment changed");
_Static_assert(offsetof(renderer_combiner_portion_t, inputs) == 0x00,
               "i386 register-combiner input-array offset changed");
_Static_assert(sizeof(((renderer_combiner_portion_t *)0)->inputs) == 0x30,
               "i386 register-combiner input-array extent changed");
_Static_assert(offsetof(renderer_combiner_portion_t, output) == 0x30,
               "i386 register-combiner output offset changed");
_Static_assert(sizeof(((renderer_combiner_portion_t *)0)->output) == 0x18,
               "i386 register-combiner output extent changed");
_Static_assert(sizeof(renderer_combiner_portion_t) == 0x48,
               "original i386 register-combiner portion size changed");
_Static_assert(_Alignof(renderer_general_combiner_t) == 0x04,
               "i386 general-combiner alignment changed");
_Static_assert(offsetof(renderer_general_combiner_t, constantColors) == 0x00,
               "i386 general-combiner constants offset changed");
_Static_assert(sizeof(((renderer_general_combiner_t *)0)->constantColors) ==
                   0x20,
               "i386 general-combiner constants extent changed");
_Static_assert(offsetof(renderer_general_combiner_t, rgb) == 0x20,
               "i386 general-combiner RGB portion offset changed");
_Static_assert(sizeof(((renderer_general_combiner_t *)0)->rgb) == 0x48,
               "i386 general-combiner RGB portion extent changed");
_Static_assert(offsetof(renderer_general_combiner_t, alpha) == 0x68,
               "i386 general-combiner alpha portion offset changed");
_Static_assert(sizeof(((renderer_general_combiner_t *)0)->alpha) == 0x48,
               "i386 general-combiner alpha portion extent changed");
_Static_assert(sizeof(renderer_general_combiner_t) == 0xb0,
               "original i386 general-combiner size changed");
_Static_assert(_Alignof(renderer_final_combiner_t) == 0x04,
               "i386 final-combiner alignment changed");
_Static_assert(offsetof(renderer_final_combiner_t, inputs) == 0x00,
               "i386 final-combiner input-array offset changed");
_Static_assert(sizeof(((renderer_final_combiner_t *)0)->inputs) == 0x54,
               "i386 final-combiner input-array extent changed");
_Static_assert(offsetof(renderer_final_combiner_t, clampColorSum) == 0x54,
               "i386 final-combiner clamp flag offset changed");
_Static_assert(sizeof(((renderer_final_combiner_t *)0)->clampColorSum) ==
                   0x01,
               "i386 final-combiner clamp flag extent changed");
_Static_assert(sizeof(renderer_final_combiner_t) == 0x58,
               "original i386 final-combiner size changed");
_Static_assert(_Alignof(renderer_register_combiners_t) == 0x04,
               "i386 register-combiner description alignment changed");
_Static_assert(offsetof(renderer_register_combiners_t, constantColors) ==
                   0x000,
               "i386 register-combiner global constants offset changed");
_Static_assert(sizeof(((renderer_register_combiners_t *)0)->constantColors) ==
                   0x020,
               "i386 register-combiner global constants extent changed");
_Static_assert(offsetof(renderer_register_combiners_t, general) == 0x020,
               "i386 general-combiner array offset changed");
_Static_assert(sizeof(((renderer_register_combiners_t *)0)->general) == 0x580,
               "i386 general-combiner array extent changed");
_Static_assert(offsetof(renderer_register_combiners_t, final) == 0x5a0,
               "i386 final-combiner input offset changed");
_Static_assert(sizeof(((renderer_register_combiners_t *)0)->final) == 0x58,
               "i386 final-combiner extent changed");
_Static_assert(offsetof(renderer_register_combiners_t, generalCombinerCount) ==
                   0x5f8,
               "i386 general-combiner count offset changed");
_Static_assert(sizeof(((renderer_register_combiners_t *)0)
                          ->generalCombinerCount) == 0x04,
               "i386 general-combiner count extent changed");
_Static_assert(offsetof(renderer_register_combiners_t, perStageConstants) ==
                   0x5fc,
               "i386 per-stage constants flag offset changed");
_Static_assert(sizeof(((renderer_register_combiners_t *)0)
                          ->perStageConstants) == 0x04,
               "i386 per-stage constants flag extent changed");
_Static_assert(sizeof(renderer_register_combiners_t) == 0x600,
               "original i386 register-combiner description size changed");
_Static_assert(_Alignof(renderer_atifs_constant_definition_t) == 0x04,
               "i386 ATI fragment constant-definition alignment changed");
_Static_assert(offsetof(renderer_atifs_constant_definition_t, value) == 0x00,
               "i386 ATI fragment constant value offset changed");
_Static_assert(sizeof(((renderer_atifs_constant_definition_t *)0)->value) ==
                   0x10,
               "i386 ATI fragment constant value extent changed");
_Static_assert(offsetof(renderer_atifs_constant_definition_t, defined) ==
                   0x10,
               "i386 ATI fragment constant-defined offset changed");
_Static_assert(sizeof(((renderer_atifs_constant_definition_t *)0)->defined) ==
                   0x04,
               "i386 ATI fragment constant-defined extent changed");
_Static_assert(sizeof(renderer_atifs_constant_definition_t) == 0x14,
               "original i386 ATI fragment constant-definition size changed");
_Static_assert(_Alignof(renderer_atifs_texture_read_t) == 0x04,
               "i386 ATI fragment texture-read alignment changed");
_Static_assert(offsetof(renderer_atifs_texture_read_t, sampleMap) == 0x00,
               "i386 ATI fragment sample-map flag offset changed");
_Static_assert(sizeof(((renderer_atifs_texture_read_t *)0)->sampleMap) ==
                   0x04,
               "i386 ATI fragment sample-map flag extent changed");
_Static_assert(offsetof(renderer_atifs_texture_read_t, source) == 0x04,
               "i386 ATI fragment texture source offset changed");
_Static_assert(sizeof(((renderer_atifs_texture_read_t *)0)->source) == 0x04,
               "i386 ATI fragment texture source extent changed");
_Static_assert(offsetof(renderer_atifs_texture_read_t, swizzle) == 0x08,
               "i386 ATI fragment texture swizzle offset changed");
_Static_assert(sizeof(((renderer_atifs_texture_read_t *)0)->swizzle) == 0x04,
               "i386 ATI fragment texture swizzle extent changed");
_Static_assert(sizeof(renderer_atifs_texture_read_t) == 0x0c,
               "original i386 ATI fragment texture-read size changed");
_Static_assert(_Alignof(renderer_atifs_argument_t) == 0x04,
               "i386 ATI fragment argument alignment changed");
_Static_assert(offsetof(renderer_atifs_argument_t, source) == 0x00,
               "i386 ATI fragment argument source offset changed");
_Static_assert(sizeof(((renderer_atifs_argument_t *)0)->source) == 0x04,
               "i386 ATI fragment argument source extent changed");
_Static_assert(offsetof(renderer_atifs_argument_t, componentUsage) == 0x04,
               "i386 ATI fragment argument component offset changed");
_Static_assert(sizeof(((renderer_atifs_argument_t *)0)->componentUsage) ==
                   0x04,
               "i386 ATI fragment argument component extent changed");
_Static_assert(offsetof(renderer_atifs_argument_t, modifier) == 0x08,
               "i386 ATI fragment argument modifier offset changed");
_Static_assert(sizeof(((renderer_atifs_argument_t *)0)->modifier) == 0x04,
               "i386 ATI fragment argument modifier extent changed");
_Static_assert(sizeof(renderer_atifs_argument_t) == 0x0c,
               "original i386 ATI fragment argument size changed");
_Static_assert(_Alignof(renderer_atifs_instruction_t) == 0x04,
               "i386 ATI fragment instruction alignment changed");
_Static_assert(offsetof(renderer_atifs_instruction_t, operation) == 0x00,
               "i386 ATI fragment operation offset changed");
_Static_assert(sizeof(((renderer_atifs_instruction_t *)0)->operation) == 0x04,
               "i386 ATI fragment operation extent changed");
_Static_assert(offsetof(renderer_atifs_instruction_t, destination) == 0x04,
               "i386 ATI fragment destination offset changed");
_Static_assert(sizeof(((renderer_atifs_instruction_t *)0)->destination) ==
                   0x04,
               "i386 ATI fragment destination extent changed");
_Static_assert(offsetof(renderer_atifs_instruction_t, destinationMask) == 0x08,
               "i386 ATI fragment destination-mask offset changed");
_Static_assert(sizeof(((renderer_atifs_instruction_t *)0)->destinationMask) ==
                   0x04,
               "i386 ATI fragment destination-mask extent changed");
_Static_assert(offsetof(renderer_atifs_instruction_t,
                        destinationModifier) == 0x0c,
               "i386 ATI fragment destination-modifier offset changed");
_Static_assert(sizeof(((renderer_atifs_instruction_t *)0)
                          ->destinationModifier) == 0x04,
               "i386 ATI fragment destination-modifier extent changed");
_Static_assert(offsetof(renderer_atifs_instruction_t, arguments) == 0x10,
               "i386 ATI fragment argument-array offset changed");
_Static_assert(sizeof(((renderer_atifs_instruction_t *)0)->arguments) == 0x24,
               "i386 ATI fragment argument-array extent changed");
_Static_assert(sizeof(renderer_atifs_instruction_t) == 0x34,
               "original i386 ATI fragment instruction size changed");
_Static_assert(_Alignof(renderer_atifs_operation_pair_t) == 0x04,
               "i386 ATI fragment operation-pair alignment changed");
_Static_assert(offsetof(renderer_atifs_operation_pair_t, color) == 0x00,
               "i386 ATI fragment color-operation offset changed");
_Static_assert(sizeof(((renderer_atifs_operation_pair_t *)0)->color) == 0x34,
               "i386 ATI fragment color-operation extent changed");
_Static_assert(offsetof(renderer_atifs_operation_pair_t, alpha) == 0x34,
               "i386 ATI fragment alpha-operation offset changed");
_Static_assert(sizeof(((renderer_atifs_operation_pair_t *)0)->alpha) == 0x34,
               "i386 ATI fragment alpha-operation extent changed");
_Static_assert(sizeof(renderer_atifs_operation_pair_t) == 0x68,
               "original i386 ATI fragment operation-pair size changed");
_Static_assert(_Alignof(renderer_atifs_phase_t) == 0x04,
               "i386 ATI fragment phase alignment changed");
_Static_assert(offsetof(renderer_atifs_phase_t, textureReads) == 0x00,
               "i386 ATI fragment texture-read array offset changed");
_Static_assert(sizeof(((renderer_atifs_phase_t *)0)->textureReads) == 0x48,
               "i386 ATI fragment texture-read array extent changed");
_Static_assert(offsetof(renderer_atifs_phase_t, operationPairs) == 0x48,
               "i386 ATI fragment operation-pair offset changed");
_Static_assert(sizeof(((renderer_atifs_phase_t *)0)->operationPairs) == 0x340,
               "i386 ATI fragment operation-pair array extent changed");
_Static_assert(sizeof(renderer_atifs_phase_t) == 0x388,
               "original i386 ATI fragment phase size changed");
_Static_assert(_Alignof(renderer_atifs_program_t) == 0x04,
               "i386 ATI fragment program alignment changed");
_Static_assert(offsetof(renderer_atifs_program_t, constantDefinitions) == 0x00,
               "i386 ATI fragment parser constants offset changed");
_Static_assert(sizeof(((renderer_atifs_program_t *)0)->constantDefinitions) ==
                   0x0b4,
               "i386 ATI fragment parser constants extent changed");
_Static_assert(offsetof(renderer_atifs_program_t, parsed) == 0x00,
               "i386 ATI fragment parsed view offset changed");
_Static_assert(offsetof(renderer_atifs_program_t,
                        parsed.uploadConstants) == 0x00,
               "i386 ATI fragment upload constants offset changed");
_Static_assert(sizeof(((renderer_atifs_program_t *)0)
                          ->parsed.uploadConstants) == 0x0a0,
               "i386 ATI fragment upload constants extent changed");
_Static_assert(offsetof(renderer_atifs_program_t, parsed.phases) == 0xa0,
               "i386 ATI fragment first-phase offset changed");
_Static_assert(sizeof(((renderer_atifs_program_t *)0)->parsed.phases) == 0x710,
               "i386 ATI fragment phase-array extent changed");
_Static_assert(sizeof(renderer_atifs_program_t) == 0x7b0,
               "original i386 ATI fragment program size changed");
_Static_assert(_Alignof(renderer_vertex_program_t) == 0x04,
               "i386 renderer vertex-program alignment changed");
_Static_assert(offsetof(renderer_vertex_program_t, name) == 0x00,
               "i386 renderer vertex-program name offset changed");
_Static_assert(sizeof(((renderer_vertex_program_t *)0)->name) == 0x40,
               "i386 renderer vertex-program name extent changed");
_Static_assert(offsetof(renderer_vertex_program_t, glProgramName) == 0x40,
               "i386 vertex-program GL name offset changed");
_Static_assert(sizeof(((renderer_vertex_program_t *)0)->glProgramName) == 0x04,
               "i386 renderer vertex-program GL name extent changed");
_Static_assert(sizeof(renderer_vertex_program_t) == 0x44,
               "original i386 renderer vertex-program size changed");
_Static_assert(_Alignof(shader_texture_combine_args_t) == 0x04,
               "i386 texture-combine argument alignment changed");
_Static_assert(offsetof(shader_texture_combine_args_t, operation) == 0x00,
               "i386 texture-combine operation offset changed");
_Static_assert(sizeof(((shader_texture_combine_args_t *)0)->operation) ==
                   0x04,
               "i386 texture-combine operation extent changed");
_Static_assert(offsetof(shader_texture_combine_args_t, sources) == 0x04,
               "i386 texture-combine sources offset changed");
_Static_assert(sizeof(((shader_texture_combine_args_t *)0)->sources) == 0x0c,
               "i386 texture-combine sources extent changed");
_Static_assert(offsetof(shader_texture_combine_args_t, operands) == 0x10,
               "i386 texture-combine operands offset changed");
_Static_assert(sizeof(((shader_texture_combine_args_t *)0)->operands) ==
                   0x0c,
               "i386 texture-combine operands extent changed");
_Static_assert(offsetof(shader_texture_combine_args_t, scale) == 0x1c,
               "i386 texture-combine scale offset changed");
_Static_assert(sizeof(((shader_texture_combine_args_t *)0)->scale) == 0x04,
               "i386 texture-combine scale extent changed");
_Static_assert(sizeof(shader_texture_combine_args_t) == 0x20,
               "original i386 texture-combine argument size changed");
_Static_assert(_Alignof(shader_texture_combine_t) == 0x04,
               "i386 texture-combine alignment changed");
_Static_assert(offsetof(shader_texture_combine_t, environmentColor) == 0x00,
               "i386 texture-combine environment-color offset changed");
_Static_assert(sizeof(((shader_texture_combine_t *)0)->environmentColor) ==
                   0x10,
               "i386 texture-combine environment-color extent changed");
_Static_assert(offsetof(shader_texture_combine_t, rgb) == 0x10,
               "i386 RGB texture-combine arguments moved");
_Static_assert(sizeof(((shader_texture_combine_t *)0)->rgb) == 0x20,
               "i386 RGB texture-combine argument extent changed");
_Static_assert(offsetof(shader_texture_combine_t, alpha) == 0x30,
               "i386 alpha texture-combine arguments moved");
_Static_assert(sizeof(((shader_texture_combine_t *)0)->alpha) == 0x20,
               "i386 alpha texture-combine argument extent changed");
_Static_assert(sizeof(shader_texture_combine_t) == 0x50,
               "original i386 texture-combine description size changed");
_Static_assert(_Alignof(shader_requirement_operand_t) == 0x04,
               "i386 shader-requirement operand alignment changed");
_Static_assert(offsetof(shader_requirement_operand_t, type) == 0x00,
               "i386 shader-requirement operand type offset changed");
_Static_assert(sizeof(((shader_requirement_operand_t *)0)->type) == 0x04,
               "i386 shader-requirement operand type extent changed");
_Static_assert(offsetof(shader_requirement_operand_t, dependency) == 0x04,
               "i386 shader-requirement dependency offset changed");
_Static_assert(sizeof(((shader_requirement_operand_t *)0)->dependency) ==
                   0x02,
               "i386 shader-requirement dependency extent changed");
_Static_assert(offsetof(shader_requirement_operand_t, leadingNotCount) ==
                   0x06,
               "i386 shader-requirement not-count offset changed");
_Static_assert(sizeof(((shader_requirement_operand_t *)0)->leadingNotCount) ==
                   0x02,
               "i386 shader-requirement not-count extent changed");
_Static_assert(offsetof(shader_requirement_operand_t, value) == 0x08,
               "i386 shader-requirement value offset changed");
_Static_assert(sizeof(((shader_requirement_operand_t *)0)->value) == 0xf8,
               "i386 shader-requirement value extent changed");
_Static_assert(sizeof(shader_requirement_operand_t) == 0x100,
               "original i386 shader-requirement operand size changed");
_Static_assert(_Alignof(shader_texture_shader_t) == 0x04,
               "i386 texture-shader alignment changed");
_Static_assert(offsetof(shader_texture_shader_t, operation) == 0x00,
               "i386 texture-shader operation offset changed");
_Static_assert(sizeof(((shader_texture_shader_t *)0)->operation) == 0x04,
               "i386 texture-shader operation extent changed");
_Static_assert(offsetof(shader_texture_shader_t, previousTextureInput) ==
                   0x04,
               "i386 texture-shader previous-input offset changed");
_Static_assert(sizeof(((shader_texture_shader_t *)0)->previousTextureInput) ==
                   0x04,
               "i386 texture-shader previous-input extent changed");
_Static_assert(offsetof(shader_texture_shader_t, dotProductMapping) == 0x08,
               "i386 texture-shader dot-mapping offset changed");
_Static_assert(sizeof(((shader_texture_shader_t *)0)->dotProductMapping) ==
                   0x04,
               "i386 texture-shader dot-mapping extent changed");
_Static_assert(offsetof(shader_texture_shader_t, parameters) == 0x0c,
               "i386 texture-shader parameter offset changed");
_Static_assert(sizeof(((shader_texture_shader_t *)0)->parameters) == 0x10,
               "i386 texture-shader parameter extent changed");
_Static_assert(offsetof(shader_texture_shader_t, parameters.floats) == 0x0c,
               "i386 texture-shader float parameters moved");
_Static_assert(sizeof(((shader_texture_shader_t *)0)->parameters.floats) ==
                   0x10,
               "i386 texture-shader float parameters extent changed");
_Static_assert(offsetof(shader_texture_shader_t, parameters.cullModes) ==
                   0x0c,
               "i386 texture-shader cull parameters moved");
_Static_assert(sizeof(((shader_texture_shader_t *)0)->parameters.cullModes) ==
                   0x10,
               "i386 texture-shader cull parameters extent changed");
_Static_assert(sizeof(shader_texture_shader_t) == 0x1c,
               "original i386 texture-shader size changed");
_Static_assert(offsetof(shaderCommands_t, indexes) == 0,
               "i386 tessellation index offset changed");
_Static_assert(offsetof(shaderCommands_t, xyz) == 0x0c0000,
               "i386 tessellation xyz offset changed");
_Static_assert(offsetof(shaderCommands_t, texCoords) == 0x1c0000,
               "i386 tessellation texcoord offset changed");
_Static_assert(offsetof(shaderCommands_t, vertexColors) == 0x3c0000,
               "i386 tessellation color offset changed");
_Static_assert(offsetof(shaderCommands_t, stageNormals) == 0x400000,
               "i386 tessellation stage-normal offset changed");
_Static_assert(offsetof(shaderCommands_t, stageBitangents) == 0x4c0000,
               "i386 tessellation stage-bitangent offset changed");
_Static_assert(offsetof(shaderCommands_t, stageTangents) == 0x580000,
               "i386 tessellation stage-tangent offset changed");
_Static_assert(offsetof(shaderCommands_t, activeTexCoords) == 0x680000,
               "i386 tessellation active-texcoord table offset changed");
_Static_assert(offsetof(shaderCommands_t, generatedTexCoords) ==
                   0x680020,
               "i386 tessellation generated-texcoord storage offset changed");
_Static_assert(offsetof(shaderCommands_t, stageVertexColors) == 0x640000,
               "i386 tessellation stage-color offset changed");
_Static_assert(offsetof(shaderCommands_t, constantColor255) == 0xe80020,
               "i386 tessellation constant-color offset changed");
_Static_assert(offsetof(shaderCommands_t, vertexComponentCount) ==
                   0xec0020,
               "i386 tessellation vertex-component offset changed");
_Static_assert(offsetof(shaderCommands_t, requiresVertexBasis) ==
                   0xec0024,
               "i386 tessellation vertex-basis requirement offset changed");
_Static_assert(offsetof(shaderCommands_t, stageTangentsValid) ==
                   0xec0028,
               "i386 tessellation tangent-validity offset changed");
_Static_assert(offsetof(shaderCommands_t, stageBitangentsValid) ==
                   0xec002c,
               "i386 tessellation bitangent-validity offset changed");
_Static_assert(offsetof(shaderCommands_t, shader) == 0xec0030,
               "i386 tessellation shader offset changed");
_Static_assert(offsetof(shaderCommands_t, unrecoveredState034) ==
                   0xec0034,
               "i386 tessellation dormant-state offset changed");
_Static_assert(offsetof(shaderCommands_t, shaderTime) == 0xec0038,
               "i386 tessellation shader-time offset changed");
_Static_assert(offsetof(shaderCommands_t, entity) == 0xec003c,
               "i386 tessellation entity offset changed");
_Static_assert(offsetof(shaderCommands_t, dlightBits) == 0xec0040,
               "i386 tessellation dlight-bit offset changed");
_Static_assert(offsetof(shaderCommands_t, indexCount) == 0xec0044,
               "i386 tessellation index-count offset changed");
_Static_assert(offsetof(shaderCommands_t, vertexCount) == 0xec0048,
               "i386 tessellation vertex-count offset changed");
_Static_assert(offsetof(shaderCommands_t, optimizedFirstVertex) ==
                   0xec004c,
               "i386 optimized first-vertex offset changed");
_Static_assert(offsetof(shaderCommands_t, optimizedVertexEnd) ==
                   0xec0050,
               "i386 optimized vertex-end offset changed");
_Static_assert(offsetof(shaderCommands_t, optimizedIndexes) ==
                   0xec0054,
               "i386 optimized index-buffer offset changed");
_Static_assert(offsetof(shaderCommands_t, renderedIndexCount) ==
                   0xf00054,
               "i386 tessellation rendered-index count offset changed");
_Static_assert(offsetof(shaderCommands_t, renderedVertexCount) ==
                   0xf00058,
               "i386 tessellation rendered-vertex count offset changed");
_Static_assert(offsetof(shaderCommands_t, activeStageCount) ==
                   0xf0005c,
               "i386 tessellation active-stage count offset changed");
_Static_assert(offsetof(shaderCommands_t, stageIterator) == 0xf00060,
               "i386 tessellation stage-iterator offset changed");
_Static_assert(offsetof(shaderCommands_t, activeStages) == 0xf00064,
               "i386 tessellation active-stage offset changed");
_Static_assert(sizeof(shaderCommands_t) == 15728744,
               "i386 tessellation size changed");
_Static_assert(sizeof(srfPoly_t) == 16,
               "i386 renderer surface-poly size changed");
_Static_assert(offsetof(srfPoly_t, surfaceType) == 0x00,
               "i386 renderer polygon surface-type offset changed");
_Static_assert(offsetof(srfPoly_t, hShader) == 0x04,
               "i386 renderer polygon shader-handle offset changed");
_Static_assert(offsetof(srfPoly_t, numVerts) == 0x08,
               "i386 renderer polygon vertex-count offset changed");
_Static_assert(offsetof(srfPoly_t, verts) == 0x0c,
               "i386 renderer polygon vertex-array offset changed");
_Static_assert(sizeof(polyVert_t) == 32,
               "i386 renderer poly-vertex size changed");
_Static_assert(offsetof(backEndData_t, sceneEntities) == 0x81700,
               "i386 renderer scene-entity array offset changed");
_Static_assert(offsetof(backEndData_t, dlights) == 0x80000,
               "i386 renderer dynamic-light array offset changed");
_Static_assert(offsetof(backEndData_t, coronas) == 0x81100,
               "i386 renderer corona array offset changed");
_Static_assert(offsetof(backEndData_t, entitySurfaces) == 0x12f700,
               "i386 renderer entity-surface array offset changed");
_Static_assert(offsetof(backEndData_t, polys) == 0x13f700,
               "i386 renderer polygon array offset changed");
_Static_assert(offsetof(backEndData_t, polyVertices) == 0x14f700,
               "i386 renderer polygon-vertex array offset changed");
_Static_assert(sizeof(renderer_scene_frame_state_t) == 40,
               "i386 renderer scene-frame state size changed");
_Static_assert(_Alignof(renderer_debug_state_t) == 0x04,
               "i386 renderer debug-state alignment changed");
_Static_assert(offsetof(renderer_debug_state_t, polygonVertexCapacity) == 0x00,
               "i386 renderer debug polygon-vertex capacity moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->polygonVertexCapacity) == 0x04,
               "i386 renderer debug polygon-vertex capacity extent changed");
_Static_assert(offsetof(renderer_debug_state_t, polygonVertexCount) == 0x04,
               "i386 renderer debug polygon-vertex count moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->polygonVertexCount) == 0x04,
               "i386 renderer debug polygon-vertex count extent changed");
_Static_assert(offsetof(renderer_debug_state_t, polygonVertices) == 0x08,
               "i386 renderer debug polygon-vertex pointer moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->polygonVertices) == 0x04,
               "i386 renderer debug polygon-vertex pointer extent changed");
_Static_assert(offsetof(renderer_debug_state_t, polygonCapacity) == 0x0c,
               "i386 renderer debug polygon capacity moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->polygonCapacity) == 0x04,
               "i386 renderer debug polygon capacity extent changed");
_Static_assert(offsetof(renderer_debug_state_t, polygonCount) == 0x10,
               "i386 renderer debug polygon count moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->polygonCount) == 0x04,
               "i386 renderer debug polygon count extent changed");
_Static_assert(offsetof(renderer_debug_state_t, polygons) == 0x14,
               "i386 renderer debug polygon pointer moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->polygons) == 0x04,
               "i386 renderer debug polygon pointer extent changed");
_Static_assert(offsetof(renderer_debug_state_t, stringCapacity) == 0x18,
               "i386 renderer debug string capacity moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->stringCapacity) == 0x04,
               "i386 renderer debug string capacity extent changed");
_Static_assert(offsetof(renderer_debug_state_t, stringCount) == 0x1c,
               "i386 renderer debug string count moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->stringCount) == 0x04,
               "i386 renderer debug string count extent changed");
_Static_assert(offsetof(renderer_debug_state_t, strings) == 0x20,
               "i386 renderer debug string pointer moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->strings) == 0x04,
               "i386 renderer debug string pointer extent changed");
_Static_assert(offsetof(renderer_debug_state_t, font) == 0x24,
               "i386 renderer debug font pointer moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->font) == 0x04,
               "i386 renderer debug font pointer extent changed");
_Static_assert(offsetof(renderer_debug_state_t, locatedStringCount) == 0x28,
               "i386 renderer located-string count moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->locatedStringCount) == 0x04,
               "i386 renderer located-string count extent changed");
_Static_assert(offsetof(renderer_debug_state_t, locatedStrings) == 0x2c,
               "i386 renderer located-string pointer moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->locatedStrings) == 0x04,
               "i386 renderer located-string pointer extent changed");
_Static_assert(offsetof(renderer_debug_state_t, lineCapacity) == 0x30,
               "i386 renderer debug line capacity moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->lineCapacity) == 0x04,
               "i386 renderer debug line capacity extent changed");
_Static_assert(offsetof(renderer_debug_state_t, lineCount) == 0x34,
               "i386 renderer debug line count moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->lineCount) == 0x04,
               "i386 renderer debug line count extent changed");
_Static_assert(offsetof(renderer_debug_state_t, lines) == 0x38,
               "i386 renderer debug line pointer moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->lines) == 0x04,
               "i386 renderer debug line pointer extent changed");
_Static_assert(offsetof(renderer_debug_state_t, locatedLineCount) == 0x3c,
               "i386 renderer located-line count moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->locatedLineCount) == 0x04,
               "i386 renderer located-line count extent changed");
_Static_assert(offsetof(renderer_debug_state_t, locatedLines) == 0x40,
               "i386 renderer located-line pointer moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->locatedLines) == 0x04,
               "i386 renderer located-line pointer extent changed");
_Static_assert(offsetof(renderer_debug_state_t, plumeCount) == 0x44,
               "i386 renderer plume count moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->plumeCount) == 0x04,
               "i386 renderer plume count extent changed");
_Static_assert(offsetof(renderer_debug_state_t, plumeCapacity) == 0x48,
               "i386 renderer plume capacity moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->plumeCapacity) == 0x04,
               "i386 renderer plume capacity extent changed");
_Static_assert(offsetof(renderer_debug_state_t, plumes) == 0x4c,
               "i386 renderer plume pointer moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->plumes) == 0x04,
               "i386 renderer plume pointer extent changed");
_Static_assert(offsetof(renderer_debug_state_t, immediateModeActive) == 0x50,
               "i386 renderer immediate-active flag moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->immediateModeActive) == 0x04,
               "i386 renderer immediate-active flag extent changed");
_Static_assert(offsetof(renderer_debug_state_t, immediatePrimitiveMode) == 0x54,
               "i386 renderer immediate primitive mode moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->immediatePrimitiveMode) == 0x04,
               "i386 renderer immediate primitive-mode extent changed");
_Static_assert(offsetof(renderer_debug_state_t, immediateLineWidth) == 0x58,
               "i386 renderer immediate line width moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->immediateLineWidth) == 0x04,
               "i386 renderer immediate line-width extent changed");
_Static_assert(offsetof(renderer_debug_state_t, immediateTexCoord) == 0x5c,
               "i386 renderer immediate texture coordinate moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->immediateTexCoord) == 0x08,
               "i386 renderer immediate texture-coordinate extent changed");
_Static_assert(offsetof(renderer_debug_state_t, immediateColor) == 0x64,
               "i386 renderer immediate color moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->immediateColor) == 0x04,
               "i386 renderer immediate color extent changed");
_Static_assert(offsetof(renderer_debug_state_t, unusedImmediateState) == 0x68,
               "i386 renderer unused immediate-state region moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->unusedImmediateState) == 0x0c,
               "i386 renderer unused immediate-state extent changed");
_Static_assert(offsetof(renderer_debug_state_t, immediateVertexCount) == 0x74,
               "i386 renderer immediate vertex count moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->immediateVertexCount) == 0x04,
               "i386 renderer immediate vertex-count extent changed");
_Static_assert(offsetof(renderer_debug_state_t, immediateVertexCapacity) == 0x78,
               "i386 renderer immediate vertex capacity moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->immediateVertexCapacity) == 0x04,
               "i386 renderer immediate vertex-capacity extent changed");
_Static_assert(offsetof(renderer_debug_state_t, immediateVertices) == 0x7c,
               "i386 renderer immediate vertex pointer moved");
_Static_assert(sizeof(((renderer_debug_state_t *)0)->immediateVertices) == 0x04,
               "i386 renderer immediate vertex-pointer extent changed");
_Static_assert(sizeof(renderer_debug_state_t) == 0x80,
               "original i386 renderer debug-state size changed");
_Static_assert(_Alignof(renderer_sun_state_t) == 0x04,
               "i386 renderer sun-state alignment changed");
_Static_assert(offsetof(renderer_sun_state_t, spriteShader) == 0x00,
               "i386 renderer sun sprite shader moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->spriteShader) == 0x04,
               "i386 renderer sun sprite-shader extent changed");
_Static_assert(offsetof(renderer_sun_state_t, spriteVertices) == 0x04,
               "i386 renderer sun sprite vertices moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->spriteVertices) == 0x40,
               "i386 renderer sun sprite-vertex extent changed");
_Static_assert(offsetof(renderer_sun_state_t, spriteSize) == 0x44,
               "i386 renderer sun sprite size moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->spriteSize) == 0x04,
               "i386 renderer sun sprite-size extent changed");
_Static_assert(offsetof(renderer_sun_state_t, flareShader) == 0x48,
               "i386 renderer sun flare shader moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->flareShader) == 0x04,
               "i386 renderer sun flare-shader extent changed");
_Static_assert(offsetof(renderer_sun_state_t, flareMinHalfSize) == 0x4c,
               "i386 renderer sun flare minimum half-size moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->flareMinHalfSize) == 0x04,
               "i386 renderer sun flare minimum half-size extent changed");
_Static_assert(offsetof(renderer_sun_state_t, flareMinCosAngle) == 0x50,
               "i386 renderer sun flare minimum cosine moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->flareMinCosAngle) == 0x04,
               "i386 renderer sun flare minimum-cosine extent changed");
_Static_assert(offsetof(renderer_sun_state_t, flareMaxHalfSize) == 0x54,
               "i386 renderer sun flare maximum half-size moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->flareMaxHalfSize) == 0x04,
               "i386 renderer sun flare maximum half-size extent changed");
_Static_assert(offsetof(renderer_sun_state_t, flareMaxCosAngle) == 0x58,
               "i386 renderer sun flare maximum cosine moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->flareMaxCosAngle) == 0x04,
               "i386 renderer sun flare maximum-cosine extent changed");
_Static_assert(offsetof(renderer_sun_state_t, flareMaxAlpha) == 0x5c,
               "i386 renderer sun flare maximum alpha moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->flareMaxAlpha) == 0x04,
               "i386 renderer sun flare maximum-alpha extent changed");
_Static_assert(offsetof(renderer_sun_state_t, flareFadeInMsec) == 0x60,
               "i386 renderer sun flare fade-in time moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->flareFadeInMsec) == 0x04,
               "i386 renderer sun flare fade-in extent changed");
_Static_assert(offsetof(renderer_sun_state_t, flareFadeOutMsec) == 0x64,
               "i386 renderer sun flare fade-out time moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->flareFadeOutMsec) == 0x04,
               "i386 renderer sun flare fade-out extent changed");
_Static_assert(offsetof(renderer_sun_state_t, blindMinCosAngle) == 0x68,
               "i386 renderer sun blindness minimum cosine moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->blindMinCosAngle) == 0x04,
               "i386 renderer sun blindness minimum-cosine extent changed");
_Static_assert(offsetof(renderer_sun_state_t, blindMaxCosAngle) == 0x6c,
               "i386 renderer sun blindness maximum cosine moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->blindMaxCosAngle) == 0x04,
               "i386 renderer sun blindness maximum-cosine extent changed");
_Static_assert(offsetof(renderer_sun_state_t, blindMaxDarken) == 0x70,
               "i386 renderer sun blindness maximum darkening moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->blindMaxDarken) == 0x04,
               "i386 renderer sun blindness maximum-darkening extent changed");
_Static_assert(offsetof(renderer_sun_state_t, blindFadeInMsec) == 0x74,
               "i386 renderer sun blindness fade-in time moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->blindFadeInMsec) == 0x04,
               "i386 renderer sun blindness fade-in extent changed");
_Static_assert(offsetof(renderer_sun_state_t, blindFadeOutMsec) == 0x78,
               "i386 renderer sun blindness fade-out time moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->blindFadeOutMsec) == 0x04,
               "i386 renderer sun blindness fade-out extent changed");
_Static_assert(offsetof(renderer_sun_state_t, glareMinCosAngle) == 0x7c,
               "i386 renderer sun glare minimum cosine moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->glareMinCosAngle) == 0x04,
               "i386 renderer sun glare minimum-cosine extent changed");
_Static_assert(offsetof(renderer_sun_state_t, glareMaxCosAngle) == 0x80,
               "i386 renderer sun glare maximum cosine moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->glareMaxCosAngle) == 0x04,
               "i386 renderer sun glare maximum-cosine extent changed");
_Static_assert(offsetof(renderer_sun_state_t, glareMaxLighten) == 0x84,
               "i386 renderer sun glare maximum lightening moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->glareMaxLighten) == 0x04,
               "i386 renderer sun glare maximum-lightening extent changed");
_Static_assert(offsetof(renderer_sun_state_t, glareFadeInMsec) == 0x88,
               "i386 renderer sun glare fade-in time moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->glareFadeInMsec) == 0x04,
               "i386 renderer sun glare fade-in extent changed");
_Static_assert(offsetof(renderer_sun_state_t, glareFadeOutMsec) == 0x8c,
               "i386 renderer sun glare fade-out time moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->glareFadeOutMsec) == 0x04,
               "i386 renderer sun glare fade-out extent changed");
_Static_assert(offsetof(renderer_sun_state_t, currentBlindFraction) == 0x90,
               "i386 renderer current blindness fraction moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->currentBlindFraction) == 0x04,
               "i386 renderer current blindness-fraction extent changed");
_Static_assert(offsetof(renderer_sun_state_t, currentGlareFraction) == 0x94,
               "i386 renderer current glare fraction moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->currentGlareFraction) == 0x04,
               "i386 renderer current glare-fraction extent changed");
_Static_assert(offsetof(renderer_sun_state_t, lastUpdateTime) == 0x98,
               "i386 renderer sun update time moved");
_Static_assert(sizeof(((renderer_sun_state_t *)0)->lastUpdateTime) == 0x04,
               "i386 renderer sun update-time extent changed");
_Static_assert(sizeof(renderer_sun_state_t) == 0x9c,
               "original i386 renderer sun-state size changed");
_Static_assert(_Alignof(renderer_scene_frame_state_t) == 0x04,
               "i386 renderer scene-frame state alignment changed");
_Static_assert(offsetof(renderer_scene_frame_state_t, entityCount) == 0x00,
               "i386 renderer scene entity count moved");
_Static_assert(sizeof(((renderer_scene_frame_state_t *)0)->entityCount) == 0x04,
               "i386 renderer scene entity-count extent changed");
_Static_assert(offsetof(renderer_scene_frame_state_t, coronaCount) == 0x04,
               "i386 renderer scene corona count moved");
_Static_assert(sizeof(((renderer_scene_frame_state_t *)0)->coronaCount) == 0x04,
               "i386 renderer scene corona-count extent changed");
_Static_assert(offsetof(renderer_scene_frame_state_t, dlightCount) == 0x08,
               "i386 renderer scene dynamic-light count moved");
_Static_assert(sizeof(((renderer_scene_frame_state_t *)0)->dlightCount) == 0x04,
               "i386 renderer scene dynamic-light count extent changed");
_Static_assert(offsetof(renderer_scene_frame_state_t, firstDlight) == 0x0c,
               "i386 renderer scene first dynamic-light index moved");
_Static_assert(sizeof(((renderer_scene_frame_state_t *)0)->firstDlight) == 0x04,
               "i386 renderer scene first dynamic-light extent changed");
_Static_assert(offsetof(renderer_scene_frame_state_t, polyVertexCount) == 0x10,
               "i386 renderer scene polygon-vertex count moved");
_Static_assert(sizeof(((renderer_scene_frame_state_t *)0)->polyVertexCount) == 0x04,
               "i386 renderer scene polygon-vertex count extent changed");
_Static_assert(offsetof(renderer_scene_frame_state_t, firstPoly) == 0x14,
               "i386 renderer scene first-polygon index moved");
_Static_assert(sizeof(((renderer_scene_frame_state_t *)0)->firstPoly) == 0x04,
               "i386 renderer scene first-polygon extent changed");
_Static_assert(offsetof(renderer_scene_frame_state_t, drawSurfCount) == 0x18,
               "i386 renderer scene draw-surface count moved");
_Static_assert(sizeof(((renderer_scene_frame_state_t *)0)->drawSurfCount) == 0x04,
               "i386 renderer scene draw-surface count extent changed");
_Static_assert(offsetof(renderer_scene_frame_state_t, firstCorona) == 0x1c,
               "i386 renderer scene first-corona index moved");
_Static_assert(sizeof(((renderer_scene_frame_state_t *)0)->firstCorona) == 0x04,
               "i386 renderer scene first-corona extent changed");
_Static_assert(offsetof(renderer_scene_frame_state_t, firstEntity) == 0x20,
               "i386 renderer scene first-entity index moved");
_Static_assert(sizeof(((renderer_scene_frame_state_t *)0)->firstEntity) == 0x04,
               "i386 renderer scene first-entity extent changed");
_Static_assert(offsetof(renderer_scene_frame_state_t, polyCount) == 0x24,
               "i386 renderer scene polygon count moved");
_Static_assert(sizeof(((renderer_scene_frame_state_t *)0)->polyCount) == 0x04,
               "i386 renderer scene polygon-count extent changed");
_Static_assert(sizeof(renderer_scene_frame_state_t) == 0x28,
               "original i386 renderer scene-frame state size changed");
_Static_assert(_Alignof(backEndData_t) == 0x04,
               "i386 renderer backend-data alignment changed");
_Static_assert(offsetof(backEndData_t, drawSurfs) == 0x000000,
               "i386 renderer draw-surface array moved");
_Static_assert(sizeof(((backEndData_t *)0)->drawSurfs) == 0x080000,
               "i386 renderer draw-surface array extent changed");
_Static_assert(offsetof(backEndData_t, dlights) == 0x080000,
               "i386 renderer dynamic-light array moved");
_Static_assert(sizeof(((backEndData_t *)0)->dlights) == 0x001100,
               "i386 renderer dynamic-light array extent changed");
_Static_assert(offsetof(backEndData_t, coronas) == 0x081100,
               "i386 renderer corona array moved");
_Static_assert(sizeof(((backEndData_t *)0)->coronas) == 0x000600,
               "i386 renderer corona array extent changed");
_Static_assert(offsetof(backEndData_t, sceneEntities) == 0x081700,
               "i386 renderer scene-entity array moved");
_Static_assert(sizeof(((backEndData_t *)0)->sceneEntities) == 0x0ae000,
               "i386 renderer scene-entity array extent changed");
_Static_assert(offsetof(backEndData_t, entitySurfaces) == 0x12f700,
               "i386 renderer entity-surface array moved");
_Static_assert(sizeof(((backEndData_t *)0)->entitySurfaces) == 0x010000,
               "i386 renderer entity-surface array extent changed");
_Static_assert(offsetof(backEndData_t, polys) == 0x13f700,
               "i386 renderer polygon array moved");
_Static_assert(sizeof(((backEndData_t *)0)->polys) == 0x010000,
               "i386 renderer polygon array extent changed");
_Static_assert(offsetof(backEndData_t, polyVertices) == 0x14f700,
               "i386 renderer polygon-vertex array moved");
_Static_assert(sizeof(((backEndData_t *)0)->polyVertices) == 0x080000,
               "i386 renderer polygon-vertex array extent changed");
_Static_assert(offsetof(backEndData_t, commandBuffer) == 0x1cf700,
               "i386 renderer command buffer moved");
_Static_assert(sizeof(((backEndData_t *)0)->commandBuffer) == 0x040000,
               "i386 renderer command-buffer extent changed");
_Static_assert(offsetof(backEndData_t, commandUsed) == 0x20f700,
               "i386 renderer command-used counter moved");
_Static_assert(sizeof(((backEndData_t *)0)->commandUsed) == 0x04,
               "i386 renderer command-used extent changed");
_Static_assert(offsetof(backEndData_t, unusedAllocationTail) ==
                   0x20f704,
               "i386 renderer unused allocation tail moved");
_Static_assert(sizeof(backEndData_t) == 0x20f704,
               "original i386 renderer backend-data prefix size changed");
_Static_assert(offsetof(backEndData_t, commandUsed) == 0x20f700,
               "i386 renderer command-used offset changed");
_Static_assert(offsetof(backEndData_t, commandBuffer) == 0x1cf700,
               "i386 renderer command-buffer offset changed");
_Static_assert(sizeof(backEndData_t) == 0x20f704,
               "i386 renderer backend-data prefix size changed");
_Static_assert(_Alignof(setColorCommand_t) == 4,
               "i386 set-color command alignment changed");
_Static_assert(offsetof(setColorCommand_t, commandId) == 0x00,
               "i386 set-color command id moved");
_Static_assert(offsetof(setColorCommand_t, color) == 0x04,
               "i386 set-color command color moved");
_Static_assert(sizeof(setColorCommand_t) == 0x08,
               "i386 set-color command size changed");
_Static_assert(_Alignof(stretchPicCommand_t) == 4,
               "i386 stretch-pic command alignment changed");
_Static_assert(offsetof(stretchPicCommand_t, commandId) == 0x00,
               "i386 stretch-pic command id moved");
_Static_assert(offsetof(stretchPicCommand_t, shader) == 0x04,
               "i386 stretch-pic command shader moved");
_Static_assert(offsetof(stretchPicCommand_t, x) == 0x08,
               "i386 stretch-pic command X moved");
_Static_assert(offsetof(stretchPicCommand_t, y) == 0x0c,
               "i386 stretch-pic command Y moved");
_Static_assert(offsetof(stretchPicCommand_t, w) == 0x10,
               "i386 stretch-pic command width moved");
_Static_assert(offsetof(stretchPicCommand_t, h) == 0x14,
               "i386 stretch-pic command height moved");
_Static_assert(offsetof(stretchPicCommand_t, s1) == 0x18,
               "i386 stretch-pic command S1 moved");
_Static_assert(offsetof(stretchPicCommand_t, t1) == 0x1c,
               "i386 stretch-pic command T1 moved");
_Static_assert(offsetof(stretchPicCommand_t, s2) == 0x20,
               "i386 stretch-pic command S2 moved");
_Static_assert(offsetof(stretchPicCommand_t, t2) == 0x24,
               "i386 stretch-pic command T2 moved");
_Static_assert(sizeof(stretchPicCommand_t) == 0x28,
               "i386 stretch-pic command size changed");
_Static_assert(_Alignof(stretch_pic_gradient_command_t) == 4,
               "i386 gradient stretch-pic command alignment changed");
_Static_assert(offsetof(stretch_pic_gradient_command_t, commandId) == 0x00,
               "i386 gradient stretch-pic command id moved");
_Static_assert(offsetof(stretch_pic_gradient_command_t, shader) == 0x04,
               "i386 gradient stretch-pic shader moved");
_Static_assert(offsetof(stretch_pic_gradient_command_t, x) == 0x08,
               "i386 gradient stretch-pic X moved");
_Static_assert(offsetof(stretch_pic_gradient_command_t, y) == 0x0c,
               "i386 gradient stretch-pic Y moved");
_Static_assert(offsetof(stretch_pic_gradient_command_t, width) == 0x10,
               "i386 gradient stretch-pic width moved");
_Static_assert(offsetof(stretch_pic_gradient_command_t, height) == 0x14,
               "i386 gradient stretch-pic height moved");
_Static_assert(offsetof(stretch_pic_gradient_command_t, s1) == 0x18,
               "i386 gradient stretch-pic S1 moved");
_Static_assert(offsetof(stretch_pic_gradient_command_t, t1) == 0x1c,
               "i386 gradient stretch-pic T1 moved");
_Static_assert(offsetof(stretch_pic_gradient_command_t, s2) == 0x20,
               "i386 gradient stretch-pic S2 moved");
_Static_assert(offsetof(stretch_pic_gradient_command_t, t2) == 0x24,
               "i386 gradient stretch-pic T2 moved");
_Static_assert(offsetof(stretch_pic_gradient_command_t, gradientColor) == 0x28,
               "i386 gradient stretch-pic color moved");
_Static_assert(offsetof(stretch_pic_gradient_command_t, gradientType) == 0x2c,
               "i386 gradient stretch-pic type moved");
_Static_assert(sizeof(stretch_pic_gradient_command_t) == 0x30,
               "i386 gradient stretch-pic command size changed");
_Static_assert(_Alignof(stretch_pic_rotate_command_t) == 4,
               "i386 rotated stretch-pic command alignment changed");
_Static_assert(offsetof(stretch_pic_rotate_command_t, commandId) == 0x00,
               "i386 rotated stretch-pic command id moved");
_Static_assert(offsetof(stretch_pic_rotate_command_t, shader) == 0x04,
               "i386 rotated stretch-pic shader moved");
_Static_assert(offsetof(stretch_pic_rotate_command_t, x) == 0x08,
               "i386 rotated stretch-pic X moved");
_Static_assert(offsetof(stretch_pic_rotate_command_t, y) == 0x0c,
               "i386 rotated stretch-pic Y moved");
_Static_assert(offsetof(stretch_pic_rotate_command_t, width) == 0x10,
               "i386 rotated stretch-pic width moved");
_Static_assert(offsetof(stretch_pic_rotate_command_t, height) == 0x14,
               "i386 rotated stretch-pic height moved");
_Static_assert(offsetof(stretch_pic_rotate_command_t, s1) == 0x18,
               "i386 rotated stretch-pic S1 moved");
_Static_assert(offsetof(stretch_pic_rotate_command_t, t1) == 0x1c,
               "i386 rotated stretch-pic T1 moved");
_Static_assert(offsetof(stretch_pic_rotate_command_t, s2) == 0x20,
               "i386 rotated stretch-pic S2 moved");
_Static_assert(offsetof(stretch_pic_rotate_command_t, t2) == 0x24,
               "i386 rotated stretch-pic T2 moved");
_Static_assert(offsetof(stretch_pic_rotate_command_t, angleDegrees) == 0x28,
               "i386 rotated stretch-pic angle moved");
_Static_assert(sizeof(stretch_pic_rotate_command_t) == 0x2c,
               "i386 rotated stretch-pic command size changed");
_Static_assert(_Alignof(draw_quad_pic_command_t) == 4,
               "i386 draw-quad-pic command alignment changed");
_Static_assert(offsetof(draw_quad_pic_command_t, commandId) == 0x00,
               "i386 draw-quad-pic command id moved");
_Static_assert(offsetof(draw_quad_pic_command_t, shader) == 0x04,
               "i386 draw-quad-pic shader moved");
_Static_assert(offsetof(draw_quad_pic_command_t, positions) == 0x08,
               "i386 draw-quad-pic positions moved");
_Static_assert(offsetof(draw_quad_pic_command_t, texCoords) == 0x28,
               "i386 draw-quad-pic texture coordinates moved");
_Static_assert(sizeof(draw_quad_pic_command_t) == 0x48,
               "i386 draw-quad-pic command size changed");
_Static_assert(_Alignof(drawSurfsCommand_t) == 4,
               "i386 draw-surfs command alignment changed");
_Static_assert(offsetof(drawSurfsCommand_t, commandId) == 0x000,
               "i386 draw-surfs command id moved");
_Static_assert(offsetof(drawSurfsCommand_t, refdef) == 0x004,
               "i386 draw-surfs refdef moved");
_Static_assert(offsetof(drawSurfsCommand_t, viewParms) == 0x18c,
               "i386 draw-surfs view parameters moved");
_Static_assert(offsetof(drawSurfsCommand_t, drawSurfs) == 0x3ec,
               "i386 draw-surfs surface pointer moved");
_Static_assert(offsetof(drawSurfsCommand_t, numDrawSurfs) == 0x3f0,
               "i386 draw-surfs surface count moved");
_Static_assert(sizeof(drawSurfsCommand_t) == 0x3f4,
               "i386 draw-surfs command size changed");
_Static_assert(_Alignof(drawBufferCommand_t) == 4,
               "i386 draw-buffer command alignment changed");
_Static_assert(offsetof(drawBufferCommand_t, commandId) == 0x00,
               "i386 draw-buffer command id moved");
_Static_assert(offsetof(drawBufferCommand_t, buffer) == 0x04,
               "i386 draw-buffer target moved");
_Static_assert(sizeof(drawBufferCommand_t) == 0x08,
               "i386 draw-buffer command size changed");
_Static_assert(_Alignof(save_screen_command_t) == 4,
               "i386 save-screen command alignment changed");
_Static_assert(offsetof(save_screen_command_t, commandId) == 0x00,
               "i386 save-screen command id moved");
_Static_assert(sizeof(save_screen_command_t) == 0x04,
               "i386 save-screen command size changed");
_Static_assert(_Alignof(blend_saved_screen_command_t) == 4,
               "i386 blend-saved-screen command alignment changed");
_Static_assert(offsetof(blend_saved_screen_command_t, commandId) == 0x00,
               "i386 blend-saved-screen command id moved");
_Static_assert(offsetof(blend_saved_screen_command_t, duration) == 0x04,
               "i386 blend-saved-screen duration moved");
_Static_assert(sizeof(blend_saved_screen_command_t) == 0x08,
               "i386 blend-saved-screen command size changed");
_Static_assert(_Alignof(swapBuffersCommand_t) == 4,
               "i386 swap-buffers command alignment changed");
_Static_assert(offsetof(swapBuffersCommand_t, commandId) == 0x00,
               "i386 swap-buffers command id moved");
_Static_assert(sizeof(swapBuffersCommand_t) == 0x04,
               "i386 swap-buffers command size changed");
_Static_assert(_Alignof(text_paint_command_t) == 4,
               "i386 text-paint command alignment changed");
_Static_assert(offsetof(text_paint_command_t, commandId) == 0x00,
               "i386 text-paint command id moved");
_Static_assert(offsetof(text_paint_command_t, x) == 0x04,
               "i386 text-paint command X moved");
_Static_assert(offsetof(text_paint_command_t, y) == 0x08,
               "i386 text-paint command Y moved");
_Static_assert(offsetof(text_paint_command_t, fontHandle) == 0x0c,
               "i386 text-paint command font moved");
_Static_assert(offsetof(text_paint_command_t, scale) == 0x10,
               "i386 text-paint command scale moved");
_Static_assert(offsetof(text_paint_command_t, color) == 0x14,
               "i386 text-paint command color moved");
_Static_assert(offsetof(text_paint_command_t, fixedAdvance) == 0x18,
               "i386 text-paint command fixed advance moved");
_Static_assert(offsetof(text_paint_command_t, textStyle) == 0x1c,
               "i386 text-paint command style moved");
_Static_assert(offsetof(text_paint_command_t, cursorPosition) == 0x20,
               "i386 text-paint command cursor position moved");
_Static_assert(offsetof(text_paint_command_t, cursorCharacter) == 0x24,
               "i386 text-paint command cursor character moved");
_Static_assert(offsetof(text_paint_command_t, text) == 0x25,
               "i386 text-paint command text offset changed");
_Static_assert(sizeof(text_paint_command_t) == 0x28,
               "i386 text-paint command minimum extent changed");
_Static_assert(sizeof(renderer_dynamic_buffer_allocation_t) == 0x08,
               "i386 dynamic-buffer allocation size changed");
_Static_assert(offsetof(renderer_dynamic_buffer_allocation_t, offset) == 0x00,
               "i386 dynamic-buffer allocation offset field moved");
_Static_assert(offsetof(renderer_dynamic_buffer_allocation_t, size) == 0x04,
               "i386 dynamic-buffer allocation size field moved");
_Static_assert(sizeof(renderer_dynamic_buffer_storage_t) == 0x04,
               "i386 dynamic-buffer storage width changed");
_Static_assert(offsetof(renderer_dynamic_buffer_t, storage) == 0x00,
               "i386 dynamic-buffer storage offset changed");
_Static_assert(offsetof(renderer_dynamic_buffer_t, capacity) == 0x04,
               "i386 dynamic-buffer capacity offset changed");
_Static_assert(offsetof(renderer_dynamic_buffer_t, currentOffset) == 0x08,
               "i386 dynamic-buffer write offset changed");
_Static_assert(offsetof(renderer_dynamic_buffer_t, frameSerial) == 0x0c,
               "i386 dynamic-buffer frame serial moved");
_Static_assert(offsetof(renderer_dynamic_buffer_t, unused010) == 0x10,
               "i386 dynamic-buffer unused slot moved");
_Static_assert(offsetof(renderer_dynamic_buffer_t, allocationSequence) == 0x14,
               "i386 dynamic-buffer allocation sequence moved");
_Static_assert(offsetof(renderer_dynamic_buffer_t, reclaimSequence) == 0x18,
               "i386 dynamic-buffer reclaim sequence moved");
_Static_assert(offsetof(renderer_dynamic_buffer_t, freeBytes) == 0x1c,
               "i386 dynamic-buffer free-byte count moved");
_Static_assert(offsetof(renderer_dynamic_buffer_t, allocations) == 0x20,
               "i386 dynamic-buffer allocation offset changed");
_Static_assert(sizeof(renderer_dynamic_buffer_t) == 0x1020,
               "i386 dynamic-buffer size changed");
#endif

#ifdef __cplusplus
extern "C" {
#endif

void RB_Hyperspace(void);
void RB_StageIteratorGeneric(qboolean portalPass);
void RB_StageIteratorGenericARB(qboolean portalPass);
void RB_StageIteratorGenericATI(qboolean portalPass);
void RB_StageIteratorGenericNV(qboolean portalPass);
void RB_IterateStagesGeneric(void);
void RB_IterateStagesGenericATI(void);
void RB_IterateStagesGenericNV(void);
void RB_IterateStagesGenericARB(void);
void ProjectDlightTexture(void);
void ProjectDlightTextureATI(void);
void ProjectDlightTextureNV(void);
void ProjectDlightTextureARB(void);
int32_t RB_PickBufferOffsetATI(int32_t *currentOffset, int32_t size,
                               int32_t capacity);
int32_t RB_PickBufferOffsetARB(int32_t *currentOffset, int32_t size,
                               int32_t capacity);
void RB_SingleStageGenericATI(shaderStage_t *stage, int32_t indexCount,
                              const uint16_t *indexes);
void RB_SingleStageGenericNV(shaderStage_t *stage, int32_t indexCount,
                             const uint16_t *indexes);
void RB_SingleStageGenericARB2(shaderStage_t *stage, int32_t indexCount,
                               const uint16_t *indexes);
void RB_SingleStageGenericARB(shaderStage_t *stage, int32_t indexCount,
                              const uint16_t *indexes);
void R_Init(void);
void R_ModelInit(void);
void RE_EndRegistration(void);
void RE_Shutdown(qboolean destroyWindow);
void R_SyncRenderThread(void);
void RE_ClearFlares(void);
void RE_ClearScene(void);
void R_InitAllocators(void);
void R_SetColorMappings(void);
void GLimp_InitGamma(void);
void GLimp_SetGamma(const uint8_t red[256], const uint8_t green[256],
                    const uint8_t blue[256]);
void GLimp_RestoreGamma(void);
void RestoreSystemGammas(void);
void R_InitImages(void);
void R_CreateBuiltinImages(void);
void R_DeleteTextures(void);
void R_ColorShiftLightingBytes(const uint8_t input[4], uint8_t output[4]);
void R_CopyLightmap(uint8_t *destination, const uint8_t *source,
                    int32_t destinationX, int32_t destinationY,
                    int32_t destinationWidth);
int32_t R_BuildLightmapMergability(
    const lump_t *surfaceLump,
    int32_t atlasDimensions[R_MAX_LIGHTMAPS][2],
    int32_t lightmapOrder[R_MAX_LIGHTMAPS]);
void R_LoadLightmaps(
    const lump_t *lightmapLump,
    const lump_t *surfaceLump,
    renderer_lightmap_placement_t placements[R_MAX_LIGHTMAPS]);
renderer_vertex_program_t *R_LoadVertexProgram(const char *name);
renderer_vertex_program_t *R_FindVertexProgram(const char *name);
void R_DeleteVertexPrograms(void);
void R_InitVertexPrograms(void);
void R_SaveLightVisHistory(void);
void R_InitLightVisHistory(void);
void R_ShutdownAllocators(void);
void R_ShutdownStaticModels(void);
void GLimp_Shutdown(void);
void R_ShutdownDebug(void);
void R_InitShaders(void);
void R_InitFreeType(void);
void R_DoneFreeType(void);
void R_SetHwLightGlobals(void);
void R_ClearLightVisCache(void);
uint32_t reverse_bits(uint32_t value);
qboolean R_SkyTracePassed(const trace_t *trace);
uint8_t R_LightCacheSkyTrace(const vec3_t start, const vec3_t end);
qboolean R_LightCacheTrace(const vec3_t start, const vec3_t end);
float R_MaxLightIntensity(const vec3_t point,
                          const renderer_light_t *light);
void R_MergeLights(const vec3_t point, trRefEntity_t *entity,
                   float *contributions,
                   const int32_t *sortedIndices);
int32_t R_GetStaticLightContributions(
    const vec3_t point, float *contributions,
    float *diffuseSunContribution, renderer_light_t **lights);
void R_PickFinalLights(const trRefdef_t *refdef,
                       const vec3_t point, trRefEntity_t *entity,
                       int32_t lightCount, float *contributions,
                       float diffuseSunContribution,
                       renderer_light_t **lights, qboolean mergeLights);
void R_PickLights(const trRefdef_t *refdef, const vec3_t point,
                  trRefEntity_t *entity, qboolean mergeLights);
void R_SetupEntityLighting(const trRefdef_t *refdef,
                           trRefEntity_t *entity);
void R_SetupStaticModelLighting(const trRefdef_t *refdef,
                                trRefEntity_t *entity);
void R_LightVisHash(int32_t gridX, int32_t gridY, int32_t gridZ,
                    int32_t cluster,
                    uint32_t *cacheKey, uint32_t *bucketIndex);
uint32_t R_GetCachedVisibility(
    int32_t gridX, int32_t gridY, int32_t gridZ, int32_t cluster,
    int32_t lightCount, renderer_light_t *const *lights,
    const vec3_t traceTarget, uint8_t *diffuseSunVisibility);
int32_t R_SortedHistoryEntry(int32_t gridX, int32_t gridY,
                             int32_t gridZ, int32_t updateMode);
qboolean R_AddSortedHistoryEntry(
    int32_t insertIndex,
    const renderer_light_vis_sort_entry_t *entry);
void R_PrecalcLightVisCachePoint(
    int32_t gridX, int32_t gridY, int32_t gridZ,
    const vec3_t traceTarget,
    renderer_light_vis_disk_entry_t
        cache[R_LIGHT_VIS_BUCKET_COUNT][R_LIGHT_VIS_ENTRIES_PER_BUCKET]);
void R_PrecalcLightVisCache(int32_t *checksum);
qboolean R_InitLightVisCacheFromBuffer(
    const renderer_light_vis_disk_entry_t *diskCache,
    int32_t diskCacheSize);
mnode_t *R_PointInLeaf(const vec3_t point);
void R_SampleDiffuseSunVisibility(
    renderer_light_vis_cache_entry_t *cacheEntry,
    const vec3_t origin,
    const mnode_t *leaf);
mnode_t *R_SampleLightVisibility(
    renderer_light_vis_cache_entry_t *cacheEntry,
    const vec3_t origin,
    int32_t lightCount,
    renderer_light_t *const *lights,
    const vec3_t traceTarget);
void R_AddDebugLine(const vec3_t start, const vec3_t end,
                    const vec4_t color);
void R_AddDebugPolygon(const vec4_t color, int32_t pointCount,
                       const vec3_t *points);
void R_AddDebugString(const vec3_t origin, const vec4_t color,
                      float scale, const char *text);
void R_AddScaledDebugString(const vec3_t origin, const vec4_t color,
                            const char *text);
void R_AddDebugBox(const vec3_t mins, const vec3_t maxs,
                   const vec4_t color);
int32_t R_FurthestReplaceableRefEntity(void);
void R_SetSceneRefEntity(
    int32_t entityIndex, const refEntity_t *entity,
    renderer_static_model_t *staticLighting);
void RE_AddRefEntityToScene(
    const refEntity_t *entity,
    renderer_static_model_t *staticLighting);
void RB_AddPlumeStrings(void);
void RB_DrawDebugPolys(void);
void RB_DrawDebugLines(const renderer_debug_line_t *lines,
                       int32_t lineCount);
void RB_DrawDebugStrings(const renderer_debug_string_t *strings,
                         int32_t stringCount);
cull_result_t R_CullPointAndRadius(const vec3_t point, float radius);
cull_result_t R_CullLocalBox(const vec3_t bounds[2]);
cull_result_t R_CullLocalPointAndRadius(const vec3_t point, float radius);
void R_LocalNormalToWorld(const vec3_t local, vec3_t world);
void R_LocalPointToWorld(const vec3_t local, vec3_t world);
void R_ShowLeafLights(const vec3_t point, const trRefEntity_t *entity);
void R_ShowLightVisCachePoints(void);
void R_VC_Stats_f(void);
void R_LightVisHistoryFilename(char *filename);
void R_InitDebug(void);
void R_InitWater(void);
void FFT_Init(void);
void FFT(renderer_water_complex_t *data, int32_t log2Count,
         int32_t stride);
void GaussianRandom(float *real, float *imaginary);
void PickWaterFrequencies(shader_water_map_t *waterMap);
void WaterFrequenciesAtTime(renderer_water_complex_t *frequencies,
                            const shader_water_map_t *waterMap,
                            float timeSeconds);
void WaterAmplitudesFromFrequencies(
    renderer_water_complex_t *amplitudes,
    const shader_water_map_t *waterMap);
void WaterNormalsFromAmplitudes(uint32_t *normalPixels,
                                renderer_water_complex_t *amplitudes,
                                const shader_water_map_t *waterMap);
qboolean WatersEquivalent(const shader_water_map_t *first,
                          const shader_water_map_t *second);
shader_water_map_t *R_GetWaterTexture(
    const shader_water_map_t *configuration);
image_t *R_CreateImage(const char *name, uint8_t *pixels,
                       int32_t width, int32_t height, uint32_t format,
                       uint32_t flags, int32_t imageTrack,
                       const float *colorScale);
int32_t SmallestTextureSizeFitting(int32_t dimension);
int32_t generateHashValue(const char *name);
image_t *R_GetImageByIndex(int32_t imageIndex);
uint32_t R_GenerateShaderHashValue(const char *name);
void BuildShaderChecksumLookup(void);
void ScanAndLoadShaderFiles(void);
void CreateDefaultShader(void);
void CreateShadowShader(void);
void CreateShowTrisShader(void);
void CreateShowImagesShader(void);
void CreateScreenShader(void);
void CreateInternalShaders(void);
void CreateExternalShaders(void);
char *FindShaderInShaderText(const char *shaderName);
qboolean MergableShader(shader_t *shader);
void UpdateDelayLoadImagesForShader(shader_t *shader, qboolean forceLoad);
int32_t CompareTexEnvCombineArgs(
    const shader_texture_combine_args_t *first,
    const shader_texture_combine_args_t *second);
int32_t CompareTexEnvCombine(const shader_texture_combine_t *first,
                             const shader_texture_combine_t *second);
int32_t CompareNvTexShaders(const shader_texture_shader_t *first,
                            const shader_texture_shader_t *second);
int32_t CompareNvRegisterCombiners(
    const renderer_register_combiners_t *first,
    const renderer_register_combiners_t *second);
int32_t CompareWaveForms(const waveForm_t *first,
                         const waveForm_t *second);
int32_t CompareTextureBundles(
    const textureBundle_t *first,
    const textureBundle_t *second,
    const image_t *firstEquivalentImage,
    const image_t *secondEquivalentImage);
qboolean MatchShaderToken(char **text, const char *expected,
                          const char *functionName);
qboolean MatchShaderTokenOnLine(char **text, const char *expected,
                                const char *functionName);
qboolean ParseVector(char **text, int32_t componentCount, float *values);
uint32_t NameToAFunc(const char *name);
uint32_t NameToSrcBlendMode(const char *name);
uint32_t NameToDstBlendMode(const char *name);
shader_wave_func_t NameToGenFunc(const char *name);
void ParseWaveForm(char **text, waveForm_t *waveform);
float MaxWaveFormDeformation(const waveForm_t *waveform);
void ParseDeform(char **text);
void ParseSkyParms(char **text, renderer_image_track_t imageTrack);
qboolean ParseSort(char **text);
void ParseSurfaceParm(char **text);
void ParseTexMod(shaderStage_t *stage, int32_t bundleIndex, char **text);
qboolean ParseTexEnvCombineFunction(
    shader_texture_combine_args_t *arguments,
    qboolean alphaChannel, char **text);
qboolean ParseTextureEnvCombine(
    textureBundle_t *bundle, char **text);
qboolean ParseImage(
    char **text, qboolean allowTextureName, qboolean loadImage,
    renderer_image_track_t imageTrack, const float colorScale[4],
    image_t **outImage);
float ParseWaterMapPositiveFloat(
    const char *parameterName, char **text, float *value);
float ParseWaterMapFloat(
    const char *parameterName, char **text, float *value);
qboolean ParseWaterMapInt(
    const char *parameterName, char **text, int32_t minimum,
    int32_t maximum, int32_t *value);
qboolean ParseWaterMap(
    shaderStage_t *stage, char **text, int32_t bundleIndex);
qboolean ParseNVCullParm(
    shaderStage_t *stage, char **text, int32_t bundleIndex,
    int32_t parameterIndex);
qboolean ParseNVFloatParm(
    shaderStage_t *stage, char **text, int32_t bundleIndex,
    int32_t parameterIndex);
qboolean ParseNVTextureInput(
    shaderStage_t *stage, char **text, int32_t bundleIndex,
    qboolean allowExpandMapping);
qboolean ParseNVTexShaderArgs(
    shaderStage_t *stage, char **text, int32_t bundleIndex,
    int32_t argumentCount,
    const nv_texture_shader_argument_type_t *argumentTypes);
qboolean ParseNVTexShader(
    shaderStage_t *stage, char **text, int32_t bundleIndex);
qboolean ParseNVRC_ConstColor(
    char **text, vec4_t constantColors[2], qboolean *finished);
qboolean ParseNVRC_ConstColors(
    char **text, vec4_t constantColors[2],
    qboolean perStageConstants);
qboolean ParseNVRC_RegisterFromTable(
    const char *name,
    const renderer_nv_register_definition_t *definitions,
    int32_t definitionCount, uint32_t *registerName,
    uint32_t *componentUsage, uint32_t defaultComponentUsage);
qboolean ParseNVRC_ReadWriteRegister(
    char **text, uint32_t *registerName, uint32_t *componentUsage,
    uint32_t defaultComponentUsage, qboolean *valid);
qboolean ParseNVRC_GeneralOutputRegister(
    char **text, uint32_t *registerName,
    uint32_t requiredComponentUsage, qboolean *valid);
qboolean ParseNVRC_ReadRegister(
    char **text, uint32_t *registerName, uint32_t *componentUsage,
    uint32_t defaultComponentUsage);
qboolean ParseNVRC_FinalRegister(
    char **text, uint32_t *registerName, uint32_t *componentUsage,
    uint32_t defaultComponentUsage);
qboolean ParseNVRC_GeneralMappedRegister(
    char **text, renderer_combiner_input_t *input,
    uint32_t defaultComponentUsage);
qboolean ParseNVRC_FinalMappedRegister(
    char **text, renderer_combiner_input_t *input,
    uint32_t defaultComponentUsage);
qboolean ParseNVRC_SingleGeneralFunction(
    char **text, renderer_combiner_portion_t *portion,
    uint32_t componentUsage, int32_t functionIndex,
    qboolean *valid);
qboolean ParseNVRC_GeneralFunction(
    char **text, renderer_combiner_portion_t *portion,
    uint32_t componentUsage);
qboolean ParseNVRC_GeneralPortion(
    char **text, renderer_general_combiner_t *combiner,
    qboolean *valid);
qboolean ParseNVRC_General(
    char **text, renderer_general_combiner_t *combiner);
qboolean ParseNVRC_FinalMulSum(
    char **text, renderer_final_combiner_t *combiner);
qboolean ParseNVRC_FinalLerp(
    char **text, renderer_final_combiner_t *combiner);
qboolean ParseNVRC_FinalRgbFunc(
    char **text, renderer_final_combiner_t *combiner);
qboolean ParseNVRC_FinalRgbAlpha(
    char **text, renderer_final_combiner_t *combiner);
qboolean ParseNVRC_Final(
    char **text, renderer_final_combiner_t *combiner);
void ParseNVRC_ClearGeneralCombinerPortion(
    renderer_combiner_portion_t *portion, uint32_t componentUsage);
qboolean ParseNVRegCombiners(
    shaderStage_t *stage, char **text);
uint32_t ParseATIFS_ConstReg(char **text);
uint32_t ParseATIFS_Reg(char **text);
uint32_t ParseATIFS_TexCoord(char **text);
qboolean ParseATIFS_ConstDefs(
    char **text,
    renderer_atifs_constant_definition_t *constantDefinitions);
qboolean ParseATIFS_TexReads(
    char **text, renderer_atifs_texture_read_t *textureReads,
    qboolean allowTemporaryRegisterSource);
void ParseATIFS_DestReg(
    char **text, uint32_t *destination, uint32_t *rgbWriteMask,
    qboolean *writesRgb, qboolean *writesAlpha);
qboolean ParseATIFS_ArgReg(
    char **text, renderer_atifs_argument_t *argument);
qboolean ParseATIFS_FunctionArgs(
    char **text, renderer_atifs_argument_t *arguments,
    int32_t argumentCount,
    qboolean *usesPrimaryOrSecondaryColor);
qboolean ParseATIFS_FragOps(
    char **text, renderer_atifs_operation_pair_t *operationPairs,
    qboolean *usesPrimaryOrSecondaryColor);
qboolean ParseATIFS_Phase(
    char **text, renderer_atifs_phase_t *phase,
    qboolean allowTemporaryRegisterSource,
    qboolean *usesPrimaryOrSecondaryColor);
void ATIFS_ColorOp(
    const renderer_atifs_instruction_t *instruction);
void ATIFS_AlphaOp(
    const renderer_atifs_instruction_t *instruction);
qboolean ParseATIFragmentShader(
    shaderStage_t *stage, char **text,
    qboolean compileShader);
qboolean ParseVertexProgram(
    shaderStage_t *stage, char **text);
qboolean ParseStageRequirementsOperand(
    char **text, shader_requirement_operand_t *operand);
void UpdateRequiresCondition(
    shader_requirement_operator_t operatorKind,
    const shader_requirement_operand_t *first,
    const shader_requirement_operand_t *second);
qboolean ParseStageRequirements(
    char **text, qboolean suppressDependencyUpdates);
qboolean ParseStage(
    shaderStage_t *stage, char **text,
    qboolean allowTextureName, renderer_image_track_t imageTrack,
    qboolean *outStageActive);
qboolean ParseShader(char **text, qboolean allowTextureName,
                     renderer_image_track_t imageTrack);
void ComputeHardwareNeeds(void);
void ComputeStageIteratorFunc(void);
void CreateDlightStage(void);
qboolean CollapseMultitexture(void);
void SortNewShader(void);
shader_t *FinishShader(void);
image_t *R_AllocImage(const char *name, uint32_t target,
                      int32_t width, int32_t height, uint32_t flags,
                      int32_t imageTrack);
void R_DeleteImage(image_t *image);
void R_FreeImage(image_t *image);
qboolean R_CreateImageInternal(image_t *image, uint8_t *pixels,
                               uint32_t uploadTarget, uint32_t format,
                               const float *colorScale);
uint32_t PickInternalFormat(const uint8_t *pixels, uint32_t format,
                            int32_t width, int32_t height,
                            uint32_t flags, qboolean isLightmap);
int32_t GetCardMemoryAmount(uint32_t internalFormat,
                            int32_t width, int32_t height);
typedef enum renderer_image_allocation_kind_e {
    R_IMAGE_ALLOCATION_FILE = 0,
    R_IMAGE_ALLOCATION_TEMP_MEMORY = 1
} renderer_image_allocation_kind_t;

/* R_LoadImage's final argument is not a qboolean. LoadImageTile passes 2 so
 * portrait TGA tiles are transposed into landscape orientation while they are
 * decoded; ordinary image and cube-map loads pass 1, and precache probes pass
 * 0 to read only format metadata. */
typedef enum renderer_image_load_mode_e {
    R_IMAGE_LOAD_METADATA = 0,
    R_IMAGE_LOAD_PIXELS = 1,
    R_IMAGE_LOAD_TILE = 2
} renderer_image_load_mode_t;
void R_RememberImageAllocation(void *memory,
                               renderer_image_allocation_kind_t kind);
void R_ResetImageAllocations(void);
void R_FreeImageAllocations(void);
void *R_AllocTempMemory(size_t size);
int32_t R_ReadFile(const char *name, void **buffer);
void R_TexImage2D(uint32_t target, int32_t level,
                  uint32_t internalFormat, int32_t width, int32_t height,
                  uint32_t format, const uint8_t *pixels);
void R_BlendOverTexture_RGBA(uint8_t *pixels, int32_t pixelCount,
                             const uint8_t blendColor[4]);
void R_BlendOverTexture_S3TC(uint8_t *blocks, int32_t pixelCount,
                             const uint8_t blendColor[4],
                             int32_t blockStride);
void R_BlendOverTexture(uint8_t *pixels, int32_t pixelCount,
                        const uint8_t blendColor[4], uint32_t format);
void R_LightScaleTexture(uint8_t *pixels, int32_t width, int32_t height,
                         qboolean onlyGamma, qboolean noOverbright,
                         uint32_t format);
void R_MipMap8(uint8_t *pixels, int32_t width, int32_t height);
void R_MipMap2(uint8_t *pixels, int32_t width, int32_t height);
uint8_t *R_MipMap(char *name, uint8_t *pixels,
                  int32_t width, int32_t height, uint32_t format,
                  renderer_image_load_mode_t loadMode);
void R_FlipImageDiagonally(uint8_t *pixels, int32_t width, int32_t height);
void R_FlipImageHorizontally(uint8_t *pixels, int32_t width, int32_t height);
void R_FlipImageVertically(uint8_t *pixels, int32_t width, int32_t height);
const char *R_MangleTextureName(const char *name, uint32_t textureTarget,
                                uint32_t flags, const float colorScale[4],
                                float heightScale);
void R_UnmangleTextureName(const char *originalName,
                           const char *mangledName, char *outputName,
                           uint32_t *textureTarget, uint32_t *flags,
                           float colorScale[4], float *heightScale);
void R_HeightmapImage(uint8_t *pixels, int32_t width, int32_t height,
                      float heightScale);
image_t *R_FindExistingImage(const char *name, uint32_t textureTarget,
                             uint32_t flags,
                             renderer_image_track_t imageTrack);
void R_LoadSingleDelayedImage(image_t *image);
void R_UpdateDelayLoadImage(image_t *image, shader_t *shader,
                            qboolean forceLoad);
int32_t CompareMergableShaders(const shader_t *left,
                               const shader_t *right,
                               const image_t *leftImage,
                               const image_t *rightImage);
image_t *R_FindImageFile(const char *name, uint32_t textureTarget,
                         uint32_t flags,
                         renderer_image_track_t imageTrack,
                         const float colorScale[4], float heightScale);
image_t *R_FindImageInstance(const char *originalName,
                             renderer_image_track_t imageTrack,
                             const char *mangledName, shader_t *shader,
                             qboolean forceLoad);
image_t *R_LoadCubeMapImage(const char *name, uint32_t flags,
                            renderer_image_track_t imageTrack,
                            const float colorScale[4]);
void CopyImageTile_RGBA(const image_t *destinationImage,
                        uint8_t *destinationPixels,
                        const uint8_t *sourcePixels,
                        uint16_t destinationX, uint16_t destinationY,
                        uint16_t sourceWidth, uint16_t sourceHeight);
void CopyImageTileLevel_DXT1(uint16_t destinationWidth,
                             uint8_t *destinationPixels,
                             const uint8_t *sourcePixels,
                             uint16_t destinationX, uint16_t destinationY,
                             uint16_t sourceWidth, uint16_t sourceHeight);
void CopyImageTile_DXT1(const image_t *destinationImage,
                        uint8_t *destinationPixels,
                        const uint8_t *sourcePixels,
                        uint16_t destinationX, uint16_t destinationY,
                        uint16_t sourceWidth, uint16_t sourceHeight);
void CopyImageTileLevel_DXT3(uint16_t destinationWidth,
                             uint8_t *destinationPixels,
                             const uint8_t *sourcePixels,
                             uint16_t destinationX, uint16_t destinationY,
                             uint16_t sourceWidth, uint16_t sourceHeight);
void CopyImageTile_DXT3(const image_t *destinationImage,
                        uint8_t *destinationPixels,
                        const uint8_t *sourcePixels,
                        uint16_t destinationX, uint16_t destinationY,
                        uint16_t sourceWidth, uint16_t sourceHeight);
void CopyImageTileLevel_DXT5(uint16_t destinationWidth,
                             uint8_t *destinationPixels,
                             const uint8_t *sourcePixels,
                             uint16_t destinationX, uint16_t destinationY,
                             uint16_t sourceWidth, uint16_t sourceHeight);
void CopyImageTile_DXT5(const image_t *destinationImage,
                        uint8_t *destinationPixels,
                        const uint8_t *sourcePixels,
                        uint16_t destinationX, uint16_t destinationY,
                        uint16_t sourceWidth, uint16_t sourceHeight);
void LoadImageTile(image_t *image, image_t *destinationImage,
                   uint8_t *destinationPixels,
                   uint8_t *destinationPixelsEnd,
                   uint16_t destinationX, uint16_t destinationY);
void UploadImageGroup_r(renderer_image_group_node_t *group,
                        image_t *destinationImage,
                        uint8_t *destinationPixels,
                        uint8_t *destinationPixelsEnd,
                        uint16_t destinationX, uint16_t destinationY);
void UploadImageGroup(renderer_image_group_node_t *group,
                      int32_t groupIndex, uint32_t format, uint32_t flags);
int compare_mergable_common(const renderer_image_group_node_t *left,
                            const renderer_image_group_node_t *right);
int compare_mergable_grouped(const void *leftElement,
                             const void *rightElement);
int compare_mergable_ungrouped(const void *leftElement,
                               const void *rightElement);
renderer_image_merge_direction_t PickMergeDirection(
    renderer_image_group_node_t *const *groups,
    int32_t groupCount, int32_t groupIndex);
int32_t MergeImageList(renderer_image_group_node_t **groups,
                       int32_t groupCount,
                       renderer_image_group_node_t *mergeNodes,
                       int32_t mergeNodeCount);
int32_t CombineImageGroups(renderer_image_group_node_t **groups,
                           int32_t *groupCount);
int compare_image_types(const void *leftElement, const void *rightElement);
void MergeAndLoadDelayedImages(image_t **images, int32_t imageCount,
                               uint32_t format, uint32_t flags);
void R_LoadImage(const char *name, uint8_t **pixels,
                 uint16_t *width, uint16_t *height, uint32_t *format,
                 qboolean *mipMapsAvailable,
                 renderer_image_load_mode_t loadMode);
void LoadDDS(const char *name, uint8_t **pixels,
             uint16_t *width, uint16_t *height, uint32_t *format,
             qboolean *mipMapsAvailable,
             renderer_image_load_mode_t loadMode);
void LoadTGA(const char *name, uint8_t **pixels,
             uint16_t *width, uint16_t *height, uint32_t *format,
             renderer_image_load_mode_t loadMode);
/* Pending original decoder body. */
void LoadJPG(const char *name, uint8_t **pixels,
             uint16_t *width, uint16_t *height, uint32_t *format,
             renderer_image_load_mode_t loadMode);
void TransposeDDSBlockDXT1(const uint8_t source[8],
                           uint8_t destination[8]);
void TransposeDDSBlockDXT3(const uint8_t source[16],
                           uint8_t destination[16]);
void TransposeDDSBlockDXT5(const uint8_t source[16],
                           uint8_t destination[16]);
qboolean UploadImage(const char *name, uint8_t *pixels,
                     uint32_t textureTarget, uint32_t uploadTarget,
                     uint32_t format, int32_t width, int32_t height,
                     uint32_t flags, qboolean isLightmap,
                     uint32_t *internalFormat, uint16_t *uploadWidth,
                     uint16_t *uploadHeight, int32_t *cardMemory,
                     int32_t *textureMemory);
void GL_BindFragmentShaderATI(uint32_t shader);
void R_DeleteFragmentShaders(void);
void AssertCvarRange(cvar_t *cvar, float minimum, float maximum,
                     qboolean integral);
void InitOpenGL(void);
void GL_SetupVBO(void);
void GL_CheckErrors(const char *location);
qboolean R_GetModeInfo(int32_t *width, int32_t *height,
                       float *windowAspect, int32_t mode);
void R_ModeList_f(void);
void R_SetNVFogMode(void);
void GL_TextureMode(const char *textureMode);
void R_ScreenshotFilename(int32_t screenshotNumber, char *fileName);
void R_ScreenshotFilenameJPEG(int32_t screenshotNumber, char *fileName);
void R_ScreenShot_f(void);
void R_ScreenShotJPEG_f(void);
void R_LevelShot(void);
void R_SaveGameShot(const char *name);

typedef enum cubemap_face_e {
    CUBEMAP_FACE_UP = 1,
    CUBEMAP_FACE_DOWN = 2,
    CUBEMAP_FACE_LEFT = 3,
    CUBEMAP_FACE_RIGHT = 4,
    CUBEMAP_FACE_FRONT = 5,
    CUBEMAP_FACE_BACK = 6
} cubemap_face_t;

void RE_CubemapShot(const char *fileName, int32_t faceSize,
                    cubemap_face_t face,
                    float fresnelN0, float fresnelN1);
void RE_CubemapWaterShot(const char *fileName, int32_t faceSize,
                         cubemap_face_t face,
                         const vec3_t horizonColor,
                         const vec3_t zenithColor);
void R_GammaCorrect(uint8_t *buffer, int32_t byteCount);
void R_TakeScreenshot(int32_t x, int32_t y,
                      int32_t width, int32_t height,
                      const char *fileName);
void R_TakeScreenshotJPEG(int32_t x, int32_t y,
                          int32_t width, int32_t height,
                          const char *fileName);
void SaveJPG(const char *fileName, int32_t quality,
             int32_t imageWidth, int32_t imageHeight,
             uint8_t *imageBuffer, qboolean flipVertical);
void GfxInfo_f(void);
void R_ImageList_f(void);
void R_ShaderList_f(void);
shader_t *R_GetShaderByHandle(int32_t shaderHandle);
void R_MemInfo_f(void);
void R_VC_Stats_f(void);
void R_StaticModelCacheStats_f(void);
void R_StaticModelCacheFlush_f(void);
void R_InitStaticModelCache(void);
void R_UsedCachedStaticModelSurface(
    renderer_cached_static_model_surface_t *surface);
renderer_cached_static_model_surface_t *R_CacheStaticModelSurface(
    renderer_static_model_surface_t *source,
    renderer_static_model_t *owner,
    int32_t surfaceIndex,
    trRefEntity_t *entity,
    const axis_t inverseAxis);
void R_LoadSun_f(void);
void R_SaveSun_f(void);
void R_SunHelp_f(void);
void R_LoadSunThroughCvars(const char *sunName);
void R_SaveSunFromCvars(const char *sunName);
void R_VboRefresh_f(void);
void R_Register(void);
void R_RefreshOptimizedWorldSurfaces_ARB(void);
void R_IncrementalRefreshOptimizedWorldSurfaces_ARB(void);
void OptimizeVertices_T2T2C4V3_GenericOrNV(
    const renderer_world_mesh_surface_t *surface,
    renderer_world_interleaved_vertex_t *vertices);
void OptimizeVertices_T2T2C4V3_ARB(
    const renderer_world_mesh_surface_t *surface,
    uint32_t vertexBuffer, size_t vertexOffset);
void OptimizeVertices_T2T2C4V3_ATI(
    const renderer_world_mesh_surface_t *surface,
    uint32_t objectBuffer, size_t vertexOffset);
void AdjustColors(renderer_world_mesh_surface_t *surface,
                  const msurface_t *worldSurface);
void LittleVertices_T2T2C4V3(
    const drawVert_t *vertices, int32_t vertexCount,
    renderer_world_mesh_surface_t *surface,
    const renderer_lightmap_placement_t *lightmapPlacement,
    int32_t firstVertex);
qboolean CanOptimizeShader(const shader_t *shader);
void BeginShaderSurfaces(shader_t *shader, int32_t vertexCount,
                         renderer_shader_surface_build_t *build);
qboolean BuildOptimizedSurface(
    msurface_t *worldSurface,
    renderer_shader_surface_build_t *build,
    const renderer_lightmap_placement_t *lightmapPlacement,
    int32_t vertexCount, const drawVert_t *vertices,
    int32_t indexCount, const int16_t *indices);
void ParseTriangleSoup(
    const dsurface_t *diskSurface,
    renderer_shader_surface_build_t *build,
    const renderer_lightmap_placement_t *lightmapPlacements,
    const drawVert_t *vertices,
    msurface_t *worldSurface,
    const int16_t *indices);
void R_LoadSurfaces(
    const lump_t *surfaceLump,
    const lump_t *vertexLump,
    const lump_t *indexLump,
    const renderer_lightmap_placement_t *lightmapPlacements);
void R_LoadSubmodels(const lump_t *submodelLump);
void R_SetParentAndCell(mnode_t *node,
                        mnode_t *parent);
void R_LoadNodesAndLeafs(const lump_t *nodeLump,
                         const lump_t *leafLump);
void R_LoadShaders(const lump_t *shaderLump);
void R_LoadLights(const lump_t *lightLump);
void R_LoadLightIndexes(const lump_t *lightIndexLump);
void R_LoadLightVisCache(const lump_t *lightVisLump);
void R_LoadEntities(const lump_t *entityLump);
qboolean R_GetEntityToken(char *buffer, int32_t bufferSize);
int32_t R_FinishLoadingAABBTrees_r(renderer_aabb_tree_t *tree,
                                   int32_t nextTreeIndex);
void R_LoadAABBTrees(const lump_t *aabbTreeLump);
void R_LoadCells(const lump_t *cellLump);
void R_LoadPortalVerts(const lump_t *portalVertexLump);
void R_LoadPortals(const lump_t *portalLump);
void R_LoadCullGroups(const lump_t *cullGroupLump);
void R_LoadCullGroupIndexes(const lump_t *indexLump);
void R_LoadOccluders(const lump_t *occluderLump,
                     const lump_t *planeIndexLump,
                     const lump_t *edgeLump);
void R_LoadOccluderIndexes(const lump_t *indexLump);
void RE_LoadWorldMap(const char *name, int32_t *checksum);
void R_RefreshStaticModels_ARB(
    renderer_vbo_refresh_components_t refreshComponents);
void R_IncrementalRefreshStaticModels_ARB(
    renderer_vbo_refresh_components_t refreshComponents);
void R_IncrementalRefreshXModels_ARB(
    renderer_vbo_refresh_components_t refreshComponents);
void R_RefreshXModels_ARB(
    renderer_vbo_refresh_components_t refreshComponents);
void GL_SetDefaultState(void);
void GLimp_Init(void);
void GLimp_Extensions(void);
qboolean GLW_ValidateOpenGLStrings(void);
void SetViewportAndScissor(void);
void RB_BeginDrawingView(void);
void RB_SetupLight(const renderer_light_t *light, int32_t lightIndex,
                   float scale);
qboolean RB_EnableHWLights(void);
void RB_RenderDrawSurfList(const drawSurf_t *drawSurfs,
                           int32_t drawSurfCount);
void R_ChopPolyBehindPlane(
    int32_t inPointCount, const renderer_mark_clip_vertex_t *inPoints,
    int32_t *outPointCount, renderer_mark_clip_vertex_t *outPoints,
    const vec3_t planeNormal, float planeDistance, float epsilon);
qboolean R_AddMarkFragment(
    int32_t pointCount,
    renderer_mark_clip_vertex_t
        clipPoints[R_MARK_CLIP_BUFFER_COUNT][R_MARK_CLIP_MAX_VERTICES],
    int32_t clipPlaneCount, const vec3_t *clipPlaneNormals,
    const float *clipPlaneDistances, int32_t maxPoints,
    polyVert_t *pointBuffer, markFragment_t *fragment);
void R_AABBTreeSurfaces_r(
    const renderer_aabb_tree_t *tree, const vec3_t mins, const vec3_t maxs,
    msurface_t **surfaceBuffer, int32_t maxSurfaces,
    int32_t *surfaceCount);
void R_CellSurfaces(
    renderer_world_cell_t *cell, const vec3_t mins, const vec3_t maxs,
    msurface_t **surfaceBuffer, int32_t maxSurfaces,
    int32_t *surfaceCount);
void R_BoxSurfaces_r(
    const mnode_t *node, const vec3_t mins, const vec3_t maxs,
    msurface_t **surfaceBuffer, int32_t maxSurfaces,
    int32_t *surfaceCount);
void R_AddStaticModelToCell(world_t *world,
                            renderer_static_model_t *model,
                            int32_t cellIndex);
void R_FilterStaticModelIntoCells_r(
    world_t *world, const mnode_t *node,
    renderer_static_model_t *model, const vec3_t mins, const vec3_t maxs);
void R_OptimizeSModelSurfGeneric(renderer_static_model_surface_t *surface);
void R_OptimizeSModelSurfNV(renderer_static_model_surface_t *surface);
qboolean R_OptimizeSModelSurfATI(renderer_static_model_surface_t *surface);
qboolean R_OptimizeSModelSurfARB(renderer_static_model_surface_t *surface);
renderer_registered_static_model_t *R_SetupDObjToStaticModel(
    const char *name, const DObj *obj);
void R_FinishDObjToStaticModel(
    renderer_registered_static_model_t *staticModel, const DObj *obj);
qboolean R_NeedsBoundsAdjustment(const refEntity_t *entity);
void R_AdjustBoundsForAutosprite(const refEntity_t *entity,
                                 vec3_t mins, vec3_t maxs);
renderer_registered_static_model_t *R_RegisterStaticModel(
    const char *name, const DObj *obj);
void R_CreateStaticModel(const char *name, const vec3_t origin,
                         const vec3_t angles, const vec3_t scale,
                         const vec3_t lightingPrecalc);
void R_AddStaticModelToWorld(renderer_static_model_instance_t *instance);
shader_t *R_CacheableStaticModelShader(const shader_t *shader);
qboolean R_CanOptimizeStaticModelStage(
    const shaderStage_t *stage, const shaderStage_t *firstStage);
void CloneShader(const shader_t *shader);
shader_t *R_FindShaderByName(const char *name);
shader_t *R_LoadShaderType(const char *shaderTypeName,
                           renderer_image_track_t imageTrack);
shader_t *GeneratePermanentShader(void);
void R_RemapTextureCoordinatesForSheet(shader_t *shader,
                                       int32_t vertexCount,
                                       vec2_t *texCoords);
const void *RB_DrawSurfs(const drawSurfsCommand_t *command);
const void *RB_DrawBuffer(const drawBufferCommand_t *command);
const void *RB_SaveScreen(const save_screen_command_t *command);
const void *RB_BlendSavedScreen(
    const blend_saved_screen_command_t *command);
const void *RB_SwapBuffers(const swapBuffersCommand_t *command);
void RB_ExecuteRenderCommands(const void *data);
void RB_ShowImages(void);
void GLimp_EndFrame(void);
void GLimp_LogComment(const char *comment);
void GLW_MissingDriverFeatureError(const char *featureErrorKey);
void RB_SetGL2D(void);
void RB_DrawDebug(void);
void RB_EndMultitexture(void);
const void *RB_SetColor(const setColorCommand_t *command);
const void *RB_StretchPic(const stretchPicCommand_t *command);
const void *RB_StretchPicGradient(
    const stretch_pic_gradient_command_t *command);
const void *RB_StretchPicRotate(const stretch_pic_rotate_command_t *command);
const void *RB_DrawQuadPic(const draw_quad_pic_command_t *command);
void RB_DrawStretchPic(shader_t *shader,
                       float x, float y, float width, float height,
                       float s1, float t1, float s2, float t2,
                       const renderer_rgba8_t *color);
void RB_LookupColor(uint8_t colorCode, renderer_rgba8_t *outColor);
void RB_UpdateColorInternal(const float color[4], uint8_t outColor[4]);
void RB_UpdateColor(const float color8[4], const float color9[4]);
void RB_Text_PaintChar(int32_t shaderHandle,
                       float x, float y, float width, float height, float scale,
                       float s1, float t1, float s2, float t2,
                       const renderer_rgba8_t *color);
int32_t RB_Text_PaintWithCursor(int32_t fontHandle, const char *text,
                                float x, float y, float scale,
                                const renderer_rgba8_t *color,
                                int32_t cursorPosition,
                                uint8_t cursorCharacter,
                                float fixedAdvance, int32_t textStyle);
const void *RB_Text_Paint(const text_paint_command_t *command);
glyphInfo_t *R_GetCharacterGlyph(int32_t character,
                                    fontInfo_t *font);
int32_t R_GetAsianCode(int32_t character);
float R_GetAsianGlyphHeight(void);
float R_GetAsianScale(fontInfo_t *font, float scale);
float R_GetGlyphHorizAdvance(fontInfo_t *font, int32_t character);
void R_LoadAsianFont(int32_t loadMode);
fontInfo_t *R_GetFontInfo(int32_t fontHandle, float scale);
const char *RE_GetFontLanguageDAT(const char *fontDataName);
const char *RE_GetFontLanguageTGA(const char *shaderName);
int32_t Korean_CollapseKSC5601HangulCode(int32_t character);
qboolean Korean_ValidKSC5601HangulCodePacked(int32_t character);
qboolean Taiwanese_ValidBig5Code(int32_t character);
qboolean Taiwanese_IsTrailingPunctuation(int32_t character);
int32_t Taiwanese_CollapseBig5Code(int32_t character);
qboolean Japanese_ValidShiftJISCode(int32_t character);
qboolean Japanese_IsTrailingPunctuation(int32_t character);
int32_t Japanese_CollapseShiftJISCode(int32_t character);
qboolean Chinese_ValidGBCode(int32_t character);
qboolean Chinese_IsTrailingPunctuation(int32_t character);
int32_t Chinese_CollapseGBCode(int32_t character);
void R_IssueRenderCommands(qboolean runPerformanceCounters);
void R_SyncRenderThread(void);
void R_PerformanceCounters(void);
void R_ToggleSmpFrame(void);
void R_SumOfUsedImages(int32_t *imageMemory, int32_t *lightmapMemory,
                       int32_t *textureMemory);
void RE_RegisterFont(const char *name, int32_t pointSize,
                     fontInfo_t *font, int32_t loadMode);
int32_t RE_Text_Width(const char *text, int32_t fontHandle, float scale,
                      float fixedAdvance, int32_t limit);
int32_t RE_Text_Height(int32_t fontHandle, float scale);
void RE_Text_Paint(float x, float y, int32_t fontHandle, float scale,
                   const float color[4], const char *text,
                   float fixedAdvance, int32_t limit, int32_t textStyle);
int32_t R_DrawStrlen(const char *text);
int32_t RE_Text_ConsoleWidth(const uint16_t *encoded, int32_t fontHandle,
                             float scale, float fixedAdvance,
                             int32_t encodedCount);
void RE_Text_ConsolePaint(float x, float y, int32_t fontHandle, float scale,
                          const float color[4], const uint16_t *encoded,
                          float fixedAdvance, int32_t encodedCount,
                          int32_t textStyle);
void RE_Text_PaintWithCursor(float x, float y, int32_t fontHandle,
                             float scale, const float color[4],
                             const char *text, int32_t cursorPosition,
                             uint8_t cursorCharacter, float fixedAdvance,
                             int32_t limit, int32_t textStyle);
void *R_GetCommandBuffer(int32_t byteCount);
void RE_SetColor(const float *rgba);
void RE_StretchPic(float x, float y, float width, float height,
                   float s1, float t1, float s2, float t2,
                   int32_t shaderHandle);
void RE_UploadCinematic(int32_t width, int32_t height,
                        int32_t columns, int32_t rows,
                        const uint8_t *data, int32_t client,
                        qboolean dirty);
void RE_StretchRaw(int32_t x, int32_t y, int32_t width, int32_t height,
                   int32_t columns, int32_t rows, const uint8_t *data,
                   int32_t client, qboolean dirty);

void RB_BeginSurface(shader_t *shader, int32_t vertexComponentCount);
void RB_EndSurface_Optimized(void);
void RB_CheckOverflow_Optimized(void);
void RB_EndSurface_OptimizedGeneric(void);
void RB_EndSurface_OptimizedARB(void);
void RB_EndSurface_OptimizedATI(void);
void RB_EndSurface_OptimizedNV(void);
void RB_EndSurface_CachedStaticModelGeneric(void);
void RB_EndSurface_CachedStaticModelARB(void);
void RB_EndSurface_CachedStaticModelATI(void);
void RB_EndSurface_CachedStaticModelNV(void);
void RB_DrawSun(void);
void RB_AddSunEffects(void);
void RB_CalcSunFlare(float sunViewDot);
void RB_CalcSunBlind(float sunVisibility, float *blindFraction,
                     float *glareFraction);
void R_ClearSun(void);
void R_FlushSun(void);
void RB_DrawSunSprite(void);
void RB_UpdateSunFlare(shader_t *shader, float alpha, float size,
                       float spriteSize, int32_t fadeInMsec,
                       int32_t fadeOutMsec);
void RB_AddFlare(const renderer_flare_source_t *source,
                 const vec3_t direction);
void RB_AddDlightFlares(void);
void RB_AddCoronaFlares(void);
void R_SetSunFromCvars(void);
shader_t *R_FindSunSpriteShader(const char *name);
void R_SetSunSpriteSize(float spriteSize);
shader_t *R_FindShader(const char *name, int32_t lightmapIndex,
                       qboolean mipRawImage, int32_t shaderUsage);
int32_t R_RegisterShaderFromImage(const char *name,
                                  int32_t lightmapIndex,
                                  image_t *image);
int32_t RE_RegisterShaderLightMap(const char *name,
                                  int32_t lightmapIndex,
                                  int32_t shaderUsage);
int32_t RE_RegisterShader(const char *name, int32_t shaderUsage);
int32_t RE_RegisterShaderNoMip(const char *name,
                               int32_t shaderUsage);
void R_MergeShaderList(shader_t **shaders, int32_t shaderCount,
                       int32_t mergeIndex);
int32_t compare_mergable_shaders(const void *left,
                                 const void *right);
void R_MergeShadersForImageSheets(void);
shader_t *ShaderFromShaderType(const char *name, int32_t lightmapIndex,
                               renderer_image_track_t imageTrack);
void R_BuildShaderFromImage(int32_t lightmapIndex, image_t *image);
shader_t *ShaderForShaderNum(int32_t shaderNum, int32_t lightmapIndex,
                             int32_t shaderUsage);
int32_t R_PicmipForImageFlags(uint32_t imageFlags);
void R_SetupTextureCoordinateRemap(shader_t *shader, vec2_t scale,
                                   vec2_t offset,
                                   int32_t *sourceUIndex,
                                   int32_t *sourceVIndex);
void AddSkyPolygon(int32_t vertexCount, vec3_t *vertices);
void ClipSkyPolygon(int32_t vertexCount, vec3_t *vertices,
                    int32_t clipStage);
void MakeSkyVec(float s, float t, int32_t axis,
                vec2_t texCoord, vec4_t position);
void RB_ClipSkyPolygons(shaderCommands_t *tessellation);
void RB_BuildCloudData(shaderCommands_t *tessellation);
void R_InitSkyTexCoords(float cloudHeight);
void R_SetSkyBox(renderer_sky_vertex_storage_t *storage);
void DrawSkyBox(shader_t *shader, int32_t boxSet);
void RB_StageIteratorSky(qboolean portalPass);
void RB_AddQuadStampExt(const vec3_t origin, const vec3_t left,
                        const vec3_t up, const uint8_t color[4],
                        float s1, float t1, float s2, float t2);
void RB_AddQuadStampExtFade(const vec3_t origin, const vec3_t left,
                            const vec3_t up, const uint8_t color[4],
                            float s1, float t1, float s2, float t2);
void RB_AddQuadStamp(const vec3_t origin, const vec3_t left,
                     const vec3_t up, const uint8_t color[4]);
void DrawTris(shaderCommands_t *tessellation);
void DrawNormals(shaderCommands_t *tessellation);
void DrawColoredNormals(shaderCommands_t *tessellation);
void RB_EndSurface(void);
void RB_SelectStorage(renderer_static_vertex_memory_source_t storageMode);
void RB_SelectStorageATI(renderer_static_vertex_memory_source_t storageMode);
void RB_SelectStorageNV(renderer_static_vertex_memory_source_t storageMode);
uint8_t *RB_GetBuffersNV(int32_t size);
void RB_FinishFenceNV(void);
void RB_SetFenceNV(void);
void R_RotateForEntity(trRefEntity_t *entity,
                       const viewParms_t *viewParms,
                       orientationr_t *orientation);
void R_RotateForModelEntity(trRefEntity_t *entity,
                            const viewParms_t *viewParms,
                            orientationr_t *orientation);
void R_RotateForViewer(void);
void R_WorldNormalToLocal(const vec3_t world, vec3_t local);
void R_TransformModelToClip(const vec3_t source,
                            const float modelMatrix[16],
                            const float projectionMatrix[16],
                            vec4_t eye, vec4_t clip);
void R_TransformHomogenousModelToClip(const vec4_t source,
                                      const float modelMatrix[16],
                                      const float projectionMatrix[16],
                                      vec4_t eye, vec4_t clip);
void R_TransformClipToWindow(const vec4_t clip,
                             const viewParms_t *viewParms,
                             vec4_t normalized, vec4_t window);
void myGlMultMatrix(const float left[16], const float right[16],
                    float product[16]);
void R_TransformDlights(int32_t lightCount, renderer_light_t *lights,
                        const orientationr_t *orientation);
uint32_t R_CullDlightsForBox(const vec3_t boundsMin,
                             const vec3_t boundsMax,
                             uint32_t dlightBits);
uint32_t R_DlightTris(renderer_lit_surface_t *surface,
                      uint32_t dlightBits);
qboolean R_DlightSurface(msurface_t *worldSurface,
                         uint32_t dlightBits);
void R_DlightBmodel(bmodel_t *bmodel);
void R_AddEdgeDef(int32_t firstVertex, int32_t secondVertex,
                  qboolean facing);
void R_RenderShadowEdges(void);
void RB_ProjectionShadowDeform(void);
void RB_ShadowTessEnd(void);
void RB_ShadowFinish(void);
void RB_RenderFlares(void);
void RB_CheckOverflow(int32_t vertexCount, int32_t indexCount);
void RB_SurfaceBad(renderer_surface_t *surface);
void RB_SurfaceSkip(renderer_surface_t *surface);
void RB_SurfaceEntity(renderer_surface_t *surface);
void RB_SurfaceAxis(void);
void RB_SurfaceSplash(void);
void RB_SurfaceSprite(void);
void RB_SurfaceOrientedQuad(void);
void RB_SurfacePolychain(renderer_surface_t *surfaceData);
void RB_SurfaceBeam(void);
void RB_SurfaceRailCore(void);
void RB_SurfaceRailRings(void);
void RB_SurfaceLightningBolt(void);
void RB_SurfaceLine(void);
void RB_SurfaceCylinder(void);
void RB_SurfaceTriangles(renderer_surface_t *surfaceData);
void RB_DlightFallback(renderer_world_mesh_surface_t *surface);
void RB_SurfaceOptimized(renderer_surface_t *surfaceData);
void RB_SetModelMatrixForRigidSurface(
    const renderer_entity_surface_t *entitySurface);
void RB_SurfaceXModelRigid(renderer_surface_t *surfaceData);
void RB_SurfaceXModelRigidSSE(renderer_surface_t *surfaceData);
void RB_SurfaceXModelRigidARB(renderer_surface_t *surfaceData);
void RB_SurfaceXModelRigidATI(renderer_surface_t *surfaceData);
void RB_SurfaceXModelRigidNV(renderer_surface_t *surfaceData);
void RB_SurfaceXModelWeight(renderer_surface_t *surfaceData);
void RB_SurfaceXModelWeightSSE(renderer_surface_t *surfaceData);
void RB_SurfaceStaticModel(renderer_surface_t *surfaceData);
void RB_SurfaceStaticModelCached(renderer_surface_t *surfaceData);
void RB_SurfaceStaticModelT2V3_ARB(renderer_surface_t *surfaceData);
void RB_SurfaceStaticModelT2N3V3_ARB(renderer_surface_t *surfaceData);
void RB_SurfaceStaticModelATI(renderer_surface_t *surfaceData);
void RB_SurfaceStaticModelT2V3_NV(renderer_surface_t *surfaceData);
void RB_SurfaceStaticModelT2N3V3_Generic(renderer_surface_t *surfaceData);
void RB_SurfaceStaticModelT2V3_Generic(renderer_surface_t *surfaceData);
void RB_SurfaceStaticModelT2N3V3_NV(renderer_surface_t *surfaceData);
void R_Fog(renderer_fog_t *fog);
void R_FogOff(void);
void R_FogOn(void);
void RB_SetIteratorFog(void);
void RB_SetupMultitexture(
    shaderStage_t *stage,
    const void *const baseTexCoords[R_MAX_TEXTURE_UNITS],
    int32_t vertexStride);
void RB_SetupMultitextureATI(
    shaderStage_t *stage, uint32_t objectBuffer,
    const uint32_t texCoordOffsets[R_MAX_TEXTURE_UNITS],
    int32_t vertexStride);
void RB_SetupStage(
    shaderStage_t *stage,
    const void *const baseTexCoords[R_MAX_TEXTURE_UNITS],
    int32_t vertexStride);
void RB_SetupStageATI(
    shaderStage_t *stage, uint32_t objectBuffer,
    const uint32_t texCoordOffsets[R_MAX_TEXTURE_UNITS],
    int32_t vertexStride);
void RB_ChooseSurfaceCountColor(int32_t indexCount, uint8_t color[4]);
void RB_MakeNormalVectors(const vec3_t normal, vec3_t tangent,
                          vec3_t bitangent);
void RB_ComputeTexCoords(shaderStage_t *stage);
const float *TableForFunc(shader_wave_func_t function);
long double EvalWaveForm(const waveForm_t *waveform);
long double EvalWaveFormClamped(const waveForm_t *waveform);
void RB_CalcColorFromEntity(uint8_t colors[][4]);
void RB_CalcColorFromOneMinusEntity(uint8_t colors[][4]);
void RB_CalcAlphaFromEntity(uint8_t colors[][4]);
void RB_CalcAlphaFromOneMinusEntity(uint8_t colors[][4]);
int32_t RB_X87RoundFloatToInt(float value);
void RB_CalcDiffuseColor(uint8_t colors[][4]);
void RB_CalcSpecularAlpha(uint8_t colors[][4]);
void RB_CalcWaveColor(const waveForm_t *waveform,
                      uint8_t colors[][4]);
void RB_CalcWaveAlpha(const waveForm_t *waveform,
                      uint8_t colors[][4]);
void RB_CalcDeformVertexes(const deformStage_t *deform);
void RB_CalcFlapVertexes(const deformStage_t *deform,
                         int32_t textureCoordinateAxis);
void RB_CalcDeformSyncNormals(const deformStage_t *deform);
void RB_CalcDeformNormals(const deformStage_t *deform);
void RB_CalcBulgeVertexes(const deformStage_t *deform);
void RB_CalcMoveVertexes(const deformStage_t *deform);
void GlobalPositionToLocal(const vec3_t world, vec3_t local);
void GlobalVectorToLocal(const vec3_t world, vec3_t local);
void DeformText(const char *text);
void AutospriteDeform(void);
void Autosprite2Deform(void);
void RB_DeformTessGeometry(void);
void RB_CalcEnvironmentTexCoords(vec2_t textureCoords[]);
void RB_CalcTangentSpace(void);
void RB_ComputeColors(const shaderStage_t *stage);
void RB_CalcCubeMapTbnTexCoords(vec3_t textureCoords[], int32_t axis);
void RB_CalcCubeMapVertexToEyeTexCoords(vec3_t textureCoords[]);
void RB_CalcCubeMapEyeToVertexTexCoords(vec3_t textureCoords[]);
void RB_CalcCubeMapReflectionTexCoords(vec3_t textureCoords[]);
void RB_CalcCubeMapLightVectorTexCoords(vec3_t textureCoords[]);
void RB_CalcCubeMapLightHalfAngleTexCoords(vec3_t textureCoords[]);
void RB_CalcCubeMapSunHalfAngleTexCoords(vec3_t textureCoords[]);
void RB_CalcCubeMapDot3ReflectTexCoords(vec4_t textureCoords[], int32_t axis);
void RB_CalcTransformTexCoords(const texModInfo_t *texMod,
                               vec2_t textureCoords[]);
void RB_CalcTurbulentTexCoords(const waveForm_t *waveform,
                               vec2_t textureCoords[]);
void RB_CalcScrollTexCoords(const vec2_t scroll, vec2_t textureCoords[]);
void RB_CalcScaleTexCoords(const vec2_t scale, vec2_t textureCoords[]);
void RB_CalcStretchTexCoords(const waveForm_t *waveform,
                             vec2_t textureCoords[]);
void RB_CalcRotateTexCoords(float rotateSpeed, vec2_t textureCoords[]);
void RB_CalcSwapTexCoords(vec2_t textureCoords[]);
void RB_CalcCubeMapNegateTexCoords(vec3_t textureCoords[]);
void RB_CalcCubeMapBumpmapFrameTexCoords(vec3_t textureCoords[]);
int32_t RB_BuildDlightArrays(const renderer_light_t *light,
                             vec2_t textureCoords[R_MAX_TESS_VERTICES],
                             uint8_t colors[R_MAX_TESS_VERTICES][4],
                             uint16_t filteredIndexes[R_MAX_TESS_INDEXES]);
void RB_SetSurfaceCountColor(int32_t indexCount);
void R_DrawElements(int32_t indexCount, const uint16_t *indexes);
void RB_EnableClientTmu(int32_t textureUnit,
                        const textureBundle_t *bundle,
                        const void *texCoords, int32_t vertexStride);
void RB_EnableClientTmuATI(int32_t textureUnit,
                           const textureBundle_t *bundle,
                           uint32_t objectBuffer, uint32_t texCoordOffset,
                           int32_t vertexStride);
void RB_DisableClientTmu(int32_t textureUnit);
void RB_SetupClientTmu(
    int32_t textureUnit, shaderStage_t *stage,
    const void *const baseTexCoords[R_MAX_TEXTURE_UNITS],
    int32_t vertexStride);
void RB_SetupClientTmuATI(
    int32_t textureUnit, shaderStage_t *stage, uint32_t objectBuffer,
    const uint32_t texCoordOffsets[R_MAX_TEXTURE_UNITS],
    int32_t vertexStride);
void RB_EnableTMU(textureBundle_t *bundle, int32_t textureUnit);
void RB_SetupTmu(int32_t textureUnit, shaderStage_t *stage);
void RB_SetupRegisterCombiners(
    renderer_register_combiners_t *registerCombiners);
void RB_SetupVertexProgram(renderer_vertex_program_t *vertexProgram);
image_t *RB_GetAnimatedImage(textureBundle_t *bundle,
                             int32_t textureUnit);
void RB_BindAnimatedImage(textureBundle_t *bundle);
void DrawMultitextured(shaderStage_t *stage, int32_t indexCount,
                       const uint16_t *indexes);
void RB_SingleStageGeneric(shaderStage_t *stage, int32_t indexCount,
                           const uint16_t *indexes);
void RB_UploadWaterTexture(shader_water_map_t *waterMap,
                           int32_t time, int32_t textureUnit);
void R_SetFogColor(void);
void R_SetFog(int32_t fogIndex, int32_t fogStart, int32_t fogEnd,
              float red, float green, float blue, float density);
void R_SetFrameFog(void);
void SetFarClip(void);
void R_SetupProjection(void);
void R_SetupFrustum(void);
void R_MirrorPoint(const vec3_t point,
                   const orientationr_t *surface,
                   const orientationr_t *camera,
                   vec3_t mirroredPoint);
void R_MirrorVector(const vec3_t vector,
                    const orientationr_t *surface,
                    const orientationr_t *camera,
                    vec3_t mirroredVector);
void R_PlaneForSurface(const renderer_surface_t *surface,
                       renderer_frustum_plane_t *plane);
void R_SetPlaneSidesDPVS(renderer_dpvs_plane_t *plane);
qboolean R_BoxBehindPlane(const vec3_t bounds[2],
                          const renderer_dpvs_plane_t *plane);
qboolean R_BoxInFrontOfPlane(const vec3_t bounds[2],
                             const renderer_dpvs_plane_t *plane);
qboolean R_CullBoxDPVS(const vec3_t bounds[2],
                       const renderer_dpvs_plane_t *planes,
                       int32_t planeCount, int32_t planeIndex);
qboolean R_CullBoxDPVSStrict(const vec3_t bounds[2],
                             const renderer_dpvs_plane_t *planes,
                             int32_t planeCount, int32_t planeIndex);
void R_AddTrianglesSurface(msurface_t *worldSurface,
                           uint32_t dlightBits,
                           const renderer_dpvs_plane_t *planes,
                           int32_t planeCount, int32_t planeIndex);
void R_AddSkySurfacesDPVS(void);
void R_AddCoronas(const renderer_dpvs_plane_t *planes,
                  int32_t planeCount);
int32_t R_CellForCamera(void);
qboolean R_PortalBehindAnyPlane(const renderer_portal_t *portal,
                                const renderer_dpvs_plane_t *planes,
                                int32_t planeCount);
qboolean R_PortalBehindAllPlanes(const renderer_portal_t *portal,
                                 const renderer_dpvs_plane_t *planes,
                                 int32_t planeCount);
qboolean R_CullOccluderByPlane(const renderer_occluder_t *occluder,
                               const renderer_dpvs_plane_t *plane);
vec3_t *R_ChopPortalWinding(const renderer_dpvs_plane_t *plane,
                            vec3_t *inputVertices,
                            int32_t *vertexCount,
                            vec3_t *outputVertices);
renderer_dpvs_plane_t *R_PortalClipPlanesInternal(
    vec3_t *vertices, int32_t *planeCount, int32_t vertexCount);
renderer_dpvs_plane_t *R_PortalClipPlanes(
    const renderer_portal_t *portal,
    const renderer_dpvs_plane_t *portalPlane,
    const renderer_dpvs_plane_t *planes,
    int32_t planeCount,
    int32_t *outPlaneCount);
void R_AddCellOccluders(renderer_world_cell_t *cell,
                        const renderer_dpvs_plane_t *planes,
                        int32_t planeCount);
void R_AddCellSurfaces(renderer_world_cell_t *cell,
                       const renderer_dpvs_plane_t *planes,
                       int32_t planeCount, uint32_t dlightBits);
void R_AddCullGroupDPVS(renderer_cull_group_t *group,
                        const renderer_dpvs_plane_t *planes,
                        int32_t planeCount, uint32_t dlightBits);
void R_AddStaticModelDPVS(renderer_static_model_t *model,
                          const renderer_dpvs_plane_t *planes,
                          int32_t planeCount);
void R_AddAABBTreeSurfaces_r(renderer_aabb_tree_t *tree,
                             const renderer_dpvs_plane_t *planes,
                             int32_t planeCount, uint32_t dlightBits,
                             int32_t planeIndex);
void R_CullModels(renderer_world_cell_t *cell,
                  const renderer_dpvs_plane_t *planes,
                  int32_t planeCount);
void R_FilterModelsIntoCells(const renderer_dpvs_plane_t *planes,
                             int32_t planeCount);
void R_AddCellCullGroups(renderer_world_cell_t *cell,
                         const renderer_dpvs_plane_t *planes,
                         int32_t planeCount, uint32_t dlightBits);
void R_RecursivePortalWalk(renderer_world_cell_t *cell,
                           const renderer_dpvs_plane_t *sourcePlane,
                           const renderer_dpvs_plane_t *planes,
                           int32_t planeCount,
                           uint32_t dlightBits);
extern renderer_dpvs_plane_t *rendererDpvsActiveNearPlane;
extern renderer_dpvs_plane_t *rendererDpvsActiveFarPlane;
extern int32_t rendererDpvsOccluderCount;
extern renderer_occluder_t **rendererDpvsOccluders;
extern int32_t rendererDpvsActivePlaneCount;
extern renderer_dpvs_plane_t *rendererDpvsActivePlanes;
extern int32_t rendererDpvsCellEntityLinkCount;
extern renderer_cell_entity_link_t *rendererDpvsCellEntityLinks;
extern int32_t rendererDpvsCullPlaneLimit;
extern float rendererCullDistance;
extern renderer_dpvs_plane_t
    rendererDpvsFrustumPlanes[R_FRUSTUM_PLANE_COUNT];
extern vec3_t rendererDpvsViewOrigin;
extern renderer_dpvs_plane_t rendererDpvsNearPlane;
extern renderer_dpvs_plane_t rendererDpvsFarPlane;
extern float rendererDpvsWorldViewProjectionMatrix[4][4];
extern float rendererDpvsInverseWorldViewProjectionMatrix[4][4];
extern qboolean rendererDpvsInitialSetup;
float RE_GetFarPlaneDist(void);
void RE_SetCullDist(float distance);
void R_SetupDPVSFrustumPlanes(void);
void R_SetupDPVSProjectionMatrices(void);
void R_SetupDPVS(void);
qboolean R_GetPortalOrientations(int32_t entityNumber,
                                 const drawSurf_t *drawSurf,
                                 orientationr_t *surface,
                                 orientationr_t *camera,
                                 vec3_t pvsOrigin, qboolean *mirror);
qboolean IsMirror(int32_t entityNumber, const drawSurf_t *drawSurf);
qboolean SurfIsOffscreen(const drawSurf_t *drawSurf);
qboolean R_MirrorViewBySurface(const drawSurf_t *drawSurf,
                               int32_t entityNumber);
void R_AddDrawSurf(renderer_surface_t *surface, int32_t storageMode,
                   shader_t *shader, uint32_t batchFlag0,
                   uint32_t batchFlag2, uint32_t worldEntity);
void R_DecomposeSort(
    uint32_t sort, renderer_static_vertex_memory_source_t *storageMode,
    int32_t *entityNumber, shader_t **shader, uint32_t *batchFlag0,
    uint32_t *batchFlag2);
void R_AddWorldSurfaceNoCull(msurface_t *worldSurface,
                             uint32_t dlightBits);
qboolean R_CullWorldSurface(const renderer_surface_t *surface);
void R_AddWorldSurface(msurface_t *worldSurface,
                       uint32_t dlightBits);
void R_AddWorldSurfaces(void);
void R_AddEntityDrawSurf(trRefEntity_t *entity, DObj *obj,
                         XSurface *surface, shader_t *shader,
                         int32_t modelIndex);
int32_t R_DObjGetSurfIndex(const DObj *obj, int32_t modelIndex);
void R_XModelDebugBoxes(trRefEntity_t *entity,
                        const uint32_t *partBits);
void R_XModelDebugAxes(trRefEntity_t *entity,
                       const uint32_t *partBits);
void R_XModelDebug(trRefEntity_t *entity, const uint32_t *partBits);
float R_GetLodDist(const trRefEntity_t *entity);
void R_AddDrawSurfCmd(drawSurf_t *drawSurfs, int32_t drawSurfCount);
void R_SortDrawSurfs(drawSurf_t *drawSurfs, int32_t drawSurfCount);
void R_AddBrushModelSurfaces(trRefEntity_t *entity);
void R_BeginDelayedImageGroup(const char *modelName);
void R_SetImageGroupTileMode(int32_t tileMode);
void R_SetImageGroupTriCount(int32_t triangleCount);
void R_EndDelayedImageGroup(void);
model_t *R_AllocModel(void);
model_t *R_GetModelByHandle(int32_t modelHandle);
int32_t RE_RegisterModel(const char *name, int32_t loadMode);
XModel *RE_GetXModelByHandle(int32_t modelHandle);
int32_t RE_GetShaderFromModel(int32_t modelHandle, int32_t surfaceIndex);
void R_LoadXModel(model_t *model, const char *name,
                  int32_t shaderUsage);
void R_LoadDelayedImages(void);
void R_MergeShadersForImageSheets(void);
void R_FinishLoadingStaticModels(void);
void R_OptimizeXModelSurfaces(void);
void R_FixupXModelTexCoords(void);
void R_GetXModelBounds(const DObj *obj, const axis_t transform,
                       vec3_t mins, vec3_t maxs);
void R_AddXModelSurfaces(trRefEntity_t *entity);
cull_result_t R_CullModel(const trRefEntity_t *entity);
void R_AddStaticModelSurfaces(trRefEntity_t *entity);
void R_AddEntitySurfaces(void);
void R_AddPolygonSurfaces(void);
void R_AddWorldSurfacesDPVS(void);
void R_GenerateDrawSurfs(void);
void R_RenderView(const viewParms_t *viewParms);
int32_t RE_SaveFogState(void *buffer, uint32_t bufferSize);
int32_t RE_RestoreFogState(const void *buffer, uint32_t bufferSize);
float R_UpdateOverTime(float currentValue, float targetValue,
                       int32_t riseTime, int32_t fallTime,
                       int32_t elapsedTime);

#ifdef __cplusplus
}
#endif

#endif
