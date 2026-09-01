// Source: uo_cgame_mp_x86.dll 0x30059670..0x3005973f;
//         uo_ui_mp_x86.dll    0x4001b1e0..0x4001b2af (exact after rebasing).
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30059670_3005973f.mcode
//
// ItemParse_columns — menu itemDef "columns" keyword handler.
//
// Name resolution: the .data keyword-table entry at 0x3008b410 pairs the string
// "columns" (0x3007b348) with this handler, which fixes the name as the Q3
// ui_shared.c ItemParse_columns. The .mcode header's size-matched guess
// (PlayerCmd_getCurrentWeapon) is rejected: this parses UI listbox columns.
//
// The function first ensures the item's listbox typeData is allocated via
// Item_ValidateTypeData(item, handle), then requires the item to have validated
// as ITEM_TYPE_LISTBOX (itemDef_t.typeValidated, +0xcc). It parses one leading
// int (numColumns, clamped to MAX_LB_COLUMNS = 16) then three ints per column
// (pos, width, maxChars) into listBoxDef_t.columnInfo[]. Any failed integer
// parse returns qfalse.
//
// i386 register ABI (recorded, expressed as ordinary C):
//   - item arrives in the arg0 stack slot (entry+4), loaded into EBX.
//   - handle arrives in the arg1 stack slot (entry+8), loaded into EDI and kept
//     across the whole body as the parse handle for PC_Int_Parse.
//   - Item_ValidateTypeData is called with item in EAX and handle on the stack.
//   - PC_Int_Parse is called with handle in EDI and the out int* in EBX.
// The numColumns local reuses the arg0 stack slot (entry+4) once `item` is no
// longer needed (EBX is repurposed as the PC_Int_Parse out-pointer register).

#include "ui_parse.h"
#include "ui_runtime.h"

void Com_Printf(const char *format, ...);

qboolean ItemParse_columns(itemDef_t *item, int handle)
{
    listBoxDef_t *listPtr;
    int numColumns;
    int i;

    /* 0x30059681: Item_ValidateTypeData(item, handle) — allocate the listbox
     * typeData payload once (caller-cleaned stack arg). */
    Item_ValidateTypeData(item, handle);

    /* 0x30059686: reload typeData after validation; 0x3005968f/0x30059691:
     * fail (return qfalse) if it is still NULL. */
    listPtr = (listBoxDef_t *)item->typeData;
    if (listPtr == 0) {
        return qfalse;
    }

    /* 0x30059693: require the committed type to be ITEM_TYPE_LISTBOX; otherwise
     * emit the menu error and return qfalse. */
    if (item->typeValidated != ITEM_TYPE_LISTBOX) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");
        return qfalse;
    }

    /* 0x300596b6: parse the column count. */
    if (!PC_Int_Parse(handle, &numColumns)) {
        return qfalse;
    }

    /* 0x300596c3: clamp numColumns to MAX_LB_COLUMNS (JLE keeps <= 16 as-is). */
    if (numColumns > MAX_LB_COLUMNS) {
        numColumns = MAX_LB_COLUMNS;
    }

    /* 0x300596d9: commit the (clamped) count. */
    listPtr->numColumns = numColumns;

    /* 0x300596d7/0x300596dc: signed test — a non-positive count parses nothing
     * and still succeeds. The loop index (EBP) is a signed compare (JL). */
    for (i = 0; i < numColumns; i++) {
        int pos;
        int width;
        int maxChars;

        /* 0x300596e5 / 0x300596f2 / 0x300596ff: three ints per column, in call
         * order pos, width, maxChars; any failure returns qfalse. */
        if (!PC_Int_Parse(handle, &pos)) {
            return qfalse;
        }
        if (!PC_Int_Parse(handle, &width)) {
            return qfalse;
        }
        if (!PC_Int_Parse(handle, &maxChars)) {
            return qfalse;
        }

        /* 0x30059714/0x3005971b/0x3005971d: store this column. */
        listPtr->columnInfo[i].pos = pos;
        listPtr->columnInfo[i].width = width;
        listPtr->columnInfo[i].maxChars = maxChars;
    }

    /* 0x3005972b: return qtrue. */
    return qtrue;
}
