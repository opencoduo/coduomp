#include "bg_pmove.h"

#include "bg_vehicle.h"
#include "bg_weapon.h"
#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"
#include "qcommon/q_bits.h"

#include <stdint.h>

#define PM_WEAPON_NO_AMMO_TIME_PENALTY 500

/*
 * Complete firing and melee state-machine cluster shared by cgame and game.
 * The retained Windows cgame/game bodies are instruction-identical apart from
 * relocated globals and calls:
 *
 *   function                            cgame       game
 *   PM_Weapon_WeaponTimeAdjust          0x30013c50  0x20013b90
 *   PM_Weapon_FinishFiring              0x30013ea0  0x20013de0
 *   PM_Weapon_StartFiring               0x30013f20  0x20013e60
 *   PM_Weapon_GetAmmoRequired           0x300140d0  0x20014010
 *   PM_Weapon_CheckFiringAmmo           0x300140e0  0x20014020
 *   PM_Weapon_SetFPSFireAnim            0x300141b0  0x200140f0
 *   PM_Weapon_AddFiringAimSpreadScale   0x30014240  0x20014180
 *   PM_Weapon_FireWeapon                0x300142a0  0x200141e0
 *   PM_Weapon_FireMelee                 0x30014450  0x20014390
 *   PM_Weapon_FinishMelee               0x300144c0  0x20014400
 *   PM_Weapon_CheckForMelee             0x30014520  0x20014460
 *   PM_SetProneMovementOverride         0x3000d780  0x2000d530
 *
 * Linux game retains the same state transitions at RVAs 0x000360fa,
 * 0x000366b2, 0x0003679b, 0x00036997, 0x000369a1, 0x00036acc,
 * 0x00036b6a, 0x00036c0e, 0x00036e91, 0x00036f38, 0x00036fb7,
 * and 0x0002c5dd.  Mac symbols supply the canonical names that correct four
 * swapped or provisional cgame reconstruction names.
 *
 * Windows inlines the guard from PM_ContinueWeaponAnim(0) at three sites and
 * then calls PM_StartWeaponAnim(0); Linux and Mac retain the wrapper call.
 * The predicates are exact, so this common source uses the original
 * PM_ContinueWeaponAnim spelling without a platform variant.
 */

qboolean PM_Weapon_WeaponTimeAdjust(void)
{
    playerState_t *ps = pm->ps;

    if (ps->weaponTime != 0) {
        ps->weaponTime -= pml.msec;
        if (ps->weaponTime < 1) {
            if (pml.weaponInfo->weaponTimeHold != 0 && (pm->command.buttons & PM_BUTTON_FIRE) != 0 &&
                ps->currentWeapon == pm->command.weapon && PM_WeaponAmmoAvailable(ps->currentWeapon) != 0) {
                ps->weaponTime = 1;
                if (ps->weaponState == WEAPON_STATE_RECHAMBERING) {
                    PM_Weapon_FinishRechamber();
                } else if (ps->weaponState == WEAPON_STATE_FIRING || ps->weaponState == WEAPON_STATE_MELEE_WINDUP ||
                           ps->weaponState == WEAPON_STATE_MELEE_RELAX) {
                    PM_ContinueWeaponAnim(0);
                    ps->weaponState = WEAPON_STATE_IDLE;
                }
            } else {
                ps->weaponTime = 0;
            }
        }
    }

    if (ps->weaponDelay == 0) {
        return qfalse;
    }

    ps->weaponDelay -= pml.msec;
    if (ps->weaponDelay > 0) {
        return qfalse;
    }

    ps->weaponDelay = 0;
    return qtrue;
}

