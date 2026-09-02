#ifndef CODUOMP_RENDERER_REFDEF_H
#define CODUOMP_RENDERER_REFDEF_H

#include <stdint.h>

#include "../q_shared.h"

struct trRefEntity_s;
struct renderer_light_s;
struct renderer_corona_s;
struct drawSurf_s;
struct srfPoly_s;
struct renderer_entity_surface_s;

/* Quake III trRefdef_t: the public refdef plus renderer-generated scene data.
 * The CoD layout drops Q3's area-mask lanes and adds split dynamic-light
 * counts, coronas, and entity surfaces. RB_DrawSurfs command copies and the
 * direct scene consumers prove the complete tail. Pointers are native-width. */
typedef struct trRefdef_s {
    int32_t x;                               /* original +0x000 */
    int32_t y;                               /* original +0x004 */
    int32_t width;                           /* original +0x008 */
    int32_t height;                          /* original +0x00c */
    float fov_x;                             /* original +0x010 */
    float fov_y;                             /* original +0x014 */
    vec3_t vieworg;                          /* original +0x018 */
    axis_t viewaxis;                         /* original +0x024 */
    int32_t time;                           /* original +0x048 */
    int32_t rdflags;                        /* original +0x04c */
    float floatTime;                        /* original +0x050 */
    /* The eight text-deform modes select one fixed 32-byte string slot.
     * CoDUOMP.exe copies and reads these slots but has no field-specific
     * producer that writes this embedded array. */
    char text[8][32];                       /* original +0x054 */
    int32_t num_entities;                   /* original +0x154 */
    struct trRefEntity_s *entities;         /* original +0x158 */
    /* CoD retains Q3's num_dlights for the world-light pass and adds a second
     * count for the entity-light pass. No original spelling is exposed for
     * the added distinction, so that lane retains its semantic name. */
    int32_t num_dlights;                    /* original +0x15c */
    int32_t entityDlightCount;              /* original +0x160 */
    struct renderer_light_s *dlights;       /* original +0x164 */
    int32_t coronaCount;                    /* original +0x168 */
    struct renderer_corona_s *coronas;      /* original +0x16c */
    int32_t numPolys;                       /* original +0x170 */
    struct srfPoly_s *polys; /* original +0x174 */
    int32_t numDrawSurfs;                   /* original +0x178 */
    struct drawSurf_s *drawSurfs;          /* original +0x17c */
    int32_t entitySurfaceCount;             /* original +0x180 */
    struct renderer_entity_surface_s *entitySurfaces; /* original +0x184 */
} trRefdef_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(trRefdef_t) == 4, "i386 renderer refdef alignment changed");
_Static_assert(offsetof(trRefdef_t, x) == 0x000, "i386 renderer refdef viewport-X offset changed");
_Static_assert(offsetof(trRefdef_t, y) == 0x004, "i386 renderer refdef viewport-Y offset changed");
_Static_assert(offsetof(trRefdef_t, width) == 0x008, "i386 renderer refdef viewport-width offset changed");
_Static_assert(offsetof(trRefdef_t, height) == 0x00c, "i386 renderer refdef viewport-height offset changed");
_Static_assert(offsetof(trRefdef_t, fov_x) == 0x010, "i386 renderer refdef horizontal-FOV offset changed");
_Static_assert(offsetof(trRefdef_t, fov_y) == 0x014, "i386 renderer refdef vertical-FOV offset changed");
_Static_assert(offsetof(trRefdef_t, vieworg) == 0x018, "i386 renderer refdef view-origin offset changed");
_Static_assert(sizeof(((trRefdef_t *)0)->vieworg) == 0x00c, "i386 renderer refdef view-origin extent changed");
_Static_assert(offsetof(trRefdef_t, viewaxis) == 0x024, "i386 renderer refdef view-axis offset changed");
_Static_assert(sizeof(((trRefdef_t *)0)->viewaxis) == 0x024, "i386 renderer refdef view-axis extent changed");
_Static_assert(offsetof(trRefdef_t, time) == 0x048, "i386 renderer refdef-time offset changed");
_Static_assert(offsetof(trRefdef_t, rdflags) == 0x04c, "i386 renderer refdef flags offset changed");
_Static_assert(offsetof(trRefdef_t, floatTime) == 0x050, "i386 renderer refdef floating-time offset changed");
_Static_assert(offsetof(trRefdef_t, text) == 0x054, "i386 renderer shader-text offset changed");
_Static_assert(sizeof(((trRefdef_t *)0)->text) == 0x100, "i386 renderer shader-text extent changed");
_Static_assert(offsetof(trRefdef_t, num_entities) == 0x154, "i386 renderer entity-count offset changed");
_Static_assert(offsetof(trRefdef_t, entities) == 0x158, "i386 renderer entity-array offset changed");
_Static_assert(offsetof(trRefdef_t, num_dlights) == 0x15c, "i386 renderer world-light-count offset changed");
_Static_assert(offsetof(trRefdef_t, entityDlightCount) == 0x160, "i386 renderer entity-light-count offset changed");
_Static_assert(offsetof(trRefdef_t, dlights) == 0x164, "i386 renderer light-array offset changed");
_Static_assert(offsetof(trRefdef_t, coronaCount) == 0x168, "i386 renderer corona-count offset changed");
_Static_assert(offsetof(trRefdef_t, coronas) == 0x16c, "i386 renderer corona-array offset changed");
_Static_assert(offsetof(trRefdef_t, numDrawSurfs) == 0x178, "i386 renderer draw-surface-count offset changed");
_Static_assert(offsetof(trRefdef_t, numPolys) == 0x170, "i386 renderer polygon-count offset changed");
_Static_assert(offsetof(trRefdef_t, polys) == 0x174, "i386 renderer polygon-array offset changed");
_Static_assert(offsetof(trRefdef_t, drawSurfs) == 0x17c, "i386 renderer draw-surface-array offset changed");
_Static_assert(offsetof(trRefdef_t, entitySurfaceCount) == 0x180, "i386 renderer entity-surface-count offset changed");
_Static_assert(offsetof(trRefdef_t, entitySurfaces) == 0x184, "i386 renderer entity-surface-array offset changed");
_Static_assert(sizeof(trRefdef_t) == 0x188, "i386 renderer refdef size changed");
#endif

#endif
