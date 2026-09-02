#include "q_math.h"

#include <stdint.h>

/*
 * The original same-target bodies are byte-identical:
 *
 *                           Windows client  Windows game  Linux engine  Linux game
 * Vec10Copy                 0x00434b30      0x20019ce0    0x0806ad90    0x0003ec50
 * _Vector5Add               0x00434e70      0x2001a020    0x0806b1e1    0x0003f0ff
 * _Vector5Scale             0x00434ea0      0x2001a050    0x0806b255    0x0003f173
 * _Vector53Copy             0x00434ee0      0x2001a090    0x0806b2b3    0x0003f1d1
 *
 * The copy helpers operate on raw 32-bit lanes.  The arithmetic helpers store
 * each independent binary32 result before advancing to the next lane.
 */
void Vec10Copy(const uint32_t input[10], uint32_t output[10])
{
    for (int32_t lane = 0; lane < 10; ++lane) {
        output[lane] = input[lane];
    }
}

void _Vector5Add(const float first[5], const float second[5],
                 float result[5])
{
    for (int32_t lane = 0; lane < 5; ++lane) {
        result[lane] = first[lane] + second[lane];
    }
}

void _Vector5Scale(const float input[5], float scale, float result[5])
{
    for (int32_t lane = 0; lane < 5; ++lane) {
        result[lane] = input[lane] * scale;
    }
}

void _Vector53Copy(const uint32_t input[3], uint32_t result[3])
{
    for (int32_t lane = 0; lane < 3; ++lane) {
        result[lane] = input[lane];
    }
}
