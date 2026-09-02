#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_runtime_private.h"

#define SYS_BYTE_SWAP_32(value) \
    ((((value) & 0x000000ffU) << 24) | (((value) & 0x0000ff00U) << 8) | (((value) & 0x00ff0000U) >> 8) | (((value) & 0xff000000U) >> 24))

#define SYS_HAS_X87_INLINE_ASM CODUO_ENGINE_HAS_X87_INLINE_ASM

enum {
    SYS_NOT_SUPPORTED_RESULT = -1,
    SYS_FILE_HELPER_ERROR = -1,
    SYS_FILE_HELPER_OK = 0,
    SYS_CASE_COMPARE_LEFT_LESS = -1,
    SYS_CASE_COMPARE_EQUAL = 0,
    SYS_CASE_COMPARE_LEFT_GREATER = 1,
    SYS_COMPARE_CASE_DEFAULT = 0,
    SYS_COMPARE_CASE_SENSITIVE = 1,
    SYS_FREAD_ELEMENT_COUNT = 1,
    SYS_SHORT_READ_SIZE = 2,
    SYS_LONG_READ_SIZE = 4,
    SYS_ZIP_EOCD_SIGNATURE_SIZE = 4,
    SYS_ZIP_EOCD_SCAN_MAX_BACKTRACK = 65535,
    SYS_ZIP_EOCD_SCAN_CHUNK_SIZE = 1024,
    SYS_ZIP_EOCD_SCAN_BUFFER_SIZE = SYS_ZIP_EOCD_SCAN_CHUNK_SIZE + SYS_ZIP_EOCD_SIGNATURE_SIZE,
    SYS_ZIP_EOCD_NOT_FOUND = 0,
    SYS_ZIP_EOCD_SIGNATURE_BYTE_0 = 'P',
    SYS_ZIP_EOCD_SIGNATURE_BYTE_1 = 'K',
    SYS_ZIP_EOCD_SIGNATURE_BYTE_2 = 0x05,
    SYS_ZIP_EOCD_SIGNATURE_BYTE_3 = 0x06
};

/*
 * Checked 2026-06-29: these Sys utility address-band leaves return the original
 * unsupported sentinel or are empty platform hooks; no source-level names proven.
 */
int32_t FUN_080cb267(void)
{
    return SYS_NOT_SUPPORTED_RESULT;
}

void FUN_080cb271(void)
{
}

void FUN_080cb276(void)
{
}

void Sys_SnapVector(vec3_t vector)
{
#if SYS_HAS_X87_INLINE_ASM
    __asm__ __volatile__("fnstcw %0" : "=m"(sys_snapVectorSavedFpuControlWord) : : "memory");
    __asm__ __volatile__("fldcw %0" : : "m"(sys_snapVectorFpuControlWord) : "memory");

    __asm__ __volatile__("flds %0\n\t"
                         "fistpl %0\n\t"
                         "fildl %0\n\t"
                         "fstps %0"
                         : "+m"(vector[0])
                         :
                         : "memory");
    __asm__ __volatile__("flds %0\n\t"
                         "fistpl %0\n\t"
                         "fildl %0\n\t"
                         "fstps %0"
                         : "+m"(vector[1])
                         :
                         : "memory");
    __asm__ __volatile__("flds %0\n\t"
                         "fistpl %0\n\t"
                         "fildl %0\n\t"
                         "fstps %0"
                         : "+m"(vector[2])
                         :
                         : "memory");

    __asm__ __volatile__("fldcw %0" : : "m"(sys_snapVectorSavedFpuControlWord) : "memory");
#elif defined(CODUO_ENGINE_ALLOW_NON_X87_FLOAT)
    vector[0] = (float)(int32_t)rintf(vector[0]);
    vector[1] = (float)(int32_t)rintf(vector[1]);
    vector[2] = (float)(int32_t)rintf(vector[2]);
#else
#error "Sys_SnapVector requires x87 inline assembly for original behavior"
#endif
}

