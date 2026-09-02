#include "filesystem.h"
#include "filesystem_path_security.h"
#include "filesystem_services.h"

#include "qcommon/q_string.h"

#include <stdint.h>
#include <string.h>

enum {
    FS_UNIQUE_STRING_LIST_STACK_CAPACITY = 4096,
    FS_MOD_DESCRIPTION_SIZE = FS_PACK_NAME_SIZE,
    FS_MOD_DESCRIPTION_READ_LIMIT = 48
};

#define FS_MOD_DESCRIPTION_FILE "description.txt"
#define FS_PK3_EXTENSION ".pk3"
#define FS_MAIN_GAME_DIR "main"
#define FS_MAIN_GAME_DESCRIPTION "CoD:United Offensive Multiplayer"

/* Source: CoDUOMP.exe 0x0042eeb0..0x0042f310; coduo_lnxded
 * 0x08063988..0x08063efb. Name and signature: exact same-module Mac symbol
 * FS_ListFilteredFiles. The original declarations are a 4096-pointer result
 * scratch list plus MAX_QPATH and MAX_OSPATH character arrays. */
char **FS_ListFilteredFiles(const char *path, const char *extension, const char *filter, int32_t *fileCount)
{
    filesystem_compat_check_started();

    if (path == NULL) {
        *fileCount = 0;
        return NULL;
    }
    if (extension == NULL)
        extension = "";
    if (path[0] == '/' || path[0] == '\\')
        ++path;

    /* NOT_FROM_ORIGINAL_SOURCE: a module-supplied list path must remain
     * relative and below the selected game root. */
    if (path[0] != '\0' && coduo_compat_path_is_safe_relative(path) == qfalse) {
        *fileCount = 0;
        return NULL;
    }

    const qboolean listFolders = Q_stricmp(extension, "/") == 0 ? qtrue : qfalse;
    int32_t pathLength = (int32_t)strlen(path);
    if (pathLength != 0 && (path[pathLength - 1] == '/' || path[pathLength - 1] == '\\')) {
        --pathLength;
    }
    const int32_t extensionLength = (int32_t)strlen(extension);

    char *list[FS_UNIQUE_STRING_LIST_STACK_CAPACITY];
    int32_t count = 0;

    char directory[MAX_OSPATH];
    int32_t pathDepth;
    if (FS_ReturnPath(path, directory, &pathDepth) < 0) {
        *fileCount = 0;
        return NULL;
    }
    if (path[0] != '\0')
        ++pathDepth;

    for (searchpath_t *search = fs_searchpaths; search != NULL; search = search->next) {
        if (FS_UseSearchPath(search) == qfalse)
            continue;

        pack_t *const pack = search->pack;
        if (pack != NULL) {
            if (search->localized == qfalse && FS_PakIsPure(pack) == qfalse) {
                continue;
            }

            for (int32_t index = 0; index < pack->numFiles; ++index) {
                const char *const packName = pack->fileList[index].name;

                if (filter != NULL) {
                    if (Com_FilterPath(filter, packName, qfalse) != qfalse)
                        count = FS_AddFileToList(packName, list, count);
                    continue;
                }

                int32_t filenameDepth;
                const int32_t filenameBaseLength = FS_ReturnPath(packName, directory, &filenameDepth);
                if (filenameBaseLength < 0 || filenameDepth != pathDepth || pathLength > filenameBaseLength ||
                    (pathLength > 0 && packName[pathLength] != '/') || Q_stricmpn(packName, path, pathLength) != 0) {
                    continue;
                }

                const int32_t packNameLength = (int32_t)strlen(packName);
                if (packNameLength < extensionLength || Q_stricmp(packName + packNameLength - extensionLength, extension) != 0) {
                    continue;
                }

                const size_t relativeOffset = (size_t)pathLength + (pathLength != 0 ? 1u : 0u);
                if (listFolders == qfalse) {
                    count = FS_AddFileToList(packName + relativeOffset, list, count);
                } else {
                    char strippedName[FS_PACK_NAME_SIZE];
                    const char *const relativeName = packName + relativeOffset;
                    const size_t relativeLength = strlen(relativeName);

                    /* NOT_FROM_ORIGINAL_SOURCE: the accepted pack-relative
                     * name and final NUL must fit the policy-sized scratch
                     * object before it is copied. */
                    if (relativeLength == 0 || relativeLength >= sizeof(strippedName)) {
                        Com_Printf("FS_ListFilteredFiles: pak folder name exceeds FS_PACK_NAME_SIZE\n");
                        continue;
                    }
                    memcpy(strippedName, relativeName, relativeLength + 1u);
                    strippedName[relativeLength - 1u] = '\0';
                    count = FS_AddFileToList(strippedName, list, count);
                }
            }
            continue;
        }

        directory_t *const hostDirectory = search->dir;
        if (hostDirectory == NULL)
            continue;
        if ((fs_restrict->integer != 0 || fs_numServerPaks != 0) && Q_stricmp(extension, "svg") != 0) {
            continue;
        }

        char osPath[MAX_OSPATH];
        FS_BuildOSPath(hostDirectory->path, hostDirectory->gamedir, path, osPath);
        char resolvedPath[MAX_OSPATH];
        const char *listPath = osPath;
        if (filesystem_compat_resolve_case_path(hostDirectory->path, osPath, resolvedPath, sizeof(resolvedPath)) != qfalse) {
            listPath = resolvedPath;
        }

        int32_t hostFileCount;
        char **const hostFiles = Sys_ListFiles(listPath, extension, filter, &hostFileCount, listFolders);
        for (int32_t index = 0; index < hostFileCount; ++index)
            count = FS_AddFileToList(hostFiles[index], list, count);
        Sys_FreeFileList(hostFiles);
    }

    *fileCount = count;
    if (count == 0)
        return NULL;

    char **const result = Z_MallocInternal(((size_t)count + 1u) * sizeof(*result));
    memcpy(result, list, (size_t)count * sizeof(*result));
    result[count] = NULL;
    return result;
}

