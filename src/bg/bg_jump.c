#include "bg_pmove.h"
#include "bg_weapon.h"

#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <stddef.h>
#include <stdint.h>

// Sources: uo_cgame_mp_x86.dll 0x30008d00..0x30008f1e,
//          uo_game_mp_x86.dll  0x20008ab0..0x20008cce,
//          game.mp.uo.i386.so  RVA 0x00024483..0x00024837
//
// PM_CheckJump — the pmove ground-jump check. Returns qboolean (EAX): 0 when the
// player did NOT jump this frame (any eligibility gate failed), 1 when the player
// jumped. Runs on the shared global pmove context pm (0x30539850) and its
// playerState pm->ps.
//
// NAME ADJUDICATION: the .mcode size-guess "CG_DrawCrosshairNames" (win 0x21f matched
// to 0x220) is REJECTED per the no-size-match rule — the 0x30008xxx band is pmove, the
// real crosshair-names draw is 0x3001a610, and this body performs jump physics, not a
// HUD draw. The name PM_CheckJump was already adopted (provisional decl in
// client_recovered.h) from the same-module cgame_mp.dll PPC bank; these bytes confirm
// it: a ground/duck/prone gate followed by PM_Jump(39.0f), a jump event, aim-spread,
// and a random jump animation script.
//
// ABI: no source arguments; qboolean return in EAX. The two SUB ESP,0x18 / ADD ESP,0x18
// frames are the local vector scratch used by the surface-redirect block.
//
// -------------------------------------------------------------------------------------
// GATE (0x30008d00..0x30008d6c) — every failure path returns 0.
//   ECX = move->command.commandTime (pm->command.commandTime, +0x04) - ps->lastJumpCommandTime (+0x68)
//   CMP ECX,0x1f4 / JL fail        : must have been off/leaving the ground >= 500 ms.
//   TEST DH,0x8   (flags & 0x800)  : PMF_RESPAWNED set -> cannot jump.
//   ps->viewHeightTarget (+0xf4) == ps->crouchViewHeight (+0x578)  -> fail  (duck/prone viewheight;
//   ps->viewHeightTarget (+0xf4) == ps->proneViewHeight (+0x574)  -> fail   both mean crouched/prone)
//   if (flags & 0x20 / PMF_ADS):
//       bg_weaponInfos[ps->currentWeapon]->weaponClass == 3 (WEAPCLASS_LMG)
//         -> fail (mounted/spread weapon blocks jumping)
//   CMP byte [move+0x1a],0xa / JL fail : move->command.upmove < 10 -> not settled -> fail.
//   if (!(flags & 0x8 / PMF_JUMP_HELD))  -> JUMP (fall to 0x30008d6d).
//   else  move->command.upmove = 0; return 0.       (jump-held latch already consumed)
//
// LAUNCH AND SURFACE REDIRECT (0x30008d6d..0x30008e63):
//   PM_Jump(39.0f)   (0x421c0000)                    : always perform the launch.
// Only when the reloaded ps->playerStateFlags & PMF_LADDER (0x10) is set (a
// mantle/ladder surface contact recorded elsewhere):
//   ps->velocity[2] *= 0.75f  (0x3007be38)           : damp the vertical launch.
//   nf = normalize( { pml.forward[0], pml.forward[1], 0 } )  (horizontal forward).
//   dot = pml.forward . ps->ladderNormal (+0x5c)     : RAW forward vs surface normal.
//   if (dot < 0)  h = nf - 2*(nf . ladderNormal)*ladderNormal  (reflect off surface),
//                 then normalize(h);
//   else          h = nf;                            : forward already leaves the surface.
//   ps->velocity[0] = h[0] * 128.0f;  ps->velocity[1] = h[1] * 128.0f  (0x3007c154).
//   ps->playerStateFlags &= ~PMF_LADDER.                 : consume the surface-contact flag.
// (When PMF_LADDER is clear only the redirect is skipped; the plain-ground jump
// has already been launched by PM_Jump.)
//
// TAIL (0x30008e64..0x30008f1e) — always runs on the jump path:
//   PM_AddEvent( PM_JumpForSurface() )           : post the jump animation event.
//   ps->aimSpreadScale (+0x628) += 64.0f, clamped up to 255.0f (0x3007bd64 / 0x437f0000).
//   Play a random jump animation script (same idiom as PM_GroundTrace's PM_PlayLiftAnim):
//     list = *(0x30134cc8) + (move->command.forwardmove < 0 ? 0x20294 : 0x20090);
//     gated on ps->pmType (+0x04) < 6 and list->count != 0;
//     script = BG_FirstValidItem(ps->psClientNum, list); require non-NULL and
//     script->commandCount != 0; idx = rand() % commandCount;
//     BG_AnimScriptAnimation(&script->commands[idx], ps, setTimer=1, allowContinue=0,
//                            checkDuration=1).
//   return 1.
//
// Constants dumped from .rdata (objdump -s -j .rdata):
//   0x3007be38 = 0.75f    0x3007c150 = -2.0f    0x3007c154 = 128.0f
//   0x3007c000 = 64.0f    0x3007bd64 = 255.0f   0x3007bcec = 0.0f   0x421c0000 = 39.0f

