#include "backend.h"

#include "qcommon/hunk.h"

enum {
    /* The original function reserves exactly 0x1000 bytes for this qboolean
     * table and indexes one entry per surface in the current model LOD. */
    R_MAX_XMODEL_SURFACES_PER_LOD = 1024
};

/* Source: CoDUOMP.exe 0x00517c40..0x00517e13.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00517c40_00517e13.mcode.
 * Name and source-level XModel/XSurface helper calls: exact same-module Mac
 * symbol R_OptimizeXModelSurfaces. Both binaries prove the global optimization
 * gates, per-LOD large-surface table, duplicate-shader suppression for smaller
 * surfaces, rigid-surface optimization, and final XModelSetOptimize state. */
void R_OptimizeXModelSurfaces(void)
{
    qboolean largeSurface[R_MAX_XMODEL_SURFACES_PER_LOD];

    if (r_optimize->integer == 0 || r_optimizeXModels->integer <= 0)
        return;

    for (int32_t modelIndex = 1; modelIndex < tr.modelCount; ++modelIndex) {
        model_t *model = tr.models[modelIndex];
        XSurface **surfaces;
        int32_t lodCount;

        if (model->xmodel == NULL)
            continue;

        lodCount = XModelGetNumLods(model->xmodel);
        for (int32_t lodIndex = 0; lodIndex < lodCount; ++lodIndex) {
            uint16_t *shaderHandles = model->shaderHandles[lodIndex];
            int32_t surfaceCount =
                XModelGetSurfaces(model->xmodel, &surfaces, lodIndex);

            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (surfaceCount > R_MAX_XMODEL_SURFACES_PER_LOD) {
                ri.Printf(R_PRINT_WARNING, "WARNING: model '%s' LOD %i has %i surfaces; skipping optimization (limit %i)\n", model->name, lodIndex, surfaceCount, R_MAX_XMODEL_SURFACES_PER_LOD);
                continue;
            }

            for (int32_t surfaceIndex = 0;
                 surfaceIndex < surfaceCount; ++surfaceIndex) {
                largeSurface[surfaceIndex] = qfalse;
                if (shaderHandles[surfaceIndex] != 0 &&
                    XSurfaceGetNumVerts(surfaces[surfaceIndex]) >=
                        r_optimizeXModels->integer) {
                    largeSurface[surfaceIndex] = qtrue;
                }
            }

            for (int32_t surfaceIndex = 0;
                 surfaceIndex < surfaceCount; ++surfaceIndex) {
                uint16_t shaderHandle = shaderHandles[surfaceIndex];

                if (shaderHandle == 0)
                    continue;

                if (largeSurface[surfaceIndex] == qfalse) {
                    int32_t matchingSurface;

                    for (matchingSurface = 0;
                         matchingSurface < surfaceCount;
                         ++matchingSurface) {
                        if (matchingSurface != surfaceIndex &&
                            shaderHandles[matchingSurface] == shaderHandle) {
                            break;
                        }
                    }
                    if (matchingSurface != surfaceCount)
                        continue;
                }

                XSurfaceOptimize(
                    surfaces[surfaceIndex], Hunk_AllocXModelPrecacheMesh);
            }
        }
    }

    XModelSetOptimize(qtrue);
}
