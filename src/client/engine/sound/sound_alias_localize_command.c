#include "sound/alias/sound_alias.h"

#include "filesystem/filesystem.h"
#include "qcommon/hunk.h"
#include "../q_shared.h"

#include <stdio.h>
#include <string.h>

/* Source: CoDUOMP.exe 0x00439690..0x00439878, recovered from the executable
 * gap between Ghidra records. Name: same-module Mac symbol
 * Com_WriteLocalizedSoundAliasFiles. The editable source-data StringEd file
 * is copied into the virtual filesystem, updated once per sound-alias CSV,
 * then copied back only after every CSV has been processed. */
void Com_WriteLocalizedSoundAliasFiles(void)
{
    static const char sourceStringEdRelativePath[] = "../source_data/string_resources/subtitle.st";
    static const char localStringEdPath[] = "soundaliases/subtitle.st";
    enum {
        LOCALIZE_PATH_CAPACITY = MAX_OSPATH
    };
    char sourceStringEdOSPath[LOCALIZE_PATH_CAPACITY];
    char localStringEdOSPath[LOCALIZE_PATH_CAPACITY];
    char aliasPath[LOCALIZE_PATH_CAPACITY];

    FS_BuildOSPath(fs_homepath->string, sourceStringEdRelativePath, "", sourceStringEdOSPath);
    sourceStringEdOSPath[strlen(sourceStringEdOSPath) - 1] = '\0';

    FILE *const writableProbe = fopen(sourceStringEdOSPath, "r+");
    if (writableProbe == NULL) {
        Com_Printf("WARNING: Can not write to StringEd file %s\n", sourceStringEdOSPath);
        return;
    }
    (void)fclose(writableProbe);

    FS_BuildOSPath(fs_basepath->string, fs_currentGameDir, localStringEdPath, localStringEdOSPath);
    FS_Copyfiles(sourceStringEdOSPath, localStringEdOSPath);
    if (FS_FileExists(localStringEdPath) == qfalse) {
        Com_Printf("WARNING: Could not make local copy of StringEd file %s\n", localStringEdPath);
        return;
    }

    Com_Printf("Localizing sound alias subtitle text...\n");
    Com_Printf("Writing to StringEd file %s\n", sourceStringEdOSPath);

    int32_t fileCount;
    char **const files = FS_ListFilteredFiles("soundaliases", "csv", NULL, &fileCount);
    if (fileCount == 0) {
        Com_Printf("WARNING: can't find any sound alias files "
                   "(soundaliases/*.csv)\n");
        return;
    }

    Hunk_SetMarkTemp();
    for (int32_t fileIndex = 0; fileIndex < fileCount; ++fileIndex) {
        Com_sprintf(aliasPath, (int32_t)sizeof(aliasPath), "soundaliases/%s", files[fileIndex]);
        Com_LocalizeSoundAliasCsvFile(aliasPath, NULL);
        Hunk_ClearToMarkTemp();
    }

    FS_FreeFileList(files);
    COM_WriteFinalStringEdFile(localStringEdOSPath, sourceStringEdOSPath);
    (void)remove(localStringEdOSPath);
    Com_Printf("done\n");
}