/* Jump launch height passed to PM_Jump (caller-pushed 0x421c0000). */
#define PM_CHECKJUMP_JUMP_HEIGHT 39.0f

/* >= this many ms must elapse since the ground-leave was recorded (ps->lastJumpCommandTime)
 * before a jump is allowed. 0x30008d13 CMP ECX,0x1f4. */
#define PM_CHECKJUMP_GROUND_DELAY 500

/* move->command.upmove must be settled (>= 10) for a jump. 0x30008d56 CMP byte,0xa. */
#define PM_CHECKJUMP_PMOVESTATE_MIN 10

/* Vertical-velocity damping applied on the surface-redirect launch (0x3007be38). */
#define PM_CHECKJUMP_VELZ_SCALE 0.75f

/* Reflection overbounce for the surface redirect: h = nf - 2*(nf.n)*n (0x3007c150). */
#define PM_CHECKJUMP_REFLECT_SCALE -2.0f

/* Horizontal launch speed applied to the (reflected) forward direction (0x3007c154). */
#define PM_CHECKJUMP_HSPEED 128.0f

/* Aim-spread penalty added on a jump, clamped to the 255 ceiling (0x3007c000/0x3007bd64). */
#define PM_CHECKJUMP_SPREAD_ADD 64.0f
#define PM_CHECKJUMP_SPREAD_MAX 255.0f

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

