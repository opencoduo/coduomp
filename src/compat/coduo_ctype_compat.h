#ifndef CODUO_CTYPE_COMPAT_H
#define CODUO_CTYPE_COMPAT_H

#include "coduo_int32_bits.h"

/* NOT_FROM_ORIGINAL_SOURCE: preserve the original Linux/x86 signed-char ctype
 * input domain without passing an invalid negative value to a strict C
 * library. glibc deliberately accepts -128..-1 for old signed-char callers,
 * so the original target retains its exact input. Other hosts map -128..-2 to
 * the corresponding unsigned byte; -1 remains EOF. Call sites which the
 * machine code proves zero-extend a byte should keep an explicit unsigned-char
 * argument instead of using this adapter. */
static inline int coduo_ctype_signed_byte_arg(int value)
{
    int signedValue = (int)coduo_int8_from_bits((uint8_t)value);

#if defined(__GLIBC__)
    return signedValue;
#else
    return signedValue < -1 ? (int)(uint8_t)value : signedValue;
#endif
}

#endif /* CODUO_CTYPE_COMPAT_H */
