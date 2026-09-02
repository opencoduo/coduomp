#include "q_math.h"

#include <string.h>

enum {
    Q_RAND_MULTIPLIER = 69069,
    Q_RAND_MASK = 65535
};

/*
 * The authoritative Windows Q_rand bodies are instruction-identical at:
 *
 *   CoDUOMP.exe                 0x004310a0
 *   uo_cgame_mp_x86.dll        0x30049200
 *   uo_ui_mp_x86.dll           0x400011d0
 *   uo_game_mp_x86.dll         0x20016250
 *
 * Q_random and Q_crandom are likewise instruction-identical within the
 * Windows family, apart from image-local constant addresses, at
 * 0x004310b0/0x004310e0, 0x30049210/0x30049240,
 * 0x400011e0/0x40001210, and 0x20016260/0x20016290.
 *
 * The Linux engine bodies at 0x08065fd0, 0x08065feb, and 0x08066013 have the
 * same operation graphs as game.mp.uo.i386.so RVAs 0x00039960, 0x0003997b,
 * and 0x000399b3. Windows spills the masked integer through binary32 before
 * scaling while Linux keeps it in x87. That difference is inert: every
 * integer in [0, 65535], division by 2^16, subtraction of 1/2, and doubling
 * are exact in binary32. The canonical float interface therefore preserves
 * every possible result on all authoritative targets without a behavior gate.
 */
int32_t Q_rand(int32_t *seed)
{
    const uint32_t next =
        (uint32_t)*seed * (uint32_t)Q_RAND_MULTIPLIER + UINT32_C(1);
    int32_t result;

    /* The LCG state is a complete modulo-2^32 dword. Preserve its bits without
     * relying on signed-overflow or out-of-range unsigned-to-signed rules. */
    memcpy(seed, &next, sizeof(next));
    memcpy(&result, &next, sizeof(result));
    return result;
}

float Q_random(int32_t *seed)
{
    const int32_t sample = Q_rand(seed) & Q_RAND_MASK;

    return (float)sample / 65536.0f;
}

float Q_crandom(int32_t *seed)
{
    return (Q_random(seed) - 0.5f) * 2.0f;
}
