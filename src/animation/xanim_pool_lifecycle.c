#include "animation_private.h"

enum {
    XANIM_POOL_SENTINEL_INDEX = 0,
    XANIM_POOL_RESERVED_NODE_COUNT = 1,
    XANIM_NO_SCRIPT_VARIABLE_HANDLE = 0,
    XANIM_END_NOTIFY_STRING_USER = 0,
    XANIM_END_NOTIFY_STRING_TYPE = 3
};

/*
 * The authoritative Windows client and Linux dedicated-server bodies agree
 * on the complete pool lifecycle:
 *
 *   CoDUOMP.exe   0x00495a60..0x00495b15, 0x00495b20..0x00495b84
 *   coduo_lnxded  0x080b7d84..0x080b7eb8, 0x080b7eba..0x080b7ee3
 *
 * Both initialize the same circular 2048-record free list, clear both state
 * lanes in the reserved sentinel, intern "end" for string user 0/type 3, and
 * start both allocation counters at one.  Windows calls SL_GetStringOfLen
 * directly for the literal while Linux calls SL_GetString_; SL_GetString_
 * computes the same four-byte literal extent and delegates to
 * SL_GetStringOfLen, so the common spelling preserves the observable result.
 */
void XAnimInit(void)
{
    XAnimInfo *sentinel;

    for (int32_t index = 0; index < XANIM_POOL_NODE_COUNT; ++index) {
        xanim_pool[index].freePrev = (uint16_t)((index + XANIM_POOL_NODE_COUNT - 1) % XANIM_POOL_NODE_COUNT);
        xanim_pool[index].freeNext = (uint16_t)((index + 1) % XANIM_POOL_NODE_COUNT);
    }

    sentinel = &xanim_pool[XANIM_POOL_SENTINEL_INDEX];
    sentinel->states[XANIM_USER_CLIENT].time = 0.0f;
    sentinel->states[XANIM_USER_CLIENT].oldTime = 0.0f;
    sentinel->states[XANIM_USER_SERVER].time = 0.0f;
    sentinel->states[XANIM_USER_SERVER].oldTime = 0.0f;
    sentinel->states[XANIM_USER_CLIENT].cycleCount = 0;
    sentinel->states[XANIM_USER_CLIENT].oldCycleCount = 0;
    sentinel->states[XANIM_USER_SERVER].cycleCount = 0;
    sentinel->states[XANIM_USER_SERVER].oldCycleCount = 0;

    xanim_endNotifyHandle = SL_GetString_("end", XANIM_END_NOTIFY_STRING_USER, XANIM_END_NOTIFY_STRING_TYPE);
    xanim_poolUsedCount = XANIM_POOL_RESERVED_NODE_COUNT;
    xanim_poolHighWaterCount = XANIM_POOL_RESERVED_NODE_COUNT;
}

void XAnimShutdown(void)
{
    if (xanim_endNotifyHandle != XANIM_NO_SCRIPT_VARIABLE_HANDLE) {
        SL_RemoveRefToString(xanim_endNotifyHandle);
        xanim_endNotifyHandle = XANIM_NO_SCRIPT_VARIABLE_HANDLE;
    }
}
