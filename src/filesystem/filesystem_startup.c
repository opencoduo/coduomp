#include "filesystem.h"
#include "filesystem_services.h"

#include "qcommon/com_startup_commands.h"
#include "qcommon/com_sprintf.h"
#include "qcommon/q_command.h"
#include "qcommon/q_string.h"

#include "client/common/client_video_settings.h"
#include <stdlib.h>
#include <string.h>



enum {
    FILESYSTEM_COMPAT_ASPECT_MODE_UNAVAILABLE = -1
};

/* NOT_FROM_ORIGINAL_SOURCE: snapshot the user's presentation preference
 * before an fs_game transition exposes the destination mod's archived
 * configuration. r_aspectMode is a compatibility setting rather than a
 * retail per-mod cvar, so downloaded mods must not stage an old letterboxed
 * value for the next cgame registration. Preserve a pending graphics-menu
 * selection when one exists. */
static int32_t filesystem_compat_saved_aspect_mode(void)
{
    const cvar_t *const aspectMode = Cvar_FindVar("r_aspectMode");

    if (aspectMode == NULL)
        return FILESYSTEM_COMPAT_ASPECT_MODE_UNAVAILABLE;

    const int32_t selectedMode = aspectMode->latchedString != NULL
        ? atoi(aspectMode->latchedString) : aspectMode->integer;

    if (selectedMode == CODUOMP_ASPECT_MODE_WIDESCREEN || selectedMode == CODUOMP_ASPECT_MODE_FIT_SCREEN)
        return selectedMode;
    return CODUOMP_ASPECT_MODE_LETTERBOX;
}

/* NOT_FROM_ORIGINAL_SOURCE: run immediately after the destination mod's
 * uoconfig_mp.cfg. If that file tried to replace r_aspectMode, restoring the
 * already-active value cancels its latch before a later CG_RegisterCvars can
 * apply it. The normal archived-config writer then heals the stale mod copy. */
static void filesystem_compat_queue_saved_aspect_mode(int32_t aspectMode)
{
    char command[64];

    if (aspectMode == FILESYSTEM_COMPAT_ASPECT_MODE_UNAVAILABLE)
        return;

    Com_sprintf(command, sizeof(command), "set r_aspectMode %d\n", aspectMode);
    Cbuf_AddText(command);
}

/* NOT_FROM_ORIGINAL_SOURCE: common implementation for the original restart
 * entry point and the client stream-preserving compatibility entry point. */
static void coduomp_FS_RestartInternal(int32_t checksumFeed,
                                       int32_t preservedHandle);

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the two identical
 * basegame/fs_game directory-addition blocks in FS_Startup. */
static void filesystem_compat_add_startup_game_directories(
    const char *gameName)
{
    if (fs_cdpath->string[0] != '\0')
        FS_AddLocalizedGameDirectory(fs_cdpath->string, gameName);
    if (fs_basepath->string[0] != '\0')
        FS_AddLocalizedGameDirectory(fs_basepath->string, gameName);
    if (fs_homepath->string[0] != '\0' &&
        Q_stricmp(fs_homepath->string,
                  fs_basepath->string) != 0) {
        FS_AddLocalizedGameDirectory(fs_homepath->string, gameName);
    }
    filesystem_compat_add_server_game_directory(gameName);
}

/* Source: CoDUOMP.exe 0x00430800..0x00430b95.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00430800_00430b96.mcode.
 * Name and argument role: exact same-module Mac symbol FS_Startup. Unlike the
 * Linux server build, the Windows client uses its current working directory
 * as the initial fs_basepath value. */
