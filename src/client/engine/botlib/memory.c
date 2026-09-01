#include "memory.h"

#include "qcommon/hunk.h"
#include "../system_fatal.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BOTLIB_HEAP_ALLOCATION_MARKER UINT32_C(0x12345678)
#define BOTLIB_HUNK_ALLOCATION_MARKER UINT32_C(0x87654321)

enum {
    BOTLIB_HUNK_ALIGNMENT = 32
};

/* The original i386 allocator prepends one marker dword. Native 64-bit builds
 * widen that header to max_align_t so the returned payload remains suitable
 * for every ordinary host type instead of becoming four-byte aligned. */
#if UINTPTR_MAX == UINT32_MAX
typedef struct botlib_memory_header_s {
    uint32_t allocationMarker;
} botlib_memory_header_t;
#else
/* NOT_FROM_ORIGINAL_SOURCE: native-width alignment carrier for the portable
 * allocator. The original executable has no 64-bit form of this header. */
typedef union botlib_memory_header_u {
    uint32_t allocationMarker;
    max_align_t alignment;
} botlib_memory_header_t;
#endif

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(botlib_memory_header_t) == 4,
               "original botlib allocation header is four bytes");
_Static_assert(_Alignof(botlib_memory_header_t) == 4,
               "original botlib allocation header is dword-aligned");
_Static_assert(offsetof(botlib_memory_header_t, allocationMarker) == 0,
               "botlib allocation marker must precede the payload");
_Static_assert(sizeof(((botlib_memory_header_t *)0)->allocationMarker) == 4,
               "botlib allocation marker must remain one dword");
#endif

/* Source: CoDUOMP.exe 0x00442740..0x00442772.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00442740_00442773.mcode.
 * Name: exact same-module Mac symbol GetMemory. */
void *GetMemory(size_t size)
{
    const size_t allocationSize = sizeof(botlib_memory_header_t) + size;
    botlib_memory_header_t *allocation = malloc(allocationSize);
    if (allocation == NULL)
        Sys_OutOfMemory();

    memset(allocation, 0, allocationSize);
    allocation->allocationMarker = BOTLIB_HEAP_ALLOCATION_MARKER;
    return allocation + 1;
}

/* Source: CoDUOMP.exe 0x00442780..0x004427c6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00442780_004427c7.mcode.
 * Name: exact same-module Mac symbol GetClearedMemory. The second payload
 * clear is present after the inlined GetMemory body in the PE. */
void *GetClearedMemory(size_t size)
{
    void *memory = GetMemory(size);
    memset(memory, 0, size);
    return memory;
}

/* Source: CoDUOMP.exe 0x004427d0..0x004427eb.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004427d0_004427ec.mcode.
 * Role name: the botlib GetHunkMemory API. */
void *GetHunkMemory(size_t size)
{
    botlib_memory_header_t *allocation = Hunk_AllocAlignInternal(
        sizeof(botlib_memory_header_t) + size, BOTLIB_HUNK_ALIGNMENT);
    if (allocation == NULL)
        return NULL;

    allocation->allocationMarker = BOTLIB_HUNK_ALLOCATION_MARKER;
    return allocation + 1;
}

/* Source: CoDUOMP.exe 0x004427f0..0x00442828.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004427f0_00442829.mcode.
 * Role name: the botlib GetClearedHunkMemory API. */
void *GetClearedHunkMemory(size_t size)
{
    void *memory = GetHunkMemory(size);
    memset(memory, 0, size);
    return memory;
}

/* Source: CoDUOMP.exe 0x00442830..0x00442845.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00442830_00442846.mcode.
 * Name: exact same-module Mac symbol FreeMemory. A foreign or hunk allocation
 * is deliberately ignored rather than passed to the CRT heap. */
void FreeMemory(void *memory)
{
    botlib_memory_header_t *allocation =
        (botlib_memory_header_t *)memory - 1;
    if (allocation->allocationMarker == BOTLIB_HEAP_ALLOCATION_MARKER)
        free(allocation);
}

/* Source: CoDUOMP.exe 0x00442850. The non-MEMDEBUG botlib implementation is
 * an intentional no-op. */
void PrintUsedMemorySize(void)
{
}

/* Source: CoDUOMP.exe 0x00442860. The non-MEMDEBUG botlib implementation is
 * an intentional no-op. */
void PrintMemoryLabels(void)
{
}
