#include "cgame.h"
#include "widescreen_2d_compat.h"

#include "cinematic.h"
#include "console.h"
#include "../animation/xanim_pool.h"
#include "../math/vector_math.h"
#include "../platform/crt_boundary.h"
#include "qcommon/q_string.h"
#include "../renderer/renderer_api.h"
#include "../sound/miles_boundary.h"
#include "../system_platform.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Original Win32 clientActive_t at 0x049581a0..0x04ad3cd3. */
clientActive_t cl;
/* Original Win32 cvar-pointer slots are proved individually by the returned
 * Cvar_Get stores in CL_Init and Com_Init. */
cvar_t *cl_activeAction;
cvar_t *cl_freezeDemo;
cvar_t *cl_showTimeDelta;
cvar_t *cl_timeNudge;
cvar_t *cl_timedemo;
cvar_t *cl_paused;
cvar_t *sv_paused;
cvar_t *com_timescale;

/* Original Win32 input-frame timestamps at 0x04e1cbd8 and 0x04e1cbe0.
 * CL_CreateNewCommands owns both updates; the key accumulators and mouse
 * scaling consume the current command-frame duration. */
static uint32_t cl_previousCommandFrameTime;
uint32_t cl_commandFrameMsec;

/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): the retail packet scheduler
 * clamps cl_maxpackets to 100. Improved builds accept the modern default of
 * 125; the stock source retains the original 15..100 domain. */
#define CL_MAXPACKETS_MAX_VALUE 125
#define CL_MAXPACKETS_MAX_TEXT "125"

/* Source: CoDUOMP.exe 0x0058ebd8..0x0058ec58 (.rdata). Standard inline-text
 * colors ^0 through ^7, stored as four-float RGBA vectors. */
static const vec4_t cl_baseColorCodes[8] = {
    {0.0f, 0.0f, 0.0f, 1.0f},
    {1.0f, 0.0f, 0.0f, 1.0f},
    {0.0f, 1.0f, 0.0f, 1.0f},
    {1.0f, 1.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 1.0f, 1.0f},
    {0.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 0.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f}
};

enum {
    CL_CONNECTIONLESS_SEQUENCE = -1,
    CL_PACKET_SEQUENCE_BYTES = 4,
    CL_TIMEOUT_GRACE_COUNT = 5,
    CL_MILLISECONDS_PER_SECOND = 1000,
    CL_TIME_NUDGE_MIN_MSEC = -30,
    CL_TIME_NUDGE_MAX_MSEC = 30,
    CL_TIMEDEMO_FRAME_MSEC = 50,
    CL_EXTRAPOLATION_MARGIN_MSEC = 5,
    CL_TIME_DELTA_FAST_THRESHOLD_MSEC = 100,
    CL_TIME_DELTA_RESET_THRESHOLD_MSEC = 500,
    CL_TIME_DELTA_EXTRAPOLATION_CORRECTION_MSEC = 2,
    CL_AVIDEMO_MIN_FRAME_MSEC = 1,
    /* Original CL_CmdButtons leaves buttons bit 7 unused. */
    CL_WBUTTON_FLAME_DAMAGE = PM_WBUTTON_0,
    CL_WBUTTON_SHELLSHOCK_SCREEN_BLUR = PM_WBUTTON_WALK
};

#define CL_AVIDEMO_MSEC_PER_SECOND 1000.0f
#define CL_TIMEGRAPH_FRAME_SCALE 0.25f
#define CL_VIEW_PITCH_LIMIT 90.0f /* exact 0x42b40000 */
#define CL_ANGLE_TO_SHORT_SCALE \
    182.04444885253906f /* exact 0x43360b61, 65536 / 360 */
#define CL_MSEC_TO_SECONDS \
    0.0010000000474974513f /* exact 0x3a83126f */

/* Source: CoDUOMP.exe 0x0040b680..0x0040b6fc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040b680_0040b6fd.mcode.
 * Name and accumulator semantics: exact same-module Mac symbol CL_KeyState. */
float CL_KeyState(clKeyButton_t *button)
{
    int32_t elapsedMsec =
        button->accumulatedMsec;
    button->accumulatedMsec = 0;

    if (button->active != qfalse) {
        if (button->downtime == 0) {
            elapsedMsec = com_frameTime;
        } else {
            elapsedMsec = (int32_t)(
                (uint32_t)elapsedMsec +
                ((uint32_t)com_frameTime -
                 (uint32_t)button->downtime));
        }
        button->downtime = com_frameTime;
    }

    const float fraction =
        (float)elapsedMsec /
        (float)cl_commandFrameMsec;
    if (fraction < 0.0f)
        return 0.0f;
    if (fraction > 1.0f)
        return 1.0f;
    return fraction;
}

/* Source: CoDUOMP.exe 0x0040be90..0x0040bf3d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040be90_0040bf3e.mcode.
 * Name and input mapping: exact same-module Mac symbol CL_AdjustAngles. */
void CL_AdjustAngles(void)
{
    float speed =
        (float)cls.frameTime;
    if (in_speed.active != qfalse)
        speed *= cl_anglespeedkey->value;
    speed *= CL_MSEC_TO_SECONDS;

    if (in_strafe.active == qfalse) {
        cl.inputState.viewAngles[1] -=
            speed * cl_yawspeed->value *
            CL_KeyState(&in_right);
        cl.inputState.viewAngles[1] +=
            speed * cl_yawspeed->value *
            CL_KeyState(&in_left);
    }

    cl.inputState.viewAngles[0] -=
        speed * cl_pitchspeed->value *
        CL_KeyState(&in_lookup);
    cl.inputState.viewAngles[0] +=
        speed * cl_pitchspeed->value *
        CL_KeyState(&in_lookdown);
}

/* Source: CoDUOMP.exe 0x0040bf40..0x0040c175.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040bf40_0040c176.mcode.
 * Name and movement/button construction: exact same-module Mac symbol
 * CL_KeyMove. */
