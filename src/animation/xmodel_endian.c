#include "xmodel.h"

/*
 * Complete XModel byte-order conversion cluster for the retained little-endian
 * targets.
 *
 * Windows authority: CoDUOMP.exe 0x0049cd20..0x0049cd44.
 * Linux authority: coduo_lnxded 0x080c4e86..0x080c4eb2.
 *
 * Both engines return the input bits unchanged.  The longer Linux bodies are
 * unoptimized function prologues and spills, not different conversions.  In
 * particular, XModelLittleFloat loads its binary32 input into x87 ST0 in both
 * binaries; the value is already exactly representable in that carrier.
 */

int16_t XModelLittleInt16(int16_t value)
{
    return value;
}

uint32_t XModelLittleUInt32(uint32_t value)
{
    return value;
}

float XModelLittleFloat(float value)
{
    return value;
}
