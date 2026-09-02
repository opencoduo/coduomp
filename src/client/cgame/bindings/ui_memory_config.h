#ifndef CGAME_UI_MEMORY_CONFIG_H
#define CGAME_UI_MEMORY_CONFIG_H

#include "client/menu/ui_menu_types.h"

#include <stdint.h>

enum {
    UI_MEMORY_POOL_PE32_CAPACITY = 128 * 1024,
    UI_STRING_POOL_CAPACITY = 128 * 1024
};

#if UINTPTR_MAX == UINT32_MAX
enum {
    UI_MEMORY_POOL_CAPACITY = UI_MEMORY_POOL_PE32_CAPACITY
};
#else
/* NOT_FROM_ORIGINAL_SOURCE: the PE32 pool held pointer-bearing UI objects.
 * Double native storage so widened pointers do not reduce its workload. */
enum {
    UI_MEMORY_POOL_CAPACITY = 2 * UI_MEMORY_POOL_PE32_CAPACITY
};
#endif

/* Original cgame storage retained at its authoritative PE32 addresses. */
extern const char *emptyStr; /* 0x3008acbc */
extern stringDef_t *strHandle[UI_STRING_HASH_SIZE]; /* 0x300f1b90 */
extern unsigned char memoryPool[UI_MEMORY_POOL_CAPACITY]; /* 0x300f3b90 */
extern char strPool[UI_STRING_POOL_CAPACITY]; /* 0x30113c10 */
extern int32_t allocPoint; /* 0x30133c10 */
extern qboolean outOfMemory; /* 0x30133c14 */
extern int32_t strPoolIndex; /* 0x30134d50 */
extern int32_t strHandleCount; /* 0x30134d54 */

#define UI_STRING_REFRESH_BINDINGS() Controls_GetConfig()

#endif
