#include "filesystem.h"
#include "filesystem_path_security.h"
#include "filesystem_services.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Source: CoDUOMP.exe 0x0042d170..0x0042d25f. Name: exact same-module Mac
 * symbol FS_FOpenFileWrite. The Linux dedicated body performs the same handle,
 * path, create, fopen, name, and sync operations after its standard
 * initialized-state assertion. */
int32_t FS_FOpenFileWrite(const char *qpath)
{
    filesystem_compat_check_started();

    /* NOT_FROM_ORIGINAL_SOURCE: require a relative virtual path below the
     * selected writable root before host-path construction. */
    if (coduo_compat_path_is_safe_relative(qpath) == qfalse)
        return 0;


    const int32_t handle = FS_HandleForFile(qfalse);
    fileHandleData_t *const fileHandle = &fs_handleFiles[handle];
    char osPath[MAX_OSPATH];

    fileHandle->zipArchive = NULL;
    FS_BuildOSPath(filesystem_compat_state_root(fs_homepath->string), fs_currentGameDir, qpath, osPath);

    if (fs_debug->integer != 0)
        Com_Printf("FS_FOpenFileWrite: %s\n", osPath);

    if (FS_CreatePath(osPath) != qfalse)
        return 0;

    fileHandle->ioObject = fopen(osPath, "wb");
    Q_strncpyz(fileHandle->name, qpath, FS_HANDLE_NAME_SIZE);
    fileHandle->sync = qfalse;
    return fileHandle->ioObject != NULL ? handle : 0;
}

/* Source: CoDUOMP.exe 0x0042d260..0x0042d34f. Name: exact same-module Mac
 * symbol FS_FOpenTextFileWrite. The Linux dedicated body has the same
 * operation order and differs only at the standard target services. */
int32_t FS_FOpenTextFileWrite(const char *qpath)
{
    filesystem_compat_check_started();

    if (coduo_compat_path_is_safe_relative(qpath) == qfalse)
        return 0;

    const int32_t handle = FS_HandleForFile(qfalse);
    fileHandleData_t *const fileHandle = &fs_handleFiles[handle];
    char osPath[MAX_OSPATH];

    fileHandle->zipArchive = NULL;
    FS_BuildOSPath(filesystem_compat_state_root(fs_homepath->string), fs_currentGameDir, qpath, osPath);

    if (fs_debug->integer != 0)
        Com_Printf("FS_FOpenFileWrite: %s\n", osPath);

    if (FS_CreatePath(osPath) != qfalse)
        return 0;

    fileHandle->ioObject = fopen(osPath, "wt");
    Q_strncpyz(fileHandle->name, qpath, FS_HANDLE_NAME_SIZE);
    fileHandle->sync = qfalse;
    return fileHandle->ioObject != NULL ? handle : 0;
}

/* Source: CoDUOMP.exe 0x0042d350..0x0042d439. Name: exact same-module Mac
 * symbol FS_FOpenFileAppend. The Linux dedicated body agrees on the recovered
 * algorithm and its placement of the handle name before path construction. */
int32_t FS_FOpenFileAppend(const char *qpath)
{
    filesystem_compat_check_started();

    if (coduo_compat_path_is_safe_relative(qpath) == qfalse)
        return 0;

    const int32_t handle = FS_HandleForFile(qfalse);
    fileHandleData_t *const fileHandle = &fs_handleFiles[handle];
    char osPath[MAX_OSPATH];

    fileHandle->zipArchive = NULL;
    Q_strncpyz(fileHandle->name, qpath, FS_HANDLE_NAME_SIZE);
    FS_BuildOSPath(filesystem_compat_state_root(fs_homepath->string), fs_currentGameDir, qpath, osPath);

    if (fs_debug->integer != 0)
        Com_Printf("FS_FOpenFileAppend: %s\n", osPath);

    if (FS_CreatePath(osPath) != qfalse)
        return 0;

    fileHandle->ioObject = fopen(osPath, "ab");
    fileHandle->sync = qfalse;
    return fileHandle->ioObject != NULL ? handle : 0;
}

