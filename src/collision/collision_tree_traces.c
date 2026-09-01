#include "collision_tree_traces.h"

#include "collision_leaf_queries.h"
#include "collision_leaf_traces.h"
#include "compat/coduo_x87emu.h"

#include <stdint.h>

extern collisionLeaf_t *cm_leafs;
extern collisionNode_t *cm_nodes;
extern int32_t cm_checkcount;

/*
 * Complete BSP trace-traversal cluster:
 *
 *   CoDUOMP.exe  CM_PositionTest          0x004266f0..0x00426814
 *                CM_TraceThroughTree      0x00427bd0..0x00427eca
 *                CM_SightTraceThroughTree 0x00429820..0x00429b12
 *   coduo_lnxded CM_PositionTest          0x080589fe..0x08058b84
 *                CM_TraceThroughTree      0x0805a58f..0x0805a9d4
 *                CM_SightTraceThroughTree 0x0805c90a..0x0805cd61
 *
 * Both builds traverse the same nodes and leaves in the same order. Their
 * bodies retain the platform-specific x87 operation graphs and spill points.
 */
#if defined(WINDOWS_BEHAVIOR)
/* Source: CoDUOMP.exe 0x004266f0..0x00426814.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004266f0_00426815.mcode.
 * Name: same-family Linux symbol CM_PositionTest. Stationary traces
 * enumerate the BSP leaves touched by the start-position bounds, expanded by
 * one unit, then test each leaf until one proves the trace wholly solid. */
void CM_PositionTest(
    traceWork_t *traceWork)
{
    enum {
        CM_TEST_TRACE_MAX_LEAVES = 1024
    };

    cmLeafQueryWork_t leafWork;
    int32_t leafList[CM_TEST_TRACE_MAX_LEAVES];

    leafWork.count = 0;
    leafWork.maxCount =
        CM_TEST_TRACE_MAX_LEAVES;
    leafWork.overflowed = qfalse;
    leafWork.items = leafList;
    for (int32_t axis = 0; axis < 3; ++axis) {
        leafWork.mins[axis] =
            traceWork->start[axis] +
            traceWork->mins[axis] -
            1.0f;
        leafWork.maxs[axis] =
            traceWork->start[axis] +
            traceWork->maxs[axis] +
            1.0f;
    }
    leafWork.lastLeaf = 0;
    leafWork.storeLeafs = CM_StoreLeafs;

    cm_checkcount++;
    CM_BoxLeafnums_r(&leafWork, 0);
    cm_checkcount++;

    for (int32_t leafIndex = 0;
         leafIndex < leafWork.count;
         ++leafIndex) {
        CM_TestInLeaf(
            traceWork,
            &cm_leafs[leafList[leafIndex]]);
        if (traceWork->trace.allsolid != qfalse)
            return;
    }
}

/* Source: CoDUOMP.exe 0x00427bd0..0x00427eca.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00427bd0_00427ecb.mcode.
 * Name: same-family Linux symbol CM_TraceThroughTree. Recursively split a
 * moving trace at BSP planes, preserving the original z+y+x non-axial dot
 * product order and the two independently clamped child fractions. */
