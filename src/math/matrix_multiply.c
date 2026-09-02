#include "q_math.h"

#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#include "compat/coduo_x87emu.h"

/*
 * The four authoritative Windows MatrixMultiply bodies are byte-identical:
 *
 *   CoDUOMP.exe                 0x00432450
 *   uo_cgame_mp_x86.dll        0x3004a5b0
 *   uo_ui_mp_x86.dll           0x40002580
 *   uo_game_mp_x86.dll         0x20017600
 *
 * The corresponding MatrixMultiplyEquals bodies are likewise byte-identical
 * at 0x00432530, 0x3004a690, 0x40002660, and 0x200176e0. Windows retains the
 * compiler-selected term order for every output cell and evaluates each
 * unspilled x87 chain under the process PC=53 policy.
 *
 * The two authoritative Linux MatrixMultiply bodies are byte-identical at
 * coduo_lnxded 0x080678fc and game.mp.uo.i386.so RVA 0x0003b508. Their
 * MatrixMultiplyEquals bodies are byte-identical at 0x08067b08 and RVA
 * 0x0003b714. Linux folds every dot in k=0,1,2 order under PC=64.
 *
 * All targets store output cells in row-major order. MatrixMultiplyEquals
 * first buffers rows 0 and 1, writes row 2 directly, then copies the buffered
 * binary32 values back with integer moves. That order is part of its in-place
 * behavior and is retained below.
 */

#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
#define MATRIX_STRINGIFY_INNER(value) #value
#define MATRIX_STRINGIFY(value) MATRIX_STRINGIFY_INNER(value)
#define MATRIX_ADDRESS(base, offset)                                          \
    MATRIX_STRINGIFY(offset) "(%[" MATRIX_STRINGIFY(base) "])"
#define MATRIX_NATIVE_X87_DOT(destinationBase, destinationOffset,             \
                              aBase, aOffset, bBase, bOffset,                 \
                              cBase, cOffset, dBase, dOffset,                 \
                              eBase, eOffset, fBase, fOffset)                 \
    __asm__ __volatile__("flds " MATRIX_ADDRESS(aBase, aOffset) "\n\t"       \
                         "fmuls " MATRIX_ADDRESS(bBase, bOffset) "\n\t"      \
                         "flds " MATRIX_ADDRESS(cBase, cOffset) "\n\t"       \
                         "fmuls " MATRIX_ADDRESS(dBase, dOffset) "\n\t"      \
                         "faddp %%st, %%st(1)\n\t"                          \
                         "flds " MATRIX_ADDRESS(eBase, eOffset) "\n\t"       \
                         "fmuls " MATRIX_ADDRESS(fBase, fOffset) "\n\t"      \
                         "faddp %%st, %%st(1)\n\t"                          \
                         "fstps " MATRIX_ADDRESS(destinationBase,             \
                                                  destinationOffset)          \
                         :                                                     \
                         : [left] "r"(left), [right] "r"(right),              \
                           [store] "r"(store)                                 \
                         : "st", "st(1)", "memory")
#define MATRIX_HAS_NATIVE_X87 1
#else
#define MATRIX_HAS_NATIVE_X87 0
#endif

#if defined(WINDOWS_BEHAVIOR)