void FS_Startup(const char *gameName)
{
    const char *defaultHomePath;

    Com_Printf("----- FS_Startup -----\n");
    fs_packFiles = 0;

    fs_debug = Cvar_Get(
        "fs_debug", "0", CVAR_NONE);
    fs_copyfiles = Cvar_Get(
        "fs_copyfiles", "0", CVAR_INIT);
    fs_cdpath = Cvar_Get(
        "fs_cdpath", Sys_DefaultCDPath(), CVAR_INIT);
    fs_basepath = Cvar_Get(
        "fs_basepath", Sys_DefaultBasePath(), CVAR_INIT);
    fs_basegame = Cvar_Get(
        "fs_basegame", "uo", CVAR_INIT);

    defaultHomePath = Sys_DefaultHomePath();
    fs_homepath = Cvar_Get(
        "fs_homepath",
        defaultHomePath != NULL ? defaultHomePath : fs_basepath->string,
        CVAR_INIT);
    fs_game = Cvar_Get(
        "fs_game", "",
        CVAR_SYSTEMINFO | CVAR_INIT);
    fs_restrict = Cvar_Get(
        "fs_restrict", "", CVAR_INIT);
    fs_ignoreLocalized = Cvar_Get(
        "fs_ignoreLocalized", "0",
        CVAR_LATCH | CVAR_CHEAT);

    if (fs_cdpath->string[0] != '\0')
        FS_AddLocalizedGameDirectory(
            fs_cdpath->string, gameName);
    if (fs_basepath->string[0] != '\0')
        FS_AddLocalizedGameDirectory(
            fs_basepath->string, gameName);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (fs_basepath->string[0] != '\0' &&
        Q_stricmp(fs_homepath->string,
                  fs_basepath->string) != 0) {
        FS_AddLocalizedGameDirectory(
            fs_homepath->string, gameName);
    }
    filesystem_compat_add_server_game_directory(gameName);

    if (fs_basegame->string[0] != '\0' &&
        gameName != NULL &&
        Q_stricmp(gameName, "main") == 0 &&
        Q_stricmp(fs_basegame->string, gameName) != 0) {
        filesystem_compat_add_startup_game_directories(fs_basegame->string);
    }

    if (fs_game->string[0] != '\0' &&
        gameName != NULL &&
        Q_stricmp(gameName, "main") == 0 &&
        Q_stricmp(fs_game->string, gameName) != 0) {
        filesystem_compat_add_startup_game_directories(fs_game->string);
    }

    FS_AddNonPackFileDirectory("xanim", "");
    FS_AddNonPackFileDirectory("xmodel", "");
    FS_AddNonPackFileDirectory("xmodelparts", "");
    FS_AddNonPackFileDirectory("xmodelsurfs", "");
    FS_AddNonPackFileDirectory("weapons", "");
    FS_AddNonPackFileDirectory("animtrees", ".atr");

    filesystem_compat_read_cd_key();
    if (fs_game->string[0] != '\0')
        filesystem_compat_append_cd_key(fs_game->string);
    FS_AddCommands();
    FS_DisplayPath(qtrue);

    fs_game->modified = qfalse;

    Com_Printf("----------------------\n");
    Com_Printf("%d files in pk3 files\n", fs_packFiles);
}

/* NOT_FROM_ORIGINAL_SOURCE: the standalone dedicated server has no Windows
 * registry or client archive to select cl_language. A non-English install
 * keeps default_mp.cfg inside its localized paks, so when the operator did
 * not pass an explicit +set cl_language, adopt the installed localized
 * language that carries the default config instead of failing startup. An
 * explicit command-line language still wins. */
static qboolean coduomp_FS_languageWasSpecified;

