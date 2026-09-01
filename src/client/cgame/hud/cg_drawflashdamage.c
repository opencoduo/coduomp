#include "../client_recovered.h"
#include "../globals.h"

#include <math.h>
#include <stddef.h>

// Source: uo_cgame_mp_x86.dll 0x3001a8e0..0x3001a977
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001a8e0_3001a977.mcode
//
// CG_DrawFlashDamage — draw the red "took damage" full-screen flash overlay.
//
// Naming: the .mcode's size-matched "DynaSink" guess is rejected. DynaSink is a
// Windows networking API name, unrelated to this code; this function is a CoD HUD
// draw. Behavior + call graph resolve it: it gates on cg_snap, uses the
// cg_damageFlashEndTime/cg_damageFlashScale effect state (set to cg_time+500 on
// damage at 0x30034d1e), and paints a red full-screen rect via CG_FillRect. The
// same-module PPC bank lists cgame_mp!CG_DrawFlashDamage among the flash/blend
// draws; this matches its red-flash role (its sibling CG_DrawFlashFade is a
// separate function).
//
// Machine-code proof (entry ESP = E; after SUB ESP,0x14 the frame base is
// S = E-0x14; the 5 arg PUSHes then move ESP to S-0x14, and the color[] locals
// live at S+4..S+0x10):
//
//   MOV EAX,[cg_snap]; SUB ESP,0x14; TEST EAX,EAX; JZ  end   ; no snapshot -> skip
//   MOV EAX,[cg_damageFlashEndTime]; MOV ECX,[cg_time]
//   CMP EAX,ECX; JLE end                                     ; signed endTime<=now -> skip
//   SUB EAX,ECX; MOV [S+0],EAX; FILD [S+0]                   ; remaining = endTime-cg_time (int)
//   FMUL [0.002 = 1/500]        (0x3007bf28)                 ; remaining/500
//   FMUL [cg_damageFlashScale]  (0x3048af10)                 ; * scale (signed)
//   FABS
//   FCOM [5.0] (0x3007bde0); FNSTSW AX; TEST AH,0x41; JNZ keep
//     ; TEST AH,0x41 tests C3|C0 (ST0<=5.0); if set (<=5.0) keep, else clamp:
//   FSTP ST0; FLD [5.0]                                      ; f = min(f, 5.0)
// keep:
//   FMUL [0.2] (0x3007be10)                                  ; * 0.2
//   LEA EAX,[S+4]; PUSH EAX                                  ; &color  (CG_FillRect arg5)
//   PUSH 0x43f50000 (490.0f)                                 ; height
//   PUSH 0x44228000 (650.0f)                                 ; width
//   FMUL [0.7] (0x3007bf24)                                  ; * 0.7   (final alpha)
//   PUSH 0xc1200000 (-10.0f)                                 ; y
//   PUSH 0xc1200000 (-10.0f)                                 ; x
//   FSTP [S+0x10]                                            ; color[3] = alpha
//   MOV [S+4], 0x3e4ccccd (0.2f)                             ; color[0] = 0.2 (red)
//   MOV [S+8], 0                                             ; color[1] = 0.0 (green)
//   MOV [S+0xc], 0                                           ; color[2] = 0.0 (blue)
//   CALL CG_FillRect                                         ; fill (-10,-10,650,490,color)
//   ADD ESP,0x14 (caller-cleaned args); ADD ESP,0x14; RET
//
// Constants in natural form (raw hex kept above for traceability):
//   0.002f = 1/500 (window is 500 ms); 0.2f, 0.7f fade factors; 5.0f alpha cap.
//   Color is (0.2, 0.0, 0.0) — a dark red damage tint.
// The rect (-10,-10,650,490) covers the full virtual 640x480 screen with a 10px
// overscan, so the tint has no visible border.

void CG_DrawFlashDamage(void)
{
    // Gate: nothing to draw until a snapshot is installed.
    if (cg_snap == NULL) {
        return;
    }

    // Signed CMP/JLE: both dwords are retained for the following SUB.
    int32_t endTime = cg_damageFlashEndTime;
    int32_t cgameTime = coduo_int32_from_bits(cg_time);
    if (endTime <= cgameTime) {
        return;
    }

    // remaining = endTime - cg_time, converted int -> float by FILD.
    int32_t remaining = coduo_int32_from_bits((uint32_t)endTime -
                                         (uint32_t)cgameTime);

    // alpha ramps down over the 500 ms window, scaled by the (signed) flash
    // magnitude; fabs()'d then capped at 5.0, then attenuated by 0.2 * 0.7.
    //
    // The whole chain is ONE 80-bit x87 computation with exactly ONE rounding,
    // at the color[3] store (FSTP [ESP+0x24], 0x3001a94f):
    //   FILD remaining        (0x3001a904; no float store of the cast)
    //   FMUL 0.002f           (0x3001a907, [0x3007bf28] = 1/500)
    //   FMUL cg_damageFlashScale (0x3001a90d, [0x3048af10])
    //   FABS                  (0x3001a913; the bare x87 op, not a float fabsf)
    //   FCOM 5.0f             (0x3001a915; compares the UNROUNDED st0)
    //   FMUL 0.2f / FMUL 0.7f (0x3001a92a / 0x3001a93f)
    // Hence `long double`: a float `alpha` local would round at the fabs, at the
    // clamp and again at the 0.2*0.7 attenuation -- three roundings the DLL does
    // not perform. The clamp's not-taken path (FSTP ST0; FLD 5.0f, 0x3001a922)
    // DISCARDS st0 and reloads the float 5.0f, which the assignment models.
    long double alpha = __builtin_fabsl((long double)remaining * (1.0f / 500.0f)
                                        * cg_damageFlashScale);
    if (alpha > 5.0f) {
        alpha = 5.0f;
    }

    // The attenuation remains live through both multiplies and is stored to
    // color[3] before the three RGB dword stores.
    alpha *= 0.2f;
    alpha *= 0.7f;
    float color[4];
    color[3] = (float)alpha;
    color[0] = 0.2f;
    color[1] = 0.0f;
    color[2] = 0.0f;

    // Fill the whole virtual screen (10px overscan on all sides) with the tint.
    CG_FillRect(-10.0f, -10.0f, 650.0f, 490.0f, color);
}
