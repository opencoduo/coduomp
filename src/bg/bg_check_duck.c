// Source: uo_cgame_mp_x86.dll 0x3000b010..0x3000b941
//         uo_game_mp_x86.dll  0x2000add0..0x2000b701
//         game.mp.uo.i386.so  RVA 0x00027e16..0x0002937c
//
// PM_CheckDuck -- update prone/crouch state, collision bounds, view height, and
// the prone ground-pitch offsets for the current pmove. The identity is proved by
// the same-module PPC symbol and the body; the mcode's size-based cursor-hint name
// is rejected.  The two Windows bodies have the same 637-instruction graph
// after relocation normalization.  Their only semantic instruction difference
// is the module-owned prone-debug threshold, retained by the local service
// adapter.  Supporting Mac cgame/game symbols independently name equal-size
// 0xb58-byte PM_CheckDuck bodies at code offsets 0x5ba0 and 0x6480.

#include "bg_pmove.h"

#include "bg_pmove_services.h"
#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <stdint.h>

enum {
    PM_EVENT_PRONE_BLOCKED_ALT = 142,
    PM_EVENT_PRONE_BLOCKED = 143,
    PM_EVENT_STANCE_RAISE_BLOCKED = 144,

    PM_VIEWHEIGHT_PRONE_LERP_TIME = 1800
};

/* Repeated predictable-event append emitted inline at four stance branches. */
#define PM_APPEND_STANCE_EVENT(ps_, event_) \
    do { \
        int32_t pm_event_index_ = (ps_)->eventIndex & (MAX_PS_EVENTS - 1); \
        (ps_)->events[pm_event_index_] = (event_); \
        (ps_)->eventParms[pm_event_index_] = 0; \
        (ps_)->eventIndex = coduo_int32_from_bits((uint32_t)(ps_)->eventIndex + 1u); \
    } while (0)

/* Repeated viewheight-transition animation reset at 0x3000b570/0x3000b5ce. */
#define PM_RESET_TORSO_FOR_VIEWHEIGHT_CHANGE(ps_) \
    do { \
        if ((ps_)->weaponState != WEAPON_STATE_DEPLOYING && (ps_)->weaponState != WEAPON_STATE_BREAKING_DOWN && \
            ((ps_)->torsoAnim & ~ANIM_TOGGLEBIT) != 0) { \
            (ps_)->torsoAnim = (~(ps_)->torsoAnim) & ANIM_TOGGLEBIT; \
        } \
    } while (0)

