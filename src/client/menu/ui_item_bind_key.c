// Sources: uo_cgame_mp_x86.dll 0x30057020..0x30057210 and
//          uo_ui_mp_x86.dll    0x40018b80..0x40018d70.
// Their normalized instruction streams are identical. The target adapters
// below do not alter that original algorithm; they contain UI's optional,
// non-original console-binding extension and reduce to stock behavior for
// cgame and stock UI builds.
//
// Item_Bind_HandleKey — the ui_shared.c key/mouse handler for an ITEM_TYPE_BIND
// control (a "press a key to bind this action" row). Two phases:
//
//   1. Arm: if the mouse cursor (DC->cursorx/cursory, +0xf4/+0xf8) is inside the
//      item's window.rect and the UI is not already capturing (g_waitingForKey == 0),
//      then a *down* press of K_MOUSE1 (0xc8) or K_ENTER (0xd) arms capture —
//      g_waitingForKey = 1 and g_bindItem = item (0x30134d38) — and returns qtrue.
//      A non-arming key inside the rect still returns qtrue (the item consumed it).
//
//   2. Reassign: once armed (g_waitingForKey != 0 and g_bindItem != NULL), the next
//      non-character key press (K_CHAR_FLAG 0x400 is ignored) is applied to the
//      g_bindings[] row named by the current item->cvar (+0x118), located with
//      BindingIDFromName(command):
//        - K_ESCAPE (0x1b): cancel — clear g_waitingForKey, return qtrue.
//        - key 0x60 (the console/grave key): ignored — return qtrue.
//        - K_BACKSPACE (0x7f): clear the binding. Machine code routes this by
//          setting the working key to -1 (only when the command already has a row)
//          and falling into the common finalize path, whose key == -1 branch unbinds
//          both keys of that row via DC->setBinding(oldKey, "").
//        - any other key: first strip this key from every *other* g_bindings[] row
//          (so a key is bound to at most one command), then place it into the target
//          row's bind1 (or bind2 if bind1 is taken; if both are taken it replaces
//          them, unbinding the displaced keys via setBinding("")).
//      All reassign paths finish with Controls_SetConfig() (write the table back +
//      "in_restart") and clear g_waitingForKey, then return qtrue.
//
// The function always returns qtrue (EAX == 1 on every RET).
//
// Name adjudication: the .mcode "# name BG_CalculateWeaponPosition_BobOffset" is a
// pure size-match from the wrong DLL (game_mp_uo) and is REJECTED per the
// size-match naming rule — there is no weapon or bob math here. Identity is proven
// by (1) DC (0x30134d2c) driving cursorx/cursory + setBinding, (2) the g_bindings[]
// table (0x3008adc8 = &g_bindings[0].bind1) rewritten by index, (3) BindingIDFromName
// / Controls_SetConfig callees, and (4) the g_waitingForKey/g_bindItem capture pair.
// This is exactly Q3 ui_shared.c Item_Bind_HandleKey.
//
// 0x30074a0c is the empty string "" (dumped: objdump .rdata -> a zero-length C
// string at 0x30074a0c); setBinding(key, "") is the engine's "unbind this key" call.
//
// ABI (recorded, not modeled as attributes): item arrives in EDX, key in EAX
// (immediately copied to EBX), down in ECX — a fastcall-ish/inlined-callsite shape.
// BindingIDFromName takes its command in EDI (register arg), matching its own
// recovered ABI. Callee-saved EBX/ESI/EDI pushed/popped; returns qboolean in EAX.
// Modeled as normal C parameters until an i386 Windows build needs explicit
// calling-convention attributes.

#include "ui_runtime.h"

#include "ui_menu_globals.h"

extern displayContextDef_t *DC;

enum {
    BIND_KEY_ENTER = 13,
    BIND_KEY_ESCAPE = 27,
    BIND_KEY_CONSOLE = 96,
    BIND_KEY_BACKSPACE = 127,
    BIND_KEY_MOUSE1 = 200,
    BIND_KEY_CHARACTER_FLAG = 0x400
};

#define BIND_KEY_UNBOUND ((int32_t)-1)

