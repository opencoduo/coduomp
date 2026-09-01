/* Original authored FX primitive factories recovered from CoDUOMP.exe. */

#include "fx_classes.hpp"
#include "../platform/crt_boundary.h"

#include <string.h>

namespace {

enum particle_lerp_flag_e {
    FX_PARTICLE_ALPHA_LERP = 0x1,
    FX_PARTICLE_ALPHA_MODE_MASK = 0xc,
    FX_PARTICLE_ALPHA_MODE_COSINE = 0x8,
    FX_PARTICLE_COLOR_LERP = 0x10,
    FX_PARTICLE_COLOR_MODE_MASK = 0xc0,
    FX_PARTICLE_COLOR_MODE_COSINE = 0x80,
    FX_PARTICLE_SIZE_LERP = 0x100,
    FX_PARTICLE_SIZE_GROUP_MASK = 0xf00,
    FX_PARTICLE_SIZE_MODE_MASK = 0xc00,
    FX_PARTICLE_SIZE_MODE_COSINE = 0x800,
    FX_TAIL_LENGTH_LERP = 0x1000,
    FX_TAIL_LENGTH_MODE_MASK = 0xc000,
    FX_TAIL_LENGTH_MODE_COSINE = 0x8000,
    FX_PARTICLE_SIZE2_LERP = 0x10000,
    FX_PARTICLE_SIZE2_GROUP_MASK = 0xf0000,
    FX_PARTICLE_SIZE2_MODE_MASK = 0xc0000,
    FX_PARTICLE_SIZE2_MODE_COSINE = 0x80000,
    FX_PARTICLE_SIZE2_PARTICLE_COSINE_COMPARE = 0x800
};

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the inlined
 * CFxBoltFramePtr copy-assignment sequence at the start of each primitive
 * factory. It preserves the original release-before-acquire ownership order. */
void coduomp_fx_assign_bolt_frame_ptr(cfx_bolt_frame_ptr_t *destination,
                                      const cfx_bolt_frame_ptr_t &source)
{
    if (destination->frame == source.frame) {
        return;
    }
    if (destination->frame != nullptr) {
        CFxBoltFrame_Release(destination->frame);
        destination->frame = nullptr;
    }
    if (source.frame != nullptr) {
        ++source.frame->referenceCount;
        destination->frame = source.frame;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the identical channel-
 * time initializers in the original FX_Add* factories. The Windows code
 * leaves the destination untouched when neither a mode nor the channel's
 * lerp bit is present, so the boolean result controls whether the caller
 * stores it. */
bool coduomp_fx_resolve_channel_time(int32_t lerpFlags,
                                     int32_t lerpBit,
                                     int32_t modeMask,
                                     int32_t cosineMode,
                                     float parameter,
                                     int32_t lifetime,
                                     float *channelTime)
{
    /* Exact float32 constants at 0x005b9f10 and 0x005b9bf8. */
    constexpr float cosineTimeScale = 0.0031415901612490416f;
    constexpr float percentTimeScale = 0.009999999776482582f;

    const int32_t mode = lerpFlags & modeMask;
    if (mode == cosineMode) {
        *channelTime = static_cast<float>(
            static_cast<long double>(parameter) *
            static_cast<long double>(cosineTimeScale));
        return true;
    }
    if (mode != 0) {
        *channelTime = static_cast<float>(
            static_cast<long double>(lifetime) *
                static_cast<long double>(parameter) *
                static_cast<long double>(percentTimeScale) +
            static_cast<long double>(fxCurrentTime));
        return true;
    }
    if ((lerpFlags & lerpBit) != 0) {
        *channelTime = parameter;
        return true;
    }
    return false;
}

} // namespace

/* Source: CoDUOMP.exe 0x004b0150..0x004b05df.
 * Name and complete source-level type sequence: same-module Mac symbol
 * FX_AddParticle(CFxBoltFramePtr &, float *, float *, float *, float *, bool,
 * ...). CPrimitiveTemplate::CreateEffect's sole Windows call at 0x004a8873
 * proves every argument's role. The Windows LTCG convention carries
 * boltFrame, origin, and acceleration in ECX, EAX, and EBX respectively. */
CParticle *FX_AddParticle(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t origin,
    vec3_t velocity,
    vec3_t velocityGoal,
    vec3_t acceleration,
    bool nonUniformScale,
    float sizeStart, float sizeEnd, float sizeParm,
    float size2Start, float size2End, float size2Parm,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    float rotation, float rotationDelta, float rotationAccel,
    float rotationClamp,
    vec3_t traceMins, vec3_t traceMaxs,
    float elasticity,
    int32_t deathEffectId, int32_t impactEffectId,
    int32_t lifetime, int32_t shaderHandle,
    uint32_t flags, int32_t lerpFlags)
{
    if (fxFrameTime < 1) {
        return nullptr;
    }

    CEffect *effect = new CParticle;
    if (effect == nullptr) {
        return nullptr;
    }
    CParticle *particle = static_cast<CParticle *>(effect);

    coduomp_fx_assign_bolt_frame_ptr(&particle->boltFrame, boltFrame);
    if (particle->boltFrame.frame != nullptr) {
        orientation_t *boltOrientation =
            CFxBoltFrame_GetOrientation(particle->boltFrame.frame);
        vec3_t localOrigin;
        vec3_t localVelocity;
        vec3_t localVelocityGoal;
        vec3_t localAcceleration;
        OrientationPosFromWorldPos(boltOrientation, origin, localOrigin);
        OrientationDirFromWorldDir(boltOrientation, velocity, localVelocity);
        OrientationDirFromWorldDir(boltOrientation, velocityGoal,
                                   localVelocityGoal);
        OrientationDirFromWorldDir(boltOrientation, acceleration,
                                   localAcceleration);
        particle->SetOrigin(localOrigin);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        particle->SetVelocity(localVelocityGoal);
        particle->SetVelocityGoal(localVelocity);
        particle->SetAcceleration(localAcceleration);
    } else {
        particle->SetOrigin(origin);
        particle->SetVelocity(velocity);
        particle->SetVelocityGoal(velocityGoal);
        particle->SetAcceleration(acceleration);
    }

    particle->SetColorStart(colorStart);
    particle->SetColorEnd(colorEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_COLOR_LERP,
        FX_PARTICLE_COLOR_MODE_MASK, FX_PARTICLE_COLOR_MODE_COSINE,
        colorParm, lifetime, &particle->colorTime);

    particle->SetAlphaStart(alphaStart);
    particle->SetAlphaEnd(alphaEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_ALPHA_LERP,
        FX_PARTICLE_ALPHA_MODE_MASK, FX_PARTICLE_ALPHA_MODE_COSINE,
        alphaParm, lifetime, &particle->alphaTime);

    particle->SetSizeStart(sizeStart);
    particle->SetSizeEnd(sizeEnd);
    const bool sizeTimeSet = coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_SIZE_LERP,
        FX_PARTICLE_SIZE_MODE_MASK, FX_PARTICLE_SIZE_MODE_COSINE,
        sizeParm, lifetime, &particle->sizeTime);

    if (!nonUniformScale) {
        particle->SetSize2Start(sizeStart);
        particle->SetSize2End(sizeEnd);
        if (sizeTimeSet) {
            particle->SetSize2Time(particle->sizeTime);
        }
        lerpFlags &= ~FX_PARTICLE_SIZE2_GROUP_MASK;
        lerpFlags |=
            (lerpFlags & FX_PARTICLE_SIZE_GROUP_MASK) << 8;
    } else {
        particle->SetSize2Start(size2Start);
        particle->SetSize2End(size2End);
        const int32_t size2Mode =
            lerpFlags & FX_PARTICLE_SIZE2_MODE_MASK;
        if (size2Mode != 0) {
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            (void)coduomp_fx_resolve_channel_time(
                lerpFlags, FX_PARTICLE_SIZE2_LERP,
                FX_PARTICLE_SIZE2_MODE_MASK,
                FX_PARTICLE_SIZE2_PARTICLE_COSINE_COMPARE,
                size2Parm, lifetime, &particle->size2Time);
        } else if ((lerpFlags & FX_PARTICLE_SIZE2_LERP) != 0) {
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            particle->SetSizeTime(size2Parm);
        }
    }

    particle->renderEntity.spriteShaderHandle = shaderHandle;
    particle->flags = flags;
    particle->lerpFlags = lerpFlags;
    particle->SetRotation(rotation);
    particle->SetRotationGoal(rotationClamp);
    particle->SetRotationVelocity(rotationDelta);
    particle->SetRotationAcceleration(rotationAccel);
    particle->SetElasticity(elasticity);
    particle->SetTraceMins(traceMins);
    particle->SetTraceMaxs(traceMaxs);
    particle->SetImpactEffectID(impactEffectId);
    particle->SetDeathEffectID(deathEffectId);

    FX_AddPrimitive(&effect, lifetime);
    return particle;
}

/* Source: CoDUOMP.exe 0x004b05e0..0x004b08af.
 * Name and type sequence: same-module Mac symbol
 * FX_AddLine(CFxBoltFramePtr &, float *, float *, float, ...). The vtable
 * 0x005a2050, allocator 0x0389ffe0, and renderer type 13 prove CLine. Windows
 * LTCG carries boltFrame, start, and end in ECX, EAX, and EBX. */
CLine *FX_AddLine(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t start, vec3_t end,
    float sizeStart, float sizeEnd, float sizeParm,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    int32_t lifetime, int32_t shaderHandle,
    uint32_t flags, int32_t lerpFlags)
{
    if (fxFrameTime < 1) {
        return nullptr;
    }

    CEffect *effect = new CLine;
    if (effect == nullptr) {
        return nullptr;
    }
    CLine *line = static_cast<CLine *>(effect);

    coduomp_fx_assign_bolt_frame_ptr(&line->boltFrame, boltFrame);
    if (line->boltFrame.frame != nullptr) {
        orientation_t *boltOrientation =
            CFxBoltFrame_GetOrientation(line->boltFrame.frame);
        vec3_t localStart;
        vec3_t localEnd;
        OrientationPosFromWorldPos(boltOrientation, start, localStart);
        OrientationPosFromWorldPos(boltOrientation, end, localEnd);
        line->SetOrigin(localStart);
        line->SetEnd(localEnd);
    } else {
        line->SetOrigin(start);
        line->SetEnd(end);
    }

    line->SetColorStart(colorStart);
    line->SetColorEnd(colorEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_COLOR_LERP,
        FX_PARTICLE_COLOR_MODE_MASK, FX_PARTICLE_COLOR_MODE_COSINE,
        colorParm, lifetime, &line->colorTime);

    line->SetAlphaStart(alphaStart);
    line->SetAlphaEnd(alphaEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_ALPHA_LERP,
        FX_PARTICLE_ALPHA_MODE_MASK, FX_PARTICLE_ALPHA_MODE_COSINE,
        alphaParm, lifetime, &line->alphaTime);

    line->SetSizeStart(sizeStart);
    line->SetSizeEnd(sizeEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_SIZE_LERP,
        FX_PARTICLE_SIZE_MODE_MASK, FX_PARTICLE_SIZE_MODE_COSINE,
        sizeParm, lifetime, &line->sizeTime);

    line->renderEntity.spriteShaderHandle = shaderHandle;
    line->lerpFlags = lerpFlags;
    line->flags = flags;
    line->SetShaderTexCoord(1.0f, 1.0f);

    FX_AddPrimitive(&effect, lifetime);
    return line;
}

/* Source: CoDUOMP.exe 0x004b08b0..0x004b0b87.
 * Name and type sequence: same-module Mac symbol FX_AddElectricity. The
 * Windows factory allocates from the CElectricity pool, installs vtable
 * 0x005a206c, and finishes with CElectricity::Initialize. */
CElectricity *FX_AddElectricity(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t start, vec3_t end,
    float sizeStart, float sizeEnd, float sizeParm,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    float electricityParm,
    int32_t lifetime, int32_t shaderHandle,
    uint32_t flags, int32_t lerpFlags)
{
    if (fxFrameTime < 1) {
        return nullptr;
    }

    CEffect *effect = new CElectricity;
    if (effect == nullptr) {
        return nullptr;
    }
    CElectricity *electricity = static_cast<CElectricity *>(effect);

    coduomp_fx_assign_bolt_frame_ptr(&electricity->boltFrame, boltFrame);
    if (electricity->boltFrame.frame != nullptr) {
        orientation_t *boltOrientation =
            CFxBoltFrame_GetOrientation(electricity->boltFrame.frame);
        vec3_t localStart;
        vec3_t localEnd;
        OrientationPosFromWorldPos(boltOrientation, start, localStart);
        OrientationPosFromWorldPos(boltOrientation, end, localEnd);
        electricity->SetOrigin(localStart);

        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        electricity->SetOrigin(localEnd);
    } else {
        electricity->SetOrigin(start);
        memcpy(&electricity->end[0], &end[0], sizeof(electricity->end[0]));
        memcpy(&electricity->end[1], &end[1], sizeof(electricity->end[1]));
        memcpy(&electricity->end[2], &end[2], sizeof(electricity->end[2]));
    }

    electricity->SetColorStart(colorStart);
    electricity->SetColorEnd(colorEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_COLOR_LERP,
        FX_PARTICLE_COLOR_MODE_MASK, FX_PARTICLE_COLOR_MODE_COSINE,
        colorParm, lifetime, &electricity->colorTime);

    electricity->SetAlphaStart(alphaStart);
    electricity->SetAlphaEnd(alphaEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_ALPHA_LERP,
        FX_PARTICLE_ALPHA_MODE_MASK, FX_PARTICLE_ALPHA_MODE_COSINE,
        alphaParm, lifetime, &electricity->alphaTime);

    electricity->SetSizeStart(sizeStart);
    electricity->SetSizeEnd(sizeEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_SIZE_LERP,
        FX_PARTICLE_SIZE_MODE_MASK, FX_PARTICLE_SIZE_MODE_COSINE,
        sizeParm, lifetime, &electricity->sizeTime);

    electricity->renderEntity.spriteShaderHandle = shaderHandle;
    electricity->lerpFlags = lerpFlags;
    electricity->flags = flags;
    electricity->SetElectricityParm(electricityParm);
    electricity->SetShaderTexCoord(1.0f, 1.0f);

    FX_AddPrimitive(&effect, lifetime);
    electricity->Initialize();
    return electricity;
}

/* Source: CoDUOMP.exe 0x004b0b90..0x004b0f62.
 * Name and type sequence: same-module Mac symbol FX_AddTail. Windows vtable
 * 0x005a20c0 and allocator 0x0389ffc8 prove CTail; LTCG carries boltFrame,
 * origin, and acceleration in ECX, EAX, and EBX. */
CTail *FX_AddTail(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t origin,
    vec3_t velocity,
    vec3_t velocityGoal,
    vec3_t acceleration,
    float sizeStart, float sizeEnd, float sizeParm,
    float lengthStart, float lengthEnd, float lengthParm,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    vec3_t traceMins, vec3_t traceMaxs,
    float elasticity,
    int32_t deathEffectId, int32_t impactEffectId,
    int32_t lifetime, int32_t shaderHandle,
    uint32_t flags, int32_t lerpFlags)
{
    if (fxFrameTime < 1) {
        return nullptr;
    }

    CEffect *effect = new CTail;
    if (effect == nullptr) {
        return nullptr;
    }
    CTail *tail = static_cast<CTail *>(effect);

    coduomp_fx_assign_bolt_frame_ptr(&tail->boltFrame, boltFrame);
    if (tail->boltFrame.frame != nullptr) {
        orientation_t *boltOrientation =
            CFxBoltFrame_GetOrientation(tail->boltFrame.frame);
        vec3_t localOrigin;
        vec3_t localVelocity;
        vec3_t localVelocityGoal;
        vec3_t localAcceleration;
        OrientationPosFromWorldPos(boltOrientation, origin, localOrigin);
        OrientationDirFromWorldDir(boltOrientation, velocity, localVelocity);
        OrientationDirFromWorldDir(boltOrientation, velocityGoal,
                                   localVelocityGoal);
        OrientationDirFromWorldDir(boltOrientation, acceleration,
                                   localAcceleration);
        tail->SetOrigin(localOrigin);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        tail->SetVelocity(localVelocityGoal);
        tail->SetVelocityGoal(localVelocity);
        tail->SetAcceleration(localAcceleration);
    } else {
        tail->SetOrigin(origin);
        tail->SetVelocity(velocity);
        tail->SetVelocityGoal(velocityGoal);
        tail->SetAcceleration(acceleration);
    }

    tail->SetColorStart(colorStart);
    tail->SetColorEnd(colorEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_COLOR_LERP,
        FX_PARTICLE_COLOR_MODE_MASK, FX_PARTICLE_COLOR_MODE_COSINE,
        colorParm, lifetime, &tail->colorTime);

    tail->SetAlphaStart(alphaStart);
    tail->SetAlphaEnd(alphaEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_ALPHA_LERP,
        FX_PARTICLE_ALPHA_MODE_MASK, FX_PARTICLE_ALPHA_MODE_COSINE,
        alphaParm, lifetime, &tail->alphaTime);

    tail->SetSizeStart(sizeStart);
    tail->SetSizeEnd(sizeEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_SIZE_LERP,
        FX_PARTICLE_SIZE_MODE_MASK, FX_PARTICLE_SIZE_MODE_COSINE,
        sizeParm, lifetime, &tail->sizeTime);

    tail->SetLengthStart(lengthStart);
    tail->SetLengthEnd(lengthEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_TAIL_LENGTH_LERP,
        FX_TAIL_LENGTH_MODE_MASK, FX_TAIL_LENGTH_MODE_COSINE,
        lengthParm, lifetime, &tail->lengthTime);

    tail->lerpFlags = lerpFlags;
    tail->flags = flags;
    tail->renderEntity.spriteShaderHandle = shaderHandle;
    tail->SetElasticity(elasticity);
    tail->SetTraceMins(traceMins);
    tail->SetTraceMaxs(traceMaxs);
    tail->SetDeathEffectID(deathEffectId);
    tail->SetImpactEffectID(impactEffectId);
    tail->SetShaderTexCoord(1.0f, 1.0f);

    FX_AddPrimitive(&effect, lifetime);
    return tail;
}

/* Source: CoDUOMP.exe 0x004b0f70..0x004b12e9.
 * Name and type sequence: same-module Mac symbol FX_AddCylinder. Windows
 * vtable 0x005a2034 and allocator 0x0389ffa0 prove CCylinder; LTCG carries
 * boltFrame, origin, and direction in ECX, EAX, and EBX. */
CCylinder *FX_AddCylinder(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t origin, vec3_t direction,
    float sizeStart, float sizeEnd, float sizeParm,
    float size2Start, float size2End, float size2Parm,
    float lengthStart, float lengthEnd, float lengthParm,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    int32_t lifetime, int32_t shaderHandle,
    uint32_t flags, int32_t lerpFlags)
{
    if (fxFrameTime < 1) {
        return nullptr;
    }

    CEffect *effect = new CCylinder;
    if (effect == nullptr) {
        return nullptr;
    }
    CCylinder *cylinder = static_cast<CCylinder *>(effect);

    coduomp_fx_assign_bolt_frame_ptr(&cylinder->boltFrame, boltFrame);
    if (cylinder->boltFrame.frame != nullptr) {
        orientation_t *boltOrientation =
            CFxBoltFrame_GetOrientation(cylinder->boltFrame.frame);
        vec3_t localOrigin;
        vec3_t localDirection;
        OrientationPosFromWorldPos(boltOrientation, origin, localOrigin);
        OrientationDirFromWorldDir(boltOrientation, direction,
                                   localDirection);
        cylinder->SetOrigin(localOrigin);
        cylinder->SetDirection(localDirection);
    } else {
        cylinder->SetOrigin(origin);
        cylinder->SetDirection(direction);
    }

    cylinder->SetColorStart(colorStart);
    cylinder->SetColorEnd(colorEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_COLOR_LERP,
        FX_PARTICLE_COLOR_MODE_MASK, FX_PARTICLE_COLOR_MODE_COSINE,
        colorParm, lifetime, &cylinder->colorTime);

    cylinder->SetSizeStart(sizeStart);
    cylinder->SetSizeEnd(sizeEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_SIZE_LERP,
        FX_PARTICLE_SIZE_MODE_MASK, FX_PARTICLE_SIZE_MODE_COSINE,
        sizeParm, lifetime, &cylinder->sizeTime);

    cylinder->SetSecondarySizeStart(size2Start);
    cylinder->SetSecondarySizeEnd(size2End);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_SIZE2_LERP,
        FX_PARTICLE_SIZE2_MODE_MASK, FX_PARTICLE_SIZE2_MODE_COSINE,
        size2Parm, lifetime, &cylinder->size2Time);

