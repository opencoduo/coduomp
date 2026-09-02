#include "animation_private.h"

#include <stdbool.h>
#include <string.h>

#define XANIM_BLEND_TIME_EPSILON 0.0010000000474974513f
#define XANIM_WEIGHT_EPSILON 1.0000001111620804e-06f

enum {
    XANIM_GOAL_WEIGHT_RESULT_OK = 0,
    XANIM_GOAL_WEIGHT_RESULT_KNOB_NOT_ANCESTOR = 1,
    XANIM_GOAL_WEIGHT_RESULT_NO_NOTIFY_DESCENDANT = 2,
    XANIM_NOTIFY_CHILD_NONE = 0,
    XANIM_NOTIFY_INDEX_NONE = -1,
    XANIM_PROPERTY_LOOP_SYNC = 1,
    XANIM_PROPERTY_NON_LOOP_SYNC = 2,
    XANIM_PROPERTY_NOTIFY_SOURCE = 4,
    XANIM_SERVER_NOTIFY_ARGUMENT_COUNT = 1
};

/* Source: CoDUOMP.exe 0x00498220..0x00498330.
 * Name: same-module Mac symbol XAnimGetAverageRateFrequency. */
float XAnimGetAverageRateFrequency(uint32_t nodeIndex)
{
    XAnimEntry *entry = &xanim_currentTree->sourceTree->entries[nodeIndex];

    if (entry->childCount == 0) {
        return entry->payload.leafAsset->data.xanimParts->frequency;
    }

    float weightSum = 0.0f;
    float frequencySum = 0.0f;
    for (int32_t child = 0; child < entry->childCount; ++child) {
        int32_t childIndex = entry->payload.parent.firstChildIndex + child;
        uint16_t handle = xanim_currentTree->poolNodeHandles[childIndex];

        if (handle != 0) {
            XAnimState *payload = &xanim_pool[handle].states[xanim_activePoolPayloadSlot];
            float weight = payload->currentWeight;

            if (weight != 0.0f) {
#if defined(WINDOWS_BEHAVIOR)
                const long double frequency = (long double)XAnimGetAverageRateFrequency(childIndex);

                if (frequency != (long double)0.0f) {
                    weightSum += weight;
                    frequencySum = (float)((long double)frequencySum + (frequency * (long double)payload->rateScale) * (long double)weight);
                }
#else
                float frequency = XAnimGetAverageRateFrequency(childIndex);

                if (frequency != 0.0f) {
#if EMULATE_X87
                    weightSum = x87f_store_f32(x87f_add(x87f_load_f32(weightSum), x87f_load_f32(weight)));
                    frequencySum = x87f_store_f32(
                        x87f_add(x87f_mul(x87f_mul(x87f_load_f32(frequency), x87f_load_f32(weight)), x87f_load_f32(payload->rateScale)),
                                 x87f_load_f32(frequencySum)));
#else
                    weightSum += weight;
                    frequencySum += frequency * weight * payload->rateScale;
#endif
                }
#endif
            }
        }
    }

#if defined(WINDOWS_BEHAVIOR)
    return weightSum != 0.0f ? (float)((long double)frequencySum / (long double)weightSum) : 0.0f;
#else
    if (weightSum == 0.0f) {
        return 0.0f;
    }
#if EMULATE_X87
    return x87f_store_f32(x87f_div(x87f_load_f32(frequencySum), x87f_load_f32(weightSum)));
#else
    return frequencySum / weightSum;
#endif
#endif
}

/* Source: CoDUOMP.exe 0x00498340..0x00498445.
 * Name: exact same-module Mac symbol XAnimUpdateInfoNoWeightClient. */
qboolean XAnimUpdateInfoNoWeightClient(uint32_t nodeIndex)
{
    uint16_t handle = xanim_currentTree->poolNodeHandles[nodeIndex];

    if (handle == 0) {
        return qfalse;
    }

    XAnimInfo *node = &xanim_pool[handle];
    XAnimState *primary = &node->states[0];

    primary->weightBlendTimeRemaining = 0.0f;
    primary->currentWeight = primary->targetWeight;

    XAnimEntry *entry = &xanim_currentTree->sourceTree->entries[nodeIndex];
    qboolean hasLiveChild = qfalse;

    for (int32_t child = 0; child < entry->childCount; ++child) {
        if (XAnimUpdateInfoNoWeightClient(entry->payload.parent.firstChildIndex + child)) {
            hasLiveChild = qtrue;
        }
    }

    if (hasLiveChild || primary->currentWeight != 0.0f || primary->targetWeight != 0.0f || node->states[1].currentWeight != 0.0f ||
        node->states[1].targetWeight != 0.0f) {
        return qtrue;
    }

    XAnimFreeInfo(xanim_currentTree, handle);
    xanim_currentTree->poolNodeHandles[nodeIndex] = 0;
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00498620..0x00498768.
 * Name: same-module Mac symbol XAnimAddClientNotify. */
void XAnimAddClientNotify(XAnimInfo *node, XAnimEntry *entry, uint16_t nameHandle, float timeFrac, uint16_t notifyType)
{
    (void)node;
    (void)entry;

    /* NOT_FROM_ORIGINAL_SOURCE: the global count covers the complete active
     * animation tree; enforce the queue capacity before ordered insertion. */
    if (xanim_deferredNotifyCount >= XANIM_DEFERRED_NOTIFY_CAPACITY) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "XAnimAddClientNotify: deferred notify limit exceeded (%i)",
                  XANIM_DEFERRED_NOTIFY_CAPACITY);
        return;
    }

    int32_t insertIndex = xanim_deferredNotifyCount;
    while (--insertIndex >= 0) {
        if (xanim_deferredNotifies[insertIndex].timeFrac <= timeFrac) {
            break;
        }
        xanim_deferredNotifies[insertIndex + 1] = xanim_deferredNotifies[insertIndex];
    }

    ++insertIndex;
    xanim_deferredNotifies[insertIndex].name =
#if defined(WINDOWS_BEHAVIOR)
        nameHandle != 0 ? SL_ConvertToString(nameHandle) : NULL;
#else
        SL_ConvertToString(nameHandle);
#endif
    xanim_deferredNotifies[insertIndex].notifyType = notifyType;
    xanim_deferredNotifies[insertIndex].timeFrac = timeFrac;
    ++xanim_deferredNotifyCount;
}

/* Source: CoDUOMP.exe 0x00498770..0x004989f5.
 * Name: same-module Mac symbol XAnimSetClientTime. */
void XAnimSetClientTime(XAnimInfo *node, XAnimEntry *entry, uint16_t notifyType)
{
    node->states[0].oldTime = xanim_evalStartTime;
    node->states[0].time = xanim_evalCurrentTime;
    node->states[0].oldCycleCount = xanim_evalStartFrame;
    node->states[0].cycleCount = xanim_evalCurrentFrame;

    if (notifyType == 0 || xanim_evalTimeStep == 0.0f) {
        return;
    }

    xanim_evalWindowTime = xanim_evalStartTime;
    xanim_evalWindowFrame = xanim_evalStartFrame;

    if (xanim_evalStartTime == 1.0f) {
        XAnimAddClientNotify(node, entry, xanim_endNotifyHandle, XAnimGetNotifyFracLeaf(1.0f), notifyType);
        return;
    }

    if (entry->childCount != 0) {
        if (xanim_evalCurrentTime < xanim_evalStartTime || xanim_evalCurrentTime == 1.0f) {
            XAnimAddClientNotify(node, entry, xanim_endNotifyHandle, XAnimGetNotifyFracLeaf(1.0f), notifyType);
        }
        return;
    }

    XAnimParts *record = entry->payload.leafAsset->data.xanimParts;
    xanim_notetrack_t *notify = &record->noteTracks[XAnimGetNextNotifyTime(entry, node, xanim_evalStartTime)];

    if (xanim_evalCurrentTime < xanim_evalStartTime) {
        if (notify->time < xanim_evalCurrentTime) {
            do {
                XAnimAddClientNotify(node, entry, notify->nameHandle, XAnimGetNotifyFracLeaf(notify->time), notifyType);
                ++notify;
                if (notify->nameHandle == 0) {
                    return;
                }
            } while (notify->time < xanim_evalCurrentTime);
        } else if (!(xanim_evalStartTime > notify->time)) {
            do {
                XAnimAddClientNotify(node, entry, notify->nameHandle, XAnimGetNotifyFracLeaf(notify->time), notifyType);
                ++notify;
            } while (notify->nameHandle != 0);

            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            for (notify = record->noteTracks; notify->time < xanim_evalCurrentTime; notify += 2) {
                XAnimAddClientNotify(node, entry, notify->nameHandle, XAnimGetNotifyFracLeaf(notify->time), notifyType);
            }
        }
    } else if (xanim_evalCurrentTime == 1.0f) {
        if (!(xanim_evalStartTime > notify->time)) {
            do {
                XAnimAddClientNotify(node, entry, notify->nameHandle, XAnimGetNotifyFracLeaf(notify->time), notifyType);
                ++notify;
            } while (notify->nameHandle != 0);
        }
    } else if (!(notify->time >= xanim_evalCurrentTime) && !(xanim_evalStartTime > notify->time)) {
        do {
            XAnimAddClientNotify(node, entry, notify->nameHandle, XAnimGetNotifyFracLeaf(notify->time), notifyType);
            ++notify;
            if (notify->nameHandle == 0) {
                return;
            }
        } while (notify->time < xanim_evalCurrentTime);
    }
}

/* Source: CoDUOMP.exe 0x00498a00..0x00498c12.
 * Name: exact same-module Mac symbol XAnimUpdateClientInfoSyncInternal. */
