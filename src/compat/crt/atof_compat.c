#include "compat/crt/msvc_compat.h"
#include "compat/coduo_int32_bits.h"

#include <string.h>

/* Windows C-locale decimal-to-double conversion. Function RVAs identify the
 * game module's CRT copy. Parsing never delegates to the host CRT or x87 emulation. */
enum {
    CODUO_CRT_DECIMAL_LIMBS = 5,
    CODUO_CRT_PRODUCT_LIMBS = 6,
    CODUO_CRT_LIMB_BITS = 16,
    CODUO_CRT_EXTENDED_BIAS = 16383,
    CODUO_CRT_EXTENDED_INFINITY = 32767,
    CODUO_CRT_PRODUCT_OVERFLOW_SUM = 49149,
    CODUO_CRT_PRODUCT_UNDERFLOW_SUM = 16319,
    CODUO_CRT_POWER_GROUP_BITS = 3,
    CODUO_CRT_POWER_GROUP_SIZE = 7
};

/* An 80-bit integer significand and a separate biased exponent. This is the
 * decimal converter's working format, not the host long-double format. */
typedef struct {
    uint16_t significand[CODUO_CRT_DECIMAL_LIMBS];
    uint16_t exponent;
} coduo_crt_decimal80_t;

/* NOT_FROM_ORIGINAL_SOURCE: unsigned limb arithmetic shared by normalization
 * and the decimal power multiplication. Limbs run from least to most significant. */
