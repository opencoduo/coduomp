// Source: uo_cgame_mp_x86.dll 0x3003eba0..0x3003ebbd
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003eba0_3003ebbd.mcode
//
// trap_XAnimGetAnimName -- a thin cdecl wrapper for the XAnim name query
// (CG_XANIM_GET_ANIM_NAME = 0xa4). It receives ONE packed 32-bit argument and
// unpacks it into the trap's two arguments: the high word becomes the tree handle
// (trap arg1) and the low word becomes the bone index (trap arg2). It issues
// cgame_syscall(0xa4, packed >> 16, packed & 0xffff) and returns the trap's
// result (a const char * animation-name string) unchanged.
//
// This is the argument-unpacking twin of trap_XAnimIsLooped (0x3003e960),
// differing only in the pushed trap id (0xa4 here vs 0x92 there). The trap id 0xa4
// is CG_XANIM_GET_ANIM_NAME in the original CoDUOMP.exe dispatcher: command 164
// resolves the high-word XAnim tree handle and calls XAnimGetAnimName with the
// low-word animation index. The exact same-module Mac symbol is
// trap_XAnimGetAnimName.
//
// NAMING ADJUDICATION: the .mcode header carries the mechanical name
// "compare_use", assigned ONLY by a corpus size match (win size 0x1d ~ matched
// size 0x1c, from game_mp.dll). That is a size guess and is REJECTED: the
// contract forbids identifying a function by size, and the body has no
// comparison behavior whatsoever -- it is a pure argument-unpacking trap thunk
// whose sole engine effect is cgame_syscall(0xa4, ...). The working name
// trap_XAnimGetAnimName is independently proven by the engine dispatcher and
// same-module Mac symbol/call graph.
//
// PACKING / ARGUMENT ORDER (proven from the push order, verified by objdump):
//   0x3003eba0 MOVZX EAX, word [ESP+4]   ; EAX = packed & 0xffff  (low word)
//   0x3003eba5 MOV   ECX, dword [ESP+4]  ; ECX = packed           (full dword)
//   0x3003eba9 PUSH  EAX                 ; last cdecl arg (arg2, boneIndex/low)
//   0x3003ebaa SHR   ECX, 0x10           ; ECX = packed >> 16     (high word)
//   0x3003ebad PUSH  ECX                 ; middle cdecl arg (arg1, treeHandle/high)
//   0x3003ebae PUSH  0xa4                ; first cdecl arg (trap id)
//   0x3003ebb3 CALL  [0x30085e9c]        ; cgame_syscall
//   0x3003ebb9 ADD   ESP, 0xc            ; caller-cleaned, 3 dwords
//   0x3003ebbc RET
// Stack after the three pushes, low->high: [0xa4][packed>>16][packed&0xffff],
// so the cdecl call is cgame_syscall(0xa4, packed >> 16, packed & 0xffff). The
// low word is zero-extended (MOVZX), so boneIndex carries no sign; the high word
// is produced by a logical shift (SHR).
//
// CALLING CONVENTION: plain __cdecl. The sole argument is a single 32-bit stack
// dword at [ESP+4]; this wrapper cleans nothing of its own (bare RET) and the
// caller cleans the inner call (ADD ESP,0xc). Modeled as one uint32_t parameter.
// No prologue/epilogue, no locals.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>

const char *trap_XAnimGetAnimName(uint32_t packed)
{
    /*
     * treeHandle = high 16 bits (SHR ECX,0x10), boneIndex = low 16 bits
     * (MOVZX EAX, word). Both are handed to the trap as 32-bit args in that
     * order after the id.
     */
    int32_t treeHandle =
        (int32_t)(packed >> SCR_ANIM_TREE_INDEX_SHIFT);
    int32_t animIndex  = (int32_t)(packed & 0xffffu);

    return (const char *)(intptr_t)cgame_syscall(CG_XANIM_GET_ANIM_NAME,
                                                 treeHandle, animIndex);
}
