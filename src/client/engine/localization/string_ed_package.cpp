#include "string_ed_package.hpp"

#include "client/engine/platform/crt_boundary.h"
#include "client/engine/q_shared.h"

#include <cstdlib>
#include <cstring>

enum {
    STRINGED_FILE_VERSION = 1,
    STRINGED_FLAG_MASK_BIT_COUNT = 32
};

static constexpr char STRINGED_LANGUAGE_PREFIX[] = "LANG_";

/* Source: CoDUOMP.exe 0x00471320..0x004713fa.
 * Class name and member-container types are corroborated by the same-module
 * Mac CStringEdPackage and STL-instantiation symbols.  The Windows body
 * default-constructs four strings, map<string, SE_Entry_s>, vector<string>,
 * and map<string, int32_t>, then calls Clear(0). */
CStringEdPackage::CStringEdPackage()
    : currentReference(), currentString(), currentPackage(), currentLanguage(),
      entries(), flagNames(), flagReferences()
{
    Clear(0);
}

/* Source: CoDUOMP.exe 0x00471400..0x00471551.
 * The explicit Clear(0) precedes the compiler-emitted reverse member
 * destruction in the original body. */
CStringEdPackage::~CStringEdPackage()
{
    Clear(0);
}

/* Source: CoDUOMP.exe 0x004715d0..0x0047168a.
 * Name/signature: same-module Mac symbol CStringEdPackage::Clear(int).
 * A nonzero argument preserves the flag-name vector and flag-reference map;
 * the entry map and current parse strings are always reset. */
void CStringEdPackage::Clear(int32_t preserveFlagData)
{
    entries.clear();
    if (preserveFlagData == 0) {
        /* 0x004715ff..0x00471636 destroys the rows, releases the allocation,
         * and zeros all three vector pointers rather than retaining capacity. */
        std::vector<std::string>().swap(flagNames);
        flagReferences.clear();
    }

    endMarkerFound = 0;
    currentReference.clear();
    currentString.clear();
}

/* Source: CoDUOMP.exe 0x00471690..0x004716d3.
 * Name/signature: same-module Mac symbol
 * CStringEdPackage::Filename_PathOnly(const char *).  The fixed 64-byte static
 * result and unbounded copy are part of the original contract. */
const char *CStringEdPackage::Filename_PathOnly(const char *filename)
{
    static char pathOnly[MAX_QPATH];

    std::strcpy(pathOnly, filename);
    char *separator = std::strrchr(pathOnly, '\\');
    char *forwardSeparator = std::strrchr(pathOnly, '/');
    if (separator == nullptr ||
        (forwardSeparator != nullptr && separator < forwardSeparator)) {
        separator = forwardSeparator;
    }
    if (separator != nullptr)
        *separator = '\0';
    return pathOnly;
}

/* Source: CoDUOMP.exe 0x004716e0..0x0047173d.
 * Name/signature: same-module Mac symbol
 * CStringEdPackage::Filename_WithoutExt(const char *). */
const char *CStringEdPackage::Filename_WithoutExt(const char *filename)
{
    static char withoutExtension[MAX_QPATH];

    std::strcpy(withoutExtension, filename);
    char *extension = std::strrchr(withoutExtension, '.');
    const char *backSeparator = std::strrchr(withoutExtension, '\\');
    const char *forwardSeparator = std::strrchr(withoutExtension, '/');
    if (extension != nullptr &&
        (backSeparator == nullptr || extension > backSeparator) &&
        (forwardSeparator == nullptr || extension > forwardSeparator)) {
        *extension = '\0';
    }
    return withoutExtension;
}

/* Source: CoDUOMP.exe 0x00471740..0x00471774.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00471740_00471774.mcode.
 * Name/signature: same-module Mac symbol
 * CStringEdPackage::Filename_WithoutPath(const char *). The fixed 64-byte
 * static result and unbounded copy are part of the original contract. */
const char *CStringEdPackage::Filename_WithoutPath(const char *filename)
{
    static char withoutPath[MAX_QPATH];
    const char *lastComponent = filename;

    for (const char *cursor = filename; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\')
            lastComponent = cursor + 1;
    }
    std::strcpy(withoutPath, lastComponent);
    return withoutPath;
}

/* Source: CoDUOMP.exe 0x00471780..0x004717c2.
 * Name/signature: same-module Mac symbol
 * CStringEdPackage::ExtractLanguageFromPath(const char *). */
const char *CStringEdPackage::ExtractLanguageFromPath(const char *filename)
{
    const char *path = Filename_PathOnly(filename);
    return Filename_WithoutPath(path);
}

