#include "q_math.h"

#include "compat/coduo_int32_bits.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#include "compat/coduo_x87emu.h"

/* Canonical Quake all-zero vector.  The original images retain identical
 * three-binary32 storage at CoDUOMP.exe 0x0058ec58,
 * uo_cgame_mp_x86.dll 0x30071f58, coduo_lnxded 0x080dd708, and in both game
 * modules.  Consumers take its address as a const vec3_t. */
const vec3_t vec3_origin = {0.0f, 0.0f, 0.0f};

/*
 * The four authoritative Windows Q_acos bodies are instruction-identical
 * apart from their image-local CRT and constant addresses:
 *
 *   CoDUOMP.exe                 0x004311c0
 *   uo_cgame_mp_x86.dll        0x30049320
 *   uo_ui_mp_x86.dll           0x400012f0
 *   uo_game_mp_x86.dll         0x20016370
 *
 * They narrow the CRT result to binary32 and compare it with binary32 +/-pi.
 * The two Linux bodies at coduo_lnxded 0x08066133 and game.mp.uo.i386.so RVA
 * 0x00039b03 use binary64 +/-pi instead.  Positive rounded pi takes a
 * different branch but returns the same bits.  The negative threshold cannot
 * distinguish any finite acos result because acos is nonnegative, and both
 * bodies pass an unordered NaN through.  The comparison lowering therefore
 * does not require behavior-specific recovered source.
 * The supporting PowerPC executable and game module retain the Windows graph
 * at PEF file offsets 0x000dd4b0 and 0x00017b50 respectively.
 */
float Q_acos(float value)
{
    const float pi = 3.1415927410125732f;
    const float angle = (float)acos((double)value);

    if (angle > pi || angle < -pi) {
        return pi;
    }
    return angle;
}

/*
 * The original out-of-line Quake helper is `_DotProduct`; `DotProduct` was
 * recovered as a function name in two trees even though the authoritative
 * game-module symbol and same-family naming retain the leading underscore.
 * All four Windows bodies below are instruction-identical and accumulate
 * Z, then Y, then X, leaving the result live in ST0:
 *
 *   CoDUOMP.exe                 0x004312e0
 *   uo_cgame_mp_x86.dll        0x30049440
 *   uo_ui_mp_x86.dll           0x40001410
 *   uo_game_mp_x86.dll         0x20016490
 *
 * Both Linux bodies instead accumulate X, then Y, then Z: coduo_lnxded
 * 0x08066333 and game.mp.uo.i386.so RVA 0x00039d64.  The source interface is
 * binary32 even though native i386 returns the unspilled x87 value in ST0.
 * None of the supporting PowerPC images retains a standalone traceback body.
 */
#if defined(WINDOWS_BEHAVIOR)
#if defined(__i386__) && defined(__GNUC__) && !defined(__clang__)
/* The original i386 body returns its binary32 expression without first
 * spilling ST0.  Keep that excess-precision return even in callers whose
 * surrounding build selects ISO excess-precision assignment semantics. */
__attribute__((optimize("-fexcess-precision=fast")))
#endif
float _DotProduct(const vec3_t left, const vec3_t right)
{
#if defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
    /* FLT_EVAL_METHOD=2 leaves this source-level float return live in ST0. */
    return (left[2] * right[2] + left[1] * right[1]) +
           left[0] * right[0];
#else
    return (float)(((double)left[2] * (double)right[2] +
                    (double)left[1] * (double)right[1]) +
                   (double)left[0] * (double)right[0]);
#endif
}
#else
float _DotProduct(const vec3_t left, const vec3_t right)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(left[0]), x87f_load_f32(right[0])),
                 x87f_mul(x87f_load_f32(left[1]), x87f_load_f32(right[1]))),
        x87f_mul(x87f_load_f32(left[2]), x87f_load_f32(right[2]))));
#elif defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
    /* FLT_EVAL_METHOD=2 leaves this source-level float return live in ST0. */
    return (left[0] * right[0] + left[1] * right[1]) +
           left[2] * right[2];
#else
    return (float)(((long double)left[0] * (long double)right[0] +
                    (long double)left[1] * (long double)right[1]) +
                   (long double)left[2] * (long double)right[2]);
#endif
}
#endif

