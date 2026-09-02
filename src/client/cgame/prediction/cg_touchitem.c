// Source: uo_cgame_mp_x86.dll 0x30035680..0x3003570e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30035680_3003570e.mcode
//
// CG_TouchItem - client-side predicted item pickup for one client entity.
//
// If item prediction is enabled, and the predicted player origin is inside the
// pickup box of this item entity, and the item was not already touched this
// frame, and BG_CanItemBeGrabbed allows the pickup, then hide the item locally
// (eFlags |= EF_NODRAW) and append an EV_ITEM_PICKUP predictable event carrying
// the item's modelindex to cg.predictedPlayerState, so the local player sees the
// pickup a frame before the server snapshot confirms it. This is the textbook
// Quake3/CoD CG_TouchItem: cg_predictItems_vmCvar.integer gate + BG_PlayerTouchesItem +
// miscTime dedup + BG_CanItemBeGrabbed + BG_AddPredictableEventToPlayerstate.
//
// Naming: the .mcode header carries the SIZE-GUESS name "AxisCopy" (matched only
// on byte size 0x8e). REJECTED: this function copies no 3x3 matrix; it gates on
// globals, calls two BG item helpers, and mutates the predicted player state's
// event rings. Identification is proven by the call graph: callee 0x30005e00 is
// BG_CanItemBeGrabbed (its own "BG_CanItemBeGrabbed: index out of range" /
// "BG_CanItemBeGrabbed: IT_BAD" diagnostic strings), and callee 0x30005d70 is
// the item-pickup proximity test BG_PlayerTouchesItem (box x,y +/-36, z -88..+18
// against cg.predictedPlayerState). Event id 148 (0x94) stored into the 4-slot
// playerState event ring is EV_ITEM_PICKUP.
//
// ABI: the client entity is passed in EDI (register calling convention). ESI is
// caller-saved across the BG_PlayerTouchesItem call (PUSH/POP ESI); the single
// dword pushed before BG_CanItemBeGrabbed is caller-cleaned (ADD ESP,4). Plain
// RET, no stack args, no return value.

#include <stddef.h>
#include <stdint.h>

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

// Offsets this function proves against the machine code (i386, 4-byte pointers):
_Static_assert(offsetof(centity_t, currentState.eFlags) == 0x08, "currentState.eFlags at +0x08 (OR [EDI+0x8],0x80)");
_Static_assert(offsetof(centity_t, currentState.itemIndex) == 0x8c, "currentState.modelindex at +0x8c (MOV EAX,[EDI+0x8c])");
_Static_assert(offsetof(centity_t, currentState.clientNum) == 0x94, "currentState.otherEntityNum at +0x94");
_Static_assert(offsetof(centity_t, miscTime) == 0x1fc, "centity miscTime at +0x1fc (MOV [EDI+0x1fc],ECX)");

// The predicted player state is the typed object based at the fixed cgame address
// 0x304831c4 (declared in globals.h as cg_predictedPlayerState). The machine code forms
// &cg_predictedPlayerState twice as two
// differently-typed views:
//   ESI = 0x304831c4 -> BG_PlayerTouchesItem reference base (reads origin +0x14),
//   EAX = 0x304831c4 -> playerState_t for BG_CanItemBeGrabbed (psClientNum +0xd4,
//                       weaponBits +0x534).
// Each callee still carries a provisional parameter view type, so the shared base
// is cast to the matching view at the call boundary (one symbol, one storage; not
// an alias). Its event rings are consumed through the individually-declared field
// globals cg_predictedPlayerState.eventIndex/Events/EventParms.

void CG_TouchItem(centity_t *cent)
{
    // 30035680..30035687: if (!cg_predictItems_vmCvar.integer) return;
    if (!cg_predictItems_vmCvar.integer) {
        return;
    }

    // 3003568d..300356a2: BG_PlayerTouchesItem tests whether the predicted player
    // origin is inside this item's pickup box at cg.time. atTime = cg_time
    // arrives in EAX; the entity in ECX; the reference base (&cg.predictedPlayerState)
    // in ESI. Bail if the player is not touching the item.
    if (!CG_TrajectoryPointInBounds(cent, &cg_predictedPlayerState, coduo_int32_from_bits(cg_time))) {
        return;
    }

    // 300356a4..300356b0: don't touch the same item twice in one frame.
    if (cent->miscTime == coduo_int32_from_bits(cg_time)) {
        return;
    }

    // 300356b2..300356c5: BG_CanItemBeGrabbed(item, ps, canTake=1); bail if the
    // player cannot currently hold this item.
    if (!BG_CanItemBeGrabbed(&cent->currentState, &cg_predictedPlayerState, 1)) {
        return;
    }

    // 300356c7: hide the item locally until the server snapshot confirms pickup.
    cent->currentState.eFlags |= EF_NODRAW;

    // 300356ce/300356d4/300356da: record the touch time and cache the item id.
    // EAX holds cent->currentState.itemIndex from here through the parm store below.
    int32_t modelindex = cent->currentState.itemIndex;
    cent->miscTime = coduo_int32_from_bits(cg_time);

    // 300356e0..30035707: append the predictable pickup event to the player state
    // event rings (BG_AddPredictableEventToPlayerstate, inlined):
    //   events[eventIndex & 3]     = EV_ITEM_PICKUP;
    //   eventParms[eventIndex & 3] = (uint8_t)modelindex;   // MOVZX EAX,AL
    //   eventIndex++;
    // Note the machine code re-reads eventIndex for the parm store; the mask is
    // recomputed each time but the value is unchanged until the final INC.
    cg_predictedPlayerState.events[cg_predictedPlayerState.eventIndex & (MAX_PS_EVENTS - 1)] = EV_ITEM_PICKUP;
    cg_predictedPlayerState.eventParms[cg_predictedPlayerState.eventIndex & (MAX_PS_EVENTS - 1)] = (int32_t)(uint32_t)(uint8_t)modelindex;
    cg_predictedPlayerState.eventIndex = coduo_int32_from_bits((uint32_t)cg_predictedPlayerState.eventIndex + 1u);
}
