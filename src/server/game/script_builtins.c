/*
 * Source reconstruction for general script builtins.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "recovered_game.h"
#include "compat/coduo_ctype_compat.h"
#include "qcommon/info.h"
#include "game_globals.h"
#include "level_locals.h"
#include "scr_vm.h"
#include "g_syscalls.h"
#include "bg_state.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */
#include "compat/coduo_native_x87.h"
#include "compat/libm/coduo_libm.h"
#include "qcommon/com_sprintf.h"
#include "qcommon/q_string.h"

typedef struct script_entity_field_s script_entity_field_t;
typedef void (*script_entity_field_setter_t)(gentity_t *ent, uint32_t fieldIndex);

typedef struct script_spawn_field_s {
    const char *name;
    size_t offset;
    script_spawn_field_type_t type;
} script_spawn_field_t;

typedef struct script_spawn_entry_s {
    const char *classname;
    void (*spawn)(gentity_t *ent);
} script_spawn_entry_t;

typedef struct ipFilter_s {
    uint32_t mask;
    uint32_t compare;
} ipFilter_t;

struct script_entity_field_s {
    const char *name;
    size_t offset;
    script_spawn_field_type_t type;
    script_entity_field_setter_t setter;
};

typedef struct script_function_s {
    const char *name;
    script_function_callback_t callback;
    int32_t developerOnly;
} script_function_t;

#define SCRIPT_IPRINTLN_COMMAND "f"
#define SCRIPT_IPRINTLN_BOLD_COMMAND "g"
#define SCRIPT_LOCALIZED_SEPARATOR '\x14'
#define SCRIPT_PLAIN_SEPARATOR '\x15'
#define SCRIPT_ESCAPE_CHAR '\x16'
#define SCRIPT_PLAYER_NAME_MESSAGE_FORMAT "%s^7"
#define SCRIPT_DEBUG_DEFAULT_SCALE 1.0f
#define SCRIPT_DEBUG_DEFAULT_DEPTH_TEST 0
#define SCRIPT_DEBUG_LINE_DURATION 0
#define SCRIPT_WEAPON_NONE "none"
#define SCRIPT_EMPTY_STRING ""
#define BULLETTRACE_MASK MASK_BULLETTRACE
#define BULLETTRACE_CHARACTER_CONTENTS CONTENTS_BODY
#define GRENADE_EXPLOSION_TEMP_EVENT EV_PROJECTILE_EXPLODE
#define GRENADE_EXPLOSION_TEMP_DC_VALUE 0
#define GRENADE_EXPLOSION_TRACE_DROP 17.0f
#define GRENADE_EXPLOSION_TRACE_CONTENTS MASK_GRENADE_TRACE
#define SCRIPT_SPAWN_ERROR_FORMAT "unable to spawn \"%s\" entity"
#define SCRIPT_PRECACHE_TURRET_INIT_ERROR "precacheTurret must be called before any wait statements in the level script\n"
#define OBITUARY_TEMP_EVENT EV_OBITUARY
#define OBITUARY_TEMP_FLAG 0x00000008u
#define OBITUARY_MOD_EVENT_FLAG 0x80
#define POSITION_TELEFRAG_MAX_ENTITIES 1024
#define POSITION_TELEFRAG_PLAYER_CONTENTS CONTENTS_BODY
/*
 * The second pass is observed as contents mask 0x80 plus s.eType == 12.
 * The console entity-list path names value 12 ET_VEHICLE; no separate global
 * player-corpse entity enum has been recovered from the game module.
 */
#define POSITION_TELEFRAG_SECONDARY_CONTENTS CONTENTS_TELEFRAG_BLOCKER
#define POSITION_TELEFRAG_SECONDARY_ENTITY_TYPE ET_VEHICLE
#define SCRIPT_MENU_CONFIGSTRING_BASE CS_SCRIPTMENUS
#define SCRIPT_MENU_CONFIGSTRING_COUNT CS_SCRIPTMENUS_COUNT
#define STATUS_ICON_CONFIGSTRING_BASE CS_STATUS_ICONS
#define STATUS_ICON_CONFIGSTRING_COUNT CS_STATUS_ICONS_COUNT
#define HEAD_ICON_CONFIGSTRING_BASE CS_HEAD_ICONS
#define HEAD_ICON_CONFIGSTRING_COUNT CS_HEAD_ICONS_COUNT
#define SCRIPT_PI 3.141592653589793 /* original double64 0x400921fb54442d18 */
#define SCRIPT_HALF_CIRCLE_DEGREES 180.0 /* original double64 0x4066800000000000 */
#define SCRIPT_HALF_CIRCLE_DEGREES_FLOAT 180.0f /* original float32 0x43340000 */
#define RELIABLE_SERVER_COMMAND 1
#define SCRIPT_SECONDS_TO_MILLISECONDS 1000.0f
#define SCRIPT_MUSIC_PLAY_COMMAND "o %s"
#define SCRIPT_MUSIC_STOP_COMMAND "p %i"
#define SCRIPT_SOUNDFADE_COMMAND "q %f %i\n"
#define SCRIPT_AMBIENT_CONFIGSTRING CS_AMBIENT
#define SCRIPT_AMBIENT_PLAY_CONFIGSTRING "n\\%s\\t\\%i"
#define SCRIPT_AMBIENT_STOP_CONFIGSTRING "t\\%i"
#define UNRELIABLE_SERVER_COMMAND 0
#define SCRIPT_REWINDFX_COMMAND "F %d"
#define SCRIPT_REWINDFX_TIME_SCALE 1000
#define SCRIPT_ATMOS_DEFAULT "-1"
#define SCRIPT_FX_CONFIGSTRING_BASE CS_EFFECTS
#define SCRIPT_LOADFX_ERROR \
    "loadFx must be called before any wait statements in the gametype or level " \
    "script, or on an already loaded effect\n"
#define SCRIPT_REWINDFX_USAGE "USAGE: rewindfx <entity> <time>"
#define SCRIPT_PLAYFX_USAGE \
    "USAGE: playFx <effect id from loadFx> <vector position of effect> " \
    "<optional forward vector>"
#define SCRIPT_PLAYFX_UNLOADED "not successfully loaded"
#define SCRIPT_PLAYFX_ZERO_FORWARD "playFx called with (0 0 0) forward direction (effect = %s)\n"
#define SCRIPT_FX_TAG_CONFIGSTRING_BASE CS_FX
#define SCRIPT_FX_TAG_CONFIGSTRING_COUNT CS_FX_COUNT
#define SCRIPT_FX_ID_MAX 79
#define SCRIPT_PLAYFXONTAG_USAGE "USAGE: playFxOnTag <effect id from loadFx> <entity> <tag name>"
#define SCRIPT_INVALID_EFFECT_ID "effect id %i is invalid\n"
#define SCRIPT_PLAYFXONTAG_NO_MODEL "cannot play fx on entity with no model"
#define SCRIPT_PLAYFXONTAG_QUOTE_ERROR "cannot use \" characters in tag names\n"
#define SCRIPT_PLAYFXONTAG_MISSING_TAG "tag '%s' does not exist on entity with model '%s'"
#define SCRIPT_FX_ON_TAG_CONFIGSTRING_FORMAT "%02d%s"
#define SCRIPT_ATMOS_CVAR "cg_atmos"
#define SCRIPT_ATMOS_EFFECT_ID_BUFFER_SIZE 44
#define SCRIPT_SETWIND_USAGE "USAGE: SetWind <vector angles> <strength>"
#define SCRIPT_WIND_CONFIGSTRING CS_WIND
#define SCRIPT_WIND_CONFIGSTRING_FORMAT "%.2f %.2f %.2f %.2f"
#define SCRIPT_PLAYLOOPEDFX_USAGE \
    "USAGE: playLoopedFx <effect id from loadFx> <repeat delay> " \
    "<vector position of effect> <optional cull distance (0 = never cull)> " \
    "<optional forward vector>"
#define SCRIPT_PLAYLOOPEDFX_ZERO_FORWARD "playLoopedFx called with (0 0 0) forward direction (effect = %s)\n"
#define LOOPED_FX_ENTITY_FLAG SVF_LOOPED_FX
#define SCRIPT_SETCULLFOG_USAGE "USAGE: setCullFog(near distance, far distance, red, green, blue, transition time);\n"
#define SCRIPT_SETEXPFOG_USAGE \
    "USAGE: setExpFog(density, red, green, blue, transition time);\n" \
    "Density must be greater than 0 and less than 1, and typically less than .001.  " \
    "For example, .0002 means the fog gets .02%% more dense for every 1 unit of " \
    "distance (about 1%% thicker every 50 units of distance)\n"
#define SCRIPT_SETEXPFOG_DENSITY_ERROR "setExpFog: distance must be greater than 0 and less than 1"
#define SCRIPT_FOG_FORMAT "%g %g %g %g %g %g %.0f"
#define SCRIPT_MAX_PLAYER_NUMBER 63
#define SCRIPT_GAMESTATE_CONFIGSTRING CS_GAMESTATE
#define SCRIPT_WINNER_KEY "winner"
#define SCRIPT_WINNER_FORMAT "%i"
#define SCRIPT_WINNER_ALLIES -2
#define SCRIPT_WINNER_AXIS -1
#define SCRIPT_WINNER_NONE 0
#define SCRIPT_WINNING_TEAM_ERROR "Illegal team string '%s'. Must be allies, axis, or none."
#define SCRIPT_ANNOUNCEMENT_COMMAND "c \"%s\" 2"
#define SCRIPT_TEAM_SCORE_ERROR "Illegal team string '%s'. Must be allies, or axis."
#define SCRIPT_AXIS_SCORE_CONFIGSTRING CS_TEAM_SCORE_AXIS
#define SCRIPT_ALLIES_SCORE_CONFIGSTRING CS_TEAM_SCORE_ALLIES
#define SCRIPT_TEAM_SCORE_FORMAT "%i"
#define SCRIPT_CLIENT_NAME_MODE_UNKNOWN "Unknown mode"
#define SCRIPT_CLIENT_NAME_MANUAL_ONLY "Only works in [manual_change] mode"
#define CLIENT_NAME_SIZE 32
#define EARTHQUAKE_TEMP_EVENT EV_EARTHQUAKE
#define SCRIPT_SHELLSHOCK_CONFIGSTRING_BASE CS_SHELLSHOCKS
#define SCRIPT_SHELLSHOCK_CONFIGSTRING_COUNT CS_SHELLSHOCKS_COUNT
#define SCRIPT_SHELLSHOCK_FIRST_INDEX 1
#define SCRIPT_SHELLSHOCK_LAST_INDEX 15
#define SCRIPT_SHELLSHOCK_USAGE "USAGE: <player> shellshock(<shellshockname>, <duration>)\n"
#define SCRIPT_SHELLSHOCK_NOT_PRECACHED "shellshock '%s' was not precached\n"
#define SCRIPT_SHELLSHOCK_DURATION_ERROR "duration %g should be >= 0 and <= 60"
#define SCRIPT_SHELLSHOCK_DURATION_MAX_MS 60000
#define SCRIPT_SHELLSHOCK_SECONDS_PER_MS 0.001f
#define SCRIPT_STOPSHELLSHOCK_USAGE "USAGE: <player> stopshellshock()\n"
#define SCRIPT_VIEWKICK_USAGE "USAGE: <player> viewkick <force 0-127> <source position>\n"
#define SCRIPT_VIEWKICK_DAMAGE_ERROR "viewkick: damage %g < 0\n"
#define SCRIPT_VIEWKICK_DAMAGE_ROUND 50
#define SCRIPT_VIEWKICK_DAMAGE_SCALE 100
#define SCRIPT_PART_INDEX_RANGE "index out of range (0 - %d)"
#define SCRIPT_PART_BAD_MODEL "bad model"
#define TURRET_NOT_TURRET_ERROR "entity is not a turret"
#define SPAWNPOINT_TRACE_MASK 0x02810011u
#define SPAWNPOINT_TRACE_UP 128.0f
#define SPAWNPOINT_TRACE_DOWN 262144.0f
#define SPAWNPOINT_SOLID_WARNING "WARNING: Spawn point entity %i is in solid at (%i, %i, %i)\n"
#define SCRIPT_EXEC_NO_FILENAME "exec command requires a filename"
#define SCRIPT_EXEC_COMMAND_FORMAT "exec %s\n"
#define SCRIPT_EXIT_STATE_NONE 0
#define SCRIPT_EXIT_STATE_MAP_RESTART 1
#define SCRIPT_EXIT_STATE_EXIT_LEVEL 2
#define SCRIPT_MAP_RESTART_ALREADY_CALLED "map_restart already called"
#define SCRIPT_EXIT_LEVEL_ALREADY_CALLED "exitlevel already called"
#define SCRIPT_MAP_RESTART_COMMAND "map_restart\n"
#define SCRIPT_CONSOLE_COMMAND_NOW 2
#define SCRIPT_SPAWNDUPLICATE_NO_DETAILS "Attempted to spawn duplicate of an entity that didn't save it's spawn details"
#define GAMETYPE_LIST_PATH "maps/mp/gametypes"
#define GAMETYPE_LIST_EXTENSION "gsc"
#define GAMETYPE_SCRIPT_EXTENSION ".gsc"
#define GAMETYPE_SCRIPT_EXTENSION_LENGTH 4u
#define GAMETYPE_DESCRIPTION_PATH "maps/mp/gametypes/%s.txt"
#define GAMETYPE_SCRIPT_PATH "maps/mp/gametypes/%s"
#define GAMETYPE_CALLBACK_SETUP_SCRIPT "maps/mp/gametypes/_callbacksetup"
#define SCRIPT_MAIN_LABEL "main"
#define SCRIPT_CALLBACK_START_GAMETYPE "CodeCallback_StartGameType"
#define SCRIPT_CALLBACK_PLAYER_CONNECT "CodeCallback_PlayerConnect"
#define SCRIPT_CALLBACK_PLAYER_DISCONNECT "CodeCallback_PlayerDisconnect"
#define SCRIPT_CALLBACK_PLAYER_DAMAGE "CodeCallback_PlayerDamage"
#define SCRIPT_CALLBACK_PLAYER_KILLED "CodeCallback_PlayerKilled"
#define LEVEL_SCRIPT_PATH "maps/mp/%s"
#define LEVEL_MAPNAME_CVAR "mapname"
#define LEVEL_MAPNAME_CVAR_FLAGS (CVAR_SERVERINFO | CVAR_ROM)
#define SCRIPT_CLASS_ENTITY_NAME "entity"
#define SCRIPT_CLASS_HUDELEM_NAME "hudelem"
#define SCRIPT_CLASS_VEHICLE_NODE_NAME "vehiclenode"
#define GAMETYPE_TEAM_TOKEN "team"
#define GAMETYPE_FILE_LIST_BUFFER_SIZE 4096
#define GAMETYPE_DESCRIPTION_MAX_LENGTH 1023
#define SCRIPT_BAD_MEANS_OF_DEATH "badMOD"
#define SCRIPT_MOVER_COMMAND_BLOCKED_FLAG 0x00000004u
#define SCRIPT_MOVER_SVFLAGS 0x80u
#define SCRIPT_MODEL_CONTENTS 0x2080
#define SCRIPT_ORIGIN_CONTENTS 0
#define SCRIPT_SPAWN_FIELD_ERROR_FORMAT COM_ERROR_MARKER "classname '%s', key '%s', value '%s': %s"
#define SCRIPT_READ_ONLY_ENTITY_FIELD_ERROR "Tried to set a read only entity field"
#define SCRIPT_NEW_STRING_MAX_LENGTH 65536u
#define WORLDSPAWN_CLASSNAME "worldspawn"
#define WORLDSPAWN_GAME_VALUE "cod"
#define WORLDSPAWN_START_TIME_FORMAT "%i"
#define WORLDSPAWN_AMBIENT_FORMAT "n\\%s"
#define WORLDSPAWN_DEFAULT_GRAVITY "800"
#define WORLDSPAWN_DEFAULT_ZERO "0"
#define SCRIPT_MOVER_LIGHT_DEFAULT "100"
#define WORLDSPAWN_CONFIGSTRING_GAME 2
#define WORLDSPAWN_CONFIGSTRING_MESSAGE 4
#define WORLDSPAWN_CONFIGSTRING_NORTHYAW 11
#define WORLDSPAWN_CONFIGSTRING_START_TIME 14
#define SCR_CONST_WORLDSPAWN_INDEX 100u
#define SCRIPT_ENTITY_FIELD_CLIENT_MASK 0xc000u
#define SCRIPT_ENTITY_FIELD_CLIENT_VALUE SCRIPT_ENTITY_FIELD_CLIENT_MASK
#define SCRIPT_ENTITY_FIELD_CLIENT_INDEX_MASK 0xffff3fffu
#define SCRIPT_VEHICLE_NODE_READ_ONLY "vehicle node is read-only\n"
#define SCRIPT_HUDELEM_COUNT 2048
#define SPAWN_FIELD_CLASSNAME_OFFSET offsetof(gentity_t, scriptClassname)
#define SPAWN_FIELD_ORIGIN_OFFSET offsetof(gentity_t, currentOrigin)
#define SPAWN_FIELD_MODEL_INDEX_OFFSET offsetof(gentity_t, modelIndex)
#define SPAWN_FIELD_BRUSHMODEL_INDEX_OFFSET offsetof(gentity_t, s.itemIndex)
#define SPAWN_FIELD_SPAWNFLAGS_OFFSET offsetof(gentity_t, spawnflags)
#define SPAWN_FIELD_SPEED_OFFSET offsetof(gentity_t, maxSpeed)
#define SPAWN_FIELD_CLOSESPEED_OFFSET offsetof(gentity_t, doorAltSpeed)
#define SPAWN_FIELD_TARGET_OFFSET offsetof(gentity_t, target)
#define SPAWN_FIELD_TARGETNAME_OFFSET offsetof(gentity_t, targetname)
#define SPAWN_FIELD_MESSAGE_OFFSET offsetof(gentity_t, targetLocationMessage)
#define SPAWN_FIELD_TEAMNAME_OFFSET offsetof(gentity_t, teamName)
#define SPAWN_FIELD_WAIT_OFFSET offsetof(gentity_t, itemWait)
#define SPAWN_FIELD_RANDOM_OFFSET offsetof(gentity_t, itemRandom)
#define SPAWN_FIELD_COUNT_OFFSET offsetof(gentity_t, itemCount)
#define SPAWN_FIELD_HEALTH_OFFSET offsetof(gentity_t, health)
#define SPAWN_FIELD_LIGHT_OFFSET ((size_t)0u)
#define SPAWN_FIELD_DMG_OFFSET offsetof(gentity_t, damage)
#define SPAWN_FIELD_ANGLES_OFFSET offsetof(gentity_t, currentAngles)
#define SPAWN_FIELD_DURATION_OFFSET offsetof(gentity_t, ownerIconDelaySeconds)
#define SPAWN_FIELD_ROTATE_OFFSET offsetof(gentity_t, damageDir)
#define SPAWN_FIELD_DEGREES_OFFSET offsetof(gentity_t, doorYawOffset)
#define SPAWN_FIELD_COLOR_OFFSET offsetof(gentity_t, abiGap_2ac_2b7)
#define SPAWN_FIELD_KEY_OFFSET offsetof(gentity_t, doorLocked)
#define SPAWN_FIELD_HARC_OFFSET offsetof(gentity_t, vehiclePrimaryYawClamp)
#define SPAWN_FIELD_VARC_OFFSET offsetof(gentity_t, scriptVehiclePrimaryPitchClamp)
#define SPAWN_FIELD_DELAY_OFFSET offsetof(gentity_t, concussiveFxEndTime)
#define SPAWN_FIELD_RADIUS_OFFSET offsetof(gentity_t, enemyScanRadius)
#define SPAWN_FIELD_MISSIONLEVEL_OFFSET offsetof(gentity_t, missionLevel)
#define SPAWN_FIELD_START_SIZE_OFFSET offsetof(gentity_t, startSize)
#define SPAWN_FIELD_END_SIZE_OFFSET offsetof(gentity_t, endSize)
#define SPAWN_FIELD_SPAWNITEM_OFFSET offsetof(gentity_t, spawnItem)
#define SPAWN_FIELD_TRACK_OFFSET offsetof(gentity_t, scriptTrack)
#define SPAWN_FIELD_VEHICLETYPE_OFFSET offsetof(gentity_t, vehicleSpawnName)
#define SPAWN_FIELD_CAPTURING_OFFSET offsetof(gentity_t, vehiclePrimaryDisabled)
#define SPAWN_FIELD_VEHICLE_OWNER_OFFSET offsetof(gentity_t, vehicleOwner)
#define ENTITY_FIELD_CLIENT_OFFSET offsetof(gentity_t, client)
#define IP_FILTER_OCTET_COUNT 4
#define IP_FILTER_OCTET_BUFFER_SIZE 128
#define MAX_IP_FILTERS 1024
#define IP_FILTER_CVAR_NAME "g_banIPs"
#define IP_FILTER_FULL_MESSAGE "IP filter list is full\n"
#define IP_FILTER_ADD_USAGE "Usage:  addip <ip-mask>\n"
#define IP_FILTER_REMOVE_USAGE "Usage:  sv removeip <ip-mask>\n"
#define IP_FILTER_REMOVED_MESSAGE "Removed.\n"
#define IP_FILTER_NOT_FOUND_FORMAT "Didn't find %s.\n"
#define CONSOLE_LISTIP_EXEC_TIME 1
#define CONSOLE_SAY_RELIABLE 0
#define CONSOLE_ENTITY_INDEX_FORMAT "%3i:"
#define CONSOLE_ENTITY_TYPE_FORMAT "%3i                 "
#define CONSOLE_ENTITY_ORIGIN_FORMAT "%s, origin: %g %g %g"
#define CONSOLE_ENTITY_LINE_END "\n"
#define CONSOLE_ENTITY_TOTAL_FORMAT "Total entities used: %i\n"
#define CONSOLE_BAD_CLIENT_SLOT_FORMAT "Bad client slot: %i\n"
#define CONSOLE_CLIENT_NOT_CONNECTED_FORMAT "Client %i is not connected\n"
#define CONSOLE_USER_NOT_FOUND_FORMAT "User %s is not on the server\n"
#define CONSOLE_LISTIP_COMMAND "g_banIPs\n"
#define CONSOLE_SAY_COMMAND "say"
#define CONSOLE_SAY_FORMAT "e \"GAME_SERVER\x15: %s\""
#define SPAWN_CLASS_INFO_VEHICLE_NODE "info_vehicle_node"
#define SPAWN_CLASS_INFO_VEHICLE_NODE_ROTATE "info_vehicle_node_rotate"
#define SPAWN_NULL_CLASSNAME "G_CallSpawn: NULL classname\n"
#define SPAWN_ENTITY_NULL_CLASSNAME "G_CallSpawnEntity: NULL classname\n"
#define SPAWN_NO_FUNCTION_FORMAT "%s doesn't have a spawn function\n"


#if !defined(_WIN32)
int strcasecmp(const char *a, const char *b);
#endif
const char *ConcatArgs(int start);
void BG_EvaluateTrajectory(const trajectory_t *trajectory, int atTime, float *out);
int G_IndexForMeansOfDeath(const char *name);
uint16_t G_GetHitLocationString(int hitLocation);
int G_RadiusDamage(const float *origin, gentity_t *inflictor, gentity_t *attacker, float damage, float minDamage, float radius,
                   gentity_t *ignore, int meansOfDeath);
int G_EffectIndex(const char *name);
int G_FindConfigstringIndex(const char *name, int start, int max, qboolean create, const char *errormsg);
const char *G_ModelName(int modelIndex);
void G_AddEvent(gentity_t *ent, int event, int eventParm);
gentity_t *G_Spawn(void);
gentity_t *G_CallSpawn(void);
gentity_t *G_TempEntity(const float *origin, int event);
void G_SetOrigin(gentity_t *ent, const float *origin);
void G_SetAngle(gentity_t *ent, const float *angles);
void G_setfog(const char *fog);
qboolean G_CallSpawnEntity(gentity_t *ent);
void G_SpawnItem(gentity_t *ent, gitem_t *item);
void SP_trigger_mount_no_brush(gentity_t *ent, qboolean largeTrigger);
void G_SpawnTurret(gentity_t *ent, const char *weaponName);
void SP_info_null(gentity_t *ent);
void SP_info_notnull(gentity_t *ent);
void SP_light(gentity_t *ent);
void SP_misc_teleporter_dest(gentity_t *ent);
void SP_misc_model(gentity_t *ent);
void SP_corona(gentity_t *ent);
void SP_misc_spawner(gentity_t *ent);
void SP_turret(gentity_t *ent);
void SP_info_vehicle_node(qboolean useNodeAngles);
void SP_func_door(gentity_t *ent);
void SP_func_static(gentity_t *ent);
void SP_func_rotating(gentity_t *ent);
void SP_func_bobbing(gentity_t *ent);
void SP_func_pendulum(gentity_t *ent);
void SP_func_door_rotating(gentity_t *ent);
void SP_trigger_multiple(gentity_t *ent);
void SP_trigger_hurt(gentity_t *ent);
void SP_trigger_damage(gentity_t *ent);
void SP_trigger_mount(gentity_t *ent);
void SP_trigger_once(gentity_t *ent);
void SP_target_location(gentity_t *ent);
void trigger_use(gentity_t *ent);
void SP_trigger_lookat(gentity_t *ent);
void SP_script_vehicle(gentity_t *ent);
void SP_script_vehicle_collmap(gentity_t *ent);
void SP_script_vehicle_owner_icon(gentity_t *ent);
int G_LocalizedStringIndex(const char *value);
int G_ModelIndex(const char *modelName);
int G_ShaderIndex(const char *name);
int G_ShellShockIndex(const char *name);
void RegisterItem(int itemIndex, int updateConfigString);
void Com_Error(errorParm_t code, const char *format, ...);
void Com_Printf(const char *format, ...);
void Com_DPrintf(const char *format, ...);
void G_Error(const char *format, ...);
void G_Printf(const char *format, ...);
void G_DPrintf(const char *format, ...);
void G_LogPrintf(const char *format, ...);
char *Com_Parse(void *parse);
void ClientUserinfoChanged(int clientNum);
void ExitLevel(void);


void G_DObjUpdate(gentity_t *ent);
int G_SpawnFloat(const char *key, const char *defaultValue, float *out);
int G_SpawnVector(const char *key, const char *defaultValue, float *out);
static const char *script_spawn_field_key;
static const char *script_spawn_field_value;

static ipFilter_t ipFilters[MAX_IP_FILTERS];
static int numIPFilters;

int Script_BiasedRoundToInt(float value);
void Script_SinCos(float radians, float *sine, float *cosine);
char *Script_LowercaseString(char *value);
void Scr_SetOrigin(gentity_t *ent);
void Scr_SetAngles(gentity_t *ent);
void Scr_SetHealth(gentity_t *ent);
void Scr_SetEntityFieldReadOnly(void);
void Scr_GetGenericField(void *base, int type, size_t offset);
void Scr_SetGenericField(void *base, int type, size_t offset);
void SP_script_brushmodel(gentity_t *ent);
void SP_script_model(gentity_t *ent);
void SP_script_origin(gentity_t *ent);
int ScriptMover_Updatemove(trajectory_t *trajectory, const float *currentValue, float speed, float linearTime, float decelTime,
                           const float *linearStart, const float *decelStart, const float *targetValue);
