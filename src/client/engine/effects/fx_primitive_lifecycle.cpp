#include "fx_classes.hpp"

#include "../math/vector_math.h"
#include "../platform/crt_boundary.h"

#include <cmath>
#include <string.h>

/* These constructors contain the inlined CEffect constructor followed by the
 * derived vtable and renderer-type assignments. */

/* Source: CoDUOMP.exe 0x004a2440..0x004a2457. */
COrientedParticle::COrientedParticle()
{
    renderEntity.reType = RT_ORIENTED_QUAD;
}

/* Source: CoDUOMP.exe 0x004af180..0x004af19a.
 * Name follows the proved COrientedParticle+0x154 orientation field. */
void COrientedParticle::SetOrientation(const vec3_t value)
{
    memcpy(&orientation[0], &value[0], sizeof(orientation[0]));
    memcpy(&orientation[1], &value[1], sizeof(orientation[1]));
    memcpy(&orientation[2], &value[2], sizeof(orientation[2]));
}

/* Source: CoDUOMP.exe 0x004a2600..0x004a2617. */
CLine::CLine()
{
    renderEntity.reType = RT_LINE;
}

/* Source: CoDUOMP.exe 0x004af130..0x004af14a.
 * The Mac compiler inlined this operation; the role name follows CLine+0x154. */
void CLine::SetEnd(const vec3_t value)
{
    memcpy(&end[0], &value[0], sizeof(end[0]));
    memcpy(&end[1], &value[1], sizeof(end[1]));
    memcpy(&end[2], &value[2], sizeof(end[2]));
}

/* Source: CoDUOMP.exe 0x004a27c0..0x004a27d7. */
CElectricity::CElectricity()
{
    renderEntity.reType = RT_ELECTRICITY;
}

/* Source: CoDUOMP.exe 0x004af160..0x004af16c.
 * Name follows the proved CElectricity renderer parameter field. */
void CElectricity::SetElectricityParm(float value)
{
    memcpy(&electricityParm, &value, sizeof(electricityParm));
}

/* Source: CoDUOMP.exe 0x004a2850..0x004a28c8.
 * Name: same-module Mac symbol CElectricity::Initialize. The Windows LTCG
 * caller at 0x004b0b78 carries `this` in ESI rather than the public ABI. */
void CElectricity::Initialize()
{
    constexpr float kRandomUnit = 3.0517578125e-05f;
    constexpr float kRandomFrameScale = 1265536.0f;

    renderEntity.frame = static_cast<int32_t>(
        static_cast<float>(coduo_crt_rand()) *
        kRandomUnit * kRandomFrameScale);
    renderEntity.electricityEndTime =
        static_cast<float>(timeEnd - timeStart + fxLastTime);

    if ((flags & FX_EFFECT_FLAG_DEPTH_HACK) != 0U) {
        renderEntity.renderfx |= FX_RENDER_FLAG_DEPTH_HACK;
    }
    if ((flags & FX_EFFECT_FLAG_ELECTRICITY_OPTION_A) != 0U) {
        renderEntity.renderfx |= FX_RENDER_FLAG_ELECTRICITY_OPTION_A;
    }
    if ((flags & FX_EFFECT_FLAG_RENDER_EFFECT) != 0U) {
        renderEntity.renderfx |= FX_RENDER_FLAG_ELECTRICITY_EFFECT;
    }
    if ((flags & FX_EFFECT_FLAG_ELECTRICITY_OPTION_B) != 0U) {
        renderEntity.renderfx |= FX_RENDER_FLAG_ELECTRICITY_OPTION_B;
    }
}

/* Source: CoDUOMP.exe 0x004a2a40..0x004a2a57. */
CTail::CTail()
{
    renderEntity.reType = RT_LINE;
}

/* Source: CoDUOMP.exe 0x004af1b0..0x004af1dc.
 * These role names follow CTail's proved length animation channel. */
void CTail::SetLengthStart(float value)
{
    memcpy(&lengthStart, &value, sizeof(lengthStart));
}

void CTail::SetLengthEnd(float value)
{
    memcpy(&lengthEnd, &value, sizeof(lengthEnd));
}

