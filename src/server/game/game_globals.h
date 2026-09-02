#ifndef GAME_GLOBALS_H
#define GAME_GLOBALS_H

#include <stdint.h>
#include "recovered_game.h"
#include "bg_state.h"
#include "level_locals.h"

/*
 * Global variable declarations.
 * These are defined in the binary and accessed throughout the codebase.
 */

/* Level state */
extern level_locals_t level;
extern bgs_t bgs;
extern gentity_t g_entities[MAX_GENTITIES];
extern gclient_t g_clients[MAX_CLIENTS];

/* Background timing state */
extern bg_t bg;

/* Player bounds */
extern vec3_t playerMins;
extern vec3_t playerMaxs;

typedef struct game_cvar_table_s {
    vmCvar_t *vmCvar;
    const char *cvarName;
    const char *defaultString;
    int32_t cvarFlags;
    int32_t modificationCount;
    qboolean trackChange;
} game_cvar_table_t;

#if UINTPTR_MAX == UINT32_MAX
GAME_STATIC_ASSERT(game_cvar_table_size, sizeof(game_cvar_table_t) == 0x18);
GAME_STATIC_ASSERT(game_cvar_table_flags_offset, offsetof(game_cvar_table_t, cvarFlags) == 0x0c);
GAME_STATIC_ASSERT(game_cvar_table_modification_count_offset, offsetof(game_cvar_table_t, modificationCount) == 0x10);
GAME_STATIC_ASSERT(game_cvar_table_track_change_offset, offsetof(game_cvar_table_t, trackChange) == 0x14);
#endif

