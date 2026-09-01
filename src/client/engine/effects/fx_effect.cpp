#include "fx_classes.hpp"
#include "fx_archive.h"

#include <string.h>

/* Source: CoDUOMP.exe 0x004a0e40..0x004a0e50.
 * Name: same-module Mac symbol CEffect::CEffect. */
CEffect::CEffect()
{
    boltFrame.frame = nullptr;
}

/* Source: CoDUOMP.exe 0x004a0e90..0x004a0ea5.
 * Name: same-module Mac symbol CEffect::~CEffect. */
CEffect::~CEffect()
{
    CFxBoltFramePtr_Destroy(&boltFrame);
}

/* Source: CoDUOMP.exe 0x004aebd0..0x004aebe6.
 * The Windows LTCG calling convention carries this in EAX and the two raw
 * float words on the stack. The Mac compiler inlined this accessor, so the
 * name is derived from the proved refEntity+0x70 destination. */
void CEffect::SetShaderTexCoord(float s, float t)
{
    memcpy(&renderEntity.shaderTexCoord[0], &s,
           sizeof(renderEntity.shaderTexCoord[0]));
    memcpy(&renderEntity.shaderTexCoord[1], &t,
           sizeof(renderEntity.shaderTexCoord[1]));
}

/* Source: CoDUOMP.exe 0x004aebf0..0x004aec11.
 * Name is derived from the proved CEffect+0x20 destination field. The
 * nonnull path transports three raw float dwords. */
void CEffect::SetTraceMins(const vec3_t value)
{
    if (value != nullptr) {
        memcpy(&traceMins[0], &value[0], sizeof(traceMins[0]));
        memcpy(&traceMins[1], &value[1], sizeof(traceMins[1]));
        memcpy(&traceMins[2], &value[2], sizeof(traceMins[2]));
    } else {
        traceMins[2] = 0.0f;
        traceMins[1] = 0.0f;
        traceMins[0] = 0.0f;
    }
}

/* Source: CoDUOMP.exe 0x004aec20..0x004aec41.
 * Name is derived from the proved CEffect+0x2c destination field. The
 * nonnull path transports three raw float dwords. */
void CEffect::SetTraceMaxs(const vec3_t value)
{
    if (value != nullptr) {
        memcpy(&traceMaxs[0], &value[0], sizeof(traceMaxs[0]));
        memcpy(&traceMaxs[1], &value[1], sizeof(traceMaxs[1]));
        memcpy(&traceMaxs[2], &value[2], sizeof(traceMaxs[2]));
    } else {
        traceMaxs[2] = 0.0f;
        traceMaxs[1] = 0.0f;
        traceMaxs[0] = 0.0f;
    }
}

/* Source: CoDUOMP.exe 0x004aec50..0x004aec53. */
void CEffect::SetFlags(uint32_t value)
{
    flags = value;
}

/* Source: CoDUOMP.exe 0x004aec60..0x004aec6a. */
void CEffect::ClearFlags(uint32_t mask)
{
    flags &= ~mask;
}

/* Source: CoDUOMP.exe 0x004aec70..0x004aec73. */
void CEffect::SetLerpFlags(int32_t value)
{
    lerpFlags = value;
}

/* Source: CoDUOMP.exe 0x004aec80..0x004aeca1.
 * Name is derived from the proved CEffect+0x04 destination field. */
void CEffect::SetOrigin(const vec3_t value)
{
    if (value != nullptr) {
        /* 0x004aec86..0x004aec94 transports three raw dwords. */
        memcpy(&origin[0], &value[0], sizeof(origin[0]));
        memcpy(&origin[1], &value[1], sizeof(origin[1]));
        memcpy(&origin[2], &value[2], sizeof(origin[2]));
    } else {
        origin[2] = 0.0f;
        origin[1] = 0.0f;
        origin[0] = 0.0f;
    }
}

/* Source: CoDUOMP.exe 0x004af3e0..0x004af3fc.
 * The Mac compiler inlined this operation, so the name is derived from the
 * proved refEntity lighting-origin destination and RF_LIGHTING_ORIGIN bit.
 * Unlike the general vector setters, a null input deliberately does nothing. */
