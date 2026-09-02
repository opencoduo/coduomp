#include "netchan.h"
#include "net_profile_services.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    NET_SHOWPROFILE_PACKETS = 2,
    NET_PROFILE_SAMPLE_WINDOW_MILLISECONDS = 1000,
    NET_PROFILE_RATE_UPDATE_MILLISECONDS = 100,
    NET_PROFILE_EMPTY_MIN_BYTES = 9999
};

static const float net_profileMillisecondsToSeconds = 0.0010000000474974513f;

void Com_Printf(const char *format, ...);
void Sys_OutOfMemory(void);
#if defined(WINDOWS_BEHAVIOR)
uint32_t Sys_Milliseconds(void);
#else
int32_t Sys_Milliseconds(void);
#endif

extern cvar_t *cl_running;
extern cvar_t *sv_running;

/*
 * Complete shared network-profiling core:
 *
 *                              Windows client       Linux dedicated
 * NetProf_PrepProfiling        0x0044cff0           0x08083884
 * NetProf_AddPacket            0x0044d0b0           0x08083973
 * NetProf_NewSendPacket        0x0044d110           0x080839f9
 * NetProf_NewRecievePacket     0x0044d160           0x08083a7a
 * NetProf_UpdateStatistics     0x0044d1c0           0x08083b00
 *
 * The retained binaries agree on allocation ownership, ring transitions,
 * sample aging, display strings, update timing, and byte-rate formula.  The
 * final rate expression remains live in x87 through its integer conversion.
 * WINDOWS_BEHAVIOR selects PC=53 and the low dword of `_ftol2`'s signed-qword
 * result; LINUX_BEHAVIOR selects PC=64 and the direct signed-dword FISTP.
 * EMULATE_X87 is independently usable for either target behavior.
 */

/*
 * The command body is the final common member of the profiling subsystem.
 * CoDUOMP.exe 0x0044d420 selects CL_Netchan_PrintProfileStats when the active
 * mode is client and SV_Netchan_PrintProfileStats otherwise.  The dedicated
 * body at coduo_lnxded 0x08083e34 has no client mode and directly selects the
 * server printer.  NET_PROFILE_DUMP_STATS keeps that target ownership at the
 * engine boundary without duplicating the command's common state/error path.
 */
void Net_DumpProfile_f(void)
{
    if (net_profileActiveMode == NET_PROFILE_OFF) {
        Com_Printf("Network profiling is not on. Set net_profile to turn on network profiling\n");
        return;
    }

    NET_PROFILE_DUMP_STATS();
}

void NetProf_PrepProfiling(netProfileInfo_t **profile)
{
    if (net_profile->integer != 0) {
        if (net_profileActiveMode == NET_PROFILE_OFF) {
            if (sv_running->integer == 0 || (cl_running->integer != 0 && net_profile->integer == NET_PROFILE_SERVER)) {
                net_profileActiveMode = NET_PROFILE_CLIENT;
            } else {
                net_profileActiveMode = NET_PROFILE_SERVER;
            }

            Com_Printf("Net Profiling turned on: %s\n", net_profileSocketNames[net_profileActiveMode - NET_PROFILE_CLIENT]);
        }

        if (*profile == NULL) {
            *profile = malloc(sizeof(**profile));
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (*profile == NULL) {
                Sys_OutOfMemory();
                return;
            }
            memset(*profile, 0, sizeof(**profile));
        }
        return;
    }

    if (net_profileActiveMode != NET_PROFILE_OFF) {
        net_profileActiveMode = NET_PROFILE_OFF;
        Com_Printf("Net Profiling turned off\n");
    }
    if (*profile != NULL) {
        free(*profile);
        *profile = NULL;
    }
}

void NetProf_AddPacket(netProfileStream_t *profile, int32_t length, qboolean fragmented)
{
    profile->ringIndex = (profile->ringIndex + 1) % NET_PROFILE_SAMPLE_COUNT;
    netProfileSample_t *sample = &profile->samples[profile->ringIndex];
    sample->time = (int32_t)Sys_Milliseconds();
    sample->bytes = length;
    sample->fragmented = fragmented;
}