void XAnimUpdateClientInfoSyncInternal(uint32_t nodeIndex, qboolean notify)
{
    uint16_t handle = xanim_currentTree->poolNodeHandles[nodeIndex];

    if (handle == 0) {
        return;
    }

    XAnimInfo *node = &xanim_pool[handle];
    XAnimState *primary = &node->states[0];
    qboolean allowNotify = notify;

    if (primary->targetWeight == 0.0f) {
        allowNotify = qfalse;
    }

    if (primary->weightBlendTimeRemaining < xanim_evalTime + XANIM_BLEND_TIME_EPSILON) {
        primary->currentWeight = primary->targetWeight;
        primary->weightBlendTimeRemaining = 0.0f;
    } else {
#if defined(WINDOWS_BEHAVIOR)
        /* 0x00498a6c..0x00498a81 divides before multiplying, stores the
         * result at 0x00498a7e, and compares the retained x87 value. */
        const long double currentWeightRaw =
            (((long double)primary->targetWeight - (long double)primary->currentWeight) / (long double)primary->weightBlendTimeRemaining) *
                (long double)xanim_evalTime +
            (long double)primary->currentWeight;
        primary->currentWeight = (float)currentWeightRaw;
        if (currentWeightRaw < (long double)XANIM_WEIGHT_EPSILON) {
            primary->currentWeight = primary->targetWeight * XANIM_BLEND_TIME_EPSILON;
        }
#elif EMULATE_X87
        x87f currentWeightRaw =
            x87f_add(x87f_load_f32(primary->currentWeight),
                     x87f_div(x87f_mul(x87f_sub(x87f_load_f32(primary->targetWeight), x87f_load_f32(primary->currentWeight)),
                                       x87f_load_f32(xanim_evalTime)),
                              x87f_load_f32(primary->weightBlendTimeRemaining)));
        primary->currentWeight = x87f_store_f32(currentWeightRaw);
        if (primary->currentWeight < XANIM_WEIGHT_EPSILON) {
            primary->currentWeight = primary->targetWeight * XANIM_BLEND_TIME_EPSILON;
        }
#else
        const long double currentWeightRaw =
            (long double)primary->currentWeight +
            (((long double)primary->targetWeight - (long double)primary->currentWeight) * (long double)xanim_evalTime) /
                (long double)primary->weightBlendTimeRemaining;
        primary->currentWeight = (float)currentWeightRaw;
        if (primary->currentWeight < XANIM_WEIGHT_EPSILON) {
            primary->currentWeight = primary->targetWeight * XANIM_BLEND_TIME_EPSILON;
        }
#endif
        primary->weightBlendTimeRemaining -= xanim_evalTime;
    }

    XAnimEntry *entry = &xanim_currentTree->sourceTree->entries[nodeIndex];
    XAnimSetClientTime(node, entry, allowNotify ? node->notifyType : 0);

    if (entry->childCount == 0) {
        if (primary->currentWeight == 0.0f && primary->targetWeight == 0.0f && node->states[1].currentWeight == 0.0f &&
            node->states[1].targetWeight == 0.0f) {
            XAnimFreeInfo(xanim_currentTree, handle);
            xanim_currentTree->poolNodeHandles[nodeIndex] = 0;
        }
        return;
    }

    if (primary->currentWeight == 0.0f && primary->targetWeight == 0.0f) {
        qboolean hasLiveChild = qfalse;

        for (int32_t child = 0; child < entry->childCount; ++child) {
            if (XAnimUpdateInfoNoWeightClient(entry->payload.parent.firstChildIndex + child)) {
                hasLiveChild = qtrue;
            }
        }
        if (!hasLiveChild && node->states[1].currentWeight == 0.0f && node->states[1].targetWeight == 0.0f) {
            XAnimFreeInfo(xanim_currentTree, handle);
            xanim_currentTree->poolNodeHandles[nodeIndex] = 0;
        }
        return;
    }

    for (int32_t child = 0; child < entry->childCount; ++child) {
        XAnimUpdateClientInfoSyncInternal(entry->payload.parent.firstChildIndex + child, allowNotify);
    }
}

/* Source: CoDUOMP.exe 0x00498c20..0x00498fb6.
 * Name: exact same-module Mac symbol XAnimUpdateClientInfoInternal. */
void XAnimUpdateClientInfoInternal(uint32_t nodeIndex, float delta, qboolean notify)
{
    uint16_t handle = xanim_currentTree->poolNodeHandles[nodeIndex];

    if (handle == 0) {
        return;
    }

    XAnimInfo *node = &xanim_pool[handle];
    XAnimState *primary = &node->states[0];
    qboolean allowNotify = notify;

    if (primary->targetWeight == 0.0f) {
        allowNotify = qfalse;
    }

    if (primary->weightBlendTimeRemaining < xanim_evalTime + XANIM_BLEND_TIME_EPSILON) {
        primary->currentWeight = primary->targetWeight;
        primary->weightBlendTimeRemaining = 0.0f;
    } else {
#if defined(WINDOWS_BEHAVIOR)
        /* 0x00498c8b..0x00498ca0 is the same retained blend chain as the
         * standalone primary-weight update. */
        const long double currentWeightRaw =
            (((long double)primary->targetWeight - (long double)primary->currentWeight) / (long double)primary->weightBlendTimeRemaining) *
                (long double)xanim_evalTime +
            (long double)primary->currentWeight;
        primary->currentWeight = (float)currentWeightRaw;
        if (currentWeightRaw < (long double)XANIM_WEIGHT_EPSILON) {
            primary->currentWeight = primary->targetWeight * XANIM_BLEND_TIME_EPSILON;
        }
#elif EMULATE_X87
        x87f currentWeightRaw =
            x87f_add(x87f_load_f32(primary->currentWeight),
                     x87f_div(x87f_mul(x87f_sub(x87f_load_f32(primary->targetWeight), x87f_load_f32(primary->currentWeight)),
                                       x87f_load_f32(xanim_evalTime)),
                              x87f_load_f32(primary->weightBlendTimeRemaining)));
        primary->currentWeight = x87f_store_f32(currentWeightRaw);
        if (primary->currentWeight < XANIM_WEIGHT_EPSILON) {
            primary->currentWeight = primary->targetWeight * XANIM_BLEND_TIME_EPSILON;
        }
#else
        const long double currentWeightRaw =
            (long double)primary->currentWeight +
            (((long double)primary->targetWeight - (long double)primary->currentWeight) * (long double)xanim_evalTime) /
                (long double)primary->weightBlendTimeRemaining;
        primary->currentWeight = (float)currentWeightRaw;
        if (primary->currentWeight < XANIM_WEIGHT_EPSILON) {
            primary->currentWeight = primary->targetWeight * XANIM_BLEND_TIME_EPSILON;
        }
#endif
        primary->weightBlendTimeRemaining -= xanim_evalTime;
    }

    XAnimEntry *entry = &xanim_currentTree->sourceTree->entries[nodeIndex];
    if (entry->childCount == 0) {
        if (primary->currentWeight == 0.0f && primary->targetWeight == 0.0f) {
            if (node->states[1].currentWeight == 0.0f && node->states[1].targetWeight == 0.0f) {
                XAnimFreeInfo(xanim_currentTree, handle);
                xanim_currentTree->poolNodeHandles[nodeIndex] = 0;
            }
            return;
        }

        XAnimParts *record = entry->payload.leafAsset->data.xanimParts;
#if defined(WINDOWS_BEHAVIOR)
        /* 0x00498d4b..0x00498d64 stores the time step but retains its x87
         * value for the add and all wrap/clamp decisions. */
        const long double timeStepRaw = ((long double)record->frequency * (long double)primary->rateScale) * (long double)delta;
        xanim_evalTimeStep = (float)timeStepRaw;
        long double currentTimeRaw = timeStepRaw + (long double)primary->time;
        int16_t currentFrame = primary->cycleCount;

        if ((long double)1.0f <= currentTimeRaw) {
            /* 0x00498d71 reads loadedRecord+0x02, the loop flag. The adjacent
             * +0x03 flag denotes delta-motion data and must not control time
             * wrapping. */
            if (record->looped == 0) {
                currentTimeRaw = (long double)1.0f;
            } else {
                do {
                    currentTimeRaw -= (long double)1.0f;
                    ++currentFrame;
                } while ((long double)1.0f <= currentTimeRaw);
            }
        }

        xanim_evalStartTime = primary->time;
        xanim_evalStartFrame = primary->cycleCount;
        xanim_evalCurrentTime = (float)currentTimeRaw;
#else
#if EMULATE_X87
        xanim_evalTimeStep =
            x87f_store_f32(x87f_mul(x87f_mul(x87f_load_f32(primary->rateScale), x87f_load_f32(record->frequency)), x87f_load_f32(delta)));
        float currentTime = x87f_store_f32(x87f_add(x87f_load_f32(primary->time), x87f_load_f32(xanim_evalTimeStep)));
#else
        xanim_evalTimeStep = (primary->rateScale * record->frequency) * delta;
        float currentTime = primary->time + xanim_evalTimeStep;
#endif
        int16_t currentFrame = primary->cycleCount;

        if (1.0f <= currentTime) {
            if (record->looped == 0) {
                currentTime = 1.0f;
            } else {
                do {
#if EMULATE_X87
                    currentTime = x87f_store_f32(x87f_sub(x87f_load_f32(currentTime), x87f_load_f32(1.0f)));
#else
                    currentTime -= 1.0f;
#endif
                    ++currentFrame;
                } while (1.0f <= currentTime);
            }
        }

        xanim_evalStartTime = primary->time;
        xanim_evalStartFrame = primary->cycleCount;
        xanim_evalCurrentTime = currentTime;
#endif
        xanim_evalCurrentFrame = currentFrame;
        XAnimSetClientTime(node, entry, allowNotify ? node->notifyType : 0);
        return;
    }

    if (primary->currentWeight == 0.0f && primary->targetWeight == 0.0f) {
        qboolean hasLiveChild = qfalse;

        for (int32_t child = 0; child < entry->childCount; ++child) {
            if (XAnimUpdateInfoNoWeightClient(entry->payload.parent.firstChildIndex + child)) {
                hasLiveChild = qtrue;
            }
        }
        if (!hasLiveChild && node->states[1].currentWeight == 0.0f && node->states[1].targetWeight == 0.0f) {
            XAnimFreeInfo(xanim_currentTree, handle);
            xanim_currentTree->poolNodeHandles[nodeIndex] = 0;
        }
        return;
    }

    if ((entry->payload.parent.flags & (XANIM_PROPERTY_LOOP_SYNC | XANIM_PROPERTY_NON_LOOP_SYNC)) == 0) {
        for (int32_t child = 0; child < entry->childCount; ++child) {
            XAnimUpdateClientInfoInternal(entry->payload.parent.firstChildIndex + child, delta * primary->rateScale, allowNotify);
        }
        return;
    }

#if defined(WINDOWS_BEHAVIOR)
    /* 0x00498ea3..0x00498ec1 retains this independently computed step across
     * its float store and the following add/wrap chain. */
    const long double timeStepRaw =
        ((long double)XAnimGetAverageRateFrequency(nodeIndex) * (long double)primary->rateScale) * (long double)delta;
    xanim_evalTimeStep = (float)timeStepRaw;
    long double currentTimeRaw = timeStepRaw + (long double)primary->time;
    int16_t currentFrame = primary->cycleCount;

    if ((long double)1.0f <= currentTimeRaw) {
        if ((entry->payload.parent.flags & XANIM_PROPERTY_NON_LOOP_SYNC) == 0) {
            do {
                currentTimeRaw -= (long double)1.0f;
                ++currentFrame;
            } while ((long double)1.0f <= currentTimeRaw);
        } else {
            currentTimeRaw = (long double)1.0f;
        }
    }

    xanim_evalStartTime = primary->time;
    xanim_evalStartFrame = primary->cycleCount;
    xanim_evalCurrentTime = (float)currentTimeRaw;
#else
#if EMULATE_X87
    xanim_evalTimeStep = x87f_store_f32(x87f_mul(
        x87f_mul(x87f_load_f32(XAnimGetAverageRateFrequency(nodeIndex)), x87f_load_f32(primary->rateScale)), x87f_load_f32(delta)));
    float currentTime = x87f_store_f32(x87f_add(x87f_load_f32(primary->time), x87f_load_f32(xanim_evalTimeStep)));
#else
    xanim_evalTimeStep = (XAnimGetAverageRateFrequency(nodeIndex) * primary->rateScale) * delta;
    float currentTime = primary->time + xanim_evalTimeStep;
#endif
    int16_t currentFrame = primary->cycleCount;

    if (1.0f <= currentTime) {
        if ((entry->payload.parent.flags & XANIM_PROPERTY_NON_LOOP_SYNC) == 0) {
            do {
#if EMULATE_X87
                currentTime = x87f_store_f32(x87f_sub(x87f_load_f32(currentTime), x87f_load_f32(1.0f)));
#else
                currentTime -= 1.0f;
#endif
                ++currentFrame;
            } while (1.0f <= currentTime);
        } else {
            currentTime = 1.0f;
        }
    }

    xanim_evalStartTime = primary->time;
    xanim_evalStartFrame = primary->cycleCount;
    xanim_evalCurrentTime = currentTime;
#endif
    xanim_evalCurrentFrame = currentFrame;
    XAnimSetClientTime(node, entry, allowNotify ? node->notifyType : 0);

    xanim_evalStartTime = primary->oldTime;
    xanim_evalStartFrame = primary->oldCycleCount;
    xanim_evalCurrentTime = primary->time;
    xanim_evalCurrentFrame = primary->cycleCount;
    for (int32_t child = 0; child < entry->childCount; ++child) {
        XAnimUpdateClientInfoSyncInternal(entry->payload.parent.firstChildIndex + child, allowNotify);
    }
}