void CTail::SetLengthTime(float value)
{
    memcpy(&lengthTime, &value, sizeof(lengthTime));
}

/* Source: CoDUOMP.exe 0x004a2ee0..0x004a2ef7. */
CCylinder::CCylinder()
{
    renderEntity.reType = RT_CYLINDER;
}

/* Source: CoDUOMP.exe 0x004af1f0..0x004af231.
 * The cylinder uses the inherited secondary-size channel for its second
 * animated radius. These class-specific role names distinguish the separate
 * Windows out-of-line copies from CParticle's general size2 setters. */
void CCylinder::SetSecondarySizeStart(float value)
{
    memcpy(&size2Start, &value, sizeof(size2Start));
}

void CCylinder::SetSecondarySizeEnd(float value)
{
    memcpy(&size2End, &value, sizeof(size2End));
}

void CCylinder::SetSecondarySizeTime(float value)
{
    memcpy(&size2Time, &value, sizeof(size2Time));
}

void CCylinder::SetDirection(const vec3_t value)
{
    memcpy(&renderEntity.axis[0][0], &value[0],
           sizeof(renderEntity.axis[0][0]));
    memcpy(&renderEntity.axis[0][1], &value[1],
           sizeof(renderEntity.axis[0][1]));
    memcpy(&renderEntity.axis[0][2], &value[2],
           sizeof(renderEntity.axis[0][2]));
}

/* Source: CoDUOMP.exe 0x004a30c0..0x004a312f.
 * Name: same-module Mac symbol CEmitter::CEmitter. */
CEmitter::CEmitter()
{
    renderEntity.reType = RT_MODEL;
    for (int component = 0; component < 3; ++component) {
        emitterOrigin[component] = 0.0f;
        emitterVelocity[component] = 0.0f;
        emitterBoltOrigin[component] = 0.0f;
        angles[component] = 0.0f;
        angularVelocity[component] = 0.0f;
    }
}

/* Source: CoDUOMP.exe 0x004af250..0x004af316.
 * The Mac compiler inlined these accessors. Names follow the proved CEmitter
 * fields and the semantic arguments of FX_AddEmitter. */
void CEmitter::SetModel(DObj *value)
{
    renderEntity.dobj = value;
}

void CEmitter::SetAngles(const vec3_t value)
{
    if (value != nullptr) {
        memcpy(&angles[0], &value[0], sizeof(angles[0]));
        memcpy(&angles[1], &value[1], sizeof(angles[1]));
        memcpy(&angles[2], &value[2], sizeof(angles[2]));
    } else {
        angles[2] = 0.0f;
        angles[1] = 0.0f;
        angles[0] = 0.0f;
    }
}

void CEmitter::SetAngularVelocity(const vec3_t value)
{
    if (value != nullptr) {
        memcpy(&angularVelocity[0], &value[0],
               sizeof(angularVelocity[0]));
        memcpy(&angularVelocity[1], &value[1],
               sizeof(angularVelocity[1]));
        memcpy(&angularVelocity[2], &value[2],
               sizeof(angularVelocity[2]));
    } else {
        angularVelocity[2] = 0.0f;
        angularVelocity[1] = 0.0f;
        angularVelocity[0] = 0.0f;
    }
}

void CEmitter::SetEmitterEffectID(int32_t value)
{
    emitterEffectId = value;
}

void CEmitter::SetDensity(float value)
{
    memcpy(&density, &value, sizeof(density));
}

void CEmitter::SetVariance(float value)
{
    memcpy(&variance, &value, sizeof(variance));
}

void CEmitter::SetLastEmitTime(int32_t value)
{
    lastEmitTime = value;
}

/* Source: CoDUOMP.exe 0x004a0900..0x004a092d, recovered from an exporter
 * function-boundary gap. The exact source name was not retained;
 * RandomizeDensity describes the proved write to currentDensity. The same
 * expression is inlined in FX_AddEmitter and CEmitter::Update. */
void CEmitter::RandomizeDensity()
{
    constexpr double randomUnit = 3.0517578125e-05;
    const double randomSigned =
        static_cast<double>(coduo_crt_rand()) *
            randomUnit * 2.0 - 1.0;
    currentDensity = static_cast<float>(
        randomSigned * static_cast<double>(variance) +
        static_cast<double>(density));
}

