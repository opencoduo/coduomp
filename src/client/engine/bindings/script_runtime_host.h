#ifndef CODUOMP_SCRIPT_RUNTIME_HOST_H
#define CODUOMP_SCRIPT_RUNTIME_HOST_H

#include "qcommon/hunk.h"
#include "client/engine/math/vector_math.h"
#include "client/engine/platform/crt_boundary.h"
#include "qcommon/q_string.h"
#include "client/engine/scripting/script_compile.h"
#include "client/engine/scripting/script_runtime.h"

fileData_t *FS_GetDataForFile(const char *type, const char *name, const char *extension);
#define SCRIPT_HUNK_ALLOC(size) Hunk_AllocInternal(size)
#define SCRIPT_HUNK_ALLOC_ALIGN(size, alignment) Hunk_AllocAlignInternal((size), (alignment))
#define SCRIPT_HUNK_ALLOC_LOW(size) Hunk_AllocLowInternal(size)
#define SCRIPT_HUNK_ALLOC_TEMP_HIGH(size) Hunk_AllocateTempMemoryHighInternal(size)
#define SCRIPT_HUNK_CLEAR_TEMP_HIGH() Hunk_ClearTempMemoryHigh()
#define SCRIPT_HUNK_COMMIT_TEMP() Hunk_CommitTempMemory()
#define SCRIPT_OUT_OF_MEMORY() Sys_OutOfMemory()
#define SCRIPT_HOST_ALLOCA(size) CODUOMP_ALLOCA(size)
#define SCRIPT_STRICMP(left, right) coduo_crt_stricmp((left), (right))
#define SCRIPT_ISALNUM_SIGNED_BYTE(value) coduo_crt_isalnum(value)
#define SCRIPT_TOLOWER_SIGNED_BYTE(value) coduo_crt_tolower((int8_t)(uint8_t)(value))
#define SCRIPT_SPRINTF coduomp_crt_sprintf

#endif