qboolean PM_CheckJump(void)
{
    pmove_t *move = pm;
    playerState_t *ps = move->ps;

    /* --- eligibility gate (0x30008d0d..0x30008d6c). Any failure -> return 0. --- */

    /* 0x30008d10 SUB ECX,[EAX+0x68] / CMP ECX,0x1f4 / JL: need >= 500 ms since the
     * ground-leave was recorded. */
    int32_t timeSinceGroundLeave = coduo_int32_from_bits((uint32_t)move->command.commandTime - (uint32_t)ps->lastJumpCommandTime);
    if (timeSinceGroundLeave < PM_CHECKJUMP_GROUND_DELAY) {
        return qfalse;
    }

    uint32_t flags = ps->playerStateFlags;

    /* 0x30008d1e TEST DH,0x8 / JNZ: PMF_RESPAWNED blocks the jump. */
    if ((flags & PMF_RESPAWNED) != 0) {
        return qfalse;
    }

    /* Windows inlines the two comparisons from PM_GetEffectiveStance; Linux
     * calls the retained function at RVA 0x000233e1. */
    if (PM_GetEffectiveStance(ps) != EFFECTIVE_STANCE_STAND) {
        return qfalse;
    }

    /* 0x30008d39 TEST DL,0x20 / JZ: when the QUALIFY flag is set, a mounted/spread
     * weapon (weaponClass == WEAPCLASS_LMG) blocks jumping. */
    if ((flags & PMF_ADS) != 0) {
        if (BG_GetInfoForWeapon(ps->currentWeapon)->weaponClass == WEAPCLASS_LMG) {
            return qfalse;
        }
    }

    /* 0x30008d56 CMP byte [move+0x1a],0xa / JL: pmove state must be settled. */
    if (move->command.upmove < PM_CHECKJUMP_PMOVESTATE_MIN) {
        return qfalse;
    }

    /* 0x30008d5c TEST DL,0x8 / JZ 0x30008d6d: PMF_JUMP_HELD clear -> proceed to
     * jump; set -> the jump-held latch is already consumed, reset command.upmove, no jump. */
    if ((flags & PMF_JUMP_HELD) != 0) {
        move->command.upmove = 0;
        return qfalse;
    }

    /* 0x30008d6d calls PM_Jump for every accepted jump. The flag tested after
     * the call controls only the ladder/surface redirect below. */
    PM_Jump(PM_CHECKJUMP_JUMP_HEIGHT);

    /* --- surface redirect (0x30008d77..0x30008e63). --- */
    playerState_t *jumpPs = move->ps; /* 0x30008d77 reload after PM_Jump. */
    if ((jumpPs->playerStateFlags & PMF_LADDER) != 0) {

        /* 0x30008d88 FLD [EAX+0x28] / FMUL 0.75 / FSTP: damp the vertical launch. */
        jumpPs->velocity[2] = (float)((long double)jumpPs->velocity[2] * PM_CHECKJUMP_VELZ_SCALE);

        /* 0x30008d92..0x30008dba: horizontal forward direction, flattened and unit
         * normalized (the length is discarded). */
        vec3_t forward;
        forward[0] = pml.forward[0];
        forward[1] = pml.forward[1];
        forward[2] = 0.0f;
        (void)VectorNormalize(forward);

        playerState_t *surfacePs = move->ps; /* 0x30008dbc reload after normalize. */
        qboolean movingIntoSurface;

        /* Both targets compare this dot without a binary32 spill.  Windows
         * folds X,Z,Y; Linux folds X,Y,Z. */
#if defined(WINDOWS_BEHAVIOR)
#if EMULATE_X87
        {
            const x87f rawDot = x87f_add(x87f_add(x87f_mul(x87f_load_f32(pml.forward[0]), x87f_load_f32(surfacePs->ladderNormal[0])),
                                                  x87f_mul(x87f_load_f32(pml.forward[2]), x87f_load_f32(surfacePs->ladderNormal[2]))),
                                         x87f_mul(x87f_load_f32(pml.forward[1]), x87f_load_f32(surfacePs->ladderNormal[1])));
            movingIntoSurface = x87f_lt(rawDot, x87f_load_f32(0.0f)) ? qtrue : qfalse;
        }
#else
        {
            const long double rawDot = ((long double)pml.forward[0] * (long double)surfacePs->ladderNormal[0] +
                                        (long double)pml.forward[2] * (long double)surfacePs->ladderNormal[2]) +
                                       (long double)pml.forward[1] * (long double)surfacePs->ladderNormal[1];
            movingIntoSurface = rawDot < 0.0L ? qtrue : qfalse;
        }
#endif
#else
#if EMULATE_X87
        {
            const x87f rawDot = x87f_add(x87f_add(x87f_mul(x87f_load_f32(pml.forward[0]), x87f_load_f32(surfacePs->ladderNormal[0])),
                                                  x87f_mul(x87f_load_f32(pml.forward[1]), x87f_load_f32(surfacePs->ladderNormal[1]))),
                                         x87f_mul(x87f_load_f32(pml.forward[2]), x87f_load_f32(surfacePs->ladderNormal[2])));
            movingIntoSurface = x87f_lt(rawDot, x87f_load_f32(0.0f)) ? qtrue : qfalse;
        }
#else
        {
            const long double rawDot = ((long double)pml.forward[0] * (long double)surfacePs->ladderNormal[0] +
                                        (long double)pml.forward[1] * (long double)surfacePs->ladderNormal[1]) +
                                       (long double)pml.forward[2] * (long double)surfacePs->ladderNormal[2];
            movingIntoSurface = rawDot < 0.0L ? qtrue : qfalse;
        }
#endif
#endif

        float h0, h1;
        if (movingIntoSurface != qfalse) {
            /* 0x30008dee..0x30008e30: reflect the NORMALIZED forward off the surface,
             * h = forward - 2*(forward . ladderNormal)*ladderNormal, then normalize.
             * nDot is consumed by FMUL -2.0f (0x30008e07) and d is kept in st0 and
             * duplicated with FLD ST0 (0x30008e0d/0x30008e1a) -- neither is stored to
             * a float slot, so both are carried long double. The reflected[] stores
             * (FSTP at 0x30008e16/0x30008e1c/0x30008e20) are the only roundings. */
            vec3_t reflected;
#if defined(WINDOWS_BEHAVIOR)
#if EMULATE_X87
            {
                const x87f nDot = x87f_add(x87f_add(x87f_mul(x87f_load_f32(forward[0]), x87f_load_f32(surfacePs->ladderNormal[0])),
                                                    x87f_mul(x87f_load_f32(forward[2]), x87f_load_f32(surfacePs->ladderNormal[2]))),
                                           x87f_mul(x87f_load_f32(forward[1]), x87f_load_f32(surfacePs->ladderNormal[1])));
                const x87f redirect = x87f_mul(x87f_load_f32(PM_CHECKJUMP_REFLECT_SCALE), nDot);
                for (int32_t lane = 0; lane < 3; ++lane) {
                    reflected[lane] = x87f_store_f32(
                        x87f_add(x87f_load_f32(forward[lane]), x87f_mul(redirect, x87f_load_f32(surfacePs->ladderNormal[lane]))));
                }
            }
#else
            {
                const long double nDot = ((long double)forward[0] * (long double)surfacePs->ladderNormal[0] +
                                          (long double)forward[2] * (long double)surfacePs->ladderNormal[2]) +
                                         (long double)forward[1] * (long double)surfacePs->ladderNormal[1];
                const long double redirect = (long double)PM_CHECKJUMP_REFLECT_SCALE * nDot;
                for (int32_t lane = 0; lane < 3; ++lane) {
                    reflected[lane] = (float)((long double)forward[lane] + redirect * (long double)surfacePs->ladderNormal[lane]);
                }
            }
#endif
#else
#if EMULATE_X87
            {
                const float nDot =
                    x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(forward[0]), x87f_load_f32(surfacePs->ladderNormal[0])),
                                                     x87f_mul(x87f_load_f32(forward[1]), x87f_load_f32(surfacePs->ladderNormal[1]))),
                                            x87f_mul(x87f_load_f32(forward[2]), x87f_load_f32(surfacePs->ladderNormal[2]))));
                for (int32_t lane = 0; lane < 3; ++lane) {
                    reflected[lane] = x87f_store_f32(x87f_add(
                        x87f_load_f32(forward[lane]), x87f_mul(x87f_mul(x87f_load_f32(nDot), x87f_load_f32(PM_CHECKJUMP_REFLECT_SCALE)),
                                                               x87f_load_f32(surfacePs->ladderNormal[lane]))));
                }
            }
