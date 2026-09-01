#include "q_math.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#include "compat/coduo_x87emu.h"

/*
 * The four authoritative Windows bodies are byte-identical:
 *
 *   CoDUOMP.exe                 0x004333c0
 *   uo_cgame_mp_x86.dll        0x3004b520
 *   uo_ui_mp_x86.dll           0x400034f0
 *   uo_game_mp_x86.dll         0x20018570
 *
 * Windows folds the X output in Y,Z,X order and the remaining outputs in
 * X,Y,Z order under the process PC=53 policy. The two authoritative Linux
 * bodies are likewise byte-identical at coduo_lnxded 0x080692a8 and
 * game.mp.uo.i386.so RVA 0x0003ceeb, but fold every output in X,Y,Z order
 * under PC=64. Each output is stored to binary32 before the next lane begins,
 * preserving the original progressive behavior when input and output overlap.
 *
 * The supporting Mac cgame and game modules retain the exact
 * MatrixTransformVector43 name and the compact 0x30-byte layout.
 */
#if defined(WINDOWS_BEHAVIOR)
void MatrixTransformVector43(const vec3_t input,
                             const matrix43_t *matrix, vec3_t output)
{
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds 12(%1)\n\t"
                         "fmuls 4(%0)\n\t"
                         "flds 24(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 0(%0)\n\t"
                         "fmuls 0(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fadds 36(%1)\n\t"
                         "fstps 0(%2)\n\t"
                         "flds 4(%1)\n\t"
                         "fmuls 0(%0)\n\t"
                         "flds 16(%1)\n\t"
                         "fmuls 4(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 28(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fadds 40(%1)\n\t"
                         "fstps 4(%2)\n\t"
                         "flds 8(%1)\n\t"
                         "fmuls 0(%0)\n\t"
                         "flds 20(%1)\n\t"
                         "fmuls 4(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 32(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fadds 44(%1)\n\t"
                         "fstps 8(%2)"
                         :
                         : "r"(input), "r"(matrix), "r"(output)
                         : "st", "st(1)", "memory");
#else
    output[0] = (float)((((double)matrix->axis[1][0] * (double)input[1] +
                          (double)matrix->axis[2][0] * (double)input[2]) +
                         (double)matrix->axis[0][0] * (double)input[0]) +
                        (double)matrix->origin[0]);
    output[1] = (float)((((double)matrix->axis[0][1] * (double)input[0] +
                          (double)matrix->axis[1][1] * (double)input[1]) +
                         (double)matrix->axis[2][1] * (double)input[2]) +
                        (double)matrix->origin[1]);
    output[2] = (float)((((double)matrix->axis[0][2] * (double)input[0] +
                          (double)matrix->axis[1][2] * (double)input[1]) +
                         (double)matrix->axis[2][2] * (double)input[2]) +
                        (double)matrix->origin[2]);
#endif
}
#else
void MatrixTransformVector43(const vec3_t input,
                             const matrix43_t *matrix, vec3_t output)
{
#if EMULATE_X87
    output[0] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_add(x87f_mul(x87f_load_f32(input[0]),
                              x87f_load_f32(matrix->axis[0][0])),
                     x87f_mul(x87f_load_f32(input[1]),
                              x87f_load_f32(matrix->axis[1][0]))),
            x87f_mul(x87f_load_f32(input[2]),
                     x87f_load_f32(matrix->axis[2][0]))),
        x87f_load_f32(matrix->origin[0])));
    output[1] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_add(x87f_mul(x87f_load_f32(input[0]),
                              x87f_load_f32(matrix->axis[0][1])),
                     x87f_mul(x87f_load_f32(input[1]),
                              x87f_load_f32(matrix->axis[1][1]))),
            x87f_mul(x87f_load_f32(input[2]),
                     x87f_load_f32(matrix->axis[2][1]))),
        x87f_load_f32(matrix->origin[1])));
    output[2] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_add(x87f_mul(x87f_load_f32(input[0]),
                              x87f_load_f32(matrix->axis[0][2])),
                     x87f_mul(x87f_load_f32(input[1]),
                              x87f_load_f32(matrix->axis[1][2]))),
            x87f_mul(x87f_load_f32(input[2]),
                     x87f_load_f32(matrix->axis[2][2]))),
        x87f_load_f32(matrix->origin[2])));
#elif (defined(__i386__) || defined(__x86_64__)) && \
      (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds 0(%0)\n\t"
                         "fmuls 0(%1)\n\t"
                         "flds 4(%0)\n\t"
                         "fmuls 12(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 8(%0)\n\t"
                         "fmuls 24(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fadds 36(%1)\n\t"
                         "fstps 0(%2)\n\t"
                         "flds 0(%0)\n\t"
                         "fmuls 4(%1)\n\t"
                         "flds 4(%0)\n\t"
                         "fmuls 16(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 8(%0)\n\t"
                         "fmuls 28(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fadds 40(%1)\n\t"
                         "fstps 4(%2)\n\t"
                         "flds 0(%0)\n\t"
                         "fmuls 8(%1)\n\t"
                         "flds 4(%0)\n\t"
                         "fmuls 20(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 8(%0)\n\t"
                         "fmuls 32(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fadds 44(%1)\n\t"
                         "fstps 8(%2)"
                         :
                         : "r"(input), "r"(matrix), "r"(output)
                         : "st", "st(1)", "memory");
#else
    output[0] = (float)((((long double)input[0] * matrix->axis[0][0] +
                          (long double)input[1] * matrix->axis[1][0]) +
                         (long double)input[2] * matrix->axis[2][0]) +
                        (long double)matrix->origin[0]);
    output[1] = (float)((((long double)input[0] * matrix->axis[0][1] +
                          (long double)input[1] * matrix->axis[1][1]) +
                         (long double)input[2] * matrix->axis[2][1]) +
                        (long double)matrix->origin[1]);
    output[2] = (float)((((long double)input[0] * matrix->axis[0][2] +
                          (long double)input[1] * matrix->axis[1][2]) +
                         (long double)input[2] * matrix->axis[2][2]) +
                        (long double)matrix->origin[2]);
#endif
}
#endif
