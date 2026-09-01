// Source: uo_cgame_mp_x86.dll 0x3000a8e0..0x3000a96d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3000a8e0_3000a96d.mcode
//
// PM_ViewHeightTableLerp - linear interpolation over a pmLerpEntry_t table keyed
// by an integer. The Windows game body at 0x2000a6a0 is instruction-identical
// to this cgame body apart from relocations. Both Mac modules and the Linux ELF
// retain PM_ViewHeightTableLerp, resolving the earlier PM_TableLerp recovery
// spelling.
//
// Custom register ABI proven at every call site in FUN_3000aa70:
//   EAX = key (search key), EDI = table, EBX = out (float *).
// The interpolated float range value is returned in ST0 (FLD/FADD chain); the
// second interpolated value is stored through EBX. The function issues a bare
// RET (no callee stack cleanup of arguments; the two SUB ESP,8 / ADD ESP,8
// dwords are its own scratch locals), so the register inputs are modeled here as
// ordinary C parameters.
//
// The .mcode header name "entry" is a size guess and is REJECTED: the body is a
// pure table lerp with no relation to any "entry" routine.

#include "bg_pmove.h"

#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select exactly one PM_ViewHeightTableLerp behavior mode"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select exactly one PM_ViewHeightTableLerp behavior mode"
#endif

#if defined(WINDOWS_BEHAVIOR)
long double PM_ViewHeightTableLerp(int32_t percent,
                                   const pmLerpEntry_t *table, float *out)
{
    // TEST ESI,ESI / JZ 0x3000a907: percent == 0 returns table[0] verbatim.
    // The scan starts at table[1] and stops at the first entry whose percent is
    //   - equal to the lookup percentage (JZ, exact match),
    //   - greater than the lookup percentage (JG, bracketing -> interpolate), or
    //   - the -1 terminator (CMP ECX,-1 / JNZ loop) -> falls through to the
    //     same table[0]-verbatim path as percent == 0.
    if (percent != 0) {
        int32_t i = 1;                    // EDX, index into table
        int32_t curPercent = table[i].percent; // ECX = [EDI + 0xc]

        for (;;) {
            // CMP ECX,ESI ; JZ 0x3000a914
            if (curPercent == percent) {
                // Exact match: return table[i] verbatim.
                *out = (float)table[i].originAdjust; // FILD [EAX+8] ; FSTP [EBX]
                return (long double)table[i].height; // FLD  [EAX+4]
            }
            // JG 0x3000a921: table[i].percent > percent -> interpolate.
            if (curPercent > percent) {
                const pmLerpEntry_t *prev = &table[i - 1]; // EDI + i*12 - 12
                const pmLerpEntry_t *cur  = &table[i];     // EAX

                // t = (percent - prev.percent) / (cur.percent - prev.percent)
                // Both subtractions are 32-bit wrapping SUBs whose result is then
                // read back by FILD as a signed 32-bit integer.
                int32_t num = coduo_int32_from_bits((uint32_t)percent -
                                               (uint32_t)prev->percent);
                int32_t den = coduo_int32_from_bits((uint32_t)cur->percent -
                                               (uint32_t)prev->percent);
                /* FILD num; FIDIV den leaves t live in x87 for both interpolations. */
                long double t = (long double)num / den;

                // originAdjust (int) diff, interpolated in float, stored through *out.
                int32_t db = coduo_int32_from_bits((uint32_t)cur->originAdjust -
                                              (uint32_t)prev->originAdjust);
                // FILD db (bare) ; FMUL t ; FIADD prev.originAdjust (integer add) ; FSTP [EBX]
                long double interpolatedOrigin = (long double)db * t;
                interpolatedOrigin += prev->originAdjust;
                *out = (float)interpolatedOrigin;

                // height (float) diff, interpolated, returned in ST0.
                // FLD cur.height ; FSUB prev.height ; FMULP by t ; FADD prev.height
                long double interpolatedHeight =
                    (long double)cur->height - prev->height;
                interpolatedHeight *= t;
                interpolatedHeight += prev->height;
                return interpolatedHeight;
            }
            // MOV ECX,[EAX+0xc] ; ADD EAX,0xc ; INC EDX : advance to next entry.
            // CMP ECX,-1 ; JNZ: keep scanning until the terminator percentage.
            ++i;
            curPercent = table[i].percent;
            if (curPercent == PM_LERP_TABLE_END) {
                break;
            }
        }
    }

    // 0x3000a907: percent == 0, or the scan reached the -1 terminator without
    // finding percent <= entry -> return table[0] verbatim.
    *out = (float)table[0].originAdjust; // FILD [EDI+8] ; FSTP [EBX]
    return (long double)table[0].height; // FLD  [EDI+4]
}
#else
/* Linux game.mp.uo.i386.so RVA 0x00061a85 stores the interpolation fraction
 * and the returned height through binary32 locals before reloading ST0. */
long double PM_ViewHeightTableLerp(int32_t percent,
                                   const pmLerpEntry_t *table,
                                   float *outOriginAdjust)
{
    float viewHeight;
    const pmLerpEntry_t *entry;
    int32_t index;

    if (percent == 0) {
        *outOriginAdjust = (float)table[0].originAdjust;
        viewHeight = table[0].height;
    } else {
        entry = &table[1];
        index = 1;
        for (;;) {
            if (entry->percent == PM_LERP_TABLE_END) {
                *outOriginAdjust = (float)table[0].originAdjust;
                viewHeight = table[0].height;
                break;
            }
            if (percent < entry->percent) {
                const pmLerpEntry_t *previous = &entry[-1];
                int32_t percentDelta = coduo_int32_from_bits(
                    (uint32_t)percent - (uint32_t)previous->percent);
                int32_t percentSpan = coduo_int32_from_bits(
                    (uint32_t)entry->percent -
                    (uint32_t)previous->percent);
                int32_t originAdjustSpan = coduo_int32_from_bits(
                    (uint32_t)entry->originAdjust -
                    (uint32_t)previous->originAdjust);
#if EMULATE_X87
                float fraction = x87f_store_f32(x87f_div(
                    x87f_load_i32(percentDelta),
                    x87f_load_i32(percentSpan)));
                *outOriginAdjust = x87f_store_f32(x87f_add(
                    x87f_mul(x87f_load_i32(originAdjustSpan),
                             x87f_load_f32(fraction)),
                    x87f_load_i32(previous->originAdjust)));
                viewHeight = x87f_store_f32(x87f_add(
                    x87f_mul(x87f_sub(x87f_load_f32(entry->height),
                                      x87f_load_f32(previous->height)),
                             x87f_load_f32(fraction)),
                    x87f_load_f32(previous->height)));
#else
                float fraction =
                    (float)((long double)percentDelta /
                            (long double)percentSpan);

                *outOriginAdjust =
                    (float)((long double)originAdjustSpan *
                                (long double)fraction +
                            (long double)previous->originAdjust);
                viewHeight = (entry->height - previous->height) * fraction +
                             previous->height;
#endif
                break;
            }
            if (percent == entry->percent) {
                *outOriginAdjust = (float)entry->originAdjust;
                viewHeight = entry->height;
                break;
            }
            ++index;
            entry = &table[index];
        }
    }
    return viewHeight;
}
#endif
