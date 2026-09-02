// Source: uo_cgame_mp_x86.dll 0x30019520..0x300195da
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30019520_300195da.mcode
//
// CG_CalcAdsOverlayFrac — compute the ADS (aim-down-sight) scope/overlay display
// fraction for the current predicted weapon, and return whether that overlay is
// active this frame.
//
// The out-fraction pointer arrives in ECX (register-argument ABI; PUSH ECX at
// entry both reserves a 4-byte float scratch slot and preserves that register),
// and is modeled here as a normal `float *outFrac` parameter. Result convention:
// AL/EAX -> qboolean (MOV EAX,1 / XOR EAX,EAX; RET, caller-cleans-nothing).
//
// Behavior (all steps proven against the .mcode):
//   *outFrac = 0.0f;                                            (30019526)
//   wi = cg_currentWeaponInfo;                                  (3001952c)
//   if (wi->adsOverlayShader[0] == '\0' &&                      (30019535..3001953e)
//       wi->adsOverlayReticle == 0) return qfalse;              (30019540..30019548)
//   if (adsFraction != 0.0f) {                                  (3001954e..3001955e; FUCOMPP)
//       // zooming in uses adsZoomInFrac, else adsZoomOutFrac
//       frac = wi->adsZoom{In,Out}Frac;                         (mode = cg_adsZoomingIn)
//       num  = adsFraction - (1.0f - frac);                     (FLD 1.0; FSUB frac; FSUBR t)
//       *outFrac = num;                                         (FST [ECX], no pop)
//       if (num > 0.0f) *outFrac = num / frac;                  (FCOM 0.0; only divide if >0)
//   }
//   return !(*outFrac <= 0.01f);                               (300195c0..300195cd; FCOMP 0.01)
// The final TEST/JP treats unordered as active, exactly like a value > 0.01.
//
// Constants recovered from .rdata: [0x3007bcec]=0.0f, [0x3007bce0]=1.0f,
// [0x3007bdb4]=0.01f. cg_currentWeaponInfo (0x30487980) is bg_weaponInfos[current-
// weapon] cached per frame; cg_adsZoomingIn (0x30487954) is set at 0x30045521 when
// this frame's adsFraction rose above last frame's; cg_predictedPlayerState.adsFraction
// (0x304832a4) is cg.predictedPlayerState.adsFraction (+0xe0).
//
// Name evidence: the ADS-overlay gate (adsOverlayShader[0]/adsOverlayReticle),
// the zoom-in/zoom-out easing over adsFraction, and the HUD/draw address cluster
// (0x30019...) identify this as the ADS overlay-fraction helper. The mechanical
// size-guess name "SP_trigger_mount_no_brush" is rejected: that is a server GSC
// map-entity spawn function, whereas this is a pure x87 cgame view computation over
// weaponInfo/adsFraction globals, with no strings, entity args, or trap calls.
// (size 0xba was matched by size alone, which the contract forbids.)

#include "client/cgame/globals.h"          /* cg_currentWeaponInfo, cg_adsZoomingIn,
                               * cg_predictedPlayerState.adsFraction */
#include "client/cgame/client_recovered.h" /* weaponInfo_t */

/* Overlay is "active" once its computed fraction exceeds this small threshold. */
#define CG_ADS_OVERLAY_ACTIVE_THRESHOLD 0.01f

qboolean CG_CalcAdsOverlayFrac(float *outFrac)
{
    weaponInfo_t *wi;
    float adsFraction;
    float frac;
    qboolean zoomingIn;
    long double num;   /* stays in st0 unrounded: FST [ECX] (3001957e) stores a
                        * float copy but the FCOM 0.0 (30019578) and the FDIV
                        * (3001958d) both consume the UNROUNDED 80-bit value */

    /* MOV EAX,[0x304832a4] read before the *outFrac clear (order preserved via
     * the [ESP] scratch slot); MOV dword ptr [ECX],0 zeroes the output. */
    adsFraction = cg_predictedPlayerState.adsFraction;
    *outFrac = 0.0f;

    wi = cg_currentWeaponInfo;

    /* 3001953b CMP byte ptr [EAX],0 / 30019546 TEST EAX,EAX: proceed only if the
     * weapon has an ADS overlay shader OR the overlay-reticle ADS-reduce gate is
     * set; otherwise bail with qfalse. */
    if (wi->adsOverlayShader[0] == '\0' && wi->adsOverlayReticle == 0) {
        return qfalse;
    }

    /* 30019557 FUCOMPP adsFraction vs 0.0 / TEST AH,0x44 / JNP: skip the mapping
     * when adsFraction is exactly 0.0 (the output stays 0.0). */
    if (adsFraction != 0.0f) {
        /* 30019560 MOV EAX,[0x30487954] / TEST / JZ: select the easing fraction
         * for the current zoom direction. */
        zoomingIn = cg_adsZoomingIn;
        if (zoomingIn) {
            frac = wi->adsZoomInFrac;    /* +0x278 */
        } else {
            frac = wi->adsZoomOutFrac;   /* +0x27c */
        }

        /* FLD 1.0; FSUB frac; FSUBR adsFraction: num = adsFraction - (1.0 - frac). */
        num = (long double)adsFraction - (1.0L - (long double)frac);

        /* FST [ECX]: store the raw numerator first (no pop). */
        *outFrac = num;

        /* FCOM 0.0 / TEST AH,0x41 / JNZ: divide by the easing fraction only when
         * the numerator is strictly positive; otherwise keep the raw value. */
        if (num > 0.0f) {
            /* The target reloads cg_currentWeaponInfo after the numerator store,
             * then reads the divisor from the branch selected by the earlier
             * cg_adsZoomingIn load. */
            weaponInfo_t *divisionWi = cg_currentWeaponInfo;
            float divisor = zoomingIn ? divisionWi->adsZoomInFrac : divisionWi->adsZoomOutFrac;
            *outFrac = (float)(num / (long double)divisor);
        }
    }

    /* 300195c0 FLD [ECX]; FCOMP 0.01 / TEST AH,0x41 / JP: active when
     * strictly greater OR unordered. */
    if (!(*outFrac <= CG_ADS_OVERLAY_ACTIVE_THRESHOLD)) {
        return qtrue;
    }
    return qfalse;
}
