#include "q_memory.h"

#include <stdlib.h>
#include <string.h>

void Sys_OutOfMemory(void);

/*
 * Complete common heap/string-copy and memory-operation boundary:
 *
 *                              Windows client       Linux dedicated
 * Z_FreeInternal               0x00435520           0x0806bb25
 * Z_MallocInternal             0x00435530           0x0806bb38
 * CopyStringInternal           0x00435560           0x0806bb76
 * Com_Memcpy                   0x00436170           0x080c835c
 * Com_Memset                   0x004362a0           0x080c837d
 *
 * The supporting Mac client exports the same five canonical names. Windows
 * expands the copy/fill operations and duplicates the allocation/clear
 * sequence inside CopyStringInternal; Linux retains libc calls and calls
 * Z_MallocInternal from CopyStringInternal. The resulting ownership, zeroing,
 * byte-copy, and failure contracts agree. The platform-specific
 * Sys_OutOfMemory body remains in each engine.
 */

void Com_Memcpy(void *destination, const void *source, size_t count)
{
    if (count == 0) {
        return;
    }
    (void)memcpy(destination, source, count);
}

void Com_Memset(void *destination, int32_t value, size_t count)
{
    if (count == 0) {
        return;
    }
    (void)memset(destination, value, count);
}

void Z_FreeInternal(void *memory)
{
    free(memory);
}

void *Z_MallocInternal(size_t size)
{
    void *const memory = malloc(size);

    if (memory == NULL) {
        Sys_OutOfMemory();
    }
    Com_Memset(memory, 0, size);
    return memory;
}

char *CopyStringInternal(const char *string)
{
    char *const copy = Z_MallocInternal(strlen(string) + 1);

    strcpy(copy, string);
    return copy;
}
