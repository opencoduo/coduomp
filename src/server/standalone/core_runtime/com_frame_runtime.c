#define _POSIX_C_SOURCE 200809L

#include <setjmp.h>
#include <stdint.h>

#include "qcommon/com_frame.h"
#include "core_runtime_private.h"
#include "../core_cvar/cvar_private.h"
#include "qcommon/q_command.h"
#include "../core_math/core_math_private.h"
#include "scripting/script_anim.h"
#include "../server/server_private.h"

#define COM_FRAME_MSEC_BASE 1000
#define COM_FRAME_DEFAULT_MSEC 1
#define COM_FRAME_LONGJMP_SAVE_MASK 0

qboolean com_configAutowriteEnabled;
int32_t com_frameLastTime;
coduo_jump_buffer_t com_frameAbortContext;

void Com_Frame(void)
{
    int32_t frameStartTime = 0;
    int32_t eventTime = 0;
    int32_t serverTime = 0;
    int32_t clientStartTime = 0;
    int32_t frameEndTime = 0;
    int32_t rawMsec;
    int32_t scaledMsec;
    int32_t minMsec;

    if (CODUO_SETJMP(com_frameAbortContext, COM_FRAME_LONGJMP_SAVE_MASK) != 0) {
        Com_ErrorCleanup();
        return;
    }

    Com_WriteConfiguration();
    if (host_cvar_viewlog_pointer_slot->modified != 0) {
        if (dedicated->value == 0.0f) {
            Sys_ShowConsole(host_cvar_viewlog_pointer_slot->integer, qfalse);
        }
        host_cvar_viewlog_pointer_slot->modified = qfalse;
    }

    SetAnimCheck(host_cvar_com_animCheck_pointer_slot->integer);

    if (com_speeds->integer != 0) {
        frameStartTime = Sys_Milliseconds();
    }

    minMsec = COM_FRAME_DEFAULT_MSEC;
    if (host_cvar_com_maxfps_pointer_slot->integer > 0 && dedicated->integer == 0) {
        minMsec = COM_FRAME_MSEC_BASE / host_cvar_com_maxfps_pointer_slot->integer;
    }

    do {
        com_frameTime = Com_EventLoop();
        if (com_frameTime < com_frameLastTime) {
            com_frameLastTime = com_frameTime;
        }
        rawMsec = com_frameTime - com_frameLastTime;
    } while (rawMsec < minMsec);

    Cbuf_Execute();
    com_frameLastTime = com_frameTime;

    rawMsec = Com_ClampMsec(rawMsec);
    scaledMsec = Com_ModifyMsec(rawMsec);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (com_speeds->integer != 0) {
        eventTime = Sys_Milliseconds();
    }

    SV_Frame(rawMsec);
    if (com_speeds->integer != 0) {
        serverTime = Sys_Milliseconds();
    }

    if (dedicated->integer == 0) {
        Com_EventLoop();
        Cbuf_Execute();
        if (com_speeds->integer != 0) {
            clientStartTime = Sys_Milliseconds();
        }
        CL_Frame(rawMsec, scaledMsec);
        if (com_speeds->integer != 0) {
            frameEndTime = Sys_Milliseconds();
        }
    } else if (com_speeds->integer != 0) {
        frameEndTime = Sys_Milliseconds();
        clientStartTime = frameEndTime;
    }

    if (com_speeds->integer != 0) {
        Com_Printf("frame:%i all:%3i sv:%3i ev:%3i cl:%3i gm:%3i rf:%3i "
                   "bk:%3i\n",
                   com_frameNumber, frameEndTime - eventTime, (serverTime - eventTime) - com_timeGame,
                   ((eventTime - frameStartTime) + clientStartTime) - serverTime,
                   (frameEndTime - clientStartTime) - (com_timeFrontend + com_timeBackend), com_timeGame, com_timeFrontend,
                   com_timeBackend);
    }

    ++com_frameNumber;
}
