#include "bg_animation.h"
#include "bg_animation_services.h"
#include "compat/coduo_int32_bits.h"
#include "qcommon/q_string.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    BG_ANIM_HASH_INITIAL_MULTIPLIER = 119,
    BG_STRING_WHOLE_COMPARE_COUNT = 99999
};

/*
 * The original Windows cgame and game functions have the same instruction
 * graph at 0x300011b0 and 0x200011b0. Linux game retains the symbol at RVA
 * 0x0001f439 and the same signed-byte hash. No original image calls setlocale,
 * so each CRT tolower call has the C-locale result expressed below: ASCII
 * uppercase folds and all other stored bytes remain unchanged.
 */
int32_t BG_StringHashValue(const char *text)
{
    const int8_t *cursor = (const int8_t *)text;
    uint32_t hash = 0;
    uint32_t multiplier = BG_ANIM_HASH_INITIAL_MULTIPLIER;

    while (*cursor != 0) {
        int32_t character = *cursor++;

        if (character >= 'A' && character <= 'Z') {
            character += 'a' - 'A';
        }
        hash += (uint32_t)character * multiplier++;
    }

    if (hash == UINT32_MAX) {
        hash = 0;
    }
    return coduo_int32_from_bits(hash);
}

/*
 * Original Windows bodies: cgame 0x30001420, game 0x20001410. Their table
 * stride, lazy hash fill, hash gate, case-insensitive confirmation, missing
 * diagnostic, and return paths agree. Linux game RVA 0x0001f77d has the same
 * behavior. The Linux ELF signature supplies the canonical argument order.
 */
int32_t BG_IndexForString(const char *name, bg_indexed_string_t *strings,
                          qboolean allowMissing)
{
    const int32_t hash = BG_StringHashValue(name);
    int32_t index;

    for (index = 0; strings[index].name != NULL; ++index) {
        if (strings[index].hash == BG_INDEXED_STRING_HASH_UNSET) {
            strings[index].hash = BG_StringHashValue(strings[index].name);
        }

        if (hash == strings[index].hash && name != NULL &&
            strings[index].name != NULL &&
            Q_stricmpn(name, strings[index].name,
                       BG_STRING_WHOLE_COMPARE_COUNT) == 0) {
            return index;
        }
    }

    if (!allowMissing) {
        BG_AnimParseError("BG_IndexForString: unknown token '%s'", name);
    }
    return -1;
}

/*
 * Original Windows bodies: cgame 0x300014a0, game 0x20001490. Linux game RVA
 * 0x0001f877 agrees, including its strict `used + strlen + 1 < bufferSize`
 * test. Keep the original 32-bit unsigned addition on wider hosts.
 */
char *BG_CopyStringIntoBuffer(const char *text, char *buffer,
                              uint32_t bufferSize, int32_t *used)
{
    const uint32_t textLength = (uint32_t)strlen(text);
    const uint32_t nextUsed = (uint32_t)*used + textLength + UINT32_C(1);
    uint32_t copiedBytes;
    char *out;

    if (nextUsed >= bufferSize) {
        BG_AnimParseError("BG_CopyStringIntoBuffer: out of buffer space");
    }

    out = &buffer[*used];
    strcpy(out, text);
    copiedBytes = (uint32_t)strlen(text) + UINT32_C(1);
    *used = coduo_int32_from_bits((uint32_t)*used + copiedBytes);
    return out;
}

/*
 * Original Windows bodies: cgame 0x300012a0, game 0x20001290. Linux game RVA
 * 0x0001f550 has the same two-mode lookup/registration behavior. The apparent
 * module difference is only how Scr_FindAnim reaches its owner: cgame invokes
 * the script-import slot and game invokes its syscall veneer.
 */
int32_t BG_AnimationIndexForString(const char *name)
{
    const int32_t hash = BG_StringHashValue(name);
    int32_t count;
    int32_t index;

    if (bgRuntimeAnimations == NULL) {
        for (index = 0; index < bgAnimStaticTable->entryCount; ++index) {
            bg_static_animation_t *animation =
                &bgAnimStaticTable->entries[index];

            if (hash == animation->hash && name != NULL &&
                Q_stricmpn(name, animation->name,
                           BG_STRING_WHOLE_COMPARE_COUNT) == 0) {
                return index;
            }
        }

        BG_AnimParseError(
            "BG_AnimationIndexForString: unknown player animation '%s'", name);
        return -1;
    }

    count = *bgRuntimeAnimationCount;
    /* NOT_FROM_ORIGINAL_SOURCE: a full table remains searchable, but every
     * published count must stay within the shared animation array. */
    if (count < 0 || count > BG_ANIM_MAX_ANIMATIONS) {
        BG_AnimParseError("BG_AnimationIndexForString: invalid runtime animation count %i", count);
        return -1;
    }
    for (index = 0; index < count; ++index) {
        bg_runtime_animation_t *animation = &bgRuntimeAnimations[index];

        /* The Windows TEST of the name cursor cannot fail for an entry reached
         * from a non-NULL array base; the field is an inline array in the
         * recovered type, so no separate source-level pointer test remains. */
        if (hash == animation->hash && name != NULL &&
            Q_stricmpn(name, animation->name,
                       BG_STRING_WHOLE_COMPARE_COUNT) == 0) {
            return index;
        }
    }

    {
        bg_runtime_animation_t *animation;
        int32_t *publishCount;
        int32_t newCount;

        /* NOT_FROM_ORIGINAL_SOURCE: a new entry requires one free record and a
         * complete terminated name that fits its ABI-fixed inline field. */
        if (count == BG_ANIM_MAX_ANIMATIONS) {
            BG_AnimParseError("BG_AnimationIndexForString: exceeded maximum runtime animations (%i)", BG_ANIM_MAX_ANIMATIONS);
            return -1;
        }
        if (strlen(name) >= sizeof(bgRuntimeAnimations[count].name)) {
            BG_AnimParseError("BG_AnimationIndexForString: animation name is too long");
            return -1;
        }

        animation = &bgRuntimeAnimations[count];
        Scr_FindAnim("multiplayer", name, &animation->anim);
        strcpy(animation->name, name);
        animation->hash = hash;
        publishCount = bgRuntimeAnimationCount;
        newCount = coduo_int32_from_bits((uint32_t)*publishCount + UINT32_C(1));
        *publishCount = newCount;
        return newCount - 1;
    }
}

/*
 * Original Windows bodies: cgame 0x300013b0, game 0x200013a0. Linux game RVA
 * 0x0001f6d2 agrees on lookup order, error level/string, and NULL fallback.
 */
bg_static_animation_t *BG_AnimationForString(const char *name)
{
    const int32_t hash = BG_StringHashValue(name);
    int32_t index;

    for (index = 0; index < bgAnimStaticTable->entryCount; ++index) {
        bg_static_animation_t *animation = &bgAnimStaticTable->entries[index];

        if (hash == animation->hash && name != NULL &&
            Q_stricmpn(name, animation->name,
                       BG_STRING_WHOLE_COMPARE_COUNT) == 0) {
            return animation;
        }
    }

    Com_Error(ERR_DROP,
              "\x15" "BG_AnimationForString: unknown player animation '%s'",
              name);
    return NULL;
}
