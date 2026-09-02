#include "q_checksum.h"
#include "q_memory.h"

#include <stddef.h>
#include <stdint.h>

#define MD4_INIT_STATE_0 UINT32_C(0x67452301)
#define MD4_INIT_STATE_1 UINT32_C(0xefcdab89)
#define MD4_INIT_STATE_2 UINT32_C(0x98badcfe)
#define MD4_INIT_STATE_3 UINT32_C(0x10325476)
#define MD4_BYTE_BITS 8
#define MD4_COUNT_HIGH_SHIFT 29
#define MD4_FINAL_LENGTH_SIZE 8
#define MD4_FINAL_COUNT_TARGET 56
#define MD4_FINAL_COUNT_TARGET_NEXT_BLOCK 120
#define COM_BLOCK_CHECKSUM_KEY_BYTES 4
#define COM_HASH_POSITION_BIAS 119U
#define COM_HASH_MIX_SHIFT 10U
#define COM_HASH_SIGN_FILL_SHIFT (32U - COM_HASH_MIX_SHIFT)
#define MD4_ROUND2_CONSTANT UINT32_C(0x5a827999)
#define MD4_ROUND3_CONSTANT UINT32_C(0x6ed9eba1)

enum {
    MD4_STATE_WORD_COUNT = 4,
    MD4_COUNT_WORD_COUNT = 2,
    MD4_BLOCK_SIZE = 64,
    MD4_DIGEST_SIZE = 16,
    MD4_BLOCK_MASK = MD4_BLOCK_SIZE - 1
};

/* RFC 1320 reference context identity used by MD4Init/Update/Final. */
typedef struct {
    uint32_t state[MD4_STATE_WORD_COUNT];
    uint32_t count[MD4_COUNT_WORD_COUNT];
    uint8_t buffer[MD4_BLOCK_SIZE];
} MD4_CTX;

_Static_assert(sizeof(MD4_CTX) == 0x58, "MD4_CTX size mismatch");
_Static_assert(offsetof(MD4_CTX, state) == 0x00, "MD4_CTX.state offset mismatch");
_Static_assert(offsetof(MD4_CTX, count) == 0x10, "MD4_CTX.count offset mismatch");
_Static_assert(offsetof(MD4_CTX, buffer) == 0x18, "MD4_CTX.buffer offset mismatch");

/*
 * The complete embedded MD4/checksum subsystem agrees between the two
 * authoritative executable targets:
 *
 *                               Windows CoDUOMP.exe   Linux coduo_lnxded
 * MD4Init                       0x00448c00             0x0807f628
 * MD4Update                     0x00448c30             0x0807f668
 * MD4Final                      0x00448cd0             0x0807f764
 * MD4Transform                  0x00448d90             0x0807f825
 * Encode                        0x004493f0             0x0807ff31
 * Decode                        0x00449430             0x0807ffd9
 * Com_BlockChecksum             0x00449470             0x08080055
 * Com_BlockChecksumKey          0x004494f0             0x080800b1
 * Com_HashKey                   0x00435480             0x0806ba16
 *
 * Both MD4 bodies use the same 0x58-byte context, little-endian word codec,
 * RFC 1320 round graph, bit-count updates, padding boundary, final context
 * clear, and decoded-word scratch clear. Both keyed wrappers update with the
 * four bytes of the native i386 key object before updating with the payload.
 * Com_HashKey uses signed input bytes, wrapping 32-bit multiply/add, and
 * arithmetic shifts by 10 and 20 on both targets.
 */
static uint8_t md4_padding[MD4_BLOCK_SIZE] = {0x80};

static void MD4Transform(MD4_CTX *context, const uint8_t block[MD4_BLOCK_SIZE]);
static void Encode(uint8_t *output, const uint32_t *input, uint32_t length);
static void Decode(uint32_t *output, const uint8_t *input, uint32_t length);