void CL_KeyMove(usercmd_t *command)
{
    enum {
        CL_STANCE_STAND = 0,
        CL_STANCE_CROUCH = 1,
        CL_STANCE_PRONE = 2
    };

    uint8_t stanceButtons =
        command->wbuttons;
    if (in_prone.active != qfalse) {
        stanceButtons =
            (uint8_t)((stanceButtons &
                       (uint8_t)~PM_WBUTTON_STANCE_MASK) |
                      PM_WBUTTON_PRONE);
        stanceButtons |= PM_WBUTTON_STANCE_LATCH;
    } else if (in_down.active != qfalse) {
        stanceButtons =
            (uint8_t)((stanceButtons &
                       (uint8_t)~PM_WBUTTON_STANCE_MASK) |
                      PM_WBUTTON_CROUCH);
        stanceButtons |= PM_WBUTTON_STANCE_LATCH;
    } else {
        stanceButtons &=
            (uint8_t)~PM_WBUTTON_STANCE_MASK;
        if (cl_stance->integer == CL_STANCE_CROUCH) {
            stanceButtons |=
                PM_WBUTTON_CROUCH;
        } else if (cl_stance->integer ==
                   CL_STANCE_PRONE) {
            stanceButtons |=
                PM_WBUTTON_PRONE;
        }
        stanceButtons &=
            (uint8_t)~PM_WBUTTON_STANCE_LATCH;
    }
    command->wbuttons = stanceButtons;

    if ((cl_run->integer != 0) ==
        (in_speed.active != qfalse)) {
        command->buttons |=
            PM_BUTTON_ADS;
    } else {
        command->buttons &=
            (uint8_t)~PM_BUTTON_ADS;
    }

    if (in_sprint.active != qfalse) {
        command->buttons |=
            PM_BUTTON_SPRINT;
    } else {
        command->buttons &=
            (uint8_t)~PM_BUTTON_SPRINT;
    }

    int32_t rightMove = 0;
    if (in_strafe.active != qfalse) {
        rightMove =
            (int32_t)(CL_KeyState(&in_right) *
                      127.0f) +
            (int32_t)(CL_KeyState(&in_left) *
                      -127.0f);
    }
    rightMove +=
        (int32_t)(CL_KeyState(&in_moveright) *
                  127.0f);
    rightMove +=
        (int32_t)(CL_KeyState(&in_moveleft) *
                  -127.0f);

    const pmType_t pmType =
        cl.snap.ps.pmType;
    const uint32_t entityFlags =
        cl.snap.ps.entityStateFlags;
    int32_t upMove = 0;
    if (pmType == PM_TYPE_NOCLIP ||
        pmType == PM_TYPE_UFO ||
        ((entityFlags &
          EF_IN_VEHICLE) != 0 &&
         (entityFlags &
          EF_VEHICLE_ALLOW_WEAPON) == 0) ||
        pmType == PM_TYPE_SPECTATOR) {
        upMove =
            (int32_t)(CL_KeyState(&in_stanceUp) *
                      127.0f);
    }

    upMove +=
        (int32_t)(CL_KeyState(&in_up) *
                  127.0f);

    if ((command->wbuttons &
         PM_WBUTTON_CROUCH) != 0) {
        if (in_down.active != qfalse) {
            upMove +=
                (int32_t)(CL_KeyState(&in_down) *
                          -127.0f);
        } else {
            upMove -= 127;
        }
    } else if ((command->wbuttons &
                PM_WBUTTON_PRONE) != 0) {
        upMove -= 127;
    }

    const int32_t forwardMove =
        (int32_t)(CL_KeyState(&in_forward) *
                  127.0f) +
        (int32_t)(CL_KeyState(&in_back) *
                  -127.0f);

    if ((entityFlags & EF_FORCED_STANCE_MASK) != 0) {
        return;
    }

    command->forwardmove =
        ClampChar(forwardMove);
    command->rightmove =
        ClampChar(rightMove);
    command->upmove =
        ClampChar(upMove);
}

/* Source: CoDUOMP.exe 0x0040c700..0x0040c89f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040c700_0040c8a0.mcode.
 * Name and direct button-index mapping: exact same-module Mac symbol
 * CL_CmdButtons. */
void CL_CmdButtons(usercmd_t *command)
{
    if (in_attack.active != qfalse ||
        in_attack.wasPressed != qfalse) {
        command->buttons |= PM_BUTTON_FIRE;
    }
    in_attack.wasPressed = qfalse;

    if (in_commandButton1.active != qfalse ||
        in_commandButton1.wasPressed != qfalse) {
        command->buttons |= PM_BUTTON_UI_CAPTURE;
    }
    in_commandButton1.wasPressed = qfalse;

    if (in_dropWeapon.active != qfalse ||
        in_dropWeapon.wasPressed != qfalse) {
        command->buttons |= PM_BUTTON_DROP_WEAPON;
    }
    in_dropWeapon.wasPressed = qfalse;

    if (in_sprint.active != qfalse ||
        in_sprint.wasPressed != qfalse) {
        command->buttons |= PM_BUTTON_SPRINT;
    }
    in_sprint.wasPressed = qfalse;

    if (in_commandButton4.active != qfalse ||
        in_commandButton4.wasPressed != qfalse) {
        command->buttons |= PM_BUTTON_ADS;
    }
    in_commandButton4.wasPressed = qfalse;

    if (in_melee.active != qfalse ||
        in_melee.wasPressed != qfalse) {
        command->buttons |= PM_BUTTON_MELEE;
    }
    in_melee.wasPressed = qfalse;

    if (in_activate.active != qfalse ||
        in_activate.wasPressed != qfalse) {
        command->buttons |= PM_BUTTON_ACTIVATE;
    }
    in_activate.wasPressed = qfalse;

    if (in_commandButton7.active != qfalse ||
        in_commandButton7.wasPressed != qfalse) {
        command->wbuttons |= PM_WBUTTON_0;
    }
    in_commandButton7.wasPressed = qfalse;

    if (in_commandButton8.active != qfalse ||
        in_commandButton8.wasPressed != qfalse) {
        command->wbuttons |= PM_WBUTTON_STANCE_LATCH;
    }
    in_commandButton8.wasPressed = qfalse;

    if (in_commandButton9.active != qfalse ||
        in_commandButton9.wasPressed != qfalse) {
        command->wbuttons |= PM_WBUTTON_WALK;
    }
    in_commandButton9.wasPressed = qfalse;

    if (in_reload.active != qfalse ||
        in_reload.wasPressed != qfalse) {
        command->wbuttons |= PM_WBUTTON_RELOAD;
    }
    in_reload.wasPressed = qfalse;

    if (in_leanLeft.active != qfalse ||
        in_leanLeft.wasPressed != qfalse) {
        command->wbuttons |= PM_WBUTTON_LEAN_LEFT;
    }
    in_leanLeft.wasPressed = qfalse;

    if (in_leanRight.active != qfalse ||
        in_leanRight.wasPressed != qfalse) {
        command->wbuttons |= PM_WBUTTON_LEAN_RIGHT;
    }
    in_leanRight.wasPressed = qfalse;

    if (in_prone.active != qfalse ||
        in_prone.wasPressed != qfalse) {
        command->wbuttons |= PM_WBUTTON_PRONE;
    }
    in_prone.wasPressed = qfalse;

    if (cls.keyCatchers != 0 &&
        cl_bypassMouseInput->integer == 0) {
        command->buttons |= PM_BUTTON_UI_CAPTURE;
    }
    if (cl.inputState.shellshockScreenBlur != 0) {
        command->wbuttons |= CL_WBUTTON_SHELLSHOCK_SCREEN_BLUR;
    }
    if (cl.inputState.flameDamage != 0) {
        command->wbuttons |= CL_WBUTTON_FLAME_DAMAGE;
    }
}

/* Source: CoDUOMP.exe 0x0040c210..0x0040c316.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040c210_0040c317.mcode.
 * Name and joystick-axis mapping: exact same-module Mac symbol
 * CL_JoystickMove. */
void CL_JoystickMove(usercmd_t *command)
{
    if ((cl_run->integer != 0) ==
        (in_speed.active != qfalse)) {
        command->buttons |= PM_BUTTON_ADS;
    }

    float angleSpeed =
        (float)cls.frameTime;
    if (in_speed.active != qfalse)
        angleSpeed *= cl_anglespeedkey->value;
    angleSpeed *= CL_MSEC_TO_SECONDS;

    if (in_strafe.active == qfalse) {
        cl.inputState.viewAngles[1] +=
            (float)cl.inputState.joystickAxis[0] *
            cl_yawspeed->value * angleSpeed;
    } else {
        command->rightmove =
            ClampChar(
                (int32_t)command->rightmove +
                cl.inputState.joystickAxis[0]);
    }

    if (in_mlooking != qfalse) {
        cl.inputState.viewAngles[0] +=
            (float)cl.inputState.joystickAxis[1] *
            cl_pitchspeed->value * angleSpeed;
    } else {
        command->forwardmove =
            ClampChar(
                (int32_t)command->forwardmove +
                cl.inputState.joystickAxis[1]);
    }

    command->upmove =
        ClampChar(
            (int32_t)command->upmove +
            cl.inputState.joystickAxis[2]);
}

