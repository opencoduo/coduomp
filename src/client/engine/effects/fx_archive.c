#include "fx_archive.h"
#include "fx_model.h"

#include "../scripting/script_runtime.h"

#include <string.h>

enum {
    FX_ARCHIVE_RUN_COUNT_MASK = 63,
    FX_ARCHIVE_RUN_TYPE_MASK = 192,
    FX_ARCHIVE_RUN_ZERO_AFTER_1_LITERAL = 0,
    FX_ARCHIVE_RUN_ZERO_AFTER_2_LITERALS = 64,
    FX_ARCHIVE_RUN_ZERO_AFTER_4_LITERALS = 128,
    FX_ARCHIVE_RUN_LITERAL = 192,
    FX_ARCHIVE_ASSET_NAME_CAPACITY = MAX_QPATH
};

_Static_assert(FX_ARCHIVE_ASSET_NAME_CAPACITY <= UINT8_MAX + 1,
               "FX archive asset names must fit the one-byte length field");

/* Source: CoDUOMP.exe 0x0049fcc0..0x0049fce5. */
void CFxArchive_Init(fx_archive_t *archive)
{
    archive->buffer = NULL;
    archive->capacity = 0;
    archive->cursor = 0;
    archive->loading = 0;
    archive->saving = 0;
    archive->uncompressedBytes = 0;
    archive->runControlOffset = 0;
    archive->literalBytesRemaining = 0;
    archive->zeroBytesRemaining = 0;
}

/* Source: CoDUOMP.exe 0x0049fcf0..0x0049fd22. */
void CFxArchive_InitRead(fx_archive_t *archive, uint8_t *buffer,
                         int32_t capacity)
{
    archive->buffer = buffer;
    archive->capacity = capacity;
    archive->cursor = 0;
    archive->loading = 1;
    archive->saving = 0;
    archive->uncompressedBytes = 0;
    archive->runControlOffset = 0;
    archive->literalBytesRemaining = 0;
    archive->zeroBytesRemaining = 0;
    memset(archive->references, 0, sizeof(archive->references));
}

/* Source: CoDUOMP.exe 0x0049fd30..0x0049fd56. */
void CFxArchive_InitWrite(fx_archive_t *archive, uint8_t *buffer,
                          int32_t capacity)
{
    archive->buffer = buffer;
    archive->capacity = capacity;
    archive->cursor = 0;
    archive->loading = 0;
    archive->saving = 1;
    archive->uncompressedBytes = 0;
    archive->runControlOffset = 0;
    archive->literalBytesRemaining = 0;
    archive->zeroBytesRemaining = 0;
}

/* Source: CoDUOMP.exe 0x0049fc40..0x0049fc55. */
uint8_t CFxArchive_ReadByte(fx_archive_t *archive)
{
    uint8_t value;
    CFxArchive_ReadData(archive, &value, sizeof(value));
    return value;
}

/* Source: CoDUOMP.exe 0x0049fc60..0x0049fc74. */
int16_t CFxArchive_ReadShort(fx_archive_t *archive)
{
    int16_t value;
    CFxArchive_ReadData(archive, &value, sizeof(value));
    return value;
}

/* Source: CoDUOMP.exe 0x0049fc80..0x0049fc90. */
void CFxArchive_WriteByte(fx_archive_t *archive, uint8_t value)
{
    CFxArchive_WriteData(archive, &value, sizeof(value));
}

/* Source: CoDUOMP.exe 0x0049fca0..0x0049fcb0. */
void CFxArchive_WriteShort(fx_archive_t *archive, int16_t value)
{
    CFxArchive_WriteData(archive, &value, sizeof(value));
}

/* Source: CoDUOMP.exe 0x0049fd60..0x0049fd64. */
void CFxArchive_SetModel(fx_archive_t *archive, int32_t index,
                         DObj *model)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if ((uint32_t)index >= (uint32_t)FX_ARCHIVE_MODEL_CAPACITY) {
        Com_Error(ERR_DROP, "\x15" "Loading FX system state: invalid model reference %i\n", index);
        return;
    }
    archive->references[index].model = model;
}

