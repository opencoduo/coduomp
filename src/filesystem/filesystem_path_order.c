#include "filesystem.h"
#include "filesystem_services.h"

#include "qcommon/q_string.h"

#include <stdlib.h>
#include <string.h>

enum {
    FS_LOCALIZED_PAK_PREFIX_LENGTH = 10,
    FS_LANGUAGE_NAME_SIZE = 64
};

/* Source: CoDUOMP.exe 0x0042cb00..0x0042cb1c.
 * Name: exact same-module Mac symbol FS_ReplaceSeparators. This converts an
 * assembled host path to the separator native to the target filesystem. */
void FS_ReplaceSeparators(char *path)
{
    for (; *path != '\0'; ++path) {
        if (*path == '/' || *path == '\\')
            *path = FS_HOST_PATH_SEPARATOR;
    }
}

/* Source: CoDUOMP.exe 0x0042f470..0x0042f48c.
 * Evidence: repaired executable-gap boundary; exact same-module Mac symbol
 * FS_ConvertPath. This distinct helper canonicalizes Quake paths to '/'. */
void FS_ConvertPath(char *path)
{
    for (; *path != '\0'; ++path) {
        if (*path == '\\' || *path == ':')
            *path = '/';
    }
}

/* The authoritative clients and Linux dedicated server fold the same signed
 * byte domain and normalize backslash/colon identically, but return opposite
 * ordering polarity. Their callers compensate by reversing argument order;
 * retain complete target bodies so the public contracts do not drift. */
#if defined(WINDOWS_BEHAVIOR)
int32_t FS_PathCmp(const char *left, const char *right)
{
    for (;;) {
        int32_t rightCharacter = (int8_t)*right++;
        int32_t leftCharacter = (int8_t)*left++;

        if (rightCharacter >= 'a' && rightCharacter <= 'z')
            rightCharacter -= 'a' - 'A';
        if (leftCharacter >= 'a' && leftCharacter <= 'z')
            leftCharacter -= 'a' - 'A';
        if (rightCharacter == '\\' || rightCharacter == ':')
            rightCharacter = '/';
        if (leftCharacter == '\\' || leftCharacter == ':')
            leftCharacter = '/';

        if (rightCharacter < leftCharacter)
            return -1;
        if (rightCharacter > leftCharacter)
            return 1;
        if (rightCharacter == '\0')
            return 0;
    }
}
#else
int32_t FS_PathCmp(const char *leftPath, const char *rightPath)
{
    for (;;) {
        int32_t left = (int32_t)(int8_t)(uint8_t)*leftPath++;
        int32_t right = (int32_t)(int8_t)(uint8_t)*rightPath++;

        if (Q_islower(left) != qfalse)
            left -= 'a' - 'A';
        if (Q_islower(right) != qfalse)
            right -= 'a' - 'A';
        if (left == '\\' || left == ':')
            left = '/';
        if (right == '\\' || right == ':')
            right = '/';

        if (left < right)
            return -1;
        if (right < left)
            return 1;
        if (left == '\0')
            return 0;
    }
}
#endif

/* Source: CoDUOMP.exe 0x0042f4f0..0x0042f599.
 * Name and signature: exact same-module Mac symbol FS_SortFileList. Both
 * targets perform the same stable insertion sort; only the call order needed
 * to consume their opposite FS_PathCmp contracts differs. */