/* Source: CoDUOMP.exe 0x00499150..0x004992eb.
 * Name: exact same-module Mac symbol XAnimUpdateServerInfoSyncInternal. */
void XAnimUpdateServerInfoSyncInternal(uint32_t nodeIndex)
{
    uint16_t handle = xanim_currentTree->poolNodeHandles[nodeIndex];

    if (handle == 0) {
        return;
    }

    XAnimInfo *node = &xanim_pool[handle];
    XAnimState *secondary = &node->states[1];

    secondary->oldTime = secondary->time;
    secondary->oldCycleCount = secondary->cycleCount;
    if (secondary->weightBlendTimeRemaining < xanim_evalTime + XANIM_BLEND_TIME_EPSILON) {
        secondary->currentWeight = secondary->targetWeight;
        secondary->weightBlendTimeRemaining = 0.0f;
    } else {
#if defined(WINDOWS_BEHAVIOR)
        /* 0x004991b0..0x004991c9 divides before multiplying and compares the
         * retained result after storing its float copy at 0x004991c3. */
        const long double currentWeightRaw = (((long double)secondary->targetWeight - (long double)secondary->currentWeight) /
                                              (long double)secondary->weightBlendTimeRemaining) *
                                                 (long double)xanim_evalTime +
                                             (long double)secondary->currentWeight;
        secondary->currentWeight = (float)currentWeightRaw;
        if (currentWeightRaw < (long double)XANIM_WEIGHT_EPSILON) {
            secondary->currentWeight = secondary->targetWeight * XANIM_BLEND_TIME_EPSILON;
        }
#elif EMULATE_X87
        x87f currentWeightRaw =
            x87f_add(x87f_load_f32(secondary->currentWeight),
                     x87f_div(x87f_mul(x87f_sub(x87f_load_f32(secondary->targetWeight), x87f_load_f32(secondary->currentWeight)),
                                       x87f_load_f32(xanim_evalTime)),
                              x87f_load_f32(secondary->weightBlendTimeRemaining)));
        secondary->currentWeight = x87f_store_f32(currentWeightRaw);
        if (secondary->currentWeight < XANIM_WEIGHT_EPSILON) {
            secondary->currentWeight = secondary->targetWeight * XANIM_BLEND_TIME_EPSILON;
        }
#else
        const long double currentWeightRaw =
            (long double)secondary->currentWeight +
            (((long double)secondary->targetWeight - (long double)secondary->currentWeight) * (long double)xanim_evalTime) /
                (long double)secondary->weightBlendTimeRemaining;
        secondary->currentWeight = (float)currentWeightRaw;
        if (secondary->currentWeight < XANIM_WEIGHT_EPSILON) {
            secondary->currentWeight = secondary->targetWeight * XANIM_BLEND_TIME_EPSILON;
        }
#endif
        secondary->weightBlendTimeRemaining -= xanim_evalTime;
    }

    XAnimEntry *entry = &xanim_currentTree->sourceTree->entries[nodeIndex];
    if (entry->childCount == 0) {
        if (secondary->currentWeight == 0.0f && secondary->targetWeight == 0.0f && node->states[0].currentWeight == 0.0f &&
            node->states[0].targetWeight == 0.0f) {
            XAnimFreeInfo(xanim_currentTree, handle);
            xanim_currentTree->poolNodeHandles[nodeIndex] = 0;
        }
        return;
    }

    if (secondary->currentWeight == 0.0f && secondary->targetWeight == 0.0f) {
        qboolean hasLiveChild = qfalse;

        for (int32_t child = 0; child < entry->childCount; ++child) {
            if (XAnimUpdateOldServerTimeNoWeight(entry->payload.parent.firstChildIndex + child)) {
                hasLiveChild = qtrue;
            }
        }
        if (!hasLiveChild && node->states[0].currentWeight == 0.0f && node->states[0].targetWeight == 0.0f) {
            XAnimFreeInfo(xanim_currentTree, handle);
            xanim_currentTree->poolNodeHandles[nodeIndex] = 0;
        }
        return;
    }

    for (int32_t child = 0; child < entry->childCount; ++child) {
        XAnimUpdateServerInfoSyncInternal(entry->payload.parent.firstChildIndex + child);
    }
}

/* Source: CoDUOMP.exe 0x00499360..0x00499398.
 * Name: same-module Mac symbol NotifyServerNotetrack. */
void NotifyServerNotetrack(uint16_t rootHandle, uint16_t notifyName, uint16_t nameHandle)
{
    Scr_AddConstString(nameHandle);
    Scr_NotifyId(rootHandle, notifyName, XANIM_SERVER_NOTIFY_ARGUMENT_COUNT);
}

/* Source: CoDUOMP.exe 0x004993a0..0x00499438.
 * Name: exact same-module Mac symbol XAnimGetServerNotifyFracSyncTotal. */
float XAnimGetServerNotifyFracSyncTotal(XAnimInfo *node, XAnimEntry *entry)
{
    float earliest = XAnimGetNotifyFracServer(node, entry);

    for (int32_t child = 0; child < entry->childCount; ++child) {
        int32_t childIndex = entry->payload.parent.firstChildIndex + child;
        uint16_t handle = xanim_currentTree->poolNodeHandles[childIndex];

        if (handle != 0) {
            XAnimInfo *childNode = &xanim_pool[handle];

            if (childNode->states[1].currentWeight != 0.0f && childNode->states[1].targetWeight != 0.0f) {
#if defined(WINDOWS_BEHAVIOR)
                const long double childFraction =
                    (long double)XAnimGetServerNotifyFracSyncTotal(childNode, &xanim_currentTree->sourceTree->entries[childIndex]);

                if (childFraction < (long double)earliest) {
                    earliest = (float)childFraction;
                }
#else
                float childFraction = XAnimGetServerNotifyFracSyncTotal(childNode, &xanim_currentTree->sourceTree->entries[childIndex]);

                if (childFraction < earliest) {
                    earliest = childFraction;
                }
#endif
            }
        }
    }
    return earliest;
}

