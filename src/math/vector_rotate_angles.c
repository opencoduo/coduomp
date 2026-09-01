#include "q_math.h"

#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * The four Windows VectorRotateAngles bodies are instruction-identical apart
 * from relocated constants and the sin/cos target:
 *
 *   CoDUOMP.exe                 0x00434c30
 *   uo_cgame_mp_x86.dll        0x3004cd90
 *   uo_ui_mp_x86.dll           0x40004da0
 *   uo_game_mp_x86.dll         0x20019de0
 *
 * Linux coduo_lnxded 0x0806af24 and game.mp.uo.i386.so RVA 0x0003ee0e
 * retain the same lane table, branch behavior, sin/cos call, binary32 stores,
 * and output.  There is one genuine source-level difference: Windows
 * multiplies by binary32 pi before dividing by binary64 180, whereas Linux
 * multiplies by binary64 pi.  EMULATE_X87 is independent of that behavior
 * selection and preserves the selected x87 operation graph on non-x87 hosts.
 */
void VectorRotateAngles(const vec3_t input, const vec3_t angles,
                        vec3_t output)
{
    const int32_t componentPairs[3][2] = {
        {1, 2},
        {2, 0},
        {0, 1}
    };
    vec3_t previous = {input[0], input[1], input[2]};
    vec3_t rotated = {input[0], input[1], input[2]};

    for (int32_t axis = 0; axis < 3; ++axis) {
        if (angles[axis] != 0.0f) {
            double radians;
            double sine;
            double cosine;

#if EMULATE_X87
#if defined(WINDOWS_BEHAVIOR)
            radians = x87f_store_f64(x87f_div(
                x87f_mul(x87f_load_f32(angles[axis]),
                         x87f_load_f32(3.1415927410125732f)),
                x87f_load_f64(180.0)));
#else
            radians = x87f_store_f64(x87f_div(
                x87f_mul(x87f_load_f32(angles[axis]),
                         x87f_load_f64(3.141592653589793)),
                x87f_load_f64(180.0)));
#endif
#else
#if defined(WINDOWS_BEHAVIOR)
            radians = (double)(
                (long double)angles[axis] *
                (long double)3.1415927410125732f / 180.0L);
#else
            radians = (double)(
                (long double)angles[axis] *
                (long double)3.141592653589793 / 180.0L);
#endif
#endif
            coduo_x87_sincos(radians, &sine, &cosine);

            const int32_t first = componentPairs[axis][0];
            const int32_t second = componentPairs[axis][1];
#if EMULATE_X87
            rotated[first] = x87f_store_f32(x87f_sub(
                x87f_mul(x87f_load_f32(previous[first]),
                         x87f_load_f64(cosine)),
                x87f_mul(x87f_load_f32(previous[second]),
                         x87f_load_f64(sine))));
            rotated[second] = x87f_store_f32(x87f_add(
                x87f_mul(x87f_load_f32(previous[second]),
                         x87f_load_f64(cosine)),
                x87f_mul(x87f_load_f32(previous[first]),
                         x87f_load_f64(sine))));
#else
            rotated[first] = (float)(
                (long double)previous[first] * (long double)cosine -
                (long double)previous[second] * (long double)sine);
            rotated[second] = (float)(
                (long double)previous[second] * (long double)cosine +
                (long double)previous[first] * (long double)sine);
#endif
        }

        previous[0] = rotated[0];
        previous[1] = rotated[1];
        previous[2] = rotated[2];
    }

    output[0] = rotated[0];
    output[1] = rotated[1];
    output[2] = rotated[2];
}

/*
 * The four Windows bodies are byte-identical and the two Linux bodies retain
 * the same three binary32 subtractions, call, and binary32 additions:
 * CoDUOMP.exe 0x00434d50, cgame 0x3004ceb0, UI 0x40004ec0, game
 * 0x20019f00, coduo_lnxded 0x0806b08c, and game-module RVA 0x0003ef8d.
 */
void VectorRotateAnglesAroundPoint(const vec3_t point,
                                   const vec3_t angles,
                                   const vec3_t origin,
                                   vec3_t output)
{
    vec3_t relative;
    vec3_t rotated;

    relative[0] = point[0] - origin[0];
    relative[1] = point[1] - origin[1];
    relative[2] = point[2] - origin[2];
    VectorRotateAngles(relative, angles, rotated);
    output[0] = rotated[0] + origin[0];
    output[1] = rotated[1] + origin[1];
    output[2] = rotated[2] + origin[2];
}
