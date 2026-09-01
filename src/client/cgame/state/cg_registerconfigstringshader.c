// FUN_300387e0_30038826  (0x300387e0..0x30038826)  uo_cgame_mp_x86.dll
//
// CG_RegisterConfigStringShader — resolve the config string at a gameState index
// and, when it is non-empty, register it as a 2D/HUD shader with the engine.
//
// Called from the config-string-modified dispatcher (0x300392dc) whenever a
// config string in the shader range CS_SHADERS..CS_SHADERS+CS_SHADERS_COUNT
// (0x675..0x775) changes: the changed index arrives in ECX and the string it
// now names is (re)registered so its qhandle stays current.
//
// Naming: the .mcode size-guess "Scr_FreeHudElem" (win size 0x46 == matched
// size 0x46, broad-corpus name) is REJECTED — the body frees nothing; it reads a
// config string and issues a shader-register trap. Nor is this CG_ConfigString
// itself (that is 0x3002c990, which merely returns the string pointer): this one
// shares CG_ConfigString's inlined bounds check + "bad index" error but then goes
// on to register the string as a shader. Named CG_RegisterConfigStringShader from
// that proven behavior (config-string lookup + CG_R_REGISTERSHADER); exact CoD
// symbol unproven.
//
// Machine-code shape (index in ESI := ECX register arg):
//   300387e0  PUSH ESI
//   300387e1  MOV ESI,ECX                  ; ESI = index (register arg)
//   300387e3  TEST ESI,ESI / JL            ; signed: index < 0  -> error
//   300387e7  CMP ESI,0x800 / JL           ; index >= MAX_CONFIGSTRINGS -> error
//   300387ef  PUSH ESI                     ;   Com_ErrorMessage(fmt, index)
//   300387f0  PUSH 0x30077d90              ;   "CG_ConfigString: bad index: %i"
//   300387f5  CALL 0x3002b300              ;   Com_ErrorMessage (caller-cleaned)
//   300387fa  ADD ESP,0x8
//   300387fd  MOV ESI,[ESI*4 + 0x30440a00] ; ESI = cg_gameState.stringOffsets[index]
//   30038804  ADD ESI,0x30442a00           ; ESI = &cg_gameState.stringData[offset]
//   3003880a  CMP byte ptr [ESI],0x0 / JZ  ; empty string -> skip registration
//   3003880f  PUSH 0x0                      ; CG_DrawInformation pump arg (stays live)
//   30038811  CALL 0x3002a530              ; CG_DrawInformation(0) (no-arg cleanup)
//   30038816  PUSH 0x5                      ; sort/NoMip flag = 5
//   30038818  PUSH ESI                      ; name = &cg_gameState.stringData[offset]
//   30038819  PUSH 0x59                     ; trap id 0x59 = CG_R_REGISTERSHADER
//   3003881b  CALL [0x30085e9c]             ; cgame_syscall(0x59, name, 5)
//   30038821  ADD ESP,0x10                  ; clean 4 dwords (incl. the pump 0)
//   30038824  POP ESI / RET
//
// The bounds check mirrors the inlined CG_ConfigString one exactly: on an
// out-of-range index it prints "CG_ConfigString: bad index: %i" via
// Com_ErrorMessage and then FALLS THROUGH to the (now unbounded) offset lookup —
// preserved here as written (apparent original behavior, not a reconstruction
// fix). The shader-register result (a qhandle_t) is discarded: this is a precache
// side effect, so the function returns void.
//
// ABI (i386, not source-level): index is a register arg in ECX (custom fastcall-
// style). Com_ErrorMessage and cgame_syscall are variadic/caller-cleaned. The
// PUSH 0 for CG_DrawInformation stays live on the stack and is reclaimed by the
// single ADD ESP,0x10 after the trap — same idiom as trap_R_RegisterShaderNoMip
// (0x3003dda0).

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

void CG_RegisterConfigStringShader(int index)
{
    const char *name;

    /* 0x300387e3..0x300387ed: reject a negative or >= MAX_CONFIGSTRINGS index. */
    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        /* 0x300387f5: shared inlined CG_ConfigString bad-index diagnostic. */
        Com_ErrorMessage(cg_configStringBadIndexFmt, index);
        /* Falls through to the lookup with the still-bad index (as in the DLL). */
    }

    /* 0x300387fd/0x30038804: config string N = &stringData[offsets[N]]. */
    name = &cg_gameState.stringData[cg_gameState.stringOffsets[index]];

    /* 0x3003880a: only register a non-empty name. */
    if (name[0] != '\0') {
        /* 0x30038811: loading-HUD pump before registering the asset. */
        CG_DrawInformation(0);
        /* 0x3003881b: cgame_syscall(CG_R_REGISTERSHADER, name, 5) — the returned
         * qhandle_t is discarded (precache side effect only). */
        cgame_syscall(CG_R_REGISTERSHADER, name, 5);
    }
}
