// Source: uo_cgame_mp_x86.dll 0x3002a400..0x3002a43d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002a400_3002a43d.mcode
//
// hudElemSortCompare — qsort comparator for HUD elements, ordering them by their
// float sort key (hudElem_t.sortKey at +0x6c) in ascending order.
//
// NAME ADJUDICATION: the .mcode's mechanical pre-hint `trap_DObjGetHierarchyBits`
// (a pure size match, "win size 0x3d == matched size 0x3d") is REJECTED. There is
// no trap here — the body issues no `PUSH id; CALL [0x30085e9c]` syscall, reads no
// hierarchy/DObj state, and returns no bit set. It is a two-argument float-field
// comparator. The name hudElemSortCompare is proven by behavior + call graph:
//   - its ONLY reference is `PUSH 0x3002a400` at 0x3002a482 inside
//     CG_GetSortedHudElems (0x3002a440), which passes it to qsort (0x3005ba40) as
//     the `compar` callback (PUSH cmp; PUSH 4; PUSH count; PUSH base);
//   - qsort's element size at that call is 4 and the array holds POINTERS to
//     hudElem_t, so each comparator argument points to a stored `hudElem_t *`; the
//     body dereferences once (MOV ECX,[EAX]) then reads the float at +0x6c, which
//     the shared hudElem_t model names `sortKey`.
//
// ABI: cdecl `int (const void *, const void *)` — args at [ESP+4]/[ESP+8], result
// in EAX, plain RET (caller cleans the stack). This is the exact signature the
// statically-linked MSVC qsort requires of its callback.
//
// BODY (the standard MSVC float-compare-to-zero idiom):
//   MOV EAX,[ESP+4]; MOV ECX,[EAX]        -> ea = *(hudElem_t **)a
//   MOV EDX,[ESP+8]; MOV EAX,[EDX]        -> eb = *(hudElem_t **)b
//   FLD [ECX+0x6c]; FSUB [EAX+0x6c]       -> st0 = ea->sortKey - eb->sortKey
//   FCOM 0.0; FNSTSW AX; TEST AH,5; JP    -> if (diff < 0) { FSTP; return -1; }
//   FCOMP 0.0; FNSTSW AX; TEST AH,0x41; JNZ-> if (diff > 0) return 1; else return 0;
// i.e. ea->sortKey < eb->sortKey -> -1, > -> +1, == -> 0.
//
// The 0.0f compared against is at .rdata 0x3007bcec (exact address dumped via
// objdump -s -j .rdata: the pool 3007bce0 = 1.0f/2.0f/0.5f/0.0f, so bcec = 0.0f).
// The adjacent 0.5f at 0x3007bce8 is NOT the constant used here.

#include "client/cgame/globals.h"          /* hudElem_t, hudElemSortCompare decl */

/*
 * qsort element type is `hudElem_t *`, so each argument points to a stored
 * pointer; dereference once, then compare the float sort keys.
 */
int hudElemSortCompare(const void *a, const void *b)
{
    const hudElem_t *ea = *(const hudElem_t *const *)a;
    const hudElem_t *eb = *(const hudElem_t *const *)b;
    const long double difference =
        (long double)ea->sortKey - (long double)eb->sortKey;

    /* The DLL compares the one unrounded FSUB result to 0 twice.  This also
     * preserves its unordered/NaN result of equality (0). */
    if (difference < (long double)0.0f) {
        return -1;
    }
    if (difference > (long double)0.0f) {
        return 1;
    }
    return 0;
}
