#include "fx_classes.hpp"
#include "../client/cgame.h"
#include "../client/debug_lines.h"
#include "../math/vector_math.h"
#include "../platform/crt_boundary.h"
#include "../renderer/renderer_api.h"
#include "../scripting/script_runtime.h"
#include "../sound/miles_boundary.h"
#include "../ui/ui_module_loader.h"

#include "fx_model.h"
#include "fx_primitive_template.hpp"
#include "fx_runtime.h"
#include "fx_scheduler.hpp"
#include "compat/coduo_native_x87.h"

#include <math.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
    FX_STATMON_WARNING_DURATION_MSEC = 3000,
    FX_STATMON_MAX_EFFECTS_ENTRY = 7,
    FX_STATMON_MAX_EFFECTS_ENTRY_COUNT = 8,
    FX_MAX_FRAME_TIME_MSEC = 200,
    FX_SHADER_LOAD_MODE = 9,
    FX_STATMON_SHADER_LOAD_MODE = 1,
    FX_SOUND_TIME_SHIFT_NONE = 0
};

enum { FX_DEBUG_PLUME_DURATION_MSEC = 3000 };

enum {
    FX_RANDOM_MULTIPLIER = 214013,
    FX_RANDOM_INCREMENT = 2531011,
    FX_RANDOM_RESULT_SHIFT = 17,
    FX_RANDOM_ELEMENT_SCALE_SHIFT = 15
};

/* Original private scheduler counters at 0x0389fe84 and 0x0389fe88.  The
 * first counts primitive templates considered for playback and the second
 * counts individual spawn attempts before immediate/delayed dispatch. */
static int32_t fxPrimitiveTemplatePlayCount;
static int32_t fxPrimitiveSpawnCount;

/* Original renderer debug-plume color at 0x0058fbe8.  RE_AddPlume consumes
 * only the first three floats of the shared yellow vec4 constant. */
static const vec3_t fxSpawnCountPlumeColor = {1.0f, 1.0f, 0.0f};

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the identical inlined
 * LCG update in the three Windows helpers at 0x004a5e70..0x004a5fcc and in
 * CFxScheduler::CreateEffect. */
static uint32_t FX_NextRandom15(void)
{
    sharedRandSeed =
        sharedRandSeed * FX_RANDOM_MULTIPLIER + FX_RANDOM_INCREMENT;
    return sharedRandSeed >> FX_RANDOM_RESULT_SHIFT;
}

float FX_RandomFloatRange(const fx_float_range_t &range);

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the repeated vector-
 * range sampling and local-axis transform in CreateEffect.  The transformed
 * path samples z, y, x in that order, matching the Windows call sequence. */
