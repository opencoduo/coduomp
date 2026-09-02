// Source: uo_cgame_mp_x86.dll 0x3002ddf0..0x3002de88
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002ddf0_3002de88.mcode
//
// CG_BuildVoteHudStrings(void) — rebuild the four "vote" HUD display values from
// their config strings at gamestate setup. This is the sibling of
// CG_BuildTimeoutHudStrings (0x3002de90): the two batch builders are invoked
// back-to-back by the cgame gamestate/config setup routine (0x3002df30, which does
// CALL 0x3002ddf0 immediately followed by a tail-JMP to 0x3002de90). The per-field
// version of each block below lives in the config-string dispatcher
// CG_ConfigStringModified (0x30038e70): its cases 0x10 (voteTime), 0x11 (voteString),
// 0x12 (voteYes), 0x13 (voteNo) run the SAME globals, the SAME "vote string" format
// literal (0x300779cc), the SAME trap ids (6 and 0x39), and the SAME callees (Q_atoi
// 0x3005b6ce and CRT strncpy 0x3005be00). This function does all four at once.
//
// Vote cluster (cgs.vote* mirror of Q3/CoD): voteTime (absolute end time),
// voteYes / voteNo (tallies), voteString (a 256-byte display buffer whose final
// byte is explicitly cleared). The vote HUD drawer at 0x3001b7d0 early-outs on voteTime==0, computes
// remaining seconds as (voteTime - trap(6) ms)/1000, and formats voteString with
// (voteYes, voteNo).
//
// NAME REJECTED: the .mcode size-guess "ItemParse_model_origin" is wrong — there is
// no item parse, no model, and no origin vector here; the body is config-string
// parse + engine text format + fixed 255-byte string copy into a HUD buffer. Named
// by proven role (vote-HUD counterpart of the timeout builder).
//
// Instruction evidence (every behavior-affecting statement):
//
//   3002ddf0  MOV EAX,[0x30440a40]              ; EAX = cg_gameState.stringOffsets[CS_VOTE_TIME]
//   3002ddf5  LEA ECX,[EAX + 0x30442a00]        ; ECX = &cg_gameState.stringData[off]
//   3002ddfb  PUSH ECX
//   3002ddfc  CALL 0x3005b6ce                   ; EAX = Q_atoi(configString)
//   3002de01  ADD ESP,4                         ; caller-clean 1 arg
//   3002de04  TEST EAX,EAX
//   3002de06  MOV [0x30447ca0],EAX              ; cg_voteTime = parsed time
//   3002de0b  JZ 0x3002de26                     ; if 0, skip the milliseconds add
//   3002de0d  PUSH 6                            ; trap id 6 (engine milliseconds)
//   3002de0f  CALL [0x30085e9c]                 ; EAX = cgame_syscall(6)
//   3002de15  MOV ECX,[0x30447ca0]              ; ECX = cg_voteTime
//   3002de1b  ADD ESP,4                         ; caller-clean the id
//   3002de1e  ADD ECX,EAX                       ; voteTime += milliseconds
//   3002de20  MOV [0x30447ca0],ECX              ; store absolute end time
//   3002de26  MOV EDX,[0x30440a48]              ; EDX = cg_gameState.stringOffsets[CS_VOTE_YES]
//   3002de2c  LEA EAX,[EDX + 0x30442a00]        ; EAX = &cg_gameState.stringData[off]
//   3002de32  PUSH EAX                          ; (arg not cleaned until the ADD ESP,0x20 below)
//   3002de33  CALL 0x3005b6ce                   ; EAX = Q_atoi(voteYes cs)
//   3002de38  MOV ECX,[0x30440a4c]              ; ECX = cg_gameState.stringOffsets[CS_VOTE_NO]
//   3002de3e  LEA EDX,[ECX + 0x30442a00]        ; EDX = &cg_gameState.stringData[off]
//   3002de44  PUSH EDX                          ; (arg not cleaned until the ADD ESP,0x20 below)
//   3002de45  MOV [0x30447ca4],EAX              ; cg_voteYes = parsed tally
//   3002de4a  CALL 0x3005b6ce                   ; EAX = Q_atoi(voteNo cs)
//   3002de4f  MOV [0x30447ca8],EAX              ; cg_voteNo = parsed tally
//   3002de54  MOV EAX,[0x30440a44]              ; EAX = cg_gameState.stringOffsets[CS_VOTE_STRING]
//   3002de59  LEA EAX,[EAX + 0x30442a00]        ; EAX = &cg_gameState.stringData[off]
//   3002de5f  PUSH 0x300779cc                   ; "vote string" format literal
//   3002de64  PUSH EAX                          ; source config string
//   3002de65  PUSH 0x39                         ; trap id 0x39 (CS text format)
//   3002de67  CALL [0x30085e9c]                 ; EAX = cgame_syscall(0x39, src, fmt)
//   3002de6d  PUSH 0xff                         ; strncpy count = 255
//   3002de72  PUSH EAX                          ; strncpy src = formatted text
//   3002de73  PUSH 0x30447cb0                   ; strncpy dst = cg_voteString
//   3002de78  CALL 0x3005be00                   ; strncpy(cg_voteString, txt, 255)
//   3002de7d  ADD ESP,0x20                      ; caller-clean 8 dwords deferred here:
//                                               ;   2 pending Q_atoi args (0x3002de32,
//                                               ;   0x3002de44) + 3 trap + 3 strncpy
//   3002de80  MOV byte [0x30447daf],0           ; cg_voteString[255] = '\0'
//   3002de87  RET
//
// Stack note: the two middle Q_atoi calls do NOT clean their pushed arg
// individually; the compiler defers cleanup so all eight remaining dwords are
// popped together by the single ADD ESP,0x20 at 0x3002de7d. That is an ABI detail —
// the source is just three Q_atoi calls, a format trap, and a strncpy.
//
// Provisional callee decls (Q_atoi, cgame_syscall) are caller-observed and are
// superseded by each callee's own .mcode reconstruction; strncpy is the statically
// linked MSVC CRT strncpy at 0x3005be00 (dword-scan copy, no forced NUL).

