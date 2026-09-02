#include "../client_recovered.h"
#include "../globals.h"
#include "qcommon/qcommon_limits.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Source: uo_cgame_mp_x86.dll 0x30038e70..0x30039382
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30038e70_30039382.mcode
//
// CG_ConfigStringModified — refresh the cgame gameState and dispatch the changed
// config-string index supplied as command argument 1. The same-module PPC name
// agrees with the complete x86 behavior. The former weapon-position assignment
// was a forbidden size match; this routine performs no weapon-position math.

enum {
    CS_CLIENT_STATE_11 = 11,
    CS_HUD_STAT_14 = 14,
    CG_ASSET_SORT_2D = 5,
    CG_HUD_TEXT_CAPACITY = 255
};

void CG_ConfigStringModified(void)
{
    int32_t index;
    const char *value;

    trap_Argv(1, g_textScratchBuffer, (int32_t)sizeof(g_textScratchBuffer));
    index = coduo_crt_atoi(g_textScratchBuffer);
    cgame_syscall(CG_GET_GAME_STATE, (intptr_t)cg_gameState.stringOffsets);

    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        Com_ErrorMessage("CG_ConfigString: bad index: %i", index);
    }
    value = &cg_gameState.stringData[cg_gameState.stringOffsets[index]];

    if (index == CS_ITEMS) {
        CG_RegisterItems();
    } else if (index == CS_AMBIENT) {
        CG_ConfigString3Modified();
    } else if (index == CS_SERVERINFO) {
        CG_ParseServerinfo();
    } else if (index >= CS_CONFIGVALUE_NAMES && index < CS_CONFIGVALUE_VALUES + CS_CONFIGVALUE_COUNT) {
        CG_SetConfigValues();
    } else if (index == CS_TEAM_SCORE_AXIS) {
        cg_hudStat5Value = coduo_crt_atoi(value);
    } else if (index == CS_TEAM_SCORE_ALLIES) {
        cg_hudStat6Value = coduo_crt_atoi(value);
    } else if (index == CS_HUD_STAT_14) {
        cg_hudStat14Value = coduo_crt_atoi(value);
    } else if (index == CS_VOTE_TIME) {
        cg_voteTime = coduo_crt_atoi(value);
        if (cg_voteTime != 0) {
            cg_voteTime = coduo_int32_from_bits((uint32_t)cg_voteTime + (uint32_t)cgame_syscall(CG_MILLISECONDS));
        }
        cg_voteModified = qtrue;
    } else if (index == CS_VOTE_YES) {
        cg_voteYes = coduo_crt_atoi(value);
        cg_voteModified = qtrue;
    } else if (index == CS_VOTE_NO) {
        cg_voteNo = coduo_crt_atoi(value);
        cg_voteModified = qtrue;
    } else if (index == CS_VOTE_STRING) {
        const char *display = CG_TranslateMessage(value, "vote string");
        strncpy(cg_voteString, display, CG_HUD_TEXT_CAPACITY);
        cg_voteString[CG_HUD_TEXT_CAPACITY] = '\0';
    } else if (index == CS_TIMEOUT_TIME) {
        cg_timeoutEndTime = coduo_crt_atoi(value);
        if (cg_timeoutEndTime != 0) {
            cg_timeoutEndTime = coduo_int32_from_bits((uint32_t)cg_timeoutEndTime + (uint32_t)cgame_syscall(CG_MILLISECONDS));
        }
        cg_timeoutActive = 0;
    } else if (index == CS_TIMEOUT_STRING) {
        const char *display = CG_TranslateMessage(value, "timeout string");
        strncpy(cg_timeoutString, display, CG_HUD_TEXT_CAPACITY);
        cg_timeoutString[CG_HUD_TEXT_CAPACITY] = '\0';
    } else if (index == CS_FOGVARS) {
        CG_ParseFog();
    } else if (index >= CS_MODELS && index < CS_SOUNDS) {
        cg_gameModels[index - CS_MODELS] = CG_RegisterModel(value, 7);
    } else if (index >= CS_EFFECTS && index < CS_FX) {
        cg_effectDefs[index - CS_EFFECTS] = (uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)value);
    } else if (index >= CS_SHELLSHOCKS && index < CS_SHELLSHOCKS + CS_SHELLSHOCKS_COUNT) {
        shellshock_t *params;

        if (value[0] == '\0' || CG_ShellShockLoad(value) == 0) {
            return;
        }
        params = index == CS_SHELLSHOCKS ? &cg_consoleShellShock : &cg_shellShocks[index - CS_SHELLSHOCKS - 1];
        CG_SetShellShockParams(params);
    } else if (index >= CS_SCRIPTMENUS && index < CS_SCRIPTMENUS + CS_SCRIPTMENUS_COUNT) {
        CG_RegisterConfigStringMenu(index);
    } else if (index >= CS_STATUS_ICONS && index < CS_HEAD_ICONS) {
        trap_R_RegisterShaderNoMip(CG_ConfigString(index), CG_ASSET_SORT_2D);
    } else if (index >= CS_HEAD_ICONS && index < CS_LOCATIONS) {
        CG_RegisterMaterial(CG_ConfigString(index), CG_ASSET_SORT_2D);
    } else if (index >= CS_SHADERS && index < CS_SHADERS + CS_SHADERS_COUNT) {
        CG_RegisterConfigStringShader(index);
    } else if (index == CS_CLIENT_STATE_11) {
        CG_ConfigString11Modified();
    } else if (index == CS_WIND) {
        vec3_t direction;
        float intensity;
        char buffer[BIG_INFO_STRING];
        const char *const windString = CG_ConfigString(index);
        const size_t windStringLength = strlen(windString);

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if (windStringLength >= sizeof(buffer)) {
            Com_Error(ERR_DROP, "\x15"
                                "CG_ConfigStringModified: wind config string "
                                "exceeds BIG_INFO_STRING");
            return;
        }
        memcpy(buffer, windString, windStringLength + 1);
        if (sscanf(buffer, "%f %f %f %f", &direction[0], &direction[1], &direction[2], &intensity) != 4) {
            Com_Error(ERR_DROP, "\x15"
                                "CG_ConfigStringModified: invalid wind config "
                                "string");
            return;
        }
        trap_FX_SetWind(direction, intensity);
    }
}