/* Source: CoDUOMP.exe 0x0049fd70..0x0049fd8e.
 * Name: same-module Mac symbol CFxArchive::ReadModel. */
DObj *CFxArchive_ReadModel(fx_archive_t *archive)
{
    int16_t index = CFxArchive_ReadShort(archive);
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if ((uint32_t)(int32_t)index >= (uint32_t)FX_ARCHIVE_MODEL_CAPACITY) {
        Com_Error(ERR_DROP, "\x15" "Loading FX system state: invalid model reference %i\n", index);
        return NULL;
    }
    return archive->references[index].model;
}

/* Source: CoDUOMP.exe 0x0049ffb0..0x0049ffc3.
 * Name: same-module Mac symbol CFxArchive::WriteModel. The Windows private
 * archive writes exactly the low 16 bits of its pointer-shaped model handle;
 * ReadModel sign-extends those bytes as an archive reference-table index.
 * This is an explicit platform-local serialization boundary, not permission
 * to narrow ordinary native pointers elsewhere. */
void CFxArchive_WriteModel(fx_archive_t *archive, const DObj *model)
{
    uint16_t encodedReference = (uint16_t)(uintptr_t)model;
    CFxArchive_WriteData(archive, &encodedReference,
                         sizeof(encodedReference));
}

/* Source: CoDUOMP.exe 0x0049fd90..0x0049fdf7.
 * Name: same-module Mac symbol CFxArchive::ReadShader. */
int32_t CFxArchive_ReadShader(fx_archive_t *archive)
{
    char name[FX_ARCHIVE_ASSET_NAME_CAPACITY];
    uint8_t length = CFxArchive_ReadByte(archive);
    if (length == 0) {
        return 0;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length >= sizeof(name)) {
        Com_Error(ERR_DROP, "\x15" "Loading FX system state: invalid shader name length %i\n", length);
        return 0;
    }

    CFxArchive_ReadData(archive, name, length);
    name[length] = '\0';
    return SFxHelper_RegisterShader(name);
}

/* Source: CoDUOMP.exe 0x0049fe00..0x0049fe66.
 * Name: same-module Mac symbol CFxArchive::ReadEffectID. */
DObj *CFxArchive_ReadEffectID(fx_archive_t *archive)
{
    char name[FX_ARCHIVE_ASSET_NAME_CAPACITY];
    uint8_t length = CFxArchive_ReadByte(archive);
    if (length == 0) {
        return NULL;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length >= sizeof(name)) {
        Com_Error(ERR_DROP, "\x15" "Loading FX system state: invalid effect name length %i\n", length);
        return NULL;
    }

    CFxArchive_ReadData(archive, name, length);
    name[length] = '\0';
    return CFxModel_Register(name);
}

/* Source: CoDUOMP.exe 0x0049ffd0..0x004a001e.
 * Name: same-module Mac symbol CFxArchive::WriteShader. */
void CFxArchive_WriteShader(fx_archive_t *archive, int32_t shader)
{
    const char *name = RE_GetShaderName(shader);
    if (name == NULL) {
        name = "";
    }

    const size_t length = strlen(name);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length >= FX_ARCHIVE_ASSET_NAME_CAPACITY) {
        Com_Error(ERR_DROP, "\x15" "Saving FX system state: shader name exceeds archive capacity\n");
        return;
    }
    CFxArchive_WriteByte(archive, (uint8_t)length);
    if (length != 0) {
        CFxArchive_WriteData(archive, name, (int32_t)length);
    }
}

/* Source: CoDUOMP.exe 0x004a0020..0x004a006e.
 * Name: same-module Mac symbol CFxArchive::WriteEffectID.  Registered effect
 * ids point at the effect payload immediately following its 64-byte name. */
