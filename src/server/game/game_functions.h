#ifndef GAME_FUNCTIONS_H
#define GAME_FUNCTIONS_H

#include <stdint.h>
#if defined(_WIN32)
#include <string.h>
#endif
#include "qcommon/com_parse.h"
#include "qcommon/com_sprintf.h"
#include "bg/bg_player_state.h"
#include "qcommon/q_bits.h"
#include "qcommon/q_endian.h"
#include "math/q_math.h"
#include "qcommon/q_path.h"
#include "qcommon/q_shared_misc.h"
#include "qcommon/q_string.h"
#include "recovered_game.h"
#include "scr_vm.h"

/*
 * Game function prototypes.
 * Non-Scr_, non-trap_ functions used throughout the codebase.
 */

#if !defined(_WIN32)
extern int strcasecmp(const char *a, const char *b);
#endif

/* Math utilities */
extern void G_SetMovedir(float *angles, float *movedir);
/* Configstring/index functions */
extern int G_FindConfigstringIndex(const char *name, int start, int max, qboolean create, const char *fieldname);
extern int G_ModelIndex(const char *modelName);
extern const char *G_ModelName(int modelIndex);
extern int G_ShaderIndex(const char *name);
extern int G_ShellShockIndex(const char *name);
extern int G_EffectIndex(const char *name);
extern uint8_t G_SoundAliasIndex(const char *name);
extern int G_LocalizedStringIndex(const char *value);
extern int G_TagIndex(const char *tagName);
extern int G_IndexForMeansOfDeath(const char *name);
extern const char *SL_ConvertToString(uint16_t stringId);
extern uint16_t SL_GetString(const char *value, uint8_t user);
extern uint16_t SL_GetLowercaseString(const char *value, uint8_t user);
extern uint16_t SL_FindLowercaseString(const char *value);

/* Memory tag wrappers */
extern void *MT_Alloc(size_t size, int tag);
extern void MT_Free(void *ptr, size_t size);

/* Entity management */
extern void G_Error(const char *fmt, ...);
extern void G_DPrintf(const char *format, ...);
extern void Com_Printf(const char *format, ...);
extern void Com_DPrintf(const char *format, ...);
extern void Com_Error(errorParm_t code, const char *format, ...);
extern void G_SetOrigin(gentity_t *ent, const float *origin);
extern void G_SetAngle(gentity_t *ent, const float *angles);
extern void G_SetModel(gentity_t *ent, const char *modelName);
extern void G_SetConstString(uint16_t *slot, const char *value);
extern void G_BackupSpawnVars(gentity_t *ent);
extern int G_SpawnString(const char *key, const char *defaultValue, const char **out);
extern int G_SpawnFloat(const char *key, const char *defaultValue, float *out);
extern int G_SpawnInt(const char *key, const char *defaultValue, int *out);
extern gentity_t *G_Spawn(void);
extern gentity_t *G_SpawnPlayerClone(void);
extern void G_InitGentity(gentity_t *ent);
extern void SP_info_camp(gentity_t *ent);
extern void SP_info_null(gentity_t *ent);
extern void SP_info_notnull(gentity_t *ent);
extern void SP_light(gentity_t *ent);
extern void SP_misc_teleporter_dest(gentity_t *ent);
extern void SP_sound_blend(gentity_t *ent);
extern gentity_t *G_SpawnSoundBlend(void);
extern void G_SetSoundBlend(gentity_t *ent, int soundAlias0, int soundAlias1, float repeatDelay);
extern void G_SetSoundBlendAndPitch(gentity_t *ent, int soundAlias0, int soundAlias1, float volume, float pitch);
extern void SP_misc_model(gentity_t *ent);
extern void use_corona(gentity_t *ent, gentity_t *other, gentity_t *activator);
extern void SP_corona(gentity_t *ent);
extern qboolean G_CalcTurretMuzzlePoints(gentity_t *turret, gentity_t *fireEnt, weapon_muzzle_t *muzzlePoints);
extern void G_FireTurret(gentity_t *turret, gentity_t *fireEnt, int damage);
extern void G_PlayerTurretPositionAndBlend(gentity_t *player, gentity_t *turret);
extern void G_UpdateTurretClientAiming(gentity_t *turret, gentity_t *player);
extern void G_FireTurretFromClient(gentity_t *turret, gentity_t *player);
extern void G_RunClientTurret(gentity_t *turret, gentity_t *player);
extern void G_UpdateTurretSound(gentity_t *turret);
extern void G_ClientStopUsingTurret(gentity_t *turret);
extern void turret_think_client(gentity_t *turret);
extern void G_SpawnTurret(gentity_t *turret, const char *weaponName);
extern void SP_turret(gentity_t *ent);
extern void misc_spawner_think(gentity_t *ent);
extern void misc_spawner_use(gentity_t *ent, gentity_t *other, gentity_t *activator);
extern void SP_misc_spawner(gentity_t *ent);
extern void miscGunnerEnemyScan(gentity_t *ent);

