// Source: uo_cgame_mp_x86.dll 0x3001e380..0x3001e42a
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001e380_3001e42a.mcode
//
// CG_SetupWeaponLightingOrigin (provisional, role-derived name)
//
// Given a client entity (centity_t, in ECX as `this`) and an
// output refEntity_t (in EDX), fill the refEntity's lightingOrigin from the
// entity's smoothed view-weapon vec3 and flag it RF_LIGHTING_ORIGIN.
//
// The two register arguments are passed non-standard (ECX = entity `this`,
// EDX = out refEntity), matching the caller CG_...(0x3001f260) which loads the
// centity_t into ECX and LEA's the on-stack refEntity into EDX before the CALL.
// There is no stack argument (plain RET, no imm), so this is expressed with
// explicit pointer parameters; a comment records the ECX/EDX register mapping.
//
// Behavior proven from the bytes:
//   * MOV EAX,[ECX+8]; TEST AH,AH; JNS 0x3001e415
//       -> if bit 15 (0x8000) of currentState.eFlags is CLEAR, take the "unset"
//          branch: zero smoothedWeaponAngles and return (no refEntity write).
//   * Otherwise, for each of the three smoothedWeaponAngles components, compare
//     against 0.0f (FLD 0.0f @0x3007bcec; FLD comp; FUCOMPP; FNSTSW AX; TEST AH,0x44;
//     JP skip). The subtrahend/compare constant at 0x3007bcec is 0.0f (proven via
//     objdump -s -j .rdata: 0x3007bce8=0.5f, 0x3007bcec=0.0f); a prior pass misread
//     it as 0.5f.
//       - FUCOMPP sets C3(0x40) when equal; TEST AH,0x44 masks C3|C2. Equal ->
//         one bit set -> PF=0 -> JP not taken -> check the next component. Any
//         component != 0.0f -> zero bits set -> PF=1 -> JP to 0x3001e3ee (skip
//         the seed copy and use the existing smoothed values).
//       So the seed copy runs only when ALL three components still equal the
//       0.0f "unset" sentinel: first-use initialization from lerpOrigin.
//   * 0x3001e3ee: copy smoothedWeaponAngles.{x,y,z} into refEntity.lightingOrigin
//     (EDX+0xc/+0x10/+0x14) as raw dwords, OR refEntity.renderfx (EDX+0x4) with
//     RF_LIGHTING_ORIGIN (0x80), and return.
//
// The .mcode size-guess name "script_method_player_getviewmodel" is REJECTED: it
// was matched only by byte size (0xaa == 0xaa) — exactly the size-matching the
// contract forbids. This function issues no script/VM trap, reads no script
// argument, and takes an entity `this` + an out refEntity by register; it is a
// render-entity setup helper, not a script method.

#include "client/cgame/client_recovered.h"

#include <string.h>

void CG_SetupWeaponLightingOrigin(centity_t *ent /* ECX */, refEntity_t *re /* EDX */)
{
    /* MOV EAX,[ECX+8]; TEST AH,AH; JNS -> test the sign bit of the AH byte, i.e.
     * bit 15 of eFlags. JNS (not signed) means bit 15 clear. */
    if ((ent->currentState.eFlags & EF_VIEWMODEL_ANGLES_VALID) == 0) {
        /* 0x3001e415: XOR EAX,EAX; store 0 into all three smoothed components.
         * (Instruction order writes +0x228, then +0x224, then +0x220.) */
        ent->smoothedWeaponAngles[2] = 0.0f;
        ent->smoothedWeaponAngles[1] = 0.0f;
        ent->smoothedWeaponAngles[0] = 0.0f;
        return;
    }

    /* Seed the smoothed angles from the current weapon angles only while every
     * component still holds the 0.0f "unset" sentinel (first-use init). Any
     * component already differing means the smoothed value is live -> keep it. */
    if (ent->smoothedWeaponAngles[0] == CG_WEAPON_ANGLE_SMOOTH_UNSET && ent->smoothedWeaponAngles[1] == CG_WEAPON_ANGLE_SMOOTH_UNSET &&
        ent->smoothedWeaponAngles[2] == CG_WEAPON_ANGLE_SMOOTH_UNSET) {
        ent->smoothedWeaponAngles[0] = ent->lerpOrigin[0];
        ent->smoothedWeaponAngles[1] = ent->lerpOrigin[1];
        ent->smoothedWeaponAngles[2] = ent->lerpOrigin[2];
    }

    /* 0x3001e3ee: publish the smoothed angles as the refEntity lighting origin and
     * mark the flag. The stores are dword MOVs (raw copies), interleaved in the
     * bytes with the renderfx OR; the ordering has no observable effect here. */
    uint32_t componentBits;
    memcpy(&componentBits, &ent->smoothedWeaponAngles[0], sizeof(componentBits));
    memcpy(&re->lightingOrigin[0], &componentBits, sizeof(componentBits));
    memcpy(&componentBits, &ent->smoothedWeaponAngles[1], sizeof(componentBits));
    memcpy(&re->lightingOrigin[1], &componentBits, sizeof(componentBits));
    int32_t renderfx = coduo_int32_from_bits((uint32_t)re->renderfx | (uint32_t)RF_LIGHTING_ORIGIN);
    memcpy(&componentBits, &ent->smoothedWeaponAngles[2], sizeof(componentBits));
    memcpy(&re->lightingOrigin[2], &componentBits, sizeof(componentBits));
    re->renderfx = renderfx;
}
