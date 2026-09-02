#include "huffman.h"
#include "q_memory.h"

/*
 * The complete subsystem below has the same operation graph in CoDUOMP.exe
 * 0x004415c0..0x004421c7 and coduo_lnxded
 * 0x08076b48..0x080779a3. The symbolized Mac client independently supplies
 * the exact public and file-local names at code offsets 0x4aea0..0x4bd0b.
 * Compiler-shaped control flow differs, but no platform behavior switch is
 * present in the adaptive tree, bit cursor, or message transforms.
 */

/* Original Windows global bit cursor at 0x0389fd5c. The offset-taking entry
 * points copy through this cursor because the tree walkers share
 * add_bit/get_bit. */
static int32_t bloc;

/* Source: CoDUOMP.exe 0x00441630..0x00441660.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00441630_00441661.mcode.
 * Name: exact same-module Mac symbol add_bit. */
static void add_bit(int8_t bit, uint8_t *output)
{
    if ((bloc & HUFFMAN_BYTE_BIT_MASK) == 0)
        output[bloc >> HUFFMAN_BYTE_OFFSET_SHIFT] = 0;
    output[bloc >> HUFFMAN_BYTE_OFFSET_SHIFT] |=
        (uint8_t)(bit << (bloc & HUFFMAN_BYTE_BIT_MASK));
    ++bloc;
}

/* Source: CoDUOMP.exe 0x00441670..0x00441692.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00441670_00441693.mcode.
 * Name: exact same-module Mac symbol get_bit. */
static int32_t get_bit(const uint8_t *input)
{
    const int32_t bit =
        (input[bloc >> HUFFMAN_BYTE_OFFSET_SHIFT] >>
         (bloc & HUFFMAN_BYTE_BIT_MASK)) & 1;
    ++bloc;
    return bit;
}

/* Source: CoDUOMP.exe 0x004415c0..0x004415f3.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004415c0_004415f4.mcode.
 * Name inferred from the canonical Huffman API and proved by its offset
 * copy-in/copy-out wrapper around the add_bit operation. */
void Huff_putBit(int32_t bit, uint8_t *output, int32_t *offset)
{
    bloc = *offset;
    add_bit((int8_t)bit, output);
    *offset = bloc;
}

/* Source: CoDUOMP.exe 0x00441600..0x00441620.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00441600_00441621.mcode.
 * Name inferred from the canonical Huffman API and proved by its offset
 * copy-in/copy-out wrapper around the get_bit operation. */
int32_t Huff_getBit(const uint8_t *input, int32_t *offset)
{
    bloc = *offset;
    const int32_t bit = get_bit(input);
    *offset = bloc;
    return bit;
}

/* Source: CoDUOMP.exe 0x004416a0..0x004416c1.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004416a0_004416c2.mcode.
 * Name: exact same-module Mac symbol get_ppnode. */
static node_t **get_ppnode(huff_t *state)
{
    node_t **head;

    if (state->freelist == NULL)
        return &state->nodePtrs[state->blocPtrs++];

    head = state->freelist;
    state->freelist = (node_t **)*head;
    return head;
}

/* Source: CoDUOMP.exe 0x004416d0..0x004416de.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004416d0_004416df.mcode.
 * Name: exact same-module Mac symbol free_ppnode. */
static void free_ppnode(huff_t *state, node_t **head)
{
    *head = (node_t *)state->freelist;
    state->freelist = head;
}

/* Source: CoDUOMP.exe 0x004416e0..0x00441724.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004416e0_00441725.mcode.
 * Static helper role is proved by increment: exchange two tree nodes
 * without changing their linked-list positions. */
static void swap(huff_t *state, node_t *first,
                 node_t *second)
{
    node_t *firstParent = first->parent;
    node_t *secondParent = second->parent;

    if (firstParent != NULL) {
        if (firstParent->left == first)
            firstParent->left = second;
        else
            firstParent->right = second;
    } else {
        state->tree = second;
    }

    if (secondParent != NULL) {
        if (secondParent->left == second)
            secondParent->left = first;
        else
            secondParent->right = first;
    } else {
        state->tree = first;
    }

    first->parent = secondParent;
    second->parent = firstParent;
}

/* Source: CoDUOMP.exe 0x00441730..0x00441782.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00441730_00441783.mcode.
 * Static helper role is proved by increment: exchange two nodes in the
 * adaptive tree's weight-ordered doubly linked list. */