/* Source: CoDUOMP.exe 0x00499440..0x004996d8.
 * Name: exact same-module Mac symbol XAnimFindServerNoteTrack.
 * The platform carrier preserves the leaf tail-return in ST0 through the
 * recursive Windows comparison; Linux has already narrowed it to binary32. */
xanim_notify_fraction_t XAnimFindServerNoteTrack(uint32_t nodeIndex, float delta)
{
    uint16_t handle = xanim_currentTree->poolNodeHandles[nodeIndex];

    if (handle == 0) {
        return 1.0f;
    }

    XAnimInfo *node = &xanim_pool[handle];
    XAnimState *secondary = &node->states[1];

    if (secondary->currentWeight == 0.0f || secondary->targetWeight == 0.0f) {
        return 1.0f;
    }

    XAnimEntry *entry = &xanim_currentTree->sourceTree->entries[nodeIndex];
    if (entry->childCount == 0) {
        XAnimParts *record = entry->payload.leafAsset->data.xanimParts;
#if defined(WINDOWS_BEHAVIOR)
        const float timeStep = (float)(((long double)record->frequency * (long double)secondary->rateScale) * (long double)delta);

        if (timeStep == 0.0f) {
            return 1.0f;
        }

        long double currentTimeRaw = (long double)secondary->oldTime + (long double)timeStep;
        int16_t currentFrame = secondary->oldCycleCount;
        /* 0x004994d8 reads loadedRecord+0x02, the loop flag. */
        if (record->looped == 0) {
            if ((long double)1.0f <= currentTimeRaw) {
                currentTimeRaw = (long double)1.0f;
            }
        } else {
            while ((long double)1.0f <= currentTimeRaw) {
                currentTimeRaw -= (long double)1.0f;
                ++currentFrame;
            }
        }

        if ((long double)((int32_t)currentFrame - (int32_t)secondary->cycleCount) < (long double)secondary->time - currentTimeRaw) {
            return 1.0f;
        }

        const float currentTime = (float)currentTimeRaw;
#else
#if EMULATE_X87
        const float timeStep =
            x87f_store_f32(x87f_mul(x87f_mul(x87f_load_f32(secondary->rateScale), x87f_load_f32(record->frequency)), x87f_load_f32(delta)));
        float currentTime = x87f_store_f32(x87f_add(x87f_load_f32(secondary->oldTime), x87f_load_f32(timeStep)));
#else
        const float timeStep = (secondary->rateScale * record->frequency) * delta;
        float currentTime = secondary->oldTime + timeStep;
#endif
        if (timeStep == 0.0f) {
            return 1.0f;
        }

        int16_t currentFrame = secondary->oldCycleCount;
        if (record->looped == 0) {
            if (1.0f <= currentTime) {
                currentTime = 1.0f;
            }
        } else {
            while (1.0f <= currentTime) {
#if EMULATE_X87
                currentTime = x87f_store_f32(x87f_sub(x87f_load_f32(currentTime), x87f_load_f32(1.0f)));
#else
                currentTime -= 1.0f;
#endif
                ++currentFrame;
            }
        }

#if EMULATE_X87
        if (x87f_lt(x87f_load_i32((int32_t)currentFrame - (int32_t)secondary->cycleCount),
                    x87f_sub(x87f_load_f32(secondary->time), x87f_load_f32(currentTime)))) {
            return 1.0f;
        }
#else
        if ((long double)((int32_t)currentFrame - (int32_t)secondary->cycleCount) <
            (long double)secondary->time - (long double)currentTime) {
            return 1.0f;
        }
#endif
#endif
        xanim_evalStartTime = secondary->oldTime;
        xanim_evalStartFrame = secondary->oldCycleCount;
        xanim_evalCurrentTime = currentTime;
        xanim_evalWindowTime = secondary->time;
        xanim_evalWindowFrame = secondary->cycleCount;
        xanim_evalTimeStep = timeStep;
        return XAnimGetNotifyFracServer(node, entry);
    }

    if ((entry->payload.parent.flags & (XANIM_PROPERTY_LOOP_SYNC | XANIM_PROPERTY_NON_LOOP_SYNC)) == 0) {
        float childDelta = delta * secondary->rateScale;

        if (childDelta == 0.0f) {
            return 1.0f;
        }

        float earliest = 1.0f;
        for (int32_t child = 0; child < entry->childCount; ++child) {
            const xanim_notify_fraction_t childFraction =
                XAnimFindServerNoteTrack(entry->payload.parent.firstChildIndex + child, childDelta);

            if (childFraction < (xanim_notify_fraction_t)earliest) {
                earliest = (float)childFraction;
            }
        }
        return earliest;
    }

#if defined(WINDOWS_BEHAVIOR)
    const float timeStep =
        (float)(((long double)XAnimGetAverageRateFrequency(nodeIndex) * (long double)secondary->rateScale) * (long double)delta);
    if (timeStep == 0.0f) {
        return 1.0f;
    }

    long double currentTimeRaw = (long double)secondary->oldTime + (long double)timeStep;
    int16_t currentFrame = secondary->oldCycleCount;
    if ((entry->payload.parent.flags & XANIM_PROPERTY_NON_LOOP_SYNC) == 0) {
        while ((long double)1.0f <= currentTimeRaw) {
            currentTimeRaw -= (long double)1.0f;
            ++currentFrame;
        }
    } else if ((long double)1.0f <= currentTimeRaw) {
        currentTimeRaw = (long double)1.0f;
    }

    if ((long double)((int32_t)currentFrame - (int32_t)secondary->cycleCount) < (long double)secondary->time - currentTimeRaw) {
        return 1.0f;
    }

    const float currentTime = (float)currentTimeRaw;
#else
#if EMULATE_X87
    const float timeStep = x87f_store_f32(x87f_mul(
        x87f_mul(x87f_load_f32(XAnimGetAverageRateFrequency(nodeIndex)), x87f_load_f32(secondary->rateScale)), x87f_load_f32(delta)));
    float currentTime = x87f_store_f32(x87f_add(x87f_load_f32(secondary->oldTime), x87f_load_f32(timeStep)));
#else
    const float timeStep = (XAnimGetAverageRateFrequency(nodeIndex) * secondary->rateScale) * delta;
    float currentTime = secondary->oldTime + timeStep;
#endif
    if (timeStep == 0.0f) {
        return 1.0f;
    }

    int16_t currentFrame = secondary->oldCycleCount;
    if ((entry->payload.parent.flags & XANIM_PROPERTY_NON_LOOP_SYNC) == 0) {
        while (1.0f <= currentTime) {
#if EMULATE_X87
            currentTime = x87f_store_f32(x87f_sub(x87f_load_f32(currentTime), x87f_load_f32(1.0f)));
#else
            currentTime -= 1.0f;
#endif
            ++currentFrame;
        }
    } else if (1.0f <= currentTime) {
        currentTime = 1.0f;
    }

#if EMULATE_X87
    if (x87f_lt(x87f_load_i32((int32_t)currentFrame - (int32_t)secondary->cycleCount),
                x87f_sub(x87f_load_f32(secondary->time), x87f_load_f32(currentTime)))) {
        return 1.0f;
    }
#else
    if ((long double)((int32_t)currentFrame - (int32_t)secondary->cycleCount) < (long double)secondary->time - (long double)currentTime) {
        return 1.0f;
    }
#endif
#endif
    xanim_evalStartTime = secondary->oldTime;
    xanim_evalStartFrame = secondary->oldCycleCount;
    xanim_evalCurrentTime = currentTime;
    xanim_evalWindowTime = secondary->time;
    xanim_evalWindowFrame = secondary->cycleCount;
    xanim_evalTimeStep = timeStep;
    return XAnimGetServerNotifyFracSyncTotal(node, entry);
}

/* Source: CoDUOMP.exe 0x004996e0..0x004999b4.
 * Name: exact same-module Mac symbol XAnimProcessServerNotify. */
void XAnimProcessServerNotify(XAnimInfo *node, XAnimEntry *entry)
{
    if (xanim_evalRootHandle == 0 || node->notifyName == 0) {
        return;
    }

    if (xanim_evalStartTime == 1.0f) {
        NotifyServerNotetrack(xanim_evalRootHandle, node->notifyName, xanim_endNotifyHandle);
        return;
    }

    if (entry->childCount != 0) {
        if (node->notifyChildIndex == 0) {
            if (!(xanim_evalStartTime > xanim_evalCurrentTime || xanim_evalCurrentTime == 1.0f)) {
                return;
            }
            NotifyServerNotetrack(xanim_evalRootHandle, node->notifyName, xanim_endNotifyHandle);
            return;
        }
        entry = &xanim_currentTree->sourceTree->entries[node->notifyChildIndex];
    }

    XAnimParts *record = entry->payload.leafAsset->data.xanimParts;
    xanim_notetrack_t *notify = &record->noteTracks[node->notifyIndex];

    if (xanim_evalCurrentTime < xanim_evalStartTime) {
        if (notify->time < xanim_evalCurrentTime) {
            do {
                NotifyServerNotetrack(xanim_evalRootHandle, node->notifyName, notify->nameHandle);
                ++notify;
                if (notify->nameHandle == 0) {
                    break;
                }
            } while (notify->time < xanim_evalCurrentTime);

            node->notifyIndex = (int16_t)XAnimGetNextNotifyTime(entry, node, xanim_evalCurrentTime);
        } else if (!(xanim_evalStartTime > notify->time)) {
            do {
                NotifyServerNotetrack(xanim_evalRootHandle, node->notifyName, notify->nameHandle);
                ++notify;
            } while (notify->nameHandle != 0);

            for (notify = record->noteTracks; notify->time < xanim_evalCurrentTime; ++notify) {
                NotifyServerNotetrack(xanim_evalRootHandle, node->notifyName, notify->nameHandle);
            }
            node->notifyIndex = (int16_t)XAnimGetNextNotifyTime(entry, node, xanim_evalCurrentTime);
        }
    } else if (xanim_evalCurrentTime == 1.0f) {
        if (!(xanim_evalStartTime > notify->time)) {
            do {
                NotifyServerNotetrack(xanim_evalRootHandle, node->notifyName, notify->nameHandle);
                ++notify;
            } while (notify->nameHandle != 0);
        }
    } else if (!(notify->time >= xanim_evalCurrentTime) && !(xanim_evalStartTime > notify->time)) {
        do {
            NotifyServerNotetrack(xanim_evalRootHandle, node->notifyName, notify->nameHandle);
            ++notify;
            if (notify->nameHandle == 0) {
                break;
            }
        } while (notify->time < xanim_evalCurrentTime);

        node->notifyIndex = (int16_t)XAnimGetNextNotifyTime(entry, node, xanim_evalCurrentTime);
    }
}

