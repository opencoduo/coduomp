#include "floating_point_boundary.h"

#if defined(_WIN32)

#include <float.h>
#include <stdint.h>

#if UINTPTR_MAX != UINT32_MAX && \
    (!defined(__x86_64__) || defined(__SSE_MATH__) || !defined(__FLT_EVAL_METHOD__) || __FLT_EVAL_METHOD__ != 2)
#error "The recovered Windows engine requires i686 or x86-64 GCC x87 code generation"
#endif

#if UINTPTR_MAX == UINT64_MAX
enum {
    CODUOMP_X87_PRECISION_MASK = 0x0300,
    CODUOMP_X87_DOUBLE_PRECISION = 0x0200
};
#endif

/* NOT_FROM_ORIGINAL_SOURCE: the retail entry path pushes 1 at 0x0056fb55
 * before calling the CRT initializer at 0x0056eeee. Its pre-init pointer
 * 0x005c2fc8 selects 0x0056fc37, so the nonzero branch at 0x0056fc4b calls
 * 0x005783fc. That helper passes _PC_53/_MCW_PC to the control-word path whose
 * FLDCW is at 0x0057e212. The _fpreset path repeats the same operation at
 * 0x0057e043..0x0057e052, but has no direct caller or stored function-pointer
 * reference in the executable. MinGW starts i686 x87 in 64-bit extended
 * precision, so the recovered engine explicitly selects the retail process
 * mode at its startup boundary. */
void coduomp_restore_retail_x87_precision(void)
{
#if UINTPTR_MAX == UINT32_MAX
    (void)_controlfp(_PC_53, _MCW_PC);
#else
    uint16_t controlWord;

    /* NOT_FROM_ORIGINAL_SOURCE: x86-64 MinGW can still emit x87 operations,
     * but the 64-bit CRT does not expose a reliable precision-control path.
     * Apply the same 53-bit x87 control-word state directly for that port. */
    __asm__ volatile("fnstcw %0" : "=m"(controlWord));
    controlWord = (uint16_t)((controlWord & (uint16_t)~CODUOMP_X87_PRECISION_MASK) | CODUOMP_X87_DOUBLE_PRECISION);
    __asm__ volatile("fldcw %0" : : "m"(controlWord));
#endif
}

#endif
