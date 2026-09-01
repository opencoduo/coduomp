#ifndef QCOMMON_VM_RUNTIME_SERVICES_H
#define QCOMMON_VM_RUNTIME_SERVICES_H

#include "q_shared_types.h"
#include "vm_types.h"

typedef intptr_t (VM_CDECL *vmDllSyscall_t)(intptr_t command, ...);

/*
 * Target-owned boundaries used by the common native-VM runtime.  They retain
 * client/dedicated loader policy and native-host call adaptation without
 * forking the recovered VM lifecycle itself.
 */
void vm_compat_before_create(void);
void vm_compat_bad_create_parameters(void);
void vm_compat_no_free_slot(void);
void *vm_compat_load_library(const char *moduleName, char *loadedPath,
                             vmMain_t *entryPoint,
                             vmDllSyscall_t variadicSyscall,
                             vmSystemCall_t vectorSyscall);
void vm_compat_load_failure(void);
void vm_compat_free_library(void *libraryHandle);
void vm_compat_clear_library(void *libraryHandle);
qboolean vm_compat_call_uses_fixed_arguments(void);

#endif
