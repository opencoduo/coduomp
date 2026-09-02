/*
 * Native-x87 compiler adapters for operation graphs that modern GCC does not
 * preserve from ordinary C on x86-64.
 */
#ifndef CODUO_NATIVE_X87_H
#define CODUO_NATIVE_X87_H

#include <limits.h>
#include <math.h>
#include <stdint.h>

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || \
    defined(_M_X64)
#define CODUO_ARCH_HAS_X87 1
#else
#define CODUO_ARCH_HAS_X87 0
#endif

#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))

#define CODUO_X87_TRUNCATE_ROUNDING_BITS UINT16_C(3072)

typedef struct {
    uint16_t savedControlWord;
    uint16_t truncateControlWord;
} coduo_x87_truncation_control_t;

/*
 * NOT_FROM_ORIGINAL_SOURCE: these expression adapters keep the caller's
 * arithmetic live in x87 until the truncating store. The register-qualified
 * extended carrier is intentional: an always-inline function parameter still
 * acquires a compiler-generated TBYTE spill at -O0 on supported GCC, while the
 * statement expression does not. Typed operands and literal suffixes at call
 * sites determine each original load width and operation graph; named
 * float/double constant objects are used where the retail graph loads memory.
 */
#define CODUO_X87_TRUNCATE_I32_FIRST(controlPointer, expression)              \
    __extension__({                                                           \
        coduo_x87_truncation_control_t *coduo_x87_control_ =                   \
            (controlPointer);                                                 \
        register long double coduo_x87_value_ = (expression);                 \
        int32_t coduo_x87_result_;                                            \
        __asm__ __volatile__(                                                 \
            "fnstcw %[saved]\n\t"                                             \
            "movzwl %[saved], %%eax\n\t"                                      \
            "orw $%c[truncateBits], %%ax\n\t"                                 \
            "movw %%ax, %[truncate]\n\t"                                      \
            "fldcw %[truncate]\n\t"                                           \
            "fistpl %[result]\n\t"                                            \
            "fldcw %[saved]"                                                  \
            : [saved] "=m"(coduo_x87_control_->savedControlWord),             \
              [truncate] "=m"(coduo_x87_control_->truncateControlWord),       \
              [result] "=m"(coduo_x87_result_)                               \
            : "t"(coduo_x87_value_),                                          \
              [truncateBits] "i"(CODUO_X87_TRUNCATE_ROUNDING_BITS)            \
            : "eax", "cc", "memory", "st");                                  \
        coduo_x87_result_;                                                     \
    })

#define CODUO_X87_TRUNCATE_I32_NEXT(controlPointer, expression)               \
    __extension__({                                                           \
        const coduo_x87_truncation_control_t *coduo_x87_control_ =             \
            (controlPointer);                                                 \
        register long double coduo_x87_value_ = (expression);                 \
        int32_t coduo_x87_result_;                                            \
        __asm__ __volatile__(                                                 \
            "fldcw %[truncate]\n\t"                                           \
            "fistpl %[result]\n\t"                                            \
            "fldcw %[saved]"                                                  \
            : [result] "=m"(coduo_x87_result_)                               \
            : "t"(coduo_x87_value_),                                          \
              [saved] "m"(coduo_x87_control_->savedControlWord),              \
              [truncate] "m"(coduo_x87_control_->truncateControlWord)         \
            : "memory", "st");                                                \
        coduo_x87_result_;                                                     \
    })

#define CODUO_X87_TRUNCATE_I32(expression)                                    \
    __extension__({                                                           \
        coduo_x87_truncation_control_t coduo_x87_direct_control_;              \
        CODUO_X87_TRUNCATE_I32_FIRST(                                         \
            &coduo_x87_direct_control_, (expression));                        \
    })

#if defined(LINUX_BEHAVIOR)
#define CODUO_X87_SCALE_F32_INSTRUCTIONS                                      \
            "flds %[scale]\n\t"                                               \
            "fmulp %%st, %%st(1)\n\t"
#define CODUO_X87_SCALE_F32_CLOBBERS "st", "st(1)"
#else
#define CODUO_X87_SCALE_F32_INSTRUCTIONS "fmuls %[scale]\n\t"
#define CODUO_X87_SCALE_F32_CLOBBERS "st"
#endif

/*
 * NOT_FROM_ORIGINAL_SOURCE: caller expression adapter for the XAnim length
 * millisecond paths.  It preserves the returned value in x87, applies the
 * original binary32 scale operand, and performs the original truncating
 * integer store.  Windows used FMUL m32; Linux used FLD m32 followed by
 * FMULP, so the behavior policy also retains that operation graph.
 */
