#include "../client_recovered.h"

#include <string.h>

// Source: uo_cgame_mp_x86.dll 0x3004fd00..0x3004fd4e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3004fd00_3004fd4e.mcode
//
// cgame vmMain command 18 (dispatched from vmMain 0x3002af00 via the jump table
// at 0x3002b148, entry index 18 = 0x3002b107; the handler calls this with
// arg0 = [EBP+0xc] and returns EAX to the engine).
//
// Behavior, byte for byte:
//   0x3004fd00  PUSH ESI; MOV ESI,EAX; TEST ESI,ESI; JZ 0x3004fd15
//       -> imports arrives in EAX; if NULL, skip the copy.
//   0x3004fd07  PUSH EDI; MOV ECX,0x66; MOV EDI,0x300f08f8; REP MOVSD; POP EDI
//       -> copy 0x66 (102) dwords = 408 bytes from imports into the global
//          script-import table cg_scriptImports (base 0x300f08f8). The table runs
//          up to 0x300f0a90 (the next symbol), exactly 102 * 4 bytes.
//   0x3004fd15..0x3004fd47  five MOV dword [0x300f08e0+4*i], <stub>
//       -> seed Scr_GetFunction, Scr_GetMethod, Scr_SetObjectField,
//          Scr_GetObjectField, and Scr_LoadRead. The first, second, and fifth
//          return NULL; the other two are no-ops. These stores are unconditional.
//          The unrelated four-byte storage gap at 0x300f08f4 is not returned.
//   0x3004fd47  MOV EAX,0x300f08e0; POP ESI; RET
//       -> return &cg_scriptExports.
//
// Scr_FarHook is the exact same-module Mac symbol and matches the independently
// reconstructed server/engine script callback handshake.
cg_scriptExportTable_t *Scr_FarHook(const cg_scriptImportTable_t *imports)
{
    if (imports != NULL) {
        unsigned char *destination = (unsigned char *)&cg_scriptImports;
        const unsigned char *source = (const unsigned char *)imports;
        unsigned char slotBytes[sizeof(uintptr_t)];

        /* REP MOVSD loads and stores one four-byte pointer slot at a time toward
         * increasing addresses. Preserve that per-slot read-before-write rule
         * for overlap, widening the unit to one native pointer slot on 64-bit
         * builds so all 102 logical imports survive pointer expansion. */
        for (size_t offset = 0;
             offset < sizeof(cg_scriptImports);
             offset += sizeof(slotBytes)) {
            for (size_t byte = 0; byte < sizeof(slotBytes); ++byte)
                slotBytes[byte] = source[offset + byte];
            for (size_t byte = 0; byte < sizeof(slotBytes); ++byte)
                destination[offset + byte] = slotBytes[byte];
        }
    }

    cg_scriptExports.getFunction = Scr_GetFunction;
    cg_scriptExports.getMethod = Scr_GetMethod;
    cg_scriptExports.setObjectField = Scr_SetObjectField;
    cg_scriptExports.getObjectField = Scr_GetObjectField;
    cg_scriptExports.loadRead = Scr_LoadRead;

    return &cg_scriptExports;
}
