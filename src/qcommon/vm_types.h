#ifndef QCOMMON_VM_TYPES_H
#define QCOMMON_VM_TYPES_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
#define VM_TYPES_STATIC_ASSERT(expression, message) static_assert((expression), message)
#define VM_TYPES_ALIGNOF(type) alignof(type)
#else
#define VM_TYPES_STATIC_ASSERT(expression, message) _Static_assert((expression), message)
#define VM_TYPES_ALIGNOF(type) _Alignof(type)
#endif

enum {
    VM_NAME_SIZE = 64,
    VM_FIND_PATH_SIZE = 256,
    VM_COUNT = 3
};

#define VM_DLL_ENTRY_SYMBOL "dllEntry"
#define VM_MAIN_SYMBOL "vmMain"

#if defined(_MSC_VER) && defined(_M_IX86)
#define VM_CDECL __cdecl
#elif defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
#define VM_CDECL __attribute__((cdecl))
#else
#define VM_CDECL
#endif

/* The engine stores an array-vector dispatcher in each native VM record.  Its
 * DLL syscall thunk converts the module's cdecl variadic call frame to this
 * pointer before invoking the engine service. */
typedef intptr_t(VM_CDECL *vmSystemCall_t)(intptr_t *arguments);

typedef intptr_t(VM_CDECL *vmMain_t)(int32_t command, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4,
                                     intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, intptr_t arg9, intptr_t arg10,
                                     intptr_t arg11);

/* CoDUOMP.exe and coduo_lnxded independently use the same three 0x90-byte
 * i386 native-VM slots: dispatcher +0x00, name +0x04, loaded path +0x44,
 * bulk-cleared but otherwise unused word +0x84, library handle +0x88, and
 * vmMain +0x8c.  The former server-only `searchPath` identity for +0x84 was
 * inherited from another engine and has no supporting CoD:UO access. */
typedef struct vm_s {
    vmSystemCall_t systemCall;
    char name[VM_NAME_SIZE];
    char loadedPath[VM_NAME_SIZE];
    uint32_t unused84;
    void *libraryHandle;
    vmMain_t vmMain;
} vm_t;

#if UINTPTR_MAX == UINT32_MAX
VM_TYPES_STATIC_ASSERT(VM_TYPES_ALIGNOF(vm_t) == 4, "i386 VM record alignment changed");
VM_TYPES_STATIC_ASSERT(offsetof(vm_t, systemCall) == 0x00, "i386 VM system-call entry moved");
VM_TYPES_STATIC_ASSERT(offsetof(vm_t, name) == 0x04, "i386 VM name moved");
VM_TYPES_STATIC_ASSERT(sizeof(((vm_t *)0)->name) == 0x40, "i386 VM name extent changed");
VM_TYPES_STATIC_ASSERT(offsetof(vm_t, loadedPath) == 0x44, "i386 VM loaded path moved");
VM_TYPES_STATIC_ASSERT(sizeof(((vm_t *)0)->loadedPath) == 0x40, "i386 VM loaded-path extent changed");
VM_TYPES_STATIC_ASSERT(offsetof(vm_t, unused84) == 0x84, "i386 VM unused lane moved");
VM_TYPES_STATIC_ASSERT(offsetof(vm_t, libraryHandle) == 0x88, "i386 VM library handle moved");
VM_TYPES_STATIC_ASSERT(offsetof(vm_t, vmMain) == 0x8c, "i386 VM entry point moved");
VM_TYPES_STATIC_ASSERT(sizeof(vm_t) == 0x90, "i386 VM record stride changed");
#endif

#undef VM_TYPES_STATIC_ASSERT
#undef VM_TYPES_ALIGNOF

#endif
