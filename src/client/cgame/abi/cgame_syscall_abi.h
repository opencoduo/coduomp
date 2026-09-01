#ifndef CODUO_CGAME_SYSCALL_ABI_H
#define CODUO_CGAME_SYSCALL_ABI_H

#include "cgame_module_abi.h"

enum {
    CGAME_SYSCALL_FIRST = 0,
    CGAME_SYSCALL_LAST_OBSERVED = 250,
    CGAME_SYSCALL_SURFACE_SIZE = 251
};

/* Every original syscall argument occupies one i386 dword. Pointers are module
 * addresses and float values are transported as their raw IEEE-754 bit pattern
 * unless a command contract explicitly says the module performs a conversion. */
typedef cgame_syscall32_t cgame_engine_dispatch32_t;

#endif
