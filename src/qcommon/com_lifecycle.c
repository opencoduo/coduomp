#include "com_lifecycle.h"

#include "com_startup_commands.h"
#include "animation/dobj.h"
#include "filesystem/filesystem.h"
#include "hunk.h"
#include "q_shared_types.h"
#include "scripting/script_runtime_state.h"
#include "animation/xanim.h"

void CL_Disconnect(qboolean showMainMenu);
void CL_ShutdownAll(void);
void CL_StartHunkUsers(void);
void SV_Shutdown(const char *finalMessage);

/*
 * Complete common engine shutdown/close lifecycle.  The original bodies are:
 *
 * Function       Windows       Linux        supporting Mac client
 * Com_Shutdown   0x00439da0    0x080702fc   0x10046de0
 * Com_Close      0x0043c9f0    0x080720e4   0x10043920
 *
 * Windows inlines portions of CL_ShutdownAll, Hunk_ClearToStart,
 * Com_ShutdownDObj, and Scr_Shutdown.  Linux retains the corresponding calls.
 * The Mac client exposes the canonical function and callee names and retains
 * the same source-level order.  No target-specific operation remains in this
 * boundary.
 */

void Com_Shutdown(const char *finalMessage)
{
    CL_Disconnect(qtrue);
    CL_ShutdownAll();
    SV_Shutdown(finalMessage);
    Hunk_ClearToStart();
    CL_StartHunkUsers();
}

void Com_Close(void)
{
    Com_ShutdownDObj();
    DObjShutdown();
    XAnimShutdown();
    Scr_Shutdown();

    if (com_consoleLogFile != 0) {
        FS_FCloseFile(com_consoleLogFile);
        com_consoleLogFile = 0;
    }

    if (com_journalFile != 0) {
        FS_FCloseFile(com_journalFile);
        com_journalFile = 0;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (com_journalDataFile != 0) {
        FS_FCloseFile(com_journalDataFile);
        com_journalDataFile = 0;
    }
}
