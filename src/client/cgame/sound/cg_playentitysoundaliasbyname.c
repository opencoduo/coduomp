// Source: uo_cgame_mp_x86.dll 0x3002ca50..0x3002ca72
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002ca50_3002ca72.mcode
//
// CG_PlayEntitySoundAliasByName — play a sound by config-string name on a specific client's
// entity origin. A thin wrapper over CG_PlaySoundAliasByName (0x3002ca80): it forms
// &cg_entities[clientNum].currentStatePos.trBase (cg_entities base 0x3048c6e0,
// field +0x18, stride 0x288; therefore absolute base 0x3048c6f8) and forwards
//
//     CG_PlaySoundAliasByName(&cg_entities[clientNum].currentStatePos.trBase,
//                        soundName, clientNum)
//
// i.e. the channel object, the sound name, and the client number as the entityNum.
// This is the per-client analogue of CG_PlayClientSoundAliasByName (0x3002ca30), which
// does the same forward for the LOCAL player using &cg_snap->ps.psOrigin and
// cg_snap->ps.psClientNum. Same tail-forward shape is used by the sibling wrappers at
// 0x300226b0/0x300226e8 (tail JMP) and 0x300228b0.../0x30022900... which all form
// &cg_entities[n].currentStatePos.trBase the identical way and call CG_PlaySoundAliasByName.
//
// The mechanical size-guess name "Use_Item" (win size 0x22 matched some 0x22 PPC
// function) has ZERO behavioral basis and is rejected: the body performs no item
// use — it only computes a sound-channel address and tail-forwards to the
// local-sound starter.
//
// Register/stack trace:
//   3002ca50 PUSH ECX                      ; 4-byte scratch reservation (frame align)
//   3002ca51 MOV EAX,[ESP+8]               ; EAX = arg1 clientNum (orig [ESP+4])
//   3002ca55 MOV ECX,EAX                   ; ECX = clientNum
//   3002ca57 IMUL ECX,ECX,0x288            ; ECX = clientNum * 0x288 (element stride)
//   3002ca5d PUSH EAX                       ; stack arg = clientNum (entityNum)
//   3002ca5e MOV EAX,[ESP+0x10]            ; EAX = arg2 soundName (orig [ESP+8])
//   3002ca62 ADD ECX,0x3048c6f8            ; ECX = entity currentStatePos.trBase
//   3002ca68 CALL 0x3002ca80              ; CG_PlaySoundAliasByName(ECX=channelObj,
//                                          ;   EAX=soundName, [stack]=entityNum)
//   3002ca6d ADD ESP,4                      ; balance the pushed entityNum
//   3002ca70 POP ECX                        ; undo the scratch reservation
//   3002ca71 RET                            ; caller balances clientNum/soundName (cdecl)
//
// ABI note: CG_PlaySoundAliasByName receives channelObj in ECX and soundName in EAX
// (custom register ABI, proven in FUN_3002ca80_3002cb40.c) with entityNum as its
// single pushed stack argument; the callee's .c models this as a plain 3-arg
// (channelObj, soundName, entityNum) signature, so the source forward is a plain
// call. This function returns whatever CG_PlaySoundAliasByName returns in EAX (the
// started-sound number, 0 on failure).

#include "../client_recovered.h"
#include "../globals.h"

void CG_PlayEntitySoundAliasByName(int clientNum, const char *soundName)
{
    /* 3002ca57/3002ca62: base 0x3048c6f8 is cg_entities+0x18; the client number
     * is reused as the entityNum argument. */
    (void)CG_PlaySoundAliasByName(clientNum, &cg_entities[clientNum].currentState.pos.trBase, soundName);
}
