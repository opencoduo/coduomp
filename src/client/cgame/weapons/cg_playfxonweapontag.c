// Source: uo_cgame_mp_x86.dll 0x30045c10..0x30045c91
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30045c10_30045c91.mcode
//
// CG_PlayFxOnWeaponTag - resolve a named tag on one of a weapon's cached DObj
// handles and play a registered effect on that tag.
//
// Rejected .mcode name guess `script_func_randomintrange`: that name was a pure
// size match (win size 0x81 == corpus size 0x81) and is contradicted by the
// machine code, which issues the cgame render/effect syscalls CG_RESOLVE_TAG
// (0xe3) and CG_PLAY_EFFECT_ON_TAG (0xe9) against a cg_weaponInfos[] DObj handle
// and then tail-calls a cgame draw helper. It is a cgame weapon/effect routine,
// not a script builtin.
//
// Custom register+stack ABI (all facts from the single caller at 0x30047efb and
// the callee-clean `add esp,0xc` after the call):
//   EAX (in) = selectViewDObj flag: when nonzero, prefer weapon->viewDObj (+0xc8)
//   EBX (in) = weaponIndex, scaled by sizeof(cgWeaponInfo_t)==0x1c4
//   EDI (in) = model/DObj object id resolved against for the tag (3rd arg of 0xe3)
//   stack arg0 [E+4] = effectHandle passed to CG_PLAY_EFFECT_ON_TAG
//   stack arg1 [E+8] = tagName string (2nd arg of 0xe3), also handed to the draw helper
//   stack arg2 [E+0xc] = gate: nonzero -> also run the weapon-tag draw helper
// The function pushes EBP=arg1 early (mov ebp,[esp+0x14]) and cleans nothing of
// its own args (cdecl callee-observed: `add esp,8`, `ret` with no imm).
//
// DObj selection (0x30045c13..0x30045c3f): dobj = (selectViewDObj &&
// weapon->viewDObj) ? weapon->viewDObj : weapon->worldDObj; if dobj is NULL the
// function returns without touching the engine.

#include "client/cgame/client_recovered.h"
#include "qcommon/fx_types.h"
#include "client/cgame/globals.h"

/*
 * FUN_30045550 - cgame weapon-tag draw/placement helper. Provisional,
 * caller-observed ABI only (this is its sole caller): push order EBP(tagName),
 * EBX(weaponIndex), EDI(model) => C args (model, weaponIndex, tagName); callee
 * cleans nothing (caller does `add esp,0xc`). Superseded by its own .mcode
 * reconstruction. Named by role: it renders using the just-resolved tag.
 */
void CG_PlayFxOnWeaponTag(qboolean selectViewDObj, int32_t weaponIndex, int32_t model, const vec3_t effectOrigin, const char *tagName,
                          int32_t drawTagModel)
{
    uint32_t effect;
    int32_t tagIndex;
    sfx_bolt_info_t boltInfo;

    /* 0x30045c13..0x30045c3f: select the view or world muzzle effect. */
    effect = 0;
    if (selectViewDObj) {
        effect = cg_weaponInfos[weaponIndex].viewFlashEffect; /* +0xc8 */
    }
    if (effect == 0) {
        effect = cg_weaponInfos[weaponIndex].worldFlashEffect; /* +0xcc */
        if (effect == 0) {
            return; /* JZ 0x30045c8b: no registered effect for this weapon */
        }
    }

    /* 0x30045c41..0x30045c5b: resolve the named tag on `model`.
     * cgame_syscall(0xe3, model, tagName) -> tag index; negative means the tag
     * does not exist (JL 0x30045c8b, signed). */
    boltInfo.entityNum = model;          /* mov [esp+0x14],edi (E-8) */
    tagIndex = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_RESOLVE_TAG, model, (intptr_t)tagName));
    boltInfo.boneIndex = tagIndex;       /* mov [esp+0xc],eax (E-4) */
    if (tagIndex < 0) {
        return;
    }

    /* 0x30045c5d..0x30045c7c: play the selected effect on the resolved DObj tag.
     * cgame_syscall(0xe9, effect, effectOrigin, 0, &boltInfo); the return is
     * ignored (mov eax,[esp+0x30] reloads stack arg2 immediately after). */
    (void)cgame_syscall(CG_PLAY_EFFECT_ON_TAG, (int32_t)effect, (intptr_t)effectOrigin, 0, (intptr_t)&boltInfo);

    /* 0x30045c7c..0x30045c88: when the gate arg is set, also run the draw helper
     * for this weapon tag. */
    if (drawTagModel != 0) {
        CG_FakeTrajectoryEffects(model, weaponIndex, tagName);
    }
}
