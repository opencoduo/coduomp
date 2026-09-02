#include "com_memory.h"

#include "animation/dobj.h"
#include "hunk.h"

#include <stdint.h>

enum {
    COM_FIRST_SKELETON_CACHE_KEY = 1
};

/*
 * The skeleton-cache reset and temporary-hunk clear pair is shared by both
 * engine executables:
 *
 *                                  Windows client       Linux dedicated
 * Com_ResetSkeletonCache           0x00439f20           0x08070435
 * Com_ClearTempMemory              0x00439f40           0x0807044f
 *
 * The Linux reconstruction-only DObjBumpSkelCacheKey name described the first
 * body but was not its original identity; the Windows and supporting Mac
 * engines retain the canonical Com_ResetSkeletonCache name. Windows inlines
 * that reset into Com_ClearTempMemory, while Linux emits all three calls. The
 * state update and the low/high temporary-hunk reset order are identical.
 */

void Com_ResetSkeletonCache(void)
{
    dobj_skelCacheKey =
        (int32_t)((uint32_t)dobj_skelCacheKey + 1u);
    if (dobj_skelCacheKey == 0) {
        dobj_skelCacheKey = COM_FIRST_SKELETON_CACHE_KEY;
    }
}

void Com_ClearTempMemory(void)
{
    Com_ResetSkeletonCache();
    Hunk_ClearTempMemory();
    Hunk_ClearTempMemoryHigh();
}
