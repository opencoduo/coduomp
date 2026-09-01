/*
 * Source reconstruction for server main entry points.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recovered_game.h"
#include "g_public.h"
#include "game_globals.h"
#include "g_syscalls.h"
#include "game_functions.h"
#include "scr_vm.h"
#include "level_locals.h"

/*
 * Global syscall function pointer.
 * Set by dllEntry during module initialization.
 * Used by all trap_* functions to call into the engine.
 */
game_syscall_t g_syscall = NULL;  /* DAT_000be908 */

/* ------------------------------------------------------------------ */
/*  0x7389c  dllEntry                                                 */
/* ------------------------------------------------------------------ */

/*
 * Module entry point called by the engine during initialization.
 * Stores the syscall function pointer for later use by trap_* functions.
 */
/* VERIFIED_DECOMPILER(0x7389c, 8389c_dllEntry.c, VERIFY-SERVER-MAIN-PACKET-2026-06-17): DATAFLOW_VERIFIED - exported entry signature, syscall pointer store to DAT_000be908, and void return checked against current decompiler output. */
void dllEntry(game_syscall_t syscallPtr)
{
    g_syscall = syscallPtr;
}

/* ------------------------------------------------------------------ */
/*  Forward declarations for vmMain targets not in headers             */
/* ------------------------------------------------------------------ */

#define ENTITY_TEAM_SLAVE_FLAG 0x00000004u
#define FUNC_TRAMCAR_TEAMMASTER_SPAWNFLAG 0x00000008u
#define CVAR_NOTIFY_UNRELIABLE 0
#define UI_CVAR_BUFFER_SIZE 256
#define GAMETYPE_DEFAULT "dm"
/* INTENTIONAL_OVERRIDE (NOT_FROM_ORIGINAL_SOURCE): display-only identity for
 * the two G_InitGame console-log lines. The gamename and gamedate server-info
 * cvars retain their retail values in gameCvarTable. */
#define GAME_LOG_DISPLAY_NAME "Open CoD:United Offensive"
#define GAME_LOG_DISPLAY_DATE "Aug 20 2026"
/* rodata 0xa1100: "e \"GAME_SERVER\x15: %s \x14GAME_CHANGEDTO\x15 %s\"" -
 * the marker before GAME_CHANGEDTO is 0x14, not 0x15. */
#define CVAR_CHANGE_COMMAND "e \"GAME_SERVER\x15: %s \x14GAME_CHANGEDTO\x15 %s\""
#define INITIAL_WINNER_CONFIGSTRING_VALUE "0"
#define CG_ATMOS_DEFAULT "-1"

enum {
    SERVERINFO_BUFFER_SIZE = 1024,
    INITIAL_RESERVED_ENTITY_COUNT =
        PLAYER_CLONE_ENTITYNUM_BASE + PLAYER_CLONE_COUNT
};

/* ------------------------------------------------------------------ */
/*  0x56ac0  vmMain                                                   */
/* ------------------------------------------------------------------ */

/*
 * Main dispatch function called by the engine for all game module operations.
 * Routes commands to appropriate handler functions based on command ID.
 */
