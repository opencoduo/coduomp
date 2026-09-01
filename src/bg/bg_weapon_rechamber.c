#include "bg_pmove.h"

#include "qcommon/q_bits.h"

/*
 * Rechamber state-machine cluster shared by cgame and game.  The original
 * Windows bodies are instruction-identical apart from global and call targets:
 *
 *   uo_cgame_mp_x86.dll  PM_Weapon_FinishRechamber   0x30012460
 *   uo_game_mp_x86.dll   PM_Weapon_FinishRechamber   0x200123a0
 *   uo_cgame_mp_x86.dll  PM_Weapon_CheckForRechamber 0x300124b0
 *   uo_game_mp_x86.dll   PM_Weapon_CheckForRechamber 0x200123f0
 *
 * The Linux game bodies at RVAs 0x00034073 and 0x000340ab use the same state
 * tests, events, timings, and ADS threshold.  Its compiler retains calls to the
 * shared bit and event helpers that MSVC inlines.  The ADS operation is only a
 * comparison of the stored binary32 value with 0.75f; all targets select the
 * hip animation when that comparison is false, including unordered input.
 */

void PM_Weapon_FinishRechamber(void)
{
    PM_ContinueWeaponAnim(PM_WEAPON_ANIM_IDLE);
    pm->ps->weaponState = WEAPON_STATE_IDLE;
}

qboolean PM_Weapon_CheckForRechamber(qboolean allowInterrupt)
{
    playerState_t *const ps = pm->ps;
    const int32_t weaponState = ps->weaponState;

    if (pml.weaponInfo->raiseEnabled == 0 ||
        Com_BitCheck(ps->weaponRechamberBits, ps->currentWeapon) == 0) {
        return qfalse;
    }

    if (weaponState == WEAPON_STATE_RECHAMBERING &&
        allowInterrupt != qfalse) {
        Com_BitClear(ps->weaponRechamberBits, ps->currentWeapon);
        PM_AddEvent(EV_EJECT_BRASS);
        if (ps->weaponTime != 0) {
            return qtrue;
        }
    }

    if (ps->weaponTime == 0 ||
        ((weaponState != WEAPON_STATE_FIRING &&
          weaponState != WEAPON_STATE_RECHAMBERING &&
          weaponState != WEAPON_STATE_MELEE_WINDUP &&
          weaponState != WEAPON_STATE_MELEE_RELAX) &&
         ps->weaponDelay == 0)) {
        if (weaponState == WEAPON_STATE_RECHAMBERING) {
            PM_Weapon_FinishRechamber();
        } else if (weaponState == WEAPON_STATE_IDLE) {
            if (ps->adsFraction > PM_WEAPON_ADS_RAISE_THRESHOLD) {
                PM_StartWeaponAnim(PM_WEAPON_ANIM_ADS_RECHAMBER);
            } else {
                PM_StartWeaponAnim(PM_WEAPON_ANIM_RECHAMBER);
            }

            ps->weaponState = WEAPON_STATE_RECHAMBERING;
            ps->weaponTime = pml.weaponInfo->raiseTime;
            if (pml.weaponInfo->raiseInterruptTime == 0 ||
                pml.weaponInfo->raiseTime <=
                    pml.weaponInfo->raiseInterruptTime) {
                ps->weaponDelay = 1;
            } else {
                ps->weaponDelay = pml.weaponInfo->raiseInterruptTime;
            }
            PM_AddEvent(EV_RECHAMBER_WEAPON);
        }
    }

    return qfalse;
}
