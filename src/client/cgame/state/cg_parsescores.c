// Source: uo_cgame_mp_x86.dll 0x30038060..0x3003837a
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30038060_3003837a.mcode
//
// CG_ParseScores — consume the `scores` server-command arguments, rebuild the
// 64-row client scoreboard, and derive per-team row counts/average pings.
//
// The .mcode name `G_TouchTriggers` is rejected: this function reads argv[1..],
// fills the scoreboard globals, registers status icons, and performs no entity
// trigger/contact work.  Behavior plus the same-module PPC name CG_ParseScores
// identifies it directly.

#include "../client_recovered.h"
#include "../globals.h"

#include <string.h>

enum {
    CG_SCORE_MAX_CLIENTS = 64,
    CG_SCORE_FIRST_CLIENT_ARG = 4,
    CG_SCORE_ARGS_PER_CLIENT = 5,
    CG_SCORE_STATUS_FIRST = 1,
    CG_SCORE_STATUS_LAST = 16,
    CG_SCORE_STATUS_CONFIG_BASE = 21
};

void CG_ParseScores(void)
{
    int32_t clientArg;
    int32_t i;
    int32_t team;

    trap_Argv(1, g_textScratchBuffer, (int32_t)sizeof(g_textScratchBuffer));
    cg_scoreboardNumClients = coduo_crt_atoi(g_textScratchBuffer);
    if (cg_scoreboardNumClients > CG_SCORE_MAX_CLIENTS) {
        cg_scoreboardNumClients = CG_SCORE_MAX_CLIENTS;
    }

    memset(cg_scoreboardTeamScores, 0, sizeof(cg_scoreboardTeamScores));

    trap_Argv(2, g_textScratchBuffer, (int32_t)sizeof(g_textScratchBuffer));
    cg_scoreboardTeamScores[1] = coduo_crt_atoi(g_textScratchBuffer);
    trap_Argv(3, g_textScratchBuffer, (int32_t)sizeof(g_textScratchBuffer));
    cg_scoreboardTeamScores[2] = coduo_crt_atoi(g_textScratchBuffer);

    memset(cg_scoreboardEntries, 0, sizeof(cg_scoreboardEntries));
    memset(cg_scoreboardTeamPings, 0, sizeof(cg_scoreboardTeamPings));
    memset(cg_scoreboardTeamCount, 0, sizeof(cg_scoreboardTeamCount));

    clientArg = CG_SCORE_FIRST_CLIENT_ARG;
    for (i = 0; i < cg_scoreboardNumClients;
         i = coduo_int32_from_bits((uint32_t)i + 1u), clientArg = coduo_int32_from_bits((uint32_t)clientArg + CG_SCORE_ARGS_PER_CLIENT)) {
        int32_t client;
        int32_t status;

        trap_Argv(clientArg, g_textScratchBuffer, (int32_t)sizeof(g_textScratchBuffer));
        client = coduo_crt_atoi(g_textScratchBuffer);
        cg_scoreboardEntries[i].client = client;

        trap_Argv(coduo_int32_from_bits((uint32_t)clientArg + 1u), g_textScratchBuffer, (int32_t)sizeof(g_textScratchBuffer));
        cg_scoreboardEntries[i].score = coduo_crt_atoi(g_textScratchBuffer);

        trap_Argv(coduo_int32_from_bits((uint32_t)clientArg + 2u), g_textScratchBuffer, (int32_t)sizeof(g_textScratchBuffer));
        cg_scoreboardEntries[i].ping = coduo_crt_atoi(g_textScratchBuffer);

        trap_Argv(coduo_int32_from_bits((uint32_t)clientArg + 3u), g_textScratchBuffer, (int32_t)sizeof(g_textScratchBuffer));
        cg_scoreboardEntries[i].deaths = coduo_crt_atoi(g_textScratchBuffer);

        trap_Argv(coduo_int32_from_bits((uint32_t)clientArg + 4u), g_textScratchBuffer, (int32_t)sizeof(g_textScratchBuffer));
        status = coduo_crt_atoi(g_textScratchBuffer);
        cg_scoreboardEntries[i].statusIcon = status;

        if (status >= CG_SCORE_STATUS_FIRST && status <= CG_SCORE_STATUS_LAST) {
            const char *shaderName = CG_ConfigString(status + CG_SCORE_STATUS_CONFIG_BASE);
            CG_DrawInformation(0);
            cg_scoreboardEntries[i].statusIcon =
                coduo_int32_from_bits((uint32_t)cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)shaderName, 5));
        }

        if (client < 0 || client >= CG_SCORE_MAX_CLIENTS) {
            client = 0;
            cg_scoreboardEntries[i].client = 0;
        }

        /* 0x30038278 MOV [eax+0x305e1f68],edx: store this row's score
         * ([esi+0x4]) in clientInfo_t.score (+0x34), read by retail owner-draw
         * CG_PLAYER_SCORE (21). The server command producer at 0x20021864..82
         * supplies each row as clientNum, score, ping, deaths, statusIcon. */
        bgs.clientinfo[client].score = cg_scoreboardEntries[i].score;

        if (bgs.clientinfo[client].infoValid != 0) {
            team = bgs.clientinfo[client].team;
        } else {
            team = 0;
        }
        cg_scoreboardEntries[i].team = team;

        /* clientState_t.team is a two-bit snapshot netfield, so the direct
         * four-row aggregate lookup at 0x30038295/0x300382a2 has domain 0..3. */
        cg_scoreboardTeamCount[team] = coduo_int32_from_bits((uint32_t)cg_scoreboardTeamCount[team] + 1u);
        cg_scoreboardTeamPings[team] =
            coduo_int32_from_bits((uint32_t)cg_scoreboardTeamPings[team] + (uint32_t)cg_scoreboardEntries[i].ping);
    }

    for (team = 0; team < 4; team = coduo_int32_from_bits((uint32_t)team + 1u)) {
        if (cg_scoreboardTeamCount[team] >= 1 && cg_scoreboardTeamPings[team] >= 1) {
            cg_scoreboardTeamPings[team] /= cg_scoreboardTeamCount[team];
        } else {
            cg_scoreboardTeamPings[team] = 0;
        }
    }
}
