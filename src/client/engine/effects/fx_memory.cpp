#include "fx_memory.h"

#include "fx_archive.h"
#include "fx_bolt.h"

#include "../scripting/script_runtime.h"

/* Original storage at 0x00b8d548: 64 contiguous 32-KiB effect blocks. */
fx_mem_block_t fxMemBlocks[FX_MEM_BLOCK_COUNT];
int32_t fxMemHighWaterBytes; /* original 0x0389fe78 */

/* NOT_FROM_ORIGINAL_SOURCE: retain the fixed arena's target 0x8000-byte
 * block selection without subtracting pointers from distinct storage-array
 * subobjects on native 64-bit hosts. */
static fx_mem_block_t *coduomp_fx_mem_block_for_element(const void *element)
{
    const uintptr_t arenaBase =
        (uintptr_t)(const void *)&fxMemBlocks[0];
    const uintptr_t elementAddress =
        (uintptr_t)element;
    const uintptr_t blockOffset =
        (elementAddress - arenaBase) &
        ~(uintptr_t)(FX_MEM_BLOCK_SIZE - 1u);

    return (fx_mem_block_t *)(void *)(arenaBase + blockOffset);
}

/* Original descriptors 0x0389ff90 and 0x0389ffa8 are zero in the PE image.
 * Their compiler-generated startup functions at 0x00584f30 and 0x00584f00
 * initialize these exact payload-capacity quotients and null block heads.
 * PE_ZERO_WITH_RUNTIME_INITIALIZER */
fx_pool_allocator_t fxModelAllocator = { /* original 0x0389ff90 */
    (int32_t)(sizeof(fxMemBlocks[0].storage) /
              sizeof(fx_model_registration_t)),
    NULL
};
fx_pool_allocator_t fxBoltFrameAllocator = { /* original 0x0389ffa8 */
    (int32_t)(sizeof(fxMemBlocks[0].storage) /
              sizeof(cfx_bolt_frame_t)),
    NULL
}; /* PE_ZERO_WITH_RUNTIME_INITIALIZER */

#if UINTPTR_MAX == UINT32_MAX
static_assert(MAX_QPATH != 64 ||
                  sizeof(fxMemBlocks[0].storage) /
                          sizeof(fx_model_registration_t) == 204,
              "i386 FX model pool capacity changed");
static_assert(sizeof(fxMemBlocks[0].storage) /
                  sizeof(cfx_bolt_frame_t) == 481,
              "i386 FX bolt-frame pool capacity changed");
#endif

static_assert(sizeof(fx_model_registration_t) <=
                  sizeof(fxMemBlocks[0].storage),
              "FX model registration no longer fits an arena block");

/* Source: CoDUOMP.exe 0x004a05b0..0x004a05d2.
 * Name: same-module Mac symbol FxMem_Init. */
void FxMem_Init(void)
{
    for (int32_t blockIndex = 0;
         blockIndex < FX_MEM_BLOCK_COUNT;
         ++blockIndex) {
        fxMemBlocks[blockIndex].freeCount = FX_MEM_BLOCK_UNUSED;
    }
}

/* The Windows CRT initializer at CoDUOMP.exe
 * RVA 0x00184ed0..0x00184ef2 is a byte-for-byte inlined copy of FxMem_Init.
 * The same-module Mac translation unit is named FxMemMgr.cpp, and its
 * __sinit_/FxMemMgr_cpp at
 * 0x1000f320..0x1000f33f calls FxMem_Init directly.  The initializer's
 * source-level variable name and scalar type are not retained in either
 * binary; this TU-scope comma initializer is the minimal C++ representation
 * of the proven original static-initialization dependency.
 * ORIGINAL_STATIC_INITIALIZER_NO_STORAGE */
static const int32_t fxMemInitialized = (FxMem_Init(), 0);

/* Source: CoDUOMP.exe 0x004a0610..0x004a06a8.
 * Name: same-module Mac symbol FxMem_ClaimBlock. */
