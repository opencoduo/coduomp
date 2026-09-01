#include "q_math.h"

/*
 * The four authoritative Windows AnglesToAxis bodies are instruction-identical
 * apart from relocated calls and the vec3_origin address:
 *
 *   CoDUOMP.exe                 0x004340a0
 *   uo_cgame_mp_x86.dll        0x3004c200
 *   uo_ui_mp_x86.dll           0x40004210
 *   uo_game_mp_x86.dll         0x20019250
 *
 * The Linux game body at RVA 0x0003e24a and dedicated-engine body at
 * 0x0806a46e retain the same AngleVectors call and the same three
 * vec3_origin[i] - right[i] stores.  The dedicated reconstruction's former
 * matrix43_t output type was over-wide: the original body writes exactly the
 * nine axis_t lanes and no translation lanes.
 */
void AnglesToAxis(const vec3_t angles, axis_t axis)
{
    vec3_t right;

    AngleVectors(angles, axis[0], right, axis[2]);
    axis[1][0] = vec3_origin[0] - right[0];
    axis[1][1] = vec3_origin[1] - right[1];
    axis[1][2] = vec3_origin[2] - right[2];
}
