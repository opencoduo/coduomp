#include "q_math.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#include "compat/coduo_x87emu.h"

/*
 * QuatMultiply uses the original CoD convention: product = second * first,
 * where each quaternion is stored as [x, y, z, w]. The four Windows bodies
 * are byte-identical (124-byte SHA-256
 * 4d5d04a993fbe5b908c4fa32f885b3842f78a5a36741d396135100012005424e):
 *
 *   CoDUOMP.exe                 0x004335c0
 *   uo_cgame_mp_x86.dll        0x3004b720
 *   uo_ui_mp_x86.dll           0x400036f0
 *   uo_game_mp_x86.dll         0x20018770
 *
 * The Windows register allocation is ECX=first, EAX=second, EDX=product. The
 * corresponding G_DObjSetLocalTagInternal calls prove that order: Linux passes
 * (pitch, yaw) at RVA 0x00078d63, while Windows places pitch in ECX and yaw in
 * EAX at 0x20052702. This corrects the former cgame/UI reconstruction, which
 * had named EAX as the first formal operand and consequently reversed both the
 * recovered function convention and its callers.
 *
 * Linux coduo_lnxded 0x0806960c and game.mp.uo.i386.so RVA 0x0003d25f
 * retain the same mathematical product but use a different unspilled x87
 * addition order. Windows evaluates under PC=53; Linux evaluates under PC=64.
 * Each lane is stored to binary32 before the next begins, so output aliasing
 * either input remains observable. The supporting PowerPC executable, cgame,
 * and game bodies are instruction-identical and also compute second * first.
 */