    cylinder->SetLengthStart(lengthStart);
    cylinder->SetLengthEnd(lengthEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_TAIL_LENGTH_LERP,
        FX_TAIL_LENGTH_MODE_MASK, FX_TAIL_LENGTH_MODE_COSINE,
        lengthParm, lifetime, &cylinder->lengthTime);

    cylinder->SetAlphaStart(alphaStart);
    cylinder->SetAlphaEnd(alphaEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_ALPHA_LERP,
        FX_PARTICLE_ALPHA_MODE_MASK, FX_PARTICLE_ALPHA_MODE_COSINE,
        alphaParm, lifetime, &cylinder->alphaTime);

    cylinder->renderEntity.spriteShaderHandle = shaderHandle;
    cylinder->lerpFlags = lerpFlags;
    cylinder->flags = flags;

    FX_AddPrimitive(&effect, lifetime);
    return cylinder;
}

/* Source: CoDUOMP.exe 0x004b12f0..0x004b1795.
 * Name and complete type sequence: same-module Mac symbol FX_AddEmitter.
 * CPrimitiveTemplate::CreateEffect's sole Windows call at 0x004a8de6 proves
 * the resource, density, variance, model, and flag arguments. Windows LTCG
 * carries boltFrame and origin in ECX and EAX. */