/* Game cvars */
extern vmCvar_t g_cheats;
extern vmCvar_t g_night;
extern vmCvar_t g_obituary;
extern vmCvar_t g_gametype;
extern vmCvar_t g_maxclients;
extern vmCvar_t g_synchronousClients;
extern vmCvar_t g_intermissionDelay;
extern vmCvar_t g_log;
extern vmCvar_t g_logSync;
extern vmCvar_t g_password;
extern vmCvar_t g_banIPs;
extern vmCvar_t g_filterBan;
extern vmCvar_t g_dedicated;
extern vmCvar_t g_speed;
extern vmCvar_t g_gravity;
extern vmCvar_t g_knockback;
extern vmCvar_t g_weaponRespawn;
extern vmCvar_t g_weaponAmmoPools;
extern vmCvar_t g_maxDroppedWeapons;
extern vmCvar_t g_inactivity;
extern vmCvar_t g_inactivityspectator;
extern vmCvar_t g_debugMove;
extern vmCvar_t g_debugProneCheck;
extern vmCvar_t g_debugProneCheckDepthCheck;
/* NOT_FROM_ORIGINAL_SOURCE: gated spectator/archive diagnostics. */
extern vmCvar_t g_debugArchiveCheck;
/* NOT_FROM_ORIGINAL_SOURCE: gated corpse clone snapshot diagnostics. */
extern vmCvar_t g_debugCorpseCheck;
/* NOT_FROM_ORIGINAL_SOURCE: gated water movement diagnostics. */
extern vmCvar_t g_debugWaterCheck;
extern vmCvar_t g_debugDamage;
extern vmCvar_t g_debugAlloc;
extern vmCvar_t g_debugBullets;
extern vmCvar_t g_motd;
extern vmCvar_t g_tracerChance;
extern vmCvar_t g_tracerChanceLMG;
extern vmCvar_t g_vehicleDrawPath;
extern vmCvar_t g_vehicleDebug;
extern vmCvar_t g_vehicleTexScrollScale;
extern vmCvar_t g_vehicleForceBulletDamage;
extern vmCvar_t g_vehicleForceGrenadeDamage;
extern vmCvar_t g_vehicleEnableCollisionDamage;
extern vmCvar_t g_vehicleTrafficStressTest;
extern vmCvar_t g_vehicleHorns;
extern vmCvar_t g_vehicleBurnTime;
extern vmCvar_t g_allowVote;
extern vmCvar_t g_allowVoteMapRestart;
extern vmCvar_t g_allowVoteMapRotate;
extern vmCvar_t g_allowVoteTypeMap;
extern vmCvar_t g_allowVoteMap;
extern vmCvar_t g_allowVoteGameType;
extern vmCvar_t g_allowVoteKick;
extern vmCvar_t g_allowVoteClientKick;
extern vmCvar_t g_allowVoteTempBanUser;
extern vmCvar_t g_allowVoteTempBanClient;
extern vmCvar_t g_allowVoteDrawFriend;
extern vmCvar_t g_allowVoteKillCam;
extern vmCvar_t g_allowVoteFriendlyFire;
extern vmCvar_t g_listEntity;
extern vmCvar_t ui_allowVote;
extern vmCvar_t ui_allowVoteMapRestart;
extern vmCvar_t ui_allowVoteMapRotate;
extern vmCvar_t ui_allowVoteTypeMap;
extern vmCvar_t ui_allowVoteMap;
extern vmCvar_t ui_allowVoteGameType;
extern vmCvar_t ui_allowVoteKick;
extern vmCvar_t ui_allowVoteClientKick;
extern vmCvar_t ui_allowVoteTempBanUser;
extern vmCvar_t ui_allowVoteTempBanClient;
extern vmCvar_t ui_allowVoteDrawFriend;
extern vmCvar_t ui_allowVoteKillCam;
extern vmCvar_t ui_allowVoteFriendlyFire;
extern vmCvar_t scr_drawfriend;
extern vmCvar_t scr_friendlyfire;
extern vmCvar_t scr_killcam;
extern vmCvar_t ui_drawfriend;
extern vmCvar_t ui_friendlyfire;
extern vmCvar_t ui_killcam;
extern vmCvar_t g_complaintlimit;
extern vmCvar_t g_voiceChatsAllowed;
extern vmCvar_t g_deadChat;
extern vmCvar_t g_developer;
extern vmCvar_t g_ScoresBanner_Allies;
extern vmCvar_t g_ScoresBanner_Axis;
extern vmCvar_t g_ScoresBanner_None;
extern vmCvar_t g_ScoresBanner_Spectators;
extern vmCvar_t g_TeamName_Allies;
extern vmCvar_t g_TeamName_Axis;
extern vmCvar_t g_TeamColor_Allies;
extern vmCvar_t g_TeamColor_Axis;
extern vmCvar_t g_smoothClients;
extern vmCvar_t pmove_fixed;
extern vmCvar_t pmove_msec;
extern vmCvar_t g_scriptMainMenu;
extern vmCvar_t bg_viewheight_standing;
extern vmCvar_t bg_viewheight_crouched;
extern vmCvar_t bg_viewheight_prone;
extern vmCvar_t bg_ladder_yawcap;
extern vmCvar_t bg_prone_yawcap;
extern vmCvar_t bg_lmg_yawcap;
extern vmCvar_t bg_debugWeaponAnim;
extern vmCvar_t bg_debugWeaponMessages;
extern vmCvar_t bg_debugWeaponState;
extern vmCvar_t bg_bobAmplitudeStanding;
extern vmCvar_t bg_bobAmplitudeDucked;
extern vmCvar_t bg_bobAmplitudeProne;
extern vmCvar_t bg_bobMax;
extern vmCvar_t g_bounds_width;
extern vmCvar_t g_bounds_height_standing;
extern vmCvar_t g_NoScriptSpam;
extern vmCvar_t g_debugShowHit;
extern vmCvar_t g_debugLocDamage;
extern vmCvar_t bg_debugAnim;
extern vmCvar_t g_useGear;
extern vmCvar_t g_languagewarnings;
extern vmCvar_t g_languagewarningsaserrors;
extern vmCvar_t g_dumpAnims;
extern vmCvar_t g_autoscreenshot;
extern vmCvar_t g_autodemo;
extern vmCvar_t g_timeoutsAllowed;
extern vmCvar_t g_timeoutBank;
extern vmCvar_t g_timeoutLength;
extern vmCvar_t g_timeoutRecovery;
extern vmCvar_t ui_timeoutsAllowed;
extern vmCvar_t ui_timeoutBank;
extern vmCvar_t ui_timeoutLength;
extern vmCvar_t ui_timeoutRecovery;
extern game_cvar_table_t gameCvarTable[];
extern int gameCvarTableCount;
extern const char emptyString[];
extern const char zeroString[];
extern const char g_log_path[];
extern const char unknownCmdFmt[];
extern uint8_t bulletPriorityMap[];
extern uint8_t riflePriorityMap[];
extern const char *modNames[];
extern const char *hintStrings[];
extern const char *eventnames[EV_MAX_EVENTS];
extern const char **pEventNamesList;
extern turret_state_t g_turretStates[MAX_TURRETS];
extern game_hudElem_t g_hudelems[];
extern float g_fHitLocDamageMult[];

