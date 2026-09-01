#include "q_math.h"

#include <stdint.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#include "compat/coduo_x87emu.h"

/*
 * The four authoritative Windows bodies are byte-identical except for their
 * image-local binary32 1.0 address:
 *
 *   CoDUOMP.exe                 0x00432d30
 *   uo_cgame_mp_x86.dll        0x3004ae90
 *   uo_ui_mp_x86.dll           0x40002e60
 *   uo_game_mp_x86.dll         0x20017ee0
 *
 * Linux retains the same column extraction, minors, adjugate, determinant,
 * and final scaling at coduo_lnxded 0x08068b35 and game.mp.uo.i386.so RVA
 * 0x0003c76d.  Their x87 sequences agree except for FLD1 in the engine versus
 * a load of the same exact binary32 1.0 in the PIC module.  The supporting Mac
 * executable retains MatrixInverse44 at PEF
 * address 0x000dbeb0 with the same formula but fused binary32 PowerPC
 * arithmetic.  All original bodies intentionally omit a singular-matrix
 * guard and snapshot the complete input before writing output, making
 * identical or overlapping input/output storage safe.
 *
 * Windows keeps each positive and negative three-term cofactor group live and
 * performs one final subtraction before the binary32 store.  Linux stores the
 * positive group to binary32, reloads it, then subtracts the negative group.
 * Their determinant fold orders also differ.  Keep complete platform bodies.
 */
#if defined(WINDOWS_BEHAVIOR)
#define MATRIX_INVERSE44_PRODUCT(a, b) ((double)(a) * (double)(b))
#define MATRIX_INVERSE44_SUM3(a, b, c) (((a) + (b)) + (c))

