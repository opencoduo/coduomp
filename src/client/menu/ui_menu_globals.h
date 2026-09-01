#ifndef CLIENT_UI_MENU_GLOBALS_H
#define CLIENT_UI_MENU_GLOBALS_H

#include "ui_menu_types.h"

#include <stdint.h>

/*
 * Each client module owns a separate instance of the original ui_shared.c menu
 * registry and open-menu stack.  The authoritative PE32 storage addresses are:
 *
 *                    cgame       UI
 * menuCount          0x30134d40  0x401c46d8
 * openMenuCount      0x30134d44  0x401c46dc
 * Menus              0x30136940  0x401c62e0
 * menuStack          0x30169880  0x401f9220
 *
 * Both registries contain MAX_MENUS menuDef_t records and both stacks contain
 * MAX_OPEN_MENUS menuDef_t pointers.  The former ui_* and Menus_openMenus
 * spellings were reconstruction-local names, not distinct interfaces.
 */
extern int32_t menuCount;
extern int32_t openMenuCount;
extern menuDef_t Menus[MAX_MENUS];
extern menuDef_t *menuStack[MAX_OPEN_MENUS];

/*
 * Each client module also owns one copy of the original held-control capture
 * state.  The Windows cgame/UI functions that read and write these objects are
 * instruction twins after rebasing their data references:
 *
 *                    cgame       UI
 * captureFunc        0x30134d20  0x401c46b8
 * captureData        0x30134d24  0x401c46bc
 * captureItem        0x30134d28  0x401c46c0
 * ui_scrollInfo      0x30133c28  0x401c35f8
 */
extern ui_captureFunc_t captureFunc;
extern void *captureData;
extern itemDef_t *captureItem;
extern scrollInfo_t ui_scrollInfo;

/* Shared key/edit/debug state used by the identical menu input and paint
 * runtime.  The nonmatching ui_* spellings in the UI reconstruction were local
 * recovery names; cgame's names follow the retained ui_shared interface.
 *
 *                         cgame       UI
 * g_waitingForKey         0x30134d30  0x401c46c8
 * g_editingField          0x30134d34  0x401c46cc
 * g_bindItem              0x30134d38  0x401c46d0
 * g_editItem              0x30134d3c  0x401c46d4
 * debugMode               0x30134d48  0x401c46e0
 * lastListBoxClickTime    0x30134d4c  0x401c46e4
 * inHandleKey             0x30134d58  0x401c46f0
 */
extern int32_t g_waitingForKey;
extern int32_t g_editingField;
extern itemDef_t *g_bindItem;
extern itemDef_t *g_editItem;
extern int32_t debugMode;
extern int32_t lastListBoxClickTime;
extern int32_t inHandleKey;

#endif
