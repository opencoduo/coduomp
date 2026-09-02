#include "bg_pmove.h"

#include "bg_player_state.h"
#include "bg_vehicle.h"
#include "bg_weapon.h"
#include "qcommon/q_bits.h"

#include <stdint.h>

#define PM_CHANGE_ADS_EPSILON 9.9999997e-05f
#define PM_AIM_SPREAD_SCALE_ALT_SWITCH_MIN 128.0f
/*
 * Complete weapon-change and ADS transition subsystem shared by the cgame and
 * game modules.  The retained Windows bodies agree instruction for instruction
 * apart from relocated globals and calls:
 *
 *   function                              cgame       game
 *   PM_BeginWeaponDeploy                  0x300129a0  0x200128e0
 *   PM_BeginWeaponBreakingdown            0x30012ab0  0x200129f0
 *   PM_BeginWeaponChange                  0x30012bc0  0x20012b00
 *   PM_Weapon_FinishWeaponChange          0x30012e70  0x20012db0
 *   PM_Weapon_FinishWeaponRaise           0x300131b0  0x200130f0
 *   PM_Weapon_FinishWeaponDeploy          0x30013200  0x20013140
 *   PM_Weapon_FinishWeaponBreakdown       0x30013250  0x20013190
 *   PM_Weapon_CheckForDeployBreakdown     0x300138c0  0x20013800
 *   PM_Weapon_CheckForChangeWeapon        0x30013d30  0x20013c70
 *
 * The Linux game module implements the same decisions at RVAs 0x000347aa,
 * 0x0003484c, 0x000348ee, 0x00034c14, 0x00035064, 0x000350c2,
 * 0x00035120, 0x00035b66, and 0x000362ea.  Its provisional
 * PM_BeginAimDownSight/PM_EndAimDownSight spellings are normalized to the
 * canonical names exported by both Mac modules.  All floating-point operands
 * here are values already stored as binary32; no expression retains a live x87
 * intermediate, so this subsystem has no platform or EMULATE_X87 variant.
 */

void PM_BeginWeaponDeploy(void)
{
    playerState_t *ps = pm->ps;

    if (ps->weaponState != WEAPON_STATE_DEPLOYING) {
        ps->weaponState = WEAPON_STATE_DEPLOYING;
        ps->weaponTime = pml.weaponInfo->adsInTime;
        PM_StartWeaponAnim(PM_WEAPON_ANIM_ADS_IN);
        PM_AddEvent(EV_DEPLOY_WEAPON);
        BG_AnimScriptEvent(ps, ANIM_EVENT_LMG_DEPLOY, qfalse, qtrue);
    }
}

void PM_BeginWeaponBreakingdown(void)
{
    playerState_t *ps = pm->ps;

    if (ps->weaponState != WEAPON_STATE_BREAKING_DOWN) {
        ps->weaponState = WEAPON_STATE_BREAKING_DOWN;
        ps->weaponTime = pml.weaponInfo->adsOutTime;
        PM_StartWeaponAnim(PM_WEAPON_ANIM_ADS_OUT);
        PM_AddEvent(EV_BREAKDOWN_WEAPON);
        BG_AnimScriptEvent(ps, ANIM_EVENT_LMG_BREAKDOWN, qfalse, qtrue);
    }
}

