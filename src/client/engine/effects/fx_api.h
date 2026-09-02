#ifndef CODUOMP_FX_API_H
#define CODUOMP_FX_API_H

#include "fx_bolt.h"
#include "fx_render_types.h"
#include "../physics/cm_trace.h"
#include "sound/alias/sound_alias.h"

/* Exact original type name from the same-module Mac symbol
 * FX_FreeMember(SEffectList *, bool). */
typedef struct SEffectList fx_effect_slot_t;

#ifdef __cplusplus
class CFxScheduler;
typedef CFxScheduler cfx_scheduler_t;
#else
typedef struct CFxScheduler cfx_scheduler_t;
#endif

/* Windows scheduled-effect record. Windows 0x004a9520 proves this complete
 * 0x44-byte serialized layout; its exact original type name is not yet proven. */
typedef struct sfx_scheduled_effect_s {
#ifdef __cplusplus
    static void *operator new(size_t size) noexcept;
    static void operator delete(void *allocation) noexcept;
#endif
    int32_t effectId;
    int32_t primitiveIndex;
    int32_t scheduledTime;
    sfx_bolt_info_t boltInfo;
    vec3_t origin;
    axis_t axis;
} sfx_scheduled_effect_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(sfx_scheduled_effect_t) == 0x04, "i386 scheduled-effect alignment changed");
_Static_assert(offsetof(sfx_scheduled_effect_t, effectId) == 0x00, "i386 scheduled-effect id offset changed");
_Static_assert(sizeof(((sfx_scheduled_effect_t *)0)->effectId) == 0x04, "i386 scheduled-effect id extent changed");
_Static_assert(offsetof(sfx_scheduled_effect_t, primitiveIndex) == 0x04, "i386 scheduled-effect primitive-index offset changed");
_Static_assert(sizeof(((sfx_scheduled_effect_t *)0)->primitiveIndex) == 0x04, "i386 scheduled-effect primitive-index extent changed");
_Static_assert(offsetof(sfx_scheduled_effect_t, scheduledTime) == 0x08, "i386 scheduled-effect scheduled-time offset changed");
_Static_assert(sizeof(((sfx_scheduled_effect_t *)0)->scheduledTime) == 0x04, "i386 scheduled-effect scheduled-time extent changed");
_Static_assert(offsetof(sfx_scheduled_effect_t, boltInfo) == 0x0c, "i386 scheduled-effect bolt-info offset changed");
_Static_assert(sizeof(((sfx_scheduled_effect_t *)0)->boltInfo) == 0x08, "i386 scheduled-effect bolt-info extent changed");
_Static_assert(offsetof(sfx_scheduled_effect_t, origin) == 0x14, "i386 scheduled-effect origin offset changed");
_Static_assert(sizeof(((sfx_scheduled_effect_t *)0)->origin) == 0x0c, "i386 scheduled-effect origin extent changed");
_Static_assert(offsetof(sfx_scheduled_effect_t, axis) == 0x20, "i386 scheduled-effect axis offset changed");
_Static_assert(sizeof(((sfx_scheduled_effect_t *)0)->axis) == 0x24, "i386 scheduled-effect axis extent changed");
_Static_assert(sizeof(sfx_scheduled_effect_t) == 0x44, "i386 scheduled-effect size changed");
#endif

#ifdef __cplusplus
class CEffect;
typedef CEffect cfx_effect_t;
#else
typedef struct CEffect cfx_effect_t;
#endif

struct SEffectList {
    cfx_effect_t *effect;
    /* Absolute FX-system time at which the effect is reclaimed.  The writer
     * at 0x004b0112 stores fxCurrentTime + lifetime; FX_Update at 0x004b0005
     * compares the current time against this word. */
    int32_t expirationTime;
    fx_effect_slot_t *next;
};

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(fx_effect_slot_t) == 0x04, "i386 FX slot alignment changed");
_Static_assert(offsetof(fx_effect_slot_t, effect) == 0x00, "i386 FX slot effect offset changed");
_Static_assert(sizeof(((fx_effect_slot_t *)0)->effect) == 0x04, "i386 FX slot effect extent changed");
_Static_assert(offsetof(fx_effect_slot_t, expirationTime) == 0x04, "i386 FX slot expiration-time offset changed");
_Static_assert(sizeof(((fx_effect_slot_t *)0)->expirationTime) == 0x04, "i386 FX slot expiration-time extent changed");
_Static_assert(offsetof(fx_effect_slot_t, next) == 0x08, "i386 FX slot free-list link offset changed");
_Static_assert(sizeof(((fx_effect_slot_t *)0)->next) == 0x04, "i386 FX slot free-list link extent changed");
_Static_assert(sizeof(fx_effect_slot_t) == 0xc, "i386 FX slot size changed");
#endif

