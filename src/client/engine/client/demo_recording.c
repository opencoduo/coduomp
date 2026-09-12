#include "cgame.h"
#include "console.h"

#include "filesystem/filesystem.h"
#include "../filesystem/server_namespace.h"
#include "../platform/crt_boundary.h"
#include "qcommon/q_string.h"
#include "../renderer/renderer_api.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
    CODUOMP_DEMO_CONTROL_TEXT_STYLE = 3,
    CODUOMP_DEMO_PLAYBACK_SPEED_MAX = 8,
    CODUOMP_DEMO_TIME_TEXT_CAPACITY = 32,
    CODUOMP_DEMO_STATUS_TEXT_CAPACITY = 192
};

#define CODUOMP_DEMO_TIMELINE_X 50.0f
#define CODUOMP_DEMO_TIMELINE_WIDTH 540.0f

typedef struct coduomp_demo_playback_state_s {
    qboolean timelineKnown;
    qboolean timelineScanning;
    qboolean timelineScanEnded;
    qboolean scrubActive;
    qboolean resumeAfterScrub;
    int32_t startTime;
    int32_t endTime;
    double scrubFraction;
} coduomp_demo_playback_state_t;

static cvar_t *coduomp_demoPlaybackSpeed;
static coduomp_demo_playback_state_t coduomp_demoPlaybackState;

typedef struct coduomp_demo_list_entry_s {
    char modName[FS_PACK_NAME_SIZE];
    char demoFileName[CL_DEMO_FILENAME_CAPACITY];
    char sourceName[MAX_QPATH];
    int64_t modificationTime;
    int64_t numberSuffix;
    qboolean hasNumberSuffix;
} coduomp_demo_list_entry_t;

typedef struct coduomp_demo_list_s {
    coduomp_demo_list_entry_t *entries;
    size_t count;
    size_t capacity;
    qboolean allocationFailed;
} coduomp_demo_list_t;

/* Original temporary base-name storage at 0x008ce960. It is only used while
 * CL_Record_f chooses and opens a demo, then copied into clc.demoName. */
static char cl_demoBaseName[CL_DEMO_BASE_NAME_CAPACITY];

/* NOT_FROM_ORIGINAL_SOURCE: register the improved demo-player speed control
 * without changing the process-wide timescale setting. */
void coduomp_DemoPlaybackInit(void)
{
    coduomp_demoPlaybackSpeed =
        Cvar_Get("cl_demoPlaybackSpeed", "1", CVAR_TEMP);
}

/* NOT_FROM_ORIGINAL_SOURCE: keep the demo-only speed in the supported cycle
 * even if the cvar is changed manually from the console. */
static int32_t coduomp_demo_playback_speed(void)
{
    if (coduomp_demoPlaybackSpeed == NULL)
        return 1;

    const int32_t speed = coduomp_demoPlaybackSpeed->integer;
    if (speed < 1)
        return 1;
    if (speed > CODUOMP_DEMO_PLAYBACK_SPEED_MAX)
        return CODUOMP_DEMO_PLAYBACK_SPEED_MAX;
    return speed;
}

/* NOT_FROM_ORIGINAL_SOURCE: apply the demo-only multiplier to the client
 * clock after the ordinary process timescale has been evaluated. */
