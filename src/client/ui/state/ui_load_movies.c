#include "../module/ui_functions.h"

#include <string.h>

enum {
    UI_MOVIE_LIST_BUFFER_SIZE = 4096,
    UI_MOVIE_EXTENSION_LENGTH = 4
};

// Source: uo_ui_mp_x86.dll 0x4000b860..0x4000b942
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000b860_4000b942.mcode
// Exact same-module PPC symbol: UI_LoadMovies.
void UI_LoadMovies(void)
{
    enum { UI_MOVIE_SUFFIX_COMPARE_LIMIT = 99999 };
    char list[UI_MOVIE_LIST_BUFFER_SIZE];
    char *filename = list;
    int32_t movieIndex;

    ui_movieCount = trap_FS_GetFileList("video", "roq", list, sizeof(list));
    if (ui_movieCount > UI_MAX_MOVIES) {
        ui_movieCount = UI_MAX_MOVIES;
    }

    for (movieIndex = 0; movieIndex < ui_movieCount; ++movieIndex) {
        size_t length = strlen(filename);
        char *suffix = NULL;

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (length >= UI_MOVIE_EXTENSION_LENGTH) {
            suffix = filename + length - UI_MOVIE_EXTENSION_LENGTH;
        }
        if (suffix != NULL && Q_stricmpn(".roq", suffix, UI_MOVIE_SUFFIX_COMPARE_LIMIT) == 0) {
            *suffix = '\0';
        }
        Q_strupr(filename);
        ui_movieNames[movieIndex] = String_Alloc(filename);
        filename += length + 1;
    }
}
