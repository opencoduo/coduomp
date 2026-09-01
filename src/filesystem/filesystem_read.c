#include "filesystem.h"
#include "filesystem_services.h"

#include <stdint.h>
#include <string.h>

enum {
    FS_READ_UNIQUE_HANDLE_FIRST = 51
};

/* Source: CoDUOMP.exe 0x0042e1c0..0x0042e28a and coduo_lnxded
 * 0x08062853..0x08062982. Name and signature: exact same-module Mac symbol
 * FS_Read. Archive handles forward to the target ZIP service. Plain streams
 * retry one zero-byte read, then return the partial count; a -1 result is
 * tolerated only for unique handles 51..63. */
int32_t FS_Read(void *buffer, int32_t byteCount, int32_t handle)
{
    filesystem_compat_check_started();

    /* NOT_FROM_ORIGINAL_SOURCE: require a nonnegative transfer count before
     * accessing either the handle or destination. */
    if (byteCount < 0) {
        Com_Printf("FS_Read: negative byte count %i\n", byteCount);
        return 0;
    }

    if (handle == 0)
        return 0;

    fileHandleData_t *const fileHandle = &fs_handleFiles[handle];
    if (fileHandle->zipArchive != NULL) {
        const int32_t currentPosition =
            filesystem_compat_archive_tell(fileHandle);
        const int32_t fileLength =
            filesystem_compat_archive_length(fileHandle);
        if (currentPosition < 0 || fileLength < currentPosition) {
            char entryName[FS_HANDLE_NAME_SIZE];
            memcpy(entryName, fileHandle->name, sizeof(entryName));
            entryName[sizeof(entryName) - 1u] = '\0';
            /* NOT_FROM_ORIGINAL_SOURCE: release the current member and any
             * owned archive clone before a nonreturning drop path. */
            FS_FCloseFile(handle);
            Com_Error(ERR_DROP, "\x15" "FS_Read: invalid archive state for %s",
                      entryName);
            return -1;
        }

        const uint32_t remaining =
            (uint32_t)(fileLength - currentPosition);
        const uint32_t expectedByteCount =
            (uint32_t)byteCount < remaining ? (uint32_t)byteCount : remaining;
        const int32_t bytesRead = filesystem_compat_archive_read(
            fileHandle, buffer, (uint32_t)byteCount);

        /* NOT_FROM_ORIGINAL_SOURCE: ordinary reads may end at the declared
         * member boundary; an error or earlier short read closes the member
         * and follows the logged drop path. */
        if (bytesRead < 0 || (uint32_t)bytesRead != expectedByteCount) {
            uint32_t completed = bytesRead > 0 ? (uint32_t)bytesRead : 0;
            if (completed > expectedByteCount)
                completed = expectedByteCount;
            if (buffer != NULL && completed < expectedByteCount) {
                memset((uint8_t *)buffer + completed, 0,
                       expectedByteCount - completed);
            }
            char entryName[FS_HANDLE_NAME_SIZE];
            memcpy(entryName, fileHandle->name, sizeof(entryName));
            entryName[sizeof(entryName) - 1u] = '\0';
            FS_FCloseFile(handle);
            Com_Error(ERR_DROP,
                      "\x15" "FS_Read: malformed archive entry %s (%i of %u bytes)",
                      entryName, bytesRead, expectedByteCount);
            return -1;
        }
        return bytesRead;
    }

    uint8_t *cursor = buffer;
    uint32_t remaining = (uint32_t)byteCount;
    qboolean sawZeroByteRead = qfalse;

    while (remaining != 0) {
        const uint32_t bytesRead = (uint32_t)fread(
            cursor, 1, (size_t)remaining,
            (FILE *)fileHandle->ioObject);
        if (bytesRead == 0) {
            if (sawZeroByteRead != qfalse)
                return byteCount - (int32_t)remaining;
            sawZeroByteRead = qtrue;
        } else if (bytesRead == UINT32_MAX) {
            if (handle >= FS_READ_UNIQUE_HANDLE_FIRST &&
                handle < FS_HANDLE_COUNT) {
                return -1;
            }
            Com_Error(ERR_FATAL, "\x15" "FS_Read: -1 bytes read");
        }

        remaining -= bytesRead;
        cursor += bytesRead;
    }

    return byteCount;
}

/* Source: CoDUOMP.exe 0x0043fa30..0x0043fa71 and coduo_lnxded
 * 0x0809d47e..0x0809d4df. Role name: FS_Read2, matching the recovered Linux
 * helper. Both bodies clear and restore the streamed-read guard; the target
 * service retains Windows' folded ordinary read versus Linux's stream-thread
 * dispatch. */
int32_t FS_Read2(void *buffer, int32_t byteCount, int32_t handle)
{
    filesystem_compat_check_started();
    if (handle == 0)
        return 0;

    fileHandleData_t *const fileHandle = &fs_handleFiles[handle];
    if (fileHandle->seekCallbackGuard == qfalse)
        return FS_Read(buffer, byteCount, handle);

    fileHandle->seekCallbackGuard = qfalse;
    const int32_t bytesRead = filesystem_compat_streamed_read(
        buffer, byteCount, handle);
    fileHandle->seekCallbackGuard = qtrue;
    return bytesRead;
}