void CFxArchive_WriteEffectID(fx_archive_t *archive,
                              const DObj *effectId)
{
    const char *name = "";
    if (effectId != NULL) {
        /* 0x004a002a..0x004a0036 tests the enclosing registration address
         * after subtracting the DObj member offset. Test the one address that
         * produces NULL before applying the valid container conversion. */
        if ((uintptr_t)(const void *)effectId !=
            offsetof(fx_model_registration_t, dobj)) {
            const fx_model_registration_t *registration =
                (const fx_model_registration_t *)(
                    (const uint8_t *)effectId -
                    offsetof(fx_model_registration_t, dobj));
            name = registration->name;
        }
    }

    const size_t length = strlen(name);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length >= FX_ARCHIVE_ASSET_NAME_CAPACITY) {
        Com_Error(ERR_DROP, "\x15" "Saving FX system state: effect name exceeds archive capacity\n");
        return;
    }
    CFxArchive_WriteByte(archive, (uint8_t)length);
    if (length != 0) {
        CFxArchive_WriteData(archive, name, (int32_t)length);
    }
}

/* Source: CoDUOMP.exe 0x004a0290..0x004a02a8.
 * Name: same-module Mac symbol CFxArchive::ArchiveData. */
void CFxArchive_ArchiveData(fx_archive_t *archive, void *data,
                            int32_t length)
{
    if (archive->loading != 0) {
        CFxArchive_ReadData(archive, data, length);
    } else {
        CFxArchive_WriteData(archive, data, length);
    }
}

/* Source: CoDUOMP.exe 0x004a02b0..0x004a02f2.
 * Name and source-level call shape: same-module Mac symbol
 * CFxArchive::ArchiveModel. MSVC inlined ReadModel/WriteModel into this body. */
void CFxArchive_ArchiveModel(fx_archive_t *archive, DObj **model)
{
    if (archive->loading != 0) {
        *model = CFxArchive_ReadModel(archive);
    } else {
        CFxArchive_WriteModel(archive, *model);
    }
}

/* Source: CoDUOMP.exe 0x004a0950..0x004a0953. */
uint8_t CFxArchive_IsLoading(const fx_archive_t *archive)
{
    return archive->loading;
}

/* Source: CoDUOMP.exe 0x004a0960..0x004a0973. */
int32_t CFxArchive_ReadInt(fx_archive_t *archive)
{
    int32_t value;
    CFxArchive_ReadData(archive, &value, sizeof(value));
    return value;
}

/* Source: CoDUOMP.exe 0x004a0980..0x004a0990. */
void CFxArchive_WriteInt(fx_archive_t *archive, int32_t value)
{
    CFxArchive_WriteData(archive, &value, sizeof(value));
}

/* Source: CoDUOMP.exe 0x004a09a0..0x004a09bd. */
void CFxArchive_ArchiveInt(fx_archive_t *archive, int32_t *value)
{
    CFxArchive_ArchiveData(archive, value, sizeof(*value));
}

/* Source: CoDUOMP.exe 0x004a09c0..0x004a09dd. */
void CFxArchive_ArchiveFloat(fx_archive_t *archive, float *value)
{
    CFxArchive_ArchiveData(archive, value, sizeof(*value));
}

/* Source: CoDUOMP.exe 0x004a09e0..0x004a09fd. */
void CFxArchive_ArchiveVec3(fx_archive_t *archive, vec3_t value)
{
    CFxArchive_ArchiveData(archive, value, sizeof(vec3_t));
}

/* Source: CoDUOMP.exe 0x004a0300..0x004a0315.
 * Name: same-module Mac symbol CFxArchive::ArchiveShader. */
void CFxArchive_ArchiveShader(fx_archive_t *archive, int32_t *shader)
{
    if (archive->loading != 0) {
        *shader = CFxArchive_ReadShader(archive);
    } else {
        CFxArchive_WriteShader(archive, *shader);
    }
}