static void MD4Init(MD4_CTX *context)
{
    context->count[1] = 0;
    context->count[0] = 0;
    context->state[0] = MD4_INIT_STATE_0;
    context->state[1] = MD4_INIT_STATE_1;
    context->state[2] = MD4_INIT_STATE_2;
    context->state[3] = MD4_INIT_STATE_3;
}

static void MD4Update(MD4_CTX *context, const void *data, uint32_t length)
{
    const uint8_t *input = data;
    uint32_t inputIndex;
    uint32_t partLength;
    uint32_t copyIndex;
    uint32_t lengthBits;

    inputIndex = (context->count[0] >> 3) & MD4_BLOCK_MASK;
    lengthBits = length << 3;
    context->count[0] += lengthBits;
    if (context->count[0] < lengthBits) {
        ++context->count[1];
    }
    context->count[1] += length >> MD4_COUNT_HIGH_SHIFT;

    partLength = MD4_BLOCK_SIZE - inputIndex;
    if (length < partLength) {
        copyIndex = 0;
    } else {
        Com_Memcpy(&context->buffer[inputIndex], input, partLength);
        MD4Transform(context, context->buffer);
        for (copyIndex = partLength; copyIndex + MD4_BLOCK_SIZE - 1 < length; copyIndex += MD4_BLOCK_SIZE) {
            MD4Transform(context, &input[copyIndex]);
        }
        inputIndex = 0;
    }

    Com_Memcpy(&context->buffer[inputIndex], &input[copyIndex], length - copyIndex);
}

static void MD4Final(uint8_t digest[MD4_DIGEST_SIZE], MD4_CTX *context)
{
    uint8_t bits[MD4_FINAL_LENGTH_SIZE];
    uint32_t inputIndex;
    uint32_t padLength;

    Encode(bits, context->count, MD4_FINAL_LENGTH_SIZE);
    inputIndex = (context->count[0] >> 3) & MD4_BLOCK_MASK;
    if (inputIndex < MD4_FINAL_COUNT_TARGET) {
        padLength = MD4_FINAL_COUNT_TARGET - inputIndex;
    } else {
        padLength = MD4_FINAL_COUNT_TARGET_NEXT_BLOCK - inputIndex;
    }

    MD4Update(context, md4_padding, padLength);
    MD4Update(context, bits, MD4_FINAL_LENGTH_SIZE);
    Encode(digest, context->state, MD4_DIGEST_SIZE);

    Com_Memset(context, 0, sizeof(*context));
}

uint32_t Com_BlockChecksum(const void *buffer, int32_t length)
{
    MD4_CTX context;
    uint8_t digest[MD4_DIGEST_SIZE];
    uint32_t digestWords[MD4_STATE_WORD_COUNT];

    MD4Init(&context);
    MD4Update(&context, buffer, (uint32_t)length);
    MD4Final(digest, &context);
    /* The i386 authorities read the encoded digest as little-endian words.
     * Decode keeps that result independent of the reconstruction host; the
     * supporting PowerPC Mac wrapper instead used native big-endian loads. */
    Decode(digestWords, digest, MD4_DIGEST_SIZE);

    return digestWords[1] ^ digestWords[0] ^ digestWords[2] ^ digestWords[3];
}

uint32_t Com_BlockChecksumKey(const void *buffer, int32_t length, int32_t key)
{
    MD4_CTX context;
    uint8_t digest[MD4_DIGEST_SIZE];
    uint32_t digestWords[MD4_STATE_WORD_COUNT];
    const uint32_t keyValue = (uint32_t)key;
    const uint8_t keyBytes[COM_BLOCK_CHECKSUM_KEY_BYTES] = {(uint8_t)keyValue, (uint8_t)(keyValue >> 8U), (uint8_t)(keyValue >> 16U),
                                                            (uint8_t)(keyValue >> 24U)};

    MD4Init(&context);
    /* The Windows and Linux authorities are little-endian. The supporting
     * PowerPC Mac body at code offset 0x00050210 instead passes its native
     * big-endian key object. Materializing the authority byte sequence keeps
     * reconstruction behavior independent of the new host's byte order. */
    MD4Update(&context, keyBytes, COM_BLOCK_CHECKSUM_KEY_BYTES);
    MD4Update(&context, buffer, (uint32_t)length);
    MD4Final(digest, &context);
    Decode(digestWords, digest, MD4_DIGEST_SIZE);

    return digestWords[1] ^ digestWords[0] ^ digestWords[2] ^ digestWords[3];
}

