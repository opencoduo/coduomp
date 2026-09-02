#include "q_math.h"

#include "compat/coduo_x87emu.h"

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * Complete orientation transform bank.  The 0x138-byte Windows cluster is
 * byte-identical in all four authoritative images:
 *
 *   CoDUOMP.exe                 0x004506f0..0x00450828
 *   uo_cgame_mp_x86.dll        0x3004f4e0..0x3004f618
 *   uo_ui_mp_x86.dll           0x40007510..0x40007648
 *   uo_game_mp_x86.dll         0x20058d00..0x20058e38
 *
 * The Linux engine functions at 0x08087ab5, 0x08087b63, 0x08087bfe, and
 * 0x08087ca7 have the same instruction-level arithmetic as the game-module
 * functions at RVAs 0x0009474a, 0x000947f8, 0x00094893, and 0x0009493c.
 * Linux deliberately stores the origin-relative position deltas as binary32;
 * Windows keeps those three values live in x87 registers.  The platforms also
 * retain different addition orders, so each platform body remains whole.
 */

#if defined(WINDOWS_BEHAVIOR)

void OrientationPosToWorldPos(const orientation_t *orientation, const vec3_t position, vec3_t worldPosition)
{
    for (int32_t component = 0; component < 3; ++component) {
#if EMULATE_X87
        worldPosition[component] =
            x87f_store_f32(x87f_add(x87f_add(x87f_add(x87f_mul(x87f_load_f32(orientation->axis[2][component]), x87f_load_f32(position[2])),
                                                      x87f_mul(x87f_load_f32(orientation->axis[0][component]), x87f_load_f32(position[0]))),
                                             x87f_mul(x87f_load_f32(orientation->axis[1][component]), x87f_load_f32(position[1]))),
                                    x87f_load_f32(orientation->origin[component])));
#else
        worldPosition[component] = (float)((((long double)orientation->axis[2][component] * position[2] +
                                             (long double)orientation->axis[0][component] * position[0]) +
                                            (long double)orientation->axis[1][component] * position[1]) +
                                           (long double)orientation->origin[component]);
#endif
    }
}

void OrientationDirToWorldDir(const orientation_t *orientation, const vec3_t direction, vec3_t worldDirection)
{
    for (int32_t component = 0; component < 3; ++component) {
#if EMULATE_X87
        worldDirection[component] =
            x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(orientation->axis[2][component]), x87f_load_f32(direction[2])),
                                             x87f_mul(x87f_load_f32(orientation->axis[0][component]), x87f_load_f32(direction[0]))),
                                    x87f_mul(x87f_load_f32(orientation->axis[1][component]), x87f_load_f32(direction[1]))));
#else
        worldDirection[component] = (float)(((long double)orientation->axis[2][component] * direction[2] +
                                             (long double)orientation->axis[0][component] * direction[0]) +
                                            (long double)orientation->axis[1][component] * direction[1]);
#endif
    }
}

void OrientationPosFromWorldPos(const orientation_t *orientation, const vec3_t worldPosition, vec3_t position)
{
#if EMULATE_X87
    const x87f translated0 = x87f_sub(x87f_load_f32(worldPosition[0]), x87f_load_f32(orientation->origin[0]));
    const x87f translated1 = x87f_sub(x87f_load_f32(worldPosition[1]), x87f_load_f32(orientation->origin[1]));
    const x87f translated2 = x87f_sub(x87f_load_f32(worldPosition[2]), x87f_load_f32(orientation->origin[2]));

    for (int32_t component = 0; component < 3; ++component) {
        position[component] = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(orientation->axis[component][2]), translated2),
                                                               x87f_mul(x87f_load_f32(orientation->axis[component][1]), translated1)),
                                                      x87f_mul(x87f_load_f32(orientation->axis[component][0]), translated0)));
    }
#else
    const long double translated0 = (long double)worldPosition[0] - orientation->origin[0];
    const long double translated1 = (long double)worldPosition[1] - orientation->origin[1];
    const long double translated2 = (long double)worldPosition[2] - orientation->origin[2];

    for (int32_t component = 0; component < 3; ++component) {
        position[component] = (float)(((long double)orientation->axis[component][2] * translated2 +
                                       (long double)orientation->axis[component][1] * translated1) +
                                      (long double)orientation->axis[component][0] * translated0);
    }
#endif
}

void OrientationDirFromWorldDir(const orientation_t *orientation, const vec3_t worldDirection, vec3_t direction)
{
    for (int32_t component = 0; component < 3; ++component) {
#if EMULATE_X87
        direction[component] =
            x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(orientation->axis[component][2]), x87f_load_f32(worldDirection[2])),
                                             x87f_mul(x87f_load_f32(orientation->axis[component][0]), x87f_load_f32(worldDirection[0]))),
                                    x87f_mul(x87f_load_f32(orientation->axis[component][1]), x87f_load_f32(worldDirection[1]))));
