#include "script_memory.h"
#include "script_runtime_host.h"
#include "script_string.h"

#include "compat/coduo_ctype_compat.h"
#include "qcommon/com_sprintf.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* The retained string interning subsystem corresponds to the common Windows
 * CoDUOMP.exe SL_* cluster and the coduo_lnxded cluster beginning at
 * 0x080a4430.  Its
 * table geometry, string-entry layout, hashing, ownership bits, reference
 * operations, and canonicalization agree; the few proven source-interface
 * distinctions are gated at the affected functions below. */

enum {
    SCRIPT_STRING_HASH_SHORT_TEXT_LIMIT = 256,
    SCRIPT_STRING_ENTRY_BYTE_COUNT_LIMIT = UINT8_MAX,
    SCRIPT_STRING_HASH_MULTIPLIER = 31,
    SCRIPT_STRING_HASH_OCCUPIED = 0x8000,
    SCRIPT_STRING_HASH_CHAINED = 0x4000,
    SCRIPT_STRING_HASH_FLAGS_MASK = 0xc000,
    SCRIPT_STRING_HASH_LINK_MASK = 0x3fff,
    SCRIPT_STRING_CANONICAL_FILENAME_TYPE = 7,
    SCRIPT_STRING_DEFAULT_TYPE = 6,
    SCRIPT_STRING_RUNTIME_TYPE = 14,
    SCRIPT_STRING_USAGE_RUNTIME = 1,
    SCRIPT_STRING_FORMAT_BUFFER_SIZE = 128,
    /* Three negative finite FLT_MAX values need at most 135 visible bytes in
     * the retail "(%.2f, %.2f, %.2f)" form, plus the terminator. */
    SCRIPT_VECTOR_FORMAT_BUFFER_SIZE = 136,
    SCRIPT_STRING_HANDLE_UNIT_SIZE = 8,
    SCRIPT_STRING_TEMP_HASH_ENTRY_COUNT = 65536
};

typedef struct script_string_entry_s {
    int16_t refCount;
    uint8_t byteCount;
    uint8_t flags;
    char text[];
} script_string_entry_t;

/* NOT_FROM_ORIGINAL_SOURCE: express the original INC/DEC word operations
 * without relying on the host C implementation's signed-overflow rules. */
