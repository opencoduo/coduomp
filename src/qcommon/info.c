#include "info.h"
#include "info_private.h"
#include "compat/coduo_fp_conversion.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    MAX_INFO_STRING = 1024,
    BIG_INFO_STRING = 8192,
    INFO_VALUE_COMPARE_LIMIT = 99999,
    INFO_ERROR_DROP = 1,
    INFO_PRINT_FIELD_WIDTH = 20,
    INFO_PRINT_BUFFER_CAPACITY = MAX_INFO_STRING,
    MAX_INFO_STRING_LAST_INDEX = MAX_INFO_STRING - 1,
    BIG_INFO_STRING_LAST_INDEX = BIG_INFO_STRING - 1
};

/*
 * Complete shared info-string subsystem. Direct objdump comparison proves an
 * instruction-identical Windows CoDUOMP/cgame/UI/game cluster for
 * Info_NextPair, Info_Validate, and both Info_SetValueForKey variants. The
 * parsing and storage graphs of the remaining shared functions also agree,
 * but the Windows game module routes their fatal errors directly through
 * G_Error while the other authoritative bodies call Com_Error. The
 * component-owned INFO_ERROR binding preserves that original dependency edge.
 * Linux Q_strncpyz is exactly strncpy(size - 1) followed by the final NUL store
 * written here; its helper call is code-generation structure, not different
 * behavior. The supporting Mac client symbols confirm the Info_* and
 * Com_sprintf names. ParseConfigStringToStruct below separately preserves its
 * proved platform conversion difference.
 */

/* Source: CoDUOMP.exe 0x009b8430..0x009bc42f.
 * Source: uo_cgame_mp_x86.dll 0x300dce58..0x300e0e57.
 * Source: uo_ui_mp_x86.dll 0x40041c48..0x40045c47.
 * Source: uo_game_mp_x86.dll 0x200fb0f8..0x200ff0f7.
 * Source: coduo_lnxded 0x0829ad80..0x0829ed7f.
 * Source: game.mp.uo.i386.so 0x00103640..0x0010763f. */
static char infoValueBuffers[2][BIG_INFO_STRING];

/* Source: CoDUOMP.exe 0x0389fd6c.
 * Source: uo_cgame_mp_x86.dll 0x30134d1c.
 * Source: uo_ui_mp_x86.dll 0x401c469c.
 * Source: uo_game_mp_x86.dll 0x2010ed4c.
 * Source: coduo_lnxded 0x080f4f68.
 * Source: game.mp.uo.i386.so 0x000b3384. */
static int32_t infoValueBufferIndex;

/* Source: CoDUOMP.exe 0x0044fb70..0x0044fc81.
 * Source: uo_cgame_mp_x86.dll 0x3004e960..0x3004ea72.
 * Source: uo_ui_mp_x86.dll 0x40006990..0x40006aa2.
 * Source: uo_game_mp_x86.dll 0x20058180..0x20058291.
 * Source: coduo_lnxded 0x08086e7f..0x08086fd2.
 * Source: game.mp.uo.i386.so 0x00093a32..0x00093ba9. */
const char *Info_ValueForKey(const char *info, const char *key)
{
    char keyBuffer[BIG_INFO_STRING];
    char *value;

    if (info == NULL || key == NULL) {
        return "";
    }

    {
        const char *end = info;
        while (*end != '\0') {
            end++;
        }
        if ((size_t)(end - info) >= BIG_INFO_STRING) {
            INFO_ERROR(INFO_ERROR_DROP, "\x15"
                                        "Info_ValueForKey: oversize infostring");
        }
    }

    infoValueBufferIndex ^= 1;
    value = infoValueBuffers[infoValueBufferIndex];
    if (*info == '\\') {
        info++;
    }

    for (;;) {
        char *keyCursor = keyBuffer;
        char *valueCursor;

        while (*info != '\\') {
            if (*info == '\0') {
                return "";
            }
            *keyCursor++ = *info++;
        }
        info++;
        *keyCursor = '\0';

        valueCursor = value;
        while (*info != '\\' && *info != '\0') {
            *valueCursor++ = *info++;
        }
        *valueCursor = '\0';

        if (Q_stricmpn(keyBuffer, key, INFO_VALUE_COMPARE_LIMIT) == 0) {
            return value;
        }
        if (*info == '\0') {
            return "";
        }
        info++;
    }
}

