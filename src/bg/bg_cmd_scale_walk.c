// Sources: uo_cgame_mp_x86.dll 0x30008770..0x300089f0,
//          uo_game_mp_x86.dll  0x20008520..0x200087a0,
//          game.mp.uo.i386.so  RVA 0x00023af5..0x00023f62
//
// PM_CmdScale_Walk — the WALK-specific movement command-speed scaler, distinct from
// PM_CmdScale (0x30008690). PM_WalkMove (0x300091e0) is the sole caller: it copies
// pm->command into a stack scratch usercmd window and passes a pointer to
// it. This routine returns the fraction of full walk speed the current command asks
// for, after applying the per-axis (forward/back/strafe) walk scales, the stance
// (walk/run/sprint/prone/crouch) scale, the pmType free-move multipliers, the water-
// depth slowdown, the per-weapon walk scale, and the aim/zoom-walk reduction. Result
// is returned as a live x87 register value; the caller stores it to binary32 and
// cleans up with ADD ESP,4.
//
// NAMING: the .mcode header's size-only "CG_CalcVehicleViewValues" guess is REJECTED
// (that is a vehicle-view function in the 0x30040xxx band; this is 0x30008xxx pmove).
// The name PM_CmdScale_Walk is retained by the supporting Mac symbol bank and is
// re-derived here from the bytes: __cdecl, one pushed command-window pointer, reads
// command forwardmove(+0x14)/rightmove(+0x15), scales by ps->backSpeedScale
// (+0x59c) / ps->strafeSpeedScale (+0x598), and calls PM_GetViewHeightLerp — exactly
// the WALK member of the same-module PPC pmove cluster (PM_CmdScale_Walk + PM_CmdScale).
//
// Structure access: pm (0x30539850) is the BG pmove context; ps =
// pm->ps is the playerState. bg_weaponInfos[] is 0x30134cd8.
//
// Exact constants (objdump -s -j .rdata verified):
//   0x3007bce0 : float 1.0f          (water/lerp base, 1.0 - x forms)
//   0x3007bce8 : float 0.5f          (water-depth slowdown coefficient)
//   0x3007bcec : float 0.0f          (shared .rdata zero, sign/threshold compares)
//   0x3007be60 : float 127.0f        (movement-byte full-scale denominator)
//   0x3007be5c : float 3.0f          (pmType 2 spectator multiplier)
//   0x3007bddc : float 6.0f          (pmType 3 noclip multiplier)
//   0x3007bf80 : float 0.33333334f   (1/3; water-depth slowdown, waterlevel/6)
//   0x3007162c : float 0.4f          (aim/zoom-walk speed reduction)

#include "bg_pmove.h"

#include "bg_weapon.h"
#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"

#if defined(LINUX_BEHAVIOR)
#include "compat/libm/coduo_libm.h"
#endif

#include <math.h>
#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/* Instruction-provenance constants (all proven from .rdata bytes at the cited
 * addresses; see the FMUL/FLD sites in the .mcode). */
#define PM_WALK_MOVE_FULLSCALE 127.0f /* .rdata 0x3007be60 */
#define PM_WALK_WATER_THIRD 0.33333334f /* .rdata 0x3007bf80 == 1/3 */
#define PM_WALK_WATER_HALF 0.5f   /* .rdata 0x3007bce8 */
#define PM_WALK_AIM_SLOWDOWN 0.4f   /* .rdata 0x3007162c */

/*
 * The current viewheight (ps->viewHeightTarget / viewHeightTarget, +0xf4)
 * matched against the prone/crouched viewheight values selects the shared
 * effective stance when no viewheight lerp is in flight
 * (0x300088b2..0x300088d6 EBX selector).
 */
