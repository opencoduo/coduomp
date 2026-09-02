/*
 * Source reconstruction for client command helper functions.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "compat/coduo_ctype_compat.h"
#include "recovered_game.h"
#include "game_globals.h"
#include "level_locals.h"
#include "scr_vm.h"
#include "g_syscalls.h"
#include "game_functions.h"

#define COMMAND_SEND_UNRELIABLE 0
#define COMMAND_SEND_RELIABLE 1
#define FOG_CONFIGSTRING 13
#define COLOR_ESCAPE_BYTE 27
#define GIVE_ALL_AMMO_AMOUNT 998
#define ITEM_AUTO_TOUCH_ENABLED 1
#define ITEM_TOUCH_TRACE_MODE 2
#define ENTITY_FLAG_GODMODE 0x00000001u
#define ENTITY_FLAG_NOTARGET 0x00000002u
#define SUICIDE_DAMAGE 100000
#define DEFAULT_KILLCAM_SECONDS 20
#define KILLCAM_MS_PER_SECOND 1000
#define CLIENT_ARCHIVE_NONE -1
#define PS_FLAGS_STOPFOLLOW_CLEAR 0x00106000u
#define STOPFOLLOW_TRACE_MASK 0x810011
#define STOPFOLLOW_VIEW_PITCH_OFFSET 15.0f
#define STOPFOLLOW_BACK_OFFSET -40.0f
#define STOPFOLLOW_UP_OFFSET 10.0f
#define STOPFOLLOW_CAPSULE_EXTENT 8.0f
#define GAMECOMMAND_MODE 2
#define ENTITY_FLAG_VEHICLE_OCCUPANT 0x00100000u
#define FLAME_DAMAGE_ARG_BUFFER_SIZE 4
#define MENU_RESPONSE_CONFIGSTRING_BASE CS_SCRIPTMENUS
#define MENU_RESPONSE_CONFIGSTRING_COUNT CS_SCRIPTMENUS_COUNT
#define SAY_TEXT_COPY_SIZE 150
#define SAY_LOCATION_BUFFER_SIZE 64
#define SAY_NAME_BUFFER_SIZE 64
#define SAY_PREFIX_BUFFER_SIZE 128
#define SAY_MODE_ALL 0
#define SAY_MODE_TEAM 1
#define SAY_MODE_TELL 2
#define SAY_MODE_SQUAD 3
#define SAY_COMMAND_TEAM "i"
#define SAY_COMMAND_ALL "h"
#define TARGET_LOCATION_INITIAL_BEST_DIST_SQ 201326592.0f
#define TARGET_LOCATION_COLOR_MIN 0
#define TARGET_LOCATION_COLOR_MAX 7
#define TARGET_LOCATION_COLOR_BASE '0'
#define TEAM_STATUS_TRACE_DISTANCE 8192.0f
#define TEAM_STATUS_MIN_VIEWHEIGHT 8.0f
#define TEAM_STATUS_MAX_TARGETS 64
#define TEAM_STATUS_TRACE_MASK (CONTENTS_SOLID | CONTENTS_BODY)
#define SAY_COLOR_ESCAPE '^'
#define SAY_COLOR_TELL '3'
#define SAY_COLOR_TEAM '5'
#define SAY_COLOR_VOICE_TEAM '5'
#define SAY_COLOR_VOICE_TELL '6'
#define SAY_COLOR_VOICE_ALL '2'
#define SAY_COLOR_ALL '7'
#define SAY_COLOR_AXIS "^9"
#define SAY_COLOR_ALLIES "^8"
#define SAY_COLOR_RESET "^7"
#define TELL_NAME_BUFFER_SIZE 64
#define TELL_MODE SAY_MODE_TELL
#define VOTE_ARG_BUFFER_SIZE 64
#define PS_FLAG_VOTE_CAST 0x00010000u
#define CALLVOTE_ARG_BUFFER_SIZE 256
#define CALLVOTE_CLIENT_NAME_BUFFER_SIZE 64
#define CALLVOTE_CVAR_BUFFER_SIZE 256
#define CALLVOTE_DURATION_MS 30000
/* The original game checks calledVotes against this limit, but never increments
 * calledVotes, so the limit never takes effect and players can call unlimited votes. */
#define CALLVOTE_MAX_CALLED 3
#define CALLVOTE_MAPNAME_BUFFER_SIZE 256
#define CALLVOTE_MAPNAME_CVAR_FLAGS \
    (CVAR_SERVERINFO | CVAR_ROM)
#define ACTIVATE_SCRIPT_SYSTEM 1
#define ACTIVATE_MOUNT_BLOCK_FLAG 0x6000u
#define SCOREBOARD_MAX_CLIENTS 64
#define SCOREBOARD_MESSAGE_BUFFER_SIZE 1408
#define SCOREBOARD_MESSAGE_LIMIT 1024
#define SCOREBOARD_CLIENT_CONNECTING 1


void G_SpawnItem(gentity_t *ent, gitem_t *item);
void G_VEH_CycleSlot(gentity_t *ent, int previous);
void G_CheckForCursorHints(gentity_t *ent);
void G_TryDoor(gentity_t *ent, gentity_t *other, gentity_t *activator);
gentity_t *G_IsVehicleUnusable(gentity_t *ent);
qboolean G_IsVehicleUsable(gentity_t *vehicle, gentity_t *player);
qboolean G_IsTurretUsable(gentity_t *turret, gentity_t *player);


static char concatArgsBuffer[MAX_STRING_CHARS];

