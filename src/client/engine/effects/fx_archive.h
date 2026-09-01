#ifndef CODUOMP_FX_ARCHIVE_H
#define CODUOMP_FX_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

#include "qcommon/q_shared_types.h"

#include "../animation/dobj.h"

enum {
    FX_ARCHIVE_MODEL_CAPACITY = 1800
};

/* Original CFxModel registration node. At the retail MAX_QPATH of 64,
 * Register allocates 0xa0 bytes, keeps the name at +0x00, constructs the
 * complete 0x5c-byte DObj at +0x40, and links through +0x9c. Deriving the name
 * extent from MAX_QPATH preserves the renderer-registration safety invariant
 * when a modified client raises that global path limit. */
typedef struct fx_model_registration_s {
    char name[MAX_QPATH];
    DObj dobj;
    struct fx_model_registration_s *next;
} fx_model_registration_t;

/* CFxArchive uses the same 1,800-entry scratch table for two different
 * load-time reference domains.  Model archiving stores native DObj pointers;
 * scheduler archiving stores the int32 effect id returned by RegisterEffect.
 * They are both one dword in the original i386 object, but they must not be
 * represented by narrowing native pointers on a 64-bit host. */
typedef union fx_archive_reference_u {
    DObj *model;
    int32_t effectId;
} fx_archive_reference_t;

/* Original CFxArchive object.  The two bytes after saving are ordinary
 * compiler padding required to align uncompressedBytes at +0x10; no Windows
 * CoDUOMP archive path reads or writes those bytes. */
typedef struct fx_archive_s {
    uint8_t *buffer;
    int32_t capacity;
    int32_t cursor;
    uint8_t loading;
    uint8_t saving; /* initialized but otherwise unused by CoDUOMP.exe. */
    int32_t uncompressedBytes;
    fx_archive_reference_t references[FX_ARCHIVE_MODEL_CAPACITY];
    int32_t literalBytesRemaining;
    int32_t zeroBytesRemaining;
    int32_t runControlOffset;
} fx_archive_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(fx_model_registration_t) == 0x04,
               "i386 CFxModel registration alignment changed");
_Static_assert(offsetof(fx_model_registration_t, name) == 0x00,
               "i386 CFxModel name offset changed");
_Static_assert(sizeof(((fx_model_registration_t *)0)->name) == MAX_QPATH,
               "i386 CFxModel name extent changed");
_Static_assert(MAX_QPATH != 64 ||
                   offsetof(fx_model_registration_t, dobj) == 0x40,
               "i386 CFxModel DObj offset changed");
_Static_assert(sizeof(((fx_model_registration_t *)0)->dobj) == 0x5c,
               "i386 CFxModel DObj extent changed");
_Static_assert(MAX_QPATH != 64 ||
                   offsetof(fx_model_registration_t, next) == 0x9c,
               "i386 CFxModel list-link offset changed");
_Static_assert(sizeof(((fx_model_registration_t *)0)->next) == 0x04,
               "i386 CFxModel list-link extent changed");
_Static_assert(MAX_QPATH != 64 ||
                   sizeof(fx_model_registration_t) == 0xa0,
               "i386 CFxModel registration size changed");
_Static_assert(_Alignof(fx_archive_reference_t) == 0x04,
               "i386 CFxArchive reference alignment changed");
_Static_assert(offsetof(fx_archive_reference_t, model) == 0x00,
               "i386 CFxArchive model-reference offset changed");
_Static_assert(sizeof(((fx_archive_reference_t *)0)->model) == 0x04,
               "i386 CFxArchive model-reference extent changed");
_Static_assert(offsetof(fx_archive_reference_t, effectId) == 0x00,
               "i386 CFxArchive effect-reference offset changed");
_Static_assert(sizeof(((fx_archive_reference_t *)0)->effectId) == 0x04,
               "i386 CFxArchive effect-reference extent changed");
_Static_assert(sizeof(fx_archive_reference_t) == 0x04,
               "i386 CFxArchive reference size changed");
_Static_assert(_Alignof(fx_archive_t) == 0x04,
               "i386 CFxArchive alignment changed");
_Static_assert(offsetof(fx_archive_t, buffer) == 0x00,
               "i386 CFxArchive buffer offset changed");
_Static_assert(sizeof(((fx_archive_t *)0)->buffer) == 0x04,
               "i386 CFxArchive buffer extent changed");
_Static_assert(offsetof(fx_archive_t, capacity) == 0x04,
               "i386 CFxArchive capacity offset changed");
_Static_assert(sizeof(((fx_archive_t *)0)->capacity) == 0x04,
               "i386 CFxArchive capacity extent changed");
_Static_assert(offsetof(fx_archive_t, cursor) == 0x08,
               "i386 CFxArchive cursor offset changed");