uint32_t Com_HashKey(const char *text, int32_t length)
{
    uint32_t hash = 0;

    for (int32_t index = 0; index < length && text[index] != '\0'; ++index) {
        hash += ((uint32_t)index + COM_HASH_POSITION_BIAS) * (uint32_t)(int32_t)(int8_t)(uint8_t)text[index];
    }

    /* Arithmetic right shift is made explicit so the original SAR/SRAWI
     * result does not depend on implementation-defined signed right shift.
     * The Windows body performs the two shifts as a chained XOR; arithmetic
     * shift distributes over XOR, producing the same hash>>10/hash>>20 graph
     * emitted by Linux and the supporting Mac body at 0x000dead0. */
    const uint32_t signFill = 0U - (hash >> 31U);
    const uint32_t shifted10 = (hash >> COM_HASH_MIX_SHIFT) | (signFill << COM_HASH_SIGN_FILL_SHIFT);
    const uint32_t shifted20 = (hash >> (2U * COM_HASH_MIX_SHIFT)) | (signFill << (32U - (2U * COM_HASH_MIX_SHIFT)));

    return hash ^ shifted10 ^ shifted20;
}

static void MD4Transform(MD4_CTX *context, const uint8_t block[MD4_BLOCK_SIZE])
{
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t x[MD4_BLOCK_SIZE / sizeof(uint32_t)];

    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];

    Decode(x, block, MD4_BLOCK_SIZE);

#define MD4_ROTATE_LEFT(value, bits) (((value) << (bits)) | ((value) >> (32U - (bits))))
#define MD4_ROUND1(a_, b_, c_, d_, x_, s_) \
    do { \
        (a_) += (((~(b_)) & (d_)) | ((b_) & (c_))) + (x_); \
        (a_) = MD4_ROTATE_LEFT((a_), (s_)); \
    } while (0)
#define MD4_ROUND2(a_, b_, c_, d_, x_, s_) \
    do { \
        (a_) += (((b_) & (c_)) | (((b_) | (c_)) & (d_))) + (x_) + MD4_ROUND2_CONSTANT; \
        (a_) = MD4_ROTATE_LEFT((a_), (s_)); \
    } while (0)
