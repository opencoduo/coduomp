#include "ui_memory.h"

#include "ui_memory_config.h"
#include "ui_menu_globals.h"
#include "ui_parse.h"
#include "ui_runtime.h"

#include "compat/coduo_int32_bits.h"
#include "compat/crt/msvc_compat.h"

#include <stdint.h>
#include <string.h>

void Com_Printf(const char *format, ...);
void Com_Error(errorParm_t code, const char *format, ...);
void Item_SetupKeywordHash(void);
void Menu_SetupKeywordHash(void);

enum {
    UI_MEMORY_ALIGNMENT = 16,
    UI_MEMORY_ALIGNMENT_MASK = UI_MEMORY_ALIGNMENT - 1,
    UI_STRING_HASH_BASE_WEIGHT = 119
};

/*
 * Complete ui_shared.c memory and string-pool subsystem.  The retained
 * Windows client-DLL bodies agree instruction for instruction apart from
 * relocated storage/callees and the two module-owned pool-limit immediates:
 *
 *                                   cgame       UI
 * UI_Alloc                          0x3004fd50  0x40011870
 * UI_InitMemory                     0x3004fda0  0x400118c0
 * UI_OutOfMemory                    0x3004fdb0  0x400118d0
 * String_Hash                       0x3004fdc0  0x400118e0
 * String_Alloc                      0x3004fe00  0x40011920
 * String_Init                       0x3004ffb0  0x40011ad0
 *
 * Cgame owns 128 KiB memory and string pools.  UI owns a 1 MiB memory pool
 * and a 384 KiB string pool.  Those original object extents remain in each
 * module's ui_memory_config.h rather than masquerading as platform behavior.
 */

void *UI_Alloc(size_t size)
{
    const uint32_t requestedBits = (uint32_t)size;
    const uint32_t usedBits = (uint32_t)allocPoint;
    const int32_t requestedSize = coduo_int32_from_bits(requestedBits);
    const int32_t requestedEnd =
        coduo_int32_from_bits(usedBits + requestedBits);
    void *allocation;

    /* Both originals use a signed comparison before rounding the request. */
    if (requestedEnd > UI_MEMORY_POOL_CAPACITY) {
        outOfMemory = qtrue;
        Com_Printf("UI_Alloc: failed to allocate %d bytes\n", requestedSize);
        Com_Error(ERR_DROP,
                  "\x15" "UI_Alloc: " "\x14" "EXE_ERR_OUT_OF_MEMORY");
        return NULL;
    }

    allocation = &memoryPool[coduo_int32_from_bits(usedBits)];
    allocPoint = coduo_int32_from_bits(
        usedBits +
        ((requestedBits + UI_MEMORY_ALIGNMENT_MASK) &
         ~(uint32_t)UI_MEMORY_ALIGNMENT_MASK));
    return allocation;
}

void UI_InitMemory(void)
{
    allocPoint = 0;
    outOfMemory = qfalse;
}

qboolean UI_OutOfMemory(void)
{
    return outOfMemory;
}

int32_t String_Hash(const char *string)
{
    uint32_t hash = 0;
    uint32_t index;

    for (index = 0; string[index] != '\0'; ++index) {
        const int32_t character = (int32_t)(signed char)string[index];

        hash += (uint32_t)coduo_crt_tolower(character) *
                (UI_STRING_HASH_BASE_WEIGHT + index);
    }
    return (int32_t)(hash & (UI_STRING_HASH_SIZE - 1));
}

const char *String_Alloc(const char *string)
{
    stringDef_t *entry;
    stringDef_t *previous;
    int32_t hash;
    int32_t length;
    int32_t nextPoolIndex;
    char *pooledString;

    if (string == NULL) {
        return NULL;
    }
    if (string[0] == '\0') {
        return emptyStr;
    }

    hash = String_Hash(string);
    entry = strHandle[hash];
    while (entry != NULL) {
        if (strcmp(string, entry->str) == 0) {
            return entry->str;
        }
        entry = entry->next;
    }

    length = (int32_t)strlen(string);
    nextPoolIndex = coduo_int32_from_bits(
        (uint32_t)strPoolIndex + (uint32_t)length + 1u);
    if (nextPoolIndex >= UI_STRING_POOL_CAPACITY) {
        Com_Printf("String_Alloc: failed to allocate %d bytes\n",
                   coduo_int32_from_bits((uint32_t)length + 1u));
        Com_Error(ERR_DROP,
                  "\x15" "String_Alloc: " "\x14" "EXE_ERR_OUT_OF_MEMORY");
        return NULL;
    }

    pooledString = &strPool[strPoolIndex];
    strcpy(pooledString, string);
    strPoolIndex = nextPoolIndex;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    previous = strHandle[hash];
    if (previous != NULL) {
        while (previous->next != NULL) {
            previous = previous->next;
        }
    }

    entry = UI_Alloc(sizeof(*entry));
    entry->next = NULL;
    entry->str = pooledString;
    if (previous != NULL) {
        previous->next = entry;
    } else {
        strHandle[hash] = entry;
    }
    return pooledString;
}

void String_Init(void)
{
    int32_t index;

    for (index = 0; index < UI_STRING_HASH_SIZE; ++index) {
        strHandle[index] = NULL;
    }
    strHandleCount = 0;
    strPoolIndex = 0;
    menuCount = 0;
    openMenuCount = 0;
    allocPoint = 0;
    outOfMemory = qfalse;
    Item_SetupKeywordHash();
    Menu_SetupKeywordHash();
    if (DC != NULL && DC->getBindingBuf != NULL) {
        UI_STRING_REFRESH_BINDINGS();
    }
}
