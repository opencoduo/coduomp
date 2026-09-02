#include "bg_pmove.h"

#include "compat/coduo_x87emu.h"

#include <stdint.h>

void Com_Printf(const char *format, ...);

/*
 * Sources:
 *   uo_cgame_mp_x86.dll 0x3000a470..0x3000a79c
 *   uo_game_mp_x86.dll  0x2000a230..0x2000a55b
 *   game.mp.uo.i386.so  0x00026823..0x00026cf0
 *
 * This is the complete ground-contact driver: initial trace, all-solid
 * recovery, retry, missed-ground handling, kickoff, steep-plane handling,
 * landing, and touch registration.  The two Windows bodies have the same
 * operation graph.  Linux retains the same decisions.  The only computational
 * distinction is the live x87 dot-product order documented at the comparison.
 */

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

void PM_GroundTrace(void)
{
    pmove_t *move = pm;
    playerState_t *ps = move->ps;
    trace_t trace;
    vec3_t start;
    vec3_t end;

    start[0] = ps->psOrigin[0];
    start[1] = ps->psOrigin[1];
    end[0] = ps->psOrigin[0];
    end[1] = ps->psOrigin[1];

    if ((ps->entityStateFlags & EF_RESTRICTED_MASK) != 0) {
        start[2] = ps->psOrigin[2];
        end[2] = ps->psOrigin[2] - 1.0f;
    } else {
        start[2] = ps->psOrigin[2] + 0.25f;
        end[2] = ps->psOrigin[2] - 0.25f;
    }

    PM_trace(&trace, start, move->mins, move->maxs, end,
             ps->psClientNum, move->traceMask);
    pml.groundTrace = trace;

    if (trace.allsolid != 0 && PM_CorrectAllSolid(&trace) == qfalse) {
        return;
    }

    if (trace.startsolid != 0) {
        move = pm;
        start[2] = move->ps->psOrigin[2] - 0.001f;
        PM_trace(&trace, start, move->mins, move->maxs, end,
                 move->ps->psClientNum, move->traceMask);
        if (trace.startsolid != 0) {
            move = pm;
            move->ps->groundEntityNum = ENTITYNUM_NONE;
            pml.walking = qfalse;
            pml.groundPlane = qfalse;
            pml.groundLiftFlag = qfalse;
            return;
        }
        pml.groundTrace = trace;
    }

    if (trace.fraction == 1.0f) {
        PM_GroundTraceMissed();
        return;
    }

    move = pm;
    ps = move->ps;
    if ((ps->playerStateFlags & PMF_LADDER) == 0 &&
        ps->velocity[2] > 0.0f) {
        qboolean kickoff;

        /* Windows cgame/game both evaluate Y+X+Z (0x3000a5fc and
         * 0x2000a3bc). Linux evaluates X+Y+Z at 0x00026ab4. All three compare
         * the unspilled x87 value against 10.0. The order is the only genuine
         * platform distinction in this function. */
#if EMULATE_X87
#if defined(WINDOWS_BEHAVIOR)
        const x87f velocityDot = x87f_add(
            x87f_add(x87f_mul(x87f_load_f32(trace.normal[1]),
                              x87f_load_f32(ps->velocity[1])),
                     x87f_mul(x87f_load_f32(trace.normal[0]),
                              x87f_load_f32(ps->velocity[0]))),
            x87f_mul(x87f_load_f32(trace.normal[2]),
                     x87f_load_f32(ps->velocity[2])));
        kickoff = x87f_lt_signaling(x87f_load_f32(10.0f), velocityDot)
                       ? qtrue
                       : qfalse;
#else
        const x87f velocityDot = x87f_add(
            x87f_add(x87f_mul(x87f_load_f32(ps->velocity[0]),
                              x87f_load_f32(trace.normal[0])),
                     x87f_mul(x87f_load_f32(ps->velocity[1]),
                              x87f_load_f32(trace.normal[1]))),
            x87f_mul(x87f_load_f32(ps->velocity[2]),
                     x87f_load_f32(trace.normal[2])));
        kickoff = x87f_lt(x87f_load_f32(10.0f), velocityDot)
                       ? qtrue
                       : qfalse;
#endif
#else
#if defined(WINDOWS_BEHAVIOR)
        const long double velocityDot =
            ((long double)trace.normal[1] * (long double)ps->velocity[1] +
             (long double)trace.normal[0] * (long double)ps->velocity[0]) +
            (long double)trace.normal[2] * (long double)ps->velocity[2];
#else
        const long double velocityDot =
            ((long double)ps->velocity[0] * (long double)trace.normal[0] +
             (long double)ps->velocity[1] * (long double)trace.normal[1]) +
            (long double)ps->velocity[2] * (long double)trace.normal[2];
#endif
        kickoff = velocityDot > 10.0L ? qtrue : qfalse;
#endif

        if (kickoff != qfalse) {
            if (move->debugMove != 0) {
                Com_Printf("%i:kickoff\n", c_pmove);
                move = pm;
            }

            BG_AnimScriptEvent(
                move->ps,
                move->command.forwardmove < 0
                    ? ANIM_EVENT_JUMP_BACK
                    : ANIM_EVENT_JUMP,
                qfalse, qfalse);
            move = pm;
            pml.groundLiftFlag = qfalse;
            move->ps->groundEntityNum = ENTITYNUM_NONE;
            pml.groundPlane = qfalse;
            pml.walking = qfalse;
            return;
        }
    }

    if (trace.normal[2] < 0.7f) {
        if (move->debugMove != 0) {
            Com_Printf("%i:steep\n", c_pmove);
            move = pm;
        }
        move->ps->groundEntityNum = ENTITYNUM_NONE;
        pml.groundPlane = qtrue;
        pml.groundLiftFlag = qtrue;
        pml.walking = qfalse;
        move->ps->playerStateFlags &= ~(uint32_t)PMF_WALLJUMP;
        move->ps->jumpOriginZ = 0.0f;
        return;
    }

    pml.groundPlane = qtrue;
    pml.groundLiftFlag = qtrue;
    pml.walking = qtrue;
    if (move->ps->groundEntityNum == ENTITYNUM_NONE) {
        if (move->debugMove != 0) {
            Com_Printf("%i:Land\n", c_pmove);
        }
        PM_CrashLand();
        move = pm;
    }

    move->ps->groundEntityNum = trace.entityNum;
    PM_AddTouchEnt(trace.entityNum);
}
