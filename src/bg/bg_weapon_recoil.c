#include "bg_weapon.h"

#include "compat/coduo_x87emu.h"
#include "compat/crt/random_compat.h"

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * The Windows cgame and game bodies are instruction-identical apart from
 * relocations to their weapon table, rand routine, and constants:
 *
 *   uo_cgame_mp_x86.dll  0x30016120..0x300162e5
 *   uo_game_mp_x86.dll   0x20016060..0x20016225
 *
 * Both receive playerState_t, recoil velocity, and view kick in EAX, EBX, and
 * EDI respectively.  The Linux game body at RVA 0x000396bc uses the same
 * source-level argument order through its ordinary stack ABI.  Its first
 * pointer is named gclient_t in game code, but playerState_t is its zero-offset
 * prefix and is the only portion this function reads.
 *
 * The behavioral split is genuine.  Windows rand has a 15-bit result and its
 * optimized body retains selected max-min/sample expressions in x87 before
 * their final stores.  Linux rand has a 31-bit result and the unoptimized body
 * spills each range delta and random sample to binary32 before later use.
 */

#if defined(WINDOWS_BEHAVIOR)

#if EMULATE_X87
/* NOT_FROM_ORIGINAL_SOURCE: expresses one repeated original Windows x87
 * random-range graph without inserting a binary32 store. */
static x87f bg_compat_weapon_recoil_range(float minimum, float maximum)
{
    return x87f_add(x87f_load_f32(minimum), x87f_mul(x87f_mul(x87f_load_i32(coduo_server_rand()), x87f_load_f32(1.0f / 32768.0f)),
                                                     x87f_sub(x87f_load_f32(maximum), x87f_load_f32(minimum))));
}
#else
/* NOT_FROM_ORIGINAL_SOURCE: native-x87 carrier for the repeated original
 * Windows random-range expression. */
static long double bg_compat_weapon_recoil_range(float minimum, float maximum)
{
    return (long double)minimum +
           (long double)coduo_server_rand() * (long double)(1.0f / 32768.0f) * ((long double)maximum - (long double)minimum);
}
#endif

void BG_WeaponFireRecoil(playerState_t *ps, vec2_t recoilVelocity, vec3_t viewKick)
{
    const weaponInfo_t *weaponInfo = bg_weaponInfos[ps->currentWeapon];
    const float adsFraction = ps->adsFraction;

#if EMULATE_X87
    x87f viewKickYaw;
    float viewKickPitch;

    if (adsFraction == 1.0f) {
        viewKickPitch =
            x87f_store_f32(bg_compat_weapon_recoil_range(weaponInfo->fireViewkickAdsPitchMin, weaponInfo->fireViewkickAdsPitchMax));
        viewKickYaw = bg_compat_weapon_recoil_range(weaponInfo->fireViewkickAdsYawMin, weaponInfo->fireViewkickAdsYawMax);
    } else {
        viewKickPitch =
            x87f_store_f32(bg_compat_weapon_recoil_range(weaponInfo->fireViewkickHipPitchMin, weaponInfo->fireViewkickHipPitchMax));
        viewKickYaw = bg_compat_weapon_recoil_range(weaponInfo->fireViewkickHipYawMin, weaponInfo->fireViewkickHipYawMax);
    }

    viewKick[0] = x87f_store_f32(x87f_neg(x87f_load_f32(viewKickPitch)));
    viewKick[1] = x87f_store_f32(viewKickYaw);
    viewKick[2] = x87f_store_f32(x87f_mul(viewKickYaw, x87f_load_f32(-0.5f)));

    x87f recoilYaw;
    float recoilPitch;
    if (adsFraction > 0.0f) {
        recoilPitch = x87f_store_f32(bg_compat_weapon_recoil_range(weaponInfo->fireRecoilAdsPitchMin, weaponInfo->fireRecoilAdsPitchMax));
        recoilYaw = bg_compat_weapon_recoil_range(weaponInfo->fireRecoilAdsYawMin, weaponInfo->fireRecoilAdsYawMax);
    } else {
        recoilPitch = x87f_store_f32(bg_compat_weapon_recoil_range(weaponInfo->fireRecoilHipPitchMin, weaponInfo->fireRecoilHipPitchMax));
        recoilYaw = bg_compat_weapon_recoil_range(weaponInfo->fireRecoilHipYawMin, weaponInfo->fireRecoilHipYawMax);
    }

    recoilVelocity[0] = x87f_store_f32(x87f_add(x87f_load_f32(recoilPitch), x87f_load_f32(recoilVelocity[0])));
    recoilVelocity[1] = x87f_store_f32(x87f_add(recoilYaw, x87f_load_f32(recoilVelocity[1])));
#else
    long double viewKickYaw;
    float viewKickPitch;

    if (adsFraction == 1.0f) {
        viewKickPitch = (float)bg_compat_weapon_recoil_range(weaponInfo->fireViewkickAdsPitchMin, weaponInfo->fireViewkickAdsPitchMax);
        viewKickYaw = bg_compat_weapon_recoil_range(weaponInfo->fireViewkickAdsYawMin, weaponInfo->fireViewkickAdsYawMax);
    } else {
        viewKickPitch = (float)bg_compat_weapon_recoil_range(weaponInfo->fireViewkickHipPitchMin, weaponInfo->fireViewkickHipPitchMax);
        viewKickYaw = bg_compat_weapon_recoil_range(weaponInfo->fireViewkickHipYawMin, weaponInfo->fireViewkickHipYawMax);
    }

    viewKick[0] = -viewKickPitch;
    viewKick[1] = (float)viewKickYaw;
    viewKick[2] = (float)(viewKickYaw * (long double)-0.5f);

    long double recoilYaw;
    float recoilPitch;
    if (adsFraction > 0.0f) {
        recoilPitch = (float)bg_compat_weapon_recoil_range(weaponInfo->fireRecoilAdsPitchMin, weaponInfo->fireRecoilAdsPitchMax);
        recoilYaw = bg_compat_weapon_recoil_range(weaponInfo->fireRecoilAdsYawMin, weaponInfo->fireRecoilAdsYawMax);
    } else {
        recoilPitch = (float)bg_compat_weapon_recoil_range(weaponInfo->fireRecoilHipPitchMin, weaponInfo->fireRecoilHipPitchMax);
        recoilYaw = bg_compat_weapon_recoil_range(weaponInfo->fireRecoilHipYawMin, weaponInfo->fireRecoilHipYawMax);
    }

    recoilVelocity[0] = (float)((long double)recoilPitch + (long double)recoilVelocity[0]);
    recoilVelocity[1] = (float)(recoilYaw + (long double)recoilVelocity[1]);
#endif
}