/* NOT_FROM_ORIGINAL_SOURCE: helper for repeated localized status commands. */
static void game_compat_command_send_localized_status(gentity_t *ent, const char *localized)
{
    trap_SendServerCommand(
        (uint32_t)(int)(ent - g_entities),
        COMMAND_SEND_UNRELIABLE,
        va("e \"%s\"", localized));
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for direct binary literal status commands. */
static void game_compat_command_send_literal_status(gentity_t *ent, const char *message)
{
    trap_SendServerCommand(
        (uint32_t)(int)(ent - g_entities),
        COMMAND_SEND_UNRELIABLE,
        message);
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
static qboolean game_compat_command_vote_arg_is_yes(const char arg[VOTE_ARG_BUFFER_SIZE])
{
    return arg[0] == 'y' || arg[0] == 'Y' || arg[0] == '1';
}

static const char *gameCommandMessages[] = {
    "GAME_GC_HOLDYOURPOSITION",
    "GAME_GC_HOLDTHISPOSITION",
    "GAME_GC_COMEHERE",
    "GAME_GC_COVERME",
    "GAME_GC_GUARDLOCATION",
    "GAME_GC_SEARCHDESTROY",
    "GAME_GC_REPORT",
};

/* VERIFIED_DECOMPILER(0x49f58, 59f58_DeathmatchScoreboardMessage.c, VERIFY-CLIENT-COMMANDS-CHEATS-FOG-2026-06-17): DATAFLOW_VERIFIED - scoreboard count clamp, ping fallback, entry formatting, 1024-byte payload cap, team scores, and reliable send checked. */
void DeathmatchScoreboardMessage(gentity_t *ent)
{
    level_locals_t *scoreboardLevel = &level;
    char message[SCOREBOARD_MESSAGE_BUFFER_SIZE];
    char entry[MAX_STRING_CHARS];
    int count = scoreboardLevel->sortedClientCount;
    int length = 0;
    int index;

    message[0] = '\0';

    if (count > SCOREBOARD_MAX_CLIENTS) {
        count = SCOREBOARD_MAX_CLIENTS;
    }

    for (index = 0; index < count; index++) {
        int clientNum = scoreboardLevel->sortedClients[index];
        gclient_t *client = &level.clients[clientNum];
        int ping = client->connectedState == SCOREBOARD_CLIENT_CONNECTING
                       ? -1
                       : trap_GetClientPing(clientNum);
        size_t entryLength;

        Com_sprintf(entry, MAX_STRING_CHARS,
                    " %i %i %i %i %i",
                    clientNum, client->score, ping, client->deaths,
                    client->statusIcon);

        entryLength = strlen(entry);
        if ((int)(entryLength + (size_t)length) > SCOREBOARD_MESSAGE_LIMIT) {
            break;
        }

        strcpy(message + length, entry);
        length += (int)entryLength;
    }

    trap_SendServerCommand(
        (uint32_t)(int)(ent - g_entities),
        COMMAND_SEND_RELIABLE,
        va("b %i %i %i%s",
           index, scoreboardLevel->teamScoreAxis, scoreboardLevel->teamScoreAllies,
           message));
}

/* VERIFIED_DECOMPILER(0x4a172, 5a172_Cmd_Score_f.c, VERIFY-WAVE3-CLIENT-COMMANDS-STANCE-TEAM-2026-06-17): DATAFLOW_VERIFIED - single DeathmatchScoreboardMessage call and void return checked. */
void Cmd_Score_f(gentity_t *ent)
{
    DeathmatchScoreboardMessage(ent);
}

/* VERIFIED_DECOMPILER(0x4a195, 5a195_CheatsOk.c, VERIFY-CLIENT-COMMANDS-CHEATS-FOG-2026-06-17): DATAFLOW_VERIFIED - g_cheats gate, alive check, localized failures, unreliable send, and boolean return checked. */
qboolean CheatsOk(gentity_t *ent)
{
    if (g_cheats.integer == 0) {
        trap_SendServerCommand(
            (uint32_t)(int)(ent - g_entities),
            COMMAND_SEND_UNRELIABLE,
            va("e \"GAME_CHEATSNOTENABLED\""));
        return 0;
    }

    if (ent->health < 1) {
        trap_SendServerCommand(
            (uint32_t)(int)(ent - g_entities),
            COMMAND_SEND_UNRELIABLE,
            va("e \"GAME_MUSTBEALIVECOMMAND\""));
        return 0;
    }

    return 1;
}

/* VERIFIED_DECOMPILER(0x4a24d, 5a24d_ConcatArgs.c, VERIFY-CLIENT-COMMANDS-CHEATS-FOG-2026-06-17): DATAFLOW_VERIFIED - argc loop, 1024-byte argv buffer, 1022-byte append limit, inter-arg spaces, terminator, and static return checked. */
const char *ConcatArgs(int start)
{
    char arg[MAX_STRING_CHARS];
    int argc = trap_Argc();
    int length = 0;

    for (int index = start; index < argc; index++) {
        size_t argLength;

        trap_Argv(index, arg, MAX_STRING_CHARS);
        argLength = strlen(arg);

        if ((int)(argLength + (size_t)length) > MAX_STRING_CHARS - 2) {
            break;
        }

        memcpy(concatArgsBuffer + length, arg, argLength);
        length += (int)argLength;

        if (index != argc - 1) {
            concatArgsBuffer[length] = ' ';
            length++;
        }
    }

    concatArgsBuffer[length] = '\0';
    return concatArgsBuffer;
}

/* VERIFIED_DECOMPILER(0x4a32c, 5a32c_SanitizeString.c, VERIFY-CLIENT-COMMANDS-CHEATS-FOG-2026-06-17): DATAFLOW_VERIFIED - signed byte filter, color escape skip, control skip, lowercase copy, and terminator checked against decompiler and disassembly. */
void SanitizeString(const char *input, char *output)
{
    while (*input != '\0') {
        signed char value = (signed char)*input;

        if (value == COLOR_ESCAPE_BYTE) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (input[1] == '\0') {
                break;
            }
            input = &input[2];
        } else if (value < ' ') {
            input++;
        } else {
            char *destination = output;

            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            input++;
            output++;
            *destination =
                (char)tolower(coduo_ctype_signed_byte_arg((int)value));
        }
    }

    *output = '\0';
}

/* VERIFIED_DECOMPILER(0x4a391, 5a391_ClientNumberFromString.c, VERIFY-CLIENT-COMMANDS-CHEATS-FOG-2026-06-17): DATAFLOW_VERIFIED - numeric/name paths, sanitize buffers, connected-state checks, maxclient bounds, error strings, and returns checked. */
int ClientNumberFromString(gentity_t *to, const char *text)
{
    if (text[0] < '0' || text[0] > '9') {
        char cleanName[MAX_STRING_CHARS];
        char cleanInput[MAX_STRING_CHARS];
        gclient_t *client;

        SanitizeString(text, cleanInput);
        client = level.clients;
        for (int clientNum = 0; clientNum < level.maxclients;
             clientNum++, client++) {
            if (client->connectedState != CON_CONNECTED) {
                continue;
            }

            SanitizeString(client->userInfoName, cleanName);
            if (strcmp(cleanName, cleanInput) == 0) {
                return clientNum;
            }
        }

        trap_SendServerCommand(
            (uint32_t)(int)(to - g_entities),
            COMMAND_SEND_UNRELIABLE,
            va("e \"GAME_USERNOTONSERVER\x15" "%s\"", text));
        return -1;
    }

    int clientNum = atoi(text);
    if (clientNum < 0 || clientNum >= level.maxclients) {
        trap_SendServerCommand(
            (uint32_t)(int)(to - g_entities),
            COMMAND_SEND_UNRELIABLE,
            va("e \"GAME_BADCLIENTSLOT\x15" " %i\"", clientNum));
        return -1;
    }

    if (level.clients[clientNum].connectedState != CON_CONNECTED) {
        trap_SendServerCommand(
            (uint32_t)(int)(to - g_entities),
            COMMAND_SEND_UNRELIABLE,
            va("e \"GAME_CLIENTNOTACTIVE\x15" "%i\"", clientNum));
        return -1;
    }

    return clientNum;
}

/* VERIFIED_DECOMPILER(0x4a59a, 5a59a_G_setfog.c, VERIFY-CLIENT-COMMANDS-CHEATS-FOG-2026-06-17): DATAFLOW_VERIFIED - configstring 13, FLT_MAX resets, seven-float parse, third-value gate, and opaque distance stores checked. */
void G_setfog(const char *fog)
{
    float values[7];

    trap_SetConfigstring(FOG_CONFIGSTRING, fog);

    level.fogOpaqueDist = FLT_MAX;
    level.fogOpaqueDistSq = FLT_MAX;

    if (sscanf(fog, "%f %f %f %f %f %f %f",
               &values[0], &values[1], &values[2], &values[3], &values[4],
               &values[5], &values[6]) == 7 &&
        values[2] >= 1.0f) {
        level.fogOpaqueDist = (values[1] - values[0]) + values[0];
        level.fogOpaqueDistSq = level.fogOpaqueDist * level.fogOpaqueDist;
    }
}

/* VERIFIED_DECOMPILER(0x4a683, 5a683_Cmd_Fogswitch_f.c, VERIFY-CLIENT-COMMANDS-CHEATS-FOG-2026-06-17): DATAFLOW_VERIFIED - ConcatArgs(1) result and G_setfog dispatch checked. */
void Cmd_Fogswitch_f(void)
{
    G_setfog(ConcatArgs(1));
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for Cmd_Take_f ammo subtraction branches. */
static void game_compat_command_take_weapon_ammo(gclient_t *client, int weapon, int amount)
{
    int ammoIndex = BG_AmmoForWeapon(weapon);
    int clipIndex;
    int32_t negativeAmmo;

    client->ps.ammo[ammoIndex] = coduo_int32_from_bits(
        (uint32_t)client->ps.ammo[ammoIndex] - (uint32_t)amount);
    if (client->ps.ammo[BG_AmmoForWeapon(weapon)] >= 0) {
        return;
    }

    clipIndex = BG_ClipForWeapon(weapon);
    negativeAmmo = client->ps.ammo[BG_AmmoForWeapon(weapon)];
    client->ps.clips[clipIndex] = coduo_int32_from_bits(
        (uint32_t)client->ps.clips[clipIndex] + (uint32_t)negativeAmmo);
    client->ps.ammo[BG_AmmoForWeapon(weapon)] = 0;

    if (client->ps.clips[BG_ClipForWeapon(weapon)] < 0) {
        client->ps.clips[BG_ClipForWeapon(weapon)] = 0;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for Cmd_Take_f weapon/ammo clears. */
static void game_compat_command_clear_weapon_ammo(gclient_t *client, int weapon)
{
    client->ps.ammo[BG_AmmoForWeapon(weapon)] = 0;
    client->ps.clips[BG_ClipForWeapon(weapon)] = 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for Cmd_Give_f all-weapons branch. */
static void game_compat_command_give_all_weapons(gclient_t *client)
{
    level.spawning = 1;
    for (int weapon = 1; weapon <= BG_GetNumWeapons(); weapon++) {
        BG_GivePlayerWeapon(&client->ps, weapon);
    }
    level.spawning = 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for Cmd_Take_f all-weapons branch. */
static void game_compat_command_take_all_weapons(gentity_t *ent)
{
    gclient_t *client = ent->client;

    for (int weapon = 1; weapon <= BG_GetNumWeapons(); weapon++) {
        BG_TakePlayerWeapon(&client->ps, weapon);
        game_compat_command_clear_weapon_ammo(client, weapon);
    }

    if (client->ps.currentWeapon != 0) {
        client->ps.currentWeapon = 0;
        trap_SendServerCommand(
            (uint32_t)(int)(ent - g_entities),
            COMMAND_SEND_RELIABLE,
            "a 0");
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for Cmd_Give_f owned-weapon ammo loop. */
static void game_compat_command_give_owned_weapon_ammo(gentity_t *ent, int amount)
{
    for (int weapon = 1; weapon <= BG_GetNumWeapons(); weapon++) {
        if (Com_BitCheck(ent->client->ps.weaponBits, weapon)) {
            Add_Ammo(ent, weapon, amount, 1);
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for Cmd_Give_f item spawn/touch branch. */
static void game_compat_command_give_named_item(gentity_t *ent, const char *name)
{
    gitem_t *item = BG_FindItem(name);
    gentity_t *itemEnt;

    if (item == 0) {
        return;
    }

    level.spawning = 1;
    itemEnt = G_Spawn();
    itemEnt->currentOrigin[0] = ent->currentOrigin[0];
    itemEnt->currentOrigin[1] = ent->currentOrigin[1];
    itemEnt->currentOrigin[2] = ent->currentOrigin[2];
    G_SetConstString(&itemEnt->scriptClassname, item->classname);
    G_SpawnItem(itemEnt, item);
    itemEnt->activeState = ITEM_AUTO_TOUCH_ENABLED;
    Touch_Item(itemEnt, ent, ITEM_TOUCH_TRACE_MODE);
    itemEnt->activeState = 0;

    if (itemEnt->linked != 0) {
        G_FreeEntity(itemEnt);
    }

    level.spawning = 0;
}

/* VERIFIED_DECOMPILER(0x4a6af, 5a6af_Cmd_Give_f.c, VERIFY-CLIENT-COMMANDS-CHEATS-FOG-2026-06-17): DATAFLOW_VERIFIED - cheat gate, argument parsing, all/health/weapons/ammo/allammo branches, item spawn/touch path, and helper side effects checked. */
void Cmd_Give_f(gentity_t *ent)
{
    const char *amountText;
    int amount;
    const char *name;
    qboolean all;

    if (!CheatsOk(ent)) {
        return;
    }

    amountText = ConcatArgs(2);
    amount = atoi(amountText);
    name = ConcatArgs(1);
    if (name == 0 || name[0] == '\0') {
        return;
    }

    all = Q_stricmp(name, "all") == 0;

    if (all || Q_stricmpn(name, "health", 6) == 0) {
        if (amount == 0) {
            ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
        } else {
            ent->health = coduo_int32_from_bits(
                (uint32_t)ent->health + (uint32_t)amount);
        }

        if (!all) {
            return;
        }
    }

    if (all || Q_stricmp(name, "weapons") == 0) {
        game_compat_command_give_all_weapons(ent->client);
        if (!all) {
            level.spawning = 0;
            return;
        }
    }

    if (all || Q_stricmpn(name, "ammo", 4) == 0) {
        if (amount == 0) {
            game_compat_command_give_owned_weapon_ammo(ent, GIVE_ALL_AMMO_AMOUNT);
        } else if (ent->client->ps.currentWeapon != 0) {
            Add_Ammo(ent, ent->client->ps.currentWeapon, amount, 1);
        }

        if (!all) {
            return;
        }
    }

    if (Q_stricmpn(name, "allammo", 7) == 0 && amount != 0) {
        for (int weapon = 1; weapon <= BG_GetNumWeapons(); weapon++) {
            Add_Ammo(ent, weapon, amount, 1);
        }

        if (!all) {
            return;
        }
    }

    if (!all) {
        game_compat_command_give_named_item(ent, name);
    }
}

/* VERIFIED_DECOMPILER(0x4aa5a, 5aa5a_Cmd_Take_f.c, VERIFY-CLIENT-COMMANDS-CHEATS-FOG-2026-06-17): DATAFLOW_VERIFIED - cheat gate, argument parsing, health floor, weapon clears, ammo subtraction/clip clamp, allammo loop, and current-weapon command checked. */
void Cmd_Take_f(gentity_t *ent)
{
    const char *amountText;
    int amount;
    const char *name;
    qboolean all;

    if (!CheatsOk(ent)) {
        return;
    }

    amountText = ConcatArgs(2);
    amount = atoi(amountText);
    name = ConcatArgs(1);
    if (name == 0 || name[0] == '\0') {
        return;
    }

    all = Q_stricmp(name, "all") == 0;

    if (all || Q_stricmpn(name, "health", 6) == 0) {
        if (amount == 0) {
            ent->health = 1;
        } else {
            ent->health = coduo_int32_from_bits(
                (uint32_t)ent->health - (uint32_t)amount);
            if (ent->health < 1) {
                ent->health = 1;
            }
        }

        if (!all) {
            return;
        }
    }

    if (all || Q_stricmp(name, "weapons") == 0) {
        game_compat_command_take_all_weapons(ent);
        if (!all) {
            return;
        }
    }

    if (all || Q_stricmpn(name, "ammo", 4) == 0) {
        if (amount == 0) {
            for (int weapon = 1; weapon <= BG_GetNumWeapons(); weapon++) {
                game_compat_command_clear_weapon_ammo(ent->client, weapon);
            }
        } else if (ent->client->ps.currentWeapon != 0) {
            game_compat_command_take_weapon_ammo(ent->client, ent->client->ps.currentWeapon, amount);
        }

        if (!all) {
            return;
        }
    }

    if (Q_stricmpn(name, "allammo", 7) == 0 && amount != 0) {
        for (int weapon = 1; weapon <= BG_GetNumWeapons(); weapon++) {
            game_compat_command_take_weapon_ammo(ent->client, weapon, amount);
        }
    }
}

/* VERIFIED_DECOMPILER(0x4af77, 5af77_Cmd_God_f.c, VERIFY-CLIENT-COMMANDS-CHEATS-FOG-2026-06-17): DATAFLOW_VERIFIED - cheat gate, flags xor 1, status selection, and unreliable localized send checked. */
void Cmd_God_f(gentity_t *ent)
{
    const char *message;

    if (!CheatsOk(ent)) {
        return;
    }

    ent->flags ^= ENTITY_FLAG_GODMODE;
    if ((ent->flags & ENTITY_FLAG_GODMODE) == 0) {
        message = "GAME_GODMODEOFF";
    } else {
        message = "GAME_GODMODEON";
    }

    game_compat_command_send_localized_status(ent, message);
}

/* VERIFIED_DECOMPILER(0x4b016, 5b016_Cmd_Notarget_f.c, VERIFY-CLIENT-COMMANDS-CHEATS-FOG-2026-06-17): DATAFLOW_VERIFIED - cheat gate, flags xor 2, status selection, and unreliable localized send checked. */
void Cmd_Notarget_f(gentity_t *ent)
{
    const char *message;

    if (!CheatsOk(ent)) {
        return;
    }

    ent->flags ^= ENTITY_FLAG_NOTARGET;
    if ((ent->flags & ENTITY_FLAG_NOTARGET) == 0) {
        message = "GAME_NOTARGETOFF";
    } else {
        message = "GAME_NOTARGETON";
    }

    game_compat_command_send_localized_status(ent, message);
}

/* VERIFIED_DECOMPILER(0x4b0b5, 5b0b5_Cmd_Noclip_f.c, VERIFY-CLIENT-COMMANDS-CHEATS-FOG-2026-06-17): DATAFLOW_VERIFIED - cheat gate, client noclip read, toggle store, status selection, and unreliable localized send checked. */
void Cmd_Noclip_f(gentity_t *ent)
{
    const char *message;

    if (!CheatsOk(ent)) {
        return;
    }

    if (ent->client->noclip == 0) {
        message = "GAME_NOCLIPON";
    } else {
        message = "GAME_NOCLIPOFF";
    }

    ent->client->noclip = ent->client->noclip == 0;
    game_compat_command_send_localized_status(ent, message);
}

/* VERIFIED_DECOMPILER(0x4b169, 5b169_Cmd_UFO_f.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-2026-06-17): DATAFLOW_VERIFIED - cheat gate, ufo message selection, toggle store, and unreliable localized send checked. */
void Cmd_UFO_f(gentity_t *ent)
{
    const char *message;

    if (!CheatsOk(ent)) {
        return;
    }

    if (ent->client->ufo == 0) {
        message = "GAME_UFOON";
    } else {
        message = "GAME_UFOOFF";
    }

    ent->client->ufo = ent->client->ufo == 0;
    game_compat_command_send_localized_status(ent, message);
}

/* VERIFIED_DECOMPILER(0x4b21d, 5b21d_Cmd_Kill_f.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-2026-06-17): DATAFLOW_VERIFIED - playing-state gate, godmode clear, health stores, and suicide player_die arguments checked. */
void Cmd_Kill_f(gentity_t *ent)
{
    if (ent->client->sessionState != SESS_STATE_PLAYING) {
        return;
    }

    ent->flags &= ~ENTITY_FLAG_GODMODE;
    ent->health = 0;
    ent->client->ps.stats[STAT_HEALTH] = 0;
    player_die(ent, ent, ent, SUICIDE_DAMAGE, MOD_SUICIDE, 0, 0, 0);
}

/* VERIFIED_DECOMPILER(0x4b2bf, 5b2bf_StopFollowing.c, VERIFY-WAVE3-CLIENT-COMMANDS-STANCE-TEAM-2026-06-17): DATAFLOW_VERIFIED - follow/archive clears, trace capsule placement, ps/stance flag clears, view-angle update, and velocity zero stores checked. */
void StopFollowing(gentity_t *ent)
{
    gclient_t *client = ent->client;

    client->followClient = CLIENT_ARCHIVE_NONE;
    client->archiveClient = CLIENT_ARCHIVE_NONE;

    if ((client->ps.playerStateFlags & PSF_FOLLOWING) == 0) {
        return;
    }

    vec3_t angles = {
        client->ps.viewAngles[0],
        client->ps.viewAngles[1],
        client->ps.viewAngles[2],
    };
    vec3_t forward;
    vec3_t up;
    vec3_t origin;
    vec3_t end;
    vec3_t mins = {
        -STOPFOLLOW_CAPSULE_EXTENT,
        -STOPFOLLOW_CAPSULE_EXTENT,
        -STOPFOLLOW_CAPSULE_EXTENT,
    };
    vec3_t maxs = {
        STOPFOLLOW_CAPSULE_EXTENT,
        STOPFOLLOW_CAPSULE_EXTENT,
        STOPFOLLOW_CAPSULE_EXTENT,
    };
    trace_t trace;

    AngleVectors(angles, forward, 0, up);
    angles[0] += STOPFOLLOW_VIEW_PITCH_OFFSET;

    origin[0] = client->ps.psOrigin[0];
    origin[1] = client->ps.psOrigin[1];
    origin[2] = client->ps.psOrigin[2] + client->ps.viewHeightCurrent;
    G_AddLean(ent, origin);

    /* The binary rounds the back-offset point to float per axis and then
     * reloads it for the up-offset pass (fstp/fld at 0x4b3ad..0x4b40c). */
    for (int axis = 0; axis < 3; axis++) {
        end[axis] = origin[axis] + forward[axis] * STOPFOLLOW_BACK_OFFSET;
    }
    for (int axis = 0; axis < 3; axis++) {
        end[axis] = end[axis] + up[axis] * STOPFOLLOW_UP_OFFSET;
    }

    trap_TraceCapsule(&trace, origin, mins, maxs, end, ENTITYNUM_NONE,
                      STOPFOLLOW_TRACE_MASK);

    origin[0] = trace.endpos[0];
    origin[1] = trace.endpos[1];
    origin[2] = trace.endpos[2];

    client->ps.psClientNum = (int32_t)(uint32_t)(int)(ent - g_entities);
    client->ps.entityStateFlags &= ~PS_FLAGS_STOPFOLLOW_CLEAR;
    client->ps.viewLocked = 0;
    client->ps.viewLockedEntityNum = ENTITYNUM_NONE;
    client->ps.playerStateFlags &= ~(PSF_FOLLOWING | PMF_ADS);
    client->ps.adsFraction = 0;

    G_SetOrigin(ent, origin);
    client->ps.psOrigin[0] = origin[0];
    client->ps.psOrigin[1] = origin[1];
    client->ps.psOrigin[2] = origin[2];
    SetClientViewAngle(ent, angles);

    client->ps.motionState.externalVelocity[0] = 0.0f;
    client->ps.motionState.externalVelocity[1] = 0.0f;
    client->ps.motionState.externalVelocity[2] = 0.0f;
}

/* VERIFIED_DECOMPILER(0x4b589, 5b589_Cmd_FollowCycle_f.c, VERIFY-WAVE3-CLIENT-COMMANDS-STANCE-TEAM-2026-06-17): DATAFLOW_VERIFIED - direction validation, spectator/archive guards, archived-client scan, spectate-team gate, and archiveClient store checked. */
qboolean Cmd_FollowCycle_f(gentity_t *ent, int direction)
{
    playerState_t archivedPlayerState;
    clientState_t archiveMeta;
    int startClient;
    int clientNum;

    if (direction != 1 && direction != -1) {
        G_Error("Cmd_FollowCycle_f: bad dir %i", direction);
    }

    if (ent->client->sessionState != SESS_STATE_SPECTATOR ||
        ent->client->followClient >= 0) {
        return 0;
    }

    clientNum = ent->client->archiveClient;
    if (clientNum < 0) {
        clientNum = 0;
    }
    startClient = clientNum;

    do {
        clientNum += direction;
        if (clientNum >= level.maxclients) {
            clientNum = 0;
        }
        if (clientNum < 0) {
            clientNum = level.maxclients - 1;
        }

        if (trap_GetArchivedClientInfo(clientNum, &ent->client->archiveTime,
                                       &archivedPlayerState, &archiveMeta) &&
            G_ClientCanSpectateTeam(ent->client, archiveMeta.team)) {
            ent->client->archiveClient = clientNum;
            ent->client->sessionState = SESS_STATE_SPECTATOR;
            return 1;
        }
    } while (clientNum != startClient);

    return 0;
}

/* VERIFIED_DECOMPILER(0x4b70a, 5b70a_Cmd_Killcam_f.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-2026-06-17): DATAFLOW_VERIFIED - argv parse, 20-second fallback, follow/archive stores, spectator state, archive command, and true return checked. */
qboolean Cmd_Killcam_f(gentity_t *ent)
{
    char arg[64];
    int seconds;
    int32_t archiveMsec;

    trap_Argv(1, arg, sizeof(arg));
    seconds = atoi(arg);
    if (seconds < 1) {
        seconds = DEFAULT_KILLCAM_SECONDS;
    }
    archiveMsec = coduo_int32_from_bits(
        (uint32_t)seconds * (uint32_t)KILLCAM_MS_PER_SECOND);

    ent->client->followClient = ent->s.clientNum;
    ent->client->archiveClient = ent->s.clientNum;
    ent->client->archiveTime = archiveMsec;
    ent->client->sessionState = SESS_STATE_SPECTATOR;
    trap_SendServerCommand((uint32_t)ent->s.clientNum,
                           COMMAND_SEND_UNRELIABLE,
                           va("C %i", archiveMsec));
    return 1;
}

/* VERIFIED_DECOMPILER(0x4b7f0, 5b7f0_G_IsPlaying.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-2026-06-17): DATAFLOW_VERIFIED - sessionState playing predicate and qboolean return checked. */
qboolean G_IsPlaying(gentity_t *ent)
{
    return ent->client->sessionState == SESS_STATE_PLAYING;
}

/* VERIFIED_DECOMPILER(0x75bc0, 85bc0_OnSameTeam.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-2026-06-17): DATAFLOW_VERIFIED - null-client guards, TEAM_FREE rejection, same-team compare, and return values checked. */
int OnSameTeam(gentity_t *ent1, gentity_t *ent2)
{
    if (ent1->client == NULL || ent2->client == NULL) {
        return 0;
    }

    if (ent1->client->sessionTeam == TEAM_FREE) {
        return 0;
    }

    return ent1->client->sessionTeam == ent2->client->sessionTeam;
}

/* VERIFIED_DECOMPILER(0x75c37, 85c37_InSameSquad.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-2026-06-17): DATAFLOW_VERIFIED - OnSameTeam gate, no-squad rejection, int16 squad compare, and return values checked. */
int InSameSquad(gentity_t *ent1, gentity_t *ent2)
{
    int16_t squad1;

    if (OnSameTeam(ent1, ent2) == 0) {
        return 0;
    }

    squad1 = (int16_t)ent1->client->sessionSquad;
    if (squad1 == SESS_SQUAD_NONE) {
        return 0;
    }

    return squad1 == (int16_t)ent2->client->sessionSquad;
}

/* VERIFIED_DECOMPILER(0x75cbf, 85cbf_Team_GetLocation.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-2026-06-17): DATAFLOW_VERIFIED - origin copy, target-location list walk, distance-squared best update, PVS gate, and null/best return checked. */
gentity_t *Team_GetLocation(gentity_t *ent)
{
    gentity_t *bestLocation = NULL;
    float bestDistanceSquared = TARGET_LOCATION_INITIAL_BEST_DIST_SQ;
    vec3_t origin;

    origin[0] = ent->currentOrigin[0];
    origin[1] = ent->currentOrigin[1];
    origin[2] = ent->currentOrigin[2];

    for (gentity_t *location = level.targetLocationHead; location != NULL;
         location = location->targetLocationNext) {
        /* The binary keeps each axis delta in the x87 stack (recomputing it
         * for both factors) and rounds only the final sum (0x75d20..0x75d6f);
         * float delta temporaries would add roundings it does not perform. */
        float distanceSquared = (float)(
            ((long double)origin[0] -
             (long double)location->currentOrigin[0]) *
                ((long double)origin[0] -
                 (long double)location->currentOrigin[0]) +
            ((long double)origin[1] -
             (long double)location->currentOrigin[1]) *
                ((long double)origin[1] -
                 (long double)location->currentOrigin[1]) +
            ((long double)origin[2] -
             (long double)location->currentOrigin[2]) *
                ((long double)origin[2] -
                 (long double)location->currentOrigin[2]));

        /* 0x75d72..0x75d81 rejects only ordered greater-than; unordered
         * values continue to the PVS call. */
        if (!(distanceSquared > bestDistanceSquared) &&
            trap_InPVS(origin, location->currentOrigin) != qfalse) {
            bestDistanceSquared = distanceSquared;
            bestLocation = location;
        }
    }

    return bestLocation;
}

/* VERIFIED_DECOMPILER(0x75dc4, 85dc4_Team_GetLocationMsg.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-2026-06-17): DATAFLOW_VERIFIED - missing-location false return, color clamp mutation, string-id conversion, Com_sprintf formats, and true return checked. */
int Team_GetLocationMsg(gentity_t *ent, char *buffer, int bufferSize)
{
    gentity_t *location = Team_GetLocation(ent);
    int32_t *color;

    if (location == NULL) {
        return 0;
    }

    color = &location->itemCount;
    if (*color == 0) {
        const char *locationName =
            SL_ConvertToString(location->targetLocationMessage);

        Com_sprintf(buffer, bufferSize, "\x14%s\x15", locationName);
    } else {
        const char *locationName;

        if (*color < TARGET_LOCATION_COLOR_MIN) {
            *color = TARGET_LOCATION_COLOR_MIN;
        }
        if (*color > TARGET_LOCATION_COLOR_MAX) {
            *color = TARGET_LOCATION_COLOR_MAX;
        }

        locationName = SL_ConvertToString(location->targetLocationMessage);
        Com_sprintf(buffer, bufferSize, "\x15%c%c\x14%s\x15^7",
                    SAY_COLOR_ESCAPE,
                    *color + TARGET_LOCATION_COLOR_BASE,
                    locationName);
    }

    return 1;
}

/* VERIFIED_DECOMPILER(0x75ec2, 85ec2_TeamplayInfoMessage.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-2026-06-17): DATAFLOW_VERIFIED - playing/no-team clear, muzzle and spectator trace setup, trace mask, target client/team-token gate, and target stores checked. */
void TeamplayInfoMessage(gentity_t *ent)
{
    gclient_t *client = ent->client;
    vec3_t start;
    vec3_t end;
    trace_t trace;
    uint32_t targetNum;
    int32_t targetLocation;

    if (client->sessionState == SESS_STATE_PLAYING) {
        if (client->sessionTeam == TEAM_FREE) {
            client->ps.stats[STAT_IDENT_CLIENT_NUM] = -1;
            client->ps.stats[STAT_IDENT_CLIENT_HEALTH] = 0;
            return;
        }

        weapon_muzzle_t muzzle;

        CalcMuzzlePoints(ent, &muzzle);
        start[0] = muzzle.origin[0];
        start[1] = muzzle.origin[1];
        start[2] = muzzle.origin[2];
        end[0] = (float)((long double)muzzle.forward[0] *
                             TEAM_STATUS_TRACE_DISTANCE +
                         (long double)muzzle.origin[0]);
        end[1] = (float)((long double)muzzle.forward[1] *
                             TEAM_STATUS_TRACE_DISTANCE +
                         (long double)muzzle.origin[1]);
        end[2] = (float)((long double)muzzle.forward[2] *
                             TEAM_STATUS_TRACE_DISTANCE +
                         (long double)muzzle.origin[2]);
    } else {
        vec3_t forward;

        AngleVectors(client->ps.viewAngles, forward, NULL, NULL);
        CalcMuzzlePoint(ent, start);
        if (client->ps.viewHeightCurrent < TEAM_STATUS_MIN_VIEWHEIGHT) {
            start[2] = (float)(
                ((long double)TEAM_STATUS_MIN_VIEWHEIGHT -
                 (long double)client->ps.viewHeightCurrent) +
                (long double)start[2]);
        }

        end[0] = (float)((long double)forward[0] *
                             TEAM_STATUS_TRACE_DISTANCE +
                         (long double)start[0]);
        end[1] = (float)((long double)forward[1] *
                             TEAM_STATUS_TRACE_DISTANCE +
                         (long double)start[1]);
        end[2] = (float)((long double)forward[2] *
                             TEAM_STATUS_TRACE_DISTANCE +
                         (long double)start[2]);
    }

    trap_Trace(&trace, start, vec3_origin, vec3_origin, end,
               client->ps.psClientNum, TEAM_STATUS_TRACE_MASK);

    targetNum = (uint16_t)trace.entityNum;
    if (targetNum < TEAM_STATUS_MAX_TARGETS) {
        gentity_t *target = &g_entities[(int)targetNum];

        if (target->client != NULL &&
            (G_IsPlaying(ent) == 0 ||
             (uint16_t)target->teamName ==
                 (uint16_t)ent->teamName)) {
            targetLocation = target->health;
        } else {
            targetNum = UINT32_MAX;
            targetLocation = 0;
        }
    } else {
        targetNum = UINT32_MAX;
        targetLocation = 0;
    }

    client->ps.stats[STAT_IDENT_CLIENT_NUM] = (int32_t)targetNum;
    client->ps.stats[STAT_IDENT_CLIENT_HEALTH] = targetLocation;
}

/* VERIFIED_DECOMPILER(0x76164, 86164_CheckTeamStatus.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-LOCAL-2026-06-17): DATAFLOW_VERIFIED - void signature, throttle guard, team-status timestamp store, maxclients loop, linked/following gates, and TeamplayInfoMessage argument checked. */
void CheckTeamStatus(void)
{
    level_locals_t *lvl = &level;

    if (coduo_int32_from_bits((uint32_t)lvl->time -
                                    (uint32_t)lvl->teamStatusTime) <= 0) {
        return;
    }

    lvl->teamStatusTime = lvl->time;
    for (int clientNum = 0; clientNum < g_maxclients.integer; clientNum++) {
        gentity_t *ent = &g_entities[clientNum];

        if (ent->linked != 0 &&
            (ent->client->ps.playerStateFlags & PSF_FOLLOWING) == 0) {
            TeamplayInfoMessage(ent);
        }
    }

    return;
}

/* VERIFIED_DECOMPILER(0x4b80b, 5b80b_FUN_0005b80b.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-2026-06-17): DATAFLOW_VERIFIED - recipient eligibility, team/squad filters, dead-chat gate, command selection, prefix/color/message send arguments, and unreliable send checked. */
void G_SayTo(gentity_t *ent, gentity_t *other, int mode, char color,
             const char *prefix, const char *message)
{
    const char *command;

    if (other == NULL || other->linked == 0 || other->client == NULL ||
        other->client->connectedState != CON_CONNECTED) {
        return;
    }

    if (mode == SAY_MODE_TEAM && OnSameTeam(ent, other) == 0) {
        return;
    }

    if (mode == SAY_MODE_SQUAD && InSameSquad(ent, other) == 0) {
        return;
    }

    if (g_deadChat.integer == 0 &&
        G_IsPlaying(ent) == 0 &&
        G_IsPlaying(other) != 0) {
        return;
    }

    command = mode == SAY_MODE_TEAM ? SAY_COMMAND_TEAM : SAY_COMMAND_ALL;
    trap_SendServerCommand(
        (uint32_t)(int)(other - g_entities),
        COMMAND_SEND_UNRELIABLE,
        va("%s \"\x15%s%c%c%s\"",
           command, prefix, SAY_COLOR_ESCAPE, color, message));
}

/* VERIFIED_DECOMPILER(0x4b949, 5b949_G_Say.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-2026-06-17): DATAFLOW_VERIFIED - mode fallback, clean name, team/dead prefixes, log formats, location prefixes, text copy, dedicated print, fanout, and target send checked. */
void G_Say(gentity_t *ent, gentity_t *target, int mode, const char *message)
{
    char cleanName[SAY_NAME_BUFFER_SIZE];
    char location[SAY_LOCATION_BUFFER_SIZE];
    char text[SAY_TEXT_COPY_SIZE];
    char namePrefix[SAY_NAME_BUFFER_SIZE];
    char messagePrefix[SAY_PREFIX_BUFFER_SIZE];
    const char *teamColor;
    const char *teamName;
    const char *squadName;
    char color;

    if (mode == SAY_MODE_TEAM &&
        ent->client->sessionTeam != TEAM_AXIS &&
        ent->client->sessionTeam != TEAM_ALLIES) {
        mode = SAY_MODE_ALL;
    } else if (mode == SAY_MODE_SQUAD &&
               ent->client->sessionSquad != SESS_SQUAD_ALPHA &&
               ent->client->sessionSquad != SESS_SQUAD_BRAVO) {
        mode = SAY_MODE_TEAM;
    }

    Q_strncpyz(cleanName, ent->client->userInfoName, SAY_NAME_BUFFER_SIZE);
    Q_CleanStr(cleanName);

    if (ent->client->sessionTeam == TEAM_AXIS) {
        teamColor = SAY_COLOR_AXIS;
    } else if (ent->client->sessionTeam == TEAM_ALLIES) {
        teamColor = SAY_COLOR_ALLIES;
    } else {
        teamColor = "";
    }

    if (ent->client->sessionTeam == TEAM_SPECTATOR) {
        Com_sprintf(namePrefix, SAY_NAME_BUFFER_SIZE,
                    "\x15(\x14GAME_SPECTATOR\x15)");
    } else if (ent->client->sessionState == SESS_STATE_PLAYING) {
        Com_sprintf(namePrefix, SAY_NAME_BUFFER_SIZE,
                    "\x15%s", teamColor);
    } else {
        Com_sprintf(namePrefix, SAY_NAME_BUFFER_SIZE,
                    "\x15%s(\x14GAME_DEAD\x15)", teamColor);
    }

    if (mode == SAY_MODE_TEAM) {
        int senderGuid;

        teamName = ent->client->sessionTeam == TEAM_AXIS
                       ? "GAME_AXIS"
                       : "GAME_ALLIES";
        senderGuid = trap_GetGuid((uint32_t)ent->s.number);
        G_LogPrintf("sayteam;%d;%d;%s;%s\n",
                    senderGuid, ent->s.number, cleanName, message);

        if (Team_GetLocationMsg(ent, location, SAY_LOCATION_BUFFER_SIZE) == 0) {
            Com_sprintf(messagePrefix, SAY_PREFIX_BUFFER_SIZE,
                        "%s(\x14%s\x15)%s%s: ",
                        namePrefix, teamName, cleanName, SAY_COLOR_RESET);
        } else {
            Com_sprintf(messagePrefix, SAY_PREFIX_BUFFER_SIZE,
                        "%s(\x14%s\x15)%s%s (\x14%s\x15): ",
                        namePrefix, teamName, cleanName, SAY_COLOR_RESET,
                        location);
        }
        color = SAY_COLOR_TEAM;
    } else if (mode == SAY_MODE_TELL) {
        if (target == NULL || target->client == NULL ||
            target->client->sessionTeam != ent->client->sessionTeam ||
            Team_GetLocationMsg(ent, location, SAY_LOCATION_BUFFER_SIZE) == 0) {
            Com_sprintf(messagePrefix, SAY_PREFIX_BUFFER_SIZE,
                        "%s[%s]%s: ",
                        namePrefix, cleanName, SAY_COLOR_RESET);
        } else {
            Com_sprintf(messagePrefix, SAY_PREFIX_BUFFER_SIZE,
                        "%s[%s]%s (%s): ",
                        namePrefix, cleanName, SAY_COLOR_RESET, location);
        }
        color = SAY_COLOR_TELL;
    } else if (mode == SAY_MODE_SQUAD) {
        int senderGuid;

        teamName = ent->client->sessionTeam == TEAM_AXIS
                       ? "GAME_AXIS"
                       : "GAME_ALLIES";
        squadName = ent->client->sessionSquad == SESS_SQUAD_ALPHA
                        ? "PATCH_1_5_SQUADALPHA"
                        : "PATCH_1_5_SQUADBRAVO";
        senderGuid = trap_GetGuid((uint32_t)ent->s.number);
        G_LogPrintf("saysquad;%d;%d;%s;%s\n",
                    senderGuid, ent->s.number, cleanName, message);

        if (Team_GetLocationMsg(ent, location, SAY_LOCATION_BUFFER_SIZE) == 0) {
            Com_sprintf(messagePrefix, SAY_PREFIX_BUFFER_SIZE,
                        "%s(\x14%s\x15 \x14%s\x15)%s%s: ",
                        namePrefix, teamName, squadName, cleanName,
                        SAY_COLOR_RESET);
        } else {
            Com_sprintf(messagePrefix, SAY_PREFIX_BUFFER_SIZE,
                        "%s(\x14%s\x15 \x14%s\x15)%s%s (\x14%s\x15): ",
                        namePrefix, teamName, squadName, cleanName,
                        SAY_COLOR_RESET, location);
        }
        color = SAY_COLOR_TEAM;
    } else {
        int senderGuid = trap_GetGuid((uint32_t)ent->s.number);

        G_LogPrintf("say;%d;%d;%s;%s\n",
                    senderGuid, ent->s.number, cleanName, message);
        Com_sprintf(messagePrefix, SAY_PREFIX_BUFFER_SIZE,
                    "%s%s%s: ", namePrefix, cleanName, SAY_COLOR_RESET);
        color = SAY_COLOR_ALL;
    }

    Q_strncpyz(text, message, SAY_TEXT_COPY_SIZE);

    if (target == NULL) {
        if (g_dedicated.integer != 0) {
            G_Printf("%s%s\n", messagePrefix, text);
        }

        for (int clientNum = 0; clientNum < level.maxclients; clientNum++) {
            G_SayTo(ent, &g_entities[clientNum], mode, color,
                    messagePrefix, text);
        }
    } else {
        G_SayTo(ent, target, mode, color, messagePrefix, text);
    }
}

/* VERIFIED_DECOMPILER(0x4c037, 5c037_FUN_0005c037.c, VERIFY-WAVE3-CLIENT-COMMANDS-STANCE-TEAM-2026-06-17): DATAFLOW_VERIFIED - argc/arg0 early return, ConcatArgs start index, and G_Say(NULL, mode, message) dispatch checked. */
void Cmd_Say_f(gentity_t *ent, int mode, qboolean arg0)
{
    const char *message;

    if (trap_Argc() <= 1 && arg0 == qfalse) {
        return;
    }

    if (arg0 == qfalse) {
        message = ConcatArgs(1);
    } else {
        message = ConcatArgs(0);
    }

    G_Say(ent, NULL, mode, message);
}

/* VERIFIED_DECOMPILER(0x4c284, 5c284_FUN_0005c284.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-2026-06-23): DATAFLOW_VERIFIED - ConcatArgs(1), Scr_AddString, vsay notify DAT_00449f98, and void return checked. */
void Cmd_Voice_f(gentity_t *ent)
{
    Scr_AddString(ConcatArgs(1));
    Scr_Notify(ent, scr_const_vsay, 1);
}

/* VERIFIED_DECOMPILER(0x4c2da, 5c2da_FUN_0005c2da.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-2026-06-17): DATAFLOW_VERIFIED - recipient eligibility, team filter, command/color selection, voice number, sender number, rounded origin, and unreliable send checked. */
void G_VoiceTo(gentity_t *ent, gentity_t *other, int mode, const char *voice,
               int voiceNumber)
{
    const char *command;
    char color;

    if (other == NULL || other->linked == 0 || other->client == NULL ||
        other->client->connectedState != CON_CONNECTED) {
        return;
    }

    if (mode == SAY_MODE_TEAM && OnSameTeam(ent, other) == 0) {
        return;
    }

    if (mode == SAY_MODE_TEAM) {
        command = "k";
        color = SAY_COLOR_VOICE_TEAM;
    } else if (mode == SAY_MODE_TELL) {
        command = "l";
        color = SAY_COLOR_VOICE_TELL;
    } else {
        command = "j";
        color = SAY_COLOR_VOICE_ALL;
    }

    trap_SendServerCommand(
        (uint32_t)(int)(other - g_entities),
        COMMAND_SEND_UNRELIABLE,
        va("%s %d %d %d %s %i %i %i",
           command, voiceNumber, ent->s.number, color, voice,
           game_compat_int32_from_float_trunc(ent->currentOrigin[0]),
           game_compat_int32_from_float_trunc(ent->currentOrigin[1]),
           game_compat_int32_from_float_trunc(ent->currentOrigin[2])));
}

/* VERIFIED_DECOMPILER(0x4c431, 5c431_G_Voice.c, VERIFY-CLIENT-COMMANDS-FULL-AUDIT-2026-07-01): DATAFLOW_VERIFIED - empty original voice broadcast stub checked. */
void G_Voice(void)
{
}

/* VERIFIED_DECOMPILER(0x4c0a8, 5c0a8_FUN_0005c0a8.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-2026-06-17): DATAFLOW_VERIFIED - argc gate, target argv parse/bounds, target linked/client guards, ConcatArgs(2), cleaned log names, GUID order, tell log format, target send, and self-echo checked. */
void Cmd_Tell_f(gentity_t *ent)
{
    char arg[MAX_STRING_CHARS];
    char senderName[TELL_NAME_BUFFER_SIZE];
    char targetName[TELL_NAME_BUFFER_SIZE];
    const char *message;
    gentity_t *target;
    int targetClientNum;
    int senderGuid;
    int targetGuid;

    if (trap_Argc() <= 1) {
        return;
    }

    trap_Argv(1, arg, MAX_STRING_CHARS);
    targetClientNum = atoi(arg);
    if (targetClientNum < 0 || targetClientNum >= level.maxclients) {
        return;
    }

    target = &g_entities[targetClientNum];
    if (target->linked == 0 || target->client == NULL) {
        return;
    }

    message = ConcatArgs(2);

    Q_strncpyz(senderName, ent->client->userInfoName, sizeof(senderName));
    Q_CleanStr(senderName);
    Q_strncpyz(targetName, target->client->userInfoName, sizeof(targetName));
    Q_CleanStr(targetName);

    targetGuid = trap_GetGuid((uint32_t)target->s.number);
    senderGuid = trap_GetGuid((uint32_t)ent->s.number);
    G_LogPrintf("tell;%d;%d;%s;%d;%d;%s;%s\n",
                senderGuid, ent->s.number, senderName,
                targetGuid, target->s.number, targetName,
                message);

    G_Say(ent, target, TELL_MODE, message);
    G_Say(ent, ent, TELL_MODE, message);
}

/* VERIFIED_DECOMPILER(0x4c436, 5c436_Cmd_GameCommand_f.c, VERIFY-CLIENT-COMMANDS-CHAT-TEAM-2026-06-17): DATAFLOW_VERIFIED - argc gate, target/command argv parsing, bounds checks, linked target guard, game-command table lookup, target send, and self-echo checked. */
void Cmd_GameCommand_f(gentity_t *ent)
{
    char arg[MAX_STRING_CHARS];
    int clientNum;
    int commandIndex;
    gentity_t *target;

    if (trap_Argc() <= 1) {
        return;
    }

    trap_Argv(1, arg, MAX_STRING_CHARS);
    clientNum = atoi(arg);
    trap_Argv(2, arg, MAX_STRING_CHARS);
    commandIndex = atoi(arg);

    if (clientNum < 0 || clientNum >= level.maxclients ||
        commandIndex < 0 ||
        commandIndex >= (int)(sizeof(gameCommandMessages) /
                              sizeof(gameCommandMessages[0]))) {
        return;
    }

    target = &g_entities[clientNum];
    if (target->linked == 0) {
        return;
    }

    G_Say(ent, target, GAMECOMMAND_MODE, gameCommandMessages[commandIndex]);
    G_Say(ent, ent, GAMECOMMAND_MODE, gameCommandMessages[commandIndex]);
}

/* VERIFIED_DECOMPILER(0x4c56e, 5c56e_Cmd_Where_f.c, VERIFY-WAVE3-CLIENT-COMMANDS-STANCE-TEAM-2026-06-17): DATAFLOW_VERIFIED - currentOrigin vtos formatting and unreliable SendServerCommand target checked. */
void Cmd_Where_f(gentity_t *ent)
{
    trap_SendServerCommand(
        (uint32_t)(int)(ent - g_entities),
        COMMAND_SEND_UNRELIABLE,
        va("e \"\x15%s\n\"", vtos(ent->currentOrigin)));
}

/* VERIFIED_DECOMPILER(0x4dea0, 5dea0_Cmd_SetViewpos_f.c, VERIFY-CLIENT-COMMANDS-REMAINING-2026-06-17): DATAFLOW_VERIFIED - cheat/argc gates, argv parsing, zeroed pitch/roll, yaw store, TeleportPlayer arguments, and command errors checked. */
void Cmd_SetViewpos_f(gentity_t *ent)
{
    char arg[MAX_STRING_CHARS];
    vec3_t origin;
    vec3_t angles = {0.0f, 0.0f, 0.0f};

    if (g_cheats.integer == 0) {
        trap_SendServerCommand(
            (uint32_t)(int)(ent - g_entities),
            COMMAND_SEND_UNRELIABLE,
            va("e \"GAME_CHEATSNOTENABLED\""));
        return;
    }

    if (trap_Argc() != 5) {
        trap_SendServerCommand(
            (uint32_t)(int)(ent - g_entities),
            COMMAND_SEND_UNRELIABLE,
            va("e \"GAME_USAGE\x15: setviewpos x y z yaw\""));
        return;
    }

    for (int axis = 0; axis < 3; axis++) {
        trap_Argv(axis + 1, arg, MAX_STRING_CHARS);
        origin[axis] = (float)atof(arg);
    }

    trap_Argv(4, arg, MAX_STRING_CHARS);
    angles[1] = (float)atof(arg);
    TeleportPlayer(ent, origin, angles);
}

/* VERIFIED_DECOMPILER(0x4e4aa, 5e4aa_Cmd_EntityCount_f.c, VERIFY-CLIENT-COMMANDS-REMAINING-2026-06-17): DATAFLOW_VERIFIED - cheat gate, entity-count global read, print format, and void return checked. */
void Cmd_EntityCount_f(gentity_t *ent)
{
    (void)ent;

    if (g_cheats.integer != 0) {
        G_Printf("entity count = %i\n", level.num_entities);
    }
}

/* VERIFIED_DECOMPILER(0x4ed9b, 5ed9b_Cmd_NextVehSlot_f.c, VERIFY-CLIENT-COMMANDS-REMAINING-2026-06-17): DATAFLOW_VERIFIED - vehicle-occupant flag, positive-health gate, G_VEH_CycleSlot(ent, 0), and void return checked. */
void Cmd_NextVehSlot_f(gentity_t *ent)
{
    if ((ent->s.eFlags & ENTITY_FLAG_VEHICLE_OCCUPANT) != 0 &&
        ent->health > 0) {
        G_VEH_CycleSlot(ent, 0);
    }
}

/* VERIFIED_DECOMPILER(0x4ede1, 5ede1_Cmd_PrevVehSlot_f.c, VERIFY-CLIENT-COMMANDS-REMAINING-2026-06-17): DATAFLOW_VERIFIED - vehicle-occupant flag, positive-health gate, G_VEH_CycleSlot(ent, 1), and void return checked. */
void Cmd_PrevVehSlot_f(gentity_t *ent)
{
    if ((ent->s.eFlags & ENTITY_FLAG_VEHICLE_OCCUPANT) != 0 &&
        ent->health > 0) {
        G_VEH_CycleSlot(ent, 1);
    }
}

/* VERIFIED_DECOMPILER(0x4ee27, 5ee27_Cmd_TraceProfile_f.c, VERIFY-CLIENT-COMMANDS-REMAINING-2026-06-17): DATAFLOW_VERIFIED - empty void stub checked. */
void Cmd_TraceProfile_f(gentity_t *ent)
{
    (void)ent;
}

/* VERIFIED_DECOMPILER(0x4ee2c, 5ee2c_Cmd_FlameDamageClient_f.c, VERIFY-CLIENT-COMMANDS-REMAINING-2026-06-17): DATAFLOW_VERIFIED - argc gate, 4-byte argv buffer, inclusive maxclient bound, error prints, inflictor/time stores, and void return checked. */
void Cmd_FlameDamageClient_f(gentity_t *ent)
{
    char arg[FLAME_DAMAGE_ARG_BUFFER_SIZE];
    int inflictor;

    if (trap_Argc() != 2) {
        G_Printf("Cmd_FlameDamageClient_f: bad arg count\n");
        return;
    }

    trap_Argv(1, arg, FLAME_DAMAGE_ARG_BUFFER_SIZE);
    inflictor = atoi(arg);
    if (inflictor < 0 || level.maxclients < inflictor) {
        G_Printf("Cmd_FlameDamageClient_f: bad inflictor num\n");
        return;
    }

    ent->client->flameDamageInflictor = inflictor;
    ent->client->flameDamageTime = level.time;
}

/* VERIFIED_DECOMPILER(0x4ec46, 5ec46_Cmd_MenuResponse_f.c, VERIFY-CLIENT-COMMANDS-REMAINING-2026-06-17): DATAFLOW_VERIFIED - argc/serverId gate, menu configstring lookup bounds, bad fallback, Scr_AddString order, and menuresponse notify checked. */
void Cmd_MenuResponse_f(gentity_t *ent)
{
    char response[MAX_STRING_CHARS];
    char menu[MAX_STRING_CHARS];
    char arg[MAX_STRING_CHARS];
    int menuIndex = -1;

    if (trap_Argc() == 4) {
        trap_Argv(1, arg, MAX_STRING_CHARS);
        if (atoi(arg) != trap_Cvar_VariableIntegerValue("sv_serverId")) {
            return;
        }

        trap_Argv(2, menu, MAX_STRING_CHARS);
        menuIndex = atoi(menu);
        if (menuIndex >= 0 &&
            menuIndex < MENU_RESPONSE_CONFIGSTRING_COUNT) {
            trap_GetConfigstring(
                menuIndex + MENU_RESPONSE_CONFIGSTRING_BASE,
                menu,
                MAX_STRING_CHARS);
        }

        trap_Argv(3, response, MAX_STRING_CHARS);
    } else {
        menu[0] = '\0';
        strcpy(response, "bad");
    }

    Scr_AddString(response);
    Scr_AddString(menu);
    Scr_Notify(ent, scr_const_menuresponse, 2);
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for duplicated complaint pending clear. */
static void game_compat_command_clear_pending_complaint(gclient_t *client)
{
    client->pendingComplaintTime = -1;
    client->pendingComplaintClient = -1;
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for Cmd_Vote_f complaint vote branch. */
static void game_compat_command_handle_complaint_vote(gentity_t *ent, char arg[VOTE_ARG_BUFFER_SIZE])
{
    gclient_t *client = ent->client;
    gentity_t *target = &g_entities[client->pendingComplaintClient];
    gclient_t *targetClient = target->client;

    if (targetClient == 0 || targetClient->connectedState != CON_CONNECTED) {
        return;
    }

    if (targetClient->complaintDisabled != 0) {
        trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                               COMMAND_SEND_RELIABLE,
                               "m -3");
        return;
    }

    game_compat_command_clear_pending_complaint(client);
    trap_Argv(1, arg, VOTE_ARG_BUFFER_SIZE);

    if (game_compat_command_vote_arg_is_yes(arg)) {
        int warningsLeft;

        targetClient->complaintCount = coduo_int32_from_bits(
            (uint32_t)targetClient->complaintCount + UINT32_C(1));
        warningsLeft = coduo_int32_from_bits(
            (uint32_t)g_complaintlimit.integer -
            (uint32_t)targetClient->complaintCount);
        if (warningsLeft < 1 && targetClient->complaintDisabled == 0) {
            trap_DropClient((int)(targetClient - level.clients),
                            "GAME_KICKEDFROMCOMPLAINTS");
            trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                                   COMMAND_SEND_RELIABLE,
                                   "m -1");
        } else {
            trap_SendServerCommand(
                (uint32_t)targetClient->ps.psClientNum,
                COMMAND_SEND_UNRELIABLE,
                va("e \"\x15^1\x14GAME_WARNING\x15^7: "
                   "\x14GAME_COMPLAINTFILEDAGAINST\x15%d\"",
                   warningsLeft));
            trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                                   COMMAND_SEND_RELIABLE,
                                   "m -1");
        }
    } else {
        trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                               COMMAND_SEND_RELIABLE,
                               "m -2");
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
static qboolean game_compat_command_callvote_arg_hits_stock_binary_reject(const char *value)
{
    return Q_stricmp(value, "0") == 0 && Q_stricmp(value, "1") == 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for friendlyfire vote argument whitelist. */
static qboolean game_compat_command_callvote_arg_is_friendlyfire_value(const char *value)
{
    return Q_stricmp(value, "0") == 0 || Q_stricmp(value, "1") == 0 ||
           Q_stricmp(value, "2") == 0 || Q_stricmp(value, "3") == 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for kick/temp-ban target resolution. */
static int game_compat_command_callvote_client_from_arg(const char *command,
                                            const char *arg,
                                            char cleanName[CALLVOTE_CLIENT_NAME_BUFFER_SIZE])
{
    int clientNum = SCOREBOARD_MAX_CLIENTS;

    if (Q_stricmp(command, "kick") == 0 ||
        Q_stricmp(command, "tempBanUser") == 0) {
        int index;

        for (index = 0; index < SCOREBOARD_MAX_CLIENTS; index++) {
            if (level.clients[index].connectedState == CON_CONNECTED) {
                Q_strncpyz(cleanName, level.clients[index].userInfoName,
                           CALLVOTE_CLIENT_NAME_BUFFER_SIZE);
                Q_CleanStr(cleanName);
                if (Q_stricmp(cleanName, arg) == 0) {
                    clientNum = index;
                }
            }
        }
    } else {
        clientNum = atoi(arg);
        if ((clientNum == 0 && Q_stricmp(arg, "0") != 0) || clientNum < 0 ||
            clientNum >= SCOREBOARD_MAX_CLIENTS ||
            level.clients[clientNum].connectedState != CON_CONNECTED) {
            clientNum = SCOREBOARD_MAX_CLIENTS;
        } else {
            Q_strncpyz(cleanName, level.clients[clientNum].userInfoName,
                       CALLVOTE_CLIENT_NAME_BUFFER_SIZE);
            Q_CleanStr(cleanName);
        }
    }

    return clientNum;
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for callvote command allow-cvar dispatch. */
static qboolean game_compat_command_callvote_allowed_cvar(const char *command,
                                              int *allowVote)
{
    if (Q_stricmp(command, "map_restart") == 0) {
        *allowVote = g_allowVoteMapRestart.integer;
    } else if (Q_stricmp(command, "map_rotate") == 0) {
        *allowVote = g_allowVoteMapRotate.integer;
    } else if (Q_stricmp(command, "typemap") == 0) {
        *allowVote = g_allowVoteTypeMap.integer;
    } else if (Q_stricmp(command, "map") == 0) {
        *allowVote = g_allowVoteMap.integer;
    } else if (Q_stricmp(command, "g_gametype") == 0) {
        *allowVote = g_allowVoteGameType.integer;
    } else if (Q_stricmp(command, "kick") == 0) {
        *allowVote = g_allowVoteKick.integer;
    } else if (Q_stricmp(command, "clientkick") == 0) {
        *allowVote = g_allowVoteClientKick.integer;
    } else if (Q_stricmp(command, "tempBanUser") == 0) {
        *allowVote = g_allowVoteTempBanUser.integer;
    } else if (Q_stricmp(command, "tempBanClient") == 0) {
        *allowVote = g_allowVoteTempBanClient.integer;
    } else if (Q_stricmp(command, "drawfriend") == 0) {
        *allowVote = g_allowVoteDrawFriend.integer;
    } else if (Q_stricmp(command, "killcam") == 0) {
        *allowVote = g_allowVoteKillCam.integer;
    } else if (Q_stricmp(command, "friendlyfire") == 0) {
        *allowVote = g_allowVoteFriendlyFire.integer;
    } else {
        return qfalse;
    }

    return qtrue;
}

/* VERIFIED_DECOMPILER(0x4c5d0, 5c5d0_Cmd_CallVote_f.c, VERIFY-CLIENT-COMMANDS-REMAINING-2026-06-17): DATAFLOW_VERIFIED - voting gates, command allow-cvars, map/gametype/kick/cvar vote branches, configstring updates, and helper side effects checked. */
void Cmd_CallVote_f(gentity_t *ent)
{
    level_locals_t *lvl = &level;
    gclient_t *client = ent->client;
    char command[CALLVOTE_ARG_BUFFER_SIZE];
    char arg[CALLVOTE_ARG_BUFFER_SIZE];
    char currentValue[CALLVOTE_CVAR_BUFFER_SIZE];
    char cleanName[CALLVOTE_CLIENT_NAME_BUFFER_SIZE];
    int allowVote = 0;
    int index;

    if (g_allowVote.integer == 0) {
        game_compat_command_send_literal_status(ent, "e \"GAME_VOTINGNOTENABLED\"");
        return;
    }

    if (lvl->voteTime != 0) {
        game_compat_command_send_literal_status(ent, "e \"GAME_VOTEALREADYINPROGRESS\"");
        return;
    }

    /* Retail tests calledVotes against three, but the original game module
     * contains no write to that field. The shipped behavior therefore permits
     * unlimited sequential vote calls while retaining this dormant gate. */
    if (client->calledVotes >= CALLVOTE_MAX_CALLED) {
        game_compat_command_send_literal_status(ent, "e \"GAME_MAXVOTESCALLED\"");
        return;
    }

    if (client->sessionTeam == TEAM_SPECTATOR) {
        game_compat_command_send_literal_status(ent, "e \"GAME_NOSPECTATORCALLVOTE\"");
        return;
    }

    trap_Argv(1, command, CALLVOTE_ARG_BUFFER_SIZE);
    trap_Argv(2, arg, CALLVOTE_ARG_BUFFER_SIZE);
    if (strchr(command, ';') != NULL || strchr(arg, ';') != NULL) {
        game_compat_command_send_literal_status(ent, "e \"GAME_INVALIDVOTESTRING\"");
        return;
    }

    if (!game_compat_command_callvote_allowed_cvar(command, &allowVote)) {
        game_compat_command_send_literal_status(ent, "e \"GAME_INVALIDVOTESTRING\"");
        trap_SendServerCommand(
            (uint32_t)(int)(ent - g_entities),
            COMMAND_SEND_UNRELIABLE,
            "e \"GAME_VOTECOMMANDSARE\x15 map_restart, map_rotate, map "
            "<mapname>, g_gametype <typename>, typemap <typename> <mapname>, "
            "kick <player>, clientkick <clientnum>, tempBanUser <player>, "
            "tempBanClient <clientNum>, drawfriend <value>, killcam <value>, "
            "friendlyfire <value>\"");
        return;
    }

    if (allowVote == 0) {
        game_compat_command_send_literal_status(ent, "e \"GAME_VOTINGNOTENABLED\"");
        return;
    }

    if (lvl->voteExecuteTime != 0) {
        lvl->voteExecuteTime = 0;
        trap_SendConsoleCommand(GAMECOMMAND_MODE, va("%s\n", lvl->voteString));
    }

    if (Q_stricmp(command, "typemap") == 0) {
        char mapName[CALLVOTE_MAPNAME_BUFFER_SIZE];
        vmCvar_t mapnameCvar;

        if (Scr_IsValidGameType(arg) == 0) {
            game_compat_command_send_literal_status(ent, "e \"GAME_INVALIDGAMETYPE\"");
            return;
        }

        if (Q_stricmp(arg, g_gametype.string) == 0) {
            arg[0] = '\0';
        }

        trap_Argv(3, mapName, CALLVOTE_MAPNAME_BUFFER_SIZE);
        if (trap_MapExists(mapName) == 0) {
            trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                                   COMMAND_SEND_UNRELIABLE,
                                   "e \"\x15" "the server doesn't have that map\"");
            return;
        }

        trap_Cvar_Register(&mapnameCvar, "mapname", emptyString,
                           CALLVOTE_MAPNAME_CVAR_FLAGS);
        if (Q_stricmp(mapName, mapnameCvar.string) == 0) {
            mapName[0] = '\0';
        }

        if (arg[0] == '\0' && mapName[0] == '\0') {
            game_compat_command_send_literal_status(ent, "e \"GAME_TYPEMAP_NOCHANGE\"");
            return;
        }

        if (mapName[0] == '\0') {
            Com_sprintf(lvl->voteString, sizeof(lvl->voteString),
                        "g_gametype %s; map_restart", arg);
            Com_sprintf(lvl->voteDisplayString, sizeof(lvl->voteDisplayString),
                        "GAME_VOTE_GAMETYPE\x14%s",
                        Scr_GetGameTypeNameForScript(arg));
        } else {
            if (arg[0] == '\0') {
                Com_sprintf(lvl->voteString, sizeof(lvl->voteString),
                            "map %s", mapName);
            } else {
                Com_sprintf(lvl->voteString, sizeof(lvl->voteString),
                            "g_gametype %s; map %s", arg, mapName);
            }

            if (arg[0] == '\0') {
                Com_sprintf(lvl->voteDisplayString,
                            sizeof(lvl->voteDisplayString),
                            "GAME_VOTE_MAP\x15%s", mapName);
            } else {
                Com_sprintf(lvl->voteDisplayString,
                            sizeof(lvl->voteDisplayString),
                            "GAME_VOTE_GAMETYPE\x14%s\x15 - \x14GAME_VOTE_MAP\x15%s",
                            Scr_GetGameTypeNameForScript(arg), mapName);
            }
        }
    } else if (Q_stricmp(command, "g_gametype") == 0) {
        if (Scr_IsValidGameType(arg) == 0) {
            game_compat_command_send_literal_status(ent, "e \"GAME_INVALIDGAMETYPE\"");
            return;
        }
        Com_sprintf(lvl->voteString, sizeof(lvl->voteString),
                    "%s %s; map_restart", command, arg);
        Com_sprintf(lvl->voteDisplayString, sizeof(lvl->voteDisplayString),
                    "GAME_VOTE_GAMETYPE\x14%s",
                    Scr_GetGameTypeNameForScript(arg));
    } else if (Q_stricmp(command, "map_restart") == 0) {
        Com_sprintf(lvl->voteString, sizeof(lvl->voteString), "%s", command);
        Com_sprintf(lvl->voteDisplayString, sizeof(lvl->voteDisplayString),
                    "GAME_VOTE_MAPRESTART");
    } else if (Q_stricmp(command, "map_rotate") == 0) {
        Com_sprintf(lvl->voteString, sizeof(lvl->voteString), "%s", command);
        Com_sprintf(lvl->voteDisplayString, sizeof(lvl->voteDisplayString),
                    "GAME_VOTE_NEXTMAP");
    } else if (Q_stricmp(command, "map") == 0) {
        if (trap_MapExists(arg) == 0) {
            trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                                   COMMAND_SEND_UNRELIABLE,
                                   "e \"\x15" "the server doesn't have that map\"");
            return;
        }
        Com_sprintf(lvl->voteString, sizeof(lvl->voteString),
                    "%s %s", command, arg);
        Com_sprintf(lvl->voteDisplayString, sizeof(lvl->voteDisplayString),
                    "GAME_VOTE_MAP\x15%s", arg);
    } else if (Q_stricmp(command, "kick") == 0 ||
               Q_stricmp(command, "clientkick") == 0 ||
               Q_stricmp(command, "tempBanUser") == 0 ||
               Q_stricmp(command, "tempBanClient") == 0) {
        int clientNum =
            game_compat_command_callvote_client_from_arg(command, arg, cleanName);
        const char *kickCommand;

        if (clientNum == SCOREBOARD_MAX_CLIENTS) {
            game_compat_command_send_literal_status(ent, "e \"GAME_CLIENTNOTONSERVER\"");
            return;
        }

        kickCommand = (command[0] == 't' || command[0] == 'T')
                          ? "tempBanClient"
                          : "clientkick";
        Com_sprintf(lvl->voteString, sizeof(lvl->voteString),
                    "%s \"%d\"", kickCommand, clientNum);
        Com_sprintf(lvl->voteDisplayString, sizeof(lvl->voteDisplayString),
                    "GAME_VOTE_KICK\x15(%i)%s",
                    clientNum, level.clients[clientNum].userInfoName);
    } else if (Q_stricmp(command, "drawfriend") == 0) {
        if (game_compat_command_callvote_arg_hits_stock_binary_reject(arg)) {
            trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                                   COMMAND_SEND_UNRELIABLE,
                                   va("e \"PATCH_1_5_VOTE_ARG_NOTVALID\x15%s\"",
                                      arg));
            return;
        }

        trap_Cvar_VariableStringBuffer("scr_drawfriend", currentValue,
                                       CALLVOTE_CVAR_BUFFER_SIZE);
        if (Q_stricmp(arg, currentValue) == 0) {
            trap_SendServerCommand(
                (uint32_t)(int)(ent - g_entities),
                COMMAND_SEND_UNRELIABLE,
                "e \"PATCH_1_5_DRAWFRIEND_NOCHANGE\"");
            return;
        }

        Com_sprintf(lvl->voteString, sizeof(lvl->voteString),
                    "setdrawfriend %s", arg);
        Com_sprintf(lvl->voteDisplayString, sizeof(lvl->voteDisplayString),
                    "PATCH_1_5_VOTE_DRAWFRIEND\x14%s",
                    arg[0] == '1' ? "MENU_ON" : "MENU_OFF");
    } else if (Q_stricmp(command, "killcam") == 0) {
        trap_Cvar_VariableStringBuffer("scr_killcam", currentValue,
                                       CALLVOTE_CVAR_BUFFER_SIZE);
        if (game_compat_command_callvote_arg_hits_stock_binary_reject(arg)) {
            trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                                   COMMAND_SEND_UNRELIABLE,
                                   va("e \"PATCH_1_5_VOTE_ARG_NOTVALID\x15%s\"",
                                      arg));
            return;
        }

        if (Q_stricmp(arg, currentValue) == 0) {
            trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                                   COMMAND_SEND_UNRELIABLE,
                                   "e \"PATCH_1_5_KILLCAM_NOCHANGE\"");
            return;
        }

        Com_sprintf(lvl->voteString, sizeof(lvl->voteString),
                    "setkillcam %s", arg);
        Com_sprintf(lvl->voteDisplayString, sizeof(lvl->voteDisplayString),
                    "PATCH_1_5_VOTE_KILLCAM\x14%s",
                    arg[0] == '1' ? "MENU_ON" : "MENU_OFF");
    } else if (Q_stricmp(command, "friendlyfire") == 0) {
        trap_Cvar_VariableStringBuffer("scr_friendlyfire", currentValue,
                                       CALLVOTE_CVAR_BUFFER_SIZE);
        if (Q_stricmp(arg, currentValue) == 0) {
            trap_SendServerCommand(
                (uint32_t)(int)(ent - g_entities),
                COMMAND_SEND_UNRELIABLE,
                "e \"PATCH_1_5_FRIENDLYFIRE_NOCHANGE\"");
            return;
        }

        if (!game_compat_command_callvote_arg_is_friendlyfire_value(arg)) {
            trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                                   COMMAND_SEND_UNRELIABLE,
                                   va("e \"PATCH_1_5_VOTE_ARG_NOTVALID\x15%s\"",
                                      arg));
            return;
        }

        if (Q_stricmp(arg, "0") == 0) {
            Com_sprintf(lvl->voteDisplayString, sizeof(lvl->voteDisplayString),
                        "PATCH_1_5_VOTE_FRIENDLYFIRE\x14MENU_OFF");
        } else if (Q_stricmp(arg, "1") == 0) {
            Com_sprintf(lvl->voteDisplayString, sizeof(lvl->voteDisplayString),
                        "PATCH_1_5_VOTE_FRIENDLYFIRE\x14MENU_ON");
        } else if (Q_stricmp(arg, "2") == 0) {
            Com_sprintf(lvl->voteDisplayString, sizeof(lvl->voteDisplayString),
                        "PATCH_1_5_VOTE_FRIENDLYFIRE\x14MENU_REFLECT");
        } else {
            Com_sprintf(lvl->voteDisplayString, sizeof(lvl->voteDisplayString),
                        "PATCH_1_5_VOTE_FRIENDLYFIRE\x14MENU_SHARED");
        }

        Com_sprintf(lvl->voteString, sizeof(lvl->voteString),
                    "setfriendlyfire %s", arg);
    }

    trap_SendServerCommand(SERVER_COMMAND_ALL_CLIENTS,
                           COMMAND_SEND_UNRELIABLE,
                           va("e \"GAME_CALLEDAVOTE\x15%s\"",
                              client->userInfoName));
    lvl->voteTime = coduo_int32_from_bits(
        (uint32_t)trap_Milliseconds() + CALLVOTE_DURATION_MS);
    lvl->voteYes = 1;
    lvl->voteNo = 0;
    for (index = 0; index < lvl->maxclients; index++) {
        gclient_t *voteClient = &level.clients[index];
        voteClient->ps.entityStateFlags &= ~PS_FLAG_VOTE_CAST;
    }
    client->ps.entityStateFlags |= PS_FLAG_VOTE_CAST;
    trap_SetConfigstring(CS_VOTE_TIME,
                         va("%i", CALLVOTE_DURATION_MS));
    trap_SetConfigstring(CS_VOTE_STRING,
                         lvl->voteDisplayString);
    trap_SetConfigstring(CS_VOTE_YES, va("%i", lvl->voteYes));
    trap_SetConfigstring(CS_VOTE_NO, va("%i", lvl->voteNo));
}

/* VERIFIED_DECOMPILER(0x4da7a, 5da7a_Cmd_Vote_f.c, VERIFY-CLIENT-COMMANDS-REMAINING-2026-06-17): DATAFLOW_VERIFIED - complaint vote branch, pending complaint clears, vote gates, binary yes-byte test, count increments, and configstring updates checked. */
void Cmd_Vote_f(gentity_t *ent)
{
    char arg[VOTE_ARG_BUFFER_SIZE];
    gclient_t *client = ent->client;
    const int32_t complaintTimeRemaining = coduo_int32_from_bits(
        (uint32_t)client->pendingComplaintTime - (uint32_t)level.time);

    /* NOT_FROM_ORIGINAL_SOURCE: require a complete pending complaint state and
     * a modulo-32-bit remaining interval inside the producer's fixed window. */
    if ((uint32_t)client->pendingComplaintClient <
            (uint32_t)level.maxclients &&
        complaintTimeRemaining > 0 &&
        (uint32_t)complaintTimeRemaining <=
            (uint32_t)GAME_COMPLAINT_WINDOW_MSEC) {
        game_compat_command_handle_complaint_vote(ent, arg);
        return;
    }

    game_compat_command_clear_pending_complaint(client);

    if (level.voteTime == 0) {
        trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                               COMMAND_SEND_UNRELIABLE,
                               "e \"GAME_NOVOTEINPROGRESS\"");
        return;
    }

    if ((client->ps.entityStateFlags & PS_FLAG_VOTE_CAST) != 0) {
        trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                               COMMAND_SEND_UNRELIABLE,
                               "e \"GAME_VOTEALREADYCAST\"");
        return;
    }

    if (client->sessionTeam == TEAM_SPECTATOR) {
        trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                               COMMAND_SEND_UNRELIABLE,
                               "e \"GAME_NOSPECTATORVOTE\"");
        return;
    }

    trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                           COMMAND_SEND_UNRELIABLE,
                           "e \"GAME_VOTECAST\"");
    client->ps.entityStateFlags |= PS_FLAG_VOTE_CAST;

    trap_Argv(1, arg, VOTE_ARG_BUFFER_SIZE);
    if (game_compat_command_vote_arg_is_yes(arg)) {
        level.voteYes = coduo_int32_from_bits(
            (uint32_t)level.voteYes + UINT32_C(1));
        trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
    } else {
        level.voteNo = coduo_int32_from_bits(
            (uint32_t)level.voteNo + UINT32_C(1));
        trap_SetConfigstring(CS_VOTE_NO, va("%i", level.voteNo));
    }
}

/* VERIFIED_DECOMPILER(0x4dffb, 5dffb_Cmd_Activate_f.c, VERIFY-WAVE3-CLIENT-COMMANDS-STANCE-TEAM-2026-06-17): DATAFLOW_VERIFIED - system-active gate, active-state reset, cursor hint target dispatch, item/turret/vehicle callbacks, flak mount stores, and checkpoint health increment checked. */
qboolean Cmd_Activate_f(gentity_t *ent)
{
    gclient_t *client = ent->client;
    gentity_t *unusableVehicle;
    gentity_t *target;
    uint16_t classname;
    qboolean result = 1;

    if (Scr_IsSystemActive(ACTIVATE_SCRIPT_SYSTEM) == 0) {
        return 0;
    }

    unusableVehicle = G_IsVehicleUnusable(ent);
    if (unusableVehicle != 0) {
        unusableVehicle->use(unusableVehicle, ent, ent);
        return 1;
    }

    if (ent->activeState != 0) {
        if ((client->ps.entityStateFlags & ACTIVATE_MOUNT_BLOCK_FLAG) == 0) {
            ent->activeState = 0;
        } else {
            ent->activeState = 2;
        }
        return 1;
    }

    G_CheckForCursorHints(ent);
    if (client->ps.cursorHintEntNum == ENTITYNUM_NONE) {
        return 0;
    }

    target = &g_entities[client->ps.cursorHintEntNum];
    classname = target->scriptClassname;
    if (classname == 0) {
        return result;
    }

    if (classname == scr_const_func_door ||
        classname == scr_const_func_door_rotating) {
        G_TryDoor(target, ent, ent);
    } else if (classname == scr_const_trigger_use) {
        Scr_AddEntity(ent);
        Scr_Notify(target, scr_const_trigger, 1);
        target->use(target, ent, ent);
    } else if (target->s.eType == ET_VEHICLE) {
        if (G_IsVehicleUsable(target, ent) == 0) {
            return 0;
        }
        target->use(target, ent, ent);
    } else if (target->s.eType == ET_ITEM) {
        Scr_AddEntity(ent);
        Scr_Notify(target, scr_const_touch, 1);
        if (target->touch == 0) {
            result = 0;
        } else {
            target->activeState = 1;
            target->touch(target, ent, 0);
        }
    } else if (target->s.eType == ET_TURRET) {
        if (G_IsTurretUsable(target, ent) == 0) {
            return 0;
        }
        target->use(target, ent, ent);
    } else if (classname == scr_const_misc_flak &&
               target->activeState == 0) {
        if (infront(target, ent) == 0) {
            gclient_t *activationClient = &level.clients[ent->s.clientNum];

            if (activationClient->ps.grenadeTimeLeft == 0) {
                target->activeState = 1;
                ent->activeState = 1;
                target->passEntityNum = ent->s.number;
                memcpy(target->scriptMoverAngleTarget, target->currentAngles,
                       sizeof(target->currentAngles));
            } else {
                result = 0;
            }
        } else {
            result = 0;
        }
    } else if (classname == scr_const_script_brushmodel) {
        Scr_AddEntity(ent);
        Scr_Notify(target, scr_const_trigger, 1);
        if (target->use == 0) {
            result = 0;
        } else {
            target->use(target, ent, ent);
        }
    } else if (classname == scr_const_team_WOLF_checkpoint &&
               target->itemCount != client->sessionTeam) {
        target->health = coduo_int32_from_bits(
            (uint32_t)target->health + UINT32_C(1));
    }

    return result;
}

/* VERIFIED_DECOMPILER(0x4e4eb, 5e4eb_Cmd_MatchTimeout_f.c, VERIFY-CLIENT-COMMANDS-REMAINING-2026-06-17): DATAFLOW_VERIFIED - timeout enable/play gates, in-progress gate, allies/axis quota and cache branches, timescale store, configstrings, and broadcast checked. */
void Cmd_MatchTimeout_f(gentity_t *ent)
{
    level_locals_t *lvl = &level;
    gclient_t *client = ent->client;
    const char *teamName;
    char cleanName[CALLVOTE_CLIENT_NAME_BUFFER_SIZE];

    if (g_timeoutsAllowed.integer < 1 || g_timeoutLength.integer < 1) {
        trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                               COMMAND_SEND_UNRELIABLE,
                               "e \"PATCH_1_5_TIMEOUT_NOTENABLED\"");
        return;
    }

    if (client->sessionState != SESS_STATE_PLAYING ||
        client->sessionTeam == TEAM_SPECTATOR) {
        trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                               COMMAND_SEND_UNRELIABLE,
                               "e \"PATCH_1_5_TIMEOUT_MUSTBEPLAYING\"");
        return;
    }

    if (lvl->matchTimeoutDuration != 0 ||
        lvl->matchTimeoutRecoveryEndTime != 0) {
        trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                               COMMAND_SEND_UNRELIABLE,
                               "e \"PATCH_1_5_TIMEOUT_ALREADYINPROGRESS\"");
        return;
    }

    if (client->sessionTeam == TEAM_ALLIES) {
        teamName = "GAME_ALLIES";
        if (g_timeoutsAllowed.integer <= lvl->timeoutUsedAllies) {
            trap_SendServerCommand(
                (uint32_t)(int)(ent - g_entities),
                COMMAND_SEND_UNRELIABLE,
                va("e \"PATCH_1_5_TIMEOUT_MAXCALLED\x14%s\"", teamName));
            return;
        }
        if (lvl->timeoutCache1 < 1) {
            trap_SendServerCommand(
                (uint32_t)(int)(ent - g_entities),
                COMMAND_SEND_UNRELIABLE,
                va("e \"PATCH_1_5_TIMEOUT_MAXTIMEUSED\x14%s\"", teamName));
            return;
        }

        lvl->timeoutUsedAllies = coduo_int32_from_bits(
            (uint32_t)lvl->timeoutUsedAllies + UINT32_C(1));
        lvl->matchTimeoutTeam = TEAM_ALLIES;
        Com_sprintf(lvl->timeoutMessage, sizeof(lvl->timeoutMessage),
                    "PATCH_1_5_TIMEOUT_CALLED\x14%s\x15", teamName);
        lvl->matchTimeoutDuration = lvl->timeoutCache1;
        if (g_timeoutLength.integer < lvl->timeoutCache1) {
            lvl->matchTimeoutDuration = g_timeoutLength.integer;
        }
    } else {
        if (client->sessionTeam != TEAM_AXIS) {
            trap_SendServerCommand(
                (uint32_t)(int)(ent - g_entities),
                COMMAND_SEND_UNRELIABLE,
                "e \"PATCH_1_5_TIMEOUT_INVALIDGAMETYPE\"");
            return;
        }

        teamName = "GAME_AXIS";
        if (g_timeoutsAllowed.integer <= lvl->timeoutUsedAxis) {
            trap_SendServerCommand(
                (uint32_t)(int)(ent - g_entities),
                COMMAND_SEND_UNRELIABLE,
                va("e \"PATCH_1_5_TIMEOUT_MAXCALLED\x14%s\"", teamName));
            return;
        }
        if (lvl->timeoutCache2 < 1) {
            trap_SendServerCommand(
                (uint32_t)(int)(ent - g_entities),
                COMMAND_SEND_UNRELIABLE,
                va("e \"PATCH_1_5_TIMEOUT_MAXTIMEUSED\x14%s\"", teamName));
            return;
        }

        lvl->timeoutUsedAxis = coduo_int32_from_bits(
            (uint32_t)lvl->timeoutUsedAxis + UINT32_C(1));
        lvl->matchTimeoutTeam = TEAM_AXIS;
        Com_sprintf(lvl->timeoutMessage, sizeof(lvl->timeoutMessage),
                    "PATCH_1_5_TIMEOUT_CALLED\x14%s\x15", teamName);
        lvl->matchTimeoutDuration = lvl->timeoutCache2;
        if (g_timeoutLength.integer < lvl->timeoutCache2) {
            lvl->matchTimeoutDuration = g_timeoutLength.integer;
        }
    }

    Q_strncpyz(cleanName, client->userInfoName,
               CALLVOTE_CLIENT_NAME_BUFFER_SIZE);
    Q_CleanStr(cleanName);
    trap_Cvar_Set("timescale", "0");
    lvl->matchTimeoutStartTime = trap_Milliseconds();
    trap_SetConfigstring(CS_TIMEOUT_TIME,
                         va("%i", lvl->matchTimeoutDuration));
    trap_SetConfigstring(CS_TIMEOUT_STRING,
                         lvl->timeoutMessage);
    trap_SendServerCommand(SERVER_COMMAND_ALL_CLIENTS,
                           COMMAND_SEND_UNRELIABLE,
                           va("e \"PATCH_1_5_TIMEOUT_CALLED_PLAYERNAME\x15%s\"",
                              cleanName));
}

/* VERIFIED_DECOMPILER(0x4e99c, 5e99c_Cmd_MatchTimein_f.c, VERIFY-CLIENT-COMMANDS-REMAINING-2026-06-17): DATAFLOW_VERIFIED - timeout presence/play/team gates, ending message, binary timeoutCache1 decrement behavior, recovery timer, configstrings, and broadcast checked. */
void Cmd_MatchTimein_f(gentity_t *ent)
{
    level_locals_t *lvl = &level;
    gclient_t *client = ent->client;
    char cleanName[CALLVOTE_CLIENT_NAME_BUFFER_SIZE];
    int now;

    if (g_timeoutsAllowed.integer < 1) {
        trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                               COMMAND_SEND_UNRELIABLE,
                               "e \"PATCH_1_5_TIMEOUT_NOTENABLED\"");
        return;
    }

    if (lvl->matchTimeoutDuration == 0) {
        trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                               COMMAND_SEND_UNRELIABLE,
                               "e \"PATCH_1_5_TIMEOUT_NONEINPROGRESS\"");
        return;
    }

    if (client->sessionState != SESS_STATE_PLAYING ||
        client->sessionTeam == TEAM_SPECTATOR) {
        trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                               COMMAND_SEND_UNRELIABLE,
                               "e \"PATCH_1_5_TIMEIN_MUSTBEPLAYING\"");
        return;
    }

    if (client->sessionTeam != lvl->matchTimeoutTeam) {
        trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                               COMMAND_SEND_UNRELIABLE,
                               "e \"PATCH_1_5_TIMEIN_WRONGTEAM\"");
        return;
    }

    Com_sprintf(lvl->timeoutMessage, sizeof(lvl->timeoutMessage),
                "PATCH_1_5_TIMEOUT_ENDING\x15");
    Q_strncpyz(cleanName, client->userInfoName,
               CALLVOTE_CLIENT_NAME_BUFFER_SIZE);
    Q_CleanStr(cleanName);
    now = trap_Milliseconds();
    if (lvl->matchTimeoutTeam == TEAM_ALLIES) {
        int elapsed = coduo_int32_from_bits(
            (uint32_t)now - (uint32_t)lvl->matchTimeoutStartTime);

        lvl->timeoutCache1 = coduo_int32_from_bits(
            (uint32_t)lvl->timeoutCache1 - (uint32_t)elapsed);
    } else {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        int elapsed = coduo_int32_from_bits(
            (uint32_t)now - (uint32_t)lvl->matchTimeoutStartTime);

        lvl->timeoutCache2 = coduo_int32_from_bits(
            (uint32_t)lvl->timeoutCache2 - (uint32_t)elapsed);
    }

    lvl->matchTimeoutDuration = 0;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    lvl->matchTimeoutRecoveryEndTime = coduo_int32_from_bits(
        (uint32_t)g_timeoutRecovery.integer +
        (uint32_t)trap_Milliseconds());
    trap_SetConfigstring(CS_TIMEOUT_TIME,
                         va("%i", g_timeoutRecovery.integer));
    trap_SetConfigstring(CS_TIMEOUT_STRING,
                         lvl->timeoutMessage);
    trap_SendServerCommand(SERVER_COMMAND_ALL_CLIENTS,
                           COMMAND_SEND_UNRELIABLE,
                           va("e \"PATCH_1_5_TIMEIN_CALLED_PLAYERNAME\x15%s\"",
                              cleanName));
}

