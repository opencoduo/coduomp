// Source: uo_cgame_mp_x86.dll 0x30031720..0x30031864
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30031720_30031864.mcode
//
// CG_OwnerDrawValue -- numeric-value callback installed in DC slot +0x58.
// It returns HUD values used by Item_OwnerDraw_Paint color ranges.  The switch
// and all integer-to-float conversions below come directly from x87 FILD/FIDIV.

#include "client/cgame/client_recovered.h"

enum cgOwnerDrawValueId {
    CG_ODV_LOCAL_HUD_VALUE = 4,
    CG_ODV_AMMO = 5,
    CG_PLAYER_SCORE = 21, /* Exact retail uo/menudef.h name; reads score at 0x30031824. */
    CG_ODV_HUD_STAT_DC = 27,
    CG_ODV_HUD_STAT_E0 = 28,
    CG_ODV_ITERATED_HUD_VALUE = 41,
    CG_ODV_CLIP = 70
};

float CG_OwnerDrawValue(int32_t ownerDraw, int32_t colorRangeType)
{
    int32_t clientNum = cg_snap->ps.psClientNum;

    switch (ownerDraw) {
    case CG_ODV_LOCAL_HUD_VALUE: {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
            Com_Error(ERR_DROP,
                      "\x15" "CG_OwnerDrawValue: invalid client number %i",
                      clientNum);
            return 0.0f;
        }
        clientInfo_t *state = &bgs.clientinfo[clientNum];
        return state->infoValid != 0 ? (float)state->health : 0.0f;
    }

    case CG_ODV_AMMO:
    case CG_ODV_CLIP: {
        int32_t weapon = cg_entities[clientNum].currentState.weapon;
        if (weapon == 0) {
            break;
        }
        weaponInfo_t *wi = bg_weaponInfos[weapon];
        if (ownerDraw == CG_ODV_AMMO) {
            int32_t index = wi->ammoIndex;
            int32_t value = cg_snap->ps.ammo[index];
            return colorRangeType == COLOR_RANGE_RELATIVE
                ? (float)((long double)value /
                          (long double)bg_ammoTypeMax[index])
                : (float)value;
        } else {
            int32_t index = wi->clipIndex;
            int32_t value = cg_snap->ps.clips[index];
            return colorRangeType == COLOR_RANGE_RELATIVE
                ? (float)((long double)value /
                          (long double)bg_ammoClipSizes[index])
                : (float)value;
        }
    }

    case CG_PLAYER_SCORE: {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
            Com_Error(ERR_DROP,
                      "\x15" "CG_OwnerDrawValue: invalid client number %i",
                      clientNum);
            return 0.0f;
        }
        clientInfo_t *state = &bgs.clientinfo[clientNum];
        return state->infoValid != 0 ? (float)state->score : 0.0f;
    }

    case CG_ODV_HUD_STAT_DC:
        return (float)cg_hudStat5Value;

    case CG_ODV_HUD_STAT_E0:
        return (float)cg_hudStat6Value;

    case CG_ODV_ITERATED_HUD_VALUE:
        if (cg_currentSelectedPlayer_vmCvar.integer < 0 || cg_currentSelectedPlayer_vmCvar.integer >= cg_hudEmitCount) {
            cg_currentSelectedPlayer_vmCvar.integer = 0;
        }
        int32_t selectedClient = cgame_compat_read_target_i32_index(
            cg_hudEmitClientTable,
            cg_currentSelectedPlayer_vmCvar.integer);
        clientInfo_t *state = cgame_compat_unchecked_clientinfo(
            &bgs.clientinfo[0], selectedClient);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (state->infoValid != 0) {
            return (float)state->health;
        }
        break;
    }

    return -1.0f;
}
