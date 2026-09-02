#include "animation_private.h"

/*
 * These state-lane primitives have the same data flow in the authoritative
 * Windows client and Linux dedicated server.  In particular, the functions
 * named XAnimInitClientTime and XAnimInitServerTime intentionally clear the
 * opposite user lane:
 *
 *   CoDUOMP.exe   0x0049bcd0, 0x0049bcf0, 0x0049bf40,
 *                 0x0049c120, 0x0049c160
 *   coduo_lnxded  0x080c0456, 0x080c048c, 0x080c0804,
 *                 0x080c0b1e, 0x080c0ba8
 *
 * Linux XAnimSetTime also materializes the source-entry pointer and then does
 * not consume it.  Retaining the expression permits the original unoptimized
 * Linux compilation shape while leaving the function's behavior unchanged.
 */
void XAnimInitClientTime(XAnimInfo *node)
{
    XAnimState *state = &node->states[XANIM_USER_SERVER];

    state->time = 0.0f;
    state->cycleCount = 0;
    state->oldTime = 0.0f;
    state->oldCycleCount = 0;
}

void XAnimInitServerTime(XAnimInfo *node)
{
    XAnimState *state = &node->states[XANIM_USER_CLIENT];

    state->time = 0.0f;
    state->cycleCount = 0;
    state->oldTime = 0.0f;
    state->oldCycleCount = 0;
}

void XAnimSetAnimRateInternal(uint32_t animIndex, float rate)
{
    uint16_t handle = xanim_currentTree->poolNodeHandles[animIndex];

    xanim_pool[handle].states[xanim_activePoolPayloadSlot].rateScale = rate;
}

void XAnimSetTime(XAnimTree *tree, uint32_t animIndex, float time)
{
    uint16_t handle = tree->poolNodeHandles[animIndex];

    if (handle == 0) {
        return;
    }

    volatile XAnimEntry *entry = &tree->sourceTree->entries[animIndex];
    XAnimState *state =
        &xanim_pool[handle].states[xanim_activePoolPayloadSlot];

    (void)entry;
    state->time = time;
    state->cycleCount = 0;
    state->oldTime = time;
    state->oldCycleCount = 0;
}

void XAnimCopyTimes(XAnimInfo *source, XAnimInfo *dest)
{
    XAnimState *sourceState =
        &source->states[xanim_activePoolPayloadSlot];
    XAnimState *destState =
        &dest->states[xanim_activePoolPayloadSlot];

    destState->time = sourceState->time;
    destState->cycleCount = sourceState->cycleCount;
    destState->oldTime = sourceState->oldTime;
    destState->oldCycleCount = sourceState->oldCycleCount;
}

/*
 * The runtime state queries likewise agree in field selection, binary32 zero
 * and one comparisons, signed 16-bit cycle comparison, and recursive tree
 * traversal:
 *
 *   CoDUOMP.exe   0x0049aba0, 0x0049abd0, 0x0049ac00,
 *                 0x0049b3f0, 0x0049b430, 0x0049b4c0
 *   coduo_lnxded  0x080bef30, 0x080bef88, 0x080befe0,
 *                 0x080bfa7c, 0x080bfafc, 0x080bfbdc
 *
 * Both x87 comparison sequences treat NaNs as unequal to zero/one and as
 * neither less nor greater, matching these ordinary C comparisons.
 */
float XAnimGetTime(XAnimTree *tree, uint32_t animIndex)
{
    uint16_t handle = tree->poolNodeHandles[animIndex];

    return handle != 0
        ? xanim_pool[handle].states[xanim_activePoolPayloadSlot].time
        : 0.0f;
}

float XAnimGetWeight(XAnimTree *tree, uint32_t animIndex)
{
    uint16_t handle = tree->poolNodeHandles[animIndex];

    return handle != 0
        ? xanim_pool[handle]
              .states[xanim_activePoolPayloadSlot]
              .currentWeight
        : 0.0f;
}

qboolean XAnimHasFinished(XAnimTree *tree, uint32_t animIndex)
{
    uint16_t handle = tree->poolNodeHandles[animIndex];

    if (handle == 0) {
        return qtrue;
    }

    XAnimState *state =
        &xanim_pool[handle].states[xanim_activePoolPayloadSlot];
    if (state->time < state->oldTime || state->time == 1.0f) {
        return qtrue;
    }

    return state->oldCycleCount < state->cycleCount;
}

qboolean XAnimHasEffectiveParentWeight(XAnimTree *tree,
                                       uint32_t animIndex)
{
    while (animIndex != 0) {
        animIndex = tree->sourceTree->entries[animIndex].parentIndex;

        uint16_t handle = tree->poolNodeHandles[animIndex];
        if (xanim_pool[handle]
                .states[xanim_activePoolPayloadSlot]
                .currentWeight == 0.0f) {
            return qfalse;
        }
    }

    return qtrue;
}

