#include "../client_recovered.h"
#include "../globals.h"

#include <string.h>

// Source: uo_cgame_mp_x86.dll 0x300310b0..0x300311e1
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300310b0_300311e1.mcode
//
// CG_GetTranslatedLocationString — resolve the localized display text for a map
// location, given its location index. It first resolves the raw location config
// string by an INLINED CG_ConfigString lookup at index (CS_LOCATIONS + locationIndex):
//   cfgIndex = locationIndex + 53
//   if (cfgIndex < 0 || cfgIndex >= MAX_CONFIGSTRINGS)
//       Com_ErrorMessage("CG_ConfigString: bad index: %i", cfgIndex);   // 0x30077d90
//   raw = &cg_gameState.stringData[cg_gameState.stringOffsets[cfgIndex]];
//   if (raw == NULL || raw[0] == '\0') raw = "CGAME_UNKNOWN";           // 0x300797d0
// This is the same table/base pair and out-of-range diagnostic as CG_ConfigString
// (0x3002c990: table 0x30440a00, data base 0x30442a00, bound 0x800). The +53 offset
// selects the location config-string range (CS_LOCATIONS), which begins immediately
// after the head-icon range (UO_CONFIGSTRING_HEAD_ICON_BASE 38 + COUNT 15 = 53); the
// "map location string" diagnostic .rdata strings confirm the domain.
//
// It then asks the engine to translate the raw location string via
// cgame_syscall(CG_SE_TRANSLATE_REFERENCE /* 0x38 */, raw). On a hit (non-null) that translated
// pointer is returned directly. On a miss (0), the behavior mirrors the sibling
// CG_GetTranslatedVoiceChatString (0x3003a150) exactly:
//   - cl_languagewarnings_vmCvar.integer == 0: return a plain strcpy of the raw string in
//     the static result buffer cg_translatedLocationString.
//   - cl_languagewarnings_vmCvar.integer != 0: emit a diagnostic — a fatal BG_AnimParseError(7,...)
//     when cl_languagewarningsaserrors_vmCvar.integer is set, otherwise a Com_Printf warning — then
//     return an "^1UNLOCALIZED(^7" + raw + "^1)^7" placeholder in that same buffer.
//
// Naming: adopted from the same-module (cgame_mp) PPC name bank
// (CG_GetTranslatedLocationString, alongside CG_GetTranslatedVoiceChatString), and
// corroborated by the two hardcoded .rdata "map location string" diagnostics
// (0x300797a0 error, 0x30079764 warning) and the shared translate-failure idiom.
// The .mcode size-guess name "Menus_RemoveFromStack" is REJECTED: it is a pure
// size-only corpus match (win size 0x131 vs 0x130) with no behavioral basis — this
// function performs a config-string localization lookup and builds an UNLOCALIZED
// placeholder, it does nothing with a menu stack.
//
// ABI: the location index arrives in EAX (register calling convention); both RET
// paths are plain (caller-cleaned, no stack args consumed). Returns a char *
// (either the engine-owned translated string or the static result buffer).
//
// Com_Error is called with level 7: a literal com_error code the standard ERR_*
// enum in this build does not cover, so it is passed as the raw literal 7 rather
// than a named ERR_ constant (PUSH 0x7 at 0x30031117).

/* .rdata string constants used to decorate an untranslated location reference.
 * 0x30077b38 = "^1UNLOCALIZED(^7" (prefix, 17 bytes), 0x30077b30 = "^1)^7" (suffix,
 * 6 bytes). The original inlined strcpy(prefix)/strcat(raw)/strcat(suffix); the
 * compiler emitted the 17-byte prefix copy as register moves
 * (0x30031130..0x30031164) and the 6-byte suffix as a dword+word store
 * (0x300311a8..0x300311b6). */
static const char CG_UNLOCALIZED_PREFIX[] = "^1UNLOCALIZED(^7";
static const char CG_UNLOCALIZED_SUFFIX[] = "^1)^7";

_Static_assert(
    sizeof(cg_translatedLocationString) >=
        sizeof(CG_UNLOCALIZED_PREFIX) + sizeof(CG_UNLOCALIZED_SUFFIX) - 1,
    "translated location buffer must hold its decoration and terminator");

/* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
static qboolean cgame_compat_locationTruncationReported = qfalse;

const char *CG_GetTranslatedLocationString(int32_t locationIndex)
{
    const char *raw;

    /* 0x300310b1..0x300310be: cfgIndex = locationIndex + 53; the JS/CMP guard
     * treats it as signed and errors when < 0 or >= MAX_CONFIGSTRINGS. */
    int32_t cfgIndex = coduo_int32_from_bits(
        (uint32_t)locationIndex + (uint32_t)CS_LOCATIONS);
    if (cfgIndex < 0 || cfgIndex >= MAX_CONFIGSTRINGS) {
        /* 0x300310c0..0x300310cb: Com_ErrorMessage("CG_ConfigString: bad index: %i",
         * cfgIndex). The lookup below still proceeds unbounded, exactly as in
         * CG_ConfigString (0x3002c990). */
        Com_ErrorMessage("CG_ConfigString: bad index: %i", cfgIndex);
    }

    /* 0x300310ce..0x300310e5: raw = &cg_gameState.stringData[
     *     cg_gameState.stringOffsets[cfgIndex]].  The JZ at 0x300310db tests the
     * post-ADD pointer for null (preserved from the machine code though the nonzero
     * data base makes it effectively unreachable); the CMP byte [ESI],0 tests for an
     * empty string. Either case falls through to the "CGAME_UNKNOWN" placeholder. */
    raw = &cg_gameState.stringData[cg_gameState.stringOffsets[cfgIndex]];
    if (raw == (const char *)0 || raw[0] == '\0') {
        /* 0x300310e2: raw = "CGAME_UNKNOWN". */
        raw = "CGAME_UNKNOWN";
    }

    /* 0x300310e7..0x300310f5: EAX = (int32_t)cgame_syscall(0x38, raw). If the engine has a
     * translation (nonzero), return it directly (JNZ 0x300311df -> POP ESI; RET,
     * EAX unchanged). */
    {
        const char *translated =
            (const char *)(intptr_t)cgame_syscall(CG_SE_TRANSLATE_REFERENCE,
                                                  (intptr_t)raw);
        if (translated != (const char *)0) {
            return translated;
        }
    }

    /* 0x300310fb..0x30031102: if cl_languagewarnings_vmCvar.integer is 0, just copy the raw
     * location string into the static buffer and return it (JZ 0x300311c2). */
    if (cl_languagewarnings_vmCvar.integer == 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        size_t rawLength = strlen(raw);
        size_t copyLength = rawLength;
        if (copyLength >= sizeof(cg_translatedLocationString)) {
            copyLength = sizeof(cg_translatedLocationString) - 1;
            if (cgame_compat_locationTruncationReported == qfalse) {
                cgame_compat_locationTruncationReported = qtrue;
                Com_Printf(
                    "^3WARNING: map location string truncated to fit "
                    "the client display buffer\n");
            }
        }
        memcpy(cg_translatedLocationString, raw, copyLength);
        cg_translatedLocationString[copyLength] = '\0';
        /* 0x300311da: EAX = &cg_translatedLocationString. */
        return cg_translatedLocationString;
    }

    /* 0x30031108..0x30031121: reporting enabled — emit the diagnostic. raw is the
     * %s argument in both branches (pushed once at 0x3003110f). */
    if (cl_languagewarningsaserrors_vmCvar.integer != 0) {
        /* 0x30031112..0x30031119: Com_Error(7, "Could not translate map
         * location string \"%s\"", raw). Level 7 is a raw com_error code not covered
         * by the ERR_* enum in this build (PUSH 0x7 at 0x30031117). */
        Com_Error(7, "Could not translate map location string \"%s\"", raw);
    } else {
        /* 0x30031123..0x3003112d: Com_Printf("^3WARNING: Could not translate map
         * location string \"%s\"\n", raw). */
        Com_Printf("^3WARNING: Could not translate map location string \"%s\"\n", raw);
    }

    /* 0x30031130..0x300311b6: build "^1UNLOCALIZED(^7" + raw + "^1)^7" in the static
     * result buffer. The prefix copy is an inlined strcpy (register moves), the raw
     * append and suffix append are inlined strcat (strlen-to-end + rep movs, then a
     * dword+word store for the 6-byte suffix). */
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    {
        const size_t prefixLength = sizeof(CG_UNLOCALIZED_PREFIX) - 1;
        const size_t suffixLength = sizeof(CG_UNLOCALIZED_SUFFIX) - 1;
        const size_t rawCapacity =
            sizeof(cg_translatedLocationString) - prefixLength -
            suffixLength - 1;
        const size_t rawLength = strlen(raw);
        size_t copyLength = rawLength;
        char *output = cg_translatedLocationString;

        if (copyLength > rawCapacity) {
            copyLength = rawCapacity;
            if (cgame_compat_locationTruncationReported == qfalse) {
                cgame_compat_locationTruncationReported = qtrue;
                Com_Printf(
                    "^3WARNING: map location string truncated to fit "
                    "the client display buffer\n");
            }
        }

        memcpy(output, CG_UNLOCALIZED_PREFIX, prefixLength);
        output += prefixLength;
        memcpy(output, raw, copyLength);
        output += copyLength;
        memcpy(output, CG_UNLOCALIZED_SUFFIX, suffixLength);
        output[suffixLength] = '\0';
    }

    /* 0x300311bb: EAX = &cg_translatedLocationString. */
    return cg_translatedLocationString;
}