void Reached_ScriptMover(gentity_t *ent);

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_message_mode_name). */
static const char *game_compat_script_message_mode_name(script_message_mode_t mode)
{
    switch (mode) {
    case SCRIPT_MESSAGE_MODE_GAME:
        return "Game Message";
    case SCRIPT_MESSAGE_MODE_CVAR_VALUE:
        return "Cvar Value";
    case SCRIPT_MESSAGE_MODE_HINT_STRING:
        return "Hint String";
    case SCRIPT_MESSAGE_MODE_ANNOUNCEMENT:
        return "Announcement String";
    case SCRIPT_MESSAGE_MODE_CLIENT_CVAR_VALUE:
        return "Client Cvar Value";
    case SCRIPT_MESSAGE_MODE_CLIENT_CHAT:
        return "Client Chat Message";
    default:
        return "BAD";
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_check_message_length). */
static void game_compat_script_check_message_length(uint32_t index, const char *modeName, uint32_t size, uint32_t used,
                                                    uint32_t prefixLength, uint32_t textLength)
{
    if (size <= used + prefixLength + textLength) {
        Scr_ParamError(index, va("%s is too long. Max length is %i\n", modeName, size));
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_validate_localized_string). */
static void game_compat_script_validate_localized_string(uint32_t index, const char *text)
{
    for (uint32_t charIndex = 0; text[charIndex] != '\0'; charIndex++) {
        int ch = text[charIndex];

        if (isalnum(coduo_ctype_signed_byte_arg(ch)) == 0 && ch != '_') {
            Scr_ParamError(index,
                           va("Illegal localized string reference: %s (must contain only alpha-numeric characters and underscores", text));
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_plain_string_has_letters). */
static qboolean game_compat_script_plain_string_has_letters(uint32_t index, const char *modeName, const char *text)
{
    qboolean hasLetters = qfalse;

    for (uint32_t charIndex = 0; text[charIndex] != '\0'; charIndex++) {
        int ch = text[charIndex];

        if (ch == SCRIPT_LOCALIZED_SEPARATOR || ch == SCRIPT_PLAIN_SEPARATOR || ch == SCRIPT_ESCAPE_CHAR) {
            Scr_ParamError(index, va("bad escape character (%i) present in string", (int)text[charIndex]));
        }

        if (isalpha(coduo_ctype_signed_byte_arg(ch)) != 0) {
            if (!hasLetters && g_languagewarnings.integer != 0) {
                if (g_languagewarningsaserrors.integer == 0) {
                    Com_Printf("^3WARNING: Non-localized %s string is not allowed to have letters in it. Must be "
                               "changed over to a localized string: \"%s\"\n",
                               modeName, text);
                } else {
                    Scr_LocalizationError(index,
                                          va("non-localized %s strings are not allowed to have letters in them: \"%s\"", modeName, text));
                }
            }
            hasLetters = qtrue;
        }
    }

    return hasLetters;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_append_message_prefix). */
static void game_compat_script_append_message_prefix(char *buffer, uint32_t *used, char prefix)
{
    buffer[*used] = prefix;
    buffer[*used + 1] = '\0';
    (*used)++;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_append_message_text). */
static void game_compat_script_append_message_text(char *buffer, uint32_t *used, const char *text)
{
    uint32_t textLength = (uint32_t)strlen(text);

    for (uint32_t charIndex = 0; charIndex < textLength; charIndex++) {
        unsigned char ch = (unsigned char)text[charIndex];

        if (ch > 0x1f && ch < 0x7f) {
            buffer[*used + charIndex] = (char)ch;
        } else {
            buffer[*used + charIndex] = '.';
        }
    }

    *used += textLength;
    buffer[*used] = '\0';
}

/* VERIFIED_DECOMPILER(0x6689c, 7689c_Scr_LocalizationError.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - ignored index, Com_Error type/message arguments, and void return checked against current decompiler output. */
void Scr_LocalizationError(uint32_t index, const char *message)
{
    (void)index;
    /* NOT_FROM_ORIGINAL_SOURCE: keep the localization diagnostic as data
     * through a literal conversion. */
    Com_Error(7, "%s", message);
}

/* VERIFIED_DECOMPILER(0x668c7, 768c7_Scr_ConstructMessageString.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - mode strings, parameter loop, localized/plain markers, player-name formatting, validation errors, length checks, sanitized copy, and empty-message marker checked against current decompiler output. */
void Scr_ConstructMessageString(uint32_t index, char *buffer, uint32_t size, script_message_mode_t mode)
{
    const char *modeName = game_compat_script_message_mode_name(mode);
    qboolean needsPlainMarker = qtrue;
    uint32_t used = 0;
    uint32_t paramCount = Scr_GetNumParam();

    buffer[0] = '\0';

    for (uint32_t paramIndex = index; paramIndex < paramCount; paramIndex++) {
        script_variable_type_t type = Scr_GetType(paramIndex);
        const char *text;
        uint32_t textLength;
        uint32_t prefixLength = 0;
        char prefix = '\0';

        if (type == SCRIPT_VAR_LOCALIZED_STRING) {
            text = Scr_GetIString(paramIndex);
            textLength = (uint32_t)strlen(text);
            game_compat_script_validate_localized_string(paramIndex, text);

            prefixLength = used != 0 ? 1u : 0u;
            game_compat_script_check_message_length(paramIndex, modeName, size, used, prefixLength, textLength);
            if (prefixLength != 0) {
                game_compat_script_append_message_prefix(buffer, &used, SCRIPT_LOCALIZED_SEPARATOR);
            }
            needsPlainMarker = qtrue;
        } else if (type == SCRIPT_VAR_OBJECT && Scr_GetPointerType(paramIndex) == SCRIPT_VAR_ENTITY) {
            gentity_t *ent = Scr_GetEntity(paramIndex);

            if (ent->client == 0) {
                Scr_ParamError(paramIndex, "Entity is not a player");
            }

            text = va(SCRIPT_PLAYER_NAME_MESSAGE_FORMAT, ent->client->userInfoName);
            textLength = (uint32_t)strlen(text);
            prefixLength = needsPlainMarker ? 1u : 0u;
            game_compat_script_check_message_length(paramIndex, modeName, size, used, prefixLength, textLength);
            if (prefixLength != 0) {
                game_compat_script_append_message_prefix(buffer, &used, SCRIPT_PLAIN_SEPARATOR);
            }
            needsPlainMarker = qfalse;
        } else {
            qboolean hasLetters;

            text = Scr_GetString(paramIndex);
            textLength = (uint32_t)strlen(text);
            hasLetters = game_compat_script_plain_string_has_letters(paramIndex, modeName, text);

            if (hasLetters) {
                prefixLength = used != 0 ? 1u : 0u;
                prefix = SCRIPT_LOCALIZED_SEPARATOR;
            } else if (needsPlainMarker) {
                prefixLength = 1u;
                prefix = SCRIPT_PLAIN_SEPARATOR;
            }

            game_compat_script_check_message_length(paramIndex, modeName, size, used, prefixLength, textLength);
            if (prefixLength != 0) {
                game_compat_script_append_message_prefix(buffer, &used, prefix);
            }
            needsPlainMarker = qfalse;
        }

        game_compat_script_append_message_text(buffer, &used, text);
    }

    if (needsPlainMarker) {
        buffer[used] = SCRIPT_PLAIN_SEPARATOR;
        buffer[used + 1] = '\0';
    }
}

/* VERIFIED_DECOMPILER(0x66dde, 76dde_Scr_MakeGameMessage.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - 1024-byte message build, command formatting, unreliable server command, entity target, and void return checked against current decompiler output. */
void Scr_MakeGameMessage(int entityNum, const char *command)
{
    char message[MAX_STRING_CHARS];

    Scr_ConstructMessageString(0, message, sizeof(message), SCRIPT_MESSAGE_MODE_GAME);
    trap_SendServerCommand(entityNum, UNRELIABLE_SERVER_COMMAND, va("%s \"%s\"", command, message));
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_debug_set_default_color). */
static void game_compat_script_debug_set_default_color(vec3_t color)
{
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_item_index_from_def). */
static int game_compat_script_item_index_from_def(const gitem_t *item)
{
    return (int)(item - bg_itemlist);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_weapon_index_for_name). */
static int game_compat_script_weapon_index_for_name(const char *name)
{
    return BG_GetWeaponIndexForName(name) & 0xff;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_weapon_missing_name_should_warn). */
static int game_compat_script_weapon_missing_name_should_warn(const char *name)
{
    return name[0] != '\0' && Q_stricmp(name, SCRIPT_WEAPON_NONE) != 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_add_means_of_death_name). */
static void game_compat_script_add_means_of_death_name(int meansOfDeath)
{
    if (meansOfDeath < 0 || meansOfDeath >= MOD_COUNT) {
        Scr_AddString(SCRIPT_BAD_MEANS_OF_DEATH);
        return;
    }

    Scr_AddString(modNames[meansOfDeath]);
}

/* VERIFIED_DECOMPILER(0x6f0b9, 7f0b9_FUN_0007f0b9.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - 0.5 bias, ROUND call semantics, integer cast, and return checked against current decompiler output. */
int Script_BiasedRoundToInt(float value)
{
    /*
     * 0x6f0cb..0x6f0e8: fld value; fadd 0.5f; truncating fistp (RC=0xc00).
     * The sum is truncated straight from the x87 register -> shim.
     */
#if EMULATE_X87
    return x87f_store_i32_trunc(x87f_add(x87f_load_f32(value), x87f_load_f32(0.5f)));
#else
    return (int)((long double)value + (long double)0.5f);
#endif
}

/* VERIFIED_DECOMPILER(0x6f0f7, 7f0f7_FUN_0007f0f7.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - binary32 load, single fsincos operation, cosine-then-sine binary32 stores, and void return checked. */
/* Stock 0x6f0f7 is `fld DWORD; fsincos; fstp(cos); fstp(sin)`.  Native x87
 * targets use that exact sequence; only non-x87 targets use the permitted libm
 * fallback.  script_func_sin/cos call the stock libm boundary directly; only
 * tan routes through this helper. */
void Script_SinCos(float radians, float *sine, float *cosine)
{
#if defined(__i386__) || defined(__x86_64__)
    coduo_x87_sincosf(radians, sine, cosine);
#else
    *cosine = (float)CoduoLibm_Cos((double)radians);
    *sine = (float)CoduoLibm_Sin((double)radians);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_copy_vector). */
static void game_compat_script_copy_vector(vec3_t dest, const vec3_t src)
{
    dest[0] = src[0];
    dest[1] = src[1];
    dest[2] = src[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_obituary_uses_means_of_death_eventparm). */
static qboolean game_compat_script_obituary_uses_means_of_death_eventparm(int meansOfDeath)
{
    return meansOfDeath == MOD_MELEE || meansOfDeath == MOD_MELEE_BINOCULARS || meansOfDeath == MOD_HEAD_SHOT ||
           meansOfDeath == MOD_ARTILLERY || meansOfDeath == MOD_SUICIDE || meansOfDeath == MOD_FALLING || meansOfDeath == MOD_CRUSH ||
           meansOfDeath == MOD_CRUSH_TANK || meansOfDeath == MOD_CRUSH_JEEP || meansOfDeath == MOD_WATER;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_builtin_make_player_box). */
static void game_compat_script_builtin_make_player_box(const vec3_t origin, vec3_t mins, vec3_t maxs)
{
    for (uint32_t axis = 0; axis < 3; axis++) {
        mins[axis] = origin[axis] + playerMins[axis];
        maxs[axis] = origin[axis] + playerMaxs[axis];
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_precache_configstring). */
static void game_compat_script_precache_configstring(const char *value, int base, int count, const char *duplicateMessage,
                                                     const char *tooManyMessage)
{
    char buffer[MAX_STRING_CHARS];

    for (int index = 0; index < count; index++) {
        trap_GetConfigstring(base + index, buffer, sizeof(buffer));
        if (Q_stricmp(buffer, value) == 0) {
            Com_DPrintf(duplicateMessage, value);
            return;
        }
    }

    for (int index = 0; index < count; index++) {
        trap_GetConfigstring(base + index, buffer, sizeof(buffer));
        if (buffer[0] == '\0') {
            trap_SetConfigstring(base + index, value);
            return;
        }
    }

    Scr_Error(va(tooManyMessage, count));
    /* The original error call falls through to this out-of-range store if the
     * script VM returns: index has reached count at 0x6b046..0x6b07a. */
    trap_SetConfigstring(base + count, value);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_find_configstring_index). */
static int game_compat_script_find_configstring_index(const char *value, int base, int count, const char *missingMessage, int emptyValue,
                                                      int returnBias)
{
    char buffer[MAX_STRING_CHARS];

    if (returnBias != 0 && value[0] == '\0') {
        return 0;
    }

    for (int index = 0; index < count; index++) {
        trap_GetConfigstring(base + index, buffer, sizeof(buffer));
        if (Q_stricmp(buffer, value) == 0) {
            return index + returnBias;
        }
    }

    Scr_Error(va(missingMessage, value));
    return emptyValue;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_check_precache_phase). */
static void game_compat_script_check_precache_phase(const char *message)
{
    if (level.spawning == 0) {
        Scr_Error(message);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_bullettrace_add_entity). */
static void game_compat_script_bullettrace_add_entity(uint16_t entityNum)
{
    if (entityNum == ENTITYNUM_NONE || entityNum == ENTITYNUM_WORLD) {
        Scr_AddUndefined();
        return;
    }

    Scr_AddEntity(&g_entities[entityNum]);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_bullettrace_add_position_or_direction). */
static void game_compat_script_bullettrace_add_position_or_direction(const trace_t *trace, const vec3_t start, const vec3_t end)
{
    vec3_t direction;

    if (trace->fraction < 1.0f) {
        Scr_AddVector(trace->endpos);
        return;
    }

    direction[0] = end[0] - start[0];
    direction[1] = end[1] - start[1];
    direction[2] = end[2] - start[2];
    VectorNormalize(direction);
    Scr_AddVector(direction);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_get_anim_delta_time_range). */
static void game_compat_script_get_anim_delta_time_range(float *startTime, float *endTime)
{
    uint32_t paramCount = Scr_GetNumParam();

    *startTime = 0.0f;
    *endTime = 1.0f;

    if (paramCount != 1) {
        if (paramCount != 2) {
            *endTime = Scr_GetFloat(2);
            if (*endTime < 0.0f || *endTime > 1.0f) {
                Scr_ParamError(2, "end time must be between 0 and 1");
            }
        }

        *startTime = Scr_GetFloat(1);
        if (*startTime < 0.0f || *startTime > 1.0f) {
            Scr_ParamError(1, "start time must be between 0 and 1");
        }
    }
}

/* VERIFIED_DECOMPILER(0x66800, 76800_script_func_print.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - g_NoScriptSpam guard, parameter count loop, Scr_GetDebugString calls, Com_Printf format, and void return checked against current decompiler output. */
void script_func_print(void)
{
    if (g_NoScriptSpam.integer != 0) {
        return;
    }

    uint32_t paramCount = Scr_GetNumParam();
    for (uint32_t index = 0; index < paramCount; index++) {
        Com_Printf("%s", Scr_GetDebugString(index));
    }
}

/* VERIFIED_DECOMPILER(0x66863, 76863_script_func_println.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - g_NoScriptSpam guard, print helper call, newline Com_Printf, and void return checked against current decompiler output. */
void script_func_println(void)
{
    if (g_NoScriptSpam.integer != 0) {
        return;
    }

    script_func_print();
    Com_Printf("\n");
}

/* VERIFIED_DECOMPILER(0x66e58, 76e58_script_func_iprintln.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - global client target, command string, Scr_MakeGameMessage call, and void return checked against current decompiler output. */
void script_func_iprintln(void)
{
    Scr_MakeGameMessage(SERVER_COMMAND_ALL_CLIENTS, SCRIPT_IPRINTLN_COMMAND);
}

/* VERIFIED_DECOMPILER(0x66e86, 76e86_script_func_iprintlnbold.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - global client target, bold command string, Scr_MakeGameMessage call, and void return checked against current decompiler output. */
void script_func_iprintlnbold(void)
{
    Scr_MakeGameMessage(SERVER_COMMAND_ALL_CLIENTS, SCRIPT_IPRINTLN_BOLD_COMMAND);
}

/* VERIFIED_DECOMPILER(0x66eb4, 76eb4_script_func_print3d.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - defaults, optional argument ladder including two-arg skip, vector/text call order, and trap_AddDebugString arguments checked against current decompiler output. */
void script_func_print3d(void)
{
    float scale = SCRIPT_DEBUG_DEFAULT_SCALE;
    vec3_t color;
    vec3_t origin;
    const char *text;
    uint32_t paramCount = Scr_GetNumParam();

    game_compat_script_debug_set_default_color(color);

    if (paramCount != 2) {
        if (paramCount != 3) {
            if (paramCount != 4) {
                scale = Scr_GetFloat(4);
            }
            (void)Scr_GetFloat(3);
        }
        Scr_GetVector(2, color);
    }

    text = Scr_GetString(1);
    Scr_GetVector(0, origin);
    trap_AddDebugString(origin, color, scale, text);
}

/* VERIFIED_DECOMPILER(0x66f96, 76f96_script_func_line.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - defaults, optional argument ladder including two-arg skip, start/end vector order, depth-test argument, duration 0, and trap_AddDebugLine arguments checked against current decompiler output. */
void script_func_line(void)
{
    int depthTest = SCRIPT_DEBUG_DEFAULT_DEPTH_TEST;
    vec3_t color;
    vec3_t start;
    vec3_t end;
    uint32_t paramCount = Scr_GetNumParam();

    game_compat_script_debug_set_default_color(color);

    if (paramCount != 2) {
        if (paramCount != 3) {
            if (paramCount != 4) {
                depthTest = Scr_GetInt(4);
            }
            (void)Scr_GetFloat(3);
        }
        Scr_GetVector(2, color);
    }

    Scr_GetVector(1, end);
    Scr_GetVector(0, start);
    trap_AddDebugLine(start, end, color, depthTest, SCRIPT_DEBUG_LINE_DURATION);
}

/* VERIFIED_DECOMPILER(0x67084, 77084_script_func_assert.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - empty implementation and void return checked against current decompiler output. */
void script_func_assert(void)
{
}

/* VERIFIED_DECOMPILER(0x67089, 77089_script_func_isdefined.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - type query, pointer-type threshold, undefined-type fallback, Scr_AddInt arguments, and void return checked against current decompiler output. */
void script_func_isdefined(void)
{
    script_variable_type_t type = Scr_GetType(0);

    if (type == SCRIPT_VAR_OBJECT) {
        Scr_AddInt(Scr_GetPointerType(0) <= SCRIPT_VAR_ARRAY);
    } else {
        Scr_AddInt(type != SCRIPT_VAR_UNDEFINED);
    }
}

/* VERIFIED_DECOMPILER(0x670eb, 770eb_script_func_isalive.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - pointer/entity guards, entity fetch, health > 0 truth value, false fallbacks, and Scr_AddInt calls checked against current decompiler output. */
void script_func_isalive(void)
{
    if (Scr_GetType(0) == SCRIPT_VAR_OBJECT && Scr_GetPointerType(0) == SCRIPT_VAR_ENTITY) {
        gentity_t *ent = Scr_GetEntity(0);
        Scr_AddInt(ent->health > 0);
        return;
    }

    Scr_AddInt(0);
}

/* VERIFIED_DECOMPILER(0x67176, 77176_script_func_isvalidplayer.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - pointer/entity guards, linked byte, client pointer, connected-state check, false fallbacks, and Scr_AddInt calls checked against current decompiler output. */
void script_func_isvalidplayer(void)
{
    if (Scr_GetType(0) == SCRIPT_VAR_OBJECT && Scr_GetPointerType(0) == SCRIPT_VAR_ENTITY) {
        gentity_t *ent = Scr_GetEntity(0);

        Scr_AddInt(ent->linked != 0 && ent->client != 0 && ent->client->connectedState == CON_CONNECTED);
        return;
    }

    Scr_AddInt(0);
}

/* VERIFIED_DECOMPILER(0x6749e, 7749e_script_func_gettime.c, VERIFY-SCRIPT-BUILTINS-PACKET-2026-06-17): DATAFLOW_VERIFIED - level time global read, Scr_AddInt argument, and void return checked against current decompiler output. */
void script_func_gettime(void)
{
    Scr_AddInt(level.time);
}

/* VERIFIED_DECOMPILER(0x674ca, 774ca_script_func_getentbynum.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - unsigned entity index, 1024 bound, gentity lookup, linked byte guard, Scr_AddEntity argument, and void return checked against current decompiler output. */
void script_func_getentbynum(void)
{
    uint32_t entityNumber = (uint32_t)Scr_GetInt(0);

    if (entityNumber < MAX_GENTITIES) {
        gentity_t *ent = script_object_to_gentity(entityNumber);

        if (ent->linked != 0) {
            Scr_AddEntity(ent);
        }
    }
}

/* VERIFIED_DECOMPILER(0x67523, 77523_script_func_getweaponmodel.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - Scr_GetString, byte weapon index, none/empty warning suppression, empty-string fallback, BG_GetInfoForWeapon call, worldModel offset, Scr_AddString calls, and void return checked against current decompiler output. */
void script_func_getweaponmodel(void)
{
    const char *weaponName = Scr_GetString(0);
    int weapon = game_compat_script_weapon_index_for_name(weaponName);

    if (weapon == 0) {
        if (game_compat_script_weapon_missing_name_should_warn(weaponName)) {
            /* NOT_FROM_ORIGINAL_SOURCE: forward the completed warning as data
             * through a literal conversion. */
            Com_Printf("%s", va("unknown weapon '%s' in getWeaponModel\n", weaponName));
        }
        Scr_AddString(SCRIPT_EMPTY_STRING);
        return;
    }

    Scr_AddString(((weaponInfo_t *)BG_GetInfoForWeapon(weapon))->worldModel);
}

/* VERIFIED_DECOMPILER(0x675c8, 775c8_script_func_getweaponclassname.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - byte weapon index, missing-weapon warning path, undefined fallback, alt-weapon loop, scriptClassname and altWeapon offsets, no-Radiant-name warning, and void return checked against current decompiler output. */
void script_func_getweaponclassname(void)
{
    const char *weaponName = Scr_GetString(0);
    int weapon = game_compat_script_weapon_index_for_name(weaponName);
    int currentWeapon;

    if (weapon == 0) {
        if (game_compat_script_weapon_missing_name_should_warn(weaponName)) {
            /* NOT_FROM_ORIGINAL_SOURCE: forward the completed warning as data
             * through a literal conversion. */
            Com_Printf("%s", va("unknown weapon '%s' in getWeaponClassname\n", weaponName));
        }
        Scr_AddUndefined();
        return;
    }

    currentWeapon = weapon;
    do {
        const weaponInfo_t *weaponInfo = BG_GetInfoForWeapon(currentWeapon);
        const char *classname = weaponInfo->scriptClassname;

        if (classname[0] != '\0') {
            Scr_AddString(classname);
            return;
        }

        currentWeapon = weaponInfo->altWeapon;
    } while (currentWeapon != 0 && currentWeapon != weapon);

    /* NOT_FROM_ORIGINAL_SOURCE: forward the completed warning as data through
     * a literal conversion. */
    Com_Printf("%s", va("^3WARNING^7: no Radiant name found for weapon '%s' in "
                        "getWeaponClassname\n",
                        weaponName));
    Scr_AddUndefined();
}

/* VERIFIED_DECOMPILER(0x676c6, 776c6_script_func_getanimlength.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - Scr_GetAnim output, primitive guard, exact Scr_ParamError text, trap_XAnimGetLengthSeconds result, Scr_AddFloat call, and void return checked against current decompiler output. */
void script_func_getanimlength(void)
{
    scr_anim_t animRef;
    uint32_t anim;

    animRef = Scr_GetAnim(0, NULL);
    anim = ((uint32_t)animRef.treeIndex << SCR_ANIM_TREE_INDEX_SHIFT) | animRef.animIndex;
    if (!trap_XAnimIsPrimitive(anim)) {
        Scr_ParamError(0, "non-primitive animation has no concept of length");
    }

    Scr_AddFloat(trap_XAnimGetLengthSeconds(anim));
}

/* VERIFIED_DECOMPILER(0x67733, 77733_script_func_animhasnotetrack.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - Scr_GetAnim, Scr_GetConstString index 1, trap_XAnimNotetrackExists argument order, Scr_AddBool result, and void return checked against current decompiler output. */
void script_func_animhasnotetrack(void)
{
    scr_anim_t animRef;
    uint32_t anim;
    uint16_t notetrack;

    animRef = Scr_GetAnim(0, NULL);
    anim = ((uint32_t)animRef.treeIndex << SCR_ANIM_TREE_INDEX_SHIFT) | animRef.animIndex;
    notetrack = Scr_GetConstString(1);
    Scr_AddBool(trap_XAnimNotetrackExists(anim, notetrack));
}

/* VERIFIED_DECOMPILER(0x67793, 77793_script_func_getbrushmodelcenter.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - Scr_GetEntity, absMin/absMax x/y/z offsets, 0.5 multiply, stack vector layout, Scr_AddVector call, and void return checked against current decompiler output. */
void script_func_getbrushmodelcenter(void)
{
    gentity_t *ent = Scr_GetEntity(0);
    vec3_t center;

    /*
     * 0x677ba..0x6781a: each absMin+absMax sum is stored to the center slot
     * (rounded to float), then reloaded and scaled by 0.5f in place — two
     * roundings per component, adds first, then the three multiplies.
     */
    center[0] = ent->absMin[0] + ent->absMax[0];
    center[1] = ent->absMin[1] + ent->absMax[1];
    center[2] = ent->absMin[2] + ent->absMax[2];
    center[0] *= 0.5f;
    center[1] *= 0.5f;
    center[2] *= 0.5f;
    Scr_AddVector(center);
}

/* VERIFIED_DECOMPILER(0x6782e, 7782e_script_func_getfullclipammo.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - Scr_GetString, byte weapon index, BG_GetInfoForWeapon call, clipSize offset, Scr_AddInt argument, and void return checked against current decompiler output. */
void script_func_getfullclipammo(void)
{
    const char *weaponName = Scr_GetString(0);
    int weapon = game_compat_script_weapon_index_for_name(weaponName);
    const weaponInfo_t *weaponInfo = BG_GetInfoForWeapon(weapon);

    Scr_AddInt(weaponInfo->clipSize);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_spawn_var_active). */
static int game_compat_script_spawn_var_active(void)
{
    return level.spawningMapEntities;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_spawn_set_var_active). */
static void game_compat_script_spawn_set_var_active(int active)
{
    level.spawningMapEntities = active;
}

static const script_spawn_field_t script_spawn_fields[] = {{"classname", SPAWN_FIELD_CLASSNAME_OFFSET, SCRIPT_SPAWN_FIELD_STRING},
                                                           {"origin", SPAWN_FIELD_ORIGIN_OFFSET, SCRIPT_SPAWN_FIELD_VECTOR},
                                                           {"model", SPAWN_FIELD_MODEL_INDEX_OFFSET, SCRIPT_SPAWN_FIELD_MODEL},
                                                           {"spawnflags", SPAWN_FIELD_SPAWNFLAGS_OFFSET, SCRIPT_SPAWN_FIELD_INT},
                                                           {"speed", SPAWN_FIELD_SPEED_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT},
                                                           {"closespeed", SPAWN_FIELD_CLOSESPEED_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT},
                                                           {"target", SPAWN_FIELD_TARGET_OFFSET, SCRIPT_SPAWN_FIELD_STRING},
                                                           {"targetname", SPAWN_FIELD_TARGETNAME_OFFSET, SCRIPT_SPAWN_FIELD_STRING},
                                                           {"message", SPAWN_FIELD_MESSAGE_OFFSET, SCRIPT_SPAWN_FIELD_STRING},
                                                           {"teamname", SPAWN_FIELD_TEAMNAME_OFFSET, SCRIPT_SPAWN_FIELD_STRING},
                                                           {"wait", SPAWN_FIELD_WAIT_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT},
                                                           {"random", SPAWN_FIELD_RANDOM_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT},
                                                           {"count", SPAWN_FIELD_COUNT_OFFSET, SCRIPT_SPAWN_FIELD_INT},
                                                           {"health", SPAWN_FIELD_HEALTH_OFFSET, SCRIPT_SPAWN_FIELD_INT},
                                                           {"light", SPAWN_FIELD_LIGHT_OFFSET, SCRIPT_SPAWN_FIELD_LIGHT},
                                                           {"dmg", SPAWN_FIELD_DMG_OFFSET, SCRIPT_SPAWN_FIELD_INT},
                                                           {"angles", SPAWN_FIELD_ANGLES_OFFSET, SCRIPT_SPAWN_FIELD_VECTOR},
                                                           {"duration", SPAWN_FIELD_DURATION_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT},
                                                           {"rotate", SPAWN_FIELD_ROTATE_OFFSET, SCRIPT_SPAWN_FIELD_VECTOR},
                                                           {"degrees", SPAWN_FIELD_DEGREES_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT},
                                                           {"time", SPAWN_FIELD_SPEED_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT},
                                                           {"_color", SPAWN_FIELD_COLOR_OFFSET, SCRIPT_SPAWN_FIELD_VECTOR},
                                                           {"color", SPAWN_FIELD_COLOR_OFFSET, SCRIPT_SPAWN_FIELD_VECTOR},
                                                           {"key", SPAWN_FIELD_KEY_OFFSET, SCRIPT_SPAWN_FIELD_INT},
                                                           {"harc", SPAWN_FIELD_HARC_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT},
                                                           {"varc", SPAWN_FIELD_VARC_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT},
                                                           {"delay", SPAWN_FIELD_DELAY_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT},
                                                           {"radius", SPAWN_FIELD_RADIUS_OFFSET, SCRIPT_SPAWN_FIELD_INT},
                                                           {"missionlevel", SPAWN_FIELD_MISSIONLEVEL_OFFSET, SCRIPT_SPAWN_FIELD_INT},
                                                           {"start_size", SPAWN_FIELD_START_SIZE_OFFSET, SCRIPT_SPAWN_FIELD_INT},
                                                           {"end_size", SPAWN_FIELD_END_SIZE_OFFSET, SCRIPT_SPAWN_FIELD_INT},
                                                           {"shard", SPAWN_FIELD_COUNT_OFFSET, SCRIPT_SPAWN_FIELD_INT},
                                                           {"spawnitem", SPAWN_FIELD_SPAWNITEM_OFFSET, SCRIPT_SPAWN_FIELD_STRING},
                                                           {"track", SPAWN_FIELD_TRACK_OFFSET, SCRIPT_SPAWN_FIELD_STRING},
                                                           {"vehicletype", SPAWN_FIELD_VEHICLETYPE_OFFSET, SCRIPT_SPAWN_FIELD_STRING},
                                                           {"capturing", SPAWN_FIELD_CAPTURING_OFFSET, SCRIPT_SPAWN_FIELD_INT},
                                                           {"vehicle_owner", SPAWN_FIELD_VEHICLE_OWNER_OFFSET, SCRIPT_SPAWN_FIELD_ENTITY},
                                                           {0, 0, 0}};

const script_spawn_entry_t spawns[] = {{"info_null", SP_info_null},
                                       {"info_notnull", SP_info_notnull},
                                       {"func_door", SP_func_door},
                                       {"func_static", SP_func_static},
                                       {"func_rotating", SP_func_rotating},
                                       {"func_bobbing", SP_func_bobbing},
                                       {"func_pendulum", SP_func_pendulum},
                                       {"func_group", SP_info_null},
                                       {"func_door_rotating", SP_func_door_rotating},
                                       {"trigger_multiple", SP_trigger_multiple},
                                       {"trigger_hurt", SP_trigger_hurt},
                                       {"trigger_dammage", SP_trigger_damage},
                                       {"trigger_mount", SP_trigger_mount},
                                       {"trigger_once", SP_trigger_once},
                                       {"target_location", SP_target_location},
                                       {"mp_target_location", SP_target_location},
                                       {"light", SP_light},
                                       {"misc_teleporter_dest", SP_misc_teleporter_dest},
                                       {"misc_model", SP_misc_model},
                                       {"misc_mg42", SP_turret},
                                       {"misc_turret", SP_turret},
                                       {"misc_spawner", SP_misc_spawner},
                                       {"corona", SP_corona},
                                       {"trigger_use", trigger_use},
                                       {"trigger_damage", SP_trigger_damage},
                                       {"trigger_lookat", SP_trigger_lookat},
                                       {"script_brushmodel", SP_script_brushmodel},
                                       {"script_model", SP_script_model},
                                       {"script_origin", SP_script_origin},
                                       {"script_vehicle", SP_script_vehicle},
                                       {"script_vehicle_collmap", SP_script_vehicle_collmap},
                                       {"script_vehicle_owner_icon", SP_script_vehicle_owner_icon},
                                       {0, 0}};

static void game_compat_script_entity_field_set_read_only(gentity_t *ent, uint32_t fieldIndex);
static void game_compat_script_entity_field_set_origin(gentity_t *ent, uint32_t fieldIndex);
static void game_compat_script_entity_field_set_angles(gentity_t *ent, uint32_t fieldIndex);
static void game_compat_script_entity_field_set_health(gentity_t *ent, uint32_t fieldIndex);

static const script_entity_field_t script_entity_fields[] = {
    {"classname", SPAWN_FIELD_CLASSNAME_OFFSET, SCRIPT_SPAWN_FIELD_STRING, game_compat_script_entity_field_set_read_only},
    {"origin", SPAWN_FIELD_ORIGIN_OFFSET, SCRIPT_SPAWN_FIELD_VECTOR, game_compat_script_entity_field_set_origin},
    {"model", SPAWN_FIELD_MODEL_INDEX_OFFSET, SCRIPT_SPAWN_FIELD_MODEL, game_compat_script_entity_field_set_read_only},
    {"spawnflags", SPAWN_FIELD_SPAWNFLAGS_OFFSET, SCRIPT_SPAWN_FIELD_INT, game_compat_script_entity_field_set_read_only},
    {"speed", SPAWN_FIELD_SPEED_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT, 0},
    {"closespeed", SPAWN_FIELD_CLOSESPEED_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT, 0},
    {"target", SPAWN_FIELD_TARGET_OFFSET, SCRIPT_SPAWN_FIELD_STRING, 0},
    {"targetname", SPAWN_FIELD_TARGETNAME_OFFSET, SCRIPT_SPAWN_FIELD_STRING, 0},
    {"message", SPAWN_FIELD_MESSAGE_OFFSET, SCRIPT_SPAWN_FIELD_STRING, 0},
    {"teamname", SPAWN_FIELD_TEAMNAME_OFFSET, SCRIPT_SPAWN_FIELD_STRING, 0},
    {"wait", SPAWN_FIELD_WAIT_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT, 0},
    {"random", SPAWN_FIELD_RANDOM_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT, 0},
    {"count", SPAWN_FIELD_COUNT_OFFSET, SCRIPT_SPAWN_FIELD_INT, 0},
    {"health", SPAWN_FIELD_HEALTH_OFFSET, SCRIPT_SPAWN_FIELD_INT, game_compat_script_entity_field_set_health},
    {"light", SPAWN_FIELD_LIGHT_OFFSET, SCRIPT_SPAWN_FIELD_LIGHT, 0},
    {"dmg", SPAWN_FIELD_DMG_OFFSET, SCRIPT_SPAWN_FIELD_INT, 0},
    {"angles", SPAWN_FIELD_ANGLES_OFFSET, SCRIPT_SPAWN_FIELD_VECTOR, game_compat_script_entity_field_set_angles},
    {"duration", SPAWN_FIELD_DURATION_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT, 0},
    {"rotate", SPAWN_FIELD_ROTATE_OFFSET, SCRIPT_SPAWN_FIELD_VECTOR, 0},
    {"degrees", SPAWN_FIELD_DEGREES_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT, 0},
    {"time", SPAWN_FIELD_SPEED_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT, 0},
    {"_color", SPAWN_FIELD_COLOR_OFFSET, SCRIPT_SPAWN_FIELD_VECTOR, 0},
    {"color", SPAWN_FIELD_COLOR_OFFSET, SCRIPT_SPAWN_FIELD_VECTOR, 0},
    {"key", SPAWN_FIELD_KEY_OFFSET, SCRIPT_SPAWN_FIELD_INT, 0},
    {"harc", SPAWN_FIELD_HARC_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT, 0},
    {"varc", SPAWN_FIELD_VARC_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT, 0},
    {"delay", SPAWN_FIELD_DELAY_OFFSET, SCRIPT_SPAWN_FIELD_FLOAT, 0},
    {"radius", SPAWN_FIELD_RADIUS_OFFSET, SCRIPT_SPAWN_FIELD_INT, 0},
    {"missionlevel", SPAWN_FIELD_MISSIONLEVEL_OFFSET, SCRIPT_SPAWN_FIELD_INT, 0},
    {"start_size", SPAWN_FIELD_START_SIZE_OFFSET, SCRIPT_SPAWN_FIELD_INT, 0},
    {"end_size", SPAWN_FIELD_END_SIZE_OFFSET, SCRIPT_SPAWN_FIELD_INT, 0},
    {"shard", SPAWN_FIELD_COUNT_OFFSET, SCRIPT_SPAWN_FIELD_INT, 0},
    {"spawnitem", SPAWN_FIELD_SPAWNITEM_OFFSET, SCRIPT_SPAWN_FIELD_STRING, 0},
    {"track", SPAWN_FIELD_TRACK_OFFSET, SCRIPT_SPAWN_FIELD_STRING, 0},
    {"vehicletype", SPAWN_FIELD_VEHICLETYPE_OFFSET, SCRIPT_SPAWN_FIELD_STRING, game_compat_script_entity_field_set_read_only},
    {"capturing", SPAWN_FIELD_CAPTURING_OFFSET, SCRIPT_SPAWN_FIELD_INT, 0},
    {"vehicle_owner", SPAWN_FIELD_VEHICLE_OWNER_OFFSET, SCRIPT_SPAWN_FIELD_ENTITY, 0},
    {0, 0, 0, 0}};

static const char script_radiant_fields[] = ".txt";

/* NOT_FROM_ORIGINAL_SOURCE: byte-offset accessor for script/reflection field tables. */
static uint8_t *game_compat_script_spawn_entity_byte_slot(gentity_t *ent, size_t offset)
{
    return &((uint8_t *)(void *)ent)[offset];
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_entity_string_slot). */
static uint16_t *game_compat_script_entity_string_slot(gentity_t *ent, size_t offset)
{
    return (uint16_t *)(void *)game_compat_script_spawn_entity_byte_slot(ent, offset);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_entity_classname). */
static uint16_t game_compat_script_entity_classname(gentity_t *ent)
{
    return *game_compat_script_entity_string_slot(ent, SPAWN_FIELD_CLASSNAME_OFFSET);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_entity_client). */
static gclient_t *game_compat_script_entity_client(gentity_t *ent)
{
    return *(gclient_t **)(void *)game_compat_script_spawn_entity_byte_slot(ent, ENTITY_FIELD_CLIENT_OFFSET);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_spawn_copy_vector_at). */
static void game_compat_script_spawn_copy_vector_at(gentity_t *ent, size_t offset, const vec3_t value)
{
    float *slot = (float *)(void *)game_compat_script_spawn_entity_byte_slot(ent, offset);

    slot[0] = value[0];
    slot[1] = value[1];
    slot[2] = value[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_spawn_set_string_at). */
static void game_compat_script_spawn_set_string_at(gentity_t *ent, size_t offset, uint16_t value)
{
    *game_compat_script_entity_string_slot(ent, offset) = value;
}

/* VERIFIED_DECOMPILER(0x712e4, 812e4_G_SpawnString.c, VERIFY-P1-SPAWN-PARSE-2026-06-17): DATAFLOW_VERIFIED - inactive default store still falls through to spawn-var scan, key comparison, value output, and return values checked against current decompiler output. */
int G_SpawnString(const char *key, const char *defaultValue, const char **out)
{
    level_locals_t *lvl = &level;

    if (game_compat_script_spawn_var_active() == 0) {
        *out = defaultValue;
    }

    for (int index = 0; index < lvl->spawnVarCount; index++) {
        char **pair = &lvl->spawnVarPairSlots[index * 2];

        if (strcmp(key, pair[0]) == 0) {
            *out = pair[1];
            return 1;
        }
    }

    *out = defaultValue;
    return 0;
}

/* VERIFIED_DECOMPILER(0x71387, 81387_G_SpawnFloat.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - G_SpawnString arguments, local text output, atof conversion, float store, return value, and no side effects beyond out parameter checked against current decompiler output. */
int G_SpawnFloat(const char *key, const char *defaultValue, float *out)
{
    const char *text;
    int found = G_SpawnString(key, defaultValue, &text);

    *out = (float)atof(text);
    return found;
}

/* VERIFIED_DECOMPILER(0x713d0, 813d0_G_SpawnInt.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - G_SpawnString arguments, local text output, atoi conversion, int store, return value, and no side effects beyond out parameter checked against current decompiler output. */
int G_SpawnInt(const char *key, const char *defaultValue, int *out)
{
    const char *text;
    int found = G_SpawnString(key, defaultValue, &text);

    *out = atoi(text);
    return found;
}

/* VERIFIED_DECOMPILER(0x71419, 81419_G_SpawnVector.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - G_SpawnString arguments, sscanf format, x/y/z output addresses at +0/+4/+8, return value, and no other side effects checked against current decompiler output. */
int G_SpawnVector(const char *key, const char *defaultValue, float *out)
{
    const char *text;
    int found = G_SpawnString(key, defaultValue, &text);

    sscanf(text, "%f %f %f", &out[0], &out[1], &out[2]);
    return found;
}

/* VERIFIED_DECOMPILER(0x71480, 81480_FUN_00081480.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - exact read-only error string, Scr_Error call, and void return checked against current decompiler output. */
void Scr_SetEntityFieldReadOnly(void)
{
    Scr_Error(SCRIPT_READ_ONLY_ENTITY_FIELD_ERROR);
}

/* VERIFIED_DECOMPILER(0x714a6, 814a6_FUN_000814a6.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - spawn-var active branch, classname lookup, global key/value usage, va argument order, direct Com_Error format argument, Scr_ObjectError fallback, and void return checked against current decompiler output. */
void Scr_SetEntityFieldError(gentity_t *ent, const char *message)
{
    const char *classname;

    (void)ent;

    if (game_compat_script_spawn_var_active() != 0) {
        G_SpawnString("classname", SCRIPT_EMPTY_STRING, &classname);
        /* NOT_FROM_ORIGINAL_SOURCE: keep spawn fields and the completed
         * diagnostic as data through a single formatting pass. */
        Com_Error(ERR_DROP, SCRIPT_SPAWN_FIELD_ERROR_FORMAT, classname, script_spawn_field_key, script_spawn_field_value, message);
    }

    Scr_ObjectError(message);
}

/* VERIFIED_DECOMPILER(0x71537, 81537_FUN_00081537.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - Scr_FindField result/type handling, float/string/int conversions for dynamic type tags 4/1/5, early returns, GScr_SetDynamicEntityField arguments, and void return checked against current decompiler output. */
void G_ParseDynamicEntityField(const char *key, const char *value, gentity_t *ent)
{
    int fieldType;
    uint16_t fieldName = Scr_FindField(key, &fieldType);

    if (fieldName == 0) {
        return;
    }

    if (fieldType == SCRIPT_VAR_FLOAT) {
        Scr_AddFloat((float)atof(value));
    } else if (fieldType < SCRIPT_VAR_INT) {
        if (fieldType != SCRIPT_VAR_STRING) {
            return;
        }
        Scr_AddString(value);
    } else {
        if (fieldType != SCRIPT_VAR_INT) {
            return;
        }
        Scr_AddInt(atoi(value));
    }

    GScr_SetDynamicEntityField(ent, fieldName);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_spawn_find_field). */
static const script_spawn_field_t *game_compat_script_spawn_find_field(const char *key)
{
    for (const script_spawn_field_t *field = script_spawn_fields; field->name != 0; field++) {
        if (Q_stricmp(field->name, key) == 0) {
            return field;
        }
    }

    return 0;
}

/* VERIFIED_DECOMPILER(0x715de, 815de_FUN_000815de.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - global key/value stores, spawn-field table lookup, dynamic-field fallback, int/float/string/vector/model cases, string reset/new-string order, brushmodel/model stores, and void return checked against current decompiler output. */
void G_ParseEntityField(const char *key, const char *value, gentity_t *ent)
{
    const script_spawn_field_t *field;
    uint8_t *base;

    script_spawn_field_key = key;
    script_spawn_field_value = value;

    field = game_compat_script_spawn_find_field(key);
    if (field == 0) {
        G_ParseDynamicEntityField(key, value, ent);
        return;
    }

    base = game_compat_script_spawn_entity_byte_slot(ent, field->offset);
    switch (field->type) {
    case SCRIPT_SPAWN_FIELD_INT:
        *(int32_t *)(void *)base = atoi(value);
        break;
    case SCRIPT_SPAWN_FIELD_FLOAT:
        *(float *)(void *)base = (float)atof(value);
        break;
    case SCRIPT_SPAWN_FIELD_STRING:
        Scr_SetString((uint16_t *)(void *)base, 0);
        game_compat_script_spawn_set_string_at(ent, field->offset, G_NewString(value));
        break;
    case SCRIPT_SPAWN_FIELD_VECTOR: {
        vec3_t vector;

        sscanf(value, "%f %f %f", &vector[0], &vector[1], &vector[2]);
        game_compat_script_spawn_copy_vector_at(ent, field->offset, vector);
        break;
    }
    case SCRIPT_SPAWN_FIELD_MODEL:
        if (value[0] == '*') {
            *(uint32_t *)(void *)game_compat_script_spawn_entity_byte_slot(ent, SPAWN_FIELD_BRUSHMODEL_INDEX_OFFSET) =
                (uint32_t)atoi(&value[1]) & 0xffffu;
        } else {
            *game_compat_script_spawn_entity_byte_slot(ent, SPAWN_FIELD_MODEL_INDEX_OFFSET) = (uint8_t)G_ModelIndex(value);
        }
        break;
    default:
        break;
    }
}

/* VERIFIED_DECOMPILER(0x717a5, 817a5_FUN_000817a5.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - spawn-var count loop, key/value pair indexing, G_ParseEntityField argument order, G_SetOrigin currentOrigin offset, G_SetAngle currentAngles offset, and void return checked against current decompiler output. */
void G_ParseEntityFields(gentity_t *ent)
{
    level_locals_t *lvl = &level;

    for (int index = 0; index < lvl->spawnVarCount; index++) {
        char **pair = &lvl->spawnVarPairSlots[index * 2];

        G_ParseEntityField(pair[0], pair[1], ent);
    }

    G_SetOrigin(ent, (float *)(void *)game_compat_script_spawn_entity_byte_slot(ent, SPAWN_FIELD_ORIGIN_OFFSET));
    G_SetAngle(ent, (float *)(void *)game_compat_script_spawn_entity_byte_slot(ent, SPAWN_FIELD_ANGLES_OFFSET));
}

/* VERIFIED_DECOMPILER(0x7183f, 8183f_G_DuplicateEntityFields.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - spawn-field table loop, int/float dword copies, string Scr_SetString copy, vector three-dword copy, model byte copy, default/no-op cases, and void return checked against current decompiler output. */
void G_DuplicateEntityFields(gentity_t *dest, gentity_t *source)
{
    for (const script_spawn_field_t *field = script_spawn_fields; field->name != 0; field++) {
        uint8_t *destBase = game_compat_script_spawn_entity_byte_slot(dest, field->offset);
        uint8_t *sourceBase = game_compat_script_spawn_entity_byte_slot(source, field->offset);

        switch (field->type) {
        case SCRIPT_SPAWN_FIELD_INT:
        case SCRIPT_SPAWN_FIELD_FLOAT:
            *(uint32_t *)(void *)destBase = *(uint32_t *)(void *)sourceBase;
            break;
        case SCRIPT_SPAWN_FIELD_STRING:
            Scr_SetString((uint16_t *)(void *)destBase, *(uint16_t *)(void *)sourceBase);
            break;
        case SCRIPT_SPAWN_FIELD_VECTOR: {
            uint32_t *destWords = (uint32_t *)(void *)destBase;
            const uint32_t *sourceWords = (const uint32_t *)(const void *)sourceBase;

            destWords[0] = sourceWords[0];
            destWords[1] = sourceWords[1];
            destWords[2] = sourceWords[2];
        } break;
        case SCRIPT_SPAWN_FIELD_MODEL:
            *destBase = *sourceBase;
            break;
        default:
            break;
        }
    }
}

/* VERIFIED_DECOMPILER(0x71970, 81970_G_DuplicateScriptFields.c, VERIFY-SCRIPT-BUILTINS-PACKET2-2026-06-17): DATAFLOW_VERIFIED - source/dest s_number argument order, zero flags argument, Scr_CopyEntityNum call, and void return checked against current decompiler output. */
void G_DuplicateScriptFields(gentity_t *dest, gentity_t *source)
{
    Scr_CopyEntityNum(source->s.number, dest->s.number, 0);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_spawn_find_entry). */
static const script_spawn_entry_t *game_compat_script_spawn_find_entry(const char *classname)
{
    for (const script_spawn_entry_t *entry = spawns; entry->classname != 0; entry++) {
        if (strcmp(entry->classname, classname) == 0) {
            return entry;
        }
    }

    return 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_entity_field_registers). */
static qboolean game_compat_script_entity_field_registers(script_spawn_field_type_t type)
{
    switch (type) {
    case SCRIPT_SPAWN_FIELD_INT:
    case SCRIPT_SPAWN_FIELD_FLOAT:
    case SCRIPT_SPAWN_FIELD_CSTRING:
    case SCRIPT_SPAWN_FIELD_STRING:
    case SCRIPT_SPAWN_FIELD_VECTOR:
    case SCRIPT_SPAWN_FIELD_ENTITY:
    case SCRIPT_SPAWN_FIELD_OBJECT:
    case SCRIPT_SPAWN_FIELD_MODEL:
        return qtrue;
    default:
        return qfalse;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_spawn_find_item). */
static gitem_t *game_compat_script_spawn_find_item(const char *classname)
{
    gitem_t *item = &bg_itemlist[1];

    for (;;) {
        const char *itemClassname = item->classname;

        if (itemClassname == 0) {
            return 0;
        }
        if (strcmp(itemClassname, classname) == 0) {
            return item;
        }
        item++;
    }
}

/* VERIFIED_DECOMPILER(0x719a6, 819a6_G_CallSpawn.c, VERIFY-P1-SPAWN-2026-06-17): DATAFLOW_VERIFIED - classname read, vehicle-node side effects, item loop, spawn table loop, G_ParseEntityFields order, spawn calls, and return values checked against current decompiler output. */
gentity_t *G_CallSpawn(void)
{
    const char *classname;
    gitem_t *item;
    const script_spawn_entry_t *entry;
    gentity_t *ent;

    G_SpawnString("classname", SCRIPT_EMPTY_STRING, &classname);
    if (classname == 0) {
        G_Printf(SPAWN_NULL_CLASSNAME);
        return 0;
    }

    if (strcmp(SPAWN_CLASS_INFO_VEHICLE_NODE, classname) == 0) {
        SP_info_vehicle_node(qfalse);
        return 0;
    }

    if (strcmp(SPAWN_CLASS_INFO_VEHICLE_NODE_ROTATE, classname) == 0) {
        SP_info_vehicle_node(qtrue);
        return 0;
    }

    item = game_compat_script_spawn_find_item(classname);
    if (item != 0) {
        ent = G_Spawn();
        G_ParseEntityFields(ent);
        G_SpawnItem(ent, item);
        return ent;
    }

    entry = game_compat_script_spawn_find_entry(classname);
    ent = G_Spawn();
    G_ParseEntityFields(ent);
    if (entry != 0) {
        entry->spawn(ent);
    }

    return ent;
}

/* VERIFIED_DECOMPILER(0x71b3a, 81b3a_G_CallSpawnEntity.c, VERIFY-P1-SPAWN-2026-06-17): DATAFLOW_VERIFIED - classname field read, item loop, spawn table loop, missing-spawn diagnostic, spawn calls, and return values checked against current decompiler output. */
qboolean G_CallSpawnEntity(gentity_t *ent)
{
    uint16_t classnameId = game_compat_script_entity_classname(ent);
    const char *classname;
    gitem_t *item;
    const script_spawn_entry_t *entry;

    if (classnameId == 0) {
        G_Printf(SPAWN_ENTITY_NULL_CLASSNAME);
        return qfalse;
    }

    classname = SL_ConvertToString(classnameId);
    item = game_compat_script_spawn_find_item(classname);
    if (item != 0) {
        G_SpawnItem(ent, item);
        return qtrue;
    }

    entry = game_compat_script_spawn_find_entry(classname);
    if (entry == 0) {
        G_Printf(SPAWN_NO_FUNCTION_FORMAT, SL_ConvertToString(classnameId));
        return qfalse;
    }

    entry->spawn(ent);
    return qtrue;
}

/* VERIFIED_DECOMPILER(0x71c57, 81c57_G_NewString.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - length guard, escape conversion loop, newline special case, backslash fallback, SL_GetString arguments, and return value checked against current decompiler output. */
uint16_t G_NewString(const char *value)
{
    char converted[SCRIPT_NEW_STRING_MAX_LENGTH];
    char *out = converted;
    uint32_t copyLength = (uint32_t)strlen(value) + UINT32_C(1);
    int32_t signedCopyLength;

    if (copyLength > SCRIPT_NEW_STRING_MAX_LENGTH) {
        G_Error("G_NewString: len = %i > %i\n", (int)copyLength, (int)SCRIPT_NEW_STRING_MAX_LENGTH);
    }

    /* 0x71c77 and 0x71cc6 consume the wrapped strlen-plus-one dword. */
    signedCopyLength = coduo_int32_from_bits(copyLength);
    for (int32_t index = 0; index < signedCopyLength; index++) {
        if (value[index] == '\\' && index < coduo_int32_from_bits(copyLength - UINT32_C(1))) {
            index++;
            if (value[index] == 'n') {
                *out = '\n';
            } else {
                *out = '\\';
            }
        } else {
            *out = value[index];
        }
        out++;
    }

    return SL_GetString(converted, 0);
}

/* VERIFIED_DECOMPILER(0x71d79, 81d79_GScr_AddFieldsForEntity.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - entity classnum load, field-table walk, registered type switch cases, class-field index calculation, and client-field registration checked against current decompiler output. */
void GScr_AddFieldsForEntity(void)
{
    uint16_t classnum = g_scr_data.classMap[SCRIPT_OBJECT_ENTITY].classnum;

    for (uint32_t index = 0; script_entity_fields[index].name != 0; index++) {
        const script_entity_field_t *field = &script_entity_fields[index];

        if (game_compat_script_entity_field_registers(field->type)) {
            Scr_AddClassField(classnum, field->name, (uint16_t)index);
        }
    }

    GScr_AddFieldsForClient(classnum);
}

/* VERIFIED_DECOMPILER(0x71e16, 81e16_GScr_AddFieldsForRadiant.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - radiant class name, field string argument, Scr_AddFields call, and void return checked against current decompiler output. */
void GScr_AddFieldsForRadiant(void)
{
    Scr_AddFields("radiant", script_radiant_fields);
}

/* VERIFIED_DECOMPILER(0x71e46, 81e46_Scr_SetEntityField.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - g_entities index math, client-field mask branch, player guard/error, client field index mask, direct field-table setter test, generic setter path, callback path, and void return checked against current decompiler output. */
void Scr_SetEntityField(int entityNum, uint32_t fieldIndex)
{
    gentity_t *ent = &g_entities[entityNum];
    const script_entity_field_t *field;

    if ((fieldIndex & SCRIPT_ENTITY_FIELD_CLIENT_MASK) == SCRIPT_ENTITY_FIELD_CLIENT_VALUE) {
        gclient_t *client = game_compat_script_entity_client(ent);

        if (client == 0) {
            Scr_SetEntityFieldError(ent, "field must be applied to a player");
        }
        Scr_SetClientField(client, (int)(fieldIndex & SCRIPT_ENTITY_FIELD_CLIENT_INDEX_MASK));
        return;
    }

    field = &script_entity_fields[fieldIndex];
    if (field->setter == 0) {
        Scr_SetGenericField(ent, field->type, field->offset);
    } else {
        field->setter(ent, fieldIndex);
    }
}

/* VERIFIED_DECOMPILER(0x71f15, 81f15_Scr_SetGenericField.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - int/float/string/vector/entity/vector-y cases, VM getter call order, destination offsets, Scr_SetString use, vector element stores, default fallthrough, and void return checked against current decompiler output. */
void Scr_SetGenericField(void *base, int type, size_t offset)
{
    uint8_t *slot = &((uint8_t *)base)[offset];

    switch ((script_spawn_field_type_t)type) {
    case SCRIPT_SPAWN_FIELD_INT:
        *(int32_t *)(void *)slot = Scr_GetInt(0);
        break;
    case SCRIPT_SPAWN_FIELD_FLOAT:
        *(float *)(void *)slot = Scr_GetFloat(0);
        break;
    case SCRIPT_SPAWN_FIELD_STRING: {
        uint16_t value = Scr_GetConstString(0);

        Scr_SetString((uint16_t *)(void *)slot, value);
        break;
    }
    case SCRIPT_SPAWN_FIELD_VECTOR: {
        vec3_t value;

        Scr_GetVector(0, value);
        float *vectorSlot = (float *)(void *)slot;

        vectorSlot[0] = value[0];
        vectorSlot[1] = value[1];
        vectorSlot[2] = value[2];
        break;
    }
    case SCRIPT_SPAWN_FIELD_ENTITY:
        *(gentity_t **)(void *)slot = Scr_GetEntity(0);
        break;
    case SCRIPT_SPAWN_FIELD_VECTOR_Y: {
        vec3_t value;

        Scr_GetVector(0, value);
        *(float *)(void *)slot = value[1];
        break;
    }
    default:
        break;
    }
}

/* VERIFIED_DECOMPILER(0x72012, 82012_Scr_SetObjectField.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - hudelem/entity/vehicle-node class branches, argument forwarding, read-only vehicle-node error through va/Scr_Error, and void return checked against current decompiler output. */
void Scr_SetObjectField(int classnum, int objectNum, int fieldIndex)
{
    if (classnum == SCRIPT_OBJECT_HUDELEM) {
        Scr_SetHudElemField(objectNum, fieldIndex);
    } else if (classnum == SCRIPT_OBJECT_ENTITY) {
        Scr_SetEntityField(objectNum, (uint32_t)fieldIndex);
    } else if (classnum == SCRIPT_OBJECT_VEHICLE_NODE) {
        Scr_Error(va(SCRIPT_VEHICLE_NODE_READ_ONLY));
    }
}

/* VERIFIED_DECOMPILER(0x72082, 82082_Scr_GetEntityField.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - g_entities index math, client-field mask branch, player guard/error, client field index mask, direct generic field-table lookup, Scr_GetGenericField arguments, and void return checked against current decompiler output. */
void Scr_GetEntityField(int entityNum, uint32_t fieldIndex)
{
    gentity_t *ent = &g_entities[entityNum];
    const script_entity_field_t *field;

    if ((fieldIndex & SCRIPT_ENTITY_FIELD_CLIENT_MASK) == SCRIPT_ENTITY_FIELD_CLIENT_VALUE) {
        gclient_t *client = game_compat_script_entity_client(ent);

        if (client == 0) {
            Scr_SetEntityFieldError(ent, "field must be applied to a player");
        }
        Scr_GetClientField(client, (int)(fieldIndex & SCRIPT_ENTITY_FIELD_CLIENT_INDEX_MASK));
        return;
    }

    field = &script_entity_fields[fieldIndex];
    Scr_GetGenericField(ent, field->type, field->offset);
}

/* VERIFIED_DECOMPILER(0x72131, 82131_Scr_GetGenericField.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - int/float/cstring/string/vector/entity/vector-y/object/model cases, zero guards, VM adder call arguments, vector-y stack layout, model byte read, default fallthrough, and void return checked against current decompiler output. */
void Scr_GetGenericField(void *base, int type, size_t offset)
{
    uint8_t *slot = &((uint8_t *)base)[offset];

    switch ((script_spawn_field_type_t)type) {
    case SCRIPT_SPAWN_FIELD_INT:
        Scr_AddInt(*(int32_t *)(void *)slot);
        break;
    case SCRIPT_SPAWN_FIELD_FLOAT:
        Scr_AddFloat(*(float *)(void *)slot);
        break;
    case SCRIPT_SPAWN_FIELD_CSTRING:
        Scr_AddString((const char *)(const void *)slot);
        break;
    case SCRIPT_SPAWN_FIELD_STRING: {
        uint16_t value = *(uint16_t *)(void *)slot;

        if (value != 0) {
            Scr_AddConstString(value);
        }
        break;
    }
    case SCRIPT_SPAWN_FIELD_VECTOR:
        Scr_AddVector((const float *)(const void *)slot);
        break;
    case SCRIPT_SPAWN_FIELD_ENTITY: {
        gentity_t *ent = *(gentity_t **)(void *)slot;

        if (ent != 0) {
            Scr_AddEntity(ent);
        }
        break;
    }
    case SCRIPT_SPAWN_FIELD_VECTOR_Y: {
        vec3_t vector = {0.0f, *(float *)(void *)slot, 0.0f};

        Scr_AddVector(vector);
        break;
    }
    case SCRIPT_SPAWN_FIELD_OBJECT: {
        uint16_t value = *(uint16_t *)(void *)slot;

        if (value != 0) {
            Scr_AddObject(value);
        }
        break;
    }
    case SCRIPT_SPAWN_FIELD_MODEL:
        Scr_AddString(G_ModelName(*slot));
        break;
    default:
        break;
    }
}

/* VERIFIED_DECOMPILER(0x7225e, 8225e_Scr_GetObjectField.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - hudelem/entity/vehicle-node class branches, argument forwarding, vehicle-node getter path, and void return checked against current decompiler output. */
void Scr_GetObjectField(int classnum, int objectNum, int fieldIndex)
{
    if (classnum == SCRIPT_OBJECT_HUDELEM) {
        Scr_GetHudElemField(objectNum, fieldIndex);
    } else if (classnum == SCRIPT_OBJECT_ENTITY) {
        Scr_GetEntityField(objectNum, (uint32_t)fieldIndex);
    } else if (classnum == SCRIPT_OBJECT_VEHICLE_NODE) {
        GScr_GetVehicleNodeField(objectNum, fieldIndex);
    }
}

/* VERIFIED_DECOMPILER(0x722ca, 822ca_Scr_FreeEntityConstStrings.c, VERIFY-WAVE2-SCRIPT-SPAWNFREE-2026-06-17): DATAFLOW_VERIFIED - string-field table scan, Scr_SetString clears, six attach model byte clears, and attach tag string clears checked against current decompiler output. */
void Scr_FreeEntityConstStrings(gentity_t *ent)
{
    for (const script_entity_field_t *field = script_entity_fields; field->name != 0; field++) {
        if (field->type == SCRIPT_SPAWN_FIELD_STRING) {
            Scr_SetString(game_compat_script_entity_string_slot(ent, field->offset), 0);
        }
    }

    for (uint32_t index = 0; index < 6; index++) {
        ent->attachModelIndex[index] = 0;
        Scr_SetString(&ent->attachTagIndex[index], 0);
    }
}

/* VERIFIED_DECOMPILER(0x72367, 82367_Scr_FreeEntity.c, VERIFY-WAVE2-SCRIPT-SPAWNFREE-2026-06-17): DATAFLOW_VERIFIED - const-string cleanup call and Scr_FreeEntityNum(ent->s.number, 0) checked against current decompiler output. */
void Scr_FreeEntity(gentity_t *ent)
{
    Scr_FreeEntityConstStrings(ent);
    Scr_FreeEntityNum(ent->s.number, 0);
}

/* VERIFIED_DECOMPILER(0x7239f, 8239f_Scr_AddEntity.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - s_number load, entity class id, Scr_AddEntityNum argument order, and void return checked against current decompiler output. */
void Scr_AddEntity(gentity_t *ent)
{
    Scr_AddEntityNum(ent->s.number, SCRIPT_OBJECT_ENTITY);
}

/* VERIFIED_DECOMPILER(0x723cc, 823cc_Scr_GetEntity.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - Scr_GetEntityNum class output, entity class and 1024 bound checks, g_entities stride lookup, Scr_ParamError fallback, null fallback return, and pointer return checked against current decompiler output. */
gentity_t *Scr_GetEntity(uint32_t index)
{
    int classnum;
    uint32_t entityNum = Scr_GetEntityNum(index, &classnum);

    if (classnum == SCRIPT_OBJECT_ENTITY && entityNum < MAX_GENTITIES) {
        return &g_entities[entityNum];
    }

    Scr_ParamError(index, "not an entity");
    return 0;
}

/* VERIFIED_DECOMPILER(0x72440, 82440_Scr_FreeHudElem.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - const-string cleanup call, hudelem index derivation, hudelem class id, Scr_FreeEntityNum arguments, and void return checked against current decompiler output. */
void Scr_FreeHudElem(game_hudElem_t *elem)
{
    Scr_FreeHudElemConstStrings(elem);
    Scr_FreeEntityNum((int)(elem - g_hudelems), SCRIPT_OBJECT_HUDELEM);
}

/* VERIFIED_DECOMPILER(0x72487, 82487_Scr_AddHudElem.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - hudelem index derivation, hudelem class id, Scr_AddEntityNum arguments, and void return checked against current decompiler output. */
void Scr_AddHudElem(game_hudElem_t *elem)
{
    Scr_AddEntityNum((int)(elem - g_hudelems), SCRIPT_OBJECT_HUDELEM);
}

/* VERIFIED_DECOMPILER(0x724c3, 824c3_Scr_GetHudElem.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - Scr_GetEntityNum class output, hudelem class and 2048 bound checks, g_hudelems stride lookup, Scr_ParamError fallback, null fallback return, and pointer return checked against current decompiler output. */
game_hudElem_t *Scr_GetHudElem(uint32_t index)
{
    int classnum;
    uint32_t elemNum = Scr_GetEntityNum(index, &classnum);

    if (classnum == SCRIPT_OBJECT_HUDELEM && elemNum < SCRIPT_HUDELEM_COUNT) {
        return &g_hudelems[elemNum];
    }

    Scr_ParamError(index, "not a hudelem");
    return 0;
}

/* VERIFIED_DECOMPILER(0x7253d, 8253d_Scr_ExecEntThread.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - s_number load, entity class id, handle/paramCount forwarding, Scr_ExecEntThreadNum argument order, and return value checked against current decompiler output. */
uint16_t Scr_ExecEntThread(gentity_t *ent, uint32_t handle, int paramCount)
{
    return Scr_ExecEntThreadNum(ent->s.number, SCRIPT_OBJECT_ENTITY, handle, paramCount);
}

/* VERIFIED_DECOMPILER(0x7257b, 8257b_Scr_AddExecEntThread.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - s_number load, entity class id, handle/paramCount forwarding, Scr_AddExecEntThreadNum argument order, and void return checked against current decompiler output. */
void Scr_AddExecEntThread(gentity_t *ent, uint32_t handle, int paramCount)
{
    Scr_AddExecEntThreadNum(ent->s.number, SCRIPT_OBJECT_ENTITY, handle, paramCount);
}

/* VERIFIED_DECOMPILER(0x725b6, 825b6_Scr_Notify.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - s_number load, entity class id, event/paramCount forwarding, Scr_NotifyNum argument order, and void return checked against current decompiler output. */
void Scr_Notify(gentity_t *ent, uint16_t event, int paramCount)
{
    Scr_NotifyNum(ent->s.number, SCRIPT_OBJECT_ENTITY, event, paramCount);
}

/* VERIFIED_DECOMPILER(0x725f9, 825f9_Scr_GetEnt.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - const-string/string parameter reads, entity class offset lookup, negative/type guards, level entity loop, linked/string/match checks, duplicate-match error, Scr_AddEntity result, and void return checked against current decompiler output. */
void Scr_GetEnt(void)
{
    uint16_t matchValue = Scr_GetConstString(0);
    const char *fieldName = Scr_GetString(1);
    int32_t fieldIndex = coduo_int32_from_bits(Scr_GetOffset(g_scr_data.classMap[SCRIPT_OBJECT_ENTITY].classnum, fieldName));
    const script_entity_field_t *field;

    if (fieldIndex < 0) {
        return;
    }

    field = &script_entity_fields[(uint32_t)fieldIndex];
    if (field->type != SCRIPT_SPAWN_FIELD_STRING) {
        return;
    }

    gentity_t *match = 0;
    level_locals_t *lvl = &level;
    for (int entityNum = 0; entityNum < lvl->num_entities; entityNum++) {
        gentity_t *ent = &g_entities[(uint32_t)entityNum];
        uint16_t value = *game_compat_script_entity_string_slot(ent, field->offset);

        if (ent->linked != 0 && value != 0 && value == matchValue) {
            if (match != 0) {
                Scr_Error("getent used with more than one entity");
            }
            match = ent;
        }
    }

    if (match != 0) {
        Scr_AddEntity(match);
    }
}

/* VERIFIED_DECOMPILER(0x72716, 82716_Scr_GetEntArray.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - parameter-count branch, array creation order, linked-entity enumeration, filtered const-string/string reads, offset/type guards, matching loop, Scr_AddEntity/Scr_AddArray order, and void return checked against current decompiler output. */
void Scr_GetEntArray(void)
{
    level_locals_t *lvl = &level;
    uint32_t paramCount = Scr_GetNumParam();

    if (paramCount == 0) {
        Scr_MakeArray();
        for (int entityNum = 0; entityNum < lvl->num_entities; entityNum++) {
            gentity_t *ent = &g_entities[(uint32_t)entityNum];

            if (ent->linked != 0) {
                Scr_AddEntity(ent);
                Scr_AddArray();
            }
        }
        return;
    }

    uint16_t matchValue = Scr_GetConstString(0);
    const char *fieldName = Scr_GetString(1);
    int32_t fieldIndex = coduo_int32_from_bits(Scr_GetOffset(g_scr_data.classMap[SCRIPT_OBJECT_ENTITY].classnum, fieldName));
    const script_entity_field_t *field;

    if (fieldIndex < 0) {
        return;
    }

    field = &script_entity_fields[(uint32_t)fieldIndex];
    if (field->type != SCRIPT_SPAWN_FIELD_STRING) {
        return;
    }

    Scr_MakeArray();
    for (int entityNum = 0; entityNum < lvl->num_entities; entityNum++) {
        gentity_t *ent = &g_entities[(uint32_t)entityNum];
        uint16_t value = *game_compat_script_entity_string_slot(ent, field->offset);

        if (ent->linked != 0 && value != 0 && value == matchValue) {
            Scr_AddEntity(ent);
            Scr_AddArray();
        }
    }
}

/* VERIFIED_DECOMPILER(0x72872, 82872_GScr_SetDynamicEntityField.c, VERIFY-SCRIPT-FIELDS-2026-06-17): DATAFLOW_VERIFIED - s_number load, entity class id, fieldName forwarding, Scr_SetDynamicEntityField argument order, and void return checked against current decompiler output. */
void GScr_SetDynamicEntityField(gentity_t *ent, uint16_t fieldName)
{
    Scr_SetDynamicEntityField(ent->s.number, SCRIPT_OBJECT_ENTITY, fieldName);
}

/* VERIFIED_DECOMPILER(0x728ae, 828ae_G_SpawnGEntityFromSpawnVars.c, VERIFY-P1-SPAWN-PARSE-2026-06-17): DATAFLOW_VERIFIED - direct G_CallSpawn wrapper checked against current decompiler output. */
gentity_t *G_SpawnGEntityFromSpawnVars(void)
{
    return G_CallSpawn();
}

/* VERIFIED_DECOMPILER(0x728cb, 828cb_G_AddSpawnVarToken.c, VERIFY-P1-SPAWN-PARSE-2026-06-17): DATAFLOW_VERIFIED - token length, text-pool capacity check, memcpy size, spawnTextLength update, and return pointer checked against current decompiler output. */
char *G_AddSpawnVarToken(const char *token)
{
    level_locals_t *lvl = &level;
    uint32_t length = (uint32_t)strlen(token);
    uint32_t copyLength = length + UINT32_C(1);
    int32_t requiredLength = coduo_int32_from_bits(length + (uint32_t)lvl->spawnTextLength + UINT32_C(1));
    char *text = lvl->spawnText;
    char *dest;

    /* 0x728eb..0x72900 performs a wrapped dword sum and signed compare. */
    if (requiredLength > LEVEL_SPAWN_VAR_TEXT_SIZE) {
        G_Error("G_AddSpawnVarToken: MAX_SPAWN_VARS");
    }

    dest = &text[lvl->spawnTextLength];
    memcpy(dest, token, (size_t)copyLength);
    lvl->spawnTextLength = coduo_int32_from_bits((uint32_t)lvl->spawnTextLength + copyLength);
    return dest;
}

/* VERIFIED_DECOMPILER(0x7296a, 8296a_G_ParseSpawnVars.c, VERIFY-P1-SPAWN-PARSE-2026-06-17): DATAFLOW_VERIFIED - reset fields, token parsing, brace/error paths, max-var check, spawn-var pair order, and return values checked against current decompiler output. */
int G_ParseSpawnVars(void)
{
    level_locals_t *lvl = &level;
    char key[MAX_TOKEN_CHARS];
    char value[MAX_TOKEN_CHARS];
    char **pairs = lvl->spawnVarPairSlots;

    lvl->spawnVarCount = 0;
    lvl->spawnTextLength = 0;

    if (trap_GetEntityToken(key, MAX_TOKEN_CHARS) == qfalse) {
        return qfalse;
    }

    if (key[0] != '{') {
        G_Error("G_ParseSpawnVars: found %s when expecting {", key);
    }

    for (;;) {
        if (trap_GetEntityToken(value, MAX_TOKEN_CHARS) == qfalse) {
            G_Error("G_ParseSpawnVars: EOF without closing brace");
        }
        if (value[0] == '}') {
            return qtrue;
        }

        if (trap_GetEntityToken(key, MAX_TOKEN_CHARS) == qfalse) {
            G_Error("G_ParseSpawnVars: EOF without closing brace");
        }
        if (key[0] == '}') {
            G_Error("G_ParseSpawnVars: closing brace without data");
        }

        if (lvl->spawnVarCount == LEVEL_SPAWN_VAR_MAX_PAIRS) {
            G_Error("G_ParseSpawnVars: MAX_SPAWN_VARS");
        }

        /* The first token after "{" is the map key despite this legacy local name. */
        pairs[lvl->spawnVarCount * 2] = G_AddSpawnVarToken(value);
        pairs[lvl->spawnVarCount * 2 + 1] = G_AddSpawnVarToken(key);
        lvl->spawnVarCount++;
    }
}

/* VERIFIED_DECOMPILER(0x72af8, 82af8_SP_worldspawn.c, VERIFY-SCRIPT-BUILTINS-CONSOLE-IP-2026-06-17): DATAFLOW_VERIFIED - spawn key/default sequence, configstring/cvar writes, world entity stores, classname string slot, and linked flag checked against current decompiler output. */
void SP_worldspawn(void)
{
    level_locals_t *lvl = &level;
    const char *value;
    gentity_t *world = &g_entities[ENTITYNUM_WORLD];

    G_SpawnString("classname", SCRIPT_EMPTY_STRING, &value);
    if (Q_stricmp(value, WORLDSPAWN_CLASSNAME) != 0) {
        G_Error("SP_worldspawn: The first entity isn't 'worldspawn'");
    }

    trap_SetConfigstring(WORLDSPAWN_CONFIGSTRING_GAME, WORLDSPAWN_GAME_VALUE);
    trap_SetConfigstring(WORLDSPAWN_CONFIGSTRING_START_TIME, va(WORLDSPAWN_START_TIME_FORMAT, lvl->startTime));

    G_SpawnString("ambienttrack", SCRIPT_EMPTY_STRING, &value);
    if (value[0] == '\0') {
        trap_SetConfigstring(CS_AMBIENT, SCRIPT_EMPTY_STRING);
    } else {
        trap_SetConfigstring(CS_AMBIENT, va(WORLDSPAWN_AMBIENT_FORMAT, value));
    }

    G_SpawnString("message", SCRIPT_EMPTY_STRING, &value);
    trap_SetConfigstring(WORLDSPAWN_CONFIGSTRING_MESSAGE, value);
    /* 0x72c2b loads the exported g_motd object and passes its +0x10 string. */
    trap_SetConfigstring(CS_MOTD, g_motd.string);

    G_SpawnString("gravity", WORLDSPAWN_DEFAULT_GRAVITY, &value);
    trap_Cvar_Set("g_gravity", value);

    G_SpawnString("northyaw", SCRIPT_EMPTY_STRING, &value);
    if (value[0] == '\0') {
        trap_SetConfigstring(WORLDSPAWN_CONFIGSTRING_NORTHYAW, WORLDSPAWN_DEFAULT_ZERO);
    } else {
        trap_SetConfigstring(WORLDSPAWN_CONFIGSTRING_NORTHYAW, value);
    }

    G_SpawnString("spawnflags", WORLDSPAWN_DEFAULT_ZERO, &value);
    world->worldspawnSpawnflagsScratch = atoi(value);
    world->spawnflags = world->worldspawnSpawnflagsScratch;
    world->s.number = ENTITYNUM_WORLD;
    Scr_SetString(game_compat_script_entity_string_slot(world, SPAWN_FIELD_CLASSNAME_OFFSET), scr_const[SCR_CONST_WORLDSPAWN_INDEX]);
    world->linked = 1;
}

/* VERIFIED_DECOMPILER(0x72d61, 82d61_G_SpawnEntitiesFromString.c, VERIFY-P1-SPAWN-PARSE-2026-06-17): DATAFLOW_VERIFIED - spawn-var-active flag, worldspawn parse, entity parse loop, G_SpawnGEntityFromSpawnVars calls, and final flag clear checked against current decompiler output. */
void G_SpawnEntitiesFromString(void)
{
    level_locals_t *lvl = &level;

    game_compat_script_spawn_set_var_active(1);
    lvl->spawnVarCount = 0;

    if (G_ParseSpawnVars() == qfalse) {
        G_Error("SpawnEntities: no entities");
    }

    SP_worldspawn();

    while (G_ParseSpawnVars() != qfalse) {
        G_SpawnGEntityFromSpawnVars();
    }

    game_compat_script_spawn_set_var_active(0);
}

/* VERIFIED_DECOMPILER(0x72ddc, 82ddc_FUN_00082ddc.c, VERIFY-SCRIPT-BUILTINS-CONSOLE-IP-2026-06-17): DATAFLOW_VERIFIED - digit validation, octet parse loop, mask/compare byte stores, bad-address print, and return values checked against current decompiler output. */
qboolean StringToFilter(const char *text, ipFilter_t *filter)
{
    uint8_t mask[IP_FILTER_OCTET_COUNT] = {0};
    uint8_t compare[IP_FILTER_OCTET_COUNT] = {0};
    char octet[IP_FILTER_OCTET_BUFFER_SIZE];

    for (int index = 0; index < IP_FILTER_OCTET_COUNT; index++) {
        if (text[0] < '0' || text[0] > '9') {
            G_Printf("Bad filter address: %s\n", text);
            return qfalse;
        }

        int length = 0;
        while (text[0] >= '0' && text[0] <= '9') {
            /* NOT_FROM_ORIGINAL_SOURCE: require each numeric filter component
             * and its terminator to fit the fixed parser field. */
            if ((size_t)length == sizeof(octet) - 1u) {
                G_Printf("Bad filter address: %s\n", text - length);
                return qfalse;
            }
            octet[length] = text[0];
            length++;
            text++;
        }
        octet[length] = '\0';

        compare[index] = (uint8_t)atoi(octet);
        if (compare[index] != 0) {
            mask[index] = 0xffu;
        }

        if (text[0] == '\0') {
            break;
        }
        text++;
    }

    memcpy(&filter->mask, mask, sizeof(filter->mask));
    memcpy(&filter->compare, compare, sizeof(filter->compare));
    return qtrue;
}

/* VERIFIED_DECOMPILER(0x72f54, 82f54_FUN_00082f54.c, VERIFY-SCRIPT-BUILTINS-CONSOLE-IP-2026-06-17): DATAFLOW_VERIFIED - 1024-byte local buffer, disabled-filter skip, strlen/Com_sprintf append sizing, dotted-octet order, and g_banIPs write checked against original machine code. */
void UpdateIPBans(void)
{
    char cvarValue[MAX_STRING_CHARS];

    cvarValue[0] = '\0';
    for (int index = 0; index < numIPFilters; index++) {
        if (ipFilters[index].compare == UINT32_MAX) {
            continue;
        }

        uint32_t compare = ipFilters[index].compare;
        uint32_t used = (uint32_t)strlen(cvarValue);
        uint32_t remaining = (uint32_t)MAX_STRING_CHARS - used;
        Com_sprintf(&cvarValue[used], (size_t)remaining, "%i.%i.%i.%i ", compare & 0xff, (compare >> 8) & 0xff, (compare >> 16) & 0xff,
                    compare >> 24);
    }

    trap_Cvar_Set(IP_FILTER_CVAR_NAME, cvarValue);
}

/* VERIFIED_DECOMPILER(0x73038, 83038_G_FilterPacket.c, VERIFY-SCRIPT-BUILTINS-CONSOLE-IP-2026-06-17): DATAFLOW_VERIFIED - source IP byte parse, dot/colon termination, filter mask compare, g_filterBan polarity, and return paths checked against current decompiler output. */
qboolean G_FilterPacket(const char *from)
{
    uint32_t in = 0;

    for (int index = 0; from[0] != '\0' && index < IP_FILTER_OCTET_COUNT; index++) {
        uint8_t *octets = (uint8_t *)(void *)&in;

        octets[index] = 0;
        while (from[0] >= '0' && from[0] <= '9') {
            octets[index] = (uint8_t)(octets[index] * 10 + from[0] - '0');
            from++;
        }

        if (from[0] == '\0' || from[0] == ':') {
            break;
        }
        from++;
    }

    for (int index = 0; index < numIPFilters; index++) {
        if ((in & ipFilters[index].mask) == ipFilters[index].compare) {
            return g_filterBan.integer != 0;
        }
    }

    return g_filterBan.integer == 0;
}

/* VERIFIED_DECOMPILER(0x73138, 83138_FUN_00083138.c, VERIFY-SCRIPT-BUILTINS-CONSOLE-IP-2026-06-17): DATAFLOW_VERIFIED - free-slot scan, full-list error, numIPFilters growth, StringToFilter failure tombstone, and UpdateIPBans call checked against current decompiler output. */
void AddIP(const char *ip)
{
    int index;

    for (index = 0; index < numIPFilters; index++) {
        if (ipFilters[index].compare == UINT32_MAX) {
            break;
        }
    }

    if (index == numIPFilters) {
        if (numIPFilters == MAX_IP_FILTERS) {
            G_Printf(IP_FILTER_FULL_MESSAGE);
            return;
        }
        numIPFilters++;
    }

    if (StringToFilter(ip, &ipFilters[index]) == qfalse) {
        ipFilters[index].compare = UINT32_MAX;
    }

    UpdateIPBans();
}

/* VERIFIED_DECOMPILER(0x731df, 831df_G_ProcessIPBans.c, VERIFY-SCRIPT-BUILTINS-CONSOLE-IP-2026-06-17): DATAFLOW_VERIFIED - numIPFilters reset, g_banIPs.string copy, space tokenization, empty-token skip, AddIP calls, and loop exit checked against current decompiler output. */
void G_ProcessIPBans(void)
{
    char banIPs[MAX_STRING_CHARS];
    char *token;
    char *cursor;

    numIPFilters = 0;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    Q_strncpyz(banIPs, g_banIPs.string, sizeof(banIPs));

    cursor = g_banIPs.string;
    while (cursor[0] != '\0') {
        token = cursor;
        cursor = strchr(cursor, ' ');
        if (cursor == 0) {
            break;
        }

        while (cursor[0] == ' ') {
            cursor[0] = '\0';
            cursor++;
        }

        if (token[0] != '\0') {
            AddIP(token);
        }
    }
}

/* VERIFIED_DECOMPILER(0x73294, 83294_Svcmd_AddIP_f.c, VERIFY-SCRIPT-BUILTINS-CONSOLE-IP-2026-06-17): DATAFLOW_VERIFIED - argc guard, usage text, Argv index/size, AddIP call, and void return checked against current decompiler output. */
void Svcmd_AddIP_f(void)
{
    char ip[MAX_STRING_CHARS];

    if (trap_Argc() < 2) {
        G_Printf(IP_FILTER_ADD_USAGE);
    } else {
        trap_Argv(1, ip, sizeof(ip));
        AddIP(ip);
    }
}

/* VERIFIED_DECOMPILER(0x732f8, 832f8_Svcmd_RemoveIP_f.c, VERIFY-SCRIPT-BUILTINS-CONSOLE-IP-2026-06-17): DATAFLOW_VERIFIED - argc guard, usage text, Argv index/size, parsed filter comparison, tombstone store, messages, and UpdateIPBans call checked against current decompiler output. */
void Svcmd_RemoveIP_f(void)
{
    char ip[MAX_STRING_CHARS];
    ipFilter_t filter;

    if (trap_Argc() < 2) {
        G_Printf(IP_FILTER_REMOVE_USAGE);
        return;
    }

    trap_Argv(1, ip, sizeof(ip));
    if (StringToFilter(ip, &filter) == qfalse) {
        return;
    }

    for (int index = 0; index < numIPFilters; index++) {
        if (ipFilters[index].mask == filter.mask && ipFilters[index].compare == filter.compare) {
            ipFilters[index].compare = UINT32_MAX;
            G_Printf(IP_FILTER_REMOVED_MESSAGE);
            UpdateIPBans();
            return;
        }
    }

    G_Printf(IP_FILTER_NOT_FOUND_FORMAT, ip);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_console_entity_type_name). */
static const char *game_compat_console_entity_type_name(int entityType)
{
    switch (entityType) {
    case ET_GENERAL:
        return "ET_GENERAL          ";
    case ET_PLAYER:
        return "ET_PLAYER           ";
    case ET_ITEM:
        return "ET_ITEM             ";
    case ET_MISSILE:
        return "ET_MISSILE          ";
    case ET_MOVER:
        return "ET_MOVER            ";
    case ET_PORTAL:
        return "ET_PORTAL           ";
    case ET_INVISIBLE:
        return "ET_INVISIBLE        ";
    case ET_SCRIPTMOVER:
        return "ET_SCRIPTMOVER      ";
    case ET_SOUND_BLEND:
        return "ET_SOUND_BLEND      ";
    case ET_VEHICLE:
        return "ET_VEHICLE          ";
    default:
        return 0;
    }
}

/* VERIFIED_DECOMPILER(0x733e0, 833e0_Svcmd_EntityList_f.c, VERIFY-SCRIPT-BUILTINS-CONSOLE-IP-2026-06-17): DATAFLOW_VERIFIED - entity stride/loop bound, linked guard, type switch helper, classname/origin print, newline, and total count checked against current decompiler output. */
void Svcmd_EntityList_f(void)
{
    level_locals_t *lvl = &level;
    int usedCount = 0;

    for (int entityNum = 1; entityNum < lvl->num_entities; entityNum++) {
        gentity_t *ent = &g_entities[entityNum];
        const char *typeName;

        if (ent->linked == 0) {
            continue;
        }

        usedCount++;
        G_Printf(CONSOLE_ENTITY_INDEX_FORMAT, entityNum);

        typeName = game_compat_console_entity_type_name(ent->s.eType);
        if (typeName != 0) {
            G_Printf(typeName);
        } else {
            G_Printf(CONSOLE_ENTITY_TYPE_FORMAT, ent->s.eType);
        }

        if (ent->scriptClassname != 0) {
            G_Printf(CONSOLE_ENTITY_ORIGIN_FORMAT, SL_ConvertToString(ent->scriptClassname), (double)ent->currentOrigin[0],
                     (double)ent->currentOrigin[1], (double)ent->currentOrigin[2]);
        }

        G_Printf(CONSOLE_ENTITY_LINE_END);
    }

    G_Printf(CONSOLE_ENTITY_TOTAL_FORMAT, usedCount);
}

/* VERIFIED_DECOMPILER(0x735cb, 835cb_ClientForString.c, VERIFY-SCRIPT-BUILTINS-CONSOLE-IP-2026-06-17): DATAFLOW_VERIFIED - numeric/name branches, client bounds, connected-state checks, name comparison, diagnostic text, and return values checked against current decompiler output. */
gclient_t *ClientForString(const char *text)
{
    if (text[0] >= '0' && text[0] <= '9') {
        int clientNum = atoi(text);
        gclient_t *client;

        if (clientNum < 0 || clientNum >= level.maxclients) {
            Com_Printf(CONSOLE_BAD_CLIENT_SLOT_FORMAT, clientNum);
            return 0;
        }

        client = &level.clients[clientNum];
        if (client->connectedState == 0) {
            G_Printf(CONSOLE_CLIENT_NOT_CONNECTED_FORMAT, clientNum);
            return 0;
        }

        return client;
    }

    for (int clientNum = 0; clientNum < level.maxclients; clientNum++) {
        gclient_t *client = &level.clients[clientNum];

        if (client->connectedState != 0 && Q_stricmp(client->userInfoName, text) == 0) {
            return client;
        }
    }

    G_Printf(CONSOLE_USER_NOT_FOUND_FORMAT, text);
    return 0;
}

/* VERIFIED_DECOMPILER(0x73714, 83714_ConsoleCommand.c, VERIFY-SCRIPT-BUILTINS-CONSOLE-IP-2026-06-17): DATAFLOW_VERIFIED - command buffer Argv, entitylist/addip/removeip/listip dispatch, dedicated say path, server command arguments, and return values checked against current decompiler output. */
int ConsoleCommand(void)
{
    char cmd[MAX_STRING_CHARS];

    trap_Argv(0, cmd, sizeof(cmd));

    if (Q_stricmp(cmd, "entitylist") == 0) {
        Svcmd_EntityList_f();
        return 1;
    }

    if (Q_stricmp(cmd, "addip") == 0) {
        Svcmd_AddIP_f();
        return 1;
    }

    if (Q_stricmp(cmd, "removeip") == 0) {
        Svcmd_RemoveIP_f();
        return 1;
    }

    if (Q_stricmp(cmd, "listip") == 0) {
        trap_SendConsoleCommand(CONSOLE_LISTIP_EXEC_TIME, CONSOLE_LISTIP_COMMAND);
        return 1;
    }

    if (g_dedicated.integer != 0 && Q_stricmp(cmd, CONSOLE_SAY_COMMAND) == 0) {
        trap_SendServerCommand(SERVER_COMMAND_ALL_CLIENTS, CONSOLE_SAY_RELIABLE, va(CONSOLE_SAY_FORMAT, ConcatArgs(1)));
        return 1;
    }

    return 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_entity_field_set_read_only). */
static void game_compat_script_entity_field_set_read_only(gentity_t *ent, uint32_t fieldIndex)
{
    (void)ent;
    (void)fieldIndex;
    Scr_SetEntityFieldReadOnly();
}

/* NOT_FROM_ORIGINAL_SOURCE: C type adapter for the original entity-field
 * table entry, which dispatches origin writes to Scr_SetOrigin. */
static void game_compat_script_entity_field_set_origin(gentity_t *ent, uint32_t fieldIndex)
{
    (void)fieldIndex;
    Scr_SetOrigin(ent);
}

/* NOT_FROM_ORIGINAL_SOURCE: C type adapter for the original entity-field
 * table entry, which dispatches angle writes to Scr_SetAngles. */
static void game_compat_script_entity_field_set_angles(gentity_t *ent, uint32_t fieldIndex)
{
    (void)fieldIndex;
    Scr_SetAngles(ent);
}

/* NOT_FROM_ORIGINAL_SOURCE: C type adapter for the original entity-field
 * table entry, which dispatches health writes to Scr_SetHealth. */
static void game_compat_script_entity_field_set_health(gentity_t *ent, uint32_t fieldIndex)
{
    (void)fieldIndex;
    Scr_SetHealth(ent);
}

/* VERIFIED_DECOMPILER(0x67885, 77885_script_func_spawn.c, VERIFY-WAVE2-SCRIPT-SPAWNFREE-2026-06-17): DATAFLOW_VERIFIED - classname/origin/spawnflags parameter handling, entity allocation/stores, G_CallSpawnEntity branch, error formatting, and Scr_AddEntity path checked against current decompiler output. */
void script_func_spawn(void)
{
    uint16_t classname = Scr_GetConstString(0);
    vec3_t origin;
    int spawnflags = 0;
    gentity_t *ent;

    Scr_GetVector(1, origin);
    if (Scr_GetNumParam() >= 3) {
        spawnflags = Scr_GetInt(2);
    }

    ent = G_Spawn();
    Scr_SetString(&ent->scriptClassname, classname);
    game_compat_script_copy_vector(ent->currentOrigin, origin);
    ent->spawnflags = spawnflags;

    if (!G_CallSpawnEntity(ent)) {
        Scr_Error(va(SCRIPT_SPAWN_ERROR_FORMAT, SL_ConvertToString(classname)));
        return;
    }

    Scr_AddEntity(ent);
}

/* VERIFIED_DECOMPILER(0x67974, 77974_script_func_spawntriggermount.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; vectors, min/max swap loop, trigger classname store, spawn call, and entity return checked against current decompiler output. */
void script_func_spawntriggermount(void)
{
    vec3_t mins;
    vec3_t maxs;
    qboolean largeTrigger;
    gentity_t *ent;

    Scr_GetVector(0, mins);
    Scr_GetVector(1, maxs);
    largeTrigger = Scr_GetBool(2);

    for (uint32_t axis = 0; axis < 3; axis++) {
        if (maxs[axis] < mins[axis]) {
            float swap = mins[axis];
            mins[axis] = maxs[axis];
            maxs[axis] = swap;
        }
    }

    ent = G_Spawn();
    Scr_SetString(&ent->scriptClassname, scr_const_trigger_mount);
    game_compat_script_copy_vector(ent->mins, mins);
    game_compat_script_copy_vector(ent->maxs, maxs);
    SP_trigger_mount_no_brush(ent, largeTrigger);
    Scr_AddEntity(ent);
}

/* VERIFIED_DECOMPILER(0x67aa0, 77aa0_script_func_spawnturret.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; classname/origin stores, weapon-name fetch, turret spawn call, and entity return checked against current decompiler output. */
void script_func_spawnturret(void)
{
    uint16_t classname = Scr_GetConstString(0);
    vec3_t origin;
    const char *weaponName;
    gentity_t *ent;

    Scr_GetVector(1, origin);
    weaponName = Scr_GetString(2);

    ent = G_Spawn();
    Scr_SetString(&ent->scriptClassname, classname);
    game_compat_script_copy_vector(ent->currentOrigin, origin);
    G_SpawnTurret(ent, weaponName);
    Scr_AddEntity(ent);
}

/* VERIFIED_DECOMPILER(0x67b4b, 77b4b_script_func_precacheturret.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; precache phase guard, low-byte weapon index behavior, and RegisterItem call checked against current decompiler output. */
void script_func_precacheturret(void)
{
    const char *weaponName = Scr_GetString(0);
    int weapon;

    if (level.spawning == 0) {
        Scr_Error(SCRIPT_PRECACHE_TURRET_INIT_ERROR);
    }

    weapon = game_compat_script_weapon_index_for_name(weaponName);
    if (weapon != 0) {
        RegisterItem(weapon, 1);
    }
}

/* VERIFIED_DECOMPILER(0x6a74c, 7a74c_script_func_logprint.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; 1024-byte buffer, signed accumulated length check, strcat loop, and G_LogPrintf call checked against current decompiler output. */
void script_func_logprint(void)
{
    char buffer[MAX_STRING_CHARS];
    int total = 0;
    int32_t paramCount = coduo_int32_from_bits(Scr_GetNumParam());
    int32_t index;

    buffer[0] = '\0';

    for (index = 0; index < paramCount; index++) {
        const char *text = Scr_GetString(index);
        uint32_t length = (uint32_t)strlen(text);
        int32_t combinedLength = coduo_int32_from_bits(length + (uint32_t)total);

        if (combinedLength > (int32_t)(MAX_STRING_CHARS - 1u)) {
            break;
        }

        strcat(buffer, text);
        total = coduo_int32_from_bits((uint32_t)total + length);
    }

    /* NOT_FROM_ORIGINAL_SOURCE: forward script-built log text as data through
     * a literal conversion. */
    G_LogPrintf("%s", buffer);
}

/* VERIFIED_DECOMPILER(0x6a7f5, 7a7f5_script_func_worldentnumber.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; world entity-number return checked against current decompiler output. */
void script_func_worldentnumber(void)
{
    Scr_AddInt(ENTITYNUM_WORLD);
}

/* VERIFIED_DECOMPILER(0x6a819, 7a819_script_func_obituary.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; obituary cvar guard, weapon/MOD lookup, temp-event fields, attacker fallback, and MOD eventparm set checked against current decompiler output. */
void script_func_obituary(void)
{
    int weapon;
    int meansOfDeath;
    gentity_t *victim;
    gentity_t *temp;

    if (g_obituary.integer == 0) {
        return;
    }

    weapon = BG_GetWeaponIndexForName(Scr_GetString(2));
    meansOfDeath = G_IndexForMeansOfDeath(Scr_GetString(3));
    victim = Scr_GetEntity(0);

    temp = G_TempEntity(vec3_origin, OBITUARY_TEMP_EVENT);
    temp->s.vehicleEntityNum = victim->s.number;

    if (Scr_GetType(1) == SCRIPT_VAR_OBJECT && Scr_GetPointerType(1) == SCRIPT_VAR_ENTITY) {
        gentity_t *attacker = Scr_GetEntity(1);

        temp->s.vehicleSlot = attacker->s.number;
    } else {
        temp->s.vehicleSlot = ENTITYNUM_WORLD;
    }

    temp->svFlags = OBITUARY_TEMP_FLAG;

    if (game_compat_script_obituary_uses_means_of_death_eventparm(meansOfDeath)) {
        temp->s.tempEffectId = meansOfDeath | OBITUARY_MOD_EVENT_FLAG;
    } else {
        temp->s.tempEffectId = weapon & 0xff;
    }
}

/* VERIFIED_DECOMPILER(0x6abc2, 7abc2_script_func_getstarttime.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; level start-time return checked against current decompiler output. */
void script_func_getstarttime(void)
{
    Scr_AddInt(level.startTime);
}

/* VERIFIED_DECOMPILER(0x6a964, 7a964_script_func_positionwouldtelefrag.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; player bounds, two trap_EntitiesInBox masks, client pmType gate, corpse/entity-type gate, and returns checked against current decompiler output. */
void script_func_positionwouldtelefrag(void)
{
    int entityNumbers[POSITION_TELEFRAG_MAX_ENTITIES];
    vec3_t origin;
    vec3_t mins;
    vec3_t maxs;
    int count;
    int index;

    Scr_GetVector(0, origin);
    game_compat_script_builtin_make_player_box(origin, mins, maxs);

    count = trap_EntitiesInBox(mins, maxs, entityNumbers, POSITION_TELEFRAG_MAX_ENTITIES, POSITION_TELEFRAG_PLAYER_CONTENTS);
    for (index = 0; index < count; index++) {
        gentity_t *ent = script_object_to_gentity((uint32_t)entityNumbers[index]);

        if (ent->client != 0 && ent->client->ps.pmType < PM_TYPE_DEAD) {
            Scr_AddInt(1);
            return;
        }
    }

    game_compat_script_builtin_make_player_box(origin, mins, maxs);
    count = trap_EntitiesInBox(mins, maxs, entityNumbers, POSITION_TELEFRAG_MAX_ENTITIES, POSITION_TELEFRAG_SECONDARY_CONTENTS);
    for (index = 0; index < count; index++) {
        gentity_t *ent = script_object_to_gentity((uint32_t)entityNumbers[index]);

        if (ent->s.eType == POSITION_TELEFRAG_SECONDARY_ENTITY_TYPE) {
            Scr_AddInt(1);
            return;
        }
    }

    Scr_AddInt(0);
}

/* VERIFIED_DECOMPILER(0x6abee, 7abee_script_func_precachemenu.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; menu configstring duplicate scan, empty-slot scan, overflow error, and set call checked against current decompiler output. */
void script_func_precachemenu(void)
{
    game_compat_script_precache_configstring(Scr_GetString(0), SCRIPT_MENU_CONFIGSTRING_BASE, SCRIPT_MENU_CONFIGSTRING_COUNT,
                                             "Script tried to precache the menu '%s' more than once\n",
                                             "Too many menus precached. Max allowed menus is %i");
}

/* VERIFIED_DECOMPILER(0x6ad04, 7ad04_GScr_GetScriptMenuIndex.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; menu configstring lookup loop, error path, and zero-based return checked against current decompiler output. */
int GScr_GetScriptMenuIndex(const char *menuName)
{
    return game_compat_script_find_configstring_index(menuName, SCRIPT_MENU_CONFIGSTRING_BASE, SCRIPT_MENU_CONFIGSTRING_COUNT,
                                                      "Menu '%s' was not precached\n", 0, 0);
}

/* VERIFIED_DECOMPILER(0x6adab, 7adab_script_func_precachestatusicon.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; status-icon configstring duplicate scan, empty-slot scan, overflow error, and set call checked against current decompiler output. */
void script_func_precachestatusicon(void)
{
    game_compat_script_precache_configstring(Scr_GetString(0), STATUS_ICON_CONFIGSTRING_BASE, STATUS_ICON_CONFIGSTRING_COUNT,
                                             "Script tried to precache the player status icon '%s' more than once\n",
                                             "Too many player status icons precached. Max allowed is %i");
}

/* VERIFIED_DECOMPILER(0x6aebb, 7aebb_GScr_GetStatusIconIndex.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; empty-name return, status-icon lookup loop, error path, and one-based return checked against current decompiler output. */
int GScr_GetStatusIconIndex(const char *name)
{
    return game_compat_script_find_configstring_index(name, STATUS_ICON_CONFIGSTRING_BASE, STATUS_ICON_CONFIGSTRING_COUNT,
                                                      "Status icon '%s' was not precached\n", 0, 1);
}

/* VERIFIED_DECOMPILER(0x6af78, 7af78_script_func_precacheheadicon.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; head-icon configstring duplicate scan, empty-slot scan, overflow error, and set call checked against current decompiler output. */
void script_func_precacheheadicon(void)
{
    game_compat_script_precache_configstring(Scr_GetString(0), HEAD_ICON_CONFIGSTRING_BASE, HEAD_ICON_CONFIGSTRING_COUNT,
                                             "Script tried to precache the player head icon '%s' more than once\n",
                                             "Too many player head icons precached. Max allowed is %i");
}

/* VERIFIED_DECOMPILER(0x6b088, 7b088_GScr_GetHeadIconIndex.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; empty-name return, head-icon lookup loop, error path, and one-based return checked against current decompiler output. */
int GScr_GetHeadIconIndex(const char *name)
{
    return game_compat_script_find_configstring_index(name, HEAD_ICON_CONFIGSTRING_BASE, HEAD_ICON_CONFIGSTRING_COUNT,
                                                      "Head icon '%s' was not precached\n", 0, 1);
}

/* VERIFIED_DECOMPILER(0x6b145, 7b145_script_func_bullettrace.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; trace mask/pass entity handling, LocationalTrace call, array keys, entity/position fallback, and surface-type extraction checked against current decompiler output. */
void script_func_bullettrace(void)
{
    trace_t trace;
    vec3_t start;
    vec3_t end;
    uint32_t contentMask = BULLETTRACE_MASK;
    int passEntityNum = ENTITYNUM_NONE;
    uint32_t surfaceType;

    Scr_GetVector(0, start);
    Scr_GetVector(1, end);

    if (!Scr_GetBool(2)) {
        contentMask &= ~BULLETTRACE_CHARACTER_CONTENTS;
    }

    if (Scr_GetType(3) == SCRIPT_VAR_OBJECT && Scr_GetPointerType(3) == SCRIPT_VAR_ENTITY) {
        gentity_t *passEntity = Scr_GetEntity(3);

        passEntityNum = passEntity->s.number;
    }

    trap_LocationalTrace(&trace, start, end, passEntityNum, contentMask, bulletPriorityMap);

    Scr_MakeArray();
    Scr_AddFloat(trace.fraction);
    Scr_AddArrayStringIndexed(scr_const_fraction);

    Scr_AddVector(trace.normal);
    Scr_AddArrayStringIndexed(scr_const_normal);

    game_compat_script_bullettrace_add_entity(trace.entityNum);
    Scr_AddArrayStringIndexed(scr_const_entity);

    game_compat_script_bullettrace_add_position_or_direction(&trace, start, end);
    Scr_AddArrayStringIndexed(scr_const_position);

    if (trace.fraction < 1.0f) {
        surfaceType = (trace.surfaceFlags & (SURFACE_TYPE_MASK << SURFACE_TYPE_SHIFT)) >> SURFACE_TYPE_SHIFT;
        Scr_AddString(trap_SurfaceTypeToName((int)surfaceType));
    } else {
        Scr_AddConstString(scr_const_none);
    }
    Scr_AddArrayStringIndexed(scr_const_surfacetype);
}

/* VERIFIED_DECOMPILER(0x6c110, 7c110_script_func_grenadeexplosioneffect.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; event origin, temp-event fields, up-vector DirToByte, downward trace, and surface eventParm checked against current decompiler output. */
void script_func_grenadeexplosioneffect(void)
{
    trace_t trace;
    vec3_t origin;
    vec3_t eventOrigin;
    vec3_t traceEnd;
    vec3_t up = {0.0f, 0.0f, 1.0f};
    gentity_t *temp;

    Scr_GetVector(0, origin);
    game_compat_script_copy_vector(eventOrigin, origin);
    eventOrigin[2] += 1.0f;

    temp = G_TempEntity(eventOrigin, GRENADE_EXPLOSION_TEMP_EVENT);
    temp->s.hintStringIndex = GRENADE_EXPLOSION_TEMP_DC_VALUE;
    temp->s.tempEffectId = DirToByte(up) & 0xff;

    game_compat_script_copy_vector(traceEnd, eventOrigin);
    traceEnd[2] -= GRENADE_EXPLOSION_TRACE_DROP;

    trap_Trace(&trace, eventOrigin, vec3_origin, vec3_origin, traceEnd, ENTITYNUM_NONE, GRENADE_EXPLOSION_TRACE_CONTENTS);
    temp->s.surfType = (int)((trace.surfaceFlags & (SURFACE_TYPE_MASK << SURFACE_TYPE_SHIFT)) >> SURFACE_TYPE_SHIFT);
}

/* VERIFIED_DECOMPILER(0x6c229, 7c229_script_func_radiusdamage.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; vector/float parameter order, repeated Scr_GetNumParam checks, attacker fallback, radius-damage call, and ignore-player globals checked against current decompiler output. */
void script_func_radiusdamage(void)
{
    vec3_t origin;
    float radius;
    float damage;
    float minDamage;
    gentity_t *attacker = 0;
    gentity_t *inflictor = 0;

    Scr_GetVector(0, origin);
    radius = Scr_GetFloat(1);
    damage = Scr_GetFloat(2);
    minDamage = Scr_GetFloat(3);

    if (Scr_GetNumParam() > 4) {
        attacker = Scr_GetEntity(4);
    }
    if (Scr_GetNumParam() > 5) {
        inflictor = Scr_GetEntity(5);
    }

    level.radiusDamageIgnorePlayersActive = level.radiusDamageIgnorePlayersSetting;
    if (attacker == 0) {
        attacker = &g_entities[ENTITYNUM_WORLD];
    }

    G_RadiusDamage(origin, inflictor, attacker, damage, minDamage, radius, 0, MOD_EXPLOSIVE);
    level.radiusDamageIgnorePlayersActive = 0;
}

/* VERIFIED_DECOMPILER(0x6c348, 7c348_script_func_setplayerignoreradiusdamage.c, VERIFY-SCRIPT-BUILTINS-TRIGGER-MENU-TRACE-2026-06-17): DATAFLOW_VERIFIED; bool fetch and ignore-player setting store checked against current decompiler output. */
void script_func_setplayerignoreradiusdamage(void)
{
    level.radiusDamageIgnorePlayersSetting = Scr_GetBool(0);
}

/* VERIFIED_DECOMPILER(0x6d7e0, 7d7e0_GScr_GetNumParts.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; model string fetch, XModel lookup, bone count, and integer return checked against current decompiler output. */
void GScr_GetNumParts(void)
{
    XModel *model = trap_XModelGet(Scr_GetString(0));

    Scr_AddInt(trap_XModelNumBones(model));
}

/* VERIFIED_DECOMPILER(0x6d822, 7d822_GScr_GetPartName.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; model lookup, unsigned part index range check, bone-name fetch, bad-model error path, and const-string return checked against current decompiler output. */
void GScr_GetPartName(void)
{
    XModel *model = trap_XModelGet(Scr_GetString(0));
    uint32_t partIndex = (uint32_t)Scr_GetInt(1);
    int partCount = trap_XModelNumBones(model);
    const uint16_t *boneNames;
    uint16_t partName;

    if ((uint32_t)partCount <= partIndex) {
        Scr_ParamError(1, va(SCRIPT_PART_INDEX_RANGE, partCount - 1));
    }

    boneNames = trap_XModelGetBoneNames(model);
    partName = boneNames[partIndex];
    if (partName == 0) {
        Scr_ParamError(0, SCRIPT_PART_BAD_MODEL);
    }

    Scr_AddConstString(partName);
}

/* VERIFIED_DECOMPILER(0x6c37a, 7c37a_script_func_getmovedelta.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; optional time-range helper behavior, Scr_GetAnim arguments, XAnim rel-delta call, and move-vector return checked against current decompiler output. */
void script_func_getmovedelta(void)
{
    scr_anim_t animRef;
    uint32_t anim[2];
    vec3_t rotationDelta;
    float moveDelta[4];
    float startTime;
    float endTime;

    game_compat_script_get_anim_delta_time_range(&startTime, &endTime);
    animRef = Scr_GetAnim(0, NULL);
    anim[0] = ((uint32_t)animRef.treeIndex << SCR_ANIM_TREE_INDEX_SHIFT) | animRef.animIndex;
    trap_XAnimGetRelDelta(anim[0], rotationDelta, moveDelta, startTime, endTime);
    Scr_AddVector(moveDelta);
}

/* VERIFIED_DECOMPILER(0x6c499, 7c499_script_func_getangledelta.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; optional time-range helper behavior, Scr_GetAnim arguments, XAnim rel-delta call, RotationToYaw, and float return checked against current decompiler output. */
void script_func_getangledelta(void)
{
    scr_anim_t animRef;
    uint32_t anim[2];
    vec3_t rotationDelta;
    float moveDelta[4];
    float startTime;
    float endTime;

    game_compat_script_get_anim_delta_time_range(&startTime, &endTime);
    animRef = Scr_GetAnim(0, NULL);
    anim[0] = ((uint32_t)animRef.treeIndex << SCR_ANIM_TREE_INDEX_SHIFT) | animRef.animIndex;
    trap_XAnimGetRelDelta(anim[0], rotationDelta, moveDelta, startTime, endTime);
    Scr_AddFloat(RotationToYaw(rotationDelta));
}

/* VERIFIED_DECOMPILER(0x6c5c0, 7c5c0_script_func_loadfx.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; effect lookup, command-item-spawn error gate, and integer return checked against current decompiler output. */
void script_func_loadfx(void)
{
    int effectId = G_EffectIndex(Scr_GetString(0));

    if (effectId == 0 && level.spawning == 0) {
        Scr_Error(SCRIPT_LOADFX_ERROR);
    }

    Scr_AddInt(effectId);
}

/* VERIFIED_DECOMPILER(0x6c620, 7c620_script_func_rewindfx.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; parameter-count error fallthrough, entity/time reads, 1000 multiplier, server command target, and command formatting checked against current decompiler output. */
void script_func_rewindfx(void)
{
    gentity_t *ent;
    int rewindTime;

    if (Scr_GetNumParam() != 2) {
        Scr_Error(SCRIPT_REWINDFX_USAGE);
    }

    ent = Scr_GetEntity(0);
    rewindTime = coduo_int32_from_bits((uint32_t)Scr_GetInt(1) * (uint32_t)SCRIPT_REWINDFX_TIME_SCALE);
    trap_SendServerCommand((uint32_t)ent->s.number, UNRELIABLE_SERVER_COMMAND, va(SCRIPT_REWINDFX_COMMAND, rewindTime));
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_get_effect_configstring_name). */
static void game_compat_script_get_effect_configstring_name(int effectId, char *buffer, uint32_t bufferSize)
{
    if (effectId == 0) {
        strcpy(buffer, SCRIPT_PLAYFX_UNLOADED);
    } else {
        trap_GetConfigstring(effectId + SCRIPT_FX_CONFIGSTRING_BASE, buffer, (int)bufferSize);
    }
}

/* VERIFIED_DECOMPILER(0x6c6a2, 7c6a2_script_func_playfx.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; repeated parameter-count checks, zero-forward error fallthrough, effect-name lookup helper, temp-event selection, effect byte, and DirToByte payload checked against current decompiler output. */
void script_func_playfx(void)
{
    uint32_t paramCount = Scr_GetNumParam();
    int effectId;
    vec3_t origin;
    gentity_t *temp;

    if (paramCount < 2 || Scr_GetNumParam() > 3) {
        Scr_Error(SCRIPT_PLAYFX_USAGE);
    }

    effectId = Scr_GetInt(0);
    Scr_GetVector(1, origin);

    if (Scr_GetNumParam() == 3) {
        char effectName[MAX_STRING_CHARS];
        vec3_t forward;

        Scr_GetVector(2, forward);
        if (VectorNormalize(forward) == 0.0f) {
            game_compat_script_get_effect_configstring_name(effectId, effectName, sizeof(effectName));
            Scr_Error(va(SCRIPT_PLAYFX_ZERO_FORWARD, effectName));
        }

        temp = G_TempEntity(origin, EV_PLAY_FX_DIR);
        temp->s.tempEffectId = (uint32_t)effectId & 0xff;
        temp->s.hintStringIndex = (uint32_t)DirToByte(forward) & 0xff;
        return;
    }

    temp = G_TempEntity(origin, EV_PLAY_FX);
    temp->s.tempEffectId = (uint32_t)effectId & 0xff;
}

/* VERIFIED_DECOMPILER(0x6cabe, 7cabe_script_func_playloopedfx.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; repeated parameter-count checks, optional cull/forward handling, zero-forward error fallthrough, repeat-delay biased round, looped-FX entity fields, link, and return entity checked against current decompiler output. */
void script_func_playloopedfx(void)
{
    uint32_t paramCount = Scr_GetNumParam();
    int effectId;
    vec3_t origin;
    vec3_t forward = {0.0f, 0.0f, 0.0f};
    float cullDistance = 0.0f;
    int repeatDelay;
    gentity_t *ent;

    if (paramCount < 3 || Scr_GetNumParam() > 5) {
        Scr_Error(SCRIPT_PLAYLOOPEDFX_USAGE);
    }

    effectId = Scr_GetInt(0);
    paramCount = Scr_GetNumParam();

    if (paramCount == 5) {
        char effectName[MAX_STRING_CHARS];

        Scr_GetVector(4, forward);
        if (VectorNormalize(forward) == 0.0f) {
            game_compat_script_get_effect_configstring_name(effectId, effectName, sizeof(effectName));
            Scr_Error(va(SCRIPT_PLAYLOOPEDFX_ZERO_FORWARD, effectName));
        }
    }

    if (paramCount == 4 || paramCount == 5) {
        cullDistance = Scr_GetFloat(3);
    }

    Scr_GetVector(2, origin);
    repeatDelay = Script_BiasedRoundToInt(Scr_GetFloat(1) * SCRIPT_SECONDS_TO_MILLISECONDS);

    ent = G_Spawn();
    ent->s.eType = ET_LOOPED_FX;
    ent->svFlags |= LOOPED_FX_ENTITY_FLAG;
    ent->s.hintStringIndex = (uint32_t)effectId & 0xff;
    G_SetOrigin(ent, origin);
    game_compat_script_copy_vector(ent->s.loopedFxForward, forward);
    ent->s.loopedFx.cullDistance = cullDistance;
    ent->s.loopedFx.repeatDelayMs = (float)repeatDelay;
    trap_LinkEntity(ent);
    Scr_AddEntity(ent);
}

/* VERIFIED_DECOMPILER(0x6c808, 7c808_script_func_playfxontag.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; parameter-count/id/model/tag validation fallthrough, DObj bone lookup, dump-on-missing, configstring index format, and FX tag event checked against current decompiler output. */
void script_func_playfxontag(void)
{
    int effectId;
    gentity_t *ent;
    const char *tagName;
    int eventParm;

    if (Scr_GetNumParam() != 3) {
        Scr_Error(SCRIPT_PLAYFXONTAG_USAGE);
    }

    effectId = Scr_GetInt(0);
    if (effectId < 1 || effectId > SCRIPT_FX_ID_MAX) {
        Scr_ParamError(0, va(SCRIPT_INVALID_EFFECT_ID, effectId));
    }

    ent = Scr_GetEntity(1);
    if (ent->modelIndex == 0) {
        Scr_ParamError(1, SCRIPT_PLAYFXONTAG_NO_MODEL);
    }

    tagName = Scr_GetString(2);
    if (strchr(tagName, '"') != 0) {
        Scr_ParamError(2, SCRIPT_PLAYFXONTAG_QUOTE_ERROR);
    }

    if (trap_DObjGetBoneIndex(ent, tagName) < 0) {
        trap_DObjDumpInfo(ent);
        Scr_ParamError(2, va(SCRIPT_PLAYFXONTAG_MISSING_TAG, tagName, G_ModelName(ent->modelIndex)));
    }

    eventParm = G_FindConfigstringIndex(va(SCRIPT_FX_ON_TAG_CONFIGSTRING_FORMAT, effectId, tagName), SCRIPT_FX_TAG_CONFIGSTRING_BASE,
                                        SCRIPT_FX_TAG_CONFIGSTRING_COUNT, 1, 0);
    G_AddEvent(ent, EV_PLAY_FX_ON_TAG, eventParm);
}

/* VERIFIED_DECOMPILER(0x6c9a4, 7c9a4_script_func_playfxonplayer.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; effect-id validation fallthrough, stack sprintf buffer, and cg_atmos cvar set checked against current decompiler output. */
void script_func_playfxonplayer(void)
{
    int effectId = Scr_GetInt(0);
    char effectIdText[SCRIPT_ATMOS_EFFECT_ID_BUFFER_SIZE];

    if (effectId > SCRIPT_FX_ID_MAX) {
        Scr_ParamError(0, va(SCRIPT_INVALID_EFFECT_ID, effectId));
    }

    sprintf(effectIdText, "%d", effectId);
    trap_Cvar_Set(SCRIPT_ATMOS_CVAR, effectIdText);
}

/* VERIFIED_DECOMPILER(0x6ca27, 7ca27_script_func_setwind.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; repeated parameter-count checks, vector/strength reads, configstring format, and wind configstring store checked against current decompiler output. */
void script_func_setwind(void)
{
    uint32_t paramCount = Scr_GetNumParam();
    vec3_t angles;
    float strength;

    if (paramCount == 0 || Scr_GetNumParam() > 2) {
        Scr_Error(SCRIPT_SETWIND_USAGE);
    }

    Scr_GetVector(0, angles);
    strength = Scr_GetFloat(1);
    trap_SetConfigstring(SCRIPT_WIND_CONFIGSTRING,
                         va(SCRIPT_WIND_CONFIGSTRING_FORMAT, (double)angles[0], (double)angles[1], (double)angles[2], (double)strength));
}

/* VERIFIED_DECOMPILER(0x6cca8, 7cca8_FUN_0007cca8.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; validation error fallthroughs, fog format, transition-ms conversion, and G_setfog call checked against current decompiler output. */
static void Scr_SetFog(const char *caller, float nearDistance, float farDistance, float density, float red, float green, float blue,
                       float transitionSeconds)
{
    if (nearDistance < 0.0f) {
        Scr_Error(va("%s: near distance must be >= 0", caller));
    }
    if (farDistance <= nearDistance) {
        Scr_Error(va("%s: near distance must be less than far distance", caller));
    }
    if (red < 0.0f || red > 1.0f || green < 0.0f || green > 1.0f || blue < 0.0f || blue > 1.0f) {
        Scr_Error(va("%s: red/green/blue color components must be in the range [0, 1]", caller));
    }
    if (transitionSeconds < 0.0f) {
        Scr_Error(va("%s: transition time must be >= 0 seconds", caller));
    }

    G_setfog(va(SCRIPT_FOG_FORMAT, (double)nearDistance, (double)farDistance, (double)density, (double)red, (double)green, (double)blue,
                (double)((long double)transitionSeconds * (long double)SCRIPT_SECONDS_TO_MILLISECONDS)));
}

/* VERIFIED_DECOMPILER(0x6ce1e, 7ce1e_script_func_setcullfog.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; count error fallthrough, six float reads, density constant, and Scr_SetFog argument order checked against current decompiler output. */
void Scr_SetLinearFog(void)
{
    float nearDistance;
    float farDistance;
    float red;
    float green;
    float blue;
    float transitionSeconds;

    if (Scr_GetNumParam() != 6) {
        Scr_Error(SCRIPT_SETCULLFOG_USAGE);
    }

    nearDistance = Scr_GetFloat(0);
    farDistance = Scr_GetFloat(1);
    red = Scr_GetFloat(2);
    green = Scr_GetFloat(3);
    blue = Scr_GetFloat(4);
    transitionSeconds = Scr_GetFloat(5);

    Scr_SetFog("setCullFog", nearDistance, farDistance, 1.0f, red, green, blue, transitionSeconds);
}

/* VERIFIED_DECOMPILER(0x6cee8, 7cee8_script_func_setexpfog.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; count/density error fallthroughs, five float reads, near/far constants, and Scr_SetFog argument order checked against current decompiler output. */
void Scr_SetExponentialFog(void)
{
    float density;
    float red;
    float green;
    float blue;
    float transitionSeconds;

    if (Scr_GetNumParam() != 5) {
        Scr_Error(SCRIPT_SETEXPFOG_USAGE);
    }

    density = Scr_GetFloat(0);
    red = Scr_GetFloat(1);
    green = Scr_GetFloat(2);
    blue = Scr_GetFloat(3);
    transitionSeconds = Scr_GetFloat(4);

    if (density <= 0.0f || density >= 1.0f) {
        Scr_Error(SCRIPT_SETEXPFOG_DENSITY_ERROR);
    }

    Scr_SetFog("setExpFog", 0.0f, 1.0f, density, red, green, blue, transitionSeconds);
}

/* VERIFIED_DECOMPILER(0x6cfd6, 7cfd6_script_func_isplayer.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; pointer/entity type gates, client pointer test, and integer returns checked against current decompiler output. */
void script_func_isplayer(void)
{
    if (Scr_GetType(0) == SCRIPT_VAR_OBJECT && Scr_GetPointerType(0) == SCRIPT_VAR_ENTITY && Scr_GetEntity(0)->client != 0) {
        Scr_AddInt(1);
        return;
    }

    Scr_AddInt(0);
}

/* VERIFIED_DECOMPILER(0x6d045, 7d045_script_func_isplayernumber.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; inclusive 0..63 player-number range and integer return checked against current decompiler output. */
void script_func_isplayernumber(void)
{
    int playerNumber = Scr_GetInt(0);

    Scr_AddInt(playerNumber >= 0 && playerNumber <= SCRIPT_MAX_PLAYER_NUMBER);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_set_winner_value). */
static void game_compat_script_set_winner_value(int winner)
{
    char config[MAX_STRING_CHARS];
    const char *winnerValue = va(SCRIPT_WINNER_FORMAT, winner);

    trap_GetConfigstring(SCRIPT_GAMESTATE_CONFIGSTRING, config, sizeof(config));
    if (Q_stricmp(Info_ValueForKey(config, SCRIPT_WINNER_KEY), winnerValue) != 0) {
        Info_SetValueForKey(config, SCRIPT_WINNER_KEY, winnerValue);
        trap_SetConfigstring(SCRIPT_GAMESTATE_CONFIGSTRING, config);
    }
}

/* VERIFIED_DECOMPILER(0x6d094, 7d094_script_func_setwinningplayer.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; entity-number plus one, winner configstring read, Info compare/set, and conditional configstring write checked against current decompiler output. */
void script_func_setwinningplayer(void)
{
    gentity_t *ent = Scr_GetEntity(0);

    game_compat_script_set_winner_value(ent->s.number + 1);
}

/* VERIFIED_DECOMPILER(0x6d16a, 7d16a_script_func_setwinningteam.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; allies/axis/none mapping, invalid-team error path, winner configstring helper behavior, and conditional write checked against current decompiler output. */
void script_func_setwinningteam(void)
{
    uint16_t team = Scr_GetConstString(0);
    int winner;

    if (team == scr_const_allies) {
        winner = SCRIPT_WINNER_ALLIES;
    } else if (team == scr_const_axis) {
        winner = SCRIPT_WINNER_AXIS;
    } else if (team == scr_const_none) {
        winner = SCRIPT_WINNER_NONE;
    } else {
        Scr_ParamError(0, va(SCRIPT_WINNING_TEAM_ERROR, SL_ConvertToString(team)));
        return;
    }

    game_compat_script_set_winner_value(winner);
}

/* VERIFIED_DECOMPILER(0x6d2b3, 7d2b3_script_func_announcement.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; message construction arguments, global unreliable command target, and command format checked against current decompiler output. */
void script_func_announcement(void)
{
    char message[MAX_STRING_CHARS];

    Scr_ConstructMessageString(0, message, sizeof(message), SCRIPT_MESSAGE_MODE_ANNOUNCEMENT);
    trap_SendServerCommand(SERVER_COMMAND_ALL_CLIENTS, UNRELIABLE_SERVER_COMMAND, va(SCRIPT_ANNOUNCEMENT_COMMAND, message));
}

/* VERIFIED_DECOMPILER(0x6d327, 7d327_script_func_clientannouncement.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; entity target, message construction offset, unreliable command, and command format checked against current decompiler output. */
void script_func_clientannouncement(void)
{
    gentity_t *ent = Scr_GetEntity(0);
    char message[MAX_STRING_CHARS];

    Scr_ConstructMessageString(1, message, sizeof(message), SCRIPT_MESSAGE_MODE_ANNOUNCEMENT);
    trap_SendServerCommand((uint32_t)ent->s.number, UNRELIABLE_SERVER_COMMAND, va(SCRIPT_ANNOUNCEMENT_COMMAND, message));
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_team_score_error). */
static void game_compat_script_team_score_error(uint16_t team)
{
    Scr_Error(va(SCRIPT_TEAM_SCORE_ERROR, SL_ConvertToString(team)));
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_get_team_score_value). */
static int game_compat_script_get_team_score_value(uint16_t team)
{
    if (team == scr_const_allies) {
        return level.teamScoreAllies;
    }
    if (team == scr_const_axis) {
        return level.teamScoreAxis;
    }

    game_compat_script_team_score_error(team);
    return level.teamScoreAxis;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_team_score_slot). */
static int *game_compat_script_team_score_slot(uint16_t team, int *configstring)
{
    if (team == scr_const_allies) {
        *configstring = SCRIPT_ALLIES_SCORE_CONFIGSTRING;
        return &level.teamScoreAllies;
    }
    if (team == scr_const_axis) {
        *configstring = SCRIPT_AXIS_SCORE_CONFIGSTRING;
        return &level.teamScoreAxis;
    }

    game_compat_script_team_score_error(team);
    *configstring = SCRIPT_AXIS_SCORE_CONFIGSTRING;
    return &level.teamScoreAxis;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_level_client). */
static gclient_t *game_compat_script_level_client(int clientNum)
{
    return &level.clients[clientNum];
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_team_from_const). */
static int game_compat_script_team_from_const(uint16_t team)
{
    if (team == scr_const_allies) {
        return TEAM_ALLIES;
    }
    if (team == scr_const_axis) {
        return TEAM_AXIS;
    }

    game_compat_script_team_score_error(team);
    return TEAM_AXIS;
}

/* VERIFIED_DECOMPILER(0x6d3ab, 7d3ab_script_func_getteamscore.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; team-string validation helper, allies/axis score selection, and integer return checked against current decompiler output. */
void script_func_getteamscore(void)
{
    uint16_t team = Scr_GetConstString(0);

    Scr_AddInt(game_compat_script_get_team_score_value(team));
}

/* VERIFIED_DECOMPILER(0x6d454, 7d454_script_func_setteamscore.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; team-string validation helper, score slot/configstring selection, integer fetch, configstring update, and dirty flag checked against current decompiler output. */
void script_func_setteamscore(void)
{
    int configstring;
    int *score = game_compat_script_team_score_slot(Scr_GetConstString(0), &configstring);

    *score = Scr_GetInt(1);
    trap_SetConfigstring(configstring, va(SCRIPT_TEAM_SCORE_FORMAT, *score));
    level.scoreboardDirty = 1;
}

/* VERIFIED_DECOMPILER(0x6d56d, 7d56d_script_func_setclientnamemode.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; auto/manual const-string mapping and unknown-mode error path checked against current decompiler output. */
void script_func_setclientnamemode(void)
{
    uint16_t mode = Scr_GetConstString(0);

    if (mode == scr_const_auto_change) {
        level.clientNameMode = SCRIPT_CLIENT_NAME_MODE_AUTO;
    } else if (mode == scr_const_manual_change) {
        level.clientNameMode = SCRIPT_CLIENT_NAME_MODE_MANUAL;
    } else {
        Scr_Error(SCRIPT_CLIENT_NAME_MODE_UNKNOWN);
    }
}

/* VERIFIED_DECOMPILER(0x6d5ed, 7d5ed_script_func_updateclientnames.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; manual-mode gate, level client stride loop, connected/name compare, dead local name copy, userinfo-name update, and ClientUserinfoChanged call checked against current decompiler output. */
void script_func_updateclientnames(void)
{
    if (level.clientNameMode == SCRIPT_CLIENT_NAME_MODE_AUTO) {
        Scr_Error(SCRIPT_CLIENT_NAME_MANUAL_ONLY);
    }

    for (int clientNum = 0; clientNum < level.maxclients; clientNum++) {
        gclient_t *client = game_compat_script_level_client(clientNum);
        char *cleanName = client->cleanName;
        char *userinfoName = client->userInfoName;

        if (client->connectedState == CON_CONNECTED && strcmp(userinfoName, cleanName) != 0) {
            char previousName[CLIENT_NAME_SIZE];

            Q_strncpyz(previousName, userinfoName, CLIENT_NAME_SIZE);
            Q_strncpyz(userinfoName, cleanName, CLIENT_NAME_SIZE);
            ClientUserinfoChanged(clientNum);
        }
    }
}

/* VERIFIED_DECOMPILER(0x6d6db, 7d6db_script_func_getteamplayersalive.c, VERIFY-SCRIPT-BUILTINS-FX-TEAM-2026-06-17): DATAFLOW_VERIFIED; team-string validation helper, g_maxclients loop, linked/session-team/health gates, count, and integer return checked against current decompiler output. */
void script_func_getteamplayersalive(void)
{
    int team = game_compat_script_team_from_const(Scr_GetConstString(0));
    int count = 0;

    for (int clientNum = 0; clientNum < g_maxclients.integer; clientNum++) {
        gentity_t *ent = &g_entities[clientNum];

        if (ent->linked != 0 && ent->client->sessionTeam == team && ent->health > 0) {
            count++;
        }
    }

    Scr_AddInt(count);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_biased_seconds_to_milliseconds). */
static int game_compat_script_biased_seconds_to_milliseconds(float seconds)
{
    return Script_BiasedRoundToInt((float)((long double)SCRIPT_SECONDS_TO_MILLISECONDS * (long double)seconds));
}

/* VERIFIED_DECOMPILER(0x6d8dd, 7d8dd_script_func_earthquake.c, VERIFY-SCRIPT-BUILTINS-EARTHQUAKE-ADMIN-2026-06-17): DATAFLOW_VERIFIED; float/second-to-ms parameter order, temp-event type 201, angles2 earthquake offsets +0x68/+0x70/+0x6c, and void return checked against current decompiler output. */
void script_func_earthquake(void)
{
    vec3_t origin;
    float scale = Scr_GetFloat(0);
    int duration = game_compat_script_biased_seconds_to_milliseconds(Scr_GetFloat(1));
    float radius;
    float durationValue;
    gentity_t *temp;

    Scr_GetVector(2, origin);
    radius = Scr_GetFloat(3);
    durationValue = (float)duration;

    temp = G_TempEntity(origin, EARTHQUAKE_TEMP_EVENT);
    temp->s.earthquake.scale = scale;
    temp->s.earthquake.durationMs = durationValue;
    temp->s.earthquake.radius = radius;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_shellshock_index). */
static int game_compat_script_shellshock_index(const char *name)
{
    char config[MAX_STRING_CHARS];

    for (int index = SCRIPT_SHELLSHOCK_FIRST_INDEX; index <= SCRIPT_SHELLSHOCK_LAST_INDEX; index++) {
        trap_GetConfigstring(SCRIPT_SHELLSHOCK_CONFIGSTRING_BASE + index, config, sizeof(config));
        if (strcasecmp(config, name) == 0) {
            return index;
        }
    }

    Scr_Error(va(SCRIPT_SHELLSHOCK_NOT_PRECACHED, name));
    return 0;
}

/* VERIFIED_DECOMPILER(0x6d976, 7d976_script_method_scriptbuiltin_shellshock.c, VERIFY-SCRIPT-BUILTINS-EARTHQUAKE-ADMIN-2026-06-17): DATAFLOW_VERIFIED; player object lookup, usage error fallthrough, shellshock configstring scan 1..15 at base 0x4e5, not-precached return, duration bias/limit/error fallthrough, and client offsets +0x62c/+0x630/+0x634 checked against current decompiler output. */
void GScr_ShellShock(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_player(scriptObject);
    const char *name;
    int shellshockIndex;
    int duration;

    if (Scr_GetNumParam() != 2) {
        Scr_Error(SCRIPT_SHELLSHOCK_USAGE);
    }

    name = Scr_GetString(0);
    shellshockIndex = game_compat_script_shellshock_index(name);
    if (shellshockIndex == 0) {
        return;
    }

    duration = game_compat_script_biased_seconds_to_milliseconds(Scr_GetFloat(1));
    if (duration < 0 || duration > SCRIPT_SHELLSHOCK_DURATION_MAX_MS) {
        /* 0x6da41..0x6da4c: fild duration feeds the multiply directly (no
         * intermediate float store), then one rounding to double at the
         * vararg slot. */
        Scr_ParamError(
            1, va(SCRIPT_SHELLSHOCK_DURATION_ERROR, (double)((long double)duration * (long double)SCRIPT_SHELLSHOCK_SECONDS_PER_MS)));
    }

    /*
     * RECOVERED(UO-GAME-UNK-0119): these offsets overlap the current partial
     * stop-follow velocity view in recovered_game.h; keep explicit offsets until
     * the broader player-state layout resolves ownership.
     */
    ent->client->ps.motionState.shellshock.index = shellshockIndex;
    ent->client->ps.motionState.shellshock.time = level.time;
    ent->client->ps.motionState.shellshock.duration = duration;
}

/* VERIFIED_DECOMPILER(0x6dadf, 7dadf_script_method_scriptbuiltin_stopshellshock.c, VERIFY-SCRIPT-BUILTINS-EARTHQUAKE-ADMIN-2026-06-17): DATAFLOW_VERIFIED; player object lookup, zero-param usage error fallthrough, and shellshock index/start/duration clears at client offsets +0x62c/+0x630/+0x634 checked against current decompiler output. */
void GScr_StopShellShock(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_player(scriptObject);

    if (Scr_GetNumParam() != 0) {
        Scr_Error(SCRIPT_STOPSHELLSHOCK_USAGE);
    }

    ent->client->ps.motionState.shellshock.index = 0;
    ent->client->ps.motionState.shellshock.time = 0;
    ent->client->ps.motionState.shellshock.duration = 0;
}

/* VERIFIED_DECOMPILER(0x6db55, 7db55_script_method_scriptbuiltin_viewkick.c, VERIFY-SCRIPT-BUILTINS-EARTHQUAKE-ADMIN-2026-06-17): DATAFLOW_VERIFIED; player object lookup, usage/damage error fallthrough, damage formula `(force * maxHealth + 50) / 100`, source vector order, and client damage offsets +0x4658/+0x465c checked against current decompiler output. */
void GScr_ViewKick(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_player(scriptObject);
    int force;
    int damage;
    vec3_t source;
    float *damageFrom;

    if (Scr_GetNumParam() != 2) {
        Scr_Error(SCRIPT_VIEWKICK_USAGE);
    }

    force = Scr_GetInt(0);
    damage = coduo_int32_from_bits((uint32_t)force * (uint32_t)ent->maxHealth + (uint32_t)SCRIPT_VIEWKICK_DAMAGE_ROUND) /
             SCRIPT_VIEWKICK_DAMAGE_SCALE;
    ent->client->damageTaken = damage;
    if (damage < 0) {
        Scr_Error(va(SCRIPT_VIEWKICK_DAMAGE_ERROR, (double)Scr_GetFloat(0)));
    }

    Scr_GetVector(1, source);
    damageFrom = ent->client->damageFrom;
    damageFrom[0] = ent->client->ps.psOrigin[0] - source[0];
    damageFrom[1] = ent->client->ps.psOrigin[1] - source[1];
    damageFrom[2] = ent->client->ps.psOrigin[2] - source[2];
}

/* VERIFIED_DECOMPILER(0x6dc79, 7dc79_script_method_scriptbuiltin_localtoworldcoords.c, VERIFY-SCRIPT-BUILTINS-EARTHQUAKE-ADMIN-2026-06-17): DATAFLOW_VERIFIED; entity object lookup, vector/axis transform call order, currentAngles +0x148, currentOrigin additions +0x13c/+0x140/+0x144, and vector return checked against current decompiler output. */
void GScr_LocalToWorldCoords(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    vec3_t local;
    axis_t axis;
    vec3_t world;

    Scr_GetVector(0, local);
    AnglesToAxis(ent->currentAngles, axis);
    /* C99 multidimensional-array qualifier bridge; the transform is
     * read-only. */
    MatrixTransformVector(local, (const vec_t(*)[3])axis, world);
    world[0] += ent->currentOrigin[0];
    world[1] += ent->currentOrigin[1];
    world[2] += ent->currentOrigin[2];
    Scr_AddVector(world);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_turret_state). */
static turret_state_t *game_compat_script_turret_state(gentity_t *ent)
{
    if (ent->turretState == 0) {
        Scr_Error(TURRET_NOT_TURRET_ERROR);
    }

    return ent->turretState;
}

/* VERIFIED_DECOMPILER(0x6dd1a, 7dd1a_script_method_scriptbuiltin_setrightarc.c, VERIFY-SCRIPT-BUILTINS-EARTHQUAKE-ADMIN-2026-06-17): DATAFLOW_VERIFIED; entity object lookup, turretState +0x168 error fallthrough, negated float store to turret +0x10, and positive clamp to zero checked against current decompiler output. */
void GScr_SetRightArc(uint32_t scriptObject)
{
    turret_state_t *turret = game_compat_script_turret_state(script_object_to_gentity(scriptObject));

    turret->rightArc = -Scr_GetFloat(0);
    if (turret->rightArc > 0.0f) {
        turret->rightArc = 0.0f;
    }
}

/* VERIFIED_DECOMPILER(0x6dda0, 7dda0_script_method_scriptbuiltin_setleftarc.c, VERIFY-SCRIPT-BUILTINS-EARTHQUAKE-ADMIN-2026-06-17): DATAFLOW_VERIFIED; entity object lookup, turretState +0x168 error fallthrough, float store to turret +0x18, and negative clamp to zero checked against current decompiler output. */
void GScr_SetLeftArc(uint32_t scriptObject)
{
    turret_state_t *turret = game_compat_script_turret_state(script_object_to_gentity(scriptObject));

    turret->leftArc = Scr_GetFloat(0);
    if (turret->leftArc < 0.0f) {
        turret->leftArc = 0.0f;
    }
}

/* VERIFIED_DECOMPILER(0x6de19, 7de19_script_method_scriptbuiltin_settoparc.c, VERIFY-SCRIPT-BUILTINS-EARTHQUAKE-ADMIN-2026-06-17): DATAFLOW_VERIFIED; entity object lookup, turretState +0x168 error fallthrough, negated float store to turret +0x0c, and positive clamp to zero checked against current decompiler output. */
void GScr_SetTopArc(uint32_t scriptObject)
{
    turret_state_t *turret = game_compat_script_turret_state(script_object_to_gentity(scriptObject));

    turret->topArc = -Scr_GetFloat(0);
    if (turret->topArc > 0.0f) {
        turret->topArc = 0.0f;
    }
}

/* VERIFIED_DECOMPILER(0x6de9f, 7de9f_script_method_scriptbuiltin_setbottomarc.c, VERIFY-SCRIPT-BUILTINS-EARTHQUAKE-ADMIN-2026-06-17): DATAFLOW_VERIFIED; entity object lookup, turretState +0x168 error fallthrough, float store to turret +0x14, and negative clamp to zero checked against current decompiler output. */
void GScr_SetBottomArc(uint32_t scriptObject)
{
    turret_state_t *turret = game_compat_script_turret_state(script_object_to_gentity(scriptObject));

    turret->bottomArc = Scr_GetFloat(0);
    if (turret->bottomArc < 0.0f) {
        turret->bottomArc = 0.0f;
    }
}

/* VERIFIED_DECOMPILER(0x6df18, 7df18_script_method_scriptbuiltin_placespawnpoint.c, VERIFY-SCRIPT-BUILTINS-EARTHQUAKE-ADMIN-2026-06-17): DATAFLOW_VERIFIED; entity lookup, up/down/point trace sequence, player bounds, trace mask 0x2810011, ground entity store +0x7c, truncating solid-warning coordinates, and G_SetOrigin endpos checked against current decompiler output. */
void GScr_PlaceSpawnPoint(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    trace_t trace;
    vec3_t start;
    vec3_t end;

    game_compat_script_copy_vector(start, ent->currentOrigin);
    game_compat_script_copy_vector(end, ent->currentOrigin);
    end[2] += SPAWNPOINT_TRACE_UP;
    trap_TraceCapsule(&trace, start, playerMins, playerMaxs, end, ent->s.number, SPAWNPOINT_TRACE_MASK);

    game_compat_script_copy_vector(start, trace.endpos);
    game_compat_script_copy_vector(end, trace.endpos);
    end[2] -= SPAWNPOINT_TRACE_DOWN;
    trap_TraceCapsule(&trace, start, playerMins, playerMaxs, end, ent->s.number, SPAWNPOINT_TRACE_MASK);
    ent->s.groundEntityNum = trace.entityNum;

    game_compat_script_copy_vector(start, trace.endpos);
    trap_TraceCapsule(&trace, start, playerMins, playerMaxs, start, ent->s.number, SPAWNPOINT_TRACE_MASK);
    if (trace.startsolid) {
        /* 0x6e0a2..0x6e0e4: each coordinate uses x87 fistp with RC=0xc00.
         * The defined helper also preserves INT32_MIN for NaN/out-of-range
         * input instead of relying on an undefined host-language cast. */
        Com_Printf(SPAWNPOINT_SOLID_WARNING, ent->s.number, game_compat_int32_from_float_trunc(ent->currentOrigin[0]),
                   game_compat_int32_from_float_trunc(ent->currentOrigin[1]), game_compat_int32_from_float_trunc(ent->currentOrigin[2]));
    }

    G_SetOrigin(ent, trace.endpos);
}

/* VERIFIED_DECOMPILER(0x6e11c, 7e11c_script_func_exec.c, VERIFY-SCRIPT-BUILTINS-EARTHQUAKE-ADMIN-2026-06-17): DATAFLOW_VERIFIED; zero-param error fallthrough, filename fetch, `exec %s\n` formatting, console command time 2, and void return checked against current decompiler output. */
void script_func_exec(void)
{
    if (Scr_GetNumParam() == 0) {
        Scr_Error(SCRIPT_EXEC_NO_FILENAME);
    }

    trap_SendConsoleCommand(SCRIPT_CONSOLE_COMMAND_NOW, va(SCRIPT_EXEC_COMMAND_FORMAT, Scr_GetString(0)));
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_exit_state_error_message). */
static const char *game_compat_script_exit_state_error_message(int exitState)
{
    if (exitState == SCRIPT_EXIT_STATE_MAP_RESTART) {
        return SCRIPT_MAP_RESTART_ALREADY_CALLED;
    }

    return SCRIPT_EXIT_LEVEL_ALREADY_CALLED;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_set_exit_state). */
static void game_compat_script_set_exit_state(int exitState)
{
    level_locals_t *lvl = &level;

    if (lvl->exitState != SCRIPT_EXIT_STATE_NONE) {
        Scr_Error(game_compat_script_exit_state_error_message(lvl->exitState));
    }

    lvl->exitState = exitState;
    lvl->scriptExitParam = 0;
    if (Scr_GetNumParam() != 0) {
        lvl->scriptExitParam = Scr_GetInt(0);
    }
}

/* VERIFIED_DECOMPILER(0x6e179, 7e179_script_func_map_restart.c, VERIFY-SCRIPT-BUILTINS-EARTHQUAKE-ADMIN-2026-06-17): DATAFLOW_VERIFIED; exit-state error selection/fallthrough, map-restart state 1, optional integer parameter storage, `map_restart\n` command, and console command time 2 checked against current decompiler output. */
void script_func_map_restart(void)
{
    game_compat_script_set_exit_state(SCRIPT_EXIT_STATE_MAP_RESTART);
    trap_SendConsoleCommand(SCRIPT_CONSOLE_COMMAND_NOW, SCRIPT_MAP_RESTART_COMMAND);
}

/* VERIFIED_DECOMPILER(0x6e227, 7e227_script_func_exitlevel.c, VERIFY-SCRIPT-BUILTINS-EARTHQUAKE-ADMIN-2026-06-17): DATAFLOW_VERIFIED; exit-state error selection/fallthrough, exit-level state 2, optional integer parameter storage, ExitLevel call, and void return checked against current decompiler output. */
void script_func_exitlevel(void)
{
    game_compat_script_set_exit_state(SCRIPT_EXIT_STATE_EXIT_LEVEL);
    ExitLevel();
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_copy_saved_spawn_vars). */
static void game_compat_script_copy_saved_spawn_vars(gentity_t *ent)
{
    level_locals_t *lvl = &level;
    char **levelPairs = lvl->spawnVarPairSlots;
    char *levelText = lvl->spawnText;
    uint32_t textLength = (uint32_t)ent->savedSpawnTextLength;

    lvl->spawnTextLength = ent->savedSpawnTextLength;
    memcpy(levelText, ent->savedSpawnText, (size_t)textLength);
    lvl->spawnVarCount = ent->savedSpawnVarCount;

    for (int pairIndex = 0; pairIndex < ent->savedSpawnVarCount; pairIndex++) {
        for (int slot = 0; slot < 2; slot++) {
            int index = pairIndex * 2 + slot;
            levelPairs[index] = &levelText[ent->savedSpawnVarPairs[index] - ent->savedSpawnText];
        }
    }
}

/* VERIFIED_DECOMPILER(0x6e2c4, 7e2c4_script_method_scriptbuiltin_spawnduplicate.c, VERIFY-WAVE2-SCRIPT-SPAWNFREE-2026-06-17): DATAFLOW_VERIFIED - saved-spawn guard, spawn-var copy helper, G_CallSpawn branch, error formatting, saved metadata transfer, and Scr_AddEntity path checked against current decompiler output. */
void GScr_RespawnDuplicate(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    gentity_t *spawned;

    if (ent->savedSpawnVarCount == 0) {
        Scr_Error(SCRIPT_SPAWNDUPLICATE_NO_DETAILS);
    }

    game_compat_script_copy_saved_spawn_vars(ent);
    spawned = G_CallSpawn();
    if (spawned == 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        Scr_Error(va(SCRIPT_SPAWN_ERROR_FORMAT, SL_ConvertToString(ent->scriptClassname)));
        return;
    } else {
        spawned->savedSpawnTextLength = ent->savedSpawnTextLength;
        spawned->savedSpawnText = ent->savedSpawnText;
        spawned->savedSpawnVarCount = ent->savedSpawnVarCount;
        spawned->savedSpawnVarPairs = ent->savedSpawnVarPairs;
        Scr_AddEntity(spawned);
    }
}

/* VERIFIED_DECOMPILER(0x6e47d, 7e47d_script_func_addtestclient.c, VERIFY-SCRIPT-BUILTINS-EARTHQUAKE-ADMIN-2026-06-17): DATAFLOW_VERIFIED; trap_AddTestClient call, non-null branch, Scr_AddEntity argument, and void return checked against current decompiler output. */
void script_func_addtestclient(void)
{
    gentity_t *ent = trap_AddTestClient();

    if (ent != 0) {
        Scr_AddEntity(ent);
    }
}

/* VERIFIED_DECOMPILER(0x6e5ee, 7e5ee_script_func_setarchive.c, VERIFY-SCRIPT-BUILTINS-EARTHQUAKE-ADMIN-2026-06-17): DATAFLOW_VERIFIED; bool parameter fetch, trap_SetArchive argument order, and void return checked against current decompiler output. */
void script_func_setarchive(void)
{
    trap_SetArchive(Scr_GetBool(0));
}

/* VERIFIED_DECOMPILER(0x6b397, 7b397_script_func_randomint.c, VERIFY-SCRIPT-BUILTINS-RANDOM-TRIG-2026-06-17): DATAFLOW_VERIFIED; positive max guard, error print, irand bounds, and integer return checked against current decompiler output. */
void script_func_randomint(void)
{
    int max = Scr_GetInt(0);

    if (max < 1) {
        Com_Printf("RandomInt parm: %d  ", max);
        Scr_Error("RandomInt parm must be positive integer.\n");
    } else {
        Scr_AddInt(irand(0, max));
    }
}

/* VERIFIED_DECOMPILER(0x6b404, 7b404_script_func_randomfloat.c, VERIFY-SCRIPT-BUILTINS-RANDOM-TRIG-2026-06-17): DATAFLOW_VERIFIED; float fetch, flrand bounds, and float return checked against current decompiler output. */
void script_func_randomfloat(void)
{
    float max = Scr_GetFloat(0);

    Scr_AddFloat(flrand(0.0f, max));
}

/* VERIFIED_DECOMPILER(0x6b446, 7b446_script_func_randomintrange.c, VERIFY-SCRIPT-BUILTINS-RANDOM-TRIG-2026-06-17): DATAFLOW_VERIFIED; min/max fetches, fallthrough error path, irand bounds, and integer return checked against current decompiler output. */
void script_func_randomintrange(void)
{
    int min = Scr_GetInt(0);
    int max = Scr_GetInt(1);

    if (max <= min) {
        Com_Printf("RandomIntRange parms: %d %d ", min, max);
        Scr_Error("RandomIntRange range must be positive integer.\n");
    }

    Scr_AddInt(irand(min, max));
}

/* VERIFIED_DECOMPILER(0x6b4c8, 7b4c8_script_func_randomfloatrange.c, VERIFY-SCRIPT-BUILTINS-RANDOM-TRIG-2026-06-17): DATAFLOW_VERIFIED; min/max fetches, fallthrough error path, flrand bounds, and float return checked against current decompiler output. */
void script_func_randomfloatrange(void)
{
    float min = Scr_GetFloat(0);
    float max = Scr_GetFloat(1);

    if (max <= min) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        Com_Printf("Scr_RandomFloatRange parms: %f %f ", (double)min, (double)max);
        Scr_Error("Scr_RandomFloatRange range must be positive float.\n");
    }

    Scr_AddFloat(flrand(min, max));
}

/* VERIFIED_DECOMPILER(0x6b551, 7b551_script_func_sin.c, VERIFY-SCRIPT-BUILTINS-RANDOM-TRIG-2026-06-17): DATAFLOW_VERIFIED; degree-to-radian conversion, sin call, float cast, and return checked against current decompiler output. */
void script_func_sin(void)
{
    /* 0x6b56f..0x6b57f: fmul QWORD pi, fdiv QWORD 180.0 (double consts), rounded
     * to double at the libm CoduoLibm_Sin() call boundary -> shim. */
#if EMULATE_X87
    double radians = x87f_store_f64(
        x87f_div(x87f_mul(x87f_load_f32(Scr_GetFloat(0)), x87f_load_f64(SCRIPT_PI)), x87f_load_f64(SCRIPT_HALF_CIRCLE_DEGREES)));
#else
    double radians = (double)Scr_GetFloat(0) * SCRIPT_PI / SCRIPT_HALF_CIRCLE_DEGREES;
#endif
    Scr_AddFloat((float)CoduoLibm_Sin(radians));
}

/* VERIFIED_DECOMPILER(0x6b59b, 7b59b_script_func_cos.c, VERIFY-SCRIPT-BUILTINS-RANDOM-TRIG-2026-06-17): DATAFLOW_VERIFIED; degree-to-radian conversion, cos call, float cast, and return checked against current decompiler output. */
void script_func_cos(void)
{
    /* 0x6b5b9..0x6b5c9: fmul QWORD pi, fdiv QWORD 180.0 (see script_func_sin). */
#if EMULATE_X87
    double radians = x87f_store_f64(
        x87f_div(x87f_mul(x87f_load_f32(Scr_GetFloat(0)), x87f_load_f64(SCRIPT_PI)), x87f_load_f64(SCRIPT_HALF_CIRCLE_DEGREES)));
#else
    double radians = (double)Scr_GetFloat(0) * SCRIPT_PI / SCRIPT_HALF_CIRCLE_DEGREES;
#endif
    Scr_AddFloat((float)CoduoLibm_Cos(radians));
}

/* VERIFIED_DECOMPILER(0x6b5e5, 7b5e5_script_func_tan.c, VERIFY-SCRIPT-BUILTINS-RANDOM-TRIG-2026-06-17): DATAFLOW_VERIFIED; SinCos helper call, divide-by-zero error fallthrough, and sine/cosine return checked against current decompiler output. */
void script_func_tan(void)
{
    /* 0x6b603..0x6b613: fmul QWORD pi, fdiv QWORD 180.0, then ONE direct
     * rounding to a float slot (fstp DWORD) — no intermediate double rounding. */
#if EMULATE_X87
    float radians = x87f_store_f32(
        x87f_div(x87f_mul(x87f_load_f32(Scr_GetFloat(0)), x87f_load_f64(SCRIPT_PI)), x87f_load_f64(SCRIPT_HALF_CIRCLE_DEGREES)));
#else
    float radians = (float)((double)Scr_GetFloat(0) * SCRIPT_PI / SCRIPT_HALF_CIRCLE_DEGREES);
#endif
    float sine;
    float cosine;

    /* Script_SinCos uses native x87 fsincos where available. */
    Script_SinCos(radians, &sine, &cosine);

    if (cosine == 0.0f) {
        Scr_Error("divide by 0");
    }

    /* sine / cosine kept 80-bit, one store -> shim. */
#if EMULATE_X87
    Scr_AddFloat(x87f_store_f32(x87f_div(x87f_load_f32(sine), x87f_load_f32(cosine))));
#else
    Scr_AddFloat(sine / cosine);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_trig_check_unit_range). */
static void game_compat_script_trig_check_unit_range(float value)
{
    if (value < -1.0f || value > 1.0f) {
        Scr_Error(va("%g out of range", (double)value));
    }
}

/* VERIFIED_DECOMPILER(0x6b667, 7b667_script_func_asin.c, VERIFY-SCRIPT-BUILTINS-RANDOM-TRIG-2026-06-17): DATAFLOW_VERIFIED; unit-range guard, asin call, float degree conversion, and return checked against current decompiler output. */
void script_func_asin(void)
{
    float value = Scr_GetFloat(0);

    game_compat_script_trig_check_unit_range(value);
    /* 0x6b6cf..0x6b6ea: asin result rounded to a float slot, then reloaded,
     * fmul DWORD 180.0f, fdiv QWORD pi, rounded to float again — not a
     * precombined 180/pi float factor. */
    {
        float degrees = (float)CoduoLibm_Asin((double)value);

        /* degrees * 180.0f(DWORD) / PI(QWORD) kept 80-bit, one store -> shim. */
#if EMULATE_X87
        degrees = x87f_store_f32(
            x87f_div(x87f_mul(x87f_load_f32(degrees), x87f_load_f32(SCRIPT_HALF_CIRCLE_DEGREES_FLOAT)), x87f_load_f64(SCRIPT_PI)));
#else
        degrees = degrees * SCRIPT_HALF_CIRCLE_DEGREES_FLOAT / SCRIPT_PI;
#endif
        Scr_AddFloat(degrees);
    }
}

/* VERIFIED_DECOMPILER(0x6b6fe, 7b6fe_script_func_acos.c, VERIFY-SCRIPT-BUILTINS-RANDOM-TRIG-2026-06-17): DATAFLOW_VERIFIED; unit-range guard, acos call, float degree conversion, and return checked against current decompiler output. */
void script_func_acos(void)
{
    float value = Scr_GetFloat(0);

    game_compat_script_trig_check_unit_range(value);
    /* 0x6b76b..0x6b781: same shape as script_func_asin — float store, fmul
     * DWORD 180.0f, fdiv QWORD pi, float store. */
    {
        float degrees = (float)CoduoLibm_Acos((double)value);

        /* degrees * 180.0f(DWORD) / PI(QWORD) kept 80-bit, one store -> shim. */
#if EMULATE_X87
        degrees = x87f_store_f32(
            x87f_div(x87f_mul(x87f_load_f32(degrees), x87f_load_f32(SCRIPT_HALF_CIRCLE_DEGREES_FLOAT)), x87f_load_f64(SCRIPT_PI)));
#else
        degrees = degrees * SCRIPT_HALF_CIRCLE_DEGREES_FLOAT / SCRIPT_PI;
#endif
        Scr_AddFloat(degrees);
    }
}

/* VERIFIED_DECOMPILER(0x6b795, 7b795_script_func_atan.c, VERIFY-SCRIPT-BUILTINS-RANDOM-TRIG-2026-06-17): DATAFLOW_VERIFIED; atan call, float degree conversion, and return checked against current decompiler output. */
void script_func_atan(void)
{
    /* 0x6b7b3..0x6b7d1: argument passed straight to atan (no float local),
     * result rounded to a float slot, fmul DWORD 180.0f, fdiv QWORD pi,
     * rounded to float again. */
    float degrees = (float)atan((double)Scr_GetFloat(0));

    /* degrees * 180.0f(DWORD) / PI(QWORD) kept 80-bit, one store -> shim. */
#if EMULATE_X87
    degrees = x87f_store_f32(
        x87f_div(x87f_mul(x87f_load_f32(degrees), x87f_load_f32(SCRIPT_HALF_CIRCLE_DEGREES_FLOAT)), x87f_load_f64(SCRIPT_PI)));
#else
    degrees = degrees * SCRIPT_HALF_CIRCLE_DEGREES_FLOAT / SCRIPT_PI;
#endif
    Scr_AddFloat(degrees);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_vector_length_squared). */
static float game_compat_script_vector_length_squared(const vec3_t value)
{
    /* 3-mul/2-add dot kept 80-bit, rounded to float on return -> shim. */
#if EMULATE_X87
    return x87f_store_f32(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(value[0]), x87f_load_f32(value[0])), x87f_mul(x87f_load_f32(value[1]), x87f_load_f32(value[1]))),
        x87f_mul(x87f_load_f32(value[2]), x87f_load_f32(value[2]))));
#else
    return value[0] * value[0] + value[1] * value[1] + value[2] * value[2];
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_vector_dot). */
static float game_compat_script_vector_dot(const vec3_t a, const vec3_t b)
{
#if EMULATE_X87
    return x87f_store_f32(
        x87f_add(x87f_add(x87f_mul(x87f_load_f32(a[0]), x87f_load_f32(b[0])), x87f_mul(x87f_load_f32(a[1]), x87f_load_f32(b[1]))),
                 x87f_mul(x87f_load_f32(a[2]), x87f_load_f32(b[2]))));
#else
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_fade_seconds_to_milliseconds). */
static int game_compat_script_fade_seconds_to_milliseconds(float seconds)
{
    return Script_BiasedRoundToInt(seconds * SCRIPT_SECONDS_TO_MILLISECONDS);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_soundfade_seconds_to_milliseconds). */
static int game_compat_script_soundfade_seconds_to_milliseconds(float seconds)
{
    /* 0x6bd0a..0x6bd24: fmul DWORD 1000.0f then truncating fistp (RC=0xc00)
     * straight from the x87 register — plain truncation -> shim. */
#if EMULATE_X87
    return x87f_store_i32_trunc(x87f_mul(x87f_load_f32(seconds), x87f_load_f32(SCRIPT_SECONDS_TO_MILLISECONDS)));
#else
    return game_compat_int32_from_long_double_trunc((long double)seconds * (long double)SCRIPT_SECONDS_TO_MILLISECONDS);
#endif
}

/* VERIFIED_DECOMPILER(0x6b7e5, 7b7e5_script_func_distance.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; vector fetch order, VectorDistance call, and float return checked against current decompiler output. */
void script_func_distance(void)
{
    vec3_t a;
    vec3_t b;

    Scr_GetVector(0, a);
    Scr_GetVector(1, b);
    Scr_AddFloat(VectorDistance(a, b));
}

/* VERIFIED_DECOMPILER(0x6b83d, 7b83d_script_func_distancesquared.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; vector fetch order, VectorDistanceSquared call, and float return checked against current decompiler output. */
void script_func_distancesquared(void)
{
    vec3_t a;
    vec3_t b;

    Scr_GetVector(0, a);
    Scr_GetVector(1, b);
    Scr_AddFloat(VectorDistanceSquared(a, b));
}

/* VERIFIED_DECOMPILER(0x6b895, 7b895_script_func_length.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; vector fetch, squared-length helper, double sqrt, float cast, and return checked against current decompiler output. */
void script_func_length(void)
{
    vec3_t value;

    Scr_GetVector(0, value);
    /* 0x6b8ba..0x6b8d8: the squared-length chain stays in the x87 register and
     * is rounded to DOUBLE at the sqrt argument store (fstp QWORD) — no
     * intermediate float rounding, so the helper (which returns float) is not
     * usable here. */
    /* dot 80-bit -> double at sqrt boundary -> float -> shim. */
#if EMULATE_X87
    Scr_AddFloat((float)CoduoLibm_Sqrt(x87f_store_f64(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(value[0]), x87f_load_f32(value[0])), x87f_mul(x87f_load_f32(value[1]), x87f_load_f32(value[1]))),
        x87f_mul(x87f_load_f32(value[2]), x87f_load_f32(value[2]))))));
#else
    Scr_AddFloat((float)CoduoLibm_Sqrt((double)(value[0] * value[0] + value[1] * value[1] + value[2] * value[2])));
#endif
}

/* VERIFIED_DECOMPILER(0x6b8ec, 7b8ec_script_func_lengthsquared.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; vector fetch, squared-length expression, and float return checked against current decompiler output. */
void script_func_lengthsquared(void)
{
    vec3_t value;

    Scr_GetVector(0, value);
    Scr_AddFloat(game_compat_script_vector_length_squared(value));
}

/* VERIFIED_DECOMPILER(0x6b935, 7b935_script_func_closer.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; three vector fetches, distance-squared comparison order, and integer return checked against current decompiler output. */
void script_func_closer(void)
{
    vec3_t origin;
    vec3_t a;
    vec3_t b;
    float aDistanceSquared;
    float bDistanceSquared;

    Scr_GetVector(0, origin);
    Scr_GetVector(1, a);
    Scr_GetVector(2, b);
    aDistanceSquared = VectorDistanceSquared(a, origin);
    bDistanceSquared = VectorDistanceSquared(b, origin);
    Scr_AddInt(aDistanceSquared < bDistanceSquared);
}

/* VERIFIED_DECOMPILER(0x6b9cb, 7b9cb_script_func_vectordot.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; vector fetch order, dot-product component order, and float return checked against current decompiler output. */
void script_func_vectordot(void)
{
    vec3_t a;
    vec3_t b;

    Scr_GetVector(0, a);
    Scr_GetVector(1, b);
    Scr_AddFloat(game_compat_script_vector_dot(a, b));
}

/* VERIFIED_DECOMPILER(0x6ba27, 7ba27_script_func_vectornormalize.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; vector fetch, local copy, VectorNormalize call, and vector return checked against current decompiler output. */
void script_func_vectornormalize(void)
{
    vec3_t input;
    vec3_t normalized;
    uint32_t bits;

    Scr_GetVector(0, input);
    /* 0x6ba54..0x6ba68 transports the lanes with ordered integer dword
     * loads/stores, without floating-point evaluation. */
    memcpy(&bits, &input[0], sizeof(bits));
    memcpy(&normalized[0], &bits, sizeof(bits));
    memcpy(&bits, &input[1], sizeof(bits));
    memcpy(&normalized[1], &bits, sizeof(bits));
    memcpy(&bits, &input[2], sizeof(bits));
    memcpy(&normalized[2], &bits, sizeof(bits));
    VectorNormalize(normalized);
    Scr_AddVector(normalized);
}

/* VERIFIED_DECOMPILER(0x6ba7c, 7ba7c_script_func_vectortoangles.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; vector fetch, vectoangles call, and vector return checked against current decompiler output. */
void script_func_vectortoangles(void)
{
    vec3_t value;
    vec3_t angles;

    Scr_GetVector(0, value);
    vectoangles(value, angles);
    Scr_AddVector(angles);
}

/* VERIFIED_DECOMPILER(0x6bac4, 7bac4_script_func_anglestoup.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; vector fetch, AngleVectors up-output argument order, and vector return checked against current decompiler output. */
void script_func_anglestoup(void)
{
    vec3_t angles;
    vec3_t up;

    Scr_GetVector(0, angles);
    AngleVectors(angles, 0, 0, up);
    Scr_AddVector(up);
}

/* VERIFIED_DECOMPILER(0x6bb1c, 7bb1c_script_func_anglestoright.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; vector fetch, AngleVectors right-output argument order, and vector return checked against current decompiler output. */
void script_func_anglestoright(void)
{
    vec3_t angles;
    vec3_t right;

    Scr_GetVector(0, angles);
    AngleVectors(angles, 0, right, 0);
    Scr_AddVector(right);
}

/* VERIFIED_DECOMPILER(0x6bb74, 7bb74_script_func_anglestoforward.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; vector fetch, AngleVectors forward-output argument order, and vector return checked against current decompiler output. */
void script_func_anglestoforward(void)
{
    vec3_t angles;
    vec3_t forward;

    Scr_GetVector(0, angles);
    AngleVectors(angles, forward, 0, 0);
    Scr_AddVector(forward);
}

/* VERIFIED_DECOMPILER(0x6bbcc, 7bbcc_script_func_musicplay.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; string fetch, command format, global reliable target, and void return checked against current decompiler output. */
void script_func_musicplay(void)
{
    const char *alias = Scr_GetString(0);

    trap_SendServerCommand(SERVER_COMMAND_ALL_CLIENTS, RELIABLE_SERVER_COMMAND, va(SCRIPT_MUSIC_PLAY_COMMAND, alias));
}

/* VERIFIED_DECOMPILER(0x6bc1a, 7bc1a_script_func_musicstop.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; parameter-count branches, biased millisecond conversion, negative-time error fallthrough, and command send checked against current decompiler output. */
void script_func_musicstop(void)
{
    int fadeTime;
    uint32_t paramCount = Scr_GetNumParam();

    if (paramCount == 0) {
        fadeTime = 0;
    } else if (paramCount == 1) {
        fadeTime = game_compat_script_fade_seconds_to_milliseconds(Scr_GetFloat(0));
    } else {
        Scr_Error(va("USAGE: musicStop([fadetime]);\n"));
        return;
    }

    if (fadeTime < 0) {
        Scr_Error(va("musicStop: fade time must be >= 0\n"));
    }

    trap_SendServerCommand(SERVER_COMMAND_ALL_CLIENTS, RELIABLE_SERVER_COMMAND, va(SCRIPT_MUSIC_STOP_COMMAND, fadeTime));
}

/* VERIFIED_DECOMPILER(0x6bcd3, 7bcd3_script_func_soundfade.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; volume fetch, optional direct rounded millisecond conversion, command format, and global reliable send checked against current decompiler output. */
void script_func_soundfade(void)
{
    float volume = Scr_GetFloat(0);
    int fadeTime = 0;

    if (Scr_GetNumParam() >= 2) {
        fadeTime = game_compat_script_soundfade_seconds_to_milliseconds(Scr_GetFloat(1));
    }

    trap_SendServerCommand(SERVER_COMMAND_ALL_CLIENTS, RELIABLE_SERVER_COMMAND, va(SCRIPT_SOUNDFADE_COMMAND, (double)volume, fadeTime));
}

/* VERIFIED_DECOMPILER(0x6bd6d, 7bd6d_script_func_precachemodel.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; precache-phase error fallthrough, string fetch, and G_ModelIndex call checked against current decompiler output. */
void script_func_precachemodel(void)
{
    game_compat_script_check_precache_phase("precacheModel must be called before any wait statements in the gametype or "
                                            "level script\n");
    G_ModelIndex(Scr_GetString(0));
}

/* VERIFIED_DECOMPILER(0x6bdb3, 7bdb3_script_func_precacheshellshock.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; precache-phase error fallthrough, string fetch, and G_ShellShockIndex call checked against current decompiler output. */
void script_func_precacheshellshock(void)
{
    game_compat_script_check_precache_phase("precacheShellShock must be called before any wait statements in the gametype or "
                                            "level script\n");
    G_ShellShockIndex(Scr_GetString(0));
}

/* VERIFIED_DECOMPILER(0x6bdf9, 7bdf9_script_func_precacheitem.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; precache-phase error fallthrough, item lookup, unknown-item Scr_ParamError fallthrough, item-index arithmetic helper, and RegisterItem call checked against current decompiler output. */
void script_func_precacheitem(void)
{
    const char *itemName;
    gitem_t *item;

    game_compat_script_check_precache_phase("precacheItem must be called before any wait statements in the gametype or "
                                            "level script\n");

    itemName = Scr_GetString(0);
    item = BG_FindItem(itemName);
    if (item == 0) {
        Scr_ParamError(0, va("unknown item '%s'", itemName));
    }

    RegisterItem(game_compat_script_item_index_from_def(item), 1);
}

/* VERIFIED_DECOMPILER(0x6be9d, 7be9d_script_func_precacheshader.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; precache-phase error fallthrough, shader string fetch, empty-string error fallthrough, and G_ShaderIndex call checked against current decompiler output. */
void script_func_precacheshader(void)
{
    const char *shaderName;

    game_compat_script_check_precache_phase("precacheShader must be called before any wait statements in the gametype or "
                                            "level script\n");

    shaderName = Scr_GetString(0);
    if (shaderName[0] == '\0') {
        Scr_ParamError(0, "Shader name string is empty");
    }

    G_ShaderIndex(shaderName);
}

/* VERIFIED_DECOMPILER(0x6bf07, 7bf07_script_func_precachestring.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; precache-phase error fallthrough, localized string fetch, non-empty branch, and G_LocalizedStringIndex call checked against current decompiler output. */
void script_func_precachestring(void)
{
    const char *value;

    game_compat_script_check_precache_phase("precacheString must be called before any wait statements in the gametype or "
                                            "level script\n");

    value = Scr_GetIString(0);
    if (value[0] != '\0') {
        G_LocalizedStringIndex(value);
    }
}

/* VERIFIED_DECOMPILER(0x6bf5b, 7bf5b_script_func_ambientplay.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; parameter-count branches, biased millisecond conversion, empty-alias/negative-time error fallthroughs, and configstring format checked against current decompiler output. */
void script_func_ambientplay(void)
{
    int fadeTime = 0;
    const char *alias;
    uint32_t paramCount = Scr_GetNumParam();

    if (paramCount == 2) {
        fadeTime = game_compat_script_fade_seconds_to_milliseconds(Scr_GetFloat(1));
    } else if (paramCount != 1) {
        Scr_Error(va("USAGE: ambientPlay(alias_name, <fadetime>);\n"));
        return;
    }

    alias = Scr_GetString(0);
    if (alias[0] == '\0') {
        Scr_Error(va("ambientPlay: alias name cannot be the empty string... use stop or "
                     "fade version\n"));
    }

    if (fadeTime < 0) {
        Scr_Error(va("ambientPlay: fade time must be >= 0\n"));
    }

    trap_SetConfigstring(SCRIPT_AMBIENT_CONFIGSTRING,
                         va(SCRIPT_AMBIENT_PLAY_CONFIGSTRING, alias, coduo_int32_from_bits((uint32_t)fadeTime + (uint32_t)level.time)));
}

/* VERIFIED_DECOMPILER(0x6c053, 7c053_script_func_ambientstop.c, VERIFY-SCRIPT-BUILTINS-VECTOR-AUDIO-2026-06-17): DATAFLOW_VERIFIED; parameter-count branches, biased millisecond conversion, negative-time error fallthrough, and configstring format checked against current decompiler output. */
void script_func_ambientstop(void)
{
    int fadeTime;
    uint32_t paramCount = Scr_GetNumParam();

    if (paramCount == 0) {
        fadeTime = 0;
    } else if (paramCount == 1) {
        fadeTime = game_compat_script_fade_seconds_to_milliseconds(Scr_GetFloat(0));
    } else {
        Scr_Error(va("USAGE: ambientStop(<fadetime>);\n"));
        return;
    }

    if (fadeTime < 0) {
        Scr_Error(va("ambientStop: fade time must be >= 0\n"));
    }

    trap_SetConfigstring(SCRIPT_AMBIENT_CONFIGSTRING,
                         va(SCRIPT_AMBIENT_STOP_CONFIGSTRING, coduo_int32_from_bits((uint32_t)fadeTime + (uint32_t)level.time)));
}

#define SCRIPT_FUNCTION_COUNT 120u
#define SCRIPTBUILTIN_METHOD_COUNT 56u
#define SCRIPTENT_METHOD_COUNT 12u

void GScr_GetCvar(void);
void GScr_GetCvarInt(void);
void GScr_GetCvarFloat(void);
void GScr_SetCvar(void);
void GScr_MakeCvarServerInfo(void);
void Scr_Objective_Add(void);
void Scr_Objective_Delete(void);
void Scr_Objective_State(void);
void Scr_Objective_Icon(void);
void Scr_Objective_Position(void);
void Scr_Objective_OnEntity(void);
void Scr_Objective_Current(void);
void GScr_Objective_Team(void);
void GScr_PrecacheVehicle(void);
void GScr_GetNumVehicles(void);
void GScr_NewHudElem(void);
void GScr_NewClientHudElem(void);
void GScr_NewTeamHudElem(void);
void Scr_Prof_Begin(void);
void Scr_Prof_End(void);
script_method_callback_t Player_GetMethod(const char **name);
script_method_callback_t ScriptEnt_GetMethod(const char **name);
script_method_callback_t ScriptVehicle_GetMethod(const char **name);
script_method_callback_t HudElem_GetMethod(const char **name);
script_method_callback_t BuiltIn_GetMethod(const char **name);

void ScrCmd_GetStance(uint32_t scriptObject);
void ScrCmd_attach(uint32_t scriptObject);
void ScrCmd_detach(uint32_t scriptObject);
void ScrCmd_detachAll(uint32_t scriptObject);
void ScrCmd_GetAttachSize(uint32_t scriptObject);
void ScrCmd_GetAttachModelName(uint32_t scriptObject);
void ScrCmd_GetAttachTagName(uint32_t scriptObject);
void ScrCmd_GetAttachIgnoreCollision(uint32_t scriptObject);
void ScrCmd_LinkTo(uint32_t scriptObject);
void ScrCmd_Unlink(uint32_t scriptObject);
void ScrCmd_EnableLinkTo(uint32_t scriptObject);
void ScrCmd_GetOrigin(uint32_t scriptObject);
void ScrCmd_GetEye(uint32_t scriptObject);
void ScrCmd_UseBy(uint32_t scriptObject);
void ScrCmd_IsTouching(uint32_t scriptObject);
void ScrCmd_LockDoor(uint32_t scriptObject);
void ScrCmd_UnlockDoor(uint32_t scriptObject);
void ScrCmd_IsDoorLocked(uint32_t scriptObject);
void ScrCmd_PlaySound(uint32_t scriptObject);
void ScrCmd_PlayLoopSound(uint32_t scriptObject);
void ScrCmd_StopLoopSound(uint32_t scriptObject);
void ScrCmd_Delete(uint32_t scriptObject);
void ScrCmd_SetModel(uint32_t scriptObject);
void ScrCmd_GetNormalHealth(uint32_t scriptObject);
void ScrCmd_SetNormalHealth(uint32_t scriptObject);
void ScrCmd_DoDamage(uint32_t scriptObject);
void ScrCmd_DoDamageMod(uint32_t scriptObject);
void ScrCmd_SetTakeDamage(uint32_t scriptObject);
void ScrCmd_Show(uint32_t scriptObject);
void ScrCmd_Hide(uint32_t scriptObject);
void ScrCmd_UnlinkFromWorld(uint32_t scriptObject);
void ScrCmd_LinkIntoWorld(uint32_t scriptObject);
void ScrCmd_VerifyPosition(uint32_t scriptObject);
void ScrCmd_ClearVehiclePosition(uint32_t scriptObject);
void ScrCmd_SetContents(uint32_t scriptObject);
void ScrCmd_SetBounds(uint32_t scriptObject);
void GScr_SetCursorHint(uint32_t scriptObject);
void GScr_SetHintString(uint32_t scriptObject);
void GScr_GetEntityNumber(uint32_t scriptObject);
void GScr_GetTurretHeat(uint32_t scriptObject);
void GScr_GetTurretOverheating(uint32_t scriptObject);
void GScr_EnableGrenadeTouchDamage(uint32_t scriptObject);
void GScr_DisableGrenadeTouchDamage(uint32_t scriptObject);
void GScr_EnableGrenadeBounce(uint32_t scriptObject);
void GScr_DisableGrenadeBounce(uint32_t scriptObject);

void ScriptEntCmd_MoveTo(uint32_t scriptObject);
void ScriptEntCmd_MoveX(uint32_t scriptObject);
void ScriptEntCmd_MoveY(uint32_t scriptObject);
void ScriptEntCmd_MoveZ(uint32_t scriptObject);
void ScriptEntCmd_GravityMove(uint32_t scriptObject);
void ScriptEntCmd_RotateTo(uint32_t scriptObject);
void ScriptEntCmd_RotatePitch(uint32_t scriptObject);
void ScriptEntCmd_RotateYaw(uint32_t scriptObject);
void ScriptEntCmd_RotateRoll(uint32_t scriptObject);
void ScriptEntCmd_RotateVelocity(uint32_t scriptObject);
void ScriptEntCmd_Solid(uint32_t scriptObject);
void ScriptEntCmd_NotSolid(uint32_t scriptObject);

const script_method_t scriptbuiltin_methods[SCRIPTBUILTIN_METHOD_COUNT] = {
    {"getstance", ScrCmd_GetStance},
    {"attach", ScrCmd_attach},
    {"detach", ScrCmd_detach},
    {"detachall", ScrCmd_detachAll},
    {"getattachsize", ScrCmd_GetAttachSize},
    {"getattachmodelname", ScrCmd_GetAttachModelName},
    {"getattachtagname", ScrCmd_GetAttachTagName},
    {"getattachignorecollision", ScrCmd_GetAttachIgnoreCollision},
    {"linkto", ScrCmd_LinkTo},
    {"unlink", ScrCmd_Unlink},
    {"enablelinkto", ScrCmd_EnableLinkTo},
    {"getorigin", ScrCmd_GetOrigin},
    {"geteye", ScrCmd_GetEye},
    {"useby", ScrCmd_UseBy},
    {"istouching", ScrCmd_IsTouching},
    {"lockdoor", ScrCmd_LockDoor},
    {"unlockdoor", ScrCmd_UnlockDoor},
    {"isdoorlocked", ScrCmd_IsDoorLocked},
    {"playsound", ScrCmd_PlaySound},
    {"playloopsound", ScrCmd_PlayLoopSound},
    {"stoploopsound", ScrCmd_StopLoopSound},
    {"delete", ScrCmd_Delete},
    {"setmodel", ScrCmd_SetModel},
    {"getnormalhealth", ScrCmd_GetNormalHealth},
    {"setnormalhealth", ScrCmd_SetNormalHealth},
    {"dodamage", ScrCmd_DoDamage},
    {"dodamagemod", ScrCmd_DoDamageMod},
    {"settakedamage", ScrCmd_SetTakeDamage},
    {"show", ScrCmd_Show},
    {"hide", ScrCmd_Hide},
    {"unlinkfromworld", ScrCmd_UnlinkFromWorld},
    {"linkintoworld", ScrCmd_LinkIntoWorld},
    {"verifyposition", ScrCmd_VerifyPosition},
    {"clearvehicleposition", ScrCmd_ClearVehiclePosition},
    {"setcontents", ScrCmd_SetContents},
    {"setbounds", ScrCmd_SetBounds},
    {"setcursorhint", GScr_SetCursorHint},
    {"sethintstring", GScr_SetHintString},
    {"shellshock", GScr_ShellShock},
    {"stopshellshock", GScr_StopShellShock},
    {"viewkick", GScr_ViewKick},
    {"localtoworldcoords", GScr_LocalToWorldCoords},
    {"setrightarc", GScr_SetRightArc},
    {"setleftarc", GScr_SetLeftArc},
    {"settoparc", GScr_SetTopArc},
    {"setbottomarc", GScr_SetBottomArc},
    {"getentitynumber", GScr_GetEntityNumber},
    {"enablegrenadetouchdamage", GScr_EnableGrenadeTouchDamage},
    {"disablegrenadetouchdamage", GScr_DisableGrenadeTouchDamage},
    {"enablegrenadebounce", GScr_EnableGrenadeBounce},
    {"disablegrenadebounce", GScr_DisableGrenadeBounce},
    {"placespawnpoint", GScr_PlaceSpawnPoint},
    {"spawnduplicate", GScr_RespawnDuplicate},
    {"getentitynumber", GScr_GetEntityNumber},
    {"getturretheat", GScr_GetTurretHeat},
    {"getturretoverheating", GScr_GetTurretOverheating},
};

typedef char scriptbuiltin_methods_count_check
    [(sizeof(scriptbuiltin_methods) / sizeof(scriptbuiltin_methods[0]) == SCRIPTBUILTIN_METHOD_COUNT) ? 1 : -1];

const script_method_t scriptent_methods[SCRIPTENT_METHOD_COUNT] = {
    {"moveto", ScriptEntCmd_MoveTo},
    {"movex", ScriptEntCmd_MoveX},
    {"movey", ScriptEntCmd_MoveY},
    {"movez", ScriptEntCmd_MoveZ},
    {"movegravity", ScriptEntCmd_GravityMove},
    {"rotateto", ScriptEntCmd_RotateTo},
    {"rotatepitch", ScriptEntCmd_RotatePitch},
    {"rotateyaw", ScriptEntCmd_RotateYaw},
    {"rotateroll", ScriptEntCmd_RotateRoll},
    {"rotatevelocity", ScriptEntCmd_RotateVelocity},
    {"solid", ScriptEntCmd_Solid},
    {"notsolid", ScriptEntCmd_NotSolid},
};

typedef char scriptent_methods_count_check[(sizeof(scriptent_methods) / sizeof(scriptent_methods[0]) == SCRIPTENT_METHOD_COUNT) ? 1 : -1];

const script_function_t functions[SCRIPT_FUNCTION_COUNT] = {
    {"print", script_func_print, 1},
    {"println", script_func_println, 1},
    {"iprintln", script_func_iprintln, 0},
    {"iprintlnbold", script_func_iprintlnbold, 0},
    {"print3d", script_func_print3d, 0},
    {"line", script_func_line, 0},
    {"getent", Scr_GetEnt, 0},
    {"getentarray", Scr_GetEntArray, 0},
    {"spawn", script_func_spawn, 0},
    {"spawntriggermount", script_func_spawntriggermount, 0},
    {"spawnturret", script_func_spawnturret, 0},
    {"precacheturret", script_func_precacheturret, 0},
    {"spawnstruct", Scr_AddStruct, 0},
    {"assert", script_func_assert, 1},
    {"isdefined", script_func_isdefined, 0},
    {"isalive", script_func_isalive, 0},
    {"isvalidplayer", script_func_isvalidplayer, 0},
    {"getcvar", GScr_GetCvar, 0},
    {"getcvarint", GScr_GetCvarInt, 0},
    {"getcvarfloat", GScr_GetCvarFloat, 0},
    {"setcvar", GScr_SetCvar, 0},
    {"gettime", script_func_gettime, 0},
    {"getentbynum", script_func_getentbynum, 0},
    {"getweaponmodel", script_func_getweaponmodel, 0},
    {"getweaponclassname", script_func_getweaponclassname, 0},
    {"getanimlength", script_func_getanimlength, 0},
    {"animhasnotetrack", script_func_animhasnotetrack, 0},
    {"getbrushmodelcenter", script_func_getbrushmodelcenter, 0},
    {"getfullclipammo", script_func_getfullclipammo, 0},
    {"objective_add", Scr_Objective_Add, 0},
    {"objective_delete", Scr_Objective_Delete, 0},
    {"objective_state", Scr_Objective_State, 0},
    {"objective_icon", Scr_Objective_Icon, 0},
    {"objective_position", Scr_Objective_Position, 0},
    {"objective_onentity", Scr_Objective_OnEntity, 0},
    {"objective_current", Scr_Objective_Current, 0},
    {"bullettrace", script_func_bullettrace, 0},
    {"getmovedelta", script_func_getmovedelta, 0},
    {"getangledelta", script_func_getangledelta, 0},
    {"randomint", script_func_randomint, 0},
    {"randomfloat", script_func_randomfloat, 0},
    {"randomintrange", script_func_randomintrange, 0},
    {"randomfloatrange", script_func_randomfloatrange, 0},
    {"sin", script_func_sin, 0},
    {"cos", script_func_cos, 0},
    {"tan", script_func_tan, 0},
    {"asin", script_func_asin, 0},
    {"acos", script_func_acos, 0},
    {"atan", script_func_atan, 0},
    {"distance", script_func_distance, 0},
    {"distancesquared", script_func_distancesquared, 0},
    {"length", script_func_length, 0},
    {"lengthsquared", script_func_lengthsquared, 0},
    {"closer", script_func_closer, 0},
    {"vectordot", script_func_vectordot, 0},
    {"vectornormalize", script_func_vectornormalize, 0},
    {"vectortoangles", script_func_vectortoangles, 0},
    {"anglestoup", script_func_anglestoup, 0},
    {"anglestoright", script_func_anglestoright, 0},
    {"anglestoforward", script_func_anglestoforward, 0},
    {"musicplay", script_func_musicplay, 0},
    {"musicstop", script_func_musicstop, 0},
    {"soundfade", script_func_soundfade, 0},
    {"ambientplay", script_func_ambientplay, 0},
    {"ambientstop", script_func_ambientstop, 0},
    {"precachemodel", script_func_precachemodel, 0},
    {"precacheshellshock", script_func_precacheshellshock, 0},
    {"precacheitem", script_func_precacheitem, 0},
    {"precacheshader", script_func_precacheshader, 0},
    {"precachestring", script_func_precachestring, 0},
    {"precachevehicle", GScr_PrecacheVehicle, 0},
    {"precacheturret", script_func_precacheturret, 0},
    {"getnumvehicles", GScr_GetNumVehicles, 0},
    {"loadfx", script_func_loadfx, 0},
    {"rewindfx", script_func_rewindfx, 0},
    {"playfx", script_func_playfx, 0},
    {"playfxontag", script_func_playfxontag, 0},
    {"playloopedfx", script_func_playloopedfx, 0},
    {"playfxonplayer", script_func_playfxonplayer, 0},
    {"setwind", script_func_setwind, 0},
    {"setcullfog", Scr_SetLinearFog, 0},
    {"setexpfog", Scr_SetExponentialFog, 0},
    {"grenadeexplosioneffect", script_func_grenadeexplosioneffect, 0},
    {"radiusdamage", script_func_radiusdamage, 0},
    {"setplayerignoreradiusdamage", script_func_setplayerignoreradiusdamage, 0},
    {"getnumparts", GScr_GetNumParts, 0},
    {"getpartname", GScr_GetPartName, 0},
    {"earthquake", script_func_earthquake, 0},
    {"newhudelem", GScr_NewHudElem, 0},
    {"newclienthudelem", GScr_NewClientHudElem, 0},
    {"newteamhudelem", GScr_NewTeamHudElem, 0},
    {"resettimeout", Scr_ResetTimeout, 0},
    {"isplayer", script_func_isplayer, 0},
    {"isplayernumber", script_func_isplayernumber, 0},
    {"setwinningplayer", script_func_setwinningplayer, 0},
    {"setwinningteam", script_func_setwinningteam, 0},
    {"announcement", script_func_announcement, 0},
    {"clientannouncement", script_func_clientannouncement, 0},
    {"getteamscore", script_func_getteamscore, 0},
    {"setteamscore", script_func_setteamscore, 0},
    {"setclientnamemode", script_func_setclientnamemode, 0},
    {"updateclientnames", script_func_updateclientnames, 0},
    {"getteamplayersalive", script_func_getteamplayersalive, 0},
    {"objective_team", GScr_Objective_Team, 0},
    {"logprint", script_func_logprint, 0},
    {"worldentnumber", script_func_worldentnumber, 0},
    {"obituary", script_func_obituary, 0},
    {"positionwouldtelefrag", script_func_positionwouldtelefrag, 0},
    {"getstarttime", script_func_getstarttime, 0},
    {"precachemenu", script_func_precachemenu, 0},
    {"precachestatusicon", script_func_precachestatusicon, 0},
    {"precacheheadicon", script_func_precacheheadicon, 0},
    {"exec", script_func_exec, 0},
    {"map_restart", script_func_map_restart, 0},
    {"exitlevel", script_func_exitlevel, 0},
    {"addtestclient", script_func_addtestclient, 0},
    {"makecvarserverinfo", GScr_MakeCvarServerInfo, 0},
    {"setarchive", script_func_setarchive, 0},
    {"prof_begin", Scr_Prof_Begin, 0},
    {"prof_end", Scr_Prof_End, 0},
};

typedef char script_functions_count_check[(sizeof(functions) / sizeof(functions[0]) == SCRIPT_FUNCTION_COUNT) ? 1 : -1];

/* VERIFIED_DECOMPILER(0x6e61a, 7e61a_Scr_Prof_Begin.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; empty body and void return checked against current decompiler output. */
void Scr_Prof_Begin(void)
{
}

/* VERIFIED_DECOMPILER(0x6e61f, 7e61f_Scr_Prof_End.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; empty body and void return checked against current decompiler output. */
void Scr_Prof_End(void)
{
}

/* VERIFIED_DECOMPILER(0x6e624, 7e624_Scr_GetFunction.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; 120-entry scan, strcmp break, name/developerOnly stores, callback return, and miss return checked against current decompiler output. */
script_function_callback_t Scr_GetFunction(const char **name, int *developerOnly)
{
    for (uint32_t index = 0; index < SCRIPT_FUNCTION_COUNT; index++) {
        if (strcmp(*name, functions[index].name) == 0) {
            *name = functions[index].name;
            *developerOnly = functions[index].developerOnly;
            return functions[index].callback;
        }
    }

    return 0;
}

scr_data_t g_scr_data;

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_hunk_copy_string). */
static char *game_compat_script_hunk_copy_string(const char *value)
{
    size_t length = strlen(value);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    char *copy = trap_Hunk_AllocLowInternal(length + 1u);

    strcpy(copy, value);
    return copy;
}

/* VERIFIED_DECOMPILER(0x6f10f, 7f10f_FUN_0007f10f.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; null return, in-place loop, signed-char tolower argument, end-pointer return, and void-free side effects checked against current decompiler output. */
char *Script_LowercaseString(char *value)
{
    char *result = value;

    if (value != NULL) {
        while (*value != '\0') {
            *value = (char)tolower(coduo_ctype_signed_byte_arg(*value));
            value++;
        }
        result = value;
    }

    return result;
}

/* VERIFIED_DECOMPILER(0x6e95d, 7e95d_Scr_ParseGameTypeList.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; local count lifetime, file-list "gsc" extension, file-list loop/cursor increments, underscore skip, ".gsc" extension trim, hunk copies, description load/error paths, team token parse, close guard, and final count stores checked against current decompiler output. */
void Scr_ParseGameTypeList(void)
{
    char fileList[GAMETYPE_FILE_LIST_BUFFER_SIZE];
    char description[MAX_STRING_CHARS];
    char *cursor;
    int gametypeCount;
    int fileCount;
    int fileIndex;

    gametypeCount = 0;
    memset(g_scr_data.gametypes, 0, sizeof(g_scr_data.gametypes));

    fileCount = trap_FS_GetFileList(GAMETYPE_LIST_PATH, GAMETYPE_LIST_EXTENSION, fileList, sizeof(fileList));
    cursor = fileList;

    for (fileIndex = 0; fileIndex < fileCount; fileIndex++) {
        script_gametype_info_t *entry;
        size_t entryLength = strlen(cursor);
        size_t extensionLength = GAMETYPE_SCRIPT_EXTENSION_LENGTH;
        int fileLength;
        int handle = 0;

        if (cursor[0] == '_') {
            cursor = &cursor[entryLength + 1];
            continue;
        }

        if (Q_stricmp(&cursor[entryLength - extensionLength], GAMETYPE_SCRIPT_EXTENSION) == 0) {
            cursor[entryLength - extensionLength] = '\0';
        }

        if (gametypeCount == GAMETYPE_MAX_COUNT) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            G_Printf("Too many game type scripts found! Only loading the first %i\n", GAMETYPE_MAX_COUNT);
            g_scr_data.gametypeCount = gametypeCount;
            return;
        }

        entry = &g_scr_data.gametypes[gametypeCount];
        entry->script = game_compat_script_hunk_copy_string(cursor);
        Script_LowercaseString(entry->script);

        fileLength = trap_FS_FOpenFile(va(GAMETYPE_DESCRIPTION_PATH, cursor), &handle, FS_READ);
        if (fileLength < 1 || fileLength > GAMETYPE_DESCRIPTION_MAX_LENGTH) {
            if (fileLength < 1) {
                Com_Printf("WARNING: Could not load GameType description file %s "
                           "for gametype %s\n",
                           va(GAMETYPE_DESCRIPTION_PATH, cursor), cursor);
            } else {
                Com_Printf("WARNING: GameType description file %s is too big to "
                           "load.\n",
                           va(GAMETYPE_DESCRIPTION_PATH, cursor));
            }

            entry->displayName = entry->script;
            entry->teamBased = qfalse;
        } else {
            char *parse;
            char *token;

            memset(description, 0, sizeof(description));
            trap_FS_Read(description, fileLength, handle);
            parse = description;

            token = Com_Parse(&parse);
            entry->displayName = game_compat_script_hunk_copy_string(token);

            token = Com_Parse(&parse);
            entry->teamBased = token != 0 && Q_stricmp(token, GAMETYPE_TEAM_TOKEN) == 0 ? qtrue : qfalse;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (handle != 0) {
            trap_FS_FCloseFile(handle);
        }

        gametypeCount++;
        cursor = &cursor[entryLength + 1];
    }

    g_scr_data.gametypeCount = gametypeCount;
}

/* VERIFIED_DECOMPILER(0x6ed1f, 7ed1f_Scr_GetGameTypeNameForScript.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; count-bound scan, script-name compare, display-name return, and null miss return checked against current decompiler output. */
const char *Scr_GetGameTypeNameForScript(const char *scriptName)
{
    for (int index = 0; index < g_scr_data.gametypeCount; index++) {
        if (Q_stricmp(g_scr_data.gametypes[index].script, scriptName) == 0) {
            return g_scr_data.gametypes[index].displayName;
        }
    }

    return NULL;
}

/* VERIFIED_DECOMPILER(0x6eda9, 7eda9_Scr_IsValidGameType.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; Scr_GetGameTypeNameForScript call and non-null boolean return checked against current decompiler output. */
qboolean Scr_IsValidGameType(const char *scriptName)
{
    return Scr_GetGameTypeNameForScript(scriptName) != NULL;
}

/* VERIFIED_DECOMPILER(0x652a4, 752a4_GScr_AllocString.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; Scr_AllocString value/1 arguments and 16-bit return checked against current decompiler output. */
uint16_t GScr_AllocString(const char *value)
{
    return Scr_AllocString(value, 1);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_gscr_load_const). */
static void game_compat_gscr_load_const(uint16_t index, const char *value)
{
    scr_const[index] = GScr_AllocString(value);
}

/* VERIFIED_DECOMPILER(0x652d2, 752d2_GScr_LoadConsts.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; allocation order, sparse scr_const indices, literal values, single destination stores, and void return checked against current decompiler output. */
void GScr_LoadConsts(void)
{
    game_compat_gscr_load_const(0, "active");
    game_compat_gscr_load_const(1, "air strike");
    game_compat_gscr_load_const(2, "allies");
    game_compat_gscr_load_const(3, "animdone");
    game_compat_gscr_load_const(4, "axis");
    game_compat_gscr_load_const(5, "bodyque");
    game_compat_gscr_load_const(6, "combat");
    game_compat_gscr_load_const(7, "connected");
    game_compat_gscr_load_const(8, "connecting");
    game_compat_gscr_load_const(9, "count");
    game_compat_gscr_load_const(11, "crouch");
    game_compat_gscr_load_const(12, "crowbar");
    game_compat_gscr_load_const(10, "current");
    game_compat_gscr_load_const(13, "damage");
    game_compat_gscr_load_const(14, "death");
    game_compat_gscr_load_const(15, "disconnected");
    game_compat_gscr_load_const(16, "dlight");
    game_compat_gscr_load_const(17, "done");
    game_compat_gscr_load_const(18, "empty");
    game_compat_gscr_load_const(19, "enemy");
    game_compat_gscr_load_const(20, "enemyhidden");
    game_compat_gscr_load_const(21, "enemyvisible");
    game_compat_gscr_load_const(22, "entity");
    game_compat_gscr_load_const(23, "failed");
    game_compat_gscr_load_const(24, "flamebarrel");
    game_compat_gscr_load_const(25, "fraction");
    game_compat_gscr_load_const(26, "func_door");
    game_compat_gscr_load_const(27, "func_door_rotating");
    game_compat_gscr_load_const(28, "func_rotating");
    game_compat_gscr_load_const(29, "func_tramcar");
    game_compat_gscr_load_const(30, "goal");
    game_compat_gscr_load_const(31, "grenade");
    game_compat_gscr_load_const(32, "info_notnull");
    game_compat_gscr_load_const(33, "invisible");
    game_compat_gscr_load_const(34, "key1");
    game_compat_gscr_load_const(35, "key2");
    game_compat_gscr_load_const(36, "killanimscript");
    game_compat_gscr_load_const(37, "left");
    game_compat_gscr_load_const(38, "misc_flak");
    game_compat_gscr_load_const(39, "misc_mg42");
    game_compat_gscr_load_const(40, "misc_turret");
    game_compat_gscr_load_const(41, "misc_tagemitter");
    game_compat_gscr_load_const(42, "mortar");
    game_compat_gscr_load_const(43, "movedone");
    game_compat_gscr_load_const(44, "noclass");
    game_compat_gscr_load_const(45, "noenemy");
    game_compat_gscr_load_const(46, "noncombat");
    game_compat_gscr_load_const(47, "normal");
    game_compat_gscr_load_const(48, "pistol");
    game_compat_gscr_load_const(49, "plane_waypoint");
    game_compat_gscr_load_const(50, "player");
    game_compat_gscr_load_const(51, "position");
    game_compat_gscr_load_const(52, "primary");
    game_compat_gscr_load_const(53, "primaryb");
    game_compat_gscr_load_const(54, "prone");
    game_compat_gscr_load_const(55, "reached_end_node");
    game_compat_gscr_load_const(56, "reached_wait_node");
    game_compat_gscr_load_const(57, "reached_wait_node");
    game_compat_gscr_load_const(58, "right");
    game_compat_gscr_load_const(59, "rocket");
    game_compat_gscr_load_const(60, "rotatedone");
    game_compat_gscr_load_const(62, "script_brushmodel");
    game_compat_gscr_load_const(63, "script_model");
    game_compat_gscr_load_const(64, "script_origin");
    game_compat_gscr_load_const(65, "script_vehicle");
    game_compat_gscr_load_const(66, "script_vehicle_corpse");
    game_compat_gscr_load_const(67, "script_vehicle_collmap");
    game_compat_gscr_load_const(68, "front_left");
    game_compat_gscr_load_const(69, "front_right");
    game_compat_gscr_load_const(70, "back_left");
    game_compat_gscr_load_const(71, "back_right");
    game_compat_gscr_load_const(72, "middle_left");
    game_compat_gscr_load_const(73, "middle_right");
    game_compat_gscr_load_const(74, "scriptcamera");
    game_compat_gscr_load_const(75, "spawned");
    game_compat_gscr_load_const(76, "spectator");
    game_compat_gscr_load_const(78, "stand");
    game_compat_gscr_load_const(79, "surfacetype");
    game_compat_gscr_load_const(80, "tag_engine1");
    game_compat_gscr_load_const(81, "tag_engine2");
    game_compat_gscr_load_const(82, "target_location");
    game_compat_gscr_load_const(83, "target_script_trigger");
    game_compat_gscr_load_const(84, "tempEntity");
    game_compat_gscr_load_const(85, "muzzleEntity");
    game_compat_gscr_load_const(86, "smokegrenade");
    game_compat_gscr_load_const(87, "touch");
    game_compat_gscr_load_const(88, "trigger");
    game_compat_gscr_load_const(89, "trigger_use");
    game_compat_gscr_load_const(90, "trigger_damage");
    game_compat_gscr_load_const(91, "trigger_mount");
    game_compat_gscr_load_const(92, "trigger_lookat");
    game_compat_gscr_load_const(93, "truck_cam");
    game_compat_gscr_load_const(94, "turret_fire");
    game_compat_gscr_load_const(95, "turret_alt_fire");
    game_compat_gscr_load_const(96, "turret_gunner_fire");
    game_compat_gscr_load_const(97, "turret_on_target");
    game_compat_gscr_load_const(98, "turret_on_vistarget");
    game_compat_gscr_load_const(101, "xmodel/airborne");
    game_compat_gscr_load_const(102, "xmodel/wehrmacht");
    game_compat_gscr_load_const(100, "worldspawn");
    game_compat_gscr_load_const(103, "begin");
    game_compat_gscr_load_const(104, "dynamite");
    game_compat_gscr_load_const(105, "explosive_indicator");
    game_compat_gscr_load_const(106, "flamechunk");
    game_compat_gscr_load_const(107, "follow");
    game_compat_gscr_load_const(108, "free");
    game_compat_gscr_load_const(109, "freed");
    game_compat_gscr_load_const(110, "func_leaky");
    game_compat_gscr_load_const(111, "info_player_checkpoint");
    game_compat_gscr_load_const(112, "initialize");
    game_compat_gscr_load_const(113, "intermission");
    game_compat_gscr_load_const(114, "item_stamina_brandy");
    game_compat_gscr_load_const(115, "menuresponse");
    game_compat_gscr_load_const(116, "misc_gunner_gun");
    game_compat_gscr_load_const(117, "misc_gunner_ring");
    game_compat_gscr_load_const(118, "mp_info_player_deathmatch");
    game_compat_gscr_load_const(119, "mp_info_player_intermission");
    game_compat_gscr_load_const(120, "mp_team_alliedplayer_respawn");
    game_compat_gscr_load_const(121, "mp_team_alliedplayer_start");
    game_compat_gscr_load_const(122, "mp_team_axisplayer_respawn");
    game_compat_gscr_load_const(123, "mp_team_axisplayer_start");
    game_compat_gscr_load_const(124, "nail");
    game_compat_gscr_load_const(125, "not");
    game_compat_gscr_load_const(126, "playing");
    game_compat_gscr_load_const(127, "prox_mine");
    game_compat_gscr_load_const(128, "reset");
    game_compat_gscr_load_const(129, "script_mover");
    game_compat_gscr_load_const(130, "script_multiplayer");
    game_compat_gscr_load_const(131, "spear");
    game_compat_gscr_load_const(77, "sprint");
    game_compat_gscr_load_const(132, "tag_hand");
    game_compat_gscr_load_const(133, "tag_rider");
    game_compat_gscr_load_const(134, "tag_ring");
    game_compat_gscr_load_const(135, "team_CTF_blueflag");
    game_compat_gscr_load_const(136, "team_CTF_redflag");
    game_compat_gscr_load_const(137, "team_WOLF_checkpoint");
    game_compat_gscr_load_const(138, "team_WOLF_objective");
    game_compat_gscr_load_const(139, "trigger_aidoor");
    game_compat_gscr_load_const(140, "trigger_flagonly");
    game_compat_gscr_load_const(141, "trigger_multiple");
    game_compat_gscr_load_const(142, "trigger_objective_info");
    game_compat_gscr_load_const(143, "waiting_for_players");
    game_compat_gscr_load_const(144, "WP");
    game_compat_gscr_load_const(145, "zombiespit");
    game_compat_gscr_load_const(146, "none");
    game_compat_gscr_load_const(147, "dead");
    game_compat_gscr_load_const(148, "auto_change");
    game_compat_gscr_load_const(149, "manual_change");
    game_compat_gscr_load_const(150, "freelook");
    game_compat_gscr_load_const(151, "activated");
    game_compat_gscr_load_const(152, "deactivated");
    game_compat_gscr_load_const(153, "vehicle_collision");
    game_compat_gscr_load_const(154, "vehicle_activated");
    game_compat_gscr_load_const(155, "vehicle_deactivated");
    game_compat_gscr_load_const(156, "vsay");
    game_compat_gscr_load_const(157, "artillery");
    game_compat_gscr_load_const(99, "overheating");
    game_compat_gscr_load_const(158, "squad_alpha");
    game_compat_gscr_load_const(159, "squad_bravo");
}

/* VERIFIED_DECOMPILER(0x66340, 76340_FUN_00076340.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; Scr_LoadScript required guard, script/label G_Error argument order, function-handle lookup, and return checked against current decompiler output. */
uint32_t Script_LoadFunctionHandle(const char *scriptName, const char *labelName, qboolean required)
{
    uint32_t handle;

    if (Scr_LoadScript(scriptName) == 0 && required) {
        G_Error("Could not find script '%s'", scriptName);
    }

    handle = Scr_GetFunctionHandle(scriptName, labelName);
    if (handle == 0 && required) {
        G_Error("Could not find label '%s' in script '%s'", labelName, scriptName);
    }

    return handle;
}

/* VERIFIED_DECOMPILER(0x663c2, 763c2_GScr_LoadGameTypeScript.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; 64-byte path buffer, gametype format, callback setup script loads, required flags, and handle stores checked against current decompiler output. */
void GScr_LoadGameTypeScript(void)
{
    char scriptName[MAX_QPATH];

    Com_sprintf(scriptName, sizeof(scriptName), GAMETYPE_SCRIPT_PATH, g_gametype.string);
    g_scr_data.gametypeScriptMain = Script_LoadFunctionHandle(scriptName, SCRIPT_MAIN_LABEL, qtrue);
    g_scr_data.gametypeStart = Script_LoadFunctionHandle(GAMETYPE_CALLBACK_SETUP_SCRIPT, SCRIPT_CALLBACK_START_GAMETYPE, qtrue);
    g_scr_data.playerConnect = Script_LoadFunctionHandle(GAMETYPE_CALLBACK_SETUP_SCRIPT, SCRIPT_CALLBACK_PLAYER_CONNECT, qtrue);
    g_scr_data.playerDisconnect = Script_LoadFunctionHandle(GAMETYPE_CALLBACK_SETUP_SCRIPT, SCRIPT_CALLBACK_PLAYER_DISCONNECT, qtrue);
    g_scr_data.playerDamage = Script_LoadFunctionHandle(GAMETYPE_CALLBACK_SETUP_SCRIPT, SCRIPT_CALLBACK_PLAYER_DAMAGE, qtrue);
    g_scr_data.playerKilled = Script_LoadFunctionHandle(GAMETYPE_CALLBACK_SETUP_SCRIPT, SCRIPT_CALLBACK_PLAYER_KILLED, qtrue);
}

/* VERIFIED_DECOMPILER(0x664f9, 764f9_FUN_000764f9.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; mapname cvar registration, 64-byte level script path format, optional main-label load, and handle store checked against current decompiler output. */
void GScr_LoadLevelScript(void)
{
    vmCvar_t mapname;
    char scriptName[MAX_QPATH];

    trap_Cvar_Register(&mapname, LEVEL_MAPNAME_CVAR, emptyString, LEVEL_MAPNAME_CVAR_FLAGS);
    Com_sprintf(scriptName, sizeof(scriptName), LEVEL_SCRIPT_PATH, mapname.string);
    g_scr_data.levelScriptMain = Script_LoadFunctionHandle(scriptName, SCRIPT_MAIN_LABEL, qfalse);
}

/* VERIFIED_DECOMPILER(0x66592, 76592_FUN_00076592.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; three class-name stores, class-map registration, source-model classnum alias mirrors, field registration order, and void return checked against current decompiler output. */
void GScr_RegisterScriptClasses(void)
{
    g_scr_data.classMap[SCRIPT_OBJECT_ENTITY].name = SCRIPT_CLASS_ENTITY_NAME;
    g_scr_data.classMap[SCRIPT_OBJECT_HUDELEM].name = SCRIPT_CLASS_HUDELEM_NAME;
    g_scr_data.classMap[SCRIPT_OBJECT_VEHICLE_NODE].name = SCRIPT_CLASS_VEHICLE_NODE_NAME;

    Scr_SetClassMap(g_scr_data.classMap, (int)SCRIPT_CLASS_MAP_COUNT);
    GScr_AddFieldsForEntity();
    GScr_AddFieldsForHudElems();
    GScr_AddFieldsForRadiant();
}

/* VERIFIED_DECOMPILER(0x66605, 76605_FUN_00076605.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; size argument forwarding, allocator side effect, and callback return register preservation checked against current decompiler output. */
void *GScr_AllocAnimTreeMemory(size_t size)
{
    return trap_Hunk_AllocLowInternal(size);
}

/* VERIFIED_DECOMPILER(0x66628, 76628_GScr_LoadScripts.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; cg_atmos default -1, script load order, 512 runtime-animation records, bgs animation table argument, anim-tree callback, and final animation calls checked against current decompiler output. */
void GScr_LoadScripts(void)
{
    int32_t runtimeAnimationCount = 0;
    bg_runtime_animation_t runtimeAnimations[BG_ANIM_MAX_ANIMATIONS];

    trap_Cvar_Set(SCRIPT_ATMOS_CVAR, SCRIPT_ATMOS_DEFAULT);
    Scr_BeginLoadScripts();
    GScr_LoadGameTypeScript();
    GScr_LoadLevelScript();
    GScr_RegisterScriptClasses();
    Scr_EndLoadScripts();
    BG_FindAnims();
    BG_AnimParseAnimScript(&bgs.animationTable, runtimeAnimations, &runtimeAnimationCount);
    Scr_PrecacheAnimTrees(GScr_AllocAnimTreeMemory);
    BG_FindAnimTrees();
    Scr_EndLoadAnimTrees();
    BG_FinalizePlayerAnims();
    BG_LoadAnimTreeInstances();
}

/* VERIFIED_DECOMPILER(0x666ca, 766ca_GScr_FreeScripts.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; Scr_RemoveClassMap call and void return checked against current decompiler output. */
void GScr_FreeScripts(void)
{
    Scr_RemoveClassMap();
}

/* VERIFIED_DECOMPILER(0x6ede3, 7ede3_Scr_LoadGameType.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; gametype main handle execution, zero parameter count, thread-id capture, and free checked against current decompiler output. */
void Scr_LoadGameType(void)
{
    uint16_t threadId = Scr_ExecThread(g_scr_data.gametypeScriptMain, 0);

    Scr_FreeThread(threadId);
}

/* VERIFIED_DECOMPILER(0x662f3, 762f3_Scr_LoadLevel.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; nonzero level-handle guard, zero-parameter execution, thread-id capture, and free checked against current decompiler output. */
void Scr_LoadLevel(void)
{
    if (g_scr_data.levelScriptMain != 0) {
        uint16_t threadId = Scr_ExecThread(g_scr_data.levelScriptMain, 0);

        Scr_FreeThread(threadId);
    }
}

/* VERIFIED_DECOMPILER(0x6ee24, 7ee24_Scr_StartupGameType.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; start callback handle execution, zero parameter count, thread-id capture, and free checked against current decompiler output. */
void Scr_StartupGameType(void)
{
    uint16_t threadId = Scr_ExecThread(g_scr_data.gametypeStart, 0);

    Scr_FreeThread(threadId);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_exec_entity_thread). */
static void game_compat_script_exec_entity_thread(gentity_t *ent, uint32_t handle, int paramCount)
{
    uint16_t threadId = Scr_ExecEntThread(ent, handle, paramCount);

    Scr_FreeThread(threadId);
}

/* VERIFIED_DECOMPILER(0x6ee65, 7ee65_Scr_PlayerConnect.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; entity callback handle, zero parameter count, thread-id capture via helper, and free checked against current decompiler output. */
void Scr_PlayerConnect(gentity_t *ent)
{
    game_compat_script_exec_entity_thread(ent, g_scr_data.playerConnect, 0);
}

/* VERIFIED_DECOMPILER(0x6eead, 7eead_Scr_PlayerDisconnect.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; entity callback handle, zero parameter count, thread-id capture via helper, and free checked against current decompiler output. */
void Scr_PlayerDisconnect(gentity_t *ent)
{
    game_compat_script_exec_entity_thread(ent, g_scr_data.playerDisconnect, 0);
}

/* VERIFIED_DECOMPILER(0x6eef5, 7eef5_Scr_PlayerDamage.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; hit-location, dir/point vectors, weapon pickup name, MOD/badMOD helper, flags/damage/entity push order, 9-arg entity thread, and free checked against current decompiler output. */
void Scr_PlayerDamage(gentity_t *target, gentity_t *inflictor, gentity_t *attacker, int damage, int flags, int meansOfDeath, int weapon,
                      const float *point, const float *dir, int hitLocation)
{
    const weaponInfo_t *weaponInfo;

    Scr_AddConstString(G_GetHitLocationString(hitLocation));
    GScr_AddVector(dir);
    GScr_AddVector(point);
    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(weapon);
    Scr_AddString(weaponInfo->pickupName);
    game_compat_script_add_means_of_death_name(meansOfDeath);
    Scr_AddInt(flags);
    Scr_AddInt(damage);
    GScr_AddEntity(attacker);
    GScr_AddEntity(inflictor);
    game_compat_script_exec_entity_thread(target, g_scr_data.playerDamage, 9);
}

/* VERIFIED_DECOMPILER(0x6efdd, 7efdd_Scr_PlayerKilled.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; hit-location, dir vector, weapon pickup name, MOD/badMOD helper, damage/entity push order, 7-arg entity thread, and free checked against current decompiler output. */
void Scr_PlayerKilled(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath, int weapon,
                      const float *dir, int hitLocation)
{
    const weaponInfo_t *weaponInfo;

    Scr_AddConstString(G_GetHitLocationString(hitLocation));
    GScr_AddVector(dir);
    weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(weapon);
    Scr_AddString(weaponInfo->pickupName);
    game_compat_script_add_means_of_death_name(meansOfDeath);
    Scr_AddInt(damage);
    GScr_AddEntity(attacker);
    GScr_AddEntity(inflictor);
    game_compat_script_exec_entity_thread(self, g_scr_data.playerKilled, 7);
}

/* VERIFIED_DECOMPILER(0x6f0af, 7f0af_Scr_LoadRead.c, VERIFY-SCRIPT-BUILTINS-REGISTRY-LOAD-2026-06-17): DATAFLOW_VERIFIED; constant zero return and no side effects checked against original .so machine code; engine export ABI passes one size argument. */
void *Scr_LoadRead(uint32_t size)
{
    (void)size;
    return NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_truncate_milliseconds). */
static int game_compat_script_mover_truncate_milliseconds(float seconds)
{
    /*
     * All mover duration conversions (e.g. 0x6f1b2, 0x6f717, 0x6fc01) are
     * fmul DWORD 1000.0f followed by a truncating fistp (RC=0xc00) straight
     * from the x87 register — no +0.5 bias and no intermediate float rounding
     * of the product -> shim.
     */
#if EMULATE_X87
    return x87f_store_i32_trunc(x87f_mul(x87f_load_f32(seconds), x87f_load_f32(SCRIPT_SECONDS_TO_MILLISECONDS)));
#else
    return game_compat_int32_from_long_double_trunc((long double)seconds * (long double)SCRIPT_SECONDS_TO_MILLISECONDS);
#endif
}

/* VERIFIED_DECOMPILER(0x6f168, 7f168_FUN_0007f168.c, VERIFY-SCRIPT-MOVER-CORE-2026-06-17): DATAFLOW_VERIFIED; accel-to-linear, decel, gravity-evaluate, stationary target snap, trajectory stores, return values, and round-to-ms conversions checked against current decompiler output.
 * Mac PEF symbol evidence: game_mp.dll code 0x46080, size 0x220.
 */
int ScriptMover_Updatemove(trajectory_t *trajectory, const float *currentValue, float speed, float linearTime, float decelTime,
                           const float *linearStart, const float *decelStart, const float *targetValue)
{
    (void)currentValue;

    if (trajectory->trType == TR_ACCELERATE && linearTime > 0.0f) {
        vec3_t delta;
        float inverseDuration;

        trajectory->trTime = level.time;
        /* 0x6f1b2..0x6f1cf: inline fmul 1000.0f + truncating fistp — no +0.5. */
        trajectory->trDuration = game_compat_script_mover_truncate_milliseconds(linearTime);
        game_compat_script_copy_vector(trajectory->trBase, linearStart);

        /* 0x6f1fc..0x6f22c: each difference is stored to a float slot before
         * the inverse-duration multiply reloads it. */
        delta[0] = decelStart[0] - linearStart[0];
        delta[1] = decelStart[1] - linearStart[1];
        delta[2] = decelStart[2] - linearStart[2];
        /* 0x6f22f: fild trDuration feeds the divide directly (no float cast). */
#if EMULATE_X87
        inverseDuration = x87f_store_f32(x87f_div(x87f_load_f32(SCRIPT_SECONDS_TO_MILLISECONDS), x87f_load_i32(trajectory->trDuration)));
#else
        inverseDuration = SCRIPT_SECONDS_TO_MILLISECONDS / trajectory->trDuration;
#endif
        trajectory->trDelta[0] = delta[0] * inverseDuration;
        trajectory->trDelta[1] = delta[1] * inverseDuration;
        trajectory->trDelta[2] = delta[2] * inverseDuration;
        trajectory->trType = TR_LINEAR_STOP;
        return 0;
    }

    if ((trajectory->trType == TR_LINEAR_STOP || (trajectory->trType == TR_ACCELERATE && linearTime <= 0.0f)) && decelTime > 0.0f) {
        vec3_t direction;

        trajectory->trTime = level.time;
        /* 0x6f2c3..0x6f2e0: inline fmul 1000.0f + truncating fistp — no +0.5. */
        trajectory->trDuration = game_compat_script_mover_truncate_milliseconds(decelTime);
        game_compat_script_copy_vector(trajectory->trBase, decelStart);

        direction[0] = targetValue[0] - decelStart[0];
        direction[1] = targetValue[1] - decelStart[1];
        direction[2] = targetValue[2] - decelStart[2];
        VectorNormalize(direction);
        trajectory->trDelta[0] = direction[0] * speed;
        trajectory->trDelta[1] = direction[1] * speed;
        trajectory->trDelta[2] = direction[2] * speed;
        trajectory->trType = TR_DECCELERATE;
        return 0;
    }

    if (trajectory->trType == TR_GRAVITY) {
        BG_EvaluateTrajectory(trajectory, level.time, trajectory->trBase);
    } else {
        game_compat_script_copy_vector(trajectory->trBase, targetValue);
    }

    trajectory->trTime = level.time;
    trajectory->trType = TR_STATIONARY;
    return 1;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_set_reached). */
static void game_compat_script_mover_set_reached(gentity_t *ent, void (*reached)(gentity_t *ent))
{
    ent->moverReached = reached;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_classname). */
static uint16_t game_compat_script_mover_classname(gentity_t *ent)
{
    return ent->scriptClassname;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_pos_speed). */
static float *game_compat_script_mover_pos_speed(gentity_t *ent)
{
    return (float *)(void *)&ent->maxSpeed;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_pos_linear_time). */
static float *game_compat_script_mover_pos_linear_time(gentity_t *ent)
{
    return &ent->itemWait;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_pos_decel_time). */
static float *game_compat_script_mover_pos_decel_time(gentity_t *ent)
{
    return &ent->concussiveFxEndTime;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_pos_linear_start). */
static float *game_compat_script_mover_pos_linear_start(gentity_t *ent)
{
    return ent->moverPos1;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_pos_decel_start). */
static float *game_compat_script_mover_pos_decel_start(gentity_t *ent)
{
    return ent->moverPos2;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_pos_target). */
static float *game_compat_script_mover_pos_target(gentity_t *ent)
{
    return ent->damagePoint;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_apos_speed). */
static float *game_compat_script_mover_apos_speed(gentity_t *ent)
{
    return &ent->doorAltSpeed;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_apos_linear_time). */
static float *game_compat_script_mover_apos_linear_time(gentity_t *ent)
{
    return &ent->doorYawOffset;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_apos_decel_time). */
static float *game_compat_script_mover_apos_decel_time(gentity_t *ent)
{
    return &ent->itemRandom;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_apos_linear_start). */
static float *game_compat_script_mover_apos_linear_start(gentity_t *ent)
{
    return ent->moverDir;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_apos_decel_start). */
static float *game_compat_script_mover_apos_decel_start(gentity_t *ent)
{
    return ent->damageDir;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_apos_target). */
static float *game_compat_script_mover_apos_target(gentity_t *ent)
{
    return ent->scriptMoverAngleTarget;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_is_script_entity). */
static qboolean game_compat_script_mover_is_script_entity(gentity_t *ent)
{
    uint16_t classname = game_compat_script_mover_classname(ent);

    return classname == scr_const_script_brushmodel || classname == scr_const_script_model || classname == scr_const_script_origin;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_commands_blocked). */
static qboolean game_compat_script_mover_commands_blocked(gentity_t *ent)
{
    return (ent->flags & SCRIPT_MOVER_COMMAND_BLOCKED_FLAG) != 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_command_entity). */
static gentity_t *game_compat_script_mover_command_entity(uint32_t scriptObject)
{
    gentity_t *ent;

    if (scriptObject >= MAX_GENTITIES) {
        Scr_Error(va("%i is not a valid entity number", scriptObject));
        return 0;
    }

    ent = script_object_to_gentity(scriptObject);
    if (!game_compat_script_mover_is_script_entity(ent)) {
        Scr_Error(va("entity %i is not a script_brushmodel, script_model, or script_origin", scriptObject));
        return 0;
    }

    return ent;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_clamp_light_component). */
static uint32_t game_compat_script_mover_clamp_light_component(int component)
{
    if (component > 255) {
        component = 255;
    }

    return (uint32_t)component;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_mover_init_light). */
static void game_compat_script_mover_init_light(gentity_t *ent)
{
    float light;
    vec3_t color;
    int hasLight = G_SpawnFloat("light", SCRIPT_MOVER_LIGHT_DEFAULT, &light);
    int hasColor = G_SpawnVector("color", "1 1 1", color);

    if (hasLight != 0 || hasColor != 0) {
        /*
         * 0x703fc..0x70494: each color component is fmul DWORD 255.0f then a
         * truncating fistp straight from the register (no round(), no float
         * store of the product); the intensity is fdiv DWORD 4.0f — a divide,
         * not a multiply by 0.25f.
         */
        /* color[k]*255.0f fistp-direct; light/4.0f divide fistp-direct -> shim. */
#if EMULATE_X87
        uint32_t red =
            game_compat_script_mover_clamp_light_component(x87f_store_i32_trunc(x87f_mul(x87f_load_f32(color[0]), x87f_load_f32(255.0f))));
        uint32_t green =
            game_compat_script_mover_clamp_light_component(x87f_store_i32_trunc(x87f_mul(x87f_load_f32(color[1]), x87f_load_f32(255.0f))));
        uint32_t blue =
            game_compat_script_mover_clamp_light_component(x87f_store_i32_trunc(x87f_mul(x87f_load_f32(color[2]), x87f_load_f32(255.0f))));
        uint32_t intensity =
            game_compat_script_mover_clamp_light_component(x87f_store_i32_trunc(x87f_div(x87f_load_f32(light), x87f_load_f32(4.0f))));
#else
        uint32_t red = game_compat_script_mover_clamp_light_component(
            game_compat_int32_from_long_double_trunc((long double)color[0] * (long double)255.0f));
        uint32_t green = game_compat_script_mover_clamp_light_component(
            game_compat_int32_from_long_double_trunc((long double)color[1] * (long double)255.0f));
        uint32_t blue = game_compat_script_mover_clamp_light_component(
            game_compat_int32_from_long_double_trunc((long double)color[2] * (long double)255.0f));
        uint32_t intensity = game_compat_script_mover_clamp_light_component(
            game_compat_int32_from_long_double_trunc((long double)light / (long double)4.0f));
#endif

        ent->s.constantLight = (intensity << 24) | (blue << 16) | (green << 8) | red;
    }
}

/* VERIFIED_DECOMPILER(0x6f662, 7f662_FUN_0007f662.c, VERIFY-SCRIPT-MOVER-CORE-2026-06-17): DATAFLOW_VERIFIED; target delta, active trajectory evaluation, no-accel path, speed computation via double sqrt, accel/linear/decel trajectory setup, stored waypoints, and current evaluation checked against current decompiler output.
 * Mac PEF symbol evidence: game_mp.dll code 0x45a60, size 0x43c.
 */
void ScriptMover_SetupMove(trajectory_t *trajectory, const float *targetValue, float totalTime, float accelTime, float decelTime,
                           float *currentValue, float *speed, float *linearTime, float *storedDecelTime, float *linearStart,
                           float *decelStart, float *storedTargetValue)
{
    vec3_t delta;

    delta[0] = targetValue[0] - currentValue[0];
    delta[1] = targetValue[1] - currentValue[1];
    delta[2] = targetValue[2] - currentValue[2];

    if (trajectory->trType != TR_STATIONARY) {
        BG_EvaluateTrajectory(trajectory, level.time, currentValue);
    }

    if (accelTime == 0.0f && decelTime == 0.0f) {
        float inverseDuration;

        trajectory->trTime = level.time;
        trajectory->trDuration = game_compat_script_mover_truncate_milliseconds(totalTime);
        *linearTime = totalTime;
        *storedDecelTime = 0.0f;
        game_compat_script_copy_vector(storedTargetValue, targetValue);
        game_compat_script_copy_vector(trajectory->trBase, currentValue);

        /* 0x6f7a0: fild trDuration feeds the divide directly (no float cast). */
#if EMULATE_X87
        inverseDuration = x87f_store_f32(x87f_div(x87f_load_f32(SCRIPT_SECONDS_TO_MILLISECONDS), x87f_load_i32(trajectory->trDuration)));
#else
        inverseDuration = SCRIPT_SECONDS_TO_MILLISECONDS / trajectory->trDuration;
#endif
        trajectory->trDelta[0] = delta[0] * inverseDuration;
        trajectory->trDelta[1] = delta[1] * inverseDuration;
        trajectory->trDelta[2] = delta[2] * inverseDuration;
        trajectory->trType = TR_LINEAR_STOP;
        BG_EvaluateTrajectory(trajectory, level.time, currentValue);
    } else {

        /* totalTime - accelTime - decelTime kept 80-bit, one store -> shim. */
#if EMULATE_X87
        *linearTime = x87f_store_f32(x87f_sub(x87f_sub(x87f_load_f32(totalTime), x87f_load_f32(accelTime)), x87f_load_f32(decelTime)));
#else
        *linearTime = totalTime - accelTime - decelTime;
#endif
        *storedDecelTime = decelTime;

        {
#if EMULATE_X87
            float distance =
                (float)CoduoLibm_Sqrt(x87f_store_f64(x87f_add(x87f_add(x87f_mul(x87f_load_f32(delta[0]), x87f_load_f32(delta[0])),
                                                                       x87f_mul(x87f_load_f32(delta[1]), x87f_load_f32(delta[1]))),
                                                              x87f_mul(x87f_load_f32(delta[2]), x87f_load_f32(delta[2])))));
#else
            float distance = (float)CoduoLibm_Sqrt((double)(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]));
#endif
            vec3_t velocity;

            /* (dist+dist) / ((tt+tt) - at - dt) kept 80-bit, one store -> shim. */
#if EMULATE_X87
            *speed = x87f_store_f32(
                x87f_div(x87f_add(x87f_load_f32(distance), x87f_load_f32(distance)),
                         x87f_sub(x87f_sub(x87f_add(x87f_load_f32(totalTime), x87f_load_f32(totalTime)), x87f_load_f32(accelTime)),
                                  x87f_load_f32(decelTime))));
#else
            *speed = (distance + distance) / ((totalTime + totalTime) - accelTime - decelTime);
#endif
            VectorNormalize2(delta, velocity);
            velocity[0] *= *speed;
            velocity[1] *= *speed;
            velocity[2] *= *speed;

            if (accelTime != 0.0f) {
                trajectory->trTime = level.time;
                trajectory->trDuration = game_compat_script_mover_truncate_milliseconds(accelTime);
                game_compat_script_copy_vector(trajectory->trBase, currentValue);
                game_compat_script_copy_vector(trajectory->trDelta, velocity);
                trajectory->trType = TR_ACCELERATE;
                BG_EvaluateTrajectory(trajectory, level.time + trajectory->trDuration, linearStart);
            } else {
                game_compat_script_copy_vector(linearStart, currentValue);

                if (*linearTime != 0.0f) {
                    vec3_t linearDelta;
                    float inverseDuration;

                    trajectory->trTime = level.time;
                    trajectory->trDuration = game_compat_script_mover_truncate_milliseconds(*linearTime);
                    game_compat_script_copy_vector(trajectory->trBase, currentValue);

                    linearDelta[0] = velocity[0] * *linearTime;
                    linearDelta[1] = velocity[1] * *linearTime;
                    linearDelta[2] = velocity[2] * *linearTime;
                    /* 0x6fa1b: fild trDuration feeds the divide directly. */
#if EMULATE_X87
                    inverseDuration =
                        x87f_store_f32(x87f_div(x87f_load_f32(SCRIPT_SECONDS_TO_MILLISECONDS), x87f_load_i32(trajectory->trDuration)));
#else
                    inverseDuration = SCRIPT_SECONDS_TO_MILLISECONDS / trajectory->trDuration;
#endif
                    trajectory->trDelta[0] = linearDelta[0] * inverseDuration;
                    trajectory->trDelta[1] = linearDelta[1] * inverseDuration;
                    trajectory->trDelta[2] = linearDelta[2] * inverseDuration;
                    trajectory->trType = TR_LINEAR_STOP;
                } else {
                    trajectory->trTime = level.time;
                    trajectory->trDuration = game_compat_script_mover_truncate_milliseconds(*storedDecelTime);
                    game_compat_script_copy_vector(trajectory->trBase, currentValue);
                    game_compat_script_copy_vector(trajectory->trDelta, velocity);
                    trajectory->trType = TR_DECCELERATE;
                }
            }

            /* linearStart[k] + velocity[k]*linearTime, MA one store -> shim. */
#if EMULATE_X87
            for (int k = 0; k < 3; k++) {
                decelStart[k] = x87f_store_f32(
                    x87f_add(x87f_mul(x87f_load_f32(velocity[k]), x87f_load_f32(*linearTime)), x87f_load_f32(linearStart[k])));
            }
#else
            decelStart[0] = linearStart[0] + velocity[0] * *linearTime;
            decelStart[1] = linearStart[1] + velocity[1] * *linearTime;
            decelStart[2] = linearStart[2] + velocity[2] * *linearTime;
#endif
            game_compat_script_copy_vector(storedTargetValue, targetValue);
        }

        BG_EvaluateTrajectory(trajectory, level.time, currentValue);
    }
}

