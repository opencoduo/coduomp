#ifndef CODUO_INT32_BITS_H
#define CODUO_INT32_BITS_H

#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER)
#define CODUO_INT32_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define CODUO_INT32_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define CODUO_INT32_ALWAYS_INLINE inline
#endif

/* NOT_FROM_ORIGINAL_SOURCE: portable spelling of original byte and 32-bit
 * register operations. The stock i386 code treats signed values as two's-
 * complement bytes/dwords and uses SAR/SHL/IMUL wrap semantics. These helpers
 * preserve those exact values and bits without relying on implementation-
 * defined signed conversion/right shift or undefined signed left overflow. */
static CODUO_INT32_ALWAYS_INLINE int32_t coduo_int32_from_bits(uint32_t bits)
{
    int32_t value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static CODUO_INT32_ALWAYS_INLINE uint32_t coduo_int32_bits(int32_t value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static CODUO_INT32_ALWAYS_INLINE int32_t coduo_int8_from_bits(uint8_t bits)
{
    return bits < UINT8_C(0x80) ? (int32_t)bits : (int32_t)bits - INT32_C(0x100);
}

static CODUO_INT32_ALWAYS_INLINE uint32_t coduo_int32_sar_bits(uint32_t bits, unsigned int count)
{
    count &= 31U;
    if (count == 0U) {
        return bits;
    }

    return (bits >> count) | ((UINT32_C(0) - (bits >> 31U)) << (32U - count));
}

static CODUO_INT32_ALWAYS_INLINE int32_t coduo_int32_sar(uint32_t bits, unsigned int count)
{
    return coduo_int32_from_bits(coduo_int32_sar_bits(bits, count));
}

#undef CODUO_INT32_ALWAYS_INLINE

#endif /* CODUO_INT32_BITS_H */