void Sys_SnapVectorWithControlWord(vec3_t vector, uint16_t controlWord)
{
#if SYS_HAS_X87_INLINE_ASM
    __asm__ __volatile__("fnstcw %0" : "=m"(sys_snapVectorSavedFpuControlWord) : : "memory");
    __asm__ __volatile__("fldcw %0" : : "m"(controlWord) : "memory");

    __asm__ __volatile__("flds %0\n\t"
                         "fistpl %0\n\t"
                         "fildl %0\n\t"
                         "fstps %0"
                         : "+m"(vector[0])
                         :
                         : "memory");
    __asm__ __volatile__("flds %0\n\t"
                         "fistpl %0\n\t"
                         "fildl %0\n\t"
                         "fstps %0"
                         : "+m"(vector[1])
                         :
                         : "memory");
    __asm__ __volatile__("flds %0\n\t"
                         "fistpl %0\n\t"
                         "fildl %0\n\t"
                         "fstps %0"
                         : "+m"(vector[2])
                         :
                         : "memory");

    __asm__ __volatile__("fldcw %0" : : "m"(sys_snapVectorSavedFpuControlWord) : "memory");
#elif defined(CODUO_ENGINE_ALLOW_NON_X87_FLOAT)
    (void)controlWord;
    vector[0] = (float)(int32_t)rintf(vector[0]);
    vector[1] = (float)(int32_t)rintf(vector[1]);
    vector[2] = (float)(int32_t)rintf(vector[2]);
#else
#error "Sys_SnapVectorWithControlWord requires x87 inline assembly for original behavior"
#endif
}

int32_t Sys_ShortSwap(uint16_t value)
{
    uint16_t swapped = (uint16_t)((value << 8) | (value >> 8));

    return (int32_t)(int16_t)swapped;
}

int32_t Sys_ShortNoSwap(int16_t value)
{
    return value;
}

int32_t Sys_LongSwap(uint32_t value)
{
    return (int32_t)SYS_BYTE_SWAP_32(value);
}

int32_t Sys_LongNoSwap(int32_t value)
{
    return value;
}

float Sys_FloatSwap(float value)
{
    uint32_t bits;
    float swapped;

    memcpy(&bits, &value, sizeof(bits));
    bits = SYS_BYTE_SWAP_32(bits);
    memcpy(&swapped, &bits, sizeof(swapped));

    return swapped;
}

float Sys_FloatNoSwap(float value)
{
    return value;
}

int32_t Sys_ReadLittleShort(FILE *file, uint32_t *value)
{
    uint16_t rawValue;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (fread(&rawValue, SYS_SHORT_READ_SIZE, SYS_FREAD_ELEMENT_COUNT, file) != SYS_FREAD_ELEMENT_COUNT) {
        return SYS_FILE_HELPER_ERROR;
    }
    *value = (uint32_t)rawValue;

    return SYS_FILE_HELPER_OK;
}

int32_t Sys_ReadLittleLong(FILE *file, uint32_t *value)
{
    int32_t rawValue;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (fread(&rawValue, SYS_LONG_READ_SIZE, SYS_FREAD_ELEMENT_COUNT, file) != SYS_FREAD_ELEMENT_COUNT) {
        return SYS_FILE_HELPER_ERROR;
    }
    *value = (uint32_t)Sys_LongNoSwap(rawValue);

    return SYS_FILE_HELPER_OK;
}

