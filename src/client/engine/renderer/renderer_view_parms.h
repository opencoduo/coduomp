#ifndef CODUOMP_RENDERER_VIEW_PARMS_H
#define CODUOMP_RENDERER_VIEW_PARMS_H

#include <stddef.h>
#include <stdint.h>

#include "../q_shared.h"
#include "renderer_fog.h"
#include "renderer_orientation.h"

enum {
    R_FRUSTUM_PLANE_COUNT = 4,
    R_PLANE_NON_AXIAL = 3,
    R_DPVS_SIDE_X_NEGATIVE_OFFSET = 0,
    R_DPVS_SIDE_X_POSITIVE_OFFSET = 12,
    R_DPVS_SIDE_Y_NEGATIVE_OFFSET = 4,
    R_DPVS_SIDE_Y_POSITIVE_OFFSET = 16,
    R_DPVS_SIDE_Z_NEGATIVE_OFFSET = 8,
    R_DPVS_SIDE_Z_POSITIVE_OFFSET = 20,
    R_DPVS_CAMERA_BEHIND_PLANE = 0,
    R_DPVS_CAMERA_IN_FRONT_OF_PLANE = 1
};

/* The 20-byte plane form consumed by R_CullPointAndRadius. The final four
 * bytes are the original plane classification fields, not compiler padding. */
typedef struct renderer_frustum_plane_s {
    vec3_t normal;                         /* original +0x00 */
    float distance;                       /* original +0x0c */
    uint8_t type;                         /* original +0x10 */
    uint8_t signBits;                     /* original +0x11 */
    uint8_t padding12[2];                 /* original +0x12..+0x13 */
} renderer_frustum_plane_t;

/* Portal plane consumed by the DPVS front end. R_SetPlaneSidesDPVS writes one
 * byte per normal component; each byte is an offset selecting the positive or
 * negative bound used by the visibility code. */
typedef struct renderer_dpvs_plane_s {
    vec3_t normal;                         /* original +0x00 */
    float distance;                       /* original +0x0c */
    uint8_t sideOffsets[3];                /* original +0x10 */
    uint8_t cameraSide;                    /* original +0x13 */
} renderer_dpvs_plane_t;

/* CoD's continuation of Quake III's private viewParms_t. Q3 calls the first
 * member `or`, which is a C++ keyword; this shared C/C++ header uses the
 * portable spelling `orientation`. CoD changes the portal-plane representation
 * and adds the LOD, depth-hack projection, and fog state.
 * Windows R_RenderView copies this exact byte count, while the CoD 1 and UO
 * PowerPC builds each copy 76 pairs of 32-bit words; RE_RenderScene also
 * clears 0x260 bytes on all three. */
typedef struct viewParms_s {
    orientationr_t orientation;            /* original +0x000 */
    orientationr_t world;                  /* original +0x07c */
    vec3_t pvsOrigin;                       /* original +0x0f8 */
    qboolean isPortal;                      /* original +0x104 */
    qboolean isMirror;                      /* original +0x108 */
    int32_t frameSceneNum;                  /* original +0x10c */
    int32_t frameCount;                     /* original +0x110 */
    renderer_dpvs_plane_t portalPlane;      /* original +0x114 */
    int32_t viewportX;                      /* original +0x128 */
    int32_t viewportY;                      /* original +0x12c */
    int32_t viewportWidth;                  /* original +0x130 */
    int32_t viewportHeight;                 /* original +0x134 */
    float fovX;                             /* original +0x138 */
    float fovY;                             /* original +0x13c */
    float lodBias;                          /* original +0x140 */
    float lodScale;                         /* original +0x144 */
    float projectionMatrix[16];             /* original +0x148 */
    float depthHackProjectionMatrix[16];    /* original +0x188 */
    renderer_frustum_plane_t
        frustum[R_FRUSTUM_PLANE_COUNT];     /* original +0x1c8 */
    float zFar;                             /* original +0x218 */
    /* RTCW lineage places dirty between zFar and glFog. CoDUOMP zeroes and
     * whole-record copies this lane but has no field-specific access to it. */
    int32_t dirty;                           /* original +0x21c */
    /* RB_StageIteratorSky reads registered/drawSky at view offsets
     * +0x250/+0x254. The remaining lanes are zeroed and copied with the view;
     * no other field-specific access to this embedded instance was found. */
    renderer_fog_t glFog;                    /* original +0x220 */
} viewParms_t;

