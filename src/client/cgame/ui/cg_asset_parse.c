// Source: uo_cgame_mp_x86.dll 0x3002cb40..0x3002d106
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002cb40_3002d106.mcode
//
// CG_Asset_Parse - parse an assetGlobalDef block used by the HUD/menu system.
// The source handle arrives in ECX and the menu asset loadMode is the sole
// stack argument in the original i386 DLL; represented here as ordinary C args.
// Name corroborated by the same-module PPC symbol bank. The mechanical
// G_ParseScrVehicleInfo header guess is rejected: every operation is a PC-parser
// read or a UI font/shader/sound/fade asset assignment.

#include "client/cgame/client_recovered.h"

enum {
    UI_STRING_COMPARE_LIMIT = 99999
};

#define ASSET_TOKEN_IS(text_) (Q_stricmpn((text_), token.string, UI_STRING_COMPARE_LIMIT) == 0)
#define READ_ASSET_TOKEN() trap_PC_ReadToken(sourceHandle, &token)

qboolean CG_Asset_Parse(int32_t sourceHandle, int32_t loadMode)
{
    pc_token_t token;
    const char *name;
    int pointSize;

    if (!READ_ASSET_TOKEN() || !ASSET_TOKEN_IS("{")) {
        return qfalse;
    }

    while (READ_ASSET_TOKEN()) {
        if (ASSET_TOKEN_IS("}")) {
            return qtrue;
        }

        if (ASSET_TOKEN_IS("font")) {
            if (!PC_String_Parse(sourceHandle, &name) || !PC_Int_Parse(sourceHandle, &pointSize)) {
                return qfalse;
            }
            g_uiDCInstance.registerFont(name, pointSize, &g_uiDCInstance.textFont, loadMode);
        } else if (ASSET_TOKEN_IS("smallFont")) {
            if (!PC_String_Parse(sourceHandle, &name) || !PC_Int_Parse(sourceHandle, &pointSize)) {
                return qfalse;
            }
            g_uiDCInstance.registerFont(name, pointSize, &g_uiDCInstance.smallFont, loadMode);
        } else if (ASSET_TOKEN_IS("bigfont")) {
            if (!PC_String_Parse(sourceHandle, &name) || !PC_Int_Parse(sourceHandle, &pointSize)) {
                return qfalse;
            }
            g_uiDCInstance.registerFont(name, pointSize, &g_uiDCInstance.bigFont, loadMode);
        } else if (ASSET_TOKEN_IS("extrabigfont")) {
            if (!PC_String_Parse(sourceHandle, &name) || !PC_Int_Parse(sourceHandle, &pointSize)) {
                return qfalse;
            }
            g_uiDCInstance.registerFont(name, pointSize, &g_uiDCInstance.extraBigFont, loadMode);
        } else if (ASSET_TOKEN_IS("boldFont")) {
            if (!PC_String_Parse(sourceHandle, &name) || !PC_Int_Parse(sourceHandle, &pointSize)) {
                return qfalse;
            }
            g_uiDCInstance.registerFont(name, pointSize, &g_uiDCInstance.boldFont, loadMode);
        } else if (ASSET_TOKEN_IS("consoleFont")) {
            if (!PC_String_Parse(sourceHandle, &name) || !PC_Int_Parse(sourceHandle, &pointSize)) {
                return qfalse;
            }
            g_uiDCInstance.registerFont(name, pointSize, &g_uiDCInstance.consoleFont, loadMode);
        } else if (ASSET_TOKEN_IS("gradientbar")) {
            if (!PC_String_Parse(sourceHandle, &name)) {
                return qfalse;
            }
            g_uiDCInstance.gradientBar = trap_R_RegisterShaderNoMip(name, loadMode);
        } else if (ASSET_TOKEN_IS("menuEnterSound")) {
            if (!PC_String_Parse(sourceHandle, &name)) {
                return qfalse;
            }
            g_uiDCInstance.menuEnterSound = trap_Com_SoundAliasString(name);
        } else if (ASSET_TOKEN_IS("menuExitSound")) {
            if (!PC_String_Parse(sourceHandle, &name)) {
                return qfalse;
            }
            g_uiDCInstance.menuExitSound = trap_Com_SoundAliasString(name);
        } else if (ASSET_TOKEN_IS("itemFocusSound")) {
            if (!PC_String_Parse(sourceHandle, &name)) {
                return qfalse;
            }
            g_uiDCInstance.itemFocusSound = trap_Com_SoundAliasString(name);
        } else if (ASSET_TOKEN_IS("menuBuzzSound")) {
            if (!PC_String_Parse(sourceHandle, &name)) {
                return qfalse;
            }
            g_uiDCInstance.menuBuzzSound = trap_Com_SoundAliasString(name);
        } else if (ASSET_TOKEN_IS("cursor")) {
            if (!PC_String_Parse(sourceHandle, &g_uiDCInstance.cursorName)) {
                return qfalse;
            }
            g_uiDCInstance.cursor = trap_R_RegisterShaderNoMip(g_uiDCInstance.cursorName, loadMode);
        } else if (ASSET_TOKEN_IS("fadeClamp")) {
            if (!PC_Float_Parse(sourceHandle, &g_uiDCInstance.menuFadeClamp)) {
                return qfalse;
            }
        } else if (ASSET_TOKEN_IS("fadeCycle")) {
            if (!PC_Int_Parse(sourceHandle, &g_uiDCInstance.menuFadeCycle)) {
                return qfalse;
            }
        } else if (ASSET_TOKEN_IS("fadeAmount")) {
            if (!PC_Float_Parse(sourceHandle, &g_uiDCInstance.menuFadeAmountOut)) {
                return qfalse;
            }
        } else if (ASSET_TOKEN_IS("fadeInAmount")) {
            if (!PC_Float_Parse(sourceHandle, &g_uiDCInstance.menuFadeAmountIn)) {
                return qfalse;
            }
        } else if (ASSET_TOKEN_IS("shadowX")) {
            if (!PC_Float_Parse(sourceHandle, &g_uiDCInstance.shadowX)) {
                return qfalse;
            }
        } else if (ASSET_TOKEN_IS("shadowY")) {
            if (!PC_Float_Parse(sourceHandle, &g_uiDCInstance.shadowY)) {
                return qfalse;
            }
        } else if (ASSET_TOKEN_IS("shadowColor")) {
            if (!PC_Color_Parse(sourceHandle, g_uiDCInstance.shadowColor)) {
                return qfalse;
            }
            g_uiDCInstance.shadowFadeClamp = g_uiDCInstance.shadowColor[3];
        }
    }

    return qfalse;
}

#undef READ_ASSET_TOKEN
#undef ASSET_TOKEN_IS
