#include "../client_recovered.h"

// Source: uo_cgame_mp_x86.dll 0x3003b5f0..0x3003b66f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003b5f0_3003b66f.mcode

enum {
    POISSON_SAMPLE_TABLE_COUNT = 128
};

/*
 * The helper at 0x3003b510 receives the previous pair in EDI, the output pair
 * in ESI, and a stack float with bits 0x3f000000. Its CG_PoissonDiskSample
 * declaration is centralized in client_recovered.h. The starting pair is the
 * first two components of vec3_origin, and the print format is the source
 * literal at 0x3007a418.
 */

void CG_PrintPoissonDiskSamples(void)
{
    vec2_t table[POISSON_SAMPLE_TABLE_COUNT];

    CG_PoissonDiskSample(table[0], vec3_origin, 0.5f);

    for (int i = 1; i < POISSON_SAMPLE_TABLE_COUNT; i++) {
        CG_PoissonDiskSample(table[i], table[i - 1], 0.5f);
    }

    for (int i = 0; i < POISSON_SAMPLE_TABLE_COUNT; i++) {
        Com_Printf("\t{%f, %f},\n", (double)table[i][0], (double)table[i][1]);
    }
}
