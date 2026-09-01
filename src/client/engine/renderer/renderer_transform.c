#include "backend.h"

#include <math.h>

#include "gl_state.h"

/* Source: CoDUOMP.exe 0x004e3aa0..0x004e3b03.
 * Evidence: original PE instructions promoted to
 * coduomp/mcode/CoDUOMP/FUN_004e3aa0_004e3b03.mcode.
 * Provisional name: this is the inverse-direction companion to
 * R_LocalNormalToWorld. The three outputs are the world vector dotted with
 * the three current-orientation axes. Each complete dot product remains in
 * x87 until its destination float store. Ghidra initially left the entire
 * body in an executable gap even though it has a normal RET-terminated
 * boundary. */
void R_WorldNormalToLocal(const vec3_t world, vec3_t local)
{
    local[0] = (float)(
        ((long double)tr.orientation.axis[0][0] * world[0] +
         (long double)tr.orientation.axis[0][2] * world[2]) +
        (long double)tr.orientation.axis[0][1] * world[1]);
    local[1] = (float)(
        ((long double)tr.orientation.axis[1][0] * world[0] +
         (long double)tr.orientation.axis[1][2] * world[2]) +
        (long double)tr.orientation.axis[1][1] * world[1]);
    local[2] = (float)(
        ((long double)tr.orientation.axis[2][0] * world[0] +
         (long double)tr.orientation.axis[2][2] * world[2]) +
        (long double)tr.orientation.axis[2][1] * world[1]);
}

/* Source: CoDUOMP.exe 0x004e3b10..0x004e3bff.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e3b10_004e3bff.mcode.
 * Name: same-module Mac symbol R_TransformModelToClip. The Windows caller at
 * 0x004e52a3 proves source/model/projection/eye/clip argument roles. Parentheses
 * retain the x87 addition order used for each component. */
void R_TransformModelToClip(const vec3_t source,
                            const float modelMatrix[16],
                            const float projectionMatrix[16],
                            vec4_t eye, vec4_t clip)
{
    eye[0] = (float)(
        (((long double)modelMatrix[4] * source[1] +
          (long double)modelMatrix[8] * source[2]) +
         (long double)modelMatrix[0] * source[0]) +
        modelMatrix[12]);
    eye[1] = (float)(
        (((long double)modelMatrix[5] * source[1] +
          (long double)modelMatrix[1] * source[0]) +
         (long double)modelMatrix[9] * source[2]) +
        modelMatrix[13]);
    eye[2] = (float)(
        (((long double)modelMatrix[6] * source[1] +
          (long double)modelMatrix[2] * source[0]) +
         (long double)modelMatrix[10] * source[2]) +
        modelMatrix[14]);
    eye[3] = (float)(
        (((long double)modelMatrix[7] * source[1] +
          (long double)modelMatrix[3] * source[0]) +
         (long double)modelMatrix[11] * source[2]) +
        modelMatrix[15]);

    clip[0] = (float)(
        (((long double)projectionMatrix[12] * eye[3] +
          (long double)projectionMatrix[8] * eye[2]) +
         (long double)projectionMatrix[4] * eye[1]) +
        (long double)projectionMatrix[0] * eye[0]);
    clip[1] = (float)(
        (((long double)projectionMatrix[13] * eye[3] +
          (long double)projectionMatrix[1] * eye[0]) +
         (long double)projectionMatrix[9] * eye[2]) +
        (long double)projectionMatrix[5] * eye[1]);
    clip[2] = (float)(
        (((long double)projectionMatrix[14] * eye[3] +
          (long double)projectionMatrix[2] * eye[0]) +
         (long double)projectionMatrix[10] * eye[2]) +
        (long double)projectionMatrix[6] * eye[1]);
    clip[3] = (float)(
        (((long double)projectionMatrix[15] * eye[3] +
          (long double)projectionMatrix[3] * eye[0]) +
         (long double)projectionMatrix[11] * eye[2]) +
        (long double)projectionMatrix[7] * eye[1]);
}

/* Source: CoDUOMP.exe 0x004e3c00..0x004e3d03.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e3c00_004e3d03.mcode.
 * Name: same-module Mac symbol R_TransformHomogenousModelToClip. The caller at
 * 0x004eeec3 supplies a four-component source, unlike R_TransformModelToClip's
 * implicit homogeneous 1.0 component. */
