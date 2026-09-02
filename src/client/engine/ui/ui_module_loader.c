#include "ui_module_loader.h"

#include "client/engine/client/cgame.h"
#include "qcommon/game_module_abi_types.h"
#include "qcommon/q_string.h"
#include "sound/alias/sound_alias.h"
#include "client/engine/system_localization.h"
#include "client/engine/system_platform.h"

#include <string.h>

_Noreturn extern void Com_Error(errorParm_t errorCode, const char *format, ...);

// Source: CoDUOMP.exe 0x048a54c0..0x048a566f.
vm_t vmTable[VM_COUNT];

// Source: CoDUOMP.exe 0x0389fdc8.
vm_t *currentVM;

// Source: CoDUOMP.exe 0x04957f24.
vm_t *coduo_uiVm;

// Source: CoDUOMP.exe 0x04e19990.
vm_t *coduo_cgameVm;

/* NOT_FROM_ORIGINAL_SOURCE: the Windows client has no VM_Create preflight
 * call corresponding to the dedicated engine's memory diagnostic. */
void vm_compat_before_create(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: retains the client executable's exact fatal text
 * at the target-owned error boundary. */
void vm_compat_bad_create_parameters(void)
{
    Com_Error(ERR_FATAL, "VM_Create: bad parms");
}

/* NOT_FROM_ORIGINAL_SOURCE: retains the client executable's exact fatal text
 * at the target-owned error boundary. */
void vm_compat_no_free_slot(void)
{
    Com_Error(ERR_FATAL, "VM_Create: no free vm_t");
}

/* NOT_FROM_ORIGINAL_SOURCE: adapts the shared VM lifecycle to the client
 * loader. Native cgame/UI modules consume vectors, while the native game
 * module retains its variadic game_syscall_t contract. */
void *vm_compat_load_library(const char *moduleName, char *loadedPath, vmMain_t *entryPoint, vmDllSyscall_t variadicSyscall,
                             vmSystemCall_t vectorSyscall)
{
#if UINTPTR_MAX == UINT32_MAX
    (void)vectorSyscall;
    return Sys_LoadDll(moduleName, loadedPath, entryPoint, variadicSyscall);
#else
    coduo_module_syscall_t moduleSyscall = vectorSyscall;
    if (strcmp(moduleName, "game") == 0) {
        _Static_assert(sizeof(moduleSyscall) == sizeof(variadicSyscall), "native VM syscall pointer size mismatch");
        memcpy(&moduleSyscall, &variadicSyscall, sizeof(moduleSyscall));
    }
    return Sys_LoadDll(moduleName, loadedPath, entryPoint, moduleSyscall);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: retains the client's localized DLL-load fatal. */
void vm_compat_load_failure(void)
{
    Com_Error(ERR_FATAL, "%s\n", Sys_LocalizeString("WIN_UNABLE_LOAD_DLL_BODY"));
}

/* NOT_FROM_ORIGINAL_SOURCE: client VM_Free ignores an already-empty handle
 * and treats an actual unload failure as fatal. */
void vm_compat_free_library(void *libraryHandle)
{
    if (libraryHandle != NULL && Sys_UnloadDll(libraryHandle) == 0)
        Com_Error(ERR_FATAL, "Sys_UnloadDll FreeLibrary failed");
}

/* NOT_FROM_ORIGINAL_SOURCE: target adapter for VM_Clear's proven nonnull
 * library handles. */
void vm_compat_clear_library(void *libraryHandle)
{
    if (Sys_UnloadDll(libraryHandle) == 0)
        Com_Error(ERR_FATAL, "Sys_UnloadDll FreeLibrary failed");
}

/* NOT_FROM_ORIGINAL_SOURCE: client call sites provide twelve explicit slots,
 * cast by ui_module_loader.h before entering the common variadic function. */
qboolean vm_compat_call_uses_fixed_arguments(void)
{
    return qtrue;
}

/* Source: CoDUOMP.exe 0x0041c300..0x0041c36b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041c300_0041c36b.mcode.
 * The VM_Create callee at 0x00468640 proves the native loader resolves
 * dllEntry/vmMain and invokes dllEntry with CL_UISystemCalls before returning
 * this record. The same-version Mac symbol/call record identifies the initial
 * 0x00437f40 call as Com_LoadSoundAliases. */
void CL_InitUI(void)
{
    intptr_t apiVersion;

    Com_LoadSoundAliases("menu", SND_ALIAS_BANK_COMMON);
    coduo_uiVm = VM_Create("ui", CL_UISystemCalls);
    if (coduo_uiVm == NULL) {
        Com_Error(ERR_FATAL, "VM_Create on UI failed");
    }

    apiVersion = VM_Call(coduo_uiVm, UIVM_GET_API_VERSION, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (apiVersion != UIVM_API_VERSION) {
        Com_Error(ERR_FATAL, "User Interface is version %d, expected %d", (int32_t)apiVersion, UIVM_API_VERSION);
    }

    (void)VM_Call(coduo_uiVm, UIVM_INIT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* Source: CoDUOMP.exe 0x0041c370..0x0041c38d.
 * Evidence: coduomp/mcode/CoDUOMP/executable_gaps.mcode and original objdump
 * at 0x0041c370. Exact same-version Mac symbol: UI_usesUniqueCDKey. */
int32_t UI_usesUniqueCDKey(void)
{
    if (coduo_uiVm == NULL) {
        return 0;
    }
    return VM_Call(coduo_uiVm, UIVM_USES_UNIQUE_CD_KEY, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == 1;
}

/* Source: CoDUOMP.exe 0x0041c390..0x0041c3a8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041c390_0041c3a8.mcode.
 * Exact same-version Mac symbol: UI_checkKeyExec. */
int32_t UI_checkKeyExec(int32_t key)
{
    if (coduo_uiVm == NULL) {
        return 0;
    }
    return (int32_t)VM_Call(coduo_uiVm, UIVM_CHECK_EXEC_KEY, key, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* Source: CoDUOMP.exe 0x0041c3b0..0x0041c3cc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041c3b0_0041c3cc.mcode.
 * Exact same-version Mac symbol: UI_GameCommand. */
qboolean UI_GameCommand(void)
{
    if (coduo_uiVm == NULL)
        return qfalse;

    return (qboolean)VM_Call(coduo_uiVm, UIVM_CONSOLE_COMMAND, cls.realtime, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}
