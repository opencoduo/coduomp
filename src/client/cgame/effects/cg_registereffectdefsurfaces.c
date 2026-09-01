#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3001dcc0..0x3001dd2e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001dcc0_3001dd2e.mcode
//
// CG_RegisterEffectDefSurfaces: register the engine effect handles for every
// surface type of one effect type, returning the number of surface slots that were
// missing a definition (so the caller can report a total).
//
// Naming (by BEHAVIOR + CALL GRAPH, NOT size):
//   * The mechanical .mcode name `script_func_isplayer` is a pure size match
//     (win 0x6e == server 0x6e) and is REJECTED: there is no script/player logic
//     here at all. Per AGENTS.md, size is not evidence.
//   * The body is a 24-iteration loop over 64-byte entries (EDI stride 0x40) that
//     either registers each entry through cgame trap 0xe2 (CG_FX_REGISTER_EFFECT, the
//     register-effect-from-definition trap) storing the handle, or emits the
//     diagnostic "no entry for effect type '%s' on surface type '%s'\n"
//     (.rdata 0x30077104). The second '%s' is filled by cgame trap 0xc9
//     (CG_SURFACE_TYPE_TO_NAME), the surface-type-name lookup already proven by the sibling
//     CG_RegisterSurfaceTypeSounds (0x3002b4f0). This is effect/surface asset
//     registration, hence the role name.
//   * Caller (0x3001dfb0, loop at 0x3001e32a): for each of 22 effect types it sets
//     EAX = &defsTable (a 24*64 = 0x600-stride block), pushes handles[] and
//     effectTypeName, calls here, and accumulates our return (EBX += EAX). A
//     nonzero total then drives a summary print at 0x3001e35d. This proves our
//     return is the missing-entry count.
//
// ABI (register-argument, proven from the prologue and the caller push order):
//   EAX          = defs           (effectDef_t[24], walked at stride 0x40 via EDI)
//   [esp+4]      = effectTypeName (stack arg0; the first "%s")  <- caller EAX
//   [esp+8]      = handles        (stack arg1; qhandle_t[24] out) <- caller EDX
//   MOV EBP,[ESP+0x10] after PUSH EBX/PUSH EBP reads [entryESP+8] = handles, so
//   EBP is the output array; the printed effectTypeName is [entryESP+4], reached at
//   the print point as [ESP+0x20] (4 prologue pushes + 3 in-block pushes = 0x1c;
//   0x20-0x1c = +4). Bare RET (no callee stack cleanup) -> plain C parameters.
//
// Instruction-by-instruction proof:
//   3001dcc0 PUSH EBX / 3001dcc1 PUSH EBP                 save; EBP=handles base
//   3001dcc2 MOV EBP,[ESP+0x10]                           EBP = handles (arg1)
//   3001dcc6 PUSH ESI / 3001dcc7 PUSH EDI                 save
//   3001dcc8 XOR EBX,EBX                                  missing = 0 (return)
//   3001dcca XOR ESI,ESI                                  i = 0 (surface index)
//   3001dccc MOV EDI,EAX                                  EDI = &defs[0]
//   3001dcce MOV EDI,EDI                                  (2-byte NOP alignment)
//  loop head (0x3001dcd0):
//   3001dcd0 CMP byte ptr [EDI],0x0                       defs[i].name[0] == 0 ?
//   3001dcd3 JNZ 0x3001dd0b                               nonzero -> register path
//   3001dcd5 MOV AL,byte ptr [EDI+1]                      empty-name optional marker
//   3001dcd8 TEST AL,AL / 3001dcda JNZ 0x3001dd01         optional!=0 -> store 0, no warn
//   3001dcdc CMP ESI,0x17 / 3001dcdf JZ 0x3001dd01        i==23 -> store 0, no warn
//   3001dce1 PUSH ESI                                     CG_SURFACE_TYPE_TO_NAME arg = i
//   3001dce2 PUSH 0xc9                                    command = CG_SURFACE_TYPE_TO_NAME
//   3001dce7 CALL [cgame_syscall]                         EAX = surfaceName(i)
//   3001dced PUSH EAX                                     Com_PrintMessage 2nd %s
//   3001dcee MOV EAX,[ESP+0x20]                           = effectTypeName (arg0)
//   3001dcf2 PUSH EAX                                     Com_PrintMessage 1st %s
//   3001dcf3 PUSH 0x30077104                              format string
//   3001dcf8 CALL 0x3002b2b0                              Com_PrintMessage(fmt,...)
//   3001dcfd ADD ESP,0x14                                 pop 5 dwords (2 trap + 3 print)
//   3001dd00 INC EBX                                      ++missing
//  store-zero path (0x3001dd01):
//   3001dd01 MOV dword ptr [EBP+ESI*4],0x0                handles[i] = 0
//   3001dd09 JMP 0x3001dd1e                               -> loop step
//  register path (0x3001dd0b):
//   3001dd0b PUSH EDI                                     CG_FX_REGISTER_EFFECT arg = &defs[i]
//   3001dd0c PUSH 0xe2                                    command = CG_FX_REGISTER_EFFECT
//   3001dd11 CALL [cgame_syscall]                         EAX = registered handle
//   3001dd17 ADD ESP,0x8                                  pop 2 dwords
//   3001dd1a MOV dword ptr [EBP+ESI*4],EAX                handles[i] = handle
//  loop step (0x3001dd1e):
//   3001dd1e INC ESI                                      ++i
//   3001dd1f ADD EDI,0x40                                 advance one 64-byte entry
//   3001dd22 CMP ESI,0x18 / 3001dd25 JL 0x3001dcd0        while i < 24 (signed)
//   3001dd27 POP EDI / POP ESI / POP EBP                  restore
//   3001dd2a MOV EAX,EBX                                  return missing count
//   3001dd2c POP EBX / 3001dd2d RET
//
// The two surface-index limits are distinct: the last surface index whose absence
// is not warned about is CG_SURFACE_MAX_INDEX (23, the CMP ESI,0x17 guard), while
// the loop runs over CG_EFFECT_SURFACE_COUNT (24, the CMP ESI,0x18 bound). Both are
// exact loop/branch constants from the machine code.
enum {
    CG_EFFECT_SURFACE_COUNT = 24, /* CMP ESI,0x18 / JL: entries per effect type */
    CG_SURFACE_MAX_INDEX = 23     /* CMP ESI,0x17 / JZ: index exempt from the warning */
};

int32_t CG_RegisterEffectDefSurfaces(const effectDef_t *defs,
                                     const char *effectTypeName,
                                     qhandle_t *handles)
{
    int32_t missing = 0;

    for (int32_t i = 0; i < CG_EFFECT_SURFACE_COUNT; ++i) {
        if (defs[i].name[0] != 0) {
            // 0x3001dd0b: register this surface's effect definition, keep its handle.
            handles[i] = (qhandle_t)coduo_int32_from_bits((uint32_t)cgame_syscall(
                CG_FX_REGISTER_EFFECT, (intptr_t)&defs[i]));
        } else {
            // 0x3001dcd5..0x3001dd00: no definition. Warn unless the slot is marked
            // optional or is the last (exempt) surface index, then store handle 0.
            if (defs[i].name[1] == 0 && i != CG_SURFACE_MAX_INDEX) {
                const char *surfaceName =
                    (const char *)(intptr_t)cgame_syscall(CG_SURFACE_TYPE_TO_NAME, i);
                Com_PrintMessage(
                    "no entry for effect type '%s' on surface type '%s'\n",
                    effectTypeName, surfaceName);
                ++missing;
            }
            handles[i] = 0;
        }
    }

    return missing;
}