enum {
    /* Original Windows runtime TypeID return values. */
    FX_EFFECT_TYPE_BASE = 0,
    FX_EFFECT_TYPE_PARTICLE = 1,
    FX_EFFECT_TYPE_LINE = 2,
    FX_EFFECT_TYPE_TAIL = 3,
    FX_EFFECT_TYPE_CYLINDER = 4,
    FX_EFFECT_TYPE_EMITTER = 5,
    FX_EFFECT_TYPE_ORIENTED_PARTICLE = 8,
    FX_EFFECT_TYPE_ELECTRICITY = 9,
    FX_EFFECT_TYPE_LIGHT = 11,
    FX_EFFECT_TYPE_FLASH = 13,
    FX_EFFECT_TYPE_QUAD = 14,
    FX_EFFECT_TYPE_DECAL = 15,
    /* Retail CDecal storage owns five points and five texture-coordinate
     * pairs. This is the single layout, archive, and draw-capacity knob. */
    FX_DECAL_POINT_CAPACITY = 5,
    FX_RENDER_FLAG_DEPTH_HACK = 8,
    /* Tell the renderer to use refEntity.lightingOrigin instead of sampling
     * lighting at the entity's changing origin. */
    FX_RENDER_FLAG_USE_LIGHTING_ORIGIN = 0x80,
    FX_RENDER_FLAG_ELECTRICITY_OPTION_A = 0x400,
    FX_RENDER_FLAG_ELECTRICITY_EFFECT = 0x800,
    FX_RENDER_FLAG_ELECTRICITY_OPTION_B = 0x1000,
    FX_EFFECT_FLAG_VELOCITY_GOAL_X = 0x1,
    FX_EFFECT_FLAG_VELOCITY_GOAL_Y = 0x2,
    FX_EFFECT_FLAG_VELOCITY_GOAL_Z = 0x4,
    /* Accelerate rotation toward rotationGoal and stop after crossing it. */
    FX_EFFECT_FLAG_ROTATION_GOAL = 0x8,
    FX_EFFECT_FLAG_DEPTH_HACK = 0x100000,
    /* Keep lighting attached to the moving effect. Without this bit emitter
     * factories capture the initial world origin as a fixed lighting origin. */
    FX_EFFECT_FLAG_CONTINUAL_LIGHTING = 0x800000,
    /* This flag enables CEmitter submission and maps to the electricity
     * renderer's effect bit. Exact original source spelling is unavailable. */
    FX_EFFECT_FLAG_RENDER_EFFECT = 0x1000000,
    FX_EFFECT_FLAG_ELECTRICITY_OPTION_A = 0x2000000,
    FX_EFFECT_FLAG_ELECTRICITY_OPTION_B = 0x4000000,
    /* Preserve RGB and emit opacity through shaderRGBA[3]. Without this bit,
     * CParticle::UpdateAlpha bakes opacity into RGB and forces alpha to 255. */
    FX_EFFECT_FLAG_USE_ALPHA_CHANNEL = 0x8000000,
    /* Advance an emitter's distance-spaced child-effect stream. */
    FX_EFFECT_FLAG_EMIT_EFFECTS = 0x10000000,
    /* Trace particle motion against world/entity collision. */
    FX_EFFECT_FLAG_TRACE_COLLISION = 0x2000000,
    /* Use the configured trace bounds instead of a point trace. */
    FX_EFFECT_FLAG_TRACE_VOLUME = 0x4000000,
    /* Stop the particle immediately after a trace hit. */
    FX_EFFECT_FLAG_DIE_ON_IMPACT = 0x40000000,
    /* Play deathResource.effectId from CParticle::Die. */
    FX_EFFECT_FLAG_PLAY_DEATH_EFFECT = 0x20000000,
    FX_EFFECT_SLOT_COUNT = 1800
};

