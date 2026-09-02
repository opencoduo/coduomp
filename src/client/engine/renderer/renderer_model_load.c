#include "backend.h"

#include "qcommon/hunk.h"

#include <string.h>

/* Source: CoDUOMP.exe 0x00508d60..0x00508da5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00508d60_00508da5.mcode.
 * Name: same-module Mac symbol R_BeginDelayedImageGroup.
 * MSVC additionally inlines this body at the start of R_LoadXModel
 * (0x0051765e..0x005176a2) and the static-model loader
 * (0x00519457..0x005194a3). */
void R_BeginDelayedImageGroup(const char *modelName)
{
    if (tr.modelsFinishedLoading == qfalse) {
        ++tr.delayedImageGroupSequence;
        tr.delayedImageGroup = tr.delayedImageGroupSequence;
        return;
    }

    if (r_highLodDist->integer != 0 && tr.ignorePrecacheErrorCount == 0) {
        ri.Error(ERR_DROP, "\x15model '%s' not precached\n", modelName);
    }
}

/* Source: CoDUOMP.exe 0x00508db0..0x00508dc3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00508db0_00508dc3.mcode.
 * Name: same-module Mac symbol R_SetImageGroupTileMode.
 * The conditional store is additionally inlined at
 * 0x005177aa..0x005177c6. */
void R_SetImageGroupTileMode(int32_t tileMode)
{
    if (tr.delayedImageGroup != 0)
        tr.delayedImageGroupTileMode = tileMode;
}

/* Source: CoDUOMP.exe 0x00508dd0..0x00508de3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00508dd0_00508de3.mcode.
 * Name: same-module Mac symbol R_SetImageGroupTriCount.
 * The conditional store is additionally inlined beside the tile mode at
 * 0x005177aa..0x005177c6. */
void R_SetImageGroupTriCount(int32_t triangleCount)
{
    if (tr.delayedImageGroup != 0)
        tr.delayedImageGroupTriCount = triangleCount;
}

/* Source: CoDUOMP.exe 0x00508df0..0x00508dfd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00508df0_00508dfd.mcode.
 * Name: same-module Mac symbol R_EndDelayedImageGroup.
 * MSVC additionally emits the two stores inline at
 * 0x00517861..0x00517868. */
void R_EndDelayedImageGroup(void)
{
    tr.delayedImageGroup = 0;
    tr.delayedImageGroupTriCount = 0;
}

/* Source: CoDUOMP.exe 0x00517650..0x00517879.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00517650_0051787a.mcode.
 * Name, ordinary model/name/shaderUsage signature, and source-level helper
 * calls: exact same-module Mac symbol R_LoadXModel. Windows instructions prove
 * the combined LOD-pointer/surface-handle allocation, "skins/" construction,
 * shader lookup arguments, remapped/defaulted shader handling, and final model
 * fields. The distinct 0x0043d360/0x0043d370 callbacks both select
 * 32-byte-aligned high-hunk allocation in this executable. */
void R_LoadXModel(model_t *model, const char *name, int32_t shaderUsage)
{
    static const char shaderPrefix[] = "skins/";
    int32_t totalSurfaceCount = 0;
    XSurface **surfaces;

    R_BeginDelayedImageGroup(name);

    XModel *xmodel = XModelPrecache(name, XMODEL_LOAD_SURFACES_PREPROCESSED, Hunk_AllocXModelPrecache, Hunk_AllocXModelPrecacheMesh);
    int32_t lodCount = XModelGetNumLods(xmodel);

    for (int32_t lodIndex = 0; lodIndex < lodCount; ++lodIndex) {
        totalSurfaceCount += XModelGetSurfaces(xmodel, &surfaces, lodIndex);
    }

    size_t lodPointerBytes = (size_t)lodCount * sizeof(model->shaderHandles[0]);
    size_t surfaceHandleBytes = (size_t)totalSurfaceCount * sizeof(uint16_t);
    uint16_t **shaderHandles = Hunk_AllocInternal(lodPointerBytes + surfaceHandleBytes);
    uint16_t *surfaceHandle = (uint16_t *)((uint8_t *)shaderHandles + lodPointerBytes);

    for (int32_t lodIndex = 0; lodIndex < lodCount; ++lodIndex) {
        int32_t surfaceCount = XModelGetSurfaces(xmodel, &surfaces, lodIndex);
        shaderHandles[lodIndex] = surfaceHandle;

        for (int32_t surfaceIndex = 0; surfaceIndex < surfaceCount; ++surfaceIndex) {
            char shaderName[MAX_QPATH];
            shader_t *shader;
            const char *surfaceName = XModelGetSurfaceName(xmodel, surfaceIndex, lodIndex);
            const size_t surfaceNameLength = strlen(surfaceName);

            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (surfaceNameLength > sizeof(shaderName) - sizeof(shaderPrefix)) {
                model->type = MODEL_BAD;
                R_EndDelayedImageGroup();
                ri.Error(ERR_DROP, "\x15XModel '%s' has an overlong surface name", name);
                return;
            }
            memcpy(shaderName, shaderPrefix, sizeof(shaderPrefix) - 1);
            memcpy(shaderName + sizeof(shaderPrefix) - 1, surfaceName, surfaceNameLength + 1);

            R_SetImageGroupTriCount(XSurfaceGetNumTris(surfaces[surfaceIndex]));
            R_SetImageGroupTileMode(XSurfaceTileMode(surfaces[surfaceIndex]));

            shader = R_FindShader(shaderName, -1, qtrue, shaderUsage);
            if ((shader->flags & SHADER_FLAG_REMAPPED) != 0) {
                ri.Printf(R_PRINT_WARNING, "WARNING: model '%s' not precached, bad texturing will result on some surfaces\n", name);
                shader = shader->remappedShader;
            }

            *surfaceHandle = (shader->flags & SHADER_FLAG_DEFAULTED) == 0 ? (uint16_t)shader->index : 0;
            ++surfaceHandle;
        }
    }

    model->type = MODEL_XMODEL;
    model->shaderHandles = shaderHandles;
    model->xmodel = xmodel;

    R_EndDelayedImageGroup();
}
