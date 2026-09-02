#include "animation_private.h"
#include "xanim_compat.h"

#include <string.h>

/*
 * The authoritative engines recursively release the same pool-node subtree
 * in child order, then clear its handle after XAnimFreeInfo:
 *
 *   CoDUOMP.exe   0x0049bbb0..0x0049bc12
 *   coduo_lnxded  0x080c02d8..0x080c0362
 *
 * The supporting Mac body at PEF code offset 0x000e5d80 has the same unsigned
 * node-index signature and call graph.
 */
void XAnimClearTreeWeights(XAnimTree *tree, uint32_t animIndex)
{
    uint16_t handle = tree->poolNodeHandles[animIndex];

    if (handle == 0) {
        return;
    }

    XAnimEntry *entry = &tree->sourceTree->entries[animIndex];
    for (int32_t child = 0; child < entry->childCount; ++child) {
        XAnimClearTreeWeights(tree, entry->payload.parent.firstChildIndex + child);
    }

    XAnimFreeInfo(tree, handle);
    tree->poolNodeHandles[animIndex] = 0;
}

/*
 * All three retained engines clear the active root, then release both packed
 * per-node part-remap tables in table order and descending node order:
 *
 *   CoDUOMP.exe   0x0049bc20..0x0049bcc3
 *   coduo_lnxded  0x080c0364..0x080c0455
 *   CoD UO MP PEF 0x000e5c80..0x000e5d48
 *
 * The byte-addressed compatibility accessors preserve the original unaligned
 * uint16_t table layout on hosts that cannot legally dereference it directly.
 */
void XAnimClearTree(XAnimTree *tree)
{
    XAnimClearTreeWeights(tree, XANIM_ROOT_NODE_INDEX);

    uint32_t nodeCount = tree->sourceTree->nodeCount;
    for (int32_t selector = 0; selector < XANIM_PART_REMAP_TABLE_COUNT; ++selector) {
        uint8_t *partRemapTable = coduo_xanim_part_remap_table_bytes(tree, selector);

        uint32_t lastIndexBits = nodeCount - 1U;
        int32_t animIndex;
        /* Both i386 bodies decrement the node count as a dword before the
         * signed-negative loop test, including the zero-count wrap case. */
        memcpy(&animIndex, &lastIndexBits, sizeof(animIndex));
        for (; animIndex >= 0; --animIndex) {
            uint16_t handle = coduo_xanim_part_remap_handle_load(partRemapTable, (size_t)animIndex);
            if (handle == 0) {
                continue;
            }

            XAnimEntry *entry = &tree->sourceTree->entries[animIndex];
            XAnimParts *record = entry->payload.leafAsset->data.xanimParts;
            uint32_t remapSize = (uint32_t)(int16_t)record->partNameHandles[0] + DOBJ_PART_REMAP_PREFIX_SIZE;
            SL_RemoveRefToStringOfLen(handle, remapSize);
            coduo_xanim_part_remap_handle_store(partRemapTable, (size_t)animIndex, 0);
        }
    }
}