/* Source: CoDUOMP.exe 0x004af320..0x004af353.
 * The Mac compiler inlined this operation, so the method name is derived from
 * the proved CEmitter+0x154 destination field. The Windows LTCG calling
 * convention carries this in EAX and value in ECX. */
void CEmitter::SetEmitterOrigin(const vec3_t value)
{
    if (value != nullptr) {
        memcpy(&emitterOrigin[0], &value[0], sizeof(emitterOrigin[0]));
        memcpy(&emitterOrigin[1], &value[1], sizeof(emitterOrigin[1]));
        memcpy(&emitterOrigin[2], &value[2], sizeof(emitterOrigin[2]));
    } else {
        emitterOrigin[2] = 0.0f;
        emitterOrigin[1] = 0.0f;
        emitterOrigin[0] = 0.0f;
    }
}

/* Source: CoDUOMP.exe 0x004af360..0x004af393.
 * Name is derived from the proved CEmitter+0x160 destination field. */
void CEmitter::SetEmitterVelocity(const vec3_t value)
{
    if (value != nullptr) {
        memcpy(&emitterVelocity[0], &value[0],
               sizeof(emitterVelocity[0]));
        memcpy(&emitterVelocity[1], &value[1],
               sizeof(emitterVelocity[1]));
        memcpy(&emitterVelocity[2], &value[2],
               sizeof(emitterVelocity[2]));
    } else {
        emitterVelocity[2] = 0.0f;
        emitterVelocity[1] = 0.0f;
        emitterVelocity[0] = 0.0f;
    }
}

/* Source: CoDUOMP.exe 0x004af3a0..0x004af3d3.
 * The factory passes the current bolt orientation's world origin, and the
 * emitter update advances this component independently of its local ballistic
 * origin. The Mac compiler inlined the operation, so the name is role-based. */
void CEmitter::SetEmitterBoltOrigin(const vec3_t value)
{
    if (value != nullptr) {
        memcpy(&emitterBoltOrigin[0], &value[0],
               sizeof(emitterBoltOrigin[0]));
        memcpy(&emitterBoltOrigin[1], &value[1],
               sizeof(emitterBoltOrigin[1]));
        memcpy(&emitterBoltOrigin[2], &value[2],
               sizeof(emitterBoltOrigin[2]));
    } else {
        emitterBoltOrigin[2] = 0.0f;
        emitterBoltOrigin[1] = 0.0f;
        emitterBoltOrigin[0] = 0.0f;
    }
}

/* Source: CoDUOMP.exe 0x004af410..0x004af416.
 * Names for these three accessors are role-based because the Mac compiler
 * inlined them; the Windows destination fields prove their operations. */
void CDecal::SetPointCount(int32_t value)
{
    pointCount = value;
}

/* Source: CoDUOMP.exe 0x004af420..0x004af453. */
void CDecal::SetAngularVelocity(const vec3_t value)
{
    if (value != nullptr) {
        memcpy(&angularVelocity[0], &value[0],
               sizeof(angularVelocity[0]));
        memcpy(&angularVelocity[1], &value[1],
               sizeof(angularVelocity[1]));
        memcpy(&angularVelocity[2], &value[2],
               sizeof(angularVelocity[2]));
    } else {
        angularVelocity[2] = 0.0f;
        angularVelocity[1] = 0.0f;
        angularVelocity[0] = 0.0f;
    }
}

/* Source: CoDUOMP.exe 0x004af460..0x004af46e.
 * The input is a relative delay; the stored field is the absolute FX time at
 * which point rotation begins. */
void CDecal::SetRotationDelay(int32_t delay)
{
    const uint32_t absoluteTimeBits =
        static_cast<uint32_t>(fxCurrentTime) +
        static_cast<uint32_t>(delay);
    memcpy(&rotationStartTime, &absoluteTimeBits,
           sizeof(rotationStartTime));
}

/* Source: CoDUOMP.exe 0x004a3cd0..0x004a3ce0. */
CLight::CLight() = default;

