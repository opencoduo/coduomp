#include <stddef.h>

#include "sound_alias_private.h"

#define SOUND_ALIAS_LOADSPEC_LIST_LIMIT 4095
#define SOUND_ALIAS_DEFAULT_LOADSPEC "soundloadspecs/mp/default.csv"
#define SOUND_ALIAS_LOADSPEC_NOT_FOUND_MESSAGE \
    "\x15" "Could not load either sound loadspec file %s or %s"

/* CoDUOMP.exe 0x00437ce0..0x00437d36 and coduo_lnxded
 * 0x0806e081..0x0806e108; canonical name confirmed by the supporting Mac
 * engine symbol. */
int32_t
Com_AddFileToList(
    const char *text, char **table,
    int32_t count)
{
    int32_t index;

    if (count == SOUND_ALIAS_LOADSPEC_LIST_LIMIT) {
        return count;
    }

    for (index = 0; index < count; index++) {
        if (Q_stricmp(text, table[index]) == 0) {
            return count;
        }
    }

    table[count] = CopyStringInternal(text);
    return count + 1;
}

/* CoDUOMP.exe 0x00437d40..0x00437d6b and coduo_lnxded
 * 0x0806e108..0x0806e15e; canonical name confirmed by the supporting Mac
 * engine symbol. */
void Com_FreeFileList(char **list)
{
    int32_t index;

    if (list == NULL) {
        return;
    }

    for (index = 0; list[index] != NULL; index++) {
        Z_FreeInternal(list[index]);
    }

    Z_FreeInternal(list);
}

/* CoDUOMP.exe 0x00437d70..0x00437f3e and coduo_lnxded
 * 0x0806e15e..0x0806e35a; canonical name confirmed by the supporting Mac
 * engine symbol. */
char **Com_ParseLoadSpecFile(
    const char *filename, int32_t *countOut)
{
    const char *activeFilename;
    void *fileBuffer;
    char *parseCursor;
    char *token;
    char *loadspecs[SOUND_ALIAS_LOADSPEC_LIST_LIMIT];
    char **list;
    int32_t count;
    int32_t index;

    activeFilename = filename;
    count = 0;
    if (FS_ReadFile(activeFilename, &fileBuffer) < 0) {
        activeFilename = SOUND_ALIAS_DEFAULT_LOADSPEC;
        if (FS_ReadFile(activeFilename, &fileBuffer) < 0) {
            Com_Error(ERR_DROP,
                      SOUND_ALIAS_LOADSPEC_NOT_FOUND_MESSAGE,
                      filename, activeFilename);
            return NULL;
        }
    }

    Com_BeginParseSession(activeFilename);
    Com_SetCSV(qtrue);
    parseCursor = fileBuffer;
    while (parseCursor != NULL) {
        token = Com_Parse(&parseCursor);
        if (token[0] == '\0' || token[0] == '#') {
            Com_SkipRestOfLine(&parseCursor);
        } else {
            count =
                Com_AddFileToList(token, loadspecs, count);
            Com_SkipRestOfLine(&parseCursor);
        }
    }

    Com_EndParseSession();
    FS_FreeFile(fileBuffer);
    *countOut = count;
    if (count == 0) {
        return NULL;
    }

    list = Z_MallocInternal(((size_t)count + 1U) * sizeof(*list));
    for (index = 0; index < count; index++) {
        list[index] = loadspecs[index];
    }
    list[index] = NULL;
    return list;
}
