#include "bg_pmove.h"

#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"
#include "qcommon/q_bits.h"

#include <stdint.h>

/*
 * Complete weapon-reload subsystem shared by cgame and game.  Every Windows
 * cgame body has an instruction-identical Windows game body apart from global,
 * branch, and call addresses:
 *
 *                                      cgame       game
 *   PM_ReloadClip                      0x30012290  0x200121d0
 *   PM_SetWeaponReloadAddAmmoDelay     0x30012660  0x200125a0
 *   PM_SetReloadingState               0x30012740  0x20012680
 *   PM_BeginWeaponReload               0x30012860  0x200127a0
 *   PM_Weapon_AllowReload              0x300132a0  0x200131e0
 *   PM_Weapon_ReloadDelayedAction      0x30013320  0x20013260
 *   PM_Weapon_FinishReload             0x30013470  0x200133b0
 *   PM_Weapon_CheckForReload           0x300136d0  0x20013610
 *   PM_RemoveEmptyClipOnlyWeapon       0x30013a00  0x20013940
 *
 * The Linux game bodies at RVAs 0x00033c78, 0x00034320, 0x0003454b,
 * 0x00034659, 0x0003517e, 0x000352d2, 0x00035564, 0x000358b2, and
 * 0x00035ce8 retain the same gates, state transitions, integer operations,
 * events, and return values.  Mac cgame/game traceback symbols independently
 * supply the canonical names used here.  The Linux reconstruction's
 * PM_FillCurrentWeaponClip, PM_SetReloadAddDelay, and PM_BeginReloadLoop names
 * were provisional aliases for PM_ReloadClip, PM_SetWeaponReloadAddAmmoDelay,
 * and PM_SetReloadingState.
 *
 * The only floating-point operation in this cluster is comparison of the
 * stored binary32 ADS fraction with binary32 0.99f.  An unordered value passes
 * that LMG gate in every authoritative body, so the comparison below remains a
 * direct ordered less-than test.  No platform behavior or x87 emulation split
 * is required.
 */

void PM_ReloadClip(void)
{
    playerState_t *const ps = pm->ps;
    const int32_t weaponState = ps->weaponState;
    const qboolean reloadStartState = weaponState == WEAPON_STATE_RELOAD_START || weaponState == WEAPON_STATE_RELOAD_START_INTERRUPT;
    const weaponInfo_t *const weaponInfo = BG_GetInfoForWeapon(ps->currentWeapon);
    const int32_t ammoIndex = weaponInfo->ammoIndex;
    const int32_t clipIndex = weaponInfo->clipIndex;
    const int32_t clipSize = BG_GetAmmoClipSize(clipIndex);
    int32_t amount;

    if (reloadStartState != qfalse && pml.weaponInfo->reloadStartAmmoAdd == 0) {
        return;
    }

    amount = coduo_int32_from_bits((uint32_t)clipSize - (uint32_t)ps->clips[clipIndex]);
    if (ps->ammo[ammoIndex] < amount) {
        amount = ps->ammo[ammoIndex];
    }

    if (reloadStartState != qfalse) {
        const int32_t reloadStartAmmoAdd = pml.weaponInfo->reloadStartAmmoAdd;

        if (reloadStartAmmoAdd < clipSize && reloadStartAmmoAdd < amount) {
            amount = reloadStartAmmoAdd;
        }
    } else {
        const int32_t reloadAmmoAdd = pml.weaponInfo->reloadAmmoAdd;

        if (reloadAmmoAdd != 0 && reloadAmmoAdd < clipSize && reloadAmmoAdd < amount) {
            amount = reloadAmmoAdd;
        }
    }

    if (amount != 0) {
        ps->ammo[ammoIndex] = coduo_int32_from_bits((uint32_t)ps->ammo[ammoIndex] - (uint32_t)amount);
        ps->clips[clipIndex] = coduo_int32_from_bits((uint32_t)ps->clips[clipIndex] + (uint32_t)amount);
    }
}