/* Source: CoDUOMP.exe 0x0044fc90..0x0044fcd7.
 * Source: uo_cgame_mp_x86.dll 0x3004ea80..0x3004eac8.
 * Source: uo_ui_mp_x86.dll 0x40006ab0..0x40006af8.
 * Source: uo_game_mp_x86.dll 0x200582a0..0x200582e7.
 * Source: coduo_lnxded 0x08086fd3..0x08087086.
 * Source: game.mp.uo.i386.so 0x00093baa..0x00093c5d. */
void Info_NextPair(const char **head, char *key, char *value)
{
    const char *cursor = *head;
    char *output = key;

    /* The original interface carries no destination capacities. Its retained
     * caller limits the complete source to BIG_INFO_STRING and supplies two
     * buffers of that same capacity; any future caller must preserve that
     * source/output relationship. */
    if (*cursor == '\\') {
        cursor++;
    }
    *key = '\0';
    *value = '\0';

    while (*cursor != '\\' && *cursor != '\0') {
        *output++ = *cursor++;
    }
    if (*cursor == '\\') {
        cursor++;
        *output = '\0';
        output = value;
        while (*cursor != '\\' && *cursor != '\0') {
            *output++ = *cursor++;
        }
    }
    *output = '\0';
    *head = cursor;
}

/* Source: CoDUOMP.exe 0x0044fce0..0x0044fdff.
 * Source: uo_cgame_mp_x86.dll 0x3004ead0..0x3004ebf0.
 * Source: uo_ui_mp_x86.dll 0x40006b00..0x40006c20.
 * Source: uo_game_mp_x86.dll 0x200582f0..0x2005840f.
 * Source: coduo_lnxded 0x08087087..0x080871b8.
 * Source: game.mp.uo.i386.so 0x00093c5e..0x00093da4. */
void Info_RemoveKey(char *info, const char *key)
{
    char parsedKey[MAX_INFO_STRING];
    char parsedValue[MAX_INFO_STRING];
    char *cursor = info;

    if (strlen(info) >= MAX_INFO_STRING) {
        INFO_ERROR(INFO_ERROR_DROP, "\x15"
                                    "Info_RemoveKey: oversize infostring");
    }
    if (strchr(key, '\\') != NULL) {
        return;
    }

    for (;;) {
        char *pairStart = cursor;
        char *output;

        if (*cursor == '\\') {
            cursor++;
        }
        output = parsedKey;
        while (*cursor != '\\') {
            if (*cursor == '\0') {
                return;
            }
            *output++ = *cursor++;
        }
        *output = '\0';

        cursor++;
        output = parsedValue;
        while (*cursor != '\\' && *cursor != '\0') {
            *output++ = *cursor++;
        }
        *output = '\0';

        if (strcmp(key, parsedKey) == 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            memmove(pairStart, cursor, strlen(cursor) + 1);
            return;
        }
        if (*cursor == '\0') {
            return;
        }
    }
}

/* Source: CoDUOMP.exe 0x0044fe00..0x0044ff1f.
 * Source: uo_cgame_mp_x86.dll 0x3004ebf0..0x3004ed10.
 * Source: uo_ui_mp_x86.dll 0x40006c20..0x40006d40.
 * Source: uo_game_mp_x86.dll 0x20058410..0x2005852f.
 * Source: coduo_lnxded 0x080871b9..0x080872ea.
 * Source: game.mp.uo.i386.so 0x00093da5..0x00093eeb. */
void Info_RemoveKey_Big(char *info, const char *key)
{
    char parsedKey[BIG_INFO_STRING];
    char parsedValue[BIG_INFO_STRING];
    char *cursor = info;

    if (strlen(info) >= BIG_INFO_STRING) {
        INFO_ERROR(INFO_ERROR_DROP, "\x15"
                                    "Info_RemoveKey_Big: oversize infostring");
    }
    if (strchr(key, '\\') != NULL) {
        return;
    }

    for (;;) {
        char *pairStart = cursor;
        char *output;

        if (*cursor == '\\') {
            cursor++;
        }
        output = parsedKey;
        while (*cursor != '\\') {
            if (*cursor == '\0') {
                return;
            }
            *output++ = *cursor++;
        }
        *output = '\0';

        cursor++;
        output = parsedValue;
        while (*cursor != '\\' && *cursor != '\0') {
            *output++ = *cursor++;
        }
        *output = '\0';

        if (strcmp(key, parsedKey) == 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            memmove(pairStart, cursor, strlen(cursor) + 1);
            return;
        }
        if (*cursor == '\0') {
            return;
        }
    }
}