void R_TransformHomogenousModelToClip(const vec4_t source,
                                      const float modelMatrix[16],
                                      const float projectionMatrix[16],
                                      vec4_t eye, vec4_t clip)
{
    eye[0] = (float)(
        (((long double)modelMatrix[8] * source[2] +
          (long double)modelMatrix[4] * source[1]) +
         (long double)modelMatrix[12] * source[3]) +
        (long double)modelMatrix[0] * source[0]);
    eye[1] = (float)(
        (((long double)modelMatrix[9] * source[2] +
          (long double)modelMatrix[5] * source[1]) +
         (long double)modelMatrix[1] * source[0]) +
        (long double)modelMatrix[13] * source[3]);
    eye[2] = (float)(
        (((long double)modelMatrix[10] * source[2] +
          (long double)modelMatrix[6] * source[1]) +
         (long double)modelMatrix[2] * source[0]) +
        (long double)modelMatrix[14] * source[3]);
    eye[3] = (float)(
        (((long double)modelMatrix[11] * source[2] +
          (long double)modelMatrix[7] * source[1]) +
         (long double)modelMatrix[3] * source[0]) +
        (long double)modelMatrix[15] * source[3]);

    clip[0] = (float)(
        (((long double)projectionMatrix[8] * eye[2] +
          (long double)projectionMatrix[4] * eye[1]) +
         (long double)projectionMatrix[12] * eye[3]) +
        (long double)projectionMatrix[0] * eye[0]);
    clip[1] = (float)(
        (((long double)projectionMatrix[9] * eye[2] +
          (long double)projectionMatrix[5] * eye[1]) +
         (long double)projectionMatrix[1] * eye[0]) +
        (long double)projectionMatrix[13] * eye[3]);
    clip[2] = (float)(
        (((long double)projectionMatrix[10] * eye[2] +
          (long double)projectionMatrix[6] * eye[1]) +
         (long double)projectionMatrix[2] * eye[0]) +
        (long double)projectionMatrix[14] * eye[3]);
    clip[3] = (float)(
        (((long double)projectionMatrix[11] * eye[2] +
          (long double)projectionMatrix[7] * eye[1]) +
         (long double)projectionMatrix[3] * eye[0]) +
        (long double)projectionMatrix[15] * eye[3]);
}

/* Source: CoDUOMP.exe 0x004e3d10..0x004e3d93.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e3d10_004e3d93.mcode.
 * Name: same-module Mac symbol R_TransformClipToWindow. The explicit float
 * stores before floor preserve the binary's sequence: calculate each window
 * coordinate as float, add the exact 0.5f constant, call double floor, then
 * store the result back as float. */
void R_TransformClipToWindow(const vec4_t clip,
                             const viewParms_t *viewParms,
                             vec4_t normalized, vec4_t window)
{
    normalized[0] = (float)(
        (long double)clip[0] / clip[3]);
    normalized[1] = (float)(
        (long double)clip[1] / clip[3]);
    normalized[2] = (float)(
        ((long double)clip[2] + clip[3]) /
        ((long double)clip[3] + clip[3]));

    window[0] = (float)(
        ((long double)normalized[0] + 1.0L) *
        (long double)viewParms->viewportWidth * 0.5L);
    window[1] = (float)(
        (long double)viewParms->viewportHeight *
        ((long double)normalized[1] + 1.0L) * 0.5L);
    window[2] = normalized[2];

    window[0] = (float)floor(
        (double)((long double)window[0] + 0.5L));
    window[1] = (float)floor(
        (double)((long double)window[1] + 0.5L));
}

/* Source: CoDUOMP.exe 0x004e3da0..0x004e3e41.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e3da0_004e3e41.mcode.
 * Name: same-module Mac symbol myGlMultMatrix. Each loop iteration consumes
 * one contiguous row from left and writes one row to product. The nonuniform
 * source order below follows the original x87 instruction order exactly. */
void myGlMultMatrix(const float left[16], const float right[16],
                    float product[16])
{
    for (int32_t row = 0; row < 4; ++row) {
        const int32_t base = row * 4;

        product[base] = (float)(
            (((long double)left[base + 2] * right[8] +
              (long double)left[base + 3] * right[12]) +
             (long double)left[base + 1] * right[4]) +
            (long double)left[base] * right[0]);
        product[base + 1] = (float)(
            (((long double)left[base + 3] * right[13] +
              (long double)left[base + 1] * right[5]) +
             (long double)left[base + 2] * right[9]) +
            (long double)left[base] * right[1]);
        product[base + 2] = (float)(
            (((long double)left[base + 3] * right[14] +
              (long double)left[base + 1] * right[6]) +
             (long double)left[base] * right[2]) +
            (long double)left[base + 2] * right[10]);
        product[base + 3] = (float)(
            (((long double)left[base + 2] * right[11] +
              (long double)left[base + 3] * right[15]) +
             (long double)left[base + 1] * right[7]) +
            (long double)left[base] * right[3]);
    }
}
