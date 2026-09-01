#include "filesystem.h"
#include "filesystem_path_security.h"
#include "filesystem_services.h"

#include "qcommon/com_sprintf.h"
#include "compat/crt/qsort_compat.h"
#include "qcommon/q_string.h"

#include <stdlib.h>
#include <string.h>

enum {
    FS_MAX_PAKFILES_PER_DIRECTORY = 1024,
    FS_LOCALIZED_PREFIX_LENGTH = 10,
    FS_LOCALIZED_GAME_NAME_SIZE = 64
};

/* Source: CoDUOMP.exe 0x0042f970..0x0042fd47 and coduo_lnxded
 * 0x080645a4..0x0806490d. Name and argument roles: exact same-module Mac
 * symbol FS_AddPakFilesForGameDirectory. Localized pak prefixes are
 * temporarily replaced with spaces so paksort can group them, then restored
 * before each archive is validated and loaded. The target service retains the
 * original Windows all-language versus Linux English-only acceptance policy. */
void FS_AddPakFilesForGameDirectory(const char *base, const char *game)
{
    char *sortedPakFiles[FS_MAX_PAKFILES_PER_DIRECTORY];
    char osPath[MAX_OSPATH];

    FS_BuildOSPath(base, game, "", osPath);
    osPath[strlen(osPath) - 1] = '\0';
    char resolvedDirectory[MAX_OSPATH];
    const char *directoryPath = osPath;
    if (filesystem_compat_resolve_case_path(
            base, osPath, resolvedDirectory,
            sizeof(resolvedDirectory)) != qfalse) {
        directoryPath = resolvedDirectory;
    }

    int32_t numPakFiles;
    char **const pakFiles =
        Sys_ListFiles(directoryPath, ".pk3", NULL,
                      &numPakFiles, qfalse);
    if (numPakFiles > FS_MAX_PAKFILES_PER_DIRECTORY) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        Com_Printf(
            "WARNING: Exceeded max number of pak files in %s/%s "
            "(%i/%i)\n",
            base, game, numPakFiles,
            FS_MAX_PAKFILES_PER_DIRECTORY);
        numPakFiles = FS_MAX_PAKFILES_PER_DIRECTORY;
    }

    for (int32_t index = 0; index < numPakFiles; ++index) {
        sortedPakFiles[index] = pakFiles[index];
        if (Q_strncmp(sortedPakFiles[index], "localized_",
                      FS_LOCALIZED_PREFIX_LENGTH) == 0) {
            memcpy(sortedPakFiles[index], "          ",
                   FS_LOCALIZED_PREFIX_LENGTH);
        }
    }

    coduo_qsort(sortedPakFiles, (size_t)numPakFiles,
                sizeof(sortedPakFiles[0]), paksort);

    for (int32_t index = 0; index < numPakFiles; ++index) {
        qboolean localized;
        int32_t language;

        if (Q_strncmp(sortedPakFiles[index], "          ",
                      FS_LOCALIZED_PREFIX_LENGTH) == 0) {
            memcpy(sortedPakFiles[index], "localized_",
                   FS_LOCALIZED_PREFIX_LENGTH);
            localized = qtrue;

            const char *const languageName =
                PakFileLanguage(sortedPakFiles[index]);
            if (languageName[0] == '\0') {
                Com_Printf(
                    "WARNING: Localized assets pak file %s/%s/%s has "
                    "invalid name (no language specified). Proper naming "
                    "convention is: localized_[language]_pak#.pk3\n",
                    base, game, sortedPakFiles[index]);
                continue;
            }

            const int32_t languageCount =
                filesystem_compat_language_count();
            for (language = 0; language < languageCount; ++language) {
                const char *const supportedName =
                    filesystem_compat_language_name(language);
                if (supportedName != NULL &&
                    Q_stricmp(languageName, supportedName) == 0) {
                    break;
                }
            }
            if (language == languageCount) {
                filesystem_compat_report_unsupported_pak_language(
                    base, game, sortedPakFiles[index]);
                continue;
            }
        } else {
            localized = qfalse;
            language = 0;
        }

        FS_BuildOSPath(base, game, sortedPakFiles[index], osPath);
        char resolvedPakPath[MAX_OSPATH];
        const char *pakPath = osPath;
        if (filesystem_compat_resolve_case_path(
                base, osPath, resolvedPakPath,
                sizeof(resolvedPakPath)) != qfalse) {
            pakPath = resolvedPakPath;
        }
        pack_t *const pack =
            FS_LoadZipFile(pakPath, sortedPakFiles[index]);
        if (pack == NULL)
            continue;

        strcpy(pack->pakGamename, game);
        searchpath_t *const searchpath =
            Z_MallocInternal(sizeof(*searchpath));
        searchpath->pack = pack;
        searchpath->localized = localized;
        searchpath->language = language;
        FS_AddSearchPath(searchpath);
    }

    Sys_FreeFileList(pakFiles);
}

