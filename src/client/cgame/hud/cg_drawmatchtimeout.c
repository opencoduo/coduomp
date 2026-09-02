// Source: uo_cgame_mp_x86.dll 0x3001bbd0..0x3001bd1b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001bbd0_3001bd1b.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

/*
 * CG_DrawMatchTimeout (0x3001bbd0)
 *
 * Draws the screen-centered "match timeout / paused" HUD overlay. Early-outs when
 * no timeout is running, otherwise builds one display string, measures it, centers
 * it on the 640x480 virtual screen, and draws it in white through the cgame draw
 * traps.
 *
 * NAME: the .mcode header's size-guess "PM_ReloadClip" is REJECTED. This is not a
 * PM_Weapon step: it touches no playerState, no clip/reserve ammo, no bg_* weapon
 * info, and no pmove state. Its body reads the timeout HUD state (cg_timeoutEndTime
 * / cg_timeoutActive / cg_timeoutString), localizes "CGAME_PAUSED" in the "cgame"
 * domain, formats a display string, and dispatches cgame draw syscalls (trap 52
 * measure, trap 53 measure, trap 54 draw) through *0x30085e9c. The "PM_ReloadClip"
 * label was a pure win/PPC size collision (0x14b vs 0x14c) with no behavioral basis
 * (name_evidence in the .mcode admits it is a size match). The behavioral name is
 * corroborated by the same-module PPC anchor CG_DrawMatchTimeout in cgame_mp.dll.
 * It is the draw-time counterpart of CG_BuildTimeoutHudStrings (0x3002de90), which
 * fills cg_timeoutEndTime / cg_timeoutActive / cg_timeoutString from the config
 * strings; this function renders them.
 *
 * Control flow / gate (0x3001bbd3..0x3001bbfa):
 *   EAX = cg_timeoutEndTime (0x30447fd0); if it is 0 the function only writes the
 *   local white color vec4 (unused on this path) and returns (JZ 0x3001bd17). So the
 *   overlay is drawn only while a timeout end time is set.
 *
 * Seconds-remaining (0x3001bc00..0x3001bc26), only used on the inactive branch:
 *   trap(6) returns the engine milliseconds (CG_MILLISECONDS, same as
 *   CG_BuildTimeoutHudStrings). remainingMs = cg_timeoutEndTime - now; the
 *   0x10624dd3 multiply + `SAR EDX,6` + sign fixup is the classic signed
 *   divide-by-1000 (remainingMs / 1000). The trailing `SHR EAX,0x1f; ADD EAX,EDX;
 *   JNS; XOR EAX,EAX` clamps a negative quotient to 0, i.e.
 *   secondsRemaining = max(0, remainingMs / 1000).
 *
 * Display string (0x3001bc28..0x3001bc70):
 *   translated = CG_SafeTranslateString_Internal("cgame", "CGAME_PAUSED")  (fastcall EAX=domain,
 *   ECX=reference at 0x3001bc36/0x3001bc3d). Then, branching on cg_timeoutActive
 *   (0x30447fd4, TEST/JZ at 0x3001bc2f/0x3001bc3b):
 *     - active   != 0: str = va("%s\n%s",      translated, cg_timeoutString)
 *     - active   == 0: str = va("%s: %i \n%s", translated, secondsRemaining,
 *                               cg_timeoutString)
 *   (the "%s\n%s" 0x30076ac4 and "%s: %i \n%s" 0x30076ab8 literals; cg_timeoutString
 *   is 0x30447fd8, pushed at 0x3001bc31 before the branch). va (0x3004e8a0) returns
 *   its rolling temp buffer, stored in ESI as the text to draw.
 *
 * Measure + center (0x3001bc72..0x3001bd02), floats forwarded to the variadic traps
 * by raw dword (FSTP float -> pushed 4-byte word), reproduced with CG_FloatBits:
 *   width  = cgame_syscall(52, str, 0, CG_FloatBits(0.5f), 0)         (trap 52)
 *   x      = (640.0f - (float)width)  * 0.5f    ; FILD/FSUBR 0x3007bf34=640.0f
 *                                               ; FMUL 0x3007bce8=0.5f
 *   height = cgame_syscall(53, 0, CG_FloatBits(0.5f))                 (trap 53)
 *   y      = (480.0f - (float)height) * 0.5f    ; FILD/FSUBR 0x3007c148=480.0f
 *                                               ; FMUL 0x3007bce8=0.5f
 * (640.0f / 480.0f / 0.5f are the .rdata constants g_const_float_640,
 * g_const_float_480, floatOneHalf, written here as
 * natural float literals. Constant addresses re-verified byte-exact via objdump to
 * avoid the adjacent-constant misread hazard.)
 *
 * Draw (0x3001bd06..0x3001bd0d), 10 pushed dwords (id + 9 args), lowest slot first:
 *   cgame_syscall(54,
 *                 CG_FloatBits(x),      // centered x
 *                 CG_FloatBits(y),      // centered y
 *                 0,                    // style
 *                 CG_FloatBits(0.5f),   // scale
 *                 &white,               // white rgba vec4 {1,1,1,1}
 *                 str,                  // text
 *                 0,                    // size slot
 *                 0,
 *                 6);                   // mode
 * white is the local vec4 written at 0x3001bbda..0x3001bbf2 (four 0x3f800000 dwords,
 * i.e. 1.0f) and passed by address (LEA ECX,[ESP+0x44] at 0x3001bcf5). This is the
 * same 10-slot trap-54 draw shape used by CG_DrawSpectatorMessage (0x3001b720); the
 * trap-52/53/54 engine services are unproven (no cgame syscall-id table recovered),
 * so they keep the honest CG_R_TEXT_WIDTH/53/54 role ids from client_recovered.h.
 *
 * ABI: no incoming source arguments (pure scratch frame). CG_SafeTranslateString_Internal is
 * caller-observed as fastcall(EAX=domain, ECX=reference) but declared as a plain
 * 2-arg C call by the shared header; va and cgame_syscall are variadic. Provisional
 * callee decls are caller-observed and superseded by each callee's own .mcode.
 */

