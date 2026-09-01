/*
 * Source-owned shared globals recovered from the game module data segment.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stddef.h>
#include <stdint.h>

#include "game_globals.h"
#include "entity_dispatch_private.h"

level_locals_t level = { 0 };
bgs_t bgs = { 0 };
gentity_t g_entities[MAX_GENTITIES] = { 0 };
gclient_t g_clients[MAX_CLIENTS] = { 0 };
gitem_t bg_itemlist[BG_ITEMLIST_SLOT_COUNT] = { 0 };
bg_t bg = { 0 };

vec3_t playerMins = { -15.0f, -15.0f, 0.0f };
vec3_t playerMaxs = { 15.0f, 15.0f, 72.0f };

const char emptyString[] = "";
const char zeroString[] = "0";
const char g_log_path[] = "games_mp.log";
const char unknownCmdFmt[] = "e \"GAME_UNKNOWNCLIENTCOMMAND\x15%s\"";

mover_push_record_t *moverPushStackCursor;
turret_state_t g_turretStates[MAX_TURRETS];

uint8_t bulletPriorityMap[] = {
    1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0
};
uint8_t riflePriorityMap[] = {
    1, 9, 9, 9, 8, 7, 6, 6, 6, 6, 5, 5, 4, 4, 4, 4, 3, 3, 0
};

const char *modNames[] = {
    "MOD_UNKNOWN",
    "MOD_PISTOL_BULLET",
    "MOD_RIFLE_BULLET",
    "MOD_GRENADE",
    "MOD_GRENADE_SPLASH",
    "MOD_PROJECTILE",
    "MOD_PROJECTILE_SPLASH",
    "MOD_MELEE",
    "MOD_HEAD_SHOT",
    "MOD_MORTAR",
    "MOD_MORTAR_SPLASH",
    "MOD_DYNAMITE",
    "MOD_DYNAMITE_SPLASH",
    "MOD_ARTILLERY",
    "MOD_ARTILLERY_SPLASH",
    "MOD_WATER",
    "MOD_CRUSH",
    "MOD_CRUSH_TANK",
    "MOD_CRUSH_JEEP",
    "MOD_TELEFRAG",
    "MOD_FALLING",
    "MOD_SUICIDE",
    "MOD_TRIGGER_HURT",
    "MOD_EXPLOSIVE",
    "MOD_COLLISION",
    "MOD_FLAME",
    "MOD_MELEE_BINOCULARS"
};

/* Stock event-name string table recovered from 4d2391c1 .rodata string order. */
const char *eventnames[EV_MAX_EVENTS] = {
#include "qcommon/entity_event_names.inc"
};
const char **pEventNamesList = eventnames;

const char *hintStrings[] = {
    "",
    "HINT_NONE",
    "HINT_ACTIVATE",
    "HINT_NOACTIVATE",
    "HINT_DOOR",
    "HINT_DOOR_LOCKED",
    "HINT_MG42",
    "HINT_LMG",
    "HINT_HEALTH",
    "HINT_LADDER",
    "HINT_FRIENDLY",
    NULL
};

uint16_t scr_const[SCR_CONST_COUNT];

