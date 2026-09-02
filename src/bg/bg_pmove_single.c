#include "bg_pmove.h"

#include "bg_pmove_services.h"
#include "bg_vehicle.h"
#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>   /* memset for the pmove-locals clear */

// Sources: uo_cgame_mp_x86.dll 0x3000e050..0x3000e720,
//          uo_game_mp_x86.dll  0x2000de00..0x2000e4d0,
//          game.mp.uo.i386.so  RVA 0x0002d530..0x0002e1c7.
// The Windows bodies are instruction-identical apart from relocated globals,
// callees, and the final module syscall number. Linux retains the same state
// machine; its final velocity-sanity expression has additional binary32 spill
// points, preserved below.
//
// NAME: PmoveSingle. The .mcode mechanical `# name G_CheckForCursorHints` is a
// SIZE-ONLY guess (win size 0x6d0 ~ corpus 0x6cf) and is REJECTED — this is neither
// a server function nor a cursor-hint routine. Proven identity:
//   * it publishes the incoming pmove_t* (arg0 at [ESP+0x18]) into the global
//     pm (0x30539850), the pmove/anim context every pmove step reads
//     (0x3000e088: MOV [0x30539850],EBP);
//   * it snapshots the frame delta into pml.msec (0x305395a8, clamp 1..200) and
//     pml.frametime (0x305395a4 = pml.msec*0.001s) exactly as the Quake3 PmoveSingle
//     prologue, and snapshots pre-move origin/velocity into the pmove locals;
//   * it builds the movement basis via AngleVectors(ps->viewAngles, pml.forward,
//     pml.right, pml.up) (0x3004a200);
//   * it dispatches on ps->pmType through a 7-entry jump table (0x3000e720) into the
//     move / weapon / footstep / fatigue chains; every reconstructed callee is a
//     PM_*/BG_* pmove step (PM_DropTimers, PM_SetWaterLevel, PM_AirMove, PM_WalkMove,
//     PM_Weapon, PM_FoliageSounds, PM_UpdateFatigue, PM_UpdatePlayerSprintingFlag, ...).
// Its sole caller is Pmove (0x3000e740), once per movement substep.
//
// move == the incoming pmove_t (server pmove_t); ps == move->ps (the
// playerState it drives). Both are the established codebase struct names; field
// roles below come from their definitions in globals.h.
//
// x87 ordered-compare idiom `FCOMPP; FNSTSW AX; TEST AH,mask; Jcc`:
//   AH bit0=C0 (top-of-stack operand ST0 < ST1), bit2=C2 (unordered), bit6=C3 (equal).
//   The two setup magnitude tests do `TEST AH,5 (C0|C2)`; for ordered operands
//   (C2=0) that isolates C0. The tail velocity test uses `TEST AH,0x41 (C0|C3)`.

/* playerState_t.playerStateFlags and entityStateFlags bits used below now use
 * the shared movement names: PMF_BACKPEDAL, PMF_FOLLOW, PMF_PRONE_BLOCKED,
 * EF_ADS_HELD, and EF_FIRING. */

enum {
    PMOVE_FRAME_MSEC_MIN = 1,    /* pml.msec clamp floor (0x3000e2cb) */
    PMOVE_FRAME_MSEC_MAX = 200,  /* pml.msec clamp ceiling (0x3000e2e1, 0xc8) */
    PMOVE_STATE_LOCK_LIMIT = 10  /* command.upmove (+0x1a) < 10 clears ps->playerStateFlags bit 0x8 (0x3000e3a9) */
};

/* Exact .rdata constants (addresses dumped, not inferred from neighbours). */
#define PM_MS_TO_SEC 0.001f  /* 0x3007bd94 = 0x3a83126f */
#define PM_VEL_QUARTER 0.25f   /* 0x3007be58 = 0x3e800000 */
#define PM_ONE 1.0f    /* 0x3007bce0 = 0x3f800000 (float pool 1.0/2.0/0.5/0.0) */

/*
 * Provisional caller-observed callee decls for the pmove steps not yet
 * reconstructed. Each is invoked with NO stack arguments and reads pm /
 * pml.weaponInfo from the globals directly (the call sites do no PUSH and no
 * post-call cleanup), so the ABI is void(void). Their declarations now live in
 * the shared BG interface alongside this caller.
 */

/*
 * PM_ApplyMovementBasis (0x3000c8e0): the pmove-locals finalizer run once after the
 * frame-time setup. Two cdecl stack args (caller-cleaned, ADD ESP,8):
 *   arg0 = &move->command (LEA ESI,[EBP+0x4], the current usercmd command block base);
 *   arg1 = move->trace3   (the overhead trace callback at pmove_t +0x10c).
 * Caller-observed / provisional; exact source name and full types UNPROVEN. Its
 * declaration is centralized in the shared BG interface.
 */