#if defined(__cplusplus)
#define RENDERER_VIEW_PARMS_STATIC_ASSERT static_assert
#define RENDERER_VIEW_PARMS_ALIGNOF alignof
#else
#define RENDERER_VIEW_PARMS_STATIC_ASSERT _Static_assert
#define RENDERER_VIEW_PARMS_ALIGNOF _Alignof
#endif

#if UINTPTR_MAX == UINT32_MAX
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    RENDERER_VIEW_PARMS_ALIGNOF(renderer_frustum_plane_t) == 4,
    "renderer frustum-plane alignment changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(sizeof(renderer_frustum_plane_t) == 0x14,
                                  "renderer frustum-plane size changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(renderer_frustum_plane_t, normal) == 0x00,
    "renderer frustum-plane normal offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(renderer_frustum_plane_t, distance) == 0x0c,
    "renderer frustum-plane distance offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(renderer_frustum_plane_t, type) == 0x10,
    "renderer frustum-plane type offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(renderer_frustum_plane_t, signBits) == 0x11,
    "renderer frustum-plane sign-bits offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(renderer_frustum_plane_t, padding12) == 0x12,
    "renderer frustum-plane padding offset changed");

RENDERER_VIEW_PARMS_STATIC_ASSERT(
    RENDERER_VIEW_PARMS_ALIGNOF(renderer_dpvs_plane_t) == 4,
    "renderer DPVS-plane alignment changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(sizeof(renderer_dpvs_plane_t) == 0x14,
                                  "renderer DPVS-plane size changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(renderer_dpvs_plane_t, normal) == 0x00,
    "renderer DPVS-plane normal offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(renderer_dpvs_plane_t, distance) == 0x0c,
    "renderer DPVS-plane distance offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(renderer_dpvs_plane_t, sideOffsets) == 0x10,
    "renderer DPVS-plane side-offset table moved");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(renderer_dpvs_plane_t, cameraSide) == 0x13,
    "renderer DPVS-plane camera-side offset changed");

RENDERER_VIEW_PARMS_STATIC_ASSERT(
    RENDERER_VIEW_PARMS_ALIGNOF(orientationr_t) == 4,
    "renderer orientation alignment changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(sizeof(orientationr_t) == 0x7c,
                                  "renderer orientation size changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    RENDERER_VIEW_PARMS_ALIGNOF(viewParms_t) == 4,
    "renderer view-parms alignment changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(sizeof(viewParms_t) == 0x260,
                                  "renderer view-parms size changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, orientation) == 0x000,
    "renderer view orientation offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, world) == 0x07c,
    "renderer world orientation offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, pvsOrigin) == 0x0f8,
    "renderer PVS-origin offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, isPortal) == 0x104,
    "renderer portal marker offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, isMirror) == 0x108,
    "renderer mirror marker offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, frameSceneNum) == 0x10c,
    "renderer view frame-scene-number offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, frameCount) == 0x110,
    "renderer view frame-count offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, portalPlane) == 0x114,
    "renderer portal-plane offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, viewportX) == 0x128,
    "renderer view viewport-X offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, viewportY) == 0x12c,
    "renderer view viewport-Y offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, viewportWidth) == 0x130,
    "renderer view viewport-width offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, viewportHeight) == 0x134,
    "renderer view viewport-height offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, fovX) == 0x138,
    "renderer horizontal-FOV offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, fovY) == 0x13c,
    "renderer vertical-FOV offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, lodBias) == 0x140,
    "renderer LOD-bias offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, lodScale) == 0x144,
    "renderer LOD-scale offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, projectionMatrix) == 0x148,
    "renderer projection-matrix offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, depthHackProjectionMatrix) == 0x188,
    "renderer depth-hack projection offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, frustum) == 0x1c8,
    "renderer frustum offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, zFar) == 0x218,
    "renderer far-clip offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, dirty) == 0x21c,
    "renderer view dirty offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, glFog) == 0x220,
    "renderer view-local fog offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, glFog.registered) == 0x250,
    "renderer view-local fog registration offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, glFog.drawSky) == 0x254,
    "renderer view-local fog draw-sky offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, glFog.clearScreen) == 0x258,
    "renderer view-local fog clear-screen offset changed");
RENDERER_VIEW_PARMS_STATIC_ASSERT(
    offsetof(viewParms_t, glFog.dirty) == 0x25c,
    "renderer view-local fog dirty offset changed");
#endif

#undef RENDERER_VIEW_PARMS_ALIGNOF
#undef RENDERER_VIEW_PARMS_STATIC_ASSERT

#endif
