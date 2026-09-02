#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs_private.h"

enum {
    UNZ_OK = 0,
    UNZ_ERRNO = -1,
    UNZ_END_OF_LIST_OF_FILE = -100,
    UNZ_PARAMERROR = -102,
    UNZ_BADZIPFILE = -103,
    UNZ_INTERNALERROR = -104,
    UNZ_CRCERROR = -105,
    UNZ_STORED_METHOD = 0,
    UNZ_DEFLATED_METHOD = 8,
    UNZ_Z_SYNC_FLUSH = 2,
    UNZ_Z_STREAM_END = 1,
    UNZ_RAW_DEFLATE_WINDOW_BITS = -15,
    UNZ_FLAG_DATA_DESCRIPTOR = 8,
    UNZ_CASE_SENSITIVE = 1,
    UNZ_EOF_REACHED = 1,
    UNZ_FREAD_ELEMENT_COUNT = 1,
    UNZ_EOCD_DISK_NUMBER = 0,
    UNZ_EOCD_CENTRAL_DISK_NUMBER = 0,
    UNZ_EOCD_SIGNATURE = 0x06054b50,
    UNZ_CENTRAL_DIRECTORY_HEADER_SIGNATURE = 0x02014b50,
    UNZ_LOCAL_FILE_HEADER_SIGNATURE = 0x04034b50,
    UNZ_LOCAL_FILE_HEADER_FIXED_SIZE = 30,
    UNZ_EOCD_FIXED_SIZE = 22,
    UNZ_DOS_TIME_DAY_SHIFT = 16,
    UNZ_DOS_TIME_DAY_MASK = 0x1f,
    UNZ_DOS_TIME_MONTH_SHIFT = 21,
    UNZ_DOS_TIME_MONTH_MASK = 0x0f,
    UNZ_DOS_TIME_YEAR_SHIFT = 25,
    UNZ_DOS_TIME_YEAR_BASE = 1980,
    UNZ_DOS_TIME_HOUR_SHIFT = 11,
    UNZ_DOS_TIME_HOUR_MASK = 0x1f,
    UNZ_DOS_TIME_MINUTE_SHIFT = 5,
    UNZ_DOS_TIME_MINUTE_MASK = 0x3f,
    UNZ_DOS_TIME_SECOND_MASK = 0x1f,
    UNZ_DOS_TIME_SECOND_SCALE = 2,
    UNZ_CRC_TABLE_SIZE = 256,
    UNZ_CRC_UNROLL_COUNT = 8,
    UNZ_READ_BUFFER_SIZE = 65536,
    UNZ_FILE_NAME_SEARCH_LIMIT = 256,
    UNZ_CENTRAL_DIRECTORY_FIXED_SIZE = 46,
    UNZ_STOCK_Z_STREAM_SIZE = 56
};

static const char unz_zlibVersion[] = "1.1.3";

typedef struct coduo_unz_read_info_s {
    uint8_t *read_buffer;
    z_stream stream;
    uint32_t file_offset_after_local_extra;
    qboolean stream_initialised;
    uint32_t offset_local_extrafield;
    uint32_t size_local_extrafield;
    uint32_t pos_local_extrafield;
    uint32_t crc32;
    uint32_t crc32_wait;
    uint32_t rest_read_compressed;
    uint32_t rest_read_uncompressed;
    FILE *file;
    uint32_t compression_method;
    uint32_t byte_before_the_zipfile;
} coduo_unz_read_info_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(coduo_unz_read_info_t) == 0x6c, "coduo_unz_read_info_t size mismatch");
_Static_assert(offsetof(coduo_unz_read_info_t, read_buffer) == 0x00, "coduo_unz_read_info_t.read_buffer offset mismatch");
_Static_assert(offsetof(coduo_unz_read_info_t, stream) == 0x04, "coduo_unz_read_info_t.stream offset mismatch");
_Static_assert(offsetof(coduo_unz_read_info_t, stream_initialised) == 0x40, "coduo_unz_read_info_t.stream_initialised offset mismatch");
_Static_assert(offsetof(coduo_unz_read_info_t, offset_local_extrafield) == 0x44,
               "coduo_unz_read_info_t.offset_local_extrafield offset mismatch");
_Static_assert(offsetof(coduo_unz_read_info_t, size_local_extrafield) == 0x48,
               "coduo_unz_read_info_t.size_local_extrafield offset mismatch");
_Static_assert(offsetof(coduo_unz_read_info_t, pos_local_extrafield) == 0x4c, "coduo_unz_read_info_t.pos_local_extrafield offset mismatch");
_Static_assert(offsetof(coduo_unz_read_info_t, crc32) == 0x50, "coduo_unz_read_info_t.crc32 offset mismatch");
_Static_assert(offsetof(coduo_unz_read_info_t, crc32_wait) == 0x54, "coduo_unz_read_info_t.crc32_wait offset mismatch");
_Static_assert(offsetof(coduo_unz_read_info_t, rest_read_compressed) == 0x58, "coduo_unz_read_info_t.rest_read_compressed offset mismatch");
_Static_assert(offsetof(coduo_unz_read_info_t, rest_read_uncompressed) == 0x5c,
               "coduo_unz_read_info_t.rest_read_uncompressed offset mismatch");
_Static_assert(offsetof(coduo_unz_read_info_t, file) == 0x60, "coduo_unz_read_info_t.file offset mismatch");
_Static_assert(offsetof(coduo_unz_read_info_t, compression_method) == 0x64, "coduo_unz_read_info_t.compression_method offset mismatch");
_Static_assert(offsetof(coduo_unz_read_info_t, byte_before_the_zipfile) == 0x68,
               "coduo_unz_read_info_t.byte_before_the_zipfile offset mismatch");
#endif

