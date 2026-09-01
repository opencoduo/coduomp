// Source: uo_cgame_mp_x86.dll 0x3002d800..0x3002d847
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002d800_3002d847.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

//
// CG_SafeTranslateHudElemString (0x3002d800) — return the localized hud-element string for a
// hud-element string index.
//
// Behavior (proven from machine code):
//   - The index arrives in EAX (register argument). TEST EAX,EAX / JNZ: a zero index
//     short-circuits and returns the shared empty-string literal g_str_empty (0x30074a0c,
//     the "" heap constant used as a cvar/string default across the cgame).
//   - Otherwise it forms cfgIndex = index + CS_LOCALIZED_STRINGS (0x575 = 1397), the base of the
//     hud-element config-string range.
//   - It runs the shared inlined CG_ConfigString bounds check: if cfgIndex is < 0 (JL)
//     or >= MAX_CONFIGSTRINGS (0x800), it reports
//     Com_ErrorMessage("CG_ConfigString: bad index: %i", cfgIndex) and (as with every
//     inlined CG_ConfigString site) proceeds with the lookup regardless.
//   - It resolves the config string &cg_gameState.stringData[cg_gameState.stringOffsets[cfgIndex]]
//     (offsets table at 0x30440a00, data heap at 0x30442a00).
//   - It returns CG_TranslateMessage(str, "hudelem string") — the cgame config-string
//     localize/text-format service (trap 57) with the fixed "hudelem string"
//     diagnostic label (cg_hudElemLocalizationContext).
//
// Name adjudication: the .mcode size-match guess "G_InitTurrets" (win size 0x47 ==
// matched 0x47) is REJECTED. The body is a getter that returns a default empty string
// on a null index and otherwise resolves + localizes a hud-element config string; it
// performs no turret initialization. The behavioral name follows the "hudelem string"
// diagnostic label, the CS_LOCALIZED_STRINGS (0x575) config-string base, and the sole caller
// (the hud-element string-building dispatcher 0x30029c00, which stores the result as
// the element's display string).
//
// Register-argument ABI: the index arrives in EAX and is read at entry (TEST EAX,EAX)
// before any stack traffic; modeled as the first C parameter. The two words pushed for
// CG_TranslateMessage (and the two for Com_ErrorMessage) are cleaned by the ADD ESP,8 after each
// call, and the function ends in a plain RET.
//
// Instruction map:
//   3002d800 TEST EAX,EAX
//   3002d802 JNZ  0x3002d80a               -> index != 0
//   3002d804 MOV  EAX,0x30074a0c           return g_str_empty ("")
//   3002d809 RET
//   3002d80a LEA  ESI,[EAX+0x575]          cfgIndex = index + CS_LOCALIZED_STRINGS
//   3002d811 TEST ESI,ESI / JL             cfgIndex < 0  -> error path
//   3002d815 CMP  ESI,0x800 / JL           cfgIndex < MAX_CONFIGSTRINGS -> skip error
//   3002d81d PUSH ESI / PUSH 0x30077d90 / CALL 0x3002b300 / ADD ESP,8
//                                          Com_ErrorMessage(cg_configStringBadIndexFmt, cfgIndex)
//   3002d82b MOV  EAX,[ESI*4 + 0x30440a00] EAX = cg_gameState.stringOffsets[cfgIndex]
//   3002d832 ADD  EAX,0x30442a00           EAX = &cg_gameState.stringData[offset]
//   3002d837 PUSH 0x30077b18 / PUSH EAX / CALL 0x3002d850 / ADD ESP,8
//                                          CG_TranslateMessage(str, "hudelem string")
//   3002d845 POP ESI / RET                 return the localized string
//
const char *CG_SafeTranslateHudElemString(int index /* EAX */)
{
    int cfgIndex;
    const char *str;

    if (index == 0) {
        return g_str_empty;
    }

    cfgIndex = coduo_int32_from_bits((uint32_t)index +
                                (uint32_t)CS_LOCALIZED_STRINGS);
    if (cfgIndex < 0 || cfgIndex >= MAX_CONFIGSTRINGS) {
        /* Shared inlined CG_ConfigString bounds report; the lookup still proceeds
         * with the out-of-range index, matching every other inlined site. */
        Com_ErrorMessage(cg_configStringBadIndexFmt, cfgIndex);
    }

    str = &cg_gameState.stringData[cg_gameState.stringOffsets[cfgIndex]];
    return CG_TranslateMessage(str, cg_hudElemLocalizationContext);
}