qboolean PM_Weapon_FinishFiring(qboolean delayExpired)
{
    playerState_t *ps = pm->ps;
    const qboolean primaryHeld = (pm->command.buttons & PM_BUTTON_FIRE) != 0 ? qtrue : qfalse;

    if (primaryHeld == qfalse && pml.weaponInfo->weaponType == WEAPTYPE_GRENADE && pml.weaponInfo->specialTimeEnabled != 0 &&
        ps->grenadeTimeLeft < pml.weaponInfo->specialTimeThreshold && ps->grenadeTimeLeft != 0) {
        return qfalse;
    }

    if (primaryHeld == qfalse && delayExpired == qfalse) {
        if (ps->weaponState == WEAPON_STATE_FIRING) {
            PM_ContinueWeaponAnim(0);
        }
        ps->weaponState = WEAPON_STATE_IDLE;
        return qtrue;
    }

    return qfalse;
}

void PM_Weapon_StartFiring(qboolean delayExpired)
{
    playerState_t *ps = pm->ps;

    if (pml.weaponInfo->weaponType == WEAPTYPE_GRENADE) {
        if (delayExpired == qfalse && ps->grenadeTimeLeft == 0) {
            if (PM_WeaponAmmoAvailable(ps->currentWeapon) != 0) {
                ps->grenadeTimeLeft = pml.weaponInfo->specialTimeThreshold;
                PM_StartWeaponAnim(PM_WEAPON_ANIM_SPECIAL_FIRE);
                PM_AddEvent(EV_PULLBACK_WEAPON);
            }

            ps->weaponDelay = pml.weaponInfo->specialFireDelay;
            ps->weaponTime = 0;
        }
    } else {
        ps->weaponDelay = pml.weaponInfo->fireDelay;
        ps->weaponTime = pml.weaponInfo->fireTime;

        if (pml.weaponInfo->adsFireDelayEnabled != 0) {
            const float one = 1.0f;
            const float rate = pml.weaponInfo->adsFireDelayRate;
            const float adsFraction = ps->adsFraction;

            /* The product remains live through the original truncating FISTP. */
#if EMULATE_X87
            ps->weaponDelay = x87f_store_i32_trunc(
                x87f_mul(x87f_div(x87f_load_f32(one), x87f_load_f32(rate)), x87f_sub(x87f_load_f32(one), x87f_load_f32(adsFraction))));
#elif defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
            ps->weaponDelay =
                CODUO_X87_TRUNCATE_I32(((long double)one / (long double)rate) * ((long double)one - (long double)adsFraction));
#else
            ps->weaponDelay = (int32_t)((one / rate) * (one - adsFraction));
#endif
        }

        BG_AnimScriptEvent(ps, ANIM_EVENT_FIRE_WEAPON, qfalse, qtrue);
        if (pml.weaponInfo->raiseEnabled != 0) {
            Com_BitSet(ps->weaponRechamberBits, ps->currentWeapon);
        }
    }

    ps->weaponState = WEAPON_STATE_FIRING;
    PM_SetProneMovementOverride();
}

int32_t PM_Weapon_GetAmmoRequired(int32_t weapon)
{
    (void)weapon;
    return 1;
}

qboolean PM_Weapon_CheckFiringAmmo(void)
{
    playerState_t *ps = pm->ps;
    const int32_t weapon = ps->currentWeapon;
    const int32_t ammoRequired = PM_Weapon_GetAmmoRequired(weapon);
    const weaponInfo_t *weaponInfo = BG_GetInfoForWeapon(weapon);
    qboolean reserveHasAmmo;

    if (ps->clips[weaponInfo->clipIndex] >= ammoRequired) {
        return qtrue;
    }

    reserveHasAmmo = ps->ammo[weaponInfo->ammoIndex] >= ammoRequired ? qtrue : qfalse;

    if (pml.weaponInfo->weaponType != WEAPTYPE_GRENADE && reserveHasAmmo == qfalse && pml.weaponInfo->weaponClass != WEAPCLASS_SPOTTER) {
        PM_AddEvent(EV_NOAMMO);
    }

    if (reserveHasAmmo != qfalse) {
        PM_BeginWeaponReload();
    } else {
        PM_ContinueWeaponAnim(0);
        ps->weaponTime += PM_WEAPON_NO_AMMO_TIME_PENALTY;
    }

    return qfalse;
}

