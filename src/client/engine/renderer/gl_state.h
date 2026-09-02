#ifndef CODUOMP_RENDERER_GL_STATE_H
#define CODUOMP_RENDERER_GL_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "qcommon/asset_type_names.h"
#include "qcommon/q_renderer_types.h"
#include "../q_shared.h"
#include "renderer_cvars.h"
#include "renderer_entity.h"
#include "renderer_orientation.h"
#include "renderer_refdef.h"
#include "renderer_vbo.h"
#include "renderer_view_parms.h"

enum {
    R_MAX_TEXTURE_UNITS = 8,
    R_MAX_SCRATCH_IMAGES = 32,
    R_MAX_LIGHTMAPS = 128,
    R_MAX_IMAGES = 2048,
    R_MAX_MODELS = 2048,
    R_MAX_SHADERS = 4096,
    R_WORLD_NAME_SIZE = 64
};

typedef void (*renderer_stage_iterator_fn_t)(qboolean portalPass);

typedef enum renderer_surface_type_e {
    R_SURFACE_BAD = 0,
    R_SURFACE_SKIP = 1,
    R_SURFACE_POLY = 2,
    R_SURFACE_ENTITY = 3,
    R_SURFACE_XMODEL_RIGID = 4,
    R_SURFACE_XMODEL_RIGID_SSE = 5,
    R_SURFACE_XMODEL_RIGID_ARB = 6,
    R_SURFACE_XMODEL_RIGID_ATI = 7,
    R_SURFACE_XMODEL_RIGID_NV = 8,
    R_SURFACE_XMODEL_WEIGHT = 9,
    R_SURFACE_XMODEL_WEIGHT_SSE = 10,
    R_SURFACE_STATIC_MODEL = 11,
    R_SURFACE_CACHED_STATIC_MODEL_GENERIC = 12,
    R_SURFACE_CACHED_STATIC_MODEL_ARB = 13,
    R_SURFACE_CACHED_STATIC_MODEL_ATI = 14,
    R_SURFACE_CACHED_STATIC_MODEL_NV = 15,
    R_SURFACE_STATIC_MODEL_T2V3_GENERIC = 16,
    R_SURFACE_STATIC_MODEL_T2V3_ARB = 17,
    R_SURFACE_STATIC_MODEL_T2V3_ATI = 18,
    R_SURFACE_STATIC_MODEL_T2V3_NV = 19,
    R_SURFACE_STATIC_MODEL_T2N3V3_GENERIC = 20,
    R_SURFACE_STATIC_MODEL_T2N3V3_ARB = 21,
    R_SURFACE_STATIC_MODEL_T2N3V3_ATI = 22,
    R_SURFACE_STATIC_MODEL_T2N3V3_NV = 23,
    R_SURFACE_WORLD_UNOPTIMIZED = 24,
    R_SURFACE_OPTIMIZED_GENERIC = 25,
    R_SURFACE_OPTIMIZED_ARB = 26,
    R_SURFACE_OPTIMIZED_ATI = 27,
    R_SURFACE_OPTIMIZED_NV = 28,
    R_SURFACE_INDEXED_POSITION_FIRST = R_SURFACE_WORLD_UNOPTIMIZED
} renderer_surface_type_t;

enum renderer_image_flag_e {
    IMAGE_FLAG_NONE = 0,
    IMAGE_FLAG_MIPMAP = 0x0001,
    /* R_PicmipForImageFlags selects r_picmip only when this bit is set. */
    IMAGE_FLAG_ALLOW_PICMIP = 0x0002,
    /* This bit selects r_picmip2 instead of r_picmip. */
    IMAGE_FLAG_USE_PICMIP2 = 0x0004,
    /* Set on generated *lightmap atlases. No recovered upload branch consumes
     * it independently; its proven role is the stored lightmap-image tag. */
    IMAGE_FLAG_LIGHTMAP = 0x0008,
    IMAGE_FLAG_CLAMP_S = 0x0010,
    IMAGE_FLAG_CLAMP_T = 0x0020,
    /* Select the $h2n_ texture-key path and convert a loaded height map to a
     * normal map before image creation. */
    IMAGE_FLAG_HEIGHT_TO_NORMAL = 0x0040,
    /* Defer upload so compatible images can be packed into texture sheets. */
    IMAGE_FLAG_DELAYED_UPLOAD = 0x0080,
    /* Keep this image out of the delayed texture-sheet packing path. */
    IMAGE_FLAG_NO_TEXTURE_SHEET = 0x0100,
    /* Select an unsized/RGB5 internal format from the active color depth
     * before falling back to per-pixel alpha inspection. */
    IMAGE_FLAG_COLOR_DEPTH = 0x0200,
    /* Suppress the overbright contribution while retaining gamma mapping. */
    IMAGE_FLAG_NO_OVERBRIGHT = 0x0400
};

struct shader_s;
struct image_s;

/* IMAGE_FLAG_DELAYED_UPLOAD selects delayedShader. Once that flag is cleared,
 * this same original +0x68 slot is either NULL or the physical sheet image. */
typedef union renderer_image_link_u {
    struct image_s *textureSheet;
    struct shader_s *delayedShader;
} renderer_image_link_t;

typedef struct renderer_image_sheet_state_s {
    uint16_t x;                            /* original +0x00 / image +0x6c */
    uint16_t y;                            /* original +0x02 / image +0x6e */
    /* Both clients store width < height here. CoDUOMP resets and writes this
     * byte but never reads it; coordinate remapping recomputes the predicate. */
    uint8_t rotated;                       /* original +0x04 / image +0x70 */
} renderer_image_sheet_state_t;

typedef struct renderer_image_delay_state_s {
    int32_t group;                         /* original +0x00 / image +0x6c */
    int32_t groupTriCount;                 /* original +0x04 / image +0x70 */
} renderer_image_delay_state_t;

/* The delayed variant proves this union is eight bytes. In the sheet variant,
 * +0x71..+0x73 are inactive tail storage, not an unrecovered field: neither
 * the Windows nor PowerPC Mac client accesses them. */
typedef union renderer_image_state_u {
    renderer_image_sheet_state_t sheet;
    renderer_image_delay_state_t delay;
} renderer_image_state_t;

/* This retains Q3's image_s/image_t identity, but not its obsolete layout.
 * Windows and PowerPC Mac R_AllocImage both request exactly 0x78 bytes.
 * R_LoadSingleDelayedImage independently copies all 0x78 bytes when replacing
 * a failed delayed image with the default-image record. */
typedef struct image_s {
    char imgName[MAX_QPATH]; /* original +0x00; inherited image path */
    uint16_t width;        /* original +0x40 */
    uint16_t height;       /* original +0x42 */
    uint16_t uploadWidth;  /* original +0x44 */
    uint16_t uploadHeight; /* original +0x46 */
    int32_t cardMemory;    /* original +0x48 */
    int32_t textureMemory; /* original +0x4c */
    renderer_image_track_t imageTrack; /* original +0x50 */
    uint32_t target;       /* original +0x54 */
    /* SortNewShader uses unsigned-below/unsigned-below-or-equal branches on
     * this OpenGL texture object name (0x00501fed/0x00502004). */
    uint32_t texnum;       /* original +0x58 */
    int32_t frameUsed;     /* original +0x5c */
    uint32_t internalFormat; /* original +0x60 */
    uint32_t flags;        /* original +0x64: IMAGE_FLAG_* */
    renderer_image_link_t link;    /* original +0x68 */
    renderer_image_state_t state;  /* original +0x6c */
    struct image_s *hashNext;     /* original +0x74 */
} image_t;

/* Texture-sheet packing list/tree node. A nonterminal list element uses
 * child; the final element (next == NULL) uses image. MergeImageList builds
 * these lists and UploadImageGroup_r consumes the same tagged shape. The
 * triangleCount lane is copied from delayed groupTriCount for leaves, summed
 * for merges, and used as the final packing-priority comparison; it is not a
 * pixel area. */
typedef struct renderer_image_group_node_s renderer_image_group_node_t;
struct renderer_image_group_node_s {
    union {
        renderer_image_group_node_t *child; /* original +0x00, nonterminal */
        image_t *image;                     /* original +0x00, terminal */
    };
    renderer_image_group_node_t *next;      /* original +0x04 */
    int32_t triangleCount;                  /* original +0x08 */
    int32_t group;                          /* original +0x0c */
    int32_t width;                          /* original +0x10 */
    int32_t height;                         /* original +0x14 */
};

typedef enum renderer_image_merge_direction_e {
    R_IMAGE_MERGE_HORIZONTAL = 0,
    R_IMAGE_MERGE_VERTICAL = 1
} renderer_image_merge_direction_t;

struct mnode_s;
struct renderer_world_cell_s;
struct bmodel_s;
struct renderer_legacy_md3_header_s;
struct dshader_s;
struct msurface_s;
struct renderer_aabb_tree_s;
struct renderer_portal_s;
struct renderer_cull_group_s;
struct renderer_occluder_s;
struct renderer_registered_static_model_s;
struct DObj_s;

/* Sky-box upload vertex consumed identically by client arrays and the three
 * object/memory backends: two texture-coordinate floats followed by the
 * homogeneous four-float position generated by MakeSkyVec. */
typedef struct renderer_sky_vertex_s {
    vec2_t texCoord;
    vec4_t position;
} renderer_sky_vertex_t;

typedef enum renderer_sky_vertex_backend_e {
    R_SKY_VERTEX_BACKEND_CLIENT = 0,
    R_SKY_VERTEX_BACKEND_ARB_BUFFER = 1,
    R_SKY_VERTEX_BACKEND_ATI_OBJECT = 2,
    R_SKY_VERTEX_BACKEND_NV_MEMORY = 3
} renderer_sky_vertex_backend_t;

/* backend selects this overlay: client/NV paths carry a host vertex pointer;
 * ARB/ATI paths carry a 32-bit OpenGL buffer/object name. */
typedef union renderer_sky_vertex_base_u {
    renderer_sky_vertex_t *vertices;
    uint32_t bufferObject;
} renderer_sky_vertex_base_t;

/* Complete sky-box storage descriptor. memorySource selects RB storage state,
 * backend selects the union interpretation and pointer API, and objectOffset
 * is consumed only by the ATI-object alternative. Native pointer widening is
 * private to the maintained renderer; original i386 offsets are guarded where
 * the descriptor is used. */
typedef struct renderer_sky_vertex_storage_s {
    renderer_static_vertex_memory_source_t memorySource; /* original +0x00 */
    renderer_sky_vertex_backend_t backend;                /* original +0x04 */
    renderer_sky_vertex_base_t base;                       /* original +0x08 */
    uint32_t objectOffset;                                 /* original +0x0c */
} renderer_sky_vertex_storage_t;

/* CoD's reduced continuation of Quake III frontEndCounters_t, printed by
 * r_speeds modes 1, 2, and 4. The patch-cull
 * names and order are proved by the mode-2 format string and argument order.
 * CoDUOMP.exe reads and clears the first seven counters but has no nonzero
 * producer for them; dlightSurfaceCount is the only produced field in this
 * record. dlightSurfaceCullCount is likewise read, printed, and cleared but
 * has no nonzero CoDUOMP.exe producer. These are dormant diagnostic counters,
 * not padding. */
typedef struct frontEndCounters_s {
    int32_t patchSphereCullIn;      /* original tr +0xaac */
    int32_t patchSphereCullClip;    /* original tr +0xab0 */
    int32_t patchSphereCullOut;     /* original tr +0xab4 */
    int32_t patchBoxCullIn;         /* original tr +0xab8 */
    int32_t patchBoxCullClip;       /* original tr +0xabc */
    int32_t patchBoxCullOut;        /* original tr +0xac0 */
    int32_t leafCount;              /* original tr +0xac4 */
    int32_t dlightSurfaceCount;     /* original tr +0xac8 */
    int32_t dlightSurfaceCullCount; /* original tr +0xacc */
} frontEndCounters_t;