void MatrixInverse44(float input[4][4], float output[4][4])
{
    float source[16];
    float products[12];
    float determinant;
    int32_t index;

    for (index = 0; index < 4; ++index) {
        memcpy(&source[index], &input[index][0], sizeof(source[index]));
        memcpy(&source[index + 4], &input[index][1], sizeof(source[index]));
        memcpy(&source[index + 8], &input[index][2], sizeof(source[index]));
        memcpy(&source[index + 12], &input[index][3], sizeof(source[index]));
    }

    products[0] = source[10] * source[15];
    products[1] = source[11] * source[14];
    products[2] = source[9] * source[15];
    products[3] = source[11] * source[13];
    products[4] = source[9] * source[14];
    products[5] = source[10] * source[13];
    products[6] = source[8] * source[15];
    products[7] = source[11] * source[12];
    products[8] = source[8] * source[14];
    products[9] = source[10] * source[12];
    products[10] = source[8] * source[13];
    products[11] = source[9] * source[12];

    output[0][0] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[4], source[7]),
            MATRIX_INVERSE44_PRODUCT(products[3], source[6]),
            MATRIX_INVERSE44_PRODUCT(products[0], source[5])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[5], source[7]),
            MATRIX_INVERSE44_PRODUCT(products[2], source[6]),
            MATRIX_INVERSE44_PRODUCT(products[1], source[5])));
    output[0][1] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[9], source[7]),
            MATRIX_INVERSE44_PRODUCT(products[6], source[6]),
            MATRIX_INVERSE44_PRODUCT(products[1], source[4])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[8], source[7]),
            MATRIX_INVERSE44_PRODUCT(products[7], source[6]),
            MATRIX_INVERSE44_PRODUCT(products[0], source[4])));
    output[0][2] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[10], source[7]),
            MATRIX_INVERSE44_PRODUCT(products[7], source[5]),
            MATRIX_INVERSE44_PRODUCT(products[2], source[4])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[11], source[7]),
            MATRIX_INVERSE44_PRODUCT(products[6], source[5]),
            MATRIX_INVERSE44_PRODUCT(products[3], source[4])));
    output[0][3] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[11], source[6]),
            MATRIX_INVERSE44_PRODUCT(products[8], source[5]),
            MATRIX_INVERSE44_PRODUCT(products[5], source[4])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[10], source[6]),
            MATRIX_INVERSE44_PRODUCT(products[9], source[5]),
            MATRIX_INVERSE44_PRODUCT(products[4], source[4])));
    output[1][0] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[5], source[3]),
            MATRIX_INVERSE44_PRODUCT(products[2], source[2]),
            MATRIX_INVERSE44_PRODUCT(products[1], source[1])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[4], source[3]),
            MATRIX_INVERSE44_PRODUCT(products[3], source[2]),
            MATRIX_INVERSE44_PRODUCT(products[0], source[1])));
    output[1][1] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[8], source[3]),
            MATRIX_INVERSE44_PRODUCT(products[0], source[0]),
            MATRIX_INVERSE44_PRODUCT(products[7], source[2])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[9], source[3]),
            MATRIX_INVERSE44_PRODUCT(products[1], source[0]),
            MATRIX_INVERSE44_PRODUCT(products[6], source[2])));
    output[1][2] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[11], source[3]),
            MATRIX_INVERSE44_PRODUCT(products[3], source[0]),
            MATRIX_INVERSE44_PRODUCT(products[6], source[1])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[10], source[3]),
            MATRIX_INVERSE44_PRODUCT(products[2], source[0]),
            MATRIX_INVERSE44_PRODUCT(products[7], source[1])));
    output[1][3] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[4], source[0]),
            MATRIX_INVERSE44_PRODUCT(products[10], source[2]),
            MATRIX_INVERSE44_PRODUCT(products[9], source[1])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[5], source[0]),
            MATRIX_INVERSE44_PRODUCT(products[11], source[2]),
            MATRIX_INVERSE44_PRODUCT(products[8], source[1])));

    products[0] = source[2] * source[7];
    products[1] = source[3] * source[6];
    products[2] = source[1] * source[7];
    products[3] = source[3] * source[5];
    products[4] = source[1] * source[6];
    products[5] = source[2] * source[5];
    products[6] = source[0] * source[7];
    products[7] = source[3] * source[4];
    products[8] = source[0] * source[6];
    products[9] = source[2] * source[4];
    products[10] = source[0] * source[5];
    products[11] = source[1] * source[4];

    output[2][0] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[3], source[14]),
            MATRIX_INVERSE44_PRODUCT(products[0], source[13]),
            MATRIX_INVERSE44_PRODUCT(products[4], source[15])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[1], source[13]),
            MATRIX_INVERSE44_PRODUCT(products[2], source[14]),
            MATRIX_INVERSE44_PRODUCT(products[5], source[15])));
    output[2][1] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[1], source[12]),
            MATRIX_INVERSE44_PRODUCT(products[6], source[14]),
            MATRIX_INVERSE44_PRODUCT(products[9], source[15])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[7], source[14]),
            MATRIX_INVERSE44_PRODUCT(products[0], source[12]),
            MATRIX_INVERSE44_PRODUCT(products[8], source[15])));
    output[2][2] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[7], source[13]),
            MATRIX_INVERSE44_PRODUCT(products[2], source[12]),
            MATRIX_INVERSE44_PRODUCT(products[10], source[15])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[3], source[12]),
            MATRIX_INVERSE44_PRODUCT(products[6], source[13]),
            MATRIX_INVERSE44_PRODUCT(products[11], source[15])));
    output[2][3] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[5], source[12]),
            MATRIX_INVERSE44_PRODUCT(products[8], source[13]),
            MATRIX_INVERSE44_PRODUCT(products[11], source[14])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[4], source[12]),
            MATRIX_INVERSE44_PRODUCT(products[9], source[13]),
            MATRIX_INVERSE44_PRODUCT(products[10], source[14])));
    output[3][0] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[5], source[11]),
            MATRIX_INVERSE44_PRODUCT(products[1], source[9]),
            MATRIX_INVERSE44_PRODUCT(products[2], source[10])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[4], source[11]),
            MATRIX_INVERSE44_PRODUCT(products[0], source[9]),
            MATRIX_INVERSE44_PRODUCT(products[3], source[10])));
    output[3][1] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[8], source[11]),
            MATRIX_INVERSE44_PRODUCT(products[0], source[8]),
            MATRIX_INVERSE44_PRODUCT(products[7], source[10])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[9], source[11]),
            MATRIX_INVERSE44_PRODUCT(products[1], source[8]),
            MATRIX_INVERSE44_PRODUCT(products[6], source[10])));
    output[3][2] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[3], source[8]),
            MATRIX_INVERSE44_PRODUCT(products[6], source[9]),
            MATRIX_INVERSE44_PRODUCT(products[11], source[11])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[7], source[9]),
            MATRIX_INVERSE44_PRODUCT(products[2], source[8]),
            MATRIX_INVERSE44_PRODUCT(products[10], source[11])));
    output[3][3] = (float)(
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[4], source[8]),
            MATRIX_INVERSE44_PRODUCT(products[9], source[9]),
            MATRIX_INVERSE44_PRODUCT(products[10], source[10])) -
        MATRIX_INVERSE44_SUM3(
            MATRIX_INVERSE44_PRODUCT(products[5], source[8]),
            MATRIX_INVERSE44_PRODUCT(products[8], source[9]),
            MATRIX_INVERSE44_PRODUCT(products[11], source[10])));

    determinant = (float)(
        ((MATRIX_INVERSE44_PRODUCT(source[2], output[0][2]) +
          MATRIX_INVERSE44_PRODUCT(source[3], output[0][3])) +
         MATRIX_INVERSE44_PRODUCT(source[0], output[0][0])) +
        MATRIX_INVERSE44_PRODUCT(source[1], output[0][1]));
    determinant = (float)((double)1.0f / (double)determinant);
    for (index = 0; index < 16; ++index) {
        output[index / 4][index % 4] =
            (float)((double)output[index / 4][index % 4] * determinant);
    }
}

