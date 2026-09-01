// Source: uo_cgame_mp_x86.dll 0x30029730..0x30029780
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30029730_30029780.mcode
//
// HUD-element text-width evaluator. Given a string and its draw item, it either
// multiplies the engine's printable-character count by a fixed glyph width or
// asks the renderer for the proportional text width. Proven behavior:
//
//   if (item->fontWidth != 0.0f)
//       return (float)cgame_syscall(CG_SE_PRINT_STRLEN, text) * item->fontWidth;
//   else
//       return (float)cgame_syscall(CG_R_TEXT_WIDTH, text, item->font,
//                                   item->fontScaleBits, 0);
//
// Both syscalls return an int32 that is converted to float via FILD (x87), and the
// float result is returned in st0 (x87 return convention).
//
// ABI / provenance notes (register-argument helper, from its two call sites in
// FUN_30029c00 at 0x30029d66 and 0x30029d7b):
//   * ECX carries `text` — a NUL-terminated string pointer. The caller loads it from
//     item->label (+0x10) / item->text (+0x18) and forwards it unchanged; our
//     function pushes it straight through as the first syscall data argument.
//   * ESI points at the draw item and is used without being saved/restored, so it
//     is a live register argument set up by the caller (not a stack parameter).
//   * The function opens with `PUSH ECX` purely to reserve one 4-byte stack slot,
//     which is later reused as the FILD (int->float) conversion temporary; the
//     matching `POP ECX` in each branch balances the stack before RET.
//
// Field offsets proven against the caller (FUN_30029c00), which writes this same
// cgAlignedDrawItem through ESI:
//   +0x20 font       : renderer font selector -> CG_R_TEXT_WIDTH arg2
//   +0x24 fontScale  : raw float bits          -> CG_R_TEXT_WIDTH arg3
//   +0x2c fontWidth  : fixed glyph width       -> gate + printable length scale
//
// The name guess `PM_Weapon_FinishWeaponDeploy` in the .mcode header is a
// size-matched broad-corpus label and is REJECTED: this is not a weapon
// player-movement routine — it measures a HUD-element string via the cgame
// syscall trap (the caller cluster references the "hudelem string" .rdata literal
// at 0x30077b18). The exact name CG_HudElemStringWidth is anchored by the
// same-module Mac traceback symbol order.

#include <string.h>

#include "client/cgame/client_recovered.h"

/*
 * CG_HudElemStringWidth (0x30029730). `text` arrives in ECX, `item` in ESI
 * (see ABI notes above).
 */
float CG_HudElemStringWidth(const char *text, const cgAlignedDrawItem *item)
{
    if (item->fontWidth != 0.0f) {
        /* FUCOMPP against 0.0f; JNP-taken == equal path is the else below. */
        int32_t length = coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_SE_PRINT_STRLEN, (intptr_t)text));
        /* 0x30029750 FILD feeds 0x30029757 FMUL directly with NO intermediate float
         * store, so length enters the product exact; an explicit (float) cast would
         * round it first under -fexcess-precision=standard (Class 4). The product's
         * raw st(0) return is inert: both callers FSTP it immediately (0x30029d6b,
         * 0x30029d80), so the float return is faithful. */
        return length * item->fontWidth;
    }

    /* MOV EAX,[ESI+0x20] and MOV EDX,[ESI+0x24] push the raw dwords: `mode` is an
     * int32, but `base` is a float forwarded by its raw bit pattern (no numeric
     * float->int conversion happens in the machine code). Reinterpret its bits. */
    int32_t base_bits;
    memcpy(&base_bits, &item->fontScale, sizeof base_bits);
    int32_t width = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_R_TEXT_WIDTH,
        (intptr_t)text,
        item->font,
        base_bits,
        0));
    return (float)width;
}
