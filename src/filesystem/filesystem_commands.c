#include "filesystem.h"
#include "filesystem_services.h"

#include "qcommon/q_command.h"

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "filesystem_commands.c requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * The console-facing filesystem command cluster is common to both engines.
 * The complete target bodies below retain the original differences in command
 * registration order and localized search-path display behavior.
 */

#if defined(WINDOWS_BEHAVIOR)
/* Source: CoDUOMP.exe 0x0042f5a0..0x0042f75f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0042f5a0_0042f760.mcode.
 * Name and argument role: exact same-module Mac symbol FS_DisplayPath. A true
 * filter suppresses localized search paths that are disabled or belong to a
 * language other than the active language. */
void FS_DisplayPath(qboolean localizedFilter)
{
    const int32_t currentLanguage = cl_language->integer;

    Com_Printf("Current language: %s\n",
               filesystem_compat_language_name(currentLanguage));
    if (fs_ignoreLocalized->integer != 0) {
        Com_Printf(
            "    localized assets are being ignored\n");
    }
    Com_Printf("Current search path:\n");

    for (const searchpath_t *search = fs_searchpaths;
         search != NULL;
         search = search->next) {
        if (filesystem_compat_server_scope_allows_searchpath(search) ==
            qfalse) {
            continue;
        }
        if (localizedFilter != qfalse &&
            search->localized != qfalse &&
            (fs_ignoreLocalized->integer != 0 ||
             search->language != cl_language->integer)) {
            continue;
        }

        if (search->pack != NULL) {
            const pack_t *const pack = search->pack;
            Com_Printf("%s (%i files)\n",
                       pack->pakFilename, pack->numFiles);
            if (search->localized != qfalse) {
                Com_Printf(
                    "    localized assets pak file for %s\n",
                    filesystem_compat_language_name(search->language));
            }
            if (fs_numServerPaks != 0) {
                if (FS_PakIsPure(pack) != qfalse)
                    Com_Printf("    on the pure list\n");
                else
                    Com_Printf("    not on the pure list\n");
            }
            continue;
        }

        const directory_t *const directory =
            search->dir;
        Com_Printf("%s/%s\n",
                   directory->path, directory->gamedir);
        if (search->localized != qfalse) {
            Com_Printf(
                "    localized assets game folder for %s\n",
                filesystem_compat_language_name(search->language));
        }
    }

    Com_Printf("\nFile Handles:\n");
    for (int32_t handle = 1;
         handle < FS_HANDLE_COUNT;
         ++handle) {
        if (fs_handleFiles[handle].ioObject != NULL) {
            Com_Printf("handle %i: %s\n",
                       handle, fs_handleFiles[handle].name);
        }
    }
}
#else
void FS_DisplayPath(qboolean localizedFilter)
{
    const searchpath_t *searchpath;
    const pack_t *pack;
    const directory_t *dir;
    int32_t handle;

    if (fs_ignoreLocalized->integer != 0) {
        Com_Printf("    localized assets are being ignored\n");
    }

    Com_Printf("Current search path:\n");
    for (searchpath = fs_searchpaths; searchpath != NULL;
         searchpath = searchpath->next) {
        if (filesystem_compat_server_scope_allows_searchpath(searchpath) ==
            qfalse) {
            continue;
        }
        if (localizedFilter == 0 || FS_UseSearchPath(searchpath) != 0) {
            if (searchpath->pack != NULL) {
                pack = searchpath->pack;
                Com_Printf("%s (%i files)\n", pack->pakFilename,
                           pack->numFiles);
                if (fs_numServerPaks != 0) {
                    if (FS_PakIsPure(pack) == 0) {
                        Com_Printf("    not on the pure list\n");
                    } else {
                        Com_Printf("    on the pure list\n");
                    }
                }
            } else {
                dir = searchpath->dir;
                Com_Printf("%s/%s\n", dir->path, dir->gamedir);
            }
        }
    }

    Com_Printf("\nFile Handles:\n");
    for (handle = 1; handle < FS_HANDLE_COUNT; handle++) {
        if (fs_handleFiles[handle].ioObject != NULL) {
            Com_Printf("handle %i: %s\n", handle,
                       fs_handleFiles[handle].name);
        }
    }
}
#endif

/* Source: CoDUOMP.exe 0x0042f760..0x0042f766.
 * Evidence: repaired executable-gap boundary and exact same-module Mac symbol
 * FS_Path_f. */
void FS_Path_f(void)
{
    FS_DisplayPath(qfalse);
}

