#include "animation_private.h"
#include "qcommon/hunk.h"
#include "scripting/script_memory.h"
#include "xanim_compat.h"

#include <stddef.h>

enum {
    XANIM_SYNC_LOOPING = 1,
    XANIM_SYNC_NON_LOOPING = 2,
    XANIM_SYNC_MASK = XANIM_SYNC_LOOPING | XANIM_SYNC_NON_LOOPING,
    XANIM_NOTIFY_SOURCE = 4,
    XANIM_RUNTIME_TREE_MT_TAG = 5
};

/* Sources: CoDUOMP.exe 0x00496d00 and coduo_lnxded 0x080b9570.
 * Name: exact same-version Mac symbol XAnimSetLeafNode. */
void XAnimSetLeafNode(XAnim *tree, uint16_t nodeIndex,
                      const char *animName)
{
    fileData_t *asset = FS_GetDataForFile("xanim", animName, "");

    if (asset == NULL) {
        Com_Error(1, "\x15" "Cannot find 'xanim/%s'", animName);
        return;
    }

    tree->entries[nodeIndex].childCount = 0;
    tree->entries[nodeIndex].payload.leafAsset = asset;
}

/* Sources: CoDUOMP.exe 0x00496d40 and coduo_lnxded 0x080b95e2.
 * Name: exact same-version Mac symbol XAnimSetParentNode. */
void XAnimSetParentNode(XAnim *tree, uint16_t nodeIndex,
                        const char *unusedName, uint16_t firstChildIndex,
                        uint16_t childCount, uint16_t flags)
{
    XAnimEntry *entry;

    (void)unusedName;

    entry = &tree->entries[nodeIndex];
    entry->childCount = childCount;
    entry->payload.parent.flags = flags;
    entry->payload.parent.firstChildIndex = firstChildIndex;

    for (int32_t child = 0; child < childCount; ++child) {
        tree->entries[firstChildIndex + child].parentIndex = nodeIndex;
    }
}

/* Sources: CoDUOMP.exe 0x00496d80 and coduo_lnxded 0x080b966a.
 * Name: exact same-version Mac symbol XAnimAllocTree. */
XAnim *XAnimAllocTree(const char *name, uint32_t nodeCount,
                      script_anim_tree_alloc_t alloc)
{
    XAnim *tree = alloc(
        sizeof(*tree) + (size_t)nodeCount * sizeof(tree->entries[0]));

    tree->name = name;
    tree->nodeCount = nodeCount;
    return tree;
}

/* Sources: CoDUOMP.exe 0x00496da0 and coduo_lnxded 0x080b969a.
 * Both authoritative bodies allocate exactly 8 * nodeCount + 14 bytes on
 * i386, clear that complete range, then store the source-tree pointer. */
XAnimTree *XAnimAllocRuntimeTree(
    XAnim *sourceTree, script_anim_tree_alloc_t alloc)
{
    size_t size = coduo_xanim_runtime_tree_size(sourceTree->nodeCount);
    XAnimTree *runtimeTree = alloc(size);

    Com_Memset(runtimeTree, 0, size);
    runtimeTree->sourceTree = sourceTree;
    return runtimeTree;
}

/* Sources: CoDUOMP.exe 0x00496dd0 and coduo_lnxded 0x080b9700.
 * Both bodies pass the same packed runtime-tree size to the callback. */
void XAnimCopyRuntimeTree(XAnimTree *runtimeTree,
                          void (*copy)(void *data, size_t size))
{
    copy(runtimeTree,
         coduo_xanim_runtime_tree_size(runtimeTree->sourceTree->nodeCount));
}

/* Sources: CoDUOMP.exe 0x0043d3c0 and coduo_lnxded 0x08072aba.
 * Name: exact same-version Mac symbol MT_AllocAnimTree.  Linux retains an
 * allocation-tag argument in the MT allocator ABI, but MT_Alloc does not
 * inspect it; both bodies return the script-memory block selected for size. */
void *MT_AllocAnimTree(size_t size)
{
#if defined(WINDOWS_BEHAVIOR)
    return MT_Alloc(size);
#else
    return MT_Alloc(size, XANIM_RUNTIME_TREE_MT_TAG);
#endif
}

/* Sources: CoDUOMP.exe 0x0043d380 and coduo_lnxded 0x08072a9f.
 * The Windows compiler inlines XAnimAllocRuntimeTree and the hunk allocator;
 * Linux retains this source-level call graph. */
