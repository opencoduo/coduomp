// Source: uo_cgame_mp_x86.dll 0x30037810..0x300378a4
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30037810_300378a4.mcode
//
// CG_DrawScoreboard_ScoresList — draws every scoreboard score row that belongs
// to one team section, advancing (and returning) the running y position.
//
// Behavior proven from the machine code:
//   * The team color for this section is fetched once, up front, into a local
//     vec4: CG_DrawScoreboard_GetTeamColor(team, rowColor) fills rowColor[0..2]
//     (0x30037821 CALL 0x30036f20 with the color buffer in ESI), then
//     rowColor[3] is set to drawCtx->color[3] (+0xc, 0x3003782f/0x30037839).
//   * It then iterates cg_scoreboardEntries[0 .. cg_scoreboardNumClients-1]
//     (base 0x30489f54, stride 0x18 = sizeof(cgScore_t); count from
//     cg_scoreboardNumClients at 0x30489f20, a SIGNED loop bound — JLE/JL).
//   * For each entry it skips clients whose per-client anim/player state is not
//     live: EAX = entry->client; bgs.clientinfo[client].infoValid
//     (0x305e1f34 + client*0x4d0, +0x00) must be non-zero (0x30037858 TEST/JZ).
//   * It also skips entries whose entry->team (+0x10) != the requested team
//     (0x30037866 CMP [ESI+0x10],team / JNZ).
//   * A matching entry draws one client row: CG_DrawClientScore(&rowState,
//     rowColor, entry, rowScale, rowShade, y) returns the new y (FSTP into the
//     y slot, 0x30037881), and the alternating-shade flag toggles 0^1 each drawn
//     row (0x30037888 XOR EDI,0x1 — even/odd row background).
//   * Returns the final running y (FLD of the y slot at 0x30037899). When no
//     entry matched, that slot still holds the caller's incoming y, so y passes
//     through unchanged.
//
// Naming note: CG_DrawScoreboard_ScoresList / CG_DrawClientScore /
// CG_DrawScoreboard_GetTeamColor are role-proven from behavior and the call
// graph (invoked once per team section by CG_DrawScoreboardBody, 0x30037b50,
// which owns this scoreboard cluster). The names are corroborated by the
// same-module PPC name bank (CG_DrawScoreboard_ScoresList, CG_DrawClientScore,
// CG_DrawScoreboard_GetTeamColor); PPC RVAs do not map to these Windows RVAs, so
// they are anchors, not authority — behavior/call-graph evidence is the basis.
// Rejected the .mcode size-guess name "Item_CorrectedTextRect" (assigned only by
// a 0x94 byte-size match to a ui_mp/cgame corpus symbol): this function walks the
// cgame scoreboard score list and draws team rows, which has nothing to do with
// a UI item's text rectangle. Size matching is explicitly disallowed.
//
// ABI (i386, evidence): args are cdecl stack slots; RET (no imm) — the caller
// cleans the five pushed args (0x30037c6e ADD ESP,0x24 across this + the sibling
// CALL 0x30037090). EDI (alternating shade) is a local, initialized to 0.
// The float result is returned in ST0.

#include "client/cgame/client_recovered.h"

float CG_DrawScoreboard_ScoresList(const cgScoreboardDrawCtx_t *drawCtx, float y, int team, float rowScale, int *rowCounter)
{
    vec4_t rowColor;
    int alternateShade;
    int i;

    /* 0x30037821: team section color -> rowColor[0..2]; rowColor[3] = alpha. */
    CG_DrawScoreboard_GetTeamColor(team, rowColor);
    rowColor[3] = drawCtx->color[3];

    alternateShade = 0; /* 0x3003781f XOR EDI,EDI */

    /* 0x3003782a/0x30037896: signed row-count loop over the collected entries. */
    for (i = 0; i < cg_scoreboardNumClients; i = coduo_int32_from_bits((uint32_t)i + 1u)) {
        const cgScore_t *entry = &cg_scoreboardEntries[i];

        /* 0x30037852..0x30037860: only rows whose client anim/player state has
         * advanced (infoValid != 0) are live. CG_ParseScores has already
         * normalized the stored entry client to 0..63. */
        if (bgs.clientinfo[entry->client].infoValid == 0)
            continue;

        /* 0x30037866: only rows of the requested team section. */
        if (entry->team != team)
            continue;

        /* 0x3003787c: draw one client row; y advances by the returned amount. */
        y = CG_DrawClientScore(&rowColor[0], y, entry, rowScale, alternateShade, rowCounter);

        /* 0x30037888: alternate the row background shade on each drawn row. */
        alternateShade ^= 1;
    }

    return y; /* 0x30037899 FLD of the running-y slot */
}
