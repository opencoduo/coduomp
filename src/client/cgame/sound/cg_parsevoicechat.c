#include "../client_recovered.h"
#include "../globals.h"

#include <string.h>

// Source: uo_cgame_mp_x86.dll 0x3003a410..0x3003a5a8
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003a410_3003a5a8.mcode
//
// CG_ParseVoiceChat — client handler for a server-sent voice-chat command. It reads
// the console tokens of the command via trap_Argv (cgame_syscall(0xd / CG_ARGV, n,
// g_textScratchBuffer, 1024)), Q_atoi's the numeric fields (three of them into a
// vec3_t origin, the rest as ints), and forwards the decoded message to the
// voice-chat display routine CG_VoiceChat (0x3003a250) — unless a category filter is
// enabled and the message is one of the "insult"/"taunt"/"praise"/"gauntlet"
// categories, in which case it is suppressed.
//
// Naming: the .mcode size-guess name "BG_CanItemBeGrabbed" is REJECTED — a pure
// win-size match (0x198) against a game_mp_uo (wrong DLL) function, zero behavioral
// basis; there is no item-grab logic here. The behavior — argv parsing of a
// voice-chat wire command, the voice-chat category strings kill_insult/taunt/
// death_insult/kill_gauntlet/praise, and the tail-call into CG_VoiceChat (the
// voice-chat display sibling that itself calls CG_GetTranslatedVoiceChatString) —
// fixes this as the voice-chat command parser. The three call sites in
// CG_ServerCommand (0x3003aec8/0x3003aee7/0x3003af06) each push a distinct `mode`
// literal 0/1/2 before calling, so `mode` selects the broadcast/team/target variant.
//
// Callees:
//   trap_Argv(n, buf, len) -> cgame_syscall(0xd, ...)   copy console token n into buf
//   Q_atoi(str)            -> CALL 0x3005b6ce (JMP-thunk to 0x3005b646)  parse int
//   CG_VoiceChat(...)      -> CALL 0x3003a250 (fastcall, ECX=&msg)       display
//
// Token reads and roles (each into the shared 1024-byte g_textScratchBuffer at
// 0x300da488, re-read every time; the argv indices are the literal PUSH values):
//   argv[1] -> Q_atoi -> int  (EBX)             -- field1 (kept in a callee-saved reg)
//   argv[2] -> Q_atoi -> int  (EBP)             -- field2 (kept in a callee-saved reg)
//   argv[3] -> Q_atoi -> FILD/FSTP -> float     -- origin[0]
//   argv[5] -> Q_atoi -> int                    -- color (the '^'+%c chat-line char)
//   argv[6] -> Q_atoi -> FILD/FSTP -> float     -- origin[1]
//   argv[7] -> Q_atoi -> FILD/FSTP -> float     -- origin[2]
//   argv[4] -> (string, NOT atoi'd; left in g_textScratchBuffer) -- voiceChatString
//
// origin is a genuine packed vec3_t: the three FSTP destinations resolve (after
// anchoring the frame at the proven RET epilogue) to three consecutive dwords
// (origin[1] = origin[0]+4, origin[2] = origin[0]+4). argv[4] is read LAST, so
// g_textScratchBuffer still holds its string for both the category comparisons and
// the CG_VoiceChat call.
//
// Category filter (0x3003a50e..0x3003a581): if cg_noTaunt_vmCvar.integer
// (0x30421c0c) is nonzero, the argv[4] string in g_textScratchBuffer is compared (via
// REP CMPSB, i.e. memcmp over each category name's full stored length including its
// trailing NUL, ECX = 12/6/13/14/7) against "kill_insult", "taunt", "death_insult",
// "kill_gauntlet" and "praise". A match on any one skips the display and returns. When
// the flag is zero the comparisons are bypassed and the message is always displayed.
// (The REP CMPSB blocks zero EAX/EDX first, but that scratch value is dead afterward —
// only the compare's ZF is tested by the JZ; modeled as memcmp()==0.)
//
// The display call (0x3003a583..0x3003a59d): CG_VoiceChat is fastcall with ECX = a
// pointer into a local message struct that holds the origin vec3 (proven contiguous)
// and an int, plus five caller-cleaned (`add esp,0x14`) cdecl stack args. Anchoring
// the frame at the proven RET epilogue, the FIRST stack arg (EDX, [ESP+0x28]) is the
// incoming `mode` argument itself; the middle two are field1 (EBX=argv[1]) and field2
// (EBP=argv[2]); the fourth ([ESP+0x14], a local int) is the argv[5] value
// (`color` — the one int token that is neither field1/field2 nor a float, so it is
// the remaining local; this argv[5]->color mapping is a well-motivated inference,
// not a hard proof of the [ESP+0x14] slot's origin); the fifth is the argv[4]
// string in g_textScratchBuffer. CG_VoiceChat (0x3003a250, reconstructed) proves
// the roles: arg4 is printed after '^' via the second %c of the chat-line format.
//
// ABI: cdecl; `mode` is the single incoming stack arg (callers `add esp,4`; the mode
// value is also re-passed to CG_VoiceChat as its first stack arg). No return value.