static const uint32_t unz_crcTable[UNZ_CRC_TABLE_SIZE] = {
    UINT32_C(0x00000000), UINT32_C(0x77073096), UINT32_C(0xee0e612c), UINT32_C(0x990951ba), UINT32_C(0x076dc419), UINT32_C(0x706af48f),
    UINT32_C(0xe963a535), UINT32_C(0x9e6495a3), UINT32_C(0x0edb8832), UINT32_C(0x79dcb8a4), UINT32_C(0xe0d5e91e), UINT32_C(0x97d2d988),
    UINT32_C(0x09b64c2b), UINT32_C(0x7eb17cbd), UINT32_C(0xe7b82d07), UINT32_C(0x90bf1d91), UINT32_C(0x1db71064), UINT32_C(0x6ab020f2),
    UINT32_C(0xf3b97148), UINT32_C(0x84be41de), UINT32_C(0x1adad47d), UINT32_C(0x6ddde4eb), UINT32_C(0xf4d4b551), UINT32_C(0x83d385c7),
    UINT32_C(0x136c9856), UINT32_C(0x646ba8c0), UINT32_C(0xfd62f97a), UINT32_C(0x8a65c9ec), UINT32_C(0x14015c4f), UINT32_C(0x63066cd9),
    UINT32_C(0xfa0f3d63), UINT32_C(0x8d080df5), UINT32_C(0x3b6e20c8), UINT32_C(0x4c69105e), UINT32_C(0xd56041e4), UINT32_C(0xa2677172),
    UINT32_C(0x3c03e4d1), UINT32_C(0x4b04d447), UINT32_C(0xd20d85fd), UINT32_C(0xa50ab56b), UINT32_C(0x35b5a8fa), UINT32_C(0x42b2986c),
    UINT32_C(0xdbbbc9d6), UINT32_C(0xacbcf940), UINT32_C(0x32d86ce3), UINT32_C(0x45df5c75), UINT32_C(0xdcd60dcf), UINT32_C(0xabd13d59),
    UINT32_C(0x26d930ac), UINT32_C(0x51de003a), UINT32_C(0xc8d75180), UINT32_C(0xbfd06116), UINT32_C(0x21b4f4b5), UINT32_C(0x56b3c423),
    UINT32_C(0xcfba9599), UINT32_C(0xb8bda50f), UINT32_C(0x2802b89e), UINT32_C(0x5f058808), UINT32_C(0xc60cd9b2), UINT32_C(0xb10be924),
    UINT32_C(0x2f6f7c87), UINT32_C(0x58684c11), UINT32_C(0xc1611dab), UINT32_C(0xb6662d3d), UINT32_C(0x76dc4190), UINT32_C(0x01db7106),
    UINT32_C(0x98d220bc), UINT32_C(0xefd5102a), UINT32_C(0x71b18589), UINT32_C(0x06b6b51f), UINT32_C(0x9fbfe4a5), UINT32_C(0xe8b8d433),
    UINT32_C(0x7807c9a2), UINT32_C(0x0f00f934), UINT32_C(0x9609a88e), UINT32_C(0xe10e9818), UINT32_C(0x7f6a0dbb), UINT32_C(0x086d3d2d),
    UINT32_C(0x91646c97), UINT32_C(0xe6635c01), UINT32_C(0x6b6b51f4), UINT32_C(0x1c6c6162), UINT32_C(0x856530d8), UINT32_C(0xf262004e),
    UINT32_C(0x6c0695ed), UINT32_C(0x1b01a57b), UINT32_C(0x8208f4c1), UINT32_C(0xf50fc457), UINT32_C(0x65b0d9c6), UINT32_C(0x12b7e950),
    UINT32_C(0x8bbeb8ea), UINT32_C(0xfcb9887c), UINT32_C(0x62dd1ddf), UINT32_C(0x15da2d49), UINT32_C(0x8cd37cf3), UINT32_C(0xfbd44c65),
    UINT32_C(0x4db26158), UINT32_C(0x3ab551ce), UINT32_C(0xa3bc0074), UINT32_C(0xd4bb30e2), UINT32_C(0x4adfa541), UINT32_C(0x3dd895d7),
    UINT32_C(0xa4d1c46d), UINT32_C(0xd3d6f4fb), UINT32_C(0x4369e96a), UINT32_C(0x346ed9fc), UINT32_C(0xad678846), UINT32_C(0xda60b8d0),
    UINT32_C(0x44042d73), UINT32_C(0x33031de5), UINT32_C(0xaa0a4c5f), UINT32_C(0xdd0d7cc9), UINT32_C(0x5005713c), UINT32_C(0x270241aa),
    UINT32_C(0xbe0b1010), UINT32_C(0xc90c2086), UINT32_C(0x5768b525), UINT32_C(0x206f85b3), UINT32_C(0xb966d409), UINT32_C(0xce61e49f),
    UINT32_C(0x5edef90e), UINT32_C(0x29d9c998), UINT32_C(0xb0d09822), UINT32_C(0xc7d7a8b4), UINT32_C(0x59b33d17), UINT32_C(0x2eb40d81),
    UINT32_C(0xb7bd5c3b), UINT32_C(0xc0ba6cad), UINT32_C(0xedb88320), UINT32_C(0x9abfb3b6), UINT32_C(0x03b6e20c), UINT32_C(0x74b1d29a),
    UINT32_C(0xead54739), UINT32_C(0x9dd277af), UINT32_C(0x04db2615), UINT32_C(0x73dc1683), UINT32_C(0xe3630b12), UINT32_C(0x94643b84),
    UINT32_C(0x0d6d6a3e), UINT32_C(0x7a6a5aa8), UINT32_C(0xe40ecf0b), UINT32_C(0x9309ff9d), UINT32_C(0x0a00ae27), UINT32_C(0x7d079eb1),
    UINT32_C(0xf00f9344), UINT32_C(0x8708a3d2), UINT32_C(0x1e01f268), UINT32_C(0x6906c2fe), UINT32_C(0xf762575d), UINT32_C(0x806567cb),
    UINT32_C(0x196c3671), UINT32_C(0x6e6b06e7), UINT32_C(0xfed41b76), UINT32_C(0x89d32be0), UINT32_C(0x10da7a5a), UINT32_C(0x67dd4acc),
    UINT32_C(0xf9b9df6f), UINT32_C(0x8ebeeff9), UINT32_C(0x17b7be43), UINT32_C(0x60b08ed5), UINT32_C(0xd6d6a3e8), UINT32_C(0xa1d1937e),
    UINT32_C(0x38d8c2c4), UINT32_C(0x4fdff252), UINT32_C(0xd1bb67f1), UINT32_C(0xa6bc5767), UINT32_C(0x3fb506dd), UINT32_C(0x48b2364b),
    UINT32_C(0xd80d2bda), UINT32_C(0xaf0a1b4c), UINT32_C(0x36034af6), UINT32_C(0x41047a60), UINT32_C(0xdf60efc3), UINT32_C(0xa867df55),
    UINT32_C(0x316e8eef), UINT32_C(0x4669be79), UINT32_C(0xcb61b38c), UINT32_C(0xbc66831a), UINT32_C(0x256fd2a0), UINT32_C(0x5268e236),
    UINT32_C(0xcc0c7795), UINT32_C(0xbb0b4703), UINT32_C(0x220216b9), UINT32_C(0x5505262f), UINT32_C(0xc5ba3bbe), UINT32_C(0xb2bd0b28),
    UINT32_C(0x2bb45a92), UINT32_C(0x5cb36a04), UINT32_C(0xc2d7ffa7), UINT32_C(0xb5d0cf31), UINT32_C(0x2cd99e8b), UINT32_C(0x5bdeae1d),
    UINT32_C(0x9b64c2b0), UINT32_C(0xec63f226), UINT32_C(0x756aa39c), UINT32_C(0x026d930a), UINT32_C(0x9c0906a9), UINT32_C(0xeb0e363f),
    UINT32_C(0x72076785), UINT32_C(0x05005713), UINT32_C(0x95bf4a82), UINT32_C(0xe2b87a14), UINT32_C(0x7bb12bae), UINT32_C(0x0cb61b38),
    UINT32_C(0x92d28e9b), UINT32_C(0xe5d5be0d), UINT32_C(0x7cdcefb7), UINT32_C(0x0bdbdf21), UINT32_C(0x86d3d2d4), UINT32_C(0xf1d4e242),
    UINT32_C(0x68ddb3f8), UINT32_C(0x1fda836e), UINT32_C(0x81be16cd), UINT32_C(0xf6b9265b), UINT32_C(0x6fb077e1), UINT32_C(0x18b74777),
    UINT32_C(0x88085ae6), UINT32_C(0xff0f6a70), UINT32_C(0x66063bca), UINT32_C(0x11010b5c), UINT32_C(0x8f659eff), UINT32_C(0xf862ae69),
    UINT32_C(0x616bffd3), UINT32_C(0x166ccf45), UINT32_C(0xa00ae278), UINT32_C(0xd70dd2ee), UINT32_C(0x4e048354), UINT32_C(0x3903b3c2),
    UINT32_C(0xa7672661), UINT32_C(0xd06016f7), UINT32_C(0x4969474d), UINT32_C(0x3e6e77db), UINT32_C(0xaed16a4a), UINT32_C(0xd9d65adc),
    UINT32_C(0x40df0b66), UINT32_C(0x37d83bf0), UINT32_C(0xa9bcae53), UINT32_C(0xdebb9ec5), UINT32_C(0x47b2cf7f), UINT32_C(0x30b5ffe9),
    UINT32_C(0xbdbdf21c), UINT32_C(0xcabac28a), UINT32_C(0x53b39330), UINT32_C(0x24b4a3a6), UINT32_C(0xbad03605), UINT32_C(0xcdd70693),
    UINT32_C(0x54de5729), UINT32_C(0x23d967bf), UINT32_C(0xb3667a2e), UINT32_C(0xc4614ab8), UINT32_C(0x5d681b02), UINT32_C(0x2a6f2b94),
    UINT32_C(0xb40bbe37), UINT32_C(0xc30c8ea1), UINT32_C(0x5a05df1b), UINT32_C(0x2d02ef8d)};

