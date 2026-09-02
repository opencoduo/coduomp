// Source: uo_cgame_mp_x86.dll 0x3003f9a0..0x3003f9e5
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003f9a0_3003f9e5.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <string.h>

/*
 * CG_SettleViewOriginZ — a no-arg, no-return helper that applies a time-decaying
 * subtraction to the view-origin Z (cg_refdef.vieworg[2], 0x30487a98) over a fixed
 * 100 ms settle window. It is called once, at the tail of the view-kick evaluator
 * FUN_3003fb60 (0x3003ff59), immediately after that function has ADDed the kick
 * envelope onto cg_refdef.vieworg[2]; this call fades the residual back out.
 *
 * Mechanics (exact byte decode):
 *   elapsed = (int)cg_time - lastStamp                       (0x304831b0 - 0x304879dc)
 *   if (elapsed < 0)  lastStamp = cg_time;   // clock reset/rollback: re-baseline;
 *                                            // elapsed stays < 0 so still < 100.
 *   if (elapsed < 100) {
 *       cg_refdef.vieworg[2] -= (float)(100 - elapsed) * envelope * 0.01f;
 *   }
 *   // elapsed >= 100 -> the settle window has expired, do nothing (JGE return).
 *
 * The JNS at 0x3003f9b1 only re-baselines the stamp on a negative delta and does
 * NOT skip the math (EAX is left negative, so the CMP EAX,0x64 / JGE still falls
 * through into the subtract). FSUBR computes mem - st = viewOrgZ - product, so the
 * scaled envelope is subtracted from the view Z. The weight (100 - elapsed)/100
 * decays linearly from 1.0 at elapsed 0 to 0.0 at elapsed 100 ms.
 *
 * Globals:
 *   cg_time                         0x304831b0  current cgame time (ms)
 *   cg_weaponChangeViewOffsetTime  0x304879dc  the cg.time stamp of the last update
 *   cg_weaponChangeViewOffset  0x304879d8  the float envelope amplitude
 *   floatOneHundredth 0x3007bdb4  0.01f (bits 0x3c23d70a)
 *   cg_refdef.vieworg[2]             0x30487a98  view-origin Z (the settled quantity)
 * These are the same envelope/stamp pair CG_EntityEvent (0x30022810) maintains for
 * the weapon-changed / low-ammo warning event (0x91); this helper reads them to
 * bleed the effect out of the view origin.
 *
 * NAME: the .mcode size-guess "Cmd_PrevVehSlot_f" is REJECTED. It was matched only
 * by byte size (win 0x45 == corpus 0x45), which the naming rules forbid, and the
 * machine code contradicts it outright: this is not a console command handler (no
 * argc/argv, no Cmd_* calls, no vehicle-slot state) — it is a void(void) float
 * routine that decays cg_refdef.vieworg[2] over time. CG_SettleViewOriginZ is a
 * behavioral name; the exact original CoD symbol is not proven.
 */
void CG_SettleViewOriginZ(void)
{
    int32_t now = coduo_int32_from_bits(cg_time);
    int32_t elapsed = coduo_int32_from_bits(cg_time - (uint32_t)cg_weaponChangeViewOffsetTime);

    /* Negative delta => the cg.time base moved backward (map restart / rollback):
     * re-baseline the stamp. elapsed is left negative, so the window test below
     * still treats us as inside the 100 ms window. */
    if (elapsed < 0) {
        cg_weaponChangeViewOffsetTime = now;
    }

    if (elapsed < 100) {
        float k; /* 0.01f from the .rdata float pool */
        memcpy(&k, &floatOneHundredth, sizeof(k));
        /* 0x3003f9c8: FILD(100-elapsed); FMUL offset; FMUL k; FSUBR vieworg[2];
         * FSTP. One 80-bit chain -- the count feeds a bare FILD (no (float) cast
         * rounding, Class 4) and there is no intermediate float `amount` store. */
        int32_t remaining = coduo_int32_from_bits(100u - (uint32_t)elapsed);
        long double amount = (long double)remaining * cg_weaponChangeViewOffset * k;
        cg_refdef.vieworg[2] = (float)((long double)cg_refdef.vieworg[2] - amount); /* FSUBR: mem - st */
    }
}
