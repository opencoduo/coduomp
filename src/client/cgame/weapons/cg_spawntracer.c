// Source: uo_cgame_mp_x86.dll 0x30048d60..0x30048e5e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30048d60_30048e5e.mcode
//
// CG_SpawnTracer — probabilistically spawn a bullet tracer for one shot.
//
// NAME ADJUDICATION: the .mcode's mechanical `# name BG_AnimScriptStateChange` is a
// size-guess (win 0xfe ~ matched 0xff) and is REJECTED. This function contains no
// anim-script state machine. Its machine code proves cgame tracer/muzzle-flash work:
//   - it reads the tracer-chance floats cg_tracerchance_vmCvar.value (0x30456208) /
//     cg_tracerchancelmg_vmCvar.value (0x30455908) and gates on chance > 0.0;
//   - it resolves a model muzzle tag ("tag_flash"-family, passed as tagName) into a
//     world point via CG_CalcMuzzlePoint (0x30048b60, itself reconstructed);
//   - it rolls rand() (0x3005b879, returning 0..0x7fff) against
//     chance*32768 to decide whether to draw;
//   - it spawns a moving tracer (0x30048a00) or a line tracer (0x30048260) and emits a
//     bullet whiz-by sound test (0x300480f0).
// It sits in the 0x30048xxx tracer cluster next to CG_AddMovingTracer /
// CG_DrawMovingTracerPoly / CG_CalcMuzzlePoint. Role name from behavior; exact CoD
// symbol unproven.
//
// ABI (proven from the three call sites 0x30048fea / 0x3004904f / 0x30049097 and this
// body): mixed register/stack, caller-cleaned (RET, no imm; callers ADD ESP,8):
//   EAX -> impactOrigin (vec3*, the bullet impact world point; copied to EBX)
//   ESI -> entityNum   (client/entity number of the shooter)
//   EDI -> weaponIndex (index into bg_weaponInfos[])
//   stack arg0 [callee ESP+0x20] -> surfaceType (int; value 7 selects the
//                                    direct-polygon tracer path)
//   stack arg1 [callee ESP+0x28] -> tagName (const char*, the muzzle tag name)
// The register/stack split is an i386 calling-convention detail; the five arguments are
// recovered here as ordered C parameters with no calling-convention attribute (the
// syntax-only build does not require one).

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

enum { CG_TRACER_DIRECT_SURFACE_TYPE = 7 };

/* cg_entities[] base (centity_t, stride 0x288) at 0x3048c6e0, accessed through
 * the same typed view CG_CalcMuzzlePoint uses. */

