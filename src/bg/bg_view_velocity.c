// Source: uo_cgame_mp_x86.dll 0x30015b50..0x30015c27
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30015b50_30015c27.mcode
//
// BG_CalculateView_Velocity — subtract the player's view bob from the
// running weapon/view offset vector `angles`. Name adopted from the same-module PPC
// bank (cgame_mp.dll BG_CalculateView_Velocity), which sits with the
// BG_GetVerticalBobFactor / BG_GetHorizontalBobFactor pair this function calls.
// The .mcode header's G_IsVehicleImmune is a size-only guess and is REJECTED:
// this returns void, writes a float offset vector, and evaluates the bob-factor
// helpers scaled by the weapon's adsViewBobScale and the player's adsFraction —
// it is not a vehicle-immunity predicate.
//
// Non-default register ABI (both callers 0x30015c45 / 0x3003fbe4 prove it): the
// output vector arrives in ESI, the state parameter block in EBX, no stack args,
// plain RET. Expressed here as (angles, state). The function writes only angles[0] and
// angles[1]; the wrapper zeroes all three components before calling.

#include "bg_weapon_position.h"
#include "bg_bob.h"
#include "bg_bob_binding.h"
#include "bg_vehicle.h"
#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <string.h>

void BG_CalculateView_Velocity(bg_view_angle_state_t *state,
                               vec3_t angles)
{
#if defined(WINDOWS_BEHAVIOR)
    /* 0x30015b51..0x30015b62: EDX = state->ps; then EDI = the current weapon's
     * weaponInfo_t (bg_weaponInfos[ps->currentWeapon]). ECX = bg_weaponInfos base
     * (0x30134cd8), EAX = ps->currentWeapon, EDI = [ECX + EAX*4]. */
    playerState_t *ps = state->ps;
    int32_t currentWeapon = ps->currentWeapon;
    weaponInfo_t **weaponTable = bg_weaponInfos;
    weaponInfo_t *weaponInfo = weaponTable[currentWeapon];

    /* 0x30015b63..0x30015b83: vehicle-bob gate. If the player state carries any of
     * the vehicle-bob entity flags, the bob is only kept for a specific gunner
     * pose: vehicleType == 1 and vehiclePosition == 3. Otherwise bail with no
     * change to `angles`. (TEST [ps+0x84],0x106000 / JZ skips the check entirely for
     * a non-vehicle player.) */
    if (ps->entityStateFlags & EF_RESTRICTED_MASK) {
        if (ps->vehicleType != 1) {         /* CMP [ps+0x618],1 / JNZ 0x30015c24 */
            return;
        }
        if (ps->vehiclePosition != 3) {     /* CMP [ps+0x614],3 / JNZ 0x30015c24 */
            return;
        }
    }

    /* 0x30015b89..0x30015b9c: FLD 0.0f, FLD ps->adsFraction, FUCOMPP, TEST AH,0x44,
     * JNP. The compare/branch idiom proceeds only when adsFraction != 0.0f (equal
     * bails). No ADS interpolation -> no ADS view bob. */
    if (ps->adsFraction == 0.0f) {
        return;
    }

    /* 0x30015ba2..0x30015bb5: same idiom against weaponInfo->adsViewBobScale.
     * A weapon with no ADS view-bob transfer (scale == 0.0f) contributes nothing. */
    if (weaponInfo->adsViewBobScale == 0.0f) {
        return;
    }

    /* 0x30015bb7..0x30015be3: bob phase angle from the 0..255 bob cycle.
     *   phase = (float)(ps->bobCycle & 0xff) * (2*PI/255) + 2*PI
     * The FMUL/FADD .rdata constants are the exact 32-bit floats 0.024639944
     * (2*PI/255, 0x3007be48) and 6.2831855 (2*PI, 0x3007be44); read from their
     * shared bit patterns to preserve machine-code fidelity. */
    /* The bob amplitude used as both amp input and upper clamp; the 45.0f constant
     * (PUSH 0x42340000) is the fixed maxAmp passed to both bob-factor helpers. */
    float speed = state->speed;
    const float bobMaxAmp = 45.0f;
    float bobPhase = (float)BG_GetBobCycle(ps);

    /* 0x30015be8..0x30015c00: vertical bob applied to angles[0].
     *   angles[0] -= BG_GetVerticalBobFactor(ps, bobPhase, speed, 45.0f)
     *            * weaponInfo->adsViewBobScale * ps->adsFraction
     * (FMUL [EDI+0x294], FMUL [EDX+0xe0], FSUBR [ESI] -> angles[0] = angles[0] - product.) */
    /* Both helpers return RAW unrounded ST0 (their tails are FMUL/FMULP; RET,
     * no float spill), and this caller multiplies that return directly
     * (0x30015bed FMUL) -- so the locals are long double, no float rounding.
     * (Full fidelity additionally needs the shared decls retyped to long
     * double return; see the float-audit HEADER-CHANGE-NEEDED note.) */
    long double vertBob = BG_GetVerticalBobFactor(ps, bobPhase, speed, bobMaxAmp);
    angles[0] = angles[0] - vertBob * weaponInfo->adsViewBobScale * ps->adsFraction;

    /* 0x30015c02..0x30015c21: horizontal bob applied to angles[1] the same way.
     *   angles[1] -= BG_GetHorizontalBobFactor(ps, bobPhase, speed, 45.0f)
     *            * weaponInfo->adsViewBobScale * ps->adsFraction
     * (The 45.0f maxAmp pushed for the vertical call is reused as this call's
     * clamp arg; the source shape is the same call.) */
    float horizontalBobAmplitude = state->speed;
    long double horizBob = BG_GetHorizontalBobFactor(
        ps, bobPhase, horizontalBobAmplitude, bobMaxAmp);
    angles[1] = angles[1] - horizBob * weaponInfo->adsViewBobScale * ps->adsFraction;
#else
    playerState_t *ps = state->ps;
    const weaponInfo_t *weapon = BG_GetInfoForWeapon(ps->currentWeapon);
    const float adsFraction = ps->adsFraction;
    const float bobScale = weapon->adsViewBobScale;
    const float phase = BG_GetBobCycle(ps);
    float bob;

    if ((ps->entityStateFlags & EF_RESTRICTED_MASK) != 0 &&
        BG_AllowPlayerWeaponAtVehiclePos(
            ps->vehicleType, ps->vehiclePosition) == 0) {
        return;
    }
    if (adsFraction == 0.0f || bobScale == 0.0f) {
        return;
    }

    bob = BG_GetVerticalBobFactor(
        ps, phase, state->speed, 45.0f);
#if EMULATE_X87
    bob = x87f_store_f32(x87f_mul(
        x87f_mul(x87f_load_f32(adsFraction),
                 x87f_load_f32(bobScale)),
        x87f_load_f32(bob)));
    angles[0] = x87f_store_f32(x87f_sub(
        x87f_load_f32(angles[0]), x87f_load_f32(bob)));
#else
    bob = adsFraction * bobScale * bob;
    angles[0] -= bob;
#endif

    bob = BG_GetHorizontalBobFactor(
        ps, phase, state->speed, 45.0f);
#if EMULATE_X87
    bob = x87f_store_f32(x87f_mul(
        x87f_mul(x87f_load_f32(adsFraction),
                 x87f_load_f32(bobScale)),
        x87f_load_f32(bob)));
    angles[1] = x87f_store_f32(x87f_sub(
        x87f_load_f32(angles[1]), x87f_load_f32(bob)));
#else
    bob = adsFraction * bobScale * bob;
    angles[1] -= bob;
#endif
#endif
}
