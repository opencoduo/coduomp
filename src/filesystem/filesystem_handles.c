#include "filesystem.h"
#include "filesystem_services.h"

#include <stdint.h>
#include <string.h>

enum {
    FS_REGULAR_HANDLE_FIRST = 1,
    FS_REGULAR_HANDLE_COUNT = 50,
    FS_UNIQUE_HANDLE_FIRST = 51,
    FS_UNIQUE_HANDLE_COUNT = 13
};

/* Source: CoDUOMP.exe 0x0042ca80..0x0042ca8c; coduo_lnxded
 * 0x080608d4..0x080608eb. Name and return type: exact same-module Mac symbol
 * FS_FileForHandle. */
FILE *FS_FileForHandle(int32_t handle)
{
    return (FILE *)fs_handleFiles[handle].ioObject;
}

/* Source: CoDUOMP.exe 0x0042c9f0..0x0042ca78; coduo_lnxded
 * 0x0806080d..0x080608d3. Name and handle partitions: exact same-module Mac
 * symbol FS_HandleForFile. */
int32_t FS_HandleForFile(qboolean uniqueFile)
{
    const int32_t firstHandle = uniqueFile != qfalse ? FS_UNIQUE_HANDLE_FIRST : FS_REGULAR_HANDLE_FIRST;
    const int32_t handleCount = uniqueFile != qfalse ? FS_UNIQUE_HANDLE_COUNT : FS_REGULAR_HANDLE_COUNT;

    for (int32_t offset = 0; offset < handleCount; ++offset) {
        const int32_t handle = firstHandle + offset;
        if (fs_handleFiles[handle].ioObject == NULL)
            return handle;
    }

    for (int32_t handle = FS_REGULAR_HANDLE_FIRST; handle < FS_HANDLE_COUNT; ++handle) {
        Com_Printf("FILE %2i: '%s'\n", handle, fs_handleFiles[handle].name);
    }

    Com_Error(ERR_DROP, "\x15"
                        "FS_HandleForFile: none free");
    return -1;
}

/* Source: CoDUOMP.exe 0x0042ca90..0x0042caab; coduo_lnxded
 * 0x080608ec..0x08060924. Name: exact same-module Mac symbol FS_ForceFlush.
 * This historical name changes the stream to unbuffered mode; it does not
 * perform fflush. */
void FS_ForceFlush(int32_t handle)
{
    (void)setvbuf(FS_FileForHandle(handle), NULL, _IONBF, 0);
}

/* Source: CoDUOMP.exe 0x00431000..0x00431013. Name: exact same-module Mac
 * symbol FS_Flush. The stripped Linux reconstruction formerly called this
 * distinct operation FS_ForceFlush. */
void FS_Flush(int32_t handle)
{
    (void)fflush(FS_FileForHandle(handle));
}

/* Source: CoDUOMP.exe 0x00430fc0..0x00430ff4. Name and argument: exact
 * same-module Mac symbol FS_FTell. */
int32_t FS_FTell(int32_t handle)
{
    const fileHandleData_t *const fileHandle = &fs_handleFiles[handle];
    if (fileHandle->zipArchive != NULL)
        return filesystem_compat_archive_tell(fileHandle);
    return (int32_t)ftell((FILE *)fileHandle->ioObject);
}

/* Source: CoDUOMP.exe 0x0042cab0..0x0042cafe; coduo_lnxded
 * 0x08060925..0x080609d2. Name and stream behavior: exact same-module Mac
 * symbol FS_filelength. */
int32_t FS_filelength(int32_t handle)
{
    filesystem_compat_check_started();

    const fileHandleData_t *const fileHandle = &fs_handleFiles[handle];
    if (fileHandle->zipArchive != NULL)
        return filesystem_compat_archive_length(fileHandle);

    FILE *const file = (FILE *)fileHandle->ioObject;
    const long currentPosition = ftell(file);
    (void)fseek(file, 0, SEEK_END);
    const int32_t fileLength = (int32_t)ftell(file);
    (void)fseek(file, currentPosition, SEEK_SET);
    return fileLength;
}

/* Source: CoDUOMP.exe 0x0042d0d0..0x0042d163. Name and ownership contract:
 * exact same-module Mac symbol FS_FCloseFile. Both engines close the current
 * archive entry, close a unique archive object, then clear the full handle.
 * The dedicated adapter additionally ends its streamed-file state first. */
void FS_FCloseFile(int32_t handle)
{
    filesystem_compat_check_started();
    filesystem_compat_end_streamed_file(handle);

    fileHandleData_t *const fileHandle = &fs_handleFiles[handle];
    if (fileHandle->zipArchive != NULL) {
        filesystem_compat_archive_close_current(fileHandle);
        if (fileHandle->uniqueObject != qfalse)
            filesystem_compat_archive_close(fileHandle);
    } else if (fileHandle->ioObject != NULL) {
        (void)fclose((FILE *)fileHandle->ioObject);
    }

    memset(fileHandle, 0, sizeof(*fileHandle));
}