/* Play impactResource.effectId when a trace hits. Kept outside the enum so
 * the high bit does not force unrelated count constants to unsigned type. */
#define FX_EFFECT_FLAG_PLAY_IMPACT_EFFECT UINT32_C(0x80000000)

extern cfx_scheduler_t fxScheduler;
#ifdef __cplusplus
extern "C" {
#endif
extern fx_effect_slot_t fxEffectSlots[FX_EFFECT_SLOT_COUNT];
extern fx_effect_slot_t *fxEffectFreeList;
extern int32_t fxActiveEffectCount;
extern int32_t fxDrawnEffectCount;       /* 0x00d8d54c */
extern int32_t fxReplacementSlotIndex;   /* 0x038b50a4 */

fx_effect_slot_t *FX_GetNextEffectSlot(fx_effect_slot_t *slot);
qboolean FX_HasActiveEffects(void);

extern cvar_t *fx_draw;                  /* 0x04e1994c */
extern cvar_t *fx_cull;                  /* 0x04e19984 */
extern cvar_t *fx_enable;                /* 0x04958078 */
extern cvar_t *fx_cullscale;             /* 0x04e19978 */
extern cvar_t *fx_cullbias;              /* 0x04df96a8 */
extern cvar_t *fx_count;                 /* 0x04dc8840 */
extern cvar_t *fx_debug;                 /* 0x04e19950 */
extern cvar_t *fx_freeze;                /* 0x04dc8820 */

#ifdef __cplusplus
}
#endif

void CFxScheduler_PlaySimpleEffect(cfx_scheduler_t *scheduler, const char *name, const vec3_t origin);
void CFxScheduler_PlayEffect(cfx_scheduler_t *scheduler, const char *name, const vec3_t origin, const vec3_t forward);
void CFxScheduler_PlayEntityEffect(cfx_scheduler_t *scheduler, const char *name, const vec3_t origin, const axis_t axis,
                                   const sfx_bolt_info_t *boltInfo);
void CFxScheduler_PlaySimpleEffectID(cfx_scheduler_t *scheduler, int32_t effectId, const vec3_t origin);
void CFxScheduler_PlayEffectID(cfx_scheduler_t *scheduler, int32_t effectId, const vec3_t origin, const vec3_t forward);
void CFxScheduler_PlayEntityEffectID(cfx_scheduler_t *scheduler, int32_t effectId, const vec3_t origin, const axis_t axis,
                                     const sfx_bolt_info_t *boltInfo);
void ReportPlayEffectError(int32_t effectId);
void CFxScheduler_AddScheduledEffects(cfx_scheduler_t *scheduler);
void CFxScheduler_Clean(cfx_scheduler_t *scheduler, qboolean freeTemplates, int32_t preserveEffectId);
void CFxScheduler_FreeEntityEffects(cfx_scheduler_t *scheduler, int32_t entityNum);
/* Source counterpart remains to be recovered from 0x004a92e0..0x004a951d.
 * The call ABI and body prove that the archive owns this operation and that
 * the sole object argument is the FX scheduler. */
void CFxArchive_ArchiveScheduler(fx_archive_t *archive, cfx_scheduler_t *scheduler);
void CFxArchive_ArchiveScheduledEffect(fx_archive_t *archive, sfx_scheduled_effect_t *effect);
/* Renderer and sound boundaries used directly by the original FX source.
 * GetRefAPI installs the renderer entries; the Windows table at 0x049580a0
 * proves the order and signatures used here. */
