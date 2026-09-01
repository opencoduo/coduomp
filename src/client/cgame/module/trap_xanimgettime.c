// Source: uo_cgame_mp_x86.dll 0x3003e9d0..0x3003e9ee
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003e9d0_3003e9ee.mcode

#include <string.h>

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * trap_XAnimGetTime — float-returning cgame trap wrapper for syscall id 0x95 (149).
 *
 * Calling convention (proven from the sole caller at 0x30003c6d..0x30003c7f):
 *   - arg1 arrives in ECX (register). Caller: MOV ECX,[ESP+0x10] before the CALL.
 *   - arg2 is a single 32-bit stack slot the caller pushes (MOV EAX,[ESP+0xc];
 *     PUSH EAX) and the CALLER cleans afterward (ADD ESP,0x4). The wrapper itself
 *     issues a plain RET (not RET imm), so the stack arg is caller-cleaned; ECX is
 *     an incoming register argument. This is a fastcall-style ECX + one cdecl
 *     stack arg.
 *   - the result is a float in ST(0): the caller does FSTP float ptr [ESP+0x24].
 *
 * Body (0x3003e9d0..0x3003e9ee):
 *   MOVZX EAX,word ptr [ESP+4]   ; narrow arg2 to its low 16 bits, zero-extended
 *   PUSH EAX                     ; cgame_syscall arg2 = (uint16_t)arg2
 *   PUSH ECX                     ; cgame_syscall arg1 = ECX (incoming reg arg)
 *   PUSH 0x95                    ; cgame_syscall command = 0x95 (149)
 *   CALL [0x30085e9c]            ; EAX = cgame_syscall(0x95, arg1, (uint16_t)arg2)
 *   MOV [ESP+0x10],EAX           ; overwrite the arg2 stack slot with the int result
 *   ADD ESP,0xc                  ; drop the 3 pushed dwords
 *   FLD float ptr [ESP+4]        ; load that same slot back as a 32-bit float
 *   RET                          ; return the syscall's int result reinterpreted
 *                                ; bit-for-bit as a float in ST(0)
 *
 * CoDUOMP.exe's syscall dispatcher at 0x00404027 calls XAnimGetTime
 * (0x0049aba0) for command 149. The same-module Mac client independently calls
 * trap_XAnimGetTime from this BG_SetNewAnimation path.
 *
 * Naming note: the mechanical `.mcode` header suggested `Scr_MakeArray` purely by a
 * PPC/server size match (win 0x1e vs 0x1f). That is rejected: Scr_MakeArray is a
 * no-argument, void server script builtin, whereas this is a fastcall, float-
 * returning cgame trap wrapper. Size matching is disallowed and the body
 * contradicts the guess, so the function is named trap_XAnimGetTime by its proven role.
 */
float trap_XAnimGetTime(XAnimTree *tree, uint16_t animIndex)
{
    /* EAX from the syscall is reinterpreted bit-for-bit as a float (the FLD reads
     * the same dword slot the int result was just stored into). */
    int32_t resultBits = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_XANIM_GET_TIME, (intptr_t)tree, animIndex));
    float result;

    memcpy(&result, &resultBits, sizeof(result));
    return result;
}
