// Source: uo_cgame_mp_x86.dll 0x3003ee00..0x3003ee27
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003ee00_3003ee27.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * trap_XAnimGetChildAt — thin cgame trap wrapper for syscall id 0xba (186).
 *
 * It forwards a packed 32-bit stack argument plus a register argument (EAX) to
 * cgame_syscall (the VM trap pointer at .data 0x30085e9c) as three int args:
 * cgame_syscall(0xba, packed>>16, (uint16_t)packed, eax). The 16-bit syscall
 * result (AX) is spliced back over the low 16 bits of the original packed value
 * and the reconstituted 32-bit dword is returned.
 *
 * Calling convention: one incoming cdecl stack dword at [ESP+4] (the packed
 * value) plus one register argument in EAX. Proven from the call sites in
 * FUN_30032fe0 and FUN_30033b70 (e.g. 0x3003312d, 0x30033165, 0x300331f4): each
 * computes EAX = arithmetic-halved value, then `PUSH <regvalue>; CALL 0x3003ee00`,
 * and cleans the pushed stack dword itself (the wrapper issues a plain RET, so the
 * stack arg is caller-cleaned) — hence EAX is a register argument, not a spill.
 *
 * Body (0x3003ee00..0x3003ee26):
 *   MOVZX ECX,word  [ESP+4]  ; ECX = (uint16_t)packed        (low 16 bits, zero-ext)
 *   MOV   EDX,dword [ESP+4]  ; EDX = packed                  (full 32-bit stack arg)
 *   PUSH  EAX                ; syscall arg pushed first  -> becomes the LAST arg
 *   PUSH  ECX                ; low word                  ->            3rd arg
 *   SHR   EDX,0x10           ; EDX = packed >> 16            (high 16 bits)
 *   PUSH  EDX                ; high word                 ->            2nd arg
 *   PUSH  0xba               ; command id 0xba (186)     ->            1st arg
 *   CALL  [0x30085e9c]       ; EAX = cgame_syscall(0xba, packed>>16, (uint16_t)packed, eax)
 *   MOV   word [ESP+0x14],AX ; splice AX over low 16 bits of the packed slot.
 *                            ; after 4 pushes, [ESP+0x14] == original [ESP+4].
 *   MOV   EAX,dword [ESP+0x14]; reload full 32-bit dword (hi half = packed>>16 << 16,
 *                            ; lo half = AX)
 *   ADD   ESP,0x10           ; drop the 4 pushed dwords
 *   RET                      ; plain RET -> the single incoming stack dword is caller-cleaned
 *
 * Because the stack grows downward, the last-pushed dword becomes the syscall's
 * first argument. The high half is passed full-width (SHR yields a 16-bit value
 * zero-extended into a 32-bit slot); the low half is MOVZX'd to (uint16_t) before
 * its push. The 16-bit return (AX) only overwrites the low word of the packed
 * value, so the high 16 bits of the returned int are the original packed>>16 bits.
 *
 * Naming note: the mechanical `.mcode` header guessed `Script_SetAsset` by a size
 * match (win 0x27 vs corpus 0x28). That is rejected — size matching is disallowed
 * by the contract, and the body contradicts the guess: it parses no asset, touches
 * no script/menu state, and is a pure forwarder to the cgame VM syscall pointer.
 * CoDUOMP.exe's command-186 dispatcher arm resolves the high-word tree handle
 * and calls XAnimGetChildAt with the low-word animation index and EAX ordinal.
 * The exact same-module Mac symbol is trap_XAnimGetChildAt.
 */
int32_t trap_XAnimGetChildAt(uint32_t packed, int32_t extra)
{
    /* AX (16-bit syscall result) is spliced into the low word of `packed`; the
     * high word of the returned value keeps packed>>16 shifted back up. */
    uint16_t lo = (uint16_t)cgame_syscall(CG_XANIM_GET_CHILD_AT,
                                          (int32_t)(packed >> SCR_ANIM_TREE_INDEX_SHIFT),
                                          (uint16_t)packed,
                                          extra);
    return coduo_int32_from_bits((packed & 0xffff0000u) | (uint32_t)lo);
}
