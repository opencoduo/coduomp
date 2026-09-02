// Source: uo_cgame_mp_x86.dll 0x3003f2d0..0x3003f2eb
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003f2d0_3003f2eb.mcode
//
// trap_FX_SetWind — thin cgame trap wrapper for syscall id 0xf1 (241). It forwards two
// arguments to *cgame_syscall: a vec3 pointer (received in EDX) and a float's raw
// bits (received on the stack), i.e. cgame_syscall(0xf1, direction, intensityBits), returning
// the trap's int32 result unchanged.
//
// Body (0x3003f2d0..0x3003f2eb):
//   MOV EAX,[ESP+0x4]     ; EAX = intensity bits (sole incoming stack dword)
//   MOV ECX,EAX           ; ECX = intensity bits (copy for the push)
//   PUSH ECX              ; cgame_syscall arg2 = intensity bits
//   PUSH EDX              ; cgame_syscall arg1 = direction
//   PUSH 0xf1             ; cgame_syscall command = 0xf1 (241)   (lowest of the pushes)
//   MOV [ESP+0x10],EAX    ; dead store: after the 3 pushes [ESP+0x10] is the original
//                         ;   arg slot [ESP_entry+4]; writing `code` back into its own
//                         ;   slot has no effect (register-scheduling artifact — the
//                         ;   value is never reloaded, unlike the float-returning
//                         ;   siblings that FLD this slot back). Not modeled in C.
//   CALL [0x30085e9c]     ; EAX = cgame_syscall(0xf1,direction,intensityBits)
//   ADD ESP,0xc           ; caller-cleaned, 3 dwords
//   RET                   ; plain RET — the single stack arg is caller-cleaned cdecl,
//                         ;   EDX is a register argument; EAX result passed through.
//
// Stack after the three pushes, low->high: [0xf1][direction][intensityBits].
//
// ARGUMENT SHAPE / CALLING CONVENTION — proven from the sole caller (0x3003935d):
//   MOV EDX,[ESP+0x20]    ; fourth parsed float's raw bits
//   PUSH EDX              ; -> wrapper stack arg (intensity)
//   LEA EDX,[ESP+0x28]    ; EDX = &first parsed float; sscanf (0x3005bf24) just
//                         ;   parsed "%f %f %f %f" into four float slots
//                         ;   (0x3005bf24 is CRT sscanf, not sprintf)
//   CALL 0x3003f2d0       ; trap_FX_SetWind(&floats[0], floats[3])
//   ADD ESP,0x1c          ; caller cleans (includes the pushed float dword)
// Thus arg1 is a vec3 pointer and arg2 is the fourth parsed float.
// This is a fastcall-style EDX register arg + one cdecl stack arg; the register arg is
// modeled first to match the cgame_syscall push order.
//
// CoDUOMP.exe's syscall dispatcher maps command 241 to FX_SetWind, taking the
// direction pointer and the second argument's raw float word. The exact
// same-module Mac symbol and CG_ConfigStringModified call edge are
// trap_FX_SetWind.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>
#include <string.h>

int32_t trap_FX_SetWind(const vec3_t direction, float intensity)
{
    uint32_t intensityBits;

    memcpy(&intensityBits, &intensity, sizeof(intensityBits));
    return coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_SET_WIND, (intptr_t)direction, coduo_int32_from_bits(intensityBits)));
}
