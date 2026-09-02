// Source: uo_cgame_mp_x86.dll 0x300320e0..0x30032648
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300320e0_30032648.mcode
//
// CG_OwnerDraw -- display-context owner-draw dispatcher.  Identity is proven by
// installation into displayContextDef_t::ownerDrawItem at 0x3002db77, the exact
// 15-argument ui_ownerDrawItem_t ABI, and the same-module Mac CG_OwnerDraw symbol.
// The compiler's two-level switch table maps owner-draw ids 4..106 onto the HUD
// painters below. Individual painters repurpose some generic owner-draw scalar
// slots, while the color and local rect retain their proved pointer identities.
// Identifiers without the CG_OD_ recovery prefix retain their exact retail UO
// ui_mp/menudef.h spellings. Corresponding painter names are taken from the
// same-module macOS owner-draw jump table where available.

#include "client/cgame/client_recovered.h"

enum cgOwnerDrawId {
    CG_OD_HUD_DIGITS = 4,
    CG_PLAYER_AMMO_VALUE = 5,
    CG_PLAYER_AMMO_BACKDROP = 6,
    CG_SELECTEDPLAYER_NAME = 8,
    CG_SELECTEDPLAYER_LOCATION = 9,
    CG_PLAYER_STANCE = 20,
    CG_BLUE_SCORE = 27,
    CG_RED_SCORE = 28,
    CG_PLAYER_LOCATION = 33,
    CG_OD_TEAM_BACKGROUND = 34,
    CG_OD_GAMETYPE = 39,
    CG_OD_ICON_OR_VALUE = 41,
    CG_AREA_SYSTEMCHAT = 46,
    CG_AREA_TEAMCHAT = 47,
    CG_AREA_CHAT = 48,
    CG_OD_OBITUARY = 50,
    CG_VOICE_NAME = 63,
    CG_1STPLACE = 67,
    CG_2NDPLACE = 68,
    /* Exact retail name. The Windows and macOS dispatch tables deliberately
     * route this id and CG_PLAYER_AMMO_VALUE to the same painter. */
    CG_PLAYER_AMMOCLIP_VALUE = 70,
    CG_OD_USABLE_HINT = 72,
    CG_OD_SELECTED_WEAPON_NAME = 81,
    CG_OD_SELECTED_WEAPON_BACKGROUND = 82,
    CG_PLAYER_WEAPON_MODE_ICON = 83,
    CG_OD_SPINNING_PIC = 84,
    CG_OD_SLIDE_PIC = 85,
    CG_OD_OBJECTIVE_POINTERS = 86,
    CG_OD_DRAW_16570 = 88,
    CG_OD_DRAW_16C00 = 89,
    CG_OD_STAT_BAR = 90,
    CG_PLAYER_BAR_HEALTH_TITLE = 91,
    CG_OD_STRETCH_PIC = 92,
    CG_OD_ROTATED_TAG = 93,
    CG_OD_TURRET_TAG = 94,
    CG_OD_BONE_TAG = 95,
    CG_OD_SLIDE_TAG = 100,
    CG_PLAYER_WEAPON_MODE_ICON_VEHICLE = 103,
    CG_PLAYER_AMMO_VALUE_VEHICLE = 105,
    CG_PLAYER_AMMO_BACKDROP_VEHICLE = 106
};

