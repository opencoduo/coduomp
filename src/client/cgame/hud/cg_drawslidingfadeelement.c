// Source: uo_cgame_mp_x86.dll 0x3001af10..0x3001afc1
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001af10_3001afc1.mcode

#include <stddef.h>  /* NULL */

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_DrawSlidingFadeElement (0x3001af10)
 *
 * A member of the developer HUD / fade-draw cluster (0x3001af10..0x3001d264),
 * sibling of CG_DrawScoreboardFadeElement (0x3001afd0, trap 0x1b),
 * CG_DrawDebugFadeElement (0x3001b070, trap 0x1c) and CG_DrawFixedFadeElement
 * (0x3001b0f0, trap 0x1d). It emits a single fixed-position 2D draw through cgame
 * trap 0x1a, with a trailing mode/count constant 2.
 *
 * Unlike the constant-coordinate siblings, this emitter's X coordinate SLIDES with
 * the HUD slide fraction: X = 357.0 - (cg_hudCompassSize_vmCvar.value - 1.0) * 160.0, computed
 * on the x87 stack before the scoreboard branch. The Y coordinate is the constant
 * 6.0f. Like CG_DrawScoreboardFadeElement it fades: when the scoreboard is showing
 * it multiplies cg_hudAlpha_vmCvar.value by the alpha component (color[3]) of
 * CG_FadeColor(cg_scoreboardShowTime, 100) and draws nothing when that fade has
 * fully expired (NULL); otherwise it passes cg_hudAlpha_vmCvar.value unscaled.
 *
 * Name: the .mcode size-guess "ByteToDir" is REJECTED. ByteToDir is a Quake3
 * byte->unit-vector table lookup (indexes a bytedirs[] table, returns a vec3),
 * whereas this function reads cgame HUD/scoreboard draw-state globals, computes a
 * sliding screen coordinate via the x87 float math + float->int idiom, calls
 * CG_FadeColor, and issues a draw trap. Named here by proven role; the exact
 * original symbol is unresolved (a single call site, and no cgame syscall-id table
 * was recovered to bind trap 0x1a to its engine name).
 *
 * Behaviour, proven instruction-by-instruction (0x3001af10..0x3001afc0):
 *   - x87: FLD cg_hudCompassSize_vmCvar.value; FSUB 1.0; FMUL 160.0; FSUBR 345.0; FADD 12.0
 *          -> X = 345.0 - (cg_hudCompassSize_vmCvar.value - 1.0)*160.0 + 12.0, spilled to
 *          [ESP+4] as a float (0x3001af13..0x3001af40).
 *   - [ESP+8] = 6.0f (0x40c00000), the Y coordinate constant (0x3001af26).
 *   - EAX = cg_scoreboardShowing; if != 0:                        (MOV/TEST/JZ)
 *         color = CG_FadeColor(cg_scoreboardShowTime, 100)        (EDX/ECX regparm)
 *         if color == NULL: skip the draw                         (TEST EAX / JZ)
 *         alpha = cg_hudAlpha_vmCvar.value * color[3]                    (FLD/FMUL/FSTP)
 *     else:
 *         alpha = cg_hudAlpha_vmCvar.value (raw word, no promotion)      (MOV EAX / MOV)
 *   - xInt = round_to_nearest(X + 2^-30)                          (FLD/FADD/FISTP)
 *   - yInt = round_to_nearest(6.0f + 2^-30)   -> 6               (FLD/FADD/FISTP)
 *   - trap(0x1a, yInt, xInt, alpha bits, 2)                       (5-arg cdecl syscall)
 *     Push order (0x3001af9c..0x3001afb2), verified by tracking ESP through the
 *     interleaved pushes: PUSH 2, PUSH alpha, PUSH xInt, PUSH yInt, PUSH 0x1a, then
 *     ADD ESP,0x14 cleans five dwords -> cgame_syscall(0x1a, yInt, xInt, alpha, 2).
 *     The MOV [ESP+0x10],ECX at 0x3001afa8 spills alpha into the (now dead) Y-coord
 *     float slot and is not read back.
 */

