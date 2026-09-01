#include "fx_classes.hpp"
#include "fx_archive.h"
#include "fx_scheduler.hpp"

#include "../scripting/script_runtime.h"

#include <cstring>

enum {
    FX_SCHEDULER_ARCHIVE_NAME_CAPACITY = 256
};

/* Source: CoDUOMP.exe 0x004a9520..0x004a9626.  The first serialized short is
 * an archive-local effect reference.  Loading replaces it with the int32
 * scheduler effect id installed in CFxArchive::references by the enclosing
 * scheduler archive operation. */
void CFxArchive_ArchiveScheduledEffect(fx_archive_t *archive,
                                       sfx_scheduled_effect_t *effect)
{
    int16_t effectReference;

    if (CFxArchive_IsLoading(archive)) {
        CFxArchive_ArchiveData(archive, &effectReference,
                               sizeof(effectReference));
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        if ((uint32_t)(int32_t)effectReference >= (uint32_t)FX_ARCHIVE_MODEL_CAPACITY) {
            Com_Error(ERR_DROP, "\x15" "Loading FX system state: invalid effect reference %i\n", effectReference);
            effect->effectId = 0;
        } else {
            effect->effectId = archive->references[effectReference].effectId;
        }
    } else {
        effectReference = static_cast<int16_t>(effect->effectId);
        CFxArchive_ArchiveData(archive, &effectReference,
                               sizeof(effectReference));
    }

    CFxArchive_ArchiveData(archive, &effect->primitiveIndex,
                           sizeof(effect->primitiveIndex));
    CFxArchive_ArchiveData(archive, &effect->scheduledTime,
                           sizeof(effect->scheduledTime));
    CFxArchive_ArchiveData(archive, &effect->boltInfo,
                           sizeof(effect->boltInfo));
    CFxArchive_ArchiveData(archive, effect->origin, sizeof(effect->origin));
    CFxArchive_ArchiveData(archive, effect->axis[0], sizeof(effect->axis[0]));
    CFxArchive_ArchiveData(archive, effect->axis[1], sizeof(effect->axis[1]));
    CFxArchive_ArchiveData(archive, effect->axis[2], sizeof(effect->axis[2]));
}

/* Source: CoDUOMP.exe 0x004a92e0..0x004a951d.  On disk the registered-effect
 * map is a sequence of (int16 reference, uint8 name length, name bytes), ended
 * by reference zero.  Scheduled records then follow in list order.  The
 * emitted MSVC map/list node traversal and fixed-pool specializations are
 * represented here by their source-level container and allocator operations. */
void CFxArchive_ArchiveScheduler(fx_archive_t *archive,
                                 cfx_scheduler_t *scheduler)
{
    if (CFxArchive_IsLoading(archive)) {
        int16_t effectReference = CFxArchive_ReadShort(archive);
        while (effectReference != 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            const bool referenceValid =
                (uint32_t)(int32_t)effectReference <
                (uint32_t)FX_ARCHIVE_MODEL_CAPACITY;
            if (!referenceValid) {
                Com_Error(ERR_DROP, "\x15" "Loading FX system state: invalid effect reference %i\n", effectReference);
            }
            uint8_t nameLength = CFxArchive_ReadByte(archive);
            if (nameLength != 0) {
                char name[FX_SCHEDULER_ARCHIVE_NAME_CAPACITY];
                CFxArchive_ReadData(archive, name, nameLength);
                name[nameLength] = '\0';
                if (referenceValid) {
                    archive->references[effectReference].effectId =
                        scheduler->RegisterEffect(name, false);
                }
            }
            effectReference = CFxArchive_ReadShort(archive);
        }

        uint32_t scheduledEffectsRemaining =
            static_cast<uint32_t>(CFxArchive_ReadInt(archive));
        while (scheduledEffectsRemaining != 0U) {
            sfx_scheduled_effect_t *effect =
                new sfx_scheduled_effect_t;
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (effect == nullptr) {
                Com_Error(ERR_DROP, "\x15" "Loading FX system state: out of effects memory\n");
                return;
            }
            CFxArchive_ArchiveScheduledEffect(archive, effect);

            qboolean valid = qfalse;
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (effect->effectId > 0 &&
                effect->effectId < FX_EFFECT_TEMPLATE_COUNT) {
                sfx_effect_template_t *effectTemplate =
                    &scheduler->effectTemplates[effect->effectId];
                if (effectTemplate->active != 0 &&
                    effect->primitiveIndex >= 0 &&
                    effect->primitiveIndex < effectTemplate->primitiveCount &&
                    effectTemplate->primitives[effect->primitiveIndex] !=
                        nullptr) {
                    valid = qtrue;
                }
            }

            if (valid != qfalse) {
                scheduler->scheduledEffects.push_back(effect);
            } else {
                delete effect;
            }
            --scheduledEffectsRemaining;
        }
        return;
    }

    /* 0x004a9414 and 0x004a94dd hard-code the singleton map and list. Only
     * the serialized list count at 0x004a94be is loaded through scheduler. */
    for (const auto &registeredEffect : fxScheduler.effectIdsByName) {
        const char *name = registeredEffect.first.c_str();
        size_t nameLength = std::strlen(name);
        if (nameLength >= FX_SCHEDULER_ARCHIVE_NAME_CAPACITY) {
            Com_Error(ERR_DROP,
                      "\x15" "Found effect name > 255 characters while "
                      "saving effects state\n");
        }

        CFxArchive_WriteShort(
            archive, static_cast<int16_t>(registeredEffect.second));
        CFxArchive_WriteByte(archive, static_cast<uint8_t>(nameLength));
        if (nameLength != 0U) {
            CFxArchive_WriteData(archive, name,
                                 static_cast<int32_t>(nameLength));
        }
    }

    CFxArchive_WriteShort(archive, 0);
    CFxArchive_WriteInt(
        archive,
        static_cast<int32_t>(scheduler->scheduledEffects.size()));
    for (sfx_scheduled_effect_t *effect : fxScheduler.scheduledEffects) {
        CFxArchive_ArchiveScheduledEffect(archive, effect);
    }
}

