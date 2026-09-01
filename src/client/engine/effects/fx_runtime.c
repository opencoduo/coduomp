#include "fx_runtime.h"
#include "fx_bolt.h"

#include "../client/debug_lines.h"
#include "compat/coduo_native_x87.h"
#include "../scripting/script_runtime.h"
#include "../ui/ui_module_loader.h"

#include <math.h>
#include <string.h>

cvar_t *fx_debugBolt; /* original 0x04dc8824 */
cfx_bolt_frame_t *fxBoltFrames; /* original list head 0x0389fe7c */
int32_t fxDrawnEffectCount;     /* original 0x00d8d54c */
int32_t fxReplacementSlotIndex; /* original 0x038b50a4 */

/* Original float32 0x3c8efa35; mathematically pi / 180. */
#define FX_DEGREES_TO_RADIANS 0.01745329238474369f

enum {
    FX_BONE_INDEX_NOT_FOUND = -1
};

/* Source: CoDUOMP.exe 0x004a0350..0x004a0392.
 * Name: same-module Mac symbol FX_GetBoneIndex. The Windows compiler inlines
 * Com_GetClientDObj's handle-table and shared-pool lookup. */
int32_t FX_GetBoneIndex(int32_t entityNum, const char *tagName)
{
    DObj *obj = Com_GetClientDObj(entityNum);
    if (obj == NULL) {
        return FX_BONE_INDEX_NOT_FOUND;
    }

    uint16_t partName = SL_FindLowercaseString(tagName);
    if (partName == 0) {
        return FX_BONE_INDEX_NOT_FOUND;
    }
    return DObjFindPartIndex(obj, partName);
}

/* Source: CoDUOMP.exe 0x004a04f0..0x004a05a4.
 * Name: same-module Mac symbol FX_SetWind.  The original x87 path rounds each
 * degree-to-radian product and each sine/cosine result to float before forming
 * the forward direction. */
void FX_SetWind(const vec2_t angles, float strength)
{
    float yawRadians = angles[1] * FX_DEGREES_TO_RADIANS;
    float yawSin;
    float yawCos;
    coduo_x87_sincosf(yawRadians, &yawSin, &yawCos);
    float pitchRadians = angles[0] * FX_DEGREES_TO_RADIANS;
    float pitchSin;
    float pitchCos;
    coduo_x87_sincosf(pitchRadians, &pitchSin, &pitchCos);

    float forwardX = pitchCos * yawCos;
    float forwardY = pitchCos * yawSin;
    float forwardZ = -pitchSin;
    fx_windDirection[0] = forwardX * strength;
    fx_windDirection[1] = forwardY * strength;
    fx_windDirection[2] = forwardZ * strength;
}

/* Source: CoDUOMP.exe 0x004a0a00..0x004a0a3c. The x87 multiply keeps the
 * exact product of the float input and 255 until _ftol2 returns the low dword
 * of its truncating signed-64 conversion. */
void ClampVec(const vec3_t color, uint8_t colorBytes[3])
{
    for (int32_t component = 0; component < 3; ++component) {
        int32_t value = coduo_fp_to_i32_extended(
            (long double)color[component] * 255.0L);
        if (value < 0) {
            value = 0;
        } else if (value > 255) {
            value = 255;
        }
        colorBytes[component] = (uint8_t)value;
    }
}

/* Original debug-axis colors at 0x0058fba8, 0x0058fbb8, and 0x0058fbd8. */
static const vec4_t fxBoltAxisColorX = {1.0f, 0.0f, 0.0f, 1.0f};
static const vec4_t fxBoltAxisColorY = {0.0f, 1.0f, 0.0f, 1.0f};
static const vec4_t fxBoltAxisColorZ = {0.0f, 0.0f, 1.0f, 1.0f};

/* Source: CoDUOMP.exe 0x004af560..0x004af883.
 * Name and signature: same-module Mac symbol
 * FX_GetBoneOrientation(SFxBoltInfo const *, orientation_t *). Windows VM
 * commands 13 and 12 respectively obtain the entity frame and force the
 * requested client's DObj bone transform current before it is read. */
qboolean FX_GetBoneOrientation(const sfx_bolt_info_t *boltInfo,
                               orientation_t *orientation)
{
    orientation_t entityOrientation;

    (void)VM_Call(coduo_cgameVm, CGVM_GET_ENTITY_ORIGIN_AXIS,
                  boltInfo->entityNum,
                  (intptr_t)entityOrientation.origin,
                  (intptr_t)entityOrientation.axis,
                  0, 0, 0, 0, 0, 0, 0, 0, 0);

    if (boltInfo->boneIndex < 0) {
        *orientation = entityOrientation;
        return qtrue;
    }

    DObj *obj = Com_GetClientDObj(boltInfo->entityNum);
    if (obj == NULL) {
        return qfalse;
    }
    if (boltInfo->boneIndex >= obj->boneCount) {
        return qfalse;
    }

    (void)VM_Call(coduo_cgameVm, CGVM_DOBJ_CALC_BONE_GENERIC,
                  boltInfo->entityNum, boltInfo->boneIndex,
                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    const int32_t matrixIndex =
        (int32_t)obj->modelPartBaseIndices[0] + boltInfo->boneIndex;
    const DObjSkelMat *boneMatrix =
        &obj->evaluationStorage->partSpans[matrixIndex].basePose;

    /* 0x004af624..0x004af72e: the x87 schedule accumulates each axis result
     * in 2,1,0 term order. Each float product is exact in double and the sum
     * needs at most 50 significand bits, preserving the final float store. */
    for (int row = 0; row < 3; ++row) {
        for (int component = 0; component < 3; ++component) {
            orientation->axis[row][component] = (float)(
                (double)entityOrientation.axis[2][component] *
                    boneMatrix->axis[row][2] +
                (double)entityOrientation.axis[1][component] *
                    boneMatrix->axis[row][1] +
                (double)entityOrientation.axis[0][component] *
                    boneMatrix->axis[row][0]);
        }
    }

    /* 0x004af731..0x004af79a uses the observed 0,2,1 product order, then adds
     * the entity origin, before the float store. */
    for (int component = 0; component < 3; ++component) {
        orientation->origin[component] = (float)(
            ((double)entityOrientation.axis[0][component] *
                 boneMatrix->origin[0] +
             (double)entityOrientation.axis[2][component] *
                 boneMatrix->origin[2]) +
            (double)entityOrientation.axis[1][component] *
                boneMatrix->origin[1] +
            entityOrientation.origin[component]);
    }

    if (fx_debugBolt->integer != 0) {
        const vec4_t *axisColors[3] = {
            &fxBoltAxisColorX, &fxBoltAxisColorY, &fxBoltAxisColorZ
        };

        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            /* 0x004af7a8, 0x004af7f2, and 0x004af838 reload and convert the
             * cvar integer separately for the three submitted axes. */
            const float axisLength = (float)fx_debugBolt->integer;
            vec3_t end;
            for (int component = 0; component < 3; ++component) {
                end[component] =
                    orientation->origin[component] +
                    axisLength * orientation->axis[axisIndex][component];
            }
            CL_AddDebugLine(orientation->origin, end,
                            *axisColors[axisIndex], qtrue, 0, qfalse);
        }
    }

    return qtrue;
}