/* Source: CoDUOMP.exe 0x0040c320..0x0040c32d, recovered from an executable
 * gap. Name and return role: exact same-module Mac symbol
 * CL_IsInMatchTimeout. The client frame duration is zero while input-driven
 * view changes are suspended. */
qboolean CL_IsInMatchTimeout(void)
{
    return cls.frameTime == 0;
}

/* Source: CoDUOMP.exe 0x0040c330..0x0040c6fb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040c330_0040c6fc.mcode.
 * Name and argument role: exact same-module Mac symbol CL_MouseMove. */
void CL_MouseMove(usercmd_t *command)
{
    enum {
        CL_PLAYERSTATE_FLAG_FOLLOW = 0x4000,
        CL_ENTITYSTATE_SCOPE_MASK = 0x6000
    };
    const float filteredMouseScale =
        0.5f; /* exact 0x3f000000, average two samples */
    const float scopeMouseXScale =
        2.5f; /* exact 0x40200000 */
    const float scopeMouseYScale =
        2.0f; /* exact x87 FADD ST0,ST0 */

    float mouseX;
    float mouseY;
    if (m_filter->integer != 0) {
        mouseX =
            (float)(cl.inputState.mouseDx[0] +
                    cl.inputState.mouseDx[1]) *
            filteredMouseScale;
        mouseY =
            (float)(cl.inputState.mouseDy[0] +
                    cl.inputState.mouseDy[1]) *
            filteredMouseScale;
    } else {
        mouseX =
            (float)cl.inputState.mouseDx[
                cl.inputState.mouseIndex];
        mouseY =
            (float)cl.inputState.mouseDy[
                cl.inputState.mouseIndex];
    }

    cl.inputState.mouseIndex ^= 1;
    cl.inputState.mouseDx[cl.inputState.mouseIndex] = 0;
    cl.inputState.mouseDy[cl.inputState.mouseIndex] = 0;

    const float mouseRate =
        sqrtf(mouseX * mouseX + mouseY * mouseY) /
        (float)cl_commandFrameMsec;
    const float acceleratedSensitivity =
        mouseRate * cl_mouseAccel->value +
        sensitivity->value;
    const float effectiveSensitivity =
        cl.inputState.userCmdSensitivityScale *
        acceleratedSensitivity;

    if (mouseRate != 0.0f &&
        cl_showmouserate->integer != 0) {
        Com_Printf("%f : %f\n",
                   (double)mouseRate,
                   (double)effectiveSensitivity);
    }

    if ((cl.snap.ps.playerStateFlags &
         CL_PLAYERSTATE_FLAG_FOLLOW) != 0 ||
        CL_IsInMatchTimeout()) {
        return;
    }

    if ((cl.snap.ps.entityStateFlags &
         CL_ENTITYSTATE_SCOPE_MASK) != 0) {
        mouseX *= scopeMouseXScale;
        mouseY *= scopeMouseYScale;
    } else {
        mouseX *= effectiveSensitivity;
        mouseY *= effectiveSensitivity;
    }

    if (mouseX == 0.0f && mouseY == 0.0f)
        return;

    if (in_strafe.active != qfalse) {
        command->rightmove =
            ClampChar(
                (int32_t)command->rightmove +
                (int32_t)(mouseX * m_side->value));
    } else {
        float yawDelta = mouseX * m_yaw->value;
        if (cl.inputState.shellshockMouseMaxYawSpeed !=
            0.0f) {
            const float maximumYawDelta =
                (float)cl_commandFrameMsec *
                cl.inputState.shellshockMouseMaxYawSpeed *
                CL_MSEC_TO_SECONDS;
            yawDelta = Com_ClampFloat(
                -maximumYawDelta, maximumYawDelta,
                yawDelta);
        }
        cl.inputState.viewAngles[1] -= yawDelta;
    }

    if ((in_mlooking != qfalse ||
         cl_freelook->integer != 0) &&
        in_strafe.active == qfalse) {
        float pitchDelta = mouseY * m_pitch->value;
        if (cl.inputState.shellshockMouseMaxPitchSpeed !=
            0.0f) {
            const float maximumPitchDelta =
                (float)cl_commandFrameMsec *
                cl.inputState.shellshockMouseMaxPitchSpeed *
                CL_MSEC_TO_SECONDS;
            pitchDelta = Com_ClampFloat(
                -maximumPitchDelta, maximumPitchDelta,
                pitchDelta);
        }
        cl.inputState.viewAngles[0] += pitchDelta;
    } else {
        command->forwardmove =
            ClampChar(
                (int32_t)command->forwardmove -
                (int32_t)(mouseY * m_forward->value));
    }
}

/* Source: CoDUOMP.exe 0x0040c8a0..0x0040c8fa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040c8a0_0040c8fb.mcode.
 * Name and command fields: exact same-module Mac symbol CL_FinishMove. */
void CL_FinishMove(usercmd_t *command)
{
    enum {
        CL_MAX_COMMAND_TIME_AHEAD_MSEC = 5000
    };

    command->weapon =
        (uint8_t)cl.inputState.userCmdValue;

    int32_t commandTime = cl.serverTime;
    const int32_t timeAhead = (int32_t)(
        (uint32_t)cl.serverTime -
        (uint32_t)cl.snap.serverTime);
    if (timeAhead >
        CL_MAX_COMMAND_TIME_AHEAD_MSEC) {
        commandTime = (int32_t)(
            (uint32_t)cl.snap.serverTime +
            (uint32_t)CL_MAX_COMMAND_TIME_AHEAD_MSEC);
    }
    command->commandTime = commandTime;

    for (int32_t axis = 0; axis < 3; ++axis) {
        command->angles[axis] =
            (int32_t)(
                (cl.inputState.userCmdAimValues[axis] +
                 cl.inputState.viewAngles[axis]) *
                CL_ANGLE_TO_SHORT_SCALE) &
            (int32_t)UINT16_MAX;
    }
}

/* Source: CoDUOMP.exe 0x0040c900..0x0040cac2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040c900_0040cac3.mcode.
 * Name and command-building order: exact same-module Mac symbol
 * CL_CreateCmd. Cgame syscall 247 may replace the engine view angles before
 * this function runs; these are the same angles adjusted by normal client
 * input and encoded into the returned command. */