fx_mem_block_t *FxMem_ClaimBlock(fx_mem_block_t *previous,
                                 int32_t elementCount,
                                 size_t elementSize)
{
    fx_mem_block_t *block = NULL;
    int32_t blockIndex;
    for (blockIndex = 0; blockIndex < FX_MEM_BLOCK_COUNT; ++blockIndex) {
        if (fxMemBlocks[blockIndex].freeCount < 0) {
            block = &fxMemBlocks[blockIndex];
            break;
        }
    }

    if (block == NULL) {
        Com_DPrintf("^1Out of effects memory!\n");
        return NULL;
    }

    /* 0x004a0626 advances the scan extent before 0x004a064f compares it
     * with the high-water global, so block zero records 0x8000 bytes. */
    const int32_t claimedBytes = (blockIndex + 1) * FX_MEM_BLOCK_SIZE;
    if (fxMemHighWaterBytes < claimedBytes) {
        fxMemHighWaterBytes = claimedBytes;
    }

    block->freeCount = elementCount;
    uint8_t *element = block->storage;
    /* 0x004a0665 decrements the target dword before its signed-positive
     * test; -fwrapv retains that ordering for every input bit pattern. */
    const int32_t remainingLinks = elementCount - 1;
    if (remainingLinks > 0) {
        for (int32_t linkIndex = 0;
             linkIndex < remainingLinks;
             ++linkIndex) {
            uint8_t *next = element + elementSize;
            *(uint8_t **)element = next;
            element = next;
        }
    }
    *(uint8_t **)element = NULL;
    block->freeList = block->storage;

    if (previous != NULL) {
        previous->next = block;
    }
    block->previous = previous;
    block->next = NULL;
    return block;
}

/* Source: CoDUOMP.exe 0x004a06b0..0x004a06ec.
 * Name: same-module Mac symbol FxMem_UnlinkBlock. */
void FxMem_UnlinkBlock(fx_mem_block_t *block)
{
    if (block->next != NULL) {
        block->next->previous = block->previous;
    }
    if (block->previous != NULL) {
        block->previous->next = block->next;
    }
    block->next = NULL;
    block->previous = NULL;
}

/* Source: CoDUOMP.exe 0x004a06f0..0x004a0736.
 * Name: same-module Mac symbol FxMem_ReleaseBlock. */
void FxMem_ReleaseBlock(fx_mem_block_t *block)
{
    FxMem_UnlinkBlock(block);
    block->freeCount = FX_MEM_BLOCK_UNUSED;
}

/* Source: CoDUOMP.exe 0x004a0740..0x004a0765.
 * Name: same-module Mac symbol FxMem_FreeElem.  elementSize is part of the
 * original public signature but the free path does not consume it. */
