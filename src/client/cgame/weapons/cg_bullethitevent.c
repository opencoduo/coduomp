// Source: uo_cgame_mp_x86.dll 0x30048e60..0x3004905d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30048e60_3004905d.mcode
//
// CG_BulletHitEvent — the client-side bullet fire/impact event handler: it plays the
// surface impact sound and impact/blood effects at the hit point, then spawns the
// bullet tracer from the correct muzzle tag (a plain-weapon "tag_flash", or, for a
// vehicle-mounted weapon, "tag_altfire" / "tag_secondary_flash").
//
// NAME: the function names itself. Its sole Com_PrintMessage argument (0x30049013) is
// the .rdata string at 0x3007a7f4, "CG_BulletHitEvent: unknown vehicle position\n"
// (dumped via objdump -s -j .rdata; referenced ONLY here), printed when a vehicle
// weapon reports a mount position that is neither 1 nor 2. The .mcode's mechanical
// size-guess name "RegisterItem" (win 0x1fd == a game_mp_uo function size) is REJECTED
// per naming policy — this is a cgame bullet-effect event, not an item registrar.
//
// 0x30085eec / 0x30085efc / 0x30085f00 IDENTITY: these are NOT a second syscall
// dispatcher. They are three .data slots holding const char* pointers into .rdata:
//   [0x30085eec] = 0x300772c0 -> "tag_flash"            (plain-weapon muzzle tag)
//   [0x30085efc] = 0x3007ac10 -> "tag_altfire"          (vehicle mount position 1)
//   [0x30085f00] = 0x3007abfc -> "tag_secondary_flash"  (vehicle mount position 2)
// Each is forwarded verbatim as the tagName argument to CG_SpawnTracer. (The single
// runtime trap dispatcher used by this function is [0x30085e9c] = cgame_syscall.)
//
// Callees (identities reused per provisional-decl policy; call shapes re-derived from
// THIS body's bytes):
//   0x3002ca80 CG_PlaySoundAliasByName(void *channelObj, const char *soundName,
//     int entityNum): ECX=channelObj=&origin, EAX=soundName=surface bullet-sound handle,
//     pushed stack arg=0x3fe (the fixed local sound/loop number). Result discarded.
//   [0x30085e9c] cgame_syscall(CG_PLAY_EFFECT_ORIENTED=0xe8, qhandle_t effectHandle,
//     const vec3_t origin, const vec3_t dir): plays an oriented impact effect. Called
//     up to twice — the primary impact effect and the secondary (blood/debris) effect.
//   0x30048d60 CG_SpawnTracer(vec3_t impactOrigin, int entityNum, int weaponIndex,
//     int surfaceType, const char *tagName): EAX=impactOrigin=origin, ESI=entityNum,
//     EDI=weaponIndex, stack=[surfaceType, tagName]; ADD ESP,8 (caller-cleaned).
//   0x3002b2b0 Com_PrintMessage(const char *fmt, ...): the "unknown vehicle position"
//     diagnostic (single pushed const char*; ADD ESP,4).
//
// .rdata string constant (dumped via objdump -s -j .rdata; never guessed):
//   0x3007a7f4 = "CG_BulletHitEvent: unknown vehicle position\n"
//
// Globals:
//   0x30134cd8 bg_weaponInfos[]                — the weaponInfo_t* array; [weaponIndex] gives
//                                                the firing weapon record.
//   0x3044bf10 cg_bulletSmallSurfaceSounds[23]/0x3044bf6c cg_bulletLargeSurfaceSounds[23]
//                                              — per-surface bullet impact sound handles.
//   0x3044c274 cg_impactEffects[22][24]        — per-(effectType,surface) impact effect
//                                                handles; the primary effect is read from
//                                                row `fxRow` and the secondary from row
//                                                `fxRow + 6` (base 0x3044c4b4 == +6 rows).
//   0x3044cab4 cgs_media_fleshImpactEffect     — the no-blood flesh impact effect.
//   0x3052ec8c cg_blood_vmCvar.integer                        — cg_blood_vmCvar.integer cvar mirror (0 == blood off).
//   0x3048c6e0 cg_entities[] (centity_t, stride 0x288); the head resolves the
//              linked vehicle entity through this typed base.
//
// ABI (proven from the two call sites 0x3002375e and 0x30045b72; both push 8 dwords and
// clean 0x20 after the call — plain cdecl, caller-cleaned). The eight ordered arguments:
//   arg0 fireEntityNum         [callee ESP+0x18] : firing entity number (indexes cg_entities)
//   arg1 origin                [callee ESP+0x1c] : impact/effect world origin & sound channel
//   arg2 effectDir1            [callee ESP+0x20] : direction for the primary impact effect
//   arg3 effectDir2            [callee ESP+0x24] : direction for the secondary impact effect
//   arg4 weaponIndex           [callee ESP+0x28] : index into bg_weaponInfos[]
//   arg5 surfaceType           [callee ESP+0x2c] : impacted surface type (0..22);
//                                                  forwarded to CG_SpawnTracer
//   arg6 linkedEntitySlotPlus1 [callee ESP+0x30] : (linked-vehicle slot + 1), or 0; in/out
//                                                  scratch, also recomputed by the head
//   arg7 vehicleMountPos       [callee ESP+0x34] : vehicle weapon mount position (1/2), or 0
//                                                  to derive it from the firing entity

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>
#include <string.h>

