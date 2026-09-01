// Source: uo_cgame_mp_x86.dll 0x3002a4a0..0x3002a4d7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002a4a0_3002a4d7.mcode
//
// CG_DrawHudElems — gather the active HUD elements (sorted by sortKey) and draw
// each one.
//
// NAME ADJUDICATION: the .mcode's mechanical pre-hint `Q_random` (a pure size
// match, "win size 0x37 == matched size 0x37") is REJECTED. Size matching is
// forbidden and the body is not a PRNG: it allocates a 0x1f8-byte (504-byte)
// on-stack list, fills it via CG_GetSortedHudElems, and calls CG_DrawSingleHudElem on
// each returned element. The name CG_DrawHudElems is proven by:
//   - the call graph: it calls CG_GetSortedHudElems (0x3002a440) with the list
//     pointer in EDX and CG_DrawSingleHudElem (0x3002a310) per element with the element
//     pointer in ECX (both callees independently reconstructed);
//   - the frame size: 0x1f8 / 4 = 126 = twice the 63-element HUD capacity
//     the maximum CG_GetSortedHudElems can gather from the two hud arrays;
//   - the same-module PPC cgame_mp.dll name bank, which pairs
//     CG_DrawHudElems / CG_GetSortedHudElems.
// The header already declares this as CG_DrawHudElems(void) at 0x3002a4a0.
//
// ABI: void return, no arguments (plain `SUB ESP / ... / ADD ESP / RET`, no stack
// cleanup expected of the callee). EDI/ESI are callee-saved (PUSH/POP). The list
// pointer is passed to CG_GetSortedHudElems in EDX (its register-arg convention),
// and each element pointer is passed to CG_DrawSingleHudElem in ECX (its `this` register).
//
// Machine-code proof of the body:
//   3002a4a0 SUB ESP,0x1f8            reserve sortedList[126]
//   3002a4a6 PUSH EDI                 save EDI (holds the returned count)
//   3002a4a7 LEA EDX,[ESP+4]          EDX = &sortedList[0]
//   3002a4ab CALL 0x3002a440         count = CG_GetSortedHudElems(sortedList)
//   3002a4b0 MOV EDI,EAX             EDI = count
//   3002a4b2 TEST EDI,EDI / JZ end   if (count == 0) return
//   3002a4b6 PUSH ESI                save ESI (loop index)
//   3002a4b7 XOR ESI,ESI             i = 0
//   3002a4b9 TEST EDI,EDI / JLE end2 defensive count<=0 guard (already !=0 here)
//   3002a4c0 MOV ECX,[ESP+ESI*4+8]  ECX = sortedList[i] (ESP+8 after PUSH EDI+ESI)
//   3002a4c4 CALL 0x3002a310         CG_DrawSingleHudElem(sortedList[i])
//   3002a4c9 INC ESI / CMP ESI,EDI / JL loop
//   3002a4ce POP ESI / 3002a4cf POP EDI / ADD ESP,0x1f8 / RET

#include "../globals.h"           /* hudElem_t, PLAYERSTATE_HUD_ELEM_COUNT */
#include "../client_recovered.h"  /* CG_DrawHudElems, CG_GetSortedHudElems, CG_DrawSingleHudElem */

void CG_DrawHudElems(void)
{
    /* SUB ESP,0x1f8: room for pointers to every gatherable element from both
     * embedded HUD arrays (2 * PLAYERSTATE_HUD_ELEM_COUNT). */
    hudElem_t *sortedList[PLAYERSTATE_HUD_ELEM_COUNT * 2];
    int count;
    int i;

    /* CALL 0x3002a440 with EDX = &sortedList[0]; count returned in EAX. */
    count = CG_GetSortedHudElems(sortedList);
    if (count == 0) {
        return;
    }

    /* Draw each gathered element in sorted order; CG_DrawSingleHudElem takes the
     * element pointer in ECX. */
    for (i = 0; i < count; i++) {
        CG_DrawSingleHudElem(sortedList[i]);
    }
}
