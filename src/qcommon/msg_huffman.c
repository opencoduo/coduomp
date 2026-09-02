#include "huffman.h"

/*
 * The fixed state and training table are the same subsystem data in all
 * supported engine builds. The 1,024 table bytes match exactly between
 * CoDUOMP.exe 0x005c58d0..0x005c5ccf and coduo_lnxded
 * 0x080f0540..0x080f093f. The Mac client provides the exact
 * MSG_initHuffman, MSG_ReadBitsCompress, and MSG_WriteBitsCompress names.
 */
qboolean msgInit;
huffman_t msgHuff;
int32_t msg_hData[HUFFMAN_SYMBOL_COUNT] = {
    250315, 41193, 6292, 7106, 3730,  3750, 6110, 23283, 33317, 6950, 7838, 9714, 9257,  17259, 3949,  1778,  8288, 1604, 1590, 1663,
    1100,   1213,  1238, 1134, 1749,  1059, 1246, 1149,  1273,  4486, 2805, 3472, 21819, 1159,  1670,  1066,  1043, 1012, 1053, 1070,
    1726,   888,   1180, 850,  960,   780,  1752, 3296,  10630, 4514, 5881, 2685, 4650,  3837,  2093,  1867,  2584, 1949, 1972, 940,
    1134,   1788,  1670, 1206, 5719,  6128, 7222, 6654,  3710,  3795, 1492, 1524, 2215,  1140,  1355,  971,   2180, 1248, 1328, 1195,
    1770,   1078,  1264, 1266, 1168,  965,  1155, 1186,  1347,  1228, 1529, 1600, 2617,  2048,  2546,  3275,  2410, 3585, 2504, 2800,
    2675,   6146,  3663, 2840, 14253, 3164, 2221, 1687,  3208,  2739, 3512, 4796, 4091,  3515,  5288,  4016,  7937, 6031, 5360, 3924,
    4892,   3743,  4566, 4807, 5852,  6400, 6225, 8291,  23243, 7838, 7073, 8935, 5437,  4483,  3641,  5256,  5312, 5328, 5370, 3492,
    2458,   1694,  1821, 2121, 1916,  1149, 1516, 1367,  1236,  1029, 1258, 1104, 1245,  1006,  1149,  1025,  1241, 952,  1287, 997,
    1713,   1009,  1187, 879,  1099,  929,  1078, 951,   1656,  930,  1153, 1030, 1262,  1062,  1214,  1060,  1621, 930,  1106, 912,
    1034,   892,   1158, 990,  1175,  850,  1121, 903,   1087,  920,  1144, 1056, 3462,  2240,  4397,  12136, 7758, 1345, 1307, 3278,
    1950,   886,   1023, 1112, 1077,  1042, 1061, 1071,  1484,  1001, 1096, 915,  1052,  995,   1070,  876,   1111, 851,  1059, 805,
    1112,   923,   1103, 817,  1899,  1872, 976,  841,   1127,  956,  1159, 950,  7791,  954,   1289,  933,   1127, 3207, 1020, 927,
    1355,   768,   1040, 745,  952,   805,  1073, 740,   1013,  805,  1008, 796,  996,   1057,  11457, 13504};

/* CoDUOMP.exe 0x0044cf80..0x0044cfdf and coduo_lnxded
 * 0x080837eb..0x08083868 implement the same latch publication, dual-state
 * initialization, and count-controlled training of all 256 byte symbols. */
void MSG_initHuffman(void)
{
    msgInit = qtrue;
    Huff_Init(&msgHuff);

    for (int32_t symbol = 0; symbol < HUFFMAN_SYMBOL_COUNT; ++symbol) {
        for (int32_t count = 0; count < msg_hData[symbol]; ++count) {
            Huff_addRef(&msgHuff.compressor, (uint8_t)symbol);
            Huff_addRef(&msgHuff.decompressor, (uint8_t)symbol);
        }
    }
}

/* CoDUOMP.exe 0x00449780..0x004497c2 and coduo_lnxded
 * 0x0808041f..0x08080472 implement the same fixed-tree symbol loop and rounded
 * byte-count result. The compiler and calling-convention shapes differ, but
 * the behavior and preseeded state are shared. */
int32_t MSG_WriteBitsCompress(const uint8_t *input, uint8_t *output, int32_t inputLength)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    int32_t bitOffset = 0;
    for (uint32_t index = 0; index != (uint32_t)inputLength; ++index) {
        Huff_offsetTransmit(&msgHuff.compressor, input[index], output, &bitOffset);
    }
    return (bitOffset + HUFFMAN_BYTE_BIT_MASK) >> HUFFMAN_BYTE_OFFSET_SHIFT;
}

/* CoDUOMP.exe 0x004497d0..0x00449822 and coduo_lnxded
 * 0x08080473..0x080804db implement the same byte-counted decode loop.
 * outputCapacity is native compatibility plumbing for bounded decoding. */
int32_t MSG_ReadBitsCompress(const uint8_t *input, uint8_t *output, int32_t inputLength, int32_t outputCapacity)
{
    /* NOT_FROM_ORIGINAL_SOURCE: bound both transform extents and use the
     * byte-counted format's deterministic zero padding for a final partial
     * code. HUFFMAN_TRANSFORM_ERROR leaves an unpublished partial result. */
    if (inputLength < 0 || outputCapacity < 0 || inputLength > INT32_MAX / HUFFMAN_BITS_PER_BYTE) {
        return HUFFMAN_TRANSFORM_ERROR;
    }

    const int64_t inputBits = (int64_t)inputLength * HUFFMAN_BITS_PER_BYTE;
    int32_t bitOffset = 0;
    int32_t outputLength = 0;

    while (bitOffset < inputBits) {
        node_t *node = msgHuff.decompressor.tree;
        while (node != NULL && node->symbol == INTERNAL_NODE) {
            int32_t bit;
            if (bitOffset < inputBits) {
                bit = Huff_getBit(input, &bitOffset);
            } else {
                /* NOT_FROM_ORIGINAL_SOURCE: the byte-counted format has no
                 * exact final-bit count, so a final partial code uses
                 * deterministic zero padding. */
                bit = 0;
                ++bitOffset;
            }
            node = bit != 0 ? node->right : node->left;
        }
        /* NOT_FROM_ORIGINAL_SOURCE: publish a symbol only while the output
         * extent has capacity; otherwise report a partial transform. */
        if (node == NULL || outputLength >= outputCapacity)
            return HUFFMAN_TRANSFORM_ERROR;

        output[outputLength++] = (uint8_t)node->symbol;
    }
    return outputLength;
}
