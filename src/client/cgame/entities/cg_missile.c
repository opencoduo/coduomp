// Source: uo_cgame_mp_x86.dll 0x3001edb0..0x3001f114
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001edb0_3001f114.mcode
//
// CG_Missile — a CG_AddCEntity render handler (eType-dispatch arm index 4, whose
// trampoline at 0x300221c0 calls this function; the same 16-entry jump table at
// 0x30022228 routes eType 0 to CG_General (0x3001e430) and eType 3 to CG_Item
// (0x3001e680)). Given a client entity (centity_t *cent in EBX, /GS frame), it
// renders this weapon's muzzle/laser effect:
//   * plays the weapon's flash local sound,
//   * registers two muzzle-effect resources and, when both are live, emits a
//     view-facing beam through trap_MSS_PlayBlendedSoundAliases whose alpha fades with how edge-on the
//     laser direction is to the muzzle->camera line and with muzzle->camera distance,
//   * refreshes the entity's DObj anim tree,
//   * adds the muzzle-flash dynamic light,
//   * builds and submits one RT_MODEL refEntity_t, and
//   * plays the "tag_origin" effect exactly once via CG_PLAY_EFFECT_ON_TAG.
//
// Naming: the mechanical .mcode name "G_GetNonPVSTankInfo" is REJECTED — it is a
// pure win-size==server-size guess (0x364==0x364), which the contract forbids, and
// this is a cgame render handler, not a server tank-info query. The Mac cgame
// symbol CG_Missile shares the four distinctive recovered direct callees and has
// the corresponding weapon/sound calls. Together with the eType-4 dispatch role,
// that cross-architecture fingerprint resolves the source name.
//
// Register/stack ABI (proven from the bytes, expressed as plain C per the contract):
//   * cent arrives in EBX (register arg); no stack args; plain RET.
//   * /GS frame: snapshot __security_cookie (0x30081650) into the frame on entry and
//     verify via __security_check_cookie (0x30061639) on exit — not source-level;
//     omitted from the body.
//   * VectorNormalize (0x30049700) takes its vec3 pointer in ESI (register arg).
//   * CG_RefreshEntityDObjAnimTree (0x30021ea0) takes cent->currentState.number in ESI plus its
//     two stack args (eType, animTreeParam), caller-cleaned; the cleanup is folded
//     into the ADD ESP,0x10 after the following trap-0xa5 call.
//   * AnglesToAxisNegRight (0x3004c200) takes out-axis in EAX and source angles in EDX.
//
// Constants (exact .rdata/.data values, dumped with objdump -s):
//   0x30487a90/94/98 = cg_refdef.vieworg vec3 (client view/camera origin).
//   0x3007c144 = 4000.0f, 0x3007c140 = -4000.0f, 0x3007c13c = 700.0f,
//   0x3007c138 = 1/700 (0.0014285714f), 0x3007c134 = 1/4000 (0.00025f),
//   0x3007bce0 = 1.0f, 0x3007bce8 = 0.5f, 0x3007bcec = 0.0f.

#include "../client_recovered.h"
#include "qcommon/fx_types.h"
#include "../globals.h"
#include <math.h> /* sqrtl: inline x87 FSQRT at 0x3001eed3 */
#include <string.h>

/* Beam-alpha clamp limits and scale factors (0x3007c134..0x3007c144). Named by
 * their proven role in the muzzle->camera facing fade; kept file-local because they
 * are one-function tuning constants. */
enum { CG_LASER_MAX_LEN = 4000, CG_LASER_FADE_DIST = 700 };
#define CG_LASER_INV_FADE_DIST 0.0014285714132711291f /* 1/700  @0x3007c138 */
#define CG_LASER_INV_MAX_LEN   0.0002500000118743628f /* 1/4000 @0x3007c134 */

