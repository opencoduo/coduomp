#ifndef BG_WEAPON_POSITION_H
#define BG_WEAPON_POSITION_H

#include "qcommon/player_state_types.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/q_vector_types.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Shared state passed through the weapon-angle calculation cluster.  The
 * original Windows cgame and game functions are instruction-identical and
 * dereference the first word directly as playerState_t.  Game historically
 * spelled that word gclient_t * only because playerState_t is gclient_t's
 * first field; the pointee used by the common code is playerState_t.
 */
typedef struct pm_weapon_angle_state_s {
    playerState_t *ps;                      /* i386 +0x00 */
    float speed;                            /* +0x04 */
    float frameTime;                        /* +0x08 */
    vec3_t moveOffset;                      /* +0x0c */
    float idleScale;                        /* +0x18 */
    int32_t time;                           /* +0x1c */
    int32_t viewKickStartTime;              /* +0x20 */
    float viewKickPitch;                    /* +0x24 */
    float viewKickYaw;                      /* +0x28 */
    float recoilPitch;                      /* +0x2c */
    float recoilYaw;                        /* +0x30 */
    float recoilRoll;                       /* +0x34 */
    float recoilPitchVelocity;              /* +0x38 */
    float recoilYawVelocity;                /* +0x3c */
    int32_t weaponRecoilState;              /* +0x40 */
    vec2_t baseAngles;                      /* +0x44 */
} pm_weapon_angle_state_t;

/* View-angle companion record used by the same source subsystem. */
typedef struct bg_view_angle_state_s {
    playerState_t *ps;                      /* i386 +0x00 */
    int32_t viewKickStartTime;              /* +0x04 */
    int32_t time;                           /* +0x08 */
    float viewKickPitch;                    /* +0x0c */
    float viewKickRoll;                     /* +0x10 */
    float speed;                            /* +0x14 */
} bg_view_angle_state_t;

#if UINTPTR_MAX == UINT32_MAX
typedef char pm_weapon_angle_state_size[
    sizeof(pm_weapon_angle_state_t) == 0x4c ? 1 : -1];
typedef char pm_weapon_angle_state_recoil_state_offset[
    offsetof(pm_weapon_angle_state_t, weaponRecoilState) == 0x40 ? 1 : -1];
typedef char pm_weapon_angle_state_base_angles_offset[
    offsetof(pm_weapon_angle_state_t, baseAngles) == 0x44 ? 1 : -1];
typedef char bg_view_angle_state_size[
    sizeof(bg_view_angle_state_t) == 0x18 ? 1 : -1];
#endif

void BG_CalculateWeaponPosition_BasePosition_angles(
    pm_weapon_angle_state_t *state, vec3_t angles);
void BG_CalculateWeaponPosition_BaseAngles(pm_weapon_angle_state_t *state,
                                           vec3_t angles);
void BG_CalculateWeaponPosition_IdleAngles(pm_weapon_angle_state_t *state,
                                           vec3_t angles);
void BG_CalculateWeaponPosition_BobOffset(pm_weapon_angle_state_t *state,
                                          vec3_t angles);
void BG_CalculateWeaponPosition_DamageKick(pm_weapon_angle_state_t *state,
                                           vec3_t angles);
qboolean BG_CalculateWeaponPosition_GunRecoil_SingleAngle(
    float *angle, float *velocity, float frameTime, float maxAngle,
    float returnAcceleration, float maxVelocity, float damping,
    float friction);
void BG_CalculateWeaponPosition_GunRecoil(pm_weapon_angle_state_t *state,
                                          vec3_t angles);
void BG_CalculateWeaponAngles(pm_weapon_angle_state_t *state,
                              vec3_t weaponAngles);

void BG_CalculateView_DamageKick(bg_view_angle_state_t *state,
                                 vec3_t angles);
void BG_CalculateView_Velocity(bg_view_angle_state_t *state,
                               vec3_t angles);
void BG_CalculateViewAngles(bg_view_angle_state_t *state,
                            vec3_t viewAngles);

#if defined(WINDOWS_BEHAVIOR)
long double BG_SmoothWeaponSwayValue(float target, float current, float rate,
                                     int32_t msec);
#else
float BG_SmoothWeaponSwayValue(float target, float current, float rate,
                               int32_t msec);
#endif
void BG_CalculateWeaponPosition_Sway(const playerState_t *ps,
                                     vec3_t previousViewAngles,
                                     vec3_t swayOffsets,
                                     vec2_t swayAngles, float scale,
                                     int32_t msec);

#endif