void CEffect::SetLightingOrigin(const vec3_t value)
{
    if (value == nullptr) {
        return;
    }
    memcpy(&renderEntity.lightingOrigin[0], &value[0],
           sizeof(renderEntity.lightingOrigin[0]));
    memcpy(&renderEntity.lightingOrigin[1], &value[1],
           sizeof(renderEntity.lightingOrigin[1]));
    memcpy(&renderEntity.lightingOrigin[2], &value[2],
           sizeof(renderEntity.lightingOrigin[2]));
    renderEntity.renderfx |= FX_RENDER_FLAG_USE_LIGHTING_ORIGIN;
}

/* Source: CoDUOMP.exe 0x004aecb0..0x004aecb3. */
int32_t CEffect::GetTimeStart() const
{
    return timeStart;
}

/* Source: CoDUOMP.exe 0x004a5010..0x004a5029.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004a5010_004a502a.mcode.
 * Name: exact same-module Mac symbol CEffect::SetTimeStart(int). The literal
 * is the exact float32 value 0x3a83126f; conceptually it converts integer
 * milliseconds to renderer shader seconds. */
void CEffect::SetTimeStart(int32_t value)
{
    constexpr float millisecondsToSeconds =
        0.0010000000474974513f;

    timeStart = value;
    renderEntity.shaderTime = static_cast<float>(
        static_cast<long double>(value) *
        static_cast<long double>(millisecondsToSeconds));
}

/* Source: CoDUOMP.exe 0x004aecc0..0x004aecc3. */
void CEffect::SetTimeEnd(int32_t value)
{
    timeEnd = value;
}

/* Source: CoDUOMP.exe 0x004aecd0..0x004aecd3.
 * The dword destination is consumed as an effect id by particle impact code. */
void CEffect::SetImpactEffectID(int32_t effectId)
{
    impactResource.effectId = effectId;
}

/* Source: CoDUOMP.exe 0x004aece0..0x004aece3.
 * The dword destination is consumed as an effect id by CParticle::Die. */
void CEffect::SetDeathEffectID(int32_t effectId)
{
    deathResource.effectId = effectId;
}

/* Source: CoDUOMP.exe 0x004a0eb0.  Name and vtable slot are proven by the
 * same-module Mac CEffect symbols and the Windows vtable at 0x005a1fe0. */
void CEffect::Die()
{
}

/* Source: CoDUOMP.exe 0x004a0ec0..0x004a0ec2. */
qboolean CEffect::Cull()
{
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004a0ed0. */
void CEffect::Draw()
{
}

/* Source: CoDUOMP.exe 0x004a0ee0..0x004a0eec. */
qboolean CEffect::Update()
{
    return timeStart <= fxCurrentTime ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x004a0ef0..0x004a0ef2. */
int32_t CEffect::TypeID()
{
    return FX_EFFECT_TYPE_BASE;
}

/* Source: CoDUOMP.exe 0x004a0f00..0x004a10be.
 * Name: same-module Mac symbol CEffect::Archive.  The original archive is a
 * platform-local development save: its raw refEntity block has the native
 * renderer layout, followed by explicit resource-handle fixups. */
void CEffect::Archive(fx_archive_t &archive)
{
    CFxArchive_ArchiveVec3(&archive, origin);
    CFxArchive_ArchiveInt(&archive, &timeStart);
    CFxArchive_ArchiveInt(&archive, &timeEnd);
    CFxArchive_ArchiveInt(&archive, reinterpret_cast<int32_t *>(&flags));
    CFxArchive_ArchiveInt(&archive, &lerpFlags);
    CFxArchive_ArchiveVec3(&archive, traceMins);
    CFxArchive_ArchiveVec3(&archive, traceMaxs);
    CFxArchive_ArchiveModel(&archive, &impactResource.model);
    CFxArchive_ArchiveModel(&archive, &deathResource.model);
    CFxArchive_ArchiveData(&archive, &renderEntity,
                           static_cast<int32_t>(sizeof(renderEntity)));
    CFxArchive_ArchiveShader(&archive, &renderEntity.spriteShaderHandle);
    CFxArchive_ArchiveEffectID(&archive, &renderEntity.dobj);
    CFxBoltFramePtr_Archive(&boltFrame, &archive);
}
