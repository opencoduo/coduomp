#include "../client_recovered.h"
#include "../abi/cgame_module_abi.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x3003d470..0x3003d47a
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003d470_3003d47a.mcode
//
CGAME_EXPORT void CGAME_ABI_CDECL dllEntry(cgame_syscall_t systemCall)
{
    cgame_compat_reset_module_load_state();
#if UINTPTR_MAX == UINT32_MAX
    cgame_syscall = systemCall;
#else
    cgame_syscall_vector = systemCall;
#endif
}
