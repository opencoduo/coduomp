#include "backend.h"

/* Sources: CoDUOMP.exe 0x004e9440..0x004e944c and
 * 0x004e9450..0x004e945c.
 * Evidence:
 *   coduomp/mcode/CoDUOMP/FUN_004e9440_004e944d.mcode
 *   coduomp/mcode/CoDUOMP/FUN_004e9450_004e945d.mcode
 * The same-module Mac R_DoneFreeType and R_InitFreeType bodies perform these
 * same two stores, and its RE_Shutdown/R_Init call graph fixes their lifecycle
 * roles. Windows LTCG inlines the resets at 0x004c51ec and 0x004c5001. The
 * formerly assigned one-byte RET records at 0x004c5590/0x004c55a0 have no
 * references and do not match the Mac function behavior; their old names were
 * therefore erroneous. The two true Windows bodies are identical, so their
 * individual address-to-name ordering is not behaviorally distinguishable. */
void R_DoneFreeType(void)
{
    rendererRegisteredFontCount = 0;
    rendererAsianFontLoaded = qfalse;
}

void R_InitFreeType(void)
{
    rendererRegisteredFontCount = 0;
    rendererAsianFontLoaded = qfalse;
}
