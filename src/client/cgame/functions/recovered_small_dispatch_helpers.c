// Source: uo_cgame_mp_x86.dll at the RVAs noted below.
// Evidence: corresponding cgame_mp/mcode/uo_cgame_mp_x86/FUN_*.mcode records.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "compat/coduo_native_x87.h"

#include <math.h>
#include <string.h>

void CG_UpdateCvars(void) /* 0x3002b260 */
{
    for (int32_t i = 0; i < CG_CVAR_TABLE_COUNT; ++i) {
        cgame_syscall(CG_CVAR_UPDATE, (intptr_t)cg_cvarTable[i].vmCvar);
    }
    cgame_compat_update_presentation_cvars();
    cgame_compat_configure_screen_scales();
}

/* The Mac CG_VoiceChatListForClient is the corresponding selector used by
 * CG_VoiceChatLocal; this Windows leaf selects one of the two voice-chat lists
 * from the client's animation/team state. */
cgVoiceChatTable_t *CG_VoiceChatListForClient(int32_t clientNum) /* 0x30039fc0 */
{
    /* The leaf itself performs the target dword IMUL lookup with no check;
     * callers own the client-number domain. */
    const clientInfo_t *state =
        cgame_compat_unchecked_clientinfo(&bgs.clientinfo[0], clientNum);
    if (state->infoValid == 0 || state->team == 1) {
        return &cg_voiceChatTables[0];
    }
    return &cg_voiceChatTables[1];
}

void CG_ResetEntity(centity_t *cent) /* 0x3003c9a0 */
{
    memcpy(&cent->currentState, &cent->nextState, sizeof(cent->nextState));
    cent->currentValid = 1;
    CG_CheckEvents(cent);
}
