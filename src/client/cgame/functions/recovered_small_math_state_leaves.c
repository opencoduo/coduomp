// Small complete leaf routines recovered from uo_cgame_mp_x86.dll.
// Each address annotation names the exact authoritative mcode record. Functions
// whose original symbol is not proven retain an address-shaped name.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

// Source RVA: 0x30042a10
int32_t CG_FloatSign(float value)
{
    return CG_FloatBits(value) < 0 ? -1 : 1;
}

// Source RVA: 0x30004d40
qboolean IsNonzero(int32_t value)
{
    return value != 0 ? qtrue : qfalse;
}

// Source RVA: 0x300140d0
qboolean AlwaysTrue(void)
{
    return qtrue;
}

// Source RVA: 0x3002aee0
qboolean CG_ReturnFalseStubA(void)
{
    return qfalse;
}

// Source RVA: 0x3002aef0
qboolean CG_ReturnFalseStubB(void)
{
    return qfalse;
}

// Source RVA: 0x3003d480
void NoOp(int32_t unused)
{
    (void)unused;
}

// Source RVA: 0x3003c1b0
void ClearShellshockBlur(void)
{
    cg_shellshockScreenBlurX = 0.0f;
    cg_shellshockScreenBlurY = 0.0f;
}

// Source RVA: 0x3005b100
void ResetMenuCount(void)
{
    menuCount = 0;
}

// Source RVA: 0x3005adb0
int32_t GetMenuCount(void)
{
    return menuCount;
}

// Source RVA: 0x3005b110
displayContextDef_t *GetDisplayContext(void)
{
    return DC;
}
