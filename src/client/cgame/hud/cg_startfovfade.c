// Source: uo_cgame_mp_x86.dll 0x3001ab50..0x3001ab81
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001ab50_3001ab81.mcode

#include "../client_recovered.h"
#include "../globals.h"

#define CG_FOV_FADE_BYTE_SCALE 0.0039215689f

/*
 * CG_StartFovFade — seed the cg_fovFade timed alpha animator (cgFovFade_t at
 * 0x3044b69c) for a new screen fade, then decide whether the fade is already
 * finished.
 *
 * Calling convention (nonstandard; proven from the two callers inside CG_CalcFov
 * at 0x300400db and 0x3004010b):
 *   - startTime    arrives in ECX      (caller: LEA ECX,[EDX-0xa] = cg.time - 10)
 *   - durationMs   arrives in EAX      (caller: MOV EAX,1)
 *   - numerator255 is one 32-bit stack slot the caller PUSHes (PUSH 0xff = 255)
 *     and cleans afterward (ADD ESP,4); the wrapper issues a plain RET.
 * So this is a fastcall-style ECX + EAX register pair plus one cdecl stack arg.
 *
 * Body (0x3001ab50..0x3001ab81), per instruction:
 *   FILD  dword [ESP+4]           ; load numerator255 as an integer onto ST(0)
 *   MOV   [0x3044b6a4],ECX        ; cg_fovFade.startTime  = startTime
 *   ADD   ECX,EAX                 ; ECX = startTime + durationMs  (fade end time)
 *   MOV   [0x3044b6a8],EAX        ; cg_fovFade.durationMs = durationMs
 *   FMUL  float [0x3007be24]      ; ST(0) = numerator255 * (1/255)   [0x3007be24
 *                                 ;   = 0x3b808081 = 0.00392157f = 1.0f/255.0f]
 *   MOV   EAX,[0x304831b0]        ; EAX = cg.time (cg_time global, ms)
 *   CMP   ECX,EAX                 ; compare endTime vs cg.time
 *   FSTP  float [0x3044b69c]      ; cg_fovFade.startValue = numerator255 / 255.0f
 *   JG    0x3001ab80             ; if endTime >  cg.time the fade is still pending
 *                                 ;   -> leave currentValue untouched, return
 *   MOV   EAX,[0x3044b69c]        ; else (endTime <= cg.time: schedule already
 *   MOV   [0x3044b6a0],EAX        ;   expired) snap currentValue = startValue
 *   RET
 *
 * startValue is the 0..1 fade target (numerator / 255); CG_ScreenFade (0x3001a7c0)
 * later steps currentValue toward it over durationMs, and if the schedule has
 * already elapsed the target is applied immediately here. The 1/255 constant is
 * the standard 8-bit-numerator-to-normalized-alpha scale.
 *
 * Naming: the .mcode size-guess name script_func_setplayerignoreradiusdamage is
 * rejected (that is a server script builtin; this is x87 HUD/FOV fade state setup,
 * seeding a cgFovFade_t and reading cg.time). Named CG_StartFovFade by its proven
 * role, matching the storage it drives and the CG_ScreenFade evaluator that
 * consumes it.
 */
void CG_StartFovFade(int32_t startTime, int32_t durationMs, int32_t numerator255)
{
    /* The numerator's FILD occurs first and remains live while the integer state
     * is published. ADD is target-dword wrapping and precedes durationMs' store. */
    long double startValueRaw = (long double)numerator255;
    cg_fovFade.startTime = startTime;
    int32_t endTime = coduo_int32_from_bits(
        (uint32_t)startTime + (uint32_t)durationMs);
    cg_fovFade.durationMs = durationMs;
    startValueRaw *= (long double)CG_FOV_FADE_BYTE_SCALE;
    int32_t now = cg_time;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    qboolean expired = endTime <= now;
    cg_fovFade.startValue = (float)startValueRaw;

    /* CMP (startTime+durationMs), cg.time / JG: if the fade's end time is still in
     * the future the fade is pending and currentValue keeps its prior value; only
     * when the schedule has already elapsed (endTime <= cg.time) is the target
     * snapped in immediately. */
    if (expired) {
        uint32_t startValueBits = (uint32_t)CG_FloatBits(
            cg_fovFade.startValue);
        cg_fovFade.currentValue = CG_FloatFromBits(startValueBits);
    }
}
