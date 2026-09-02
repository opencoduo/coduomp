#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../module/ui_functions.h"
#include "../module/ui_globals.h"
#include "client/common/client_format_validation.h"
#include "client/common/client_legacy_crt.h"
#include "compat/crt/qsort_compat.h"

enum {
    UI_SERVER_FEEDER = 2,
    UI_FOUND_PLAYER_FEEDER = 14,
    UI_FOUND_PLAYER_RESERVED_SLOTS = 1,
    UI_LEADING_SCRIPT_COMPARE_LIMIT = 99999
};

// Source: uo_ui_mp_x86.dll 0x4000c1b0..0x4000d570
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000c1b0_4000d570.mcode
// Exact same-module PPC symbol: UI_RunMenuScript.
void UI_RunMenuScript(char **arguments)
{
    const char *command;
    const char *value;
    char serverInfo[MAX_STRING_CHARS];
    char serverAddress[MAX_STRING_CHARS];
    char favoriteName[32];
    char favoriteAddress[32];
    int32_t index;

    if (!String_Parse(arguments, &command)) {
        return;
    }
    /* A NULL command does not return: the machine (0x4000c1e8 JZ 0x4000c564)
     * skips only the leading Q_stricmpn-based compares (StartServer through
     * loadArenas) and falls into the NULL-safe Q_stricmp chain below, so it
     * ends at the "unknown UI script" Com_Printf with a NULL argument. */
    if (command == NULL) {
        goto stricmp_chain;
    }

    if (Q_stricmpn("StartServer", command, UI_LEADING_SCRIPT_COMPARE_LIMIT) == 0) {
        if (ui_dedicated.integer == 0 && coduo_fp_to_i32_extended((long double)trap_Cvar_VariableValue("sv_punkbuster")) != 0 &&
            coduo_fp_to_i32_extended((long double)trap_Cvar_VariableValue("cl_punkbuster")) == 0) {
            menuDef_t *menu = Menus_FindByName("startpb_popmenu");
            if (menu != NULL) {
                Menus_Open(menu);
            }
            return;
        }
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (ui_netGameType < 0 || ui_netGameType >= ui_gameTypeCount || ui_currentNetMap < 0 || ui_currentNetMap >= ui_mapCount) {
            Com_Printf("WARNING: StartServer has an invalid game or map selection\n");
            return;
        }
        trap_Cvar_Set("cg_thirdPerson", "0");
        trap_Cvar_SetValue("dedicated", (float)(ui_dedicated.integer < 0 ? 0 : (ui_dedicated.integer > 2 ? 2 : ui_dedicated.integer)));
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        trap_Cvar_Set("g_gametype", ui_gameTypes[ui_netGameType].gameType);
        trap_Cmd_ExecuteText(EXEC_APPEND, va("wait ; wait ; map %s\n", ui_maps[ui_currentNetMap].mapName));
        return;
    }
    if (Q_stricmpn("resetDefaults", command, UI_LEADING_SCRIPT_COMPARE_LIMIT) == 0) {
        trap_Cmd_ExecuteText(EXEC_NOW, "cvar_restart\n");
        trap_Cmd_ExecuteText(EXEC_NOW, "exec default_mp.cfg\n");
        trap_Cmd_ExecuteText(EXEC_NOW, "exec language.cfg\n");
        trap_Cmd_ExecuteText(EXEC_NOW, "setRecommended\n");
        ui_compat_controls_set_defaults();
        trap_Cvar_Set("com_introPlayed", "1");
        trap_Cvar_Set("com_recommendedSet", "1");
        trap_Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n");
        return;
    }
    if (Q_stricmpn("getCDKey", command, UI_LEADING_SCRIPT_COMPARE_LIMIT) == 0) {
        return;
    }
    if (Q_stricmpn("verifyCDKey", command, UI_LEADING_SCRIPT_COMPARE_LIMIT) == 0) {
        char key[MAX_STRING_CHARS] = "";
        char checksum[MAX_STRING_CHARS] = "";
        Q_strcat(key, sizeof(key), UI_Cvar_VariableString("cdkey1"));
        Q_strcat(key, sizeof(key), UI_Cvar_VariableString("cdkey2"));
        Q_strcat(key, sizeof(key), UI_Cvar_VariableString("cdkey3"));
        Q_strcat(key, sizeof(key), UI_Cvar_VariableString("cdkey4"));
        Q_strcat(checksum, sizeof(checksum), UI_Cvar_VariableString("cdkey5"));
        if (ui_compat_verify_cd_key(key, checksum)) {
            trap_Cvar_Set("ui_cdkeyvalid", UI_SafeTranslateString("EXE_CDKEYVALID"));
            ui_compat_set_cd_key(key, checksum);
        } else {
            trap_Cvar_Set("ui_cdkeyvalid", UI_SafeTranslateString("EXE_CDKEYINVALID"));
        }
        return;
    }
    if (Q_stricmpn("loadArenas", command, UI_LEADING_SCRIPT_COMPARE_LIMIT) == 0) {
        UI_LoadArenas();
        UI_SelectCurrentGameType();
        (void)UI_MapCountByGameType();
        Menu_SetFeederSelection(NULL, "createserver_maps", 4, 0);
        UI_SelectCurrentMap();
        return;
    }
stricmp_chain:
    if (Q_stricmp(command, "saveControls") == 0) {
        client_ui_compat_controls_set_config();
        return;
    }
    if (Q_stricmp(command, "loadControls") == 0) {
        ui_compat_controls_get_config();
        return;
    }
    if (Q_stricmp(command, "clearError") == 0) {
        trap_Cvar_Set("com_errorMessage", "");
        return;
    }
    if (Q_stricmp(command, "loadGameInfo") == 0) {
        UI_GetGameTypesList();
        return;
    }
    if (Q_stricmp(command, "resetScores") == 0) {
        return;
    }
    if (Q_stricmp(command, "RefreshServers") == 0 || Q_stricmp(command, "RefreshFilter") == 0) {
        UI_StartServerRefresh(Q_stricmp(command, "RefreshServers") == 0);
        UI_BuildServerDisplayList(1);
        return;
    }
    if (Q_stricmp(command, "RunSPDemo") == 0) {
        if (ui_displayContextStorage.demoAvailable) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (ui_currentMap < 0 || ui_currentMap >= ui_mapCount || ui_gameType < 0 || ui_gameType >= ui_gameTypeCount) {
                Com_Printf("WARNING: RunSPDemo has an invalid game or map selection\n");
                return;
            }
            trap_Cmd_ExecuteText(EXEC_APPEND, va("demo %s_%s\n", ui_maps[ui_currentMap].mapName, ui_gameTypes[ui_gameType].gameType));
        }
        return;
    }
    if (Q_stricmp(command, "LoadDemos") == 0) {
        UI_LoadDemos();
        return;
    }
    if (Q_stricmp(command, "LoadMovies") == 0) {
        UI_LoadMovies();
        return;
    }
    if (Q_stricmp(command, "LoadMods") == 0) {
        UI_LoadMods();
        return;
    }
    if (Q_stricmp(command, "playMovie") == 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (ui_movieIndex < 0 || ui_movieIndex >= ui_movieCount) {
            Com_Printf("WARNING: playMovie has an invalid movie selection\n");
            return;
        }
        if (ui_previewMovie >= 0) {
            trap_CIN_StopCinematic(ui_previewMovie);
        }
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        trap_Cmd_ExecuteText(EXEC_APPEND, va("cinematic %s.roq 2\n", ui_movieNames[ui_movieIndex]));
        return;
    }
    if (Q_stricmp(command, "RunMod") == 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (ui_modIndex < 0 || ui_modIndex >= ui_modCount) {
            Com_Printf("WARNING: RunMod has an invalid mod selection\n");
            return;
        }
        if (ui_mods[ui_modIndex].directory != NULL && ui_mods[ui_modIndex].directory[0] != '\0') {
            trap_Cvar_Set("fs_game", ui_mods[ui_modIndex].directory);
            trap_Cmd_ExecuteText(EXEC_APPEND, "vid_restart;");
        }
        return;
    }
    if (Q_stricmp(command, "RunDemo") == 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (ui_demoIndex < 0 || ui_demoIndex >= ui_demoCount) {
            Com_Printf("WARNING: RunDemo has an invalid demo selection\n");
            return;
        }
        trap_Cmd_ExecuteText(EXEC_APPEND, va("demo %s\n", ui_demoNames[ui_demoIndex]));
        return;
    }
    if (Q_stricmp(command, "Quake3") == 0) {
        trap_Cvar_Set("fs_game", "");
        trap_Cmd_ExecuteText(EXEC_APPEND, "vid_restart;");
        return;
    }
    if (Q_stricmp(command, "closeJoin") == 0) {
        if (ui_serverRefreshActive) {
            UI_StopServerRefresh();
            ui_nextDisplayRefresh = 0;
            ui_serverStatusNextRefresh = 0;
            ui_findPlayerNextRefresh = 0;
            UI_BuildServerDisplayList(1);
        } else {
            Menus_CloseByName("joinserver");
            Menus_OpenByName("main");
        }
        return;
    }
    if (Q_stricmp(command, "StopRefresh") == 0) {
        UI_StopServerRefresh();
        ui_nextDisplayRefresh = 0;
        ui_serverStatusNextRefresh = 0;
        ui_findPlayerNextRefresh = 0;
        return;
    }
    if (Q_stricmp(command, "UpdateFilter") == 0) {
        if (ui_netSource == LAN_SERVER_SOURCE_LOCAL) {
            UI_StartServerRefresh(qtrue);
        }
        UI_BuildServerDisplayList(1);
        UI_FeederSelection(2.0f, 0);
        return;
    }
    if (Q_stricmp(command, "ServerStatus") == 0) {
        UI_UpdateDisplayServers();
        if (ui_currentServer >= 0 && ui_currentServer < ui_displayServerCount) {
            ui_compat_lan_get_server_address(ui_netSource, ui_displayServers[ui_currentServer], ui_serverStatusAddress,
                                             sizeof(ui_serverStatusAddress));
            UI_BuildServerStatus(qtrue);
        }
        return;
    }
    if (Q_stricmp(command, "FoundPlayerServerStatus") == 0) {
        /* The retail producer and every reader use a deliberate one-based
         * result layout: slot 0 is reserved and selection index zero reads
         * the first populated record at +0x40. */
        Q_strncpyz(ui_serverStatusAddress, ui_foundPlayerServerAddresses[ui_foundPlayerServerIndex + UI_FOUND_PLAYER_RESERVED_SLOTS],
                   sizeof(ui_serverStatusAddress));
        UI_BuildServerStatus(qtrue);
        Menu_SetFeederSelection(NULL, NULL, UI_FOUND_PLAYER_FEEDER, 0);
        return;
    }
    if (Q_stricmp(command, "FindPlayer") == 0) {
        UI_BuildFindPlayerList(qtrue);
        ui_serverStatusInfo.numLines = 0;
        Menu_SetFeederSelection(NULL, NULL, UI_FOUND_PLAYER_FEEDER, 0);
        return;
    }
    if (Q_stricmp(command, "JoinServer") == 0) {
        trap_Cvar_Set("cg_thirdPerson", "0");
        UI_UpdateDisplayServers();
        if (ui_currentServer < 0 || ui_currentServer >= ui_displayServerCount) {
            return;
        }
        index = ui_displayServers[ui_currentServer];
        if (ui_compat_lan_server_is_punkbuster(ui_netSource, index) &&
            coduo_fp_to_i32_extended((long double)trap_Cvar_VariableValue("cl_punkbuster")) == 0) {
            Menus_OpenByName("joinpb_popmenu");
            return;
        }
        ui_compat_lan_get_server_address(ui_netSource, index, serverAddress, sizeof(serverAddress));
        trap_Cmd_ExecuteText(EXEC_APPEND, va("connect %s\n", serverAddress));
        return;
    }
    if (Q_stricmp(command, "FoundPlayerJoinServer") == 0) {
        if (ui_foundPlayerServerIndex >= 0 && ui_foundPlayerServerIndex < ui_foundPlayerServerCount) {
            /* Reads the reserved-slot-0 scheme described above. */
            trap_Cmd_ExecuteText(
                EXEC_APPEND, va("connect %s\n", ui_foundPlayerServerAddresses[ui_foundPlayerServerIndex + UI_FOUND_PLAYER_RESERVED_SLOTS]));
        }
        return;
    }
    if (Q_stricmp(command, "Quit") == 0) {
        trap_Cmd_ExecuteText(EXEC_NOW, "quit");
        return;
    }
    if (Q_stricmp(command, "Controls") == 0) {
        trap_Cvar_Set("cl_paused", "1");
        trap_Key_SetCatcher(KEYCATCH_UI);
        Menus_CloseAll();
        Menus_OpenByName("setup_menu2");
        return;
    }
    if (Q_stricmp(command, "Leave") == 0) {
        trap_Cmd_ExecuteText(EXEC_APPEND, "disconnect\n");
        trap_Key_SetCatcher(KEYCATCH_UI);
        Menus_CloseAll();
        Menus_OpenByName("main");
        return;
    }
    if (Q_stricmp(command, "ServerSort") == 0) {
        if (Int_Parse(arguments, &index)) {
            if (index == ui_serverSortKey) {
                ui_serverSortDirection = !ui_serverSortDirection;
            }
            ui_serverSortKey = index;
            coduo_crt_qsort(ui_displayServers, (size_t)(uint32_t)ui_displayServerCount, sizeof(ui_displayServers[0]),
                            UI_ServersQsortCompare);
        }
        return;
    }
    if (Q_stricmp(command, "nextSkirmish") == 0 || Q_stricmp(command, "SkirmishStart") == 0) {
        return;
    }
    if (Q_stricmp(command, "voteTypeMap") == 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (ui_netGameType < 0 || ui_netGameType >= ui_gameTypeCount || ui_currentNetMap < 0 || ui_currentNetMap >= ui_mapCount) {
            Com_Printf("WARNING: voteTypeMap has an invalid game or map selection\n");
            return;
        }
        trap_Cmd_ExecuteText(EXEC_APPEND,
                             va("callvote typemap %s %s\n", ui_gameTypes[ui_netGameType].gameType, ui_maps[ui_currentNetMap].mapName));
        return;
    }
    if (Q_stricmp(command, "voteMap") == 0) {
        if (ui_currentNetMap >= 0 && ui_currentNetMap < ui_mapCount) {
            trap_Cmd_ExecuteText(EXEC_APPEND, va("callvote map %s\n", ui_maps[ui_currentNetMap].mapName));
        }
        return;
    }
    if (Q_stricmp(command, "voteKick") == 0 || Q_stricmp(command, "voteTempBan") == 0) {
        if (ui_playerIndex >= 0 && ui_playerIndex < ui_playerCount) {
            trap_Cmd_ExecuteText(EXEC_APPEND,
                                 va(Q_stricmp(command, "voteKick") == 0 ? "callvote kick \"%s\"\n" : "callvote tempBanUser \"%s\"\n",
                                    ui_playerNames[ui_playerIndex]));
        }
        return;
    }
    if (Q_stricmp(command, "voteGame") == 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (ui_netGameType < 0 || ui_netGameType >= ui_gameTypeCount) {
            Com_Printf("WARNING: voteGame has an invalid game selection\n");
            return;
        }
        trap_Cmd_ExecuteText(EXEC_APPEND, va("callvote g_gametype %s\n", ui_gameTypes[ui_netGameType].gameType));
        return;
    }
    if (Q_stricmp(command, "addFavorite") == 0) {
        if (ui_netSource == LAN_SERVER_SOURCE_FAVORITES) {
            return;
        }
        favoriteName[0] = '\0';
        favoriteAddress[0] = '\0';
        UI_UpdateDisplayServers();
        if (ui_currentServer >= 0 && ui_currentServer < ui_displayServerCount) {
            ui_compat_lan_get_server_info(ui_netSource, ui_displayServers[ui_currentServer], serverInfo, sizeof(serverInfo));
            Q_strncpyz(favoriteName, Info_ValueForKey(serverInfo, "hostname"), sizeof(favoriteName));
            Q_strncpyz(favoriteAddress, Info_ValueForKey(serverInfo, "addr"), sizeof(favoriteAddress));
        }
        UI_AddServerToFavoritesList(favoriteName, favoriteAddress);
        return;
    }
    if (Q_stricmp(command, "deleteFavorite") == 0) {
        if (ui_netSource == LAN_SERVER_SOURCE_FAVORITES && ui_currentServer >= 0 && ui_currentServer < ui_displayServerCount) {
            UI_UpdateDisplayServers();
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (ui_currentServer < 0 || ui_currentServer >= ui_displayServerCount) {
                return;
            }
            ui_compat_lan_get_server_info(ui_netSource, ui_displayServers[ui_currentServer], serverInfo, sizeof(serverInfo));
            Q_strncpyz(favoriteAddress, Info_ValueForKey(serverInfo, "addr"), sizeof(favoriteAddress));
            if (favoriteAddress[0] != '\0') {
                ui_compat_lan_remove_server(LAN_SERVER_SOURCE_FAVORITES, favoriteAddress);
            }
        }
        return;
    }
    if (Q_stricmp(command, "createFavorite") == 0) {
        if (ui_netSource != LAN_SERVER_SOURCE_FAVORITES) {
            return;
        }
        Q_strncpyz(favoriteName, UI_Cvar_VariableString("ui_favoriteName"), sizeof(favoriteName));
        Q_strncpyz(favoriteAddress, UI_Cvar_VariableString("ui_favoriteAddress"), sizeof(favoriteAddress));
        UI_AddServerToFavoritesList(favoriteName, favoriteAddress);
        return;
    }
    if (Q_stricmp(command, "orders") == 0) {
        if (!String_Parse(arguments, &value)) {
            return;
        }
        index = coduo_fp_to_i32_extended((long double)trap_Cvar_VariableValue("cg_selectedPlayer"));
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (index < 0) {
            Com_Printf("WARNING: orders has an invalid player selection\n");
            goto close_in_game;
        }
        if (index < ui_teamPlayerCount) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve the zero-or-one integer
             * command contract and reject other conversions. */
            if (client_compat_validate_format_signature(value, "i") == qfalse) {
                Com_Printf("WARNING: rejected invalid orders format\n");
                goto close_in_game;
            }
            Com_sprintf(serverAddress, sizeof(serverAddress), value, ui_playerNumbers[index]);
            trap_Cmd_ExecuteText(EXEC_APPEND, serverAddress);
            trap_Cmd_ExecuteText(EXEC_APPEND, "\n");
        } else {
            for (index = 0; index < ui_teamPlayerCount; ++index) {
                if (Q_stricmp(UI_Cvar_VariableString("name"), ui_teamPlayerNames[index]) == 0) {
                    continue;
                }
                /* NOT_FROM_ORIGINAL_SOURCE: this branch supplies one player
                 * name, so accept only the zero-or-one string contract. */
                if (client_compat_validate_format_signature(value, "s") == qfalse) {
                    Com_Printf("WARNING: rejected invalid orders format\n");
                    goto close_in_game;
                }
                Com_sprintf(serverAddress, sizeof(serverAddress), value, ui_teamPlayerNames[index]);
                trap_Cmd_ExecuteText(EXEC_APPEND, serverAddress);
                trap_Cmd_ExecuteText(EXEC_APPEND, "\n");
            }
        }
        goto close_in_game;
    }
    if (Q_stricmp(command, "voiceOrdersTeam") == 0) {
        if (!String_Parse(arguments, &value)) {
            return;
        }
        index = coduo_fp_to_i32_extended((long double)trap_Cvar_VariableValue("cg_selectedPlayer"));
        if (index == ui_teamPlayerCount) {
            trap_Cmd_ExecuteText(EXEC_APPEND, value);
            trap_Cmd_ExecuteText(EXEC_APPEND, "\n");
        }
        goto close_in_game;
    }
    if (Q_stricmp(command, "voiceOrders") == 0) {
        if (!String_Parse(arguments, &value)) {
            return;
        }
        index = coduo_fp_to_i32_extended((long double)trap_Cvar_VariableValue("cg_selectedPlayer"));
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (index < 0) {
            Com_Printf("WARNING: voiceOrders has an invalid player selection\n");
            goto close_in_game;
        }
        if (index < ui_teamPlayerCount) {
            /* NOT_FROM_ORIGINAL_SOURCE: enforce the one-client-number template
             * contract before formatting the command. */
            if (client_compat_validate_format_signature(value, "i") == qfalse) {
                Com_Printf("WARNING: rejected invalid voiceOrders format\n");
                goto close_in_game;
            }
            Com_sprintf(serverAddress, sizeof(serverAddress), value, ui_playerNumbers[index]);
            trap_Cmd_ExecuteText(EXEC_APPEND, serverAddress);
            trap_Cmd_ExecuteText(EXEC_APPEND, "\n");
        }
        goto close_in_game;
    }
    if (Q_stricmp(command, "update") == 0) {
        if (String_Parse(arguments, &value)) {
            UI_Update(value);
        }
        return;
    }
    if (Q_stricmp(command, "showSpecScores") == 0) {
        if (coduo_crt_atoi(UI_Cvar_VariableString("ui_isSpectator")) != 0) {
            trap_Cmd_ExecuteText(EXEC_APPEND, "+scores\n");
        }
        return;
    }
    if (Q_stricmp(command, "setPbClStatus") == 0) {
        if (Int_Parse(arguments, &index)) {
            ui_compat_set_pb_client_status(index);
        }
        return;
    }
    if (Q_stricmp(command, "startSingleplayer") == 0) {
        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): this multiplayer
         * replacement has no single-player executable to launch. */
        // trap_Cmd_ExecuteText(EXEC_APPEND, "startSingleplayer\n");
        return;
    }
    if (Q_stricmp(command, "getLanguage") == 0) {
        char language[8] = "";
        Q_strncpyz(language, UI_Cvar_VariableString("cl_language"), sizeof(language));
        trap_Cvar_Set("ui_language", language);
        UI_VerifyLanguage();
        return;
    }
    if (Q_stricmp(command, "verifyLanguage") == 0) {
        UI_VerifyLanguage();
        return;
    }
    if (Q_stricmp(command, "updateLanguage") == 0) {
        char language[8] = "";
        Q_strncpyz(language, UI_Cvar_VariableString("ui_language"), sizeof(language));
        trap_Cvar_Set("cl_language", language);
        UI_VerifyLanguage();
        trap_Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n");
        return;
    }
    if (Q_stricmp(command, "closeingame") == 0) {
    close_in_game:
        trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
        trap_Key_ClearStates();
        trap_Cvar_Set("cl_paused", "0");
        Menus_CloseAll();
        return;
    }

    Com_Printf("unknown UI script %s\n", command);
}