/*
 * The four authoritative Windows _VectorLength bodies are instruction-
 * identical apart from the image-local square-root target.  They accumulate
 * X^2 + Y^2 + Z^2 under the Windows PC=53 policy, pass the live ST0 value to
 * the CRT square root, and explicitly narrow its result to binary32:
 *
 *   CoDUOMP.exe                 0x00431400
 *   uo_cgame_mp_x86.dll        0x30049560
 *   uo_ui_mp_x86.dll           0x40001530
 *   uo_game_mp_x86.dll         0x200165b0
 *
 * The Linux bodies at coduo_lnxded 0x08066531 and game.mp.uo.i386.so RVA
 * 0x00039f6f use the same X,Y,Z fold under PC=64, store that sum to binary64,
 * call the platform double square root, and narrow to binary32.  A PC=53 x87
 * sum is exactly representable by the same binary64 carrier, so this boundary
 * does not create a distinct Windows computation.
 * None of the supporting PowerPC images retains a standalone traceback body.
 */
float _VectorLength(const vec3_t vector)
{
    double squaredLength;

#if EMULATE_X87
    squaredLength = x87f_store_f64(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(vector[0]),
                          x87f_load_f32(vector[0])),
                 x87f_mul(x87f_load_f32(vector[1]),
                          x87f_load_f32(vector[1]))),
        x87f_mul(x87f_load_f32(vector[2]), x87f_load_f32(vector[2]))));
#elif (defined(__i386__) || defined(__x86_64__)) && \
      (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds %1\n\t"
                         "fmuls %1\n\t"
                         "flds %2\n\t"
                         "fmuls %2\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds %3\n\t"
                         "fmuls %3\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstpl %0"
                         : "=m"(squaredLength)
                         : "m"(vector[0]), "m"(vector[1]), "m"(vector[2])
                         : "st", "st(1)", "memory");
#else
    squaredLength = (double)(
        ((long double)vector[0] * (long double)vector[0] +
         (long double)vector[1] * (long double)vector[1]) +
        (long double)vector[2] * (long double)vector[2]);
#endif
    return (float)sqrt(squaredLength);
}

/*
 * The four Windows bodies are instruction-identical: CoDUOMP.exe 0x004318a0,
 * cgame 0x30049a00, UI 0x400019d0, and game 0x20016a50.  Linux
 * coduo_lnxded 0x08066ae7 and game module RVA 0x0003a5c3 retain the same two
 * comparisons and binary32 return.  The Linux and Mac symbols establish
 * VectorMax as the canonical name; VectorMaxComponent and
 * VectorMaxComponent3 were reconstruction-local spellings.
 *
 * Each comparison selects the right operand when unordered.  A negated
 * greater-than-or-equal preserves that NaN behavior and the left-operand
 * choice for equal finite values and signed zero.
 */
float VectorMax(const vec3_t vector)
{
    const float maximum =
        !(vector[0] >= vector[1]) ? vector[1] : vector[0];

    return !(maximum >= vector[2]) ? vector[2] : maximum;
}

/*
 * The four authoritative Windows vector-distance clusters are instruction-
 * identical after normalizing their local square-root target:
 *
 *                              3D       3D squared  2D       2D squared
 *   CoDUOMP.exe                0x431450 0x4314a0    0x4314e0 0x431520
 *   uo_cgame_mp_x86.dll       0x300495b0 0x30049600 0x30049640 0x30049680
 *   uo_ui_mp_x86.dll          0x40001580 0x400015d0 0x40001610 0x40001650
 *   uo_game_mp_x86.dll        0x20016600 0x20016650 0x20016690 0x200166d0
 *
 * Windows computes and stores first-second deltas, then folds Z,Y,X or Y,X
 * under PC=53.  The square-root variants feed the live sum to _CIsqrt and
 * narrow its result to binary32.  The squared variants return the live ST0
 * value through the source-level binary32 interface.
 *
 * Linux computes and stores second-first deltas, then folds X,Y,Z or X,Y
 * under PC=64.  Its square-root variants store the complete sum directly to
 * binary64 for sqrt(double), then narrow the result to binary32.  The engine
 * bodies are at 0x0806657b, 0x080665e0, 0x08066631, and 0x0806667b; the game
 * module bodies are at RVAs 0x00039fc9, 0x0003a03e, 0x0003a08f, and
 * 0x0003a0e9.  Only the three-term 3D folds require distinct source bodies;
 * the two-term 2D forms are normalized below.
 */
