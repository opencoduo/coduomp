// Source: uo_cgame_mp_x86.dll 0x3001afd0..0x3001b067
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001afd0_3001b067.mcode

#include <stddef.h>  /* NULL */

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_DrawScoreboardFadeElement (0x3001afd0)
 *
 * A member of the developer HUD / fade-draw cluster (0x3001af10..0x3001d264),
 * called once by the CG_Draw2D tail dispatcher (0x3001c0d5) alongside its
 * siblings CG_DrawFixedFadeElement (0x3001b0f0) and CG_DrawDebugFadeElement
 * (0x3001b070). It emits a single fixed-position 2D draw at screen coordinates
 * (150, 320) through cgame trap 0x1b, with a trailing mode/count constant 3.
 *
 * Unlike its siblings (which always pass the raw cg_hudAlpha_vmCvar.value word), this
 * emitter fades: when the scoreboard is showing it multiplies cg_hudAlpha_vmCvar.value
 * by the alpha component of CG_FadeColor(cg_scoreboardShowTime, 100), so the
 * element ramps out over the scoreboard's 100 ms fade window; when the scoreboard
 * is not showing it passes cg_hudAlpha_vmCvar.value unscaled; and when CG_FadeColor
 * reports the fade has fully expired (NULL) it emits nothing.
 *
 * Name: the .mcode size-guess "Com_Parse3DMatrix" is REJECTED. Com_Parse3DMatrix
 * is a text-parsing routine that tokenizes a matrix of numbers; this function
 * parses nothing — it reads cgame draw-state globals, does fixed-point
 * screen-coordinate math via the x87 float->int idiom, calls CG_FadeColor, and
 * issues a draw trap. Named here by proven role; the exact original symbol is
 * unresolved (a single call site, and no cgame syscall-id table was recovered to
 * bind trap 0x1b to its engine name).
 *
 * Behaviour, proven instruction-by-instruction:
 *   - [ESP+8] = 320.0f, [ESP+4] = 150.0f (the two coordinate constants)
 *   - if cg_scoreboardShowing != 0:                          (MOV/TEST/JZ)
 *         color = CG_FadeColor(cg_scoreboardShowTime, 100)   (EDX/ECX regparm)
 *         if color == NULL: return                           (TEST EAX / JZ)
 *         alpha = cg_hudAlpha_vmCvar.value * color[3]                (FLD/FMUL/FSTP)
 *     else:
 *         alpha = cg_hudAlpha_vmCvar.value (raw word, no promotion) (MOV EAX / MOV)
 *   - x = round_to_nearest(150.0f + 2^-30)  -> 150           (FLD/FADD/FISTP)
 *   - y = round_to_nearest(320.0f + 2^-30)  -> 320           (FLD/FADD/FISTP)
 *   - trap(0x1b, y, x, alpha bits, 3)                        (5-arg cdecl syscall)
 */

/* trap 0x1b's trailing mode/count argument; role only, exact meaning unproven. */
enum {
    CG_SCOREBOARD_FADE_DRAW_MODE = 3
};

void CG_DrawScoreboardFadeElement(void)
{
    float alpha;
    int32_t x;
    int32_t y;
    qboolean scoreboardShowing = cg_scoreboardShowing != 0;
    float y_coord = 320.0f;
    float x_coord = 150.0f;

    /*
     * 0x3001afd3 MOV EAX,[cg_scoreboardShowing] / 0x3001afd8 TEST / 0x3001afea JZ:
     * when the scoreboard is up, fade the element's alpha; otherwise draw it at the
     * fixed cg_hudAlpha_vmCvar.value level.
     */
    if (scoreboardShowing) {
        /*
         * 0x3001afec/aff2 CG_FadeColor(cg_scoreboardShowTime, 100): startMsec in
         * EDX, totalMsec in ECX (regparm). NULL means the scoreboard fade has
         * fully expired, in which case nothing is drawn.
         */
        int32_t scoreboardShowTime = cg_scoreboardShowTime;
        const vec_t *color = CG_FadeColor(scoreboardShowTime, CG_FADE_TIME);
        if (color == NULL) {
            /* 0x3001affe JZ 0x3001b063: skip the draw entirely. */
            return;
        }
        /*
         * 0x3001b000 FLD cg_hudAlpha_vmCvar.value / 0x3001b006 FMUL color[3] /
         * 0x3001b009 FSTP: alpha = cg_hudAlpha_vmCvar.value * fade alpha (color[3]).
         */
        alpha = (float)((long double)cg_hudAlpha_vmCvar.value * (long double)color[3]);
    } else {
        /*
         * 0x3001b00e MOV EAX,[cg_hudAlpha_vmCvar.value] / 0x3001b013 MOV [ESP],EAX: the
         * raw 32-bit float word is used directly, without an x87 promotion.
         */
        alpha = CG_FloatFromBits((uint32_t)CG_FloatBits(cg_hudAlpha_vmCvar.value));
    }

    /*
     * 0x3001b016..0x3001b03e: the two coordinate constants are converted to int
     * via the shared x87 float->int idiom (float + 2^-30, then FISTP round), X
     * first and then Y. Both land exactly on their integer values.
     */
    x = CG_RoundToNearest(x_coord);
    y = CG_RoundToNearest(y_coord);

    /*
     * 0x3001b049..0x3001b05a: trap(0x1b, y, x, alpha bits, 3). The computed alpha
     * float is forwarded to the variadic trap by its raw 4-byte word (the machine
     * code stores it via FSTP/MOV and PUSHes the dword, never promoting to double);
     * CG_FloatBits reproduces that bit-exact forwarding. Push order gives the y
     * coordinate as the first coordinate argument and x as the second.
     */
    int32_t alphaBits = CG_FloatBits(alpha);
    int32_t xArg = x;
    int32_t deadAlphaCopy = alphaBits;
    int32_t yArg = y;
    (void)deadAlphaCopy;
    cgame_syscall(CG_DRAW_SCOREBOARD_FADE_ELEMENT, yArg, xArg, alphaBits, CG_SCOREBOARD_FADE_DRAW_MODE);
}