void PM_Weapon_SetFPSFireAnim(void)
{
    playerState_t *ps = pm->ps;
    const qboolean clipEmpty = PM_WeaponClipEmpty(ps->currentWeapon);
    pmWeaponAnim_t animation;

    if (ps->adsFraction > PM_WEAPON_ADS_RAISE_THRESHOLD) {
        animation = clipEmpty != qfalse ? PM_WEAPON_ANIM_ADS_FIRE_LASTSHOT : PM_WEAPON_ANIM_ADS_FIRE;
    } else {
        animation = clipEmpty != qfalse ? PM_WEAPON_ANIM_FIRE_LASTSHOT : PM_WEAPON_ANIM_FIRE;
    }

    PM_StartWeaponAnim(animation);
}

void PM_Weapon_AddFiringAimSpreadScale(void)
{
    playerState_t *ps = pm->ps;

    if (ps->adsFraction == 1.0f) {
        return;
    }

    /* Both original x87 bodies retain multiply/add through the binary32 store. */
#if EMULATE_X87
    ps->aimSpreadScale =
        x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(pml.weaponInfo->fireAimSpreadScale), x87f_load_f32(PM_AIM_SPREAD_SCALE_MAX)),
                                x87f_load_f32(ps->aimSpreadScale)));
#else
    ps->aimSpreadScale =
        (float)((long double)pml.weaponInfo->fireAimSpreadScale * (long double)PM_AIM_SPREAD_SCALE_MAX + (long double)ps->aimSpreadScale);
#endif
    if (ps->aimSpreadScale > PM_AIM_SPREAD_SCALE_MAX) {
        ps->aimSpreadScale = PM_AIM_SPREAD_SCALE_MAX;
    }
}

void PM_Weapon_FireWeapon(qboolean delayExpired)
{
    playerState_t *ps = pm->ps;
    const int32_t weapon = ps->currentWeapon;

    if ((ps->playerStateFlags & PMF_SPRINTING) != 0) {
        return;
    }
    if (pml.weaponInfo->weaponClass == WEAPCLASS_SPOTTER &&
        ((ps->playerStateFlags & PMF_ADS) == 0 || pml.weaponInfo->adsFireDelayEnabled == 0)) {
        return;
    }
    if (pml.weaponInfo->weaponClass == WEAPCLASS_LMG &&
        ((ps->playerStateFlags & PMF_ADS) == 0 || ps->adsFraction < PM_WEAPON_LMG_ADS_FRACTION_MIN)) {
        return;
    }

    PM_Weapon_StartFiring(delayExpired);
    if (PM_Weapon_CheckFiringAmmo() == qfalse || ps->weaponDelay != 0) {
        return;
    }

    if (PM_WeaponAmmoAvailable(weapon) != -1 && ((ps->entityStateFlags & EF_RESTRICTED_MASK) == 0 ||
                                                 BG_AllowPlayerWeaponAtVehiclePos(ps->vehicleType, ps->vehiclePosition) != qfalse)) {
        PM_WeaponUseAmmo(weapon, PM_Weapon_GetAmmoRequired(weapon));
        if (pml.weaponInfo->weaponType == WEAPTYPE_GAS) {
            PM_WeaponUseAmmo(weapon, PM_Weapon_GetAmmoRequired(weapon));
        }
    }

    if (pml.weaponInfo->weaponType == WEAPTYPE_GRENADE || pml.weaponInfo->weaponType == WEAPTYPE_GAS) {
        ps->weaponTime = pml.weaponInfo->fireTime;
    }

    PM_Weapon_SetFPSFireAnim();
    PM_AddEvent(PM_WeaponClipEmpty(weapon) != qfalse ? EV_FIRE_WEAPON_LASTSHOT : EV_FIRE_WEAPON);
    PM_Weapon_AddFiringAimSpreadScale();
    PM_RemoveEmptyClipOnlyWeapon();
}