#if defined(WINDOWS_BEHAVIOR)
float VectorDistance(const vec3_t first, const vec3_t second)
{
    const float differenceX = first[0] - second[0];
    const float differenceY = first[1] - second[1];
    const float differenceZ = first[2] - second[2];

#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    float result;

    __asm__ __volatile__("flds %[z]\n\t"
                         "fmuls %[z]\n\t"
                         "flds %[y]\n\t"
                         "fmuls %[y]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds %[x]\n\t"
                         "fmuls %[x]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fsqrt\n\t"
                         "fstps %[result]"
                         : [result] "=m"(result)
                         : [x] "m"(differenceX), [y] "m"(differenceY),
                           [z] "m"(differenceZ)
                         : "st", "st(1)", "memory");
    return result;
#else
    const double squaredDistance =
        ((double)differenceZ * (double)differenceZ +
         (double)differenceY * (double)differenceY) +
        (double)differenceX * (double)differenceX;

    return (float)sqrt(squaredDistance);
#endif
}

#if defined(__i386__) && defined(__GNUC__) && !defined(__clang__)
/* The original i386 body returns its binary32 expression without first
 * spilling ST0.  This local compiler setting prevents an added return-boundary
 * binary32 store/reload in client and UI builds. */
__attribute__((optimize("-fexcess-precision=fast")))
#endif
float VectorDistanceSquared(const vec3_t first, const vec3_t second)
{
    const float differenceX = first[0] - second[0];
    const float differenceY = first[1] - second[1];
    const float differenceZ = first[2] - second[2];

#if defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
    /* FLT_EVAL_METHOD=2 retains the original unspilled ST0 return. */
    return (differenceZ * differenceZ + differenceY * differenceY) +
           differenceX * differenceX;
#else
    return (float)(((double)differenceZ * (double)differenceZ +
                    (double)differenceY * (double)differenceY) +
                   (double)differenceX * (double)differenceX);
#endif
}

#else
float VectorDistance(const vec3_t first, const vec3_t second)
{
#if EMULATE_X87
    const float differenceX = x87f_store_f32(
        x87f_sub(x87f_load_f32(second[0]), x87f_load_f32(first[0])));
    const float differenceY = x87f_store_f32(
        x87f_sub(x87f_load_f32(second[1]), x87f_load_f32(first[1])));
    const float differenceZ = x87f_store_f32(
        x87f_sub(x87f_load_f32(second[2]), x87f_load_f32(first[2])));
    const x87f squaredDistance = x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(differenceX),
                          x87f_load_f32(differenceX)),
                 x87f_mul(x87f_load_f32(differenceY),
                          x87f_load_f32(differenceY))),
        x87f_mul(x87f_load_f32(differenceZ),
                 x87f_load_f32(differenceZ)));

    return (float)sqrt(x87f_store_f64(squaredDistance));
#elif (defined(__i386__) || defined(__x86_64__)) && \
      (defined(__GNUC__) || defined(__clang__))
    float differenceX;
    float differenceY;
    float differenceZ;
    double squaredDistance;

    __asm__ __volatile__("flds %[secondX]\n\t"
                         "fsubs %[firstX]\n\t"
                         "fstps %[x]\n\t"
                         "flds %[secondY]\n\t"
                         "fsubs %[firstY]\n\t"
                         "fstps %[y]\n\t"
                         "flds %[secondZ]\n\t"
                         "fsubs %[firstZ]\n\t"
                         "fstps %[z]\n\t"
                         "flds %[x]\n\t"
                         "fmuls %[x]\n\t"
                         "flds %[y]\n\t"
                         "fmuls %[y]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds %[z]\n\t"
                         "fmuls %[z]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstpl %[sum]"
                         : [x] "=&m"(differenceX),
                           [y] "=&m"(differenceY),
                           [z] "=&m"(differenceZ),
                           [sum] "=m"(squaredDistance)
                         : [firstX] "m"(first[0]),
                           [firstY] "m"(first[1]),
                           [firstZ] "m"(first[2]),
                           [secondX] "m"(second[0]),
                           [secondY] "m"(second[1]),
                           [secondZ] "m"(second[2])
                         : "st", "st(1)", "memory");
    return (float)sqrt(squaredDistance);
