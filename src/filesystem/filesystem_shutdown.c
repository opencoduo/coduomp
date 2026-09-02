#include "filesystem.h"
#include "filesystem_services.h"

#include "qcommon/com_lifecycle.h"
#include "qcommon/com_startup_commands.h"

#include "qcommon/q_command.h"

/* Source: CoDUOMP.exe 0x004305b0..0x0043062d and coduo_lnxded
 * 0x0806523c..0x080652e3. Name and argument role: exact same-module Mac symbol
 * FS_ShutdownSearchPaths. ZIP teardown remains behind each target's archive
 * service, while the search-path ownership graph is common. */
void FS_ShutdownSearchPaths(searchpath_t *searchpath)
{
    while (searchpath != NULL) {
        searchpath_t *const next = searchpath->next;

        if (searchpath->pack != NULL) {
            pack_t *const pack = searchpath->pack;
            filesystem_compat_pack_close(pack);
            Z_FreeInternal(pack->fileList);
            Z_FreeInternal(pack);
        }
        if (searchpath->dir != NULL)
            Z_FreeInternal(searchpath->dir);

        Z_FreeInternal(searchpath);
        searchpath = next;
    }
}

/* Source: CoDUOMP.exe 0x00430730..0x004307ff and coduo_lnxded
 * 0x080654ae..0x08065604. Name and clearLookupLists contract: exact
 * same-module Mac symbol FS_Shutdown. Client-only sound/localization/case
 * teardown remains at the target service boundary. */
void FS_Shutdown(qboolean clearLookupLists)
{
    filesystem_compat_shutdown_begin();

    for (int32_t handle = 1; handle < FS_HANDLE_COUNT; ++handle) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (handle == com_consoleLogFile ||
            handle == com_journalFile ||
            handle == com_journalDataFile) {
            continue;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (fs_handleFiles[handle].ioObject != NULL)
            FS_FCloseFile(handle);
    }

    if (clearLookupLists != qfalse) {
        FS_ShutdownSearchPaths(fs_searchpaths);
        FS_ShutdownFileLists(fs_dirFileLists);
        fs_lookupSearchpaths = NULL;
        fs_lookupDirFileLists = NULL;
    } else {
        if (fs_lookupSearchpaths != fs_searchpaths)
            FS_ShutdownSearchPaths(fs_searchpaths);
        if (fs_lookupDirFileLists != fs_dirFileLists)
            FS_ShutdownFileLists(fs_dirFileLists);
    }

    fs_searchpaths = NULL;
    fs_dirFileLists = NULL;

    Cmd_RemoveCommand("path");
    Cmd_RemoveCommand("fullpath");
    Cmd_RemoveCommand("dir");
    Cmd_RemoveCommand("fdir");
    Cmd_RemoveCommand("touchFile");
}
