#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

// Source: uo_cgame_mp_x86.dll 0x30029780..0x300297c9
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30029780_300297c9.mcode
//
// CG_GetHudElemTime — return a HUD element's timer/clock value in
// milliseconds, clamped to >= 0. This is the shared value getter called by the
// three timer-string formatters CG_HudElemTimerString (0x300297f0),
// CG_HudElemTenthsTimerString (0x30029870) and 0x3002a1d0; those call sites and
// this body agree on the register ABI and field reads, so the caller-observed
// decl is now confirmed against the getter's own bytes.
//
// The mcode header's assigned name `ClearBounds` is a size-matched broad-corpus
// label (win 0x49 == some PPC symbol 0x49) and is REJECTED: there is no bounds
// vector, no min/max init here — the body reads a time discriminant and does
// signed timer arithmetic against cg_time.
//
// Dispatch (machine-code proof):
//   MOV EAX,[ECX]; ADD EAX,-4; CMP EAX,5; JA default    // type-4, gate 0..5
//   JMP [EAX*4 + 0x300297cc]                             // 6-entry jump table
// so the discriminant elem->type is switched over hudElemType_t values 4..9 (the
// timer/clock element types). Any other type (< 4 or > 9) takes the JA default
// and returns 0. Jump table @0x300297cc (objdump -s -j .text), index = type - 4:
//   [0] type 4 -> 0x30029791   [1] type 5 -> 0x300297ba
//   [2] type 6 -> 0x300297a1   [3] type 7 -> 0x300297ba
//   [4] type 8 -> 0x300297af   [5] type 9 -> 0x300297ba
//
// Per-case arithmetic (all signed int32; 0x304831b0 == cg_time):
//   type 4  HE_TYPE_TIMER            EAX = elem->timerValue - cg_time + 0x3e7 (999)
//   type 6  HE_TYPE_TENTHS_TIMER     EAX = elem->timerValue - cg_time + 0x63  (99)
//   type 8  HE_TYPE_CLOCK            EAX = elem->timerValue - cg_time
//   type 5  HE_TYPE_TIMER_UP        \
//   type 7  HE_TYPE_TENTHS_TIMER_UP  } EAX = cg_time - elem->timerValue
//   type 9  HE_TYPE_CLOCK_UP        /
// Countdown types (4/6/8) measure time remaining until timerValue; the +999/+99
// biases round the ms up to the next whole second / tenth-second before the
// callers divide by 1000 / 100. Count-up types (5/7/9) measure time elapsed
// since timerValue.
//
// Tail (0x300297c2): TEST EAX,EAX; JGE +2; XOR EAX,EAX — a negative result is
// clamped to 0 (expired countdown / not-yet-started count-up reads as 0). The
// default arm (0x300297c6) shares this XOR EAX,EAX, so unknown types also
// return 0. The Mac CG_GetHudElemTime performs this same timer-type dispatch
// and clamped time calculation, resolving the source name.
//
// ABI: the element pointer arrives in ECX (thiscall-style register argument),
// result in EAX; plain `RET`, no stack args. cg_time is uint32_t but every use
// here is signed subtraction and a signed >= 0 clamp, so the working value is
// int32_t.

int32_t CG_GetHudElemTime(const struct hudElem_s *elem)
{
    uint32_t value;

    switch (elem->type) {
    case HE_TYPE_TIMER:        /* 4: countdown, whole-second rounding */
        value = (uint32_t)elem->timerValue - cg_time + 999u;
        break;
    case HE_TYPE_TENTHS_TIMER: /* 6: countdown, tenth-second rounding */
        value = (uint32_t)elem->timerValue - cg_time + 99u;
        break;
    case HE_TYPE_CLOCK:        /* 8: countdown, no rounding */
        value = (uint32_t)elem->timerValue - cg_time;
        break;
    case HE_TYPE_TIMER_UP:        /* 5 */
    case HE_TYPE_TENTHS_TIMER_UP: /* 7 */
    case HE_TYPE_CLOCK_UP:        /* 9: count-up, elapsed since timerValue */
        value = cg_time - (uint32_t)elem->timerValue;
        break;
    default:                   /* type < 4 or > 9: JA default */
        return 0;
    }

    /* ADD/SUB are defined modulo 2^32 on the retail Windows/i386 target. The
     * following TEST/JGE treats that bit pattern as signed and returns only a
     * nonnegative result, so the final conversion is representable. */
    if ((value & 0x80000000u) != 0)
        return 0;
    return (int32_t)value;
}
