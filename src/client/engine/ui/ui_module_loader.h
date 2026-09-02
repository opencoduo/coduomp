#ifndef CODUOMP_UI_MODULE_LOADER_H
#define CODUOMP_UI_MODULE_LOADER_H

#include "../q_shared.h"
#include "../vm_runtime_compat.h"
#include "qcommon/cgame_module_abi_types.h"
#include "qcommon/ui_module_abi_types.h"

#include <stddef.h>
#include <stdint.h>

/* The executable stores an array-vector dispatcher in each VM record.  Its
 * 0x004685a0 native-DLL thunk converts the module's cdecl variadic call frame
 * to this pointer before invoking the engine service. */
#if UINTPTR_MAX == UINT32_MAX
typedef intptr_t (VM_CDECL *coduo_module_syscall_t)(
    intptr_t command, ...);
#else
typedef vmSystemCall_t coduo_module_syscall_t;
#endif
typedef void (VM_CDECL *coduo_dll_entry_t)(
    coduo_module_syscall_t moduleSyscall);

#ifdef __cplusplus
extern "C" {
#endif

extern vm_t *coduo_uiVm;
extern vm_t *coduo_cgameVm;

void *Sys_LoadDll(const char *moduleName, char *loadedPath,
                  vmMain_t *vmMain,
                  coduo_module_syscall_t moduleSyscall);
intptr_t CL_UISystemCalls(intptr_t *arguments);
void CL_InitUI(void);
int32_t UI_usesUniqueCDKey(void);
int32_t UI_checkKeyExec(int32_t key);
qboolean UI_GameCommand(void);

#ifdef __cplusplus
}
#endif

#endif