usercmd_t CL_CreateCmd(void)
{
    const float oldPitch =
        cl.inputState.viewAngles[0];
    const float oldYaw =
        cl.inputState.viewAngles[1];

    CL_AdjustAngles();

    usercmd_t command;
    memset(&command, 0, sizeof(command));
    CL_CmdButtons(&command);
    CL_KeyMove(&command);
    CL_MouseMove(&command);
    CL_JoystickMove(&command);

    if (cl_viewPitchCompensate->value != 0.0f) {
        cl.inputState.viewAngles[0] +=
            cl_viewPitchCompensate->value;
        Cvar_Set("cl_viewPitchCompensate", "0");
    }
    if (cl_viewYawCompensate->value != 0.0f) {
        cl.inputState.viewAngles[1] +=
            cl_viewYawCompensate->value;
        Cvar_Set("cl_viewYawCompensate", "0");
    }

    if (cl.inputState.viewAngles[0] - oldPitch >
        CL_VIEW_PITCH_LIMIT) {
        cl.inputState.viewAngles[0] =
            oldPitch + CL_VIEW_PITCH_LIMIT;
    } else if (oldPitch -
                   cl.inputState.viewAngles[0] >
               CL_VIEW_PITCH_LIMIT) {
        cl.inputState.viewAngles[0] =
            oldPitch - CL_VIEW_PITCH_LIMIT;
    }

    CL_FinishMove(&command);

    if (cl_debugMove->integer == 1) {
        SCR_DebugGraph(
            fabsf(cl.inputState.viewAngles[1] - oldYaw),
            0);
    } else if (cl_debugMove->integer == 2) {
        SCR_DebugGraph(
            fabsf(cl.inputState.viewAngles[0] - oldPitch),
            0);
    }

    return command;
}

/* Source: CoDUOMP.exe 0x004131b0..0x0041338e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004131b0_0041338f.mcode.
 * Name and three-argument signature: exact same-module Mac symbol
 * CL_PacketEvent. */
void CL_PacketEvent(netadr_t from, msg_t *message, int32_t time)
{
    int32_t sequence;

    if (message->cursize >= CL_PACKET_SEQUENCE_BYTES) {
        memcpy(&sequence, message->data, sizeof(sequence));
        if (sequence == CL_CONNECTIONLESS_SEQUENCE) {
            CL_ConnectionlessPacket(from, message, time);
            return;
        }
    }

    if (cls.state < CA_CONNECTED)
        return;

    if (message->cursize < CL_PACKET_SEQUENCE_BYTES) {
        Com_Printf("%s: Runt packet\n", NET_AdrToString(from));
        return;
    }

    if (NET_CompareAdr(from, clc.netchan.remoteAddress) == qfalse) {
        Com_DPrintf("%s:sequenced packet without connection\n",
                    NET_AdrToString(from));
        return;
    }

    clc.lastPacketTime = cls.realTime;
    if (Netchan_Process(&clc.netchan, message) == qfalse)
        return;

    const int32_t headerBytes = message->readcount;
    memcpy(&clc.serverMessageSequence, message->data,
           sizeof(clc.serverMessageSequence));
    clc.reliableAcknowledge = MSG_ReadLong(message);

    const int32_t oldestReliableSequence = (int32_t)(
        (uint32_t)clc.reliableSequence -
        (uint32_t)CODUO_RELIABLE_COMMAND_COUNT);
    if (clc.reliableAcknowledge < oldestReliableSequence) {
        clc.reliableAcknowledge = clc.reliableSequence;
        return;
    }

    CL_Netchan_Decode(
        message->data + message->readcount,
        message->cursize - message->readcount);
    CL_ParseServerMessage(message);

    if (clc.demoRecording != qfalse &&
        clc.demoWaiting == qfalse) {
        CL_WriteDemoMessage(message, headerBytes);
    }
}

/* Source: CoDUOMP.exe 0x00413390..0x00413414.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00413390_00413415.mcode.
 * Name and timeout-count behavior: exact same-module Mac symbol
 * CL_CheckTimeout. The client tolerates five consecutive expired frames
 * before dropping the connection. */
void CL_CheckTimeout(void)
{
    if (cl_paused->integer != 0 &&
        sv_paused->integer != 0) {
        cl.timeoutCount = 0;
        return;
    }

    if (cls.state < CA_CONNECTED ||
        cls.state == CA_CINEMATIC ||
        cls.state == CA_LOGO) {
        cl.timeoutCount = 0;
        return;
    }

    const int32_t elapsed = (int32_t)(
        (uint32_t)cls.realTime -
        (uint32_t)clc.lastPacketTime);
    if ((float)elapsed >
        cl_timeout->value *
            (float)CL_MILLISECONDS_PER_SECOND) {
        cl.timeoutCount = (int32_t)(
            (uint32_t)cl.timeoutCount + 1u);
        if (cl.timeoutCount >
            CL_TIMEOUT_GRACE_COUNT) {
            Com_Error(
                ERR_DROP,
                "EXE_ERR_SERVER_TIMEOUT");
        }
        return;
    }

    cl.timeoutCount = 0;
}

/* Source: CoDUOMP.exe 0x004137c0..0x00413810.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004137c0_00413811.mcode.
 * Name and userinfo command: exact same-module Mac symbol CL_CheckUserinfo. */
void CL_CheckUserinfo(void)
{
    if (cls.state < CA_CHALLENGING ||
        cl_paused->integer != 0 ||
        (cvar_modifiedFlags & CVAR_USERINFO) == 0) {
        return;
    }

    cvar_modifiedFlags &= ~((uint32_t)CVAR_USERINFO);
    CL_AddReliableCommand(
        va("userinfo \"%s\"",
           Cvar_InfoString(CVAR_USERINFO)));
}

/* Source: CoDUOMP.exe 0x00413820..0x0041386b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00413820_0041386c.mcode.
 * Name and cvar values: exact same-module Mac symbol
 * CL_UpdateInGameState. */
void CL_UpdateInGameState(void)
{
    if (cls.state == CA_ACTIVE) {
        if (cl_ingame->integer == 0)
            (void)Cvar_Set2("cl_ingame", "1", qtrue);
        return;
    }

    if (cl_ingame->integer != 0)
        (void)Cvar_Set2("cl_ingame", "0", qtrue);
}

/* Source: CoDUOMP.exe 0x0040cad0..0x0040cb55.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040cad0_0040cb56.mcode.
 * Name and returned-command assignment: exact same-module Mac symbol
 * CL_CreateNewCommands. */
void CL_CreateNewCommands(void)
{
    enum {
        CL_COMMAND_FRAME_MAX_MSEC = 200,
        CL_USERCMD_RING_MASK = CODUO_USERCMD_BACKUP_COUNT - 1
    };

    if (cls.state < CA_PRIMED)
        return;

    cl_commandFrameMsec =
        (uint32_t)com_frameTime -
        cl_previousCommandFrameTime;
    if (cl_commandFrameMsec >
        CL_COMMAND_FRAME_MAX_MSEC) {
        cl_commandFrameMsec =
            CL_COMMAND_FRAME_MAX_MSEC;
    }
    cl_previousCommandFrameTime =
        (uint32_t)com_frameTime;

    cl.cmdNumber = (int32_t)(
        (uint32_t)cl.cmdNumber + 1u);
    cl.cmds[
        (uint32_t)cl.cmdNumber &
        (uint32_t)CL_USERCMD_RING_MASK] =
        CL_CreateCmd();
}

/* Source: CoDUOMP.exe 0x0040cb60..0x0040cc77.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040cb60_0040cc78.mcode.
 * Name and packet-rate gates: exact same-module Mac symbol
 * CL_ReadyToSendPacket. */