static void swaplist(node_t *first, node_t *second)
{
    node_t *temporary;

    temporary = first->next;
    first->next = second->next;
    second->next = temporary;

    temporary = first->prev;
    first->prev = second->prev;
    second->prev = temporary;

    if (first->next == first)
        first->next = second;
    if (second->next == second)
        second->next = first;

    if (first->next != NULL)
        first->next->prev = first;
    if (second->next != NULL)
        second->next->prev = second;
    if (first->prev != NULL)
        first->prev->next = first;
    if (second->prev != NULL)
        second->prev->next = second;
}

/* Source: CoDUOMP.exe 0x00441790..0x0044186d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00441790_0044186e.mcode.
 * Static helper role and recursion are proved by Huff_addRef and the direct
 * self-call at 0x00441846. The pointer-slot free list deliberately stores its
 * next link through a node-pointer slot, matching the original weak typing. */
static void increment(huff_t *state, node_t *node)
{
    node_t *leader;

    if (node == NULL)
        return;

    if (node->next != NULL && node->next->weight == node->weight) {
        leader = *node->head;
        if (leader != node->parent)
            swap(state, leader, node);
        swaplist(leader, node);
    }

    if (node->prev != NULL && node->prev->weight == node->weight) {
        *node->head = node->prev;
    } else {
        *node->head = NULL;
        free_ppnode(state, node->head);
    }

    ++node->weight;

    if (node->next != NULL && node->next->weight == node->weight) {
        node->head = node->next->head;
    } else {
        node->head = get_ppnode(state);
        *node->head = node;
    }

    if (node->parent != NULL) {
        increment(state, node->parent);
        if (node->prev == node->parent) {
            swaplist(node, node->parent);
            if (*node->head == node)
                *node->head = node->parent;
        }
    }
}

/* Source: CoDUOMP.exe 0x00441870..0x00441a02.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00441870_00441a03.mcode.
 * Name and byte-symbol argument: exact same-module Mac symbol Huff_addRef.
 * Every node/list mutation below maps to the PE offsets in node_t and
 * huff_t; the seemingly asymmetric head-slot assignments are
 * retained exactly. */
void Huff_addRef(huff_t *state, uint8_t symbol)
{
    node_t *internalNode;
    node_t *leafNode;

    if (state->loc[symbol] != NULL) {
        increment(state, state->loc[symbol]);
        return;
    }

    leafNode = &state->nodeList[state->blocNode++];
    internalNode = &state->nodeList[state->blocNode++];

    internalNode->symbol = INTERNAL_NODE;
    internalNode->weight = 1;
    internalNode->next = state->lhead->next;
    if (state->lhead->next != NULL) {
        state->lhead->next->prev = internalNode;
        if (state->lhead->next->weight == 1) {
            internalNode->head = state->lhead->next->head;
        } else {
            internalNode->head = get_ppnode(state);
            *internalNode->head = internalNode;
        }
    } else {
        internalNode->head = get_ppnode(state);
        *internalNode->head = internalNode;
    }

    state->lhead->next = internalNode;
    internalNode->prev = state->lhead;

    leafNode->symbol = symbol;
    leafNode->weight = 1;
    leafNode->next = state->lhead->next;
    if (state->lhead->next != NULL) {
        state->lhead->next->prev = leafNode;
        if (state->lhead->next->weight == 1) {
            leafNode->head = state->lhead->next->head;
        } else {
            leafNode->head = get_ppnode(state);
            *leafNode->head = internalNode;
        }
    } else {
        leafNode->head = get_ppnode(state);
        *leafNode->head = leafNode;
    }

    state->lhead->next = leafNode;
    leafNode->prev = state->lhead;
    leafNode->left = NULL;
    leafNode->right = NULL;

    if (state->lhead->parent != NULL) {
        if (state->lhead->parent->left == state->lhead)
            state->lhead->parent->left = internalNode;
        else
            state->lhead->parent->right = internalNode;
    } else {
        state->tree = internalNode;
    }

    internalNode->right = leafNode;
    internalNode->left = state->lhead;
    internalNode->parent = state->lhead->parent;
    leafNode->parent = internalNode;
    state->lhead->parent = internalNode;
    state->loc[symbol] = leafNode;

    increment(state, internalNode->parent);
}

/* Source: CoDUOMP.exe 0x00441a10..0x00441a68.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00441a10_00441a69.mcode.
 * Name and arguments: exact same-module Mac symbol Huff_Receive. A null path
 * returns zero without modifying the caller's output symbol. */
int32_t Huff_Receive(node_t *node, int32_t *symbol,
                     const uint8_t *input)
{
    while (node != NULL && node->symbol == INTERNAL_NODE) {
        node = get_bit(input) != 0 ? node->right : node->left;
    }
    if (node == NULL)
        return 0;

    *symbol = node->symbol;
    return node->symbol;
}

