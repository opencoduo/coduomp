#include "animation_private.h"

/*
 * All retained original bodies test the same byte-addressed skip bit, advance
 * through 0x20-byte DObjAnimMat records, and clear lanes in the exact order
 * quat XYZW, weight, translation ZYX:
 *
 *   CoDUOMP.exe                 0x00497a10..0x00497a5f
 *   coduo_lnxded               0x080baa8a..0x080bab1b
 *   CoD United Offensive MP    PEF file 0x000eff70..0x000effe3
 */
void XAnimClearData(DObjAnimMat *part)
{
    const uint8_t *skipBytes = (const uint8_t *)xanim_evalSkipBits;

    for (int32_t partIndex = 0; partIndex < xanim_evalPartCount;
         ++partIndex, ++part) {
        if ((skipBytes[partIndex >> 3] &
             (uint8_t)(1U << (partIndex & 7))) != 0) {
            continue;
        }

        part->quat[0] = 0.0f;
        part->quat[1] = 0.0f;
        part->quat[2] = 0.0f;
        part->quat[3] = 0.0f;
        part->accumulatedWeight = 0.0f;
        part->translation[2] = 0.0f;
        part->translation[1] = 0.0f;
        part->translation[0] = 0.0f;
    }
}
