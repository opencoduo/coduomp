// Sources: uo_cgame_mp_x86.dll 0x30054aa0..0x30054ab2 and
//          uo_ui_mp_x86.dll    0x40016600..0x40016612
//
// Item_Action — run a menu item's activation script. Given an itemDef_t* in EAX,
// if the item is non-NULL it fetches the item's action script (item->action,
// +0x108) and hands it to Item_RunScript (0x300526e0), which tokenizes the
// string and dispatches each command against the global menu-script command
// table. A NULL item is a no-op. This is the ui_shared.c routine invoked when a
// menu item is activated (mouse click / key press on the item's rect).
//
// Name adjudication: the .mcode header's "# name G_SetClientSound" is a pure
// size-only corpus match (win size 0x12 vs matched 0x11) with zero behavioral
// basis and is REJECTED. The body proves a UI-item script dispatch, not a client
// sound: it calls the already-reconstructed Item_RunScript (0x300526e0, the
// ui_shared.c menu-script runner) with the item's +0x108 script-string field.
// This is exactly Q3 ui_shared.c's Item_Action(itemDef_t*): { if (item)
// Item_RunScript(item, item->action); }.
//
// Caller evidence (0x300553f2): the sole caller is a menu key/mouse handler that
// loads EAX with the item that was just hit-tested (Rect_ContainsPoint, via the
// 0x30051130 call gate at 0x300553e0/0x300553e8) and then CALLs 0x30054aa0. The
// item is activated on a rect hit — the "action" script role of +0x108.
//
// Register ABI (proven from the body and the caller):
//   - item : passed in EAX (0x30054aa0 TEST EAX; 0x30054aab PUSH EAX pushes it as
//            Item_RunScript's single stack argument).
//   - Item_RunScript's second argument (the script string) is passed in EDX,
//     loaded at 0x30054aa4 (MOV EDX,[EAX+0x108]) — item->action — matching
//     Item_RunScript's documented custom ABI (context on the stack, script in EDX).
//
// Machine-code walkthrough:
//   30054aa0 TEST EAX,EAX             item == NULL ?
//   30054aa2 JZ   0x30054ab1          -> return, no-op
//   30054aa4 MOV  EDX,[EAX+0x108]     EDX = item->action (Item_RunScript's script)
//   30054aaa PUSH EAX                 push item as Item_RunScript's stack argument
//   30054aab CALL 0x300526e0          Item_RunScript(item, item->action)
//   30054ab0 POP  ECX                 discard the pushed item (caller-cleaned)
//   30054ab1 RET

#include "ui_runtime.h"

#include <stddef.h>

void Item_Action(itemDef_t *item)
{
    /* 0x30054aa0/0x30054aa2: a NULL item does nothing. */
    if (item == NULL) {
        return;
    }

    /* 0x30054aa4..0x30054ab0: run the item's activation script. item->action
     * (+0x108) is loaded into EDX as Item_RunScript's script argument, and item
     * is pushed as its context argument. Item_RunScript itself no-ops on a NULL
     * or empty script string. */
    Item_RunScript(item, item->action);
}
