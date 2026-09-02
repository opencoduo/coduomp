#ifndef CODUOMP_RENDERER_ENTITY_H
#define CODUOMP_RENDERER_ENTITY_H

#include <stddef.h>
#include <stdint.h>

#include "../effects/fx_render_types.h"
#include "../q_shared.h"

#ifdef __cplusplus
#define RENDERER_ENTITY_ALIGNOF(type_) alignof(type_)
#else
#define RENDERER_ENTITY_ALIGNOF(type_) _Alignof(type_)
#endif

enum {
    R_MAX_ENTITY_LIGHTS = 8,
    R_ENTITY_GENERATED_LIGHTS = 3
};

typedef enum renderer_light_type_e {
    R_LIGHT_TYPE_SUN = 1,
    R_LIGHT_TYPE_POINT = 2,
    R_LIGHT_TYPE_LINEAR_POINT = 3,
    R_LIGHT_TYPE_CUSTOM_POINT = 4,
    R_LIGHT_TYPE_SPOT = 5,
    /* Type 6 carries only the common color terms; R_LoadLights installs no
     * position or attenuation payload for it. Exact source name unproved. */
    R_LIGHT_TYPE_COLOR_ONLY = 6,
    R_LIGHT_TYPE_CUSTOM_SPOT = 7,
    R_LIGHT_TYPE_DIFFUSE_SUN = 8
} renderer_light_type_t;

typedef enum cull_result_e {
    CULL_IN = 0,
    CULL_CLIP = 1,
    CULL_OUT = 2
} cull_result_t;

/* Shared static/dynamic renderer light. Windows R_LoadLights allocates and
 * advances exactly 0x88 bytes per entry; RE_AddLightToScene uses the same
 * stride, and R_TransformDlights writes the three local-space coordinates at
 * +0x78..+0x80. PowerPC CoD 1.5 and UO 1.51 independently use the identical
 * naturally aligned layout.
 *
 * The final word is a retained RTCW renderer-lineage `int overdraw` member:
 * that lineage has the exact radius / transformed[3] / overdraw suffix found
 * here. No CoDUOMP light producer or consumer has a field-specific access to
 * +0x84, so it is dormant in this game, but it is a real source-layout member
 * rather than compiler tail padding. This type contains no pointers, and the
 * assertions below require the shipped layout on both 32- and 64-bit builds. */
typedef struct renderer_light_s {
    renderer_light_type_t type;            /* original +0x00 */
    vec3_t color;                          /* original +0x04 */
    float intensity;                       /* original +0x10 */
    vec4_t ambient;                        /* original +0x14 */
    vec4_t diffuse;                        /* original +0x24 */
    vec4_t specular;                       /* original +0x34 */
    vec4_t position;                       /* original +0x44 */
    vec3_t spotDirection;                  /* original +0x54 */
    float constantAttenuation;             /* original +0x60 */
    float linearAttenuation;               /* original +0x64 */
    float quadraticAttenuation;            /* original +0x68 */
    float spotExponent;                    /* original +0x6c */
    float spotCutoff;                      /* original +0x70 */
    float radius;                          /* original +0x74 */
    vec3_t transformedPosition;            /* original +0x78 */
    int32_t overdraw;                      /* original +0x84; unused in
                                              * CoDUOMP */
} renderer_light_t;

_Static_assert(sizeof(renderer_light_type_t) == 0x04,
               "renderer light-type enum width changed");
_Static_assert(RENDERER_ENTITY_ALIGNOF(renderer_light_type_t) == 0x04,
               "renderer light-type enum alignment changed");
_Static_assert(offsetof(renderer_light_t, type) == 0x00,
               "renderer light type moved");
_Static_assert(offsetof(renderer_light_t, color) == 0x04,
               "renderer light normalized color moved");
_Static_assert(offsetof(renderer_light_t, intensity) == 0x10,
               "renderer light intensity moved");