void CM_TraceThroughTree(
    traceWork_t *traceWork, int32_t nodeNum,
    float startFraction, float endFraction,
    const vec3_t start, const vec3_t end)
{
    vec3_t currentStart = {start[0], start[1], start[2]};
    const float *const currentEnd = end;

    /* Stock 0x00427cca, 0x00427d15, and 0x00427ebb immediately
     * return after their recursive child calls. Reuse this native frame for
     * those proven tail calls; the non-tail near-child call at 0x00427e30
     * remains recursive so leaf order and fraction updates stay unchanged. */
    for (;;) {
        if (traceWork->trace.fraction <=
            startFraction) {
            return;
        }

        if (nodeNum < 0) {
            CM_TraceThroughLeaf(
                traceWork, &cm_leafs[-1 - nodeNum]);
            return;
        }

        const collisionNode_t *const node =
            &cm_nodes[nodeNum];
        const cplane_t *const plane = node->plane;
        float startDistance;
        float endDistance;
        float offset;

        if (plane->type < 3) {
            startDistance =
                currentStart[plane->type] -
                plane->dist;
            endDistance =
                currentEnd[plane->type] -
                plane->dist;
            offset = traceWork->maxs[plane->type];
        } else {
            startDistance =
                ((plane->normal[2] * currentStart[2] +
                  plane->normal[1] * currentStart[1]) +
                 plane->normal[0] * currentStart[0]) -
                plane->dist;
            endDistance =
                ((plane->normal[2] * currentEnd[2] +
                  plane->normal[1] * currentEnd[1]) +
                 plane->normal[0] * currentEnd[0]) -
                plane->dist;
            offset =
                traceWork->isPoint == qfalse
                    ? 2048.0f
                    : 0.0f;
        }

        if (startDistance >= offset + 1.0f &&
            endDistance >= offset + 1.0f) {
            nodeNum = node->children[0];
            continue;
        }

        if (startDistance < -1.0f - offset &&
            endDistance < -1.0f - offset) {
            nodeNum = node->children[1];
            continue;
        }

        int32_t side;
        float firstFraction;
        float secondFraction;

        if (startDistance < endDistance) {
            const float inverseDistance =
                1.0f /
                (startDistance - endDistance);
            side = 1;
            secondFraction =
                (startDistance + offset + 0.125f) *
                inverseDistance;
            firstFraction =
                (startDistance - offset + 0.125f) *
                inverseDistance;
        } else if (endDistance < startDistance) {
            const float inverseDistance =
                1.0f /
                (startDistance - endDistance);
            side = 0;
            secondFraction =
                (startDistance - offset - 0.125f) *
                inverseDistance;
            firstFraction =
                (startDistance + offset + 0.125f) *
                inverseDistance;
        } else {
            side = 0;
            firstFraction = 1.0f;
            secondFraction = 0.0f;
        }

        if (firstFraction < 0.0f)
            firstFraction = 0.0f;
        if (firstFraction > 1.0f)
            firstFraction = 1.0f;

        const float firstMidFraction =
            startFraction +
            (endFraction - startFraction) *
                firstFraction;
        vec3_t mid;
        for (int32_t axis = 0; axis < 3; ++axis) {
            mid[axis] =
                currentStart[axis] +
                (currentEnd[axis] - currentStart[axis]) *
                    firstFraction;
        }

        CM_TraceThroughTree(
            traceWork, node->children[side],
            startFraction, firstMidFraction,
            currentStart, mid);

        if (secondFraction < 0.0f)
            secondFraction = 0.0f;
        if (secondFraction > 1.0f)
            secondFraction = 1.0f;

        const float secondMidFraction =
            startFraction +
            (endFraction - startFraction) *
                secondFraction;
        for (int32_t axis = 0; axis < 3; ++axis) {
            mid[axis] =
                currentStart[axis] +
                (currentEnd[axis] - currentStart[axis]) *
                    secondFraction;
        }

        nodeNum = node->children[side ^ 1];
        startFraction = secondMidFraction;
        currentStart[0] = mid[0];
        currentStart[1] = mid[1];
        currentStart[2] = mid[2];
    }
}
/* Source: CoDUOMP.exe 0x00429820..0x00429b12.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00429820_00429b13.mcode.
 * Name: exact same-module Mac symbol CM_SightTraceThroughTree. Recursively
 * split the sight segment at BSP planes, visit the near child first, and
 * propagate the first nonzero brush or terrain hit number. */
