// Source: uo_cgame_mp_x86.dll 0x30045ca0..0x30046487
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30045ca0_30046487.mcode
//
// CG_AddPlayerWeapon — attach/render the held or first-person weapon. For the
// first-person (ps != NULL) path it builds the view-weapon refEntity (RT_MODEL,
// renderfx 0x8c, lightingOrigin from the player origin + viewHeightCurrent with a
// scaled lean/yaw deflection), advances the view DObj's animation by
// cg.frametime seconds plus an in-place trace-part begin/prep/flush pass,
// processes weapon notetracks, offsets the entity origin by the cg_gunX/Y/Z
// cvars along the refdef view axes, optionally submits it to the scene, commits
// the placement to cg_specialTagPlacement, publishes the tag_brass bone's world
// position to cg_brassEffectOrigin, and (for an actively-firing GAS weapon)
// emits flame chunks from the tag_flash bone's world position. The tail
// consumes the centity's weapon-fire effect latch and plays the muzzle-flash
// effect on the resolved tag ("tag_flash" via the cg_muzzleTagNames[0] slot).
// The world-player (ps == NULL) path only updates the looping flash sound and
// runs the latch/muzzle tail; its refEntity is built but never submitted.
// Item_ListBox_Paint was a UI size collision and is rejected by the
// DObj/weapon/centity data flow.

#include "../client_recovered.h"
#include "qcommon/fx_types.h"

#include <string.h>

enum {
    /* Exact source enum names are unresolved. Machine code proves that this
     * vehicle type/position pair is the only disabled-state pose allowed to
     * continue rendering the weapon (30045d21/30045d2e). */
    CG_WEAPON_VEHICLE_TYPE_ALLOWED = 1,
    CG_WEAPON_VEHICLE_POSITION_ALLOWED = 3,

    /* Packed cent->currentState.stateFilter fields selecting the equivalent world-player
     * pose when no playerState is supplied (30045d56..30045d63). */
    CG_WEAPON_POSE_CLASS_MASK = 0x38,
    CG_WEAPON_POSE_CLASS_ALLOWED = 0x08,
    CG_WEAPON_POSE_SUBSTATE_MASK = 0x07,
    CG_WEAPON_POSE_SUBSTATE_ALLOWED = 0x03
};

/* NOT_FROM_ORIGINAL_SOURCE: build the matrix43_t placement (axis rows + origin
 * row) the composer takes as
 * its local operand, from the view-weapon refEntity. Emitted twice in the
 * machine code (30046178..300461ef for tag_brass, 3004628e..30046313 for
 * tag_flash) as 12 plain dword copies: entity axis[0..2] -> rows[0..2], entity
 * origin -> rows[3]. */
static void cgame_compat_weapon_entity_placement(
    const refEntity_t *entity, matrix43_t *placement)
{
    memcpy(placement->axis, entity->axis, sizeof(placement->axis));
    memcpy(placement->origin, entity->origin, sizeof(entity->origin));
}