/* Source: CoDUOMP.exe 0x0043f470..0x0043f588. Name and root-path behavior:
 * exact same-module Mac symbol FS_SV_FOpenFileWrite. The Linux dedicated body
 * agrees: this variant writes below fs_homepath, not fs_currentGameDir. */
int32_t FS_SV_FOpenFileWrite(const char *qpath)
{
    return coduomp_fs_root_fopen_file_write(fs_homepath->string, qpath);
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit-root form used only by the client
 * server-namespace adapter. FS_SV_FOpenFileWrite above preserves the
 * original fs_homepath behavior. */
int32_t coduomp_fs_root_fopen_file_write(const char *root, const char *qpath)
{
    filesystem_compat_check_started();

    if (root == NULL || root[0] == '\0' || coduo_compat_path_is_safe_relative(qpath) == qfalse) {
        return 0;
    }

    char osPath[MAX_OSPATH];
    FS_BuildOSPath(root, qpath, "", osPath);
    osPath[strlen(osPath) - 1] = '\0';

    const int32_t handle = FS_HandleForFile(qfalse);
    fileHandleData_t *const fileHandle = &fs_handleFiles[handle];
    fileHandle->zipArchive = NULL;

    if (fs_debug->integer != 0)
        Com_Printf("FS_SV_FOpenFileWrite: %s\n", osPath);

    if (FS_CreatePath(osPath) != qfalse)
        return 0;

    Com_DPrintf("writing to: %s\n", osPath);
    fileHandle->ioObject = fopen(osPath, "wb");
    Q_strncpyz(fileHandle->name, qpath, FS_HANDLE_NAME_SIZE);
    fileHandle->sync = qfalse;
    return fileHandle->ioObject != NULL ? handle : 0;
}

/* Source: CoDUOMP.exe 0x0043f590..0x0043f7c2. Name and search order: exact
 * same-module Mac symbol FS_SV_FOpenFileRead. Both authoritative engines
 * search homepath, a distinct basepath, then cdpath. */
int32_t FS_SV_FOpenFileRead(const char *qpath, int32_t *handleOut)
{
    filesystem_compat_check_started();

    if (coduo_compat_path_is_safe_relative(qpath) == qfalse) {
        *handleOut = 0;
        return 0;
    }

    int32_t handle = FS_HandleForFile(qfalse);
    fileHandleData_t *fileHandle = &fs_handleFiles[handle];
    char osPath[MAX_OSPATH];

    fileHandle->zipArchive = NULL;
    Q_strncpyz(fileHandle->name, qpath, FS_HANDLE_NAME_SIZE);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    FS_BuildOSPath(fs_homepath->string, qpath, "", osPath);
    osPath[strlen(osPath) - 1] = '\0';
    if (fs_debug->integer != 0)
        Com_Printf("FS_SV_FOpenFileRead (fs_homepath): %s\n", osPath);
    fileHandle->ioObject = filesystem_compat_fopen_read(fs_homepath->string, osPath);
    fileHandle->sync = qfalse;

    if (fileHandle->ioObject == NULL && Q_stricmp(fs_homepath->string, fs_basepath->string) != 0) {
        FS_BuildOSPath(fs_basepath->string, qpath, "", osPath);
        osPath[strlen(osPath) - 1] = '\0';
        if (fs_debug->integer != 0)
            Com_Printf("FS_SV_FOpenFileRead (fs_basepath): %s\n", osPath);
        fileHandle->ioObject = filesystem_compat_fopen_read(fs_basepath->string, osPath);
        fileHandle->sync = qfalse;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    fileHandle = &fs_handleFiles[handle];
    if (fileHandle->ioObject == NULL) {
        FS_BuildOSPath(fs_cdpath->string, qpath, "", osPath);
        osPath[strlen(osPath) - 1] = '\0';
        if (fs_debug->integer != 0)
            Com_Printf("FS_SV_FOpenFileRead (fs_cdpath) : %s\n", osPath);
        fileHandle->ioObject = filesystem_compat_fopen_read(fs_cdpath->string, osPath);
        fileHandle->sync = qfalse;
        if (fileHandle->ioObject == NULL)
            handle = 0;
    }

    *handleOut = handle;
    return handle != 0 ? FS_filelength(handle) : 0;
}
