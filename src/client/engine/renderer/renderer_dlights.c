#include "backend.h"

#include "gl_state.h"

enum {
    R_DLIGHT_BIT_INDEX_MASK = 31
};

/* Source: CoDUOMP.exe 0x004c5f20..0x004c5fe4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c5f20_004c5fe5.mcode.
 * Name and source parameter roles: same-module Mac symbol
 * R_TransformDlights. A directional light (position.w == 0) is rotated;
 * positional lights are translated by the orientation origin before the same
 * three dot products. Windows proves the 0x88 light stride and +0x78 output. */
void R_TransformDlights(int32_t lightCount, renderer_light_t *lights,
                        const orientationr_t *orientation)
{
    for (int32_t lightIndex = 0;
         lightIndex < lightCount;
         ++lightIndex) {
        renderer_light_t *light = &lights[lightIndex];

        if (light->position[3] == 0.0f) {
            /* 0x004c5f8c..0x004c5fda reloads the three binary32 position
             * components for each dot product. */
            light->transformedPosition[0] = (float)(
                ((long double)light->position[0] *
                     (long double)orientation->axis[0][0] +
                 (long double)light->position[2] *
                     (long double)orientation->axis[0][2]) +
                (long double)light->position[1] *
                    (long double)orientation->axis[0][1]);
            light->transformedPosition[1] = (float)(
                ((long double)light->position[0] *
                     (long double)orientation->axis[1][0] +
                 (long double)light->position[2] *
                     (long double)orientation->axis[1][2]) +
                (long double)light->position[1] *
                    (long double)orientation->axis[1][1]);
            light->transformedPosition[2] = (float)(
                ((long double)light->position[2] *
                     (long double)orientation->axis[2][2] +
                 (long double)light->position[1] *
                     (long double)orientation->axis[2][1]) +
                (long double)light->position[0] *
                    (long double)orientation->axis[2][0]);
        } else {
            /* 0x004c5f42..0x004c5fda retains all three subtractions in x87
             * registers through the three stores. */
            const long double localPosition[3] = {
                (long double)light->position[0] -
                    (long double)orientation->origin[0],
                (long double)light->position[1] -
                    (long double)orientation->origin[1],
                (long double)light->position[2] -
                    (long double)orientation->origin[2]
            };

            light->transformedPosition[0] = (float)(
                (localPosition[2] *
                     (long double)orientation->axis[0][2] +
                 localPosition[0] *
                     (long double)orientation->axis[0][0]) +
                localPosition[1] *
                    (long double)orientation->axis[0][1]);
            light->transformedPosition[1] = (float)(
                (localPosition[2] *
                     (long double)orientation->axis[1][2] +
                 localPosition[0] *
                     (long double)orientation->axis[1][0]) +
                localPosition[1] *
                    (long double)orientation->axis[1][1]);
            light->transformedPosition[2] = (float)(
                (localPosition[2] *
                     (long double)orientation->axis[2][2] +
                 localPosition[1] *
                     (long double)orientation->axis[2][1]) +
                localPosition[0] *
                    (long double)orientation->axis[2][0]);
        }
    }
}

/* Source: CoDUOMP.exe 0x0050eed0..0x0050ef5b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050eed0_0050ef5c.mcode.
 * Name and parameter order: same-module Mac symbol R_CullDlightsForBox. The
 * PE walks only bits present in the input mask and tests strict overlap with
 * the transformed light position and radius on all three axes. The x87
 * branches reject `high <= min` and `low >= max`; unordered comparisons take
 * neither rejection branch. */
uint32_t R_CullDlightsForBox(const vec3_t boundsMin,
                             const vec3_t boundsMax,
                             uint32_t dlightBits)
{
    uint32_t survivingBits = 0;
    uint32_t lightBit = 1;
    renderer_light_t *light = tr.refdef.dlights;

    while (dlightBits != 0) {
        if ((dlightBits & lightBit) != 0) {
            qboolean overlaps = qtrue;

            dlightBits &= ~lightBit;
            for (int32_t axis = 0; axis < 3; ++axis) {
                const long double high =
                    (long double)light->transformedPosition[axis] +
                    light->radius;
                const long double low =
                    (long double)light->transformedPosition[axis] -
                    light->radius;
                if (high <= boundsMin[axis] || low >= boundsMax[axis]) {
                    overlaps = qfalse;
                    break;
                }
            }

            if (overlaps != qfalse)
                survivingBits |= lightBit;
        }

        lightBit <<= 1;
        ++light;
    }

    return survivingBits;
}

