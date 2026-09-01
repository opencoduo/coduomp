#include <stddef.h>
#include <stdint.h>

#include "server/standalone/bindings/coduo_engine_structs.h"

#define SEEDED_MD5_INIT_STATE_0 UINT32_C(0x67452301)
#define SEEDED_MD5_INIT_STATE_1 UINT32_C(0xefcdab89)
#define SEEDED_MD5_INIT_STATE_2 UINT32_C(0x98badcfe)
#define SEEDED_MD5_INIT_STATE_3 UINT32_C(0x10325476)
#define SEEDED_MD5_SEED_STATE_0_MULTIPLIER UINT32_C(11)
#define SEEDED_MD5_SEED_STATE_1_MULTIPLIER UINT32_C(71)
#define SEEDED_MD5_SEED_STATE_2_MULTIPLIER UINT32_C(37)
#define SEEDED_MD5_SEED_STATE_3_MULTIPLIER UINT32_C(97)
#define SEEDED_MD5_BYTE_BITS 8U
#define SEEDED_MD5_COUNT_HIGH_SHIFT 29U
#define SEEDED_MD5_BLOCK_MASK (SEEDED_MD5_BLOCK_SIZE - 1U)
#define SEEDED_MD5_FINAL_COUNT_TARGET 56U
#define SEEDED_MD5_FINAL_COUNT_TARGET_NEXT_BLOCK 120U

enum {
    SEEDED_MD5_STATE_WORD_COUNT = 4,
    SEEDED_MD5_COUNT_WORD_COUNT = 2,
    SEEDED_MD5_BLOCK_SIZE = 64,
    SEEDED_MD5_DIGEST_SIZE = 16,
    SEEDED_MD5_PADDING_SIZE = 64,
    SEEDED_MD5_CONTEXT_SIZE = 104,
    SEEDED_MD5_CONTEXT_COUNT_OFFSET = 0,
    SEEDED_MD5_CONTEXT_STATE_OFFSET = 8,
    SEEDED_MD5_CONTEXT_BUFFER_OFFSET = 24,
    SEEDED_MD5_CONTEXT_DIGEST_OFFSET = 88
};

typedef struct seeded_md5_context_s {
    uint32_t count[SEEDED_MD5_COUNT_WORD_COUNT];
    uint32_t state[SEEDED_MD5_STATE_WORD_COUNT];
    uint8_t buffer[SEEDED_MD5_BLOCK_SIZE];
    uint8_t digest[SEEDED_MD5_DIGEST_SIZE];
} seeded_md5_context_t;

_Static_assert(sizeof(seeded_md5_context_t) ==
                   SEEDED_MD5_CONTEXT_SIZE,
               "seeded MD5 context size mismatch");
_Static_assert(offsetof(seeded_md5_context_t, count) ==
                   SEEDED_MD5_CONTEXT_COUNT_OFFSET,
               "seeded MD5 count offset mismatch");
_Static_assert(offsetof(seeded_md5_context_t, state) ==
                   SEEDED_MD5_CONTEXT_STATE_OFFSET,
               "seeded MD5 state offset mismatch");
_Static_assert(offsetof(seeded_md5_context_t, buffer) ==
                   SEEDED_MD5_CONTEXT_BUFFER_OFFSET,
               "seeded MD5 buffer offset mismatch");
_Static_assert(offsetof(seeded_md5_context_t, digest) ==
                   SEEDED_MD5_CONTEXT_DIGEST_OFFSET,
               "seeded MD5 digest offset mismatch");

/* 128 sets the first padding byte's high bit. */
static uint8_t seeded_md5_padding[SEEDED_MD5_PADDING_SIZE] = {0x80};

void SeededMD5_Transform(
    uint32_t state[SEEDED_MD5_STATE_WORD_COUNT],
    const uint32_t block[SEEDED_MD5_BLOCK_SIZE / sizeof(uint32_t)])
{
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];

#define SEEDED_MD5_ROTATE_LEFT(value, bits) \
    (((value) << (bits)) | ((value) >> (32U - (bits))))
#define SEEDED_MD5_ROUND1(a_, b_, c_, d_, k_, s_, ti_) \
    do { \
        (a_) += (((b_) & (c_)) | (~(b_) & (d_))) + block[(k_)] + (ti_); \
        (a_) = SEEDED_MD5_ROTATE_LEFT((a_), (s_)) + (b_); \
    } while (0)
