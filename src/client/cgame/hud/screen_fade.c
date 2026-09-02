#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>

// Source: uo_cgame_mp_x86.dll 0x3001a7c0..0x3001a8db
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001a7c0_3001a8db.mcode
//
// CG_ScreenFade — advance the cg_fovFade timed alpha animator by real elapsed time
// and, if the resulting alpha is positive, fill the whole virtual 640x480 screen
// with an opaque-black quad at that alpha. Runs once per frame with no arguments.
//
// Naming: the .mcode size-guess name `vectosignedangles` is REJECTED. That name
// implies a vector->euler-angle conversion (atan2/FSQRT/BAMS math over a vec3_t
// input). This function reads no vector, takes no arguments, performs no atan2 or
// FSQRT, references no 180-degree/BAMS constant, and its only floating math is a
// simple linear step of one scalar toward a target. It is a timed screen-fade
// drawer. The chosen name CG_ScreenFade matches the same-module cgame_mp.dll PPC
// symbol by behavior (advance a timed fade fraction, then draw a fullscreen black
// quad); it is provisional (no cgame symbol table recovered), not a size match.
//
// The animator storage is cg_fovFade (cgFovFade_t at 0x3044b69c, defined in
// globals.h): startValue (+0x00) is the fade target, currentValue (+0x04) is the
// live alpha, startTime (+0x08) and durationMs (+0x0c) are the schedule. It is
// seeded by CG_StartFovFade (startValue = numerator/255.0) and by CG_CalcFov,
// which fixes currentValue as a 0..1 alpha. cg_screenFadeLastMs (0x300a84b8) holds
// the trap_Milliseconds() tick from the previous advance.
//
// Machine-code proof of behavior (per-instruction):
//   endTime = cg_fovFade.startTime + cg_fovFade.durationMs        (EAX+EDX)
//   ECX = 0                                                        (XOR ECX,ECX)
//   if (endTime < cg_time)  -> snap currentValue = startValue      (CMP;JL 0xa877)
//   else:
//     if (currentValue == startValue) -> go draw                  (FUCOMPP;JNP 0xa881)
//     now = trap_Milliseconds();  ECX = now                       (PUSH 6;CALL syscall)
//     deltaMs = now - cg_screenFadeLastMs                         (SUB EAX,[..b8])
//     if (deltaMs >= 500) -> go draw                              (CMP 0x1f4;JGE)
//     if (deltaMs <= 0)   -> go draw                              (TEST;JLE)
//     step = (float)deltaMs / (float)durationMs                   (FILD;FIDIV)
//     if (currentValue <= startValue):                           (FCOMP;TEST 0x41;JNZ)
//        currentValue += step                                    (FADD [..a0];FST)
//        if (currentValue < startValue) -> go draw               (FCOMP;TEST 0x41;JNZ)
//        else snap currentValue = startValue  (overshot up)
//     else:
//        currentValue -= step        (FSUBR: mem - ST0 = curr-step)(FSUBR [..a0];FST)
//        if (currentValue >= startValue) -> go draw              (FCOMP;TEST 5;JP)
//        else snap currentValue = startValue  (overshot down)
//   draw:
//     cg_screenFadeLastMs = ECX      (0 unless the trap ran)      (MOV [..b8],ECX)
//     if (currentValue > 0.0) CG_FillRect(0,0,640,480,{0,0,0,curr}) (FCOMP dbl 0.0)
//
// The 500 ms and duration divisor are the standard timed-fade frame-hitch clamp:
// no step is taken across a paused/hitched frame (deltaMs >= 500) or a zero/negative
// delta. FSUBR at 0xa837 computes mem-ST0 = currentValue - step (ST0 held the step).
// The FUCOMPP/FCOMP + FNSTSW + TEST AH,{0x44,0x41,0x05} + Jcc sequences are the
// MSVC x87 float-compare idioms; each condition above is the ordered outcome the
// TEST/branch selects (C3=0x40 equal, C0=0x01 less, C2=0x04 unordered in AH).
//
// The FILD/FIDIV operate on the ms delta stored in a stack local (MOV [ESP],EAX),
// and durationMs is divided as an integer memory operand (FIDIV dword). The final
// FCOMP is against the .rdata double 0.0 at 0x3007bcf0 (g_double_zero_0),
// so the "positive alpha" gate is a true double 0.0 compare.