/* Source: CoDUOMP.exe 0x004999c0..0x00499a42.
 * Name: exact same-module Mac symbol XAnimProcessServerNotify_r. */
void XAnimProcessServerNotify_r(XAnimInfo *node, XAnimEntry *entry)
{
    XAnimProcessServerNotify(node, entry);

    for (int32_t child = 0; child < entry->childCount; ++child) {
        int32_t childIndex = entry->payload.parent.firstChildIndex + child;
        uint16_t handle = xanim_currentTree->poolNodeHandles[childIndex];

        if (handle != 0) {
            XAnimInfo *childNode = &xanim_pool[handle];

            if (childNode->states[1].currentWeight != 0.0f && childNode->states[1].targetWeight != 0.0f) {
                XAnimProcessServerNotify_r(childNode, &xanim_currentTree->sourceTree->entries[childIndex]);
            }
        }
    }
}

/* Source: CoDUOMP.exe 0x00499a50..0x00499aaa.
 * Name: proven recursive secondary-window timestamp write. */
void XAnimStampSecondaryWindowStart(int32_t nodeIndex)
{
    uint16_t handle = xanim_currentTree->poolNodeHandles[nodeIndex];

    if (handle == 0) {
        return;
    }

    XAnimInfo *node = &xanim_pool[handle];
    node->states[1].time = xanim_evalCurrentTime;
    node->states[1].cycleCount = xanim_evalCurrentFrame;

    XAnimEntry *entry = &xanim_currentTree->sourceTree->entries[nodeIndex];
    for (int32_t child = 0; child < entry->childCount; ++child) {
        XAnimStampSecondaryWindowStart(entry->payload.parent.firstChildIndex + child);
    }
}

/* Source: CoDUOMP.exe 0x00499ab0..0x00499d4f.
 * Name: exact same-module Mac symbol XAnimUpdateServerInfoInternal. */
void XAnimUpdateServerInfoInternal(uint32_t nodeIndex, float delta, qboolean notify)
{
    uint16_t handle = xanim_currentTree->poolNodeHandles[nodeIndex];

    if (handle == 0) {
        return;
    }

    XAnimInfo *node = &xanim_pool[handle];
    XAnimState *secondary = &node->states[1];
    qboolean allowNotify = notify;

    if (secondary->currentWeight == 0.0f) {
        return;
    }
    if (secondary->targetWeight == 0.0f) {
        allowNotify = qfalse;
    }

    XAnimEntry *entry = &xanim_currentTree->sourceTree->entries[nodeIndex];
    if (entry->childCount == 0) {
        XAnimParts *record = entry->payload.leafAsset->data.xanimParts;
#if defined(WINDOWS_BEHAVIOR)
        /* 0x00499b1b..0x00499bb4 retains this complete x87 chain through
         * every clamp, wrap, and the frame-window comparison. */
        long double currentTimeRaw =
            (((long double)record->frequency * (long double)secondary->rateScale) * (long double)delta) + (long double)secondary->oldTime;
        int16_t currentFrame = secondary->oldCycleCount;

        /* 0x00499b1e reads loadedRecord+0x02, the loop flag. */
        if (record->looped == 0) {
            if ((long double)1.0f <= currentTimeRaw) {
                currentTimeRaw = (long double)1.0f;
            } else if (currentTimeRaw <= (long double)0.0f) {
                return;
            }
        } else {
            while (currentTimeRaw < (long double)0.0f) {
                currentTimeRaw += (long double)1.0f;
                --currentFrame;
            }
            while ((long double)1.0f <= currentTimeRaw) {
                currentTimeRaw -= (long double)1.0f;
                ++currentFrame;
            }
        }

        if ((long double)secondary->time - currentTimeRaw <= (long double)((int32_t)currentFrame - (int32_t)secondary->cycleCount)) {
            float currentTime = (float)currentTimeRaw;
            if (allowNotify) {
                xanim_evalCurrentTime = currentTime;
                xanim_evalStartTime = secondary->time;
                xanim_evalStartFrame = secondary->cycleCount;
                XAnimProcessServerNotify(node, entry);
            }
            secondary->time = currentTime;
            secondary->cycleCount = currentFrame;
        }
#else
#if EMULATE_X87
        float currentTime = x87f_store_f32(
            x87f_add(x87f_load_f32(secondary->oldTime),
                     x87f_mul(x87f_mul(x87f_load_f32(delta), x87f_load_f32(secondary->rateScale)), x87f_load_f32(record->frequency))));
#else
        float currentTime = secondary->oldTime + delta * secondary->rateScale * record->frequency;
#endif
        int16_t currentFrame = secondary->oldCycleCount;

        if (record->looped == 0) {
            if (1.0f <= currentTime) {
                currentTime = 1.0f;
            } else if (currentTime <= 0.0f) {
                return;
            }
        } else {
            while (currentTime < 0.0f) {
#if EMULATE_X87
                currentTime = x87f_store_f32(x87f_add(x87f_load_f32(currentTime), x87f_load_f32(1.0f)));
#else
                currentTime += 1.0f;
#endif
                --currentFrame;
            }
            while (1.0f <= currentTime) {
#if EMULATE_X87
                currentTime = x87f_store_f32(x87f_sub(x87f_load_f32(currentTime), x87f_load_f32(1.0f)));
#else
                currentTime -= 1.0f;
#endif
                ++currentFrame;
            }
        }

#if EMULATE_X87
        qboolean reachedWindow = x87f_le(x87f_sub(x87f_load_f32(secondary->time), x87f_load_f32(currentTime)),
                                         x87f_load_i32((int32_t)currentFrame - (int32_t)secondary->cycleCount));
#else
        qboolean reachedWindow = (long double)secondary->time - (long double)currentTime <=
                                 (long double)((int32_t)currentFrame - (int32_t)secondary->cycleCount);
#endif
        if (reachedWindow) {
            if (allowNotify) {
                xanim_evalCurrentTime = currentTime;
                xanim_evalStartTime = secondary->time;
                xanim_evalStartFrame = secondary->cycleCount;
                XAnimProcessServerNotify(node, entry);
            }
            secondary->time = currentTime;
            secondary->cycleCount = currentFrame;
        }
#endif
        return;
    }

    if ((entry->payload.parent.flags & (XANIM_PROPERTY_LOOP_SYNC | XANIM_PROPERTY_NON_LOOP_SYNC)) == 0) {
        for (int32_t child = 0; child < entry->childCount; ++child) {
            XAnimUpdateServerInfoInternal(entry->payload.parent.firstChildIndex + child, delta * secondary->rateScale, allowNotify);
        }
        return;
    }

#if defined(WINDOWS_BEHAVIOR)
    /* 0x00499c1e..0x00499ccd likewise leaves the synchronized-node time on
     * the x87 stack until its final decision/store. */
    long double currentTimeRaw =
        ((long double)XAnimGetAverageRateFrequency(nodeIndex) * (long double)secondary->rateScale) * (long double)delta +
        (long double)secondary->oldTime;
    int16_t currentFrame = secondary->oldCycleCount;

    if ((entry->payload.parent.flags & XANIM_PROPERTY_NON_LOOP_SYNC) == 0) {
        while (currentTimeRaw < (long double)0.0f) {
            currentTimeRaw += (long double)1.0f;
            --currentFrame;
        }
        while ((long double)1.0f <= currentTimeRaw) {
            currentTimeRaw -= (long double)1.0f;
            ++currentFrame;
        }
    } else if ((long double)1.0f <= currentTimeRaw) {
        currentTimeRaw = (long double)1.0f;
    } else if (currentTimeRaw <= (long double)0.0f) {
        return;
    }

    if ((long double)secondary->time - currentTimeRaw <= (long double)((int32_t)currentFrame - (int32_t)secondary->cycleCount)) {
        float currentTime = (float)currentTimeRaw;
        xanim_evalCurrentTime = currentTime;
        if (allowNotify) {
            xanim_evalStartTime = secondary->time;
            xanim_evalStartFrame = secondary->cycleCount;
            XAnimProcessServerNotify_r(node, entry);
        }
        xanim_evalCurrentFrame = currentFrame;
        XAnimStampSecondaryWindowStart(nodeIndex);
    }
#else
#if EMULATE_X87
    float timeStep = x87f_store_f32(x87f_mul(
        x87f_mul(x87f_load_f32(XAnimGetAverageRateFrequency(nodeIndex)), x87f_load_f32(secondary->rateScale)), x87f_load_f32(delta)));
    float currentTime = x87f_store_f32(x87f_add(x87f_load_f32(secondary->oldTime), x87f_load_f32(timeStep)));
#else
    float timeStep = (XAnimGetAverageRateFrequency(nodeIndex) * secondary->rateScale) * delta;
    float currentTime = secondary->oldTime + timeStep;
#endif
    int16_t currentFrame = secondary->oldCycleCount;

    if ((entry->payload.parent.flags & XANIM_PROPERTY_NON_LOOP_SYNC) == 0) {
        while (currentTime < 0.0f) {
#if EMULATE_X87
            currentTime = x87f_store_f32(x87f_add(x87f_load_f32(currentTime), x87f_load_f32(1.0f)));
#else
            currentTime += 1.0f;
#endif
            --currentFrame;
        }
        while (1.0f <= currentTime) {
#if EMULATE_X87
            currentTime = x87f_store_f32(x87f_sub(x87f_load_f32(currentTime), x87f_load_f32(1.0f)));
#else
            currentTime -= 1.0f;
#endif
            ++currentFrame;
        }
    } else if (1.0f <= currentTime) {
        currentTime = 1.0f;
    } else if (currentTime <= 0.0f) {
        return;
    }

#if EMULATE_X87
    qboolean reachedWindow = x87f_le(x87f_sub(x87f_load_f32(secondary->time), x87f_load_f32(currentTime)),
                                     x87f_load_i32((int32_t)currentFrame - (int32_t)secondary->cycleCount));
#else
    qboolean reachedWindow =
        (long double)secondary->time - (long double)currentTime <= (long double)((int32_t)currentFrame - (int32_t)secondary->cycleCount);
#endif
    if (reachedWindow) {
        xanim_evalCurrentTime = currentTime;
        if (allowNotify) {
            xanim_evalStartTime = secondary->time;
            xanim_evalStartFrame = secondary->cycleCount;
            XAnimProcessServerNotify_r(node, entry);
        }
        xanim_evalCurrentFrame = currentFrame;
        XAnimStampSecondaryWindowStart(nodeIndex);
    }
#endif
}

