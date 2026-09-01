// Source: uo_cgame_mp_x86.dll 0x3003d530..0x3003d53f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003d530_3003d53f.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * trap_DateTimeStamp — no-argument cgame trap wrapper for syscall id 0xf9 (249).
 *
 * Body (0x3003d530..0x3003d53f):
 *   PUSH 0xf9              ; cgame_syscall command = 0xf9 (249)
 *   CALL [0x30085e9c]      ; EAX = engine-owned timestamp-string pointer
 *   ADD ESP,0x4           ; caller-cleans the single pushed id dword (cdecl)
 *   RET                   ; return the syscall's pointer bits in EAX
 *
 * No user arguments are pushed; the sole stack push is the trap id itself, which
 * the wrapper cleans with ADD ESP,4 before a plain RET. Both direct consumers push
 * EAX as the `%s` timestamp argument to va, proving that the return carrier is a
 * string pointer rather than an integer. Preserve the native engine pointer on
 * 64-bit hosts while retaining the exact 32-bit EAX carrier on the retail target.
 *
 * The engine service behind trap 0xf9 is not proven (no cgame syscall-id table was
 * recovered), so it keeps its proven trap-id name CG_DATE_TIME_STAMP / trap_DateTimeStamp.
 *
 * Naming note: the mechanical `.mcode` header suggested `trap_syscall_249` from a
 * size/id match with zero behavioral basis. The proven role is a no-arg cgame trap
 * wrapper, matching the corpus CG_TrapNN family, so it is named trap_DateTimeStamp.
 */
const char *trap_DateTimeStamp(void)
{
    return (const char *)(uintptr_t)cgame_syscall(CG_DATE_TIME_STAMP);
}
