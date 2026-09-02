#include "filesystem.h"
#include "filesystem_services.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    FS_PRINTF_BUFFER_SIZE = 4096
};

/* Source: CoDUOMP.exe 0x0042e340..0x0042e3ae. Name and signature: exact
 * same-module Mac symbol FS_Printf. */
void FS_Printf(int32_t fileHandle, const char *format, ...)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    char buffer[FS_PRINTF_BUFFER_SIZE];
    va_list arguments;

    va_start(arguments, format);
    const int32_t formattedLength = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);

    /* NOT_FROM_ORIGINAL_SOURCE: write only a complete formatted record that
     * fits the fixed staging buffer. */
    if (formattedLength < 0 || (size_t)formattedLength >= sizeof(buffer)) {
        Com_Printf("FS_Printf: formatted output exceeds %i bytes\n", FS_PRINTF_BUFFER_SIZE - 1);
        return;
    }

    (void)FS_Write(buffer, formattedLength, fileHandle);
}

/* Source: CoDUOMP.exe 0x0042e290..0x0042e333 and coduo_lnxded
 * 0x08062983..0x08062a7e. Name and signature: exact same-module Mac symbol
 * FS_Write. */
int32_t FS_Write(const void *buffer, int32_t byteCount, int32_t handle)
{
    filesystem_compat_check_started();

    /* NOT_FROM_ORIGINAL_SOURCE: require a nonnegative transfer count before
     * accessing either the handle or source. */
    if (byteCount < 0) {
        Com_Printf("FS_Write: negative byte count %i\n", byteCount);
        return 0;
    }

    if (handle == 0)
        return 0;

    fileHandleData_t *const fileHandle = &fs_handleFiles[handle];
    FILE *const file = (FILE *)fileHandle->ioObject;
    const uint8_t *cursor = buffer;

    uint32_t remaining = (uint32_t)byteCount;
    qboolean sawZeroByteWrite = qfalse;
    while (remaining != 0) {
        const uint32_t bytesWritten = (uint32_t)fwrite(cursor, 1, (size_t)remaining, file);
        if (bytesWritten == 0) {
            if (sawZeroByteWrite != qfalse) {
                Com_Printf("FS_Write: 0 bytes written\n");
                return 0;
            }
            sawZeroByteWrite = qtrue;
        } else if (bytesWritten == UINT32_MAX) {
            Com_Printf("FS_Write: -1 bytes written\n");
            return 0;
        }

        remaining -= bytesWritten;
        cursor += bytesWritten;
    }

    if (fileHandle->sync != qfalse)
        (void)fflush(file);
    return byteCount;
}

/* Source: CoDUOMP.exe 0x0042e910..0x0042e967. Name and signature: exact
 * same-module Mac symbol FS_WriteFile. */
void FS_WriteFile(const char *qpath, const void *buffer, int32_t byteCount)
{
    filesystem_compat_check_started();

    if (qpath == NULL || buffer == NULL)
        Com_Error(ERR_FATAL, "\x15"
                             "FS_WriteFile: NULL parameter");

    const int32_t handle = FS_FOpenFileWrite(qpath);
    if (handle == 0) {
        Com_Printf("Failed to open %s\n", qpath);
        return;
    }

    (void)FS_Write(buffer, byteCount, handle);
    FS_FCloseFile(handle);
}