/* Source: CoDUOMP.exe 0x004aed80..0x004aee3d.
 * The Mac compiler inlined these accessors. Their role names follow the
 * proved CLight fields and the semantic channel arguments to FX_AddLight and
 * FX_AddFlash. The retained Windows copies are not called by those factories
 * because LTCG inlined the same assignments there. */
void CLight::SetSizeStart(float value)
{
    memcpy(&sizeStart, &value, sizeof(sizeStart));
}

void CLight::SetSizeEnd(float value)
{
    memcpy(&sizeEnd, &value, sizeof(sizeEnd));
}

void CLight::SetSizeTime(float value)
{
    memcpy(&sizeTime, &value, sizeof(sizeTime));
}

void CLight::SetColorStart(const vec3_t value)
{
    if (value != nullptr) {
        memcpy(&colorStart[0], &value[0], sizeof(colorStart[0]));
        memcpy(&colorStart[1], &value[1], sizeof(colorStart[1]));
        memcpy(&colorStart[2], &value[2], sizeof(colorStart[2]));
    } else {
        colorStart[2] = 0.0f;
        colorStart[1] = 0.0f;
        colorStart[0] = 0.0f;
    }
}

void CLight::SetColorEnd(const vec3_t value)
{
    if (value != nullptr) {
        memcpy(&colorEnd[0], &value[0], sizeof(colorEnd[0]));
        memcpy(&colorEnd[1], &value[1], sizeof(colorEnd[1]));
        memcpy(&colorEnd[2], &value[2], sizeof(colorEnd[2]));
    } else {
        colorEnd[2] = 0.0f;
        colorEnd[1] = 0.0f;
        colorEnd[0] = 0.0f;
    }
}

void CLight::SetColorTime(float value)
{
    memcpy(&colorTime, &value, sizeof(colorTime));
}

/* Source: CoDUOMP.exe 0x004aecf0..0x004aed00.
 * Class binding: Windows vtable 0x005a2be8. */
CQuad::CQuad() = default;

/* Source: CoDUOMP.exe 0x004a4650..0x004a4667. */
CDecal::CDecal()
{
    renderEntity.reType = RT_SPRITE;
}

/* Source: CoDUOMP.exe 0x004a4b90..0x004a4c9f.
 * The type-15 decal's local points are recentered around their centroid, which
 * becomes the effect origin, and the bounding sphere radius is the farthest
 * recentered point. */
void CDecal::Init()
{
    if (pointCount < 3) {
        return;
    }

    /* The factory and archive loader validate pointCount against the owned
     * CDecal arrays before this initializer can run. */
    long double originRaw[3] = {0.0L, 0.0L, 0.0L};
    for (int32_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
        for (int component = 0; component < 3; ++component) {
            originRaw[component] +=
                static_cast<long double>(points[pointIndex][component]);
        }
    }

    const float inversePointCount =
        1.0f / static_cast<float>(pointCount);
    for (int component = 0; component < 3; ++component) {
        origin[component] = static_cast<float>(
            originRaw[component] *
            static_cast<long double>(inversePointCount));
    }

    float radiusSquared = 0.0f;
    for (int32_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
        points[pointIndex][0] = static_cast<float>(
            static_cast<long double>(points[pointIndex][0]) -
            static_cast<long double>(origin[0]));
        points[pointIndex][1] = static_cast<float>(
            static_cast<long double>(points[pointIndex][1]) -
            static_cast<long double>(origin[1]));
        const long double zRaw =
            static_cast<long double>(points[pointIndex][2]) -
            static_cast<long double>(origin[2]);
        points[pointIndex][2] = static_cast<float>(zRaw);
        const float pointRadiusSquared = static_cast<float>(
            zRaw * zRaw +
            static_cast<long double>(points[pointIndex][1]) *
                static_cast<long double>(points[pointIndex][1]) +
            static_cast<long double>(points[pointIndex][0]) *
                static_cast<long double>(points[pointIndex][0]));
        if (pointRadiusSquared > radiusSquared) {
            radiusSquared = pointRadiusSquared;
        }
    }
    renderEntity.radius = std::sqrt(radiusSquared);
}