void PM_SetWeaponReloadAddAmmoDelay(void)
{
    playerState_t *const ps = pm->ps;
    const weaponInfo_t *const weaponInfo = pml.weaponInfo;
    const int32_t weaponState = ps->weaponState;
    int32_t delay;

    if (weaponState == WEAPON_STATE_RELOAD_START || weaponState == WEAPON_STATE_RELOAD_START_INTERRUPT) {
        const int32_t reloadStartAddTime = weaponInfo->reloadStartAddTime;

        if (reloadStartAddTime == 0) {
            delay = 0;
        } else if (reloadStartAddTime < weaponInfo->reloadStartTime) {
            delay = reloadStartAddTime;
        } else {
            delay = weaponInfo->reloadStartTime;
        }
    } else {
        const int32_t clipIndex = BG_GetInfoForWeapon(ps->currentWeapon)->clipIndex;

        if (ps->clips[clipIndex] == 0 && weaponInfo->weaponType == WEAPTYPE_BULLET) {
            delay = weaponInfo->reloadEmptyTime;
        } else {
            delay = weaponInfo->reloadTime;
        }

        if (weaponInfo->reloadAddTime != 0 && weaponInfo->reloadAddTime < delay) {
            delay = weaponInfo->reloadAddTime;
        }
    }

    if (weaponInfo->raiseEnabled != 0 && Com_BitCheck(ps->weaponRechamberBits, ps->currentWeapon) != 0) {
        if (delay == 0) {
            delay = ps->weaponTime;
        }
        if (weaponInfo->raiseInterruptTime < delay) {
            delay = weaponInfo->raiseInterruptTime;
        }
        if (delay == 0) {
            delay = 1;
        }
        ps->weaponDelay = delay;
        return;
    }

    if (delay != 0) {
        ps->weaponDelay = delay;
    }
}

void PM_SetReloadingState(void)
{
    playerState_t *const ps = pm->ps;
    const int32_t clipIndex = BG_GetInfoForWeapon(ps->currentWeapon)->clipIndex;

    if (ps->clips[clipIndex] == 0 && pml.weaponInfo->weaponType == WEAPTYPE_BULLET) {
        PM_StartWeaponAnim(PM_WEAPON_ANIM_RELOAD_EMPTY);
        ps->weaponTime = pml.weaponInfo->reloadEmptyTime;
        PM_AddEvent(EV_RELOAD_FROM_EMPTY);
    } else {
        PM_StartWeaponAnim(PM_WEAPON_ANIM_RELOAD);
        ps->weaponTime = pml.weaponInfo->reloadTime;
        PM_AddEvent(EV_RELOAD);
    }

    if (ps->weaponState == WEAPON_STATE_RELOAD_START_INTERRUPT) {
        ps->weaponState = WEAPON_STATE_RELOADING_INTERRUPT;
    } else {
        ps->weaponState = WEAPON_STATE_RELOADING;
    }

    PM_SetWeaponReloadAddAmmoDelay();
}

void PM_BeginWeaponReload(void)
{
    playerState_t *const ps = pm->ps;
    const int32_t weaponState = ps->weaponState;
    const int32_t weapon = ps->currentWeapon;

    if ((weaponState != WEAPON_STATE_IDLE && weaponState != WEAPON_STATE_FIRING && weaponState != WEAPON_STATE_RECHAMBERING) ||
        weapon == 0 || weapon > BG_GetNumWeapons()) {
        return;
    }

    if (BG_GetInfoForWeapon(weapon)->clipRequired == 0) {
        BG_AnimScriptEvent(ps, ANIM_EVENT_RELOAD, qfalse, qtrue);
    }

    if (pml.weaponInfo->segmentedReload == 0 || pml.weaponInfo->reloadStartTime == 0) {
        PM_SetReloadingState();
        return;
    }

    PM_StartWeaponAnim(PM_WEAPON_ANIM_RELOAD_START);
    ps->weaponTime = pml.weaponInfo->reloadStartTime;
    ps->weaponState = WEAPON_STATE_RELOAD_START;
    PM_AddEvent(EV_RELOAD_START);
    PM_SetWeaponReloadAddAmmoDelay();
}

qboolean PM_Weapon_AllowReload(void)
{
    const playerState_t *const ps = pm->ps;
    const weaponInfo_t *const weaponInfo = BG_GetInfoForWeapon(ps->currentWeapon);
    const int32_t clipIndex = weaponInfo->clipIndex;
    const int32_t ammoIndex = weaponInfo->ammoIndex;
    const int32_t clipSize = BG_GetAmmoClipSize(clipIndex);
    const int32_t clipAmmo = ps->clips[clipIndex];
    int32_t reloadAmmoAdd;

    if (ps->ammo[ammoIndex] == 0 || clipAmmo >= clipSize) {
        return qfalse;
    }
    if (pml.weaponInfo->reloadRequiresAddRoom == 0) {
        return qtrue;
    }

    reloadAmmoAdd = pml.weaponInfo->reloadAmmoAdd;
    if (reloadAmmoAdd == 0 || reloadAmmoAdd >= clipSize) {
        return clipAmmo == 0 ? qtrue : qfalse;
    }

    return reloadAmmoAdd <= coduo_int32_from_bits((uint32_t)clipSize - (uint32_t)clipAmmo) ? qtrue : qfalse;
}