void MatrixMultiply(axis_t left, axis_t right, axis_t output)
{
#if MATRIX_HAS_NATIVE_X87
    float *store = &output[0][0];

    MATRIX_NATIVE_X87_DOT(store, 0, left, 0, right, 0,
                          left, 4, right, 12, right, 24, left, 8);
    MATRIX_NATIVE_X87_DOT(store, 4, left, 4, right, 16,
                          right, 28, left, 8, right, 4, left, 0);
    MATRIX_NATIVE_X87_DOT(store, 8, left, 4, right, 20,
                          right, 32, left, 8, right, 8, left, 0);

    MATRIX_NATIVE_X87_DOT(store, 12, left, 16, right, 12,
                          left, 12, right, 0, left, 20, right, 24);
    MATRIX_NATIVE_X87_DOT(store, 16, left, 20, right, 28,
                          left, 16, right, 16, left, 12, right, 4);
    MATRIX_NATIVE_X87_DOT(store, 20, left, 20, right, 32,
                          left, 16, right, 20, left, 12, right, 8);

    MATRIX_NATIVE_X87_DOT(store, 24, left, 24, right, 0,
                          right, 24, left, 32, left, 28, right, 12);
    MATRIX_NATIVE_X87_DOT(store, 28, right, 28, left, 32,
                          right, 16, left, 28, right, 4, left, 24);
    MATRIX_NATIVE_X87_DOT(store, 32, right, 32, left, 32,
                          right, 20, left, 28, right, 8, left, 24);
#else
    output[0][0] = (float)(((double)left[0][0] * right[0][0] +
                            (double)left[0][1] * right[1][0]) +
                           (double)right[2][0] * left[0][2]);
    output[0][1] = (float)(((double)left[0][1] * right[1][1] +
                            (double)right[2][1] * left[0][2]) +
                           (double)right[0][1] * left[0][0]);
    output[0][2] = (float)(((double)left[0][1] * right[1][2] +
                            (double)right[2][2] * left[0][2]) +
                           (double)right[0][2] * left[0][0]);

    output[1][0] = (float)(((double)left[1][1] * right[1][0] +
                            (double)left[1][0] * right[0][0]) +
                           (double)left[1][2] * right[2][0]);
    output[1][1] = (float)(((double)left[1][2] * right[2][1] +
                            (double)left[1][1] * right[1][1]) +
                           (double)left[1][0] * right[0][1]);
    output[1][2] = (float)(((double)left[1][2] * right[2][2] +
                            (double)left[1][1] * right[1][2]) +
                           (double)left[1][0] * right[0][2]);

    output[2][0] = (float)(((double)left[2][0] * right[0][0] +
                            (double)right[2][0] * left[2][2]) +
                           (double)left[2][1] * right[1][0]);
    output[2][1] = (float)(((double)right[2][1] * left[2][2] +
                            (double)right[1][1] * left[2][1]) +
                           (double)right[0][1] * left[2][0]);
    output[2][2] = (float)(((double)right[2][2] * left[2][2] +
                            (double)right[1][2] * left[2][1]) +
                           (double)right[0][2] * left[2][0]);
#endif
}

void MatrixMultiplyEquals(axis_t left, axis_t right)
{
    vec3_t buffered[2];

#if MATRIX_HAS_NATIVE_X87
    float *store = &buffered[0][0];

    MATRIX_NATIVE_X87_DOT(store, 0, left, 4, right, 12,
                          left, 0, right, 0, right, 24, left, 8);
    MATRIX_NATIVE_X87_DOT(store, 4, right, 28, left, 8,
                          left, 4, right, 16, right, 4, left, 0);
    MATRIX_NATIVE_X87_DOT(store, 8, right, 32, left, 8,
                          left, 0, right, 8, left, 4, right, 20);

    MATRIX_NATIVE_X87_DOT(store, 12, left, 20, right, 24,
                          left, 12, right, 0, left, 16, right, 12);
    MATRIX_NATIVE_X87_DOT(store, 16, left, 20, right, 28,
                          left, 16, right, 16, left, 12, right, 4);
    MATRIX_NATIVE_X87_DOT(store, 20, left, 12, right, 8,
                          right, 32, left, 20, right, 20, left, 16);

    MATRIX_NATIVE_X87_DOT(right, 24, left, 32, right, 24,
                          left, 24, right, 0, left, 28, right, 12);
    MATRIX_NATIVE_X87_DOT(right, 28, left, 32, right, 28,
                          left, 28, right, 16, left, 24, right, 4);
    MATRIX_NATIVE_X87_DOT(right, 32, left, 24, right, 8,
                          right, 32, left, 32, right, 20, left, 28);
#else
    buffered[0][0] = (float)(((double)left[0][1] * right[1][0] +
                              (double)left[0][0] * right[0][0]) +
                             (double)right[2][0] * left[0][2]);
    buffered[0][1] = (float)(((double)right[2][1] * left[0][2] +
                              (double)left[0][1] * right[1][1]) +
                             (double)right[0][1] * left[0][0]);
    buffered[0][2] = (float)(((double)right[2][2] * left[0][2] +
                              (double)left[0][0] * right[0][2]) +
                             (double)left[0][1] * right[1][2]);

    buffered[1][0] = (float)(((double)left[1][2] * right[2][0] +
                              (double)left[1][0] * right[0][0]) +
                             (double)left[1][1] * right[1][0]);
    buffered[1][1] = (float)(((double)left[1][2] * right[2][1] +
                              (double)left[1][1] * right[1][1]) +
                             (double)left[1][0] * right[0][1]);
    buffered[1][2] = (float)(((double)left[1][0] * right[0][2] +
                              (double)right[2][2] * left[1][2]) +
                             (double)right[1][2] * left[1][1]);

    right[2][0] = (float)(((double)left[2][2] * right[2][0] +
                           (double)left[2][0] * right[0][0]) +
                          (double)left[2][1] * right[1][0]);
    right[2][1] = (float)(((double)left[2][2] * right[2][1] +
                           (double)left[2][1] * right[1][1]) +
                          (double)left[2][0] * right[0][1]);
    right[2][2] = (float)(((double)left[2][0] * right[0][2] +
                           (double)right[2][2] * left[2][2]) +
                          (double)right[1][2] * left[2][1]);
#endif

    memcpy(&right[0][0], &buffered[0][0], sizeof(right[0][0]));
    memcpy(&right[0][1], &buffered[0][1], sizeof(right[0][1]));
    memcpy(&right[0][2], &buffered[0][2], sizeof(right[0][2]));
    memcpy(&right[1][0], &buffered[1][0], sizeof(right[1][0]));
    memcpy(&right[1][1], &buffered[1][1], sizeof(right[1][1]));
    memcpy(&right[1][2], &buffered[1][2], sizeof(right[1][2]));
}