#define CODUO_X87_SCALE_F32_TRUNCATE_I32(expression, scaleExpression)         \
    __extension__({                                                           \
        register long double coduo_x87_scale_value_ = (expression);           \
        coduo_x87_truncation_control_t coduo_x87_scale_control_;               \
        int32_t coduo_x87_scale_result_;                                      \
        __asm__ __volatile__(                                                 \
            CODUO_X87_SCALE_F32_INSTRUCTIONS                                  \
            "fnstcw %[saved]\n\t"                                             \
            "movzwl %[saved], %%eax\n\t"                                      \
            "orw $%c[truncateBits], %%ax\n\t"                                 \
            "movw %%ax, %[truncate]\n\t"                                     \
            "fldcw %[truncate]\n\t"                                          \
            "fistpl %[result]\n\t"                                           \
            "fldcw %[saved]"                                                 \
            : [saved] "=m"(coduo_x87_scale_control_.savedControlWord),        \
              [truncate] "=m"(                                               \
                  coduo_x87_scale_control_.truncateControlWord),              \
              [result] "=m"(coduo_x87_scale_result_)                          \
            : "t"(coduo_x87_scale_value_),                                    \
              [scale] "m"(scaleExpression),                                   \
              [truncateBits] "i"(CODUO_X87_TRUNCATE_ROUNDING_BITS)            \
            : "eax", "cc", "memory", CODUO_X87_SCALE_F32_CLOBBERS);         \
        coduo_x87_scale_result_;                                              \
    })

#define CODUO_X87_TRUNCATE_I16_FIRST(controlPointer, expression)              \
    __extension__({                                                           \
        coduo_x87_truncation_control_t *coduo_x87_control_ =                   \
            (controlPointer);                                                 \
        register long double coduo_x87_value_ = (expression);                 \
        int16_t coduo_x87_result_;                                            \
        __asm__ __volatile__(                                                 \
            "fnstcw %[saved]\n\t"                                             \
            "movzwl %[saved], %%eax\n\t"                                      \
            "orw $%c[truncateBits], %%ax\n\t"                                 \
            "movw %%ax, %[truncate]\n\t"                                      \
            "fldcw %[truncate]\n\t"                                           \
            "fistps %[result]\n\t"                                            \
            "fldcw %[saved]"                                                  \
            : [saved] "=m"(coduo_x87_control_->savedControlWord),             \
              [truncate] "=m"(coduo_x87_control_->truncateControlWord),       \
              [result] "=m"(coduo_x87_result_)                               \
            : "t"(coduo_x87_value_),                                          \
              [truncateBits] "i"(CODUO_X87_TRUNCATE_ROUNDING_BITS)            \
            : "eax", "cc", "memory", "st");                                  \
        coduo_x87_result_;                                                     \
    })

#define CODUO_X87_TRUNCATE_I16_NEXT(controlPointer, expression)               \
    __extension__({                                                           \
        const coduo_x87_truncation_control_t *coduo_x87_control_ =             \
            (controlPointer);                                                 \
        register long double coduo_x87_value_ = (expression);                 \
        int16_t coduo_x87_result_;                                            \
        __asm__ __volatile__(                                                 \
            "fldcw %[truncate]\n\t"                                           \
            "fistps %[result]\n\t"                                            \
            "fldcw %[saved]"                                                  \
            : [result] "=m"(coduo_x87_result_)                               \
            : "t"(coduo_x87_value_),                                          \
              [saved] "m"(coduo_x87_control_->savedControlWord),              \
              [truncate] "m"(coduo_x87_control_->truncateControlWord)         \
            : "memory", "st");                                                \
        coduo_x87_result_;                                                     \
    })

#define CODUO_X87_TRUNCATE_I16(expression)                                    \
    __extension__({                                                           \
        coduo_x87_truncation_control_t coduo_x87_direct_control_;              \
        CODUO_X87_TRUNCATE_I16_FIRST(                                         \
            &coduo_x87_direct_control_, (expression));                        \
    })

#define CODUO_X87_TRUNCATE_I64(expression)                                    \
    __extension__({                                                           \
        register long double coduo_x87_value_ = (expression);                 \
        uint16_t coduo_x87_saved_control_;                                    \
        uint16_t coduo_x87_truncate_control_;                                 \
        int64_t coduo_x87_result_;                                            \
        __asm__ __volatile__(                                                 \
            "fnstcw %[saved]\n\t"                                             \
            "movzwl %[saved], %%eax\n\t"                                      \
            "orw $%c[truncateBits], %%ax\n\t"                                 \
            "movw %%ax, %[truncate]\n\t"                                      \
            "fldcw %[truncate]\n\t"                                           \
            "fistpq %[result]\n\t"                                            \
            "fldcw %[saved]"                                                  \
            : [saved] "=m"(coduo_x87_saved_control_),                         \
              [truncate] "=m"(coduo_x87_truncate_control_),                   \
              [result] "=m"(coduo_x87_result_)                               \
            : "t"(coduo_x87_value_),                                          \
              [truncateBits] "i"(CODUO_X87_TRUNCATE_ROUNDING_BITS)            \
            : "eax", "cc", "memory", "st");                                  \
        coduo_x87_result_;                                                     \
    })

#endif

#if defined(__GNUC__) || defined(__clang__)
#define CODUO_X87_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define CODUO_X87_ALWAYS_INLINE inline
#endif

