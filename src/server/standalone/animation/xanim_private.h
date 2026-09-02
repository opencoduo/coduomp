#ifndef CODUO_XANIM_PRIVATE_H
#define CODUO_XANIM_PRIVATE_H

#include <stddef.h>
#include <string.h>

#include "server/standalone/bindings/coduo_engine_structs.h"
#include "scripting/script_anim.h"
#include "animation/xanim.h"
#include "animation/xanim_asset_load.h"
#include "animation/xanim_compat.h"
#include "animation/xanim_eval.h"

extern XAnimTree *xanim_currentTree;
extern int32_t xanim_deferredNotifyCount;
extern xanim_deferred_notify_t xanim_deferredNotifies[XANIM_DEFERRED_NOTIFY_CAPACITY];
extern uint16_t xanim_endNotifyHandle;
extern int16_t xanim_evalCurrentFrame;
extern float xanim_evalCurrentTime;
extern uint16_t xanim_evalRootHandle;
extern int16_t xanim_evalStartFrame;
extern float xanim_evalStartTime;
extern float xanim_evalTime;
extern float xanim_evalTimeStep;
extern int16_t xanim_evalWindowFrame;
extern float xanim_evalWindowTime;
extern XAnimInfo xanim_pool[XANIM_POOL_NODE_COUNT];
extern int32_t xanim_poolHighWaterCount;
extern int32_t xanim_poolUsedCount;
XAnim *Scr_GetAnims(uint32_t treeIndex);
const char *XAnimGetAnimTreeDebugName(XAnim *tree);

#endif