/* Source: CoDUOMP.exe 0x0049b4f0..0x0049b58f.
 * Name: same-module Mac symbol XAnimClearGoalWeight. */
void XAnimClearGoalWeight(XAnimTree *tree, uint32_t animIndex, float blendTime)
{
    uint16_t handle = tree->poolNodeHandles[animIndex];

    if (handle == 0) {
        return;
    }

    XAnimInfo *node = &xanim_pool[handle];
    XAnimState *payload = &node->states[xanim_activePoolPayloadSlot];
    if (!XAnimHasEffectiveParentWeight(tree, animIndex) || !XAnimHasEffectiveChildWeight(tree, animIndex)) {
        payload->currentWeight = 0.0f;
        payload->weightBlendTimeRemaining = 0.0f;
        payload->targetWeight = 0.0f;
    } else if (payload->targetWeight != 0.0f) {
        payload->targetWeight = 0.0f;
        payload->weightBlendTimeRemaining = blendTime;
    } else if (blendTime < payload->weightBlendTimeRemaining) {
        payload->weightBlendTimeRemaining = blendTime;
    }

    if (xanim_activePoolPayloadSlot != 0) {
        XAnimClearServerNotify(node);
    }
}

/* Source: CoDUOMP.exe 0x0049b590..0x0049b5e3.
 * Name: same-module Mac symbol XAnimClearTreeGoalWeights_r. */
void XAnimClearTreeGoalWeights_r(XAnimTree *tree, uint32_t animIndex, float blendTime)
{
    if (tree->poolNodeHandles[animIndex] == 0) {
        return;
    }

    XAnimClearGoalWeight(tree, animIndex, blendTime);
    XAnimEntry *entry = &tree->sourceTree->entries[animIndex];
    for (int32_t child = 0; child < entry->childCount; ++child) {
        XAnimClearTreeGoalWeights_r(tree, entry->payload.parent.firstChildIndex + child, blendTime);
    }
}

/* Source: CoDUOMP.exe 0x0049b5f0..0x0049b618.
 * Name: same-module Mac symbol XAnimClearTreeGoalWeights. */
void XAnimClearTreeGoalWeights(XAnimTree *tree, uint32_t animIndex, float blendTime)
{
    if (blendTime < XANIM_BLEND_TIME_EPSILON) {
        blendTime = 0.0f;
    }
    XAnimClearTreeGoalWeights_r(tree, animIndex, blendTime);
}

/* Source: CoDUOMP.exe 0x0049b620..0x0049b671.
 * Name: same-module Mac symbol XAnimClearTreeGoalWeightsStrict. */
void XAnimClearTreeGoalWeightsStrict(XAnimTree *tree, uint32_t animIndex, float blendTime)
{
    if (blendTime < XANIM_BLEND_TIME_EPSILON) {
        blendTime = 0.0f;
    }

    XAnimEntry *entry = &tree->sourceTree->entries[animIndex];
    for (int32_t child = 0; child < entry->childCount; ++child) {
        XAnimClearTreeGoalWeights_r(tree, entry->payload.parent.firstChildIndex + child, blendTime);
    }
}

/* Source: CoDUOMP.exe 0x0049b680..0x0049b93a.
 * Name: same-module Mac symbol XAnimClearGoalWeightKnobInternal. */
void XAnimClearGoalWeightKnobInternal(XAnimTree *tree, uint32_t animIndex, float weight, float blendTime)
{
    if (animIndex == 0) {
        return;
    }

    uint32_t parentIndex = tree->sourceTree->entries[animIndex].parentIndex;
    XAnimEntry *parent = &tree->sourceTree->entries[parentIndex];
    float maxWeight = 0.0f;

    for (int32_t child = 0; child < parent->childCount; ++child) {
        uint32_t childIndex = parent->payload.parent.firstChildIndex + (uint32_t)child;
        uint16_t handle = tree->poolNodeHandles[childIndex];
        float currentWeight = handle != 0 ? xanim_pool[handle].states[xanim_activePoolPayloadSlot].currentWeight : 0.0f;
        float siblingWeight;
        if (childIndex == animIndex) {
            /* 0x0049b707..0x0049b720 rounds the subtraction to binary32,
             * then clears that stored value's IEEE-754 sign bit. */
            float signedDifference = (float)((long double)weight - (long double)currentWeight);
            uint32_t differenceBits;
            memcpy(&differenceBits, &signedDifference, sizeof(differenceBits));
            differenceBits &= UINT32_C(0x7fffffff);
            memcpy(&siblingWeight, &differenceBits, sizeof(siblingWeight));
        } else {
            siblingWeight = currentWeight;
        }

        if (maxWeight < siblingWeight) {
            maxWeight = siblingWeight;
        }
    }

#if defined(WINDOWS_BEHAVIOR)
    /* 0x0049b8ec..0x0049b8f4 stores this product for child calls, while its
     * retained x87 value controls the epsilon clamp. */
    const long double siblingBlendTimeRaw = (long double)maxWeight * (long double)blendTime;
    float siblingBlendTime = (float)siblingBlendTimeRaw;
    if (siblingBlendTimeRaw < (long double)XANIM_BLEND_TIME_EPSILON) {
        siblingBlendTime = 0.0f;
    }
#else
#if EMULATE_X87
    float siblingBlendTime = x87f_store_f32(x87f_mul(x87f_load_f32(maxWeight), x87f_load_f32(blendTime)));
#else
    float siblingBlendTime = maxWeight * blendTime;
#endif
    if (siblingBlendTime < XANIM_BLEND_TIME_EPSILON) {
        siblingBlendTime = 0.0f;
    }
#endif

    for (int32_t child = 0; child < parent->childCount; ++child) {
        uint32_t childIndex = parent->payload.parent.firstChildIndex + (uint32_t)child;

        if (childIndex != animIndex) {
            XAnimClearGoalWeight(tree, childIndex, siblingBlendTime);
        }
    }
}

/* Source: CoDUOMP.exe 0x0049b940..0x0049b993.
 * Name: same-module Mac symbol XAnimSetCompleteGoalWeightKnob. */
void XAnimSetCompleteGoalWeightKnob(XAnimTree *tree, uint32_t animIndex, float weight, float blendTime, float rate, uint16_t notifyName,
                                    uint16_t notifyType, qboolean restart)
{
    if (weight < XANIM_BLEND_TIME_EPSILON) {
        weight = 0.0f;
    }

    XAnimClearGoalWeightKnobInternal(tree, animIndex, weight, blendTime);
    XAnimSetCompleteGoalWeight(tree, animIndex, weight, blendTime, rate, notifyName, notifyType, restart);
}

/* Source: CoDUOMP.exe 0x0049b9a0..0x0049bae2.
 * Name: same-module Mac symbol XAnimSetCompleteGoalWeightKnobAll. */
