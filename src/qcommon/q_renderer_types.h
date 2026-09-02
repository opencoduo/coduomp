#ifndef QCOMMON_Q_RENDERER_TYPES_H
#define QCOMMON_Q_RENDERER_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "asset_type_names.h"
#include "q_shared_types.h"
#include "q_vector_types.h"

struct renderer_registered_static_model_s;

/*
 * Public render-entity discriminator shared by the renderer and client
 * modules.  The Windows renderer's surface dispatch fixes the complete
 * 0..15 domain; cgame independently constructs model (1), sprite (4), splash
 * (5), beam (6), rail-core (7), and portal-surface (11) records with the same
 * values.  Quake renderer spellings are retained where the CoD domain agrees.
 * Value 8 has no accepted renderer path and no recovered original name.
 */
typedef enum refEntityType_e {
    RT_BRUSH_MODEL = 0,
    RT_MODEL = 1,
    RT_STATIC_MODEL = 2,
    RT_INVALID = 3,
    RT_SPRITE = 4,
    RT_SPLASH = 5,
    RT_BEAM = 6,
    RT_RAIL_CORE = 7,
    RT_RAIL_RINGS = 9,
    RT_LIGHTNING = 10,
    RT_PORTALSURFACE = 11,
    RT_ORIENTED_QUAD = 12,
    RT_LINE = 13,
    RT_ELECTRICITY = 14,
    RT_CYLINDER = 15,
    RT_MAX_REF_ENTITY_TYPE = 16
} refEntityType_t;

typedef char q_renderer_ref_entity_type_abi_size[sizeof(refEntityType_t) == 4 ? 1 : -1];

/* Canonical refEntity_t.renderfx bits.  Cgame's submitted records and the
 * Windows renderer consumers agree on every value below. */
enum refEntityRenderFlags_e {
    RF_THIRD_PERSON = 0x0002,
    RF_FIRST_PERSON = 0x0004,
    RF_DEPTHHACK = 0x0008,
    RF_CROSSHAIR = 0x0010,
    RF_NOSHADOW = 0x0040,
    RF_LIGHTING_ORIGIN = 0x0080,
    RF_DOBJ_MODEL = 0x2000,
    RF_DEPTH_RANGE_FLAGS = RF_DEPTHHACK | RF_CROSSHAIR
};

/* Public refdef_t.rdflags domain shared by the client modules and renderer.
 * The Windows cgame writers and renderer consumers agree on the bit values;
 * UI independently uses RDF_NOWORLDMODEL for model previews. */
enum renderer_refdef_flags_e {
    RDF_NOWORLDMODEL = 0x01,
    RDF_HYPERSPACE = 0x04,
    RDF_SKYBOX_PORTAL = 0x08,
    /* RE_RenderScene copies this bit into the persistent portal-active
     * marker consumed by RB_BeginDrawingView. Exact source spelling is not
     * present in the Windows image. */
    RDF_SKYBOX_PORTAL_ACTIVE = 0x10,
    RDF_DRAW_SKYBOX = 0x20,
    /* RB_SetIteratorFog selects the dedicated skybox fog while this backend
     * render-state bit is set. Exact original enum spelling is unavailable. */
    RDF_DRAWING_SKYBOX = 0x40
};

/* Renderer image-accounting/usage domain carried through RegisterShader,
 * RegisterShaderNoMip, model loading, and image creation.  Client cgame passes
 * HUD/effect/UI values 5/4/2 and UI passes 2; the Windows renderer stores the
 * same discriminator in every resulting image and reports all eleven rows. */
typedef enum renderer_image_track_e {
    R_IMAGE_TRACK_MISC = 0,
    R_IMAGE_TRACK_DEBUG = 1,
    R_IMAGE_TRACK_UI = 2,
    R_IMAGE_TRACK_LIGHTMAP = 3,
    R_IMAGE_TRACK_EFFECT = 4,
    R_IMAGE_TRACK_HUD = 5,
    R_IMAGE_TRACK_VIEWMODEL = 6,
    R_IMAGE_TRACK_MODEL = 7,
    R_IMAGE_TRACK_WORLD = 8,
    R_IMAGE_TRACK_FX = 9,
    R_IMAGE_TRACK_GENERATED_TEXTURE = 10,
    R_IMAGE_TRACK_COUNT = 11
} renderer_image_track_t;