#else

#if EMULATE_X87
#define MATRIX_LINUX_DOT(destination, a, b, c, d, e, f)                       \
    ((destination) = x87f_store_f32(x87f_add(                                \
         x87f_add(x87f_mul(x87f_load_f32(a), x87f_load_f32(b)),              \
                  x87f_mul(x87f_load_f32(c), x87f_load_f32(d))),             \
         x87f_mul(x87f_load_f32(e), x87f_load_f32(f)))))
#else
#define MATRIX_LINUX_DOT(destination, a, b, c, d, e, f)                       \
    ((destination) = ((a) * (b) + (c) * (d)) + (e) * (f))
#endif

void MatrixMultiply(axis_t left, axis_t right, axis_t output)
{
#if MATRIX_HAS_NATIVE_X87 && !EMULATE_X87
    float *store = &output[0][0];

    MATRIX_NATIVE_X87_DOT(store, 0, left, 0, right, 0,
                          left, 4, right, 12, left, 8, right, 24);
    MATRIX_NATIVE_X87_DOT(store, 4, left, 0, right, 4,
                          left, 4, right, 16, left, 8, right, 28);
    MATRIX_NATIVE_X87_DOT(store, 8, left, 0, right, 8,
                          left, 4, right, 20, left, 8, right, 32);

    MATRIX_NATIVE_X87_DOT(store, 12, left, 12, right, 0,
                          left, 16, right, 12, left, 20, right, 24);
    MATRIX_NATIVE_X87_DOT(store, 16, left, 12, right, 4,
                          left, 16, right, 16, left, 20, right, 28);
    MATRIX_NATIVE_X87_DOT(store, 20, left, 12, right, 8,
                          left, 16, right, 20, left, 20, right, 32);

    MATRIX_NATIVE_X87_DOT(store, 24, left, 24, right, 0,
                          left, 28, right, 12, left, 32, right, 24);
    MATRIX_NATIVE_X87_DOT(store, 28, left, 24, right, 4,
                          left, 28, right, 16, left, 32, right, 28);
    MATRIX_NATIVE_X87_DOT(store, 32, left, 24, right, 8,
                          left, 28, right, 20, left, 32, right, 32);
#else
    MATRIX_LINUX_DOT(output[0][0], left[0][0], right[0][0],
                     left[0][1], right[1][0], left[0][2], right[2][0]);
    MATRIX_LINUX_DOT(output[0][1], left[0][0], right[0][1],
                     left[0][1], right[1][1], left[0][2], right[2][1]);
    MATRIX_LINUX_DOT(output[0][2], left[0][0], right[0][2],
                     left[0][1], right[1][2], left[0][2], right[2][2]);

    MATRIX_LINUX_DOT(output[1][0], left[1][0], right[0][0],
                     left[1][1], right[1][0], left[1][2], right[2][0]);
    MATRIX_LINUX_DOT(output[1][1], left[1][0], right[0][1],
                     left[1][1], right[1][1], left[1][2], right[2][1]);
    MATRIX_LINUX_DOT(output[1][2], left[1][0], right[0][2],
                     left[1][1], right[1][2], left[1][2], right[2][2]);

    MATRIX_LINUX_DOT(output[2][0], left[2][0], right[0][0],
                     left[2][1], right[1][0], left[2][2], right[2][0]);
    MATRIX_LINUX_DOT(output[2][1], left[2][0], right[0][1],
                     left[2][1], right[1][1], left[2][2], right[2][1]);
    MATRIX_LINUX_DOT(output[2][2], left[2][0], right[0][2],
                     left[2][1], right[1][2], left[2][2], right[2][2]);
#endif
}