/* Source: CoDUOMP.exe 0x004aab40..0x004aabc1.  This is the archive operation
 * on the timing prefix of the global SFxHelper object at 0x038b5010.  The two
 * otherwise-unused words are retained because they are part of the original
 * save format. */
void SFxHelper_ArchiveTiming(fx_archive_t *archive)
{
    CFxArchive_ArchiveInt(archive, &fxCurrentTime);
    CFxArchive_ArchiveInt(archive, &fxPreviousTime);
    CFxArchive_ArchiveInt(archive, &fxFrameTime);
    CFxArchive_ArchiveInt(archive, &fxArchivedTimingState);

    if (CFxArchive_IsLoading(archive)) {
        fxLastTime = 0;
    }
}

/* Source: CoDUOMP.exe 0x004afb60..0x004afc3c.  Exact original symbol spelling
 * is unavailable; the role name describes the complete FX-system save stream.
 * The Windows function receives capacity in ECX and buffer on the stack, but
 * that private i386 convention is represented as an ordinary portable API. */
extern "C" int32_t FX_Save(uint8_t *buffer, int32_t capacity)
{
    fx_archive_t archive;
    CFxArchive_InitWrite(&archive, buffer, capacity);

    SFxHelper_ArchiveTiming(&archive);
    CFxArchive_ArchiveScheduler(&archive, &fxScheduler);

    for (int32_t slotIndex = 0;
         slotIndex < FX_EFFECT_SLOT_COUNT;
         ++slotIndex) {
        fx_effect_slot_t *slot = &fxEffectSlots[slotIndex];
        if (slot->effect == nullptr) {
            continue;
        }

        uint8_t type = static_cast<uint8_t>(slot->effect->TypeID());
        CFxArchive_ArchiveData(&archive, &type, sizeof(type));
        CFxArchive_ArchiveInt(&archive, &slot->expirationTime);
        slot->effect->Archive(archive);
    }

    uint8_t terminator = FX_EFFECT_TYPE_BASE;
    CFxArchive_ArchiveData(&archive, &terminator, sizeof(terminator));
    return archive.cursor;
}

/* Source: CoDUOMP.exe 0x004afc40..0x004aff98.  The stream contains only
 * occupied slots: TypeID, expiration time, then the class virtual archive.
 * Type IDs absent from the Windows restore switch remain unsupported rather
 * than being inferred from the template parser's separate type numbering. */
extern "C" int32_t FX_Load(uint8_t *buffer, int32_t capacity)
{
    FX_Free(qfalse);

    fx_archive_t archive;
    CFxArchive_InitRead(&archive, buffer, capacity);
    fxEffectFreeList = nullptr;

    SFxHelper_ArchiveTiming(&archive);
    CFxArchive_ArchiveScheduler(&archive, &fxScheduler);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    for (int32_t slotIndex = 0;
         slotIndex < FX_EFFECT_SLOT_COUNT;
         ++slotIndex) {
        uint8_t type;
        CFxArchive_ArchiveData(&archive, &type, sizeof(type));
        if (type == FX_EFFECT_TYPE_BASE) {
            fxEffectFreeList = &fxEffectSlots[slotIndex];
            return archive.capacity;
        }

        fx_effect_slot_t *slot = &fxEffectSlots[slotIndex];
        CFxArchive_ArchiveInt(&archive, &slot->expirationTime);

        switch (type) {
        case FX_EFFECT_TYPE_PARTICLE:
            slot->effect = new CParticle;
            break;
        case FX_EFFECT_TYPE_LINE:
            slot->effect = new CLine;
            break;
        case FX_EFFECT_TYPE_TAIL:
            slot->effect = new CTail;
            break;
        case FX_EFFECT_TYPE_CYLINDER:
            slot->effect = new CCylinder;
            break;
        case FX_EFFECT_TYPE_EMITTER:
            slot->effect = new CEmitter;
            break;
        case FX_EFFECT_TYPE_ORIENTED_PARTICLE:
            slot->effect = new COrientedParticle;
            break;
        case FX_EFFECT_TYPE_ELECTRICITY:
            slot->effect = new CElectricity;
            break;
        case FX_EFFECT_TYPE_LIGHT:
            slot->effect = new CLight;
            break;
        case FX_EFFECT_TYPE_FLASH:
            slot->effect = new CFlash;
            break;
        case FX_EFFECT_TYPE_QUAD:
            slot->effect = new CQuad;
            break;
        case FX_EFFECT_TYPE_DECAL:
            slot->effect = new CDecal;
            break;
        default:
            slot->effect = nullptr;
            slot->expirationTime = 0;
            continue;
        }

        /* The original allocation-failure arm also dispatches through the
         * resulting null pointer.  Pool exhaustion is therefore fatal rather
         * than a silently skipped archive record. */
        slot->effect->Archive(archive);
    }

    return archive.capacity;
}