/* trap 0x1a's trailing mode/count argument; role only, exact meaning unproven. */
enum { CG_SLIDING_FADE_DRAW_MODE = 2 };

void CG_DrawSlidingFadeElement(void)
{
    float x_coord;
    float alpha;
    int32_t x;
    int32_t y;

    /*
     * 0x3001af13..0x3001af40: the sliding X screen coordinate, computed on the x87
     * stack. FLD cg_hudCompassSize_vmCvar.value / FSUB 1.0 / FMUL 160.0 / FSUBR 345.0 (reverse
     * subtract: 345.0 - st0) / FADD 12.0, spilled to [ESP+4] as a float.
     */
    long double xRaw = (long double)cg_hudCompassSize_vmCvar.value;
    qboolean scoreboardShowing = cg_scoreboardShowing != 0;
    xRaw -= (long double)1.0f;
    float y_coord = 6.0f;
    xRaw *= (long double)160.0f;
    xRaw = (long double)345.0f - xRaw;
    xRaw += (long double)12.0f;
    x_coord = (float)xRaw;

    /*
     * 0x3001af19 MOV EAX,[cg_scoreboardShowing] / 0x3001af1e TEST / 0x3001af44 JZ:
     * when the scoreboard is up, fade the element's alpha; otherwise draw it at the
     * fixed cg_hudAlpha_vmCvar.value level.
     */
    if (scoreboardShowing) {
        /*
         * 0x3001af46/af4c CG_FadeColor(cg_scoreboardShowTime, 100): startMsec in
         * EDX, totalMsec in ECX (regparm). NULL means the scoreboard fade has fully
         * expired, in which case nothing is drawn.
         */
        int32_t scoreboardShowTime = cg_scoreboardShowTime;
        const vec_t *color = CG_FadeColor(
            scoreboardShowTime, CG_FADE_TIME);
        if (color == NULL) {
            /* 0x3001af58 JZ 0x3001afbd: skip the draw entirely. */
            return;
        }
        /*
         * 0x3001af5a FLD cg_hudAlpha_vmCvar.value / 0x3001af60 FMUL color[3] / 0x3001af63
         * FSTP: alpha = cg_hudAlpha_vmCvar.value * fade alpha (color[3]).
         */
        alpha = (float)(
            (long double)cg_hudAlpha_vmCvar.value
            * (long double)color[3]);
    } else {
        /*
         * 0x3001af68 MOV EAX,[cg_hudAlpha_vmCvar.value] / 0x3001af6d MOV [ESP],EAX: the raw
         * 32-bit float word is used directly, without an x87 promotion.
         */
        alpha = CG_FloatFromBits(
            (uint32_t)CG_FloatBits(cg_hudAlpha_vmCvar.value));
    }

    /*
     * 0x3001af70..0x3001af98: both coordinates are converted to int via the shared
     * x87 float->int idiom (float + 2^-30, then FISTP round), X first and then Y.
     * The Y constant 6.0f lands exactly on 6; the sliding X rounds to nearest.
     */
    x = CG_RoundToNearest(x_coord);
    y = CG_RoundToNearest(y_coord);

    /*
     * 0x3001af9c..0x3001afba: trap(0x1a, yInt, xInt, alpha bits, 2). The alpha float
     * is forwarded to the variadic trap by its raw 4-byte word (the machine code
     * stores it via FSTP/MOV and forwards the dword, never promoting to double);
     * CG_FloatBits reproduces that bit-exact forwarding. Push order gives the y
     * coordinate as the first coordinate argument and x as the second.
     */
    int32_t alphaBits = CG_FloatBits(alpha);
    int32_t xArg = x;
    int32_t deadAlphaCopy = alphaBits;
    int32_t yArg = y;
    (void)deadAlphaCopy;
    cgame_syscall(CG_DRAW_SLIDING_FADE_ELEMENT, yArg, xArg, alphaBits,
                  CG_SLIDING_FADE_DRAW_MODE);
}
