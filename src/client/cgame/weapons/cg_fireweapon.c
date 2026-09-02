// Source: uo_cgame_mp_x86.dll 0x30047d20..0x30048044
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30047d20_30048044.mcode
//
// CG_FireWeapon — run one client weapon-fire event: choose the active weapon,
// apply local first-person recoil, play the muzzle effect and fire sound from the
// correct DObj/tag, arm the looping-fire sound lifetime, and eject brass.
//
// The .mcode header's PM_UpdateLean assignment is rejected: it was a same-size
// collision. The embedded "CG_FireWeapon: ent->weapon > BG_GetNumWeapons()"
// diagnostic names this routine, and every side effect is a fire-event effect.
//
// Original i386 ABI: packed flags/weapon override in EAX; four caller-cleaned
// stack arguments (cent, model, event, muzzleTagIndex).

#include "../client_recovered.h"
#include "../globals.h"

enum {
    CG_FIRE_EVENT_ALT = 0xa4,
    CG_FIRE_EVENT_SECONDARY = 0xa5,
    CG_FIRE_EVENT_LAST_SHOT = 0xa6,
    CG_FIRE_DRAW_TAG_MODEL = 0x80,
    CG_TURRET_MUZZLE_POSE_MASK = 0x38,
    CG_TURRET_MUZZLE_POSE = 0x08,
    CG_TURRET_SEAT_MASK = 0x07,
    CG_TURRET_SEAT_GUNNER = 0x03,
    CG_LOCAL_VIEW_WEAPON_MODE = 2,
    CG_FIRE_LOOP_TAIL_MS = 51
};

