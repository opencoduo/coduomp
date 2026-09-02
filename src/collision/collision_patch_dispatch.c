#include "collision_patch_dispatch.h"

#include "collision_patch_trace.h"
#include "compat/coduo_x87emu.h"
#include "collision_terrain_dispatch.h"
#include "collision_trace_bounds.h"

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "collision_patch_dispatch.c requires a platform behavior mode"
#endif

extern dshader_t *cm_materials;

/*
 * Complete terrain-patch dispatch pair:
 *
 *   CoDUOMP.exe  CM_TraceThroughPatch      0x004269f0..0x00426a74
 *                CM_SightTraceThroughPatch 0x00428aa0..0x00428ad1
 *   coduo_lnxded CM_TraceThroughPatch      0x08058e30..0x08058f08
 *                CM_SightTraceThroughPatch 0x0805b60b..0x0805b67b
 *
 * Both binaries use +0x24 for the curved-grid patchCollide_t and +0x28 for
 * the indexed terrain triangle soup, and install the same material metadata
 * only when the delegated trace lowers the fraction.  Windows uses signaling
 * FCOMP for that last comparison while Linux uses quiet FUCOMPP; only that
 * floating-point status difference requires platform selection.
 */
void CM_TraceThroughPatch(traceWork_t *traceWork, const collisionTerrainPatch_t *terrainPatch)
{
    if (CM_TraceWorkIntersectsBounds(traceWork, terrainPatch->bounds[0], terrainPatch->bounds[1]) == qfalse) {
        return;
    }

    const float savedFraction = traceWork->trace.fraction;

    if (terrainPatch->curveCollide != NULL) {
        CM_TraceThroughPatchCollide(traceWork, terrainPatch->curveCollide);
    } else {
        CM_TraceThroughTerrainCollide(traceWork, terrainPatch->terrainCollide);
    }

#if EMULATE_X87
#if defined(WINDOWS_BEHAVIOR)
    const qboolean closer = x87f_lt_signaling(x87f_load_f32(traceWork->trace.fraction), x87f_load_f32(savedFraction)) ? qtrue : qfalse;
#else
    const qboolean closer = x87f_lt(x87f_load_f32(traceWork->trace.fraction), x87f_load_f32(savedFraction)) ? qtrue : qfalse;
#endif
#else
    const qboolean closer = traceWork->trace.fraction < savedFraction ? qtrue : qfalse;
#endif

    if (closer != qfalse) {
        dshader_t *const material = &cm_materials[terrainPatch->materialIndex];

        traceWork->trace.surfaceFlags = material->surfaceFlags;
        traceWork->trace.contents = terrainPatch->contents;
        traceWork->trace.material = material->shader;
    }
}

qboolean CM_SightTraceThroughPatch(const traceWork_t *traceWork, const collisionTerrainPatch_t *terrainPatch)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (CM_TraceWorkIntersectsBounds(traceWork, terrainPatch->bounds[0], terrainPatch->bounds[1]) == qfalse) {
        return qtrue;
    }

    if (terrainPatch->curveCollide != NULL) {
        return CM_SightTraceThroughPatchCollide(traceWork, terrainPatch->curveCollide);
    }

    return CM_SightTraceThroughTerrainCollide(traceWork, terrainPatch->terrainCollide);
}
