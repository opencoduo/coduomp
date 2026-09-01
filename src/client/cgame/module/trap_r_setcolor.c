#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>

// Source: uo_cgame_mp_x86.dll 0x3003e0d0..0x3003e0e1
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003e0d0_3003e0e1.mcode
//
// trap_R_SetColor: the cgame trap-72 wrapper for the engine's "set 2D draw color"
// service. It forwards its single 32-bit stack argument (a pointer to a float[4]
// r,g,b,a, or NULL to reset the color to opaque white) unchanged to cgame_syscall
// with command id CG_R_SETCOLOR (72 = 0x48). The service has no source-level
// result; EAX merely remains untouched after the syscall call.
//
// The .mcode's mechanical `# name trap_syscall_72` is a placeholder from the
// syscall-id alone; the trap-72 -> R_SetColor binding is proven by the HUD
// fill/soft-line draw cluster that brackets its draws with trap(72, color) then
// trap(72, NULL): CG_FillRect (0x3001c4e0) and the sibling line drawer at
// 0x3001ca20 (see the CG_R_SETCOLOR enum evidence in client_recovered.h). The
// same-module PPC bank lists cgame_mp!trap_R_SetColor with this one-pointer shape.
// This is the wrapper whose caller-observed provisional decl in client_recovered.h
// was awaiting reconstruction.
//
// Machine-code proof (args ESP-relative; no frame pointer). Let E be entry ESP,
// so [E+4] = rgba (the single 32-bit argument):
//   MOV EAX,[E+4]                 ; rgba
//   PUSH EAX                      ; syscall arg
//   PUSH 0x48                     ; command CG_R_SETCOLOR (72)
//   CALL [cgame_syscall]          ; *0x30085e9c
//   ADD ESP,8                     ; caller-cleaned the two pushed dwords (cdecl)
//   RET                           ; EAX remains the syscall result, but is unused
//
// The argument is forwarded as an opaque 32-bit word: the wrapper never
// dereferences it, so NULL and a float[4] pointer both pass through unchanged.

void trap_R_SetColor(const float *rgba)
{
    cgame_syscall(CG_R_SETCOLOR, (intptr_t)rgba);
}
