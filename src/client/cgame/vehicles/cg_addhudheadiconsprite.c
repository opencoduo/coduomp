// Source: uo_cgame_mp_x86.dll 0x300213c0..0x30021531
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300213c0_30021531.mcode
//
// CG_AddHudHeadIconSprite (0x300213c0) — build one camera-facing head-icon sprite
// refEntity_t at a client entity's interpolated origin and submit it to the render
// scene via trap_R_AddRefEntityToScene (cgame syscall id 0x3d). Called by
// CG_VehicleOwnerIcon (0x30021540) once per rotating-icon pulse phase.
//
// The .mcode mechanical size-guess name `script_method_player_setweaponslotclipammo`
// is REJECTED: that is a server GSC weapon/ammo script-method, and the size match is
// meaningless (the naming banks warn size never identifies a function). This body does
// no weapon-slot / clip-ammo / playerState work at all — it is a pure render-entity
// emitter that zeroes a stack refEntity_t, fills origin/radius/color, and hands it to
// the renderer. Resolved by behavior + call graph:
//   - trap id 0x3d through *cgame_syscall = trap_R_AddRefEntityToScene (one refEntity_t*);
//   - reType 4 = RT_SPRITE (camera-facing icon sprite);
//   - reads cg_snap (0x30459160), cg_refdef.vieworg (0x30487a90) and the view-follow
//     gate 0x304831c0 exactly like the sibling sprite builder CG_AddHeadIconSprite
//     (0x30032910); this is the head-tag-less variant that always uses cent+0x208.
// The existing caller-observed provisional decl in client_recovered.h already carries
// this exact signature; this file supersedes its "caller-observed only" status with the
// instruction-proven body.
//
// i386 details recorded but not modeled as source: the /GS stack cookie
// (__security_cookie @0x30081650 snapshotted on entry, verified via
// __security_check_cookie @0x30061639 on exit) and cdecl caller-cleanup (RET; the
// caller does ADD ESP,0x18 for the six 4-byte args).

#include <stdint.h>
#include <string.h>
#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "compat/coduo_native_x87.h"

/* _ftol2's argument arrives on the x87 stack and truncates toward zero. Only the
 * low byte of its EAX result is consumed here as the alpha channel. */