/* Source: CoDUOMP.exe 0x0042f320..0x0042f339; coduo_lnxded
 * 0x08063efc..0x08063f24. Name: exact same-module Mac symbol FS_ListFiles. */
char **FS_ListFiles(const char *path, const char *extension, int32_t *fileCount)
{
    return FS_ListFilteredFiles(path, extension, NULL, fileCount);
}

/* Source: CoDUOMP.exe 0x0042f340..0x0042f370; coduo_lnxded
 * 0x08063f25..0x08063f7f. Name: exact same-module Mac symbol
 * FS_FreeFileList. */
void FS_FreeFileList(char **files)
{
    filesystem_compat_check_started();

    if (files == NULL)
        return;
    for (int32_t index = 0; files[index] != NULL; ++index)
        Z_FreeInternal(files[index]);
    Z_FreeInternal(files);
}

/* Source: CoDUOMP.exe 0x0043fb80..0x0043fb9b. The Mac traceback table has no
 * distinct symbol; this is the retained helper for the common concatenation
 * body below. */
static size_t Sys_CountFileListEntries(char *const *list)
{
    size_t count = 0;

    if (list != NULL) {
        while (list[count] != NULL)
            ++count;
    }
    return count;
}

/* Source: CoDUOMP.exe 0x0043fba0..0x0043fcb0; coduo_lnxded
 * 0x08075335..0x08075463. Name and ownership contract: exact same-module Mac
 * symbol Sys_ConcatenateFileLists. Input arrays surrender only their pointer
 * storage; their strings transfer into the result. */
char **Sys_ConcatenateFileLists(char **firstList, char **secondList, char **thirdList)
{
    const size_t firstCount = Sys_CountFileListEntries(firstList);
    const size_t secondCount = Sys_CountFileListEntries(secondList);
    const size_t thirdCount = Sys_CountFileListEntries(thirdList);
    char **const result = Z_MallocInternal((firstCount + secondCount + thirdCount + 1u) * sizeof(*result));
    char **writeSlot = result;

    if (firstList != NULL) {
        for (char **readSlot = firstList; *readSlot != NULL; ++readSlot)
            *writeSlot++ = *readSlot;
    }
    if (secondList != NULL) {
        for (char **readSlot = secondList; *readSlot != NULL; ++readSlot)
            *writeSlot++ = *readSlot;
    }
    if (thirdList != NULL) {
        for (char **readSlot = thirdList; *readSlot != NULL; ++readSlot)
            *writeSlot++ = *readSlot;
    }
    *writeSlot = NULL;

    if (firstList != NULL)
        Z_FreeInternal(firstList);
    if (secondList != NULL)
        Z_FreeInternal(secondList);
    if (thirdList != NULL)
        Z_FreeInternal(thirdList);
    return result;
}

/* NOT_FROM_ORIGINAL_SOURCE: isolates case-sensitive-host recovery at the
 * otherwise identical three-root mod pak probe. Both authoritative bodies
 * perform every fallback probe and pass the FS_BuildOSPath result unchanged;
 * in particular, an empty cd root is not skipped at this stage. */
static int32_t filesystem_compat_count_mod_paks(const char *root, const char *directoryName)
{
    char osPath[MAX_OSPATH];
    FS_BuildOSPath(root, directoryName, "", osPath);

    char resolvedPath[MAX_OSPATH];
    const char *listPath = osPath;
    if (filesystem_compat_resolve_case_path(root, osPath, resolvedPath, sizeof(resolvedPath)) != qfalse) {
        listPath = resolvedPath;
    }

    int32_t pakCount = 0;
    Sys_FreeFileList(Sys_ListFiles(listPath, FS_PK3_EXTENSION, NULL, &pakCount, qfalse));
    return pakCount;
}

/* Source: CoDUOMP.exe 0x0043fcc0..0x00440171; coduo_lnxded
 * 0x08075464..0x0807596a. Name and signature: exact same-module Mac symbol
 * FS_GetModList. */
