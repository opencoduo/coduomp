// Source: uo_cgame_mp_x86.dll 0x3003de10..0x3003de2c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003de10_3003de2c.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * trap_R_Text_Height — thin cdecl cgame trap wrapper for syscall id 53 (0x35, CG_R_TEXT_HEIGHT).
 *
 * Forwards its two stack arguments unchanged to cgame_syscall (the cgame VM trap
 * pointer at .data 0x30085e9c, distinct from the KERNEL32/USER32/GDI32 IAT at
 * 0x3006f000..0x3006f168) and returns the syscall's int32 result in EAX.
 *
 * cdecl, both args on the stack. On entry:
 *   [ESP+0x04]=a0  [ESP+0x08]=a1
 *
 * Body dataflow (0x3003de10..0x3003de2b):
 *   MOV EAX,[ESP+8]        ; EAX = a1
 *   MOV EDX,[ESP+4]        ; EDX = a0
 *   MOV ECX,EAX            ; ECX = a1
 *   PUSH ECX               ; push a1
 *   PUSH EDX               ; push a0
 *   PUSH 0x35              ; push command 53
 *   MOV [ESP+0x14],EAX     ; a1 -> its own inbound slot (compiler scratch store,
 *                          ;   no observable effect — the slot already held a1)
 *   CALL [0x30085e9c]      ; cgame_syscall(53, a0, a1)
 *   ADD ESP,0xc            ; balance the 3 dwords this wrapper pushed
 *   RET
 * Because the stack grows downward, cgame_syscall receives the pushes in
 * ascending order as (53, a0, a1). Both args pass full 32 bits (no narrowing).
 *
 * The plain RET leaves the two incoming args caller-cleaned; the ADD ESP,0xc only
 * balances the three dwords (2 args + command) pushed for the CALL.
 *
 * Naming: the .mcode header carried the mechanical guess trap_syscall_53 and the
 * command byte 0x35 == 53 proves trap id 53. That id is the text/line-metric query
 * CG_R_TEXT_HEIGHT, paired with CG_R_TEXT_WIDTH in the stance-hint draw (0x3002f42a) where the
 * result is FILD'd to a float line height. The exact original engine symbol behind
 * trap 53 is not proven (no cgame syscall-id table was recovered), so the id keeps
 * its honest CG_R_TEXT_HEIGHT name and the wrapper is named trap_R_Text_Height by its proven trap
 * id. Sibling of trap_R_Text_Width (0x3003dde0) and trap_R_Text_Paint (0x3003de30).
 */
int32_t trap_R_Text_Height(int32_t a0, int32_t a1)
{
    return (int32_t)cgame_syscall(CG_R_TEXT_HEIGHT, a0, a1);
}
