#ifndef SHARED_ANIMATION_XANIM_H
#define SHARED_ANIMATION_XANIM_H

#include "compat/coduo_fp_platform.h"
#include "qcommon/q_shared_types.h"
#include "math/q_matrix_types.h"
#include "qcommon/script_types.h"
#include "qcommon/xanim_types.h"

#if defined(LINUX_BEHAVIOR) && EMULATE_X87
#include "compat/coduo_x87emu.h"
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int32_t xanim_activePoolPayloadSlot;

int32_t XAnimGetNumChildren(XAnim *tree, int32_t animIndex);
int32_t XAnimGetChildAt(XAnim *tree, int32_t animIndex, int32_t childIndex);
const char *XAnimGetAnimName(XAnim *tree, int32_t animIndex);
XAnim *XAnimRuntimeTreeSourceTree(XAnimTree *runtimeTree);
int32_t XAnimGetAnimTreeSize(XAnim *tree);
qboolean XAnimHasTime(XAnim *tree, int32_t animIndex);
qboolean XAnimIsPrimitive(XAnim *tree, int32_t animIndex);
qboolean XAnimIsLooped(XAnim *tree, int32_t animIndex);
qboolean XAnimNotetrackExists(XAnim *tree, int32_t animIndex, uint16_t notetrack);
/*
 * XAnimGetLength returns its unspilled x87 quotient.  Windows computes under
 * PC=53, which a binary64 carrier represents exactly.  Linux computes under
 * PC=64 and callers can consume the value before any narrowing store, so its
 * native and software-x87 carriers must retain the full result.
 */
#if defined(WINDOWS_BEHAVIOR)
typedef double xanim_length_t;
#elif EMULATE_X87
typedef x87f xanim_length_t;
#else
typedef long double xanim_length_t;
#endif
xanim_length_t XAnimGetLength(XAnim *tree, int32_t animIndex);
uint16_t XAnimGetNextNotifyTime(XAnimEntry *entry, XAnimInfo *node, float time);
/*
 * The Win32 x87 ABI carries these PC=53 results past the nominal float return
 * without a binary32 store.  Linux explicitly stores to binary32 in the
 * callee.  This source-level carrier preserves the observable distinction on
 * non-x87 targets while retaining the original numeric return on native x87.
 */
#if defined(WINDOWS_BEHAVIOR)
typedef double xanim_notify_fraction_t;
#else
typedef float xanim_notify_fraction_t;
#endif
xanim_notify_fraction_t XAnimGetNotifyFracLeaf(float notifyTime);
xanim_notify_fraction_t XAnimGetNotifyFracServer(XAnimInfo *node, XAnimEntry *entry);
int32_t XAnimGetPoolHighWaterBytes(void);
int32_t XAnimGetPoolUsedBytes(void);
void XAnimSetUser(xanimUser_t user);
void XAnimInit(void);
void XAnimShutdown(void);
void XAnimInitClientTime(XAnimInfo *node);
void XAnimInitServerTime(XAnimInfo *node);
void XAnimSetAnimRateInternal(uint32_t animIndex, float rate);
void XAnimSetTime(XAnimTree *tree, uint32_t animIndex, float time);
void XAnimCopyTimes(XAnimInfo *source, XAnimInfo *dest);
float XAnimGetTime(XAnimTree *tree, uint32_t animIndex);
float XAnimGetWeight(XAnimTree *tree, uint32_t animIndex);
qboolean XAnimHasFinished(XAnimTree *tree, uint32_t animIndex);
qboolean XAnimHasEffectiveParentWeight(XAnimTree *tree, uint32_t animIndex);
qboolean XAnimHasEffectiveChildWeight(XAnimTree *tree, uint32_t animIndex);
qboolean XAnimHasEffectiveWeight(XAnimTree *tree, uint32_t animIndex);
void XAnimUpdateOldServerTime(uint32_t animIndex);
qboolean XAnimUpdateOldServerTimeNoWeight(uint32_t animIndex);
void XAnimCopySecondaryTreeStateToPrimary(DObj *obj);
void XAnimClearTreeWeights(XAnimTree *tree, uint32_t animIndex);
void XAnimClearTree(XAnimTree *tree);
XAnimInfo *XAnimAllocInfo(XAnimTree *tree, uint32_t animIndex);
void XAnimClearServerInfo(XAnimInfo *node);
void XAnimClearServerNotify(XAnimInfo *node);
void XAnimFreeInfo(XAnimTree *tree, uint16_t handle);
void XAnimSetLeafNode(XAnim *tree, uint16_t nodeIndex, const char *animName);
void XAnimSetParentNode(XAnim *tree, uint16_t nodeIndex, const char *unusedName, uint16_t firstChildIndex, uint16_t childCount,
                        uint16_t flags);