/* cg_entities[] base (centity_t, stride 0x288) at 0x3048c6e0 — the same typed
 * view CG_SpawnTracer/CG_CalcMuzzlePoint use. */

/* cg_tagFlashName / cg_tagAltfireName / cg_tagSecondaryFlashName (the .data pointer
 * slots at 0x30085eec/0x30085efc/0x30085f00) are declared in client_recovered.h. */

/* Fixed local sound/loop number used for the impact sound (0x3fe pushed at 0x30048f48). */
#define CG_LOCAL_SOUND_ENTITY 0x3fe

/* Firing weapon's low fire-mode/effect-class field (centity_t.stateFilter, +0x88):
 * its low three bits select the vehicle mount position when the caller passes 0. */
#define CG_FIRE_MODE_MASK 0x7

enum {
    CG_BULLET_SURFACE_SOUND_COUNT = sizeof(cg_bulletSmallSurfaceSounds) / sizeof(cg_bulletSmallSurfaceSounds[0])
};

_Static_assert(CG_BULLET_SURFACE_SOUND_COUNT == sizeof(cg_bulletLargeSurfaceSounds) / sizeof(cg_bulletLargeSurfaceSounds[0]),
               "bullet surface sound rows differ in size");
_Static_assert(CG_BULLET_SURFACE_SOUND_COUNT <= CG_IMPACT_SURFACE_TYPES, "bullet sound domain exceeds impact effect domain");

