#ifndef CODUOMP_DYNAMIC_LIBRARY_BOUNDARY_H
#define CODUOMP_DYNAMIC_LIBRARY_BOUNDARY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void coduomp_system_library_directory(char *buffer, size_t bufferSize);
void *coduomp_library_open(const char *libraryName);
void coduomp_library_symbol(void *libraryHandle, const char *symbolName, void *destination, size_t destinationSize);
int32_t coduomp_library_close(void *libraryHandle);
uint32_t coduomp_platform_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
