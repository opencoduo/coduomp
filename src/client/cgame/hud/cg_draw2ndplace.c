#include "../client_recovered.h"

#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x30031bd0..0x30031c3c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30031bd0_30031c3c.mcode
//
// Fifth member of the CG_R_TEXT_PAINT emitter family. It issues cgame engine syscall
// id 54 (CG_R_TEXT_PAINT) for the object pointed to by its register argument, forwarding
// the object's two leading raw dwords, three caller words, a string argument, two
// zero words, and a fourth caller word (the same 10-slot argument vector shape as
// CG_DrawPlayerBarHealthTitle at 0x3002fca0). It differs in two proven ways:
//   1) It is gated on the system-info HUD stat cg_hudStat6Value. When that
//      stat holds its "unset" sentinel -9999 (0xffffd8f1), the whole emit is skipped
//      and the function returns without calling the syscall.
//   2) The string argument is produced by va("%2i", cg_hudStat6Value) rather
//      than from a translated or static buffer.
//
// Emitted call (proven by the push trace below):
//   cgame_syscall(CG_R_TEXT_PAINT,
//                 bits(obj->x), bits(obj->y),          // ESI[0], ESI[4], raw dwords
//                 arg0, arg1, arg2,                // three forwarded caller words
//                 va("%2i", cg_hudStat6Value),  // string buffer
//                 0, 0,                            // two zero words
//                 arg3);                           // fourth caller word (final slot)
//
// Name adjudication: the .mcode header's size-matched "PitchToQuaternion" guess is
// REJECTED. There is no quaternion math and no x87 in this function at all; its body
// is the byte-exact text-paint argument vector forwarded to CG_R_TEXT_PAINT, gated
// on a HUD stat and using a va("%2i", stat) string. Retail UO assigns this owner-draw
// case CG_2NDPLACE, and the macOS owner-draw jump table names its target
// CG_Draw2ndPlace. `va` is a caller-observed provisional decl (callee 0x3004e8a0,
// a q_shared-style sprintf-into-ring-buffer); superseded when 0x3004e8a0 is
// reconstructed from its own .mcode. cg_hudStat6Value is an atoi-parsed HUD
// stat (see globals.h); the mechanical owner=script_orbit label is rejected.
//
// Register-argument ABI: the object pointer arrives in ESI (read via MOV EDX,[ESI+4]
// / MOV ECX,[ESI] at 0x30031bea / 0x30031bf5; never set from a stack slot, never
// saved/restored). The caller (0x30032597) loads ESI = LEA 0x1c(%esp) to a local
// object and pushes four cdecl stack words, cleaning them with ADD ESP,0x10 after the
// plain RET here. The function's own ADD ESP,0x30 unwinds the 0x30 bytes pushed for
// the syscall; the ADD ESP,0x10 unwinds the entry SUB ESP,0x10 scratch frame.
//
// Instruction map (frame base E = ESP after `SUB ESP,0x10`; incoming stack args at
// arg0=E+0x14, arg1=E+0x18, arg2=E+0x1c, arg3=E+0x20; the compiler spills through
// E+0x4/E+0x8/E+0xc scratch slots, and E+0x0 = 0 for the zero words):
//   30031bd0 MOV  EAX,[0x304480e0]   EAX = cg_hudStat6Value
//   30031bd8 CMP  EAX,0xffffd8f1     compare against sentinel -9999
//   30031bdd JZ   0x30031c38         if sentinel -> skip emit, return
//   30031bdf PUSH EAX                va arg: the stat value
//   30031be0 PUSH 0x30076c3c         va format: "%2i"
//   30031be5 CALL 0x3004e8a0         EAX = va("%2i", stat)   (string pointer)
//   30031bea MOV  EDX,[ESI+0x4]      EDX = bits(obj->y)
//   30031bf5 MOV  ECX,[ESI]          ECX = bits(obj->x)
//   ... interleaved PUSH sequence assembles (low addr first, id lowest):
//   0x36 | bits(obj->x) | bits(obj->y) | arg0 | arg1 | arg2 | va_string | 0 | 0 | arg3
//   30031c2f CALL [0x30085e9c]       cgame_syscall(...)
//   30031c35 ADD  ESP,0x30 ; 30031c38 ADD ESP,0x10 ; RET

void CG_Draw2ndPlace(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3)
{
    /* -9999 (0xffffd8f1) is the "score unset" sentinel; when set, nothing is emitted. */
    if (cg_hudStat6Value == CG_SCORE_VALUE_UNSET)
        return;

    const char *statString = va("%2i", cg_hudStat6Value);

    cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(obj->x), CG_FloatBits(obj->y), arg0, arg1, arg2, (intptr_t)statString, 0, 0, arg3);
}
