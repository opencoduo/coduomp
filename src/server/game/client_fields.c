/*
 * Source reconstruction for player/client script fields.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>

#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */
#include "recovered_game.h"
#include "game_globals.h"
#include "game_functions.h"
#include "scr_vm.h"

/* Private descriptor used only by the client script-field table below. */
typedef struct client_field_s client_field_t;
typedef void (*client_field_callback_t)(gclient_t *client, gclient_t *self,
                                        const client_field_t *field);

struct client_field_s {
    const char *name;
    size_t offset;
    int32_t type;
    client_field_callback_t setter;
    client_field_callback_t getter;
};

#define CLIENT_FIELD_CLASS_OFFSET 0xc000u
#define STATUS_ICON_CONFIGSTRING_BASE (CS_STATUS_ICONS - 1)
#define STATUS_ICON_LIMIT (CS_STATUS_ICONS_COUNT + 1)
#define HEAD_ICON_CONFIGSTRING_BASE (CS_HEAD_ICONS - 1)
#define HEAD_ICON_LIMIT (CS_HEAD_ICONS_COUNT + 1)
#define CLIENT_FIELD_MAX_CLIENTS 64
/* RESOLVED(UO-GAME-UNK-0072): script-visible client field table offsets.
 * Offsets for fields accessed via Scr_GetGenericField/Scr_SetGenericField
 * are stored as binary byte offsets, but are derived from named gclient_t fields.
 * Fields with custom getter/setter functions use struct field access directly.
 */
/* GENTITY_OFFSET_HEAD_ICON, HEAD_ICON_TEAM, MAX_SPEED -- all replaced
 * by gentity_t struct field access (headIcon, headIconTeam, maxSpeed). */
#define CLIENT_FIELD_OFFSET(field) offsetof(gclient_t, field)
#define CLIENT_FIELD_NAME CLIENT_FIELD_OFFSET(userInfoName)
#define CLIENT_FIELD_MAX_HEALTH CLIENT_FIELD_OFFSET(normalMaxHealth)
#define CLIENT_FIELD_MAX_SPEED CLIENT_FIELD_OFFSET(maxSpeed)
#define CLIENT_FIELD_HANDICAP CLIENT_FIELD_OFFSET(handicap)
#define CLIENT_FIELD_SCORE CLIENT_FIELD_OFFSET(score)
#define CLIENT_FIELD_DEATHS CLIENT_FIELD_OFFSET(deaths)
#define CLIENT_FIELD_SPECTATOR_CLIENT CLIENT_FIELD_OFFSET(followClient)
#define CLIENT_FIELD_ARCHIVE_TIME CLIENT_FIELD_OFFSET(archiveTime)
#define CLIENT_FIELD_PERS CLIENT_FIELD_OFFSET(pers)
#define CLIENT_FIELD_NO_INACTIVITY_KICK \
    CLIENT_FIELD_OFFSET(spectatorActivityState)
#define CLIENT_FIELD_ARCHIVE_MILLISECONDS 1000.0f
#define CLIENT_FIELD_ARCHIVE_SECONDS_SCALE 0.001f

void CalculateRanks(void);
void trap_GetConfigstring(int index, char *buffer, int bufferLength);

