// Source: uo_cgame_mp_x86.dll small common/math routines at the RVAs shown.
// Evidence: corresponding cgame_mp/mcode/uo_cgame_mp_x86/FUN_*.mcode records.

#include <stdint.h>

#include "client/cgame/client_recovered.h"

/* The DLL leaves the raw x87 sum in ST0 (no store before the RET at
 * 0x30049454). long double is the project's raw-register carrier; the target
 * control word, not the source carrier type, governs PC-sensitive arithmetic.
 *
 * The term order is load-order parity only: the chain is one unbroken
 * FLD/FMUL/FADDP with no store, so regrouping is value-identical, but at -O0
 * GCC emits the loads in source term order and stock loads [ECX+8], [ECX+4],
 * [ECX] (0x30049440..52) -- the [2] product first, the [0] product last. */
long double DotProduct3(const vec3_t a, const vec3_t b) /* 0x30049440 */
{
    return (long double)a[2] * b[2] + (long double)a[1] * b[1] + (long double)a[0] * b[0];
}

void VectorNegate3(vec3_t value) /* 0x300499b0 */
{
    value[0] = -value[0];
    value[1] = -value[1];
    value[2] = -value[2];
}

void VectorScale4(const float in[4], float scale, float out[4]) /* 0x300499d0 */
{
    out[0] = scale * in[0];
    out[1] = scale * in[1];
    out[2] = scale * in[2];
    out[3] = scale * in[3];
}

void MatrixTransformVector3(const vec3_t in, const float matrix[3][3], vec3_t out) /* 0x30049a40 */
{
    /* out[0] leads with the [0][1] product for load-order parity: stock loads
     * [EAX+4], [EAX+8], [EAX] (0x30049a40..52). Value is unaffected -- each
     * component is one unbroken FLD/FMUL/FADDP chain with a single trailing
     * FSTP. Rows 1-2 load +0xc/+0x10/+0x14 and +0x18/+0x1c/+0x20 in natural
     * order (0x30049a56/0x30049a6e), so they stay as written. */
    out[0] = (float)(((double)matrix[0][1] * (double)in[1] + (double)matrix[0][2] * (double)in[2]) + (double)matrix[0][0] * (double)in[0]);
    out[1] = (float)(((double)matrix[1][0] * (double)in[0] + (double)matrix[1][1] * (double)in[1]) + (double)matrix[1][2] * (double)in[2]);
    out[2] = (float)(((double)matrix[2][0] * (double)in[0] + (double)matrix[2][1] * (double)in[1]) + (double)matrix[2][2] * (double)in[2]);
}