#else
            {
                const float nDot = (float)(((long double)forward[0] * (long double)surfacePs->ladderNormal[0] +
                                            (long double)forward[1] * (long double)surfacePs->ladderNormal[1]) +
                                           (long double)forward[2] * (long double)surfacePs->ladderNormal[2]);
                for (int32_t lane = 0; lane < 3; ++lane) {
                    reflected[lane] = (float)((long double)forward[lane] + (long double)nDot * (long double)PM_CHECKJUMP_REFLECT_SCALE *
                                                                               (long double)surfacePs->ladderNormal[lane]);
                }
            }
#endif
#endif
            (void)VectorNormalize(reflected);

            h0 = reflected[0];
            h1 = reflected[1];
        } else {
            /* 0x30008e43: forward already leaves the surface — use it directly. */
            h0 = forward[0];
            h1 = forward[1];
        }

        /* 0x30008e49/0x30008e55 FMUL 128.0 / FSTP velocity[0], velocity[1]. */
        surfacePs->velocity[0] = (float)((long double)h0 * PM_CHECKJUMP_HSPEED);
        playerState_t *horizontalYPs = move->ps; /* 0x30008e52 reload. */
        horizontalYPs->velocity[1] = (float)((long double)h1 * PM_CHECKJUMP_HSPEED);

        /* 0x30008e60 AND [EAX+0xc],~0x10: consume the surface-contact flag. */
        playerState_t *flagPs = move->ps; /* 0x30008e5e reload. */
        flagPs->playerStateFlags &= ~(uint32_t)PMF_LADDER;
    }

    /* --- jump event + aim-spread + animation (0x30008e64..0x30008f11). --- */

    /* 0x30008e64 CALL 0x30008c30; MOV ECX,EAX; CALL 0x30008310:
     * PM_AddEvent(PM_JumpForSurface()). */
    PM_AddEvent(PM_JumpForSurface());

    /* 0x30008e70..0x30008ea2: aimSpreadScale += 64, clamped up to 255. */
    playerState_t *spreadPs = move->ps; /* 0x30008e70 reload after both event calls. */
    spreadPs->aimSpreadScale = (float)((long double)spreadPs->aimSpreadScale + PM_CHECKJUMP_SPREAD_ADD);
    playerState_t *spreadClampPs = move->ps; /* 0x30008e84 independent reload. */
    if (spreadClampPs->aimSpreadScale > PM_CHECKJUMP_SPREAD_MAX) {
        spreadClampPs->aimSpreadScale = PM_CHECKJUMP_SPREAD_MAX;
    }

    /* The Windows optimizer inlines this complete event helper at
     * 0x30008ea3/0x20008c53; Linux retains the call at RVA 0x000247f7. */
    BG_AnimScriptEvent(move->ps, move->command.forwardmove < 0 ? ANIM_EVENT_JUMP_BACK : ANIM_EVENT_JUMP, qfalse, qtrue);

    /* 0x30008f15 MOV EAX,1 / RET: the player jumped. */
    return qtrue;
}

