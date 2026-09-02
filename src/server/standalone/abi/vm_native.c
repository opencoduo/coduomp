#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "server/standalone/bindings/coduo_engine_abi.h"
#include "server/standalone/bindings/coduo_engine_structs.h"
#include "qcommon/vm_runtime.h"
#include "../core_memory/core_memory_private.h"
#include "../core_runtime/core_runtime_private.h"
#include "../filesystem/fs_private.h"

void Sys_UnloadDll(void *dllHandle);
void Sys_UnableToLoadDLLError(void);
void *Sys_LoadDll(const char *name, char *fqpath, vmMain_t *entryPoint, game_syscall_t syscall);

static game_dll_entry_t
/* NOT_FROM_ORIGINAL_SOURCE:
 * POSIX dlsym uses void * even for function symbols;
 * keep the non-ISO conversion local so GCC -pedantic does not reject the build.
 */
coduomp_vm_load_dll_entry_symbol(void *handle, const char *symbolName)
{
#if defined(_WIN32)
    FARPROC symbol = GetProcAddress((HMODULE)handle, symbolName);
#else
    void *symbol = dlsym(handle, symbolName);
#endif
    game_dll_entry_t function = NULL;

    _Static_assert(sizeof(function) == sizeof(symbol), "dlsym function pointer size mismatch");
    memcpy(&function, &symbol, sizeof(function));
    return function;
}

static vmMain_t
/* NOT_FROM_ORIGINAL_SOURCE:
 * POSIX dlsym uses void * even for function symbols;
 * keep the non-ISO conversion local so GCC -pedantic does not reject the build.
 */
coduomp_vm_load_dll_main_symbol(void *handle, const char *symbolName)
{
#if defined(_WIN32)
    FARPROC symbol = GetProcAddress((HMODULE)handle, symbolName);
#else
    void *symbol = dlsym(handle, symbolName);
#endif
    vmMain_t function = NULL;

    _Static_assert(sizeof(function) == sizeof(symbol), "dlsym function pointer size mismatch");
    memcpy(&function, &symbol, sizeof(function));
    return function;
}

/* NOT_FROM_ORIGINAL_SOURCE: preserves the dedicated VM_Create preflight call
 * at the target boundary. */
void vm_compat_before_create(void)
{
    (void)Hunk_MemoryRemaining();
}

/* NOT_FROM_ORIGINAL_SOURCE: retains the Linux executable's exact fatal text. */
void vm_compat_bad_create_parameters(void)
{
    Com_Error(0, "\x15"
                 "VM_Create: bad parms");
}

/* NOT_FROM_ORIGINAL_SOURCE: retains the Linux executable's exact fatal text. */
void vm_compat_no_free_slot(void)
{
    Com_Error(0, "\x15"
                 "VM_Create: no free vm_t");
}

/* NOT_FROM_ORIGINAL_SOURCE: the dedicated loader owns only game modules and
 * therefore consumes the common variadic game syscall adapter. */
void *vm_compat_load_library(const char *moduleName, char *loadedPath, vmMain_t *entryPoint, vmDllSyscall_t variadicSyscall,
                             vmSystemCall_t vectorSyscall)
{
    (void)vectorSyscall;
    return Sys_LoadDll(moduleName, loadedPath, entryPoint, variadicSyscall);
}

/* NOT_FROM_ORIGINAL_SOURCE: preserves the dedicated loader's fatal boundary. */
void vm_compat_load_failure(void)
{
    Sys_UnableToLoadDLLError();
}

/* NOT_FROM_ORIGINAL_SOURCE: Linux VM_Free forwards even a null handle so the
 * platform routine can emit its original diagnostic. */
void vm_compat_free_library(void *libraryHandle)
{
    Sys_UnloadDll(libraryHandle);
}

/* NOT_FROM_ORIGINAL_SOURCE: target adapter for VM_Clear's proven nonnull
 * library handles. */
void vm_compat_clear_library(void *libraryHandle)
{
    Sys_UnloadDll(libraryHandle);
}

/* NOT_FROM_ORIGINAL_SOURCE: native dedicated callers retain their original
 * variable argument counts and use the shared game-command layout table. */
qboolean vm_compat_call_uses_fixed_arguments(void)
{
    return qfalse;
}

