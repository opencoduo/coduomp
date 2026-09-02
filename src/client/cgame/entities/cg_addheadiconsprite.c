// Source: uo_cgame_mp_x86.dll 0x30032910..0x30032ac0
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30032910_30032ac0.mcode
//
// CG_AddHeadIconSprite (provisional-by-role) — build and submit one camera-facing
// icon sprite positioned over an entity's head, used to draw the marker above a
// client (the voice-chat / talking / follow icon over the head).
//
// Naming adjudication: the .mcode header carries the size-guessed corpus name
// "CG_VoiceChat" (win size 0x1b0 matched size 0x1b0). That is a SIZE match, which the
// workflow forbids as identification, and the body does no voice-chat string/buffer
// work at all — it is a pure render-entity emitter. Its five callers
// (0x30032b4d..0x30032c11, the CG_AddHeadIcon dispatcher at 0x30032b20) each pick a
// sprite-shader handle (cg_headIconShaders at 0x3044b6d0/d4/d8) and a per-icon radius
// scale (via 0x3006be3c) for a speaking/spectated/objective condition and hand it
// here. So this is the head-icon SPRITE builder, not the voice-chat command. The
// address-shaped fallback is avoided; the role name is used with this note. If a
// later reconstruction of the 0x30032b20 dispatcher proves the exact CoD symbol,
// supersede this name at the declaration.
//
// ABI: __cdecl, caller-cleaned (callers do `add esp,0xc`). The subject entity arrives
// in ESI (a register argument in this module's regparm-style ABI; callers load it
// before the call, e.g. 0x30032b62 CMP [ESI],eax). The three caller-cleaned stack
// args are, in order:
//   arg0 sfxOrShaderHandle (renderer handle, [EBP+0x8])  -> refEntity.spriteShaderHandle (+0x68)
//   arg1 iconScale (int, [EBP+0xc]), FILD-converted       -> Z lift and radius calc
//   arg2 attenuateByDistance (flag, [EBP+0x10])
// The register arg is modeled as the leading ordered parameter (as the committed
// sibling reconstructions in this module do); no calling-convention attribute is
// added for the syntax-only build.
//
// Frame: MSVC /GS. The prologue snapshots __security_cookie into the canary slot and
// the epilogue verifies it via __security_check_cookie (0x30061639). AND ESP,~7 gives
// the 8-byte alignment for the x87 spills. These are calling-convention details,
// recorded here, not modeled as source statements.

#include <math.h> /* sqrtl: the inline x87 FSQRT at 0x300329ed */
#include <stdint.h>
#include <string.h>
#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* The fixed head-bone tag name the sprite is pinned to: .rdata "Bip01 Head" at
 * 0x30079af8, loaded into EAX just before the bone-matrix call. */
static const char CG_HEAD_TAG_NAME[] = "Bip01 Head";

