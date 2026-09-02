#include "filesystem.h"
#include "filesystem_services.h"

enum {
    FS_BAD_MODE_SENTINEL = 6969
};

/* Source: CoDUOMP.exe 0x0042df00..0x0042df10 and coduo_lnxded
 * 0x0806252f..0x08062557. */
int32_t FS_FOpenFileReadStream(const char *qpath, int32_t *handle,
                               qboolean uniqueFile)
{
    return FS_FOpenFileRead_Internal(qpath, handle, uniqueFile, qtrue);
}

/* Source: CoDUOMP.exe 0x0042df20..0x0042df3a and coduo_lnxded
 * 0x08062558..0x0806258a.  The Linux global previously reconstructed as
 * fs_loadStack is the same file-access latch used by Windows, not the whole-
 * file temporary-allocation stack. */
int32_t FS_FOpenFileRead(const char *qpath, int32_t *handle,
                         qboolean uniqueFile)
{
    fs_fileAccessed = qtrue;
    return FS_FOpenFileRead_Internal(qpath, handle, uniqueFile, qfalse);
}

/* Source: CoDUOMP.exe 0x0042df40..0x0042df75 and coduo_lnxded
 * 0x0806258b..0x080625d0. The result tests the allocated handle rather than
 * the open return value in both targets. */
qboolean FS_TouchFile(const char *qpath)
{
    int32_t handle;

    (void)FS_FOpenFileRead(qpath, &handle, qfalse);
    if (handle == 0)
        return qfalse;

    FS_FCloseFile(handle);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00430e90..0x00430fa2 and coduo_lnxded
 * 0x08065d85..0x08065f42. Mode dispatch, result sentinel, handle bookkeeping,
 * and the null-output defect agree; FS_FTell retains target archive ownership. */
int32_t FS_FOpenFileByMode(const char *qpath, int32_t *handle, fsMode_t mode)
{
    int32_t result = FS_BAD_MODE_SENTINEL;
    qboolean sync = qfalse;

    switch (mode) {
    case FS_READ:
        result = FS_FOpenFileRead(qpath, handle, qtrue);
        break;

    case FS_WRITE:
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (handle == NULL)
            return -1;
        *handle = FS_FOpenFileWrite(qpath);
        result = *handle != 0 ? 0 : -1;
        break;

    case FS_APPEND_SYNC:
        sync = qtrue;
        /* fall through */
    case FS_APPEND:
        /* The same output contract applies to both append modes. */
        if (handle == NULL)
            return -1;
        *handle = FS_FOpenFileAppend(qpath);
        result = *handle != 0 ? 0 : -1;
        break;

    default:
        Com_Error(ERR_FATAL, "\x15" "FSH_FOpenFile: bad mode");
        break;
    }

    if (handle == NULL)
        return result;

    if (*handle != 0) {
        fileHandleData_t *const fileHandle = &fs_handleFiles[*handle];
        fileHandle->position = FS_FTell(*handle);
        fileHandle->size = result;
        fileHandle->seekCallbackGuard = qfalse;
    }
    fs_handleFiles[*handle].sync = sync;
    return result;
}