const uint32_t *Unzip_Crc32Table(void)
{
    return unz_crcTable;
}

uint32_t Unzip_UpdateCrc32(uint32_t crc, const uint8_t *buffer, uint32_t length)
{
    const uint32_t *crcTable = unz_crcTable;

    if (buffer == NULL) {
        return 0;
    }

    crc = ~crc;
    while (length > UNZ_CRC_UNROLL_COUNT - 1) {
        uint32_t value = (crc >> 8) ^ crcTable[(buffer[0] ^ crc) & 0xffU];
        value = (value >> 8) ^ crcTable[(buffer[1] ^ value) & 0xffU];
        value = (value >> 8) ^ crcTable[(buffer[2] ^ value) & 0xffU];
        value = (value >> 8) ^ crcTable[(buffer[3] ^ value) & 0xffU];
        value = (value >> 8) ^ crcTable[(buffer[4] ^ value) & 0xffU];
        value = (value >> 8) ^ crcTable[(buffer[5] ^ value) & 0xffU];
        value = (value >> 8) ^ crcTable[(buffer[6] ^ value) & 0xffU];
        crc = (value >> 8) ^ crcTable[(buffer[7] ^ value) & 0xffU];
        buffer = &buffer[UNZ_CRC_UNROLL_COUNT];
        length -= UNZ_CRC_UNROLL_COUNT;
    }

    while (length != 0) {
        crc = (crc >> 8) ^ crcTable[(*buffer ^ crc) & 0xffU];
        buffer = &buffer[1];
        length--;
    }

    return ~crc;
}

struct coduo_unz_s *Unzip_CloneArchiveForPack(pack_t *pack, const struct coduo_unz_s *source)
{
    FILE *file = fopen(pack->pakFilename, "rb");

    if (file == NULL) {
        return NULL;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    struct coduo_unz_s *clone = malloc(sizeof(*clone));
    if (clone == NULL) {
        fclose(file);
        return NULL;
    }
    memcpy(clone, source, sizeof(*clone));
    clone->file = file;
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    clone->pfile_in_zip_read = NULL;

    return clone;
}

struct coduo_unz_s *Unzip_Open(const char *path)
{
    int32_t status = UNZ_OK;
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return NULL;
    }

    uint32_t centralPos = (uint32_t)Sys_FindZipEndOfCentralDirectory(file);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (centralPos == 0) {
        status = UNZ_ERRNO;
    }

    uint32_t eocdSignature = 0;
    uint32_t diskNumber = 0;
    uint32_t centralDirectoryDisk = 0;
    uint32_t entriesOnDisk = 0;
    uint32_t entriesTotal = 0;
    uint32_t centralDirectorySize = 0;
    uint32_t centralDirectoryOffset = 0;
    uint32_t commentSize = 0;