int32_t CM_SightTraceThroughTree(
    const traceWork_t *traceWork,
    int32_t nodeNum,
    float startFraction, float endFraction,
    const vec3_t start, const vec3_t end)
{
    if (nodeNum < 0) {
        return CM_SightTraceThroughLeaf(
            traceWork,
            &cm_leafs[-1 - nodeNum]);
    }

    const collisionNode_t *const node =
        &cm_nodes[nodeNum];
    const cplane_t *const plane =
        node->plane;
    long double startDistanceRaw;
    float endDistance;
    long double offsetRaw;
    const float nonAxialOffset = 2048.0f;

    if (plane->type < 3) {
        startDistanceRaw =
            (long double)start[plane->type] -
            plane->dist;
        endDistance =
            end[plane->type] -
            plane->dist;
        offsetRaw =
            traceWork->maxs[plane->type];
    } else {
        startDistanceRaw =
            ((long double)plane->normal[1] * start[1] +
             (long double)plane->normal[2] * start[2]) +
            (long double)plane->normal[0] * start[0] -
            plane->dist;
        endDistance = (float)(
            ((long double)plane->normal[1] * end[1] +
             (long double)plane->normal[2] * end[2]) +
            (long double)plane->normal[0] * end[0] -
            plane->dist);
        offsetRaw =
            traceWork->isPoint == qfalse
                ? nonAxialOffset
                : 0.0f;
    }

    if (startDistanceRaw >= offsetRaw + 1.0f &&
        (long double)endDistance >=
            offsetRaw + 1.0f) {
        return CM_SightTraceThroughTree(
            traceWork, node->children[0],
            startFraction, endFraction,
            start, end);
    }

    if (startDistanceRaw < -1.0f - offsetRaw &&
        (long double)endDistance <
            -1.0f - offsetRaw) {
        return CM_SightTraceThroughTree(
            traceWork, node->children[1],
            startFraction, endFraction,
            start, end);
    }

    int32_t side;
    float firstFraction;
    long double secondFractionRaw;
    const float splitEpsilon = 0.125f;
    if (startDistanceRaw <
        (long double)endDistance) {
        const long double inverseDistanceRaw =
            1.0f /
            (startDistanceRaw - endDistance);
        side = 1;
        firstFraction = (float)(
            (startDistanceRaw + offsetRaw +
             splitEpsilon) *
            inverseDistanceRaw);
        secondFractionRaw =
            (startDistanceRaw - offsetRaw +
             splitEpsilon) *
            inverseDistanceRaw;
    } else if ((long double)endDistance <
               startDistanceRaw) {
        const long double inverseDistanceRaw =
            1.0f /
            (startDistanceRaw - endDistance);
        side = 0;
        firstFraction = (float)(
            (startDistanceRaw - offsetRaw -
             splitEpsilon) *
            inverseDistanceRaw);
        secondFractionRaw =
            (startDistanceRaw + offsetRaw +
             splitEpsilon) *
            inverseDistanceRaw;
    } else {
        side = 0;
        firstFraction = 0.0f;
        secondFractionRaw = 1.0f;
    }

    if (secondFractionRaw < 0.0f)
        secondFractionRaw = 0.0f;
    if (secondFractionRaw > 1.0f)
        secondFractionRaw = 1.0f;

    const float secondMidFraction = (float)(
        (long double)startFraction +
        ((long double)endFraction -
         startFraction) *
            secondFractionRaw);
    vec3_t mid;
    for (int32_t axis = 0; axis < 3;
         ++axis) {
        mid[axis] = (float)(
            (long double)start[axis] +
            ((long double)end[axis] -
             start[axis]) *
                secondFractionRaw);
    }

    const int32_t sightHit =
        CM_SightTraceThroughTree(
            traceWork, node->children[side],
            startFraction, secondMidFraction,
            start, mid);
    if (sightHit != 0)
        return sightHit;

    if (firstFraction < 0.0f)
        firstFraction = 0.0f;
    if (firstFraction > 1.0f)
        firstFraction = 1.0f;

    const float firstMidFraction =
        startFraction +
        (endFraction - startFraction) *
            firstFraction;
    for (int32_t axis = 0; axis < 3;
         ++axis) {
        mid[axis] =
            start[axis] +
            (end[axis] - start[axis]) *
                firstFraction;
    }

    return CM_SightTraceThroughTree(
        traceWork, node->children[side ^ 1],
        firstMidFraction, endFraction,
        mid, end);
}
#else

