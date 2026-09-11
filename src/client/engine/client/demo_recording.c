#include "cgame.h"
#include "console.h"

#include "../platform/crt_boundary.h"
#include "qcommon/q_string.h"
#include "../renderer/renderer_api.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

enum {
    CL_DEMO_BASE_NAME_CAPACITY = 64,
    CL_DEMO_FILENAME_CAPACITY = 256,
    CL_DEMO_EXTENSION_CAPACITY = 32,
    CL_DEMO_COMMAND_CAPACITY = 1024,
    CL_DEMO_MESSAGE_CAPACITY = 32768,
    CL_DEMO_MAX_AUTONAME_NUMBER = 9999,
    CL_DEMO_NUMBER_BASE = 10,
    CL_DEMO_HUNDREDS_DIVISOR = 100,
    CL_DEMO_THOUSANDS_DIVISOR = 1000,
    CL_DEMO_STREAM_END = -1,
    CL_DEMO_PROTOCOL_VERSION = 3,
    CL_DEMO_SVC_GAMESTATE = 2,
    CL_DEMO_SVC_CONFIGSTRING = 3,
    CL_DEMO_SVC_BASELINE = 4,
    CL_DEMO_SVC_EOF = 8,
    CODUOMP_DEMO_SEEK_MSEC = 5000,
    CODUOMP_DEMO_TIME_NUDGE_MIN_MSEC = -30,
    CODUOMP_DEMO_TIME_NUDGE_MAX_MSEC = 30,
    CODUOMP_DEMO_CONTROL_FONT = 5,
    CODUOMP_DEMO_CONTROL_TEXT_STYLE = 3
};

/* Original temporary base-name storage at 0x008ce960. It is only used while
 * CL_Record_f chooses and opens a demo, then copied into clc.demoName. */
static char cl_demoBaseName[CL_DEMO_BASE_NAME_CAPACITY];

/* NOT_FROM_ORIGINAL_SOURCE: keep manual demo controls on the existing demo
 * clock while preventing timedemo or AVI capture from overriding a pause. */
static void coduomp_demo_prepare_manual_control(void)
{
    if (cl_timedemo != NULL && cl_timedemo->integer != 0) {
        (void)Cvar_Set2("timedemo", "0", qtrue);
        if (clc.timeDemoLogFile != 0) {
            FS_FCloseFile(clc.timeDemoLogFile);
            clc.timeDemoLogFile = 0;
        }
    }
    if (cl_avidemo != NULL && cl_avidemo->integer != 0)
        (void)Cvar_Set2("cl_avidemo", "0", qtrue);
    if (cl_forceavidemo != NULL && cl_forceavidemo->integer != 0)
        (void)Cvar_Set2("cl_forceavidemo", "0", qtrue);
}

/* NOT_FROM_ORIGINAL_SOURCE: freeze both the demo clock and game audio. When
 * playback resumes, rebase the demo clock so wall time spent paused is not
 * interpreted as a request to skip forward. */
static void coduomp_demo_set_paused(qboolean paused)
{
    if (paused == qfalse) {
        int32_t timeNudge = cl_timeNudge != NULL
            ? cl_timeNudge->integer : 0;
        if (timeNudge < CODUOMP_DEMO_TIME_NUDGE_MIN_MSEC)
            timeNudge = CODUOMP_DEMO_TIME_NUDGE_MIN_MSEC;
        else if (timeNudge > CODUOMP_DEMO_TIME_NUDGE_MAX_MSEC)
            timeNudge = CODUOMP_DEMO_TIME_NUDGE_MAX_MSEC;
        cl.serverTimeDelta = (int32_t)(
            (uint32_t)cl.serverTime - (uint32_t)cls.realtime +
            (uint32_t)timeNudge);
        cl.oldServerTime = cl.serverTime;
    }

    (void)Cvar_Set2("cl_freezeDemo", paused != qfalse ? "1" : "0", qtrue);
    (void)Cvar_Set2("cl_paused", paused != qfalse ? "1" : "0", qtrue);
}

/* NOT_FROM_ORIGINAL_SOURCE: report whether the packet stream has reached the
 * active snapshot state required by playback seeking and frame stepping. */
