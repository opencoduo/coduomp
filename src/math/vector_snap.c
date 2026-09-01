#include "q_math.h"

#include <math.h>

/*
 * The four authoritative Windows bodies are instruction-identical apart from
 * relocated 0.5f constants and floor calls:
 *
 *   CoDUOMP.exe                 0x00434e20
 *   uo_cgame_mp_x86.dll        0x3004cf80
 *   uo_ui_mp_x86.dll           0x40004f90
 *   uo_game_mp_x86.dll         0x20019fd0
 *
 * Linux game RVA 0x0003f08b and dedicated 0x0806b17a perform the same three
 * float loads, additions with 0.5f, binary64 stores for floor, and binary32
 * result stores.  Converting each binary32 input to double before the addition
 * expresses that common binary64 call boundary directly; no target-specific
 * arithmetic body is present in the originals.
 */
void VectorSnap(vec3_t vector)
{
    vector[0] = (float)floor((double)vector[0] + 0.5);
    vector[1] = (float)floor((double)vector[1] + 0.5);
    vector[2] = (float)floor((double)vector[2] + 0.5);
}
