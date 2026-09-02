// Source: uo_cgame_mp_x86.dll 0x3003f030..0x3003f04b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003f030_3003f04b.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

/*
 * trap_MSS_FadeAllSounds — thin cdecl wrapper for cgame syscall id 0xdb
 * (CG_MSS_FADE_ALL_SOUNDS). The Windows engine dispatcher proves that the
 * service it invokes is MSS_FadeAllSounds; the former FX-oriented recovery
 * name described neither the receiver nor the caller's sound-fade command.
 *
 * Body (0x3003f030..0x3003f04b):
 *   MOV  EAX,[ESP+0x4]      ; EAX = the single incoming stack argument (a float word)
 *   PUSH ECX                ; trap payload1 = incoming ECX duration
 *   MOV  EDX,EAX
 *   PUSH EDX                ; cgame_syscall payload0 = the float word (arg)
 *   PUSH 0xdb               ; command = CG_MSS_FADE_ALL_SOUNDS
 *   MOV  [ESP+0x10],EAX     ; spill the float back to the caller's own arg slot
 *                           ;   (compiler artifact of the float local; same value)
 *   CALL [0x30085e9c]       ; cgame_syscall(0xdb, floatBits, <ECX scratch>)
 *   ADD  ESP,0xc            ; drop the 3 pushed dwords (caller-cleaned cdecl)
 *   RET                     ; plain RET; caller cleans the single stack float arg
 *
 * Argument shape: the sole call site (0x3003b03b) does `FSTP DWORD [ESP]` immediately
 * before the CALL and `ADD ESP,4` after it, so it passes exactly one 4-byte float and
 * cleans one dword. This wrapper forwards that float (as its raw bit pattern) as the
 * trap's first payload dword. The trap's real 2-argument shape is
 * (float targetVolume, int durationMsec) — the
 * siblings CG_InstallSnapshotResetEffects (0x3003c9d0), FUN_3003d2d0, and
 * CG_ShutdownEffectsAndHud (0x3002e390) all issue the same fade-all command
 * with (1.0f_bits, 0). This thin wrapper receives the duration in ECX: the
 * sole caller at 0x3003b039 explicitly does
 * `MOV ECX,ESI` immediately before the call. The wrapper therefore has a mixed ABI:
 * float on the stack and duration in ECX, modeled as two ordinary parameters.
 *
 * CoDUOMP.exe's syscall arm at 0x00402a2f passes the float and duration to
 * MSS_FadeAllSounds (0x004533d0), proving the service and wrapper roles. The
 * return value is unused.
 */
void trap_MSS_FadeAllSounds(float targetVolume, int32_t durationMsec)
{
    cgame_syscall(CG_MSS_FADE_ALL_SOUNDS, CG_FloatBits(targetVolume), durationMsec);
}
