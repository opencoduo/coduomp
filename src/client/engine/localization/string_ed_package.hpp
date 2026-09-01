#ifndef CODUOMP_STRING_ED_PACKAGE_HPP
#define CODUOMP_STRING_ED_PACKAGE_HPP

#include <cstdint>
#include <map>
#include <string>
#include <vector>

/* Same-module Mac template/destructor symbols prove SE_Entry_s, and its code
 * places two 0x0c-byte STL strings at +0x00/+0x0c and flags at +0x18 (size
 * 0x1c). Windows uses two 0x1c-byte MSVC strings and flags at +0x38 (size
 * 0x3c). These private layouts prove member order, not a portable retail ABI;
 * maintained source uses host std::string and carries no original-layout
 * assertion, including on unrelated i386 standard-library implementations. */
struct SE_Entry_s {
    std::string text;
    std::string debugText;
    int32_t flags = 0;
};

/* The original Windows object at 0x0389fec8 uses MSVC's 32-bit string and STL
 * layouts.  These containers are private engine state and cross no binary,
 * file, or network ABI, so maintained source deliberately uses the host C++
 * library rather than preserving those implementation-specific layouts.
 * Member roles beyond the methods recovered so far remain documented here so
 * subsequent CStringEdPackage recovery can refine names without raw offsets. */
class CStringEdPackage {
public:
    CStringEdPackage();
    ~CStringEdPackage();

    void Clear(int32_t preserveFlagData);
    const char *Filename_PathOnly(const char *filename);
    const char *Filename_WithoutExt(const char *filename);
    const char *Filename_WithoutPath(const char *filename);
    const char *ExtractLanguageFromPath(const char *filename);
    void SetupNewFileParse(const char *filename, int32_t loadDebugValue);
    bool CheckLineForKeyword(const char *keyword, const char *&line);
    const char *ConvertCRLiterals_Read(const char *text);
    void REMKill(char *text);
    bool ReadLine(const char *&source, char *line);
    const char *InsideQuotes(const char *text);
    int32_t GetFlagMask(const char *flagName);
    void AddFlagReference(const char *flagName, const char *reference);
    const char *GetCurrentReference_ParseOnly();
    void AddEntry(const char *reference);
    void SetString(const char *reference, const char *text,
                   int32_t forceSameText);
    const char *ParseLine(const char *line);

    int32_t endMarkerFound;
    std::string currentReference;
    std::string currentString;
    std::string currentPackage;
    std::string currentLanguage;
    /* +0x74 in the original object.  The constructor deliberately leaves this
     * word untouched; SetupNewFileParse establishes it before use. */
    int32_t currentLanguageIsEnglish;
    std::map<std::string, SE_Entry_s> entries;
    /* Original +0x84, between the two tree/vector subobjects.  It is the first
     * integer argument propagated from SE_Load through SE_Load_Actual. */
    int32_t loadDebug;
    std::vector<std::string> flagNames;
    std::map<std::string, int32_t> flagReferences;
};

extern CStringEdPackage stringEdPackage;

/* Original global vector at 0x0389ff6c.  SE_BuildFileList owns its contents;
 * later localization recovery uses the Mac SE_GetFoundFile naming evidence to
 * expose entries without retaining the MSVC vector layout. */
extern std::vector<std::string> seFoundFiles;

const char *SE_Load_Actual(const char *filename, int32_t loadDebug,
                           int32_t quiet);
const char *SE_GetFoundFile(std::string &foundFiles);
const char *SE_Load(const char *filename, int32_t loadDebug);
const char *SE_GetString(const char *reference, int32_t useDebug);
const char *SE_GetStringByPackage(
    const char *package, const char *reference, int32_t useDebug);
int32_t SE_GetStringFlags(const char *reference);
int32_t SE_GetStringFlagsByPackage(
    const char *package, const char *reference);
int32_t SE_GetFlagMask(const char *flagName);
int32_t SE_GetNumFlags();
const char *SE_GetFlagName(int32_t flagIndex);
unsigned char *SE_LoadFileData(const char *filename, int32_t *fileLength);
void SE_FreeFileDataAfterLoad(unsigned char *fileData);
void SE_R_ListFiles(const char *path, const char *extension,
                    std::string &foundFiles);
int32_t SE_BuildFileList(const char *path, std::string &foundFiles);
int32_t SE_GetNumLanguages();
const char *SE_GetLanguageName(int32_t languageIndex);
const char *SE_GetLanguagePath(int32_t languageIndex);
void SE_ShutDown();
void SE_Init();
void SE_NewLanguage();
const char *SE_LoadLanguage(const char *language, int32_t loadDebug);

#endif