void CG_AddPlayerWeapon(refEntity_t *parent, playerState_t *ps,
                        centity_t *cent, qboolean viewWeapon,
                        float viewOriginOffset)
{
    /* 30045cc3..30045cee: localFirstPerson = snapshot first-person view flags
     * set AND this centity is the snapshot's own client. */
    qboolean localFirstPerson =
        (cg_snap->ps.playerStateFlags & PSF_PLAYER_ENTITY_MASK) != 0 &&
        cent->currentState.number == cg_snap->ps.psClientNum;
    /* 30045cee: EDI = cent->currentState.weapon (+0xcc). */
    int32_t weaponIndex = cent->currentState.weapon;
    weaponInfo_t *weapon;
    cgWeaponInfo_t *registered;
    refEntity_t weaponEntity;

    /* 30045cf6..30045d0e: with a playerState, a nonzero screen/view transition
     * gate skips everything. */
    if (ps != NULL && g_cgScreenReadyState != 0) {
        return;
    }

    /* 30045d14: TEST cent->currentState.eFlags,0x106000 — disabled-state animation flags. */
    if ((cent->currentState.eFlags & EF_RESTRICTED_MASK) != 0) {
        if (ps != NULL) {
            /* 30045d21: [ps+0x618] != 1 -> return; 30045d2e: [ps+0x614] == 3
             * continues, else return (inline epilogue at 30045d37). */
            if (ps->vehicleType != CG_WEAPON_VEHICLE_TYPE_ALLOWED ||
                ps->vehiclePosition != CG_WEAPON_VEHICLE_POSITION_ALLOWED) {
                return;
            }
        } else if ((cent->currentState.stateFilter & CG_WEAPON_POSE_CLASS_MASK) !=
                       CG_WEAPON_POSE_CLASS_ALLOWED ||
                   (cent->currentState.stateFilter & CG_WEAPON_POSE_SUBSTATE_MASK) !=
                       CG_WEAPON_POSE_SUBSTATE_ALLOWED) {
            /* 30045d4e..30045d66: AND 0x38 == 8 and AND 7 == 3 on stateFilter. */
            return;
        }
    }

    /* 30045d6c: CG_RegisterWeapon(weaponIndex). */
    CG_RegisterWeapon(weaponIndex);
    /* 30045d72/30045d78: weapon = bg_weaponInfos[weaponIndex] (saved [ESP+0x5c]). */
    weapon = bg_weaponInfos[weaponIndex];
    /* 30045d80: EBX = weaponIndex*0x1c4 into the cg_weaponInfos value array. */
    registered = &cg_weaponInfos[weaponIndex];

    /* 30045d86..30045d9b: world players update the looping flash sound. */
    if (ps == NULL) {
        CG_WeaponUpdateLoopingSound(cent);
    }

    /* 30045d9e..30045daf: REP STOSD of 0x27 dwords (0x9c bytes) at [ESP+0xb0]. */
    memset(&weaponEntity, 0, sizeof(weaponEntity));
    /* 30045db1..30045ddf: common copies from the parent entity — renderfx (+0x4),
     * lightingOrigin (+0xc..0x14), shadowPlane (+0x18). Axis and origin are NOT
     * copied here; that happens only inside the ps branch. */
    weaponEntity.renderfx = parent->renderfx;
    memcpy(weaponEntity.lightingOrigin, parent->lightingOrigin,
           sizeof(weaponEntity.lightingOrigin));
    weaponEntity.shadowPlane = parent->shadowPlane;

    /* 30045de6: JZ on (ps != NULL) — the world-player path skips straight to the
     * muzzle-latch tail. */
    if (ps != NULL) {
        /* 30045df2..30045e09: shaderRGBA = {255,255,255,255}. */
        memset(weaponEntity.shaderRGBA, 255, sizeof(weaponEntity.shaderRGBA));
        /* 30045e10/30045e16: view DObj handle into entity +0x90. */
        weaponEntity.dobj = registered->viewDObjSelf;
        /* 30045dec/30045e1d..30045e50: lightingOrigin = player origin with the
         * eye height added on Z (FLD ps->psOrigin[2]; FADD ps->viewHeightCurrent +0xf8). */
        weaponEntity.lightingOrigin[0] = ps->psOrigin[0];
        weaponEntity.lightingOrigin[1] = ps->psOrigin[1];
        weaponEntity.lightingOrigin[2] = (float)(
            (long double)ps->psOrigin[2] +
            (long double)ps->viewHeightCurrent);
        /* 30045e65: reType = 1; 30045e70: renderfx = 0x8c (whole-field write). */
        weaponEntity.reType = RT_MODEL;
        weaponEntity.renderfx = RF_LIGHTING_ORIGIN | RF_DEPTHHACK | RF_FIRST_PERSON;
        /* 30045e20/30045e57..30045e7b: AddLeanToPosition(EDX=&lightingOrigin,
         * ps->viewAngles[1] (+0xec, yaw), ps->leanFraction (+0x44), 16.0f
         * (imm 0x41800000), 20.0f (imm 0x41a00000)). */
        AddLeanToPosition(weaponEntity.lightingOrigin, ps->viewAngles[1],
                              ps->leanFraction, 16.0f, 20.0f);

        /* 30045e80..30045eae: FILD cg.frametime (0x304831ac), FSTP to a float
         * temporary, reload, then FMUL 0.001f (.rdata 0x3007bd94 = 0x3a83126f);
         * trap(0x98, viewDObjSelf, float bits) advances the view DObj animation. */
        {
            float frameTime = (float)(long double)cg_frametime;
            float dtSeconds = (float)(
                (long double)frameTime * (long double)0.001f);
            cgame_syscall(CG_DOBJ_ADVANCE_SERVER_TIME,
                          (intptr_t)registered->viewDObjSelf,
                          CG_FloatBits(dtSeconds));
        }

        /* 30045eb5..30045f14: calculate every view-weapon DObj bone using a
         * four-word part bitset initialized to all ones (OR EDX,-1; four stores
         * [ESP+0x60..0x6c]). CreateSkelForBones (0xac) == 0 -> CalcAnim (0x9a)
         * then CalcSkel (0xae); a nonzero create result skips both. Same sequence
         * as CG_DObjCalcPose but intentionally without entity controllers. */
        {
            uint32_t partBits[4] = {
                UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX
            };
            if (cgame_syscall(CG_DOBJ_CREATE_SKEL_FOR_BONES,
                              (intptr_t)registered->viewDObjSelf,
                              (intptr_t)partBits) == 0) {
                cgame_syscall(CG_DOBJ_CALC_ANIM,
                              (intptr_t)registered->viewDObjSelf,
                              (intptr_t)partBits);
                cgame_syscall(CG_DOBJ_CALC_SKEL,
                              (intptr_t)registered->viewDObjSelf,
                              (intptr_t)partBits);
            }
        }

        /* 30045f17: CALL 0x30042c40. */
        CG_ProcessWeaponNoteTracks();

        /* 30045f1c/30045f36: forward = cg_gunX.value (0x304521e8) +
         * viewOriginOffset (the caller's pullback scalar, [ESP+0x16c]). */
        {
            float forward = cg_gunX_vmCvar.value + viewOriginOffset;
            int32_t i;

            /* 30045f29..30045fec: entity origin (+0x44) and axis (+0x1c) copied
             * from the parent (plain dword copies, interleaved with the math). */
            memcpy(weaponEntity.axis, parent->axis, sizeof(weaponEntity.axis));
            memcpy(weaponEntity.origin, parent->origin, sizeof(weaponEntity.origin));

            /* 30045f41..3004608b: per component i, three separate x87
             * accumulate-and-store steps (each FADD origin[i]; FSTP origin[i]):
             *   origin[i] += forward * viewaxis[0][i]                (0x30487a9c..)
             *   origin[i] += viewaxis[1][i] (0x30487aa8..) * cg_gunY.value (0x30421ae8)
             *   origin[i] += viewaxis[2][i] (0x30487ab4..) * cg_gunZ.value (0x30450268)
             * Kept as three += statements to preserve the rounding order. */
            for (i = 0; i < 3; ++i) {
                weaponEntity.origin[i] += forward * cg_refdef.viewaxis[0][i];
                weaponEntity.origin[i] += cg_refdef.viewaxis[1][i] * cg_gunY_vmCvar.value;
                weaponEntity.origin[i] += cg_refdef.viewaxis[2][i] * cg_gunZ_vmCvar.value;
            }
        }

        /* 30045fae TEST / 30046092 JZ: submit to the scene only when the
         * viewWeapon flag is set (trap 0x3d with &entity, 30046094..300460a4). */
        if (viewWeapon) {
            trap_R_AddRefEntityToScene(&weaponEntity);
        }

        /* 300460a7..3004614a: unconditionally commit the view-weapon placement
         * to cg_specialTagPlacement — origin to 0x3048b0e4/e8/ec, the nine axis
         * floats to 0x3048b0f0..0x3048b110. */
        memcpy(cg_specialTagPlacement.origin, weaponEntity.origin,
               sizeof(cg_specialTagPlacement.origin));
        memcpy(cg_specialTagPlacement.axis, weaponEntity.axis,
               sizeof(cg_specialTagPlacement.axis));

        /* 30046119..30046234: publish the tag_brass bone's world position.
         * trap(0xb2, viewDObjSelf, "tag_brass" @0x3007a8cc); 3004615e JLE skips
         * the block when the bone index is <= -1. */
        {
            int32_t brassBone = coduo_int32_from_bits((uint32_t)cgame_syscall(
                CG_DOBJ_GET_BONE_INDEX,
                (intptr_t)registered->viewDObjSelf,
                (intptr_t)cg_brassEjectTagName));
            if (brassBone > -1) {
                /* 30046164..30046172: bone-matrix table = trap(0xa0, self, 0);
                 * 300461f6/30046204: entry at table + (bone << 6) — a 0x40-byte
                 * (16-float) stride. */
                DObjSkelMat *boneMatrixTable = (DObjSkelMat *)(intptr_t)cgame_syscall(
                    CG_DOBJ_GET_BONE_MATRICES,
                    (intptr_t)registered->viewDObjSelf, 0);
                matrix43_t placement;
                DObjSkelMat worldMatrix;

                /* 30046178..300461ef: placement rows from entity axis+origin. */
                cgame_compat_weapon_entity_placement(&weaponEntity, &placement);
                /* 3004620e: CG_ComposeBoneMatrix(ECX=bone entry, EAX=&placement,
                 * EDX=&worldMatrix). */
                CG_ComposeBoneMatrix(&boneMatrixTable[brassBone], &placement,
                                     &worldMatrix);
                /* 30046213..30046234: world translation (out +0x30/34/38) to
                 * cg_brassEffectOrigin (0x301698c0/c4/c8). */
                cg_brassEffectOrigin[0] = worldMatrix.origin[0];
                cg_brassEffectOrigin[1] = worldMatrix.origin[1];
                cg_brassEffectOrigin[2] = worldMatrix.origin[2];
            }
        }

        /* 3004623d: weapon->weaponType (+0x7c) == 4; 30046247/3004624c: TEST
         * AH,0x2 on cg_predictedPlayerState.entityStateFlags (0x30483248) —
         * flame chunks only for an actively-firing GAS weapon. */
        if (weapon->weaponType == WEAPTYPE_GAS &&
            (cg_predictedPlayerState.entityStateFlags &
             EF_FIRING) != 0) {
            /* 30046255..30046274: trap(0xb2, viewDObjSelf, "tag_flash"
             * @0x300772c0); 30046274 JLE skips emission when the bone is
             * absent. */
            int32_t flashBone = coduo_int32_from_bits((uint32_t)cgame_syscall(
                CG_DOBJ_GET_BONE_INDEX,
                (intptr_t)registered->viewDObjSelf,
                (intptr_t)cg_muzzleFlashTagName));
            if (flashBone > -1) {
                /* 3004627a..30046324: same bone-matrix composition as the
                 * tag_brass block, on the tag_flash bone. */
                DObjSkelMat *boneMatrixTable = (DObjSkelMat *)(intptr_t)cgame_syscall(
                    CG_DOBJ_GET_BONE_MATRICES,
                    (intptr_t)registered->viewDObjSelf, 0);
                matrix43_t placement;
                DObjSkelMat worldMatrix;
                vec3_t flameViewAngles;

                cgame_compat_weapon_entity_placement(&weaponEntity, &placement);
                CG_ComposeBoneMatrix(&boneMatrixTable[flashBone], &placement,
                                     &worldMatrix);

                /* 30046330..3004633e: the snapshot's own client uses the
                 * predicted view angles (0x304832ac =
                 * cg_predictedPlayerState.viewAngles); anyone else uses
                 * cent->lerpAngles (+0x214..0x21c) (30046353..3004635f). */
                if (cent->currentState.number == cg_snap->ps.psClientNum) {
                    memcpy(flameViewAngles, cg_predictedPlayerState.viewAngles,
                           sizeof(flameViewAngles));
                } else {
                    memcpy(flameViewAngles, cent->lerpAngles,
                           sizeof(flameViewAngles));
                }

                /* 30046365..30046387: CG_EmitPlayerFlameChunks(
                 * EAX=&flameViewAngles, cent, &worldMatrix[12] (tag_flash world
                 * position, out +0x30), 1.8f (imm 0x3fe66666), 1, 0) — same
                 * shape as the CG_Player call site (player_entity.c). */
                CG_EmitPlayerFlameChunks(flameViewAngles, cent, worldMatrix.origin,
                                         1.8f, 1, 0);
            }
        }
    }

    /* 30046398: muzzle tail — only [cent+0x1ec] (weaponEffectActive) is tested;
     * currentValid (+0x1e8) is never read by this function. */
    if (cent->weaponEffectActive == 0) {
        return;
    }
    /* 300463a6..300463b0: localFirstPerson set AND ps == NULL (EDI holds the
     * (ps != NULL) boolean computed at 30045cf6) -> return without clearing. */
    if (localFirstPerson && ps == NULL) {
        return;
    }
    /* 300463bf: the latch is cleared before the viewWeapon test (the store at
     * 300463bf is unconditional once both gates above pass). */
    cent->weaponEffectActive = qfalse;
    /* 300463b6/300463c9: no view weapon -> done. */
    if (!viewWeapon) {
        return;
    }

    {
        /* Exact SFxBoltInfo handed to trap 0xe9 by address: the DObj entity
         * number ([ESP+0x14], stored at 30046402/3004643e) with the resolved
         * bone index adjacent ([ESP+0x18], stored at 30046411/3004644d). */
        sfx_bolt_info_t muzzleBolt;
        uint32_t effect;
        int32_t boneIndex;

        /* 300463cf: branch on (ps != NULL) — NOT on localFirstPerson. */
        if (ps != NULL) {
            /* 300463d3/300463dd: DObj entity id = weaponIndex + 0x400, the
             * view-weapon pseudo-entity band above MAX_GENTITIES. */
            int32_t dobjEntityNum = coduo_int32_from_bits(
                (uint32_t)weaponIndex + (uint32_t)MAX_GENTITIES);

            /* 300463d7..300463f1: viewFlashEffect (+0xc8) with worldFlashEffect
             * (+0xcc) fallback; zero -> return. */
            effect = registered->viewFlashEffect;
            if (effect == 0) {
                effect = registered->worldFlashEffect;
            }
            if (effect == 0) {
                return;
            }

            /* 300463f5..30046406: tag name from the mutable slot
             * cg_muzzleTagNames[0] (0x30085eec -> "tag_flash");
             * bone = trap(0xe3, dobjEntityNum, tagName). */
            muzzleBolt.entityNum = dobjEntityNum;
            boneIndex = coduo_int32_from_bits((uint32_t)cgame_syscall(
                CG_RESOLVE_TAG, dobjEntityNum,
                (intptr_t)cg_muzzleTagNames[0]));
            /* 30046411: the bone is stored next to the entity number before the
             * sign check; 30046415 JL -> return on a negative index. */
            muzzleBolt.boneIndex = boneIndex;
            if (boneIndex < 0) {
                return;
            }

            /* 30046417..3004646d: trap(0xe9, effect,
             * cg_specialTagPlacement.origin (0x3048b0e4 — the placement
             * committed above), 0, &muzzleBolt). */
            cgame_syscall(CG_PLAY_EFFECT_ON_TAG, (intptr_t)effect,
                          (intptr_t)cg_specialTagPlacement.origin,
                          (intptr_t)NULL, (intptr_t)&muzzleBolt);
        } else {
            /* 30046425..3004642d: world players use worldFlashEffect only — no
             * viewFlashEffect fallback; zero -> return. */
            effect = registered->worldFlashEffect;
            if (effect == 0) {
                return;
            }

            /* 3004642f..30046442: bone = trap(0xe3, cent->currentState.number,
             * cg_muzzleTagNames[0]); entity number stored at 3004643e. */
            muzzleBolt.entityNum = cent->currentState.number;
            boneIndex = coduo_int32_from_bits((uint32_t)cgame_syscall(
                CG_RESOLVE_TAG, cent->currentState.number,
                (intptr_t)cg_muzzleTagNames[0]));
            muzzleBolt.boneIndex = boneIndex;
            if (boneIndex < 0) {
                return;
            }

            /* 30046453..3004646d: trap(0xe9, effect, &cent->lerpOrigin
             * (+0x208), 0, &muzzleBolt). */
            cgame_syscall(CG_PLAY_EFFECT_ON_TAG, (intptr_t)effect,
                          (intptr_t)cent->lerpOrigin,
                          (intptr_t)NULL, (intptr_t)&muzzleBolt);
        }
    }
    /* 30046470: shared epilogue (i386 /GS cookie check, not modeled). */
}
