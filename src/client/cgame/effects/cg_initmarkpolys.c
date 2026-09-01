// Source: uo_cgame_mp_x86.dll 0x3002e400..0x3002e44e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002e400_3002e44e.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <string.h>

/*
 * CG_InitMarkPolys — reset the cgame mark-poly (decal) pool at startup.
 *
 * NAME: the .mcode name "QuatInverse" is REJECTED. It was a pure size match
 * (win 0x4e ~ corpus 0x4d), which the naming rules forbid, and the machine code
 * does no quaternion math whatsoever. This is the classic Quake3/CoD mark-poly
 * pool initializer: it memsets the whole cg_markPolys[] array, resets the
 * cg_activeMarkPolys sentinel to an empty circular list, points cg_freeMarkPolys
 * at cg_markPolys[0], and threads every node onto the singly-linked free list via
 * nextMark. The sibling allocator CG_AllocMark (0x3002e490) and the pool globals
 * (cg_markPolys @0x303b5d40, cg_freeMarkPolys @0x303b5d20, cg_activeMarkPolys
 * @0x30412d40) — already resolved in globals.h — corroborate the identity. It is
 * called from CG_Init immediately after CG_InitLocalEntities (both at 0x3002e2e1
 * / 0x3002e2e6).
 *
 * Machine-code facts (all proven against the .mcode):
 *   - PUSH EDI / POP EDI bracket only the REP STOSD scratch use of EDI; not
 *     source-level behavior.
 *   - MOV ECX,0x17400 ; MOV EDI,cg_markPolys ; XOR EAX,EAX ; REP STOSD
 *       zeroes 0x17400 dwords == MAX_MARK_POLYS * 0x174 == sizeof(cg_markPolys).
 *   - MOV EAX,&cg_activeMarkPolys ; MOV [+0x4],EAX ; MOV [+0x0],EAX
 *       sets cg_activeMarkPolys.nextMark (+0x4) then .prevMark (+0x0) to the
 *       sentinel itself (empty list). Store order is nextMark-then-prevMark.
 *   - MOV [cg_freeMarkPolys],&cg_markPolys[0].
 *   - Free-list thread loop: EAX = &cg_markPolys[1] (0x303b5eb4), ECX =
 *     &cg_markPolys[0], EDX = 0x3ff (MAX_MARK_POLYS - 1). Rotated so the body
 *     runs first via the JMP; each pass does [ECX+0x4] = EAX (node.nextMark =
 *     next node), ECX = EAX, EAX += 0x174 (advance one node), DEC EDX, loop while
 *     nonzero. Net: cg_markPolys[i].nextMark = &cg_markPolys[i+1] for
 *     i = 0 .. MAX_MARK_POLYS-2. The final node's nextMark is left as the NULL
 *     the memset wrote (loop stops before touching it).
 */
void CG_InitMarkPolys(void)
{
    int32_t i;

    memset(cg_markPolys, 0, sizeof(cg_markPolys));

    cg_activeMarkPolys.nextMark = &cg_activeMarkPolys;
    cg_activeMarkPolys.prevMark = &cg_activeMarkPolys;

    cg_freeMarkPolys = &cg_markPolys[0];
    for (i = 0; i < MAX_MARK_POLYS - 1; i++) {
        cg_markPolys[i].nextMark = &cg_markPolys[i + 1];
    }
}
