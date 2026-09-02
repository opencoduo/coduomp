// Source: uo_cgame_mp_x86.dll 0x3005a0f0..0x3005a194;
//         uo_ui_mp_x86.dll    0x4001bc60..0x4001bd04 (exact after rebasing).
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3005a0f0_3005a194.mcode
//
// ParseColorRange — the shared body behind the menu-item `addColorRange` and
// `addColorRangeRel` keyword handlers (Q3/CoD ui_shared.c). Appends one
// colorRangeDef_t (a value band low..high mapped to an RGBA color) to the item's
// colorRanges[] table.
//
// NAME ADJUDICATION: the .mcode header's assigned name `Menu_FindItemByName` is a
// pure size-match guess (win size 0xa4 == corpus size 0xa4) and is REJECTED. That
// name already belongs to the reconstructed 0x30051520 (functions/
// FUN_30051520_30051583.c), which does a case-insensitive item-name search — this
// function does no such thing. Its real identity is proven by:
//   - the diagnostic string it prints, "both addColorRange and addColorRangeRel -
//     set within same itemdef" (0x3007b478), which is verbatim the ui_shared.c
//     ParseColorRange conflict message;
//   - its two tiny wrappers immediately following it: 0x3005a1a0 calls it with
//     type=1 (addColorRangeRel) and 0x3005a1c0 with type=0 (addColorRange), each
//     forwarding (item, handle) — matching the PPC bank's ItemParse_addColorRange /
//     ItemParse_addColorRangeRel and their shared ParseColorRange helper;
//   - it parses two floats + an RGBA color into a 0x1c-byte record and appends it
//     to item->colorRanges[item->numColors++], capped at MAX_COLOR_RANGES.
//
// Register/stack ABI (i386, compiler-chosen; recorded, expressed as ordinary C):
//   - parse handle arrives in ECX  (moved to EDI, 0x3005a0f9);
//   - `type` discriminant arrives in EAX (0 = addColorRange, 1 = addColorRangeRel);
//   - `item` is the sole stack argument, read at [ESP+0x24] after SUB ESP,0x1c
//     (0x3005a0f4);
//   - returns the qboolean in EAX; RET (no imm) — the single stack arg is
//     caller-cleaned (the wrappers do ADD ESP,4).
//
// Both PC_Float_Parse (0x30050270) and PC_Color_Parse (0x300503a0) write their
// results into stack locals; on success the local low/high/color are laid out
// contiguously as one colorRangeDef_t and REP MOVSD-copied (7 dwords) into the
// item's table slot (0x3005a166..0x3005a176). The +0x10 gap dword of the record is
// not written by this path and is copied through as-is (see colorRangeDef_t).

#include "ui_parse.h"

#include <string.h>

qboolean ParseColorRange(int handle, int type, itemDef_t *item)
{
    // 0x3005a0fb..0x3005a10b: an item may carry only one KIND of color range. If it
    // already has ranges (numColors != 0) that were added by the OTHER keyword
    // (colorRangeType != type), reject with the conflict diagnostic.
    if (item->numColors != 0 && type != item->colorRangeType) {
        PC_SourceError(handle,
                       "both addColorRange and addColorRangeRel - set within same itemdef\n");
        return qfalse;                                  // 0x3005a11c XOR EAX,EAX
    }

    // 0x3005a128: stamp the owning keyword's kind (redundant when it already matched).
    item->colorRangeType = type;

    // 0x3005a12e / 0x3005a13b: parse the two band bounds; 0x3005a14b: parse the color.
    // Any failure returns qfalse without appending (0x3005a18b). The two floats and
    // the color are collected into one contiguous record on the stack.
    colorRangeDef_t range;
    if (!PC_Float_Parse(handle, &range.low))            // 0x3005a12e (out ptr in EBX)
        return qfalse;
    if (!PC_Float_Parse(handle, &range.high))           // 0x3005a13b
        return qfalse;
    if (!PC_Color_Parse(handle, range.color))           // 0x3005a14b (handle in EAX)
        return qfalse;

    // 0x3005a157..0x3005a17e: append the record while there is room. The cap is
    // silent — once the table is full the token is still consumed and qtrue returned.
    if (item->numColors < MAX_COLOR_RANGES) {      // CMP ...,0xa / JGE (signed)
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        memset(range.reserved10, 0, sizeof(range.reserved10));
        memcpy(&item->colorRanges[item->numColors], &range, sizeof(range));
        item->numColors++;                         // 0x3005a178 INC [EBP+0x12c]
    }

    return qtrue;                                       // 0x3005a181 MOV EAX,1
}