static void FX_SampleVectorRange(const fx_vector_range_t &range,
                                 const axis_t axis, bool direct,
                                 vec3_t sampled)
{
    if (direct) {
        for (int32_t component = 0; component < 3; ++component) {
            const fx_float_range_t componentRange = {
                range.start[component], range.end[component]
            };
            sampled[component] = FX_RandomFloatRange(componentRange);
        }
        return;
    }

    vec3_t local;
    for (int32_t component = 2; component >= 0; --component) {
        const fx_float_range_t componentRange = {
            range.start[component], range.end[component]
        };
        local[component] = FX_RandomFloatRange(componentRange);
    }
    for (int32_t component = 0; component < 3; ++component) {
        sampled[component] =
            local[0] * axis[0][component] +
            local[1] * axis[1][component] +
            local[2] * axis[2][component];
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit VM float-bit transport for the impact-
 * mark command.  Native pointers widen, but VM scalar float payloads remain
 * the original four-byte representation. */
static intptr_t FX_FloatVmArgument(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return static_cast<intptr_t>(bits);
}

/* Source: CoDUOMP.exe 0x004a5e70..0x004a5eaa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004a5e70_004a5eaa.mcode.
 * Role name: the same i386 body serves every four-byte media/effect vector;
 * maintained native source keeps it generic so pointer-bearing resources
 * widen with their host platform. */
template <typename T>
T FX_RandomElement(const std::vector<T> &values)
{
    if (values.empty()) {
        T emptyValue{};
        /* The stock helper returns a zero dword.  Pointer-bearing native
         * resource unions widen to eight bytes, and value-initializing their
         * first int32_t member alone does not define the upper padding bytes.
         * Clear the complete native representation so the stock zero result
         * remains a null model pointer. */
        memset(&emptyValue, 0, sizeof(emptyValue));
        return emptyValue;
    }

    const uint32_t index =
        (FX_NextRandom15() * static_cast<uint32_t>(values.size())) >>
        FX_RANDOM_ELEMENT_SCALE_SHIFT;
    return values[index];
}

/* Source: CoDUOMP.exe 0x004a5ee0..0x004a5f4e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004a5ee0_004a5f4e.mcode.
 * Role name: choose a uniformly distributed value in the half-open range. */
float FX_RandomFloatRange(const fx_float_range_t &range)
{
    if (range.start == range.end) {
        return range.start;
    }

    const float randomValue = static_cast<float>(FX_NextRandom15());
    return (range.end - range.start) * randomValue / 32768.0f +
           range.start;
}

/* Source: CoDUOMP.exe 0x004a5f50..0x004a5fcc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004a5f50_004a5fcc.mcode.
 * Role name: choose and round an integral-valued FX range.  The equal-endpoint
 * path intentionally converts directly without adding the rounding bias. */
int32_t FX_RandomIntRange(const fx_float_range_t &range)
{
    if (range.start == range.end) {
        return coduo_fp_to_i32_extended((long double)range.start);
    }

    const float randomValue = static_cast<float>(FX_NextRandom15());
    const float value =
        (range.end - range.start) * randomValue / 32768.0f + range.start;
    return coduo_fp_to_i32_extended((long double)value + 0.5L);
}

static const char fxMaxEffectsWarningShaderName[] =
    "gfx/2d/warning@maxeffects.jpg";

/* Original SFxHelper singleton fields at 0x038b5010..0x038b509c.  The mixed
 * C/C++ recovery currently exposes the storage through role-named globals;
 * each SFxHelper_* body below is the corresponding original C++ method. */
int32_t fxLastTime;
int32_t fxCurrentTime;
int32_t fxPreviousTime;
int32_t fxFrameTime;
int32_t fxArchivedTimingState;
vec3_t fxViewOrigin;
fx_cull_plane_t fxCullPlanes[6];
int32_t fxCullPlaneCount;
vec3_t fx_windDirection;

fx_effect_slot_t fxEffectSlots[FX_EFFECT_SLOT_COUNT]; /* 0x00d8d550 */
fx_effect_slot_t *fxEffectFreeList;
int32_t fxActiveEffectCount;
cvar_t *fx_draw;
cvar_t *fx_cull;
cvar_t *fx_enable;
cvar_t *fx_cullscale;
cvar_t *fx_cullbias;
cvar_t *fx_count;
cvar_t *fx_debug;
cvar_t *fx_freeze;

/* 0x0389fe90: FX_Init's one-time slot-storage initialization guard. */
static qboolean fxEffectSlotsInitialized;

/* Source: CoDUOMP.exe 0x004aa5a0..0x004aa5ae.
 * Name and field ownership: same-module Mac symbol SFxHelper::SFxHelper.
 * The constructor deliberately leaves lastTime untouched and clears only the
 * four following timing/archive words. */
void SFxHelper_Construct(void)
{
    fxCurrentTime = 0;
    fxPreviousTime = 0;
    fxFrameTime = 0;
    fxArchivedTimingState = 0;
}

/* Source: CoDUOMP.exe 0x004aa5b0..0x004aa5de.
 * Name and field ownership: same-module Mac symbol SFxHelper::Init. */
void SFxHelper_Init(void)
{
    fxLastTime = 0;
    fxCurrentTime = 0;
    fxPreviousTime = 0;
    fxFrameTime = 0;
    fxArchivedTimingState = 0;
    RE_SetFXImageMemory(0);
    fx_windDirection[2] = 0.0f;
    fx_windDirection[1] = 0.0f;
    fx_windDirection[0] = 0.0f;
}

/* Source: CoDUOMP.exe 0x004aa5e0..0x004aa628.
 * Name: same-module Mac symbol SFxHelper::Print.  The original formats into a
 * 1024-byte stack buffer with unbounded vsprintf, then passes the resulting
 * text directly as the Com_Printf format string. */
void SFxHelper_Print(const char *format, ...)
{
    char text[MAX_STRING_CHARS];
    va_list args;

    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    (void)vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    Com_Printf("%s", text);
}

/* Source: CoDUOMP.exe 0x004aaa90..0x004aaacd.
 * Name and source-level call shape: same-module Mac symbol
 * SFxHelper::RegisterShader.  The renderer tracks total image memory and a
 * separate FX-owned portion; shader registration charges the exact increase
 * in total image memory to the FX counter. */
int32_t SFxHelper_RegisterShader(const char *name)
{
    const uint32_t imageMemoryBefore = RE_GetImageMemory();
    const int32_t shader = RE_RegisterShader(name, FX_SHADER_LOAD_MODE);
    const uint32_t imageMemoryUsed =
        RE_GetImageMemory() - imageMemoryBefore;

    RE_SetFXImageMemory(RE_GetFXImageMemory() + imageMemoryUsed);
    return shader;
}

/* Source: CoDUOMP.exe 0x004aaad0..0x004aab0a.
 * Name and source-level decomposition: same-module Mac symbol
 * SFxHelper::RegisterSound, whose Com_PickSoundAlias/Com_SoundAliasIndex
 * calls compile into the lookup plus 0x4c-byte alias-table index arithmetic
 * seen in Windows. */
int32_t SFxHelper_RegisterSound(const char *name)
{
    snd_alias_t *alias =
        Com_PickSoundAlias(name, SND_ALIAS_BANK_CGAME, vec3_origin);
    return Com_SoundAliasIndex(alias, SND_ALIAS_BANK_CGAME);
}

/* Source: CoDUOMP.exe 0x004aa9a0..0x004aa9ac.
 * Name and signature: same-module Mac symbol SFxHelper::OpenFile.  Both
 * original implementations ignore the helper's third argument and always
 * open through the filesystem read mode. */
int32_t SFxHelper_OpenFile(const char *name, int32_t *fileHandle,
                           int32_t unusedMode)
{
    (void)unusedMode;
    return FS_FOpenFileByMode(name, fileHandle, FS_READ);
}

/* Source: CoDUOMP.exe 0x004aa9b0..0x004aa9c0.
 * Name and signature: same-module Mac symbol SFxHelper::ReadFile. */
qboolean SFxHelper_ReadFile(void *buffer, int32_t byteCount,
                            int32_t fileHandle)
{
    FS_Read(buffer, byteCount, fileHandle);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004aa9d0..0x004aa9d7.
 * Name and signature: same-module Mac symbol SFxHelper::CloseFile. */
void SFxHelper_CloseFile(int32_t fileHandle)
{
    FS_FCloseFile(fileHandle);
}

/* Source: CoDUOMP.exe 0x004aaa80..0x004aaa8c.
 * Name: same-module Mac symbol SFxHelper::AddFxToScene. */
void SFxHelper_AddFxToScene(refEntity_t *entity)
{
    RE_AddRefEntityToScene(entity, NULL);
}

/* Source: CoDUOMP.exe 0x004aab10..0x004aab14.
 * Name: same-module Mac symbol SFxHelper::RegisterModel. */
DObj *SFxHelper_RegisterModel(const char *name)
{
    return CFxModel_Register(name);
}

/* Source: CoDUOMP.exe 0x004aab20..0x004aab2b.
 * Name: same-module Mac symbol SFxHelper::SetIgnorePrecacheErrors.  The
 * Windows body consumes only AL, matching an original boolean parameter. */
void SFxHelper_SetIgnorePrecacheErrors(qboolean ignorePrecacheErrors)
{
    RE_SetIgnorePrecacheErrors(
        static_cast<uint8_t>(ignorePrecacheErrors) != 0
        ? qtrue
        : qfalse);
}

/* Source: CoDUOMP.exe 0x004aab30..0x004aab3a.
 * Name: same-module Mac symbol SFxHelper::GetShaderName. */
const char *SFxHelper_GetShaderName(int32_t shader)
{
    return RE_GetShaderName(shader);
}

/* Source: CoDUOMP.exe 0x004aabd0..0x004aabf4.
 * Name and signature: same-module Mac symbol SFxHelper::AddLightToScene. */
void SFxHelper_AddLightToScene(const vec3_t origin, float radius,
                               float red, float green, float blue)
{
    RE_AddLightToScene(origin, radius, red, green, blue);
}

/* Source: CoDUOMP.exe 0x004aa9e0..0x004aaa2c.
 * Name and source-level call structure: same-module Mac symbol
 * SFxHelper::PlaySound. Windows inlines Com_GetSoundAlias as the 0x4c-byte
 * alias-table lookup before selecting the position-dependent alias. */
void SFxHelper_PlaySound(const vec3_t origin, int32_t entityNum,
                         int32_t soundHandle)
{
    snd_alias_t *registeredAlias = Com_GetSoundAlias(
        soundHandle, SND_ALIAS_BANK_CGAME);
    if (registeredAlias == nullptr) {
        return;
    }

    snd_alias_t *alias = Com_PickSoundAlias(
        registeredAlias->aliasName, SND_ALIAS_BANK_CGAME, origin);
    if (alias != nullptr) {
        (void)MSS_PlaySoundAlias(alias, entityNum, origin,
                                 FX_SOUND_TIME_SHIFT_NONE);
    }
}

/* Source: CoDUOMP.exe 0x004aaa30..0x004aaa6f.
 * Name and signature: same-module Mac symbol SFxHelper::Trace. The first
 * integer argument is present in both original ABIs but is not consumed; the
 * helper traces only the collision model, then supplies the world/none entity
 * selector that the FX collision response expects. */
void SFxHelper_Trace(trace_t *trace, const vec3_t start,
                     const vec3_t mins, const vec3_t maxs,
                     const vec3_t end, int32_t unusedPassEntityNum,
                     int32_t contentMask)
{
    (void)unusedPassEntityNum;
    CM_BoxTrace(trace, start, end, mins, maxs,
                CM_WORLD_MODEL, contentMask, qfalse);
    trace->entityNum = trace->fraction == 1.0f
        ? ENTITYNUM_NONE
        : ENTITYNUM_WORLD;
}

/* Source: CoDUOMP.exe 0x004aac00..0x004aac0c.
 * Role: Windows-retained SFxHelper renderer adapter adjacent to the other
 * helper methods. The GetRefAPI table proves slot +0x4c is
 * RE_AddPolyToScene; the Mac compiler inlines this source wrapper. */
void SFxHelper_AddPolyToScene(int32_t shaderHandle, int32_t vertexCount,
                              const polyVert_t *vertices)
{
    RE_AddPolyToScene(shaderHandle, vertexCount, vertices);
}

/* Source: CoDUOMP.exe 0x004aac10..0x004aac37.
 * Name, signature, and VM argument order: same-module Mac symbol
 * SFxHelper::CameraShake. Command 16 is the recovered cgame
 * CGVM_ADD_CAMERA_SHAKE entry. */
void SFxHelper_CameraShake(const vec3_t origin, float amplitude,
                           int32_t radius, int32_t duration)
{
    constexpr float cameraShakeScale = 0.05000000074505806f;
    float scaledAmplitude = amplitude * cameraShakeScale;

    (void)VM_Call(coduo_cgameVm, CGVM_ADD_CAMERA_SHAKE,
                  reinterpret_cast<intptr_t>(&scaledAmplitude),
                  duration, reinterpret_cast<intptr_t>(origin), radius,
                  0, 0, 0, 0, 0, 0, 0, 0);
}

/* Source: CoDUOMP.exe 0x004aa8e0..0x004aa927.
 * Name: same-module Mac symbol SFxHelper::CullSphere.  Double intermediates
 * retain every bit of the original x87 products and accumulated comparison
 * for float inputs. */
qboolean SFxHelper_CullSphere(const vec3_t origin, float radius)
{
    for (int32_t planeIndex = 0;
         planeIndex < fxCullPlaneCount;
         ++planeIndex) {
        const fx_cull_plane_t *plane = &fxCullPlanes[planeIndex];
        double distance =
            static_cast<double>(plane->normal[0]) * origin[0] +
            static_cast<double>(plane->normal[2]) * origin[2] +
            static_cast<double>(plane->normal[1]) * origin[1] +
            radius;
        if (distance < plane->distance) {
            return qtrue;
        }
    }
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004aa930..0x004aa997.
 * Name: same-module Mac symbol SFxHelper::CullCylinder.  A cylinder is outside
 * only when both capped endpoints lie behind the same culling plane. */
qboolean SFxHelper_CullCylinder(const vec3_t start, const vec3_t end,
                                float startRadius, float endRadius)
{
    for (int32_t planeIndex = 0;
         planeIndex < fxCullPlaneCount;
         ++planeIndex) {
        const fx_cull_plane_t *plane = &fxCullPlanes[planeIndex];
        /* The retail x87 graph accumulates z, y, then x. Its status-word
         * parity branch also treats an unordered result as not behind. */
        double startDistance =
            static_cast<double>(plane->normal[2]) * start[2] +
            static_cast<double>(plane->normal[1]) * start[1] +
            static_cast<double>(plane->normal[0]) * start[0] +
            startRadius;
        if (!(startDistance < plane->distance)) {
            continue;
        }

        double endDistance =
            static_cast<double>(plane->normal[2]) * end[2] +
            static_cast<double>(plane->normal[1]) * end[1] +
            static_cast<double>(plane->normal[0]) * end[0] +
            endRadius;
        if (endDistance < plane->distance) {
            return qtrue;
        }
    }
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004aa630..0x004aa693.
 * Name: same-module Mac symbol SFxHelper::AdjustTime. */
void SFxHelper_AdjustTime(int32_t time)
{
    if (fx_freeze->integer != 0) {
        fxFrameTime = 0;
        fxLastTime = time;
        return;
    }

    int32_t frameTime = fxLastTime != 0 ? time - fxLastTime : 0;
    if (frameTime < 0) {
        frameTime = 0;
    } else if (frameTime > FX_MAX_FRAME_TIME_MSEC) {
        Com_DPrintf("^1large frame time %i\n", frameTime);
        frameTime = FX_MAX_FRAME_TIME_MSEC;
    }

    fxFrameTime = frameTime;
    fxPreviousTime = fxCurrentTime;
    fxCurrentTime += frameTime;
    fxLastTime = time;
}

/* Source: CoDUOMP.exe 0x004aa6a0..0x004aa8d7.
 * Name and signature: same-module Mac symbol SFxHelper::AdjustCamera. The
 * wrapper's caller obtains farPlaneDistance from RE_GetFarPlaneDist. The
 * exact 0x3c0efa35 multiplier converts a full FOV in degrees to its half-angle
 * in radians. */
void SFxHelper_AdjustCamera(refdef_t *refdef, float farPlaneDistance)
{
    constexpr float degreesToHalfRadians =
        0.0087266461923718452f;

    for (int component = 0; component < 3; ++component) {
        fxViewOrigin[component] = refdef->vieworg[component];
        fxCullPlanes[0].normal[component] =
            refdef->viewaxis[0][component];
    }

    const float horizontalHalfAngle =
        refdef->fov_x * degreesToHalfRadians;
    float horizontalSin;
    float horizontalCos;
    coduo_x87_sincosf(
        horizontalHalfAngle, &horizontalSin, &horizontalCos);
    for (int component = 0; component < 3; ++component) {
        fxCullPlanes[1].normal[component] =
            horizontalSin * refdef->viewaxis[0][component];
        fxCullPlanes[1].normal[component] +=
            horizontalCos * refdef->viewaxis[1][component];

        fxCullPlanes[2].normal[component] =
            horizontalSin * refdef->viewaxis[0][component];
        fxCullPlanes[2].normal[component] +=
            -horizontalCos * refdef->viewaxis[1][component];
    }

    const float verticalHalfAngle =
        refdef->fov_y * degreesToHalfRadians;
    float verticalSin;
    float verticalCos;
    coduo_x87_sincosf(
        verticalHalfAngle, &verticalSin, &verticalCos);
    for (int component = 0; component < 3; ++component) {
        fxCullPlanes[3].normal[component] =
            verticalSin * refdef->viewaxis[0][component];
        fxCullPlanes[3].normal[component] +=
            verticalCos * refdef->viewaxis[2][component];

        fxCullPlanes[4].normal[component] =
            verticalSin * refdef->viewaxis[0][component];
        fxCullPlanes[4].normal[component] +=
            -verticalCos * refdef->viewaxis[2][component];
    }

    fxCullPlaneCount = 5;
    if (farPlaneDistance > 0.0f) {
        for (int component = 0; component < 3; ++component) {
            fxCullPlanes[5].normal[component] =
                -refdef->viewaxis[0][component];
        }
        fxCullPlaneCount = 6;
    }

    for (int planeIndex = 0;
         planeIndex < fxCullPlaneCount;
         ++planeIndex) {
        const fx_cull_plane_t *plane = &fxCullPlanes[planeIndex];
        const double distance =
            static_cast<double>(plane->normal[1]) * fxViewOrigin[1] +
            static_cast<double>(plane->normal[0]) * fxViewOrigin[0] +
            static_cast<double>(plane->normal[2]) * fxViewOrigin[2];
        fxCullPlanes[planeIndex].distance = static_cast<float>(distance);
    }

    if (farPlaneDistance > 0.0f) {
        fxCullPlanes[5].distance -= farPlaneDistance;
    }
}

/* Source: CoDUOMP.exe 0x004afa30..0x004afb33.
 * Name: same-module Mac symbol FX_GetValidEffect. */
fx_effect_slot_t *FX_GetValidEffect(void)
{
    fx_effect_slot_t *slot = fxEffectFreeList;
    if (slot != NULL) {
        if (slot->effect != NULL) {
            return NULL;
        }

        fxEffectFreeList = slot->next;
        return slot;
    }

    if (com_statmon->integer != 0) {
        statmonEntries[FX_STATMON_MAX_EFFECTS_ENTRY].expireTime =
            static_cast<int32_t>(
                Sys_Milliseconds() +
                static_cast<uint32_t>(FX_STATMON_WARNING_DURATION_MSEC));

        if (statmonEntries[FX_STATMON_MAX_EFFECTS_ENTRY].shaderHandle == 0 &&
            cls.rendererStarted != qfalse) {
            statmonEntries[FX_STATMON_MAX_EFFECTS_ENTRY].shaderHandle =
                RE_RegisterShaderNoMip(
                fxMaxEffectsWarningShaderName,
                FX_STATMON_SHADER_LOAD_MODE);
        }

        if (statmonEntryCount < FX_STATMON_MAX_EFFECTS_ENTRY_COUNT) {
            statmonEntryCount = FX_STATMON_MAX_EFFECTS_ENTRY_COUNT;
        }
    }

    int32_t slotIndex = fxReplacementSlotIndex;
    ++fxReplacementSlotIndex;
    if (fxReplacementSlotIndex >= FX_EFFECT_SLOT_COUNT) {
        fxReplacementSlotIndex = 0;
    }

    slot = &fxEffectSlots[slotIndex];
    if (slot->effect != NULL) {
        delete slot->effect;
    }

    --fxActiveEffectCount;
    slot->effect = NULL;
    slot->next = fxEffectFreeList;
    fxEffectFreeList = NULL;
    return slot;
}

/* Source: CoDUOMP.exe 0x004b0100..0x004b014a.
 * Name: same-module Mac symbol FX_AddPrimitive(CEffect **, int). */
void FX_AddPrimitive(cfx_effect_t **effect, int32_t lifetime)
{
    fx_effect_slot_t *slot = FX_GetValidEffect();
    slot->effect = *effect;
    const int32_t endTime = fxCurrentTime + lifetime;
    slot->expirationTime = endTime;
    ++fxActiveEffectCount;

    (*effect)->SetTimeStart(fxCurrentTime);
    (*effect)->timeEnd = endTime;
}

/* Source: CoDUOMP.exe 0x004affe0..0x004b00f9.
 * Name: same-module Mac symbol FX_Add. */
void FX_Add(void)
{
    fxDrawnEffectCount = 0;

    for (int32_t slotIndex = 0;
         slotIndex < FX_EFFECT_SLOT_COUNT;
         ++slotIndex) {
        fx_effect_slot_t *slot = &fxEffectSlots[slotIndex];
        cfx_effect_t *effect = slot->effect;
        if (effect == NULL) {
            continue;
        }

        if (fxCurrentTime > slot->expirationTime) {
            effect->flags &= ~FX_EFFECT_FLAG_DIE_ON_IMPACT;
            FX_FreeMember(slot, true);
            continue;
        }

        if (fxFrameTime > 0 && effect->Update() == qfalse) {
            FX_FreeMember(slot, true);
            continue;
        }

        if (fx_draw->integer == 0) {
            continue;
        }
        /* 0x004b008f and 0x004b009f reload slot->effect before the Cull and
         * Draw virtual calls: a spawn during this slot's own Update() can
         * replace the slot's effect, and the original culls/draws the
         * replacement rather than the freed object. */
        if (fx_cull->integer != 0 && slot->effect->Cull() != qfalse) {
            continue;
        }

        ++fxDrawnEffectCount;
        slot->effect->Draw();
    }

    if (fx_debug->integer != 0) {
        SFxHelper_Print("Active    FX: %i\n", fxActiveEffectCount);
        SFxHelper_Print("Drawn     FX: %i\n", fxDrawnEffectCount);
        SFxHelper_Print("Scheduled FX: %i\n",
                        fxScheduler.GetScheduledEffectCount());
    }
}

/* Source: CoDUOMP.exe 0x004af480..0x004af483.
 * The Mac compiler inlined this one-field access; the role name describes the
 * proven SEffectList::next load. */
fx_effect_slot_t *FX_GetNextEffectSlot(fx_effect_slot_t *slot)
{
    return slot->next;
}

/* Source: CoDUOMP.exe 0x004afb40..0x004afb5a.
 * No Mac traceback symbol survives for this unreferenced Windows copy, so the
 * role name describes its exact predicate: an allocated primitive or a
 * scheduled effect means the FX system still has active work. */
qboolean FX_HasActiveEffects(void)
{
    return (fxActiveEffectCount > 0 ||
            fxScheduler.GetScheduledEffectCount() > 0)
        ? qtrue
        : qfalse;
}

/* Source: CoDUOMP.exe 0x004af4a0..0x004af51d.
 * Name: same-module Mac symbol FX_Free. */
qboolean FX_Free(qboolean freeTemplates)
{
    fx_effect_slot_t *previousSlot = &fxEffectSlots[0];
    if (previousSlot->effect != NULL) {
        delete previousSlot->effect;
    }
    previousSlot->effect = NULL;
    fxEffectFreeList = previousSlot;

    /* 0x004af4d0..0x004af4ee deletes each following effect before publishing
     * that slot through the preceding free-list link. */
    for (int32_t slotIndex = 1;
         slotIndex < FX_EFFECT_SLOT_COUNT;
         ++slotIndex) {
        fx_effect_slot_t *slot = &fxEffectSlots[slotIndex];
        if (slot->effect != NULL) {
            delete slot->effect;
        }
        slot->effect = NULL;
        previousSlot->next = slot;
        previousSlot = slot;
    }
    previousSlot->next = NULL;
    fxActiveEffectCount = 0;
    CFxModel_Clean();
    CFxScheduler_Clean(&fxScheduler, freeTemplates, 0);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004af520..0x004af55c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004af520_004af55d.mcode.
 * Role name: the Mac traceback table has no separate symbol. Unlike FX_Free,
 * this retained cleanup preserves the established free-list links and model
 * cache while deleting every active effect and clearing scheduled work. */
void FX_ClearActiveEffects(void)
{
    for (int32_t slotIndex = 0;
         slotIndex < FX_EFFECT_SLOT_COUNT;
         ++slotIndex) {
        fx_effect_slot_t *slot = &fxEffectSlots[slotIndex];

        if (slot->effect != nullptr) {
            delete slot->effect;
            slot->effect = nullptr;
        }
    }

    fxActiveEffectCount = 0;
    CFxScheduler_Clean(&fxScheduler, qfalse, 0);
}

/* Source: CoDUOMP.exe 0x004af890..0x004af90f.
 * Name: same-module Mac symbol FX_Init.  The first pass clears only effect
 * pointers; FX_Free then builds the complete free-list chain before the
 * SFxHelper singleton resets its renderer accounting and wind state. */
qboolean FX_Init(void)
{
    if (fxEffectSlotsInitialized == qfalse) {
        fxEffectSlotsInitialized = qtrue;
        for (int32_t slotIndex = 0;
             slotIndex < FX_EFFECT_SLOT_COUNT;
             ++slotIndex) {
            fxEffectSlots[slotIndex].effect = nullptr;
        }
    }

    FX_Free(qtrue);
    SFxHelper_Init();
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004af910..0x004af946.
 * Name and parameter types: exact same-module Mac symbol
 * FX_FreeMember(SEffectList *, bool). CEffect::Die is vtable slot 1, proving
 * that callDie controls the optional virtual call before object destruction. */
void FX_FreeMember(fx_effect_slot_t *slot, bool callDie)
{
    if (callDie) {
        slot->effect->Die();
    }

    delete slot->effect;
    slot->effect = nullptr;
    slot->next = fxEffectFreeList;
    fxEffectFreeList = slot;
    --fxActiveEffectCount;
}

/* Source: CoDUOMP.exe 0x004a0340..0x004a034d.
 * Name: same-module Mac symbol FX_RegisterEffect. */
int32_t FX_RegisterEffect(const char *name)
{
    return fxScheduler.RegisterEffect(name, false);
}

/* Source: CoDUOMP.exe 0x004a0460..0x004a0464.  Windows-retained public
 * wrapper around the entity-bolted effect cleanup at 0x004af9c0. */
void FX_FreeEntity(int32_t entityNum)
{
    FX_FreeEntityEffects(entityNum);
}

/* Source: CoDUOMP.exe 0x004a03a0..0x004a03ad.
 * Name: same-module Mac symbol FX_PlaySimpleEffect. */
void FX_PlaySimpleEffect(const char *name, const vec3_t origin)
{
    CFxScheduler_PlaySimpleEffect(&fxScheduler, name, origin);
}

/* Source: CoDUOMP.exe 0x004a03b0..0x004a03bb.
 * Name: same-module Mac symbol FX_PlayEffect. */
void FX_PlayEffect(const char *name, const vec3_t origin,
                   const vec3_t forward)
{
    CFxScheduler_PlayEffect(&fxScheduler, name, origin, forward);
}

/* Source: CoDUOMP.exe 0x004a03c0..0x004a03d3.
 * Name: same-module Mac symbol FX_PlayEntityEffect. */
void FX_PlayEntityEffect(const char *name, const vec3_t origin,
                         const axis_t axis,
                         const sfx_bolt_info_t *boltInfo)
{
    CFxScheduler_PlayEntityEffect(&fxScheduler, name, origin, axis,
                                  boltInfo);
}

/* Source: CoDUOMP.exe 0x004a03e0..0x004a03ea.
 * Name: same-module Mac symbol FX_PlaySimpleEffectID. */
void FX_PlaySimpleEffectID(int32_t effectId, const vec3_t origin)
{
    CFxScheduler_PlaySimpleEffectID(&fxScheduler, effectId, origin);
}

/* Source: CoDUOMP.exe 0x004a03f0..0x004a0438.
 * Name: same-module Mac symbol FX_PlayEffectID. */
void FX_PlayEffectID(int32_t effectId, const vec3_t origin,
                     const vec3_t forward)
{
    CFxScheduler_PlayEffectID(&fxScheduler, effectId, origin, forward);
}

/* Source: CoDUOMP.exe 0x004a6e20..0x004a6e81.
 * Name: same-module Mac symbol CFxScheduler::PlayEffect(int, origin).
 * The constant basis is the engine's default forward/right/up orientation. */
void CFxScheduler_PlaySimpleEffectID(cfx_scheduler_t *scheduler,
                                     int32_t effectId,
                                     const vec3_t origin)
{
    const axis_t axis = {
        {0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    };
    CFxScheduler_PlayEntityEffectID(scheduler, effectId, origin,
                                    axis, nullptr);
}

/* Source: CoDUOMP.exe 0x004a6e90..0x004a6ed8.
 * Name: same-module Mac symbol CFxScheduler::PlayEffect(int, origin, forward).
 * The Windows register allocation carries forward in EAX. */
void CFxScheduler_PlayEffectID(cfx_scheduler_t *scheduler,
                               int32_t effectId,
                               const vec3_t origin,
                               const vec3_t forward)
{
    axis_t axis;
    for (int component = 0; component < 3; ++component) {
        axis[0][component] = forward[component];
    }
    /* MakeNormalVectors writes every component of the right and up outputs;
     * axis[1] and axis[2] are therefore output storage, not input state. */
    MakeNormalVectors(axis[0], axis[1], axis[2]);
    CFxScheduler_PlayEntityEffectID(scheduler, effectId, origin,
                                    axis, nullptr);
}

/* Source: CoDUOMP.exe 0x004a6ee0..0x004a6fb4.
 * Name: same-module Mac symbol
 * CFxScheduler::PlayEffect(char const *, origin, axis, boltInfo).  Playback
 * strips a filename extension but deliberately does not lowercase the name;
 * std::map::operator[] preserves the original zero-ID result and insertion for
 * an unregistered spelling. */
void CFxScheduler_PlayEntityEffect(cfx_scheduler_t *scheduler,
                                   const char *name,
                                   const vec3_t origin,
                                   const axis_t axis,
                                   const sfx_bolt_info_t *boltInfo)
{
    char effectName[FX_EFFECT_TEMPLATE_NAME_CAPACITY];
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    const size_t effectNameLength = strcspn(name, ".");
    if (effectNameLength >= sizeof(effectName)) {
        CFxScheduler_PlayEntityEffectID(scheduler, 0, origin, axis, boltInfo);
        return;
    }
    memcpy(effectName, name, effectNameLength);
    effectName[effectNameLength] = '\0';

    const int32_t effectId = scheduler->effectIdsByName[effectName];
    CFxScheduler_PlayEntityEffectID(scheduler, effectId, origin, axis,
                                    boltInfo);
}

/* Source: CoDUOMP.exe 0x004a7400..0x004a74c8.
 * Name: same-module Mac symbol
 * CFxScheduler::PlayEffect(char const *, origin). */
void CFxScheduler_PlaySimpleEffect(cfx_scheduler_t *scheduler,
                                   const char *name,
                                   const vec3_t origin)
{
    char effectName[FX_EFFECT_TEMPLATE_NAME_CAPACITY];
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    const size_t effectNameLength = strcspn(name, ".");
    if (effectNameLength >= sizeof(effectName)) {
        CFxScheduler_PlaySimpleEffectID(scheduler, 0, origin);
        return;
    }
    memcpy(effectName, name, effectNameLength);
    effectName[effectNameLength] = '\0';

    const int32_t effectId = scheduler->effectIdsByName[effectName];
    CFxScheduler_PlaySimpleEffectID(scheduler, effectId, origin);
}

/* Source: CoDUOMP.exe 0x004a74d0..0x004a75e7.
 * Name: same-module Mac symbol
 * CFxScheduler::PlayEffect(char const *, origin, forward). */
void CFxScheduler_PlayEffect(cfx_scheduler_t *scheduler,
                             const char *name,
                             const vec3_t origin,
                             const vec3_t forward)
{
    char effectName[FX_EFFECT_TEMPLATE_NAME_CAPACITY];
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    const size_t effectNameLength = strcspn(name, ".");
    if (effectNameLength >= sizeof(effectName)) {
        CFxScheduler_PlayEffectID(scheduler, 0, origin, forward);
        return;
    }
    memcpy(effectName, name, effectNameLength);
    effectName[effectNameLength] = '\0';

    const int32_t effectId = scheduler->effectIdsByName[effectName];
    axis_t axis;
    for (int component = 0; component < 3; ++component) {
        axis[0][component] = forward[component];
    }
    MakeNormalVectors(axis[0], axis[1], axis[2]);
    CFxScheduler_PlayEntityEffectID(scheduler, effectId, origin,
                                    axis, nullptr);
}

/* Source: CoDUOMP.exe 0x004a6e10..0x004a6e1e.
 * Name and signature: exact same-module Mac symbol
 * ReportPlayEffectError(int). */
void ReportPlayEffectError(int32_t effectId)
{
    SFxHelper_Print(
        "^3CFxScheduler::PlayEffect called with invalid effect ID: %i\n",
        effectId);
}

/* Source: CoDUOMP.exe 0x004a6fc0..0x004a73f0.
 * Name and signature: same-module Mac symbol
 * CFxScheduler::PlayEffect(int, origin, axis, boltInfo).  Invalid IDs fall
 * back through the singleton scheduler's fx/error.efx template.  Delayed
 * instances retain precisely the orientation data needed by the later bolt
 * resolution path and are pushed at the front of the scheduler list. */
void CFxScheduler::PlayEntityEffectID(
    int32_t effectId, const vec3_t origin, const axis_t axis,
    const sfx_bolt_info_t *boltInfo)
{
    if (effectId < 1 || effectId >= FX_EFFECT_TEMPLATE_COUNT ||
        effectTemplates[effectId].active == 0) {
        RE_SetIgnorePrecacheErrors(qtrue);
        effectId = fxScheduler.RegisterEffect("fx/error.efx", false);
        RE_SetIgnorePrecacheErrors(qfalse);
        if (effectId == 0) {
            ReportPlayEffectError(effectId);
            return;
        }
    }

    if (fx_freeze->integer != 0 || fx_enable->integer == 0) {
        return;
    }

    orientation_t effectOrientation;
    if (boltInfo != nullptr && boltInfo->entityNum >= 0) {
        if (FX_GetBoneOrientation(boltInfo, &effectOrientation) == qfalse) {
            return;
        }
    } else {
        if (origin != nullptr) {
            for (int32_t component = 0; component < 3; ++component) {
                effectOrientation.origin[component] = origin[component];
            }
        } else {
            for (int32_t component = 0; component < 3; ++component) {
                effectOrientation.origin[component] = 0.0f;
            }
        }
        for (int32_t row = 0; row < 3; ++row) {
            for (int32_t component = 0; component < 3; ++component) {
                effectOrientation.axis[row][component] =
                    axis[row][component];
            }
        }
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    const float *const cullOrigin =
        origin != nullptr ? origin : effectOrientation.origin;

    sfx_effect_template_t *effectTemplate = &effectTemplates[effectId];
    int32_t totalSpawnCount = 0;
    for (int32_t primitiveIndex = 0;
         primitiveIndex < effectTemplate->primitiveCount;
         ++primitiveIndex) {
        ++fxPrimitiveTemplatePlayCount;
        CPrimitiveTemplate *primitiveTemplate =
            effectTemplate->primitives[primitiveIndex];

        if (primitiveTemplate->cullRange != 0) {
            const float cullDistance =
                static_cast<float>(primitiveTemplate->cullRange) *
                    fx_cullscale->value +
                static_cast<float>(fx_cullbias->integer);
            /* 0x004a7134..0x004a7157 subtracts through EBP, which was loaded
             * from the caller's origin argument at 0x004a7078/0x004a7081.
             * Bolted effects still use effectOrientation for spawning, but
             * retail deliberately performs this distance gate against the
             * supplied origin rather than the resolved bone position whenever
             * that pointer exists. The security fallback above affects only
             * the original NULL domain. */
            const float deltaZ = fxViewOrigin[2] - cullOrigin[2];
            const float deltaY = fxViewOrigin[1] - cullOrigin[1];
            const float deltaX = fxViewOrigin[0] - cullOrigin[0];
            const float distanceSquared =
                (deltaZ * deltaZ + deltaY * deltaY) + deltaX * deltaX;
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (distanceSquared > cullDistance * cullDistance) {
                if (primitiveTemplate->temporary) {
                    delete primitiveTemplate;
                    effectTemplate->primitives[primitiveIndex] = nullptr;
                }
                continue;
            }
        }

        const int32_t spawnCount =
            FX_RandomIntRange(primitiveTemplate->count);
        if (primitiveTemplate->temporary) {
            primitiveTemplate->remainingSpawnCount = spawnCount;
        }
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (primitiveTemplate->temporary && spawnCount <= 0) {
            totalSpawnCount += spawnCount;
            delete primitiveTemplate;
            effectTemplate->primitives[primitiveIndex] = nullptr;
            continue;
        }

        float evenSpawnDelay = 0.0f;
        if ((primitiveTemplate->spawnFlags &
             FX_SPAWNFLAG_EVEN_DISTRIBUTION) != 0) {
            evenSpawnDelay =
                fabsf(primitiveTemplate->delay.end -
                      primitiveTemplate->delay.start) /
                static_cast<float>(spawnCount);
        }

        totalSpawnCount += spawnCount;
        for (int32_t spawnIndex = 0;
             spawnIndex < spawnCount;
             ++spawnIndex) {
            ++fxPrimitiveSpawnCount;
            float spawnDelay;
            if ((primitiveTemplate->spawnFlags &
                 FX_SPAWNFLAG_EVEN_DISTRIBUTION) != 0) {
                spawnDelay =
                    primitiveTemplate->delay.start +
                    static_cast<float>(spawnIndex) * evenSpawnDelay;
            } else {
                spawnDelay = FX_RandomFloatRange(primitiveTemplate->delay);
            }

            const int32_t delayMilliseconds =
                coduo_fp_to_i32_extended((long double)spawnDelay);
            if (delayMilliseconds < 1) {
                CreateEffect(primitiveTemplate, boltInfo,
                             effectOrientation.origin,
                             effectOrientation.axis,
                             -delayMilliseconds);
                continue;
            }

            sfx_scheduled_effect_t *scheduledEffect =
                new sfx_scheduled_effect_t;
            if (scheduledEffect == nullptr) {
                /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                primitiveTemplate->coduomp_retire_pending_spawn();
                continue;
            }

            scheduledEffect->effectId = effectId;
            scheduledEffect->primitiveIndex = primitiveIndex;
            scheduledEffect->scheduledTime =
                fxCurrentTime + delayMilliseconds;

            if (boltInfo != nullptr) {
                scheduledEffect->boltInfo = *boltInfo;
                if (boltInfo->entityNum >= 0) {
                    if (boltInfo->boneIndex < 0) {
                        for (int32_t row = 0; row < 3; ++row) {
                            for (int32_t component = 0;
                                 component < 3;
                                 ++component) {
                                scheduledEffect->axis[row][component] =
                                    axis[row][component];
                            }
                        }
                    }
                    scheduledEffects.push_front(scheduledEffect);
                    continue;
                }
            } else {
                scheduledEffect->boltInfo.entityNum = -1;
                scheduledEffect->boltInfo.boneIndex = -1;
            }

            if (origin != nullptr) {
                for (int32_t component = 0; component < 3; ++component) {
                    scheduledEffect->origin[component] = origin[component];
                }
            } else {
                for (int32_t component = 0; component < 3; ++component) {
                    scheduledEffect->origin[component] = 0.0f;
                }
            }
            for (int32_t row = 0; row < 3; ++row) {
                for (int32_t component = 0; component < 3; ++component) {
                    scheduledEffect->axis[row][component] =
                        axis[row][component];
                }
            }
            scheduledEffects.push_front(scheduledEffect);
        }
    }

    if (totalSpawnCount != 0 && fx_count->integer != 0) {
        RE_AddPlume(effectOrientation.origin, totalSpawnCount,
                    fxSpawnCountPlumeColor,
                    FX_DEBUG_PLUME_DURATION_MSEC);
    }
    if (effectTemplate->temporary != 0) {
        effectTemplate->active = 0;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: portable C-style boundary retained for the
 * mixed C/C++ recovered callers; the original body is the member above. */
void CFxScheduler_PlayEntityEffectID(cfx_scheduler_t *scheduler,
                                     int32_t effectId,
                                     const vec3_t origin,
                                     const axis_t axis,
                                     const sfx_bolt_info_t *boltInfo)
{
    scheduler->PlayEntityEffectID(effectId, origin, axis, boltInfo);
}

/* Source: CoDUOMP.exe 0x004a7660..0x004a776e.
 * Name and signature: same-module Mac symbol
 * CFxScheduler::AddScheduledEffects.  The list is not time-sorted: every
 * record is examined, due records are dispatched or discarded, and future
 * records retain their relative order. */
void CFxScheduler::AddScheduledEffects()
{
    if (fx_enable->integer == 0) {
        return;
    }

    auto scheduledIt = scheduledEffects.begin();
    while (scheduledIt != scheduledEffects.end()) {
        auto currentIt = scheduledIt++;
        sfx_scheduled_effect_t *scheduledEffect = *currentIt;
        if (scheduledEffect->scheduledTime > fxCurrentTime) {
            continue;
        }

        CPrimitiveTemplate *primitiveTemplate =
            effectTemplates[scheduledEffect->effectId]
                .primitives[scheduledEffect->primitiveIndex];
        if (scheduledEffect->boltInfo.entityNum >= 0) {
            orientation_t boltOrientation;
            if (FX_GetBoneOrientation(&scheduledEffect->boltInfo,
                                      &boltOrientation) != qfalse) {
                CreateEffect(
                    primitiveTemplate, &scheduledEffect->boltInfo,
                    boltOrientation.origin, boltOrientation.axis,
                    fxCurrentTime - scheduledEffect->scheduledTime);
            } else {
                /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                SFxHelper_Print(
                    "^3Effect play failed: %s: Could not get bone orientation\n",
                    effectTemplates[scheduledEffect->effectId].name);
                primitiveTemplate->coduomp_retire_pending_spawn();
            }
        } else {
            CreateEffect(
                primitiveTemplate, &scheduledEffect->boltInfo,
                scheduledEffect->origin, scheduledEffect->axis,
                fxCurrentTime - scheduledEffect->scheduledTime);
        }

        delete scheduledEffect;
        scheduledEffects.erase(currentIt);
    }

    FX_Add();
}

/* Source: CoDUOMP.exe 0x004a7770..0x004a929d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004a7770_004a929e.mcode.
 * Name and complete member signature: same-module Mac symbol
 * CFxScheduler::CreateEffect. Direct coduo_crt_rand calls and the shared
 * game/FX LCG range helpers are intentionally kept as separate random streams,
 * exactly as in the Windows routine. */
void CFxScheduler::CreateEffect(CPrimitiveTemplate *primitiveTemplate,
                                const sfx_bolt_info_t *boltInfo,
                                const vec3_t origin, const axis_t axis,
                                int32_t timeOffset)
{
    axis_t spawnAxis;
    for (int32_t row = 0; row < 3; ++row) {
        for (int32_t component = 0; component < 3; ++component)
            spawnAxis[row][component] = axis[row][component];
    }

    if ((primitiveTemplate->spawnFlags &
         FX_SPAWNFLAG_RANDOM_ROTATION_AROUND_FORWARD) != 0) {
        vec3_t rotatedRight;
        RotatePointAroundVector(rotatedRight, spawnAxis[0], spawnAxis[1],
                                coduo_crt_randf() * 360.0f);
        for (int32_t component = 0; component < 3; ++component)
            spawnAxis[1][component] = rotatedRight[component];
        spawnAxis[2][0] = spawnAxis[0][1] * spawnAxis[1][2] -
                          spawnAxis[0][2] * spawnAxis[1][1];
        spawnAxis[2][1] = spawnAxis[0][2] * spawnAxis[1][0] -
                          spawnAxis[0][0] * spawnAxis[1][2];
        spawnAxis[2][2] = spawnAxis[0][0] * spawnAxis[1][1] -
                          spawnAxis[0][1] * spawnAxis[1][0];
    }

    vec3_t spawnOrigin;
    FX_SampleVectorRange(
        primitiveTemplate->origin1, spawnAxis,
        (primitiveTemplate->spawnFlags & FX_SPAWNFLAG_CHEAP_ORIGIN) != 0,
        spawnOrigin);
    for (int32_t component = 0; component < 3; ++component)
        spawnOrigin[component] += origin[component];

    if ((primitiveTemplate->spawnFlags &
         FX_SPAWNFLAG_ORIGIN_ON_SPHERE) != 0) {
        const float azimuth = coduo_crt_randf() * 360.0f *
                              3.1415927410125732f / 180.0f;
        float sinAzimuth;
        float cosAzimuth;
        coduo_x87_sincosf(azimuth, &sinAzimuth, &cosAzimuth);
        const float polar = coduo_crt_randf() * 180.0f *
                            3.1415927410125732f / 180.0f;
        float sinPolar;
        float cosPolar;
        coduo_x87_sincosf(polar, &sinPolar, &cosPolar);
        const float radius = FX_RandomFloatRange(primitiveTemplate->radius);
        const float height = FX_RandomFloatRange(primitiveTemplate->height);
        vec3_t radialOffset = {
            radius * sinPolar * sinAzimuth,
            radius * sinPolar * cosAzimuth,
            height * cosPolar
        };
        for (int32_t component = 0; component < 3; ++component)
            spawnOrigin[component] += radialOffset[component];

        if ((primitiveTemplate->spawnFlags &
             FX_SPAWNFLAG_AXIS_FROM_SPHERE) != 0) {
            VectorNormalize2(radialOffset, spawnAxis[0]);
            MakeNormalVectors(spawnAxis[0], spawnAxis[1], spawnAxis[2]);
        }
    } else if ((primitiveTemplate->spawnFlags &
                FX_SPAWNFLAG_ORIGIN_ON_CYLINDER) != 0) {
        const float height = FX_RandomFloatRange(primitiveTemplate->height);
        const float axialOffset =
            (coduo_crt_randf() * 2.0f - 1.0f) * height * 0.5f;
        const float radius = FX_RandomFloatRange(primitiveTemplate->radius);
        vec3_t unrotatedOffset;
        for (int32_t component = 0; component < 3; ++component) {
            unrotatedOffset[component] =
                spawnAxis[1][component] * radius +
                spawnAxis[0][component] * axialOffset;
        }
        vec3_t radialOffset;
        RotatePointAroundVector(radialOffset, spawnAxis[0],
                                unrotatedOffset,
                                coduo_crt_randf() * 360.0f);
        for (int32_t component = 0; component < 3; ++component)
            spawnOrigin[component] += radialOffset[component];

        if ((primitiveTemplate->spawnFlags &
             FX_SPAWNFLAG_AXIS_FROM_SPHERE) != 0) {
            VectorNormalize2(radialOffset, spawnAxis[0]);
            vec3_t cardinal = {0.0f, 0.0f, 1.0f};
            if (spawnAxis[0][2] == 1.0f) {
                cardinal[1] = 1.0f;
                cardinal[2] = 0.0f;
            }
            spawnAxis[1][0] = cardinal[1] * spawnAxis[0][2] -
                              cardinal[2] * spawnAxis[0][1];
            spawnAxis[1][1] = cardinal[2] * spawnAxis[0][0] -
                              cardinal[0] * spawnAxis[0][2];
            spawnAxis[1][2] = cardinal[0] * spawnAxis[0][1] -
                              cardinal[1] * spawnAxis[0][0];
            spawnAxis[2][0] = spawnAxis[0][1] * spawnAxis[1][2] -
                              spawnAxis[0][2] * spawnAxis[1][1];
            spawnAxis[2][1] = spawnAxis[0][2] * spawnAxis[1][0] -
                              spawnAxis[0][0] * spawnAxis[1][2];
            spawnAxis[2][2] = spawnAxis[0][0] * spawnAxis[1][1] -
                              spawnAxis[0][1] * spawnAxis[1][0];
        }
    }

    vec3_t velocity;
    vec3_t velocityGoal;
    vec3_t acceleration;
    const bool movingPrimitive =
        primitiveTemplate->primitiveType == FX_PRIMITIVE_TYPE_PARTICLE ||
        primitiveTemplate->primitiveType ==
            FX_PRIMITIVE_TYPE_ORIENTED_PARTICLE ||
        primitiveTemplate->primitiveType == FX_PRIMITIVE_TYPE_TAIL ||
        primitiveTemplate->primitiveType == FX_PRIMITIVE_TYPE_EMITTER;
    if (movingPrimitive) {
        FX_SampleVectorRange(primitiveTemplate->velocityClamp, spawnAxis,
                             true, velocityGoal);
        if ((primitiveTemplate->flags &
             FX_PRIMITIVE_FLAG_CLAMP_VELOCITY_X) == 0)
            velocityGoal[0] = 0.0f;
        if ((primitiveTemplate->flags &
             FX_PRIMITIVE_FLAG_CLAMP_VELOCITY_Y) == 0)
            velocityGoal[1] = 0.0f;
        if ((primitiveTemplate->flags &
             FX_PRIMITIVE_FLAG_CLAMP_VELOCITY_Z) == 0)
            velocityGoal[2] = 0.0f;

        FX_SampleVectorRange(
            primitiveTemplate->velocity, spawnAxis,
            (primitiveTemplate->spawnFlags &
             FX_SPAWNFLAG_ABSOLUTE_VELOCITY) != 0,
            velocity);
        if ((primitiveTemplate->spawnFlags &
             FX_SPAWNFLAG_AFFECTED_BY_WIND) != 0) {
            const float windScale =
                FX_RandomFloatRange(primitiveTemplate->windModifier) *
                0.009999999776482582f;
            for (int32_t component = 0; component < 3; ++component)
                velocity[component] +=
                    fx_windDirection[component] * windScale;
        }

        FX_SampleVectorRange(
            primitiveTemplate->acceleration, spawnAxis,
            (primitiveTemplate->spawnFlags &
             FX_SPAWNFLAG_ABSOLUTE_ACCELERATION) != 0,
            acceleration);
        if ((primitiveTemplate->spawnFlags &
             FX_SPAWNFLAG_OPPOSITE_ACCELERATION) != 0) {
            constexpr float oppositeAccelerationThreshold =
                1.0000000116860974e-7f; /* 0x005b9f28 */
            for (int32_t component = 0; component < 3; ++component) {
                if (velocity[component] * acceleration[component] >
                    oppositeAccelerationThreshold) {
                    acceleration[component] = -acceleration[component];
                }
            }
        }
        acceleration[2] +=
            FX_RandomFloatRange(primitiveTemplate->gravity);

        if (timeOffset > 0) {
            const float elapsed = static_cast<float>(timeOffset) *
                                  0.0010000000474974513f;
            const float positionScale =
                elapsed * elapsed * 0.5f + elapsed;
            for (int32_t component = 0; component < 3; ++component)
                velocity[component] += acceleration[component] * elapsed;
            for (int32_t component = 0; component < 3; ++component) {
                spawnOrigin[component] +=
                    velocity[component] * positionScale;
            }
        }
    }

    vec3_t secondOrigin;
    if (primitiveTemplate->primitiveType == FX_PRIMITIVE_TYPE_LINE ||
        primitiveTemplate->primitiveType == FX_PRIMITIVE_TYPE_ELECTRICITY) {
        if ((primitiveTemplate->spawnFlags &
             FX_SPAWNFLAG_ORIGIN2_FROM_TRACE) != 0) {
            vec3_t traceEnd;
            for (int32_t component = 0; component < 3; ++component) {
                traceEnd[component] =
                    spawnOrigin[component] +
                    spawnAxis[0][component] * 16384.0f;
            }
            if ((primitiveTemplate->spawnFlags &
                 FX_SPAWNFLAG_ORIGIN2_IS_OFFSET) != 0) {
                vec3_t traceOffset;
                FX_SampleVectorRange(
                    primitiveTemplate->origin2, spawnAxis,
                    (primitiveTemplate->spawnFlags &
                     FX_SPAWNFLAG_CHEAP_ORIGIN2) != 0,
                    traceOffset);
                for (int32_t component = 0; component < 3; ++component)
                    traceEnd[component] += traceOffset[component];
            }

            trace_t trace;
            SFxHelper_Trace(&trace, spawnOrigin, vec3_origin, vec3_origin,
                            traceEnd, 0, CONTENTS_SOLID);
            for (int32_t component = 0; component < 3; ++component)
                secondOrigin[component] = trace.endpos[component];

            if ((primitiveTemplate->spawnFlags &
                 FX_SPAWNFLAG_TRACE_IMPACT_EFFECT) != 0) {
                const int32_t impactEffect =
                    FX_RandomElement(primitiveTemplate->impactEffects);
                axis_t impactAxis;
                for (int32_t component = 0; component < 3; ++component)
                    impactAxis[0][component] = trace.normal[component];
                MakeNormalVectors(impactAxis[0], impactAxis[1],
                                  impactAxis[2]);
                PlayEntityEffectID(impactEffect, trace.endpos, impactAxis,
                                   nullptr);
            }
        } else {
            FX_SampleVectorRange(
                primitiveTemplate->origin2, spawnAxis,
                (primitiveTemplate->spawnFlags &
                 FX_SPAWNFLAG_CHEAP_ORIGIN2) != 0,
                secondOrigin);
            for (int32_t component = 0; component < 3; ++component)
                secondOrigin[component] += origin[component];
        }
    }

    vec3_t colorStart;
    vec3_t colorEnd;
    if (primitiveTemplate->primitiveType != FX_PRIMITIVE_TYPE_SOUND &&
        primitiveTemplate->primitiveType != FX_PRIMITIVE_TYPE_FX_RUNNER &&
        primitiveTemplate->primitiveType !=
            FX_PRIMITIVE_TYPE_CAMERA_SHAKE) {
        if ((primitiveTemplate->spawnFlags &
             FX_SPAWNFLAG_RGB_COMPONENT_INTERPOLATION) != 0) {
            const float fraction = coduo_crt_randf();
            for (int32_t component = 0; component < 3; ++component) {
                colorStart[component] =
                    (primitiveTemplate->rgb.start.end[component] -
                     primitiveTemplate->rgb.start.start[component]) *
                        fraction +
                    primitiveTemplate->rgb.start.start[component];
                colorEnd[component] =
                    (primitiveTemplate->rgb.end.end[component] -
                     primitiveTemplate->rgb.end.start[component]) *
                        fraction +
                    primitiveTemplate->rgb.end.start[component];
            }
        } else {
            for (int32_t component = 0; component < 3; ++component) {
                const fx_float_range_t startRange = {
                    primitiveTemplate->rgb.start.start[component],
                    primitiveTemplate->rgb.start.end[component]
                };
                colorStart[component] = FX_RandomFloatRange(startRange);
            }
            for (int32_t component = 0; component < 3; ++component) {
                const fx_float_range_t endRange = {
                    primitiveTemplate->rgb.end.start[component],
                    primitiveTemplate->rgb.end.end[component]
                };
                colorEnd[component] = FX_RandomFloatRange(endRange);
            }
        }
    }

    cfx_bolt_frame_ptr_t boltFrame = {nullptr};
    if ((primitiveTemplate->flags & FX_PRIMITIVE_FLAG_RELATIVE) != 0 &&
        boltInfo != nullptr && boltInfo->entityNum >= 0) {
        boltFrame.frame = CFxBoltFrame_Acquire(boltInfo);
        if (boltFrame.frame == nullptr ||
            CFxBoltFrame_GetOrientation(boltFrame.frame) == nullptr) {
            if (boltFrame.frame != nullptr)
                CFxBoltFrame_Release(boltFrame.frame);
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            primitiveTemplate->coduomp_retire_pending_spawn();
            return;
        }
    }

    const float rotation =
        FX_RandomFloatRange(primitiveTemplate->rotation);
    float rotationDelta =
        FX_RandomFloatRange(primitiveTemplate->rotationDelta);
    float rotationAccel =
        FX_RandomFloatRange(primitiveTemplate->rotationAccel);
    const float rotationClamp =
        FX_RandomFloatRange(primitiveTemplate->rotationClamp);
    if ((primitiveTemplate->spawnFlags &
         FX_SPAWNFLAG_OPPOSITE_ROTATION) != 0 &&
        rotationAccel * rotationDelta > 0.0f) {
        rotationAccel = -rotationAccel;
    }
    if ((primitiveTemplate->flags & FX_PRIMITIVE_FLAG_CLAMP_ROTATION) != 0) {
        if ((rotationDelta > rotationClamp && rotationAccel > 0.0f) ||
            (rotationDelta < rotationClamp && rotationAccel < 0.0f)) {
            rotationDelta = rotationClamp;
            rotationAccel = 0.0f;
        }
    }

    switch (primitiveTemplate->primitiveType) {
    case FX_PRIMITIVE_TYPE_PARTICLE: {
        const fx_template_resource_t resource =
            FX_RandomElement(primitiveTemplate->resources);
        const int32_t lifetime = coduo_fp_to_i32_extended(
            (long double)FX_RandomFloatRange(primitiveTemplate->life));
        const int32_t impactEffect =
            FX_RandomElement(primitiveTemplate->impactEffects);
        const int32_t deathEffect =
            FX_RandomElement(primitiveTemplate->deathEffects);
        const float elasticity =
            FX_RandomFloatRange(primitiveTemplate->elasticity.parm);
        const float colorParm =
            FX_RandomFloatRange(primitiveTemplate->rgb.parm);
        const float alphaParm =
            FX_RandomFloatRange(primitiveTemplate->alpha.parm);
        const float alphaEnd =
            FX_RandomFloatRange(primitiveTemplate->alpha.end);
        const float alphaStart =
            FX_RandomFloatRange(primitiveTemplate->alpha.start);
        const float size2Parm =
            FX_RandomFloatRange(primitiveTemplate->size2.parm);
        const float size2End =
            FX_RandomFloatRange(primitiveTemplate->size2.end);
        const float size2Start =
            FX_RandomFloatRange(primitiveTemplate->size2.start);
        const float sizeParm =
            FX_RandomFloatRange(primitiveTemplate->size.parm);
        const float sizeEnd =
            FX_RandomFloatRange(primitiveTemplate->size.end);
        const float sizeStart =
            FX_RandomFloatRange(primitiveTemplate->size.start);
        (void)FX_AddParticle(
            boltFrame, spawnOrigin, velocity, velocityGoal, acceleration,
            primitiveTemplate->nonUniformScale,
            sizeStart, sizeEnd, sizeParm,
            size2Start, size2End, size2Parm,
            alphaStart, alphaEnd, alphaParm,
            colorStart, colorEnd, colorParm,
            rotation, rotationDelta, rotationAccel, rotationClamp,
            primitiveTemplate->boundsMin, primitiveTemplate->boundsMax,
            elasticity, deathEffect, impactEffect, lifetime,
            resource.handle, primitiveTemplate->flags,
            primitiveTemplate->parameterFlags);
        break;
    }
    case FX_PRIMITIVE_TYPE_LINE: {
        const fx_template_resource_t resource =
            FX_RandomElement(primitiveTemplate->resources);
        const int32_t lifetime = coduo_fp_to_i32_extended(
            (long double)FX_RandomFloatRange(primitiveTemplate->life));
        const float colorParm =
            FX_RandomFloatRange(primitiveTemplate->rgb.parm);
        const float alphaParm =
            FX_RandomFloatRange(primitiveTemplate->alpha.parm);
        const float alphaEnd =
            FX_RandomFloatRange(primitiveTemplate->alpha.end);
        const float alphaStart =
            FX_RandomFloatRange(primitiveTemplate->alpha.start);
        const float sizeParm =
            FX_RandomFloatRange(primitiveTemplate->size.parm);
        const float sizeEnd =
            FX_RandomFloatRange(primitiveTemplate->size.end);
        const float sizeStart =
            FX_RandomFloatRange(primitiveTemplate->size.start);
        (void)FX_AddLine(boltFrame, spawnOrigin, secondOrigin,
                         sizeStart, sizeEnd, sizeParm,
                         alphaStart, alphaEnd, alphaParm,
                         colorStart, colorEnd, colorParm,
                         lifetime, resource.handle, primitiveTemplate->flags,
                         primitiveTemplate->parameterFlags);
        break;
    }
    case FX_PRIMITIVE_TYPE_TAIL: {
        const fx_template_resource_t resource =
            FX_RandomElement(primitiveTemplate->resources);
        const int32_t lifetime = coduo_fp_to_i32_extended(
            (long double)FX_RandomFloatRange(primitiveTemplate->life));
        const int32_t impactEffect =
            FX_RandomElement(primitiveTemplate->impactEffects);
        const int32_t deathEffect =
            FX_RandomElement(primitiveTemplate->deathEffects);
        const float elasticity =
            FX_RandomFloatRange(primitiveTemplate->elasticity.parm);
        const float colorParm =
            FX_RandomFloatRange(primitiveTemplate->rgb.parm);
        const float alphaParm =
            FX_RandomFloatRange(primitiveTemplate->alpha.parm);
        const float alphaEnd =
            FX_RandomFloatRange(primitiveTemplate->alpha.end);
        const float alphaStart =
            FX_RandomFloatRange(primitiveTemplate->alpha.start);
        const float lengthParm =
            FX_RandomFloatRange(primitiveTemplate->length.parm);
        const float lengthEnd =
            FX_RandomFloatRange(primitiveTemplate->length.end);
        const float lengthStart =
            FX_RandomFloatRange(primitiveTemplate->length.start);
        const float sizeParm =
            FX_RandomFloatRange(primitiveTemplate->size.parm);
        const float sizeEnd =
            FX_RandomFloatRange(primitiveTemplate->size.end);
        const float sizeStart =
            FX_RandomFloatRange(primitiveTemplate->size.start);
        (void)FX_AddTail(
            boltFrame, spawnOrigin, velocity, velocityGoal, acceleration,
            sizeStart, sizeEnd, sizeParm,
            lengthStart, lengthEnd, lengthParm,
            alphaStart, alphaEnd, alphaParm,
            colorStart, colorEnd, colorParm,
            primitiveTemplate->boundsMin, primitiveTemplate->boundsMax,
            elasticity, deathEffect, impactEffect, lifetime,
            resource.handle, primitiveTemplate->flags,
            primitiveTemplate->parameterFlags);
        break;
    }
    case FX_PRIMITIVE_TYPE_CYLINDER: {
        const fx_template_resource_t resource =
            FX_RandomElement(primitiveTemplate->resources);
        const int32_t lifetime = coduo_fp_to_i32_extended(
            (long double)FX_RandomFloatRange(primitiveTemplate->life));
        const float colorParm =
            FX_RandomFloatRange(primitiveTemplate->rgb.parm);
        const float alphaParm =
            FX_RandomFloatRange(primitiveTemplate->alpha.parm);
        const float alphaEnd =
            FX_RandomFloatRange(primitiveTemplate->alpha.end);
        const float alphaStart =
            FX_RandomFloatRange(primitiveTemplate->alpha.start);
        const float lengthParm =
            FX_RandomFloatRange(primitiveTemplate->length.parm);
        const float lengthEnd =
            FX_RandomFloatRange(primitiveTemplate->length.end);
        const float lengthStart =
            FX_RandomFloatRange(primitiveTemplate->length.start);
        const float size2Parm =
            FX_RandomFloatRange(primitiveTemplate->size2.parm);
        const float size2End =
            FX_RandomFloatRange(primitiveTemplate->size2.end);
        const float size2Start =
            FX_RandomFloatRange(primitiveTemplate->size2.start);
        const float sizeParm =
            FX_RandomFloatRange(primitiveTemplate->size.parm);
        const float sizeEnd =
            FX_RandomFloatRange(primitiveTemplate->size.end);
        const float sizeStart =
            FX_RandomFloatRange(primitiveTemplate->size.start);
        (void)FX_AddCylinder(
            boltFrame, spawnOrigin, spawnAxis[0],
            sizeStart, sizeEnd, sizeParm,
            size2Start, size2End, size2Parm,
            lengthStart, lengthEnd, lengthParm,
            alphaStart, alphaEnd, alphaParm,
            colorStart, colorEnd, colorParm,
            lifetime, resource.handle, primitiveTemplate->flags,
            primitiveTemplate->parameterFlags);
        break;
    }
    case FX_PRIMITIVE_TYPE_EMITTER: {
        vec3_t angleOffset;
        for (int32_t component = 0; component < 3; ++component) {
            const fx_float_range_t range = {
                primitiveTemplate->angle.start[component],
                primitiveTemplate->angle.end[component]
            };
            angleOffset[component] = FX_RandomFloatRange(range);
        }
        vec3_t angles;
        vectoangles(spawnAxis[0], angles);
        for (int32_t component = 0; component < 3; ++component)
            angles[component] += angleOffset[component];
        vec3_t angularVelocity;
        for (int32_t component = 0; component < 3; ++component) {
            const fx_float_range_t range = {
                primitiveTemplate->angleDelta.start[component],
                primitiveTemplate->angleDelta.end[component]
            };
            angularVelocity[component] = FX_RandomFloatRange(range);
        }

        const fx_template_resource_t resource =
            FX_RandomElement(primitiveTemplate->resources);
        const int32_t lifetime = coduo_fp_to_i32_extended(
            (long double)FX_RandomFloatRange(primitiveTemplate->life));
        const float variance =
            FX_RandomFloatRange(primitiveTemplate->variance);
        const float density =
            FX_RandomFloatRange(primitiveTemplate->density);
        const int32_t emitterEffect =
            FX_RandomElement(primitiveTemplate->emitterEffects);
        const int32_t impactEffect =
            FX_RandomElement(primitiveTemplate->impactEffects);
        const int32_t deathEffect =
            FX_RandomElement(primitiveTemplate->deathEffects);
        const float elasticity =
            FX_RandomFloatRange(primitiveTemplate->elasticity.parm);
        const float colorParm =
            FX_RandomFloatRange(primitiveTemplate->rgb.parm);
        const float alphaParm =
            FX_RandomFloatRange(primitiveTemplate->alpha.parm);
        const float alphaEnd =
            FX_RandomFloatRange(primitiveTemplate->alpha.end);
        const float alphaStart =
            FX_RandomFloatRange(primitiveTemplate->alpha.start);
        const float sizeParm =
            FX_RandomFloatRange(primitiveTemplate->size.parm);
        const float sizeEnd =
            FX_RandomFloatRange(primitiveTemplate->size.end);
        const float sizeStart =
            FX_RandomFloatRange(primitiveTemplate->size.start);
        (void)FX_AddEmitter(
            boltFrame, spawnOrigin, velocity, velocityGoal, acceleration,
            sizeStart, sizeEnd, sizeParm,
            alphaStart, alphaEnd, alphaParm,
            colorStart, colorEnd, colorParm,
            angles, angularVelocity,
            primitiveTemplate->boundsMin, primitiveTemplate->boundsMax,
            elasticity, deathEffect, impactEffect, emitterEffect,
            density, variance, lifetime, resource.model,
            primitiveTemplate->flags, primitiveTemplate->parameterFlags);
        break;
    }
    case FX_PRIMITIVE_TYPE_SOUND: {
        const fx_template_resource_t resource =
            FX_RandomElement(primitiveTemplate->resources);
        SFxHelper_PlaySound(spawnOrigin, ENTITYNUM_NONE, resource.handle);
        break;
    }
    case FX_PRIMITIVE_TYPE_DECAL: {
        const float alpha =
            FX_RandomFloatRange(primitiveTemplate->alpha.start);
        const float radius =
            FX_RandomFloatRange(primitiveTemplate->size.start);
        const int32_t temporary = coduo_fp_to_i32_extended(
            (long double)FX_RandomFloatRange(primitiveTemplate->life));
        const fx_template_resource_t resource =
            FX_RandomElement(primitiveTemplate->resources);
        (void)VM_Call(
            coduo_cgameVm, CGVM_IMPACT_MARK,
            resource.handle, reinterpret_cast<intptr_t>(spawnOrigin),
            reinterpret_cast<intptr_t>(spawnAxis[0]),
            FX_FloatVmArgument(rotation), FX_FloatVmArgument(colorStart[0]),
            FX_FloatVmArgument(colorStart[1]),
            FX_FloatVmArgument(colorStart[2]), FX_FloatVmArgument(alpha),
            1, FX_FloatVmArgument(radius), 0, temporary);
        break;
    }
    case FX_PRIMITIVE_TYPE_ORIENTED_PARTICLE: {
        const fx_template_resource_t resource =
            FX_RandomElement(primitiveTemplate->resources);
        const int32_t lifetime = coduo_fp_to_i32_extended(
            (long double)FX_RandomFloatRange(primitiveTemplate->life));
        const int32_t impactEffect =
            FX_RandomElement(primitiveTemplate->impactEffects);
        const int32_t deathEffect =
            FX_RandomElement(primitiveTemplate->deathEffects);
        const float elasticity =
            FX_RandomFloatRange(primitiveTemplate->elasticity.parm);
        const float colorParm =
            FX_RandomFloatRange(primitiveTemplate->rgb.parm);
        const float alphaParm =
            FX_RandomFloatRange(primitiveTemplate->alpha.parm);
        const float alphaEnd =
            FX_RandomFloatRange(primitiveTemplate->alpha.end);
        const float alphaStart =
            FX_RandomFloatRange(primitiveTemplate->alpha.start);
        const float size2Parm =
            FX_RandomFloatRange(primitiveTemplate->size2.parm);
        const float size2End =
            FX_RandomFloatRange(primitiveTemplate->size2.end);
        const float size2Start =
            FX_RandomFloatRange(primitiveTemplate->size2.start);
        const float sizeParm =
            FX_RandomFloatRange(primitiveTemplate->size.parm);
        const float sizeEnd =
            FX_RandomFloatRange(primitiveTemplate->size.end);
        const float sizeStart =
            FX_RandomFloatRange(primitiveTemplate->size.start);
        (void)FX_AddOrientedParticle(
            boltFrame, spawnOrigin, spawnAxis[0], velocity, velocityGoal,
            acceleration, primitiveTemplate->nonUniformScale,
            sizeStart, sizeEnd, sizeParm,
            size2Start, size2End, size2Parm,
            alphaStart, alphaEnd, alphaParm,
            colorStart, colorEnd, colorParm,
            rotation, rotationDelta, rotationAccel, rotationClamp,
            primitiveTemplate->boundsMin, primitiveTemplate->boundsMax,
            elasticity, deathEffect, impactEffect, lifetime,
            resource.handle, primitiveTemplate->flags,
            primitiveTemplate->parameterFlags);
        break;
    }
    case FX_PRIMITIVE_TYPE_ELECTRICITY: {
        const fx_template_resource_t resource =
            FX_RandomElement(primitiveTemplate->resources);
        const int32_t lifetime = coduo_fp_to_i32_extended(
            (long double)FX_RandomFloatRange(primitiveTemplate->life));
        const float electricityParm =
            FX_RandomFloatRange(primitiveTemplate->elasticity.parm);
        const float colorParm =
            FX_RandomFloatRange(primitiveTemplate->rgb.parm);
        const float alphaParm =
            FX_RandomFloatRange(primitiveTemplate->alpha.parm);
        const float alphaEnd =
            FX_RandomFloatRange(primitiveTemplate->alpha.end);
        const float alphaStart =
            FX_RandomFloatRange(primitiveTemplate->alpha.start);
        const float sizeParm =
            FX_RandomFloatRange(primitiveTemplate->size.parm);
        const float sizeEnd =
            FX_RandomFloatRange(primitiveTemplate->size.end);
        const float sizeStart =
            FX_RandomFloatRange(primitiveTemplate->size.start);
        (void)FX_AddElectricity(
            boltFrame, spawnOrigin, secondOrigin,
            sizeStart, sizeEnd, sizeParm,
            alphaStart, alphaEnd, alphaParm,
            colorStart, colorEnd, colorParm, electricityParm,
            lifetime, resource.handle, primitiveTemplate->flags,
            primitiveTemplate->parameterFlags);
        break;
    }
    case FX_PRIMITIVE_TYPE_FX_RUNNER: {
        const int32_t effectId =
            FX_RandomElement(primitiveTemplate->playEffects);
        const sfx_bolt_info_t *runnerBoltInfo =
            boltFrame.frame != nullptr ? &boltFrame.frame->boltInfo : nullptr;
        PlayEntityEffectID(effectId, spawnOrigin, spawnAxis, runnerBoltInfo);
        break;
    }
    case FX_PRIMITIVE_TYPE_LIGHT: {
        const int32_t lifetime = coduo_fp_to_i32_extended(
            (long double)FX_RandomFloatRange(primitiveTemplate->life));
        const float colorParm =
            FX_RandomFloatRange(primitiveTemplate->rgb.parm);
        const float sizeParm =
            FX_RandomFloatRange(primitiveTemplate->size.parm);
        const float sizeEnd =
            FX_RandomFloatRange(primitiveTemplate->size.end);
        const float sizeStart =
            FX_RandomFloatRange(primitiveTemplate->size.start);
        (void)FX_AddLight(boltFrame, spawnOrigin,
                          sizeStart, sizeEnd, sizeParm,
                          colorStart, colorEnd, colorParm,
                          lifetime, primitiveTemplate->flags,
                          primitiveTemplate->parameterFlags);
        break;
    }
    case FX_PRIMITIVE_TYPE_CAMERA_SHAKE: {
        const int32_t lifetime = coduo_fp_to_i32_extended(
            (long double)FX_RandomFloatRange(primitiveTemplate->life));
        const int32_t radius = coduo_fp_to_i32_extended(
            (long double)FX_RandomFloatRange(primitiveTemplate->radius));
        const float amplitude =
            FX_RandomFloatRange(primitiveTemplate->elasticity.parm);
        SFxHelper_CameraShake(spawnOrigin, amplitude, radius, lifetime);
        break;
    }
    case FX_PRIMITIVE_TYPE_FLASH: {
        const fx_template_resource_t resource =
            FX_RandomElement(primitiveTemplate->resources);
        const int32_t lifetime = coduo_fp_to_i32_extended(
            (long double)FX_RandomFloatRange(primitiveTemplate->life));
        const float colorParm =
            FX_RandomFloatRange(primitiveTemplate->rgb.parm);
        (void)FX_AddFlash(spawnOrigin, colorStart, colorEnd, colorParm,
                          lifetime, resource.handle, primitiveTemplate->flags,
                          primitiveTemplate->parameterFlags);
        break;
    }
    default:
        break;
    }

    primitiveTemplate->coduomp_retire_pending_spawn();
    if (boltFrame.frame != nullptr)
        CFxBoltFrame_Release(boltFrame.frame);
}

/* NOT_FROM_ORIGINAL_SOURCE: portable C-style boundary retained for the
 * mixed C/C++ recovered callers; the original body is the member above. */
void CFxScheduler_AddScheduledEffects(cfx_scheduler_t *scheduler)
{
    scheduler->AddScheduledEffects();
}

/* Source: CoDUOMP.exe 0x004a0440..0x004a0452.
 * Name: same-module Mac symbol FX_PlayEntityEffectID. */
void FX_PlayEntityEffectID(int32_t effectId, const vec3_t origin,
                           const axis_t axis,
                           const sfx_bolt_info_t *boltInfo)
{
    CFxScheduler_PlayEntityEffectID(&fxScheduler, effectId, origin, axis,
                                    boltInfo);
}

/* Source: CoDUOMP.exe 0x004a0470..0x004a047c.
 * Name: same-module Mac symbol FX_AddScheduledEffects. */
void FX_AddScheduledEffects(void)
{
    CFxScheduler_AddScheduledEffects(&fxScheduler);
}

/* Source: CoDUOMP.exe 0x004a0480..0x004a0484.
 * Name: same-module Mac symbol FX_InitSystem. */
qboolean FX_InitSystem(void)
{
    return FX_Init();
}

/* Source: CoDUOMP.exe 0x004a0490..0x004a049d.
 * Name: same-module Mac symbol FX_FreeActive. */
qboolean FX_FreeActive(void)
{
    return FX_Free(qtrue);
}

/* Source: CoDUOMP.exe 0x004a04a0..0x004a04ad.
 * Name: same-module Mac symbol FX_FreeSystem. */
qboolean FX_FreeSystem(void)
{
    return FX_Free(qfalse);
}

/* Source: CoDUOMP.exe 0x004a04d0..0x004a04dc.
 * Name: same-module Mac symbol FX_AdjustTime. */
void FX_AdjustTime(int32_t time)
{
    SFxHelper_AdjustTime(time);
}

/* Source: CoDUOMP.exe 0x004a04e0..0x004a04e4.
 * Name: same-module Mac symbol FX_RewindTime. */
void FX_RewindTime(int32_t timeDelta)
{
    FX_Rewind(timeDelta);
}

/* Source: CoDUOMP.exe 0x004af950..0x004af9b2.
 * Name: same-module Mac symbol FX_Rewind. */
void FX_Rewind(int32_t timeDelta)
{
    int32_t cutoffTime = fxCurrentTime - timeDelta;

    for (int32_t slotIndex = 0;
         slotIndex < FX_EFFECT_SLOT_COUNT;
         ++slotIndex) {
        fx_effect_slot_t *slot = &fxEffectSlots[slotIndex];
        cfx_effect_t *effect = slot->effect;
        if (effect == NULL || effect->timeStart <= cutoffTime) {
            continue;
        }

        FX_FreeMember(slot, false);
    }

    CFxScheduler_Clean(&fxScheduler, qfalse, 0);
}

/* Source: CoDUOMP.exe 0x004af9c0..0x004afa29.  The public wrapper at
 * 0x004a0460 reaches this internal cleanup with the entity number in EAX. */
void FX_FreeEntityEffects(int32_t entityNum)
{
    for (int32_t slotIndex = 0;
         slotIndex < FX_EFFECT_SLOT_COUNT;
         ++slotIndex) {
        fx_effect_slot_t *slot = &fxEffectSlots[slotIndex];
        cfx_effect_t *effect = slot->effect;
        cfx_bolt_frame_t *boltFrame = effect != NULL
            ? effect->boltFrame.frame
            : NULL;

        if (boltFrame == NULL || boltFrame->boltInfo.entityNum != entityNum) {
            continue;
        }

        FX_FreeMember(slot, false);
    }

    CFxScheduler_FreeEntityEffects(&fxScheduler, entityNum);
}

/* Source: CoDUOMP.exe 0x004a04b0..0x004a04c3.
 * Name: same-module Mac symbol FX_AdjustCamera. */
void FX_AdjustCamera(refdef_t *refdef, float farPlaneDistance)
{
    SFxHelper_AdjustCamera(refdef, farPlaneDistance);
}