/* VERIFIED_DECOMPILER(0x6fb7a, 7fb7a_FUN_0007fb7a.c, VERIFY-SCRIPT-MOVER-CORE-2026-06-17): DATAFLOW_VERIFIED; velocity setup, active trajectory evaluation, no-accel path, speed via double sqrt, accel/linear/decel setup, predicted target trajectory, and current evaluation checked against current decompiler output.
 * Mac PEF symbol evidence: game_mp.dll code 0x45680, size 0x394.
 */
void ScriptMover_SetupMoveSpeed(trajectory_t *trajectory, const float *velocity, float totalTime, float accelTime, float decelTime,
                                float *currentValue, float *speed, float *linearTime, float *storedDecelTime, float *linearStart,
                                float *decelStart, float *targetValue)
{
    if (trajectory->trType != TR_STATIONARY) {
        BG_EvaluateTrajectory(trajectory, level.time, currentValue);
    }

    if (accelTime == 0.0f && decelTime == 0.0f) {
        trajectory->trTime = level.time;
        trajectory->trDuration = game_compat_script_mover_truncate_milliseconds(totalTime);
        *linearTime = totalTime;
        *storedDecelTime = 0.0f;
        game_compat_script_copy_vector(trajectory->trBase, currentValue);
        game_compat_script_copy_vector(trajectory->trDelta, velocity);
        trajectory->trType = TR_LINEAR_STOP;
        BG_EvaluateTrajectory(trajectory, level.time, currentValue);
        BG_EvaluateTrajectory(trajectory, level.time + trajectory->trDuration, targetValue);
    } else {

        /* totalTime - accelTime - decelTime, 2 subs one store; velocity length
         * dot 80-bit -> double -> sqrt -> float -> shim. */
#if EMULATE_X87
        *linearTime = x87f_store_f32(x87f_sub(x87f_sub(x87f_load_f32(totalTime), x87f_load_f32(accelTime)), x87f_load_f32(decelTime)));
        *storedDecelTime = decelTime;
        *speed = (float)CoduoLibm_Sqrt(x87f_store_f64(x87f_add(x87f_add(x87f_mul(x87f_load_f32(velocity[0]), x87f_load_f32(velocity[0])),
                                                                        x87f_mul(x87f_load_f32(velocity[1]), x87f_load_f32(velocity[1]))),
                                                               x87f_mul(x87f_load_f32(velocity[2]), x87f_load_f32(velocity[2])))));
#else
        *linearTime = totalTime - accelTime - decelTime;
        *storedDecelTime = decelTime;
        *speed = (float)CoduoLibm_Sqrt((double)(velocity[0] * velocity[0] + velocity[1] * velocity[1] + velocity[2] * velocity[2]));
#endif

        if (accelTime != 0.0f) {
            trajectory->trTime = level.time;
            trajectory->trDuration = game_compat_script_mover_truncate_milliseconds(accelTime);
            game_compat_script_copy_vector(trajectory->trBase, currentValue);
            game_compat_script_copy_vector(trajectory->trDelta, velocity);
            trajectory->trType = TR_ACCELERATE;
            BG_EvaluateTrajectory(trajectory, level.time + trajectory->trDuration, linearStart);
        } else {
            game_compat_script_copy_vector(linearStart, currentValue);

            if (*linearTime != 0.0f) {
                trajectory->trTime = level.time;
                trajectory->trDuration = game_compat_script_mover_truncate_milliseconds(*linearTime);
                game_compat_script_copy_vector(trajectory->trBase, currentValue);
                game_compat_script_copy_vector(trajectory->trDelta, velocity);
                trajectory->trType = TR_LINEAR_STOP;
            } else {
                trajectory->trTime = level.time;
                trajectory->trDuration = game_compat_script_mover_truncate_milliseconds(*storedDecelTime);
                game_compat_script_copy_vector(trajectory->trBase, currentValue);
                game_compat_script_copy_vector(trajectory->trDelta, velocity);
                trajectory->trType = TR_DECCELERATE;
            }
        }

        /* linearStart[k] + velocity[k]*linearTime, MA one store -> shim. */
#if EMULATE_X87
        for (int k = 0; k < 3; k++) {
            decelStart[k] =
                x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(velocity[k]), x87f_load_f32(*linearTime)), x87f_load_f32(linearStart[k])));
        }
