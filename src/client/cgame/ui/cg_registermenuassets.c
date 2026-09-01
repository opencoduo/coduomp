// Source: uo_cgame_mp_x86.dll 0x3002dcf0..0x3002dded
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002dcf0_3002dded.mcode
//
// CG_RegisterMenuAssets — precache the ui_shared.c cachedAssets_t scrollbar,
// slider, and gradient-bar shaders used by the in-game menu/HUD widgets.
//
// The .mcode header's mechanical name "PlayerCmd_takeWeapon" is a pure size
// guess (win size 0xfd == matched size 0xfd) and is REJECTED: the body does no
// weapon/inventory work. It registers a fixed list of "ui/assets/*.tga" shaders
// through the cgame trap vector and stores each returned qhandle_t into the
// shared cachedAssets_t slots (0x30440220..0x3044024c). The registered filenames
// (scrollbar arrows, scrollbar/slider thumbs, gradient bar) are exactly the
// ui_shared.c cachedAssets_t members, which fixes both the function's role name
// and the individual field names.
//
// Machine-code facts proven for every iteration:
//   * PUSH 0; CALL 0x3002a530  -> CG_DrawInformation(qfalse). One int arg,
//     cdecl, caller-cleaned. It is called once BEFORE each shader is registered;
//     its stack slot is folded into the batched
//     ADD ESP cleanups below, confirming cdecl.
//   * PUSH 2; PUSH <name>; PUSH 0x59; CALL [0x30085e9c]  ->
//     cgame_syscall(CG_R_REGISTERSHADER=89, name, 2). Pushed low->high after the
//     id are (name, flags): the syscall wrapper takes the id as the first stack
//     dword; callers of id 0x59 clean 3 dwords (ADD ESP,0xc elsewhere, e.g.
//     0x3001c20f), and the return EAX is a qhandle_t. The trailing flag is 2
//     (a NoMip/2D type flag; 2 and 5 are the observed values across cgame).
//   * MOV [g], EAX stores the just-returned handle into the cachedAssets slot.
//
// Stack cleanup: each iteration pushes 4 dwords (1 for the pump arg + 3 for the
// register syscall) = 0x10 bytes. The function batches: ADD ESP,0x40 after the
// 4th iteration (0x3002dd57), ADD ESP,0x40 after the 8th (0x3002ddc6), and
// ADD ESP,0x10 after the 9th (0x3002dde4) — 4+4+1 = 9 registrations, matching
// the 9 stored handles. Bare RET (no imm), no locals: a plain cdecl void().

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

void CG_RegisterMenuAssets(void)
{
    /* 0x3002dcf0..0x3002dd08: gradient bar */
    CG_DrawInformation(qfalse);
    g_uiDCInstance.gradientBar =
        coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_REGISTERSHADER, cg_uiGradientBarMaterialPath,
            R_IMAGE_TRACK_UI));

    /* 0x3002dd0d..0x3002dd23: scrollbar */
    CG_DrawInformation(qfalse);
    g_uiDCInstance.scrollBar =
        coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_REGISTERSHADER, cg_uiScrollBarMaterialPath,
            R_IMAGE_TRACK_UI));

    /* 0x3002dd28..0x3002dd3e: scrollbar down arrow */
    CG_DrawInformation(qfalse);
    g_uiDCInstance.scrollBarArrowDown =
        coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_REGISTERSHADER, cg_uiScrollDownArrowMaterialPath,
            R_IMAGE_TRACK_UI));

    /* 0x3002dd43..0x3002dd5c: scrollbar up arrow (ADD ESP,0x40 batch cleanup) */
    CG_DrawInformation(qfalse);
    g_uiDCInstance.scrollBarArrowUp =
        coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_REGISTERSHADER, cg_uiScrollUpArrowMaterialPath,
            R_IMAGE_TRACK_UI));

    /* 0x3002dd61..0x3002dd77: scrollbar left arrow */
    CG_DrawInformation(qfalse);
    g_uiDCInstance.scrollBarArrowLeft =
        coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_REGISTERSHADER, cg_uiScrollLeftArrowMaterialPath,
            R_IMAGE_TRACK_UI));

    /* 0x3002dd7c..0x3002dd92: scrollbar right arrow */
    CG_DrawInformation(qfalse);
    g_uiDCInstance.scrollBarArrowRight =
        coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_REGISTERSHADER, cg_uiScrollRightArrowMaterialPath,
            R_IMAGE_TRACK_UI));

    /* 0x3002dd97..0x3002ddad: scrollbar thumb */
    CG_DrawInformation(qfalse);
    g_uiDCInstance.scrollBarThumb =
        coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_REGISTERSHADER, cg_uiScrollThumbMaterialPath,
            R_IMAGE_TRACK_UI));

    /* 0x3002ddb2..0x3002ddcb: slider bar (ADD ESP,0x40 batch cleanup) */
    CG_DrawInformation(qfalse);
    g_uiDCInstance.sliderBar =
        coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_REGISTERSHADER, cg_uiSliderTrackMaterialPath,
            R_IMAGE_TRACK_UI));

    /* 0x3002ddd0..0x3002dde7: slider thumb (ADD ESP,0x10 cleanup, then RET) */
    CG_DrawInformation(qfalse);
    g_uiDCInstance.sliderThumb =
        coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_REGISTERSHADER, cg_uiSliderThumbMaterialPath,
            R_IMAGE_TRACK_UI));
}
