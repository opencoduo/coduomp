// Source: uo_cgame_mp_x86.dll 0x3003e2c0..0x3003e2cc

#include "client/cgame/client_recovered.h"

/* Set the renderer statistics output record (NULL stops tracking). The
 * original wrapper forwards stats (EAX) with syscall 0x6d
 * (CG_R_TRACK_STATISTICS). */
void trap_R_TrackStatistics(renderer_frame_statistics_t *stats)
{
    cgame_syscall(CG_R_TRACK_STATISTICS, (intptr_t)stats);
}
