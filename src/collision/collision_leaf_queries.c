#include "collision_leaf_queries.h"

#include "math/q_math.h"

extern collisionNode_t *cm_nodes;
extern collisionLeaf_t *cm_leafs;
extern int32_t *cm_leafbrushes;
extern collisionBrush_t *cm_brushes;
extern int32_t cm_checkcount;

/*
 * Complete callback-driven BSP leaf/brush traversal subsystem.  The Windows
 * client/listen-server bodies occupy CoDUOMP.exe 0x00425710..0x00425969; the
 * Linux dedicated-server bodies occupy coduo_lnxded
 * 0x08057259..0x080575a8.  The retained source differences were scheduling
 * and local-spill artifacts, not behavioral differences.
 */

void CM_StoreLeafs(cmLeafQueryWork_t *work, int32_t nodeNum)
{
    const int32_t leafNum = -1 - nodeNum;
    int32_t *const leafNums = work->items;

    if (cm_leafs[leafNum].cluster != -1) {
        work->lastLeaf = leafNum;
    }

    if (work->count < work->maxCount) {
        leafNums[work->count] = leafNum;
        work->count++;
    } else {
        work->overflowed = qtrue;
    }
}

void CM_StoreLeafBrushes(cmLeafQueryWork_t *work, int32_t nodeNum)
{
    const collisionLeaf_t *const leaf = &cm_leafs[-1 - nodeNum];
    collisionBrush_t **const brushes = work->items;

    for (int32_t leafBrushIndex = 0; leafBrushIndex < (int32_t)leaf->numLeafBrushes; ++leafBrushIndex) {
        const int32_t brushIndex = cm_leafbrushes[leaf->firstLeafBrush + leafBrushIndex];
        collisionBrush_t *const brush = &cm_brushes[brushIndex];

        if (brush->checkcount == cm_checkcount) {
            continue;
        }
        brush->checkcount = cm_checkcount;

        int32_t axis;
        for (axis = 0; axis < 3 && !(brush->mins[axis] >= work->maxs[axis]) && !(work->mins[axis] >= brush->maxs[axis]); ++axis) {
        }
        if (axis != 3) {
            continue;
        }

        if (work->count >= work->maxCount) {
            work->overflowed = qtrue;
            return;
        }

        brushes[work->count] = brush;
        work->count++;
    }
}

void CM_BoxLeafnums_r(cmLeafQueryWork_t *work, int32_t nodeNum)
{
    while (nodeNum >= 0) {
        const collisionNode_t *const node = &cm_nodes[nodeNum];
        const int32_t side = BoxOnPlaneSide(work->mins, work->maxs, node->plane);

        if (side == BOX_ON_PLANE_SIDE_FRONT) {
            nodeNum = node->children[0];
        } else if (side == BOX_ON_PLANE_SIDE_BACK) {
            nodeNum = node->children[1];
        } else {
            CM_BoxLeafnums_r(work, node->children[0]);
            nodeNum = node->children[1];
        }
    }

    work->storeLeafs(work, nodeNum);
}

int32_t CM_BoxLeafnums(const vec3_t mins, const vec3_t maxs, int32_t *leafList, int32_t leafListSize, int32_t *lastLeaf)
{
    cmLeafQueryWork_t work;

    cm_checkcount++;
    work.count = 0;
    work.maxCount = leafListSize;
    work.overflowed = qfalse;
    work.items = leafList;
    for (int32_t axis = 0; axis < 3; ++axis) {
        work.mins[axis] = mins[axis];
        work.maxs[axis] = maxs[axis];
    }
    work.lastLeaf = 0;
    work.storeLeafs = CM_StoreLeafs;

    CM_BoxLeafnums_r(&work, 0);
    *lastLeaf = work.lastLeaf;
    return work.count;
}

int32_t CM_BoxBrushes(const vec3_t mins, const vec3_t maxs, collisionBrush_t **brushes, int32_t maxCount)
{
    cmLeafQueryWork_t work;

    cm_checkcount++;
    work.count = 0;
    work.maxCount = maxCount;
    work.overflowed = qfalse;
    work.items = brushes;
    for (int32_t axis = 0; axis < 3; ++axis) {
        work.mins[axis] = mins[axis];
        work.maxs[axis] = maxs[axis];
    }
    work.lastLeaf = 0;
    work.storeLeafs = CM_StoreLeafBrushes;

    CM_BoxLeafnums_r(&work, 0);
    return work.count;
}