void CG_AddHeadIconSprite(centity_t *entity, int32_t sfxOrShaderHandle,
                          int32_t iconScale, int32_t attenuateByDistance)
{
    /* ---- Spectator/follow-view renderfx gate (0x30032928..0x30032952) ------------
     * renderfx = RF_THIRD_PERSON when the local player is following/spectating this
     * very entity in first person AND the mid-transition follow-view flag is clear;
     * otherwise 0. This keeps the head icon out of the first-person view. */
    int32_t renderfx = 0;
    if ((cg_snap->ps.playerStateFlags & PSF_PLAYER_ENTITY_MASK) != 0 &&  /* TEST [cg_snap+0x18],0xc0000 */
        entity->currentState.number == cg_snap->ps.psClientNum &&                    /* CMP [ESI],[cg_snap+0xe0] */
        cg_thirdPerson == 0) {                                      /* TEST [0x304831c0] */
        /* 0x304831c0: a per-frame follow/intermission view gate written by the
         * function at 0x30042510 as (cg_nextSnap->ps.pmType >= 6 || flag@0x3044f2ac).
         * Its mechanical owner label ("pm_viewheighttablelerp") is wrong; the exact
         * source name is not resolved from current evidence, so the mechanical symbol
         * is kept (no alias). Tested here as a boolean guard only. */
        renderfx = (int32_t)RF_THIRD_PERSON;                          /* MOV EBX,2 */
    }

    /* ---- Icon origin: head tag if available, else the entity origin --------------
     * trap(CG_DOBJ_GET_HANDLE=0xa5, entity->currentState.number) returns this entity's DObj
     * handle; nonzero means it has a skeleton. Carried through the x87 math as:
     *   originX     -> ST1 (refEntity.origin.x)
     *   originYBits -> a raw dword ([ESP+0x10]) (refEntity.origin.y, stored verbatim)
     *   originZ     -> ST0 (iconScale + origin.z + fixed lift) */
    float   originX;
    int32_t originYBits;
    /* originZ is a raw-st value: the FILD/FADD/FADD chain (0x3003298a..0x3003299b
     * / 0x300329a9..0x300329b6) is never stored — it rides the x87 stack through
     * the distance math and only rounds at the final refEntity origin[2] FSTP
     * (0x30032a4e), so it stays long double. */
    long double originZ;

    intptr_t dobjHandle = cgame_syscall(CG_DOBJ_GET_HANDLE, entity->currentState.number); /* push entityNum; push 0xa5 */

    /* Exact DObjSkelMat output record at [ESP+0x18]; its origin row is at
     * +0x30..+0x38. */
    DObjSkelMat boneMatrix;
    int   haveHeadTag = 0;

    if (dobjHandle != 0) {
        /* The caller loads the tag name into EAX right before the call (MOV EAX,
         * 0x30079af8 = "Bip01 Head"); self = dobjHandle (ECX at the call). EAX is
         * the tagName argument the callee (CG_DObjGetWorldTagMatrix, 0x3001fdf0)
         * forwards to trap(0xb2, self, tagName) — proven from its body; the earlier
         * "EAX is scratch" reading was wrong and the signature is now widened. */
        haveHeadTag = CG_DObjGetWorldTagMatrix((void *)dobjHandle,
                                                      CG_HEAD_TAG_NAME, entity,
                                                      &boneMatrix);
    }

    if (dobjHandle != 0 && haveHeadTag) {
        /* Head-tag branch (0x30032982..0x3003299b): at the head bone plus an 18-unit
         * lift, raised by iconScale. */
        originX     = boneMatrix.origin[0];                  /* FLD [ESP+0x48] (tag.x) */
        originYBits = CG_FloatBits(boneMatrix.origin[1]);    /* MOV [ESP+0x10],[ESP+0x4c] (tag.y bits) */
        originZ = (long double)iconScale +
                  (long double)boneMatrix.origin[2] + 18.0L;
    } else {
        /* No-tag branch (0x3003299d..0x300329b6): at the entity's placement origin
         * (centity_t +0x208, the interpolated placement vec3 the entity copier
         * fills; the canonical centity name is lerpOrigin) plus a 72-unit lift. */
        originX     = entity->lerpOrigin[0];               /* FLD [ESI+0x208] */
        originYBits = CG_FloatBits(entity->lerpOrigin[1]); /* MOV [ESP+0x10],[ESI+0x20c] */
        originZ = (long double)iconScale +
                  (long double)entity->lerpOrigin[2] + 72.0L;
    }

    /* ---- Distance-based radius (0x300329bc..0x30032a34) ---------------------------
     * When attenuateByDistance is set, scale the sprite by its distance from the
     * current view/camera origin (cg_refdef.vieworg, the .data vec3 at
     * 0x30487a90..0x30487a98) and lift origin.z; otherwise use a fixed radius and
     * leave z alone. */
    /* The whole attenuation chain — subtractions, squares, FSQRT, the 0.6 clamp
     * FCOM and both products — runs in st registers with NO float store
     * (0x300329c3..0x30032a2c); the only roundings are the refEntity FSTP/FSTs
     * at 0x30032a47..0x30032a63.  All the locals are long double for that
     * reason (cg_addscalefade.c precedent). */
    long double radius;   /* ST0 at the store block (refEntity.radius / radius2) */
    long double finalZ;   /* ST1 at the store block (refEntity.origin.z) */

    if (attenuateByDistance != 0) {
        float originY = CG_FloatFromBits((uint32_t)originYBits);
        long double dx =
            (long double)originX - (long double)cg_refdef.vieworg[0];
        long double dy =
            (long double)originY - (long double)cg_refdef.vieworg[1];
        long double dz =
            originZ - (long double)cg_refdef.vieworg[2];
        long double dist = sqrtl(dz * dz + dy * dy + dx * dx); /* FSQRT (dz^2 + dy^2 + dx^2), summed in that order */

        /* scaled = dist*(1/256) + 0.2, floored to 0.6. The FCOM/FNSTSW/TEST AH,5/JP
         * keeps the value when value >= 0.6 and clamps up to 0.6 otherwise, i.e.
         * max(value, 0.6). */
        long double scaled = dist * 0.00390625f + 0.2f;      /* FMUL 1/256; FADD 0.2 */
        if (scaled < 0.6f)                                   /* FCOM 0.6; clamp-up path */
            scaled = 0.6f;

        finalZ = originZ + (8.0f * scaled - 8.0f);           /* FLD 8; FMUL scaled; FSUB 8; FADDP into z */
        radius = scaled * 6.66f;                             /* FMUL 6.66 */
    } else {
        finalZ = originZ;
        radius = 6.66f;                                     /* FLD [0x3007c2cc] */
    }

    /* ---- Build the sprite refEntity and submit it (0x30032a38..0x30032aab) --------
     * The whole refEntity (39 dwords) is zeroed first (rep stosd), so any field not
     * written stays 0. origin is stored non-sequentially: x and z via x87 spills, y
     * copied as the raw dword carried through the math. */
    refEntity_t re;
    memset(&re, 0, sizeof(re));                              /* XOR EAX; MOV ECX,0x27; rep stosd */

    /* 0x30032a47..0x30032a63 drains the three live x87 values in the
     * retail order: origin.x, origin.z, radius at +0x7c (FST), then the same
     * radius at +0x64 (FSTP). */
    re.origin[0] = originX;                                 /* FSTP [+0x44] */
    re.origin[2] = finalZ;                                  /* FSTP [+0x4c] */
    re.radius  = radius;                                    /* FST  [+0x7c] (no pop) */
    re.radius2 = radius;                                    /* FSTP [+0x64] */
    re.spriteShaderHandle = sfxOrShaderHandle;               /* MOV [+0x68],EAX */

    /* These integer stores follow the radius stores in the original body.
     * origin.y is the raw dword carried from the selected source position. */
    memcpy(&re.origin[1], &originYBits, sizeof(originYBits)); /* MOV [+0x48],EDX */
    re.reType   = (int32_t)RT_SPRITE;             /* MOV [+0x00],4 */
    re.renderfx = renderfx;                                  /* MOV [+0x04],EBX */

    re.shaderRGBA[0] = 0xff;                                /* MOV byte [+0x6c],0xff */
    re.shaderRGBA[1] = 0xff;                                /* MOV byte [+0x6d],0xff */
    re.shaderRGBA[2] = 0xff;                                /* MOV byte [+0x6e],0xff */
    re.shaderRGBA[3] = 0xff;                                /* MOV byte [+0x6f],0xff (opaque white) */

    trap_R_AddRefEntityToScene(&re);                        /* push &re; push 0x3d; call cgame_syscall */

    /* epilogue: __security_check_cookie(canary); RET (caller-cleaned) */
}
