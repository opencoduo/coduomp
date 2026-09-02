#ifndef CLIENT_UI_DISPLAY_CONTEXT_TYPES_H
#define CLIENT_UI_DISPLAY_CONTEXT_TYPES_H

#include "qcommon/q_renderer_types.h"
#include "qcommon/q_shared_types.h"

#include <stddef.h>
#include <stdint.h>

/*
 * The menu display-context record is common to the Windows cgame and UI DLLs
 * from callback slot +0x000 through fps at +0x1e3dc.  The cgame instance at
 * 0x30421f60 ends at 0x30440340, where the next independent cgame global
 * begins.  The UI instance appends six front-end-only state words, so that tail
 * belongs to its local storage wrapper rather than this common record.
 *
 * Callback and field offsets are corroborated by the matching ui_shared
 * consumers in uo_cgame_mp_x86.dll and uo_ui_mp_x86.dll.  At slot +0x6c, the
 * original i386 ABI carries both results in x87 ST0: UI loads the syscall's
 * exact binary32 word, while cgame loads atof's binary64 result and returns
 * without narrowing it.  The shared reconstructed declaration uses an
 * extended portable carrier to preserve both behaviors; it is not evidence
 * that the original field had a long-double function type.  Individual
 * consumers still store to binary32 or binary64 exactly where their machine
 * code does.  The Mac cgame CG_Cvar_Get body ends in FRSP, confirming that its
 * supporting build intentionally rounds this slot differently.
 */

typedef qhandle_t (*ui_registerShaderNoMip_t)(const char *name, int32_t loadMode);
typedef void (*ui_setColor_t)(const vec4_t color);
typedef void (*ui_drawHandlePic_t)(float x, float y, float width, float height, qhandle_t shader);
typedef void (*ui_drawStretchPic_t)(float x, float y, float width, float height, float s0, float t0, float s1, float t1, qhandle_t shader);
typedef void (*ui_drawText_t)(float x, float y, int32_t font, float scale, const vec4_t color, const char *text, float fixedAdvance,
                              int32_t limit, int32_t textStyle);
typedef int32_t (*ui_textWidth_t)(const char *text, int32_t font, float scale, int32_t limit);
typedef int32_t (*ui_textHeight_t)(int32_t font, float scale);
typedef const char *(*ui_translateString_t)(const char *reference);
typedef const char *(*ui_getLocalizedString_t)(const char *reference);
typedef const char *(*ui_localizeWithBinding_t)(const char *message, const char *context);
typedef void (*ui_setFont_t)(int32_t font);
typedef qhandle_t (*ui_registerModel_t)(const char *name, int32_t loadMode);
typedef void (*ui_modelBounds_t)(qhandle_t model, vec3_t minimums, vec3_t maximums);
typedef void (*ui_fillRect_t)(float x, float y, float width, float height, const vec4_t color);
typedef void (*ui_drawRect_t)(float x, float y, float width, float height, float size, const vec4_t color);
typedef void (*ui_drawBorderParts_t)(float x, float y, float width, float height, float size);
typedef void (*ui_clearScene_t)(void);
typedef void (*ui_addRefEntity_t)(const refEntity_t *entity);
typedef void (*ui_renderScene_t)(const refdef_t *refdef);
typedef void (*ui_registerFont_t)(const char *name, int32_t pointSize, fontInfo_t *fontStorage, intptr_t context);
typedef void (*ui_ownerDrawItem_t)(float x, float y, float width, float height, float textX, float textY, int32_t ownerDraw,
                                   int32_t ownerDrawFlags, int32_t alignment, float special, int32_t font, float textScale, vec4_t color,
                                   qhandle_t background, int32_t textStyle);
typedef float (*ui_ownerDrawValue_t)(int32_t ownerDraw, int32_t colorRangeType);
typedef qboolean (*ui_ownerDrawVisible_t)(int32_t ownerDrawFlags);
typedef void (*ui_runScript_t)(char **arguments);
typedef void (*ui_getTeamColor_t)(vec4_t color);
typedef void (*ui_getCVarString_t)(const char *name, char *buffer, int32_t bufferSize);
typedef long double (*ui_getCVarValue_t)(const char *name);
typedef void (*ui_setCVar_t)(const char *name, const char *value);
typedef const char *(*ui_configString_t)(int32_t index);
typedef void (*ui_drawTextWithCursor_t)(float x, float y, int32_t font, float scale, const vec4_t color, const char *text,
                                        int32_t cursorPosition, int8_t cursorCharacter, int32_t limit, int32_t textStyle);
