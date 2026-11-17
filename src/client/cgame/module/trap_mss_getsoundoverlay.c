// Source: uo_cgame_mp_x86.dll 0x3003f090..0x3003f0a6

#include "client/cgame/client_recovered.h"

/* Fill the sound debug overlay records; returns the record count. The original
 * wrapper forwards mode (stack), overlays (EDX), maxOverlays (ECX) and
 * cpuUsage (EAX) with syscall 0xde (CG_MSS_GET_SOUND_OVERLAY). */
int32_t trap_MSS_GetSoundOverlay(int32_t mode, mss_sound_overlay_t *overlays, int32_t maxOverlays, int32_t *cpuUsage)
{
    return (int32_t)cgame_syscall(CG_MSS_GET_SOUND_OVERLAY, mode, (intptr_t)overlays, maxOverlays, (intptr_t)cpuUsage);
}
