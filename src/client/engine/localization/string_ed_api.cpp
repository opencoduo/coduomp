#include "string_ed_api.h"
#include "string_ed_package.hpp"

#include "client/engine/platform/crt_boundary.h"
#include "filesystem/filesystem.h"
#include "client/engine/q_shared.h"
#include "qcommon/com_sprintf.h"
#include "qcommon/q_string.h"

#include <cstddef>
#include <cstring>
#include <set>

enum {
    STRINGED_COMBINED_REFERENCE_SIZE = 256,
    STRINGED_PARSE_LINE_SIZE = 16384,
    SEH_LANGUAGE_COUNT = 14,
    SEH_MULTIBYTE_LANGUAGE_FIRST = 8,
    SEH_MULTIBYTE_LANGUAGE_LAST = 12,
    SEH_CVAR_ARCHIVE = 1,
    SEH_CVAR_LATCH = 32,
    SEH_LOCALIZED_MESSAGE_BUFFER_COUNT = 2,
    SEH_LOCALIZED_SEPARATOR = 0x14,
    SEH_PLAIN_SEPARATOR = 0x15,
    SEH_ESCAPE_CHAR = 0x16,
    SEH_FORMAT_INSERT_LENGTH = 2
};

/* The Windows table at 0x005ca678 and Mac table both use an 0x08 stride.
 * Name lookup reads +0x00; Mac FS_LanguageHasAssets and both client consumers
 * prove the qboolean asset-availability result at +0x04. */
typedef struct seh_language_info_s {
    const char *name;
    qboolean hasAssets;
} seh_language_info_t;

#if UINTPTR_MAX == UINT32_MAX
static_assert(alignof(seh_language_info_t) == 0x04,
              "i386 language-info alignment changed");
static_assert(offsetof(seh_language_info_t, name) == 0x00,
              "i386 language-info name offset changed");
static_assert(sizeof(((seh_language_info_t *)0)->name) == 0x04,
              "i386 language-info name extent changed");
static_assert(offsetof(seh_language_info_t, hasAssets) == 0x04,
              "i386 language-info asset flag offset changed");
static_assert(sizeof(((seh_language_info_t *)0)->hasAssets) == 0x04,
              "i386 language-info asset flag extent changed");
static_assert(sizeof(seh_language_info_t) == 0x08,
              "i386 language-info size changed");
#endif

/* Original table at 0x005ca678. PE_RELOCATION_VALUES_VERIFIED: all fourteen
 * language-name targets match the PE; every flag begins false. */
static seh_language_info_t sehLanguages[SEH_LANGUAGE_COUNT] = {
    {"english", qfalse},
    {"french", qfalse},
    {"german", qfalse},
    {"italian", qfalse},
    {"spanish", qfalse},
    {"british", qfalse},
    {"russian", qfalse},
    {"polish", qfalse},
    {"korean", qfalse},
    {"taiwanese", qfalse},
    {"japanese", qfalse},
    {"chinese", qfalse},
    {"thai", qfalse},
    {"leet", qfalse}
};

extern "C" {
cvar_t *cl_language;                  /* original 0x04e19994 */
cvar_t *cl_languagesavailable;        /* original 0x04dc8830 */
cvar_t *cl_languagetranslate;         /* original 0x0495819c */
cvar_t *cl_languagewarnings;          /* original 0x04e1999c */
cvar_t *cl_languagewarningsaserrors;  /* original 0x04958068 */
qboolean rendererMultibyteTextEnabled; /* original 0x009d5fac */
}

/* Original rotating storage at 0x038b50a0/0x038b51f8 and safe-translation
 * storage at 0x038b59f8. */
static int32_t sehLocalizedMessageBufferIndex;
static char sehLocalizedMessageBuffers
    [SEH_LOCALIZED_MESSAGE_BUFFER_COUNT]
    [MAX_STRING_CHARS];
static char
    sehSafeTranslateBuffer[MAX_STRING_CHARS];

/* Source: CoDUOMP.exe 0x00470150..0x00470158.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470150_00470159.mcode.
 * Name and signature: exact same-module Mac symbol SEH_GetCurrentLanguage. */
extern "C" int32_t SEH_GetCurrentLanguage(void)
{
    return cl_language->integer;
}

/* Source: CoDUOMP.exe 0x00470160..0x00470176.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470160_00470177.mcode.
 * Name and signature: exact same-module Mac symbol SEH_GetLanguageName.
 * Invalid indices deliberately fall back to English. */
extern "C" const char *SEH_GetLanguageName(int32_t languageIndex)
{
    if (languageIndex < 0 || languageIndex >= SEH_LANGUAGE_COUNT)
        languageIndex = 0;
    return sehLanguages[languageIndex].name;
}