typedef char q_renderer_image_track_abi_size[sizeof(renderer_image_track_t) == 4 ? 1 : -1];

/* Public polygon vertex passed between both client modules and the renderer.
 * CoDUOMP.exe's retained polygon paths and cgame CG_ImpactMark/CG_AddMarks
 * agree on the 0x20-byte stride and every lane below; the two floats at +0x14
 * are the renderer's world-surface lightmap coordinates. */
typedef struct polyVert_s {
    vec3_t xyz;
    float st[2];
    vec2_t lightmapCoords;
    uint8_t modulate[4];
} polyVert_t;

/* Renderer mark-fragment result returned across the cgame syscall boundary.
 * CoDUOMP.exe writes the shader handle and point span at +0/+4/+8, while
 * cgame CG_ImpactMark (0x3002e520) walks the same 0x0c-byte records. */
typedef struct markFragment_s {
    qhandle_t shaderHandle;
    int32_t firstPoint;
    int32_t numPoints;
} markFragment_t;

/* Renderer statistics record shared by the renderer and cgame through
 * CG_R_TRACK_STATISTICS. RE_TrackStatistics retains the caller's optional
 * pointer and R_PerformanceCounters refreshes the record once per issued frame.
 * The submitted/drawn geometry counters, image byte counts, entity count, and
 * overdraw ratio agree at every offset in CoDUOMP.exe and
 * uo_cgame_mp_x86.dll. */
typedef struct renderer_frame_statistics_s {
    int32_t indexCount;
    int32_t drawnIndexCount;
    int32_t vertexCount;
    int32_t drawCallCount;
    int32_t imageMemory;
    int32_t lightmapMemory;
    int32_t textureMemory;
    int32_t entityCount;
    float overdrawRatio;
} renderer_frame_statistics_t;

/* Windows renderer configuration copied across both client-module boundaries.
 * CoDUOMP.exe RE_BeginRegistration (0x00518060) publishes all 0xa0 i386 bytes;
 * the cgame and UI consumers agree with the renderer on every offset.  The
 * five strings are source pointers and widen consistently in native builds.
 * The supporting Mac client uses a larger platform carrier, so only the
 * Windows ABI has the original offsets asserted below. */
typedef struct glconfig_s {
    const char *rendererString;
    const char *vendorString;
    const char *versionString;
    const char *extensionsString;
    const char *wglExtensionsString;
    int32_t maxTextureSize;
    int32_t maxActiveTextures;
    int32_t maxLights;
    int32_t colorBits;
    int32_t depthBits;
    int32_t stencilBits;
    qboolean deviceSupportsGamma;
    qboolean textureFilterAnisotropicAvailable;
    float maxTextureFilterAnisotropy;
    qboolean textureEnvAddAvailable;
    qboolean cubeMapAvailable;
    qboolean textureEnvCombineAvailable;
    qboolean textureEnvDot3Available;
    qboolean vertexBufferObjectAvailable;
    qboolean vertexProgramAvailable;
    qboolean rescaleNormalAvailable;
    qboolean fogDistanceAvailable;
    int32_t NVFogMode;
    int32_t vertexArrayRangeMode;
    qboolean fenceNVAvailable;
    int32_t registerCombinerMode;
    qboolean textureShaderNVAvailable;
    int32_t maxPNTrianglesTessellationLevel;
    int32_t pnTrianglesNormalMode;
    int32_t pnTrianglesPointMode;
    qboolean vertexArrayObjectATIAvailable;
    qboolean elementArrayATIAvailable;
    qboolean fragmentShaderATIAvailable;
    int32_t vidWidth;
    int32_t vidHeight;
    float windowAspect;
    int32_t displayFrequency;
    qboolean isFullscreen;
    qboolean stereoEnabled;
    qboolean smpActive;
} glconfig_t;

/*
 * Public renderer entity shared by cgame, UI, and the engine.  The exact
 * refEntity_t alias is retained by the Mac AddFxToScene symbol.  On Windows,
 * cgame CG_Item (0x3001e751) and UI Item_Model_Paint (0x40018fcc) each clear
 * exactly 39 dwords before submitting the record, while CoDUOMP.exe
 * R_SetSceneRefEntity (0x004e607a) copies the same 39 dwords into renderer
 * storage.  Pointer members are native-width source fields; only the original
 * i386 build has the resulting 0x9c-byte layout.
 */
