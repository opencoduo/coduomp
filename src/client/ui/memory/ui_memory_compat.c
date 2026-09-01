#include "../module/ui_functions.h"
#include "ui_memory_config.h"

#include <string.h>

/* NOT_FROM_ORIGINAL_SOURCE: a retained native image must reproduce the fresh
 * PE .bss backing store, not only rewind its cursor. The original multi/model
 * type-data allocations are intentionally not cleared by their callers. */
void ui_compat_reset_memory_pool_state(void)
{
    memset(memoryPool, 0, sizeof(memoryPool));
}