CEmitter *FX_AddEmitter(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t origin,
    vec3_t velocity,
    vec3_t velocityGoal,
    vec3_t acceleration,
    float sizeStart, float sizeEnd, float sizeParm,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    vec3_t angles, vec3_t angularVelocity,
    vec3_t traceMins, vec3_t traceMaxs,
    float elasticity,
    int32_t deathEffectId, int32_t impactEffectId,
    int32_t emitterEffectId,
    float density, float variance,
    int32_t lifetime, DObj *model,
    uint32_t flags, int32_t lerpFlags)
{
    if (fxFrameTime < 1) {
        return nullptr;
    }

    CEffect *effect = new CEmitter;
    if (effect == nullptr) {
        return nullptr;
    }
    CEmitter *emitter = static_cast<CEmitter *>(effect);

    coduomp_fx_assign_bolt_frame_ptr(&emitter->boltFrame, boltFrame);
    if (emitter->boltFrame.frame != nullptr) {
        orientation_t *boltOrientation =
            CFxBoltFrame_GetOrientation(emitter->boltFrame.frame);
        vec3_t localOrigin;
        vec3_t localVelocity;
        vec3_t localVelocityGoal;
        vec3_t localAcceleration;
        OrientationPosFromWorldPos(boltOrientation, origin, localOrigin);
        OrientationDirFromWorldDir(boltOrientation, velocity, localVelocity);
        OrientationDirFromWorldDir(boltOrientation, velocityGoal,
                                   localVelocityGoal);
        OrientationDirFromWorldDir(boltOrientation, acceleration,
                                   localAcceleration);
        emitter->SetOrigin(localOrigin);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        emitter->SetVelocity(localVelocityGoal);
        emitter->SetVelocityGoal(localVelocity);
        emitter->SetAcceleration(localAcceleration);
        emitter->SetEmitterOrigin(localOrigin);
        emitter->SetEmitterVelocity(localVelocity);
        emitter->SetEmitterBoltOrigin(boltOrientation->origin);
    } else {
        emitter->SetOrigin(origin);
        emitter->SetVelocity(velocity);
        emitter->SetVelocityGoal(velocityGoal);
        emitter->SetAcceleration(acceleration);
        emitter->SetEmitterOrigin(origin);
        emitter->SetEmitterVelocity(velocity);
        emitter->SetEmitterBoltOrigin(nullptr);
    }

    if ((flags & FX_EFFECT_FLAG_CONTINUAL_LIGHTING) == 0U) {
        emitter->SetLightingOrigin(origin);
    }

    emitter->SetColorStart(colorStart);
    emitter->SetColorEnd(colorEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_COLOR_LERP,
        FX_PARTICLE_COLOR_MODE_MASK, FX_PARTICLE_COLOR_MODE_COSINE,
        colorParm, lifetime, &emitter->colorTime);

    emitter->SetSizeStart(sizeStart);
    emitter->SetSizeEnd(sizeEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_SIZE_LERP,
        FX_PARTICLE_SIZE_MODE_MASK, FX_PARTICLE_SIZE_MODE_COSINE,
        sizeParm, lifetime, &emitter->sizeTime);

    emitter->SetAlphaStart(alphaStart);
    emitter->SetAlphaEnd(alphaEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_ALPHA_LERP,
        FX_PARTICLE_ALPHA_MODE_MASK, FX_PARTICLE_ALPHA_MODE_COSINE,
        alphaParm, lifetime, &emitter->alphaTime);

    if (model == nullptr) {
        flags &= ~static_cast<uint32_t>(FX_EFFECT_FLAG_RENDER_EFFECT);
    }
    emitter->SetAngles(angles);
    emitter->SetAngularVelocity(angularVelocity);

    emitter->lerpFlags = lerpFlags;
    emitter->flags = flags;
    emitter->SetModel(model);
    emitter->SetElasticity(elasticity);
    emitter->SetTraceMins(traceMins);
    emitter->SetTraceMaxs(traceMaxs);
    emitter->SetDeathEffectID(deathEffectId);
    emitter->SetImpactEffectID(impactEffectId);
    emitter->SetEmitterEffectID(emitterEffectId);
    emitter->SetDensity(density);
    emitter->SetVariance(variance);
    emitter->SetLastEmitTime(fxCurrentTime);

    /* Exact float32 rand scale at 0x005b9b68 (0x38000000). The Windows x87
     * expression keeps intermediates wider until the final float store. */
    constexpr long double randomUnit = 3.0517578125e-05L;
    const long double randomSigned =
        static_cast<long double>(coduo_crt_rand()) * randomUnit * 2.0L -
        1.0L;
    emitter->currentDensity = static_cast<float>(
        randomSigned * static_cast<long double>(variance) +
        static_cast<long double>(density));

    FX_AddPrimitive(&effect, lifetime);
    return emitter;
}

