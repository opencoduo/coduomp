// Source: uo_cgame_mp_x86.dll 0x3002c990..0x3002c9bd
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002c990_3002c9bd.mcode
//
// CG_ConfigString(int index) — return the config string for a gameState index.
//
// Naming: the mechanical pre-hint `GScr_AllocString` is a size-guess (win size
// 0x2d == corpus size 0x2d) and is REJECTED. This is not a server script
// string allocator: the machine code passes the exact format string
// "CG_ConfigString: bad index: %i" (cg_configStringBadIndexFmt @0x30077d90) to
// the cgame error emitter on an out-of-range index, then indexes the cgame
// gameState_t config-string tables. That is the id-Tech/CoD CG_ConfigString
// accessor. Name resolved by behavior + the referenced error string, not size.
//
// Machine-code facts (all proven against the .mcode / objdump):
//   PUSH ESI                          ; save ESI (callee-saved)
//   MOV  ESI,[ESP+8]                  ; ESI = index (first stack arg, after push)
//   TEST ESI,ESI / JL   error         ; signed: index < 0  -> error
//   CMP  ESI,0x800 / JL  lookup       ; signed: index < 2048 -> skip error
//   (fallthrough) error:
//     PUSH ESI; PUSH 0x30077d90; CALL 0x3002b300; ADD ESP,8   ; caller-cleaned
//   lookup:
//     MOV  EAX,[ESI*4 + 0x30440a00]   ; EAX = cg_gameState.stringOffsets[index]
//     ADD  EAX,0x30442a00             ; EAX = cg_gameState.stringData + offset
//   POP  ESI; RET                     ; cdecl, caller-cleaned (no RET imm)
//
// Note the bounds check only *reports* an out-of-range index via
// Com_ErrorMessage and then falls through to the (unbounded) table lookup —
// the ADD/MOV are reached on every path. This matches the original Q3/CoD
// CG_ConfigString, which relies on Com_Error to not return.
//
// 0x30440a00 = cg_gameState.stringOffsets[2048] (int32 byte-offsets),
// 0x30442a00 = cg_gameState.stringData (packed string heap); their 0x2000-byte
// separation proves MAX_CONFIGSTRINGS == 2048. The layout and dimensions come
// from the shared game_state_types.h boundary included by globals.h; the
// function declarations remain in client_recovered.h.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

const char *CG_ConfigString(int index)
{
    // TEST ESI,ESI / JL  ;  CMP ESI,0x800 / JL  — both compares are signed.
    // Out of [0, MAX_CONFIGSTRINGS) reports the bad index, then still proceeds
    // to the lookup (fallthrough), exactly as the machine code does.
    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        Com_ErrorMessage(cg_configStringBadIndexFmt, index);
    }

    // MOV EAX,[ESI*4 + 0x30440a00] ; ADD EAX,0x30442a00
    return &cg_gameState.stringData[cg_gameState.stringOffsets[index]];
}
