#include "../client_recovered.h"

// Source: uo_cgame_mp_x86.dll 0x30029920..0x3002997d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30029920_3002997d.mcode
//
// Assigned mechanical name PM_Weapon_FinishRechamber is REJECTED: that server
// symbol is `qboolean PM_Weapon_FinishRechamber(void)` (items.c / pmove) and
// was matched only by byte size (win 0x5d == server 0x5d), which the naming
// rules forbid. This function instead returns a scalar in ST0, reads the client
// game clock cg_time, and time-lerps a scalar property from a start value toward
// a target value. Its callers (FUN_300299e0, FUN_3002a1d0, FUN_3002a000) pair it
// with a sibling at 0x30029980 to produce UI item X/Y coordinates and colors, so
// it is a UI menu-item animated-scalar helper, not weapon/pmove code.
//
// Register-argument ABI (compiler-internal helper, no stack args, `RET` with no
// immediate): the descriptor pointer arrives in EAX; the fallback-value struct
// pointer arrives in ECX (moved into ESI by the callers). Expressed here as two
// pointer parameters; no calling-convention attribute is added because the
// current syntax-only build target does not require one. The exact names
// CG_HudElemShaderDimension and CG_HudElemShaderWidth are anchored by the
// same-module Mac traceback symbol order.

/* 0x30029900..0x30029914: shared endpoint conversion used by the animated-item
 * coordinate path.  A nonzero integer is converted with signed FILD; zero means
 * use the descriptor's float default at +0x28. */
// Source RVA: 0x30029900
long double CG_HudElemShaderDimension(int32_t value, const cgAlignedDrawItem *item)
{
    /* Both exits return the live x87 value directly; neither has an m64 store.
     * Use the established raw-ST0 carrier so a native caller does not introduce
     * a double rounding that is absent at 0x3002990c/0x30029914. */
    return value != 0 ? (long double)value : (long double)item->fontHeight;
}

// Source RVA: 0x30029920
long double CG_HudElemShaderWidth(const hudElem_t *elem,
                              const cgAlignedDrawItem *item)
{
    /*
     * 0x30029923..0x30029932: target = anim->targetValue. If nonzero, convert
     * the signed int exactly (FILD); otherwise use base->baseValue.
     * This is the animation's end value.
     */
    long double endValue;
    if (elem->width != 0) {
        endValue = (long double)elem->width;
    } else {
        endValue = item->fontHeight;
    }

    /*
     * 0x30029935..0x3002993e: duration = anim->duration (signed). If it is <= 0
     * there is no active animation window: return the end value as-is.
     */
    int32_t duration = elem->scaleTime;
    if (duration <= 0) {
        return endValue;
    }

    /*
     * 0x30029940..0x3002994f: elapsed = cg_time - anim->startTime (signed).
     * If elapsed >= duration the animation has completed: return the end value.
     */
    int32_t elapsed = coduo_int32_from_bits((uint32_t)cg_time -
                                      (uint32_t)elem->scaleStartTime);
    if (elapsed >= duration) {
        return endValue;
    }

    /*
     * 0x30029951..0x30029960: startValue = anim->startValue. If nonzero, convert
     * the signed int exactly; otherwise use base->baseValue.
     */
    long double startValue;
    if (elem->scaleFromWidth != 0) {
        startValue = (long double)elem->scaleFromWidth;
    } else {
        startValue = item->fontHeight;
    }

    /*
     * 0x30029963..0x3002996d: fraction is built as (1.0 / duration)
     * * elapsed, NOT elapsed / duration. FILD pushes the signed duration,
     * FDIVR against the shared .rdata 1.0f at 0x3007bce0 yields
     * 1.0f/duration, then FIMUL by the signed int elapsed. The reciprocal-then-
     * multiply order is preserved because it rounds differently from a direct
     * divide. The chain is never stored; long double is the raw-register
     * carrier while the native x87 control word supplies arithmetic precision.
     *
     * 0x3002996d..0x30029977: the x87 tail (FXCH ST2 / FSUB ST0,ST1 / FMULP ST2
     * / FADDP) evaluates startValue + (endValue - startValue) * fraction with
     * that subtraction done before the multiply, leaving the result in ST0.
     */
    const long double fraction =
        (1.0f / (long double)duration) * (long double)elapsed;
    return startValue + (endValue - startValue) * fraction;
}
