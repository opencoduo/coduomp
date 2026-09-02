#include "q_shared.h"
#include "platform/crt_boundary.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include "platform/macos_app_bundle.h"
#endif

#if defined(_WIN32)
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

enum {
    SYS_FILE_CASE_COMPARE_LIMIT = 99999,
    SYS_LISTFILES_MAX_COUNT = 4095,
    SYS_LISTFILES_STACK_CAPACITY = SYS_LISTFILES_MAX_COUNT + 1
};

/* Original CoDUOMP.exe 0x009cd948..0x009cda47. Sys_Cwd and the two default-path
 * accessors whose bodies inline it all use this same static buffer. */
static char sysCurrentWorkingDirectory[MAX_OSPATH];
/* NOT_FROM_ORIGINAL_SOURCE: native writable-data path used on modern Unix
 * targets. The retail Windows executable has no per-user filesystem root. */
static char sysDefaultHomePath[MAX_OSPATH];

/* Source: CoDUOMP.exe 0x004688f0..0x004688f7.
 * Name and signature: exact same-module Mac symbol Sys_Mkdir. */
void Sys_Mkdir(const char *path)
{
    (void)coduomp_crt_mkdir(path);
}

/* Source: CoDUOMP.exe 0x00468900..0x0046891e.
 * Name and signature: exact same-module Mac symbol Sys_Cwd. The final byte is
 * cleared unconditionally, matching the original bounded CRT call. */
const char *Sys_Cwd(void)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (coduomp_crt_getcwd(sysCurrentWorkingDirectory, sizeof(sysCurrentWorkingDirectory) - 1u) == NULL) {
        sysCurrentWorkingDirectory[0] = '.';
        sysCurrentWorkingDirectory[1] = '\0';
    }
    sysCurrentWorkingDirectory[sizeof(sysCurrentWorkingDirectory) - 1u] = '\0';
    return sysCurrentWorkingDirectory;
}

/* Source: CoDUOMP.exe 0x00468920..0x00468925.
 * Name: exact same-module Mac symbol Sys_DefaultCDPath. */
const char *Sys_DefaultCDPath(void)
{
#if defined(__APPLE__)
    const char *const dataPath = coduomp_macos_default_cd_path();
    if (dataPath != NULL && dataPath[0] != '\0')
        return dataPath;
#endif
    return "";
}

/* Source: CoDUOMP.exe 0x00468930..0x0046894e.
 * Name and role: established engine platform API. The Windows compiler inlined
 * Sys_Cwd, producing the same shared-buffer body at both addresses. */
const char *Sys_DefaultBasePath(void)
{
#if defined(__APPLE__)
    const char *const resourcesPath = coduomp_macos_bundle_resources_path();
    if (resourcesPath != NULL)
        return resourcesPath;
#endif
    return Sys_Cwd();
}

/* Source: CoDUOMP.exe 0x00468950..0x00468952.
 * Name: exact same-module Mac symbol Sys_DefaultHomePath. The recovered
 * Windows behavior remains NULL. Modern native Unix targets replace that
 * unsafe installation-directory fallback with the platform's per-user
 * application-data directory. */
const char *Sys_DefaultHomePath(void)
{
#if defined(_WIN32)
    return NULL;
#else
    const char *const home = getenv("HOME");
    if (home == NULL || home[0] == '\0')
        return NULL;

#if defined(__APPLE__)
    /* NOT_FROM_ORIGINAL_SOURCE: standard modern macOS application-data root. */
    Com_sprintf(sysDefaultHomePath, sizeof(sysDefaultHomePath), "%s/Library/Application Support/OpenCoDUO", home);
#else
    /* NOT_FROM_ORIGINAL_SOURCE: XDG persistent user-data root, with the
     * specified ~/.local/share fallback when XDG_DATA_HOME is unset. */
    const char *const xdgDataHome = getenv("XDG_DATA_HOME");
    if (xdgDataHome != NULL && xdgDataHome[0] == '/') {
        Com_sprintf(sysDefaultHomePath, sizeof(sysDefaultHomePath), "%s/opencoduo", xdgDataHome);
    } else {
        Com_sprintf(sysDefaultHomePath, sizeof(sysDefaultHomePath), "%s/.local/share/opencoduo", home);
    }
#endif

    return sysDefaultHomePath;
#endif
}

/* Source: CoDUOMP.exe 0x00468960..0x0046897e.
 * Name: exact same-module Mac symbol Sys_DefaultInstallPath. The Windows
 * compiler inlined Sys_Cwd here as well. */