void CG_BulletHitEvent(int32_t fireEntityNum, vec3_t origin, vec3_t effectDir1, vec3_t effectDir2, int32_t weaponIndex, int32_t surfaceType,
                       int32_t linkedEntitySlotPlus1, int32_t vehicleMountPos)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if (weaponIndex <= 0 || weaponIndex > bg_numWeapons || (uint32_t)weaponIndex >= (uint32_t)MAX_WEAPONS ||
        bg_weaponInfos[weaponIndex] == NULL) {
        Com_Printf("WARNING: CG_BulletHitEvent: invalid weapon index %i\n", weaponIndex);
        return;
    }
    if ((uint32_t)surfaceType >= (uint32_t)CG_BULLET_SURFACE_SOUND_COUNT) {
        Com_Printf("WARNING: CG_BulletHitEvent: invalid surface type %i\n", surfaceType);
        return;
    }

    centity_t *fireEnt = cgame_compat_unchecked_cgentity(fireEntityNum);

    /* 30048e6c..30048edf: resolve `linkedVehicle`, the vehicle entity whose mounted
     * weapon fired (0 when this is a plain hand-weapon shot). */
    centity_t *linkedVehicle = 0;
    if (fireEnt->currentValid == 0) {
        /* 30048ec8: no DObj on the firing entity — only use a caller-supplied slot. */
        if (linkedEntitySlotPlus1 != 0) {
            /* 30048ed0: linkedEntitySlotPlus1 is (slot + 1). */
            linkedVehicle = cgame_compat_unchecked_cgentity(coduo_int32_from_bits((uint32_t)linkedEntitySlotPlus1 - 1u));
        }
    } else if (fireEnt->currentState.eType == ET_VEHICLE) {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if (linkedEntitySlotPlus1 == 0) {
            Com_Printf("WARNING: CG_BulletHitEvent: vehicle event has no linked entity\n");
            return;
        }

        linkedVehicle = cgame_compat_unchecked_cgentity(coduo_int32_from_bits((uint32_t)linkedEntitySlotPlus1 - 1u));
    } else if ((fireEnt->currentState.eFlags & 0x100000u) != 0u && fireEnt->currentState.vehicleEntityNum != 0x3ff) {
        /* 30048e87..30048eae: a live vehicle-linked weapon (eFlags bit 0x100000 set,
         * vehicleEntityNum not the 0x3ff sentinel). Suppress when the fire-mode field
         * marks a state that must not spawn a linked tracer: (stateFilter & 0x38)==0x8
         * AND (stateFilter & 7)==3. */
        int32_t stateFilter = fireEnt->currentState.stateFilter;
        if (!(((stateFilter & 0x38) == 0x8) && ((stateFilter & CG_FIRE_MODE_MASK) == 3))) {
            /* 30048eb0: the linked vehicle is cg_entities[vehicleEntityNum]; remember its
             * (slot + 1) for the tracer entity-number selection below. */
            int32_t slot = fireEnt->currentState.vehicleEntityNum;
            linkedVehicle = cgame_compat_unchecked_cgentity(slot);
            linkedEntitySlotPlus1 = coduo_int32_from_bits((uint32_t)slot + 1u);
        }
    }

    /* 30048ee0..30048f2b: pick the surface impact sound and the two impact-effect handles.
     * fxRow = weapon->animTreeClass (+0x90), the projectile/impact render class. */
    weaponInfo_t *weapon = bg_weaponInfos[weaponIndex];
    int32_t fxRow = weapon->ammoType; /* weaponInfo_t +0x90: projectile/impact class */

    const char *bulletSound;
    if (fxRow == 1 || fxRow == 0) {
        bulletSound = cg_bulletSmallSurfaceSounds[surfaceType];
    } else {
        bulletSound = cg_bulletLargeSurfaceSounds[surfaceType];
    }

    /* 30048f18..30048f27: flat index = surfaceType + fxRow*24 into cg_impactEffects.
     * The primary effect is that entry; the secondary (blood/debris) effect is the same
     * column six rows further on (base 0x3044c4b4 == &cg_impactEffects[0][0] + 6*24). */
    int32_t flatIndex = coduo_int32_from_bits((uint32_t)fxRow * (uint32_t)CG_IMPACT_SURFACE_TYPES + (uint32_t)surfaceType);
    qhandle_t primaryEffect = (qhandle_t)cgame_compat_read_target_i32_index(&cg_impactEffects[0][0], flatIndex);
    qhandle_t secondaryEffect = (qhandle_t)cgame_compat_read_target_i32_index(
        &cg_impactEffects[0][0], coduo_int32_from_bits((uint32_t)flatIndex + 6u * CG_IMPACT_SURFACE_TYPES));

    /* 30048f2d..30048f42: with blood disabled, a flesh hit (surface 7) uses the no-blood
     * flesh impact effect and drops the secondary blood effect. */
    if (cg_blood_vmCvar.integer == 0 && surfaceType == 7) {
        primaryEffect = cgs_media_fleshImpactEffect;
        secondaryEffect = 0;
    }

    /* 30048f44..30048f52: play the surface bullet-impact sound at the hit point. */
    CG_PlaySoundAliasByName(CG_LOCAL_SOUND_ENTITY, origin, bulletSound);

    /* 30048f55..30048f6f: primary oriented impact effect. */
    if (primaryEffect != 0) {
        cgame_syscall(CG_PLAY_EFFECT_ORIENTED, coduo_int32_from_bits((uint32_t)primaryEffect), (intptr_t)origin, (intptr_t)effectDir1);
    }

    /* 30048f72..30048f8c: secondary oriented impact effect. */
    if (secondaryEffect != 0) {
        cgame_syscall(CG_PLAY_EFFECT_ORIENTED, coduo_int32_from_bits((uint32_t)secondaryEffect), (intptr_t)origin, (intptr_t)effectDir2);
    }

    /* 30048f8f..30049025: vehicle-mounted-weapon tracer. Only when a linked vehicle
     * exists that has a DObj and is an ET_VEHICLE. */
    if (linkedVehicle != 0) {
        /* 30048f97..30048fa9: the linked vehicle must be a renderable ET_VEHICLE. */
        if (linkedVehicle->currentValid == 0 || linkedVehicle->currentState.eType != ET_VEHICLE) {
            return;
        }

        /* 30048faf..30048fc2: resolve the mount position (0 -> derive from the firing
         * entity's fire-mode field). */
        int32_t mountPos = vehicleMountPos;
        if (mountPos == 0) {
            mountPos = fireEnt->currentState.stateFilter & CG_FIRE_MODE_MASK;
        }

        /* The tracer's entity number is the linked-vehicle slot (or the firing entity's
         * vehicleEntityNum when the caller did not supply a slot). */
        int32_t tracerEntityNum;
        if (linkedEntitySlotPlus1 != 0) {
            tracerEntityNum = coduo_int32_from_bits((uint32_t)linkedEntitySlotPlus1 - 1u);
        } else {
            tracerEntityNum = fireEnt->currentState.vehicleEntityNum;
        }

        if (mountPos == 2) {
            /* 30048fc9..30048ff7: secondary mount — "tag_secondary_flash". */
            CG_SpawnTracer(origin, tracerEntityNum, weaponIndex, surfaceType, cg_muzzleTagNames[5]);
            return;
        }
        if (mountPos == 1) {
            /* 30048ffd..30049011: primary/alt mount — "tag_altfire". Falls through to the
             * shared CG_SpawnTracer call below with tracerEntityNum as the entity. */
            CG_SpawnTracer(origin, tracerEntityNum, weaponIndex, surfaceType, cg_muzzleTagNames[4]);
            return;
        }

        /* 30049013..30049025: a vehicle weapon reported an unknown mount position. */
        Com_PrintMessage("CG_BulletHitEvent: unknown vehicle position\n");
        return;
    }

    /* 30049026..30049035: plain hand-weapon shot. A weapon whose projectile class
     * (weaponInfo_t.weaponType, +0x7c) is 1 or 2 emits no tracer. */
    if (weapon->weaponType == 2 || weapon->weaponType == 1) {
        return;
    }

    /* 30049037..30049054: spawn the ordinary "tag_flash" tracer for this shot, keyed on
     * the firing entity number. */
    CG_SpawnTracer(origin, fireEntityNum, weaponIndex, surfaceType, cg_muzzleTagNames[0]);
}
