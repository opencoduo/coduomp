// Source: uo_cgame_mp_x86.dll 0x300490b0..0x300491df
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300490b0_300491df.mcode
//
// CG_WeaponUpdateLoopingSound (0x300490b0) — per-frame emitter for
// a firing entity's muzzle-flash sound. For the weapon the effect entity currently
// holds (cg_weaponInfos[cent->currentState.weapon]), it plays a positional "fire" sound at
// the weapon's tag_flash bone. The remaining-lifetime timer (cent->flashSoundLifetime,
// +0x204) selects which sound and counts the emission down:
//   * while the timer is > 0 it decrements by cg_frametime and plays the looping
//     fire sound (cgWeaponInfo.loopFireSound, +0x128);
//   * on the frame the timer reaches <= 0 it plays the one-shot tail sound
//     (cgWeaponInfo.stopFireSound, +0x12c) instead.
// The 3D sound position is the tag_flash world point: for the LOCAL predicted player
// (cent->currentState.number == cg_snap->ps.psClientNum) it uses the view-weapon DObj special-tag
// matrix (CG_DObjGetSpecialTagWorldMatrix on cgWeaponInfo.viewDObjSelf); for any
// other entity it queries the entity's DObj handle (trap CG_DOBJ_GET_HANDLE) and
// builds the entity-bone world matrix (CG_DObjGetWorldTagMatrix). If neither
// tag resolves, the sound position falls back to the entity's trajectory position
// (BG_EvaluateTrajectory of currentState.pos at cg_time).
//
// The .mcode pre-hint name "PlayerCmd_SetWeaponSlotClipAmmo" is REJECTED: it is a
// broad server-side corpus/size guess (win size 0x12f vs matched 0x130) with no
// support in the machine code. There is no clip/ammo state work here at all — the
// body resolves the tag_flash bone matrix and plays weapon sounds. Name assigned by
// proven behavior (DObj tag_flash resolution + CG_PlaySoundAliasByName of the per-weapon
// flash sounds). The Mac cgame symbol CG_WeaponUpdateLoopingSound has the identical
// two named direct callees, resolving the source name. See client_recovered.h /
// globals.h for the shared declarations this touches.
//
// ABI: the effect entity `cent` (a centity_t*) is the sole stack argument
// ([ESP+0x5c] after the SUB 0x50 frame and the ebx/ebp/esi pushes; MOV EBP,[ESP+0x5c]).
// Plain cdecl RET (caller-cleaned), no return value.
//
// Machine-code self-check (0x300490b0..0x300491df):
//   - 0x300490ba IMUL cent->currentState.weapon,0x1c4; ADD 0x30413580 -> wi (cgWeaponInfos[]).
//   - 0x300490cc gate on wi->loopFireSound (+0x128) != 0 (JZ end).
//   - 0x300490db gate on cent->flashSoundLifetime (+0x204) > 0 (JLE end, signed).
//   - 0x300490e9 SUB cg_frametime (raw ms), store back to +0x204.
//   - 0x300490f5/0x300490fe CMP cent->currentState.number(+0x00) vs cg_snap->ps.psClientNum(+0xe0).
//   - local branch: MOV EDI,[wi] (wi->viewDObjSelf); if 0 -> fallback; else
//     CG_DObjGetSpecialTagWorldMatrix(EDI, "tag_flash", &tagMatrix); if 0 -> fallback;
//     else soundPos = tagMatrix translation row (float 12/13/14 at [ESP+0x50/54/58]).
//   - remote branch: handle = trap(CG_DOBJ_GET_HANDLE, cent->currentState.number); if 0 ->
//     fallback; else CG_DObjGetWorldTagMatrix(handle, "tag_flash", cent,
//     &tagMatrix); if 0 -> fallback; else soundPos = tagMatrix translation row.
//   - fallback (0x30049182): BG_EvaluateTrajectory(&cent->currentState.pos,
//     cg_time, soundPos).
//   - 0x30049193 reload cent->flashSoundLifetime; if > 0 play loopFireSound (+0x128),
//     else if wi->stopFireSound (+0x12c) != 0 play stopFireSound. Both via
//     CG_PlaySoundAliasByName(channelObj=&soundPos, soundName, entityNum=cent->currentState.number).

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

/* cg_muzzleFlashTagName ("tag_flash", the muzzle-flash bone/tag on the weapon
 * DObj) is declared in globals.h and used as the tag name below. */

/* currentState.pos (a trajectory_t) sits at cent+0x0c and is modelled as the named
 * centity_t member `currentStatePos` (a union with the
 * currentStateOrigin/laserDir vec3 sub-views its trBase/trDelta overlap). Offset
 * proven by BG_EvaluateTrajectory's LEA EBX,[EBP+0xc] at 0x30049187. */