/* The decoded-fields struct handed to CG_VoiceChat (ECX) is now cgVoiceChatMessage_t,
 * promoted to client_recovered.h so CG_VoiceChat's reconstruction shares the exact
 * contract. `origin` is the argv[3]/argv[6]/argv[7] vec3 and `color` is argv[5]. */

void CG_ParseVoiceChat(int32_t mode)
{
    cgVoiceChatMessage_t msg;

    /* 0x3003a417..0x3003a433: argv[1] -> int. */
    trap_Argv(1, g_textScratchBuffer, sizeof(g_textScratchBuffer));
    int32_t field1 = coduo_crt_atoi(g_textScratchBuffer);

    /* 0x3003a435..0x3003a453: argv[2] -> int. */
    trap_Argv(2, g_textScratchBuffer, sizeof(g_textScratchBuffer));
    int32_t field2 = coduo_crt_atoi(g_textScratchBuffer);

    /* 0x3003a455..0x3003a4b3: argv[3] -> int -> float (origin[0]). */
    trap_Argv(3, g_textScratchBuffer, sizeof(g_textScratchBuffer));
    msg.origin[0] = (float)coduo_crt_atoi(g_textScratchBuffer);

    /* 0x3003a475..0x3003a4c3: argv[5] -> the chat-line color char value. */
    trap_Argv(5, g_textScratchBuffer, sizeof(g_textScratchBuffer));
    msg.color = coduo_crt_atoi(g_textScratchBuffer);

    /* 0x3003a4a7..0x3003a4dd: argv[6] -> int -> float (origin[1]). */
    trap_Argv(6, g_textScratchBuffer, sizeof(g_textScratchBuffer));
    msg.origin[1] = (float)coduo_crt_atoi(g_textScratchBuffer);

    /* 0x3003a4d1..0x3003a507: argv[7] -> int -> float (origin[2]). */
    trap_Argv(7, g_textScratchBuffer, sizeof(g_textScratchBuffer));
    msg.origin[2] = (float)coduo_crt_atoi(g_textScratchBuffer);

    /* 0x3003a4fb..0x3003a50c: argv[4] left as a string in g_textScratchBuffer (no
     * Q_atoi) — the voice-chat-string token used below and by CG_VoiceChat. */
    trap_Argv(4, g_textScratchBuffer, sizeof(g_textScratchBuffer));

    /* 0x3003a50e..0x3003a581: if the category filter is enabled, suppress the
     * insult/taunt/praise/gauntlet categories. */
    if (cg_noTaunt_vmCvar.integer != 0) {
        if (memcmp(g_textScratchBuffer, cg_voiceChatKillInsultCommandName, sizeof(cg_voiceChatKillInsultCommandName)) == 0) {
            return;
        }
        if (memcmp(g_textScratchBuffer, cg_voiceChatTauntCommandName, sizeof(cg_voiceChatTauntCommandName)) == 0) {
            return;
        }
        if (memcmp(g_textScratchBuffer, cg_voiceChatDeathInsultCommandName, sizeof(cg_voiceChatDeathInsultCommandName)) == 0) {
            return;
        }
        if (memcmp(g_textScratchBuffer, cg_voiceChatGauntletKillCommandName, sizeof(cg_voiceChatGauntletKillCommandName)) == 0) {
            return;
        }
        if (memcmp(g_textScratchBuffer, cg_voiceChatPraiseCommandName, sizeof(cg_voiceChatPraiseCommandName)) == 0) {
            return;
        }
    }

    /* 0x3003a583..0x3003a59d: display the decoded voice-chat message. */
    CG_VoiceChat(&msg, mode, field1, field2, msg.color, g_textScratchBuffer);
}
