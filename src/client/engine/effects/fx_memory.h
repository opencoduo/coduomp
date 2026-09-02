#ifndef CODUOMP_FX_MEMORY_H
#define CODUOMP_FX_MEMORY_H

#include <stddef.h>
#include <stdint.h>

enum {
    FX_MEM_BLOCK_SIZE = 32768,
    FX_MEM_BLOCK_COUNT = 64,
    FX_MEM_BLOCK_UNUSED = -1
};

typedef struct fx_mem_block_s fx_mem_block_t;
typedef struct fx_pool_allocator_s fx_pool_allocator_t;

/* Original 0x8000-byte fixed-pool block.  FxMem_Init and FxMem_ClaimBlock
 * walk the 64-record array with a 0x8000 stride.  Claim/free and link
 * management prove the payload at +0x0000..+0x7fef, signed free count at
 * +0x7ff0, free-list head at +0x7ff4, and intrusive links at +0x7ff8/+0x7ffc. */
struct fx_mem_block_s {
    uint8_t storage[FX_MEM_BLOCK_SIZE - sizeof(int32_t) - 3 * sizeof(void *)];
    int32_t freeCount;
    uint8_t *freeList;
    fx_mem_block_t *previous;
    fx_mem_block_t *next;
};

/* Original fixed-pool descriptor.  Every emitted allocator specialization
 * reads the element capacity at +0x00 and the current block head at +0x04;
 * the compiler-generated global initializers also write those two dwords. */
struct fx_pool_allocator_s {
    int32_t elementsPerBlock;
    fx_mem_block_t *blocks;
};

#ifdef __cplusplus
#define CODUOMP_FX_STATIC_ASSERT static_assert
#define CODUOMP_FX_ALIGNOF alignof
#else
#define CODUOMP_FX_STATIC_ASSERT _Static_assert
#define CODUOMP_FX_ALIGNOF _Alignof
#endif

/* coduomp_fx_mem_block_for_element selects a block by masking a byte offset
 * within this array. Keep that native stride invariant explicit when host
 * pointers widen and the storage payload correspondingly shrinks. */
CODUOMP_FX_STATIC_ASSERT(sizeof(fx_mem_block_t) == FX_MEM_BLOCK_SIZE, "native FX block size changed");
CODUOMP_FX_STATIC_ASSERT(offsetof(fx_mem_block_t, storage) == 0, "native FX block storage offset changed");

#if UINTPTR_MAX == UINT32_MAX
CODUOMP_FX_STATIC_ASSERT(CODUOMP_FX_ALIGNOF(fx_mem_block_t) == 0x04, "i386 FX block alignment changed");
CODUOMP_FX_STATIC_ASSERT(offsetof(fx_mem_block_t, storage) == 0x0000, "i386 FX block storage offset changed");
CODUOMP_FX_STATIC_ASSERT(sizeof(((fx_mem_block_t *)0)->storage) == 0x7ff0, "i386 FX block storage extent changed");
CODUOMP_FX_STATIC_ASSERT(offsetof(fx_mem_block_t, freeCount) == 0x7ff0, "i386 FX block free-count offset changed");
CODUOMP_FX_STATIC_ASSERT(sizeof(((fx_mem_block_t *)0)->freeCount) == 0x04, "i386 FX block free-count extent changed");
CODUOMP_FX_STATIC_ASSERT(offsetof(fx_mem_block_t, freeList) == 0x7ff4, "i386 FX block free-list offset changed");
CODUOMP_FX_STATIC_ASSERT(sizeof(((fx_mem_block_t *)0)->freeList) == 0x04, "i386 FX block free-list extent changed");
CODUOMP_FX_STATIC_ASSERT(offsetof(fx_mem_block_t, previous) == 0x7ff8, "i386 FX block previous-link offset changed");
CODUOMP_FX_STATIC_ASSERT(sizeof(((fx_mem_block_t *)0)->previous) == 0x04, "i386 FX block previous-link extent changed");
CODUOMP_FX_STATIC_ASSERT(offsetof(fx_mem_block_t, next) == 0x7ffc, "i386 FX block next-link offset changed");
CODUOMP_FX_STATIC_ASSERT(sizeof(((fx_mem_block_t *)0)->next) == 0x04, "i386 FX block next-link extent changed");
CODUOMP_FX_STATIC_ASSERT(sizeof(fx_mem_block_t) == 0x8000, "i386 FX block size changed");
CODUOMP_FX_STATIC_ASSERT(CODUOMP_FX_ALIGNOF(fx_pool_allocator_t) == 0x04, "i386 FX pool descriptor alignment changed");
CODUOMP_FX_STATIC_ASSERT(offsetof(fx_pool_allocator_t, elementsPerBlock) == 0x00, "i386 FX pool element-capacity offset changed");
CODUOMP_FX_STATIC_ASSERT(sizeof(((fx_pool_allocator_t *)0)->elementsPerBlock) == 0x04, "i386 FX pool element-capacity extent changed");
CODUOMP_FX_STATIC_ASSERT(offsetof(fx_pool_allocator_t, blocks) == 0x04, "i386 FX pool block-head offset changed");
CODUOMP_FX_STATIC_ASSERT(sizeof(((fx_pool_allocator_t *)0)->blocks) == 0x04, "i386 FX pool block-head extent changed");
CODUOMP_FX_STATIC_ASSERT(sizeof(fx_pool_allocator_t) == 0x08, "i386 FX pool descriptor size changed");
#endif
#undef CODUOMP_FX_ALIGNOF
#undef CODUOMP_FX_STATIC_ASSERT

extern fx_mem_block_t fxMemBlocks[FX_MEM_BLOCK_COUNT];
/* Peak byte extent claimed from the arena base (0x8000 for block 0). */
extern int32_t fxMemHighWaterBytes;
extern fx_pool_allocator_t fxModelAllocator;
extern fx_pool_allocator_t fxBoltFrameAllocator;

#ifdef __cplusplus
extern "C" {
#endif

void FxMem_Init(void);
fx_mem_block_t *FxMem_ClaimBlock(fx_mem_block_t *previous, int32_t elementCount, size_t elementSize);
void FxMem_UnlinkBlock(fx_mem_block_t *block);
void FxMem_ReleaseBlock(fx_mem_block_t *block);
void FxMem_FreeElem(uint8_t *element, size_t elementSize);
void *FxMem_AllocModel(fx_pool_allocator_t *allocator, size_t size);
void FxMem_FreeModel(fx_pool_allocator_t *allocator, void *element);
void *FxMem_AllocBoltFrame(fx_pool_allocator_t *allocator, size_t size);
void FxMem_FreeBoltFrame(fx_pool_allocator_t *allocator, void *element);
void *FxModelAlloc(int32_t size);
/* NOT_FROM_ORIGINAL_SOURCE: portable source-level entry shared by the
 * original class-specific fixed-pool allocation template instantiations. */
void *coduomp_fx_mem_alloc_from_pool(fx_pool_allocator_t *allocator, size_t size);
/* NOT_FROM_ORIGINAL_SOURCE: portable source-level entry shared by the
 * original class-specific fixed-pool template instantiations. */
void coduomp_fx_mem_free_from_pool(fx_pool_allocator_t *allocator, void *element);

#ifdef __cplusplus
}
#endif

#endif
