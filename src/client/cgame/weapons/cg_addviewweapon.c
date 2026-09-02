// Source: uo_cgame_mp_x86.dll 0x30046570..0x30046a37
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30046570_30046a37.mcode
//
// CG_AddViewWeapon — build the local first-person weapon placement, advance its
// XModel animation, and hand it to CG_AddPlayerWeapon.  The mechanical
// BG_PlayerStateToEntityState assignment is a size-only false match.

#include <string.h>

#include "../client_recovered.h"
#include "../globals.h"

enum {
    CG_VIEW_WEAPON_RENDERFX = 12,
    CG_VIEW_WEAPON_PULLBACK_CLASS = 3,
    CG_DRAW_GUN_HIDDEN = 0,
    CG_DRAW_GUN_ALWAYS = 2
};

#define CG_VIEW_WEAPON_PULLBACK_FLAG ((uint32_t)0x00000020u)
#define CG_MILLISECONDS_TO_SECONDS 0.001f
#define CG_VIEW_WEAPON_PULLBACK_DISTANCE (-19.0f)

void CG_AddViewWeapon(void)
{
    playerState_t *ps = &cg_predictedPlayerState;

    if (ps->pmType == PM_TYPE_SPECTATOR || ps->pmType == PM_TYPE_INTERMISSION) {
        return;
    }
    if (ps->pmType >= PM_TYPE_DEAD) {
        CG_ResetWeaponAnimTrees(ps);
        return;
    }
    if (cg_thirdPerson != 0 || cg_viewWeaponSuppressed != 0) {
        return;
    }

    qboolean drawViewWeapon = qtrue;
    if (cg_drawGun_vmCvar.integer != CG_DRAW_GUN_ALWAYS) {
        float overlayFrac;
        if (cg_drawGun_vmCvar.integer == CG_DRAW_GUN_HIDDEN || CG_CalcAdsOverlayFrac(&overlayFrac)) {
            drawViewWeapon = qfalse;
        }
    }

    if ((ps->entityStateFlags & EF_RESTRICTED_MASK) != 0 && !BG_AllowPlayerWeaponAtVehiclePos(ps->vehicleType, ps->vehiclePosition)) {
        if (ps->currentWeapon > 0) {
            cgWeaponInfo_t *wi = &cg_weaponInfos[ps->currentWeapon];
            CG_WeaponRunXModelAnims((playerState_t *)ps, wi);
        }
        return;
    }

    if (ps->currentWeapon <= 0) {
        cg_effectProjAnglePitch = cg_refdefViewAngles[0];
        cg_effectProjAngleYaw = cg_refdefViewAngles[1];
        memset(cg_adsViewOffset, 0, sizeof(cg_adsViewOffset));
        return;
    }

    CG_RegisterWeapon(ps->currentWeapon);
    cgWeaponInfo_t *wi = &cg_weaponInfos[ps->currentWeapon];
    CG_WeaponSway_ApplyShellShock();
    CG_TrackAdsZoomDirection();

    vec3_t positionOffset;
    CG_CalcViewLeanKickOffset(positionOffset);

    axis_t viewAxis;
    axis_t offsetAxis;
    axis_t weaponAxis;
    vec3_t angleOffset;
    pm_weapon_angle_state_t state;
    memset(&state, 0, sizeof(state));

    AnglesToAxisNegRight(viewAxis, cg_refdefViewAngles);

    state.ps = ps;
    state.speed = cg_weaponMoveSpeed;
    /* cg_frametime enters via a bare FILD fed straight into FMUL 0.001f (0x300466c2
     * FILD; 0x300466d3 FMUL; 0x300466ed FSTP) with no FSTP DWORD between, so drop the
     * (float) cast (Class 4). */
    state.frameTime = (float)((long double)cg_frametime * (long double)CG_MILLISECONDS_TO_SECONDS);
    state.moveOffset[0] = cg_weaponPositionPrevAngles[0];
    state.moveOffset[1] = cg_weaponPositionPrevAngles[1];
    state.moveOffset[2] = cg_weaponPositionPrevAngles[2];
    state.idleScale = cg_weaponPositionMoveScale;
    state.time = coduo_int32_from_bits((uint32_t)cg_time - (uint32_t)ps->deltaTime);
    state.viewKickStartTime =
        cg_damageDirLatestServerTime != 0 ? coduo_int32_from_bits((uint32_t)cg_damageDirLatestServerTime - (uint32_t)ps->deltaTime) : 0;
    state.viewKickPitch = cg_damageFlashScale;
    state.viewKickYaw = cg_damageFlashX;
    state.recoilPitch = cg_weaponPositionBaseAngles[0];
    state.recoilYaw = cg_weaponPositionBaseAngles[1];
    state.recoilRoll = cg_weaponPositionBaseAngles[2];
    state.recoilPitchVelocity = cg_weaponRecoilAngles[0];
    state.recoilYawVelocity = cg_weaponRecoilAngles[1];
    /* The cgame caller copies the third recoil float through the otherwise
     * unused +0x40 word.  Preserve that original dword without converting it
     * through the game module's integer spelling for the same slot. */
    memcpy(&state.weaponRecoilState, &cg_weaponRecoilAngles[2], sizeof(state.weaponRecoilState));
    state.baseAngles[0] = cg_weaponSwayAngles[0];
    state.baseAngles[1] = cg_weaponSwayAngles[1];

    BG_CalculateWeaponAngles(&state, angleOffset);
    AnglesToAxisNegRight(offsetAxis, angleOffset);
    MatrixMultiply(offsetAxis, viewAxis, weaponAxis);

    const weaponInfo_t *weapon = bg_weaponInfos[ps->currentWeapon];
    if (weapon->adsEnabled != 0 && ps->adsFraction != 0.0f) {
        vec3_t projectedAngles;
        AxisToAngles(weaponAxis, projectedAngles);
        cg_effectProjAnglePitch = AngleNormalize360(projectedAngles[0]);
        cg_effectProjAngleYaw = AngleNormalize360(projectedAngles[1]);
    } else {
        cg_effectProjAnglePitch = cg_refdefViewAngles[0];
        cg_effectProjAngleYaw = cg_refdefViewAngles[1];
    }

    /* 0x30046843..0x300468a1 copies the state mutated in the local calc block
     * back to its persistent globals. These are output stores, not restoration
     * of the pre-call inputs: AddIdleSway advances idleScale,
     * BasePosition_angles advances moveOffset, and AddWeaponIdle advances the
     * value/rate spring pairs. */
    cg_weaponPositionMoveScale = state.idleScale;
    memcpy(cg_weaponPositionPrevAngles, state.moveOffset, sizeof(cg_weaponPositionPrevAngles));
    cg_weaponPositionBaseAngles[0] = state.recoilPitch;
    cg_weaponPositionBaseAngles[1] = state.recoilYaw;
    cg_weaponPositionBaseAngles[2] = state.recoilRoll;
    cg_weaponRecoilAngles[0] = state.recoilPitchVelocity;
    cg_weaponRecoilAngles[1] = state.recoilYawVelocity;
    memcpy(&cg_weaponRecoilAngles[2], &state.weaponRecoilState, sizeof(cg_weaponRecoilAngles[2]));

    CG_WeaponRunXModelAnims((playerState_t *)ps, wi);

    refEntity_t hand;
    memset(&hand, 0, sizeof(hand));
    hand.renderfx = CG_VIEW_WEAPON_RENDERFX;

    float viewOriginOffset = 0.0f;
    if ((ps->playerStateFlags & CG_VIEW_WEAPON_PULLBACK_FLAG) != 0 && weapon->weaponClass == CG_VIEW_WEAPON_PULLBACK_CLASS) {
        viewOriginOffset = CG_VIEW_WEAPON_PULLBACK_DISTANCE;
        CG_PerturbCamera(viewOriginOffset);
    }

    hand.axis[0][0] = weaponAxis[0][0];
    hand.axis[0][1] = weaponAxis[0][1];
    hand.axis[0][2] = weaponAxis[0][2];
    hand.axis[1][0] = weaponAxis[1][0];
    hand.axis[1][1] = weaponAxis[1][1];
    hand.axis[1][2] = weaponAxis[1][2];
    hand.axis[2][0] = weaponAxis[2][0];
    hand.axis[2][1] = weaponAxis[2][1];
    hand.axis[2][2] = weaponAxis[2][2];

    {
        long double base0 = (long double)cg_gunX_vmCvar.value * (long double)cg_refdef.viewaxis[0][0] + (long double)positionOffset[0];
        long double base1 = (long double)cg_gunX_vmCvar.value * (long double)cg_refdef.viewaxis[0][1] + (long double)positionOffset[1];
        float base2 = (float)((long double)cg_gunX_vmCvar.value * (long double)cg_refdef.viewaxis[0][2] + (long double)positionOffset[2]);
        float xy0 = (float)(base0 + (long double)cg_gunY_vmCvar.value * (long double)cg_refdef.viewaxis[1][0]);
        float xy1 = (float)(base1 + (long double)cg_gunY_vmCvar.value * (long double)cg_refdef.viewaxis[1][1]);
        long double yzWide = (long double)base2 + (long double)cg_gunY_vmCvar.value * (long double)cg_refdef.viewaxis[1][2];

        /* Components 0/1 are FSTP'd after the gun-Y term. Component 2 first
         * spills the gun-X/base sum to base2, then reloads it; that reloaded
         * value stays live across the gun-Y and gun-Z additions until its
         * final origin store. */
        hand.origin[0] = (float)((long double)xy0 + (long double)cg_gunZ_vmCvar.value * (long double)cg_refdef.viewaxis[2][0]);
        hand.origin[1] = (float)((long double)xy1 + (long double)cg_gunZ_vmCvar.value * (long double)cg_refdef.viewaxis[2][1]);
        hand.origin[2] = (float)(yzWide + (long double)cg_gunZ_vmCvar.value * (long double)cg_refdef.viewaxis[2][2]);
    }

    CG_AddPlayerWeapon(&hand, ps, &cg_predictedEventEntity, drawViewWeapon, viewOriginOffset);
}