void CG_OwnerDraw(float x, float y, float w, float h, float textX, float textY, int32_t ownerDraw, int32_t ownerDrawFlags,
                  int32_t alignment, float special, int32_t font, float textScale, vec4_t color, int32_t background, int32_t textStyle)
{
    (void)textX;
    (void)textY;
    (void)ownerDrawFlags;
    (void)alignment;
    (void)special;

    if (cg_drawStatus_vmCvar.integer == 0) {
        return;
    }

    /* 0x300320f0..0x30032115 copies the four rect dwords only after the
     * cg_drawStatus gate, then decodes ownerDraw. */
    rectDef_t rect = {x, y, w, h};
    int32_t scaleBits = CG_FloatBits(textScale);

    switch (ownerDraw) {
    case CG_OD_HUD_DIGITS:
        CG_HudEmitDigits(background, color, &rect, textScale);
        return;
    case CG_PLAYER_AMMO_VALUE:
    case CG_PLAYER_AMMOCLIP_VALUE:
        CG_DrawPlayerAmmoValue(0, &rect, font, scaleBits, color, textStyle);
        return;
    case CG_PLAYER_AMMO_VALUE_VEHICLE:
        CG_DrawPlayerAmmoValue(1, &rect, font, scaleBits, color, textStyle);
        return;
    case CG_PLAYER_AMMO_BACKDROP:
        CG_DrawPlayerAmmoBackdrop(0, &rect, color, background);
        return;
    case CG_PLAYER_AMMO_BACKDROP_VEHICLE:
        CG_DrawPlayerAmmoBackdrop(1, &rect, color, background);
        return;
    case CG_SELECTEDPLAYER_NAME:
    case CG_VOICE_NAME:
        CG_DrawSelectedPlayerName(&rect, font, scaleBits, (intptr_t)color, textStyle);
        return;
    case CG_SELECTEDPLAYER_LOCATION:
        CG_DrawSelectedPlayerLocation(&rect, font, scaleBits, (intptr_t)color, textStyle);
        return;
    case CG_PLAYER_STANCE:
        CG_DrawPlayerStance(&rect, color, font, scaleBits, textStyle);
        return;
    case CG_BLUE_SCORE:
        CG_DrawBlueScore(&rect, font, scaleBits, (intptr_t)color, textStyle);
        return;
    case CG_RED_SCORE:
        CG_DrawRedScore(&rect, font, scaleBits, (intptr_t)color, textStyle);
        return;
    case CG_PLAYER_LOCATION:
        CG_DrawPlayerLocation(&rect, font, scaleBits, (intptr_t)color, textStyle);
        return;
    case CG_OD_TEAM_BACKGROUND:
        CG_EmitLocalTeamBackground(&rect, color);
        return;
    case CG_OD_GAMETYPE:
        CG_DrawGameType(&rect, font, scaleBits, (intptr_t)color, textStyle);
        return;
    case CG_OD_ICON_OR_VALUE:
        CG_HudEmitIconOrValue(background, &rect, font, scaleBits, color, textStyle);
        return;
    case CG_AREA_SYSTEMCHAT:
        CG_DrawAreaSystemChat(&rect, font, scaleBits, (intptr_t)color);
        return;
    case CG_AREA_TEAMCHAT:
        CG_DrawAreaTeamChat(&rect, font, scaleBits, (intptr_t)color);
        return;
    case CG_AREA_CHAT:
        CG_DrawAreaChat(&rect, font, scaleBits, (intptr_t)color);
        return;
    case CG_OD_OBITUARY:
        CG_DrawObituaryLine(&rect, font, scaleBits, (intptr_t)color, textStyle);
        return;
    case CG_1STPLACE:
        CG_Draw1stPlace(&rect, font, scaleBits, (intptr_t)color, textStyle);
        return;
    case CG_2NDPLACE:
        CG_Draw2ndPlace(&rect, font, scaleBits, (intptr_t)color, textStyle);
        return;
    case CG_OD_USABLE_HINT:
        CG_DrawCursorhint(&rect, font, scaleBits, color, textStyle);
        return;
    case CG_OD_SELECTED_WEAPON_NAME:
        /* 0x30032417 leaves color in EAX and 0x30032421 forms &rect in
         * EDI for CG_DrawPlayerWeaponName; their equal 16-byte extents do not
         * make those register arguments interchangeable. */
        CG_DrawPlayerWeaponName(color, &rect, font, scaleBits, textStyle);
        return;
    case CG_OD_SELECTED_WEAPON_BACKGROUND:
        /* 0x30032441 reloads color into EAX and 0x30032447 forms &rect in
         * ESI for CG_DrawPlayerWeaponNameBack. */
        CG_DrawPlayerWeaponNameBack(color, &rect, font, scaleBits, background);
        return;
    case CG_PLAYER_WEAPON_MODE_ICON:
        CG_DrawPlayerWeaponModeIcon(0, &rect, color);
        return;
    case CG_PLAYER_WEAPON_MODE_ICON_VEHICLE:
        CG_DrawPlayerWeaponModeIcon(1, &rect, color);
        return;
    case CG_OD_SPINNING_PIC:
        CG_DrawSpinningPic(&rect, background, color);
        return;
    case CG_OD_SLIDE_PIC:
        CG_DrawHudSlidePicColor(&rect, background, color);
        return;
    case CG_OD_OBJECTIVE_POINTERS:
        CG_DrawObjectivePointers(&rect, color);
        return;
    case CG_OD_STAT_BAR:
        CG_DrawStatBarWithDecay(color, &rect, background);
        return;
    case CG_PLAYER_BAR_HEALTH_TITLE:
        CG_DrawPlayerBarHealthTitle(&rect, font, scaleBits, (intptr_t)color, textStyle);
        return;
    case CG_OD_STRETCH_PIC:
        CG_DrawStretchPicColor(&rect, background, color);
        return;
    case CG_OD_ROTATED_TAG:
        CG_DrawTankBody(&rect, font, background, color);
        return;
    case CG_OD_TURRET_TAG:
        /* 0x300325ed mov ecx,[esp+0x4c] = font (arg11), the same slot the sibling
         * ROTATED_TAG/BONE_TAG cases pass -- not specialBits (arg10). */
        CG_DrawTankBarrel(&rect, font, background, color);
        return;
    case CG_OD_BONE_TAG:
        CG_DrawTankPositionStatus(&rect, font, background, color, textStyle);
        return;
    case CG_OD_SLIDE_TAG:
        CG_DrawJeepBody(&rect, background, color);
        return;
    case CG_OD_DRAW_16570:
        CG_DrawCompassFriendlies(&rect, color);
        return;
    case CG_OD_DRAW_16C00:
        CG_DrawCompassTanks(&rect, color);
        return;
    default:
        return;
    }
}