#else
        direction[component] = (float)(((long double)orientation->axis[component][2] * worldDirection[2] +
                                        (long double)orientation->axis[component][0] * worldDirection[0]) +
                                       (long double)orientation->axis[component][1] * worldDirection[1]);
#endif
    }
}

#else

void OrientationPosToWorldPos(const orientation_t *orientation, const vec3_t position, vec3_t worldPosition)
{
    for (int32_t component = 0; component < 3; ++component) {
#if EMULATE_X87
        worldPosition[component] =
            x87f_store_f32(x87f_add(x87f_add(x87f_add(x87f_mul(x87f_load_f32(position[0]), x87f_load_f32(orientation->axis[0][component])),
                                                      x87f_load_f32(orientation->origin[component])),
                                             x87f_mul(x87f_load_f32(position[1]), x87f_load_f32(orientation->axis[1][component]))),
                                    x87f_mul(x87f_load_f32(position[2]), x87f_load_f32(orientation->axis[2][component]))));
#else
        worldPosition[component] =
            (float)((((long double)position[0] * orientation->axis[0][component] + (long double)orientation->origin[component]) +
                     (long double)position[1] * orientation->axis[1][component]) +
                    (long double)position[2] * orientation->axis[2][component]);
#endif
    }
}

void OrientationDirToWorldDir(const orientation_t *orientation, const vec3_t direction, vec3_t worldDirection)
{
    for (int32_t component = 0; component < 3; ++component) {
#if EMULATE_X87
        worldDirection[component] =
            x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(direction[0]), x87f_load_f32(orientation->axis[0][component])),
                                             x87f_mul(x87f_load_f32(direction[1]), x87f_load_f32(orientation->axis[1][component]))),
                                    x87f_mul(x87f_load_f32(direction[2]), x87f_load_f32(orientation->axis[2][component]))));
#else
        worldDirection[component] = (float)(((long double)direction[0] * orientation->axis[0][component] +
                                             (long double)direction[1] * orientation->axis[1][component]) +
                                            (long double)direction[2] * orientation->axis[2][component]);
#endif
    }
}

void OrientationPosFromWorldPos(const orientation_t *orientation, const vec3_t worldPosition, vec3_t position)
{
    float translated[3];

#if EMULATE_X87
    translated[0] = x87f_store_f32(x87f_sub(x87f_load_f32(worldPosition[0]), x87f_load_f32(orientation->origin[0])));
    translated[1] = x87f_store_f32(x87f_sub(x87f_load_f32(worldPosition[1]), x87f_load_f32(orientation->origin[1])));
    translated[2] = x87f_store_f32(x87f_sub(x87f_load_f32(worldPosition[2]), x87f_load_f32(orientation->origin[2])));

    for (int32_t component = 0; component < 3; ++component) {
        position[component] =
            x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(translated[0]), x87f_load_f32(orientation->axis[component][0])),
                                             x87f_mul(x87f_load_f32(translated[1]), x87f_load_f32(orientation->axis[component][1]))),
                                    x87f_mul(x87f_load_f32(translated[2]), x87f_load_f32(orientation->axis[component][2]))));
    }
#else
    translated[0] = (float)((long double)worldPosition[0] - orientation->origin[0]);
    translated[1] = (float)((long double)worldPosition[1] - orientation->origin[1]);
    translated[2] = (float)((long double)worldPosition[2] - orientation->origin[2]);

    for (int32_t component = 0; component < 3; ++component) {
        position[component] = (float)(((long double)translated[0] * orientation->axis[component][0] +
                                       (long double)translated[1] * orientation->axis[component][1]) +
                                      (long double)translated[2] * orientation->axis[component][2]);
    }
#endif
}

void OrientationDirFromWorldDir(const orientation_t *orientation, const vec3_t worldDirection, vec3_t direction)
{
    for (int32_t component = 0; component < 3; ++component) {
#if EMULATE_X87
        direction[component] =
            x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(worldDirection[0]), x87f_load_f32(orientation->axis[component][0])),
                                             x87f_mul(x87f_load_f32(worldDirection[1]), x87f_load_f32(orientation->axis[component][1]))),
                                    x87f_mul(x87f_load_f32(worldDirection[2]), x87f_load_f32(orientation->axis[component][2]))));
#else
        direction[component] = (float)(((long double)worldDirection[0] * orientation->axis[component][0] +
                                        (long double)worldDirection[1] * orientation->axis[component][1]) +
                                       (long double)worldDirection[2] * orientation->axis[component][2]);
#endif
    }
}

#endif
