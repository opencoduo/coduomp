// Source: uo_cgame_mp_x86.dll 0x300297f0..0x30029865
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300297f0_30029865.mcode
//
// CG_HudElemTimerString — format a HUD element's countdown/count-up timer value as
// a clock string. It asks the shared HUD-elem timer-value getter (FUN_30029780,
// caller-observed as CG_GetHudElemTime) for the element's timer value in
// milliseconds, converts it to whole seconds, splits that into hours / minutes /
// seconds, and returns "h:mm:ss" when there is a nonzero hour component and "m:ss"
// otherwise, via the id-Tech `va` ring-buffer formatter.
//
// Name evidence (behavior + call graph, not size):
//   * The two .rdata format strings pushed are "%i:%02i:%02i" (0x30077800) and
//     "%i:%02i" (0x300777f8) — the classic h:mm:ss / m:ss clock forms.
//   * The formatter callee 0x3004e8a0 is `va` (proven q_shared sprintf-into-
//     ring-buffer; see client_recovered.h), so this returns a formatted string.
//   * The value callee 0x30029780 is shared by exactly three call sites: this
//     function (0x300297f1), the adjacent 0x30029870 which divides the same value
//     by 100 (tenths of a second) and formats it, and 0x3002a092. The 0x30029870
//     sibling is CG_HudElemTenthsTimerString (PPC cgame_mp.dll); this function
//     divides by 1000 (whole seconds, no tenths) and is therefore the plain
//     CG_HudElemTimerString listed immediately after it in the PPC name bank.
//
// The mcode header's assigned name `BG_CopyStringIntoBuffer` is a size-matched
// broad-corpus label and is REJECTED: this function performs timer arithmetic and
// clock formatting, not a string copy.
//
// ABI: the element pointer arrives in ECX (thiscall-style register argument) and is
// forwarded unchanged to CG_GetHudElemTime (which consumes [ECX] and
// [ECX+0x5c]); this function never dereferences it, so it is treated as opaque.
//
// Machine-code proof of the arithmetic (all signed 32-bit; the getter clamps its
// result to >= 0, so every quotient below is a non-negative truncating divide):
//   ms   = CG_GetHudElemTime(elem)            // EAX from call
//   tsec = ms / 1000                                // magic 0x10624dd3, SAR 6 (ECX)
//   hrs  = tsec / 3600                              // magic 0x91a2b3c5 (+ECX), SAR 11 (ESI)
//   rem  = tsec - 3600*hrs   (== tsec % 3600)       // IMUL EAX,ESI,-3600; ADD (ECX)
//   min  = rem / 60                                 // magic 0x88888889 (+ECX), SAR 5 (EAX)
//   sec  = rem - 60*min      (== rem % 60)          // IMUL EDX,EAX,60; SUB (ECX)
//   TEST ESI,ESI (hrs) selects the format; PUSH order gives va's argument order.

#include "client/cgame/client_recovered.h"

const char *CG_HudElemTimerString(const struct hudElem_s *elem)
{
    int32_t ms = CG_GetHudElemTime(elem);

    int32_t totalSeconds = ms / 1000;
    int32_t hours   = totalSeconds / 3600;
    int32_t rem     = totalSeconds - hours * 3600;   /* totalSeconds % 3600 */
    int32_t minutes = rem / 60;
    int32_t seconds = rem - minutes * 60;            /* rem % 60 */

    if (hours != 0) {
        /* fall-through path: PUSH sec, PUSH min, PUSH hours, PUSH "%i:%02i:%02i" */
        return va("%i:%02i:%02i", hours, minutes, seconds);
    }

    /* JZ path: PUSH sec, PUSH min, PUSH "%i:%02i" (hours omitted) */
    return va("%i:%02i", minutes, seconds);
}