_Static_assert(offsetof(renderer_light_t, ambient) == 0x14,
               "renderer light ambient color moved");
_Static_assert(offsetof(renderer_light_t, diffuse) == 0x24,
               "renderer light diffuse color moved");
_Static_assert(offsetof(renderer_light_t, specular) == 0x34,
               "renderer light specular color moved");
_Static_assert(offsetof(renderer_light_t, position) == 0x44,
               "renderer light homogeneous position moved");
_Static_assert(offsetof(renderer_light_t, spotDirection) == 0x54,
               "renderer light spot direction moved");
_Static_assert(offsetof(renderer_light_t, constantAttenuation) == 0x60,
               "renderer light constant attenuation moved");
_Static_assert(offsetof(renderer_light_t, linearAttenuation) == 0x64,
               "renderer light linear attenuation moved");
_Static_assert(offsetof(renderer_light_t, quadraticAttenuation) == 0x68,
               "renderer light quadratic attenuation moved");
_Static_assert(offsetof(renderer_light_t, spotExponent) == 0x6c,
               "renderer light spot exponent moved");
_Static_assert(offsetof(renderer_light_t, spotCutoff) == 0x70,
               "renderer light spot cutoff moved");
_Static_assert(offsetof(renderer_light_t, radius) == 0x74,
               "renderer light radius moved");
_Static_assert(offsetof(renderer_light_t, transformedPosition) == 0x78,
               "renderer light transformed position moved");
_Static_assert(offsetof(renderer_light_t, overdraw) == 0x84,
               "renderer light dormant overdraw slot moved");
_Static_assert(RENDERER_ENTITY_ALIGNOF(renderer_light_t) == 0x04,
               "renderer light alignment changed");
_Static_assert(sizeof(renderer_light_t) == 0x88,
               "renderer light size changed");

typedef struct renderer_entity_light_s {
    renderer_light_t *light;
    float scale;
} renderer_entity_light_t;

typedef struct renderer_static_model_s renderer_static_model_t;

/* Renderer-private scene entity. R_SetSceneRefEntity advances the Windows
 * scene array by exactly 0x2b8 bytes, copies the 0x9c-byte public entity
 * prefix, and then initializes the renderer-owned suffix. PowerPC CoD 1.5 and
 * UO 1.51 independently use the same 32-bit stride and field offsets.
 *
 * The two bytes at original +0x0a2..+0x0a3 are ordinary compiler padding
 * between the byte-sized state flags and lightDir; no CoDUOMP field
 * access addresses either byte, and both PowerPC builds likewise advance
 * directly from +0x0a1 to the naturally aligned float at +0x0a4. The explicit
 * array preserves that original object-representation gap without inventing
 * a semantic member. The guards below cover only the assumed-correct original
 * 32-bit ABI; native pointer widening is not an original-layout contract.
 *
 * Quake III names this renderer-private continuation trRefEntity_t. CoD
 * replaces Q3's axisLength/needDlights prefix with the dlight bitmask and
 * byte-sized dynamic-light marker below, but retains the inherited public
 * entity, lighting direction, ambient-light, and directed-light roles. */
typedef struct trRefEntity_s {
    refEntity_t e;                        /* original +0x000 */
    uint32_t dlightBits;                   /* original +0x09c */
    uint8_t hasDynamicLights;              /* original +0x0a0 */
    uint8_t lightingCalculated;            /* original +0x0a1 */
    uint8_t padding0a2[2];                 /* original ABI padding; no
                                              * semantic CoDUOMP access */
    vec3_t lightDir;                       /* original +0x0a4 */
    vec3_t ambientLight;                   /* original +0x0b0 */
    uint32_t ambientLightInt;              /* original +0x0bc */
    vec3_t directedLight;                  /* original +0x0c0 */
    int32_t lightCount;                    /* original +0x0cc */
    renderer_entity_light_t lights[R_MAX_ENTITY_LIGHTS]; /* +0x0d0 */
    float diffuseSunContribution;          /* original +0x110; residual after
                                              * generated diffuse-sun lights */
    renderer_light_t generatedLights[R_ENTITY_GENERATED_LIGHTS]; /* +0x114 */
    uint32_t normalizationTarget;           /* original +0x2ac */
    renderer_static_model_t *staticModelLighting; /* +0x2b0 */
    cull_result_t cullState;                /* original +0x2b4 */
} trRefEntity_t;

