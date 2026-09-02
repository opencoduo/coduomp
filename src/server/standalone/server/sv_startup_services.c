#include "server_startup_services.h"

#include "compat/coduo_ctype_compat.h"
#include "compat/coduo_int32_bits.h"
#include "compat/crt/random_compat.h"
#include "animation/dobj.h"
#include "../core_runtime/core_runtime_private.h"
#include "../punkbuster/pb_private.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>

/* NOT_FROM_ORIGINAL_SOURCE: target adapter for the dedicated engine's empty
 * client map-loading and shutdown boundaries in SV_SpawnServer. */
void server_compat_begin_map_load(void)
{
    CL_MapLoading();
    CL_ShutdownAll();
}

/* NOT_FROM_ORIGINAL_SOURCE: the dedicated target has no cgame or UI shutdown
 * between its two SV_ShutdownGameProgs calls. */
void server_compat_prepare_second_game_shutdown(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: the dedicated target has no client cinematic
 * table to drain after shutting down the game module. */
void server_compat_finish_client_shutdown(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: target adapter using the selected server-random
 * domain and high-rand, low-rand, time call order at
 * coduo_lnxded 0x08091d2c..0x08091d51. */
int32_t server_compat_generate_checksum_feed(void)
{
    const uint32_t seed = (uint32_t)Sys_Milliseconds();
    uint32_t checksumHigh;
    uint32_t checksumLow;
    uint32_t checksumTime;

    srand(seed);
    checksumHigh = (uint32_t)coduo_server_rand() << 16;
    checksumLow = (uint32_t)coduo_server_rand();
    checksumTime = (uint32_t)Sys_Milliseconds();
    return coduo_int32_from_bits(
        checksumTime ^ checksumHigh ^ checksumLow);
}

/* NOT_FROM_ORIGINAL_SOURCE: target adapter for the dedicated engine's
 * combined new-server runtime-pool initialization. */
void server_compat_initialize_new_server_pools(void)
{
    Com_InitServerRuntimePools();
}

/* NOT_FROM_ORIGINAL_SOURCE: target adapter for the dedicated restart-path
 * DObj pool reset. */
void server_compat_initialize_restart_pools(void)
{
    Com_InitDObj();
}

/* NOT_FROM_ORIGINAL_SOURCE: target adapter preserving the signed-byte input
 * passed to glibc tolower by SV_InitCvar. */
int32_t server_compat_tolower_gametype_byte(char value)
{
    return tolower(coduo_ctype_signed_byte_arg(value));
}

/* NOT_FROM_ORIGINAL_SOURCE: target adapter for the Linux dedicated
 * PunkBuster enabled/disabled notification boundary. */
void server_compat_notify_punkbuster_state(qboolean enabled)
{
    if (enabled != qfalse) {
        PB_NotifyServerEnabled();
    } else {
        PB_NotifyServerDisabled();
    }
}
