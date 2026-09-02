// Source: uo_cgame_mp_x86.dll 0x30008190..0x300081d1
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30008190_300081d1.mcode
//
// Rider/seat attach-tag-name accessor: index -> tag string.
//
// The function reserves a 7-entry pointer array on the stack, fills it with the
// addresses of seven .rdata tag-name strings, then returns table[index] where
// `index` arrives in EAX (fastcall-style register argument in this binary):
//
//     30008190  SUB ESP,0x1c                       ; room for exactly 7 dwords
//     30008193  MOV [ESP+0x00],0x30072d74          ; table[0] = "*unused*"
//     3000819a  MOV [ESP+0x04],0x30072d68          ; table[1] = "tag_player"
//     300081a2  MOV [ESP+0x08],0x30072d50          ; table[2] = "tag_secondary_player"
//     300081aa  MOV [ESP+0x0c],0x30072d40          ; table[3] = "tag_passenger"
//     300081b2  MOV [ESP+0x10],0x30072d30          ; table[4] = "tag_passenger2"
//     300081ba  MOV [ESP+0x14],0x30072d20          ; table[5] = "tag_passenger3"
//     300081c2  MOV [ESP+0x18],0x30072d10          ; table[6] = "tag_passenger4"
//     300081ca  MOV EAX,[ESP+EAX*4]                ; EAX = table[index]
//     300081cd  ADD ESP,0x1c
//     300081d0  RET                                ; caller-cleanup none (index in EAX)
//
// The network selector is wider than the recovered seven-entry tag table.
// Reject values outside that table before resolving the tag.
//
// NAMING: the mechanical `.mcode` name `GScr_GetNumParts` is a pure size-match
// (win 0x41 == matched 0x41) and is REJECTED — GScr_* is a server script builtin,
// and this 65-byte cgame routine is a string-table accessor, not a script command.
// Named by behavior from the resolved .rdata strings (tag_player /
// tag_secondary_player / tag_passenger[2..4] / *unused*). The exact original symbol
// is unproven; `CG_GetRiderTagName` describes the proven role (map a rider/seat
// slot index to its model-attach tag name).

#include "client/cgame/globals.h"

/*
 * Rider/seat attach-tag-name table, indexed by seat slot.
 *
 *   0 -> "*unused*"              (empty / no rider)
 *   1 -> "tag_player"           (driver / primary)
 *   2 -> "tag_secondary_player"
 *   3 -> "tag_passenger"
 *   4 -> "tag_passenger2"
 *   5 -> "tag_passenger3"
 *   6 -> "tag_passenger4"
 */
enum {
    RIDER_TAG_UNUSED = 0,
    RIDER_TAG_PLAYER = 1,
    RIDER_TAG_SECONDARY_PLAYER = 2,
    RIDER_TAG_PASSENGER = 3,
    RIDER_TAG_PASSENGER2 = 4,
    RIDER_TAG_PASSENGER3 = 5,
    RIDER_TAG_PASSENGER4 = 6,
    RIDER_TAG_COUNT = 7
};

const char *CG_GetRiderTagName(int index)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if (index < 0 || index >= RIDER_TAG_COUNT) {
        return bg_unusedBoneName;
    }

    /*
     * The table is materialized on the stack by the machine code (seven MOV
     * immediates into [ESP+0..0x18]) and then indexed. Expressed as a local
     * array of the recovered .rdata string globals; the indexed load
     * `MOV EAX,[ESP+EAX*4]` is the array subscript.
     */
    const char *tagNames[RIDER_TAG_COUNT] = {
        bg_unusedBoneName,     /* 0 */
        bg_playerTagName,         /* 1 */
        bg_secondaryPlayerTagName,/* 2 */
        bg_passengerTagName,      /* 3 */
        bg_passenger2TagName,     /* 4 */
        bg_passenger3TagName,     /* 5 */
        bg_passenger4TagName      /* 6 */
    };

    return tagNames[index];
}