void CM_PositionTest(traceWork_t *traceWork)
{
    cmLeafQueryWork_t leafWork;
    int32_t leafList[1024];
    vec3_t mins;
    vec3_t maxs;

    mins[0] = traceWork->start[0] + traceWork->mins[0];
    mins[1] = traceWork->start[1] + traceWork->mins[1];
    mins[2] = traceWork->start[2] + traceWork->mins[2];
    maxs[0] = traceWork->start[0] + traceWork->maxs[0];
    maxs[1] = traceWork->start[1] + traceWork->maxs[1];
    maxs[2] = traceWork->start[2] + traceWork->maxs[2];

    for (int32_t axis = 0; axis < 3; ++axis) {
        mins[axis] -= 1.0f;
        maxs[axis] += 1.0f;
    }

    leafWork.count = 0;
    leafWork.maxCount = 1024;
    leafWork.items = leafList;
    leafWork.storeLeafs = CM_StoreLeafs;
    leafWork.lastLeaf = 0;
    leafWork.overflowed = 0;
    leafWork.mins[0] = mins[0];
    leafWork.mins[1] = mins[1];
    leafWork.mins[2] = mins[2];
    leafWork.maxs[0] = maxs[0];
    leafWork.maxs[1] = maxs[1];
    leafWork.maxs[2] = maxs[2];

    cm_checkcount++;
    CM_BoxLeafnums_r(&leafWork, 0);
    cm_checkcount++;

    for (int32_t leafIndex = 0; leafIndex < leafWork.count; ++leafIndex) {
        CM_TestInLeaf(traceWork, &cm_leafs[leafList[leafIndex]]);

        if (traceWork->trace.allsolid != 0) {
            return;
        }
    }
}