void PmoveSingle(pmove_t *move)
{
    pmove_t *stepPm;
    playerState_t *ps;
    weaponInfo_t *wi;
    int32_t viewLerpMode = 0;   /* [ESP+0x20]: viewheight-lerp discriminant (0/1/2) */
    uint32_t pmType;

    /* 0x3000e05b: PUSH move; CALL 0x300035f0; ADD ESP,4. */
    BG_AnimUpdatePlayerStateConditions(move);

    /* 0x3000e061: ++pmoveCount. */
    c_pmove++;

    ps = move->ps;              /* 0x3000e073: ECX = move->ps */
    move->watertype = 0;         /* 0x3000e076: [move+0xf0] = 0 */
    move->waterlevel = 0;         /* 0x3000e07c: [move+0xf1] = 0 */
    uint32_t setupFlags = ps->playerStateFlags; /* MOV EAX,[ECX+0xc] */
    pm = move;        /* 0x3000e088: [0x30539850] = move */

    /* 0x3000e085: if (ps->playerStateFlags & 0x4000) clear the command/state
     * bytes (TEST AH,0x40 — AH holds flag bits 8..15). */
    if (setupFlags & PMF_FOLLOW) {
        uint8_t maskedWButtons = (uint8_t)(move->command.wbuttons & 0xc2u);
        move->command.buttons = 0;
        move->command.wbuttons = maskedWButtons;
        move->command.forwardmove = 0;
        move->command.rightmove = 0;
        move->command.upmove = 0;
    }

    /* 0x3000e0a6: if (move->command.buttons & 0x2) restrict it and clear the command bytes. */
    if (move->command.buttons & 0x2u) {
        move->command.buttons = (uint8_t)(move->command.buttons & 0x12u);
        move->command.wbuttons = (uint8_t)(move->command.wbuttons & 0xc2u);
        move->command.forwardmove = 0;
        move->command.rightmove = 0;
        move->command.upmove = 0;
    }

    /* 0x3000e0be: ps->playerStateFlags &= ~0x8000. */
    ps->playerStateFlags &= ~(uint32_t)PMF_PRONE_BLOCKED;

    /* 0x3000e0c5: if (ps->pmType >= 6) move->traceMask &= ~0x02000000. */
    ps = move->ps;
    if (ps->pmType >= PM_TYPE_DEAD)
        move->traceMask &= 0xfdffffffu;

    /* 0x3000e0db: pml.weaponInfo = bg_weaponInfos[ps->currentWeapon]. */
    pml.weaponInfo = bg_weaponInfos[ps->currentWeapon];

    /*
     * ---- command-consistency / weapon-finish block (0x3000e0f0..0x3000e1db) ----
     * dl = (uint8_t)ps->playerStateFlags (low byte). If ps->playerStateFlags bit 0x1 is clear, jump
     * straight to the "clear 0x400, then run the SPRINTING-finish + viewLerpMode
     * selection" path.
     */
    {
        ps = move->ps; /* 0x3000e0f0: reload after the weapon-info cache write. */
        uint32_t flags = ps->playerStateFlags;
        int takeFinishPath;   /* true -> 0x3000e159 finish-anim path; false -> 0x3000e1a1 */

        if (flags & PMF_PRONE) {
            /* 0x3000e0ff: compare current vs previous forward-move byte. */
            if (move->command.forwardmove == move->oldCommand.forwardmove) {
                takeFinishPath = 0;   /* JE -> block B (right-move test) */
            } else {
                /* 0x3000e109-0x3000e12a: FILD cur, FABS; FILD old, FABS; FCOMPP
                 * compares ST0=|old| against ST1=|cur| (C0=1 iff |old| < |cur|);
                 * TEST AH,0x5; JNP is taken iff exactly one of C0/C2 is set —
                 * for ordered byte inputs, iff |old| < |cur| (the magnitude
                 * INCREASED). Exact on integer bytes, so expressed as an
                 * integer compare. */
                int acur = (int)(int8_t)move->command.forwardmove;
                int aold = (int)(int8_t)move->oldCommand.forwardmove;
                if (acur < 0)
                    acur = -acur;
                if (aold < 0)
                    aold = -aold;
                if (aold < acur)
                    takeFinishPath = 1;   /* JNP -> 0x3000e159 finish path */
                else
                    takeFinishPath = 0;   /* fall to block B */
            }

            if (!takeFinishPath) {
                /* 0x3000e12c block B: right-move byte. */
                if (move->command.rightmove == move->oldCommand.rightmove) {
                    takeFinishPath = -1;  /* JE -> block E (0x3000e1a1) */
                } else {
                    /* 0x3000e136-0x3000e157: same FABS/FCOMPP setup; here the
                     * branch is JP (taken iff C0 CLEAR for ordered inputs, i.e.
                     * |old| >= |cur|) to block E; the fall-through when the
                     * magnitude increased goes to the finish path. */
                    int acur = (int)(int8_t)move->command.rightmove;
                    int aold = (int)(int8_t)move->oldCommand.rightmove;
                    if (acur < 0)
                        acur = -acur;
                    if (aold < 0)
                        aold = -aold;
                    if (aold >= acur)
                        takeFinishPath = -1;  /* JP -> block E (0x3000e1a1) */
                    else
                        takeFinishPath = 1;   /* fall to the finish path */
                }
            }
        } else {
            /* 0x3000e0f9: ps->playerStateFlags bit 0x1 clear -> 0x3000e1c1: clear 0x400, then
             * the SPRINTING-finish + viewLerpMode path (block F/G). */
            takeFinishPath = -2;
        }

        if (takeFinishPath == 1) {
            /* ---- finish-anim path (0x3000e159) ---- */
            if (PM_InteruptWeaponWithProneMove() != 0) {
                ps = move->ps;
                ps->playerStateFlags &= ~(uint32_t)PMF_PRONE_MOVEMENT_OVERRIDE;   /* &~0x400 */
                ps = move->ps;
                ps->playerStateFlags &= ~(uint32_t)PMF_ADS; /* &~0x20  */
            }
        } else if (takeFinishPath == -1) {
            /* ---- weapon-state path (0x3000e1a1) ---- */
            if (!(flags & PMF_ADS)) {   /* TEST DL,0x20 */
                uint32_t st = (uint32_t)ps->weaponState;
                if (st == 0 || st == 1 || st == 2 || st == 5)
                    ps->playerStateFlags &= ~(uint32_t)PMF_PRONE_MOVEMENT_OVERRIDE;  /* 0x3000e1c1: &~0x400 */
                /* else (0x3000e1bf JNE) fall through with 0x400 untouched */
            }
            /* both sub-branches converge to the SPRINTING-finish path below */
        } else if (takeFinishPath == -2) {
            /* 0x3000e1c1 reached from the no-attack case: clear 0x400. */
            ps->playerStateFlags &= ~(uint32_t)PMF_PRONE_MOVEMENT_OVERRIDE;
        }

        /* ---- shared SPRINTING-finish + viewLerpMode selection (0x3000e173/e175) ---- */
        ps = move->ps;                                        /* MOV ECX,[EBP] */
        if (ps->playerStateFlags & PMF_SPRINTING)          /* TEST [ps+0xc],0x10000 */
            PM_InteruptWeaponWithSprintMove();      /* 0x300121b0 */

        /* 0x3000e186: pick the viewheight-lerp discriminant. */
        ps = move->ps;                                        /* MOV EAX,[EBP] */
        if (ps->viewHeightTarget == ps->crouchViewHeight)      /* [ps+0xf4] == [ps+0x578] */
            viewLerpMode = 2;
        else                                        /* 0x3000e1ca */
            viewLerpMode = (ps->viewHeightTarget == ps->proneViewHeight) ? 1 : 0; /* SETZ vs [ps+0x574] */

        /* 0x3000e1db: if (ps->playerStateFlags & 0x20) && viewLerpMode == 1, clear command bytes. */
        if ((ps->playerStateFlags & PMF_ADS) && viewLerpMode == 1) {
            move->command.forwardmove = 0;
            move->command.rightmove = 0;
        }
    }

    /* 0x3000e1ee: if (ps->playerStateFlags & 0x20) and the current weapon's weaponInfo_t +0x80
     * field == 3, clear the command-move bytes. */
    ps = move->ps;                                            /* MOV EAX,[EBP] */
    if (ps->playerStateFlags & PMF_ADS) {
        wi = bg_weaponInfos[ps->currentWeapon];
        if (wi->weaponClass == WEAPCLASS_LMG) {   /* CMP [wi+0x80],3 */
            move->command.forwardmove = 0;
            move->command.rightmove = 0;
        }
    }

    /* 0x3000e215: +0x84 bit 0x20000 = (move->command.buttons & 0x2) ? set : clear. */
    uint8_t commandButtons = move->command.buttons;
    ps = move->ps;
    uint32_t entityFlags = ps->entityStateFlags;
    if (commandButtons & 0x2u)
        entityFlags |= (uint32_t)EF_ADS_HELD;
    else
        entityFlags &= ~(uint32_t)EF_ADS_HELD;
    ps->entityStateFlags = entityFlags;

    /* 0x3000e23b: always clear the +0x84 firing bit. */
    ps = move->ps;
    ps->entityStateFlags &= ~(uint32_t)EF_FIRING;

    /* 0x3000e245: firing-eligibility -> maybe re-set the +0x84 firing bit. */
    ps = move->ps;
    if (ps->pmType != PM_TYPE_INTERMISSION) {            /* CMP [ps+0x4],5; JZ skip */
        uint32_t firingFlags = ps->playerStateFlags;
        if (!(firingFlags & PMF_RESPAWNED)) {                       /* TEST DH,0x8 */
            uint32_t st = (uint32_t)ps->weaponState;
            if (st == 0 || st == 3) {                    /* [ps+0xdc] == 0 || == 3 */
                wi = bg_weaponInfos[ps->currentWeapon];
                if (ps->clips[wi->clipIndex] != 0) {  /* [ps + clipIndex*4 + 0x334] */
                    if (!(firingFlags & PMF_SPRINTING)) {               /* TEST 0x10000 */
                        if (move->command.buttons & 0x1u)               /* TEST [move+0x8],1 */
                            ps->entityStateFlags |= (uint32_t)EF_FIRING;
                    }
                }
            }
        }
    }

    /* 0x3000e29b: release the respawn attack latch when attack is no longer held. */
    ps = move->ps;
    if (ps->pmType < PM_TYPE_DEAD && !(move->command.buttons & 0x1u))
        ps->playerStateFlags &= ~(uint32_t)PMF_RESPAWNED;

    /* ---- pmove-locals prologue (0x3000e2b0) ---- */
    /* 0x3000e2b0: REP STOSD zeroes exactly 35 dwords (0x23, 140 bytes)
     * from 0x30539580 through 0x3053960b.  The now-shared pml_t expresses
     * that original single contiguous object directly. */
    memset(&pml, 0, sizeof(pml));

    /* pml.msec = clamp(move->command.commandTime - ps->commandTime, 1, 200); ps->commandTime = move->command.commandTime. */
    {
        ps = move->ps;
        int32_t delta = coduo_int32_from_bits((uint32_t)move->command.commandTime - (uint32_t)ps->commandTime);
        pml.msec = delta; /* MOV before clamps */
        if (delta < PMOVE_FRAME_MSEC_MIN)
            pml.msec = PMOVE_FRAME_MSEC_MIN;
        else if (delta > PMOVE_FRAME_MSEC_MAX)
            pml.msec = PMOVE_FRAME_MSEC_MAX;
        ps = move->ps; /* MOV EAX,[EBP] */
        ps->commandTime = move->command.commandTime; /* MOV [ps],[&move->command.commandTime] */
    }

    /* FILD pml.msec begins the frame-time expression before these snapshots;
     * the multiply remains live in x87 while every component reloads move->ps.
     * The final m32 frame-time store precedes the velocity[2] dword store. */
    ps = move->ps;
    pml.previousOrigin[0] = ps->psOrigin[0];
    ps = move->ps;
    pml.previousOrigin[1] = ps->psOrigin[1];
    ps = move->ps;
    pml.previousOrigin[2] = ps->psOrigin[2];
    ps = move->ps;
    pml.previousVelocity[0] = ps->velocity[0];
    ps = move->ps;
    pml.previousVelocity[1] = ps->velocity[1];
    ps = move->ps;
    float previousVelocityZ = ps->velocity[2];
#if EMULATE_X87
    pml.frametime = x87f_store_f32(x87f_mul(x87f_load_i32(pml.msec), x87f_load_f32(PM_MS_TO_SEC)));
#else
    pml.frametime = (float)((long double)pml.msec * (long double)PM_MS_TO_SEC);
#endif
    pml.previousVelocity[2] = previousVelocityZ;

    ps = move->ps;
    pml.weaponInfo = bg_weaponInfos[ps->currentWeapon]; /* 0x3000e363: re-cache */

    PM_AdjustAimSpreadScale(); /* 0x30013a90 */

    /* 0x3000e36e/379: PM_UpdateViewAngles(ps, &move->command.commandTime, move->trace3). The command
     * pointer is LEA ESI,[EBP+0x4] — the usercmd window at pmove_t +0x04, which
     * this pmove_t view begins at the `time` field. */
    pm_trace_fn_t trace3 = move->trace3;
    ps = move->ps;
    PM_UpdateViewAngles(ps, &move->command, trace3);

    /* 0x3000e39e: AngleVectors(ps->viewAngles, pml.forward, pml.right, pml.up). */
    stepPm = pm;
    ps = stepPm->ps;
    AngleVectors(ps->viewAngles, pml.forward, pml.right, pml.up);

    /* ---- substep view-lock finalize (re-reads pm, 0x3000e3a3) ---- */
    stepPm = pm;

    /* 0x3000e3a9: CMP byte [move+0x1a],0xa; JGE — a signed-byte compare: values
     * 0x80..0xff are negative, i.e. < 10, and do clear the bit. */
    if (stepPm->command.upmove < PMOVE_STATE_LOCK_LIMIT)
        stepPm->ps->playerStateFlags &= ~(uint32_t)PMF_JUMP_HELD; /* MOV EAX,[ECX]; &~0x8 */

    /* 0x3000e3b5: re-derive the view-lock bit from the command forward-move sign. */
    if (stepPm->command.forwardmove < 0) {
        ps = stepPm->ps;
        ps->playerStateFlags |= (uint32_t)PMF_BACKPEDAL; /* OR 0x40 */
    } else if (stepPm->command.forwardmove > 0 || stepPm->command.rightmove != 0) {
        ps = stepPm->ps;
        ps->playerStateFlags &= ~(uint32_t)PMF_BACKPEDAL; /* AND ~0x40 */
    }

    /* 0x3000e3de: if (ps->pmType >= 6) clear the three command/state bytes. */
    ps = stepPm->ps;
    if (ps->pmType >= PM_TYPE_DEAD) {
        stepPm->command.forwardmove = 0;
        stepPm->command.rightmove = 0;
        stepPm->command.upmove = 0;
    }

    /* 0x3000e3f5: if (viewLerpMode == 1 && (ps->playerStateFlags & 0x400)) clear command bytes. */
    if (viewLerpMode == 1 && (ps->playerStateFlags & PMF_PRONE_MOVEMENT_OVERRIDE)) { /* TEST DH,0x4 */
        stepPm->command.forwardmove = 0;
        stepPm->command.rightmove = 0;
    }

    /* ---- pmType dispatch (0x3000e410) ---- */
    pmType = (uint32_t)ps->pmType;
    if ((uint32_t)(pmType - 1) > 6) /* CMP EDX,ESI(6); JA default */
        goto pm_default_tail;

    switch (pmType) {
    case PM_TYPE_SPECTATOR: /* table[3] -> 0x3000e423 */
        ps->playerStateFlags &= ~(uint32_t)PMF_LADDER;
        PM_UpdateAimDownSightFlag(); /* 0x30011b60 */
        PM_UpdatePlayerWalkingFlag(); /* 0x3000d7a0 */
        PM_UpdatePlayerSprintingFlag(); /* 0x3000d800 */
        PM_CheckDuck(); /* 0x3000b010 */
        PM_FlyMove(); /* 0x30008f20 */
        PM_DropTimers(); /* 0x3000c320 */
        PM_UpdateFatigue(); /* tail JMP 0x3000c420 */
        return;

    case PM_TYPE_NOCLIP: /* table[1] -> 0x3000e451 */
        ps->playerStateFlags &= ~(uint32_t)PMF_LADDER;
        PM_UpdateAimDownSightFlag();
        PM_UpdatePlayerWalkingFlag();
        PM_UpdatePlayerSprintingFlag();
        PM_NoclipMove(); /* 0x30009700 */
        goto pm_weapon_fatigue_tail; /* JMP 0x3000e483 */

    case PM_TYPE_UFO: /* table[2] -> 0x3000e46b */
        ps->playerStateFlags &= ~(uint32_t)PMF_LADDER;
        PM_UpdateAimDownSightFlag();
        PM_UpdatePlayerWalkingFlag();
        PM_UpdatePlayerSprintingFlag();
        PM_UFOMove(); /* 0x300098c0 */
    pm_weapon_fatigue_tail:
        PM_Weapon(); /* 0x3000e483 CALL 0x30014710 */
        PM_DropTimers(); /* 0x3000e488 CALL 0x3000c320 */
        PM_UpdateFatigue(); /* tail JMP 0x3000c420 */
        return;

    case PM_TYPE_INTERMISSION: /* table[4] -> 0x3000e499: minimal path */
        ps->playerStateFlags &= ~(uint32_t)PMF_LADDER;
        return;

    case PM_TYPE_DEAD: /* table[5] -> 0x3000e52d: the SAME tail as out-of-range
                       * pmType values (verified from the jump table dwords at
                       * 0x3000e720), NOT the grounded 0x3000e4aa body. */
        goto pm_default_tail;

    case PM_TYPE_LINKED: /* table[0]/table[6] -> 0x3000e4aa */
    case PM_TYPE_LINKED_DEAD:
    default:
        break;
    }

    /* ---- PM_TYPE_LINKED / PM_TYPE_LINKED_DEAD body (0x3000e4aa) ---- */
    ps->playerStateFlags &= ~(uint32_t)PMF_LADDER;
    ps = stepPm->ps; /* MOV EDX,[ECX] */
    ps->groundEntityNum = ENTITYNUM_NONE; /* MOV [ps+0x58],0x3ff */
    pml.groundPlane = 0; /* 0x305395b0 */
    pml.groundLiftFlag = 0; /* 0x305395b4 */
    pml.walking = 0; /* 0x305395ac */
    PM_UpdateAimDownSightFlag();
    PM_UpdatePlayerWalkingFlag();
    PM_UpdatePlayerSprintingFlag();
    PM_CheckDuck();
    PM_DropTimers();
    PM_UpdateFatigue();

    /* 0x3000e4e7: post-move vehicle/footstep finalize. EVERY exit of this stub
     * ends in JMP 0x3000bba0 (PM_Footsteps); the (vehicleType==1 &&
     * vehiclePosition==3) path and the mask-clear path first CALL 0x30014710
     * (PM_Weapon) at 0x3000e51c, the other vehicle path first CALLs 0x30011f50
     * (PM_UpdateAimDownSightLerp) at 0x3000e50b. */
    ps = pm->ps;
    if (ps->entityStateFlags & EF_RESTRICTED_MASK) { /* TEST +0x84,0x106000 */
        if (ps->vehicleType == 1 && ps->vehiclePosition == 3) { /* +0x618==1 && +0x614==3 */
            PM_Weapon(); /* 0x3000e51c CALL 0x30014710 */
            PM_Footsteps(); /* JMP 0x3000bba0 */
            return;
        }
        PM_UpdateAimDownSightLerp(); /* 0x3000e50b CALL 0x30011f50 */
        PM_Footsteps(); /* JMP 0x3000bba0 */
        return;
    }
    PM_Weapon(); /* 0x3000e51c CALL 0x30014710 */
    PM_Footsteps(); /* JMP 0x3000bba0 */
    return;

pm_default_tail:
    /* ---- pmType==6 / out-of-range body (0x3000e52d) ---- */
    if (ps->entityStateFlags & EF_RESTRICTED_MASK) { /* TEST +0x84,0x106000; JZ full */
        ps->playerStateFlags &= ~(uint32_t)PMF_LADDER;
        ps = stepPm->ps; /* MOV ECX,[ECX] */
        ps->groundEntityNum = ENTITYNUM_NONE;
        pml.groundPlane = 0;
        pml.groundLiftFlag = 0;
        pml.walking = 0;
        PM_UpdateAimDownSightFlag();
        PM_UpdatePlayerWalkingFlag();
        PM_UpdatePlayerSprintingFlag();
        PM_CheckDuck();
        PM_DropTimers();
        PM_UpdateFatigue();

        /* 0x3000e576: same finalize shape as the grounded stub — both paths
         * end in JMP 0x3000bba0 (PM_Footsteps), after PM_Weapon (0x3000e58f)
         * or PM_UpdateAimDownSightLerp (0x3000e5a0) respectively. */
        ps = pm->ps;
        if (ps->vehicleType == 1 && ps->vehiclePosition == 3) {
            PM_Weapon(); /* 0x3000e58f CALL 0x30014710 */
            PM_Footsteps(); /* JMP 0x3000bba0 */
            return;
        }
        PM_UpdateAimDownSightLerp(); /* 0x3000e5a0 CALL 0x30011f50 */
        PM_Footsteps(); /* JMP 0x3000bba0 */
        return;
    }

    /* ---- full move body (0x3000e5b1): no sprint-mask disable ---- */
    PM_SetWaterLevel(); /* 0x3000a7a0 */
    pml.previousWaterLevel = move->waterlevel; /* MOVZX [move+0xf1]; store 0x30539604 */
    PM_CheckDuck(); /* 0x3000b010 */
    PM_GroundTrace(); /* 0x3000a470 */
    PM_UpdateAimDownSightFlag(); /* 0x30011b60 */
    PM_UpdatePlayerWalkingFlag(); /* 0x3000d7a0 */
    PM_UpdatePlayerSprintingFlag(); /* 0x3000d800 */
    PM_UpdatePronePitch(); /* 0x3000d470 */

    ps = pm->ps;
    if (ps->pmType == PM_TYPE_DEAD) /* CMP [ps+0x4],6 */
        PM_DeadMove(); /* 0x30009660 */

    PM_CheckLadderMove(); /* 0x3000d920 */
    PM_DropTimers(); /* 0x3000c320 */
    PM_UpdateFatigue(); /* 0x3000c420 */

    /* 0x3000e601: movement mode select. */
    ps = pm->ps;
    if (ps->playerStateFlags & PMF_LADDER) { /* TEST [ps+0xc],0x10 */
        PM_LadderMove(); /* 0x3000dc70 */
    } else if (pml.walking != 0) { /* CMP [0x305395ac],0 */
        PM_WalkMove(); /* 0x300091e0 */
    } else {
        PM_AirMove(); /* 0x30009060 */
    }

    PM_GroundTrace(); /* 0x3000a470 */
    PM_SetWaterLevel(); /* 0x3000a7a0 */
    PM_Footsteps(); /* 0x3000bba0 */
    PM_Weapon(); /* 0x30014710 */
    PM_FoliageSounds(); /* 0x3000c110 */
    PM_WaterEvents(); /* 0x3000c290 */

    /*
     * ---- derive velocity from the frame position delta (0x3000e647) ----
     * ps = pm->ps.
     * d  = ps->psOrigin - pml.previousOrigin.
     * The x87 sequence forms:
     *   moveSq  = dx*dx + dy*dy + dz*dz              (squared distance moved)
     *   velSq   = vx*vx + vy*vy + vz*vz              (squared current velocity)
     *   lhs     = moveSq / (pml.frametime*pml.frametime)
     *   rhs     = velSq  * 0.25f
     * FCOMPP compares rhs vs lhs; TEST AH,0x41 (C0|C3). JNE (rhs<=lhs) skips the
     * derivation. When rhs > lhs (velocity large relative to the actual move),
     * velocity is REBUILT from the position delta over the frame time:
     *   ps->velocity = d / pml.frametime.
     */
    {
        pmove_t *notifyPm = pm; /* MOV EDX,[global] */
        playerState_t *p = notifyPm->ps; /* MOV ECX,[EDX] */
        /* dy and dz are spilled to float ([ESP+0x14]/[ESP+0x18] @ 0x3000e661/e66e),
         * but dx is kept in the x87 stack across dx*dx (FMUL ST5 @ 0x3000e68f) and
         * the velocity[0] rebuild (FMUL ST1 @ 0x3000e6d2) with no store -- asymmetric,
         * so dx is long double while dy/dz stay float. moveSq/velSq/lhs/rhs are all
         * register-carried into the FCOMPP (0x3000e6b7), never stored to a float slot. */
#if defined(WINDOWS_BEHAVIOR)
#if EMULATE_X87
        const x87f dx = x87f_sub(x87f_load_f32(p->psOrigin[0]), x87f_load_f32(pml.previousOrigin[0]));
        const float dy = x87f_store_f32(x87f_sub(x87f_load_f32(p->psOrigin[1]), x87f_load_f32(pml.previousOrigin[1])));
        const float dz = x87f_store_f32(x87f_sub(x87f_load_f32(p->psOrigin[2]), x87f_load_f32(pml.previousOrigin[2])));
        const x87f moveSq = x87f_add(
            x87f_add(x87f_mul(x87f_load_f32(dz), x87f_load_f32(dz)), x87f_mul(x87f_load_f32(dy), x87f_load_f32(dy))), x87f_mul(dx, dx));
        const x87f lhs = x87f_div(moveSq, x87f_mul(x87f_load_f32(pml.frametime), x87f_load_f32(pml.frametime)));
        const x87f velSq = x87f_add(x87f_add(x87f_mul(x87f_load_f32(p->velocity[0]), x87f_load_f32(p->velocity[0])),
                                             x87f_mul(x87f_load_f32(p->velocity[1]), x87f_load_f32(p->velocity[1]))),
                                    x87f_mul(x87f_load_f32(p->velocity[2]), x87f_load_f32(p->velocity[2])));
        const x87f rhs = x87f_mul(velSq, x87f_load_f32(PM_VEL_QUARTER));

        if (x87f_lt_signaling(lhs, rhs)) {
            p->velocity[0] = x87f_store_f32(x87f_mul(x87f_div(x87f_load_f32(PM_ONE), x87f_load_f32(pml.frametime)), dx));
            p = notifyPm->ps;
            p->velocity[1] = x87f_store_f32(x87f_mul(x87f_div(x87f_load_f32(PM_ONE), x87f_load_f32(pml.frametime)), x87f_load_f32(dy)));
            p = notifyPm->ps;
            p->velocity[2] = x87f_store_f32(x87f_mul(x87f_div(x87f_load_f32(PM_ONE), x87f_load_f32(pml.frametime)), x87f_load_f32(dz)));
        }
#else
        const long double dx = (long double)p->psOrigin[0] - (long double)pml.previousOrigin[0];
        const float dy = (float)((long double)p->psOrigin[1] - (long double)pml.previousOrigin[1]);
        const float dz = (float)((long double)p->psOrigin[2] - (long double)pml.previousOrigin[2]);
        const long double moveSq = ((long double)dz * dz + (long double)dy * dy) + dx * dx;
        const long double lhs = moveSq / ((long double)pml.frametime * (long double)pml.frametime);
        const long double velSq = ((long double)p->velocity[0] * p->velocity[0] + (long double)p->velocity[1] * p->velocity[1]) +
                                  (long double)p->velocity[2] * p->velocity[2];
        const long double rhs = velSq * (long double)PM_VEL_QUARTER;

        if (lhs < rhs) {
            long double inverseFrame = (long double)PM_ONE / (long double)pml.frametime;
            p->velocity[0] = (float)(inverseFrame * dx);
            p = notifyPm->ps;
            inverseFrame = (long double)PM_ONE / (long double)pml.frametime;
            p->velocity[1] = (float)(inverseFrame * (long double)dy);
            p = notifyPm->ps;
            inverseFrame = (long double)PM_ONE / (long double)pml.frametime;
            p->velocity[2] = (float)(inverseFrame * (long double)dz);
        }
#endif
#else
        /* Linux RVAs 0x0002e072..0x0002e12c store all three deltas, the
         * distance-derived speed, and the velocity squared to binary32. */
#if EMULATE_X87
        const float dx = x87f_store_f32(x87f_sub(x87f_load_f32(p->psOrigin[0]), x87f_load_f32(pml.previousOrigin[0])));
        const float dy = x87f_store_f32(x87f_sub(x87f_load_f32(p->psOrigin[1]), x87f_load_f32(pml.previousOrigin[1])));
        const float dz = x87f_store_f32(x87f_sub(x87f_load_f32(p->psOrigin[2]), x87f_load_f32(pml.previousOrigin[2])));
        const float deltaSpeedSq = x87f_store_f32(
            x87f_div(x87f_add(x87f_add(x87f_mul(x87f_load_f32(dx), x87f_load_f32(dx)), x87f_mul(x87f_load_f32(dy), x87f_load_f32(dy))),
                              x87f_mul(x87f_load_f32(dz), x87f_load_f32(dz))),
                     x87f_mul(x87f_load_f32(pml.frametime), x87f_load_f32(pml.frametime))));
        const float velocitySpeedSq =
            x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(p->velocity[0]), x87f_load_f32(p->velocity[0])),
                                             x87f_mul(x87f_load_f32(p->velocity[1]), x87f_load_f32(p->velocity[1]))),
                                    x87f_mul(x87f_load_f32(p->velocity[2]), x87f_load_f32(p->velocity[2]))));
        const x87f rhs = x87f_mul(x87f_load_f32(velocitySpeedSq), x87f_load_f32(PM_VEL_QUARTER));

        if (x87f_lt(x87f_load_f32(deltaSpeedSq), rhs)) {
            for (int32_t lane = 0; lane < 3; ++lane) {
                const float delta = lane == 0 ? dx : lane == 1 ? dy : dz;
                p = notifyPm->ps;
                p->velocity[lane] =
                    x87f_store_f32(x87f_mul(x87f_div(x87f_load_f32(PM_ONE), x87f_load_f32(pml.frametime)), x87f_load_f32(delta)));
            }
        }
