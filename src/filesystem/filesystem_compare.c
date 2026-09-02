#include "filesystem.h"

#include "filesystem_services.h"
#include "qcommon/q_string.h"

#include <stdlib.h>
#include <string.h>

enum {
    FS_CASE_COMPARE_LIMIT = 99999,
};

/* 0x0099afa0 .bss. The next separately referenced object begins at
 * 0x0099b3a0, proving the original allocation spans 1024 bytes. */
static char fs_shiftedString[MAX_STRING_CHARS];

/* Source: CoDUOMP.exe 0x0042d440..0x0042d491.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0042d440_0042d492.mcode.
 * Name: exact same-module Mac symbol FS_FilenameCompare. ASCII case is
 * ignored, and slash, backslash, and colon are equivalent path separators. */
int32_t FS_FilenameCompare(const char *left, const char *right)
{
    for (;;) {
        int32_t leftCharacter = (int8_t)*left++;
        int32_t rightCharacter = (int8_t)*right++;

        if (leftCharacter >= 'a' && leftCharacter <= 'z')
            leftCharacter -= 'a' - 'A';
        if (rightCharacter >= 'a' && rightCharacter <= 'z')
            rightCharacter -= 'a' - 'A';

        if (leftCharacter == '\\' || leftCharacter == ':')
            leftCharacter = '/';
        if (rightCharacter == '\\' || rightCharacter == ':')
            rightCharacter = '/';

        if (leftCharacter != rightCharacter)
            return -1;
        if (leftCharacter == '\0')
            return 0;
    }
}

/* Source: CoDUOMP.exe 0x0042d4a0..0x0042d63d.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_0042d4a0_0042d63e.mcode.
 * Role name: exact diagnostic strings identify FS_FileCompare. */
qboolean FS_FileCompare(const char *leftPath, const char *rightPath)
{
    FILE *leftFile = fopen(leftPath, "rb");
    if (leftFile == NULL) {
        Com_Error(0, "\x15" "FS_FileCompare: %s does not exist\n",
                  leftPath);
    }

    FILE *rightFile = fopen(rightPath, "rb");
    if (rightFile == NULL) {
        (void)fclose(leftFile);
        return qfalse;
    }

    const int32_t leftPosition = (int32_t)ftell(leftFile);
    (void)fseek(leftFile, 0, SEEK_END);
    const int32_t leftLength = (int32_t)ftell(leftFile);
    (void)fseek(leftFile, (long)leftPosition, SEEK_SET);

    const int32_t rightPosition = (int32_t)ftell(rightFile);
    (void)fseek(rightFile, 0, SEEK_END);
    const int32_t rightLength = (int32_t)ftell(rightFile);
    (void)fseek(rightFile, (long)rightPosition, SEEK_SET);

    if (leftLength != rightLength) {
        (void)fclose(leftFile);
        (void)fclose(rightFile);
        return qfalse;
    }

    const size_t transferSize = (size_t)(uint32_t)leftLength;
    uint8_t *const leftData = Z_MallocInternal(transferSize);
    if (fread(leftData, 1, transferSize, leftFile) != transferSize) {
        Com_Error(0, "\x15" "Short read in FS_FileCompare()\n");
    }
    (void)fclose(leftFile);

    uint8_t *const rightData = Z_MallocInternal(transferSize);
    if (fread(rightData, 1, transferSize, rightFile) != transferSize) {
        Com_Error(0, "\x15" "Short read in FS_FileCompare()\n");
    }
    (void)fclose(rightFile);

    const qboolean equal =
        (leftLength <= 0 ||
         memcmp(leftData, rightData, transferSize) == 0)
            ? qtrue
            : qfalse;
    free(leftData);
    free(rightData);
    return equal;
}

/* Source: CoDUOMP.exe 0x0042d640..0x0042d6ac.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0042d640_0042d6ad.mcode.
 * Name: exact same-module Mac symbol FS_ShiftedStrStr. */
const char *FS_ShiftedStrStr(const char *haystack,
                             const char *encodedNeedle, int8_t shift)
{
    char needle[MAX_OSPATH];
    int32_t index;

    for (index = 0; encodedNeedle[index] != '\0'; ++index) {
        needle[index] = (char)(
            (uint8_t)encodedNeedle[index] + (uint8_t)shift);
    }
    needle[index] = '\0';
    return strstr(haystack, needle);
}

/* Retained original bodies:
 *   CoDUOMP.exe  0x0043f8d0..0x0043f923
 *   coduo_lnxded 0x08075080..0x080750d6
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043f8d0_0043f924.mcode and direct
 * Linux disassembly. Name and signature: exact same-module Mac symbol
 * FS_ShiftStr. Both bodies call strlen once, add the low byte of shift to each
 * source byte, terminate the shared static result, and return it. The Linux
 * destination is 0x0826bd40; the next separately referenced BSS object begins
 * at 0x0826c240, disproving the former recovery's unsupported 256-byte bound
 * and accommodating the retained 1024-byte allocation. The original performs
 * no bounds check; callers must keep text within that result buffer. */
const char *FS_ShiftStr(const char *text, int32_t shift)
{
    const int32_t length = (int32_t)strlen(text);
    int32_t index;

    for (index = 0; index < length; ++index)
        fs_shiftedString[index] = (char)(text[index] + shift);

    fs_shiftedString[index] = '\0';
    return fs_shiftedString;
}

/* Source: CoDUOMP.exe 0x0042d6b0..0x0042d6e1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0042d6b0_0042d6e2.mcode.
 * Name: exact same-module Mac symbol FS_GetExtensionSubString. */
const char *FS_GetExtensionSubString(const char *path)
{
    const char *extension = "";

    for (; *path != '\0'; ++path) {
        if (*path == '.')
            extension = path + 1;
        else if (*path == '/' || *path == '\\')
            extension = "";
    }

    return extension;
}

/* Source: CoDUOMP.exe 0x0042d6f0..0x0042d758.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0042d6f0_0042d759.mcode.
 * Name: exact same-module Mac symbol FS_PureIgnoresExtension. */
qboolean FS_PureIgnoresExtension(const char *extension)
{
    if (*extension == '.')
        ++extension;

    if (fs_compat_stricmp(extension, "cfg") == 0)
        return qtrue;
    if (Q_stricmpn(extension, "menu", FS_CASE_COMPARE_LIMIT) == 0)
        return qtrue;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (Q_stricmpn(extension, ".dm_NETWORK_PROTOCOL_VERSION",
                   FS_CASE_COMPARE_LIMIT) == 0) {
        return qtrue;
    }
    if (Q_stricmpn(extension, "dat", FS_CASE_COMPARE_LIMIT) == 0)
        return qtrue;
    return qfalse;
}
