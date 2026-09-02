#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

// Source: uo_cgame_mp_x86.dll 0x30029a30..0x30029a73
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30029a30_30029a73.mcode
//
// The mcode header name _VectorSubtract is REJECTED: it was attached only by a
// size match (win 0x43 == some corpus symbol 0x43) with zero behavioral basis,
// which the naming rules forbid. _VectorSubtract is a 3-float componentwise
// subtract; there is no vector, no subtract and no three-lane store here. The
// body is a jump-table dispatch on a value-node type discriminant that returns a
// single float in ST0.
//
// This is the HUD-element height dispatcher. It returns item->fontHeight for
// text/value/timer types and the animated shader height for shader/clock types,
// then applies the original nonnegative clamp. The exact name CG_HudElemHeight
// is anchored by the same-module Mac traceback symbol order.
//
// Register-argument ABI (compiler-internal helper, `RET` with no immediate, no
// stack args): the value-node pointer arrives in EAX; the owner pointer arrives
// in ECX and is moved into ESI. Expressed here as two explicit pointer params; no
// calling-convention attribute is added because the syntax-only build does not
// require one.

/*
 * At 0x30029a4e the call passes EAX (node) and ESI (owner) unchanged in
 * registers, matching the centrally declared two-register ABI, and returns the
 * animated height in ST0.
 */

long double CG_HudElemHeight(const hudElem_t *elem, const cgAlignedDrawItem *item)
{
    /* long double: in the animated case `value` receives CG_HudElemShaderHeight's
     * raw st(0) return, and the clamp at 0x30029a5a FCOMs it against
     * item->fontHeight WITHOUT storing it to a float slot first -- a float
     * local would round before that compare, which the DLL does not. The static
     * case widens fontHeight exactly. The long-double return carries the raw
     * ST0 value to the sole caller's immediate FSTP (0x30029dc5). */
    long double value;

    /*
     * 0x30029a33..0x30029a39: index = node->type - 1. If (unsigned)index > 8 the
     * type is out of the 1..9 dispatch range: return the shared .rdata 0.0f at
     * 0x3007bcec (bit pattern 0x00000000).
     *
     * 0x30029a3b..0x30029a42: two-level dispatch. A 9-byte selector table at
     * 0x30029a7c (00 00 01 00 00 00 01 01 01) maps index -> jump-table slot, then
     * a 2-entry jump table at 0x30029a74 selects the case. Types 1,2,4,5,6 hit the
     * static case (slot 0 -> 0x30029a49); types 3,7,8,9 hit the animated case
     * (slot 1 -> 0x30029a4e).
     */
    switch (elem->type) {
    case 1: /* -> slot 0 */
    case 2: /* -> slot 0 */
    case 4: /* -> slot 0 */
    case 5: /* -> slot 0 */
    case 6: /* -> slot 0 */
        /*
         * 0x30029a49: static height = item->fontHeight, loaded raw.
         * (FLD [ESI+0x28]).
         */
        value = item->fontHeight;
        break;

    case 3: /* -> slot 1 */
    case 7: /* -> slot 1 */
    case 8: /* -> slot 1 */
    case 9: /* -> slot 1 */
        /*
         * 0x30029a4e: animated value = CG_HudElemShaderHeight(node, owner).
         * (CALL 0x30029980) Result left in ST0.
         */
        value = CG_HudElemShaderHeight(elem, item);
        break;

    default:
        /*
         * 0x30029a6b: node->type == 0 or > 9 -> return 0.0f.
         * (FLD float [0x3007bcec] == 0.0f) No clamp applied.
         */
        return 0.0f;
    }

    /*
     * Common tail, 0x30029a53..0x30029a6a: optional lower-bound clamp.
     *
     * 0x30029a53..0x30029a58: if item->label is null, return `value` as-is.
     *
     * 0x30029a5a..0x30029a62: FCOM value against item->fontHeight, then
     * TEST AH,0x5 / JP. The mask isolates C0 (ST0 < mem, AH bit0) and C2
     * (unordered, AH bit2); JP (even parity) is taken when C0 is clear, i.e. when
     * value >= fontHeight (or unordered) -> return `value` unchanged.
     *
     * 0x30029a64..0x30029a66: otherwise value < fontHeight -> discard value
     * (FSTP ST0) and reload item->fontHeight (FLD [ESI+0x28]) as the result.
     * Net effect: a nonnull label clamps value up to the font-height floor.
     */
    if (item->label != NULL && value < item->fontHeight) {
        value = item->fontHeight;
    }
    return value;
}
