#include "fx_scheduler.hpp"

#include "fx_primitive_template.hpp"
#include "../platform/crt_boundary.h"

#include <string.h>

enum {
    FX_EFFECT_FILE_DATA_CAPACITY = 65536,
    FX_EFFECT_FILE_MAX_LENGTH = FX_EFFECT_FILE_DATA_CAPACITY - 1,
    FX_EFFECT_FILE_EXTENSION_LENGTH = 4
};

/* Source: CoDUOMP.exe 0x004a6260..0x004a62e1.
 * Name: same-module Mac symbol CFxScheduler::CFxScheduler.  MSVC emits the map
 * and list sentinel construction explicitly, then clears all 512 templates. */
CFxScheduler::CFxScheduler()
    : effectTemplates{}, effectIdsByName{}, scheduledEffects{}
{
}

/* Source: CoDUOMP.exe 0x004a6110..0x004a6167.
 * Name: same-module Mac symbol CFxScheduler::~CFxScheduler.  Container
 * destructors release their nodes; scheduled-effect payload ownership is
 * handled by Clean during FX shutdown, as in the original. */
CFxScheduler::~CFxScheduler() = default;

/* Original object at 0x0389fff8.  Compiler startup thunk 0x005851d0 invokes
 * the constructor and registers the 0x00585350 destruction thunk. */
cfx_scheduler_t fxScheduler;

/* Original fixed-pool descriptor 0x0389fff0.  The compiler-generated startup
 * initializer at 0x005851a0 computes the usable 32-KiB block payload divided
 * by the scheduled-effect record size, then clears the block-list pointer.
 * PE_ZERO_WITH_RUNTIME_INITIALIZER */
fx_pool_allocator_t fxScheduledEffectAllocator = { /* original 0x0389fff0 */
    static_cast<int32_t>(
        sizeof(((fx_mem_block_t *)nullptr)->storage) /
        sizeof(sfx_scheduled_effect_t)),
    nullptr
};

/* Source: CoDUOMP.exe 0x004a60f0..0x004a60fb. The exact scheduled-record
 * type spelling is absent from the Mac traceback symbols; the body is the
 * class-specific allocation adapter for the fixed pool at 0x0389fff0. */
void *sfx_scheduled_effect_s::operator new(size_t size) noexcept
{
    return coduomp_fx_mem_alloc_from_pool(&fxScheduledEffectAllocator, size);
}

/* Source: CoDUOMP.exe 0x004a6100..0x004a6109. */
void sfx_scheduled_effect_s::operator delete(void *allocation) noexcept
{
    coduomp_fx_mem_free_from_pool(&fxScheduledEffectAllocator, allocation);
}

#if UINTPTR_MAX == UINT32_MAX
static_assert(sizeof(((fx_mem_block_t *)nullptr)->storage) /
                  sizeof(sfx_scheduled_effect_t) == 481,
              "i386 scheduled-effect pool capacity changed");
#endif

/* Source: CoDUOMP.exe 0x004a6b80..0x004a6c81.
 * Name and signature: same-module Mac symbol
 * CFxScheduler::GetNewEffectTemplate(int *, char const *).  Effect zero is
 * reserved; the first inactive slot from 1 through 511 is cleared, registered
 * by name when one is supplied, and marked active. */
sfx_effect_template_t *CFxScheduler::GetNewEffectTemplate(
    int32_t *effectId, const char *name)
{
    const size_t nameLength = name != nullptr ? strlen(name) : 0;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (name != nullptr && nameLength >= FX_EFFECT_TEMPLATE_NAME_CAPACITY) {
        SFxHelper_Print("^1FxScheduler: effect name is too long\n");
        *effectId = 0;
        return nullptr;
    }

    int32_t availableId;
    for (availableId = 1;
         availableId < FX_EFFECT_TEMPLATE_COUNT;
         ++availableId) {
        if (effectTemplates[availableId].active == 0) {
            break;
        }
    }

    if (availableId == FX_EFFECT_TEMPLATE_COUNT) {
        SFxHelper_Print(
            "^1FxScheduler:  Error--reached max effects\n");
        *effectId = 0;
        return nullptr;
    }

    *effectId = availableId;
    sfx_effect_template_t *effectTemplate =
        &effectTemplates[availableId];
    memset(effectTemplate, 0, sizeof(*effectTemplate));

    if (name != nullptr) {
        effectIdsByName[name] = availableId;
        memcpy(effectTemplate->name, name, nameLength + 1);
    }
    effectTemplate->active = 1;
    return effectTemplate;
}

