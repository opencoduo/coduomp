#include <string.h>

#include "../filesystem/fs_private.h"
#include "core_runtime_private.h"

#define COM_CDKEY_FILE "cdkey"
#define COM_CDKEY_DEFAULT "                "
#define COM_CDKEY_TEXT_BYTES 16
#define COM_CDKEY_HASH_BYTES 4

char com_cdkey[COM_CDKEY_TEXT_BYTES + 1] = "123456789";
char com_cdkeyHash[COM_CDKEY_HASH_BYTES + 1];

void Com_ResetCDKey(void)
{
    strcpy(com_cdkey, COM_CDKEY_DEFAULT);
}

void Com_ReadCDKey(void)
{
    void *fileBuffer = NULL;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    (void)FS_ReadFile(COM_CDKEY_FILE, NULL);
    (void)FS_ReadFile(COM_CDKEY_FILE, &fileBuffer);
    if (fileBuffer != NULL) {
        FS_FreeFile(fileBuffer);
    }
    Com_ResetCDKey();
}
