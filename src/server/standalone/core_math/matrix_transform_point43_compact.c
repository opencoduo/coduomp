#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */

#include "core_math_private.h"

void MatrixTransformPoint43Compact(const vec3_t in,
                  const matrix43_t *matrix,
                  vec3_t out)
{
#if EMULATE_X87
    /* x87-faithful transcription (stock coduo_lnxded 0x080692a8): each output is
     * the 80-bit affine dot ((in0*a0 + in1*a1) + in2*a2) + origin, rounded to
     * float at the store. */
    for (int j = 0; j < 3; ++j) {
        out[j] = x87f_store_f32(x87f_add(
            x87f_add(x87f_add(x87f_mul(x87f_load_f32(in[0]),
                                       x87f_load_f32(matrix->axis[0][j])),
                              x87f_mul(x87f_load_f32(in[1]),
                                       x87f_load_f32(matrix->axis[1][j]))),
                     x87f_mul(x87f_load_f32(in[2]),
                              x87f_load_f32(matrix->axis[2][j]))),
            x87f_load_f32(matrix->origin[j])));
    }
#else
    out[0] = (((in[0] * matrix->axis[0][0]) +
               (in[1] * matrix->axis[1][0])) +
              (in[2] * matrix->axis[2][0])) +
             matrix->origin[0];
    out[1] = (((in[0] * matrix->axis[0][1]) +
               (in[1] * matrix->axis[1][1])) +
              (in[2] * matrix->axis[2][1])) +
             matrix->origin[1];
    out[2] = (((in[0] * matrix->axis[0][2]) +
               (in[1] * matrix->axis[1][2])) +
              (in[2] * matrix->axis[2][2])) +
             matrix->origin[2];
#endif
}