/* Source: CoDUOMP.exe 0x004b17a0..0x004b19a0.
 * Name and complete type sequence: same-module Mac symbol FX_AddLight.
 * CPrimitiveTemplate::CreateEffect's sole Windows call at 0x004a907d proves
 * the channel and flag binding. Windows LTCG carries boltFrame in EAX and
 * origin in EBX. */
CLight *FX_AddLight(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t origin,
    float sizeStart, float sizeEnd, float sizeParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    int32_t lifetime, uint32_t flags, int32_t lerpFlags)
{
    if (fxFrameTime < 1) {
        return nullptr;
    }

    CEffect *effect = new CLight;
    if (effect == nullptr) {
        return nullptr;
    }
    CLight *light = static_cast<CLight *>(effect);

    coduomp_fx_assign_bolt_frame_ptr(&light->boltFrame, boltFrame);
    if (light->boltFrame.frame != nullptr) {
        orientation_t *boltOrientation =
            CFxBoltFrame_GetOrientation(light->boltFrame.frame);
        vec3_t localOrigin;
        OrientationPosFromWorldPos(boltOrientation, origin, localOrigin);
        light->SetOrigin(localOrigin);
    } else {
        light->SetOrigin(origin);
    }

    light->SetColorStart(colorStart);
    light->SetColorEnd(colorEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_COLOR_LERP,
        FX_PARTICLE_COLOR_MODE_MASK, FX_PARTICLE_COLOR_MODE_COSINE,
        colorParm, lifetime, &light->colorTime);

    light->SetSizeStart(sizeStart);
    light->SetSizeEnd(sizeEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_SIZE_LERP,
        FX_PARTICLE_SIZE_MODE_MASK, FX_PARTICLE_SIZE_MODE_COSINE,
        sizeParm, lifetime, &light->sizeTime);

    light->lerpFlags = lerpFlags;
    light->flags = flags;
    FX_AddPrimitive(&effect, lifetime);
    return light;
}