typedef struct refEntity_s {
    refEntityType_t reType;                 /* original +0x00 */
    int32_t renderfx;                       /* original +0x04 */
    qhandle_t hModel;                       /* original +0x08 */
    vec3_t lightingOrigin;                  /* original +0x0c */
    float shadowPlane;                      /* original +0x18 */
    axis_t axis;                            /* original +0x1c */
    /* R_RotateForModelEntity reads this lane with FLD and compares it with
     * 0.0f, so it is a float marker rather than Quake III's qboolean. */
    float nonNormalizedAxes;                /* original +0x40 */
    vec3_t origin;                          /* original +0x44 */
    int32_t frame;                          /* original +0x50 */
    vec3_t oldorigin;                       /* original +0x54 */
    int32_t oldframe;                       /* original +0x60 */
    union {
        float backlerp;                     /* original +0x64 */
        float radius2;                      /* CoD sprite/quad radius */
    };
    qhandle_t spriteShaderHandle;           /* original +0x68 */
    uint8_t shaderRGBA[4];                  /* original +0x6c */
    float shaderTexCoord[2];                /* original +0x70 */
    float shaderTime;                       /* original +0x78 */
    float radius;                           /* original +0x7c */
    union {
        qhandle_t customShader;             /* original +0x80 */
        /* Sprite-style effects reuse the renderer word as a float angle;
         * CParticle::UpdateRotation writes it as such. */
        float rotation;
        float electricityEndTime;
    };
    union {
        /* Non-electricity entities retain these copied renderer-extension
         * bytes without interpreting them. */
        uint8_t rendererPrivateState84[12]; /* original +0x84 */
        struct {
            /* FX_AddElectricity passes this renderer parameter as float. */
            float parameter;                /* original +0x84 */
            float lifeTime;                 /* original +0x88 */
            /* Copied and archived, but otherwise unused by CoDUOMP.exe. */
            uint32_t reserved8c;             /* original +0x8c */
        } electricity;
    };
    DObj *dobj;                              /* original +0x90 */
    union {
        /* Animated entities use the owner passed to CG_DObjCalcPose. Static
         * entities retain their renderer registration in the same word. */
        void *owner;                         /* original +0x94 */
        struct renderer_registered_static_model_s *staticModelRegistration;
    };
    /* Copied at the scene, FX, and archive boundaries, but otherwise unused
     * by CoDUOMP.exe. */
    uint8_t rendererPrivateState98[4];       /* original +0x98 */
} refEntity_t;

enum {
    GLYPHS_PER_FONT = 256
};

/* Inherited Quake glyphInfo_t/fontInfo_t identity with CoD:UO's changed
 * serialized lanes.  CoDUOMP.exe RE_RegisterFont (0x004e8f40) decodes every
 * field below, and the supporting Mac RE_RegisterFont body at code+0xfc1c0
 * corroborates the same 0x5048-byte record.  The Windows cgame and UI font
 * syscalls pass this complete record across the renderer boundary. */
typedef struct glyphInfo_s {
    int32_t height;                         /* +0x00 */
    int32_t width;                          /* +0x04 */
    float top;                              /* +0x08 */
    float left;                             /* +0x0c */
    float xSkip;                            /* +0x10 */
    int32_t imageWidth;                     /* +0x14 */
    int32_t imageHeight;                    /* +0x18 */
    float s;                                /* +0x1c */
    float t;                                /* +0x20 */
    float s2;                               /* +0x24 */
    float t2;                               /* +0x28 */
    qhandle_t glyph;                        /* +0x2c */
    char shaderName[32];                    /* +0x30 */
} glyphInfo_t;

typedef struct fontInfo_s {
    glyphInfo_t glyphs[GLYPHS_PER_FONT];    /* +0x0000 */
    float glyphScale;                       /* +0x5000 */
    float lineHeight;                       /* +0x5004 */
    char fontDataName[MAX_QPATH];           /* +0x5008 */
} fontInfo_t;