#undef MATRIX_INVERSE44_SUM3
#undef MATRIX_INVERSE44_PRODUCT
#else
#if EMULATE_X87
#define MATRIX_INVERSE44_DOT3(a, b, c, d, e, f)                              \
    x87f_add(x87f_add(x87f_mul(x87f_load_f32(a), x87f_load_f32(b)),          \
                      x87f_mul(x87f_load_f32(c), x87f_load_f32(d))),         \
             x87f_mul(x87f_load_f32(e), x87f_load_f32(f)))
#define MATRIX_INVERSE44_SET(dst, a, b, c, d, e, f)                           \
    (dst) = x87f_store_f32(MATRIX_INVERSE44_DOT3(a, b, c, d, e, f))
#define MATRIX_INVERSE44_SUB(dst, a, b, c, d, e, f)                           \
    (dst) = x87f_store_f32(                                                   \
        x87f_sub(x87f_load_f32(dst), MATRIX_INVERSE44_DOT3(a, b, c, d, e, f)))
#else
#define MATRIX_INVERSE44_SET(dst, a, b, c, d, e, f)                           \
    (dst) = (((a) * (b)) + ((c) * (d))) + ((e) * (f))
#define MATRIX_INVERSE44_SUB(dst, a, b, c, d, e, f)                           \
    (dst) -= (((a) * (b)) + ((c) * (d))) + ((e) * (f))
#endif

