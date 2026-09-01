#include "bg_pmove.h"

#include "qcommon/entity_event_types.h"
#include "qcommon/player_state_types.h"
#include "qcommon/q_collision_types.h"

/*
 * The complete movement surface-event selector family agrees behaviorally in
 * Windows cgame, Windows game, and Linux game.  The Windows modules inline the
 * common PM_GroundSurfaceType operation into the six selectors and produce
 * instruction-identical selector bodies apart from global addresses:
 *
 *   function                    cgame       Windows game   Linux game RVA
 *   PM_GroundSurfaceType        0x30008c00  0x200089b0     0x00060eaa
 *   PM_JumpForSurface           0x30008c30  0x200089e0     0x00060f12
 *   PM_FootstepForSurface       0x30009bc0  0x20009970     0x00060f5f
 *   PM_LightLandingForSurface   0x30009c30  0x200099e0     0x000613b9
 *   PM_MediumLandingForSurface  0x30009c70  0x20009a20     0x000613e0
 *   PM_HardLandingForSurface    0x30009cb0  0x20009a60     0x00061406
 *   PM_DamageLandingForSurface  0x30009cf0  0x20009aa0     0x0006141d
 *
 * Linux retains PM_GroundSurfaceType as a local function and calls it.  That
 * compilation-shape difference does not change the shared source contract.
 */

enum {
    /* Ladder jumps deliberately select the metal member of the jump-event
     * range in all three binaries. */
    PM_LADDER_JUMP_EVENT = EV_JUMP_METAL
};

int32_t PM_GroundSurfaceType(void)
{
    uint32_t surfaceFlags = pml.groundTrace.surfaceFlags;
    int32_t surfaceType;

    if ((surfaceFlags & SURF_NOSTEPS) != 0) {
        return 0;
    }

    surfaceType = (int32_t)((surfaceFlags >> SURFACE_TYPE_SHIFT) &
                            SURFACE_TYPE_MASK);
    if (pm != NULL && pm->waterlevel != 0) {
        return SURFACE_TYPE_WATER;
    }
    return surfaceType;
}

int32_t PM_JumpForSurface(void)
{
    int32_t surfaceType;

    if ((pm->ps->playerStateFlags & PMF_LADDER) != 0) {
        return PM_LADDER_JUMP_EVENT;
    }

    surfaceType = PM_GroundSurfaceType();
    if (surfaceType == 0) {
        return 0;
    }
    return surfaceType + EV_JUMP_DEFAULT;
}

int32_t PM_FootstepForSurface(uint32_t playerStateFlags)
{
    int32_t surfaceType = PM_GroundSurfaceType();

    if (surfaceType == 0) {
        return 0;
    }
    if ((playerStateFlags & PMF_PRONE) != 0) {
        return surfaceType + EV_FOOTSTEP_PRONE_DEFAULT;
    }
    if ((playerStateFlags & PMF_SPRINTING) != 0) {
        return surfaceType + EV_JUMP_DEFAULT;
    }
    if ((playerStateFlags & PMF_WALKING) != 0 ||
        pm->ps->leanFraction != 0.0f) {
        return surfaceType + EV_FOOTSTEP_WALK_DEFAULT;
    }
    return surfaceType + EV_FOOTSTEP_RUN_DEFAULT;
}

int32_t PM_LightLandingForSurface(void)
{
    int32_t surfaceType = PM_GroundSurfaceType();

    if (surfaceType != 0) {
        surfaceType += EV_FOOTSTEP_WALK_DEFAULT;
    }
    return surfaceType;
}

int32_t PM_MediumLandingForSurface(void)
{
    int32_t surfaceType = PM_GroundSurfaceType();

    if (surfaceType != 0) {
        surfaceType += EV_FOOTSTEP_RUN_DEFAULT;
    }
    return surfaceType;
}

int32_t PM_HardLandingForSurface(void)
{
    return PM_GroundSurfaceType() + EV_LANDING_DEFAULT;
}

int32_t PM_DamageLandingForSurface(void)
{
    return PM_GroundSurfaceType() + EV_LANDING_PAIN_DEFAULT;
}
