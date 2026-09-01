#include "script_error_exception.hpp"
#include "script_runtime_host.h"

#if defined(LINUX_BEHAVIOR)
/* The Linux compiler retains this empty constructor as the original leaf at
 * coduo_lnxded 0x080b0892; MSVC inlines the corresponding no-op. */
ScriptErrorClass::ScriptErrorClass()
{
}
#endif

/* Source: CoDUOMP.exe 0x00488490..0x004884c9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488490_004884ca.mcode.
 * The i386 MSVC build throws its one-byte ScriptErrorClass object through
 * _CxxThrowException while the VM owns a script frame. */
extern "C" void ScriptRuntime_RaiseError(void)
{
    if (script_callStackDepth != 0) {
        throw ScriptErrorClass();
    }

    Com_Error(ERR_DROP, "\x15%s", script_errorMessage);
}
