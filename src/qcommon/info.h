#ifndef QCOMMON_INFO_H
#define QCOMMON_INFO_H

#include "q_shared_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum parseFieldType_e {
    PARSE_FIELD_STRING_ALLOC = 0,
    PARSE_FIELD_STRING = 1,       /* char[MAX_STRING_CHARS] */
    PARSE_FIELD_QPATH = 2,        /* char[MAX_QPATH] */
    PARSE_FIELD_CVAR_VALUE = 3,   /* char[MAX_CVAR_VALUE_STRING] */
    PARSE_FIELD_INT = 4,
    PARSE_FIELD_BOOL = 5,
    PARSE_FIELD_FLOAT = 6,
    PARSE_FIELD_MILLISECONDS = 7,
    PARSE_FIELD_CUSTOM_FIRST = 8
} parseFieldType_t;

/* The original i386 descriptor is three dwords: key pointer, byte offset,
 * and parse type.  The pointer widens naturally on 64-bit hosts; the stored
 * record offset remains the original signed 32-bit domain. */
typedef struct parseField_s {
    const char *key;
    int32_t offset;
    int32_t type;
} parseField_t;

typedef qboolean (*parse_config_custom_t)(void *base, const char *value,
                                          int32_t fieldType);
typedef void (*parse_config_copy_string_t)(char *destination,
                                           const char *value);

const char *Info_ValueForKey(const char *info, const char *key);
void Info_NextPair(const char **head, char *key, char *value);
void Info_RemoveKey(char *info, const char *key);
void Info_RemoveKey_Big(char *info, const char *key);
qboolean Info_Validate(const char *info);
void Info_SetValueForKey(char *info, const char *key, const char *value);
void Info_SetValueForKey_Big(char *info, const char *key, const char *value);
qboolean ParseConfigStringToStruct(
    void *base, const parseField_t *fields, int32_t fieldCount,
    const char *info, int32_t customTypeLimit,
    parse_config_custom_t customParser,
    parse_config_copy_string_t stringSetter);
void Info_Print(const char *info);

#ifdef __cplusplus
}
#endif

#endif