#define SEEDED_MD5_ROUND2(a_, b_, c_, d_, k_, s_, ti_) \
    do { \
        (a_) += (((b_) & (d_)) | ((c_) & ~(d_))) + block[(k_)] + (ti_); \
        (a_) = SEEDED_MD5_ROTATE_LEFT((a_), (s_)) + (b_); \
    } while (0)
#define SEEDED_MD5_ROUND3(a_, b_, c_, d_, k_, s_, ti_) \
    do { \
        (a_) += ((b_) ^ (c_) ^ (d_)) + block[(k_)] + (ti_); \
        (a_) = SEEDED_MD5_ROTATE_LEFT((a_), (s_)) + (b_); \
    } while (0)
#define SEEDED_MD5_ROUND4(a_, b_, c_, d_, k_, s_, ti_) \
    do { \
        (a_) += ((c_) ^ ((b_) | ~(d_))) + block[(k_)] + (ti_); \
        (a_) = SEEDED_MD5_ROTATE_LEFT((a_), (s_)) + (b_); \
    } while (0)

    SEEDED_MD5_ROUND1(a, b, c, d, 0, 7, UINT32_C(0xd76aa478));
    SEEDED_MD5_ROUND1(d, a, b, c, 1, 12, UINT32_C(0xe8c7b756));
    SEEDED_MD5_ROUND1(c, d, a, b, 2, 17, UINT32_C(0x242070db));
    SEEDED_MD5_ROUND1(b, c, d, a, 3, 22, UINT32_C(0xc1bdceee));
    SEEDED_MD5_ROUND1(a, b, c, d, 4, 7, UINT32_C(0xf57c0faf));
    SEEDED_MD5_ROUND1(d, a, b, c, 5, 12, UINT32_C(0x4787c62a));
    SEEDED_MD5_ROUND1(c, d, a, b, 6, 17, UINT32_C(0xa8304613));
    SEEDED_MD5_ROUND1(b, c, d, a, 7, 22, UINT32_C(0xfd469501));
    SEEDED_MD5_ROUND1(a, b, c, d, 8, 7, UINT32_C(0x698098d8));
    SEEDED_MD5_ROUND1(d, a, b, c, 9, 12, UINT32_C(0x8b44f7af));
    SEEDED_MD5_ROUND1(c, d, a, b, 10, 17, UINT32_C(0xffff5bb1));
    SEEDED_MD5_ROUND1(b, c, d, a, 11, 22, UINT32_C(0x895cd7be));
    SEEDED_MD5_ROUND1(a, b, c, d, 12, 7, UINT32_C(0x6b901122));
    SEEDED_MD5_ROUND1(d, a, b, c, 13, 12, UINT32_C(0xfd987193));
    SEEDED_MD5_ROUND1(c, d, a, b, 14, 17, UINT32_C(0xa679438e));
    SEEDED_MD5_ROUND1(b, c, d, a, 15, 22, UINT32_C(0x49b40821));

    SEEDED_MD5_ROUND2(a, b, c, d, 1, 5, UINT32_C(0xf61e2562));
    SEEDED_MD5_ROUND2(d, a, b, c, 6, 9, UINT32_C(0xc040b340));
    SEEDED_MD5_ROUND2(c, d, a, b, 11, 14, UINT32_C(0x265e5a51));
    SEEDED_MD5_ROUND2(b, c, d, a, 0, 20, UINT32_C(0xe9b6c7aa));
    SEEDED_MD5_ROUND2(a, b, c, d, 5, 5, UINT32_C(0xd62f105d));
    SEEDED_MD5_ROUND2(d, a, b, c, 10, 9, UINT32_C(0x02441453));
    SEEDED_MD5_ROUND2(c, d, a, b, 15, 14, UINT32_C(0xd8a1e681));
    SEEDED_MD5_ROUND2(b, c, d, a, 4, 20, UINT32_C(0xe7d3fbc8));
    SEEDED_MD5_ROUND2(a, b, c, d, 9, 5, UINT32_C(0x21e1cde6));
    SEEDED_MD5_ROUND2(d, a, b, c, 14, 9, UINT32_C(0xc33707d6));
    SEEDED_MD5_ROUND2(c, d, a, b, 3, 14, UINT32_C(0xf4d50d87));
    SEEDED_MD5_ROUND2(b, c, d, a, 8, 20, UINT32_C(0x455a14ed));
    SEEDED_MD5_ROUND2(a, b, c, d, 13, 5, UINT32_C(0xa9e3e905));
    SEEDED_MD5_ROUND2(d, a, b, c, 2, 9, UINT32_C(0xfcefa3f8));
    SEEDED_MD5_ROUND2(c, d, a, b, 7, 14, UINT32_C(0x676f02d9));
    SEEDED_MD5_ROUND2(b, c, d, a, 12, 20, UINT32_C(0x8d2a4c8a));

    SEEDED_MD5_ROUND3(a, b, c, d, 5, 4, UINT32_C(0xfffa3942));
    SEEDED_MD5_ROUND3(d, a, b, c, 8, 11, UINT32_C(0x8771f681));
    SEEDED_MD5_ROUND3(c, d, a, b, 11, 16, UINT32_C(0x6d9d6122));
    SEEDED_MD5_ROUND3(b, c, d, a, 14, 23, UINT32_C(0xfde5380c));
    SEEDED_MD5_ROUND3(a, b, c, d, 1, 4, UINT32_C(0xa4beea44));
    SEEDED_MD5_ROUND3(d, a, b, c, 4, 11, UINT32_C(0x4bdecfa9));
    SEEDED_MD5_ROUND3(c, d, a, b, 7, 16, UINT32_C(0xf6bb4b60));
    SEEDED_MD5_ROUND3(b, c, d, a, 10, 23, UINT32_C(0xbebfbc70));
    SEEDED_MD5_ROUND3(a, b, c, d, 13, 4, UINT32_C(0x289b7ec6));
    SEEDED_MD5_ROUND3(d, a, b, c, 0, 11, UINT32_C(0xeaa127fa));
    SEEDED_MD5_ROUND3(c, d, a, b, 3, 16, UINT32_C(0xd4ef3085));
    SEEDED_MD5_ROUND3(b, c, d, a, 6, 23, UINT32_C(0x04881d05));
    SEEDED_MD5_ROUND3(a, b, c, d, 9, 4, UINT32_C(0xd9d4d039));
    SEEDED_MD5_ROUND3(d, a, b, c, 12, 11, UINT32_C(0xe6db99e5));
    SEEDED_MD5_ROUND3(c, d, a, b, 15, 16, UINT32_C(0x1fa27cf8));
    SEEDED_MD5_ROUND3(b, c, d, a, 2, 23, UINT32_C(0xc4ac5665));

    SEEDED_MD5_ROUND4(a, b, c, d, 0, 6, UINT32_C(0xf4292244));
    SEEDED_MD5_ROUND4(d, a, b, c, 7, 10, UINT32_C(0x432aff97));
    SEEDED_MD5_ROUND4(c, d, a, b, 14, 15, UINT32_C(0xab9423a7));
    SEEDED_MD5_ROUND4(b, c, d, a, 5, 21, UINT32_C(0xfc93a039));
    SEEDED_MD5_ROUND4(a, b, c, d, 12, 6, UINT32_C(0x655b59c3));
    SEEDED_MD5_ROUND4(d, a, b, c, 3, 10, UINT32_C(0x8f0ccc92));
    SEEDED_MD5_ROUND4(c, d, a, b, 10, 15, UINT32_C(0xffeff47d));
    SEEDED_MD5_ROUND4(b, c, d, a, 1, 21, UINT32_C(0x85845dd1));
    SEEDED_MD5_ROUND4(a, b, c, d, 8, 6, UINT32_C(0x6fa87e4f));
    SEEDED_MD5_ROUND4(d, a, b, c, 15, 10, UINT32_C(0xfe2ce6e0));
    SEEDED_MD5_ROUND4(c, d, a, b, 6, 15, UINT32_C(0xa3014314));
    SEEDED_MD5_ROUND4(b, c, d, a, 13, 21, UINT32_C(0x4e0811a1));
    SEEDED_MD5_ROUND4(a, b, c, d, 4, 6, UINT32_C(0xf7537e82));
    SEEDED_MD5_ROUND4(d, a, b, c, 11, 10, UINT32_C(0xbd3af235));
    SEEDED_MD5_ROUND4(c, d, a, b, 2, 15, UINT32_C(0x2ad7d2bb));
    SEEDED_MD5_ROUND4(b, c, d, a, 9, 21, UINT32_C(0xeb86d391));

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

