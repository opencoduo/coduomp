#include "q_math.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#include "compat/coduo_x87emu.h"

/*
 * Complete XSurface affine-transform cluster.
 *
 * The Windows client bodies are CoDUOMP.exe 0x0049e8e0..0x0049ea36.
 * The Linux dedicated bodies are coduo_lnxded 0x080c5086..0x080c5337.
 * They share signatures, DObjSkelMat layout, output-store order, and the same
 * transformations.  Windows uses compiler-selected lane-specific x87 fold
 * orders under PC=53; Linux folds every dot product in X,Y,Z order under
 * PC=64.  The complete platform bodies preserve that genuine arithmetic
 * distinction.  Each lane is stored to binary32 before the next lane starts.
 */

#if defined(WINDOWS_BEHAVIOR)

void XSurfaceTransformPoint43(const vec3_t point,
                              const DObjSkelMat *matrix,
                              vec3_t out)
{
#if EMULATE_X87
    out[0] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_add(
                x87f_mul(x87f_load_f32(point[2]),
                         x87f_load_f32(matrix->axis[2][0])),
                x87f_mul(x87f_load_f32(point[1]),
                         x87f_load_f32(matrix->axis[1][0]))),
            x87f_mul(x87f_load_f32(point[0]),
                     x87f_load_f32(matrix->axis[0][0]))),
        x87f_load_f32(matrix->origin[0])));
    out[1] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_add(
                x87f_mul(x87f_load_f32(point[0]),
                         x87f_load_f32(matrix->axis[0][1])),
                x87f_mul(x87f_load_f32(point[2]),
                         x87f_load_f32(matrix->axis[2][1]))),
            x87f_mul(x87f_load_f32(point[1]),
                     x87f_load_f32(matrix->axis[1][1]))),
        x87f_load_f32(matrix->origin[1])));
    out[2] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_add(
                x87f_mul(x87f_load_f32(point[0]),
                         x87f_load_f32(matrix->axis[0][2])),
                x87f_mul(x87f_load_f32(point[2]),
                         x87f_load_f32(matrix->axis[2][2]))),
            x87f_mul(x87f_load_f32(point[1]),
                     x87f_load_f32(matrix->axis[1][2]))),
        x87f_load_f32(matrix->origin[2])));
#else
    out[0] = (float)(
        (((long double)point[2] * matrix->axis[2][0] +
          (long double)point[1] * matrix->axis[1][0]) +
         (long double)point[0] * matrix->axis[0][0]) +
        (long double)matrix->origin[0]);
    out[1] = (float)(
        (((long double)point[0] * matrix->axis[0][1] +
          (long double)point[2] * matrix->axis[2][1]) +
         (long double)point[1] * matrix->axis[1][1]) +
        (long double)matrix->origin[1]);
    out[2] = (float)(
        (((long double)point[0] * matrix->axis[0][2] +
          (long double)point[2] * matrix->axis[2][2]) +
         (long double)point[1] * matrix->axis[1][2]) +
        (long double)matrix->origin[2]);
#endif
}

void XSurfaceAccumulateWeightedPoint43(
    const vec3_t point, float weight,
    const DObjSkelMat *matrix, vec3_t out)
{
#if EMULATE_X87
    out[0] = x87f_store_f32(x87f_add(
        x87f_mul(
            x87f_add(
                x87f_add(
                    x87f_add(
                        x87f_mul(x87f_load_f32(point[1]),
                                 x87f_load_f32(matrix->axis[1][0])),
                        x87f_mul(x87f_load_f32(point[2]),
                                 x87f_load_f32(matrix->axis[2][0]))),
                    x87f_mul(x87f_load_f32(point[0]),
                             x87f_load_f32(matrix->axis[0][0]))),
                x87f_load_f32(matrix->origin[0])),
            x87f_load_f32(weight)),
        x87f_load_f32(out[0])));
    out[1] = x87f_store_f32(x87f_add(
        x87f_mul(
            x87f_add(
                x87f_add(
                    x87f_add(
                        x87f_mul(x87f_load_f32(point[1]),
                                 x87f_load_f32(matrix->axis[1][1])),
                        x87f_mul(x87f_load_f32(point[2]),
                                 x87f_load_f32(matrix->axis[2][1]))),
                    x87f_mul(x87f_load_f32(point[0]),
                             x87f_load_f32(matrix->axis[0][1]))),
                x87f_load_f32(matrix->origin[1])),
            x87f_load_f32(weight)),
        x87f_load_f32(out[1])));
    out[2] = x87f_store_f32(x87f_add(
        x87f_mul(
            x87f_add(
                x87f_add(
                    x87f_add(
                        x87f_mul(x87f_load_f32(point[1]),
                                 x87f_load_f32(matrix->axis[1][2])),
                        x87f_mul(x87f_load_f32(point[2]),
                                 x87f_load_f32(matrix->axis[2][2]))),
                    x87f_mul(x87f_load_f32(point[0]),
                             x87f_load_f32(matrix->axis[0][2]))),
                x87f_load_f32(matrix->origin[2])),
            x87f_load_f32(weight)),
        x87f_load_f32(out[2])));
#else
    out[0] = (float)((
        (((long double)point[1] * matrix->axis[1][0] +
          (long double)point[2] * matrix->axis[2][0]) +
         (long double)point[0] * matrix->axis[0][0]) +
        (long double)matrix->origin[0]) * (long double)weight +
        (long double)out[0]);
    out[1] = (float)((
        (((long double)point[1] * matrix->axis[1][1] +
          (long double)point[2] * matrix->axis[2][1]) +
         (long double)point[0] * matrix->axis[0][1]) +
        (long double)matrix->origin[1]) * (long double)weight +
        (long double)out[1]);
    out[2] = (float)((
        (((long double)point[1] * matrix->axis[1][2] +
          (long double)point[2] * matrix->axis[2][2]) +
         (long double)point[0] * matrix->axis[0][2]) +
        (long double)matrix->origin[2]) * (long double)weight +
        (long double)out[2]);
#endif
}

