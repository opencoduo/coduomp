// Source: uo_cgame_mp_x86.dll 0x300435d0..0x3004488d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300435d0_3004488d.mcode
//
// CG_RegisterWeapon -- lazily register every client-side model, animation,
// material, effect, sound, icon, and localized string used by one weapon.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>
#include <string.h>

enum {
    CG_WEAPON_DOBJ_MODEL_COUNT = 2,
    CG_WEAPON_NAME_COPY_SIZE = 63,
    CG_REGISTER_WEAPON_SHADER_SORT = 5,
    CG_REGISTER_WEAPON_MODEL_CATEGORY = 7,
    CG_REGISTER_VIEW_MODEL_CATEGORY = 6,
    CG_LOADING_CVAR_BUFFER_SIZE = 64,
};

void CG_RegisterWeapon(int weaponIndex)
{
    weaponInfo_t *weapon;
    cgWeaponInfo_t *weaponInfo;
    const gitem_t *item;
    itemInfo_t *itemInfo;
    intptr_t xanimDefinition = 0;
    intptr_t xanimTree = 0;

    if (weaponIndex == 0) {
        return;
    }

    weapon = bg_weaponInfos[weaponIndex];
    weaponInfo = &cg_weaponInfos[weaponIndex];
    if (weaponInfo->registered != 0) {
        return;
    }

    memset(weaponInfo, 0, sizeof(*weaponInfo));
    item = &bg_itemlist[weaponIndex];
    itemInfo = &cg_items[weaponIndex];
    weaponInfo->registered = 1;
    weaponInfo->item = item;
    CG_RegisterItemVisuals(weaponIndex);
    weaponInfo->lastRunAnim = -1;

    if (weapon->gunModel[0] != '\0') {
        const char *animNames[WEAPON_XANIM_COUNT - 1] = {
            weapon->idleAnim,         weapon->emptyIdleAnim,   weapon->fireAnim,         weapon->holdFireAnim,    weapon->lastShotAnim,
            weapon->rechamberAnim,    weapon->meleeAnim,       weapon->reloadAnim,       weapon->reloadEmptyAnim, weapon->reloadStartAnim,
            weapon->reloadEndAnim,    weapon->raiseAnim,       weapon->dropAnim,         weapon->altRaiseAnim,    weapon->altDropAnim,
            weapon->adsFireAnim,      weapon->adsLastShotAnim, weapon->adsRechamberAnim, weapon->lmgDeployedAnim, weapon->lmgDeployAnim,
            weapon->lmgBreakdownAnim, weapon->adsUpAnim,       weapon->adsDownAnim};
        /* The PE32 producer writes every consumed lane below but leaves each
         * abiGap_00a word untouched; CoDUOMP.exe skips that word. */
        DObjModel models[CG_WEAPON_DOBJ_MODEL_COUNT];
        char handModelPath[MAX_QPATH];
        char gunModelPath[MAX_QPATH];
        int32_t animIndex;

        if (weapon->handModel == NULL || weapon->handModel[0] == '\0') {
            Com_Error(ERR_DROP, cg_registerWeaponMissingHandError, weapon->displayName);
        }
        if (weapon->idleAnim == NULL || weapon->idleAnim[0] == '\0') {
            Com_Error(ERR_DROP, cg_registerWeaponMissingIdleError, weapon->displayName);
        }

        xanimDefinition = cgame_syscall(CG_XANIM_CREATE_ANIMS, (intptr_t)"VIEWMODEL", WEAPON_XANIM_COUNT);
        cgame_syscall(CG_XANIM_BLEND, xanimDefinition, 0, (intptr_t)"root", 1, WEAPON_XANIM_COUNT - 1, 0);

        for (animIndex = 1; animIndex < WEAPON_XANIM_COUNT; ++animIndex) {
            const char *animName = animNames[animIndex - 1];
            if (animName[0] == '\0') {
                animName = weapon->idleAnim;
            }
            cgame_syscall(CG_XANIM_PRECACHE, (intptr_t)animName);
            cgame_syscall(CG_XANIM_CREATE, xanimDefinition, animIndex, (intptr_t)animName);
        }

        xanimTree = cgame_syscall(CG_XANIM_CREATE_TREE, xanimDefinition);
        for (animIndex = 0; animIndex < WEAPON_XANIM_COUNT; ++animIndex) {
            weaponInfo->animRates[animIndex] = 1.0f;
        }

        /* mcode (e.g. 0x30043832-0x30043843): the length is loaded via a bare
         * FILD (exact) and divided by the duration via FIDIV (integer divisor,
         * exact), with the ONLY float round at the trailing FSTP DWORD store.
         * Both operands must therefore enter exact: long double preserves the
         * x87-register evaluation convention, where a (float) cast would round
         * each operand first (Class 4). */
#define SET_ANIM_RATE(animNumber, durationMsec) \
    do { \
        int32_t duration_ = (durationMsec); \
        weaponInfo->animRates[(animNumber)] = \
            duration_ <= 0 \
                ? 0.0f \
                : (long double)coduo_int32_from_bits((uint32_t)cgame_syscall(CG_XANIM_GET_LENGTH, xanimDefinition, (animNumber))) / \
                      (long double)duration_; \
    } while (0)

        SET_ANIM_RATE(WEAPON_XANIM_HOLD_FIRE, weapon->specialFireDelay);
        SET_ANIM_RATE(WEAPON_XANIM_MELEE, weapon->meleeTime);
        SET_ANIM_RATE(WEAPON_XANIM_RELOAD, weapon->reloadTime);
        SET_ANIM_RATE(WEAPON_XANIM_RELOAD_EMPTY, weapon->reloadEmptyTime);
        SET_ANIM_RATE(WEAPON_XANIM_RELOAD_START, weapon->reloadStartTime);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        SET_ANIM_RATE(WEAPON_XANIM_RELOAD_END, weapon->reloadEndTime);
        SET_ANIM_RATE(WEAPON_XANIM_RAISE, weapon->switchRaiseTime);
        SET_ANIM_RATE(WEAPON_XANIM_DROP, weapon->lowerTime);
        SET_ANIM_RATE(WEAPON_XANIM_ALT_RAISE, weapon->altSwitchRaiseTime);
        SET_ANIM_RATE(WEAPON_XANIM_ALT_DROP, weapon->altSwitchLowerTime);
        SET_ANIM_RATE(WEAPON_XANIM_LMG_DEPLOY, weapon->adsInTime);
        SET_ANIM_RATE(WEAPON_XANIM_LMG_BREAKDOWN, weapon->adsOutTime);
#undef SET_ANIM_RATE

        cgame_syscall(CG_XANIM_CLEAR_TREE_GOAL_WEIGHTS, xanimTree, 0, 0);
        cgame_syscall(CG_XANIM_SET_GOAL_WEIGHT, xanimTree, 0, CG_FloatBits(1.0f), CG_FloatBits(0.0f),
                      CG_FloatBits(weaponInfo->animRates[0]), 1, 0);

        if (weapon->adsUpAnim[0] != '\0' && cgame_syscall(CG_XANIM_IS_LOOPING, xanimDefinition, WEAPON_XANIM_ADS_UP) != 0) {
            Com_Error(ERR_DROP, cg_registerWeaponLoopingAdsError, weapon->adsUpAnim);
        }
        if (weapon->adsDownAnim[0] != '\0' && cgame_syscall(CG_XANIM_IS_LOOPING, xanimDefinition, WEAPON_XANIM_ADS_DOWN) != 0) {
            Com_Error(ERR_DROP, cg_registerWeaponLoopingAdsError, weapon->adsDownAnim);
        }

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if (strlen(weapon->handModel) > sizeof(handModelPath) - sizeof("xmodel/") ||
            strlen(weapon->gunModel) > sizeof(gunModelPath) - sizeof("xmodel/")) {
            Com_Error(ERR_DROP, "\x15"
                                "CG_RegisterWeapon: model path is too long");
            return;
        }
        Com_sprintf(handModelPath, sizeof(handModelPath), "xmodel/%s", weapon->handModel);
        models[0].modelIndex = (int16_t)CG_RegisterModel(handModelPath, CG_REGISTER_VIEW_MODEL_CATEGORY);
        models[0].model = (XModel *)(intptr_t)cgame_syscall(CG_DOBJ_WRAP_MODEL, models[0].modelIndex);
        models[0].tagName = NULL;
        models[0].ignoreCollision = 0;

        Com_sprintf(gunModelPath, sizeof(gunModelPath), "xmodel/%s", weapon->gunModel);
        models[1].modelIndex = (int16_t)CG_RegisterModel(gunModelPath, CG_REGISTER_VIEW_MODEL_CATEGORY);
        models[1].model = (XModel *)(intptr_t)cgame_syscall(CG_DOBJ_WRAP_MODEL, models[1].modelIndex);
        models[1].tagName = "tag_weapon";
        models[1].ignoreCollision = 0;

        cgame_syscall(CG_CLIENT_DOBJ_CREATE, (intptr_t)models, CG_WEAPON_DOBJ_MODEL_COUNT, xanimTree,
                      weaponIndex + CG_VIEW_WEAPON_DOBJ_HANDLE_BASE);
        weaponInfo->viewDObjSelf =
            (struct DObj_s *)(intptr_t)cgame_syscall(CG_DOBJ_GET_HANDLE, weaponIndex + CG_VIEW_WEAPON_DOBJ_HANDLE_BASE);
        Q_strncpyz(weaponInfo->name, weapon->handModel, CG_WEAPON_NAME_COPY_SIZE);
        weaponInfo->name[CG_WEAPON_NAME_COPY_SIZE] = '\0';
    }

    if (weapon->worldModel[0] != '\0') {
        weaponInfo->worldModelHandle = CG_RegisterModel(weapon->worldModel, CG_REGISTER_WEAPON_MODEL_CATEGORY);
    }
    if (weapon->worldModel[0] != '\0' && weaponInfo->worldModelHandle == 0) {
        Com_Printf(cg_registerWeaponWorldModelWarning, weapon->worldModel);
    }

    weaponInfo->viewModelHandle = weapon->viewModel[0] != '\0' ? CG_RegisterModel(weapon->viewModel, CG_REGISTER_WEAPON_MODEL_CATEGORY) : 0;
    weaponInfo->pickupModelHandle =
        weapon->pickupModel[0] != '\0' ? CG_RegisterModel(weapon->pickupModel, CG_REGISTER_WEAPON_MODEL_CATEGORY) : 0;

    weaponInfo->itemHudIconShader = CG_RegisterMaterial(item->hudIcon, CG_REGISTER_WEAPON_SHADER_SORT);
    weaponInfo->itemSelectIconShader = CG_RegisterMaterial(va("%s_select", item->hudIcon), CG_REGISTER_WEAPON_SHADER_SORT);
    weaponInfo->itemAmmoIconShader = CG_RegisterMaterial(item->ammoIcon, CG_REGISTER_WEAPON_SHADER_SORT);

    if (weapon->reticleCenter[0] != '\0') {
        if (cg_snap == NULL && cg_updateScreenActive == 0) {
            char expectedUsageString[CG_LOADING_CVAR_BUFFER_SIZE];
            int32_t levelShot = 0;
            const char *serverInfo;
            const char *mapName;
            int32_t expectedUsage;

            cg_updateScreenActive = 1;
            if (cl_serverloadmap.string[0] != 0) {
                cgame_syscall(CG_CVAR_SET, (intptr_t)"cl_serverloadmap", (intptr_t)"");
            }
            if (cl_serverloadgametype.string[0] != 0) {
                cgame_syscall(CG_CVAR_SET, (intptr_t)"cl_serverloadgametype", (intptr_t)"");
            }
            if (cl_serverloadwaiting.integer != 0) {
                cgame_syscall(CG_CVAR_SET, (intptr_t)"cl_serverloadwaiting", (intptr_t)"0");
            }

            serverInfo = &cg_gameState.stringData[cg_gameState.stringOffsets[0]];
            mapName = Info_ValueForKey(serverInfo, "mapname");
            if (mapName != NULL && mapName[0] != '\0') {
                levelShot = trap_R_RegisterShaderNoMip(va("levelshots/%s.tga", mapName), 2);
            }
            if (levelShot == 0) {
                levelShot = trap_R_RegisterShaderNoMip("menu/art/unknownmap", 2);
            }

            cgame_syscall(CG_R_SETCOLOR, 0);
            trap_R_DrawStretchPic(CG_FloatBits(0.0f * cgs_screenXScale), CG_FloatBits(0.0f * cgs_screenYScale),
                                  CG_FloatBits(640.0f * cgs_screenXScale), CG_FloatBits(480.0f * cgs_screenYScale), CG_FloatBits(0.0f),
                                  CG_FloatBits(0.0f), CG_FloatBits(1.0f), CG_FloatBits(1.0f), levelShot);

            cgame_syscall(CG_CVAR_VARIABLE_STRING_BUFFER, (intptr_t)"com_expectedhunkusage", (intptr_t)expectedUsageString,
                          (int32_t)sizeof(expectedUsageString));
            expectedUsage = coduo_crt_atoi(expectedUsageString);
            /* mcode note (0x30043e04-0x30043e1c here, repeated in the two
             * clone blocks below): the compiler emits four DEAD 0.8f
             * (0x3f4ccccd) stack stores right after the atoi -- never read,
             * omitted as behavior-neutral. The quotient is FILD [hunk] (exact)
             * / FIDIV [expectedUsage] (integer divisor, exact) with the single
             * float round at the FST DWORD (0x30043e39-0x30043e3d). The retail
             * >1.0 clamp compares the UNROUNDED x87 quotient (FST keeps ST0,
             * then FCOMP vs 1.0 @0x3007bce0), while the rounded copy is passed
             * to the drawing helper. */
            if (expectedUsage > 0) {
                long double fractionRaw =
                    (long double)coduo_int32_from_bits((uint32_t)cgame_syscall(CG_HUNK_USED)) / (long double)expectedUsage;
                float fraction = (float)fractionRaw;
                if (fractionRaw > 1.0f) {
                    fraction = 1.0f;
                }
                CG_DrawFilledBarStyled(200.0f, 468.0f, 240.0f, 10.0f, fraction);
            }

            cgame_syscall(CG_UPDATE_SCREEN);
            cg_updateScreenActive = coduo_int32_from_bits((uint32_t)cg_updateScreenActive - 1u);
        }
        weaponInfo->reticleCenterShader =
            (qhandle_t)cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)weapon->reticleCenter, CG_REGISTER_WEAPON_SHADER_SORT);
    }
    if (weapon->reticleSide[0] != '\0') {
        if (cg_snap == NULL && cg_updateScreenActive == 0) {
            char expectedUsageString[CG_LOADING_CVAR_BUFFER_SIZE];
            int32_t levelShot = 0;
            const char *serverInfo;
            const char *mapName;
            int32_t expectedUsage;

            cg_updateScreenActive = 1;
            if (cl_serverloadmap.string[0] != 0) {
                cgame_syscall(CG_CVAR_SET, (intptr_t)"cl_serverloadmap", (intptr_t)"");
            }
            if (cl_serverloadgametype.string[0] != 0) {
                cgame_syscall(CG_CVAR_SET, (intptr_t)"cl_serverloadgametype", (intptr_t)"");
            }
            if (cl_serverloadwaiting.integer != 0) {
                cgame_syscall(CG_CVAR_SET, (intptr_t)"cl_serverloadwaiting", (intptr_t)"0");
            }

            serverInfo = &cg_gameState.stringData[cg_gameState.stringOffsets[0]];
            mapName = Info_ValueForKey(serverInfo, "mapname");
            if (mapName != NULL && mapName[0] != '\0') {
                levelShot = trap_R_RegisterShaderNoMip(va("levelshots/%s.tga", mapName), 2);
            }
            if (levelShot == 0) {
                levelShot = trap_R_RegisterShaderNoMip("menu/art/unknownmap", 2);
            }

            cgame_syscall(CG_R_SETCOLOR, 0);
            trap_R_DrawStretchPic(CG_FloatBits(0.0f * cgs_screenXScale), CG_FloatBits(0.0f * cgs_screenYScale),
                                  CG_FloatBits(640.0f * cgs_screenXScale), CG_FloatBits(480.0f * cgs_screenYScale), CG_FloatBits(0.0f),
                                  CG_FloatBits(0.0f), CG_FloatBits(1.0f), CG_FloatBits(1.0f), levelShot);

            cgame_syscall(CG_CVAR_VARIABLE_STRING_BUFFER, (intptr_t)"com_expectedhunkusage", (intptr_t)expectedUsageString,
                          (int32_t)sizeof(expectedUsageString));
            expectedUsage = coduo_crt_atoi(expectedUsageString);
            if (expectedUsage > 0) {
                long double fractionRaw =
                    (long double)coduo_int32_from_bits((uint32_t)cgame_syscall(CG_HUNK_USED)) / (long double)expectedUsage;
                float fraction = (float)fractionRaw;
                if (fractionRaw > 1.0f) {
                    fraction = 1.0f;
                }
                CG_DrawFilledBarStyled(200.0f, 468.0f, 240.0f, 10.0f, fraction);
            }

            cgame_syscall(CG_UPDATE_SCREEN);
            cg_updateScreenActive = coduo_int32_from_bits((uint32_t)cg_updateScreenActive - 1u);
        }
        weaponInfo->reticleSideShader =
            (qhandle_t)cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)weapon->reticleSide, CG_REGISTER_WEAPON_SHADER_SORT);
    }
    if (weapon->adsOverlayShader[0] != '\0') {
        if (cg_snap == NULL && cg_updateScreenActive == 0) {
            char expectedUsageString[CG_LOADING_CVAR_BUFFER_SIZE];
            int32_t levelShot = 0;
            const char *serverInfo;
            const char *mapName;
            int32_t expectedUsage;

            cg_updateScreenActive = 1;
            if (cl_serverloadmap.string[0] != 0) {
                cgame_syscall(CG_CVAR_SET, (intptr_t)"cl_serverloadmap", (intptr_t)"");
            }
            if (cl_serverloadgametype.string[0] != 0) {
                cgame_syscall(CG_CVAR_SET, (intptr_t)"cl_serverloadgametype", (intptr_t)"");
            }
            if (cl_serverloadwaiting.integer != 0) {
                cgame_syscall(CG_CVAR_SET, (intptr_t)"cl_serverloadwaiting", (intptr_t)"0");
            }

            serverInfo = &cg_gameState.stringData[cg_gameState.stringOffsets[0]];
            mapName = Info_ValueForKey(serverInfo, "mapname");
            if (mapName != NULL && mapName[0] != '\0') {
                levelShot = trap_R_RegisterShaderNoMip(va("levelshots/%s.tga", mapName), 2);
            }
            if (levelShot == 0) {
                levelShot = trap_R_RegisterShaderNoMip("menu/art/unknownmap", 2);
            }

            cgame_syscall(CG_R_SETCOLOR, 0);
            trap_R_DrawStretchPic(CG_FloatBits(0.0f * cgs_screenXScale), CG_FloatBits(0.0f * cgs_screenYScale),
                                  CG_FloatBits(640.0f * cgs_screenXScale), CG_FloatBits(480.0f * cgs_screenYScale), CG_FloatBits(0.0f),
                                  CG_FloatBits(0.0f), CG_FloatBits(1.0f), CG_FloatBits(1.0f), levelShot);

            cgame_syscall(CG_CVAR_VARIABLE_STRING_BUFFER, (intptr_t)"com_expectedhunkusage", (intptr_t)expectedUsageString,
                          (int32_t)sizeof(expectedUsageString));
            expectedUsage = coduo_crt_atoi(expectedUsageString);
            if (expectedUsage > 0) {
                long double fractionRaw =
                    (long double)coduo_int32_from_bits((uint32_t)cgame_syscall(CG_HUNK_USED)) / (long double)expectedUsage;
                float fraction = (float)fractionRaw;
                if (fractionRaw > 1.0f) {
                    fraction = 1.0f;
                }
                CG_DrawFilledBarStyled(200.0f, 468.0f, 240.0f, 10.0f, fraction);
            }

            cgame_syscall(CG_UPDATE_SCREEN);
            cg_updateScreenActive = coduo_int32_from_bits((uint32_t)cg_updateScreenActive - 1u);
        }
        weaponInfo->adsOverlayShader =
            (qhandle_t)cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)weapon->adsOverlayShader, CG_REGISTER_WEAPON_SHADER_SORT);
    }

    if (weapon->viewFlashEffect[0] != '\0') {
        weaponInfo->viewFlashEffect = (uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)weapon->viewFlashEffect);
    }
    if (weapon->worldFlashEffect[0] != '\0') {
        weaponInfo->worldFlashEffect = (uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)weapon->worldFlashEffect);
    }

    weaponInfo->projectileSound = trap_Com_SoundAliasString(weapon->projectileSound);
    weaponInfo->pullbackSound = trap_Com_SoundAliasString(weapon->pullbackSound);
    weaponInfo->fireSound = trap_Com_SoundAliasString(weapon->fireSound);
    weaponInfo->fireEchoSound = trap_Com_SoundAliasString(weapon->fireEchoSound);
    weaponInfo->lastShotSound = trap_Com_SoundAliasString(weapon->lastShotSound);
    weaponInfo->rechamberSound = trap_Com_SoundAliasString(weapon->rechamberSound);
    weaponInfo->reloadSound = trap_Com_SoundAliasString(weapon->reloadSound);
    weaponInfo->reloadEmptySound = trap_Com_SoundAliasString(weapon->reloadEmptySound);
    weaponInfo->reloadStartSound = trap_Com_SoundAliasString(weapon->reloadStartSound);
    weaponInfo->reloadEndSound = trap_Com_SoundAliasString(weapon->reloadEndSound);
    weaponInfo->raiseSound = trap_Com_SoundAliasString(weapon->raiseSound);
    if (weaponInfo->raiseSound == 0) {
        weaponInfo->raiseSound = trap_Com_SoundAliasString("weap_raise");
    }
    weaponInfo->altSwitchSound = trap_Com_SoundAliasString(weapon->altSwitchSound);
    weaponInfo->putawaySound = trap_Com_SoundAliasString(weapon->putawaySound);
    if (weaponInfo->putawaySound == 0) {
        weaponInfo->putawaySound = trap_Com_SoundAliasString("weap_putaway");
    }
    weaponInfo->noteTrackSoundA = trap_Com_SoundAliasString(weapon->noteTrackSoundA);
    weaponInfo->noteTrackSoundB = trap_Com_SoundAliasString(weapon->noteTrackSoundB);
    weaponInfo->noteTrackSoundC = trap_Com_SoundAliasString(weapon->noteTrackSoundC);
    weaponInfo->noteTrackSoundD = trap_Com_SoundAliasString(weapon->noteTrackSoundD);
    weaponInfo->loopFireSound = trap_Com_SoundAliasString(weapon->loopFireSound);
    weaponInfo->stopFireSound = trap_Com_SoundAliasString(weapon->stopFireSound);
    weaponInfo->projectileSoundBlend1 = trap_Com_SoundAliasString(weapon->projectileSoundBlend1);
    weaponInfo->projectileSoundBlend2 = trap_Com_SoundAliasString(weapon->projectileSoundBlend2);
    weaponInfo->deploySound = trap_Com_SoundAliasString(weapon->deploySound);
    weaponInfo->breakdownSound = trap_Com_SoundAliasString(weapon->breakdownSound);

    if (itemInfo->pickupSound == NULL) {
        itemInfo->pickupSound = trap_Com_SoundAliasString("weap_pickup");
    }
    itemInfo->pickupSoundAlt = trap_Com_SoundAliasString(weapon->ammoPickupSound);
    if (itemInfo->pickupSoundAlt == NULL) {
        itemInfo->pickupSoundAlt = trap_Com_SoundAliasString("weap_ammo_pickup");
    }
    if (weapon->shellEjectEffect[0] != '\0') {
        weaponInfo->shellEjectEffect = (uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)weapon->shellEjectEffect);
    }
    if (weapon->lastShotEjectEffect[0] != '\0') {
        weaponInfo->lastShotEjectEffect = (uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)weapon->lastShotEjectEffect);
    } else {
        weaponInfo->lastShotEjectEffect = weaponInfo->shellEjectEffect;
    }

    if (weapon->clipModel[0] != '\0') {
        weaponInfo->clipModelHandle = CG_RegisterModel(weapon->clipModel, CG_REGISTER_WEAPON_MODEL_CATEGORY);
        if (weaponInfo->clipModelHandle == 0) {
            Com_ErrorMessage(cg_registerWeaponInvalidProjectileModel, weapon->pickupName, weapon->worldModel);
        }
    }
    if (weapon->projectileExplosionEffect[0] != '\0') {
        weaponInfo->projectileExplosionEffect = (uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)weapon->projectileExplosionEffect);
    }
    weaponInfo->projectileExplosionSound = trap_Com_SoundAliasString(weapon->projectileExplosionSound);
    if (weapon->projectileTrailEffect[0] != '\0') {
        weaponInfo->projectileTrailEffect = (uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)weapon->projectileTrailEffect);
    }
    weaponInfo->projectileDLight = (float)(long double)coduo_int32_from_bits((uint32_t)weapon->projectileDLight);

    if (weapon->hudIcon[0] != '\0') {
        weaponInfo->hudIconShader = CG_RegisterMaterial(weapon->hudIcon, CG_REGISTER_WEAPON_SHADER_SORT);
        cg_weaponHudIcons[weaponIndex] = (qhandle_t)weaponInfo->hudIconShader;
    } else {
        cg_weaponHudIcons[weaponIndex] = cgs_media_usableHintShaders[CURSOR_HINT_ACTIVATE];
    }
    if (weapon->killIcon[0] != '\0') {
        (void)CG_RegisterMaterial(weapon->killIcon, CG_REGISTER_WEAPON_SHADER_SORT);
    }
    if (weapon->modeIcon[0] != '\0') {
        weaponInfo->modeIconShader = CG_RegisterMaterial(weapon->modeIcon, CG_REGISTER_WEAPON_SHADER_SORT);
    }
    if (weapon->ammoIcon[0] != '\0') {
        weaponInfo->ammoIconShader = CG_RegisterMaterial(weapon->ammoIcon, CG_REGISTER_WEAPON_SHADER_SORT);
        cg_weaponAmmoIcons[weaponIndex] = (qhandle_t)weaponInfo->ammoIconShader;
    } else {
        cg_weaponAmmoIcons[weaponIndex] = cgs_media_usableHintShaders[CURSOR_HINT_ACTIVATE];
    }

    {
        const char *reference = weapon->displayName;
        const char *translated = (const char *)(intptr_t)cgame_syscall(CG_SE_TRANSLATE_REFERENCE, (intptr_t)reference);

        if (translated == NULL) {
            if (cl_languagewarnings_vmCvar.integer != 0) {
                if (cl_languagewarningsaserrors_vmCvar.integer != 0) {
                    Com_Error(ERR_LOCALIZATION, cg_registerWeaponErrorDisplayName, weapon->pickupName, reference);
                } else {
                    Com_Printf(cg_registerWeaponWarnDisplayName, weapon->pickupName, reference);
                }
            }
            translated = reference;
        }
        weaponInfo->displayName = translated;
    }

    {
        const char *reference = weapon->modeName;
        const char *translated = (const char *)(intptr_t)cgame_syscall(CG_SE_TRANSLATE_REFERENCE, (intptr_t)reference);

        if (translated == NULL) {
            if (cl_languagewarnings_vmCvar.integer != 0) {
                if (cl_languagewarningsaserrors_vmCvar.integer != 0) {
                    Com_Error(ERR_LOCALIZATION, cg_registerWeaponErrorModeName, weapon->pickupName, reference);
                } else {
                    Com_Printf(cg_registerWeaponWarnModeName, weapon->pickupName, reference);
                }
            }
            translated = reference;
        }
        weaponInfo->modeName = translated;
    }

    {
        const char *reference = weapon->aiOverlayDescription;
        const char *translated = (const char *)(intptr_t)cgame_syscall(CG_SE_TRANSLATE_REFERENCE, (intptr_t)reference);

        if (translated == NULL) {
            if (cl_languagewarnings_vmCvar.integer != 0) {
                if (cl_languagewarningsaserrors_vmCvar.integer != 0) {
                    Com_Error(ERR_LOCALIZATION, cg_registerWeaponErrorAiOverlay, weapon->pickupName, reference);
                } else {
                    Com_Printf(cg_registerWeaponWarnAiOverlay, weapon->pickupName, reference);
                }
            }
            translated = reference;
        }
        weaponInfo->aiOverlayDescription = translated;
    }
}
