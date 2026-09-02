#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

// Source: uo_cgame_mp_x86.dll 0x300299e0..0x30029a13
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300299e0_30029a13.mcode
//
// Assigned mechanical name G_GetVehicleInfoName is REJECTED: it was attached
// only by a size match (win 0x33 == some server symbol 0x33) with zero
// behavioral basis, which the naming rules forbid. There are no vehicles, no
// string handling and no server-info reads here; the body is a jump-table
// dispatch that returns a scalar in ST0.
//
// This is the HUD-element width dispatcher. It takes a hudElem_t in EAX and
// cgAlignedDrawItem in ESI, switches on elem->type, and returns either
// textWidth + labelWidth or animated shader width + labelWidth.
//
// Register-argument ABI (compiler-internal helper, `RET` with no immediate, no
// stack args): the value-node pointer arrives in EAX; the owner pointer arrives
// in ECX and is moved into ESI. Expressed here as two explicit pointer params;
// no calling-convention attribute is added because the syntax-only build does
// not require one.
//
// The exact name CG_HudElemWidth is anchored by the same-module Mac traceback
// symbol order.

/*
 * At 0x30029a01 the call passes EAX (node) and ESI (owner) unchanged, matching
     * the two-register ABI declared centrally for CG_HudElemShaderWidth.
     * Returns the animated width in ST0.
 */

long double CG_HudElemWidth(const hudElem_t *elem, const cgAlignedDrawItem *item)
{
    /*
     * 0x300299e3..0x300299e9: index = node->type - 1. If (unsigned)index > 8 the
     * type is out of the 1..9 dispatch range: return the shared .rdata 0.0f at
     * 0x3007bcec (bit pattern 0x00000000).
     *
     * 0x300299eb..0x300299f2: two-level dispatch. A 9-byte selector table at
     * 0x30029a1c (00 00 01 00 00 00 00 01 01) maps index -> jump-table slot,
     * then a 2-entry jump table at 0x30029a14 selects the case. Types 1,2,4,5,6,7
     * hit the static case (slot 0); types 3,8,9 hit the animated case (slot 1).
     */
    switch (elem->type) {
    case HE_TYPE_TEXT:             /* -> slot 0 */
    case HE_TYPE_VALUE:            /* -> slot 0 */
    case HE_TYPE_TIMER:            /* -> slot 0 */
    case HE_TYPE_TIMER_UP:         /* -> slot 0 */
    case HE_TYPE_TENTHS_TIMER:     /* -> slot 0 */
    case HE_TYPE_TENTHS_TIMER_UP:  /* -> slot 0 */
        /*
         * 0x300299f9..0x30029a00: static width = textWidth + labelWidth.
         * (FLD [ESI+0x1c]; FADD [ESI+0x14])
         */
        return (long double)item->textWidth + (long double)item->labelWidth;

    case HE_TYPE_SHADER:   /* -> slot 1 */
    case HE_TYPE_CLOCK:    /* -> slot 1 */
    case HE_TYPE_CLOCK_UP: /* -> slot 1 */
        /*
         * 0x30029a01..0x30029a0a: animated value = CG_HudElemShaderWidth(node,owner)
         * + labelWidth. (CALL 0x30029920; FADD [ESI+0x14])
         */
        return CG_HudElemShaderWidth(elem, item) + item->labelWidth;

    default:
        /*
         * 0x30029a0b..0x30029a12: node->type == 0 or > 9 -> return 0.0f.
         */
        return (long double)floatZero; /* FLD m32 [0x3007bcec] */
    }
}