static void coduo_crt_shift_limbs_left(uint16_t *limbs, size_t count)
{
    uint32_t carry = 0;
    for (size_t i = 0; i < count; ++i) {
        uint32_t value = ((uint32_t)limbs[i] << 1) | carry;
        limbs[i] = (uint16_t)value;
        carry = value >> CODUO_CRT_LIMB_BITS;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: bounded unsigned shift used for gradual underflow. */
static void coduo_crt_shift_limbs_right(uint16_t *limbs, size_t count)
{
    uint16_t carry = 0;
    while (count != 0) {
        --count;
        uint16_t next = (uint16_t)(limbs[count] << (CODUO_CRT_LIMB_BITS - 1));
        limbs[count] = (uint16_t)((limbs[count] >> 1) | carry);
        carry = next;
    }
}

/* RVA 0x6788a: decimal digits are accumulated as an integer, then normalized with bit 79
 * explicit. The parser supplies at most 24 digits and a nonzero value. */
static coduo_crt_decimal80_t coduo_crt_digits_to_decimal80(const unsigned char *digits, size_t count)
{
    coduo_crt_decimal80_t value = {{0}, CODUO_CRT_EXTENDED_BIAS + CODUO_CRT_DECIMAL_LIMBS * CODUO_CRT_LIMB_BITS - 1};
    for (size_t digit = 0; digit < count; ++digit) {
        uint32_t carry = digits[digit];
        for (size_t i = 0; i < CODUO_CRT_DECIMAL_LIMBS; ++i) {
            uint32_t lane = (uint32_t)value.significand[i] * 10u + carry;
            value.significand[i] = (uint16_t)lane;
            carry = lane >> CODUO_CRT_LIMB_BITS;
        }
    }
    while ((value.significand[CODUO_CRT_DECIMAL_LIMBS - 1] & 0x8000u) == 0) {
        coduo_crt_shift_limbs_left(value.significand, CODUO_CRT_DECIMAL_LIMBS);
        --value.exponent;
    }
    return value;
}

/* RVA 0x67968: multiply the parser's nonnegative working values. The CRT accumulates only
 * product diagonals 4 through 8, then rounds the remaining 80 bits to nearest,
 * ties to even. Carries from the omitted low diagonals are deliberately absent. */
static void coduo_crt_multiply_decimal80(coduo_crt_decimal80_t *value, const coduo_crt_decimal80_t *factor)
{
    uint16_t product[CODUO_CRT_PRODUCT_LIMBS] = {0};
    int exponent = value->exponent + factor->exponent;

    if (value->exponent >= CODUO_CRT_EXTENDED_INFINITY || factor->exponent >= CODUO_CRT_EXTENDED_INFINITY || exponent > CODUO_CRT_PRODUCT_OVERFLOW_SUM) {
        *value = (coduo_crt_decimal80_t){{0, 0, 0, 0, 0x8000}, CODUO_CRT_EXTENDED_INFINITY};
        return;
    }
    if (exponent <= CODUO_CRT_PRODUCT_UNDERFLOW_SUM) {
        *value = (coduo_crt_decimal80_t){{0}, 0};
        return;
    }
    if (value->exponent == 0) {
        ++exponent;
        uint16_t nonzero = 0;
        for (size_t i = 0; i < CODUO_CRT_DECIMAL_LIMBS; ++i)
            nonzero |= value->significand[i];
        if (nonzero == 0)
            return;
    }
    if (factor->exponent == 0)
        ++exponent;

    for (size_t diagonal = CODUO_CRT_DECIMAL_LIMBS - 1; diagonal < 2 * CODUO_CRT_DECIMAL_LIMBS - 1; ++diagonal) {
        size_t lane = diagonal - (CODUO_CRT_DECIMAL_LIMBS - 1);
        for (size_t i = lane; i < CODUO_CRT_DECIMAL_LIMBS; ++i) {
            size_t j = diagonal - i;
            uint32_t term = (uint32_t)value->significand[i] * factor->significand[j];
            uint32_t previous = (uint32_t)product[lane] | ((uint32_t)product[lane + 1] << CODUO_CRT_LIMB_BITS);
            uint32_t sum = previous + term;
            product[lane] = (uint16_t)sum;
            product[lane + 1] = (uint16_t)(sum >> CODUO_CRT_LIMB_BITS);
            /* The last diagonal cannot carry: the complete retained product
             * is strictly smaller than 2^96 for two 80-bit significands. */
            if (sum < previous)
                ++product[lane + 2];
        }
    }
    exponent -= CODUO_CRT_EXTENDED_BIAS - 1;
    while (exponent > 0 && (product[CODUO_CRT_PRODUCT_LIMBS - 1] & 0x8000u) == 0) {
        coduo_crt_shift_limbs_left(product, CODUO_CRT_PRODUCT_LIMBS);
        --exponent;
    }
    if (exponent <= 0) {
        --exponent;
        uint16_t sticky = 0;
        while (exponent < 0) {
            sticky |= product[0] & 1u;
            coduo_crt_shift_limbs_right(product, CODUO_CRT_PRODUCT_LIMBS);
            ++exponent;
        }
        product[0] |= sticky;
    }
    if (product[0] > 0x8000u || (product[0] == 0x8000u && (product[1] & 1u) != 0)) {
        size_t lane = 1;
        while (lane < CODUO_CRT_PRODUCT_LIMBS && ++product[lane] == 0)
            ++lane;
        if (lane == CODUO_CRT_PRODUCT_LIMBS) {
            product[CODUO_CRT_PRODUCT_LIMBS - 1] = 0x8000;
            ++exponent;
        }
    }
    if (exponent >= CODUO_CRT_EXTENDED_INFINITY) {
        *value = (coduo_crt_decimal80_t){{0, 0, 0, 0, 0x8000}, CODUO_CRT_EXTENDED_INFINITY};
        return;
    }
    for (size_t i = 0; i < CODUO_CRT_DECIMAL_LIMBS; ++i)
        value->significand[i] = product[i + 1];
    value->exponent = (uint16_t)exponent;
}

/* Powers 10^(d * 8^group), d=1..7, through the final 10^4096 entry. */
static const coduo_crt_decimal80_t coduo_crt_positive_powers[] = {
    {{0x0000, 0x0000, 0x0000, 0x0000, 0xa000}, 0x4002},
    {{0x0000, 0x0000, 0x0000, 0x0000, 0xc800}, 0x4005},
    {{0x0000, 0x0000, 0x0000, 0x0000, 0xfa00}, 0x4008},
    {{0x0000, 0x0000, 0x0000, 0x0000, 0x9c40}, 0x400c},
    {{0x0000, 0x0000, 0x0000, 0x0000, 0xc350}, 0x400f},
    {{0x0000, 0x0000, 0x0000, 0x0000, 0xf424}, 0x4012},
    {{0x0000, 0x0000, 0x0000, 0x8000, 0x9896}, 0x4016},
    {{0x0000, 0x0000, 0x0000, 0x2000, 0xbebc}, 0x4019},
    {{0x0000, 0x0000, 0x0400, 0xc9bf, 0x8e1b}, 0x4034},
    {{0x0000, 0xa100, 0xcced, 0x1bce, 0xd3c2}, 0x404e},
    {{0xf020, 0xb59e, 0x2b70, 0xada8, 0x9dc5}, 0x4069},
    {{0x5dd0, 0x25fd, 0x1ae5, 0x4f8e, 0xeb19}, 0x4083},
    {{0x9671, 0x95d7, 0x0e43, 0x8d05, 0xaf29}, 0x409e},
    {{0xbff9, 0x44a0, 0x81ed, 0x8f12, 0x8281}, 0x40b9},
    {{0x3cbf, 0xa6d5, 0xffcf, 0x1f49, 0xc278}, 0x40d3},
    {{0xc66f, 0x8ce0, 0x80e9, 0x47c9, 0x93ba}, 0x41a8},
    {{0x85bc, 0x556b, 0x3927, 0xf78d, 0xe070}, 0x427c},
    {{0xddbc, 0xde8e, 0x9df9, 0xebfb, 0xaa7e}, 0x4351},
    {{0xe6a1, 0xe376, 0xf2cc, 0x2f29, 0x8184}, 0x4426},
    {{0x1028, 0xaa17, 0xaef8, 0xe310, 0xc4c5}, 0x44fa},
    {{0xa7eb, 0xf3d4, 0xebf7, 0x4ae1, 0x957a}, 0x45cf},
    {{0xcc65, 0x91c7, 0xa60e, 0xa0ae, 0xe319}, 0x46a3},
    {{0x650d, 0x0c17, 0x8175, 0x7586, 0xc976}, 0x4d48},
    {{0x4258, 0xa7e4, 0x3993, 0x353b, 0xb2b8}, 0x53ed},
    {{0xa74d, 0x5de5, 0xc53d, 0x3b5d, 0x9e8b}, 0x5a92},
    {{0x5dff, 0xf0a6, 0x20a1, 0x54c0, 0x8ca5}, 0x6137},
    {{0xfdd1, 0x5a8b, 0xd88b, 0x5d25, 0xf989}, 0x67db},
    {{0x95aa, 0xf3f8, 0xbf27, 0xc8a2, 0xdd5d}, 0x6e80},
    {{0xc94c, 0x979b, 0x8a20, 0x5202, 0xc460}, 0x7525},
};

static const coduo_crt_decimal80_t coduo_crt_negative_powers[] = {
    {{0xcccd, 0xcccd, 0xcccc, 0xcccc, 0xcccc}, 0x3ffb},
    {{0x3d71, 0xd70a, 0x70a3, 0x0a3d, 0xa3d7}, 0x3ff8},
    {{0x645a, 0xdf3b, 0x8d4f, 0x6e97, 0x8312}, 0x3ff5},
    {{0xd3c3, 0x652c, 0xe219, 0x1758, 0xd1b7}, 0x3ff1},
    {{0x0fd0, 0x8423, 0x1b47, 0xac47, 0xa7c5}, 0x3fee},
    {{0xa640, 0x69b6, 0xaf6c, 0xbd05, 0x8637}, 0x3feb},
    {{0x3d33, 0x42bc, 0xe57a, 0x94d5, 0xd6bf}, 0x3fe7},
    {{0xfdc2, 0xcefd, 0x8461, 0x7711, 0xabcc}, 0x3fe4},
    {{0x4c2f, 0xe15b, 0xc44d, 0x94be, 0xe695}, 0x3fc9},
    {{0xc492, 0x3b53, 0x4475, 0x14cd, 0x9abe}, 0x3faf},
    {{0x67de, 0x94ba, 0x4539, 0x1ead, 0xcfb1}, 0x3f94},
    {{0x2324, 0xe2c6, 0xbabc, 0x313b, 0x8b61}, 0x3f7a},
    {{0x5561, 0xc159, 0xb17e, 0x7c53, 0xbb12}, 0x3f5f},
    {{0xeed7, 0x8d2f, 0xbe06, 0x8592, 0xfb15}, 0x3f44},
    {{0x3f24, 0xe9a5, 0xa539, 0xea27, 0xa87f}, 0x3f2a},
    {{0xac7d, 0xe4a1, 0x64bc, 0x467c, 0xddd0}, 0x3e55},
    {{0x7b63, 0xcc06, 0x5423, 0x8377, 0x91ff}, 0x3d81},
    {{0xfa91, 0x193a, 0x637a, 0x4325, 0xc031}, 0x3cac},
    {{0x8921, 0x38d1, 0x4782, 0xb897, 0xfd00}, 0x3bd7},
    {{0x88dc, 0x0858, 0xb11b, 0xe3e8, 0xa686}, 0x3b03},
    {{0x84c6, 0x4245, 0xb607, 0x7599, 0xdb37}, 0x3a2e},
    {{0x7133, 0xd21c, 0xdb23, 0xee32, 0x9049}, 0x395a},
    {{0x87a6, 0xc0be, 0xda57, 0x82a5, 0xa2a6}, 0x32b5},
    {{0x68e2, 0x11b2, 0x52a7, 0x449f, 0xb759}, 0x2c10},
    {{0x4925, 0x2de4, 0x3436, 0x534f, 0xceae}, 0x256b},
    {{0x598f, 0xa404, 0xdec0, 0x7dc2, 0xe8fb}, 0x1ec6},
    {{0xe79e, 0x5a88, 0x9157, 0xbf3c, 0x8350}, 0x1822},
    {{0x4b4e, 0x6265, 0x83fd, 0xaf8f, 0x9406}, 0x117d},
    {{0x2de4, 0x9fde, 0xd2ce, 0x04c8, 0xa6dd}, 0x0ad8},
};

/* RVA 0x67b9a: apply decimal powers in increasing octal groups. Before the first multiply,
 * atof discards the low extension word. Power-table entries whose extension
 * is at least halfway have their next 32-bit lane decremented before use. */
static void coduo_crt_scale_decimal80(coduo_crt_decimal80_t *value, int exponent)
{
    const coduo_crt_decimal80_t *powers = coduo_crt_positive_powers;
    if (exponent == 0)
        return;
    if (exponent < 0) {
        exponent = -exponent;
        powers = coduo_crt_negative_powers;
    }
    value->significand[0] = 0;
    size_t group = 0;
    while (exponent != 0) {
        unsigned digit = (unsigned)exponent & CODUO_CRT_POWER_GROUP_SIZE;
        exponent >>= CODUO_CRT_POWER_GROUP_BITS;
        if (digit != 0) {
            coduo_crt_decimal80_t factor = powers[group * CODUO_CRT_POWER_GROUP_SIZE + digit - 1];
            if (factor.significand[0] >= 0x8000u) {
                uint32_t lane = (uint32_t)factor.significand[1] | ((uint32_t)factor.significand[2] << CODUO_CRT_LIMB_BITS);
                --lane;
                factor.significand[1] = (uint16_t)lane;
                factor.significand[2] = (uint16_t)(lane >> CODUO_CRT_LIMB_BITS);
            }
            coduo_crt_multiply_decimal80(value, &factor);
        }
        ++group;
    }
}

enum {
    CODUO_CRT_PARSE_UNDERFLOW = 1,
    CODUO_CRT_PARSE_OVERFLOW = 2,
    CODUO_CRT_PARSE_INVALID = 4,
    CODUO_CRT_DECIMAL_SIGN = 32768,
    CODUO_CRT_DECIMAL_INFINITY_EXPONENT = 32767,
    CODUO_CRT_DECIMAL_DIGITS = 24,
    CODUO_CRT_DECIMAL_BUFFER_DIGITS = 25,
    CODUO_CRT_DECIMAL_EXPONENT_LIMIT = 5200
};

/* NOT_FROM_ORIGINAL_SOURCE: the C-locale atof path of the decimal parser;
 * optional end-pointer and implicit-exponent modes are not exposed. */
static unsigned coduo_crt_parse_decimal80(const char *text, coduo_crt_decimal80_t *out)
{
    const unsigned char *cursor = (const unsigned char *)text;
    unsigned char digits[CODUO_CRT_DECIMAL_BUFFER_DIGITS];
    size_t count = 0;
    uint32_t decimalShift = 0;
    uint16_t sign = 0;
    int haveDigits = 0;
    int explicitExponent = 0;

    *out = (coduo_crt_decimal80_t){0};
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r') {
        ++cursor;
    }
    if (*cursor == '+' || *cursor == '-') {
        if (*cursor == '-') {
            sign = CODUO_CRT_DECIMAL_SIGN;
        }
        ++cursor;
    }

    /* Leading integer zeros do not occupy significant-digit slots. */
    while (*cursor == '0') {
        haveDigits = 1;
        ++cursor;
    }
    while (*cursor >= '0' && *cursor <= '9') {
        haveDigits = 1;
        if (count < sizeof(digits)) {
            digits[count++] = (unsigned char)(*cursor - '0');
        } else {
            ++decimalShift;
        }
        ++cursor;
    }

    if (*cursor == '.') {
        ++cursor;
        if (count == 0) {
            while (*cursor == '0') {
                haveDigits = 1;
                --decimalShift;
                ++cursor;
            }
        }
        while (*cursor >= '0' && *cursor <= '9') {
            haveDigits = 1;
            if (count < sizeof(digits)) {
                digits[count++] = (unsigned char)(*cursor - '0');
                --decimalShift;
            }
            ++cursor;
        }
    }

    out->exponent = sign;
    if (!haveDigits) {
        /* The caller maps this status to positive zero, even after '-'. */
        return CODUO_CRT_PARSE_INVALID;
    }

    if (*cursor == 'e' || *cursor == 'E' || *cursor == 'd' || *cursor == 'D') {
        int exponentSign = 1;
        ++cursor;
        if (*cursor == '+' || *cursor == '-') {
            if (*cursor == '-') {
                exponentSign = -1;
            }
            ++cursor;
        }
        /* A missing exponent digit leaves the mantissa's numeric value intact.
         * The original also rewinds endptr to the marker; atof discards it. */
        while (*cursor >= '0' && *cursor <= '9') {
            if (explicitExponent <= CODUO_CRT_DECIMAL_EXPONENT_LIMIT) {
                explicitExponent = explicitExponent * 10 + (*cursor - '0');
                if (explicitExponent > CODUO_CRT_DECIMAL_EXPONENT_LIMIT) {
                    explicitExponent = CODUO_CRT_DECIMAL_EXPONENT_LIMIT + 1;
                }
            }
            ++cursor;
        }
        explicitExponent *= exponentSign;
    }

    if (count > CODUO_CRT_DECIMAL_DIGITS) {
        /* RVA 0x64417 tests buffer[23], not the discarded buffer[24]. */
        if (digits[CODUO_CRT_DECIMAL_DIGITS - 1] >= 5) {
            ++digits[CODUO_CRT_DECIMAL_DIGITS - 1];
        }
        count = CODUO_CRT_DECIMAL_DIGITS;
        ++decimalShift;
    }
    if (count == 0) {
        return 0;
    }
    while (digits[count - 1] == 0) {
        --count;
        ++decimalShift;
    }

    *out = coduo_crt_digits_to_decimal80(digits, count);
    int32_t exponent = coduo_int32_from_bits(decimalShift + (uint32_t)explicitExponent);
    if (exponent > CODUO_CRT_DECIMAL_EXPONENT_LIMIT) {
        *out = (coduo_crt_decimal80_t){0};
        out->significand[4] = CODUO_CRT_DECIMAL_SIGN;
        out->exponent = CODUO_CRT_DECIMAL_INFINITY_EXPONENT | sign;
        return CODUO_CRT_PARSE_OVERFLOW;
    }
    if (exponent < -CODUO_CRT_DECIMAL_EXPONENT_LIMIT) {
        *out = (coduo_crt_decimal80_t){0};
        out->exponent = sign;
        return CODUO_CRT_PARSE_UNDERFLOW;
    }
    coduo_crt_scale_decimal80(out, exponent);
    out->exponent |= sign;
    return 0;
}

enum {
    CODUO_CRT_BINARY64_CONVERT_OK = 0,
    CODUO_CRT_BINARY64_CONVERT_OVERFLOW = 1,
    CODUO_CRT_BINARY64_CONVERT_UNDERFLOW = 2
};

/* RVA 63e7a: logical right shift of three most-significant-first words.
 * The binary64 converter calls this only with counts from 0 through 53. */
static void coduo_crt_atof_shift_right96(uint32_t words[3], unsigned count)
{
    const unsigned wholeWords = count / 32;
    const unsigned partialBits = count % 32;
    const uint32_t original[3] = {words[0], words[1], words[2]};

    for (unsigned target = 0; target != 3; ++target) {
        uint32_t value = 0;
        if (target >= wholeWords) {
            const unsigned source = target - wholeWords;
            value = original[source] >> partialBits;
            if (partialBits != 0 && source != 0)
                value |= original[source - 1] << (32 - partialBits);
        }
        words[target] = value;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: specializes RVA 63dc8 and its 63d49/63d7b/677b0
 * dependencies for the binary64 format's fixed precision of 53 bits.
 * Bit 0 in the original helper's indexing is the most significant bit.
 * Increment retained bit 52 only when bit 53 and some lower bit are set.
 * An exact halfway value is therefore left toward zero, not ties-to-even.
 * The guard bit itself is kept here; the final packing shift discards it. */
static int coduo_crt_atof_round53(uint32_t words[3])
{
    const uint32_t guardBit = UINT32_C(1) << 10;
    const uint32_t retainedIncrement = UINT32_C(1) << 11;
    const uint32_t lowerMask = guardBit - 1;
    int carry = 0;

    if ((words[1] & guardBit) != 0 &&
        ((words[1] & lowerMask) != 0 || words[2] != 0)) {
        const uint32_t oldMiddle = words[1];
        words[1] += retainedIncrement;
        if (words[1] < oldMiddle) {
            ++words[0];
            carry = words[0] == 0;
        }
    }
    words[1] &= ~lowerMask;
    words[2] = 0;
    return carry;
}

/* RVA 6404d selects format table 82c40 and calls 63ef5.
 * Format fields are {1024, -1023, 53, 11, 64, 1023}.
 * The result is the exact binary64 representation, independent of host FP.
 * Status 2 includes nonzero subnormals, not just a result rounded to zero. */
static int coduo_crt_atof_decimal80_to_binary64(const coduo_crt_decimal80_t *input, uint64_t *outputBits)
{
    const uint64_t sign = (uint64_t)(input->exponent & UINT16_C(0x8000)) << 48;
    const unsigned biasedExponent = input->exponent & UINT16_C(0x7fff);
    uint32_t words[3] = {
        (uint32_t)input->significand[3] | (uint32_t)input->significand[4] << 16,
        (uint32_t)input->significand[1] | (uint32_t)input->significand[2] << 16,
        (uint32_t)input->significand[0] << 16
    };
    const uint32_t saved[3] = {words[0], words[1], words[2]};
    int exponent = (int)biasedExponent - 16383;

    if (biasedExponent == 0) {
        *outputBits = sign;
        return (words[0] | words[1] | words[2]) == 0
            ? CODUO_CRT_BINARY64_CONVERT_OK
            : CODUO_CRT_BINARY64_CONVERT_UNDERFLOW;
    }

    exponent += coduo_crt_atof_round53(words);
    if (exponent < -1076) {
        *outputBits = sign;
        return CODUO_CRT_BINARY64_CONVERT_UNDERFLOW;
    }
    if (exponent <= -1023) {
        /* 63f95..63fc5 restores the unrounded significand but retains the
         * exponent after the first round's carry. The second carry is ignored.
         * Preserve this dependency and the final 12-bit shift literally. */
        words[0] = saved[0];
        words[1] = saved[1];
        words[2] = saved[2];
        coduo_crt_atof_shift_right96(words, (unsigned)(-1023 - exponent));
        (void)coduo_crt_atof_round53(words);
        coduo_crt_atof_shift_right96(words, 12);
        *outputBits = sign | (uint64_t)words[0] << 32 | words[1];
        return CODUO_CRT_BINARY64_CONVERT_UNDERFLOW;
    }
    if (exponent >= 1024) {
        *outputBits = sign | UINT64_C(0x7ff0000000000000);
        return CODUO_CRT_BINARY64_CONVERT_OVERFLOW;
    }

    words[0] &= UINT32_C(0x7fffffff);
    coduo_crt_atof_shift_right96(words, 11);
    *outputBits = sign | (uint64_t)(exponent + 1023) << 52 |
        (uint64_t)words[0] << 32 | words[1];
    return CODUO_CRT_BINARY64_CONVERT_OK;
}

/* RVA 0x5a08e: C-locale atof. Decimal parsing and binary64 rounding are performed with
 * integer arithmetic. Loading the completed representation supplies the normal
 * double return convention without depending on the host decimal parser. */
double coduo_crt_atof(const char *string)
{
    coduo_crt_decimal80_t decimal;
    uint64_t bits = 0;
    double result;

    while (*string == ' ' || (*string >= '\t' && *string <= '\r'))
        ++string;
    if ((coduo_crt_parse_decimal80(string, &decimal) & CODUO_CRT_PARSE_INVALID) == 0)
        (void)coduo_crt_atof_decimal80_to_binary64(&decimal, &bits);
    /* This constructs a double from its encoded IEEE representation. */
    memcpy(&result, &bits, sizeof(result));
    return result;
}
