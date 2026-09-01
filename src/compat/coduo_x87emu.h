/*
 * coduo_x87emu.h — thin x87 shim over Berkeley SoftFloat (EMULATE_X87 builds)
 *
 * Reproduces the original engine's x87 floating-point behaviour in software on
 * platforms with no x87 unit (arm64 / Apple Silicon), so the reconstructed
 * collision/geometry math is bit-faithful to the reference build. Only compiled
 * when EMULATE_X87 is set (see coduo_fp_platform.h); native-x87 builds may
 * include this header, but it exposes no emulation API in that mode.
 *
 * Design (see docs/platform-discrepancies/fp-emulation-design.md):
 * - The 80-bit register value is SoftFloat's extFloat80_t (portable; arm64 has
 *   no native 80-bit type).
 * - The FPU runs the selected original platform invariant: Windows precision
 *   control 53-bit or Linux precision control 64-bit, both round-to-nearest.
 *   The explicit *_trunc conversions model temporary truncate-control-word
 *   sites. There is no mutable control-word state machine.
 * - Each primitive mirrors one x87 instruction so a reconstructed function can
 *   be transcribed op-for-op from the original instruction stream, and its
 *   rounding matches (notably: a store to a 32-bit float slot rounds via
 *   x87f_store_f32, exactly as x87 `fstps` does).
 *
 * This is a HEADER-ONLY inline shim: it introduces no non-x87-source wrappers
 * into the canonical code (the canonical file is a separate variant), and lets
 * the compiler inline everything so the emulated variant reads naturally.
 */
#ifndef CODUO_X87EMU_H
#define CODUO_X87EMU_H

#include "coduo_fp_platform.h"

#if EMULATE_X87

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "x87 emulation requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif defined(WINDOWS_BEHAVIOR)
/* SoftFloat's 64 setting means binary64/53-significand x87 precision. */
#define CODUO_X87_EMULATED_ROUNDING_PRECISION 64
#else
/* SoftFloat's 80 setting means the full 64-significand x87 precision. */
#define CODUO_X87_EMULATED_ROUNDING_PRECISION 80
#endif

/*
 * Backend selection. EMULATE_X87 chooses the *emulated source form* (the
 * op-for-op x87 transcription in the .c files); this second axis chooses what
 * the x87f_* primitives are backed by:
 *   EMU_X87_SOFTFLOAT — Berkeley SoftFloat, 80-bit, bit-exact to x87 (default;
 *                       use for demo replay / stock-server interop).
 *   EMU_X87_DOUBLE    — native double, 53-bit, a fast approximation (NEON /
 *                       Apple Silicon, when SoftFloat's overhead is the barrier;
 *                       NOT bit-exact — measure with x87_backend_flip_rate.sh).
 * The reconstructed .c source is identical for both; only this header changes.
 */
#define EMU_X87_SOFTFLOAT 0
#define EMU_X87_DOUBLE    1
#ifndef EMULATE_X87_BACKEND
#define EMULATE_X87_BACKEND EMU_X87_SOFTFLOAT
#endif

#include <stdbool.h>
#include <stdint.h>

#if EMULATE_X87_BACKEND == EMU_X87_DOUBLE

#include "coduo_x87emu_double.h"

#elif EMULATE_X87_BACKEND == EMU_X87_SOFTFLOAT

/*
 * These must match the flags SoftFloat's own build used (vendor/softfloat/
 * build/coduo-x87), or the shared struct layouts and declarations disagree
 * between this consumer and the library, silently corrupting values.
 *   - LITTLEENDIAN selects the field order of struct extFloat80M
 *     ({signif; signExp} vs {signExp; signif}); both our targets (x86-64,
 *     arm64) are little-endian, and the library is built LITTLEENDIAN.
 *   - SOFTFLOAT_FAST_INT64 selects the struct-form (not array-M) extF80 API.
 */
#ifndef LITTLEENDIAN
#define LITTLEENDIAN 1
#endif
#ifndef SOFTFLOAT_FAST_INT64
#define SOFTFLOAT_FAST_INT64 1
#endif
#include "softfloat.h"

/* NOT_FROM_ORIGINAL_SOURCE: establish the compile-time-selected original x87
 * control-word invariant before an emulated arithmetic/store operation.  This
 * is intentionally idempotent and thread-local in the SoftFloat backend. */
static inline void coduo_x87f_prepare(void)
{
    softfloat_roundingMode = softfloat_round_near_even; /* x87 RC = nearest */
    extF80_roundingPrecision = CODUO_X87_EMULATED_ROUNDING_PRECISION;
}

static inline void x87f_init(void)
{
    coduo_x87f_prepare();
    softfloat_exceptionFlags = 0;
}

/* The 80-bit x87 register value. */
typedef extFloat80_t x87f;

/* flds  — load a 32-bit float into an 80-bit register (exact widening). */
static inline x87f x87f_load_f32(float v)
{
    union { float f; uint32_t u; } c;
    c.f = v;
    return f32_to_extF80((float32_t){ c.u });
}

/* fildl — load a 32-bit int into an 80-bit register. */
static inline x87f x87f_load_i32(int32_t v) { return i32_to_extF80(v); }

