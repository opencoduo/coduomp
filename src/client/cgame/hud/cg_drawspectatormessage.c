// Source: uo_cgame_mp_x86.dll 0x3001b720..0x3001b7cf
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001b720_3001b7cf.mcode

#include "../client_recovered.h"

#include <stdint.h>

/*
 * CG_DrawSpectatorMessage (0x3001b720)
 *
 * Draws the horizontally-centered "CGAME_SPECTATOR" HUD message. It first
 * resolves the localized text via CG_SafeTranslateString_Internal("cgame", "CGAME_SPECTATOR"),
 * measures that text's pixel width with cgame trap 52 (CG_R_TEXT_WIDTH), computes a
 * centered x coordinate x = (640.0f - width) * 0.5f, then emits the text through
 * cgame trap 54 (CG_R_TEXT_PAINT) at y = 443.0f in white, using the same 10-argument
 * 2D-draw shape as the sibling emitter CG_EmitTrap54Draw (0x3001cf10).
 *
 * Name adjudication: the .mcode header's size-matched guess
 * "script_method_player_getguid" is REJECTED. This function is not a script
 * method and produces no GUID: it takes no script/console arguments, returns
 * nothing, translates a fixed .rdata string, and dispatches two cgame draw
 * syscalls (trap 52 measure + trap 54 draw) through *0x30085e9c. The match was a
 * size collision (both 0xaf bytes) with no behavioral basis. The engine service
 * behind traps 52/54 is unproven (no cgame syscall-id table recovered), so they
 * keep the honest role names CG_R_TEXT_WIDTH/CG_R_TEXT_PAINT (see client_recovered.h) and
 * this function keeps a behavioral name; the exact original CoD symbol is
 * unresolved.
 *
 * Trap 52 (measure) call, proven from the push trace 0x3001b755..0x3001b781:
 *   cgame_syscall(52, translated, 4, 1/3, 0)   -> int32 pixel width
 * The 0x3eaaaaab scale (1/3) is forwarded as a raw float dword (built in a stack
 * slot then PUSHed by value). The 5 pushed dwords (id + 4 args) match the sibling
 * measure site at 0x30029730 documented on CG_R_TEXT_WIDTH.
 *
 * Center computation, proven from 0x3001b787..0x3001b7ba:
 *   FILD  width                 st0 = (float)width          (0x3001b791)
 *   FSUBR [0x3007bf34]=640.0f   st0 = 640.0f - width        (0x3001b79c)
 *   FMUL  [0x3007bce8]=0.5f     st0 = (640.0f - width)*0.5f (0x3001b7ad)
 *   FSTP  slot                  x = that centered coordinate (0x3001b7b6)
 * (640.0f and 0.5f are the .rdata constants g_const_float_640 and
 * floatOneHalf; used here as literals in natural form.)
 *
 * Trap 54 (draw) call, proven from the push trace 0x3001b78b..0x3001b7c1
 * (10 dwords: id + 9 args, low address / lowest id first):
 *   cgame_syscall(54,
 *                 CG_FloatBits(x),      // centered x            (PUSH EDX 0x3001b7be)
 *                 CG_FloatBits(443.0f), // y = 443.0f            (PUSH ECX 0x3001b7b5)
 *                 4,                    // style                 (PUSH 4   0x3001b7b3)
 *                 CG_FloatBits(1/3),    // scale                 (PUSH EAX 0x3001b7ac)
 *                 &color,               // white rgba vec4       (PUSH EDX 0x3001b7ab)
 *                 translated,           // text handle/pointer   (PUSH ESI 0x3001b7a6)
 *                 0,                    // (trap 54 size slot; 0 here vs 16.0f in
 *                                       //  CG_EmitTrap54Draw)   (PUSH ECX 0x3001b79b)
 *                 0,                    //                       (PUSH 0   0x3001b799)
 *                 3);                   // mode                  (PUSH 3   0x3001b78b)
 * The color argument points at a local vec4 white {1,1,1,1} written at
 * 0x3001b72e..0x3001b746 (four 0x3f800000 dwords) and passed by address
 * (LEA EDX,[ESP+0x38] at 0x3001b7a7). Like the vec4 in CG_EmitTrap54Draw, this
 * site materializes four 1.0f components, so the color is modeled as vec4_t.
 *
 * Float dwords are forwarded to the variadic trap by raw bit pattern (the i386
 * code PUSHes the 4-byte float word, never promoted to double); CG_FloatBits
 * reproduces that exactly, matching the sibling emitters.
 *
 * ABI: no incoming source arguments (the frame is pure scratch). The trailing
 * ADD ESP,0x3c unwinds only this frame's scratch plus the pushed trap args; the
 * final POP ESI / ADD ESP,0x20 restore the SUB ESP,0x20 + PUSH ESI prologue, then
 * a plain RET (no callee cleanup of caller args).
 */

/* Fixed CG_R_TEXT_PAINT draw parameters, proven from the pushed immediates. */
enum {
    CG_SPEC_STYLE = 4, /* PUSH 4 (int) style / font id                        */
    CG_SPEC_MODE = 3, /* trailing PUSH 3 (int) mode                          */
};
#define CG_SPEC_SCREEN_WIDTH 640.0f /* 0x3007bf34; centering reference width   */
#define CG_SPEC_HALF 0.5f   /* 0x3007bce8; center = (w - width) * 0.5f  */
#define CG_SPEC_Y 443.0f /* 0x43dd8000; fixed y coordinate           */
#define CG_SPEC_SCALE (1.0f / 3.0f) /* 0x3eaaaaab; measure + draw scale  */

void CG_DrawSpectatorMessage(void)
{
    /* 0x3001b72e..0x3001b746 initializes the complete local before the
     * translation call; the trap later receives this exact stack object. */
    vec4_t color = {1.0f, 1.0f, 1.0f, 1.0f};
    char *translated = CG_SafeTranslateString_Internal("cgame", "CGAME_SPECTATOR");
    int32_t trailingZero = 0;
    int32_t scaleBits = CG_FloatBits(CG_SPEC_SCALE);
    int32_t yBits = CG_FloatBits(CG_SPEC_Y);

    /* 0x3001b755..0x3001b781: the scale word and the three later draw words are
     * materialized before the syscall; only EAX's low dword is consumed. */
    int32_t width = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_R_TEXT_WIDTH, (intptr_t)translated, CG_SPEC_STYLE, scaleBits, 0));

    /* Center horizontally: x = (640.0f - width) * 0.5f. 0x3001b791 FILD width;
     * FSUBR 640.0f; FMUL 0.5f; FSTP -- width is loaded straight into the FSUBR with
     * no FSTP DWORD, so the implicit int->float conversion must stay exact (no
     * (float) cast, which would round width under -std=c11). */
    float x = (float)(((long double)CG_SPEC_SCREEN_WIDTH - (long double)width) * (long double)CG_SPEC_HALF);

    cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(x), yBits, CG_SPEC_STYLE, scaleBits, (intptr_t)color, (intptr_t)translated, trailingZero, 0,
                  CG_SPEC_MODE);
}