#if defined(WINDOWS_BEHAVIOR)
void FS_SortFileList(char **list, int32_t count)
{
    char **const sortedList =
        Z_MallocInternal(((size_t)count + 1u) * sizeof(*sortedList));
    sortedList[0] = NULL;

    int32_t sortedCount = 0;
    for (int32_t inputIndex = 0; inputIndex < count; ++inputIndex) {
        int32_t insertIndex = 0;
        while (insertIndex < sortedCount &&
               FS_PathCmp(sortedList[insertIndex],
                          list[inputIndex]) >= 0) {
            ++insertIndex;
        }
        for (int32_t shiftIndex = sortedCount;
             shiftIndex > insertIndex; --shiftIndex) {
            sortedList[shiftIndex] = sortedList[shiftIndex - 1];
        }
        sortedList[insertIndex] = list[inputIndex];
        ++sortedCount;
    }

    Com_Memcpy(list, sortedList, (size_t)count * sizeof(*list));
    Z_FreeInternal(sortedList);
}
#else
void FS_SortFileList(char **list, int32_t count)
{
    char **const sortedList =
        Z_MallocInternal(((size_t)count + 1u) * sizeof(*sortedList));
    sortedList[0] = NULL;

    int32_t sortedCount = 0;
    for (int32_t inputIndex = 0; inputIndex < count; ++inputIndex) {
        int32_t insertIndex = 0;
        while (insertIndex < sortedCount &&
               FS_PathCmp(list[inputIndex],
                          sortedList[insertIndex]) >= 0) {
            ++insertIndex;
        }
        for (int32_t shiftIndex = sortedCount;
             shiftIndex > insertIndex; --shiftIndex) {
            sortedList[shiftIndex] = sortedList[shiftIndex - 1];
        }
        sortedList[insertIndex] = list[inputIndex];
        ++sortedCount;
    }

    Com_Memcpy(list, sortedList, (size_t)count * sizeof(*list));
    Z_FreeInternal(sortedList);
}
#endif

/* Source: CoDUOMP.exe 0x0042f780..0x0042f812.
 * Name and alternating-buffer contract: exact same-module Mac symbol
 * PakFileLanguage. */
char *PakFileLanguage(const char *text)
{
    fs_languageNameBufferIndex ^= 1;
    char *const language =
        fs_languageNameBuffers[fs_languageNameBufferIndex];

    if (strlen(text) < FS_LOCALIZED_PAK_PREFIX_LENGTH) {
        language[0] = '\0';
        return language;
    }

    memset(language, 0, FS_LANGUAGE_NAME_SIZE);
    int32_t index = FS_LOCALIZED_PAK_PREFIX_LENGTH;
    while (index < FS_LANGUAGE_NAME_SIZE &&
           text[index] != '\0' &&
           fs_compat_isalpha((int32_t)(int8_t)(uint8_t)text[index]) != 0) {
        language[index - FS_LOCALIZED_PAK_PREFIX_LENGTH] = text[index];
        ++index;
    }
    return language;
}

/* Source: CoDUOMP.exe 0x0042f820..0x0042f927.
 * Name and qsort signature: exact same-module Mac symbol paksort. English
 * localized archives precede other languages; the final call order consumes
 * the target-specific FS_PathCmp polarity. */
#if defined(WINDOWS_BEHAVIOR)
int32_t paksort(const void *leftEntry, const void *rightEntry)
{
    char *const leftPath = *(char *const *)leftEntry;
    char *const rightPath = *(char *const *)rightEntry;

    if (strncmp(leftPath, "          ", FS_LOCALIZED_PAK_PREFIX_LENGTH) == 0 &&
        strncmp(rightPath, "          ", FS_LOCALIZED_PAK_PREFIX_LENGTH) == 0) {
        const char *const leftLanguage = PakFileLanguage(leftPath);
        const char *const rightLanguage = PakFileLanguage(rightPath);

        if (Q_stricmp(leftLanguage, "english") == 0) {
            if (Q_stricmp(rightLanguage, "english") != 0)
                return -1;
        } else if (Q_stricmp(rightLanguage, "english") == 0) {
            return 1;
        }
    }

    return FS_PathCmp(rightPath, leftPath);
}
#else
int32_t paksort(const void *leftEntry, const void *rightEntry)
{
    char *const leftPath = *(char *const *)leftEntry;
    char *const rightPath = *(char *const *)rightEntry;

    if (Q_strncmp(leftPath, "          ", FS_LOCALIZED_PAK_PREFIX_LENGTH) == 0 &&
        Q_strncmp(rightPath, "          ", FS_LOCALIZED_PAK_PREFIX_LENGTH) == 0) {
        const char *const leftLanguage = PakFileLanguage(leftPath);
        const char *const rightLanguage = PakFileLanguage(rightPath);

        if (Q_stricmp(leftLanguage, "english") == 0) {
            if (Q_stricmp(rightLanguage, "english") != 0)
                return -1;
        } else if (Q_stricmp(rightLanguage, "english") == 0) {
            return 1;
        }
    }

    return FS_PathCmp(leftPath, rightPath);
}
#endif
