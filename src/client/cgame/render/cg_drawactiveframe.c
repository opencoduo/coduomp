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

void CG_DrawActiveFrame(int32_t serverTime, int32_t stereoView, qboolean demoPlayback, int32_t lockedViewFace, int32_t lockedViewSize,
                        qboolean drawFrame)
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
        cg_physicsTime = coduo_int32_from_bits(cg_time);
    }

    cg_demoPlayback = demoPlayback;
    cg_time = (uint32_t)serverTime;
    cg_effectTime = (uint32_t)serverTime;
    cg_lockedViewFace = lockedViewFace;
    cg_lockedViewSize = lockedViewSize;

    if (stereoView != STEREO_RIGHT) {
        cg_frametime = coduo_int32_from_bits((uint32_t)serverTime - (uint32_t)cg_physicsTime);
        if (cg_frametime < 0) {
            cg_frametime = 0;
            cg_physicsTime = serverTime;
        }

        cg_lagometerFrameSamples[cg_lagometerFrameCount & (LAG_SAMPLES - 1)] =
            coduo_int32_from_bits((uint32_t)serverTime - (uint32_t)cg_latestSnapshotServerTime);
        cg_lagometerFrameCount = coduo_int32_from_bits((uint32_t)cg_lagometerFrameCount + 1u);
    }
    cg_effectFrameTime = (uint32_t)cg_frametime;

    /* 0x30042209..0x30042225: all 184 table entries, handle field only. */
    for (i = 0; i < CG_CVAR_TABLE_COUNT; ++i) {
        cgame_syscall(CG_CVAR_UPDATE, (intptr_t)cg_cvarTable[i].vmCvar);
    }

    /* Asset-registration status text owns the frame until it is cleared. */
    if (cg_loadingScratch[0] != '\0') {
        if (drawFrame != qfalse && cg_snap == NULL) {
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
    if (cg_snap == NULL) {
        /* 0x300427ec..0x300429d7 repeats the same no-snapshot draw body. */
        CG_DrawActive(stereoView);
        return;
    }
    if ((cg_snap->snapFlags & SNAPFLAG_NOT_ACTIVE) != 0) {
        return;
    }

    /* If live input has run ahead of the installed snapshot but is not in the
     * future relative to cg.time, force the connection-information draw. */
    if (cl_serverloadmap.string[0] != '\0' && cl_serverloadgametype.string[0] != '\0') {
        usercmd_t oldestCmd;
        int32_t oldestCmdNumber = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_GET_CURRENT_CMD_NUMBER) - (uint32_t)(CMD_BACKUP - 1));

        cgame_syscall(CG_GET_USER_CMD, oldestCmdNumber, (intptr_t)&oldestCmd);
        if (oldestCmd.commandTime > cg_snap->ps.commandTime && oldestCmd.commandTime <= serverTime) {
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

    cg_clientFrame = coduo_int32_from_bits((uint32_t)cg_clientFrame + 1u);
    CG_PredictPlayerState();

    /* motionState is a union at embedded ps+0x62c. Zero selects the manual
     * console shellshock; positive indices select the registered array with the
     * console block occupying conceptual slot zero. */
    if (cg_snap->ps.motionState.shellshock.index == 0) {
        shellshockParams = &cg_consoleShellShock;
        shellshockStartTime = cg_shellShockStartTime;
        shellshockDuration = cg_shellShockDuration;
    } else {
        shellshockParams = &cg_shellShocks[cg_snap->ps.motionState.shellshock.index - 1];
        shellshockStartTime = cg_snap->ps.motionState.shellshock.time;
        shellshockDuration = cg_snap->ps.motionState.shellshock.duration;
    }
    cg_shellShockSwayParams = shellshockParams;
    cg_shellShockSwayStartTime = shellshockStartTime;
    cg_shellShockSwayDuration = shellshockDuration;
    CG_UpdateShellShock(shellshockStartTime, shellshockParams, shellshockDuration);

    cg_thirdPerson = (cg_thirdPerson_vmCvar.integer != qfalse || cg_nextSnap->ps.pmType >= PM_TYPE_DEAD) ? qtrue : qfalse;

    if ((cg_nextSnap->ps.playerStateFlags & CG_FRAME_VIEW_KICK_FLAG_0x80000) != 0) {
        CG_UpdateViewKick();
    } else {
        cg_viewKickVel[0] = 0.0f;
        cg_viewKickVel[1] = 0.0f;
        cg_viewKickVel[2] = 0.0f;
        cg_viewKickAngles[0] = 0.0f;
        cg_viewKickAngles[1] = 0.0f;
        cg_viewKickAngles[2] = 0.0f;
    }

    cameraClipped = CG_CalcViewValues();

    if (cg_lockedViewFace == 0) {
        CG_CalcViewShake();
        AnglesToAxisNegRight(cg_refdef.viewaxis, cg_refdefViewAngles);
        CG_UpdateShellShockCamera();
    }

    CG_DrawSkyBoxPortal();
    cgame_syscall(CG_FX_ADJUST_TIME, coduo_int32_from_bits(cg_time));
    cgame_syscall(CG_FX_ADJUST_CAMERA, (intptr_t)&cg_refdef);

    CG_UpdatePeriodicEffect();
    CG_AddPacketEntities();
    CG_AddMarks();
    CG_AddLocalEntities();
    if (cameraClipped == qfalse) {
        CG_AddViewWeapon();
    }
    CG_FireFlameChunks();

    cgame_syscall(CG_FX_ADD_SCHEDULED_EFFECTS);
    cg_refdef.time = coduo_int32_from_bits(cg_time);

    if (cg_weaponSelect_vmCvar.integer < 0 || cg_weaponSelect_vmCvar.integer > bg_numWeapons) {
        int32_t slot;

        Com_PrintMessage(cg_invalidWeaponSelectWarning, cg_weaponSelect_vmCvar.integer, bg_numWeapons);

        /* 0x3004266f..0x300426b5: replay every nonzero inventory-slot byte
         * through cg_weaponSelect, then leave the cvar at the literal "0". */
        for (slot = 1; slot < WEAPSLOT_COUNT; ++slot) {
            if (cg_predictedPlayerState.weaponSlots[slot] != 0) {
                /* 0x3004267e: MOVSX EDX,AL — the slot byte is SIGN-extended
                 * into va("%i", ...), despite the unsigned storage. */
                trap_Cvar_Set(cg_weaponSelectCvarName, va("%i", (int32_t)(int8_t)cg_predictedPlayerState.weaponSlots[slot]));
            }
        }
        trap_Cvar_Set(cg_weaponSelectCvarName, "0");
    }

    shellshockBlend = cg_zoomSensitivity;
    /* FUCOMPP/TEST AH,0x44/JNP skips only ordered equality with zero;
     * unordered values fall through and are multiplied just like C !=. */
    if (cg_shellshockMouseSensitivityScale != 0.0f) {
        shellshockBlend = (float)((long double)shellshockBlend * (long double)cg_shellshockMouseSensitivityScale);
    }

    viewAngleOffset[0] = (float)((long double)cg_adsViewErrorAngles[0] + (long double)cg_viewKickAngles[0]);
    viewAngleOffset[1] = (float)((long double)cg_adsViewErrorAngles[1] + (long double)cg_viewKickAngles[1]);
    viewAngleOffset[2] = (float)((long double)cg_adsViewErrorAngles[2] + (long double)cg_viewKickAngles[2]);

    cgame_syscall(CG_SET_USER_CMD_AIM_VALUES, (intptr_t)viewAngleOffset);
    cgame_syscall(CG_SET_USER_CMD_VALUE, cg_weaponSelect_vmCvar.integer, CG_FloatBits(shellshockBlend));
    cgame_syscall(CG_SET_CLIENT_LERP_ORIGIN, CG_FloatBits(cg_refdef.vieworg[0]), CG_FloatBits(cg_refdef.vieworg[1]),
                  CG_FloatBits(cg_refdef.vieworg[2]));

    CG_DrawActive(stereoView);

    cgame_syscall(CG_MSS_SET_LISTENER, cg_snap->ps.psClientNum, (intptr_t)cg_refdef.vieworg, (intptr_t)cg_refdef.viewaxis[0]);
    cgame_syscall(CG_MSS_UPDATE_LOOPING_SOUNDS);

    if (cg_stats_vmCvar.integer != qfalse) {
        Com_PrintMessage(cg_clientFrameDiagnostic, cg_clientFrame);
    }
}