void PM_Weapon_ReloadDelayedAction(void)
{
    playerState_t *const ps = pm->ps;
    const weaponInfo_t *const weaponInfo = pml.weaponInfo;
    const int32_t weaponState = ps->weaponState;
    int32_t fillTime;
    int32_t interruptTime;

    if (weaponInfo->raiseEnabled == 0 || Com_BitCheck(ps->weaponRechamberBits, ps->currentWeapon) == 0) {
        PM_ReloadClip();
        return;
    }

    Com_BitClear(ps->weaponRechamberBits, ps->currentWeapon);
    PM_AddEvent(EV_EJECT_BRASS);

    if ((weaponState == WEAPON_STATE_RELOAD_START || weaponState == WEAPON_STATE_RELOAD_START_INTERRUPT) &&
        weaponInfo->reloadStartAddTime == 0) {
        return;
    }
    if (ps->weaponTime == 0) {
        PM_ReloadClip();
        return;
    }

    if (weaponState == WEAPON_STATE_RELOAD_START || weaponState == WEAPON_STATE_RELOAD_START_INTERRUPT) {
        fillTime = weaponInfo->reloadStartAddTime;
        if (weaponInfo->reloadStartTime < fillTime) {
            fillTime = weaponInfo->reloadStartTime;
        }
    } else {
        const int32_t clipIndex = BG_GetInfoForWeapon(ps->currentWeapon)->clipIndex;

        if (ps->clips[clipIndex] == 0 && weaponInfo->weaponType == WEAPTYPE_BULLET) {
            fillTime = weaponInfo->reloadEmptyTime;
        } else {
            fillTime = weaponInfo->reloadTime;
        }
        if (weaponInfo->reloadAddTime != 0 && weaponInfo->reloadAddTime < fillTime) {
            fillTime = weaponInfo->reloadAddTime;
        }
    }

    if (weaponInfo->raiseInterruptTime < fillTime) {
        interruptTime = weaponInfo->raiseInterruptTime;
    } else {
        interruptTime = 1;
    }

    fillTime = coduo_int32_from_bits((uint32_t)fillTime - (uint32_t)interruptTime);
    if (fillTime < 1) {
        PM_ReloadClip();
    } else {
        ps->weaponDelay = fillTime;
    }
}

qboolean PM_Weapon_FinishReload(qboolean pendingInterrupt)
{
    playerState_t *const ps = pm->ps;
    const int32_t weaponState = ps->weaponState;

    if (weaponState == WEAPON_STATE_RELOAD_START || weaponState == WEAPON_STATE_RELOAD_START_INTERRUPT) {
        if (pendingInterrupt != qfalse) {
            PM_Weapon_ReloadDelayedAction();
        }

        if (ps->weaponTime == 0) {
            const int32_t clipIndex = BG_GetInfoForWeapon(ps->currentWeapon)->clipIndex;

            if ((weaponState == WEAPON_STATE_RELOAD_START_INTERRUPT && ps->clips[clipIndex] != 0) || PM_Weapon_AllowReload() == qfalse) {
                Com_BitClear(ps->weaponRechamberBits, ps->currentWeapon);
                if (pml.weaponInfo->reloadEndTime == 0) {
                    ps->weaponState = WEAPON_STATE_IDLE;
                    PM_StartWeaponAnim(PM_WEAPON_ANIM_IDLE);
                    return qfalse;
                }

                ps->weaponState = WEAPON_STATE_RELOAD_END;
                PM_StartWeaponAnim(PM_WEAPON_ANIM_RELOAD_END);
                ps->weaponTime = pml.weaponInfo->reloadEndTime;
                PM_AddEvent(EV_RELOAD_END);
            } else {
                PM_SetReloadingState();
            }
        }
        return qtrue;
    }

    if (weaponState == WEAPON_STATE_RELOAD_END) {
        ps->weaponState = WEAPON_STATE_IDLE;
        PM_StartWeaponAnim(PM_WEAPON_ANIM_IDLE);
    } else if (weaponState == WEAPON_STATE_RELOADING || weaponState == WEAPON_STATE_RELOADING_INTERRUPT) {
        if (pendingInterrupt != qfalse) {
            PM_Weapon_ReloadDelayedAction();
            if (ps->weaponTime != 0) {
                return qtrue;
            }
        }

        if (ps->weaponTime == 0) {
            Com_BitClear(ps->weaponRechamberBits, ps->currentWeapon);
            if (pml.weaponInfo->segmentedReload == 0) {
                ps->weaponState = WEAPON_STATE_IDLE;
                PM_StartWeaponAnim(PM_WEAPON_ANIM_IDLE);
            } else {
                if (weaponState != WEAPON_STATE_RELOADING_INTERRUPT && PM_Weapon_AllowReload() != qfalse) {
                    PM_SetReloadingState();
                    return qtrue;
                }

                if (pml.weaponInfo->reloadEndTime != 0) {
                    ps->weaponState = WEAPON_STATE_RELOAD_END;
                    PM_StartWeaponAnim(PM_WEAPON_ANIM_RELOAD_END);
                    ps->weaponTime = pml.weaponInfo->reloadEndTime;
                    PM_AddEvent(EV_RELOAD_END);
                    return qtrue;
                }

                ps->weaponState = WEAPON_STATE_IDLE;
                PM_StartWeaponAnim(PM_WEAPON_ANIM_IDLE);
            }
        }

        if (pendingInterrupt != qfalse) {
            return qtrue;
        }
    }

    return qfalse;
}

