#include "backend.h"

enum {
    R_DEFAULT_SHADER_HANDLE = 0
};

/* Source: CoDUOMP.exe 0x00518030..0x0051804a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00518030_0051804b.mcode.
 * Name and signature: same-module Mac RE_SetIgnorePrecacheErrors and renderer
 * export slot 8. The value is a nesting counter, so false deliberately
 * decrements without clamping. */
void RE_SetIgnorePrecacheErrors(qboolean ignorePrecacheErrors)
{
    if (ignorePrecacheErrors != qfalse)
        ++tr.ignorePrecacheErrorCount;
    else
        --tr.ignorePrecacheErrorCount;
}

/* Source: CoDUOMP.exe 0x00518050..0x0051805d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00518050_0051805e.mcode.
 * Name and signature: same-module Mac RE_GetIgnorePrecacheErrors and renderer
 * export slot 9. */
qboolean RE_GetIgnorePrecacheErrors(void)
{
    return tr.ignorePrecacheErrorCount != 0 ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x00518000..0x00518022.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00518000_00518023.mcode.
 * Name and source call sequence: same-module Mac RE_FinishLoadingModels.
 * Windows tail-calls R_OptimizeXModelSurfaces; that is the compiler form of
 * the final ordinary source call. */
void RE_FinishLoadingModels(void)
{
    R_LoadDelayedImages();
    tr.modelsFinishedLoading = qtrue;
    R_MergeShadersForImageSheets();
    R_FinishLoadingStaticModels();
    R_FixupXModelTexCoords();
    R_OptimizeXModelSurfaces();
}

/* Source: CoDUOMP.exe 0x00518060..0x00518166.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00518060_00518167.mcode.
 * Name and normal signature: same-module Mac RE_BeginRegistration and renderer
 * export slot 1. Windows LTCG inlines R_SyncRenderThread, RE_ClearScene, and
 * RE_StretchPic; the independently named Mac body preserves these source
 * calls. The 0x04885aa0 consumer prints "viewcluster: %i", proving the field
 * initialized to -1 here. */
void RE_BeginRegistration(glconfig_t *config)
{
    R_Init();
    *config = glConfig;
    R_SyncRenderThread();
    tr.viewCluster = -1;
    RE_ClearFlares();
    RE_ClearScene();
    tr.registered = qtrue;
    RE_StretchPic(0.0f, 0.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 1.0f, 1.0f,
                  R_DEFAULT_SHADER_HANDLE);
}