/* Source: CoDUOMP.exe 0x0044ff20..0x0044ff42.
 * Source: uo_cgame_mp_x86.dll 0x3004ed10..0x3004ed33.
 * Source: uo_ui_mp_x86.dll 0x40006d40..0x40006d63.
 * Source: uo_game_mp_x86.dll 0x20058530..0x20058552.
 * Source: coduo_lnxded 0x080872eb..0x0808733c.
 * Source: game.mp.uo.i386.so 0x00093eec..0x00093f4d.
 * Name: exact same-module Mac cgame symbol Info_Validate. */
qboolean Info_Validate(const char *info)
{
    if (strchr(info, '"') != NULL) {
        return 0;
    }
    return strchr(info, ';') == NULL;
}

/* Source: CoDUOMP.exe 0x0044ff50..0x0045017e.
 * Source: uo_cgame_mp_x86.dll 0x3004ed40..0x3004ef6f.
 * Source: uo_ui_mp_x86.dll 0x40006d70..0x40006f9f.
 * Source: uo_game_mp_x86.dll 0x20058560..0x2005878e.
 * Source: coduo_lnxded 0x0808733d..0x0808756f.
 * Source: game.mp.uo.i386.so 0x00093f4e..0x0009419b. */
void Info_SetValueForKey(char *info, const char *key, const char *value)
{
    char cleaned[MAX_INFO_STRING];
    char pair[MAX_INFO_STRING];
    int32_t readIndex = 0;
    int32_t writeIndex = 0;

    if (strlen(info) >= MAX_INFO_STRING) {
        Com_Printf("\x15"
                   "Info_SetValueForKey: oversize infostring");
        return;
    }
    while (readIndex < MAX_INFO_STRING_LAST_INDEX && value[readIndex] != '\0') {
        char character = value[readIndex];
        if (character != '\\' && character != ';' && character != '"') {
            cleaned[writeIndex++] = character;
        }
        readIndex++;
    }
    cleaned[writeIndex] = '\0';

    if (strchr(key, '\\') != NULL) {
        Com_Printf("\x15"
                   "Can't use keys with a \\\nkey: '%s'\nvalue: '%s'",
                   key, value);
        return;
    }
    if (strchr(key, ';') != NULL) {
        Com_Printf("\x15"
                   "Can't use keys with a semicolon\nkey: '%s'\n"
                   "value: '%s'",
                   key, value);
        return;
    }
    if (strchr(key, '"') != NULL) {
        Com_Printf("\x15"
                   "Can't use keys with a \"\nkey: '%s'\nvalue: '%s'",
                   key, value);
        return;
    }

    Info_RemoveKey(info, key);
    if (cleaned[0] == '\0') {
        return;
    }
    if (Com_sprintf(pair, sizeof(pair), "\\%s\\%s", key, cleaned) <= 0) {
        Com_Printf("\x15"
                   "Server info buffer length exceeded, not including "
                   "key/value pair in response\n");
        return;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: the combined info pair must leave room for
     * strcat's trailing NUL. */
    if ((uint32_t)strlen(info) + (uint32_t)strlen(pair) >= MAX_INFO_STRING) {
        Com_Printf("\x15"
                   "Info string length exceeded\nkey: '%s'\n"
                   "value: '%s'\nInfo string:\n%s\n",
                   key, value, info);
        return;
    }
    strcat(info, pair);
}

/* Source: CoDUOMP.exe 0x00450180..0x004503ae.
 * Source: uo_cgame_mp_x86.dll 0x3004ef70..0x3004f19f.
 * Source: uo_ui_mp_x86.dll 0x40006fa0..0x400071cf.
 * Source: uo_game_mp_x86.dll 0x20058790..0x200589be.
 * Source: coduo_lnxded 0x08087570..0x080877a2.
 * Source: game.mp.uo.i386.so 0x0009419c..0x000943e9. */
void Info_SetValueForKey_Big(char *info, const char *key, const char *value)
{
    char cleaned[BIG_INFO_STRING];
    char pair[BIG_INFO_STRING];
    int32_t readIndex = 0;
    int32_t writeIndex = 0;

    if (strlen(info) >= BIG_INFO_STRING) {
        Com_Printf("\x15"
                   "Info_SetValueForKey: oversize infostring");
        return;
    }
    while (readIndex < BIG_INFO_STRING_LAST_INDEX && value[readIndex] != '\0') {
        char character = value[readIndex];
        if (character != '\\' && character != ';' && character != '"') {
            cleaned[writeIndex++] = character;
        }
        readIndex++;
    }
    cleaned[writeIndex] = '\0';

    if (strchr(key, '\\') != NULL) {
        Com_Printf("\x15"
                   "Can't use keys with a \\\nkey: '%s'\nvalue: '%s'",
                   key, value);
        return;
    }
    if (strchr(key, ';') != NULL) {
        Com_Printf("\x15"
                   "Can't use keys with a semicolon\nkey: '%s'\n"
                   "value: '%s'",
                   key, value);
        return;
    }
    if (strchr(key, '"') != NULL) {
        Com_Printf("\x15"
                   "Can't use keys with a \"\nkey: '%s'\nvalue: '%s'",
                   key, value);
        return;
    }

    Info_RemoveKey_Big(info, key);
    if (cleaned[0] == '\0') {
        return;
    }
    if (Com_sprintf(pair, sizeof(pair), "\\%s\\%s", key, cleaned) <= 0) {
        Com_Printf("\x15"
                   "Server info buffer length exceeded, not including "
                   "key/value pair in response\n");
        return;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: the combined large info pair must leave room
     * for strcat's trailing NUL. */
    if ((uint32_t)strlen(info) + (uint32_t)strlen(pair) >= BIG_INFO_STRING) {
        Com_Printf("\x15"
                   "BIG Info string length exceeded\nkey: '%s'\n"
                   "value: '%s'\nInfo string:\n%s\n",
                   key, value, info);
        return;
    }
    strcat(info, pair);
}

/* Source: CoDUOMP.exe 0x004503b0..0x00450508.
 * Source: uo_cgame_mp_x86.dll 0x3004f1a0..0x3004f2f8.
 * Source: uo_ui_mp_x86.dll 0x400071d0..0x40007329.
 * Source: uo_game_mp_x86.dll 0x200589c0..0x20058b1c.
 * Source: coduo_lnxded 0x080877a3..0x080879a9.
 * Source: game.mp.uo.i386.so 0x000943ea..0x00094601.
 * Name: same-family Mac client/game symbol ParseConfigStringToStruct. */
qboolean ParseConfigStringToStruct(void *base, const parseField_t *fields, int32_t fieldCount, const char *info, int32_t customTypeLimit,
                                   parse_config_custom_t customParser, parse_config_copy_string_t stringSetter)
{
    int32_t fieldIndex;

    for (fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex) {
        const parseField_t *field = &fields[fieldIndex];
        const char *value = Info_ValueForKey(info, field->key);
        char *destination;

        if (*value == '\0') {
            continue;
        }
        if (field->type >= PARSE_FIELD_CUSTOM_FIRST) {
            if (customTypeLimit <= 0 || field->type >= customTypeLimit) {
                INFO_ERROR(INFO_ERROR_DROP,
                           "\x15"
                           "Bad field type %i\n",
                           field->type);
            } else if (customParser(base, value, field->type) == 0) {
                return 0;
            }
            continue;
        }
        if (field->type < PARSE_FIELD_STRING_ALLOC) {
            continue;
        }

        destination = (char *)base + field->offset;
        switch (field->type) {
        case PARSE_FIELD_STRING_ALLOC:
            stringSetter(destination, value);
            break;
        case PARSE_FIELD_STRING:
            strncpy(destination, value, MAX_STRING_CHARS - 1);
            destination[MAX_STRING_CHARS - 1] = '\0';
            break;
        case PARSE_FIELD_QPATH:
            strncpy(destination, value, MAX_QPATH - 1);
            destination[MAX_QPATH - 1] = '\0';
            break;
        case PARSE_FIELD_CVAR_VALUE:
            strncpy(destination, value, MAX_CVAR_VALUE_STRING - 1);
            destination[MAX_CVAR_VALUE_STRING - 1] = '\0';
            break;
        case PARSE_FIELD_INT:
        case PARSE_FIELD_BOOL: {
            int32_t parsed;
#if defined(WINDOWS_BEHAVIOR)
            const unsigned char *cursor = (const unsigned char *)value;
            uint32_t magnitude = 0;
            int32_t negative = 0;
            uint32_t bits;

            /* The Windows CRT accumulates in a wrapping 32-bit register. */
            while (*cursor == ' ' || (*cursor >= '\t' && *cursor <= '\r')) {
                ++cursor;
            }
            if (*cursor == '-' || *cursor == '+') {
                negative = *cursor == '-';
                ++cursor;
            }
            while (*cursor >= '0' && *cursor <= '9') {
                magnitude = magnitude * 10u + (uint32_t)(*cursor - '0');
                ++cursor;
            }
            bits = negative != 0 ? 0u - magnitude : magnitude;
            memcpy(&parsed, &bits, sizeof(parsed));
#else
            long hostValue = strtol(value, NULL, 10);

            /* The original Linux ABI has a 32-bit long. Reapply that range
             * when this source is built on an LP64 support host. */
            if (hostValue > INT32_MAX) {
                parsed = INT32_MAX;
            } else if (hostValue < INT32_MIN) {
                parsed = INT32_MIN;
            } else {
                parsed = (int32_t)hostValue;
            }
#endif
            if (field->type == PARSE_FIELD_BOOL) {
                parsed = parsed != 0;
            }
            memcpy(destination, &parsed, sizeof(parsed));
            break;
        }
        case PARSE_FIELD_FLOAT: {
            float parsed = (float)atof(value);
            memcpy(destination, &parsed, sizeof(parsed));
            break;
        }
        case PARSE_FIELD_MILLISECONDS: {
            int32_t parsed;
#if defined(WINDOWS_BEHAVIOR)
            volatile double scaledMilliseconds = atof(value) * 1000.0;

            /* ORIGINAL_PLATFORM_DIFFERENCE: every authoritative Windows body
             * keeps atof's binary64 value through the multiply under the
             * process PC=53 x87 policy, then retains _ftol2's low dword. */
            parsed = coduo_fp_to_i32_f64(scaledMilliseconds);
#else
            volatile float seconds = (float)atof(value);
            const long double scaledMilliseconds = (long double)seconds * (long double)1000.0f;

            /* ORIGINAL_PLATFORM_DIFFERENCE: both authoritative Linux server
             * bodies explicitly spill atof to binary32 before multiplying.
             * Their process x87 policy is PC=64, but this product needs at
             * most 34 significant bits after that spill. */
            parsed = coduo_fp_to_i32_extended(scaledMilliseconds);
#endif
            memcpy(destination, &parsed, sizeof(parsed));
            break;
        }
        default:
            break;
        }
    }

    return fieldIndex == fieldCount;
}

/* Source: CoDUOMP.exe 0x0043a5c0..0x0043a6c4.
 * Source: coduo_lnxded 0x08070924..0x08070a71.
 * Name: exact same-module Mac client symbol Info_Print. */
void Info_Print(const char *info)
{
    char key[INFO_PRINT_BUFFER_CAPACITY];
    char value[INFO_PRINT_BUFFER_CAPACITY];
    size_t infoLength = 0;

    /* NOT_FROM_ORIGINAL_SOURCE: require a terminated info string inside the
     * protocol extent before copying complete components for display. */
    while (infoLength < MAX_INFO_STRING && info[infoLength] != '\0') {
        ++infoLength;
    }
    if (infoLength == MAX_INFO_STRING) {
        Com_Printf("Info_Print: oversize infostring\n");
        return;
    }

    if (*info == '\\') {
        ++info;
    }

    while (*info != '\0') {
        char *keyOut = key;
        char *valueOut;

        while (*info != '\0' && *info != '\\') {
            *keyOut++ = *info++;
        }
        while (keyOut < key + INFO_PRINT_FIELD_WIDTH) {
            *keyOut++ = ' ';
        }
        *keyOut = '\0';
        Com_Printf("%s", key);

        if (*info == '\0') {
            Com_Printf("MISSING VALUE\n");
            return;
        }
        ++info;

        valueOut = value;
        while (*info != '\0' && *info != '\\') {
            *valueOut++ = *info++;
        }
        *valueOut = '\0';
        Com_Printf("%s\n", value);
        if (*info == '\\') {
            ++info;
        }
    }
}
