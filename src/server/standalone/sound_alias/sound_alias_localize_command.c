#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../core_runtime/core_runtime_private.h"
#include "../filesystem/fs_private.h"
#include "sound/alias/sound_alias.h"

#define SOUND_ALIAS_EMPTY_STRING ""
#define SOUND_ALIAS_SUBTITLE_STRING_FILE "soundaliases/subtitle.st"
#define SOUND_ALIAS_SOURCE_SUBTITLE_STRING_FILE \
    "../source_data/string_resources/subtitle.st"
#define SOUND_ALIAS_CSV_UPDATE_MODE "r+"
#define SOUND_ALIAS_CSV_EXTENSION "csv"
#define SOUND_ALIAS_CSV_DIRECTORY "soundaliases"
#define SOUND_ALIAS_CSV_QPATH_FORMAT "soundaliases/%s"

/* Source: coduo_lnxded 0x0806fb9b..0x0806fd9f. */
void Com_LocalizeSoundAliasSubtitleText(void)
{
    char sourceStringEdPath[MAX_OSPATH];
    char localStringEdPath[MAX_OSPATH];
    char csvPath[MAX_OSPATH];
    FILE *writableProbe;
    char **soundAliasFiles;
    size_t sourcePathLength;
    int soundAliasFileCount;
    int fileIndex;

    FS_BuildOSPath(fs_homepath->string,
                   SOUND_ALIAS_SOURCE_SUBTITLE_STRING_FILE,
                   SOUND_ALIAS_EMPTY_STRING, sourceStringEdPath);
    sourcePathLength = strlen(sourceStringEdPath);
    sourceStringEdPath[sourcePathLength - 1] = '\0';

    writableProbe = fopen(sourceStringEdPath, SOUND_ALIAS_CSV_UPDATE_MODE);
    if (writableProbe == NULL) {
        Com_Printf("WARNING: Can not write to StringEd file %s\n",
                   sourceStringEdPath);
        return;
    }

    fclose(writableProbe);
    FS_BuildOSPath(fs_basepath->string, fs_currentGameDir,
                   SOUND_ALIAS_SUBTITLE_STRING_FILE, localStringEdPath);
    FS_Copyfiles(sourceStringEdPath, localStringEdPath);

    if (!FS_FileExists(SOUND_ALIAS_SUBTITLE_STRING_FILE)) {
        Com_Printf("WARNING: Could not make local copy of StringEd file %s\n",
                   SOUND_ALIAS_SUBTITLE_STRING_FILE);
        return;
    }

    Com_Printf("Localizing sound alias subtitle text...\n");
    Com_Printf("Writing to StringEd file %s\n", sourceStringEdPath);

    soundAliasFiles = FS_ListFiles(SOUND_ALIAS_CSV_DIRECTORY,
                                   SOUND_ALIAS_CSV_EXTENSION,
                                   &soundAliasFileCount);
    if (soundAliasFileCount == 0) {
        Com_Printf(
            "WARNING: can't find any sound alias files "
            "(soundaliases/*.csv)\n");
        return;
    }

    for (fileIndex = 0; fileIndex < soundAliasFileCount; fileIndex++) {
        Hunk_SetMarkTemp();
        Com_sprintf(csvPath, sizeof(csvPath), SOUND_ALIAS_CSV_QPATH_FORMAT,
                    soundAliasFiles[fileIndex]);
        Com_LocalizeSoundAliasCsvFile(csvPath, localStringEdPath);
        Hunk_ClearToMarkTemp();
    }

    FS_FreeFileList(soundAliasFiles);
    COM_WriteFinalStringEdFile(localStringEdPath, sourceStringEdPath);
    FS_Remove(localStringEdPath);
    Com_Printf("done\n");
}
