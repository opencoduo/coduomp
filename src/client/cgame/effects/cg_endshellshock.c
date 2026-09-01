// Source: uo_cgame_mp_x86.dll 0x3003c1d0..0x3003c227
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003c1d0_3003c227.mcode
//
// CG_EndShellShock — stop the shellshock screen effect and restore the screen to
// its normal, un-blurred, un-tinted state. Part of the cgame shellshock subsystem
// (the disorienting blur/flash after a nearby explosion). Behavior, proven from the
// machine code:
//
//   1. CG_EndShellShockSound() — stop the running shellshock loop sound and fire the
//      "shellshock_end_abort" event (callee 0x3003c0f0; it reads the loop-sound
//      handle cg_shellshockLoopSound at 0x3048bfe8 and pushes the .rdata string
//      cg_shellshockEndAbortAliasName at 0x3007a3cc).
//   2. Restore the shellshock mouse-sensitivity scale at 0x3048bfec to 1.0f and
//      clear the engine pitch/yaw speed limits via trap(246, 0, 0).
//   3. Reset the screen-blur pair cg_shellshockScreenBlurX/Y (0x3048bff0/0x3048bff4)
//      to 0.0f and push the cleared blur amount via trap(0x57, 0).
//
// Naming: the .mcode header's size-matched "BG_IsCrouchingAnim" guess is REJECTED —
// this function returns void, writes the shellshock screen-state globals, and issues
// cgame shellshock mouse/blur traps; it is not a crouch-anim predicate. The resolved
// role matches the
// same-module PPC bank symbol cgame_mp!CG_EndShellShock, adopted as the name; the
// mouse trap binding is independently proven by the recovered engine dispatcher.
//
// Machine-code notes:
//   * SUB ESP,0x8 reserves two 4-byte stack slots [ESP] and [ESP+4]; both are set to
//     0, reloaded into EAX/ECX, and pushed as the two trap(0xf6) arguments. They are
//     zero-initialized mouse speed limits, so this is trap(246, 0, 0).
//   * Argument push order for the id-0xf6 call is PUSH EAX (slot0); PUSH ECX (slot1);
//     PUSH 246, i.e. cgame_syscall(246, slot0, slot1),
//     matching the 2-arg thin wrapper at 0x3003f338.
//   * The id-0x57 call is PUSH 0x0; PUSH 0x57, i.e. cgame_syscall(0x57, 0), matching
//     the 1-arg thin wrapper at 0x3003e370.
//   * The two global writes 0x3048bfec=0x3f800000 (1.0f) and 0x3048bff0/bff4=0 happen
//     between the argument pushes but before their respective calls; ordering does
//     not affect the trap arguments (all pushed values are constants/zeroed locals).
//   * RET is a plain near return; the caller-side ADD ESP,0x1c (28 = 2 local + 3
//     call-1 arg + 2 call-2 arg dwords) is cdecl caller cleanup, so no non-default
//     return convention.

#include "client/cgame/client_recovered.h"

void CG_EndShellShock(void)
{
    /* Stop the loop sound / fire the abort event (callee 0x3003c0f0). */
    CG_EndShellShockSound();

    /* Restore neutral mouse sensitivity and clear the pitch/yaw speed limits. */
    cg_shellshockMouseSensitivityScale = 1.0f;
    cgame_syscall(CG_SET_SHELLSHOCK_MOUSE_LIMITS, 0, 0);

    /* Clear the screen blur: both components 0, push the cleared amount. */
    cg_shellshockScreenBlurX = 0.0f;
    cg_shellshockScreenBlurY = 0.0f;
    cgame_syscall(CG_SET_SHELLSHOCK_SCREEN_BLUR, 0);
}
