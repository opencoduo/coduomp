#include "../module/ui_functions.h"

#include <stdio.h>
#include <string.h>

enum {
    UI_ARENA_PATH_SIZE = 128,
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    UI_ARENA_FILE_LIST_SIZE = UI_MAX_ARENA_INFOS * UI_ARENA_PATH_SIZE,
    UI_ARENA_TYPE_COMPARE_LIMIT = 99999
};

/* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
static char ui_arenaFileList[UI_ARENA_FILE_LIST_SIZE];

// Source: uo_ui_mp_x86.dll 0x400081f0..0x400085a0
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400081f0_400085a0.mcode
// Same-module PPC symbol: UI_LoadArenas.
void UI_LoadArenas(void)
{
    int32_t fileCount;
    char *filename;
    int32_t arenaIndex;

    ui_arenaInfoCount = 0;
    ui_mapCount = 0;
    fileCount = trap_FS_GetFileList("mp", ".arena", ui_arenaFileList,
                                    sizeof(ui_arenaFileList));
    filename = ui_arenaFileList;
    while (fileCount > 0) {
        char path[UI_ARENA_PATH_SIZE];
        size_t filenameLength = strlen(filename);

        /* NOT_FROM_ORIGINAL_SOURCE: skip an arena filename unless its complete
         * path and terminator fit; truncation could select another arena. */
        if (filenameLength > sizeof(path) - sizeof("mp/")) {
            trap_Print("WARNING: ignoring overlong .arena filename\n");
            filename += filenameLength + 1;
            fileCount--;
            continue;
        }
        Com_sprintf(path, sizeof(path), "mp/%s", filename);
        UI_LoadArenasFromFile(path);
        filename += filenameLength + 1;
        fileCount--;
    }

    if (outOfMemory != qfalse) {
        /* "anough" is the original retail spelling (typo) at rodata 0x40032a9c;
         * preserved byte-exact for machine-code fidelity, do not "correct". */
        trap_Print("^3WARNING: not anough memory in pool to load all arenas\n");
    }

    for (arenaIndex = 0;
         arenaIndex < ui_arenaInfoCount && ui_mapCount < UI_MAX_MAPS;
         arenaIndex++) {
        const char *info = ui_arenaInfos[arenaIndex];
        uiMapInfo_t *map = &ui_maps[ui_mapCount];
        const char *gameTypes;

        map->cinematic = -1;
        map->mapName = String_Alloc(Info_ValueForKey(info, "map"));
        map->displayName = String_Alloc(Info_ValueForKey(info, "longname"));
        map->imageShader = -1;
        map->imageName = String_Alloc(va("levelshots/%s", map->mapName));
        gameTypes = Info_ValueForKey(info, "gametype");
        if (gameTypes == NULL || *gameTypes == '\0') {
            map->typeBits = -1;
        } else {
            char *cursor = (char *)gameTypes;
            char *token;

            map->typeBits = 0;
            Com_BeginParseSession(va(".arena files : %s", map->mapName));
            for (;;) {
                int32_t gameTypeIndex;

                token = Com_Parse(&cursor);
                if (token == NULL || *token == '\0') {
                    break;
                }
                for (gameTypeIndex = 0; gameTypeIndex < ui_gameTypeCount;
                     gameTypeIndex++) {
                    if (ui_gameTypes[gameTypeIndex].gameType != NULL &&
                        Q_stricmpn(token,
                                  ui_gameTypes[gameTypeIndex].gameType,
                                  UI_ARENA_TYPE_COMPARE_LIMIT) == 0) {
                        map->typeBits = coduo_int32_from_bits(
                            (uint32_t)map->typeBits |
                            (1u << ((uint32_t)gameTypeIndex & 31u)));
                    }
                }
            }
            Com_EndParseSession();
        }
        ui_mapCount++;
    }
}
