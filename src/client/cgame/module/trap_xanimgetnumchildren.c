// Source: uo_cgame_mp_x86.dll 0x3003ede0..0x3003edfd
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003ede0_3003edfd.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * trap_XAnimGetNumChildren — thin cdecl cgame trap wrapper for syscall id 0xb9 (185).
 *
 * It takes ONE packed 32-bit argument and forwards it to cgame_syscall (the VM
 * trap pointer at .data 0x30085e9c) as two 16-bit halves, returning the int32
 * result in EAX. Confirmed against the four call sites (0x30033101, 0x30033135,
 * 0x300333db, 0x3003340f in FUN_30032fe0_30033b68, and 0x30033d31, 0x30033da5 in
 * FUN_30033b70_300343d4): each pushes exactly one dword (PUSH EBP / PUSH EDI /
 * PUSH ESI) before the CALL — a single cdecl stack argument, caller-cleaned.
 *
 * Body (0x3003ede0..0x3003edfc):
 *   MOVZX EAX,word [ESP+4]   ; EAX = (uint16_t)packed        (low 16 bits, zero-ext)
 *   MOV   ECX,dword [ESP+4]  ; ECX = packed                  (full 32-bit arg)
 *   PUSH  EAX                ; syscall arg pushed first  -> becomes the LAST arg
 *   SHR   ECX,0x10           ; ECX = packed >> 16            (high 16 bits)
 *   PUSH  ECX                ; syscall arg pushed second -> becomes the FIRST arg
 *   PUSH  0xb9               ; command = 0xb9 (185)
 *   CALL  [0x30085e9c]       ; EAX = cgame_syscall(0xb9, packed>>16, (uint16_t)packed)
 *   ADD   ESP,0xc            ; drop the 3 pushed dwords
 *   RET                      ; plain RET -> the one incoming stack arg is caller-cleaned
 *
 * Because the stack grows downward, the last-pushed dword is the syscall's first
 * argument: cgame_syscall receives (0xb9, packed>>16, (uint16_t)packed). The high
 * half is passed full-width (SHR yields a 16-bit value zero-extended into a 32-bit
 * slot); the low half is MOVZX'd to (uint16_t) before its push.
 *
 * Naming note: the mechanical `.mcode` header guessed `G_GetHitLocationString`
 * purely by a size match (win 0x1d vs corpus 0x1c). That is rejected — size
 * matching is disallowed by the contract, and the body contradicts the guess: it
 * builds no string, touches no hit-location table, and is a pure 2-arg forwarder to
 * the cgame VM syscall pointer. CoDUOMP.exe's command-185 dispatcher arm
 * resolves the high-word tree handle and calls XAnimGetNumChildren with the
 * low-word animation index. The exact same-module Mac symbol is
 * trap_XAnimGetNumChildren.
 */
int32_t trap_XAnimGetNumChildren(uint32_t packed)
{
    return (int32_t)cgame_syscall(CG_XANIM_GET_NUM_CHILDREN, (int32_t)(packed >> SCR_ANIM_TREE_INDEX_SHIFT), (uint16_t)packed);
}
