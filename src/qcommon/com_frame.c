#include "com_frame.h"

#include "q_cvar.h"
#include "qcommon_runtime_types.h"

#include <stdint.h>

enum {
    COM_HITCH_WARNING_THRESHOLD_MSEC = 500,
    COM_HITCH_WARNING_SANITY_LIMIT_MSEC = 500000,
    COM_MAX_FRAME_MSEC = 5000,
    COM_MAX_LISTEN_SERVER_FRAME_MSEC = 200,
    COM_MIN_SCALED_MSEC = 1
};

extern cvar_t *dedicated;
extern cvar_t *sv_running;

void Com_Printf(const char *format, ...);

/*
 * Both original engines reset a negative timescale to the string "0", then
 * either use fixedtime or multiply the integer input by the binary32
 * timescale without an intermediate store:
 *
 *   CoDUOMP.exe   0x0043c4c0
 *   coduo_lnxded  0x08071d30
 *
 * Windows lowers the final conversion through _ftol2 under PC=53; Linux uses
 * a truncating dword FISTP under PC=64.  That lowering difference is inert for
 * the intended inputs: Com_Frame supplies at most 5000 and SV_Frame supplies
 * 1000 / sv_fps (0..1000), so a finite in-range product needs at most 37
 * significant bits.  One source expression therefore preserves all ordinary
 * behavior without a platform split.  Out-of-range conversion and NaN are not
 * part of this frame-time interface's valid domain.
 */
int32_t Com_ModifyMsec(int32_t msec)
{
    if (com_timescale->value < 0.0f) {
        Cvar_Set("timescale", "0");
    }

    if (com_fixedtime->integer == 0) {
        msec = (int32_t)((long double)msec * (long double)com_timescale->value);
    } else {
        msec = com_fixedtime->integer;
    }

    if (msec < COM_MIN_SCALED_MSEC && com_timescale->value != 0.0f) {
        msec = COM_MIN_SCALED_MSEC;
    }
    return msec;
}

/*
 * The original Windows client and Linux dedicated-server bodies implement the
 * same clamp and hitch-reporting decisions:
 *
 *   CoDUOMP.exe   0x0043c530
 *   coduo_lnxded  0x08071dc1
 *
 * The same-module Mac client exports the canonical name Com_ClampMsec.
 */
int32_t Com_ClampMsec(int32_t msec)
{
    int32_t maximumMsec;

    if (dedicated->integer != 0) {
        if (msec > COM_HITCH_WARNING_THRESHOLD_MSEC && msec < COM_HITCH_WARNING_SANITY_LIMIT_MSEC) {
            Com_Printf("Hitch warning: %i msec frame time\n", msec);
        }
        maximumMsec = COM_MAX_FRAME_MSEC;
    } else if (sv_running->integer != 0) {
        maximumMsec = COM_MAX_LISTEN_SERVER_FRAME_MSEC;
    } else {
        maximumMsec = COM_MAX_FRAME_MSEC;
    }

    if (msec > maximumMsec) {
        return maximumMsec;
    }
    return msec;
}
