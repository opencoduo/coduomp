#include "ui_runtime.h"

#include "compat/coduo_int32_bits.h"
#include "qcommon/q_string.h"

#include <stddef.h>
#include <stdint.h>

enum {
    KEYWORD_HASH_START_WEIGHT = 119,
    KEYWORD_HASH_MIX_SHIFT = 10,
    KEYWORD_HASH_MASK = KEYWORDHASH_SIZE - 1,
    KEYWORD_FIND_COMPARE_LIMIT = 99999
};

/*
 * The three standalone keyword-hash primitives are instruction-identical
 * between the original Windows client modules, apart from image addresses:
 *
 *   uo_cgame_mp_x86.dll  0x30058e40, 0x30058e90, 0x30058eb0
 *   uo_ui_mp_x86.dll     0x4001a9b0, 0x4001aa00, 0x4001aa20
 *
 * Their canonical keywordHash_t interface belongs to the item parser.  Menu
 * setup and parsing use the layout-compatible menuKeywordHash_t record, but
 * retain their own inlined insertion/traversal bodies in both original DLLs.
 */
int32_t KeywordHash_Key(const char *keyword)
{
    uint32_t sum = 0;
    int32_t weight = KEYWORD_HASH_START_WEIGHT;
    uint32_t mixed;

    while (*keyword != '\0') {
        int32_t character = (int8_t)*keyword++;

        if (character >= 'A' && character <= 'Z') {
            character += 'a' - 'A';
        }
        sum += (uint32_t)(character * weight++);
    }

    mixed = (uint32_t)coduo_int32_sar(
                sum, KEYWORD_HASH_MIX_SHIFT) ^ sum;
    mixed = (uint32_t)coduo_int32_sar(
                mixed, KEYWORD_HASH_MIX_SHIFT) ^ sum;
    return (int32_t)(mixed & KEYWORD_HASH_MASK);
}

void KeywordHash_Add(keywordHash_t **hashTable, keywordHash_t *keyword)
{
    const int32_t hash = KeywordHash_Key(keyword->keyword);

    keyword->next = hashTable[hash];
    hashTable[hash] = keyword;
}

keywordHash_t *KeywordHash_Find(keywordHash_t *const *hashTable,
                                const char *keyword)
{
    keywordHash_t *entry = hashTable[KeywordHash_Key(keyword)];

    while (entry != NULL) {
        if (entry->keyword != NULL && keyword != NULL &&
            Q_stricmpn(keyword, entry->keyword,
                       KEYWORD_FIND_COMPARE_LIMIT) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}
