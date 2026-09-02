// Source: uo_cgame_mp_x86.dll 0x30021fa0..0x30021fdd
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30021fa0_30021fdd.mcode
//
// CG_Player_DoControllers (0x30021fa0) -- the ET_PLAYER arm dispatched by
// CG_DoControllers (0x30021fe0).
//
// It (1) resolves the part's DObj into an engine handle via trap CG_DOBJ_GET_HANDLE
// (0xa5), (2) indexes the per-client model-part table bgs.clientinfo[] (base
// 0x305e1f34, stride 0x4d0) by part->currentState.clientNum(+0x94), and (3) for a LIVE table
// entry (its first dword nonzero) hands that entry -- together with the part, the
// resolved DObj handle, and the dispatcher's partBits -- to the model-part processor
// BG_Player_DoControllers (0x30005730), which lerps the entry's stored spine
// control-tag / tag_origin angles and pushes them to the engine.
//
// Naming: the .mcode size-guess "Scr_ExecEntThread" (pure 0x3d==0x3d window match)
// is REJECTED -- there is no script/thread work here; this issues DObj trap 0xa5 and
// dispatches to BG_Player_DoControllers. The Mac CG_DoControllers calls
// CG_Player_DoControllers in its player arm, and this wrapper performs the matching
// player-state lookup and BG controller call, resolving the source name.
//
// i386 register-argument client ABI (proven from the sole caller
// CG_DoControllers, 0x30021fe0, which does PUSH ECX; CALL 0x30021fa0 with
// the part inherited in the shared ESI register):
//   ESI (incoming) = part    -- centity_t *; +0x00 dobjHandle, +0x94 modelIndex.
//   [ESP+4]        = partBits -- the dispatcher's four-word DObj selection bitset,
//                               passed to BG_Player_DoControllers' tag gates.
// EDI is callee-saved (PUSH EDI / POP EDI) and reused to hold the resolved DObj
// handle across the table lookup. The trap-arg pushes are caller-cleaned (ADD ESP,8).

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

void CG_Player_DoControllers(centity_t *part, uint32_t *partBits)
{
    // 0x30021fa0 MOV EAX,[ESI]; PUSH EAX; PUSH 0xa5; CALL cgame_syscall; MOV EDI,EAX:
    // resolve this part's DObj into an engine handle (trap CG_DOBJ_GET_HANDLE).
    intptr_t dobjHandle = cgame_syscall(CG_DOBJ_GET_HANDLE, part->currentState.number);

    // 0x30021fb1 MOV EAX,[ESI+0x94]; IMUL EAX,EAX,0x4d0; ADD EAX,0x305e1f34: index the
    // per-client model-part table by part->currentState.clientNum. bgs.clientinfo[] is the
    // 0x4d0-stride array at 0x305e1f34. The record's first dword (+0x00,
    // clientInfo_t.infoValid) gates liveness.
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    const int32_t clientNum = part->currentState.clientNum;
    if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "CG_Player_DoControllers: "
                  "invalid client number %i",
                  clientNum);
        return;
    }
    clientInfo_t *clientInfo = &bgs.clientinfo[clientNum];
    // 0x30021fc2 MOV ECX,[EAX]; TEST ECX,ECX; JZ end: only a live entry is processed.
    if (clientInfo->infoValid != 0) {
        // 0x30021fcb PUSH EAX(record); MOV EAX,[ESP+0xc](partBits); PUSH EAX; MOV EAX,ESI
        // (part); CALL 0x30005730; ADD ESP,8. The original callee takes part in EAX,
        // dobjHandle in EDI, partBits as arg, and the record as its final arg; the
        // maintained shared C boundary names the equivalent owner/state values.
        BG_Player_DoControllers((void *)dobjHandle, &part->currentState, partBits, clientInfo);
    }

    // 0x30021fdb POP EDI; RET.
}