void CM_TraceThroughTree(traceWork_t *traceWork, int32_t nodeNum,
                         float startFraction, float endFraction,
                         const vec3_t start, const vec3_t end)
{
    if (traceWork->trace.fraction <= startFraction) {
        return;
    }

    if (nodeNum < 0) {
        CM_TraceThroughLeaf(traceWork, &cm_leafs[-1 - nodeNum]);
        return;
    }

    const collisionNode_t *node = &cm_nodes[nodeNum];
    const cplane_t *plane = node->plane;
    float startDistance;
    float endDistance;
    float offset;

    if (plane->type < 3) {
#if EMULATE_X87
        startDistance = x87f_store_f32(
            x87f_sub(x87f_load_f32(start[plane->type]),
                     x87f_load_f32(plane->dist)));
        endDistance = x87f_store_f32(
            x87f_sub(x87f_load_f32(end[plane->type]),
                     x87f_load_f32(plane->dist)));
#else
        startDistance = start[plane->type] - plane->dist;
        endDistance = end[plane->type] - plane->dist;
#endif
        offset = traceWork->maxs[plane->type];
    } else {
        /* The original forms each distance as a SINGLE 80-bit chain — three
         * products, two adds and the -dist, rounded once at the store
         * (fmul;fmul;faddp;fmul;faddp;fsub dist;fstp). Writing it as
         * `d = a*b; d += c*d; d += e*f; d -= dist;` would instead round the
         * float slot four times and does NOT match the stock binary (it
         * diverges by 1-2 ULP on roughly a quarter of realistic plane/point
         * values), so keep it one expression. */
#if EMULATE_X87
        startDistance = x87f_store_f32(x87f_sub(
            x87f_add(x87f_add(x87f_mul(x87f_load_f32(plane->normal[0]),
                                       x87f_load_f32(start[0])),
                              x87f_mul(x87f_load_f32(plane->normal[1]),
                                       x87f_load_f32(start[1]))),
                     x87f_mul(x87f_load_f32(plane->normal[2]),
                              x87f_load_f32(start[2]))),
            x87f_load_f32(plane->dist)));

        endDistance = x87f_store_f32(x87f_sub(
            x87f_add(x87f_add(x87f_mul(x87f_load_f32(plane->normal[0]),
                                       x87f_load_f32(end[0])),
                              x87f_mul(x87f_load_f32(plane->normal[1]),
                                       x87f_load_f32(end[1]))),
                     x87f_mul(x87f_load_f32(plane->normal[2]),
                              x87f_load_f32(end[2]))),
            x87f_load_f32(plane->dist)));
#else
        startDistance = ((plane->normal[0] * start[0]) +
                         (plane->normal[1] * start[1]) +
                         (plane->normal[2] * start[2])) -
                        plane->dist;

        endDistance = ((plane->normal[0] * end[0]) +
                       (plane->normal[1] * end[1]) +
                       (plane->normal[2] * end[2])) -
                      plane->dist;
#endif

        if (traceWork->isPoint == 0) {
            offset = 2048.0f;
        } else {
            offset = 0.0f;
        }
    }

#if EMULATE_X87
    /* The side tests form offset+1 and -1-offset in 80-bit and compare there
     * (fld offset; fld1; faddp; fld distance; fucompp) — no float store, so
     * they must not round the sum first. */
    const x87f sideOffsetPlusOne =
        x87f_add(x87f_load_f32(offset), x87f_load_f32(1.0f));
    const x87f sideMinusOneMinusOffset =
        x87f_sub(x87f_load_f32(-1.0f), x87f_load_f32(offset));

    if (!x87f_lt(x87f_load_f32(startDistance), sideOffsetPlusOne) &&
        !x87f_lt(x87f_load_f32(endDistance), sideOffsetPlusOne)) {
        CM_TraceThroughTree(traceWork, node->children[0], startFraction,
                            endFraction, start, end);
        return;
    }

    if (x87f_lt(x87f_load_f32(startDistance), sideMinusOneMinusOffset) &&
        x87f_lt(x87f_load_f32(endDistance), sideMinusOneMinusOffset)) {
        CM_TraceThroughTree(traceWork, node->children[1], startFraction,
                            endFraction, start, end);
        return;
    }
#else
    if (startDistance >= offset + 1.0f &&
        endDistance >= offset + 1.0f) {
        CM_TraceThroughTree(traceWork, node->children[0], startFraction,
                            endFraction, start, end);
        return;
    }

    if (startDistance < -1.0f - offset &&
        endDistance < -1.0f - offset) {
        CM_TraceThroughTree(traceWork, node->children[1], startFraction,
                            endFraction, start, end);
        return;
    }
#endif

    int32_t side;
    float firstFraction;
    float secondFraction;

    if (startDistance < endDistance) {
#if EMULATE_X87
        /* 1/(startDistance-endDistance) in 80-bit rounded to float (fld;fsub;
         * fld1;fdivrp;fstp); each fraction is one 80-bit chain rounded once
         * (fadd offset; fld 0.125f; faddp; fmul inverseDistance; fstp). */
        const float inverseDistance = x87f_store_f32(
            x87f_div(x87f_load_f32(1.0f),
                     x87f_sub(x87f_load_f32(startDistance),
                              x87f_load_f32(endDistance))));

        side = 1;
        secondFraction = x87f_store_f32(x87f_mul(
            x87f_add(x87f_add(x87f_load_f32(startDistance),
                              x87f_load_f32(offset)),
                     x87f_load_f32(0.125f)),
            x87f_load_f32(inverseDistance)));
        firstFraction = x87f_store_f32(x87f_mul(
            x87f_add(x87f_sub(x87f_load_f32(startDistance),
                              x87f_load_f32(offset)),
                     x87f_load_f32(0.125f)),
            x87f_load_f32(inverseDistance)));
#else
        const float inverseDistance = 1.0f / (startDistance - endDistance);

        side = 1;
        secondFraction = (startDistance + offset + 0.125f) * inverseDistance;
        firstFraction = (startDistance - offset + 0.125f) * inverseDistance;
#endif
    } else if (endDistance < startDistance) {
#if EMULATE_X87
        const float inverseDistance = x87f_store_f32(
            x87f_div(x87f_load_f32(1.0f),
                     x87f_sub(x87f_load_f32(startDistance),
                              x87f_load_f32(endDistance))));

        side = 0;
        secondFraction = x87f_store_f32(x87f_mul(
            x87f_sub(x87f_sub(x87f_load_f32(startDistance),
                              x87f_load_f32(offset)),
                     x87f_load_f32(0.125f)),
            x87f_load_f32(inverseDistance)));
        firstFraction = x87f_store_f32(x87f_mul(
            x87f_add(x87f_add(x87f_load_f32(startDistance),
                              x87f_load_f32(offset)),
                     x87f_load_f32(0.125f)),
            x87f_load_f32(inverseDistance)));
#else
        const float inverseDistance = 1.0f / (startDistance - endDistance);

        side = 0;
        secondFraction =
            (startDistance - offset - 0.125f) * inverseDistance;
        firstFraction = (startDistance + offset + 0.125f) * inverseDistance;
#endif
    } else {
        side = 0;
        firstFraction = 1.0f;
        secondFraction = 0.0f;
    }

    if (firstFraction < 0.0f) {
        firstFraction = 0.0f;
    }
    if (firstFraction > 1.0f) {
        firstFraction = 1.0f;
    }

#if EMULATE_X87
    /* Each lerp is one 80-bit chain rounded once at the store
     * (fld end; fsub start; fmul frac; fld start; faddp; fstp). */
    const float firstMidFraction = x87f_store_f32(x87f_add(
        x87f_load_f32(startFraction),
        x87f_mul(x87f_sub(x87f_load_f32(endFraction),
                          x87f_load_f32(startFraction)),
                 x87f_load_f32(firstFraction))));
    vec3_t mid;

    for (int32_t axis = 0; axis < 3; ++axis) {
        mid[axis] = x87f_store_f32(x87f_add(
            x87f_load_f32(start[axis]),
            x87f_mul(x87f_sub(x87f_load_f32(end[axis]),
                              x87f_load_f32(start[axis])),
                     x87f_load_f32(firstFraction))));
    }
#else
    const float firstMidFraction =
        startFraction + (endFraction - startFraction) * firstFraction;
    vec3_t mid;

    mid[0] = start[0] + (end[0] - start[0]) * firstFraction;
    mid[1] = start[1] + (end[1] - start[1]) * firstFraction;
    mid[2] = start[2] + (end[2] - start[2]) * firstFraction;
#endif

    CM_TraceThroughTree(traceWork, node->children[side], startFraction,
                        firstMidFraction, start, mid);

    if (secondFraction < 0.0f) {
        secondFraction = 0.0f;
    }
    if (secondFraction > 1.0f) {
        secondFraction = 1.0f;
    }

#if EMULATE_X87
    const float secondMidFraction = x87f_store_f32(x87f_add(
        x87f_load_f32(startFraction),
        x87f_mul(x87f_sub(x87f_load_f32(endFraction),
                          x87f_load_f32(startFraction)),
                 x87f_load_f32(secondFraction))));

    for (int32_t axis = 0; axis < 3; ++axis) {
        mid[axis] = x87f_store_f32(x87f_add(
            x87f_load_f32(start[axis]),
            x87f_mul(x87f_sub(x87f_load_f32(end[axis]),
                              x87f_load_f32(start[axis])),
                     x87f_load_f32(secondFraction))));
    }
#else
    const float secondMidFraction =
        startFraction + (endFraction - startFraction) * secondFraction;

    mid[0] = start[0] + (end[0] - start[0]) * secondFraction;
    mid[1] = start[1] + (end[1] - start[1]) * secondFraction;
    mid[2] = start[2] + (end[2] - start[2]) * secondFraction;
#endif

    CM_TraceThroughTree(traceWork, node->children[side ^ 1],
                        secondMidFraction, endFraction, mid, end);
}