/* Source: CoDUOMP.exe 0x004717d0..0x004718ba.
 * Name/signature: same-module Mac symbol
 * CStringEdPackage::SetupNewFileParse(const char *, int).  The Windows body
 * derives an uppercase package basename and its containing language directory,
 * then records whether the language is English and propagates loadDebug. */
void CStringEdPackage::SetupNewFileParse(const char *filename,
                                         int32_t loadDebugValue)
{
    const char *pathWithoutExtension = Filename_WithoutExt(filename);
    const char *packageName = pathWithoutExtension;
    for (const char *cursor = pathWithoutExtension; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\')
            packageName = cursor + 1;
    }

    char packageBuffer[MAX_QPATH];
    std::strcpy(packageBuffer, packageName);
    coduo_crt_strupr(packageBuffer);
    currentPackage.assign(packageBuffer);

    currentLanguage.assign(ExtractLanguageFromPath(filename));
    currentLanguageIsEnglish =
        coduo_crt_stricmp(currentLanguage.c_str(), "english") == 0;
    loadDebug = loadDebugValue;
}

/* Source: CoDUOMP.exe 0x004718c0..0x00471909.
 * Name/signature: same-module Mac symbol
 * CStringEdPackage::CheckLineForKeyword(const char *, const char *&). */
bool CStringEdPackage::CheckLineForKeyword(const char *keyword,
                                           const char *&line)
{
    const size_t keywordLength = std::strlen(keyword);
    if (coduo_crt_strnicmp(line, keyword, keywordLength) != 0)
        return false;

    line += keywordLength;
    while (*line == '\t' || *line == ' ')
        ++line;
    return true;
}

/* Source: CoDUOMP.exe 0x00471910..0x004719e1.
 * Name/signature argument: same-module Mac symbol
 * CStringEdPackage::ConvertCRLiterals_Read(const char *).  The Windows return
 * value is a borrowed pointer into the function-local static string.  Each
 * search restarts at position zero, exactly as in the original body. */
const char *CStringEdPackage::ConvertCRLiterals_Read(const char *text)
{
    static std::string convertedText;

    convertedText.assign(text);
    for (std::string::size_type position = convertedText.find("\\n", 0, 2);
         position != std::string::npos;
         position = convertedText.find("\\n", 0, 2)) {
        convertedText[position] = '\n';
        convertedText.erase(position + 1, 1);
    }

    return convertedText.c_str();
}

/* Source: CoDUOMP.exe 0x004719f0..0x00471a7c.
 * Name/signature: same-module Mac symbol CStringEdPackage::REMKill(char *).
 * A // begins a comment only after an even number of double quotes.  After an
 * in-quote match the next search deliberately resumes at the second slash. */
void CStringEdPackage::REMKill(char *text)
{
    char *searchStart = text;
    int32_t quoteCount = 0;

    for (char *comment = std::strstr(searchStart, "//"); comment != nullptr;
         comment = std::strstr(searchStart, "//")) {
        for (char *cursor = searchStart; cursor < comment; ++cursor) {
            if (*cursor == '"')
                ++quoteCount;
        }
        if ((quoteCount & 1) != 0) {
            searchStart = comment + 1;
            continue;
        }

        *comment = '\0';
        if (*searchStart != '\0') {
            int32_t index = (int32_t)std::strlen(searchStart) - 1;
            while (index >= 0 && coduo_crt_isspace(
                                     (int32_t)(signed char)searchStart[index])) {
                searchStart[index--] = '\0';
            }
        }
        return;
    }
}

/* Source: CoDUOMP.exe 0x00471a80..0x00471b5f.
 * Name/signature: same-module Mac symbol
 * CStringEdPackage::ReadLine(const char *&, char *). */
bool CStringEdPackage::ReadLine(const char *&source, char *line)
{
    if (*source == '\0')
        return false;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    const char *newline = std::strchr(source, '\n');
    if (newline != nullptr) {
        const size_t lineLength = (size_t)(newline - source);
        std::strncpy(line, source, lineLength);
        line[lineLength] = '\0';
        source += lineLength;
        while (*source != '\0' && std::strchr("\r\n", *source) != nullptr)
            ++source;
    } else {
        std::strcpy(line, source);
        source += std::strlen(source);
    }

    if (*line != '\0') {
        int32_t index = (int32_t)std::strlen(line) - 1;
        while (index >= 0 &&
               coduo_crt_isspace((int32_t)(signed char)line[index])) {
            line[index--] = '\0';
        }
        REMKill(line);
    }
    return true;
}

