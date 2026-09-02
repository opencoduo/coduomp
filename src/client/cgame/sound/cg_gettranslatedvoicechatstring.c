#include "../client_recovered.h"
#include "../globals.h"

#include <string.h>

// Source: uo_cgame_mp_x86.dll 0x3003a150..0x3003a242
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003a150_3003a242.mcode
//
// CG_GetTranslatedVoiceChatString — resolve the localized display text for a voice
// chat string reference. It first asks the engine to translate the reference via
// cgame_syscall(CG_SE_TRANSLATE_REFERENCE /* 0x38 */, string); if the engine returns a non-null
// pointer that translated string is returned directly. On a miss (return 0) the
// behavior depends on the cl_languagewarnings_vmCvar.integer flag:
//   - flag == 0: silently return a plain strcpy of the input in the static result
//     buffer cg_translatedVoiceChatString.
//   - flag != 0: emit a diagnostic — fatal BG_AnimParseError(7,...) when
//     cl_languagewarningsaserrors_vmCvar.integer is set, otherwise a Com_Printf warning — and
//     return an "^1UNLOCALIZED(^7" + input + "^1)^7" decorated placeholder built in
//     the same static buffer.
//
// Naming: adopted from the same-module (cgame_mp) PPC name bank
// (CG_GetTranslatedVoiceChatString, in the voice-chat cluster) and corroborated by
// the two hardcoded .rdata error strings that name the subject "voice chat string",
// and by the sole caller CG_VoiceChat (0x3003a250). The .mcode size-guess name
// "G_Activate" (win size 0xf2, matched size 0xf2) is REJECTED: it is a pure
// size-only corpus match with no behavioral basis — this function performs a
// string-editor localization lookup and builds an UNLOCALIZED placeholder, it does
// not activate any game entity/mover. This is the voice-chat sibling of
// CG_SafeTranslateString_Internal (0x3002d6e0): same structure and shared gating globals
// (cl_languagewarnings_vmCvar.integer 0x30452aec, cl_languagewarningsaserrors_vmCvar.integer 0x30451b2c),
// but the domain here is fixed to voice chat and there is no format/domain argument.
//
// ABI: input string arrives in EAX (register calling convention). Returns a char *
// (either the engine-owned translated string or the static result buffer).
//
// Com_Error is called with level 7: this is a literal com_error code that the
// standard ERR_* enum in this build does not cover, so it is passed as the raw
// literal 7 rather than a named ERR_ constant (see the .mcode: PUSH 0x7 at
// 0x3003a183 ahead of CALL 0x3002b3d0).

/* .rdata string constants used to decorate an untranslated voice chat reference.
 * 0x30077b38 = "^1UNLOCALIZED(^7" (prefix), 0x30077b30 = "^1)^7" (suffix). The
 * original inlined strcpy(prefix) / strcat(input) / strcat(suffix); the compiler
 * emitted the 17-byte prefix copy as register moves (0x3003a19c..0x3003a1d0) and
 * the 6-byte suffix as a dword+word store (0x3003a20e..0x3003a21c). */
enum { CG_UNLOCALIZED_SUFFIX_LEN = 6 }; /* "^1)^7" + NUL, dword+word appended */

static const char CG_UNLOCALIZED_PREFIX[] = "^1UNLOCALIZED(^7";
static const char CG_UNLOCALIZED_SUFFIX[] = "^1)^7";

const char *CG_GetTranslatedVoiceChatString(const char *string)
{
    /* 0x3003a150..0x3003a161: EAX = (int32_t)cgame_syscall(0x38, string). If the engine has
     * a translation (nonzero), return it directly (JNZ 0x3003a240 -> POP ESI; RET,
     * EAX unchanged). */
    const char *translated =
        (const char *)(intptr_t)cgame_syscall(CG_SE_TRANSLATE_REFERENCE,
                                              (intptr_t)string);
    if (translated != (const char *)0) {
        return translated;
    }

    /* 0x3003a167..0x3003a16e: if cl_languagewarnings_vmCvar.integer is 0, just copy the
     * input into the static buffer and return it (JZ 0x3003a228). */
    if (cl_languagewarnings_vmCvar.integer == 0) {
        /* 0x3003a228..0x3003a239: inlined strcpy(cg_translatedVoiceChatString, string). */
        strcpy(cg_translatedVoiceChatString, string);
        /* 0x3003a23b: EAX = &cg_translatedVoiceChatString. */
        return cg_translatedVoiceChatString;
    }

    /* 0x3003a174..0x3003a199: reporting enabled — emit the diagnostic. */
    if (cl_languagewarningsaserrors_vmCvar.integer != 0) {
        /* 0x3003a17b..0x3003a18a: Com_Error(7, "Could not translate voice
         * chat string \"%s\"", string). Level 7 is a raw com_error code not covered
         * by the ERR_* enum in this build (PUSH 0x7 at 0x3003a183). */
        Com_Error(7, "Could not translate voice chat string \"%s\"", string);
    } else {
        /* 0x3003a18f..0x3003a199: Com_Printf("^3WARNING: Could not translate voice
         * chat string \"%s\"\n", string). */
        Com_Printf("^3WARNING: Could not translate voice chat string \"%s\"\n", string);
    }

    /* 0x3003a19c..0x3003a21c: build "^1UNLOCALIZED(^7" + string + "^1)^7" in the
     * static result buffer. The prefix copy is an inlined strcpy, the input append
     * and suffix append are inlined strcat (strlen-to-end + rep movs / dword+word). */
    strcpy(cg_translatedVoiceChatString, CG_UNLOCALIZED_PREFIX);
    strcat(cg_translatedVoiceChatString, string);
    strcat(cg_translatedVoiceChatString, CG_UNLOCALIZED_SUFFIX);

    /* 0x3003a221: EAX = &cg_translatedVoiceChatString. */
    return cg_translatedVoiceChatString;
}