int32_t coduomp_DemoPlaybackScaleMsec(int32_t msec)
{
    if (clc.demoPlayback == qfalse)
        return msec;

    const int64_t scaled =
        (int64_t)msec * (int64_t)coduomp_demo_playback_speed();
    if (scaled > INT32_MAX)
        return INT32_MAX;
    if (scaled < INT32_MIN)
        return INT32_MIN;
    return (int32_t)scaled;
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the effective demo multiplier to the
 * sound mixer so fast playback accelerates active channels consistently. */
float coduomp_DemoPlaybackSpeedScale(void)
{
    return clc.demoPlayback != qfalse
        ? (float)coduomp_demo_playback_speed() : 1.0f;
}

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
        if (coduomp_demoPlaybackState.timelineKnown != qfalse &&
            cl.snap.serverTime >= coduomp_demoPlaybackState.endTime) {
            break;
        }

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

/* NOT_FROM_ORIGINAL_SOURCE: scan the delta stream once to discover its true
 * first and last snapshot timestamps, then rebuild playback at the position
 * visible before the scan. */
static qboolean coduomp_demo_ensure_timeline(void)
{
    if (coduomp_demoPlaybackState.timelineKnown != qfalse)
        return qtrue;
    if (coduomp_demo_controls_available() == qfalse)
        return qfalse;

    coduomp_demo_prepare_manual_control();
    const qboolean wasPaused =
        cl_freezeDemo != NULL && cl_freezeDemo->integer != 0
            ? qtrue : qfalse;
    const int32_t visibleTime = cl.serverTime;

    coduomp_demo_set_paused(qtrue);
    MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
    coduomp_demoPlaybackState.startTime = clc.timeDemoBaseTime;
    coduomp_demoPlaybackState.timelineScanning = qtrue;
    coduomp_demoPlaybackState.timelineScanEnded = qfalse;

    while (clc.demoFile != 0 && cls.state == CA_ACTIVE &&
           coduomp_demoPlaybackState.timelineScanEnded == qfalse) {
        CL_ReadDemoMessage();
    }

    coduomp_demoPlaybackState.timelineScanning = qfalse;
    if (clc.demoFile == 0 || cl.snap.valid == qfalse)
        return qfalse;

    coduomp_demoPlaybackState.endTime = cl.snap.serverTime;
    if (coduomp_demoPlaybackState.endTime <
        coduomp_demoPlaybackState.startTime) {
        coduomp_demoPlaybackState.endTime =
            coduomp_demoPlaybackState.startTime;
    }
    coduomp_demoPlaybackState.timelineKnown = qtrue;

    int32_t restoredTime = visibleTime;
    if (restoredTime < coduomp_demoPlaybackState.startTime)
        restoredTime = coduomp_demoPlaybackState.startTime;
    else if (restoredTime > coduomp_demoPlaybackState.endTime)
        restoredTime = coduomp_demoPlaybackState.endTime;

    if (coduomp_demo_replay_to_time(restoredTime) == qfalse) {
        coduomp_demoPlaybackState.timelineKnown = qfalse;
        return qfalse;
    }

    MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
    coduomp_demo_set_paused(wasPaused);
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: index the active demo before screen rendering so
 * the one-time cgame rebuild cannot invalidate an in-progress render frame. */
void coduomp_DemoPlaybackUpdate(void)
{
    if (Cvar_VariableIntegerValue("cl_demoControlOverlay") == 0 ||
        (cl_timedemo != NULL && cl_timedemo->integer != 0) ||
        (cl_avidemo != NULL && cl_avidemo->integer != 0) ||
        (cl_forceavidemo != NULL && cl_forceavidemo->integer != 0)) {
        return;
    }
    (void)coduomp_demo_ensure_timeline();
}

/* NOT_FROM_ORIGINAL_SOURCE: seek to an indexed absolute timestamp while
 * preserving whether playback was paused before the operation. */
static qboolean coduomp_demo_seek_absolute(int32_t targetTime)
{
    if (coduomp_demo_ensure_timeline() == qfalse) {
        Com_Printf("Demo playback controls require an active demo.\n");
        return qfalse;
    }

    coduomp_demo_prepare_manual_control();
    const qboolean wasPaused =
        cl_freezeDemo != NULL && cl_freezeDemo->integer != 0
            ? qtrue : qfalse;

    if (targetTime < coduomp_demoPlaybackState.startTime)
        targetTime = coduomp_demoPlaybackState.startTime;
    else if (targetTime > coduomp_demoPlaybackState.endTime)
        targetTime = coduomp_demoPlaybackState.endTime;

    const qboolean seekCompleted = targetTime < cl.serverTime
        ? coduomp_demo_replay_to_time(targetTime)
        : coduomp_demo_advance_to_time(targetTime);
    if (seekCompleted != qfalse) {
        MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
        coduomp_demo_set_paused(wasPaused);
    }
    return seekCompleted;
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

    int64_t targetTime = (int64_t)cl.serverTime + (int64_t)deltaMsec;
    if (targetTime < INT32_MIN)
        targetTime = INT32_MIN;
    else if (targetTime > INT32_MAX)
        targetTime = INT32_MAX;

    (void)coduomp_demo_seek_absolute((int32_t)targetTime);
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

/* NOT_FROM_ORIGINAL_SOURCE: cycle the active demo through useful analysis
 * speeds while leaving the global timescale cvar untouched. */
void coduomp_DemoFastForward_f(void)
{
    if (coduomp_demo_controls_available() == qfalse) {
        Com_Printf("Demo playback controls require an active demo.\n");
        return;
    }

    coduomp_demo_prepare_manual_control();
    const int32_t currentSpeed = coduomp_demo_playback_speed();
    int32_t nextSpeed;
    if (currentSpeed < 2)
        nextSpeed = 2;
    else if (currentSpeed < 4)
        nextSpeed = 4;
    else if (currentSpeed < CODUOMP_DEMO_PLAYBACK_SPEED_MAX)
        nextSpeed = CODUOMP_DEMO_PLAYBACK_SPEED_MAX;
    else
        nextSpeed = 1;

    (void)Cvar_Set2(
        "cl_demoPlaybackSpeed", va("%i", nextSpeed), qtrue);
}

/* NOT_FROM_ORIGINAL_SOURCE: begin relative mouse dragging at the currently
 * displayed timeline position and freeze playback until release. */
static void coduomp_demo_begin_scrub(void)
{
    if (coduomp_demo_ensure_timeline() == qfalse)
        return;

    const int64_t duration =
        (int64_t)coduomp_demoPlaybackState.endTime -
        (int64_t)coduomp_demoPlaybackState.startTime;
    coduomp_demoPlaybackState.scrubFraction = duration > 0
        ? (double)((int64_t)cl.serverTime -
                   (int64_t)coduomp_demoPlaybackState.startTime) /
              (double)duration
        : 0.0;
    if (coduomp_demoPlaybackState.scrubFraction < 0.0)
        coduomp_demoPlaybackState.scrubFraction = 0.0;
    else if (coduomp_demoPlaybackState.scrubFraction > 1.0)
        coduomp_demoPlaybackState.scrubFraction = 1.0;

    coduomp_demoPlaybackState.resumeAfterScrub =
        cl_freezeDemo == NULL || cl_freezeDemo->integer == 0
            ? qtrue : qfalse;
    coduomp_demoPlaybackState.scrubActive = qtrue;
    coduomp_demo_set_paused(qtrue);
}

/* NOT_FROM_ORIGINAL_SOURCE: convert the dragged timeline thumb to an exact
 * demo timestamp, seek on release, and restore the prior pause state. */
static void coduomp_demo_finish_scrub(void)
{
    if (coduomp_demoPlaybackState.scrubActive == qfalse)
        return;

    coduomp_demoPlaybackState.scrubActive = qfalse;
    const int64_t duration =
        (int64_t)coduomp_demoPlaybackState.endTime -
        (int64_t)coduomp_demoPlaybackState.startTime;
    const int64_t offset = (int64_t)(
        coduomp_demoPlaybackState.scrubFraction * (double)duration + 0.5);
    const int32_t targetTime = (int32_t)(
        (int64_t)coduomp_demoPlaybackState.startTime + offset);

    if (coduomp_demo_seek_absolute(targetTime) != qfalse &&
        coduomp_demoPlaybackState.resumeAfterScrub != qfalse) {
        coduomp_demo_set_paused(qfalse);
    }
    coduomp_demoPlaybackState.resumeAfterScrub = qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: discard an interrupted drag without changing the
 * current position and restore playback if it was running beforehand. */
static void coduomp_demo_cancel_scrub(void)
{
    if (coduomp_demoPlaybackState.scrubActive == qfalse)
        return;

    coduomp_demoPlaybackState.scrubActive = qfalse;
    if (coduomp_demoPlaybackState.resumeAfterScrub != qfalse)
        coduomp_demo_set_paused(qfalse);
    coduomp_demoPlaybackState.resumeAfterScrub = qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: use captured relative mouse motion to move the
 * timeline thumb while a demo scrub is active. */
qboolean coduomp_DemoPlaybackMouseEvent(int32_t deltaX, int32_t deltaY)
{
    (void)deltaY;
    if (coduomp_demoPlaybackState.scrubActive == qfalse ||
        coduomp_demo_controls_available() == qfalse ||
        cls.keyCatchers != 0) {
        return qfalse;
    }

    const int32_t videoWidth = cls.rendererConfig.vidWidth;
    if (videoWidth > 0) {
        const double physicalTimelineWidth =
            (double)videoWidth *
            ((double)CODUOMP_DEMO_TIMELINE_WIDTH / 640.0);
        coduomp_demoPlaybackState.scrubFraction +=
            (double)deltaX / physicalTimelineWidth;
        if (coduomp_demoPlaybackState.scrubFraction < 0.0)
            coduomp_demoPlaybackState.scrubFraction = 0.0;
        else if (coduomp_demoPlaybackState.scrubFraction > 1.0)
            coduomp_demoPlaybackState.scrubFraction = 1.0;
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: restore ordinary client timing and audio state
 * when demo playback begins, completes, or is disconnected manually. */
void coduomp_DemoPlaybackReset(void)
{
    if (cl_freezeDemo != NULL)
        (void)Cvar_Set2("cl_freezeDemo", "0", qtrue);
    if (cl_paused != NULL)
        (void)Cvar_Set2("cl_paused", "0", qtrue);
    if (coduomp_demoPlaybackSpeed != NULL)
        (void)Cvar_Set2("cl_demoPlaybackSpeed", "1", qtrue);
    memset(&coduomp_demoPlaybackState, 0,
           sizeof(coduomp_demoPlaybackState));
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
        CODUOMP_DEMO_KEY_FRAME_STEP,
        CODUOMP_DEMO_KEY_FAST_FORWARD,
        CODUOMP_DEMO_KEY_SCRUB
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
    else if (key == 'f')
        action = CODUOMP_DEMO_KEY_FAST_FORWARD;
    else if (key == K_MOUSE1 &&
             Cvar_VariableIntegerValue("cl_demoControlOverlay") != 0)
        action = CODUOMP_DEMO_KEY_SCRUB;
    else if (binding != NULL && Q_stricmp(binding, "demopause") == 0)
        action = CODUOMP_DEMO_KEY_PAUSE;
    else if (binding != NULL && Q_stricmp(binding, "demorewind") == 0)
        action = CODUOMP_DEMO_KEY_REWIND;
    else if (binding != NULL && Q_stricmp(binding, "demoforward") == 0)
        action = CODUOMP_DEMO_KEY_FORWARD;
    else if (binding != NULL && Q_stricmp(binding, "demoframestep") == 0)
        action = CODUOMP_DEMO_KEY_FRAME_STEP;
    else if (binding != NULL &&
             Q_stricmp(binding, "demofastforward") == 0)
        action = CODUOMP_DEMO_KEY_FAST_FORWARD;

    if (key == K_ESCAPE && down != qfalse)
        coduomp_demo_cancel_scrub();

    if (action == CODUOMP_DEMO_KEY_NONE)
        return qfalse;
    if (action == CODUOMP_DEMO_KEY_SCRUB) {
        if (down != qfalse)
            coduomp_demo_begin_scrub();
        else
            coduomp_demo_finish_scrub();
        return qtrue;
    }
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
    case CODUOMP_DEMO_KEY_FAST_FORWARD:
        coduomp_DemoFastForward_f();
        break;
    case CODUOMP_DEMO_KEY_SCRUB:
        break;
    case CODUOMP_DEMO_KEY_NONE:
        break;
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: format demo-relative time with millisecond
 * precision for frame-oriented playback analysis. */
static void coduomp_demo_format_time(int64_t milliseconds, char *text,
                                     size_t textCapacity)
{
    if (milliseconds < 0)
        milliseconds = 0;

    const int64_t totalSeconds = milliseconds / 1000;
    const int32_t millis = (int32_t)(milliseconds % 1000);
    const int32_t seconds = (int32_t)(totalSeconds % 60);
    const int32_t minutes = (int32_t)((totalSeconds / 60) % 60);
    const int64_t hours = totalSeconds / 3600;
    if (hours > 0) {
        (void)coduo_crt_snprintf(
            text, textCapacity, "%lld:%02i:%02i.%03i",
            (long long)hours, minutes, seconds, millis);
    } else {
        (void)coduo_crt_snprintf(
            text, textCapacity, "%02i:%02i.%03i",
            minutes, seconds, millis);
    }
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
    static const vec4_t timelineColor = { 0.2f, 0.2f, 0.2f, 0.9f };
    static const vec4_t progressColor = { 0.9f, 0.16f, 0.12f, 1.0f };
    static const vec4_t thumbColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    const qboolean paused =
        cl_freezeDemo != NULL && cl_freezeDemo->integer != 0
            ? qtrue : qfalse;
    char statusText[CODUOMP_DEMO_STATUS_TEXT_CAPACITY];
    (void)coduo_crt_snprintf(
        statusText, sizeof(statusText),
        paused != qfalse
            ? "DEMO PAUSED  %ix   SPACE Play   F Speed   LEFT/RIGHT 5s   . Step   MOUSE1 Scrub"
            : "DEMO  %ix   SPACE Pause   F Speed   LEFT/RIGHT 5s   . Step   MOUSE1 Scrub",
        coduomp_demo_playback_speed());

    if (clc.demoPlayback == qfalse || cls.state != CA_ACTIVE ||
        cls.keyCatchers != 0 ||
        Cvar_VariableIntegerValue("cl_demoControlOverlay") == 0) {
        return;
    }

    const float fixedAdvance = 6.0f;
    const float textWidth = (float)rendererExports.TextWidth(
        statusText, CODUOMP_DEMO_CONTROL_FONT, textScale, fixedAdvance, 0);
    const float textX = (640.0f - textWidth) * 0.5f;
    SCR_FillRect(24.0f, 426.0f, 592.0f, 52.0f, backgroundColor);
    rendererExports.TextPaint(
        textX, 441.0f, CODUOMP_DEMO_CONTROL_FONT, textScale,
        paused != qfalse ? pausedColor : playingColor, statusText,
        fixedAdvance, 0, CODUOMP_DEMO_CONTROL_TEXT_STYLE);

    if (coduomp_demoPlaybackState.timelineKnown == qfalse)
        return;

    const int64_t duration =
        (int64_t)coduomp_demoPlaybackState.endTime -
        (int64_t)coduomp_demoPlaybackState.startTime;
    double fraction;
    int64_t visibleTime;
    if (coduomp_demoPlaybackState.scrubActive != qfalse) {
        fraction = coduomp_demoPlaybackState.scrubFraction;
        visibleTime = (int64_t)(fraction * (double)duration + 0.5);
    } else {
        visibleTime = (int64_t)cl.serverTime -
                      (int64_t)coduomp_demoPlaybackState.startTime;
        fraction = duration > 0
            ? (double)visibleTime / (double)duration : 0.0;
    }
    if (fraction < 0.0)
        fraction = 0.0;
    else if (fraction > 1.0)
        fraction = 1.0;
    if (visibleTime < 0)
        visibleTime = 0;
    else if (visibleTime > duration)
        visibleTime = duration;

    const float progressWidth =
        (float)(fraction * (double)CODUOMP_DEMO_TIMELINE_WIDTH);
    SCR_FillRect(CODUOMP_DEMO_TIMELINE_X, 449.0f,
                 CODUOMP_DEMO_TIMELINE_WIDTH, 6.0f, timelineColor);
    if (progressWidth > 0.0f) {
        SCR_FillRect(CODUOMP_DEMO_TIMELINE_X, 449.0f,
                     progressWidth, 6.0f, progressColor);
    }
    SCR_FillRect(
        CODUOMP_DEMO_TIMELINE_X + progressWidth - 2.0f,
        446.0f, 4.0f, 12.0f, thumbColor);

    char currentText[CODUOMP_DEMO_TIME_TEXT_CAPACITY];
    char durationText[CODUOMP_DEMO_TIME_TEXT_CAPACITY];
    char timeText[CODUOMP_DEMO_STATUS_TEXT_CAPACITY];
    coduomp_demo_format_time(
        visibleTime, currentText, sizeof(currentText));
    coduomp_demo_format_time(
        duration, durationText, sizeof(durationText));
    (void)coduo_crt_snprintf(
        timeText, sizeof(timeText), "%s / %s",
        currentText, durationText);
    const float timeWidth = (float)rendererExports.TextWidth(
        timeText, CODUOMP_DEMO_CONTROL_FONT, textScale, fixedAdvance, 0);
    rendererExports.TextPaint(
        (640.0f - timeWidth) * 0.5f, 473.0f,
        CODUOMP_DEMO_CONTROL_FONT, textScale, playingColor, timeText,
        fixedAdvance, 0, CODUOMP_DEMO_CONTROL_TEXT_STYLE);
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

    if (clc.demoFile == 0) {
        CL_DemoCompleted();
        return;
    }
    if (FS_Read(&sequence, sizeof(sequence), clc.demoFile) !=
        (int32_t)sizeof(sequence)) {
        /* NOT_FROM_ORIGINAL_SOURCE: timeline indexing owns EOF temporarily;
         * ordinary playback retains the original completion path below. */
        if (coduomp_demoPlaybackState.timelineScanning != qfalse) {
            coduomp_demoPlaybackState.timelineScanEnded = qtrue;
            return;
        }
        CL_DemoCompleted();
        return;
    }

    clc.serverMessageSequence = sequence;
    MSG_Init(&message, messageData, sizeof(messageData));
    if (FS_Read(&message.cursize, sizeof(message.cursize), clc.demoFile) !=
            (int32_t)sizeof(message.cursize) ||
        message.cursize == CL_DEMO_STREAM_END) {
        /* NOT_FROM_ORIGINAL_SOURCE: keep the stream open so the completed
         * index can rewind it to the user's visible timestamp. */
        if (coduomp_demoPlaybackState.timelineScanning != qfalse) {
            coduomp_demoPlaybackState.timelineScanEnded = qtrue;
            return;
        }
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

/* NOT_FROM_ORIGINAL_SOURCE: normalize the optional protocol suffix once for
 * both ordinary and cached demo playback. */
static qboolean coduomp_demo_build_playback_path(
    const char *demoName, char demoFileName[CL_DEMO_FILENAME_CAPACITY],
    char path[CL_DEMO_FILENAME_CAPACITY])
{
    char extension[CL_DEMO_EXTENSION_CAPACITY];
    const int32_t extensionWritten = coduo_crt_snprintf(
        extension, sizeof(extension), ".dm_%d", CL_DEMO_PROTOCOL_VERSION);
    if (demoName == NULL || demoName[0] == '\0' || extensionWritten <= 0 ||
        extensionWritten >= (int32_t)sizeof(extension)) {
        return qfalse;
    }

    const size_t demoNameLength = strlen(demoName);
    const size_t extensionLength = (size_t)extensionWritten;
    int32_t fileNameWritten;
    if (demoNameLength >= extensionLength &&
        Q_stricmp(demoName + demoNameLength - extensionLength,
                  extension) == 0) {
        fileNameWritten = coduo_crt_snprintf(
            demoFileName, CL_DEMO_FILENAME_CAPACITY, "%s", demoName);
    } else {
        fileNameWritten = coduo_crt_snprintf(
            demoFileName, CL_DEMO_FILENAME_CAPACITY, "%s%s",
            demoName, extension);
    }
    if (fileNameWritten <= 0 ||
        fileNameWritten >= CL_DEMO_FILENAME_CAPACITY) {
        return qfalse;
    }

    const int32_t pathWritten = coduo_crt_snprintf(
        path, CL_DEMO_FILENAME_CAPACITY, "demos/%s", demoFileName);
    return pathWritten > 0 && pathWritten < CL_DEMO_FILENAME_CAPACITY
               ? qtrue : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: a cached demo opens inside the selected server
 * namespace before its first gamestate supplies the recorded checksum feed.
 * Preserve the next-message position while the filesystem is rebuilt with
 * that feed instead of leaving clc.demoFile bound to the closed handle. */
qboolean coduomp_demo_restart_cached_filesystem(int32_t checksumFeed)
{
    if (clc.demoPlayback == qfalse || clc.demoFile == 0 ||
        coduomp_server_namespace_is_active() == qfalse) {
        return qfalse;
    }

    const int32_t demoPosition = FS_FTell(clc.demoFile);
    char demoFileName[CL_DEMO_FILENAME_CAPACITY];
    char path[CL_DEMO_FILENAME_CAPACITY];
    if (demoPosition < 0 ||
        coduomp_demo_build_playback_path(
            clc.demoName, demoFileName, path) == qfalse) {
        Com_Error(ERR_DROP, "Could not preserve cached demo position");
    }

    FS_FCloseFile(clc.demoFile);
    clc.demoFile = 0;
    FS_Restart(checksumFeed);

    (void)FS_FOpenFileRead(path, &clc.demoFile, qtrue);
    if (clc.demoFile == 0 ||
        FS_Seek(clc.demoFile, demoPosition, FS_SEEK_ORIGIN_SET) != 0) {
        if (clc.demoFile != 0) {
            FS_FCloseFile(clc.demoFile);
            clc.demoFile = 0;
        }
        Com_Error(ERR_DROP, "Could not reopen cached demo %s", path);
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: switch the filesystem to the selected cached mod
 * before the demo gamestate loads its map and client module. Normal disconnect
 * teardown restores the frontend filesystem when playback ends. */
static void coduomp_demo_activate_cached_mod(
    const char *serverName, const char *modName)
{
    CL_ShutdownAll();
    Hunk_ClearToStart();
    if (coduomp_server_namespace_activate_cached(serverName) == qfalse) {
        Com_Error(ERR_DROP, "Could not activate Server Cache for %s", serverName);
    }

    Cvar_Set("fs_game", modName);
    FS_PureServerSetLoadedPaks("", "");
    FS_PureServerSetReferencedPaks("", "");
    FS_Restart(0);
    cl_connectedToPureServer = qfalse;
    CL_StartHunkUsers();
}

/* NOT_FROM_ORIGINAL_SOURCE: extract the last decimal run before the demo
 * extension as a recency fallback for files without comparable timestamps. */
static qboolean coduomp_demo_list_number_suffix(
    const char *demoFileName, int64_t *numberOut)
{
    const char *const extension = strrchr(demoFileName, '.');
    const char *const numberEnd = extension != NULL
                                      ? extension
                                      : demoFileName + strlen(demoFileName);
    const char *numberStart = numberEnd;

    while (numberStart > demoFileName &&
           isdigit((unsigned char)numberStart[-1]) != 0) {
        --numberStart;
    }
    if (numberStart == numberEnd)
        return qfalse;

    int64_t number = 0;
    for (const char *digit = numberStart; digit < numberEnd; ++digit) {
        const int32_t value = *digit - '0';
        if (number > (INT64_MAX - value) / 10) {
            number = INT64_MAX;
            break;
        }
        number = number * 10 + value;
    }
    *numberOut = number;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: keep the conventional cache-only mods prefix out
 * of the group heading so ordinary and cached copies share one mod identity. */
static const char *coduomp_demo_list_user_mod_name(const char *modName)
{
    static const char modsPrefix[] = "mods/";

    return Q_stricmpn(modName, modsPrefix, sizeof(modsPrefix) - 1u) == 0 &&
                   modName[sizeof(modsPrefix) - 1u] != '\0'
               ? modName + sizeof(modsPrefix) - 1u
               : modName;
}

/* NOT_FROM_ORIGINAL_SOURCE: grow the temporary unified demo catalog used only
 * while the listdemos command formats its sorted output. */
static qboolean coduomp_demo_list_append(
    coduomp_demo_list_t *list, const char *modName,
    const char *demoFileName, const char *sourceName,
    int64_t modificationTime)
{
    if (list->count == list->capacity) {
        const size_t newCapacity = list->capacity == 0
                                       ? 32u
                                       : list->capacity * 2u;
        if (newCapacity < list->capacity ||
            newCapacity > SIZE_MAX / sizeof(*list->entries)) {
            list->allocationFailed = qtrue;
            return qfalse;
        }
        coduomp_demo_list_entry_t *const entries =
            (coduomp_demo_list_entry_t *)realloc(
                list->entries, newCapacity * sizeof(*list->entries));
        if (entries == NULL) {
            list->allocationFailed = qtrue;
            return qfalse;
        }
        list->entries = entries;
        list->capacity = newCapacity;
    }

    coduomp_demo_list_entry_t *const entry = &list->entries[list->count++];
    Q_strncpyz(entry->modName, modName, sizeof(entry->modName));
    Q_strncpyz(entry->demoFileName, demoFileName,
               sizeof(entry->demoFileName));
    Q_strncpyz(entry->sourceName, sourceName, sizeof(entry->sourceName));
    entry->modificationTime = modificationTime;
    entry->hasNumberSuffix = coduomp_demo_list_number_suffix(
        demoFileName, &entry->numberSuffix);
    if (entry->hasNumberSuffix == qfalse)
        entry->numberSuffix = 0;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: adapt server-cache enumeration to the temporary
 * unified catalog without leaking its allocation policy into the provider. */
static qboolean coduomp_demo_list_append_cached(
    const char *modName, const char *demoFileName,
    const char *serverName, int64_t modificationTime,
    void *context)
{
    return coduomp_demo_list_append(
        (coduomp_demo_list_t *)context, modName, demoFileName,
        serverName, modificationTime);
}

/* NOT_FROM_ORIGINAL_SOURCE: obtain recency for ordinary loose recordings
 * when present, while allowing catalog and PK3 entries to use the suffix
 * fallback. */
static int64_t coduomp_demo_list_ordinary_modification_time(
    const char *demoFileName)
{
    char qpath[CL_DEMO_FILENAME_CAPACITY];
    char osPath[MAX_OSPATH];
    struct stat status;

    const int32_t written = coduo_crt_snprintf(
        qpath, sizeof(qpath), "demos/%s", demoFileName);
    if (written <= 0 || written >= (int32_t)sizeof(qpath))
        return 0;

    const char *roots[2] = {fs_homepath->string, fs_basepath->string};
    for (size_t index = 0; index < 2u; ++index) {
        if (strlen(roots[index]) + strlen(fs_currentGameDir) +
                strlen(qpath) + 4u >
            sizeof(osPath)) {
            continue;
        }
        FS_BuildOSPath(
            roots[index], fs_currentGameDir, qpath, osPath);
        if (stat(osPath, &status) == 0 && S_ISREG(status.st_mode))
            return (int64_t)status.st_mtime;
    }
    return 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: group case-insensitively by user-visible mod,
 * then order each group by timestamp and numbered-name recency. */
static int coduomp_demo_list_compare(const void *leftValue,
                                     const void *rightValue)
{
    const coduomp_demo_list_entry_t *const left =
        (const coduomp_demo_list_entry_t *)leftValue;
    const coduomp_demo_list_entry_t *const right =
        (const coduomp_demo_list_entry_t *)rightValue;
    int comparison = Q_stricmp(left->modName, right->modName);

    if (comparison != 0)
        return comparison;
    if ((left->modificationTime != 0) !=
        (right->modificationTime != 0)) {
        return left->modificationTime != 0 ? -1 : 1;
    }
    if (left->modificationTime != right->modificationTime) {
        return left->modificationTime > right->modificationTime ? -1 : 1;
    }
    if (left->hasNumberSuffix != qfalse &&
        right->hasNumberSuffix != qfalse &&
        left->numberSuffix != right->numberSuffix) {
        return left->numberSuffix > right->numberSuffix ? -1 : 1;
    }
    if ((left->hasNumberSuffix != qfalse) !=
        (right->hasNumberSuffix != qfalse)) {
        return left->hasNumberSuffix != qfalse ? -1 : 1;
    }

    comparison = Q_stricmp(right->demoFileName, left->demoFileName);
    if (comparison != 0)
        return comparison;
    comparison = Q_stricmp(left->sourceName, right->sourceName);
    if (comparison != 0)
        return comparison;
    comparison = strcmp(left->modName, right->modName);
    if (comparison != 0)
        return comparison;
    comparison = strcmp(left->demoFileName, right->demoFileName);
    return comparison != 0
               ? comparison
               : strcmp(left->sourceName, right->sourceName);
}

/* NOT_FROM_ORIGINAL_SOURCE: list ordinary demos visible in the current
 * filesystem plus every recording retained under Server Cache, grouped by
 * mod name and ordered newest-first within each group. */
void coduomp_ListDemos_f(void)
{
    if (Cmd_Argc() > 2) {
        Com_Printf("listdemos [moddir]\n");
        return;
    }

    const char *const modFilter = Cmd_Argc() == 2 ? Cmd_Argv(1) : NULL;
    coduomp_demo_list_t list = {0};
    int32_t ordinaryCount = 0;
    if (coduomp_server_namespace_is_active() == qfalse &&
        (modFilter == NULL ||
         Q_stricmp(modFilter, fs_currentGameDir) == 0)) {
        char **const ordinaryDemos = FS_ListFiles(
            "demos", "dm_3", &ordinaryCount);
        for (int32_t index = 0; index < ordinaryCount; ++index) {
            if (coduomp_demo_list_append(
                    &list,
                    coduomp_demo_list_user_mod_name(fs_currentGameDir),
                    ordinaryDemos[index],
                    "current filesystem",
                    coduomp_demo_list_ordinary_modification_time(
                        ordinaryDemos[index])) == qfalse) {
                break;
            }
        }
        FS_FreeFileList(ordinaryDemos);
    }

    if (list.allocationFailed == qfalse) {
        (void)coduomp_server_namespace_visit_cached_demos(
            modFilter, coduomp_demo_list_append_cached, &list);
    }
    if (list.allocationFailed != qfalse) {
        free(list.entries);
        Com_Printf("Could not list demos: out of memory.\n");
        return;
    }

    if (list.count > 1u) {
        qsort(list.entries, list.count, sizeof(*list.entries),
              coduomp_demo_list_compare);
    }

    Com_Printf("Available demos:\n");
    const char *previousMod = NULL;
    for (size_t index = 0; index < list.count; ++index) {
        const coduomp_demo_list_entry_t *const entry = &list.entries[index];
        if (previousMod == NULL ||
            Q_stricmp(previousMod, entry->modName) != 0) {
            Com_Printf("%s:\n", entry->modName);
            previousMod = entry->modName;
        }
        Com_Printf("  %s  (%s)\n", entry->demoFileName,
                   entry->sourceName);
    }

    const int32_t totalCount = (int32_t)list.count;
    Com_Printf("%d demo%s\n", totalCount,
               totalCount == 1 ? "" : "s");
    free(list.entries);
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
    const int32_t argumentCount = Cmd_Argc();
    /* NOT_FROM_ORIGINAL_SOURCE: extend the recovered one-name command with a
     * cached mod/demo locator and optional server disambiguator. */
    if (argumentCount < 2 || argumentCount > 4) {
        Com_Printf("playdemo <demoname>\n");
        Com_Printf("playdemo <moddir> <demoname> [server-name]\n");
        return;
    }

    if (sv_running->integer != 0) {
        Com_Printf("listen server cannot play a demo.\n");
        return;
    }

    const char *const demoArgument =
        Cmd_Argv(argumentCount == 2 ? 1 : 2);
    if (strlen(demoArgument) >= CL_DEMO_FILENAME_CAPACITY) {
        Com_Printf("Demo name is too long or invalid.\n");
        return;
    }
    char demoName[CL_DEMO_FILENAME_CAPACITY];
    Q_strncpyz(demoName, demoArgument, sizeof(demoName));
    char demoFileName[CL_DEMO_FILENAME_CAPACITY];
    char path[CL_DEMO_FILENAME_CAPACITY];
    if (coduomp_demo_build_playback_path(
            demoName, demoFileName, path) == qfalse) {
        Com_Printf("Demo name is too long or invalid.\n");
        return;
    }

    char resolvedServer[MAX_QPATH];
    char resolvedMod[FS_PACK_NAME_SIZE];
    if (argumentCount >= 3) {
        const char *const modName = Cmd_Argv(1);
        const char *const serverName =
            argumentCount == 4 ? Cmd_Argv(3) : "";
        const int32_t matchCount =
            coduomp_server_namespace_resolve_cached_demo(
                modName, demoFileName, serverName,
                resolvedServer, resolvedMod);
        if (matchCount == 0) {
            Com_Printf("No cached demo matches %s %s%s%s.\n",
                       modName, demoName,
                       serverName[0] != '\0' ? " on " : "",
                       serverName);
            return;
        }
        if (matchCount > 1) {
            Com_Printf(
                "Specify the server: playdemo %s %s \"<server-name>\"\n",
                modName, demoName);
            return;
        }
    }

    CL_Disconnect(qtrue);
    /* NOT_FROM_ORIGINAL_SOURCE: a manually frozen prior demo must not carry
     * its timing or audio pause into newly started playback. */
    coduomp_DemoPlaybackReset();
    if (argumentCount >= 3)
        coduomp_demo_activate_cached_mod(resolvedServer, resolvedMod);

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
