#include "client_math.h"

/*
 * The complete Windows client bodies are instruction-identical:
 *
 *   CoDUOMP.exe          0x004329d0
 *   uo_cgame_mp_x86.dll 0x3004ab30
 *   uo_ui_mp_x86.dll    0x40002b00
 *
 * Each body is 0x14a bytes and has SHA-256
 * 02a097d103dfe6540225f27cb9bff507b031f537857a6371c7ebbed08ece119e.
 * The expressions below retain every x87 product/addition order and binary32
 * store point.  The last homogeneous-lane store precedes the final origin-Z
 * FSTP in the original, so origin Z remains an extended temporary until after
 * output->origin[3] is written.
 */
void CG_ComposeBoneMatrix(const DObjSkelMat *parent,
                          const matrix43_t *local, DObjSkelMat *output)
{
    const vec3_t *const axis = local->axis;

    output->axis[0][0] = (float)(
        ((long double)parent->axis[0][0] * axis[0][0] +
         (long double)parent->axis[0][1] * axis[1][0]) +
        (long double)axis[2][0] * parent->axis[0][2]);
    output->axis[1][0] = (float)(
        ((long double)parent->axis[1][1] * axis[1][0] +
         (long double)parent->axis[1][0] * axis[0][0]) +
        (long double)parent->axis[1][2] * axis[2][0]);
    output->axis[2][0] = (float)(
        ((long double)parent->axis[2][1] * axis[1][0] +
         (long double)parent->axis[2][0] * axis[0][0]) +
        (long double)parent->axis[2][2] * axis[2][0]);

    output->axis[0][1] = (float)(
        ((long double)parent->axis[0][1] * axis[1][1] +
         (long double)axis[2][1] * parent->axis[0][2]) +
        (long double)axis[0][1] * parent->axis[0][0]);
    output->axis[1][1] = (float)(
        ((long double)parent->axis[1][2] * axis[2][1] +
         (long double)parent->axis[1][1] * axis[1][1]) +
        (long double)parent->axis[1][0] * axis[0][1]);
    output->axis[2][1] = (float)(
        ((long double)parent->axis[2][2] * axis[2][1] +
         (long double)parent->axis[2][1] * axis[1][1]) +
        (long double)parent->axis[2][0] * axis[0][1]);

    output->axis[0][2] = (float)(
        ((long double)parent->axis[0][0] * axis[0][2] +
         (long double)parent->axis[0][1] * axis[1][2]) +
        (long double)axis[2][2] * parent->axis[0][2]);
    output->axis[1][2] = (float)(
        ((long double)parent->axis[1][2] * axis[2][2] +
         (long double)parent->axis[1][1] * axis[1][2]) +
        (long double)parent->axis[1][0] * axis[0][2]);
    output->axis[2][2] = (float)(
        ((long double)parent->axis[2][2] * axis[2][2] +
         (long double)parent->axis[2][1] * axis[1][2]) +
        (long double)parent->axis[2][0] * axis[0][2]);

    output->axis[0][3] = 0.0f;
    output->axis[1][3] = 0.0f;
    output->axis[2][3] = 0.0f;

    output->origin[0] = (float)(
        (((long double)parent->origin[0] * axis[0][0] +
          (long double)axis[2][0] * parent->origin[2]) +
         (long double)parent->origin[1] * axis[1][0]) +
        local->origin[0]);
    output->origin[1] = (float)(
        (((long double)axis[2][1] * parent->origin[2] +
          (long double)axis[1][1] * parent->origin[1]) +
         (long double)axis[0][1] * parent->origin[0]) +
        local->origin[1]);

    const long double originZ =
        (((long double)parent->origin[0] * axis[0][2] +
          (long double)parent->origin[2] * axis[2][2]) +
         (long double)parent->origin[1] * axis[1][2]) +
        local->origin[2];
    output->origin[3] = 1.0f;
    output->origin[2] = (float)originZ;
}