qboolean CL_ReadyToSendPacket(void)
{
    enum {
        CL_DISCONNECTED_DOWNLOAD_PACKET_MSEC = 50,
        CL_CONNECTING_PACKET_MSEC = 1000,
        CL_MAXPACKETS_MIN = 15,
        CL_MAXPACKETS_MAX = CL_MAXPACKETS_MAX_VALUE,
        CL_OUT_PACKET_MASK =
            CODUO_SNAPSHOT_BACKUP_COUNT - 1,
        CL_MILLISECONDS_PER_SECOND_LOCAL = 1000
    };

    if (clc.demoPlayback != qfalse ||
        cls.state == CA_CINEMATIC ||
        cls.state == CA_LOGO) {
        return qfalse;
    }

    const int32_t elapsedSincePacket =
        (int32_t)((uint32_t)cls.realTime -
                  (uint32_t)clc.lastPacketSentTime);
    const qboolean downloading =
        cls.staticDownload.downloadTempName[0] != '\0'
            ? qtrue
            : qfalse;

    if (downloading != qfalse &&
        elapsedSincePacket <
            CL_DISCONNECTED_DOWNLOAD_PACKET_MSEC) {
        return qfalse;
    }

    if (cls.state != CA_ACTIVE &&
        cls.state != CA_PRIMED &&
        downloading == qfalse &&
        elapsedSincePacket <
            CL_CONNECTING_PACKET_MSEC) {
        return qfalse;
    }

    if (clc.netchan.remoteAddress.type ==
            NA_LOOPBACK ||
        Sys_IsLANAddress(
            clc.netchan.remoteAddress) != qfalse) {
        return qtrue;
    }

    if (cl_maxpackets->integer <
        CL_MAXPACKETS_MIN) {
        Cvar_Set("cl_maxpackets", "15");
    } else if (cl_maxpackets->integer >
               CL_MAXPACKETS_MAX) {
        Cvar_Set("cl_maxpackets", CL_MAXPACKETS_MAX_TEXT);
    }

    const clOutPacket_t *const lastPacket =
        &cl.outPackets[
            ((uint32_t)clc.netchan.outgoingSequence -
             1u) &
            (uint32_t)CL_OUT_PACKET_MASK];
    const int32_t elapsedSinceRecordedPacket =
        (int32_t)((uint32_t)cls.realTime -
                  (uint32_t)lastPacket->sendRealTime);
    return elapsedSinceRecordedPacket >=
                   CL_MILLISECONDS_PER_SECOND_LOCAL /
                       cl_maxpackets->integer
        ? qtrue
        : qfalse;
}

/* Source: CoDUOMP.exe 0x0040cc80..0x0040d0c8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040cc80_0040d0c9.mcode.
 * Name and packet format: exact same-module Mac symbol CL_WritePacket. The
 * first nine bytes remain uncompressed because they carry the client server
 * id and the two acknowledgement sequences used to decode the rest. */
void CL_WritePacket(void)
{
    enum {
        CL_CLIENT_COMMAND_BITS = 2,
        CL_CLIENT_COMMAND_RELIABLE = 2,
        CL_CLIENT_COMMAND_MOVE = 0,
        CL_CLIENT_COMMAND_MOVE_NO_DELTA = 1,
        CL_CLIENT_COMMAND_EOF = 3,
        CL_PACKET_HEADER_BYTES = 9,
        CL_MAX_PACKET_USERCMDS = 32,
        CL_PACKET_DUP_MIN = 0,
        CL_PACKET_DUP_MAX = 5,
        CL_RELIABLE_COMMAND_MASK =
            CODUO_RELIABLE_COMMAND_COUNT - 1,
        CL_USERCMD_RING_MASK =
            CODUO_USERCMD_BACKUP_COUNT - 1,
        CL_OUT_PACKET_MASK =
            CODUO_SNAPSHOT_BACKUP_COUNT - 1,
        CL_COMMAND_HASH_LENGTH = 32
    };

    if (clc.demoPlayback != qfalse ||
        cls.state == CA_CINEMATIC ||
        cls.state == CA_LOGO) {
        return;
    }

    msg_t message;
    uint8_t messageData[MAX_MSGLEN];
    uint8_t packetData[MAX_MSGLEN];
    usercmd_t defaultCommand;
    const usercmd_t *lastCommand = &defaultCommand;

    MSG_SetDefaultUserCmd(&cl.snap.ps, &defaultCommand);
    MSG_Init(&message, messageData, sizeof(messageData));

    MSG_WriteByte(&message, cl.serverId);
    MSG_WriteLong(
        &message, clc.serverMessageSequence);
    MSG_WriteLong(
        &message, clc.serverCommandSequence);

    for (int32_t sequence =
             (int32_t)((uint32_t)clc.reliableAcknowledge + 1u);
         sequence <= clc.reliableSequence;
         sequence = (int32_t)((uint32_t)sequence + 1u)) {
        MSG_WriteBits(
            &message, CL_CLIENT_COMMAND_RELIABLE,
            CL_CLIENT_COMMAND_BITS);
        MSG_WriteLong(&message, sequence);
        MSG_WriteString(
            &message,
            clc.reliableCommands[
                (uint32_t)sequence &
                (uint32_t)CL_RELIABLE_COMMAND_MASK]);
    }

    if (cl_packetdup->integer <
        CL_PACKET_DUP_MIN) {
        Cvar_Set("cl_packetdup", "0");
    } else if (cl_packetdup->integer >
               CL_PACKET_DUP_MAX) {
        Cvar_Set("cl_packetdup", "5");
    }

    const clOutPacket_t *const duplicateBasePacket =
        &cl.outPackets[
            ((uint32_t)clc.netchan.outgoingSequence -
             (uint32_t)cl_packetdup->integer - 1u) &
            (uint32_t)CL_OUT_PACKET_MASK];
    int32_t commandCount = (int32_t)(
        (uint32_t)cl.cmdNumber -
        (uint32_t)duplicateBasePacket->lastCommandNumber);
    if (commandCount >
        CL_MAX_PACKET_USERCMDS) {
        Com_Printf("MAX_PACKET_USERCMDS\n");
        commandCount =
            CL_MAX_PACKET_USERCMDS;
    }

    if (commandCount >= 1) {
        if (cl_showSend->integer != 0)
            Com_Printf("(%i) ", commandCount);

        const qboolean useDelta =
            cl_nodelta->integer == 0 &&
            cl.snap.valid != qfalse &&
            clc.demoWaiting == qfalse &&
            clc.serverMessageSequence ==
                cl.snap.messageNum
                ? qtrue
                : qfalse;
        MSG_WriteBits(
            &message,
            useDelta != qfalse
                ? CL_CLIENT_COMMAND_MOVE
                : CL_CLIENT_COMMAND_MOVE_NO_DELTA,
            CL_CLIENT_COMMAND_BITS);
        MSG_WriteByte(&message, commandCount);

        uint32_t key =
            (uint32_t)clc.checksumFeed ^
            (uint32_t)clc.serverMessageSequence;
        key ^= Com_HashKey(
            clc.serverCommands[
                (uint32_t)clc.serverCommandSequence &
                (uint32_t)CL_RELIABLE_COMMAND_MASK],
            CL_COMMAND_HASH_LENGTH);

        for (int32_t commandIndex = 0;
             commandIndex < commandCount;
             ++commandIndex) {
            const int32_t commandNumber =
                (int32_t)(
                    (uint32_t)cl.cmdNumber -
                    (uint32_t)commandCount +
                    (uint32_t)commandIndex + 1u);
            const usercmd_t *const command =
                &cl.cmds[
                    (uint32_t)commandNumber &
                    (uint32_t)CL_USERCMD_RING_MASK];
            MSG_WriteDeltaUsercmdKey(
                &message, key, lastCommand, command);
            lastCommand = command;
        }
    }

    MSG_WriteBits(
        &message, CL_CLIENT_COMMAND_EOF,
        CL_CLIENT_COMMAND_BITS);

    memcpy(packetData, messageData,
           CL_PACKET_HEADER_BYTES);
    const int32_t packetSize =
        MSG_WriteBitsCompress(
            messageData + CL_PACKET_HEADER_BYTES,
            packetData + CL_PACKET_HEADER_BYTES,
            message.cursize -
                CL_PACKET_HEADER_BYTES) +
        CL_PACKET_HEADER_BYTES;

    clOutPacket_t *const outgoingPacket =
        &cl.outPackets[
            (uint32_t)clc.netchan.outgoingSequence &
            (uint32_t)CL_OUT_PACKET_MASK];
    outgoingPacket->lastCommandNumber = cl.cmdNumber;
    outgoingPacket->lastCommandTime =
        lastCommand->commandTime;
    outgoingPacket->sendRealTime = cls.realTime;
    clc.lastPacketSentTime = cls.realTime;

    if (cl_showSend->integer != 0)
        Com_Printf("%i ", packetSize);

    CL_Netchan_Transmit(
        &clc.netchan, packetData, packetSize);
    while (clc.netchan.unsentFragments != qfalse) {
        CL_Netchan_TransmitNextFragment(
            &clc.netchan);
    }
}