void PM_BeginWeaponChange(int32_t currentWeapon, int32_t nextWeapon)
{
    playerState_t *ps = pm->ps;
    const weaponInfo_t *currentWeaponInfo;
    qboolean isAltSwitch;
    qboolean canPlayLowerAnim;
    int32_t altSlot;

    if (nextWeapon < 0 || nextWeapon > BG_GetNumWeapons()) {
        return;
    }
    if (nextWeapon != 0 && Com_BitCheck(ps->weaponBits, nextWeapon) == qfalse) {
        return;
    }
    if (ps->weaponState == WEAPON_STATE_DROPPING) {
        return;
    }
    if ((pml.weaponInfo->weaponClass == WEAPCLASS_LMG || pml.weaponInfo->weaponClass == WEAPCLASS_SPOTTER) &&
        ((ps->playerStateFlags & PMF_ADS) != 0 || ps->adsFraction > PM_CHANGE_ADS_EPSILON)) {
        return;
    }

    ps->weaponDelay = 0;

    if (currentWeapon == 0 || Com_BitCheck(ps->weaponBits, currentWeapon) == qfalse || ps->grenadeTimeLeft > 0) {
        ps->weaponTime = 0;
        ps->weaponState = WEAPON_STATE_DROPPING;
        ps->grenadeTimeLeft = 0;
        PM_SetProneMovementOverride();
        BG_AddPredictableEventToPlayerstate(EV_REMOVE_WEAPON_ATTACHMENTS, currentWeapon, ps);
        return;
    }

    currentWeaponInfo = BG_GetInfoForWeapon(currentWeapon);
    isAltSwitch = nextWeapon != 0 && nextWeapon == currentWeaponInfo->altWeapon;
    canPlayLowerAnim = qtrue;
    if (currentWeaponInfo->clipRequired != 0 && ps->clips[currentWeaponInfo->clipIndex] == 0) {
        canPlayLowerAnim = qfalse;
    }

    ps->grenadeTimeLeft = 0;
    if (isAltSwitch != qfalse) {
        PM_AddEvent(EV_WEAPON_ALT);
        PM_StartWeaponAnim(PM_WEAPON_ANIM_ALT_SWITCH_LOWER);
    } else if (canPlayLowerAnim != qfalse) {
        PM_AddEvent(EV_PUTAWAY_WEAPON);
        PM_StartWeaponAnim(PM_WEAPON_ANIM_LOWER);
    }

    if (isAltSwitch == qfalse) {
        BG_AnimScriptEvent(ps, ANIM_EVENT_DROP_WEAPON, qfalse, qfalse);
    }

    ps->weaponState = WEAPON_STATE_DROPPING;
    PM_SetProneMovementOverride();

    if (isAltSwitch != qfalse) {
        ps->weaponTime = currentWeaponInfo->altSwitchLowerTime;
        altSlot = BG_IsPlayerWeaponInSlot(ps, currentWeapon, qtrue);
        if (altSlot != WEAPSLOT_NONE) {
            BG_SetPlayerWeaponForSlot(ps, altSlot, nextWeapon);
        }
    } else {
        ps->weaponTime = currentWeaponInfo->lowerTime;
    }
}

