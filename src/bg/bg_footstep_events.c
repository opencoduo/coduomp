#include "bg_pmove.h"

#include "compat/coduo_x87emu.h"

#include <math.h>
#include <stdint.h>

/*
 * This complete footstep-event cluster is shared by the cgame and game
 * modules.  The retained Windows bodies agree instruction for instruction
 * apart from relocations and the pm global address:
 *
 *   uo_cgame_mp_x86.dll  PM_FootstepEvent       0x3000b950
 *   uo_game_mp_x86.dll   PM_FootstepEvent       0x2000b710
 *   uo_cgame_mp_x86.dll  PM_ShouldMakeFootsteps 0x3000bb60
 *   uo_game_mp_x86.dll   PM_ShouldMakeFootsteps 0x2000b920
 *
 * The Linux game module retains the same decisions at RVAs 0x00060ffa and
 * 0x00061335.  Its unoptimized body spills more temporaries, but does not
 * define a different result.  The explicit x87 path preserves the original
 * load/multiply/subtract/store chain when an x87-compatible host is emulated.
 */

void PM_FootstepEvent(int32_t oldBobCycle, int32_t newBobCycle,
                      int32_t shouldMake)
{
    uint8_t oldBob = (uint8_t)oldBobCycle;
    uint8_t newBob = (uint8_t)newBobCycle;

    if ((((oldBob + 64u) ^ (newBob + 64u)) & 128u) == 0) {
        return;
    }

    if (pm->waterlevel == 0) {
        if (pm->ps->groundEntityNum == ENTITYNUM_NONE) {
            if (shouldMake != 0 &&
                (pm->ps->playerStateFlags & PMF_LADDER) != 0) {
                vec3_t mins;
                vec3_t maxs;
                vec3_t end;
                trace_t trace;
                int32_t surfaceType;

                mins[0] = pm->mins[0] + 6.0f;
                mins[1] = pm->mins[1] + 6.0f;
                mins[2] = 8.0f;
                maxs[0] = pm->maxs[0] - 6.0f;
                maxs[1] = pm->maxs[1] - 6.0f;
                maxs[2] = pm->maxs[2];
                if (maxs[2] < 8.0f) {
                    maxs[2] = 8.0f;
                }

#if EMULATE_X87
                for (int32_t lane = 0; lane < 3; ++lane) {
                    end[lane] = x87f_store_f32(x87f_sub(
                        x87f_load_f32(pm->ps->psOrigin[lane]),
                        x87f_mul(x87f_load_f32(pm->ps->ladderNormal[lane]),
                                 x87f_load_f32(31.0f))));
                }
#else
                end[0] = pm->ps->psOrigin[0] -
                         pm->ps->ladderNormal[0] * 31.0f;
                end[1] = pm->ps->psOrigin[1] -
                         pm->ps->ladderNormal[1] * 31.0f;
                end[2] = pm->ps->psOrigin[2] -
                         pm->ps->ladderNormal[2] * 31.0f;
#endif

                PM_trace(&trace, pm->ps->psOrigin, mins, maxs, end,
                         pm->ps->psClientNum,
                         (int32_t)((uint32_t)pm->traceMask &
                                   ~PM_FOOTSTEP_TRACE_MASK_CLEAR));
                surfaceType =
                    ((uint32_t)trace.surfaceFlags >> SURFACE_TYPE_SHIFT) &
                    SURFACE_TYPE_MASK;
                if (trace.fraction == 1.0f || surfaceType == 0) {
                    surfaceType = PM_FOOTSTEP_DEFAULT_SURFACE;
                }
                PM_AddEvent(surfaceType + EV_FOOTSTEP_RUN_DEFAULT);
            }
        } else if (shouldMake != 0) {
            PM_AddEvent(PM_FootstepForSurface(pm->ps->playerStateFlags));
        }
        return;
    }

    if (pm->waterlevel == 1 || pm->waterlevel == 2) {
        if ((pm->ps->playerStateFlags & PMF_PRONE) != 0) {
            PM_AddEvent(EV_FOOTSTEP_PRONE_WATER);
        } else if ((pm->ps->playerStateFlags & PMF_WALKING) != 0 ||
                   pm->ps->leanFraction != 0.0f ||
                   isnan(pm->ps->leanFraction)) {
            PM_AddEvent(EV_FOOTSTEP_WALK_WATER);
        } else if ((pm->ps->playerStateFlags & PMF_SPRINTING) != 0) {
            PM_AddEvent(EV_JUMP_WATER);
        } else {
            PM_AddEvent(EV_FOOTSTEP_RUN_WATER);
        }
    }
}

int32_t PM_ShouldMakeFootsteps(void)
{
    const playerState_t *ps = pm->ps;
    const int32_t viewHeight = ps->viewHeightTarget;
    const uint32_t walking = ps->playerStateFlags & PMF_WALKING;

    if (viewHeight == ps->crouchViewHeight) {
        return 0;
    }
    if (viewHeight == ps->proneViewHeight) {
        return 0;
    }
    if (walking != 0) {
        return 0;
    }
    return 1;
}