static qboolean coduomp_demo_controls_available(void)
{
    return clc.demoPlayback != qfalse && clc.demoFile != 0 &&
           cls.state == CA_ACTIVE && cl.snap.valid != qfalse &&
           coduo_cgameVm != NULL
               ? qtrue : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: consume packets until the current snapshot lies
 * beyond the requested presentation time. Advance the cgame without drawing
 * at every crossed snapshot so its finite snapshot and server-command rings
 * cannot cycle out state needed at the destination. */
static qboolean coduomp_demo_advance_to_time(int32_t targetTime)
{
    while (clc.demoFile != 0 && cls.state == CA_ACTIVE &&
           cl.snap.valid != qfalse && cl.snap.serverTime <= targetTime) {
        const int32_t presentationTime = cl.snap.serverTime;
        CL_ReadDemoMessage();
        if (clc.demoFile != 0 && cls.state == CA_ACTIVE &&
            cl.newSnapshots != qfalse) {
            cl.newSnapshots = qfalse;
            cl.serverTime = presentationTime;
            cl.oldServerTime = presentationTime;
            CL_CGameRendering(STEREO_CENTER, qfalse);
        }
    }

    if (coduomp_demo_controls_available() == qfalse)
        return qfalse;

    cl.serverTime = targetTime;
    cl.oldServerTime = targetTime;
    cl.serverTimeDelta = (int32_t)(
        (uint32_t)targetTime - (uint32_t)cls.realtime);
    cl.extrapolatedSnapshot = qfalse;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: demos contain forward-only delta snapshots, so a
 * rewind restarts the stream, rebuilds cgame state from its gamestate record,
 * and replays packets up to the requested presentation time. */
static qboolean coduomp_demo_replay_to_time(int32_t targetTime)
{
    MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
    if (FS_Seek(clc.demoFile, 0, FS_SEEK_ORIGIN_SET) != 0) {
        Com_Printf("Unable to rewind the demo stream.\n");
        return qfalse;
    }

    clc.lastExecutedServerCommand = 0;
    clc.demoFirstFrameSkipped = qtrue;
    clc.timeDemoFrameCount = 0;
    clc.timeDemoStartTime = 0;
    clc.timeDemoPreviousFrameTime = 0;
    cls.state = CA_CONNECTED;

    while (clc.demoFile != 0 && cls.state >= CA_CONNECTED &&
           cls.state < CA_PRIMED) {
        CL_ReadDemoMessage();
    }
    while (clc.demoFile != 0 && cls.state == CA_PRIMED &&
           cl.newSnapshots == qfalse) {
        CL_ReadDemoMessage();
    }

    if (clc.demoFile == 0 || cls.state != CA_PRIMED ||
        cl.newSnapshots == qfalse || cl.snap.valid == qfalse) {
        return qfalse;
    }

    cl.newSnapshots = qfalse;
    CL_FirstSnapshot();
    if (cls.state != CA_ACTIVE)
        return qfalse;

    if (targetTime < cl.snap.serverTime)
        targetTime = cl.snap.serverTime;
    return coduomp_demo_advance_to_time(targetTime);
}

/* NOT_FROM_ORIGINAL_SOURCE: seek relative to the currently presented demo
 * time. Backward movement rebuilds delta-dependent state from the stream's
 * gamestate; forward movement can consume the existing stream directly. */
static void coduomp_demo_seek_relative(int32_t deltaMsec)
{
    if (coduomp_demo_controls_available() == qfalse) {
        Com_Printf("Demo playback controls require an active demo.\n");
        return;
    }

    coduomp_demo_prepare_manual_control();
    const qboolean wasPaused =
        cl_freezeDemo != NULL && cl_freezeDemo->integer != 0
            ? qtrue : qfalse;
    int64_t targetTime = (int64_t)cl.serverTime + (int64_t)deltaMsec;
    if (targetTime < INT32_MIN)
        targetTime = INT32_MIN;
    else if (targetTime > INT32_MAX)
        targetTime = INT32_MAX;

    const qboolean seekCompleted = deltaMsec < 0
        ? coduomp_demo_replay_to_time((int32_t)targetTime)
        : coduomp_demo_advance_to_time((int32_t)targetTime);
    if (seekCompleted != qfalse) {
        MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
        coduomp_demo_set_paused(wasPaused);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: toggle manual playback pause. */
void coduomp_DemoPause_f(void)
{
    if (coduomp_demo_controls_available() == qfalse) {
        Com_Printf("Demo playback controls require an active demo.\n");
        return;
    }

    coduomp_demo_prepare_manual_control();
    coduomp_demo_set_paused(
        cl_freezeDemo == NULL || cl_freezeDemo->integer == 0
            ? qtrue : qfalse);
}

/* NOT_FROM_ORIGINAL_SOURCE: rewind the active demo by five seconds. */
void coduomp_DemoRewind_f(void)
{
    coduomp_demo_seek_relative(-CODUOMP_DEMO_SEEK_MSEC);
}

/* NOT_FROM_ORIGINAL_SOURCE: advance the active demo by five seconds. */
void coduomp_DemoForward_f(void)
{
    coduomp_demo_seek_relative(CODUOMP_DEMO_SEEK_MSEC);
}

/* NOT_FROM_ORIGINAL_SOURCE: advance exactly one recorded snapshot and remain
 * paused so each key press produces one newly rendered demo frame. */
void coduomp_DemoFrameStep_f(void)
{
    if (coduomp_demo_controls_available() == qfalse) {
        Com_Printf("Demo playback controls require an active demo.\n");
        return;
    }

    coduomp_demo_prepare_manual_control();
    coduomp_demo_set_paused(qtrue);
    MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
    (void)coduomp_demo_advance_to_time(cl.snap.serverTime);
}

/* NOT_FROM_ORIGINAL_SOURCE: restore ordinary client timing and audio state
 * when demo playback begins, completes, or is disconnected manually. */
void coduomp_DemoPlaybackReset(void)
{
    if (cl_freezeDemo != NULL)
        (void)Cvar_Set2("cl_freezeDemo", "0", qtrue);
    if (cl_paused != NULL)
        (void)Cvar_Set2("cl_paused", "0", qtrue);
}

/* NOT_FROM_ORIGINAL_SOURCE: give demos a small set of direct player controls
 * while retaining Escape as the existing route back to the main menu. Exact
 * command bindings are also recognized so advanced users can remap controls. */
qboolean coduomp_DemoPlaybackKeyEvent(int32_t key, qboolean down,
                                      const char *binding)
{
    enum coduomp_demo_key_action_e {
        CODUOMP_DEMO_KEY_NONE,
        CODUOMP_DEMO_KEY_PAUSE,
        CODUOMP_DEMO_KEY_REWIND,
        CODUOMP_DEMO_KEY_FORWARD,
        CODUOMP_DEMO_KEY_FRAME_STEP
    } action = CODUOMP_DEMO_KEY_NONE;

    if (coduomp_demo_controls_available() == qfalse || cls.keyCatchers != 0)
        return qfalse;

    if (key == K_SPACE)
        action = CODUOMP_DEMO_KEY_PAUSE;
    else if (key == K_LEFTARROW || key == K_KP_LEFTARROW)
        action = CODUOMP_DEMO_KEY_REWIND;
    else if (key == K_RIGHTARROW || key == K_KP_RIGHTARROW)
        action = CODUOMP_DEMO_KEY_FORWARD;
    else if (key == '.')
        action = CODUOMP_DEMO_KEY_FRAME_STEP;
    else if (binding != NULL && Q_stricmp(binding, "demopause") == 0)
        action = CODUOMP_DEMO_KEY_PAUSE;
    else if (binding != NULL && Q_stricmp(binding, "demorewind") == 0)
        action = CODUOMP_DEMO_KEY_REWIND;
    else if (binding != NULL && Q_stricmp(binding, "demoforward") == 0)
        action = CODUOMP_DEMO_KEY_FORWARD;
    else if (binding != NULL && Q_stricmp(binding, "demoframestep") == 0)
        action = CODUOMP_DEMO_KEY_FRAME_STEP;

    if (action == CODUOMP_DEMO_KEY_NONE)
        return qfalse;
    if (down == qfalse)
        return qtrue;

    switch (action) {
    case CODUOMP_DEMO_KEY_PAUSE:
        coduomp_DemoPause_f();
        break;
    case CODUOMP_DEMO_KEY_REWIND:
        coduomp_DemoRewind_f();
        break;
    case CODUOMP_DEMO_KEY_FORWARD:
        coduomp_DemoForward_f();
        break;
    case CODUOMP_DEMO_KEY_FRAME_STEP:
        coduomp_DemoFrameStep_f();
        break;
    case CODUOMP_DEMO_KEY_NONE:
        break;
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: draw a compact, optional control legend during
 * demo playback so the direct controls are discoverable. */
void SCR_DrawDemoPlaybackControls(void)
{
    static const float textScale =
        0.3333333432674408f; /* 0x3eaaaaab, semantically 1/3 */
    static const vec4_t backgroundColor = { 0.0f, 0.0f, 0.0f, 0.62f };
    static const vec4_t playingColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    static const vec4_t pausedColor = { 1.0f, 0.82f, 0.2f, 1.0f };
    const qboolean paused =
        cl_freezeDemo != NULL && cl_freezeDemo->integer != 0
            ? qtrue : qfalse;
    const char *const text = paused != qfalse
        ? "DEMO PAUSED  SPACE Play   LEFT -5s   RIGHT +5s   . Frame step"
        : "DEMO  SPACE Pause   LEFT -5s   RIGHT +5s   . Frame step";

    if (clc.demoPlayback == qfalse || cls.state != CA_ACTIVE ||
        cls.keyCatchers != 0 ||
        Cvar_VariableIntegerValue("cl_demoControlOverlay") == 0) {
        return;
    }

    const float fixedAdvance = 7.0f;
    const float textWidth = (float)rendererExports.TextWidth(
        text, CODUOMP_DEMO_CONTROL_FONT, textScale, fixedAdvance, 0);
    const float textX = (640.0f - textWidth) * 0.5f;
    SCR_FillRect(textX - 7.0f, 450.0f, textWidth + 14.0f, 24.0f,
                 backgroundColor);
    rendererExports.TextPaint(
        textX, 468.0f, CODUOMP_DEMO_CONTROL_FONT, textScale,
        paused != qfalse ? pausedColor : playingColor, text,
        fixedAdvance, 0, CODUOMP_DEMO_CONTROL_TEXT_STYLE);
}

/* NOT_FROM_ORIGINAL_SOURCE: draw a resolution-independent recording dot as a
 * regular polygon so the indicator does not depend on proprietary UI assets. */
static void coduomp_scr_draw_demo_recording_indicator(void)
{
    enum {
        SCR_DEMO_INDICATOR_POINT_COUNT = 16
    };
    static const vec2_t unitCircle[SCR_DEMO_INDICATOR_POINT_COUNT + 1] = {
        { 1.0f, 0.0f },
        { 0.9238795f, 0.3826834f },
        { 0.7071068f, 0.7071068f },
        { 0.3826834f, 0.9238795f },
        { 0.0f, 1.0f },
        { -0.3826834f, 0.9238795f },
        { -0.7071068f, 0.7071068f },
        { -0.9238795f, 0.3826834f },
        { -1.0f, 0.0f },
        { -0.9238795f, -0.3826834f },
        { -0.7071068f, -0.7071068f },
        { -0.3826834f, -0.9238795f },
        { 0.0f, -1.0f },
        { 0.3826834f, -0.9238795f },
        { 0.7071068f, -0.7071068f },
        { 0.9238795f, -0.3826834f },
        { 1.0f, 0.0f }
    };
    static const vec2_t textureCoordinates[4] = {
        { 0.5f, 0.5f }, { 1.0f, 0.5f },
        { 0.5f, 1.0f }, { 0.5f, 0.5f }
    };
    static const vec4_t recordingColor = { 1.0f, 0.0f, 0.0f, 1.0f };
    const float scale = (float)cls.rendererConfig.vidHeight / 480.0f;
    const float radius = 7.0f * scale;
    const float centerX =
        (float)cls.rendererConfig.vidWidth - 16.0f * scale;
    const float centerY = (float)cls.rendererConfig.vidHeight * 0.5f;

    rendererExports.SetColor(recordingColor);
    for (int32_t point = 0;
         point < SCR_DEMO_INDICATOR_POINT_COUNT; ++point) {
        const vec2_t positions[4] = {
            { centerX, centerY },
            {
                centerX + unitCircle[point][0] * radius,
                centerY + unitCircle[point][1] * radius
            },
            {
                centerX + unitCircle[point + 1][0] * radius,
                centerY + unitCircle[point + 1][1] * radius
            },
            { centerX, centerY }
        };

        rendererExports.DrawQuadPic(
            positions, textureCoordinates, cls.whiteShader);
    }
    rendererExports.SetColor(NULL);
}

/* Source: CoDUOMP.exe 0x00419a60..0x00419b5e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00419a60_00419b5f.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * SCR_DrawDemoRecording. The Windows optimizer inlines FS_FTell. */
void SCR_DrawDemoRecording(void)
{
    enum {
        SCR_DEMO_TEXT_CAPACITY = 1024,
        SCR_DEMO_BYTES_PER_KILOBYTE = 1024,
        SCR_DEMO_FONT = 5,
        SCR_DEMO_TEXT_STYLE = 0
    };
    static const float textScale =
        0.3333333432674408f; /* 0x3eaaaaab, semantically 1/3 */
    char text[SCR_DEMO_TEXT_CAPACITY];
    vec4_t color;

    if (clc.demoRecording == qfalse)
        return;

    const int32_t kilobytes =
        FS_FTell(clc.demoFile) / SCR_DEMO_BYTES_PER_KILOBYTE;
    if (Cvar_FindVar("cg_showdemoname")->integer == 1) {
        (void)coduo_crt_snprintf(
            text, sizeof(text), "RECORDING %s: %ik", clc.demoName,
            kilobytes);
    } else {
        (void)coduo_crt_snprintf(
            text, sizeof(text), "RECORDING: %ik", kilobytes);
    }

    CL_LookupColor('7', color);
    rendererExports.TextPaint(
        5.0f, 479.0f, SCR_DEMO_FONT, textScale, color, text,
        8.0f, 0, SCR_DEMO_TEXT_STYLE);

    /* NOT_FROM_ORIGINAL_SOURCE: supplement the stock text with a compact
     * right-center recording indicator that remains round at every aspect. */
    coduomp_scr_draw_demo_recording_indicator();
}

/* Source: CoDUOMP.exe 0x0040fa10..0x0040fa61.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040fa10_0040fa62.mcode.
 * Name and signature: exact same-module Mac symbol CL_WriteDemoMessage. The
 * Windows whole-program optimizer passes message in EBX and headerBytes in
 * EDI; the direct caller at 0x00413382 proves both source arguments. */
void CL_WriteDemoMessage(const msg_t *message, int32_t headerBytes)
{
    const int32_t sequence = clc.serverMessageSequence;
    const int32_t payloadSize = message->cursize - headerBytes;

    (void)FS_Write(&sequence, sizeof(sequence), clc.demoFile);
    (void)FS_Write(&payloadSize, sizeof(payloadSize), clc.demoFile);
    (void)FS_Write(message->data + headerBytes, payloadSize, clc.demoFile);
}

/* Source: CoDUOMP.exe 0x0040fa70..0x0040fae4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040fa70_0040fae5.mcode.
 * Name and signature: exact same-module Mac symbol CL_StopRecord_f. */
void CL_StopRecord_f(void)
{
    if (clc.demoRecording == qfalse) {
        Com_Printf("Not recording a demo.\n");
        return;
    }

    const int32_t endMarker = CL_DEMO_STREAM_END;
    (void)FS_Write(&endMarker, sizeof(endMarker), clc.demoFile);
    (void)FS_Write(&endMarker, sizeof(endMarker), clc.demoFile);
    FS_FCloseFile(clc.demoFile);

    clc.demoFile = 0;
    clc.demoRecording = qfalse;
    Com_Printf("Stopped demo.\n");
}

/* Source: CoDUOMP.exe 0x0040faf0..0x0040fb7a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040faf0_0040fb7b.mcode.
 * Name and signature: exact same-module Mac symbol CL_DemoFilename. The
 * Windows whole-program optimizer passes number in ECX and fileName in EAX. */
void CL_DemoFilename(int32_t number, char *fileName)
{
    if (number < 0 || number > CL_DEMO_MAX_AUTONAME_NUMBER) {
        Com_sprintf(fileName, CL_DEMO_FILENAME_CAPACITY, "demo9999");
        return;
    }

    const int32_t thousands = number / CL_DEMO_THOUSANDS_DIVISOR;
    number -= thousands * CL_DEMO_THOUSANDS_DIVISOR;
    const int32_t hundreds = number / CL_DEMO_HUNDREDS_DIVISOR;
    number -= hundreds * CL_DEMO_HUNDREDS_DIVISOR;
    const int32_t tens = number / CL_DEMO_NUMBER_BASE;
    const int32_t ones = number - tens * CL_DEMO_NUMBER_BASE;

    Com_sprintf(fileName, CL_DEMO_FILENAME_CAPACITY, "demo%i%i%i%i",
                thousands, hundreds, tens, ones);
}

/* Source: CoDUOMP.exe 0x0040fb80..0x0040ffa6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040fb80_0040ffa7.mcode.
 * Name and signature: exact same-module Mac symbol CL_Record_f. The message
 * helper calls below are the source-level operations that MSVC inlined into
 * the original function. */
void CL_Record_f(void)
{
    char path[CL_DEMO_FILENAME_CAPACITY];
    uint8_t messageData[CL_DEMO_MESSAGE_CAPACITY];
    uint8_t compressedData[CL_DEMO_MESSAGE_CAPACITY];
    entityState_t nullEntity;
    msg_t message;

    if (Cmd_Argc() > 2) {
        Com_Printf("record <demoname>\n");
        return;
    }
    if (clc.demoRecording != qfalse) {
        Com_Printf("Already recording.\n");
        return;
    }
    if (cls.state != CA_ACTIVE) {
        Com_Printf("You must be in a level to record.\n");
        return;
    }

    if (Cmd_Argc() == 2) {
        Q_strncpyz(cl_demoBaseName, Cmd_Argv(1),
                   sizeof(cl_demoBaseName));
        Com_sprintf(path, sizeof(path), "demos/%s.dm_%d",
                    cl_demoBaseName, CL_DEMO_PROTOCOL_VERSION);
    } else {
        int32_t number = 0;
        do {
            CL_DemoFilename(number, cl_demoBaseName);
            Com_sprintf(path, sizeof(path), "demos/%s.dm_%d",
                        cl_demoBaseName, CL_DEMO_PROTOCOL_VERSION);
            if (FS_FileExists(path) == qfalse)
                break;
            ++number;
        } while (number <= CL_DEMO_MAX_AUTONAME_NUMBER);
    }

    Com_Printf("recording to %s.\n", path);
    clc.demoFile = FS_FOpenFileWrite(path);
    if (clc.demoFile == 0) {
        Com_Printf("ERROR: couldn't open.\n");
        return;
    }

    clc.demoRecording = qtrue;
    Q_strncpyz(clc.demoName, cl_demoBaseName, sizeof(clc.demoName));
    clc.demoWaiting = qtrue;

    MSG_Init(&message, messageData, sizeof(messageData));
    MSG_WriteLong(&message, clc.reliableSequence);
    MSG_WriteByte(&message, CL_DEMO_SVC_GAMESTATE);
    MSG_WriteLong(&message, clc.serverCommandSequence);

    for (int32_t index = 0; index < MAX_CONFIGSTRINGS; ++index) {
        const int32_t offset = cl.gameState.stringOffsets[index];
        if (offset == 0)
            continue;

        MSG_WriteByte(&message, CL_DEMO_SVC_CONFIGSTRING);
        MSG_WriteShort(&message, index);
        MSG_WriteBigString(&message, &cl.gameState.stringData[offset]);
    }

    memset(&nullEntity, 0, sizeof(nullEntity));
    for (int32_t index = 0; index < MAX_GENTITIES; ++index) {
        const entityState_t *const baseline = &cl.entityBaselines[index];
        if (baseline->number == 0)
            continue;

        MSG_WriteByte(&message, CL_DEMO_SVC_BASELINE);
        MSG_WriteDeltaEntity(&message, &nullEntity, baseline, qtrue);
    }

    MSG_WriteByte(&message, CL_DEMO_SVC_EOF);
    MSG_WriteLong(&message, clc.clientNum);
    MSG_WriteLong(&message, clc.checksumFeed);
    MSG_WriteByte(&message, CL_DEMO_SVC_EOF);

    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): the extended client can
     * accumulate more configstring data through runtime updates than the
     * unchanged 32-KiB demo gamestate message can serialize. Close the new
     * recording through its normal terminator path instead of compressing a
     * partial message after MSG_Write* publishes overflowed. */
    if (message.overflowed != qfalse) {
        Com_Printf("ERROR: gamestate is too large to record.\n");
        CL_StopRecord_f();
        return;
    }

    memcpy(compressedData, messageData, sizeof(int32_t));
    const int32_t compressedSize =
        MSG_WriteBitsCompress(
            messageData + sizeof(int32_t),
            compressedData + sizeof(int32_t),
            message.cursize - (int32_t)sizeof(int32_t)) +
        (int32_t)sizeof(int32_t);

    const int32_t sequence = clc.serverMessageSequence;
    (void)FS_Write(&sequence, sizeof(sequence), clc.demoFile);
    (void)FS_Write(&compressedSize, sizeof(compressedSize), clc.demoFile);
    (void)FS_Write(compressedData, compressedSize, clc.demoFile);
}

/* NOT_FROM_ORIGINAL_SOURCE: provide one bindable action for manually starting
 * an auto-named demo and stopping the active recording on the next press. */
void coduomp_ToggleRecord_f(void)
{
    if (clc.demoRecording != qfalse) {
        CL_StopRecord_f();
        return;
    }

    CL_Record_f();
}

/* Source: CoDUOMP.exe 0x0040ffb0..0x0041005b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040ffb0_0041005c.mcode.
 * Name and signature: exact same-module Mac symbol CL_DemoCompleted. The
 * original constants are the exact doubles 1000.0
 * (0x408f400000000000) and 0.001 (0x3f50624dd2f1a9fc). */
void CL_DemoCompleted(void)
{
    if (cl_timedemo != NULL && cl_timedemo->integer != 0) {
        const int32_t elapsedMilliseconds =
            (int32_t)(Sys_Milliseconds() - clc.timeDemoStartTime);
        if (elapsedMilliseconds > 0) {
            const double seconds = (double)elapsedMilliseconds * 0.001;
            const double framesPerSecond =
                (double)clc.timeDemoFrameCount * 1000.0 /
                (double)elapsedMilliseconds;
            Com_Printf("%i frames, %3.1f seconds: %3.1f fps\n",
                       clc.timeDemoFrameCount, seconds, framesPerSecond);
        }
    }

    if (clc.timeDemoLogFile != 0) {
        FS_FCloseFile(clc.timeDemoLogFile);
        clc.timeDemoLogFile = 0;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: demo completion also releases any manual
     * playback pause before the next queued demo begins. */
    coduomp_DemoPlaybackReset();
    CL_Disconnect(qtrue);
    CL_NextDemo();
}

/* Source: CoDUOMP.exe 0x00410060..0x004101c2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00410060_004101c3.mcode.
 * Name and signature: exact same-module Mac symbol CL_ReadDemoMessage. */
void CL_ReadDemoMessage(void)
{
    uint8_t messageData[CL_DEMO_MESSAGE_CAPACITY];
    msg_t message;
    int32_t sequence;

    if (clc.demoFile == 0 ||
        FS_Read(&sequence, sizeof(sequence), clc.demoFile) !=
            (int32_t)sizeof(sequence)) {
        CL_DemoCompleted();
        return;
    }

    clc.serverMessageSequence = sequence;
    MSG_Init(&message, messageData, sizeof(messageData));
    if (FS_Read(&message.cursize, sizeof(message.cursize), clc.demoFile) !=
            (int32_t)sizeof(message.cursize) ||
        message.cursize == CL_DEMO_STREAM_END) {
        CL_DemoCompleted();
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (message.cursize < 0 || message.cursize > message.maxsize) {
        Com_Error(ERR_DROP, "\x15" "CL_ReadDemoMessage: invalid demo message length %i", message.cursize);
        return;
    }

    if (FS_Read(message.data, message.cursize, clc.demoFile) !=
        message.cursize) {
        Com_Printf("Demo file was truncated.\n");
        CL_DemoCompleted();
        return;
    }

    clc.lastPacketTime = cls.realTime;
    message.bit = 0;
    clc.reliableAcknowledge = MSG_ReadLong(&message);
    if (clc.reliableAcknowledge <
        clc.reliableSequence - CODUO_RELIABLE_COMMAND_COUNT) {
        clc.reliableAcknowledge = clc.reliableSequence;
        return;
    }

    CL_ParseServerMessage(&message);
}

/* Source: CoDUOMP.exe 0x004101d0..0x004103d0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004101d0_004103d1.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_PlayDemo_f. The executable accepts either a bare demo name or one already
 * ending in the current .dm_3 protocol suffix, opens the demo as a unique
 * filesystem handle, then consumes messages until the connection reaches the
 * primed state or demo completion disconnects it. */
void CL_PlayDemo_f(void)
{
    if (Cmd_Argc() != 2) {
        Com_Printf("playdemo <demoname>\n");
        return;
    }

    if (sv_running->integer != 0) {
        Com_Printf("listen server cannot play a demo.\n");
        return;
    }

    CL_Disconnect(qtrue);
    /* NOT_FROM_ORIGINAL_SOURCE: a manually frozen prior demo must not carry
     * its timing or audio pause into newly started playback. */
    coduomp_DemoPlaybackReset();

    const char *const demoName = Cmd_Argv(1);
    char extension[CL_DEMO_EXTENSION_CAPACITY];
    char path[CL_DEMO_FILENAME_CAPACITY];

    Com_sprintf(extension, sizeof(extension), ".dm_%d",
                CL_DEMO_PROTOCOL_VERSION);

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    const size_t demoNameLength = strlen(demoName);
    const size_t extensionLength = strlen(extension);
    if (demoNameLength >= extensionLength &&
        Q_stricmp(demoName + demoNameLength - extensionLength,
                  extension) == 0) {
        Com_sprintf(path, sizeof(path), "demos/%s", demoName);
    } else {
        Com_sprintf(path, sizeof(path), "demos/%s.dm_%d",
                    demoName, CL_DEMO_PROTOCOL_VERSION);
    }

    (void)FS_FOpenFileRead(path, &clc.demoFile, qtrue);
    if (clc.demoFile == 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        Com_Error(ERR_DROP, "EXE_ERR_NOT_FOUND\x15%s", path);
    }

    Q_strncpyz(clc.demoName, demoName, sizeof(clc.demoName));
    Con_Close();

    clc.demoPlayback = qtrue;
    cls.state = CA_CONNECTED;
    Q_strncpyz(cls.serverName, demoName, sizeof(cls.serverName));

    while (cls.state >= CA_CONNECTED &&
           cls.state < CA_PRIMED) {
        CL_ReadDemoMessage();
    }

    clc.demoFirstFrameSkipped = qfalse;
}

/* Source: CoDUOMP.exe 0x004103e0..0x004103f4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004103e0_004103f5.mcode.
 * Name and signature: exact same-module Mac symbol CL_StartDemoLoop. */
void CL_StartDemoLoop(void)
{
    Cbuf_AddText("d1\n");
    cls.keyCatchers = 0;
}

/* Source: CoDUOMP.exe 0x00410400..0x0041049c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00410400_0041049d.mcode.
 * Name and signature: exact same-module Mac symbol CL_NextDemo. */
void CL_NextDemo(void)
{
    char command[CL_DEMO_COMMAND_CAPACITY];
    const cvar_t *const nextDemo = Cvar_FindVar("nextdemo");

    Q_strncpyz(command, nextDemo != NULL ? nextDemo->string : "",
               sizeof(command));
    Com_DPrintf("CL_NextDemo: %s\n", command);
    if (command[0] == '\0')
        return;

    Cvar_Set2("nextdemo", "", qtrue);
    Cbuf_AddText(command);
    Cbuf_AddText("\n");
    Cbuf_Execute();
}