qboolean PM_Weapon_FinishWeaponChange(void)
{
    playerState_t *ps = pm->ps;
    int32_t requestedWeapon;
    int32_t oldWeapon;
    const weaponInfo_t *newWeaponInfo;
    qboolean isAltSwitch;

    if (ps->weaponState != WEAPON_STATE_DROPPING) {
        return qfalse;
    }

    requestedWeapon = 0;
    if ((ps->playerStateFlags & PMF_LADDER) == 0 && Com_BitCheck(ps->weaponBits, pm->command.weapon) != qfalse) {
        if ((ps->playerStateFlags & PMF_WEAPON_DISABLED) == 0 && Com_BitCheck(ps->weaponBits, pm->command.weapon) != qfalse &&
            ((ps->entityStateFlags & EF_IN_VEHICLE) == 0 || (ps->entityStateFlags & EF_VEHICLE_ALLOW_WEAPON) != 0 ||
             BG_AllowPlayerWeaponAtVehiclePos(ps->vehicleType, ps->vehiclePosition) != qfalse)) {
            requestedWeapon = pm->command.weapon;
            if (requestedWeapon > BG_GetNumWeapons()) {
                requestedWeapon = 0;
            }
        }
    }
    if (Com_BitCheck(ps->weaponBits, requestedWeapon) == qfalse) {
        requestedWeapon = 0;
    }

    oldWeapon = ps->currentWeapon;
    ps->currentWeapon = requestedWeapon;
    pml.weaponInfo = BG_GetInfoForWeapon(ps->currentWeapon);

    if (oldWeapon == requestedWeapon) {
        ps->weaponState = WEAPON_STATE_IDLE;
        PM_StartWeaponAnim(PM_WEAPON_ANIM_IDLE);
        return qtrue;
    }

    ps->weaponState = WEAPON_STATE_RAISING;
    if (oldWeapon == 0) {
        ps->weaponTime = BG_GetInfoForWeapon(requestedWeapon)->switchRaiseTime;
        ps->aimSpreadScale = PM_AIM_SPREAD_SCALE_MAX;
        PM_StartWeaponAnim(PM_WEAPON_ANIM_SWITCH_RAISE);
        PM_SetProneMovementOverride();
        return qtrue;
    }

    PM_SetProneMovementOverride();
    newWeaponInfo = BG_GetInfoForWeapon(requestedWeapon);
    if (newWeaponInfo->weaponClass == WEAPCLASS_LMG) {
        ps->adsFraction = 0.0f;
    }

    isAltSwitch = requestedWeapon != 0 && requestedWeapon == BG_GetInfoForWeapon(oldWeapon)->altWeapon;
    if (isAltSwitch != qfalse) {
        ps->weaponTime = newWeaponInfo->altSwitchRaiseTime;
    } else {
        PM_AddEvent(EV_RAISE_WEAPON);
        ps->weaponTime = newWeaponInfo->switchRaiseTime;
    }

    BG_UpdateConditionValue(ps->psClientNum, ANIM_COND_WEAPON, requestedWeapon, qtrue);
    BG_UpdateConditionValue(ps->psClientNum, ANIM_COND_WEAPONCLASS, newWeaponInfo->weaponClass, qtrue);

    if (isAltSwitch == qfalse) {
        BG_AnimScriptEvent(ps, ANIM_EVENT_RAISE_WEAPON, qfalse, qfalse);
        ps->aimSpreadScale = PM_AIM_SPREAD_SCALE_MAX;
        PM_StartWeaponAnim(PM_WEAPON_ANIM_SWITCH_RAISE);
    } else {
        if (ps->aimSpreadScale < PM_AIM_SPREAD_SCALE_ALT_SWITCH_MIN) {
            ps->aimSpreadScale = PM_AIM_SPREAD_SCALE_ALT_SWITCH_MIN;
        }
        PM_StartWeaponAnim(PM_WEAPON_ANIM_ALT_SWITCH_RAISE);
    }

    return qtrue;
}

qboolean PM_Weapon_FinishWeaponRaise(void)
{
    playerState_t *ps = pm->ps;
    qboolean handled = ps->weaponState == WEAPON_STATE_RAISING;

    if (handled != qfalse) {
        ps->weaponState = WEAPON_STATE_IDLE;
        PM_StartWeaponAnim(PM_WEAPON_ANIM_IDLE);
    }
    return handled;
}

qboolean PM_Weapon_FinishWeaponDeploy(void)
{
    playerState_t *ps = pm->ps;
    qboolean handled = ps->weaponState == WEAPON_STATE_DEPLOYING;

    if (handled != qfalse) {
        ps->weaponState = WEAPON_STATE_IDLE;
        PM_StartWeaponAnim(PM_WEAPON_ANIM_IDLE);
    }
    return handled;
}

qboolean PM_Weapon_FinishWeaponBreakdown(void)
{
    playerState_t *ps = pm->ps;
    qboolean handled = ps->weaponState == WEAPON_STATE_BREAKING_DOWN;

    if (handled != qfalse) {
        ps->weaponState = WEAPON_STATE_IDLE;
        PM_StartWeaponAnim(PM_WEAPON_ANIM_IDLE);
    }
    return handled;
}

