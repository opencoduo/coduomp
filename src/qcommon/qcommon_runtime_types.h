#ifndef QCOMMON_RUNTIME_TYPES_H
#define QCOMMON_RUNTIME_TYPES_H

#include "q_shared_types.h"

#include <stddef.h>
#include <stdint.h>

enum {
    CVAR_HASH_BUCKET_COUNT = 256,
    CVAR_MAX_COUNT = 2048
};

/* Engine-owned cvar object. The Windows executable and Linux server agree on
 * the complete 0x2c-byte i386 layout, including the sorted and hash links.
 * Linux Cvar_Get at 0x08073114 allocates the same 2048-record table and
 * Cvar_FindVar walks the 256-bucket hash table through hashNext +0x28. */
typedef struct cvar_s {
    char *name;
    char *string;
    char *resetString;
    char *latchedString;
    uint32_t flags;
    qboolean modified;
    int32_t modificationCount;
    float value;
    int32_t integer;
    struct cvar_s *next;
    struct cvar_s *hashNext;
} cvar_t;

/* Registered command list node. Cmd_AddCommand in the Linux server at
 * 0x08060272 and the corresponding Windows client body both walk next +0x00,
 * compare name +0x04, and call/store function +0x08. */
typedef void (*xcommand_t)(void);

typedef struct cmd_function_s {
    struct cmd_function_s *next;
    char *name;
    xcommand_t function;
} cmd_function_t;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(cvar_t) == 0x2c, "cvar_t size mismatch");
_Static_assert(offsetof(cvar_t, name) == 0x00, "cvar_t.name offset mismatch");
_Static_assert(offsetof(cvar_t, string) == 0x04, "cvar_t.string offset mismatch");
_Static_assert(offsetof(cvar_t, resetString) == 0x08, "cvar_t.resetString offset mismatch");
_Static_assert(offsetof(cvar_t, latchedString) == 0x0c, "cvar_t.latchedString offset mismatch");
_Static_assert(offsetof(cvar_t, flags) == 0x10, "cvar_t.flags offset mismatch");
_Static_assert(offsetof(cvar_t, modified) == 0x14, "cvar_t.modified offset mismatch");
_Static_assert(offsetof(cvar_t, modificationCount) == 0x18, "cvar_t.modificationCount offset mismatch");
_Static_assert(offsetof(cvar_t, value) == 0x1c, "cvar_t.value offset mismatch");
_Static_assert(offsetof(cvar_t, integer) == 0x20, "cvar_t.integer offset mismatch");
_Static_assert(offsetof(cvar_t, next) == 0x24, "cvar_t.next offset mismatch");
_Static_assert(offsetof(cvar_t, hashNext) == 0x28, "cvar_t.hashNext offset mismatch");
_Static_assert(sizeof(cmd_function_t) == 0x0c, "cmd_function_t size mismatch");
_Static_assert(offsetof(cmd_function_t, next) == 0x00, "cmd_function_t.next offset mismatch");
_Static_assert(offsetof(cmd_function_t, name) == 0x04, "cmd_function_t.name offset mismatch");
_Static_assert(offsetof(cmd_function_t, function) == 0x08, "cmd_function_t.function offset mismatch");
#endif

#endif