/* Source: CoDUOMP.exe 0x004aee50..0x004aee60.
 * Name and hierarchy: Windows vtable 0x005a2bcc and the same-module Mac
 * CFlash symbols. The emitted constructor is the fieldless CLight constructor
 * followed by the derived vtable assignment. */
CFlash::CFlash() = default;

/* Source: CoDUOMP.exe 0x004aeea0..0x004aeea6.
 * This retained accessor lies inside the CFlash method bank and writes the
 * proved sprite shader slot at CEffect+0xa8. The Mac compiler inlined the
 * operation, so the role name is derived from the destination field. */
void CFlash::SetSpriteShaderHandle(int32_t value)
{
    renderEntity.spriteShaderHandle = value;
}

/* Source: CoDUOMP.exe 0x004a4d80..0x004a4e7a.
 * Name: same-module Mac symbol CFlash::Init. The original retains the x87
 * intermediate attenuation while scaling all six inherited RGB endpoints;
 * double preserves the float-input products without introducing extra
 * float-rounding steps on modern targets. */
void CFlash::Init()
{
    vec3_t direction;
    for (int component = 0; component < 3; ++component) {
        direction[component] = origin[component] - fxViewOrigin[component];
    }

    const float distance = VectorNormalize(direction);
    const long double facing =
        static_cast<long double>(fxCullPlanes[0].normal[2]) * direction[2] +
        static_cast<long double>(fxCullPlanes[0].normal[1]) * direction[1] +
        static_cast<long double>(fxCullPlanes[0].normal[0]) * direction[0];

    long double attenuation;
    if (distance > 600.0f) {
        attenuation = 0.0L;
    /* 0x004a4de5..0x004a4df0 takes this path for greater, equal, or
     * unordered input: JP observes even parity after TEST AH, 5. */
    } else if (!(facing < 0.5L)) {
        attenuation = facing;
    } else if (distance > 100.0f) {
        attenuation = 0.0L;
    } else {
        /* 0x005b9d6c = 0x3f8ccccd. */
        attenuation = facing + 1.100000023841858f;
    }

    /* 0x005b9d68 = 0x363a69dc, mathematically 1 / (600 * 600). */
    attenuation *=
        1.0L - static_cast<long double>(distance) *
                   static_cast<long double>(distance) *
                   static_cast<long double>(2.7777778086601757e-06f);

    for (int component = 0; component < 3; ++component) {
        colorStart[component] = static_cast<float>(
            attenuation *
            static_cast<long double>(colorStart[component]));
        colorEnd[component] = static_cast<float>(
            attenuation *
            static_cast<long double>(colorEnd[component]));
    }
}

/* Each derived destructor is exactly the inlined CEffect destructor. */

/* Source: CoDUOMP.exe 0x004a2490..0x004a24a5. */
COrientedParticle::~COrientedParticle() = default;
/* Source: CoDUOMP.exe 0x004a2650..0x004a2665. */
CLine::~CLine() = default;
/* Source: CoDUOMP.exe 0x004a2670..0x004a2670.
 * Name and class binding: Windows CLine vtable 0x005a2050. */
void CLine::Die() {}
/* Source: CoDUOMP.exe 0x004a2810..0x004a2825. */
CElectricity::~CElectricity() = default;
/* Source: CoDUOMP.exe 0x004a2830..0x004a2830.
 * Name and class binding: Windows CElectricity vtable 0x005a206c. */
void CElectricity::Die() {}
/* Source: CoDUOMP.exe 0x004a2a90..0x004a2aa5. */
CTail::~CTail() = default;
/* Source: CoDUOMP.exe 0x004a2f30..0x004a2f45. */
CCylinder::~CCylinder() = default;
/* Source: CoDUOMP.exe 0x004a3160..0x004a3175. */
CEmitter::~CEmitter() = default;
/* Source: CoDUOMP.exe 0x004a3d20..0x004a3d35. */
CLight::~CLight() = default;
/* Source: CoDUOMP.exe 0x004aed10..0x004aed25. */
CQuad::~CQuad() = default;
/* Source: CoDUOMP.exe 0x004a46a0..0x004a46b5. */
CDecal::~CDecal() = default;
/* Source: CoDUOMP.exe 0x004aee70..0x004aee85. */
CFlash::~CFlash() = default;
