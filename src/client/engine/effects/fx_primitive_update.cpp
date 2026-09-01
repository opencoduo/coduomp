/* Primitive-effect update virtuals recovered from CoDUOMP.exe. */

#include "fx_classes.hpp"

#include "compat/coduo_native_x87.h"
#include "fx_runtime.h"

#include "../math/vector_math.h"
#include "../platform/crt_boundary.h"

#include <cmath>

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the seven equivalent
 * particle, tail, and light channel-interpolation instruction sequences. The
 * delayed and early modes average their independent channel weight with the
 * lifetime weight when lifetime interpolation is enabled; cosine mode instead
 * multiplies by the lifetime weight. Before a delayed transition begins, its
 * channel weight is 1.0 rather than the lifetime weight. */
static float coduomp_fx_evaluate_channel_start_weight(
    int32_t flags, float channelTime,
    int32_t lifetimeBit, int32_t randomBit,
    int32_t modeMask, int32_t delayedMode,
    int32_t cosineMode, int32_t earlyMode,
    int32_t timeStart, int32_t timeEnd,
    bool channelTimeIsLifetimePeak)
{
    float lifetimeWeight = 1.0f;
    const bool lifetimeEnabled = (flags & lifetimeBit) != 0;
    if (lifetimeEnabled) {
        const int32_t lifetime = timeEnd - timeStart;
        const int32_t elapsed = fxCurrentTime - timeStart;
        if (channelTimeIsLifetimePeak && channelTime > 0.0f &&
            channelTime < 0.9990000128746033f) {
            const int32_t peakTime = static_cast<int32_t>(
                static_cast<float>(lifetime) * channelTime);
            if (elapsed < peakTime) {
                lifetimeWeight =
                    static_cast<float>(elapsed) /
                    static_cast<float>(peakTime);
            } else {
                lifetimeWeight = 1.0f -
                    static_cast<float>(elapsed - peakTime) /
                    static_cast<float>(lifetime - peakTime);
            }
        } else {
            lifetimeWeight = 1.0f -
                static_cast<float>(elapsed) /
                static_cast<float>(lifetime);
        }
    }

    float startWeight = lifetimeWeight;
    const int32_t mode = flags & modeMask;
    if (mode == delayedMode) {
        startWeight = 1.0f;
        /* The repeated x87 compare branches on C0 or C3, so the transition
         * expression is entered only when current time is strictly greater
         * than the channel time (0x004a19a4..0x004a19af and peers). */
        if (static_cast<float>(fxCurrentTime) > channelTime) {
            startWeight = 1.0f -
                (static_cast<float>(fxCurrentTime) - channelTime) /
                (static_cast<float>(timeEnd) - channelTime);
        }
        if (lifetimeEnabled) {
            startWeight = (startWeight + lifetimeWeight) * 0.5f;
        }
    } else if (mode == cosineMode) {
        startWeight = std::cos(
            static_cast<float>(fxCurrentTime - timeStart) * channelTime) *
            lifetimeWeight;
    } else if (mode == earlyMode) {
        /* 0x004a19ff..0x004a1a21 and the corresponding channel copies retain
         * the quotient only for a strict current-time-less-than comparison. */
        if (static_cast<float>(fxCurrentTime) < channelTime) {
            startWeight =
                (channelTime - static_cast<float>(fxCurrentTime)) /
                (channelTime - static_cast<float>(timeStart));
        } else {
            startWeight = 0.0f;
        }
        if (lifetimeEnabled) {
            startWeight = (startWeight + lifetimeWeight) * 0.5f;
        }
    }

    if ((flags & randomBit) != 0) {
        startWeight *=
            static_cast<float>(coduo_crt_rand()) * 3.0517578125e-05f;
    }
    return startWeight;
}

/* Source: CoDUOMP.exe 0x004a24f0..0x004a25a2.
 * Name: same-module Mac symbol COrientedParticle::Update. */
