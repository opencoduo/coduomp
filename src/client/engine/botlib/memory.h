#ifndef CODUOMP_BOTLIB_MEMORY_H
#define CODUOMP_BOTLIB_MEMORY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *GetMemory(size_t size);
void *GetClearedMemory(size_t size);
void *GetHunkMemory(size_t size);
void *GetClearedHunkMemory(size_t size);
void FreeMemory(void *memory);
void PrintUsedMemorySize(void);
void PrintMemoryLabels(void);

#ifdef __cplusplus
}
#endif

#endif
