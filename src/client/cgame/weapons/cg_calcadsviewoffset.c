#include "../client_recovered.h"

// Source: uo_cgame_mp_x86.dll 0x300451a0..0x30045225
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300451a0_30045225.mcode
//
// CG_CalcAdsViewOffset — compute the aim-down-sight (ADS) view-relative offset
// for a world point `pos`, storing the result into the cgame global vec3
// cg_adsViewOffset, or zeroing it when ADS is not active.
//
// The .mcode header's size-matched guess "script_method_scriptbuiltin_setrightarc"
// is REJECTED: that is a server GSC script builtin (takes script arguments,
// returns void, touches the script VM); this function has no strings, no
// parameters on the stack, and no calls — it is a pure x87 projection over
// cgame globals. Name is role-derived from the ADS gating; exact original source
// name unproven.
//
// Register-argument ABI: `pos` (the world point to project) arrives in ECX
// (caller-set), no stack args, plain `RET`. Modeled here as a single vec3
// pointer parameter; no calling-convention attribute is added because the
// syntax-only build does not require one.

void CG_CalcAdsViewOffset(const vec3_t pos)
{
    /*
     * 0x300451a0..0x300451b6: look up the current predicted weapon's definition
     * and test its ADS-support flag.
     *   EDX = bg_weaponInfos               (base of weaponInfo_t* array, 0x30134cd8)
     *   EAX = cg.predictedPlayerState.currentWeapon           (0x3048329c)
     *   EAX = bg_weaponInfos[currentWeapon]      (MOV EAX,[EDX + EAX*4])
     *   EDX = weaponInfo->adsEnabled            (MOV EDX,[EAX + 0x328])
     * If adsEnabled == 0 (TEST EDX,EDX / JZ 0x30045206) fall straight to the
     * zeroing path.
     */
    const weaponInfo_t *weaponInfo = bg_weaponInfos[cg_predictedPlayerState.currentWeapon];

    if (weaponInfo->adsEnabled != 0) {
        /*
         * 0x300451b8..0x300451cd: adsFraction > 0.0f ?
         *   FLD [0x304832a4]  -> adsFraction
         *   FLD [0x3007bcec]  -> 0.0f
         *   FLD ST1           -> copy of adsFraction
         *   FUCOMPP           -> compare adsFraction vs 0.0f, pop both operands
         *   FNSTSW AX ; TEST AH,0x44 ; JNP 0x30045204
         * The 0x44/JNP idiom is the standard `if (x > y)` test: the fall-through
         * (compute) path is taken only when adsFraction is ordered and strictly
         * greater than 0.0f; adsFraction <= 0.0f (or NaN) branches to the zeroing
         * path. The FLD ST1 copy leaves adsFraction on the x87 stack (ST0) as the
         * multiplier used by the compute path below.
         */
        if (cg_predictedPlayerState.adsFraction > 0.0f) {
            /*
             * 0x300451cf..0x30045201: project `pos` relative to the view origin
             * and scale each component by adsFraction.
             *   FLD [ECX+i]  - pos[i]
             *   FSUB [cg_refdef.vieworg+i]
             *   FMUL ST1     - * adsFraction (the copy still on the stack)
             *   FSTP [cg_adsViewOffset+i]
             * The trailing FSTP ST0 (0x30045201) pops the leftover adsFraction.
             */
            cg_adsViewOffset[0] =
                (float)(((long double)pos[0] - (long double)cg_refdef.vieworg[0]) * (long double)cg_predictedPlayerState.adsFraction);
            cg_adsViewOffset[1] =
                (float)(((long double)pos[1] - (long double)cg_refdef.vieworg[1]) * (long double)cg_predictedPlayerState.adsFraction);
            cg_adsViewOffset[2] =
                (float)(((long double)pos[2] - (long double)cg_refdef.vieworg[2]) * (long double)cg_predictedPlayerState.adsFraction);
            return;
        }
        /* 0x30045204: FSTP ST0 pops the adsFraction copy before zeroing. */
    }

    /*
     * 0x30045206..0x30045224: ADS inactive (weapon has no ADS, or adsFraction is
     * not > 0). Zero the offset vector as three raw dwords.
     */
    cg_adsViewOffset[0] = 0.0f;
    cg_adsViewOffset[1] = 0.0f;
    cg_adsViewOffset[2] = 0.0f;
}
