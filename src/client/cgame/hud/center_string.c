// Source: uo_cgame_mp_x86.dll 0x300191b0..0x30019361
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300191b0_30019361.mcode
//
// CG_DrawCenterString — draw the queued center-screen HUD message.
//
// Naming: the .mcode's mechanical pre-hint "script_method_player_setclientcvar"
// (a game_mp_uo SERVER GSC script method, matched only by byte size 0x1b1/0x1b2)
// is REJECTED. This is client cgame code: it has NO GSC/script-VM interaction and
// touches no cvar. It reads the cg.centerPrint* HUD state block written by
// CG_PriorityCenterPrint (0x30019050) — cg_centerPrintTime/String/Y/CharWidth/Lines — fades
// it via CG_FadeColor, and paints each wrapped line with the 2D text traps. The
// same-module PPC bank lists cgame_mp.dll!CG_DrawCenterString, and the body is the
// stock Quake3/CoD center-string drawer (fade gate + per-line centering + draw).
// Name proven by behavior + call graph (it is the draw-side counterpart of the
// already-reconstructed producer CG_PriorityCenterPrint).
//
// Trap ids (first arg to cgame_syscall, all through *0x30085e9c), resolved by role
// and matching the shared cgameSyscallId_t domain:
//   0x48 CG_R_SETCOLOR     : set the 2D draw color (a vec4 ptr, or NULL to reset)
//   0x35 CG_R_TEXT_HEIGHT        : (53, 0, scale) -> int line/char height for `scale`
//   0x34 CG_R_TEXT_WIDTH        : (52, string, 0, scale, 0) -> int pixel width of `string`
//   0x36 CG_R_TEXT_PAINT        : (54, x, y, 0, scale, color, string, 0, 0, 3) -> draw text
// The two metric traps return ints that the code FILDs back to float; the draw trap
// is the 2D text/draw service identified by CoDUOMP.exe's recovered dispatcher.
//
// x87: raw FILD/FMUL dependency chains use the project's long-double carrier
// convention; each FST/FSTP dword boundary is represented by a float local before
// its exact bits are forwarded to a variadic trap.
//
// ABI: SUB ESP,0x420 local frame; EBX/ESI/EBP/EDI are callee-saved registers (their
// PUSH/POP are register saves, not arguments). No incoming stack arguments; `RET`
// with no immediate (cdecl, no args). The /GS stack cookie handling (save
// __security_cookie to the top of frame at entry, __security_check_cookie at exit)
// is an i386 build detail, not source-level behavior, and is omitted.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

/* Per-line temporary buffer: the copy loop takes at most 75 (0x4b) characters of a
 * line before forcing a break (0x300192b0 CMP EAX,0x4b / JL), then NUL-terminates.
 * The frame reserves the buffer at [ESP+0x2c]; 128 bytes is ample for 75 chars + NUL. */
enum {
    CG_CENTERSTRING_MAX_LINE_CHARS = 75
}; /* 0x4b: max glyphs copied per line */