#else
        const float dx = (float)((long double)p->psOrigin[0] - (long double)pml.previousOrigin[0]);
        const float dy = (float)((long double)p->psOrigin[1] - (long double)pml.previousOrigin[1]);
        const float dz = (float)((long double)p->psOrigin[2] - (long double)pml.previousOrigin[2]);
        const float deltaSpeedSq = (float)((((long double)dx * dx + (long double)dy * dy) + (long double)dz * dz) /
                                           ((long double)pml.frametime * (long double)pml.frametime));
        const float velocitySpeedSq =
            (float)(((long double)p->velocity[0] * p->velocity[0] + (long double)p->velocity[1] * p->velocity[1]) +
                    (long double)p->velocity[2] * p->velocity[2]);

        if ((long double)deltaSpeedSq < (long double)velocitySpeedSq * (long double)PM_VEL_QUARTER) {
            const float delta[3] = {dx, dy, dz};
            for (int32_t lane = 0; lane < 3; ++lane) {
                p = notifyPm->ps;
                p->velocity[lane] = (float)(((long double)PM_ONE / (long double)pml.frametime) * (long double)delta[lane]);
            }
        }
#endif
#endif

        /* 0x3000e707 reloads ps through the retained 0x3000e647 move pointer;
         * it does not reread pm after the arithmetic. */
        p = notifyPm->ps;
        trap_SnapVector(&p->velocity[0]);
    }
}