_Static_assert(sizeof(cull_result_t) == 0x04,
               "renderer cull-result enum width changed");
_Static_assert(RENDERER_ENTITY_ALIGNOF(cull_result_t) == 0x04,
               "renderer cull-result enum alignment changed");

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(renderer_entity_light_t, light) == 0x00,
               "i386 entity-light pointer moved");
_Static_assert(offsetof(renderer_entity_light_t, scale) == 0x04,
               "i386 entity-light scale moved");
_Static_assert(RENDERER_ENTITY_ALIGNOF(renderer_entity_light_t) == 0x04,
               "i386 entity-light alignment changed");
_Static_assert(sizeof(renderer_entity_light_t) == 0x08,
               "i386 entity-light size changed");
_Static_assert(offsetof(trRefEntity_t, e) == 0x000,
               "i386 renderer-entity public prefix moved");
_Static_assert(offsetof(trRefEntity_t, dlightBits) == 0x09c,
               "i386 renderer-entity dlight-bit offset changed");
_Static_assert(offsetof(trRefEntity_t, hasDynamicLights) == 0x0a0,
               "i386 renderer-entity dynamic-light marker offset changed");
_Static_assert(offsetof(trRefEntity_t, lightingCalculated) == 0x0a1,
               "i386 renderer-entity lighting marker offset changed");
_Static_assert(offsetof(trRefEntity_t, padding0a2) == 0x0a2,
               "i386 renderer-entity flag padding moved");
_Static_assert(offsetof(trRefEntity_t, lightDir) == 0x0a4,
               "i386 renderer-entity light-direction offset changed");
_Static_assert(offsetof(trRefEntity_t, ambientLight) == 0x0b0,
               "i386 renderer-entity ambient-light offset changed");
_Static_assert(offsetof(trRefEntity_t, ambientLightInt) == 0x0bc,
               "i386 renderer-entity packed-ambient offset changed");
_Static_assert(offsetof(trRefEntity_t, directedLight) == 0x0c0,
               "i386 renderer-entity directed-light offset changed");
_Static_assert(offsetof(trRefEntity_t, lightCount) == 0x0cc,
               "i386 renderer-entity light-count offset changed");
_Static_assert(offsetof(trRefEntity_t, lights) == 0x0d0,
               "i386 renderer-entity light-array offset changed");
_Static_assert(offsetof(trRefEntity_t, diffuseSunContribution) == 0x110,
               "i386 renderer-entity diffuse-sun offset changed");
_Static_assert(offsetof(trRefEntity_t, generatedLights) == 0x114,
               "i386 renderer-entity generated-light offset changed");
_Static_assert(offsetof(trRefEntity_t, normalizationTarget) == 0x2ac,
               "i386 renderer-entity normalization-target offset changed");
_Static_assert(offsetof(trRefEntity_t, staticModelLighting) == 0x2b0,
               "i386 renderer-entity static-lighting offset changed");
_Static_assert(offsetof(trRefEntity_t, cullState) == 0x2b4,
               "i386 renderer-entity cull-state offset changed");
_Static_assert(RENDERER_ENTITY_ALIGNOF(trRefEntity_t) == 0x04,
               "i386 renderer-entity alignment changed");
_Static_assert(sizeof(trRefEntity_t) == 0x2b8,
               "i386 renderer-entity size changed");
#endif

#undef RENDERER_ENTITY_ALIGNOF

#endif
