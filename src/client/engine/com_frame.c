#include "q_shared.h"

#include "client/cgame.h"
#include "scripting/script_runtime.h"
#include "server/server.h"
#include "system_console.h"
#include "system_process_lock.h"

#include <setjmp.h>

#if defined(_WIN32)
#include <windows.h>
#endif

enum {
    COM_STATMON_FPS_ENTRY = 0,
    COM_STATMON_FILE_ENTRY = 1,
    COM_STATMON_WARNING_DURATION_MSEC = 3000,
    COM_STATMON_SLOW_FRAME_MSEC = 33,
    COM_CONSOLE_VISIBLE = 1,
    COM_FRAME_MINIMUM_MSEC = 1,
    CODUOMP_FRAME_PACING_MAX_FPS = 1000,
    CODUOMP_FRAME_PACING_CORRECTION_DIVISOR = 4
};

#define CODUOMP_NANOSECONDS_PER_SECOND UINT64_C(1000000000)
#define CODUOMP_FRAME_PACING_MAX_CORRECTION_NSEC UINT64_C(1000000)

/* Original common-frame bookkeeping. These are private to Com_Frame in the
 * Windows executable; addresses are retained only as binary evidence. */
static int32_t comPreviousEventTime;        /* original 0x00980230 */
static int32_t comPreviousStatmonFrameTime; /* original 0x00981e80 */
static int32_t comFrameNumber;              /* original 0x049290a0 */

/* NOT_FROM_ORIGINAL_SOURCE: state for the opt-in improved-client frame-start
 * scheduler. None of these values enter the engine's gameplay time domain. */
typedef struct {
    qboolean initialized;
    int32_t maxFps;
    uint64_t nextIdealNanoseconds;
    uint64_t lastStartNanoseconds;
    uint64_t periodNanoseconds;
    uint64_t periodRemainder;
    uint64_t remainderAccumulator;
} coduomp_frame_pacing_state_t;

static coduomp_frame_pacing_state_t coduompFramePacing;

/* NOT_FROM_ORIGINAL_SOURCE: abandon all phase history when precise pacing is
 * disabled so re-enabling it cannot repay stale timing debt. */