static int16_t coduomp_script_string_ref_from_bits(uint16_t bits)
{
    int16_t value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: defined signed view of a target dword for the
 * original pointer-delta division and decrement-to-index operations. */
static int32_t coduomp_script_string_int32_from_bits(uint32_t bits)
{
    int32_t value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level spelling of the original word INC. */
static void coduomp_script_string_increment_ref(script_string_entry_t *entry)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((uint16_t)entry->refCount == SCRIPT_REFERENCE_COUNT_MAX) {
        Com_Error(ERR_DROP, "\x15"
                            "script string reference count overflow");
    }
    entry->refCount = coduomp_script_string_ref_from_bits((uint16_t)((uint16_t)entry->refCount + 1u));
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level spelling of the original word DEC. */
static void coduomp_script_string_decrement_ref(script_string_entry_t *entry)
{
    entry->refCount = coduomp_script_string_ref_from_bits((uint16_t)((uint16_t)entry->refCount - 1u));
}

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(script_string_entry_t) == 0x04 && _Alignof(script_string_entry_t) == 0x02 &&
                   offsetof(script_string_entry_t, byteCount) == 0x02 && offsetof(script_string_entry_t, flags) == 0x03 &&
                   offsetof(script_string_entry_t, text) == 0x04,
               "original i386 script-string entry layout changed");
#endif

/* Source: CoDUOMP.exe 0x00482270..0x0048227c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482270_0048227d.mcode.
 * Same-module Mac identity: GetRefString(unsigned short). The returned pointer
 * is the header of the string-pool entry selected by the 16-bit arena handle. */
static script_string_entry_t *GetRefStringByHandle(uint16_t string)
{
    return (script_string_entry_t *)(script_stringPoolBase + (size_t)string * SCRIPT_STRING_HANDLE_UNIT_SIZE);
}

/* Source: CoDUOMP.exe 0x00482280..0x00482283.
 * Same-module Mac identity: GetRefString(const char *). This overload reverses
 * the fixed text-member displacement used by SL_ConvertToString. */
static const script_string_entry_t *GetRefStringByText(const char *text)
{
    return (const script_string_entry_t *)((const uint8_t *)(const void *)text - offsetof(script_string_entry_t, text));
}

/* Source: CoDUOMP.exe 0x004822b0..0x004822c2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004822b0_004822c3.mcode.
 * Name and argument: exact same-module Mac symbol SL_ConvertFromString. */
uint16_t SL_ConvertFromString(const char *text)
{
    const script_string_entry_t *entry = GetRefStringByText(text);
    uint32_t byteOffsetBits = (uint32_t)((uintptr_t)(const void *)entry - (uintptr_t)(const void *)script_stringPoolBase);
    int32_t byteOffset = coduomp_script_string_int32_from_bits(byteOffsetBits);

    return (uint16_t)(byteOffset / SCRIPT_STRING_HANDLE_UNIT_SIZE);
}

/* NOT_FROM_ORIGINAL_SOURCE: local predicate for the repeated string-entry
 * size and byte comparison in the original lookup and intern functions. */
static qboolean coduomp_script_string_entry_matches(const script_string_hash_slot_t *slot, const char *text, size_t size)
{
    const script_string_entry_t *entry = GetRefStringByHandle(slot->stringHandle);

    /* NOT_FROM_ORIGINAL_SOURCE: callers establish that size fits the entry's
     * one-byte length field before this complete comparison. */
    return entry->byteCount == (uint8_t)size && memcmp(entry->text, text, size) == 0 ? qtrue : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the hash-chain head
 * rotation emitted inline by the original compiler. */
static void coduomp_script_string_move_slot_to_hash_head(uint16_t hash, uint16_t previousSlot, uint16_t matchSlot)
{
    script_string_hash_slot_t *head = &script_stringHashSlots[hash];
    script_string_hash_slot_t *previous = &script_stringHashSlots[previousSlot];
    script_string_hash_slot_t *match = &script_stringHashSlots[matchSlot];

    previous->linkAndFlags =
        (uint16_t)((match->linkAndFlags & SCRIPT_STRING_HASH_LINK_MASK) | (previous->linkAndFlags & SCRIPT_STRING_HASH_FLAGS_MASK));
    match->linkAndFlags =
        (uint16_t)((head->linkAndFlags & SCRIPT_STRING_HASH_LINK_MASK) | (match->linkAndFlags & SCRIPT_STRING_HASH_FLAGS_MASK));
    head->linkAndFlags = (uint16_t)(matchSlot | (head->linkAndFlags & SCRIPT_STRING_HASH_FLAGS_MASK));

    uint16_t movedHandle = match->stringHandle;
    match->stringHandle = head->stringHandle;
    head->stringHandle = movedHandle;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the repeated free hash
 * slot removal and exhaustion check in SL_GetStringOfLen. */
static uint16_t coduomp_script_string_pop_free_hash_slot(void)
{
    uint16_t freeSlot = script_stringHashSlots[0].linkAndFlags;
    if (freeSlot == 0) {
        Scr_DumpScriptThreads();
        Com_Error(1, "\x15"
                     "exceeded maximum number of script strings\n");
    }

    script_stringHashSlots[0].linkAndFlags = script_stringHashSlots[freeSlot].linkAndFlags & SCRIPT_STRING_HASH_LINK_MASK;
    script_stringHashSlots[script_stringHashSlots[0].linkAndFlags].stringHandle = 0;
    return freeSlot;
}

/* NOT_FROM_ORIGINAL_SOURCE: factors insertion of a collision node after an
 * occupied hash head; the original function emits these assignments inline. */
static void coduomp_script_string_insert_into_occupied_head(uint16_t hash, uint16_t freeSlot)
{
    script_string_hash_slot_t *head = &script_stringHashSlots[hash];
    script_string_hash_slot_t *slot = &script_stringHashSlots[freeSlot];

    slot->linkAndFlags = (uint16_t)((head->linkAndFlags & SCRIPT_STRING_HASH_LINK_MASK) | SCRIPT_STRING_HASH_CHAINED);
    head->linkAndFlags = (uint16_t)((head->linkAndFlags & SCRIPT_STRING_HASH_FLAGS_MASK) | freeSlot);
    slot->stringHandle = head->stringHandle;
}

/* NOT_FROM_ORIGINAL_SOURCE: factors unlinking an unused hash head from the
 * doubly represented free-slot chain. */
static void coduomp_script_string_unlink_free_slot(uint16_t slot)
{
    uint16_t next = script_stringHashSlots[slot].linkAndFlags & SCRIPT_STRING_HASH_LINK_MASK;
    uint16_t previous = script_stringHashSlots[slot].stringHandle;

    script_stringHashSlots[previous].linkAndFlags =
        (uint16_t)(next | (script_stringHashSlots[previous].linkAndFlags & SCRIPT_STRING_HASH_FLAGS_MASK));
    script_stringHashSlots[next].stringHandle = previous;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of collision-head
 * insertion when the hash-numbered slot already belongs to another chain. */
static void coduomp_script_string_insert_into_linked_hash(uint16_t hash, uint16_t freeSlot)
{
    uint16_t previousSlot = script_stringHashSlots[hash].linkAndFlags & SCRIPT_STRING_HASH_LINK_MASK;
    while ((script_stringHashSlots[previousSlot].linkAndFlags & SCRIPT_STRING_HASH_LINK_MASK) != hash) {
        previousSlot = script_stringHashSlots[previousSlot].linkAndFlags & SCRIPT_STRING_HASH_LINK_MASK;
    }

    script_stringHashSlots[previousSlot].linkAndFlags =
        (uint16_t)((script_stringHashSlots[previousSlot].linkAndFlags & SCRIPT_STRING_HASH_FLAGS_MASK) | freeSlot);
    script_stringHashSlots[freeSlot].linkAndFlags =
        (uint16_t)((script_stringHashSlots[hash].linkAndFlags & SCRIPT_STRING_HASH_LINK_MASK) | SCRIPT_STRING_HASH_CHAINED);
    script_stringHashSlots[freeSlot].stringHandle = script_stringHashSlots[hash].stringHandle;
}

/* Source: CoDUOMP.exe 0x004822d0..0x0048230b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004822d0_0048230c.mcode. */
uint16_t GetHashCode(const char *text, size_t size)
{
    uint32_t targetSize = (uint32_t)size;
    uint32_t hash;

    if (targetSize < SCRIPT_STRING_HASH_SHORT_TEXT_LIMIT) {
        hash = 0;
        for (uint32_t index = 0; index < targetSize; ++index) {
            hash = hash * SCRIPT_STRING_HASH_MULTIPLIER + (int8_t)(uint8_t)text[index];
        }
    } else {
        hash = targetSize >> 2;
    }

    return (uint16_t)(hash % SCRIPT_STRING_HASH_LINK_MASK + 1);
}

/* Source: CoDUOMP.exe 0x00482390..0x004824d7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482390_004824d8.mcode. */
uint16_t SL_FindStringOfLen(const char *text, size_t size)
{
    /* NOT_FROM_ORIGINAL_SOURCE: reject lookup sizes that the entry's one-byte
     * length field cannot represent. */
    if (size > SCRIPT_STRING_ENTRY_BYTE_COUNT_LIMIT) {
        return 0;
    }

    size = (uint32_t)size;
    uint16_t hash = GetHashCode(text, size);
    script_string_hash_slot_t *head = &script_stringHashSlots[hash];

    if ((head->linkAndFlags & SCRIPT_STRING_HASH_FLAGS_MASK) != SCRIPT_STRING_HASH_OCCUPIED) {
        return 0;
    }
    if (coduomp_script_string_entry_matches(head, text, size) != qfalse) {
        return head->stringHandle;
    }

    uint16_t previousSlot = hash;
    uint16_t currentSlot = head->linkAndFlags;
    for (;;) {
        currentSlot &= SCRIPT_STRING_HASH_LINK_MASK;
        script_string_hash_slot_t *current = &script_stringHashSlots[currentSlot];
        if (current == head) {
            return 0;
        }
        if (coduomp_script_string_entry_matches(current, text, size) != qfalse) {
            coduomp_script_string_move_slot_to_hash_head(hash, previousSlot, currentSlot);
            return head->stringHandle;
        }

        previousSlot = currentSlot;
        currentSlot = current->linkAndFlags;
    }
}

/* Source: CoDUOMP.exe 0x004824e0..0x004824fb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004824e0_004824fc.mcode.
 * Name and role: exact same-module Mac symbol SL_FindString. The Windows
 * optimizer passes text in EDX and emits strlen(text) + 1 inline. */
uint16_t SL_FindString(const char *text)
{
    return SL_FindStringOfLen(text, (uint32_t)strlen(text) + 1u);
}

/* Source: CoDUOMP.exe 0x00482500..0x0048256f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482500_00482570.mcode. */
uint16_t SL_FindLowercaseString(const char *text)
{
    size_t textSize = strlen(text) + 1u;
    if (textSize > SCRIPT_STRING_ENTRY_BYTE_COUNT_LIMIT) {
        return 0;
    }

    uint32_t size = (uint32_t)textSize;
    /* NOT_FROM_ORIGINAL_SOURCE: fixed scratch capacity matches the complete
     * representable script-string entry domain established above. */
    char lowercase[SCRIPT_STRING_ENTRY_BYTE_COUNT_LIMIT];

    for (int32_t index = coduomp_script_string_int32_from_bits(size - 1u); index >= 0; --index) {
        lowercase[index] = (char)tolower(coduo_ctype_signed_byte_arg(text[index]));
    }
    return SL_FindStringOfLen(lowercase, size);
}

/* Source: CoDUOMP.exe 0x00482570..0x004828a7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482570_004828a8.mcode. */
uint16_t SL_GetStringOfLen(const char *text, uint8_t user, size_t size, int32_t type)
{
    /* NOT_FROM_ORIGINAL_SOURCE: reject sizes that the entry's one-byte length
     * field cannot represent before hash matching or publication. */
    if (size > SCRIPT_STRING_ENTRY_BYTE_COUNT_LIMIT) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "script string exceeds maximum length of %i bytes",
                  SCRIPT_STRING_ENTRY_BYTE_COUNT_LIMIT - 1);
        return 0;
    }

    size = (uint32_t)size;
    uint16_t hash = GetHashCode(text, size);
    script_string_hash_slot_t *head = &script_stringHashSlots[hash];

    if ((head->linkAndFlags & SCRIPT_STRING_HASH_FLAGS_MASK) == SCRIPT_STRING_HASH_OCCUPIED) {
        if (coduomp_script_string_entry_matches(head, text, size) != qfalse) {
            script_string_entry_t *entry = GetRefStringByHandle(head->stringHandle);
            if ((entry->flags & user) == 0) {
                entry->flags |= user;
                coduomp_script_string_increment_ref(entry);
            }
            return head->stringHandle;
        }

        uint16_t previousSlot = hash;
        uint16_t currentSlot = head->linkAndFlags;
        for (;;) {
            currentSlot &= SCRIPT_STRING_HASH_LINK_MASK;
            script_string_hash_slot_t *current = &script_stringHashSlots[currentSlot];
            if (current == head) {
                break;
            }
            if (coduomp_script_string_entry_matches(current, text, size) != qfalse) {
                coduomp_script_string_move_slot_to_hash_head(hash, previousSlot, currentSlot);
                script_string_entry_t *entry = GetRefStringByHandle(head->stringHandle);
                if ((entry->flags & user) == 0) {
                    entry->flags |= user;
                    coduomp_script_string_increment_ref(entry);
                }
                return head->stringHandle;
            }

            previousSlot = currentSlot;
            currentSlot = current->linkAndFlags;
        }

        coduomp_script_string_insert_into_occupied_head(hash, coduomp_script_string_pop_free_hash_slot());
    } else {
        if ((head->linkAndFlags & SCRIPT_STRING_HASH_FLAGS_MASK) == 0) {
            coduomp_script_string_unlink_free_slot(hash);
        } else {
            coduomp_script_string_insert_into_linked_hash(hash, coduomp_script_string_pop_free_hash_slot());
        }
        head->linkAndFlags = hash | SCRIPT_STRING_HASH_OCCUPIED;
    }

    uint32_t allocationSize = (uint32_t)size + (uint32_t)sizeof(script_string_entry_t);
#if defined(WINDOWS_BEHAVIOR)
    (void)type;
    head->stringHandle = MT_AllocIndex(allocationSize);
#else
    head->stringHandle = MT_AllocIndex(allocationSize, type);
#endif
    script_string_entry_t *entry = GetRefStringByHandle(head->stringHandle);
    memcpy(entry->text, text, size);
    entry->flags = user;
    entry->refCount = 0;
    entry->byteCount = (uint8_t)size;
    return head->stringHandle;
}

/* Windows source: CoDUOMP.exe 0x004828b0..0x004828d5.
 * Linux source: coduo_lnxded 0x080a4d5c..0x080a4d98.
 * Linux narrows user to a byte at entry; Windows forwards its complete dword
 * and SL_GetStringOfLen consumes its low byte. */
#if defined(WINDOWS_BEHAVIOR)
uint16_t SL_GetString_(const char *text, int32_t user, int32_t type)
{
    return SL_GetStringOfLen(text, (uint8_t)user, (uint32_t)strlen(text) + 1u, type);
}
#else
uint16_t SL_GetString_(const char *text, uint8_t user, int32_t type)
{
    return SL_GetStringOfLen(text, user, (uint32_t)strlen(text) + 1u, type);
}
#endif

/* Source: CoDUOMP.exe 0x00482910..0x00482977.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482910_00482978.mcode. */
uint16_t SL_GetLowercaseStringOfLen(const char *text, uint8_t user, size_t size, int32_t type)
{
    if (size > SCRIPT_STRING_ENTRY_BYTE_COUNT_LIMIT) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "script string exceeds maximum length of %i bytes",
                  SCRIPT_STRING_ENTRY_BYTE_COUNT_LIMIT - 1);
        return 0;
    }

    uint32_t targetSize = (uint32_t)size;
    /* NOT_FROM_ORIGINAL_SOURCE: fixed scratch capacity matches the complete
     * representable script-string entry domain established above. */
    char lowercase[SCRIPT_STRING_ENTRY_BYTE_COUNT_LIMIT];

    for (int32_t index = coduomp_script_string_int32_from_bits(targetSize - 1u); index >= 0; --index) {
        lowercase[index] = (char)tolower(coduo_ctype_signed_byte_arg(text[index]));
    }
    return SL_GetStringOfLen(lowercase, user, targetSize, type);
}