/* NOT_FROM_ORIGINAL_SOURCE: host adapter for original one-instruction
 * FSINCOS paths with binary32 input and output stores. Intel builds execute
 * the hardware instruction explicitly. The portable branch preserves the
 * original cosine-then-sine store order, including when outputs alias. */
static CODUO_X87_ALWAYS_INLINE void
coduo_x87_sincosf(float angle, float *sinOut, float *cosOut)
{
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds %2\n\t"
                         "fsincos\n\t"
                         "fstps %1\n\t"
                         "fstps %0"
                         : "=m"(*sinOut), "=m"(*cosOut)
                         : "m"(angle)
                         : "st");
#else
    float sine;
    float cosine;

#if defined(__APPLE__)
    __sincosf(angle, &sine, &cosine);
#elif defined(__GNUC__) || defined(__clang__)
    __builtin_sincosf(angle, &sine, &cosine);
#else
    sine = sinf(angle);
    cosine = cosf(angle);
#endif

    *cosOut = cosine;
    *sinOut = sine;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: binary64 companion for original one-instruction
 * FSINCOS paths. The two stores retain the original cosine-then-sine order. */
static CODUO_X87_ALWAYS_INLINE void
coduo_x87_sincos(double angle, double *sinOut, double *cosOut)
{
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("fldl %2\n\t"
                         "fsincos\n\t"
                         "fstpl %1\n\t"
                         "fstpl %0"
                         : "=m"(*sinOut), "=m"(*cosOut)
                         : "m"(angle)
                         : "st");
#else
    double sine;
    double cosine;

#if defined(__APPLE__)
    __sincos(angle, &sine, &cosine);
#elif defined(__GNUC__) || defined(__clang__)
    __builtin_sincos(angle, &sine, &cosine);
#else
    sine = sin(angle);
    cosine = cos(angle);
#endif

    *cosOut = cosine;
    *sinOut = sine;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit hardware FSIN boundary for an original
 * extended-precision operand that remains live in ST0. */
static CODUO_X87_ALWAYS_INLINE long double
coduo_x87_sinl(long double angle)
{
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("fsin" : "+t"(angle));
    return angle;
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_sinl(angle);
#else
    return sinl(angle);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit hardware FPTAN boundary. FPTAN pushes
 * 1.0 in ST0; the pop discards it and leaves tan(angle) as the result. */
static CODUO_X87_ALWAYS_INLINE long double
coduo_x87_tanl(long double angle)
{
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("fptan\n\t"
                         "fstp %%st(0)"
                         : "+t"(angle));
    return angle;
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_tanl(angle);
#else
    return tanl(angle);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit hardware FPATAN boundary. Loading x in
 * ST0 and y in ST1 produces atan2(y, x) while popping the denominator. */
static CODUO_X87_ALWAYS_INLINE long double
coduo_x87_atan2l(long double y, long double x)
{
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    long double result;

    __asm__ __volatile__("fpatan"
                         : "=t"(result)
                         : "0"(x), "u"(y)
                         : "st(1)");
    return result;
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_atan2l(y, x);
#else
    return atan2l(y, x);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit hardware FSQRT boundary for an original
 * extended-precision operand. The portable branch is not x87 emulation. */
static CODUO_X87_ALWAYS_INLINE long double
coduo_x87_sqrtl(long double value)
{
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("fsqrt" : "+t"(value));
    return value;
#else
    return sqrtl(value);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit FCOS plus binary32 store for original
 * paths whose input remains in extended precision. */
static CODUO_X87_ALWAYS_INLINE float
coduo_x87_fcos_to_f32(long double angle)
{
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    float result;

    __asm__ __volatile__("fldt %1\n\t"
                         "fcos\n\t"
                         "fstps %0"
                         : "=m"(result)
                         : "m"(angle)
                         : "st");
    return result;
#else
    return (float)cosl(angle);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: narrow spelling for original direct FISTP m32
 * sites. Native Intel builds obey the active x87 rounding mode. */
static CODUO_X87_ALWAYS_INLINE int32_t
coduo_x87_fistp_i32(long double value)
{
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    int32_t result;

    __asm__ __volatile__("fistpl %0" : "=m"(result) : "t"(value) : "st");
    return result;
#else
    long double rounded = rintl(value);

    if (!isfinite(rounded) || rounded > (long double)INT32_MAX ||
        rounded < (long double)INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)rounded;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: binary32-load companion for original
 * `FLD m32; FISTP m32` paths. Native Intel builds retain both operand widths
 * and obey the active x87 rounding mode. */
static CODUO_X87_ALWAYS_INLINE int32_t
coduo_x87_fistp_f32_i32(float value)
{
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    int32_t result;

    __asm__ __volatile__("flds %1\n\t"
                         "fistpl %0"
                         : "=m"(result)
                         : "m"(value)
                         : "st");
    return result;
#else
    return (int32_t)lrintf(value);
#endif
}

#undef CODUO_X87_ALWAYS_INLINE

#endif
