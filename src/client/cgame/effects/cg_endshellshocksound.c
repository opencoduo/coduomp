// Source: uo_cgame_mp_x86.dll 0x3003c0f0..0x3003c16a
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003c0f0_3003c16a.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

/*
 * CG_EndShellShockSound (0x3003c0f0) — end the shellshock looping sound.
 *
 * Name adjudication: the .mcode header's "Player_GetMethod" guess is a pure
 * size match (win size 0x7a == matched size 0x7a) and is REJECTED — this
 * function performs no method lookup. Its behavior (issue the shellshock sound
 * traps, register/play "shellshock_end_abort", clear the shellshock sound state)
 * plus its call graph — the only callers are CG_EndShellShock (0x3003c1d0, the
 * PPC bank's tiny CG_EndShellShock that tail-calls CG_EndShellShockSound) and the
 * per-frame shellshock update at 0x3003c247 — identify it as the sound-cleanup
 * member of the shellshock End* family. The same-module PPC bank lists
 * cgame_mp!CG_EndShellShockSound; adopted as the role name.
 *
 * Behavior (every call, unconditionally):
 *   1. trap(220, cg_soundChannelFullVolumes, 0) // restore 10 channel volumes
 *   2. trap(221, "generic", 0, 0)          // reset the room environment
 * Then, only if a shellshock sound is still active
 * (cg_shellshockSoundEndTime != 0):
 *   3. handle = trap(196, "shellshock_end_abort", vec3_origin)
 *      cg_shellshockSoundEndTime = 0;                                // clear state
 *   4. trap(198, handle, 1023, vec3_origin, 0)
 *
 * The traps go through cgame_syscall (*0x30085e9c). Trap ids 220/221/196/198 are
 * proven trap numbers named CG_MSS_FADE_SELECT_SOUNDS,
 * CG_MSS_SET_ENVIRONMENT_EFFECTS, CG_COM_PICK_SOUND_ALIAS, and
 * CG_MSS_PLAY_SOUND_ALIAS.
 *
 * Instruction map:
 *   3003c0f0 PUSH ECX                          reserve 4-byte stack local (handle)
 *   3003c0f1 PUSH 0x0                           trap220 arg1 = 0
 *   3003c0f3 PUSH 0x30071f30                    trap220 arg0 = full-volume table
 *   3003c0f8 PUSH 0xdc                          trap id 220
 *   3003c0fd CALL [0x30085e9c]                  cgame_syscall(220, fullVolumes, 0)
 *   3003c103 MOV  [ESP+0xc],0x0                 local = 0  (the pushed-ECX slot)
 *   3003c10b MOV  EAX,[ESP+0xc]                 EAX = 0
 *   3003c10f PUSH 0x0                            trap221 arg2 = 0
 *   3003c111 PUSH EAX                            trap221 arg1 = 0
 *   3003c112 PUSH 0x3007a3e4                     trap221 arg0 = "generic"
 *   3003c117 PUSH 0xdd                           trap id 221
 *   3003c11c CALL [0x30085e9c]                  cgame_syscall(221, "generic", 0, 0)
 *   3003c122 MOV  EAX,[0x3048bfe8]              EAX = cg_shellshockSoundEndTime
 *   3003c127 ADD  ESP,0x1c                       clean trap220(3)+trap221(4)=7 dwords
 *   3003c12a TEST EAX,EAX
 *   3003c12c JZ   3003c168                       if (endTime == 0) return
 *   3003c12e PUSH 0x30071f58                     trap196 arg1 = &vec3_origin
 *   3003c133 PUSH 0x3007a3cc                     trap196 arg0 = "shellshock_end_abort"
 *   3003c138 PUSH 0xc4                           trap id 196
 *   3003c13d MOV  [0x3048bfe8],0x0              cg_shellshockSoundEndTime = 0
 *   3003c147 CALL [0x30085e9c]                  EAX = handle = trap(196, name, &vec3_origin)
 *   3003c14d PUSH 0x0                            trap198 arg3 = 0
 *   3003c14f PUSH 0x30071f58                     trap198 arg2 = &vec3_origin
 *   3003c154 PUSH 0x3ff                          trap198 arg1 = 1023 (channel mask)
 *   3003c159 PUSH EAX                            trap198 arg0 = handle
 *   3003c15a PUSH 0xc6                           trap id 198
 *   3003c15f CALL [0x30085e9c]                  cgame_syscall(198, handle, 1023, &vec3_origin, 0)
 *   3003c165 ADD  ESP,0x20                       clean trap196(3)+trap198(5)=8 dwords
 *   3003c168 POP  ECX ; RET
 *
 * Note on the pushed pointers: trap 220 receives 0x30071f30, the start of ten
 * consecutive 1.0f channel-volume targets ending at 0x30071f57. Traps 196 and
 * 198 both receive the distinct following address 0x30071f58 = vec3_origin.
 */
void CG_EndShellShockSound(void)
{
    /* Restore all ten sound channels and reset the room environment. */
    cgame_syscall(CG_MSS_FADE_SELECT_SOUNDS, (intptr_t)cg_soundChannelFullVolumes, 0);
    cgame_syscall(CG_MSS_SET_ENVIRONMENT_EFFECTS, (intptr_t)cg_genericShellshockAliasName, 0, 0);

    /* Only if a shellshock sound was still scheduled/active: register and play
     * the "shellshock_end_abort" sound, then clear the active-sound state. */
    if (cg_shellshockSoundEndTime != 0) {
        /* The store to cg_shellshockSoundEndTime (0x3003c13d) precedes the trap-196
         * CALL (0x3003c147) in the machine code; trap 196 does not read the global,
         * so the observable order is preserved either way. */
        cg_shellshockSoundEndTime = 0;
        snd_alias_t *alias = trap_Com_PickSoundAlias(cg_shellshockEndAbortAliasName, vec3_origin);
        (void)trap_MSS_PlaySoundAlias(alias, 1023, vec3_origin, 0);
    }
}