int32_t Sys_ZipStringCompareNoCase(const char *left, const char *right)
{
    for (size_t index = 0;; ++index) {
        char leftChar = left[index];
        char rightChar = right[index];

        if (leftChar > '`' && leftChar < '{') {
            leftChar = (char)(leftChar - ('a' - 'A'));
        }
        if (rightChar > '`' && rightChar < '{') {
            rightChar = (char)(rightChar - ('a' - 'A'));
        }

        if (leftChar == '\0') {
            return rightChar == '\0' ? SYS_CASE_COMPARE_EQUAL : SYS_CASE_COMPARE_LEFT_LESS;
        }
        if (rightChar == '\0') {
            return SYS_CASE_COMPARE_LEFT_GREATER;
        }
        if (leftChar < rightChar) {
            return SYS_CASE_COMPARE_LEFT_LESS;
        }
        if (rightChar < leftChar) {
            return SYS_CASE_COMPARE_LEFT_GREATER;
        }
    }
}

int32_t Sys_ZipStringCompare(const char *left, const char *right, int32_t compareMode)
{
    if (compareMode == SYS_COMPARE_CASE_DEFAULT) {
        compareMode = SYS_COMPARE_CASE_SENSITIVE;
    }

    if (compareMode == SYS_COMPARE_CASE_SENSITIVE) {
        return strcmp(left, right);
    }

    return Sys_ZipStringCompareNoCase(left, right);
}

int32_t Sys_FindZipEndOfCentralDirectory(FILE *file)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    uint32_t maxBacktrack = SYS_ZIP_EOCD_SCAN_MAX_BACKTRACK;
    uint32_t eocdOffset = SYS_ZIP_EOCD_NOT_FOUND;

    if (fseek(file, 0, SEEK_END) != 0) {
        return SYS_ZIP_EOCD_NOT_FOUND;
    }

    /*
     * Stock keeps ftell and every search cursor in dwords.  Preserve that
     * domain on native64, including its unsigned max/backtrack comparisons.
     */
    uint32_t fileLength = (uint32_t)ftell(file);
    if (fileLength < maxBacktrack) {
        maxBacktrack = fileLength;
    }

    uint8_t *buffer = malloc(SYS_ZIP_EOCD_SCAN_BUFFER_SIZE);
    if (buffer == NULL) {
        return SYS_ZIP_EOCD_NOT_FOUND;
    }

    for (uint32_t backtrack = SYS_ZIP_EOCD_SIGNATURE_SIZE; backtrack < maxBacktrack && eocdOffset == SYS_ZIP_EOCD_NOT_FOUND;) {
        if (maxBacktrack < backtrack + SYS_ZIP_EOCD_SCAN_CHUNK_SIZE) {
            backtrack = maxBacktrack;
        } else {
            backtrack += SYS_ZIP_EOCD_SCAN_CHUNK_SIZE;
        }

        uint32_t chunkOffset = fileLength - backtrack;
        size_t chunkLength = (size_t)(fileLength - chunkOffset);
        if (SYS_ZIP_EOCD_SCAN_BUFFER_SIZE < chunkLength) {
            chunkLength = SYS_ZIP_EOCD_SCAN_BUFFER_SIZE;
        }

        if (fseek(file, (long)(int32_t)chunkOffset, SEEK_SET) != 0 ||
            fread(buffer, chunkLength, SYS_FREAD_ELEMENT_COUNT, file) != SYS_FREAD_ELEMENT_COUNT) {
            break;
        }

        for (int32_t scanOffset = (int32_t)chunkLength - (SYS_ZIP_EOCD_SIGNATURE_SIZE - 1); scanOffset >= 1; --scanOffset) {
            int32_t signatureOffset = scanOffset - 1;

            if (buffer[signatureOffset] == SYS_ZIP_EOCD_SIGNATURE_BYTE_0 && buffer[scanOffset] == SYS_ZIP_EOCD_SIGNATURE_BYTE_1 &&
                buffer[scanOffset + 1] == SYS_ZIP_EOCD_SIGNATURE_BYTE_2 && buffer[scanOffset + 2] == SYS_ZIP_EOCD_SIGNATURE_BYTE_3) {
                /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                eocdOffset = chunkOffset + (uint32_t)signatureOffset;
                break;
            }
        }
    }

    free(buffer);
    return (int32_t)eocdOffset;
}