/* CG_FillRect virtual-640x480 screen extents (the raw 640.0f/480.0f pushed at
 * 0x3001a8aa/0x3001a8a5). Fixed screen size, so plain float literals. */
enum {
    CG_SCREEN_VIRTUAL_WIDTH = 640,
    CG_SCREEN_VIRTUAL_HEIGHT = 480
};

/* Only advance the fade for a plausible per-frame real-time delta: strictly more
 * than 0 ms and strictly less than this cap (skips paused/hitched frames). */
enum {
    CG_SCREEN_FADE_MAX_STEP_MS = 500
};

void CG_ScreenFade(void)
{
    /* ECX in the machine code: the value written back to cg_screenFadeLastMs at the
     * draw label. It stays 0 unless the trap_Milliseconds() path runs and sets it to
     * `now`. Modeled explicitly to preserve that the schedule-expired and
     * already-at-target paths reset cg_screenFadeLastMs to 0. */
    int32_t lastMsWriteback = 0;

    int32_t startTime = cg_fovFade.startTime;
    int32_t durationMs = cg_fovFade.durationMs;
    int32_t endTime = coduo_int32_from_bits((uint32_t)startTime + (uint32_t)durationMs);
    int32_t cgameTime = coduo_int32_from_bits(cg_time);

    if (endTime < cgameTime) {
        /* 0xa877: schedule expired -> settle immediately at the target. */
        cg_fovFade.currentValue = cg_fovFade.startValue;
    } else {
        float targetForEquality = cg_fovFade.startValue;
        float currentForEquality = cg_fovFade.currentValue;

        if (currentForEquality != targetForEquality) {
        /* Fade still scheduled and not yet at target: step by real elapsed time. */
            int32_t now = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_MILLISECONDS));
            int32_t deltaMs = coduo_int32_from_bits((uint32_t)now - (uint32_t)cg_screenFadeLastMs);
            lastMsWriteback = now;

            if (deltaMs < CG_SCREEN_FADE_MAX_STEP_MS && deltaMs > 0) {
                float currentForDirection = cg_fovFade.currentValue;
                float targetForDirection = cg_fovFade.startValue;
                long double step = (long double)deltaMs / (long double)cg_fovFade.durationMs;

                /* TEST AH,0x41 sends less, equal, and unordered through the
                 * rising arm; only ordered greater selects the falling arm. */
                if (!(currentForDirection > targetForDirection)) {
                    /* 0xa85e: rising toward the target. */
                    long double candidate = step + cg_fovFade.currentValue;
                    cg_fovFade.currentValue = (float)candidate;
                    if (candidate > cg_fovFade.startValue) {
                        /* overshot up -> snap to target; equality already stored it */
                        cg_fovFade.currentValue = cg_fovFade.startValue;
                    }
                } else {
                    /* 0xa837: falling toward the target (currentValue -= step). */
                    long double candidate = (long double)cg_fovFade.currentValue - step;
                    cg_fovFade.currentValue = (float)candidate;
                    if (candidate < cg_fovFade.startValue) {
                        /* overshot down -> snap to target */
                        cg_fovFade.currentValue = cg_fovFade.startValue;
                    }
                }
            }
            /* deltaMs out of range: leave currentValue unchanged and fall through. */
        }
    }
    /* currentValue == startValue on the scheduled path: unchanged, fall through. */

    /* 0xa887: commit the writeback tick (0 unless the trap ran this call). */
    cg_screenFadeLastMs = lastMsWriteback;

    /* 0xa88d: draw only while the alpha is strictly positive (double-0.0 compare). */
    if (cg_fovFade.currentValue > 0.0f) {
        /* 0xa89a..: build color (0,0,0,alpha) in stack locals and fill the whole
         * virtual screen. The four color dwords are written 0,0,0 then the alpha
         * from currentValue (MOV [ESP+0x24],ECX where ECX = cg_fovFade.currentValue
         * bits). */
        float alpha = cg_fovFade.currentValue;
        float color[4];
        color[0] = 0.0f;
        color[1] = 0.0f;
        color[2] = 0.0f;
        color[3] = alpha;

        CG_FillRect(0.0f, 0.0f, (float)CG_SCREEN_VIRTUAL_WIDTH, (float)CG_SCREEN_VIRTUAL_HEIGHT, color);
    }
}