#if defined(WINDOWS_BEHAVIOR)
/* Source: CoDUOMP.exe 0x0042f770..0x0042f779.
 * Evidence: repaired executable-gap boundary and exact same-module Mac symbol
 * FS_FullPath_f. Unlike the ordinary path command, this requests localized
 * search-path filtering. */
void FS_FullPath_f(void)
{
    FS_DisplayPath(qtrue);
}
#else
void FS_FullPath_f(void)
{
    FS_DisplayPath(qfalse);
}
#endif

/* Source: CoDUOMP.exe 0x00440180..0x00440251; coduo_lnxded
 * 0x0807596b..0x08075a5e. Name and command contract: exact same-module Mac
 * symbol FS_Dir_f. MSVC inlines the one-call FS_ListFiles wrapper while the
 * Linux compiler retains it; both original bodies therefore implement this
 * same source-level operation. */
void FS_Dir_f(void)
{
    const int32_t argumentCount = Cmd_Argc();
    if (argumentCount < 2 || argumentCount > 3) {
        Com_Printf("usage: dir <directory> [extension]\n");
        return;
    }

    const char *const path = Cmd_Argv(1);
    const char *const extension =
        argumentCount == 3 ? Cmd_Argv(2) : "";

    Com_Printf("Directory of %s %s\n", path, extension);
    Com_Printf("---------------\n");

    int32_t fileCount;
    char **const files = FS_ListFiles(path, extension, &fileCount);
    for (int32_t index = 0; index < fileCount; ++index)
        Com_Printf("%s\n", files[index]);
    FS_FreeFileList(files);
}

/* Source: CoDUOMP.exe 0x00440260..0x00440340; coduo_lnxded
 * 0x08075a5f..0x08075b4d. Name: exact same-module Mac symbol FS_NewDir_f.
 * The stripped Linux reconstruction's FS_FDir_f name described the console
 * command rather than the original function; both bodies are behaviorally
 * identical. */
void FS_NewDir_f(void)
{
    if (Cmd_Argc() < 2) {
        Com_Printf("usage: fdir <filter>\n");
        Com_Printf("example: fdir *q3dm*.bsp\n");
        return;
    }

    Com_Printf("---------------\n");

    int32_t fileCount;
    char **const files = FS_ListFilteredFiles(
        "", "", Cmd_Argv(1), &fileCount);
    FS_SortFileList(files, fileCount);

    for (int32_t index = 0; index < fileCount; ++index) {
        FS_ConvertPath(files[index]);
        Com_Printf("%s\n", files[index]);
    }
    Com_Printf("%d files listed\n", fileCount);
    FS_FreeFileList(files);
}

/* Source: CoDUOMP.exe 0x00440350..0x0044039b; coduo_lnxded
 * 0x08075b4e..0x08075b81. Name and command contract: exact same-module Mac
 * symbol FS_TouchFile_f. MSVC inlines FS_TouchFile; Linux retains the call. */
void FS_TouchFile_f(void)
{
    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: touchFile <file>\n");
        return;
    }

    (void)FS_TouchFile(Cmd_Argv(1));
}

#if defined(WINDOWS_BEHAVIOR)
/* Source: CoDUOMP.exe 0x00440930..0x0044097e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00440930_0044097f.mcode.
 * Name and registration order: exact same-module Mac symbol
 * FS_AddCommands. */
void FS_AddCommands(void)
{
    Cmd_AddCommand("fullpath", FS_FullPath_f);
    Cmd_AddCommand("path", FS_Path_f);
    Cmd_AddCommand("dir", FS_Dir_f);
    Cmd_AddCommand("fdir", FS_NewDir_f);
    Cmd_AddCommand("touchFile", FS_TouchFile_f);
}
#else
void FS_AddCommands(void)
{
    Cmd_AddCommand("path", FS_Path_f);
    Cmd_AddCommand("fullpath", FS_FullPath_f);
    Cmd_AddCommand("dir", FS_Dir_f);
    Cmd_AddCommand("fdir", FS_NewDir_f);
    Cmd_AddCommand("touchFile", FS_TouchFile_f);
}
#endif

/* Source: CoDUOMP.exe 0x00440900..0x0044092b.
 * Exact source symbol is absent from the Mac traceback table; named by its
 * command-lifecycle role. This retained, unreferenced body removes exactly
 * four commands. FS_Shutdown's separately inlined sequence also removes
 * "fullpath"; that behavioral distinction is present in the PE and is not
 * normalized here. */
void FS_RemoveCommands(void)
{
    Cmd_RemoveCommand("path");
    Cmd_RemoveCommand("dir");
    Cmd_RemoveCommand("fdir");
    Cmd_RemoveCommand("touchFile");
}
