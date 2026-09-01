/* CoduoLibm_Asin / CoduoLibm_Acos — reconstruction of the MSVC CRT routines.
 *
 * Stock: uo_gamex86.dll 0x2008a700 -> 0x2008a71d (asin)
 *                       0x2008a0e0 -> 0x2008a0fd (acos)
 *
 * The two bodies are identical apart from one instruction and their error
 * codes. Shared shape:
 *
 *   push edx / fstcw [esp]            ; save control word
 *   je   <inf/NaN>                    ; ZF from the classify helper 0x2008aa88
 *   cmp  [esp],0x27f / call 0x2008aa15; CW := (saved & 0x300) | 0x7f
 *   cmp  eax,0x3ff00000 / jae <|x|>=1>
 *   fld1 / fadd st,st(1)              ; 1+x
 *   fld1 / fsub st,st(2)              ; 1-x
 *   fmulp st(1),st                    ; (1+x)*(1-x)   <-- not 1-x*x
 *   fsqrt                             ; sqrt(1-x^2)
 *   [acos only] fxch st(1)
 *   fpatan                            ; atan(st(1)/st(0))
 *
 * acos leaves x in st(0) via the FXCH, giving atan(sqrt(1-x^2)/x); asin omits
 * it, giving atan(x/sqrt(1-x^2)). The 1-x^2 factoring as (1+x)(1-x) is stock
 * and is preserved deliberately — it is not algebraically interchangeable with
 * 1-x*x in floating point.
 *
 * The control-word helper preserves the precision-control field, so both run at
 * the ambient 64-bit precision (see docs/x87-transcendental-reconstruction-scope.md).
 *
 * |x| is classified from the exponent field alone, exactly as the stock code
 * does: the classify helper returns high & 0x7ff00000 for finite inputs, which
 * is compared against 0x3ff00000. Equality plus a zero mantissa means |x| == 1;
 * anything above means |x| > 1 and is a domain error.
 *
 *   |x| < 1   -> computed as above
 *   x == +1   -> asin: +pi/2 (TBYTE 0x200b018a)   acos: +0.0 (FLDZ)
 *   x == -1   -> asin: -pi/2 (FCHS of the same)   acos: pi   (FLDPI)
 *   |x| > 1   -> indefinite QNaN (TBYTE 0x200b0180), domain error
 *   NaN       -> quieted
 *
 * EMULATION STATUS: none. Both depend on FPATAN, which has no SoftFloat
 * equivalent and awaits the x87 transcendental reverse engineering. On
 * EMULATE_X87 builds these fall through to the platform libm, which is a known
 * and deliberate gap — see the table in coduo_libm.h.
 */
#include "coduo_libm.h"
#include "compat/coduo_x87emu.h"

#include <math.h>
#include <string.h>

#define ASIN_HIGH_SIGN_MASK     0x80000000u
#define ASIN_HIGH_EXPONENT_MASK 0x7ff00000u
#define ASIN_HIGH_MANTISSA_MASK 0x000fffffu
#define ASIN_EXPONENT_OF_ONE    0x3ff00000u

#if !EMULATE_X87
static uint32_t AsinHighDword(double value)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return (uint32_t)(bits >> 32);
}

static uint32_t AsinLowDword(double value)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return (uint32_t)bits;
}

/* TBYTE 0x200b0180: the x87 indefinite QNaN loaded on a domain error. */
static double AsinIndefiniteNaN(void)
{
    const uint64_t bits = UINT64_C(0xfff8000000000000);
    double result;

    memcpy(&result, &bits, sizeof(result));
    return result;
}

/* TBYTE 0x200b018a: pi/2 as an 80-bit constant, significand
 * 0xC90FDAA22168C235 with exponent 0x3FFF. Built from those bytes rather than
 * from a decimal literal so the value is exact. */
static long double AsinHalfPi(void)
{
    const unsigned char raw[10] = {
        0x35, 0xc2, 0x68, 0x21, 0xa2, 0xda, 0x0f, 0xc9, 0xff, 0x3f
    };
    long double value = 0.0L;

    memcpy(&value, raw, sizeof(raw));
    return value;
}

/* Helper 0x2008aa15: preserve the precision-control field, force
 * round-to-nearest with every exception masked. */