void CG_Missile(centity_t *cent /* EBX */)
{
    /* Exact SFxBoltInfo local at ESP+0x20, later passed to
     * CG_PLAY_EFFECT_ON_TAG. */
    sfx_bolt_info_t boltInfo;

    /* 0x3001edc3: MOV AL,[EBX+8]; TEST AL,AL; JS -> the low byte's sign bit is
     * EF_NODRAW (0x80). A drawn-suppressed entity skips straight to the epilogue. */
    if (cent->currentState.eFlags & EF_NODRAW)
        return;

    /* 0x3001edd0: clamp the weapon index into the registered range. The compare is
     * signed (JLE), against bg_numWeapons (0x30134cd4); an out-of-range index is
     * reset to 0 (the default/none weapon). */
    if (cent->currentState.weapon > bg_numWeapons)
        cent->currentState.weapon = 0;

    /* 0x3001ede8: wi = &cg_weaponInfos[weaponIndex] (IMUL idx,0x1c4; ADD base). */
    cgWeaponInfo_t *wi = &cg_weaponInfos[cent->currentState.weapon];

    /* 0x3001edfa: play the muzzle-flash local sound when this weapon defines one.
     * ECX = &cent->lerpOrigin (channel object), EAX = wi->flashSound (soundName),
     * one stack arg = cent->currentState.number. */
    if (wi->projectileSound != 0) {
        CG_PlaySoundAliasByName(cent->currentState.number, &cent->lerpOrigin,
                                wi->projectileSound);
    }

    /* 0x3001ee15: the view-facing beam is only emitted when this weapon carries a
     * first muzzle-effect resource; otherwise skip straight to the anim-tree /
     * refEntity phase at label_addModel (0x3001efa9). */
    if (wi->projectileSoundBlend1 != 0) {
        /* 0x3001ee29: register both muzzle-effect resources into engine handles via
         * CG_COM_PICK_SOUND_ALIAS (trap 0xc4: name, channelObj = &cent->lerpOrigin). */
        snd_alias_t *alias0 = trap_Com_PickSoundAlias(
            wi->projectileSoundBlend1, cent->lerpOrigin);
        snd_alias_t *alias1 = trap_Com_PickSoundAlias(
            wi->projectileSoundBlend2, cent->lerpOrigin);

        /* 0x3001ee55/0x3001ee61: both handles must be live to draw the beam. */
        if (alias0 != NULL && alias1 != NULL) {
            /* 0x3001ee6f/0x3001ee74/0x3001ee77 snapshot the three raw laser
             * direction dwords before the to-camera vector is finished and before
             * VectorNormalize. */
            vec3_t laserDir;
            memcpy(&laserDir[0], &cent->currentState.laserDir[0], sizeof(laserDir[0]));
            memcpy(&laserDir[1], &cent->currentState.laserDir[1], sizeof(laserDir[1]));
            memcpy(&laserDir[2], &cent->currentState.laserDir[2], sizeof(laserDir[2]));

            /* 0x3001ee69: toCamera = cg_refdef.vieworg - cent->lerpOrigin (the muzzle
             * origin). Computed at single precision and normalized in place; its
             * pre-normalization length is the muzzle->camera distance. */
            vec3_t toCamera;
            toCamera[0] = (float)((long double)cg_refdef.vieworg[0] -
                                  (long double)cent->lerpOrigin[0]);
            toCamera[1] = (float)((long double)cg_refdef.vieworg[1] -
                                  (long double)cent->lerpOrigin[1]);
            toCamera[2] = (float)((long double)cg_refdef.vieworg[2] -
                                  (long double)cent->lerpOrigin[2]);
            float camDist = VectorNormalize(toCamera);

            /* 0x3001eeb7..0x3001eee0: clamp the laser direction's length to
             * CG_LASER_MAX_LEN. mag = |laserDir| (single precision, FSQRT). The FCOMP
             * vs 4000.0 sets ZF|CF for mag <= 4000; TEST AH,0x41 / JNZ then skips the
             * clamp in that case. */
            /* mag = |laserDir|: the DLL runs the sum-of-squares straight into an
             * inline FSQRT (0x3001eed3) and FCOMPs the UNROUNDED st0 against 4000.0f
             * (0x3001eed5) with no float store -- so mag stays long double and the
             * root is sqrtl (no result rounding). */
            long double mag = sqrtl(
                (long double)laserDir[2] * (long double)laserDir[2] +
                (long double)laserDir[1] * (long double)laserDir[1] +
                (long double)laserDir[0] * (long double)laserDir[0]);
            if (mag > (float)CG_LASER_MAX_LEN) {
                /* 0x3001eee2: normalize laserDir in place (discard its length via
                 * FSTP ST0) then scale each component to length 4000. */
                (void)VectorNormalize(laserDir);
                laserDir[0] = (float)((long double)laserDir[0] *
                                      (long double)(float)CG_LASER_MAX_LEN);
                laserDir[1] = (float)((long double)laserDir[1] *
                                      (long double)(float)CG_LASER_MAX_LEN);
                laserDir[2] = (float)((long double)laserDir[2] *
                                      (long double)(float)CG_LASER_MAX_LEN);
            }

            /* 0x3001ef17: facing = dot(laserDir, toCamera). The dot, both clamps,
             * the conditional camDist*(1/700) scale and the alpha map all run in st0
             * with NO float store until the single FSTP of alpha at 0x3001ef98 -- so
             * facing stays long double and the FCOM clamps (0x3001ef33/0x3001ef4a)
             * compare the unrounded value. */
            long double facing =
                (long double)laserDir[2] * (long double)toCamera[2] +
                (long double)laserDir[1] * (long double)toCamera[1] +
                (long double)laserDir[0] * (long double)toCamera[0];

            /* 0x3001ef33..0x3001ef5f: clamp facing to [-4000, 4000].
             * FCOM vs -4000.0 / JP-skip pattern: facing < -4000 -> -4000; then
             * FCOM vs 4000.0: facing > 4000 -> 4000. */
            if (facing < (float)-CG_LASER_MAX_LEN)
                facing = (float)-CG_LASER_MAX_LEN;
            else if (facing > (float)CG_LASER_MAX_LEN)
                facing = (float)CG_LASER_MAX_LEN;

            /* 0x3001ef5f: when the muzzle->camera distance is under CG_LASER_FADE_DIST,
             * scale facing by camDist / 700 (facing * camDist * (1/700)). The FCOMP vs
             * 700.0 / JP-skip leaves facing unscaled when camDist >= 700. */
            if (camDist < (float)CG_LASER_FADE_DIST)
                facing = facing * (long double)camDist *
                         (long double)CG_LASER_INV_FADE_DIST;

            /* 0x3001ef7a: alpha = (facing/4000 + 1.0) * 0.5, mapping the clamped facing
             * from [-4000,4000] into [0,1]. */
            float alpha = (float)(
                (facing * (long double)CG_LASER_INV_MAX_LEN +
                 (long double)1.0f) * (long double)0.5f);

            /* 0x3001ef80..0x3001efa6: play the two selected aliases at the
             * entity's weapon origin, with alpha as the blend and timeShift 0.
             * EDX=EDI supplies &cent->lerpOrigin and ECX=0 supplies timeShift. */
            trap_MSS_PlayBlendedSoundAliases(alias0, alias1, alpha,
                                              cent->currentState.number,
                                              cent->lerpOrigin, 0);
        }
    }

    /* label_addModel (0x3001efa9): (re)bind the entity's DObj weapon anim tree.
     * ECX-slot arg = cent->currentState.eType, EAX-slot arg = wi->clipModelHandle; ESI carries
     * cent->currentState.number (register). Caller-cleaned; the cleanup folds into the ADD
     * ESP,0x10 after the trap-0xa5 call below. */
    CG_RefreshEntityDObjAnimTree(cent->currentState.number, cent->currentState.eType,
                                 wi->clipModelHandle);

    /* 0x3001efbb: handle = (int32_t)cgame_syscall(CG_DOBJ_GET_HANDLE, cent->currentState.number).
     * A zero handle means no DObj skeleton — nothing to draw, jump to the epilogue. */
    struct DObj_s *dobj = (struct DObj_s *)cgame_syscall(
        CG_DOBJ_GET_HANDLE, cent->currentState.number);
    if (dobj == NULL)
        return;

    /* 0x3001efd6: emit the muzzle-flash dynamic light when its intensity is nonzero.
     * FLD 0.0 / FLD wi->projectileDLight / FUCOMPP / TEST AH,0x44 / JNP is the
     * "!= 0.0" test. */
    if (wi->projectileDLight != 0.0f) {
        trap_R_AddLightToScene(cent->lerpOrigin,
                               wi->projectileDLight,
                               wi->flashLightR,
                               wi->flashLightG,
                               wi->flashLightB);
    }

    /* 0x3001f016..0x3001f0a1: build and submit one RT_MODEL render entity.
     * REP STOSD zero-fills 0x27 (39) dwords = 0x9c = sizeof(refEntity_t). */
    refEntity_t re;
    uint32_t yBits;
    memcpy(&yBits, &cent->lerpOrigin[1], sizeof(yBits));
    memset(&re, 0, sizeof(re));

    /* 0x3001f016 snapshots Y before the clear; X and Z are loaded after it. The
     * six destination writes are raw MOVs in X/X/Y/Z/Y/Z order. */
    uint32_t xBits;
    uint32_t zBits;
    memcpy(&xBits, &cent->lerpOrigin[0], sizeof(xBits));
    memcpy(&zBits, &cent->lerpOrigin[2], sizeof(zBits));
    memcpy(&re.origin[0], &xBits, sizeof(xBits));
    memcpy(&re.oldorigin[0], &xBits, sizeof(xBits));
    uint32_t flashRenderFx = (uint32_t)wi->flashRenderFx;
    memcpy(&re.origin[1], &yBits, sizeof(yBits));
    memcpy(&re.origin[2], &zBits, sizeof(zBits));
    memcpy(&re.oldorigin[1], &yBits, sizeof(yBits));
    memcpy(&re.oldorigin[2], &zBits, sizeof(zBits));

    /* 0x3001f046/0x3001f068/0x3001f075: renderfx = wi->flashRenderFx | RF_NOSHADOW. */
    re.renderfx = coduo_int32_from_bits(flashRenderFx | (uint32_t)RF_NOSHADOW);

    /* 0x3001f06b/0x3001f071/0x3001f079: axis = AnglesToAxisNegRight(re.axis,
     * cent->lerpAngles). EAX = &re.axis, EDX = &cent->lerpAngles. */
    AnglesToAxisNegRight(re.axis, cent->lerpAngles);

    /* 0x3001f082/0x3001f091/0x3001f098: DObj handle, owning centity, reType. */
    re.dobj = dobj;
    re.owner = cent;
    re.reType = RT_MODEL;

    /* 0x3001f089/0x3001f09c: trap_R_AddRefEntityToScene(&re). */
    trap_R_AddRefEntityToScene(&re);

    /* 0x3001f0a2: play the one-shot tag effect only if this weapon defines one and it
     * has not yet fired for this entity. */
    if (wi->projectileTrailEffect == 0)
        return;
    if (cent->laserEffectStarted != 0)
        return;

    /* 0x3001f0b9: tagResult = (int32_t)cgame_syscall(CG_RESOLVE_TAG, cent->currentState.number,
     * "tag_origin"). A negative result (signed JL) means no such tag — skip the play
     * but still latch the started flag. */
    /* 0x3001f0b9/0x3001f0c6 stores the entity word into the same record before
     * resolving its bone. The previous raw int[2] reconstruction omitted this
     * machine-code store and left the engine-facing entityNum uninitialized. */
    boltInfo.entityNum = cent->currentState.number;
    boltInfo.boneIndex = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_RESOLVE_TAG, cent->currentState.number, (intptr_t)cg_originTagName));
    if (boltInfo.boneIndex >= 0) {
        /* 0x3001f0db: cgame_syscall(CG_PLAY_EFFECT_ON_TAG,
         * wi->projectileTrailEffect, &cent->lerpOrigin, 0, &boltInfo). */
        cgame_syscall(CG_PLAY_EFFECT_ON_TAG,
                      (int32_t)wi->projectileTrailEffect,
                      (intptr_t)&cent->lerpOrigin,
                      0,
                      (intptr_t)&boltInfo);
    }

    /* 0x3001f0f8: latch so the tag effect plays exactly once (ESI = 1). */
    cent->laserEffectStarted = 1;

    /* 0x3001f0fe..0x3001f113: /GS cookie verify + epilogue (omitted). */
}