#else
        decelStart[0] = linearStart[0] + velocity[0] * *linearTime;
        decelStart[1] = linearStart[1] + velocity[1] * *linearTime;
        decelStart[2] = linearStart[2] + velocity[2] * *linearTime;
#endif

        if (*storedDecelTime != 0.0f) {
            trajectory_t decelTrajectory;

            decelTrajectory.trType = TR_DECCELERATE;
            decelTrajectory.trTime = level.time;
            decelTrajectory.trDuration = game_compat_script_mover_truncate_milliseconds(*storedDecelTime);
            game_compat_script_copy_vector(decelTrajectory.trBase, decelStart);
            game_compat_script_copy_vector(decelTrajectory.trDelta, velocity);
            BG_EvaluateTrajectory(&decelTrajectory, level.time + decelTrajectory.trDuration, targetValue);
        } else {
            game_compat_script_copy_vector(targetValue, decelStart);
        }

        BG_EvaluateTrajectory(trajectory, level.time, currentValue);
    }
}

/* VERIFIED_DECOMPILER(0x700ca, 800ca_FUN_000800ca.c, VERIFY-SCRIPT-MOVER-CORE-2026-06-17): DATAFLOW_VERIFIED; position trajectory/setup storage arguments and trap_LinkEntity side effect checked against current decompiler output.
 * Mac PEF symbol evidence: game_mp.dll code 0x455e0, size 0x64.
 */