/* VERIFIED_DECOMPILER(0x56ac0, 66ac0_vmMain.c, VERIFY-SERVER-MAIN-PACKET-2026-06-17): DATAFLOW_VERIFIED - command switch cases 1..0x17, default -1, call/argument order, return values, weapon bound branch, and level/client offset accesses checked against current decompiler output. */
intptr_t vmMain(int32_t command, intptr_t arg0, intptr_t arg1, intptr_t arg2,
                intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6,
                intptr_t arg7, intptr_t arg8, intptr_t arg9, intptr_t arg10,
                intptr_t arg11)
{
    intptr_t result = 0;

    (void)arg4;
    (void)arg5;
    (void)arg6;
    (void)arg7;
    (void)arg8;
    (void)arg9;
    (void)arg10;
    (void)arg11;

    switch (command) {
    default:
        result = -1;
        break;

    case GAME_GET_API_VERSION:
        return GAME_API_VERSION;

    case GAME_INIT:
        G_InitGame((int)arg0, (int)arg1, (int)arg2, (int)arg3);
        break;

    case GAME_SHUTDOWN:
        G_ShutdownGame((int)arg0);
        break;

    case GAME_CLIENT_CONNECT:
        result = (intptr_t)ClientConnect((int)arg0, (int)(arg1 & 0xffff));
        break;

    case GAME_CLIENT_BEGIN:
        ClientBegin((int)arg0);
        break;

    case GAME_CLIENT_USERINFO_CHANGED:
        ClientUserinfoChanged((int)arg0);
        break;

    case GAME_CLIENT_DISCONNECT:
        ClientDisconnect((int)arg0);
        break;

    case GAME_CLIENT_COMMAND:
        ClientCommand((int)arg0);
        break;

    case GAME_CLIENT_THINK:
        ClientThink((int)arg0);
        break;

    case GAME_GET_FOLLOW_PLAYER_STATE:
        result = GetFollowPlayerState((int)arg0, (void *)arg1);
        break;

    case GAME_UPDATE_CVARS:
        G_UpdateCvars();
        break;

    case GAME_RUN_FRAME:
        G_RunFrame((int)arg0);
        break;

    case GAME_CONSOLE_COMMAND:
        result = ConsoleCommand();
        break;

    case GAME_SCRIPT_FAR_HOOK:
        result = (intptr_t)Scr_FarHook((script_callback_fn_t *)arg0);
        break;

    case GAME_DOBJ_CALC_POSE:
        G_DObjCalcPose(&level.gentities[(uint32_t)arg0]);
        break;

    case GAME_IS_VALID_WEAPON:
        if (arg0 < 0 || arg0 > BG_GetNumWeapons()) {
            result = 0;
        } else {
            result = 1;
        }
        break;

    case GAME_SET_MATCH_STATE:
        level.scriptExitParam = (int32_t)arg0;
        break;

    case GAME_GET_MATCH_STATE:
        result = level.scriptExitParam;
        break;

    case GAME_GET_CLIENT_STATE:
        result = (intptr_t)&level.clients[(uint32_t)arg0].clientNum;
        break;

    case GAME_GET_CLIENT_ARCHIVE_TIME:
        result = level.clients[(uint32_t)arg0].archiveTime;
        break;

    case GAME_SET_CLIENT_ARCHIVE_TIME:
        level.clients[(uint32_t)arg0].archiveTime = (int32_t)arg1;
        break;

    case GAME_GET_CLIENT_SCORE:
        /* Binary command reads gclient +0x4514, which is the score field. */
        result = level.clients[(uint32_t)arg0].score;
        break;

    case GAME_GET_FOG_OPAQUE_DIST_SQ_BITS:
        /*
         * Original case 0x17 loads the raw DWORD at level +0x2208
         * (fogOpaqueDistSq) into eax.  This return value crosses the engine
         * ABI as float bits, not as a numeric float-to-int conversion.
         */
        {
            uint32_t fogBits;

            memcpy(&fogBits, &level.fogOpaqueDistSq, sizeof(fogBits));
            result = (intptr_t)fogBits;
        }
        break;
    }

    return result;
}

