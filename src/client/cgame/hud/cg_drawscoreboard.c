// Source: uo_cgame_mp_x86.dll 0x30037d90..0x30037e1d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30037d90_30037e1d.mcode
//
// CG_DrawScoreboard — top-level multiplayer scoreboard draw for the CoD:UO cgame
// client. Returns qtrue when the scoreboard was drawn, qfalse otherwise.
//
// Naming: the .mcode's size-matched "YawVectors" guess is REJECTED. YawVectors is a
// vector-math routine; this function reads scoreboard-visibility state, computes a
// fade via CG_FadeColor, throttles a "score" client-command request, and drives two
// scoreboard drawers. It sits in the scoreboard cluster (CG_ScoreboardHeight
// 0x30036e50, CG_SetConfigValues 0x30038430) and is the top-level caller of the
// scoreboard body drawer at 0x30037b50 (which CG_ScoreboardHeight's own comment
// already calls "the CG_DrawScoreboard body"). Behavior matches the stock Quake3/CoD
// CG_DrawScoreboard: fade-in/out gating plus the every-2000ms trap_SendClientCommand
// ("score") update. Name proven by behavior + call graph.
//
// ABI: `PUSH ECX` reserves one dword local (the fade-alpha scale); no incoming stack
// arguments. Returns the qboolean in EAX (0/1). `RET` with no immediate (cdecl).

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

qboolean CG_DrawScoreboard(void)
{
    float fadeAlpha;   /* [ESP] local reserved by PUSH ECX: the alpha scale passed
                        * to the two scoreboard drawers */

    /* 0x30037d91: if the global draw-inhibit gate is set, draw nothing. */
    if (cl_paused_vmCvar.integer != 0) {
        return qfalse;                       /* 0x30037d98 JNZ -> XOR EAX,EAX; RET */
    }

    /* 0x30037d9a: the scoreboard-enable gate must be set to draw at all. */
    if (timescale_vmCvar.integer == 0) {
        return qfalse;                       /* 0x30037da1 JZ -> XOR EAX,EAX; RET */
    }

    /* 0x30037da3: if the scoreboard is currently showing, draw at full alpha;
     * otherwise fade it out over the last CG_FADE_TIME (100) ms from the time the
     * scoreboard state last changed (cg_scoreboardShowTime). */
    if (cg_scoreboardShowing != 0) {
        fadeAlpha = 1.0f;                    /* 0x30037dac MOV [ESP],0x3f800000 */
    } else {
        /* 0x30037dbb: EDX=cg_scoreboardShowTime (startMsec), ECX=100 (totalMsec) */
        vec_t *fade = CG_FadeColor(cg_scoreboardShowTime, CG_FADE_TIME);
        if (fade == 0) {
            /* 0x30037dc9: fully faded out -> empty the "fragged by" name string
             * (store 0 into cg_fraggedByName[0], AL==0) so the obituary line stops
             * drawing, and draw nothing. */
            cg_fraggedByName[0] = 0;
            return qfalse;                   /* 0x30037dce XOR EAX,EAX; RET */
        }
        /* 0x30037dd2: MOV EAX,[EAX] -> read fade[0] (the color's first component,
         * a 1.0 white channel), used as the alpha scale for the drawers. */
        fadeAlpha = fade[0];
    }

    /* 0x30037dd7: re-request "score" info from the server at most once every
     * 2000 ms. cgs_scoreboardTime holds cg_time of the last request; the compare is
     * the signed 32-bit `cmp (scoreboardTime + 2000), cg_time` with JGE skipping the
     * request, i.e. request only when cg_time - cgs_scoreboardTime > 2000. */
    if (coduo_int32_from_bits((uint32_t)cgs_scoreboardTime + 2000u) <
        coduo_int32_from_bits((uint32_t)cg_time)) {
        cgs_scoreboardTime = coduo_int32_from_bits((uint32_t)cg_time);
        cgame_syscall(CG_SEND_CLIENT_COMMAND, (intptr_t)"score");
    }

    /* 0x30037e01: draw the scoreboard, header first then body, both scaled by the
     * fade-alpha computed above. The dword local is forwarded to each drawer as a
     * float by value. */
    CG_DrawScoreboard_ListBanner(fadeAlpha);      /* 0x30037e07 CALL 0x300361d0 */
    CG_DrawScoreboardBody(fadeAlpha);        /* 0x30037e0d CALL 0x30037b50 */

    return qtrue;                            /* 0x30037e15 MOV EAX,1; RET */
}