/* Source: CoDUOMP.exe 0x00470180..0x004701ba.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470180_004701bb.mcode.
 * Name and signature: exact same-module Mac symbol
 * SEH_GetLanguageIndexForName. */
extern "C" qboolean SEH_GetLanguageIndexForName(
    const char *name, int32_t *languageIndex)
{
    for (int32_t index = 0; index < SEH_LANGUAGE_COUNT; ++index) {
        if (Q_stricmp(name, sehLanguages[index].name) == 0) {
            *languageIndex = index;
            return qtrue;
        }
    }

    *languageIndex = 0;
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004701c0..0x00470268.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004701c0_00470269.mcode.
 * Name: exact same-module Mac symbol SEH_InitLanguage. */
extern "C" void SEH_InitLanguage(void)
{
    cl_language = Cvar_Get(
        "cl_language", "0", SEH_CVAR_ARCHIVE | SEH_CVAR_LATCH);
    cl_languagesavailable =
        Cvar_Get("cl_languagesavailable", "0", 0);
    cl_languagetranslate =
        Cvar_Get("cl_languagetranslate", "1", SEH_CVAR_LATCH);
    cl_languagewarnings =
        Cvar_Get("cl_languagewarnings", "0", 0);
    cl_languagewarningsaserrors =
        Cvar_Get("cl_languagewarningsaserrors", "0", 0);
    (void)Cvar_Get(
        "cl_language", "0", SEH_CVAR_ARCHIVE | SEH_CVAR_LATCH);

    rendererMultibyteTextEnabled =
        cl_language->integer >= SEH_MULTIBYTE_LANGUAGE_FIRST &&
        cl_language->integer <= SEH_MULTIBYTE_LANGUAGE_LAST
            ? qtrue
            : qfalse;
}

/* Source: CoDUOMP.exe 0x004703e0..0x0047041e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004703e0_0047041f.mcode.
 * Name and return role: exact same-module Mac symbol
 * SEH_UpdateCurrentLanguage. The first available language at or after the
 * requested index is selected with wraparound. */
extern "C" int32_t SEH_UpdateCurrentLanguage(int32_t languageIndex)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (languageIndex < 0 || languageIndex >= SEH_LANGUAGE_COUNT)
        languageIndex = 0;

    if (sehLanguages[languageIndex].hasAssets != qfalse)
        return languageIndex;

    for (int32_t offset = 0; offset < SEH_LANGUAGE_COUNT; ++offset) {
        const int32_t candidate =
            (languageIndex + offset) % SEH_LANGUAGE_COUNT;
        if (sehLanguages[candidate].hasAssets != qfalse)
            return candidate;
    }
    return 0;
}

/* Source: CoDUOMP.exe 0x00470420..0x0047042e.
 * Name and behavior: same-module Mac symbol SE_ShutDown. The Windows body
 * carries the global package pointer in EBX and calls Clear(0). */
void SE_ShutDown()
{
    stringEdPackage.Clear(0);
}

/* Source: CoDUOMP.exe 0x00470430..0x0047043e.
 * Name and behavior: same-module Mac symbol SE_Init. The two operations are
 * intentionally identical; both reset all package and flag state. */
void SE_Init()
{
    stringEdPackage.Clear(0);
}

/* Source: CoDUOMP.exe 0x00470450..0x0047055d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470450_0047055e.mcode.
 * Name and boolean result: exact same-module Mac symbol
 * SEH_VerifyLanguageSelection. */