static unsigned short AsinEnterControlWord(void)
{
    unsigned short saved;
    unsigned short entry;

    __asm__ __volatile__("fstcw %0" : "=m"(saved));
    entry = (unsigned short)((saved & 0x0300u) | 0x007fu);
    __asm__ __volatile__("fldcw %0" : : "m"(entry));
    return saved;
}

static void AsinRestoreControlWord(unsigned short saved)
{
    __asm__ __volatile__("fldcw %0" : : "m"(saved));
}

/* (1+x)*(1-x) then FSQRT, matching the stock instruction order. */
static long double AsinRoot(long double x)
{
    long double t = (1.0L + x) * (1.0L - x);

    __asm__ __volatile__("fsqrt" : "=t"(t) : "0"(t));
    return t;
}
#endif /* !EMULATE_X87 */

double CoduoLibm_Asin(double value)
{
#if EMULATE_X87
    /* No emulation: depends on FPATAN. Documented gap; platform libm is used. */
    return asin(value);
#else
    const uint32_t high = AsinHighDword(value);
    const uint32_t low = AsinLowDword(value);
    const uint32_t exponent = high & ASIN_HIGH_EXPONENT_MASK;
    const uint32_t mantissa = high & ASIN_HIGH_MANTISSA_MASK;
    unsigned short saved;
    long double result;

    if (exponent == ASIN_HIGH_EXPONENT_MASK) {
        if (mantissa != 0u || low != 0u) {
            return (double)((long double)value + 1.0L); /* quiet the NaN */
        }
        return AsinIndefiniteNaN(); /* +-inf is a domain error */
    }

    if (exponent >= ASIN_EXPONENT_OF_ONE) {
        if (exponent != ASIN_EXPONENT_OF_ONE || mantissa != 0u || low != 0u) {
            return AsinIndefiniteNaN(); /* |x| > 1 */
        }
        /* |x| == 1: +-pi/2, the sign taken from x (FCHS at 0x2008a786). */
        result = AsinHalfPi();
        if ((high & ASIN_HIGH_SIGN_MASK) != 0u) {
            result = -result;
        }
        return (double)result;
    }

    saved = AsinEnterControlWord();
    {
        const long double x = (long double)value;
        const long double root = AsinRoot(x);

        /* No FXCH: st(0)=root, st(1)=x -> atan(x/root). */
        __asm__ __volatile__("fpatan" : "=t"(result) : "0"(root), "u"(x)
                             : "st(1)");
    }
    AsinRestoreControlWord(saved);
    return (double)result;
#endif
}

double CoduoLibm_Acos(double value)
{
#if EMULATE_X87
    /* No emulation: depends on FPATAN. Documented gap; platform libm is used. */
    return acos(value);
#else
    const uint32_t high = AsinHighDword(value);
    const uint32_t low = AsinLowDword(value);
    const uint32_t exponent = high & ASIN_HIGH_EXPONENT_MASK;
    const uint32_t mantissa = high & ASIN_HIGH_MANTISSA_MASK;
    unsigned short saved;
    long double result;

    if (exponent == ASIN_HIGH_EXPONENT_MASK) {
        if (mantissa != 0u || low != 0u) {
            return (double)((long double)value + 1.0L); /* quiet the NaN */
        }
        return AsinIndefiniteNaN(); /* +-inf is a domain error */
    }

    if (exponent >= ASIN_EXPONENT_OF_ONE) {
        if (exponent != ASIN_EXPONENT_OF_ONE || mantissa != 0u || low != 0u) {
            return AsinIndefiniteNaN(); /* |x| > 1 */
        }
        /* |x| == 1: FLDZ for +1 (0x2008a166), FLDPI for -1 (0x2008a162). */
        if ((high & ASIN_HIGH_SIGN_MASK) != 0u) {
            __asm__ __volatile__("fldpi" : "=t"(result));
            return (double)result;
        }
        return 0.0;
    }

    saved = AsinEnterControlWord();
    {
        const long double x = (long double)value;
        const long double root = AsinRoot(x);

        /* FXCH first: st(0)=x, st(1)=root -> atan(root/x). */
        __asm__ __volatile__("fpatan" : "=t"(result) : "0"(x), "u"(root)
                             : "st(1)");
    }
    AsinRestoreControlWord(saved);
    return (double)result;
#endif
}
