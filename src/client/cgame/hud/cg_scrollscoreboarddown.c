// Source: uo_cgame_mp_x86.dll 0x30037e60..0x30037e8b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30037e60_30037e8b.mcode
//
// CG_ScrollScoreboardDown — scroll the scoreboard list down by one step.
//
// Reads only scoreboard-state globals; no arguments, returns void (plain RET,
// no callee stack cleanup):
//   30037e60  MOV EAX,[0x3048a564]   ; cg_scoreboardOverflowed
//   30037e65  TEST EAX,EAX
//   30037e67  JZ  0x30037e8a         ; nothing overflowed off the bottom -> no scroll
//   30037e69  MOV EAX,[0x3048a560]   ; cg_scoreboardScrollPos
//   30037e6e  ADD EAX,[0x30421d2c]   ; + cg_scoreboardScrollStep_vmCvar.integer
//   30037e74  MOV ECX,[0x30489f20]   ; cg_scoreboardNumClients
//   30037e7a  DEC ECX                ; limit = count - 1
//   30037e7b  CMP EAX,ECX
//   30037e7d  MOV [0x3048a560],EAX   ; store the advanced position first...
//   30037e82  JLE 0x30037e8a         ; signed: if advanced <= limit, keep it
//   30037e84  MOV [0x3048a560],ECX   ; ...else clamp down to limit
//   30037e8a  RET
//
// The machine code writes the advanced position unconditionally, then overwrites
// it with the clamp value on the >-limit path. Both the CMP/JLE and DEC operate
// on signed int32 (scroll indices), matching the signed global types.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_ScrollScoreboardDown(void)
{
    int32_t newPos;
    int32_t limit;

    /* Only scroll when content has flowed past the bottom of the visible area. */
    if (cg_scoreboardOverflowed == 0) {
        return;
    }

    /* 0x30037e6e ADD and 0x30037e7a DEC are modulo-2^32 dword
     * operations; their results are interpreted as signed only by CMP/JLE. */
    newPos = coduo_int32_from_bits((uint32_t)cg_scoreboardScrollPos +
                              (uint32_t)cg_scoreboardScrollStep_vmCvar.integer);
    limit = coduo_int32_from_bits((uint32_t)cg_scoreboardNumClients - 1u);

    cg_scoreboardScrollPos = newPos;
    if (newPos > limit) {
        cg_scoreboardScrollPos = limit;
    }
}
