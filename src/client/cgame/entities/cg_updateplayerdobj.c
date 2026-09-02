// Source: uo_cgame_mp_x86.dll 0x30034830..0x3003487f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30034830_3003487f.mcode

#include "../client_recovered.h"

/* The Mac CG_UpdatePlayerDObj follows the corresponding valid-entity, DObj-handle,
 * and player-model update path, resolving the source name. */

void CG_UpdatePlayerDObj(centity_t *cent)
{
    if (!cent->currentValid) {
        return;
    }

    intptr_t handle = cgame_syscall(CG_DOBJ_GET_HANDLE, cent->nextState.clientNum);
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    const int32_t clientNum = coduo_int32_from_bits(cent->corpseModelInfo.clientNumBits);
    if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "CG_UpdatePlayerDObj: invalid client number %i",
                  clientNum);
        return;
    }
    clientInfo_t *state = &bgs.clientinfo[clientNum];
    CG_BuildCorpseDObjModels(state, handle, &cent->corpseModelInfo, cent->corpseTagState);
}
