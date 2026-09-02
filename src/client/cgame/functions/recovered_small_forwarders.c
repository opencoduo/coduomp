// Complete forwarding/accessor leaves recovered from the exact DLL instructions.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

// Source RVA: 0x30031c40
const char *CG_GameTypeString(void)
{
    return cgs_gametype;
}

// Source RVA: 0x30004d50
qhandle_t RegisterWorldModel(const char *name)
{
    enum {
        WORLD_MODEL_CATEGORY = 7
    };
    return CG_RegisterModel(name, WORLD_MODEL_CATEGORY);
}

// Source RVA: 0x3001c100
qboolean UpdateShellshockOverlay(void)
{
    return CG_UpdateFadeOverlay(cg_shellShockSwayParams, cg_shellShockSwayStartTime, cg_shellShockSwayDuration);
}

// Source RVA: 0x3002d7f0
const char *CG_SafeTranslateString(const char *reference)
{
    /* EAX is the exact shared 0x30077b28 localization-domain object. */
    return CG_SafeTranslateString_Internal(cg_localizationContext, reference);
}

void CG_CenterPrint(const char *text, float y, float charWidth) /* 0x30019190 */
{
    CG_PriorityCenterPrint(text, y, charWidth, 0);
}
