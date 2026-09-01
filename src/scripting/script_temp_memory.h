#ifndef SCRIPT_TEMP_MEMORY_H
#define SCRIPT_TEMP_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern size_t script_codeTempSize;

void TempMemoryReset(void);
uint8_t *TempMalloc(size_t size);
void TempMemorySetPos(uint8_t *pos);

#ifdef __cplusplus
}
#endif

#endif
