#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */

#include "core_math_private.h"

#include <string.h>

#if EMULATE_X87
/* 80-bit affine dot ((p0*a0 + p1*a1) + p2*a2) + origin, rounded to float. */
#define CODUO_TP43_DOT(p0, p1, p2, m, j)                                       \
    x87f_store_f32(x87f_add(                                                   \
        x87f_add(x87f_add(x87f_mul(x87f_load_f32(p0),                         \
                                   x87f_load_f32((m)->axis[0][j])),           \
                          x87f_mul(x87f_load_f32(p1),                         \
                                   x87f_load_f32((m)->axis[1][j]))),          \
                 x87f_mul(x87f_load_f32(p2), x87f_load_f32((m)->axis[2][j]))),\
        x87f_load_f32((m)->origin[j])))
#endif

void MatrixTransformPoint43CompactInPlace(vec3_t point,
                  const matrix43_t *matrix)
{
    const float point0 = point[0];
    const float point1 = point[1];
    const float point2 = point[2];

#if EMULATE_X87
    /* x87-faithful transcription (stock coduo_lnxded 0x080694d5): 80-bit affine
     * dots rounded to float; columns 0/1 via temps, column 2 written directly,
     * all from the saved original point (in-place ordering preserved). */
    const float transformed0 = CODUO_TP43_DOT(point0, point1, point2, matrix, 0);
    const float transformed1 = CODUO_TP43_DOT(point0, point1, point2, matrix, 1);

    point[2] = CODUO_TP43_DOT(point0, point1, point2, matrix, 2);
#else
    const float transformed0 =
        (((point0 * matrix->axis[0][0]) + (point1 * matrix->axis[1][0])) +
         (point2 * matrix->axis[2][0])) +
        matrix->origin[0];
    const float transformed1 =
        (((point0 * matrix->axis[0][1]) + (point1 * matrix->axis[1][1])) +
         (point2 * matrix->axis[2][1])) +
        matrix->origin[1];

    point[2] = (((point0 * matrix->axis[0][2]) +
                 (point1 * matrix->axis[1][2])) +
                (point2 * matrix->axis[2][2])) +
               matrix->origin[2];
#endif
    memcpy(&point[0], &transformed0, sizeof(point[0]));
    memcpy(&point[1], &transformed1, sizeof(point[1]));
}
