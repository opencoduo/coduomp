#include "../client_recovered.h"

#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x30031b60..0x30031bcc
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30031b60_30031bcc.mcode
//
// Fifth member of the cgame trap-54 emitter family (0x30031940 / 0x30031a00 /
// 0x300319a0 / 0x3002fca0 / this). Like CG_DrawPlayerBarHealthTitle (0x3002fca0) it
// takes the object in ESI plus four cdecl stack words, reads bits(obj->x) and
// bits(obj->y) as raw dwords, and forwards the fixed 10-slot vector
//   cgame_syscall(CG_R_TEXT_PAINT, word0, word4, arg0, arg1, arg2, <string>, 0, 0, arg3)
// The one difference from the translated sibling is the string slot: this member
// formats a parsed integer config value as a right-aligned 2-wide decimal with
// va("%2i", value). If that value is still the -9999 "unset" sentinel, the whole
// format is skipped and the function returns immediately without emitting the trap.
//
// Name adjudication: the .mcode header's size-matched "YawToQuaternion" guess is
// REJECTED. There is no quaternion math and no floating-point instruction anywhere
// in the body (no FLD/FADD/FSTP); it is a fixed trap-54 argument vector with a
// va()-formatted integer string. The exact original CoD symbol and the engine
// service call is CG_R_TEXT_PAINT. Retail UO assigns this owner-draw case
// CG_1STPLACE, and the macOS owner-draw jump table names its target CG_Draw1stPlace.
//
// The formatted value lives in a .data int32 global at 0x304480dc. Direct machine
// -code evidence proves it is a *signed* 32-bit integer with -9999 as its
// "unset/invalid" sentinel: this function CMP EAX,0xffffd8f1 (== -9999) and the
// producer at 0x3003184f does FILD DWORD PTR [0x304480dc] (signed int -> float),
// while the writers at 0x3003885d/0x30038f6c store the return of an atoi-style
// string parser (0x3005b6ce). The exporter's owner label "menuparse_itemdef" is
// the mechanical first-touch heuristic and does not name the datum; its real source
// identity is unproven without reconstructing the parser at 0x30038830, so the
// shared symbol is left mechanical (no alias/guess) and consumed by role here.
//
// Register-argument ABI: the object pointer arrives in ESI (read via MOV [ESI+0x4]
// / MOV [ESI] at 0x30031b7a / 0x30031b85, never set from a stack slot, never
// saved/restored). The four forwarded words arrive as ordinary cdecl stack
// arguments; the function ends in a plain RET (no callee cleanup of the incoming
// slots). The ADD ESP,0x30 unwinds the two va() arg pushes (0x8) plus the 10 dwords
// (0x28) pushed for the syscall, and the trailing ADD ESP,0x10 unwinds SUB ESP,0x10.
//
// Instruction map (post-`SUB ESP,0x10` frame base S; incoming stack args at
// arg0=S+0x14, arg1=S+0x18, arg2=S+0x1c, arg3=S+0x20; scratch locals S+0x0..S+0xc):
//   30031b60 MOV  EAX,[0x304480dc]     EAX = formattedValue (signed int32)
//   30031b68 CMP  EAX,0xffffd8f1       compare against -9999 sentinel
//   30031b6d JZ   0x30031bc8           if unset, skip everything and return
//   30031b6f PUSH EAX                  va arg1 = formattedValue
//   30031b70 PUSH 0x30076c3c           va arg0 = "%2i"
//   30031b75 CALL 0x3004e8a0           EAX = va("%2i", formattedValue)  (char*)
//   30031b7a MOV  EDX,[ESI+0x4]        EDX = bits(obj->y)  (raw dword)
//   30031b7d MOV  ECX,[S+0x18]         ECX = arg1
//   30031b81 MOV  [S+0x4],ECX          scratch L4 = arg1
//   30031b85 MOV  ECX,[ESI]            ECX = bits(obj->x)  (raw dword)
//   30031b87 MOV  [S+0x8],EDX          scratch L8 = bits(obj->y)
//   30031b8b MOV  EDX,[S+0x20]         EDX = arg3
//   30031b8f PUSH EDX                  -> syscall slot (arg3, final)
//   30031b90 MOV  EDX,[S+0x1c]         EDX = arg2
//   30031b94 PUSH 0                    -> zero word
//   30031b96 MOV  [S+0xc],ECX          scratch Lc = bits(obj->x)
//   30031b9a MOV  [S+0x0],0            scratch L0 = 0
//   30031ba2 MOV  ECX,[S+0x0]          ECX = 0
//   30031ba6 PUSH ECX                  -> zero word
//   30031ba7 MOV  ECX,[S+0x14]         ECX = arg0
//   30031bab PUSH EAX                  -> va() string pointer
//   30031bac MOV  EAX,[S+0x4]          EAX = arg1
//   30031bb0 PUSH EDX                  -> arg2
//   30031bb1 MOV  EDX,[S+0x8]          EDX = bits(obj->y)
//   30031bb5 PUSH EAX                  -> arg1
//   30031bb6 MOV  EAX,[S+0xc]          EAX = bits(obj->x)
//   30031bba PUSH ECX                  -> arg0
//   30031bbb PUSH EDX                  -> bits(obj->y)
//   30031bbc PUSH EAX                  -> bits(obj->x)
//   30031bbd PUSH 0x36                 -> command id 54 (CG_R_TEXT_PAINT)
//   30031bbf CALL [0x30085e9c]         cgame_syscall(...)
//   30031bc5 ADD  ESP,0x30             unwind 2 va args + 10 syscall dwords
//   30031bc8 ADD  ESP,0x10 ; RET       unwind frame; cdecl caller cleans args
//
// Resulting call (arg order = reverse of push order; proven by the map above):
//   cgame_syscall(CG_R_TEXT_PAINT, bits(obj->x), bits(obj->y), arg0, arg1, arg2,
//                 va("%2i", formattedValue), 0, 0, arg3)

void CG_Draw1stPlace(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3)
{
    /*
     * Signed int32 config value formatted for the trap-54 HUD string. -9999 is the
     * "unset/invalid" sentinel. This is the middle sibling of the parallel HUD-stat
     * array d8/dc/e0, resolved to its proven role name at the canonical definition
     * (globals.h). FILD elsewhere (0x3003184f) proves the signed read.
     */
    int32_t formattedValue = cg_hudStat5Value;

    /* CMP EAX,0xffffd8f1 (== -9999): the sentinel gates the whole emit. */
    if (formattedValue == CG_SCORE_VALUE_UNSET)
        return;

    cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(obj->x), CG_FloatBits(obj->y), arg0, arg1, arg2, (intptr_t)va("%2i", formattedValue), 0, 0,
                  arg3);
}