/* Source: CoDUOMP.exe 0x004a0320..0x004a0335.
 * Name: same-module Mac symbol CFxArchive::ArchiveEffectID. */
void CFxArchive_ArchiveEffectID(fx_archive_t *archive,
                                DObj **effectId)
{
    if (archive->loading != 0) {
        *effectId = CFxArchive_ReadEffectID(archive);
    } else {
        CFxArchive_WriteEffectID(archive, *effectId);
    }
}

/* Source: CoDUOMP.exe 0x0049fe70..0x0049ffa1.
 * Name: same-module Mac symbol CFxArchive::ReadData.  Each control byte stores
 * a 1..64 count in its low six bits.  The high two bits select a literal run
 * or a zero run preceded by one, two, or four literal bytes. */
void CFxArchive_ReadData(fx_archive_t *archive, void *destination,
                         int32_t length)
{
    uint8_t *output = destination;
    int32_t remaining = length;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (length <= 0) {
        Com_Error(ERR_DROP, "\x15" "Loading FX system state: invalid read length %i\n", length);
        return;
    }

    archive->uncompressedBytes += length;
    for (;;) {
        while (archive->literalBytesRemaining != 0) {
            --archive->literalBytesRemaining;
            --remaining;
            if (archive->cursor >= archive->capacity) {
                Com_Error(ERR_DROP,
                          "\x15Loading FX system state: read past the end of the buffer\n");
            }
            *output++ = archive->buffer[archive->cursor];
            ++archive->cursor;
            if (remaining == 0) {
                return;
            }
        }

        while (archive->zeroBytesRemaining != 0 && remaining != 0) {
            --archive->zeroBytesRemaining;
            --remaining;
            *output++ = 0;
            if (remaining == 0) {
                return;
            }
        }

        if (archive->cursor >= archive->capacity) {
            Com_Error(ERR_DROP,
                      "\x15Loading FX system state: read past the end of the buffer\n");
        }
        uint8_t control = archive->buffer[archive->cursor];
        ++archive->cursor;
        int32_t count = (control & FX_ARCHIVE_RUN_COUNT_MASK) + 1;

        switch (control & FX_ARCHIVE_RUN_TYPE_MASK) {
        case FX_ARCHIVE_RUN_ZERO_AFTER_1_LITERAL:
            archive->literalBytesRemaining = 1;
            archive->zeroBytesRemaining = count;
            break;
        case FX_ARCHIVE_RUN_ZERO_AFTER_2_LITERALS:
            archive->literalBytesRemaining = 2;
            archive->zeroBytesRemaining = count;
            break;
        case FX_ARCHIVE_RUN_ZERO_AFTER_4_LITERALS:
            archive->literalBytesRemaining = 4;
            archive->zeroBytesRemaining = count;
            break;
        case FX_ARCHIVE_RUN_LITERAL:
            archive->literalBytesRemaining = count;
            archive->zeroBytesRemaining = 0;
            break;
        }
    }
}

/* Source: CoDUOMP.exe 0x004a0070..0x004a0283.
 * Name: same-module Mac symbol CFxArchive::WriteData.  This is the exact
 * streaming inverse of CFxArchive_ReadData.  It extends the current control
 * byte in place and, when a literal run ends in two zeros and receives a
 * third, rewrites that tail as a one-literal/three-zero run. */