typedef void (*ui_setOverstrikeMode_t)(qboolean overstrike);
typedef qboolean (*ui_getOverstrikeMode_t)(void);
typedef void (*ui_startLocalSound_t)(const char *soundName);
typedef qboolean (*ui_ownerDrawHandleKey_t)(int32_t ownerDraw, int32_t flags, float *special, int32_t key);
typedef int32_t (*ui_feederCount_t)(float feeder);
enum {
    UI_FEEDER_IMAGE_HANDLE_NONE = -1
};
typedef const char *(*ui_feederItemText_t)(float feeder, int32_t index, int32_t column, int32_t *imageHandle);
typedef const char *(*ui_resolveTextToken_t)(const char *token);
typedef qhandle_t (*ui_feederItemImage_t)(float feeder, int32_t index);
typedef void (*ui_feederSelection_t)(float feeder, int32_t index);
typedef void (*ui_feederAddItem_t)(float feeder, const char *name, int32_t value);
typedef void (*ui_getAutoUpdate_t)(void);
typedef qboolean (*ui_runningGame_t)(void);
typedef void (*ui_keynumToStringBuf_t)(int32_t keynum, char *buffer, int32_t bufferSize);
typedef void (*ui_getBindingBuf_t)(int32_t keynum, char *buffer, int32_t bufferSize);
typedef void (*ui_setBinding_t)(int32_t keynum, const char *binding);
typedef void (*ui_executeText_t)(int32_t executionMode, const char *text);
typedef void (*ui_error_t)(errorParm_t level, const char *format, ...);
typedef void (*ui_print_t)(const char *format, ...);
typedef void (*ui_pause_t)(qboolean paused);
typedef int32_t (*ui_ownerDrawWidth_t)(int32_t ownerDraw, int32_t font, float scale);
typedef const char *(*ui_registerAsset_t)(const char *name);
typedef int32_t (*ui_playCinematic_t)(const char *name, float x, float y, float width, float height);
typedef void (*ui_stopCinematic_t)(int32_t cinematic);
typedef void (*ui_drawCinematic_t)(int32_t cinematic, float x, float y, float width, float height);
typedef void (*ui_runCinematicFrame_t)(int32_t cinematic);

