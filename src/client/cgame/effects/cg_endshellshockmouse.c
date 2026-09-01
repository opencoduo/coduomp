// Source: uo_cgame_mp_x86.dll 0x3003c170..0x3003c1a4
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003c170_3003c1a4.mcode
//
// CG_EndShellShockMouse — restore neutral mouse sensitivity and clear the
// shellshock pitch/yaw speed limits. The exact name is present in the same-module
// PPC symbol bank; CoDUOMP's recovered trap dispatcher proves syscall 246 is
// CG_SET_SHELLSHOCK_MOUSE_LIMITS rather than a renderer blend operation.
//
// This is the neutral mouse leg shared across the shellshock subsystem. It is
// tail-called by CG_UpdateShellShockMouse (0x3003c530) when the envelope time is
// nonpositive, and is structurally identical to the mouse-reset leg inlined in
// CG_EndShellShock (0x3003c1d0).
//
// Naming: the .mcode header's size-matched "PM_LightLandingForSurface" guess is
// REJECTED — there is no pmove/landing/surface work here. The body restores the
// shellshock mouse-sensitivity multiplier and clears the engine mouse limits.
//
// Machine-code notes:
//   * SUB ESP,0x8 reserves two 4-byte local slots [ESP] and [ESP+4]; both are set
//     to 0 (MOVL $0x0), reloaded into EAX/ECX, and pushed as the two trap
//     arguments. They are zero-initialized maximum pitch/yaw speeds, so this is
//     trap(246, 0, 0).
//   * Argument push order for the id-0xf6 call is PUSH EAX (slot0); PUSH ECX
//     (slot1); PUSH 246, i.e. cgame_syscall(246, slot0, slot1).
//   * The write 0x3048bfec = 0x3f800000 (=1.0f) happens between the argument
//     pushes and the call; ordering does not affect the trap arguments (all
//     pushed values are zeroed locals).
//   * RET is a plain near return; the ADD ESP,0x14 (20 = 2 local + 3 pushed call
//     dwords) is cdecl caller cleanup, so no non-default return convention.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_EndShellShockMouse(void)
{
    cg_shellshockMouseSensitivityScale = 1.0f;
    cgame_syscall(CG_SET_SHELLSHOCK_MOUSE_LIMITS, 0, 0);
}