/* VERIFIED_DECOMPILER(0x43644, 53644_FUN_00053644.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_ReadOnly(gclient_t *client, gclient_t *self,
                          const client_field_t *field)
{
    (void)client;
    (void)self;

    Scr_Error(va("player field %s is read-only\n", field->name));
}

/* VERIFIED_DECOMPILER(0x4367b, 5367b_FUN_0005367b.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_SetSessionTeam(gclient_t *client, gclient_t *self,
                                const client_field_t *field)
{
    uint16_t value = Scr_GetConstString(0);

    (void)self;
    (void)field;

    if (value == scr_const_axis) {
        client->sessionTeam = TEAM_AXIS;
    } else if (value == scr_const_allies) {
        client->sessionTeam = TEAM_ALLIES;
    } else if (value == scr_const_spectator) {
        client->sessionTeam = TEAM_SPECTATOR;
    } else if (value == scr_const_none) {
        client->sessionTeam = TEAM_FREE;
    } else {
        Scr_Error(va("'%s' is an illegal sessionteam string. Must be allies, "
                     "axis, none, or spectator.",
                     SL_ConvertToString(value)));
    }

    ClientUserinfoChanged((int)(client - level.clients));
    CalculateRanks();
}

/* VERIFIED_DECOMPILER(0x43771, 53771_FUN_00053771.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_GetSessionTeam(gclient_t *client, gclient_t *self,
                                const client_field_t *field)
{
    (void)self;
    (void)field;

    if (client->sessionTeam == TEAM_AXIS) {
        Scr_AddConstString(scr_const_axis);
    } else if (client->sessionTeam == TEAM_FREE) {
        Scr_AddConstString(scr_const_none);
    } else if (client->sessionTeam == TEAM_ALLIES) {
        Scr_AddConstString(scr_const_allies);
    } else if (client->sessionTeam == TEAM_SPECTATOR) {
        Scr_AddConstString(scr_const_spectator);
    }
}

/* VERIFIED_DECOMPILER(0x43803, 53803_FUN_00053803.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_SetSessionSquad(gclient_t *client, gclient_t *self,
                                 const client_field_t *field)
{
    uint16_t value = Scr_GetConstString(0);

    (void)self;
    (void)field;

    if (value == scr_const_squad_alpha) {
        client->sessionSquad = SESS_SQUAD_ALPHA;
    } else if (value == scr_const_squad_bravo) {
        client->sessionSquad = SESS_SQUAD_BRAVO;
    } else if (value == scr_const_none) {
        client->sessionSquad = SESS_SQUAD_NONE;
    } else {
        Scr_Error(va("'%s' is an illegal sessionsquad string. Must be "
                     "squad_alpha, squad_bravo, or none.",
                     SL_ConvertToString(value)));
    }

    ClientUserinfoChanged((int)(client - level.clients));
}

/* VERIFIED_DECOMPILER(0x438d2, 538d2_FUN_000538d2.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_GetSessionSquad(gclient_t *client, gclient_t *self,
                                 const client_field_t *field)
{
    int sessionSquad = client->sessionSquad;

    (void)self;
    (void)field;

    if (sessionSquad == SESS_SQUAD_ALPHA) {
        Scr_AddConstString(scr_const_squad_alpha);
    } else if (sessionSquad == SESS_SQUAD_NONE) {
        Scr_AddConstString(scr_const_none);
    } else if (sessionSquad == SESS_SQUAD_BRAVO) {
        Scr_AddConstString(scr_const_squad_bravo);
    }
}

/* VERIFIED_DECOMPILER(0x43956, 53956_FUN_00053956.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_SetSessionState(gclient_t *client, gclient_t *self,
                                 const client_field_t *field)
{
    uint16_t value = Scr_GetConstString(0);

    (void)self;
    (void)field;

    if (value == scr_const_playing) {
        client->sessionState = SESS_STATE_PLAYING;
    } else if (value == scr_const_dead) {
        client->sessionState = SESS_STATE_DEAD;
    } else if (value == scr_const_spectator) {
        client->sessionState = SESS_STATE_SPECTATOR;
    } else if (value == scr_const_intermission) {
        client->sessionState = SESS_STATE_INTERMISSION;
    } else {
        Scr_Error(va("'%s' is an illegal sessionstate string. Must be playing, "
                     "dead, spectator, or intermission.",
                     SL_ConvertToString(value)));
    }
}

/* VERIFIED_DECOMPILER(0x43a2f, 53a2f_FUN_00053a2f.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_GetSessionState(gclient_t *client, gclient_t *self,
                                 const client_field_t *field)
{
    (void)self;
    (void)field;

    if (client->sessionState == SESS_STATE_DEAD) {
        Scr_AddConstString(scr_const_dead);
    } else if (client->sessionState == SESS_STATE_PLAYING) {
        Scr_AddConstString(scr_const_playing);
    } else if (client->sessionState == SESS_STATE_SPECTATOR) {
        Scr_AddConstString(scr_const_spectator);
    } else if (client->sessionState == SESS_STATE_INTERMISSION) {
        Scr_AddConstString(scr_const_intermission);
    }
}

/* VERIFIED_DECOMPILER(0x43ac7, 53ac7_FUN_00053ac7.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_SetMaxHealth(gclient_t *client, gclient_t *self,
                              const client_field_t *field)
{
    (void)self;
    (void)field;

    client->normalMaxHealth = Scr_GetInt(0);
    if (client->normalMaxHealth < 1) {
        client->normalMaxHealth = 1;
    }

    if (client->normalMaxHealth < client->ps.stats[STAT_HEALTH]) {
        client->ps.stats[STAT_HEALTH] = client->normalMaxHealth;
    }

    g_entities[(int)(client - level.clients)].health =
        client->ps.stats[STAT_HEALTH];
    client->ps.stats[STAT_MAX_HEALTH] = client->normalMaxHealth;
}

/* VERIFIED_DECOMPILER(0x43b82, 53b82_FUN_00053b82.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_SetMaxSpeed(gclient_t *client, gclient_t *self,
                             const client_field_t *field)
{
    (void)self;
    (void)field;

    client->maxSpeed = Scr_GetInt(0);
    if (client->maxSpeed < 0) {
        client->maxSpeed = 0;
    }

    g_entities[(int)(client - level.clients)].maxSpeed =
        (float)client->maxSpeed;
}

/* VERIFIED_DECOMPILER(0x43c05, 53c05_FUN_00053c05.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_SetScore(gclient_t *client, gclient_t *self,
                          const client_field_t *field)
{
    (void)self;
    (void)field;

    client->score = Scr_GetInt(0);
    CalculateRanks();
}

/* VERIFIED_DECOMPILER(0x43c3d, 53c3d_FUN_00053c3d.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_SetSpectatorClient(gclient_t *client, gclient_t *self,
                                    const client_field_t *field)
{
    int clientNum = Scr_GetInt(0);

    (void)self;
    (void)field;

    if (clientNum < -1 || clientNum >= CLIENT_FIELD_MAX_CLIENTS) {
        Scr_Error("spectatorclient can only be set to -1, or a valid client number");
    }

    client->followClient = clientNum;
}

/* VERIFIED_DECOMPILER(0x43c8c, 53c8c_FUN_00053c8c.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_SetStatusIcon(gclient_t *client, gclient_t *self,
                               const client_field_t *field)
{
    (void)self;
    (void)field;

    client->statusIcon = GScr_GetStatusIconIndex(Scr_GetString(0));
}

/* VERIFIED_DECOMPILER(0x43cc9, 53cc9_FUN_00053cc9.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_GetStatusIcon(gclient_t *client, gclient_t *self,
                               const client_field_t *field)
{
    int icon = client->statusIcon;
    char configString[MAX_STRING_CHARS];

    (void)self;
    (void)field;

    if (icon == 0) {
        Scr_AddString("");
    } else if (icon < STATUS_ICON_LIMIT) {
        trap_GetConfigstring(icon + STATUS_ICON_CONFIGSTRING_BASE, configString,
                             sizeof(configString));
        Scr_AddString(configString);
    }
}

/* VERIFIED_DECOMPILER(0x43d43, 53d43_FUN_00053d43.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_SetHeadIcon(gclient_t *client, gclient_t *self,
                             const client_field_t *field)
{
    gentity_t *ent = &g_entities[(int)(client - level.clients)];

    (void)self;
    (void)field;

    ent->s.headIcon = GScr_GetHeadIconIndex(Scr_GetString(0));
}

/* VERIFIED_DECOMPILER(0x43da9, 53da9_FUN_00053da9.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_GetHeadIcon(gclient_t *client, gclient_t *self,
                             const client_field_t *field)
{
    gentity_t *ent = &g_entities[(int)(client - level.clients)];
    int icon = ent->s.headIcon;
    char configString[MAX_STRING_CHARS];

    (void)self;
    (void)field;

    if (icon == 0) {
        Scr_AddString("");
    } else if (icon < HEAD_ICON_LIMIT) {
        trap_GetConfigstring(icon + HEAD_ICON_CONFIGSTRING_BASE, configString,
                             sizeof(configString));
        Scr_AddString(configString);
    }
}

/* VERIFIED_DECOMPILER(0x43e4c, 53e4c_FUN_00053e4c.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_SetHeadIconTeam(gclient_t *client, gclient_t *self,
                                 const client_field_t *field)
{
    gentity_t *ent = &g_entities[(int)(client - level.clients)];
    uint16_t value = Scr_GetConstString(0);

    (void)self;
    (void)field;

    if (value == scr_const_none) {
        ent->s.headIconTeam = TEAM_FREE;
    } else if (value == scr_const_allies) {
        ent->s.headIconTeam = TEAM_ALLIES;
    } else if (value == scr_const_axis) {
        ent->s.headIconTeam = TEAM_AXIS;
    } else if (value == scr_const_spectator) {
        ent->s.headIconTeam = TEAM_SPECTATOR;
    } else {
        Scr_Error(va("'%s' is an illegal head icon team string. Must be none, "
                     "allies, axis, or spectator.",
                     SL_ConvertToString(value)));
    }
}

/* VERIFIED_DECOMPILER(0x43f48, 53f48_FUN_00053f48.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_GetHeadIconTeam(gclient_t *client, gclient_t *self,
                                 const client_field_t *field)
{
    int headIconTeam = g_entities[(int)(client - level.clients)].s.headIconTeam;

    (void)self;
    (void)field;

    if (headIconTeam == TEAM_AXIS) {
        Scr_AddConstString(scr_const_axis);
    } else if (headIconTeam == TEAM_ALLIES) {
        Scr_AddConstString(scr_const_allies);
    } else if (headIconTeam == TEAM_SPECTATOR) {
        Scr_AddConstString(scr_const_spectator);
    } else {
        Scr_AddConstString(scr_const_none);
    }
}

/* VERIFIED_DECOMPILER(0x44005, 54005_FUN_00054005.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_SetArchiveTime(gclient_t *client, gclient_t *self,
                                const client_field_t *field)
{
    (void)self;
    (void)field;

    /* 0x44024..0x44041: the product feeds truncating fistp directly. */
