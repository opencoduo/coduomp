// Source: uo_cgame_mp_x86.dll 0x3003e490..0x3003e4a4
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003e490_3003e4a4.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * trap_Com_SoundAliasString — thin cdecl cgame trap wrapper for syscall id 0xc3.
 *
 * Forwards its single stack argument to cgame_syscall (the VM trap pointer at
 * .data 0x30085e9c) and returns the syscall's int32 result in EAX.
 *
 * Body (0x3003e490..0x3003e4a4):
 *   MOV EAX,[ESP+4]        ; arg0 = the single incoming stack argument
 *   PUSH EAX               ; cgame_syscall arg1 = arg0
 *   PUSH 0xc3              ; cgame_syscall command = 0xc3 (195)
 *   CALL [0x30085e9c]      ; EAX = cgame_syscall(0xc3, arg0)
 *   ADD ESP,0x8            ; drop the 2 pushed dwords (this frame's own pushes)
 *   RET                    ; plain RET -> the one incoming arg is caller-cleaned (cdecl)
 *
 * The named Mac cgame wrapper and engine Com_SoundAliasString establish the
 * source name and result type. The engine finds the alias list and returns the
 * canonical alias-name pointer stored at snd_alias_t +0x00; it does not allocate
 * or return an integer sound handle.
 *
 * Naming note: the mechanical .mcode header labeled this `trap_syscall_195`, a
 * generic mechanical name derived from the pushed command id. It is retained in
 * substance (the trap id is proven); the original Mac symbol supplies the exact
 * trap_Com_SoundAliasString spelling.
 */
const char *trap_Com_SoundAliasString(const char *name)
{
    return (const char *)(uintptr_t)cgame_syscall(CG_COM_SOUND_ALIAS_STRING, (intptr_t)name);
}
