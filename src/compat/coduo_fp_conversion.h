/*
 * Shared source adapters for the platform-selected floating-point-to-integer
 * compiler lowering used by the recovered multiplayer binaries.
 */
#ifndef CODUO_FP_CONVERSION_H
#define CODUO_FP_CONVERSION_H

#include <stdint.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#if defined(__GNUC__) || defined(__clang__)
#define CODUO_FP_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define CODUO_FP_ALWAYS_INLINE inline
#endif

/*
 * NOT_FROM_ORIGINAL_SOURCE: shared expression of the final integer result
 * produced by the original compiler conversions. WINDOWS_BEHAVIOR models the
 * low dword of MSVC `_ftol2`'s signed-qword result; its masked invalid result
 * is INT64_MIN and therefore has a zero low dword. LINUX_BEHAVIOR models the
 * Linux binaries' direct signed-dword truncation; its masked invalid result is
 * INT32_MIN. Ordered range comparisons also reject NaNs and infinities.
 *
 * CALL-SITE VERIFICATION REQUIRED: choosing one of these adapters proves only
 * the final conversion width and invalid-result bits. Every caller must still
 * be checked against its own machine code for the operand's original load
 * width, intervening float/double spills and reloads, x87 precision policy,
 * literal widths, and the complete arithmetic operation graph. In particular,
 * passing a value through the binary64 adapter or adding a binary32 cast can
 * round a live x87 expression and change behavior before this conversion.
 */
static CODUO_FP_ALWAYS_INLINE int32_t coduo_fp_to_i32_extended(long double value)
{
#if defined(WINDOWS_BEHAVIOR)
    uint32_t lowBits = 0;
    int32_t result;

    /* The byte-identical Windows helper first performs a nearest-mode
     * signed-qword FISTP (uo_game_mp_x86.dll 0x20069c5b) and only then corrects
     * the finite result toward zero. At +2^63-0.5 that first step is already
     * integer-indefinite, so its low dword is zero rather than -1. */
    if (value >= -0x1p63L && value < 0x1p63L - 0.5L) {
        lowBits = (uint32_t)(uint64_t)(int64_t)value;
    }
    memcpy(&result, &lowBits, sizeof(result));
    return result;
#else
    if (!(value >= -0x1p31L && value < 0x1p31L)) {
        return INT32_MIN;
    }
    return (int32_t)value;
#endif
}

/*
 * NOT_FROM_ORIGINAL_SOURCE: binary64-input companion. The volatile automatic
 * object preserves a proved m64/ABI rounding boundary even when this adapter
 * is inlined into an x87 build that otherwise retains excess precision.
 */
static CODUO_FP_ALWAYS_INLINE int32_t coduo_fp_to_i32_f64(double value)
{
    volatile double roundedValue = value;
    return coduo_fp_to_i32_extended((long double)roundedValue);
}

/* NOT_FROM_ORIGINAL_SOURCE: unsigned low-dword view of the same conversion. */
static CODUO_FP_ALWAYS_INLINE uint32_t coduo_fp_to_u32_extended(long double value)
{
    return (uint32_t)coduo_fp_to_i32_extended(value);
}

/* NOT_FROM_ORIGINAL_SOURCE: low-byte view used by original AL consumers. */
static CODUO_FP_ALWAYS_INLINE uint8_t coduo_fp_to_u8_extended(long double value)
{
    return (uint8_t)coduo_fp_to_i32_extended(value);
}

#undef CODUO_FP_ALWAYS_INLINE

#endif
