#ifndef SHARED_ANIMATION_XANIM_COMPAT_H
#define SHARED_ANIMATION_XANIM_COMPAT_H

#include "qcommon/xanim_types.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* NOT_FROM_ORIGINAL_SOURCE: portable spelling of the original packed
 * runtime-tree allocation formula.  Both authoritative i386 engines allocate
 * a pointer/two-dword header, one uint16_t handle per node, two remap tables
 * of 3 * nodeCount + 1 bytes each, and no alignment padding between tables. */
static inline size_t coduo_xanim_runtime_tree_size(size_t nodeCount)
{
    return offsetof(XAnimTree, poolNodeHandles) +
           nodeCount * sizeof(uint16_t) +
           nodeCount * sizeof(xanim_runtime_tree_tail_span_t) +
           sizeof(uint16_t);
}

/* NOT_FROM_ORIGINAL_SOURCE: byte-addressed access to the two packed remap
 * tables.  The second table is deliberately odd-aligned when nodeCount is
 * even, matching the original 3 * nodeCount + 1-byte stride. */
static inline uint8_t *coduo_xanim_part_remap_table_bytes(
    XAnimTree *runtimeTree, int32_t selector)
{
    size_t nodeCount = runtimeTree->sourceTree->nodeCount;
    uint8_t *tailBytes =
        (uint8_t *)&runtimeTree->poolNodeHandles[nodeCount];

    return &tailBytes[(size_t)selector * (nodeCount * 3U + 1U)];
}

/* NOT_FROM_ORIGINAL_SOURCE: unaligned-load adapter for the packed remap
 * tables. */
static inline uint16_t coduo_xanim_part_remap_handle_load(
    const uint8_t *tableBytes, size_t nodeIndex)
{
    uint16_t handle;

    memcpy(&handle, &tableBytes[nodeIndex * sizeof(handle)], sizeof(handle));
    return handle;
}

/* NOT_FROM_ORIGINAL_SOURCE: unaligned-store adapter for the packed remap
 * tables. */
static inline void coduo_xanim_part_remap_handle_store(
    uint8_t *tableBytes, size_t nodeIndex, uint16_t handle)
{
    memcpy(&tableBytes[nodeIndex * sizeof(handle)], &handle, sizeof(handle));
}

/* NOT_FROM_ORIGINAL_SOURCE: byte-addressed accessor for a remap table's
 * generation byte and per-node generation bytes. */
static inline uint8_t *coduo_xanim_part_remap_generation_bytes(
    uint8_t *tableBytes, size_t nodeCount)
{
    return &tableBytes[nodeCount * sizeof(uint16_t)];
}

#endif
