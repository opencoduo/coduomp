#include "q_math.h"

#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"

/*
 * The four Windows bodies are instruction-identical apart from their load
 * addresses:
 *
 *   CoDUOMP.exe                 0x00434da0
 *   uo_cgame_mp_x86.dll        0x3004cf00
 *   uo_ui_mp_x86.dll           0x40004f10
 *   uo_game_mp_x86.dll         0x20019f50
 *
 * Linux coduo_lnxded 0x0806b113 and game.mp.uo.i386.so RVA 0x0003f024 use
 * the same two binary32 FSINCOS result pairs and the same three products.  The
 * compiler loads radius first on Linux and a cosine first on Windows.  Since
 * the first product contains only two binary32 operands, it is exact at both
 * original x87 precision settings; the finite binary32 results agree without
 * a platform behavior fork.
 */
void VectorPolar(vec3_t output, float radius, float angle)
{
    float firstSine;
    float firstCosine;
    float secondSine;
    float secondCosine;

    coduo_x87_sincosf(angle, &firstSine, &firstCosine);
    coduo_x87_sincosf(angle, &secondSine, &secondCosine);

#if EMULATE_X87
    output[0] = x87f_store_f32(x87f_mul(
        x87f_mul(x87f_load_f32(secondCosine),
                 x87f_load_f32(firstCosine)),
        x87f_load_f32(radius)));
    output[1] = x87f_store_f32(x87f_mul(
        x87f_mul(x87f_load_f32(secondCosine),
                 x87f_load_f32(firstSine)),
        x87f_load_f32(radius)));
    output[2] = x87f_store_f32(
        x87f_mul(x87f_load_f32(secondSine), x87f_load_f32(radius)));
#else
    output[0] = (float)((long double)secondCosine *
                        (long double)firstCosine * (long double)radius);
    output[1] = (float)((long double)secondCosine *
                        (long double)firstSine * (long double)radius);
    output[2] =
        (float)((long double)secondSine * (long double)radius);
#endif
}