/* fldl — load a 64-bit double into an 80-bit register (exact widening). */
static inline x87f x87f_load_f64(double v)
{
    union { double d; uint64_t u; } c;
    c.d = v;
    return f64_to_extF80((float64_t){ c.u });
}

/* fmul (…s/…p) — 80-bit multiply. */
static inline x87f x87f_mul(x87f a, x87f b)
{
    coduo_x87f_prepare();
    return extF80_mul(a, b);
}
/* fadd — 80-bit add. */
static inline x87f x87f_add(x87f a, x87f b)
{
    coduo_x87f_prepare();
    return extF80_add(a, b);
}
/* fsub — 80-bit subtract (a - b). */
static inline x87f x87f_sub(x87f a, x87f b)
{
    coduo_x87f_prepare();
    return extF80_sub(a, b);
}
/* fdiv — 80-bit divide (a / b). */
static inline x87f x87f_div(x87f a, x87f b)
{
    coduo_x87f_prepare();
    return extF80_div(a, b);
}
/* fsqrt — 80-bit square root. */
static inline x87f x87f_sqrt(x87f a)
{
    coduo_x87f_prepare();
    return extF80_sqrt(a);
}
/* fchs — negate. */
static inline x87f x87f_neg(x87f a)
{
    a.signExp ^= 0x8000u;
    return a;
}
/* fabs — clear the sign bit (80-bit absolute value). */
static inline x87f x87f_abs(x87f a)
{
    a.signExp &= (uint16_t)0x7fffu;
    return a;
}

/* fucompp-style comparisons on 80-bit registers. fucompp is the QUIET compare
 * (does not signal on a quiet NaN), and returns unordered for any NaN — the
 * _quiet SoftFloat variants match that, and return false for unordered, which
 * is how the engine's post-fucompp branch conditions treat the epsilon tests
 * (|diff| < eps is taken only when strictly-below and ordered). Used for x87
 * epsilon tests the original computes in extended precision (the source writes
 * these as `long double`, which is NOT 80-bit on arm64 — route through the shim
 * so the width matches x87). */
static inline bool x87f_lt(x87f a, x87f b) { return extF80_lt_quiet(a, b); }
static inline bool x87f_le(x87f a, x87f b) { return extF80_le_quiet(a, b); }
/* extF80_eq is already the quiet form in SoftFloat (extF80_eq_signaling is the
 * signaling one), matching fucompp's non-signaling equality. */
static inline bool x87f_eq(x87f a, x87f b) { return extF80_eq(a, b); }
/* FCOMP-style signaling comparisons used at specifically proved Windows
 * sites.  Unlike FUCOM, these raise invalid for a quiet NaN as well. */
static inline bool x87f_lt_signaling(x87f a, x87f b)
{
    return extF80_lt(a, b);
}
static inline bool x87f_le_signaling(x87f a, x87f b)
{
    return extF80_le(a, b);
}
static inline bool x87f_eq_signaling(x87f a, x87f b)
{
    return extF80_eq_signaling(a, b);
}

/* fstps — round an 80-bit register to a 32-bit float and return it. This is the
 * rounding that x87 performs when storing to a `float` memory slot, and the one
 * that governs the collision-classification faithfulness. Rounds to nearest. */
static inline float x87f_store_f32(x87f a)
{
    coduo_x87f_prepare();
    float32_t r = extF80_to_f32(a);
    union { uint32_t u; float f; } c;
    c.u = r.v;
    return c.f;
}

/* fstpl — round an 80-bit register to a 64-bit double and return it (e.g. when
 * the original stores the x87 value as a double, as the argument passed to the
 * CRT sqrt(double)). Note: sqrt itself is IEEE-mandated correctly-rounded, so
 * native sqrt(double) is bit-portable across libms — the emulated variant may
 * call native sqrt on the x87f_store_f64 result and reload with x87f_load_f64. */
static inline double x87f_store_f64(x87f a)
{
    coduo_x87f_prepare();
    float64_t r = extF80_to_f64(a);
    union { uint64_t u; double d; } c;
    c.u = r.v;
    return c.d;
}

/* fistp with the default (round-to-nearest) control word. */
static inline int32_t x87f_store_i32(x87f a)
{
    return extF80_to_i32(a, softfloat_round_near_even, false);
}

/* fistp under the truncate control word (Q_rint's save-set-restore bracket:
 * RC = toward zero). The one non-nearest rounding site. */
static inline int32_t x87f_store_i32_trunc(x87f a)
{
    return extF80_to_i32(a, softfloat_round_minMag, false);
}

/* NOT_FROM_ORIGINAL_SOURCE: fistpq under truncate semantics. Windows `_ftol2`
 * call sites consume the low dword of this signed-qword conversion, which
 * differs from fistpl on invalid or out-of-range inputs. */
static inline int64_t x87f_store_i64_trunc(x87f a)
{
    return extF80_to_i64(a, softfloat_round_minMag, false);
}

#else
#error "EMULATE_X87_BACKEND must be EMU_X87_SOFTFLOAT or EMU_X87_DOUBLE"
#endif /* EMULATE_X87_BACKEND */

#endif /* EMULATE_X87 */

#endif /* CODUO_X87EMU_H */
