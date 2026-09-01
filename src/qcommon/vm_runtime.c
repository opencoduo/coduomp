#include "vm_runtime.h"

#include "coduo_vm_native_varargs.h"
#include "game_module_abi_types.h"
#include "q_string.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    VM_NAME_COMPARE_LIMIT = 99999,
    VM_CALL_ARGUMENT_COUNT = 12,
    VM_SYSCALL_ARGUMENT_COUNT = 12
};

/*
 * Complete native-VM lifecycle shared by the Windows client engine and Linux
 * dedicated engine:
 *
 *                         CoDUOMP.exe       coduo_lnxded
 * VM_Init                 0x00468580        0x0809bc50
 * VM_DllSyscall           0x004685a0        0x0809bc74
 * VM_Restart              0x004685c0        0x0809bc8b
 * VM_Create               0x00468640        0x0809bcdb
 * VM_Find                 0x00468750        0x0809be54
 * VM_Free                 0x004687b0        0x0809beed
 * VM_Clear                0x004687f0        0x0809bf2b
 * VM_Call                 0x00468890        0x0809bfb4
 *
 * Both i386 implementations own three 0x90-byte records, preserve the same
 * current-VM stack around vmMain, and forward twelve raw argument words.  The
 * dynamic-library search/error policy remains a target service.  Native64
 * argument materialization is explicitly marked compatibility code below;
 * it does not claim to be an original function.
 */

void VM_Init(void)
{
    memset(vmTable, 0, sizeof(vmTable));
}

#if UINTPTR_MAX != UINT32_MAX
/* NOT_FROM_ORIGINAL_SOURCE: returns the promoted native type sequence for one
 * game-module syscall before rebuilding the original intptr_t vector. */