typedef struct displayContextDef_s {
    ui_registerShaderNoMip_t registerShaderNoMip;       /* +0x000 */
    ui_setColor_t setColor;                             /* +0x004 */
    ui_drawHandlePic_t drawHandlePic;                   /* +0x008 */
    ui_drawStretchPic_t drawStretchPic;                 /* +0x00c */
    ui_drawText_t drawText;                             /* +0x010 */
    ui_textWidth_t textWidth;                           /* +0x014 */
    ui_textHeight_t textHeight;                         /* +0x018 */
    ui_translateString_t translateString;               /* +0x01c */
    ui_getLocalizedString_t getLocalizedString;         /* +0x020 */
    ui_localizeWithBinding_t localizeWithBinding;       /* +0x024 */
    ui_setFont_t setFont;                               /* +0x028 */
    ui_registerModel_t registerModel;                   /* +0x02c */
    ui_modelBounds_t modelBounds;                       /* +0x030 */
    ui_fillRect_t fillRect;                             /* +0x034 */
    ui_drawRect_t drawRect;                             /* +0x038 */
    ui_drawBorderParts_t drawSides;                     /* +0x03c */
    ui_drawBorderParts_t drawTopBottom;                 /* +0x040 */
    ui_clearScene_t clearScene;                         /* +0x044 */
    ui_addRefEntity_t addRefEntityToScene;              /* +0x048 */
    ui_renderScene_t renderScene;                       /* +0x04c */
    ui_registerFont_t registerFont;                     /* +0x050 */
    ui_ownerDrawItem_t ownerDrawItem;                   /* +0x054 */
    ui_ownerDrawValue_t ownerDrawValue;                 /* +0x058 */
    ui_ownerDrawVisible_t ownerDrawVisible;             /* +0x05c */
    ui_runScript_t runScript;                           /* +0x060 */
    ui_getTeamColor_t getTeamColor;                     /* +0x064 */
    ui_getCVarString_t getCVarString;                   /* +0x068 */
    ui_getCVarValue_t getCVarValue;                     /* +0x06c */
    ui_setCVar_t setCVar;                               /* +0x070 */
    ui_configString_t getConfigString;                  /* +0x074 */
    ui_drawTextWithCursor_t drawTextWithCursor;         /* +0x078 */
    ui_setOverstrikeMode_t setOverstrikeMode;           /* +0x07c */
    ui_getOverstrikeMode_t getOverstrikeMode;           /* +0x080 */
    ui_startLocalSound_t startLocalSound;               /* +0x084 */
    ui_ownerDrawHandleKey_t ownerDrawHandleKey;         /* +0x088 */
    ui_feederCount_t feederCount;                       /* +0x08c */
    ui_feederItemText_t feederItemText;                 /* +0x090 */
    ui_resolveTextToken_t resolveTextToken;             /* +0x094 */
    ui_feederItemImage_t feederItemImage;               /* +0x098 */
    ui_feederSelection_t feederSelection;               /* +0x09c */
    ui_feederAddItem_t feederAddItem;                   /* +0x0a0 */
    ui_getAutoUpdate_t getAutoUpdate;                   /* +0x0a4 */
    ui_runningGame_t runningGame;                       /* +0x0a8 */
    ui_keynumToStringBuf_t keynumToStringBuf;           /* +0x0ac */
    ui_getBindingBuf_t getBindingBuf;                   /* +0x0b0 */
    ui_setBinding_t setBinding;                         /* +0x0b4 */
    ui_executeText_t executeText;                       /* +0x0b8 */
    ui_error_t error;                                   /* +0x0bc */
    ui_print_t print;                                   /* +0x0c0 */
    ui_pause_t pause;                                   /* +0x0c4 */
    ui_ownerDrawWidth_t ownerDrawWidth;                 /* +0x0c8 */
    ui_registerAsset_t registerAsset;                   /* +0x0cc */
    ui_playCinematic_t playCinematic;                   /* +0x0d0 */
    ui_stopCinematic_t stopCinematic;                   /* +0x0d4 */
    ui_drawCinematic_t drawCinematic;                   /* +0x0d8 */
    ui_runCinematicFrame_t runCinematicFrame;           /* +0x0dc */
    float yscale;                                       /* +0x0e0 */
    float xscale;                                       /* +0x0e4 */
    float bias;                                         /* +0x0e8 */
    int32_t realTime;                                   /* +0x0ec */
    int32_t frameTime;                                  /* +0x0f0 */
    int32_t cursorx;                                    /* +0x0f4 */
    int32_t cursory;                                    /* +0x0f8 */
    int32_t debug;                                      /* +0x0fc */
    /* Q3 cachedAssets_t::fontStr; retained by CoD although not consumed here. */
    const char *fontStr;                                /* +0x100 */
    const char *cursorName;                             /* +0x104 */
    const char *gradientStr;                            /* +0x108 */
    fontInfo_t textFont;                                /* +0x10c */
    fontInfo_t smallFont;                               /* +0x5154 */
    fontInfo_t bigFont;                                 /* +0xa19c */
    fontInfo_t extraBigFont;                            /* +0xf1e4 */
    fontInfo_t boldFont;                                /* +0x1422c */
    fontInfo_t consoleFont;                             /* +0x19274 */
    qhandle_t cursor;                                   /* +0x1e2bc */
    qhandle_t gradientBar;                              /* +0x1e2c0 */
    qhandle_t scrollBarArrowUp;                         /* +0x1e2c4 */
    qhandle_t scrollBarArrowDown;                       /* +0x1e2c8 */
    qhandle_t scrollBarArrowLeft;                       /* +0x1e2cc */
    qhandle_t scrollBarArrowRight;                      /* +0x1e2d0 */
    qhandle_t scrollBar;                                /* +0x1e2d4 */
    qhandle_t scrollBarThumb;                           /* +0x1e2d8 */
    qhandle_t buttonMiddle;                             /* +0x1e2dc */
    qhandle_t buttonInside;                             /* +0x1e2e0 */
    qhandle_t solidBox;                                 /* +0x1e2e4 */
    qhandle_t sliderBar;                                /* +0x1e2e8 */
    qhandle_t sliderThumb;                              /* +0x1e2ec */
    const char *menuEnterSound;                         /* +0x1e2f0 */
    const char *menuExitSound;                          /* +0x1e2f4 */
    const char *menuBuzzSound;                          /* +0x1e2f8 */
    const char *itemFocusSound;                         /* +0x1e2fc */
    float menuFadeClamp;                                /* +0x1e300 */
    int32_t menuFadeCycle;                              /* +0x1e304 */
    float menuFadeAmountOut;                            /* +0x1e308 */
    float menuFadeAmountIn;                             /* +0x1e30c */
    float shadowX;                                      /* +0x1e310 */
    float shadowY;                                      /* +0x1e314 */
    vec4_t shadowColor;                                 /* +0x1e318 */
    float shadowFadeClamp;                              /* +0x1e328 */
    int32_t textFontRegistered;                         /* +0x1e32c */
    glconfig_t glConfig;                                /* +0x1e330 */
    qhandle_t whiteShader;                              /* +0x1e3d0 */
    qhandle_t gradientImage;                            /* +0x1e3d4 */
    /* Q3 displayContextDef_t::cursor, distinct from cachedAssets_t::cursor. */
    qhandle_t displayCursor;                            /* +0x1e3d8 */
    float fps;                                          /* +0x1e3dc */
} displayContextDef_t;

