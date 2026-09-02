#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

// Source: uo_cgame_mp_x86.dll 0x300164b0..0x30016567
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300164b0_30016567.mcode
//
// Name adjudication: the .mcode "# name BG_IndexForString" is REJECTED. It was
// assigned purely by size match (win 0xb7 == corpus 0xb7), which the rules
// forbid. BG_IndexForString is a token/string -> index parser; this function
// does no string handling at all. Behaviorally this is an EFFECT emitter: it
// reads a global spin angle, builds a Z-axis rotation basis, computes an origin
// 256 units in front of the camera, and fires an effect handle through the
// play-effect trap (id 0xe9, CG_PLAY_EFFECT_ON_TAG). Named CG_PlaySpinningViewEffect
// by proven role; exact original CoD symbol unproven (no cgame syscall/name table
// recovered), so the name is provisional.
//
// Machine-code derivation (frame base F = ESP right after `SUB ESP,0x3c`, before
// the PUSH ESI / PUSH EAX prologue pushes):
//
//   0x300164b3  MOV EAX,[cg_refdefViewAngles[1]]            ; float angle, by-value copy
//   0x300164b8  PUSH ESI                                ; save ESI (callee-saved)
//   0x300164b9  PUSH EAX                                ; stack arg = spin angle
//   0x300164ba  LEA ESI,[ESP+8]  = &right (locals[F])    ; ESI output
//   0x300164be  LEA EDX,[ESP+0x20]=&forward(locals[F+0x18]); EDX output
//   0x300164c2  CALL YawVectors(angle,forward,right)
//        => right   = (sin, -cos, 0)  at F..F+8
//           forward = (cos,  sin, 0)  at F+0x18..F+0x20
//
// Then it assembles a 3x3 orientation axis and an origin:
//   axis[0] = right                = ( cos,  sin, 0)   (F+0x18..)
//   axis[1] = -right               = (-sin,  cos, 0)   (F+0x24..) via FLD 0.0/FSUB
//   axis[2] = ( 0, 0, 1 )                              (F+0x30..) two zeroed dwords
//                                                       + 1.0f (0x3f800000)
//   origin[i] = right[i]*256.0 + cg_refdef.vieworg[i]    (F+0xc..) FMUL 256/FADD org
// This axis is exactly a rotation about +Z by the spin angle.
//
// Syscall (0x30016559  CALL [cgame_syscall @ 0x30085e9c]) args, pushed low->high:
//   PUSH 0                    (a4 = 0, from XOR EAX,EAX)
//   PUSH &axis (=F+0x18)      (a3 = orientation)
//   PUSH &origin (=F+0xc)     (a2 = origin)
//   PUSH cg_effectDefs[effectId] (a1 = engine effect handle; EAX*4+table)
//   PUSH 0xe9                 (id = CG_PLAY_EFFECT_ON_TAG)
// After the call `ADD ESP,0x18` cleans 6 dwords: the 5 syscall pushes plus the
// single angle arg to YawVectors (which returns with a plain RET, so the
// caller cleans it). `effectId` is the one incoming argument at [ESP+0x50] = arg0.
//
// axis[2] = (0,0,1): the two zero stores are `MOV [ESP+0x40],EAX` (ESP=F-0x10 =>
// F+0x30 = axis[2].x) and `MOV [ESP+0x44],EAX` (=> F+0x34 = axis[2].y) with EAX=0,
// and `MOV [ESP+0x54],0x3f800000` (ESP=F-0x1c => F+0x38 = axis[2].z = 1.0f).

void CG_PlaySpinningViewEffect(int effectId)
{
    vec3_t right;   /* YawVectors right out (ESI) -> negated into axis[1] */
    vec3_t origin;  /* effect world position (arg2 of the play trap)  */
    axis_t axis;    /* 3x3 orientation matrix (arg3 of the play trap) */

    /*
     * 0x300164b3..0x300164c2: build the two 2D basis vectors from the animated
     * spin angle. forward=(cos,sin,0), right=(sin,-cos,0).
     */
    YawVectors(cg_refdefViewAngles[1], axis[0], right);

    /*
     * 0x300164c7..0x30016555: assemble the Z-rotation orientation.
     *   axis[0] = right
     *   axis[1] = -right         (FLD 0.0 ; FSUB right[i])
     *   axis[2] = (0, 0, 1)
     * and the origin = right*256 + cg_refdef.vieworg.
     */
    axis[1][0] = (float)((long double)0.0f - (long double)right[0]);
    long double axisLeftY = (long double)0.0f - (long double)right[1];
    axis[2][0] = 0.0f;           /* 0x300164e1 MOV [F+0x30],0 */
    axis[2][1] = 0.0f;           /* 0x300164e9 MOV [F+0x34],0 */
    uint32_t effectHandle = cg_effectDefs[effectId];
    axis[1][1] = (float)axisLeftY;

    long double axisLeftZ = (long double)0.0f - (long double)right[2];
    axis[2][2] = 1.0f;           /* 0x30016511 MOV [F+0x38],0x3f800000 */
    axis[1][2] = (float)axisLeftZ;

    origin[0] = (float)((long double)axis[0][0] * (long double)256.0f +
                        (long double)cg_refdef.vieworg[0]);
    origin[1] = (float)((long double)axis[0][1] * (long double)256.0f +
                        (long double)cg_refdef.vieworg[1]);
    origin[2] = (float)((long double)axis[0][2] * (long double)256.0f +
                        (long double)cg_refdef.vieworg[2]);

    /*
     * 0x30016559: fire the effect. cg_effectDefs[effectId] is the engine handle
     * (EAX*4 + 0x304484e4). Trailing argument is a literal 0.
     */
    cgame_syscall(CG_PLAY_EFFECT_ON_TAG,
                  coduo_int32_from_bits(effectHandle),
                  (intptr_t)origin,
                  (intptr_t)axis,
                  0);
}
