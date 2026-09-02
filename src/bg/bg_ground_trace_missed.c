#include "bg_pmove.h"

#include <stdint.h>

void Com_Printf(const char *format, ...);

/*
 * Sources:
 *   uo_cgame_mp_x86.dll 0x3000a2a0..0x3000a46a
 *   uo_game_mp_x86.dll  0x2000a060..0x2000a229
 *   game.mp.uo.i386.so  0x0002656d..0x00026822
 *
 * PM_GroundTraceMissed probes beneath a player who has lost the current
 * supporting trace, updates the moving-lift latch, optionally starts the
 * walk-off animation, and marks the player airborne.  The two Windows bodies
 * retain the same instruction graph apart from module addresses.  Linux keeps
 * the same source decisions but calls BG_AnimScriptEvent instead of inlining
 * it, which supplies the canonical source-level call used here.
 */

#define PM_GROUNDTRACE_DROP_GROUNDED 64.0f
#define PM_GROUNDTRACE_DROP_AIRBORNE 1.0f
#define PM_GROUNDTRACE_SNAP_FRACTION 0.015625f

void PM_GroundTraceMissed(void)
{
    pmove_t *move = pm;
    playerState_t *ps = move->ps;
    trace_t trace;
    vec3_t end;

    if (ps->groundEntityNum == ENTITYNUM_NONE) {
        end[0] = ps->psOrigin[0];
        end[1] = ps->psOrigin[1];
        end[2] = ps->psOrigin[2] - PM_GROUNDTRACE_DROP_AIRBORNE;
        move->trace(&trace, ps->psOrigin, move->mins, move->maxs, end, ps->psClientNum, move->traceMask);
        pml.groundLiftFlag = trace.fraction != 1.0f ? qtrue : qfalse;
    } else {
        if (move->debugMove != 0) {
            Com_Printf("%i:lift\n", c_pmove);
            move = pm;
        }

        ps = move->ps;
        end[0] = ps->psOrigin[0];
        end[1] = ps->psOrigin[1];
        end[2] = ps->psOrigin[2] - PM_GROUNDTRACE_DROP_GROUNDED;
        move->trace(&trace, ps->psOrigin, move->mins, move->maxs, end, ps->psClientNum, move->traceMask);

        if (trace.fraction == 1.0f) {
            const bg_anim_event_t event = move->command.forwardmove < 0 ? ANIM_EVENT_JUMP_BACK : ANIM_EVENT_JUMP;

            BG_AnimScriptEvent(move->ps, event, qfalse, qtrue);
            pml.groundLiftFlag = qfalse;
        } else {
            pml.groundLiftFlag = trace.fraction < PM_GROUNDTRACE_SNAP_FRACTION ? qtrue : qfalse;
        }
    }

    move = pm;
    move->ps->groundEntityNum = ENTITYNUM_NONE;
    pml.groundPlane = qfalse;
    pml.walking = qfalse;
}
