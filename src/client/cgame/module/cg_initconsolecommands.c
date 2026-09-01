// FUN_30017920_30017a99  (0x30017920..0x30017a99)  uo_cgame_mp_x86.dll
//
// CG_InitConsoleCommands — register every cgame console command with the engine.
//
// Two phases, both issuing the CG_ADD_COMMAND trap (0x17) with a single command
// name string per call:
//   1. Walk cg_consoleCommands[] (the dispatch table at 0x30071780) and register
//      each entry's .name, advancing by the 8-byte {name, function} stride until a
//      NULL name terminates the table. The loop is guarded by an initial NULL-name
//      test, so an empty table registers nothing in this phase.
//   2. Register a fixed set of ~25 additional command names that the client does
//      not itself handle but forwards to / shares with the server (killcam,
//      matchtimein, matchtimeout, follow*/vote/callvote, team, voice, tell, say*,
//      stats, jumptonode, setviewpos, levelshot, ufo, noclip, notarget, god, take,
//      give, kill). Registering them makes them appear in the console/autocomplete.
//
// The .mcode size-guess "PlayerCmd_getFractionMaxAmmo" is a byte-size collision
// with zero behavioral basis and is rejected — this body reads no player command
// and touches no ammo; it registers console commands.
//
// Machine code (abridged; the 25 explicit registrations are all the same shape):
//   30017920  MOV EAX,[0x30071780]         ; EAX = cg_consoleCommands[0].name
//   30017925  TEST EAX,EAX / JZ 0x30017947 ; empty table? skip phase 1
//   3001792a  MOV ESI,0x30071780           ; ESI = &cg_consoleCommands[0]
//   30017930  PUSH EAX                     ; PUSH name
//   30017931  PUSH 0x17                    ; trap id 0x17 = CG_ADD_COMMAND
//   30017933  CALL [0x30085e9c]            ; cgame_syscall(CG_ADD_COMMAND, name)
//   30017939  MOV EAX,[ESI+0x8]            ; EAX = next entry .name
//   3001793c  ADD ESI,0x8                  ; advance one consoleCommand_t
//   3001793f  ADD ESP,0x8                  ; clean the 2 pushed dwords
//   30017942  TEST EAX,EAX / JNZ 0x30017930; loop while name != NULL
//   30017947  PUSH 0x30076774 ("kill")     ; --- phase 2: 25 fixed names ---
//   3001794c  PUSH 0x17 / CALL [...]       ; cgame_syscall(CG_ADD_COMMAND, "kill")
//   ...       (24 more, ADD ESP,0x40 batched every 8 calls)
//   30017a98  RET
//
// Callees:
//   [0x30085e9c] cgame_syscall (globals.h) — VM syscall vector; first vararg is
//                the trap id. Here always CG_ADD_COMMAND (0x17), second vararg the
//                command-name string. Result discarded.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_InitConsoleCommands(void)
{
    const consoleCommand_t *cmd;

    /* Phase 1 (0x30017920..0x30017946): register each table command name. The
     * NULL-name terminator ends the walk; the leading TEST short-circuits an empty
     * table. The "mr" table entry (NULL function) still has its name registered. */
    for (cmd = cg_consoleCommands; cmd->name != NULL; cmd++) {
        cgame_syscall(CG_ADD_COMMAND, cmd->name);
    }

    /* Phase 2 (0x30017947..0x30017a98): register the fixed server-forwarded
     * command names, in the exact emission order from the .text (push order). */
    cgame_syscall(CG_ADD_COMMAND, "kill");
    cgame_syscall(CG_ADD_COMMAND, "give");
    cgame_syscall(CG_ADD_COMMAND, "take");
    cgame_syscall(CG_ADD_COMMAND, "god");
    cgame_syscall(CG_ADD_COMMAND, "notarget");
    cgame_syscall(CG_ADD_COMMAND, "noclip");
    cgame_syscall(CG_ADD_COMMAND, "ufo");
    cgame_syscall(CG_ADD_COMMAND, "levelshot");
    cgame_syscall(CG_ADD_COMMAND, "setviewpos");
    cgame_syscall(CG_ADD_COMMAND, "jumptonode");
    cgame_syscall(CG_ADD_COMMAND, "stats");
    cgame_syscall(CG_ADD_COMMAND, "say");
    cgame_syscall(CG_ADD_COMMAND, "say_team");
    cgame_syscall(CG_ADD_COMMAND, "say_squad");
    cgame_syscall(CG_ADD_COMMAND, "tell");
    cgame_syscall(CG_ADD_COMMAND, "voice");
    cgame_syscall(CG_ADD_COMMAND, "team");
    cgame_syscall(CG_ADD_COMMAND, "follow");
    cgame_syscall(CG_ADD_COMMAND, "callvote");
    cgame_syscall(CG_ADD_COMMAND, "vote");
    cgame_syscall(CG_ADD_COMMAND, "follownext");
    cgame_syscall(CG_ADD_COMMAND, "followprev");
    cgame_syscall(CG_ADD_COMMAND, "matchtimeout");
    cgame_syscall(CG_ADD_COMMAND, "matchtimein");
    cgame_syscall(CG_ADD_COMMAND, "killcam");
}