/* Windows source: CoDUOMP.exe 0x00482980..0x004829a5.
 * Linux source: coduo_lnxded 0x080a4e42..0x080a4e7e. */
#if defined(WINDOWS_BEHAVIOR)
uint16_t SL_GetLowercaseString_(const char *text, int32_t user, int32_t type)
{
    return SL_GetLowercaseStringOfLen(text, (uint8_t)user, (uint32_t)strlen(text) + 1u, type);
}
#else
uint16_t SL_GetLowercaseString_(const char *text, uint8_t user, int32_t type)
{
    return SL_GetLowercaseStringOfLen(text, user, (uint32_t)strlen(text) + 1u, type);
}
#endif

/* Source: CoDUOMP.exe 0x00482310..0x00482387.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482310_00482388.mcode. */
void SL_Init(void)
{
    MT_Init();

    script_stringHashSlots[0].linkAndFlags = 0;
    uint16_t previous = 0;
    for (uint16_t slot = 1; slot < SCRIPT_STRING_HASH_SLOT_COUNT; ++slot) {
        script_stringHashSlots[slot].linkAndFlags = 0;
        script_stringHashSlots[previous].linkAndFlags |= slot;
        script_stringHashSlots[slot].stringHandle = previous;
        previous = slot;
    }
    script_stringHashSlots[0].stringHandle = previous;
}

