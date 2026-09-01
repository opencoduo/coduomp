#ifndef QCOMMON_HUFFMAN_H
#define QCOMMON_HUFFMAN_H

#include <stddef.h>
#include <stdint.h>

#include "msg.h"

enum {
    HUFFMAN_SYMBOL_COUNT = 256,
    NYT = 256,
    INTERNAL_NODE = 257,
    HUFFMAN_LOC_COUNT = NYT + 1,
    HUFFMAN_NODE_POOL_COUNT = 768,
    HUFFMAN_BITS_PER_BYTE = 8,
    HUFFMAN_BYTE_OFFSET_SHIFT = 3,
    HUFFMAN_BYTE_BIT_MASK = HUFFMAN_BITS_PER_BYTE - 1,
    HUFFMAN_INITIAL_BIT_OFFSET = 16,
    HUFFMAN_MESSAGE_SCRATCH_SIZE = 65536,
    HUFFMAN_TRANSFORM_ERROR = -1
};

typedef struct nodetype node_t;

/* Adaptive MSG Huffman tree node. Both authoritative i386 binaries use the
 * same eight 32-bit lanes and 0x20-byte stride. Linux node allocation at
 * 0x08076fb9 and Windows Huffman helpers at 0x004416e0..0x00441d3c establish
 * the child, parent, list, leader-slot, weight, and symbol fields. */
struct nodetype {
    node_t *left;
    node_t *right;
    node_t *parent;
    node_t *next;
    node_t *prev;
    node_t **head; /* shared leader slot for this weight block */
    int32_t weight;
    int32_t symbol;
};

/* Native adaptive-Huffman state. Pointer fields widen with the host; the
 * layout assertions below describe the original i386 representation. Linux
 * Huff_Init at 0x0807780c initializes two adjacent 0x701c-byte states, with
 * the node and leader-slot pools at +0x041c and +0x641c respectively. */
typedef struct huff_s {
    int32_t blocNode; /* next unused nodeList index */
    int32_t blocPtrs; /* next unused nodePtrs index */
    node_t *tree; /* adaptive-code tree root */
    node_t *lhead; /* NYT leaf and adaptive-list insertion anchor */
    node_t *ltail; /* initialized for decode states */
    node_t *loc[HUFFMAN_LOC_COUNT]; /* symbol-to-leaf lookup; 256 is NYT */
    node_t **freelist; /* reusable weight-block leader slots */
    node_t nodeList[HUFFMAN_NODE_POOL_COUNT]; /* node pool */
    node_t *nodePtrs[HUFFMAN_NODE_POOL_COUNT]; /* leader-slot pool */
} huff_t;

typedef struct huffman_s {
    huff_t compressor;
    huff_t decompressor;
} huffman_t;

#if defined(__cplusplus)
#define HUFFMAN_STATIC_ASSERT static_assert
#define HUFFMAN_ALIGNOF alignof
#else
#define HUFFMAN_STATIC_ASSERT _Static_assert
#define HUFFMAN_ALIGNOF _Alignof
#endif

#if UINTPTR_MAX == UINT32_MAX
HUFFMAN_STATIC_ASSERT(HUFFMAN_ALIGNOF(node_t) == 4,
                      "i386 Huffman node alignment changed");
HUFFMAN_STATIC_ASSERT(offsetof(node_t, left) == 0x00,
                      "i386 Huffman node left child moved");
HUFFMAN_STATIC_ASSERT(offsetof(node_t, right) == 0x04,
                      "i386 Huffman node right child moved");
HUFFMAN_STATIC_ASSERT(offsetof(node_t, parent) == 0x08,
                      "i386 Huffman node parent moved");
HUFFMAN_STATIC_ASSERT(offsetof(node_t, next) == 0x0c,
                      "i386 Huffman node next link moved");
HUFFMAN_STATIC_ASSERT(offsetof(node_t, prev) == 0x10,
                      "i386 Huffman node previous link moved");
HUFFMAN_STATIC_ASSERT(offsetof(node_t, head) == 0x14,
                      "i386 Huffman node head slot moved");
HUFFMAN_STATIC_ASSERT(offsetof(node_t, weight) == 0x18,
                      "i386 Huffman node weight moved");
HUFFMAN_STATIC_ASSERT(offsetof(node_t, symbol) == 0x1c,
                      "i386 Huffman node symbol moved");
HUFFMAN_STATIC_ASSERT(sizeof(node_t) == 0x20,
                      "i386 Huffman node size changed");