void XSurfaceTransformNormal43(const vec3_t normal,
                               const DObjSkelMat *matrix,
                               vec3_t out)
{
#if EMULATE_X87
    out[0] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_mul(x87f_load_f32(normal[2]),
                     x87f_load_f32(matrix->axis[2][0])),
            x87f_mul(x87f_load_f32(normal[1]),
                     x87f_load_f32(matrix->axis[1][0]))),
        x87f_mul(x87f_load_f32(normal[0]),
                 x87f_load_f32(matrix->axis[0][0]))));
    out[1] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_mul(x87f_load_f32(normal[0]),
                     x87f_load_f32(matrix->axis[0][1])),
            x87f_mul(x87f_load_f32(normal[2]),
                     x87f_load_f32(matrix->axis[2][1]))),
        x87f_mul(x87f_load_f32(normal[1]),
                 x87f_load_f32(matrix->axis[1][1]))));
    out[2] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_mul(x87f_load_f32(normal[0]),
                     x87f_load_f32(matrix->axis[0][2])),
            x87f_mul(x87f_load_f32(normal[2]),
                     x87f_load_f32(matrix->axis[2][2]))),
        x87f_mul(x87f_load_f32(normal[1]),
                 x87f_load_f32(matrix->axis[1][2]))));
#else
    out[0] = (float)(
        ((long double)normal[2] * matrix->axis[2][0] +
         (long double)normal[1] * matrix->axis[1][0]) +
        (long double)normal[0] * matrix->axis[0][0]);
    out[1] = (float)(
        ((long double)normal[0] * matrix->axis[0][1] +
         (long double)normal[2] * matrix->axis[2][1]) +
        (long double)normal[1] * matrix->axis[1][1]);
    out[2] = (float)(
        ((long double)normal[0] * matrix->axis[0][2] +
         (long double)normal[2] * matrix->axis[2][2]) +
        (long double)normal[1] * matrix->axis[1][2]);
#endif
}

void XSurfaceTransformVectorRows43(
    const vec3_t vector, const DObjSkelMat *matrix,
    vec3_t out)
{
#if EMULATE_X87
    out[0] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_mul(x87f_load_f32(vector[1]),
                     x87f_load_f32(matrix->axis[0][1])),
            x87f_mul(x87f_load_f32(vector[2]),
                     x87f_load_f32(matrix->axis[0][2]))),
        x87f_mul(x87f_load_f32(vector[0]),
                 x87f_load_f32(matrix->axis[0][0]))));
    out[1] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_mul(x87f_load_f32(vector[0]),
                     x87f_load_f32(matrix->axis[1][0])),
            x87f_mul(x87f_load_f32(vector[1]),
                     x87f_load_f32(matrix->axis[1][1]))),
        x87f_mul(x87f_load_f32(vector[2]),
                 x87f_load_f32(matrix->axis[1][2]))));
    out[2] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_mul(x87f_load_f32(vector[0]),
                     x87f_load_f32(matrix->axis[2][0])),
            x87f_mul(x87f_load_f32(vector[1]),
                     x87f_load_f32(matrix->axis[2][1]))),
        x87f_mul(x87f_load_f32(vector[2]),
                 x87f_load_f32(matrix->axis[2][2]))));
