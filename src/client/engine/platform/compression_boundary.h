#ifndef CODUOMP_COMPRESSION_BOUNDARY_H
#define CODUOMP_COMPRESSION_BOUNDARY_H

/*
 * Classic Minizip intentionally defines its archive handle as opaque storage.
 * Native and Windows-compatible builds link a pinned classic Minizip
 * implementation. Minizip metadata uses zlib's host-width uLong, so unsigned
 * long is intentional here:
 * it is 32-bit in the original Win32 ABI and follows the selected compression
 * ABI on modern LP64 targets.
 */
typedef void *unzFile;

/* NOT_FROM_ORIGINAL_SOURCE: these prefixed carriers mirror classic Minizip's
 * public ABI; they are not recovered engine-owned retail structures. */
typedef struct coduomp_unz_global_info_s {
    unsigned long entryCount;
    unsigned long commentSize; /* Minizip output unused by CoDUOMP.exe. */
} coduomp_unz_global_info_t;

typedef struct coduomp_unz_file_position_s {
    unsigned long directoryPosition;
    unsigned long fileIndex; /* Minizip output unused by CoDUOMP.exe. */
} coduomp_unz_file_position_t;

/* Retail CoDUOMP.exe consumes crc and uncompressedSize. The shared filesystem
 * security adapter also consumes filenameSize so a full output buffer cannot
 * be mistaken for a terminated archive-entry name; all remaining members are
 * required Minizip outputs but otherwise unused by the engine. */
typedef struct coduomp_unz_file_info_s {
    unsigned long version;
    unsigned long requiredVersion;
    unsigned long flags;
    unsigned long compressionMethod;
    unsigned long dosDate;
    unsigned long crc;
    unsigned long compressedSize;
    unsigned long uncompressedSize;
    unsigned long filenameSize;
    unsigned long extraDataSize;
    unsigned long commentSize;
    unsigned long startingDisk;
    unsigned long internalAttributes;
    unsigned long externalAttributes;
    unsigned int second;
    unsigned int minute;
    unsigned int hour;
    unsigned int monthDay;
    unsigned int month;
    unsigned int year;
} coduomp_unz_file_info_t;

#ifdef __cplusplus
extern "C" {
#endif

unzFile unzReOpen(const char *path, unzFile source);
unzFile unzOpen(const char *path);
int unzGetGlobalInfo(unzFile file, coduomp_unz_global_info_t *globalInfo);
int unzGoToFirstFile(unzFile file);
int unzGoToNextFile(unzFile file);
int unzGetFilePos(unzFile file, coduomp_unz_file_position_t *position);
int unzSetCurrentFileInfoPosition(unzFile file, unsigned long position);
int unzGetCurrentFileInfo(unzFile file, coduomp_unz_file_info_t *fileInfo, char *filename, unsigned long filenameBufferSize,
                          void *extraData, unsigned long extraDataBufferSize, char *comment, unsigned long commentBufferSize);
int unzOpenCurrentFile(unzFile file);
int unzReadCurrentFile(unzFile file, void *buffer, unsigned int length);
long unztell(unzFile file);
int unzCloseCurrentFile(unzFile file);
int unzClose(unzFile file);

#ifdef __cplusplus
}
#endif

#endif