#else

/* NOT_FROM_ORIGINAL_SOURCE: expresses the Linux body's repeated random-range
 * block, including both of its original binary32 spill points. */
static float bg_compat_weapon_recoil_range(float minimum, float maximum)
{
#if EMULATE_X87
    const float delta = x87f_store_f32(x87f_sub(x87f_load_f32(maximum), x87f_load_f32(minimum)));
    return x87f_store_f32(
        x87f_add(x87f_load_f32(minimum),
                 x87f_mul(x87f_div(x87f_load_i32(coduo_server_rand()), x87f_load_f32(2147483648.0f)), x87f_load_f32(delta))));
#else
    const float delta = maximum - minimum;
    return (float)((long double)minimum + ((long double)coduo_server_rand() / (long double)2147483648.0f) * (long double)delta);
#endif
}

void BG_WeaponFireRecoil(playerState_t *ps, vec2_t recoilVelocity, vec3_t viewKick)
{
    const weaponInfo_t *weaponInfo = BG_GetInfoForWeapon(ps->currentWeapon);
    const float adsFraction = ps->adsFraction;
    float viewKickPitch;
    float viewKickYaw;

    if (adsFraction == 1.0f) {
        viewKickPitch = bg_compat_weapon_recoil_range(weaponInfo->fireViewkickAdsPitchMin, weaponInfo->fireViewkickAdsPitchMax);
        viewKickYaw = bg_compat_weapon_recoil_range(weaponInfo->fireViewkickAdsYawMin, weaponInfo->fireViewkickAdsYawMax);
    } else {
        viewKickPitch = bg_compat_weapon_recoil_range(weaponInfo->fireViewkickHipPitchMin, weaponInfo->fireViewkickHipPitchMax);
        viewKickYaw = bg_compat_weapon_recoil_range(weaponInfo->fireViewkickHipYawMin, weaponInfo->fireViewkickHipYawMax);
    }

    viewKick[0] = -viewKickPitch;
    viewKick[1] = viewKickYaw;
#if EMULATE_X87
    viewKick[2] = x87f_store_f32(x87f_mul(x87f_load_f32(viewKickYaw), x87f_load_f32(-0.5f)));
#else
    viewKick[2] = viewKickYaw * -0.5f;
#endif

    float recoilPitch;
    float recoilYaw;
    if (adsFraction > 0.0f) {
        recoilPitch = bg_compat_weapon_recoil_range(weaponInfo->fireRecoilAdsPitchMin, weaponInfo->fireRecoilAdsPitchMax);
        recoilYaw = bg_compat_weapon_recoil_range(weaponInfo->fireRecoilAdsYawMin, weaponInfo->fireRecoilAdsYawMax);
    } else {
        recoilPitch = bg_compat_weapon_recoil_range(weaponInfo->fireRecoilHipPitchMin, weaponInfo->fireRecoilHipPitchMax);
        recoilYaw = bg_compat_weapon_recoil_range(weaponInfo->fireRecoilHipYawMin, weaponInfo->fireRecoilHipYawMax);
    }

#if EMULATE_X87
    recoilVelocity[0] = x87f_store_f32(x87f_add(x87f_load_f32(recoilVelocity[0]), x87f_load_f32(recoilPitch)));
    recoilVelocity[1] = x87f_store_f32(x87f_add(x87f_load_f32(recoilVelocity[1]), x87f_load_f32(recoilYaw)));
#else
    recoilVelocity[0] += recoilPitch;
    recoilVelocity[1] += recoilYaw;
#endif
}

#endif