void CG_AddHudHeadIconSprite(centity_t *cent, qhandle_t material, int32_t yaw,
                             int32_t drawFlag, int32_t secondaryAngle, float alphaScale)
{
    /* ---- Third-person renderfx gate (0x300213cc..0x30021408) ----------------------
     * renderfx = RF_THIRD_PERSON (2) only when the local player is in a first-person
     * view (PSF_PLAYER_ENTITY_MASK bits set in ps.playerStateFlags), this very
     * entity is the local player's own client entity, and the mid-transition view
     * gate (0x304831c0) is clear — so the player's own icon is suppressed except in
     * the third-person render pass. Otherwise 0. */
    int32_t renderfx = 0;                                                /* XOR EDX,EDX default */
    if ((cg_snap->ps.playerStateFlags & PSF_PLAYER_ENTITY_MASK) != 0 &&/* TEST [cg_snap+0x18],0xc0000 / JZ */
        cent->currentState.number == cg_snap->ps.psClientNum &&                         /* CMP [ESI],[cg_snap+0xe0] / JNZ */
        cg_thirdPerson == 0) {                                         /* TEST [0x304831c0] / JNZ */
        /* 0x304831c0: per-frame follow/intermission view gate (its mechanical owner
         * label "pm_viewheighttablelerp" is wrong and its exact source name is not
         * resolved from current evidence, so the mechanical symbol is kept, no alias).
         * Tested here as a boolean guard only. */
        renderfx = (int32_t)RF_THIRD_PERSON;                             /* MOV EDX,2 */
    }

    /* ---- Sprite world origin (0x30021408..0x30021426) -----------------------------
     * Taken from the entity's interpolated placement block (cent->lerpOrigin, +0x208;
     * the caller copies the render origin here before the call), with `yaw` (an integer)
     * added to z and a fixed 72-unit lift. y is carried verbatim as its raw dword (the
     * machine stores [cent+0x20c] straight through to refEntity.origin[1]). */
    float   originX     = cent->lerpOrigin[0];                            /* FLD [EAX+0x208] */
    int32_t originYBits = CG_FloatBits(cent->lerpOrigin[1]);              /* MOV ECX,[EAX+0x20c]; -> [ESP+8] */
    long double originZ =
        (long double)yaw + (long double)cent->lerpOrigin[2] + 72.0L;

    /* ---- Distance-based radius / z-lift (0x30021428..0x3002149f) -------------------
     * When drawFlag is set, scale the sprite by its distance from the current
     * view/camera origin (cg_refdef.vieworg, .data vec3 @0x30487a90..0x30487a98) and
     * lift its z; otherwise use a fixed radius and leave z alone. */
    long double radius;   /* ST0 at the store block (refEntity.radius / radius2) */
    long double finalZ;   /* origin.z at the store block */

    if (drawFlag != 0) {                                                 /* TEST [EBP+0x14]; JZ 0x30021499 */
        float originY = CG_FloatFromBits((uint32_t)originYBits);
        long double dx =
            (long double)originX - (long double)cg_refdef.vieworg[0];
        long double dy =
            (long double)originY - (long double)cg_refdef.vieworg[1];
        long double dz =
            originZ - (long double)cg_refdef.vieworg[2];
        long double dist = coduo_x87_sqrtl(
            dz * dz + dy * dy + dx * dx);

        /* scaled = max(dist/256 + 0.2, 0.6). The FCOM 0.6 / FNSTSW / TEST AH,5 / JP
         * keeps the value when it is < 0.6 is FALSE, i.e. clamps UP to 0.6: the
         * fall-through (C0=0 => ST0 >= 0.6) keeps ST0; the JP-not-taken path (C0=1 =>
         * ST0 < 0.6) replaces it with 0.6. */
        long double scaled = dist * 0.00390625L + (long double)0.2f;
        if (scaled < (long double)0.6f)
            scaled = (long double)0.6f;

        finalZ = originZ + (8.0L * scaled - 8.0L);
        radius = scaled * 20.0L;
    } else {
        finalZ = originZ;
        radius = 20.0f;                                                  /* FLD [0x3007be04] = 20.0 */
    }

    /* ---- Build the sprite refEntity and submit it (0x3002149f..0x30021517) ---------
     * The whole refEntity (39 dwords) is zeroed first (rep stosd), so any field not
     * written stays 0. origin is stored non-sequentially: x and z via x87 spills, y as
     * the raw dword carried through the math. */
    refEntity_t re;
    memset(&re, 0, sizeof(re));                                          /* XOR EAX; MOV ECX,0x27; LEA EDI,[ESP+0x10]; rep stosd */

    re.reType   = (int32_t)RT_SPRITE;                        /* MOV [+0x00],4 */
    re.renderfx = renderfx;                                             /* MOV [+0x04],EDX */

    re.origin[0] = originX;                                             /* FSTP [+0x44] */
    re.origin[2] = (float)finalZ;                                       /* FSTP [+0x4c] */
    memcpy(&re.origin[1], &originYBits, sizeof(originYBits));           /* MOV [+0x48],EAX (raw y dword) */

    re.radius  = (float)radius;                                        /* FST  [+0x7c] (no pop) */
    re.radius2 = (float)radius;                                        /* FSTP [+0x64] (duplicate radius slot) */
    re.spriteShaderHandle = material;                                  /* MOV [+0x68],ECX (arg1 render handle) */

    /* secondaryAngle (=180 at the call site) is stored as its float bit pattern into
     * the customShader slot when nonzero — the machine does FILD arg4 / FSTP [+0x80],
     * writing float(secondaryAngle) bits there, not the integer. */
    if (secondaryAngle != 0)                                           /* TEST [EBP+0x18]; JZ 0x300214e9 */
        re.rotation = (float)secondaryAngle;                            /* FILD [EBP+0x18]; FSTP [+0x80] */

    re.shaderRGBA[0] = 0xff;                                           /* MOV byte [+0x6c],0xff */
    re.shaderRGBA[1] = 0xff;                                           /* MOV byte [+0x6d],0xff */
    re.shaderRGBA[2] = 0xff;                                           /* MOV byte [+0x6e],0xff (opaque white RGB) */
    /* alpha = trunc(alphaScale * 255), low byte only (FLD arg5; FMUL 255.0;
     * CALL `_ftol2`). */
    re.shaderRGBA[3] = (uint8_t)coduo_fp_to_i32_extended(
        (long double)alphaScale * 255.0L);

    trap_R_AddRefEntityToScene(&re);                                   /* LEA EDX,&re; push &re; push 0x3d; call *cgame_syscall */

    /* epilogue: __security_check_cookie(canary); RET (cdecl, caller-cleaned) */
}