#if defined(__cplusplus)
#define Q_RENDERER_STATIC_ASSERT(expression, message) static_assert((expression), message)
#define Q_RENDERER_ALIGNOF(type) alignof(type)
#else
#define Q_RENDERER_STATIC_ASSERT(expression, message) _Static_assert((expression), message)
#define Q_RENDERER_ALIGNOF(type) _Alignof(type)
#endif

Q_RENDERER_STATIC_ASSERT(sizeof(glyphInfo_t) == 0x50, "glyphInfo_t extent changed");
Q_RENDERER_STATIC_ASSERT(offsetof(glyphInfo_t, glyph) == 0x2c, "glyphInfo_t.glyph moved");
Q_RENDERER_STATIC_ASSERT(offsetof(glyphInfo_t, shaderName) == 0x30, "glyphInfo_t.shaderName moved");
Q_RENDERER_STATIC_ASSERT(offsetof(fontInfo_t, glyphScale) == 0x5000, "fontInfo_t.glyphScale moved");
Q_RENDERER_STATIC_ASSERT(offsetof(fontInfo_t, lineHeight) == 0x5004, "fontInfo_t.lineHeight moved");
Q_RENDERER_STATIC_ASSERT(offsetof(fontInfo_t, fontDataName) == 0x5008, "fontInfo_t.fontDataName moved");
Q_RENDERER_STATIC_ASSERT(sizeof(fontInfo_t) == 0x5048, "fontInfo_t extent changed");

Q_RENDERER_STATIC_ASSERT(Q_RENDERER_ALIGNOF(renderer_frame_statistics_t) == 4, "renderer frame-statistics alignment changed");
Q_RENDERER_STATIC_ASSERT(offsetof(renderer_frame_statistics_t, indexCount) == 0x00, "renderer frame-statistics index count moved");
Q_RENDERER_STATIC_ASSERT(offsetof(renderer_frame_statistics_t, drawnIndexCount) == 0x04,
                         "renderer frame-statistics drawn index count moved");
Q_RENDERER_STATIC_ASSERT(offsetof(renderer_frame_statistics_t, vertexCount) == 0x08, "renderer frame-statistics vertex count moved");
Q_RENDERER_STATIC_ASSERT(offsetof(renderer_frame_statistics_t, drawCallCount) == 0x0c, "renderer frame-statistics draw-call count moved");
Q_RENDERER_STATIC_ASSERT(offsetof(renderer_frame_statistics_t, imageMemory) == 0x10, "renderer frame-statistics image memory moved");
Q_RENDERER_STATIC_ASSERT(offsetof(renderer_frame_statistics_t, lightmapMemory) == 0x14, "renderer frame-statistics lightmap memory moved");
Q_RENDERER_STATIC_ASSERT(offsetof(renderer_frame_statistics_t, textureMemory) == 0x18, "renderer frame-statistics texture memory moved");
Q_RENDERER_STATIC_ASSERT(offsetof(renderer_frame_statistics_t, entityCount) == 0x1c, "renderer frame-statistics entity count moved");
Q_RENDERER_STATIC_ASSERT(offsetof(renderer_frame_statistics_t, overdrawRatio) == 0x20, "renderer frame-statistics overdraw ratio moved");
Q_RENDERER_STATIC_ASSERT(sizeof(renderer_frame_statistics_t) == 0x24, "renderer frame-statistics extent changed");

Q_RENDERER_STATIC_ASSERT(offsetof(polyVert_t, xyz) == 0x00, "polyVert_t.xyz moved");
Q_RENDERER_STATIC_ASSERT(offsetof(polyVert_t, st) == 0x0c, "polyVert_t.st moved");
Q_RENDERER_STATIC_ASSERT(offsetof(polyVert_t, lightmapCoords) == 0x14, "polyVert_t.lightmapCoords moved");
Q_RENDERER_STATIC_ASSERT(offsetof(polyVert_t, modulate) == 0x1c, "polyVert_t.modulate moved");
Q_RENDERER_STATIC_ASSERT(sizeof(polyVert_t) == 0x20, "polyVert_t extent changed");
Q_RENDERER_STATIC_ASSERT(offsetof(markFragment_t, shaderHandle) == 0x00, "markFragment_t.shaderHandle moved");
Q_RENDERER_STATIC_ASSERT(offsetof(markFragment_t, firstPoint) == 0x04, "markFragment_t.firstPoint moved");
Q_RENDERER_STATIC_ASSERT(offsetof(markFragment_t, numPoints) == 0x08, "markFragment_t.numPoints moved");
Q_RENDERER_STATIC_ASSERT(sizeof(markFragment_t) == 0x0c, "markFragment_t extent changed");