/* Source: CoDUOMP.exe 0x00471b60..0x00471cff.
 * Name/signature argument: same-module Mac symbol
 * CStringEdPackage::InsideQuotes(const char *).  The Windows return value is a
 * borrowed pointer into the function-local static string.  The original only
 * bypasses trailing inspection when the post-prefix input is initially empty;
 * the loop intentionally has no added empty-string guard. */
const char *CStringEdPackage::InsideQuotes(const char *text)
{
    static std::string quotedText;

    quotedText.clear();
    while (*text == ' ' || *text == '\t')
        ++text;
    if (*text == '"')
        ++text;

    quotedText.assign(text);
    if (*text != '\0') {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        while (!quotedText.empty() &&
               (quotedText.c_str()[quotedText.length() - 1] == ' ' ||
                quotedText.c_str()[quotedText.length() - 1] == '\t')) {
            quotedText.erase(quotedText.length() - 1, 1);
        }
        if (!quotedText.empty() &&
            quotedText.c_str()[quotedText.length() - 1] == '"') {
            quotedText.erase(quotedText.length() - 1, 1);
        }
    }

    return quotedText.c_str();
}

/* Source: CoDUOMP.exe 0x00471d00..0x00471daf.
 * Name/signature: same-module Mac symbol
 * CStringEdPackage::GetFlagMask(const char *).  The original +0x98 member is
 * proven here as map<string, int32_t>: a found tree node returns its mapped
 * word at node +0x28. */
int32_t CStringEdPackage::GetFlagMask(const char *flagName)
{
    const auto found = flagReferences.find(flagName);
    if (found == flagReferences.end())
        return 0;
    return found->second;
}

/* Source: CoDUOMP.exe 0x00471db0..0x00471f79.
 * Name/signature: same-module Mac symbol
 * CStringEdPackage::AddFlagReference(const char *, const char *).  New flag
 * names receive the next power-of-two mask; the mask is then merged into the
 * already-created package/reference entry when that entry exists. */
void CStringEdPackage::AddFlagReference(const char *flagName,
                                        const char *reference)
{
    int32_t flagMask = GetFlagMask(flagName);
    if (flagMask == 0) {
        flagNames.push_back(flagName);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        const uint32_t flagMaskBits =
            1u << (((uint32_t)flagNames.size() - 1u) &
                   (STRINGED_FLAG_MASK_BIT_COUNT - 1u));
        std::memcpy(&flagMask, &flagMaskBits, sizeof(flagMask));
        flagReferences[flagName] = flagMask;
    }

    const std::string fullReference(
        va("%s_%s", currentPackage.c_str(), reference));
    const auto entry = entries.find(fullReference);
    if (entry != entries.end())
        entry->second.flags |= flagMask;
}

/* Source: CoDUOMP.exe 0x00471f80..0x00472317.
 * Name/signature: same-module Mac symbol
 * CStringEdPackage::ParseLine(const char *).  A null return means the line was
 * accepted; parse failures are returned through the original rotating va()
 * buffer.  Keyword order, case-insensitive prefix matching, English fallback,
 * and the fixed 1024-byte token buffer follow the Windows instruction flow. */
