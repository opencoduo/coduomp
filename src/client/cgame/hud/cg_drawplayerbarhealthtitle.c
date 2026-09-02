#include "../client_recovered.h"

#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3002fca0..0x3002fd01
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002fca0_3002fd01.mcode
//
// Issues cgame engine syscall id 54 (CG_R_TEXT_PAINT) for the object pointed to by its
// register argument. Before the emit it runs CG_SafeTranslateString_Internal("cgame",
// "CGAME_HEALTH") and forwards the returned translated-string pointer as the
// string-buffer argument of the trap. The layout of the syscall argument vector is
// the same 10-slot CG_R_TEXT_PAINT shape used by the area-chat trio
// (0x30031940 / 0x30031a00 / 0x300319a0): a leading pair of raw object words, three
// forwarded caller words, the string buffer, then trailing zero words -- except this
// member forwards a runtime-translated string (not a static global buffer) and its
// final slot carries a fourth caller word rather than a zero.
//
// Emitted call (proven by the push trace below):
//   cgame_syscall(CG_R_TEXT_PAINT,
//                 bits(obj->x), bits(obj->y),        // ESI[0], ESI[4], raw dwords
//                 arg0, arg1, arg2,              // three forwarded caller words
//                 CG_SafeTranslateString_Internal("cgame", "CGAME_HEALTH"),  // string buffer
//                 0, 0,                          // two zero words
//                 arg3);                         // fourth caller word (final slot)
//
// Name adjudication: the .mcode header's size-matched "Cmd_Where_f" guess is
// REJECTED. This function is not a console command handler: it takes no console
// arguments, registers nothing, and its body is a fixed argument vector forwarded to
// cgame trap 54 (CG_R_TEXT_PAINT) with a translated HUD string. Retail UO assigns
// this owner-draw case CG_PLAYER_BAR_HEALTH_TITLE, and the macOS owner-draw jump
// table names its target CG_DrawPlayerBarHealthTitle. CG_SafeTranslateString_Internal
// is a caller-observed provisional decl (see header)
// proven only by its two .rdata string inputs; it is superseded when 0x3002d6e0 is
// reconstructed from its own .mcode.
//
// Register-argument ABI: the object pointer arrives in ESI (read via MOV EDX,[ESI+4]
// / MOV ECX,[ESI] at 0x3002fcb2 / 0x3002fcbd, never set from a stack slot and never
// saved/restored). The four forwarded words (arg0..arg3) arrive as ordinary cdecl
// stack arguments; the function ends in a plain RET (no callee cleanup of the
// incoming slots). The ADD ESP,0x34 only unwinds this frame's SUB ESP,0xc plus the
// 10 dwords (0x28) pushed for the syscall.
//
// Instruction map (post-`SUB ESP,0xc` frame base B; incoming stack args at
// arg0=B+0x10, arg1=B+0x14, arg2=B+0x18, arg3=B+0x1c; the compiler spills through
// B+0x0/B+0x4/B+0x8 scratch slots):
//   3002fca3 MOV  EAX,0x30077b28       EAX = "cgame"        (domain)
//   3002fca8 MOV  ECX,0x300798a4       ECX = "CGAME_HEALTH" (reference)
//   3002fcad CALL 0x3002d6e0           EAX = CG_SafeTranslateString_Internal(domain, reference)
//   3002fcb2 MOV  EDX,[ESI+0x4]        EDX = bits(obj->y)
//   3002fcbd MOV  ECX,[ESI]            ECX = bits(obj->x)
//   3002fcbf MOV  [B+0x4],EDX          scratch4 = bits(obj->y)
//   3002fcce MOV  [B+0x8],ECX          scratch8 = bits(obj->x)
//   3002fcd2 MOV  [B+0x0],0            scratch0 = 0  (the two zero words below)
//   ... interleaved PUSH sequence assembles (low addr first, id lowest):
//   0x36 | bits(obj->x) | bits(obj->y) | arg0 | arg1 | arg2 | translated | 0 | 0 | arg3
//   3002fcf7 CALL [0x30085e9c]         cgame_syscall(...)
//   3002fcfd ADD  ESP,0x34             unwind 0xc frame + 0x28 pushed args
//   3002fd00 RET                        (cdecl: caller cleans arg0..arg3)

void CG_DrawPlayerBarHealthTitle(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3)
{
    char *translated = CG_SafeTranslateString_Internal("cgame", "CGAME_HEALTH");

    cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(obj->x), CG_FloatBits(obj->y), arg0, arg1, arg2, (intptr_t)translated, 0, 0, arg3);
}
