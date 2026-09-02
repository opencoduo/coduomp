#include "filesystem.h"
#include "filesystem_services.h"

#include "qcommon/com_startup_commands.h"
#include "qcommon/q_command.h"
#include "qcommon/q_string.h"

#if defined(WINDOWS_BEHAVIOR)
#include <stdlib.h>
#endif
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "filesystem_startup.c requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#if defined(LINUX_BEHAVIOR)
const char *Sys_DefaultCDPath(void);
const char *Sys_DefaultHomePath(void);
const char *Sys_DefaultInstallPath(void);
void Com_ReadCDKey(void);
#endif

#if defined(WINDOWS_BEHAVIOR)
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
    if (aspectMode->latchedString != NULL)
        return atoi(aspectMode->latchedString) != 0 ? 1 : 0;
    return aspectMode->integer != 0 ? 1 : 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: run immediately after the destination mod's
 * uoconfig_mp.cfg. If that file tried to replace r_aspectMode, restoring the
 * already-active value cancels its latch before a later CG_RegisterCvars can
 * apply it. The normal archived-config writer then heals the stale mod copy. */
static void filesystem_compat_queue_saved_aspect_mode(int32_t aspectMode)
{
    if (aspectMode == FILESYSTEM_COMPAT_ASPECT_MODE_UNAVAILABLE)
        return;

    Cbuf_AddText(aspectMode != 0 ? "set r_aspectMode 1\n" : "set r_aspectMode 0\n");
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the two identical
 * basegame/fs_game directory-addition blocks in FS_Startup. */
static void filesystem_compat_add_startup_game_directories(const char *gameName)
{
    if (fs_cdpath->string[0] != '\0')
        FS_AddLocalizedGameDirectory(fs_cdpath->string, gameName);
    if (fs_basepath->string[0] != '\0')
        FS_AddLocalizedGameDirectory(fs_basepath->string, gameName);
    if (fs_homepath->string[0] != '\0' && Q_stricmp(fs_homepath->string, fs_basepath->string) != 0) {
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

    fs_debug = Cvar_Get("fs_debug", "0", CVAR_NONE);
    fs_copyfiles = Cvar_Get("fs_copyfiles", "0", CVAR_INIT);
    fs_cdpath = Cvar_Get("fs_cdpath", Sys_DefaultCDPath(), CVAR_INIT);
    fs_basepath = Cvar_Get("fs_basepath", Sys_DefaultBasePath(), CVAR_INIT);
    fs_basegame = Cvar_Get("fs_basegame", "uo", CVAR_INIT);

    defaultHomePath = Sys_DefaultHomePath();
    fs_homepath = Cvar_Get("fs_homepath", defaultHomePath != NULL ? defaultHomePath : fs_basepath->string, CVAR_INIT);
    fs_game = Cvar_Get("fs_game", "", CVAR_SYSTEMINFO | CVAR_INIT);
    fs_restrict = Cvar_Get("fs_restrict", "", CVAR_INIT);
    fs_ignoreLocalized = Cvar_Get("fs_ignoreLozalized", "0", CVAR_LATCH | CVAR_CHEAT);

    if (fs_cdpath->string[0] != '\0')
        FS_AddLocalizedGameDirectory(fs_cdpath->string, gameName);
    if (fs_basepath->string[0] != '\0')
        FS_AddLocalizedGameDirectory(fs_basepath->string, gameName);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (fs_basepath->string[0] != '\0' && Q_stricmp(fs_homepath->string, fs_basepath->string) != 0) {
        FS_AddLocalizedGameDirectory(fs_homepath->string, gameName);
    }
    filesystem_compat_add_server_game_directory(gameName);

    if (fs_basegame->string[0] != '\0' && gameName != NULL && Q_stricmp(gameName, "main") == 0 &&
        Q_stricmp(fs_basegame->string, gameName) != 0) {
        filesystem_compat_add_startup_game_directories(fs_basegame->string);
    }

    if (fs_game->string[0] != '\0' && gameName != NULL && Q_stricmp(gameName, "main") == 0 && Q_stricmp(fs_game->string, gameName) != 0) {
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

    filesystem_compat_init_language();
    FS_Startup("main");
    filesystem_compat_clear_localized_strings();
    filesystem_compat_update_language_info();
    FS_CheckRestrictedDemoPaks();

    if (FS_ReadFile("default_mp.cfg", NULL) <= 0) {
        Com_Error(ERR_FATAL,
                  "Couldn't load %s.  Make sure Call of Duty is run from the "
                  "correct folder.",
                  "default_mp.cfg");
    }

    Q_strncpyz(fs_savedBasePath, fs_basepath->string, sizeof(fs_savedBasePath));
    Q_strncpyz(fs_savedGame, fs_game->string, sizeof(fs_savedGame));
    memset(fs_gameDirVar, 0, sizeof(fs_gameDirVar));
}

/* Source: CoDUOMP.exe 0x00430cf0..0x00430e40.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00430cf0_00430e41.mcode.
 * Name and checksum-feed argument: exact same-module Mac symbol FS_Restart.
 * The recovery path restores the last accepted filesystem cvars before its
 * recursive retry. */
void FS_Restart(int32_t checksumFeed)
{
    const int32_t savedAspectMode = filesystem_compat_saved_aspect_mode();

    FS_Shutdown(qfalse);
    fs_checksumFeed = checksumFeed;
    FS_ClearPakReferences(qfalse);
    FS_Startup("main");
    filesystem_compat_clear_localized_strings();
    filesystem_compat_update_language_info();
    FS_CheckRestrictedDemoPaks();

    if (FS_ReadFile("default_mp.cfg", NULL) <= 0) {
        if (fs_savedBasePath[0] != '\0') {
            FS_PureServerSetLoadedPaks("", "");
            (void)Cvar_Set2("fs_basepath", fs_savedBasePath, qtrue);
            (void)Cvar_Set2("fs_gamedirvar", fs_savedGame, qtrue);
            fs_savedBasePath[0] = '\0';
            fs_savedGame[0] = '\0';
            (void)Cvar_Set2("fs_restrict", "0", qtrue);
            FS_Restart(checksumFeed);
            Com_Error(ERR_DROP, "Invalid game folder\n");
        }

        Com_Error(ERR_FATAL,
                  "Couldn't load %s.  Make sure Call of Duty is run from the "
                  "correct folder.",
                  "default_mp.cfg");
    }

    if (Q_stricmp(fs_game->string, fs_savedGame) != 0 && Com_SafeMode() == qfalse) {
        Cbuf_AddText(va("exec %s\n", "uoconfig_mp.cfg"));
        filesystem_compat_queue_saved_aspect_mode(savedAspectMode);
    }

    Q_strncpyz(fs_savedBasePath, fs_basepath->string, sizeof(fs_savedBasePath));
    Q_strncpyz(fs_savedGame, fs_game->string, sizeof(fs_savedGame));
}

#else

void FS_Startup(const char *gameName)
{
    const char *homePath;

    Com_Printf("----- FS_Startup -----\n");
    fs_packFiles = 0;

    fs_debug = Cvar_Get("fs_debug", "0", 0);
    fs_copyfiles = Cvar_Get("fs_copyfiles", "0", CVAR_INIT);
    fs_cdpath = Cvar_Get("fs_cdpath", Sys_DefaultCDPath(), CVAR_INIT);
    fs_basepath = Cvar_Get("fs_basepath", Sys_DefaultInstallPath(), CVAR_INIT);
    fs_basegame = Cvar_Get("fs_basegame", "uo", CVAR_INIT);

    homePath = Sys_DefaultHomePath();
    if (homePath == NULL || *homePath == '\0') {
        homePath = fs_basepath->string;
    }
    fs_homepath = Cvar_Get("fs_homepath", homePath, CVAR_INIT);

    fs_game = Cvar_Get("fs_game", "", CVAR_INIT | CVAR_SYSTEMINFO);
    fs_restrict = Cvar_Get("fs_restrict", "", CVAR_INIT);
    fs_ignoreLocalized = Cvar_Get("fs_ignoreLocalized", "0", CVAR_LATCH | CVAR_CHEAT);

    if (*fs_cdpath->string != '\0') {
        FS_AddLocalizedGameDirectory(fs_cdpath->string, gameName);
    }
    if (*fs_basepath->string != '\0') {
        FS_AddLocalizedGameDirectory(fs_basepath->string, gameName);
    }
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (*fs_basepath->string != '\0') {
        if (Q_stricmp(fs_homepath->string, fs_basepath->string) != 0) {
            FS_AddLocalizedGameDirectory(fs_homepath->string, gameName);
        }
    }

    if (*fs_basegame->string != '\0') {
        if (Q_stricmp(gameName, "main") == 0) {
            if (Q_stricmp(fs_basegame->string, gameName) != 0) {
                if (*fs_cdpath->string != '\0') {
                    FS_AddLocalizedGameDirectory(fs_cdpath->string, fs_basegame->string);
                }
                if (*fs_basepath->string != '\0') {
                    FS_AddLocalizedGameDirectory(fs_basepath->string, fs_basegame->string);
                }
                if (*fs_homepath->string != '\0') {
                    if (Q_stricmp(fs_homepath->string, fs_basepath->string) != 0) {
                        FS_AddLocalizedGameDirectory(fs_homepath->string, fs_basegame->string);
                    }
                }
            }
        }
    }

    if (*fs_game->string != '\0') {
        if (Q_stricmp(gameName, "main") == 0) {
            if (Q_stricmp(fs_game->string, gameName) != 0) {
                if (*fs_cdpath->string != '\0') {
                    FS_AddLocalizedGameDirectory(fs_cdpath->string, fs_game->string);
                }
                if (*fs_basepath->string != '\0') {
                    FS_AddLocalizedGameDirectory(fs_basepath->string, fs_game->string);
                }
                if (*fs_homepath->string != '\0') {
                    if (Q_stricmp(fs_homepath->string, fs_basepath->string) != 0) {
                        FS_AddLocalizedGameDirectory(fs_homepath->string, fs_game->string);
                    }
                }
            }
        }
    }

    FS_AddNonPackFileDirectory("xanim", "");
    FS_AddNonPackFileDirectory("xmodel", "");
    FS_AddNonPackFileDirectory("xmodelparts", "");
    FS_AddNonPackFileDirectory("xmodelsurfs", "");
    FS_AddNonPackFileDirectory("weapons", "");
    FS_AddNonPackFileDirectory("animtrees", ".atr");

    Com_ReadCDKey();
    FS_AddCommands();
    FS_Path_f();

    fs_game->modified = 0;

    Com_Printf("----------------------\n");
    Com_Printf("%d files in pk3 files\n", fs_packFiles);
}

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

    FS_Startup("main");
    FS_CheckRestrictedDemoPaks();

    if (FS_ReadFile("default_mp.cfg", NULL) < 1) {
        Com_Error(ERR_FATAL,
                  "Couldn't load %s.  Make sure Call of Duty is run from the "
                  "correct folder.",
                  "default_mp.cfg");
    }

    Q_strncpyz(fs_savedBasePath, fs_basepath->string, sizeof(fs_savedBasePath));
    Q_strncpyz(fs_savedGame, fs_game->string, sizeof(fs_savedGame));
    /* coduo_lnxded 0x08065bad clears the same 0x084843a0 object populated by
     * BSP lookup and consumed by sound-loadspec game_ matching. */
    memset(fs_gameDirVar, 0, sizeof(fs_gameDirVar));
}

void FS_Restart(int32_t checksumFeed)
{
    FS_Shutdown(qfalse);
    fs_checksumFeed = checksumFeed;
    FS_ClearPakReferences(qfalse);
    FS_Startup("main");
    FS_CheckRestrictedDemoPaks();

    if (FS_ReadFile("default_mp.cfg", NULL) < 1) {
        if (fs_savedBasePath[0] != '\0') {
            FS_PureServerSetLoadedPaks("", "");
            Cvar_Set("fs_basepath", fs_savedBasePath);
            Cvar_Set("fs_gamedirvar", fs_savedGame);
            fs_savedBasePath[0] = '\0';
            fs_savedGame[0] = '\0';
            Cvar_Set("fs_restrict", "0");
            FS_Restart(checksumFeed);
            Com_Error(ERR_DROP, "Invalid game folder\n");
        }
        Com_Error(ERR_FATAL,
                  "Couldn't load %s.  Make sure Call of Duty is run from the "
                  "correct folder.",
                  "default_mp.cfg");
    }

    if (Q_stricmp(fs_game->string, fs_savedGame) != 0) {
        if (Com_SafeMode() == 0) {
            Cbuf_AddText(va("exec %s\n", "uoconfig_mp_server.cfg"));
        }
    }

    Q_strncpyz(fs_savedBasePath, fs_basepath->string, sizeof(fs_savedBasePath));
    Q_strncpyz(fs_savedGame, fs_game->string, sizeof(fs_savedGame));
}

#endif

/* Source: CoDUOMP.exe 0x00430e50..0x00430e83.
 * Evidence: repaired executable-gap boundary and exact same-module Mac symbol
 * FS_ConditionalRestart. A running local server owns its filesystem state;
 * otherwise a changed game directory or checksum feed triggers a restart. */
qboolean FS_ConditionalRestart(int32_t checksumFeed)
{
    if (sv_running->integer != 0)
        return qfalse;

    if (fs_game->modified == qfalse && checksumFeed == fs_checksumFeed) {
        return qfalse;
    }

    FS_Restart(checksumFeed);
    return qtrue;
}
