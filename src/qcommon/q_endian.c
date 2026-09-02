#include "q_endian.h"

#include <stdint.h>
#include <string.h>

/*
 * This complete byte-order family is behaviorally identical in every retained
 * original target.  Each family appears in the same order at these ranges:
 *
 *   CoDUOMP.exe                 0x0044f480..0x0044f6c7
 *   uo_cgame_mp_x86.dll        0x3004e270..0x3004e4b7
 *   uo_ui_mp_x86.dll           0x400062a0..0x400064e7
 *   uo_game_mp_x86.dll         0x20057a90..0x20057cd7
 *   coduo_lnxded               0x0808649a..0x08086783
 *   game.mp.uo.i386.so         0x00092ef4..0x0009325b
 *
 * Supporting PowerPC bodies agree as well: the Mac engine has Swap_Init and
 * the swap/no-swap bodies at code-section offsets 0x000e25e0..0x000e2a00;
 * the Mac game module has them at 0x00019080..0x00019380.
 *
 * Quake 3 names the eight-byte source type qint64.  The Windows i386 compiler
 * returns that small aggregate in EDX:EAX, while Linux i386 and PowerPC use a
 * hidden result pointer.  Those are compiler ABI representations of the same
 * source type, not platform-specific behavior.  All targets reverse the same
 * eight bytes, and CoD consistently changed FloatSwap from Quake 3's pointer
 * parameter to a float passed by value.
 */
static int16_t (*_BigShort)(int16_t value);
static int16_t (*_LittleShort)(int16_t value);
static int32_t (*_BigLong)(int32_t value);
static int32_t (*_LittleLong)(int32_t value);
static qint64 (*_BigLong64)(qint64 value);
static qint64 (*_LittleLong64)(qint64 value);
static float (*_BigFloat)(float value);
static float (*_LittleFloat)(float value);

int16_t BigShort(int16_t value)
{
    return _BigShort(value);
}

int32_t BigLong(int32_t value)
{
    return _BigLong(value);
}

qint64 BigLong64(qint64 value)
{
    return _BigLong64(value);
}

qint64 LittleLong64(qint64 value)
{
    return _LittleLong64(value);
}

float BigFloat(float value)
{
    return _BigFloat(value);
}

int16_t ShortSwap(int16_t value)
{
    const uint16_t bits = (uint16_t)value;
    const uint16_t swapped = (uint16_t)((bits << 8U) | (bits >> 8U));
    int16_t result;

    memcpy(&result, &swapped, sizeof(result));
    return result;
}

int16_t ShortNoSwap(int16_t value)
{
    return value;
}

int32_t LongSwap(int32_t value)
{
    const uint32_t bits = (uint32_t)value;
    const uint32_t swapped =
        ((bits & UINT32_C(0x000000ff)) << 24U) |
        ((bits & UINT32_C(0x0000ff00)) << 8U) |
        ((bits & UINT32_C(0x00ff0000)) >> 8U) |
        ((bits & UINT32_C(0xff000000)) >> 24U);
    int32_t result;

    memcpy(&result, &swapped, sizeof(result));
    return result;
}

int32_t LongNoSwap(int32_t value)
{
    return value;
}

qint64 Long64Swap(qint64 value)
{
    qint64 result;

    result.b0 = value.b7;
    result.b1 = value.b6;
    result.b2 = value.b5;
    result.b3 = value.b4;
    result.b4 = value.b3;
    result.b5 = value.b2;
    result.b6 = value.b1;
    result.b7 = value.b0;
    return result;
}

qint64 Long64NoSwap(qint64 value)
{
    return value;
}

float FloatSwap(float value)
{
    uint32_t bits;
    int32_t signedBits;

    memcpy(&bits, &value, sizeof(bits));
    memcpy(&signedBits, &bits, sizeof(signedBits));
    signedBits = LongSwap(signedBits);
    memcpy(&bits, &signedBits, sizeof(bits));
    memcpy(&value, &bits, sizeof(value));
    return value;
}

float FloatNoSwap(float value)
{
    return value;
}

void Swap_Init(void)
{
    const uint16_t endianProbe = UINT16_C(1);

    if (*(const uint8_t *)(const void *)&endianProbe == UINT8_C(1)) {
        _BigShort = ShortSwap;
        _LittleShort = ShortNoSwap;
        _BigLong = LongSwap;
        _LittleLong = LongNoSwap;
        _BigLong64 = Long64Swap;
        _LittleLong64 = Long64NoSwap;
        _BigFloat = FloatSwap;
        _LittleFloat = FloatNoSwap;
    } else {
        _BigShort = ShortNoSwap;
        _LittleShort = ShortSwap;
        _BigLong = LongNoSwap;
        _LittleLong = LongSwap;
        _BigLong64 = Long64NoSwap;
        _LittleLong64 = Long64Swap;
        _BigFloat = FloatNoSwap;
        _LittleFloat = FloatSwap;
    }
}