/* Source: CoDUOMP.exe 0x004b19b0..0x004b1e73.
 * Name and complete type sequence: same-module Mac symbol
 * FX_AddOrientedParticle. CPrimitiveTemplate::CreateEffect's sole Windows
 * call at 0x004a8fb0 proves every argument. Windows LTCG carries boltFrame,
 * origin, and orientation in ECX, EBX, and EAX. */
COrientedParticle *FX_AddOrientedParticle(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t origin, vec3_t orientation,
    vec3_t velocity, vec3_t velocityGoal, vec3_t acceleration,
    bool nonUniformScale,
    float sizeStart, float sizeEnd, float sizeParm,
    float size2Start, float size2End, float size2Parm,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    float rotation, float rotationDelta, float rotationAccel,
    float rotationClamp,
    vec3_t traceMins, vec3_t traceMaxs,
    float elasticity,
    int32_t deathEffectId, int32_t impactEffectId,
    int32_t lifetime, int32_t shaderHandle,
    uint32_t flags, int32_t lerpFlags)
{
    if (fxFrameTime < 1) {
        return nullptr;
    }

    CEffect *effect = new COrientedParticle;
    if (effect == nullptr) {
        return nullptr;
    }
    COrientedParticle *particle =
        static_cast<COrientedParticle *>(effect);

    coduomp_fx_assign_bolt_frame_ptr(&particle->boltFrame, boltFrame);
    if (particle->boltFrame.frame != nullptr) {
        orientation_t *boltOrientation =
            CFxBoltFrame_GetOrientation(particle->boltFrame.frame);
        vec3_t localOrigin;
        vec3_t localOrientation;
        vec3_t localVelocity;
        vec3_t localAcceleration;
        OrientationPosFromWorldPos(boltOrientation, origin, localOrigin);
        OrientationDirFromWorldDir(boltOrientation, orientation,
                                   localOrientation);
        OrientationDirFromWorldDir(boltOrientation, velocity, localVelocity);
        OrientationDirFromWorldDir(boltOrientation, acceleration,
                                   localAcceleration);
        particle->SetOrigin(localOrigin);
        particle->SetOrientation(localOrientation);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        particle->SetVelocityGoal(localVelocity);
        particle->SetVelocity(localVelocity);
        particle->SetAcceleration(localAcceleration);
    } else {
        particle->SetOrigin(origin);
        particle->SetOrientation(orientation);
        particle->SetVelocity(velocity);
        particle->SetVelocityGoal(velocityGoal);
        particle->SetAcceleration(acceleration);
    }

    particle->SetColorStart(colorStart);
    particle->SetColorEnd(colorEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_COLOR_LERP,
        FX_PARTICLE_COLOR_MODE_MASK, FX_PARTICLE_COLOR_MODE_COSINE,
        colorParm, lifetime, &particle->colorTime);

    particle->SetAlphaStart(alphaStart);
    particle->SetAlphaEnd(alphaEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_ALPHA_LERP,
        FX_PARTICLE_ALPHA_MODE_MASK, FX_PARTICLE_ALPHA_MODE_COSINE,
        alphaParm, lifetime, &particle->alphaTime);

    particle->SetSizeStart(sizeStart);
    particle->SetSizeEnd(sizeEnd);
    const bool sizeTimeSet = coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_SIZE_LERP,
        FX_PARTICLE_SIZE_MODE_MASK, FX_PARTICLE_SIZE_MODE_COSINE,
        sizeParm, lifetime, &particle->sizeTime);

    if (!nonUniformScale) {
        particle->SetSize2Start(sizeStart);
        particle->SetSize2End(sizeEnd);
        if (sizeTimeSet) {
            particle->SetSize2Time(particle->sizeTime);
        }
        lerpFlags &= ~FX_PARTICLE_SIZE2_GROUP_MASK;
        lerpFlags |=
            (lerpFlags & FX_PARTICLE_SIZE_GROUP_MASK) << 8;
    } else {
        particle->SetSize2Start(size2Start);
        particle->SetSize2End(size2End);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        (void)coduomp_fx_resolve_channel_time(
            lerpFlags, FX_PARTICLE_SIZE2_LERP,
            FX_PARTICLE_SIZE2_MODE_MASK,
            FX_PARTICLE_SIZE2_PARTICLE_COSINE_COMPARE,
            size2Parm, lifetime, &particle->size2Time);
    }

    particle->renderEntity.spriteShaderHandle = shaderHandle;
    particle->flags = flags;
    particle->lerpFlags = lerpFlags;
    particle->SetRotation(rotation);
    particle->SetRotationGoal(rotationClamp);
    particle->SetRotationVelocity(rotationDelta);
    particle->SetRotationAcceleration(rotationAccel);
    particle->SetElasticity(elasticity);
    particle->SetTraceMins(traceMins);
    particle->SetTraceMaxs(traceMaxs);
    particle->SetDeathEffectID(deathEffectId);
    particle->SetImpactEffectID(impactEffectId);

    FX_AddPrimitive(&effect, lifetime);
    return particle;
}

