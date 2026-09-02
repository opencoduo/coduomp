// Source: uo_cgame_mp_x86.dll 0x30037b50..0x30037d8d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30037b50_30037d8d.mcode
//
// CG_DrawScoreboardBody — draw the whole multiplayer scoreboard for one frame:
// the objective-info band, the localized column headers, the four team sections
// (the two scored teams in lead order, then free-for-all, then spectators), and
// the scroll indicators when the list overflows; finally it clamps the scroll
// position and resets the 2D draw color.
//
// NAME: the .mcode size-matched guess "CG_EntityPreEvent" is REJECTED — it was
// assigned purely because the Windows byte size (0x23d) is close to a PPC
// function's size (0x23c), which the naming rules forbid as evidence.
// CG_EntityPreEvent is entity-event dispatch (event bits, sound/effect spawns);
// this function does none of that. Its behaviour and call graph prove it owns the
// scoreboard-draw cluster:
//   * it registers the {1,1,1,alpha} draw color, calls
//     CG_DrawObjectiveInfo (0x30036900, "scoreboard objective info"),
//     CG_ScoreboardHeight (0x30036e50), CG_DrawScoreboard_ListColumnHeaders
//     (0x30036d60), CG_DrawScoreboardTeamHeader (0x30037090) and
//     CG_DrawScoreboard_ScoresList (0x30037810) — the entire scoreboard section
//     drawer family, all of which name THIS address as their sole caller;
//   * it reads/writes the scoreboard state globals cg_scoreboardOverflowed,
//     cg_scoreboardTeamScores[], cg_scoreboardTeamCount[], cg_scoreboardLeadTeam and
//     cg_scoreboardScrollPos.
// The name CG_DrawScoreboardBody is the role already used by every callee's
// reconstruction comment and by the client_recovered.h forward decl
// (void CG_DrawScoreboardBody(float fadeAlpha)); exact original CoD symbol
// unproven, so it is a proven-role name, not an RVA/size match.
//
// ABI (i386, evidence): one cdecl stack arg (fadeAlpha, [ESP+0x20] read before the
// callee-save pushes). No result. RET with immediate-less cleanup is via
// ADD ESP,0x24 at 0x30037d89 (the trailing trap-72 call's 8-byte cleanup folded
// into the frame teardown). EBX/ESI/EDI are callee-saved (pushed/popped).
//
// FLOAT constants (dumped exact via objdump -s -j .rdata):
//   0x3007bdc0 = 0x43d80000 = 432.0f  (screen/visible-area height)
//   0x3007be40 = 0x40800000 = 4.0f    (inter-section vertical gap)
//   immediates: 0x42500000 = 52.0f (objective-band starting Y),
//               0x42000000 = 32.0f (team-header banner height),
//               0x43bb0000 = 374.0f / 0x43bf0000 = 382.0f (board width:
//               narrower when a scrollbar is shown), 0x3f800000 = 1.0f.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* Starting Y coordinate of the objective-text band. */
#define CG_SB_OBJECTIVE_BAND_START_Y 52.0f
/* Banner height passed to each team-section header. */
#define CG_SB_TEAM_BANNER_HEIGHT   32.0f
/* Vertical gap added after each drawn section. */
#define CG_SB_SECTION_GAP          4.0f
/* Visible-area height; if the list is taller a scrollbar is shown. */
#define CG_SB_VISIBLE_HEIGHT       432.0f
/* Board width with / without the scrollbar column. */
#define CG_SB_WIDTH_SCROLLING      374.0f
#define CG_SB_WIDTH_FULL           382.0f

