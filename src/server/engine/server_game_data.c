#include "server_game_data.h"

#include "qcommon/qcommon_runtime_types.h"

#include <stddef.h>
#include <stdint.h>

void Com_Error(errorParm_t code, const char *format, ...);

/*
 * Complete server game-data address bridge shared by both engines.  Windows
 * CoDUOMP.exe retains the five address operations at
 * 0x0045c720..0x0045c7d7 and SV_LocateGameData at
 * 0x0045cdf0..0x0045ce14.  Linux coduo_lnxded retains the same operations at
 * 0x0808e0c8..0x0808e199 and 0x0808e898..0x0808e8c4.  The Linux stores prove
 * that the five values are separate original globals at
 * 0x084f6e9c..0x084f6eac, in the same order as the Windows globals.
 *
 * Byte arithmetic is intentional at this ABI boundary: the loaded game
 * module supplies the complete native strides of both records.
 */

void SV_LocateGameData(sharedEntity_t *gentities, int32_t numGentities,
                       int32_t sizeofGentity, playerState_t *clients,
                       int32_t sizeofGameClient)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (gentities == NULL || clients == NULL || numGentities < 0 ||
        numGentities > MAX_GENTITIES ||
        sizeofGentity < (int32_t)sizeof(sharedEntity_t) ||
        sizeofGameClient < (int32_t)sizeof(playerState_t)) {
        Com_Error(ERR_DROP,
                  "\x15" "SV_LocateGameData: invalid game data");
        return;
    }

    sv_gentities = gentities;
    sv_gentitySize = sizeofGentity;
    sv_numGentities = numGentities;
    sv_gameClients = clients;
    sv_gameClientSize = sizeofGameClient;
}

int32_t SV_NumForGentity(const sharedEntity_t *gentity)
{
    return (int32_t)(((const uint8_t *)gentity -
                      (const uint8_t *)sv_gentities) /
                     sv_gentitySize);
}

sharedEntity_t *SV_GentityNum(int32_t entityNum)
{
    return (sharedEntity_t *)((uint8_t *)sv_gentities +
                              (ptrdiff_t)entityNum * sv_gentitySize);
}

playerState_t *SV_GameClientNum(int32_t clientNum)
{
    return (playerState_t *)((uint8_t *)sv_gameClients +
                             (ptrdiff_t)clientNum * sv_gameClientSize);
}

svEntity_t *SV_SvEntityForGentity(const sharedEntity_t *gentity)
{
    if (gentity == NULL || gentity->entityState.number < 0 ||
        gentity->entityState.number >= MAX_GENTITIES) {
        Com_Error(ERR_DROP,
                  "\x15" "SV_SvEntityForGentity: bad gEnt");
    }
    return &sv_entities[gentity->entityState.number];
}

sharedEntity_t *SV_GEntityForSvEntity(const svEntity_t *serverEntity)
{
    return SV_GentityNum((int32_t)(serverEntity - sv_entities));
}