#include <string.h>

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

void CG_BuildVoteHudStrings(void)
{
    /* Vote-time config string parsed to an integer time via Q_atoi. */
    cg_voteTime = coduo_crt_atoi(&cg_gameState.stringData[cg_gameState.stringOffsets[CS_VOTE_TIME]]);
    if (cg_voteTime != 0) {
        /* Non-zero vote: convert the relative time into an absolute end time by
         * adding the current engine milliseconds. (Unlike the timeout builder,
         * there is no separate "active" store here — the voteModified flag at
         * 0x30447cac is set by the dispatcher, not by this batch builder.) */
        cg_voteTime = coduo_int32_from_bits(
            (uint32_t)cg_voteTime +
            (uint32_t)cgame_syscall(CG_MILLISECONDS));
    }

    /* Vote-yes / vote-no tallies, each Q_atoi of its config string. */
    cg_voteYes = coduo_crt_atoi(&cg_gameState.stringData[cg_gameState.stringOffsets[CS_VOTE_YES]]);
    cg_voteNo = coduo_crt_atoi(&cg_gameState.stringData[cg_gameState.stringOffsets[CS_VOTE_NO]]);

    /* Format the vote-text config string through the engine, copy at most 255
     * bytes, then force the final byte of the 256-byte buffer to NUL. */
    {
        char *formatted = (char *)(intptr_t)cgame_syscall(
            CG_SE_LOCALIZE_MESSAGE,
            &cg_gameState.stringData[cg_gameState.stringOffsets[CS_VOTE_STRING]],
            "vote string");
        strncpy(cg_voteString, formatted, sizeof(cg_voteString) - 1);
    }

    cg_voteString[sizeof(cg_voteString) - 1] = '\0';
}
