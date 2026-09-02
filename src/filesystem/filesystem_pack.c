#include "filesystem.h"
#include "filesystem_path_security.h"
#include "filesystem_services.h"

#include "qcommon/q_checksum.h"
#include "qcommon/q_memory.h"
#include "qcommon/q_string.h"

#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

enum {
    FS_ARCHIVE_OK = 0,
    FS_PACK_HASH_MAXIMUM = 2048
};

void Com_Printf(const char *format, ...);

/*
 * Common pak catalog construction:
 *
 *   CoDUOMP.exe   0x0042e970..0x0042ede5
 *   coduo_lnxded  0x080632b9..0x08063873
 *
 * Both bodies publish the archive header count, perform the same two metadata
 * passes, build the same hash/list records, and checksum the same nonempty
 * entry CRCs. Target-local services expose only the distinct ZIP library.
 */
pack_t *FS_LoadZipFile(const char *zipFile, const char *basename)
{
    struct stat archiveStatus;
    qboolean archiveSizeUnsupported = qfalse;

    if (stat(zipFile, &archiveStatus) == 0) {
        archiveSizeUnsupported =
            !S_ISREG(archiveStatus.st_mode) || archiveStatus.st_size < 0 || (uintmax_t)archiveStatus.st_size > FS_MAX_PAK_ARCHIVE_BYTES
                ? qtrue
                : qfalse;
    } else {
        archiveSizeUnsupported = qtrue;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: require a measured regular archive extent
     * within both the configured package policy and signed position domain. */
    if (archiveSizeUnsupported != qfalse) {
        Com_Printf("WARNING: refusing pak '%s': archive size is unavailable or "
                   "exceeds FS_MAX_PAK_ARCHIVE_BYTES\n",
                   zipFile);
        return NULL;
    }

    void *archive;
    uint32_t entryCount;
    if (filesystem_compat_archive_open_catalog(zipFile, &archive, &entryCount) == qfalse) {
        return NULL;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate the central-directory count against
     * the configured catalog policy before accounting or allocation. */
    if (entryCount > FS_MAX_PAK_ENTRIES) {
        Com_Printf("WARNING: refusing pak '%s': %u entries exceed FS_MAX_PAK_ENTRIES\n", zipFile, entryCount);
        filesystem_compat_archive_close_catalog(archive);
        return NULL;
    }

    fs_packFiles += (int32_t)entryCount;

    size_t namesByteCount = 0;
    if (entryCount != 0 && filesystem_compat_archive_go_to_first(archive) != FS_ARCHIVE_OK) {
        /* NOT_FROM_ORIGINAL_SOURCE: a declared nonempty catalog must expose a
         * readable first row before its header count is accepted. */
        Com_Printf("WARNING: refusing pak '%s': could not select first entry\n", zipFile);
        fs_packFiles -= (int32_t)entryCount;
        filesystem_compat_archive_close_catalog(archive);
        return NULL;
    }
    for (uint32_t index = 0; index < entryCount; ++index) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        char filename[FS_PACK_NAME_SIZE];
        filesystem_compat_archive_file_info_t fileInfo;
        if (filesystem_compat_archive_get_current_info(archive, &fileInfo, filename, sizeof(filename)) != FS_ARCHIVE_OK) {
            /* NOT_FROM_ORIGINAL_SOURCE: every declared row must provide
             * complete metadata before the catalog count is published. */
            Com_Printf("WARNING: refusing pak '%s': could not read entry metadata\n", zipFile);
            fs_packFiles -= (int32_t)entryCount;
            filesystem_compat_archive_close_catalog(archive);
            return NULL;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: the separately reported filename length
         * must leave a NUL slot before C-string operations begin. */
        if (fileInfo.filenameLength >= sizeof(filename)) {
            Com_Printf("WARNING: refusing pak '%s' with an overlong entry name\n", zipFile);
            fs_packFiles -= (int32_t)entryCount;
            filesystem_compat_archive_close_catalog(archive);
            return NULL;
        }
        /* NOT_FROM_ORIGINAL_SOURCE: the raw archive-name field must encode one
         * complete C string with no embedded NUL. */
        if (strlen(filename) != (size_t)fileInfo.filenameLength) {
            Com_Printf("WARNING: refusing pak '%s' with an embedded-NUL entry name\n", zipFile);
            fs_packFiles -= (int32_t)entryCount;
            filesystem_compat_archive_close_catalog(archive);
            return NULL;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: validate each declared uncompressed size
         * against the configured entry policy before publication. */
        if (fileInfo.uncompressedSize > FS_MAX_PAK_ENTRY_BYTES) {
            Com_Printf("WARNING: refusing pak '%s': entry '%s' exceeds "
                       "FS_MAX_PAK_ENTRY_BYTES\n",
                       zipFile, filename);
            fs_packFiles -= (int32_t)entryCount;
            filesystem_compat_archive_close_catalog(archive);
            return NULL;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: every archive entry must remain relative
         * and below the virtual filesystem root. */
        if (coduo_compat_path_is_safe_relative(filename) == qfalse) {
            Com_Printf("WARNING: refusing pak '%s' with unsafe entry '%s'\n", zipFile, filename);
            fs_packFiles -= (int32_t)entryCount;
            filesystem_compat_archive_close_catalog(archive);
            return NULL;
        }

        namesByteCount += (size_t)fileInfo.filenameLength + 1u;
        if (index + 1u < entryCount && filesystem_compat_archive_go_to_next(archive) != FS_ARCHIVE_OK) {
            Com_Printf("WARNING: refusing pak '%s': could not select next entry\n", zipFile);
            fs_packFiles -= (int32_t)entryCount;
            filesystem_compat_archive_close_catalog(archive);
            return NULL;
        }
    }

    fileInPack_t *const packFiles = Z_MallocInternal((size_t)entryCount * sizeof(*packFiles) + namesByteCount);
    char *nameCursor = (char *)(packFiles + entryCount);
    uint32_t *const checksums = Z_MallocInternal((size_t)entryCount * sizeof(*checksums));

    int32_t hashSize = 1;
    while ((uint32_t)hashSize <= entryCount && hashSize < FS_PACK_HASH_MAXIMUM) {
        hashSize <<= 1;
    }

    pack_t *const pack = Z_MallocInternal(sizeof(*pack) + (size_t)hashSize * sizeof(*pack->hashTable));
    pack->hashSize = hashSize;
    pack->hashTable = (fileInPack_t **)(pack + 1);

    Q_strncpyz(pack->pakFilename, zipFile, FS_PACK_NAME_SIZE);
    Q_strncpyz(pack->pakBasename, basename, FS_PACK_NAME_SIZE);
    const size_t basenameLength = strlen(pack->pakBasename);
    if (basenameLength > 4 && Q_stricmp(pack->pakBasename + basenameLength - 4, ".pk3") == 0) {
        pack->pakBasename[basenameLength - 4] = '\0';
    }

    pack->zipFile = archive;
    pack->numFiles = (int32_t)entryCount;

    uint32_t checksumCount = 0;
    size_t remainingNameBytes = namesByteCount;
    if (entryCount != 0 && filesystem_compat_archive_go_to_first(archive) != FS_ARCHIVE_OK) {
        Com_Printf("WARNING: refusing pak '%s': could not reselect first entry\n", zipFile);
        fs_packFiles -= (int32_t)entryCount;
        filesystem_compat_archive_close_catalog(archive);
        Z_FreeInternal(pack);
        Z_FreeInternal(checksums);
        Z_FreeInternal(packFiles);
        return NULL;
    }
    for (uint32_t index = 0; index < entryCount; ++index) {
        char filename[FS_PACK_NAME_SIZE];
        filesystem_compat_archive_file_info_t fileInfo;
        if (filesystem_compat_archive_get_current_info(archive, &fileInfo, filename, sizeof(filename)) != FS_ARCHIVE_OK) {
            /* NOT_FROM_ORIGINAL_SOURCE: every publication-pass row must expose
             * complete metadata or the partial catalog is released. */
            Com_Printf("WARNING: refusing pak '%s': could not reread entry metadata\n", zipFile);
            fs_packFiles -= (int32_t)entryCount;
            filesystem_compat_archive_close_catalog(archive);
            Z_FreeInternal(pack);
            Z_FreeInternal(checksums);
            Z_FreeInternal(packFiles);
            return NULL;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: repeat the filename-length proof at the
         * publication pass before lowercase, hash, or copy operations. */
        if (fileInfo.filenameLength >= sizeof(filename)) {
            Com_Printf("WARNING: refusing pak '%s' with an overlong entry name\n", zipFile);
            fs_packFiles -= (int32_t)entryCount;
            filesystem_compat_archive_close_catalog(archive);
            Z_FreeInternal(pack);
            Z_FreeInternal(checksums);
            Z_FreeInternal(packFiles);
            return NULL;
        }
        if (strlen(filename) != (size_t)fileInfo.filenameLength) {
            Com_Printf("WARNING: refusing pak '%s' with an embedded-NUL entry name\n", zipFile);
            fs_packFiles -= (int32_t)entryCount;
            filesystem_compat_archive_close_catalog(archive);
            Z_FreeInternal(pack);
            Z_FreeInternal(checksums);
            Z_FreeInternal(packFiles);
            return NULL;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: repeat the configured entry-size proof at
         * the publication pass. */
        if (fileInfo.uncompressedSize > FS_MAX_PAK_ENTRY_BYTES) {
            Com_Printf("WARNING: refusing pak '%s': entry '%s' exceeds "
                       "FS_MAX_PAK_ENTRY_BYTES\n",
                       zipFile, filename);
            fs_packFiles -= (int32_t)entryCount;
            filesystem_compat_archive_close_catalog(archive);
            Z_FreeInternal(pack);
            Z_FreeInternal(checksums);
            Z_FreeInternal(packFiles);
            return NULL;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: repeat the relative-path proof for the
         * filename actually observed and published by the second pass. */
        if (coduo_compat_path_is_safe_relative(filename) == qfalse) {
            Com_Printf("WARNING: refusing pak '%s' with unsafe entry '%s'\n", zipFile, filename);
            fs_packFiles -= (int32_t)entryCount;
            filesystem_compat_archive_close_catalog(archive);
            Z_FreeInternal(pack);
            Z_FreeInternal(checksums);
            Z_FreeInternal(packFiles);
            return NULL;
        }

        const size_t publishedNameBytes = (size_t)fileInfo.filenameLength + 1u;
        /* NOT_FROM_ORIGINAL_SOURCE: bound publication by the trailing name
         * pool measured during the first metadata pass. */
        if (publishedNameBytes > remainingNameBytes) {
            Com_Printf("WARNING: refusing pak '%s': entry names changed while loading\n", zipFile);
            fs_packFiles -= (int32_t)entryCount;
            filesystem_compat_archive_close_catalog(archive);
            Z_FreeInternal(pack);
            Z_FreeInternal(checksums);
            Z_FreeInternal(packFiles);
            return NULL;
        }
        remainingNameBytes -= publishedNameBytes;

        if (fileInfo.uncompressedSize != 0)
            checksums[checksumCount++] = fileInfo.crc;

        Q_strlwr(filename);
        const uint32_t hash = FS_HashFileName(filename, pack->hashSize);
        fileInPack_t *const packFile = &packFiles[index];

        packFile->name = nameCursor;
        strcpy(packFile->name, filename);

        int32_t basenameIndex = (int32_t)strlen(nameCursor) - 1;
        while (basenameIndex >= 0 && nameCursor[basenameIndex] != '/' && nameCursor[basenameIndex] != '\\') {
            --basenameIndex;
        }
        packFile->data.name = nameCursor + basenameIndex + 1;
        nameCursor += publishedNameBytes;

        const int32_t zipPosition = filesystem_compat_archive_get_current_position(archive);
        /* NOT_FROM_ORIGINAL_SOURCE: publish a catalog row only with a valid
         * central-directory position for later member opening. */
        if (zipPosition < 0) {
            Com_Printf("WARNING: refusing pak '%s': could not save entry position\n", zipFile);
            fs_packFiles -= (int32_t)entryCount;
            filesystem_compat_archive_close_catalog(archive);
            Z_FreeInternal(pack);
            Z_FreeInternal(checksums);
            Z_FreeInternal(packFiles);
            return NULL;
        }
        packFile->zipPosition = zipPosition;
        packFile->next = pack->hashTable[hash];
        pack->hashTable[hash] = packFile;
        if (index + 1u < entryCount && filesystem_compat_archive_go_to_next(archive) != FS_ARCHIVE_OK) {
            Com_Printf("WARNING: refusing pak '%s': could not reselect next entry\n", zipFile);
            fs_packFiles -= (int32_t)entryCount;
            filesystem_compat_archive_close_catalog(archive);
            Z_FreeInternal(pack);
            Z_FreeInternal(checksums);
            Z_FreeInternal(packFiles);
            return NULL;
        }
    }

    const int32_t checksumBytes = (int32_t)(checksumCount * sizeof(*checksums));
    pack->checksum = (int32_t)Com_BlockChecksum(checksums, checksumBytes);
    pack->pureChecksum = (int32_t)Com_BlockChecksumKey(checksums, checksumBytes, fs_checksumFeed);
    Z_FreeInternal(checksums);
    pack->fileList = packFiles;
    return pack;
}
