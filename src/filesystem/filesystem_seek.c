#include "filesystem.h"
#include "filesystem_services.h"

#include <stdio.h>
#include <stdint.h>

enum {
    FS_ZIP_SEEK_BUFFER_SIZE = 65536
};

/* Source: CoDUOMP.exe 0x0042e3b0..0x0042e5f5 and coduo_lnxded
 * 0x08062ad4..0x08062eb0. Name and signature: exact same-module Mac symbol
 * FS_Seek. The original offset is one signed 32-bit word throughout. Archive
 * backward movement reopens the saved member and forward movement consumes
 * decompressed bytes. Target services retain the Windows recursive streamed
 * seek versus Linux stream-thread dispatch and each ZIP implementation. */
int32_t FS_Seek(int32_t handle, int32_t offset, int32_t origin)
{
    filesystem_compat_check_started();
    fileHandleData_t *const fileHandle = &fs_handleFiles[handle];

    if (fileHandle->seekCallbackGuard != qfalse) {
        fileHandle->seekCallbackGuard = qfalse;
        filesystem_compat_stream_seek(handle, offset, origin);
        fileHandle->seekCallbackGuard = qtrue;
    }

    if (fileHandle->zipArchive != NULL) {
        if (offset == 0 && origin == FS_SEEK_ORIGIN_SET)
            return filesystem_compat_archive_rewind(fileHandle);
        if (offset == 0 && origin == FS_SEEK_ORIGIN_CURRENT)
            return 0;

        const int32_t currentPosition =
            filesystem_compat_archive_tell(fileHandle);
        int32_t fileLength = 0;

        int64_t targetPosition;
        if (origin == FS_SEEK_ORIGIN_CURRENT) {
            targetPosition = (int64_t)currentPosition + (int64_t)offset;
        } else if (origin == FS_SEEK_ORIGIN_END) {
            fileLength = FS_filelength(handle);
            if (fileLength < 0) {
                Com_Printf("FS_Seek: invalid archive length\n");
                return -1;
            }
            targetPosition = (int64_t)fileLength + (int64_t)offset;
        } else if (origin == FS_SEEK_ORIGIN_SET) {
            targetPosition = (int64_t)offset;
        } else {
            return -1;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: validate the resolved archive position in
         * the nonnegative signed domain before any rewind changes state. */
        if (currentPosition < 0 || targetPosition < 0 ||
            targetPosition > INT32_MAX) {
            Com_Printf("FS_Seek: invalid archive target position\n");
            return -1;
        }

        if (origin == FS_SEEK_ORIGIN_CURRENT) {
            if (offset < 0) {
                if (filesystem_compat_archive_rewind(fileHandle) != 0)
                    return -1;
                offset = (int32_t)((uint32_t)offset +
                                   (uint32_t)currentPosition);
            }
        } else if (origin == FS_SEEK_ORIGIN_END) {
            /* NOT_FROM_ORIGINAL_SOURCE: derive the target from the single
             * validated live length above. */
            if (targetPosition < currentPosition) {
                if (filesystem_compat_archive_rewind(fileHandle) != 0)
                    return -1;
                offset = (int32_t)targetPosition;
            } else {
                offset = (int32_t)(targetPosition - currentPosition);
            }
        } else if (origin == FS_SEEK_ORIGIN_SET) {
            if (offset < currentPosition) {
                if (filesystem_compat_archive_rewind(fileHandle) != 0)
                    return -1;
            } else {
                offset = (int32_t)((uint32_t)offset -
                                   (uint32_t)currentPosition);
            }
        }

        uint8_t discard[FS_ZIP_SEEK_BUFFER_SIZE];
        while (offset != 0) {
            int32_t requestedBytes;
            if (offset < FS_ZIP_SEEK_BUFFER_SIZE) {
                requestedBytes = offset;
            } else {
                requestedBytes = FS_ZIP_SEEK_BUFFER_SIZE;
            }
            const int32_t bytesRead =
                FS_Read(discard, requestedBytes, handle);

            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (bytesRead != requestedBytes)
                return -1;
            offset -= requestedBytes;
        }
        return 0;
    }

    int32_t whence;
    switch (origin) {
    case FS_SEEK_ORIGIN_CURRENT:
        whence = SEEK_CUR;
        break;
    case FS_SEEK_ORIGIN_END:
        whence = SEEK_END;
        break;
    case FS_SEEK_ORIGIN_SET:
        whence = SEEK_SET;
        break;
    default:
        return 0;
    }
    return fseek((FILE *)fileHandle->ioObject, offset, whence);
}
