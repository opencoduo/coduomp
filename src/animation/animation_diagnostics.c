#include "animation_private.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * The authoritative client and dedicated-server bodies retain the same tree
 * walk, branch graph, notetrack lookup, format strings, and print order:
 *
 *   CoDUOMP.exe  0x0049a460..0x0049a783
 *   coduo_lnxded 0x080be3ca..0x080be80e
 *   CoD UO MP PEF file offset 0x000ec3b0..0x000ec7b4
 *
 * Windows keeps the leaf time subtraction, wrap addition, and frequency
 * division live in x87 under the process PC=53 policy until the vararg double
 * store.  Linux stores each result to binary32 before the next operation under
 * its PC=64 policy.  The two complete bodies preserve that observable target
 * difference without splitting the common diagnostic subsystem.
 */
#if defined(WINDOWS_BEHAVIOR)
void XAnimDisplay(XAnimTree *tree, uint32_t animIndex, int32_t depth)
{
    XAnim *sourceTree = tree->sourceTree;
    XAnimEntry *entry = &sourceTree->entries[animIndex];
    uint16_t handle = tree->poolNodeHandles[animIndex];

    if (handle == 0) {
        return;
    }

    XAnimInfo *node = &xanim_pool[handle];
    XAnimState *payload = &node->states[xanim_activePoolPayloadSlot];

    if (payload->currentWeight == 0.0f) {
        return;
    }

    for (int32_t indent = 0; indent < depth; ++indent) {
        Com_Printf("  ");
    }

    if (entry->childCount == 0) {
        XAnimParts *record = entry->payload.leafAsset->data.xanimParts;
        long double realTimeDelta =
            (long double)payload->time - (long double)payload->oldTime;

        if (realTimeDelta < (long double)0.0f) {
            realTimeDelta += (long double)1.0f;
        }
        if (record->frequency != 0.0f) {
            realTimeDelta /= (long double)record->frequency;
        } else {
            realTimeDelta = (long double)0.0f;
        }

        if (xanim_activePoolPayloadSlot == 0 || node->notifyName == 0) {
            Com_Printf("(name) %s: (weight) %f -> %f, "
                       "(time) %f -> %f, (realtimedelta) %f\n",
                       entry->payload.leafAsset->name,
                       (double)payload->currentWeight,
                       (double)payload->targetWeight,
                       (double)payload->oldTime,
                       (double)payload->time,
                       (double)realTimeDelta);
        } else {
            float notifyWeight =
                node->notifyIndex < 0
                    ? 1.0f
                    : record->noteTracks[node->notifyIndex].time;

            Com_Printf("(name) %s: (weight) %f -> %f, "
                       "(time) %f -> %f, (realtimedelta) %f, "
                       "'%s' (%f)\n",
                       entry->payload.leafAsset->name,
                       (double)payload->currentWeight,
                       (double)payload->targetWeight,
                       (double)payload->oldTime,
                       (double)payload->time,
                       (double)realTimeDelta,
                       SL_ConvertToString(node->notifyName),
                       (double)notifyWeight);
        }
        return;
    }

    if (xanim_activePoolPayloadSlot == 0 || node->notifyName == 0) {
        if (XAnimHasTime(sourceTree, animIndex) == qfalse) {
            Com_Printf("(index) %d: (weight) %f -> %f\n", animIndex,
                       (double)payload->currentWeight,
                       (double)payload->targetWeight);
        } else {
            Com_Printf("(index) %d: (weight) %f -> %f, "
                       "(time) %f -> %f\n",
                       animIndex, (double)payload->currentWeight,
                       (double)payload->targetWeight,
                       (double)payload->oldTime,
                       (double)payload->time);
        }
    } else {
        const char *notifyName = SL_ConvertToString(node->notifyName);

        if (XAnimHasTime(sourceTree, animIndex) == qfalse) {
            Com_Printf("(index) %d: (weight) %f -> %f, '%s'\n",
                       animIndex, (double)payload->currentWeight,
                       (double)payload->targetWeight, notifyName);
        } else if (node->notifyChildIndex == 0) {
            Com_Printf("(index) %d: (weight) %f -> %f, "
                       "(time) %f -> %f, '%s'\n",
                       animIndex, (double)payload->currentWeight,
                       (double)payload->targetWeight,
                       (double)payload->oldTime,
                       (double)payload->time, notifyName);
        } else {
            XAnimEntry *sourceEntry =
                &sourceTree->entries[node->notifyChildIndex];
            XAnimParts *sourceRecord =
                sourceEntry->payload.leafAsset->data.xanimParts;
            float notifyWeight =
                node->notifyIndex < 0
                    ? 1.0f
                    : sourceRecord->noteTracks[node->notifyIndex].time;

            Com_Printf("(index) %d: (weight) %f -> %f, "
                       "(time) %f -> %f, '%s', (%f)\n",
                       animIndex, (double)payload->currentWeight,
                       (double)payload->targetWeight,
                       (double)payload->oldTime,
                       (double)payload->time, notifyName,
                       (double)notifyWeight);
        }
    }

    for (int32_t child = 0; child < entry->childCount; ++child) {
        XAnimDisplay(tree, entry->payload.parent.firstChildIndex + child,
                     depth + 1);
    }
}
#else
void XAnimDisplay(XAnimTree *tree, uint32_t animIndex, int32_t depth)
{
    XAnim *sourceTree = tree->sourceTree;
    XAnimEntry *entry = &sourceTree->entries[animIndex];
    uint16_t handle = tree->poolNodeHandles[animIndex];

    if (handle == 0) {
        return;
    }

    XAnimInfo *node = &xanim_pool[handle];
    XAnimState *payload = &node->states[xanim_activePoolPayloadSlot];

    if (payload->currentWeight == 0.0f) {
        return;
    }

    for (int32_t indent = 0; indent < depth; ++indent) {
        Com_Printf("  ");
    }

    if (entry->childCount == 0) {
        XAnimParts *record = entry->payload.leafAsset->data.xanimParts;
        float realTimeDelta = payload->time - payload->oldTime;

        if (realTimeDelta < 0.0f) {
            realTimeDelta += 1.0f;
        }
        if (record->frequency != 0.0f) {
            realTimeDelta /= record->frequency;
        } else {
            realTimeDelta = 0.0f;
        }

        if (xanim_activePoolPayloadSlot == 0 || node->notifyName == 0) {
            Com_Printf("(name) %s: (weight) %f -> %f, "
                       "(time) %f -> %f, (realtimedelta) %f\n",
                       entry->payload.leafAsset->name,
                       (double)payload->currentWeight,
                       (double)payload->targetWeight,
                       (double)payload->oldTime,
                       (double)payload->time,
                       (double)realTimeDelta);
        } else {
            float notifyWeight =
                node->notifyIndex < 0
                    ? 1.0f
                    : record->noteTracks[node->notifyIndex].time;

            Com_Printf("(name) %s: (weight) %f -> %f, "
                       "(time) %f -> %f, (realtimedelta) %f, "
                       "'%s' (%f)\n",
                       entry->payload.leafAsset->name,
                       (double)payload->currentWeight,
                       (double)payload->targetWeight,
                       (double)payload->oldTime,
                       (double)payload->time,
                       (double)realTimeDelta,
                       SL_ConvertToString(node->notifyName),
                       (double)notifyWeight);
        }
        return;
    }

    if (xanim_activePoolPayloadSlot == 0 || node->notifyName == 0) {
        if (XAnimHasTime(sourceTree, animIndex) == qfalse) {
            Com_Printf("(index) %d: (weight) %f -> %f\n", animIndex,
                       (double)payload->currentWeight,
                       (double)payload->targetWeight);
        } else {
            Com_Printf("(index) %d: (weight) %f -> %f, "
                       "(time) %f -> %f\n",
                       animIndex, (double)payload->currentWeight,
                       (double)payload->targetWeight,
                       (double)payload->oldTime,
                       (double)payload->time);
        }
    } else {
        const char *notifyName = SL_ConvertToString(node->notifyName);

        if (XAnimHasTime(sourceTree, animIndex) == qfalse) {
            Com_Printf("(index) %d: (weight) %f -> %f, '%s'\n",
                       animIndex, (double)payload->currentWeight,
                       (double)payload->targetWeight, notifyName);
        } else if (node->notifyChildIndex == 0) {
            Com_Printf("(index) %d: (weight) %f -> %f, "
                       "(time) %f -> %f, '%s'\n",
                       animIndex, (double)payload->currentWeight,
                       (double)payload->targetWeight,
                       (double)payload->oldTime,
                       (double)payload->time, notifyName);
        } else {
            XAnimEntry *sourceEntry =
                &sourceTree->entries[node->notifyChildIndex];
            XAnimParts *sourceRecord =
                sourceEntry->payload.leafAsset->data.xanimParts;
            float notifyWeight =
                node->notifyIndex < 0
                    ? 1.0f
                    : sourceRecord->noteTracks[node->notifyIndex].time;

            Com_Printf("(index) %d: (weight) %f -> %f, "
                       "(time) %f -> %f, '%s', (%f)\n",
                       animIndex, (double)payload->currentWeight,
                       (double)payload->targetWeight,
                       (double)payload->oldTime,
                       (double)payload->time, notifyName,
                       (double)notifyWeight);
        }
    }

    for (int32_t child = 0; child < entry->childCount; ++child) {
        XAnimDisplay(tree, entry->payload.parent.firstChildIndex + child,
                     depth + 1);
    }
}
#endif

