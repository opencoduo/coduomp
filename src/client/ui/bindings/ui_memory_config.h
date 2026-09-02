#ifndef UI_UI_MEMORY_CONFIG_H
#define UI_UI_MEMORY_CONFIG_H

#include "client/menu/ui_menu_types.h"

#include <stdint.h>

enum {
    UI_MEMORY_POOL_PE32_CAPACITY = 1024 * 1024,
    UI_STRING_POOL_CAPACITY = 384 * 1024
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

extern unsigned char memoryPool[UI_MEMORY_POOL_CAPACITY];
extern stringDef_t *strHandle[UI_STRING_HASH_SIZE];
extern char strPool[UI_STRING_POOL_CAPACITY];
extern int32_t allocPoint;
extern qboolean outOfMemory;
extern int32_t strPoolIndex;
extern int32_t strHandleCount;
extern const char *emptyStr;

void ui_compat_controls_get_config(void);

#define UI_STRING_REFRESH_BINDINGS() ui_compat_controls_get_config()

#endif
