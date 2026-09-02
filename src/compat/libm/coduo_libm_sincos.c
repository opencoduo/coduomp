/* CoduoLibm_Sin / CoduoLibm_Cos — reconstruction of the MSVC CRT routines.
 *
 * Stock: uo_gamex86.dll 0x2008a2e0 -> 0x2008a2fd (sin)
 *                       0x2008a230 -> 0x2008a24d (cos)
 *
 * The two bodies are identical apart from the transcendental and their error
 * codes (0x1e for sin, 0x12 for cos):
 *
 *   push edx / fstcw [esp]              ; save control word
 *   je   <inf/NaN>                      ; ZF from the classify helper
 *   cmp  [esp],0x27f
 *   fldcw ds:0x200a1778                 ; CW := 0x027F  (PC = 53-bit)
 *   fsin / fcos
 *   fstsw ax / sahf / jp <reduce>       ; C2 set => |x| >= 2^63, out of range
 *   ...
 *  <reduce>:
 *   fld  TBYTE ds:0x200a177a            ; pi * 2^62
 *   fxch st(1)
 *  1: fprem1 / fstsw ax / sahf / jp 1b  ; partial remainder, loop until done
 *   fstp st(1)
 *   fsin / fcos                         ; retry, now in range
 *
 * Two things here are unlike the rest of the CRT math surface.
 *
 * First, these are the ONLY routines in the CRT that change the precision
 * control field: they force 0x027F, i.e. PC = 53-bit, for the duration and
 * restore on exit. Everything else preserves the ambient 64-bit precision. See
 * docs/x87-transcendental-reconstruction-scope.md.
 *
 * Second, the reduction modulus is pi * 2^62 (significand 0xC90FDAA22168C235,
 * exponent 0x403E), not 2*pi. It is an exact multiple of 2*pi, so reducing by
 * it leaves sin/cos unchanged while bringing |x| below the 2^63 limit at which
 * FSIN/FCOS set C2 and decline to compute. Game angles never reach that
 * magnitude, so this path is effectively unreachable in practice, but it is
 * reproduced for completeness.
 *
 * C2 is bit 10 of the status word. The stock sequence FSTSW/SAHF/JP routes it
 * through the parity flag; TEST $0x400 is used here instead, which is the same
 * predicate without depending on SAHF.
 *
 *   +-inf -> indefinite QNaN (domain error)
 *   NaN   -> quieted
 *
 * EMULATION STATUS: none. These depend on FSIN and FCOS, which have no
 * SoftFloat equivalent and await the x87 transcendental reverse engineering. On
 * EMULATE_X87 builds they fall through to the platform libm -- a known,
 * deliberate gap; see the table in coduo_libm.h.
 */
#include "coduo_libm.h"
#include "compat/coduo_x87emu.h"

#include <math.h>
#include <string.h>

#define SINCOS_HIGH_EXPONENT_MASK 0x7ff00000u
#define SINCOS_HIGH_MANTISSA_MASK 0x000fffffu
#define SINCOS_CONTROL_WORD 0x027fu /* ds:0x200a1778 -- PC = 53-bit */

#if !EMULATE_X87
/* TBYTE ds:0x200a177a — pi * 2^62, built from the stock bytes so it is exact. */
static const union {
    unsigned char raw[sizeof(long double)];
    long double value;
} kSinCosReduction = {{0x35, 0xc2, 0x68, 0x21, 0xa2, 0xda, 0x0f, 0xc9, 0x3e, 0x40}};

static uint32_t SinCosHighDword(double value)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return (uint32_t)(bits >> 32);
}

static uint32_t SinCosLowDword(double value)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return (uint32_t)bits;
}

/* TBYTE 0x200b0180: the x87 indefinite QNaN loaded on a domain error. */
static double SinCosIndefiniteNaN(void)
{
    const uint64_t bits = UINT64_C(0xfff8000000000000);
    double result;

    memcpy(&result, &bits, sizeof(result));
    return result;
}

static unsigned short SinCosEnterControlWord(void)
{
    unsigned short saved;
    const unsigned short entry = (unsigned short)SINCOS_CONTROL_WORD;

    __asm__ __volatile__("fstcw %0" : "=m"(saved));
    if (saved != entry) {
        __asm__ __volatile__("fldcw %0" : : "m"(entry));
    }
    return saved;
}

static void SinCosRestoreControlWord(unsigned short saved)
{
    __asm__ __volatile__("fldcw %0" : : "m"(saved));
}
#endif /* !EMULATE_X87 */

double CoduoLibm_Sin(double value)
{
#if EMULATE_X87
    /* No emulation: depends on FSIN. Documented gap; platform libm is used. */
    return sin(value);
#else
    const uint32_t high = SinCosHighDword(value);
    const uint32_t low = SinCosLowDword(value);
    unsigned short saved;
    long double result;

    if ((high & SINCOS_HIGH_EXPONENT_MASK) == SINCOS_HIGH_EXPONENT_MASK) {
        if ((high & SINCOS_HIGH_MANTISSA_MASK) != 0u || low != 0u) {
            return (double)((long double)value + 1.0L); /* quiet the NaN */
        }
        return SinCosIndefiniteNaN(); /* +-inf is a domain error */
    }

    saved = SinCosEnterControlWord();
    __asm__ __volatile__("fsin\n\t"
                         "fnstsw %%ax\n\t"
                         "testl $0x400, %%eax\n\t"
                         "jz 1f\n\t"
                         "fldt %1\n\t"
                         "fxch %%st(1)\n\t"
                         "2:\n\t"
                         "fprem1\n\t"
                         "fnstsw %%ax\n\t"
                         "testl $0x400, %%eax\n\t"
                         "jnz 2b\n\t"
                         "fstp %%st(1)\n\t"
                         "fsin\n\t"
                         "1:\n\t"
                         : "=t"(result)
                         : "m"(kSinCosReduction.value), "0"((long double)value)
                         : "eax", "cc", "st(1)");
    SinCosRestoreControlWord(saved);
    return (double)result;
#endif
}

double CoduoLibm_Cos(double value)
{
#if EMULATE_X87
    /* No emulation: depends on FCOS. Documented gap; platform libm is used. */
    return cos(value);
#else
    const uint32_t high = SinCosHighDword(value);
    const uint32_t low = SinCosLowDword(value);
    unsigned short saved;
    long double result;

    if ((high & SINCOS_HIGH_EXPONENT_MASK) == SINCOS_HIGH_EXPONENT_MASK) {
        if ((high & SINCOS_HIGH_MANTISSA_MASK) != 0u || low != 0u) {
            return (double)((long double)value + 1.0L); /* quiet the NaN */
        }
        return SinCosIndefiniteNaN(); /* +-inf is a domain error */
    }

    saved = SinCosEnterControlWord();
    __asm__ __volatile__("fcos\n\t"
                         "fnstsw %%ax\n\t"
                         "testl $0x400, %%eax\n\t"
                         "jz 1f\n\t"
                         "fldt %1\n\t"
                         "fxch %%st(1)\n\t"
                         "2:\n\t"
                         "fprem1\n\t"
                         "fnstsw %%ax\n\t"
                         "testl $0x400, %%eax\n\t"
                         "jnz 2b\n\t"
                         "fstp %%st(1)\n\t"
                         "fcos\n\t"
                         "1:\n\t"
                         : "=t"(result)
                         : "m"(kSinCosReduction.value), "0"((long double)value)
                         : "eax", "cc", "st(1)");
    SinCosRestoreControlWord(saved);
    return (double)result;
#endif
}