/* Offsets this reconstruction depends on. pmove_t leads with a native pointer
 * (anim), so its member offsets only hold at the 32-bit target ABI — guard those checks
 * to no-op on 64-bit. playerState_t has
 * no leading pointer, so its asserts hold unconditionally. */
#define BG_JUMP_LAYOUT_ASSERT(name_, expression_) typedef char name_[(expression_) ? 1 : -1]

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
BG_JUMP_LAYOUT_ASSERT(bg_jump_pmove_current_command_time_offset, offsetof(pmove_t, command.commandTime) == 0x04);
BG_JUMP_LAYOUT_ASSERT(bg_jump_pmove_forward_move_offset, offsetof(pmove_t, command.forwardmove) == 0x18);
BG_JUMP_LAYOUT_ASSERT(bg_jump_pmove_up_move_offset, offsetof(pmove_t, command.upmove) == 0x1a);
#endif
BG_JUMP_LAYOUT_ASSERT(bg_jump_last_command_time_offset, offsetof(playerState_t, lastJumpCommandTime) == 0x68);
BG_JUMP_LAYOUT_ASSERT(bg_jump_view_height_target_offset, offsetof(playerState_t, viewHeightTarget) == 0xf4);
BG_JUMP_LAYOUT_ASSERT(bg_jump_prone_view_height_offset, offsetof(playerState_t, proneViewHeight) == 0x574);
BG_JUMP_LAYOUT_ASSERT(bg_jump_crouch_view_height_offset, offsetof(playerState_t, crouchViewHeight) == 0x578);
BG_JUMP_LAYOUT_ASSERT(bg_jump_ladder_normal_offset, offsetof(playerState_t, ladderNormal) == 0x5c);
BG_JUMP_LAYOUT_ASSERT(bg_jump_aim_spread_scale_offset, offsetof(playerState_t, aimSpreadScale) == 0x628);

#undef BG_JUMP_LAYOUT_ASSERT