HUFFMAN_STATIC_ASSERT(HUFFMAN_ALIGNOF(huff_t) == 4,
                      "i386 Huffman state alignment changed");
HUFFMAN_STATIC_ASSERT(offsetof(huff_t, blocNode) == 0x0000,
                      "i386 Huffman node cursor moved");
HUFFMAN_STATIC_ASSERT(offsetof(huff_t, blocPtrs) == 0x0004,
                      "i386 Huffman pointer cursor moved");
HUFFMAN_STATIC_ASSERT(offsetof(huff_t, tree) == 0x0008,
                      "i386 Huffman tree root moved");
HUFFMAN_STATIC_ASSERT(offsetof(huff_t, lhead) == 0x000c,
                      "i386 Huffman list head moved");
HUFFMAN_STATIC_ASSERT(offsetof(huff_t, ltail) == 0x0010,
                      "i386 Huffman list tail moved");
HUFFMAN_STATIC_ASSERT(offsetof(huff_t, loc) == 0x0014,
                      "i386 Huffman symbol locations moved");
HUFFMAN_STATIC_ASSERT(sizeof(((huff_t *)0)->loc) == 0x0404,
                      "i386 Huffman symbol-location extent changed");
HUFFMAN_STATIC_ASSERT(offsetof(huff_t, freelist) == 0x0418,
                      "i386 Huffman free list moved");
HUFFMAN_STATIC_ASSERT(offsetof(huff_t, nodeList) == 0x041c,
                      "i386 Huffman node pool moved");
HUFFMAN_STATIC_ASSERT(sizeof(((huff_t *)0)->nodeList) == 0x6000,
                      "i386 Huffman node-pool extent changed");
HUFFMAN_STATIC_ASSERT(offsetof(huff_t, nodePtrs) == 0x641c,
                      "i386 Huffman head-slot pool moved");
HUFFMAN_STATIC_ASSERT(sizeof(((huff_t *)0)->nodePtrs) == 0x0c00,
                      "i386 Huffman head-slot pool extent changed");
HUFFMAN_STATIC_ASSERT(sizeof(huff_t) == 0x701c,
                      "i386 Huffman state size changed");
HUFFMAN_STATIC_ASSERT(HUFFMAN_ALIGNOF(huffman_t) == 4,
                      "i386 Huffman state-pair alignment changed");
HUFFMAN_STATIC_ASSERT(offsetof(huffman_t, compressor) == 0x0000,
                      "i386 Huffman compressor moved");
HUFFMAN_STATIC_ASSERT(offsetof(huffman_t, decompressor) == 0x701c,
                      "i386 Huffman decompressor moved");
HUFFMAN_STATIC_ASSERT(sizeof(huffman_t) == 0xe038,
                      "i386 Huffman state-pair size changed");
#endif

#undef HUFFMAN_ALIGNOF
#undef HUFFMAN_STATIC_ASSERT

#ifdef __cplusplus
extern "C" {
#endif

extern huffman_t msgHuff;
extern int32_t msg_hData[HUFFMAN_SYMBOL_COUNT];
extern qboolean msgInit;

void Huff_Init(huffman_t *huffman);
void Huff_addRef(huff_t *state, uint8_t symbol);
void Huff_putBit(int32_t bit, uint8_t *output, int32_t *offset);
int32_t Huff_getBit(const uint8_t *input, int32_t *offset);
int32_t Huff_Receive(node_t *node, int32_t *symbol,
                     const uint8_t *input);

void Huff_offsetReceive(node_t *node, int32_t *symbol,
                        const uint8_t *input, int32_t *offset);
void Huff_offsetTransmit(huff_t *state, int32_t symbol,
                         uint8_t *output, int32_t *offset);
void Huff_transmit(huff_t *state, int32_t symbol, uint8_t *output);

void Huff_Compress(msg_t *message, int32_t offset);

/* NOT_FROM_ORIGINAL_SOURCE: qfalse reports that decoding could not remain
 * inside the declared input extent; no result is published. */
qboolean Huff_Decompress(msg_t *message, int32_t offset);

void MSG_initHuffman(void);

int32_t MSG_WriteBitsCompress(const uint8_t *input, uint8_t *output,
                              int32_t inputLength);
/* NOT_FROM_ORIGINAL_SOURCE: the result reports input- or output-extent
 * failure; callers must discard a partial transform. */
int32_t MSG_ReadBitsCompress(const uint8_t *input, uint8_t *output,
                             int32_t inputLength, int32_t outputCapacity);

#ifdef __cplusplus
}
#endif

#endif
