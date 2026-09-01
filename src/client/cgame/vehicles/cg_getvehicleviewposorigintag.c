// Source: uo_cgame_mp_x86.dll 0x300407c0..0x300407f3
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300407c0_300407f3.mcode
//
// CG_GetVehicleViewPosOriginTag — map a vehicle view/seat mode to the model-attach
// tag string used to anchor the vehicle camera. The ghidra size-guess name
// "Scr_CopyEntityNum" is REJECTED (this is a jump-table string getter with no
// script-VM or entity-number semantics whatsoever).
//
// ABI: the mode arrives in EAX (register argument). The body computes (mode - 2)
// and, if that is <= 4 (unsigned), dispatches through a compiler-generated 5-entry
// jump table at 0x300407f4; otherwise it falls through to the default tag.
//
// Verified against the binary:
//   jump table @0x300407f4 = { 0x300407cf, 0x300407d5, 0x300407db, 0x300407e1,
//                              0x300407e7 } (indices mode-2 = 0..4)
//   string pointers (objdump -s):
//     0x300771b8 "tag_secondary_gun"   0x30072d40 "tag_passenger"
//     0x30072d30 "tag_passenger2"      0x30072d20 "tag_passenger3"
//     0x30072d10 "tag_passenger4"      0x300771ec "tag_turret" (default)
//
// Reconstructed as a clean switch (not a raw jump-table byte cast); the compiler
// re-emits the equivalent dispatch. Sole observed caller is CG_CalcVehicleViewPos
// (0x300409c8, 0x30040dca).

#include "client/cgame/client_recovered.h"

const char *CG_GetVehicleViewPosOriginTag(int32_t viewMode)
{
    switch (viewMode) {
    case 2:
        return "tag_secondary_gun";
    case 3:
        return "tag_passenger";
    case 4:
        return "tag_passenger2";
    case 5:
        return "tag_passenger3";
    case 6:
        return "tag_passenger4";
    default:
        return "tag_turret";
    }
}
