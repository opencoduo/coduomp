#include "q_math.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#include "compat/coduo_x87emu.h"

/*
 * LocalMatrixTransformVector43 is the client-family name for the padded
 * 0x40-byte DObjSkelMat transform. The authoritative Windows bodies are
 * byte-identical:
 *
 *   CoDUOMP.exe                 0x00433410 and 0x0045d410
 *   uo_cgame_mp_x86.dll        0x3004b570
 *   uo_ui_mp_x86.dll           0x40003540
 *
 * The Windows game-module body at 0x200185c0 has the same machine code, but
 * the exact Linux game-module symbol identifies the server-family source name
 * as DObjSkelMatrixTransformVector43; that entry point is retained separately.
 * Windows folds X as Z,Y,X and Y/Z as X,Z,Y under PC=53. The Linux operation
 * graph folds every lane X,Y,Z under PC=64.
 */
#if defined(WINDOWS_BEHAVIOR)
void LocalMatrixTransformVector43(const vec3_t input, const float matrix[16],
                                  vec3_t output)
{
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds 32(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "flds 16(%1)\n\t"
                         "fmuls 4(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 0(%0)\n\t"
                         "fmuls 0(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fadds 48(%1)\n\t"
                         "fstps 0(%2)\n\t"
                         "flds 4(%1)\n\t"
                         "fmuls 0(%0)\n\t"
                         "flds 36(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 20(%1)\n\t"
                         "fmuls 4(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fadds 52(%1)\n\t"
                         "fstps 4(%2)\n\t"
                         "flds 8(%1)\n\t"
                         "fmuls 0(%0)\n\t"
                         "flds 40(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 24(%1)\n\t"
                         "fmuls 4(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fadds 56(%1)\n\t"
                         "fstps 8(%2)"
                         :
                         : "r"(input), "r"(matrix), "r"(output)
                         : "st", "st(1)", "memory");
#else
    output[0] = (float)((((double)matrix[8] * (double)input[2] +
                          (double)matrix[4] * (double)input[1]) +
                         (double)matrix[0] * (double)input[0]) +
                        (double)matrix[12]);
    output[1] = (float)((((double)matrix[1] * (double)input[0] +
                          (double)matrix[9] * (double)input[2]) +
                         (double)matrix[5] * (double)input[1]) +
                        (double)matrix[13]);
    output[2] = (float)((((double)matrix[2] * (double)input[0] +
                          (double)matrix[10] * (double)input[2]) +
                         (double)matrix[6] * (double)input[1]) +
                        (double)matrix[14]);
#endif
}
#else
void LocalMatrixTransformVector43(const vec3_t input, const float matrix[16],
                                  vec3_t output)
{
#if EMULATE_X87
    output[0] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_add(x87f_mul(x87f_load_f32(input[0]),
                              x87f_load_f32(matrix[0])),
                     x87f_mul(x87f_load_f32(input[1]),
                              x87f_load_f32(matrix[4]))),
            x87f_mul(x87f_load_f32(input[2]),
                     x87f_load_f32(matrix[8]))),
        x87f_load_f32(matrix[12])));
    output[1] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_add(x87f_mul(x87f_load_f32(input[0]),
                              x87f_load_f32(matrix[1])),
                     x87f_mul(x87f_load_f32(input[1]),
                              x87f_load_f32(matrix[5]))),
            x87f_mul(x87f_load_f32(input[2]),
                     x87f_load_f32(matrix[9]))),
        x87f_load_f32(matrix[13])));
    output[2] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_add(x87f_mul(x87f_load_f32(input[0]),
                              x87f_load_f32(matrix[2])),
                     x87f_mul(x87f_load_f32(input[1]),
                              x87f_load_f32(matrix[6]))),
            x87f_mul(x87f_load_f32(input[2]),
                     x87f_load_f32(matrix[10]))),
        x87f_load_f32(matrix[14])));
#elif (defined(__i386__) || defined(__x86_64__)) && \
      (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds 0(%0)\n\t"
                         "fmuls 0(%1)\n\t"
                         "flds 4(%0)\n\t"
                         "fmuls 16(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 8(%0)\n\t"
                         "fmuls 32(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fadds 48(%1)\n\t"
                         "fstps 0(%2)\n\t"
                         "flds 0(%0)\n\t"
                         "fmuls 4(%1)\n\t"
                         "flds 4(%0)\n\t"
                         "fmuls 20(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 8(%0)\n\t"
                         "fmuls 36(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fadds 52(%1)\n\t"
                         "fstps 4(%2)\n\t"
                         "flds 0(%0)\n\t"
                         "fmuls 8(%1)\n\t"
                         "flds 4(%0)\n\t"
                         "fmuls 24(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 8(%0)\n\t"
                         "fmuls 40(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fadds 56(%1)\n\t"
                         "fstps 8(%2)"
                         :
                         : "r"(input), "r"(matrix), "r"(output)
                         : "st", "st(1)", "memory");
#else
    output[0] = (float)((((long double)input[0] * matrix[0] +
                          (long double)input[1] * matrix[4]) +
                         (long double)input[2] * matrix[8]) +
                        (long double)matrix[12]);
    output[1] = (float)((((long double)input[0] * matrix[1] +
                          (long double)input[1] * matrix[5]) +
                         (long double)input[2] * matrix[9]) +
                        (long double)matrix[13]);
    output[2] = (float)((((long double)input[0] * matrix[2] +
                          (long double)input[1] * matrix[6]) +
                         (long double)input[2] * matrix[10]) +
                        (long double)matrix[14]);
#endif
}
#endif
