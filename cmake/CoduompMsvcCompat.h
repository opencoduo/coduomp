/*
 * NOT_FROM_ORIGINAL_SOURCE: MSVC compatibility aliases for the C library names
 * the recovered sources use. The authoritative retail Windows build and the
 * maintained MinGW targets get these names from their C library headers; the
 * Microsoft CRT spells them with a leading underscore and no POSIX wrapper.
 *
 * The CMake MSVC project force-includes this header (see CMakeLists.txt), so it
 * never affects the Makefile builds and adds no code of its own.
 */
#ifndef CODUOMP_MSVC_COMPAT_H
#define CODUOMP_MSVC_COMPAT_H

#if defined(_MSC_VER)

#include <io.h>
#include <malloc.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef strcasecmp
#define strcasecmp _stricmp
#endif

#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

#ifndef strdup
#define strdup _strdup
#endif

#ifndef isatty
#define isatty _isatty
#endif

#ifndef fileno
#define fileno _fileno
#endif

/* MSVC's <sys/stat.h> exposes the _S_IF* file-type bits but not the POSIX
 * S_IS* predicates the shared filesystem sources use. */
#include <sys/stat.h>
#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISCHR
#define S_ISCHR(mode) (((mode) & _S_IFMT) == _S_IFCHR)
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(mode) (((mode) & _S_IFMT) == _S_IFIFO)
#endif
#ifndef S_ISBLK
#define S_ISBLK(mode) 0
#endif

/* The recovered sources spell these GCC builtins directly. The Microsoft CRT
 * exposes the same operations; all of them are binary64-wide because MSVC has
 * no 80-bit long double. */
#ifndef __builtin_alloca
#define __builtin_alloca(size) _alloca(size)
#endif

#ifndef __builtin_fabsl
#define __builtin_fabsl fabsl
#endif

#ifndef __builtin_sinl
#define __builtin_sinl sinl
#endif

#ifndef __builtin_cosl
#define __builtin_cosl cosl
#endif

#ifndef __builtin_sqrtl
#define __builtin_sqrtl sqrtl
#endif

#ifndef __builtin_unreachable
#define __builtin_unreachable() __assume(0)
#endif

#if defined(__cplusplus)
/* MSVC supports __typeof__ only in C; the C++ effects units use the same
 * renderer headers, where decltype provides the equivalent type. */
#ifndef __typeof__
#define __typeof__(expression) decltype(expression)
#endif
#endif

#endif /* _MSC_VER */

#endif /* CODUOMP_MSVC_COMPAT_H */