#if defined(WINDOWS_BEHAVIOR)
float PM_CmdScale_Walk(const usercmd_t *cmd)
{
    int32_t forwardmove = cmd->forwardmove; /* +0x14 signed */

    /*
     * Per-axis walk magnitudes (0x30008779..0x300087d9). Forward-move is scaled by
     * backSpeedScale only when moving backward (forwardmove < 0); otherwise it enters
     * the magnitude raw. Right-move is always scaled by strafeSpeedScale. The move
     * magnitude fed to the base-scale formula is the larger of the two absolute
     * per-axis terms.
     */
    /* forwardTerm is kept in st0 (FILD [;FMUL backSpeedScale]; FABS at
     * 0x3000878f..0x300087aa) and consumed by the FCOM at 0x300087c8 without a
     * store, so it is long double; the movement byte enters via bare FILD (an
     * explicit (float) cast would round, Class 4). strafeTerm IS stored (FSTP
     * [ESP+8] at 0x300087c4), so it stays float. moveMag = max(forwardTerm,
     * strafeTerm) is kept in st0 and later multiplied by speed (FMUL ST1 at
     * 0x30008803) unstored, so it too is long double. */
    long double forwardTerm;
    if (forwardmove < 0) {
        /* The negative-forward arm alone performs this first move/ps load. */
        pmove_t *backScalePm = pm;       /* 0x30008780 */
        playerState_t *backScalePs = backScalePm->ps;
        forwardTerm = (long double)forwardmove * (long double)backScalePs->backSpeedScale;
    } else {
        forwardTerm = forwardmove;
    }
    int32_t rightmove = cmd->rightmove; /* 0x300087a6 fresh signed-byte load */
    forwardTerm = fabsl(forwardTerm);

    /* 0x300087ac/0x300087b2 reload the global move pointer and its ps for all
     * remaining player-state accesses, including after the negative arm above. */
    pmove_t *move = pm;
    playerState_t *ps = move->ps;
    float strafeTerm = fabsf(rightmove * ps->strafeSpeedScale);

    long double moveMag = (forwardTerm > strafeTerm) ? forwardTerm : strafeTerm;

    /* No movement at all: return 0.0f (0x300087d9..0x300087f7 FLD 0.0f; RET). */
    if (moveMag == 0.0f) {
        return 0.0f;
    }

    /*
     * Base command scale = speed * moveMag / (sqrt(fwd^2 + rt^2) * 127)
     * (0x300087fc..0x30008830). Only the forward/right components form the
     * sum-of-squares here (no upmove, unlike PM_CmdScale). speed is FILD'd as a
     * signed int; the sum of squares is FILD'd as a signed int.
     */
    int32_t squaredForwardmove = cmd->forwardmove; /* 0x300087f8 MOVSX */
    int32_t squaredRightmove = cmd->rightmove;     /* 0x300087ff MOVSX */
    int32_t total = (squaredForwardmove * squaredForwardmove) + (squaredRightmove * squaredRightmove);
    uint32_t flags = ps->playerStateFlags;    /* [ps+0xc], captured for the sign/mask tests below */

    /* speed and total enter via bare FILD (0x300087fc/0x3000881a) and FSQRT
     * (0x30008822) is not stored (FSTP ST2 is register-to-register), so the base
     * scale is one 80-bit chain: (float) casts and sqrtf would round where the DLL
     * does not. scale is carried long double through the stance select below, then
     * rounded ONCE at the FSTP [ESP+8] (see the (float) cast after the stance
     * block); the chain is finally returned raw (RET at 0x300089ef) and every
     * caller FSTPs it. */
    long double scale = ((long double)ps->speed * moveMag) / (coduo_x87_sqrtl((long double)total) * PM_WALK_MOVE_FULLSCALE);

    /*
     * Stance scale selection (0x30008830..0x30008883). Force the walk scale when the
     * walk flag is set, or the player is leaning, or a "held" weapon (animState 0xd,
     * weaponClass 3) is active. Otherwise sprint or run.
     */
    qboolean forceWalk = (flags & PMF_WALKING) != 0;
    if (!forceWalk && ps->leanFraction != 0.0f) {
        forceWalk = qtrue;
    }
    if (!forceWalk && ps->weaponState == WEAPON_STATE_BREAKING_DOWN) {
        const weaponInfo_t *wi = BG_GetInfoForWeapon(ps->currentWeapon);
        if (wi->weaponClass == WEAPCLASS_LMG) {
            forceWalk = qtrue;
        }
    }

    if (forceWalk) {
        scale *= ps->walkSpeedScale;   /* [ps+0x584] */
    } else if (flags & PMF_SPRINTING) {
        scale *= ps->sprintSpeedScale; /* [ps+0x58c] */
    } else {
        scale *= ps->runSpeedScale;    /* [ps+0x588] */
    }

    /* 0x30008886 FSTP float ptr [ESP+8]: the base*stance chain is rounded to float
     * and stored here, then reloaded (FLD [ESP+8]/[ESP+0x10]) by every branch
     * below. Model that store+reload so the remaining multiplies run on the rounded
     * value, while staying long double afterwards (they are not stored again until
     * the raw return). */
    scale = (float)scale;

    /*
     * Free-move pmTypes get an extra fixed multiplier (0x30008883..0x300088ad). The
     * two comparisons are sequential and mutually exclusive; either one, when it
     * fires, jumps straight past the stance-lerp/water/weapon blending to the
     * aim-slowdown tail (0x300089ab), so the blending below applies only to ordinary
     * movement pmTypes.
     */
    int32_t pmType = ps->pmType; /* 0x30008883: one load feeds both CMPs. */
    if (pmType == PM_TYPE_NOCLIP) {
        scale *= 3.0f;   /* .rdata 0x3007be5c */
    } else if (pmType == PM_TYPE_UFO) {
        scale *= 6.0f;   /* .rdata 0x3007bddc */
    } else {
        /*
         * Prone<->crouch viewheight blending (0x300088b2..0x30008972). Determine the
         * current stance from the current viewheight, then either blend the prone and
         * crouched stance scales by the active viewheight lerp, or, if no lerp is in
         * flight, apply the flat stance scale.
         */
        int32_t viewHeightTarget = ps->viewHeightTarget;              /* +0xf4 viewHeightTarget */
        effectiveStance_t stance = EFFECTIVE_STANCE_STAND;
        if (viewHeightTarget == ps->crouchViewHeight) {       /* 0x300088b8 */
            stance = EFFECTIVE_STANCE_CROUCH;
        } else if (viewHeightTarget == ps->proneViewHeight) { /* 0x300088c9 */
            stance = EFFECTIVE_STANCE_PRONE;
        }

        /* The two call arguments are independent reloads after stance selection. */
        int32_t proneViewheight = ps->proneViewHeight;       /* 0x300088d6 */
        int32_t crouchedViewheight = ps->crouchViewHeight;   /* 0x300088dc */

        /* First probe the crouched->prone transition (from=crouched, to=prone). */
        long double lerp = PM_GetViewHeightLerp(crouchedViewheight, proneViewheight);
        if (lerp != 0.0f) {
            /* Active crouch->prone lerp: blend crouched into prone by lerp. */
            scale *= (lerp * ps->proneSpeedScale) + ((1.0f - lerp) * ps->crouchSpeedScale);
        } else {
            /* Then probe the prone->crouch transition (from=prone, to=crouched). */
            lerp = PM_GetViewHeightLerp(proneViewheight, crouchedViewheight);
            if (lerp != 0.0f) {
                /* Active prone->crouch lerp: blend prone into crouched by lerp. */
                scale *= (lerp * ps->crouchSpeedScale) + ((1.0f - lerp) * ps->proneSpeedScale);
            } else if (stance == EFFECTIVE_STANCE_PRONE) {
                scale *= ps->proneSpeedScale;    /* [ps+0x590] */
            } else if (stance == EFFECTIVE_STANCE_CROUCH) {
                scale *= ps->crouchSpeedScale; /* [ps+0x594] */
            }
            /* stance NONE with no lerp: leave scale unchanged. */
        }

        /*
         * Water-depth slowdown (0x30008972..0x300089a5). When submerged, reduce the
         * scale by half of a third of the waterlevel: scale *= 1 - waterlevel/6.
         * waterlevel 3 (head) halves the walk speed.
         */
        pmove_t *waterPm = pm; /* 0x30008972 reloads the global. */
        uint8_t waterlevel = waterPm->waterlevel;
        if (waterlevel != 0) {
            /* waterlevel enters via bare FILD (0x3000898f) -- no (float) cast. */
            scale *= 1.0L - (long double)waterlevel * (long double)PM_WALK_WATER_THIRD * (long double)PM_WALK_WATER_HALF;
        }
    }

    /*
     * Per-weapon walk scale (0x300089ab..0x300089de). Apply the current weapon's
     * moveSpeedScale when a weapon is equipped (currentWeapon != 0), that scale is
     * positive, and the player is not sprinting.
     */
    int32_t currentWeapon = ps->currentWeapon;          /* 0x300089ab */
    const weaponInfo_t *wi = BG_GetInfoForWeapon(currentWeapon);
    if (currentWeapon != 0) {
        float moveSpeedScaleForCompare = wi->moveSpeedScale;
        if (moveSpeedScaleForCompare > 0.0f && (flags & PMF_SPRINTING) == 0) {
            float moveSpeedScaleForMultiply = wi->moveSpeedScale; /* 0x300089d8 reload */
            scale *= moveSpeedScaleForMultiply;
        }
    }

    /*
     * Aim/zoom-walk reduction (0x300089de..0x300089ef). When the aim button is held
     * in the command window, walk at 40% speed.
     */
    if (cmd->wbuttons & PM_WBUTTON_WALK) {   /* [cmd+0x5] bit 0x4 */
        scale *= PM_WALK_AIM_SLOWDOWN;
    }

    return (float)scale;
}
#else
/* Linux stores each named magnitude, scale, lerp, water, weapon, and aim
 * result as binary32.  Its exported pm_waterSwimScale and
 * pm_shellshockScale constants contain the same 0.5f and 0.4f values folded
 * into both Windows DLLs. */
