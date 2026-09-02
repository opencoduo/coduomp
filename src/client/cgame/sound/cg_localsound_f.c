// Source: uo_cgame_mp_x86.dll 0x3003ac00..0x3003ac90
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003ac00_3003ac90.mcode
//
// CG_LocalSound_f — the enable-gated console-command variant of CG_LocalSound.
// It plays a numbered local (non-positional) sound named by a CS_SOUNDS config
// string.
//
// Name evidence: the body is BYTE-FOR-BYTE identical to CG_LocalSound
// (0x3003ab70) apart from a leading enable gate, and it embeds the same two
// .rdata error strings that name the function outright:
//   0x30079f20 "ERROR: CG_LocalSound called with %i args (should be 2)\n"
//   0x30079ed8 "ERROR: CG_LocalSound called with index %i (should be in range[1,%i])\n"
// So the mechanical size-matched guess `MenuParse_forecolor` (win size 0x90) is
// rejected — the strings prove local-sound behavior, not a menu forecolor parser.
// This is a *separate* handler from CG_LocalSound, not an alias: the cgame command
// dispatch table (0x3003ac90) has one case that CALLs 0x3003ab70 (CG_LocalSound)
// and a distinct case that CALLs 0x3003ac00 (this). The only difference is the gate
// at the top, so it is named with the `_f` command-handler suffix; the exact CoD
// command string is unproven.
//
// Machine-code notes (all behavior below is traced to the .mcode):
//   - The leading MOV EAX,[0x3045884c] / TEST EAX,EAX / JZ 0x3003ac8f is a boolean
//     enable gate: when cg_announcerSounds_vmCvar.integer is zero the whole handler is a
//     no-op. (This global is refs=1 and never written in this DLL, so its exact
//     source identity is unresolved; typed qboolean and named by role.)
//   - trap_Argc()  == cgame_syscall(12)          -> EAX = console argument count
//   - trap_Argv()  == cgame_syscall(13, ...)     -> copies argv[1] into the buffer
//   - Q_atoi(0x3005b6ce -> body 0x3005b646)       -> parse the numeric argument
//   - CG_ConfigString(0x3002c990)                 -> gameState config string lookup
//   - CG_PlaySoundAliasByName(0x3002ca80)              -> play it (custom register ABI)
//   - Com_PrintMessage(0x3002b2b0)                -> variadic print backend
//   - cg_snap == *(snapshot_t **)0x30459160     -> current snapshot pointer
//   - g_textScratchBuffer == 0x300da488[1024]     -> shared 1024-byte text buffer
//   All comparisons are signed (JLE at 0x3003ac4f, JG at 0x3003ac56); 0x295 == 661
//   is CS_SOUNDS and 0x100 == 256 is CS_SOUNDS_COUNT (both natural-form constants).

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_LocalSound_f(void)
{
    /* MOV EAX,[0x3045884c] / TEST EAX,EAX / JZ 0x3003ac8f : enable gate. */
    if (!cg_announcerSounds_vmCvar.integer) {
        return;
    }

    /* PUSH 0xc / CALL *cgame_syscall / ADD ESP,4 ; CMP EAX,2 / JZ 0x3003ac2c */
    int argc = trap_Argc();
    if (argc != 2) {
        /* PUSH EAX (argc) / PUSH fmt / CALL Com_PrintMessage / ADD ESP,8 / RET */
        Com_PrintMessage("ERROR: CG_LocalSound called with %i args (should be 2)\n", argc);
        return;
    }

    /* PUSH 0x400 / PUSH buf / PUSH 1 / PUSH 0xd / CALL *cgame_syscall :
     * trap_Argv(1, g_textScratchBuffer, 1024) */
    trap_Argv(1, g_textScratchBuffer, 1024);

    /* PUSH buf / CALL Q_atoi (0x3005b6ce) ; ADD ESP,0x14 cleans the four
     * trap_Argv args plus this one. EAX = parsed sound index. */
    int soundIndex = coduo_crt_atoi(g_textScratchBuffer);

    /* TEST EAX,EAX / JLE 0x3003ac7c ; CMP EAX,0x100 / JG 0x3003ac7c
     * (signed: reject index <= 0 or > 256) */
    if (soundIndex <= 0 || soundIndex > CS_SOUNDS_COUNT) {
        /* PUSH 0x100 / PUSH EAX (soundIndex) / PUSH fmt / CALL Com_PrintMessage
         * / ADD ESP,0xc / RET  (cdecl: fmt, soundIndex, 256) */
        Com_PrintMessage("ERROR: CG_LocalSound called with index %i (should be in range[1,%i])\n", soundIndex, CS_SOUNDS_COUNT);
        return;
    }

    /* ADD EAX,0x295 ; PUSH EAX ; CALL CG_ConfigString (0x3002c990)
     * -> config string for CS_SOUNDS + soundIndex (the config-string-index push
     *    is cleaned together with the clientNum push by the trailing ADD ESP,8). */
    const char *soundName = CG_ConfigString(CS_SOUNDS + soundIndex);

    /* MOV EDX,[cg_snap] ; LEA ECX,[EDX+0x20] ; MOV EDX,[EDX+0xe0] ; PUSH EDX
     * ; CALL CG_PlaySoundAliasByName (0x3002ca80) ; ADD ESP,8 ; RET
     * ECX(this) = &cg_snap->ps.psOrigin, EAX = soundName, stack = clientNum. */
    CG_PlaySoundAliasByName(cg_snap->ps.psClientNum, &cg_snap->ps.psOrigin, soundName);
}