/* "unknown command" format string at ELF 0x0a0f740 */

/* ------------------------------------------------------------------ */
/*  0x4eedd / image 0x5eedd  ClientCommand                            */
/* ------------------------------------------------------------------ */

/*
 * Client command dispatcher.  Called from vmMain GAME_CLIENT_COMMAND.
 *
 * Reads the first argument from the client's command buffer and
 * dispatches to the appropriate handler.  Cheat commands are gated
 * on the client not being in intermission.
 */
/* VERIFIED_DECOMPILER(0x4eedd, 5eedd_ClientCommand.c, VERIFY-WAVE3-CLIENT-COMMANDS-STANCE-TEAM-2026-06-17): DATAFLOW_VERIFIED - chat/score pre-gate routing, non-intermission command order, follow direction constants, and gated unknown-command send checked. */
void ClientCommand(int clientNum)
{
    gentity_t *ent;
    char cmd[MAX_STRING_CHARS];

    ent = &g_entities[clientNum];
    if (ent->client == NULL) {
        return;
    }

    trap_Argv(0, cmd, sizeof(cmd));

    /* ---- Chat commands (always available) ---- */
    if (Q_stricmp(cmd, "say") == 0) {
        Cmd_Say_f(ent, SAY_MODE_ALL, qfalse);
        return;
    }
    if (Q_stricmp(cmd, "say_team") == 0) {
        Cmd_Say_f(ent, SAY_MODE_TEAM, qfalse);
        return;
    }
    if (Q_stricmp(cmd, "say_squad") == 0) {
        Cmd_Say_f(ent, SAY_MODE_SQUAD, qfalse);
        return;
    }
    if (Q_stricmp(cmd, "voice") == 0) {
        Cmd_Voice_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "tell") == 0) {
        Cmd_Tell_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "score") == 0) {
        Cmd_Score_f(ent);
        return;
    }

    /* ---- Cheat commands (gated on not in intermission) ---- */
    if (ent->client->ps.pmType != PM_TYPE_INTERMISSION) {
        if (Q_stricmp(cmd, "mr") == 0) {
            Cmd_MenuResponse_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "give") == 0) {
            Cmd_Give_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "take") == 0) {
            Cmd_Take_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "god") == 0) {
            Cmd_God_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "notarget") == 0) {
            Cmd_Notarget_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "noclip") == 0) {
            Cmd_Noclip_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "ufo") == 0) {
            Cmd_UFO_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "kill") == 0) {
            Cmd_Kill_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "follownext") == 0) {
            Cmd_FollowCycle_f(ent, 1);
            return;
        }
        if (Q_stricmp(cmd, "followprev") == 0) {
            Cmd_FollowCycle_f(ent, -1);
            return;
        }
        if (Q_stricmp(cmd, "where") == 0) {
            Cmd_Where_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "callvote") == 0) {
            Cmd_CallVote_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "vote") == 0) {
            Cmd_Vote_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "gc") == 0) {
            Cmd_GameCommand_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "setviewpos") == 0) {
            Cmd_SetViewpos_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "entitycount") == 0) {
            Cmd_EntityCount_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "nextvehslot") == 0) {
            Cmd_NextVehSlot_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "prevvehslot") == 0) {
            Cmd_PrevVehSlot_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "killcam") == 0) {
            Cmd_Killcam_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "fdc") == 0) {
            Cmd_FlameDamageClient_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "trace_profile") == 0) {
            Cmd_TraceProfile_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "matchtimeout") == 0) {
            Cmd_MatchTimeout_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "matchtimein") == 0) {
            Cmd_MatchTimein_f(ent);
            return;
        }

        /* Unknown command */
        trap_SendServerCommand(clientNum, 0, va(unknownCmdFmt, cmd));
    }
}
