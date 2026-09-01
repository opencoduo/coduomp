// Source: uo_cgame_mp_x86.dll 0x3003c530..0x3003c628
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003c530_3003c628.mcode
//
// CG_UpdateShellShockMouse — advance the shellshock mouse-sensitivity and
// pitch/yaw speed-limit envelope for the current frame. One of the three per-frame sub-updates the
// shellshock dispatcher CG_UpdateShellShock (0x3003c750) fans out to: sound,
// this mouse update, and the screen-blur vibration calculation. It writes the
// interpolated sensitivity multiplier at 0x3048bfec and sends maximum pitch/yaw
// speeds through CG_SET_SHELLSHOCK_MOUSE_LIMITS.
//
// Naming: the .mcode header's size-matched "vectosignedpitch" guess is REJECTED —
// this function does no signed-pitch conversion; it gates and interpolates the
// shellshock mouse fields. The recovered engine trap arm and same-module PPC
// symbol prove the mouse role.
//
// ABI: the shellshock_t pointer arrives in ECX (register arg — the caller does
// `MOV ECX,EBX` then a plain CALL); startTime/endTime arrive as the two 32-bit
// stack args pushed right-to-left (PUSH endTime; PUSH startTime). Expressed here as
// a normal 3-arg C function; the register-in-ECX detail is a calling-convention
// fact recorded in this comment, not source-level behavior. RET is a plain near
// return with cdecl caller cleanup (the ADD ESP,0xc + ADD ESP,8 before each RET
// balance the trap args and the two local stack slots reserved by SUB ESP,8).
//
// Machine-code notes:
//   * `t = endTime - startTime` (MOV EDX,[ESP+0xc]=startTime; MOV EAX,[ESP+0x10]=
//     endTime; SUB EAX,EDX). Signed 32-bit subtraction; compared signed to
//     mouseFadeTime (CMP + JGE) and to 0 (TEST + JLE).
//   * The float constant at 0x3007bce0 is 1.0f (0x3f800000), used as the fade
//     baseline and the fraction reference.
//   * The in-progress leg's FUCOMPP/FNSTSW/TEST AH,0x44/JP compares the fraction
//     `t/blendDuration` against 1.0f: parity is even (JP taken -> in-progress leg)
//     for any value other than exactly 1.0f, including t/dur > 1.0f and unordered;
//     exactly 1.0f falls through to the finished (end-state) leg. Since t<duration
//     here, the normal case is fraction<1.0f -> in-progress leg.
//   * Finished leg installs mouseSensitivityScale and the raw max-pitch/max-yaw
//     floats. The in-progress leg interpolates sensitivity from 1.0 and divides
//     both speed limits by the elapsed fraction.
//   * The t<=0 leg is `ADD ESP,8; JMP 0x3003c170`: it unwinds the two local slots
//     and tail-calls CG_EndShellShockMouse.
//   * A couple of dead spills to [ESP+4]/[ESP+0xc] (register-reuse artifacts) do not
//     affect the trap arguments, which come from the recomputed float results.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <string.h>

void CG_UpdateShellShockMouse(shellshock_t *params, int32_t startTime,
                              int32_t endTime)
{
    /* [ECX+0x68]: disabled mouse effect restores neutral limits. */
    if (params->mouseEnabled == 0) {
        cg_shellshockMouseSensitivityScale = 1.0f;
        cgame_syscall(CG_SET_SHELLSHOCK_MOUSE_LIMITS, 0, 0);
        return;
    }

    int32_t t = coduo_int32_from_bits((uint32_t)endTime -
                                 (uint32_t)startTime);
    int32_t duration = params->mouseFadeTime;

    if (t < duration) {
        if (t <= 0) {
            /* Not started yet (or negative): restore neutral mouse behavior. */
            CG_EndShellShockMouse();
            return;
        }

        /* Fade in progress: frac in (0,1). The DLL forms t/duration with
         * FILD;FIDIV (0x3003c58e/0x3003c592) and keeps the quotient in st(0)
         * through the compare and the lerp -- it is never stored to a float
         * slot -- so frac is long double (a float local would round where the
         * DLL does not). No (float) casts on t/duration either: they enter the
         * divide exact as integers. */
        long double frac = (long double)t / duration;
        if (frac != 1.0f) {
            /* Interpolate sensitivity from 1.0f toward the configured scale. */
            cg_shellshockMouseSensitivityScale =
                (params->mouseSensitivityScale - 1.0f) * frac + 1.0f;

            /* Scale both mouse-speed limits by 1.0f/frac (FDIVR 1.0f at
             * 0x3003c5ee). Both trap args remain raw float bit patterns. */
            long double invFrac = 1.0f / frac;
            float color = invFrac * params->mouseMaxPitchSpeed;
            float alpha = invFrac * params->mouseMaxYawSpeed;

            int32_t colorArg, alphaArg;
            memcpy(&colorArg, &color, sizeof colorArg);
            memcpy(&alphaArg, &alpha, sizeof alphaArg);
            cgame_syscall(CG_SET_SHELLSHOCK_MOUSE_LIMITS,
                          colorArg, alphaArg);
            return;
        }
        /* frac == 1.0f exactly: fall through to the finished end-state below. */
    }

    /* Finished (t >= duration, or frac exactly 1.0f): use the end-state values. */
    cg_shellshockMouseSensitivityScale = params->mouseSensitivityScale;

    int32_t colorArg, alphaArg;
    memcpy(&colorArg, &params->mouseMaxPitchSpeed, sizeof colorArg);
    memcpy(&alphaArg, &params->mouseMaxYawSpeed, sizeof alphaArg);
    cgame_syscall(CG_SET_SHELLSHOCK_MOUSE_LIMITS, colorArg, alphaArg);
}
