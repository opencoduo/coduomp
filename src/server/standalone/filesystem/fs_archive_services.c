#include "fs_private.h"
#include "filesystem_services.h"

/* NOT_FROM_ORIGINAL_SOURCE: target-local retained-unzip adaptation for the
 * shared original filesystem handle functions. */
int32_t filesystem_compat_archive_tell(const fileHandleData_t *fileHandle)
{
    return Unzip_TellCurrentFile((struct coduo_unz_s *)fileHandle->ioObject);
}

/* NOT_FROM_ORIGINAL_SOURCE: target-local retained-unzip adaptation for the
 * shared original filesystem handle functions. */
int32_t filesystem_compat_archive_length(const fileHandleData_t *fileHandle)
{
    const coduo_fs_zip_io_file_t *const zipFile = (const coduo_fs_zip_io_file_t *)fileHandle->ioObject;
    /* NOT_FROM_ORIGINAL_SOURCE: repeat the mounted-entry size policy at the
     * live reader boundary before publishing its length. */
    if (zipFile->cur_file_info.uncompressed_size > FS_MAX_PAK_ENTRY_BYTES) {
        return -1;
    }
    return (int32_t)zipFile->cur_file_info.uncompressed_size;
}

/* NOT_FROM_ORIGINAL_SOURCE: target-local retained-unzip adaptation for the
 * shared original filesystem handle functions. */
void filesystem_compat_archive_close_current(fileHandleData_t *fileHandle)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    (void)Unzip_CloseCurrentFile((struct coduo_unz_s *)fileHandle->ioObject);
}

/* NOT_FROM_ORIGINAL_SOURCE: target-local retained-unzip adaptation for the
 * shared original filesystem handle functions. */
void filesystem_compat_archive_close(fileHandleData_t *fileHandle)
{
    (void)Unzip_Close((struct coduo_unz_s *)fileHandle->ioObject);
}

int32_t filesystem_compat_archive_read(fileHandleData_t *fileHandle, void *buffer, uint32_t byteCount)
{
    return Unzip_ReadCurrentFile((struct coduo_unz_s *)fileHandle->ioObject, buffer, byteCount);
}

int32_t filesystem_compat_archive_rewind(fileHandleData_t *fileHandle)
{
    struct coduo_unz_s *const archive = (struct coduo_unz_s *)fileHandle->ioObject;
    const int32_t positionStatus = Unzip_GoToFilePosition(archive, fileHandle->zipRewindOffset);
    if (positionStatus != 0)
        return positionStatus;
    return Unzip_OpenCurrentFile(archive);
}

void filesystem_compat_pack_close(pack_t *pack)
{
    (void)Unzip_Close((struct coduo_unz_s *)pack->zipFile);
}

/* NOT_FROM_ORIGINAL_SOURCE: target-local retained-unzip realization of the
 * archive-entry edge used by the shared original read-open algorithm. */
qboolean filesystem_compat_archive_open_entry(pack_t *pack, const fileInPack_t *packFile, fileHandleData_t *fileHandle, qboolean uniqueFile,
                                              qboolean quiet)
{
    void *archive = pack->zipFile;

    if (uniqueFile != qfalse) {
        archive = Unzip_CloneArchiveForPack(pack, pack->zipFile);
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

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    int32_t status = Unzip_GoToFilePosition((struct coduo_unz_s *)archive, packFile->zipPosition);
    if (status == 0)
        status = Unzip_OpenCurrentFile((struct coduo_unz_s *)archive);

    /* NOT_FROM_ORIGINAL_SOURCE: publish the member handle only after successful
     * positioning and local-header opening; release an owned clone on failure. */
    if (status != 0) {
        if (archive != pack->zipFile)
            (void)Unzip_Close((struct coduo_unz_s *)archive);
        if (quiet == qfalse)
            Com_Printf("WARNING: could not open '%s' from pak '%s'\n", packFile->name, pack->pakFilename);
        return qfalse;
    }

    fileHandle->ioObject = archive;
    fileHandle->zipArchive = pack;
    fileHandle->zipRewindOffset = packFile->zipPosition;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: retained-unzip catalog adapters for the shared
 * FS_LoadZipFile algorithm. */
qboolean filesystem_compat_archive_open_catalog(const char *path, void **archiveOut, uint32_t *entryCountOut)
{
    coduo_zip_global_info_t globalInfo;
    struct coduo_unz_s *const archive = Unzip_Open(path);
    const int32_t status = Unzip_GetGlobalInfo(archive, &globalInfo);
    if (status != 0) {
        /* Preserve the retail Linux failure edge: it returns without closing
         * an archive which opened but failed the global-info read. */
        return qfalse;
    }

    *archiveOut = archive;
    *entryCountOut = globalInfo.number_entry;
    return qtrue;
}

void filesystem_compat_archive_close_catalog(void *archive)
{
    (void)Unzip_Close((struct coduo_unz_s *)archive);
}

int32_t filesystem_compat_archive_go_to_first(void *archive)
{
    return Unzip_GoToFirstFile((struct coduo_unz_s *)archive);
}

int32_t filesystem_compat_archive_go_to_next(void *archive)
{
    return Unzip_GoToNextFile((struct coduo_unz_s *)archive);
}

int32_t filesystem_compat_archive_get_current_info(void *archive, filesystem_compat_archive_file_info_t *fileInfo, char *filename,
                                                   uint32_t filenameSize)
{
    coduo_zip_file_info_t targetInfo = {0};
    const int32_t status = Unzip_GetCurrentFileInfo((struct coduo_unz_s *)archive, &targetInfo, filename, filenameSize, NULL, 0, NULL, 0);
    if (status == 0) {
        fileInfo->crc = targetInfo.crc;
        fileInfo->uncompressedSize = targetInfo.uncompressed_size;
        fileInfo->filenameLength = targetInfo.size_filename;
    }
    return status;
}

int32_t filesystem_compat_archive_get_current_position(void *archive)
{
    fileInPack_t position = {0};
    if (Unzip_GetCurrentFilePosition((struct coduo_unz_s *)archive, &position) != 0) {
        return -1;
    }
    return position.zipPosition;
}
