// Source: uo_cgame_mp_x86.dll 0x30042160..0x30042a01
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30042160_30042a01.mcode
//
// CG_DrawActiveFrame — command-4 cgame frame driver. It installs the frame clocks
// and render-mode arguments, refreshes every registered cvar, pumps snapshots,
// predicts the local player, builds/submits the scene, and finally draws the active
// stereo view. The mcode header's CG_PredictPlayerState_Internal name is rejected:
// that routine is independently proven at 0x30035800, while this function's sole
// caller is vmMain(CGVM_DRAW_ACTIVE_FRAME) and its behavior is the canonical
// CG_DrawActiveFrame pipeline.

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

enum {
    STEREO_RIGHT = 2,
    CMD_BACKUP = 128,
    CG_FRAME_VIEW_KICK_FLAG_0x80000 = 0x80000
};

void CG_DrawActiveFrame(int32_t serverTime, int32_t stereoView,
                        qboolean demoPlayback, int32_t lockedViewFace,
                        int32_t lockedViewSize, qboolean drawFrame)
{
    int32_t i;
    qboolean cameraClipped;
    vec3_t viewAngleOffset;
    float shellshockBlend;
    shellshock_t *shellshockParams;
    int32_t shellshockStartTime;
    int32_t shellshockDuration;

    /* A stereo-right render reuses the prediction/physics delta established by
     * the first eye. Every other frame advances the physics clock and lag ring. */
    if (stereoView != STEREO_RIGHT) {
        cg.oldTime = cg.time;
    }

    cg.demoPlayback = demoPlayback;
    cg.time = serverTime;
    cg_effectTime = serverTime;
    cg.lockedViewFace = lockedViewFace;
    cg.lockedViewSize = lockedViewSize;

    if (stereoView != STEREO_RIGHT) {
        cg.frametime = coduo_int32_from_bits(
            (uint32_t)serverTime - (uint32_t)cg.oldTime);
        if (cg.frametime < 0) {
            cg.frametime = 0;
            cg.oldTime = serverTime;
        }

        lagometer.frameSamples[lagometer.frameCount & (LAG_SAMPLES - 1)] =
            coduo_int32_from_bits((uint32_t)serverTime -
                             (uint32_t)cg.latestSnapshotTime);
        lagometer.frameCount = coduo_int32_from_bits(
            (uint32_t)lagometer.frameCount + 1u);
    }
    cg_effectFrameTime = cg.frametime;

    /* 0x30042209..0x30042225: all 184 table entries, handle field only. */
    for (i = 0; i < CG_CVAR_TABLE_COUNT; ++i) {
        cgame_syscall(CG_CVAR_UPDATE,
                      (intptr_t)cg_cvarTable[i].vmCvar);
    }
    /* NOT_FROM_ORIGINAL_SOURCE: refresh the compatibility cvars beside the
     * recovered table so live presentation-mode changes take effect. */
    cgame_compat_update_presentation_cvars();

    /* Asset-registration status text owns the frame until it is cleared. */
    if (cg.infoScreenText[0] != '\0') {
        if (drawFrame != qfalse && cg.snap == NULL) {
            /* 0x30042256..0x3004241f is the same guarded levelshot,
             * hunk-progress, UpdateScreen sequence implemented by the
             * no-snapshot half of CG_DrawActive. Reuse that recovered body. */
            CG_DrawActive(stereoView);
        }
        return;
    }

    cgame_syscall(CG_R_CLEAR_SCENE);
    CG_ProcessSnapshots();

    if (drawFrame == qfalse) {
        return;
    }
    if (cg.snap == NULL) {
        /* 0x300427ec..0x300429d7 repeats the same no-snapshot draw body. */
        CG_DrawActive(stereoView);
        return;
    }
    if ((cg.snap->snapFlags & SNAPFLAG_NOT_ACTIVE) != 0) {
        return;
    }

    /* If live input has run ahead of the installed snapshot but is not in the
     * future relative to cg.time, force the connection-information draw. */
    if (cl_serverloadmap.string[0] != '\0' &&
        cl_serverloadgametype.string[0] != '\0') {
        usercmd_t oldestCmd;
        int32_t oldestCmdNumber = coduo_int32_from_bits(
            (uint32_t)trap_GetCurrentCmdNumber() -
            (uint32_t)(CMD_BACKUP - 1));

        trap_GetUserCmd(oldestCmdNumber, &oldestCmd);
        if (oldestCmd.commandTime > cg.snap->ps.commandTime &&
            oldestCmd.commandTime <= serverTime) {
            CG_DrawInformation(qtrue);
            return;
        }
    }

    if (cl_serverloadwaiting.integer != 0) {
        trap_Cvar_Set(cl_serverLoadWaitingCvarName, "0");
    }
    if (cg_norender_vmCvar.integer != qfalse) {
        return;
    }

    cg.clientFrame = coduo_int32_from_bits((uint32_t)cg.clientFrame + 1u);
    CG_PredictPlayerState();

    /* motionState is a union at embedded ps+0x62c. Zero selects the manual
     * console shellshock; positive indices select the registered array with the
     * console block occupying conceptual slot zero. */
    if (cg.snap->ps.motionState.shellshock.index == 0) {
        shellshockParams = &cgs.shellshocks[0];
        shellshockStartTime = cg.shellShockStartTime;
        shellshockDuration = cg.shellShockDuration;
    } else {
        shellshockParams = &cgs.shellshocks[cg.snap->ps.motionState.shellshock.index];
        shellshockStartTime = cg.snap->ps.motionState.shellshock.time;
        shellshockDuration = cg.snap->ps.motionState.shellshock.duration;
    }
    cg.shellShockSwayParams = shellshockParams;
    cg.shellShockSwayStartTime = shellshockStartTime;
    cg.shellShockSwayDuration = shellshockDuration;
    CG_UpdateShellShock(shellshockStartTime, shellshockParams, shellshockDuration);

    cg.renderingThirdPerson =
        (cg_thirdPerson_vmCvar.integer != qfalse ||
         cg.nextSnap->ps.pmType >= PM_TYPE_DEAD) ? qtrue : qfalse;

    if ((cg.nextSnap->ps.playerStateFlags &
         CG_FRAME_VIEW_KICK_FLAG_0x80000) != 0) {
        CG_UpdateViewKick();
    } else {
        cg.viewKickVel[0] = 0.0f;
        cg.viewKickVel[1] = 0.0f;
        cg.viewKickVel[2] = 0.0f;
        cg.viewKickAngles[0] = 0.0f;
        cg.viewKickAngles[1] = 0.0f;
        cg.viewKickAngles[2] = 0.0f;
    }

    cameraClipped = CG_CalcViewValues();

    if (cg.lockedViewFace == 0) {
        CG_CalcViewShake();
        AnglesToAxisNegRight(cg.refdef.viewaxis, cg.refdefViewAngles);
        CG_UpdateShellShockCamera();
    }

    CG_DrawSkyBoxPortal();
    trap_FX_AdjustTime(cg.time);
    trap_FX_AdjustCamera(&cg.refdef);

    CG_UpdatePeriodicEffect();
    CG_AddPacketEntities();
    CG_AddMarks();
    CG_AddLocalEntities();
    if (cameraClipped == qfalse) {
        CG_AddViewWeapon();
    }
    CG_AddFlameChunks();

    trap_FX_AddScheduledEffects();
    cg.refdef.time = cg.time;

    if (cg_weaponSelect_vmCvar.integer < 0 || cg_weaponSelect_vmCvar.integer > bg_numWeapons) {
        int32_t slot;

        Com_PrintMessage(cg_invalidWeaponSelectWarning,
                         cg_weaponSelect_vmCvar.integer, bg_numWeapons);

        /* 0x3004266f..0x300426b5: replay every nonzero inventory-slot byte
         * through cg_weaponSelect, then leave the cvar at the literal "0". */
        for (slot = 1; slot < WEAPSLOT_COUNT; ++slot) {
            if (cg.predictedPlayerState.weaponSlots[slot] != 0) {
                /* 0x3004267e: MOVSX EDX,AL — the slot byte is SIGN-extended
                 * into va("%i", ...), despite the unsigned storage. */
                trap_Cvar_Set(
                    cg_weaponSelectCvarName,
                    va("%i",
                       (int32_t)(int8_t)cg.predictedPlayerState.weaponSlots[slot]));
            }
        }
        trap_Cvar_Set(cg_weaponSelectCvarName, "0");
    }

    shellshockBlend = cg.zoomSensitivity;
    /* FUCOMPP/TEST AH,0x44/JNP skips only ordered equality with zero;
     * unordered values fall through and are multiplied just like C !=. */
    if (cg.shellshockMouseSensitivityScale != 0.0f) {
        shellshockBlend = (float)(
            (long double)shellshockBlend *
            (long double)cg.shellshockMouseSensitivityScale);
    }

    viewAngleOffset[0] = (float)((long double)cg.adsViewErrorAngles[0] +
                                 (long double)cg.viewKickAngles[0]);
    viewAngleOffset[1] = (float)((long double)cg.adsViewErrorAngles[1] +
                                 (long double)cg.viewKickAngles[1]);
    viewAngleOffset[2] = (float)((long double)cg.adsViewErrorAngles[2] +
                                 (long double)cg.viewKickAngles[2]);

    trap_SetUserCmdAimValues(viewAngleOffset);
    trap_SetUserCmdValue(cg_weaponSelect_vmCvar.integer, shellshockBlend);
    trap_SetClientLerpOrigin(cg.refdef.vieworg[0], cg.refdef.vieworg[1], cg.refdef.vieworg[2]);

    CG_DrawActive(stereoView);

    trap_MSS_SetListener(cg.snap->ps.psClientNum, cg.refdef.vieworg, cg.refdef.viewaxis);
    trap_MSS_UpdateLoopingSounds();

    if (cg_stats_vmCvar.integer != qfalse) {
        Com_PrintMessage(cg_clientFrameDiagnostic, cg.clientFrame);
    }
}
