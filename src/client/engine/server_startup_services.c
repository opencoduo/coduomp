#include "server_startup_services.h"

#include "animation/dobj.h"
#include "client/cgame.h"
#include "client/cinematic.h"
#include "compat/coduo_int32_bits.h"
#include "compat/crt/random_compat.h"
#include "platform/crt_boundary.h"

#include <stdint.h>
#include <stdlib.h>

uint32_t Sys_Milliseconds(void);
void Com_Restart(void);

/* NOT_FROM_ORIGINAL_SOURCE: target adapter for the Windows client-owned map
 * transition calls embedded in SV_SpawnServer at 0x0045fb4d..0x0045fb54. */
void server_compat_begin_map_load(void)
{
    CL_SetupForNewServerMap();
    CL_ShutdownAll();
}

/* NOT_FROM_ORIGINAL_SOURCE: target adapter for the Windows cgame and UI
 * shutdown calls embedded between the two SV_ShutdownGameProgs calls. */
void server_compat_prepare_second_game_shutdown(void)
{
    CL_ShutdownCGame();
    CL_ShutdownUI();
}

/* NOT_FROM_ORIGINAL_SOURCE: target adapter for the Windows cinematic-table
 * walk embedded in SV_SpawnServer at 0x0045fba2..0x0045fbd8. */
void server_compat_finish_client_shutdown(void)
{
    for (int32_t handle = 0; handle < MAX_VIDEO_HANDLES; ++handle) {
        if (cinematics[handle].fileName[0] != '\0') {
            (void)CIN_StopCinematic(handle);
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: target adapter using the Windows server-random
 * domain and the original time, high-rand, low-rand evaluation order at
 * CoDUOMP.exe 0x0045fc5f..0x0045fcb5. */
int32_t server_compat_generate_checksum_feed(void)
{
    const uint32_t seed = (uint32_t)Sys_Milliseconds();
    uint32_t checksumTime;
    uint32_t checksumHigh;
    uint32_t checksumLow;

    srand(seed);
    checksumTime = (uint32_t)Sys_Milliseconds();
    checksumHigh = (uint32_t)coduo_server_rand() << 16;
    checksumLow = (uint32_t)coduo_server_rand();
    return coduo_int32_from_bits(checksumTime ^ checksumHigh ^ checksumLow);
}

/* NOT_FROM_ORIGINAL_SOURCE: target adapter for the Windows new-server DObj
 * initialization and common-engine restart sequence. */
void server_compat_initialize_new_server_pools(void)
{
    Com_InitDObj();
    Com_Restart();
}

/* NOT_FROM_ORIGINAL_SOURCE: target adapter for the Windows restart-path DObj
 * pool resets. */
void server_compat_initialize_restart_pools(void)
{
    Com_InitDObj();
    DObjInit();
}

/* NOT_FROM_ORIGINAL_SOURCE: target adapter preserving the signed-byte input
 * passed to the linked MSVC tolower by SV_InitCvar. */
int32_t server_compat_tolower_gametype_byte(char value)
{
    return coduo_crt_tolower((int8_t)(uint8_t)value);
}

/* NOT_FROM_ORIGINAL_SOURCE: the original Windows tail enters the retired
 * embedded PunkBuster backend, which is intentionally absent here. */
void server_compat_notify_punkbuster_state(qboolean enabled)
{
    (void)enabled;
}
