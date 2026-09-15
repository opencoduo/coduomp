// Source: uo_cgame_mp_x86.dll 0x30014710..0x30014a80
//         uo_game_mp_x86.dll  0x20014650..0x200149c0
//         game.mp.uo.i386.so  RVA 0x0003726c
//
// PM_Weapon -- the shared per-frame weapon pmove driver.
// The two Windows bodies have the same 260-instruction graph after relocation
// normalization.  Only the module-owned debug print-target constant differs:
// cgame suppresses target 3 while game suppresses target 2.  Local service
// adapters preserve that diagnostic policy without a platform fork.  The
// supporting Mac cgame/game symbols independently name equal-size 0x3c8-byte
// PM_Weapon bodies at code offsets 0xdac0 and 0xe3a0.
//
// NAME ADJUDICATION: the .mcode header's "CalcMuzzlePoint" is a pure SIZE guess
// (win size 0x370 vs matched 0x36d) and is REJECTED per the no-size rule. This
// function computes NO muzzle point and never produces a vec3 fire origin from view
// angles + weapon offsets. What it actually does (proven below) is the Quake3/CoD
// PM_Weapon dispatcher: it early-outs on dead/held/vehicle-locked players, runs the
// weapon-idle state update, adjusts the two weapon countdown timers, handles grenade
// cook-off "special time" event generation, then runs the whole weapon fire / reload
// / melee / change / rechamber / finish dispatch chain and the two dev-mode weapon
// state loggers. Resolved by behavior + call graph -> PM_Weapon.
//
// Struct identity (shared with the rest of the pmove cluster; see globals.h):
//   pm (0x30539850)  -- the BG pmove context (pmove_t).
//   pm->ps (+0x00) -- the playerState (playerState_t), reached
//                                  through the leading pointer.
//   pml.weaponInfo (0x30539608) -- cached current-weapon weaponInfo_t pointer.
//   pml.msec (0x305395a8)       -- current pmove frame time delta (ms).
//
// Touched playerState (playerState_t) offsets, all matching the shared layout:
//   flags +0x0c, pmType +0x04, weaponTime +0x2c, weaponDelay +0x30,
//   grenadeTimeLeft +0x34, entityStateFlags +0x84, eventIndex +0x88, events[] +0x8c,
//   eventParms[] +0x9c, psClientNum +0xd4, currentWeapon +0xd8, weaponState +0xdc,
//   weaponAnim +0x624, aimSpreadScale +0x628.
// Touched pmove_t offsets: command.buttons +0x08, command.forwardmove +0x18,
//   command.rightmove +0x19, oldCommand.buttons +0x20.
// Touched weaponInfo_t offsets: weaponType +0x7c, specialTimeThreshold +0x268,
//   specialTimeEnabled +0x338 (cookOffHold), missileSplashDamage +0x37c.
//
// ABI: PUSH EDI at entry, ESI/EBX pushed lazily on the paths that use them, RET
// (no imm) -- no stack arguments; the routine reads all its inputs from the two
// globals. Modeled as `void PM_Weapon(void)`. Register save/restore is a
// calling-convention detail, not source behavior.
//
// PROVISIONAL CALLEES whose ABI I re-derived from THIS call site's bytes:
//   - PM_UpdateAimDownSightLerp (0x30011f50): called with no stack push/cleanup
//     -> no args. Reads pm/pml.weaponInfo internally. Its mechanical name
//     "PM_Weapon_FireWeapon" is a size-guess COLLISION with
//     PM_Weapon_StartFiring at 0x30013f20
//     and is rejected; superseded by its own .mcode.
//   - PM_WeaponUseAmmo (0x30012350): fastcall-shaped -- weapon index in
//     ECX (= ps->currentWeapon), amount in ESI (= 1). objdump confirms it does
//     ps->clips[weaponInfo->clipIndex] -= amount, clamped up to 0.
//   - BG_FirstValidItem (0x30002f40): EAX = fire scriptList, ECX = clientNum.
//   - The finish/dispatch chain callees take the delayExpired flag (ESI, from
//     PM_Weapon_WeaponTimeAdjust) as their single stack argument where they take one.

