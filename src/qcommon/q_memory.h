#ifndef QCOMMON_Q_MEMORY_H
#define QCOMMON_Q_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Com_Memcpy(void *destination, const void *source, size_t count);
void Com_Memset(void *destination, int32_t value, size_t count);
char *CopyStringInternal(const char *string);
void *Z_MallocInternal(size_t size);
void Z_FreeInternal(void *memory);

#ifdef __cplusplus
}
#endif

#endif