void MatrixMultiplyEquals(axis_t left, axis_t right)
{
    vec3_t buffered[2];

#if MATRIX_HAS_NATIVE_X87 && !EMULATE_X87
    float *store = &buffered[0][0];

    MATRIX_NATIVE_X87_DOT(store, 0, left, 0, right, 0,
                          left, 4, right, 12, left, 8, right, 24);
    MATRIX_NATIVE_X87_DOT(store, 4, left, 0, right, 4,
                          left, 4, right, 16, left, 8, right, 28);
    MATRIX_NATIVE_X87_DOT(store, 8, left, 0, right, 8,
                          left, 4, right, 20, left, 8, right, 32);

    MATRIX_NATIVE_X87_DOT(store, 12, left, 12, right, 0,
                          left, 16, right, 12, left, 20, right, 24);
    MATRIX_NATIVE_X87_DOT(store, 16, left, 12, right, 4,
                          left, 16, right, 16, left, 20, right, 28);
    MATRIX_NATIVE_X87_DOT(store, 20, left, 12, right, 8,
                          left, 16, right, 20, left, 20, right, 32);

    MATRIX_NATIVE_X87_DOT(right, 24, left, 24, right, 0,
                          left, 28, right, 12, left, 32, right, 24);
    MATRIX_NATIVE_X87_DOT(right, 28, left, 24, right, 4,
                          left, 28, right, 16, left, 32, right, 28);
    MATRIX_NATIVE_X87_DOT(right, 32, left, 24, right, 8,
                          left, 28, right, 20, left, 32, right, 32);
#else
    MATRIX_LINUX_DOT(buffered[0][0], left[0][0], right[0][0],
                     left[0][1], right[1][0], left[0][2], right[2][0]);
    MATRIX_LINUX_DOT(buffered[0][1], left[0][0], right[0][1],
                     left[0][1], right[1][1], left[0][2], right[2][1]);
    MATRIX_LINUX_DOT(buffered[0][2], left[0][0], right[0][2],
                     left[0][1], right[1][2], left[0][2], right[2][2]);

    MATRIX_LINUX_DOT(buffered[1][0], left[1][0], right[0][0],
                     left[1][1], right[1][0], left[1][2], right[2][0]);
    MATRIX_LINUX_DOT(buffered[1][1], left[1][0], right[0][1],
                     left[1][1], right[1][1], left[1][2], right[2][1]);
    MATRIX_LINUX_DOT(buffered[1][2], left[1][0], right[0][2],
                     left[1][1], right[1][2], left[1][2], right[2][2]);

    MATRIX_LINUX_DOT(right[2][0], left[2][0], right[0][0],
                     left[2][1], right[1][0], left[2][2], right[2][0]);
    MATRIX_LINUX_DOT(right[2][1], left[2][0], right[0][1],
                     left[2][1], right[1][1], left[2][2], right[2][1]);
    MATRIX_LINUX_DOT(right[2][2], left[2][0], right[0][2],
                     left[2][1], right[1][2], left[2][2], right[2][2]);
#endif

    memcpy(&right[0][0], &buffered[0][0], sizeof(right[0][0]));
    memcpy(&right[0][1], &buffered[0][1], sizeof(right[0][1]));
    memcpy(&right[0][2], &buffered[0][2], sizeof(right[0][2]));
    memcpy(&right[1][0], &buffered[1][0], sizeof(right[1][0]));
    memcpy(&right[1][1], &buffered[1][1], sizeof(right[1][1]));
    memcpy(&right[1][2], &buffered[1][2], sizeof(right[1][2]));
}

#undef MATRIX_LINUX_DOT
#endif

#if MATRIX_HAS_NATIVE_X87
#undef MATRIX_NATIVE_X87_DOT
#undef MATRIX_ADDRESS
#undef MATRIX_STRINGIFY
#undef MATRIX_STRINGIFY_INNER
#endif
#undef MATRIX_HAS_NATIVE_X87
