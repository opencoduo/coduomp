// Source: uo_cgame_mp_x86.dll 0x30031c50..0x30031ca9
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30031c50_30031ca9.mcode

#include "../client_recovered.h"

#include <stdint.h>
#include <string.h>

//
// CG_DrawGameType (0x30031c50) — member of the cgame trap-54 emitter family
// (0x30031940 CG_DrawAreaSystemChat / 0x30031a00 CG_DrawAreaChat /
// 0x300319a0 CG_DrawAreaTeamChat / 0x3002fca0
// EmitTranslated / 0x30031b60 EmitFormattedValue / ...). It reads the object's
// leading dword (bits(obj->x)) and the single-precision float sum obj->h + obj->y
// exactly like the EmitA/B/C twins, but it takes FOUR caller stack words instead of
// three (the fourth, arg3, lands in the syscall's trailing slot, matching the
// Translated/Formatted siblings) and forwards the shared serverinfo gametype string
// cgs_gametype (0x30447abc) as the string argument.
//
// Resulting call (arg order = reverse of push order):
//   cgame_syscall(54, bits(obj->x), <float sum bits>, arg0, arg1, arg2,
//                 cgs_gametype, 0, 0, arg3)
//
// Name adjudication: the .mcode header's size-matched
// "script_method_scriptbuiltin_getattachsize" guess is REJECTED (size 0x59 matched
// 0x59). The body does no script/getattach work: it derives a float sum from a
// register-passed object and forwards a fixed 10-slot argument vector to cgame trap
// 54. The same mechanical first-touch owner label is already documented as rejected
// on the 0x30447abc datum (cgs_gametype) in globals.h. The exact original CoD symbol
// and the engine service behind trap 54 are unproven (no cgame syscall-id table is
// recovered — matching how CG_R_TEXT_WIDTH/54/... are treated in client_recovered.h).
// Same-module macOS symbols resolve this source function as CG_DrawGameType and its
// adjacent 0x30031c40 fixed-string accessor as CG_GameTypeString; trap 54 keeps the
// machine-proven CG_R_TEXT_PAINT id.
//
// Register-argument ABI: the object pointer arrives in EAX, read at entry before any
// stack traffic (FLD [EAX+0xc] / MOV EDX,[EAX] / FADD [EAX+0x4]) and never reloaded
// from a stack slot. The four forwarded words arrive as ordinary cdecl stack
// arguments; the function ends in a plain RET (no callee cleanup of the incoming
// slots). The ADD ESP,0x34 only unwinds this frame's SUB ESP,0xc plus the 10 dwords
// pushed for the syscall.
//
// Instruction map (frame base S = ESP after SUB ESP,0xc; incoming stack args at
// arg0=S+0x10, arg1=S+0x14, arg2=S+0x18, arg3=S+0x1c; scratch spills at S+0x0..S+0x8;
// the float sum spills to S+0x4):
//   30031c53 FLD  [EAX+0xc]        st0 = obj->h                       (float)
//   30031c56 MOV  EDX,[EAX]        EDX = bits(obj->x)                     (raw dword)
//   30031c58 FADD [EAX+0x4]        st0 = obj->h + obj->y            (float add)
//   30031c5b MOV  EAX,[S+0x1c]     EAX = arg3
//   30031c5f MOV  ECX,[S+0x14]     ECX = arg1
//   30031c63 PUSH EAX             -> arg3 (final syscall slot)
//   30031c64 PUSH 0              -> zero word
//   30031c66 FSTP [S-8+0xc]=[S+4] scratch = float sum bits
//   30031c6a MOV  [S+0x14],ECX    (respill arg1 in place)
//   30031c6e MOV  EAX,[S+0x14]    EAX = arg1
//   30031c72 MOV  [S+0x8],0       scratch = 0
//   30031c7a MOV  ECX,[S+0x8]     ECX = 0
//   30031c7e PUSH ECX            -> zero word
//   30031c7f MOV  ECX,[S+0x10]    ECX = arg0     (esp=S-0xc, [esp+0x1c]=[S+0x10])
//   30031c83 PUSH 0x30447abc     -> cgs_gametype (string slot)
//   30031c88 MOV  [S+0x8],EDX     scratch = bits(obj->x)  (esp=S-0x10, [esp+0x18]=[S+8])
//   30031c8c MOV  EDX,[S+0x18]    EDX = arg2     ([esp+0x28]=[S+0x18])
//   30031c90 PUSH EDX            -> arg2
//   30031c91 MOV  EDX,[S+0x4]     EDX = float sum bits ([esp+0x18]=[S+4])
//   30031c95 PUSH EAX            -> arg1
//   30031c96 MOV  EAX,[S+0x8]     EAX = bits(obj->x)     ([esp+0x20]=[S+8])
//   30031c9a PUSH ECX            -> arg0
//   30031c9b PUSH EDX            -> float sum bits
//   30031c9c PUSH EAX            -> bits(obj->x)
//   30031c9d PUSH 0x36           -> command id 54 (CG_R_TEXT_PAINT)
//   30031c9f CALL *cgame_syscall
//   30031ca5 ADD  ESP,0x34 ; RET
//
// The float sum is passed by its 32-bit bit pattern through a syscall dword; it is
// produced with a single x87 FADD (float precision throughout), so it is
// reconstructed as `float` and forwarded as raw bits — exactly as the rest of the
// CG_R_TEXT_PAINT emitter family does.

void CG_DrawGameType(rectDef_t *obj /* EAX */,
                     intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3)
{
    /* 0x30031c53..0x30031c58 loads h, snapshots the raw x dword, then adds y.
     * Keeping x after the completed sum would change the retail memory-access
     * order for this register-passed object. */
    const float height = obj->h;
    const int32_t xBits = CG_FloatBits(obj->x);
    float sum = height + obj->y;
    int32_t sumBits;

    /* the sum is forwarded through a plain 32-bit syscall slot as its bit pattern */
    memcpy(&sumBits, &sum, sizeof(sumBits));

    cgame_syscall(CG_R_TEXT_PAINT,
                  xBits,
                  sumBits,
                  arg0,
                  arg1,
                  arg2,
                  (intptr_t)cgs_gametype,
                  0,
                  0,
                  arg3);
}
