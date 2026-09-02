// Source: uo_cgame_mp_x86.dll 0x30049060..0x300490a2
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30049060_300490a2.mcode
//
// CG_ModelEventFireWeapon — the weapon-fire visual/audio side effect for a model
// sub-entity event. Sole caller is CG_EntityPreEvent (0x30023690) on its
// EV_BULLET_HIT_CLIENT_SMALL / EV_BULLET_HIT_CLIENT_LARGE path (event ids 179/180), which
// share one handler target. This routine does two things:
//   1) plays the surface-appropriate bullet-impact sound, and
//   2) spawns the bullet tracer for the shot.
//
// NAME ADJUDICATION: the .mcode's mechanical `# name trap_FS_GetFileList` is a pure
// size-match guess (win 0x42 == some 0x42-byte PPC routine) and is REJECTED. This
// body issues NO filesystem trap: there is no CALL [0x30085e9c] with an FS id here —
// it selects a pre-registered sound handle from a surface-indexed table, calls
// CG_PlaySoundAliasByName, then CG_SpawnTracer. The provisional role name
// CG_ModelEventFireWeapon (already recorded in client_recovered.h from the caller) is
// kept; exact CoD symbol unproven.
//
// REGISTER-ARGUMENT ABI (proven from the sole call site 0x30023786 and this body):
//   EAX   -> event            (179 == EV_BULLET_HIT_CLIENT_SMALL selects the "small" pool;
//                              anything else, i.e. 180, selects the "large" pool)
//   ECX   -> poseType         (surface-type index 0..22 into the sound tables)
//   EDI   -> weaponIndex      (model->weaponIndex, forwarded to CG_SpawnTracer)
//   EBX   -> lerpOrigin     (&cent->lerpOrigin at cent+0x208; the sound channel
//                              object AND the tracer impact origin)
//   stack -> vehicleEntityNum (one caller-cleaned stack arg; the tracer entityNum)
//
// The two 23-entry handle tables are registered once by
// CG_RegisterSurfaceTypeSounds(table, "bullet_small"/"bullet_large") at 0x3002b7ff /
// 0x3002b80e: cg_bulletSmallSurfaceSounds / cg_bulletLargeSurfaceSounds, each holding
// one canonical sound-alias name per surface type. The sibling reader at 0x30048f00
// selects between the exact same two tables and forwards the same 0x3fe channel id to
// CG_PlaySoundAliasByName.
//
// Register/stack trace:
//   30049060 PUSH ECX                 ; reserve/save the poseType slot (frame align)
//   30049061 CMP EAX,0xb3             ; event == 179 ?
//   30049066 PUSH ESI
//   30049067 MOV ESI,ECX              ; ESI = poseType (surface index)
//   30049069 JNZ 0x30049074           ; not 179 -> use the "large" pool
//   3004906b MOV EAX,[ESI*4+cg_bulletSmallSurfaceSounds] ; small-bullet impact handle
//   30049072 JMP 0x3004907b
//   30049074 MOV EAX,[ESI*4+cg_bulletLargeSurfaceSounds] ; large-bullet impact handle
//   3004907b PUSH 0x3fe               ; entityNum arg for CG_PlaySoundAliasByName (1022)
//   30049080 MOV ECX,EBX              ; channelObj = &cent->lerpOrigin
//   30049082 CALL 0x3002ca80          ; CG_PlaySoundAliasByName(channelObj, handle, 0x3fe)
//   30049087 MOV EAX,[0x30085eec]     ; cg_muzzleTagNames[0] == "tag_flash"
//   3004908c ADD ESP,4                ; balance the pushed 0x3fe
//   3004908f PUSH EAX                 ; CG_SpawnTracer stack arg1 = tagName
//   30049090 PUSH ESI                 ; CG_SpawnTracer stack arg0 = surfaceType (poseType)
//   30049091 MOV ESI,[ESP+0x14]       ; ESI = vehicleEntityNum (the caller stack arg)
//   30049095 MOV EAX,EBX              ; impactOrigin = &cent->lerpOrigin
//   30049097 CALL 0x30048d60          ; CG_SpawnTracer(impactOrigin, entityNum,
//                                      ;   weaponIndex(EDI), surfaceType, tagName)
//   3004909c ADD ESP,8                ; balance the two tracer stack args
//   3004909f POP ESI
//   300490a0 POP ECX
//   300490a1 RET                      ; caller cleans vehicleEntityNum (cdecl-ish)
//
// ABI NOTE: CG_PlaySoundAliasByName's EAX slot is typed `const char *soundName`, but here
// it receives a pre-registered asset handle (qhandle_t) rather than a name string —
// the callee re-passes it to the sound-register trap CG_COM_PICK_SOUND_ALIAS, which accepts the
// already-registered handle. The int handle is passed through the pointer slot; this
// matches the sibling reader at 0x30048f00 that feeds the same tables to the same
// call. lerpOrigin serves double duty as both the sound channel object and the
// tracer start origin (both are the same cent+0x208 address).

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

enum {
    CG_MODEL_EVENT_SURFACE_SOUND_COUNT = sizeof(cg_bulletSmallSurfaceSounds) / sizeof(cg_bulletSmallSurfaceSounds[0])
};

_Static_assert(CG_MODEL_EVENT_SURFACE_SOUND_COUNT == sizeof(cg_bulletLargeSurfaceSounds) / sizeof(cg_bulletLargeSurfaceSounds[0]),
               "model-event bullet sound rows differ in size");

void CG_ModelEventFireWeapon(int32_t event, uint32_t poseType, uint32_t weaponIndex, vec3_t lerpOrigin, int32_t vehicleEntityNum)
{
    const char *impactSound;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if (weaponIndex == 0 || weaponIndex >= (uint32_t)MAX_WEAPONS || bg_numWeapons < 1 || weaponIndex > (uint32_t)bg_numWeapons ||
        bg_weaponInfos[weaponIndex] == NULL) {
        Com_Printf("WARNING: CG_ModelEventFireWeapon: invalid weapon index %u\n", weaponIndex);
        return;
    }
    if (poseType >= (uint32_t)CG_MODEL_EVENT_SURFACE_SOUND_COUNT) {
        Com_Printf("WARNING: CG_ModelEventFireWeapon: invalid surface type %u\n", poseType);
        return;
    }

    /* 0x30049061/0x3004906b/0x30049074: event 179 (EV_BULLET_HIT_CLIENT_SMALL) plays the
     * small-bullet impact sound for this surface type; any other id (180) plays the
     * large-bullet one. */
    if (event == 179) {
        impactSound = cg_bulletSmallSurfaceSounds[poseType];
    } else {
        impactSound = cg_bulletLargeSurfaceSounds[poseType];
    }

    /* 0x3004907b..0x30049082: play the impact sound on the entity's weapon-angles
     * channel object. The registered handle occupies the soundName pointer slot (see
     * ABI NOTE); 0x3fe (1022) is the fixed entityNum passed by this and the sibling
     * caller at 0x30048f4d. */
    CG_PlaySoundAliasByName(0x3fe, lerpOrigin, impactSound);

    /* 0x30049087..0x30049097: spawn the bullet tracer from the muzzle. The impact is
     * the same weapon-angles context; the surface index doubles as the tracer's
     * surfaceType; the muzzle bone tag is cg_muzzleTagNames[0] ("tag_flash"). */
    CG_SpawnTracer(lerpOrigin, vehicleEntityNum, (int32_t)weaponIndex, (int32_t)poseType, cg_muzzleTagNames[0]);
}