#else
    const float differenceX = second[0] - first[0];
    const float differenceY = second[1] - first[1];
    const float differenceZ = second[2] - first[2];
    const double squaredDistance = (double)(
        ((long double)differenceX * (long double)differenceX +
         (long double)differenceY * (long double)differenceY) +
        (long double)differenceZ * (long double)differenceZ);

    return (float)sqrt(squaredDistance);
#endif
}

float VectorDistanceSquared(const vec3_t first, const vec3_t second)
{
#if EMULATE_X87
    const float differenceX = x87f_store_f32(
        x87f_sub(x87f_load_f32(second[0]), x87f_load_f32(first[0])));
    const float differenceY = x87f_store_f32(
        x87f_sub(x87f_load_f32(second[1]), x87f_load_f32(first[1])));
    const float differenceZ = x87f_store_f32(
        x87f_sub(x87f_load_f32(second[2]), x87f_load_f32(first[2])));

    return x87f_store_f32(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(differenceX),
                          x87f_load_f32(differenceX)),
                 x87f_mul(x87f_load_f32(differenceY),
                          x87f_load_f32(differenceY))),
        x87f_mul(x87f_load_f32(differenceZ),
                 x87f_load_f32(differenceZ))));
#elif defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
    const float differenceX = second[0] - first[0];
    const float differenceY = second[1] - first[1];
    const float differenceZ = second[2] - first[2];

    /* FLT_EVAL_METHOD=2 retains the original unspilled ST0 return. */
    return (differenceX * differenceX + differenceY * differenceY) +
           differenceZ * differenceZ;
#else
    const float differenceX = second[0] - first[0];
    const float differenceY = second[1] - first[1];
    const float differenceZ = second[2] - first[2];

    return (float)(((long double)differenceX * (long double)differenceX +
                    (long double)differenceY * (long double)differenceY) +
                   (long double)differenceZ * (long double)differenceZ);
#endif
}

#endif

/*
 * The 2D pair has the same computation on both platforms. Windows reverses
 * each stored delta and commutes the two squared terms; negation disappears on
 * squaring and a two-term addition has no reassociation. The selected x87
 * backend still supplies PC=53 or PC=64 before the common return boundary.
 */
float VectorDistance2D(const vec3_t first, const vec3_t second)
{
#if EMULATE_X87
    const float differenceX = x87f_store_f32(
        x87f_sub(x87f_load_f32(first[0]), x87f_load_f32(second[0])));
    const float differenceY = x87f_store_f32(
        x87f_sub(x87f_load_f32(first[1]), x87f_load_f32(second[1])));
    const x87f squaredDistance = x87f_add(
        x87f_mul(x87f_load_f32(differenceX),
                 x87f_load_f32(differenceX)),
        x87f_mul(x87f_load_f32(differenceY),
                 x87f_load_f32(differenceY)));

    return (float)sqrt(x87f_store_f64(squaredDistance));
#else
    volatile float differenceX = first[0] - second[0];
    volatile float differenceY = first[1] - second[1];
    const double squaredDistance = (double)(
        (long double)differenceX * (long double)differenceX +
        (long double)differenceY * (long double)differenceY);

    return (float)sqrt(squaredDistance);
#endif
}

#if defined(__i386__) && defined(__GNUC__) && !defined(__clang__)
/* Preserve the original live-ST0 return for the two-lane body. */
__attribute__((optimize("-fexcess-precision=fast")))
#endif
float VectorDistanceSquared2D(const vec3_t first, const vec3_t second)
{
#if EMULATE_X87
    const float differenceX = x87f_store_f32(
        x87f_sub(x87f_load_f32(first[0]), x87f_load_f32(second[0])));
    const float differenceY = x87f_store_f32(
        x87f_sub(x87f_load_f32(first[1]), x87f_load_f32(second[1])));

    return x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(differenceX),
                 x87f_load_f32(differenceX)),
        x87f_mul(x87f_load_f32(differenceY),
                 x87f_load_f32(differenceY))));