/* Source: CoDUOMP.exe 0x0042fd50..0x0042ffcf and coduo_lnxded
 * 0x0806490e..0x08064b5a. Name and logical four-argument signature: exact
 * same-module Mac symbol FS_AddGameDirectory. Every original caller supplies
 * a non-null filesystem-root base; the Windows null test has no behavior in
 * that proven call domain and is retained as the canonical source guard. */
void FS_AddGameDirectory(const char *base, const char *game,
                         qboolean localized, int32_t language)
{
    char gameName[FS_LOCALIZED_GAME_NAME_SIZE];
    if (localized != qfalse) {
        Com_sprintf(gameName, sizeof(gameName), "%s_%s", game,
                    filesystem_compat_language_name(language));
    } else {
        Q_strncpyz(gameName, game, sizeof(gameName));
    }

    for (searchpath_t *search = fs_searchpaths;
         search != NULL; search = search->next) {
        directory_t *const directory = search->dir;
        if (directory == NULL || base == NULL ||
            Q_stricmp(directory->path, base) != 0 ||
            Q_stricmp(directory->gamedir, gameName) != 0) {
            continue;
        }

        if (search->localized != localized) {
            const char *const existingFolderType =
                search->localized == qfalse
                    ? "non-localized"
                    : "localized";
            Com_Printf(
                "WARNING: game folder %s/%s added as both localized & "
                "non-localized. Using folder as %s\n",
                base, gameName, existingFolderType);
        }

        if (search->localized == qfalse ||
            search->language == language) {
            return;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        Com_Printf(
            "WARNING: game golder %s/%s re-added as localized folder "
            "with different language\n",
            base, gameName);
        return;
    }

    if (localized != qfalse) {
        char osPath[MAX_OSPATH];
        FS_BuildOSPath(base, gameName, "", osPath);
        osPath[strlen(osPath) - 1] = '\0';
        char resolvedPath[MAX_OSPATH];
        const char *directoryPath = osPath;
        if (filesystem_compat_resolve_case_path(
                base, osPath, resolvedPath,
                sizeof(resolvedPath)) != qfalse) {
            directoryPath = resolvedPath;
        }
        if (FS_DirectoryHasNonDotEntries(directoryPath) == qfalse)
            return;
    } else {
        Q_strncpyz(fs_currentGameDir, gameName,
                   sizeof(fs_currentGameDir));
    }

    searchpath_t *const searchpath =
        Z_MallocInternal(sizeof(*searchpath));
    directory_t *const directory =
        Z_MallocInternal(sizeof(*directory));
    searchpath->dir = directory;

    Q_strncpyz(directory->path, base, sizeof(directory->path));
    Q_strncpyz(directory->gamedir, gameName,
               sizeof(directory->gamedir));

    searchpath->localized = localized;
    searchpath->language = language;
    FS_AddSearchPath(searchpath);
    FS_AddPakFilesForGameDirectory(base, gameName);
}

/* Source: CoDUOMP.exe 0x0042ffd0..0x0042fff7 and coduo_lnxded
 * 0x08064b5b..0x08064b8d. Name and two-argument signature: exact same-module
 * Mac symbol FS_AddLocalizedGameDirectory. The target language-count service
 * retains Windows' 13..0 walk and Linux dedicated's English-only walk. */
void FS_AddLocalizedGameDirectory(const char *base, const char *game)
{
    /* NOT_FROM_ORIGINAL_SOURCE: a game directory must remain relative and
     * below the selected base before it becomes a search path. */
    if (coduo_compat_path_is_safe_relative(game) == qfalse) {
        Com_Printf("WARNING: refusing unsafe game directory '%s'\n",
                   game != NULL ? game : "");
        return;
    }

    for (int32_t language =
             filesystem_compat_language_count() - 1;
         language >= 0; --language) {
        FS_AddGameDirectory(base, game, qtrue, language);
    }
    FS_AddGameDirectory(base, game, qfalse, 0);
}
