#include "hunk.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void Com_Error(errorParm_t code, const char *format, ...);
void Sys_OutOfMemory(void);

/*
 * Complete common hunk allocation cluster.  The Windows client and Linux
 * dedicated engine use the same cursor arithmetic, alignment, zeroing,
 * temporary header, and release behavior:
 *
 *   CoDUOMP.exe   0x00435d60..0x004360c5
 *   coduo_lnxded  0x0806c3d4..0x0806c7de
 *
 * The Linux fallback allocation/free callees at 0x0806bb38 and 0x0806bb25
 * are exactly malloc + out-of-memory handling + zeroing and free.  Keeping
 * those operations here therefore does not introduce a client/server split.
 * Canonical names follow the exact same-module Windows/Mac symbols; the
 * former Linux Hunk_Alloc* spellings described these same functions.
 */

/* NOT_FROM_ORIGINAL_SOURCE: compare the two cursor extents by subtraction so
 * their sum never has to be represented. */
#define HUNK_USAGE_EXCEEDS_TOTAL(first, second) ((first) > hunk.totalSize || (second) > hunk.totalSize - (first))

/* NOT_FROM_ORIGINAL_SOURCE: overflow-safe spelling of the allocator's
 * power-of-two alignment operation. */
static qboolean coduo_compat_hunk_align_up(size_t value, size_t alignment, size_t limit, size_t *alignedOut)
{
    if (alignment == 0 || (alignment & (alignment - 1u)) != 0 || value > limit)
        return qfalse;

    const size_t mask = alignment - 1u;
    const size_t padding = (alignment - (value & mask)) & mask;
    if (padding > limit - value)
        return qfalse;

    *alignedOut = value + padding;
    return qtrue;
}

/* CoDUOMP.exe 0x00435d60; coduo_lnxded 0x0806c3d4. */
void *Hunk_AllocInternal(size_t size)
{
    return Hunk_AllocAlignInternal(size, HUNK_ALIGNMENT);
}

/* CoDUOMP.exe 0x00435d70; coduo_lnxded 0x0806c3ef. */
void *Hunk_AllocAlignInternal(size_t size, size_t alignment)
{
    size_t newHighUsed;
    uint8_t *allocation;

    if (hunk_data == NULL) {
        Com_Error(ERR_FATAL, "\x15"
                             "Hunk_AllocAlign: Hunk memory system not initialized");
    }

    if (hunk.highUsed > hunk.totalSize || size > hunk.totalSize - hunk.highUsed ||
        coduo_compat_hunk_align_up(hunk.highUsed + size, alignment, hunk.totalSize, &newHighUsed) == qfalse ||
        HUNK_USAGE_EXCEEDS_TOTAL(newHighUsed, hunk.lowTemp)) {
        Com_Meminfo_f();
        Com_Error(ERR_DROP,
                  "\x15"
                  "Hunk_AllocAlign failed on %i",
                  (int32_t)size);
        return NULL;
    }

    allocation = hunk_data + hunk.totalSize - newHighUsed;
    hunk.highUsed = newHighUsed;
    hunk.highTemp = newHighUsed;

    hunk_used = (int32_t)(hunk.highUsed + hunk.lowUsed);
    memset(allocation, 0, size);
    return allocation;
}

/* CoDUOMP.exe 0x00435e10; coduo_lnxded 0x0806c4b7. */
void *Hunk_AllocateTempMemoryHighInternal(size_t size)
{
    size_t newHighTemp;

    if (hunk.highTemp > hunk.totalSize || size > hunk.totalSize - hunk.highTemp ||
        coduo_compat_hunk_align_up(hunk.highTemp + size, HUNK_TEMP_ALIGNMENT, hunk.totalSize, &newHighTemp) == qfalse ||
        HUNK_USAGE_EXCEEDS_TOTAL(newHighTemp, hunk.lowTemp)) {
        Com_Meminfo_f();
        Com_Error(ERR_DROP,
                  "\x15"
                  "Hunk_AllocateTempMemoryHigh: failed on %i",
                  (int32_t)size);
        return NULL;
    }

    hunk.highTemp = newHighTemp;

    return hunk_data + hunk.totalSize - hunk.highTemp;
}

