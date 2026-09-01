#ifndef FILESYSTEM_COMPAT_TYPES_H
#define FILESYSTEM_COMPAT_TYPES_H

#include <stdint.h>

/* NOT_FROM_ORIGINAL_SOURCE: narrow carriers between the common filesystem
 * algorithms and each target's distinct ZIP implementation. */
typedef struct filesystem_compat_archive_file_info_s {
    uint32_t crc;
    uint64_t uncompressedSize;
    uint32_t filenameLength;
} filesystem_compat_archive_file_info_t;

#endif