/* Source: CoDUOMP.exe 0x00482290..0x004822aa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482290_004822ab.mcode.
 * String handles are allocator-unit indexes, so byte addressing is intrinsic
 * to this arena boundary rather than a recovered struct-array access. */
const char *SL_ConvertToString(uint16_t string)
{
    if (string == 0) {
        return NULL;
    }

    return GetRefStringByHandle(string)->text;
}

/* Source: CoDUOMP.exe 0x00482a00..0x00482a10.
 * Name: exact same-module Mac symbol SL_AddRefToString. */
void SL_AddRefToString(uint16_t string)
{
    coduomp_script_string_increment_ref(GetRefStringByHandle(string));
}

/* Source: CoDUOMP.exe 0x004829e0..0x004829fc.
 * Name: exact same-module Mac symbol SL_TransferRefToString. A usage bit owns
 * one reference: if another transfer already installed that owner, discard
 * the redundant reference; otherwise transfer it by setting the owner bit. */
void SL_TransferRefToString(uint16_t string, uint8_t user)
{
    script_string_entry_t *entry = GetRefStringByHandle(string);

    if ((entry->flags & user) != 0) {
        coduomp_script_string_decrement_ref(entry);
    } else {
        entry->flags |= user;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: the source-level usage-bit operation is inlined
 * at all proven executable call sites and has no standalone function body. */
void coduomp_script_string_mark_usage(uint16_t string, uint8_t usage)
{
    script_string_entry_t *entry = GetRefStringByHandle(string);

    if ((entry->flags & usage) == 0) {
        entry->flags |= usage;
    } else {
        coduomp_script_string_decrement_ref(entry);
    }
}

/* Source: CoDUOMP.exe 0x00482b00..0x00482b3d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482b00_00482b3e.mcode. */
void SL_RemoveRefToString(uint16_t string)
{
    script_string_entry_t *entry = GetRefStringByHandle(string);

    if (entry->refCount == 0) {
        SL_FreeString(string, entry->text, (uint32_t)strlen(entry->text) + 1u);
        return;
    }
    coduomp_script_string_decrement_ref(entry);
}

/* Source: CoDUOMP.exe 0x00482b40..0x00482b6c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482b40_00482b6d.mcode. */
void SL_RemoveRefToStringOfLen(uint16_t string, uint32_t size)
{
    script_string_entry_t *entry = GetRefStringByHandle(string);

    if (entry->refCount == 0) {
        SL_FreeString(string, entry->text, size);
        return;
    }
    coduomp_script_string_decrement_ref(entry);
}

/* Source: CoDUOMP.exe 0x00482a20..0x00482af6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482a20_00482af7.mcode. */
void SL_FreeString(uint16_t string, const char *text, uint32_t size)
{
    uint16_t hash = GetHashCode(text, size);
    script_string_hash_slot_t *head = &script_stringHashSlots[hash];
    script_string_hash_slot_t *freeSlot = head;

    MT_FreeIndex(string, (uint32_t)size + (uint32_t)sizeof(script_string_entry_t));

    uint16_t replacementSlot = head->linkAndFlags & SCRIPT_STRING_HASH_LINK_MASK;
    script_string_hash_slot_t *replacement = &script_stringHashSlots[replacementSlot];
    uint16_t freeSlotIndex = hash;

    if (head->stringHandle == string) {
        if (replacement != head) {
            head->linkAndFlags = (uint16_t)((replacement->linkAndFlags & SCRIPT_STRING_HASH_LINK_MASK) | SCRIPT_STRING_HASH_OCCUPIED);
            head->stringHandle = replacement->stringHandle;
            script_stringFreedHashSlot = head;
            freeSlot = replacement;
            freeSlotIndex = replacementSlot;
        }
    } else {
        uint16_t previousSlot = hash;
        while (replacement->stringHandle != string) {
            previousSlot = replacementSlot;
            replacementSlot = replacement->linkAndFlags & SCRIPT_STRING_HASH_LINK_MASK;
            replacement = &script_stringHashSlots[replacementSlot];
        }

        script_stringHashSlots[previousSlot].linkAndFlags =
            (uint16_t)((replacement->linkAndFlags & SCRIPT_STRING_HASH_LINK_MASK) |
                       (script_stringHashSlots[previousSlot].linkAndFlags & SCRIPT_STRING_HASH_FLAGS_MASK));
        freeSlot = replacement;
        freeSlotIndex = replacementSlot;
    }

    uint16_t oldFreeHead = script_stringHashSlots[0].linkAndFlags;
    freeSlot->linkAndFlags = oldFreeHead;
    freeSlot->stringHandle = 0;
    script_stringHashSlots[oldFreeHead].stringHandle = freeSlotIndex;
    script_stringHashSlots[0].linkAndFlags = freeSlotIndex;
}

/* Source: CoDUOMP.exe 0x00482b70..0x00482bd5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482b70_00482bd6.mcode. */
void Scr_SetString(uint16_t *slot, uint16_t value)
{
    if (*slot != 0) {
        SL_RemoveRefToString(*slot);
    }
    if (value != 0) {
        SL_AddRefToString(value);
    }
    *slot = value;
}

/* Source: CoDUOMP.exe 0x00482d60..0x00482dfa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482d60_00482dfb.mcode. */
void SL_ShutdownSystem(uint8_t usage)
{
    for (uint16_t slot = 1; slot < SCRIPT_STRING_HASH_SLOT_COUNT; ++slot) {
        for (;;) {
            script_string_hash_slot_t *hashSlot = &script_stringHashSlots[slot];
            if ((hashSlot->linkAndFlags & SCRIPT_STRING_HASH_FLAGS_MASK) == 0) {
                break;
            }

            script_string_entry_t *entry = GetRefStringByHandle(hashSlot->stringHandle);
            if ((entry->flags & usage) == 0) {
                break;
            }

            entry->flags &= (uint8_t)~usage;
            script_stringFreedHashSlot = NULL;
            SL_RemoveRefToString(hashSlot->stringHandle);
            if (script_stringFreedHashSlot == NULL) {
                break;
            }
        }
    }
}

/* Source: CoDUOMP.exe 0x00482e00..0x00482e66.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482e00_00482e67.mcode. */
void CreateCanonicalFilename(char *dest, const char *source, int32_t maxLength)
{
    const char *const originalSource = source;
    const int32_t originalMaxLength = maxLength;

    for (;;) {
        uint8_t ch;
        do {
            do {
                ch = (uint8_t)*source++;
            } while (ch == '\\');
        } while (ch == '/');

        while (ch > 31) {
            /* 0x00482e22 sign-extends BL before the retail CRT call. */
            *dest++ = (char)tolower(coduo_ctype_signed_byte_arg(ch));
            --maxLength;
            if (maxLength == 0) {
                /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                Com_Error(1,
                          "\x15"
                          "Filename '%s' exceeds maximum length of %d",
                          originalSource, originalMaxLength);
            }
            if (ch == '\\') {
                break;
            }

            ch = (uint8_t)*source++;
            if (ch == '/') {
                ch = '\\';
            }
        }

        if (ch == '\0') {
            *dest = '\0';
            return;
        }
    }
}

/* Source: CoDUOMP.exe 0x00482e70..0x00482ecc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482e70_00482ecd.mcode. */
uint16_t Scr_CreateCanonicalFilename(const char *filename)
{
    char canonical[MAX_STRING_CHARS];

    CreateCanonicalFilename(canonical, filename, MAX_STRING_CHARS);
    return SL_GetString_(canonical, 0, SCRIPT_STRING_CANONICAL_FILENAME_TYPE);
}

/* Windows source: CoDUOMP.exe 0x004828e0..0x0048290c.
 * Linux source: coduo_lnxded 0x080a4d9a..0x080a4dc5. */
#if defined(WINDOWS_BEHAVIOR)
uint16_t SL_GetString(const char *text, int32_t user)
{
    return SL_GetString_(text, user, SCRIPT_STRING_DEFAULT_TYPE);
}
#else
uint16_t SL_GetString(const char *text, uint8_t user)
{
    return SL_GetString_(text, user, SCRIPT_STRING_DEFAULT_TYPE);
}
#endif

/* Windows source: CoDUOMP.exe 0x004829b0..0x004829dc.
 * Linux source immediately follows SL_GetLowercaseString_. */
#if defined(WINDOWS_BEHAVIOR)
uint16_t SL_GetLowercaseString(const char *text, int32_t user)
{
    return SL_GetLowercaseString_(text, user, SCRIPT_STRING_DEFAULT_TYPE);
}
#else
uint16_t SL_GetLowercaseString(const char *text, uint8_t user)
{
    return SL_GetLowercaseString_(text, user, SCRIPT_STRING_DEFAULT_TYPE);
}
#endif

/* Source: CoDUOMP.exe 0x00482be0..0x00482c09.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482be0_00482c0a.mcode. */
uint16_t Scr_AllocString(const char *text)
{
    return SL_GetString(text, SCRIPT_STRING_USAGE_RUNTIME);
}

/* Source: CoDUOMP.exe 0x00482c10..0x00482c73.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482c10_00482c74.mcode. */
uint16_t SL_GetStringForFloat(float value)
{
    char text[SCRIPT_STRING_FORMAT_BUFFER_SIZE];

    sprintf(text, "%g", (double)value);
    return SL_GetString_(text, 0, SCRIPT_STRING_RUNTIME_TYPE);
}

/* Source: CoDUOMP.exe 0x00482c80..0x00482cde.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482c80_00482cdf.mcode. */
uint16_t SL_GetStringForInt(int32_t value)
{
    char text[SCRIPT_STRING_FORMAT_BUFFER_SIZE];

    sprintf(text, "%i", value);
    return SL_GetString_(text, 0, SCRIPT_STRING_RUNTIME_TYPE);
}

/* Source: CoDUOMP.exe 0x00482ce0..0x00482d53.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482ce0_00482d54.mcode. */
uint16_t SL_GetStringForVector(const float *vector)
{
    char text[SCRIPT_VECTOR_FORMAT_BUFFER_SIZE];

    /* NOT_FROM_ORIGINAL_SOURCE: the fixed destination covers the textual form
     * of three finite float components; formatting remains bounded. */
    Com_sprintf(text, sizeof(text), "(%.2f, %.2f, %.2f)", (double)vector[0], (double)vector[1], (double)vector[2]);
    return SL_GetString_(text, 0, SCRIPT_STRING_RUNTIME_TYPE);
}

/* Source: CoDUOMP.exe 0x0047ff30..0x0047ff57.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047ff30_0047ff58.mcode. */
void SL_BeginLoadScripts(void)
{
    Hunk_SetHighTempMark();
    script_stringCanonicalMap = SCRIPT_HUNK_ALLOC(SCRIPT_STRING_TEMP_HASH_ENTRY_COUNT * sizeof(script_stringCanonicalMap[0]));
    script_stringCanonicalCount = 0;
}

/* Source: CoDUOMP.exe 0x0047ff60..0x0047ff9b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047ff60_0047ff9c.mcode. */
void SL_EndLoadScripts(void)
{
    Hunk_ClearToHighTempMark();
}

/* Source: CoDUOMP.exe 0x0047ffa0..0x0047ffe2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047ffa0_0047ffe3.mcode. */
uint16_t SL_TransferToCanonicalString(uint16_t handle)
{
    coduomp_script_string_mark_usage(handle, 2);

    uint16_t canonical = script_stringCanonicalMap[handle];
    if (canonical == 0) {
        script_stringCanonicalCount++;
        script_stringCanonicalMap[handle] = script_stringCanonicalCount;
        canonical = script_stringCanonicalCount;
    }

    return canonical;
}

/* Source: CoDUOMP.exe 0x0047fff0..0x00480018.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047fff0_00480019.mcode. */
uint16_t SL_FindCanonicalString(const char *text)
{
    uint16_t hash = SL_FindString(text);

    return script_stringCanonicalMap[hash];
}