void ScriptMover_Move(gentity_t *ent, const float *targetValue, float totalTime, float accelTime, float decelTime)
{
    ScriptMover_SetupMove(&ent->s.pos, targetValue, totalTime, accelTime, decelTime, ent->currentOrigin,
                          game_compat_script_mover_pos_speed(ent), game_compat_script_mover_pos_linear_time(ent),
                          game_compat_script_mover_pos_decel_time(ent), game_compat_script_mover_pos_linear_start(ent),
                          game_compat_script_mover_pos_decel_start(ent), game_compat_script_mover_pos_target(ent));
    trap_LinkEntity(ent);
}

/* VERIFIED_DECOMPILER(0x7016b, 8016b_FUN_0008016b.c, VERIFY-SCRIPT-MOVER-CORE-2026-06-17): DATAFLOW_VERIFIED; gravity trajectory time/duration/base/delta/type stores, immediate evaluation, and trap_LinkEntity side effect checked against current decompiler output.
 * Mac PEF symbol evidence: game_mp.dll code 0x45520, size 0x8c.
 */
void ScriptMover_GravityMove(gentity_t *ent, const float *velocity, float duration)
{
    ent->s.pos.trTime = level.time;
    ent->s.pos.trDuration = game_compat_script_mover_truncate_milliseconds(duration);
    game_compat_script_copy_vector(ent->s.pos.trBase, ent->currentOrigin);
    game_compat_script_copy_vector(ent->s.pos.trDelta, velocity);
    ent->s.pos.trType = TR_GRAVITY;
    BG_EvaluateTrajectory(&ent->s.pos, level.time, ent->currentOrigin);
    trap_LinkEntity(ent);
}