void CFxArchive_WriteData(fx_archive_t *archive, const void *source,
                          int32_t length)
{
    const uint8_t *input = source;
    int32_t remaining = length;
    int32_t inputIndex = 0;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (length <= 0) {
        Com_Error(ERR_DROP, "\x15" "Saving FX system state: invalid write length %i\n", length);
        return;
    }

    archive->uncompressedBytes += length;

    if (archive->runControlOffset == archive->cursor) {
        if (archive->cursor < 0 || archive->capacity < archive->cursor ||
            archive->capacity - archive->cursor < 2) {
            Com_Error(ERR_DROP, "\x15" "Saving FX system state: out of memory (%i bytes exceeded on writing %i byte(s))\n", archive->capacity, length);
            return;
        }
        archive->buffer[archive->runControlOffset] = FX_ARCHIVE_RUN_LITERAL;
        archive->buffer[archive->cursor + 1] = *input++;
        archive->cursor += 2;
        --remaining;
    }

    if (archive->cursor < 0 || archive->capacity < archive->cursor ||
        remaining > archive->capacity - archive->cursor) {
        Com_Error(ERR_DROP,
                  "\x15Saving FX system state: out of memory (%i bytes exceeded on writing %i byte(s))\n",
                  archive->capacity, remaining);
        return;
    }

    while (inputIndex < remaining) {
        uint8_t *control = &archive->buffer[archive->runControlOffset];
        uint8_t type = *control & FX_ARCHIVE_RUN_TYPE_MASK;

        if (type != FX_ARCHIVE_RUN_LITERAL) {
            while (inputIndex < remaining && input[inputIndex] == 0 &&
                   (*control & FX_ARCHIVE_RUN_COUNT_MASK) !=
                       FX_ARCHIVE_RUN_COUNT_MASK) {
                ++*control;
                ++inputIndex;
            }
            if (inputIndex >= remaining) {
                return;
            }

            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (archive->capacity < archive->cursor ||
                archive->capacity - archive->cursor < 2) {
                Com_Error(ERR_DROP, "\x15" "Saving FX system state: out of memory (%i bytes exceeded on writing %i byte(s))\n", archive->capacity, remaining - inputIndex);
                return;
            }
            archive->runControlOffset = archive->cursor;
            archive->buffer[archive->cursor] = FX_ARCHIVE_RUN_LITERAL;
            archive->buffer[archive->cursor + 1] = input[inputIndex];
            archive->cursor += 2;
            ++inputIndex;
            continue;
        }

        uint8_t literalCountMinusOne =
            *control & FX_ARCHIVE_RUN_COUNT_MASK;
        if (literalCountMinusOne == 0 && input[inputIndex] == 0) {
            *control = FX_ARCHIVE_RUN_ZERO_AFTER_1_LITERAL;
            ++inputIndex;
            continue;
        }
        if (literalCountMinusOne == 1 && input[inputIndex] == 0) {
            *control = FX_ARCHIVE_RUN_ZERO_AFTER_2_LITERALS;
            ++inputIndex;
            continue;
        }
        if (literalCountMinusOne == 3 && input[inputIndex] == 0) {
            *control = FX_ARCHIVE_RUN_ZERO_AFTER_4_LITERALS;
            ++inputIndex;
            continue;
        }
        if (literalCountMinusOne == FX_ARCHIVE_RUN_COUNT_MASK) {
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (archive->capacity < archive->cursor ||
                archive->capacity - archive->cursor < 2) {
                Com_Error(ERR_DROP, "\x15" "Saving FX system state: out of memory (%i bytes exceeded on writing %i byte(s))\n", archive->capacity, remaining - inputIndex);
                return;
            }
            archive->runControlOffset = archive->cursor;
            archive->buffer[archive->cursor] = FX_ARCHIVE_RUN_LITERAL;
            archive->buffer[archive->cursor + 1] = input[inputIndex];
            archive->cursor += 2;
            ++inputIndex;
            continue;
        }

        if (literalCountMinusOne > 5 && input[inputIndex] == 0 &&
            archive->buffer[archive->cursor - 2] == 0 &&
            archive->buffer[archive->cursor - 1] == 0) {
            *control = (uint8_t)(*control - 3U);
            archive->buffer[archive->cursor - 2] =
                archive->buffer[archive->cursor - 3];
            archive->buffer[archive->cursor - 3] = 2;
            archive->runControlOffset = archive->cursor - 3;
            --archive->cursor;
            ++inputIndex;
            continue;
        }

        ++*control;
        archive->buffer[archive->cursor] = input[inputIndex++];
        ++archive->cursor;
    }
}