/* Source: CoDUOMP.exe 0x004b1e80..0x004b2214, recovered after repairing the
 * missing Ghidra function boundary. The Windows binary contains no direct
 * code/data reference and the Mac symbol bank has no counterpart, so
 * FX_AddDecal is a role-based name. The CDecal pool, vtable, field writes,
 * and final CDecal::Init call prove the factory and complete parameter roles. */
CDecal *FX_AddDecal(
    const vec3_t *points, const vec2_t *textureCoordinates,
    int32_t pointCount,
    vec3_t velocity, vec3_t acceleration,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    vec3_t angularVelocity, float elasticity,
    int32_t rotationDelay, int32_t lifetime,
    int32_t shaderHandle, uint32_t flags, int32_t lerpFlags)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (fxFrameTime < 1 || points == nullptr ||
        (uint32_t)pointCount > (uint32_t)FX_DECAL_POINT_CAPACITY) {
        return nullptr;
    }

    CEffect *effect = new CDecal;
    if (effect == nullptr) {
        return nullptr;
    }
    CDecal *decal = static_cast<CDecal *>(effect);

    /* The original checks only points. A null texture-coordinate array with
     * a positive point count therefore remains invalid caller input. */
    for (int32_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
        const uint32_t pointByteOffset =
            static_cast<uint32_t>(pointIndex) *
            static_cast<uint32_t>(sizeof(vec3_t));
        const uint32_t textureByteOffset =
            static_cast<uint32_t>(pointIndex) *
            static_cast<uint32_t>(sizeof(vec2_t));
        uint8_t *destinationPoint = reinterpret_cast<uint8_t *>(
            reinterpret_cast<uintptr_t>(&decal->points[0]) +
            pointByteOffset);
        uint8_t *destinationTexture = reinterpret_cast<uint8_t *>(
            reinterpret_cast<uintptr_t>(&decal->textureCoordinates[0]) +
            textureByteOffset);
        const uint8_t *sourcePoint = reinterpret_cast<const uint8_t *>(
            reinterpret_cast<uintptr_t>(points) + pointByteOffset);
        const uint8_t *sourceTexture = reinterpret_cast<const uint8_t *>(
            reinterpret_cast<uintptr_t>(textureCoordinates) +
            textureByteOffset);

        for (int component = 0; component < 3; ++component) {
            memcpy(destinationPoint + component * sizeof(float),
                   sourcePoint + component * sizeof(float), sizeof(float));
        }
        for (int component = 0; component < 2; ++component) {
            memcpy(destinationTexture + component * sizeof(float),
                   sourceTexture + component * sizeof(float), sizeof(float));
        }
    }

    decal->SetVelocity(velocity);
    decal->SetAcceleration(acceleration);
    decal->SetColorStart(colorStart);
    decal->SetColorEnd(colorEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_COLOR_LERP,
        FX_PARTICLE_COLOR_MODE_MASK, FX_PARTICLE_COLOR_MODE_COSINE,
        colorParm, lifetime, &decal->colorTime);

    decal->SetAlphaStart(alphaStart);
    decal->SetAlphaEnd(alphaEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_ALPHA_LERP,
        FX_PARTICLE_ALPHA_MODE_MASK, FX_PARTICLE_ALPHA_MODE_COSINE,
        alphaParm, lifetime, &decal->alphaTime);

    decal->renderEntity.spriteShaderHandle = shaderHandle;
    decal->flags = flags;
    decal->lerpFlags = lerpFlags;
    decal->SetAngularVelocity(angularVelocity);
    decal->SetElasticity(elasticity);
    decal->SetRotationDelay(rotationDelay);
    decal->SetPointCount(pointCount);
    decal->Init();

    FX_AddPrimitive(&effect, lifetime);
    return decal;
}

