#include <stdlib.h>

#include "compat/crt/random_compat.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */

#include "core_math_private.h"

void gunrandom(float *x, float *y)
{
    float angleDegrees;
    float radius;
    float sinValue;
    float cosValue;

    /* Angle and radius consume normalized samples in either server mode. */
#if EMULATE_X87
    /* Keep the remaining angle and radius products in the emulated x87 graph. */
    angleDegrees = x87f_store_f32(x87f_mul(x87f_load_f64(coduo_server_rand_unit()), x87f_load_f32(360.0f)));
    radius = x87f_store_f32(x87f_load_f64(coduo_server_rand_unit()));
    BG_SinCos(x87f_store_f32(x87f_div(x87f_mul(x87f_load_f32(angleDegrees), x87f_load_f64(3.141592653589793)), x87f_load_f64(180.0))),
              &sinValue, &cosValue);
    *x = x87f_store_f32(x87f_mul(x87f_load_f32(radius), x87f_load_f32(cosValue)));
    *y = x87f_store_f32(x87f_mul(x87f_load_f32(radius), x87f_load_f32(sinValue)));
#else
    angleDegrees = (float)(coduo_server_rand_unit() * 360.0);
    radius = (float)coduo_server_rand_unit();
    BG_SinCos((float)(((double)angleDegrees * 3.141592653589793) / 180.0), &sinValue, &cosValue);
    *x = radius * cosValue;
    *y = radius * sinValue;
#endif
}
