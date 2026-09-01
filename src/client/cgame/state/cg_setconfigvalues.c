// Source: uo_cgame_mp_x86.dll 0x30038430..0x300384b7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30038430_300384b7.mcode
//
// CG_SetConfigValues — register the "config value" cvars from the gameState
// config-string table exactly once.
//
// Naming: the .mcode header guess PC_Rect_Parse (a broad-corpus size match,
// win size 0x87 vs 0x88) is REJECTED. The body has nothing to do with a
// rectangle parse: it loops over config strings and issues trap_Cvar_Set
// (cgame_syscall id 9) name/value pairs, guarded by an init-once flag. Named by
// that proven behavior; exact CoD symbol unproven.
//
// Machine-code shape:
//   - 0x30038430  guard: MOV EAX,[cgs_localServer]; TEST; JNZ ret.
//   - EDI is the value config-string index, initialized to 0x115 (277 =
//     CS_CONFIGVALUE_VALUES) and incremented each iteration.
//   - EBX is a walk cursor initialized to &cg_gameState.stringOffsets[277]
//     (0x30440e54) and stepped by 4 (one int32 offset slot) each iteration, so
//     [EBX]      == cg_gameState.stringOffsets[EDI]        (value cfg offset)
//     [EBX-0x200]== cg_gameState.stringOffsets[EDI-0x80]   (name  cfg offset)
//     (0x200/4 == 0x80 == 128 == CS_CONFIGVALUE_VALUES - CS_CONFIGVALUE_NAMES).
//   - Each config-string pointer is formed as
//     &cg_gameState.stringData[cg_gameState.stringOffsets[cfg]] (data base
//     0x30442a00), which is the inlined CG_ConfigString body, including its
//     out-of-range Com_ErrorMessage("CG_ConfigString: bad index: %i", cfg)
//     (bounds [0,0x800); the error is emitted but execution falls through).
//   - Stops at the first empty (leading-NUL) name string; otherwise runs the
//     full 128-entry range. Loop condition at 0x300384ab: continue while
//     (EDI - 0x115) < 0x80 (signed), i.e. up to CS_CONFIGVALUE_COUNT iterations.
//
// Callee-cleanup / register-arg details are i386 ABI, recorded here, not source.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

void CG_SetConfigValues(void)
{
    int valueIndex;

    /* 0x30038430: run-once guard. */
    if (cgs_localServer != 0) {
        return;
    }

    /* EDI starts at the value-range base; the name index trails it by 128. */
    for (valueIndex = CS_CONFIGVALUE_VALUES;
         /* 0x300384a5: LEA ECX,[EDI-0x115]; CMP ECX,0x80; JL loop (signed) */
         (valueIndex - CS_CONFIGVALUE_VALUES) < CS_CONFIGVALUE_COUNT;
         valueIndex++)
    {
        const char *name;
        const char *value;
        int nameIndex = valueIndex - CS_CONFIGVALUE_COUNT; /* EDI - 0x80 */

        /* Inlined CG_ConfigString(nameIndex): out-of-range reports the shared
         * error and still proceeds with the (unbounded) offset lookup.
         * 0x30038446..0x30038462 (signed compare, upper bound MAX_CONFIGSTRINGS). */
        if (nameIndex < 0 || nameIndex >= MAX_CONFIGSTRINGS) {
            Com_ErrorMessage(cg_configStringBadIndexFmt, nameIndex);
        }
        name = &cg_gameState.stringData[cg_gameState.stringOffsets[nameIndex]];

        /* 0x3003846e: stop at the first empty config-value name. */
        if (name[0] == '\0') {
            break;
        }

        /* Inlined CG_ConfigString(valueIndex). 0x30038473..0x3003848d. */
        if (valueIndex < 0 || valueIndex >= MAX_CONFIGSTRINGS) {
            Com_ErrorMessage(cg_configStringBadIndexFmt, valueIndex);
        }
        value = &cg_gameState.stringData[cg_gameState.stringOffsets[valueIndex]];

        /* 0x30038496: cgame_syscall(9, name, value) == trap_Cvar_Set(name,value). */
        cgame_syscall(CG_CVAR_SET, name, value);
    }
}
