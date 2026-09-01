#include "../client_recovered.h"

// Source: uo_cgame_mp_x86.dll 0x30036e50..0x30036ecb
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30036e50_30036ecb.mcode
//
// CG_ScoreboardHeight — measure the multiplayer scoreboard.
//
// The .mcode header's mechanical guess "turret_think_client" is REJECTED: it was
// assigned purely by matched byte size (0x7b == 0x7b), which the naming rules
// forbid. Behaviour + call graph prove this is scoreboard measurement, not turret
// logic:
//   - the sole caller (CG_DrawScoreboard body, 0x30037b50..0x30037b44) pushes the
//     scoreboard strings "score", "g_ScoresBanner_Allies/Axis/Spectators",
//     "hudScoreboardScroll_*", registers scoreboard shaders, and immediately after
//     the call compares 432.0 (screen height, 0x3007bdc0) minus this returned
//     height to decide whether the list needs scrolling;
//   - it reads the scoreboard-state globals cg_scoreboardNumClients (0x30489f20)
//     and cg_scoreboardTeamCount[] (0x30489f44), which the scoreboard build
//     routine at 0x30038000 fills (client count clamped to MAX_CLIENTS at
//     0x30038082; per-team counts incremented at 0x30038295
//     `inc [0x30489f44 + eax*4]`).
//
// ABI: the single argument (a pointer to the caller's line counter) arrives in
// EAX — the caller does `lea eax,[esp+0x10]` with no push (0x30037b94). The float
// height is returned on the x87 stack in ST0 (the caller does
// `fld 432.0 / fsub / fcompp` on it). RET has no immediate (no stack args to
// clean). These are i386 calling-convention details, recorded here, not modelled
// as attributes.

// Layout / row-height constants. In the machine code these are pooled read-only
// .rdata float operands loaded via FLD / FADD float ptr [addr]; here they read as
// the pixel dimensions they represent. Addresses are the constant-pool slots.
enum {
    SB_TEAM_BANNER_HEIGHT = 28, // 0x3007bdc8: added per free/spectator banner
    SB_ROW_HEIGHT         = 12, // 0x3007bdc4: added per player row
    SB_HEADER_HEIGHT      = 66, // 0x3007bdcc: header block when a team is present
    SB_EMPTY_HEIGHT       = 10, // 0x3007bda4: base height with no team header
    SB_HEADER_LINES       = 2,  // line count credited for the team header block
};

float CG_ScoreboardHeight(int32_t *lineCount)
{
    // 0x30036e50 FLD 10.0 ; 0x30036e56 MOV [EAX],0
    // Base height and line count assume no team-scores header.
    //
    // Float faithfulness: this function performs NO float store at all
    // (0x30036e50..0x30036eca contains not one FST/FSTP to memory). `height`
    // lives in ST0 from the initial FLD, accumulates purely via FADD in 80-bit,
    // and is returned raw in ST0. A `float` local would round at every `+=`;
    // long double keeps the chain 80-bit exactly as the bytes do. The `float`
    // RETURN type rounds once at `return` (the DLL returns raw ST0, consumed by
    // the sole caller's FCOMPP at 0x30037ba7 with no store), but `height` is
    // always a sum of exact small integers (10/66/28/12*n, all < 2^24), so that
    // rounding is provably a no-op -- the shared `float` decl is correct.
    long double height = (float)SB_EMPTY_HEIGHT;
    *lineCount = 0;

    // 0x30036e5c..0x30036e6e: if either scored team (AXIS or ALLIES) has any
    // rows, the scoreboard shows the team-scores header instead of the bare list.
    // 0x30036e70 FSTP ST0 discards the 10.0 base; 0x30036e78 FLD 66.0 replaces it.
    if (cg_scoreboardTeamCount[TEAM_AXIS] != 0 ||
        cg_scoreboardTeamCount[TEAM_ALLIES] != 0) {
        *lineCount = SB_HEADER_LINES;
        height = (float)SB_HEADER_HEIGHT;
    }

    // 0x30036e7e..0x30036e91: free-for-all players get their own banner section.
    if (cg_scoreboardTeamCount[TEAM_FREE] != 0) {
        height += (float)SB_TEAM_BANNER_HEIGHT;
        *lineCount = coduo_int32_from_bits((uint32_t)*lineCount + 1u);
    }

    // 0x30036e93..0x30036ea6: spectators get their own banner section.
    if (cg_scoreboardTeamCount[TEAM_SPECTATOR] != 0) {
        height += (float)SB_TEAM_BANNER_HEIGHT;
        *lineCount = coduo_int32_from_bits((uint32_t)*lineCount + 1u);
    }

    // 0x30036ea8..0x30036ec8: one row per collected client. The loop bound
    // cg_scoreboardNumClients is reloaded each iteration and the compare is
    // signed (TEST/JLE guard, CMP/JL back-edge), so a non-positive count draws
    // no rows.
    for (int32_t i = 0; i < cg_scoreboardNumClients; i++) {
        height += (float)SB_ROW_HEIGHT;
        *lineCount = coduo_int32_from_bits((uint32_t)*lineCount + 1u);
    }

    // 0x30036eca RET — height is left in ST0 as the return value.
    return height;
}
