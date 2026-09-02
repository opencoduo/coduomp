#ifndef SHARED_SCRIPT_STRING_H
#define SHARED_SCRIPT_STRING_H

#include "qcommon/script_runtime_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint16_t *script_stringCanonicalMap;
extern uint16_t script_stringCanonicalCount;
extern script_string_hash_slot_t script_stringHashSlots[SCRIPT_STRING_HASH_SLOT_COUNT];
extern script_string_hash_slot_t *script_stringFreedHashSlot;

uint16_t SL_ConvertFromString(const char *text);
const char *SL_ConvertToString(uint16_t string);
uint16_t GetHashCode(const char *text, size_t size);
uint16_t SL_FindStringOfLen(const char *text, size_t size);
uint16_t SL_FindString(const char *text);
uint16_t SL_FindLowercaseString(const char *text);
uint16_t SL_GetStringOfLen(const char *text, uint8_t user, size_t size, int32_t type);
#if defined(WINDOWS_BEHAVIOR)
uint16_t SL_GetString_(const char *text, int32_t user, int32_t type);
uint16_t SL_GetString(const char *text, int32_t user);
#else
uint16_t SL_GetString_(const char *text, uint8_t user, int32_t type);
uint16_t SL_GetString(const char *text, uint8_t user);
#endif
uint16_t SL_GetLowercaseStringOfLen(const char *text, uint8_t user, size_t size, int32_t type);
#if defined(WINDOWS_BEHAVIOR)
uint16_t SL_GetLowercaseString_(const char *text, int32_t user, int32_t type);
uint16_t SL_GetLowercaseString(const char *text, int32_t user);
#else
uint16_t SL_GetLowercaseString_(const char *text, uint8_t user, int32_t type);
uint16_t SL_GetLowercaseString(const char *text, uint8_t user);
#endif
void SL_Init(void);
void SL_AddRefToString(uint16_t string);
void SL_TransferRefToString(uint16_t string, uint8_t user);
void SL_RemoveRefToString(uint16_t string);
void SL_RemoveRefToStringOfLen(uint16_t string, uint32_t size);
void SL_FreeString(uint16_t string, const char *text, uint32_t size);
void Scr_SetString(uint16_t *slot, uint16_t value);
void SL_ShutdownSystem(uint8_t usage);
void CreateCanonicalFilename(char *dest, const char *source, int32_t maxLength);
uint16_t Scr_CreateCanonicalFilename(const char *filename);
uint16_t Scr_AllocString(const char *text);
uint16_t SL_GetStringForFloat(float value);
uint16_t SL_GetStringForInt(int32_t value);
uint16_t SL_GetStringForVector(const float *vector);
void SL_BeginLoadScripts(void);
void SL_EndLoadScripts(void);
uint16_t SL_TransferToCanonicalString(uint16_t string);
uint16_t SL_FindCanonicalString(const char *text);

/* NOT_FROM_ORIGINAL_SOURCE: source spelling for a usage-bit operation that is
 * inlined by each authoritative executable. */
void coduomp_script_string_mark_usage(uint16_t string, uint8_t usage);

#ifdef __cplusplus
}
#endif

#endif
