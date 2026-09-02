#include "backend.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_native_x87.h"
#include "math/q_math.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

/* Original read-only debug colors. Keeping them at file scope gives the
 * initializer audit stable symbols; MSVC merges equal vectors, as it does for
 * the two green entries at 0x0058fbb8. */
static const vec4_t visibleSurfaceDebugColor = {0.0f, 1.0f, 0.0f, 1.0f}; /* original 0x0058fbb8 */
static const vec4_t portalDebugColor = {0.0f, 1.0f, 1.0f, 0.25f}; /* original 0x00591380 */
static const vec4_t cullGroupDebugColor = {0.75f, 0.75f, 0.0f, 1.0f}; /* original 0x0058fbf8 */
static const vec4_t staticModelDebugColor = {0.75f, 0.75f, 0.75f, 1.0f}; /* original 0x0058fc78 */
static const vec4_t modelBoundsDebugColor = {0.0f, 1.0f, 1.0f, 1.0f}; /* original 0x0058fc28 */
static const vec4_t acceptedTreeDebugColor = {1.0f, 1.0f, 0.0f, 1.0f}; /* original 0x0058fbe8 */
static const vec4_t clippedLeafDebugColor = {1.0f, 0.69999998807907104f, 0.0f, 1.0f}; /* original 0x0058fca8 */
static const vec4_t culledEntityDebugColor = {1.0f, 0.0f, 0.0f, 1.0f}; /* original 0x0058fba8 */
static const vec4_t visibleEntityDebugColor = {0.0f, 1.0f, 0.0f, 1.0f}; /* original 0x0058fbb8 */
static const vec4_t acceptedBevelDebugColor = {0.0f, 0.75f, 0.75f, 1.0f}; /* original 0x0058fc38 */
static const vec4_t rejectedBevelDebugColor = {0.0f, 0.5f, 0.5f, 1.0f}; /* original 0x0058fc48 */

/* These are source-level fields of the renderer's DPVS working state. The
 * Windows linker placed them at 0x0388c8bc, 0x0388c8c0, 0x0388c950,
 * 0x0388c954, and 0x0388c960; the Mac R_CullBoxDPVS accesses the same fields
 * from one state-object base. They are kept as typed globals until the rest of
 * that internal state object is recovered. */
renderer_dpvs_plane_t *rendererDpvsActiveNearPlane;
renderer_dpvs_plane_t *rendererDpvsActiveFarPlane;
int32_t rendererDpvsOccluderCount;
renderer_occluder_t **rendererDpvsOccluders;
int32_t rendererDpvsActivePlaneCount;
renderer_dpvs_plane_t *rendererDpvsActivePlanes;
int32_t rendererDpvsCellEntityLinkCount;
renderer_cell_entity_link_t *rendererDpvsCellEntityLinks;
int32_t rendererDpvsCullPlaneLimit;
float rendererCullDistance;
renderer_dpvs_plane_t rendererDpvsFrustumPlanes[R_FRUSTUM_PLANE_COUNT];
vec3_t rendererDpvsViewOrigin;
renderer_dpvs_plane_t rendererDpvsNearPlane;
renderer_dpvs_plane_t rendererDpvsFarPlane;
float rendererDpvsWorldViewProjectionMatrix[4][4];
float rendererDpvsInverseWorldViewProjectionMatrix[4][4];
qboolean rendererDpvsInitialSetup = qtrue;

/* Source: CoDUOMP.exe 0x0050f000..0x0050f1bc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050f000_0050f1bd.mcode and the direct
 * call from R_SetupDPVS at 0x005121c3.
 * The Mac compiler folds this work into its named R_SetupDPVS body, so
 * R_SetupDPVSFrustumPlanes is a role-derived helper name. Each view-frustum
 * plane is copied into the DPVS form and receives the same exact 0x3a83126f
 * (nominally 0.001) distance bias and corner-selection bytes as every portal
 * and occluder plane. R_SetPlaneSidesDPVS applies that bias, so the caller
 * must copy the unmodified view-plane distance first. */
void R_SetupDPVSFrustumPlanes(void)
{
    for (int32_t planeIndex = 0; planeIndex < R_FRUSTUM_PLANE_COUNT; ++planeIndex) {
        renderer_dpvs_plane_t *dpvsPlane = &rendererDpvsFrustumPlanes[planeIndex];
        const renderer_frustum_plane_t *viewPlane = &tr.viewParms.frustum[planeIndex];

        dpvsPlane->normal[0] = viewPlane->normal[0];
        dpvsPlane->normal[1] = viewPlane->normal[1];
        dpvsPlane->normal[2] = viewPlane->normal[2];
        dpvsPlane->distance = viewPlane->distance;
        R_SetPlaneSidesDPVS(dpvsPlane);
    }
}

/* Source: CoDUOMP.exe 0x0050f1c0..0x0050f2bb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050f1c0_0050f2bc.mcode and the tail
 * transfer from R_SetupDPVS at 0x00512389.
 * The Mac compiler folds this work into R_SetupDPVS, so
 * R_SetupDPVSProjectionMatrices is a role-derived helper name. Windows forms
 * world.modelMatrix * projectionMatrix in the exact term order below, inverts
 * it, then retains both matrices for portal clipping. */
void R_SetupDPVSProjectionMatrices(void)
{
    float combined[4][4];
    const float(*worldMatrix)[4] = (const float(*)[4])tr.viewParms.world.modelMatrix;
    const float(*projectionMatrix)[4] = (const float(*)[4])tr.viewParms.projectionMatrix;

    for (int32_t column = 0; column < 4; ++column) {
        for (int32_t row = 0; row < 4; ++row) {
            combined[row][column] = (float)((((long double)worldMatrix[row][3] * projectionMatrix[3][column] +
                                              (long double)worldMatrix[row][0] * projectionMatrix[0][column]) +
                                             (long double)worldMatrix[row][1] * projectionMatrix[1][column]) +
                                            (long double)projectionMatrix[2][column] * worldMatrix[row][2]);
        }
    }

    MatrixInverse44(combined, rendererDpvsInverseWorldViewProjectionMatrix);
    /*
     * 0x0050f21f..0x0050f2b3 copies temporary offsets
     * 0x00,0x10,0x20,0x30 to the first destination row, then
     * 0x04,0x14,0x24,0x34 to the second, and so on.  The explicit PE stores
     * therefore transpose the row-major temporary; a linear memcpy changes
     * the matrix consumed by R_PortalClipPlanesInternal.
     */
    for (int32_t row = 0; row < 4; ++row) {
        for (int32_t column = 0; column < 4; ++column) {
            rendererDpvsWorldViewProjectionMatrix[row][column] = combined[column][row];
        }
    }
}

/* Source: CoDUOMP.exe 0x005121a0..0x0051238e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005121a0_0051238f.mcode.
 * Name and no-argument signature: exact same-module Mac symbol R_SetupDPVS.
 * The initialized rendererDpvsInitialSetup flag forces the first setup through
 * even when r_lockpvs is set; subsequent locked views retain the prior state.
 * The ordinary near plane is 0.1 units behind the view origin, while mirrored
 * views use their portal plane. A positive far-plane distance creates the
 * opposing plane; zero, negative, and unordered distances disable it. */
void R_SetupDPVS(void)
{
    static const float nearPlaneOffset = 0.10000000149011612f; /* 0x3dcccccd */
    static const float planeEpsilon = 0.0010000000474974513f; /* 0x3a83126f */
    uint32_t normalBits[3];

    if (rendererDpvsInitialSetup == qfalse && r_lockpvs->integer != 0)
        return;

    rendererDpvsInitialSetup = qfalse;
    R_SetupDPVSFrustumPlanes();

    rendererDpvsViewOrigin[0] = tr.viewParms.orientation.origin[0];
    rendererDpvsViewOrigin[1] = tr.viewParms.orientation.origin[1];
    rendererDpvsViewOrigin[2] = tr.viewParms.orientation.origin[2];

    rendererDpvsNearPlane.normal[0] = tr.viewParms.orientation.axis[0][0];
    rendererDpvsNearPlane.normal[1] = tr.viewParms.orientation.axis[0][1];
    rendererDpvsNearPlane.normal[2] = tr.viewParms.orientation.axis[0][2];
    rendererDpvsNearPlane.distance = (float)((((long double)rendererDpvsNearPlane.normal[2] * rendererDpvsViewOrigin[2] +
                                               (long double)rendererDpvsNearPlane.normal[1] * rendererDpvsViewOrigin[1]) +
                                              (long double)rendererDpvsNearPlane.normal[0] * rendererDpvsViewOrigin[0]) -
                                             (long double)nearPlaneOffset - planeEpsilon);
    memcpy(normalBits, rendererDpvsNearPlane.normal, sizeof(normalBits));
    rendererDpvsNearPlane.sideOffsets[0] =
        normalBits[0] != 0U && (normalBits[0] & 0x80000000U) == 0U ? R_DPVS_SIDE_X_POSITIVE_OFFSET : R_DPVS_SIDE_X_NEGATIVE_OFFSET;
    rendererDpvsNearPlane.sideOffsets[1] =
        normalBits[1] != 0U && (normalBits[1] & 0x80000000U) == 0U ? R_DPVS_SIDE_Y_POSITIVE_OFFSET : R_DPVS_SIDE_Y_NEGATIVE_OFFSET;
    rendererDpvsNearPlane.sideOffsets[2] =
        normalBits[2] != 0U && (normalBits[2] & 0x80000000U) == 0U ? R_DPVS_SIDE_Z_POSITIVE_OFFSET : R_DPVS_SIDE_Z_NEGATIVE_OFFSET;

    rendererDpvsActiveNearPlane = tr.viewParms.isMirror != qfalse ? &tr.viewParms.portalPlane : &rendererDpvsNearPlane;

    const float farPlaneDistance = RE_GetFarPlaneDist();
    if (farPlaneDistance > 0.0f) {
        rendererDpvsFarPlane.normal[0] = -tr.viewParms.orientation.axis[0][0];
        rendererDpvsFarPlane.normal[1] = -tr.viewParms.orientation.axis[0][1];
        rendererDpvsFarPlane.normal[2] = -tr.viewParms.orientation.axis[0][2];
        rendererDpvsFarPlane.distance = (float)((((long double)rendererDpvsFarPlane.normal[2] * rendererDpvsViewOrigin[2] +
                                                  (long double)rendererDpvsFarPlane.normal[1] * rendererDpvsViewOrigin[1]) +
                                                 (long double)rendererDpvsFarPlane.normal[0] * rendererDpvsViewOrigin[0]) -
                                                (long double)farPlaneDistance - planeEpsilon);
        memcpy(normalBits, rendererDpvsFarPlane.normal, sizeof(normalBits));
        rendererDpvsFarPlane.sideOffsets[0] =
            normalBits[0] != 0U && (normalBits[0] & 0x80000000U) == 0U ? R_DPVS_SIDE_X_POSITIVE_OFFSET : R_DPVS_SIDE_X_NEGATIVE_OFFSET;
        rendererDpvsFarPlane.sideOffsets[1] =
            normalBits[1] != 0U && (normalBits[1] & 0x80000000U) == 0U ? R_DPVS_SIDE_Y_POSITIVE_OFFSET : R_DPVS_SIDE_Y_NEGATIVE_OFFSET;
        rendererDpvsFarPlane.sideOffsets[2] =
            normalBits[2] != 0U && (normalBits[2] & 0x80000000U) == 0U ? R_DPVS_SIDE_Z_POSITIVE_OFFSET : R_DPVS_SIDE_Z_NEGATIVE_OFFSET;
        rendererDpvsActiveFarPlane = &rendererDpvsFarPlane;
    } else {
        rendererDpvsActiveFarPlane = NULL;
    }

    /* 0x00512373 loads the cvar pointer at 0x04899e18, which R_Register stores
     * from the r_portalbevels registration at 0x004c493c. 0x0051237c then
     * compares its value with 0.0f; the tail transfer to
     * R_SetupDPVSProjectionMatrices is reached only for an ordered positive
     * value. */
    if (r_portalbevels->value > 0.0f)
        R_SetupDPVSProjectionMatrices();
}