/* Source: CoDUOMP.exe 0x004b2220..0x004b236f.
 * Name and complete type sequence: same-module Mac symbol FX_AddFlash.
 * CPrimitiveTemplate::CreateEffect's sole Windows call at 0x004a9122 proves
 * the origin, color channels, shader, lifetime, and flag binding. Windows
 * LTCG carries origin, colorStart, and colorEnd in ECX, EAX, and EBX. */
CFlash *FX_AddFlash(
    vec3_t origin,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    int32_t lifetime, int32_t shaderHandle,
    uint32_t flags, int32_t lerpFlags)
{
    if (fxFrameTime < 1) {
        return nullptr;
    }

    CEffect *effect = new CFlash;
    if (effect == nullptr) {
        return nullptr;
    }
    CFlash *flash = static_cast<CFlash *>(effect);

    flash->SetOrigin(origin);
    flash->SetColorStart(colorStart);
    flash->SetColorEnd(colorEnd);
    (void)coduomp_fx_resolve_channel_time(
        lerpFlags, FX_PARTICLE_COLOR_LERP,
        FX_PARTICLE_COLOR_MODE_MASK, FX_PARTICLE_COLOR_MODE_COSINE,
        colorParm, lifetime, &flash->colorTime);

    flash->SetSpriteShaderHandle(shaderHandle);
    flash->SetFlags(flags);
    flash->SetLerpFlags(lerpFlags);
    flash->Init();

    FX_AddPrimitive(&effect, lifetime);
    return flash;
}