qboolean Item_Bind_HandleKey(itemDef_t *item, int32_t key, qboolean down)
{
    int32_t id;
    bind_t *binding;
    displayContextDef_t *display = DC; /* 0x30057024: one cursor snapshot base */
    float cursorY = (float)display->cursory; /* FILD then FSTP m32 */
    long double cursorX = (long double)display->cursorx; /* FILD stays in ST0 */
    qboolean cursorInside = qfalse;

    /* 0x30057040..0x30057076: the right/bottom edges are x87 sums of the two
     * float fields, not pre-rounded float additions. Unordered comparisons are
     * outside, matching the status-word tests. */
    if (item != NULL) {
        const rectDef_t *rect = &item->window.rect;
        cursorInside = cursorX >= (long double)rect->x && cursorX <= (long double)rect->x + (long double)rect->w &&
                       (long double)cursorY >= (long double)rect->y && (long double)cursorY <= (long double)rect->y + (long double)rect->h;
    }

    // 0x30057032..0x30057078: an inside-rect hit while NOT already capturing enters
    // the arm branch. A NULL item (0x30057032 TEST ESI) or an outside-rect cursor
    // falls through to the reassignment/guard branch below (via 0x300570ae/0x300570b0).
    if (cursorInside && g_waitingForKey == 0) {
        // 0x30057081: only a *down* press arms; a key-up inside the rect just
        // returns qtrue without arming.
        if (down != 0 && (key == BIND_KEY_MOUSE1 || key == BIND_KEY_ENTER)) {
            // 0x3005709f/0x300570a6: capture the item and arm.
            g_bindItem = item;
            g_waitingForKey = 1;
            client_ui_compat_bind_capture_started(item);
        }
        return qtrue;
    }

    // 0x300570b0..0x300570c4: not (or no longer) an arming hit. If nothing is being
    // captured, or no capture item is set, the press is not ours — return qtrue.
    if (g_waitingForKey == 0 || g_bindItem == NULL) {
        return qtrue;
    }

    // 0x300570ca: character keys (K_CHAR_FLAG 0x400) never form a binding.
    if ((key & BIND_KEY_CHARACTER_FLAG) != 0) {
        return qtrue;
    }

    // 0x300570d3: K_ESCAPE cancels the capture without touching any binding.
    if (key == BIND_KEY_ESCAPE) {
        // 0x300571fd: clear the wait flag and return qtrue.
        g_waitingForKey = 0;
        client_ui_compat_bind_capture_finished();
        return qtrue;
    }

    // 0x300570dc: the stock console/grave key (0x60) is swallowed but ignored.
    if (client_ui_compat_bind_key_is_ignored(item, key) != qfalse) {
        return qtrue;
    }

    client_ui_compat_bind_capture_finished();

    // 0x300570e5..0x30057103: decide the "working key" to apply.
    //   - K_BACKSPACE (0x7f): if the current item's command has a table row,
    //     force key = -1 so the finalize path unbinds it; if it has no row, the
    //     machine code leaves key unchanged and continues through duplicate removal.
    //   - otherwise, when key != -1, strip this key from every other g_bindings[]
    //     row first so it ends up bound to at most one command.
    if (key == BIND_KEY_BACKSPACE) {
        // 0x300570eb: ESI remains the incoming item; the capture global is only
        // a guard above. BindingIDFromName(item->cvar).
        id = BindingIDFromName(item->cvar);
        if (id != BIND_KEY_UNBOUND || client_ui_compat_extra_binding_for_name(item->cvar) != NULL) {
            key = BIND_KEY_UNBOUND;
        }
    }
    if (key != BIND_KEY_UNBOUND) {
        // 0x30057110..0x30057132: walk all 55 g_bindings[] rows, removing `key`.
        for (int32_t i = 0; i < CONTROL_BINDING_COUNT; i++) {
            bind_t *b = &g_bindings[i];
            // 0x30057110: if bind2 == key, clear it.
            if (b->bind2 == key) {
                b->bind2 = BIND_KEY_UNBOUND;
            }
            // 0x3005711a: if bind1 == key, slide bind2 down into bind1 and clear bind2.
            if (b->bind1 == key) {
                b->bind1 = b->bind2;
                b->bind2 = BIND_KEY_UNBOUND;
            }
        }
        client_ui_compat_remove_key_from_extra_bindings(key);
    }

    // 0x30057134..0x30057143: locate the current item's command row (again — backspace
    // re-derives it). A missing row skips straight to Controls_SetConfig.
    id = BindingIDFromName(item->cvar);
    if (id != BIND_KEY_UNBOUND) {
        binding = &g_bindings[id];
    } else {
        binding = client_ui_compat_extra_binding_for_name(item->cvar);
    }
    if (binding != NULL) {

        // 0x3005714f: split on whether we are unbinding (key == -1) or assigning.
        if (key == BIND_KEY_UNBOUND) {
            // 0x3005715a..0x30057198: clear both keys of the row, telling the engine
            // to unbind each currently-bound one.
            if (binding->bind1 != BIND_KEY_UNBOUND) {
                DC->setBinding(binding->bind1, "");
                binding->bind1 = BIND_KEY_UNBOUND;
            }
            if (binding->bind2 != BIND_KEY_UNBOUND) {
                DC->setBinding(binding->bind2, "");
                binding->bind2 = BIND_KEY_UNBOUND;
            }
        } else if (binding->bind1 == BIND_KEY_UNBOUND) {
            // 0x3005719a/0x3005719f: first key slot is free — take it.
            binding->bind1 = key;
        } else if (binding->bind1 != key && binding->bind2 == BIND_KEY_UNBOUND) {
            // 0x300571a7..0x300571ba: bind1 already used by a different key and
            // bind2 free — take the second slot.
            binding->bind2 = key;
        } else {
            // 0x300571bc..0x300571f8: both slots occupied (or bind1 already equals
            // this key) — unbind both existing keys and make this key the sole
            // primary binding.
            DC->setBinding(binding->bind1, "");
            DC->setBinding(binding->bind2, "");
            binding->bind1 = key;
            binding->bind2 = BIND_KEY_UNBOUND;
        }
    }

    // 0x300571f8: commit the edited g_bindings[] table to the engine and restart input.
    client_ui_compat_controls_set_config();
    // 0x300571fd: capture done — clear the wait flag. (Note: unlike upstream Q3,
    // this build does NOT clear g_bindItem here; it is left pointing at the last
    // captured item until the next arm overwrites it.)
    g_waitingForKey = 0;
    // 0x30057207..0x3005720f: always return qtrue.
    return qtrue;
}
