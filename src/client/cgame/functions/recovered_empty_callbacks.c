// Machine-code-proven empty callbacks from uo_cgame_mp_x86.dll. Each listed
// function is exactly one bare RET; arguments, where known from the installing
// callback table, are intentionally ignored. Address-shaped names remain where
// no honest source symbol has yet been established.

#include "client/cgame/client_recovered.h"

// 0x30016310 / 0x30016320: installed in cg_scriptExports by Scr_FarHook.
// Source RVA: 0x30016310
void CGAME_ABI_CDECL Scr_SetObjectField(
    int32_t classNum, int32_t objectNum, int32_t fieldIndex)
{
    (void)classNum;
    (void)objectNum;
    (void)fieldIndex;
}
// Source RVA: 0x30016320
void CGAME_ABI_CDECL Scr_GetObjectField(
    int32_t classNum, int32_t objectNum, int32_t fieldIndex)
{
    (void)classNum;
    (void)objectNum;
    (void)fieldIndex;
}

// 0x300257d0: same-module Mac symbol; intentionally empty flame hook.
void CG_FlameSmokeParticle(void) {}

// 0x3002d520: installed as displayContextDef_t::feederSelection.
void CG_FeederSelection(float feederID, int32_t index)
{
    (void)feederID;
    (void)index;
}

/* These seven bare-RET functions have no code or data xrefs in the DLL and no
 * callback-table installation that distinguishes their roles. Keep the agreed
 * irreducible-function spelling: the RVA is the only honest identity evidence. */
// Source RVA: 0x3002ea20
void UnresolvedFunction_3002ea20(void) {}
// Source RVA: 0x3002ea80
void UnresolvedFunction_3002ea80(void) {}
// Source RVA: 0x30031700
void UnresolvedFunction_30031700(void) {}
// Source RVA: 0x30031710
void UnresolvedFunction_30031710(void) {}
// Source RVA: 0x30032750
void UnresolvedFunction_30032750(void) {}
// Source RVA: 0x30032760
void UnresolvedFunction_30032760(void) {}
// Source RVA: 0x30032770
void UnresolvedFunction_30032770(void) {}

// 0x30032880: installed as displayContextDef_t::runScript; matching PPC symbol.
// Source RVA: 0x30032880
void CG_RunMenuScript(char **args)
{
    (void)args;
}
