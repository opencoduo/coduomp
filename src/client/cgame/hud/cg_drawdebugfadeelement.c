// Source: uo_cgame_mp_x86.dll 0x3001b070..0x3001b0ee
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001b070_3001b0ee.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_DrawDebugFadeElement (0x3001b070)
 *
 * A member of the developer HUD / debug-draw cluster (0x3001af10..0x3001d264),
 * sibling of CG_DrawFixedFadeElement (0x3001b0f0). When its developer/debug gate
 * permits, it emits a single fixed-position 2D draw at the small integer coords
 * (2, 4), forwarding the global fade-alpha scale through cgame trap 0x1c.
 *
 * Name: the assigned-by-size guess "ColorBytes3" is REJECTED. ColorBytes3 is a
 * server color-packing helper (three float components -> byte color); this
 * function reads a signed debug gate and the developer cvar, does the x87
 * float->int coordinate idiom, and issues a draw trap. It shares the sibling's
 * (gate -> coord math -> trap(coords, fadeBits)) shape with 0x3001b0f0
 * (trap 0x1d). Named here by proven role; the exact original symbol is
 * unresolved (single call site, no cgame syscall-id table recovered).
 *
 * Gate, proven instruction-by-instruction (0x3001b070..0x3001b089):
 *   EAX = con_minicon_vmCvar.integer
 *   if ((int)EAX < 0)            return;   ; JL  -> ret
 *   if (developer_vmCvar.integer == 0)            ; JNZ skips the second test
 *       if (EAX == 0)           return;    ; JZ  -> ret
 * i.e. draw only when the gate is > 0, or when it is exactly 0 and developer
 * mode is on; never when the gate is negative.
 *
 * Coordinates, proven (0x3001b08f..0x3001b0cb): each is round-to-nearest of a
 * float constant plus the tiny double epsilon 2^-30 under the default x87
 * rounding mode (the compiler's float->int idiom). 4.0f -> 4 goes to [esp+0xc],
 * 2.0f -> 2 goes to [esp]. The epsilon is far below one ULP of these values, so
 * the results are exactly 4 and 2; the runtime computation is reproduced
 * faithfully rather than folded to constants.
 *
 * Trap, proven (0x3001b0cc..0x3001b0e7): four dwords are pushed and cleaned by
 * ADD ESP,0x10 -> cgame_syscall(0x1c, a=2, b=4, fadeBits). The fade-alpha scale
 * (a float global at 0x304583c8) is copied into the variadic argument as its raw
 * 4-byte word (MOV EAX,[0x304583c8]; PUSH EAX), i.e. by bit pattern, not promoted
 * to double; CG_FloatBits reproduces that exactly. Unlike the 0x1d sibling there
 * is NO trailing constant argument.
 */

/*
 * The two coordinates are produced by the compiler's float->int idiom: a tiny
 * epsilon (the double 2^-30 = 9.313225746154785e-10, at .rdata 0x3007be50) is
 * added to the float coordinate constant, then FISTP rounds the sum to the
 * nearest integer under the default x87 rounding mode. Shared as the static
 * inline CG_RoundToNearest in client_recovered.h (the sibling emitters reuse
 * one definition instead of each carrying a divergent file-local copy).
 */

void CG_DrawDebugFadeElement(void)
{
    int32_t gate = con_minicon_vmCvar.integer;
    int32_t alphaBits;
    int32_t a;
    int32_t b;

    /*
     * Developer/debug gate: reject the draw when the gate is negative; when the
     * developer cvar is off, also reject it when the gate is exactly zero.
     * Equivalent to: proceed only if gate > 0 || (gate == 0 && developer).
     */
    if (gate < 0) {
        return;
    }
    if (developer_vmCvar.integer == 0 && gate == 0) {
        return;
    }

    /* 0x3001b08a: snapshot the raw alpha word before either x87 conversion. */
    alphaBits = CG_FloatBits(cg_hudAlpha_vmCvar.value);

    /* Screen coordinates via the x87 float->int idiom (both land on integers). */
    b = CG_RoundToNearest(4.0f);
    a = CG_RoundToNearest(2.0f);

    /*
     * trap(0x1c, a, b, cg_hudAlpha_vmCvar.value bits). The fade-alpha scale is forwarded
     * as its raw 4-byte word (MOV EAX,[0x304583c8]; PUSH EAX), by bit pattern, not
     * promoted to double; CG_FloatBits reproduces that exactly. Exactly four
     * dwords are pushed (id + three args) and cleaned by ADD ESP,0x10.
     */
    cgame_syscall(CG_DRAW_DEBUG_FADE_ELEMENT, a, b, alphaBits);
}
