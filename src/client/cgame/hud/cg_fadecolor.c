// Source: uo_cgame_mp_x86.dll 0x3001d200..0x3001d264
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001d200_3001d264.mcode
//
// CG_FadeColor(startMsec, totalMsec) — the shared Quake3/CoD "fade a HUD element
// out over time" helper. Returns a pointer to a static vec4_t RGBA color whose
// RGB is white and whose alpha ramps 1.0 -> 0.0 during the final CG_FADE_TIME
// (100) ms of the effect's life, or NULL when the effect has not started or has
// fully expired. See client_recovered.h for the full identity/ABI evidence.
//
// Name adjudication: the .mcode header names this PM_AddTouchEnt from a pure
// win-size 0x64 == cgame_mp.dll PM_AddTouchEnt 0x64 match; REJECTED per the
// no-size-matching rule. PM_AddTouchEnt is pmove touch-list bookkeeping (void,
// no x87); this reads cg.time (0x304831b0), does elapsed-time fade math, and
// returns a color pointer, matching the PPC CG_FadeColor and its many HUD/overlay
// callers. Corroborated by CG_LatchOverlaySource (0x3001a5b0), which latches
// (cg.time, duration) as this function's (startMsec, totalMsec).
//
// Register-argument ABI (custom regparm): EDX = startMsec, ECX = totalMsec
// (caller-set; e.g. 0x3001af46: MOV EDX,startMsec / MOV ECX,100). `RET` with no
// immediate. Modeled as two int parameters in source order.

#include <stddef.h>  /* NULL */

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

vec_t *CG_FadeColor(int32_t startMsec, int32_t totalMsec)
{
    int32_t t;
    int32_t remaining;
    /* Float faithfulness: frac is produced in st0 (FILD+FMUL at 0x3001d21e, or
     * FLD 1.0f at 0x3001d229) and consumed by the FMUL ST1 at 0x3001d23f while
     * still in the register -- it is never stored, so it is never rounded to
     * float. The ONLY rounding in this function is the FSTP DWORD at 0x3001d25a
     * into cg_fadeColor[3]. Held as `long double` to keep the chain 80-bit. */
    long double frac;

    // 0x3001d201 TEST EDX,EDX / 0x3001d203 JZ: effect never started.
    if (startMsec == 0) {
        // 0x3001d210 XOR EAX,EAX -> return NULL.
        return NULL;
    }

    // 0x3001d205 MOV EAX,[0x304831b0] (cg.time) / 0x3001d20a SUB EAX,EDX.
    // Elapsed time since the effect started (signed int millisecond math).
    t = coduo_int32_from_bits(cg_time - (uint32_t)startMsec);

    // 0x3001d20c CMP EAX,ECX / 0x3001d20e JL: continue only while t < totalMsec
    // (signed). Otherwise the effect has fully expired.
    if (t >= totalMsec) {
        // 0x3001d210 XOR EAX,EAX -> return NULL.
        return NULL;
    }

    // 0x3001d214 SUB ECX,EAX: milliseconds of life remaining.
    remaining = coduo_int32_from_bits((uint32_t)totalMsec - (uint32_t)t);

    // 0x3001d216 CMP ECX,0x64 / 0x3001d21c JGE (signed): fade only during the
    // last CG_FADE_TIME (100) ms; before that the element is at full alpha.
    if (remaining < CG_FADE_TIME) {
        // 0x3001d21e FILD dword[ESP] (the stored remaining, as a signed int) /
        // 0x3001d221 FMUL float ptr [0x3007bdb4] (= 0.01f == 1.0f/100, verified
        // byte-exact). The FILD feeds the FMUL with no intervening float store,
        // so the int stays exact in 80-bit; hence long double, not vec_t.
        frac = (long double)remaining * (1.0f / CG_FADE_TIME);
    } else {
        // 0x3001d229 FLD float ptr [0x3007bce0] (= 1.0f).
        frac = 1.0f;
    }

    // 0x3001d22f FLD [0x304583c8] (cg_hudAlpha_vmCvar.value) pushes alpha above
    // frac. The target then stores color[2], multiplies ST0 by ST1 at 0x3001d23f,
    // and stores color[1] and color[0] before committing the float alpha.
    // Stock Quake3 CG_FadeColor omits this global scale; the client applies it.
    long double alpha = (long double)cg_hudAlpha_vmCvar.value;
    cg_fadeColor[2] = 1.0f;
    long double scaledAlpha = alpha * frac;
    cg_fadeColor[1] = 1.0f;
    cg_fadeColor[0] = 1.0f;
    // 0x3001d25a FSTP float ptr [0x300a84f4] stores color[3]; 0x3001d260 FSTP
    // ST0 discards the leftover frac.
    cg_fadeColor[3] = (float)scaledAlpha;

    // 0x3001d255 MOV EAX,0x300a84e8 -> return &cg_fadeColor[0].
    return cg_fadeColor;
}
