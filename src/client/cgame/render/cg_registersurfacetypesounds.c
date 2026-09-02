#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3002b4f0..0x3002b55d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002b4f0_3002b55d.mcode
//
// CG_RegisterSurfaceTypeSounds: register one sound-alias handle per surface type
// for a base asset name, filling a caller-provided handle table indexed by surface
// type. For each of the 23 surface types it asks the engine for that surface's
// name string (CG_SURFACE_TYPE_TO_NAME), composes "<baseName>_<surfaceName>" via sprintf,
// resolves the composed name (CG_COM_SOUND_ALIAS_STRING -> canonical name), and stores it in
// table[i].
//
// Call graph / behavior (this is how the name is proven, NOT size):
//   * The caller cluster at 0x3002b7a0.. invokes this once per base name, each
//     time setting EDI = a qhandle_t[23] table and EBX = a base .rdata name
//     string ("grenade_bounce" 0x30076e6c, "grenade_explode" 0x30076efc,
//     "rocket_explode" 0x30076ed4, "bullet_small" 0x3007864c, ...). The tables sit
//     at 0x3044bbd4/0x3044bc30/... with 0x5c-multiple strides (0x5c = 23*4).
//   * CG_SURFACE_TYPE_TO_NAME (0xc9) maps a surface-type index 0..22 to its name string; the
//     sibling site 0x3001dce2 guards the index with `!= 23` and feeds the result
//     into the "on surface type '%s'" diagnostic (0x30077104), proving the return
//     is a printable string used as a %s argument.
//   * CG_COM_SOUND_ALIAS_STRING (0xc3) resolves a name to its canonical alias string; the caller at
//     0x3002b560 registers dozens of plain names one per call, storing each handle
//     into its own global.
//   * The produced tables are read back at 0x30023257/0x30023417 indexed by a
//     surface-type field (entity+0x88) to pick the handle for an impact event.
//
// The .mcode's mechanical name G_EntDetachAll is a pure size match and is
// REJECTED: there is no entity/detach behavior; the body is a 23-iteration
// surface-asset registration loop. Size is not evidence.
// The Mac CG_RegisterSurfaceTypeSounds performs the corresponding surface-name to
// sound-alias registration loop, resolving the source name.
//
// ABI (proven from the caller at 0x3002b7a0 and this body): register-argument
// convention. EDI = table (the qhandle_t[23] destination; written only at
// [EDI + i*4]), EBX = baseName (pushed straight as the first "%s" arg to
// sprintf; never modified). There are no stack arguments (the function ends in
// a bare RET) — expressed here as ordinary C parameters.
//
// Structure (proven instruction-by-instruction):
//   0x3002b4f0 SUB ESP,0x104              reserve 0x100 scratch buffer + canary slot
//   0x3002b4f6 MOV EAX,[0x30081650]       \  MSVC /GS prologue: snapshot the
//   0x3002b4fb PUSH ESI                    |  __security_cookie into the canary slot
//   0x3002b4fc MOV [ESP+0x104],EAX        /  at frame+0x100 (after PUSH ESI)
//   0x3002b503 XOR ESI,ESI                i = 0
//   0x3002b505 JMP 0x3002b510             enter loop (no test; count-controlled)
//  loop (0x3002b510):
//   0x3002b510 PUSH ESI                    CG_SURFACE_TYPE_TO_NAME arg = i
//   0x3002b511 PUSH 0xc9                   command = CG_SURFACE_TYPE_TO_NAME
//   0x3002b516 CALL [cgame_syscall]        EAX = surfaceName(i)  (const char *)
//   0x3002b51c PUSH EAX                    sprintf 2nd "%s" = surfaceName
//   0x3002b51d PUSH EBX                    sprintf 1st "%s" = baseName
//   0x3002b51e LEA EAX,[ESP+0x14]          EAX = &buffer (scratch, frame+4)
//   0x3002b522 PUSH 0x3007884c            sprintf format = "%s_%s"
//   0x3002b527 PUSH EAX                    sprintf dest = buffer
//   0x3002b528 CALL 0x3005b89b            sprintf(buffer,"%s_%s",baseName,surfaceName)
//   0x3002b52d LEA ECX,[ESP+0x1c]          ECX = &buffer (same scratch, after 4 pushes)
//   0x3002b531 PUSH ECX                    CG_COM_SOUND_ALIAS_STRING arg = buffer
//   0x3002b532 PUSH 0xc3                   command = CG_COM_SOUND_ALIAS_STRING
//   0x3002b537 CALL [cgame_syscall]        EAX = registered handle
//   0x3002b53d ADD ESP,0x20               caller-clean the loop's 8 pushed dwords
//   0x3002b540 MOV [EDI+ESI*4],EAX         table[i] = handle
//   0x3002b543 INC ESI                     ++i
//   0x3002b544 CMP ESI,0x17               \  loop while i < 23 (signed JL)
//   0x3002b547 JL 0x3002b510              /
//   0x3002b549 MOV ECX,[ESP+0x104]         \  MSVC /GS epilogue: reload canary and
//   0x3002b550 POP ESI                      |  verify it via __security_check_cookie
//   0x3002b551 CALL 0x30061639            /
//   0x3002b556 ADD ESP,0x104              release the frame
//   0x3002b55c RET                        void, register args (nothing to clean)
//
// The SUB/MOV cookie snapshot and the reload+check epilogue are compiler-generated
// MSVC /GS stack-protector code (present because of the 0x100-byte stack buffer),
// not source statements; the original C body is just the registration loop below.
// The number of surface types (23) is the loop bound proven by CMP ESI,0x17 / JL.

void CG_RegisterSurfaceTypeSounds(const char **table, const char *baseName)
{
    // 0x3002b4f0: 0x100-byte scratch for the composed "<base>_<surface>" name.
    char assetName[256];

    // 0x3002b503..0x3002b547: for each surface type, resolve its name, compose the
    // per-surface asset name, register it, and store the returned handle.
    for (int32_t surfaceType = 0; surfaceType < SURFACE_TYPE_COUNT; ++surfaceType) {
        // 0x3002b510..0x3002b516: surfaceName = engine name string for this surface.
        const char *surfaceName =
            (const char *)(intptr_t)cgame_syscall(CG_SURFACE_TYPE_TO_NAME, surfaceType);

        // 0x3002b51c..0x3002b528: assetName = "<baseName>_<surfaceName>".
        sprintf(assetName, "%s_%s", baseName, surfaceName);

        // 0x3002b531..0x3002b540: register the composed name and record its handle.
        table[surfaceType] = trap_Com_SoundAliasString(assetName);
    }
}