/* Source: CoDUOMP.exe 0x00441a70..0x00441ad2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00441a70_00441ad3.mcode.
 * Name and arguments: exact same-module Mac symbol Huff_offsetReceive. If
 * traversal fails, the output symbol is cleared but the caller's offset is
 * deliberately not advanced. */
void Huff_offsetReceive(node_t *node, int32_t *symbol,
                        const uint8_t *input, int32_t *offset)
{
    bloc = *offset;
    while (node != NULL && node->symbol == INTERNAL_NODE) {
        node = get_bit(input) != 0 ? node->right : node->left;
    }
    if (node == NULL) {
        *symbol = 0;
        return;
    }

    *symbol = node->symbol;
    *offset = bloc;
}

/* Source: CoDUOMP.exe 0x00441ae0..0x00441b63.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00441ae0_00441b64.mcode.
 * Name: exact same-module Mac symbol huffman_send. Recursing to the root before
 * emitting the edge bit produces the symbol's root-to-leaf code. */
static void huffman_send(node_t *node, node_t *child,
                         uint8_t *output)
{
    if (node->parent != NULL)
        huffman_send(node->parent, node, output);
    if (child != NULL)
        add_bit(node->right == child ? 1 : 0, output);
}

/* Source: CoDUOMP.exe 0x00441b70..0x00441d0d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00441b70_00441d0e.mcode.
 * Name and arguments: exact same-module Mac symbol Huff_transmit. Unknown
 * symbols transmit the NYT code followed by the literal byte, most-significant
 * bit first. */
void Huff_transmit(huff_t *state, int32_t symbol, uint8_t *output)
{
    if (state->loc[symbol] == NULL) {
        Huff_transmit(state, NYT, output);
        for (int32_t bit = HUFFMAN_BITS_PER_BYTE - 1; bit >= 0; --bit)
            add_bit((int8_t)((symbol >> bit) & 1), output);
        return;
    }

    node_t *node = state->loc[symbol];
    if (node->parent != NULL)
        huffman_send(node->parent, node, output);
}

/* Source: CoDUOMP.exe 0x00441d10..0x00441d3c.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00441d10_00441d3d.mcode.
 * Name and arguments: exact same-module Mac symbol Huff_offsetTransmit. */
void Huff_offsetTransmit(huff_t *state, int32_t symbol,
                         uint8_t *output, int32_t *offset)
{
    node_t *node = state->loc[symbol];
    bloc = *offset;
    if (node->parent != NULL)
        huffman_send(node->parent, node, output);
    *offset = bloc;
}

/* Source: CoDUOMP.exe 0x00441d40..0x00441f9c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00441d40_00441f9d.mcode.
 * Name and arguments: exact same-module Mac symbol Huff_Decompress. The
 * qboolean result is native compatibility plumbing for decode failure. */