/* Fixed CG_R_TEXT_PAINT draw parameters, proven from the pushed immediates. */
enum {
    CG_TIMEOUT_STYLE = 0, /* trap 52/54 style slot (PUSH 0)                        */
    CG_TIMEOUT_MODE  = 6, /* trailing trap 54 mode (PUSH 6)                        */
};
#define CG_TIMEOUT_SCREEN_WIDTH  640.0f /* 0x3007bf34; horizontal centering ref     */
#define CG_TIMEOUT_SCREEN_HEIGHT 480.0f /* 0x3007c148; vertical centering ref       */
#define CG_TIMEOUT_HALF          0.5f   /* 0x3007bce8; center = (dim - size) * 0.5f  */
#define CG_TIMEOUT_SCALE         0.5f   /* 0x3007bce8; measure + draw scale          */

void CG_DrawMatchTimeout(void)
{
    /* 0x3001bbd3 reads the gate before materializing the local color, then tests
     * the captured dword only after all four stores. */
    int32_t timeoutEnd = cg_timeoutEndTime;
    vec4_t white = {1.0f, 1.0f, 1.0f, 1.0f};

    /* No timeout end time set -> draw nothing. */
    if (timeoutEnd == 0) {
        return;
    }

    /* max(0, (cg_timeoutEndTime - engineMilliseconds) / 1000). Computed with the
     * signed divide-by-1000 magic sequence; negative quotients clamp to 0. Only
     * used on the "not active" display branch below. */
    int32_t milliseconds = coduo_int32_from_bits(
        (uint32_t)cgame_syscall(CG_MILLISECONDS));
    int32_t remainingMs = coduo_int32_from_bits(
        (uint32_t)cg_timeoutEndTime - (uint32_t)milliseconds);
    int32_t secondsRemaining = remainingMs / 1000;
    if (secondsRemaining < 0) {
        secondsRemaining = 0;
    }

    /* Build the one display line. When the timeout is "active" the seconds count is
     * omitted; otherwise it is embedded as "%i". 0x3001bc28 snapshots the
     * active word before translation; each leg then performs its own translation
     * so no callback can change the selected format. */
    int32_t timeoutActive = cg_timeoutActive;
    char *str;
    if (timeoutActive != 0) {
        char *translated =
            CG_SafeTranslateString_Internal("cgame", "CGAME_PAUSED");
        str = (char *)va("%s\n%s", translated, cg_timeoutString);
    } else {
        char *translated =
            CG_SafeTranslateString_Internal("cgame", "CGAME_PAUSED");
        str = (char *)va("%s: %i \n%s", translated, secondsRemaining,
                         cg_timeoutString);
    }

    /* Measure the text and center it on the 640x480 virtual screen. */
    int32_t width = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_R_TEXT_WIDTH, (intptr_t)str, CG_TIMEOUT_STYLE,
        CG_FloatBits(CG_TIMEOUT_SCALE), 0));
    /* 0x3001bc90 FILD [width]; 0x3001bca1 FSUBR 640.0f -- width is FILDed straight
     * into the subtract (no float store), so it stays exact in 80-bit; no (float)
     * cast. */
    float x = (float)(((long double)CG_TIMEOUT_SCREEN_WIDTH -
                       (long double)width) * (long double)CG_TIMEOUT_HALF);

    int32_t height = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_R_TEXT_HEIGHT, 0, CG_FloatBits(CG_TIMEOUT_SCALE)));
    /* 0x3001bcd3 FILD [height]; 0x3001bcdf FSUBR 480.0f -- direct FILD, no float
     * store; height stays exact in 80-bit. No (float) cast. */
    float y = (float)(((long double)CG_TIMEOUT_SCREEN_HEIGHT -
                       (long double)height) * (long double)CG_TIMEOUT_HALF);

    /* Draw the centered line in white (same 10-slot trap-54 draw shape as the
     * sibling emitters). */
    cgame_syscall(CG_R_TEXT_PAINT,
                  CG_FloatBits(x),
                  CG_FloatBits(y),
                  CG_TIMEOUT_STYLE,
                  CG_FloatBits(CG_TIMEOUT_SCALE),
                  (intptr_t)white,
                  (intptr_t)str,
                  0,
                  0,
                  CG_TIMEOUT_MODE);
}