void CG_SpawnTracer(vec3_t impactOrigin, int32_t entityNum, int32_t weaponIndex,
                    int32_t surfaceType, const char *tagName)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if (weaponIndex < 0 ||
        weaponIndex > bg_numWeapons ||
        (uint32_t)weaponIndex >= (uint32_t)MAX_WEAPONS) {
        Com_Printf(
            "WARNING: CG_SpawnTracer: invalid weapon index %i\n",
            weaponIndex);
        return;
    }

    /* 30048d60 FLD [cg_tracerchance_vmCvar.value]; 30048d69 FCOMP [0.0 @0x3007bcec];
     * 30048d72 FNSTSW AX; 30048d74 TEST AH,0x41; 30048d77 JNP -> skip when chance <= 0.
     * No tracers at all unless the default spawn chance is positive. */
    if (cg_tracerchance_vmCvar.value <= 0.0f) {
        return;
    }

    /* 30048d7d..30048d92: muzzle = CG_CalcMuzzlePoint(tagName, entityNum, &muzzle).
     * EAX=tagName([ESP+0x28]), ECX=entityNum(ESI), stack=&muzzle; ADD ESP,4.
     * TEST EAX,EAX; JZ -> bail if the tag/muzzle point could not be resolved. */
    vec3_t muzzle;                          /* [ESP+0x10] : muzzle world point out-buffer */
    if (!CG_CalcMuzzlePoint(tagName, entityNum, muzzle)) {
        return;
    }

    /* 30048d98..30048dc1: suppress the tracer (whiz-by test only) when this shot is the
     * local player's own and the local first-person view is active.
     * 30048d9a IMUL ECX,ESI,0x288 ; 30048da0 CMP [cg_entities[entityNum].eType],12 ; JZ ->
     *   (skip flags test) ; 30048dae TEST [cg_snap->ps.playerStateFlags],0xc0000 ; JZ ->
     *   do tracer ; 30048db7 CMP entityNum, cg_snap->ps.psClientNum ; JZ -> whiz-by test only. */
    if (cgame_compat_unchecked_cgentity(entityNum)->currentState.eType == ET_VEHICLE ||
        (cg_snap->ps.playerStateFlags & PSF_PLAYER_ENTITY_MASK) != 0u) {
        if (entityNum == cg_snap->ps.psClientNum) {
            /* 30048e4e: evaluate only the whiz-by sound and return. */
            CG_WhizbySound(impactOrigin, muzzle);
            return;
        }
    }

    /* 30048dc3..30048df8: choose the tracer spawn chance for this weapon.
     *   chance = cg_tracerchance_vmCvar.value (default); overridden to cg_tracerchancelmg_vmCvar.value when the
     *   weapon exists and its ammoType (+0x90) is LMG(3)/HMG(4)/UMG(5).
     * 30048dc8 MOV EAX,bg_weaponInfos[weaponIndex]; TEST EAX,EAX; store default; if null
     * keep default; else 30048dd9 MOV EAX,[weapon+0x90]; CMP 3/4/5; any -> mode-A. */
    float chance = cg_tracerchance_vmCvar.value;
    {
        const weaponInfo_t *weapon = bg_weaponInfos[weaponIndex];
        if (weapon != 0) {
            weaponAmmoType_t ammoType = weapon->ammoType;   /* +0x90 */
            if (ammoType == WEAPON_AMMO_TYPE_LMG ||
                ammoType == WEAPON_AMMO_TYPE_HMG ||
                ammoType == WEAPON_AMMO_TYPE_UMG) {
                chance = cg_tracerchancelmg_vmCvar.value;
            }
        }
    }

    /* 30048df8..30048e16: roll the spawn probability.
     *   r = rand();                         (0..0x7fff)
     *   if (!((float)r < chance * 32768.0f)) -> whiz-by test only.
     * 30048df8 CALL rand; store r; 30048e01 FILD r; 30048e05 FLD chance;
     * 30048e09 FMUL [32768.0 @0x3007bd10]; 30048e0f FCOMPP; TEST AH,0x41;
     * 30048e16 JNZ 0x30048e4e (r >= chance*32768 -> skip tracer). */
    int32_t r = (int32_t)coduo_crt_rand();
    if (!((float)r < chance * 32768.0f)) {
        CG_WhizbySound(impactOrigin, muzzle);
        return;
    }

    /* 30048e18: dispatch on surfaceType. Value 7 spawns a direct moving-tracer
     * polygon; every other value spawns a line-tracer local entity. EBX is the
     * impact point supplied by
     * CG_BulletHitEvent, while the local vector is the resolved muzzle point.
     * The two callees use opposite register orders, as proved below. */
    if (surfaceType == CG_TRACER_DIRECT_SURFACE_TYPE) {
        /* 30048e20 XOR EDX,EDX; 30048e22 MOV ECX,impactOrigin;
         * 30048e24 LEA EAX,&muzzle;
         * PUSH EDI(weaponIndex); CALL 0x30048a00; ADD ESP,4.
         * CG_SpawnMovingTracer consumes EAX as the tracer start and ECX as its
         * destination, so this path runs from muzzle to impact. */
        CG_SpawnMovingTracer(muzzle, impactOrigin, 0, weaponIndex);
    } else {
        /* 30048e40 MOV EAX,impactOrigin; 30048e42 LEA ECX,&muzzle;
         * PUSH EDI(weaponIndex); CALL 0x30048260; ADD ESP,4.
         * CG_SpawnTracerLine consumes EAX as its destination and ECX as
         * trBase. The previous reconstruction reversed these two arguments,
         * making ordinary tracer animations travel from impact to muzzle. */
        CG_SpawnTracerLine(impactOrigin, muzzle, weaponIndex);
    }

    /* 30048e36 / 30048e54: EAX=impactOrigin, EBX=&muzzle;
     * CALL 0x300480f0. */
    CG_WhizbySound(impactOrigin, muzzle);
}
