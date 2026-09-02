// Source: uo_cgame_mp_x86.dll 0x3002a440..0x3002a498
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002a440_3002a498.mcode
//
// CG_GetSortedHudElems — collect the active HUD elements from the local player's
// two embedded playerState hud arrays and return them, sorted for drawing.
//
// NAME ADJUDICATION: the .mcode's mechanical pre-hint `BG_WeaponTrackValue` (a
// pure size match, "win size 0x58 == matched size 0x58") is REJECTED. Size
// matching is forbidden and the machine code proves a different function
// entirely: this reads no weapon/BG state, takes no weapon index, and returns a
// count of gathered HUD-element pointers, not a per-weapon value. The name
// CG_GetSortedHudElems is proven by behavior + call graph:
//   - it walks the two hud arrays embedded in the local playerState (reached via
//     cg_snap + 0x0c = ps base, then ps+0x7f8 and ps+0x267c);
//   - it gathers the ACTIVE entries (type != 0) into the caller's list and qsorts
//     them by the float sort key at hudElem_t+0x6c (comparator at 0x3002a400);
//   - its only caller, CG_DrawHudElems (0x3002a4a0), then calls CG_DrawSingleHudElem
//     (0x3002a310) on each returned element in sorted order;
// and corroborated by the same-module PPC cgame_mp.dll bank, which names the
// sibling pair CG_DrawHudElems / CG_GetSortedHudElems.
//
// ABI (proven from the call site, 0x3002a4a0): the output list pointer arrives in
// EDX (register argument, `hudElem_t **sortedList`); the count is returned in EAX
// (plain `RET`, no stack cleanup). ESI/EDI are callee-saved (PUSH/POP). The list
// slots hold POINTERS to the hud elements (the qsort element size is 4).

#include "client/cgame/globals.h"          /* cg_snap, snapshot_t, hudElem_t, HUD count */
#include "client/cgame/client_recovered.h"
#include "compat/crt/qsort_compat.h"

#include <stdint.h>

/*
 * The qsort comparator hudElemSortCompare (0x3002a400) is reconstructed in its own
 * artifact (src/client/cgame/hud/hudelemsortcompare.c) and declared in globals.h.
 * It compares two hud elements by their float sortKey (hudElem_t+0x6c) ascending.
 */

/*
 * Gather the active HUD elements into sortedList and sort them by sortKey.
 * Returns the number of elements gathered (0..2*PLAYERSTATE_HUD_ELEM_COUNT).
 *
 * The output list pointer is the EDX register argument at the call site.
 */
int CG_GetSortedHudElems(hudElem_t **sortedList)
{
    /* MOV EDI,[0x30459160]; ADD EDI,0xc  ->  ps = &cg_snap->ps, modeled here as
     * the snapshot base since the two hud arrays are members of snapshot_t. */
    snapshot_t *snap = cg_snap;
    int count = 0;   /* ESI: running total across both arrays */
    int i;           /* ECX: per-array index, reset between the two loops */

    /* Loop 1: gather from the archival array (ps+0x267c == snapshot+0x2688).
     * LEA EAX,[EDI+0x267c]; while active (type != 0) and index < 0x3f, store the
     * element pointer and advance by 0x7c. */
    for (i = 0; i < PLAYERSTATE_HUD_ELEM_COUNT && snap->ps.hudArchival[i].type != 0; i++) {
        sortedList[count++] = &snap->ps.hudArchival[i];
    }

    /* Loop 2: gather from the current array (ps+0x7f8 == snapshot+0x804).
     * LEA EAX,[EDI+0x7f8]; same active-and-in-range test, appending after the
     * archival entries already stored (ESI is not reset, ECX is). */
    for (i = 0; i < PLAYERSTATE_HUD_ELEM_COUNT && snap->ps.hudCurrent[i].type != 0; i++) {
        sortedList[count++] = &snap->ps.hudCurrent[i];
    }

    /* PUSH 0x3002a400; PUSH 4; PUSH ESI; PUSH EDX; CALL qsort; ADD ESP,0x10.
     * cdecl args (base, count, size, cmp); element size 4 = sizeof(hudElem_t *). */
    coduo_crt_qsort(sortedList, (size_t)count, sizeof(sortedList[0]), hudElemSortCompare);

    /* MOV EAX,ESI: return the total number of gathered elements. */
    return count;
}