_Static_assert(sizeof(((fx_archive_t *)0)->cursor) == 0x04,
               "i386 CFxArchive cursor extent changed");
_Static_assert(offsetof(fx_archive_t, loading) == 0x0c,
               "i386 CFxArchive loading flag offset changed");
_Static_assert(sizeof(((fx_archive_t *)0)->loading) == 0x01,
               "i386 CFxArchive loading flag extent changed");
_Static_assert(offsetof(fx_archive_t, saving) == 0x0d,
               "i386 CFxArchive saving flag offset changed");
_Static_assert(sizeof(((fx_archive_t *)0)->saving) == 0x01,
               "i386 CFxArchive saving flag extent changed");
_Static_assert(offsetof(fx_archive_t, uncompressedBytes) == 0x10,
               "i386 CFxArchive uncompressed-byte count offset changed");
_Static_assert(sizeof(((fx_archive_t *)0)->uncompressedBytes) == 0x04,
               "i386 CFxArchive uncompressed-byte count extent changed");
_Static_assert(offsetof(fx_archive_t, references) == 0x14,
               "i386 CFxArchive reference table offset changed");
_Static_assert(sizeof(((fx_archive_t *)0)->references) == 0x1c20,
               "i386 CFxArchive reference table extent changed");
_Static_assert(offsetof(fx_archive_t, literalBytesRemaining) == 0x1c34,
               "i386 CFxArchive literal-run offset changed");
_Static_assert(sizeof(((fx_archive_t *)0)->literalBytesRemaining) == 0x04,
               "i386 CFxArchive literal-run extent changed");
_Static_assert(offsetof(fx_archive_t, zeroBytesRemaining) == 0x1c38,
               "i386 CFxArchive zero-run offset changed");
_Static_assert(sizeof(((fx_archive_t *)0)->zeroBytesRemaining) == 0x04,
               "i386 CFxArchive zero-run extent changed");
_Static_assert(offsetof(fx_archive_t, runControlOffset) == 0x1c3c,
               "i386 CFxArchive control offset changed");
_Static_assert(sizeof(((fx_archive_t *)0)->runControlOffset) == 0x04,
               "i386 CFxArchive control extent changed");
_Static_assert(sizeof(fx_archive_t) == 0x1c40,
               "i386 CFxArchive size changed");
#endif

#ifdef __cplusplus
extern "C" {
#endif

void CFxArchive_Init(fx_archive_t *archive);
void CFxArchive_InitWrite(fx_archive_t *archive, uint8_t *buffer,
                          int32_t capacity);
void CFxArchive_InitRead(fx_archive_t *archive, uint8_t *buffer,
                         int32_t capacity);
uint8_t CFxArchive_ReadByte(fx_archive_t *archive);
int16_t CFxArchive_ReadShort(fx_archive_t *archive);
void CFxArchive_WriteByte(fx_archive_t *archive, uint8_t value);
void CFxArchive_WriteShort(fx_archive_t *archive, int16_t value);
void CFxArchive_ReadData(fx_archive_t *archive, void *destination,
                         int32_t length);
void CFxArchive_WriteData(fx_archive_t *archive, const void *source,
                          int32_t length);
void CFxArchive_ArchiveData(fx_archive_t *archive, void *data,
                            int32_t length);
uint8_t CFxArchive_IsLoading(const fx_archive_t *archive);
int32_t CFxArchive_ReadInt(fx_archive_t *archive);
void CFxArchive_WriteInt(fx_archive_t *archive, int32_t value);
void CFxArchive_ArchiveInt(fx_archive_t *archive, int32_t *value);
void CFxArchive_ArchiveFloat(fx_archive_t *archive, float *value);
void CFxArchive_ArchiveVec3(fx_archive_t *archive, vec3_t value);
void CFxArchive_SetModel(fx_archive_t *archive, int32_t index,
                         DObj *model);
void CFxArchive_ArchiveModel(fx_archive_t *archive, DObj **model);
DObj *CFxArchive_ReadModel(fx_archive_t *archive);
int32_t CFxArchive_ReadShader(fx_archive_t *archive);
DObj *CFxArchive_ReadEffectID(fx_archive_t *archive);
void CFxArchive_WriteModel(fx_archive_t *archive, const DObj *model);
void CFxArchive_WriteShader(fx_archive_t *archive, int32_t shader);
void CFxArchive_WriteEffectID(fx_archive_t *archive,
                              const DObj *effectId);
void CFxArchive_ArchiveShader(fx_archive_t *archive, int32_t *shader);
void CFxArchive_ArchiveEffectID(fx_archive_t *archive,
                                DObj **effectId);
const char *RE_GetShaderName(int32_t shader);
int32_t SFxHelper_RegisterShader(const char *name);

#ifdef __cplusplus
}
#endif

#endif