/* CoDUOMP.exe 0x00435e60; coduo_lnxded 0x0806c522. */
void Hunk_ClearTempMemoryHigh(void)
{
    hunk.highTemp = hunk.highUsed;
}

/* CoDUOMP.exe 0x00435e70; coduo_lnxded 0x0806c531. */
void *Hunk_AllocLowInternal(size_t size)
{
    return Hunk_AllocLowAlignInternal(size, HUNK_ALIGNMENT);
}

/* CoDUOMP.exe 0x00435e80; coduo_lnxded 0x0806c54c. */
void *Hunk_AllocLowAlignInternal(size_t size, size_t alignment)
{
    size_t alignedLowUsed;
    size_t newLowUsed;
    uint8_t *allocation;

    if (coduo_compat_hunk_align_up(hunk.lowUsed, alignment, hunk.totalSize, &alignedLowUsed) == qfalse ||
        size > hunk.totalSize - alignedLowUsed) {
        Com_Meminfo_f();
        Com_Error(ERR_DROP,
                  "\x15"
                  "Hunk_AllocLowAlign failed on %i",
                  (int32_t)size);
        return NULL;
    }

    newLowUsed = alignedLowUsed + size;
    if (HUNK_USAGE_EXCEEDS_TOTAL(hunk.highTemp, newLowUsed)) {
        Com_Meminfo_f();
        Com_Error(ERR_DROP,
                  "\x15"
                  "Hunk_AllocLowAlign failed on %i",
                  (int32_t)size);
        return NULL;
    }

    allocation = hunk_data + alignedLowUsed;
    hunk.lowUsed = newLowUsed;
    hunk.lowTemp = newLowUsed;
    hunk_used = (int32_t)(hunk.highUsed + hunk.lowUsed);
    memset(allocation, 0, size);
    return allocation;
}

/* CoDUOMP.exe 0x0043d350; coduo_lnxded 0x08072a66.  Exact same-module Mac
 * symbol: Hunk_AllocXAnimCreateTree. */
void *Hunk_AllocXAnimCreateTree(size_t size)
{
    return Hunk_AllocLowAlignInternal(size, HUNK_ALIGNMENT);
}

/* CoDUOMP.exe 0x0043d360; coduo_lnxded 0x08072a79.  Exact same-module Mac
 * symbol: Hunk_AllocXModelPrecache. */
void *Hunk_AllocXModelPrecache(size_t size)
{
    return Hunk_AllocInternal(size);
}

/* CoDUOMP.exe 0x0043d370; coduo_lnxded 0x08072a8c.  Exact same-module Mac
 * symbol: Hunk_AllocXModelPrecacheMesh. */
void *Hunk_AllocXModelPrecacheMesh(size_t size)
{
    return Hunk_AllocInternal(size);
}

/* CoDUOMP.exe 0x00435f00; coduo_lnxded 0x0806c5f1. */
void Hunk_CommitTempMemory(void)
{
    hunk.lowUsed = hunk.lowTemp;
}

/* CoDUOMP.exe 0x00435f10; coduo_lnxded 0x0806c600. */
void *Hunk_AllocateTempMemoryInternal(size_t size)
{
    size_t oldLowTemp;
    size_t alignedLowTemp;
    size_t allocationBytes;
    hunk_temp_header_t *header;

    if (hunk_data == NULL) {
        void *const allocation = malloc(size);
        if (allocation == NULL) {
            Sys_OutOfMemory();
        }
        memset(allocation, 0, size);
        return allocation;
    }

    oldLowTemp = hunk.lowTemp;
    if (coduo_compat_hunk_align_up(oldLowTemp, HUNK_TEMP_ALIGNMENT, hunk.totalSize, &alignedLowTemp) == qfalse ||
        sizeof(*header) > hunk.totalSize - alignedLowTemp || size > hunk.totalSize - alignedLowTemp - sizeof(*header)) {
        Com_Meminfo_f();
        Com_Error(ERR_DROP,
                  "\x15"
                  "Hunk_AllocateTempMemory: failed on %i",
                  (int32_t)size);
        return NULL;
    }

    allocationBytes = sizeof(*header) + size;
    const size_t newLowTemp = alignedLowTemp + allocationBytes;
    if (HUNK_USAGE_EXCEEDS_TOTAL(hunk.highTemp, newLowTemp)) {
        Com_Meminfo_f();
        Com_Error(ERR_DROP,
                  "\x15"
                  "Hunk_AllocateTempMemory: failed on %i, needs %i",
                  (int32_t)allocationBytes, (int32_t)(hunk.highTemp - (hunk.totalSize - newLowTemp)));
        return NULL;
    }

    header = (hunk_temp_header_t *)(hunk_data + alignedLowTemp);
    hunk.lowTemp = newLowTemp;
    header->magic = HUNK_TEMP_MAGIC;
    header->sizeDelta = hunk.lowTemp - oldLowTemp;
    return header + 1;
}