#else
    out[0] = (float)(
        ((long double)vector[1] * matrix->axis[0][1] +
         (long double)vector[2] * matrix->axis[0][2]) +
        (long double)vector[0] * matrix->axis[0][0]);
    out[1] = (float)(
        ((long double)vector[0] * matrix->axis[1][0] +
         (long double)vector[1] * matrix->axis[1][1]) +
        (long double)vector[2] * matrix->axis[1][2]);
    out[2] = (float)(
        ((long double)vector[0] * matrix->axis[2][0] +
         (long double)vector[1] * matrix->axis[2][1]) +
        (long double)vector[2] * matrix->axis[2][2]);
#endif
}

#else

void XSurfaceTransformPoint43(const vec3_t point,
                              const DObjSkelMat *matrix,
                              vec3_t out)
{
#if EMULATE_X87
    out[0] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_add(
                x87f_mul(x87f_load_f32(point[0]),
                         x87f_load_f32(matrix->axis[0][0])),
                x87f_mul(x87f_load_f32(point[1]),
                         x87f_load_f32(matrix->axis[1][0]))),
            x87f_mul(x87f_load_f32(point[2]),
                     x87f_load_f32(matrix->axis[2][0]))),
        x87f_load_f32(matrix->origin[0])));
    out[1] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_add(
                x87f_mul(x87f_load_f32(point[0]),
                         x87f_load_f32(matrix->axis[0][1])),
                x87f_mul(x87f_load_f32(point[1]),
                         x87f_load_f32(matrix->axis[1][1]))),
            x87f_mul(x87f_load_f32(point[2]),
                     x87f_load_f32(matrix->axis[2][1]))),
        x87f_load_f32(matrix->origin[1])));
    out[2] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_add(
                x87f_mul(x87f_load_f32(point[0]),
                         x87f_load_f32(matrix->axis[0][2])),
                x87f_mul(x87f_load_f32(point[1]),
                         x87f_load_f32(matrix->axis[1][2]))),
            x87f_mul(x87f_load_f32(point[2]),
                     x87f_load_f32(matrix->axis[2][2]))),
        x87f_load_f32(matrix->origin[2])));
#else
    out[0] = (float)(
        (((long double)point[0] * matrix->axis[0][0] +
          (long double)point[1] * matrix->axis[1][0]) +
         (long double)point[2] * matrix->axis[2][0]) +
        (long double)matrix->origin[0]);
    out[1] = (float)(
        (((long double)point[0] * matrix->axis[0][1] +
          (long double)point[1] * matrix->axis[1][1]) +
         (long double)point[2] * matrix->axis[2][1]) +
        (long double)matrix->origin[1]);
    out[2] = (float)(
        (((long double)point[0] * matrix->axis[0][2] +
          (long double)point[1] * matrix->axis[1][2]) +
         (long double)point[2] * matrix->axis[2][2]) +
        (long double)matrix->origin[2]);
#endif
}

void XSurfaceAccumulateWeightedPoint43(
    const vec3_t point, float weight,
    const DObjSkelMat *matrix, vec3_t out)
{
#if EMULATE_X87
    out[0] = x87f_store_f32(x87f_add(
        x87f_mul(
            x87f_add(
                x87f_add(
                    x87f_add(
                        x87f_mul(x87f_load_f32(point[0]),
                                 x87f_load_f32(matrix->axis[0][0])),
                        x87f_mul(x87f_load_f32(point[1]),
                                 x87f_load_f32(matrix->axis[1][0]))),
                    x87f_mul(x87f_load_f32(point[2]),
                             x87f_load_f32(matrix->axis[2][0]))),
                x87f_load_f32(matrix->origin[0])),
            x87f_load_f32(weight)),
        x87f_load_f32(out[0])));
    out[1] = x87f_store_f32(x87f_add(
        x87f_mul(
            x87f_add(
                x87f_add(
                    x87f_add(
                        x87f_mul(x87f_load_f32(point[0]),
                                 x87f_load_f32(matrix->axis[0][1])),
                        x87f_mul(x87f_load_f32(point[1]),
                                 x87f_load_f32(matrix->axis[1][1]))),
                    x87f_mul(x87f_load_f32(point[2]),
                             x87f_load_f32(matrix->axis[2][1]))),
                x87f_load_f32(matrix->origin[1])),
            x87f_load_f32(weight)),
        x87f_load_f32(out[1])));
    out[2] = x87f_store_f32(x87f_add(
        x87f_mul(
            x87f_add(
                x87f_add(
                    x87f_add(
                        x87f_mul(x87f_load_f32(point[0]),
                                 x87f_load_f32(matrix->axis[0][2])),
                        x87f_mul(x87f_load_f32(point[1]),
                                 x87f_load_f32(matrix->axis[1][2]))),
                    x87f_mul(x87f_load_f32(point[2]),
                             x87f_load_f32(matrix->axis[2][2]))),
                x87f_load_f32(matrix->origin[2])),
            x87f_load_f32(weight)),
        x87f_load_f32(out[2])));
#else
    out[0] = (float)((
        (((long double)point[0] * matrix->axis[0][0] +
          (long double)point[1] * matrix->axis[1][0]) +
         (long double)point[2] * matrix->axis[2][0]) +
        (long double)matrix->origin[0]) * (long double)weight +
        (long double)out[0]);
    out[1] = (float)((
        (((long double)point[0] * matrix->axis[0][1] +
          (long double)point[1] * matrix->axis[1][1]) +
         (long double)point[2] * matrix->axis[2][1]) +
        (long double)matrix->origin[1]) * (long double)weight +
        (long double)out[1]);
    out[2] = (float)((
        (((long double)point[0] * matrix->axis[0][2] +
          (long double)point[1] * matrix->axis[1][2]) +
         (long double)point[2] * matrix->axis[2][2]) +
        (long double)matrix->origin[2]) * (long double)weight +
        (long double)out[2]);
#endif
}