#if UINTPTR_MAX == UINT32_MAX
Q_RENDERER_STATIC_ASSERT(offsetof(glconfig_t, rendererString) == 0x00, "glconfig_t.rendererString moved");
Q_RENDERER_STATIC_ASSERT(offsetof(glconfig_t, wglExtensionsString) == 0x10, "glconfig_t.wglExtensionsString moved");
Q_RENDERER_STATIC_ASSERT(offsetof(glconfig_t, colorBits) == 0x20, "glconfig_t.colorBits moved");
Q_RENDERER_STATIC_ASSERT(offsetof(glconfig_t, deviceSupportsGamma) == 0x2c, "glconfig_t.deviceSupportsGamma moved");
Q_RENDERER_STATIC_ASSERT(offsetof(glconfig_t, vertexArrayRangeMode) == 0x5c, "glconfig_t.vertexArrayRangeMode moved");
Q_RENDERER_STATIC_ASSERT(offsetof(glconfig_t, registerCombinerMode) == 0x64, "glconfig_t.registerCombinerMode moved");
Q_RENDERER_STATIC_ASSERT(offsetof(glconfig_t, vidWidth) == 0x84, "glconfig_t.vidWidth moved");
Q_RENDERER_STATIC_ASSERT(offsetof(glconfig_t, vidHeight) == 0x88, "glconfig_t.vidHeight moved");
Q_RENDERER_STATIC_ASSERT(offsetof(glconfig_t, windowAspect) == 0x8c, "glconfig_t.windowAspect moved");
Q_RENDERER_STATIC_ASSERT(offsetof(glconfig_t, smpActive) == 0x9c, "glconfig_t.smpActive moved");
Q_RENDERER_STATIC_ASSERT(sizeof(glconfig_t) == 0xa0, "glconfig_t extent changed");
#endif

Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, reType) == 0x00, "refEntity_t.reType moved");
Q_RENDERER_STATIC_ASSERT(sizeof(((refEntity_t *)0)->reType) == 0x04, "refEntity_t.reType extent changed");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, renderfx) == 0x04, "refEntity_t.renderfx moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, hModel) == 0x08, "refEntity_t.hModel moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, lightingOrigin) == 0x0c, "refEntity_t.lightingOrigin moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, shadowPlane) == 0x18, "refEntity_t.shadowPlane moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, axis) == 0x1c, "refEntity_t.axis moved");
Q_RENDERER_STATIC_ASSERT(sizeof(((refEntity_t *)0)->axis) == 0x24, "refEntity_t.axis extent changed");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, nonNormalizedAxes) == 0x40, "refEntity_t.nonNormalizedAxes moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, origin) == 0x44, "refEntity_t.origin moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, frame) == 0x50, "refEntity_t.frame moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, oldorigin) == 0x54, "refEntity_t.oldorigin moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, oldframe) == 0x60, "refEntity_t.oldframe moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, backlerp) == 0x64, "refEntity_t.backlerp moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, radius2) == 0x64, "refEntity_t.radius2 moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, spriteShaderHandle) == 0x68, "refEntity_t.spriteShaderHandle moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, shaderRGBA) == 0x6c, "refEntity_t.shaderRGBA moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, shaderTexCoord) == 0x70, "refEntity_t.shaderTexCoord moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, shaderTime) == 0x78, "refEntity_t.shaderTime moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, radius) == 0x7c, "refEntity_t.radius moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, customShader) == 0x80, "refEntity_t.customShader moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, rotation) == 0x80, "refEntity_t.rotation moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, electricityEndTime) == 0x80, "refEntity_t.electricityEndTime moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, rendererPrivateState84) == 0x84, "refEntity_t private state moved");
Q_RENDERER_STATIC_ASSERT(sizeof(((refEntity_t *)0)->rendererPrivateState84) == 0x0c, "refEntity_t private-state extent changed");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, electricity.parameter) == 0x84, "refEntity_t electricity parameter moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, electricity.lifeTime) == 0x88, "refEntity_t electricity lifetime moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, electricity.reserved8c) == 0x8c, "refEntity_t electricity reserved word moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, dobj) == 0x90, "refEntity_t.dobj moved");