#if UINTPTR_MAX == UINT32_MAX
#define UI_DISPLAY_CONTEXT_OFFSET_ASSERT(field, expected) \
    _Static_assert(offsetof(displayContextDef_t, field) == (expected), "displayContextDef_t." #field " moved")

UI_DISPLAY_CONTEXT_OFFSET_ASSERT(registerShaderNoMip, 0x000);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(setColor, 0x004);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(drawHandlePic, 0x008);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(drawStretchPic, 0x00c);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(drawText, 0x010);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(textWidth, 0x014);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(textHeight, 0x018);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(translateString, 0x01c);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(getLocalizedString, 0x020);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(localizeWithBinding, 0x024);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(setFont, 0x028);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(registerModel, 0x02c);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(modelBounds, 0x030);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(fillRect, 0x034);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(drawRect, 0x038);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(drawSides, 0x03c);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(drawTopBottom, 0x040);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(clearScene, 0x044);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(addRefEntityToScene, 0x048);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(renderScene, 0x04c);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(registerFont, 0x050);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(ownerDrawItem, 0x054);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(ownerDrawValue, 0x058);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(ownerDrawVisible, 0x05c);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(runScript, 0x060);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(getTeamColor, 0x064);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(getCVarString, 0x068);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(getCVarValue, 0x06c);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(setCVar, 0x070);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(getConfigString, 0x074);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(drawTextWithCursor, 0x078);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(setOverstrikeMode, 0x07c);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(getOverstrikeMode, 0x080);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(startLocalSound, 0x084);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(ownerDrawHandleKey, 0x088);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(feederCount, 0x08c);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(feederItemText, 0x090);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(resolveTextToken, 0x094);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(feederItemImage, 0x098);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(feederSelection, 0x09c);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(feederAddItem, 0x0a0);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(getAutoUpdate, 0x0a4);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(runningGame, 0x0a8);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(keynumToStringBuf, 0x0ac);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(getBindingBuf, 0x0b0);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(setBinding, 0x0b4);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(executeText, 0x0b8);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(error, 0x0bc);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(print, 0x0c0);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(pause, 0x0c4);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(ownerDrawWidth, 0x0c8);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(registerAsset, 0x0cc);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(playCinematic, 0x0d0);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(stopCinematic, 0x0d4);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(drawCinematic, 0x0d8);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(runCinematicFrame, 0x0dc);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(yscale, 0x0e0);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(xscale, 0x0e4);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(bias, 0x0e8);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(realTime, 0x0ec);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(frameTime, 0x0f0);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(cursorx, 0x0f4);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(cursory, 0x0f8);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(debug, 0x0fc);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(fontStr, 0x100);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(cursorName, 0x104);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(gradientStr, 0x108);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(textFont, 0x10c);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(smallFont, 0x5154);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(bigFont, 0xa19c);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(extraBigFont, 0xf1e4);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(boldFont, 0x1422c);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(consoleFont, 0x19274);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(cursor, 0x1e2bc);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(gradientBar, 0x1e2c0);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(scrollBarArrowUp, 0x1e2c4);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(scrollBarArrowDown, 0x1e2c8);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(scrollBarArrowLeft, 0x1e2cc);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(scrollBarArrowRight, 0x1e2d0);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(scrollBar, 0x1e2d4);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(scrollBarThumb, 0x1e2d8);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(buttonMiddle, 0x1e2dc);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(buttonInside, 0x1e2e0);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(solidBox, 0x1e2e4);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(sliderBar, 0x1e2e8);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(sliderThumb, 0x1e2ec);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(menuEnterSound, 0x1e2f0);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(menuExitSound, 0x1e2f4);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(menuBuzzSound, 0x1e2f8);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(itemFocusSound, 0x1e2fc);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(menuFadeClamp, 0x1e300);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(menuFadeCycle, 0x1e304);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(menuFadeAmountOut, 0x1e308);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(menuFadeAmountIn, 0x1e30c);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(shadowX, 0x1e310);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(shadowY, 0x1e314);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(shadowColor, 0x1e318);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(shadowFadeClamp, 0x1e328);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(textFontRegistered, 0x1e32c);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(glConfig, 0x1e330);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(whiteShader, 0x1e3d0);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(gradientImage, 0x1e3d4);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(displayCursor, 0x1e3d8);
UI_DISPLAY_CONTEXT_OFFSET_ASSERT(fps, 0x1e3dc);

_Static_assert(sizeof(displayContextDef_t) == 0x1e3e0, "original common display-context size is 123872 bytes");

#undef UI_DISPLAY_CONTEXT_OFFSET_ASSERT
#endif

#endif
