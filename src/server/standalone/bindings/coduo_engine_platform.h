#ifndef CODUO_ENGINE_PLATFORM_H
#define CODUO_ENGINE_PLATFORM_H

/*
 * Platform contract for recovered coduo_lnxded source.
 *
 * The oracle host is a Linux i386 binary whose floating-point-heavy paths use
 * x87 semantics. Strict builds should fail on targets that would evaluate
 * those expressions with different native precision or fast-math assumptions.
 *
 * Local coordinator builds may define CODUO_ENGINE_ALLOW_NON_X87_FLOAT to keep
 * compile checks usable on 64-bit Darwin. That mode proves syntax and linkage
 * only; it is not numeric equivalence evidence.
 */

#include <float.h>

#if !defined(CODUO_ENGINE_ALLOW_NON_X87_FLOAT) && !defined(__i386__) && !defined(__i486__) && !defined(__i586__) && !defined(__i686__) && \
    !defined(__x86_64__) && !defined(_M_IX86) && !defined(_M_X64)
#error "coduo_lnxded recovery requires a native x86/x87 target"
#endif

#if !defined(CODUO_ENGINE_ALLOW_NON_X87_FLOAT) && defined(__SSE_MATH__)
#error "coduo_lnxded recovery requires x87 float evaluation, not SSE math"
#endif

#if defined(__FAST_MATH__)
#error "coduo_lnxded recovery must not be built with fast-math"
#endif

#if defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__
#error "coduo_lnxded recovery requires normal IEEE exceptional float behavior"
#endif

#if !defined(CODUO_ENGINE_ALLOW_NON_X87_FLOAT) && defined(FLT_EVAL_METHOD) && FLT_EVAL_METHOD != 2
#error "coduo_lnxded recovery requires x87-style FLT_EVAL_METHOD == 2"
#endif

#if !defined(CODUO_ENGINE_ALLOW_NON_X87_FLOAT) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(long double) > sizeof(double), "coduo_lnxded recovery requires extended long double");
#endif

#endif