#else
    volatile float differenceX = first[0] - second[0];
    volatile float differenceY = first[1] - second[1];

    return (float)((long double)differenceX * (long double)differenceX +
                   (long double)differenceY * (long double)differenceY);
#endif
}

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
int32_t Q_log2(int32_t value)
{
    uint32_t shifted = coduo_int32_bits(value);
    uint32_t result = 0;

    while ((shifted = coduo_int32_sar_bits(shifted, 1U)) != 0U) {
        result += 1U;
    }

    return coduo_int32_from_bits(result);
}

/*
 * The clamp thresholds and lower-before-upper branch order agree in all six
 * authoritative bodies.  The four Windows bodies return ClampChar through AL
 * and ClampShort through AX.  Both Linux bodies happen to sign-extend the
 * result across EAX, but the i386 System V ABI leaves the upper EAX bits
 * unspecified for signed-char and signed-short returns; this is compiler
 * output variation, not a different source signature:
 *
 *   CoDUOMP.exe                 0x00431200, 0x00431210
 *   uo_cgame_mp_x86.dll        0x30049360, 0x30049370
 *   uo_ui_mp_x86.dll           0x40001330, 0x40001340
 *   uo_game_mp_x86.dll         0x200163b0, 0x200163c0
 *   coduo_lnxded               0x0806618e, 0x080661be
 *   game.mp.uo.i386.so         0x00039b6e, 0x00039b9e
 *
 * The supporting Mac client ClampChar at PEF file offset 0x000dd450 performs
 * the same comparisons and signed-byte in-range conversion.
 */
int8_t ClampChar(int32_t value)
{
    if (value < INT8_MIN) {
        return INT8_MIN;
    }
    if (value > INT8_MAX) {
        return INT8_MAX;
    }
    return (int8_t)value;
}

int16_t ClampShort(int32_t value)
{
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    return (int16_t)value;
}

/*
 * The retained original bodies agree on lane order and operation width:
 *
 *   CoDUOMP.exe                 0x00431300, 0x00431320, 0x00431340
 *   uo_cgame_mp_x86.dll        0x30049460, 0x30049480, 0x300494a0
 *   uo_ui_mp_x86.dll           0x40001430, 0x40001450, 0x40001470
 *   uo_game_mp_x86.dll         0x200164b0, 0x200164d0, 0x200164f0
 *   coduo_lnxded               0x08066366, 0x080663aa, 0x080663ee
 *   game.mp.uo.i386.so         0x00039d97, 0x00039ddb, 0x00039e1f
 *
 * Add and subtract load two binary32 operands, perform one operation, then
 * immediately store binary32 for each lane.  The copy bodies move three raw
 * 32-bit words, in ascending lane order; copying through a local word preserves
 * both the bit payload and the original behavior when vectors alias by a lane.
 */
void _VectorSubtract(const vec3_t first, const vec3_t second, vec3_t result)
{
    result[0] = first[0] - second[0];
    result[1] = first[1] - second[1];
    result[2] = first[2] - second[2];
}

void _VectorAdd(const vec3_t first, const vec3_t second, vec3_t result)
{
    result[0] = first[0] + second[0];
    result[1] = first[1] + second[1];
    result[2] = first[2] + second[2];
}

void _VectorCopy(const vec3_t source, vec3_t destination)
{
    uint32_t component;

    memcpy(&component, &source[0], sizeof(component));
    memcpy(&destination[0], &component, sizeof(component));
    memcpy(&component, &source[1], sizeof(component));
    memcpy(&destination[1], &component, sizeof(component));
    memcpy(&component, &source[2], sizeof(component));
    memcpy(&destination[2], &component, sizeof(component));
}

/*
 * The retained original bodies perform three independent binary32 products
 * and stores in ascending lane order:
 *
 *   CoDUOMP.exe                 0x00431360
 *   uo_cgame_mp_x86.dll        0x300494c0
 *   uo_ui_mp_x86.dll           0x40001490
 *   uo_game_mp_x86.dll         0x20016510
 *   coduo_lnxded               0x0806641d
 *   game.mp.uo.i386.so         0x00039e4e
 *
 * Windows loads scale first while Linux loads the vector lane first.  That is
 * only a commutation of each single multiplication; the operation and its
 * binary32 store boundary are identical.  The selected x87 backend supplies
 * the platform precision policy without duplicating this source function.
 */