/* Map-authored flare/corona record built by R_LoadCorona. scale becomes the
 * sprite radius, zCutoff rejects low view directions, and zFadeOut starts the
 * alpha fade toward that cutoff. The final four bytes are RGBA, including the
 * loader-supplied constant alpha; there is no ABI gap in this 32-byte record. */
typedef struct renderer_world_corona_s {
    struct shader_s *shader; /* original +0x00 */
    vec3_t origin;           /* original +0x04 */
    float scale;             /* original +0x10 */
    float zCutoff;           /* original +0x14 */
    float zFadeOut;          /* original +0x18 */
    uint8_t color[4];        /* original +0x1c */
} renderer_world_corona_t;

/* Windows and PPC Mac RE_LoadWorldMap independently clear the original
 * rendererWorldData allocation as exactly 0x154 bytes before their named BSP
 * loaders populate the same field groups below. dataSize, numDecisionNodes,
 * and the
 * world-level occluderCount are retained original fields even though each is
 * written during loading and never subsequently read by CoDUOMP.exe. */
typedef struct world_s {
    char name[R_WORLD_NAME_SIZE];     /* original +0x000 */
    char baseName[R_WORLD_NAME_SIZE]; /* original +0x040 */
    int32_t dataSize; /* original +0x080: allocated bytes; write-only */
    int32_t numShaders; /* original +0x084 */
    struct dshader_s *shaders; /* original +0x088 */
    struct bmodel_s *bmodels; /* original +0x08c */
    int32_t numnodes; /* original +0x090; includes leaves */
    int32_t numDecisionNodes; /* original +0x094; write-only */
    struct mnode_s *nodes; /* original +0x098 */
    int32_t numsurfaces; /* original +0x09c */
    struct msurface_s *surfaces; /* original +0x0a0 */
    int32_t skySurfaceCount; /* original +0x0a4 */
    struct msurface_s **skySurfaces; /* original +0x0a8 */
    int32_t aabbTreeCount; /* original +0x0ac */
    struct renderer_aabb_tree_s *aabbTrees; /* original +0x0b0 */
    int32_t numClusters; /* original +0x0b4 */
    char *entityString;                  /* original +0x0b8 */
    char *entityParsePoint;              /* original +0x0bc */
    renderer_world_corona_t *coronas; /* original +0x0c0 */
    int32_t coronaCount;              /* original +0x0c4 */
    renderer_sky_vertex_storage_t skyVertexStorage; /* original +0x0c8 */
    vec4_t entityAmbientBase;  /* original +0x0d8 */
    vec4_t sunDiffuseColor; /* original +0x0e8 */
    vec4_t entityAmbientScale; /* original +0x0f8 */
    float entitySunLightIntensity; /* original +0x108 */
    int32_t lightCount; /* original +0x10c */
    struct renderer_light_s *lights; /* original +0x110 */
    struct renderer_light_s *sunLight; /* original +0x114 */
    int32_t lightIndexCount; /* original +0x118 */
    int16_t *lightIndexes; /* original +0x11c */
    int32_t cellCount;     /* original +0x120 */
    struct renderer_world_cell_s *cells; /* original +0x124 */
    int32_t occluderCount; /* original +0x128; write-only */
    struct renderer_occluder_s *occluders; /* original +0x12c */
    int32_t occluderIndexCount; /* original +0x130 */
    struct renderer_occluder_s **occluderIndexes; /* original +0x134 */
    int32_t portalCount; /* original +0x138 */
    struct renderer_portal_s *portals; /* original +0x13c */
    vec3_t *portalVerts; /* original +0x140 */
    int32_t cullGroupCount; /* original +0x144 */
    struct renderer_cull_group_s *cullGroups; /* original +0x148 */
    int32_t cullGroupIndexCount; /* original +0x14c */
    struct renderer_cull_group_s **cullGroupIndexes; /* original +0x150 */
} world_t;

