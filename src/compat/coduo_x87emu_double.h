/*
 * coduo_x87emu_double.h — fast native-double backend for the x87f_* shim.
 *
 * A GOOD APPROXIMATION of x87, not a bit-exact reproduction. Selected with
 * EMULATE_X87_BACKEND == EMU_X87_DOUBLE (see coduo_x87emu.h). Intended for
 * platforms with no x87 unit where the SoftFloat backend's ~50-100x overhead is
 * too costly (notably NEON / Apple Silicon).
 *
 * How it approximates x87:
 *   x87 evaluates every op at a 64-bit mantissa (80-bit register) and rounds to
 *   32 bits only at store points. This backend evaluates every op at a 53-bit
 *   mantissa (native double) and rounds to 32 bits at exactly the same store
 *   points (x87f_store_f32). So the structure is identical — round every op,
 *   round again at each float store — with a 53-bit accumulator instead of
 *   64-bit. Products of float-derived values are exact in both (a 24x24-bit
 *   product is 48 bits, which fits in both mantissas), so the two diverge only
 *   when an accumulator needs mantissa bits 54-64. That is rare, and only
 *   changes a result when it lands exactly on a collision decision boundary
 *   (grazing hit / coplanar point). Use analysis/tools/x87_backend_flip_rate.sh
 *   to measure how often that actually happens on the map corpus — the metric
 *   is diverging traces vs the exact SoftFloat backend, and the goal is ~0.
 *
 * NOT bit-exact: this backend will NOT pass the byte-for-byte parity suites
 * (capsule_parity_check / collision_parity_check). That is expected — measure
 * it with the flip-rate tool, not the exactness suites. For true fidelity
 * (demo replay, stock-server interop) use EMU_X87_SOFTFLOAT.
 */
#ifndef CODUO_X87EMU_DOUBLE_H
#define CODUO_X87EMU_DOUBLE_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/* The x87 "register" value is a plain double in this backend. */
typedef double x87f;

/*
 * One-time FP-environment setup. x87 never flushes denormals; SSE/NEON can, if
 * some runtime or a -ffast-math startup left flush-to-zero enabled. Clear it so
 * tiny-magnitude collision math (plane distances near zero) is not flushed,
 * which would be a divergence source the approximation does not intend. Rounding
 * stays the default (round-to-nearest-even), matching x87's steady-state RC.
 * Idempotent; call once at startup (as the SoftFloat backend's x87f_init is).
 */
static inline void x87f_init(void)
{
#if defined(__aarch64__)
    uint64_t fpcr;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
    fpcr &= ~((uint64_t)1 << 24); /* FZ: flush-to-zero for single/double */
    __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
#elif (defined(__x86_64__) || defined(__i386__)) && defined(__SSE__)
    unsigned int mxcsr = __builtin_ia32_stmxcsr();
    mxcsr &= ~((1u << 15) | (1u << 6)); /* clear FTZ (bit 15) and DAZ (bit 6) */
    __builtin_ia32_ldmxcsr(mxcsr);
#endif
}

/* flds — widen a 32-bit float to the working precision (exact). */
static inline x87f x87f_load_f32(float v) { return (double)v; }
/* fildl — load a 32-bit int. */
static inline x87f x87f_load_i32(int32_t v) { return (double)v; }
/* fldl — a 64-bit double is already the working precision (exact). */
static inline x87f x87f_load_f64(double v) { return v; }

/* Arithmetic: native double, one rounding to 53-bit mantissa per op. */
static inline x87f x87f_mul(x87f a, x87f b) { return a * b; }
static inline x87f x87f_add(x87f a, x87f b) { return a + b; }
static inline x87f x87f_sub(x87f a, x87f b) { return a - b; }
static inline x87f x87f_div(x87f a, x87f b) { return a / b; }
static inline x87f x87f_sqrt(x87f a) { return sqrt(a); }
static inline x87f x87f_neg(x87f a) { return -a; }
static inline x87f x87f_abs(x87f a) { return fabs(a); }

/* Comparisons: native `<`/`<=`/`==` are quiet in the default FP environment
 * (unordered -> false), matching fucompp's non-signaling semantics that the
 * reconstructed negated-form branch conditions rely on. */
static inline bool x87f_lt(x87f a, x87f b) { return a < b; }
static inline bool x87f_le(x87f a, x87f b) { return a <= b; }
static inline bool x87f_eq(x87f a, x87f b) { return a == b; }
static inline bool x87f_lt_signaling(x87f a, x87f b) { return a < b; }
static inline bool x87f_le_signaling(x87f a, x87f b) { return a <= b; }
static inline bool x87f_eq_signaling(x87f a, x87f b) { return a == b; }

/* fstps — the store point x87 rounds a value to a 32-bit float slot. This is
 * the rounding that governs collision-classification behavior; it is the same
 * in this backend as in the exact one, only the value being rounded carries
 * 53-bit rather than 64-bit intermediate precision. */
static inline float x87f_store_f32(x87f a) { return (float)a; }
/* fstpl — the working precision is already double; no-op. */
static inline double x87f_store_f64(x87f a) { return a; }

/* fistp under round-to-nearest (default control word). */
static inline int32_t x87f_store_i32(x87f a) { return (int32_t)rint(a); }
/* fistp under the truncate control word (Q_rint's toward-zero bracket). */
static inline int32_t x87f_store_i32_trunc(x87f a) { return (int32_t)trunc(a); }

#endif /* CODUO_X87EMU_DOUBLE_H */
