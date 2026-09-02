#include "ui_runtime.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_native_x87.h"
#include "math/q_math.h"

#include <stdint.h>
#include <string.h>

enum {
    UI_MODEL_ANGLE_COUNT = 360
};

#define UI_MODEL_VIEW_INSET 1.0f
#define UI_MODEL_DEPTH_SCALE 0.2680000066757202f
#define UI_MODEL_MILLISECONDS_PER_SECOND 1000.0f
#define UI_MODEL_FRAME_TRUNCATION_BIAS (0.5 - (1.0 / 1073741824.0))

extern displayContextDef_t *DC;
void Com_Printf(const char *format, ...);

/*
 * The complete menu-model painter is shared by the two original Windows
 * client DLLs.  After rebasing module-local calls, globals, strings, and
 * constants, the instruction streams agree exactly for the full functions:
 *
 *   uo_cgame_mp_x86.dll  Item_Model_Paint  0x30057240..0x30057674
 *   uo_ui_mp_x86.dll     Item_Model_Paint  0x40018da0..0x400191d4
 *
 * Both bodies retain the same binary32 spill points, x87-to-int conversions,
 * callback reloads, animation update order, and model/refdef field writes.
 * Supporting Mac symbols independently retain the canonical function name.
 */
void Item_Model_Paint(itemDef_t *item)
{
    modelDef_t *model;
    refdef_t refdef;
    refEntity_t entity;
    vec3_t minimums;
    vec3_t maximums;
    vec3_t modelOrigin;
    vec3_t angles = {0.0f, 0.0f, 0.0f};
    vec3_t right;
    float viewportX;
    float viewportY;
    float viewportWidth;
    float viewportHeight;
    displayContextDef_t *display;

    if (item->typeValidated != ITEM_TYPE_MODEL && item->typeValidated != ITEM_TYPE_MENUMODEL) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_MODEL, "
                   "or ITEM_TYPE_MENUMODEL\n");
        return;
    }

    model = (modelDef_t *)item->typeData;
    if (model == NULL || item->asset == 0) {
        return;
    }

    memset(&refdef, 0, sizeof(refdef));
    refdef.viewaxis[0][0] = 1.0f;
    refdef.viewaxis[1][1] = 1.0f;
    refdef.viewaxis[2][2] = 1.0f;
    refdef.rdflags = RDF_NOWORLDMODEL;

    {
        /* Both DLLs spill each rect +/- constant to binary32 before reloading
         * it for the display-scale multiply. */
        float baseX = item->window.rect.x + UI_MODEL_VIEW_INSET;
        float baseY = item->window.rect.y + UI_MODEL_VIEW_INSET;
        float baseWidth;
        float baseHeight;

        display = DC;
        baseWidth = item->window.rect.w - 2.0f * UI_MODEL_VIEW_INSET;
        baseHeight = item->window.rect.h - 2.0f * UI_MODEL_VIEW_INSET;
        viewportX = baseX * display->xscale;
        viewportY = baseY * display->yscale;
        viewportWidth = baseWidth * display->xscale;
        viewportHeight = baseHeight * display->yscale;
    }
    refdef.x = coduo_fp_to_i32_extended((long double)viewportX);
    refdef.y = coduo_fp_to_i32_extended((long double)viewportY);
    refdef.width = coduo_fp_to_i32_extended((long double)viewportWidth);
    refdef.height = coduo_fp_to_i32_extended((long double)viewportHeight);
    refdef.fov_x = model->fovX != 0.0f ? model->fovX : viewportWidth;
    refdef.fov_y = model->fovY != 0.0f ? model->fovY : viewportHeight;

    display->modelBounds(item->asset, minimums, maximums);
    modelOrigin[2] = -0.5f * (maximums[2] + minimums[2]);
    modelOrigin[1] = 0.5f * (maximums[1] + minimums[1]);
    {
        /* The half-height product is stored as binary32 before the divide. */
        float halfHeight = 0.5f * (maximums[2] - minimums[2]);

        modelOrigin[0] = halfHeight / UI_MODEL_DEPTH_SCALE;
    }

    display = DC;
    display->clearScene();
    display = DC;
    refdef.time = display->realTime;

    if (model->rotationSpeed != 0 && display->realTime > item->window.nextTime) {
        int32_t rotationTime = display->realTime;

        item->window.nextTime = coduo_int32_from_bits((uint32_t)rotationTime + (uint32_t)model->rotationSpeed);
        model->angle = coduo_int32_from_bits((uint32_t)model->angle + 1u) % UI_MODEL_ANGLE_COUNT;
    }

    memset(&entity, 0, sizeof(entity));
    angles[1] = (float)model->angle;
    AngleVectors(angles, entity.axis[0], right, entity.axis[2]);
    entity.axis[1][0] = -(long double)right[0];
    entity.axis[1][1] = -(long double)right[1];
    entity.axis[1][2] = -(long double)right[2];

    if (model->frameTime != 0) {
        int32_t previousFrameTime = model->frameTime;
        int32_t elapsed;
        float elapsedFloat;
        float fpsFloat;

        display = DC;
        elapsed = coduo_int32_from_bits((uint32_t)display->realTime - (uint32_t)previousFrameTime);
        elapsedFloat = (float)elapsed;
        fpsFloat = (float)model->fps;
        model->backlerp =
            (float)(((long double)elapsedFloat / UI_MODEL_MILLISECONDS_PER_SECOND) * (long double)fpsFloat + (long double)model->backlerp);
    }
    if (model->backlerp > 1.0f) {
        int32_t elapsedFrames = coduo_x87_fistp_i32((long double)model->backlerp - UI_MODEL_FRAME_TRUNCATION_BIAS);
        int32_t startFrame = model->startFrame;
        int32_t numFrames = model->numFrames;

        model->frame = coduo_int32_from_bits((uint32_t)model->frame + (uint32_t)elapsedFrames);
        /* NOT_FROM_ORIGINAL_SOURCE: the parser establishes a positive frame
         * span; retain the guard for runtime records while preserving the
         * original positive-span frame wrapping behavior. */
        if (numFrames > 0 && coduo_int32_from_bits((uint32_t)model->frame - (uint32_t)startFrame) > numFrames) {
            model->frame = coduo_int32_from_bits((uint32_t)(model->frame % numFrames) + (uint32_t)startFrame);
        }
        model->oldFrame = coduo_int32_from_bits((uint32_t)model->oldFrame + (uint32_t)elapsedFrames);
        if (numFrames > 0 && coduo_int32_from_bits((uint32_t)model->oldFrame - (uint32_t)startFrame) > numFrames) {
            model->oldFrame = coduo_int32_from_bits((uint32_t)(model->oldFrame % numFrames) + (uint32_t)startFrame);
        }
        model->backlerp = (float)((long double)model->backlerp - (float)elapsedFrames);
    }

    display = DC;
    model->frameTime = display->realTime;

    memcpy(entity.origin, modelOrigin, sizeof(modelOrigin));
    memcpy(entity.lightingOrigin, modelOrigin, sizeof(modelOrigin));
    memcpy(entity.oldorigin, modelOrigin, sizeof(modelOrigin));
    entity.frame = model->frame;
    entity.oldframe = model->oldFrame;
    entity.backlerp = 1.0f - model->backlerp;
    entity.renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;
    display->addRefEntityToScene(&entity);
    display = DC;
    display->renderScene(&refdef);
}