static qboolean coduomp_FS_LoadDefaultConfig(void)
{
    if (FS_ReadFile("default_mp.cfg", NULL) > 0)
        return qtrue;

    if (cl_language == NULL || coduomp_FS_languageWasSpecified != qfalse ||
        fs_ignoreLocalized->integer != 0) {
        return qfalse;
    }

    const int32_t savedLanguage = cl_language->integer;
    for (int32_t language = filesystem_compat_language_count() - 1;
         language >= 0; --language) {
        if (language == savedLanguage ||
            FS_LanguageHasAssets(language) == qfalse) {
            continue;
        }

        Cvar_SetValue("cl_language", (float)language);
        if (FS_ReadFile("default_mp.cfg", NULL) > 0) {
            Com_Printf(
                "No default_mp.cfg in the selected language; using %s "
                "assets\n",
                filesystem_compat_language_name(language));
            return qtrue;
        }
    }

    Cvar_SetValue("cl_language", (float)savedLanguage);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00430bf0..0x00430ce1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00430bf0_00430ce2.mcode.
 * Name: exact same-module Mac symbol FS_InitFilesystem. */
void FS_InitFilesystem(void)
{
    Com_StartupVariable("fs_cdpath");
    Com_StartupVariable("fs_basepath");
    Com_StartupVariable("fs_homepath");
    Com_StartupVariable("fs_game");
    Com_StartupVariable("fs_copyfiles");
    Com_StartupVariable("fs_restrict");
    Com_StartupVariable("fs_usewolf");
    Com_StartupVariable("cl_language");

    const cvar_t *const language = Cvar_FindVar("cl_language");
    coduomp_FS_languageWasSpecified =
        language != NULL &&
        (language->flags & CVAR_USER_CREATED) != 0 ? qtrue : qfalse;

    filesystem_compat_init_language();
    FS_Startup("main");
    filesystem_compat_clear_localized_strings();
    filesystem_compat_update_language_info();
    FS_CheckRestrictedDemoPaks();

    if (coduomp_FS_LoadDefaultConfig() == qfalse) {
        Com_Error(
            ERR_FATAL,
            "Couldn't load %s.  Make sure Call of Duty is run from the "
            "correct folder.",
            "default_mp.cfg");
    }

    Q_strncpyz(fs_savedBasePath, fs_basepath->string,
               sizeof(fs_savedBasePath));
    Q_strncpyz(fs_savedGame, fs_game->string,
               sizeof(fs_savedGame));
    memset(fs_gameDirVar, 0, sizeof(fs_gameDirVar));
}

/* Source: CoDUOMP.exe 0x00430cf0..0x00430e40.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00430cf0_00430e41.mcode.
 * Name and checksum-feed argument: exact same-module Mac symbol FS_Restart.
 * The recovery path restores the last accepted filesystem cvars before its
 * recursive retry. */
void FS_Restart(int32_t checksumFeed)
{
    coduomp_FS_RestartInternal(checksumFeed, 0);
}

/* ORIGINAL_BINARY_BUG: FS_Restart closes every ordinary filesystem handle,
 * including a live demo stream whose client state remains active.
 * NOT_FROM_ORIGINAL_SOURCE: preserve one caller-owned stream across the same
 * filesystem rebuild performed by FS_Restart. */
void coduomp_FS_RestartPreservingFile(int32_t checksumFeed,
                                      int32_t preservedHandle)
{
    coduomp_FS_RestartInternal(checksumFeed, preservedHandle);
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve a caller-owned stream and the client's
 * presentation preferences around the original filesystem restart flow. */
static void coduomp_FS_RestartInternal(int32_t checksumFeed,
                                       int32_t preservedHandle)
{
    const int32_t savedAspectMode =
        filesystem_compat_saved_aspect_mode();

    coduomp_FS_ShutdownPreservingFile(qfalse, preservedHandle);
    fs_checksumFeed = checksumFeed;
    FS_ClearPakReferences(qfalse);
    FS_Startup("main");
    filesystem_compat_clear_localized_strings();
    filesystem_compat_update_language_info();
    FS_CheckRestrictedDemoPaks();

    if (FS_ReadFile("default_mp.cfg", NULL) <= 0) {
        if (fs_savedBasePath[0] != '\0') {
            FS_PureServerSetLoadedPaks("", "");
            (void)Cvar_Set2(
                "fs_basepath", fs_savedBasePath, qtrue);
            (void)Cvar_Set2(
                "fs_gamedirvar", fs_savedGame, qtrue);
            fs_savedBasePath[0] = '\0';
            fs_savedGame[0] = '\0';
            (void)Cvar_Set2("fs_restrict", "0", qtrue);
            coduomp_FS_RestartInternal(checksumFeed, preservedHandle);
            Com_Error(ERR_DROP, "Invalid game folder\n");
        }

        Com_Error(
            ERR_FATAL,
            "Couldn't load %s.  Make sure Call of Duty is run from the "
            "correct folder.",
            "default_mp.cfg");
    }

    if (Q_stricmp(fs_game->string, fs_savedGame) != 0 &&
        Com_SafeMode() == qfalse) {
        Cbuf_AddText(va("exec %s\n", "uoconfig_mp.cfg"));
        filesystem_compat_queue_saved_aspect_mode(savedAspectMode);
    }

    Q_strncpyz(fs_savedBasePath, fs_basepath->string,
               sizeof(fs_savedBasePath));
    Q_strncpyz(fs_savedGame, fs_game->string,
               sizeof(fs_savedGame));
}


/* Source: CoDUOMP.exe 0x00430e50..0x00430e83.
 * Evidence: repaired executable-gap boundary and exact same-module Mac symbol
 * FS_ConditionalRestart. A running local server owns its filesystem state;
 * otherwise a changed game directory or checksum feed triggers a restart. */
qboolean FS_ConditionalRestart(int32_t checksumFeed)
{
    return coduomp_FS_ConditionalRestartPreservingFile(checksumFeed, 0);
}

/* NOT_FROM_ORIGINAL_SOURCE: conditional form of the stream-preserving
 * restart; the decision remains the original FS_ConditionalRestart test. */
qboolean coduomp_FS_ConditionalRestartPreservingFile(
    int32_t checksumFeed, int32_t preservedHandle)
{
    if (sv_running->integer != 0)
        return qfalse;

    if (fs_game->modified == qfalse &&
        checksumFeed == fs_checksumFeed) {
        return qfalse;
    }

    coduomp_FS_RestartPreservingFile(checksumFeed, preservedHandle);
    return qtrue;
}