void CG_FireWeapon(uint32_t packedWeapon, centity_t *cent, entityState_t *model, int32_t event, int32_t muzzleTagIndex)
{
    int32_t weaponIndex = (int32_t)(packedWeapon & ~(uint32_t)CG_FIRE_DRAW_TAG_MODEL);
    qboolean drawTagModel = (packedWeapon & CG_FIRE_DRAW_TAG_MODEL) ? qtrue : qfalse;
    cgWeaponInfo_t *weaponVisual;
    weaponInfo_t *weapon;
    qboolean localViewEffect = qfalse;
    qboolean playMuzzleEffect = qtrue;
    int32_t effectModel = (int32_t)model->numberBits;
    const char *muzzleTag;

    /* 0x30047d38..0x30047d98: a positive packed value after clearing bit 7
     * overrides the model's weapon index. Otherwise validate and use
     * model->weapon. */
    if (weaponIndex <= 0) {
        weaponIndex = (int32_t)model->weaponIndex;
        if (weaponIndex == 0) {
            return;
        }
        if (weaponIndex > bg_numWeapons || (uint32_t)weaponIndex >= (uint32_t)MAX_WEAPONS) {
            Com_ErrorMessage("CG_FireWeapon: ent->weapon > BG_GetNumWeapons()");
            return;
        }
    } else {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if (weaponIndex > bg_numWeapons || (uint32_t)weaponIndex >= (uint32_t)MAX_WEAPONS || bg_weaponInfos[weaponIndex] == NULL) {
            Com_Printf("WARNING: CG_FireWeapon: invalid weapon override %i\n", weaponIndex);
            return;
        }
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if (bg_weaponInfos[weaponIndex] == NULL) {
        Com_Printf("WARNING: CG_FireWeapon: unregistered weapon index %i\n", weaponIndex);
        return;
    }

    weaponVisual = &cg_weaponInfos[weaponIndex];
    weapon = bg_weaponInfos[weaponIndex];

    /* Gas weapons only stamp their fire time and skip every ordinary muzzle/
     * sound/brass effect. */
    if (weapon->weaponType == WEAPTYPE_GAS) {
        cent->gasFireTime = (int32_t)cg_time;
        return;
    }

    cent->weaponEffectActive = qtrue;

    /* Local first-person fire applies random weapon view kick/recoil. */
    if ((cg_snap->ps.playerStateFlags & PSF_PLAYER_ENTITY_MASK) != 0 && (int32_t)model->numberBits == cg_snap->ps.psClientNum) {
        BG_WeaponFireRecoil(&cg_predictedPlayerState, cg_weaponRecoilAngles, cg_viewKickVel);
    }

    /* 0x30047e08..0x30047f0d: choose the DObj context and muzzle tag. Turrets use
     * the indexed tag directly. A vehicle-mounted non-gunner pose redirects to
     * the vehicle entity's DObj, and vehicle events use the vehicle model.
     * Ordinary player events jump from 0x30047e8c directly to 0x30047f0d and do
     * not play an immediate model-tag effect; CG_AddPlayerWeapon consumes the
     * weaponEffectActive latch and emits their view/world muzzle effect. */
    if (model->eType == ET_TURRET) {
        muzzleTag = cg_muzzleTagNames[muzzleTagIndex];
        localViewEffect = (cg_snap->ps.viewLockedEntityNum == (int32_t)model->numberBits) ? qtrue : qfalse;
    } else {
        centity_t *effectEntity = NULL;

        if ((int32_t)model->numberBits < MAX_CLIENTS_IN_SNAPSHOT && (model->eFlags & EF_IN_VEHICLE) != 0 &&
            (((model->poseType & CG_TURRET_MUZZLE_POSE_MASK) != CG_TURRET_MUZZLE_POSE) ||
             ((model->poseType & CG_TURRET_SEAT_MASK) != CG_TURRET_SEAT_GUNNER))) {
            localViewEffect = (cg_snap->ps.viewLockedEntityNum == (int32_t)model->numberBits) ? qtrue : qfalse;
            effectEntity = &cg_entities[model->vehicleEntityNum];
            if (effectEntity->currentValid == 0) {
                effectEntity = NULL;
                playMuzzleEffect = qfalse;
            }
        } else if (cent->currentState.eType == ET_VEHICLE) {
            effectEntity = (centity_t *)model;
            if (cg_snap->ps.viewLockedEntityNum == (int32_t)model->numberBits) {
                localViewEffect =
                    ((cg_entities[cg_snap->ps.psClientNum].currentState.stateFilter & 7) == CG_LOCAL_VIEW_WEAPON_MODE) ? qtrue : qfalse;
            }
        } else {
            /* 0x30047e88..0x30047f0d: ordinary player weapons skip the
             * immediate model-tag effect and leave the latched view/world
             * effect to CG_AddPlayerWeapon. */
            playMuzzleEffect = qfalse;
        }

        if (event == CG_FIRE_EVENT_ALT) {
            muzzleTag = cg_muzzleTagNames[4];
        } else if (event == CG_FIRE_EVENT_SECONDARY) {
            muzzleTag = cg_muzzleTagNames[5];
        } else {
            muzzleTag = cg_muzzleTagNames[muzzleTagIndex];
        }

        if (effectEntity != NULL) {
            /* The first dword of either accepted context is the DObj/model id.
             * For the vehicle centity this is currentState.number; for the model
             * record it is modelPartIndex. */
            effectModel = effectEntity->currentState.number;
        } else {
            effectModel = 0;
        }
    }

    if (playMuzzleEffect) {
        CG_PlayFxOnWeaponTag(localViewEffect, weaponIndex, effectModel, cent->lerpOrigin, muzzleTag, drawTagModel);
    }

    /* A looping fire sound arms the centity timer for weapon fireTime plus the
     * fixed 51ms tail. */
    if (weaponVisual->loopFireSound != 0) {
        cent->flashSoundLifetime = coduo_int32_from_bits((uint32_t)weapon->fireTime + (uint32_t)CG_FIRE_LOOP_TAIL_MS);
    }

    {
        const char *sound = weaponVisual->fireSound;
        vec3_t soundOrigin;
        DObjSkelMat tagMatrix;
        qboolean haveTagOrigin = qfalse;

        if (weaponVisual->lastShotSound != 0 && event == CG_FIRE_EVENT_LAST_SHOT) {
            sound = weaponVisual->lastShotSound;
        }

        if (sound != 0) {
            if ((cg_snap->ps.playerStateFlags & PSF_PLAYER_ENTITY_MASK) != 0 && (int32_t)model->numberBits == cg_snap->ps.psClientNum) {
                if (weaponVisual->viewDObjSelf != 0) {
                    haveTagOrigin = CG_DObjGetSpecialTagWorldMatrix(weaponVisual->viewDObjSelf, "tag_flash", &tagMatrix);
                }
            } else {
                void *self = (void *)(intptr_t)cgame_syscall(CG_DOBJ_GET_HANDLE, model->numberBits);
                if (self != NULL) {
                    haveTagOrigin = CG_DObjGetWorldTagMatrix(self, "tag_flash", cent, &tagMatrix);
                }
            }

            if (haveTagOrigin) {
                soundOrigin[0] = tagMatrix.origin[0];
                soundOrigin[1] = tagMatrix.origin[1];
                soundOrigin[2] = tagMatrix.origin[2];
            } else {
                BG_EvaluateTrajectory(&model->pos, (int32_t)cg_time, soundOrigin);
            }

            (void)CG_PlaySoundAliasByName((int32_t)model->numberBits, soundOrigin, sound);
        }
    }

    /* Bolt-action weapons suppress automatic brass ejection here. */
    if (weapon->raiseEnabled == 0) {
        CG_EjectWeaponBrass(model, event);
    }
}