int32_t
CM_SightTraceThroughTree(const traceWork_t *traceWork,
                         int32_t nodeNum,
                         float startFraction,
                         float endFraction,
                         const vec3_t start,
                         const vec3_t end)
{
    if (nodeNum < 0) {
        return CM_SightTraceThroughLeaf(traceWork, &cm_leafs[-1 - nodeNum]);
    }

    const collisionNode_t *node = &cm_nodes[nodeNum];
    const cplane_t *plane = node->plane;
    float startDistance;
    float endDistance;
    float offset;

    if (plane->type < 3) {
#if EMULATE_X87
        startDistance = x87f_store_f32(
            x87f_sub(x87f_load_f32(start[plane->type]),
                     x87f_load_f32(plane->dist)));
        endDistance = x87f_store_f32(
            x87f_sub(x87f_load_f32(end[plane->type]),
                     x87f_load_f32(plane->dist)));
#else
        startDistance = start[plane->type] - plane->dist;
        endDistance = end[plane->type] - plane->dist;
#endif
        offset = traceWork->maxs[plane->type];
    } else {
        /* As in CM_TraceThroughTree: the original forms each distance as a
         * SINGLE 80-bit chain rounded once at the store (DLL 0x0805c90a:
         * fmul;fmul;faddp;fmul;faddp;fsub dist;fstp). A `d = a*b; d += c*d;
         * ...` form would round the float slot four times and does NOT match
         * the stock binary, so keep it one expression. */
#if EMULATE_X87
        startDistance = x87f_store_f32(x87f_sub(
            x87f_add(x87f_add(x87f_mul(x87f_load_f32(plane->normal[0]),
                                       x87f_load_f32(start[0])),
                              x87f_mul(x87f_load_f32(plane->normal[1]),
                                       x87f_load_f32(start[1]))),
                     x87f_mul(x87f_load_f32(plane->normal[2]),
                              x87f_load_f32(start[2]))),
            x87f_load_f32(plane->dist)));

        endDistance = x87f_store_f32(x87f_sub(
            x87f_add(x87f_add(x87f_mul(x87f_load_f32(plane->normal[0]),
                                       x87f_load_f32(end[0])),
                              x87f_mul(x87f_load_f32(plane->normal[1]),
                                       x87f_load_f32(end[1]))),
                     x87f_mul(x87f_load_f32(plane->normal[2]),
                              x87f_load_f32(end[2]))),
            x87f_load_f32(plane->dist)));
#else
        startDistance = ((plane->normal[0] * start[0]) +
                         (plane->normal[1] * start[1]) +
                         (plane->normal[2] * start[2])) -
                        plane->dist;

        endDistance = ((plane->normal[0] * end[0]) +
                       (plane->normal[1] * end[1]) +
                       (plane->normal[2] * end[2])) -
                      plane->dist;
#endif

        if (traceWork->isPoint == 0) {
            offset = 2048.0f;
        } else {
            offset = 0.0f;
        }
    }

#if EMULATE_X87
    /* offset+1 and -1-offset are formed and compared in 80-bit (no store). */
    const x87f sideOffsetPlusOne =
        x87f_add(x87f_load_f32(offset), x87f_load_f32(1.0f));
    const x87f sideMinusOneMinusOffset =
        x87f_sub(x87f_load_f32(-1.0f), x87f_load_f32(offset));

    if (!x87f_lt(x87f_load_f32(startDistance), sideOffsetPlusOne) &&
        !x87f_lt(x87f_load_f32(endDistance), sideOffsetPlusOne)) {
        return CM_SightTraceThroughTree(traceWork, node->children[0],
                                        startFraction, endFraction, start,
                                        end);
    }

    if (x87f_lt(x87f_load_f32(startDistance), sideMinusOneMinusOffset) &&
        x87f_lt(x87f_load_f32(endDistance), sideMinusOneMinusOffset)) {
        return CM_SightTraceThroughTree(traceWork, node->children[1],
                                        startFraction, endFraction, start,
                                        end);
    }
#else
    if (startDistance >= offset + 1.0f &&
        endDistance >= offset + 1.0f) {
        return CM_SightTraceThroughTree(traceWork, node->children[0],
                                        startFraction, endFraction, start,
                                        end);
    }

    if (startDistance < -1.0f - offset &&
        endDistance < -1.0f - offset) {
        return CM_SightTraceThroughTree(traceWork, node->children[1],
                                        startFraction, endFraction, start,
                                        end);
    }
#endif

    int32_t side;
    float firstFraction;
    float secondFraction;

    if (startDistance < endDistance) {
#if EMULATE_X87
        const float inverseDistance = x87f_store_f32(
            x87f_div(x87f_load_f32(1.0f),
                     x87f_sub(x87f_load_f32(startDistance),
                              x87f_load_f32(endDistance))));

        side = 1;
        firstFraction = x87f_store_f32(x87f_mul(
            x87f_add(x87f_add(x87f_load_f32(startDistance),
                              x87f_load_f32(offset)),
                     x87f_load_f32(0.125f)),
            x87f_load_f32(inverseDistance)));
        secondFraction = x87f_store_f32(x87f_mul(
            x87f_add(x87f_sub(x87f_load_f32(startDistance),
                              x87f_load_f32(offset)),
                     x87f_load_f32(0.125f)),
            x87f_load_f32(inverseDistance)));
#else
        const float inverseDistance = 1.0f / (startDistance - endDistance);

        side = 1;
        firstFraction = (startDistance + offset + 0.125f) * inverseDistance;
        secondFraction = (startDistance - offset + 0.125f) * inverseDistance;
#endif
    } else if (endDistance < startDistance) {
#if EMULATE_X87
        const float inverseDistance = x87f_store_f32(
            x87f_div(x87f_load_f32(1.0f),
                     x87f_sub(x87f_load_f32(startDistance),
                              x87f_load_f32(endDistance))));

        side = 0;
        firstFraction = x87f_store_f32(x87f_mul(
            x87f_sub(x87f_sub(x87f_load_f32(startDistance),
                              x87f_load_f32(offset)),
                     x87f_load_f32(0.125f)),
            x87f_load_f32(inverseDistance)));
        secondFraction = x87f_store_f32(x87f_mul(
            x87f_add(x87f_add(x87f_load_f32(startDistance),
                              x87f_load_f32(offset)),
                     x87f_load_f32(0.125f)),
            x87f_load_f32(inverseDistance)));
#else
        const float inverseDistance = 1.0f / (startDistance - endDistance);

        side = 0;
        firstFraction =
            (startDistance - offset - 0.125f) * inverseDistance;
        secondFraction = (startDistance + offset + 0.125f) * inverseDistance;
#endif
    } else {
        side = 0;
        firstFraction = 0.0f;
        secondFraction = 1.0f;
    }

    if (secondFraction < 0.0f) {
        secondFraction = 0.0f;
    }
    if (secondFraction > 1.0f) {
        secondFraction = 1.0f;
    }

#if EMULATE_X87
    const float secondMidFraction = x87f_store_f32(x87f_add(
        x87f_load_f32(startFraction),
        x87f_mul(x87f_sub(x87f_load_f32(endFraction),
                          x87f_load_f32(startFraction)),
                 x87f_load_f32(secondFraction))));
    vec3_t mid;

    for (int32_t axis = 0; axis < 3; ++axis) {
        mid[axis] = x87f_store_f32(x87f_add(
            x87f_load_f32(start[axis]),
            x87f_mul(x87f_sub(x87f_load_f32(end[axis]),
                              x87f_load_f32(start[axis])),
                     x87f_load_f32(secondFraction))));
    }
#else
    const float secondMidFraction =
        startFraction + (endFraction - startFraction) * secondFraction;
    vec3_t mid;

    mid[0] = start[0] + (end[0] - start[0]) * secondFraction;
    mid[1] = start[1] + (end[1] - start[1]) * secondFraction;
    mid[2] = start[2] + (end[2] - start[2]) * secondFraction;
#endif

    const int32_t sightHit =
        CM_SightTraceThroughTree(traceWork, node->children[side],
                                 startFraction, secondMidFraction, start,
                                 mid);
    if (sightHit != 0) {
        return sightHit;
    }

    if (firstFraction < 0.0f) {
        firstFraction = 0.0f;
    }
    if (firstFraction > 1.0f) {
        firstFraction = 1.0f;
    }

#if EMULATE_X87
    const float firstMidFraction = x87f_store_f32(x87f_add(
        x87f_load_f32(startFraction),
        x87f_mul(x87f_sub(x87f_load_f32(endFraction),
                          x87f_load_f32(startFraction)),
                 x87f_load_f32(firstFraction))));

    for (int32_t axis = 0; axis < 3; ++axis) {
        mid[axis] = x87f_store_f32(x87f_add(
            x87f_load_f32(start[axis]),
            x87f_mul(x87f_sub(x87f_load_f32(end[axis]),
                              x87f_load_f32(start[axis])),
                     x87f_load_f32(firstFraction))));
    }
#else
    const float firstMidFraction =
        startFraction + (endFraction - startFraction) * firstFraction;

    mid[0] = start[0] + (end[0] - start[0]) * firstFraction;
    mid[1] = start[1] + (end[1] - start[1]) * firstFraction;
    mid[2] = start[2] + (end[2] - start[2]) * firstFraction;
#endif

    return CM_SightTraceThroughTree(traceWork, node->children[side ^ 1],
                                    firstMidFraction, endFraction, mid, end);
}
#endif
