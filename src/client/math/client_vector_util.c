#include "client_math.h"

#include <math.h>
#include <stdint.h>

/*
 * This complete utility cluster is shared by the original cgame and UI DLLs.
 * The vector-lane bodies are byte-identical; the two floor-based bodies differ
 * only in relocations to the module-local floor import and the same binary32
 * 0.5 constant:
 *
 *   SnapVector             0x00434e20 / 0x3004cf80 / 0x40004f90
 *   VectorAdd5             0x00434e70 / 0x3004cfd0 / 0x40004fe0
 *   VectorScale5           0x00434ea0 / 0x3004d000 / 0x40005010
 *   VectorCopy3Secondary   0x00434ee0 / 0x3004d040 / 0x40005050
 *
 * The 5-lane and copy bodies are byte-identical across all three modules.
 * SnapVector differs only in relocations to the same binary32 0.5 constant and
 * module-local floor implementation.  Its floor arguments are explicitly
 * narrowed to binary64 at the original FSTP m64 boundaries.
 */

void SnapVector(vec3_t vector)
{
    vector[0] = (float)floor((double)((long double)vector[0] + 0.5L));
    vector[1] = (float)floor((double)((long double)vector[1] + 0.5L));
    vector[2] = (float)floor((double)((long double)vector[2] + 0.5L));
}

void VectorAdd5(const float left[5], const float right[5], float output[5])
{
    for (int32_t component = 0; component < 5; ++component) {
        output[component] = left[component] + right[component];
    }
}

void VectorScale5(const float input[5], float scale, float output[5])
{
    for (int32_t component = 0; component < 5; ++component) {
        output[component] = scale * input[component];
    }
}

void VectorCopy3Secondary(const vec3_t input, vec3_t output)
{
    output[0] = input[0];
    output[1] = input[1];
    output[2] = input[2];
}