#undef SEEDED_MD5_ROUND4
#undef SEEDED_MD5_ROUND3
#undef SEEDED_MD5_ROUND2
#undef SEEDED_MD5_ROUND1
#undef SEEDED_MD5_ROTATE_LEFT
}

void SeededMD5_Init(seeded_md5_context_t *context, int32_t seed)
{
    context->count[1] = 0;
    context->count[0] = 0;
    context->state[0] =
        (uint32_t)seed * SEEDED_MD5_SEED_STATE_0_MULTIPLIER +
        SEEDED_MD5_INIT_STATE_0;
    context->state[1] =
        (uint32_t)seed * SEEDED_MD5_SEED_STATE_1_MULTIPLIER +
        SEEDED_MD5_INIT_STATE_1;
    context->state[2] =
        (uint32_t)seed * SEEDED_MD5_SEED_STATE_2_MULTIPLIER +
        SEEDED_MD5_INIT_STATE_2;
    context->state[3] =
        (uint32_t)seed * SEEDED_MD5_SEED_STATE_3_MULTIPLIER +
        SEEDED_MD5_INIT_STATE_3;
}

void SeededMD5_Update(seeded_md5_context_t *context,
                      const uint8_t *input, uint32_t length)
{
    uint32_t bufferIndex =
        (context->count[0] >> 3U) & SEEDED_MD5_BLOCK_MASK;
    uint32_t lengthBits = length * SEEDED_MD5_BYTE_BITS;

    context->count[0] += lengthBits;
    if (context->count[0] < lengthBits) {
        context->count[1]++;
    }
    context->count[1] += length >> SEEDED_MD5_COUNT_HIGH_SHIFT;

    while (length != 0) {
        context->buffer[bufferIndex] = *input;
        input++;
        length--;
        bufferIndex++;

        if (bufferIndex == SEEDED_MD5_BLOCK_SIZE) {
            uint32_t block[SEEDED_MD5_BLOCK_SIZE / sizeof(uint32_t)];

            for (uint32_t index = 0; index < SEEDED_MD5_BLOCK_SIZE;
                 index += sizeof(uint32_t)) {
                block[index / sizeof(uint32_t)] =
                    ((uint32_t)context->buffer[index]) |
                    ((uint32_t)context->buffer[index + 1U] << 8U) |
                    ((uint32_t)context->buffer[index + 2U] << 16U) |
                    ((uint32_t)context->buffer[index + 3U] << 24U);
            }

            SeededMD5_Transform(context->state, block);
            bufferIndex = 0;
        }
    }
}