#if UINTPTR_MAX == UINT32_MAX
Q_RENDERER_STATIC_ASSERT(Q_RENDERER_ALIGNOF(refEntity_t) == 0x04, "i386 refEntity_t alignment changed");
Q_RENDERER_STATIC_ASSERT(sizeof(((refEntity_t *)0)->dobj) == 0x04, "i386 refEntity_t DObj pointer widened");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, owner) == 0x94, "i386 refEntity_t.owner moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, staticModelRegistration) == 0x94, "i386 refEntity_t static registration moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, rendererPrivateState98) == 0x98, "i386 refEntity_t private tail moved");
Q_RENDERER_STATIC_ASSERT(sizeof(refEntity_t) == 0x9c, "i386 refEntity_t size changed");
#elif UINTPTR_MAX == UINT64_MAX
Q_RENDERER_STATIC_ASSERT(Q_RENDERER_ALIGNOF(refEntity_t) == 0x08, "64-bit refEntity_t alignment changed");
Q_RENDERER_STATIC_ASSERT(sizeof(((refEntity_t *)0)->dobj) == 0x08, "64-bit refEntity_t DObj pointer narrowed");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, owner) == 0x98, "64-bit refEntity_t.owner moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, staticModelRegistration) == 0x98, "64-bit refEntity_t static registration moved");
Q_RENDERER_STATIC_ASSERT(offsetof(refEntity_t, rendererPrivateState98) == 0xa0, "64-bit refEntity_t private tail moved");
Q_RENDERER_STATIC_ASSERT(sizeof(refEntity_t) == 0xa8, "64-bit refEntity_t size changed");
#endif

/* Public renderer scene definition shared by cgame, UI, and the engine.
 * Quake III supplies the inherited refdef_s/refdef_t identity and member
 * spellings, while the shipped CoD:UO clients prove the shorter CoD record:
 * CoDUOMP.exe RE_RenderScene (RVA 0x000e5360) copies all twenty dwords through
 * rdflags at +0x4c, and the cgame/UI Item_Model_Paint bodies (RVAs 0x00057240
 * and 0x00018da0) independently construct the same 0x50-byte boundary. */
typedef struct refdef_s {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    float fov_x;
    float fov_y;
    vec3_t vieworg;
    axis_t viewaxis;
    int32_t time;
    int32_t rdflags;
} refdef_t;

typedef char q_renderer_refdef_x_offset[offsetof(refdef_t, x) == 0x00 ? 1 : -1];
typedef char q_renderer_refdef_y_offset[offsetof(refdef_t, y) == 0x04 ? 1 : -1];
typedef char q_renderer_refdef_width_offset[offsetof(refdef_t, width) == 0x08 ? 1 : -1];
typedef char q_renderer_refdef_height_offset[offsetof(refdef_t, height) == 0x0c ? 1 : -1];
typedef char q_renderer_refdef_fov_x_offset[offsetof(refdef_t, fov_x) == 0x10 ? 1 : -1];
typedef char q_renderer_refdef_fov_y_offset[offsetof(refdef_t, fov_y) == 0x14 ? 1 : -1];
typedef char q_renderer_refdef_vieworg_offset[offsetof(refdef_t, vieworg) == 0x18 ? 1 : -1];
typedef char q_renderer_refdef_viewaxis_offset[offsetof(refdef_t, viewaxis) == 0x24 ? 1 : -1];
typedef char q_renderer_refdef_time_offset[offsetof(refdef_t, time) == 0x48 ? 1 : -1];
typedef char q_renderer_refdef_rdflags_offset[offsetof(refdef_t, rdflags) == 0x4c ? 1 : -1];
typedef char q_renderer_refdef_size[sizeof(refdef_t) == 0x50 ? 1 : -1];

Q_RENDERER_STATIC_ASSERT(Q_RENDERER_ALIGNOF(refdef_t) == 0x04, "refdef_t alignment changed");

#undef Q_RENDERER_ALIGNOF
#undef Q_RENDERER_STATIC_ASSERT

#endif
