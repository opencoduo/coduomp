// Sources: uo_cgame_mp_x86.dll 0x30053460..0x3005348d and
//          uo_ui_mp_x86.dll    0x40014fb0..0x40014fdd
//
// Item_OwnerDraw_HandleKey — give an owner-drawn menu item first crack at a
// key/mouse event by dispatching to the display context's owner-draw key handler
// (displayContextDef_t slot +0x88, DC->ownerDrawHandleKey). Returns the handler's
// result (nonzero => the key was consumed by the owner-draw), or qfalse when the
// item is NULL or the DC has no ownerDrawHandleKey installed. This is the standard
// Q3 ui_shared.c Item_OwnerDraw_HandleKey wrapper.
//
// Name: the mcode size-guess "script_func_iprintln" is matched only by byte size
// (win 0x2d == corpus 0x2d) and is REJECTED — this function performs no printing.
// The behavior is a NULL-guarded indirect call through DC (0x30134d2c) slot +0x88
// forwarding the item's owner-draw id/flags, a pointer to its feeder id (special,
// +0x24c), and the key. The slot at +0x88 sits between DC->startLocalSound (+0x84)
// and DC->feederCount (+0x8c); its (int ownerDraw, int flags, float *special, int key)
// call shape is exactly Q3 ui_shared.c DC->ownerDrawHandleKey.
//
// ABI (0x30053460): `item` arrives in EAX, `key` in EDX (register args, the
// convention used throughout this corpus). The int32/qboolean result is returned in
// EAX. The DC call itself is __cdecl (caller pops with ADD ESP,0x10).
//
// Machine-code trace:
//   30053460  TEST EAX,EAX / JZ 0x3005348a           ; item == NULL => return 0
//   30053464  MOV ECX,[0x30134d2c]                    ; ECX = DC
//   3005346a  MOV ECX,[ECX+0x88]                      ; ECX = DC->ownerDrawHandleKey
//   30053470  TEST ECX,ECX / JZ 0x3005348a            ; slot == NULL => return 0
//   30053474  PUSH EDX                                ; arg4 = key
//   30053475  LEA EDX,[EAX+0x24c] / PUSH EDX          ; arg3 = &item->special
//   3005347c  MOV EDX,[EAX+0x40] / (pushed below)     ; arg2 = item->window.ownerDrawFlags
//   3005347f  MOV EAX,[EAX+0x3c]                      ; arg1 = item->window.ownerDraw
//   30053482  PUSH EDX / PUSH EAX                     ; push flags then ownerDraw
//   30053484  CALL ECX                                ; ownerDrawHandleKey(...)
//   30053486  ADD ESP,0x10 / RET                      ; return its result (in EAX)
//   3005348a  XOR EAX,EAX / RET                       ; guarded-out => return 0

#include "ui_runtime.h"

#include <stddef.h>

extern displayContextDef_t *DC;

qboolean Item_OwnerDraw_HandleKey(itemDef_t *item, int32_t key)
{
    // 0x30053460: item == NULL => nothing to dispatch to.
    if (item == NULL) {
        return qfalse;
    }

    // 0x30053464/0x3005346a/0x30053470: the DC must have an ownerDrawHandleKey
    // installed (the DC pointer itself is assumed valid here — the machine code
    // dereferences it without a NULL check).
    ui_ownerDrawHandleKey_t handler = DC->ownerDrawHandleKey;
    if (handler == NULL) {
        return qfalse;
    }

    // 0x30053474..0x30053486: forward the owner-draw id/flags, the address of the
    // item's feeder id, and the key; return whatever the handler reports.
    return (qboolean)handler(item->window.ownerDraw, item->window.ownerDrawFlags, &item->special, key);
}
