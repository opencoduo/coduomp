// Source: uo_cgame_mp_x86.dll 0x3003de30..0x3003de89
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003de30_3003de89.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * trap_R_Text_Paint — thin cdecl cgame trap wrapper for syscall id 54 (0x36, CG_R_TEXT_PAINT).
 *
 * Forwards its nine stack arguments unchanged to cgame_syscall (the cgame VM trap
 * pointer at .data 0x30085e9c, distinct from the KERNEL32/USER32/GDI32 IAT at
 * 0x3006f000..0x3006f168) and returns the syscall's int32 result in EAX.
 *
 * cdecl, all args on the stack. On entry:
 *   [ESP+0x04]=a0 [ESP+0x08]=a1 [ESP+0x0c]=a2 [ESP+0x10]=a3 [ESP+0x14]=a4
 *   [ESP+0x18]=a5 [ESP+0x1c]=a6 [ESP+0x20]=a7 [ESP+0x24]=a8
 *
 * The body interleaves several EAX/ECX/EDX reloads with same-slot stores
 * (0x3003de3c a3->a3, 0x3003de49 a6->a6, 0x3003de51 a1->a1, 0x3003de5e a0->a0 —
 * compiler scratch, no observable effect), but the net dataflow is a plain
 * forward: it pushes a8,a7,a6,a5,a4,a3,a2,a1,a0 then the command 0x36. Because
 * the stack grows downward, cgame_syscall receives them in ascending order as
 *   (54, a0, a1, a2, a3, a4, a5, a6, a7, a8).
 * No argument is narrowed — all nine pass full 32 bits (unlike the MOVZX-ing
 * trap_XAnimSetCompleteGoalWeight sibling at 0x3003e8e0).
 *
 * The function issues a plain RET, so the nine incoming args are caller-cleaned;
 * the ADD ESP,0x28 at 0x3003de85 only balances the ten dwords (9 args + command)
 * this wrapper pushed for the CALL.
 *
 * Naming: the .mcode header carried the mechanical guess trap_syscall_54 and the
 * command byte 0x36 == 54 proves trap id 54. That id is the 2D text/element draw
 * service CG_R_TEXT_PAINT, exercised elsewhere with several arities (see the CG_R_TEXT_PAINT
 * emitter family in client_recovered.h); this is the fixed 9-argument entry point.
 * The exact original engine symbol behind trap 54 is not proven (no cgame
 * syscall-id table was recovered), so the id keeps its honest CG_R_TEXT_PAINT name and
 * the wrapper is named trap_R_Text_Paint by its proven trap id.
 */
int32_t trap_R_Text_Paint(intptr_t a0, intptr_t a1, intptr_t a2,
                         intptr_t a3, intptr_t a4, intptr_t a5,
                         intptr_t a6, intptr_t a7, intptr_t a8)
{
    return (int32_t)cgame_syscall(CG_R_TEXT_PAINT, a0, a1, a2, a3, a4, a5, a6, a7, a8);
}
