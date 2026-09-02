#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3002b370..0x3002b383
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002b370_3002b383.mcode
//
// CG_BoldGameMessage: a thin cgame system-call wrapper for trap id 3 (CG_BOLD_GAME_MESSAGE). It
// forwards one caller-supplied argument (passed in ECX, i.e. a __fastcall-shaped
// first parameter) plus a fixed module global to the engine via *cgame_syscall.
//
// Proven instruction-by-instruction:
//
//   3002b370 a1 4c c1 48 30   MOV EAX,[0x3048c14c]   EAX = cg_gameBoldMessageWidth_vmCvar.integer
//   3002b375 50               PUSH EAX               cgame_syscall arg3 = that global
//   3002b376 51               PUSH ECX               cgame_syscall arg2 = caller's ECX arg
//   3002b377 6a 03            PUSH 0x3               cgame_syscall command = CG_BOLD_GAME_MESSAGE (id 3)
//   3002b379 ff 15 9c 5e 08 30 CALL [0x30085e9c]     cgame_syscall(CG_BOLD_GAME_MESSAGE, arg, global)
//   3002b37f 83 c4 0c         ADD ESP,0xc            caller-clean 3 pushed dwords (cdecl)
//   3002b382 c3               RET
//
// Argument shape (3 dwords pushed, caller-cleaned): id 3, then the ECX register
// argument, then the .data global at 0x3048c14c. The ECX-in convention makes the
// wrapper itself __fastcall; the first (and only) user parameter arrives in ECX
// and is forwarded verbatim. The trailing global cg_gameBoldMessageWidth_vmCvar.integer
// (0x3048c14c) is referenced ONLY by this wrapper (refs=1) and is .data-initialized
// to 0; its role is not proven, so it stays a mechanical global.
//
// This is the sibling of CG_GameMessage (0x3002b350): identical body, differing only in
// the trap id (2 vs 3) and the trailing global (0x304568cc vs 0x3048c14c).
//
// The .mcode's mechanical name (trap_syscall_3) is a size-match guess with no
// behavioral basis and is ignored; the name follows the corpus CG_Trap<n> wrapper
// convention (siblings CG_GameMessage/trap_R_Text_Width/trap_R_Text_Height) for unidentified low cgame
// traps, keyed off the proven trap id.
//
// The wrapper leaves the syscall result in EAX. All three direct callers discard
// it, but the original engine command's return contract is not needed or assumed.
int32_t CG_BoldGameMessage(const char *message /* ECX */)
{
    return coduo_int32_from_bits((uint32_t)cgame_syscall(CG_BOLD_GAME_MESSAGE, (intptr_t)message, cg_gameBoldMessageWidth_vmCvar.integer));
}