void CG_WeaponUpdateLoopingSound(centity_t *cent)
{
    /* wi = &cg_weaponInfos[cent->currentState.weapon] (0x300490ba: IMUL idx,0x1c4; ADD base). */
    cgWeaponInfo_t *wi = &cg_weaponInfos[cent->currentState.weapon];

    /* 0x300490cc: no looping fire sound registered for this weapon -> nothing to do. */
    if (wi->loopFireSound == 0)
        return;

    /* 0x300490db: signed test — a non-positive remaining lifetime means the emission
     * has already ended, so skip it (JLE, not JZ). */
    if (cent->flashSoundLifetime <= 0)
        return;

    /* 0x300490e9: count the emission down by this frame's elapsed time (raw ms). */
    cent->flashSoundLifetime = coduo_int32_from_bits(
        (uint32_t)cent->flashSoundLifetime - (uint32_t)cg_frametime);

    /* soundPos: the 3D emission point handed to CG_PlaySoundAliasByName as the channel
     * object (its origin). Written at [ESP+0x14..0x1c] in the machine code. */
    vec3_t soundPos;
    /* Exact DObjSkelMat bone/tag world record at [ESP+0x20]; its origin row at
     * byte +0x30 is the tag_flash world position. */
    DObjSkelMat tagMatrix;
    qboolean haveTagPoint = qfalse;

    /* 0x300490f5/0x300490fe: local predicted player vs remote entity. */
    if (cent->currentState.number == cg_snap->ps.psClientNum) {
        /* 0x30049106: the local view-weapon DObj self handle. */
        struct DObj_s *self = wi->viewDObjSelf;
        if (self != NULL) {
            /* 0x30049116: CG_DObjGetSpecialTagWorldMatrix(self, "tag_flash",
             * &tagMatrix) — EDI=self, EAX=tagName, PUSH &tagMatrix. */
            if (CG_DObjGetSpecialTagWorldMatrix(self, cg_muzzleFlashTagName,
                                                &tagMatrix)) {
                /* 0x30049122-0x30049136: translation row -> soundPos. */
                soundPos[0] = tagMatrix.origin[0];
                soundPos[1] = tagMatrix.origin[1];
                soundPos[2] = tagMatrix.origin[2];
                haveTagPoint = qtrue;
            }
        }
    } else {
        /* 0x3004913c-0x30049148: query this entity's engine DObj handle. */
        struct DObj_s *self = (struct DObj_s *)(intptr_t)cgame_syscall(
            CG_DOBJ_GET_HANDLE, cent->currentState.number);
        if (self != NULL) {
            /* 0x3004915c: CG_DObjGetWorldTagMatrix(self=handle,
             * tagName="tag_flash", entity=cent, out=&tagMatrix) — ECX=handle,
             * EAX=tagName, PUSH &tagMatrix, PUSH cent. */
            if (CG_DObjGetWorldTagMatrix(self, cg_muzzleFlashTagName,
                                         cent, &tagMatrix)) {
                /* 0x30049168-0x3004917c: translation row -> soundPos. */
                soundPos[0] = tagMatrix.origin[0];
                soundPos[1] = tagMatrix.origin[1];
                soundPos[2] = tagMatrix.origin[2];
                haveTagPoint = qtrue;
            }
        }
    }

    if (!haveTagPoint) {
        /* 0x30049182-0x3004918e: no tag point — use the entity's trajectory position
         * at cg_time. result in ECX (&soundPos), tr in EBX (&currentState.pos),
         * atTime in EAX (cg_time). */
        BG_EvaluateTrajectory(&cent->currentState.pos,
                              coduo_int32_from_bits(cg_time), soundPos);
    }

    /* 0x30049193: reload the (already-decremented) remaining lifetime and choose
     * the sound: looping while still alive, tail on the frame it expires. */
    if (cent->flashSoundLifetime > 0) {
        /* 0x300491a0-0x300491ab: CG_PlaySoundAliasByName(&soundPos, wi->loopFireSound,
         * cent->currentState.number). channelObj in ECX (&soundPos), soundName in EAX
         * (wi->loopFireSound), entityNum the single stack arg (cent->currentState.number). */
        CG_PlaySoundAliasByName(cent->currentState.number, soundPos, wi->loopFireSound);
    } else if (wi->stopFireSound != 0) {
        /* 0x300491bb-0x300491cf: same call for the one-shot tail sound, gated on a
         * nonzero stopFireSound handle. */
        CG_PlaySoundAliasByName(cent->currentState.number, soundPos, wi->stopFireSound);
    }
}