const char *CStringEdPackage::ParseLine(const char *line)
{
    if (line == nullptr)
        return nullptr;

    const char *value = line;
    if (CheckLineForKeyword("VERSION", value)) {
        const int32_t version = coduo_crt_atoi(InsideQuotes(value));
        if (version == STRINGED_FILE_VERSION)
            return nullptr;
        return va("Unexpected version number %d, expecting %d!\n", version,
                  STRINGED_FILE_VERSION);
    }

    value = line;
    if (CheckLineForKeyword("CONFIG", value))
        return nullptr;
    value = line;
    if (CheckLineForKeyword("FILENOTES", value))
        return nullptr;
    value = line;
    if (CheckLineForKeyword("NOTES", value))
        return nullptr;

    value = line;
    if (CheckLineForKeyword("REFERENCE", value)) {
        AddEntry(InsideQuotes(value));
        return nullptr;
    }

    value = line;
    if (CheckLineForKeyword("FLAGS", value)) {
        if (currentReference.empty())
            return "Error parsing file: Unexpected \"FLAGS\"\n";

        char flagBuffer[MAX_STRING_CHARS] = {};
        std::strncpy(flagBuffer, value, sizeof(flagBuffer) - 1);
        for (char *flagName = std::strtok(flagBuffer, " \t");
             flagName != nullptr;
             flagName = std::strtok(nullptr, " \t")) {
            coduo_crt_strupr(flagName);
            AddFlagReference(flagName, currentReference.c_str());
        }
        return nullptr;
    }

    value = line;
    if (CheckLineForKeyword("ENDMARKER", value)) {
        endMarkerFound = 1;
        return nullptr;
    }

    if (coduo_crt_strnicmp(line, STRINGED_LANGUAGE_PREFIX,
                            sizeof(STRINGED_LANGUAGE_PREFIX) - 1) == 0) {
        if (currentReference.empty())
            return "Error parsing file: Unexpected \"LANG_\"\n";

        const char *languageStart =
            line + sizeof(STRINGED_LANGUAGE_PREFIX) - 1;
        const char *languageEnd = languageStart;
        while (*languageEnd != '\0' && *languageEnd != ' ' &&
               *languageEnd != '\t') {
            ++languageEnd;
        }

        char language[MAX_STRING_CHARS] = {};
        size_t languageLength = (size_t)(languageEnd - languageStart);
        if (languageLength > sizeof(language) - 1)
            languageLength = sizeof(language) - 1;
        std::strncpy(language, languageStart, languageLength);

        const char *text = languageStart + std::strlen(language);
        text = ConvertCRLiterals_Read(InsideQuotes(text));

        if (currentLanguageIsEnglish != 0) {
            SetString(currentReference.c_str(), text, 0);
            return nullptr;
        }

        const int32_t isEnglish =
            coduo_crt_stricmp(language, "english") == 0;
        if (isEnglish == 0 &&
            coduo_crt_stricmp(language, currentLanguage.c_str()) != 0) {
            return va("Language \"%s\" found when expecting \"%s\"!\n",
                      language, currentLanguage.c_str());
        }

        SetString(currentReference.c_str(), text, isEnglish);
        return nullptr;
    }

    return va("Unknown keyword at linestart: \"%s\"\n", line);
}

/* Source: CoDUOMP.exe 0x00472320..0x0047232e; the original Ghidra export
 * omitted this complete function from its function records.
 * Name/signature: same-module Mac symbol
 * CStringEdPackage::GetCurrentReference_ParseOnly(). */
const char *CStringEdPackage::GetCurrentReference_ParseOnly()
{
    return currentReference.c_str();
}

/* Source: CoDUOMP.exe 0x00472330..0x00472556.
 * Name/signature: same-module Mac symbol
 * CStringEdPackage::AddEntry(const char *).  The package-qualified key is
 * inserted only when absent; either way, the unqualified reference becomes
 * the current parse reference. */
void CStringEdPackage::AddEntry(const char *reference)
{
    const std::string fullReference(
        va("%s_%s", currentPackage.c_str(), reference));
    if (entries.find(fullReference) == entries.end())
        entries.emplace(fullReference, SE_Entry_s{});
    currentReference.assign(reference);
}

/* Source: CoDUOMP.exe 0x004725e0..0x004727c0.
 * Name/signature: same-module Mac symbol
 * CStringEdPackage::SetString(const char *, const char *, int).  A translated
 * "#same" reuses currentString unless forced or parsing English.  When debug
 * loading is enabled, the separately returned debug string preserves the
 * literal token inside brackets. */
void CStringEdPackage::SetString(const char *reference, const char *newText,
                                 int32_t forceSameText)
{
    const std::string fullReference(
        va("%s_%s", currentPackage.c_str(), reference));
    const auto found = entries.find(fullReference);
    if (found == entries.end())
        return;

    SE_Entry_s &entry = found->second;
    if (forceSameText == 0 && currentLanguageIsEnglish == 0 &&
        coduo_crt_stricmp(newText, "#same") == 0) {
        entry.text = currentString;
        if (loadDebug != 0)
            entry.debugText = "[#same]";
        return;
    }

    entry.text.assign(newText);
    if (loadDebug != 0) {
        entry.debugText.assign("[");
        entry.debugText.append(newText);
        entry.debugText.append("]");
    }
    currentString.assign(newText);
}

/* Original object 0x0389fec8.  Compiler startup wrapper
 * 0x00584ea0..0x00584eb5 constructs it and registers the destruction thunk at
 * 0x00585290. */
CStringEdPackage stringEdPackage;

/* Original object 0x0389ff6c.  Its trivial zero-state construction is folded
 * into static initialization; compiler wrapper 0x00584ec0 registers the
 * destruction thunk at 0x00585300. */
std::vector<std::string> seFoundFiles;