void FxMem_FreeElem(uint8_t *element, size_t elementSize)
{
    (void)elementSize;
    fx_mem_block_t *block =
        coduomp_fx_mem_block_for_element(element);

    *(uint8_t **)element = block->freeList;
    block->freeList = element;
    ++block->freeCount;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring shared by the original
 * fixed-size allocator template instantiations at 0x004a9750
 * (CPrimitiveTemplate), 0x004a98c0 (scheduled effects), 0x004b2370 (CQuad),
 * 0x004b24b0 (CLight/CFlash), 0x004b2580 (CParticle), 0x004b2650 (CLine),
 * 0x004b2720 (CElectricity), 0x004b27f0 (COrientedParticle), 0x004b28c0
 * (CTail), 0x004b2990 (CCylinder), 0x004b2a60 (CEmitter), and 0x004b2b30
 * (CDecal). The class operator-new adapters remain authored source; these
 * repeated bodies are compiler template emissions. */
void *coduomp_fx_mem_alloc_from_pool(fx_pool_allocator_t *allocator,
                                     size_t size)
{
    fx_mem_block_t *block = allocator->blocks;
    while (block != NULL && block->freeList == NULL) {
        block = block->previous;
    }

    if (block == NULL) {
        block = FxMem_ClaimBlock(allocator->blocks,
                                 allocator->elementsPerBlock, size);
        if (block == NULL) {
            return NULL;
        }
        allocator->blocks = block;
    } else if (block != allocator->blocks) {
        FxMem_UnlinkBlock(block);
        block->previous = allocator->blocks;
        allocator->blocks->next = block;
        allocator->blocks = block;
    }

    uint8_t *element = block->freeList;
    block->freeList = *(uint8_t **)element;
    --block->freeCount;
    Com_Memset(element, 0, size);
    return element;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring shared by the original
 * fixed-size deallocator template instantiations.  The compiler emitted one
 * identical specialization for each FX pool at 0x004a5130, 0x004a52a0,
 * 0x004a5340..0x004a597c, 0x004a9820, 0x004a9990, and the separately emitted
 * CQuad specialization at 0x004b2440, plus identical block-to-front helpers
 * at 0x004a5980..0x004a5df7, 0x004a9c40, 0x004a9ca0, and 0x004b2c00.  The
 * individual class operator-delete adapters retain their own source ranges in
 * fx_primitive_memory.cpp; the repeated template bodies are classified as
 * compiler instantiations. */
void coduomp_fx_mem_free_from_pool(fx_pool_allocator_t *allocator,
                                   void *allocation)
{
    uint8_t *element = static_cast<uint8_t *>(allocation);
    fx_mem_block_t *block =
        coduomp_fx_mem_block_for_element(element);

    *(uint8_t **)element = block->freeList;
    block->freeList = element;
    ++block->freeCount;

    if (allocator->blocks->freeList == NULL) {
        FxMem_UnlinkBlock(block);
        block->previous = allocator->blocks;
        allocator->blocks->next = block;
        allocator->blocks = block;
        return;
    }

    if (block->freeCount < allocator->elementsPerBlock) {
        return;
    }
    if (block == allocator->blocks) {
        fx_mem_block_t *replacement = block->previous;
        if (replacement == NULL || replacement->freeCount == 0) {
            return;
        }
        allocator->blocks = replacement;
    }
    FxMem_ReleaseBlock(block);
}

/* Source: CoDUOMP.exe 0x004a51d0..0x004a529a.  This is the model-sized
 * instantiation of the original fixed-pool allocation template. */
void *FxMem_AllocModel(fx_pool_allocator_t *allocator, size_t size)
{
    return coduomp_fx_mem_alloc_from_pool(allocator, size);
}

/* Source: CoDUOMP.exe 0x004a52a0..0x004a530c.  This is the corresponding
 * model-sized fixed-pool deallocation template instantiation. */
void FxMem_FreeModel(fx_pool_allocator_t *allocator, void *element)
{
    coduomp_fx_mem_free_from_pool(allocator, element);
}

/* Source: CoDUOMP.exe 0x004a5060..0x004a512a.  Bolt-frame-sized fixed-pool
 * allocation template instantiation. */
void *FxMem_AllocBoltFrame(fx_pool_allocator_t *allocator, size_t size)
{
    return coduomp_fx_mem_alloc_from_pool(allocator, size);
}

/* Source: CoDUOMP.exe 0x004a5130..0x004a519c.  Bolt-frame-sized fixed-pool
 * deallocation template instantiation. */
void FxMem_FreeBoltFrame(fx_pool_allocator_t *allocator, void *element)
{
    coduomp_fx_mem_free_from_pool(allocator, element);
}

/* Source: CoDUOMP.exe 0x004a0860..0x004a086b.
 * Name: same-module Mac symbol FxModelAlloc.  The model-sized pool allocator
 * instantiated at 0x004a51d0 remains a separate original function. */
void *FxModelAlloc(int32_t size)
{
    return FxMem_AllocModel(&fxModelAllocator, (size_t)size);
}