void _VectorScale(const vec3_t vector, float scale, vec3_t result)
{
#if EMULATE_X87
    result[0] = x87f_store_f32(
        x87f_mul(x87f_load_f32(vector[0]), x87f_load_f32(scale)));
    result[1] = x87f_store_f32(
        x87f_mul(x87f_load_f32(vector[1]), x87f_load_f32(scale)));
    result[2] = x87f_store_f32(
        x87f_mul(x87f_load_f32(vector[2]), x87f_load_f32(scale)));
#else
    result[0] = vector[0] * scale;
    result[1] = vector[1] * scale;
    result[2] = vector[2] * scale;
#endif
}

/*
 * The corresponding multiply-add bodies use one unspilled multiply/add chain
 * per lane.  Windows emits FADD from memory while Linux loads start and uses
 * FADDP, but both retain the product live and store only the completed sum to
 * binary32.  The addresses are the `_VectorScale` addresses above plus 0x20
 * on Windows, and 0x38 in both Linux binaries (0x08066455 and RVA 0x00039e86).
 * The x87 backend, not a second function body, selects PC=53 or PC=64.
 */
void _VectorMA(const vec3_t start, float scale, const vec3_t direction,
               vec3_t result)
{
#if EMULATE_X87
    result[0] = x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(scale), x87f_load_f32(direction[0])),
        x87f_load_f32(start[0])));
    result[1] = x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(scale), x87f_load_f32(direction[1])),
        x87f_load_f32(start[1])));
    result[2] = x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(scale), x87f_load_f32(direction[2])),
        x87f_load_f32(start[2])));
#else
    result[0] = start[0] + scale * direction[0];
    result[1] = start[1] + scale * direction[1];
    result[2] = start[2] + scale * direction[2];
#endif
}

/*
 * The original Windows bodies enter the x87 domain for each lane:
 *
 *   CoDUOMP.exe                 0x00431850
 *   uo_cgame_mp_x86.dll        0x300499b0
 *   uo_ui_mp_x86.dll           0x40001980
 *   uo_game_mp_x86.dll         0x20016a00
 *
 * Each lane is loaded with FLD, negated with FCHS, and stored with FSTP.  The
 * supporting Mac engine and game-module bodies likewise use LFS/FNEG/STFS.
 * Linux lowers the same unary negation to an integer sign-bit XOR.  This is a
 * compiler-lowering distinction, not a different source computation.
 */
void VectorInverse(vec3_t vector)
{
    vector[0] = -vector[0];
    vector[1] = -vector[1];
    vector[2] = -vector[2];
}

/*
 * All six authoritative bodies perform four separate binary32 multiplies and
 * stores in ascending lane order.  Windows loads scale first while Linux loads
 * the input lane first; that commutation of a single product does not change
 * the recovered computation.
 */
void Vector4Scale(const vec4_t input, float scale, vec4_t output)
{
#if EMULATE_X87
    output[0] = x87f_store_f32(
        x87f_mul(x87f_load_f32(input[0]), x87f_load_f32(scale)));
    output[1] = x87f_store_f32(
        x87f_mul(x87f_load_f32(input[1]), x87f_load_f32(scale)));
    output[2] = x87f_store_f32(
        x87f_mul(x87f_load_f32(input[2]), x87f_load_f32(scale)));
    output[3] = x87f_store_f32(
        x87f_mul(x87f_load_f32(input[3]), x87f_load_f32(scale)));
#else
    output[0] = input[0] * scale;
    output[1] = input[1] * scale;
    output[2] = input[2] * scale;
    output[3] = input[3] * scale;
#endif
}

/*
 * The authoritative bodies agree on both sentinel bit patterns and reverse
 * lane store order:
 *
 *   CoDUOMP.exe                 0x00433f30
 *   uo_cgame_mp_x86.dll        0x3004c090
 *   uo_ui_mp_x86.dll           0x400040a0
 *   uo_game_mp_x86.dll         0x200190e0
 *   coduo_lnxded               0x0806a164
 *   game.mp.uo.i386.so         0x0003df31
 *
 * The stores are raw dword moves in every authoritative body.  The supporting
 * Mac engine body at PEF file offset 0x000db9d0 loads and stores the same two
 * binary32 values in the same lane order.
 */
