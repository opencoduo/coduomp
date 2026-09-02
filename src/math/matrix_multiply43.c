#include "q_math.h"

#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * The four authoritative Windows MatrixMultiply43 bodies are byte-identical:
 *
 *   CoDUOMP.exe                 0x00432770
 *   uo_cgame_mp_x86.dll        0x3004a8d0
 *   uo_ui_mp_x86.dll           0x400028a0
 *   uo_game_mp_x86.dll         0x20017920
 *
 * The two Linux bodies are likewise byte-identical at coduo_lnxded
 * 0x08068010 and game.mp.uo.i386.so RVA 0x0003bc1c.  The targets agree on
 * the compact matrix43_t layout, output-store order, and affine formula.
 * Windows retains a compiler-selected product order under PC=53; Linux folds
 * every cell in axis 0,1,2 order under PC=64.  The complete target bodies
 * below preserve that genuine arithmetic-order difference.
 *
 * The adjacent Linux engine body at 0x08067d3b is MatrixMultiply34, not a
 * second MatrixMultiply43 representation: its 725 bytes exactly match the
 * game module's exported MatrixMultiply34 at RVA 0x0003b947.
 */

#if CODUO_ARCH_HAS_X87 && (defined(__GNUC__) || defined(__clang__))
/*
 * Keep the three matrix bases in registers and encode the proven field
 * displacements in the template.  Giving GCC one independent memory operand
 * for every load exhausts i386 address registers and can reject this graph as
 * impossible; it does not describe different arithmetic.  This form retains
 * the retail FLD/FMUL/FADDP/FADD/FSTP sequence while remaining valid for PIC.
 */
#define MATRIX43_STRINGIFY_INNER(value) #value
#define MATRIX43_STRINGIFY(value) MATRIX43_STRINGIFY_INNER(value)
#define MATRIX43_ADDRESS(base, offset) MATRIX43_STRINGIFY(offset) "(%[" MATRIX43_STRINGIFY(base) "])"
// clang-format off
#define MATRIX43_NATIVE_X87_STORE3(                                           \
    destinationOffset, aBase, aOffset, bBase, bOffset,                       \
    cBase, cOffset, dBase, dOffset, eBase, eOffset, fBase, fOffset)          \
    __asm__ __volatile__("flds " MATRIX43_ADDRESS(aBase, aOffset) "\n\t"   \
                         "fmuls " MATRIX43_ADDRESS(bBase, bOffset) "\n\t"  \
                         "flds " MATRIX43_ADDRESS(cBase, cOffset) "\n\t"   \
                         "fmuls " MATRIX43_ADDRESS(dBase, dOffset) "\n\t"  \
                         "faddp %%st, %%st(1)\n\t"                          \
                         "flds " MATRIX43_ADDRESS(eBase, eOffset) "\n\t"   \
                         "fmuls " MATRIX43_ADDRESS(fBase, fOffset) "\n\t"  \
                         "faddp %%st, %%st(1)\n\t"                          \
                         "fstps " MATRIX43_ADDRESS(store,                   \
                                                    destinationOffset)       \
                         :                                                   \
                         : [left] "r"(left), [right] "r"(right),            \
                           [store] "r"(output)                               \
                         : "st", "st(1)", "memory")