/* Entity functions */
extern void G_AddEvent(gentity_t *ent, int event, int eventParm);
extern void G_AddPredictableEvent(gentity_t *ent, int event, int eventParm);
extern void G_PlayerEvent(int entityNum, int event);
extern int G_EntityType(int entityNum);
extern gentity_t *G_CallSpawn(void);
extern gentity_t *G_TempEntity(const float *origin, int event);
extern void TeleportPlayer(gentity_t *ent, const float *origin, const float *angles);
extern gentity_t *G_PlaySoundAliasAtPoint(const float *origin, uint8_t soundAlias);
extern void G_PlaySoundAlias(gentity_t *ent, uint8_t soundAlias);
extern void G_KillBox(gentity_t *ent);
extern qboolean G_CallSpawnEntity(gentity_t *ent);
extern int G_RadiusDamage(const float *origin, gentity_t *inflictor, gentity_t *attacker, float damage, float radius, float innerRadius,
                          gentity_t *ignore, int meansOfDeath);
extern void G_Damage(gentity_t *target, gentity_t *inflictor, gentity_t *attacker, const float *dir, const float *point, int damage,
                     int flags, int meansOfDeath, int hitLocation);
extern void G_DamageClient(gentity_t *target, gentity_t *inflictor, gentity_t *attacker, const float *dir, const float *point, int damage,
                           int flags, int meansOfDeath, int hitLocation);
extern float CanDamage(gentity_t *target, const float *origin);
extern qboolean G_IsVehicleImmune(gentity_t *vehicle, int meansOfDeath);
extern int G_IsVehicleOccupantInvulnerable(gentity_t *player);
extern float G_VehicleOccupantRadiusDamageScale(gentity_t *player);
extern void G_AddLean(gentity_t *ent, float *origin);
extern int OnSameTeam(gentity_t *ent1, gentity_t *ent2);
extern int G_GetNonPVSFriendlyInfo(gentity_t *ent, const float *origin, int lastClient);
extern int G_GetNonPVSTankInfo(gentity_t *ent, const float *origin, int lastClient);
extern int G_IsInMatchTimeout(void);
extern void G_SetFixedLink(gentity_t *ent, int mode);
extern gentity_t *G_Find(gentity_t *from, size_t fieldOffset, uint16_t match);
extern gentity_t *G_FindStr(gentity_t *from, size_t fieldOffset, const char *match);
extern gentity_t *G_PickTarget(uint16_t targetname);
extern gentity_t *G_TestEntityPosition(gentity_t *ent, const float *origin);
extern void G_Trigger(gentity_t *ent, gentity_t *activator);
extern void InitTrigger(gentity_t *ent);
extern void InitSentientTrigger(gentity_t *ent);
extern void multi_wait(gentity_t *ent);
extern void Touch_Multi(gentity_t *ent, gentity_t *other, int traceMode);
extern void Use_Multi(gentity_t *ent, gentity_t *other, gentity_t *activator);
extern void G_TouchTriggers(gentity_t *ent);
extern void G_DoTouchTriggers(gentity_t *ent, const float *origin);

