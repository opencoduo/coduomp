#include "animation_private.h"

/*
 * The retained client and dedicated-server bodies serialize the same eight
 * XAnimState fields in the same order and store every loaded float back to
 * binary32 immediately:
 *
 *   CoDUOMP.exe   0x0049c6b0..0x0049c81f
 *   coduo_lnxded  0x080c1340..0x080c141e
 *   CoD UO MP PEF 0x000e8e70..0x000e8ed4, 0x000e8f10..0x000e8f74
 *
 * The platform-specific ReadFloat return ABI is declared in the
 * private boundary above; assignment to these fields preserves each original
 * caller's immediate binary32 store.
 */
void XAnimLoadAnimState(XAnimState *state)
{
    state->time = ReadFloat();
    state->oldTime = ReadFloat();
    state->cycleCount = (int16_t)ReadShort();
    state->oldCycleCount = (int16_t)ReadShort();
    state->targetWeight = ReadFloat();
    state->rateScale = ReadFloat();
    state->weightBlendTimeRemaining = ReadFloat();
    state->currentWeight = ReadFloat();
}

void XAnimSaveAnimState(const XAnimState *state)
{
    WriteFloat(state->time);
    WriteFloat(state->oldTime);
    WriteShort((uint16_t)state->cycleCount);
    WriteShort((uint16_t)state->oldCycleCount);
    WriteFloat(state->targetWeight);
    WriteFloat(state->rateScale);
    WriteFloat(state->weightBlendTimeRemaining);
    WriteFloat(state->currentWeight);
}

/*
 * The node record order, optional-string discriminator, state-slot order, and
 * complete 0x44-byte clone agree in both authoritative binaries:
 *
 *   CoDUOMP.exe   0x0049c820..0x0049ca08
 *   coduo_lnxded  0x080c1420..0x080c155a
 *   CoD UO MP PEF 0x000e8bf0..0x000e8cb8, 0x000e8d10..0x000e8e38
 */
void XAnimLoadAnimInfo(XAnimInfo *node)
{
    node->notifyIndex = (int16_t)ReadShort();
    node->notifyChildIndex = ReadShort();
    node->notifyType = ReadShort();
    node->notifyName = ReadByte() != 0
        ? ReadString()
        : 0;

    XAnimLoadAnimState(&node->states[XANIM_USER_CLIENT]);
    XAnimLoadAnimState(&node->states[XANIM_USER_SERVER]);
}

void XAnimSaveAnimInfo(const XAnimInfo *node)
{
    WriteShort((uint16_t)node->notifyIndex);
    WriteShort(node->notifyChildIndex);
    WriteShort(node->notifyType);

    if (node->notifyName == 0) {
        WriteByte(0);
    } else {
        WriteByte(1);
        WriteString(node->notifyName);
    }

    XAnimSaveAnimState(&node->states[XANIM_USER_CLIENT]);
    XAnimSaveAnimState(&node->states[XANIM_USER_SERVER]);
}

void XAnimCloneAnimInfo(const XAnimInfo *source, XAnimInfo *dest)
{
    *dest = *source;
    if (dest->notifyName != 0) {
        SL_AddRefToString(dest->notifyName);
    }
}

/*
 * Both originals form the live-node bitset extent with wrapping 32-bit
 * arithmetic before allocating the temporary array.  Keeping the extent in a
 * uint32_t preserves that rule on 64-bit hosts.  The bit order, ascending node
 * walks, pool allocation/free behavior, and serialized node order also agree:
 *
 *   CoDUOMP.exe   0x0049ca10..0x0049ccd8
 *   coduo_lnxded  0x080c155c..0x080c1815
 *   CoD UO MP PEF 0x000e89d0..0x000e8bb4 (save/load support)
 */
void XAnimLoadAnimTree(XAnimTree *tree)
{
    uint32_t nodeCount = tree->sourceTree->nodeCount;
    uint32_t bitsetSize = ((nodeCount - 1U) >> 3) + 1U;
    uint8_t liveNodes[bitsetSize];

    ScriptLoad_ReadData(liveNodes, bitsetSize);
    for (uint32_t animIndex = 0; animIndex < nodeCount; ++animIndex) {
        if ((liveNodes[animIndex >> 3] &
             (uint8_t)(1U << (animIndex & 7U))) == 0) {
            continue;
        }

        XAnimInfo *node = XAnimAllocInfo(tree, animIndex);
        XAnimLoadAnimInfo(node);
    }
}

void XAnimSaveAnimTree(XAnimTree *tree)
{
    uint32_t nodeCount = tree->sourceTree->nodeCount;
    uint32_t bitsetSize = ((nodeCount - 1U) >> 3) + 1U;
    uint8_t liveNodes[bitsetSize];

    Com_Memset(liveNodes, 0, bitsetSize);
    for (uint32_t animIndex = 0; animIndex < nodeCount; ++animIndex) {
        if (tree->poolNodeHandles[animIndex] != 0) {
            liveNodes[animIndex >> 3] |=
                (uint8_t)(1U << (animIndex & 7U));
        }
    }

    ScriptSave_WriteData(liveNodes, bitsetSize);
    for (uint32_t animIndex = 0; animIndex < nodeCount; ++animIndex) {
        uint16_t handle = tree->poolNodeHandles[animIndex];
        if (handle != 0) {
            XAnimSaveAnimInfo(&xanim_pool[handle]);
        }
    }
}

void XAnimCloneAnimTree(XAnimTree *sourceTree, XAnimTree *destTree)
{
    int32_t nodeCount = (int32_t)sourceTree->sourceTree->nodeCount;

    for (int32_t animIndex = 0; animIndex < nodeCount; ++animIndex) {
        uint16_t sourceHandle = sourceTree->poolNodeHandles[animIndex];
        uint16_t destHandle = destTree->poolNodeHandles[animIndex];

        if (sourceHandle == 0) {
            if (destHandle != 0) {
                XAnimFreeInfo(destTree, destHandle);
                destTree->poolNodeHandles[animIndex] = 0;
            }
            continue;
        }

        XAnimInfo *destNode;
        if (destHandle == 0) {
            destNode = XAnimAllocInfo(destTree, (uint32_t)animIndex);
        } else {
            destNode = &xanim_pool[destHandle];
            XAnimClearServerInfo(destNode);
        }

        XAnimCloneAnimInfo(&xanim_pool[sourceHandle], destNode);
    }
}
