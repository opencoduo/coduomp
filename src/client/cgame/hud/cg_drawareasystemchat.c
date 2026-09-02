#include "../client_recovered.h"

#include <stdint.h>
#include <string.h>

// Source: uo_cgame_mp_x86.dll 0x30031940..0x30031996
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30031940_30031996.mcode
//
// Issues cgame engine syscall id 54 (CG_R_TEXT_PAINT) for the object pointed to by its
// register argument, passing the object's leading dword, the float sum of two of
// its members, three caller-supplied stack words, and the shared global string
// buffer cg_trapStringBufferA (0x30538600), followed by three zero words. It is the
// byte-for-byte twin of CG_DrawAreaChat (0x30031a00); the ONLY difference between
// the two is the pushed buffer address (A=0x30538600 here vs B=0x30538700 there).
//
// Name adjudication: the .mcode header's size-matched "script_func_length" guess is
// REJECTED. This function computes no string length and looks up nothing; it derives
// a float sum from the register-passed object and forwards a fixed argument vector to
// trap 54. Together with the twin at 0x30031a00 and the trap-52-gated sibling at
// 0x30031a90 it forms one small trap-54 emitter family. The exact original CoD symbol
// is established by retail UO's CG_AREA_SYSTEMCHAT owner-draw id and the matching
// same-module macOS symbol CG_DrawAreaSystemChat. Trap 54 is CG_R_TEXT_PAINT.
//
// Register-argument ABI: the object pointer arrives in EAX (read at entry before any
// stack traffic, never reloaded from a stack slot). The three forwarded words arrive
// as ordinary cdecl stack arguments — the function ends in a plain RET (no callee
// cleanup of the incoming slots); the ADD ESP,0x34 only unwinds this frame's
// SUB ESP,0xc plus the 10 dwords pushed for the syscall. The caller at 0x3003236f
// confirms this: it sets EAX via `lea 0x18(%esp)` (pointer to a local object), pushes
// three dwords, calls, and cleans up with `add $0xc,%esp` (3 stack args, cdecl).
//
// Instruction map (post-`SUB ESP,0xc` frame base E; incoming stack args at E+0x10,
// E+0x14, E+0x18; the compiler spills through E+0x0/E+0x4/E+0x8 scratch slots):
//   30031943 FLD  [EAX+0xc]         st0 = obj->h                     (float)
//   30031946 MOV  EDX,[EAX]         EDX = bits(obj->x)                   (raw dword)
//   30031948 FADD [EAX+0x4]         st0 = obj->h + obj->y          (float add)
//   3003194b MOV  ECX,[ESP+0x14]    ECX = arg0 (E+0x14)
//   3003194f PUSH 0                 -> 4th trailing zero word
//   30031951 PUSH 0                 -> 3rd trailing zero word
//   30031953 MOV  [E+0x0],0         scratch = 0
//   3003195b FSTP [E+0x4]           scratch f = obj->h + obj->y    (store float sum)
//   3003195f MOV  EAX,[E+0x0]       EAX = 0
//   30031963 PUSH EAX               -> zero word (syscall arg7)
//   30031964 MOV  EAX,[E+0x10]      EAX = arg2 (E+0x10)
//   30031968 PUSH 0x30538600        -> cg_trapStringBufferA (syscall arg6)
//   3003196d MOV  [E+0x14],ECX      (spill arg0)
//   30031971 MOV  ECX,[E+0x18]      ECX = arg1 (E+0x18)
//   30031975 PUSH ECX               -> arg1 (syscall arg5)
//   30031976 MOV  ECX,[E+0x4]       ECX = float-sum bits
//   3003197a MOV  [E+0x8],EDX       (spill bits(obj->x))
//   3003197e MOV  EDX,[E+0x14]      EDX = arg0
//   30031982 PUSH EDX               -> arg0 (syscall arg4)
//   30031983 MOV  EDX,[E+0x8]       EDX = bits(obj->x)
//   30031987 PUSH EAX               -> arg2 (syscall arg3)
//   30031988 PUSH ECX               -> float sum (syscall arg2)
//   30031989 PUSH EDX               -> bits(obj->x) (syscall arg1)
//   3003198a PUSH 0x36              -> command id 54 (syscall arg0)
//   3003198c CALL *cgame_syscall
//   30031992 ADD  ESP,0x34 ; RET
//
// Resulting call (arg order = reverse of push order):
//   cgame_syscall(54, bits(obj->x), <float sum bits>, arg2, arg0, arg1,
//                 cg_trapStringBufferA, 0, 0, 0)
//
// The float sum is passed by its 32-bit bit pattern in a syscall dword; it is
// produced with a single x87 FADD (float precision throughout), so it is
// reconstructed as `float` and forwarded as raw bits.

/* rectDef_t and the area-chat declarations are shared in
 * client_recovered.h (promoted when this twin became a second user). */

void CG_DrawAreaSystemChat(rectDef_t *obj, intptr_t stackArg0,
                           intptr_t stackArg1, intptr_t stackArg2)
{
    float sum = obj->h + obj->y;
    int32_t sumBits;

    /* the sum is forwarded through a plain 32-bit syscall slot as its bit pattern */
    memcpy(&sumBits, &sum, sizeof(sumBits));

    cgame_syscall(CG_R_TEXT_PAINT,
                  CG_FloatBits(obj->x),
                  sumBits,
                  stackArg0,
                  stackArg1,
                  stackArg2,
                  (intptr_t)cg_trapStringBufferA,
                  0,
                  0,
                  0);
}
