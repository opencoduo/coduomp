// Source: uo_cgame_mp_x86.dll 0x3001b0f0..0x3001b15f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001b0f0_3001b15f.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_DrawFixedFadeElement (0x3001b0f0)
 *
 * A member of the developer HUD / debug-draw cluster (0x3001af10..0x3001d264).
 * When its enable gate is set it emits a single fixed-position 2D draw at screen
 * coordinates (135, 425), passing the global fade-alpha scale and a trailing 2
 * through cgame trap 0x1d.
 *
 * Name: the assigned-by-size guess "SP_script_origin" is REJECTED. That is a
 * server entity-spawn routine that parses spawn key/values; this function reads
 * two cgame globals, does fixed-point screen-coordinate math, and issues a draw
 * trap. It shares the exact (gate -> coord math -> trap(x,y,scale,2)) shape with
 * its siblings 0x3001af10 (trap 0x1a) and 0x3001b070 (trap 0x1c). Named here by
 * proven role; the exact original symbol is unresolved (only one call site, no
 * cgame syscall-id table recovered).
 *
 * Behaviour, proven instruction-by-instruction:
 *   - if cg_subtitles_vmCvar.integer == 0, return immediately          (TEST EAX,EAX / JZ)
 *   - y = round_to_nearest(425.0f + 2^-30)               (FLD/FADD/FISTP -> 425)
 *   - x = round_to_nearest(135.0f + 2^-30)               (FLD/FADD/FISTP -> 135)
 *   - trap(0x1d, x, y, cg_fadeAlphaScale_bits, 2)        (5-arg cdecl syscall)
 */

/*
 * The two coordinates are produced by the compiler's float->int idiom: a tiny
 * epsilon (the double 2^-30 = 9.313225746154785e-10, at .rdata 0x3007be50) is
 * added to the float coordinate constant, then FISTP rounds the sum to the
 * nearest integer under the default x87 rounding mode. The epsilon is far below
 * one ULP of these values, so the results are exactly 135 and 425; the runtime
 * computation is reproduced faithfully rather than folded to constants. Shared
 * as the static inline CG_RoundToNearest in client_recovered.h (the sibling
 * emitters reuse one definition instead of each carrying a divergent copy).
 */

void CG_DrawFixedFadeElement(void)
{
    int32_t alphaBits;
    int32_t x;
    int32_t y;

    /* Enable gate: skip the whole draw when the flag is clear. */
    if (cg_subtitles_vmCvar.integer == 0) {
        return;
    }

    /* Screen coordinates via the x87 float->int idiom (both land on integers). */
    y = CG_RoundToNearest(425.0f);
    x = CG_RoundToNearest(135.0f);
    /* 0x3001b138: the alpha word is not read until both FISTP stores finish. */
    alphaBits = CG_FloatBits(cg_hudAlpha_vmCvar.value);

    /*
     * trap(0x1d, x, y, cg_hudAlpha_vmCvar.value bits, 2). The fade-alpha scale (a float
     * global) is copied into the variadic trap argument as its raw 4-byte word
     * (MOV EAX,[0x304583c8]; PUSH EAX), i.e. by bit pattern, not promoted to
     * double; CG_FloatBits reproduces that exactly.
     */
    cgame_syscall(CG_DRAW_FIXED_FADE_ELEMENT, x, y, alphaBits, 2);
}