extern "C" qboolean SEH_VerifyLanguageSelection(int32_t languageIndex)
{
    SE_NewLanguage();
    if (SE_GetNumLanguages() == 0) {
        if (fs_ignoreLocalized->integer == 0 &&
            cl_languagewarnings->integer != 0) {
            const char *const languageName =
                SEH_GetLanguageName(languageIndex);
            if (cl_languagewarningsaserrors->integer != 0) {
                Com_Error(
                    ERR_LOCALIZATION,
                    "No language string information available for %s",
                    languageName);
            } else {
                Com_Printf(
                    "^3WARNING: No language string information available "
                    "for %s\n",
                    languageName);
            }
        }
        return qfalse;
    }

    const char *const languageName =
        SEH_GetLanguageName(languageIndex);
    const char *const error = SE_LoadLanguage(languageName, 1);
    if (error == nullptr)
        return qtrue;

    if (fs_ignoreLocalized->integer == 0 &&
        cl_languagewarnings->integer != 0) {
        if (cl_languagewarningsaserrors->integer != 0) {
            Com_Error(
                ERR_LOCALIZATION,
                "Could not load localization strings for %s: %s",
                languageName, error);
        } else {
            Com_Printf(
                "^3WARNING: Could not load localization strings for %s: "
                "%s\n",
                languageName, error);
        }
    }
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00470270..0x004703df.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470270_004703e0.mcode.
 * Name: exact same-module Mac symbol SEH_UpdateLanguageInfo. Localized
 * filesystem searchpaths prove which language asset sets are installed. */
extern "C" void SEH_UpdateLanguageInfo(void)
{
    (void)Cvar_Get(
        "cl_language", "0", SEH_CVAR_ARCHIVE | SEH_CVAR_LATCH);
    rendererMultibyteTextEnabled =
        cl_language->integer >= SEH_MULTIBYTE_LANGUAGE_FIRST &&
        cl_language->integer <= SEH_MULTIBYTE_LANGUAGE_LAST
            ? qtrue
            : qfalse;

    int32_t availableCount = 0;
    for (int32_t languageIndex = 0;
         languageIndex < SEH_LANGUAGE_COUNT;
         ++languageIndex) {
        qboolean hasAssets = qfalse;
        for (searchpath_t *search = fs_searchpaths;
             search != nullptr;
             search = search->next) {
            if (search->localized != qfalse &&
                search->language == languageIndex) {
                hasAssets = qtrue;
                ++availableCount;
                break;
            }
        }
        sehLanguages[languageIndex].hasAssets = hasAssets;
    }

    if (availableCount < 1) {
        Com_Printf(
            "^1ERROR: No languages available because no localized assets "
            "were found\n");
    }
    (void)Cvar_Set2(
        "cl_languagesavailable", va("%i", availableCount), qtrue);

    int32_t languageIndex = cl_language->integer;
    if (SEH_VerifyLanguageSelection(languageIndex) != qfalse)
        return;

    for (languageIndex = 0;
         languageIndex < SEH_LANGUAGE_COUNT;
         ++languageIndex) {
        if (sehLanguages[languageIndex].hasAssets == qfalse)
            continue;

        (void)Cvar_Set2(
            "cl_language", va("%i", languageIndex), qtrue);
        (void)Cvar_Get(
            "cl_language", "0",
            SEH_CVAR_ARCHIVE | SEH_CVAR_LATCH);
        rendererMultibyteTextEnabled =
            cl_language->integer >= SEH_MULTIBYTE_LANGUAGE_FIRST &&
            cl_language->integer <= SEH_MULTIBYTE_LANGUAGE_LAST
                ? qtrue
                : qfalse;
        if (SEH_VerifyLanguageSelection(languageIndex) != qfalse)
            return;
    }

    (void)Cvar_Set2("cl_language", "0", qtrue);
    (void)Cvar_Get(
        "cl_language", "0", SEH_CVAR_ARCHIVE | SEH_CVAR_LATCH);
    rendererMultibyteTextEnabled =
        cl_language->integer >= SEH_MULTIBYTE_LANGUAGE_FIRST &&
        cl_language->integer <= SEH_MULTIBYTE_LANGUAGE_LAST
            ? qtrue
            : qfalse;
}

/* Source: CoDUOMP.exe 0x004727c0..0x004728e9.
 * Name/signature: same-module Mac symbol SE_Load_Actual(const char *, int,
 * int).  The second integer controls debug strings; the third suppresses only
 * the missing-file error used by SE_Load's optional companion attempt. */
const char *SE_Load_Actual(const char *filename, int32_t loadDebug,
                           int32_t quiet)
{
    void *fileAllocation = nullptr;
    const int32_t fileLength = FS_ReadFile(filename, &fileAllocation);
    if (fileLength <= 0 || fileAllocation == nullptr) {
        if (quiet != 0)
            return nullptr;
        return va("Unable to load \"%s\"!", filename);
    }

    stringEdPackage.SetupNewFileParse(filename, loadDebug);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    stringEdPackage.endMarkerFound = 0;

    const char *fileCursor = static_cast<const char *>(fileAllocation);
    const char *const fileEnd = fileCursor + fileLength;
    const char *parseError = nullptr;
    char line[STRINGED_PARSE_LINE_SIZE];
    while (*fileCursor != '\0') {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        const std::size_t remainingLength =
            static_cast<std::size_t>(fileEnd - fileCursor);
        const std::size_t scanLength =
            remainingLength < sizeof(line) ? remainingLength : sizeof(line);
        const char *const newline = static_cast<const char *>(
            std::memchr(fileCursor, '\n', scanLength));
        if (newline == nullptr && remainingLength >= sizeof(line)) {
            parseError = va("Line exceeds %i bytes in \"%s\"!",
                            STRINGED_PARSE_LINE_SIZE - 1, filename);
            break;
        }
        if (!stringEdPackage.ReadLine(fileCursor, line)) {
            break;
        }
        if (std::strlen(line) != 0) {
            parseError = stringEdPackage.ParseLine(line);
            if (parseError != nullptr)
                break;
        }
    }

    FS_FreeFile(fileAllocation);
    if (parseError != nullptr)
        return parseError;
    /* The per-file reset above makes this check describe the file just read. */
    if (stringEdPackage.endMarkerFound == 0) {
        return va("Truncated file, failed to find \"%s\" at file end!",
                  "ENDMARKER");
    }
    return nullptr;
}

/* Source: CoDUOMP.exe 0x004728f0..0x00472976.
 * Name/signature argument: same-module Mac symbol
 * SE_GetFoundFile(std::string &).  The semicolon-delimited input is consumed
 * in place and the next path is returned through the original 64-byte static
 * buffer. */
const char *SE_GetFoundFile(std::string &foundFiles)
{
    static char foundFile[MAX_QPATH];

    if (foundFiles.empty())
        return nullptr;

    std::strncpy(foundFile, foundFiles.c_str(), sizeof(foundFile) - 1);
    foundFile[sizeof(foundFile) - 1] = '\0';
    char *separator = std::strchr(foundFile, ';');
    if (separator != nullptr) {
        *separator = '\0';
        foundFiles.erase(0, (size_t)(separator - foundFile) + 1);
    } else {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        foundFiles.erase(0, std::string::npos);
    }
    return foundFile;
}

/* Source: CoDUOMP.exe 0x00472980..0x00472a1e.
 * Name/signature: same-module Mac symbol SE_Load(const char *, int).  After
 * the requested localization file loads successfully, the same basename is
 * loaded again with the optional .ste companion extension. A missing
 * companion is quiet; a primary-file error is returned without that attempt. */
const char *SE_Load(const char *filename, int32_t loadDebug)
{
    const char *error = SE_Load_Actual(filename, loadDebug, 0);
    if (error != nullptr)
        return error;

    char companionName[MAX_QPATH];
    std::strncpy(companionName, filename, sizeof(companionName) - 1);
    companionName[sizeof(companionName) - 1] = '\0';
    char *extension = std::strrchr(companionName, '.');
    if (extension == nullptr || std::strlen(extension) != sizeof(".str") - 1)
        return error;

    std::memcpy(extension, ".ste", sizeof(".ste"));
    return SE_Load_Actual(companionName, loadDebug, 1);
}

/* Source: CoDUOMP.exe 0x00472a70..0x00472b76.
 * Name/signature: same-module Mac symbol SE_GetString(const char *, int).
 * The first SE_Entry_s string is the active text; the bracketed debug string
 * is selected only when both the caller and the loaded package request it. */
const char *SE_GetString(const char *reference, int32_t useDebug)
{
    const auto found = stringEdPackage.entries.find(reference);
    if (found == stringEdPackage.entries.end())
        return nullptr;
    if (useDebug != 0 && stringEdPackage.loadDebug != 0)
        return found->second.debugText.c_str();
    return found->second.text.c_str();
}

/* Source: CoDUOMP.exe 0x00472a20..0x00472a6d, recovered from an exporter
 * function-boundary gap. The exact source name was not retained; this is the
 * package/reference overload of SE_GetString. */
const char *SE_GetStringByPackage(
    const char *package, const char *reference, int32_t useDebug)
{
    char combinedReference[STRINGED_COMBINED_REFERENCE_SIZE];
    const std::size_t packageLength = std::strlen(package);
    const std::size_t referenceLength = std::strlen(reference);
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (packageLength >= sizeof(combinedReference) ||
        referenceLength >
            sizeof(combinedReference) - packageLength - 2) {
        Com_Printf("WARNING: localization reference is too long\n");
        return nullptr;
    }
    Com_sprintf(combinedReference, sizeof(combinedReference), "%s_%s",
                package, reference);
    return SE_GetString(combinedReference, useDebug);
}

/* Source: CoDUOMP.exe 0x00470560..0x00470589.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470560_0047058a.mcode.
 * Name and signature: exact same-module Mac symbol SEH_StringEd_GetString.
 * Translation is bypassed for empty and one-character references. */
extern "C" const char *SEH_StringEd_GetString(const char *key)
{
    if (cl_languagetranslate != nullptr &&
        cl_languagetranslate->integer != 0 &&
        key[0] != '\0' && key[1] != '\0') {
        return SE_GetString(key, 0);
    }
    return key;
}

/* Source: CoDUOMP.exe 0x00470590..0x004706b0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470590_004706b1.mcode.
 * Name and signature: exact same-module Mac symbol
 * SEH_SafeTranslateString. Missing references share the original 1024-byte
 * return buffer and receive the visibly marked fallback only when language
 * warnings are enabled. */
extern "C" const char *SEH_SafeTranslateString(const char *reference)
{
    const char *translated = SEH_StringEd_GetString(reference);
    if (translated != nullptr)
        return translated;

    if (cl_languagewarnings->integer != 0) {
        if (cl_languagewarningsaserrors->integer != 0) {
            Com_Error(
                ERR_LOCALIZATION,
                "Could not translate exe string \"%s\"", reference);
        } else {
            Com_Printf(
                "^3WARNING: Could not translate exe string \"%s\"\n",
                reference);
        }

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        constexpr const char prefix[] = "^1UNLOCALIZED(^7";
        constexpr const char suffix[] = "^1)^7";
        constexpr int32_t referenceCapacity =
            MAX_STRING_CHARS - static_cast<int32_t>(sizeof(prefix)) -
            static_cast<int32_t>(sizeof(suffix)) + 1;
        Com_sprintf(sehSafeTranslateBuffer, sizeof(sehSafeTranslateBuffer),
                    "%s%.*s%s", prefix, referenceCapacity, reference, suffix);
    } else {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        Q_strncpyz(sehSafeTranslateBuffer, reference,
                   sizeof(sehSafeTranslateBuffer));
    }
    return sehSafeTranslateBuffer;
}

/* Source: CoDUOMP.exe 0x004706c0..0x00470770.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004706c0_00470771.mcode.
 * Name and four-argument signature: exact same-module Mac symbol
 * SEH_GetLocalizedTokenReference. LOCMSG_NOERR returns failure after the
 * diagnostic so callers can retain their original unlocalized message. */
extern "C" qboolean SEH_GetLocalizedTokenReference(
    const char *reference, char *output, const char *messageType,
    msgLocErrType_t errorType)
{
    const char *translated = SEH_StringEd_GetString(reference);
    if (translated == nullptr) {
        if (cl_languagewarnings != nullptr &&
            cl_languagewarnings->integer != 0) {
            if (cl_languagewarningsaserrors != nullptr &&
                cl_languagewarningsaserrors->integer != 0 &&
                errorType != LOCMSG_NOERR) {
                Com_Error(
                    ERR_LOCALIZATION,
                    "Could not translate part of %s: \"%s\"",
                    messageType, reference);
            } else {
                Com_Printf(
                    "^3WARNING: Could not translate part of %s: \"%s\"\n",
                    messageType, reference);
            }
            translated =
                va("^1UNLOCALIZED(^7%s^1)^7", reference);
        } else {
            translated = va("%s", reference);
        }

        if (errorType == LOCMSG_NOERR)
            return qfalse;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (std::strlen(translated) >= MAX_STRING_CHARS) {
        Com_Printf("%s too long when translated: \"%s\"\n", messageType, reference);
        if (errorType != LOCMSG_NOERR) {
            Com_Error(ERR_DROP, "%s too long when translated: \"%s\"", messageType, reference);
        }
        return qfalse;
    }

    /* The original byte loop also permits the translation-bypass case where
     * source and destination are the same fragment buffer. */
    if (output != translated)
        std::strcpy(output, translated);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00470780..0x00470a90.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470780_00470a91.mcode.
 * Name and signature: exact same-module Mac symbol
 * SEH_LocalizeTextMessage. The 0x14/0x15/0x16 protocol is also emitted by
 * the recovered server's Scr_ConstructMessageString: localized fragment,
 * plain fragment, and escaped format-insertion marker respectively. */
extern "C" const char *SEH_LocalizeTextMessage(
    const char *input, const char *messageType, msgLocErrType_t errorType)
{
    sehLocalizedMessageBufferIndex =
        (sehLocalizedMessageBufferIndex + 1) %
        SEH_LOCALIZED_MESSAGE_BUFFER_COUNT;
    char *const output =
        sehLocalizedMessageBuffers[sehLocalizedMessageBufferIndex];
    std::memset(
        output, 0, MAX_STRING_CHARS);

    int32_t outputLength = 0;
    int32_t pendingInsertions = 0;
    qboolean translateFragment = qtrue;
    qboolean allowFormatInsertion = qtrue;
    qboolean restoreEscapedPercents = qfalse;
    const char *cursor = input;

    while (*cursor != '\0') {
        const char *const fragmentStart = cursor;
        while (*cursor != '\0' &&
               *cursor != SEH_LOCALIZED_SEPARATOR &&
               *cursor != SEH_PLAIN_SEPARATOR &&
               *cursor != SEH_ESCAPE_CHAR) {
            ++cursor;
        }

        if (cursor > fragmentStart) {
            const std::size_t sourceFragmentLength =
                static_cast<std::size_t>(cursor - fragmentStart);
            char fragment[MAX_STRING_CHARS];
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (sourceFragmentLength >= sizeof(fragment)) {
                Com_Printf("%s contains an oversized fragment\n", messageType);
                if (errorType != LOCMSG_NOERR) {
                    Com_Error(ERR_DROP, "%s contains an oversized fragment", messageType);
                }
                return nullptr;
            }
            int32_t fragmentLength =
                static_cast<int32_t>(sourceFragmentLength);
            std::memcpy(fragment, fragmentStart,
                        sourceFragmentLength);
            fragment[fragmentLength] = '\0';

            if (translateFragment != qfalse) {
                if (SEH_GetLocalizedTokenReference(
                        fragment, fragment, messageType,
                        errorType) == qfalse) {
                    return nullptr;
                }
                fragmentLength =
                    static_cast<int32_t>(std::strlen(fragment));
            }

            qboolean fragmentIntroducedInsertion = qfalse;
            for (int32_t index = 0;
                 index < fragmentLength - 1; ++index) {
                if (fragment[index] != '%' ||
                    fragment[index + 1] != 's') {
                    continue;
                }

                if (allowFormatInsertion != qfalse &&
                    fragmentIntroducedInsertion == qfalse) {
                    ++pendingInsertions;
                    fragmentIntroducedInsertion = qtrue;
                } else {
                    fragment[index] = SEH_ESCAPE_CHAR;
                    restoreEscapedPercents = qtrue;
                }
            }

            const qboolean replacesInsertion =
                pendingInsertions -
                    (fragmentIntroducedInsertion != qfalse ? 1 : 0) != 0
                    ? qtrue : qfalse;
            const int32_t retainedOutputLength =
                outputLength -
                (replacesInsertion != qfalse ? SEH_FORMAT_INSERT_LENGTH : 0);
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (fragmentLength >
                MAX_STRING_CHARS - 1 - retainedOutputLength) {
                Com_Printf("%s too long when translated: \"%s\"\n", messageType, input);
                if (errorType != LOCMSG_NOERR) {
                    Com_Error(ERR_DROP, "%s too long when translated: \"%s\"", messageType, input);
                }
                return nullptr;
            }

            if (replacesInsertion != qfalse) {
                int32_t insertionOffset = 0;
                while (insertionOffset < outputLength - 1) {
                    if (output[insertionOffset] == '%' &&
                        output[insertionOffset + 1] == 's') {
                        break;
                    }
                    ++insertionOffset;
                }

                char trailingText[MAX_STRING_CHARS];
                std::strcpy(
                    trailingText,
                    &output[insertionOffset +
                            SEH_FORMAT_INSERT_LENGTH]);
                output[insertionOffset] = '\0';
                std::strcpy(&output[insertionOffset], fragment);
                std::strcpy(
                    &output[insertionOffset + fragmentLength],
                    trailingText);
                outputLength -= SEH_FORMAT_INSERT_LENGTH;
                --pendingInsertions;
            } else {
                std::strcpy(&output[outputLength], fragment);
            }

            outputLength += fragmentLength;
            allowFormatInsertion = qtrue;
        }

        allowFormatInsertion = qtrue;
        if (*cursor == SEH_LOCALIZED_SEPARATOR) {
            translateFragment = qtrue;
            ++cursor;
        } else if (*cursor == SEH_PLAIN_SEPARATOR) {
            translateFragment = qfalse;
            ++cursor;
        }
        if (*cursor == SEH_ESCAPE_CHAR) {
            ++cursor;
            allowFormatInsertion = qfalse;
        }
    }

    if (restoreEscapedPercents != qfalse) {
        for (int32_t index = 0; index < outputLength; ++index) {
            if (output[index] == SEH_ESCAPE_CHAR)
                output[index] = '%';
        }
    }
    return output;
}

/* NOT_FROM_ORIGINAL_SOURCE: C-linkage bridge for maintained C translation
 * units. The original optimized call invokes CStringEdPackage::Clear
 * directly on the global package object. */
extern "C" void SEH_StringEd_Clear(int32_t preserveFlagData)
{
    stringEdPackage.Clear(preserveFlagData);
}

/* Source: CoDUOMP.exe 0x00472bd0..0x00472c71.
 * Exact Mac name was not retained; the Windows body is the flag-word analogue
 * of SE_GetString, returning SE_Entry_s::flags or zero when absent. */
int32_t SE_GetStringFlags(const char *reference)
{
    const auto found = stringEdPackage.entries.find(reference);
    if (found == stringEdPackage.entries.end())
        return 0;
    return found->second.flags;
}

/* Source: CoDUOMP.exe 0x00472b80..0x00472bc5, recovered from an exporter
 * function-boundary gap. The exact source name was not retained; this is the
 * package/reference overload of SE_GetStringFlags. */
int32_t SE_GetStringFlagsByPackage(
    const char *package, const char *reference)
{
    char combinedReference[STRINGED_COMBINED_REFERENCE_SIZE];
    const std::size_t packageLength = std::strlen(package);
    const std::size_t referenceLength = std::strlen(reference);
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (packageLength >= sizeof(combinedReference) ||
        referenceLength >
            sizeof(combinedReference) - packageLength - 2) {
        Com_Printf("WARNING: localization reference is too long\n");
        return 0;
    }
    Com_sprintf(combinedReference, sizeof(combinedReference), "%s_%s",
                package, reference);
    return SE_GetStringFlags(combinedReference);
}

/* Source: CoDUOMP.exe 0x00472d00..0x00472d0a, recovered from an exporter
 * function-boundary gap. The exact source name was not retained; the body is
 * a global-package wrapper around CStringEdPackage::GetFlagMask. */
int32_t SE_GetFlagMask(const char *flagName)
{
    return stringEdPackage.GetFlagMask(flagName);
}

/* Source: CoDUOMP.exe 0x00472c80..0x00472ca5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00472c80_00472ca6.mcode.
 * Exact source name was not retained; the body returns the number of names in
 * CStringEdPackage::flagNames. */
int32_t SE_GetNumFlags()
{
    return static_cast<int32_t>(stringEdPackage.flagNames.size());
}

/* Source: CoDUOMP.exe 0x00472cb0..0x00472cfe.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00472cb0_00472cff.mcode.
 * Exact source name was not retained; the body returns the requested flag
 * name, or the shared empty string for a negative/out-of-range index. */
const char *SE_GetFlagName(int32_t flagIndex)
{
    if (flagIndex < 0 ||
        static_cast<size_t>(flagIndex) >= stringEdPackage.flagNames.size()) {
        return "";
    }
    return stringEdPackage.flagNames[static_cast<size_t>(flagIndex)].c_str();
}

/* Original counter at 0x009d5fb0.  Both callers reset it before recursive
 * enumeration and SE_R_ListFiles increments it once per appended file. */
static int32_t seRecursiveFileCount;

/* Source: CoDUOMP.exe 0x004781a0..0x004781cf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004781a0_004781d0.mcode.
 * Name and signature: exact same-module Mac symbol SE_LoadFileData. */
unsigned char *SE_LoadFileData(const char *filename, int32_t *fileLength)
{
    void *fileData;

    if (fileLength != nullptr)
        *fileLength = 0;

    const int32_t length = FS_ReadFile(filename, &fileData);
    if (length <= 0)
        return nullptr;

    if (fileLength != nullptr)
        *fileLength = length;
    return static_cast<unsigned char *>(fileData);
}

/* Source: CoDUOMP.exe 0x004781d0..0x004781db.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004781d0_004781dc.mcode.
 * Name and signature: exact same-module Mac symbol
 * SE_FreeFileDataAfterLoad. */
void SE_FreeFileDataAfterLoad(unsigned char *fileData)
{
    if (fileData != nullptr)
        FS_FreeFile(fileData);
}

/* Source: CoDUOMP.exe 0x004781e0..0x0047833f.
 * Name/signature: same-module Mac symbol
 * SE_R_ListFiles(const char *, const char *, std::string &). */
void SE_R_ListFiles(const char *path, const char *extension,
                    std::string &foundFiles)
{
    int32_t directoryCount;
    char **directories =
        FS_ListFilteredFiles(path, "/", nullptr, &directoryCount);

    for (int32_t directoryIndex = 0;
         directoryIndex < directoryCount; ++directoryIndex) {
        const char *directory = directories[directoryIndex];
        if (directory[0] == '\0' || directory[0] == '.')
            continue;

        char childPath[MAX_QPATH];
        const std::size_t pathLength = std::strlen(path);
        const std::size_t directoryLength = std::strlen(directory);
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        if (pathLength >= sizeof(childPath) ||
            directoryLength > sizeof(childPath) - pathLength - 2) {
            Com_Printf("WARNING: ignoring overlong localization directory path\n");
            continue;
        }
        Com_sprintf(childPath, sizeof(childPath), "%s/%s", path,
                    directory);
        SE_R_ListFiles(childPath, extension, foundFiles);
    }

    int32_t fileCount;
    char **files =
        FS_ListFilteredFiles(path, extension, nullptr, &fileCount);
    for (int32_t fileIndex = 0; fileIndex < fileCount; ++fileIndex) {
        char filePath[MAX_QPATH];
        const std::size_t pathLength = std::strlen(path);
        const std::size_t fileNameLength = std::strlen(files[fileIndex]);
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        if (pathLength >= sizeof(filePath) ||
            fileNameLength > sizeof(filePath) - pathLength - 2) {
            Com_Printf("WARNING: ignoring overlong localization file path\n");
            continue;
        }
        Com_sprintf(filePath, sizeof(filePath), "%s/%s", path,
                    files[fileIndex]);
        foundFiles.append(filePath);
        foundFiles += ';';
        ++seRecursiveFileCount;
    }

    if (files != nullptr)
        FS_FreeFileList(files);
    if (directories != nullptr)
        FS_FreeFileList(directories);
}

/* Source: CoDUOMP.exe 0x00478340..0x00478370.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00478340_00478371.mcode.
 * Name and arguments: exact same-module Mac symbol SE_BuildFileList. The
 * Windows return register proves that the helper returns the number of paths
 * appended to the semicolon-delimited string. */
int32_t SE_BuildFileList(const char *path, std::string &foundFiles)
{
    seRecursiveFileCount = 0;
    foundFiles.clear();
    SE_R_ListFiles(path, ".str", foundFiles);
    return seRecursiveFileCount;
}

/* Source: CoDUOMP.exe 0x00472d10..0x00473198.
 * Name: same-module Mac symbol SE_GetNumLanguages.  The Windows optimizer
 * inlined the Mac build's SE_BuildFileList helper into this function. */
int32_t SE_GetNumLanguages()
{
    /* 0x00472d38..0x00472d76 destroys the prior elements, releases the
     * allocation, and returns all three vector pointers to zero. */
    std::vector<std::string>().swap(seFoundFiles);

    std::string foundFiles;
    (void)SE_BuildFileList("localizedstrings", foundFiles);

    std::set<std::string> foundLanguages;
    for (const char *foundFile = SE_GetFoundFile(foundFiles);
         foundFile != nullptr;
         foundFile = SE_GetFoundFile(foundFiles)) {
        const std::string language(
            stringEdPackage.ExtractLanguageFromPath(foundFile));
        if (!foundLanguages.insert(language).second)
            continue;

        if (coduo_crt_stricmp(language.c_str(), "english") == 0)
            seFoundFiles.insert(seFoundFiles.begin(), language);
        else
            seFoundFiles.push_back(language);
    }

    return static_cast<int32_t>(seFoundFiles.size());
}

/* Source: CoDUOMP.exe 0x004731d0..0x0047321e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004731d0_0047321f.mcode.
 * Exact source name was not retained; the body returns one language gathered
 * by SE_GetNumLanguages, or the shared empty string for an invalid index. */
const char *SE_GetLanguageName(int32_t languageIndex)
{
    if (languageIndex < 0 ||
        static_cast<size_t>(languageIndex) >= seFoundFiles.size()) {
        return "";
    }
    return seFoundFiles[static_cast<size_t>(languageIndex)].c_str();
}

/* Source: CoDUOMP.exe 0x00473220..0x00473280.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00473220_00473281.mcode.
 * Exact source name was not retained; valid entries are formatted as
 * "localizedstrings/<language>" through the original rotating va buffer. */
const char *SE_GetLanguagePath(int32_t languageIndex)
{
    if (languageIndex < 0 ||
        static_cast<size_t>(languageIndex) >= seFoundFiles.size()) {
        return "";
    }
    return va("%s/%s", "localizedstrings",
              seFoundFiles[static_cast<size_t>(languageIndex)].c_str());
}

/* Source: CoDUOMP.exe retained tail thunk 0x00470440..0x00470444 and body
 * 0x00473290..0x004732f0. Name/signature: same-module Mac symbol
 * SE_NewLanguage(void). MSVC routes the retained public entry to the same
 * out-of-line body used by internal callers. */
void SE_NewLanguage()
{
    stringEdPackage.Clear(1);
}

/* Source: CoDUOMP.exe 0x00473320..0x004734a4.
 * Name/signature: same-module Mac symbol SE_LoadLanguage(const char *, int).
 * The first parse error stops the scan and is returned to the caller. */
const char *SE_LoadLanguage(const char *language, int32_t loadDebug)
{
    if (language == nullptr || language[0] == '\0')
        return nullptr;

    SE_NewLanguage();

    std::string foundFiles;
    (void)SE_BuildFileList("localizedstrings", foundFiles);

    const char *error = nullptr;
    for (const char *foundFile = SE_GetFoundFile(foundFiles);
         foundFile != nullptr && error == nullptr;
         foundFile = SE_GetFoundFile(foundFiles)) {
        const char *fileLanguage =
            stringEdPackage.ExtractLanguageFromPath(foundFile);
        if (coduo_crt_stricmp(language, fileLanguage) == 0)
            error = SE_Load(foundFile, loadDebug);
    }
    return error;
}
