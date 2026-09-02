#include "filesystem.h"
#include "filesystem_services.h"

#include "qcommon/q_string.h"

#include <stdint.h>
#include <string.h>

enum {
    FS_UNIQUE_STRING_LIST_LIMIT = 4095
};

/* Source: CoDUOMP.exe 0x0042c8b0..0x0042c8bd.
 * Name: exact same-module Mac symbol FS_Initialized. */
qboolean FS_Initialized(void)
{
    return fs_searchpaths != NULL ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x0042c920..0x0042c950.
 * Name and search-path policy: exact same-module Mac symbol FS_UseSearchPath.
 * Linux accepts every localized search path unless all localized assets are
 * disabled; Windows additionally requires the active client language. */
#if defined(WINDOWS_BEHAVIOR)
qboolean FS_UseSearchPath(const searchpath_t *searchpath)
{
    if (filesystem_compat_server_scope_allows_searchpath(searchpath) ==
        qfalse) {
        return qfalse;
    }
    if (searchpath->localized == qfalse)
        return qtrue;
    if (fs_ignoreLocalized->integer != 0)
        return qfalse;
    return searchpath->language == cl_language->integer ? qtrue : qfalse;
}
#else
qboolean FS_UseSearchPath(const searchpath_t *searchpath)
{
    if (filesystem_compat_server_scope_allows_searchpath(searchpath) ==
        qfalse) {
        return qfalse;
    }
    if (searchpath->localized == qfalse ||
        fs_ignoreLocalized->integer == 0) {
        return qtrue;
    }
    return qfalse;
}
#endif

/* Source: CoDUOMP.exe 0x0042c960..0x0042c98a.
 * Name and signature: exact same-module Mac symbol FS_LanguageHasAssets. */
qboolean FS_LanguageHasAssets(int32_t language)
{
    for (const searchpath_t *searchpath = fs_searchpaths;
         searchpath != NULL; searchpath = searchpath->next) {
        if (filesystem_compat_server_scope_allows_searchpath(searchpath) !=
                qfalse &&
            searchpath->localized != qfalse &&
            searchpath->language == language) {
            return qtrue;
        }
    }
    return qfalse;
}

/* Source: CoDUOMP.exe 0x0042edf0..0x0042ee4a.
 * Name and signature: exact same-module Mac symbol FS_ReturnPath. */
int32_t FS_ReturnPath(const char *path, char *directory, int32_t *depth)
{
    int32_t separatorCount = 0;
    int32_t lastSeparator = 0;
    int32_t length = 0;

    directory[0] = '\0';
    while (path[length] != '\0') {
        if (path[length] == '/' || path[length] == '\\') {
            lastSeparator = length;
            ++separatorCount;
        }
        ++length;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: require the complete normalized catalog path
     * and NUL to fit the caller's MAX_OSPATH destination before writing. */
    if (length >= MAX_OSPATH) {
        Com_Printf("FS_ReturnPath: path exceeds MAX_OSPATH\n");
        *depth = 0;
        return -1;
    }

    memcpy(directory, path, (size_t)length + 1u);
    directory[lastSeparator] = '\0';

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (lastSeparator + 1 == length)
        --separatorCount;
    *depth = separatorCount;
    return lastSeparator;
}

/* Source: CoDUOMP.exe 0x0042ee50..0x0042eea6.
 * Name and signature: exact same-module Mac symbol FS_AddFileToList. */
int32_t FS_AddFileToList(const char *text, char **list, int32_t count)
{
    if (count == FS_UNIQUE_STRING_LIST_LIMIT)
        return count;

    for (int32_t index = 0; index < count; ++index) {
        if (Q_stricmp(text, list[index]) == 0)
            return count;
    }

    list[count] = CopyStringInternal(text);
    return count + 1;
}

/* Source: CoDUOMP.exe 0x0042f930..0x0042f966.
 * Name and insertion contract: exact same-module Mac symbol FS_AddSearchPath. */
void FS_AddSearchPath(searchpath_t *searchpath)
{
    if (searchpath->localized == qfalse || fs_searchpaths == NULL) {
        searchpath->next = fs_searchpaths;
        fs_searchpaths = searchpath;
        return;
    }

    searchpath_t *insertAfter = fs_searchpaths;
    while (insertAfter->next != NULL &&
           insertAfter->next->localized == qfalse) {
        insertAfter = insertAfter->next;
    }
    searchpath->next = insertAfter->next;
    insertAfter->next = searchpath;
}

/* Source: CoDUOMP.exe 0x004304c0..0x004305a2.
 * Name and half-open range contract: exact same-module Mac symbol
 * FS_ClearDataForFiles. uintptr_t is confined to this allocator-range
 * boundary so unrelated allocations are compared portably. */
void FS_ClearDataForFiles(const void *rangeStart, const void *rangeEnd)
{
    const uintptr_t startAddress = (uintptr_t)rangeStart;
    const uintptr_t endAddress = (uintptr_t)rangeEnd;

    for (searchpath_t *search = fs_lookupSearchpaths;
         search != NULL; search = search->next) {
        pack_t *const pack = search->pack;
        if (pack == NULL)
            continue;

        for (int32_t hash = 0; hash < pack->hashSize; ++hash) {
            for (fileInPack_t *file = pack->hashTable[hash];
                 file != NULL; file = file->next) {
                const uintptr_t payloadAddress =
                    (uintptr_t)file->data.data.generic;
                if (payloadAddress < startAddress ||
                    payloadAddress >= endAddress) {
                    continue;
                }
                if (file->data.freeData != NULL) {
                    file->data.freeData(&file->data);
                    file->data.freeData = NULL;
                }
                file->data.data.generic = NULL;
            }
        }
    }

    for (fs_dir_file_list_t *list = fs_lookupDirFileLists;
         list != NULL; list = list->next) {
        for (int32_t hash = 0; hash < list->hashSize; ++hash) {
            for (fs_dir_file_t *file = list->hashTable[hash];
                 file != NULL; file = file->next) {
                const uintptr_t payloadAddress =
                    (uintptr_t)file->data.data.generic;
                if (payloadAddress < startAddress ||
                    payloadAddress >= endAddress) {
                    continue;
                }
                if (file->data.freeData != NULL) {
                    file->data.freeData(&file->data);
                    file->data.freeData = NULL;
                }
                file->data.data.generic = NULL;
            }
        }
    }
}

/* Source: CoDUOMP.exe 0x00430630..0x00430663.
 * Name and ownership: exact same-module Mac symbol FS_ShutdownFileLists. */
void FS_ShutdownFileLists(fs_dir_file_list_t *list)
{
    while (list != NULL) {
        fs_dir_file_list_t *const next = list->next;
        Z_FreeInternal(list->fileList);
        Z_FreeInternal(list);
        list = next;
    }
}

/* Source: CoDUOMP.exe 0x00430670..0x004306aa.
 * Name: exact same-module Mac symbol FS_RefreshLookupCache. */
void FS_RefreshLookupCache(void)
{
    if (fs_lookupSearchpaths != fs_searchpaths) {
        FS_ShutdownSearchPaths(fs_lookupSearchpaths);
        fs_lookupSearchpaths = fs_searchpaths;
    }
    if (fs_lookupDirFileLists != fs_dirFileLists) {
        FS_ShutdownFileLists(fs_lookupDirFileLists);
        fs_lookupDirFileLists = fs_dirFileLists;
    }
}

/* Source: CoDUOMP.exe 0x004306b0..0x004306ed.
 * Name and ownership: exact same-module Mac symbol FS_ShutdownServerPakNames. */
void FS_ShutdownServerPakNames(void)
{
    for (int32_t index = 0; index < fs_numServerPaks; ++index) {
        if (fs_serverPakNames[index] != NULL)
            Z_FreeInternal(fs_serverPakNames[index]);
        fs_serverPakNames[index] = NULL;
    }
    fs_numServerPaks = 0;
}

/* Source: CoDUOMP.exe 0x004306f0..0x0043072d.
 * Name and ownership: exact same-module Mac symbol
 * FS_ShutdownServerReferencedPaks. */
void FS_ShutdownServerReferencedPaks(void)
{
    for (int32_t index = 0;
         index < fs_numServerReferencedPaks; ++index) {
        if (fs_serverReferencedPakNames[index] != NULL)
            Z_FreeInternal(fs_serverReferencedPakNames[index]);
        fs_serverReferencedPakNames[index] = NULL;
    }
    fs_numServerReferencedPaks = 0;
}