const char *Sys_DefaultInstallPath(void)
{
#if defined(__APPLE__)
    const char *const resourcesPath = coduomp_macos_bundle_resources_path();
    if (resourcesPath != NULL)
        return resourcesPath;
#endif
    return Sys_Cwd();
}

static qboolean Sys_IsManagedDirectoryName(const char *name)
{
    /* NOT_FROM_ORIGINAL_SOURCE: shared spelling check factored from the three
     * identical Windows Q_stricmpn chains. */
    return (Q_stricmpn(name, ".", SYS_FILE_CASE_COMPARE_LIMIT) == 0 || Q_stricmpn(name, "..", SYS_FILE_CASE_COMPARE_LIMIT) == 0 ||
            Q_stricmpn(name, "CVS", SYS_FILE_CASE_COMPARE_LIMIT) == 0)
               ? qtrue
               : qfalse;
}

#if !defined(_WIN32)
static qboolean Sys_PosixPathIsDirectory(const char *directory, const char *name)
{
    /* NOT_FROM_ORIGINAL_SOURCE: native-platform replacement for the Win32
     * _finddata_t.attrib directory bit. */
    char path[MAX_OSPATH];
    struct stat status;

    Com_sprintf(path, sizeof(path), "%s/%s", directory, name);
    return stat(path, &status) == 0 && S_ISDIR(status.st_mode) ? qtrue : qfalse;
}

static qboolean Sys_PosixNameHasExtension(const char *name, const char *extension)
{
    /* NOT_FROM_ORIGINAL_SOURCE: native-platform replacement for the suffix
     * wildcard passed to the case-insensitive Win32 _findfirst API. */
    const size_t nameLength = strlen(name);
    const size_t extensionLength = strlen(extension);

    if (extensionLength == 0)
        return qtrue;
    if (nameLength < extensionLength)
        return qfalse;
    return Q_stricmpn(name + nameLength - extensionLength, extension, SYS_FILE_CASE_COMPARE_LIMIT) == 0 ? qtrue : qfalse;
}
#endif

/* Source: CoDUOMP.exe 0x00468980..0x00468b36.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00468980_00468b37.mcode.
 * Name: exact same-module Mac symbol
 * Sys_ListFilteredFiles(char const *, char *, char *, char **, int *). */
void Sys_ListFilteredFiles(const char *directory, const char *subdirectory, const char *filter, char **list, int32_t *numFiles)
{
    char searchPath[MAX_OSPATH];
    char childSubdirectory[MAX_OSPATH];
    char relativePath[MAX_OSPATH];

    if (*numFiles >= SYS_LISTFILES_MAX_COUNT)
        return;

#if defined(_WIN32)
    struct _finddata_t findData;
    intptr_t findHandle;

    if (strlen(subdirectory) != 0) {
        Com_sprintf(searchPath, sizeof(searchPath), "%s\\%s\\*", directory, subdirectory);
    } else {
        Com_sprintf(searchPath, sizeof(searchPath), "%s\\*", directory);
    }

    findHandle = _findfirst(searchPath, &findData);
    if (findHandle == -1)
        return;

    do {
        if ((findData.attrib & _A_SUBDIR) != 0 && Sys_IsManagedDirectoryName(findData.name) == qfalse) {
            if (subdirectory != NULL) {
                Com_sprintf(childSubdirectory, sizeof(childSubdirectory), "%s\\%s", subdirectory, findData.name);
            } else {
                Com_sprintf(childSubdirectory, sizeof(childSubdirectory), "%s", findData.name);
            }
            Sys_ListFilteredFiles(directory, childSubdirectory, filter, list, numFiles);
        }

        if (*numFiles >= SYS_LISTFILES_MAX_COUNT)
            break;

        if (subdirectory != NULL) {
            Com_sprintf(relativePath, sizeof(relativePath), "%s\\%s", subdirectory, findData.name);
        } else {
            Com_sprintf(relativePath, sizeof(relativePath), "%s", findData.name);
        }
        if (Com_FilterPath(filter, relativePath, qfalse) != qfalse) {
            list[*numFiles] = CopyStringInternal(relativePath);
            ++*numFiles;
        }
    } while (_findnext(findHandle, &findData) != -1);

    _findclose(findHandle);
#else
    DIR *handle;

    if (subdirectory[0] != '\0') {
        Com_sprintf(searchPath, sizeof(searchPath), "%s/%s", directory, subdirectory);
    } else {
        Com_sprintf(searchPath, sizeof(searchPath), "%s", directory);
    }

    handle = opendir(searchPath);
    if (handle == NULL)
        return;

    for (;;) {
        const struct dirent *entry = readdir(handle);

        if (entry == NULL)
            break;
        if (Sys_PosixPathIsDirectory(searchPath, entry->d_name) != qfalse && Sys_IsManagedDirectoryName(entry->d_name) == qfalse) {
            if (subdirectory[0] != '\0') {
                Com_sprintf(childSubdirectory, sizeof(childSubdirectory), "%s/%s", subdirectory, entry->d_name);
            } else {
                Com_sprintf(childSubdirectory, sizeof(childSubdirectory), "%s", entry->d_name);
            }
            Sys_ListFilteredFiles(directory, childSubdirectory, filter, list, numFiles);
        }

        if (*numFiles >= SYS_LISTFILES_MAX_COUNT)
            break;

        Com_sprintf(relativePath, sizeof(relativePath), "%s/%s", subdirectory, entry->d_name);
        if (Com_FilterPath(filter, relativePath, qfalse) != qfalse) {
            list[*numFiles] = CopyStringInternal(relativePath);
            ++*numFiles;
        }
    }

    closedir(handle);
#endif
}

