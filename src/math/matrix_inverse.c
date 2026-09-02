#include "q_math.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#include "compat/coduo_x87emu.h"

/*
 * The authoritative Windows bodies are instruction-identical except for the
 * relocated address of their binary32 1.0 constant:
 *
 *   CoDUOMP.exe                 0x00432b60
 *   uo_cgame_mp_x86.dll        0x3004acc0
 *   uo_ui_mp_x86.dll           0x40002c90
 *   uo_game_mp_x86.dll         0x20017d10
 *
 * The Linux engine and game-module bodies at 0x080688ac and RVA 0x0003c4b8
 * retain the same adjugate and determinant under their PC=64 operation graph.
 * Windows stores the first cofactor to binary32 before using it in the
 * determinant; Linux keeps that minor live until its determinant term is
 * formed.  That boundary, rather than the engine's FLD1 versus the PIC game
 * module's load of the same exact binary32 1.0, requires the behavior split.
 * The supporting Mac executable retains the canonical MatrixInverse symbol at
 * PEF address 0x000dc390, but uses fused binary32 PowerPC arithmetic and
 * snapshots every input lane before output; it is corroborating rather than
 * authoritative behavior.
 *
 * Both x86 families intentionally lack a singular-matrix guard.  They also
 * reload source lanes after earlier destination stores, so partially or fully
 * overlapping matrices retain the original progressive overwrite behavior.
 */
#if defined(WINDOWS_BEHAVIOR)
void MatrixInverse(axis_t input, axis_t output)
{
    /* Windows stores this first cofactor to binary32 before using it in both
     * the determinant and output[0][0]. */
    const float cofactor00 = (float)(
        (double)input[1][1] * (double)input[2][2] -
        (double)input[2][1] * (double)input[1][2]);
    const float determinant = (float)(
        ((double)cofactor00 * (double)input[0][0] -
         ((double)input[0][1] * (double)input[2][2] -
          (double)input[2][1] * (double)input[0][2]) *
             (double)input[1][0]) +
        ((double)input[0][1] * (double)input[1][2] -
         (double)input[0][2] * (double)input[1][1]) *
            (double)input[2][0]);
    const float inverseDeterminant =
        (float)((double)1.0f / (double)determinant);

    output[0][0] = (float)((double)cofactor00 * inverseDeterminant);
    output[0][1] = (float)(-
        (((double)input[0][1] * (double)input[2][2] -
          (double)input[2][1] * (double)input[0][2]) *
         inverseDeterminant));
    output[0][2] = (float)(
        ((double)input[0][1] * (double)input[1][2] -
         (double)input[0][2] * (double)input[1][1]) *
        inverseDeterminant);
    output[1][0] = (float)(-
        (((double)input[1][0] * (double)input[2][2] -
          (double)input[2][0] * (double)input[1][2]) *
         inverseDeterminant));
    output[1][1] = (float)(
        ((double)input[0][0] * (double)input[2][2] -
         (double)input[0][2] * (double)input[2][0]) *
        inverseDeterminant);
    output[1][2] = (float)(-
        (((double)input[0][0] * (double)input[1][2] -
          (double)input[0][2] * (double)input[1][0]) *
         inverseDeterminant));
    output[2][0] = (float)(
        ((double)input[2][1] * (double)input[1][0] -
         (double)input[2][0] * (double)input[1][1]) *
        inverseDeterminant);
    output[2][1] = (float)(-
        (((double)input[2][1] * (double)input[0][0] -
          (double)input[0][1] * (double)input[2][0]) *
         inverseDeterminant));
    output[2][2] = (float)(
        ((double)input[1][1] * (double)input[0][0] -
         (double)input[0][1] * (double)input[1][0]) *
        inverseDeterminant);
}
#else
#if EMULATE_X87
#define MATRIX_INVERSE_MINOR(a, b, c, d)                                      \
    x87f_sub(x87f_mul(x87f_load_f32(a), x87f_load_f32(b)),                   \
             x87f_mul(x87f_load_f32(c), x87f_load_f32(d)))
#endif