void PM_Weapon_FireMelee(void)
{
    playerState_t *ps = pm->ps;
    const int32_t minimumTime = pml.weaponInfo->meleeTime - pml.weaponInfo->meleeWindup;

    if (ps->weaponTime < minimumTime) {
        ps->weaponTime = minimumTime;
    }

    PM_AddEvent(EV_FIRE_MELEE);
    ps->weaponState = WEAPON_STATE_MELEE_RELAX;
    PM_SetProneMovementOverride();
}

qboolean PM_Weapon_FinishMelee(void)
{
    playerState_t *ps = pm->ps;

    if (ps->weaponState == WEAPON_STATE_MELEE_WINDUP) {
        PM_Weapon_FireMelee();
        return qtrue;
    }
    if (ps->weaponState == WEAPON_STATE_MELEE_RELAX) {
        PM_ContinueWeaponAnim(0);
        ps->weaponState = WEAPON_STATE_IDLE;
        return qtrue;
    }
    return qfalse;
}

void PM_Weapon_CheckForMelee(qboolean delayExpired)
{
    playerState_t *ps = pm->ps;
    const int32_t weaponState = ps->weaponState;

    if (pml.weaponInfo->meleeDamage == 0) {
        return;
    }
    if ((ps->playerStateFlags & PMF_ADS) != 0 && BG_GetInfoForWeapon(ps->currentWeapon)->weaponClass == WEAPCLASS_LMG) {
        return;
    }
    if (weaponState == WEAPON_STATE_BREAKING_DOWN) {
        return;
    }
    if (pml.weaponInfo->weaponType == WEAPTYPE_GRENADE &&
        ((pm->command.buttons & PM_BUTTON_FIRE) != 0 || weaponState == WEAPON_STATE_FIRING)) {
        return;
    }
    if (delayExpired != qfalse) {
        return;
    }
    if (ps->weaponDelay != 0 && weaponState != WEAPON_STATE_RELOADING && weaponState != WEAPON_STATE_RELOADING_INTERRUPT &&
        weaponState != WEAPON_STATE_RELOAD_START && weaponState != WEAPON_STATE_RELOAD_START_INTERRUPT &&
        weaponState != WEAPON_STATE_RELOAD_END) {
        return;
    }

    if ((pm->command.buttons & PM_BUTTON_MELEE) == 0) {
        ps->playerStateFlags &= ~PMF_MELEE_HELD;
        return;
    }
    if ((ps->playerStateFlags & PMF_MELEE_HELD) != 0 || (ps->playerStateFlags & PMF_SPRINTING) != 0) {
        return;
    }

    ps->playerStateFlags |= PMF_MELEE_HELD;
    if (weaponState >= WEAPON_STATE_RAISING &&
        (weaponState <= WEAPON_STATE_DROPPING || weaponState == WEAPON_STATE_MELEE_WINDUP || weaponState == WEAPON_STATE_MELEE_RELAX)) {
        return;
    }

    BG_AnimScriptEvent(ps, ANIM_EVENT_MELEE_ATTACK, qfalse, qtrue);
    PM_StartWeaponAnim(PM_WEAPON_ANIM_MELEE);
    PM_AddEvent(EV_MELEE_SWIPE);

    if (pml.weaponInfo->meleeWindup == 0) {
        PM_Weapon_FireMelee();
        return;
    }

    ps->weaponTime = pml.weaponInfo->meleeTime;
    ps->weaponDelay = pml.weaponInfo->meleeWindup;
    ps->weaponState = WEAPON_STATE_MELEE_WINDUP;
    PM_SetProneMovementOverride();
}

void PM_SetProneMovementOverride(void)
{
    if ((pm->ps->playerStateFlags & PMF_PRONE) != 0) {
        pm->ps->playerStateFlags |= PMF_PRONE_MOVEMENT_OVERRIDE;
    }
}