void NetProf_NewSendPacket(netchan_t *channel, int32_t length, qboolean fragmented)
{
    if (net_profileActiveMode == NET_PROFILE_OFF) {
        return;
    }

    NetProf_AddPacket(&channel->profile->send, length, fragmented);
    if ((net_showprofile->integer & NET_SHOWPROFILE_PACKETS) != 0) {
        Com_Printf("%s send%s: %i\n", net_profileSocketNames[channel->sock], fragmented ? " fragment" : "", length);
    }
}

void NetProf_NewRecievePacket(netchan_t *channel, int32_t length, qboolean fragmented)
{
    if (net_profileActiveMode == NET_PROFILE_OFF) {
        return;
    }

    NetProf_AddPacket(&channel->profile->receive, length, fragmented);
    if ((net_showprofile->integer & NET_SHOWPROFILE_PACKETS) != 0) {
        Com_Printf("%s recieve%s: %i\n", net_profileSocketNames[channel->sock], fragmented ? " fragment" : "", length);
    }
}

void NetProf_UpdateStatistics(netProfileStream_t *profile)
{
    int32_t sampleCount = 0;
    int32_t fragmentSampleCount = 0;
    int32_t oldestSampleIndex = -1;
    int32_t oldestSampleTime = (int32_t)Sys_Milliseconds();
    int32_t totalBytes = 0;
    int32_t minBytes = NET_PROFILE_EMPTY_MIN_BYTES;
    int32_t maxBytes = 0;

    for (int32_t index = 0; index < NET_PROFILE_SAMPLE_COUNT; ++index) {
        const netProfileSample_t *sample = &profile->samples[index];
        if (sample->time == 0 || (int32_t)Sys_Milliseconds() > sample->time + NET_PROFILE_SAMPLE_WINDOW_MILLISECONDS) {
            continue;
        }

        ++sampleCount;
        if (sample->fragmented != qfalse) {
            ++fragmentSampleCount;
        }
        if (sample->time < oldestSampleTime) {
            oldestSampleIndex = index;
            oldestSampleTime = sample->time;
        }
        totalBytes += sample->bytes;
        if (sample->bytes < minBytes) {
            minBytes = sample->bytes;
        }
        if (sample->bytes > maxBytes) {
            maxBytes = sample->bytes;
        }
    }

    if (sampleCount == 0) {
        profile->bytesPerSecond = 0;
        profile->lastRateCalcTime = 0;
        profile->sampleCount = 0;
        profile->fragmentSampleCount = 0;
        profile->fragmentPercent = 0;
        profile->maxBytes = 0;
        profile->minBytes = 0;
        return;
    }

    profile->fragmentPercent = fragmentSampleCount != 0 ? fragmentSampleCount * 100 / sampleCount : 0;
    profile->maxBytes = maxBytes;
    profile->minBytes = minBytes;

    if (profile->lastRateCalcTime + NET_PROFILE_RATE_UPDATE_MILLISECONDS < (int32_t)Sys_Milliseconds()) {
        const int32_t elapsedMilliseconds = (int32_t)Sys_Milliseconds() - oldestSampleTime;

        if (oldestSampleIndex != -1) {
            const netProfileSample_t *oldestSample = &profile->samples[oldestSampleIndex];
            totalBytes -= oldestSample->bytes;
            --sampleCount;
            if (oldestSample->fragmented != qfalse) {
                --fragmentSampleCount;
            }
        }

        if (elapsedMilliseconds < 1 || sampleCount == 0 || totalBytes == 0) {
            profile->bytesPerSecond = 0;
        } else {
#if EMULATE_X87
            const x87f rate = x87f_div(x87f_load_i32(totalBytes),
                                       x87f_mul(x87f_load_i32(elapsedMilliseconds), x87f_load_f32(net_profileMillisecondsToSeconds)));
#if defined(WINDOWS_BEHAVIOR)
            const int64_t converted = x87f_store_i64_trunc(rate);
            profile->bytesPerSecond = coduo_int32_from_bits((uint32_t)(uint64_t)converted);
#else
            profile->bytesPerSecond = x87f_store_i32_trunc(rate);
#endif
#else
            profile->bytesPerSecond = coduo_fp_to_i32_extended(
                (long double)totalBytes / ((long double)elapsedMilliseconds * (long double)net_profileMillisecondsToSeconds));
#endif
        }
        profile->lastRateCalcTime = (int32_t)Sys_Milliseconds();
    }

    profile->sampleCount = sampleCount;
    profile->fragmentSampleCount = fragmentSampleCount;
}