void CG_DrawScoreboardBody(float fadeAlpha)
{
    /* 0x30037b6c..0x30037b84: the shared draw color is {1,1,1,alpha}. */
    cgScoreboardDrawCtx_t drawCtx;
    vec_t *drawColor = drawCtx.color;
    drawCtx.color[0] = 1.0f;
    drawCtx.color[1] = 1.0f;
    drawCtx.color[2] = 1.0f;
    drawCtx.color[3] = fadeAlpha;

    /* 0x30037b66: clear the "list overflowed the visible area" latch for this
     * frame before any section drawer can set it. */
    cg_scoreboardOverflowed = 0;

    int scrollable = 0; /* 0x30037b63 XOR EDI,EDI (EDI = the scrollbar flag) */
    int lineCount;      /* 0x30037b94 LEA EAX,[lineCount] -> out-param below */

    /* 0x30037b88: draw the top objective-info band; it returns the Y at which the
     * scored list begins. */
    float startY = CG_DrawObjectiveInfo(&drawCtx,
                                                  CG_SB_OBJECTIVE_BAND_START_Y);

    /* 0x30037b98: measure the full list height and its line count. */
    float listHeight = CG_ScoreboardHeight(&lineCount);

    /* 0x30037b9d..0x30037bc7: if the list is taller than the space below the band
     * (432 - startY < listHeight), reserve a scrollbar column: use the narrower
     * board width and raise the scrollbar flag. Otherwise use the full width. */
    float boardWidth;
    if ((long double)CG_SB_VISIBLE_HEIGHT - (long double)startY <
        (long double)listHeight) {
        boardWidth = CG_SB_WIDTH_SCROLLING;
        scrollable = 1;
    } else {
        boardWidth = CG_SB_WIDTH_FULL;
    }

    /* 0x30037bcb..0x30037bde: add the inter-section gap to startY and draw the
     * localized column headers there. The returned value is the baseline just
     * below the header row, kept both as the running section Y and for the scroll
     * indicators. colorPtr is the {1,1,1,alpha} draw color. */
    float headerBottomY =
        CG_DrawScoreboard_ListColumnHeaders(
            (float)((long double)startY +
                    (long double)CG_SB_SECTION_GAP),
            boardWidth, drawColor);

    /* 0x30037be8/0x30037bef: the result is stored into both the running-Y slot
     * and the scroll-indicator baseline slot. Both stores round the same live
     * x87 return value to float. */
    float y = headerBottomY;
    int sectionLine = 0; /* 0x30037bf5 [ESP+0xc]=0: the per-list line counter */

    /* 0x30037bf3..0x30037c06: draw the two scored team sections only when either
     * has any rows. */
    if (cg_scoreboardTeamCount[TEAM_AXIS] != 0 ||
        cg_scoreboardTeamCount[TEAM_ALLIES] != 0) {
        /* 0x30037c0c..0x30037c2f: the team with the higher aggregate leads and is
         * drawn first; on an exact tie reuse the previously latched lead team so
         * the order is stable across frames. Then latch the result. */
        int leadTeam;
        if (cg_scoreboardTeamScores[TEAM_ALLIES] < cg_scoreboardTeamScores[TEAM_AXIS]) {
            leadTeam = TEAM_AXIS;
        } else if (cg_scoreboardTeamScores[TEAM_ALLIES] >
                   cg_scoreboardTeamScores[TEAM_AXIS]) {
            leadTeam = TEAM_ALLIES;
        } else {
            leadTeam = cg_scoreboardLeadTeam;
        }
        cg_scoreboardLeadTeam = leadTeam;

        /* 0x30037c33..0x30037c71: lead team section — header, then its score rows.
         * The section is followed by a 4.0f gap. */
        y = CG_DrawScoreboardTeamHeader(&drawCtx, y, boardWidth,
                                        CG_SB_TEAM_BANNER_HEIGHT, leadTeam,
                                        &sectionLine);
        y = (float)((long double)CG_DrawScoreboard_ScoresList(
                        &drawCtx, y, leadTeam, boardWidth, &sectionLine) +
                    (long double)CG_SB_SECTION_GAP);

        /* 0x30037c73..0x30037c84: the trailing team is the other scored team
         * (AXIS<->ALLIES). */
        int trailTeam = (leadTeam == TEAM_AXIS) ? TEAM_ALLIES : TEAM_AXIS;

        /* 0x30037c79..0x30037cb4: trailing team section, same shape. */
        y = CG_DrawScoreboardTeamHeader(&drawCtx, y, boardWidth,
                                        CG_SB_TEAM_BANNER_HEIGHT, trailTeam,
                                        &sectionLine);
        y = (float)((long double)CG_DrawScoreboard_ScoresList(
                        &drawCtx, y, trailTeam, boardWidth, &sectionLine) +
                    (long double)CG_SB_SECTION_GAP);
    }

    /* 0x30037cbb..0x30037d02: free-for-all section, drawn when it has rows. */
    if (cg_scoreboardTeamCount[TEAM_FREE] != 0) {
        y = CG_DrawScoreboardTeamHeader(&drawCtx, y, boardWidth,
                                        CG_SB_TEAM_BANNER_HEIGHT, TEAM_FREE,
                                        &sectionLine);
        y = (float)((long double)CG_DrawScoreboard_ScoresList(
                        &drawCtx, y, TEAM_FREE, boardWidth, &sectionLine) +
                    (long double)CG_SB_SECTION_GAP);
    }

    /* 0x30037d06..0x30037d49: spectator section, drawn when it has rows. Its
     * ScoresList result is discarded (0x30037d47 FSTP ST0): there is nothing
     * below it, so the running Y is not advanced past this section. */
    if (cg_scoreboardTeamCount[TEAM_SPECTATOR] != 0) {
        y = CG_DrawScoreboardTeamHeader(&drawCtx, y, boardWidth,
                                        CG_SB_TEAM_BANNER_HEIGHT, TEAM_SPECTATOR,
                                        &sectionLine);
        (void)CG_DrawScoreboard_ScoresList(&drawCtx, y, TEAM_SPECTATOR, boardWidth,
                                           &sectionLine);
    }

    /* 0x30037d4c..0x30037d67: when a scrollbar is needed, draw the scroll frame /
     * arrow indicators for the visible window (from the header baseline down),
     * sized to the full collected line count (0x30037d4e reloads lineCount).
     * 0x30037d58 loads EBX = sectionLine (the running visible-line cursor), passed
     * as the callee's EBX register arg (visibleLineCount). */
    if (scrollable) {
        CG_DrawScoreboard_ScrollIndicators(drawColor, headerBottomY, lineCount,
                                           sectionLine);
    }

    /* 0x30037d6a..0x30037d7f: clamp the scroll position so it can never point past
     * the last line (lineCount - 1). */
    int32_t lastLine = coduo_int32_from_bits((uint32_t)lineCount - 1u);
    if (cg_scoreboardScrollPos > lastLine) {
        cg_scoreboardScrollPos = lastLine;
    }

    /* 0x30037d7f..0x30037d89: reset the 2D draw color to opaque white
     * (cgame_syscall(0x48, 0) == trap_R_SetColor(NULL)). */
    trap_R_SetColor((const float *)0);
}
