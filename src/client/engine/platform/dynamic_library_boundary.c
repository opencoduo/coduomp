#include "dynamic_library_boundary.h"

#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <errno.h>
#endif

#if defined(_WIN32) && defined(__MINGW32__) && UINTPTR_MAX == UINT32_MAX
extern void __register_frame(const void *frameStart);
extern void __deregister_frame(const void *frameStart);

/* NOT_FROM_ORIGINAL_SOURCE: read a bounded extent from a loaded PE image's
 * backing file. MinGW stores long final-image section names in the COFF string
 * table, which is not mapped into memory by LoadLibrary. */
static int coduomp_mingw32_read_image_file(HANDLE file, ULONGLONG offset, void *destination, DWORD byteCount)
{
    LARGE_INTEGER fileOffset;
    DWORD bytesRead;

    fileOffset.QuadPart = (LONGLONG)offset;
    return SetFilePointerEx(file, fileOffset, NULL, FILE_BEGIN) != FALSE &&
           ReadFile(file, destination, byteCount, &bytesRead, NULL) != FALSE &&
           bytesRead == byteCount;
}

/* NOT_FROM_ORIGINAL_SOURCE: resolve a slash-and-decimal COFF section name
 * such as MinGW's "/4" spelling for .eh_frame. */
static int coduomp_mingw32_long_section_name_matches(HANDLE file, const IMAGE_FILE_HEADER *fileHeader,
                                                     const IMAGE_SECTION_HEADER *section, const char *expectedName)
{
    DWORD nameOffset = 0;
    DWORD stringTableSize;
    ULONGLONG stringTableOffset;
    size_t expectedLength;
    size_t index;
    int hasDigit = 0;
    char actualName[32];

    if (section->Name[0] != '/')
        return 0;

    for (index = 1; index < IMAGE_SIZEOF_SHORT_NAME && section->Name[index] != '\0'; ++index) {
        const unsigned int digit = (unsigned int)(section->Name[index] - '0');

        if (digit > 9 || nameOffset > (UINT32_MAX - digit) / 10u)
            return 0;
        nameOffset = nameOffset * 10u + digit;
        hasDigit = 1;
    }
    if (hasDigit == 0 || nameOffset < sizeof(stringTableSize) || fileHeader->PointerToSymbolTable == 0)
        return 0;

    stringTableOffset = (ULONGLONG)fileHeader->PointerToSymbolTable +
                        (ULONGLONG)fileHeader->NumberOfSymbols * IMAGE_SIZEOF_SYMBOL;
    if (coduomp_mingw32_read_image_file(file, stringTableOffset, &stringTableSize, sizeof(stringTableSize)) == 0)
        return 0;

    expectedLength = strlen(expectedName);
    if (expectedLength + 1u > sizeof(actualName) || nameOffset > stringTableSize ||
        expectedLength + 1u > (size_t)(stringTableSize - nameOffset)) {
        return 0;
    }

    if (coduomp_mingw32_read_image_file(file, stringTableOffset + nameOffset,
                                        actualName, (DWORD)(expectedLength + 1u)) == 0) {
        return 0;
    }
    return memcmp(actualName, expectedName, expectedLength + 1u) == 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: locate a statically linked MinGW module's DWARF2
 * unwind table so the executable's single static libgcc registry can own it.
 * Without this registration, a ScriptErrorClass thrown by the engine cannot
 * unwind through uo_game_mp_x86.dll back to the engine's catch handler. */
static const void *coduomp_mingw32_static_unwind_frames(HMODULE module)
{
    const unsigned char *const imageBase = (const unsigned char *)module;
    const IMAGE_DOS_HEADER *dosHeader;
    const IMAGE_NT_HEADERS32 *ntHeaders;
    const IMAGE_SECTION_HEADER *sections;
    const void *frameStart = NULL;
    char modulePath[MAX_PATH];
    DWORD modulePathLength;
    HANDLE file;
    unsigned int index;

    if (module == NULL)
        return NULL;

    dosHeader = (const IMAGE_DOS_HEADER *)imageBase;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE || dosHeader->e_lfanew <= 0)
        return NULL;

    ntHeaders = (const IMAGE_NT_HEADERS32 *)(imageBase + (size_t)dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE ||
        ntHeaders->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        ntHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        return NULL;
    }

    sections = IMAGE_FIRST_SECTION(ntHeaders);
    if ((const unsigned char *)(sections + ntHeaders->FileHeader.NumberOfSections) >
        imageBase + ntHeaders->OptionalHeader.SizeOfHeaders) {
        return NULL;
    }

    modulePathLength = GetModuleFileNameA(module, modulePath, (DWORD)sizeof(modulePath));
    if (modulePathLength == 0 || modulePathLength >= sizeof(modulePath))
        return NULL;

    file = CreateFileA(modulePath, GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return NULL;

    for (index = 0; index < ntHeaders->FileHeader.NumberOfSections; ++index) {
        char shortName[IMAGE_SIZEOF_SHORT_NAME + 1];
        int nameMatches;
        DWORD sectionRva;
        DWORD sectionSize;

        memcpy(shortName, sections[index].Name, IMAGE_SIZEOF_SHORT_NAME);
        shortName[IMAGE_SIZEOF_SHORT_NAME] = '\0';
        nameMatches = strcmp(shortName, ".eh_frame") == 0 ||
                      coduomp_mingw32_long_section_name_matches(
                          file, &ntHeaders->FileHeader, &sections[index], ".eh_frame");
        if (nameMatches == 0)
            continue;

        sectionRva = sections[index].VirtualAddress;
        sectionSize = sections[index].Misc.VirtualSize;
        if (sectionRva != 0 && sectionRva < ntHeaders->OptionalHeader.SizeOfImage &&
            sectionSize >= sizeof(uint32_t) &&
            sectionSize <= ntHeaders->OptionalHeader.SizeOfImage - sectionRva) {
            const uint32_t *const candidate = (const uint32_t *)(imageBase + sectionRva);
            if (*candidate != 0)
                frameStart = candidate;
        }
        break;
    }

    CloseHandle(file);
    return frameStart;
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: portable boundary for the Win32
 * GetSystemDirectoryA import used by QGL_Init only to construct its diagnostic
 * library path. The actual LoadLibrary call receives the caller's unmodified
 * name. Native targets report their conventional system-library directory. */
void coduomp_system_library_directory(char *buffer, size_t bufferSize)
{
    const char *directory;
    size_t length;

    if (bufferSize == 0)
        return;

#if defined(_WIN32)
    if (GetSystemDirectoryA(buffer, (UINT)bufferSize) != 0) {
        buffer[bufferSize - 1] = '\0';
        return;
    }
    directory = "";
#elif defined(__APPLE__)
    directory = "/System/Library/Frameworks";
#else
    directory = "/usr/lib";
#endif

    length = strlen(directory);
    if (length >= bufferSize)
        length = bufferSize - 1;
    memcpy(buffer, directory, length);
    buffer[length] = '\0';
}

/* NOT_FROM_ORIGINAL_SOURCE: portable boundary for the Win32 LoadLibraryA
 * import at CoDUOMP.exe 0x004d88bf. The retail call site performs no post-load
 * x87 adjustment, and retail cgame/UI/listen-server game process attach skips
 * each module's precision helper. MinGW i686's TLS callback calls __fpreset for
 * DLL_THREAD_ATTACH, not DLL_PROCESS_ATTACH, so this boundary must not alter
 * the caller thread's control word after a successful load. */
void *coduomp_library_open(const char *libraryName)
{
#if defined(_WIN32)
    HMODULE const module = LoadLibraryA(libraryName);
#if defined(__MINGW32__) && UINTPTR_MAX == UINT32_MAX
    const void *const frameStart = coduomp_mingw32_static_unwind_frames(module);
    if (frameStart != NULL)
        __register_frame(frameStart);
#endif
    return (void *)module;
#else
    return dlopen(libraryName, RTLD_NOW | RTLD_LOCAL);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: typed function-pointer destinations cannot be
 * populated portably through an object-pointer cast. Copying the native
 * loader's symbol carrier into the caller's typed slot preserves its bits and
 * keeps that conversion confined to this explicit dynamic-library boundary. */
void coduomp_library_symbol(void *libraryHandle, const char *symbolName,
                            void *destination, size_t destinationSize)
{
#if defined(_WIN32)
    FARPROC symbol = GetProcAddress((HMODULE)libraryHandle, symbolName);
#else
    void *symbol = dlsym(libraryHandle, symbolName);
#endif

    memset(destination, 0, destinationSize);
    if (destinationSize <= sizeof(symbol))
        memcpy(destination, &symbol, destinationSize);
}

/* NOT_FROM_ORIGINAL_SOURCE: portable system boundary for the Win32
 * FreeLibrary import. QGL_Shutdown ignores the result at CoDUOMP.exe
 * 0x004d7030, while Sys_UnloadDll checks it at 0x0046b745. */
int32_t coduomp_library_close(void *libraryHandle)
{
#if defined(_WIN32)
#if defined(__MINGW32__) && UINTPTR_MAX == UINT32_MAX
    const void *const frameStart = coduomp_mingw32_static_unwind_frames((HMODULE)libraryHandle);
    if (frameStart != NULL)
        __deregister_frame(frameStart);
#endif
    return FreeLibrary((HMODULE)libraryHandle) != FALSE;
#else
    return dlclose(libraryHandle) == 0;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: portable boundary for the Win32 GetLastError IAT
 * calls made by the checked WGL wrappers. Non-Windows platform loaders expose
 * their corresponding thread-local loader error through errno. */
uint32_t coduomp_platform_last_error(void)
{
#if defined(_WIN32)
    return (uint32_t)GetLastError();
#else
    return (uint32_t)errno;
#endif
}
