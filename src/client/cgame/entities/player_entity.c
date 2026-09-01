// Source: uo_cgame_mp_x86.dll 0x300343e0..0x300346bf
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300343e0_300346bf.mcode
//
// CG_Player (role name) — the eType == ET_PLAYER (== 1) arm of the cgame per-entity
// render dispatcher. The dispatcher (jump table at 0x30022228, indexed by
// currentState.eType at 0x30022186) reaches this function through its index-1 slot
// (0x3002219f: MOV ECX,EBX / CALL 0x300343e0), so the subject centity arrives in ECX
// (register-argument client ABI). Index 2 (0x300221aa) is CG_AddPlayerCorpseEntity
// (0x300346c0), this handler's immediate sibling; the two share the DObj model-render
// idiom. This function drives a live player's DObj model: it updates model-part effects,
// runs mounted/turret and view-weapon passes, builds and submits the player model
// refEntity to the render scene, adds the head icon, and — for a flamethrower-carrying
// player — emits flame chunks from the "tag_flash" bone.
//
// The .mcode header's mechanical pre-hint "PM_NoclipMove" is REJECTED. This function
// performs NO pmove/noclip movement: there is no usercmd, no pml.forward/right/up basis,
// no velocity integration, no friction, and no trace/clip. It is a client-render handler
// (DObj handle traps 0xa5, refEntity submit trap 0x3d, head-icon and effect passes). The
// "win size 0x2df matched 0x2e0" note is a pure size-match and is exactly the kind of
// size guess the naming rules forbid. Named by behavior + call-graph position (the
// ET_PLAYER slot of the eType dispatch table); exact CoD source symbol unproven.
//
// Frame / ABI facts (all against the .mcode):
//   * SUB ESP,0x104 then PUSH EBX/EBP/ESI/EDI: a large stack frame plus 4 callee-saved
//     regs. MOV EAX,[__security_cookie] at entry and CALL __security_check_cookie
//     (0x30061639) at every exit are MSVC /GS canary bookkeeping (i386 detail, not
//     source-level), so they are not modeled.
//   * cent arrives in ECX (MOV ESI,ECX). modelInfo = &cent->corpseModelInfo (cent+0xf4),
//     the player's DObj model-info record (entityState_t); LEA EBP,[ESI+0xf4].
//   * Several callees take register arguments (EBX/EAX/ESI/EDX). Where the machine code
//     leaves pushed stack args uncleaned across two calls it is a calling-convention
//     detail; the source-level arguments are what is expressed below.
//
// x87: every FLD/FST/FADD is single-precision (DWORD ptr). The world-Z component of the
// origin is carried through an int register (its raw bits, EDX) into re.origin[2] and
// re.oldorigin[2], so it is copied bit-exact; only the lighting-origin Z is arithmetic.
//
// Light-bias .rdata float constants (dumped EXACTLY via objdump -s -j .rdata, NOT
// inferred from a neighbor): 0x3007bdc4 = 12.0f, 0x3007be04 = 20.0f, 0x3007bdd0 = 32.0f.
// The map here is 0x40->12, 0x20->20, else->32 (FADDs at 0x30034574/0x30034581/
// 0x30034589). The corpse handler (0x300347ba/0x300347c6/0x300347ce) uses the SAME
// map against the same three addresses -- an earlier note here claimed it swapped
// 20/32, which was read from that file's own (transposed) source rather than from
// the bytes; both were corrected on the 2026-07-16 float-rounding pass.

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Layout guards for the offsets/strides this function relies on (4-byte i386 pointer
 * width). centity_t / entityState_t / refEntity_t field offsets are
 * asserted next to their definitions in the shared headers; here we assert only the
 * few this file dereferences by name and that contain no pointer (safe at both ABIs),
 * plus the pointer-containing ones under a 32-bit guard. */
_Static_assert(offsetof(entityState_t, eFlags) == 0x08,
               "modelInfo.eFlags +0x08");
_Static_assert(offsetof(entityState_t, clientNum) == 0x94,
               "modelInfo.clientNum +0x94");
_Static_assert(offsetof(entityState_t, weapon) == 0xcc,
               "modelInfo.weapon +0xcc");
_Static_assert(offsetof(entityState_t, fTorsoHeight) == 0xe8,
               "modelInfo.fTorsoHeight +0xe8");
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(offsetof(clientInfo_t, viewPitch) == 0x3e8, "animState.viewPitch +0x3e8");
_Static_assert(offsetof(clientInfo_t, viewYaw) == 0x3ec, "animState.viewYaw +0x3ec");
_Static_assert(offsetof(clientInfo_t, viewRoll) == 0x3f0, "animState.viewRoll +0x3f0");
/* weaponInfo_t contains pointer fields (name +0x04, secondaryDisplayName +0x78), so
 * weaponType (+0x7c) is only at that offset at 4-byte i386 pointer width. */