    /*
     * 0x080cb9c9..0x080cbb5f performs every seek/read in order and only
     * accumulates error status; stock does not short-circuit later reads.
     */
    if (fseek(file, (long)(int32_t)centralPos, SEEK_SET) != 0) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleLong(file, &eocdSignature) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleShort(file, &diskNumber) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleShort(file, &centralDirectoryDisk) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleShort(file, &entriesOnDisk) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleShort(file, &entriesTotal) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleLong(file, &centralDirectorySize) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleLong(file, &centralDirectoryOffset) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleShort(file, &commentSize) != UNZ_OK) {
        status = UNZ_ERRNO;
    }

    if (entriesTotal != entriesOnDisk || centralDirectoryDisk != UNZ_EOCD_CENTRAL_DISK_NUMBER || diskNumber != UNZ_EOCD_DISK_NUMBER ||
        eocdSignature != UNZ_EOCD_SIGNATURE) {
        status = UNZ_BADZIPFILE;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: compute the central-directory end in a wider
     * lane and narrow it only after validating the archive bound. */
    const uint64_t centralDirectoryEnd = (uint64_t)centralDirectoryOffset + (uint64_t)centralDirectorySize;
    if (status == UNZ_OK && centralDirectoryEnd > centralPos) {
        status = UNZ_BADZIPFILE;
    }

    if (status != UNZ_OK) {
        fclose(file);
        return NULL;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    struct coduo_unz_s archiveState = {0};
    archiveState.file = file;
    archiveState.number_entry = entriesTotal;
    archiveState.size_comment = commentSize;
    archiveState.byte_before_the_zipfile = centralPos - (uint32_t)centralDirectoryEnd;
    archiveState.central_pos = centralPos;
    archiveState.size_central_dir = centralDirectorySize;
    archiveState.offset_central_dir = centralDirectoryOffset;
    archiveState.pfile_in_zip_read = NULL;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    struct coduo_unz_s *archive = malloc(sizeof(*archive));
    if (archive == NULL) {
        fclose(file);
        return NULL;
    }
    memcpy(archive, &archiveState, sizeof(*archive));
    return archive;
}

int32_t Unzip_Close(struct coduo_unz_s *file)
{
    if (file == NULL) {
        return UNZ_PARAMERROR;
    }

    if (file->pfile_in_zip_read != NULL) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        Unzip_CloseCurrentFile(file);
    }

    fclose(file->file);
    free(file);

    return UNZ_OK;
}

int32_t Unzip_GetGlobalInfo(struct coduo_unz_s *file, coduo_zip_global_info_t *globalInfo)
{
    if (file == NULL) {
        return UNZ_PARAMERROR;
    }

    globalInfo->number_entry = file->number_entry;
    globalInfo->size_comment = file->size_comment;

    return UNZ_OK;
}

void Unzip_DecodeDosDate(uint32_t dosDate, uint32_t *dateParts)
{
    dateParts[3] = (dosDate >> UNZ_DOS_TIME_DAY_SHIFT) & UNZ_DOS_TIME_DAY_MASK;
    dateParts[4] = ((dosDate >> UNZ_DOS_TIME_MONTH_SHIFT) & UNZ_DOS_TIME_MONTH_MASK) - 1;
    dateParts[5] = (dosDate >> UNZ_DOS_TIME_YEAR_SHIFT) + UNZ_DOS_TIME_YEAR_BASE;
    dateParts[2] = (dosDate >> UNZ_DOS_TIME_HOUR_SHIFT) & UNZ_DOS_TIME_HOUR_MASK;
    dateParts[1] = (dosDate >> UNZ_DOS_TIME_MINUTE_SHIFT) & UNZ_DOS_TIME_MINUTE_MASK;
    dateParts[0] = (dosDate & UNZ_DOS_TIME_SECOND_MASK) * UNZ_DOS_TIME_SECOND_SCALE;
}

