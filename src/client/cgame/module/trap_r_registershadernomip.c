// Source: uo_cgame_mp_x86.dll 0x3003dda0..0x3003ddbd
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003dda0_3003ddbd.mcode
//
// trap_R_RegisterShaderNoMip — the concrete function installed into
// displayContextDef_t slot +0x00 (DC->registerShaderNoMip). The UI shader-
// registration wrapper: it pumps the loading HUD once, then hands (name, loadMode)
// to the cgame shader-registration trap and returns the qhandle_t.
// The Mac trap_R_RegisterShaderNoMip has the same CG_DrawInformation loading-pump
// edge and is called by the corresponding asset, config-string, objective, graphics,
// and weapon registration paths, resolving the wrapper name.
//
// The .mcode size-guess "Com_GetCurrentParseLine" is a byte-size collision with
// no behavioral basis and is rejected — this body registers a 2D shader, it does
// no parser bookkeeping.
//
// Machine code / stack trace:
//   3003dda0  PUSH 0x0                     ; loading-pump arg for CG_DrawInformation
//   3003dda2  CALL 0x3002a530             ; CG_DrawInformation(0) — caller-cleaned,
//                                         ;   the PUSH 0 stays live on the stack
//   3003dda7  MOV EAX,[ESP+0xc]           ; EAX = arg2 (loadMode) (slot: 0 | ret | name | mode)
//   3003ddab  MOV ECX,[ESP+8]             ; ECX = arg1 (name)
//   3003ddaf  PUSH EAX                    ; push loadMode
//   3003ddb0  PUSH ECX                    ; push name
//   3003ddb1  PUSH 0x59                   ; trap id 0x59 == CG_R_REGISTERSHADER (89)
//   3003ddb3  CALL [0x30085e9c]           ; cgame_syscall(0x59, name, loadMode)
//   3003ddb9  ADD ESP,0x10                ; clean 4 pushed dwords (incl. the pump 0)
//   3003ddbc  RET
//
// Args (proven from the slot layout above): [ESP+4]=name (const char *),
// [ESP+8]=loadMode (int32_t). Return: cgame_syscall's int32_t result in EAX (a
// qhandle_t). Signature matches ui_registerShaderNoMip_t (client_recovered.h).
//
// Callees:
//   0x3002a530  CG_DrawInformation(qboolean force); called with qfalse as a loading pump,
//               its result is discarded here.
//   [0x30085e9c] cgame_syscall (globals.h) — the VM syscall vector; first vararg
//               id 0x59 = CG_R_REGISTERSHADER. The client forwards the caller's
//               `loadMode` as the trailing asset-load selector (a client divergence from
//               the single-arg Q3 signature; see ui_registerShaderNoMip_t note).

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

int32_t trap_R_RegisterShaderNoMip(const char *name, int32_t loadMode)
{
    /* 0x3003dda2: loading-HUD pump before registering the asset. */
    CG_DrawInformation(0);

    /* 0x3003ddb1: cgame_syscall(CG_R_REGISTERSHADER, name, loadMode) — returns the
     * registered shader's qhandle_t (int32_t). */
    return coduo_int32_from_bits((uint32_t)cgame_syscall(CG_R_REGISTERSHADER, name, loadMode));
}
