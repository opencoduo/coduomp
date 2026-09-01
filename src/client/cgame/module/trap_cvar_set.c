#include "../client_recovered.h"

// Source: uo_cgame_mp_x86.dll 0x3003d570..0x3003d586
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003d570_3003d586.mcode
//
// trap_Cvar_Set — the out-of-line cgame trap-9 wrapper. The DLL body loads its
// two cdecl stack arguments, pushes them (value first, then name) ahead of the
// command id 9, and forwards to the cgame system-call trap:
//
//   3003d570  MOV EAX,[ESP+8]        ; EAX = arg2 = value
//   3003d574  MOV ECX,[ESP+4]        ; ECX = arg1 = name
//   3003d578  PUSH EAX               ; push value  (3rd/last cgame_syscall arg)
//   3003d579  PUSH ECX               ; push name   (2nd cgame_syscall arg)
//   3003d57a  PUSH 0x9               ; push id 9   (1st cgame_syscall arg)
//   3003d57c  CALL [0x30085e9c]      ; cgame_syscall(9, name, value)
//   3003d582  ADD ESP,0xc            ; clean 3 dwords (caller-cleaned cdecl)
//   3003d585  RET
//
// Push order proves the argument shape: the trap reads [ESP]=id=9,
// [ESP+4]=name, [ESP+8]=value, so the first user parameter (arg1 = [ESP+4]) is
// the cvar NAME and the second (arg2 = [ESP+8]) is the VALUE string. Trap id 9
// is CG_CVAR_SET, the id-Tech trap_Cvar_Set (every id-9 call site across the DLL
// passes a cvar-name pointer and a value string). The wrapper never interprets
// the pointers, so they are forwarded bit-exact.

void trap_Cvar_Set(const char *name, const char *value)
{
    cgame_syscall(CG_CVAR_SET, (intptr_t)name, (intptr_t)value);
}