/* Source: CoDUOMP.exe 0x0040d0d0..0x0040d12e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040d0d0_0040d12f.mcode.
 * Name and control flow: exact same-module Mac symbol CL_SendCmd. */
void CL_SendCmd(void)
{
    if (cls.state < CA_CONNECTED)
        return;

    if (sv_running->integer != 0 &&
        sv_paused->integer == 1 &&
        cl_paused->integer == 1) {
        return;
    }

    CL_CreateNewCommands();
    if (CL_ReadyToSendPacket() != qfalse) {
        CL_WritePacket();
    } else if (cl_showSend->integer != 0) {
        Com_Printf(". ");
    }
}

/* Source: CoDUOMP.exe 0x00413870..0x00413a97.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00413870_00413a98.mcode.
 * Name and argument roles: exact same-module Mac symbol CL_Frame. Win32
 * carries the scaled frame duration in EAX and receives the real duration as
 * its sole stack argument. */
void CL_Frame(int32_t msec, int32_t realMsec)
{
    if (cl_running->integer == 0)
        return;

    if (cls.state == CA_ACTIVE &&
        cl_executeString->string[0] != '\0') {
        Cmd_ExecuteString(cl_executeString->string);
        Cvar_Set("cl_executeString", "");
    }

    CL_UpdateColor();
    XAnimSetUser(XANIM_USER_CLIENT);

    if (cls.cdDialogRequested != qfalse) {
        cls.cdDialogRequested = qfalse;
        (void)VM_Call(
            coduo_uiVm, UIVM_SET_ACTIVE_MENU,
            UI_MENU_NEED_CD,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    } else if (cls.state == CA_DISCONNECTED &&
               (cls.keyCatchers & KEYCATCH_UI) == 0 &&
               sv_running->integer == 0) {
        MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
        (void)VM_Call(
            coduo_uiVm, UIVM_SET_ACTIVE_MENU,
            UI_MENU_MAIN,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }

    if (cl_avidemo->integer != 0 && msec != 0) {
        if (cls.state == CA_ACTIVE ||
            cl_forceavidemo->integer != 0) {
            if (cl_avidemo->integer > 0) {
                Cmd_ExecuteString("screenshot silent\n");
            } else {
                Cmd_ExecuteString("screenshotjpeg silent\n");
            }

            msec = (int32_t)(
                (CL_AVIDEMO_MSEC_PER_SECOND /
                 fabsf((float)cl_avidemo->integer)) *
                com_timescale->value);
            if (msec == 0)
                msec = CL_AVIDEMO_MIN_FRAME_MSEC;
        }
    }

    CL_ChangeReliableCommand();

    cls.realtime = (int32_t)(
        (uint32_t)cls.realtime + (uint32_t)msec);
    cls.realTime = (int32_t)(
        (uint32_t)cls.realTime + (uint32_t)realMsec);
    cls.realFrametime = realMsec;
    cls.frameTime = msec;

    if (scr_timegraph->integer != 0) {
        SCR_DebugGraph(
            (float)realMsec * CL_TIMEGRAPH_FRAME_SCALE, 0);
    }

    CL_CheckUserinfo();
    CL_CheckTimeout();
    if ((cls.state == CA_CONNECTED &&
         clc.wwwDownloadActive != qfalse) ||
        cls.wwwDownloadDisconnected != 0) {
        CL_WWWDownload();
    }
    CL_PlayVoiceChat();
    CL_SendCmd();
    CL_CheckForResend();
    CL_SetCGameTime();
    CL_UpdateInGameState();
    SCR_UpdateScreen();
    MSS_Update();
    SCR_RunCinematic();
    Con_RunConsole();

    cls.frameCount = (int32_t)(
        (uint32_t)cls.frameCount + 1u);
}

/* Source: CoDUOMP.exe 0x00413ef0..0x00413f32.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00413ef0_00413f33.mcode.
 * Name: exact same-module Mac symbol CL_ScaledMilliseconds. The x87 FILD in
 * the Windows image treats the wrapping millisecond count as a signed dword. */
int32_t CL_ScaledMilliseconds(void)
{
    const int32_t milliseconds = (int32_t)Sys_Milliseconds();
    return coduo_fp_to_i32_extended(
        (long double)milliseconds * (long double)com_timescale->value);
}

/* Source: CoDUOMP.exe 0x00405510..0x00405524.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00405510_00405525.mcode.
 * Name: exact same-module Mac symbol CL_GameCommand. */
qboolean CL_GameCommand(void)
{
    if (coduo_cgameVm == NULL)
        return qfalse;

    return (qboolean)VM_Call(
        coduo_cgameVm, CGVM_CONSOLE_COMMAND,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* Source: CoDUOMP.exe 0x00405530..0x00405552.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00405530_00405553.mcode.
 * Name: exact same-module Mac symbol CL_CGameRendering. Win32 carries
 * stereoView in EDX and drawFrame in EAX; the cgame command itself proves all
 * six semantic frame arguments. */
void CL_CGameRendering(int32_t stereoView, qboolean drawFrame)
{
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): identify every renderer
     * command submitted by the cgame VM. The command-buffer markers preserve
     * that identity until deferred backend execution; a simultaneous
     * fullscreen state in the separate UI VM must not reinterpret cgame
     * menus, scoreboard, text, or HUD geometry. */
    coduomp_queue_cgame_2d_presentation(qtrue);
    coduomp_cgame_rendering_compat_active = qtrue;
    (void)VM_Call(
        coduo_cgameVm, CGVM_DRAW_ACTIVE_FRAME,
        cl.serverTime, stereoView, clc.demoPlayback,
        0, 0, drawFrame,
        0, 0, 0, 0, 0, 0);
    coduomp_cgame_rendering_compat_active = qfalse;
    coduomp_queue_cgame_2d_presentation(qfalse);
}

/* Source: CoDUOMP.exe 0x00405b50..0x00405caa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00405b50_00405cab.mcode.
 * Name and argument order: same-module Mac symbol CL_UpdateColorInternal and
 * PowerPC r3=cvarName/r4=color entry state. */
void CL_UpdateColorInternal(const char *cvarName, vec4_t color)
{
    char value[MAX_STRING_CHARS];
    cvar_t *cvar = Cvar_FindVar(cvarName);

    if (cvar != NULL) {
        strncpy(value, cvar->string, sizeof(value) - 1);
        value[sizeof(value) - 1] = '\0';
    } else {
        value[0] = '\0';
    }

    if (value[0] != '\0') {
        (void)sscanf(value, "%f %f %f", &color[0], &color[1], &color[2]);
    } else {
        color[0] = 1.0f;
        color[1] = 1.0f;
        color[2] = 1.0f;
    }
    color[3] = 1.0f;

    for (int32_t component = 0; component < 3; ++component) {
        if (color[component] < 0.0f)
            color[component] = 0.0f;
        else if (color[component] > 1.0f)
            color[component] = 1.0f;
    }
}

/* Source: CoDUOMP.exe 0x00405cb0..0x00405cf1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00405cb0_00405cf2.mcode.
 * Name: exact same-module Mac symbol CL_UpdateColor. The Windows optimizer
 * inlines RB_UpdateColor's two RB_UpdateColorInternal calls. */
void CL_UpdateColor(void)
{
    CL_UpdateColorInternal("g_TeamColor_Allies", cl.teamColorAllies);
    CL_UpdateColorInternal("g_TeamColor_Axis", cl.teamColorAxis);
    RB_UpdateColor(cl.teamColorAllies, cl.teamColorAxis);
}

/* Source: CoDUOMP.exe 0x00405aa0..0x00405b44.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00405aa0_00405b45.mcode.
 * Name and argument order: same-module Mac symbol CL_LookupColor and PowerPC
 * r3=colorCode/r4=color entry state. */
void CL_LookupColor(uint8_t colorCode, vec4_t color)
{
    const uint8_t numericCode = (uint8_t)(colorCode - (uint8_t)'0');
    const uint8_t tableIndex = numericCode < 10 ? numericCode : 7;
    const float *selected;

    if (tableIndex < 8)
        selected = cl_baseColorCodes[tableIndex];
    else if (colorCode == (uint8_t)'8')
        selected = cl.teamColorAllies;
    else if (colorCode == (uint8_t)'9')
        selected = cl.teamColorAxis;
    else
        selected = cl_baseColorCodes[7];

    for (int32_t component = 0; component < 4; ++component)
        color[component] = selected[component];
}

/* Source: CoDUOMP.exe 0x00405a10..0x00405a3e.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00405a10_00405a3f.mcode.
 * Name: exact same-module Mac symbol CL_DrawString. */
void CL_DrawString(int32_t x, int32_t y, const char *text,
                   int32_t mode, int32_t charWidth,
                   int32_t charHeight, int32_t textStyle)
{
    if (coduo_cgameVm != NULL) {
        (void)VM_Call(
            coduo_cgameVm, CGVM_DRAW_SCALED,
            x, y, (intptr_t)text, mode, charWidth, charHeight, textStyle,
            0, 0, 0, 0, 0);
    }
}

/* Source: CoDUOMP.exe 0x00405a40..0x00405a68.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00405a40_00405a69.mcode.
 * Name: exact same-module Mac symbol CL_SaveCgameState. */
int32_t CL_SaveCgameState(int32_t bufferSize, uint8_t *buffer)
{
    const int32_t fogStateBytes =
        rendererExports.SaveFogState(buffer, (uint32_t)bufferSize);
    const int32_t cgameStateBytes = (int32_t)VM_Call(
        coduo_cgameVm, CGVM_SAVE_STATE,
        (intptr_t)(buffer + fogStateBytes),
        (int32_t)((uint32_t)bufferSize - (uint32_t)fogStateBytes),
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    return (int32_t)((uint32_t)fogStateBytes +
                     (uint32_t)cgameStateBytes);
}

/* Source: CoDUOMP.exe 0x00405a70..0x00405a98.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00405a70_00405a99.mcode.
 * Name: exact same-module Mac symbol CL_RestoreCgameState. */
int32_t CL_RestoreCgameState(int32_t bufferSize, uint8_t *buffer)
{
    const int32_t fogStateBytes =
        rendererExports.RestoreFogState(buffer, (uint32_t)bufferSize);
    const int32_t cgameStateBytes = (int32_t)VM_Call(
        coduo_cgameVm, CGVM_RESTORE_STATE,
        (intptr_t)(buffer + fogStateBytes),
        (int32_t)((uint32_t)bufferSize - (uint32_t)fogStateBytes),
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    return (int32_t)((uint32_t)fogStateBytes +
                     (uint32_t)cgameStateBytes);
}

/* Source: CoDUOMP.exe 0x00405710..0x00405762.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00405710_00405763.mcode.
 * Name: exact same-module Mac symbol CL_TimeDemoLogBaseName. */
char *CL_TimeDemoLogBaseName(void)
{
    char *baseName = cl.mapBspName;
    char *extension = NULL;

    for (char *cursor = cl.mapBspName; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') {
            baseName = cursor + 1;
            extension = NULL;
        } else if (*cursor == '.') {
            extension = cursor;
        }
    }

    if (extension == NULL) {
        return baseName;
    }

    char *result = va("%s", baseName);
    result[extension - baseName] = '\0';
    return result;
}

/* Source: CoDUOMP.exe 0x00405770..0x00405835.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00405770_00405836.mcode.
 * Name: exact same-module Mac symbol CL_UpdateTimeDemo. */
void CL_UpdateTimeDemo(void)
{
    if (clc.timeDemoLogFile == 0) {
        const int32_t mode = Cvar_VariableIntegerValue("r_mode");
        const char *path =
            va("demos/timedemo_%s_mode_%i.csv",
               CL_TimeDemoLogBaseName(), mode);
        clc.timeDemoLogFile = FS_FOpenFileWrite(path);
    }

    const uint32_t now = Sys_Milliseconds();
    if (clc.timeDemoStartTime == 0) {
        clc.timeDemoStartTime = now;
    } else {
        FS_Printf(clc.timeDemoLogFile, "%i,%i\n",
                  clc.timeDemoFrameCount,
                  (int32_t)(now - clc.timeDemoPreviousFrameTime));
    }

    clc.timeDemoFrameCount =
        (int32_t)((uint32_t)clc.timeDemoFrameCount + 1u);
    clc.timeDemoPreviousFrameTime = now;
    cl.serverTime =
        (int32_t)((uint32_t)clc.timeDemoBaseTime +
                  (uint32_t)clc.timeDemoFrameCount *
                      CL_TIMEDEMO_FRAME_MSEC);
}

/* Source: CoDUOMP.exe 0x004056a0..0x00405703.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004056a0_00405704.mcode.
 * Name: exact same-module Mac symbol CL_FirstSnapshot. */
void CL_FirstSnapshot(void)
{
    if ((cl.snap.snapFlags & SNAPFLAG_NOT_ACTIVE) != 0) {
        return;
    }

    const int32_t initialTimeDelta =
        (int32_t)((uint32_t)cl.snap.serverTime -
                  (uint32_t)cls.realtime);
    cls.state = CA_ACTIVE;
    cl.serverTimeDelta = initialTimeDelta;
    cl.oldServerTime = cl.snap.serverTime;
    clc.timeDemoBaseTime = cl.snap.serverTime;

    if (cl_activeAction->string[0] != '\0') {
        Cbuf_AddText(cl_activeAction->string);
        Cbuf_AddText("\n");
        Cvar_Set("activeAction", "");
    }
}

/* Source: CoDUOMP.exe 0x00405560..0x00405697.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00405560_00405698.mcode.
 * Name: exact same-module Mac symbol CL_AdjustTimeDelta. The extra positive-
 * delta adjustment uses the previous snapshot server time captured by the
 * snapshot parser; its unsigned 500-ms comparison is preserved explicitly. */
void CL_AdjustTimeDelta(void)
{
    cl.newSnapshots = qfalse;
    if (clc.demoPlayback != qfalse) {
        return;
    }

    const int32_t newDelta =
        (int32_t)((uint32_t)cl.snap.serverTime -
                  (uint32_t)cls.realtime);
    int32_t deltaDelta =
        (int32_t)((uint32_t)newDelta - (uint32_t)cl.serverTimeDelta);

    if (deltaDelta > 0) {
        const int32_t snapshotAdvance =
            (int32_t)((uint32_t)cl.snap.serverTime -
                      (uint32_t)cl.previousSnapshotServerTime);
        if ((uint32_t)snapshotAdvance <=
            CL_TIME_DELTA_RESET_THRESHOLD_MSEC) {
            deltaDelta =
                (int32_t)((uint32_t)deltaDelta -
                          (uint32_t)snapshotAdvance);
            if (deltaDelta < 0) {
                deltaDelta = 0;
            }
            if (snapshotAdvance == 0) {
                cl.serverTimeDelta = newDelta;
                cl.oldServerTime = cl.snap.serverTime;
                cl.serverTime = cl.snap.serverTime;
            }
        }
    } else {
        deltaDelta = (int32_t)(0u - (uint32_t)deltaDelta);
    }

    if (deltaDelta > CL_TIME_DELTA_RESET_THRESHOLD_MSEC) {
        cl.serverTimeDelta = newDelta;
        cl.oldServerTime = cl.snap.serverTime;
        cl.serverTime = cl.snap.serverTime;
        if (cl_showTimeDelta->integer != 0) {
            Com_Printf("<RESET> ");
        }
    } else if (deltaDelta > CL_TIME_DELTA_FAST_THRESHOLD_MSEC) {
        if (cl_showTimeDelta->integer != 0) {
            Com_Printf("<FAST> ");
        }
        const int32_t deltaSum =
            (int32_t)((uint32_t)cl.serverTimeDelta + (uint32_t)newDelta);
        cl.serverTimeDelta = deltaSum >> 1;
    } else if (com_timescale->value == 0.0f ||
               com_timescale->value == 1.0f) {
        if (cl.extrapolatedSnapshot != qfalse) {
            cl.extrapolatedSnapshot = qfalse;
            cl.serverTimeDelta =
                (int32_t)((uint32_t)cl.serverTimeDelta -
                          CL_TIME_DELTA_EXTRAPOLATION_CORRECTION_MSEC);
        } else {
            cl.serverTimeDelta =
                (int32_t)((uint32_t)cl.serverTimeDelta + 1u);
        }
    }

    if (cl_showTimeDelta->integer != 0) {
        Com_Printf("%i ", cl.serverTimeDelta);
    }
}

/* Source: CoDUOMP.exe 0x00405840..0x00405a01.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00405840_00405a02.mcode.
 * Name: exact same-module Mac symbol CL_SetCGameTime. */
void CL_SetCGameTime(void)
{
    if (cls.state != CA_ACTIVE) {
        if (cls.state != CA_PRIMED) {
            return;
        }

        if (clc.demoPlayback != qfalse) {
            if (clc.demoFirstFrameSkipped == qfalse) {
                clc.demoFirstFrameSkipped = qtrue;
                return;
            }
            CL_ReadDemoMessage();
        }

        if (cl.newSnapshots != qfalse) {
            cl.newSnapshots = qfalse;
            CL_FirstSnapshot();
        }
        if (cls.state != CA_ACTIVE) {
            return;
        }
    }

    if (cl.snap.valid == qfalse) {
        Com_Error(ERR_DROP,
                  "\x15" "CL_SetCGameTime: !cl.snap.valid");
    }

    if (sv_paused->integer != 0 && cl_paused->integer != 0 &&
        sv_running->integer == 0) {
        return;
    }

    if (cl.snap.serverTime < cl.oldFrameServerTime) {
        if (Q_stricmp(cls.serverName, "localhost") == 0) {
            CL_FirstSnapshot();
        } else {
            Com_Error(ERR_DROP,
                      "\x15" "cl.snap.serverTime < "
                      "cl.oldFrameServerTime");
        }
    }
    cl.oldFrameServerTime = cl.snap.serverTime;

    int32_t currentServerTime = cl.serverTime;
    if (clc.demoPlayback == qfalse || cl_freezeDemo->integer == 0) {
        int32_t timeNudge = cl_timeNudge->integer;
        if (timeNudge < CL_TIME_NUDGE_MIN_MSEC) {
            timeNudge = CL_TIME_NUDGE_MIN_MSEC;
        } else if (timeNudge > CL_TIME_NUDGE_MAX_MSEC) {
            timeNudge = CL_TIME_NUDGE_MAX_MSEC;
        }

        currentServerTime =
            (int32_t)((uint32_t)cls.realtime +
                      (uint32_t)cl.serverTimeDelta -
                      (uint32_t)timeNudge);
        if (currentServerTime < cl.oldServerTime) {
            currentServerTime = cl.oldServerTime;
        }
        cl.serverTime = currentServerTime;
        cl.oldServerTime = currentServerTime;

        const int32_t unnudgedServerTime =
            (int32_t)((uint32_t)cls.realtime +
                      (uint32_t)cl.serverTimeDelta);
        const int32_t extrapolationThreshold =
            (int32_t)((uint32_t)cl.snap.serverTime -
                      CL_EXTRAPOLATION_MARGIN_MSEC);
        if (unnudgedServerTime >= extrapolationThreshold) {
            cl.extrapolatedSnapshot = qtrue;
        }
    }

    if (cl.newSnapshots != qfalse) {
        CL_AdjustTimeDelta();
        currentServerTime = cl.serverTime;
    }

    if (clc.demoPlayback == qfalse) {
        return;
    }
    if (cl_timedemo->integer != 0) {
        CL_UpdateTimeDemo();
        currentServerTime = cl.serverTime;
    }

    while (currentServerTime >= cl.snap.serverTime) {
        CL_ReadDemoMessage();
        if (cls.state != CA_ACTIVE) {
            break;
        }
        currentServerTime = cl.serverTime;
    }
}
