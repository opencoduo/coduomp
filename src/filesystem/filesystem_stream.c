#include "filesystem.h"

/*
 * Complete streamed-file operation cluster shared by both engine targets.
 * The original bodies differ only in compiler calling convention:
 *
 *   CoDUOMP.exe     Sys_StreamedRead      0x00468ef0
 *                   Sys_StreamSeek       0x00468f10
 *                   Sys_EndStreamedFile   0x00468f30
 *                   Sys_BeginStreamedFile 0x00468f40
 *   coduo_lnxded    Sys_BeginStreamedFile 0x080c9349
 *                   Sys_EndStreamedFile   0x080c934e
 *                   Sys_StreamedRead      0x080c9353
 *                   Sys_StreamSeek        0x080c9378
 *
 * Begin/end are empty platform hooks in both binaries. Read multiplies the
 * two signed dword extents and forwards the wrapped dword byte count to
 * FS_Read; seek forwards its three dword arguments unchanged to FS_Seek.
 */

void Sys_BeginStreamedFile(int32_t handle, int32_t readAhead)
{
    (void)handle;
    (void)readAhead;
}

void Sys_EndStreamedFile(int32_t handle)
{
    (void)handle;
}

int32_t Sys_StreamedRead(void *buffer, int32_t size, int32_t count,
                         int32_t handle)
{
    return FS_Read(buffer, size * count, handle);
}

void Sys_StreamSeek(int32_t handle, int32_t offset, int32_t origin)
{
    (void)FS_Seek(handle, offset, origin);
}