#define MATRIX43_NATIVE_X87_STORE4(                                           \
    destinationOffset, aBase, aOffset, bBase, bOffset,                       \
    cBase, cOffset, dBase, dOffset, eBase, eOffset, fBase, fOffset,          \
    translationBase, translationOffset)                                      \
    __asm__ __volatile__("flds " MATRIX43_ADDRESS(aBase, aOffset) "\n\t"   \
                         "fmuls " MATRIX43_ADDRESS(bBase, bOffset) "\n\t"  \
                         "flds " MATRIX43_ADDRESS(cBase, cOffset) "\n\t"   \
                         "fmuls " MATRIX43_ADDRESS(dBase, dOffset) "\n\t"  \
                         "faddp %%st, %%st(1)\n\t"                          \
                         "flds " MATRIX43_ADDRESS(eBase, eOffset) "\n\t"   \
                         "fmuls " MATRIX43_ADDRESS(fBase, fOffset) "\n\t"  \
                         "faddp %%st, %%st(1)\n\t"                          \
                         "fadds " MATRIX43_ADDRESS(translationBase,         \
                                                    translationOffset)       \
                         "\n\t"                                             \
                         "fstps " MATRIX43_ADDRESS(store,                   \
                                                    destinationOffset)       \
                         :                                                   \
                         : [left] "r"(left), [right] "r"(right),            \
                           [store] "r"(output)                               \
                         : "st", "st(1)", "memory")
// clang-format on
#endif

#if EMULATE_X87
#define MATRIX43_STORE3(destination, a, b, c, d, e, f) \
    do { \
        (destination) = \
            x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(a), x87f_load_f32(b)), x87f_mul(x87f_load_f32(c), x87f_load_f32(d))), \
                                    x87f_mul(x87f_load_f32(e), x87f_load_f32(f)))); \
    } while (0)
#define MATRIX43_STORE4(destination, a, b, c, d, e, f, translation) \
    do { \
        (destination) = x87f_store_f32( \
            x87f_add(x87f_add(x87f_add(x87f_mul(x87f_load_f32(a), x87f_load_f32(b)), x87f_mul(x87f_load_f32(c), x87f_load_f32(d))), \
                              x87f_mul(x87f_load_f32(e), x87f_load_f32(f))), \
                     x87f_load_f32(translation))); \
    } while (0)
#else
#define MATRIX43_STORE3(destination, a, b, c, d, e, f) \
    do { \
        (destination) = \
            (float)(((long double)(a) * (long double)(b) + (long double)(c) * (long double)(d)) + (long double)(e) * (long double)(f)); \
    } while (0)
#define MATRIX43_STORE4(destination, a, b, c, d, e, f, translation) \
    do { \
        (destination) = \
            (float)((((long double)(a) * (long double)(b) + (long double)(c) * (long double)(d)) + (long double)(e) * (long double)(f)) + \
                    (long double)(translation)); \
    } while (0)
#endif

