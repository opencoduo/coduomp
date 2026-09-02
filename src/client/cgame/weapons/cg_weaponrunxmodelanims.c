// Source: uo_cgame_mp_x86.dll 0x30042d30..0x30042f66
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30042d30_30042f66.mcode
//
// CG_WeaponRunXModelAnims (0x30042d30)
// ------------------------------------
// Drive one weapon's first-person XModel overlay animation state for a single
// frame. Called once per frame (from the BG_PlayerStateToEntityState path at
// 0x30046641) with the local predicted player state `ps` and the weapon's
// cgWeaponInfo record `wi = &cg_weaponInfos[ps->currentWeapon]`.
//
// NAME: the mechanical `# name CG_ResetEntity` is a SIZE-MATCH guess (win size
// 0x236) with zero behavioral basis and is REJECTED. The correct name is proven
// by the Com_Printf format string PUSHed on the unknown-anim path (0x30042f45,
// address 0x3007abc4, dumped via objdump -s -j .rdata):
//     "CG_WeaponRunXModelAnims: Unknown weapon animation %i\n"
// (This is a different routine from CG_ResetPlayerEntity at 0x30034880.)
//
// ABI: __usercall — `ps` arrives in EDI (register), `wi` is the single cdecl
// stack arg (MOV EBX,[ESP+8]); plain RET, caller-cleaned. Proven from the caller
// at 0x30046633..0x30046646: it computes EAX = &cg_weaponInfos[currentWeapon]
// (IMUL currentWeapon,0x1c4; ADD 0x30413580), sets EDI = EBX (= ps), PUSHes EAX,
// CALLs, then ADD ESP,4.
//
// Behavior:
//  1. animTree = cgame_syscall(CG_DOBJ_GET_TREE=0xb5, wi->viewDObjSelf)
//     -- resolve the weapon overlay DObj's runtime animation tree.
//  2. weaponInfo = bg_weaponInfos[ps->currentWeapon]   (the BG weaponInfo_t record).
//  3. Vehicle/mounted gate: if (ps->entityStateFlags & 0x106000) run only when the
//     player is in vehicle type 1, seat position 3; otherwise return (no run).
//  4. Compute useAdsAnim: default 0. If NOT (in WEAPON_STATE_RELOADING while
//     still inside the reload-transition window, i.e. ps->weaponTime -
//     weaponInfo->reloadLoopTime > 0), then set it from the PMF_ADS
//     (0x20) bit of ps->playerStateFlags.
//  5. ADS overlay: if the weapon has ADS enabled (weaponInfo->adsEnabled != 0)
//     and its weaponClass is neither 3 nor 8, cross-fade the ADS overlay via
//     CG_PlayADSAnim(useAdsAnim ? WEAPON_XANIM_ADS_UP :
//                               WEAPON_XANIM_ADS_DOWN, animTree).
//  6. Change gate: if ps->weaponAnim == wi->lastRunAnim the anim is unchanged
//     since last frame -> return without touching slots.
//  7. animState = ps->weaponAnim & ~ANIM_TOGGLEBIT (0x200). If animState > 0x14
//     the state is unknown: activate WEAPON_XANIM_IDLE, print the diagnostic,
//     and latch lastRunAnim. Otherwise switch on animState (jump table @0x30042f68):
//       - state 0 (idle): verify all 21 XModel anims are registered (trap 0xb8);
//         if any is missing, store lastRunAnim = -1 and return (retry next frame).
//         If all present: for the "held-weapon" classes (3 or 8) with the QUALIFY
//         flag set, activate WEAPON_XANIM_LMG_DEPLOYED; otherwise activate
//         WEAPON_XANIM_IDLE or WEAPON_XANIM_EMPTY_IDLE according to the clip.
//       - states 1..0x14: map to a fixed weapon-XAnim node via the switch and
//         push it active via CG_StartWeaponAnim.
//     Every non-deferred path finishes by latching wi->lastRunAnim = ps->weaponAnim.
//
// CG_StartWeaponAnim(weaponIndex, animTree, activeAnimIndex) activates one of the
// 21 ordinary weapon XAnim nodes using that node's animRates[] value. The animTree
// argument is the runtime tree from step 1 (EBX = ESI at the call sites).