qboolean XAnimHasEffectiveChildWeight(XAnimTree *tree,
                                      uint32_t animIndex)
{
    uint16_t handle = tree->poolNodeHandles[animIndex];

    if (handle == 0 ||
        xanim_pool[handle]
                .states[xanim_activePoolPayloadSlot]
                .currentWeight == 0.0f) {
        return qfalse;
    }

    XAnimEntry *entry = &tree->sourceTree->entries[animIndex];
    if (entry->childCount == 0) {
        return qtrue;
    }

    for (int32_t child = 0; child < entry->childCount; ++child) {
        if (XAnimHasEffectiveChildWeight(
                tree, entry->payload.parent.firstChildIndex +
                          (uint32_t)child)) {
            return qtrue;
        }
    }

    return qfalse;
}

qboolean XAnimHasEffectiveWeight(XAnimTree *tree, uint32_t animIndex)
{
    return XAnimHasEffectiveParentWeight(tree, animIndex) &&
           XAnimHasEffectiveChildWeight(tree, animIndex);
}

/*
 * The authoritative Windows and Linux bodies copy the same six secondary-lane
 * fields into the primary lane and recurse through every source-tree child:
 *
 *   CoDUOMP.exe  0x00498fc0..0x00499037
 *   coduo_lnxded 0x080bc94e..0x080bca30
 *
 * The supporting Mac symbol named XAnimUpdateOldServerTime belongs to the
 * different blend-update body corresponding to the Windows/Linux
 * XAnimUpdateServerInfoSyncInternal implementation, so it is deliberately not
 * used as evidence for this function.
 */
void XAnimUpdateOldServerTime(uint32_t animIndex)
{
    uint16_t handle = xanim_currentTree->poolNodeHandles[animIndex];

    if (handle == 0) {
        return;
    }

    XAnimInfo *node = &xanim_pool[handle];
    XAnimState *primary = &node->states[XANIM_USER_CLIENT];
    const XAnimState *secondary = &node->states[XANIM_USER_SERVER];

    primary->time = secondary->oldTime;
    primary->cycleCount = secondary->oldCycleCount;
    primary->targetWeight = secondary->targetWeight;
    primary->rateScale = secondary->rateScale;
    primary->weightBlendTimeRemaining =
        secondary->weightBlendTimeRemaining;
    primary->currentWeight = secondary->currentWeight;

    XAnimEntry *entry =
        &xanim_currentTree->sourceTree->entries[animIndex];
    for (int32_t child = 0; child < entry->childCount; ++child) {
        XAnimUpdateOldServerTime(
            entry->payload.parent.firstChildIndex + child);
    }
}

/*
 * The no-weight companion has the same recursive liveness reduction, four
 * binary32 zero tests, pool release, and handle clear in all retained bodies:
 *
 *   CoDUOMP.exe   0x00499040..0x00499145
 *   coduo_lnxded  0x080bca32..0x080bcc1c
 *   CoD UO MP PEF 0x000edef0..0x000ee068
 *
 * Windows schedules the independent blend-time clear before the target-weight
 * copy, while Linux schedules them in the opposite order and materializes a
 * leaf fast path.  No call or branch can observe either scheduling choice;
 * the common body retains the identical resulting state and decisions.
 */
qboolean XAnimUpdateOldServerTimeNoWeight(uint32_t animIndex)
{
    uint16_t handle = xanim_currentTree->poolNodeHandles[animIndex];

    if (handle == 0) {
        return qfalse;
    }

    XAnimInfo *node = &xanim_pool[handle];
    XAnimState *secondary = &node->states[XANIM_USER_SERVER];

    secondary->weightBlendTimeRemaining = 0.0f;
    secondary->currentWeight = secondary->targetWeight;

    XAnimEntry *entry =
        &xanim_currentTree->sourceTree->entries[animIndex];
    qboolean hasLiveChild = qfalse;

    for (int32_t child = 0; child < entry->childCount; ++child) {
        if (XAnimUpdateOldServerTimeNoWeight(
                entry->payload.parent.firstChildIndex + child)) {
            hasLiveChild = qtrue;
        }
    }

    if (hasLiveChild || secondary->currentWeight != 0.0f ||
        secondary->targetWeight != 0.0f ||
        node->states[XANIM_USER_CLIENT].currentWeight != 0.0f ||
        node->states[XANIM_USER_CLIENT].targetWeight != 0.0f) {
        return qtrue;
    }

    XAnimFreeInfo(xanim_currentTree, handle);
    xanim_currentTree->poolNodeHandles[animIndex] = 0;
    return qfalse;
}

/*
 * The public DObj boundary performs the same null-runtime-tree guard, installs
 * that tree as the current evaluation tree, and propagates server lane state
 * from the root in both authoritative engine bodies:
 *
 *   CoDUOMP.exe   0x0049acb0..0x0049acc3
 *   coduo_lnxded  0x080bf0f4..0x080bf11b
 */
void XAnimCopySecondaryTreeStateToPrimary(DObj *obj)
{
    if (obj->runtimeTree == NULL) {
        return;
    }

    xanim_currentTree = obj->runtimeTree;
    XAnimUpdateOldServerTime(XANIM_ROOT_NODE_INDEX);
}