/* Source: CoDUOMP.exe 0x0050ee70..0x0050eec4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050ee70_0050eec5.mcode.
 * Name and no-argument float-return signature: exact same-module Mac symbol
 * RE_GetFarPlaneDist. An explicit r_zfar value wins. With the default zero,
 * the current linear world fog contributes its end distance when valid. The
 * renderer's explicit cull distance is then a lower bound. Ordered C
 * comparisons preserve the Windows behavior for NaNs: a NaN r_zfar bypasses
 * both substitutions, while a NaN lower bound does not replace a number. */
float RE_GetFarPlaneDist(void)
{
    float farPlaneDistance = r_zfar->value;

    if (farPlaneDistance == 0.0f && rendererCurrentFogIndex > 0 && rendererFogs[R_FOG_WORLD_VIEW].registered != qfalse &&
        rendererFogs[R_FOG_WORLD_VIEW].mode == GL_LINEAR) {
        farPlaneDistance = rendererFogs[R_FOG_WORLD_VIEW].end;
    }

    if (farPlaneDistance < rendererCullDistance)
        farPlaneDistance = rendererCullDistance;

    return farPlaneDistance;
}

/* Source: CoDUOMP.exe 0x00512a80..0x00512aa5, recovered from an executable
 * gap bounded by INT3 alignment banks.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00512a80_00512aa6.mcode.
 * Name and float argument: exact renderer export-table target RE_SetCullDist.
 * The <= form is significant: it canonicalizes negative zero to the original
 * positive-zero constant, while an unordered NaN follows the store-input path
 * exactly as in the Windows x87 comparison. */
void RE_SetCullDist(float distance)
{
    if (distance <= 0.0f)
        distance = 0.0f;
    rendererCullDistance = distance;
}

/* Source: CoDUOMP.exe 0x0050eae0..0x0050eb13, recovered from an executable
 * gap bounded by INT3 alignment banks.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050eae0_0050eb14.mcode.
 * Name and ordinary (bounds, plane) signature: exact same-module Mac symbol
 * R_BoxBehindPlane. The plane's three side offsets select the bounds corner
 * farthest along its normal; the box is wholly behind the plane only when that
 * corner's signed distance is negative. */