_Static_assert(offsetof(weaponInfo_t, weaponType) == 0x7c, "weaponInfo_t.weaponType +0x7c");
_Static_assert(offsetof(struct centity_s, corpseModelInfo) == 0xf4, "cent.modelInfo +0xf4");
_Static_assert(offsetof(struct centity_s, lerpOrigin) == 0x208, "cent.lerpOrigin +0x208");
_Static_assert(offsetof(struct centity_s, lerpAngles) == 0x214, "cent.lerpAngles +0x214");
_Static_assert(offsetof(snapshot_t, ps.playerStateFlags) == 0x18,
               "cg_snap.ps.playerStateFlags +0x18");
_Static_assert(offsetof(snapshot_t, ps.psClientNum) == 0xe0, "cg_snap.clientNum +0xe0");
#endif

/* The player DObj model-part table is bgs.clientinfo[] at 0x305e1f34 with a
 * 0x4d0 clientInfo_t stride. BG_PlayerAnimation and the view-angle reads operate
 * directly on that shared record. */

/* Light-bias flag bits selecting the lighting-origin Z bias (.rdata float pool, exact
 * addresses dumped): 0x40 -> +12.0f (0x3007bdc4), 0x20 -> +20.0f (0x3007be04),
 * else -> +32.0f (0x3007bdd0). */
#define CG_PLAYER_LIGHTOFS_FLAG40 ((uint32_t)0x40)
#define CG_PLAYER_LIGHTOFS_FLAG20 ((uint32_t)0x20)

