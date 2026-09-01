/* Primitive-effect archive virtuals recovered from CoDUOMP.exe. */

#include "fx_classes.hpp"
#include "fx_archive.h"

/* Source: CoDUOMP.exe 0x004a25c0..0x004a25f2.
 * Name: same-module Mac symbol COrientedParticle::Archive. */
void COrientedParticle::Archive(fx_archive_t &archive)
{
    CParticle::Archive(archive);
    CFxArchive_ArchiveVec3(&archive, orientation);
}

/* Source: CoDUOMP.exe 0x004a2780..0x004a27b2.
 * Name: same-module Mac symbol CLine::Archive. */
void CLine::Archive(fx_archive_t &archive)
{
    CParticle::Archive(archive);
    CFxArchive_ArchiveVec3(&archive, end);
}

/* Source: CoDUOMP.exe 0x004a29c0..0x004a2a30.
 * Name: same-module Mac symbol CElectricity::Archive. */
void CElectricity::Archive(fx_archive_t &archive)
{
    CParticle::Archive(archive);
    CFxArchive_ArchiveVec3(&archive, end);
    CFxArchive_ArchiveFloat(&archive, &electricityParm);
    CFxArchive_ArchiveData(&archive, rendererSegmentData,
                           static_cast<int32_t>(sizeof(rendererSegmentData)));
}

/* Source: CoDUOMP.exe 0x004a2e20..0x004a2ece.
 * Name: same-module Mac symbol CTail::Archive. */
void CTail::Archive(fx_archive_t &archive)
{
    CParticle::Archive(archive);
    CFxArchive_ArchiveVec3(&archive, end);
    CFxArchive_ArchiveFloat(&archive, &lengthStart);
    CFxArchive_ArchiveFloat(&archive, &lengthEnd);
    CFxArchive_ArchiveFloat(&archive, &lengthTime);
    CFxArchive_ArchiveFloat(&archive, &currentLength);
}

/* Source: CoDUOMP.exe 0x004a30b0..0x004a30b4.
 * The Windows CCylinder vtable points at this tail-call thunk to the byte-
 * identical CTail archive layout. */
void CCylinder::Archive(fx_archive_t &archive)
{
    CTail::Archive(archive);
}

/* Source: CoDUOMP.exe 0x004a3b50..0x004a3cca.
 * Name: same-module Mac symbol CEmitter::Archive. */
void CEmitter::Archive(fx_archive_t &archive)
{
    CParticle::Archive(archive);
    CFxArchive_ArchiveVec3(&archive, emitterOrigin);
    CFxArchive_ArchiveVec3(&archive, emitterVelocity);
    CFxArchive_ArchiveVec3(&archive, emitterBoltOrigin);
    CFxArchive_ArchiveInt(&archive, &lastEmitTime);
    CFxArchive_ArchiveFloat(&archive, &currentDensity);
    CFxArchive_ArchiveVec3(&archive, angles);
    CFxArchive_ArchiveVec3(&archive, angularVelocity);
    /* The original stores emitter effect ids as signed 16-bit scheduler
     * references. On load the archive's effect-reference table resolves the
     * saved index to the current registration id. */
    if (archive.loading != 0) {
        const int16_t reference = CFxArchive_ReadShort(&archive);
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        if ((uint32_t)(int32_t)reference >= (uint32_t)FX_ARCHIVE_MODEL_CAPACITY) {
            Com_Error(ERR_DROP, "\x15" "Loading FX system state: invalid effect reference %i\n", reference);
            emitterEffectId = 0;
        } else {
            emitterEffectId = archive.references[reference].effectId;
        }
    } else {
        CFxArchive_WriteShort(&archive,
                              static_cast<int16_t>(emitterEffectId));
    }
    CFxArchive_ArchiveFloat(&archive, &density);
    CFxArchive_ArchiveFloat(&archive, &variance);

    if (renderEntity.dobj == nullptr) {
        flags &= ~static_cast<uint32_t>(FX_EFFECT_FLAG_RENDER_EFFECT);
    }
}

/* Source: CoDUOMP.exe 0x004a40c0..0x004a418f.
 * Name: same-module Mac symbol CLight::Archive. */
void CLight::Archive(fx_archive_t &archive)
{
    CEffect::Archive(archive);
    CFxArchive_ArchiveFloat(&archive, &sizeStart);
    CFxArchive_ArchiveFloat(&archive, &sizeEnd);
    CFxArchive_ArchiveFloat(&archive, &sizeTime);
    CFxArchive_ArchiveVec3(&archive, colorStart);
    CFxArchive_ArchiveVec3(&archive, colorEnd);
    CFxArchive_ArchiveFloat(&archive, &colorTime);
}

/* Source: CoDUOMP.exe 0x004a45f0..0x004a4645.
 * Class binding: Windows vtable 0x005a2be8. The original archives the complete
 * four-vertex track array as one 0x150-byte record, then the renderer shader. */
void CQuad::Archive(fx_archive_t &archive)
{
    CEffect::Archive(archive);
    CFxArchive_ArchiveData(&archive, vertices,
                           static_cast<int32_t>(sizeof(vertices)));
    CFxArchive_ArchiveShader(&archive, &shaderHandle);
}

/* Source: CoDUOMP.exe 0x004a4cb0..0x004a4d5e. The private FX-state loader's
 * type-15 switch can reconstruct this polygon runtime object. */
void CDecal::Archive(fx_archive_t &archive)
{
    CParticle::Archive(archive);
    CFxArchive_ArchiveData(&archive, &pointCount,
                           static_cast<int32_t>(sizeof(pointCount)));
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if ((uint32_t)pointCount > (uint32_t)FX_DECAL_POINT_CAPACITY) {
        Com_Error(ERR_DROP, "\x15" "FX system state: invalid decal point count %i\n", pointCount);
        return;
    }
    CFxArchive_ArchiveData(&archive, angularVelocity,
                           static_cast<int32_t>(sizeof(angularVelocity)));
    CFxArchive_ArchiveData(
        &archive, &rotationStartTime,
        static_cast<int32_t>(sizeof(rotationStartTime)));
    CFxArchive_ArchiveData(&archive, points,
                           static_cast<int32_t>(sizeof(points)));
    CFxArchive_ArchiveData(
        &archive, textureCoordinates,
                           static_cast<int32_t>(sizeof(textureCoordinates)));
}

/* Source: CoDUOMP.exe 0x004a5000..0x004a5004, a tail jump to
 * CLight::Archive. Name and class binding: Windows vtable 0x005a2bcc and the
 * same-module Mac symbol CFlash::Archive. */
void CFlash::Archive(fx_archive_t &archive)
{
    CLight::Archive(archive);
}
