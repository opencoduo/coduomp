// Source: uo_cgame_mp_x86.dll 0x3000fde0..0x3000fdf2
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3000fde0_3000fdf2.mcode
//
// CG_WeaponInfoSetString — the PARSE_FIELD_STRING_ALLOC field setter
// callback registered with ParseConfigStringToStruct (0x3004f1a0). It duplicates
// the parsed value string via CG_CopyString and stores the resulting pointer into
// the target weaponInfo_t string field.
//
// The .mcode header's assigned name `Think_SpawnNewDoorTrigger` is REJECTED. It is
// a pure size guess (`win size 0x12, matched size 0x12`); AGENTS.md forbids
// identifying a function by size. Nothing in the body touches doors, triggers, or a
// think function — it is a two-argument stack-to-register adapter over CG_CopyString.
//
// Role proven by call-graph:
//   - The address 0x3000fde0 is never CALLed directly. It is PUSHed as a function
//     pointer at 0x30010160, paired with the array setter 0x3000fad0, and handed to
//     ParseConfigStringToStruct (0x3004f1a0). That parser stores it as the string
//     setter and invokes it at the type-0 dispatch case (0x3004f1f4) with two cdecl
//     stack args: push value (the info-string value), push (weaponInfoBase +
//     field->offset) = dest, `call *setter`, then `add esp,8` — matching the
//     shared parse_config_copy_string_t callback shape
//     `void (*)(char *dest, const char *value)`.
//
// Behavior (the adapter): this converts that cdecl (dest, value) callback shape into
// the nonstandard register convention CG_CopyString (0x3000fd90) expects (src in
// EDI, out in EBX):
//   3000fde0 PUSH EBX                    ; save EBX
//   3000fde1 MOV EBX,[ESP+8]            ; EBX = arg0 = dest  -> CG_CopyString `out`
//   3000fde5 PUSH EDI                    ; save EDI
//   3000fde6 MOV EDI,[ESP+0x10]         ; EDI = arg1 = value -> CG_CopyString `src`
//   3000fdea CALL 0x3000fd90            ; CG_CopyString(value, (char**)dest)
//   3000fdef POP EDI / POP EBX / RET     ; restore, plain RET (cdecl, caller cleans)
// After the two PUSHes, [ESP+8] resolves to the first stack arg and [ESP+0x10] to the
// second (the return address sits at [ESP+0] once both are pushed). CG_CopyString
// duplicates the value into an engine allocation and writes the new pointer through
// its `out` param (== dest here), so *dest receives the copied string. EAX still holds
// the copy on return but the setter's callback contract is void — the return is
// discarded by the parser, so this is expressed as a void function.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* Matches shared parse_config_copy_string_t. */
void CG_WeaponInfoSetString(char *dest, const char *value)
{
    /* dest is a weaponInfo_t string field (holds a char*); CG_CopyString duplicates
     * `value` and writes the new pointer through its `out` parameter, so the copied
     * string pointer lands in *dest. The returned pointer (EAX) is discarded. */
    (void)CG_CopyString(value, (char **)dest);
}