void CG_Player(centity_t *cent)
{
    entityState_t *modelInfo = &cent->corpseModelInfo; /* cent+0xf4 */

    /* 0x300343fd TEST byte [modelInfo.eFlags],0x81 ; JNZ exit — skip when the model is
     * not drawable (bit 0x80) or explicitly suppressed (bit 0x01). */
    if (((uint8_t)modelInfo->eFlags & (EF_NODRAW | EF_DEAD)) != 0)
        return;

    uint32_t modelPartIndex = modelInfo->numberBits;       /* [EBP]      */
    uint32_t animStateIndex = modelInfo->clientNumBits;    /* [EBP+0x94] */

    /* 0x3003441b handle = trap(0xa5, modelPartIndex) — DObj skeleton handle. A zero
     * handle means this player has no live DObj this frame; draw nothing. */
    intptr_t dobjHandle = cgame_syscall(CG_DOBJ_GET_HANDLE, (int32_t)modelPartIndex);
    if (dobjHandle == 0)
        return;

    /* 0x30034430..0x3003443b REP STOSD zeroes 0x27 dwords = 0x9c bytes = the whole
     * refEntity_t. */
    refEntity_t re;
    memset(&re, 0, sizeof(re));

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if (animStateIndex >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_Player: invalid client number %u",
                  animStateIndex);
        return;
    }
    clientInfo_t *animState = &bgs.clientinfo[animStateIndex];
    re.shaderRGBA[0] = 0xff;
    re.shaderRGBA[1] = 0xff;
    re.shaderRGBA[2] = 0xff;
    re.shaderRGBA[3] = 0xff;

    /* 0x3003446c BG_PlayerAnimation(renderEntity=modelInfo, self=animState). */
    BG_PlayerAnimation(modelInfo, animState);

    /* 0x30034477 TEST AH,0x60 -> flags & (0x2000|0x4000): mounted/turret attach pass. */
    if ((modelInfo->eFlags & EF_FORCED_STANCE_MASK) != 0)
        CG_PlayerTurretPositionAndBlend(cent);

    /* 0x30034488 TEST flags,0x100000 ; 0x3003448f TEST AL,1 — special view-path branch,
     * taken only when SPECIAL_VIEW is set and NODRAW is clear. */
    uint32_t flags = modelInfo->eFlags;
    if ((flags & EF_IN_VEHICLE) != 0 &&
        (flags & EF_DEAD) == 0) {
        /* 0x30034495 CG_PlayerVehiclePositionAndBlend(cent) (EBX=cent). Zero => add the
         * held weapon against the (zeroed, white-RGBA) refEntity and return early.
         * Both CG_AddPlayerWeapon call sites pass ECX = &re: LEA ECX,[ESP+0x78] after
         * exactly four pushes = frame+0x68 = re (NOT a separate scratch buffer — an
         * earlier draft passed an uninitialized local here). */
        if (CG_PlayerVehiclePositionAndBlend(cent) == 0) {
            CG_AddPlayerWeapon(&re, NULL, cent, qtrue, 0.0f);
            return; /* 0x300344bb epilogue */
        }
        /* falls through to AFTER_DRAW (0x300344c6) */
    }

    /* AFTER_DRAW (0x300344c6): build the player model orientation basis from the entity's
     * render angles (cent+0x214), then add the head icon and per-frame effects. */
    AnglesToAxisNegRight(re.axis, cent->lerpAngles);
    CG_AddHeadIcon(cent);       /* 0x300344da (EAX=cent) */
    CG_AddPlayerWaterShadow(cent); /* 0x300344df (ESI=cent) */

    /* 0x300344e4..0x3003452c compute the base renderfx. Start at 0; set RF_THIRD_PERSON
     * (0x2) only when the local player is drawing their OWN model in first-person view
     * (so the body is drawn in the third-person pass only), then clear it in the two
     * override cases below. */
    int32_t renderfx = 0;
    if ((cg_snap->ps.playerStateFlags & PSF_PLAYER_ENTITY_MASK) != 0) {
        if ((int32_t)modelInfo->numberBits == cg_snap->ps.psClientNum) {
            if (cg_thirdPerson == 0)
                renderfx = (int32_t)RF_THIRD_PERSON;
        }
    }
    /* 0x30034511 clear RF_THIRD_PERSON when the view/render-state gate is set. */
    if (g_cgScreenReadyState != 0)
        renderfx &= ~(int32_t)RF_THIRD_PERSON;
    /* 0x3003451e clear RF_THIRD_PERSON on the special view path. */
    if ((modelInfo->eFlags & EF_IN_VEHICLE) != 0)
        renderfx &= ~(int32_t)RF_THIRD_PERSON;

    /* 0x3003452c..0x300345b4 origin / lighting-origin / oldorigin build. cent+0x208 is the
     * render origin (lerpOrigin is a provisional misnomer for the render-origin vec3, as
     * noted on centity_t). origin.z is carried as raw int bits (EDX), so origin.z and
     * oldorigin.z are bit-exact copies; only lighting-origin.z is arithmetic. */
    float originX = cent->lerpOrigin[0];   /* [ESI+0x208] */
    float originY = cent->lerpOrigin[1];   /* [ESI+0x20c] */
    int32_t originZBits;                      /* [ESI+0x210] raw dword (EDX) */
    memcpy(&originZBits, &cent->lerpOrigin[2], sizeof originZBits);

    re.origin[0] = originX;
    re.origin[1] = originY;
    memcpy(&re.origin[2], &originZBits, sizeof originZBits);

    re.lightingOrigin[0] = originX;
    re.lightingOrigin[1] = originY;

    /* lighting-origin.z = origin.z + modelInfo.fTorsoHeight + a flag-selected bias.
     * The selector uses the low byte of modelInfo.eFlags (CL).
     * One 80-bit chain, one rounding: 0x30034565 FLD origin.z; 0x3003456c FADD
     * groundOffset; ONE flag-selected FADD; the single FSTP at 0x30034593. An
     * accumulating `lightZ +=` would round at every step. */
    float originZ;
    memcpy(&originZ, &originZBits, sizeof originZ);
    if ((modelInfo->eFlags & CG_PLAYER_LIGHTOFS_FLAG40) != 0)
        re.lightingOrigin[2] = (float)(
            (long double)originZ +
            (long double)modelInfo->fTorsoHeight +
            (long double)12.0f);  /* FADD [0x3007bdc4] */
    else if ((modelInfo->eFlags & CG_PLAYER_LIGHTOFS_FLAG20) != 0)
        re.lightingOrigin[2] = (float)(
            (long double)originZ +
            (long double)modelInfo->fTorsoHeight +
            (long double)20.0f);  /* FADD [0x3007be04] */
    else
        re.lightingOrigin[2] = (float)(
            (long double)originZ +
            (long double)modelInfo->fTorsoHeight +
            (long double)32.0f);  /* FADD [0x3007bdd0] */

    re.oldorigin[0] = originX;
    re.oldorigin[1] = originY;
    memcpy(&re.oldorigin[2], &originZBits, sizeof originZBits);

    /* 0x3003454c/0x300345a0 renderfx |= RF_LIGHTING_ORIGIN | RF_DOBJ_MODEL. */
    renderfx |= (int32_t)RF_LIGHTING_ORIGIN;
    renderfx |= (int32_t)RF_DOBJ_MODEL;
    re.renderfx = renderfx;

    /* 0x300345ac..0x300345ce fill the remaining refEntity fields. */
    re.reType = RT_MODEL;              /* EBX = 1 */
    re.dobj = (struct DObj_s *)(uintptr_t)dobjHandle; /* [ESP+0x14] */
    re.owner = cent;                   /* ESI */

    /* 0x300345dc submit the player model to the current render scene. */
    cgame_syscall(CG_R_ADD_REF_ENTITY_TO_SCENE, (intptr_t)&re);

    /* 0x300345e5 CG_PlayerShadow(&shadowPlane, cent) — projects the ground blob
     * shadow (markShadow) beneath the model and writes the shadow-plane Z into
     * shadowPlane. The out pointer reuses the frame slot the compiler earlier used
     * to spill dobjHandle (LEA EAX,[ESP+0x14]) — a stack-reuse artifact, modeled as
     * a plain local. cent stays live in ESI as the second register argument. */
    float shadowPlane;
    CG_PlayerShadow(&shadowPlane, cent);

    /* 0x300345ee TEST [modelInfo.eFlags],BL (BL==1) ; JNZ skip — add the held weapon
     * against the submitted player refEntity (ECX = &re, see above) unless NODRAW
     * is set. */
    if ((modelInfo->eFlags & EF_DEAD) == 0)
        CG_AddPlayerWeapon(&re, NULL, cent, qtrue, 0.0f);

    /* 0x30034605 TAIL: flame-chunk emission for a flamethrower-carrying player.
     * First, a local-player first-person guard: when the view/render-state gate is
     * clear and this model's animStateIndex equals the local player's client number,
     * skip (don't emit the local player's own flame here). The compare at 0x30034614
     * reads frame slot +0x18, which 0x30034417 (MOV [ESP+0x20],EBX two pushes deep)
     * stored the animStateIndex into at function entry. */
    if (cg_thirdPerson == 0) {
        if ((int32_t)animStateIndex == cg_snap->ps.psClientNum)
            return; /* 0x300346a8 exit */
    }

    /* 0x30034624 TEST AH,0x2 -> flags & 0x200: only players flagged as flame-weapon
     * carriers reach the emission. */
    if ((modelInfo->eFlags & EF_FIRING) == 0)
        return;

    /* 0x3003462c the held weapon must be a GAS-type weapon (the flamethrower). */
    const int32_t weaponIndex = modelInfo->weaponIndex;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if (weaponIndex <= 0 || weaponIndex > bg_numWeapons ||
        (uint32_t)weaponIndex >= (uint32_t)MAX_WEAPONS ||
        bg_weaponInfos[weaponIndex] == NULL) {
        Com_Printf("WARNING: CG_Player: invalid weapon index %i\n",
                   weaponIndex);
        return;
    }
    weaponInfo_t *weapon = bg_weaponInfos[weaponIndex]; /* [0x30134cd8 + idx*4] */
    if (weapon->weaponType != WEAPTYPE_GAS)               /* CMP [EDX+0x7c],4 */
        return;

    /* 0x30034641 re-query the DObj handle for the model part; a zero handle means no live
     * skeleton to attach the flame to. */
    intptr_t flameDobjHandle =
        cgame_syscall(CG_DOBJ_GET_HANDLE, (int32_t)modelInfo->numberBits);
    if (flameDobjHandle == 0)
        return;

    /* 0x30034659 resolve the "tag_flash" bone world matrix on the player DObj. A zero
     * (qfalse) return means the tag could not be resolved; skip the emission. */
    DObjSkelMat tagFlashMatrix;
    if (CG_DObjGetWorldTagMatrix((void *)flameDobjHandle,
                                        cg_muzzleFlashTagName,
                                        cent, &tagFlashMatrix) == qfalse)
        return;

    /* 0x30034670..0x300346a0 emit the flame chunks. The EAX register argument is a
     * stack triple seeded with this player's animation view angles (loads from
     * animState +0x3e8/+0x3ec/+0x3f0 stored to frame +0x1c/+0x20/+0x24; LEA
     * EAX,[ESP+0x30] after five pushes = frame+0x1c). The second stack argument is
     * LEA ECX,[ESP+0x64] after three pushes = frame+0x58 = tagFlashMatrix.origin —
     * the translation row of the tag_flash world matrix filled above (the bone's
     * world position), used by the callee as the emit-origin base. */
    vec3_t flameViewAngles;
    flameViewAngles[0] = animState->viewPitch;  /* +0x3e8 */
    flameViewAngles[2] = animState->viewRoll;   /* +0x3f0 */
    flameViewAngles[1] = animState->viewYaw;    /* +0x3ec */

    CG_EmitPlayerFlameChunks(flameViewAngles, cent, tagFlashMatrix.origin, 1.8f, 1, 0);

    /* 0x300346a8 exit: __security_check_cookie(canary) (i386 /GS detail; not modeled). */
}