void PM_Weapon_CheckForReload(void)
{
    playerState_t *const ps = pm->ps;
    int32_t weaponState = ps->weaponState;
    qboolean shouldBeginReload = qfalse;

    if (pml.weaponInfo->segmentedReload != 0 && (weaponState == WEAPON_STATE_RELOAD_START || weaponState == WEAPON_STATE_RELOADING) &&
        (pm->command.buttons & PM_BUTTON_FIRE) != 0 && (pm->oldCommand.buttons & PM_BUTTON_FIRE) == 0) {
        if (weaponState == WEAPON_STATE_RELOAD_START) {
            ps->weaponState = WEAPON_STATE_RELOAD_START_INTERRUPT;
        } else {
            ps->weaponState = WEAPON_STATE_RELOADING_INTERRUPT;
        }
        weaponState = ps->weaponState;
    }

    if (pml.weaponInfo->weaponClass == WEAPCLASS_LMG) {
        if ((ps->playerStateFlags & PMF_ADS) == 0 || ps->adsFraction < PM_WEAPON_LMG_ADS_FRACTION_MIN) {
            return;
        }
    }
    if ((ps->playerStateFlags & PMF_SPRINTING) != 0) {
        return;
    }

    switch (weaponState) {
    case WEAPON_STATE_RAISING:
    case WEAPON_STATE_DROPPING:
    case WEAPON_STATE_MELEE_WINDUP:
    case WEAPON_STATE_MELEE_RELAX:
    case WEAPON_STATE_DEPLOYING:
    case WEAPON_STATE_BREAKING_DOWN:
        return;

    case WEAPON_STATE_RELOADING:
    case WEAPON_STATE_RELOADING_INTERRUPT:
    case WEAPON_STATE_RELOAD_START:
    case WEAPON_STATE_RELOAD_START_INTERRUPT:
    case WEAPON_STATE_RELOAD_END:
        if (pm->weaponAnimscriptEnabled != 0 && BG_GetInfoForWeapon(ps->currentWeapon)->clipRequired == 0) {
            BG_AnimScriptEvent(ps, ANIM_EVENT_RELOAD, qfalse, qtrue);
        }
        return;

    default: {
        const weaponInfo_t *const weaponInfo = BG_GetInfoForWeapon(ps->currentWeapon);
        const int32_t clipIndex = weaponInfo->clipIndex;
        const int32_t ammoIndex = weaponInfo->ammoIndex;

        if ((pm->command.wbuttons & PM_WBUTTON_RELOAD) != 0 && PM_Weapon_AllowReload() != qfalse) {
            shouldBeginReload = qtrue;
        }

        if (ps->clips[clipIndex] == 0 && ps->ammo[ammoIndex] != 0 && weaponState != WEAPON_STATE_FIRING &&
            ((ps->playerStateFlags & PMF_PRONE) == 0 || (pm->command.forwardmove == 0 && pm->command.rightmove == 0))) {
            shouldBeginReload = qtrue;
        }

        if (shouldBeginReload != qfalse) {
            PM_BeginWeaponReload();
        }
        return;
    }
    }
}

void PM_RemoveEmptyClipOnlyWeapon(void)
{
    playerState_t *const ps = pm->ps;
    const int32_t weapon = ps->currentWeapon;
    const weaponInfo_t *const weaponInfo = BG_GetInfoForWeapon(weapon);

    if (weaponInfo->clipRequired == 0 || pml.weaponInfo->weaponClass == WEAPCLASS_SPOTTER || ps->clips[weaponInfo->clipIndex] != 0 ||
        ps->ammo[weaponInfo->ammoIndex] != 0) {
        return;
    }

    (void)BG_TakePlayerWeapon(ps, weapon);
    PM_AddEvent(EV_NOAMMO);
}
