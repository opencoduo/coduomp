// Source: uo_cgame_mp_x86.dll 0x3002ca30..0x3002ca4c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002ca30_3002ca4c.mcode
//
// CG_PlayClientSoundAliasByName — convenience wrapper that plays a sound on the local
// player's own snapshot sound channel. It reads the current snapshot pointer
// cg_snap (0x30459160) and forwards to CG_PlaySoundAliasByName with that player's
// channel object and client number, letting callers supply just the sound to play:
//
//     CG_PlaySoundAliasByName(&cg_snap->ps.psOrigin, sound, cg_snap->ps.psClientNum)
//
// This is the exact call shape the header already documents at the CG_LocalSound /
// CG_LocalSound_f handlers and other local-sound sites; this function is the shared
// entry those call sites reach (proven callers at 0x3001a158, 0x3003adaf, 0x3003adf2,
// 0x3003ae6e each PUSH one arg, CALL 0x3002ca30, ADD ESP,4 — plain cdecl, one arg).
//
// The mechanical size-guess name "CG_SetDObjInfo" (win size 0x1c matched some 0x1c
// PPC function) has ZERO behavioral basis and is rejected: the body touches only
// cg_snap and tail-forwards to the local-sound starter — there is no DObj work here.
//
// Register/stack trace:
//   3002ca30 MOV EAX,[0x30459160]          ; EAX = cg_snap
//   3002ca35 LEA ECX,[EAX+0x20]            ; ECX = &cg_snap->ps.psOrigin (channelObj/this)
//   3002ca38 MOV EAX,[EAX+0xe0]            ; EAX = cg_snap->ps.psClientNum
//   3002ca3e PUSH EAX                      ; stack arg = clientNum (entityNum)
//   3002ca3f MOV EAX,[ESP+8]               ; EAX = this function's incoming arg (sound)
//   3002ca43 CALL 0x3002ca80              ; CG_PlaySoundAliasByName(channelObj, sound, clientNum)
//   3002ca48 ADD ESP,4                     ; balance the pushed clientNum (callee did not)
//   3002ca4b RET                           ; caller balances the `sound` arg (cdecl)
//
// ABI note: the callee CG_PlaySoundAliasByName receives channelObj in ECX and sound in
// EAX (custom register ABI, proven in FUN_3002ca80_3002cb40.c), and clientNum as its
// single stack argument. This wrapper supplies all three from cg_snap plus its own
// forwarded `sound` argument.

#include "../client_recovered.h"
#include "../globals.h"

void CG_PlayClientSoundAliasByName(const char *sound)
{
    /* 3002ca35 / 3002ca38: the local player's own sound channel object and client
     * number come straight out of the current snapshot. */
    (void)CG_PlaySoundAliasByName(cg_snap->ps.psClientNum,
                                  &cg_snap->ps.psOrigin, sound);
}
