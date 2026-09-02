// Source: uo_cgame_mp_x86.dll 0x300450e0..0x3004519c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300450e0_3004519c.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_SpinEffectPointToWorld — transform one point from the spinning view-effect's
 * local axis frame into world space, anchored at the current view origin.
 *
 * NAME NOTE: the .mcode pre-hint "Item_ListBox_MaxScroll" is a pure size-match
 * guess (win size 0xbc == matched size 0xbc) and is REJECTED per the naming rules
 * (never identify a function by size). The machine code does no listbox / scroll
 * arithmetic whatsoever: it derives an orientation basis from the effect spin
 * angles via AngleVectors and applies a rigid point transform. The name here is
 * provisional-by-role; the exact original CoD symbol is unproven.
 *
 * ABI: one 32-bit stack argument, a pointer to a vec3_t point (`RET`, no imm — the
 * caller cleans the single pushed arg). Sole caller 0x300453ee pushes EDI, a vec3
 * it has been accumulating, then reads the transformed result back from the same
 * buffer. The point is read IN PLACE first, then overwritten with the result.
 *
 * Machine-code facts proven for every statement below (post 4-push frame; the two
 * POP EDI/POP ESI at 0x30045144/0x30045147 shift later [ESP+x] refs by +8, already
 * accounted for):
 *   300450e5  MOV EBP,[ESP+0x3c]           EBP = point (the arg pointer)
 *   300450e9  MOV EDX,[EBP+8]  \
 *   300450ec  MOV EAX,[EBP+0]   } read the input point BEFORE it is overwritten
 *   300450ef  MOV ECX,[EBP+4]  /
 *   300450f4  MOV [ESP+0x18],EDX           pz = point.z
 *   30045109  MOV [ESP+0x10],EAX           px = point.x
 *   3004510d  MOV [ESP+0x14],ECX           py = point.y
 *   300450f8  LEA EBX,[ESP+0x34]           up      output buffer (arg4)
 *   300450fc  LEA EDI,[ESP+0x28]           right   output buffer (arg3)
 *   30045100  LEA ESI,[ESP+0x1c]           forward output buffer (arg2)
 *   30045104  MOV EDX,0x30487ac8           angles  = &cg_effectSpin angles vec3
 *   30045111  CALL 0x3004a200              AngleVectors(angles, forward, right, up)
 *
 * The angles source is the three CONTIGUOUS floats at 0x30487ac8/0xacc/0xad0
 * (cg_refdefViewAngles[0], cg_refdefViewAngles[1]/yaw, and the roll partner at
 * 0x30487ad0). AngleVectors requires a vec3_t base; the base of that triple is the
 * already-named scalar cg_refdefViewAngles[0] (0x30487ac8), so its address is
 * taken as the vec3 origin rather than minting a second alias symbol for the same
 * address.
 *
 * The result vec is first seeded with cg_refdef.vieworg (the current view origin,
 * 0x30487a90/0x94/0x98) via three integer dword copies (MOV [EBP],EAX from
 * [0x30487a90] etc.), then the FP block accumulates the transformed point onto it:
 *
 *   30045116 FLD [px?]  -> actually FLD [ESP+0x14]=py ; FCHS  -> ST = -py
 *   30045121 FLD right.x ; FMUL ST(1)  -> -py * right.x                (2nd block reuses -py)
 *   30045130 FLD up.x    ; FMUL pz     -> pz * up.x
 *   30045145 FADDP                     -> pz*up.x - py*right.x
 *   30045148 FLD forward.x ; FMUL px   -> px * forward.x
 *   30045150 FADDP                     -> px*fwd.x + pz*up.x - py*right.x
 *   30045152 FADD [EBP+0]=origin.x ; FSTP [EBP+0]
 *       => out.x = origin.x + px*forward.x - py*right.x + pz*up.x
 *   (0x30045158.. and 0x30045178.. repeat the identical form for .y and .z, and the
 *    third block consumes the leftover -py in ST0 rather than re-FCHS'ing.)
 *
 * i.e. out = viewOrigin + px*forward - py*right + pz*up, the standard rigid
 * local-frame-to-world placement (right is subtracted, matching the negate-right
 * AnglesToAxis convention used elsewhere in this binary).
 */
void CG_SpinEffectPointToWorld(vec3_t point /* one 32-bit stack arg: vec3_t * */)
{
    vec3_t forward, right, up;

    /* Snapshot the input point before the result overwrites it in place. */
    const float px = point[0];
    const float py = point[1];
    const float pz = point[2];
    const long double negPy = -(long double)py;

    /* AngleVectors reads the contiguous spin-angle vec3 at 0x30487ac8..0x30487ad0;
     * cg_refdefViewAngles[0] is component [0] (the vec3 base). */
    AngleVectors(cg_refdefViewAngles, forward, right, up);

    /* Seed with the view origin (copied as three dwords), then add the transform. */
    point[0] = (float)((((negPy * (long double)right[0]) +
                         (long double)pz * (long double)up[0]) +
                        (long double)px * (long double)forward[0]) +
                       (long double)cg_refdef.vieworg[0]);
    point[1] = (float)((((negPy * (long double)right[1]) +
                         (long double)pz * (long double)up[1]) +
                        (long double)px * (long double)forward[1]) +
                       (long double)cg_refdef.vieworg[1]);
    point[2] = (float)((((negPy * (long double)right[2]) +
                         (long double)pz * (long double)up[2]) +
                        (long double)px * (long double)forward[2]) +
                       (long double)cg_refdef.vieworg[2]);
}
