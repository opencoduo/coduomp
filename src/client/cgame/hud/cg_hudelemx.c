#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>

// Source: uo_cgame_mp_x86.dll 0x30029a90..0x30029afd
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30029a90_30029afd.mcode
//
// Assigned mechanical name `AxisClear` is REJECTED. AxisClear is
// `void AxisClear(vec3_t axis[3])` (a 3x3 matrix zero-then-identity fill, no x87
// arithmetic, no time read) and was matched to this function only by byte size
// (win 0x6d == matched 0x6d), which the naming rules forbid. This function reads
// the client game clock cg_time, time-interpolates an integer endpoint pair, and
// returns a float in ST0 with a type-selected post-adjustment. It is a UI/HUD
// value-node coordinate evaluator, not a matrix helper.
//
// Behavior + call graph. The single caller (FUN_30029c00 at 0x30029dca) invokes
// a run of near-identical sibling evaluators with EAX = the value node (EBP) and
// ESI = the owning output object, storing each float result into a different
// owner slot:
//   call 0x30029a30 -> item->height   (+0x0c)   (the value-node dispatcher)
//   call 0x30029a90 -> item->x    (+0x00)   (THIS function)
//   call 0x30029b00 -> item->y   (+0x04)   (sibling, byte-for-byte parallel)
// So this evaluates one component (stored at item+0x00) of the object the caller
// is building; the sibling FUN_30029b00 is identical except it reads the
// node's +0x50/+0x8/+0x18 track and subtracts item+0xc. Together they form a
// small set of parallel per-component animated-value evaluators.
//
// Relation to the value-node lerp pair at 0x30029920 (CG_HudElemShaderWidth) and
// 0x30029980 (CG_HudElemShaderHeight): same subsystem and same reciprocal-multiply
// lerp idiom, but a DIFFERENT node shape. Those read integer tracks at
// +0x30/+0x34/+0x3c/+0x40 with the window at +0x44/+0x48 and substitute
// owner->defaultValue when a track integer is 0; THIS node reads integer
// endpoints at +0x4c/+0x4 with the window at +0x54/+0x58, does NO zero
// substitution, and then applies a type-selected owner-relative adjustment keyed
// on node->alignX (+0x14). The exact name CG_HudElemX is anchored by the
// same-module Mac traceback symbol order.
//
// ABI: compiler-internal register helper (no stack args, `RET` with no
// immediate). EAX = node pointer, ESI = cgAlignedDrawItem pointer (leaf read of
// item->width only). Both are expressed as explicit pointer parameters; no
// calling-convention attribute is added because the syntax-only build target
// does not require one.
//
// CG_GetHudElemInfo proves the concrete owner identity: ESI is the
// cgAlignedDrawItem whose +0x08 field is width and whose +0x00 field receives
// this result. The HUDELEM_ALIGN_* enum is shared in player_state_types.h.

_Static_assert(offsetof(cgAlignedDrawItem, width) == 0x08,
               "cgAlignedDrawItem.width at +0x08");

long double CG_HudElemX(const hudElem_t *node,
                                  const cgAlignedDrawItem *item)
{
    /*
     * 0x30029a93..0x30029a9d: duration = node->moveTime (signed, [EAX+0x58]),
     * stashed at [ESP+8] for later FILD. If duration <= 0 there is no active
     * interpolation window (signed JLE at 0x30029a9d): use the settled value.
     */
    int32_t duration = node->moveTime;

    /* value is the surviving x87 ST0 across 0x30029abf..0x30029afc: it is never
     * stored to a float slot inside this function (every path RETs raw ST0), and
     * the alignX switch subtracts item->width from the UNROUNDED value, so it is
     * long double. A float `value` would round it before the anchor subtraction
     * (a double-rounding the DLL does not perform). The long-double return is the
     * established raw-ST0 carrier; the caller performs the target's sole float
     * rounding with FSTP [ESI] at 0x30029dcf. */
    long double value;
    int32_t elapsed = 0;
    qboolean interpolating = qfalse;

    if (duration > 0) {
        /*
         * 0x30029a9f..0x30029aaa: elapsed = cg_time - node->moveStartTime (signed
         * SUB, [0x304831b0] - [EAX+0x54]). If elapsed >= duration the window has
         * fully elapsed (signed JGE at 0x30029aaa): use the settled value.
         */
        elapsed = coduo_int32_from_bits((uint32_t)cg_time -
                                   (uint32_t)node->moveStartTime);
        if (elapsed < duration) {
            interpolating = qtrue;
        }
    }

    if (interpolating) {
        /*
         * 0x30029aac..0x30029ad3: linear interpolation between the two integer
         * endpoints, using the reciprocal-then-multiply order the compiler
         * emitted (it rounds differently from a direct divide, so it is
         * preserved):
         *   prev = node->prevValue          ([EAX+0x4c])
         *   goal = node->goalValue          ([EAX+0x4])
         *   numer = (goal - prev) * elapsed  (signed IMUL, at [ESP+4])
         *   value = prev + numer * (1.0f / duration)
         * where 1.0f is the shared .rdata constant at 0x3007bce0 (0x3f800000)
         * reached via FDIVR, and prev/duration/numer are FILD'd as signed ints.
         * No (float) casts: the DLL FILDs each integer straight into the divide/
         * multiply/FIADD with no FSTP DWORD, so the implicit conversions stay exact
         * (an explicit cast would round under -std=c11). */
        int32_t prev = node->moveFromX;
        int32_t delta = coduo_int32_from_bits((uint32_t)node->x -
                                        (uint32_t)prev);
        int32_t numer = coduo_int32_from_bits((uint32_t)delta *
                                        (uint32_t)elapsed);
        value = (long double)prev +
                (long double)numer *
                    ((long double)1.0f / (long double)duration);
    } else {
        /*
         * 0x30029ad5: settled -> value = node->x (FILD [EAX+0x4], no FSTP -> the
         * int stays exact in ST0; no (float) cast).
         */
        value = node->x;
    }

    /*
     * 0x30029ad8..0x30029afc: type-selected owner-relative adjustment keyed on
     * node->alignX ([EAX+0x14]). The machine code is a SUB 0 (test == 0),
     * DEC/JZ, DEC/JNZ ladder, so:
     *   0 -> no adjustment
     *   1 -> value - item->width * 0.5f  (0.5f is .rdata 0x3007bce8, 0x3f000000)
     *   2 -> value - item->width
     *   otherwise -> no adjustment
     */
    switch (node->alignX) {
    case HUDELEM_ALIGN_CENTER:
        /* 0x30029aee: FLD item->width; FMUL 0.5f; FSUBP -> value - width*0.5 */
        value = value -
                (long double)item->width * (long double)0.5f;
        break;
    case HUDELEM_ALIGN_END:
        /* 0x30029ae7: FSUB item->width -> value - width */
        value = value - (long double)item->width;
        break;
    case HUDELEM_ALIGN_START:
    default:
        break;
    }

    return value;
}