int32_t FS_GetModList(char *listBuffer, int32_t bufferSize)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (listBuffer == NULL || bufferSize <= 0) {
        return 0;
    }

    listBuffer[0] = '\0';

    int32_t ignoredCount = 0;
    char **const homeDirs = Sys_ListFiles(fs_homepath->string, NULL, NULL, &ignoredCount, qtrue);
    char **const baseDirs = Sys_ListFiles(fs_basepath->string, NULL, NULL, &ignoredCount, qtrue);
    char **cdDirs = NULL;
    if (fs_cdpath->string != NULL && fs_cdpath->string[0] != '\0') {
        cdDirs = Sys_ListFiles(fs_cdpath->string, NULL, NULL, &ignoredCount, qtrue);
    }

    char **const directories = Sys_ConcatenateFileLists(homeDirs, baseDirs, cdDirs);
    const int32_t directoryCount = (int32_t)Sys_CountFileListEntries(directories);
    int32_t modCount = 0;
    int32_t listBytes = 0;

    for (int32_t directoryIndex = 0; directoryIndex < directoryCount; ++directoryIndex) {
        char *const directoryName = directories[directoryIndex];
        qboolean duplicate = qfalse;

        for (int32_t previousIndex = 0; previousIndex < directoryIndex; ++previousIndex) {
            if (Q_stricmp(directories[previousIndex], directoryName) == 0) {
                duplicate = qtrue;
                break;
            }
        }
        if (duplicate != qfalse || strncmp(directoryName, ".", 1) == 0)
            continue;

        int32_t pakCount = filesystem_compat_count_mod_paks(fs_basepath->string, directoryName);
        if (pakCount < 1) {
            pakCount = filesystem_compat_count_mod_paks(fs_cdpath->string, directoryName);
        }
        if (pakCount < 1) {
            pakCount = filesystem_compat_count_mod_paks(fs_homepath->string, directoryName);
        }
        if (pakCount < 1)
            continue;

        const size_t directoryNameLength = strlen(directoryName);
        if (directoryNameLength >= FS_PACK_NAME_SIZE) {
            Com_Printf("FS_GetModList: mod directory name exceeds FS_PACK_NAME_SIZE\n");
            continue;
        }
        const int32_t directoryNameBytes = (int32_t)directoryNameLength + 1;
        char description[FS_MOD_DESCRIPTION_SIZE];
        int32_t handle = 0;
        int32_t fileLength = 0;

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (directoryNameLength < sizeof(description) && sizeof("/" FS_MOD_DESCRIPTION_FILE) <= sizeof(description) - directoryNameLength) {
            memcpy(description, directoryName, directoryNameLength);
            memcpy(&description[directoryNameLength], "/" FS_MOD_DESCRIPTION_FILE, sizeof("/" FS_MOD_DESCRIPTION_FILE));
            fileLength = FS_SV_FOpenFileRead(description, &handle);
        }
        if (fileLength > 0 && handle != 0) {
            Com_Memset(description, 0, sizeof(description));
            const int32_t bytesRead = (int32_t)fread(description, 1, FS_MOD_DESCRIPTION_READ_LIMIT, FS_FileForHandle(handle));
            if (bytesRead >= 0)
                description[bytesRead] = '\0';
            FS_FCloseFile(handle);
        } else if (Q_stricmp(directoryName, FS_MAIN_GAME_DIR) == 0) {
            strcpy(description, FS_MAIN_GAME_DESCRIPTION);
        } else {
            memcpy(description, directoryName, directoryNameLength + 1u);
        }

        const int32_t descriptionBytes = (int32_t)strlen(description) + 1;
        if (directoryNameBytes + listBytes + descriptionBytes + 2 >= bufferSize) {
            break;
        }

        strcpy(listBuffer, directoryName);
        listBuffer += directoryNameBytes;
        strcpy(listBuffer, description);
        listBuffer += descriptionBytes;
        listBytes += directoryNameBytes + descriptionBytes;
        ++modCount;
    }

    Sys_FreeFileList(directories);
    /* NOT_FROM_ORIGINAL_SOURCE: the client compatibility provider appends
     * launchable server-cache mods; every stock/server provider is inert. */
    modCount += filesystem_compat_append_cached_mods(listBuffer, bufferSize - listBytes);
    return modCount;
}

/* Source: CoDUOMP.exe 0x0042f380..0x0042f469; coduo_lnxded
 * 0x08063f80..0x0806407c. Name and signature: exact same-module Mac symbol
 * FS_GetFileList. */
int32_t FS_GetFileList(const char *path, const char *extension, char *listBuffer, int32_t bufferSize)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (listBuffer == NULL || bufferSize <= 0) {
        return 0;
    }

    listBuffer[0] = '\0';
    if (Q_stricmp(path, "$modlist") == 0)
        return FS_GetModList(listBuffer, bufferSize);

    int32_t fileCount;
    char **const files = FS_ListFiles(path, extension, &fileCount);
    int32_t listLength = 0;
    int32_t copiedCount;

    for (copiedCount = 0; copiedCount < fileCount; ++copiedCount) {
        const int32_t fileLength = (int32_t)strlen(files[copiedCount]) + 1;
        if (bufferSize <= fileLength + listLength + 1)
            break;
        strcpy(listBuffer, files[copiedCount]);
        listBuffer += fileLength;
        listLength += fileLength;
    }

    FS_FreeFileList(files);
    return copiedCount;
}