/*
 * The client and dedicated-server bodies test the runtime-tree pointer,
 * display the root at depth zero, and print the same trailing newline:
 * CoDUOMP.exe 0x0049b040..0x0049b06b and coduo_lnxded
 * 0x080bf59a..0x080bf5e0.  The supporting PowerPC body is at PEF file offset
 * 0x000eb410..0x000eb460.
 */
void DObjDisplayAnim(const DObj *obj)
{
    if (obj->runtimeTree == NULL) {
        Com_Printf("NO TREE\n");
        return;
    }

    XAnimDisplay(obj->runtimeTree, 0, 0);
    Com_Printf("\n");
}

/*
 * Both authoritative bodies enumerate the same model and bone fields, decode
 * the same byte-pair trace-remap string, and issue the same print sequence:
 * CoDUOMP.exe 0x00493a00..0x00493b40 and coduo_lnxded
 * 0x080c53b6..0x080c5560.  The supporting PowerPC body is at PEF file offset
 * 0x000f9bd0..0x000f9d58.
 */
void DObjDumpInfo(const DObj *obj)
{
    if (obj == NULL) {
        Com_Printf("No Dobj\n");
        return;
    }

    Com_Printf("\nModels:\n");

    int32_t modelBoneBase = 0;
    for (int32_t modelIndex = 0; modelIndex < obj->modelCount;
         ++modelIndex) {
        const XModel *model = obj->models[modelIndex];

        Com_Printf("%d: '%s'\n", modelBoneBase, model->name);
        modelBoneBase += XModelNumBones(model);
    }

    Com_Printf("\nBones:\n");
    for (int32_t boneIndex = 0; boneIndex < obj->boneCount; ++boneIndex) {
        Com_Printf("Bone %d: '%s'\n", boneIndex,
                   DObjGetBoneName(obj, boneIndex));
    }

    if (obj->tracePartRemapHandle == 0) {
        Com_Printf("\nNo part duplicates.\n");
    } else {
        const DObjTracePartRemap *partRemap =
            (const DObjTracePartRemap *)SL_ConvertToString(
                obj->tracePartRemapHandle);

        Com_Printf("\nPart duplicates:\n");
        for (int32_t remapIndex = 0;
             partRemap->duplicatePairs[remapIndex] != 0;
             remapIndex += 2) {
            int32_t sourcePart =
                partRemap->duplicatePairs[remapIndex] - 1;
            int32_t targetPart =
                partRemap->duplicatePairs[remapIndex + 1] - 1;

            Com_Printf("%d ('%s') -> %d ('%s')\n", sourcePart,
                       DObjGetBoneName(obj, sourcePart), targetPart,
                       DObjGetBoneName(obj, targetPart));
        }
    }

    Com_Printf("\n");
}
