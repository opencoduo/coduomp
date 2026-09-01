#include "../abi/ui_module_abi.h"
#include "ui_globals.h"

#if UINTPTR_MAX == UINT32_MAX
// Source: uo_ui_mp_x86.dll 0x400410ac.
ui_syscall_t ui_syscall = (ui_syscall_t)(intptr_t)-1;
#else
/* NOT_FROM_ORIGINAL_SOURCE: native-width vector form of the original
 * command-plus-stack-dwords trap callback. */
ui_syscall_t ui_syscall_vector = (ui_syscall_t)(intptr_t)-1;
#endif

// Source: uo_ui_mp_x86.dll 0x4001d220..0x4001d22a
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d220_4001d22a.mcode
UI_EXPORT void UI_ABI_CDECL dllEntry(ui_syscall_t systemCall)
{
    ui_compat_reset_module_load_state();
#if UINTPTR_MAX == UINT32_MAX
    ui_syscall = systemCall;
#else
    ui_syscall_vector = systemCall;
#endif
}