/* ------------------------------------------------------------------ */
/*  0x57e4f G_FindTeams                                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x57e4f, 67e4f_G_FindTeams.c, VERIFY-SERVER-MAIN-PACKET-2026-06-17): DATAFLOW_VERIFIED - linked/team/slave gates, func_tramcar spawnflag handling, team chain/master stores, slave flag set, tramcar unlink, counters, and final print checked against current decompiler output. */
void G_FindTeams(void)
{
    int teamCount = 0;
    int entityCount = 0;
    int i;

    for (i = 1; i < level.num_entities; i++) {
        gentity_t *ent = &g_entities[i];
        uint16_t teamName = ent->teamName;
        uint32_t teamFlags = ent->flags;

        if (ent->linked == 0 ||
            teamName == 0 ||
            (teamFlags & ENTITY_TEAM_SLAVE_FLAG) != 0) {
            continue;
        }

        if (ent->scriptClassname == scr_const_func_tramcar) {
            if ((ent->spawnflags & FUNC_TRAMCAR_TEAMMASTER_SPAWNFLAG) == 0) {
                continue;
            }
            ent->teamMaster = ent;
        }

        teamCount++;
        entityCount++;

        for (int j = i + 1; j < level.num_entities; j++) {
            gentity_t *candidate = &g_entities[j];
            uint32_t candidateFlags = candidate->flags;

            if (candidate->linked == 0 ||
                candidate->teamName == 0 ||
                (candidateFlags & ENTITY_TEAM_SLAVE_FLAG) != 0 ||
                candidate->teamName != teamName) {
                continue;
            }

            entityCount++;
            candidate->teamChain = ent->teamChain;
            ent->teamChain = candidate;
            candidate->teamMaster = ent;
            candidate->flags = candidateFlags | ENTITY_TEAM_SLAVE_FLAG;

            if (candidate->scriptClassname == scr_const_func_tramcar) {
                trap_UnlinkEntity(candidate);
            }
        }
    }

    G_Printf("%i teams with %i entities\n", teamCount, entityCount);
}

/* ------------------------------------------------------------------ */
/*  0x58034 G_RegisterCvars                                           */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x58034, 68034_G_RegisterCvars.c, VERIFY-SERVER-MAIN-PACKET-2026-06-17): DATAFLOW_VERIFIED - cvar table stride, register arguments, non-null modification-count snapshot, gametype validation, fallback print/set/update, and void return checked against current decompiler output. */
void G_RegisterCvars(void)
{
    int i;

    for (i = 0; i < gameCvarTableCount; i++) {
        game_cvar_table_t *entry = &gameCvarTable[i];

        trap_Cvar_Register(entry->vmCvar, entry->cvarName,
                           entry->defaultString, entry->cvarFlags);
        if (entry->vmCvar != NULL) {
            entry->modificationCount = entry->vmCvar->modificationCount;
        }
    }

    /* NOT_FROM_ORIGINAL_SOURCE: g_debugArchiveCheck gates recovery-only
     * snapshot diagnostics. Register it outside the machine-backed 132-entry
     * gameCvarTable so that the retail table's extent and entry ordering stay
     * exact; this extra registration is the intentional behavior deviation. */
    trap_Cvar_Register(&g_debugArchiveCheck, "g_debugArchiveCheck", "0", 0);

    if (Scr_IsValidGameType(g_gametype.string) == 0) {
        G_Printf("g_gametype %s is not a valid gametype, defaulting to dm\n",
                 g_gametype.string);
        trap_Cvar_Set("g_gametype", GAMETYPE_DEFAULT);
        trap_Cvar_Update(&g_gametype);
    }
}

