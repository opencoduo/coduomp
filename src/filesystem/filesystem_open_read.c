#include "filesystem.h"
#include "filesystem_path_security.h"
#include "filesystem_services.h"

#include "compat/crt/random_compat.h"
#include "qcommon/q_path.h"
#include "qcommon/q_string.h"

#include <stdio.h>
#include <string.h>

enum {
    FS_SHADER_EXTENSION_LENGTH = 7,
    FS_SHORT_EXTENSION_LENGTH = 4,
    FS_CONFIG_EXTENSION_LENGTH = 7,
    FS_ARENA_EXTENSION_LENGTH = 6,
    FS_MENU_EXTENSION_LENGTH = 5
};

void Com_Error(errorParm_t code, const char *format, ...);
void Com_Printf(const char *format, ...);

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the identical pack
 * reference-byte updates in CoDUOMP.exe 0x0042db2b and coduo_lnxded
 * 0x08061e5f. */
static void filesystem_compat_mark_pack_references(pack_t *pack, const char *qpath)
{
    const size_t length = strlen(qpath);

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (pack->generalReference == 0 &&
        (length < FS_SHADER_EXTENSION_LENGTH || Q_stricmp(qpath + length - FS_SHADER_EXTENSION_LENGTH, ".shader") != 0) &&
        (length < FS_SHORT_EXTENSION_LENGTH || Q_stricmp(qpath + length - FS_SHORT_EXTENSION_LENGTH, ".txt") != 0) &&
        (length < FS_SHORT_EXTENSION_LENGTH || Q_stricmp(qpath + length - FS_SHORT_EXTENSION_LENGTH, ".cfg") != 0) &&
        (length < FS_CONFIG_EXTENSION_LENGTH || Q_stricmp(qpath + length - FS_CONFIG_EXTENSION_LENGTH, ".config") != 0) &&
        strstr(qpath, "levelshots") == NULL &&
        (length < FS_SHORT_EXTENSION_LENGTH || Q_stricmp(qpath + length - FS_SHORT_EXTENSION_LENGTH, ".bot") != 0) &&
        (length < FS_ARENA_EXTENSION_LENGTH || Q_stricmp(qpath + length - FS_ARENA_EXTENSION_LENGTH, ".arena") != 0) &&
        (length < FS_MENU_EXTENSION_LENGTH || Q_stricmp(qpath + length - FS_MENU_EXTENSION_LENGTH, ".menu") != 0) &&
        strstr(qpath, "soundaliases") == NULL) {
        pack->generalReference = 1;
    }

    if (pack->gameModuleReference == 0 && FS_ShiftedStrStr(qpath, "{usmgskesve~><4jrr", -6) != NULL) {
        pack->gameModuleReference = 1;
    }
    if (pack->cgameModuleReference == 0 && FS_ShiftedStrStr(qpath, "wqaeicogaoraz:80fnn", -2) != NULL) {
        pack->cgameModuleReference = 1;
    }
    if (pack->uiModuleReference == 0 && FS_ShiftedStrStr(qpath, "ztdzndrud}=;3iqq", -5) != NULL) {
        pack->uiModuleReference = 1;
    }
}

/*
 * Common read-open algorithm:
 *
 *   CoDUOMP.exe   0x0042d760..0x0042def3
 *   coduo_lnxded  0x08061a74..0x0806252f
 *
 * Search order, localized filtering, pure-pack handling, reference flags,
 * handle state, loose-file copying, diagnostics, and return values agree.
 * Target-local services retain the distinct ZIP implementation, native path
 * policy, case recovery, and CRT random boundary.
 */