int32_t Unzip_GetCurrentFileInfoInternal(struct coduo_unz_s *file, coduo_zip_file_info_t *fileInfo, uint32_t *localHeaderOffset,
                                         char *filename, uint32_t filenameSize, void *extra, uint32_t extraSize, void *comment,
                                         uint32_t commentSize)
{
    if (file == NULL) {
        return UNZ_PARAMERROR;
    }

    const uint64_t centralDirectoryEnd = (uint64_t)file->offset_central_dir + (uint64_t)file->size_central_dir;
    const uint64_t currentCentralPosition = file->pos_in_central_dir;

    /* NOT_FROM_ORIGINAL_SOURCE: each central header and its fixed fields must
     * remain within the declared central-directory envelope before parsing. */
    if (currentCentralPosition < file->offset_central_dir || currentCentralPosition > centralDirectoryEnd ||
        UNZ_CENTRAL_DIRECTORY_FIXED_SIZE > centralDirectoryEnd - currentCentralPosition) {
        return UNZ_BADZIPFILE;
    }

    int32_t status = UNZ_OK;
    /* NOT_FROM_ORIGINAL_SOURCE: initialize the metadata aggregate before its
     * independent scalar-read sequence; failed reads publish no partial value. */
    coduo_zip_file_info_t centralInfo = {0};
    uint32_t offsetCurfile = 0;
    uint32_t signature = 0;

    if (fseek(file->file, (long)(int32_t)(file->byte_before_the_zipfile + file->pos_in_central_dir), SEEK_SET) != 0) {
        status = UNZ_ERRNO;
    }
    if (status == UNZ_OK) {
        if (Sys_ReadLittleLong(file->file, &signature) != UNZ_OK) {
            status = UNZ_ERRNO;
        } else if (signature != UNZ_CENTRAL_DIRECTORY_HEADER_SIGNATURE) {
            status = UNZ_BADZIPFILE;
        }
    }

    /*
     * 0x080cbdb6..0x080cbfc3 calls every scalar reader independently.  Keep
     * that sequence explicit rather than making later reads depend on an
     * earlier helper's return value.
     */
    if (Sys_ReadLittleShort(file->file, &centralInfo.version) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleShort(file->file, &centralInfo.version_needed) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleShort(file->file, &centralInfo.flag) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleShort(file->file, &centralInfo.compression_method) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleLong(file->file, &centralInfo.dos_date) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    Unzip_DecodeDosDate(centralInfo.dos_date, centralInfo.tmu_date);
    if (Sys_ReadLittleLong(file->file, &centralInfo.crc) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleLong(file->file, &centralInfo.compressed_size) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleLong(file->file, &centralInfo.uncompressed_size) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleShort(file->file, &centralInfo.size_filename) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleShort(file->file, &centralInfo.size_file_extra) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleShort(file->file, &centralInfo.size_file_comment) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleShort(file->file, &centralInfo.disk_num_start) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleShort(file->file, &centralInfo.internal_fa) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleLong(file->file, &centralInfo.external_fa) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleLong(file->file, &offsetCurfile) != UNZ_OK) {
        status = UNZ_ERRNO;
    }

    if (status == UNZ_OK) {
        const uint64_t variableSize =
            (uint64_t)centralInfo.size_filename + (uint64_t)centralInfo.size_file_extra + (uint64_t)centralInfo.size_file_comment;
        const uint64_t availableVariableSize = centralDirectoryEnd - currentCentralPosition - UNZ_CENTRAL_DIRECTORY_FIXED_SIZE;
        if (variableSize > availableVariableSize)
            status = UNZ_BADZIPFILE;
    }

    uint32_t skipLength = centralInfo.size_filename;
    if (status == UNZ_OK && filename != NULL) {
        uint32_t readLength = centralInfo.size_filename;

        /* NOT_FROM_ORIGINAL_SOURCE: the separately reported filename length
         * tells callers whether this copied field has a terminator. */
        if (centralInfo.size_filename < filenameSize) {
            filename[centralInfo.size_filename] = '\0';
        } else {
            readLength = filenameSize;
        }

        if (centralInfo.size_filename != 0 && filenameSize != 0 &&
            fread(filename, readLength, UNZ_FREAD_ELEMENT_COUNT, file->file) != UNZ_FREAD_ELEMENT_COUNT) {
            status = UNZ_ERRNO;
        }
        skipLength = centralInfo.size_filename - readLength;
    }

    if (status == UNZ_OK && extra != NULL) {
        uint32_t readLength = centralInfo.size_file_extra;

        if (centralInfo.size_file_extra > extraSize) {
            readLength = extraSize;
        }
        if (skipLength != 0 && fseek(file->file, (long)(int32_t)skipLength, SEEK_CUR) != 0) {
            status = UNZ_ERRNO;
        } else {
            skipLength = 0;
        }
        if (centralInfo.size_file_extra != 0 && extraSize != 0 &&
            fread(extra, readLength, UNZ_FREAD_ELEMENT_COUNT, file->file) != UNZ_FREAD_ELEMENT_COUNT) {
            status = UNZ_ERRNO;
        }
        skipLength += centralInfo.size_file_extra - readLength;
    } else {
        skipLength += centralInfo.size_file_extra;
    }

    if (status == UNZ_OK && comment != NULL) {
        uint32_t readLength = centralInfo.size_file_comment;

        if (centralInfo.size_file_comment < commentSize) {
            ((char *)comment)[centralInfo.size_file_comment] = '\0';
        } else {
            readLength = commentSize;
        }
        if (skipLength != 0 && fseek(file->file, (long)(int32_t)skipLength, SEEK_CUR) != 0) {
            status = UNZ_ERRNO;
        } else {
            skipLength = 0;
        }
        if (centralInfo.size_file_comment != 0 && commentSize != 0 &&
            fread(comment, readLength, UNZ_FREAD_ELEMENT_COUNT, file->file) != UNZ_FREAD_ELEMENT_COUNT) {
            status = UNZ_ERRNO;
        }
    }

    if (status == UNZ_OK && fileInfo != NULL) {
        memcpy(fileInfo, &centralInfo, sizeof(*fileInfo));
    }
    if (status == UNZ_OK && localHeaderOffset != NULL) {
        *localHeaderOffset = offsetCurfile;
    }

    return status;
}

int32_t Unzip_GetCurrentFileInfo(struct coduo_unz_s *file, coduo_zip_file_info_t *fileInfo, char *filename, uint32_t filenameSize,
                                 void *extra, uint32_t extraSize, void *comment, uint32_t commentSize)
{
    return Unzip_GetCurrentFileInfoInternal(file, fileInfo, NULL, filename, filenameSize, extra, extraSize, comment, commentSize);
}

int32_t Unzip_GetCurrentFilePosition(struct coduo_unz_s *file, fileInPack_t *position)
{
    if (file == NULL) {
        return UNZ_PARAMERROR;
    }

    position->zipPosition = (int32_t)file->pos_in_central_dir;
    return UNZ_OK;
}

int32_t Unzip_GoToFilePosition(struct coduo_unz_s *file, int32_t position)
{
    if (file == NULL) {
        return UNZ_PARAMERROR;
    }

    file->pos_in_central_dir = (uint32_t)position;
    const int32_t status = Unzip_GetCurrentFileInfoInternal(file, &file->cur_file_info, &file->cur_file_info_internal_offset_curfile, NULL,
                                                            0, NULL, 0, NULL, 0);
    file->current_file_ok = status == UNZ_OK ? qtrue : qfalse;

    /* NOT_FROM_ORIGINAL_SOURCE: return the parser result for the requested
     * directory position so callers never publish stale member metadata. */
    return status;
}

int32_t Unzip_GoToFirstFile(struct coduo_unz_s *file)
{
    int32_t status;

    if (file == NULL) {
        return UNZ_PARAMERROR;
    }

    file->pos_in_central_dir = file->offset_central_dir;
    file->num_file = 0;
    status = Unzip_GetCurrentFileInfoInternal(file, &file->cur_file_info, &file->cur_file_info_internal_offset_curfile, NULL, 0, NULL, 0,
                                              NULL, 0);
    file->current_file_ok = status == UNZ_OK ? qtrue : qfalse;

    return status;
}

int32_t Unzip_GoToNextFile(struct coduo_unz_s *file)
{
    int32_t status;

    if (file == NULL) {
        return UNZ_PARAMERROR;
    }
    if (file->current_file_ok == qfalse) {
        return UNZ_END_OF_LIST_OF_FILE;
    }
    if (file->num_file + 1 == file->number_entry) {
        return UNZ_END_OF_LIST_OF_FILE;
    }

    const uint64_t nextCentralPosition = (uint64_t)file->pos_in_central_dir + UNZ_CENTRAL_DIRECTORY_FIXED_SIZE +
                                         (uint64_t)file->cur_file_info.size_filename + (uint64_t)file->cur_file_info.size_file_extra +
                                         (uint64_t)file->cur_file_info.size_file_comment;
    const uint64_t centralDirectoryEnd = (uint64_t)file->offset_central_dir + (uint64_t)file->size_central_dir;

    /* NOT_FROM_ORIGINAL_SOURCE: advancing by variable-length fields must remain
     * within both the central-directory envelope and the cursor domain. */
    if (nextCentralPosition > centralDirectoryEnd || nextCentralPosition > UINT32_MAX) {
        file->current_file_ok = qfalse;
        return UNZ_BADZIPFILE;
    }
    file->pos_in_central_dir = (uint32_t)nextCentralPosition;
    file->num_file++;
    status = Unzip_GetCurrentFileInfoInternal(file, &file->cur_file_info, &file->cur_file_info_internal_offset_curfile, NULL, 0, NULL, 0,
                                              NULL, 0);
    file->current_file_ok = status == UNZ_OK ? qtrue : qfalse;

    return status;
}

int32_t Unzip_LocateFile(struct coduo_unz_s *file, const char *filename, int32_t compareMode)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (file == NULL || filename == NULL || strlen(filename) >= UNZ_FILE_NAME_SEARCH_LIMIT) {
        return UNZ_PARAMERROR;
    }
    if (file->current_file_ok == qfalse) {
        return UNZ_END_OF_LIST_OF_FILE;
    }

    const uint32_t savedNumFile = file->num_file;
    const uint32_t savedPosInCentralDir = file->pos_in_central_dir;
    const qboolean savedCurrentFileOk = file->current_file_ok;
    const coduo_zip_file_info_t savedCurrentFileInfo = file->cur_file_info;
    const uint32_t savedLocalHeaderOffset = file->cur_file_info_internal_offset_curfile;
    const long savedFilePosition = ftell(file->file);
    if (savedFilePosition < 0)
        return UNZ_ERRNO;
    int32_t status = Unzip_GoToFirstFile(file);
    char currentName[UNZ_FILE_NAME_SEARCH_LIMIT];

    while (status == UNZ_OK) {
        /* NOT_FROM_ORIGINAL_SOURCE: skip a stored name that cannot fit with a
         * terminator before copying it for C-string comparison. */
        if (file->cur_file_info.size_filename >= sizeof(currentName)) {
            status = Unzip_GoToNextFile(file);
            continue;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: compare a name only after the repeated
         * metadata read has produced a complete successful result. */
        const int32_t metadataStatus = Unzip_GetCurrentFileInfo(file, NULL, currentName, sizeof(currentName), NULL, 0, NULL, 0);
        if (metadataStatus != UNZ_OK) {
            status = metadataStatus;
            break;
        }
        if (Sys_ZipStringCompare(currentName, filename, compareMode) == 0) {
            return UNZ_OK;
        }

        status = Unzip_GoToNextFile(file);
    }

    /* NOT_FROM_ORIGINAL_SOURCE: restore the complete saved logical cursor and
     * member metadata when a search does not publish a match. */
    file->num_file = savedNumFile;
    file->pos_in_central_dir = savedPosInCentralDir;
    file->current_file_ok = savedCurrentFileOk;
    file->cur_file_info = savedCurrentFileInfo;
    file->cur_file_info_internal_offset_curfile = savedLocalHeaderOffset;
    /* NOT_FROM_ORIGINAL_SOURCE: restore the shared stdio cursor together with
     * the logical entry state. */
    if (fseek(file->file, savedFilePosition, SEEK_SET) != 0)
        return UNZ_ERRNO;

    return status;
}

int32_t Unzip_CheckCurrentFileCoherencyHeader(struct coduo_unz_s *file, uint32_t *localHeaderSize, uint32_t *localExtraOffset,
                                              uint32_t *localExtraSize)
{
    int32_t status = UNZ_OK;
    uint32_t signature = 0;
    uint32_t value = 0;
    uint32_t flag = 0;
    uint32_t filenameSize = 0;
    uint32_t extraSize = 0;

    *localHeaderSize = 0;
    *localExtraOffset = 0;
    *localExtraSize = 0;

    const uint64_t localHeaderOffset = file->cur_file_info_internal_offset_curfile;
    /* NOT_FROM_ORIGINAL_SOURCE: a local header's fixed fields must end before
     * the declared central-directory boundary before seeking. */
    if (localHeaderOffset > file->offset_central_dir ||
        UNZ_LOCAL_FILE_HEADER_FIXED_SIZE > (uint64_t)file->offset_central_dir - localHeaderOffset) {
        return UNZ_BADZIPFILE;
    }

    if (fseek(file->file, (long)(int32_t)(file->byte_before_the_zipfile + file->cur_file_info_internal_offset_curfile), SEEK_SET) != 0) {
        return UNZ_ERRNO;
    }

    if (Sys_ReadLittleLong(file->file, &signature) != UNZ_OK) {
        status = UNZ_ERRNO;
    } else if (signature != UNZ_LOCAL_FILE_HEADER_SIGNATURE) {
        status = UNZ_BADZIPFILE;
    }
    if (Sys_ReadLittleShort(file->file, &value) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleShort(file->file, &flag) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleShort(file->file, &value) != UNZ_OK) {
        status = UNZ_ERRNO;
    } else if (status == UNZ_OK && value != file->cur_file_info.compression_method) {
        status = UNZ_BADZIPFILE;
    }

    if (status == UNZ_OK && file->cur_file_info.compression_method != UNZ_STORED_METHOD &&
        file->cur_file_info.compression_method != UNZ_DEFLATED_METHOD) {
        status = UNZ_BADZIPFILE;
    }

    if (Sys_ReadLittleLong(file->file, &value) != UNZ_OK) {
        status = UNZ_ERRNO;
    }
    if (Sys_ReadLittleLong(file->file, &value) != UNZ_OK) {
        status = UNZ_ERRNO;
    } else if (status == UNZ_OK && value != file->cur_file_info.crc && (flag & UNZ_FLAG_DATA_DESCRIPTOR) == 0) {
        status = UNZ_BADZIPFILE;
    }

    if (Sys_ReadLittleLong(file->file, &value) != UNZ_OK) {
        status = UNZ_ERRNO;
    } else if (status == UNZ_OK && value != file->cur_file_info.compressed_size && (flag & UNZ_FLAG_DATA_DESCRIPTOR) == 0) {
        status = UNZ_BADZIPFILE;
    }

    if (Sys_ReadLittleLong(file->file, &value) != UNZ_OK) {
        status = UNZ_ERRNO;
    } else if (status == UNZ_OK && value != file->cur_file_info.uncompressed_size && (flag & UNZ_FLAG_DATA_DESCRIPTOR) == 0) {
        status = UNZ_BADZIPFILE;
    }

    if (Sys_ReadLittleShort(file->file, &filenameSize) != UNZ_OK) {
        status = UNZ_ERRNO;
    } else if (status == UNZ_OK && filenameSize != file->cur_file_info.size_filename) {
        status = UNZ_BADZIPFILE;
    }
    *localHeaderSize += filenameSize;

    if (Sys_ReadLittleShort(file->file, &extraSize) != UNZ_OK) {
        status = UNZ_ERRNO;
    }

    *localExtraOffset = file->cur_file_info_internal_offset_curfile + UNZ_LOCAL_FILE_HEADER_FIXED_SIZE + filenameSize;
    *localExtraSize = extraSize;
    *localHeaderSize += extraSize;

    /* NOT_FROM_ORIGINAL_SOURCE: the complete local header and compressed member
     * must end at or before the declared central-directory boundary. */
    const uint64_t memberDataStart = localHeaderOffset + UNZ_LOCAL_FILE_HEADER_FIXED_SIZE + (uint64_t)filenameSize + (uint64_t)extraSize;
    const uint64_t memberDataEnd = memberDataStart + (uint64_t)file->cur_file_info.compressed_size;
    if (status == UNZ_OK && (memberDataStart > file->offset_central_dir || memberDataEnd > file->offset_central_dir)) {
        status = UNZ_BADZIPFILE;
    }

    return status;
}

int32_t Unzip_OpenCurrentFile(struct coduo_unz_s *file)
{
    if (file == NULL || file->current_file_ok == qfalse) {
        return UNZ_PARAMERROR;
    }

    if (file->pfile_in_zip_read != NULL) {
        Unzip_CloseCurrentFile(file);
    }

    uint32_t localHeaderSize = 0;
    uint32_t localExtraOffset = 0;
    uint32_t localExtraSize = 0;
    if (Unzip_CheckCurrentFileCoherencyHeader(file, &localHeaderSize, &localExtraOffset, &localExtraSize) != UNZ_OK) {
        return UNZ_BADZIPFILE;
    }

    coduo_unz_read_info_t *readInfo = malloc(sizeof(*readInfo));
    if (readInfo == NULL) {
        return UNZ_INTERNALERROR;
    }

    readInfo->read_buffer = malloc(UNZ_READ_BUFFER_SIZE);
    readInfo->offset_local_extrafield = localExtraOffset;
    readInfo->size_local_extrafield = localExtraSize;
    readInfo->pos_local_extrafield = 0;
    if (readInfo->read_buffer == NULL) {
        free(readInfo);
        return UNZ_INTERNALERROR;
    }

    readInfo->stream_initialised = qfalse;
    readInfo->crc32_wait = file->cur_file_info.crc;
    readInfo->crc32 = 0;
    readInfo->compression_method = file->cur_file_info.compression_method;
    readInfo->file = file->file;
    readInfo->byte_before_the_zipfile = file->byte_before_the_zipfile;
    readInfo->stream.total_out = 0;

    if (readInfo->compression_method != UNZ_STORED_METHOD) {
        readInfo->stream.zalloc = NULL;
        readInfo->stream.zfree = NULL;
        readInfo->stream.opaque = NULL;
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        const int32_t inflateStatus =
            coduomp_zlib_inflate_init2(&readInfo->stream, UNZ_RAW_DEFLATE_WINDOW_BITS, unz_zlibVersion, UNZ_STOCK_Z_STREAM_SIZE);
        if (inflateStatus != UNZ_OK) {
            free(readInfo->read_buffer);
            free(readInfo);
            return inflateStatus;
        }
        readInfo->stream_initialised = qtrue;
    }

    readInfo->rest_read_compressed = file->cur_file_info.compressed_size;
    readInfo->rest_read_uncompressed = file->cur_file_info.uncompressed_size;
    readInfo->file_offset_after_local_extra =
        file->cur_file_info_internal_offset_curfile + UNZ_LOCAL_FILE_HEADER_FIXED_SIZE + localHeaderSize;
    readInfo->stream.avail_in = 0;
    file->pfile_in_zip_read = readInfo;
    return UNZ_OK;
}

int32_t Unzip_ReadCurrentFile(struct coduo_unz_s *file, void *buffer, uint32_t length)
{
    if (file == NULL || file->pfile_in_zip_read == NULL) {
        return UNZ_PARAMERROR;
    }
    if (file->pfile_in_zip_read->read_buffer == NULL) {
        return UNZ_END_OF_LIST_OF_FILE;
    }
    if (length == 0) {
        return UNZ_OK;
    }

    coduo_unz_read_info_t *readInfo = file->pfile_in_zip_read;
    readInfo->stream.next_out = buffer;
    readInfo->stream.avail_out = length;
    if (readInfo->rest_read_uncompressed < length) {
        readInfo->stream.avail_out = readInfo->rest_read_uncompressed;
    }

    int32_t result = UNZ_OK;
    int32_t bytesRead = 0;
    while (readInfo->stream.avail_out != 0) {
        if (readInfo->stream.avail_in == 0 && readInfo->rest_read_compressed != 0) {
            uint32_t readLength = UNZ_READ_BUFFER_SIZE;

            if (readInfo->rest_read_compressed < UNZ_READ_BUFFER_SIZE) {
                readLength = readInfo->rest_read_compressed;
            }
            if (readLength == 0) {
                return bytesRead;
            }
            if (file->cur_file_info.compressed_size == readInfo->rest_read_compressed &&
                fseek(readInfo->file, (long)(int32_t)(readInfo->byte_before_the_zipfile + readInfo->file_offset_after_local_extra),
                      SEEK_SET) != 0) {
                return UNZ_ERRNO;
            }
            if (fread(readInfo->read_buffer, readLength, UNZ_FREAD_ELEMENT_COUNT, readInfo->file) != UNZ_FREAD_ELEMENT_COUNT) {
                return UNZ_ERRNO;
            }

            readInfo->file_offset_after_local_extra += readLength;
            readInfo->rest_read_compressed -= readLength;
            readInfo->stream.next_in = readInfo->read_buffer;
            readInfo->stream.avail_in = readLength;
        }

        if (readInfo->compression_method == UNZ_STORED_METHOD) {
            uint32_t copyLength = readInfo->stream.avail_out;

            if (readInfo->stream.avail_in < copyLength) {
                copyLength = readInfo->stream.avail_in;
            }
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (copyLength == 0) {
                return UNZ_BADZIPFILE;
            }

            for (uint32_t index = 0; index < copyLength; index++) {
                readInfo->stream.next_out[index] = readInfo->stream.next_in[index];
            }
            readInfo->crc32 = Unzip_UpdateCrc32(readInfo->crc32, readInfo->stream.next_out, copyLength);
            readInfo->rest_read_uncompressed -= copyLength;
            readInfo->stream.avail_in -= copyLength;
            readInfo->stream.avail_out -= copyLength;
            readInfo->stream.next_out += copyLength;
            readInfo->stream.next_in += copyLength;
            readInfo->stream.total_out += copyLength;
            bytesRead += (int32_t)copyLength;
        } else {
            uint32_t totalOutBefore = readInfo->stream.total_out;
            uint8_t *nextOutBefore = readInfo->stream.next_out;

            result = coduomp_zlib_inflate(&readInfo->stream, UNZ_Z_SYNC_FLUSH);
            uint32_t produced = readInfo->stream.total_out - totalOutBefore;
            readInfo->crc32 = Unzip_UpdateCrc32(readInfo->crc32, nextOutBefore, produced);
            readInfo->rest_read_uncompressed -= produced;
            bytesRead += (int32_t)produced;

            if (result == UNZ_Z_STREAM_END) {
                return bytesRead;
            }
            if (result != UNZ_OK) {
                return result;
            }
        }
    }

    return result == UNZ_OK ? bytesRead : result;
}

int32_t Unzip_TellCurrentFile(struct coduo_unz_s *file)
{
    if (file == NULL || file->pfile_in_zip_read == NULL) {
        return UNZ_PARAMERROR;
    }

    return (int32_t)file->pfile_in_zip_read->stream.total_out;
}

int32_t Unzip_CurrentFileEof(struct coduo_unz_s *file)
{
    if (file == NULL || file->pfile_in_zip_read == NULL) {
        return UNZ_PARAMERROR;
    }

    return file->pfile_in_zip_read->rest_read_uncompressed == 0 ? UNZ_EOF_REACHED : UNZ_OK;
}

int32_t Unzip_GetLocalExtraField(struct coduo_unz_s *file, void *buffer, uint32_t length)
{
    if (file == NULL || file->pfile_in_zip_read == NULL) {
        return UNZ_PARAMERROR;
    }

    coduo_unz_read_info_t *readInfo = file->pfile_in_zip_read;
    uint32_t remaining = readInfo->size_local_extrafield - readInfo->pos_local_extrafield;

    if (buffer == NULL) {
        return (int32_t)remaining;
    }

    uint32_t readLength = remaining;
    if (length < readLength) {
        readLength = length;
    }
    if (readLength == 0) {
        return UNZ_OK;
    }

    if (fseek(readInfo->file,
              (long)(int32_t)(readInfo->byte_before_the_zipfile + readInfo->offset_local_extrafield + readInfo->pos_local_extrafield),
              SEEK_SET) != 0) {
        return UNZ_ERRNO;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: read only the already-clipped caller extent and
     * preserve the API's non-consuming logical position. */
    if (fread(buffer, readLength, UNZ_FREAD_ELEMENT_COUNT, readInfo->file) != UNZ_FREAD_ELEMENT_COUNT) {
        return UNZ_ERRNO;
    }
    return (int32_t)readLength;
}

int32_t Unzip_CloseCurrentFile(struct coduo_unz_s *file)
{
    if (file == NULL || file->pfile_in_zip_read == NULL) {
        return UNZ_PARAMERROR;
    }

    coduo_unz_read_info_t *readInfo = file->pfile_in_zip_read;
    int32_t status = UNZ_OK;

    if (readInfo->rest_read_uncompressed == 0 && readInfo->crc32 != readInfo->crc32_wait) {
        status = UNZ_CRCERROR;
    }

    free(readInfo->read_buffer);
    readInfo->read_buffer = NULL;
    if (readInfo->stream_initialised != qfalse) {
        coduomp_zlib_inflate_end(&readInfo->stream);
    }
    readInfo->stream_initialised = qfalse;
    free(readInfo);
    file->pfile_in_zip_read = NULL;

    return status;
}

int32_t Unzip_GetGlobalComment(struct coduo_unz_s *file, char *comment, uint32_t commentSize)
{
    if (file == NULL) {
        return UNZ_PARAMERROR;
    }

    uint32_t readLength = file->size_comment;
    if (commentSize < readLength) {
        readLength = commentSize;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (readLength != 0 && comment == NULL)
        return UNZ_PARAMERROR;

    if (fseek(file->file, (long)(int32_t)(file->central_pos + UNZ_EOCD_FIXED_SIZE), SEEK_SET) != 0) {
        return UNZ_ERRNO;
    }
    if (readLength != 0) {
        comment[0] = '\0';
        if (fread(comment, readLength, UNZ_FREAD_ELEMENT_COUNT, file->file) != UNZ_FREAD_ELEMENT_COUNT) {
            return UNZ_ERRNO;
        }
    }
    if (comment != NULL && file->size_comment < commentSize) {
        comment[file->size_comment] = '\0';
    }

    return (int32_t)readLength;
}