#if UINTPTR_MAX == UINT32_MAX
#define RENDERER_ASSERT_I386_WORLD_FIELD(member, offset, extent) \
    _Static_assert(offsetof(world_t, member) == (offset), "i386 renderer world field " #member " moved"); \
    _Static_assert(sizeof(((world_t *)0)->member) == (extent), "i386 renderer world field " #member " extent changed")
_Static_assert(_Alignof(world_t) == 0x04, "i386 renderer world alignment changed");
RENDERER_ASSERT_I386_WORLD_FIELD(name, 0x000, 0x40);
RENDERER_ASSERT_I386_WORLD_FIELD(baseName, 0x040, 0x40);
RENDERER_ASSERT_I386_WORLD_FIELD(dataSize, 0x080, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(numShaders, 0x084, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(shaders, 0x088, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(bmodels, 0x08c, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(numnodes, 0x090, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(numDecisionNodes, 0x094, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(nodes, 0x098, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(numsurfaces, 0x09c, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(surfaces, 0x0a0, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(skySurfaceCount, 0x0a4, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(skySurfaces, 0x0a8, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(aabbTreeCount, 0x0ac, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(aabbTrees, 0x0b0, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(numClusters, 0x0b4, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(entityString, 0x0b8, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(entityParsePoint, 0x0bc, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(coronas, 0x0c0, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(coronaCount, 0x0c4, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(skyVertexStorage, 0x0c8, 0x10);
RENDERER_ASSERT_I386_WORLD_FIELD(entityAmbientBase, 0x0d8, 0x10);
RENDERER_ASSERT_I386_WORLD_FIELD(sunDiffuseColor, 0x0e8, 0x10);
RENDERER_ASSERT_I386_WORLD_FIELD(entityAmbientScale, 0x0f8, 0x10);
RENDERER_ASSERT_I386_WORLD_FIELD(entitySunLightIntensity, 0x108, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(lightCount, 0x10c, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(lights, 0x110, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(sunLight, 0x114, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(lightIndexCount, 0x118, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(lightIndexes, 0x11c, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(cellCount, 0x120, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(cells, 0x124, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(occluderCount, 0x128, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(occluders, 0x12c, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(occluderIndexCount, 0x130, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(occluderIndexes, 0x134, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(portalCount, 0x138, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(portals, 0x13c, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(portalVerts, 0x140, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(cullGroupCount, 0x144, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(cullGroups, 0x148, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(cullGroupIndexCount, 0x14c, 0x04);
RENDERER_ASSERT_I386_WORLD_FIELD(cullGroupIndexes, 0x150, 0x04);
_Static_assert(sizeof(world_t) == 0x154, "i386 renderer world layout changed");
#undef RENDERER_ASSERT_I386_WORLD_FIELD
#endif

typedef enum renderer_model_type_e {
    MODEL_BAD = 0,
    MODEL_BMODEL = 1,
    MODEL_XMODEL = 2
} renderer_model_type_t;

/* CoD's continuation of Quake III's renderer-local model_t, allocated by
 * R_AllocModel. Windows,
 * PowerPC CoD 1.5, and PowerPC UO 1.51 all allocate exactly 0x60 bytes and use
 * the same naturally aligned 32-bit layout. Quake III lineage names +0x48
 * dataSize and places its third MD3 LOD pointer at +0x58. CoD's
 * shaderHandles/xmodel fields occupy the first two old mesh-pointer slots,
 * while the surrounding lineage slots remain. CoDUOMP has no field-specific
 * access to dataSize or legacyMd3Lod2; numLods is reset by RE_RegisterModel
 * but never read. These members are therefore dormant source-layout members,
 * not compiler padding.
 *
 * MODEL_BMODEL and MODEL_XMODEL are mutually exclusive, but their payloads
 * are not overlaid: original instructions store bmodel at +0x4c and the
 * XModel fields separately at +0x50/+0x54. */
typedef struct model_s {
    char name[MAX_QPATH];                   /* original +0x00 */
    renderer_model_type_t type;             /* original +0x40 */
    int32_t index;                          /* original +0x44 */
    int32_t dataSize;                       /* original +0x48; unused */
    struct bmodel_s *bmodel;                /* original +0x4c */
    uint16_t **shaderHandles;                /* original +0x50 */
    XModel *xmodel;                          /* original +0x54 */
    struct renderer_legacy_md3_header_s *legacyMd3Lod2;                     /* original +0x58; unused */
    int32_t numLods;                        /* original +0x5c; write-only */
} model_t;

/* CoD's continuation of Quake III trGlobals_t, beginning at original
 * 0x04885020. Windows R_Init
 * clears 0x13bd8 bytes from that address, but that span contains this 0x13b58-
 * byte object followed by the separate 0x80-byte rendererDebugState at
 * 0x04898b78. PowerPC UO R_Init proves the same combined clear. The registered,
 * frame, view, and scene words at +0x00..+0x0c are directly addressed by
 * RE_Shutdown and R_RenderView. The early image/world/shader fields and the
 * proved renderer suffix are placed at their absolute Windows operands:
 * world at 0x04885034, default/scratch/dlight images at
 * 0x04885038/0x0488503c/0x048850bc, the white/identity/gray built-ins at
 * 0x048850c4/0x048850c8/0x048850cc, the saved-screen state at
 * 0x048850d0..0x048850e4, and the default/stencil-shadow/saved-screen shaders
 * at 0x048850e8/0x048850ec/0x048850f0,
 * currentEntityNumber/shiftedEntityNumber at 0x04885624/28, viewParms at
 * 0x04885630, identityLight at 0x04885890, models at 0x04885af8, modelCount at
 * 0x04887af8, and images at 0x04889b20. The complete i386 object ends exactly
 * where the renderer debug state begins. */
typedef struct trGlobals_s {
    qboolean registered;   /* original +0x000 */
    int32_t frameCount;    /* original +0x004 */
    int32_t viewCount;     /* original +0x008 */
    int32_t frameSceneNum; /* original +0x00c */
    qboolean worldMapLoaded; /* original +0x010 */
    world_t *world;        /* original +0x014 */
    image_t *defaultImage; /* original +0x018 */
    image_t *scratchImages[R_MAX_SCRATCH_IMAGES]; /* original +0x01c */
    image_t *dlightImage;  /* original +0x09c */
    /* Quake III renderer lineage retains flareImage between dlightImage and
     * whiteImage. Windows has no absolute access to 0x048850c0, and PowerPC
     * R_CreateBuiltinImages skips this slot. */
    image_t *flareImage;   /* original +0x0a0; unused in CoDUOMP */
    image_t *whiteImage;         /* original +0x0a4 */
    image_t *identityLightImage; /* original +0x0a8 */
    image_t *grayImage;          /* original +0x0ac */
    image_t *screenImage;        /* original +0x0b0 */
    int32_t screenImageWidth;    /* original +0x0b4 */
    int32_t screenImageHeight;   /* original +0x0b8 */
    int32_t screenImageSaveTime; /* original +0x0bc */
    float screenImageSMax;       /* original +0x0c0 */
    float screenImageTMax;       /* original +0x0c4 */
    struct shader_s *defaultShader;       /* original +0x0c8 */
    struct shader_s *stencilShadowShader; /* original +0x0cc */
    struct shader_s *screenImageShader;   /* original +0x0d0 */
    struct shader_s *dlightShader;         /* original +0x0d4 */
    struct shader_s *flareShader;          /* original +0x0d8 */
    struct shader_s *spotLightShader;      /* original +0x0dc */
    char sunName[R_WORLD_NAME_SIZE];       /* original +0x0e0 */
    int32_t ignorePrecacheErrorCount; /* original +0x120 */
    qboolean modelsFinishedLoading;   /* original +0x124 */
    int32_t delayedImageGroup;        /* original +0x128 */
    int32_t delayedImageGroupTriCount;/* original +0x12c */
    int32_t delayedImageGroupTileMode;/* original +0x130 */
    int32_t delayedImageGroupSequence;/* original +0x134 */
    int32_t delayedImageCount;        /* original +0x138 */
    struct shader_s *showTrisShader;  /* original +0x13c */
    struct shader_s *showImagesShader;/* original +0x140 */
    int32_t lightmapCount;            /* original +0x144 */
    image_t *lightmaps[R_MAX_LIGHTMAPS]; /* original +0x148 */
    trRefEntity_t *currentEntity;      /* original +0x348 */
    trRefEntity_t worldEntity;         /* original +0x34c */
    int32_t currentEntityNumber;             /* original +0x604 */
    uint32_t shiftedEntityNumber;            /* original +0x608 */
    /* Quake III renderer lineage places currentModel here. Neither CoDUOMP
     * target has a field-specific access to the retained member. */
    model_t *currentModel; /* original +0x60c; unused in CoDUOMP */
    viewParms_t viewParms; /* original +0x610 */
    float identityLight;   /* original +0x870 */
    int32_t identityLightByte; /* original +0x874 */
    int32_t overbrightBits;    /* original +0x878 */
    orientationr_t orientation; /* original +0x87c */
    trRefdef_t refdef;                   /* original +0x8f8 */
    int32_t viewCluster;                     /* original +0xa80 */
    vec3_t sunLight;                     /* original +0xa84 */
    vec3_t sunDirection;                 /* original +0xa90 */
    int32_t maxEntityLights;       /* original +0xa9c */
    int32_t diffuseSunQuality;     /* original +0xaa0 */
    int32_t diffuseSunSteps;       /* original +0xaa4 */
    float diffuseSunSampleScale;   /* original +0xaa8 */
    frontEndCounters_t pc; /* original +0xaac */
    int32_t frontEndMsec;          /* original +0xad0 */
    renderer_frame_statistics_t *frameStatistics; /* original +0xad4 */
    model_t *models[R_MAX_MODELS]; /* original +0x0ad8 */
    int32_t modelCount;             /* original +0x2ad8 */
    /* Persistent round-robin cursor used by
     * R_IncrementalRefreshXModels_ARB. The original advances the surface
     * component before selecting the next XSurface. */
    int32_t xmodelRefreshModelIndex;   /* original +0x2adc */
    int32_t xmodelRefreshLodIndex;     /* original +0x2ae0 */
    int32_t xmodelRefreshSurfaceIndex; /* original +0x2ae4 */
    /* Independent persistent cursor used by
     * R_IncrementalRefreshStaticModels_ARB. */
    int32_t staticModelRefreshModelIndex;   /* original +0x2ae8 */
    int32_t staticModelRefreshLodIndex;     /* original +0x2aec */
    int32_t staticModelRefreshSurfaceIndex; /* original +0x2af0 */
    int32_t worldRefreshSurfaceIndex; /* original +0x2af4 */
    struct renderer_registered_static_model_s *registeredStaticModels[R_MAX_MODELS]; /* original +0x2af8 */
    int32_t registeredStaticModelCount; /* original +0x4af8 */
    int32_t imageCount;    /* original +0x4afc */
    image_t *images[R_MAX_IMAGES]; /* original +0x4b00 */
    int32_t imageMemory; /* original +0x6b00 */
    uint32_t fxImageMemory; /* original +0x6b04 */
    /* CoD 1.5 contains the same four-byte source-layout member immediately
     * before numShaders. Neither CoD 1.5 nor CoDUOMP has a field-specific
     * access, so its original name and scalar type remain unresolved. */
    uint8_t unrecovered6b08[4]; /* original +0x6b08; unused in CoDUOMP */
    int32_t numShaders;                    /* original +0x6b0c */
    struct shader_s *shaders[R_MAX_SHADERS];       /* original +0x6b10 */
    struct shader_s *sortedShaders[R_MAX_SHADERS]; /* original +0xab10 */
    int32_t fragmentShaderCount; /* original +0xeb10 */
    float sinTable[1024];        /* original +0xeb14 */
    float squareTable[1024];     /* original +0xfb14 */
    float triangleTable[1024];   /* original +0x10b14 */
    float sawToothTable[1024];   /* original +0x11b14 */
    float inverseSawToothTable[1024]; /* original +0x12b14 */
    renderer_static_vertex_memory_base_t staticVertexMemorySecondary; /* original +0x13b14 */
    size_t staticVertexMemorySecondaryLimit; /* original +0x13b18 */
    size_t staticVertexMemorySecondaryUsed;  /* original +0x13b1c */
    renderer_static_vertex_memory_base_t staticVertexMemoryPrimary; /* original +0x13b20 */
    size_t staticVertexMemoryPrimaryLimit; /* original +0x13b24 */
    size_t staticVertexMemoryPrimaryUsed;  /* original +0x13b28 */
    renderer_static_vertex_memory_source_t defaultStorageMode; /* original +0x13b2c */
    int32_t dynamicBufferFrameSerial;    /* original +0x13b30 */
    int32_t dynamicBufferMaxFrameSerial; /* original +0x13b34 */
    renderer_static_vertex_memory_source_t cachedStaticModelStorageSource; /* original +0x13b38 */
    renderer_surface_type_t cachedStaticModelSurfaceType; /* +0x13b3c */
    renderer_static_vertex_memory_base_t cachedStaticModelStorage; /* original +0x13b40 */
    size_t cachedStaticModelStorageOffset; /* original +0x13b44 */
    qboolean vboStreamDraw;  /* original +0x13b48 */
    qboolean vboInterleaved; /* original +0x13b4c */
    uint32_t vboUsage;       /* original +0x13b50 */
    renderer_stage_iterator_fn_t stageIteratorFunc; /* +0x13b54 */
} trGlobals_t;

typedef enum cullType_e {
    CT_FRONT_SIDED = 0,
    CT_BACK_SIDED = 1,
    CT_TWO_SIDED = 2
} cullType_t;

/* Q3 supplies the inherited glstate_t identity and still-valid member names;
 * the two shipped CoD clients prove this expanded and reordered 0xd0 form. */
typedef struct glstate_s {
    int32_t currenttmu;                      /* original +0x00 */
    int32_t currentClientTmu;                   /* original +0x04 */
    uint32_t currenttextures[R_MAX_TEXTURE_UNITS]; /* +0x08: texture names */
    int32_t texEnv[R_MAX_TEXTURE_UNITS];        /* +0x28: texture-environment mode cache */
    uint32_t currentTextureTargets[R_MAX_TEXTURE_UNITS]; /* +0x48: GL texture targets */
    qboolean textureShaderEnabled[R_MAX_TEXTURE_UNITS]; /* +0x68: NV operation cache */
    qboolean finishCalled;                    /* original +0x88 */
    cullType_t faceCulling;                     /* original +0x8c */
    uint32_t glStateBits;                    /* original +0x90 */
    uint32_t clientStateBits;                   /* original +0x94 */
    int32_t fogMode;                            /* original +0x98 */
    int32_t fogHint;                            /* original +0x9c */
    vec4_t fogColor;                            /* original +0xa0 */
    float fogStart;                             /* original +0xb0 */
    float fogEnd;                               /* original +0xb4 */
    float fogDensity;                           /* original +0xb8 */
    renderer_static_vertex_memory_source_t currentStorageMode;                  /* original +0xbc */
    int32_t enabledLightCount;               /* original +0xc0 */
    trRefEntity_t *currentLightingEntity; /* original +0xc4 */
    uint32_t currentLightingFlags;             /* original +0xc8 */
    uint32_t normalizeTarget;                   /* original +0xcc */
} glstate_t;

#if defined(_MSC_VER)
#define RENDERER_NORETURN_IMPORT
#else
#define RENDERER_NORETURN_IMPORT __attribute__((noreturn))
#endif

/* Q3 supplies the inherited refimport_t identity. Windows CL_InitRef
 * constructs 38 consecutive function-pointer slots and GetRefAPI copies all
 * 0x26 dwords;
 * the PowerPC Mac client constructs and copies the same 38-slot table. Native
 * adaptations widen the pointers coherently because both sides are compiled
 * together; only the original i386 layout is guarded below. */
typedef struct refimport_s {
    void (*Printf)(int32_t printLevel, const char *format, ...);
    void (*Error)(errorParm_t errorLevel, const char *format, ...) RENDERER_NORETURN_IMPORT;
    int32_t (*Milliseconds)(void);
    void *(*Hunk_Alloc)(size_t size);
    void *(*Hunk_AllocateTempMemory)(size_t size);
    void *(*Z_Malloc)(size_t size);
    void (*Z_Free)(void *memory);
    void (*Hunk_FreeTempMemory)(void *memory);
    cvar_t *(*Cvar_Get)(const char *name, const char *defaultValue, uint32_t flags);
    cvar_t *(*Cvar_FindVar)(const char *name);
    void (*Cvar_Set)(const char *name, const char *value);
    void (*Cmd_AddCommand)(const char *name, void (*callback)(void));
    void (*Cmd_RemoveCommand)(const char *name);
    int32_t (*Cmd_Argc)(void); /* original +0x34 */
    const char *(*Cmd_Argv)(int32_t argumentIndex); /* original +0x38 */
    void (*Cmd_ExecuteText)(cbufExec_t when, const char *text);
    qboolean (*Com_SaveCvarsToBuffer)(const char *const *cvarNames, int32_t cvarCount, char *buffer,
                                      size_t bufferSize); /* original +0x40 */
    qboolean (*Com_LoadCvarsFromBuffer)(const char *const *cvarNames, int32_t cvarCount, char *buffer, const char *fileName); /* +0x44 */
    int32_t (*FS_FileIsInPAK)(const char *path, int32_t *checksumOut); /* original +0x48; copied
                                                     * into the table but unused
                                                     * by CoDUOMP.exe */
    int32_t (*FS_ReadFile)(const char *filename, void **buffer);
    void (*FS_FreeFile)(void *buffer);
    char **(*FS_ListFiles)(const char *path, const char *extension, int32_t *fileCount); /* original +0x54 */
    void (*FS_FreeFileList)(char **files);      /* original +0x58 */
    void (*FS_WriteFile)(const char *filename, const void *buffer, int32_t size);
    qboolean (*FS_FileExists)(const char *filename); /* original +0x60 */
    int32_t (*FS_FOpenFileRead)(const char *filename, int32_t *fileHandle, qboolean uniqueFile); /* original +0x64 */
    void (*FS_FCloseFile)(int32_t fileHandle);       /* original +0x68 */
    int32_t (*FS_Read)(void *buffer, int32_t size, int32_t fileHandle);           /* original +0x6c */
    int32_t (*FS_Write)(const void *buffer, int32_t size, int32_t fileHandle); /* original +0x70; copied into the
                                             * table but unused by CoDUOMP.exe */
    void (*CM_SaveLump)(int32_t lumpIndex, const void *buffer, int32_t size, int32_t *checksum);
    cplane_t *(*CM_PlaneForIndex)(int32_t planeIndex); /* original +0x78 */
    void (*CIN_UploadCinematic)(int32_t cinematicHandle); /* +0x7c */
    int32_t (*CIN_PlayCinematic)(const char *name, int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags); /* +0x80 */
    cinematic_status_t (*CIN_RunCinematic)(int32_t cinematicHandle); /* +0x84 */
    int32_t (*CG_GetGameModel)(int16_t modelIndex); /* original +0x88 */
    void (*CG_DObjCalcPose)(void *owner, struct DObj_s *obj, uint32_t *partBits); /* original +0x8c */
    void (*AdjustFrom640)(float *x, float *y, float *width, float *height);
    fontInfo_t *(*CL_GetFontInfo)(int32_t fontHandle, float scale);
} refimport_t;

#undef RENDERER_NORETURN_IMPORT

typedef enum renderer_print_level_e {
    R_PRINT_ALL = 0,
    R_PRINT_DEVELOPER = 1,
    R_PRINT_WARNING = 2
} renderer_print_level_t;

/* glConfig +0x5c distinguishes the two NV vertex-array-range extensions.
 * GLimp_Init stores 1 for GL_NV_vertex_array_range and 2 for
 * GL_NV_vertex_array_range2; several back-end paths select different enable
 * tokens from this mode rather than treating it as a boolean capability. */
typedef enum renderer_vertex_array_range_mode_e {
    R_VERTEX_ARRAY_RANGE_NONE = 0,
    R_VERTEX_ARRAY_RANGE_NV = 1,
    R_VERTEX_ARRAY_RANGE_NV2 = 2
} renderer_vertex_array_range_mode_t;

typedef enum renderer_register_combiner_mode_e {
    R_REGISTER_COMBINERS_UNAVAILABLE = 0,
    R_REGISTER_COMBINERS_NV = 1,
    R_REGISTER_COMBINERS_NV2 = 2
} renderer_register_combiner_mode_t;

enum renderer_state_bits_e {
    GLS_SRCBLEND_ZERO = 0x00000001,
    GLS_SRCBLEND_ONE = 0x00000002,
    GLS_SRCBLEND_DST_COLOR = 0x00000003,
    GLS_SRCBLEND_ONE_MINUS_DST_COLOR = 0x00000004,
    GLS_SRCBLEND_SRC_ALPHA = 0x00000005,
    GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA = 0x00000006,
    GLS_SRCBLEND_DST_ALPHA = 0x00000007,
    GLS_SRCBLEND_ONE_MINUS_DST_ALPHA = 0x00000008,
    GLS_SRCBLEND_ALPHA_SATURATE = 0x00000009,
    GLS_SRCBLEND_BITS = 0x0000000f,

    GLS_DSTBLEND_ZERO = 0x00000010,
    GLS_DSTBLEND_ONE = 0x00000020,
    GLS_DSTBLEND_SRC_COLOR = 0x00000030,
    GLS_DSTBLEND_ONE_MINUS_SRC_COLOR = 0x00000040,
    GLS_DSTBLEND_SRC_ALPHA = 0x00000050,
    GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA = 0x00000060,
    GLS_DSTBLEND_DST_ALPHA = 0x00000070,
    GLS_DSTBLEND_ONE_MINUS_DST_ALPHA = 0x00000080,
    GLS_DSTBLEND_BITS = 0x000000f0,

    GLS_DEPTHMASK_TRUE = 0x00000100,
    GLS_POLYMODE_LINE = 0x00001000,
    GLS_POLYGON_OFFSET = 0x00002000,
    GLS_POLYGON_OFFSET_DOUBLE = 0x00004000,
    GLS_POLYGON_OFFSET_ZERO_FACTOR = 0x00008000,
    GLS_POLYGON_OFFSET_BITS = 0x0000e000,
    GLS_DEPTHTEST_DISABLE = 0x00010000,
    GLS_DEPTHFUNC_EQUAL = 0x00020000,
    GLS_DEPTHFUNC_GREATER = 0x00040000,
    GLS_DEPTH_BITS = 0x00070000,
    GLS_LIGHTING = 0x00100000,
    GLS_FOG = 0x00200000,
    GLS_TEXTURE_SHADER_NV = 0x00400000,
    GLS_REGISTER_COMBINERS_NV = 0x00800000,
    GLS_FRAGMENT_SHADER_ATI = 0x01000000,
    GLS_VERTEX_PROGRAM_ARB = 0x02000000,

    GLS_ATEST_GT_0 = 0x10000000,
    GLS_ATEST_LT_128 = 0x20000000,
    GLS_ATEST_GE_128 = 0x40000000,
    GLS_ATEST_BITS = 0x70000000
};

enum renderer_client_state_bits_e {
    GLS_CLIENT_TEXCOORD0_ARRAY = 0x00000001,
    GLS_CLIENT_TEXCOORD1_ARRAY = 0x00000002,
    GLS_CLIENT_TEXCOORD2_ARRAY = 0x00000004,
    GLS_CLIENT_TEXCOORD3_ARRAY = 0x00000008,
    GLS_CLIENT_TEXCOORD4_ARRAY = 0x00000010,
    GLS_CLIENT_TEXCOORD5_ARRAY = 0x00000020,
    GLS_CLIENT_TEXCOORD6_ARRAY = 0x00000040,
    GLS_CLIENT_TEXCOORD7_ARRAY = 0x00000080,
    GLS_CLIENT_COLOR_ARRAY = 0x00000100,
    GLS_CLIENT_NORMAL_ARRAY = 0x00000200,
    GLS_CLIENT_VERTEX_ARRAY = 0x00000400
};

extern trGlobals_t tr;
extern glstate_t glState;
extern refimport_t ri;
extern glconfig_t glConfig;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(frontEndCounters_t) == 0x04, "i386 renderer front-end counters alignment changed");
_Static_assert(offsetof(frontEndCounters_t, patchSphereCullIn) == 0x00, "i386 renderer sphere-cull-in counter offset changed");
_Static_assert(sizeof(((frontEndCounters_t *)0)->patchSphereCullIn) == 0x04, "i386 renderer sphere-cull-in counter extent changed");
_Static_assert(offsetof(frontEndCounters_t, patchSphereCullClip) == 0x04, "i386 renderer sphere-cull-clip counter offset changed");
_Static_assert(sizeof(((frontEndCounters_t *)0)->patchSphereCullClip) == 0x04, "i386 renderer sphere-cull-clip counter extent changed");
_Static_assert(offsetof(frontEndCounters_t, patchSphereCullOut) == 0x08, "i386 renderer sphere-cull-out counter offset changed");
_Static_assert(sizeof(((frontEndCounters_t *)0)->patchSphereCullOut) == 0x04, "i386 renderer sphere-cull-out counter extent changed");
_Static_assert(offsetof(frontEndCounters_t, patchBoxCullIn) == 0x0c, "i386 renderer box-cull-in counter offset changed");
_Static_assert(sizeof(((frontEndCounters_t *)0)->patchBoxCullIn) == 0x04, "i386 renderer box-cull-in counter extent changed");
_Static_assert(offsetof(frontEndCounters_t, patchBoxCullClip) == 0x10, "i386 renderer box-cull-clip counter offset changed");
_Static_assert(sizeof(((frontEndCounters_t *)0)->patchBoxCullClip) == 0x04, "i386 renderer box-cull-clip counter extent changed");
_Static_assert(offsetof(frontEndCounters_t, patchBoxCullOut) == 0x14, "i386 renderer box-cull-out counter offset changed");
_Static_assert(sizeof(((frontEndCounters_t *)0)->patchBoxCullOut) == 0x04, "i386 renderer box-cull-out counter extent changed");
_Static_assert(offsetof(frontEndCounters_t, leafCount) == 0x18, "i386 renderer leaf counter offset changed");
_Static_assert(sizeof(((frontEndCounters_t *)0)->leafCount) == 0x04, "i386 renderer leaf counter extent changed");
_Static_assert(offsetof(frontEndCounters_t, dlightSurfaceCount) == 0x1c, "i386 renderer dlight-surface counter offset changed");
_Static_assert(sizeof(((frontEndCounters_t *)0)->dlightSurfaceCount) == 0x04, "i386 renderer dlight-surface counter extent changed");
_Static_assert(offsetof(frontEndCounters_t, dlightSurfaceCullCount) == 0x20, "i386 renderer culled-dlight counter offset changed");
_Static_assert(sizeof(((frontEndCounters_t *)0)->dlightSurfaceCullCount) == 0x04, "i386 renderer culled-dlight counter extent changed");
_Static_assert(sizeof(frontEndCounters_t) == 0x24, "original i386 renderer front-end counters size changed");

#define RENDERER_ASSERT_I386_IMPORT_SLOT(member, slot) \
    _Static_assert(offsetof(refimport_t, member) == (slot) * 4, "i386 renderer import slot " #member " moved"); \
    _Static_assert(sizeof(((refimport_t *)0)->member) == 4, "i386 renderer import slot " #member " extent changed")
_Static_assert(_Alignof(refimport_t) == 0x04, "i386 renderer import-table alignment changed");
RENDERER_ASSERT_I386_IMPORT_SLOT(Printf, 0);
RENDERER_ASSERT_I386_IMPORT_SLOT(Error, 1);
RENDERER_ASSERT_I386_IMPORT_SLOT(Milliseconds, 2);
RENDERER_ASSERT_I386_IMPORT_SLOT(Hunk_Alloc, 3);
RENDERER_ASSERT_I386_IMPORT_SLOT(Hunk_AllocateTempMemory, 4);
RENDERER_ASSERT_I386_IMPORT_SLOT(Z_Malloc, 5);
RENDERER_ASSERT_I386_IMPORT_SLOT(Z_Free, 6);
RENDERER_ASSERT_I386_IMPORT_SLOT(Hunk_FreeTempMemory, 7);
RENDERER_ASSERT_I386_IMPORT_SLOT(Cvar_Get, 8);
RENDERER_ASSERT_I386_IMPORT_SLOT(Cvar_FindVar, 9);
RENDERER_ASSERT_I386_IMPORT_SLOT(Cvar_Set, 10);
RENDERER_ASSERT_I386_IMPORT_SLOT(Cmd_AddCommand, 11);
RENDERER_ASSERT_I386_IMPORT_SLOT(Cmd_RemoveCommand, 12);
RENDERER_ASSERT_I386_IMPORT_SLOT(Cmd_Argc, 13);
RENDERER_ASSERT_I386_IMPORT_SLOT(Cmd_Argv, 14);
RENDERER_ASSERT_I386_IMPORT_SLOT(Cmd_ExecuteText, 15);
RENDERER_ASSERT_I386_IMPORT_SLOT(Com_SaveCvarsToBuffer, 16);
RENDERER_ASSERT_I386_IMPORT_SLOT(Com_LoadCvarsFromBuffer, 17);
RENDERER_ASSERT_I386_IMPORT_SLOT(FS_FileIsInPAK, 18);
RENDERER_ASSERT_I386_IMPORT_SLOT(FS_ReadFile, 19);
RENDERER_ASSERT_I386_IMPORT_SLOT(FS_FreeFile, 20);
RENDERER_ASSERT_I386_IMPORT_SLOT(FS_ListFiles, 21);
RENDERER_ASSERT_I386_IMPORT_SLOT(FS_FreeFileList, 22);
RENDERER_ASSERT_I386_IMPORT_SLOT(FS_WriteFile, 23);
RENDERER_ASSERT_I386_IMPORT_SLOT(FS_FileExists, 24);
RENDERER_ASSERT_I386_IMPORT_SLOT(FS_FOpenFileRead, 25);
RENDERER_ASSERT_I386_IMPORT_SLOT(FS_FCloseFile, 26);
RENDERER_ASSERT_I386_IMPORT_SLOT(FS_Read, 27);
RENDERER_ASSERT_I386_IMPORT_SLOT(FS_Write, 28);
RENDERER_ASSERT_I386_IMPORT_SLOT(CM_SaveLump, 29);
RENDERER_ASSERT_I386_IMPORT_SLOT(CM_PlaneForIndex, 30);
RENDERER_ASSERT_I386_IMPORT_SLOT(CIN_UploadCinematic, 31);
RENDERER_ASSERT_I386_IMPORT_SLOT(CIN_PlayCinematic, 32);
RENDERER_ASSERT_I386_IMPORT_SLOT(CIN_RunCinematic, 33);
RENDERER_ASSERT_I386_IMPORT_SLOT(CG_GetGameModel, 34);
RENDERER_ASSERT_I386_IMPORT_SLOT(CG_DObjCalcPose, 35);
RENDERER_ASSERT_I386_IMPORT_SLOT(AdjustFrom640, 36);
RENDERER_ASSERT_I386_IMPORT_SLOT(CL_GetFontInfo, 37);
_Static_assert(sizeof(refimport_t) == 0x98, "original i386 renderer import-table size changed");
#undef RENDERER_ASSERT_I386_IMPORT_SLOT
_Static_assert(sizeof(renderer_image_track_t) == 0x04, "i386 image-track enum width changed");
_Static_assert(sizeof(renderer_image_link_t) == 0x04, "i386 image link-union size changed");
_Static_assert(_Alignof(renderer_image_link_t) == 0x04, "i386 image link-union alignment changed");
_Static_assert(offsetof(renderer_image_sheet_state_t, x) == 0x00, "i386 image sheet X moved");
_Static_assert(offsetof(renderer_image_sheet_state_t, y) == 0x02, "i386 image sheet Y moved");
_Static_assert(offsetof(renderer_image_sheet_state_t, rotated) == 0x04, "i386 image sheet rotation marker moved");
_Static_assert(sizeof(renderer_image_sheet_state_t) == 0x06, "i386 image sheet-state size changed");
_Static_assert(_Alignof(renderer_image_sheet_state_t) == 0x02, "i386 image sheet-state alignment changed");
_Static_assert(offsetof(renderer_image_delay_state_t, group) == 0x00, "i386 delayed-image group moved");
_Static_assert(offsetof(renderer_image_delay_state_t, groupTriCount) == 0x04, "i386 delayed-image triangle count moved");
_Static_assert(sizeof(renderer_image_delay_state_t) == 0x08, "i386 delayed-image state size changed");
_Static_assert(_Alignof(renderer_image_delay_state_t) == 0x04, "i386 delayed-image state alignment changed");
_Static_assert(sizeof(renderer_image_state_t) == 0x08, "i386 image state-union size changed");
_Static_assert(_Alignof(renderer_image_state_t) == 0x04, "i386 image state-union alignment changed");
_Static_assert(offsetof(image_t, imgName) == 0x00, "i386 image name offset changed");
_Static_assert(offsetof(image_t, width) == 0x40, "i386 image width offset changed");
_Static_assert(offsetof(image_t, height) == 0x42, "i386 image height offset changed");
_Static_assert(offsetof(image_t, uploadWidth) == 0x44, "i386 image upload-width offset changed");
_Static_assert(offsetof(image_t, uploadHeight) == 0x46, "i386 image upload-height offset changed");
_Static_assert(offsetof(image_t, cardMemory) == 0x48, "i386 image card-memory offset changed");
_Static_assert(offsetof(image_t, textureMemory) == 0x4c, "i386 image texture-memory offset changed");
_Static_assert(offsetof(image_t, imageTrack) == 0x50, "i386 image tracking category moved");
_Static_assert(offsetof(image_t, target) == 0x54, "i386 image target offset changed");
_Static_assert(offsetof(image_t, texnum) == 0x58, "i386 image texnum offset changed");
_Static_assert(offsetof(image_t, frameUsed) == 0x5c, "i386 image frameUsed offset changed");
_Static_assert(offsetof(image_t, internalFormat) == 0x60, "i386 image internal-format offset changed");
_Static_assert(offsetof(image_t, flags) == 0x64, "i386 image flags offset changed");
_Static_assert(offsetof(image_t, link) == 0x68, "i386 image link union moved");
_Static_assert(offsetof(image_t, link.textureSheet) == 0x68, "i386 image texture-sheet offset changed");
_Static_assert(offsetof(image_t, link.delayedShader) == 0x68, "i386 image delayed-shader offset changed");
_Static_assert(offsetof(image_t, state) == 0x6c, "i386 image state union moved");
_Static_assert(offsetof(image_t, state.sheet.x) == 0x6c, "i386 image texture-sheet X offset changed");
_Static_assert(offsetof(image_t, state.sheet.y) == 0x6e, "i386 image texture-sheet Y offset changed");
_Static_assert(offsetof(image_t, state.sheet.rotated) == 0x70, "i386 image texture-sheet rotation marker moved");
_Static_assert(offsetof(image_t, state.delay.group) == 0x6c, "i386 image delayed-group offset changed");
_Static_assert(offsetof(image_t, state.delay.groupTriCount) == 0x70, "i386 image delayed-group triangle count moved");
_Static_assert(offsetof(image_t, hashNext) == 0x74, "i386 image hash link moved");
_Static_assert(sizeof(image_t) == 0x78, "i386 image record size changed");
_Static_assert(_Alignof(renderer_image_group_node_t) == 0x04, "i386 texture-sheet group-node alignment changed");
_Static_assert(offsetof(renderer_image_group_node_t, child) == 0x00, "i386 texture-sheet child offset changed");
_Static_assert(sizeof(((renderer_image_group_node_t *)0)->child) == 0x04, "i386 texture-sheet child extent changed");
_Static_assert(offsetof(renderer_image_group_node_t, image) == 0x00, "i386 texture-sheet image offset changed");
_Static_assert(sizeof(((renderer_image_group_node_t *)0)->image) == 0x04, "i386 texture-sheet image extent changed");
_Static_assert(offsetof(renderer_image_group_node_t, next) == 0x04, "i386 texture-sheet next offset changed");
_Static_assert(sizeof(((renderer_image_group_node_t *)0)->next) == 0x04, "i386 texture-sheet next extent changed");
_Static_assert(offsetof(renderer_image_group_node_t, triangleCount) == 0x08, "i386 texture-sheet group triangle count offset changed");
_Static_assert(sizeof(((renderer_image_group_node_t *)0)->triangleCount) == 0x04, "i386 texture-sheet group triangle-count extent changed");
_Static_assert(offsetof(renderer_image_group_node_t, group) == 0x0c, "i386 texture-sheet group identifier offset changed");
_Static_assert(sizeof(((renderer_image_group_node_t *)0)->group) == 0x04, "i386 texture-sheet group identifier extent changed");
_Static_assert(offsetof(renderer_image_group_node_t, width) == 0x10, "i386 texture-sheet group width offset changed");
_Static_assert(sizeof(((renderer_image_group_node_t *)0)->width) == 0x04, "i386 texture-sheet group width extent changed");
_Static_assert(offsetof(renderer_image_group_node_t, height) == 0x14, "i386 texture-sheet group height offset changed");
_Static_assert(sizeof(((renderer_image_group_node_t *)0)->height) == 0x04, "i386 texture-sheet group height extent changed");
_Static_assert(sizeof(renderer_image_group_node_t) == 0x18, "original i386 texture-sheet group-node size changed");
_Static_assert(offsetof(world_t, baseName) == 0x40, "i386 renderer world base-name offset changed");
_Static_assert(offsetof(world_t, dataSize) == 0x80, "i386 renderer world allocation-size offset changed");
_Static_assert(offsetof(world_t, numShaders) == 0x84, "i386 renderer world disk-shader count moved");
_Static_assert(offsetof(world_t, shaders) == 0x88, "i386 renderer world disk-shader table moved");
_Static_assert(offsetof(world_t, bmodels) == 0x8c, "i386 renderer world brush-model table moved");
_Static_assert(offsetof(world_t, numnodes) == 0x90, "i386 renderer world node/leaf count moved");
_Static_assert(offsetof(world_t, numDecisionNodes) == 0x94, "i386 renderer world internal-node count moved");
_Static_assert(offsetof(world_t, nodes) == 0x98, "i386 renderer world-node root offset changed");
_Static_assert(offsetof(world_t, numsurfaces) == 0x9c, "i386 renderer world-surface count moved");
_Static_assert(offsetof(world_t, surfaces) == 0xa0, "i386 renderer world-surface table moved");
_Static_assert(offsetof(world_t, skySurfaceCount) == 0xa4, "i386 renderer world sky-surface count moved");
_Static_assert(offsetof(world_t, skySurfaces) == 0xa8, "i386 renderer world sky-surface table moved");
_Static_assert(offsetof(world_t, aabbTreeCount) == 0xac, "i386 renderer world AABB-tree count moved");
_Static_assert(offsetof(world_t, aabbTrees) == 0xb0, "i386 renderer world AABB-tree table moved");
_Static_assert(offsetof(world_t, numClusters) == 0xb4, "i386 renderer world cluster count moved");
_Static_assert(offsetof(world_t, entityString) == 0xb8, "i386 renderer world entity string moved");
_Static_assert(offsetof(world_t, entityParsePoint) == 0xbc, "i386 renderer world entity parse point moved");
_Static_assert(_Alignof(renderer_world_corona_t) == 0x04, "i386 renderer world-corona alignment changed");
_Static_assert(offsetof(renderer_world_corona_t, shader) == 0x00, "i386 renderer world-corona shader offset changed");
_Static_assert(sizeof(((renderer_world_corona_t *)0)->shader) == 0x04, "i386 renderer world-corona shader extent changed");
_Static_assert(offsetof(renderer_world_corona_t, origin) == 0x04, "i386 renderer world-corona origin offset changed");
_Static_assert(sizeof(((renderer_world_corona_t *)0)->origin) == 0x0c, "i386 renderer world-corona origin extent changed");
_Static_assert(offsetof(renderer_world_corona_t, scale) == 0x10, "i386 renderer world-corona scale offset changed");
_Static_assert(sizeof(((renderer_world_corona_t *)0)->scale) == 0x04, "i386 renderer world-corona scale extent changed");
_Static_assert(offsetof(renderer_world_corona_t, zCutoff) == 0x14, "i386 renderer world-corona Z-cutoff offset changed");
_Static_assert(sizeof(((renderer_world_corona_t *)0)->zCutoff) == 0x04, "i386 renderer world-corona Z-cutoff extent changed");
_Static_assert(offsetof(renderer_world_corona_t, zFadeOut) == 0x18, "i386 renderer world-corona Z-fade offset changed");
_Static_assert(sizeof(((renderer_world_corona_t *)0)->zFadeOut) == 0x04, "i386 renderer world-corona Z-fade extent changed");
_Static_assert(offsetof(renderer_world_corona_t, color) == 0x1c, "i386 renderer world-corona color offset changed");
_Static_assert(sizeof(((renderer_world_corona_t *)0)->color) == 0x04, "i386 renderer world-corona color extent changed");
_Static_assert(sizeof(renderer_world_corona_t) == 0x20, "original i386 renderer world-corona size changed");
_Static_assert(offsetof(world_t, coronas) == 0xc0, "i386 renderer world-corona table moved");
_Static_assert(offsetof(world_t, coronaCount) == 0xc4, "i386 renderer world-corona count moved");
_Static_assert(offsetof(world_t, lights) == 0x110, "i386 renderer world-light table offset changed");
_Static_assert(offsetof(world_t, entitySunLightIntensity) == 0x108, "i386 renderer entity-sun intensity offset changed");
_Static_assert(offsetof(world_t, sunDiffuseColor) == 0xe8, "i386 renderer sun diffuse color moved");
_Static_assert(offsetof(world_t, lightCount) == 0x10c, "i386 renderer world-light count moved");
_Static_assert(offsetof(world_t, cellCount) == 0x120, "i386 renderer world-cell count moved");
_Static_assert(offsetof(world_t, cells) == 0x124, "i386 renderer world-cell table moved");
_Static_assert(offsetof(world_t, occluders) == 0x12c, "i386 renderer world-occluder table moved");
_Static_assert(offsetof(world_t, occluderIndexes) == 0x134, "i386 renderer world-occluder index table moved");
_Static_assert(offsetof(world_t, portals) == 0x13c, "i386 renderer world-portal table moved");
_Static_assert(offsetof(world_t, portalVerts) == 0x140, "i386 renderer world portal-vertex table moved");
_Static_assert(offsetof(world_t, cullGroups) == 0x148, "i386 renderer world cull-group table moved");
_Static_assert(offsetof(world_t, cullGroupIndexes) == 0x150, "i386 renderer world cull-group index table moved");
_Static_assert(offsetof(world_t, sunLight) == 0x114, "i386 renderer world sun-light offset changed");
_Static_assert(offsetof(world_t, lightIndexCount) == 0x118, "i386 renderer world light-index count moved");
_Static_assert(offsetof(world_t, lightIndexes) == 0x11c, "i386 renderer world light-index offset changed");
_Static_assert(offsetof(world_t, cellCount) == 0x120, "i386 renderer world cell-count offset changed");
_Static_assert(offsetof(world_t, cells) == 0x124, "i386 renderer world cell-array offset changed");
_Static_assert(offsetof(trGlobals_t, registered) == 0x000, "i386 renderer registration marker moved");
_Static_assert(offsetof(trGlobals_t, frameCount) == 0x004, "i386 renderer frame-count offset changed");
_Static_assert(offsetof(trGlobals_t, imageCount) == 0x4afc, "i386 renderer image-count offset changed");
_Static_assert(offsetof(trGlobals_t, models) == 0x0ad8, "i386 renderer model-registry offset changed");
_Static_assert(sizeof(renderer_model_type_t) == 0x04, "i386 renderer model-type enum width changed");
_Static_assert(_Alignof(renderer_model_type_t) == 0x04, "i386 renderer model-type enum alignment changed");
_Static_assert(offsetof(model_t, name) == 0x00, "i386 renderer model name moved");
_Static_assert(offsetof(model_t, type) == 0x40, "i386 renderer model type moved");
_Static_assert(offsetof(model_t, index) == 0x44, "i386 renderer model registry index moved");
_Static_assert(offsetof(model_t, dataSize) == 0x48, "i386 renderer dormant data-size slot moved");
_Static_assert(offsetof(model_t, bmodel) == 0x4c, "i386 renderer brush-model pointer offset changed");
_Static_assert(offsetof(model_t, shaderHandles) == 0x50, "i386 renderer model shader-handle table offset changed");
_Static_assert(offsetof(model_t, xmodel) == 0x54, "i386 renderer model XModel offset changed");
_Static_assert(offsetof(model_t, legacyMd3Lod2) == 0x58, "i386 renderer dormant legacy-model pointer moved");
_Static_assert(offsetof(model_t, numLods) == 0x5c, "i386 renderer model LOD-count offset changed");
_Static_assert(_Alignof(model_t) == 0x04, "i386 renderer model alignment changed");
_Static_assert(sizeof(model_t) == 0x60, "i386 renderer model size changed");
_Static_assert(offsetof(trGlobals_t, modelCount) == 0x2ad8, "i386 renderer model-count offset changed");
_Static_assert(offsetof(trGlobals_t, xmodelRefreshModelIndex) == 0x2adc, "i386 XModel-refresh model cursor offset changed");
_Static_assert(offsetof(trGlobals_t, xmodelRefreshLodIndex) == 0x2ae0, "i386 XModel-refresh LOD cursor offset changed");
_Static_assert(offsetof(trGlobals_t, xmodelRefreshSurfaceIndex) == 0x2ae4, "i386 XModel-refresh surface cursor offset changed");
_Static_assert(offsetof(trGlobals_t, staticModelRefreshModelIndex) == 0x2ae8, "i386 static-model-refresh model cursor offset changed");
_Static_assert(offsetof(trGlobals_t, staticModelRefreshLodIndex) == 0x2aec, "i386 static-model-refresh LOD cursor offset changed");
_Static_assert(offsetof(trGlobals_t, staticModelRefreshSurfaceIndex) == 0x2af0, "i386 static-model-refresh surface cursor offset changed");
_Static_assert(offsetof(trGlobals_t, worldRefreshSurfaceIndex) == 0x2af4, "i386 world-refresh surface cursor offset changed");
_Static_assert(offsetof(trGlobals_t, registeredStaticModels) == 0x2af8, "i386 static-model registry offset changed");
_Static_assert(offsetof(trGlobals_t, registeredStaticModelCount) == 0x4af8, "i386 static-model count offset changed");
_Static_assert(offsetof(trGlobals_t, ignorePrecacheErrorCount) == 0x120, "i386 renderer precache-error counter offset changed");
_Static_assert(offsetof(trGlobals_t, modelsFinishedLoading) == 0x124, "i386 renderer model-loading marker offset changed");
_Static_assert(offsetof(trGlobals_t, delayedImageGroup) == 0x128, "i386 renderer delayed-image group offset changed");
_Static_assert(offsetof(trGlobals_t, delayedImageGroupTriCount) == 0x12c, "i386 renderer delayed-image triangle count moved");
_Static_assert(offsetof(trGlobals_t, delayedImageGroupTileMode) == 0x130, "i386 renderer delayed-image tile mode moved");
_Static_assert(offsetof(trGlobals_t, delayedImageGroupSequence) == 0x134, "i386 renderer delayed-image sequence offset changed");
_Static_assert(offsetof(trGlobals_t, delayedImageCount) == 0x138, "i386 renderer delayed-image count offset changed");
_Static_assert(offsetof(trGlobals_t, showTrisShader) == 0x13c, "i386 show-tris shader offset changed");
_Static_assert(offsetof(trGlobals_t, showImagesShader) == 0x140, "i386 show-images shader offset changed");
_Static_assert(offsetof(trGlobals_t, lightmapCount) == 0x144, "i386 renderer lightmap-count offset changed");
_Static_assert(offsetof(trGlobals_t, lightmaps) == 0x148, "i386 renderer lightmap-registry offset changed");
_Static_assert(offsetof(trGlobals_t, numShaders) == 0x6b0c, "i386 renderer shader-count offset changed");
_Static_assert(offsetof(trGlobals_t, shaders) == 0x6b10, "i386 renderer shader-registry offset changed");
_Static_assert(offsetof(trGlobals_t, sortedShaders) == 0xab10, "i386 renderer sorted-shader registry offset changed");
_Static_assert(offsetof(trGlobals_t, identityLightByte) == 0x874, "i386 renderer identity-light byte offset changed");
_Static_assert(offsetof(trGlobals_t, overbrightBits) == 0x878, "i386 renderer overbright-bits offset changed");
_Static_assert(offsetof(trGlobals_t, currentEntity) == 0x348, "i386 renderer current-entity offset changed");
_Static_assert(offsetof(trGlobals_t, worldEntity) == 0x34c, "i386 renderer world-entity offset changed");
_Static_assert(offsetof(trGlobals_t, world) == 0x014, "i386 renderer world pointer offset changed");
_Static_assert(offsetof(trGlobals_t, worldMapLoaded) == 0x010, "i386 renderer world-loaded marker moved");
_Static_assert(offsetof(trGlobals_t, defaultImage) == 0x018, "i386 renderer default-image offset changed");
_Static_assert(offsetof(trGlobals_t, scratchImages) == 0x01c, "i386 renderer scratch-image array offset changed");
_Static_assert(offsetof(trGlobals_t, dlightImage) == 0x09c, "i386 renderer dlight-image offset changed");
_Static_assert(offsetof(trGlobals_t, flareImage) == 0x0a0, "i386 renderer dormant flare-image slot moved");
_Static_assert(offsetof(trGlobals_t, whiteImage) == 0x0a4, "i386 renderer white-image offset changed");
_Static_assert(offsetof(trGlobals_t, identityLightImage) == 0x0a8, "i386 renderer identity-light-image offset changed");
_Static_assert(offsetof(trGlobals_t, grayImage) == 0x0ac, "i386 renderer gray-image offset changed");
_Static_assert(offsetof(trGlobals_t, screenImage) == 0x0b0, "i386 renderer saved-screen image offset changed");
_Static_assert(offsetof(trGlobals_t, screenImageWidth) == 0x0b4, "i386 renderer saved-screen width offset changed");
_Static_assert(offsetof(trGlobals_t, screenImageHeight) == 0x0b8, "i386 renderer saved-screen height offset changed");
_Static_assert(offsetof(trGlobals_t, screenImageSaveTime) == 0x0bc, "i386 renderer saved-screen time offset changed");
_Static_assert(offsetof(trGlobals_t, screenImageSMax) == 0x0c0, "i386 renderer saved-screen S extent offset changed");
_Static_assert(offsetof(trGlobals_t, screenImageTMax) == 0x0c4, "i386 renderer saved-screen T extent offset changed");
_Static_assert(offsetof(trGlobals_t, stencilShadowShader) == 0x0cc, "i386 renderer stencil-shadow shader offset changed");
_Static_assert(offsetof(trGlobals_t, defaultShader) == 0x0c8, "i386 renderer default-shader offset changed");
_Static_assert(offsetof(trGlobals_t, screenImageShader) == 0x0d0, "i386 renderer saved-screen shader offset changed");
_Static_assert(offsetof(trGlobals_t, dlightShader) == 0x0d4, "i386 renderer dynamic-light shader offset changed");
_Static_assert(offsetof(trGlobals_t, flareShader) == 0x0d8, "i386 renderer flare shader offset changed");
_Static_assert(offsetof(trGlobals_t, spotLightShader) == 0x0dc, "i386 renderer spotlight shader offset changed");
_Static_assert(offsetof(trGlobals_t, sunName) == 0x0e0, "i386 renderer sun-name buffer moved");
_Static_assert(offsetof(trGlobals_t, viewCount) == 0x008, "i386 renderer view-count offset changed");
_Static_assert(offsetof(trGlobals_t, frameSceneNum) == 0x00c, "i386 renderer frame-scene-number offset changed");
_Static_assert(offsetof(trGlobals_t, currentEntityNumber) == 0x604, "i386 renderer current-entity-number offset changed");
_Static_assert(offsetof(trGlobals_t, shiftedEntityNumber) == 0x608, "i386 renderer shifted-entity-number offset changed");
_Static_assert(offsetof(trGlobals_t, currentModel) == 0x60c, "i386 renderer dormant current-model slot moved");
_Static_assert(offsetof(trGlobals_t, viewParms) == 0x610, "i386 renderer front-end view-parms offset changed");
_Static_assert(offsetof(trGlobals_t, identityLight) == 0x870, "i386 renderer identity-light offset changed");
_Static_assert(offsetof(trGlobals_t, orientation) == 0x87c, "i386 renderer orientation offset changed");
_Static_assert(offsetof(trGlobals_t, refdef) == 0x8f8, "i386 renderer front-end refdef offset changed");
_Static_assert(offsetof(trGlobals_t, sunLight) == 0xa84, "i386 renderer sun-light offset changed");
_Static_assert(offsetof(trGlobals_t, sunDirection) == 0xa90, "i386 renderer sun-direction offset changed");
_Static_assert(offsetof(trGlobals_t, viewCluster) == 0xa80, "i386 renderer view-cluster offset changed");
_Static_assert(offsetof(trGlobals_t, maxEntityLights) == 0xa9c, "i386 renderer maximum entity-light offset changed");
_Static_assert(offsetof(trGlobals_t, diffuseSunQuality) == 0xaa0, "i386 renderer diffuse-sun quality offset changed");
_Static_assert(offsetof(trGlobals_t, diffuseSunSteps) == 0xaa4, "i386 renderer diffuse-sun step-count offset changed");
_Static_assert(offsetof(trGlobals_t, diffuseSunSampleScale) == 0xaa8, "i386 renderer diffuse-sun sample-scale offset changed");
_Static_assert(offsetof(trGlobals_t, pc) == 0xaac, "i386 renderer front-end counters moved");
_Static_assert(offsetof(trGlobals_t, frontEndMsec) == 0xad0, "i386 renderer front-end time offset changed");
_Static_assert(offsetof(trGlobals_t, frameStatistics) == 0xad4, "i386 renderer frame-statistics pointer moved");
_Static_assert(offsetof(trGlobals_t, images) == 0x4b00, "i386 renderer image-registry offset changed");
_Static_assert(offsetof(trGlobals_t, imageMemory) == 0x6b00, "i386 renderer image-memory total moved");
_Static_assert(offsetof(trGlobals_t, fxImageMemory) == 0x6b04, "i386 renderer FX image-memory total moved");
_Static_assert(offsetof(trGlobals_t, unrecovered6b08) == 0x6b08, "i386 renderer dormant image-registry slot moved");
_Static_assert(offsetof(trGlobals_t, fragmentShaderCount) == 0xeb10, "i386 renderer fragment-shader count moved");
_Static_assert(offsetof(trGlobals_t, sinTable) == 0xeb14, "i386 renderer sine table moved");
_Static_assert(offsetof(trGlobals_t, squareTable) == 0xfb14, "i386 renderer square-wave table moved");
_Static_assert(offsetof(trGlobals_t, triangleTable) == 0x10b14, "i386 renderer triangle-wave table moved");
_Static_assert(offsetof(trGlobals_t, sawToothTable) == 0x11b14, "i386 renderer sawtooth table moved");
_Static_assert(offsetof(trGlobals_t, inverseSawToothTable) == 0x12b14, "i386 renderer inverse-sawtooth table moved");
_Static_assert(offsetof(trGlobals_t, staticVertexMemorySecondary) == 0x13b14, "i386 secondary static-memory base moved");
_Static_assert(offsetof(trGlobals_t, staticVertexMemorySecondaryLimit) == 0x13b18, "i386 secondary static-memory limit moved");
_Static_assert(offsetof(trGlobals_t, staticVertexMemorySecondaryUsed) == 0x13b1c, "i386 secondary static-memory usage moved");
_Static_assert(offsetof(trGlobals_t, staticVertexMemoryPrimary) == 0x13b20, "i386 primary static-memory base moved");
_Static_assert(offsetof(trGlobals_t, staticVertexMemoryPrimaryLimit) == 0x13b24, "i386 primary static-memory limit moved");
_Static_assert(offsetof(trGlobals_t, staticVertexMemoryPrimaryUsed) == 0x13b28, "i386 primary static-memory usage moved");
_Static_assert(offsetof(trGlobals_t, defaultStorageMode) == 0x13b2c, "i386 default vertex-storage mode moved");
_Static_assert(offsetof(trGlobals_t, dynamicBufferFrameSerial) == 0x13b30, "i386 dynamic-buffer frame serial moved");
_Static_assert(offsetof(trGlobals_t, dynamicBufferMaxFrameSerial) == 0x13b34, "i386 dynamic-buffer maximum frame serial moved");
_Static_assert(offsetof(trGlobals_t, cachedStaticModelStorageSource) == 0x13b38, "i386 cached-static-model storage source moved");
_Static_assert(offsetof(trGlobals_t, cachedStaticModelSurfaceType) == 0x13b3c, "i386 cached-static-model surface type moved");
_Static_assert(offsetof(trGlobals_t, cachedStaticModelStorage) == 0x13b40, "i386 cached-static-model storage base moved");
_Static_assert(offsetof(trGlobals_t, cachedStaticModelStorageOffset) == 0x13b44, "i386 cached-static-model storage offset moved");
_Static_assert(offsetof(trGlobals_t, vboStreamDraw) == 0x13b48, "i386 VBO stream-draw marker moved");
_Static_assert(offsetof(trGlobals_t, vboInterleaved) == 0x13b4c, "i386 VBO interleave marker moved");
_Static_assert(offsetof(trGlobals_t, vboUsage) == 0x13b50, "i386 VBO usage token moved");
_Static_assert(offsetof(trGlobals_t, stageIteratorFunc) == 0x13b54, "i386 stage-iterator callback moved");
_Static_assert(_Alignof(trGlobals_t) == 0x04, "i386 renderer-global alignment changed");
_Static_assert(sizeof(trGlobals_t) == 0x13b58, "i386 renderer-global size changed");
_Static_assert(_Alignof(glstate_t) == 0x04, "i386 GL-state alignment changed");
_Static_assert(offsetof(glstate_t, currenttmu) == 0x00, "i386 GL-state current texture unit moved");
_Static_assert(sizeof(((glstate_t *)0)->currenttmu) == 0x04, "i386 GL-state current texture-unit extent changed");
_Static_assert(offsetof(glstate_t, currentClientTmu) == 0x04, "i386 GL-state current client texture unit moved");
_Static_assert(sizeof(((glstate_t *)0)->currentClientTmu) == 0x04, "i386 GL-state current client texture-unit extent changed");
_Static_assert(offsetof(glstate_t, currenttextures) == 0x08, "i386 GL-state bound-texture array moved");
_Static_assert(sizeof(((glstate_t *)0)->currenttextures) == 0x20, "i386 GL-state bound-texture array extent changed");
_Static_assert(offsetof(glstate_t, texEnv) == 0x28, "i386 GL-state texture-environment array moved");
_Static_assert(sizeof(((glstate_t *)0)->texEnv) == 0x20, "i386 GL-state texture-environment array extent changed");
_Static_assert(offsetof(glstate_t, currentTextureTargets) == 0x48, "i386 GL-state texture-target array moved");
_Static_assert(sizeof(((glstate_t *)0)->currentTextureTargets) == 0x20, "i386 GL-state texture-target array extent changed");
_Static_assert(offsetof(glstate_t, textureShaderEnabled) == 0x68, "i386 GL-state texture-shader array moved");
_Static_assert(sizeof(((glstate_t *)0)->textureShaderEnabled) == 0x20, "i386 GL-state texture-shader array extent changed");
_Static_assert(offsetof(glstate_t, finishCalled) == 0x88, "i386 GL-state finish-called flag moved");
_Static_assert(sizeof(((glstate_t *)0)->finishCalled) == 0x04, "i386 GL-state finish-called flag extent changed");
_Static_assert(offsetof(glstate_t, faceCulling) == 0x8c, "i386 GL-state face-culling mode moved");
_Static_assert(sizeof(((glstate_t *)0)->faceCulling) == 0x04, "i386 GL-state face-culling extent changed");
_Static_assert(offsetof(glstate_t, glStateBits) == 0x90, "i386 GL-state server-state bits moved");
_Static_assert(sizeof(((glstate_t *)0)->glStateBits) == 0x04, "i386 GL-state server-state extent changed");
_Static_assert(offsetof(glstate_t, clientStateBits) == 0x94, "i386 GL-state client-state bits moved");
_Static_assert(sizeof(((glstate_t *)0)->clientStateBits) == 0x04, "i386 GL-state client-state extent changed");
_Static_assert(offsetof(glstate_t, fogMode) == 0x98, "i386 GL-state fog mode moved");
_Static_assert(sizeof(((glstate_t *)0)->fogMode) == 0x04, "i386 GL-state fog-mode extent changed");
_Static_assert(offsetof(glstate_t, fogHint) == 0x9c, "i386 GL-state fog hint moved");
_Static_assert(sizeof(((glstate_t *)0)->fogHint) == 0x04, "i386 GL-state fog-hint extent changed");
_Static_assert(offsetof(glstate_t, fogColor) == 0xa0, "i386 GL-state fog color moved");
_Static_assert(sizeof(((glstate_t *)0)->fogColor) == 0x10, "i386 GL-state fog-color extent changed");
_Static_assert(offsetof(glstate_t, fogStart) == 0xb0, "i386 GL-state fog start moved");
_Static_assert(sizeof(((glstate_t *)0)->fogStart) == 0x04, "i386 GL-state fog-start extent changed");
_Static_assert(offsetof(glstate_t, fogEnd) == 0xb4, "i386 GL-state fog end moved");
_Static_assert(sizeof(((glstate_t *)0)->fogEnd) == 0x04, "i386 GL-state fog-end extent changed");
_Static_assert(offsetof(glstate_t, fogDensity) == 0xb8, "i386 GL-state fog density moved");
_Static_assert(sizeof(((glstate_t *)0)->fogDensity) == 0x04, "i386 GL-state fog-density extent changed");
_Static_assert(offsetof(glstate_t, currentStorageMode) == 0xbc, "i386 GL-state current storage mode moved");
_Static_assert(sizeof(((glstate_t *)0)->currentStorageMode) == 0x04, "i386 GL-state current storage-mode extent changed");
_Static_assert(offsetof(glstate_t, enabledLightCount) == 0xc0, "i386 GL-state enabled-light count moved");
_Static_assert(sizeof(((glstate_t *)0)->enabledLightCount) == 0x04, "i386 GL-state enabled-light count extent changed");
_Static_assert(offsetof(glstate_t, currentLightingEntity) == 0xc4, "i386 GL-state current lighting entity moved");
_Static_assert(sizeof(((glstate_t *)0)->currentLightingEntity) == 0x04, "i386 GL-state current lighting-entity extent changed");
_Static_assert(offsetof(glstate_t, currentLightingFlags) == 0xc8, "i386 GL-state current lighting flags moved");
_Static_assert(sizeof(((glstate_t *)0)->currentLightingFlags) == 0x04, "i386 GL-state current lighting-flags extent changed");
_Static_assert(offsetof(glstate_t, normalizeTarget) == 0xcc, "i386 GL-state normalization target moved");
_Static_assert(sizeof(((glstate_t *)0)->normalizeTarget) == 0x04, "i386 GL-state normalization-target extent changed");
_Static_assert(sizeof(glstate_t) == 0xd0, "original i386 GL-state size changed");
_Static_assert(_Alignof(glconfig_t) == 0x04, "i386 renderer GL-config alignment changed");
_Static_assert(offsetof(glconfig_t, rendererString) == 0x00, "i386 renderer GL renderer string moved");
_Static_assert(sizeof(((glconfig_t *)0)->rendererString) == 0x04, "i386 renderer GL renderer-string extent changed");
_Static_assert(offsetof(glconfig_t, vendorString) == 0x04, "i386 renderer GL vendor string moved");
_Static_assert(sizeof(((glconfig_t *)0)->vendorString) == 0x04, "i386 renderer GL vendor-string extent changed");
_Static_assert(offsetof(glconfig_t, versionString) == 0x08, "i386 renderer GL version string moved");
_Static_assert(sizeof(((glconfig_t *)0)->versionString) == 0x04, "i386 renderer GL version-string extent changed");
_Static_assert(offsetof(glconfig_t, extensionsString) == 0x0c, "i386 renderer GL extension string moved");
_Static_assert(sizeof(((glconfig_t *)0)->extensionsString) == 0x04, "i386 renderer GL extension-string extent changed");
_Static_assert(offsetof(glconfig_t, wglExtensionsString) == 0x10, "i386 renderer WGL extension string moved");
_Static_assert(sizeof(((glconfig_t *)0)->wglExtensionsString) == 0x04, "i386 renderer WGL extension-string extent changed");
_Static_assert(offsetof(glconfig_t, maxTextureSize) == 0x14, "i386 renderer maximum texture size moved");
_Static_assert(sizeof(((glconfig_t *)0)->maxTextureSize) == 0x04, "i386 renderer maximum texture-size extent changed");
_Static_assert(offsetof(glconfig_t, maxActiveTextures) == 0x18, "i386 renderer maximum active-texture count moved");
_Static_assert(sizeof(((glconfig_t *)0)->maxActiveTextures) == 0x04, "i386 renderer maximum active-texture extent changed");
_Static_assert(offsetof(glconfig_t, maxLights) == 0x1c, "i386 renderer maximum light count moved");
_Static_assert(sizeof(((glconfig_t *)0)->maxLights) == 0x04, "i386 renderer maximum light-count extent changed");
_Static_assert(offsetof(glconfig_t, colorBits) == 0x20, "i386 renderer framebuffer color bits moved");
_Static_assert(sizeof(((glconfig_t *)0)->colorBits) == 0x04, "i386 renderer framebuffer color-bit extent changed");
_Static_assert(offsetof(glconfig_t, depthBits) == 0x24, "i386 renderer framebuffer depth bits moved");
_Static_assert(sizeof(((glconfig_t *)0)->depthBits) == 0x04, "i386 renderer framebuffer depth-bit extent changed");
_Static_assert(offsetof(glconfig_t, stencilBits) == 0x28, "i386 renderer framebuffer stencil bits moved");
_Static_assert(sizeof(((glconfig_t *)0)->stencilBits) == 0x04, "i386 renderer framebuffer stencil-bit extent changed");
_Static_assert(offsetof(glconfig_t, deviceSupportsGamma) == 0x2c, "i386 renderer gamma capability moved");
_Static_assert(sizeof(((glconfig_t *)0)->deviceSupportsGamma) == 0x04, "i386 renderer gamma-capability extent changed");
_Static_assert(offsetof(glconfig_t, textureFilterAnisotropicAvailable) == 0x30, "i386 renderer anisotropic-filter capability moved");
_Static_assert(sizeof(((glconfig_t *)0)->textureFilterAnisotropicAvailable) == 0x04,
               "i386 renderer anisotropic-filter capability extent changed");
_Static_assert(offsetof(glconfig_t, maxTextureFilterAnisotropy) == 0x34, "i386 renderer maximum anisotropy moved");
_Static_assert(sizeof(((glconfig_t *)0)->maxTextureFilterAnisotropy) == 0x04, "i386 renderer maximum anisotropy extent changed");
_Static_assert(offsetof(glconfig_t, textureEnvAddAvailable) == 0x38, "i386 renderer texture-add capability moved");
_Static_assert(sizeof(((glconfig_t *)0)->textureEnvAddAvailable) == 0x04, "i386 renderer texture-add capability extent changed");
_Static_assert(offsetof(glconfig_t, cubeMapAvailable) == 0x3c, "i386 renderer cube-map capability moved");
_Static_assert(sizeof(((glconfig_t *)0)->cubeMapAvailable) == 0x04, "i386 renderer cube-map capability extent changed");
_Static_assert(offsetof(glconfig_t, textureEnvCombineAvailable) == 0x40, "i386 renderer texture-combine capability moved");
_Static_assert(sizeof(((glconfig_t *)0)->textureEnvCombineAvailable) == 0x04, "i386 renderer texture-combine capability extent changed");
_Static_assert(offsetof(glconfig_t, textureEnvDot3Available) == 0x44, "i386 renderer texture-DOT3 capability moved");
_Static_assert(sizeof(((glconfig_t *)0)->textureEnvDot3Available) == 0x04, "i386 renderer texture-DOT3 capability extent changed");
_Static_assert(offsetof(glconfig_t, vertexBufferObjectAvailable) == 0x48, "i386 renderer vertex-buffer capability moved");
_Static_assert(sizeof(((glconfig_t *)0)->vertexBufferObjectAvailable) == 0x04, "i386 renderer vertex-buffer capability extent changed");
_Static_assert(offsetof(glconfig_t, vertexProgramAvailable) == 0x4c, "i386 renderer vertex-program capability moved");
_Static_assert(sizeof(((glconfig_t *)0)->vertexProgramAvailable) == 0x04, "i386 renderer vertex-program capability extent changed");
_Static_assert(offsetof(glconfig_t, rescaleNormalAvailable) == 0x50, "i386 renderer rescale-normal capability moved");
_Static_assert(sizeof(((glconfig_t *)0)->rescaleNormalAvailable) == 0x04, "i386 renderer rescale-normal capability extent changed");
_Static_assert(offsetof(glconfig_t, fogDistanceAvailable) == 0x54, "i386 renderer fog-distance capability moved");
_Static_assert(sizeof(((glconfig_t *)0)->fogDistanceAvailable) == 0x04, "i386 renderer fog-distance capability extent changed");
_Static_assert(offsetof(glconfig_t, NVFogMode) == 0x58, "i386 renderer NV fog mode moved");
_Static_assert(sizeof(((glconfig_t *)0)->NVFogMode) == 0x04, "i386 renderer NV fog-mode extent changed");
_Static_assert(offsetof(glconfig_t, vertexArrayRangeMode) == 0x5c, "i386 renderer vertex-array-range mode moved");
_Static_assert(sizeof(((glconfig_t *)0)->vertexArrayRangeMode) == 0x04, "i386 renderer vertex-array-range mode extent changed");
_Static_assert(offsetof(glconfig_t, fenceNVAvailable) == 0x60, "i386 renderer NV fence capability moved");
_Static_assert(sizeof(((glconfig_t *)0)->fenceNVAvailable) == 0x04, "i386 renderer NV fence capability extent changed");
_Static_assert(offsetof(glconfig_t, registerCombinerMode) == 0x64, "i386 renderer register-combiner mode moved");
_Static_assert(sizeof(((glconfig_t *)0)->registerCombinerMode) == 0x04, "i386 renderer register-combiner mode extent changed");
_Static_assert(offsetof(glconfig_t, textureShaderNVAvailable) == 0x68, "i386 renderer NV texture-shader capability moved");
_Static_assert(sizeof(((glconfig_t *)0)->textureShaderNVAvailable) == 0x04, "i386 renderer NV texture-shader capability extent changed");
_Static_assert(offsetof(glconfig_t, maxPNTrianglesTessellationLevel) == 0x6c, "i386 renderer PN-triangle tessellation limit moved");
_Static_assert(sizeof(((glconfig_t *)0)->maxPNTrianglesTessellationLevel) == 0x04, "i386 renderer PN-triangle tessellation extent changed");
_Static_assert(offsetof(glconfig_t, pnTrianglesNormalMode) == 0x70, "i386 renderer PN-triangle normal mode moved");
_Static_assert(sizeof(((glconfig_t *)0)->pnTrianglesNormalMode) == 0x04, "i386 renderer PN-triangle normal-mode extent changed");
_Static_assert(offsetof(glconfig_t, pnTrianglesPointMode) == 0x74, "i386 renderer PN-triangle point mode moved");
_Static_assert(sizeof(((glconfig_t *)0)->pnTrianglesPointMode) == 0x04, "i386 renderer PN-triangle point-mode extent changed");
_Static_assert(offsetof(glconfig_t, vertexArrayObjectATIAvailable) == 0x78, "i386 renderer ATI vertex-array-object capability moved");
_Static_assert(sizeof(((glconfig_t *)0)->vertexArrayObjectATIAvailable) == 0x04, "i386 renderer ATI vertex-array-object extent changed");
_Static_assert(offsetof(glconfig_t, elementArrayATIAvailable) == 0x7c, "i386 renderer ATI element-array capability moved");
_Static_assert(sizeof(((glconfig_t *)0)->elementArrayATIAvailable) == 0x04, "i386 renderer ATI element-array capability extent changed");
_Static_assert(offsetof(glconfig_t, fragmentShaderATIAvailable) == 0x80, "i386 renderer ATI fragment-shader capability moved");
_Static_assert(sizeof(((glconfig_t *)0)->fragmentShaderATIAvailable) == 0x04,
               "i386 renderer ATI fragment-shader capability extent changed");
_Static_assert(offsetof(glconfig_t, vidWidth) == 0x84, "i386 renderer video width moved");
_Static_assert(sizeof(((glconfig_t *)0)->vidWidth) == 0x04, "i386 renderer video-width extent changed");
_Static_assert(offsetof(glconfig_t, vidHeight) == 0x88, "i386 renderer video height moved");
_Static_assert(sizeof(((glconfig_t *)0)->vidHeight) == 0x04, "i386 renderer video-height extent changed");
_Static_assert(offsetof(glconfig_t, windowAspect) == 0x8c, "i386 renderer window aspect moved");
_Static_assert(sizeof(((glconfig_t *)0)->windowAspect) == 0x04, "i386 renderer window-aspect extent changed");
_Static_assert(offsetof(glconfig_t, displayFrequency) == 0x90, "i386 renderer display frequency moved");
_Static_assert(sizeof(((glconfig_t *)0)->displayFrequency) == 0x04, "i386 renderer display-frequency extent changed");
_Static_assert(offsetof(glconfig_t, isFullscreen) == 0x94, "i386 renderer fullscreen flag moved");
_Static_assert(sizeof(((glconfig_t *)0)->isFullscreen) == 0x04, "i386 renderer fullscreen-flag extent changed");
_Static_assert(offsetof(glconfig_t, stereoEnabled) == 0x98, "i386 renderer stereo flag moved");
_Static_assert(sizeof(((glconfig_t *)0)->stereoEnabled) == 0x04, "i386 renderer stereo-flag extent changed");
_Static_assert(offsetof(glconfig_t, smpActive) == 0x9c, "i386 renderer dormant SMP-active slot moved");
_Static_assert(sizeof(((glconfig_t *)0)->smpActive) == 0x04, "i386 renderer dormant SMP-active extent changed");
_Static_assert(sizeof(glconfig_t) == 0xa0, "original i386 renderer GL-config size changed");
#endif

#ifdef __cplusplus
extern "C" {
#endif

void GL_Bind(image_t *image);
void GL_SetTextureTarget(uint32_t target);
void GL_SelectTexture(int32_t textureUnit);
void GL_BindMultitexture(image_t *image0, image_t *image1);
void GL_Cull(cullType_t cullType);
void GL_TexEnv(int32_t environment);
void GL_Normalize(uint32_t target);
void GL_State(uint32_t stateBits);
void GL_ClientState(uint32_t stateBits);
void GL_DrawElements(uint32_t mode, int32_t count, uint32_t type, const void *indices);
void GL_DrawRangeElements(uint32_t mode, uint32_t start, uint32_t end, int32_t count, uint32_t type, const void *indices);
void GL_DrawElementArrayATI(uint32_t mode, int32_t count);

void RB_DisableTMU(int32_t textureUnit);
void RB_EndMultitexture(void);
void R_SyncRenderThread(void);
void RB_BeginImmediateMode(void);
void RB_EndImmediateMode(void);
void RB_glBegin(uint32_t mode);
void RB_glEnd(void);
void RB_glLineWidth(float width);
void RB_glColor4f(float red, float green, float blue, float alpha);
void RB_glColor4fv(const vec4_t color);
void RB_glColor3f(float red, float green, float blue);
void RB_glColor3fv(const vec3_t color);
void RB_glTexCoord2f(float s, float t);
void RB_glTexCoord2fv(const vec2_t texCoord);
void RB_glVertex3f(float x, float y, float z);
void RB_glVertex3fv(const vec3_t vertex);
void RB_glVertex2f(float x, float y);
void RB_glVertex2fv(const vec2_t vertex);
void RB_glVertex2i(int32_t x, int32_t y);

#ifdef __cplusplus
}
#endif

#endif
