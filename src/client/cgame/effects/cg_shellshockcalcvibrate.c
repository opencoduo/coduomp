// Source: uo_cgame_mp_x86.dll 0x3003c630..0x3003c742
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003c630_3003c742.mcode
//
// CG_ShellShockCalcVibrate — advance the shellshock screen-blur displacement
// for the current frame. This is the third of the three per-frame sub-updates the
// shellshock dispatcher CG_UpdateShellShock (0x3003c750) fans out to, alongside the
// camera update (0x3003c230) and the mouse-input update
// CG_UpdateShellShockMouse (0x3003c530). It writes the screen-blur amount pair
// cg_shellshockScreenBlurX/Y (0x3048bff0/0x3048bff4) — the same vec2 CG_EndShellShock
// (0x3003c1d0) clears to 0 and the screen-blur draw pass (0x3003b670) reads.
//
// Naming: the .mcode header's "GScr_SetCursorHint" guess is REJECTED — it is an
// explicit size match ("win size 0x112, matched size 0x114"), and this body has no
// script-arg parse, no enum validation, and no trap/syscall of its own. It is pure
// x87 float math that reads a blur-parameter block and a precomputed noise table and
// stores the two screen-blur components. The same-module PPC bank proves the
// CG_ShellShockCalcVibrate source identity.
//
// ABI: EAX = duration, EDX = the shellshock parameter block, [ESP+0x18] = elapsed.
// Proven at the sole call site 0x3003c778 (`MOV ESI,elapsed`/`MOV EAX,EDI(duration)`/
// `MOV EDX,EBX(params)`; `PUSH ESI` supplies the stack arg). The caller passes the
// same params base (EBX) it hands the camera and blend updates, so EDX points at the
// same shellshock parameter block; this update reads its decoded first three fields
// (+0x00 int divisor, +0x04 float, +0x08 float). RET is a plain near return; the
// caller cleans the one pushed stack arg (cdecl).
//
// Machine-code notes (all offsets are into the assigned .mcode):
//   * remaining = duration - elapsed (ECX = EAX - ESI). If remaining <= 0 (JG not
//     taken), both blur components are set to 0 and the function returns (c644..c65c).
//   * frac clamp (c65d..c677): p0 = params->blurDivisor (int, [EDX]). If
//     remaining < p0 then frac = (float)remaining / (float)p0 (FILD/FIDIV), else
//     frac = 1.0f (the FLD of 1.0f at 0x3007bce0 is kept). This is min(rem/p0, 1).
//   * intensity (c677..c68c): smoothstep(frac) * params->blurScale, where
//     smoothstep(t) = t*t*(3 - 2t): FADD ST,ST (2t), FSUBR 3.0f (0x3007be5c),
//     FMUL frac, FMUL frac, then FMUL [EDX+0x8]. Stored to a stack temp.
//   * phase (c68e..c695): (float)elapsed * params->blurRate ([EDX+0x4]).
//   * iphase (c699..c6ab): floor(phase) via the round-half trick — FLD the 0.5-ish
//     double at 0x3007bdb8 (0x3fdfffffff000000 ~= 0.4999999990686774), subtract from
//     phase in double precision, FISTP to int (x87 round-to-nearest => floor).
//   * w = phase - (float)iphase (c6b2..c6b6): the interpolation weight in [0,1).
//   * index = (duration*61 + iphase) & 0x7f (IMUL EAX,EAX,0x3d; ADD; AND 0x7f).
//   * table row (c6c1): LEA EAX,[index*8 + cg_shellshockRandomTable]; each row is an
//     (x,y) float pair (8 bytes), so this addresses row[index]; the two output blocks
//     read a 4-pair window row[index..index+3] (offsets +0..+0x1c).
//   * X block (c6c9..c700): cubic-interpolate the .x components of the 4-pair window
//     against w, multiply by intensity, store to cg_shellshockScreenBlurX.
//   * Y block (c702..c73c): same cubic on the .y components, store to
//     cg_shellshockScreenBlurY. The x87 stack is fully unwound (FSTP ST1/ST0 pops)
//     between and after the blocks.
//
// The cubic is evaluated in the exact operand order the x87 stream uses. For window
// points a,b,c,d (the two inner points b,c bracket the sample) and weight w:
//     A = (d - c) + b - a
//     B = (a - b) - A
//     C = B + A*w
//     D = C*w + (c - a)
//     result = D*w + b
// (= A*w^3 + B*w^2 + (c-a)*w + b), a cubic passing through b at w=0 and c at w=1.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_ShellShockCalcVibrate(int32_t duration, const shellshock_t *params, int32_t elapsed)
{
    int32_t remaining = coduo_int32_from_bits((uint32_t)duration - (uint32_t)elapsed);

    if (remaining <= 0) {
        /* Not active this frame: no blur displacement. */
        cg_shellshockScreenBlurX = 0.0f;
        cg_shellshockScreenBlurY = 0.0f;
        return;
    }

    /* Ramp-in fraction, clamped to 1.0f once remaining reaches the divisor.
     * The FILD/FIDIV quotient (0x3003c66f) is never stored — it feeds the
     * smoothstep chain straight from st0, so frac stays long double. */
    int32_t divisor = params->blurDivisor;
    long double frac = (remaining < divisor)
                           ? remaining / (long double)divisor /* FILD rem / FIDIV divisor (0x3003c66f): both exact, no float store */
                           : 1.0f;

    /* smoothstep(frac) * scale — the displacement magnitude this frame.
     * Chain order per 0x3003c677..0x3003c688: FADD ST,ST (frac+frac),
     * FSUBR 3.0f, FMUL frac, FMUL frac, FMUL blurScale, one FSTP. */
    float intensity = (3.0f - (frac + frac)) * frac * frac * params->blurScale;

    /* Phase advances with elapsed time; split into integer row + fractional weight.
     * iphase = floor(phase) via subtracting ~0.5 in double precision then rounding
     * to nearest (x87 default rounding), matching FLD double 0x3007bdb8 / FISTP. */
    float phase = (long double)elapsed * params->blurRate; /* FILD elapsed (0x3003c68e), exact; one round at the phase FSTP (0x3003c695) */
    /* long double form: the FSUB result goes straight from the x87 stack into
     * FISTP (0x3003c6ab) with no double store — same idiom as
     * Script_BiasedRoundToInt (0x3003b4b0). w likewise stays in st (0x3003c6b6,
     * no store) all the way through both cubics. */
    const long double biasedPhase = (long double)phase - (long double)(double)0.4999999990686774;
    int32_t iphase = coduo_x87_fistp_i32(biasedPhase);
    long double w = phase - iphase; /* FILD iphase (0x3003c6b2), exact; result never stored (rides st into both cubics) */

    /* Table row index: 61*duration jitters the base row per shellshock length. */
    int32_t index = (int32_t)(((uint32_t)duration * 61u + (uint32_t)iphase) & 127u);
    const float(*row)[2] = &cg_shellshockRandomTable[index];

    /* Two independent cubics over the four-pair window. Keep the original
     * A/B/C/D dependency graph in this function: the DLL has no helper call and
     * leaves w live on the x87 stack across both calculations. */
    {
        const long double a = row[0][0];
        const long double b = row[1][0];
        const long double c = row[2][0];
        const long double d = row[3][0];
        const long double A = ((d - c) + b) - a;
        const long double B = (a - b) - A;
        const long double C = B + A * w;
        const long double D = C * w + (c - a);

        cg_shellshockScreenBlurX = (D * w + b) * intensity;
    }
    {
        const long double a = row[0][1];
        const long double b = row[1][1];
        const long double c = row[2][1];
        const long double d = row[3][1];
        const long double A = ((d - c) + b) - a;
        const long double B = (a - b) - A;
        const long double C = B + A * w;
        const long double D = C * w + (c - a);

        cg_shellshockScreenBlurY = (D * w + b) * intensity;
    }
}