#ifdef __cplusplus
extern "C" {
#endif
int32_t RE_RegisterShader(const char *name, int32_t loadMode);
int32_t RE_RegisterShaderNoMip(const char *name, int32_t loadMode);
const char *RE_GetShaderName(int32_t shader);
uint32_t RE_GetImageMemory(void);
uint32_t RE_GetFXImageMemory(void);
void RE_SetFXImageMemory(uint32_t imageMemory);
void RE_AddRefEntityToScene(const refEntity_t *entity, renderer_static_model_t *staticLighting);
void RE_AddLightToScene(const vec3_t origin, float radius, float red, float green, float blue);
void RE_SetIgnorePrecacheErrors(qboolean ignorePrecacheErrors);
void RE_AddPolyToScene(int32_t shaderHandle, int32_t vertexCount, const polyVert_t *vertices);
int32_t SFxHelper_RegisterShader(const char *name);
#ifdef __cplusplus
}
#endif

int32_t SFxHelper_RegisterSound(const char *name);
int32_t SFxHelper_OpenFile(const char *name, int32_t *fileHandle, int32_t unusedMode);
qboolean SFxHelper_ReadFile(void *buffer, int32_t byteCount, int32_t fileHandle);
void SFxHelper_CloseFile(int32_t fileHandle);
DObj *SFxHelper_RegisterModel(const char *name);
void SFxHelper_SetIgnorePrecacheErrors(qboolean ignorePrecacheErrors);
const char *SFxHelper_GetShaderName(int32_t shader);
void SFxHelper_PlaySound(const vec3_t origin, int32_t entityNum, int32_t soundHandle);
void SFxHelper_Trace(trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
                     int32_t unusedPassEntityNum, int32_t contentMask);
void SFxHelper_AddPolyToScene(int32_t shaderHandle, int32_t vertexCount, const polyVert_t *vertices);
void SFxHelper_CameraShake(const vec3_t origin, float amplitude, int32_t radius, int32_t duration);
void SFxHelper_Print(const char *format, ...);
void SFxHelper_Construct(void);
void SFxHelper_Init(void);
qboolean FX_Init(void);
qboolean FX_Free(qboolean freeActive);
#ifdef __cplusplus
void FX_FreeMember(fx_effect_slot_t *slot, bool callDie);
#endif
fx_effect_slot_t *FX_GetValidEffect(void);
void FX_AddPrimitive(cfx_effect_t **effect, int32_t lifetime);
void FX_Add(void);
void FX_Rewind(int32_t timeDelta);
void FX_FreeEntityEffects(int32_t entityNum);
void SFxHelper_AdjustTime(int32_t time);
void SFxHelper_AdjustCamera(refdef_t *refdef, float farPlaneDistance);
qboolean SFxHelper_CullSphere(const vec3_t origin, float radius);
qboolean SFxHelper_CullCylinder(const vec3_t start, const vec3_t end, float startRadius, float endRadius);
void SFxHelper_AddFxToScene(refEntity_t *entity);
void SFxHelper_AddLightToScene(const vec3_t origin, float radius, float red, float green, float blue);
#ifdef __cplusplus
extern "C" {
#endif

void FX_PlaySimpleEffect(const char *name, const vec3_t origin);
void FX_PlayEffect(const char *name, const vec3_t origin, const vec3_t forward);
void FX_PlayEntityEffect(const char *name, const vec3_t origin, const axis_t axis, const sfx_bolt_info_t *boltInfo);
void FX_PlaySimpleEffectID(int32_t effectId, const vec3_t origin);
void FX_PlayEffectID(int32_t effectId, const vec3_t origin, const vec3_t forward);
void FX_PlayEntityEffectID(int32_t effectId, const vec3_t origin, const axis_t axis, const sfx_bolt_info_t *boltInfo);
void FX_AddScheduledEffects(void);
qboolean FX_InitSystem(void);
qboolean FX_FreeActive(void);
qboolean FX_FreeSystem(void);
void FX_ClearActiveEffects(void);
void FX_AdjustTime(int32_t time);
void FX_RewindTime(int32_t timeDelta);
void FX_AdjustCamera(refdef_t *refdef, float farPlaneDistance);
int32_t FX_RegisterEffect(const char *name);
void FX_FreeEntity(int32_t entityNum);
int32_t FX_Save(uint8_t *buffer, int32_t capacity);
int32_t FX_Load(uint8_t *buffer, int32_t capacity);

#ifdef __cplusplus
}
#endif

#endif
