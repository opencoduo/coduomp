// Source: uo_cgame_mp_x86.dll 0x30047be0..0x30047d15
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30047be0_30047d15.mcode

#include "client/cgame/client_recovered.h"
#include "qcommon/fx_types.h"
#include "client/cgame/globals.h"

/*
 * CG_EjectWeaponBrass (0x30047be0)
 *
 * Ejects the firing weapon's shell-casing ("brass") effect from the entity's
 * "tag_brass" bone. Called thiscall with `self` (the shared entityState-shaped
 * current/model record) in ECX and
 * a single cdecl stack arg `eventId` (the weapon-event id). Callers: CG_EntityEvent
 * (0x300230e6, event 0xa8 leg), the fire-effect path at 0x30023856, and
 * CG_FireWeapon (0x30048034).
 * The Mac CG_EjectWeaponBrass performs the matching weapon-DObj, tag_brass, and
 * effect-play sequence, resolving the source name.
 *
 * Control flow (proven from the .mcode):
 *   0x30047be0  MOV EAX,[cg_brass_vmCvar.integer]      master enable flag (0x3044f3cc)
 *   0x30047be8  TEST EAX; JZ end               skip everything if brass disabled
 *   0x30047bf0  CMP [self+0x4],0x10; JGE end   skip if self->eType >= 0x10
 *   0x30047bfa  MOV EAX,[self+0xcc]            weaponModel (index into cg_weaponInfos)
 *   0x30047c00  TEST EAX; JZ end               skip if no weapon
 *   0x30047c08  CMP EAX,[bg_numWeapons]; JLE ok
 *   0x30047c10  else Com_ErrorMessage("CG_FireWeapon: ent->weapon > BG_GetNumWeapons()")
 *               and RET (the shared CG_FireWeapon range error; reuse anchors identity).
 *
 * Effect selection (0x30047c22..): record = &cg_weaponInfos[weaponModel]
 * (IMUL 0x1c4; ADD 0x30413580). If eventId == 0xa6 the eject handle is
 * record->lastShotEjectEffect (+0x170); otherwise record->shellEjectEffect
 * (+0x16c). A zero handle skips the play (JZ end in each leg).
 *
 * Object id for the tag resolve (0x30047c45.. and its 0xa6-less twin 0x30047cab..):
 *   objId = self->number, UNLESS this is the local player's own first-person view --
 *   cg_snap->ps.playerStateFlags has PSF_PLAYER_ENTITY_MASK set AND
 *   self->number == cg_snap->ps.psClientNum -- in which case objId = weaponModel + 0x400
 *   (the view-weapon DObj handle, so the brass ejects from the first-person model).
 *
 * Play:
 *   boltInfo.entityNum = objId; boltInfo.boneIndex is filled by
 *   cgame_syscall(CG_RESOLVE_TAG, objId, "tag_brass"); if that is < 0 the tag does
 *   not exist and the play is skipped, otherwise
 *   cgame_syscall(CG_PLAY_EFFECT_ON_TAG, effectHandle, &cg_brassEffectOrigin, 0,
 *                 &boltInfo) plays the eject effect on the resolved tag.
 *
 * The two legs (0xa6 vs. not) are identical except for which record field supplies
 * the handle and (in the 0xa6 leg) the extra `[ESP+0x14] == 0xa6` guard; both build
 * the same boltInfo/origin/dir arguments to the 0xe9 trap.
 *
 * Name: role name proven by the "tag_brass" tag literal (0x3007a8cc) plus the shared
 * CG_FireWeapon out-of-range error string. The .mcode's size-matched
 * "script_method_scriptbuiltin_detach" guess and the earlier caller-observed
 * "CG_ItemPickupEvent" guess are both rejected -- this is cgame brass-ejection
 * effect play, not a script builtin nor an item-pickup handler.
 */

enum {
    CG_BRASS_ENTITY_TYPE_LIMIT = 16,
    CG_BRASS_LAST_SHOT_EVENT = 166
};

void CG_EjectWeaponBrass(entityState_t *self, int eventId)
{
    cgWeaponInfo_t *record;
    const uint32_t *effectHandleSlot;
    int32_t objId;
    sfx_bolt_info_t boltInfo;
    int32_t tagIndex;
    int32_t weaponIndex;
    snapshot_t *snapshot;

    /* Master brass-ejection switch and per-entity gates. */
    if (cg_brass_vmCvar.integer == 0) {
        return;
    }
    if (self->eType >= CG_BRASS_ENTITY_TYPE_LIMIT) {
        return;
    }
    weaponIndex = self->weapon; /* 0x30047bfa: retained in EAX */
    if (weaponIndex == 0) {
        return;
    }
    if (weaponIndex > bg_numWeapons) {
        /* 0x30047c10 pushes this object itself as the sole format argument. */
        Com_ErrorMessage(cg_weaponIndexOutOfRangeErrorMessage);
        return;
    }

    record = &cg_weaponInfos[weaponIndex];

    /* 0x30047c30-43: lastShotEjectEffect (+0x170) is an OPTIONAL override, used only
     * when it is nonzero AND eventId==0xa6 -- `test edx,edx; je 0x30047ca1` and
     * `cmp [esp+0x14],0xa6; jne 0x30047ca1` BOTH branch to the shell leg. Otherwise
     * (including eventId==0xa6 with a zero override) control falls through to
     * shellEjectEffect (+0x16c) at 0x30047ca1, which returns only if IT is zero
     * (0x30047ca9 je 0x30047d0f). A prior pass returned on a zero lastShotEjectEffect
     * in the 0xa6 arm, playing nothing instead of falling back to the shell effect. */
    effectHandleSlot = &record->lastShotEjectEffect;   /* +0x170 */
    if (*effectHandleSlot == 0 || eventId != CG_BRASS_LAST_SHOT_EVENT) {
        effectHandleSlot = &record->shellEjectEffect;  /* +0x16c */
        if (*effectHandleSlot == 0) {
            return;
        }
    }

    /* Resolve the eject tag on the firing object; for the local player's own
     * first-person view, eject from the view-weapon DObj (weaponModel + 0x400). */
    snapshot = cg_snap; /* 0x30047c45/0x30047cab: one snapshot-pointer load */
    if ((snapshot->ps.playerStateFlags & PSF_PLAYER_ENTITY_MASK) != 0 && self->number == snapshot->ps.psClientNum) {
        objId = coduo_int32_from_bits((uint32_t)weaponIndex + (uint32_t)CG_VIEW_WEAPON_DOBJ_HANDLE_BASE);
    } else {
        objId = self->number;
    }

    boltInfo.entityNum = objId;
    tagIndex = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_RESOLVE_TAG, objId, (intptr_t)cg_brassEjectTagName));
    boltInfo.boneIndex = tagIndex;
    if (tagIndex < 0) {
        return;
    }

    /* Play the eject effect on the resolved tag. The machine code reloads the
     * selected handle field after CG_RESOLVE_TAG (0x30047c8c/0x30047cee), so do
     * not retain its pre-syscall value across that call. The origin is the shared
     * brass-effect origin global at 0x301698c0 (the effect is tag-relative),
     * dir is NULL, and &boltInfo is the trailing SFxBoltInfo record. */
    cgame_syscall(CG_PLAY_EFFECT_ON_TAG, coduo_int32_from_bits(*effectHandleSlot), (intptr_t)cg_brassEffectOrigin, 0, (intptr_t)&boltInfo);
}
