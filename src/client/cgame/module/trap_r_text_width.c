// Source: uo_cgame_mp_x86.dll 0x3003dde0..0x3003de06
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003dde0_3003de06.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * trap_R_Text_Width — thin cdecl cgame trap wrapper for syscall id 52 (0x34, CG_R_TEXT_WIDTH).
 *
 * Forwards its four stack arguments unchanged to cgame_syscall (the cgame VM trap
 * pointer at .data 0x30085e9c, distinct from the KERNEL32/USER32/GDI32 IAT at
 * 0x3006f000..0x3006f168) and returns the syscall's int32 result in EAX. Callers
 * FILD the result into a float text/line metric (see CG_R_TEXT_WIDTH / the stance-hint
 * width measurement in the text-draw family). Sibling of trap_R_Text_Height (0x3003de10).
 *
 * cdecl, all four args on the stack. On entry:
 *   [ESP+0x04]=a0  [ESP+0x08]=a1  [ESP+0x0c]=a2  [ESP+0x10]=a3
 *
 * Body dataflow (0x3003dde0..0x3003de05); ESP offsets below are stated relative to
 * entry, accounting for each PUSH shifting the frame:
 *   MOV EAX,[ESP+0x0c]     ; EAX = a2
 *   MOV ECX,[ESP+0x10]     ; ECX = a3
 *   PUSH ECX               ; push a3        (ESP -= 4)
 *   MOV ECX,[ESP+0x08]     ; ECX = a0       (post-push slot = entry [ESP+0x04])
 *   MOV EDX,EAX            ; EDX = a2
 *   PUSH EDX               ; push a2        (ESP -= 4)
 *   MOV [ESP+0x14],EAX     ; a2 -> its own inbound slot (compiler scratch store,
 *                          ;   no observable effect — the slot already held a2)
 *   MOV EAX,[ESP+0x10]     ; EAX = a1       (post-push slot = entry [ESP+0x08])
 *   PUSH EAX               ; push a1        (ESP -= 4)
 *   PUSH ECX               ; push a0        (ESP -= 4)
 *   PUSH 0x34              ; push command 52 (ESP -= 4)
 *   CALL [0x30085e9c]      ; cgame_syscall(52, a0, a1, a2, a3)
 *   ADD ESP,0x14           ; balance the 5 dwords this wrapper pushed
 *   RET
 * Because the stack grows downward, the pushes (a3, a2, a1, a0, 0x34) present to
 * cgame_syscall in ascending order as (52, a0, a1, a2, a3). All four args pass full
 * 32 bits (no narrowing).
 *
 * The plain RET leaves the four incoming args caller-cleaned; the ADD ESP,0x14 only
 * balances the five dwords (4 args + command) pushed for the CALL.
 *
 * Naming: the .mcode header carried the mechanical guess trap_syscall_52 and the
 * command byte 0x34 == 52 proves trap id 52 (CG_R_TEXT_WIDTH). The exact original engine
 * symbol behind trap 52 is not proven (no cgame syscall-id table was recovered), so
 * the id keeps its honest CG_R_TEXT_WIDTH name and the wrapper is named trap_R_Text_Width by its
 * proven trap id, per corpus convention. Sibling of trap_R_Text_Height (0x3003de10).
 */
int32_t trap_R_Text_Width(const char *text, int32_t font, int32_t scaleBits, int32_t limit)
{
    return coduo_int32_from_bits((uint32_t)cgame_syscall(CG_R_TEXT_WIDTH, (intptr_t)text, font, scaleBits, limit));
}