int32_t XAnimSetCompleteGoalWeightKnobAll(XAnimTree *tree, uint32_t animIndex, uint32_t knobIndex, float weight, float blendTime,
                                          float rate, uint16_t notifyName, uint16_t notifyType, qboolean restart)
{
    xanim_currentTree = tree;
    if (weight < XANIM_BLEND_TIME_EPSILON) {
        weight = 0.0f;
    }

    XAnimClearGoalWeightKnobInternal(tree, animIndex, weight, blendTime);
    int32_t result = XAnimSetGoalWeightInternal(animIndex, weight, blendTime, rate, false, notifyName, notifyType);
    XAnimEnsureGoalWeightParent(animIndex, blendTime);
    XAnimUpdateSyncTime(animIndex, restart);
    if (xanim_activePoolPayloadSlot != 0) {
        XAnimUpdateServerNotify(animIndex);
    }

    uint32_t parentIndex = animIndex;
    while (parentIndex != 0) {
        parentIndex = tree->sourceTree->entries[parentIndex].parentIndex;
        if (parentIndex == knobIndex) {
            return result;
        }

        XAnimClearGoalWeightKnobInternal(tree, parentIndex, 1.0f, blendTime);
        XAnimSetGoalWeightInternal(parentIndex, 1.0f, blendTime, 1.0f, false, 0, 0);
        XAnimUpdateSyncTime(parentIndex, restart);
        if (xanim_activePoolPayloadSlot != 0) {
            XAnimUpdateServerNotify(parentIndex);
        }
    }

    return XANIM_GOAL_WEIGHT_RESULT_KNOB_NOT_ANCESTOR;
}

/* Source: CoDUOMP.exe 0x0049baf0..0x0049bb43.
 * Name: same-module Mac symbol XAnimSetGoalWeightKnob. */
int32_t XAnimSetGoalWeightKnob(XAnimTree *tree, uint32_t animIndex, float weight, float blendTime, float rate, uint16_t notifyName,
                               uint16_t notifyType, qboolean restart)
{
    if (weight < XANIM_BLEND_TIME_EPSILON) {
        weight = 0.0f;
    }

    XAnimClearGoalWeightKnobInternal(tree, animIndex, weight, blendTime);
    return XAnimSetGoalWeight(tree, animIndex, weight, blendTime, rate, notifyName, notifyType, restart);
}

/* Source: CoDUOMP.exe 0x0049bb50..0x0049bba2.
 * Name: same-module Mac symbol XAnimClearChildGoalWeights. */
void XAnimClearChildGoalWeights(XAnimTree *tree, uint32_t animIndex, float blendTime)
{
    if (blendTime < XANIM_BLEND_TIME_EPSILON) {
        blendTime = 0.0f;
    }

    XAnimEntry *entry = &tree->sourceTree->entries[animIndex];
    for (int32_t child = 0; child < entry->childCount; ++child) {
        XAnimClearGoalWeight(tree, entry->payload.parent.firstChildIndex + child, blendTime);
    }
}

/* Source: CoDUOMP.exe 0x0049bd10..0x0049bd97.
 * Name: same-module Mac symbol XAnimGetDescendantWithGreatestWeight. */
uint32_t XAnimGetDescendantWithGreatestWeight(uint32_t animIndex)
{
    XAnimEntry *entry = &xanim_currentTree->sourceTree->entries[animIndex];
    if (entry->childCount == 0) {
        return animIndex;
    }

    float greatestWeight = 0.0f;
    uint32_t greatestDescendant = 0;
    for (int32_t child = 0; child < entry->childCount; ++child) {
        uint32_t childIndex = entry->payload.parent.firstChildIndex + (uint32_t)child;
        uint16_t handle = xanim_currentTree->poolNodeHandles[childIndex];
        float weight = xanim_pool[handle].states[xanim_activePoolPayloadSlot].targetWeight;

        /* Windows 0x0049bd5c..0x0049bd72 and Linux
         * 0x080c0556..0x080c0563 both take this path for an unordered
         * comparison as well as for weight > greatestWeight. */
        if (!(weight <= greatestWeight)) {
            uint32_t descendant = XAnimGetDescendantWithGreatestWeight(childIndex);
            if (descendant != 0) {
                greatestWeight = weight;
                greatestDescendant = descendant;
            }
        }
    }
    return greatestDescendant;
}

/* Source: CoDUOMP.exe 0x0049bda0..0x0049bf32; coduo_lnxded
 * 0x080c05a2..0x080c0803.
 * Name: same-module Mac symbol XAnimSetGoalWeightInternal. */
