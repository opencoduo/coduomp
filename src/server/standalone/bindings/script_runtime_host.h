#ifndef CODUO_ENGINE_SCRIPT_RUNTIME_HOST_H
#define CODUO_ENGINE_SCRIPT_RUNTIME_HOST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h>
#if defined(_WIN32)
#include <string.h>
#else
#include <strings.h>
#endif

#include "../core_memory/core_memory_private.h"
#include "../core_runtime/core_runtime_private.h"
#include "../filesystem/fs_private.h"
#include "../animation/xanim_private.h"
#include "../scripting/script_compile_private.h"
#include "../scripting/script_notify_private.h"
#include "../scripting/script_runtime_private.h"
#include "compat/coduo_ctype_compat.h"
#include "scripting/script_serialization.h"
#include "scripting/script_source_positions.h"
#include "scripting/script_variable.h"

#define SCRIPT_HUNK_ALLOC(size) Hunk_AllocInternal(size)
#define SCRIPT_HUNK_ALLOC_ALIGN(size, alignment) \
    Hunk_AllocAlignInternal((size), (alignment))
#define SCRIPT_HUNK_ALLOC_LOW(size) Hunk_AllocLowInternal(size)
#define SCRIPT_HUNK_ALLOC_TEMP_HIGH(size) \
    Hunk_AllocateTempMemoryHighInternal(size)
#define SCRIPT_HUNK_CLEAR_TEMP_HIGH() Hunk_ClearTempMemoryHigh()
#define SCRIPT_HUNK_COMMIT_TEMP() Hunk_CommitTempMemory()
#define SCRIPT_OUT_OF_MEMORY() Sys_OutOfMemory()
#define SCRIPT_HOST_ALLOCA(size) __builtin_alloca(size)
#if defined(_WIN32)
#define SCRIPT_STRICMP(left, right) _stricmp((left), (right))
#else
#define SCRIPT_STRICMP(left, right) strcasecmp((left), (right))
#endif
#define SCRIPT_ISALNUM_SIGNED_BYTE(value) \
    isalnum(coduo_ctype_signed_byte_arg(value))
#define SCRIPT_TOLOWER_SIGNED_BYTE(value) \
    tolower(coduo_ctype_signed_byte_arg(value))
#define SCRIPT_SPRINTF sprintf

#ifdef __cplusplus
}
#endif

#endif
