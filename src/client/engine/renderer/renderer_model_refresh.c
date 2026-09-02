#include "backend.h"

/* Source: CoDUOMP.exe 0x00518220..0x005182c9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00518220_005182ca.mcode.
 * Name and source-level XModel accessor calls: exact same-module Mac symbol
 * R_RefreshXModels_ARB. The Windows optimizer inlines XModelGetNumLods and
 * XModelGetSurfaces but preserves the same model/LOD/surface traversal. */
void R_RefreshXModels_ARB(
    renderer_vbo_refresh_components_t refreshComponents)
{
    for (int32_t modelIndex = 0; modelIndex < tr.modelCount; ++modelIndex) {
        model_t *model = tr.models[modelIndex];

        if (model->xmodel == NULL)
            continue;

        const int32_t lodCount = XModelGetNumLods(model->xmodel);
        for (int32_t lodIndex = 0; lodIndex < lodCount; ++lodIndex) {
            XSurface **surfaces;
            const int32_t surfaceCount =
                XModelGetSurfaces(model->xmodel, &surfaces, lodIndex);

            for (int32_t surfaceIndex = 0;
                 surfaceIndex < surfaceCount; ++surfaceIndex) {
                XSurfaceRefresh_ARB(surfaces[surfaceIndex],
                                    (uint32_t)refreshComponents);
            }
        }
    }
}

/* Source: CoDUOMP.exe 0x005182d0..0x00518391.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005182d0_00518392.mcode.
 * Name and source-level XModel accessor calls: exact same-module Mac symbol
 * R_IncrementalRefreshXModels_ARB. Both binaries prove that the persistent
 * surface cursor is incremented before the selected surface is refreshed;
 * retain that ordering even though index zero is consequently not selected
 * by this incremental path. At most one XSurface is refreshed per call. */
void R_IncrementalRefreshXModels_ARB(
    renderer_vbo_refresh_components_t refreshComponents)
{
    int32_t visitedModelCount = 0;

    for (;;) {
        model_t *model = tr.models[tr.xmodelRefreshModelIndex];

        if (model->xmodel != NULL) {
            XSurface **surfaces;
            const int32_t surfaceCount = XModelGetSurfaces(
                model->xmodel, &surfaces, tr.xmodelRefreshLodIndex);

            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            ++tr.xmodelRefreshSurfaceIndex;
            if (tr.xmodelRefreshSurfaceIndex < surfaceCount) {
                XSurfaceRefresh_ARB(
                    surfaces[tr.xmodelRefreshSurfaceIndex],
                    (uint32_t)refreshComponents);
                return;
            }

            tr.xmodelRefreshSurfaceIndex = 0;
            ++tr.xmodelRefreshLodIndex;
            if (tr.xmodelRefreshLodIndex <
                XModelGetNumLods(model->xmodel)) {
                continue;
            }
        }

        tr.xmodelRefreshLodIndex = 0;
        const int32_t previousModelIndex = tr.xmodelRefreshModelIndex;

        for (;;) {
            ++visitedModelCount;
            if (visitedModelCount >= tr.modelCount)
                return;

            ++tr.xmodelRefreshModelIndex;
            if (tr.xmodelRefreshModelIndex >= tr.modelCount)
                tr.xmodelRefreshModelIndex = 0;

            if (tr.xmodelRefreshModelIndex == previousModelIndex)
                return;

            if (tr.models[tr.xmodelRefreshModelIndex]->xmodel != NULL)
                break;
        }
    }
}
