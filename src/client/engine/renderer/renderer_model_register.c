#include "backend.h"

#include "../platform/crt_boundary.h"

#include <stddef.h>
#include <string.h>

enum {
    R_MODEL_SHADER_USAGE = 8
};

/* Source operation: same-module Mac symbol R_AllocModel. The Windows compiler
 * emitted one standalone copy at 0x00517e20..0x00517e5d and inlined the same
 * operation into RE_RegisterModel at 0x00517f0a..0x00517f3e. Both binaries
 * prove the 2,048-entry limit, native renderer-import allocation, assigned
 * registry index, and append order. The original allocator does not clear the
 * remaining model fields. */
model_t *R_AllocModel(void)
{
    model_t *model;

    if (tr.modelCount == R_MAX_MODELS)
        return NULL;

    model = ri.Hunk_Alloc(sizeof(*model));
    model->index = tr.modelCount;
    tr.models[tr.modelCount] = model;
    ++tr.modelCount;
    return model;
}

/* Source: CoDUOMP.exe 0x00518170..0x005181af, also emitted inline in R_Init at
 * 0x004c4fca..0x004c500c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00518170_005181b0.mcode and
 * coduomp/mcode/CoDUOMP/FUN_004c4de0_004c507b.mcode.
 * Name and source-level call to R_AllocModel: same-module Mac R_ModelInit.
 * Windows LTCG inlines the allocator in both copies. */
void R_ModelInit(void)
{
    model_t *model;

    tr.modelCount = 0;
    model = R_AllocModel();
    model->type = MODEL_BAD;
}

/* Source: CoDUOMP.exe 0x00517410..0x00517417.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00517410_00517418.mcode.
 * Name and normal source signature: same-module Mac R_GetModelByHandle. The
 * Windows internal helper receives modelHandle in EAX but performs the same
 * unchecked registry lookup. */
model_t *R_GetModelByHandle(int32_t modelHandle)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (modelHandle < 0 || modelHandle >= tr.modelCount) {
        ri.Printf(R_PRINT_WARNING,
                  "R_GetModelByHandle: out of range hModel '%d'\n",
                  modelHandle);
        return tr.models[0];
    }
    return tr.models[modelHandle];
}

/* Source: CoDUOMP.exe 0x00517420..0x0051742e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00517420_0051742f.mcode.
 * Name and signature: same-module Mac RE_GetXModelByHandle and renderer export
 * slot 2. Both binaries prove the direct unchecked model-registry lookup. */
XModel *RE_GetXModelByHandle(int32_t modelHandle)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    return R_GetModelByHandle(modelHandle)->xmodel;
}

/* Source: CoDUOMP.exe 0x0050a9a0..0x0050aa8f, recovered from an executable
 * gap after repairing the missing Ghidra function boundary.
 * Name and ordinary two-argument signature: renderer export slot 10 and the
 * exact same-module Mac symbol RE_GetShaderFromModel. Brush surfaces whose
 * shader has a lightmap are re-registered without one for this external model
 * query; the source image's mipmap flag is retained when that image exists. */
int32_t RE_GetShaderFromModel(int32_t modelHandle, int32_t surfaceIndex)
{
    if (surfaceIndex < 0)
        surfaceIndex = 0;

    model_t *model = tr.models[modelHandle];
    if (model == NULL || model->bmodel == NULL ||
        model->bmodel->firstSurface == NULL) {
        return 0;
    }

    bmodel_t *bmodel = model->bmodel;
    if (surfaceIndex >= bmodel->numSurfaces)
        surfaceIndex = 0;

    msurface_t *surface = &bmodel->firstSurface[surfaceIndex];
    shader_t *shader = surface->shader;
    if (shader->lightmapIndex > -1) {
        qboolean mipRawImage = qtrue;
        image_t *image = imageHashTable[generateHashValue(shader->name)];

        while (image != NULL) {
            if (strcmp(shader->name, image->imgName) == 0) {
                mipRawImage =
                    (image->flags & IMAGE_FLAG_MIPMAP) != 0
                        ? qtrue
                        : qfalse;
                break;
            }
            image = image->hashNext;
        }

        shader = R_FindShader(shader->name, -1, mipRawImage,
                              R_MODEL_SHADER_USAGE);
        shader->stages[0]->rgbGen = CGEN_LIGHTING_DIFFUSE;
        shader->stages[0]->stateBits = GLS_LIGHTING | GLS_DEPTHMASK_TRUE;
    }

    return shader->index;
}

/* Source: CoDUOMP.exe 0x00517e60..0x00517ff9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00517e60_00517ffa.mcode.
 * Name and source-level helper split: same-module Mac RE_RegisterModel and
 * R_AllocModel symbols. The Windows body inlines R_AllocModel and its /GS
 * cookie check; the latter is compiler machinery rather than source behavior.
 * The Windows instructions remain authoritative for null registry slots,
 * return values, allocation order, and the XModel-only format dispatch. */
int32_t RE_RegisterModel(const char *name, int32_t loadMode)
{
    static const char xmodelPrefix[] = "xmodel/";
    char normalizedName[MAX_QPATH];
    model_t *model;
    const char *source;
    char *destination;
    int32_t modelIndex;

    if (name == NULL || name[0] == '\0')
        return 0;

    if (strlen(name) >= sizeof(normalizedName)) {
        Com_Printf("Model name exceeds MAX_QPATH\n");
        return 0;
    }

    source = name;
    destination = normalizedName;
    do {
        char character = *source++;
        if (character == '\\')
            character = '/';
        *destination++ = character;
    } while (destination[-1] != '\0');

    for (modelIndex = 1; modelIndex < tr.modelCount; ++modelIndex) {
        model = tr.models[modelIndex];
        if (model != NULL && Q_stricmp(model->name, normalizedName) == 0)
            return model->type != MODEL_BAD ? modelIndex : 0;
    }

    model = R_AllocModel();
    if (model == NULL) {
        ri.Printf(R_PRINT_WARNING,
                  "RE_RegisterModel: R_AllocModel() failed for '%s'\n",
                  normalizedName);
        return 0;
    }

    Q_strncpyz(model->name, normalizedName, sizeof(model->name));
    R_SyncRenderThread();
    model->numLods = 0;

    if (coduo_crt_strnicmp(normalizedName, xmodelPrefix,
                             sizeof(xmodelPrefix) - 1) == 0) {
        R_LoadXModel(model, normalizedName + sizeof(xmodelPrefix) - 1,
                     loadMode);
        return model->index;
    }

    model->type = MODEL_BAD;
    return 0;
}
