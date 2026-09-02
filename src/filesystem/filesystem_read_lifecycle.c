#include "filesystem.h"
#include "filesystem_services.h"

/* Source: CoDUOMP.exe 0x0042c910..0x0042c915 and coduo_lnxded
 * 0x080606f8..0x08060701. The Mac symbol supplies the canonical name
 * FS_LoadStack; both bodies return the outstanding whole-file allocation
 * count unchanged. */
int32_t FS_LoadStack(void)
{
    return fs_loadStack;
}

/* Source: CoDUOMP.exe 0x0042e890..0x0042e89a and coduo_lnxded
 * 0x080631f8..0x08063206. */
void FS_ResetFiles(void)
{
    fs_loadStack = 0;
}

/* Source: CoDUOMP.exe 0x0042e8a0..0x0042e909 and coduo_lnxded
 * 0x08063207..0x0806323e. The dedicated engine's initialized-state assertion
 * remains at its target service boundary; the allocation ownership and stack
 * update are otherwise common. */
void FS_FreeFile(void *buffer)
{
    filesystem_compat_check_started();

    if (buffer == NULL)
        Com_Error(ERR_FATAL, "\x15"
                             "FS_FreeFile( NULL )");

    --fs_loadStack;
    Hunk_FreeTempMemory(buffer);
}
