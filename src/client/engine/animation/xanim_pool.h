#ifndef CODUOMP_XANIM_POOL_H
#define CODUOMP_XANIM_POOL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "animation/xanim.h"
#include "animation/xanim_compat.h"

struct DObj_s;

extern XAnimInfo xanim_pool[XANIM_POOL_NODE_COUNT];
extern uint16_t xanim_endNotifyHandle;
extern int32_t xanim_poolUsedCount;
extern int32_t xanim_poolHighWaterCount;
extern XAnimTree *xanim_currentTree;
extern int32_t xanim_deferredNotifyCount;
extern xanim_deferred_notify_t
    xanim_deferredNotifies[XANIM_DEFERRED_NOTIFY_CAPACITY];

#endif