void PM_CheckDuck(void)
{
    pmove_t *move = pm;
    playerState_t *ps = move->ps;

    move->weaponAnimscriptEnabled = qfalse;

    if (ps->pmType == PM_TYPE_SPECTATOR) {
        move->mins[0] = -8.0f;
        move->mins[1] = -8.0f;
        move->mins[2] = -8.0f;
        move->maxs[0] = 8.0f;
        move->maxs[1] = 8.0f;
        move->maxs[2] = 16.0f;
        ps->playerStateFlags &= ~(PMF_PRONE | PMF_DUCKED);
        if ((move->command.wbuttons & PM_WBUTTON_PRONE) != 0) {
            move->command.wbuttons &= (uint8_t)~PM_WBUTTON_PRONE;
            ps = move->ps;
            PM_APPEND_STANCE_EVENT(ps, PM_EVENT_PRONE_BLOCKED_ALT);
        }
        move->trace = move->trace3;
        ps = move->ps;
        ps->entityStateFlags |= EF_STANCE_VALID;
        ps = move->ps;
        ps->viewHeightTarget = 0;
        ps = move->ps;
        ps->viewHeightCurrent = 0.0f;
        return;
    }

    qboolean wasProne = (ps->playerStateFlags & PMF_PRONE) != 0;
    move->mins[0] = ps->playerMins[0];
    move->mins[1] = ps->playerMins[1];
    move->maxs[0] = ps->playerMaxs[0];
    move->maxs[1] = ps->playerMaxs[1];
    move->mins[2] = ps->playerMins[2];

    if (ps->pmType >= PM_TYPE_DEAD) {
        move->maxs[2] = ps->playerMaxs[2];
        ps->viewHeightTarget = ps->deadViewHeight;
        ps = move->ps;
        move->trace = (ps->playerStateFlags & PMF_PRONE) != 0 ? move->trace2 : move->trace3;
        ps->entityStateFlags |= EF_STANCE_VALID;
        PM_ViewHeightAdjust();
        return;
    }

    uint32_t stanceEntityFlags = ps->entityStateFlags;
    if ((stanceEntityFlags & EF_RESTRICTED_MASK) != 0) {
        qboolean forceProne = (stanceEntityFlags & EF_FORCE_PRONE) != 0;
        qboolean forceCrouch = (stanceEntityFlags & EF_FORCE_CROUCH) != 0;

        if (forceProne && !forceCrouch) {
            ps->playerStateFlags |= PMF_PRONE;
            ps = move->ps;
            ps->playerStateFlags &= ~PMF_DUCKED;
        } else if (!forceProne && forceCrouch) {
            ps->playerStateFlags |= PMF_DUCKED;
            ps = move->ps;
            ps->playerStateFlags &= ~PMF_PRONE;
        } else {
            ps->playerStateFlags &= ~(PMF_PRONE | PMF_DUCKED);
        }
        goto stance_selected;
    }

    uint32_t inputStanceFlags = ps->playerStateFlags;
    if ((inputStanceFlags & PMF_FOLLOW) != 0) {
        goto stance_selected;
    }
    if ((inputStanceFlags & PMF_ADS) != 0 && BG_PM_WEAPON_INFO(ps->currentWeapon)->weaponClass == WEAPCLASS_LMG) {
        goto stance_selected;
    }

    if ((inputStanceFlags & PMF_LADDER) != 0 && (move->command.wbuttons & PM_WBUTTON_STANCE_MASK) != 0) {
        move->command.wbuttons &= (uint8_t)~PM_WBUTTON_STANCE_MASK;
        PM_APPEND_STANCE_EVENT(ps, PM_EVENT_PRONE_BLOCKED_ALT);
    }

    if ((move->command.wbuttons & PM_WBUTTON_PRONE) != 0) {
        ps = move->ps;
        if ((ps->playerStateFlags & PMF_PRONE) == 0) {
            qboolean canProne = qfalse;

            if (ps->groundEntityNum != ENTITYNUM_NONE && move->waterlevel == 0) {
                canProne =
                    BG_CheckProne(ps->psClientNum, ps->psOrigin, move->maxs[0], 30.0f, ps->viewAngles[1], &ps->torsoHeight, &ps->torsoPitch,
                                  &ps->waistPitch, qfalse, qtrue, NULL, move->trace3, move->trace2, qfalse, 60.0f, move->entityType) != 0;
                move = pm; /* 0x3000b2a8 reload after BG_CheckProne. */
            }

            if (!canProne) {
                ps = move->ps;
                if (ps->groundEntityNum != ENTITYNUM_NONE) {
                    ps->playerStateFlags |= PMF_PRONE_BLOCKED;
                    if ((move->command.wbuttons & PM_WBUTTON_STANCE_LATCH) == 0) {
                        ps = move->ps;
                        PM_APPEND_STANCE_EVENT(ps, (ps->playerStateFlags & PMF_DUCKED) != 0 ? PM_EVENT_PRONE_BLOCKED
                                                                                            : PM_EVENT_PRONE_BLOCKED_ALT);
                    }
                }
                goto stance_selected;
            }
        }

        ps = move->ps;
        ps->playerStateFlags |= PMF_PRONE;
        ps = move->ps;
        ps->playerStateFlags &= ~PMF_DUCKED;
        goto stance_selected;
    }

    if ((move->command.wbuttons & PM_WBUTTON_CROUCH) != 0) {
        ps = move->ps;
        if ((ps->playerStateFlags & PMF_PRONE) != 0) {
            trace_t trace;
            move->maxs[2] = 50.0f;
            int32_t traceMask = coduo_int32_from_bits((uint32_t)move->traceMask & ~CONTENTS_BODY);
            int32_t passEntityNum = ps->psClientNum;
            pm_trace_fn_t traceCallback = move->trace3;
            traceCallback(&trace, ps->psOrigin, move->mins, move->maxs, ps->psOrigin, passEntityNum, traceMask);
            move = pm; /* 0x3000b345 unconditional callback reload. */
            if (trace.allsolid != 0) {
                if ((move->command.wbuttons & PM_WBUTTON_STANCE_LATCH) == 0) {
                    ps = move->ps;
                    PM_APPEND_STANCE_EVENT(ps, PM_EVENT_STANCE_RAISE_BLOCKED);
                }
                goto stance_selected;
            }
            ps = move->ps;
            ps->playerStateFlags &= ~PMF_PRONE;
        }
        ps = move->ps;
        ps->playerStateFlags |= PMF_DUCKED;
        goto stance_selected;
    }

    ps = move->ps;
    if ((ps->playerStateFlags & PMF_PRONE) != 0) {
        trace_t trace;
        move->maxs[2] = ps->playerMaxs[2];
        int32_t traceMask = coduo_int32_from_bits((uint32_t)move->traceMask & ~CONTENTS_BODY);
        int32_t passEntityNum = ps->psClientNum;
        pm_trace_fn_t traceCallback = move->trace3;
        traceCallback(&trace, ps->psOrigin, move->mins, move->maxs, ps->psOrigin, passEntityNum, traceMask);
        move = pm; /* 0x3000b3b8 unconditional callback reload. */
        ps = move->ps;
        if (trace.allsolid == 0) {
            ps->playerStateFlags &= ~(PMF_PRONE | PMF_DUCKED);
            goto stance_selected;
        }

        move->maxs[2] = 50.0f;
        traceMask = coduo_int32_from_bits((uint32_t)move->traceMask & ~CONTENTS_BODY);
        passEntityNum = ps->psClientNum;
        traceCallback = move->trace3;
        traceCallback(&trace, ps->psOrigin, move->mins, move->maxs, ps->psOrigin, passEntityNum, traceMask);
        move = pm; /* 0x3000b40c unconditional callback reload. */
        if (trace.allsolid != 0) {
            if ((move->command.wbuttons & PM_WBUTTON_STANCE_LATCH) == 0) {
                ps = move->ps;
                PM_APPEND_STANCE_EVENT(ps, PM_EVENT_STANCE_RAISE_BLOCKED);
            }
            goto stance_selected;
        }
        ps = move->ps;
        ps->playerStateFlags &= ~PMF_PRONE;
        ps = move->ps;
        ps->playerStateFlags |= PMF_DUCKED;
    } else if ((ps->playerStateFlags & PMF_DUCKED) != 0) {
        trace_t trace;
        move->maxs[2] = ps->playerMaxs[2];
        int32_t traceMask = coduo_int32_from_bits((uint32_t)move->traceMask & ~CONTENTS_BODY);
        int32_t passEntityNum = ps->psClientNum;
        pm_trace_fn_t traceCallback = move->trace3;
        traceCallback(&trace, ps->psOrigin, move->mins, move->maxs, ps->psOrigin, passEntityNum, traceMask);
        move = pm; /* 0x3000b463 unconditional callback reload. */
        if (trace.allsolid == 0) {
            ps = move->ps;
            ps->playerStateFlags &= ~PMF_DUCKED;
        } else if ((move->command.wbuttons & PM_WBUTTON_STANCE_LATCH) == 0) {
            /* 0x3000b472..0x3000b4a1: standing up from crouch is blocked —
             * append event 143 (parm 0) when the stance latch is clear. */
            ps = move->ps;
            PM_APPEND_STANCE_EVENT(ps, PM_EVENT_PRONE_BLOCKED);
        }
    }

stance_selected:
    ps = move->ps; /* 0x3000b4a7 path join reload. */
    if (ps->viewHeightLerpTime == 0) {
        if ((ps->playerStateFlags & PMF_PRONE) != 0) {
            if (ps->viewHeightTarget == ps->standViewHeight) {
                ps->viewHeightTarget = ps->crouchViewHeight;
            } else {
                if (bg_compat_pmove_prone_debug_trace_enabled() != qfalse) {
                    (void)BG_CheckProne(ps->psClientNum, ps->psOrigin, move->maxs[0], 30.0f, ps->viewAngles[1], NULL, NULL, NULL, qfalse,
                                        ps->groundEntityNum != ENTITYNUM_NONE, NULL, move->trace3, move->trace2, qfalse, 60.0f,
                                        move->entityType);
                    move = pm; /* 0x3000b542 reload after BG_CheckProne. */
                }
                ps = move->ps;
                if (ps->viewHeightTarget != ps->proneViewHeight) {
                    ps->viewHeightTarget = ps->proneViewHeight;
                    ps = move->ps;
                    move->weaponAnimscriptEnabled = qtrue;
                    PM_RESET_TORSO_FOR_VIEWHEIGHT_CHANGE(ps);
                    ps = move->ps;
                    ps->playerStateFlags |= PMF_WALLJUMP;
                    ps = move->ps;
                    ps->pmTime = PM_VIEWHEIGHT_PRONE_LERP_TIME;
                }
            }
        } else if (ps->viewHeightTarget == ps->proneViewHeight) {
            ps->viewHeightTarget = ps->crouchViewHeight;
            move->weaponAnimscriptEnabled = qtrue;
            ps = move->ps;
            PM_RESET_TORSO_FOR_VIEWHEIGHT_CHANGE(ps);
        } else {
            /* 0x3000b5f8..0x3000b610: no crouchViewHeight guard — the select
             * runs whenever viewHeightTarget != proneViewHeight. */
            ps->viewHeightTarget = (ps->playerStateFlags & PMF_DUCKED) != 0 ? ps->crouchViewHeight : ps->standViewHeight;
        }
    }

    PM_ViewHeightAdjust();
    move = pm; /* 0x3000b61b callback-boundary reload. */
    ps = move->ps;

    if (ps->viewHeightTarget == ps->crouchViewHeight) {
        /* 0x3000b680..0x3000b6ad: crouched viewheight — 50.0 hull when any
         * stance bit is set, crouch eflag 0x20 on, prone eflag 0x40 off. */
        move->maxs[2] = (ps->playerStateFlags & (PMF_PRONE | PMF_DUCKED)) == 0 ? ps->playerMaxs[2] : 50.0f;
        ps->entityStateFlags |= EF_CROUCHING;
        ps = move->ps;
        ps->entityStateFlags &= ~EF_PRONE;
    } else if (ps->viewHeightTarget == ps->proneViewHeight) {
        /* 0x3000b63d..0x3000b67c: prone viewheight — 50.0/30.0 hull select,
         * prone eflag 0x40 on, crouch eflag 0x20 off. */
        uint32_t stanceFlags = ps->playerStateFlags;
        if ((stanceFlags & (PMF_PRONE | PMF_DUCKED)) == 0) {
            move->maxs[2] = ps->playerMaxs[2];
        } else if ((stanceFlags & PMF_DUCKED) != 0) {
            move->maxs[2] = 50.0f;
        } else {
            move->maxs[2] = 30.0f;
        }
        ps->entityStateFlags |= EF_PRONE;
        ps = move->ps;
        ps->entityStateFlags &= ~EF_CROUCHING;
    } else {
        move->maxs[2] = ps->playerMaxs[2];
        ps->entityStateFlags &= ~(EF_CROUCHING | EF_PRONE);
    }

    ps = move->ps;
    if ((ps->playerStateFlags & PMF_PRONE) == 0) {
        move->trace = move->trace3;
        ps->entityStateFlags |= EF_STANCE_VALID;
        return;
    }

    move->trace = move->trace2;
    ps->entityStateFlags |= EF_STANCE_VALID;
    if (wasProne) {
        return;
    }

    int8_t forwardMove = move->command.forwardmove;
    int8_t rightMove = (forwardMove == 0) ? move->command.rightmove : 0;
    if (forwardMove != 0 || rightMove != 0) {
        ps = move->ps;
        ps->playerStateFlags &= ~PMF_PRONE_MOVEMENT_OVERRIDE;
        ps = move->ps;
        ps->playerStateFlags &= ~PMF_ADS;
    }

    {
        trace_t trace;
        ps = move->ps;
        vec3_t raised;
        vec3_t firstEnd;
        vec3_t lowered;
        float targetPitch;
        float pitchDelta;

        raised[0] = ps->psOrigin[0];
        raised[1] = ps->psOrigin[1];
#if EMULATE_X87
        raised[2] = x87f_store_f32(x87f_add(x87f_load_f32(ps->psOrigin[2]), x87f_load_f32(10.0f)));
#else
        raised[2] = (float)((long double)ps->psOrigin[2] + 10.0L);
#endif
        int32_t traceMask = coduo_int32_from_bits((uint32_t)move->traceMask & ~CONTENTS_BODY);
        int32_t passEntityNum = ps->psClientNum;
        pm_trace_fn_t traceCallback = move->trace2;
        traceCallback(&trace, ps->psOrigin, move->mins, move->maxs, raised, passEntityNum, traceMask);
        firstEnd[0] = trace.endpos[0];
        firstEnd[1] = trace.endpos[1];
        firstEnd[2] = trace.endpos[2];

        move = pm; /* 0x3000b77d reload after the upward trace. */
        traceMask = coduo_int32_from_bits((uint32_t)move->traceMask & ~CONTENTS_BODY);
        ps = move->ps;
        passEntityNum = ps->psClientNum;
        traceCallback = move->trace2;
        traceCallback(&trace, firstEnd, move->mins, move->maxs, ps->psOrigin, passEntityNum, traceMask);
        move = pm; /* 0x3000b7bf reload after the return trace. */
        playerState_t *originXPs = move->ps;
        originXPs->psOrigin[0] = trace.endpos[0];
        playerState_t *originYPs = move->ps;
        originYPs->psOrigin[1] = trace.endpos[1];
        playerState_t *originZPs = move->ps;
        originZPs->psOrigin[2] = trace.endpos[2];

        /* 0x3000b7df..0x3000b7e7: seed the prone yaw baseline from the current
         * view yaw on prone entry. */
        ps = move->ps;
        ps->proneDirection = ps->viewAngles[1];

        playerState_t *loweredXPs = move->ps;
        lowered[0] = loweredXPs->psOrigin[0];
        playerState_t *loweredYPs = move->ps;
        lowered[1] = loweredYPs->psOrigin[1];
        playerState_t *loweredZPs = move->ps;
#if EMULATE_X87
        lowered[2] = x87f_store_f32(x87f_sub(x87f_load_f32(loweredZPs->psOrigin[2]), x87f_load_f32(0.25f)));
#else
        lowered[2] = (float)((long double)loweredZPs->psOrigin[2] - 0.25L);
#endif
        traceMask = coduo_int32_from_bits((uint32_t)move->traceMask & ~CONTENTS_BODY);
        ps = move->ps;
        passEntityNum = ps->psClientNum;
        traceCallback = move->trace2;
        traceCallback(&trace, ps->psOrigin, move->mins, move->maxs, lowered, passEntityNum, traceMask);

        /* 0x3000b855..0x3000b868: TEST AH,0x5; JP — the zero path is taken on
         * startsolid OR fraction >= 1.0; the ground-normal pitch runs only when
         * the down trace actually hit something (fraction < 1.0). */
        if (!trace.startsolid && trace.fraction < 1.0f) {
            move = pm; /* 0x3000b86a reload before pitch projection. */
            ps = move->ps;
            targetPitch = (float)PitchForYawOnNormal(ps->proneDirection, trace.normal);
            ps = move->ps;
            ps->proneDirectionPitch = targetPitch;
        } else {
            move = pm; /* 0x3000b890 reload for the zero-pitch store. */
            ps = move->ps;
            ps->proneDirectionPitch = 0.0f;
        }

        ps = move->ps;
        float pitchInput;
#if EMULATE_X87
        pitchInput = x87f_store_f32(x87f_sub(x87f_load_f32(ps->proneDirectionPitch), x87f_load_f32(ps->viewAngles[0])));
#else
        pitchInput = (float)((long double)ps->proneDirectionPitch - (long double)ps->viewAngles[0]);
#endif
        pitchDelta = AngleNormalize180(pitchInput);
        /* 0x3000b8c4/0x3000b8ee: both clamp compares are strict; at exactly
         * +/-45 the targetPitch copy is taken. */
        if (pitchDelta < -45.0f) {
#if EMULATE_X87
            ps->proneTorsoPitch = x87f_store_f32(x87f_sub(x87f_load_f32(ps->viewAngles[0]), x87f_load_f32(45.0f)));
#else
            ps->proneTorsoPitch = (float)((long double)ps->viewAngles[0] - 45.0L);
#endif
        } else if (pitchDelta > 45.0f) {
#if EMULATE_X87
            ps->proneTorsoPitch = x87f_store_f32(x87f_add(x87f_load_f32(ps->viewAngles[0]), x87f_load_f32(45.0f)));
#else
            ps->proneTorsoPitch = (float)((long double)ps->viewAngles[0] + 45.0L);
#endif
        } else {
            ps->proneTorsoPitch = ps->proneDirectionPitch;
        }
    }
}
