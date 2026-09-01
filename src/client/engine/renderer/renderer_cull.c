#include "backend.h"

#include "gl_state.h"
#include "renderer_cvars.h"

/* Source: CoDUOMP.exe 0x004e3300..0x004e390e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e3300_004e390e.mcode.
 * Provisional name: the Windows body performs the established id-renderer
 * R_CullLocalBox operation. LTCG unrolled all eight local bounds corners and
 * their orientation transforms before the four-plane classification. */
cull_result_t R_CullLocalBox(const vec3_t bounds[2])
{
    vec3_t points[8];
    cull_result_t result = CULL_IN;

    if (r_nocull->integer != 0)
        return CULL_CLIP;

    for (int32_t cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
        const float localX = bounds[(cornerIndex & 1) != 0][0];
        const float localY = bounds[(cornerIndex & 2) != 0][1];
        const float localZ = bounds[(cornerIndex & 4) != 0][2];

        /* 0x004e331f..0x004e3892 uses a stable x87 schedule for all eight
         * unrolled corners. X rounds after each first two additions, Y keeps
         * origin+axis0 live until axis1, and Z keeps axis1 live until axis2. */
        const float xAfterAxis0 = (float)(
            (long double)tr.orientation.origin[0] +
            (long double)tr.orientation.axis[0][0] *
                (long double)localX);
        const float xAfterAxis1 = (float)(
            (long double)xAfterAxis0 +
            (long double)tr.orientation.axis[1][0] *
                (long double)localY);
        points[cornerIndex][0] = (float)(
            (long double)xAfterAxis1 +
            (long double)tr.orientation.axis[2][0] *
                (long double)localZ);

        const float yAfterAxis1 = (float)(
            ((long double)tr.orientation.axis[0][1] *
                 (long double)localX +
             (long double)tr.orientation.origin[1]) +
            (long double)tr.orientation.axis[1][1] *
                (long double)localY);
        points[cornerIndex][1] = (float)(
            (long double)yAfterAxis1 +
            (long double)tr.orientation.axis[2][1] *
                (long double)localZ);

        const float zAfterAxis0 = (float)(
            (long double)tr.orientation.axis[0][2] *
                (long double)localX +
            (long double)tr.orientation.origin[2]);
        points[cornerIndex][2] = (float)(
            ((long double)tr.orientation.axis[1][2] *
                 (long double)localY +
             (long double)zAfterAxis0) +
            (long double)tr.orientation.axis[2][2] *
                (long double)localZ);
    }

    for (int32_t planeIndex = 0;
         planeIndex < R_FRUSTUM_PLANE_COUNT;
         ++planeIndex) {
        const renderer_frustum_plane_t *plane =
            &tr.viewParms.frustum[planeIndex];
        qboolean anyFront = qfalse;
        qboolean anyBack = qfalse;

        for (int32_t cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
            /* 0x004e389c..0x004e38ba stores the dot as float for its local
             * copy, but the plane comparison consumes the retained x87 sum. */
            const long double distanceRaw =
                ((long double)points[cornerIndex][2] *
                     (long double)plane->normal[2] +
                 (long double)points[cornerIndex][1] *
                     (long double)plane->normal[1]) +
                (long double)points[cornerIndex][0] *
                    (long double)plane->normal[0];
            const float distance = (float)distanceRaw;

            (void)distance;
            if (distanceRaw > (long double)plane->distance)
                anyFront = qtrue;
            else
                anyBack = qtrue;

            if (anyFront != qfalse && anyBack != qfalse)
                break;
        }

        if (anyFront == qfalse)
            return CULL_OUT;
        if (anyBack != qfalse)
            result = CULL_CLIP;
    }

    return result;
}

/* Source: CoDUOMP.exe 0x004e3910..0x004e392b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e3910_004e392b.mcode.
 * Provisional name: role and call pair prove the conventional renderer helper
 * R_CullLocalPointAndRadius. */
cull_result_t R_CullLocalPointAndRadius(const vec3_t point, float radius)
{
    vec3_t worldPoint;

    R_LocalPointToWorld(point, worldPoint);
    return R_CullPointAndRadius(worldPoint, radius);
}

/* Source: CoDUOMP.exe 0x004e3930..0x004e39a8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e3930_004e39a8.mcode.
 * Name and source-level argument roles: same-module Mac symbol
 * R_CullPointAndRadius. The Windows body tests the four front-end frustum
 * planes and returns the original in/clip/out classification. */
cull_result_t R_CullPointAndRadius(const vec3_t point, float radius)
{
    cull_result_t result = CULL_IN;

    if (r_nocull->integer != 0)
        return CULL_CLIP;

    for (int32_t planeIndex = 0;
         planeIndex < R_FRUSTUM_PLANE_COUNT;
         ++planeIndex) {
        const renderer_frustum_plane_t *plane =
            &tr.viewParms.frustum[planeIndex];
        /* 0x004e3957..0x004e3982 keeps the dot/distance result in x87 for
         * both radius comparisons. */
        const long double distance =
            (((long double)plane->normal[0] * (long double)point[0] +
              (long double)plane->normal[2] * (long double)point[2]) +
             (long double)plane->normal[1] * (long double)point[1]) -
            (long double)plane->distance;

        if (distance < -(long double)radius)
            return CULL_OUT;
        if (distance <= (long double)radius)
            result = CULL_CLIP;
    }

    return result;
}

/* Source: CoDUOMP.exe 0x004e39b0..0x004e3a13.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e39b0_004e3a13.mcode.
 * Name: same-module Mac symbol R_LocalNormalToWorld. */
void R_LocalNormalToWorld(const vec3_t local, vec3_t world)
{
    for (int32_t coordinate = 0; coordinate < 3; ++coordinate) {
        /* 0x004e39b0..0x004e3a0f stores once after the complete
         * (axis0*X + axis2*Z) + axis1*Y x87 graph. */
        world[coordinate] = (float)(
            ((long double)tr.orientation.axis[0][coordinate] *
                 (long double)local[0] +
             (long double)tr.orientation.axis[2][coordinate] *
                 (long double)local[2]) +
            (long double)tr.orientation.axis[1][coordinate] *
                (long double)local[1]);
    }
}

/* Source: CoDUOMP.exe 0x004e3a20..0x004e3a95.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e3a20_004e3a95.mcode.
 * Provisional name: the body is R_LocalNormalToWorld plus the orientation
 * origin, and the caller at 0x004e3910 uses it to cull a local point. */
void R_LocalPointToWorld(const vec3_t local, vec3_t world)
{
    for (int32_t coordinate = 0; coordinate < 3; ++coordinate) {
        /* 0x004e3a20..0x004e3a91 adds the origin to the complete retained
         * local-normal transform before the sole binary32 store. */
        world[coordinate] = (float)(
            (((long double)tr.orientation.axis[0][coordinate] *
                  (long double)local[0] +
              (long double)tr.orientation.axis[2][coordinate] *
                  (long double)local[2]) +
             (long double)tr.orientation.axis[1][coordinate] *
                 (long double)local[1]) +
            (long double)tr.orientation.origin[coordinate]);
    }
}