void CG_DrawCenterString(void)
{
    const char *s;
    vec_t *color;
    int32_t fadeMs;
    int32_t charHeight; /* trap-53 result, FILD'd without a binary32 spill */
    float yBaseline;    /* running vertical position for the current line */
    float lineStep;     /* per-line vertical advance = 1.2 * charHeight */
    float scale;        /* text scale = cg_centerPrintCharWidth * (1/32) */

    /* 0x300191bd: nothing queued -> nothing to draw. (Also the shared exit path
     * below runs the /GS check and returns.) */
    if (cg_centerPrintTime == 0) {
        return;
    }

    /* 0x300191d4: fade duration in ms = truncate(cg_centertime_vmCvar.value * 1000).
     * 0x300191ea: CG_FadeColor(startMsec = cg_centerPrintTime, totalMsec = fadeMs)
     * returns the static fade color, or NULL once the message has fully expired. */
    fadeMs = coduo_fp_to_i32_extended((long double)cg_centertime_vmCvar.value * (long double)1000.0f);
    color = CG_FadeColor(cg_centerPrintTime, fadeMs);
    if (color == NULL) {
        /* 0x300191f7: expired -> clear the queue and stop. */
        cg_centerPrintTime = 0;
        cg_centerPrintPriority = 0;
        return;
    }

    /* 0x30019217: apply the fade color to following 2D draws. */
    cgame_syscall(CG_R_SETCOLOR, (intptr_t)color);

    /* 0x30019220: text scale = cg_centerPrintCharWidth * 0.03125
     * (1/32, 0x3007bf3c). The integer enters the FMUL directly via FILD. */
    scale = (float)((long double)cg_centerPrintCharWidth * (long double)0.03125f);

    s = cg_centerPrintString; /* 0x30019226 ESI = &cg_centerPrintString[0] */

    /* 0x30019239: line/char height for this scale. trap(53, 0, scale) -> int. */
    charHeight = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_R_TEXT_HEIGHT, 0, CG_FloatBits(scale)));

    /* 0x30019256..0x3001928b: vertical start, centered on the total block height.
     *   yBaseline = cg_centerPrintY - 0.5*cg_centerPrintLines*charHeight - charHeight
     *   lineStep  = 1.2 * charHeight
     * (FILD cg_centerPrintY; FILD charHeight; FMUL 0.5; FIMUL cg_centerPrintLines;
     *  FSUBP; FSUB charHeight; -> yBaseline. FMUL 1.2 (0x3007bf38) -> lineStep.) */
    yBaseline = (float)((long double)cg_centerPrintY - ((long double)charHeight * (long double)0.5f) * (long double)cg_centerPrintLines -
                        (long double)charHeight); /* FILD cg_centerPrintY (0x30019256) / FIMUL
                               * cg_centerPrintLines (0x30019276): both integers
                               * enter the chain exact, no float round/cast. */
    lineStep = (float)((long double)charHeight * (long double)1.2f);

    /* 0x30019290: draw each line. */
    for (;;) {
        char line[128];
        int32_t n;               /* glyphs copied into `line` */
        int32_t textWidth;       /* trap-52 pixel width of `line` */
        float centeredX;         /* (640 - textWidth) * 0.5 */

        /* 0x30019290..0x300192b3: copy up to 75 chars of the current line, stopping
         * at NUL or '\n'. */
        n = 0;
        while (s[n] != '\0' && s[n] != '\n') {
            line[n] = s[n];
            n = coduo_int32_from_bits((uint32_t)n + 1u);
            if (n >= CG_CENTERSTRING_MAX_LINE_CHARS) {
                break;
            }
        }
        line[n] = '\0';          /* 0x300192c0 MOV [ESP+EAX+0x38],BL */

        /* 0x300192cf: pixel width of this line. trap(52, line, 0, scale, 0). */
        textWidth = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_R_TEXT_WIDTH, (intptr_t)line, 0, CG_FloatBits(scale), 0));

        /* 0x300192ed..0x300192fa: horizontal center. centeredX = (640 - width)*0.5.
         * (FILD width (0x300192dd); FSUBR 640.0 (0x3007bf34); FMUL 0.5
         * (0x3007bce8).) width is FILD'd straight into the FSUBR, no float cast. */
        centeredX = (float)((640.0L - (long double)textWidth) * 0.5L);

        /* 0x3001930d: draw the line.
         * trap(54, centeredX, yBaseline, 0, scale, color, line, 0, 0, 3). */
        cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(centeredX), CG_FloatBits(yBaseline), 0, CG_FloatBits(scale), (intptr_t)color,
                      (intptr_t)line, 0, 0, 3);

        /* 0x30019313: advance to the next line's baseline. */
        yBaseline = (float)((long double)yBaseline + (long double)lineStep);

        /* 0x3001931b..0x30019332: skip the just-drawn line's remaining characters.
         * If it began with NUL -> done. If it began with '\n' -> handled below.
         * Otherwise walk forward until the next '\n' or NUL. */
        if (*s == '\0') {
            break;
        }
        if (*s != '\n') {
            while (*s != '\0') {
                s++;
                if (*s == '\n') {
                    break;
                }
            }
        }

        /* 0x30019357: at a '\n' (or the walk stopped at NUL). If NUL -> done;
         * else step past the '\n' and draw the next line. */
        if (*s == '\0') {
            break;
        }
        s++;                     /* 0x3001935b INC ESI (past the '\n') */
    }

    /* 0x30019334: reset the 2D draw color (NULL). */
    cgame_syscall(CG_R_SETCOLOR, 0);
}