int32_t XAnimSetGoalWeightInternal(uint32_t animIndex, float weight, float blendTime, float rate, bool create, uint16_t notifyName,
                                   uint16_t notifyType)
{
    uint16_t handle = xanim_currentTree->poolNodeHandles[animIndex];
    XAnimInfo *node;

    if (handle == 0) {
        if (weight == 0.0f && create == false) {
            return XANIM_GOAL_WEIGHT_RESULT_OK;
        }

        node = XAnimAllocInfo(xanim_currentTree, (uint32_t)animIndex);
        Com_Memset(node->states, 0, sizeof(node->states));
        node->notifyName = 0;
        node->notifyIndex = XANIM_NOTIFY_INDEX_NONE;
        node->notifyChildIndex = XANIM_NOTIFY_CHILD_NONE;
        node->notifyType = 0;
    } else {
        node = &xanim_pool[handle];
        if (xanim_activePoolPayloadSlot != 0 && node->notifyName != 0) {
            SL_RemoveRefToString(node->notifyName);
        }
    }

    if (animIndex == 0) {
        weight = 1.0f;
        blendTime = 0.0f;
        rate = 1.0f;
    }

    XAnimState *payload = &node->states[xanim_activePoolPayloadSlot];
    payload->targetWeight = weight;
    /* 0x0049be4f..0x0049be81 explicitly rounds the absolute difference to
     * float, then retains the following product across its field store and
     * epsilon comparison. */
    float signedWeightDifference = (float)((long double)weight - (long double)payload->currentWeight);
    uint32_t weightDifferenceBits;
    memcpy(&weightDifferenceBits, &signedWeightDifference, sizeof(weightDifferenceBits));
    weightDifferenceBits &= UINT32_C(0x7fffffff);
    float weightDifference;
    memcpy(&weightDifference, &weightDifferenceBits, sizeof(weightDifference));
#if defined(WINDOWS_BEHAVIOR)
    const long double blendRemainingRaw = (long double)weightDifference * (long double)blendTime;
    payload->weightBlendTimeRemaining = (float)blendRemainingRaw;
    if (blendRemainingRaw < (long double)XANIM_BLEND_TIME_EPSILON) {
        payload->weightBlendTimeRemaining = 0.0f;
        payload->currentWeight = weight;
#else
#if EMULATE_X87
    payload->weightBlendTimeRemaining = x87f_store_f32(x87f_mul(x87f_load_f32(weightDifference), x87f_load_f32(blendTime)));
#else
    payload->weightBlendTimeRemaining = (float)((long double)weightDifference * (long double)blendTime);
#endif
    if (payload->weightBlendTimeRemaining < XANIM_BLEND_TIME_EPSILON) {
        payload->weightBlendTimeRemaining = 0.0f;
        payload->currentWeight = weight;
#endif
    } else if (payload->currentWeight == 0.0f) {
        payload->currentWeight = weight * XANIM_BLEND_TIME_EPSILON;
    }
    payload->rateScale = rate;

    if (xanim_activePoolPayloadSlot == 0) {
        node->notifyType = notifyType;
        return XANIM_GOAL_WEIGHT_RESULT_OK;
    }

    node->notifyName = notifyName;
    if (notifyName != 0) {
        SL_AddRefToString(notifyName);
    }
    node->notifyIndex = XANIM_NOTIFY_INDEX_NONE;

    XAnimEntry *entry = &xanim_currentTree->sourceTree->entries[animIndex];
    if (notifyName == 0 || entry->childCount == 0 ||
        (entry->payload.parent.flags & (XANIM_PROPERTY_LOOP_SYNC | XANIM_PROPERTY_NON_LOOP_SYNC)) == 0) {
        node->notifyChildIndex = XANIM_NOTIFY_CHILD_NONE;
        return XANIM_GOAL_WEIGHT_RESULT_OK;
    }

    node->notifyChildIndex = (uint16_t)XAnimGetDescendantWithGreatestWeight(animIndex);
    return node->notifyChildIndex == XANIM_NOTIFY_CHILD_NONE ? XANIM_GOAL_WEIGHT_RESULT_NO_NOTIFY_DESCENDANT : XANIM_GOAL_WEIGHT_RESULT_OK;
}

/* Source: CoDUOMP.exe 0x0049c4a0..0x0049c4e0.
 * Name: same-module Mac symbol XAnimEnsureGoalWeightParent. */
void XAnimEnsureGoalWeightParent(uint32_t animIndex, float blendTime)
{
    while (animIndex != 0) {
        animIndex = xanim_currentTree->sourceTree->entries[animIndex].parentIndex;
        if (xanim_currentTree->poolNodeHandles[animIndex] != 0) {
            return;
        }

        XAnimSetGoalWeightInternal(animIndex, 0.0f, blendTime, 1.0f, true, 0, 0);
    }
}

/* Source: CoDUOMP.exe 0x0049c4f0..0x0049c55a.
 * Name: same-module Mac symbol XAnimSetGoalWeight. */
int32_t XAnimSetGoalWeight(XAnimTree *tree, uint32_t animIndex, float weight, float blendTime, float rate, uint16_t notifyName,
                           uint16_t notifyType, qboolean restart)
{
    xanim_currentTree = tree;
    if (weight < XANIM_BLEND_TIME_EPSILON) {
        weight = 0.0f;
    }

    int32_t result = XAnimSetGoalWeightInternal(animIndex, weight, blendTime, rate, false, notifyName, notifyType);
    XAnimEnsureGoalWeightParent(animIndex, blendTime);
    XAnimUpdateSyncTime(animIndex, restart);
    if (xanim_activePoolPayloadSlot != 0) {
        XAnimUpdateServerNotify(animIndex);
    }
    return result;
}

/* Source: CoDUOMP.exe 0x0049c5e0..0x0049c6a2.
 * Name: same-module Mac symbol XAnimSetCompleteGoalWeight. */
void XAnimSetCompleteGoalWeight(XAnimTree *tree, uint32_t animIndex, float weight, float blendTime, float rate, uint16_t notifyName,
                                uint16_t notifyType, qboolean restart)
{
    xanim_currentTree = tree;
    if (weight < XANIM_BLEND_TIME_EPSILON) {
        weight = 0.0f;
    }

    XAnimSetGoalWeightInternal(animIndex, weight, blendTime, rate, false, notifyName, notifyType);

    uint32_t parentIndex = animIndex;
    while (parentIndex != 0) {
        parentIndex = tree->sourceTree->entries[parentIndex].parentIndex;
        uint16_t handle = tree->poolNodeHandles[parentIndex];

        if (handle == 0 || xanim_pool[handle].states[xanim_activePoolPayloadSlot].targetWeight == 0.0f) {
            XAnimSetGoalWeightInternal(parentIndex, 1.0f, blendTime, 1.0f, false, 0, 0);
        }
    }

    XAnimUpdateSyncTime(animIndex, restart);
    if (xanim_activePoolPayloadSlot != 0) {
        XAnimUpdateServerNotify(animIndex);
    }
}

/* Source: CoDUOMP.exe 0x0049ad00..0x0049ad39.
 * Name: exact same-module Mac symbol DObjUpdateClientInfo. */
void DObjUpdateClientInfo(DObj *obj, float serverTime)
{
    xanim_deferredNotifyCount = 0;
    if (obj->runtimeTree == NULL) {
        return;
    }

    xanim_currentTree = obj->runtimeTree;
    xanim_evalRootHandle = obj->rootHandle;
    xanim_evalTime = serverTime;
    XAnimUpdateClientInfoInternal(0, serverTime, qtrue);
}

/* Source: CoDUOMP.exe 0x0049ad40..0x0049adda.
 * Name: exact same-module Mac symbol DObjUpdateServerInfo. */
qboolean DObjUpdateServerInfo(DObj *obj, float serverTime, qboolean notify)
{
    if (obj->runtimeTree == NULL) {
        return qfalse;
    }

    xanim_currentTree = obj->runtimeTree;
    xanim_evalRootHandle = obj->rootHandle;
    if (notify == qfalse) {
        XAnimUpdateServerInfoInternal(0, serverTime, qfalse);
        return qfalse;
    }

#if defined(WINDOWS_BEHAVIOR)
    xanim_notify_fraction_t notifyFraction = XAnimFindServerNoteTrack(0, serverTime);
    if (notifyFraction == 1.0f) {
        XAnimUpdateServerInfoInternal(0, serverTime, qtrue);
        return qfalse;
    }

    /* 0x0049ad95..0x0049ada3 stores the call argument as float, but compares
     * serverTime against the retained product-plus-epsilon value. */
    const long double notifyWindowEndRaw = (long double)notifyFraction * (long double)serverTime + (long double)XANIM_BLEND_TIME_EPSILON;
    float notifyWindowEnd = (float)notifyWindowEndRaw;
    if (!((long double)serverTime >= notifyWindowEndRaw)) {
        XAnimUpdateServerInfoInternal(0, serverTime, qtrue);
        return qfalse;
    }
#else
    float notifyFraction = XAnimFindServerNoteTrack(0, serverTime);
#if EMULATE_X87
    float notifyWindowEnd = x87f_store_f32(
        x87f_add(x87f_mul(x87f_load_f32(serverTime), x87f_load_f32(notifyFraction)), x87f_load_f32(XANIM_BLEND_TIME_EPSILON)));
#else
    float notifyWindowEnd = serverTime * notifyFraction + XANIM_BLEND_TIME_EPSILON;
#endif

    if (notifyFraction == 1.0f || !(serverTime >= notifyWindowEnd)) {
        XAnimUpdateServerInfoInternal(0, serverTime, qtrue);
        return qfalse;
    }
#endif

    XAnimUpdateServerInfoInternal(0, notifyWindowEnd, qtrue);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x0049ade0..0x0049adeb, recovered from an exporter
 * gap. Name: exact same-module Mac symbol DObjGetClientNotifyList. */
int32_t DObjGetClientNotifyList(xanim_deferred_notify_t **outNotifyList)
{
    *outNotifyList = xanim_deferredNotifies;
    return xanim_deferredNotifyCount;
}

/* Source: CoDUOMP.exe 0x0049ac90..0x0049ac92, recovered from an exporter gap.
 * Name: established by the matching reconstructed Linux engine routine. */
XAnimTree *XAnimGetRuntimeTree(DObj *obj)
{
    return obj->runtimeTree;
}

/* Source: CoDUOMP.exe 0x0049acd0..0x0049acf2, recovered from an exporter gap.
 * Name: exact same-module Mac symbol DObjInitServerTime. */
void DObjInitServerTime(DObj *obj, float serverTime)
{
    if (obj->runtimeTree == NULL) {
        return;
    }

    xanim_evalTime = serverTime;
    xanim_currentTree = obj->runtimeTree;
    XAnimUpdateServerInfoSyncInternal(0);
}

/* Source: CoDUOMP.exe 0x0049c1a0..0x0049c1fb.
 * Name: exact same-module Mac symbol XAnimUpdateServerNotify. */
void XAnimUpdateServerNotify(uint32_t animIndex)
{
    uint16_t handle = xanim_currentTree->poolNodeHandles[animIndex];
    XAnimInfo *node = &xanim_pool[handle];

    if (node->notifyName == 0) {
        return;
    }

    XAnimState *secondary = &node->states[XANIM_USER_SERVER];
    if (secondary->time == 1.0f) {
        node->notifyIndex = XANIM_NOTIFY_INDEX_NONE;
        return;
    }

    XAnimEntry *entry = &xanim_currentTree->sourceTree->entries[animIndex];
    if (entry->childCount != 0) {
        if (node->notifyChildIndex == XANIM_NOTIFY_CHILD_NONE) {
            return;
        }
        entry = &xanim_currentTree->sourceTree->entries[node->notifyChildIndex];
    }

    node->notifyIndex = (int16_t)XAnimGetNextNotifyTime(entry, node, secondary->time);
}

/* Source: CoDUOMP.exe 0x0049c200..0x0049c29b.
 * Name: same-module Mac symbol XAnimUpdateSyncTimeChildren. */
void XAnimUpdateSyncTimeChildren(uint32_t animIndex, XAnimInfo *source)
{
    XAnimEntry *entry = &xanim_currentTree->sourceTree->entries[animIndex];

    for (int32_t child = 0; child < entry->childCount; ++child) {
        uint32_t childIndex = entry->payload.parent.firstChildIndex + (uint32_t)child;
        uint16_t handle = xanim_currentTree->poolNodeHandles[childIndex];

        if (handle == 0) {
            continue;
        }

        XAnimCopyTimes(source, &xanim_pool[handle]);
        if (xanim_activePoolPayloadSlot != 0) {
            XAnimUpdateServerNotify(childIndex);
        }
        XAnimUpdateSyncTimeChildren(childIndex, source);
    }
}

/* Source: CoDUOMP.exe 0x0049c2a0..0x0049c49a.
 * Name: same-module Mac symbol XAnimUpdateSyncTime. */
void XAnimUpdateSyncTime(uint32_t animIndex, qboolean restart)
{
    uint32_t syncNodeIndex = animIndex;

    while (syncNodeIndex != XANIM_ROOT_NODE_INDEX) {
        uint16_t handle = xanim_currentTree->poolNodeHandles[syncNodeIndex];
        XAnimInfo *node = &xanim_pool[handle];
        XAnimEntry *entry = &xanim_currentTree->sourceTree->entries[syncNodeIndex];

        if (entry->childCount != 0 && (entry->payload.parent.flags & XANIM_PROPERTY_NOTIFY_SOURCE) != 0) {
            if (restart != qfalse || !XAnimHasEffectiveParentWeight(xanim_currentTree, (uint32_t)syncNodeIndex) ||
                !XAnimHasEffectiveChildWeight(xanim_currentTree, (uint32_t)syncNodeIndex)) {
                if (xanim_activePoolPayloadSlot == 0) {
                    XAnimInitServerTime(node);
                } else {
                    XAnimInitClientTime(node);
                }
            }

            XAnimUpdateSyncTimeChildren(animIndex, node);
            while (animIndex != syncNodeIndex) {
                uint16_t animHandle = xanim_currentTree->poolNodeHandles[animIndex];
                XAnimCopyTimes(node, &xanim_pool[animHandle]);
                if (xanim_activePoolPayloadSlot != 0) {
                    XAnimUpdateServerNotify(animIndex);
                }
                animIndex = xanim_currentTree->sourceTree->entries[animIndex].parentIndex;
            }
            return;
        }

        syncNodeIndex = entry->parentIndex;
    }

    XAnimEntry *entry = &xanim_currentTree->sourceTree->entries[animIndex];
    if (entry->childCount == 0 && (restart != qfalse || !XAnimHasEffectiveParentWeight(xanim_currentTree, (uint32_t)animIndex) ||
                                   !XAnimHasEffectiveChildWeight(xanim_currentTree, (uint32_t)animIndex))) {
        uint16_t handle = xanim_currentTree->poolNodeHandles[animIndex];
        if (xanim_activePoolPayloadSlot == 0) {
            XAnimInitServerTime(&xanim_pool[handle]);
        } else {
            XAnimInitClientTime(&xanim_pool[handle]);
        }
    }
}

/* Source: CoDUOMP.exe 0x0049c560..0x0049c580.
 * Name: same-module Mac symbol XAnimSetAnimRate. */
void XAnimSetAnimRate(XAnimTree *tree, uint32_t animIndex, float rate)
{
    xanim_currentTree = tree;
    uint16_t handle = tree->poolNodeHandles[animIndex];

    xanim_pool[handle].states[xanim_activePoolPayloadSlot].rateScale = rate;
}
