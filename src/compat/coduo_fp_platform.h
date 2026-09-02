/*
 * coduo_fp_platform.h — floating-point platform policy (shared, whole-program)
 *
 * Defines the single source-level feature flag EMULATE_X87. This is the header
 * half of the FP-faithfulness policy; the build half is build-mk/x87-policy.mk.
 * See docs/components/server/fp-faithfulness.md (current -mfpmath=387
 * policy) and docs/platform-discrepancies/fp-emulation-design.md (the
 * emulation plan this flag anchors).
 *
 * -----------------------------------------------------------------------------
 * Why this exists
 * -----------------------------------------------------------------------------
 * The original engine did all float math on the x87 FPU (80-bit intermediates,
 * rounding to 32-bit only at stores). A modern build using SSE (x86-64 default)
 * or NEON (arm64) rounds differently, which pervasively changes collision /
 * patch-winding geometry and can flip discrete gameplay decisions. On x86 the
 * fix is to make the compiler emit x87 (`-mfpmath=387`, GCC). Platforms with no
 * x87 unit (arm64) can only be made faithful by a software x87 layer — the
 * "emulated" variants selected by EMULATE_X87.
 *
 * -----------------------------------------------------------------------------
 * EMULATE_X87 — the single feature flag
 * -----------------------------------------------------------------------------
 *   undefined / 0 : compile the canonical source, run on native FP.
 *   1             : compile the emulated (software-x87) variants; their
 *                   canonical originals turn off. If -mfpmath=387 is also passed
 *                   it becomes moot (emulation is host-FP-independent).
 *
 * .c code selects variants by testing EMULATE_X87 and nothing else. It is a
 * source-level command verb, distinct from -mfpmath=387, which is a compiler
 * behavior switch (part of "the tool": gcc configured to emit x87).
 *
 * -----------------------------------------------------------------------------
 * Autodetection (source-only; an invoker -DEMULATE_X87=... always wins)
 * -----------------------------------------------------------------------------
 *   i386                          -> 0   native x87
 *   x86-64 with x87 codegen       -> 0   gcc -mfpmath=387: faithful
 *   x86-64 with SSE math          -> #error (the ONLY error case): the intent is
 *                                     ambiguous, so make the human choose.
 *   arm64 / any other arch        -> 1   no x87 possible -> emulate (never errors)
 *
 * The x86-64 #error fires only when building x86-64 by hand without -mfpmath=387
 * (or with clang). The project Makefiles always pass -mfpmath=387 on x86-64, so
 * the normal build path never hits it.
 * -----------------------------------------------------------------------------
 */
#ifndef CODUO_FP_PLATFORM_H
#define CODUO_FP_PLATFORM_H

#include <float.h> /* __FLT_EVAL_METHOD__ / FLT_EVAL_METHOD */

#ifndef EMULATE_X87

/*
 * Detection intermediate — DO NOT use in .c source (EMULATE_X87 is the sole
 * feature flag). #undef'd at the end of this block so it cannot leak.
 *
 * GCC exposes no positive "__X87_MATH__" macro; it defines the negative
 * __SSE_MATH__ when SSE is the FP math unit (undocumented but stable; glibc
 * headers rely on it). __FLT_EVAL_METHOD__ == 2 is the standard ISO C positive
 * assertion that float expressions evaluate in 80-bit (the x87 effect); the
 * arch gate is required because on non-x86 a value of 0/2 does not distinguish
 * x87-vs-SSE. On x86-64 the ABI default is SSE, so !defined(__SSE_MATH__) means
 * -mfpmath=387 was chosen.
 */
#if defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
#define CODUO_FP_DETECT_X87_CODEGEN 1
#elif defined(__x86_64__) || defined(__amd64__)
#if !defined(__SSE_MATH__) && defined(__FLT_EVAL_METHOD__) && (__FLT_EVAL_METHOD__ == 2)
#define CODUO_FP_DETECT_X87_CODEGEN 1
#else
#define CODUO_FP_DETECT_X87_CODEGEN 0
#endif
#else
/* Non-x86 (arm64, …): no x87 unit exists. */
#define CODUO_FP_DETECT_X87_CODEGEN 0
#endif

#if defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
/* 32-bit x86 is natively all-x87: faithful, no emulation. */
#define EMULATE_X87 0
#elif defined(__x86_64__) || defined(__amd64__)
#if CODUO_FP_DETECT_X87_CODEGEN
/* gcc -mfpmath=387 confirmed: faithful, no emulation. */
#define EMULATE_X87 0
#else
#error \
    "x86-64 build without x87 codegen: float math (collision geometry) will deviate from the reference. Build with gcc -mfpmath=387 for a faithful build, or choose explicitly: -DEMULATE_X87=1 (software x87, faithful anywhere) or -DEMULATE_X87=0 (native SSE, NOT bit-exact). See docs/fp-faithfulness.md."
#endif
#else
/* arm64 and any other non-x86 arch: no x87 unit, so emulate to be faithful.
 * (Disable deliberately with -DEMULATE_X87=0 only for e.g. differential tests.) */
#define EMULATE_X87 1
#endif

#undef CODUO_FP_DETECT_X87_CODEGEN /* detection intermediate: never leaks */

#endif /* !EMULATE_X87 */

#endif /* CODUO_FP_PLATFORM_H */