static coduo_native_vararg_layout_t
coduo_vm_native_syscall_argument_layout(intptr_t command)
{
    switch ((game_syscall_id_t)command) {
#define CODUO_VM_SYSCALL_LAYOUT_CASE(enumName, argumentTypes) \
    case enumName:                                             \
        return (coduo_native_vararg_layout_t){                 \
            argumentTypes, sizeof(argumentTypes) - 1};
        CODUO_VM_NATIVE_GAME_SYSCALL_LAYOUTS(
            CODUO_VM_SYSCALL_LAYOUT_CASE)
#undef CODUO_VM_SYSCALL_LAYOUT_CASE
    default:
        return (coduo_native_vararg_layout_t){"", 0};
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: returns the promoted native type sequence for one
 * engine-to-game vmMain command before rebuilding its twelve-word vector. */
static coduo_native_vararg_layout_t
coduo_vm_native_game_command_argument_layout(int32_t command)
{
    switch ((game_command_t)command) {
#define CODUO_VM_GAME_LAYOUT_CASE(enumName, argumentTypes) \
    case enumName:                                          \
        return (coduo_native_vararg_layout_t){              \
            argumentTypes, sizeof(argumentTypes) - 1};
        CODUO_VM_NATIVE_GAME_COMMAND_LAYOUTS(
            CODUO_VM_GAME_LAYOUT_CASE)
#undef CODUO_VM_GAME_LAYOUT_CASE
    default:
        return (coduo_native_vararg_layout_t){"", 0};
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: reads one native promoted vararg without assuming
 * the original i386 adjacent-stack-word ABI. */
static intptr_t coduo_vm_read_native_argument(
    va_list *arguments, coduo_native_vararg_type_t type)
{
    switch (type) {
    case CODUO_NATIVE_VARARG_POINTER:
        return (intptr_t)va_arg(*arguments, void *);
    case CODUO_NATIVE_VARARG_UNSIGNED_INT:
        return (intptr_t)va_arg(*arguments, unsigned int);
    case CODUO_NATIVE_VARARG_SIZE:
        return (intptr_t)va_arg(*arguments, size_t);
    case CODUO_NATIVE_VARARG_SIGNED_INT:
    default:
        return (intptr_t)va_arg(*arguments, int);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: native modules that already construct an intptr_t
 * vector enter the original engine dispatcher through this direct adapter. */
intptr_t coduo_vm_dll_syscall_vector(intptr_t *arguments)
{
    return currentVM->systemCall(arguments);
}
#endif

intptr_t VM_CDECL VM_DllSyscall(intptr_t command, ...)
{
#if UINTPTR_MAX == UINT32_MAX
    /* The command and varargs are adjacent cdecl dwords in both originals. */
    return currentVM->systemCall(&command);
#else
    intptr_t arguments[VM_SYSCALL_ARGUMENT_COUNT + 1] = {0};
    coduo_native_vararg_layout_t layout =
        coduo_vm_native_syscall_argument_layout(command);
    size_t argumentCount = layout.argumentCount;
    if (argumentCount > VM_SYSCALL_ARGUMENT_COUNT)
        argumentCount = VM_SYSCALL_ARGUMENT_COUNT;

    arguments[0] = command;
    va_list ap;
    va_start(ap, command);
    for (size_t index = 0; index < argumentCount; ++index) {
        arguments[index + 1] = coduo_vm_read_native_argument(
            &ap, CODUO_NATIVE_VARARG_TYPE_AT(layout, index));
    }
    va_end(ap);
    return currentVM->systemCall(arguments);
#endif
}

vm_t *VM_Restart(vm_t *vm)
{
    const vmSystemCall_t systemCall = vm->systemCall;
    char moduleName[VM_NAME_SIZE];

    Q_strncpyz(moduleName, vm->name, (int32_t)sizeof(moduleName));
    VM_Free(vm);
    return VM_Create(moduleName, systemCall);
}

vm_t *VM_Create(const char *moduleName, vmSystemCall_t systemCall)
{
    if (moduleName == NULL || moduleName[0] == '\0' || systemCall == NULL) {
        vm_compat_bad_create_parameters();
        return NULL;
    }

    vm_compat_before_create();

    for (int32_t slot = 0; slot < VM_COUNT; ++slot) {
        if (vmTable[slot].name[0] != '\0' &&
            Q_stricmpn(vmTable[slot].name, moduleName,
                       VM_NAME_COMPARE_LIMIT) == 0) {
            return &vmTable[slot];
        }
    }

    vm_t *vm = NULL;
    for (int32_t slot = 0; slot < VM_COUNT; ++slot) {
        if (vmTable[slot].name[0] == '\0') {
            vm = &vmTable[slot];
            break;
        }
    }
    if (vm == NULL) {
        vm_compat_no_free_slot();
        return NULL;
    }

    Q_strncpyz(vm->name, moduleName, (int32_t)sizeof(vm->name));
    vm->systemCall = systemCall;
    vm->libraryHandle = vm_compat_load_library(
        moduleName, vm->loadedPath, &vm->vmMain,
        VM_DllSyscall,
#if UINTPTR_MAX == UINT32_MAX
        NULL
#else
        coduo_vm_dll_syscall_vector
#endif
    );
    if (vm->libraryHandle == NULL) {
        vm_compat_load_failure();
        return NULL;
    }
    return vm;
}

void *VM_Find(char *nameAndPathOut)
{
    for (int32_t slot = 0; slot < VM_COUNT; ++slot) {
        if (Q_stricmp(nameAndPathOut, vmTable[slot].name) == 0) {
            Q_strncpyz(nameAndPathOut, vmTable[slot].loadedPath,
                       VM_FIND_PATH_SIZE);
            return vmTable[slot].libraryHandle;
        }
    }
    return NULL;
}

void VM_Free(vm_t *vm)
{
    vm_compat_free_library(vm->libraryHandle);
    memset(vm, 0, sizeof(*vm));
    currentVM = NULL;
}

void VM_Clear(void)
{
    for (int32_t slot = 0; slot < VM_COUNT; ++slot) {
        if (vmTable[slot].libraryHandle != NULL)
            vm_compat_clear_library(vmTable[slot].libraryHandle);
        memset(&vmTable[slot], 0, sizeof(vmTable[slot]));
    }
    currentVM = NULL;
}

intptr_t VM_Call(vm_t *vm, int32_t command, ...)
{
    vm_t *const previousVM = currentVM;
    intptr_t arguments[VM_CALL_ARGUMENT_COUNT] = {0};

    currentVM = vm;

    va_list ap;
    va_start(ap, command);
#if UINTPTR_MAX == UINT32_MAX
    for (size_t index = 0; index < VM_CALL_ARGUMENT_COUNT; ++index)
        arguments[index] = va_arg(ap, intptr_t);
#else
    if (vm_compat_call_uses_fixed_arguments() != qfalse) {
        for (size_t index = 0; index < VM_CALL_ARGUMENT_COUNT; ++index)
            arguments[index] = va_arg(ap, intptr_t);
    } else {
        coduo_native_vararg_layout_t layout =
            coduo_vm_native_game_command_argument_layout(command);
        size_t argumentCount = layout.argumentCount;
        if (argumentCount > VM_CALL_ARGUMENT_COUNT)
            argumentCount = VM_CALL_ARGUMENT_COUNT;
        for (size_t index = 0; index < argumentCount; ++index) {
            arguments[index] = coduo_vm_read_native_argument(
                &ap, CODUO_NATIVE_VARARG_TYPE_AT(layout, index));
        }
    }
#endif
    va_end(ap);

    const intptr_t result = vm->vmMain(
        command, arguments[0], arguments[1], arguments[2], arguments[3],
        arguments[4], arguments[5], arguments[6], arguments[7],
        arguments[8], arguments[9], arguments[10], arguments[11]);
    currentVM = previousVM;
    return result;
}
