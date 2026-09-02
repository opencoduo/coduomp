// Source: uo_cgame_mp_x86.dll 0x3003f390..0x3003f400
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003f390_3003f400.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_FxSetTestPosition(void)
{
    enum {
        FX_TEST_DISTANCE = 100
    };

    long double positionX = (long double)cg_refdef.viewaxis[0][0] * FX_TEST_DISTANCE + cg_refdef.vieworg[0];
    long double positionY = (long double)cg_refdef.viewaxis[0][1] * FX_TEST_DISTANCE + cg_refdef.vieworg[1];
    long double positionZ = (long double)cg_refdef.viewaxis[0][2] * FX_TEST_DISTANCE + cg_refdef.vieworg[2];

    cg_periodicEffectOrigin[0] = (float)positionX;
    cg_periodicEffectOrigin[1] = (float)positionY;
    cg_periodicEffectOrigin[2] = (float)positionZ;

    Com_Printf("\n\nFX Testing position set to: (%f, %f, %f)\n\n", cg_periodicEffectOrigin[0], cg_periodicEffectOrigin[1],
               (double)positionZ);
}