/* Source: CoDUOMP.exe 0x0051ca30..0x0051cd06.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051ca30_0051cd07.mcode.
 * Name: same-module Mac symbol R_DlightTris. LTCG unrolls the original loop
 * four lights at a time, followed by the one-light tail. The maintained loop
 * preserves its bounded light count, input-mask test, and unordered-float
 * behavior without retaining the generated unrolling. */
uint32_t R_DlightTris(renderer_lit_surface_t *surface,
                      uint32_t dlightBits)
{
    uint32_t survivingBits = 0;

    for (int32_t lightIndex = 0;
         lightIndex < tr.refdef.num_dlights;
         ++lightIndex) {
        const uint32_t lightBit =
            1U << ((uint32_t)lightIndex & R_DLIGHT_BIT_INDEX_MASK);
        const renderer_light_t *light = &tr.refdef.dlights[lightIndex];
        qboolean overlaps = qtrue;

        if ((dlightBits & lightBit) == 0)
            continue;

        for (int32_t axis = 0; axis < 3; ++axis) {
            const long double high =
                (long double)light->transformedPosition[axis] +
                light->radius;
            const long double low =
                (long double)light->transformedPosition[axis] -
                light->radius;
            if (high <= surface->boundsMin[axis] ||
                low >= surface->boundsMax[axis]) {
                overlaps = qfalse;
                break;
            }
        }
        if (overlaps != qfalse)
            survivingBits |= lightBit;
    }

    return survivingBits;
}

/* Source: CoDUOMP.exe 0x0051cd10..0x0051cd36.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051cd10_0051cd37.mcode.
 * Name: same-module Mac symbol R_DlightSurface. Surface kinds below the
 * indexed-position range cannot retain a light mask. For lit surfaces the
 * filtered mask is stored in the surface, while the return and performance
 * counter are driven by the original nonzero input mask exactly as the PE
 * tests EDI after R_DlightTris returns. */
qboolean R_DlightSurface(msurface_t *worldSurface,
                         uint32_t dlightBits)
{
    renderer_surface_t *surface = worldSurface->data;

    if (surface->surfaceType < R_SURFACE_INDEXED_POSITION_FIRST)
        return qfalse;

    renderer_lit_surface_t *litSurface =
        (renderer_lit_surface_t *)surface;
    litSurface->dlightBits = R_DlightTris(litSurface, dlightBits);
    if (dlightBits != 0)
        ++tr.pc.dlightSurfaceCount;

    return dlightBits != 0;
}

/* Source: CoDUOMP.exe 0x004c5ff0..0x004c60b2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c5ff0_004c60b3.mcode.
 * Name and bmodel parameter role: same-module Mac symbol R_DlightBmodel.
 * The Windows comparisons reject only a proved separation greater than the
 * radius, so unordered float comparisons remain non-rejecting. The resulting
 * mask is written both to tr.currentEntity and eligible surface payloads. */
void R_DlightBmodel(bmodel_t *bmodel)
{
    uint32_t dlightBits = 0;

    R_TransformDlights(tr.refdef.num_dlights, tr.refdef.dlights,
                       &tr.orientation);

    for (int32_t lightIndex = 0;
         lightIndex < tr.refdef.num_dlights;
         ++lightIndex) {
        const renderer_light_t *light = &tr.refdef.dlights[lightIndex];
        qboolean separated = qfalse;

        for (int32_t axis = 0; axis < 3; ++axis) {
            if (light->transformedPosition[axis] - bmodel->bounds[1][axis] >
                    light->radius ||
                bmodel->bounds[0][axis] - light->transformedPosition[axis] >
                    light->radius) {
                separated = qtrue;
                break;
            }
        }

        if (separated == qfalse) {
            dlightBits |=
                1U << ((uint32_t)lightIndex & R_DLIGHT_BIT_INDEX_MASK);
        }
    }

    tr.currentEntity->dlightBits = dlightBits;
    for (int32_t surfaceIndex = 0;
         surfaceIndex < bmodel->numSurfaces;
         ++surfaceIndex) {
        renderer_surface_t *surface =
            bmodel->firstSurface[surfaceIndex].data;

        if (surface->surfaceType >= R_SURFACE_INDEXED_POSITION_FIRST) {
            renderer_lit_surface_t *litSurface =
                (renderer_lit_surface_t *)surface;
            litSurface->dlightBits = dlightBits;
        }
    }
}