/* Entity linking */
extern qboolean G_EntLinkTo(gentity_t *child, gentity_t *parent, const char *tagname);
extern qboolean G_EntLinkToWithOffset(gentity_t *child, gentity_t *parent, const char *tagname, const float *offset, const float *angles);
extern qboolean G_EntIsLinkedTo(gentity_t *child, gentity_t *parent);
extern qboolean G_EntAttach(gentity_t *ent, const char *modelName, const char *tagName, qboolean ignoreCollision);
extern qboolean G_EntDetach(gentity_t *ent, const char *modelName, const char *tagName);
extern void G_CalcTagParentRelAxis(gentity_t *child, matrix43_t *outAxis);
extern void G_CalcTagAxis(gentity_t *ent, int useLinkedAngles);
extern void G_UpdateTagInfo(gentity_t *ent, qboolean updateBoneIndex);
extern void G_UpdateTagInfoOfChildren(gentity_t *parent, qboolean updateBoneIndex);
extern void G_CalcTagParentAxis(gentity_t *child, matrix43_t *outAxis);
extern void Think_GeneralLink(gentity_t *ent);
extern qboolean VEH_UnlinkPlayer(gentity_t *player, int keepVehicle);
extern void G_SafeDObjFree(gentity_t *ent);
extern uint16_t G_GetGameId(gentity_t *ent);
extern void G_UpdateTags(gentity_t *ent, qboolean updateBoneIndex);
extern qboolean G_DObjSetLocalBoneIndex(gentity_t *ent, uint32_t *partBits, int boneIndex, const float *origin, const float *angles);
extern qboolean G_DObjSetLocalTag(gentity_t *ent, uint32_t *partBits, const char *tagName, const float *origin, const float *angles);
extern qboolean G_DObjSetControlTagAngles(gentity_t *ent, uint32_t *partBits, const char *tagName, const float *angles);
extern void G_DObjUpdate(gentity_t *ent);
extern void G_DObjCalcBone(gentity_t *ent, int boneIndex);
extern DObjSkelMat *G_DObjGetLocalBoneIndexMatrix(gentity_t *ent, int boneIndex);
extern void G_DObjGetWorldBoneIndexMatrix(gentity_t *ent, int boneIndex, DObjSkelMat *outMatrix);
extern DObjSkelMat *G_DObjGetLocalTagMatrix(gentity_t *ent, const char *tagName);
extern qboolean G_DObjGetWorldTagMatrix(gentity_t *ent, const char *tagName, DObjSkelMat *outMatrix);

/* Weapon/ammo functions */
extern void BG_SetupWeaponInfo(void);
extern int Add_Ammo(gentity_t *ent, int weapon, int amount, qboolean fillClip);
extern gentity_t *Drop_Weapon(gentity_t *ent, int weapon, const char *tagName);

/* Background game functions */
extern void BG_GetMarkDir(const float *dir, const float *normal, float *out);
extern const char *BG_GetVehiclePosTag(int vehiclePos);
extern const char *vtos(const float *value);
extern const char *vtosf(const float *value);

/* Recovered weapon fire helpers */
extern qboolean infront(gentity_t *self, gentity_t *other);
extern int DebugLine(void);
extern void Weapon_Melee(gentity_t *ent, const float *muzzlePoints);
extern void SnapVectorTowards(float *point, const float *towards);
extern float Damage_Falloff(float distance, float maxDamage, float minDamagePercent, int minRange, int maxRange);
extern const char *BG_GetWeaponTypeName(int weaponType);
extern void G_CheckHitTriggerDamage(gentity_t *activator, const float *start, const float *end, int damage, int mod);
extern void G_GrenadeTouchTriggerDamage(gentity_t *grenade, const float *grenadePos, const float *explosionPos, int damage, int mod);
extern void Activate_trigger_damage(gentity_t *trigger, gentity_t *activator, int damage, int mod);
extern gentity_t *fire_grenade(gentity_t *self, float *start, float *dir, int weapon);
extern gentity_t *fire_rocket(gentity_t *self, float *start, float *dir);
extern gentity_t *fire_artillery(gentity_t *self, float *origin, int delay);
extern void fire_artillery_barrage(gentity_t *self, float *origin, int weapon);
extern qboolean Bullet_Fire(gentity_t *ent, float spread, int damage, weapon_muzzle_t *muzzlePoints, gentity_t *attacker);
extern qboolean Bullet_Fire_Extended(gentity_t *hitEnt, gentity_t *attacker, float *start, const float *end, int damage, int recursionDepth,
                                     weapon_muzzle_t *muzzlePoints, gentity_t *source);