void MatrixInverse(axis_t input, axis_t output)
{
#if EMULATE_X87
    const x87f determinantTerm0 = x87f_mul(
        MATRIX_INVERSE_MINOR(input[2][2], input[1][1],
                             input[2][1], input[1][2]),
        x87f_load_f32(input[0][0]));
    const x87f determinantTerm1 = x87f_mul(
        MATRIX_INVERSE_MINOR(input[2][2], input[0][1],
                             input[2][1], input[0][2]),
        x87f_load_f32(input[1][0]));
    const x87f determinantTerm2 = x87f_mul(
        MATRIX_INVERSE_MINOR(input[1][2], input[0][1],
                             input[1][1], input[0][2]),
        x87f_load_f32(input[2][0]));
    const float determinant = x87f_store_f32(x87f_add(
        x87f_sub(determinantTerm0, determinantTerm1), determinantTerm2));
    const float inverseDeterminant = x87f_store_f32(
        x87f_div(x87f_load_f32(1.0f), x87f_load_f32(determinant)));

    output[0][0] = x87f_store_f32(x87f_mul(
        MATRIX_INVERSE_MINOR(input[2][2], input[1][1],
                             input[2][1], input[1][2]),
        x87f_load_f32(inverseDeterminant)));
    output[0][1] = x87f_store_f32(x87f_mul(
        x87f_neg(MATRIX_INVERSE_MINOR(input[2][2], input[0][1],
                                      input[2][1], input[0][2])),
        x87f_load_f32(inverseDeterminant)));
    output[0][2] = x87f_store_f32(x87f_mul(
        MATRIX_INVERSE_MINOR(input[1][2], input[0][1],
                             input[1][1], input[0][2]),
        x87f_load_f32(inverseDeterminant)));
    output[1][0] = x87f_store_f32(x87f_mul(
        x87f_neg(MATRIX_INVERSE_MINOR(input[2][2], input[1][0],
                                      input[2][0], input[1][2])),
        x87f_load_f32(inverseDeterminant)));
    output[1][1] = x87f_store_f32(x87f_mul(
        MATRIX_INVERSE_MINOR(input[2][2], input[0][0],
                             input[2][0], input[0][2]),
        x87f_load_f32(inverseDeterminant)));
    output[1][2] = x87f_store_f32(x87f_mul(
        x87f_neg(MATRIX_INVERSE_MINOR(input[1][2], input[0][0],
                                      input[1][0], input[0][2])),
        x87f_load_f32(inverseDeterminant)));
    output[2][0] = x87f_store_f32(x87f_mul(
        MATRIX_INVERSE_MINOR(input[2][1], input[1][0],
                             input[2][0], input[1][1]),
        x87f_load_f32(inverseDeterminant)));
    output[2][1] = x87f_store_f32(x87f_mul(
        x87f_neg(MATRIX_INVERSE_MINOR(input[2][1], input[0][0],
                                      input[2][0], input[0][1])),
        x87f_load_f32(inverseDeterminant)));
    output[2][2] = x87f_store_f32(x87f_mul(
        MATRIX_INVERSE_MINOR(input[1][1], input[0][0],
                             input[1][0], input[0][1]),
        x87f_load_f32(inverseDeterminant)));
#else
    const float determinant =
        ((input[2][2] * input[1][1] - input[2][1] * input[1][2]) *
             input[0][0] -
         (input[2][2] * input[0][1] - input[2][1] * input[0][2]) *
             input[1][0]) +
        (input[1][2] * input[0][1] - input[1][1] * input[0][2]) *
            input[2][0];
    const float inverseDeterminant = 1.0f / determinant;

    output[0][0] =
        (input[2][2] * input[1][1] - input[2][1] * input[1][2]) *
        inverseDeterminant;
    output[0][1] =
        -(input[2][2] * input[0][1] - input[2][1] * input[0][2]) *
        inverseDeterminant;
    output[0][2] =
        (input[1][2] * input[0][1] - input[1][1] * input[0][2]) *
        inverseDeterminant;
    output[1][0] =
        -(input[2][2] * input[1][0] - input[2][0] * input[1][2]) *
        inverseDeterminant;
    output[1][1] =
        (input[2][2] * input[0][0] - input[2][0] * input[0][2]) *
        inverseDeterminant;
    output[1][2] =
        -(input[1][2] * input[0][0] - input[1][0] * input[0][2]) *
        inverseDeterminant;
    output[2][0] =
        (input[2][1] * input[1][0] - input[2][0] * input[1][1]) *
        inverseDeterminant;
    output[2][1] =
        -(input[2][1] * input[0][0] - input[2][0] * input[0][1]) *
        inverseDeterminant;
    output[2][2] =
        (input[1][1] * input[0][0] - input[1][0] * input[0][1]) *
        inverseDeterminant;
#endif
}

#if EMULATE_X87
#undef MATRIX_INVERSE_MINOR
#endif
#endif
