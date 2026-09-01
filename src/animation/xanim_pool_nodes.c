#include "animation_private.h"

#include <stddef.h>

/*
 * The authoritative Windows client and Linux dedicated server agree on the
 * pool-node ownership operations and their 16-bit free-list links:
 *
 *   CoDUOMP.exe   XAnimClearServerInfo  0x00498130
 *                 XAnimFreeInfo         0x00498180
 *                 XAnimAllocInfo        0x0049b370
 *   coduo_lnxded  XAnimClearServerInfo  0x080bb68c
 *                 XAnimFreeInfo         0x080bb6ae
 *                 XAnimAllocInfo        0x080bf9be
 *
 * The compilers schedule the independent free-list-head and allocation-count
 * stores differently, but there is no call or branch between them and the
 * resulting state is identical.
 */
void XAnimClearServerInfo(XAnimInfo *node)
{
    if (node->notifyName != 0) {
        SL_RemoveRefToString(node->notifyName);
    }
}

/*
 * The authoritative bodies release the same server-notify string ownership,
 * clear the handle, and reset the signed notify index to -1:
 *
 *   CoDUOMP.exe   0x004992f0..0x00499351
 *   coduo_lnxded  0x080bceb6..0x080bcee8
 *   CoD UO MP PEF 0x000edc30..0x000edc78
 *
 * Windows inlines the string-table reference-count fast path, whereas Linux
 * and this common source express the equivalent ownership release through
 * SL_RemoveRefToString.
 */
void XAnimClearServerNotify(XAnimInfo *node)
{
    if (node->notifyName != 0) {
        SL_RemoveRefToString(node->notifyName);
        node->notifyName = 0;
    }

    node->notifyIndex = -1;
}

void XAnimFreeInfo(XAnimTree *tree, uint16_t handle)
{
    XAnimInfo *node = &xanim_pool[handle];

    XAnimClearServerInfo(node);
    /* Both bodies clear the free-list backlink at +0x08.  The former Linux
     * reconstruction named this access notifyChildIndex at unrelated +0x00. */
    node->freePrev = 0;
    node->freeNext = xanim_pool[0].freeNext;
    xanim_pool[xanim_pool[0].freeNext].freePrev = handle;
    xanim_pool[0].freeNext = handle;
    --xanim_poolUsedCount;
    --tree->activePoolNodeCount;
}

XAnimInfo *XAnimAllocInfo(XAnimTree *tree, uint32_t animIndex)
{
    uint16_t handle = xanim_pool[0].freeNext;

    if (handle == 0) {
        Com_Error(ERR_DROP,
                  "\x15" "exceeded maximum number of anim info");
        return NULL;
    }

    ++xanim_poolUsedCount;
    if (xanim_poolUsedCount > xanim_poolHighWaterCount) {
        xanim_poolHighWaterCount = xanim_poolUsedCount;
    }
    ++tree->activePoolNodeCount;

    xanim_pool[0].freeNext = xanim_pool[handle].freeNext;
    xanim_pool[xanim_pool[0].freeNext].freePrev = 0;
    tree->poolNodeHandles[animIndex] = handle;
    return &xanim_pool[handle];
}
