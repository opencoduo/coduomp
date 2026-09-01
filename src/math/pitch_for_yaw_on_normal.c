#include "q_math.h"

#include <stddef.h>

/*
 * The four authoritative Windows bodies use the same instruction stream apart
 * from relocated constants and calls:
 *
 *   CoDUOMP.exe                 0x00434fa0
 *   uo_cgame_mp_x86.dll        0x3004d100
 *   uo_ui_mp_x86.dll           0x40005110
 *   uo_game_mp_x86.dll         0x2001a150
 *
 * They inline YawVectors, then call ProjectPointOnPlane and vectopitch.  The
 * Linux game body at RVA 0x0003f2c9 and dedicated body at 0x0806b397 retain
 * those same three source operations as calls.  Delegating the yaw conversion
 * to the already shared YawVectors body preserves each target's proven
 * degrees-to-radians constant and sign handling without a platform variant in
 * this function.
 */
float PitchForYawOnNormal(float yaw, const vec3_t normal)
{
    vec3_t forward;
    vec3_t projected;

    YawVectors(yaw, forward, NULL);
    ProjectPointOnPlane(projected, forward, normal);
    return vectopitch(projected);
}
