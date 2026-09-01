#include "../client_recovered.h"

#include <stddef.h>
#include <string.h>

// Source: uo_cgame_mp_x86.dll 0x30031a00..0x30031a56
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30031a00_30031a56.mcode
//
// Issues cgame engine syscall id 54 (CG_R_TEXT_PAINT) for the object pointed to by
// its register argument, passing the object's leading dword, the float sum of
// two of its members, three caller-supplied stack words, and the shared global
// string buffer cg_trapStringBufferB (0x30538700), followed by three zero words.
//
// Name adjudication: the .mcode header's size-matched "BG_PlayAnimName" guess is
// REJECTED. This function plays no animation and looks up nothing by name; it
// computes a float sum from the argument object and forwards a fixed argument
// vector to trap 54. Its byte-for-byte twin at 0x30031940 is identical except it
// targets the sibling buffer cg_trapStringBufferA (0x30538600) instead of B, and
// the third sibling at 0x30031a90 wraps the same trap-54 call with a gated
// trap-52 preamble; together they are one small trap-54 emitter family. The exact
// original identity is established by retail UO's CG_AREA_CHAT owner-draw id and
// the matching same-module macOS symbol CG_DrawAreaChat. Trap 54 is
// CG_R_TEXT_PAINT.
//
// Register-argument ABI: the object pointer arrives in EAX (never reloaded from
// the stack at entry, and read before any stack traffic). The three forwarded
// words arrive as ordinary cdecl stack arguments (plain RET, no callee cleanup of
// the incoming slots; the ADD ESP,0x34 only unwinds this frame's SUB ESP,0xc plus
// the 10 dwords pushed for the syscall).
//
// Instruction map (post-`SUB ESP,0xc` frame base E; incoming args at E+0x10,
// E+0x14, E+0x18; the compiler spills through E+0x0/E+0x4/E+0x8 scratch slots):
//   30031a03 FLD  [EAX+0xc]         st0 = obj->h                     (float)
//   30031a06 MOV  EDX,[EAX]         EDX = bits(obj->x)                   (raw dword)
//   30031a08 FADD [EAX+0x4]         st0 = obj->h + obj->y          (float add)
//   30031a0b MOV  ECX,[ESP+0x14]    ECX = arg0 (E+0x14)
//   30031a0f PUSH 0                 -> 4th trailing zero word
//   30031a11 PUSH 0                 -> 3rd trailing zero word
//   30031a13 MOV  [E+0x0],0         scratch = 0
//   30031a1b FSTP [E+0x4]           scratch f = obj->h + obj->y    (store float sum)
//   30031a1f MOV  EAX,[E+0x0]       EAX = 0
//   30031a23 PUSH EAX               -> zero word (syscall arg7)
//   30031a24 MOV  EAX,[E+0x10]      EAX = arg2 (E+0x10)
//   30031a28 PUSH 0x30538700        -> buffer (syscall arg6)
//   30031a2d MOV  [E+0x14],ECX      (spill arg0)
//   30031a31 MOV  ECX,[E+0x18]      ECX = arg1 (E+0x18)
//   30031a35 PUSH ECX               -> arg1 (syscall arg5)
//   30031a36 MOV  ECX,[E+0x4]       ECX = float-sum bits
//   30031a3a MOV  [E+0x8],EDX       (spill bits(obj->x))
//   30031a3e MOV  EDX,[E+0x14]      EDX = arg0
//   30031a42 PUSH EDX               -> arg0 (syscall arg4)
//   30031a43 MOV  EDX,[E+0x8]       EDX = bits(obj->x)
//   30031a47 PUSH EAX               -> arg2 (syscall arg3)
//   30031a48 PUSH ECX               -> float sum (syscall arg2)
//   30031a49 PUSH EDX               -> bits(obj->x) (syscall arg1)
//   30031a4a PUSH 0x36              -> command id 54 (syscall arg0)
//   30031a4c CALL *cgame_syscall
//   30031a52 ADD  ESP,0x34 ; RET
//
// Incoming stack arguments (cdecl): stackArg0 = [E+0x10], stackArg1 = [E+0x14],
// stackArg2 = [E+0x18].
//
// Resulting call (arg order = reverse of push order):
//   cgame_syscall(54, bits(obj->x), <float sum bits>, stackArg0, stackArg1,
//                 stackArg2, cg_trapStringBufferB, 0, 0, 0)
//
// The float sum is passed by its 32-bit bit pattern in a syscall dword; it is
// produced with a single x87 FADD (float precision throughout), so it is
// reconstructed as `float` and forwarded as raw bits.

/* rectDef_t and the area-chat declarations were promoted to the
 * shared client_recovered.h when the twin at 0x30031940 became a second user. */

void CG_DrawAreaChat(rectDef_t *obj, intptr_t stackArg0, intptr_t stackArg1,
                     intptr_t stackArg2)
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
                  (intptr_t)cg_trapStringBufferB,
                  0,
                  0,
                  0);
}
