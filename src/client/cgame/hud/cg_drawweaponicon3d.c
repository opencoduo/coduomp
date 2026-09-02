#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x30019ba0..0x30019ced
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30019ba0_30019ced.mcode
//
// CG_DrawWeaponIcon3D — draw the current effect entity's weapon HUD icon, centered
// inside the cropped 3D-view rectangle (cg.refdef view region).
//
// NAME ADJUDICATION: the .mcode's mechanical pre-hint "PM_ViewHeightTableLerp" is a
// pure size-match guess (win 0x14d vs matched 0x14c) and is REJECTED. This function
// performs no view-height table interpolation and touches no pmove/playerState view
// height: it reads the effect entity's weapon record, computes a screen rectangle
// from the 3D-view rect, and issues the 2D HUD draw traps trap_R_SetColor (id 72) and
// trap_R_DrawStretchPic (id 73). Named by that proven behavior and call graph. The
// exact CoD source symbol is not proven, so the name is a role name (icon-in-3D-view
// draw); it is not the pmove lerp the size-match suggested.
//
// Enable gates (all four must pass, else the function returns having drawn nothing):
//   MOV EAX,[0x3044fa8c]; TEST EAX,EAX; JZ exit    ; cg_drawCrosshair_vmCvar.integer != 0
//   MOV EAX,[0x305300cc]; TEST EAX,EAX; JNZ exit   ; cl_paused_vmCvar.integer == 0
//   MOV EAX,[0x304831c0]; TEST EAX,EAX; JNZ exit   ; view-state flag == 0
// The default color slots are initialized to {1,1,1,0} at entry. On the draw
// path, the FST at 0x30019c27 aliases color[3] after the ESI/EDI pushes and
// replaces that zero with cg_crosshairAlpha_vmCvar.value.
//
// Entity/weapon resolve:
//   MOV EAX,[0x30483780]; IMUL EAX,EAX,0x288; ADD EAX,0x3048c6e0  ; &cg_entities[cg_predictedPlayerState.viewLockedEntityNum]
//   MOV ESI,[EAX+0xcc]                          ; weaponIndex = cent->currentState.weapon; if 0 -> exit
//   MOV EAX,[0x30134cd8]; MOV EDI,[EAX+ESI*4]    ; weap = bg_weaponInfos[weaponIndex]  (weaponInfo_t*)
//   MOV ECX,[EDI+0x108]; CMP byte ptr [ECX],0; JZ exit          ; skip if icon name is empty
//
// Fade gate (x87):
//   FLD [0x30452428]; FST color[3]; FCOMP 0.01f; FNSTSW AX; TEST AH,5; JNP exit
//   -> skip only when cg_crosshairAlpha_vmCvar.value < 0.01f (JNP is taken, skipping, only on the
//      strict less-than result; equal/unordered draws). The intermediate FST to a dead
//      color-alpha store is live and is consumed by trap_R_SetColor.
//
// Color + geometry:
//   trap_R_SetColor(color) with color = {1.0,1.0,1.0,cg_crosshairAlpha}
//   FILD [EDI+0x110]                                            ; iconSize (signed int)
//   width  = iconSize * cgs.screenXScale   (FLD screenX; FMUL ST1; FSTP)
//   height = iconSize * cgs.screenYScale   (FMUL screenY; FSTP)
//   x = cg.refdef.x + (cg.refdef.width  - width )*0.5f + cgs.screenXScale*0.0f
//   y = cg.refdef.y + (cg.refdef.height - height)*0.5f + cgs.screenYScale*0.0f
// The `screen*0.0f` terms are always zero but are emitted by the machine code
// (FLD screenScale; FMUL 0.0f; FADDP) and preserved to keep the x87 stream faithful.
// The refdef rect fields are read as ints (FILD/FIADD from 0x30487a78/7c/80/84).
//   hShader = cg_weaponInfos[weaponIndex].reticleCenterShader ; MOV EAX,[ESI*0x1c4 + 0x30413708]
//   trap_R_DrawStretchPic(x, y, width, height, 0,0,1,1, hShader)   ; CALL 0x3003e0f0
//
// The eight stretch-pic coordinate/texcoord slots are single-precision float bit
// patterns forwarded to trap_R_DrawStretchPic as opaque 32-bit words, so CG_FloatBits
// reproduces the exact FSTP-to-dword forwarding (typing them as float would force a
// double promotion the machine code does not do). Calling convention: no arguments
// (SUB ESP,0x18 frame; plain RET; caller-cleaned 2D-trap pushes).

