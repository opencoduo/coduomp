// Source: uo_cgame_mp_x86.dll 0x300592b0..0x30059364;
//         uo_ui_mp_x86.dll    0x4001ae20..0x4001aed4 (exact after rebasing).
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300592b0_30059364.mcode

#include "compat/coduo_int32_bits.h"
#include "ui_parse.h"
#include "ui_runtime.h"

void Com_Printf(const char *format, ...);

qboolean ItemParse_model_animplay(itemDef_t *item, int handle)
{
    displayContextDef_t *display;
    int32_t startFrame;
    int32_t nextFrame;

    Item_ValidateTypeData(item, handle);
    if (item->typeValidated != ITEM_TYPE_MODEL &&
        item->typeValidated != ITEM_TYPE_MENUMODEL) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_MODEL, or ITEM_TYPE_MENUMODEL\n");
        return qfalse;
    }

    modelDef_t *model = (modelDef_t *)item->typeData;
    if (model == NULL)
        return qfalse;

    model->animated = 1;
    if (!PC_Int_Parse(handle, &model->startFrame) ||
        !PC_Int_Parse(handle, &model->numFrames) ||
        !PC_Int_Parse(handle, &model->loopFrames) ||
        !PC_Int_Parse(handle, &model->fps))
        return qfalse;

    /* NOT_FROM_ORIGINAL_SOURCE: publish animation state only when the frame
     * span satisfies the positive-span runtime contract. */
    if (model->numFrames <= 0) {
        PC_SourceError(handle, "model_animplay frame count must be positive\n");
        model->animated = 0;
        return qfalse;
    }

    startFrame = model->startFrame;
    display = DC;
    /* 0x30059341 LEA ECX,[EAX+1]: target dword addition wraps modulo 2^32. */
    nextFrame = coduo_int32_from_bits((uint32_t)startFrame + 1u);
    model->oldFrame = startFrame;
    model->frame = nextFrame;
    model->backlerp = 0.0f;
    model->frameTime = display->realTime;
    return qtrue;
}