static void coduomp_frame_pacing_disable(void)
{
    coduompFramePacing.initialized = qfalse;
    coduompFramePacing.maxFps = 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: distribute the fractional nanoseconds in 1/fps
 * without allowing integer division to bias the long-term frame period. */
static uint64_t coduomp_frame_pacing_period_step(void)
{
    uint64_t interval = coduompFramePacing.periodNanoseconds;

    coduompFramePacing.remainderAccumulator +=
        coduompFramePacing.periodRemainder;
    if (coduompFramePacing.remainderAccumulator >=
        (uint64_t)coduompFramePacing.maxFps) {
        coduompFramePacing.remainderAccumulator -=
            (uint64_t)coduompFramePacing.maxFps;
        ++interval;
    }

    return interval;
}

/* NOT_FROM_ORIGINAL_SOURCE: choose this frame's start boundary. Recovery from
 * a late frame is limited to one millisecond or one quarter of the requested
 * period, whichever is smaller, preventing a burst of near-zero frames. */
static uint64_t coduomp_frame_pacing_prepare(int32_t maxFps,
                                             uint64_t nowNanoseconds)
{
    if (coduompFramePacing.initialized == qfalse ||
        coduompFramePacing.maxFps != maxFps ||
        nowNanoseconds < coduompFramePacing.lastStartNanoseconds) {
        coduompFramePacing.initialized = qtrue;
        coduompFramePacing.maxFps = maxFps;
        coduompFramePacing.periodNanoseconds =
            CODUOMP_NANOSECONDS_PER_SECOND / (uint64_t)maxFps;
        coduompFramePacing.periodRemainder =
            CODUOMP_NANOSECONDS_PER_SECOND % (uint64_t)maxFps;
        coduompFramePacing.remainderAccumulator = 0;
        coduompFramePacing.nextIdealNanoseconds = nowNanoseconds;
        coduompFramePacing.lastStartNanoseconds = nowNanoseconds;
        return nowNanoseconds;
    }

    uint64_t correctionLimit =
        coduompFramePacing.periodNanoseconds /
        CODUOMP_FRAME_PACING_CORRECTION_DIVISOR;
    if (correctionLimit > CODUOMP_FRAME_PACING_MAX_CORRECTION_NSEC)
        correctionLimit = CODUOMP_FRAME_PACING_MAX_CORRECTION_NSEC;

    const uint64_t earliestStart =
        coduompFramePacing.lastStartNanoseconds +
        coduompFramePacing.periodNanoseconds - correctionLimit;

    if (coduompFramePacing.nextIdealNanoseconds < earliestStart)
        return earliestStart;
    return coduompFramePacing.nextIdealNanoseconds;
}

/* NOT_FROM_ORIGINAL_SOURCE: advance the ideal schedule after an actual frame
 * start. Missing a full period discards accumulated debt rather than trying to
 * catch up, while smaller misses remain eligible for bounded phase recovery. */
static void coduomp_frame_pacing_accept(uint64_t nowNanoseconds)
{
    if (nowNanoseconds < coduompFramePacing.lastStartNanoseconds) {
        coduomp_frame_pacing_disable();
        return;
    }

    const uint64_t lateness =
        nowNanoseconds > coduompFramePacing.nextIdealNanoseconds
            ? nowNanoseconds - coduompFramePacing.nextIdealNanoseconds
            : 0;

    coduompFramePacing.lastStartNanoseconds = nowNanoseconds;
    if (lateness >= coduompFramePacing.periodNanoseconds) {
        coduompFramePacing.remainderAccumulator = 0;
        coduompFramePacing.nextIdealNanoseconds =
            nowNanoseconds + coduomp_frame_pacing_period_step();
        return;
    }

    coduompFramePacing.nextIdealNanoseconds +=
        coduomp_frame_pacing_period_step();
}

/* Source: CoDUOMP.exe 0x0043c580..0x0043c9e5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043c580_0043c9e6.mcode.
 * Name and source-level boundaries: exact same-module Mac symbol Com_Frame.
 * The Windows optimizer inlines Com_WriteConfiguration, the millisecond clock,
 * and the dedicated-mode Sys_ShowConsole call; the maintained source uses
 * their recovered APIs. */
void Com_Frame(void)
{
    int32_t frameStartTime = 0;
    int32_t serverStartTime = 0;
    int32_t clientStartTime = 0;
    int32_t frameEndTime = 0;
    int32_t postServerTime = 0;
    int32_t minimumMsec = COM_FRAME_MINIMUM_MSEC;
    int32_t msec;
    qboolean preciseFramePacing = qfalse;
    uint64_t preciseFrameTarget = 0;
    uint64_t preciseFrameNow = 0;

    if (setjmp(com_abortFrame) != 0) {
        Com_ErrorCleanup();
        return;
    }

    Com_WriteConfiguration();

    if (com_statmon->integer != 0 && fs_fileAccessed != qfalse) {
        StatMon_Warning(COM_STATMON_FILE_ENTRY,
                        COM_STATMON_WARNING_DURATION_MSEC,
                        "gfx/2d/warning@file.jpg");
        fs_fileAccessed = qfalse;
    }

    if (com_viewlog->modified != qfalse) {
        if (dedicated->value == 0.0f)
            Sys_ShowConsole(com_viewlog->integer, qfalse);
        com_viewlog->modified = qfalse;
    }

    SetAnimCheck(com_animCheck->integer);

    if (com_speeds->integer != 0)
        frameStartTime = (int32_t)Sys_Milliseconds();

    if (com_preciseFramePacing->integer != 0 &&
        com_maxfps->integer > 0 &&
        com_maxfps->integer <= CODUOMP_FRAME_PACING_MAX_FPS &&
        dedicated->integer == 0) {
        preciseFramePacing = qtrue;
        preciseFrameNow = coduomp_monotonic_nanoseconds();
        preciseFrameTarget = coduomp_frame_pacing_prepare(
            com_maxfps->integer, preciseFrameNow);
    } else {
        coduomp_frame_pacing_disable();
        if (com_maxfps->integer > 0 && dedicated->integer == 0)
            minimumMsec = 1000 / com_maxfps->integer;
    }

    do {
        com_frameTime = Com_EventLoop();
        if (comPreviousEventTime > com_frameTime)
            comPreviousEventTime = com_frameTime;
        msec = com_frameTime - comPreviousEventTime;
        if (preciseFramePacing != qfalse)
            preciseFrameNow = coduomp_monotonic_nanoseconds();
    } while (preciseFramePacing != qfalse
                 ? (preciseFrameNow < preciseFrameTarget ||
                    msec < COM_FRAME_MINIMUM_MSEC)
                 : msec < minimumMsec);

    if (preciseFramePacing != qfalse)
        coduomp_frame_pacing_accept(preciseFrameNow);

    Cbuf_Execute();
    /* NOT_FROM_ORIGINAL_SOURCE: a server-selected mod can be torn down
     * by a command executed above. Finish its frontend restart only after the
     * stock command tokenizer's global argument storage is no longer live. */
    coduomp_client_complete_server_mod_teardown();
    comPreviousEventTime = com_frameTime;

    const int32_t realMsec = Com_ClampMsec(msec);
    const int32_t scaledMsec = Com_ModifyMsec(realMsec);

    if (com_speeds->integer != 0)
        serverStartTime = (int32_t)Sys_Milliseconds();

    SV_Frame(realMsec);

    if (dedicated->modified != qfalse) {
        const int32_t previousDedicated = dedicated->integer;
        (void)Cvar_Get("dedicated", "0", 0);
        dedicated->modified = qfalse;

        if (dedicated->integer == 0) {
            qboolean processLockAvailable = qtrue;
#if defined(_WIN32)
            processLockAvailable = Sys_CheckProcessLock();
#endif
            if (processLockAvailable == qfalse) {
                Com_Printf(
                    "cannot become non-dedicated, since a non-dedicated "
                    "game is already running\n");
                (void)Cvar_Set2("dedicated",
                                va("%i", previousDedicated), qtrue);
                (void)Cvar_Get("dedicated", "1", 0);
                dedicated->modified = qfalse;
            } else {
                SV_RemoveDedicatedCommands();
                CL_Init();
                CL_StartHunkUsers();
                Sys_ShowConsole(com_viewlog->integer, qfalse);
            }
        } else {
            CL_Shutdown();
            Sys_ShowConsole(COM_CONSOLE_VISIBLE, qtrue);
#if defined(_WIN32)
            Sys_DeleteProcessLockFile();
#endif
            Cmd_AddCommand("say", SV_ConSay_f);
            Cmd_AddCommand("tell", SV_ConTell_f);
        }
    }

    if (com_speeds->integer != 0)
        postServerTime = (int32_t)Sys_Milliseconds();

    if (dedicated->integer == 0) {
        (void)Com_EventLoop();
        Cbuf_Execute();
        coduomp_client_complete_server_mod_teardown();

        if (com_speeds->integer != 0)
            clientStartTime = (int32_t)Sys_Milliseconds();

        CL_Frame(scaledMsec, realMsec);

        if (com_statmon->integer != 0 ||
            com_speeds->integer != 0) {
            const int32_t previousFrameTime =
                comPreviousStatmonFrameTime;
            const int32_t currentFrameTime =
                (int32_t)Sys_Milliseconds();
            comPreviousStatmonFrameTime = currentFrameTime;

            if (com_statmon->integer != 0 &&
                currentFrameTime - previousFrameTime >
                    COM_STATMON_SLOW_FRAME_MSEC &&
                previousFrameTime != 0) {
                StatMon_Warning(COM_STATMON_FPS_ENTRY,
                                COM_STATMON_WARNING_DURATION_MSEC,
                                "gfx/2d/warning@fps.jpg");
            }

            if (com_speeds->integer != 0)
                frameEndTime = currentFrameTime;
        }
    } else if (com_speeds->integer != 0) {
        frameEndTime = (int32_t)Sys_Milliseconds();
        clientStartTime = frameEndTime;
    }

    if (com_speeds->integer != 0) {
        /* The three renderer/game counters are subtracted from their owning
         * frame phases exactly as in the original instruction sequence. */
        Com_Printf(
            "frame:%i all:%3i sv:%3i ev:%3i cl:%3i "
            "gm:%3i rf:%3i bk:%3i\n",
            comFrameNumber,
            frameEndTime - serverStartTime,
            postServerTime - com_timeGame - serverStartTime,
            clientStartTime - postServerTime -
                frameStartTime + serverStartTime,
            frameEndTime - com_timeBackend -
                com_timeFrontend - clientStartTime,
            com_timeGame,
            com_timeFrontend,
            com_timeBackend);
    }

    ++comFrameNumber;
}