void SeededMD5_Final(seeded_md5_context_t *context)
{
    uint32_t finalCount[SEEDED_MD5_COUNT_WORD_COUNT];
    uint32_t block[SEEDED_MD5_BLOCK_SIZE / sizeof(uint32_t)];
    uint32_t bufferIndex =
        (context->count[0] >> 3U) & SEEDED_MD5_BLOCK_MASK;
    uint32_t padLength;

    finalCount[0] = context->count[0];
    finalCount[1] = context->count[1];

    if (bufferIndex < SEEDED_MD5_FINAL_COUNT_TARGET) {
        padLength = SEEDED_MD5_FINAL_COUNT_TARGET - bufferIndex;
    } else {
        padLength = SEEDED_MD5_FINAL_COUNT_TARGET_NEXT_BLOCK - bufferIndex;
    }

    SeededMD5_Update(context, seeded_md5_padding, padLength);

    for (uint32_t index = 0; index < SEEDED_MD5_FINAL_COUNT_TARGET;
         index += sizeof(uint32_t)) {
        block[index / sizeof(uint32_t)] =
            ((uint32_t)context->buffer[index]) |
            ((uint32_t)context->buffer[index + 1U] << 8U) |
            ((uint32_t)context->buffer[index + 2U] << 16U) |
            ((uint32_t)context->buffer[index + 3U] << 24U);
    }
    block[14] = finalCount[0];
    block[15] = finalCount[1];

    SeededMD5_Transform(context->state, block);

    for (uint32_t index = 0; index < SEEDED_MD5_STATE_WORD_COUNT; ++index) {
        context->digest[index * sizeof(uint32_t)] =
            (uint8_t)context->state[index];
        context->digest[index * sizeof(uint32_t) + 1U] =
            (uint8_t)(context->state[index] >> 8U);
        context->digest[index * sizeof(uint32_t) + 2U] =
            (uint8_t)(context->state[index] >> 16U);
        context->digest[index * sizeof(uint32_t) + 3U] =
            (uint8_t)(context->state[index] >> 24U);
    }
}
