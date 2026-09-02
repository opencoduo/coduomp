// Source: uo_cgame_mp_x86.dll 0x3001d380..0x3001d39b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001d380_3001d39b.mcode
//
// CG_ConfigString11Modified — the cgame "config string 11 changed" handler.
// Config string 11 (0x30440a2c == &cg_gameState.stringOffsets[11], i.e.
// stringOffsets[11]) is read as the byte offset of that config string into the
// packed character heap cg_gameState.stringData (0x30442a00); the resulting C
// string is atof'd and the float stored into cg_hudSpinBaseTime (0x3048b5c8),
// the base time consumed by CG_UpdateHudSpinAngle (0x3001d3a0) via FSUB.
//
// NAME REJECTED: the .mcode size-guess "BG_GetWeaponTypeName" is wrong. It was a
// pure size match (win size 0x1b == matched size 0x1b) and the 27-byte body does
// no weapon-name-table lookup: it loads one config-string offset, forms the
// config string pointer, calls atof, and FSTPs the float result. Named by proven
// role from its call graph — the sole caller CG_ConfigStringModified (0x30038e70)
// invokes it only when the changed config-string index (ESI) equals 11.
//
// Instruction evidence (every behavior-affecting statement):
//
//   3001d380  MOV EAX,[0x30440a2c]              ; EAX = cg_gameState.stringOffsets[11]
//   3001d385  LEA EAX,[EAX + 0x30442a00]        ; EAX = &cg_gameState.stringData[off]
//   3001d38b  PUSH EAX                          ; atof arg = config string 11
//   3001d38c  CALL 0x3005b969                   ; ST(0) = atof(configString)
//   3001d391  FSTP float ptr [0x3048b5c8]       ; cg_hudSpinBaseTime = (float)ST(0)
//   3001d397  ADD ESP,4                         ; caller-clean the 1 dword arg
//   3001d39a  RET
//
// atof (0x3005b969) is the statically linked CRT atof (whitespace-skipping
// string->double parse, returns ST(0)); its provisional caller-observed decl in
// client_recovered.h is superseded by its own .mcode reconstruction. The FSTP of
// a DWORD narrows the double return to a 32-bit float — cg_hudSpinBaseTime is a
// float, matching the FSUB float ptr consumer at 0x3001d3aa.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

void CG_ConfigString11Modified(void)
{
    /* Config string N = &cg_gameState.stringData[cg_gameState.stringOffsets[N]];
     * here N = 11. Parse it as a float (atof returns a double in ST(0)); the FSTP
     * of a DWORD narrows the result to the 32-bit float cg_hudSpinBaseTime. */
    cg_hudSpinBaseTime = (float)atof(&cg_gameState.stringData[cg_gameState.stringOffsets[11]]);
}