void ClearBounds(vec3_t mins, vec3_t maxs)
{
    const uint32_t clearMins = UINT32_C(0x48800000);
    const uint32_t clearMaxs = UINT32_C(0xc8800000);

    memcpy(&mins[2], &clearMins, sizeof(clearMins));
    memcpy(&mins[1], &clearMins, sizeof(clearMins));
    memcpy(&mins[0], &clearMins, sizeof(clearMins));
    memcpy(&maxs[2], &clearMaxs, sizeof(clearMaxs));
    memcpy(&maxs[1], &clearMaxs, sizeof(clearMaxs));
    memcpy(&maxs[0], &clearMaxs, sizeof(clearMaxs));
}

/*
 * All six authoritative AddPointToBounds bodies visit lanes 0, 1, 2, test the
 * minimum before the maximum, and copy an accepted component as a raw dword:
 *
 *   CoDUOMP.exe                 0x00433f50
 *   uo_cgame_mp_x86.dll        0x3004c0b0
 *   uo_ui_mp_x86.dll           0x400040c0
 *   uo_game_mp_x86.dll         0x20019100
 *   coduo_lnxded               0x0806a19f
 *   game.mp.uo.i386.so         0x0003df7b
 *
 * Windows uses FCOMP while Linux uses FUCOMPP.  Both classify an unordered
 * comparison as false and leave the bound unchanged; that lowering difference
 * does not require platform-specific source.  Re-reading point for each
 * comparison and copy preserves the original behavior for overlapping arrays.
 */
