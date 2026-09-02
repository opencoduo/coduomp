#ifndef CODUOMP_VM_RUNTIME_COMPAT_H
#define CODUOMP_VM_RUNTIME_COMPAT_H

#include "qcommon/vm_runtime.h"

/* NOT_FROM_ORIGINAL_SOURCE: native client-engine calls already provide twelve
 * explicit slots. Cast them before entering the common variadic VM_Call so
 * host default promotions cannot change their width. */
#if UINTPTR_MAX != UINT32_MAX
#define VM_Call(vm, command, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
    (VM_Call)((vm), (command), (intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), \
              (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), (intptr_t)(a11))
#endif

#endif
