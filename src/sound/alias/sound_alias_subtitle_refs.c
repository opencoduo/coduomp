#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include "compat/coduo_ctype_compat.h"
#include "sound_alias_private.h"

enum {
    SOUND_ALIAS_HASH_MULTIPLIER = 31337,
    SOUND_ALIAS_HASH_BUCKET_MASK = SND_ALIAS_HASH_BUCKET_COUNT - 1,
    SOUND_ALIAS_NAME_MIN_PRINTABLE = 31,
};

#define SOUND_ALIAS_SUBTITLE_PREFIX "SUBTITLE_"
#define SOUND_ALIAS_SUBTITLE_FILE "soundaliases/subtitle.st"
#define SOUND_ALIAS_SUBTITLE_READ_WARNING_FORMAT "WARNING: Could not read local copy of StringEd file %s\n"
#define SOUND_ALIAS_SUBTITLE_REFERENCE_TOKEN "REFERENCE"
#define SOUND_ALIAS_SUBTITLE_BAD_SYNTAX_FORMAT "StringEd file %s has bad syntax"
#define SOUND_ALIAS_SUBTITLE_LANG_ENGLISH_TOKEN "LANG_ENGLISH"

/* CoDUOMP.exe 0x00436610..0x00436645 and coduo_lnxded
 * 0x0806c7e4..0x0806c82a; canonical name confirmed by the supporting Mac
 * engine symbol. */
uint32_t Com_HashAliasName(const char *name)
{
    /*
     * The stock hash recurrence (multiply by 31337, then add) relies on 32-bit
     * two's-complement wraparound. Accumulate in uint32_t so the overflow is
     * defined; the low 32 bits are bit-identical to the signed imul/add the
     * original computes.
     */
    uint32_t hash;

    hash = 0;
    while (*name != '\0') {
#if defined(WINDOWS_BEHAVIOR)
        /* MSVC's retained body lowers only ASCII A-Z. Linux reaches its CRT
         * ctype table instead. Accepted alias names use the common ASCII
         * grammar, but retain each original lowering for raw lookup input. */
        int32_t character = (int32_t)(int8_t)(uint8_t)*name;
        if (character >= 'A' && character <= 'Z') {
            character += 'a' - 'A';
        }
        hash = (uint32_t)character + hash * SOUND_ALIAS_HASH_MULTIPLIER;
#else
        hash = (uint32_t)tolower(coduo_ctype_signed_byte_arg(*name)) + hash * SOUND_ALIAS_HASH_MULTIPLIER;
#endif
        name++;
    }

    return hash & SOUND_ALIAS_HASH_BUCKET_MASK;
}

/* CoDUOMP.exe 0x00436650..0x004366a1 and coduo_lnxded
 * 0x0806c82a..0x0806c8ba; canonical name confirmed by the supporting Mac
 * engine symbol. */
qboolean Com_IsValidAliasName(const char *name)
{
#if defined(WINDOWS_BEHAVIOR)
    /* The authoritative Windows body uses the Quake ASCII predicates; the
     * Linux body below uses its signed-byte CRT ctype path. Both accept the
     * same maintained alias-name domain under the original C locale. */
    const uint8_t *cursor = (const uint8_t *)name;

    if ((int8_t)*cursor <= SOUND_ALIAS_NAME_MIN_PRINTABLE || (!Q_isalpha(*cursor) && *cursor != '_')) {
        return qfalse;
    }

    for (++cursor; *cursor != '\0'; ++cursor) {
        if ((int8_t)*cursor <= SOUND_ALIAS_NAME_MIN_PRINTABLE || (!Q_isalphanumeric(*cursor) && *cursor != '_')) {
            return qfalse;
        }
    }

    return qtrue;
#else
    signed char ch;

    ch = (signed char)*name;
    if (ch <= SOUND_ALIAS_NAME_MIN_PRINTABLE || (isalpha(coduo_ctype_signed_byte_arg(ch)) == 0 && ch != '_')) {
        return qfalse;
    }

    do {
        name++;
        ch = (signed char)*name;
        if (ch == '\0') {
            return qtrue;
        }
        if (ch <= SOUND_ALIAS_NAME_MIN_PRINTABLE || (isalnum(coduo_ctype_signed_byte_arg(ch)) == 0 && ch != '_')) {
            return qfalse;
        }
    } while (1);
#endif
}

