/*
 * NOT_FROM_ORIGINAL_SOURCE: MSVC implementation of the reconstructed CRT math
 * entry points.
 *
 * The sources in this directory reproduce the original Microsoft CRT routines
 * on compilers whose long double is not the x87 80-bit type (GCC/Clang
 * -mfpmath=387 or the SoftFloat emulation). MSVC itself is the reference
 * implementation: its 32-bit code generation evaluates these calls with the
 * same x87 instructions and the same CRT entry points the retail modules used,
 * and its long double is only 64 bits wide, so the reconstructed bodies cannot
 * be compiled there.
 *
 * This file is selected instead of coduo_libm_asin.c, coduo_libm_ftol.c,
 * coduo_libm_sincos.c and coduo_libm_sqrt.c when the CMake MSVC build targets
 * the game module. It is not part of the Makefile builds.
 */
#include "coduo_libm.h"

#include <math.h>

int64_t CoduoLibm_FloatToInt64(double value)
{
    /* MSVC x86 lowers this to the truncating _ftol2 conversion, which is the
     * routine the reconstructed body reproduces. */
    return (int64_t)value;
}

int32_t CoduoLibm_FloatToInt32(double value)
{
    return (int32_t)(int64_t)value;
}

double CoduoLibm_Sqrt(double value)
{
    return sqrt(value);
}

double CoduoLibm_SqrtGlibc(double value)
{
    return sqrt(value);
}

double CoduoLibm_Asin(double value)
{
    return asin(value);
}

double CoduoLibm_Acos(double value)
{
    return acos(value);
}

double CoduoLibm_Sin(double value)
{
    return sin(value);
}

double CoduoLibm_Cos(double value)
{
    return cos(value);
}
