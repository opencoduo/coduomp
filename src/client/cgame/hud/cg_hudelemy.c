#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>

// Source: uo_cgame_mp_x86.dll 0x30029b00..0x30029b6d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30029b00_30029b6d.mcode
//
// Assigned mechanical name `Q_SwayRand` is REJECTED. It was matched to this
// function only by byte size (win 0x6d == matched 0x6d), which the naming rules
// forbid; there is no RNG here (no seed state, no LCG/multiply-add, no masking).
// This function reads the client game clock cg_time, time-interpolates an integer
// endpoint pair, and returns a float in ST0 with a type-selected owner-relative
// post-adjustment. It is a UI/HUD coordinate value-node evaluator.
//
// This is the byte-for-byte parallel sibling of CG_HudElemX
// (0x30029a90). The two are identical except this one reads the per-component
// fields one slot higher (node +0x50/+0x8/+0x18 and owner +0xc, versus the
// sibling's +0x4c/+0x4/+0x14 and +0x8), which proves those node/owner fields are
// 2-element arrays indexed by component. The shared caller FUN_30029c00 invokes
// the run with EAX = the node (EBP) and ESI = the owning output object:
//   call 0x30029a90 -> FSTP [ESI]      => item->x   (sibling, component 0)
//   call 0x30029b00 -> FSTP [ESI+0x4]  => item->y  (THIS, component 1)
// So this evaluates component 1 into item->y.
//
// The canonical hudElem_t fields and HUDELEM_ALIGN_* enum live in the shared
// player-state declarations. The exact name CG_HudElemY is anchored by the same-module
// Mac traceback symbol order.
//
// ABI: compiler-internal register helper (no stack args, plain `RET`). EAX = node
// pointer, ESI = cgAlignedDrawItem pointer (leaf read of item->height only). Both are
// expressed as explicit pointer parameters; no calling-convention attribute is
// added because the syntax-only build target does not require one.

_Static_assert(offsetof(cgAlignedDrawItem, height) == 0x0c, "cgAlignedDrawItem.height at +0x0c");

long double CG_HudElemY(const hudElem_t *node, const cgAlignedDrawItem *item)
{
    /*
     * 0x30029b03..0x30029b0d: duration = node->moveTime (signed, [EAX+0x58]),
     * stashed at [ESP+8] for later FILD. If duration <= 0 there is no active
     * interpolation window (signed JLE at 0x30029b0d): use the settled value.
     */
    int32_t duration = node->moveTime;

    /* Float faithfulness: this function performs NO float store anywhere
     * (0x30029b00..0x30029b6c contains not one FST/FSTP DWORD) -- the value is
     * built in st0 and returned raw in st0. A `float value` local or return type
     * would round before the caller. Held and returned with the established
     * long-double raw-ST0 carrier; CG_GetHudElemInfo performs the target's sole
     * FSTP to item->y at 0x30029dd8. */
    long double value;
    int32_t elapsed = 0;
    qboolean interpolating = qfalse;

    if (duration > 0) {
        /*
         * 0x30029b0f..0x30029b1a: elapsed = cg_time - node->moveStartTime (signed
         * SUB, [0x304831b0] - [EAX+0x54]). If elapsed >= duration the window has
         * fully elapsed (signed JGE at 0x30029b1a): use the settled value.
         */
        elapsed = coduo_int32_from_bits((uint32_t)cg_time - (uint32_t)node->moveStartTime);
        if (elapsed < duration) {
            interpolating = qtrue;
        }
    }

    if (interpolating) {
        /*
         * 0x30029b1c..0x30029b43: linear interpolation between the two integer
         * endpoints, using the reciprocal-then-multiply order the compiler
         * emitted (it rounds differently from a direct divide, so it is
         * preserved):
         *   prev  = node->moveFromY        ([EAX+0x50])
         *   goal  = node->y        ([EAX+0x8])
         *   numer = (goal - prev) * elapsed   (signed IMUL, stashed at [ESP+4])
         *   value = numer * (1.0f / duration) + prev
         * where 1.0f is the shared .rdata constant at 0x3007bce0 (0x3f800000)
         * reached via FDIVR, and prev/duration/numer are FILD'd as signed ints.
         * prev is re-added with FIADD [ESP+0xc] (integer add of prevValue[1]).
         */
        int32_t prev = node->moveFromY;
        int32_t delta = coduo_int32_from_bits((uint32_t)node->y - (uint32_t)prev);
        int32_t numer = coduo_int32_from_bits((uint32_t)delta * (uint32_t)elapsed);
        /* 0x30029b3f FIADD [ESP+0xc]: prev is added as an INTEGER straight into
         * the 80-bit chain -- it is never converted to float, so no (float)prev
         * cast here (that would round). numer/duration reach the chain via bare
         * FILD (0x30029b2f/0x30029b33) with no intermediate float store either. */
        value = (long double)numer * (1.0f / (long double)duration) + prev;
    } else {
        /*
         * 0x30029b45: settled -> value = node->y via a bare FILD [EAX+0x8] with
         * no float store, so the int reaches st0 exactly (no (float) rounding).
         */
        value = (long double)node->y;
    }

    /*
     * 0x30029b48..0x30029b6c: type-selected owner-relative adjustment keyed on
     * node->alignY ([EAX+0x18]). The machine code is a SUB 0 (test == 0),
     * DEC/JZ, DEC/JNZ ladder, so:
     *   0 -> no adjustment
     *   1 -> value - item->height * 0.5f  (0.5f is .rdata 0x3007bce8, 0x3f000000)
     *   2 -> value - item->height
     *   otherwise -> no adjustment
     */
    switch (node->alignY) {
    case HUDELEM_ALIGN_CENTER:
        /* 0x30029b5e: FLD item->height; FMUL 0.5f; FSUBP -> value - height*0.5 */
        value = value - (long double)item->height * (long double)0.5f;
        break;
    case HUDELEM_ALIGN_END:
        /* 0x30029b57: FSUB item->height -> value - height */
        value = value - (long double)item->height;
        break;
    case HUDELEM_ALIGN_START:
    default:
        break;
    }

    return value;
}
