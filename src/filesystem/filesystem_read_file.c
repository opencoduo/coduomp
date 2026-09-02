#include "filesystem.h"
#include "filesystem_services.h"

#include <string.h>

extern cvar_t *com_journal;
extern int32_t com_journalDataFile;

/* Source: CoDUOMP.exe 0x0042e600..0x0042e88f and coduo_lnxded
 * 0x08062eb0..0x080631f7. Config journaling, temporary-memory ownership,
 * allocation-stack updates, reads, terminators, and returns agree. The
 * dedicated initialized-state assertion remains at its target service
 * boundary. */
int32_t FS_ReadFile(const char *qpath, void **buffer)
{
    filesystem_compat_check_started();

    if (qpath == NULL || qpath[0] == '\0')
        Com_Error(ERR_FATAL, "\x15"
                             "FS_ReadFile with empty name\n");

    const qboolean isConfig = strstr(qpath, ".cfg") != NULL ? qtrue : qfalse;
    if (isConfig != qfalse && com_journal != NULL && com_journal->integer == 2) {
        int32_t length;
        Com_DPrintf("Loading %s from journal file.\n", qpath);
        if (FS_Read(&length, (int32_t)sizeof(length), com_journalDataFile) != (int32_t)sizeof(length)) {
            if (buffer != NULL)
                *buffer = NULL;
            return -1;
        }

        if (length == 0) {
            if (buffer == NULL)
                return 1;
            *buffer = NULL;
            return -1;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: a serialized file length must belong to
         * the nonnegative signed filesystem domain before use. */
        if (length < 0) {
            Com_Printf("FS_ReadFile: refusing negative journal length %i for %s\n", length, qpath);
            if (buffer != NULL)
                *buffer = NULL;
            return -1;
        }
        if (buffer == NULL)
            return length;

        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        char *const contents = Hunk_AllocateTempMemoryInternal((size_t)((uint32_t)length + 1u));
        *buffer = contents;
        if (FS_Read(contents, length, com_journalDataFile) != length)
            Com_Error(ERR_FATAL, "EXE_ERR_JOURNAL_FILE_READ");
        ++fs_loadStack;
        contents[length] = '\0';
        return length;
    }

    int32_t handle;
    const int32_t length = FS_FOpenFileRead(qpath, &handle, qfalse);
    if (handle == 0) {
        if (buffer != NULL)
            *buffer = NULL;

        if (isConfig != qfalse && com_journal != NULL && com_journal->integer == 1) {
            int32_t journalLength = 0;
            Com_DPrintf("Writing zero for %s to journal file.\n", qpath);
            (void)FS_Write(&journalLength, (int32_t)sizeof(journalLength), com_journalDataFile);
            FS_Flush(com_journalDataFile);
        }
        return -1;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: a whole-file length must belong to the
     * nonnegative signed filesystem domain; rejection closes the handle and
     * preserves journal alignment. */
    if (length < 0) {
        Com_Printf("FS_ReadFile: refusing negative length %i for %s\n", length, qpath);
        FS_FCloseFile(handle);
        if (buffer != NULL)
            *buffer = NULL;

        if (isConfig != qfalse && com_journal != NULL && com_journal->integer == 1) {
            int32_t journalLength = 0;
            Com_DPrintf("Writing zero for %s to journal file.\n", qpath);
            (void)FS_Write(&journalLength, (int32_t)sizeof(journalLength), com_journalDataFile);
            FS_Flush(com_journalDataFile);
        }
        return -1;
    }

    if (buffer == NULL) {
        if (isConfig != qfalse && com_journal != NULL && com_journal->integer == 1) {
            Com_DPrintf("Writing len for %s to journal file.\n", qpath);
            (void)FS_Write(&length, (int32_t)sizeof(length), com_journalDataFile);
            FS_Flush(com_journalDataFile);
        }
        FS_FCloseFile(handle);
        return length;
    }

    ++fs_loadStack;
    /* The validated signed length has a representable payload-plus-NUL
     * allocation in the maintained host-width allocator. */
    char *const contents = Hunk_AllocateTempMemoryInternal((size_t)((uint32_t)length + 1u));
    *buffer = contents;
    const int32_t bytesRead = FS_Read(contents, length, handle);
    FS_FCloseFile(handle);

    /* NOT_FROM_ORIGINAL_SOURCE: publish the owned whole-file buffer only after
     * the complete requested payload has been read. */
    if (bytesRead != length) {
        Com_Printf("FS_ReadFile: short read for %s (%i of %i bytes)\n", qpath, bytesRead, length);
        FS_FreeFile(contents);
        *buffer = NULL;
        if (isConfig != qfalse && com_journal != NULL && com_journal->integer == 1) {
            int32_t journalLength = 0;
            (void)FS_Write(&journalLength, (int32_t)sizeof(journalLength), com_journalDataFile);
            FS_Flush(com_journalDataFile);
        }
        return -1;
    }
    contents[length] = '\0';

    if (isConfig != qfalse && com_journal != NULL && com_journal->integer == 1) {
        Com_DPrintf("Writing %s to journal file.\n", qpath);
        (void)FS_Write(&length, (int32_t)sizeof(length), com_journalDataFile);
        (void)FS_Write(contents, length, com_journalDataFile);
        FS_Flush(com_journalDataFile);
    }

    return length;
}
