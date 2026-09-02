#include "q_bits.h"

#include "compat/coduo_int32_bits.h"

#include <stdint.h>
#include <string.h>

enum {
    COM_BIT_WORD_SHIFT = 5,
    COM_BIT_INDEX_MASK = 31
};

/*
 * The four Windows copies are instruction-identical within each helper:
 *
 *                  EXE        cgame      UI         game
 * Com_BitCheck     0044f420   3004e210   40006240   20057a30
 * Com_BitSet       0044f440   3004e230   40006260   20057a50
 * Com_BitClear     0044f460   3004e250   40006280   20057a70
 *
 * Linux uses the same signed word index and low-five-bit mask at engine
 * addresses 0x080863f9/0x0808641c/0x0808645a and game RVAs
 * 0x00092e4f/0x00092e72/0x00092eb6.  Com_BitCheck selects the bit by shifting
 * the word on Linux and by masking then booleanizing on Windows; both return
 * exactly zero or one for every 32-bit word.  Supporting Mac cgame/game
 * traceback symbols retain these exact names.
 */

int32_t Com_BitCheck(const uint32_t *bits, int32_t bit)
{
    const uint32_t mask = UINT32_C(1) << ((uint32_t)bit & COM_BIT_INDEX_MASK);
    const int32_t word = coduo_int32_sar((uint32_t)bit, COM_BIT_WORD_SHIFT);

    return (bits[word] & mask) != 0;
}

void Com_BitSet(uint32_t *bits, int32_t bit)
{
    const int32_t word = coduo_int32_sar((uint32_t)bit, COM_BIT_WORD_SHIFT);

    bits[word] |= UINT32_C(1) << ((uint32_t)bit & COM_BIT_INDEX_MASK);
}

void Com_BitClear(uint32_t *bits, int32_t bit)
{
    const int32_t word = coduo_int32_sar((uint32_t)bit, COM_BIT_WORD_SHIFT);

    bits[word] &= ~(UINT32_C(1) << ((uint32_t)bit & COM_BIT_INDEX_MASK));
}

/* The original helper returns a binary32 payload as an integer unchanged:
 *
 *   CoDUOMP.exe copies  0x00401c90, 0x0041b6b0, 0x0045ce80
 *   coduo_lnxded        0x0808e952
 *
 * The supporting Mac client exports the canonical FloatAsInt name. memcpy
 * expresses the identical bit transfer without strict-aliasing undefined
 * behavior. */
int32_t FloatAsInt(float value)
{
    int32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}
