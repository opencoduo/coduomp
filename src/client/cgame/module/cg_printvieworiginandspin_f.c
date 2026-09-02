// Source: uo_cgame_mp_x86.dll 0x300172d0..0x3001730e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300172d0_3001730e.mcode
//
// Console debug-dump command: prints the current client view origin
// (cg_refdef.vieworg, the vec3 at 0x30487a90) and the animated view-effect yaw
// angle (cg_refdefViewAngles[1], the float at 0x30487acc), each truncated toward
// zero by MSVC's `_ftol2`, through the low-level print backend
// Com_PrintMessage using the format "(%i %i %i) : %i\n".
//
// NAME ADJUDICATION: the .mcode size-guess name `G_PlaySoundAlias` is REJECTED.
// That is a SERVER sound routine; this body touches no sound API, no server, no
// alias table. It reads two client render/effect globals and prints them. The
// 0x3e size match is meaningless (sizes do not correspond across builds). The
// name below is role-derived from behavior (a viewpos-style state dump). The
// exact original CoD command symbol is not proven, so the name carries no
// address and is flagged provisional.
//
// ABI: no register/stack inputs are read (it has no prologue and no argument
// slots) and it returns void. `ADD ESP,0x14` at the tail is the caller-clean
// cleanup of the five 32-bit args Com_PrintMessage was called with (cdecl
// varargs); it is a calling-convention detail, not source behavior. This is
// registered as a Cmd_* console-command callback, so it has no direct CALL site
// inside .text.

#include "client/cgame/globals.h"          /* cg_refdef.vieworg, cg_refdefViewAngles[1] */
#include "client/cgame/client_recovered.h" /* Q_rint, Com_PrintMessage */

/*
 * Print format at .rdata 0x300769c0: "(%i %i %i) : %i\n".
 * Evidence order (last-pushed arg is read first by cdecl):
 *   FLD [0x30487acc]; CALL _ftol2             -> effectSpinAngle truncated (pushed last)
 *   FLD [0x30487a98]; PUSH; CALL _ftol2       -> refdefViewOrg.z truncated
 *   FLD [0x30487a94]; PUSH; CALL _ftol2       -> refdefViewOrg.y truncated
 *   FLD [0x30487a90]; PUSH; CALL _ftol2       -> refdefViewOrg.x truncated
 *   PUSH (x); PUSH format; CALL Com_PrintMessage
 * so the printed arguments are (x, y, z, effectSpinAngle) in that order.
 */
void CG_PrintViewOriginAndSpin_f(void)
{
    /* The retail _ftol2 calls execute in spin,z,y,x order.  Sequence them
     * explicitly rather than relying on vararg evaluation order. */
    const int32_t spin = coduo_fp_to_i32_extended(cg_refdefViewAngles[1]);
    const int32_t z = coduo_fp_to_i32_extended(cg_refdef.vieworg[2]);
    const int32_t y = coduo_fp_to_i32_extended(cg_refdef.vieworg[1]);
    const int32_t x = coduo_fp_to_i32_extended(cg_refdef.vieworg[0]);

    Com_PrintMessage("(%i %i %i) : %i\n", x, y, z, spin);
}