vmCvar_t g_cheats;
vmCvar_t g_night;
vmCvar_t g_obituary;
vmCvar_t g_gametype;
vmCvar_t g_maxclients;
vmCvar_t g_synchronousClients;
vmCvar_t g_intermissionDelay;
vmCvar_t g_log;
vmCvar_t g_logSync;
vmCvar_t g_password;
vmCvar_t g_banIPs;
vmCvar_t g_filterBan;
vmCvar_t g_dedicated;
vmCvar_t g_speed;
vmCvar_t g_gravity;
vmCvar_t g_knockback;
vmCvar_t g_weaponRespawn;
vmCvar_t g_weaponAmmoPools;
vmCvar_t g_maxDroppedWeapons;
vmCvar_t g_inactivity;
vmCvar_t g_inactivityspectator;
vmCvar_t g_debugMove;
vmCvar_t g_debugProneCheck;
vmCvar_t g_debugProneCheckDepthCheck;
/* NOT_FROM_ORIGINAL_SOURCE: gated spectator/archive diagnostics. */
vmCvar_t g_debugArchiveCheck;
/* NOT_FROM_ORIGINAL_SOURCE: gated corpse clone snapshot diagnostics. */
vmCvar_t g_debugCorpseCheck;
/* NOT_FROM_ORIGINAL_SOURCE: gated water movement diagnostics. */
vmCvar_t g_debugWaterCheck;
vmCvar_t g_debugDamage;
vmCvar_t g_debugAlloc;
vmCvar_t g_debugBullets;
vmCvar_t g_motd;
vmCvar_t g_tracerChance;
vmCvar_t g_tracerChanceLMG;
vmCvar_t g_vehicleDrawPath;
vmCvar_t g_vehicleDebug;
vmCvar_t g_vehicleTexScrollScale;
vmCvar_t g_vehicleForceBulletDamage;
vmCvar_t g_vehicleForceGrenadeDamage;
vmCvar_t g_vehicleEnableCollisionDamage;
vmCvar_t g_vehicleTrafficStressTest;
vmCvar_t g_vehicleHorns;
vmCvar_t g_vehicleBurnTime;
vmCvar_t g_allowVote;
vmCvar_t g_allowVoteMapRestart;
vmCvar_t g_allowVoteMapRotate;
vmCvar_t g_allowVoteTypeMap;
vmCvar_t g_allowVoteMap;
vmCvar_t g_allowVoteGameType;
vmCvar_t g_allowVoteKick;
vmCvar_t g_allowVoteClientKick;
vmCvar_t g_allowVoteTempBanUser;
vmCvar_t g_allowVoteTempBanClient;
vmCvar_t g_allowVoteDrawFriend;
vmCvar_t g_allowVoteKillCam;
vmCvar_t g_allowVoteFriendlyFire;
vmCvar_t g_listEntity;
vmCvar_t ui_allowVote;
vmCvar_t ui_allowVoteMapRestart;
vmCvar_t ui_allowVoteMapRotate;
vmCvar_t ui_allowVoteTypeMap;
vmCvar_t ui_allowVoteMap;
vmCvar_t ui_allowVoteGameType;
vmCvar_t ui_allowVoteKick;
vmCvar_t ui_allowVoteClientKick;
vmCvar_t ui_allowVoteTempBanUser;
vmCvar_t ui_allowVoteTempBanClient;
vmCvar_t ui_allowVoteDrawFriend;
vmCvar_t ui_allowVoteKillCam;
vmCvar_t ui_allowVoteFriendlyFire;
vmCvar_t scr_drawfriend;
vmCvar_t scr_friendlyfire;
vmCvar_t scr_killcam;
vmCvar_t ui_drawfriend;
vmCvar_t ui_friendlyfire;
vmCvar_t ui_killcam;
vmCvar_t g_complaintlimit;
vmCvar_t g_voiceChatsAllowed;
vmCvar_t g_deadChat;
vmCvar_t g_developer;
vmCvar_t g_ScoresBanner_Allies;
vmCvar_t g_ScoresBanner_Axis;
vmCvar_t g_ScoresBanner_None;
vmCvar_t g_ScoresBanner_Spectators;
vmCvar_t g_TeamName_Allies;
vmCvar_t g_TeamName_Axis;
vmCvar_t g_TeamColor_Allies;
vmCvar_t g_TeamColor_Axis;
vmCvar_t g_smoothClients;
vmCvar_t pmove_fixed;
vmCvar_t pmove_msec;
vmCvar_t g_scriptMainMenu;
vmCvar_t bg_viewheight_standing;
vmCvar_t bg_viewheight_crouched;
vmCvar_t bg_viewheight_prone;
vmCvar_t bg_ladder_yawcap;
vmCvar_t bg_prone_yawcap;
vmCvar_t bg_lmg_yawcap;
vmCvar_t bg_nofatigue;
vmCvar_t bg_foliagesnd_minspeed;
vmCvar_t bg_foliagesnd_maxspeed;
vmCvar_t bg_foliagesnd_slowinterval;
vmCvar_t bg_foliagesnd_fastinterval;
vmCvar_t bg_foliagesnd_resetinterval;
vmCvar_t bg_fallDamageMinHeight;
vmCvar_t bg_fallDamageMaxHeight;
vmCvar_t bg_debugWeaponAnim;
vmCvar_t bg_debugWeaponMessages;
vmCvar_t bg_debugWeaponState;
vmCvar_t bg_bobAmplitudeStanding;
vmCvar_t bg_bobAmplitudeDucked;
vmCvar_t bg_bobAmplitudeProne;
vmCvar_t bg_bobMax;
vmCvar_t g_bounds_width;
vmCvar_t g_bounds_height_standing;
vmCvar_t g_NoScriptSpam;
vmCvar_t g_debugShowHit;
vmCvar_t g_debugLocDamage;
vmCvar_t bg_debugAnim;
vmCvar_t bg_swingSpeed;
vmCvar_t g_useGear;
vmCvar_t g_languagewarnings;
vmCvar_t g_languagewarningsaserrors;
vmCvar_t g_dumpAnims;
vmCvar_t g_autoscreenshot;
vmCvar_t g_autodemo;
vmCvar_t g_timeoutsAllowed;
vmCvar_t g_timeoutBank;
vmCvar_t g_timeoutLength;
vmCvar_t g_timeoutRecovery;
vmCvar_t ui_timeoutsAllowed;
vmCvar_t ui_timeoutBank;
vmCvar_t ui_timeoutLength;
vmCvar_t ui_timeoutRecovery;