/* CoDUOMP.exe 0x004385c0..0x00438766 and coduo_lnxded
 * 0x0806e8fa..0x0806ea00; canonical name confirmed by the supporting Mac
 * engine symbol. */
qboolean Com_SoundAliasSubtitleReferenceExists(const char *subtitle)
{
    void *fileBuffer;
    char *parseCursor;
    char *token;
    qboolean found;

    found = qfalse;
    if (Q_strncmp(subtitle, SOUND_ALIAS_SUBTITLE_PREFIX, (int)(sizeof(SOUND_ALIAS_SUBTITLE_PREFIX) - 1)) != 0) {
        return qfalse;
    }

    if (FS_ReadFile(SOUND_ALIAS_SUBTITLE_FILE, &fileBuffer) < 0) {
        Com_Printf(SOUND_ALIAS_SUBTITLE_READ_WARNING_FORMAT, SOUND_ALIAS_SUBTITLE_FILE);
        return qfalse;
    }

    Com_BeginParseSession(SOUND_ALIAS_SUBTITLE_FILE);
    parseCursor = fileBuffer;
    for (;;) {
        token = Com_Parse(&parseCursor);
        if (parseCursor == NULL) {
            break;
        }
        if (strcmp(token, SOUND_ALIAS_SUBTITLE_REFERENCE_TOKEN) == 0) {
            token = Com_ParseOnLine(&parseCursor);
            if (Q_stricmp(subtitle + sizeof(SOUND_ALIAS_SUBTITLE_PREFIX) - 1, token) == 0) {
                found = qtrue;
                break;
            }
        }
        Com_SkipRestOfLine(&parseCursor);
    }

    Com_EndParseSession();
    FS_FreeFile(fileBuffer);
    return found;
}

/* CoDUOMP.exe 0x00438770..0x004389ee and coduo_lnxded
 * 0x0806ea00..0x0806eb5f; canonical name confirmed by the supporting Mac
 * engine symbol. */
const char *Com_SoundAliasSubtitleReferenceForText(const char *englishText)
{
    void *fileBuffer;
    char *parseCursor;
    char *token;

    if (FS_ReadFile(SOUND_ALIAS_SUBTITLE_FILE, &fileBuffer) < 0) {
        Com_Printf(SOUND_ALIAS_SUBTITLE_READ_WARNING_FORMAT, SOUND_ALIAS_SUBTITLE_FILE);
        return NULL;
    }

    Com_BeginParseSession(SOUND_ALIAS_SUBTITLE_FILE);
    parseCursor = fileBuffer;
    while (parseCursor != NULL) {
        token = Com_Parse(&parseCursor);
        if (parseCursor == NULL) {
            break;
        }
        if (strcmp(token, SOUND_ALIAS_SUBTITLE_REFERENCE_TOKEN) == 0) {
            token = Com_ParseOnLine(&parseCursor);
            strcpy(com_soundAliasSubtitleReference, token);
            Com_SkipRestOfLine(&parseCursor);

            do {
                token = Com_Parse(&parseCursor);
                if (parseCursor == NULL) {
                    Com_Error(ERR_DROP, SOUND_ALIAS_SUBTITLE_BAD_SYNTAX_FORMAT, SOUND_ALIAS_SUBTITLE_FILE);
                }
            } while (strcmp(token, SOUND_ALIAS_SUBTITLE_LANG_ENGLISH_TOKEN) != 0);

            token = Com_ParseOnLine(&parseCursor);
            if (Q_stricmp(englishText, token) == 0) {
                Com_EndParseSession();
                FS_FreeFile(fileBuffer);
                return com_soundAliasSubtitleReference;
            }
        }
        Com_SkipRestOfLine(&parseCursor);
    }

    Com_EndParseSession();
    FS_FreeFile(fileBuffer);
    return NULL;
}
