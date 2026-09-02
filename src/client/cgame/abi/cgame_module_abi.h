#ifndef CODUO_CGAME_MODULE_ABI_H
#define CODUO_CGAME_MODULE_ABI_H

#include <stdint.h>

#include "qcommon/cgame_module_abi_types.h"

#if defined(_MSC_VER)
#define CGAME_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define CGAME_EXPORT __attribute__((visibility("default")))
#else
#define CGAME_EXPORT
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
#define CGAME_ABI_CDECL __cdecl
#define CGAME_ABI_STDCALL __stdcall
#elif defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
#define CGAME_ABI_CDECL __attribute__((cdecl))
#define CGAME_ABI_STDCALL __attribute__((stdcall))
#else
#define CGAME_ABI_CDECL
#define CGAME_ABI_STDCALL
#endif

typedef int32_t cgame_abi_word32_t;
typedef uint32_t cgame_abi_pointer32_t;

typedef cgame_abi_word32_t (CGAME_ABI_CDECL *cgame_syscall32_t)(
    cgame_abi_word32_t command, ...);
typedef void (CGAME_ABI_CDECL *cgame_dllEntry32_t)(cgame_syscall32_t dispatcher);
typedef cgame_abi_word32_t (CGAME_ABI_CDECL *cgame_vmMain32_t)(
    cgame_abi_word32_t command,
    cgame_abi_word32_t arg0, cgame_abi_word32_t arg1,
    cgame_abi_word32_t arg2, cgame_abi_word32_t arg3,
    cgame_abi_word32_t arg4, cgame_abi_word32_t arg5,
    cgame_abi_word32_t arg6, cgame_abi_word32_t arg7,
    cgame_abi_word32_t arg8, cgame_abi_word32_t arg9,
    cgame_abi_word32_t arg10, cgame_abi_word32_t arg11);

typedef struct cgame_script_export_table32_s {
    cgame_abi_pointer32_t getFunction;
    cgame_abi_pointer32_t getMethod;
    cgame_abi_pointer32_t setObjectField;
    cgame_abi_pointer32_t getObjectField;
    cgame_abi_pointer32_t loadRead;
} cgame_script_export_table32_t;

_Static_assert(sizeof(cgame_abi_word32_t) == 4, "cgame VM words are 32-bit");
_Static_assert(sizeof(cgame_script_export_table32_t) == 20,
               "script export table is five pointer dwords");

#endif
