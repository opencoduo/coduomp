#include "../client_recovered.h"
#include "../globals.h"

#include <string.h>

// Source: uo_cgame_mp_x86.dll 0x3002d6e0..0x3002d7e2
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002d6e0_3002d7e2.mcode
//
// CG_SafeTranslateString_Internal — resolve the localized text for the string `reference`
// (a "^"-coded string-editor token such as "CGAME_HEALTH") within domain
// `domain` ("cgame"), returning a pointer either to the engine-owned translated
// string or to the static result buffer cg_translatedString.
//
// Control flow (matches the .mcode exactly):
//   1. 0x3002d6e0..0x3002d6f4: EAX = cgame_syscall(CG_SE_TRANSLATE_REFERENCE /* 0x38 */, reference).
//      Only the reference is pushed (ESI); on a nonzero return (JNZ 0x3002d7df ->
//      POP EDI; POP ESI; RET) the engine translation is returned directly.
//   2. 0x3002d6fa..0x3002d701: on a miss, read cl_languagewarnings_vmCvar.integer
//      (0x30452aec). If zero (JZ 0x3002d7c2) skip the diagnostic and just copy the
//      reference into the static buffer.
//   3. 0x3002d707..0x3002d72d: reporting enabled — select the diagnostic by
//      cl_languagewarningsaserrors_vmCvar.integer (0x30451b2c). Both messages are printed with
//      (domain, reference) as their two %s arguments (PUSH ESI=reference,
//      PUSH EDI=domain before the call):
//        - flag != 0 (JNZ falls through at 0x3002d710): fatal
//          Com_Error(7, "Could not translate %s string \"%s\"", domain,
//          reference). Level 7 is a raw com_error code (PUSH 0x7 at 0x3002d717)
//          not covered by the ERR_* enum in this build, so it is the literal 7.
//        - flag == 0 (JZ 0x3002d723): Com_Printf("^3WARNING: Could not translate
//          %s string \"%s\"\n", domain, reference).
//   4. 0x3002d730..0x3002d7b6: build "^1UNLOCALIZED(^7" + reference + "^1)^7" in
//      cg_translatedString: the 17-byte constant prefix at 0x30077b38 is copied by
//      inlined moves (0x3002d730..0x3002d764), the reference is appended with an
//      inlined strcat (strlen scan 0x3002d770 + rep movs 0x3002d78f/0x3002d796),
//      and the 6-byte constant suffix "^1)^7"+NUL from 0x30077b30/0x30077b34 is
//      appended as a dword+word store after re-scanning to the end. Returns
//      &cg_translatedString (0x3002d7bb).
//   5. 0x3002d7c2..0x3002d7da (report-disabled path): inlined strcpy of the
//      reference into cg_translatedString, then return &cg_translatedString.
//
// ABI (register calling convention, proven from every call site, e.g. 0x3002fca8
// EAX=0x30077b28="cgame", ECX=token): domain in EAX, reference in ECX. Returns
// char *. Callee-saved ESI/EDI restored on both RET paths; plain RET (no stack
// args). This is the domain-tagged sibling of CG_GetTranslatedVoiceChatString
// (0x3003a150) and CG_GetTranslatedLocationString (0x300310b0); they share the
// gating globals cl_languagewarnings_vmCvar.integer (0x30452aec) and
// cl_languagewarningsaserrors_vmCvar.integer (0x30451b2c). The mechanical size-guess name
// script_method_scriptbuiltin_sethintstring is REJECTED: it is a size-only corpus
// match with no behavioral basis — this function parses no script arguments and
// sets no hint string; it performs a string-editor localization lookup and builds
// an UNLOCALIZED placeholder.
//
// The prefix/suffix decoration strings are read from .rdata (MOV from 0x30077b38 /
// 0x30077b30 / 0x30077b34); modeled here as the C string literals the compiler
// inlined, matching the reconstruction of the two sibling functions.

static const char CG_UNLOCALIZED_PREFIX[] = "^1UNLOCALIZED(^7"; /* .rdata 0x30077b38, 17 bytes */
static const char CG_UNLOCALIZED_SUFFIX[] = "^1)^7";            /* .rdata 0x30077b30, 6 bytes  */

char *CG_SafeTranslateString_Internal(const char *domain, const char *reference)
{
    /* 0x3002d6e0..0x3002d6f4: EAX = (int32_t)cgame_syscall(0x38, reference). Nonzero return
     * is the engine-owned translated string; return it directly. */
    char *translated =
        (char *)(intptr_t)cgame_syscall(CG_SE_TRANSLATE_REFERENCE,
                                        reference);
    if (translated != (char *)0) {
        return translated;
    }

    /* 0x3002d6fa..0x3002d701: if cl_languagewarnings_vmCvar.integer is 0, skip the
     * diagnostic and just copy the reference into the static buffer. */
    if (cl_languagewarnings_vmCvar.integer != 0) {
        /* 0x3002d707..0x3002d72d: reporting enabled — pick the diagnostic. */
        if (cl_languagewarningsaserrors_vmCvar.integer != 0) {
            /* 0x3002d712..0x3002d71e: localization error; PUSH 0x7 at
             * 0x3002d717 is the shared ERR_LOCALIZATION value. */
            Com_Error(ERR_LOCALIZATION,
                      "Could not translate %s string \"%s\"",
                      domain, reference);
        } else {
            /* 0x3002d723..0x3002d72d: warning print. */
            Com_Printf("^3WARNING: Could not translate %s string \"%s\"\n",
                       domain, reference);
        }

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        const size_t prefixLength = sizeof(CG_UNLOCALIZED_PREFIX) - 1u;
        const size_t referenceCapacity = sizeof(cg_translatedString) -
                                         prefixLength -
                                         (sizeof(CG_UNLOCALIZED_SUFFIX) - 1u);
        memcpy(cg_translatedString, CG_UNLOCALIZED_PREFIX, prefixLength);
        Q_strncpyz(cg_translatedString + prefixLength, reference,
                   (int32_t)referenceCapacity);
        memcpy(cg_translatedString + strlen(cg_translatedString),
               CG_UNLOCALIZED_SUFFIX, sizeof(CG_UNLOCALIZED_SUFFIX));

        /* 0x3002d7bb: EAX = &cg_translatedString. */
        return cg_translatedString;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    Q_strncpyz(cg_translatedString, reference,
               (int32_t)sizeof(cg_translatedString));
    return cg_translatedString;
}
