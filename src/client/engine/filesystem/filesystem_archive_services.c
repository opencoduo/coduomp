#include "filesystem_local.h"
#include "filesystem_services.h"

#include "../platform/compression_boundary.h"

/* NOT_FROM_ORIGINAL_SOURCE: target-local minizip adaptation for the shared
 * original filesystem handle functions. */
int32_t filesystem_compat_archive_tell(const fileHandleData_t *fileHandle)
{
    return (int32_t)unztell((unzFile)fileHandle->ioObject);
}

/* NOT_FROM_ORIGINAL_SOURCE: target-local minizip adaptation for the shared
 * original filesystem handle functions. */
int32_t filesystem_compat_archive_length(const fileHandleData_t *fileHandle)
{
    coduomp_unz_file_info_t fileInfo = {0};

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (unzGetCurrentFileInfo((unzFile)fileHandle->ioObject, &fileInfo, NULL, 0, NULL, 0, NULL, 0) != 0 ||
        fileInfo.uncompressedSize > FS_MAX_PAK_ENTRY_BYTES) {
        return -1;
    }
    return (int32_t)fileInfo.uncompressedSize;
}

/* NOT_FROM_ORIGINAL_SOURCE: target-local minizip adaptation for the shared
 * original filesystem handle functions. */
void filesystem_compat_archive_close_current(fileHandleData_t *fileHandle)
{
    (void)unzCloseCurrentFile((unzFile)fileHandle->ioObject);
}

/* NOT_FROM_ORIGINAL_SOURCE: target-local minizip adaptation for the shared
 * original filesystem handle functions. */
void filesystem_compat_archive_close(fileHandleData_t *fileHandle)
{
    (void)unzClose((unzFile)fileHandle->ioObject);
}

int32_t filesystem_compat_archive_read(fileHandleData_t *fileHandle, void *buffer, uint32_t byteCount)
{
    return unzReadCurrentFile((unzFile)fileHandle->ioObject, buffer, (unsigned int)byteCount);
}

int32_t filesystem_compat_archive_rewind(fileHandleData_t *fileHandle)
{
    const int32_t positionStatus =
        unzSetCurrentFileInfoPosition((unzFile)fileHandle->ioObject, (unsigned long)(uint32_t)fileHandle->zipRewindOffset);
    if (positionStatus != 0)
        return positionStatus;
    return unzOpenCurrentFile((unzFile)fileHandle->ioObject);
}

void filesystem_compat_pack_close(pack_t *pack)
{
    if (pack->zipFile != NULL)
        (void)unzClose((unzFile)pack->zipFile);
}

/* NOT_FROM_ORIGINAL_SOURCE: target-local classic-minizip realization of the
 * archive-entry edge used by the shared original read-open algorithm. */
qboolean filesystem_compat_archive_open_entry(pack_t *pack, const fileInPack_t *packFile, fileHandleData_t *fileHandle, qboolean uniqueFile,
                                              qboolean quiet)
{
    unzFile archive = (unzFile)pack->zipFile;

    if (uniqueFile != qfalse) {
        archive = unzReOpen(pack->pakFilename, (unzFile)pack->zipFile);
        if (archive == NULL) {
            if (quiet == qfalse) {
                Com_Error(ERR_FATAL,
                          "\x15"
                          "Couldn't reopen %s",
                          pack->pakFilename);
            }
            return qfalse;
        }
    }

    int32_t status = unzSetCurrentFileInfoPosition((unzFile)pack->zipFile, (unsigned long)(uint32_t)packFile->zipPosition);
    if (status == 0 && archive != (unzFile)pack->zipFile) {
        status = unzSetCurrentFileInfoPosition(archive, (unsigned long)(uint32_t)packFile->zipPosition);
    }
    if (status == 0)
        status = unzOpenCurrentFile(archive);

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (status != 0) {
        if (archive != (unzFile)pack->zipFile)
            (void)unzClose(archive);
        if (quiet == qfalse)
            Com_Printf("WARNING: could not open '%s' from pak '%s'\n", packFile->name, pack->pakFilename);
        return qfalse;
    }

    fileHandle->ioObject = archive;
    fileHandle->zipArchive = pack;
    fileHandle->zipRewindOffset = packFile->zipPosition;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: classic-minizip catalog adapters for the shared
 * FS_LoadZipFile algorithm. */
qboolean filesystem_compat_archive_open_catalog(const char *path, void **archiveOut, uint32_t *entryCountOut)
{
    unzFile const archive = unzOpen(path);
    if (archive == NULL)
        return qfalse;

    coduomp_unz_global_info_t globalInfo;
    if (unzGetGlobalInfo(archive, &globalInfo) != 0) {
        /* The original Windows body reads its pinned private archive record
         * directly. This failure edge belongs only to the public adapter. */
        (void)unzClose(archive);
        return qfalse;
    }
    if (globalInfo.entryCount > UINT32_MAX) {
        (void)unzClose(archive);
        return qfalse;
    }

    *archiveOut = archive;
    *entryCountOut = (uint32_t)globalInfo.entryCount;
    return qtrue;
}

void filesystem_compat_archive_close_catalog(void *archive)
{
    (void)unzClose((unzFile)archive);
}

int32_t filesystem_compat_archive_go_to_first(void *archive)
{
    return unzGoToFirstFile((unzFile)archive);
}

int32_t filesystem_compat_archive_go_to_next(void *archive)
{
    return unzGoToNextFile((unzFile)archive);
}

int32_t filesystem_compat_archive_get_current_info(void *archive, filesystem_compat_archive_file_info_t *fileInfo, char *filename,
                                                   uint32_t filenameSize)
{
    coduomp_unz_file_info_t targetInfo = {0};
    const int32_t status = unzGetCurrentFileInfo((unzFile)archive, &targetInfo, filename, (unsigned long)filenameSize, NULL, 0, NULL, 0);
    if (status == 0) {
        fileInfo->crc = (uint32_t)targetInfo.crc;
        fileInfo->uncompressedSize = (uint64_t)targetInfo.uncompressedSize;
        fileInfo->filenameLength = (uint32_t)targetInfo.filenameSize;
    }
    return status;
}

int32_t filesystem_compat_archive_get_current_position(void *archive)
{
    coduomp_unz_file_position_t position = {0};
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (unzGetFilePos((unzFile)archive, &position) != 0)
        return -1;
    if (position.directoryPosition > INT32_MAX)
        return -1;
    return (int32_t)(uint32_t)position.directoryPosition;
}
