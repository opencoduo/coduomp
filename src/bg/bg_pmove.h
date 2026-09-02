#ifndef BG_PMOVE_H
#define BG_PMOVE_H

#include "bg_animation.h"
#include "bg_movement.h"
#include "qcommon/entity_state_types.h"
#include "qcommon/pmove_types.h"

#include <stdint.h>

#define PM_AIM_SPREAD_SCALE_MAX 255.0f

extern pmove_t *pm;
extern pml_t pml;
extern int32_t c_pmove;
extern vmCvar_t bg_fallDamageMinHeight;
extern vmCvar_t bg_fallDamageMaxHeight;
extern vmCvar_t bg_nofatigue;
extern vmCvar_t bg_foliagesnd_minspeed;
extern vmCvar_t bg_foliagesnd_maxspeed;
extern vmCvar_t bg_foliagesnd_slowinterval;
extern vmCvar_t bg_foliagesnd_fastinterval;
extern vmCvar_t bg_foliagesnd_resetinterval;
extern vmCvar_t bg_viewheight_standing;
extern const float pm_ladderPushOff;
extern const int32_t pm_ladderJumpTime;
extern const float pm_ladderScale;
extern const float pm_ladderfriction;
extern int32_t PMDebugLastWeaponState;
extern uint32_t PMDebugLastWeaponAnim;
extern const char *PMDebugPrefix;

void Pmove(pmove_t *move);
void PmoveSingle(pmove_t *move);
void PM_UpdateViewAngles(playerState_t *ps, const usercmd_t *command,
                         pm_trace_fn_t traceFunc);
/* Remaining pmove driver dependencies. Their complete bodies remain separately
 * owned until each whole subsystem is adjudicated and extracted. */
void PM_CheckDuck(void);
void PM_Weapon(void);
void PM_Weapon_PrintWeaponState(void);
void PM_Weapon_PrintWeaponAnim(void);
void PM_trace(trace_t *results, const vec3_t start, const vec3_t mins,
              const vec3_t maxs, const vec3_t end, int32_t passEntityNum,
              int32_t traceType);
float PM_CmdScale(const usercmd_t *command);
float PM_CmdScale_Walk(const usercmd_t *command);
void PM_Accelerate(vec3_t wishdir, float wishspeed, float accel);
long double PM_GetSlowdownFriction(void);
long double PM_GetJumpFactor(void);
void PM_Friction(void);
void PM_Jump(float jumpHeight);
qboolean PM_CheckJump(void);
int32_t PM_VerifyPronePosition(const vec3_t origin, const vec3_t velocity);
qboolean PM_CorrectAllSolid(trace_t *trace);
void PM_GroundTraceMissed(void);
void PM_CrashLand(void);
void PM_GroundTrace(void);
void PM_FlyMove(void);
void PM_NoclipMove(void);
void PM_UFOMove(void);
void PM_SetMovementDir(void);
void PM_AirMove(void);
void PM_WalkMove(void);
void PM_DeadMove(void);
void PM_CheckLadderMove(void);
void PM_LadderMove(void);
void PM_AddEvent(int32_t event);
void PM_AddTouchEnt(int32_t entityNum);
void PM_ClipVelocity(const vec3_t input, const vec3_t normal, vec3_t output,
                     float overbounce);

/* Shared 17-argument prone-capsule validator used by the common pmove code.
 * Windows cgame names it at 0x30006e10; the game modules retain the same
 * interface and trace-callback contract. */
int32_t BG_CheckProneValid(int32_t clientNum, const vec3_t origin,
                           float radius, float height, float yaw,
                           float *groundOffset, float *pitchDown,
                           float *pitchUp, qboolean skipInitialTrace,
                           qboolean allowFallback,
                           const vec3_t groundNormal,
                           pm_trace_fn_t traceFunc,
                           pm_trace_fn_t traceDownFunc,
                           qboolean useAltContentMask, float proneLength,
                           qboolean checkForwardClearance,
                           pm_entity_type_fn_t entityTypeFunc);
int32_t BG_CheckProne(int32_t clientNum, const vec3_t origin,
                      float radius, float height, float yaw,
                      float *groundOffset, float *pitchDown,
                      float *pitchUp, qboolean skipInitialTrace,
                      qboolean allowFallback, const vec3_t groundNormal,
                      pm_trace_fn_t traceFunc,
                      pm_trace_fn_t traceDownFunc,
                      qboolean useAltContentMask, float proneLength,
                      pm_entity_type_fn_t entityTypeFunc);
int32_t BG_CheckProneTurned(playerState_t *ps, float yaw,
                            pm_trace_fn_t traceFunc);
void PM_UpdatePronePitch(void);
#if defined(WINDOWS_BEHAVIOR)
int32_t Script_RoundToNearestInt(float value);
#endif
int32_t PM_SlideMove(int32_t gravity);
void PM_StepSlideMove(int32_t gravity);

