#ifndef CLIENT_UI_MEMORY_H
#define CLIENT_UI_MEMORY_H

#include "qcommon/q_shared_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *UI_Alloc(size_t size);
void UI_InitMemory(void);
qboolean UI_OutOfMemory(void);

int32_t String_Hash(const char *string);
const char *String_Alloc(const char *string);
void String_Init(void);

#ifdef __cplusplus
}
#endif

#endif
