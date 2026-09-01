#include "../module/ui_functions.h"

#include <string.h>

enum { ASSET_TOKEN_COMPARE_LIMIT = 99999 };

// Source: uo_ui_mp_x86.dll 0x40008ce0..0x40009292
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40008ce0_40009292.mcode
// Role name: Asset_Parse; UI_ParseMenu dispatches its assetGlobalDef block here.
qboolean Asset_Parse(int32_t sourceHandle, int32_t loadMode)
{
    pc_token_t token;
    const char *name;
    int32_t pointSize;

    if (!trap_PC_ReadToken(sourceHandle, &token) ||
        Q_stricmpn("{", token.string, ASSET_TOKEN_COMPARE_LIMIT) != 0) {
        return qfalse;
    }

    for (;;) {
        memset(&token, 0, sizeof(token));
        if (!trap_PC_ReadToken(sourceHandle, &token)) {
            return qfalse;
        }
        if (Q_stricmpn("}", token.string, ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            return qtrue;
        }

        if (Q_stricmpn("font", token.string, ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_String_Parse(sourceHandle, &name) ||
                !PC_Int_Parse(sourceHandle, &pointSize)) {
                return qfalse;
            }
            trap_R_RegisterFont(name, pointSize, &DC->textFont,
                                (intptr_t)loadMode);
            DC->textFontRegistered = qtrue;
        } else if (Q_stricmpn("smallFont", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_String_Parse(sourceHandle, &name) ||
                !PC_Int_Parse(sourceHandle, &pointSize)) {
                return qfalse;
            }
            trap_R_RegisterFont(name, pointSize, &DC->smallFont,
                                (intptr_t)loadMode);
        } else if (Q_stricmpn("bigFont", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_String_Parse(sourceHandle, &name) ||
                !PC_Int_Parse(sourceHandle, &pointSize)) {
                return qfalse;
            }
            trap_R_RegisterFont(name, pointSize, &DC->bigFont,
                                (intptr_t)loadMode);
        } else if (Q_stricmpn("extraBigFont", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_String_Parse(sourceHandle, &name) ||
                !PC_Int_Parse(sourceHandle, &pointSize)) {
                return qfalse;
            }
            trap_R_RegisterFont(name, pointSize,
                                &DC->extraBigFont,
                                (intptr_t)loadMode);
        } else if (Q_stricmpn("boldFont", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_String_Parse(sourceHandle, &name) ||
                !PC_Int_Parse(sourceHandle, &pointSize)) {
                return qfalse;
            }
            trap_R_RegisterFont(name, pointSize, &DC->boldFont,
                                (intptr_t)loadMode);
        } else if (Q_stricmpn("consoleFont", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_String_Parse(sourceHandle, &name) ||
                !PC_Int_Parse(sourceHandle, &pointSize)) {
                return qfalse;
            }
            trap_R_RegisterFont(name, pointSize,
                                &DC->consoleFont,
                                (intptr_t)loadMode);
        } else if (Q_stricmpn("gradientbar", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_String_Parse(sourceHandle, &name)) return qfalse;
            DC->gradientBar =
                trap_R_RegisterShaderNoMip(name, loadMode);
        } else if (Q_stricmpn("menuEnterSound", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_String_Parse(sourceHandle, &name)) return qfalse;
            DC->menuEnterSound = trap_S_RegisterSound(name);
        } else if (Q_stricmpn("menuExitSound", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_String_Parse(sourceHandle, &name)) return qfalse;
            DC->menuExitSound = trap_S_RegisterSound(name);
        } else if (Q_stricmpn("itemFocusSound", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_String_Parse(sourceHandle, &name)) return qfalse;
            DC->itemFocusSound = trap_S_RegisterSound(name);
        } else if (Q_stricmpn("menuBuzzSound", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_String_Parse(sourceHandle, &name)) return qfalse;
            DC->menuBuzzSound = trap_S_RegisterSound(name);
        } else if (Q_stricmpn("cursor", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_String_Parse(sourceHandle,
                                 &DC->cursorName)) {
                return qfalse;
            }
            DC->cursor =
                trap_R_RegisterShaderNoMip(DC->cursorName,
                                           loadMode);
        } else if (Q_stricmpn("fadeClamp", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_Float_Parse(sourceHandle,
                                &DC->menuFadeClamp)) {
                return qfalse;
            }
        } else if (Q_stricmpn("fadeCycle", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_Int_Parse(sourceHandle,
                              &DC->menuFadeCycle)) {
                return qfalse;
            }
        } else if (Q_stricmpn("fadeAmount", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_Float_Parse(sourceHandle,
                                &DC->menuFadeAmountOut)) {
                return qfalse;
            }
        } else if (Q_stricmpn("fadeInAmount", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_Float_Parse(sourceHandle,
                                &DC->menuFadeAmountIn)) {
                return qfalse;
            }
        } else if (Q_stricmpn("shadowX", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_Float_Parse(sourceHandle, &DC->shadowX)) {
                return qfalse;
            }
        } else if (Q_stricmpn("shadowY", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_Float_Parse(sourceHandle, &DC->shadowY)) {
                return qfalse;
            }
        } else if (Q_stricmpn("shadowColor", token.string,
                              ASSET_TOKEN_COMPARE_LIMIT) == 0) {
            if (!PC_Color_Parse(sourceHandle,
                                DC->shadowColor)) {
                return qfalse;
            }
            DC->shadowFadeClamp =
                DC->shadowColor[3];
        }
    }
}


// MARK_RECONSTRUCTED