extern gentity_t *weapon_grenadelauncher_fire(gentity_t *ent, int weapon, weapon_muzzle_t *muzzlePoints);
extern void Weapon_RocketLauncher_Fire(gentity_t *ent, float spread, weapon_muzzle_t *muzzlePoints);
extern void Weapon_Artillery_Fire(gentity_t *ent, float spread, weapon_muzzle_t *muzzlePoints);
extern void Weapon_ArtilleryStrike_Fire(gentity_t *ent, float spread, weapon_muzzle_t *muzzlePoints);
extern int LogAccuracyHit(gentity_t *target, gentity_t *attacker);
extern void CalcMuzzlePoint(gentity_t *ent, float *muzzlePoint);
extern void CalcMuzzlePoints(gentity_t *ent, weapon_muzzle_t *muzzlePoints);
/* Debug drawing */
extern void G_DebugLine(const float *start, const float *end, const float *color, int depthTest, int duration);
extern void G_DebugBox(const float *mins, const float *maxs, const float *color, int depthTest, int duration);
extern void G_DebugCircle(const float *center, float radius, const float *color, int depthTest, qboolean useUpNormal, int duration);
extern void G_DebugCircleEx(const float *center, float radius, const float *normal, const float *color, int depthTest, int duration);

/* Item functions */
extern void Touch_Item(gentity_t *itemEnt, gentity_t *other, int traceMode);
extern gentity_t *Drop_Item(gentity_t *ent, gitem_t *item, float angleOffset, int stationary);
extern void RegisterItem(int itemIndex, int updateConfigString);
extern qboolean IsItemRegistered(int itemIndex);
/* Animation functions */
extern void BG_UpdatePlayerDObj(gentity_t *ent, const gentity_t *entState, clientInfo_t *clientInfo, uint8_t *dObjVersion);
/* Client functions */
extern qboolean G_ClientCanSpectateTeam(gclient_t *client, int team);
extern int BG_PlayerTouchesItem(gclient_t *client, gentity_t *itemEnt, int time);
extern qboolean Cmd_FollowCycle_f(gentity_t *ent, int direction);
extern qboolean Cmd_Activate_f(gentity_t *ent);
extern void Cmd_Score_f(gentity_t *ent);
extern void ClientEvents(gentity_t *ent, uint32_t oldEventSequence);
extern void ClientImpacts(gentity_t *ent, const pmove_t *trace);
extern qboolean ClientInactivityTimer(gclient_t *client);
extern qboolean ClientSpectatorInactivityTimer(gclient_t *client);
extern void ClientIntermissionThink(gentity_t *ent, const usercmd_t *command);
extern void SpectatorThink(gentity_t *ent, const usercmd_t *command);
extern void ClientSpawn(gentity_t *ent, const float *origin, const float *angles);
extern void player_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath, int weapon,
                       const float *dir, int hitLocation);
extern void LookAtKiller(gentity_t *self, gentity_t *inflictor, gentity_t *attacker);
extern int G_GetHitLocationIndexFromString(uint16_t hitLocationName);