/* Source: CoDUOMP.exe 0x00468b40..0x00468d8c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00468b40_00468d8d.mcode.
 * Name: exact same-module Mac symbol Sys_ListFiles. */
char **Sys_ListFiles(const char *directory, const char *extension, const char *filter, int32_t *numFiles, qboolean wantDirectories)
{
    char *list[SYS_LISTFILES_STACK_CAPACITY];
    int32_t count = 0;

    if (filter != NULL) {
        Sys_ListFilteredFiles(directory, "", filter, list, &count);
    } else {
        qboolean listDirectories = wantDirectories;

        if (extension == NULL)
            extension = "";
        if (extension[0] == '/' && extension[1] == '\0') {
            extension = "";
            listDirectories = qtrue;
        }

#if defined(_WIN32)
        char searchPath[MAX_OSPATH];
        struct _finddata_t findData;
        intptr_t findHandle;

        Com_sprintf(searchPath, sizeof(searchPath), "%s\\*%s", directory, extension);
        findHandle = _findfirst(searchPath, &findData);
        if (findHandle == -1) {
            *numFiles = 0;
            return NULL;
        }

        do {
            const qboolean isDirectory = (findData.attrib & _A_SUBDIR) != 0 ? qtrue : qfalse;

            if (isDirectory != listDirectories)
                continue;
            if (isDirectory != qfalse && Sys_IsManagedDirectoryName(findData.name) != qfalse)
                continue;
            list[count++] = CopyStringInternal(findData.name);
            if (count == SYS_LISTFILES_MAX_COUNT)
                break;
        } while (_findnext(findHandle, &findData) != -1);

        _findclose(findHandle);
#else
        DIR *handle = opendir(directory);

        if (handle == NULL) {
            *numFiles = 0;
            return NULL;
        }

        for (;;) {
            const struct dirent *entry = readdir(handle);
            qboolean isDirectory;

            if (entry == NULL)
                break;
            isDirectory = Sys_PosixPathIsDirectory(directory, entry->d_name);
            if ((listDirectories != qfalse) != (isDirectory != qfalse))
                continue;
            if (isDirectory != qfalse && Sys_IsManagedDirectoryName(entry->d_name) != qfalse)
                continue;
            if (Sys_PosixNameHasExtension(entry->d_name, extension) == qfalse) {
                continue;
            }
            list[count++] = CopyStringInternal(entry->d_name);
            if (count == SYS_LISTFILES_MAX_COUNT)
                break;
        }

        closedir(handle);
#endif
    }

    list[count] = NULL;
    *numFiles = count;
    if (count == 0)
        return NULL;

    char **result = Z_MallocInternal(((size_t)count + 1U) * sizeof(result[0]));
    memcpy(result, list, (size_t)count * sizeof(result[0]));
    result[count] = NULL;
    return result;
}

/* Source: CoDUOMP.exe 0x00468d90..0x00468dbb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00468d90_00468dbc.mcode.
 * Name and ownership contract: exact same-module Mac symbol
 * Sys_FreeFileList. Every owned string is released before the pointer array. */
void Sys_FreeFileList(char **list)
{
    if (list == NULL)
        return;

    for (char **entry = list; *entry != NULL; ++entry)
        Z_FreeInternal(*entry);
    Z_FreeInternal(list);
}
