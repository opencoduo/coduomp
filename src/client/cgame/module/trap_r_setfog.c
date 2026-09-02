// Source: uo_cgame_mp_x86.dll 0x3003e040..0x3003e08f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003e040_3003e08f.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * trap_R_SetFog — thin seven-argument cgame trap wrapper for syscall id 0x44 (68).
 *
 * Calling convention: pure cdecl. Seven 32-bit stack arguments arrive at
 * [ESP+4]..[ESP+0x1c] (a1..a7). The body forwards them, in original order,
 * to the cgame VM syscall dispatch pointer cgame_syscall (*0x30085e9c) with
 * the trap command 0x44 (68) prepended, then cleans the eight pushed dwords
 * (ADD ESP,0x20) and returns with a plain RET — the seven caller args are
 * caller-cleaned. The syscall's int result in EAX is returned unchanged.
 *
 * The instruction scheduling is heavily scrambled by the compiler: it shuffles
 * a1..a7 through EAX/ECX/EDX and even reuses the caller's own a5/a6/a7 stack
 * slots as scratch before the pushes begin. Simulating the full interleaved
 * MOV/PUSH sequence (0x3003e040..0x3003e083) proves the net effect is a
 * straight in-order forward: the eight PUSHes deposit, in call order,
 * command 0x44 followed by a1, a2, a3, a4, a5, a6, a7.
 *
 * Body (0x3003e040..0x3003e08f):
 *   ... register/slot shuffle of the 7 incoming args ...
 *   PUSH a7 ; PUSH a6 ; PUSH a5 ; PUSH a4 ; PUSH a3 ; PUSH a2 ; PUSH a1
 *   PUSH 0x44                    ; command = 0x44 (68)
 *   CALL [0x30085e9c]            ; EAX = cgame_syscall(0x44, a1..a7)
 *   ADD ESP,0x20                 ; drop the 8 pushed dwords (0x20 = 32 bytes)
 *   RET                          ; plain RET: caller cleans its own 7 args
 *
 * The engine service behind trap 0x44 is not proven (no cgame syscall-id table
 * was recovered), so it keeps its proven trap-id name CG_R_SET_FOG. The mechanical
 * header name `trap_syscall_68` is just the address-suffixed trap id and is
 * superseded here by the resolved role name.
 */
int32_t trap_R_SetFog(int32_t a1, int32_t a2, int32_t a3, int32_t a4,
                  int32_t a5, int32_t a6, int32_t a7)
{
    return (int32_t)cgame_syscall(CG_R_SET_FOG, a1, a2, a3, a4, a5, a6, a7);
}
