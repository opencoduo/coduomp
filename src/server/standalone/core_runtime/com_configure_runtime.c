#include <stdint.h>

#include "core_runtime_private.h"
#include "../filesystem/fs_private.h"
#include "../scripting/script_runtime_private.h"

#define COM_CONFIGURE_FILE_PATH "configure_mp.csv"
#define COM_CONFIGURE_FILE_NOT_FOUND_ERROR \
    "EXE_ERR_NOT_FOUND" "\x15" COM_CONFIGURE_FILE_PATH
#define COM_CONFIGURE_HASH_MULTIPLIER 31337
#define COM_CONFIGURE_HASH_MASK 0x0fffffff
#define COM_CONFIGURE_HASH_OFFSET 1

uint32_t
Com_HashConfigureFileBuffer(const char *data,
                            int32_t length)
{
    uint32_t hash = 0;

    for (int32_t index = 0; index < length; ++index) {
        hash = hash * COM_CONFIGURE_HASH_MULTIPLIER +
               (uint32_t)data[index];
    }

    return (hash & COM_CONFIGURE_HASH_MASK) + COM_CONFIGURE_HASH_OFFSET;
}

int32_t Com_ApplyConfigureFileChecksum(void)
{
    void *fileBuffer;
    int32_t fileLength;
    uint32_t checksum;

    fileBuffer = NULL;
    fileLength = FS_ReadFile(COM_CONFIGURE_FILE_PATH, &fileBuffer);
    if (fileLength < 0) {
        Com_Error(0, COM_CONFIGURE_FILE_NOT_FOUND_ERROR);
    }

    checksum = Com_HashConfigureFileBuffer(fileBuffer, fileLength);
    FS_FreeFile(fileBuffer);
    return Sys_ConfigureChecksumChanged((int32_t)checksum);
}

/*
 * Mac MP symbols name the matching recommended-config hook Com_SetRecommended;
 * the Linux dedicated body is an empty platform build variant.
 */
void Com_SetRecommended(void)
{
}

void Com_InitScriptRuntime(void)
{
    int32_t debugReport;

    debugReport = qfalse;
    if (com_developer->integer != 0 || com_logfile->integer != 0) {
        debugReport = qtrue;
    }

    Scr_Init(debugReport,
                       host_cvar_developer_script_pointer_slot->integer,
                       com_developer->integer);
}
