#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x30045480..0x30045543
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30045480_30045543.mcode
//
// CG_TrackAdsZoomDirection — once-per-frame tracker that detects when the local
// player's aim-down-sight (ADS) transition *begins* and records its direction
// into cg_adsZoomingIn, then advances cg_prevAdsFraction for the next frame.
//
// The .mcode header's size-matched guess "CG_HudElemTenthsTimerString" is
// REJECTED. That name implies formatting a timer into a "M:SS.t" string — this
// function has NO snprintf/format call, NO integer div/mod, NO strings, and no
// stack frame at all. It is pure x87 float comparisons over cgame ADS globals
// plus two dword stores. The name here is proven by the call graph: this is the
// sole writer of cg_adsZoomingIn (0x30487954) and cg_prevAdsFraction
// (0x30487950), and CG_CalcAdsOverlayFrac (0x30019520) reads cg_adsZoomingIn to
// choose the weapon's adsZoomInFrac (+0x278) vs adsZoomOutFrac (+0x27c) when
// building the ADS overlay. Exact original CoD source symbol unproven; role name.
//
// ABI: no arguments (first instruction is a global load), no stack frame, plain
// RET (two RET sites). Returns void.
//
// x87 compare idiom used throughout: `FLD A ; FLD B ; FUCOMPP ; FNSTSW AX ;
// TEST AH,0x44 ; JNP t` is MSVC's ordered-equality test — the branch is taken
// (JNP) exactly when B == A and both are ordered (the 0x44 mask = C3|C2, whose
// popcount is even for {less, greater, unordered} => PF=1 => JNP not taken, and
// odd only for equal => PF=0 => JNP taken). A paired `JP t` is the negation
// (taken when B != A or unordered).

void CG_TrackAdsZoomDirection(void)
{
    /*
     * 0x30045480..0x30045496: gate on the current predicted weapon supporting
     * ADS at all.
     *   EAX = cg_predictedPlayerState.currentWeapon        ([0x3048329c])
     *   ECX = bg_weaponInfos                          ([0x30134cd8])
     *   EDX = bg_weaponInfos[currentWeapon]           ([ECX + EAX*4])
     *   EAX = weaponInfo->adsEnabled                 ([EDX + 0x328])
     * If adsEnabled == 0 (TEST EAX,EAX / JZ 0x30045542) return immediately,
     * preserving cg_prevAdsFraction as well as the latched direction.
     */
    const weaponInfo_t *weaponInfo =
        bg_weaponInfos[cg_predictedPlayerState.currentWeapon];

    if (weaponInfo->adsEnabled == 0) {
        return;
    }

    {
        /*
         * The zoom flag is only (re)computed at the *start* of a transition,
         * i.e. when this frame's fraction is strictly between the endpoints and
         * the previous frame's fraction sat exactly on an endpoint. Any of the
         * following early-outs falls through to the tail, which advances
         * cg_prevAdsFraction WITHOUT touching cg_adsZoomingIn (leaving the flag
         * latched from whenever the transition began).
         *
         * 0x3004549c: if (ads == 1.0f) -> tail          (FLD 1.0; FLD ads; ==)
         * 0x300454b5: if (ads == 0.0f) -> tail          (FLD 0.0; FLD ads; ==)
         * Both endpoints of ads exit: only a mid-transition ads proceeds.
         */
        if (cg_predictedPlayerState.adsFraction != 1.0f &&
            cg_predictedPlayerState.adsFraction != 0.0f) {
            /*
             * 0x300454ca..0x300454f2: require prev to be exactly on an endpoint.
             *   0x300454ca: if (prev == 1.0f) skip the 0.0f test (JNP 0x300454f4)
             *   0x300454df: else if (prev != 0.0f) -> tail (JP 0x30045538)
             * So control reaches the direction test only when prev is exactly
             * 0.0f (was fully hip-fire last frame) or exactly 1.0f (was fully
             * zoomed last frame) — the first frame the transition is underway.
             */
            if (cg_prevAdsFraction == 1.0f || cg_prevAdsFraction == 0.0f) {
                /*
                 * 0x300454f4: if (ads == prev) -> tail. (Redundant with the
                 * endpoint guards above given prev is on an endpoint and ads is
                 * not, but emitted by the compiler; preserved for fidelity.)
                 *
                 * 0x30045509..0x3004551a: direction.
                 *   FLD ads ; FCOMP prev ; FNSTSW AX ; TEST AH,0x1 ; JNZ ...
                 *   AH bit 0x1 is C0 (== CF), set when ads < prev. So:
                 *     ads <  prev  -> zooming OUT  -> cg_adsZoomingIn = qfalse
                 *     ads >  prev  -> zooming IN   -> cg_adsZoomingIn = qtrue
                 */
                if (cg_predictedPlayerState.adsFraction != cg_prevAdsFraction) {
                    const float directionAds =
                        cg_predictedPlayerState.adsFraction;
                    const float directionPrev = cg_prevAdsFraction;
                    /* FCOMP/TEST C0 routes unordered with the less-than path. */
                    if (!(directionAds >= directionPrev)) {
                        /* 0x30045531: zooming out. */
                        cg_adsZoomingIn = qfalse;
                    } else {
                        /* 0x3004551c: zooming in. */
                        cg_adsZoomingIn = qtrue;
                    }
                }
            }
        }
    }

    /*
     * 0x30045526/0x3004552b and 0x30045538/0x3004553d: every path after the ADS
     * support gate copies this frame's ADS fraction for the next frame. The
     * machine code moves the raw dword ([0x304832a4] -> EAX -> [0x30487950]); as
     * a float assignment that is a bit-exact copy.
     */
    cg_prevAdsFraction = cg_predictedPlayerState.adsFraction;
}
