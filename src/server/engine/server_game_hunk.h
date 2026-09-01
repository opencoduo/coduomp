#ifndef SHARED_SERVER_GAME_HUNK_H
#define SHARED_SERVER_GAME_HUNK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *SV_Hunk_AllocInternal(size_t size);
void *SV_Hunk_AllocLowInternal(size_t size);
void *SV_Hunk_AllocAlignInternal(size_t size, size_t alignment);
void *SV_Hunk_AllocLowAlignInternal(size_t size, size_t alignment);
void *SV_Hunk_AllocateTempMemoryInternal(size_t size);
void SV_Hunk_FreeTempMemoryInternal(void *memory);

#ifdef __cplusplus
}
#endif

#endif
