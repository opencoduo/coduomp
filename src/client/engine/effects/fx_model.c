#include "fx_model.h"

#include "fx_memory.h"
#include "qcommon/hunk.h"
#include "../platform/crt_boundary.h"
#include "../renderer/renderer_api.h"

#include <string.h>

enum {
    FX_MODEL_HUNK_ALIGNMENT = 32,
    FX_MODEL_DOBJ_MODEL_COUNT = 1,
    FX_MODEL_DOBJ_GAME_ID = 0,
    FX_MODEL_RENDERER_LOAD_MODE = 9
};

static const char fxModelNamePrefix[] = "xmodel/";

/* Original list head at CoDUOMP.exe 0x0389fe80. */
fx_model_registration_t *fxModelRegistrations;

/* Source: CoDUOMP.exe 0x004a0c50..0x004a0c5d.  Static XModel-load allocator
 * used by CFxModel::Register. */
void *CFxModel_Alloc(size_t size)
{
    return Hunk_AllocAlignInternal(size, FX_MODEL_HUNK_ALIGNMENT);
}

/* Source: CoDUOMP.exe 0x004a0c60..0x004a0dc7.
 * Name: same-module Mac symbol CFxModel::Register. */
DObj *CFxModel_Register(const char *name)
{
    const size_t prefixLength = sizeof(fxModelNamePrefix) - 1U;
    if (coduo_crt_strnicmp(name, fxModelNamePrefix, prefixLength) != 0) {
        return NULL;
    }

    for (fx_model_registration_t *registration = fxModelRegistrations; registration != NULL; registration = registration->next) {
        if (coduo_crt_stricmp(name, registration->name) == 0) {
            return &registration->dobj;
        }
    }

    fx_model_registration_t *registration = FxMem_AllocModel(&fxModelAllocator, sizeof(*registration));
    if (registration == NULL) {
        return NULL;
    }

    XModel *model = XModelPrecache(name + prefixLength, XMODEL_LOAD_SURFACES_PREPROCESSED, CFxModel_Alloc, CFxModel_Alloc);
    if (model == NULL) {
        FxMem_FreeModel(&fxModelAllocator, registration);
        return NULL;
    }

    /* 0x004a0cfb..0x004a0d17 calls renderer export slot 3:
     * RegisterModel(name, 9), tests AX, and stores that renderer handle into
     * the DObj descriptor. This is not an SL_GetString call. */
    int16_t rendererModelHandle = (int16_t)rendererExports.RegisterModel(name, FX_MODEL_RENDERER_LOAD_MODE);
    if (rendererModelHandle == 0) {
        FxMem_FreeModel(&fxModelAllocator, registration);
        return NULL;
    }

    DObjModel descriptor;
    descriptor.model = model;
    descriptor.tagName = NULL;
    descriptor.modelIndex = rendererModelHandle;
    /* DObjCreate does not consume this ABI-carried halfword. */
    descriptor.reserved_00a = 0;
    descriptor.ignoreCollision = 0;
    DObjCreate(&descriptor, FX_MODEL_DOBJ_MODEL_COUNT, NULL, &registration->dobj, FX_MODEL_DOBJ_GAME_ID);

    dobj_eval_storage_t *storage = Hunk_AllocAlignInternal((size_t)DObjGetAllocSkelSize(&registration->dobj), FX_MODEL_HUNK_ALIGNMENT);
    DObjCreateSkel(&registration->dobj, storage);

    uint32_t partBits[DOBJ_PART_BITSET_WORD_COUNT];
    for (int32_t wordIndex = 0; wordIndex < DOBJ_PART_BITSET_WORD_COUNT; ++wordIndex) {
        partBits[wordIndex] = UINT32_MAX;
    }
    DObjCalcAnim(&registration->dobj, partBits);
    DObjCalcSkel(&registration->dobj, partBits);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    strcpy(registration->name, name);
    registration->next = fxModelRegistrations;
    fxModelRegistrations = registration;
    return &registration->dobj;
}

/* Source: CoDUOMP.exe 0x004a0dd0.  Name: same-module Mac symbol
 * CFxModel::DObjForHandle.  Windows stores the DObj address directly; the
 * portable interface consequently keeps it as a pointer on every host. */
DObj *CFxModel_DObjForHandle(DObj *handle)
{
    return handle;
}

/* Source: CoDUOMP.exe 0x004a0de0..0x004a0ded.
 * Name: same-module Mac symbol CFxModel::NameForDObj. */
const char *CFxModel_NameForDObj(const DObj *obj)
{
    if (obj == NULL) {
        return "";
    }
    const fx_model_registration_t *registration =
        (const fx_model_registration_t *)((const uint8_t *)obj - offsetof(fx_model_registration_t, dobj));
    return registration->name;
}

/* Source: CoDUOMP.exe 0x004a0df0..0x004a0e33.
 * Name: same-module Mac symbol CFxModel::Clean. */
void CFxModel_Clean(void)
{
    while (fxModelRegistrations != NULL) {
        fx_model_registration_t *registration = fxModelRegistrations;
        fxModelRegistrations = registration->next;
        DObjFree(&registration->dobj, qtrue);
        FxMem_FreeModel(&fxModelAllocator, registration);
    }
}