/* Source: CoDUOMP.exe 0x004a6b50..0x004a6b75, also inlined into ParseEffect
 * at 0x004a6ae8..0x004a6b03.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004a6b50_004a6b76.mcode.
 * Name and signature: same-module Mac symbol
 * CFxScheduler::AddPrimitiveToEffect(SEffectTemplate *,
 * CPrimitiveTemplate *).  The original reports overflow but leaves the newly
 * allocated primitive unreclaimed. */
void CFxScheduler::AddPrimitiveToEffect(
    sfx_effect_template_t *effectTemplate,
    CPrimitiveTemplate *primitiveTemplate)
{
    if (effectTemplate->primitiveCount >=
        FX_EFFECT_TEMPLATE_PRIMITIVE_CAPACITY) {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        SFxHelper_Print(
            "^1FxScheduler:  Error--too many primitives in an effect\n");
        delete primitiveTemplate;
        return;
    }

    effectTemplate->primitives[effectTemplate->primitiveCount] =
        primitiveTemplate;
    ++effectTemplate->primitiveCount;
}

/* Source: CoDUOMP.exe 0x004a6900..0x004a6b3e.
 * Name and signature: same-module Mac symbol
 * CFxScheduler::ParseEffect(char const *, CGPGroup *). */
int32_t CFxScheduler::ParseEffect(const char *name, CGPGroup *parser)
{
    int32_t effectId;
    sfx_effect_template_t *effectTemplate =
        GetNewEffectTemplate(&effectId, name);
    if (effectId == 0 || effectTemplate == nullptr) {
        return 0;
    }

    qboolean parseFailed = qfalse;
    for (CGPGroup *group =
             static_cast<CGPGroup *>(parser->subGroups);
         group != nullptr;
         group = static_cast<CGPGroup *>(group->next)) {
        int32_t primitiveType = 0;
        if (coduo_crt_stricmp(group->name, "particle") == 0) {
            primitiveType = FX_PRIMITIVE_TYPE_PARTICLE;
        } else if (coduo_crt_stricmp(group->name, "line") == 0) {
            primitiveType = FX_PRIMITIVE_TYPE_LINE;
        } else if (coduo_crt_stricmp(group->name, "tail") == 0) {
            primitiveType = FX_PRIMITIVE_TYPE_TAIL;
        } else if (coduo_crt_stricmp(group->name, "sound") == 0) {
            primitiveType = FX_PRIMITIVE_TYPE_SOUND;
        } else if (coduo_crt_stricmp(group->name, "cylinder") == 0) {
            primitiveType = FX_PRIMITIVE_TYPE_CYLINDER;
        } else if (coduo_crt_stricmp(group->name, "electricity") == 0) {
            primitiveType = FX_PRIMITIVE_TYPE_ELECTRICITY;
        } else if (coduo_crt_stricmp(group->name, "emitter") == 0) {
            primitiveType = FX_PRIMITIVE_TYPE_EMITTER;
        } else if (coduo_crt_stricmp(group->name, "decal") == 0) {
            primitiveType = FX_PRIMITIVE_TYPE_DECAL;
        } else if (coduo_crt_stricmp(
                       group->name, "orientedparticle") == 0) {
            primitiveType = FX_PRIMITIVE_TYPE_ORIENTED_PARTICLE;
        } else if (coduo_crt_stricmp(group->name, "fxrunner") == 0) {
            primitiveType = FX_PRIMITIVE_TYPE_FX_RUNNER;
        } else if (coduo_crt_stricmp(group->name, "light") == 0) {
            primitiveType = FX_PRIMITIVE_TYPE_LIGHT;
        } else if (coduo_crt_stricmp(group->name, "cameraShake") == 0) {
            primitiveType = FX_PRIMITIVE_TYPE_CAMERA_SHAKE;
        } else if (coduo_crt_stricmp(group->name, "flash") == 0) {
            primitiveType = FX_PRIMITIVE_TYPE_FLASH;
        }

        if (primitiveType == 0) {
            continue;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        if (effectTemplate->primitiveCount >=
            FX_EFFECT_TEMPLATE_PRIMITIVE_CAPACITY) {
            SFxHelper_Print(
                "^1FxScheduler:  Error--too many primitives in an effect\n");
            parseFailed = qtrue;
            break;
        }

        CPrimitiveTemplate *primitiveTemplate = new CPrimitiveTemplate;
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        if (primitiveTemplate == nullptr) {
            SFxHelper_Print(
                "^1FxScheduler:  Error--could not allocate primitive template\n");
            parseFailed = qtrue;
            break;
        }
        primitiveTemplate->primitiveType = primitiveType;
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        if (primitiveTemplate->ParsePrimitive(group) == qfalse) {
            delete primitiveTemplate;
            parseFailed = qtrue;
            break;
        }
        AddPrimitiveToEffect(effectTemplate, primitiveTemplate);
    }

    if (parseFailed != qfalse) {
        for (int32_t primitiveIndex = 0;
             primitiveIndex < effectTemplate->primitiveCount;
             ++primitiveIndex) {
            delete effectTemplate->primitives[primitiveIndex];
        }
        effectTemplate->primitiveCount = 0;
        effectTemplate->active = 0;
        if (name != nullptr) {
            effectIdsByName.erase(name);
        }
        return 0;
    }
    return effectId;
}

/* Source: CoDUOMP.exe 0x004a65e0..0x004a68f3.
 * Name and signature: same-module Mac symbol
 * CFxScheduler::RegisterEffect(char const *, bool).  useDirectFileName selects
 * a caller-supplied file path; the normal path lowercases the registration key
 * and appends ".efx" before opening the file. */
int32_t CFxScheduler::RegisterEffect(const char *name,
                                     bool useDirectFileName)
{
    char effectName[FX_EFFECT_TEMPLATE_NAME_CAPACITY];
    const char *nameStart = name;
    if (useDirectFileName) {
        for (const char *cursor = name; *cursor != '\0'; ++cursor) {
            if (*cursor == '/' || *cursor == '\\') {
                nameStart = cursor + 1;
            }
        }
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    const size_t effectNameLength = strcspn(nameStart, ".");
    if (effectNameLength >= sizeof(effectName)) {
        SFxHelper_Print("^1Effect file load failed: path name is too long\n");
        return 0;
    }
    memcpy(effectName, nameStart, effectNameLength);
    effectName[effectNameLength] = '\0';
    if (!useDirectFileName) {
        coduo_crt_strlwr(effectName);
    }

    const auto existingEffect = effectIdsByName.find(effectName);
    if (existingEffect != effectIdsByName.end()) {
        return existingEffect->second;
    }

    CGenericParser2 parser;
    char synthesizedFileName[FX_EFFECT_TEMPLATE_NAME_CAPACITY];
    const char *fileName = name;
    if (!useDirectFileName) {
        if (strlen(effectName) + FX_EFFECT_FILE_EXTENSION_LENGTH >=
            sizeof(synthesizedFileName)) {
            SFxHelper_Print(
                "^1Effect file load failed: %s: path name is to long\n",
                effectName);
            return 0;
        }
        coduo_crt_snprintf(synthesizedFileName,
                             sizeof(synthesizedFileName), "%s.efx",
                             effectName);
        fileName = synthesizedFileName;
    }

    int32_t fileHandle = 0;
    const int32_t fileLength =
        SFxHelper_OpenFile(fileName, &fileHandle, 0);
    if (fileLength <= 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (fileHandle != 0) {
            SFxHelper_CloseFile(fileHandle);
        }
        SFxHelper_Print(
            "^1Effect file load failed: %s: file not found\n",
            fileName);
        return 0;
    }
    if (fileLength >= FX_EFFECT_FILE_MAX_LENGTH) {
        SFxHelper_Print(
            "^1Effect file load failed: %s: file too large\n",
            fileName);
        SFxHelper_CloseFile(fileHandle);
        return 0;
    }

    char fileData[FX_EFFECT_FILE_DATA_CAPACITY];
    (void)SFxHelper_ReadFile(fileData, fileLength, fileHandle);
    fileData[fileLength] = '\0';
    char *parseCursor = fileData;
    (void)parser.Parse(&parseCursor, true, false);
    SFxHelper_CloseFile(fileHandle);
    return ParseEffect(effectName, &parser);
}

/* Source: CoDUOMP.exe 0x004a6d30..0x004a6da5.
 * Exact source name is absent from the Mac build; the Windows register flow
 * proves a scheduler helper that validates a source effect ID, reserves a new
 * named template, deep-copies it, and returns the temporary copy. */
sfx_effect_template_t *CFxScheduler::CopyEffectTemplate(
    int32_t sourceEffectId, const char *copyName,
    int32_t *copyEffectId)
{
    if (sourceEffectId < 1 ||
        sourceEffectId >= FX_EFFECT_TEMPLATE_COUNT ||
        effectTemplates[sourceEffectId].active == 0) {
        SFxHelper_Print(
            "^1FxScheduler: Bad effect file copy request\n");
        *copyEffectId = 0;
        return nullptr;
    }

    sfx_effect_template_t *copy =
        GetNewEffectTemplate(copyEffectId, copyName);
    if (copy == nullptr || *copyEffectId == 0) {
        *copyEffectId = 0;
        return nullptr;
    }

    if (CFxScheduler_CopyEffectTemplate(
            &effectTemplates[sourceEffectId], copy) == qfalse) {
        copy->active = 0;
        copy->temporary = 0;
        if (copyName != nullptr) {
            effectIdsByName.erase(copyName);
        }
        *copyEffectId = 0;
        return nullptr;
    }
    copy->temporary = 1;
    return copy;
}

/* Source: CoDUOMP.exe 0x004a6c90..0x004a6d28.
 * Exact source name is absent from the Mac build.  This overload performs the
 * original map subscript lookup—therefore inserting a zero-valued entry for an
 * unknown source name—then delegates to the ID-based copy helper above. */
sfx_effect_template_t *CFxScheduler::CopyEffectTemplate(
    const char *sourceName, const char *copyName,
    int32_t *copyEffectId)
{
    const int32_t sourceEffectId = effectIdsByName[sourceName];
    return CopyEffectTemplate(sourceEffectId, copyName, copyEffectId);
}

/* Source: CoDUOMP.exe 0x004a6db0..0x004a6e03.
 * The same-module Mac build does not retain this helper, so its exact original
 * source name is unavailable. The Windows body proves a case-insensitive
 * search of the active effect template's primitive-name table. */
CPrimitiveTemplate *CFxScheduler::FindPrimitiveTemplate(
    const sfx_effect_template_t *effectTemplate,
    const char *primitiveName)
{
    if (effectTemplate == nullptr || effectTemplate->active == 0) {
        return nullptr;
    }

    for (int32_t primitiveIndex = 0;
         primitiveIndex < effectTemplate->primitiveCount;
         ++primitiveIndex) {
        CPrimitiveTemplate *primitive =
            effectTemplate->primitives[primitiveIndex];
        if (coduo_crt_stricmp(primitive->name, primitiveName) == 0) {
            return primitive;
        }
    }
    return nullptr;
}

/* Source: CoDUOMP.exe 0x004af490..0x004af496.
 * The Mac compiler inlined std::list::size; this role name preserves the
 * scheduler-level accessor represented by the Windows body. */
int32_t CFxScheduler::GetScheduledEffectCount() const
{
    return static_cast<int32_t>(scheduledEffects.size());
}

/* Source: CoDUOMP.exe 0x004a62f0..0x004a63b3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004a62f0_004a63b6.mcode.
 * The helper deep-copies an effect template into an already reserved template
 * slot, marking that slot temporary and allocating independent primitive
 * templates from the original fixed pool. */
qboolean CFxScheduler_CopyEffectTemplate(
    const sfx_effect_template_t *source,
    sfx_effect_template_t *destination)
{
    destination->temporary = true;
    strcpy(destination->name, source->name);
    destination->primitiveCount = 0;

    for (int32_t primitiveIndex = 0;
         primitiveIndex < source->primitiveCount;
         ++primitiveIndex) {
        CPrimitiveTemplate *primitiveCopy = new CPrimitiveTemplate;
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (primitiveCopy == nullptr) {
            SFxHelper_Print(
                "^1FxScheduler: could not allocate temporary primitive\n");
            for (int32_t copiedIndex = 0;
                 copiedIndex < destination->primitiveCount;
                 ++copiedIndex) {
                delete destination->primitives[copiedIndex];
                destination->primitives[copiedIndex] = nullptr;
            }
            destination->primitiveCount = 0;
            return qfalse;
        }
        destination->primitives[destination->primitiveCount] = primitiveCopy;
        primitiveCopy->CopyForTemporaryEffect(
            *source->primitives[primitiveIndex]);
        ++destination->primitiveCount;
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004a63c0..0x004a65a5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004a63c0_004a65a8.mcode.
 * Name and signature: same-module Mac symbol CFxScheduler::Clean(bool, int).
 * Scheduled records are always released.  When template cleanup is requested,
 * effect zero and preserveEffectId are retained; every other active template
 * is destroyed, and the name map is rebuilt with only the preserved entry. */
void CFxScheduler_Clean(cfx_scheduler_t *scheduler,
                        qboolean freeTemplates,
                        int32_t preserveEffectId)
{
    auto scheduledIt = scheduler->scheduledEffects.begin();
    while (scheduledIt != scheduler->scheduledEffects.end()) {
        sfx_scheduled_effect_t *scheduledEffect = *scheduledIt;
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        scheduler->effectTemplates[scheduledEffect->effectId]
            .primitives[scheduledEffect->primitiveIndex]
            ->coduomp_retire_pending_spawn();
        delete scheduledEffect;
        scheduledIt = scheduler->scheduledEffects.erase(scheduledIt);
    }

    if (freeTemplates == qfalse) {
        return;
    }

    for (int32_t effectId = 1;
         effectId < FX_EFFECT_TEMPLATE_COUNT;
         ++effectId) {
        if (effectId == preserveEffectId) {
            continue;
        }

        sfx_effect_template_t *effectTemplate =
            &scheduler->effectTemplates[effectId];
        if (effectTemplate->active != 0) {
            for (int32_t primitiveIndex = 0;
                 primitiveIndex < effectTemplate->primitiveCount;
                 ++primitiveIndex) {
                delete effectTemplate->primitives[primitiveIndex];
            }
        }
        effectTemplate->active = 0;
    }

    if (preserveEffectId == 0) {
        scheduler->effectIdsByName.clear();
        return;
    }

    std::string preservedName;
    for (const auto &entry : scheduler->effectIdsByName) {
        if (entry.second == preserveEffectId) {
            preservedName = entry.first;
            break;
        }
    }
    scheduler->effectIdsByName.clear();
    scheduler->effectIdsByName[preservedName] = preserveEffectId;
}

/* Source: CoDUOMP.exe 0x004a75f0..0x004a7654.  Windows-only scheduler member;
 * scheduled effects bolted to the freed entity are removed without touching
 * unrelated records, preserving list order. */
void CFxScheduler_FreeEntityEffects(cfx_scheduler_t *scheduler,
                                    int32_t entityNum)
{
    auto effectIt = scheduler->scheduledEffects.begin();
    while (effectIt != scheduler->scheduledEffects.end()) {
        sfx_scheduled_effect_t *effect = *effectIt;
        if (effect->boltInfo.entityNum != entityNum) {
            ++effectIt;
            continue;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        scheduler->effectTemplates[effect->effectId]
            .primitives[effect->primitiveIndex]
            ->coduomp_retire_pending_spawn();
        delete effect;
        effectIt = scheduler->scheduledEffects.erase(effectIt);
    }
}