void PM_Weapon_CheckForDeployBreakdown(void)
{
    playerState_t *ps = pm->ps;
    int32_t weaponClass = pml.weaponInfo->weaponClass;

    if ((weaponClass != WEAPCLASS_LMG && weaponClass != WEAPCLASS_SPOTTER) || pm->adsInputBlocked != 0) {
        return;
    }

    switch (ps->weaponState) {
    case WEAPON_STATE_RAISING:
    case WEAPON_STATE_DROPPING:
    case WEAPON_STATE_MELEE_WINDUP:
    case WEAPON_STATE_MELEE_RELAX:
        return;

    case WEAPON_STATE_RELOADING:
    case WEAPON_STATE_RELOADING_INTERRUPT:
    case WEAPON_STATE_RELOAD_START:
    case WEAPON_STATE_RELOAD_START_INTERRUPT:
    case WEAPON_STATE_RELOAD_END:
        if ((ps->playerStateFlags & PMF_ADS) != 0) {
            return;
        }
        break;

    case WEAPON_STATE_BREAKING_DOWN:
        if (pm->weaponAnimscriptEnabled != 0) {
            BG_AnimScriptEvent(ps, ANIM_EVENT_LMG_BREAKDOWN, qtrue, qtrue);
        }
        break;

    default:
        break;
    }

    if ((ps->playerStateFlags & PMF_ADS) == 0 || ps->adsFraction == 1.0f || ps->weaponState == WEAPON_STATE_DEPLOYING) {
        if ((ps->playerStateFlags & PMF_ADS) == 0 && ps->adsFraction != 0.0f && ps->weaponState != WEAPON_STATE_BREAKING_DOWN) {
            PM_BeginWeaponBreakingdown();
        }
    } else {
        PM_BeginWeaponDeploy();
    }
}

void PM_Weapon_CheckForChangeWeapon(void)
{
    playerState_t *ps = pm->ps;
    int32_t weaponState = ps->weaponState;
    int32_t weaponTime = ps->weaponTime;
    int32_t weaponDelay = ps->weaponDelay;
    int32_t specialTime = ps->grenadeTimeLeft;
    int32_t currentWeapon = ps->currentWeapon;
    int32_t requestedWeapon = pm->command.weapon;

    if (pml.weaponInfo->weaponType == WEAPTYPE_GRENADE && pml.weaponInfo->specialTimeEnabled != 0 &&
        specialTime < pml.weaponInfo->specialTimeThreshold && specialTime != 0 && Com_BitCheck(ps->weaponBits, currentWeapon) != qfalse) {
        return;
    }

    if (weaponTime != 0 && weaponState != WEAPON_STATE_RELOADING && weaponState != WEAPON_STATE_RELOAD_START &&
        weaponState != WEAPON_STATE_RELOAD_END && weaponState != WEAPON_STATE_RELOAD_START_INTERRUPT &&
        weaponState != WEAPON_STATE_RELOADING_INTERRUPT && weaponState != WEAPON_STATE_RECHAMBERING &&
        (weaponState == WEAPON_STATE_FIRING || weaponState == WEAPON_STATE_MELEE_WINDUP || weaponState == WEAPON_STATE_MELEE_RELAX ||
         weaponDelay != 0)) {
        return;
    }

    if ((ps->playerStateFlags & PMF_LADDER) != 0) {
        if (currentWeapon != 0) {
            PM_BeginWeaponChange(currentWeapon, 0);
        }
        return;
    }

    if ((ps->playerStateFlags & PMF_WEAPON_DISABLED) != 0 ||
        (((ps->entityStateFlags & EF_IN_VEHICLE) != 0 && (ps->entityStateFlags & EF_VEHICLE_ALLOW_WEAPON) == 0) &&
         BG_AllowPlayerWeaponAtVehiclePos(ps->vehicleType, ps->vehiclePosition) == qfalse)) {
        if (currentWeapon != 0) {
            PM_BeginWeaponChange(currentWeapon, 0);
        }
        return;
    }

    if (currentWeapon == requestedWeapon || ((ps->playerStateFlags & PMF_FOLLOW) != 0 && currentWeapon != 0) ||
        (requestedWeapon != 0 && Com_BitCheck(ps->weaponBits, requestedWeapon) == qfalse)) {
        if (currentWeapon != 0 && Com_BitCheck(ps->weaponBits, currentWeapon) == qfalse) {
            PM_BeginWeaponChange(currentWeapon, 0);
        }
        return;
    }

    PM_BeginWeaponChange(currentWeapon, requestedWeapon);
}