/* VERIFIED_DECOMPILER(0x70253, 80253_FUN_00080253.c, VERIFY-SCRIPT-MOVER-CORE-2026-06-17): DATAFLOW_VERIFIED; angular trajectory/setup storage arguments and trap_LinkEntity side effect checked against current decompiler output.
 * Mac PEF symbol evidence: game_mp.dll code 0x45480, size 0x64.
 */
void ScriptMover_Rotate(gentity_t *ent, const float *targetAngles, float totalTime, float accelTime, float decelTime)
{
    ScriptMover_SetupMove(&ent->s.apos, targetAngles, totalTime, accelTime, decelTime, ent->currentAngles,
                          game_compat_script_mover_apos_speed(ent), game_compat_script_mover_apos_linear_time(ent),
                          game_compat_script_mover_apos_decel_time(ent), game_compat_script_mover_apos_linear_start(ent),
                          game_compat_script_mover_apos_decel_start(ent), game_compat_script_mover_apos_target(ent));
    trap_LinkEntity(ent);
}

/* VERIFIED_DECOMPILER(0x702f4, 802f4_FUN_000802f4.c, VERIFY-SCRIPT-MOVER-CORE-2026-06-17): DATAFLOW_VERIFIED; angular velocity setup storage arguments and trap_LinkEntity side effect checked against current decompiler output.
 * Mac PEF symbol evidence: game_mp.dll code 0x453e0, size 0x64.
 */
