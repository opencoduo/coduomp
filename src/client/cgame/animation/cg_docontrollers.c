#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x30021fe0..0x3002201c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30021fe0_3002201c.mcode
//
// CG_DoControllers: route one render entity to its player, MG42, or vehicle DObj
// controller according to eType (+0x04). The direct callers (0x30022040 /
// 0x30022080 / 0x300220e0) obtain the relevant DObj/entity payload through the
// engine services and hand the four-word DObj part bitset here.
//
// The Mac CG_DoControllers has the identical three direct controller callees:
// CG_Player_DoControllers, CG_mg42_DoControllers, and
// CG_Vehicle_DoControllers. This resolves the source name and shows the switch
// values are entity controller types, not a separate trace-part type system.
//
// The .mcode candidate PM_GetViewHeightLerpTime is a pure size match (win 0x3c ==
// PPC 0x3c) and is REJECTED: no float/view-height/lerp work — it is a
// compiler-generated jump-table switch. Size is not evidence.

// ABI (proven from machine code): the part pointer arrives in EAX (0x30021fe1
// MOV ESI,EAX) and the DObj partBits pointer arrives in ECX, passed straight
// through untouched. HandleModelPart/HandleStatePart inherit the part in the
// shared ESI register (PUSH ECX only), while the vehicle handler receives both partBits
// and part on its stack (PUSH ECX; PUSH ESI). This is the MSVC shared-register
// switch-dispatch idiom for `switch (part->currentState.eType) { case ...: Handler(part, ctx); }`.
void CG_DoControllers(centity_t *part, uint32_t *partBits)
{
    // 0x30021fe3 MOV EAX,[ESI+0x4]; 0x30021fe6 DEC EAX; 0x30021fe7 CMP EAX,0xc;
    // 0x30021fea JA default: unsigned range test, so type must be in 1..13.
    // The compiler's case-selector byte table at 0x3002202c (0-based on type-1) is:
    //   type 1 -> player; 2..10 -> default; 11 -> MG42; 12,13 -> vehicle.
    switch (part->currentState.eType) {
    case ET_PLAYER:
        // 0x30021ffa PUSH ECX; CALL 0x30021fa0; ADD ESP,4 (part inherited in ESI).
        CG_Player_DoControllers(part, partBits);
        break;

    case ET_TURRET:
        // 0x30022005 PUSH ECX; CALL 0x3001e9f0; ADD ESP,4 (part inherited in ESI).
        CG_mg42_DoControllers(part, partBits);
        break;

    case ET_VEHICLE:
    case ET_VEHICLE_CORPSE:
        // 0x30022010 PUSH ECX; PUSH ESI; CALL 0x30020540; ADD ESP,8: partBits
        // (pushed first) and part (pushed second) are passed on the stack. The
        // target's own reconstruction proves this record is the vehicle centity
        // overlay and the second argument is the DObj part bitset.
        CG_Vehicle_DoControllers(part, partBits);
        break;

    default:
        // types 2..10 and any out-of-range type fall through to the bare
        // POP ESI; RET at 0x3002201a: a no-op for this part.
        break;
    }
}
