// Source: uo_cgame_mp_x86.dll 0x3003e960..0x3003e97d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003e960_3003e97d.mcode
//
// trap_XAnimIsLooped -- the exact same-module XAnim loop-query wrapper
// (CG_XANIM_IS_LOOPED_BY_TREE_INDEX = 0x92). It receives ONE packed 32-bit
// animation id and unpacks it into the trap's two arguments: the high word becomes
// the tree index (trap arg2) and the low word becomes the animation index (trap
// arg3). It then issues cgame_syscall(0x92, packed >> 16, packed & 0xffff) and
// returns the trap's qboolean result unchanged.
//
// NAMING ADJUDICATION: the .mcode header carries the mechanical name
// "CG_ScoreboardDisplayed", assigned ONLY by a corpus size match
// (win size 0x1d ~ matched size 0x1c). That is a size guess and is REJECTED:
// the contract forbids identifying a function by size, and the body has no
// scoreboard behavior whatsoever -- it is a pure argument-unpacking trap thunk.
// The original engine dispatcher maps 0x92 to XAnimIsLooped after resolving the
// high-word tree index, and the same-module PPC binary names this wrapper exactly.
//
// PACKING / ARGUMENT ORDER (proven from the push order):
//   0x3003e960 MOVZX EAX, word [ESP+4]   ; EAX = packed & 0xffff  (low word)
//   0x3003e965 MOV   ECX, dword [ESP+4]  ; ECX = packed           (full dword)
//   0x3003e969 PUSH  EAX                 ; last cdecl arg (animIndex/low)
//   0x3003e96a SHR   ECX, 0x10           ; ECX = packed >> 16     (high word)
//   0x3003e96d PUSH  ECX                 ; middle cdecl arg (treeIndex/high)
//   0x3003e96e PUSH  0x92                ; first cdecl arg (trap id)
//   0x3003e973 CALL  [0x30085e9c]        ; cgame_syscall
//   0x3003e979 ADD   ESP, 0xc            ; caller-cleaned, 3 dwords
//   0x3003e97c RET
// Stack after the three pushes, low->high: [0x92][packed>>16][packed&0xffff],
// so the cdecl call is cgame_syscall(0x92, packed >> 16, packed & 0xffff).
// The return value is whatever cgame_syscall leaves in EAX (a qboolean per the
// trap's definition); it is passed straight through as this wrapper's int32
// result.
//
// CALLING CONVENTION: plain __cdecl. The sole argument is a single 32-bit stack
// dword at [ESP+4]; the callee cleans nothing (caller does ADD ESP,0xc after the
// inner call, and this wrapper itself uses a bare RET). Modeled as one uint32_t
// parameter, matching the sole caller's ABI. No prologue/epilogue, no locals.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>

qboolean trap_XAnimIsLooped(uint32_t packed)
{
    /*
     * treeIndex = high 16 bits (SHR ECX,0x10), animIndex = low 16 bits
     * (MOVZX EAX, word). Both are handed to the trap as 32-bit args in that
     * order after the id. The low word is zero-extended (MOVZX), so animIndex
     * carries no sign; the high word is produced by a logical shift.
     */
    int32_t treeIndex =
        (int32_t)(packed >> SCR_ANIM_TREE_INDEX_SHIFT);
    int32_t animIndex = (int32_t)(packed & 0xffffu);

    return (qboolean)cgame_syscall(CG_XANIM_IS_LOOPED_BY_TREE_INDEX,
                                   treeIndex, animIndex);
}