void MatrixInverse44(float input[4][4], float output[4][4])
{
    float column0[4];
    float column1[4];
    float column2[4];
    float column3[4];
    float minor0;
    float minor1;
    float minor2;
    float minor3;
    float minor4;
    float minor5;
    float minor6;
    float minor7;
    float minor8;
    float minor9;
    float minor10;
    float minor11;
    float reciprocalDeterminant;
    int32_t index;

    for (index = 0; index < 4; ++index) {
        memcpy(&column0[index], &input[index][0], sizeof(column0[index]));
        memcpy(&column1[index], &input[index][1], sizeof(column1[index]));
        memcpy(&column2[index], &input[index][2], sizeof(column2[index]));
        memcpy(&column3[index], &input[index][3], sizeof(column3[index]));
    }

    minor0 = column2[2] * column3[3];
    minor1 = column2[3] * column3[2];
    minor2 = column2[1] * column3[3];
    minor3 = column2[3] * column3[1];
    minor4 = column2[1] * column3[2];
    minor5 = column2[2] * column3[1];
    minor6 = column2[0] * column3[3];
    minor7 = column2[3] * column3[0];
    minor8 = column2[0] * column3[2];
    minor9 = column2[2] * column3[0];
    minor10 = column2[0] * column3[1];
    minor11 = column2[1] * column3[0];

    MATRIX_INVERSE44_SET(output[0][0], minor0, column1[1], minor3,
                         column1[2], minor4, column1[3]);
    MATRIX_INVERSE44_SUB(output[0][0], minor1, column1[1], minor2,
                         column1[2], minor5, column1[3]);
    MATRIX_INVERSE44_SET(output[0][1], minor1, column1[0], minor6,
                         column1[2], minor9, column1[3]);
    MATRIX_INVERSE44_SUB(output[0][1], minor0, column1[0], minor7,
                         column1[2], minor8, column1[3]);
    MATRIX_INVERSE44_SET(output[0][2], minor2, column1[0], minor7,
                         column1[1], minor10, column1[3]);
    MATRIX_INVERSE44_SUB(output[0][2], minor3, column1[0], minor6,
                         column1[1], minor11, column1[3]);
    MATRIX_INVERSE44_SET(output[0][3], minor5, column1[0], minor8,
                         column1[1], minor11, column1[2]);
    MATRIX_INVERSE44_SUB(output[0][3], minor4, column1[0], minor9,
                         column1[1], minor10, column1[2]);

    MATRIX_INVERSE44_SET(output[1][0], minor1, column0[1], minor2,
                         column0[2], minor5, column0[3]);
    MATRIX_INVERSE44_SUB(output[1][0], minor0, column0[1], minor3,
                         column0[2], minor4, column0[3]);
    MATRIX_INVERSE44_SET(output[1][1], minor0, column0[0], minor7,
                         column0[2], minor8, column0[3]);
    MATRIX_INVERSE44_SUB(output[1][1], minor1, column0[0], minor6,
                         column0[2], minor9, column0[3]);
    MATRIX_INVERSE44_SET(output[1][2], minor3, column0[0], minor6,
                         column0[1], minor11, column0[3]);
    MATRIX_INVERSE44_SUB(output[1][2], minor2, column0[0], minor7,
                         column0[1], minor10, column0[3]);
    MATRIX_INVERSE44_SET(output[1][3], minor4, column0[0], minor9,
                         column0[1], minor10, column0[2]);
    MATRIX_INVERSE44_SUB(output[1][3], minor5, column0[0], minor8,
                         column0[1], minor11, column0[2]);

    minor0 = column0[2] * column1[3];
    minor1 = column0[3] * column1[2];
    minor2 = column0[1] * column1[3];
    minor3 = column0[3] * column1[1];
    minor4 = column0[1] * column1[2];
    minor5 = column0[2] * column1[1];
    minor6 = column0[0] * column1[3];
    minor7 = column0[3] * column1[0];
    minor8 = column0[0] * column1[2];
    minor9 = column0[2] * column1[0];
    minor10 = column0[0] * column1[1];
    minor11 = column0[1] * column1[0];

    MATRIX_INVERSE44_SET(output[2][0], minor0, column3[1], minor3,
                         column3[2], minor4, column3[3]);
    MATRIX_INVERSE44_SUB(output[2][0], minor1, column3[1], minor2,
                         column3[2], minor5, column3[3]);
    MATRIX_INVERSE44_SET(output[2][1], minor1, column3[0], minor6,
                         column3[2], minor9, column3[3]);
    MATRIX_INVERSE44_SUB(output[2][1], minor0, column3[0], minor7,
                         column3[2], minor8, column3[3]);
    MATRIX_INVERSE44_SET(output[2][2], minor2, column3[0], minor7,
                         column3[1], minor10, column3[3]);
    MATRIX_INVERSE44_SUB(output[2][2], minor3, column3[0], minor6,
                         column3[1], minor11, column3[3]);
    MATRIX_INVERSE44_SET(output[2][3], minor5, column3[0], minor8,
                         column3[1], minor11, column3[2]);
    MATRIX_INVERSE44_SUB(output[2][3], minor4, column3[0], minor9,
                         column3[1], minor10, column3[2]);

    MATRIX_INVERSE44_SET(output[3][0], minor2, column2[2], minor5,
                         column2[3], minor1, column2[1]);
    MATRIX_INVERSE44_SUB(output[3][0], minor4, column2[3], minor0,
                         column2[1], minor3, column2[2]);
    MATRIX_INVERSE44_SET(output[3][1], minor8, column2[3], minor0,
                         column2[0], minor7, column2[2]);
    MATRIX_INVERSE44_SUB(output[3][1], minor6, column2[2], minor9,
                         column2[3], minor1, column2[0]);
    MATRIX_INVERSE44_SET(output[3][2], minor6, column2[1], minor11,
                         column2[3], minor3, column2[0]);
    MATRIX_INVERSE44_SUB(output[3][2], minor10, column2[3], minor2,
                         column2[0], minor7, column2[1]);
    MATRIX_INVERSE44_SET(output[3][3], minor10, column2[2], minor4,
                         column2[0], minor9, column2[1]);
    MATRIX_INVERSE44_SUB(output[3][3], minor8, column2[1], minor11,
                         column2[2], minor5, column2[0]);

#if EMULATE_X87
    reciprocalDeterminant = x87f_store_f32(x87f_add(
        x87f_add(x87f_add(x87f_mul(x87f_load_f32(column0[0]),
                                   x87f_load_f32(output[0][0])),
                          x87f_mul(x87f_load_f32(column0[1]),
                                   x87f_load_f32(output[0][1]))),
                 x87f_mul(x87f_load_f32(column0[2]),
                          x87f_load_f32(output[0][2]))),
        x87f_mul(x87f_load_f32(column0[3]),
                 x87f_load_f32(output[0][3]))));
    reciprocalDeterminant = x87f_store_f32(x87f_div(
        x87f_load_f32(1.0f), x87f_load_f32(reciprocalDeterminant)));
#else
    reciprocalDeterminant =
        ((column0[0] * output[0][0] + column0[1] * output[0][1]) +
         column0[2] * output[0][2]) +
        column0[3] * output[0][3];
    reciprocalDeterminant = 1.0f / reciprocalDeterminant;
#endif

    for (index = 0; index < 16; ++index) {
        output[index / 4][index % 4] *= reciprocalDeterminant;
    }
}

#if EMULATE_X87
#undef MATRIX_INVERSE44_DOT3
#endif
#undef MATRIX_INVERSE44_SET
#undef MATRIX_INVERSE44_SUB
#endif
