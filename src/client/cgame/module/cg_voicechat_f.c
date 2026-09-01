// Source: uo_cgame_mp_x86.dll 0x30017780..0x30017820
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30017780_30017820.mcode
//
// CG_VoiceChat_f — the "vsay" console-command handler. It is one of the cgame
// `_f` command callbacks dispatched by the console-command table (sibling of
// CG_ShellShock_Load_f 0x30017590): it takes no parameters and reads the command
// tokens through the trap_Arg* syscalls.
//
// Behavior (proven instruction-by-instruction):
//   - If trap_Argc() is odd, do nothing (the `& 1` gate below).
//   - Otherwise decide by the local player's snapshot state whether voice chat is
//     allowed:
//       * If cg_snap is present AND ps.pmType != PM_TYPE_INTERMISSION (5) AND
//         ps.playerStateFlags bit 0x80000 is clear (a normal, live participant),
//         refuse with the localized "CGAME_NOSPECTATORVOICECHAT" message.
//       * Otherwise (no snapshot yet, or an intermission/spectating player) forward
//         the chosen voice line to the server as the console command
//         va("cmd vsay %s\n", argv[1]) through CG_SEND_CONSOLE_COMMAND (trap 0x16).
//
// Naming: the .mcode size-guess "PM_InteruptWeaponWithSprintMove" is REJECTED —
// there is no pmove/weapon-interrupt work here (no PM state, no weapon fields, no
// sprint logic). The identity is proven behaviorally: the non-team "cmd vsay %s\n"
// command string (0x300768b4), the CGAME_NOSPECTATORVOICECHAT localization key
// (0x300768c8), and the `_f`-handler shape (trap_Argc gate + trap_Argv token +
// send-console-command) match the same-module PPC bank's CG_VoiceChat_f. The team
// twin "cmd vsay_team %s\n" (0x300768a0) is CG_TeamVoiceChat_f.
//
// Machine-code notes (self-check performed against every branch/width/const):
//   0x30017780 SUB ESP,0x44                 reserve token[0x40] + /GS canary slot
//   0x30017783 MOV EAX,[0x30081650]         \ MSVC /GS prologue: snapshot the
//   0x3001778a MOV [ESP+0x44],EAX           /  __security_cookie into frame+0x40
//   0x30017788 PUSH 0xc                      trap id CG_ARGC
//   0x3001778e CALL *0x30085e9c             EAX = trap_Argc()
//   0x30017794 ADD ESP,0x4                  caller-clean the 1 pushed dword
//   0x30017797 AND EAX,0x80000001           \  compute (argc & 1) with sign
//   0x3001779c JNS +5                         canonicalization (bit31|bit0); the
//   0x3001779e DEC EAX / OR EAX,-2 / INC EAX / fixup only runs for negative argc
//   0x300177a3 JNZ 0x30017813              -> if (argc & 1) do nothing, return
//   0x300177a5 MOV EAX,[0x30459160]         EAX = cg_snap
//   0x300177aa TEST EAX,EAX / JZ 0x300177e7 -> if (!cg_snap) send-vsay path
//   0x300177ae CMP [EAX+0x10],0x5 / JZ ...  -> if (ps.pmType==5) send-vsay path
//   0x300177b4 TEST [EAX+0x18],0x80000/JNZ  -> if (flags&0x80000) send-vsay path
//   0x300177bd MOV EAX,0x30077b28           "cgame"           (domain, in EAX)
//   0x300177c2 MOV ECX,0x300768c8           "CGAME_NOSPECTATORVOICECHAT" (in ECX)
//   0x300177c7 CALL 0x3002d6e0              EAX = CG_SafeTranslateString_Internal(domain, ref)
//   0x300177cc PUSH EAX / PUSH 0x300768c4   Com_PrintMessage("%s\n", translated)
//   0x300177d2 CALL 0x3002b2b0 / ADD ESP,8
//   0x300177da .. /GS epilogue, ADD ESP,0x44 / RET
//
//   0x300177e7 PUSH 0x40 / LEA &token / PUSH / PUSH 0x1 / PUSH 0xd
//   0x300177f2 CALL *0x30085e9c            trap_Argv(1, token, 0x40)
//   0x300177f8 LEA ECX,[&token] / PUSH ECX / PUSH 0x300768b4
//   0x30017802 CALL 0x3004e8a0            EAX = va("cmd vsay %s\n", token)
//   0x30017807 PUSH EAX / PUSH 0x16
//   0x3001780a CALL *0x30085e9c            cgame_syscall(0x16, cmd)
//   0x30017810 ADD ESP,0x20               clean 4(argv)+2(va)+2(send) dwords
//   0x30017813 .. /GS epilogue, ADD ESP,0x44 / RET
//
// The SUB/MOV cookie snapshot and the reload+check epilogues (both RET paths call
// __security_check_cookie at 0x30061639 with the reloaded canary in ECX) are
// compiler-generated MSVC /GS stack-protector code, not source statements; they
// are emitted because this function owns the 0x40-byte `token` stack buffer.

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

void CG_VoiceChat_f(void)
{
    // 0x30017797..0x300177a3: the command argument count must be even. The
    // AND 0x80000001 + DEC/OR/INC sequence is the MSVC idiom that yields (argc & 1)
    // for a possibly-signed argc; a nonzero (odd) result returns without acting.
    // Unlike the team twin's exact argc == 2 gate, this preserves the original
    // even-count acceptance and silently ignores tokens after argv[1].
    if (trap_Argc() & 1) {
        return;
    }

    // 0x300177a5..0x300177bb: a non-intermission spectator (snapshot present but
    // not PSF_ACTIVE_PLAYER) is refused voice chat. An active player, intermission,
    // or missing snapshot falls through to the send path below.
    snapshot_t *snap = cg_snap;
    if (snap != NULL &&
        snap->ps.pmType != PM_TYPE_INTERMISSION &&
        (snap->ps.playerStateFlags & PSF_ACTIVE_PLAYER) == 0) {
        // 0x300177bd..0x300177d7: Com_PrintMessage("%s\n", localized text). The
        // translator takes (domain, reference) in (EAX, ECX) at the call site.
        Com_PrintMessage("%s\n", CG_SafeTranslateString_Internal("cgame", "CGAME_NOSPECTATORVOICECHAT"));
        return;
    }

    // 0x300177e7..0x30017810: copy the voice-line token (argv[1]) into a 64-byte
    // buffer and forward it to the server as a console command.
    char token[64];

    trap_Argv(1, token, (int32_t)sizeof(token));
    cgame_syscall(CG_SEND_CONSOLE_COMMAND,
                  (intptr_t)va("cmd vsay %s\n", token));
}