int32_t PM_GroundSurfaceType(void);
int32_t PM_JumpForSurface(void);
int32_t PM_FootstepForSurface(uint32_t playerStateFlags);
int32_t PM_LightLandingForSurface(void);
int32_t PM_MediumLandingForSurface(void);
int32_t PM_HardLandingForSurface(void);
int32_t PM_DamageLandingForSurface(void);

void PM_FootstepEvent(int32_t oldBobCycle, int32_t newBobCycle,
                      int32_t shouldMake);
int32_t PM_ShouldMakeFootsteps(void);
void PM_Footsteps(void);

void PM_StartWeaponAnim(pmWeaponAnim_t weaponAnim);
void PM_ContinueWeaponAnim(pmWeaponAnim_t weaponAnim);
void PM_Weapon_FinishRechamber(void);
qboolean PM_Weapon_CheckForRechamber(qboolean allowInterrupt);
void PM_WeaponUseAmmo(int32_t weapon, int32_t amount);
int32_t PM_WeaponAmmoAvailable(int32_t weapon);
qboolean PM_WeaponClipEmpty(int32_t weapon);
void PM_ReloadClip(void);
void PM_SetWeaponReloadAddAmmoDelay(void);
void PM_SetReloadingState(void);
void PM_BeginWeaponReload(void);
qboolean PM_Weapon_AllowReload(void);
void PM_Weapon_ReloadDelayedAction(void);
qboolean PM_Weapon_FinishReload(qboolean pendingInterrupt);
void PM_Weapon_CheckForReload(void);
void PM_RemoveEmptyClipOnlyWeapon(void);
void PM_BeginWeaponDeploy(void);
void PM_BeginWeaponBreakingdown(void);
void PM_BeginWeaponChange(int32_t currentWeapon, int32_t nextWeapon);
qboolean PM_Weapon_FinishWeaponChange(void);
qboolean PM_Weapon_FinishWeaponRaise(void);
qboolean PM_Weapon_FinishWeaponDeploy(void);
qboolean PM_Weapon_FinishWeaponBreakdown(void);
void PM_Weapon_CheckForDeployBreakdown(void);
void PM_Weapon_CheckForChangeWeapon(void);
qboolean PM_Weapon_WeaponTimeAdjust(void);
qboolean PM_Weapon_FinishFiring(qboolean delayExpired);
void PM_Weapon_StartFiring(qboolean delayExpired);
int32_t PM_Weapon_GetAmmoRequired(int32_t weapon);
qboolean PM_Weapon_CheckFiringAmmo(void);
void PM_Weapon_SetFPSFireAnim(void);
void PM_Weapon_AddFiringAimSpreadScale(void);
void PM_Weapon_FireWeapon(qboolean delayExpired);
void PM_Weapon_FireMelee(void);
qboolean PM_Weapon_FinishMelee(void);
void PM_Weapon_CheckForMelee(qboolean delayExpired);
qboolean PM_InteruptWeaponWithProneMove(void);
qboolean PM_InteruptWeaponWithSprintMove(void);
void PM_UpdateAimDownSightFlag(void);
void PM_ClearAimDownSightFlag(void);
void PM_UpdateAimDownSightLerp(void);
void PM_AdjustAimSpreadScale(void);
void PM_UpdatePlayerWalkingFlag(void);
void PM_UpdatePlayerSprintingFlag(void);
void PM_PlayFatigueSound(void);
void PM_UpdateFatigue(void);
void PM_UpdateLean(playerState_t *ps, const usercmd_t *command,
                   pm_trace_fn_t traceFunc);
void PM_SetWaterLevel(void);
void PM_WaterEvents(void);
void PM_FoliageSounds(void);
void PM_DropTimers(void);
float PM_FloatAbs(float value);
int32_t PM_FloatIsNegative(float value);
int32_t PM_FloatSign(float value);

/* Shared PM primitive used by the common weapon state machines. */
void PM_SetProneMovementOverride(void);

#define PM_WEAPON_LMG_ADS_FRACTION_MIN 0.99f

/* Original BG/viewheight dependencies used by the shared pmove cluster. */
int32_t BG_ExecuteCommand(playerState_t *ps, int32_t stateIndex,
                          int32_t moveType, qboolean restartSame);
long double PM_GetViewHeightLerp(int32_t fromViewheight,
                                 int32_t toViewheight);
int32_t PM_GetViewHeightLerpTime(const playerState_t *ps,
                                 int32_t fromViewheight,
                                 int32_t viewHeightLerpDown);
long double PM_ViewHeightTableLerp(int32_t percent,
                                   const pmLerpEntry_t *table,
                                   float *outOriginAdjust);
void PM_ViewHeightAdjust(void);
effectiveStance_t PM_GetEffectiveStance(const playerState_t *ps);

extern const pmLerpEntry_t pmViewHeightLerpCrouchedRising[9];
extern const pmLerpEntry_t pmViewHeightLerpStanding[9];
extern const pmLerpEntry_t pmViewHeightLerpProne[11];
extern const pmLerpEntry_t pmViewHeightLerpCrouchedFalling[8];

#endif