#include "client/cgame/globals.h"          /* playerState_t, cgWeaponInfo_t, weaponInfo_t,
                                 bg_weaponInfos, cgame_syscall,
                                 cg_weaponRunXModelAnimsInvalidAnimFmt */
#include "client/cgame/client_recovered.h" /* CG_DOBJ_GET_TREE, CG_XANIM_HAS_FINISHED,
                                 CG_StartWeaponAnim, CG_PlayADSAnim,
                                 CG_WeaponRunXModelAnims, Com_Printf,
                                 PMF_ADS */

/* ps->entityStateFlags bits that force the vehicle/mounted animation path. Tested as a
 * single 0x106000 mask (TEST EAX,0x106000). Exact bit names unresolved. */
enum {
    CG_PS_MOUNTED_MASK = 0x106000
};

/* Vehicle gate constants (ps->vehicleType / ps->vehiclePosition). */
enum {
    CG_VEHICLE_TYPE_MOUNTED = 1,
    CG_VEHICLE_SEAT_GUNNER = 3
};

/* weaponClass values (weaponInfo_t::weaponClass, +0x80) that skip the ADS-overlay
 * blend and take the "held weapon" idle branch. */
enum {
    CG_WEAPONCLASS_SKIP_ADS_A = 3,
    CG_WEAPONCLASS_SKIP_ADS_B = 8
};

