// Source: uo_cgame_mp_x86.dll 0x30038790..0x300387df
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30038790_300387df.mcode
// FUN_30038790_300387df  (0x30038790..0x300387df)  uo_cgame_mp_x86.dll
//
// CG_RegisterConfigStringMenu — resolve the config string at a gameState index
// and, when it is non-empty, load/register it as a script-menu file with the
// engine; report a failure to load.
//
// Adjacent sibling of CG_RegisterConfigStringShader (0x300387e0). Where that
// registrar issues CG_R_REGISTERSHADER for the shader config-string range, this one
// issues CG_R_REGISTERMENU (trap 0x7b) for the menu-file config-string range
// [0x535, 0x555): the config-string-modified dispatcher, after clamping the changed
// index to that band (CMP 0x535 / CMP 0x555 at 0x3003923b..0x30039249), loads it
// into EAX and calls here. The identical registrar at 0x30038826 (reached for a
// different dispatcher branch) issues the same trap + error string.
//
// Naming: the .mcode size-guess "Q_stricmp" (win size 0x4f == matched 0x50,
// broad-corpus name) is REJECTED — the body compares nothing; it reads a config
// string and issues a menu-load trap. Named CG_RegisterConfigStringMenu from that
// proven behavior (config-string lookup + CG_R_REGISTERMENU whose failure prints
// "Could not load script menu file '%s'"); exact CoD symbol unproven.
//
// Machine-code shape (index in ESI := EAX register arg):
//   30038790  PUSH ESI
//   30038791  MOV ESI,EAX                  ; ESI = index (register arg)
//   30038793  TEST ESI,ESI / JL            ; signed: index < 0  -> error
//   30038797  CMP ESI,0x800 / JL           ; index >= MAX_CONFIGSTRINGS -> error
//   3003879f  PUSH ESI                     ;   Com_ErrorMessage(fmt, index)
//   300387a0  PUSH 0x30077d90              ;   "CG_ConfigString: bad index: %i"
//   300387a5  CALL 0x3002b300              ;   Com_ErrorMessage (caller-cleaned)
//   300387aa  ADD ESP,0x8
//   300387ad  MOV ESI,[ESI*4 + 0x30440a00] ; ESI = cg_gameState.stringOffsets[index]
//   300387b4  ADD ESI,0x30442a00           ; ESI = &cg_gameState.stringData[offset]
//   300387ba  CMP byte ptr [ESI],0x0 / JZ  ; empty string -> skip registration
//   300387bf  PUSH ESI                     ; name = &cg_gameState.stringData[offset]
//   300387c0  PUSH 0x7b                    ; trap id 0x7b = CG_R_REGISTERMENU
//   300387c2  CALL [0x30085e9c]            ; cgame_syscall(0x7b, name) -> int ok
//   300387c8  ADD ESP,0x8                  ; clean 2 dwords (id + name)
//   300387cb  TEST EAX,EAX / JNZ           ; nonzero -> loaded ok, done
//   300387cf  PUSH ESI                     ; name
//   300387d0  PUSH 0x3007a340              ; "Could not load script menu file '%s'\n"
//   300387d5  CALL 0x3002b300              ; Com_ErrorMessage (caller-cleaned)
//   300387da  ADD ESP,0x8
//   300387dd  POP ESI / RET
//
// The bounds check mirrors the inlined CG_ConfigString one exactly: on an
// out-of-range index it prints "CG_ConfigString: bad index: %i" via
// Com_ErrorMessage and then FALLS THROUGH to the (now unbounded) offset lookup —
// preserved here as written (apparent original behavior, not a reconstruction fix).
// The trap's int return is used only to decide whether to report a load failure;
// the function returns void.
//
// ABI (i386, not source-level): index is a register arg in EAX (custom fastcall-
// style). Com_ErrorMessage and cgame_syscall are variadic/caller-cleaned.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

void CG_RegisterConfigStringMenu(int index)
{
    const char *name;

    /* 0x30038793..0x3003879d: reject a negative or >= MAX_CONFIGSTRINGS index. */
    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        /* 0x300387a5: shared inlined CG_ConfigString bad-index diagnostic. */
        Com_ErrorMessage(cg_configStringBadIndexFmt, index);
        /* Falls through to the lookup with the still-bad index (as in the DLL). */
    }

    /* 0x300387ad/0x300387b4: config string N = &stringData[offsets[N]]. */
    name = &cg_gameState.stringData[cg_gameState.stringOffsets[index]];

    /* 0x300387ba: only register a non-empty name. */
    if (name[0] != '\0') {
        /* 0x300387c2: cgame_syscall(CG_R_REGISTERMENU, name) -> nonzero on success.
         * 0x300387cb..0x300387d5: on a zero (failed) return, report the failure. */
        if (cgame_syscall(CG_R_REGISTERMENU, name) == 0) {
            Com_ErrorMessage(cg_couldNotLoadScriptMenuFmt, name);
        }
    }
}
