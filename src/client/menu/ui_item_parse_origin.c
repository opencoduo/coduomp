// Source: uo_cgame_mp_x86.dll 0x300593c0..0x3005940e;
//         uo_ui_mp_x86.dll    0x4001af30..0x4001af7e (exact after rebasing).
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300593c0_3005940e.mcode
//
// ItemParse_origin — the menu-item `origin <x> <y>` keyword handler. Parses two
// integers from the current script and adds them (as floats) to the item's
// client-space rectangle origin, window.rectClient.x/.y.
//
// Name resolution: the .mcode's size-matched guess was `G_Error_Localized`, which
// is REJECTED — this function parses menu-script integers and mutates a rectDef_t,
// it errors/localizes nothing. The real identity is proven mechanically: the .data
// menu keyword table entry at 0x3008b378 pairs the name pointer 0x3007b3d8 (the
// ASCII string "origin") with this function pointer 0x300593c0. This is the
// standard Q3 ui_shared.c ItemParse_origin handler, whose signature is
// (itemDef_t *item, int handle).
//
// Machine-code notes:
//  - Two int locals live in the 8-byte frame reserved by `SUB ESP,0x8`
//    (0x300593c0): x at [ESP+0x8] (&frame passed to the first PC_Int_Parse via
//    LEA EBX,[ESP+0x8]) and y at [ESP+0xc] (LEA EBX,[ESP+0xc]).
//  - PC_Int_Parse receives the script `handle` in EDI (loaded from arg1 at
//    [ESP+0x18] after the two pushes) and the out-pointer in EBX (register
//    convention; see its provisional decl in client_recovered.h). It returns the
//    success flag in EAX; TEST EAX,EAX / JZ short-circuits: if the first parse
//    fails, the second is skipped and the function returns 0 (XOR EAX,EAX) at
//    0x300593e3. If the first succeeds but the second fails (JNZ not taken at
//    0x300593e1), it also returns 0 via the same tail.
//  - On the success path (0x300593eb): item is reloaded from arg0 at [ESP+0x14]
//    into EAX, then the two ints are converted to float and accumulated in place:
//      FILD [x]; FADD dword ptr [item+0x10]; FSTP dword ptr [item+0x10]
//      FILD [y]; FADD dword ptr [item+0x14]; FSTP dword ptr [item+0x14]
//    i.e. rectClient.x += (float)x; rectClient.y += (float)y. The FADD/FSTP use
//    32-bit (single-precision) float memory operands, matching rectDef_t's vec_t
//    fields. The two POPs sit between the first FILD and the first FADD but do not
//    affect the x87 stack; they only restore EDI/EBX and adjust ESP so [x]/[y]
//    read at [ESP+0x8]/[ESP+0x4] at their respective FILDs. Returns 1.

#include "ui_parse.h"

qboolean ItemParse_origin(itemDef_t *item, int handle)
{
    int x;
    int y;

    /* PC_Int_Parse passes handle in EDI and the out-pointer in EBX; the first
     * failure short-circuits the second parse (JZ at 0x300593d4). */
    if (!PC_Int_Parse(handle, &x))
        return qfalse;
    if (!PC_Int_Parse(handle, &y))
        return qfalse;

    /* 0x300593ef..0x30059402: FILD feeds FADD directly (no intervening FSTP
     * DWORD). Keep each integer exact through the x87 addition and round only
     * once at the final float store; ordinary float += would permit the integer
     * conversion to round to binary32 before the add on non-x87 hosts. */
    item->window.rectClient.x = (float)((long double)x + (long double)item->window.rectClient.x);
    item->window.rectClient.y = (float)((long double)y + (long double)item->window.rectClient.y);

    return qtrue;
}
