#include "q_math.h"

#include "compat/coduo_x87emu.h"

#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * The four Windows bodies are instruction-identical apart from the relocated
 * binary32 pi/180 constant:
 *
 *   CoDUOMP.exe                 0x00433550
 *   uo_cgame_mp_x86.dll        0x3004b6b0
 *   uo_ui_mp_x86.dll           0x40003680
 *   uo_game_mp_x86.dll         0x20018700
 *
 * The Linux bodies at coduo_lnxded 0x080695a6 and game.mp.uo.i386.so RVA
 * 0x0003d1e9 instead multiply by a binary64 pi/180 constant. Both platforms
 * spill the radians and each rotated component to binary32, and both retain
 * the original X lane until after calculating the new Y lane. The complete
 * platform bodies preserve the genuine constant-width difference;
 * EMULATE_X87 remains an independent host-arithmetic choice in each body.
 */
#if defined(WINDOWS_BEHAVIOR)
void VectorAngleMultiply(vec2_t vector, float angleDegrees)
{
    float sine;
    float cosine;
    float radians;
    float rotatedX;

#if EMULATE_X87
    radians = x87f_store_f32(x87f_mul(x87f_load_f32(angleDegrees), x87f_load_f32(0.017453292f)));
    BG_SinCos(radians, &sine, &cosine);
    rotatedX = x87f_store_f32(
        x87f_sub(x87f_mul(x87f_load_f32(vector[0]), x87f_load_f32(cosine)), x87f_mul(x87f_load_f32(vector[1]), x87f_load_f32(sine))));
    vector[1] = x87f_store_f32(
        x87f_add(x87f_mul(x87f_load_f32(vector[0]), x87f_load_f32(sine)), x87f_mul(x87f_load_f32(vector[1]), x87f_load_f32(cosine))));
#else
    radians = (float)((long double)angleDegrees * 0.017453292f);
    BG_SinCos(radians, &sine, &cosine);
    rotatedX = (float)((long double)vector[0] * cosine - (long double)vector[1] * sine);
    vector[1] = (float)((long double)vector[0] * sine + (long double)vector[1] * cosine);
#endif
    memcpy(&vector[0], &rotatedX, sizeof(vector[0]));
}
#else
void VectorAngleMultiply(vec2_t vector, float angleDegrees)
{
    float sine;
    float cosine;
    float radians;
    float rotatedX;

#if EMULATE_X87
    radians = x87f_store_f32(x87f_mul(x87f_load_f32(angleDegrees), x87f_load_f64(0.017453292519943295)));
    BG_SinCos(radians, &sine, &cosine);
    rotatedX = x87f_store_f32(
        x87f_sub(x87f_mul(x87f_load_f32(vector[0]), x87f_load_f32(cosine)), x87f_mul(x87f_load_f32(vector[1]), x87f_load_f32(sine))));
    vector[1] = x87f_store_f32(
        x87f_add(x87f_mul(x87f_load_f32(vector[1]), x87f_load_f32(cosine)), x87f_mul(x87f_load_f32(vector[0]), x87f_load_f32(sine))));
#else
    radians = (float)((long double)angleDegrees * 0.017453292519943295);
    BG_SinCos(radians, &sine, &cosine);
    rotatedX = (float)((long double)vector[0] * cosine - (long double)vector[1] * sine);
    vector[1] = (float)((long double)vector[1] * cosine + (long double)vector[0] * sine);
#endif
    memcpy(&vector[0], &rotatedX, sizeof(vector[0]));
}
#endif