#if defined(WINDOWS_BEHAVIOR)
void QuatMultiply(const vec4_t first, const vec4_t second, vec4_t product)
{
#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    /* GAS's AT&T `fsubrp` spelling encodes the original DE E9 FSUBP. */
    __asm__ __volatile__("flds 12(%1)\n\t"
                         "fmuls 0(%0)\n\t"
                         "flds 4(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 12(%0)\n\t"
                         "fmuls 0(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 8(%1)\n\t"
                         "fmuls 4(%0)\n\t"
                         "fsubrp %%st, %%st(1)\n\t"
                         "fstps 0(%2)\n\t"
                         "flds 12(%1)\n\t"
                         "fmuls 4(%0)\n\t"
                         "flds 0(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "fsubrp %%st, %%st(1)\n\t"
                         "flds 8(%1)\n\t"
                         "fmuls 0(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 12(%0)\n\t"
                         "fmuls 4(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstps 4(%2)\n\t"
                         "flds 4(%0)\n\t"
                         "fmuls 0(%1)\n\t"
                         "flds 12(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 0(%0)\n\t"
                         "fmuls 4(%1)\n\t"
                         "fsubrp %%st, %%st(1)\n\t"
                         "flds 12(%0)\n\t"
                         "fmuls 8(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstps 8(%2)\n\t"
                         "flds 12(%1)\n\t"
                         "fmuls 12(%0)\n\t"
                         "flds 0(%0)\n\t"
                         "fmuls 0(%1)\n\t"
                         "fsubrp %%st, %%st(1)\n\t"
                         "flds 4(%0)\n\t"
                         "fmuls 4(%1)\n\t"
                         "fsubrp %%st, %%st(1)\n\t"
                         "flds 8(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "fsubrp %%st, %%st(1)\n\t"
                         "fstps 12(%2)"
                         :
                         : "r"(first), "r"(second), "r"(product)
                         : "st", "st(1)", "memory");
#else
    volatile double accumulator;
    volatile double term;

    accumulator = (double)second[3] * (double)first[0];
    term = (double)second[1] * (double)first[2];
    accumulator = accumulator + term;
    term = (double)first[3] * (double)second[0];
    accumulator = accumulator + term;
    term = (double)second[2] * (double)first[1];
    product[0] = (float)(accumulator - term);

    accumulator = (double)second[3] * (double)first[1];
    term = (double)second[0] * (double)first[2];
    accumulator = accumulator - term;
    term = (double)second[2] * (double)first[0];
    accumulator = accumulator + term;
    term = (double)first[3] * (double)second[1];
    product[1] = (float)(accumulator + term);

    accumulator = (double)first[1] * (double)second[0];
    term = (double)second[3] * (double)first[2];
    accumulator = accumulator + term;
    term = (double)first[0] * (double)second[1];
    accumulator = accumulator - term;
    term = (double)first[3] * (double)second[2];
    product[2] = (float)(accumulator + term);

    accumulator = (double)second[3] * (double)first[3];
    term = (double)first[0] * (double)second[0];
    accumulator = accumulator - term;
    term = (double)first[1] * (double)second[1];
    accumulator = accumulator - term;
    term = (double)second[2] * (double)first[2];
    product[3] = (float)(accumulator - term);
#endif
}
#else
void QuatMultiply(const vec4_t first, const vec4_t second, vec4_t product)
{
#if EMULATE_X87
#define QUAT_PRODUCT(a, b) x87f_mul(x87f_load_f32(a), x87f_load_f32(b))
    product[0] = x87f_store_f32(x87f_sub(
        x87f_add(x87f_add(QUAT_PRODUCT(first[0], second[3]), QUAT_PRODUCT(first[3], second[0])), QUAT_PRODUCT(first[2], second[1])),
        QUAT_PRODUCT(first[1], second[2])));
    product[1] = x87f_store_f32(x87f_add(
        x87f_add(x87f_sub(QUAT_PRODUCT(first[1], second[3]), QUAT_PRODUCT(first[2], second[0])), QUAT_PRODUCT(first[3], second[1])),
        QUAT_PRODUCT(first[0], second[2])));
    product[2] = x87f_store_f32(x87f_add(
        x87f_sub(x87f_add(QUAT_PRODUCT(first[2], second[3]), QUAT_PRODUCT(first[1], second[0])), QUAT_PRODUCT(first[0], second[1])),
        QUAT_PRODUCT(first[3], second[2])));
    product[3] = x87f_store_f32(x87f_sub(
        x87f_sub(x87f_sub(QUAT_PRODUCT(first[3], second[3]), QUAT_PRODUCT(first[0], second[0])), QUAT_PRODUCT(first[1], second[1])),
        QUAT_PRODUCT(first[2], second[2])));
#undef QUAT_PRODUCT
#elif (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    /* GAS's AT&T `fsubrp` spelling encodes the original DE E9 FSUBP. */
    __asm__ __volatile__("flds 0(%0)\n\t"
                         "fmuls 12(%1)\n\t"
                         "flds 12(%0)\n\t"
                         "fmuls 0(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 8(%0)\n\t"
                         "fmuls 4(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 4(%0)\n\t"
                         "fmuls 8(%1)\n\t"
                         "fsubrp %%st, %%st(1)\n\t"
                         "fstps 0(%2)\n\t"
                         "flds 4(%0)\n\t"
                         "fmuls 12(%1)\n\t"
                         "flds 8(%0)\n\t"
                         "fmuls 0(%1)\n\t"
                         "fsubrp %%st, %%st(1)\n\t"
                         "flds 12(%0)\n\t"
                         "fmuls 4(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 0(%0)\n\t"
                         "fmuls 8(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstps 4(%2)\n\t"
                         "flds 8(%0)\n\t"
                         "fmuls 12(%1)\n\t"
                         "flds 4(%0)\n\t"
                         "fmuls 0(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 0(%0)\n\t"
                         "fmuls 4(%1)\n\t"
                         "fsubrp %%st, %%st(1)\n\t"
                         "flds 12(%0)\n\t"
                         "fmuls 8(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstps 8(%2)\n\t"
                         "flds 12(%0)\n\t"
                         "fmuls 12(%1)\n\t"
                         "flds 0(%0)\n\t"
                         "fmuls 0(%1)\n\t"
                         "fsubrp %%st, %%st(1)\n\t"
                         "flds 4(%0)\n\t"
                         "fmuls 4(%1)\n\t"
                         "fsubrp %%st, %%st(1)\n\t"
                         "flds 8(%0)\n\t"
                         "fmuls 8(%1)\n\t"
                         "fsubrp %%st, %%st(1)\n\t"
                         "fstps 12(%2)"
                         :
                         : "r"(first), "r"(second), "r"(product)
                         : "st", "st(1)", "memory");
#else
    product[0] = ((first[0] * second[3] + first[3] * second[0]) + first[2] * second[1]) - first[1] * second[2];
    product[1] = ((first[1] * second[3] - first[2] * second[0]) + first[3] * second[1]) + first[0] * second[2];
    product[2] = ((first[2] * second[3] + first[1] * second[0]) - first[0] * second[1]) + first[3] * second[2];
    product[3] = ((first[3] * second[3] - first[0] * second[0]) - first[1] * second[1]) - first[2] * second[2];
#endif
}
#endif
