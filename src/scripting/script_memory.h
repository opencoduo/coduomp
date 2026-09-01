#ifndef SHARED_SCRIPT_MEMORY_H
#define SHARED_SCRIPT_MEMORY_H

#include "qcommon/script_runtime_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    SCRIPT_MEMORY_LOOKUP_COUNT = 256,
    SCRIPT_MEMORY_MAX_BUCKET = 16,
    SCRIPT_MEMORY_BUCKET_COUNT = SCRIPT_MEMORY_MAX_BUCKET + 1,
    SCRIPT_MEMORY_MAX_ALLOCATION_SIZE = 65536
};

extern uint8_t *script_stringPoolBase;
extern uint8_t *script_vectorLocalPoolBase;
extern uint8_t *script_memoryArenaEnd;
extern size_t script_vectorLocalPoolByteCount;
extern script_memory_block_t script_memoryBlocks[SCRIPT_MEMORY_BLOCK_COUNT];
extern uint8_t script_memoryBitWidthTable[SCRIPT_MEMORY_LOOKUP_COUNT];
extern uint8_t script_memoryPopcountTable[SCRIPT_MEMORY_LOOKUP_COUNT];
extern uint8_t script_memoryTrailingZeroBits[SCRIPT_MEMORY_LOOKUP_COUNT];
extern uint16_t script_memoryFreeRoots[SCRIPT_MEMORY_BUCKET_COUNT];
extern int32_t script_memoryAllocatedBucketCount;
extern int32_t script_memoryAllocatedInstanceCount;

int32_t Scr_GetStringUsage(void);
void MT_InitBits(void);
int32_t MT_GetSubTreeSize(uint16_t blockIndex);
void MT_DumpTree(void);
int32_t MT_GetScore(uint16_t blockIndex);
void MT_AddMemoryNode(uint16_t blockIndex, int32_t bucket);
qboolean MT_RemoveMemoryNode(uint16_t blockIndex, int32_t bucket);
void MT_RemoveHeadMemoryNode(int32_t bucket);
void MT_Init(void);
qboolean MT_Realloc(size_t lhsSize, size_t rhsSize);
int32_t MT_GetSize(int32_t size);
void MT_Error(const char *operation, size_t size);

/* The retail Windows allocator ignores allocation tags and exposes one-argument
 * entry points.  The retail Linux allocator receives the tag as a second
 * cdecl argument but likewise does not inspect it. */
#if defined(WINDOWS_BEHAVIOR)
uint16_t MT_AllocIndex(size_t size);
void *MT_Alloc(size_t size);
#else
uint16_t MT_AllocIndex(size_t size, int32_t type);
void *MT_Alloc(size_t size, int32_t type);
#endif

void MT_FreeIndex(uint16_t blockIndex, size_t size);
void MT_Free(void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif
