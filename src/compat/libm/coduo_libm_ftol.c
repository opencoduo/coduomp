/* CoduoLibm_FloatToInt64 — reconstruction of MSVC _ftol2.
 *
 * Byte-identical multiplayer copies: CoDUOMP.exe 0x00570fd8,
 * uo_cgame_mp_x86.dll 0x3006be3c, uo_ui_mp_x86.dll 0x4002cda0, and
 * uo_game_mp_x86.dll 0x20069c4c.
 *
 *   20069c55  fld   st(0)                 ; duplicate the argument
 *   20069c57  fst   DWORD PTR [esp+0x18]  ; keep x as float32, for its sign bit
 *   20069c5b  fistp QWORD PTR [esp+0x10]  ; n = rint(x), ambient round-to-nearest
 *   20069c5f  fild  QWORD PTR [esp+0x10]  ; back to the stack as a float
 *   20069c6b  test  eax,eax               ; low dword of n
 *   20069c6d  je    0x20069cab            ; -> zero/indefinite path
 *   20069c6f  fsubp st(1),st              ; r = x - n
 *   20069c71  test  edx,edx / jns         ; sign of x (from the float32 store)
 *   negative: xor ecx,0x80000000 ; add ecx,0x7fffffff ; adc -> n += (r > 0)
 *   positive:                      add ecx,0x7fffffff ; sbb -> n -= (r < 0)
 *
 * The FPU converts with the ambient rounding mode (nearest), so the CRT
 * corrects by one afterwards to obtain truncation toward zero. The correction
 * tests the residual *after it has been rounded to float32*, which is why the
 * float32 store is reproduced rather than testing the residual directly.
 *
 * `add ecx,0x7fffffff` sets CF exactly when ecx >= 0x80000001, i.e. when the
 * float32 residual is strictly negative (-0.0 does not trigger it).
 */
#include "coduo_libm.h"
#include "compat/coduo_x87emu.h"

#include <string.h>

#define FTOL_CARRY_THRESHOLD 0x80000001u
#define FTOL_SIGN_MASK       0x80000000u

static uint32_t CoduoLibm_Float32Bits(float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

int64_t CoduoLibm_FloatToInt64(double value)
{
    uint32_t valueBits;
    uint32_t residualBits;
    int64_t rounded;

#if EMULATE_X87
    x87f x = x87f_load_f64(value);

    /* fst DWORD: x narrowed to float32, kept only for its sign bit. */
    valueBits = CoduoLibm_Float32Bits(x87f_store_f32(x));

    /* fistp QWORD: ambient rounding is round-to-nearest-even. */
    rounded = (int64_t)extF80_to_i64(x, softfloat_round_near_even, false);

    /* fild QWORD then fsubp: residual r = x - n, then stored as float32. */
    residualBits = CoduoLibm_Float32Bits(
        x87f_store_f32(x87f_sub(x, x87f_load_f64((double)rounded))));
#else
    long double x = (long double)value;

    valueBits = CoduoLibm_Float32Bits((float)x);

    /* FISTP QWORD with the ambient rounding mode. Emitted directly rather than
     * via llrintl(), so this carries no dependency on the platform libm. */
    __asm__ __volatile__("fistpll %0" : "=m"(rounded) : "t"(x) : "st");

    residualBits = CoduoLibm_Float32Bits((float)(x - (long double)rounded));
#endif

    /* 0x20069cab: when the low dword is zero and the high dword carries no
     * magnitude bits, n is either 0 or the x87 indefinite value; both are
     * returned unmodified. */
    if ((uint32_t)rounded == 0u &&
        ((uint32_t)((uint64_t)rounded >> 32) & ~FTOL_SIGN_MASK) == 0u) {
        return rounded;
    }

    if ((valueBits & FTOL_SIGN_MASK) != 0u) {
        /* Negative x: step up when the residual is strictly positive. */
        if ((residualBits ^ FTOL_SIGN_MASK) >= FTOL_CARRY_THRESHOLD) {
            rounded += 1;
        }
    } else {
        /* Non-negative x: step down when the residual is strictly negative. */
        if (residualBits >= FTOL_CARRY_THRESHOLD) {
            rounded -= 1;
        }
    }

    return rounded;
}

int32_t CoduoLibm_FloatToInt32(double value)
{
    /* Callers that consume only EAX take the low dword of the int64 result. */
    return (int32_t)(uint32_t)(uint64_t)CoduoLibm_FloatToInt64(value);
}
