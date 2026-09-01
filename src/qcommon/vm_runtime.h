#ifndef QCOMMON_VM_RUNTIME_H
#define QCOMMON_VM_RUNTIME_H

#include "vm_runtime_services.h"

#ifdef __cplusplus
extern "C" {
#endif

extern vm_t vmTable[VM_COUNT];
extern vm_t *currentVM;

void VM_Init(void);
vm_t *VM_Create(const char *moduleName, vmSystemCall_t systemCall);
vm_t *VM_Restart(vm_t *vm);
void *VM_Find(char *nameAndPathOut);
void VM_Free(vm_t *vm);
void VM_Clear(void);
intptr_t VM_Call(vm_t *vm, int32_t command, ...);
intptr_t VM_CDECL VM_DllSyscall(intptr_t command, ...);

#if UINTPTR_MAX != UINT32_MAX
/* NOT_FROM_ORIGINAL_SOURCE: native-width vector form of the original i386
 * command-plus-adjacent-stack-words DLL syscall thunk. */
intptr_t coduo_vm_dll_syscall_vector(intptr_t *arguments);
#endif

#ifdef __cplusplus
}
#endif

#endif