#define MD4_ROUND3(a_, b_, c_, d_, x_, s_) \
    do { \
        (a_) += ((b_) ^ (c_) ^ (d_)) + (x_) + MD4_ROUND3_CONSTANT; \
        (a_) = MD4_ROTATE_LEFT((a_), (s_)); \
    } while (0)

    MD4_ROUND1(a, b, c, d, x[0], 3);
    MD4_ROUND1(d, a, b, c, x[1], 7);
    MD4_ROUND1(c, d, a, b, x[2], 11);
    MD4_ROUND1(b, c, d, a, x[3], 19);
    MD4_ROUND1(a, b, c, d, x[4], 3);
    MD4_ROUND1(d, a, b, c, x[5], 7);
    MD4_ROUND1(c, d, a, b, x[6], 11);
    MD4_ROUND1(b, c, d, a, x[7], 19);
    MD4_ROUND1(a, b, c, d, x[8], 3);
    MD4_ROUND1(d, a, b, c, x[9], 7);
    MD4_ROUND1(c, d, a, b, x[10], 11);
    MD4_ROUND1(b, c, d, a, x[11], 19);
    MD4_ROUND1(a, b, c, d, x[12], 3);
    MD4_ROUND1(d, a, b, c, x[13], 7);
    MD4_ROUND1(c, d, a, b, x[14], 11);
    MD4_ROUND1(b, c, d, a, x[15], 19);

    MD4_ROUND2(a, b, c, d, x[0], 3);
    MD4_ROUND2(d, a, b, c, x[4], 5);
    MD4_ROUND2(c, d, a, b, x[8], 9);
    MD4_ROUND2(b, c, d, a, x[12], 13);
    MD4_ROUND2(a, b, c, d, x[1], 3);
    MD4_ROUND2(d, a, b, c, x[5], 5);
    MD4_ROUND2(c, d, a, b, x[9], 9);
    MD4_ROUND2(b, c, d, a, x[13], 13);
    MD4_ROUND2(a, b, c, d, x[2], 3);
    MD4_ROUND2(d, a, b, c, x[6], 5);
    MD4_ROUND2(c, d, a, b, x[10], 9);
    MD4_ROUND2(b, c, d, a, x[14], 13);
    MD4_ROUND2(a, b, c, d, x[3], 3);
    MD4_ROUND2(d, a, b, c, x[7], 5);
    MD4_ROUND2(c, d, a, b, x[11], 9);
    MD4_ROUND2(b, c, d, a, x[15], 13);

    MD4_ROUND3(a, b, c, d, x[0], 3);
    MD4_ROUND3(d, a, b, c, x[8], 9);
    MD4_ROUND3(c, d, a, b, x[4], 11);
    MD4_ROUND3(b, c, d, a, x[12], 15);
    MD4_ROUND3(a, b, c, d, x[2], 3);
    MD4_ROUND3(d, a, b, c, x[10], 9);
    MD4_ROUND3(c, d, a, b, x[6], 11);
    MD4_ROUND3(b, c, d, a, x[14], 15);
    MD4_ROUND3(a, b, c, d, x[1], 3);
    MD4_ROUND3(d, a, b, c, x[9], 9);
    MD4_ROUND3(c, d, a, b, x[5], 11);
    MD4_ROUND3(b, c, d, a, x[13], 15);
    MD4_ROUND3(a, b, c, d, x[3], 3);
    MD4_ROUND3(d, a, b, c, x[11], 9);
    MD4_ROUND3(c, d, a, b, x[7], 11);
    MD4_ROUND3(b, c, d, a, x[15], 15);

    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;

    Com_Memset(x, 0, sizeof(x));

#undef MD4_ROUND3
#undef MD4_ROUND2
#undef MD4_ROUND1
#undef MD4_ROTATE_LEFT
}

static void Encode(uint8_t *output, const uint32_t *input, uint32_t length)
{
    uint32_t inputIndex;
    uint32_t outputIndex;

    inputIndex = 0;
    for (outputIndex = 0; outputIndex < length; outputIndex += 4U) {
        output[outputIndex] = (uint8_t)input[inputIndex];
        output[outputIndex + 1U] = (uint8_t)(input[inputIndex] >> 8U);
        output[outputIndex + 2U] = (uint8_t)(input[inputIndex] >> 16U);
        output[outputIndex + 3U] = (uint8_t)(input[inputIndex] >> 24U);
        inputIndex++;
    }
}

static void Decode(uint32_t *output, const uint8_t *input, uint32_t length)
{
    uint32_t inputIndex;
    uint32_t outputIndex;

    outputIndex = 0;
    for (inputIndex = 0; inputIndex < length; inputIndex += 4U) {
        output[outputIndex] = ((uint32_t)input[inputIndex]) | ((uint32_t)input[inputIndex + 1U] << 8U) |
                              ((uint32_t)input[inputIndex + 2U] << 16U) | ((uint32_t)input[inputIndex + 3U] << 24U);
        outputIndex++;
    }
}