/* Exported by the retail module as zero-initialized storage. No maintained
 * game path currently consumes it; the symbol and initializer are retained
 * while W7 resolves the surrounding hidden-entity state. */
int g_numHidden;

game_cvar_table_t gameCvarTable[] = {
    { &g_cheats, "sv_cheats", "", 0, 0, 0 },
    { NULL, "gamename", "CoD:United Offensive", 68, 0, 0 },
    { NULL, "gamedate", "Feb 10 2005", 64, 0, 0 },
    { NULL, "sv_mapname", "", 68, 0, 0 },
    { &g_night, "sv_night", "0", 72, 0, 0 },
    { &g_obituary, "sv_obituary", "1", 8, 0, 0 },
    { &g_gametype, "g_gametype", "dm", 36, 0, 0 },
    { &g_maxclients, "sv_maxclients", "20", 37, 0, 0 },
    { &g_synchronousClients, "g_synchronousClients", "0", 8, 0, 0 },
    { &g_intermissionDelay, "g_intermissionDelay", "1000", 0, 0, 0 },
    { &g_log, "g_log", "games_mp.log", 1, 0, 0 },
    { &g_logSync, "g_logSync", "0", 1, 0, 0 },
    { &g_password, "g_password", "", 0, 0, 0 },
    { &g_banIPs, "g_banIPs", "", 1, 0, 0 },
    { &g_dedicated, "dedicated", "0", 0, 0, 0 },
    { &g_speed, "g_speed", "190", 0, 0, 1 },
    { &g_gravity, "g_gravity", "800", 0, 0, 1 },
    { &g_knockback, "g_knockback", "1000", 0, 0, 1 },
    { &g_weaponRespawn, "g_weaponrespawn", "5", 0, 0, 1 },
    { &g_weaponAmmoPools, "g_weaponAmmoPools", "0", 0, 0, 1 },
    { &g_maxDroppedWeapons, "g_maxDroppedWeapons", "16", 0, 0, 0 },
    { &g_inactivity, "g_inactivity", "0", 0, 0, 1 },
    { &g_inactivityspectator, "g_inactivityspectator", "0", 0, 0, 1 },
    { &g_debugMove, "g_debugMove", "0", 0, 0, 0 },
    { &g_debugProneCheck, "g_debugProneCheck", "0", 0, 0, 0 },
    { &g_debugProneCheckDepthCheck, "g_debugProneCheckDepthCheck", "1", 0, 0, 0 },
    { &g_debugDamage, "g_debugDamage", "0", 512, 0, 0 },
    { &g_debugAlloc, "g_debugAlloc", "0", 0, 0, 0 },
    { &g_debugBullets, "g_debugBullets", "0", 512, 0, 0 },
    { &g_motd, "g_motd", "", 0, 0, 0 },
    { &g_tracerChance, "g_tracerchance", "0.4", 1, 0, 0 },
    { &g_tracerChanceLMG, "g_tracerchancelmg", "0.7", 512, 0, 0 },
    { &g_vehicleDrawPath, "g_vehicleDrawPath", "0", 512, 0, 0 },
    { &g_vehicleDebug, "g_vehicleDebug", "0", 512, 0, 0 },
    { &g_vehicleTexScrollScale, "g_vehicleTexScrollScale", "0", 512, 0, 0 },
    { &g_vehicleForceBulletDamage, "g_vehicleForceBulletDamage", "0", 0, 0, 0 },
    { &g_vehicleForceGrenadeDamage, "g_vehicleForceGrenadeDamage", "0", 0, 0, 0 },
    { &g_vehicleEnableCollisionDamage, "g_vehicleEnableCollisionDamage", "0", 0, 0, 0 },
    { &g_vehicleTrafficStressTest, "g_vehicleTrafficStressTest", "0", 512, 0, 0 },
    { &g_vehicleHorns, "g_vehicleHorns", "1", 1, 0, 0 },
    { &g_vehicleBurnTime, "g_vehicleBurnTime", "10", 32, 0, 0 },
    { &g_allowVote, "g_allowVote", "1", 0, 0, 0 },
    { &g_allowVoteMapRestart, "g_allowVoteMapRestart", "1", 0, 0, 0 },
    { &g_allowVoteMapRotate, "g_allowVoteMapRotate", "1", 0, 0, 0 },
    { &g_allowVoteTypeMap, "g_allowVoteTypeMap", "1", 0, 0, 0 },
    { &g_allowVoteMap, "g_allowVoteMap", "1", 0, 0, 0 },
    { &g_allowVoteGameType, "g_allowVoteGameType", "1", 0, 0, 0 },
    { &g_allowVoteKick, "g_allowVoteKick", "0", 0, 0, 0 },
    { &g_allowVoteClientKick, "g_allowVoteClientKick", "0", 0, 0, 0 },
    { &g_allowVoteTempBanUser, "g_allowVoteTempBanUser", "0", 0, 0, 0 },
    { &g_allowVoteTempBanClient, "g_allowVoteTempBanClient", "0", 0, 0, 0 },
    { &g_allowVoteDrawFriend, "g_allowVoteDrawFriend", "0", 33, 0, 0 },
    { &g_allowVoteKillCam, "g_allowVoteKillCam", "0", 33, 0, 0 },
    { &g_allowVoteFriendlyFire, "g_allowVoteFriendlyFire", "0", 33, 0, 0 },
    { &g_listEntity, "g_listEntity", "0", 0, 0, 0 },
    { &ui_allowVote, "ui_allowVote", "1", 8, 0, 0 },
    { &ui_allowVoteMapRestart, "ui_allowVoteMapRestart", "1", 8, 0, 0 },
    { &ui_allowVoteMapRotate, "ui_allowVoteMapRotate", "1", 8, 0, 0 },
    { &ui_allowVoteTypeMap, "ui_allowVoteTypeMap", "1", 8, 0, 0 },
    { &ui_allowVoteMap, "ui_allowVoteMap", "1", 8, 0, 0 },
    { &ui_allowVoteGameType, "ui_allowVoteGameType", "1", 8, 0, 0 },
    { &ui_allowVoteKick, "ui_allowVoteKick", "0", 8, 0, 0 },
    { &ui_allowVoteClientKick, "ui_allowVoteClientKick", "0", 8, 0, 0 },
    { &ui_allowVoteTempBanUser, "ui_allowVoteTempBanUser", "0", 8, 0, 0 },
    { &ui_allowVoteTempBanClient, "ui_allowVoteTempBanClient", "0", 8, 0, 0 },
    { &ui_allowVoteDrawFriend, "ui_allowVoteDrawFriend", "0", 8, 0, 0 },
    { &ui_allowVoteKillCam, "ui_allowVoteKillCam", "0", 8, 0, 0 },
    { &ui_allowVoteFriendlyFire, "ui_allowVoteFriendlyFire", "0", 8, 0, 0 },
    { &scr_drawfriend, "scr_drawfriend", "0", 65, 0, 0 },
    { &scr_friendlyfire, "scr_friendlyfire", "0", 65, 0, 0 },
    { &scr_killcam, "scr_killcam", "1", 65, 0, 0 },
    { &ui_drawfriend, "ui_drawfriend", "0", 8, 0, 0 },
    { &ui_friendlyfire, "ui_friendlyfire", "0", 8, 0, 0 },
    { &ui_killcam, "ui_killcam", "1", 8, 0, 0 },
    { &g_complaintlimit, "g_complaintlimit", "3", 1, 0, 1 },
    { &g_voiceChatsAllowed, "g_voiceChatsAllowed", "4", 1, 0, 0 },
    { &g_deadChat, "g_deadChat", "0", 1, 0, 0 },
    { &g_developer, "developer", "0", 256, 0, 0 },
    { &g_ScoresBanner_Allies, "g_ScoresBanner_Allies", "gfx/hud/hud@mpflag_american.tga", 2048, 0, 0 },
    { &g_ScoresBanner_Axis, "g_ScoresBanner_Axis", "gfx/hud/hud@mpflag_german.tga", 2048, 0, 0 },
    { &g_ScoresBanner_None, "g_ScoresBanner_None", "gfx/hud/hud@mpflag_none.tga", 2048, 0, 0 },
    { &g_ScoresBanner_Spectators, "g_ScoresBanner_Spectators", "gfx/hud/hud@mpflag_spectator.tga", 2048, 0, 0 },
    { &g_TeamName_Allies, "g_TeamName_Allies", "GAME_ALLIES", 2048, 0, 0 },
    { &g_TeamName_Axis, "g_TeamName_Axis", "GAME_AXIS", 2048, 0, 0 },
    { &g_TeamColor_Allies, "g_TeamColor_Allies", "0.5 0.5 1", 2048, 0, 0 },
    { &g_TeamColor_Axis, "g_TeamColor_Axis", "1 0.5 0.5", 2048, 0, 0 },
    { &g_smoothClients, "g_smoothClients", "1", 0, 0, 0 },
    { &pmove_fixed, "pmove_fixed", "0", 8, 0, 0 },
    { &pmove_msec, "pmove_msec", "8", 8, 0, 0 },
    { &g_scriptMainMenu, "g_scriptMainMenu", "", 0, 0, 0 },
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    { &g_scriptMainMenu, "g_scriptMap", "", 0, 0, 0 },
    { &bg_viewheight_standing, "bg_viewheight_standing", "60", 2048, 0, 0 },
    { &bg_viewheight_crouched, "bg_viewheight_crouched", "40", 2048, 0, 0 },
    { &bg_viewheight_prone, "bg_viewheight_prone", "11", 2048, 0, 0 },
    { &bg_ladder_yawcap, "bg_ladder_yawcap", "100", 2048, 0, 0 },
    { &bg_prone_yawcap, "bg_prone_yawcap", "85", 2048, 0, 0 },
    { &bg_lmg_yawcap, "bg_lmg_yawcap", "55", 2048, 0, 0 },
    { &bg_nofatigue, "bg_nofatigue", "0", 2560, 0, 0 },
    { &bg_foliagesnd_minspeed, "bg_foliagesnd_minspeed", "40", 2048, 0, 0 },
    { &bg_foliagesnd_maxspeed, "bg_foliagesnd_maxspeed", "180", 2048, 0, 0 },
    { &bg_foliagesnd_slowinterval, "bg_foliagesnd_slowinterval", "1500", 2048, 0, 0 },
    { &bg_foliagesnd_fastinterval, "bg_foliagesnd_fastinterval", "500", 2048, 0, 0 },
    { &bg_foliagesnd_resetinterval, "bg_foliagesnd_resetinterval", "500", 2048, 0, 0 },
    { &bg_fallDamageMinHeight, "bg_fallDamageMinHeight", "256", 520, 0, 0 },
    { &bg_fallDamageMaxHeight, "bg_fallDamageMaxHeight", "480", 520, 0, 0 },
    { &bg_debugWeaponAnim, "bg_debugWeaponAnim", "0", 512, 0, 0 },
    { &bg_debugWeaponState, "bg_debugWeaponState", "0", 512, 0, 0 },
    { &bg_bobAmplitudeStanding, "cg_bobAmplitudeStanding", "0.007", 2048, 0, 0 },
    { &bg_bobAmplitudeDucked, "cg_bobAmplitudeDucked", "0.0075", 2048, 0, 0 },
    { &bg_bobAmplitudeProne, "cg_bobAmplitudeProne", "0.03", 2048, 0, 0 },
    { &bg_bobMax, "cg_bobMax", "8", 2048, 0, 0 },
    { &g_bounds_width, "g_bounds_width", "30", 512, 0, 0 },
    { &g_bounds_height_standing, "g_bounds_height_standing", "70", 512, 0, 0 },
    { &g_NoScriptSpam, "g_no_script_spam", "0", 0, 0, 0 },
    { &g_debugShowHit, "g_debugShowHit", "0", 512, 0, 0 },
    { &g_debugLocDamage, "g_debugLocDamage", "0", 512, 0, 0 },
    { &bg_debugAnim, "g_debuganim", "0", 512, 0, 0 },
    { &bg_swingSpeed, "bg_swingSpeed", "0.2", 512, 0, 0 },
    { &g_useGear, "g_useGear", "1", 33, 0, 0 },
    { &g_languagewarnings, "cl_languagewarnings", "0", 0, 0, 0 },
    { &g_languagewarningsaserrors, "cl_languagewarningsaserrors", "0", 0, 0, 0 },
    { &g_dumpAnims, "g_dumpAnims", "-1", 512, 0, 0 },
    { &g_autoscreenshot, "g_autoscreenshot", "0", 1, 0, 0 },
    { &g_autodemo, "g_autodemo", "0", 1, 0, 0 },
    { &g_timeoutsAllowed, "g_timeoutsallowed", "0", 37, 0, 0 },
    { &g_timeoutBank, "g_timeoutBank", "180000", 33, 0, 0 },
    { &g_timeoutLength, "g_timeoutlength", "90000", 33, 0, 0 },
    { &g_timeoutRecovery, "g_timeoutRecovery", "10000", 33, 0, 0 },
    { &ui_timeoutsAllowed, "ui_timeoutsAllowed", "180000", 8, 0, 0 },
    { &ui_timeoutBank, "ui_timeoutBank", "180000", 8, 0, 0 },
    { &ui_timeoutLength, "ui_timeoutLength", "90000", 8, 0, 0 },
    { &ui_timeoutRecovery, "ui_timeoutRecovery", "10000", 8, 0, 0 }
};

int gameCvarTableCount = (int)(sizeof(gameCvarTable) / sizeof(gameCvarTable[0]));