void XSurfaceTransformNormal43(const vec3_t normal,
                               const DObjSkelMat *matrix,
                               vec3_t out)
{
#if EMULATE_X87
    out[0] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_mul(x87f_load_f32(normal[0]),
                     x87f_load_f32(matrix->axis[0][0])),
            x87f_mul(x87f_load_f32(normal[1]),
                     x87f_load_f32(matrix->axis[1][0]))),
        x87f_mul(x87f_load_f32(normal[2]),
                 x87f_load_f32(matrix->axis[2][0]))));
    out[1] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_mul(x87f_load_f32(normal[0]),
                     x87f_load_f32(matrix->axis[0][1])),
            x87f_mul(x87f_load_f32(normal[1]),
                     x87f_load_f32(matrix->axis[1][1]))),
        x87f_mul(x87f_load_f32(normal[2]),
                 x87f_load_f32(matrix->axis[2][1]))));
    out[2] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_mul(x87f_load_f32(normal[0]),
                     x87f_load_f32(matrix->axis[0][2])),
            x87f_mul(x87f_load_f32(normal[1]),
                     x87f_load_f32(matrix->axis[1][2]))),
        x87f_mul(x87f_load_f32(normal[2]),
                 x87f_load_f32(matrix->axis[2][2]))));
#else
    out[0] = (float)(
        ((long double)normal[0] * matrix->axis[0][0] +
         (long double)normal[1] * matrix->axis[1][0]) +
        (long double)normal[2] * matrix->axis[2][0]);
    out[1] = (float)(
        ((long double)normal[0] * matrix->axis[0][1] +
         (long double)normal[1] * matrix->axis[1][1]) +
        (long double)normal[2] * matrix->axis[2][1]);
    out[2] = (float)(
        ((long double)normal[0] * matrix->axis[0][2] +
         (long double)normal[1] * matrix->axis[1][2]) +
        (long double)normal[2] * matrix->axis[2][2]);
#endif
}

void XSurfaceTransformVectorRows43(
    const vec3_t vector, const DObjSkelMat *matrix,
    vec3_t out)
{
#if EMULATE_X87
    out[0] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_mul(x87f_load_f32(vector[0]),
                     x87f_load_f32(matrix->axis[0][0])),
            x87f_mul(x87f_load_f32(vector[1]),
                     x87f_load_f32(matrix->axis[0][1]))),
        x87f_mul(x87f_load_f32(vector[2]),
                 x87f_load_f32(matrix->axis[0][2]))));
    out[1] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_mul(x87f_load_f32(vector[0]),
                     x87f_load_f32(matrix->axis[1][0])),
            x87f_mul(x87f_load_f32(vector[1]),
                     x87f_load_f32(matrix->axis[1][1]))),
        x87f_mul(x87f_load_f32(vector[2]),
                 x87f_load_f32(matrix->axis[1][2]))));
    out[2] = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_mul(x87f_load_f32(vector[0]),
                     x87f_load_f32(matrix->axis[2][0])),
            x87f_mul(x87f_load_f32(vector[1]),
                     x87f_load_f32(matrix->axis[2][1]))),
        x87f_mul(x87f_load_f32(vector[2]),
                 x87f_load_f32(matrix->axis[2][2]))));
#else
    out[0] = (float)(
        ((long double)vector[0] * matrix->axis[0][0] +
         (long double)vector[1] * matrix->axis[0][1]) +
        (long double)vector[2] * matrix->axis[0][2]);
    out[1] = (float)(
        ((long double)vector[0] * matrix->axis[1][0] +
         (long double)vector[1] * matrix->axis[1][1]) +
        (long double)vector[2] * matrix->axis[1][2]);
    out[2] = (float)(
        ((long double)vector[0] * matrix->axis[2][0] +
         (long double)vector[1] * matrix->axis[2][1]) +
        (long double)vector[2] * matrix->axis[2][2]);
#endif
}

#endif