#if EMULATE_X87
    client->archiveTime = x87f_store_i32_trunc(x87f_mul(
        x87f_load_f32(Scr_GetFloat(0)),
        x87f_load_f32(CLIENT_FIELD_ARCHIVE_MILLISECONDS)));
#else
    client->archiveTime =
        game_compat_int32_from_long_double_trunc(
            (long double)Scr_GetFloat(0) *
            (long double)CLIENT_FIELD_ARCHIVE_MILLISECONDS);
#endif
}

/* VERIFIED_DECOMPILER(0x44051, 54051_FUN_00054051.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ClientScr_GetArchiveTime(gclient_t *client, gclient_t *self,
                                const client_field_t *field)
{
    (void)self;
    (void)field;

    Scr_AddFloat((float)((long double)client->archiveTime *
                         (long double)CLIENT_FIELD_ARCHIVE_SECONDS_SCALE));
}

static const client_field_t clientFields[] = {
    {"name", CLIENT_FIELD_NAME, 4, ClientScr_ReadOnly, 0},
    {"sessionteam", 0, 5, ClientScr_SetSessionTeam, ClientScr_GetSessionTeam},
    {"sessionsquad", 0, 5, ClientScr_SetSessionSquad, ClientScr_GetSessionSquad},
    {"sessionstate", 0, 5, ClientScr_SetSessionState, ClientScr_GetSessionState},
    {"maxhealth", CLIENT_FIELD_MAX_HEALTH, 0, ClientScr_SetMaxHealth, 0},
    {"maxspeed", CLIENT_FIELD_MAX_SPEED, 0, ClientScr_SetMaxSpeed, 0},
    {"handicap", CLIENT_FIELD_HANDICAP, 0, ClientScr_ReadOnly, 0},
    {"score", CLIENT_FIELD_SCORE, 0, ClientScr_SetScore, 0},
    {"deaths", CLIENT_FIELD_DEATHS, 0, 0, 0},
    {"statusicon", 0, 5, ClientScr_SetStatusIcon, ClientScr_GetStatusIcon},
    {"headicon", 0, 5, ClientScr_SetHeadIcon, ClientScr_GetHeadIcon},
    {"headiconteam", 0, 5, ClientScr_SetHeadIconTeam, ClientScr_GetHeadIconTeam},
    {"spectatorclient", CLIENT_FIELD_SPECTATOR_CLIENT, 0,
     ClientScr_SetSpectatorClient, 0},
    {"archivetime", CLIENT_FIELD_ARCHIVE_TIME, 3, ClientScr_SetArchiveTime,
     ClientScr_GetArchiveTime},
    {"pers", CLIENT_FIELD_PERS, 10, ClientScr_ReadOnly, 0},
    {"noInactivityKick", CLIENT_FIELD_NO_INACTIVITY_KICK, 0, 0, 0},
    {0, 0, 0, 0, 0},
};

/* VERIFIED_DECOMPILER(0x44082, 54082_GScr_AddFieldsForClient.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void GScr_AddFieldsForClient(uint16_t classnum)
{
    for (uint32_t index = 0; clientFields[index].name != 0; index++) {
        const client_field_t *field = &clientFields[index];

        switch (field->type) {
        case 0:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 10:
            Scr_AddClassField(classnum, field->name,
                              (uint16_t)(CLIENT_FIELD_CLASS_OFFSET | index));
            break;
        default:
            break;
        }
    }
}

/* VERIFIED_DECOMPILER(0x44118, 54118_Scr_SetClientField.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void Scr_SetClientField(gclient_t *client, int fieldIndex)
{
    const client_field_t *field = &clientFields[fieldIndex];

    if (field->setter == 0) {
        Scr_SetGenericField(client, field->type, field->offset);
    } else {
        field->setter(client, client, field);
    }
}

/* VERIFIED_DECOMPILER(0x44192, 54192_Scr_GetClientField.c, VERIFY-CLIENT-FIELDS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void Scr_GetClientField(gclient_t *client, int fieldIndex)
{
    const client_field_t *field = &clientFields[fieldIndex];

    if (field->getter == 0) {
        Scr_GetGenericField(client, field->type, field->offset);
    } else {
        field->getter(client, client, field);
    }
}
