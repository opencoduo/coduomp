#include "q_shared.h"

#include "qcommon/hunk.h"

enum {
    COM_WEAPON_MEMORY_ALIGNMENT = 32
};

static void *com_weaponInfoMemory;        /* original 0x0389fd40 */
static int32_t com_weaponInfoMemoryOwner; /* original 0x0389fd44 */

/* Source: CoDUOMP.exe 0x0043d440..0x0043d480.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043d440_0043d481.mcode.
 * Name and signature: exact same-module Mac symbol
 * Com_GetWeaponInfoMemory. The allocation is retained between the cgame and
 * game module owners; previousOwner reports the ownership state observed
 * before this request. */
void *Com_GetWeaponInfoMemory(int32_t byteCount, int32_t *previousOwner, int32_t callerOwner)
{
    if (byteCount <= 0)
        return NULL;

    if (com_weaponInfoMemory != NULL) {
        *previousOwner = com_weaponInfoMemoryOwner;
        if (com_weaponInfoMemoryOwner == 0)
            com_weaponInfoMemoryOwner = callerOwner;
        return com_weaponInfoMemory;
    }

    com_weaponInfoMemory = Hunk_AllocLowAlignInternal((size_t)byteCount, COM_WEAPON_MEMORY_ALIGNMENT);
    *previousOwner = 0;
    com_weaponInfoMemoryOwner = callerOwner;
    return com_weaponInfoMemory;
}

/* Source: CoDUOMP.exe 0x0043d490..0x0043d4ac.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043d490_0043d4ad.mcode.
 * Name and signature: exact same-module Mac symbol
 * Com_FreeWeaponInfoMemory. Hunk memory is not individually freed; clearing
 * this pointer only releases the reusable allocation from its current owner. */
void Com_FreeWeaponInfoMemory(int32_t callerOwner, qboolean preserveAllocation)
{
    if (callerOwner != com_weaponInfoMemoryOwner)
        return;

    if (preserveAllocation == qfalse)
        com_weaponInfoMemory = NULL;
    com_weaponInfoMemoryOwner = 0;
}