/* ------------------------------------------------------------------ */
/*  0x5810d G_UpdateCvars                                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5810d, 6810d_G_UpdateCvars.c, VERIFY-SERVER-MAIN-PACKET-2026-06-17): DATAFLOW_VERIFIED - cvar table loop, null vmCvar guard, update call, modification-count store-before-track branch, va arguments, and server-command arguments checked against current decompiler output. */
void G_UpdateCvars(void)
{
    int i;

    for (i = 0; i < gameCvarTableCount; i++) {
        game_cvar_table_t *entry = &gameCvarTable[i];

        if (entry->vmCvar != NULL) {
            trap_Cvar_Update(entry->vmCvar);
            if (entry->modificationCount != entry->vmCvar->modificationCount) {
                entry->modificationCount = entry->vmCvar->modificationCount;
                if (entry->trackChange != 0) {
                    trap_SendServerCommand(
                        SERVER_COMMAND_ALL_CLIENTS,
                        CVAR_NOTIFY_UNRELIABLE,
                        va(CVAR_CHANGE_COMMAND, entry->cvarName,
                           entry->vmCvar->string));
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x581cd G_SetUICvars                                              */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x581cd, 681cd_G_SetUICvars.c, VERIFY-SERVER-MAIN-PACKET-2026-06-17): DATAFLOW_VERIFIED - 256-byte stack local, 0x100 string-buffer bounds, source/target cvar sequence, and void return checked against current decompiler output. */
void G_SetUICvars(void)
{
    char value[UI_CVAR_BUFFER_SIZE];

    trap_Cvar_VariableStringBuffer("g_allowvote", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_allowvote", value);
    trap_Cvar_VariableStringBuffer("g_allowVoteMapRestart", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_allowVoteMapRestart", value);
    trap_Cvar_VariableStringBuffer("g_allowVoteMapRotate", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_allowVoteMapRotate", value);
    trap_Cvar_VariableStringBuffer("g_allowVoteTypeMap", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_allowVoteTypeMap", value);
    trap_Cvar_VariableStringBuffer("g_allowVoteMap", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_allowVoteMap", value);
    trap_Cvar_VariableStringBuffer("g_allowVoteGameType", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_allowVoteGameType", value);
    trap_Cvar_VariableStringBuffer("g_allowVoteKick", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_allowVoteKick", value);
    trap_Cvar_VariableStringBuffer("g_allowVoteClientKick", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_allowVoteClientKick", value);
    trap_Cvar_VariableStringBuffer("g_allowVoteTempBanUser", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_allowVoteTempBanUser", value);
    trap_Cvar_VariableStringBuffer("g_allowVoteTempBanClient", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_allowVoteTempBanClient", value);
    trap_Cvar_VariableStringBuffer("g_allowVoteDrawFriend", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_allowVoteDrawFriend", value);
    trap_Cvar_VariableStringBuffer("g_allowVoteKillCam", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_allowVoteKillCam", value);
    trap_Cvar_VariableStringBuffer("g_allowVoteFriendlyFire", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_allowVoteFriendlyFire", value);
    trap_Cvar_VariableStringBuffer("scr_drawfriend", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_drawfriend", value);
    trap_Cvar_VariableStringBuffer("scr_friendlyfire", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_friendlyfire", value);
    trap_Cvar_VariableStringBuffer("scr_killcam", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_killcam", value);
    trap_Cvar_VariableStringBuffer("g_timeoutsAllowed", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_timeoutsAllowed", value);
    trap_Cvar_VariableStringBuffer("g_timeoutBank", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_timeoutBank", value);
    trap_Cvar_VariableStringBuffer("g_timeoutLength", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_timeoutLength", value);
    trap_Cvar_VariableStringBuffer("g_timeoutRecovery", value, UI_CVAR_BUFFER_SIZE);
    trap_Cvar_Set("ui_timeoutRecovery", value);
}

/* ------------------------------------------------------------------ */
/*  0x5864b G_FreeEntities                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x5864b, 6864b_G_FreeEntities.c, VERIFY-SERVER-MAIN-PACKET-2026-06-17): DATAFLOW_VERIFIED - entity loop bound, 0x34c stride via typed accessor, linked-byte frees, separate world-entity linked check, and void return checked against current decompiler output. */
void G_FreeEntities(void)
{
    int i;
    gentity_t *world;

    for (i = 0; i < level.num_entities; i++) {
        gentity_t *ent = &g_entities[i];

        if (ent->linked != 0) {
            G_FreeEntity(ent);
        }
    }

    world = &g_entities[ENTITYNUM_WORLD];
    if (world->linked != 0) {
        G_FreeEntity(world);
    }
}

/* ------------------------------------------------------------------ */
/*  0x586cf  G_InitGame                                               */
/* ------------------------------------------------------------------ */

/*
 * Game initialisation.  Called from vmMain GAME_INIT.
 *
 * param_1 = level.time (ms)
 * param_2 = random seed
 * param_3 = restart flag (0 = fresh start, nonzero = map restart)
 * param_4 = cvar restart gate; original registers cvars when restart is zero
 *           or this gate is zero.
 */
/* VERIFIED_DECOMPILER(0x586cf, 686cf_G_InitGame.c, VERIFY-SERVER-MAIN-PACKET-2026-06-17): DATAFLOW_VERIFIED - initialization print/seed/cvar gate, level/bg/bgs clears and stores, log open modes and messages, configstring winner clear, entity/client setup, spawn/vehicle/team/UI sequence, fog zero plus cg_atmos default -1, objective/script startup, and spawning reset checked against current decompiler output. */
void G_InitGame(int levelTimeIn, int randomSeed, int restart,
                int cvarRestartGate)
{
    char serverinfo[SERVERINFO_BUFFER_SIZE];
    char configstr[MAX_STRING_CHARS];
    int i;
    level_locals_t *lvl = &level;

    Swap_Init();
    G_Printf("------- Game Initialization -------\n");
    G_Printf("gamename: %s\n", GAME_LOG_DISPLAY_NAME);
    G_Printf("gamedate: %s\n", GAME_LOG_DISPLAY_DATE);

    srand((uint32_t)randomSeed);
    Rand_Init(trap_Milliseconds());

    Scr_ParseGameTypeList();

    /* Decompiler confirms cvar registration runs when restart == 0 or arg3 == 0. */
    if (restart == 0 || cvarRestartGate == 0) {
        G_RegisterCvars();
    }
    G_ProcessIPBans();
    G_SetPlayerSize();

    /* Clear all game state */
    memset(&level, 0, sizeof(level));
    lvl->cachedTagMatrixObject = MAX_GENTITIES - 1;
    memset(&bg, 0, sizeof(bg_t));
    memset(&bgs, 0, sizeof(bgs));

    /* level.spawning = 1 during init */
    lvl->spawning = 1;

    /* level.time = levelTimeIn, level.startTime = levelTimeIn */
    lvl->time = levelTimeIn;
    lvl->startTime = levelTimeIn;

    /* Cache g_timeoutBank.integer into level */
    lvl->timeoutCache1 = g_timeoutBank.integer;
    lvl->timeoutCache2 = g_timeoutBank.integer;

    game_compat_bg_set_anim_sound_callbacks(trap_Com_SoundAliasString, G_AnimScriptSound);

    /* Open log file if configured */
    if (g_log.string[0] == '\0') {
        G_Printf("Not logging to disk.\n");
    } else {
        if (g_logSync.integer == 0) {
            trap_FS_FOpenFile(g_log.string, &level.logFile, FS_APPEND);
        } else {
            trap_FS_FOpenFile(g_log.string, &level.logFile, FS_APPEND_SYNC);
        }
        if (level.logFile == 0) {
            G_Printf("WARNING: Couldn't open logfile: %s\n", g_log.string);
        } else {
            trap_GetServerinfo(serverinfo, sizeof(serverinfo));
            G_LogPrintf("------------------------------------------------------------\n");
            G_LogPrintf("InitGame: %s\n", serverinfo);
        }
    }

    BG_SetupWeaponInfo();
    G_ParseScrVehicleInfo();
    GScr_LoadScripts();
    GScr_LoadConsts();

    /* Clear winner field in the game-state configstring. */
    trap_GetConfigstring(CS_GAMESTATE, configstr,
                         MAX_STRING_CHARS);
    Info_SetValueForKey(configstr, "winner",
                        INITIAL_WINNER_CONFIGSTRING_VALUE);
    trap_SetConfigstring(CS_GAMESTATE, configstr);

    /* Clear entity and client arrays */
    memset(g_entities, 0, sizeof(g_entities));
    lvl->gentities = (gentity_t *)g_entities;   /* level.gentities = g_entities */

    lvl->maxclients = g_maxclients.integer;      /* level.maxclients */

    memset(g_clients, 0, sizeof(g_clients));
    lvl->clients = (gclient_t *)g_clients;       /* level.clients = g_clients */

    /* Wire each entity's client pointer to the corresponding gclient slot */
    for (i = 0; i < lvl->maxclients; i++) {
        g_entities[i].client = &lvl->clients[i];
    }

    /* level.num_entities = MAX_CLIENTS + 8 reserved player clone slots. */
    lvl->num_entities = INITIAL_RESERVED_ENTITY_COUNT;
    lvl->freeListHead = NULL;
    lvl->freeListTail = NULL;

    trap_LocateGameData(lvl->gentities, lvl->num_entities,
                        sizeof(lvl->gentities[0]), &lvl->clients[0].ps,
                        sizeof(lvl->clients[0]));

    G_ParseHitLocDmgTable();

    if (restart == 0) {
        ClearRegisteredItems();
    }

    G_InitVehiclePaths();
    G_InitScrVehicles();
    G_InitTurrets();
    G_SpawnEntitiesFromString();
    G_SetupVehiclePaths();
    G_SetupScrVehicles();
    G_FindTeams();
    G_SetUICvars();
    SaveRegisteredItems();

    G_setfog(zeroString);
    trap_Cvar_Set("cg_atmos", CG_ATMOS_DEFAULT);

    G_InitObjectives();
    G_Printf("-----------------------------------\n");

    /* Initialise script system */
    Scr_InitSystem(1, level.time);
    Scr_SetLoading(1);
    Scr_AllocGameVariable();
    Scr_LoadGameType();
    Scr_LoadLevel();
    Scr_StartupGameType();

    /* level.spawning = 0, init complete */
    lvl->spawning = 0;
}

/* ------------------------------------------------------------------ */
/*  0x58b6d  G_ShutdownGame                                           */
/* ------------------------------------------------------------------ */

/*
 * Shuts down the game module, freeing all entities, vehicles, scripts, and
 * closing log files.  If restart is nonzero, performs a restart instead of
 * full shutdown.
 */
/* VERIFIED_DECOMPILER(0x58b6d, 68b6d_G_ShutdownGame.c, VERIFY-SERVER-MAIN-PACKET-2026-06-17): DATAFLOW_VERIFIED - shutdown/restart print branches, log-file close path, cleanup call order, script-active branch, script-exit dependent frees, and final weapon-info free checked against current decompiler output. */
void G_ShutdownGame(int restart)
{
    level_locals_t *lvl = &level;

    if (restart == 0) {
        G_Printf("==== ShutdownGame ====\n");
    } else {
        G_Printf("==== RestartGame ====\n");
    }

    if (lvl->logFile != 0) {
        if (restart == 0) {
            G_LogPrintf("ShutdownGame:\n");
        } else {
            G_LogPrintf("RestartGame:\n");
        }
        G_LogPrintf("------------------------------------------------------------\n");
        trap_FS_FCloseFile(lvl->logFile);
    }

    G_FreeEntities();
    HudElem_DestroyAll();
    G_FreeScrVehicles();
    G_FreeVehiclePaths();

    if (Scr_IsSystemActive(1) != 0) {
        if (lvl->scriptExitParam == 0) {
            trap_FreeClientScriptPers();
        }
        Scr_SetString(&level.cachedTagName, 0);
        Scr_FreeGameVariable(lvl->scriptExitParam == 0);
        Scr_ShutdownSystem(1);
    }

    GScr_FreeScripts();
    Scr_FreeScripts(1);
    trap_FreeWeaponInfoMemory(0);
}

/* ------------------------------------------------------------------ */
/*  0x591e3  G_LogPrintf                                              */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x591e3, 691e3_G_LogPrintf.c, VERIFY-SERVER-MAIN-PACKET-2026-06-17): DATAFLOW_VERIFIED - log-file guard, varargs vsprintf, minute/second timestamp math, 0x400 Com_sprintf bound with 1024-byte stack line, strlen length, and FS write arguments checked against current decompiler output. */
void G_LogPrintf(const char *format, ...)
{
    char message[MAX_STRING_CHARS];
    char line[MAX_STRING_CHARS];
    va_list args;
    int seconds;
    level_locals_t *lvl = &level;

    if (lvl->logFile == 0) {
        return;
    }

    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted log record to its fixed
     * destination. */
    (void)vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    seconds = (lvl->time / 1000) % 60;
    Com_sprintf(line, MAX_STRING_CHARS, "%3i:%i%i %s",
                (lvl->time / 1000) / 60, seconds / 10, seconds % 10,
                message);
    trap_FS_Write(line, (int)strlen(line), lvl->logFile);
}
