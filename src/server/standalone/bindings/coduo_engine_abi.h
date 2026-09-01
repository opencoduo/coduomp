#ifndef CODUO_ENGINE_ABI_H
#define CODUO_ENGINE_ABI_H

/*
 * Engine/game boundary for the CoD:UO Linux dedicated host.
 *
 * The stock vmMain command and syscall domains are shared in
 * game_module_abi_types.h. Native loader names and host-width adaptation stay
 * here because they belong to the recovered engine compatibility boundary.
 */

#include <stddef.h>
#include <stdint.h>

#include "coduo_engine_platform.h"
#include "qcommon/game_module_abi_types.h"
#include "qcommon/vm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Original i386 Sys_LoadDll builds "<module>.mp.uo.i386.so" in a 0x100-byte
 * local buffer. Native recovered builds use a host-architecture suffix so a
 * 64-bit engine does not accidentally request the legacy i386 module name.
 */
#if defined(_WIN32)
#define CODUO_NATIVE_DLL_FILENAME_FORMAT "uo_%s_mp_x86.dll"
#elif UINTPTR_MAX == UINT32_MAX
#define CODUO_NATIVE_DLL_FILENAME_FORMAT "%s.mp.uo.i386.so"
#else
#define CODUO_NATIVE_DLL_FILENAME_FORMAT "%s.mp.uo.x86_64.so"
#endif
/*
 * VM_Call in coduo_lnxded pushes callNum plus 12 pointer-sized argument slots
 * before calling the module entry point. Native recovered game modules should
 * expose the full pointer-width signature even when a command consumes fewer
 * arguments.
 */
#ifdef __cplusplus
}
#endif

#endif /* CODUO_ENGINE_ABI_H */