/* CoDUOMP.exe 0x00435fc0; coduo_lnxded 0x0806c6d0. */
void *Hunk_ReallocateTempMemory(size_t size)
{
    if (hunk.lowUsed > hunk.totalSize || size > hunk.totalSize - hunk.lowUsed) {
        Com_Meminfo_f();
        Com_Error(ERR_DROP,
                  "\x15"
                  "Hunk_ReallocateTempMemory: failed on %i",
                  (int32_t)size);
        return NULL;
    }

    const size_t newLowTemp = hunk.lowUsed + size;
    if (HUNK_USAGE_EXCEEDS_TOTAL(hunk.highTemp, newLowTemp)) {
        Com_Meminfo_f();
        Com_Error(ERR_DROP,
                  "\x15"
                  "Hunk_ReallocateTempMemory: failed on %i",
                  (int32_t)size);
        return NULL;
    }
    hunk.lowTemp = newLowTemp;
    return hunk_data + hunk.lowUsed;
}

/* CoDUOMP.exe 0x00436010; coduo_lnxded 0x0806c724. */
void Hunk_ClearToMarkTemp(void)
{
    hunk.lowTemp = hunk.lowTempMark;
}

/* CoDUOMP.exe 0x00436020; coduo_lnxded 0x0806c733. */
void Hunk_SetMarkTemp(void)
{
    hunk.lowTempMark = hunk.lowTemp;
}

/* CoDUOMP.exe 0x00436030; coduo_lnxded 0x0806c742. */
void Hunk_FreeTempMemory(void *memory)
{
    hunk_temp_header_t *header;

    if (hunk_data == NULL) {
        free(memory);
        return;
    }

    header = (hunk_temp_header_t *)memory - 1;
    if (header->magic != HUNK_TEMP_MAGIC) {
        Com_Error(ERR_FATAL, "\x15"
                             "Hunk_FreeTempMemory: bad magic");
    }

    header->magic = HUNK_TEMP_FREED_MAGIC;
    hunk.lowTemp -= header->sizeDelta;
}

/* CoDUOMP.exe 0x00436080; coduo_lnxded 0x0806c79d. */
void Hunk_ClearTempMemory(void)
{
    if (hunk_data != NULL) {
        hunk.lowTemp = hunk.lowUsed;
    }
}

/* CoDUOMP.exe 0x004360a0; coduo_lnxded 0x0806c7b5. */
size_t Hunk_ConvertTempToPermLowInternal(void)
{
    const size_t oldLowUsed = hunk.lowUsed;
    hunk.lowUsed = hunk.lowTemp;
    return oldLowUsed;
}

/* CoDUOMP.exe 0x004360c0; coduo_lnxded 0x0806c7d2. */
void Hunk_SetLowUsedInternal(size_t lowUsed)
{
    hunk.lowUsed = lowUsed;
}

/* CoDUOMP.exe 0x004360d0; coduo_lnxded 0x0806c7df.  Exact same-module Mac
 * symbol: Com_InitZoneMemory. */
void Com_InitZoneMemory(void)
{
}

#undef HUNK_USAGE_EXCEEDS_TOTAL
