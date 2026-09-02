#ifndef QCOMMON_BSP_VALIDATION_H
#define QCOMMON_BSP_VALIDATION_H

#include "bsp_types.h"

#include <stdint.h>

enum {
    CODUO_BSP_VALIDATION_VALID = -1,
    CODUO_BSP_VALIDATION_SHORT_HEADER = -2
};

/* NOT_FROM_ORIGINAL_SOURCE: common security-validation helper for the
 * renderer, collision loader, and retained map-lump utilities. A nonnegative
 * result identifies the first negative length or nonempty lump whose signed
 * offset/length does not form a range inside the loaded file. Empty lumps do
 * not consume file bytes, and shipped/community maps can leave their unused
 * offsets outside the file. */
static inline int32_t coduo_compat_bsp_invalid_lump_index(const void *fileData, int32_t fileLength)
{
    if (fileData == NULL || fileLength < (int32_t)sizeof(dheader_t)) {
        return CODUO_BSP_VALIDATION_SHORT_HEADER;
    }

    const dheader_t *const header = (const dheader_t *)fileData;
    const uint32_t fileSize = (uint32_t)fileLength;

    for (int32_t lumpIndex = 0; lumpIndex < HEADER_LUMPS; ++lumpIndex) {
        const int32_t fileOffset = header->lumps[lumpIndex].fileofs;
        const int32_t lumpLength = header->lumps[lumpIndex].filelen;

        if (lumpLength == 0) {
            continue;
        }

        if (fileOffset < 0 || lumpLength < 0 || (uint32_t)fileOffset > fileSize || (uint32_t)lumpLength > fileSize - (uint32_t)fileOffset) {
            return lumpIndex;
        }
    }

    return CODUO_BSP_VALIDATION_VALID;
}

#endif
