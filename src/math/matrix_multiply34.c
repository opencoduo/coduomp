#include "q_math.h"

#include "compat/coduo_x87emu.h"

#if EMULATE_X87
#define MATRIX34_DOT3(la, lb, lc, ra, rb, rc)                               \
    x87f_store_f32(x87f_add(                                                 \
        x87f_add(x87f_mul(x87f_load_f32(la), x87f_load_f32(ra)),            \
                 x87f_mul(x87f_load_f32(lb), x87f_load_f32(rb))),           \
        x87f_mul(x87f_load_f32(lc), x87f_load_f32(rc))))

#define MATRIX34_DOT4(la, lb, lc, ld, ra, rb, rc)                           \
    x87f_store_f32(x87f_add(                                                 \
        x87f_add(x87f_add(x87f_mul(x87f_load_f32(la),                       \
                                   x87f_load_f32(ra)),                       \
                          x87f_mul(x87f_load_f32(lb),                        \
                                   x87f_load_f32(rb))),                      \
                 x87f_mul(x87f_load_f32(lc), x87f_load_f32(rc))),           \
        x87f_load_f32(ld)))
#endif

/*
 * The complete 725-byte bodies are instruction-identical:
 *
 *   coduo_lnxded            0x08067d3b
 *   game.mp.uo.i386.so      RVA 0x0003b947
 *
 * Each output is stored as binary32.  The first three columns retain a
 * three-product x87 chain through the store; the translation column adds the
 * left row's fourth component to that same unspilled chain.
 */
void MatrixMultiply34(const float left[3][4], const float right[3][4],
                      float output[3][4])
{
#if EMULATE_X87
    for (int row = 0; row < 3; ++row) {
        output[row][0] = MATRIX34_DOT3(
            left[row][0], left[row][1], left[row][2],
            right[0][0], right[1][0], right[2][0]);
        output[row][1] = MATRIX34_DOT3(
            left[row][0], left[row][1], left[row][2],
            right[0][1], right[1][1], right[2][1]);
        output[row][2] = MATRIX34_DOT3(
            left[row][0], left[row][1], left[row][2],
            right[0][2], right[1][2], right[2][2]);
        output[row][3] = MATRIX34_DOT4(
            left[row][0], left[row][1], left[row][2], left[row][3],
            right[0][3], right[1][3], right[2][3]);
    }
#else
    for (int row = 0; row < 3; ++row) {
        output[row][0] =
            ((left[row][0] * right[0][0]) +
             (left[row][1] * right[1][0])) +
            (left[row][2] * right[2][0]);
        output[row][1] =
            ((left[row][0] * right[0][1]) +
             (left[row][1] * right[1][1])) +
            (left[row][2] * right[2][1]);
        output[row][2] =
            ((left[row][0] * right[0][2]) +
             (left[row][1] * right[1][2])) +
            (left[row][2] * right[2][2]);
        output[row][3] =
            (((left[row][0] * right[0][3]) +
              (left[row][1] * right[1][3])) +
             (left[row][2] * right[2][3])) +
            left[row][3];
    }
#endif
}

#if EMULATE_X87
#undef MATRIX34_DOT3
#undef MATRIX34_DOT4
#endif
