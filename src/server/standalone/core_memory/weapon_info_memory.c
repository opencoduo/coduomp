#include "core_memory_private.h"

#include <stddef.h>
#include <stdint.h>

void *GetWeaponInfoMemory(int32_t bytes,
                          int32_t *priorState,
                          int32_t callerState)
{
    if (bytes <= 0) {
        return NULL;
    }

    if (weaponInfo_memory == NULL) {
        weaponInfo_memory = Hunk_AllocLowInternal((size_t)bytes);
        weaponInfo_memoryState = callerState;
        *priorState = 0;
    } else {
        *priorState = weaponInfo_memoryState;
        if (weaponInfo_memoryState == 0) {
            weaponInfo_memoryState = callerState;
        }
    }

    return weaponInfo_memory;
}

void FreeWeaponInfoMemory(int32_t callerState,
                          qboolean keepMemory)
{
    if (callerState == weaponInfo_memoryState) {
        if (keepMemory == qfalse) {
            weaponInfo_memory = NULL;
        }
        weaponInfo_memoryState = 0;
    }
}