void CG_DrawWeaponIcon3D(void)
{
    const centity_t *cent;
    const weaponInfo_t *weap;
    int32_t drawCrosshair;
    int32_t entityNum;
    int32_t weaponIndex;
    float fade;
    float color[4];
    float width, height;
    float x, y;

    /* 0x30019ba3 loads/tests the cvar before the four color stores, although
     * the branch itself is not taken until after those stores. */
    drawCrosshair = cg_drawCrosshair_vmCvar.integer;
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    color[3] = 0.0f;

    /* HUD/3D-view draw enable gate. */
    if (drawCrosshair == 0)
        return;
    /* Global draw-inhibit gate. */
    if (cl_paused_vmCvar.integer != 0)
        return;
    /* View-state flag gate (kept mechanical; exact identity unresolved, see globals.h). */
    if (cg_thirdPerson != 0)
        return;

    entityNum = cg_predictedPlayerState.viewLockedEntityNum;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)entityNum >= (uint32_t)MAX_GENTITIES) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "CG_DrawWeaponIcon3D: invalid view-lock entity %i",
                  entityNum);
        return;
    }
    cent = &cg_entities[entityNum];

    /* weaponIndex = cent->currentState.weapon (+0xcc). */
    weaponIndex = cent->currentState.weapon;
    if (weaponIndex == 0)
        return;

    weap = bg_weaponInfos[weaponIndex];

    /* Skip when this weapon's 3D-view icon shader name is empty. */
    if (weap->reticleCenter[0] == '\0')
        return;

    /* 0x30019c21..0x30019c27 loads the fade once and writes it over the
     * entry-time zero in color[3]. Skip only when that value is strictly less
     * than 0.01f. The
     * FCOMP/TEST AH,5/JNP idiom takes the skip branch only on the "less-than"
     * (C0 set, C2 clear) result, so an equal or unordered (NaN) compare draws;
     * `fade < 0.01f` reproduces that exactly (NaN < 0.01f is false -> draws). */
    fade = cg_crosshairAlpha_vmCvar.value;
    color[3] = fade;
    if (fade < 0.01f)
        return;

    trap_R_SetColor(color);

    /* Icon pixel size scaled into real-screen width/height. 0x30019c49 FILD
     * reticleCenterSize; the value stays UNROUNDED in ST0/ST1 and feeds both
     * width=size*sx (FMUL ST1) and height=size*sy (FMUL) with no FSTP, so it is
     * long double -- a float `size` would round it before the two multiplies. */
    long double size = weap->reticleCenterSize;
    width = (float)((long double)cgs_screenXScale * size);
    long double heightRaw = size * (long double)cgs_screenYScale;
    /* 0x30019c67 loads the shader while the unrounded height is still live,
     * before its binary32 store at 0x30019c74. */
    qhandle_t centerShader = cg_weaponInfos[weaponIndex].reticleCenterShader;
    height = (float)heightRaw;

    /* Center the icon in the cropped 3D-view rect. The `scale*0.0f` terms are
     * emitted by the machine code and always contribute zero. cg_refdef.x/y/
     * width/height are FILD'd / FIADD'd (integer, exact) with no FSTP DWORD
     * (0x30019c7c..0x30019cdc), so no (float) casts -- they would round. */
    /* 0x30019c7c..0x30019cb4 computes and stages Y before the corresponding
     * X chain at 0x30019cb8..0x30019cdc. */
    y = (float)(((long double)(int32_t)cg_refdef.height - (long double)height) * 0.5L + (long double)cgs_screenYScale * 0.0f +
                (long double)(int32_t)cg_refdef.y);
    x = (float)(((long double)(int32_t)cg_refdef.width - (long double)width) * 0.5L + (long double)cgs_screenXScale * 0.0f +
                (long double)(int32_t)cg_refdef.x);

    trap_R_DrawStretchPic(CG_FloatBits(x), CG_FloatBits(y), CG_FloatBits(width), CG_FloatBits(height), CG_FloatBits(0.0f),
                          CG_FloatBits(0.0f), CG_FloatBits(1.0f), CG_FloatBits(1.0f), (int32_t)centerShader);
}