qboolean R_BoxBehindPlane(const vec3_t bounds[2], const renderer_dpvs_plane_t *plane)
{
    const int32_t xBound = plane->sideOffsets[0] == R_DPVS_SIDE_X_POSITIVE_OFFSET ? 1 : 0;
    const int32_t yBound = plane->sideOffsets[1] == R_DPVS_SIDE_Y_POSITIVE_OFFSET ? 1 : 0;
    const int32_t zBound = plane->sideOffsets[2] == R_DPVS_SIDE_Z_POSITIVE_OFFSET ? 1 : 0;
    const long double cornerDistance = (long double)plane->normal[2] * bounds[zBound][2] +
                                       (long double)plane->normal[1] * bounds[yBound][1] +
                                       (long double)plane->normal[0] * bounds[xBound][0];

    return cornerDistance < plane->distance ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x0050eb20..0x0050eb5f, recovered from an executable
 * gap bounded by INT3 alignment banks.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050eb20_0050eb60.mcode.
 * The Mac compiler did not retain a separately named copy of this helper, so
 * R_BoxInFrontOfPlane is an evidence-based role name. It selects the bounds
 * corner nearest along the plane normal and returns true only when that whole
 * box corner is strictly in front. The z+y+x grouping preserves the Windows
 * x87 operation order; an unordered comparison returns false. */
qboolean R_BoxInFrontOfPlane(const vec3_t bounds[2], const renderer_dpvs_plane_t *plane)
{
    const int32_t xBound = plane->sideOffsets[0] == R_DPVS_SIDE_X_POSITIVE_OFFSET ? 0 : 1;
    const int32_t yBound = plane->sideOffsets[1] == R_DPVS_SIDE_Y_POSITIVE_OFFSET ? 0 : 1;
    const int32_t zBound = plane->sideOffsets[2] == R_DPVS_SIDE_Z_POSITIVE_OFFSET ? 0 : 1;
    const long double cornerDistance = (long double)plane->normal[2] * bounds[zBound][2] +
                                       (long double)plane->normal[1] * bounds[yBound][1] +
                                       (long double)plane->normal[0] * bounds[xBound][0];

    return cornerDistance > plane->distance ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x0050eb60..0x0050ecb9, recovered from an executable
 * gap bounded by INT3 alignment banks.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050eb60_0050ecba.mcode.
 * Name, signature, and state roles: exact same-module Mac symbol
 * R_CullBoxDPVS. Windows uses loops where the Mac compiler additionally
 * specializes common four/five-plane and nine-occluder cases; both implement
 * the same ordered plane, active near/far plane, and occluder tests. */
qboolean R_CullBoxDPVS(const vec3_t bounds[2], const renderer_dpvs_plane_t *planes, int32_t planeCount, int32_t planeIndex)
{
    if (planeIndex < rendererDpvsCullPlaneLimit) {
        rendererDpvsCullPlaneLimit = INT32_MAX;
        for (int32_t index = 0; index < planeCount; ++index) {
            const renderer_dpvs_plane_t *plane = &planes[index];
            const int32_t xBound = plane->sideOffsets[0] == R_DPVS_SIDE_X_POSITIVE_OFFSET ? 1 : 0;
            const int32_t yBound = plane->sideOffsets[1] == R_DPVS_SIDE_Y_POSITIVE_OFFSET ? 1 : 0;
            const int32_t zBound = plane->sideOffsets[2] == R_DPVS_SIDE_Z_POSITIVE_OFFSET ? 1 : 0;
            /* This inlined PE body evaluates X, Y, then Z; the standalone
             * R_BoxBehindPlane helper uses a different association. */
            const long double cornerDistance =
                ((long double)plane->normal[0] * bounds[xBound][0] + (long double)plane->normal[1] * bounds[yBound][1]) +
                (long double)plane->normal[2] * bounds[zBound][2];
            if (cornerDistance < plane->distance)
                return qtrue;
        }
    }

    if (R_BoxBehindPlane(bounds, rendererDpvsActiveNearPlane))
        return qtrue;
    if (rendererDpvsActiveFarPlane != NULL && R_BoxBehindPlane(bounds, rendererDpvsActiveFarPlane)) {
        return qtrue;
    }

    for (int32_t occluderIndex = 0; occluderIndex < rendererDpvsOccluderCount; ++occluderIndex) {
        renderer_occluder_t *occluder = rendererDpvsOccluders[occluderIndex];
        if (planeIndex >= occluder->cullPlaneLimit)
            continue;

        occluder->cullPlaneLimit = INT32_MAX;
        int32_t activePlaneIndex;
        for (activePlaneIndex = 0; activePlaneIndex < occluder->activePlaneCount; ++activePlaneIndex) {
            /*
             * 0x0050ec6a..0x0050ec9f advances only when the box is behind
             * this plane.  The first plane that does not contain the box in
             * the occluded half-space rejects this occluder.
             */
            if (!R_BoxBehindPlane(bounds, &occluder->activePlanes[activePlaneIndex])) {
                break;
            }
        }
        if (activePlaneIndex == occluder->activePlaneCount)
            return qtrue;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x0050ecc0..0x0050edc7, recovered from an executable
 * gap bounded by INT3 alignment banks.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050ecc0_0050edc8.mcode.
 * No separately named copy survives in the Mac symbol bank;
 * R_CullBoxDPVSStrict is a role name distinguishing this complementary
 * classifier from R_CullBoxDPVS. The ordinary classifier rejects a box wholly
 * behind any clipping plane. This strict form additionally rejects a box that
 * is not wholly in front of every supplied plane, and records the earliest
 * plane position at which that stronger result remains reusable. */
qboolean R_CullBoxDPVSStrict(const vec3_t bounds[2], const renderer_dpvs_plane_t *planes, int32_t planeCount, int32_t planeIndex)
{
    if (planeIndex < rendererDpvsCullPlaneLimit) {
        for (int32_t index = 0; index < planeCount; ++index) {
            const renderer_dpvs_plane_t *plane = &planes[index];
            const int32_t xBound = plane->sideOffsets[0] == R_DPVS_SIDE_X_POSITIVE_OFFSET ? 0 : 1;
            const int32_t yBound = plane->sideOffsets[1] == R_DPVS_SIDE_Y_POSITIVE_OFFSET ? 0 : 1;
            const int32_t zBound = plane->sideOffsets[2] == R_DPVS_SIDE_Z_POSITIVE_OFFSET ? 0 : 1;
            /* Windows evaluates (y+x)+z in this first plane loop. */
            const long double cornerDistance =
                ((long double)plane->normal[1] * bounds[yBound][1] + (long double)plane->normal[0] * bounds[xBound][0]) +
                (long double)plane->normal[2] * bounds[zBound][2];

            if (!(cornerDistance > plane->distance))
                return qtrue;
        }
        rendererDpvsCullPlaneLimit = planeIndex + 1;
    }

    for (int32_t occluderIndex = 0; occluderIndex < rendererDpvsOccluderCount; ++occluderIndex) {
        renderer_occluder_t *occluder = rendererDpvsOccluders[occluderIndex];
        if (planeIndex >= occluder->cullPlaneLimit)
            continue;

        int32_t activePlaneIndex;
        for (activePlaneIndex = 0; activePlaneIndex < occluder->activePlaneCount; ++activePlaneIndex) {
            const renderer_dpvs_plane_t *plane = &occluder->activePlanes[activePlaneIndex];
            const int32_t xBound = plane->sideOffsets[0] == R_DPVS_SIDE_X_POSITIVE_OFFSET ? 0 : 1;
            const int32_t yBound = plane->sideOffsets[1] == R_DPVS_SIDE_Y_POSITIVE_OFFSET ? 0 : 1;
            const int32_t zBound = plane->sideOffsets[2] == R_DPVS_SIDE_Z_POSITIVE_OFFSET ? 0 : 1;
            /* Windows evaluates (z+y)+x in the active-occluder loop. */
            const long double cornerDistance =
                ((long double)plane->normal[2] * bounds[zBound][2] + (long double)plane->normal[1] * bounds[yBound][1]) +
                (long double)plane->normal[0] * bounds[xBound][0];

            if (cornerDistance > plane->distance) {
                occluder->cullPlaneLimit = planeIndex + 1;
                break;
            }
        }
        if (activePlaneIndex == occluder->activePlaneCount)
            return qtrue;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x00510bf0..0x00510d8d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00510bf0_00510d8e.mcode.
 * Name and source-level role: exact same-module Mac symbol
 * R_AddTrianglesSurface. The Windows compiler inlines R_CullBoxDPVS into this
 * body; retaining the proved helper call preserves the same ordered frustum,
 * near/far, and occluder classification. Bit 1 of r_showaabbtrees draws every
 * surviving surface bound in the original pooled green debug color. */
void R_AddTrianglesSurface(msurface_t *worldSurface, uint32_t dlightBits, const renderer_dpvs_plane_t *planes, int32_t planeCount,
                           int32_t planeIndex)
{
    renderer_lit_surface_t *surface;

    if (worldSurface->viewCount == tr.viewCount)
        return;

    surface = (renderer_lit_surface_t *)worldSurface->data;
    if (R_CullBoxDPVS(&surface->boundsMin, planes, planeCount, planeIndex)) {
        return;
    }

    if ((r_showaabbtrees->integer & 2) != 0) {
        R_AddDebugBox(surface->boundsMin, surface->boundsMax, visibleSurfaceDebugColor);
    }
    R_AddWorldSurfaceNoCull(worldSurface, dlightBits);
}

/* Source: CoDUOMP.exe 0x00512710..0x005127af.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00512710_005127b0.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * R_AddSkySurfacesDPVS. Sky geometry is selected from the surfaces wholly
 * behind the current far plane. The active far plane is then disabled before
 * those surfaces are submitted, so their ordinary DPVS test cannot reject
 * them a second time for being beyond the world distance. */
void R_AddSkySurfacesDPVS(void)
{
    renderer_dpvs_plane_t *farPlane = rendererDpvsActiveFarPlane;

    if (farPlane == NULL)
        return;

    rendererDpvsActiveFarPlane = NULL;
    for (int32_t surfaceIndex = 0; surfaceIndex < tr.world->skySurfaceCount; ++surfaceIndex) {
        msurface_t *worldSurface = tr.world->skySurfaces[surfaceIndex];
        renderer_lit_surface_t *surface = (renderer_lit_surface_t *)worldSurface->data;

        if (worldSurface->viewCount != tr.viewCount && R_BoxBehindPlane(&surface->boundsMin, farPlane)) {
            R_AddTrianglesSurface(worldSurface, 0, rendererDpvsFrustumPlanes, R_FRUSTUM_PLANE_COUNT, 0);
        }
    }
}

/* Source: CoDUOMP.exe 0x00512390..0x0051270b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00512390_0051270c.mcode.
 * Name and two-argument source signature: exact same-module Mac symbol
 * R_AddCoronas. Each authored corona becomes a sprite shifted 16 units toward
 * the camera. Its conservative cube uses sqrt(3) times the sprite radius, and
 * the optional vertical fade is applied only after the complete DPVS box test.
 * The alpha conversion retains the original MSVC _ftol2 truncation behavior. */
void R_AddCoronas(const renderer_dpvs_plane_t *planes, int32_t planeCount)
{
    static const float coronaBoundsScale = 1.7320507764816284f; /* 0x3fddb3d7, nominally sqrt(3) */
    static const float coronaViewOffset = 16.0f; /* 0x41800000 */
    refEntity_t entity;

    memset(&entity, 0, sizeof(entity));
    entity.reType = RT_SPRITE;

    for (int32_t coronaIndex = 0; coronaIndex < tr.world->coronaCount; ++coronaIndex) {
        const renderer_world_corona_t *corona = &tr.world->coronas[coronaIndex];
        vec3_t viewDirection;
        vec3_t bounds[2];

        viewDirection[0] = tr.viewParms.orientation.origin[0] - corona->origin[0];
        viewDirection[1] = tr.viewParms.orientation.origin[1] - corona->origin[1];
        viewDirection[2] = tr.viewParms.orientation.origin[2] - corona->origin[2];
        (void)VectorNormalize(viewDirection);

        if (!(viewDirection[2] >= corona->zCutoff))
            continue;

        const long double boundsRadiusRaw = (long double)corona->scale * coronaBoundsScale;
        for (int32_t component = 0; component < 3; ++component) {
            bounds[0][component] = (float)((long double)corona->origin[component] - boundsRadiusRaw);
            bounds[1][component] = (float)(boundsRadiusRaw + (long double)corona->origin[component]);
        }

        if (R_CullBoxDPVS(bounds, planes, planeCount, 0))
            continue;

        for (int32_t component = 0; component < 3; ++component) {
            entity.origin[component] =
                (float)((long double)viewDirection[component] * coronaViewOffset + (long double)corona->origin[component]);
        }
        entity.spriteShaderHandle = corona->shader->index;
        entity.shaderRGBA[0] = corona->color[0];
        entity.shaderRGBA[1] = corona->color[1];
        entity.shaderRGBA[2] = corona->color[2];
        entity.radius = corona->scale;

        if (viewDirection[2] > corona->zFadeOut) {
            entity.shaderRGBA[3] = corona->color[3];
        } else {
            const long double fadedAlphaRaw = (long double)corona->color[3] * ((long double)corona->zCutoff - viewDirection[2]) /
                                              ((long double)corona->zCutoff - corona->zFadeOut);
            entity.shaderRGBA[3] = coduo_fp_to_u8_extended(fadedAlphaRaw);
        }

        RE_AddRefEntityToScene(&entity, NULL);
    }
}

/* Source: CoDUOMP.exe 0x00510770..0x005107e1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00510770_005107e2.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * R_CellForCamera. The world BSP is descended from its root using the current
 * view origin; nonpositive and unordered plane distances select child 1. */
int32_t R_CellForCamera(void)
{
    mnode_t *node;

    if (tr.world == NULL)
        ri.Error(ERR_DROP, "\x15"
                           "R_CellForCamera: bad model");

    node = tr.world->nodes;
    while (node->contents == R_WORLD_NODE_NO_CELL) {
        const cplane_t *plane = node->data.node.plane;
        const long double distance =
            (((long double)plane->normal[2] * rendererDpvsViewOrigin[2] + (long double)plane->normal[1] * rendererDpvsViewOrigin[1]) +
             (long double)plane->normal[0] * rendererDpvsViewOrigin[0]) -
            (long double)plane->dist;

        node = distance > 0.0f ? node->data.node.children[0] : node->data.node.children[1];
    }

    return node->cellIndex;
}

/* Source: CoDUOMP.exe 0x005107f0..0x00510846.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005107f0_00510847.mcode.
 * Name and signature: exact same-module Mac symbol
 * R_PortalBehindAnyPlane. A portal is rejected when any supplied plane has no
 * vertex strictly in front of it; unordered vertex distances are not front. */
qboolean R_PortalBehindAnyPlane(const renderer_portal_t *portal, const renderer_dpvs_plane_t *planes, int32_t planeCount)
{
    uint32_t planesRemaining = (uint32_t)planeCount;

    if (planesRemaining == 0)
        return qfalse;

    const vec3_t *const vertices = portal->vertices;
    const uint32_t vertexCount = (uint32_t)portal->vertexCount;

    while (planesRemaining != 0) {
        const vec3_t *vertex = vertices;
        uint32_t verticesRemaining = vertexCount;

        while (verticesRemaining != 0) {
            const long double distance = (long double)planes->normal[2] * (long double)(*vertex)[2] +
                                         (long double)planes->normal[1] * (long double)(*vertex)[1] +
                                         (long double)planes->normal[0] * (long double)(*vertex)[0];

            if (distance > (long double)planes->distance)
                break;
            --verticesRemaining;
            ++vertex;
        }

        if (verticesRemaining == 0)
            return qtrue;

        --planesRemaining;
        ++planes;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x00510850..0x005108ae.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00510850_005108af.mcode.
 * Name and signature: exact same-module Mac symbol
 * R_PortalBehindAllPlanes. This stricter classifier succeeds only when every
 * portal vertex is non-front-facing or unordered for every supplied plane.
 * The PE uses decrement-to-zero loops for both signed inputs; uint32_t
 * counters preserve those exact nonzero and wrap semantics. */
qboolean R_PortalBehindAllPlanes(const renderer_portal_t *portal, const renderer_dpvs_plane_t *planes, int32_t planeCount)
{
    uint32_t planesRemaining = (uint32_t)planeCount;
    const renderer_dpvs_plane_t *plane = planes;

    while (planesRemaining != 0U) {
        uint32_t verticesRemaining = (uint32_t)portal->vertexCount;
        const vec3_t *vertex = portal->vertices;

        while (verticesRemaining != 0U) {
            const long double distance =
                ((long double)plane->normal[2] * (long double)(*vertex)[2] + (long double)plane->normal[1] * (long double)(*vertex)[1]) +
                (long double)plane->normal[0] * (long double)(*vertex)[0];

            if (distance > (long double)plane->distance)
                return qfalse;

            ++vertex;
            --verticesRemaining;
        }

        ++plane;
        --planesRemaining;
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x005108b0..0x00510a68.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005108b0_00510a69.mcode.
 * Name and clipping roles: exact same-module Mac symbol R_ChopPortalWinding.
 * The original reserves 1024 output vertices and one repeated classification
 * slot. Distances strictly below -epsilon are clipped, distances strictly
 * above +epsilon are retained, and the intervening band (including unordered)
 * is coplanar; a coplanar vertex is retained and skips the intersection
 * emission entirely (0x005109bd). An edge intersection keeps its x87 fraction
 * live across all three component calculations. */
vec3_t *R_ChopPortalWinding(const renderer_dpvs_plane_t *plane, vec3_t *inputVertices, int32_t *vertexCount, vec3_t *outputVertices)
{
    enum {
        R_PORTAL_CLIP_FRONT = 0,
        R_PORTAL_CLIP_BACK = 1,
        R_PORTAL_CLIP_ON = 2
    };
    static const float planeEpsilon = 0.0010000000474974513f; /* 0x3a83126f */
    int32_t sides[R_PORTAL_CLIP_VERTEX_CAPACITY + 1];
    float distances[R_PORTAL_CLIP_VERTEX_CAPACITY + 1];
    int32_t frontCount = 0;
    int32_t backCount = 0;
    const int32_t inputCount = *vertexCount;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (inputCount < 0 || inputCount > R_PORTAL_CLIP_VERTEX_CAPACITY) {
        ri.Error(ERR_DROP, "R_ChopPortalWinding: invalid input vertex count %i", inputCount);
        *vertexCount = 0;
        return NULL;
    }

    for (int32_t vertexIndex = 0; vertexIndex < inputCount; ++vertexIndex) {
        const vec3_t *vertex = &inputVertices[vertexIndex];
        const long double distanceRaw = (long double)plane->normal[2] * (*vertex)[2] + (long double)plane->normal[0] * (*vertex)[0] +
                                        (long double)plane->normal[1] * (*vertex)[1] - (long double)plane->distance;
        distances[vertexIndex] = (float)distanceRaw;

        /*
         * 0x005108ff..0x00510920 takes the back path only for an
         * ordered value strictly below -epsilon.  Equality remains in the
         * coplanar band, as does an unordered value.
         */
        if (distanceRaw < -(long double)planeEpsilon) {
            sides[vertexIndex] = R_PORTAL_CLIP_BACK;
            ++backCount;
        } else if (distanceRaw > (long double)planeEpsilon) {
            sides[vertexIndex] = R_PORTAL_CLIP_FRONT;
            ++frontCount;
        } else {
            sides[vertexIndex] = R_PORTAL_CLIP_ON;
        }
    }

    if (frontCount == 0) {
        *vertexCount = 0;
        return NULL;
    }
    if (backCount == 0)
        return inputVertices;

    sides[inputCount] = sides[0];
    distances[inputCount] = distances[0];

    int32_t outputCount = 0;
    for (int32_t vertexIndex = 0; vertexIndex < inputCount; ++vertexIndex) {
        const int32_t side = sides[vertexIndex];

        if (side == R_PORTAL_CLIP_ON) {
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (outputCount >= R_PORTAL_CLIP_VERTEX_CAPACITY) {
                ri.Error(ERR_DROP, "R_ChopPortalWinding: clipped portal exceeds %i vertices", R_PORTAL_CLIP_VERTEX_CAPACITY);
                *vertexCount = 0;
                return NULL;
            }
            memcpy(outputVertices[outputCount], inputVertices[vertexIndex], sizeof(vec3_t));
            ++outputCount;
            /*
             * 0x005109bd jumps straight to the loop tail: a coplanar vertex
             * is retained and never evaluates the intersection emission.
             * Only front and back vertices continue below.
             */
            continue;
        }

        if (side == R_PORTAL_CLIP_FRONT) {
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (outputCount >= R_PORTAL_CLIP_VERTEX_CAPACITY) {
                ri.Error(ERR_DROP, "R_ChopPortalWinding: clipped portal exceeds %i vertices", R_PORTAL_CLIP_VERTEX_CAPACITY);
                *vertexCount = 0;
                return NULL;
            }
            memcpy(outputVertices[outputCount], inputVertices[vertexIndex], sizeof(vec3_t));
            ++outputCount;
        }

        const int32_t nextSide = sides[vertexIndex + 1];
        if (nextSide != R_PORTAL_CLIP_ON && nextSide != side) {
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (outputCount >= R_PORTAL_CLIP_VERTEX_CAPACITY) {
                ri.Error(ERR_DROP, "R_ChopPortalWinding: clipped portal exceeds %i vertices", R_PORTAL_CLIP_VERTEX_CAPACITY);
                *vertexCount = 0;
                return NULL;
            }
            const int32_t nextIndex = (vertexIndex + 1) % inputCount;
            const long double fraction =
                (long double)distances[vertexIndex] / ((long double)distances[vertexIndex] - distances[vertexIndex + 1]);

            for (int32_t component = 0; component < 3; ++component) {
                outputVertices[outputCount][component] =
                    (float)(((long double)inputVertices[nextIndex][component] - inputVertices[vertexIndex][component]) * fraction +
                            inputVertices[vertexIndex][component]);
            }
            ++outputCount;
            if (outputCount == R_PORTAL_CLIP_VERTEX_CAPACITY)
                break;
        }
    }

    *vertexCount = outputCount;
    return outputVertices;
}

/* Source: CoDUOMP.exe 0x00510a70..0x00510be0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00510a70_00510be1.mcode.
 * Name and source-level roles: exact same-module Mac symbol
 * R_PortalClipPlanes. The 24 KiB local is two alternating 1024-vertex winding
 * buffers. Portals within one unit of the camera retain a temporary copy of
 * the incoming planes; more distant portals are clipped and converted into a
 * tighter plane set by R_PortalClipPlanesInternal. The near test consumes the
 * retained camera-plane distance, and the shortcut copy size wraps at the
 * original 32-bit multiply. */
renderer_dpvs_plane_t *R_PortalClipPlanes(const renderer_portal_t *portal, const renderer_dpvs_plane_t *portalPlane,
                                          const renderer_dpvs_plane_t *planes, int32_t planeCount, int32_t *outPlaneCount)
{
    static const float nearPortalDistance = 1.0f; /* 0x3f800000 */
    vec3_t clipVertices[2][R_PORTAL_CLIP_VERTEX_CAPACITY];
    vec3_t *vertices;

    *outPlaneCount = portal->vertexCount;
    vertices = R_ChopPortalWinding(portalPlane, portal->vertices, outPlaneCount, clipVertices[0]);
    if (*outPlaneCount == 0)
        return NULL;

    const long double cameraPlaneDistance =
        (long double)portal->plane.distance - ((long double)rendererDpvsViewOrigin[2] * portal->plane.normal[2] +
                                               (long double)rendererDpvsViewOrigin[1] * portal->plane.normal[1] +
                                               (long double)rendererDpvsViewOrigin[0] * portal->plane.normal[0]);
    /*
     * 0x00510ad1..0x00510adf enters this shortcut only for an ordered
     * distance strictly below 1.0f.  Equality and unordered values continue
     * through portal clipping.
     */
    if (cameraPlaneDistance < (long double)nearPortalDistance) {
        const uint32_t planeBytes = (uint32_t)planeCount * 20u;
        renderer_dpvs_plane_t *planeCopy = ri.Hunk_AllocateTempMemory((size_t)planeBytes);
        memcpy(planeCopy, planes, (size_t)planeBytes);
        *outPlaneCount = planeCount;
        return planeCopy;
    }

    if (rendererDpvsActiveFarPlane != NULL) {
        vec3_t *output = vertices == clipVertices[0] ? clipVertices[1] : clipVertices[0];
        vertices = R_ChopPortalWinding(rendererDpvsActiveFarPlane, vertices, outPlaneCount, output);
        if (*outPlaneCount == 0)
            return NULL;
    }

    for (int32_t planeIndex = 0; planeIndex < planeCount; ++planeIndex) {
        vec3_t *output = vertices == clipVertices[0] ? clipVertices[1] : clipVertices[0];
        vertices = R_ChopPortalWinding(&planes[planeIndex], vertices, outPlaneCount, output);
        if (*outPlaneCount == 0)
            return NULL;
    }

    if (r_showportals->integer != 0) {
        R_AddDebugPolygon(portalDebugColor, *outPlaneCount, (const vec3_t *)vertices);
    }

    return R_PortalClipPlanesInternal(vertices, outPlaneCount, *outPlaneCount);
}

/* Source: CoDUOMP.exe 0x00512060..0x00512193.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00512060_00512194.mcode.
 * Name and recursive source structure: exact same-module Mac symbol
 * R_RecursivePortalWalk. Each cell installs its visible occluders and submits
 * its owned geometry before outgoing portals are considered. The per-portal
 * recursion flag prevents cycles, and every temporary plane set returned by
 * R_PortalClipPlanes is released after the child traversal. */
void R_RecursivePortalWalk(renderer_world_cell_t *cell, const renderer_dpvs_plane_t *sourcePlane, const renderer_dpvs_plane_t *planes,
                           int32_t planeCount, uint32_t dlightBits)
{
    static const float planeEpsilon = 0.0010000000474974513f; /* 0x3a83126f */

    R_AddCellOccluders(cell, planes, planeCount);
    R_AddCellSurfaces(cell, planes, planeCount, dlightBits);
    R_AddCellCullGroups(cell, planes, planeCount, dlightBits);

    renderer_portal_t *portal = cell->portalReference.portals;
    for (uint32_t portalsRemaining = (uint32_t)cell->portalCount; portalsRemaining != 0; --portalsRemaining, ++portal) {

        if (portal->recursionActive != qfalse)
            continue;

        const long double cameraDistance = (long double)rendererDpvsViewOrigin[2] * portal->plane.normal[2] +
                                           (long double)rendererDpvsViewOrigin[1] * portal->plane.normal[1] +
                                           (long double)rendererDpvsViewOrigin[0] * portal->plane.normal[0];
        if (cameraDistance > (long double)portal->plane.distance + planeEpsilon) {
            continue;
        }

        if (R_PortalBehindAnyPlane(portal, planes, planeCount))
            continue;

        qboolean occluded = qfalse;
        for (int32_t occluderIndex = 0; occluderIndex < rendererDpvsOccluderCount; ++occluderIndex) {
            const renderer_occluder_t *occluder = rendererDpvsOccluders[occluderIndex];
            if (R_PortalBehindAllPlanes(portal, occluder->activePlanes, occluder->activePlaneCount)) {
                occluded = qtrue;
                break;
            }
        }
        if (occluded)
            continue;

        int32_t clippedPlaneCount;
        renderer_dpvs_plane_t *clippedPlanes = R_PortalClipPlanes(portal, sourcePlane, planes, planeCount, &clippedPlaneCount);
        if (clippedPlaneCount == 0)
            continue;

        portal->recursionActive = qtrue;
        R_RecursivePortalWalk(portal->cell, &portal->plane, clippedPlanes, clippedPlaneCount, dlightBits);
        ri.Hunk_FreeTempMemory(clippedPlanes);
        portal->recursionActive = qfalse;
    }
}

/* Source: CoDUOMP.exe 0x00511c10..0x00511caf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00511c10_00511cb0.mcode.
 * Name and cell/AABB ownership: exact same-module Mac symbol
 * R_AddCellSurfaces. A cell root with children delegates each 40-byte child to
 * R_AddAABBTreeSurfaces_r; a leaf root submits its contiguous surface span
 * directly. Renderer entities are culled only when r_drawentities is enabled. */
void R_AddCellSurfaces(renderer_world_cell_t *cell, const renderer_dpvs_plane_t *planes, int32_t planeCount, uint32_t dlightBits)
{
    renderer_aabb_tree_t *tree = cell->aabbTree;

    rendererDpvsCullPlaneLimit = INT32_MAX;
    if (tree->childCount != 0) {
        for (int32_t childIndex = 0; childIndex < tree->childCount; ++childIndex) {
            R_AddAABBTreeSurfaces_r(&tree->children[childIndex], planes, planeCount, dlightBits, 0);
        }
    } else {
        for (int32_t surfaceIndex = 0; surfaceIndex < tree->surfaceCount; ++surfaceIndex) {
            R_AddTrianglesSurface(&tree->surfaces[surfaceIndex], dlightBits, planes, planeCount, 0);
        }
    }

    if (r_drawentities->integer != 0)
        R_CullModels(cell, planes, planeCount);
}

/* Source: CoDUOMP.exe 0x00511cb0..0x00511e37.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00511cb0_00511e38.mcode.
 * The Mac compiler folds this work into R_AddCellCullGroups, so
 * R_AddCullGroupDPVS is a role-derived helper name. The Windows compiler
 * inlines R_CullBoxDPVS here; retaining the proved helper call preserves the
 * same ordered frustum, near/far, and active-occluder classification. */
void R_AddCullGroupDPVS(renderer_cull_group_t *group, const renderer_dpvs_plane_t *planes, int32_t planeCount, uint32_t dlightBits)
{
    if (R_CullBoxDPVS(&group->mins, planes, planeCount, 0))
        return;

    if ((r_showportals->integer & 1) != 0)
        R_AddDebugBox(group->mins, group->maxs, cullGroupDebugColor);

    group->viewCount = tr.viewCount;
    uint32_t surfacesRemaining = (uint32_t)group->surfaceCount;
    msurface_t *surface = group->surfaces;
    while (surfacesRemaining != 0u) {
        R_AddWorldSurfaceNoCull(surface, dlightBits);
        ++surface;
        --surfacesRemaining;
    }
}

/* Source: CoDUOMP.exe 0x00511e40..0x00511fc3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00511e40_00511fc4.mcode.
 * The Mac compiler folds this work into R_AddCellCullGroups, so
 * R_AddStaticModelDPVS is a role-derived helper name. The Windows compiler
 * likewise inlines the shared R_CullBoxDPVS classifier. The word at model
 * offset 0xb4 is proved here and in R_AddCellCullGroups to be a view-count
 * stamp, not dynamic-light state. */
void R_AddStaticModelDPVS(renderer_static_model_t *model, const renderer_dpvs_plane_t *planes, int32_t planeCount)
{
    /* The Windows compiler inlines R_CullBoxDPVS(&model->mins, planes,
     * planeCount, 0) here.  In that inlined copy the frustum-plane corner
     * distance is evaluated (normal[0]*x + normal[1]*y) + normal[2]*z --
     * X->Y->Z, 0x00511e74..0x00511e8f -- distinct from the standalone
     * R_BoxBehindPlane's z+y+x term order.  The near/far (0x00511eb0 /
     * 0x00511ee8) and occluder sub-tests keep R_BoxBehindPlane's z+y+x order,
     * so those remain R_BoxBehindPlane calls; only the frustum loop is inlined
     * to preserve the X->Y->Z association. */
    const vec3_t *bounds = &model->mins;

    if (0 < rendererDpvsCullPlaneLimit) {
        rendererDpvsCullPlaneLimit = INT32_MAX;
        for (int32_t index = 0; index < planeCount; ++index) {
            const renderer_dpvs_plane_t *plane = &planes[index];
            const int32_t xBound = plane->sideOffsets[0] == R_DPVS_SIDE_X_POSITIVE_OFFSET ? 1 : 0;
            const int32_t yBound = plane->sideOffsets[1] == R_DPVS_SIDE_Y_POSITIVE_OFFSET ? 1 : 0;
            const int32_t zBound = plane->sideOffsets[2] == R_DPVS_SIDE_Z_POSITIVE_OFFSET ? 1 : 0;
            const long double cornerDistance =
                ((long double)plane->normal[0] * bounds[xBound][0] + (long double)plane->normal[1] * bounds[yBound][1]) +
                (long double)plane->normal[2] * bounds[zBound][2];
            if (cornerDistance < plane->distance)
                return;
        }
    }

    if (R_BoxBehindPlane(bounds, rendererDpvsActiveNearPlane))
        return;
    if (rendererDpvsActiveFarPlane != NULL && R_BoxBehindPlane(bounds, rendererDpvsActiveFarPlane)) {
        return;
    }

    for (int32_t occluderIndex = 0; occluderIndex < rendererDpvsOccluderCount; ++occluderIndex) {
        renderer_occluder_t *occluder = rendererDpvsOccluders[occluderIndex];
        if (0 >= occluder->cullPlaneLimit)
            continue;

        occluder->cullPlaneLimit = INT32_MAX;
        int32_t activePlaneIndex;
        for (activePlaneIndex = 0; activePlaneIndex < occluder->activePlaneCount; ++activePlaneIndex) {
            if (!R_BoxBehindPlane(bounds, &occluder->activePlanes[activePlaneIndex])) {
                break;
            }
        }
        if (activePlaneIndex == occluder->activePlaneCount)
            return;
    }

    if (r_showCullSModels->integer != 0)
        R_AddDebugBox(model->mins, model->maxs, staticModelDebugColor);

    model->viewCount = tr.viewCount;
    RE_AddRefEntityToScene(&model->entity, model);
}

/* Source: CoDUOMP.exe 0x00511fd0..0x00512057.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00511fd0_00512058.mcode.
 * Name and source-level loops: exact same-module Mac symbol
 * R_AddCellCullGroups. Cull groups and static models are independently stamped
 * with tr.viewCount because either can be linked into more than one cell. */
void R_AddCellCullGroups(renderer_world_cell_t *cell, const renderer_dpvs_plane_t *planes, int32_t planeCount, uint32_t dlightBits)
{
    /* 0x00511fd1 snapshots the count and the DEC/JNZ loop treats every
     * nonzero target dword as a count, including its high-bit-set range. */
    const uint32_t cullGroupCount = (uint32_t)cell->cullGroupCount;

    for (uint32_t groupIndex = 0; groupIndex < cullGroupCount; ++groupIndex) {
        renderer_cull_group_t *group = cell->cullGroups[groupIndex];
        if (group->viewCount != tr.viewCount) {
            R_AddCullGroupDPVS(group, planes, planeCount, dlightBits);
        }
    }

    if (r_drawSModels->integer == 0)
        return;

    for (renderer_cell_model_link_t *link = cell->modelLinks; link != NULL; link = link->next) {
        renderer_static_model_t *model = link->model;
        if (model->viewCount != tr.viewCount)
            R_AddStaticModelDPVS(model, planes, planeCount);
    }
}

/* Source: CoDUOMP.exe 0x00510380..0x005103c0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00510380_005103c1.mcode.
 * Name and source structure: exact same-module Mac symbol
 * R_CullOccluderByPlane. MSVC also inlines its two calls into
 * R_AddCellOccluders at 0x00510420..0x0051044a and
 * 0x00510460..0x0051048d. An occluder is culled only when every vertex lies on
 * or behind the supplied plane. The y+z+x grouping preserves the Windows x87
 * evaluation order; an unordered vertex comparison remains on the culled
 * path. */
qboolean R_CullOccluderByPlane(const renderer_occluder_t *occluder, const renderer_dpvs_plane_t *plane)
{
    for (int32_t vertexIndex = 0; vertexIndex < occluder->vertexCount; ++vertexIndex) {
        const vec3_t *vertex = &occluder->vertices[vertexIndex];
        const long double distance = ((long double)plane->normal[1] * (*vertex)[1] + (long double)plane->normal[2] * (*vertex)[2]) +
                                     (long double)plane->normal[0] * (*vertex)[0];
        if (distance > plane->distance)
            return qfalse;
    }
    return qtrue;
}

enum renderer_dpvs_occluder_limits_e {
    R_DPVS_MAX_ACTIVE_OCCLUDERS = 1024,
    R_DPVS_MAX_ACTIVE_OCCLUDER_PLANES = 6144,
    R_DPVS_MAX_CELL_ENTITY_LINKS = 4096,
    R_SHOW_CULL_MODE_BOUNDS = 1,
    R_SHOW_CULL_MODE_CELL_LINKS = 2
};

/* Source: CoDUOMP.exe 0x005103d0..0x00510768.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005103d0_00510769.mcode.
 * Name and helper-level source structure: exact same-module Mac symbol
 * R_AddCellOccluders. The camera-facing authored planes are copied into the
 * per-walk active-plane buffer. Every edge separating opposite camera sides
 * contributes one normalized silhouette plane through the view origin. */
void R_AddCellOccluders(renderer_world_cell_t *cell, const renderer_dpvs_plane_t *planes, int32_t planeCount)
{
    rendererDpvsOccluderCount = 0;
    rendererDpvsActivePlaneCount = 0;

    for (int32_t occluderIndex = 0; occluderIndex < cell->occluderCount; ++occluderIndex) {
        renderer_occluder_t *occluder = cell->occluders[occluderIndex];

        if (R_CullOccluderByPlane(occluder, rendererDpvsActiveNearPlane)) {
            continue;
        }

        qboolean clipped = qfalse;
        uint32_t planesRemaining = (uint32_t)planeCount;
        const renderer_dpvs_plane_t *plane = planes;
        while (planesRemaining != 0u) {
            qboolean culledByPlane = qtrue;
            for (int32_t vertexIndex = 0; vertexIndex < occluder->vertexCount; ++vertexIndex) {
                const vec3_t *vertex = &occluder->vertices[vertexIndex];
                /*
                 * The inlined clipping-plane test at
                 * 0x00510464..0x00510478 evaluates z+y+x, unlike the
                 * y+z+x order of the preceding active-near-plane test.
                 */
                const long double distance = ((long double)plane->normal[2] * (*vertex)[2] + (long double)plane->normal[1] * (*vertex)[1]) +
                                             (long double)plane->normal[0] * (*vertex)[0];
                if (distance > plane->distance) {
                    culledByPlane = qfalse;
                    break;
                }
            }
            if (culledByPlane != qfalse) {
                clipped = qtrue;
                break;
            }
            ++plane;
            --planesRemaining;
        }
        if (clipped)
            continue;

        if (rendererDpvsOccluderCount == R_DPVS_MAX_ACTIVE_OCCLUDERS) {
            ri.Error(ERR_DROP, "\x15Too many active occluder brushes");
        }
        rendererDpvsOccluders[rendererDpvsOccluderCount] = occluder;
        rendererDpvsOccluderCount = (int32_t)((uint32_t)rendererDpvsOccluderCount + 1u);

        occluder->cullPlaneLimit = INT32_MAX;
        occluder->activePlaneCount = rendererDpvsActivePlaneCount;
        occluder->activePlanes = &rendererDpvsActivePlanes[rendererDpvsActivePlaneCount];

        for (int32_t authoredPlaneIndex = 0; authoredPlaneIndex < occluder->planeCount; ++authoredPlaneIndex) {
            renderer_dpvs_plane_t *plane = &occluder->planes[authoredPlaneIndex];
            plane->cameraSide = R_DPVS_CAMERA_BEHIND_PLANE;

            const long double cameraDistance =
                ((long double)rendererDpvsViewOrigin[2] * plane->normal[2] + (long double)rendererDpvsViewOrigin[1] * plane->normal[1]) +
                (long double)rendererDpvsViewOrigin[0] * plane->normal[0];
            if (cameraDistance > plane->distance) {
                plane->cameraSide = R_DPVS_CAMERA_IN_FRONT_OF_PLANE;
                if (rendererDpvsActivePlaneCount == R_DPVS_MAX_ACTIVE_OCCLUDER_PLANES) {
                    ri.Error(ERR_DROP, "\x15Too many active occluder faces");
                }
                rendererDpvsActivePlanes[rendererDpvsActivePlaneCount] = *plane;
                rendererDpvsActivePlaneCount = (int32_t)((uint32_t)rendererDpvsActivePlaneCount + 1u);
            }
        }

        for (int32_t edgeIndex = 0; edgeIndex < occluder->edgeCount; ++edgeIndex) {
            renderer_occluder_edge_t *edge = &occluder->edges[edgeIndex];
            const uint8_t cameraSide = edge->planes[0]->cameraSide;
            if (cameraSide == edge->planes[1]->cameraSide)
                continue;

            vec3_t first;
            vec3_t second;
            const vec3_t *firstVertex = edge->vertices[cameraSide == R_DPVS_CAMERA_BEHIND_PLANE];
            const vec3_t *secondVertex = edge->vertices[cameraSide];
            for (int32_t component = 0; component < 3; ++component) {
                first[component] = (*firstVertex)[component] - rendererDpvsViewOrigin[component];
                second[component] = (*secondVertex)[component] - rendererDpvsViewOrigin[component];
            }

            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (rendererDpvsActivePlaneCount >= R_DPVS_MAX_ACTIVE_OCCLUDER_PLANES) {
                ri.Error(ERR_DROP, "\x15Too many active occluder faces");
                return;
            }
            renderer_dpvs_plane_t *activePlane = &rendererDpvsActivePlanes[rendererDpvsActivePlaneCount];
            activePlane->normal[0] = (float)((long double)second[1] * first[2] - (long double)second[2] * first[1]);
            activePlane->normal[1] = (float)((long double)second[2] * first[0] - (long double)second[0] * first[2]);
            activePlane->normal[2] = (float)((long double)second[0] * first[1] - (long double)second[1] * first[0]);
            VectorNormalizeFast(activePlane->normal);
            activePlane->distance = (float)(((long double)rendererDpvsViewOrigin[2] * activePlane->normal[2] +
                                             (long double)rendererDpvsViewOrigin[1] * activePlane->normal[1]) +
                                            (long double)rendererDpvsViewOrigin[0] * activePlane->normal[0]);
            R_SetPlaneSidesDPVS(activePlane);
            rendererDpvsActivePlaneCount = (int32_t)((uint32_t)rendererDpvsActivePlaneCount + 1u);
        }

        occluder->activePlaneCount = (int32_t)((uint32_t)rendererDpvsActivePlaneCount - (uint32_t)occluder->activePlaneCount);
    }
}

/* Source: CoDUOMP.exe 0x00511280..0x00511386.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00511280_00511387.mcode.
 * Name and three-argument source signature: exact same-module Mac static
 * symbol R_XModelWorldBounds. The largest squared entity-axis length supplies
 * the uniform conservative scale applied to the first DObj model's authored
 * bounds. Ordered comparisons deliberately retain the earlier axis when a
 * candidate is unordered. */
static void R_XModelWorldBounds(const trRefEntity_t *entity, vec3_t mins, vec3_t maxs)
{
    float axisLengthSquared[3];

    for (int32_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
        const vec3_t *axis = &entity->e.axis[axisIndex];
        axisLengthSquared[axisIndex] =
            (float)(((long double)(*axis)[0] * (*axis)[0] + (long double)(*axis)[1] * (*axis)[1]) + (long double)(*axis)[2] * (*axis)[2]);
    }

    float maximumAxisLengthSquared = axisLengthSquared[0];
    if (maximumAxisLengthSquared < axisLengthSquared[1])
        maximumAxisLengthSquared = axisLengthSquared[1];
    if (maximumAxisLengthSquared < axisLengthSquared[2])
        maximumAxisLengthSquared = axisLengthSquared[2];

    /* 0x00511310 keeps the x87 square root live through all six bound
     * products; there is no intervening single-precision spill. */
    const long double boundsScaleRaw = sqrtl((long double)maximumAxisLengthSquared);
    const XModelInfo *model = entity->e.dobj->models[0]->info;

    for (int32_t component = 0; component < 3; ++component) {
        mins[component] = (float)((long double)model->mins[component] * boundsScaleRaw + (long double)entity->e.origin[component]);
        maxs[component] = (float)((long double)model->maxs[component] * boundsScaleRaw + (long double)entity->e.origin[component]);
    }
}

/* Source: CoDUOMP.exe 0x00511390..0x005114ba.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00511390_005114bb.mcode.
 * Name and three-argument source signature: exact same-module Mac static
 * symbol R_BModelWorldBounds. Each of the brush model's eight local AABB
 * corners is transformed by the renderer entity axis and origin, then merged
 * into the world-space result. */
static void R_BModelWorldBounds(const trRefEntity_t *entity, vec3_t mins, vec3_t maxs)
{
    /* 0x005113a7 MOV EAX,0x48800000 (+262144.0f) inits mins; 0x005113b6
     * MOV EAX,0xc8800000 (-262144.0f) inits maxs. A prior pass used 65536.0f
     * (0x47800000), a sentinel too small for a world that reaches +/-131072, so
     * a brush model lying entirely beyond +/-65536 fails to displace it and gets
     * inflated load-time cull bounds. */
    static const float boundsInitialExtent = 262144.0f;
    const bmodel_t *model = tr.models[entity->e.hModel]->bmodel;

    for (int32_t component = 0; component < 3; ++component) {
        mins[component] = boundsInitialExtent;
        maxs[component] = -boundsInitialExtent;
    }

    for (int32_t cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
        vec3_t localPoint;
        vec3_t worldPoint;

        localPoint[0] = (cornerIndex & 1) != 0 ? model->bounds[1][0] : model->bounds[0][0];
        localPoint[1] = (cornerIndex & 2) != 0 ? model->bounds[1][1] : model->bounds[0][1];
        localPoint[2] = (cornerIndex & 4) != 0 ? model->bounds[1][2] : model->bounds[0][2];

        for (int32_t component = 0; component < 3; ++component) {
            float transformed =
                (float)((long double)localPoint[0] * entity->e.axis[0][component] + (long double)entity->e.origin[component]);
            transformed = (float)((long double)localPoint[1] * entity->e.axis[1][component] + (long double)transformed);
            worldPoint[component] = (float)((long double)localPoint[2] * entity->e.axis[2][component] + (long double)transformed);
        }
        AddPointToBounds(worldPoint, mins, maxs);
    }
}

/* Source: CoDUOMP.exe 0x005116b0..0x005116f4, plus two inlined copies in
 * R_FilterModelsIntoCells at
 * 0x0051194c..0x005119ab and 0x00511b84..0x00511bbd. A model is huge when at
 * least two world-bounds dimensions are strictly wider than 1536 units;
 * unordered extents do not count. The original caller applies it only to
 * XModels. Name: exact same-module Mac static symbol R_XModelIsHuge. */
static qboolean R_XModelIsHuge(const vec3_t mins, const vec3_t maxs)
{
    static const float hugeModelExtent = 1536.0f; /* 0x44c00000 */
    enum {
        R_HUGE_MODEL_REQUIRED_AXES = 2
    };
    int32_t hugeAxisCount = 0;

    for (int32_t component = 0; component < 3; ++component) {
        if ((long double)maxs[component] - mins[component] > hugeModelExtent)
            ++hugeAxisCount;
    }
    return hugeAxisCount >= R_HUGE_MODEL_REQUIRED_AXES ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x005114c0..0x00511550.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005114c0_00511551.mcode.
 * Name and source role: exact same-module Mac static symbol
 * R_AddModelToCell. This is the per-view entity linker, distinct from the
 * load-time static-model helper that had the same static source name in a
 * different translation unit. Repeated visits merge bounds into the existing
 * link; otherwise one record is prepended from the caller-owned scratch bank. */
static void R_AddModelToCell(trRefEntity_t *entity, const vec3_t mins, const vec3_t maxs, renderer_world_cell_t *cell)
{
    /* 0x005114c6: the overflow guard dominates the dedup search -- the DLL tests
     * the link count against the max (CMP ECX,0x1000; JNE 0x5114ea) BEFORE walking
     * cell->entityLinks, so a full table errors even when this (entity,cell) is
     * already linked. A prior pass ran the dedup loop first. The .rdata literal at
     * 0x5b77b0 begins with the color code "^1" (0x5e 0x31), not a \x15 control byte. */
    if (rendererDpvsCellEntityLinkCount == R_DPVS_MAX_CELL_ENTITY_LINKS) {
        ri.Error(ERR_DROP, "^1Max xmodel refs (%i) exceeded\n", rendererDpvsCellEntityLinkCount);
        return;
    }

    for (renderer_cell_entity_link_t *link = cell->entityLinks; link != NULL; link = link->next) {
        if (link->entity == entity) {
            ExpandBounds(mins, maxs, link->mins, link->maxs);
            return;
        }
    }

    renderer_cell_entity_link_t *link = &rendererDpvsCellEntityLinks[rendererDpvsCellEntityLinkCount++];
    link->entity = entity;
    memcpy(link->mins, mins, sizeof(link->mins));
    memcpy(link->maxs, maxs, sizeof(link->maxs));
    link->next = cell->entityLinks;
    cell->entityLinks = link;
}

enum renderer_model_filter_side_e {
    R_MODEL_FILTER_SIDE_FRONT = 1,
    R_MODEL_FILTER_SIDE_BACK = 2,
    R_MODEL_FILTER_SIDE_CROSS = 3,
    R_MODEL_FILTER_AXIAL_PLANE_COUNT = 3
};

/* Source: CoDUOMP.exe 0x00511560..0x005116ad.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00511560_005116ae.mcode.
 * Name, return value, and recursive structure: exact same-module Mac static
 * symbol R_FilterModelIntoCells_r. The return is the common terminal cell
 * index when every visited branch agrees; differing leaves collapse to the
 * internal-node sentinel. The no-cell sentinel therefore identifies a model
 * wholly outside authored cells, which the caller must leave unculled. */
static int32_t R_FilterModelIntoCells_r(const mnode_t *node, trRefEntity_t *entity, const vec3_t mins, const vec3_t maxs)
{
    while (node->cellIndex == R_WORLD_NODE_INTERNAL) {
        const int32_t side = BoxOnPlaneSide(mins, maxs, node->data.node.plane);

        if (side != R_MODEL_FILTER_SIDE_CROSS) {
            node = node->data.node.children[side - R_MODEL_FILTER_SIDE_FRONT];
            continue;
        }

        const cplane_t *plane = node->data.node.plane;
        int32_t firstCell;
        int32_t secondCell;

        if (plane->type < R_MODEL_FILTER_AXIAL_PLANE_COUNT) {
            vec3_t frontMins = {mins[0], mins[1], mins[2]};
            vec3_t backMaxs = {maxs[0], maxs[1], maxs[2]};
            const int32_t axis = plane->type;

            frontMins[axis] = plane->dist;
            backMaxs[axis] = plane->dist;
            firstCell = R_FilterModelIntoCells_r(node->data.node.children[R_MODEL_FILTER_SIDE_BACK - 1], entity, mins, backMaxs);

            if (!(frontMins[axis] < maxs[axis]))
                return firstCell;

            secondCell = R_FilterModelIntoCells_r(node->data.node.children[R_MODEL_FILTER_SIDE_FRONT - 1], entity, frontMins, maxs);
        } else {
            firstCell = R_FilterModelIntoCells_r(node->data.node.children[R_MODEL_FILTER_SIDE_FRONT - 1], entity, mins, maxs);
            secondCell = R_FilterModelIntoCells_r(node->data.node.children[R_MODEL_FILTER_SIDE_BACK - 1], entity, mins, maxs);
        }

        return firstCell == secondCell ? firstCell : R_WORLD_NODE_INTERNAL;
    }

    if (node->cellIndex >= 0) {
        R_AddModelToCell(entity, mins, maxs, &tr.world->cells[node->cellIndex]);
    }
    return node->cellIndex;
}

/* Source: CoDUOMP.exe 0x00511700..0x00511c09.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00511700_00511c0a.mcode.
 * Name and two-argument source signature: exact same-module Mac symbol
 * R_FilterModelsIntoCells. Every cell receives a fresh transient entity-link
 * list. Enabled brush and XModel entities are first rejected against the
 * current DPVS planes, then either left globally visible when huge/outside or
 * linked through the world BSP for per-cell culling. */
void R_FilterModelsIntoCells(const renderer_dpvs_plane_t *planes, int32_t planeCount)
{
    const qboolean filterBrushModels = r_cullBModels->integer != 0 && r_drawBModels->integer != 0;
    const qboolean filterXModels = r_cullXModels->integer != 0 && r_drawXModels->integer != 0;

    for (int32_t cellIndex = 0; cellIndex < tr.world->cellCount; ++cellIndex) {
        tr.world->cells[cellIndex].entityLinks = NULL;
    }
    rendererDpvsCellEntityLinkCount = 0;

    for (int32_t entityIndex = 0; entityIndex < tr.refdef.num_entities; ++entityIndex) {
        trRefEntity_t *entity = &tr.refdef.entities[entityIndex];
        cvar_t *showCull;
        vec3_t bounds[2];

        entity->cullState = CULL_IN;
        if (entity->e.reType == RT_MODEL) {
            if (filterXModels == qfalse)
                continue;
            showCull = r_showCullXModels;
            entity->cullState = CULL_OUT;
            R_XModelWorldBounds(entity, bounds[0], bounds[1]);
        } else if (entity->e.reType == RT_BRUSH_MODEL) {
            if (filterBrushModels == qfalse)
                continue;
            showCull = r_showCullBModels;
            entity->cullState = CULL_OUT;
            R_BModelWorldBounds(entity, bounds[0], bounds[1]);
        } else {
            continue;
        }

        if (R_CullBoxDPVS(bounds, planes, planeCount, 0))
            continue;

        /* The maxs-mins vs 1536 "huge" test (R_XModelIsHuge, FCOMP ds:0x5b9b88 at
         * 0x511954/0x511964/0x511985) exists ONLY on the XModel path in the DLL; the
         * brush-model path (0x511a20..0x511b83) falls straight into
         * R_FilterModelIntoCells_r with no size test. A prior pass hoisted the shared
         * cull+filter and applied the huge check to both entity types -- the
         * short-circuit here keeps it XModel-only (brush models are never marked huge). */
        if (entity->e.reType == RT_MODEL && R_XModelIsHuge(bounds[0], bounds[1]) != qfalse) {
            entity->cullState = CULL_IN;
        } else if (R_FilterModelIntoCells_r(tr.world->nodes, entity, bounds[0], bounds[1]) == R_WORLD_NODE_NO_CELL) {
            entity->cullState = CULL_IN;
        }

        if (showCull->integer == R_SHOW_CULL_MODE_BOUNDS)
            R_AddDebugBox(bounds[0], bounds[1], modelBoundsDebugColor);
    }
}

/* Source: CoDUOMP.exe 0x005110e0..0x0051127d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005110e0_0051127e.mcode.
 * Name and three-argument source signature: exact same-module Mac symbol
 * R_CullModels. MSVC inlines R_CullBoxDPVS for each linked entity; retaining
 * the recovered source helper preserves its ordered frustum, near/far, and
 * active-occluder tests while exposing the actual per-cell operation. An
 * entity already admitted by another visible cell is never tested again. */
void R_CullModels(renderer_world_cell_t *cell, const renderer_dpvs_plane_t *planes, int32_t planeCount)
{
    for (renderer_cell_entity_link_t *link = cell->entityLinks; link != NULL; link = link->next) {
        if (link->entity->cullState == CULL_IN)
            continue;

        if (R_CullBoxDPVS(&link->mins, planes, planeCount, 0) == qfalse)
            link->entity->cullState = CULL_IN;
    }
}

/* Source: CoDUOMP.exe 0x00510d90..0x005110dd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00510d90_005110de.mcode.
 * Name and five-argument recursive signature: exact same-module Mac symbol
 * R_AddAABBTreeSurfaces_r. MSVC inlines both DPVS box classifiers. A subtree
 * wholly accepted by the strict classifier submits its complete contiguous
 * surface span without descending. A clipped internal node recurses with the
 * next plane position; a clipped leaf retains per-surface culling. */
void R_AddAABBTreeSurfaces_r(renderer_aabb_tree_t *tree, const renderer_dpvs_plane_t *planes, int32_t planeCount, uint32_t dlightBits,
                             int32_t planeIndex)
{
    const vec3_t *bounds = &tree->mins;

    if (R_CullBoxDPVS(bounds, planes, planeCount, planeIndex))
        return;

    if (dlightBits != 0) {
        dlightBits = R_CullDlightsForBox(tree->mins, tree->maxs, dlightBits);
    }

    if (R_CullBoxDPVSStrict(bounds, planes, planeCount, planeIndex) == qfalse) {
        if (r_showaabbtrees->integer != 0) {
            R_AddDebugBox(tree->mins, tree->maxs, acceptedTreeDebugColor);
        }
        for (int32_t surfaceIndex = 0; surfaceIndex < tree->surfaceCount; ++surfaceIndex) {
            msurface_t *surface = &tree->surfaces[surfaceIndex];
            if (surface->viewCount != tr.viewCount)
                R_AddWorldSurfaceNoCull(surface, dlightBits);
        }
        return;
    }

    const int32_t nextPlaneIndex = planeIndex + 1;
    if (tree->childCount != 0) {
        for (int32_t childIndex = 0; childIndex < tree->childCount; ++childIndex) {
            R_AddAABBTreeSurfaces_r(&tree->children[childIndex], planes, planeCount, dlightBits, nextPlaneIndex);
        }
        return;
    }

    if (r_showaabbtrees->integer != 0) {
        R_AddDebugBox(tree->mins, tree->maxs, clippedLeafDebugColor);
    }
    for (int32_t surfaceIndex = 0; surfaceIndex < tree->surfaceCount; ++surfaceIndex) {
        R_AddTrianglesSurface(&tree->surfaces[surfaceIndex], dlightBits, planes, planeCount, nextPlaneIndex);
    }
}

/* Source: CoDUOMP.exe 0x005127b0..0x00512a71.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005127b0_00512a72.mcode.
 * Name and top-level traversal structure: exact same-module Mac symbol
 * R_AddWorldSurfacesDPVS. The original i386 frame owns the three temporary
 * arrays below; native-width source builds deliberately let pointer-bearing
 * entries widen. Valid camera cells use portal recursion unless r_singlecell
 * requests only the current cell. A camera outside the BSP instead selects
 * the two proved outsideMapEnts traversal modes. */
void R_AddWorldSurfacesDPVS(void)
{
    renderer_occluder_t *activeOccluders[R_DPVS_MAX_ACTIVE_OCCLUDERS];
    renderer_cell_entity_link_t cellEntityLinks[R_DPVS_MAX_CELL_ENTITY_LINKS];
    renderer_dpvs_plane_t activeOccluderPlanes[R_DPVS_MAX_ACTIVE_OCCLUDER_PLANES];

    if (r_drawworld->integer == 0 || (tr.refdef.rdflags & RDF_NOWORLDMODEL) != 0) {
        return;
    }

    rendererDpvsOccluders = activeOccluders;
    rendererDpvsCellEntityLinks = cellEntityLinks;
    rendererDpvsActivePlanes = activeOccluderPlanes;
    rendererDpvsOccluderCount = 0;
    rendererDpvsCellEntityLinkCount = 0;

    tr.currentEntityNumber = R_WORLD_ENTITY_NUMBER;
    tr.shiftedEntityNumber = (uint32_t)R_WORLD_ENTITY_NUMBER << R_SORT_ENTITY_SHIFT;

    /* SHL EBP,CL masks the i386 shift count to five bits. Preserve that
     * behavior explicitly when the scene reaches all 32 dlight slots. */
    const uint32_t dlightBits = ((uint32_t)1 << ((uint32_t)tr.refdef.num_dlights & (R_MAX_DLIGHTS - 1U))) - 1U;

    R_SetupDPVS();
    R_FilterModelsIntoCells(rendererDpvsFrustumPlanes, R_FRUSTUM_PLANE_COUNT);
    R_AddCoronas(rendererDpvsFrustumPlanes, R_FRUSTUM_PLANE_COUNT);

    const int32_t cameraCellIndex = R_CellForCamera();
    if (cameraCellIndex >= 0) {
        renderer_world_cell_t *cell = &tr.world->cells[cameraCellIndex];
        if (r_singlecell->integer != 0) {
            rendererDpvsActiveFarPlane = NULL;
            R_AddCellSurfaces(cell, rendererDpvsFrustumPlanes, R_FRUSTUM_PLANE_COUNT, dlightBits);
            R_AddCellCullGroups(cell, rendererDpvsFrustumPlanes, R_FRUSTUM_PLANE_COUNT, dlightBits);
        } else {
            R_RecursivePortalWalk(cell, &rendererDpvsNearPlane, rendererDpvsFrustumPlanes, R_FRUSTUM_PLANE_COUNT, dlightBits);
        }
    } else if (outsideMapEnts->integer != 0) {
        for (int32_t cellIndex = 0; cellIndex < tr.world->cellCount; ++cellIndex) {
            renderer_world_cell_t *cell = &tr.world->cells[cellIndex];
            R_AddCellSurfaces(cell, rendererDpvsFrustumPlanes, R_FRUSTUM_PLANE_COUNT, dlightBits);
            R_AddCellCullGroups(cell, rendererDpvsFrustumPlanes, R_FRUSTUM_PLANE_COUNT, dlightBits);
        }
    } else {
        for (int32_t cellIndex = 0; cellIndex < tr.world->cellCount; ++cellIndex) {
            R_AddCellSurfaces(&tr.world->cells[cellIndex], rendererDpvsFrustumPlanes, R_FRUSTUM_PLANE_COUNT, dlightBits);
        }
        for (int32_t groupIndex = 0; groupIndex < tr.world->cullGroupCount; ++groupIndex) {
            R_AddCullGroupDPVS(&tr.world->cullGroups[groupIndex], rendererDpvsFrustumPlanes, R_FRUSTUM_PLANE_COUNT, dlightBits);
        }
    }

    R_AddSkySurfacesDPVS();

    if (r_showCullXModels->integer != R_SHOW_CULL_MODE_CELL_LINKS && r_showCullBModels->integer != R_SHOW_CULL_MODE_CELL_LINKS) {
        return;
    }

    for (int32_t cellIndex = 0; cellIndex < tr.world->cellCount; ++cellIndex) {
        for (renderer_cell_entity_link_t *link = tr.world->cells[cellIndex].entityLinks; link != NULL; link = link->next) {
            trRefEntity_t *entity = link->entity;
            if (entity->e.reType == RT_MODEL && r_showCullXModels->integer != R_SHOW_CULL_MODE_CELL_LINKS) {
                continue;
            }
            if (entity->e.reType == RT_BRUSH_MODEL && r_showCullBModels->integer != R_SHOW_CULL_MODE_CELL_LINKS) {
                continue;
            }

            R_AddDebugBox(link->mins, link->maxs, entity->cullState != 0 ? culledEntityDebugColor : visibleEntityDebugColor);
        }
    }
}

/* Source: CoDUOMP.exe 0x0050f2c0..0x00510376.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050f2c0_00510377.mcode.
 * Name, arguments, and four-phase source structure: exact same-module Mac
 * symbol R_PortalClipPlanesInternal. Portal vertices first become normalized
 * view-origin rays and adjacent-ray normals. When r_portalbevels is positive,
 * their projected screen rectangle supplies up to four additional bevel
 * planes, rejected when any portal edge normal exceeds the configured dot
 * threshold. Every surviving normal becomes a biased plane through the view
 * origin in one temporary allocation returned to the caller. */
renderer_dpvs_plane_t *R_PortalClipPlanesInternal(vec3_t *vertices, int32_t *planeCount, int32_t vertexCount)
{
    enum {
        R_PORTAL_MAX_PLANE_VERTICES = 1024,
        R_PORTAL_BEVEL_COUNT = 4
    };
    static const float projectionMinimum = 1.0f;
    static const float projectionMaximum = -1.0f;
    vec3_t bevelCorners[R_PORTAL_BEVEL_COUNT + 1];
    vec3_t relativeVertices[R_PORTAL_MAX_PLANE_VERTICES + 1];
    vec3_t edgeNormals[R_PORTAL_MAX_PLANE_VERTICES];
    const qboolean allocateBevels = r_portalbevels->value > 0.0f;
    const uint32_t allocationPlaneCount = (uint32_t)vertexCount + (allocateBevels ? R_PORTAL_BEVEL_COUNT : 0u);
    const uint32_t allocationBytes = allocationPlaneCount * (uint32_t)sizeof(renderer_dpvs_plane_t);
    renderer_dpvs_plane_t *outputPlanes = ri.Hunk_AllocateTempMemory((size_t)allocationBytes);

    if (allocateBevels) {
        float minimumX = projectionMinimum;
        float minimumY = projectionMinimum;
        float maximumX = projectionMaximum;
        float maximumY = projectionMaximum;
        const int32_t unrolledVertexCount = vertexCount >= 4 ? vertexCount & ~3 : 0;

        /* 0x0050f340..0x0050f725 projects groups of four as x+z+y,
         * z+y+x, z+y+x, and y+x+z. The same lane order is used for clip W
         * and the projected X/Y numerators. */
        for (int32_t vertexIndex = 0; vertexIndex < unrolledVertexCount; vertexIndex += 4) {
            {
                const vec3_t *vertex = &vertices[vertexIndex];
                const float clipW = (float)((((long double)rendererDpvsWorldViewProjectionMatrix[3][0] * (*vertex)[0] +
                                              (long double)rendererDpvsWorldViewProjectionMatrix[3][2] * (*vertex)[2]) +
                                             (long double)rendererDpvsWorldViewProjectionMatrix[3][1] * (*vertex)[1]) +
                                            (long double)rendererDpvsWorldViewProjectionMatrix[3][3]);

                if (!(clipW > 0.0f)) {
                    const float inverseW = (float)(1.0L / (long double)clipW);
                    const float projectedX = (float)(((((long double)rendererDpvsWorldViewProjectionMatrix[0][0] * (*vertex)[0] +
                                                        (long double)rendererDpvsWorldViewProjectionMatrix[0][2] * (*vertex)[2]) +
                                                       (long double)rendererDpvsWorldViewProjectionMatrix[0][1] * (*vertex)[1]) +
                                                      (long double)rendererDpvsWorldViewProjectionMatrix[0][3]) *
                                                     (long double)inverseW);
                    const float projectedY = (float)(((((long double)rendererDpvsWorldViewProjectionMatrix[1][0] * (*vertex)[0] +
                                                        (long double)rendererDpvsWorldViewProjectionMatrix[1][2] * (*vertex)[2]) +
                                                       (long double)rendererDpvsWorldViewProjectionMatrix[1][1] * (*vertex)[1]) +
                                                      (long double)rendererDpvsWorldViewProjectionMatrix[1][3]) *
                                                     (long double)inverseW);

                    if (minimumX > projectedX)
                        minimumX = projectedX;
                    if (maximumX < projectedX)
                        maximumX = projectedX;
                    if (minimumY > projectedY)
                        minimumY = projectedY;
                    if (maximumY < projectedY)
                        maximumY = projectedY;
                }
            }

            {
                const vec3_t *vertex = &vertices[vertexIndex + 1];
                const float clipW = (float)((((long double)rendererDpvsWorldViewProjectionMatrix[3][2] * (*vertex)[2] +
                                              (long double)rendererDpvsWorldViewProjectionMatrix[3][1] * (*vertex)[1]) +
                                             (long double)rendererDpvsWorldViewProjectionMatrix[3][0] * (*vertex)[0]) +
                                            (long double)rendererDpvsWorldViewProjectionMatrix[3][3]);

                if (!(clipW > 0.0f)) {
                    const float inverseW = (float)(1.0L / (long double)clipW);
                    const float projectedX = (float)(((((long double)rendererDpvsWorldViewProjectionMatrix[0][2] * (*vertex)[2] +
                                                        (long double)rendererDpvsWorldViewProjectionMatrix[0][1] * (*vertex)[1]) +
                                                       (long double)rendererDpvsWorldViewProjectionMatrix[0][0] * (*vertex)[0]) +
                                                      (long double)rendererDpvsWorldViewProjectionMatrix[0][3]) *
                                                     (long double)inverseW);
                    const float projectedY = (float)(((((long double)rendererDpvsWorldViewProjectionMatrix[1][2] * (*vertex)[2] +
                                                        (long double)rendererDpvsWorldViewProjectionMatrix[1][1] * (*vertex)[1]) +
                                                       (long double)rendererDpvsWorldViewProjectionMatrix[1][0] * (*vertex)[0]) +
                                                      (long double)rendererDpvsWorldViewProjectionMatrix[1][3]) *
                                                     (long double)inverseW);

                    if (minimumX > projectedX)
                        minimumX = projectedX;
                    if (maximumX < projectedX)
                        maximumX = projectedX;
                    if (minimumY > projectedY)
                        minimumY = projectedY;
                    if (maximumY < projectedY)
                        maximumY = projectedY;
                }
            }

            {
                const vec3_t *vertex = &vertices[vertexIndex + 2];
                const float clipW = (float)((((long double)rendererDpvsWorldViewProjectionMatrix[3][2] * (*vertex)[2] +
                                              (long double)rendererDpvsWorldViewProjectionMatrix[3][1] * (*vertex)[1]) +
                                             (long double)rendererDpvsWorldViewProjectionMatrix[3][0] * (*vertex)[0]) +
                                            (long double)rendererDpvsWorldViewProjectionMatrix[3][3]);

                if (!(clipW > 0.0f)) {
                    const float inverseW = (float)(1.0L / (long double)clipW);
                    const float projectedX = (float)(((((long double)rendererDpvsWorldViewProjectionMatrix[0][2] * (*vertex)[2] +
                                                        (long double)rendererDpvsWorldViewProjectionMatrix[0][1] * (*vertex)[1]) +
                                                       (long double)rendererDpvsWorldViewProjectionMatrix[0][0] * (*vertex)[0]) +
                                                      (long double)rendererDpvsWorldViewProjectionMatrix[0][3]) *
                                                     (long double)inverseW);
                    const float projectedY = (float)(((((long double)rendererDpvsWorldViewProjectionMatrix[1][2] * (*vertex)[2] +
                                                        (long double)rendererDpvsWorldViewProjectionMatrix[1][1] * (*vertex)[1]) +
                                                       (long double)rendererDpvsWorldViewProjectionMatrix[1][0] * (*vertex)[0]) +
                                                      (long double)rendererDpvsWorldViewProjectionMatrix[1][3]) *
                                                     (long double)inverseW);

                    if (minimumX > projectedX)
                        minimumX = projectedX;
                    if (maximumX < projectedX)
                        maximumX = projectedX;
                    if (minimumY > projectedY)
                        minimumY = projectedY;
                    if (maximumY < projectedY)
                        maximumY = projectedY;
                }
            }

            {
                const vec3_t *vertex = &vertices[vertexIndex + 3];
                const float clipW = (float)((((long double)rendererDpvsWorldViewProjectionMatrix[3][1] * (*vertex)[1] +
                                              (long double)rendererDpvsWorldViewProjectionMatrix[3][0] * (*vertex)[0]) +
                                             (long double)rendererDpvsWorldViewProjectionMatrix[3][2] * (*vertex)[2]) +
                                            (long double)rendererDpvsWorldViewProjectionMatrix[3][3]);

                if (!(clipW > 0.0f)) {
                    const float inverseW = (float)(1.0L / (long double)clipW);
                    const float projectedX = (float)(((((long double)rendererDpvsWorldViewProjectionMatrix[0][1] * (*vertex)[1] +
                                                        (long double)rendererDpvsWorldViewProjectionMatrix[0][0] * (*vertex)[0]) +
                                                       (long double)rendererDpvsWorldViewProjectionMatrix[0][2] * (*vertex)[2]) +
                                                      (long double)rendererDpvsWorldViewProjectionMatrix[0][3]) *
                                                     (long double)inverseW);
                    const float projectedY = (float)(((((long double)rendererDpvsWorldViewProjectionMatrix[1][1] * (*vertex)[1] +
                                                        (long double)rendererDpvsWorldViewProjectionMatrix[1][0] * (*vertex)[0]) +
                                                       (long double)rendererDpvsWorldViewProjectionMatrix[1][2] * (*vertex)[2]) +
                                                      (long double)rendererDpvsWorldViewProjectionMatrix[1][3]) *
                                                     (long double)inverseW);

                    if (minimumX > projectedX)
                        minimumX = projectedX;
                    if (maximumX < projectedX)
                        maximumX = projectedX;
                    if (minimumY > projectedY)
                        minimumY = projectedY;
                    if (maximumY < projectedY)
                        maximumY = projectedY;
                }
            }
        }

        /* 0x0050f750..0x0050f847 projects each scalar-tail vertex as
         * z+x+y for clip W and both projected numerators. */
        for (int32_t vertexIndex = unrolledVertexCount; vertexIndex < vertexCount; ++vertexIndex) {
            const vec3_t *vertex = &vertices[vertexIndex];
            const float clipW = (float)((((long double)rendererDpvsWorldViewProjectionMatrix[3][2] * (*vertex)[2] +
                                          (long double)rendererDpvsWorldViewProjectionMatrix[3][0] * (*vertex)[0]) +
                                         (long double)rendererDpvsWorldViewProjectionMatrix[3][1] * (*vertex)[1]) +
                                        (long double)rendererDpvsWorldViewProjectionMatrix[3][3]);

            if (!(clipW > 0.0f)) {
                const float inverseW = (float)(1.0L / (long double)clipW);
                const float projectedX = (float)(((((long double)rendererDpvsWorldViewProjectionMatrix[0][2] * (*vertex)[2] +
                                                    (long double)rendererDpvsWorldViewProjectionMatrix[0][0] * (*vertex)[0]) +
                                                   (long double)rendererDpvsWorldViewProjectionMatrix[0][1] * (*vertex)[1]) +
                                                  (long double)rendererDpvsWorldViewProjectionMatrix[0][3]) *
                                                 (long double)inverseW);
                const float projectedY = (float)(((((long double)rendererDpvsWorldViewProjectionMatrix[1][2] * (*vertex)[2] +
                                                    (long double)rendererDpvsWorldViewProjectionMatrix[1][0] * (*vertex)[0]) +
                                                   (long double)rendererDpvsWorldViewProjectionMatrix[1][1] * (*vertex)[1]) +
                                                  (long double)rendererDpvsWorldViewProjectionMatrix[1][3]) *
                                                 (long double)inverseW);

                if (minimumX > projectedX)
                    minimumX = projectedX;
                if (maximumX < projectedX)
                    maximumX = projectedX;
                if (minimumY > projectedY)
                    minimumY = projectedY;
                if (maximumY < projectedY)
                    maximumY = projectedY;
            }
        }

        for (int32_t cornerIndex = 0; cornerIndex < R_PORTAL_BEVEL_COUNT; ++cornerIndex) {
            const float projectedX = cornerIndex < 2 ? minimumX : maximumX;
            const float projectedY = cornerIndex == 1 || cornerIndex == 2 ? minimumY : maximumY;
            float homogeneous[4];

            for (int32_t component = 0; component < 4; ++component) {
                homogeneous[component] = (float)(((long double)rendererDpvsInverseWorldViewProjectionMatrix[1][component] * projectedY +
                                                  (long double)rendererDpvsInverseWorldViewProjectionMatrix[0][component] * projectedX) +
                                                 (long double)rendererDpvsInverseWorldViewProjectionMatrix[3][component]);
            }
            const float inverseW = (float)(1.0L / (long double)homogeneous[3]);
            bevelCorners[cornerIndex][0] = (float)((long double)homogeneous[0] * inverseW);
            bevelCorners[cornerIndex][1] = (float)((long double)homogeneous[1] * inverseW);
            bevelCorners[cornerIndex][2] = (float)((long double)homogeneous[2] * inverseW);
        }
        memcpy(bevelCorners[R_PORTAL_BEVEL_COUNT], bevelCorners[0], sizeof(vec3_t));
    }

    for (int32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        relativeVertices[vertexIndex][0] = vertices[vertexIndex][0] - rendererDpvsViewOrigin[0];
        relativeVertices[vertexIndex][1] = vertices[vertexIndex][1] - rendererDpvsViewOrigin[1];
        relativeVertices[vertexIndex][2] = vertices[vertexIndex][2] - rendererDpvsViewOrigin[2];
    }
    memcpy(relativeVertices[vertexCount], relativeVertices[0], sizeof(vec3_t));

    for (int32_t edgeIndex = 0; edgeIndex < vertexCount; ++edgeIndex) {
        const vec3_t *current = &relativeVertices[edgeIndex];
        const vec3_t *next = &relativeVertices[edgeIndex + 1];
        edgeNormals[edgeIndex][0] = (float)((long double)(*next)[1] * (*current)[2] - (long double)(*next)[2] * (*current)[1]);
        edgeNormals[edgeIndex][1] = (float)((long double)(*next)[2] * (*current)[0] - (long double)(*next)[0] * (*current)[2]);
        edgeNormals[edgeIndex][2] = (float)((long double)(*next)[0] * (*current)[1] - (long double)(*next)[1] * (*current)[0]);
        VectorNormalizeFast(edgeNormals[edgeIndex]);
    }

    int32_t outputPlaneCount = 0;
    /* 0x0050fc3c reloads the cvar after vertex-normal construction. */
    if (r_portalbevels->value > 0.0f) {
        vec3_t relativeBevelCorners[R_PORTAL_BEVEL_COUNT + 1];
        for (int32_t cornerIndex = 0; cornerIndex <= R_PORTAL_BEVEL_COUNT; ++cornerIndex) {
            relativeBevelCorners[cornerIndex][0] = bevelCorners[cornerIndex][0] - rendererDpvsViewOrigin[0];
            relativeBevelCorners[cornerIndex][1] = bevelCorners[cornerIndex][1] - rendererDpvsViewOrigin[1];
            relativeBevelCorners[cornerIndex][2] = bevelCorners[cornerIndex][2] - rendererDpvsViewOrigin[2];
        }

        for (int32_t bevelIndex = 0; bevelIndex < R_PORTAL_BEVEL_COUNT; ++bevelIndex) {
            const vec3_t *current = &relativeBevelCorners[bevelIndex];
            const vec3_t *next = &relativeBevelCorners[bevelIndex + 1];
            vec3_t bevelNormal;

            bevelNormal[0] = (float)((long double)(*next)[1] * (*current)[2] - (long double)(*next)[2] * (*current)[1]);
            bevelNormal[1] = (float)((long double)(*next)[2] * (*current)[0] - (long double)(*next)[0] * (*current)[2]);
            bevelNormal[2] = (float)((long double)(*next)[0] * (*current)[1] - (long double)(*next)[1] * (*current)[0]);
            VectorNormalizeFast(bevelNormal);

            qboolean rejected = qfalse;
            for (int32_t edgeIndex = 0; edgeIndex < vertexCount; ++edgeIndex) {
                const long double dot =
                    ((long double)edgeNormals[edgeIndex][0] * bevelNormal[0] + (long double)edgeNormals[edgeIndex][2] * bevelNormal[2]) +
                    (long double)edgeNormals[edgeIndex][1] * bevelNormal[1];
                if (dot > r_portalbevels->value) {
                    rejected = qtrue;
                    break;
                }
            }

            if (rejected) {
                if ((r_showportals->integer & 2) != 0) {
                    R_AddDebugLine(bevelCorners[bevelIndex], bevelCorners[bevelIndex + 1], rejectedBevelDebugColor);
                }
                continue;
            }

            if (r_showportals->integer != 0) {
                R_AddDebugLine(bevelCorners[bevelIndex], bevelCorners[bevelIndex + 1], acceptedBevelDebugColor);
            }

            renderer_dpvs_plane_t *plane = &outputPlanes[outputPlaneCount++];
            memcpy(plane->normal, bevelNormal, sizeof(plane->normal));
            plane->distance = (float)(((long double)rendererDpvsViewOrigin[2] * plane->normal[2] +
                                       (long double)rendererDpvsViewOrigin[1] * plane->normal[1]) +
                                      (long double)rendererDpvsViewOrigin[0] * plane->normal[0]);
            R_SetPlaneSidesDPVS(plane);
        }
    }

    for (int32_t edgeIndex = 0; edgeIndex < vertexCount; ++edgeIndex) {
        const int32_t unrolledEdgeCount = vertexCount & ~3;
        long double lengthSquared;
        if (edgeIndex < unrolledEdgeCount && (edgeIndex & 3) == 0) {
            /* First unrolled lane at 0x0050feec..0x0050ff11: z+x+y. */
            lengthSquared = ((long double)edgeNormals[edgeIndex][2] * edgeNormals[edgeIndex][2] +
                             (long double)edgeNormals[edgeIndex][0] * edgeNormals[edgeIndex][0]) +
                            (long double)edgeNormals[edgeIndex][1] * edgeNormals[edgeIndex][1];
        } else {
            /* Remaining unrolled lanes (0x0050ffbd, 0x0051008a,
             * 0x0051015d) and the scalar tail (0x00510249): z+y+x. */
            lengthSquared = ((long double)edgeNormals[edgeIndex][2] * edgeNormals[edgeIndex][2] +
                             (long double)edgeNormals[edgeIndex][1] * edgeNormals[edgeIndex][1]) +
                            (long double)edgeNormals[edgeIndex][0] * edgeNormals[edgeIndex][0];
        }
        /* 0x0050ff13..0x0050ff20 skips ordered equality with zero;
         * unordered falls through and is retained. */
        if (!(lengthSquared != 0.0f))
            continue;

        renderer_dpvs_plane_t *plane = &outputPlanes[outputPlaneCount++];
        memcpy(plane->normal, edgeNormals[edgeIndex], sizeof(plane->normal));
        if (edgeIndex < unrolledEdgeCount) {
            if ((edgeIndex & 3) == 0) {
                /* First lane at 0x0050ff42..0x0050ff64: z+x+y. */
                plane->distance = (float)(((long double)rendererDpvsViewOrigin[2] * plane->normal[2] +
                                           (long double)rendererDpvsViewOrigin[0] * plane->normal[0]) +
                                          (long double)rendererDpvsViewOrigin[1] * plane->normal[1]);
            } else {
                /* Remaining unrolled lanes: z+y+x. */
                plane->distance = (float)(((long double)rendererDpvsViewOrigin[2] * plane->normal[2] +
                                           (long double)rendererDpvsViewOrigin[1] * plane->normal[1]) +
                                          (long double)rendererDpvsViewOrigin[0] * plane->normal[0]);
            }
        } else {
            /* Scalar tail at 0x00510295..0x005102c1: y+z+x. */
            plane->distance = (float)(((long double)rendererDpvsViewOrigin[1] * plane->normal[1] +
                                       (long double)rendererDpvsViewOrigin[2] * plane->normal[2]) +
                                      (long double)rendererDpvsViewOrigin[0] * plane->normal[0]);
        }
        R_SetPlaneSidesDPVS(plane);
    }

    *planeCount = outputPlaneCount;
    return outputPlanes;
}
