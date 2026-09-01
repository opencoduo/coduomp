// Source: uo_cgame_mp_x86.dll 0x3002ea30..0x3002ea73
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002ea30_3002ea73.mcode
// Small unbounded string-copy router used by the HUD trap-output buffers. The
// three bytes at 0x3002ea3d are a side-effect-free loop-alignment NOP.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void RouteHudString(char *text, qboolean rotateScratch)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (!rotateScratch) {
        Q_strncpyz(cg_trapStringBufferA, text, CG_HUD_STRING_BUFFER_SIZE);
        return;
    }

    Q_strncpyz(cg_trapStringBufferB, cg_hudEmitScratch, CG_HUD_STRING_BUFFER_SIZE);
    Q_strncpyz(cg_hudEmitScratch, text, CG_HUD_STRING_BUFFER_SIZE);
}