/* Animation globals */
extern vmCvar_t bg_swingSpeed;
extern gitem_t bg_itemlist[BG_ITEMLIST_SLOT_COUNT];

/* Event system */

/* Animation data pointers */
extern bg_static_animation_table_t *bgAnimStaticTable; /* DAT_000b9008 */

/* Fields inside the `level` global use the typed level_locals_t object. */

/* Script constants. Original symbol `scr_const` is 320 bytes. */
#define SCR_CONST_COUNT 160u
extern uint16_t scr_const[SCR_CONST_COUNT];
#define scr_const_allies (scr_const[2])  /* DAT_00449e64 */
#define scr_const_axis (scr_const[4])  /* DAT_00449e68 */
#define scr_const_current (scr_const[10])  /* DAT_00449e74 */
#define scr_const_crouch (scr_const[11])  /* DAT_00449e76 */
#define scr_const_damage (scr_const[13])  /* DAT_00449e7a */
#define scr_const_death (scr_const[14])  /* DAT_00449e7c */
#define scr_const_empty (scr_const[18])  /* DAT_00449e84 */
#define scr_const_entity (scr_const[22])  /* DAT_00449e8c */
#define scr_const_flamebarrel (scr_const[24])  /* DAT_00449e90 */
#define scr_const_fraction (scr_const[25])  /* DAT_00449e92 */
#define scr_const_func_door (scr_const[26])  /* DAT_00449e94 */
#define scr_const_func_door_rotating (scr_const[27])  /* DAT_00449e96 */
#define scr_const_func_rotating (scr_const[28])  /* DAT_00449e98 */
#define scr_const_func_tramcar (scr_const[29])  /* DAT_00449e9a */
#define scr_const_grenade_projectile (scr_const[31])  /* DAT_00449e9e */
#define scr_const_invisible (scr_const[33])  /* DAT_00449ea2 */
#define scr_const_misc_flak (scr_const[38])  /* DAT_00449eac */
#define scr_const_misc_mg42 (scr_const[39])  /* DAT_00449eae */
#define scr_const_misc_turret (scr_const[40])  /* DAT_00449eb0 */
#define scr_const_movedone (scr_const[43])  /* DAT_00449eb6 */
#define scr_const_noclass (scr_const[44])  /* DAT_00449eb8 */
#define scr_const_normal (scr_const[47])  /* DAT_00449ebe */
#define scr_const_player (scr_const[50])  /* DAT_00449ec4 */
#define scr_const_position (scr_const[51])  /* DAT_00449ec6 */
#define scr_const_prone (scr_const[54])  /* DAT_00449ecc */
#define scr_const_reached_end_node (scr_const[55])  /* DAT_00449ece */
#define scr_const_reached_wait_node (scr_const[56])  /* DAT_00449ed0 */
#define scr_const_reached_wait_node_threshold (scr_const[57])  /* DAT_00449ed2 */
#define scr_const_rocket_projectile (scr_const[59])  /* DAT_00449ed6 */
#define scr_const_rotatedone (scr_const[60])  /* DAT_00449ed8 */
#define scr_const_sound_blend (scr_const[61])  /* DAT_00449eda */
#define scr_const_script_brushmodel (scr_const[62])  /* DAT_00449edc */
#define scr_const_script_model (scr_const[63])  /* DAT_00449ede */
#define scr_const_script_origin (scr_const[64])  /* DAT_00449ee0 */
#define scr_const_script_vehicle (scr_const[65])  /* DAT_00449ee2 */
#define scr_const_script_vehicle_corpse (scr_const[66])  /* DAT_00449ee4 */
#define scr_const_script_vehicle_node (scr_const[67])  /* DAT_00449ee6 */
#define scr_const_front_left (scr_const[68])  /* DAT_00449ee8 */
#define scr_const_front_right (scr_const[69])  /* DAT_00449eea */
#define scr_const_back_left (scr_const[70])  /* DAT_00449eec */
#define scr_const_back_right (scr_const[71])  /* DAT_00449eee */
#define scr_const_middle_left (scr_const[72])  /* DAT_00449ef0 */
#define scr_const_middle_right (scr_const[73])  /* DAT_00449ef2 */
#define scr_const_spectator (scr_const[76])  /* DAT_00449ef8 */
#define scr_const_stand (scr_const[78])  /* DAT_00449efc */
#define scr_const_surfacetype (scr_const[79])  /* DAT_00449efe */
#define scr_const_target_location (scr_const[82])  /* DAT_00449f04 */
#define scr_const_tempEntity (scr_const[84])  /* DAT_00449f08 */
#define scr_const_touch (scr_const[87])  /* DAT_00449f0e */
#define scr_const_trigger (scr_const[88])  /* DAT_00449f10 */
#define scr_const_trigger_use (scr_const[89])  /* DAT_00449f12 */
#define scr_const_trigger_damage (scr_const[90])  /* DAT_00449f14 */
#define scr_const_trigger_mount (scr_const[91])  /* DAT_00449f16 */
#define scr_const_trigger_friendlyfire (scr_const[92])  /* DAT_00449f18 */
#define scr_const_turret_on_target (scr_const[97])  /* DAT_00449f22 */
#define scr_const_overheating (scr_const[99])  /* DAT_00449f26 */
#define scr_const_begin (scr_const[103])  /* DAT_00449f2e */
#define scr_const_intermission (scr_const[113])  /* DAT_00449f42 */
#define scr_const_menuresponse (scr_const[115])  /* DAT_00449f46 */
#define scr_const_playing (scr_const[126])  /* DAT_00449f5c */
#define scr_const_team_WOLF_checkpoint (scr_const[137])  /* DAT_00449f72 */
#define scr_const_trigger_objective_info (scr_const[142])  /* DAT_00449f7c */
#define scr_const_no_bounce_missile (scr_const[144])  /* DAT_00449f80 */
#define scr_const_none (scr_const[146])  /* DAT_00449f84 */
#define scr_const_dead (scr_const[147])  /* DAT_00449f86 */
#define scr_const_auto_change (scr_const[148])  /* DAT_00449f88 */
#define scr_const_manual_change (scr_const[149])  /* DAT_00449f8a */
#define scr_const_freelook (scr_const[150])  /* DAT_00449f8c */
#define scr_const_activated (scr_const[151])  /* DAT_00449f8e */
#define scr_const_deactivated (scr_const[152])  /* DAT_00449f90 */
#define scr_const_vehicle_collision (scr_const[153])  /* DAT_00449f92 */
#define scr_const_vehicle_activated (scr_const[154])  /* DAT_00449f94 */
#define scr_const_vehicle_deactivated (scr_const[155])  /* DAT_00449f96 */
#define scr_const_vsay (scr_const[156])  /* DAT_00449f98 */
#define scr_const_artillery_impact (scr_const[157])  /* DAT_00449f9a */
#define scr_const_squad_alpha (scr_const[158])  /* DAT_00449f9c */
#define scr_const_squad_bravo (scr_const[159])  /* DAT_00449f9e */

#endif /* GAME_GLOBALS_H */