void CG_WeaponRunXModelAnims(playerState_t *ps, cgWeaponInfo_t *wi)
{
    /* 1. Resolve the weapon overlay DObj's runtime animation tree. */
    intptr_t animTree = cgame_syscall(CG_DOBJ_GET_TREE, (intptr_t)wi->viewDObjSelf);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (animTree == 0) {
        return;
    }

    /* 2. The BG weapon record for the current weapon. */
    weaponInfo_t *weaponInfo = bg_weaponInfos[ps->currentWeapon];

    /* 3. Vehicle/mounted gate. */
    if ((ps->entityStateFlags & CG_PS_MOUNTED_MASK) != 0) {
        if (ps->vehicleType != CG_VEHICLE_TYPE_MOUNTED) {
            return;
        }
        if (ps->vehiclePosition != CG_VEHICLE_SEAT_GUNNER) {
            return;
        }
    }

    /* 4. useAdsAnim: 0 during the reload-transition window of the reload-loop
     * state, else driven by the QUALIFY (0x20) anim flag. */
    int32_t useAdsAnim = 0;
    int32_t reloadWindow = coduo_int32_from_bits((uint32_t)ps->weaponTime - (uint32_t)weaponInfo->reloadLoopTime);
    if (!(ps->weaponState == WEAPON_STATE_RELOADING && reloadWindow > 0)) {
        if ((ps->playerStateFlags & PMF_ADS) != 0) {
            useAdsAnim = 1;
        }
    }

    /* 5. ADS overlay cross-fade for ADS-capable weapons (except classes 3/8). */
    if (weaponInfo->adsEnabled != 0 && weaponInfo->weaponClass != CG_WEAPONCLASS_SKIP_ADS_A &&
        weaponInfo->weaponClass != CG_WEAPONCLASS_SKIP_ADS_B) {
        CG_PlayADSAnim(useAdsAnim ? WEAPON_XANIM_ADS_UP : WEAPON_XANIM_ADS_DOWN, animTree);
    }

    /* 6. Change gate: unchanged anim state -> nothing to do. */
    if (coduo_int32_from_bits((uint32_t)ps->weaponAnim) == wi->lastRunAnim) {
        return;
    }

    /* 7. Decode the anim-state enum (drop the toggle bit). */
    int32_t animState = coduo_int32_from_bits((uint32_t)ps->weaponAnim & ~(uint32_t)ANIM_TOGGLEBIT);

    if (animState == 0) {
        /* Idle: require every XModel overlay anim to report finished first. */
        int32_t animIndex;
        for (animIndex = WEAPON_XANIM_IDLE; animIndex < WEAPON_XANIM_ADS_UP; ++animIndex) {
            if (cgame_syscall(CG_XANIM_HAS_FINISHED, animTree, animIndex) == 0) {
                /* At least one anim is still running; retry next frame. */
                wi->lastRunAnim = -1;
                return;
            }
        }

        /* All present. "Held weapon" classes with the QUALIFY flag use
         * LMG_DEPLOYED; every other weapon picks IDLE (clip loaded) or
         * EMPTY_IDLE (empty). */
        if ((weaponInfo->weaponClass == CG_WEAPONCLASS_SKIP_ADS_A || weaponInfo->weaponClass == CG_WEAPONCLASS_SKIP_ADS_B) &&
            (ps->playerStateFlags & PMF_ADS) != 0) {
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_LMG_DEPLOYED);
        } else {
            int32_t activeAnimIndex = (ps->clips[weaponInfo->clipIndex] != 0) ? WEAPON_XANIM_IDLE : WEAPON_XANIM_EMPTY_IDLE;
            CG_StartWeaponAnim(ps->currentWeapon, animTree, activeAnimIndex);
        }
    } else {
        /* This is the source-level switch represented by the original jump
         * table at 0x30042f68. Keep it as control flow: the retail image has no
         * separate anim-state-to-slot data array. Each case reaches a distinct
         * PUSH immediate at 0x30042eaf..0x30042f25. */
        switch (animState) {
        case PM_WEAPON_ANIM_FIRE:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_FIRE);
            break;
        case PM_WEAPON_ANIM_FIRE_LASTSHOT:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_LAST_SHOT);
            break;
        case PM_WEAPON_ANIM_RECHAMBER:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_RECHAMBER);
            break;
        case PM_WEAPON_ANIM_ADS_FIRE:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_ADS_FIRE);
            break;
        case PM_WEAPON_ANIM_ADS_FIRE_LASTSHOT:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_ADS_LAST_SHOT);
            break;
        case PM_WEAPON_ANIM_ADS_RECHAMBER:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_ADS_RECHAMBER);
            break;
        case PM_WEAPON_ANIM_MELEE:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_MELEE);
            break;
        case PM_WEAPON_ANIM_LOWER:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_DROP);
            break;
        case PM_WEAPON_ANIM_SWITCH_RAISE:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_RAISE);
            break;
        case PM_WEAPON_ANIM_RELOAD:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_RELOAD);
            break;
        case PM_WEAPON_ANIM_RELOAD_EMPTY:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_RELOAD_EMPTY);
            break;
        case PM_WEAPON_ANIM_RELOAD_START:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_RELOAD_START);
            break;
        case PM_WEAPON_ANIM_RELOAD_END:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_RELOAD_END);
            break;
        case PM_WEAPON_ANIM_ALT_SWITCH_LOWER:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_ALT_DROP);
            break;
        case PM_WEAPON_ANIM_ALT_SWITCH_RAISE:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_ALT_RAISE);
            break;
        case PM_WEAPON_ANIM_SPECIAL_FIRE:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_HOLD_FIRE);
            break;
        case PM_WEAPON_ANIM_ADS_IN:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_LMG_DEPLOY);
            break;
        case PM_WEAPON_ANIM_ADS_OUT:
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_LMG_BREAKDOWN);
            break;
        default:
            /* Out-of-range states, the unnamed pose 1, and PM_WEAPON_ANIM_DEPLOYED
             * share the retail diagnostic block at 0x30042f2a. DEPLOYED is
             * selected through the idle-path flag gate above, not this switch. */
            CG_StartWeaponAnim(ps->currentWeapon, animTree, WEAPON_XANIM_IDLE);
            Com_Printf(cg_weaponRunXModelAnimsInvalidAnimFmt, (int)animState);
            break;
        }
    }

    /* Latch the processed anim state for next frame's change gate. */
    wi->lastRunAnim = coduo_int32_from_bits((uint32_t)ps->weaponAnim);
}
