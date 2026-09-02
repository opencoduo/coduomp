#include "filesystem.h"
#include "filesystem_services.h"

#include "qcommon/com_sprintf.h"
#include "qcommon/q_string.h"

#include <stdint.h>
#include <string.h>

enum {
    FS_LOOSE_FILE_HASH_GROWTH_LIMIT = 1024,
    FS_DATA_LOOKUP_PATH_SIZE = 1024
};

/* Source: CoDUOMP.exe 0x00430000..0x004302f8 and coduo_lnxded
 * 0x08064ba4..0x08064f6c. Name and logical four-argument signature: exact
 * same-module Mac symbol FS_AddNonPackFileDirectory_Internal. The maintained
 * common path uses bounded formatting; target services retain case recovery
 * and the distinct loading-keepalive boundaries. */
void FS_AddNonPackFileDirectory_Internal(const char *base, const char *game, const char *path, const char *extension)
{
    char gamePath[MAX_OSPATH];
    char osPath[MAX_OSPATH];

    /* NOT_FROM_ORIGINAL_SOURCE: require the complete loose-asset directory
     * and NUL to fit; never select a truncated path. */
    const size_t gameLength = strlen(game);
    const size_t pathLength = strlen(path);
    if (gameLength > sizeof(gamePath) - 2u || pathLength > sizeof(gamePath) - gameLength - 2u) {
        Com_Printf("WARNING: loose asset directory '%s/%s' is too long\n", game, path);
        return;
    }
    Com_sprintf(gamePath, sizeof(gamePath), "%s/%s", game, path);
    FS_BuildOSPath(base, gamePath, "", osPath);
    osPath[strlen(osPath) - 1] = '\0';
    char resolvedPath[MAX_OSPATH];
    const char *directoryPath = osPath;
    if (filesystem_compat_resolve_case_path(base, osPath, resolvedPath, sizeof(resolvedPath)) != qfalse) {
        directoryPath = resolvedPath;
    }

    int32_t fileCount;
    char **const files = Sys_ListFiles(directoryPath, extension, NULL, &fileCount, qfalse);

    int32_t namesLength = 0;
    for (int32_t index = 0; index < fileCount; ++index)
        namesLength += (int32_t)strlen(files[index]) + 1;

    int32_t hashSize = 1;
    while (hashSize <= fileCount && hashSize <= FS_LOOSE_FILE_HASH_GROWTH_LIMIT) {
        hashSize <<= 1;
    }

    fs_dir_file_list_t *const fileList = Z_MallocInternal(sizeof(*fileList) + (size_t)hashSize * sizeof(fileList->hashTable[0]));
    fileList->hashSize = hashSize;
    fileList->hashTable = (fs_dir_file_t **)(fileList + 1);
    for (int32_t index = 0; index < fileList->hashSize; ++index)
        fileList->hashTable[index] = NULL;

    Q_strncpyz(fileList->path, path, sizeof(fileList->path));
    fileList->next = fs_dirFileLists;
    fs_dirFileLists = fileList;
    fileList->numFiles = fileCount;

    fs_dir_file_t *const entries = Z_MallocInternal((size_t)fileCount * sizeof(*entries) + (size_t)namesLength);
    char *nameCursor = (char *)(entries + fileCount);

    for (int32_t index = 0; index < fileCount; ++index) {
        char *const name = Q_strlwr(files[index]);
        const uint32_t hash = FS_HashFileName(name, fileList->hashSize);
        fs_dir_file_t *const entry = &entries[index];

        entry->data.name = nameCursor;
        strcpy(nameCursor, name);
        nameCursor += strlen(name) + 1u;

        entry->next = fileList->hashTable[hash];
        entry->data.data.generic = NULL;
        entry->data.freeData = NULL;
        fileList->hashTable[hash] = entry;
    }
    fileList->fileList = entries;

    Sys_FreeFileList(files);
    filesystem_compat_loading_keepalive();
}

/* Source: CoDUOMP.exe 0x00430300..0x00430332 and coduo_lnxded
 * 0x08064f6d..0x08064fc4. Name and signature: exact same-module Mac symbol
 * FS_AddNonPackFileDirectory. Every active host directory contributes one
 * loose-file index for the requested relative path and extension. */
void FS_AddNonPackFileDirectory(const char *path, const char *extension)
{
    for (searchpath_t *searchpath = fs_searchpaths; searchpath != NULL; searchpath = searchpath->next) {
        if (filesystem_compat_server_scope_allows_searchpath(searchpath) != qfalse && searchpath->pack == NULL) {
            directory_t *const directory = searchpath->dir;
            FS_AddNonPackFileDirectory_Internal(directory->path, directory->gamedir, path, extension);
        }
    }
}

/* Source: CoDUOMP.exe 0x00430340..0x004304b8 and coduo_lnxded
 * 0x08064fc5..0x08065180. Name and three-argument signature: exact
 * same-module Mac symbol FS_GetDataForFile. Pack hits return the common data
 * record beginning at basename, while loose hits already begin with the
 * equivalent name/payload/callback fields. */
fileData_t *FS_GetDataForFile(const char *base, const char *path, const char *extension)
{
    char lookupPath[FS_DATA_LOOKUP_PATH_SIZE];

    /* NOT_FROM_ORIGINAL_SOURCE: the complete pack spelling is the longer
     * lookup form; requiring it to fit proves both formatted paths. */
    const size_t baseLength = strlen(base);
    const size_t pathLength = strlen(path);
    const size_t extensionLength = strlen(extension);
    if (baseLength > sizeof(lookupPath) - 2u || pathLength > sizeof(lookupPath) - baseLength - 2u ||
        extensionLength > sizeof(lookupPath) - baseLength - pathLength - 2u) {
        Com_Printf("WARNING: asset lookup path '%s/%s%s' is too long\n", base, path, extension);
        return NULL;
    }
    Com_sprintf(lookupPath, sizeof(lookupPath), "%s/%s%s", base, path, extension);

    for (searchpath_t *searchpath = fs_lookupSearchpaths; searchpath != NULL; searchpath = searchpath->next) {
        if (FS_UseSearchPath(searchpath) == qfalse)
            continue;

        pack_t *const pack = searchpath->pack;
        if (pack == NULL)
            continue;

        const uint32_t hash = FS_HashFileName(lookupPath, pack->hashSize);
        for (fileInPack_t *file = pack->hashTable[hash]; file != NULL; file = file->next) {
            if (FS_FilenameCompare(file->name, lookupPath) == 0)
                return &file->data;
        }
    }

    Com_sprintf(lookupPath, sizeof(lookupPath), "%s%s", path, extension);
    for (fs_dir_file_list_t *list = fs_lookupDirFileLists; list != NULL; list = list->next) {
        if (fs_compat_stricmp(list->path, base) != 0)
            continue;

        const uint32_t hash = FS_HashFileName(lookupPath, list->hashSize);
        for (fs_dir_file_t *file = list->hashTable[hash]; file != NULL; file = file->next) {
            if (FS_FilenameCompare(file->data.name, lookupPath) == 0)
                return &file->data;
        }
    }

    return NULL;
}
