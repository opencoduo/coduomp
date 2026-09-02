// Source: uo_cgame_mp_x86.dll 0x3003b7e0..0x3003b871
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003b7e0_3003b871.mcode
//
// CG_UpdateFadeOverlay — advance one timed 2D fade overlay for the current frame
// and, while it is visible, drive it through the engine via the CG_R_SAVE_SCREEN /
// CG_R_BLEND_SAVED_SCREEN syscall pair.
//
// Register-arg ABI (custom, proven from both call sites 0x3001c112 and 0x3001c46e,
// which set the registers from cg_snap-derived state and clean nothing on return):
//   EAX = startTime   (the overlay's start time, ms; 0 means "no overlay")
//   ECX = duration    (the overlay's total lifetime, ms)
//   ESI = overlay     (the shellshock_t element read for fadeInTime/targetLevel)
// SUB ESP,0x10 reserves a 16-byte scratch frame; the function cleans it (ADD ESP,0x10)
// and returns with a bare RET (no callee arg cleanup — the args are in registers).
//
// Name adjudication: the .mcode header guesses `ItemParse_forecolor` purely by size
// (win 0x91 ~= 0x90). That is rejected — this function parses no menu item and writes
// no color; it computes a time-ramped level and issues the CG_R_SAVE_SCREEN/71 overlay pair.
// The engine trap names behind ids 70/71 are unproven (no cgame syscall-id table
// recovered), so the function and the traps are named by their proven role.
//
// The engine syscall pointer cgame_syscall is the .data slot 0x30085e9c that dllEntry
// (0x3003d470) fills at load time; CG_R_SAVE_SCREEN/71 route through it (see
// client_recovered.h). cg_time is 0x304831b0; cg_fadeOverlayActive is 0x3048bff8.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

qboolean CG_UpdateFadeOverlay(shellshock_t *overlay, int32_t startTime,
                              int32_t duration)
{
    /* 3003b7e3 TEST EAX,EAX / JZ  0x3003b861 : startTime == 0  (no active overlay).
     * 3003b7e7 TEST ECX,ECX / JLE 0x3003b861 : duration <= 0 (signed).
     * `overlay` (ESI) is not NULL-checked here; it is only dereferenced on the
     * still-visible path below. */
    if (startTime == 0 || duration <= 0) {
        /* 3003b861: MOV [cg_fadeOverlayActive],0 ; XOR EAX,EAX ; RET */
        cg_fadeOverlayActive = 0;
        return qfalse;
    }

    /* 3003b7eb SUB EAX,[cg_time] / 3003b7f1 ADD EAX,ECX : remaining ms in overlay's
     * window = startTime - cg_time + duration. Stored to [ESP] (the FILD source). */
    int32_t remaining = coduo_int32_from_bits(
        (uint32_t)startTime - (uint32_t)cg_time + (uint32_t)duration);

    /* 3003b7f3 TEST EAX,EAX / JLE : overlay expired (remaining <= 0). */
    if (remaining <= 0) {
        cg_fadeOverlayActive = 0;
        return qfalse;
    }

    /* 3003b7fa MOV ECX,[ESI+0xc]  = fadeInTime (ramp-in window, ms)
     * 3003b7ff MOV EDX,[ESI+0x10] = targetLevel (level the overlay ramps to)
     * 3003b7fd CMP EAX,ECX / 3003b80a JGE 0x3003b833 : signed compare
     *   remaining >= fadeInTime. When the remaining time is at or beyond the ramp-in
     *   window the level is held at targetLevel; otherwise it ramps linearly. */
    int32_t fadeInTime = overlay->screenBlendFadeTime;
    int32_t level = overlay->screenBlendTime;

    if (remaining < fadeInTime) {
        /* 3003b80c..3003b82f: x87 ramp, all operands are 32-bit integers loaded
         * via FILD/FIDIV/FIMUL (signed):
         *   st0 = (float)remaining            FILD  dword [ESP]
         *   st0 = st0 / (float)fadeInTime      FIDIV dword [ESP+8]
         *   st0 = st0 * (float)targetLevel     FIMUL dword [ESP+0xc]
         *   store st0 as 32-bit float (FSTP float [ESP+0xc]) then reload it, add the
         *   double constant 2^-30 (0x3007be50 = 9.313225746154785e-10), and FISTP to
         *   a 32-bit int. FISTP uses the default round-to-nearest mode (modeled with
         *   nearbyint, which honors the current FPU rounding mode); the tiny nudge
         *   biases an exact half so it does not round down. The intermediate product
         *   is stored/reloaded at single precision, so the outer float cast is
         *   load-bearing (it truncates the mantissa exactly as FSTP float does). */
        float ramp = (float)((double)remaining / (double)fadeInTime
                             * (double)level);
        level = CG_RoundToNearest(ramp);
    }

    /* 3003b833 MOV EAX,[cg_fadeOverlayActive] / TEST / JZ 0x3003b848 :
     * only hand the engine the ramped level once the overlay was already active on
     * the previous frame (the first visible frame issues CG_R_SAVE_SCREEN alone). */
    if (cg_fadeOverlayActive != 0) {
        /* 3003b83c PUSH EDX(level) ; PUSH 0x47 ; CALL cgame_syscall ; ADD ESP,8 */
        cgame_syscall(CG_R_BLEND_SAVED_SCREEN, level);
    }

    /* 3003b848 PUSH 0x46 ; CALL cgame_syscall ; ADD ESP,4 : issued every visible
     * frame (the zero-argument begin/commit-overlay trap). */
    cgame_syscall(CG_R_SAVE_SCREEN);

    /* 3003b850 MOV EAX,1 / 3003b858 MOV [cg_fadeOverlayActive],EAX / RET */
    cg_fadeOverlayActive = 1;
    return qtrue;
}