void AddPointToBounds(const vec3_t point, vec3_t mins, vec3_t maxs)
{
    for (int32_t lane = 0; lane < 3; ++lane) {
        uint32_t component;

#if EMULATE_X87
        if (x87f_lt(x87f_load_f32(point[lane]),
                    x87f_load_f32(mins[lane]))) {
#else
        if (point[lane] < mins[lane]) {
#endif
            memcpy(&component, &point[lane], sizeof(component));
            memcpy(&mins[lane], &component, sizeof(component));
        }
#if EMULATE_X87
        if (x87f_lt(x87f_load_f32(maxs[lane]),
                    x87f_load_f32(point[lane]))) {
#else
        if (maxs[lane] < point[lane]) {
#endif
            memcpy(&component, &point[lane], sizeof(component));
            memcpy(&maxs[lane], &component, sizeof(component));
        }
    }
}

/*
 * The four-vector body is ExpandBounds, with source bounds first and the
 * destination bounds second.  That identity and argument order are explicit
 * in both Linux ABIs and the supporting Mac engine symbol/body; the Windows
 * bodies are instruction-identical at:
 *
 *   CoDUOMP.exe                 0x00433fc0
 *   uo_cgame_mp_x86.dll        0x3004c120
 *   uo_ui_mp_x86.dll           0x40004130
 *   uo_game_mp_x86.dll         0x20019170
 *   coduo_lnxded               0x0806a288
 *   game.mp.uo.i386.so         0x0003e064
 *
 * Lanes, minimum-before-maximum order, raw accepted-value copies, and overlap
 * behavior match AddPointToBounds above.  The Windows FCOMP/Linux FUCOMPP
 * lowering does not change the decisions or stored results.
 */
void ExpandBounds(const vec3_t addMins, const vec3_t addMaxs,
                  vec3_t mins, vec3_t maxs)
{
    for (int32_t lane = 0; lane < 3; ++lane) {
        uint32_t component;

#if EMULATE_X87
        if (x87f_lt(x87f_load_f32(addMins[lane]),
                    x87f_load_f32(mins[lane]))) {
#else
        if (addMins[lane] < mins[lane]) {
#endif
            memcpy(&component, &addMins[lane], sizeof(component));
            memcpy(&mins[lane], &component, sizeof(component));
        }
#if EMULATE_X87
        if (x87f_lt(x87f_load_f32(maxs[lane]),
                    x87f_load_f32(addMaxs[lane]))) {
#else
        if (maxs[lane] < addMaxs[lane]) {
#endif
            memcpy(&component, &addMaxs[lane], sizeof(component));
            memcpy(&maxs[lane], &component, sizeof(component));
        }
    }
}

/*
 * The retained original bodies agree on matrix layout and lane order:
 *
 *   CoDUOMP.exe                 0x00434030, 0x00434060
 *   uo_cgame_mp_x86.dll        0x3004c190, 0x3004c1c0
 *   uo_ui_mp_x86.dll           0x400041a0, 0x400041d0
 *   uo_game_mp_x86.dll         0x200191e0, 0x20019210
 *   coduo_lnxded               0x0806a371, 0x0806a3df
 *   game.mp.uo.i386.so         0x0003e14d, 0x0003e1bb
 *
 * AxisClear writes binary32 1.0 on the diagonal and positive zero elsewhere.
 * AxisCopy moves nine raw 32-bit words in ascending lane order.  The supporting
 * Mac cgame retains both functions (PEF file offsets 0x15020 and 0x14fc0); Mac
 * UI retains AxisClear (0x570), and the Mac engine retains AxisCopy (0xdb830).
 * Their PowerPC bodies use scalar/pair floating loads and stores but perform no
 * arithmetic.  They produce the same result for disjoint or identical matrices;
 * Linux and Windows establish the ascending raw-word behavior used here.
 */
void AxisClear(axis_t axis)
{
    for (int32_t row = 0; row < 3; ++row) {
        for (int32_t lane = 0; lane < 3; ++lane) {
            const uint32_t component =
                row == lane ? UINT32_C(0x3f800000) : UINT32_C(0);

            memcpy(&axis[row][lane], &component, sizeof(component));
        }
    }
}

void AxisCopy(const axis_t input, axis_t output)
{
    uint32_t component;

    for (int32_t row = 0; row < 3; ++row) {
        for (int32_t lane = 0; lane < 3; ++lane) {
            memcpy(&component, &input[row][lane], sizeof(component));
            memcpy(&output[row][lane], &component, sizeof(component));
        }
    }
}

/*
 * All six authoritative bodies transpose the same nine raw 32-bit words in
 * output row-major order:
 *
 *   CoDUOMP.exe                 0x00432b20
 *   uo_cgame_mp_x86.dll        0x3004ac80
 *   uo_ui_mp_x86.dll           0x40002c50
 *   uo_game_mp_x86.dll         0x20017cd0
 *   coduo_lnxded               0x0806881d
 *   game.mp.uo.i386.so         0x0003c429
 *
 * Each source word is loaded immediately before its destination store.  Keep
 * that progressive order: it preserves the original bit transport, including
 * NaN payloads, and the original overwrite behavior when the matrices alias.
 * The Linux game symbol and the supporting Mac module symbols retain the name
 * MatrixTranspose.
 */
void MatrixTranspose(const axis_t input, axis_t output)
{
    for (int32_t row = 0; row < 3; ++row) {
        for (int32_t lane = 0; lane < 3; ++lane) {
            uint32_t component;

            memcpy(&component, &input[lane][row], sizeof(component));
            memcpy(&output[row][lane], &component, sizeof(component));
        }
    }
}

/*
 * All six authoritative bodies negate the quaternion vector part in ascending
 * lane order and copy the scalar part as one raw binary32 word:
 *
 *   CoDUOMP.exe                 0x00433640
 *   uo_cgame_mp_x86.dll        0x3004b7a0
 *   uo_ui_mp_x86.dll           0x40003770
 *   uo_game_mp_x86.dll         0x200187f0
 *   coduo_lnxded               0x0806972e
 *   game.mp.uo.i386.so         0x0003d381
 *
 * The Linux game dynamic symbol directly preserves the name QuatInverse.
 * Base Quake 3 contains no QuatInverse or QuatConjugate routine that would
 * override that original-symbol evidence; the engine's recovered
 * QuatConjugate name was therefore normalized here.
 *
 * Windows enters the x87 domain separately for lanes 0..2 through
 * FLD/FCHS/FSTP. Linux lowers the same unary negation to integer sign-bit XOR.
 * That lowering distinction does not require separate recovered source.
 */
void QuatInverse(const vec4_t input, vec4_t output)
{
    output[0] = -input[0];
    output[1] = -input[1];
    output[2] = -input[2];
    memcpy(&output[3], &input[3], sizeof(output[3]));
}
