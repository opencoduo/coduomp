#include "script_import_fields.h"
#include "script_runtime_host.h"
#include "script_string.h"
#include "script_temp_memory.h"
#include "qcommon/com_sprintf.h"

#include <stdio.h>
#include <string.h>

enum {
    SCRIPT_FIELD_TYPE_STRING = 1,
    SCRIPT_FIELD_TYPE_FLOAT = 4,
    SCRIPT_FIELD_TYPE_INT = 5,
    SCRIPT_FIELD_OFFSET_SIZE = sizeof(uint16_t),
    SCRIPT_FIELD_TYPE_SIZE = sizeof(int8_t),
    SCRIPT_FIELD_VALUE_SIZE =
        SCRIPT_FIELD_OFFSET_SIZE + SCRIPT_FIELD_TYPE_SIZE,
    SCRIPT_FIELD_NOT_FOUND = 0,
    SCRIPT_FIELD_QPATH_SIZE = MAX_QPATH,
    SCRIPT_FIELD_STREAM_TERMINATOR_SIZE = 1
};

/* Source: CoDUOMP.exe 0x00487cc0..0x00487d19 and coduo_lnxded
 * 0x080aa19a..0x080aa221.  The original lookup is case-insensitive on both
 * targets.  Field records are a packed byte stream: a NUL-terminated
 * lowercase name, an unaligned uint16_t field offset, and a signed byte field
 * type. */
uint16_t Scr_FindField(const char *name, int32_t *typeOut)
{
    uint8_t *record = script_importFieldBuffer;

    while (*record != '\0') {
        size_t nameSize = strlen((const char *)record) + 1;

        if (SCRIPT_STRICMP(name, (const char *)record) == 0) {
            uint16_t offset;

            memcpy(&offset, record + nameSize, sizeof(offset));
            *typeOut = (int8_t)record[nameSize + SCRIPT_FIELD_OFFSET_SIZE];
            return offset;
        }

        record += nameSize + SCRIPT_FIELD_VALUE_SIZE;
    }

    return SCRIPT_FIELD_NOT_FOUND;
}

/* Source: CoDUOMP.exe 0x00487d20..0x00487ffb and coduo_lnxded
 * 0x080aa222..0x080aa499. */
static void Scr_AddFieldsForFile(const char *filename)
{
    int32_t fileHandle;
    int32_t fileLength =
        FS_FOpenFileByMode(filename, &fileHandle, FS_READ);

    if (fileLength < 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: keep the filename as data through the
         * single variadic formatting pass. */
        Com_Error(ERR_DROP, "\x15" "cannot find '%s'", filename);
    }

    /* Both i386 bodies increment the dword length before passing the
     * allocation extent.  Express the original wrapping ADD on wider hosts. */
    char *fileText = SCRIPT_HUNK_ALLOC_TEMP_HIGH(
        (size_t)((uint32_t)fileLength + 1u));
    FS_Read(fileText, fileLength, fileHandle);
    fileText[fileLength] = '\0';
    FS_FCloseFile(fileHandle);

    char *parse = fileText;
    Com_BeginParseSession("Scr_AddFields");

    for (;;) {
        char *typeToken = Com_Parse(&parse);
        int32_t fieldType;

        if (parse == NULL) {
            Com_EndParseSession();
            SCRIPT_HUNK_CLEAR_TEMP_HIGH();
            return;
        }

        if (strcmp(typeToken, "float") == 0) {
            fieldType = SCRIPT_FIELD_TYPE_FLOAT;
        } else if (strcmp(typeToken, "int") == 0) {
            fieldType = SCRIPT_FIELD_TYPE_INT;
        } else if (strcmp(typeToken, "string") == 0) {
            fieldType = SCRIPT_FIELD_TYPE_STRING;
        } else {
            /* NOT_FROM_ORIGINAL_SOURCE: keep parsed tokens as data through the
             * single variadic formatting pass. */
            Com_Error(ERR_DROP, "\x15" "unknown type '%s' in '%s'",
                      typeToken, filename);
            return;
        }

        char *name = Com_Parse(&parse);
        if (parse == NULL) {
            /* NOT_FROM_ORIGINAL_SOURCE: keep the completed path as data. */
            Com_Error(ERR_DROP, "\x15" "missing field name in '%s'",
                      filename);
        }

        size_t nameSize = strlen(name) + 1;
        for (int32_t index = (int32_t)nameSize - 1; index >= 0; --index) {
            name[index] = (char)SCRIPT_TOLOWER_SIGNED_BYTE(name[index]);
        }

        uint16_t offset = SL_FindCanonicalString(name);
        if (offset == 0) {
            continue;
        }

        int32_t existingType;
        if (Scr_FindField(name, &existingType) != 0) {
            Com_Error(ERR_DROP,
                      "\x15" "duplicate key '%s' in '%s'", name, filename);
        }

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        uint8_t *record =
            TempMalloc(nameSize + SCRIPT_FIELD_VALUE_SIZE) -
            SCRIPT_FIELD_STREAM_TERMINATOR_SIZE;
        memcpy(record, name, nameSize);
        memcpy(record + nameSize, &offset, sizeof(offset));
        record[nameSize + SCRIPT_FIELD_OFFSET_SIZE] = (uint8_t)fieldType;
        record[nameSize + SCRIPT_FIELD_VALUE_SIZE] = '\0';
    }
}

/* Source: CoDUOMP.exe 0x00488000..0x00488115 and coduo_lnxded
 * 0x080aa49a..0x080aa557. */
void Scr_AddFields(const char *path, const char *extension)
{
    int32_t fileCount;
    char **files = FS_ListFiles(path, extension, &fileCount);

    TempMemoryReset();
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    script_importFieldBuffer =
        TempMalloc(SCRIPT_FIELD_STREAM_TERMINATOR_SIZE);
    *script_importFieldBuffer = '\0';

    for (int32_t fileIndex = 0; fileIndex < fileCount; ++fileIndex) {
        char qpath[SCRIPT_FIELD_QPATH_SIZE];

        const size_t pathLength = strlen(path);
        const size_t fileLength = strlen(files[fileIndex]);
        /* NOT_FROM_ORIGINAL_SOURCE: require the complete mounted field qpath,
         * separator, and terminator to fit; do not substitute a truncated
         * filename. */
        if (pathLength > sizeof(qpath) - 2 ||
            fileLength > sizeof(qpath) - pathLength - 2) {
            Com_Printf("WARNING: ignoring overlong script-field path\n");
            continue;
        }
        Com_sprintf(qpath, sizeof(qpath), "%s/%s", path,
                    files[fileIndex]);
        Scr_AddFieldsForFile(qpath);
    }

    if (files != NULL) {
        FS_FreeFileList(files);
    }

    SCRIPT_HUNK_COMMIT_TEMP();
}