void ScriptMover_RotateSpeed(gentity_t *ent, const float *velocity, float totalTime, float accelTime, float decelTime)
{
    ScriptMover_SetupMoveSpeed(&ent->s.apos, velocity, totalTime, accelTime, decelTime, ent->currentAngles,
                               game_compat_script_mover_apos_speed(ent), game_compat_script_mover_apos_linear_time(ent),
                               game_compat_script_mover_apos_decel_time(ent), game_compat_script_mover_apos_linear_start(ent),
                               game_compat_script_mover_apos_decel_start(ent), game_compat_script_mover_apos_target(ent));
    trap_LinkEntity(ent);
}

/* VERIFIED_DECOMPILER(0x70395, 80395_InitScriptMover.c, VERIFY-SCRIPT-MOVER-CORE-2026-06-17): DATAFLOW_VERIFIED; light/color spawn parsing, direct ROUND component packing, reached callback, svFlags/eType, pos/apos stationary setup, and dlight flag checked against current decompiler output. */
void InitScriptMover(gentity_t *ent)
{
    game_compat_script_mover_init_light(ent);
    game_compat_script_mover_set_reached(ent, Reached_ScriptMover);
    ent->svFlags = SCRIPT_MOVER_SVFLAGS;
    ent->s.eType = ET_SCRIPTMOVER;
    game_compat_script_copy_vector(ent->s.pos.trBase, ent->currentOrigin);
    ent->s.pos.trType = TR_STATIONARY;
    game_compat_script_copy_vector(ent->s.apos.trBase, ent->currentAngles);
    ent->s.apos.trType = TR_STATIONARY;
    ent->flags |= FL_SUPPORTS_LINKTO;
}