/* Initialization functions */
extern void G_RegisterCvars(void);
extern void G_ProcessIPBans(void);
extern void G_SetPlayerSize(void);
extern void G_ParseScrVehicleInfo(void);
extern void GScr_LoadScripts(void);
extern void GScr_LoadConsts(void);
extern void G_ParseHitLocDmgTable(void);
extern void ClearRegisteredItems(void);
extern void SaveRegisteredItems(void);
extern void G_InitVehiclePaths(void);
extern void G_InitScrVehicles(void);
extern void G_InitTurrets(void);
extern void G_SpawnEntitiesFromString(void);
extern void G_SetupVehiclePaths(void);
extern void G_SetupScrVehicles(void);
extern void G_FindTeams(void);
extern void G_SetUICvars(void);
extern void G_setfog(const char *fog);
extern void G_InitObjectives(void);
extern void G_UpdateCvars(void);
extern void G_RunFrameForEntity(gentity_t *ent);
extern void G_RunThink(gentity_t *ent);
extern void G_GeneralLink(gentity_t *ent);
extern void SP_trigger_mount_no_brush(gentity_t *ent, qboolean largeTrigger);
extern int G_CheckPointInsideTriggerMount(gentity_t *ent, const float *point, int *mountHintData);
extern void G_RunMover(gentity_t *ent);
extern void G_RunMissile(gentity_t *ent);
extern void G_RunClient(gentity_t *ent);
extern void G_RunItem(gentity_t *ent);
extern int G_DObjUpdateServerTime(gentity_t *ent, int bNotify);
extern void G_FreeEntity(gentity_t *ent);
extern void G_EntUnlink(gentity_t *ent);
extern void G_FreeEntityRefs(gentity_t *ent);
extern void G_FreeTurret(gentity_t *ent);
extern void G_FreeVehicle(gentity_t *ent);
extern void G_FreeVehicleRefs(gentity_t *ent);
extern void Scr_FreeEntity(gentity_t *ent);
extern void G_DObjCalcPose(gentity_t *ent);
extern void G_FreeEntities(void);
extern void G_FreeScrVehicles(void);
extern void G_FreeVehiclePaths(void);
extern void G_UpdateObjectiveToClients(void);
extern void G_UpdateHudElemsToClients(void);
extern void G_VehicleClientThink(void);
extern void G_VehInitPathPos(vehicle_path_position_t *pathPosition);
extern int G_VehUpdatePathPos(vehicle_path_position_t *pathPosition, int16_t lastNodeIndex);
extern void VEH_GetMinsMaxs(gentity_t *ent, float *mins, float *maxs);
extern void VEH_InitPhysics(gentity_t *ent);
extern void VEH_PlayerDamage(gentity_t *player, gentity_t *vehicle, int damage);
extern void VEH_PlayerCollision(gentity_t *vehicle, gentity_t *player);
extern qboolean G_PlayerVehiclePositionAndBlend(gentity_t *ent);
extern void G_XAnimUpdateEnt(gentity_t *ent);
extern void G_XAnimUpdate(void);
extern void CheckTeamStatus(void);
extern void G_CheckForCursorHints(gentity_t *ent);
extern void G_CheckForPreventFriendlyFire(gentity_t *ent);
extern void CheckVote(void);
extern void CheckMatchTimeout(void);
extern void SendScoreboardMessageToAllIntermissionClients(void);
extern void CalculateRanks(void);
extern void ExitLevel(void);
extern void DeathmatchScoreboardMessage(gentity_t *ent);
extern void DebugDumpAnims(void);
extern qboolean G_GetHintStringIndex(int *outIndex, const char *value);
extern int Game_RoundFloatPlusHalf(float value);

/* Logging */
extern void G_Printf(const char *format, ...);
extern void G_LogPrintf(const char *format, ...);

/* Animation script */
extern void G_AnimScriptSound(int entityNum, const char *soundAliasName);
extern void game_compat_bg_set_anim_sound_callbacks(const char *(*soundAlias)(const char *name),
                                                    void (*soundEvent)(int entityNum, const char *soundAliasName));

/* Client functions */
extern void ClientEndFrame(gentity_t *ent);
extern void ClientUserinfoChanged(int clientNum);
extern void SetClientOrigin(gentity_t *ent, const float *origin);
extern void SetClientViewAngle(gentity_t *ent, const float *angles);

/* HudElem */
extern void HudElem_DestroyAll(void);
extern void HudElem_UpdateClient(gclient_t *client, int clientNum, uint32_t updateFlags);

#endif /* GAME_FUNCTIONS_H */