#include "bg_pmove.h"

#include "bg_animation.h"
#include "bg_animation_services.h"
#include "bg_pmove_services.h"
#include "compat/coduo_int32_bits.h"
#include "compat/crt/random_compat.h"

/* The player fire animation-script list is events[ANIM_EVENT_FIRE_WEAPON] in
 * the shared BG static
 * animation table, at the machine-code-proven +0x1fe8c offset. */
/* entityStateFlags (ps+0x84) bits: the shared force-stance pair blocks weapon
 * processing entirely. */

/* grenadeTimeLeft is stepped down by 10 ms per commit on the cook-off path. */
enum { GRENADE_SPECIAL_TIME_STEP = 10 };

void PM_Weapon(void)
{
    pmove_t *move = pm; /* [0x30539850] */
    playerState_t *ps = move->ps;                                /* MOV EAX,[EDI] */

    /* 0x30014719 TEST CH,0x8 on ps->playerStateFlags: skip all weapon processing this frame. */
    if (ps->playerStateFlags & PMF_RESPAWNED) {
        return; /* JNZ 0x30014a7e (POP EDI; RET) */
    }

    /* 0x30014725 CMP ps->pmType,6 / JL: pmType >= 6 is a non-live player -> drop the
     * held weapon and return. (+0xd8 is currentWeapon; store 0.) */
    if (ps->pmType >= PM_TYPE_DEAD) {
        ps->currentWeapon = 0; /* MOV [EAX+0xd8],0 */
        return;                /* POP ESI; POP EDI; RET */
    }

    /* 0x3001473f TEST CH,0x60 on ps->entityStateFlags: either 0x2000/0x4000 disable bit
     * set -> weapon processing disabled, return. */
    if (ps->entityStateFlags & EF_FORCED_STANCE_MASK) {
        return; /* JNZ 0x30014a7d */
    }

    /* 0x3001474e TEST ps->entityStateFlags,0x100000 (EF_IN_VEHICLE, in-vehicle). */
    if (ps->entityStateFlags & EF_IN_VEHICLE) {
        /* In a vehicle: allow weapon processing only in the gunner pose
         * (vehicleType == 1 && vehiclePosition == 3). Otherwise holster: run the
         * weapon-idle update, clear weaponState/weaponAnim, and reset weapon timers.
         * 0x30014757 CMP ps->vehicleType,1 / 0x30014760 CMP ps->vehiclePosition,3. */
        if (!(ps->vehicleType == 1 && ps->vehiclePosition == 3)) {
            /* 0x30014769 CALL 0x30011f50 (no args). */
            PM_UpdateAimDownSightLerp();

            /* Re-read ps (the callee may have changed pm->ps's state; the
             * machine code reloads [EDI] each time). 0x3001476e MOV EAX,[EDI]. */
            ps = move->ps;

            /* 0x30014770 if ps->weaponState != 0, sync weaponAnim to idle. */
            if (ps->weaponState != 0) {
                PM_StartWeaponAnim(0); /* XOR EDX,EDX; CALL 0x300123e0 */
            }

            /* 0x30014783..0x30014795: clear weaponTime, weaponDelay, weaponState. */
            move->ps->weaponTime = 0;   /* [ECX+0x2c] */
            move->ps->weaponDelay = 0;  /* [EDX+0x30] */
            move->ps->weaponState = 0;  /* [EAX+0xdc] = WEAPON_STATE_IDLE */

            /* 0x30014797 XOR EDX,EDX; 0x3001479a JMP 0x30012430 (tail call). */
            PM_ContinueWeaponAnim(0);
            return;
        }
        /* gunner pose -> fall through to normal processing at 0x3001479f. */
    }

    /* 0x3001479f dev-mode weapon-state change logger, gated on bg_debugWeaponState.integer
     * being enabled and not the "none" print target. */
    int32_t debugWeaponState = bg_debugWeaponState.integer;
    if (debugWeaponState != 0 &&
        debugWeaponState != bg_compat_pmove_weapon_debug_target_none()) {
        PM_Weapon_PrintWeaponState(); /* client CALL 0x30014a80 */
        move = pm; /* reloads EDI at 0x300147b4 */
    }

    /* 0x300147ba dev-mode weapon-anim (weaponAnim +0x624) pose logger. */
    int32_t debugWeaponAnim = bg_debugWeaponAnim.integer;
    if (debugWeaponAnim != 0 &&
        debugWeaponAnim != bg_compat_pmove_weapon_debug_target_none()) {
        PM_Weapon_PrintWeaponAnim(); /* client CALL 0x30014bd0 */
        move = pm; /* reloads EDI at 0x300147cd */
    }

    /* 0x300147d3 CALL 0x30011f50 (weapon-idle update again, post-logging). */
    PM_UpdateAimDownSightLerp();

    /* ---- Grenade cook-off "special time" handling ---------------------------------
     * 0x300147d8: only for the current grenade weapon (weaponType == GRENADE). */
    const weaponInfo_t *weaponInfo = pml.weaponInfo; /* [0x30539608] */
    if (weaponInfo->weaponType == WEAPTYPE_GRENADE) {
        ps = move->ps;

        /* 0x300147e8: only when the weapon uses cook-off hold (specialTimeEnabled).
         * When it does, and the attack-edge gate below holds, step the cook-off timer
         * and possibly emit the mid-cook "grenade special" event. */
        if (weaponInfo->specialTimeEnabled != 0) {
            /* 0x300147f0..0x30014805: the attack-edge gate, an XOR of two 0x10 bits.
             * stateBit = pm->command.buttons(+0x08) & 0x10; oldBit =
             * pm->oldCommand.buttons(+0x20) & 0x10. Full branch trace of the
             * four instructions proves the timer steps exactly when the two bits
             * DIFFER (stateBit != oldBit), an edge detector; when they agree the code
             * bails to the clamp-only path at 0x3001484c. */
            int stateBit = (move->command.buttons & PM_BUTTON_ADS) != 0;
            int oldBit   = (move->oldCommand.buttons & PM_BUTTON_ADS) != 0;

            if (stateBit != oldBit) {
                /* 0x30014807: only if grenadeTimeLeft >= specialTimeThreshold. */
                if (ps->grenadeTimeLeft >= weaponInfo->specialTimeThreshold) {
                    /* 0x30014814 grenadeTimeLeft += -10 (step down by 10 ms). */
                    ps->grenadeTimeLeft = coduo_int32_from_bits(
                        (uint32_t)ps->grenadeTimeLeft -
                        (uint32_t)GRENADE_SPECIAL_TIME_STEP);

                    /* 0x3001481a..0x30014840: append EV_GRENADE_SPOON (182), parm 0. */
                    ps = move->ps;
                    uint32_t eventIndex = (uint32_t)ps->eventIndex;
                    ps->events[eventIndex & (MAX_PS_EVENTS - 1u)] = EV_GRENADE_SPOON;
                    ps->eventParms[eventIndex & (MAX_PS_EVENTS - 1u)] = 0;
                    ps->eventIndex = coduo_int32_from_bits(eventIndex + 1u);

                    weaponInfo = pml.weaponInfo; /* reload EDX at 0x30014846 */
                }
            }
        }

        /* 0x3001484c: if grenadeTimeLeft <= 0 there is no cook-off in flight. */
        ps = move->ps;
        if (ps->grenadeTimeLeft > 0) {
            /* 0x30014859 CMP specialTimeEnabled,0 JZ 0x30014874 (skip bleed) and
             * 0x30014861 CMP grenadeTimeLeft,specialTimeThreshold JGE 0x30014874
             * (skip bleed): the timer bleeds down by one frame only while the weapon
             * uses cook-off hold AND is still below its threshold. */
            if (weaponInfo->specialTimeEnabled != 0 &&
                ps->grenadeTimeLeft < weaponInfo->specialTimeThreshold) {
                /* 0x30014869 grenadeTimeLeft -= pml.msec. */
                ps->grenadeTimeLeft = coduo_int32_from_bits(
                    (uint32_t)ps->grenadeTimeLeft - (uint32_t)pml.msec);
            }

            /* 0x30014874: has the cook-off run out? grenadeTimeLeft <= 50 -> commit
             * the detonation/launch events; > 50 -> jump past to 0x30014916. */
            ps = move->ps;
            if (ps->grenadeTimeLeft <= 50) {
                /* 0x30014886 clamp grenadeTimeLeft up to 50. */
                ps->grenadeTimeLeft = 50;

                /* 0x30014889..0x300148af: append EV_FIRE_WEAPON (163), parm 0. */
                uint32_t eventIndex = (uint32_t)ps->eventIndex;
                ps->events[eventIndex & (MAX_PS_EVENTS - 1u)] = EV_FIRE_WEAPON;
                ps->eventParms[eventIndex & (MAX_PS_EVENTS - 1u)] = 0;
                ps->eventIndex = coduo_int32_from_bits(eventIndex + 1u);

                /* 0x300148b5: when the weapon launches on cook-off, also append
                 * EV_GRENADE_SUICIDE (210), parm 0. */
                if (pml.weaponInfo->missileSplashDamage != 0) {
                    ps = move->ps;
                    eventIndex = (uint32_t)ps->eventIndex;
                    ps->events[eventIndex & (MAX_PS_EVENTS - 1u)] = EV_GRENADE_SUICIDE;
                    ps->eventParms[eventIndex & (MAX_PS_EVENTS - 1u)] = 0;
                    ps->eventIndex = coduo_int32_from_bits(eventIndex + 1u);
                }

                /* 0x300148ee..0x300148fb: consume one round from the current weapon's
                 * clip (ECX = ps->currentWeapon, ESI = 1). */
                ps = move->ps;
                PM_WeaponUseAmmo(ps->currentWeapon, 1);

                /* 0x30014900..0x30014909: weaponDelay = 0, weaponTime = 1600 (0x640). */
                move->ps->weaponDelay = 0;
                move->ps->weaponTime = 1600;

                /* 0x30014911 JMP 0x30013a00: tail call. */
                PM_RemoveEmptyClipOnlyWeapon();
                return;
            }
            /* grenadeTimeLeft > 50: 0x30014916 falls through to the "adjust ground
             * timers" block below. */
        } else {
            /* grenadeTimeLeft <= 0 at 0x30014853 JLE 0x3001499a -> skip straight to
             * the weapon dispatch chain. */
            goto weapon_dispatch;
        }
    } else {
        /* Not a grenade (weaponType != GRENADE): 0x300147e2 JNZ 0x3001499a. */
        goto weapon_dispatch;
    }

    /* ---- grenadeTimeLeft > 50: pre-cook weapon-delay timer bookkeeping -----------
     * 0x30014916 block. The gating bit here is pm->command.buttons(+0x08) & 1,
     * NOT a playerState flag. ps still points at move->ps (EAX at 0x30014874). */
    ps = move->ps;
    {
        /* 0x30014916 TEST pm->command.buttons,0x1: when set, ensure weaponDelay
         * is at least one frame ahead -- if (weaponDelay - pml.msec) <= 0, re-arm it
         * to pml.msec + 1. */
        if ((move->command.buttons & PM_BUTTON_FIRE) != 0) {
            /* 0x3001491c..0x3001492c. */
            int32_t remaining = coduo_int32_from_bits(
                (uint32_t)ps->weaponDelay - (uint32_t)pml.msec);
            if (remaining <= 0) {
                ps->weaponDelay = coduo_int32_from_bits((uint32_t)pml.msec + 1u);
            }
        }

        /* 0x3001492f TEST pm->command.buttons,0x1 again; JNZ 0x3001499a: when
         * set (grounded/held), skip the cook-off anim script and go to dispatch. */
        if ((move->command.buttons & PM_BUTTON_FIRE) != 0) {
            goto weapon_dispatch;
        }

        /* 0x30014935 MOV EDI,[EDI]: EDI now aliases ps for the rest of this block. */
        /* 0x30014937 if (weaponDelay - pml.msec) > 0 -> skip. */
        if (coduo_int32_from_bits((uint32_t)ps->weaponDelay - (uint32_t)pml.msec) > 0) {
            goto weapon_dispatch;
        }
        /* 0x30014944 CMP ps->pmType,6 (ESI holds 6); JGE -> skip. */
        if (ps->pmType >= PM_TYPE_DEAD) {
            goto weapon_dispatch;
        }

        /* 0x30014949: look up the player fire animation script and, if it matches the
         * current condition state, run one of its commands. The fire scriptList sits
         * at +0x1fe8c inside the BG static-animation table. */
        bg_anim_script_list_t *fireScriptList =
            &bgAnimStaticTable->events[ANIM_EVENT_FIRE_WEAPON];
        /* 0x30014959 CMP scriptCount,0; JZ -> skip when the list is empty. */
        if (fireScriptList->count != 0) {
            /* 0x3001495d ECX = ps->psClientNum (+0xd4); CALL BG_FirstValidItem
             * (EAX = &fireScriptList, ECX = clientNum). */
            bg_anim_script_t *script =
                BG_FirstValidItem(ps->psClientNum, fireScriptList);

            /* 0x3001496a JZ / 0x3001496e CMP script->commandCount,0 JZ: need a matched
             * script with at least one command. */
            if (script != NULL && script->commandCount != 0) {
                /* 0x30014976 CALL rand; 0x3001497b CDQ; 0x3001497c IDIV
                 * script->commandCount -> pick command index = rand() % commandCount. */
                int32_t commandIndex =
                    coduo_server_randrange(0, script->commandCount);

                /* 0x30014982..0x30014992: BG_AnimScriptAnimation(&script->commands[idx],
                 * ps, setTimer=1, allowContinue=0, checkDuration=1). SHL EDX,4 shows
                 * bg_anim_script_command_t is 0x10 bytes; LEA [EDX + ESI + 0x8c] where
                 * ESI = script, +0x8c = script->commands[0]. */
                BG_AnimScriptAnimation(&script->commands[commandIndex], ps,
                                       qtrue, qfalse, qtrue);
            }
        }
    }

weapon_dispatch:
    /* ---- weapon fire/reload/melee dispatch chain (0x3001499a..0x30014a79) ----------
     * The dispatch short-circuits: the first check that "handles" the frame returns.
     * delayExpired (the qboolean from PM_Weapon_WeaponTimeAdjust) threads through as
     * the argument to the finish/start callees that take it. */
    {
        /* 0x3001499a delayExpired = PM_Weapon_WeaponTimeAdjust(); */
        qboolean delayExpired = PM_Weapon_WeaponTimeAdjust();

        /* 0x300149a1..0x300149ab: unconditional per-frame updaters (no return check). */
        PM_Weapon_CheckForDeployBreakdown();        /* CALL 0x300138c0 */
        PM_Weapon_CheckForChangeWeapon();   /* CALL 0x30013d30 */
        PM_Weapon_CheckForReload();         /* CALL 0x300136d0 */

        /* 0x300149b0 PUSH ESI; CALL PM_Weapon_CheckForMelee(delayExpired). void. */
        PM_Weapon_CheckForMelee(delayExpired);

        /* 0x300149b6 PUSH ESI; CALL PM_Weapon_CheckForRechamber(delayExpired); if it
         * handled the frame (nonzero), stop. Both pushes cleaned by ADD ESP,8. */
        if (PM_Weapon_CheckForRechamber(delayExpired)) {
            return; /* JNZ 0x30014a7c */
        }

        /* 0x300149c7..0x30014a0a: pin ps->aimSpreadScale to the 255.0 max when the
         * player is moving-while-restricted or mid-melee. The write fires when:
         *   (flags & 0x1)     && (command.forwardmove || command.rightmove), OR
         *   (flags & sprint)  && (command.forwardmove || command.rightmove), OR
         *   weaponState == WEAPON_STATE_MELEE_WINDUP/RELAX. */
        move = pm;
        ps = move->ps;
        int forceMaxSpread = 0;
        if ((ps->playerStateFlags & 0x1u) &&
            (move->command.forwardmove != 0 || move->command.rightmove != 0)) {
            forceMaxSpread = 1; /* 0x300149d6/0x300149db JNZ 0x30014a02 */
        }
        if (!forceMaxSpread && (ps->playerStateFlags & PMF_SPRINTING) &&
            (move->command.forwardmove != 0 || move->command.rightmove != 0)) {
            forceMaxSpread = 1; /* 0x300149e8/0x300149ed JNZ 0x30014a02 */
        }
        if (!forceMaxSpread &&
            (ps->weaponState == WEAPON_STATE_MELEE_WINDUP ||
             ps->weaponState == WEAPON_STATE_MELEE_RELAX)) {
            forceMaxSpread = 1; /* 0x300149f8/0x300149fd */
        }
        if (forceMaxSpread) {
            /* 0x30014a02 MOV ps->aimSpreadScale,255.0f (0x437f0000). */
            ps->aimSpreadScale = PM_AIM_SPREAD_SCALE_MAX;
        }

        /* 0x30014a0c CMP delayExpired,0 / JNZ 0x30014a1c: when the weapon delay did
         * NOT expire this frame, and both weaponTime and weaponDelay are already 0,
         * the weapon is fully idle -> stop the chain. */
        if (delayExpired == 0) {
            /* 0x30014a10 MOV EAX,[EAX] (ps); CMP ps->weaponTime,0 / weaponDelay,0. */
            ps = move->ps;
            if (ps->weaponTime != 0 || ps->weaponDelay != 0) {
                return; /* JNZ 0x30014a7c */
            }
        }

        /* 0x30014a1c..0x30014a54: the finish-state chain. Each returns nonzero when it
         * committed a weapon-state finalize, ending the frame's weapon processing. */
        if (PM_Weapon_FinishReload(delayExpired)) {
            return; /* PUSH ESI; CALL 0x30013470; JNZ 0x30014a7c */
        }
        if (PM_Weapon_FinishMelee()) {
            return; /* CALL 0x300144c0; JNZ 0x30014a7c */
        }
        if (PM_Weapon_FinishWeaponChange()) {
            return; /* CALL 0x30012e70; JNZ 0x30014a7c */
        }
        if (PM_Weapon_FinishWeaponRaise()) {
            return; /* CALL 0x300131b0; JNZ 0x30014a7c */
        }
        if (PM_Weapon_FinishWeaponDeploy()) {
            return; /* CALL 0x30013200; JNZ 0x30014a7c */
        }
        if (PM_Weapon_FinishWeaponBreakdown()) {
            return; /* CALL 0x30013250; JNZ 0x30014a7c */
        }

        /* 0x30014a56 reload pm, 0x30014a5e CMP ps->currentWeapon(+0xd8),0
         * / JZ: if the player has no weapon, stop -- no finish-melee / start-firing. */
        move = pm;
        if (move->ps->currentWeapon == 0) {
            return; /* JZ 0x30014a7c */
        }

        /* 0x30014a66 PUSH ESI; CALL PM_Weapon_FinishFiring(delayExpired); if it
         * committed the firing-state finalize, stop. */
        if (PM_Weapon_FinishFiring(delayExpired)) {
            return; /* JNZ 0x30014a7c */
        }

        /* 0x30014a73 PUSH ESI; CALL PM_Weapon_FireWeapon(delayExpired). void tail. */
        PM_Weapon_FireWeapon(delayExpired);
    }
    /* 0x30014a7c POP EBX; POP ESI; POP EDI; RET. */
}
