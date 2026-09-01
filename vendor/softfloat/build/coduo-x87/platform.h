/*----------------------------------------------------------------------------
| CoDUO x87-emulation build configuration for Berkeley SoftFloat.
|
| Portable across x86-64 and arm64 (Apple Silicon): SoftFloat's arithmetic is
| pure software, so the only host assumptions here are little-endian (true on
| both x86 and ARM) and GCC/Clang builtins (__builtin_clz, __int128 — available
| on both). The *semantics* target is Intel x87 (SPECIALIZE_TYPE = 8086, set in
| the Makefile), independent of the host architecture.
*----------------------------------------------------------------------------*/
#define LITTLEENDIAN 1

#ifdef __GNUC_STDC_INLINE__
#define INLINE inline
#else
#define INLINE extern inline
#endif

#define SOFTFLOAT_BUILTIN_CLZ 1
#define SOFTFLOAT_INTRINSIC_INT128 1
#include "opts-GCC.h"
