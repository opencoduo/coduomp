/* CoduoLibm_Sqrt — reconstruction of the MSVC CRT sqrt.
 *
 * Stock: uo_gamex86.dll 0x2008a640 (public entry) -> 0x2008a65d (body).
 *
 *   2008a643  fst   QWORD PTR [esp]        ; argument kept as a double
 *   2008a646  call  0x2008aa88             ; classify: ZF set iff exponent is all ones
 *   2008a666  je    0x2008a6b9             ; -> inf/NaN path (flags survive the call)
 *   2008a668  cmp   WORD PTR [esp],0x27f
 *   2008a670  call  0x2008aa15             ; control word := (saved & 0x300) | 0x7f
 *   2008a675  test  eax,0x80000000
 *   2008a67a  jne   0x2008a69b             ; -> negative path
 *   2008a67c  fsqrt
 *
 * The control-word helper preserves the precision-control field and only forces
 * round-to-nearest with all exceptions masked. With the ambient word 0x137F it
 * yields 0x37F, so the root is taken at 64-bit extended precision and narrowed
 * to double on return. That double rounding is stock behaviour.
 *
 * Negative path (0x2008a69b) distinguishes -0.0 from a genuine negative: only
 * when exponent, mantissa and low dword are all zero does it rejoin the normal
 * exit, returning -0.0 without executing FSQRT. Anything else is a domain error
 * and loads the indefinite QNaN from 0x200b0180.
 *
 * Inf/NaN path (0x2008a6b9): +inf returns +inf with FSQRT skipped; -inf falls
 * through to the domain error; a NaN is quieted (0x2008aa2c adds 1.0, which
 * turns a signalling NaN into a quiet one).
 */
#include "coduo_libm.h"
#include "compat/coduo_x87emu.h"

#include <string.h>

#define SQRT_HIGH_SIGN_MASK 0x80000000u
#define SQRT_HIGH_EXPONENT_MASK 0x7ff00000u
#define SQRT_HIGH_MANTISSA_MASK 0x000fffffu

static uint32_t CoduoLibm_DoubleHighDword(double value)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return (uint32_t)(bits >> 32);
}

static uint32_t CoduoLibm_DoubleLowDword(double value)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return (uint32_t)bits;
}

#if EMULATE_X87
/* Correctly-rounded double square root (SoftFloat), equal to FSQRT at PC=53. */
static double f64_sqrt_wrapper(double value)
{
    float64_t in;
    float64_t out;
    double result;

    memcpy(&in, &value, sizeof(in));
    out = f64_sqrt(in);
    memcpy(&result, &out, sizeof(result));
    return result;
}
#endif

/* The x87 indefinite QNaN the CRT loads from 0x200b0180 on a domain error. */
static double CoduoLibm_IndefiniteNaN(void)
{
    const uint64_t bits = UINT64_C(0xfff8000000000000);
    double result;

    memcpy(&result, &bits, sizeof(result));
    return result;
}

double CoduoLibm_Sqrt(double value)
{
    const uint32_t high = CoduoLibm_DoubleHighDword(value);
    const uint32_t low = CoduoLibm_DoubleLowDword(value);
    const uint32_t exponent = high & SQRT_HIGH_EXPONENT_MASK;
    const uint32_t mantissa = high & SQRT_HIGH_MANTISSA_MASK;

    if (exponent == SQRT_HIGH_EXPONENT_MASK) {
        /* 0x2008a6b9: infinity or NaN. */
        if (mantissa != 0u || low != 0u) {
            /* NaN: quieted by the add at 0x2008aa39, value otherwise kept. */
#if EMULATE_X87
            return x87f_store_f64(x87f_add(x87f_load_f64(value), x87f_load_f64(1.0)));
#else
            return (double)((long double)value + 1.0L);
#endif
        }
        if ((high & SQRT_HIGH_SIGN_MASK) == 0u) {
            return value; /* +inf: FSQRT skipped, argument returned as is. */
        }
        return CoduoLibm_IndefiniteNaN(); /* -inf: domain error. */
    }

    if ((high & SQRT_HIGH_SIGN_MASK) != 0u) {
        /* 0x2008a69b: negative, unless every magnitude bit is clear (-0.0). */
        if (exponent != 0u || mantissa != 0u || low != 0u) {
            return CoduoLibm_IndefiniteNaN();
        }
        return value; /* -0.0 rejoins the normal exit without FSQRT. */
    }

    /* 0x2008a67c: FSQRT at the ambient 64-bit precision, narrowed on return. */
#if EMULATE_X87
    return x87f_store_f64(x87f_sqrt(x87f_load_f64(value)));
#else
    {
        /* FSQRT emitted directly rather than via sqrtl(), so this carries no
         * dependency on the platform libm. */
        long double root;

        __asm__ __volatile__("fsqrt" : "=t"(root) : "0"((long double)value));
        return (double)root;
    }
#endif
}

/* glibc/i386 sqrt — the Linux server's actual reference.
 *
 * glibc does NOT match the MSVC CRT here. __sqrt_finite clears bit 8 of the
 * control word before the FSQRT:
 *
 *   fstcw 0x4(%esp)          ; save
 *   mov   $0xfeff,%edx
 *   and   0x4(%esp),%edx     ; PC 11b (64-bit) -> 10b (53-bit)
 *   fldcw (%esp)
 *   fsqrt                    ; computed at 53-bit
 *   fldcw 0x4(%esp)          ; restore
 *
 * so the double result comes from a SINGLE rounding, whereas the MSVC CRT
 * computes at 64-bit and lets the caller narrow — a double rounding. The two
 * disagree for roughly 1 input in 4100 (measured: 9749 of 40e6). Reconstructions
 * that target the Linux binaries must use this variant.
 *
 * Rounding to 53 bits and then storing to a double is exact, so this is simply
 * the correctly-rounded double square root.
 */
double CoduoLibm_SqrtGlibc(double value)
{
    const uint32_t high = CoduoLibm_DoubleHighDword(value);
    const uint32_t low = CoduoLibm_DoubleLowDword(value);
    const uint32_t exponent = high & SQRT_HIGH_EXPONENT_MASK;
    const uint32_t mantissa = high & SQRT_HIGH_MANTISSA_MASK;

    if (exponent == SQRT_HIGH_EXPONENT_MASK) {
        if (mantissa != 0u || low != 0u) {
#if EMULATE_X87
            return x87f_store_f64(x87f_add(x87f_load_f64(value), x87f_load_f64(1.0)));
#else
            return (double)((long double)value + 1.0L);
#endif
        }
        if ((high & SQRT_HIGH_SIGN_MASK) == 0u) {
            return value;
        }
        return CoduoLibm_IndefiniteNaN();
    }

    if ((high & SQRT_HIGH_SIGN_MASK) != 0u && (exponent != 0u || mantissa != 0u || low != 0u)) {
        return CoduoLibm_IndefiniteNaN();
    }

#if EMULATE_X87
    /* Correctly-rounded double root: exactly what FSQRT at PC=53 produces. */
    return f64_sqrt_wrapper(value);
#else
    {
        unsigned short saved;
        unsigned short narrowed;
        long double root;

        __asm__ __volatile__("fstcw %0" : "=m"(saved));
        narrowed = (unsigned short)(saved & 0xfeffu);
        __asm__ __volatile__("fldcw %0" : : "m"(narrowed));
        __asm__ __volatile__("fsqrt" : "=t"(root) : "0"((long double)value));
        __asm__ __volatile__("fldcw %0" : : "m"(saved));
        return (double)root;
    }
#endif
}
