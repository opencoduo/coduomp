#include "xanim_pool.h"
#include "animation/xanim_eval.h"

/* CoDUOMP.exe-owned XAnim runtime storage.  The shared runtime subsystem
 * consumes these through the common declarations in animation_private.h. */
XAnimInfo xanim_pool[XANIM_POOL_NODE_COUNT]; /* 0x00b6ad10 */
uint16_t xanim_endNotifyHandle;
int32_t xanim_poolUsedCount;
int32_t xanim_poolHighWaterCount;
XAnimTree *xanim_currentTree;
float xanim_evalCurrentTime;
int16_t xanim_evalWindowFrame;
float xanim_evalWindowTime;
float xanim_evalTimeStep;
int16_t xanim_evalStartFrame;
float xanim_evalStartTime;
float xanim_evalTime;
int16_t xanim_evalCurrentFrame;
uint16_t xanim_evalRootHandle;
int32_t xanim_deferredNotifyCount;
xanim_deferred_notify_t xanim_deferredNotifies[XANIM_DEFERRED_NOTIFY_CAPACITY];
uint16_t xanim_rootTreeHandle;
int32_t xanim_evalPartCount;
uint32_t xanim_evalPartBits[DOBJ_PART_BITSET_WORD_COUNT];
uint32_t xanim_evalSkipBits[DOBJ_PART_BITSET_WORD_COUNT];
uint8_t xanim_evalLeafOutputMode;
int32_t xanim_evalPoolWeightSelector;
DObj *xanim_currentEvalState;
int32_t xanim_evalChildCount;
XModel **xanim_evalChildRefs;
size_t xanim_evalPartBytes;
