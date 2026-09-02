// Source: uo_cgame_mp_x86.dll 0x30029870..0x30029900
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30029870_30029900.mcode
//
// CG_HudElemTenthsTimerString — format a HUD element's timer value (milliseconds)
// as a clock string with one-tenth-of-a-second precision. Sibling of
// CG_HudElemTimerString (0x300297f0), which produces the same string WITHOUT the
// tenths digit; the two share the CG_GetHudElemTime getter (0x30029780).
//
// Machine-code facts (all magic-number divisions verified against the opcodes):
//   - t = CG_GetHudElemTime(elem)             ; ms, called __thiscall (ECX)
//   - centis = t / 100                               ; IMUL 0x51eb851f, SAR EDX,5
//   - hours  = centis / 36000                        ; IMUL 0x7482296b, SAR EDX,0xe
//   - rem    = centis - hours*36000                  ; IMUL EAX,ESI,-36000 (0xffff7360)
//   - mins   = rem / 600                              ; IMUL 0x1b4e81b5, SAR EDX,6
//   - rem    = rem  - mins*600                        ; IMUL EDX,EDI,-600 (0xfffffda8)
//   - secs   = rem / 10                               ; IMUL 0x66666667, SAR EDX,2
//   - tenths = rem - secs*10                          ; LEA*5, SHL 1 (=*10), SUB
//   All four divisions are signed (SAR + SHR31 + ADD sign-correction idiom), so a
//   negative t would round toward zero; in practice CG_GetHudElemTime clamps
//   its result to >= 0.
//   - TEST ESI,ESI (hours) selects the format: nonzero -> "%i:%02i:%02i.%i"
//     (h:mm:ss.t), zero -> "%i:%02i.%i" (m:ss.t). The push order in the .mcode
//     (ECX=tenths, EAX=secs, EDI=mins, [ESI=hours], fmt) is the reverse of the
//     cdecl varargs order, giving va(fmt, hours, mins, secs, tenths) and
//     va(fmt, mins, secs, tenths) respectively.
//   - va (0x3004e8a0) returns a pointer into its rolling temp buffer; the caller
//     at 0x30029c00 stores that pointer straight into the HUD draw item, so
//     the return type is const char *.
//
// The immediates are recovered in natural form: 100, 36000, 600, 10 (Ghidra
// printed the negated multipliers as -0x8ca0 / -0x258, i.e. -36000 / -600). The
// x86 SAR-based signed division is expressed here as plain C integer division on
// int32_t, which matches for the non-negative values this function receives.

#include "client/cgame/client_recovered.h"

const char *CG_HudElemTenthsTimerString(const struct hudElem_s *elem)
{
    int32_t ms = CG_GetHudElemTime(elem);

    int32_t centis = ms / 100;      /* hundredths of a second */
    int32_t hours = centis / 36000;
    centis -= hours * 36000;
    int32_t minutes = centis / 600;
    centis -= minutes * 600;
    int32_t seconds = centis / 10;
    int32_t tenths = centis - seconds * 10;

    if (hours != 0)
        return va("%i:%02i:%02i.%i", hours, minutes, seconds, tenths);

    return va("%i:%02i.%i", minutes, seconds, tenths);
}