int32_t FS_FOpenFileRead_Internal(const char *qpath, int32_t *handle, qboolean uniqueFile, qboolean quiet)
{
    filesystem_compat_check_started();

    /* NOT_FROM_ORIGINAL_SOURCE: retain public leading-separator normalization,
     * then require a relative virtual path before the common read algorithm. */
    if (handle != NULL) {
        while (*qpath == '/' || *qpath == '\\')
            ++qpath;
    }
    if (coduo_compat_path_is_safe_relative(qpath) == qfalse) {
        if (handle != NULL)
            *handle = 0;
        return handle != NULL ? -1 : qfalse;
    }

    if (handle == NULL) {
        for (searchpath_t *search = fs_searchpaths; search != NULL; search = search->next) {
            if (FS_UseSearchPath(search) == qfalse)
                continue;

            pack_t *const pack = search->pack;
            int32_t hash = 0;
            if (pack != NULL)
                hash = (int32_t)FS_HashFileName(qpath, pack->hashSize);

            fileInPack_t *packFile = pack != NULL ? pack->hashTable[hash] : NULL;
            if (packFile != NULL) {
                do {
                    if (FS_FilenameCompare(packFile->name, qpath) == 0)
                        return qtrue;
                    packFile = packFile->next;
                } while (packFile != NULL);
                continue;
            }

            directory_t *const directory = search->dir;
            if (directory != NULL) {
                char osPath[MAX_OSPATH];
                FS_BuildOSPath_Internal(directory->path, directory->gamedir, qpath, osPath, quiet);
                FILE *const file = filesystem_compat_fopen_read(directory->path, osPath);
                if (file != NULL) {
                    (void)fclose(file);
                    return qtrue;
                }
            }
        }
        return qfalse;
    }

    while (*qpath == '/' || *qpath == '\\')
        ++qpath;

    if (strstr(qpath, "..") != NULL || strstr(qpath, "::") != NULL) {
        *handle = 0;
        return -1;
    }

    *handle = FS_HandleForFile(quiet);
    fileHandleData_t *const fileHandle = &fs_handleFiles[*handle];
    fileHandle->uniqueObject = uniqueFile;

    pack_t *impurePack = NULL;
    for (searchpath_t *search = fs_searchpaths; search != NULL; search = search->next) {
        if (FS_UseSearchPath(search) == qfalse)
            continue;

        pack_t *const pack = search->pack;
        int32_t hash = 0;
        if (pack != NULL)
            hash = (int32_t)FS_HashFileName(qpath, pack->hashSize);

        if (strstr(qpath, ".bsp") != NULL) {
            fileInPack_t *const firstPackFile = pack != NULL ? pack->hashTable[hash] : NULL;
            if (pack != NULL && firstPackFile != NULL) {
                Q_strncpyz(fs_gameDirVar, pack->pakGamename, FS_PACK_NAME_SIZE);
            } else if (search->dir != NULL) {
                Q_strncpyz(fs_gameDirVar, search->dir->gamedir, FS_PACK_NAME_SIZE);
            }
        }

        fileInPack_t *packFile = pack != NULL ? pack->hashTable[hash] : NULL;
        if (pack != NULL && packFile != NULL) {
            do {
                if (FS_FilenameCompare(packFile->name, qpath) != 0) {
                    packFile = packFile->next;
                    continue;
                }

                if (search->localized == qfalse && FS_PakIsPure(pack) == qfalse) {
                    impurePack = pack;
                    break;
                }

                filesystem_compat_mark_pack_references(pack, qpath);
                if (filesystem_compat_archive_open_entry(pack, packFile, fileHandle, uniqueFile, quiet) == qfalse) {
                    FS_FCloseFile(*handle);
                    *handle = 0;
                    return -1;
                }

                Q_strncpyz(fileHandle->name, qpath, FS_HANDLE_NAME_SIZE);
                if (fs_debug->integer != 0 && quiet == qfalse) {
                    Com_Printf("FS_FOpenFileRead: %s (found in '%s')\n", qpath, pack->pakFilename);
                }
                const int32_t fileLength = FS_filelength(*handle);
                /* NOT_FROM_ORIGINAL_SOURCE: every successful open publishes a
                 * nonnegative length in the signed filesystem domain. */
                if (fileLength < 0) {
                    Com_Printf("FS_FOpenFileRead: refusing negative length for %s\n", qpath);
                    FS_FCloseFile(*handle);
                    *handle = 0;
                    return -1;
                }
                return fileLength;
            } while (packFile != NULL);
        } else if (search->dir != NULL) {
            const char *const extension = FS_GetExtensionSubString(qpath);
            if ((fs_restrict->integer != 0 || fs_numServerPaks != 0) && search->localized == qfalse &&
                FS_PureIgnoresExtension(extension) == qfalse) {
                continue;
            }

            directory_t *const directory = search->dir;
            char osPath[MAX_OSPATH];
            FS_BuildOSPath_Internal(directory->path, directory->gamedir, qpath, osPath, quiet);
            fileHandle->ioObject = filesystem_compat_fopen_read(directory->path, osPath);
            if (fileHandle->ioObject == NULL)
                continue;

            if (search->localized == qfalse && FS_PureIgnoresExtension(extension) == qfalse) {
                fs_fakeChkSum = coduo_server_rand() + 1;
            }

            Q_strncpyz(fileHandle->name, qpath, FS_HANDLE_NAME_SIZE);
            fileHandle->zipArchive = NULL;

            if (fs_debug->integer != 0 && quiet == qfalse) {
                Com_Printf("FS_FOpenFileRead: %s (found in '%s/%s')\n", qpath, directory->path, directory->gamedir);
            }

            if (fs_copyfiles->integer != 0 && Q_stricmp(directory->path, fs_cdpath->string) == 0) {
                char copyPath[MAX_OSPATH];
                char resolvedSourcePath[MAX_OSPATH];
                const char *sourcePath = osPath;
                if (filesystem_compat_resolve_case_path(directory->path, osPath, resolvedSourcePath, sizeof(resolvedSourcePath)) !=
                    qfalse) {
                    sourcePath = resolvedSourcePath;
                }
                FS_BuildOSPath_Internal(fs_basepath->string, directory->gamedir, qpath, copyPath, quiet);
                FS_Copyfiles(sourcePath, copyPath);
            }
            const int32_t fileLength = FS_filelength(*handle);
            /* NOT_FROM_ORIGINAL_SOURCE: a live loose-file handle must publish
             * a nonnegative length in the signed filesystem domain. */
            if (fileLength < 0) {
                Com_Printf("FS_FOpenFileRead: refusing negative length for %s\n", qpath);
                FS_FCloseFile(*handle);
                *handle = 0;
                return -1;
            }
            return fileLength;
        }
    }

    if (fs_debug->integer != 0 && quiet == qfalse)
        Com_Printf("Can't find %s\n", qpath);

    *handle = 0;
    if (impurePack != NULL) {
        /* NOT_FROM_ORIGINAL_SOURCE: keep the mounted pack filename as data
         * through the single variadic formatting pass. */
        Com_Error(ERR_DROP, "EXE_UNPURECLIENTDETECTED\x15\n%s", impurePack->pakFilename);
    }
    return -1;
}
