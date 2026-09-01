#include "animation_private.h"

/* Sources: CoDUOMP.exe 0x00495a40 and coduo_lnxded 0x080b7d58.
 * Both bodies multiply the peak live-node count by the 0x44-byte XAnimInfo
 * stride. */
int32_t XAnimGetPoolHighWaterBytes(void)
{
    return xanim_poolHighWaterCount * (int32_t)sizeof(XAnimInfo);
}

/* Sources: CoDUOMP.exe 0x00495a50 and coduo_lnxded 0x080b7d6e.
 * Both bodies multiply the current live-node count by the same stride. */
int32_t XAnimGetPoolUsedBytes(void)
{
    return xanim_poolUsedCount * (int32_t)sizeof(XAnimInfo);
}

/* Sources: CoDUOMP.exe 0x0049c190 and coduo_lnxded 0x080c0c82.
 * Name: exact same-version Mac symbol XAnimSetUser. */
void XAnimSetUser(xanimUser_t user)
{
    xanim_activePoolPayloadSlot = user;
}