XAnimTree *Com_XAnimCreateTree(XAnim *sourceTree)
{
    return XAnimAllocRuntimeTree(sourceTree, Hunk_AllocXAnimCreateTree);
}

/* Sources: CoDUOMP.exe 0x0043d3e0 and coduo_lnxded 0x08072ad5.
 * Windows inlines the runtime-tree allocation and script-memory lookup;
 * Linux calls the same operations through MT_AllocAnimTree. */
XAnimTree *Com_XAnimCreateSmallTree(XAnim *sourceTree)
{
    return XAnimAllocRuntimeTree(sourceTree, MT_AllocAnimTree);
}

/* Sources: CoDUOMP.exe 0x0043d420 and coduo_lnxded 0x08072af0.
 * Both free the runtime header, node-handle array, and two remap tables as one
 * allocation.  Windows computes the size inline; Linux retains the call to
 * XAnimCopyRuntimeTree. */
void Com_XAnimFreeSmallTree(XAnimTree *runtimeTree)
{
    XAnimCopyRuntimeTree(runtimeTree, MT_Free);
}

/* Sources: CoDUOMP.exe 0x0049bf70 and coduo_lnxded 0x080c0850.
 * Name: exact same-version Mac symbol XAnimFillInSyncNodes_r. */
void XAnimFillInSyncNodes_r(XAnim *tree, uint32_t animIndex,
                            uint8_t requireLooping)
{
    XAnimEntry *entry = &tree->entries[animIndex];

    if (entry->childCount == 0) {
        fileData_t *asset = entry->payload.leafAsset;

        if (asset->data.xanimParts->looped != requireLooping) {
            if (requireLooping) {
                Com_Error(1,
                          "\x15" "animation '%s' in '%s' cannot be sync "
                          "looping and nonlooping",
                          asset->name, tree->name);
            } else {
                Com_Error(1,
                          "\x15" "animation '%s' in '%s' cannot be sync "
                          "nonlooping and looping",
                          asset->name, tree->name);
            }
        }
        return;
    }

    if ((entry->payload.parent.flags & XANIM_SYNC_MASK) != 0) {
        int32_t depth = 0;
        XAnimEntry *cursor = entry;

        do {
            ++depth;
            cursor = &tree->entries[cursor->payload.parent.firstChildIndex];
        } while (cursor->childCount != 0);

        Com_Error(1,
                  "\x15" "duplicate specification of animation sync in "
                  "'%s', %d nodes above '%s'",
                  tree->name, depth, cursor->payload.leafAsset->name);
    }

    entry->payload.parent.flags |= requireLooping
        ? XANIM_SYNC_LOOPING
        : XANIM_SYNC_NON_LOOPING;
    for (int32_t child = 0; child < entry->childCount; ++child) {
        XAnimFillInSyncNodes_r(
            tree, entry->payload.parent.firstChildIndex + child,
            requireLooping);
    }
}

/* Sources: CoDUOMP.exe 0x0049c040 and coduo_lnxded 0x080c09c8.
 * Name: exact same-version Mac symbol XAnimSetupSyncNodes_r. */
void XAnimSetupSyncNodes_r(XAnim *tree, uint32_t animIndex)
{
    XAnimEntry *entry = &tree->entries[animIndex];

    if (entry->childCount == 0) {
        return;
    }

    uint16_t syncFlags = entry->payload.parent.flags & XANIM_SYNC_MASK;
    if (syncFlags == 0) {
        for (int32_t child = 0; child < entry->childCount; ++child) {
            XAnimSetupSyncNodes_r(
                tree, entry->payload.parent.firstChildIndex + child);
        }
        return;
    }

    if (syncFlags == XANIM_SYNC_MASK) {
        Com_Error(1,
                  "\x15" "animation cannot be sync looping and sync "
                  "nonlooping");
    }

    entry->payload.parent.flags |= XANIM_NOTIFY_SOURCE;
    for (int32_t child = 0; child < entry->childCount; ++child) {
        XAnimFillInSyncNodes_r(
            tree, entry->payload.parent.firstChildIndex + child,
            syncFlags == XANIM_SYNC_LOOPING);
    }
}

/* Sources: CoDUOMP.exe 0x0049c0e0 and coduo_lnxded 0x080c0aa8.
 * Name: exact same-version Mac symbol XAnimSetupSyncNodes. */
void XAnimSetupSyncNodes(XAnim *tree)
{
    XAnimSetupSyncNodes_r(tree, XANIM_ROOT_NODE_INDEX);
}