qboolean Huff_Decompress(msg_t *message, int32_t offset)
{
    huff_t state;
    uint8_t output[HUFFMAN_MESSAGE_SCRATCH_SIZE];

    /* NOT_FROM_ORIGINAL_SOURCE: validate the complete message window before
     * deriving either pointer or extent. A qfalse result publishes no decoded
     * bytes, so the caller must discard the packet. */
    if (message == NULL || message->data == NULL || offset < 0 ||
        message->cursize > message->maxsize ||
        offset > message->cursize || offset > message->maxsize) {
        return qfalse;
    }

    uint8_t *input = message->data + offset;
    const int32_t compressedLength = message->cursize - offset;
    if (compressedLength <= 0)
        return qtrue;

    /* NOT_FROM_ORIGINAL_SOURCE: the encoded output-length header occupies two
     * complete bytes. */
    if (compressedLength < 2)
        return qfalse;

    Com_Memset(&state, 0, sizeof(state));
    state.tree = &state.nodeList[state.blocNode++];
    state.lhead = state.tree;
    state.ltail = state.tree;
    state.loc[NYT] = state.tree;
    state.tree->symbol = NYT;
    state.tree->weight = 0;
    state.tree->prev = NULL;
    state.tree->next = NULL;
    state.tree->right = NULL;
    state.tree->left = NULL;
    state.tree->parent = NULL;

    int32_t outputLength =
        ((int32_t)input[0] << HUFFMAN_BITS_PER_BYTE) + input[1];
    if (outputLength > message->maxsize - offset)
        outputLength = message->maxsize - offset;

    const int64_t inputBitCount =
        (int64_t)compressedLength * HUFFMAN_BITS_PER_BYTE;
    int32_t bitOffset = HUFFMAN_INITIAL_BIT_OFFSET;
    for (int32_t index = 0; index < outputLength; ++index) {
        node_t *node = state.tree;
        while (node != NULL && node->symbol == INTERNAL_NODE) {
            /* NOT_FROM_ORIGINAL_SOURCE: every tree-walk bit must remain inside
             * the declared input extent. */
            if ((int64_t)bitOffset >= inputBitCount)
                return qfalse;
            node = Huff_getBit(input, &bitOffset) != 0
                ? node->right
                : node->left;
        }
        if (node == NULL)
            return qfalse;

        int32_t symbol = node->symbol;
        if (symbol == NYT) {
            /* NOT_FROM_ORIGINAL_SOURCE: an NYT symbol requires a complete
             * eight-bit literal inside the declared input extent. */
            if (inputBitCount - bitOffset < HUFFMAN_BITS_PER_BYTE)
                return qfalse;
            symbol = 0;
            for (int32_t bit = HUFFMAN_BITS_PER_BYTE - 1; bit >= 0; --bit)
                symbol = symbol * 2 + Huff_getBit(input, &bitOffset);
        }

        output[index] = (uint8_t)symbol;
        Huff_addRef(&state, (uint8_t)symbol);
    }

    bloc = bitOffset;
    message->cursize = offset + outputLength;
    Com_Memcpy(message->data + offset, output,
               (size_t)(uint32_t)outputLength);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00441fa0..0x004420ea.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00441fa0_004420eb.mcode.
 * Name and arguments: exact same-module Mac symbol Huff_Compress. */
void Huff_Compress(msg_t *message, int32_t offset)
{
    huff_t state;
    uint8_t output[HUFFMAN_MESSAGE_SCRATCH_SIZE];
    uint8_t *input = message->data + offset;
    const int32_t inputLength = message->cursize - offset;

    if (inputLength <= 0)
        return;

    Com_Memset(&state, 0, sizeof(state));
    state.tree = &state.nodeList[state.blocNode++];
    state.lhead = state.tree;
    state.loc[NYT] = state.tree;
    state.tree->symbol = NYT;
    state.tree->weight = 0;
    state.tree->prev = NULL;
    state.tree->next = NULL;
    state.tree->right = NULL;
    state.tree->left = NULL;
    state.tree->parent = NULL;

    output[0] = (uint8_t)(inputLength >> HUFFMAN_BITS_PER_BYTE);
    output[1] = (uint8_t)inputLength;
    bloc = HUFFMAN_INITIAL_BIT_OFFSET;

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    for (int32_t index = 0; index < inputLength; ++index) {
        const uint8_t symbol = input[index];
        Huff_transmit(&state, symbol, output);
        Huff_addRef(&state, symbol);
    }

    bloc += HUFFMAN_BITS_PER_BYTE;
    const int32_t outputLength = bloc >> HUFFMAN_BYTE_OFFSET_SHIFT;
    message->cursize = offset + outputLength;
    Com_Memcpy(message->data + offset, output,
               (size_t)(uint32_t)outputLength);
}

/* Source: CoDUOMP.exe 0x004420f0..0x004421c7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004420f0_004421c8.mcode.
 * Name and aggregate argument: exact same-module Mac symbol Huff_Init. The
 * asymmetric initial tails are original: the decompressor initializes ltail,
 * while the zeroed compressor leaves ltail null. */
void Huff_Init(huffman_t *huffman)
{
    huff_t *compressor = &huffman->compressor;
    huff_t *decompressor = &huffman->decompressor;
    node_t *node;

    Com_Memset(compressor, 0, sizeof(*compressor));
    Com_Memset(decompressor, 0, sizeof(*decompressor));

    node = &decompressor->nodeList[decompressor->blocNode++];
    decompressor->tree = node;
    decompressor->lhead = node;
    decompressor->ltail = node;
    decompressor->loc[NYT] = node;
    node->symbol = NYT;
    node->weight = 0;
    node->prev = NULL;
    node->next = NULL;
    node->right = NULL;
    node->left = NULL;
    node->parent = NULL;

    node = &compressor->nodeList[compressor->blocNode++];
    compressor->tree = node;
    compressor->lhead = node;
    node->symbol = NYT;
    node->weight = 0;
    node->prev = NULL;
    node->next = NULL;
    node->right = NULL;
    node->left = NULL;
    node->parent = NULL;
    compressor->loc[NYT] = node;
}
