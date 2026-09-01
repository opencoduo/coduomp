#include "ui_module_abi.h"

#include <string.h>

// Source: uo_ui_mp_x86.dll 0x4001d230..0x4001d239
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001d230_4001d239.mcode
// Same-module PPC symbol: PASSFLOAT.
int32_t PASSFLOAT(float value)
{
    int32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}