XAnim *XAnimAllocTree(const char *name, uint32_t nodeCount, script_anim_tree_alloc_t alloc);
XAnimTree *XAnimAllocRuntimeTree(XAnim *sourceTree, script_anim_tree_alloc_t alloc);
void XAnimCopyRuntimeTree(XAnimTree *runtimeTree, void (*copy)(void *data, size_t size));
void *MT_AllocAnimTree(size_t size);
XAnimTree *Com_XAnimCreateTree(XAnim *sourceTree);
XAnimTree *Com_XAnimCreateSmallTree(XAnim *sourceTree);
void Com_XAnimFreeSmallTree(XAnimTree *runtimeTree);
void XAnimFillInSyncNodes_r(XAnim *tree, uint32_t animIndex, uint8_t requireLooping);
void XAnimSetupSyncNodes_r(XAnim *tree, uint32_t animIndex);
void XAnimSetupSyncNodes(XAnim *tree);
void XAnimLoadAnimState(XAnimState *state);
void XAnimSaveAnimState(const XAnimState *state);
void XAnimLoadAnimInfo(XAnimInfo *node);
void XAnimSaveAnimInfo(const XAnimInfo *node);
void XAnimCloneAnimInfo(const XAnimInfo *source, XAnimInfo *dest);
void XAnimLoadAnimTree(XAnimTree *tree);
void XAnimSaveAnimTree(XAnimTree *tree);
void XAnimCloneAnimTree(XAnimTree *sourceTree, XAnimTree *destTree);
int32_t XAnimFindByteKey(float frameFrac, const uint8_t *keys, int32_t keyCount, int32_t targetKey);
int32_t XAnimFindShortKey(float frameFrac, const uint16_t *keys, int32_t keyCount, int32_t targetKey);
void XAnimClearData(DObjAnimMat *part);
void XAnimDisplay(XAnimTree *tree, uint32_t animIndex, int32_t depth);

float XAnimGetAverageRateFrequency(uint32_t nodeIndex);
qboolean XAnimUpdateInfoNoWeightClient(uint32_t nodeIndex);
void XAnimAddClientNotify(XAnimInfo *node, XAnimEntry *entry, uint16_t nameHandle, float timeFrac, uint16_t notifyType);
void XAnimSetClientTime(XAnimInfo *node, XAnimEntry *entry, uint16_t notifyType);
void XAnimUpdateClientInfoSyncInternal(uint32_t nodeIndex, qboolean notify);
void XAnimUpdateClientInfoInternal(uint32_t nodeIndex, float delta, qboolean notify);
void XAnimUpdateServerInfoSyncInternal(uint32_t nodeIndex);
float XAnimGetServerNotifyFracSyncTotal(XAnimInfo *node, XAnimEntry *entry);
xanim_notify_fraction_t XAnimFindServerNoteTrack(uint32_t nodeIndex, float delta);
void XAnimProcessServerNotify(XAnimInfo *node, XAnimEntry *entry);
void XAnimProcessServerNotify_r(XAnimInfo *node, XAnimEntry *entry);
void XAnimStampSecondaryWindowStart(int32_t nodeIndex);
void XAnimUpdateServerInfoInternal(uint32_t nodeIndex, float delta, qboolean notify);

void XAnimClearGoalWeight(XAnimTree *tree, uint32_t animIndex, float blendTime);
void XAnimClearTreeGoalWeights_r(XAnimTree *tree, uint32_t animIndex, float blendTime);
void XAnimClearTreeGoalWeights(XAnimTree *tree, uint32_t animIndex, float blendTime);
void XAnimClearTreeGoalWeightsStrict(XAnimTree *tree, uint32_t animIndex, float blendTime);
void XAnimClearGoalWeightKnobInternal(XAnimTree *tree, uint32_t animIndex, float weight, float blendTime);
uint32_t XAnimGetDescendantWithGreatestWeight(uint32_t animIndex);
int32_t XAnimSetGoalWeightInternal(uint32_t animIndex, float weight, float blendTime, float rate, bool create, uint16_t notifyName,
                                   uint16_t notifyType);
void XAnimEnsureGoalWeightParent(uint32_t animIndex, float blendTime);
int32_t XAnimSetGoalWeight(XAnimTree *tree, uint32_t animIndex, float weight, float blendTime, float rate, uint16_t notifyName,
                           uint16_t notifyType, qboolean restart);
void XAnimSetCompleteGoalWeight(XAnimTree *tree, uint32_t animIndex, float weight, float blendTime, float rate, uint16_t notifyName,
                                uint16_t notifyType, qboolean restart);
void XAnimClearChildGoalWeights(XAnimTree *tree, uint32_t animIndex, float blendTime);
int32_t XAnimSetGoalWeightKnob(XAnimTree *tree, uint32_t animIndex, float weight, float blendTime, float rate, uint16_t notifyName,
                               uint16_t notifyType, qboolean restart);
void XAnimSetCompleteGoalWeightKnob(XAnimTree *tree, uint32_t animIndex, float weight, float blendTime, float rate, uint16_t notifyName,
                                    uint16_t notifyType, qboolean restart);
int32_t XAnimSetCompleteGoalWeightKnobAll(XAnimTree *tree, uint32_t animIndex, uint32_t knobIndex, float weight, float blendTime,
                                          float rate, uint16_t notifyName, uint16_t notifyType, qboolean restart);
void XAnimUpdateServerNotify(uint32_t animIndex);
void XAnimUpdateSyncTimeChildren(uint32_t animIndex, XAnimInfo *source);
void XAnimUpdateSyncTime(uint32_t animIndex, qboolean restart);
void XAnimSetAnimRate(XAnimTree *tree, uint32_t animIndex, float rate);

void DObjUpdateClientInfo(DObj *obj, float serverTime);
qboolean DObjUpdateServerInfo(DObj *obj, float serverTime, qboolean notify);
int32_t DObjGetClientNotifyList(xanim_deferred_notify_t **outNotifyList);
XAnimTree *XAnimGetRuntimeTree(DObj *obj);
void DObjInitServerTime(DObj *obj, float serverTime);

#ifdef __cplusplus
}
#endif

#endif
