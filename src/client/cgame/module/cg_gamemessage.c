// Source: uo_cgame_mp_x86.dll 0x3002b350..0x3002b363
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002b350_3002b363.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

/*
 * CG_GameMessage — thin fastcall wrapper for cgame syscall id 2 (CG_GAME_MESSAGE).
 *
 * Body (0x3002b350..0x3002b363):
 *   MOV  EAX,[0x304568cc]   ; EAX = cg_gameMessageWidth_vmCvar.integer
 *   PUSH EAX                ; cgame_syscall arg2 = the .data mode/flag global
 *   PUSH ECX                ; cgame_syscall arg1 = ECX (incoming register arg)
 *   PUSH 0x2                ; cgame_syscall command = 2 (CG_GAME_MESSAGE)
 *   CALL [0x30085e9c]       ; cgame_syscall(2, ECX, cg_gameMessageWidth_vmCvar.integer)
 *   ADD  ESP,0xc            ; drop the 3 pushed dwords (caller-cleaned cdecl)
 *   RET                     ; plain RET — the ECX arg is an incoming register arg
 *
 * Calling convention: arg1 arrives in ECX (fastcall-style register argument; it is
 * never loaded from a stack slot, so it is an incoming argument, not scratch). The
 * plain RET (not RET imm) confirms there are no incoming stack arguments to clean.
 *
 * Argument shape (pushes are right-to-left, so the C call order is id, ECX, global):
 *   cgame_syscall(CG_GAME_MESSAGE, arg, cg_gameMessageWidth_vmCvar.integer)
 * The second argument is always the shared .data global at 0x304568cc — a persistent
 * mode/flag word that the sibling call site 0x3003cd40..0x3003cd4f also forwards as
 * the trap's second argument (there arg1 is a va()/sprintf-formatted string).
 *
 * The engine service behind trap 2 is not proven (no cgame syscall-id table was
 * recovered), so it keeps its honest proven trap-id name CG_GAME_MESSAGE, and this wrapper
 * is named CG_GameMessage by its role. The `.mcode` size-match `# name trap_syscall_2` is
 * only the honest trap id, carrying no behavioral basis; the return value is unused
 * by this wrapper (declared void — the function issues a plain RET without reading
 * EAX further, matching a fire-and-forget trap).
 */
void CG_GameMessage(const char *message)
{
    cgame_syscall(CG_GAME_MESSAGE, (intptr_t)message, cg_gameMessageWidth_vmCvar.integer);
}
