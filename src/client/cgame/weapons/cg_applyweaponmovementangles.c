// Source: uo_cgame_mp_x86.dll 0x30045070..0x300450d3
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30045070_300450d3.mcode
//
// CG_ApplyWeaponMovementAngles — compute this frame's weapon-movement view-angle offset, record it
// in the global cg_weaponMovementAngles vec3, and add it into the caller's view
// angle accumulator.
//
// Name adjudication: the mechanical header names this "Com_DPrintf", a pure
// size match (win size 0x63 == corpus size 0x63) with no behavioral basis.
// Rejected: this function does no printing/formatting. It zeroes a scratch
// vec3, calls the weapon-movement angle computation (0x30044ce0) which accumulates
// pitch/yaw/roll into that scratch, publishes the scratch to a global vec3
// (0x30487958), and FADDs it componentwise into the caller's angle vector. That
// is the CG-side weapon-movement applier. "CG_ApplyWeaponMovementAngles" is provisional by proven
// role; the callee is the BG/CG weapon-movement angle math.
//
// Calling convention: this is a non-standard register-arg function. The output
// angle vector is passed in EDI (never initialized here; the sole caller,
// 0x30045230, keeps the destination vec3 in EDI across the call and FADD/FSTPs
// into [EDI]/[EDI+4]/[EDI+8] both before and after). The callee 0x30044ce0
// preserves EDI. Modeled here as a normal vec3_t parameter; RET has no stack
// cleanup immediate (args are register-borne), consistent with that.

#include "client/cgame/client_recovered.h"

void CG_ApplyWeaponMovementAngles(vec3_t angles /* EDI: view-angle accumulator, in/out */)
{
    // 30045078..3004508f: zero a scratch vec3 (v[2],v[1],v[0] stores).
    vec3_t movement = { 0.0f, 0.0f, 0.0f };

    // 30045090: CALL 0x30044ce0 with ESI = &movement. The callee accumulates the
    // computed weapon-movement angles into movement[0..2] (three FADD [ESI+n]/FSTP).
    CG_CalcWeaponMovementAngles(movement);

    // 30045095..300450b0: publish the computed movement to the global vec3. The
    // machine code copies the three float slots through EAX/ECX/EDX as raw
    // dwords (integer MOVs) — a bit-exact copy, i.e. plain float assignment.
    cg_weaponMovementAngles[0] = movement[0];
    cg_weaponMovementAngles[1] = movement[1];
    cg_weaponMovementAngles[2] = movement[2];

    // 300450b6..300450cc: add the movement componentwise into the caller's angles.
    // (In the binary the FLD of movement[0] precedes the global stores and the
    // POP ESI shifts the stack frame, but movement is not modified in between, so
    // this is the faithful componentwise accumulation.)
    angles[0] += movement[0];
    angles[1] += movement[1];
    angles[2] += movement[2];
}