#if defined(WINDOWS_BEHAVIOR)
void MatrixMultiply43(const matrix43_t *left, const matrix43_t *right, matrix43_t *output)
{
#if CODUO_ARCH_HAS_X87 && !EMULATE_X87 && (defined(__GNUC__) || defined(__clang__))
    MATRIX43_NATIVE_X87_STORE3(0, left, 0, right, 0, left, 4, right, 12, right, 24, left, 8);
    MATRIX43_NATIVE_X87_STORE3(12, left, 16, right, 12, left, 12, right, 0, left, 20, right, 24);
    MATRIX43_NATIVE_X87_STORE3(24, left, 28, right, 12, left, 24, right, 0, left, 32, right, 24);

    MATRIX43_NATIVE_X87_STORE3(4, left, 4, right, 16, right, 28, left, 8, right, 4, left, 0);
    MATRIX43_NATIVE_X87_STORE3(16, left, 20, right, 28, left, 16, right, 16, left, 12, right, 4);
    MATRIX43_NATIVE_X87_STORE3(28, left, 32, right, 28, left, 28, right, 16, left, 24, right, 4);

    MATRIX43_NATIVE_X87_STORE3(8, left, 0, right, 8, left, 4, right, 20, right, 32, left, 8);
    MATRIX43_NATIVE_X87_STORE3(20, left, 20, right, 32, left, 16, right, 20, left, 12, right, 8);
    MATRIX43_NATIVE_X87_STORE3(32, left, 32, right, 32, left, 28, right, 20, left, 24, right, 8);

    MATRIX43_NATIVE_X87_STORE4(36, left, 40, right, 12, left, 36, right, 0, left, 44, right, 24, right, 36);
    MATRIX43_NATIVE_X87_STORE4(40, left, 44, right, 28, left, 40, right, 16, left, 36, right, 4, right, 40);
    MATRIX43_NATIVE_X87_STORE4(44, left, 44, right, 32, left, 40, right, 20, left, 36, right, 8, right, 44);
#else
    MATRIX43_STORE3(output->axis[0][0], left->axis[0][0], right->axis[0][0], left->axis[0][1], right->axis[1][0], right->axis[2][0],
                    left->axis[0][2]);
    MATRIX43_STORE3(output->axis[1][0], left->axis[1][1], right->axis[1][0], left->axis[1][0], right->axis[0][0], left->axis[1][2],
                    right->axis[2][0]);
    MATRIX43_STORE3(output->axis[2][0], left->axis[2][1], right->axis[1][0], left->axis[2][0], right->axis[0][0], left->axis[2][2],
                    right->axis[2][0]);

    MATRIX43_STORE3(output->axis[0][1], left->axis[0][1], right->axis[1][1], right->axis[2][1], left->axis[0][2], right->axis[0][1],
                    left->axis[0][0]);
    MATRIX43_STORE3(output->axis[1][1], left->axis[1][2], right->axis[2][1], left->axis[1][1], right->axis[1][1], left->axis[1][0],
                    right->axis[0][1]);
    MATRIX43_STORE3(output->axis[2][1], left->axis[2][2], right->axis[2][1], left->axis[2][1], right->axis[1][1], left->axis[2][0],
                    right->axis[0][1]);

    MATRIX43_STORE3(output->axis[0][2], left->axis[0][0], right->axis[0][2], left->axis[0][1], right->axis[1][2], right->axis[2][2],
                    left->axis[0][2]);
    MATRIX43_STORE3(output->axis[1][2], left->axis[1][2], right->axis[2][2], left->axis[1][1], right->axis[1][2], left->axis[1][0],
                    right->axis[0][2]);
    MATRIX43_STORE3(output->axis[2][2], left->axis[2][2], right->axis[2][2], left->axis[2][1], right->axis[1][2], left->axis[2][0],
                    right->axis[0][2]);

    MATRIX43_STORE4(output->origin[0], left->origin[1], right->axis[1][0], left->origin[0], right->axis[0][0], left->origin[2],
                    right->axis[2][0], right->origin[0]);
    MATRIX43_STORE4(output->origin[1], left->origin[2], right->axis[2][1], left->origin[1], right->axis[1][1], left->origin[0],
                    right->axis[0][1], right->origin[1]);
    MATRIX43_STORE4(output->origin[2], left->origin[2], right->axis[2][2], left->origin[1], right->axis[1][2], left->origin[0],
                    right->axis[0][2], right->origin[2]);
#endif
}
#else
void MatrixMultiply43(const matrix43_t *left, const matrix43_t *right, matrix43_t *output)
{
#if CODUO_ARCH_HAS_X87 && !EMULATE_X87 && (defined(__GNUC__) || defined(__clang__))
    MATRIX43_NATIVE_X87_STORE3(0, left, 0, right, 0, left, 4, right, 12, left, 8, right, 24);
    MATRIX43_NATIVE_X87_STORE3(12, left, 12, right, 0, left, 16, right, 12, left, 20, right, 24);
    MATRIX43_NATIVE_X87_STORE3(24, left, 24, right, 0, left, 28, right, 12, left, 32, right, 24);

    MATRIX43_NATIVE_X87_STORE3(4, left, 0, right, 4, left, 4, right, 16, left, 8, right, 28);
    MATRIX43_NATIVE_X87_STORE3(16, left, 12, right, 4, left, 16, right, 16, left, 20, right, 28);
    MATRIX43_NATIVE_X87_STORE3(28, left, 24, right, 4, left, 28, right, 16, left, 32, right, 28);

    MATRIX43_NATIVE_X87_STORE3(8, left, 0, right, 8, left, 4, right, 20, left, 8, right, 32);
    MATRIX43_NATIVE_X87_STORE3(20, left, 12, right, 8, left, 16, right, 20, left, 20, right, 32);
    MATRIX43_NATIVE_X87_STORE3(32, left, 24, right, 8, left, 28, right, 20, left, 32, right, 32);

    MATRIX43_NATIVE_X87_STORE4(36, left, 36, right, 0, left, 40, right, 12, left, 44, right, 24, right, 36);
    MATRIX43_NATIVE_X87_STORE4(40, left, 36, right, 4, left, 40, right, 16, left, 44, right, 28, right, 40);
    MATRIX43_NATIVE_X87_STORE4(44, left, 36, right, 8, left, 40, right, 20, left, 44, right, 32, right, 44);
#else
    MATRIX43_STORE3(output->axis[0][0], left->axis[0][0], right->axis[0][0], left->axis[0][1], right->axis[1][0], left->axis[0][2],
                    right->axis[2][0]);
    MATRIX43_STORE3(output->axis[1][0], left->axis[1][0], right->axis[0][0], left->axis[1][1], right->axis[1][0], left->axis[1][2],
                    right->axis[2][0]);
    MATRIX43_STORE3(output->axis[2][0], left->axis[2][0], right->axis[0][0], left->axis[2][1], right->axis[1][0], left->axis[2][2],
                    right->axis[2][0]);

    MATRIX43_STORE3(output->axis[0][1], left->axis[0][0], right->axis[0][1], left->axis[0][1], right->axis[1][1], left->axis[0][2],
                    right->axis[2][1]);
    MATRIX43_STORE3(output->axis[1][1], left->axis[1][0], right->axis[0][1], left->axis[1][1], right->axis[1][1], left->axis[1][2],
                    right->axis[2][1]);
    MATRIX43_STORE3(output->axis[2][1], left->axis[2][0], right->axis[0][1], left->axis[2][1], right->axis[1][1], left->axis[2][2],
                    right->axis[2][1]);

    MATRIX43_STORE3(output->axis[0][2], left->axis[0][0], right->axis[0][2], left->axis[0][1], right->axis[1][2], left->axis[0][2],
                    right->axis[2][2]);
    MATRIX43_STORE3(output->axis[1][2], left->axis[1][0], right->axis[0][2], left->axis[1][1], right->axis[1][2], left->axis[1][2],
                    right->axis[2][2]);
    MATRIX43_STORE3(output->axis[2][2], left->axis[2][0], right->axis[0][2], left->axis[2][1], right->axis[1][2], left->axis[2][2],
                    right->axis[2][2]);

    MATRIX43_STORE4(output->origin[0], left->origin[0], right->axis[0][0], left->origin[1], right->axis[1][0], left->origin[2],
                    right->axis[2][0], right->origin[0]);
    MATRIX43_STORE4(output->origin[1], left->origin[0], right->axis[0][1], left->origin[1], right->axis[1][1], left->origin[2],
                    right->axis[2][1], right->origin[1]);
    MATRIX43_STORE4(output->origin[2], left->origin[0], right->axis[0][2], left->origin[1], right->axis[1][2], left->origin[2],
                    right->axis[2][2], right->origin[2]);
#endif
}
#endif

#undef MATRIX43_STORE4
#undef MATRIX43_STORE3
#if CODUO_ARCH_HAS_X87 && (defined(__GNUC__) || defined(__clang__))
#undef MATRIX43_NATIVE_X87_STORE4
#undef MATRIX43_NATIVE_X87_STORE3
#undef MATRIX43_ADDRESS
#undef MATRIX43_STRINGIFY
#undef MATRIX43_STRINGIFY_INNER
#endif
