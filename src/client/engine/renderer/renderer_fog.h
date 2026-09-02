#ifndef CODUOMP_RENDERER_FOG_H
#define CODUOMP_RENDERER_FOG_H

#include <stddef.h>
#include <stdint.h>

#include "../q_shared.h"

enum renderer_fog_slot_e {
    R_FOG_SKYBOX_VIEW = 1,
    R_FOG_PORTAL_VIEW = 2,
    R_FOG_RESET_SLOT = 3,
    R_FOG_CONFIG_VIEW = 4,
    R_FOG_WORLD_VIEW = 5,
    R_FOG_TRANSITION_FROM = 6,
    R_FOG_TRANSITION_TO = 7,
    R_FOG_SWITCH_COMMAND = 8,
    R_FOG_SLOT_COUNT = 9
};

/* The complete 64-byte fog record used both by rendererFogs and by the
 * view-local glFog member. The field sequence and names descend from the
 * RTCW glfog_t layout; CoD 1/UO machine code independently proves the same
 * offsets, widths, and stride. */
typedef struct renderer_fog_s {
    int32_t mode;                          /* original +0x00 */
    int32_t hint;                          /* original +0x04 */
    int32_t startTime;                     /* original +0x08 */
    int32_t finishTime;                    /* original +0x0c */
    vec4_t color;                          /* original +0x10 */
    float start;                           /* original +0x20 */
    float end;                             /* original +0x24 */
    /* Retained lineage field; no field-specific CoDUOMP access was found. */
    qboolean useEndForClip;                /* original +0x28 */
    float density;                         /* original +0x2c */
    qboolean registered;                   /* original +0x30 */
    qboolean drawSky;                      /* original +0x34 */
    qboolean clearScreen;                  /* original +0x38 */
    int32_t dirty;                         /* original +0x3c */
} renderer_fog_t;

#if defined(__cplusplus)
#define RENDERER_FOG_STATIC_ASSERT static_assert
#define RENDERER_FOG_ALIGNOF alignof
#else
#define RENDERER_FOG_STATIC_ASSERT _Static_assert
#define RENDERER_FOG_ALIGNOF _Alignof
#endif

#if UINTPTR_MAX == UINT32_MAX
RENDERER_FOG_STATIC_ASSERT(RENDERER_FOG_ALIGNOF(renderer_fog_t) == 4,
                           "renderer fog alignment changed");
RENDERER_FOG_STATIC_ASSERT(sizeof(renderer_fog_t) == 0x40,
                           "renderer fog size changed");
RENDERER_FOG_STATIC_ASSERT(offsetof(renderer_fog_t, mode) == 0x00,
                           "renderer fog mode offset changed");
RENDERER_FOG_STATIC_ASSERT(offsetof(renderer_fog_t, hint) == 0x04,
                           "renderer fog hint offset changed");
RENDERER_FOG_STATIC_ASSERT(offsetof(renderer_fog_t, startTime) == 0x08,
                           "renderer fog start-time offset changed");
RENDERER_FOG_STATIC_ASSERT(offsetof(renderer_fog_t, finishTime) == 0x0c,
                           "renderer fog finish-time offset changed");
RENDERER_FOG_STATIC_ASSERT(offsetof(renderer_fog_t, color) == 0x10,
                           "renderer fog color offset changed");
RENDERER_FOG_STATIC_ASSERT(offsetof(renderer_fog_t, start) == 0x20,
                           "renderer fog start offset changed");
RENDERER_FOG_STATIC_ASSERT(offsetof(renderer_fog_t, end) == 0x24,
                           "renderer fog end offset changed");
RENDERER_FOG_STATIC_ASSERT(offsetof(renderer_fog_t, useEndForClip) == 0x28,
                           "renderer fog clip marker offset changed");
RENDERER_FOG_STATIC_ASSERT(offsetof(renderer_fog_t, density) == 0x2c,
                           "renderer fog density offset changed");
RENDERER_FOG_STATIC_ASSERT(offsetof(renderer_fog_t, registered) == 0x30,
                           "renderer fog registration offset changed");
RENDERER_FOG_STATIC_ASSERT(offsetof(renderer_fog_t, drawSky) == 0x34,
                           "renderer fog draw-sky offset changed");
RENDERER_FOG_STATIC_ASSERT(offsetof(renderer_fog_t, clearScreen) == 0x38,
                           "renderer fog clear-screen offset changed");
RENDERER_FOG_STATIC_ASSERT(offsetof(renderer_fog_t, dirty) == 0x3c,
                           "renderer fog dirty offset changed");
#endif

#undef RENDERER_FOG_ALIGNOF
#undef RENDERER_FOG_STATIC_ASSERT

#endif