float PM_CmdScale_Walk(const usercmd_t *command)
{
    const playerState_t *ps = pm->ps;
    int32_t forwardMove = command->forwardmove;
    const int32_t rightMove = command->rightmove;
    float forwardMagnitude;
    float strafeMagnitude;
    float moveMagnitude;
    float length;
    float scale;
    uint32_t flags;

    if (forwardMove < 0) {
        forwardMagnitude = fabsf((float)((long double)forwardMove * (long double)ps->backSpeedScale));
    } else {
        forwardMagnitude = fabsf((float)forwardMove);
    }
    strafeMagnitude = fabsf((float)((long double)rightMove * (long double)ps->strafeSpeedScale));
    moveMagnitude = strafeMagnitude;
    if (moveMagnitude < forwardMagnitude) {
        moveMagnitude = forwardMagnitude;
    }
    if (moveMagnitude == 0.0f) {
        return 0.0f;
    }

    length = (float)CoduoLibm_SqrtGlibc((double)(forwardMove * forwardMove + rightMove * rightMove));
#if EMULATE_X87
    scale = x87f_store_f32(x87f_div(x87f_mul(x87f_load_i32(ps->speed), x87f_load_f32(moveMagnitude)),
                                    x87f_mul(x87f_load_f32(length), x87f_load_f32(PM_WALK_MOVE_FULLSCALE))));
#else
    scale = (float)(((long double)ps->speed * (long double)moveMagnitude) / ((long double)length * (long double)PM_WALK_MOVE_FULLSCALE));
#endif

    flags = ps->playerStateFlags;
    if ((flags & PMF_WALKING) != 0 || ps->leanFraction != 0.0f ||
        (ps->weaponState == WEAPON_STATE_BREAKING_DOWN && BG_GetInfoForWeapon(ps->currentWeapon)->weaponClass == WEAPCLASS_LMG)) {
        scale = (float)((long double)scale * (long double)ps->walkSpeedScale);
    } else if ((flags & PMF_SPRINTING) != 0) {
        scale = (float)((long double)scale * (long double)ps->sprintSpeedScale);
    } else {
        scale = (float)((long double)scale * (long double)ps->runSpeedScale);
    }

    if (ps->pmType == PM_TYPE_NOCLIP) {
        scale = (float)((long double)scale * 3.0L);
    } else if (ps->pmType == PM_TYPE_UFO) {
        scale = (float)((long double)scale * 6.0L);
    } else {
        const effectiveStance_t stance = PM_GetEffectiveStance(ps);
        float lerp = (float)PM_GetViewHeightLerp(ps->crouchViewHeight, ps->proneViewHeight);

        if (lerp != 0.0f) {
#if EMULATE_X87
            scale = x87f_store_f32(
                x87f_mul(x87f_load_f32(scale),
                         x87f_add(x87f_mul(x87f_sub(x87f_load_f32(1.0f), x87f_load_f32(lerp)), x87f_load_f32(ps->crouchSpeedScale)),
                                  x87f_mul(x87f_load_f32(lerp), x87f_load_f32(ps->proneSpeedScale)))));
#else
            scale = (float)((long double)scale * ((1.0L - (long double)lerp) * (long double)ps->crouchSpeedScale +
                                                  (long double)lerp * (long double)ps->proneSpeedScale));
#endif
        } else {
            lerp = (float)PM_GetViewHeightLerp(ps->proneViewHeight, ps->crouchViewHeight);
            if (lerp != 0.0f) {
#if EMULATE_X87
                scale = x87f_store_f32(
                    x87f_mul(x87f_load_f32(scale),
                             x87f_add(x87f_mul(x87f_sub(x87f_load_f32(1.0f), x87f_load_f32(lerp)), x87f_load_f32(ps->proneSpeedScale)),
                                      x87f_mul(x87f_load_f32(lerp), x87f_load_f32(ps->crouchSpeedScale)))));
#else
                scale = (float)((long double)scale * ((1.0L - (long double)lerp) * (long double)ps->proneSpeedScale +
                                                      (long double)lerp * (long double)ps->crouchSpeedScale));
#endif
            } else if (stance == EFFECTIVE_STANCE_PRONE) {
                scale = (float)((long double)scale * (long double)ps->proneSpeedScale);
            } else if (stance == EFFECTIVE_STANCE_CROUCH) {
                scale = (float)((long double)scale * (long double)ps->crouchSpeedScale);
            }
        }

        if (pm->waterlevel != 0) {
            float waterFraction;
            float waterScale;
#if EMULATE_X87
            waterFraction = x87f_store_f32(x87f_div(x87f_load_i32(pm->waterlevel), x87f_load_f32(3.0f)));
            waterScale =
                x87f_store_f32(x87f_sub(x87f_load_f32(1.0f), x87f_mul(x87f_sub(x87f_load_f32(1.0f), x87f_load_f32(PM_WALK_WATER_HALF)),
                                                                      x87f_load_f32(waterFraction))));
            scale = x87f_store_f32(x87f_mul(x87f_load_f32(scale), x87f_load_f32(waterScale)));
#else
            waterFraction = (float)((long double)pm->waterlevel / 3.0L);
            waterScale = (float)(1.0L - (1.0L - (long double)PM_WALK_WATER_HALF) * (long double)waterFraction);
            scale = (float)((long double)scale * (long double)waterScale);
#endif
        }
    }

    {
        const int32_t weapon = ps->currentWeapon;
        const weaponInfo_t *weaponInfo = BG_GetInfoForWeapon(weapon);
        if (weapon != 0 && weaponInfo->moveSpeedScale > 0.0f && (flags & PMF_SPRINTING) == 0) {
            scale = (float)((long double)scale * (long double)weaponInfo->moveSpeedScale);
        }
    }

    if ((command->wbuttons & PM_WBUTTON_WALK) != 0) {
        scale = (float)((long double)scale * (long double)PM_WALK_AIM_SLOWDOWN);
    }
    return scale;
}
#endif
