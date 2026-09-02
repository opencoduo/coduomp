// Source: uo_cgame_mp_x86.dll 0x30044b90..0x30044c0d
//
// CG_RegisterItems — register the visuals for every item present on the current
// map, from the CS_ITEMS config string's item-present bitfield.
//
// Function identity is established by the config-string lookup, the fixed item
// domain, and the call to CG_RegisterItemVisuals for each present item.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

#include <limits.h>

// The source representation used a 260-byte local. The maintained representation
// is defined by the complete item-bitfield domain consumed below.
enum {
    CG_RETAIL_ITEMS_CONFIGSTRING_BUFSIZE = 260
};

// Highest item index the loop registers; the machine code compares i against the
// fixed literal 0x86 (134), so i runs 1..133 inclusive. This is the map's item
// count baked into the compare (Q3/CoD bg_numItems). Exact source symbol for the
// bound is unproven; named by its proven role.
enum {
    CG_REGISTER_ITEMS_COUNT = 134,
    CG_ITEMS_CONFIGSTRING_NIBBLE_COUNT = (CG_REGISTER_ITEMS_COUNT + 3) / 4
};

_Static_assert(CG_REGISTER_ITEMS_COUNT <= INT_MAX - 3, "item count must permit ceiling division by four");

void CG_RegisterItems(void)
{
    char items[CG_ITEMS_CONFIGSTRING_NIBBLE_COUNT] = {0};
    int i;

    // 0x30044ba2..0x30044ba7: inlined CG_ConfigString(CS_ITEMS). The config
    // string is a hex-packed item-present bitfield.
    const char *itemBits = &cg_gameState.stringData[cg_gameState.stringOffsets[CS_ITEMS]];
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    for (int n = 0; n < CG_ITEMS_CONFIGSTRING_NIBBLE_COUNT && itemBits[n] != '\0'; ++n) {
        items[n] = itemBits[n];
    }

    // 0x30044bc2..0x30044bf7: for each item index i in 1..133, extract its 1-bit
    // presence flag from the bitfield (4 items per hex char, low bit first) and
    // register the item's visuals when present.
    for (i = 1; i < CG_REGISTER_ITEMS_COUNT; i++) {
        // items[i >> 2] is a hex character; decode it to its 0..15 nibble value.
        // MOVSX of a signed char: '0'..'9' -> value - '0', 'a'..'f' -> value - 'a'
        // + 10 (i.e. - ('a' - 10) == - 0x57). The compare is signed (JG) against
        // '9', so any char above '9' takes the second branch.
        int hexChar = (signed char)items[i >> 2];
        int nibble;
        if (hexChar > '9') {
            nibble = hexChar - ('a' - 10);
        } else {
            nibble = hexChar - '0';
        }

        // Bit (i & 3) of that nibble is item i's presence flag.
        if (nibble & (1 << (i & 3))) {
            CG_RegisterItemVisuals(i);
        }
    }
}
