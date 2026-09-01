// Source: uo_cgame_mp_x86.dll 0x3002b4d0..0x3002b4ec
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002b4d0_3002b4ec.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_Argv — read console argument token `n` into the shared text scratch buffer and
 * return a pointer to it. This is the id-Tech `Cmd_Argv`/`CG_Argv` convenience idiom:
 * fill a static buffer via the CG_ARGV trap, then hand the buffer back to the caller.
 *
 * Machine code (0x3002b4d0..0x3002b4ec):
 *   PUSH 0x400              ; bufferLength = sizeof(g_textScratchBuffer) == 1024
 *   PUSH 0x300da488         ; buffer = &g_textScratchBuffer
 *   PUSH EAX                ; n (the argv index, arrives in EAX)
 *   PUSH 0xd                ; command = CG_ARGV (13)
 *   CALL [0x30085e9c]       ; cgame_syscall(CG_ARGV, n, &g_textScratchBuffer, 1024)
 *   ADD ESP,0x10            ; caller-cleaned cdecl into the trap: balance the 4 pushes
 *   MOV EAX,0x300da488      ; return value = &g_textScratchBuffer
 *   RET
 *
 * Because the pushes go right-to-left, cgame_syscall receives ascending
 * (0xd, n, &g_textScratchBuffer, 0x400). The syscall's own return in EAX is
 * discarded — the wrapper overwrites EAX with the buffer address and returns that.
 *
 * ABI: `n` is passed in EAX (register/fastcall-style), not on the stack — every
 * caller loads it with `mov $N,%eax` immediately before the call (e.g. 0x3003a691
 * with n=2, 0x3003acde/0x3003ad30 with n=1). There are no incoming stack args and
 * the function issues a plain RET, so nothing is caller-cleaned on entry. Expressed
 * here as an ordinary C parameter; the EAX-in convention is an i386 calling-detail,
 * not source-level behavior.
 */
char *CG_Argv(int32_t n)
{
    trap_Argv(n, g_textScratchBuffer, (int32_t)sizeof(g_textScratchBuffer));
    return g_textScratchBuffer;
}
