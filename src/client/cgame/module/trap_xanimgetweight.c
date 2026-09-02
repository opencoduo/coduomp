// Source: uo_cgame_mp_x86.dll 0x3003e9f0..0x3003ea0e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003e9f0_3003ea0e.mcode

#include <string.h>

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * trap_XAnimGetWeight — float-returning cgame trap wrapper for syscall id 0x96 (150).
 *
 * This is the sibling of trap_XAnimGetTime (0x3003e9d0): byte-for-byte the same body,
 * differing only in the pushed command id (0x96 here vs 0x95 there). Id 0x96 is
 * CG_XANIM_GET_WEIGHT: CoDUOMP.exe's syscall dispatcher at 0x0040404d calls
 * XAnimGetWeight (0x0049abd0), and the same-module Mac client calls
 * trap_XAnimGetWeight from the corresponding vehicle/turret blend paths.
 *
 * Calling convention (proven from the call sites, e.g. 0x30033213..0x30033227):
 *   MOV ECX,[ESP+0x1c] / PUSH ECX  ; caller pushes one 32-bit stack arg (arg2)
 *   MOV ECX,ESI                    ; arg1 arrives in ECX (register)
 *   CALL 0x3003e9f0
 *   FSUB [ESP+0x1c]                ; the float in ST(0) is consumed directly
 *   ADD ESP,0x10                   ; the CALLER cleans the pushed stack arg
 *   -> fastcall-style: ECX register arg1 + one caller-cleaned 16-bit stack arg;
 *      result is a float in ST(0).
 *
 * Body (0x3003e9f0..0x3003ea0e):
 *   MOVZX EAX,word ptr [ESP+4]   ; narrow arg2 to its low 16 bits, zero-extended
 *   PUSH EAX                     ; cgame_syscall arg2 = (uint16_t)arg2
 *   PUSH ECX                     ; cgame_syscall arg1 = ECX (incoming reg arg)
 *   PUSH 0x96                    ; cgame_syscall command = CG_XANIM_GET_WEIGHT
 *   CALL [0x30085e9c]            ; EAX = cgame_syscall(0x96, arg1, (uint16_t)arg2)
 *   MOV [ESP+0x10],EAX           ; overwrite the arg2 stack slot with the int result
 *   ADD ESP,0xc                  ; drop the 3 pushed dwords
 *   FLD float ptr [ESP+4]        ; load that same slot back as a 32-bit float
 *   RET                          ; return the syscall's int result reinterpreted
 *                                ; bit-for-bit as a float in ST(0)
 *
 * Naming note: the mechanical `.mcode` header suggested `Scr_AddArray` purely by a
 * PPC/server size match (win 0x1e vs 0x1f). Rejected per the no-size-matching rule:
 * a script-array builtin is not a fastcall float-returning cgame trap wrapper, and
 * the body contradicts that guess.
 */
float trap_XAnimGetWeight(XAnimTree *tree, uint16_t animIndex)
{
    /* EAX from the syscall is reinterpreted bit-for-bit as a float (the FLD reads
     * the same dword slot the int result was just stored into). */
    int32_t resultBits = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_XANIM_GET_WEIGHT, (intptr_t)tree, animIndex));
    float result;

    memcpy(&result, &resultBits, sizeof(result));
    return result;
}