/* VERIFIED_DECOMPILER(0x70570, 80570_SP_script_brushmodel.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - SetBrushModel, InitScriptMover, scriptContents +0x120 = 1, LinkEntity order, and void return checked against current decompiler output. */
void SP_script_brushmodel(gentity_t *ent)
{
    trap_SetBrushModel(ent);
    InitScriptMover(ent);
    ent->scriptContents = CONTENTS_SOLID;
    trap_LinkEntity(ent);
}

/* VERIFIED_DECOMPILER(0x705b6, 805b6_SP_script_model.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - DObj update, InitScriptMover, svFlags OR 4, scriptContents +0x120 = 0x2080, LinkEntity order, and void return checked against current decompiler output. */
void SP_script_model(gentity_t *ent)
{
    G_DObjUpdate(ent);
    InitScriptMover(ent);
    ent->svFlags |= SVF_DOBJ_USE_MODEL_BOUNDS;
    ent->scriptContents = SCRIPT_MODEL_CONTENTS;
    trap_LinkEntity(ent);
}

/* VERIFIED_DECOMPILER(0x70611, 80611_SP_script_origin.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - InitScriptMover, scriptContents clear, LinkEntity-before-light-flag order, constantLight branch, svFlags OR 1, s_flags OR 0x80, and void return checked against current decompiler output. */
void SP_script_origin(gentity_t *ent)
{
    InitScriptMover(ent);
    ent->scriptContents = SCRIPT_ORIGIN_CONTENTS;
    trap_LinkEntity(ent);

    if (ent->s.constantLight == 0) {
        ent->svFlags |= SVF_NOCLIENT;
    } else {
        ent->s.eFlags |= EF_NODRAW;
    }
}

/* VERIFIED_DECOMPILER(0x70681, 80681_ScriptEntCmdGetCommandTimes.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - total/accel/decel parameter indices, positive/nonnegative checks, optional-parameter branches, zero defaults, total-vs-accel+decel error, stores, and void return checked against current decompiler output. */
void ScriptEntCmdGetCommandTimes(float *totalTime, float *accelTime, float *decelTime)
{
    int paramCount;

    *totalTime = Scr_GetFloat(1);
    if (*totalTime <= 0.0f) {
        Scr_ParamError(1, "total time must be positive");
    }

    paramCount = Scr_GetNumParam();
    if (paramCount < 3) {
        *accelTime = 0.0f;
        *decelTime = 0.0f;
    } else {
        *accelTime = Scr_GetFloat(2);
        if (*accelTime < 0.0f) {
            Scr_ParamError(2, "accel time must be nonnegative");
        }

        if (paramCount < 4) {
            *decelTime = 0.0f;
        } else {
            *decelTime = Scr_GetFloat(3);
            if (*decelTime < 0.0f) {
                Scr_ParamError(3, "decel time must be nonnegative");
            }
        }
    }

    if (*totalTime < *accelTime + *decelTime) {
        Scr_Error("accel time plus decel time is greater than total time");
    }
}

/* VERIFIED_DECOMPILER(0x707a9, 807a9_ScriptEntCmd_MoveTo.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - entity-number/class validation helper, command-block flag +0x18c bit 4, vector argument 0, command-time helper argument order, ScriptMover_Move call, and void return checked against current decompiler output. */
void ScriptEntCmd_MoveTo(uint32_t scriptObject)
{
    gentity_t *ent = game_compat_script_mover_command_entity(scriptObject);
    vec3_t target;
    float totalTime;
    float accelTime;
    float decelTime;

    if (ent == 0 || game_compat_script_mover_commands_blocked(ent)) {
        return;
    }

    Scr_GetVector(0, target);
    ScriptEntCmdGetCommandTimes(&totalTime, &accelTime, &decelTime);
    ScriptMover_Move(ent, target, totalTime, accelTime, decelTime);
}

/* VERIFIED_DECOMPILER(0x708cd, 808cd_ScriptEntCmd_GravityMove.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - entity-number/class validation helper, command-block flag +0x18c bit 4, vector argument 0, duration argument 1, ScriptMover_GravityMove call, and void return checked against current decompiler output. */
void ScriptEntCmd_GravityMove(uint32_t scriptObject)
{
    gentity_t *ent = game_compat_script_mover_command_entity(scriptObject);
    vec3_t velocity;
    float duration;

    if (ent == 0 || game_compat_script_mover_commands_blocked(ent)) {
        return;
    }

    Scr_GetVector(0, velocity);
    duration = Scr_GetFloat(1);
    ScriptMover_GravityMove(ent, velocity, duration);
}

/* VERIFIED_DECOMPILER(0x709d9, 809d9_ScriptEnt_MoveAxis.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - entity-number/class validation helper, command-block flag +0x18c bit 4, distance argument 0, command-time helper argument order, currentOrigin +0x13c copy, axis-indexed add, ScriptMover_Move call, and void return checked against current decompiler output. */
void ScriptEnt_MoveAxis(uint32_t scriptObject, int axis)
{
    gentity_t *ent = game_compat_script_mover_command_entity(scriptObject);
    vec3_t target;
    float distance;
    float totalTime;
    float accelTime;
    float decelTime;

    if (ent == 0 || game_compat_script_mover_commands_blocked(ent)) {
        return;
    }

    distance = Scr_GetFloat(0);
    ScriptEntCmdGetCommandTimes(&totalTime, &accelTime, &decelTime);
    game_compat_script_copy_vector(target, ent->currentOrigin);
    target[axis] += distance;
    ScriptMover_Move(ent, target, totalTime, accelTime, decelTime);
}

/* VERIFIED_DECOMPILER(0x70b31, 80b31_ScriptEntCmd_MoveX.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - ScriptEnt_MoveAxis forwarding with axis 0 and void return checked against current decompiler output. */
void ScriptEntCmd_MoveX(uint32_t scriptObject)
{
    ScriptEnt_MoveAxis(scriptObject, 0);
}

/* VERIFIED_DECOMPILER(0x70b5c, 80b5c_ScriptEntCmd_MoveY.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - ScriptEnt_MoveAxis forwarding with axis 1 and void return checked against current decompiler output. */
void ScriptEntCmd_MoveY(uint32_t scriptObject)
{
    ScriptEnt_MoveAxis(scriptObject, 1);
}

/* VERIFIED_DECOMPILER(0x70b87, 80b87_ScriptEntCmd_MoveZ.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - ScriptEnt_MoveAxis forwarding with axis 2 and void return checked against current decompiler output. */
void ScriptEntCmd_MoveZ(uint32_t scriptObject)
{
    ScriptEnt_MoveAxis(scriptObject, 2);
}

/* VERIFIED_DECOMPILER(0x70bb2, 80bb2_ScriptEntCmd_RotateTo.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - entity-number/class validation helper, command-block flag +0x18c bit 4, vector argument 0, command-time helper argument order, three-axis AngleSubtract loop against currentAngles +0x148, ScriptMover_Rotate call, and void return checked against current decompiler output. */
void ScriptEntCmd_RotateTo(uint32_t scriptObject)
{
    gentity_t *ent = game_compat_script_mover_command_entity(scriptObject);
    vec3_t target;
    vec3_t adjustedTarget;
    float totalTime;
    float accelTime;
    float decelTime;

    if (ent == 0 || game_compat_script_mover_commands_blocked(ent)) {
        return;
    }

    Scr_GetVector(0, target);
    ScriptEntCmdGetCommandTimes(&totalTime, &accelTime, &decelTime);

    for (int axis = 0; axis < 3; axis++) {
        adjustedTarget[axis] = ent->currentAngles[axis] + AngleSubtract(target[axis], ent->currentAngles[axis]);
    }

    ScriptMover_Rotate(ent, adjustedTarget, totalTime, accelTime, decelTime);
}

/* VERIFIED_DECOMPILER(0x70d2f, 80d2f_ScriptEnt_RotateAxis.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - entity-number/class validation helper, command-block flag +0x18c bit 4, distance argument 0, command-time helper argument order, currentAngles +0x148 copy, axis-indexed add, ScriptMover_Rotate call, and void return checked against current decompiler output. */
void ScriptEnt_RotateAxis(uint32_t scriptObject, int axis)
{
    gentity_t *ent = game_compat_script_mover_command_entity(scriptObject);
    vec3_t target;
    float distance;
    float totalTime;
    float accelTime;
    float decelTime;

    if (ent == 0 || game_compat_script_mover_commands_blocked(ent)) {
        return;
    }

    distance = Scr_GetFloat(0);
    ScriptEntCmdGetCommandTimes(&totalTime, &accelTime, &decelTime);
    game_compat_script_copy_vector(target, ent->currentAngles);
    target[axis] += distance;
    ScriptMover_Rotate(ent, target, totalTime, accelTime, decelTime);
}

/* VERIFIED_DECOMPILER(0x70e87, 80e87_ScriptEntCmd_RotatePitch.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - ScriptEnt_RotateAxis forwarding with axis 0 and void return checked against current decompiler output. */
void ScriptEntCmd_RotatePitch(uint32_t scriptObject)
{
    ScriptEnt_RotateAxis(scriptObject, 0);
}

/* VERIFIED_DECOMPILER(0x70eb2, 80eb2_ScriptEntCmd_RotateYaw.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - ScriptEnt_RotateAxis forwarding with axis 1 and void return checked against current decompiler output. */
void ScriptEntCmd_RotateYaw(uint32_t scriptObject)
{
    ScriptEnt_RotateAxis(scriptObject, 1);
}

/* VERIFIED_DECOMPILER(0x70edd, 80edd_ScriptEntCmd_RotateRoll.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - ScriptEnt_RotateAxis forwarding with axis 2 and void return checked against current decompiler output. */
void ScriptEntCmd_RotateRoll(uint32_t scriptObject)
{
    ScriptEnt_RotateAxis(scriptObject, 2);
}

/* VERIFIED_DECOMPILER(0x70f08, 80f08_ScriptEntCmd_RotateVelocity.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - entity-number/class validation helper, command-block flag +0x18c bit 4, vector argument 0, command-time helper argument order, ScriptMover_RotateSpeed call, and void return checked against current decompiler output. */
void ScriptEntCmd_RotateVelocity(uint32_t scriptObject)
{
    gentity_t *ent = game_compat_script_mover_command_entity(scriptObject);
    vec3_t velocity;
    float totalTime;
    float accelTime;
    float decelTime;

    if (ent == 0 || game_compat_script_mover_commands_blocked(ent)) {
        return;
    }

    Scr_GetVector(0, velocity);
    ScriptEntCmdGetCommandTimes(&totalTime, &accelTime, &decelTime);
    ScriptMover_RotateSpeed(ent, velocity, totalTime, accelTime, decelTime);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from recovered script builtin behavior (game_compat_script_ent_set_solid). */
static void game_compat_script_ent_set_solid(uint32_t scriptObject, int contents)
{
    gentity_t *ent = game_compat_script_mover_command_entity(scriptObject);

    if (ent == 0) {
        return;
    }

    if (game_compat_script_mover_classname(ent) == scr_const_script_origin) {
        G_DPrintf("cannot use the solid/notsolid commands on a script_origin entity\n");
    } else if (game_compat_script_mover_classname(ent) == scr_const_script_model) {
        G_DPrintf("cannot use the solid/notsolid commands on a script_model entity\n");
    } else {
        ent->scriptContents = contents;
    }
}

/* VERIFIED_DECOMPILER(0x7102c, 8102c_ScriptEntCmd_Solid.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - entity-number/class validation helper, origin/model diagnostic branches, brushmodel scriptContents +0x120 store = 1 through game_compat_script_ent_set_solid helper, and void return checked against current decompiler output. */
void ScriptEntCmd_Solid(uint32_t scriptObject)
{
    game_compat_script_ent_set_solid(scriptObject, CONTENTS_SOLID);
}

/* VERIFIED_DECOMPILER(0x71147, 81147_ScriptEntCmd_NotSolid.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - entity-number/class validation helper, origin/model diagnostic branches, brushmodel scriptContents +0x120 store = 0 through game_compat_script_ent_set_solid helper, and void return checked against current decompiler output. */
void ScriptEntCmd_NotSolid(uint32_t scriptObject)
{
    game_compat_script_ent_set_solid(scriptObject, SCRIPT_ORIGIN_CONTENTS);
}

/* VERIFIED_DECOMPILER(0x6f418, 7f418_Reached_ScriptMover.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - position and angle trajectory active/time guards, ScriptMover_Updatemove argument offsets, trajectory evaluation/link order, movedone/rotatedone notifications, angle normalization calls, and void return checked against current decompiler output.
 * Mac PEF symbol evidence: game_mp.dll code 0x45ee0, size 0x164.
 */
void Reached_ScriptMover(gentity_t *ent)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (ent->s.pos.trType != TR_STATIONARY && ent->s.pos.trTime + ent->s.pos.trDuration <= level.time) {
        int reached = ScriptMover_Updatemove(&ent->s.pos, ent->currentOrigin, *game_compat_script_mover_pos_speed(ent),
                                             *game_compat_script_mover_pos_linear_time(ent), *game_compat_script_mover_pos_decel_time(ent),
                                             game_compat_script_mover_pos_linear_start(ent), game_compat_script_mover_pos_decel_start(ent),
                                             game_compat_script_mover_pos_target(ent));

        BG_EvaluateTrajectory(&ent->s.pos, level.time, ent->currentOrigin);
        trap_LinkEntity(ent);

        if (reached != 0) {
            Scr_Notify(ent, scr_const_movedone, 0);
        }
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (ent->s.apos.trType != TR_STATIONARY && ent->s.apos.trTime + ent->s.apos.trDuration <= level.time) {
        int reached = ScriptMover_Updatemove(
            &ent->s.apos, ent->currentAngles, *game_compat_script_mover_apos_speed(ent), *game_compat_script_mover_apos_linear_time(ent),
            *game_compat_script_mover_apos_decel_time(ent), game_compat_script_mover_apos_linear_start(ent),
            game_compat_script_mover_apos_decel_start(ent), game_compat_script_mover_apos_target(ent));

        BG_EvaluateTrajectory(&ent->s.apos, level.time, ent->currentAngles);
        trap_LinkEntity(ent);

        if (reached != 0) {
            ent->currentAngles[0] = AngleNormalize180(ent->currentAngles[0]);
            ent->currentAngles[1] = AngleNormalize360(ent->currentAngles[1]);
            ent->currentAngles[2] = AngleNormalize180(ent->currentAngles[2]);
            Scr_Notify(ent, scr_const_rotatedone, 0);
        }
    }
}

/* VERIFIED_DECOMPILER(0x6e75f, 7e75f_Scr_GetMethod.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - developerOnly clear, Player/ScriptEnt/ScriptVehicle/HudElem/BuiltIn lookup order, short-circuit return value, and no name mutation beyond callees checked against current decompiler output. */
script_method_callback_t Scr_GetMethod(const char **name, int *developerOnly)
{
    script_method_callback_t callback;

    *developerOnly = 0;

    callback = Player_GetMethod(name);
    if (callback != 0) {
        return callback;
    }

    callback = ScriptEnt_GetMethod(name);
    if (callback != 0) {
        return callback;
    }

    callback = ScriptVehicle_GetMethod(name);
    if (callback != 0) {
        return callback;
    }

    callback = HudElem_GetMethod(name);
    if (callback != 0) {
        return callback;
    }

    return BuiltIn_GetMethod(name);
}

/* VERIFIED_DECOMPILER(0x71262, 81262_ScriptEnt_GetMethod.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - 12-entry script entity method scan, strcmp input snapshot, canonical name store, callback return, null miss return, and table order checked against current decompiler output. */
script_method_callback_t ScriptEnt_GetMethod(const char **name)
{
    for (uint32_t index = 0; index < SCRIPTENT_METHOD_COUNT; index++) {
        if (strcmp(*name, scriptent_methods[index].name) == 0) {
            *name = scriptent_methods[index].name;
            return scriptent_methods[index].callback;
        }
    }

    return 0;
}

/* VERIFIED_DECOMPILER(0x6e6e4, 7e6e4_FUN_0007e6e4.c, VERIFY-SCRIPT-BUILTINS-ENTITY-METHODS-2026-06-17): DATAFLOW_VERIFIED - 56-entry builtin method scan, strcmp input snapshot, canonical name store, callback return, null miss return, and table order checked against current decompiler output. */
script_method_callback_t BuiltIn_GetMethod(const char **name)
{
    for (uint32_t index = 0; index < SCRIPTBUILTIN_METHOD_COUNT; index++) {
        if (strcmp(*name, scriptbuiltin_methods[index].name) == 0) {
            *name = scriptbuiltin_methods[index].name;
            return scriptbuiltin_methods[index].callback;
        }
    }

    return 0;
}
