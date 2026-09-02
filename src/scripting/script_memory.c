#include "script_memory.h"
#include "script_runtime_host.h"

#include <string.h>

/* The retained allocator is one common subsystem in CoDUOMP.exe
 * 0x00480650..0x00480fc5 and coduo_lnxded 0x080a2708..0x080a2fb2.
 * The platform-gated public signatures below preserve the extra, unused
 * allocation-tag argument in the Linux source interface. */

enum {
    SCRIPT_MEMORY_TARGET_SHIFT_MASK = 31
};

/* NOT_FROM_ORIGINAL_SOURCE: retain the target dword bit pattern when a native
 * size or wrapping allocator counter enters a signed original operation. */
static int32_t coduomp_script_memory_int32_from_bits(uint32_t bits)
{
    int32_t value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: narrow a native allocation size to the target
 * dword before giving it the original signed interpretation. */
static int32_t coduomp_script_memory_int32_from_size(size_t size)
{
    return coduomp_script_memory_int32_from_bits((uint32_t)size);
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve the retail byte access for zero or
 * negative sizes without granting the optimizer in-bounds array provenance. */
static uint8_t coduomp_script_memory_read_width(int32_t index)
{
    return *(const uint8_t *)((uintptr_t)(const void *)&script_memoryBitWidthTable[0] + (intptr_t)index);
}

/* Source: CoDUOMP.exe 0x00480750..0x00480755, recovered from an exporter
 * gap. Name and result: exact same-module Mac symbol Scr_GetStringUsage. */
int32_t Scr_GetStringUsage(void)
{
    return script_memoryAllocatedBucketCount;
}

/* Source: CoDUOMP.exe 0x00480760..0x004807c5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00480760_004807c6.mcode. */
void MT_InitBits(void)
{
    for (uint32_t value = 0; value < SCRIPT_MEMORY_LOOKUP_COUNT; ++value) {
        uint8_t popcount = 0;
        for (uint32_t bits = value; bits != 0; bits >>= 1) {
            if ((bits & 1U) != 0) {
                ++popcount;
            }
        }
        script_memoryPopcountTable[value] = popcount;

        uint8_t trailingZeros = 8;
        while ((((1U << trailingZeros) - 1U) & value) != 0) {
            --trailingZeros;
        }
        script_memoryTrailingZeroBits[value] = trailingZeros;

        uint8_t bitWidth = 0;
        for (uint32_t bits = value; bits != 0; bits >>= 1) {
            ++bitWidth;
        }
        script_memoryBitWidthTable[value] = bitWidth;
    }
}

/* Source: CoDUOMP.exe 0x00480650..0x00480685.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00480650_00480686.mcode. */
int32_t MT_GetSubTreeSize(uint16_t blockIndex)
{
    if (blockIndex == 0) {
        return 0;
    }

    script_memory_block_t *block = &script_memoryBlocks[blockIndex];
    int32_t rightCount = MT_GetSubTreeSize(block->freeNode.right);
    int32_t leftCount = MT_GetSubTreeSize(block->freeNode.left);
    return rightCount + leftCount + 1;
}

/* Source: CoDUOMP.exe 0x00480690..0x00480748.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00480690_00480749.mcode. */
void MT_DumpTree(void)
{
    Com_Printf("********************************\n");
    for (int32_t bucket = 0; bucket <= SCRIPT_MEMORY_MAX_BUCKET; ++bucket) {
        int32_t freeCount = MT_GetSubTreeSize(script_memoryFreeRoots[bucket]);
        int32_t bucketSize = 1 << bucket;

        Com_Printf("%d subtree has %d * %d = %d free buckets\n", bucket, freeCount, bucketSize, freeCount * bucketSize);
    }
    Com_Printf("********************************\n");
    Com_Printf("********************************\n");
    Com_Printf("total memory alloc buckets: %d (%d instances)\n", script_memoryAllocatedBucketCount, script_memoryAllocatedInstanceCount);
    Com_Printf("total memory free buckets: %d\n", 65535 - script_memoryAllocatedBucketCount);
    Com_Printf("********************************\n");
}

/* Source: CoDUOMP.exe 0x004807d0..0x00480812.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004807d0_00480813.mcode. */
int32_t MT_GetScore(uint16_t blockIndex)
{
    uint32_t distance = 65536U - blockIndex;
    uint32_t lowByte = distance & 255U;
    uint32_t highByte = (distance >> 8) & 255U;
    uint8_t trailingZeros = script_memoryTrailingZeroBits[lowByte];

    if (lowByte == 0) {
        trailingZeros += script_memoryTrailingZeroBits[highByte];
    }

    return (1 << trailingZeros) + (int32_t)(distance - script_memoryPopcountTable[lowByte] - script_memoryPopcountTable[highByte]);
}

/* Source: CoDUOMP.exe 0x00480820..0x00480953.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00480820_00480954.mcode. */
void MT_AddMemoryNode(uint16_t blockIndex, int32_t bucket)
{
    uint16_t *link = &script_memoryFreeRoots[bucket];
    uint16_t current = *link;

    if (current != 0) {
        int32_t blockKey = MT_GetScore(blockIndex);
        int32_t center = 0;
        int32_t span = 65536;

        do {
            int32_t currentKey = MT_GetScore(current);
            if (currentKey < blockKey) {
                for (;;) {
                    *link = blockIndex;
                    script_memoryBlocks[blockIndex] = script_memoryBlocks[current];
                    if (current == 0) {
                        return;
                    }

                    span >>= 1;
                    if ((int32_t)current < center) {
                        link = &script_memoryBlocks[blockIndex].freeNode.left;
                        center -= span;
                    } else {
                        link = &script_memoryBlocks[blockIndex].freeNode.right;
                        center += span;
                    }
                    blockIndex = current;
                    current = *link;
                }
            }

            span >>= 1;
            if ((int32_t)blockIndex < center) {
                link = &script_memoryBlocks[current].freeNode.left;
                center -= span;
            } else {
                link = &script_memoryBlocks[current].freeNode.right;
                center += span;
            }
            current = *link;
        } while (current != 0);
    }

    *link = blockIndex;
    script_memoryBlocks[blockIndex].freeNode.left = 0;
    script_memoryBlocks[blockIndex].freeNode.right = 0;
}

/* Source: CoDUOMP.exe 0x00480960..0x00480aeb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00480960_00480aec.mcode. */
qboolean MT_RemoveMemoryNode(uint16_t blockIndex, int32_t bucket)
{
    int32_t center = 0;
    int32_t span = 65536;
    uint16_t *link = &script_memoryFreeRoots[bucket];

    for (;;) {
        uint16_t current = *link;
        if (current == 0) {
            return qfalse;
        }
        if (blockIndex == current) {
            break;
        }
        if ((int32_t)blockIndex == center) {
            return qfalse;
        }

        span >>= 1;
        if ((int32_t)blockIndex < center) {
            link = &script_memoryBlocks[current].freeNode.left;
            center -= span;
        } else {
            link = &script_memoryBlocks[current].freeNode.right;
            center += span;
        }
    }

    script_memory_block_t replacement = script_memoryBlocks[blockIndex];
    for (;;) {
        uint16_t next;
        if (replacement.freeNode.left == 0) {
            next = replacement.freeNode.right;
            *link = next;
            if (next == 0) {
                return qtrue;
            }
            link = &script_memoryBlocks[next].freeNode.right;
        } else if (replacement.freeNode.right == 0) {
            next = replacement.freeNode.left;
            *link = next;
            link = &script_memoryBlocks[next].freeNode.left;
        } else if (MT_GetScore(replacement.freeNode.left) < MT_GetScore(replacement.freeNode.right)) {
            next = replacement.freeNode.right;
            *link = next;
            link = &script_memoryBlocks[next].freeNode.right;
        } else {
            next = replacement.freeNode.left;
            *link = next;
            link = &script_memoryBlocks[next].freeNode.left;
        }

        script_memory_block_t moved = script_memoryBlocks[next];
        script_memoryBlocks[next] = replacement;
        replacement = moved;
    }
}

/* Source: CoDUOMP.exe 0x00480af0..0x00480c55.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00480af0_00480c56.mcode. */
void MT_RemoveHeadMemoryNode(int32_t bucket)
{
    uint16_t *link = &script_memoryFreeRoots[bucket];
    script_memory_block_t replacement = script_memoryBlocks[*link];

    for (;;) {
        uint16_t next;
        if (replacement.freeNode.left == 0) {
            next = replacement.freeNode.right;
            *link = next;
            if (next == 0) {
                return;
            }
            link = &script_memoryBlocks[next].freeNode.right;
        } else if (replacement.freeNode.right == 0) {
            next = replacement.freeNode.left;
            *link = next;
            link = &script_memoryBlocks[next].freeNode.left;
        } else if (MT_GetScore(replacement.freeNode.left) < MT_GetScore(replacement.freeNode.right)) {
            next = replacement.freeNode.right;
            *link = next;
            link = &script_memoryBlocks[next].freeNode.right;
        } else {
            next = replacement.freeNode.left;
            *link = next;
            link = &script_memoryBlocks[next].freeNode.left;
        }

        script_memory_block_t moved = script_memoryBlocks[next];
        script_memoryBlocks[next] = replacement;
        replacement = moved;
    }
}

/* Source: CoDUOMP.exe 0x00480c60..0x00480cf8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00480c60_00480cf9.mcode. */
void MT_Init(void)
{
    script_stringPoolBase = (uint8_t *)script_memoryBlocks;
    MT_InitBits();
    for (int32_t bucket = 0; bucket <= SCRIPT_MEMORY_MAX_BUCKET; ++bucket) {
        script_memoryFreeRoots[bucket] = 0;
    }

    script_memoryBlocks[0].freeNode.left = 0;
    script_memoryBlocks[0].freeNode.right = 0;
    script_memoryArenaEnd = (uint8_t *)&script_memoryBlocks[SCRIPT_MEMORY_BLOCK_COUNT];
    script_vectorLocalPoolBase = script_memoryArenaEnd;
    script_vectorLocalPoolByteCount = 0;

    for (int32_t bucket = 0; bucket < SCRIPT_MEMORY_MAX_BUCKET; ++bucket) {
        MT_AddMemoryNode((uint16_t)(1U << bucket), bucket);
    }

    script_memoryAllocatedInstanceCount = 0;
    script_memoryAllocatedBucketCount = 0;
}

/* Source: CoDUOMP.exe 0x00480fd0..0x00481074.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00480fd0_00481075.mcode. */
qboolean MT_Realloc(size_t lhsSize, size_t rhsSize)
{
    return MT_GetSize(coduomp_script_memory_int32_from_size(lhsSize)) == MT_GetSize(coduomp_script_memory_int32_from_size(rhsSize))
               ? qtrue
               : qfalse;
}

/* Source: CoDUOMP.exe 0x00480d70..0x00480db1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00480d70_00480db2.mcode.
 * Name and signed-int argument: exact same-module Mac symbol MT_GetSize__Fi.
 * The Windows compiler also inlines this calculation into MT_AllocIndex and
 * MT_FreeIndex. */
int32_t MT_GetSize(int32_t size)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (size <= 0) {
        MT_Error("MT_GetSize: invalid allocation size", (size_t)size);
        return 0;
    }

    if (size >= SCRIPT_MEMORY_MAX_ALLOCATION_SIZE) {
        MT_Error("MT_GetSize: max allocation exceeded", (size_t)size);
        return 0;
    }

    int32_t blockCountMinusOne = (size + SCRIPT_MEMORY_BLOCK_SIZE - 1) / SCRIPT_MEMORY_BLOCK_SIZE - 1;
    if (blockCountMinusOne < SCRIPT_MEMORY_LOOKUP_COUNT) {
        return coduomp_script_memory_read_width(blockCountMinusOne);
    }
    return coduomp_script_memory_read_width(blockCountMinusOne >> 8) + 8;
}

/* Source: CoDUOMP.exe 0x00480d00..0x00480d66.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00480d00_00480d67.mcode. */
void MT_Error(const char *operation, size_t size)
{
    MT_DumpTree();
    Scr_TerminalError(va("%s: failed allocation of %d bytes for script usage", operation, coduomp_script_memory_int32_from_size(size)));
}

/* Source: CoDUOMP.exe 0x00480dc0..0x00480edc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00480dc0_00480edd.mcode. */
#if defined(WINDOWS_BEHAVIOR)
uint16_t MT_AllocIndex(size_t size)
#else
uint16_t MT_AllocIndex(size_t size, int32_t type)
#endif
{
#if !defined(WINDOWS_BEHAVIOR)
    (void)type;
#endif
    int32_t requestedBucket = MT_GetSize(coduomp_script_memory_int32_from_size(size));
    int32_t bucket = requestedBucket;

    while (bucket <= SCRIPT_MEMORY_MAX_BUCKET) {
        uint16_t blockIndex = script_memoryFreeRoots[bucket];
        if (blockIndex != 0) {
            MT_RemoveHeadMemoryNode(bucket);
            while (bucket != requestedBucket) {
                --bucket;
                MT_AddMemoryNode((uint16_t)(blockIndex + (1U << bucket)), bucket);
            }

            script_memoryAllocatedInstanceCount = coduomp_script_memory_int32_from_bits((uint32_t)script_memoryAllocatedInstanceCount + 1u);
            script_memoryAllocatedBucketCount = coduomp_script_memory_int32_from_bits(
                (uint32_t)script_memoryAllocatedBucketCount + (1u << ((uint32_t)requestedBucket & SCRIPT_MEMORY_TARGET_SHIFT_MASK)));
            return blockIndex;
        }
        ++bucket;
    }

    MT_Error("MT_AllocIndex", size);
    return 0;
}

/* Source: CoDUOMP.exe 0x00480ee0..0x00480f86.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00480ee0_00480f87.mcode. */
void MT_FreeIndex(uint16_t blockIndex, size_t size)
{
    int32_t bucket = MT_GetSize(coduomp_script_memory_int32_from_size(size));

    script_memoryAllocatedInstanceCount = coduomp_script_memory_int32_from_bits((uint32_t)script_memoryAllocatedInstanceCount - 1u);
    script_memoryAllocatedBucketCount = coduomp_script_memory_int32_from_bits((uint32_t)script_memoryAllocatedBucketCount -
                                                                              (1u << ((uint32_t)bucket & SCRIPT_MEMORY_TARGET_SHIFT_MASK)));

    while (bucket != SCRIPT_MEMORY_MAX_BUCKET) {
        uint32_t bucketMask = 1u << ((uint32_t)bucket & SCRIPT_MEMORY_TARGET_SHIFT_MASK);
        uint16_t buddy = (uint16_t)(blockIndex ^ bucketMask);
        if (MT_RemoveMemoryNode(buddy, bucket) == qfalse) {
            break;
        }

        blockIndex = (uint16_t)(blockIndex & ~bucketMask);
        ++bucket;
    }

    MT_AddMemoryNode(blockIndex, bucket);
}

/* Source: CoDUOMP.exe 0x00480f90..0x00480fa7 and coduo_lnxded
 * 0x080a2f68..0x080a2f8c.  Linux retains the unused allocation tag in the
 * source interface; Windows does not pass it. */
#if defined(WINDOWS_BEHAVIOR)
void *MT_Alloc(size_t size)
{
    uint16_t blockIndex = MT_AllocIndex(size);
#else
void *MT_Alloc(size_t size, int32_t type)
{
    uint16_t blockIndex = MT_AllocIndex(size, type);
#endif
    return &script_memoryBlocks[blockIndex];
}

/* Source: CoDUOMP.exe 0x00480fb0..0x00480fc5 and coduo_lnxded
 * 0x080a2f8e..0x080a2fb2. */
void MT_Free(void *ptr, size_t size)
{
    size_t byteOffset = (uint8_t *)ptr - (uint8_t *)script_memoryBlocks;

    MT_FreeIndex((uint16_t)(byteOffset / SCRIPT_MEMORY_BLOCK_SIZE), size);
}