void Sys_UnloadDll(void *dllHandle)
{
    if (dllHandle == NULL) {
        Com_Printf("Sys_UnloadDll(NULL)\n");
        return;
    }

#if defined(_WIN32)
    if (FreeLibrary((HMODULE)dllHandle) == 0) {
        Com_Printf("Sys_UnloadGame failed on FreeLibrary: %lu!\n", (unsigned long)GetLastError());
    }
#else
    dlclose(dllHandle);
    const char *error = dlerror();

    if (error != NULL) {
        Com_Printf("Sys_UnloadGame failed on dlclose: \"%s\"!\n", error);
    }
#endif
}

void Sys_UnableToLoadDLLError(void)
{
    Com_Error(0, "Unable to load shared library\n");
}

void *Sys_LoadDll(const char *name, char *fqpath, vmMain_t *entryPoint, game_syscall_t syscall)
{
#if !defined(_WIN32)
    const char *error = NULL;
#endif
    char dllName[VM_FIND_PATH_SIZE];
    char dllPath[MAX_OSPATH];

    fqpath[0] = '\0';
    snprintf(dllName, sizeof(dllName), CODUO_NATIVE_DLL_FILENAME_FORMAT, name);

    Sys_Cwd();
    const char *homepath = Cvar_VariableString("fs_homepath");
    const char *basepath = Cvar_VariableString("fs_basepath");
    const char *game = Cvar_VariableString("fs_game");

    FS_BuildOSPath(homepath, game, dllName, dllPath);
    Com_Printf("Sys_LoadDll(%s)... ", dllPath);

#if defined(_WIN32)
    void *handle = (void *)LoadLibraryA(dllPath);
    Sys_CheckCrashOrRerun();
#else
    void *handle = dlopen(dllPath, RTLD_NOW);
#endif

    if (handle == NULL) {
        Com_Printf("failed\n");

        FS_BuildOSPath(basepath, game, dllName, dllPath);
        Com_Printf("Sys_LoadDll(%s)... ", dllPath);

#if defined(_WIN32)
        handle = (void *)LoadLibraryA(dllPath);
        Sys_CheckCrashOrRerun();
#else
        handle = dlopen(dllPath, RTLD_NOW);
#endif
        if (handle == NULL) {
#if defined(_WIN32)
            Com_Printf("\nSys_LoadDll(%s) failed with Windows error %lu\n", dllPath, (unsigned long)GetLastError());
#else
            error = dlerror();
            Com_Printf("\nSys_LoadDll(%s) failed:\n\"%s\"\n", dllPath, error);
#endif
        } else {
            Com_Printf("ok\n");
        }

        if (handle == NULL) {
#if defined(_WIN32)
            Com_Printf("Sys_LoadDll(%s) failed LoadLibrary() completely!\n", name);
#else
            Com_Printf("Sys_LoadDll(%s) failed dlopen() completely!\n", name);
#endif
            return NULL;
        }
    } else {
        Com_Printf("ok\n");
    }

    Q_strncpyz(fqpath, dllPath, MAX_QPATH);

    game_dll_entry_t dllEntry = coduomp_vm_load_dll_entry_symbol(handle, VM_DLL_ENTRY_SYMBOL);
    *entryPoint = coduomp_vm_load_dll_main_symbol(handle, VM_MAIN_SYMBOL);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (*entryPoint == NULL || dllEntry == NULL) {
#if defined(_WIN32)
        Com_Printf("Sys_LoadDll(%s) failed GetProcAddress(vmMain/dllEntry): "
                   "Windows error %lu!\n",
                   name, (unsigned long)GetLastError());

        if (FreeLibrary((HMODULE)handle) == 0) {
            Com_Printf("Sys_LoadDll(%s) failed FreeLibrary: Windows error "
                       "%lu\n",
                       name, (unsigned long)GetLastError());
        }
#else
        error = dlerror();
        Com_Printf("Sys_LoadDll(%s) failed dlsym(vmMain):\n\"%s\" !\n", name, error);

        dlclose(handle);
        error = dlerror();
        if (error != NULL) {
            Com_Printf("Sys_LoadDll(%s) failed dlcose:\n\"%s\"\n", name, error);
        }
#endif

        return NULL;
    }

    Com_Printf("Sys_LoadDll(%s) found **vmMain** at  %p  \n", name, *entryPoint);
    dllEntry(syscall);
    Com_Printf("Sys_LoadDll(%s) succeeded!\n", name);

    return handle;
}
