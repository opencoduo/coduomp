#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */

#include "../core_math/core_math_private.h"
#include "../core_memory/core_memory_private.h"
#include "../core_runtime/core_runtime_private.h"
#include "server/standalone/bindings/coduo_engine_structs.h"
#include "cm_collision_globals_private.h"
#include "cm_world_sector_private.h"

int32_t CM_LittleShort(int16_t value)
{
    return value;
}

uint32_t CM_LittleLong(uint32_t value)
{
    return value;
}

long double CM_LittleFloat(float value)
{
    return value;
}

#ifdef CODUO_COLLISION_DIGEST
/*
 * Curved-patch (curveCollide) parity digest — NOT part of the reconstruction.
 *
 * The PATCHPARITY dump only covers patch->terrainCollide (the triangle-soup
 * builder). Curved patches instead populate patch->curveCollide via
 * CM_GeneratePatchCollide, whose float pipeline (CM_Subdivide, CM_PlaneFromPoints,
 * CM_FindPlane2, CM_AddFacetBevels, ChopWindingInPlace, ...) is otherwise
 * unmeasured. This hashes what that pipeline actually produces: every generated
 * plane's float bits, plus each facet's plane indices/flags (which catch
 * plane-selection and border-classification divergence).
 *
 * This diagnostic remains engine-local because its external digest sink is
 * engine-only; the recovered builders themselves live in src/collision.
 */
void coduo_engine_collision_digest_bytes_external(const void *data, size_t length, uint64_t *accum);

/* NOT_FROM_ORIGINAL_SOURCE: curved-patch parity digest. */
void coduo_engine_digest_curve_collide(const void *curveCollide, uint64_t *accum, int32_t *planeCountOut, int32_t *facetCountOut)
{
    const patchCollide_t *build = curveCollide;

    *planeCountOut = build->numPlanes;
    *facetCountOut = build->numFacets;

    for (int32_t planeIndex = 0; planeIndex < build->numPlanes; ++planeIndex) {
        const patchPlane_t *plane = &build->planes[planeIndex];
        coduo_engine_collision_digest_bytes_external(plane->normal, sizeof(plane->normal), accum);
        coduo_engine_collision_digest_bytes_external(&plane->dist, sizeof(plane->dist), accum);
        coduo_engine_collision_digest_bytes_external(&plane->signbits, sizeof(plane->signbits), accum);
    }

    for (int32_t facetIndex = 0; facetIndex < build->numFacets; ++facetIndex) {
        const facet_t *facet = &build->facets[facetIndex];
        coduo_engine_collision_digest_bytes_external(&facet->surfacePlane, sizeof(facet->surfacePlane), accum);
        coduo_engine_collision_digest_bytes_external(&facet->numBorders, sizeof(facet->numBorders), accum);
        for (int32_t border = 0; border < facet->numBorders; ++border) {
            coduo_engine_collision_digest_bytes_external(&facet->borderPlanes[border], sizeof(facet->borderPlanes[border]), accum);
            coduo_engine_collision_digest_bytes_external(&facet->borderInward[border], sizeof(facet->borderInward[border]), accum);
            coduo_engine_collision_digest_bytes_external(&facet->borderNoAdjust[border], sizeof(facet->borderNoAdjust[border]), accum);
        }
    }
}
#endif