qboolean COrientedParticle::Update()
{
    if (timeStart > fxCurrentTime || UpdateOrigin() == qfalse) {
        return qfalse;
    }

    UpdateSize();
    UpdateSize2();
    UpdateRGB();
    UpdateAlpha();
    UpdateRotation();

    if (boltFrame.frame != nullptr) {
        orientation_t *boltOrientation =
            CFxBoltFrame_GetOrientation(boltFrame.frame);
        if (boltOrientation == nullptr) {
            return qfalse;
        }
        OrientationPosToWorldPos(boltOrientation, origin,
                                 renderEntity.origin);
        OrientationDirToWorldDir(boltOrientation, orientation,
                                 renderEntity.axis[0]);
    } else {
        for (int component = 0; component < 3; ++component) {
            renderEntity.origin[component] = origin[component];
            renderEntity.axis[0][component] = orientation[component];
        }
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004a26c0..0x004a2760.
 * Name: same-module Mac symbol CLine::Update. */
qboolean CLine::Update()
{
    if (timeStart > fxCurrentTime) {
        return qfalse;
    }

    if (boltFrame.frame != nullptr) {
        orientation_t *boltOrientation =
            CFxBoltFrame_GetOrientation(boltFrame.frame);
        if (boltOrientation == nullptr) {
            return qfalse;
        }
        OrientationPosToWorldPos(boltOrientation, origin,
                                 renderEntity.origin);
        OrientationPosToWorldPos(boltOrientation, end,
                                 renderEntity.oldorigin);
    } else {
        for (int component = 0; component < 3; ++component) {
            renderEntity.origin[component] = origin[component];
            renderEntity.oldorigin[component] = end[component];
        }
    }

    UpdateSize();
    UpdateRGB();
    UpdateAlpha();
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004a2900..0x004a29a3.
 * Name: same-module Mac symbol CElectricity::Update. */
qboolean CElectricity::Update()
{
    if (timeStart > fxCurrentTime) {
        return qfalse;
    }

    UpdateSize();
    UpdateRGB();
    UpdateAlpha();

    if (boltFrame.frame != nullptr) {
        orientation_t *boltOrientation =
            CFxBoltFrame_GetOrientation(boltFrame.frame);
        if (boltOrientation == nullptr) {
            return qfalse;
        }
        OrientationPosToWorldPos(boltOrientation, origin,
                                 renderEntity.origin);
        OrientationPosToWorldPos(boltOrientation, end,
                                 renderEntity.oldorigin);
    } else {
        for (int component = 0; component < 3; ++component) {
            renderEntity.origin[component] = origin[component];
            renderEntity.oldorigin[component] = end[component];
        }
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004a2af0..0x004a2b8f.
 * Name: same-module Mac symbol CTail::Update. */
qboolean CTail::Update()
{
    if (timeStart > fxCurrentTime) {
        return qfalse;
    }

    for (int component = 0; component < 3; ++component) {
        end[component] = origin[component];
    }
    if (UpdateOrigin() == qfalse) {
        return qfalse;
    }

    if (boltFrame.frame != nullptr) {
        orientation_t *boltOrientation =
            CFxBoltFrame_GetOrientation(boltFrame.frame);
        if (boltOrientation == nullptr) {
            return qfalse;
        }
        OrientationPosToWorldPos(boltOrientation, origin,
                                 renderEntity.origin);
    } else {
        for (int component = 0; component < 3; ++component) {
            renderEntity.origin[component] = origin[component];
        }
    }

    UpdateSize();
    UpdateLength();
    UpdateRGB();
    UpdateAlpha();
    CalcNewEndpoint();
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004a2cd0..0x004a2e09.
 * Name: same-module Mac symbol CTail::CalcNewEndpoint. */
void CTail::CalcNewEndpoint()
{
    vec3_t direction;
    vec3_t endpoint;
    for (int component = 0; component < 3; ++component) {
        direction[component] = end[component] - origin[component];
    }
    (void)VectorNormalize(direction);
    for (int component = 0; component < 3; ++component) {
        endpoint[component] =
            origin[component] + direction[component] * currentLength;
    }

    if (boltFrame.frame != nullptr) {
        orientation_t *boltOrientation =
            CFxBoltFrame_GetOrientation(boltFrame.frame);
        OrientationPosToWorldPos(boltOrientation, endpoint,
                                 renderEntity.oldorigin);
    } else {
        for (int component = 0; component < 3; ++component) {
            renderEntity.oldorigin[component] = endpoint[component];
        }
    }
}

/* Source: CoDUOMP.exe 0x004a2b90..0x004a2ccc.
 * Name: same-module Mac symbol CTail::UpdateLength. */
void CTail::UpdateLength()
{
    enum {
        FX_TAIL_LENGTH_LERP = 0x1000,
        FX_TAIL_LENGTH_RANDOM = 0x2000,
        FX_TAIL_LENGTH_MODE_MASK = 0xc000,
        FX_TAIL_LENGTH_MODE_DELAYED = 0x4000,
        FX_TAIL_LENGTH_MODE_COSINE = 0x8000,
        FX_TAIL_LENGTH_MODE_EARLY = 0xc000
    };

    float startWeight = coduomp_fx_evaluate_channel_start_weight(
        lerpFlags, lengthTime,
        FX_TAIL_LENGTH_LERP, FX_TAIL_LENGTH_RANDOM,
        FX_TAIL_LENGTH_MODE_MASK, FX_TAIL_LENGTH_MODE_DELAYED,
        FX_TAIL_LENGTH_MODE_COSINE, FX_TAIL_LENGTH_MODE_EARLY,
        timeStart, timeEnd, false);

    float endWeight = 1.0f - startWeight;
    currentLength = lengthEnd * endWeight + lengthStart * startWeight;
}

/* Source: CoDUOMP.exe 0x004a2fa0..0x004a309b.
 * Name: same-module Mac symbol CCylinder::Update. */
qboolean CCylinder::Update()
{
    if (timeStart > fxCurrentTime) {
        return qfalse;
    }

    UpdateSize();
    UpdateSize2();
    UpdateLength();
    UpdateRGB();
    UpdateAlpha();

    if (boltFrame.frame != nullptr) {
        orientation_t *boltOrientation =
            CFxBoltFrame_GetOrientation(boltFrame.frame);
        if (boltOrientation == nullptr) {
            return qfalse;
        }
        OrientationPosToWorldPos(boltOrientation, origin,
                                 renderEntity.origin);
        vec3_t endpoint;
        for (int component = 0; component < 3; ++component) {
            endpoint[component] = origin[component] +
                renderEntity.axis[0][component] * currentLength;
        }
        OrientationPosToWorldPos(boltOrientation, endpoint,
                                 renderEntity.oldorigin);
    } else {
        for (int component = 0; component < 3; ++component) {
            renderEntity.origin[component] = origin[component];
            renderEntity.oldorigin[component] = origin[component] +
                renderEntity.axis[0][component] * currentLength;
        }
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004a1700..0x004a18b5.
 * Name: same-module Mac symbol CParticle::UpdateVelocity. */
void CParticle::UpdateVelocity()
{
    constexpr float velocityTimeScale = 0.0010000000474974513f;
    constexpr float settledDeltaSquared = 9.999999747378752e-06f;
    constexpr uint32_t velocityGoalMask =
        FX_EFFECT_FLAG_VELOCITY_GOAL_X |
        FX_EFFECT_FLAG_VELOCITY_GOAL_Y |
        FX_EFFECT_FLAG_VELOCITY_GOAL_Z;
    const double elapsedScale =
        static_cast<double>(fxFrameTime) * velocityTimeScale;

    if ((flags & velocityGoalMask) == 0U) {
        for (int component = 0; component < 3; ++component) {
            particleVelocity[component] = static_cast<float>(
                static_cast<double>(particleVelocity[component]) +
                elapsedScale * acceleration[component]);
        }
        return;
    }

    vec3_t nextVelocity;
    for (int component = 0; component < 3; ++component) {
        nextVelocity[component] = static_cast<float>(
            static_cast<double>(particleVelocity[component]) +
            elapsedScale * acceleration[component]);
    }

    for (int component = 0; component < 3; ++component) {
        const uint32_t componentFlag = 1U << component;
        if ((flags & componentFlag) == 0U) {
            continue;
        }
        const double previousDelta =
            static_cast<double>(velocityGoal[component]) -
            particleVelocity[component];
        const double currentDelta =
            static_cast<double>(velocityGoal[component]) -
            nextVelocity[component];
        if (previousDelta * currentDelta < 0.0) {
            nextVelocity[component] = velocityGoal[component];
            acceleration[component] = 0.0f;
        }
    }

    const double deltaSquared =
        static_cast<double>(acceleration[0]) * acceleration[0] +
        static_cast<double>(acceleration[1]) * acceleration[1] +
        static_cast<double>(acceleration[2]) * acceleration[2];
    if (deltaSquared < settledDeltaSquared) {
        flags &= ~velocityGoalMask;
    }
    for (int component = 0; component < 3; ++component) {
        particleVelocity[component] = nextVelocity[component];
    }
}

/* Source: CoDUOMP.exe 0x004a18c0..0x004a1a7b.
 * Name: same-module Mac symbol CParticle::UpdateSize. */
void CParticle::UpdateSize()
{
    enum {
        FX_PARTICLE_SIZE_LERP = 0x100,
        FX_PARTICLE_SIZE_RANDOM = 0x200,
        FX_PARTICLE_SIZE_MODE_MASK = 0xc00,
        FX_PARTICLE_SIZE_MODE_DELAYED = 0x400,
        FX_PARTICLE_SIZE_MODE_COSINE = 0x800,
        FX_PARTICLE_SIZE_MODE_EARLY = 0xc00
    };

    const float startWeight = coduomp_fx_evaluate_channel_start_weight(
        lerpFlags, sizeTime,
        FX_PARTICLE_SIZE_LERP, FX_PARTICLE_SIZE_RANDOM,
        FX_PARTICLE_SIZE_MODE_MASK, FX_PARTICLE_SIZE_MODE_DELAYED,
        FX_PARTICLE_SIZE_MODE_COSINE, FX_PARTICLE_SIZE_MODE_EARLY,
        timeStart, timeEnd, true);
    renderEntity.radius =
        sizeEnd * (1.0f - startWeight) + sizeStart * startWeight;
}

/* Source: CoDUOMP.exe 0x004a1a80..0x004a1c3e.
 * Name: same-module Mac symbol CParticle::UpdateSize2. */
void CParticle::UpdateSize2()
{
    enum {
        FX_PARTICLE_SIZE2_LERP = 0x10000,
        FX_PARTICLE_SIZE2_RANDOM = 0x20000,
        FX_PARTICLE_SIZE2_MODE_MASK = 0xc0000,
        FX_PARTICLE_SIZE2_MODE_DELAYED = 0x40000,
        FX_PARTICLE_SIZE2_MODE_COSINE = 0x80000,
        FX_PARTICLE_SIZE2_MODE_EARLY = 0xc0000
    };

    const float startWeight = coduomp_fx_evaluate_channel_start_weight(
        lerpFlags, size2Time,
        FX_PARTICLE_SIZE2_LERP, FX_PARTICLE_SIZE2_RANDOM,
        FX_PARTICLE_SIZE2_MODE_MASK, FX_PARTICLE_SIZE2_MODE_DELAYED,
        FX_PARTICLE_SIZE2_MODE_COSINE, FX_PARTICLE_SIZE2_MODE_EARLY,
        timeStart, timeEnd, true);
    renderEntity.radius2 =
        size2End * (1.0f - startWeight) + size2Start * startWeight;
}

/* Source: CoDUOMP.exe 0x004a1c40..0x004a1e49.
 * Name: same-module Mac symbol CParticle::UpdateRGB. */
void CParticle::UpdateRGB()
{
    enum {
        FX_PARTICLE_COLOR_LERP = 0x10,
        FX_PARTICLE_COLOR_RANDOM = 0x20,
        FX_PARTICLE_COLOR_MODE_MASK = 0xc0,
        FX_PARTICLE_COLOR_MODE_DELAYED = 0x40,
        FX_PARTICLE_COLOR_MODE_COSINE = 0x80,
        FX_PARTICLE_COLOR_MODE_EARLY = 0xc0
    };

    const float startWeight = coduomp_fx_evaluate_channel_start_weight(
        lerpFlags, colorTime,
        FX_PARTICLE_COLOR_LERP, FX_PARTICLE_COLOR_RANDOM,
        FX_PARTICLE_COLOR_MODE_MASK, FX_PARTICLE_COLOR_MODE_DELAYED,
        FX_PARTICLE_COLOR_MODE_COSINE, FX_PARTICLE_COLOR_MODE_EARLY,
        timeStart, timeEnd, true);
    const float endWeight = 1.0f - startWeight;
    vec3_t color;
    for (int component = 0; component < 3; ++component) {
        color[component] = colorStart[component] * startWeight +
                           colorEnd[component] * endWeight;
    }
    ClampVec(color, renderEntity.shaderRGBA);
}

/* Source: CoDUOMP.exe 0x004a1e50..0x004a20b2.
 * Name: same-module Mac symbol CParticle::UpdateAlpha. */
void CParticle::UpdateAlpha()
{
    enum {
        FX_PARTICLE_ALPHA_LERP = 0x1,
        FX_PARTICLE_ALPHA_RANDOM = 0x2,
        FX_PARTICLE_ALPHA_MODE_MASK = 0xc,
        FX_PARTICLE_ALPHA_MODE_DELAYED = 0x4,
        FX_PARTICLE_ALPHA_MODE_COSINE = 0x8,
        FX_PARTICLE_ALPHA_MODE_EARLY = 0xc
    };

    const float startWeight = coduomp_fx_evaluate_channel_start_weight(
        lerpFlags, alphaTime,
        FX_PARTICLE_ALPHA_LERP, 0,
        FX_PARTICLE_ALPHA_MODE_MASK, FX_PARTICLE_ALPHA_MODE_DELAYED,
        FX_PARTICLE_ALPHA_MODE_COSINE, FX_PARTICLE_ALPHA_MODE_EARLY,
        timeStart, timeEnd, true);
    float alpha = alphaStart * startWeight +
                  alphaEnd * (1.0f - startWeight);
    if (alpha < 0.0f) {
        alpha = 0.0f;
    } else if (alpha > 1.0f) {
        alpha = 1.0f;
    }
    if ((lerpFlags & FX_PARTICLE_ALPHA_RANDOM) != 0) {
        alpha *= static_cast<float>(coduo_crt_rand()) *
                 3.0517578125e-05f;
    }

    if ((flags & FX_EFFECT_FLAG_USE_ALPHA_CHANNEL) != 0U) {
        renderEntity.shaderRGBA[3] =
            static_cast<uint8_t>(coduo_fp_to_i32_extended(
                static_cast<long double>(alpha) * 255.0L));
    } else {
        for (int component = 0; component < 3; ++component) {
            renderEntity.shaderRGBA[component] = static_cast<uint8_t>(
                coduo_fp_to_i32_extended(
                    static_cast<long double>(
                        renderEntity.shaderRGBA[component]) *
                    static_cast<long double>(alpha)));
        }
        renderEntity.shaderRGBA[3] = 255;
    }
}

/* Source: CoDUOMP.exe 0x004a20c0..0x004a21b1.
 * Name: same-module Mac symbol CParticle::UpdateRotation. */
void CParticle::UpdateRotation()
{
    /* Exact float32 constants from 0x005b9bf8, 0x005b9b44, and 0x005b9bec. */
    constexpr float rotationTimeScale = 0.009999999776482582f;
    constexpr float accelerationTimeScale = 0.0010000000474974513f;
    constexpr float velocityDampingScale = 0.0006500000017695129f;
    constexpr float accelerationEpsilon = 9.999999974752427e-07f;

    renderEntity.rotation = static_cast<float>(
        static_cast<double>(renderEntity.rotation) +
        static_cast<double>(fxFrameTime) * rotationTimeScale *
        static_cast<double>(rotationVelocity));

    if ((flags & FX_EFFECT_FLAG_ROTATION_GOAL) != 0U) {
        const float previousVelocity = rotationVelocity;
        rotationVelocity = static_cast<float>(
            static_cast<double>(previousVelocity) +
            static_cast<double>(fxFrameTime) * accelerationTimeScale *
            static_cast<double>(rotationAcceleration));
        const double previousDelta =
            static_cast<double>(rotationGoal) - previousVelocity;
        const double currentDelta =
            static_cast<double>(rotationGoal) - rotationVelocity;
        if (previousDelta * currentDelta < 0.0) {
            rotationVelocity = rotationGoal;
            rotationAcceleration = 0.0f;
            flags &= ~static_cast<uint32_t>(FX_EFFECT_FLAG_ROTATION_GOAL);
        }
        return;
    }

    if (rotationAcceleration > accelerationEpsilon ||
        rotationAcceleration < -accelerationEpsilon) {
        rotationVelocity = static_cast<float>(
            static_cast<double>(rotationVelocity) +
            static_cast<double>(fxFrameTime) * accelerationTimeScale *
            static_cast<double>(rotationAcceleration));
    } else {
        rotationVelocity = static_cast<float>(
            (1.0 - static_cast<double>(fxFrameTime) *
                       velocityDampingScale) *
            static_cast<double>(rotationVelocity));
    }
}

/* Source: CoDUOMP.exe 0x004a3a80..0x004a3b32.
 * Name: same-module Mac symbol CEmitter::UpdateAngles. The Windows caller
 * carries this in EAX as an internal optimizer convention. */
void CEmitter::UpdateAngles()
{
    /* 0x005b9bf8 = 0x3c23d70a: one hundredth per millisecond. */
    const float angularTimeScale =
        static_cast<float>(fxFrameTime) * 0.009999999776482582f;
    for (int component = 0; component < 3; ++component) {
        angles[component] +=
            angularVelocity[component] * angularTimeScale;
    }

    vec3_t right;
    AngleVectors(angles, renderEntity.axis[0], right,
                 renderEntity.axis[2]);
    for (int component = 0; component < 3; ++component) {
        renderEntity.axis[1][component] = -right[component];
    }
}

/* Source: CoDUOMP.exe 0x004a31b0..0x004a3a75.
 * Name: same-module Mac symbol CEmitter::Update. The Windows exporter missed
 * this complete function because Ghidra did not establish its entry boundary.
 *
 * Child effects are sampled at 12 ms intervals, but emitted by distance. When
 * a sample crosses the current density radius, the original applies one
 * Newton step to the ballistic distance polynomial before committing the
 * emitter origin, velocity, and time. */
qboolean CEmitter::Update()
{
    constexpr float angularVelocityDamping = 0.699999988079071f;
    constexpr float millisecondsToSeconds = 0.0010000000474974513f;
    constexpr float half = 0.5f;
    constexpr float newtonCrossTerm = 3.0f;
    constexpr float newtonAccelerationTerm = 0.25f;
    constexpr float millisecondsPerSecond = 1000.0f;
    constexpr double fistpBias = 9.313225746154785e-10;
    constexpr int32_t sampleMilliseconds = 12;

    if (timeStart > fxCurrentTime) {
        return qfalse;
    }

    vec3_t previousOrigin;
    for (int component = 0; component < 3; ++component) {
        previousOrigin[component] = origin[component];
    }
    if (UpdateOrigin() == qfalse) {
        return qfalse;
    }

    if (origin[0] == previousOrigin[0] &&
        origin[1] == previousOrigin[1] &&
        origin[2] == previousOrigin[2]) {
        for (int component = 0; component < 3; ++component) {
            angularVelocity[component] *= angularVelocityDamping;
        }
    }
    UpdateAngles();
    UpdateSize();

    orientation_t *boltOrientation = nullptr;
    if (boltFrame.frame != nullptr) {
        boltOrientation = CFxBoltFrame_GetOrientation(boltFrame.frame);
        if (boltOrientation == nullptr) {
            return qfalse;
        }
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    vec3_t boltMotion = {0.0f, 0.0f, 0.0f};
    if ((flags & FX_EFFECT_FLAG_RENDER_EFFECT) != 0U) {
        if (boltOrientation != nullptr) {
            OrientationPosToWorldPos(boltOrientation, origin,
                                     renderEntity.origin);
            for (int row = 0; row < 3; ++row) {
                vec3_t scaledAxis;
                for (int component = 0; component < 3; ++component) {
                    scaledAxis[component] =
                        renderEntity.axis[row][component] *
                        renderEntity.radius;
                }
                OrientationDirToWorldDir(boltOrientation, scaledAxis,
                                         renderEntity.axis[row]);
            }
            renderEntity.nonNormalizedAxes = renderEntity.radius;

            if (lastEmitTime < fxCurrentTime) {
                const float elapsed =
                    static_cast<float>(fxCurrentTime - lastEmitTime) *
                    millisecondsToSeconds;
                for (int component = 0; component < 3; ++component) {
                    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                    boltMotion[component] =
                        (boltOrientation->origin[component] -
                         emitterBoltOrigin[component]) * elapsed;
                }
            }
        } else {
            for (int component = 0; component < 3; ++component) {
                renderEntity.origin[component] = origin[component];
            }
            for (int row = 0; row < 3; ++row) {
                for (int component = 0; component < 3; ++component) {
                    renderEntity.axis[row][component] *=
                        renderEntity.radius;
                }
            }
            renderEntity.nonNormalizedAxes = renderEntity.radius;

            if (lastEmitTime < fxCurrentTime) {
                const float elapsed =
                    static_cast<float>(fxCurrentTime - lastEmitTime) *
                    millisecondsToSeconds;
                for (int component = 0; component < 3; ++component) {
                    /* The same preserved seconds-squared stock path is used
                     * without a bolt orientation; see the report linked in
                     * the oriented branch above. */
                    boltMotion[component] =
                        (origin[component] - emitterBoltOrigin[component]) *
                        elapsed;
                }
            }
        }
    }

    if ((flags & FX_EFFECT_FLAG_EMIT_EFFECTS) == 0U ||
        lastEmitTime >= fxCurrentTime) {
        return qtrue;
    }

    float densitySquared = currentDensity * currentDensity;
    int32_t sampleDelta = 0;
    int32_t candidateTime = lastEmitTime;
    do {
        sampleDelta += sampleMilliseconds;
        const float sampleSeconds =
            static_cast<float>(sampleDelta) * millisecondsToSeconds;
        const float halfSampleSecondsSquared =
            sampleSeconds * sampleSeconds * half;

        vec3_t previousEmitOrigin;
        vec3_t sampleOrigin;
        for (int component = 0; component < 3; ++component) {
            previousEmitOrigin[component] =
                emitterOrigin[component] + emitterBoltOrigin[component];
            sampleOrigin[component] =
                halfSampleSecondsSquared * acceleration[component] +
                sampleSeconds * emitterVelocity[component] +
                emitterOrigin[component];
            if (boltFrame.frame != nullptr) {
                sampleOrigin[component] +=
                    sampleSeconds * boltMotion[component];
            }
        }

        vec3_t displacement;
        for (int component = 0; component < 3; ++component) {
            displacement[component] =
                previousEmitOrigin[component] - sampleOrigin[component];
        }
        const float displacementSquared =
            displacement[2] * displacement[2] +
            displacement[1] * displacement[1] +
            displacement[0] * displacement[0];
        if (displacementSquared < densitySquared) {
            candidateTime += sampleMilliseconds;
            continue;
        }

        if (boltFrame.frame != nullptr) {
            orientation_t *currentBoltOrientation =
                CFxBoltFrame_GetOrientation(boltFrame.frame);
            vec3_t worldSampleOrigin;
            OrientationPosToWorldPos(currentBoltOrientation, sampleOrigin,
                                     worldSampleOrigin);
            CFxScheduler_PlayEntityEffectID(
                &fxScheduler, emitterEffectId, worldSampleOrigin,
                renderEntity.axis, &boltFrame.frame->boltInfo);
        } else {
            CFxScheduler_PlayEntityEffectID(
                &fxScheduler, emitterEffectId, sampleOrigin,
                renderEntity.axis, nullptr);
        }

        const float accelerationSquared =
            acceleration[2] * acceleration[2] +
            acceleration[1] * acceleration[1] +
            acceleration[0] * acceleration[0];
        const float velocityDotAcceleration =
            emitterVelocity[2] * acceleration[2] +
            emitterVelocity[1] * acceleration[1] +
            emitterVelocity[0] * acceleration[0];
        const float velocitySquared =
            emitterVelocity[0] * emitterVelocity[0] +
            emitterVelocity[1] * emitterVelocity[1] +
            emitterVelocity[2] * emitterVelocity[2];

        const float sampleSecondsSquared =
            sampleSeconds * sampleSeconds;
        const float sampleSecondsCubed =
            sampleSecondsSquared * sampleSeconds;
        const float sampleSecondsFourth =
            sampleSecondsCubed * sampleSeconds;
        const float distanceDerivative =
            sampleSecondsCubed * accelerationSquared +
            sampleSecondsSquared * velocityDotAcceleration *
                newtonCrossTerm +
            sampleSeconds * velocitySquared * 2.0f;

        int32_t committedDelta = sampleDelta;
        float committedSeconds = sampleSeconds;
        if (distanceDerivative != 0.0f) {
            const float distanceError =
                sampleSecondsFourth * accelerationSquared *
                    newtonAccelerationTerm +
                sampleSecondsCubed * velocityDotAcceleration +
                sampleSecondsSquared * velocitySquared -
                densitySquared;
            const float correctionMilliseconds =
                distanceError / distanceDerivative *
                millisecondsPerSecond;
            const int32_t correction = static_cast<int32_t>(
                std::nearbyint(static_cast<double>(
                    correctionMilliseconds) + fistpBias));
            committedDelta = sampleDelta - correction;

            const int32_t sampledTime =
                candidateTime - lastEmitTime;
            if (committedDelta < sampledTime) {
                committedDelta = sampledTime;
            }
            committedSeconds =
                static_cast<float>(committedDelta) *
                millisecondsToSeconds;
            const float committedHalfSecondsSquared =
                committedSeconds * committedSeconds * half;
            for (int component = 0; component < 3; ++component) {
                sampleOrigin[component] =
                    committedHalfSecondsSquared *
                        acceleration[component] +
                    committedSeconds * emitterVelocity[component] +
                    emitterOrigin[component];
            }
        }

        for (int component = 0; component < 3; ++component) {
            emitterOrigin[component] = sampleOrigin[component];
            emitterVelocity[component] +=
                committedSeconds * acceleration[component];
            emitterBoltOrigin[component] +=
                committedSeconds * boltMotion[component];
        }
        lastEmitTime += committedDelta;
        candidateTime = lastEmitTime;
        sampleDelta = 0;

        constexpr double randomUnit = 1.0 / 32768.0;
        const double randomSigned =
            static_cast<double>(coduo_crt_rand()) *
                randomUnit * 2.0 - 1.0;
        currentDensity = static_cast<float>(
            randomSigned * static_cast<double>(variance) +
            static_cast<double>(density));
        densitySquared = currentDensity * currentDensity;
    } while (candidateTime < fxCurrentTime);

    return qtrue;
}

/* Source: CoDUOMP.exe 0x004a4920..0x004a4a9e. The Mac traceback table has no
 * surviving member name; RotatePoints describes the proven in-place two-axis
 * rotation of CDecal::points. The sole caller is CDecal::Update. */
void CDecal::RotatePoints()
{
    constexpr float timeScale = 0.009999999776482582f;
    constexpr float pi = 3.1415927410125732f;
    constexpr float degreesPerHalfTurn = 180.0f;
    const float frameTime = static_cast<float>(fxFrameTime);
    const float firstAngle = static_cast<float>(
        static_cast<double>(frameTime) * angularVelocity[1] * timeScale *
        pi / degreesPerHalfTurn);
    float firstSin;
    float firstCos;
    coduo_x87_sincosf(firstAngle, &firstSin, &firstCos);
    const float secondAngle = static_cast<float>(
        static_cast<double>(frameTime) * angularVelocity[0] * timeScale *
        pi / degreesPerHalfTurn);
    float secondSin;
    float secondCos;
    coduo_x87_sincosf(secondAngle, &secondSin, &secondCos);

    /* The factory and archive loader validate pointCount against the owned
     * CDecal array before this update path can run. */
    for (int32_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
        const float x = points[pointIndex][0];
        const float y = points[pointIndex][1];
        const float z = points[pointIndex][2];
        vec3_t rotated;
        rotated[0] = static_cast<float>(
            static_cast<double>(secondCos) * firstSin * y +
            static_cast<double>(secondSin) * firstSin * z +
            static_cast<double>(firstCos) * x);
        rotated[1] = static_cast<float>(
            static_cast<double>(secondCos) * firstCos * y +
            static_cast<double>(secondSin) * firstCos * z -
            static_cast<double>(firstSin) * x);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        rotated[2] = static_cast<float>(
            -static_cast<double>(secondSin) * y +
            static_cast<double>(secondCos) * z + x);
        for (int component = 0; component < 3; ++component) {
            points[pointIndex][component] = rotated[component];
        }
    }
}

/* Source: CoDUOMP.exe 0x004a4aa0..0x004a4b89, recovered after repairing the
 * missing Ghidra function boundary. The type-15 polygon runtime object is
 * constructed from parsed decal templates. */
qboolean CDecal::Update()
{
    if (timeStart > fxCurrentTime) {
        return qfalse;
    }

    UpdateRGB();
    UpdateAlpha();
    if (fxCurrentTime <= rotationStartTime) {
        return qtrue;
    }

    vec3_t previousOrigin;
    for (int component = 0; component < 3; ++component) {
        previousOrigin[component] = origin[component];
    }
    if (UpdateOrigin() == qfalse) {
        return qfalse;
    }
    RotatePoints();

    for (int component = 0; component < 3; ++component) {
        if (origin[component] != previousOrigin[component]) {
            return qtrue;
        }
    }

    /* 0x005b9f30 = 0x3bd4fdf3. */
    constexpr float angularDampingScale = 0.0064999996684491634f;
    const long double damping =
        1.0L - static_cast<long double>(fxFrameTime) *
                   static_cast<long double>(angularDampingScale);
    for (int component = 0; component < 3; ++component) {
        angularVelocity[component] = static_cast<float>(
            damping *
            static_cast<long double>(angularVelocity[component]));
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004a4500..0x004a45d5, recovered from a missing Ghidra
 * function boundary. Class binding: Windows vtable 0x005a2be8. The original
 * keeps the lifetime weights in x87 registers while updating all four vertex
 * tracks; double avoids additional float-rounding between those stores. */
qboolean CQuad::Update()
{
    if (timeStart > fxCurrentTime) {
        return qfalse;
    }

    const double startWeight =
        static_cast<double>(timeEnd - fxCurrentTime) /
        static_cast<double>(timeEnd - timeStart);
    const double endWeight = 1.0 - startWeight;

    for (int vertexIndex = 0; vertexIndex < 4; ++vertexIndex) {
        fx_quad_vertex_t &vertex = vertices[vertexIndex];

        vertex.alpha = static_cast<float>(
            endWeight * vertex.alphaEnd +
            startWeight * vertex.alphaStart);
        if (vertex.alpha < 0.0f) {
            vertex.alpha = 0.0f;
        }

        for (int component = 0; component < 3; ++component) {
            vertex.color[component] = static_cast<float>(
                startWeight * vertex.colorStart[component] +
                endWeight * vertex.colorEnd[component]);
        }

        vertex.textureCoordinates[0] = static_cast<float>(
            startWeight * vertex.textureCoordinatesStart[0] +
            endWeight * vertex.textureCoordinatesEnd[0]);
        if (vertex.textureCoordinates[0] > 1.0f) {
            vertex.textureCoordinates[0] = 1.0f;
        }
        vertex.textureCoordinates[1] = static_cast<float>(
            startWeight * vertex.textureCoordinatesStart[1] +
            endWeight * vertex.textureCoordinatesEnd[1]);
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004a4d70..0x004a4d7b.
 * Name and class binding: Windows vtable 0x005a2bcc and same-module Mac symbol
 * CFlash::Update. */
qboolean CFlash::Update()
{
    UpdateRGB();
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004a3d90..0x004a3df9.
 * Name: same-module Mac symbol CLight::Update. */
qboolean CLight::Update()
{
    if (timeStart > fxCurrentTime) {
        return qfalse;
    }

    UpdateSize();
    UpdateRGB();

    if (boltFrame.frame != nullptr) {
        orientation_t *boltOrientation =
            CFxBoltFrame_GetOrientation(boltFrame.frame);
        if (boltOrientation == nullptr) {
            return qfalse;
        }
        OrientationPosToWorldPos(boltOrientation, origin,
                                 renderEntity.origin);
    } else {
        for (int component = 0; component < 3; ++component) {
            renderEntity.origin[component] = origin[component];
        }
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004a3e00..0x004a3f3c.
 * Name: same-module Mac symbol CLight::UpdateSize; the Windows writes to
 * renderEntity.radius prove the channel binding. */
void CLight::UpdateSize()
{
    enum {
        FX_LIGHT_SIZE_LERP = 0x100,
        FX_LIGHT_SIZE_RANDOM = 0x200,
        FX_LIGHT_SIZE_MODE_MASK = 0xc00,
        FX_LIGHT_SIZE_MODE_DELAYED = 0x400,
        FX_LIGHT_SIZE_MODE_COSINE = 0x800,
        FX_LIGHT_SIZE_MODE_EARLY = 0xc00
    };

    const float startWeight = coduomp_fx_evaluate_channel_start_weight(
        lerpFlags, sizeTime,
        FX_LIGHT_SIZE_LERP, FX_LIGHT_SIZE_RANDOM,
        FX_LIGHT_SIZE_MODE_MASK, FX_LIGHT_SIZE_MODE_DELAYED,
        FX_LIGHT_SIZE_MODE_COSINE, FX_LIGHT_SIZE_MODE_EARLY,
        timeStart, timeEnd, false);
    const float endWeight = 1.0f - startWeight;
    renderEntity.radius =
        sizeEnd * endWeight + sizeStart * startWeight;
}

/* Source: CoDUOMP.exe 0x004a3f40..0x004a40af.
 * Name: same-module Mac symbol CLight::UpdateRGB; the Windows writes the three
 * interpolated color components into renderEntity.lightingOrigin. */
void CLight::UpdateRGB()
{
    enum {
        FX_LIGHT_COLOR_LERP = 0x10,
        FX_LIGHT_COLOR_RANDOM = 0x20,
        FX_LIGHT_COLOR_MODE_MASK = 0xc0,
        FX_LIGHT_COLOR_MODE_DELAYED = 0x40,
        FX_LIGHT_COLOR_MODE_COSINE = 0x80,
        FX_LIGHT_COLOR_MODE_EARLY = 0xc0
    };

    const float startWeight = coduomp_fx_evaluate_channel_start_weight(
        lerpFlags, colorTime,
        FX_LIGHT_COLOR_LERP, FX_LIGHT_COLOR_RANDOM,
        FX_LIGHT_COLOR_MODE_MASK, FX_LIGHT_COLOR_MODE_DELAYED,
        FX_LIGHT_COLOR_MODE_COSINE, FX_LIGHT_COLOR_MODE_EARLY,
        timeStart, timeEnd, false);
    const float endWeight = 1.0f - startWeight;
    for (int component = 0; component < 3; ++component) {
        renderEntity.lightingOrigin[component] =
            colorStart[component] * startWeight +
            colorEnd[component] * endWeight;
    }
}
