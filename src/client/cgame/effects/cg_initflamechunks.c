// Source: uo_cgame_mp_x86.dll 0x300279d0..0x30027aad
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300279d0_30027aad.mcode
//
// CG_InitFlameChunks — allocate the client flame-chunk pool and register the
// flamethrower's material and effect assets, then reset the pool to empty.
//
// The .mcode header's assigned name `BG_FindItem` is REJECTED: it was a pure size
// match (win size 0xdd == matched size 0xdd) with no behavioral basis. This function
// allocates a 0x2a0000-byte pool via cgame trap 0xc0, stores it in cg_flameChunks,
// calls CG_ClearFlameChunks, then registers the "flamethrowerFire%i"/"flameSmoke%i"
// materials and the two smoke .efx effects — it is flame-system initialization, not a
// bg_itemlist lookup. The role name CG_InitFlameChunks is proven by:
//   - the 0x2a0000-byte trap-0xc0 allocation stored in the flame pool base
//     cg_flameChunks (0x30134ce8) and the immediate CG_ClearFlameChunks (0x30025570)
//     call (0x2a0000 == FLAME_CHUNK_COUNT * FLAME_CHUNK_SIZE);
//   - the two material tables filled from "flamethrowerFire%i" (43 entries) and
//     "flameSmoke%i" (6 entries) and consumed by the flame sprite/fire renderers;
//   - the two "fx/smoke/smoke_flamethrower[_lg].efx" effect registrations.
//
// Calling convention: void(void). Standard i386 frame — SUB ESP,0x44 for the format
// scratch buffer plus locals; EBX/EBP/ESI/EDI callee-saved (pushed at entry, popped at
// exit); the /GS stack cookie is snapshotted on entry (MOV EAX,[__security_cookie];
// MOV [ESP+..],EAX) and verified on exit via __security_check_cookie (0x30061639).
// These are calling-convention/compiler details, recorded here and not modeled as
// source-level behavior.
//
// Machine-code notes:
//   0x300279dc PUSH 0x2a0000; PUSH 0xc0; CALL [cgame_syscall]  ->
//              cgame_syscall(CG_Z_MALLOC_INTERNAL /*0xc0*/, 0x2a0000); ADD ESP,8.
//   0x300279f3 MOV [cg_flameChunks], EAX                       store the pool base.
//   0x300279f8 CALL CG_ClearFlameChunks (0x30025570).
//   loop1 0x30027a00..0x30027a2f: EBX = 0..42 (CMP EBX,0x2b / JL):
//              Com_sprintf(buf, 64, "flamethrowerFire%i", EBX + 2);
//              cg_flameFireMaterials[EBX] = CG_RegisterMaterial(buf, 4);
//   loop2 0x30027a33..0x30027a63: EBX = 0..5 (via EBP = EBX+1; CMP EBX,6 / JL):
//              Com_sprintf(buf, 64, "flameSmoke%i", EBX + 1);
//              cg_flameSmokeMaterials[EBX] = CG_RegisterMaterial(buf, 4).
//   0x30027a65 PUSH "fx/smoke/smoke_flamethrower.efx"; PUSH 0xe2; CALL [cgame_syscall]
//              -> cg_flameSmokeEffect      = cgame_syscall(CG_FX_REGISTER_EFFECT, name).
//   0x30027a75 PUSH "fx/smoke/smoke_flamethrower_lg.efx"; PUSH 0xe2; CALL syscall
//              -> cg_flameSmokeEffectLarge = cgame_syscall(CG_FX_REGISTER_EFFECT, name).
//   0x30027a99 MOV [cg_flameInitStateReset], 0.
//   0x30027aa4 CALL __security_check_cookie; RET.
//
// The two INC/LEA index idioms (loop1 uses EBX directly, loop2 keeps EBP = EBX + 1)
// are compiler register bookkeeping; both loops iterate over EBX and index their table
// by EBX, so they are expressed as ordinary for-loops here.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_InitFlameChunks(void)
{
    char nameBuf[64];   /* 0x40-byte format scratch (Com_sprintf size ESI = 0x40) */
    int i;

    /* 0x300279dc: allocate the flame-chunk pool and store its base. The original
     * i386 request is 0x2a0000 == FLAME_CHUNK_COUNT * sizeof(flameChunk_t).
     * sizeof keeps that exact request on i386 and accommodates widened intrusive
     * list pointers in the native 64-bit record. */
    cg_flameChunks = (flameChunk_t *)(intptr_t)cgame_syscall(CG_Z_MALLOC_INTERNAL, (int)((size_t)FLAME_CHUNK_COUNT * sizeof(flameChunk_t)));

    /* 0x300279f8: thread the pool into the free list and clear the flame-info region. */
    CG_ClearFlameChunks();

    /* loop1 0x30027a00: register the flamethrower-fire sprite materials.
     * counter passed to the format is EBX + 2 (LEA EAX,[EBX+2]); loop bound EBX < 0x2b. */
    for (i = 0; i < 43; ++i) {
        Com_sprintf(nameBuf, 64, "flamethrowerFire%i", i + 2);
        cg_flameFireMaterials[i] = CG_RegisterMaterial(nameBuf, 4);
    }

    /* loop2 0x30027a33: register the flame-smoke sprite materials.
     * counter passed to the format is EBX + 1 (LEA EBP,[EBX+1]); loop bound EBX < 6. */
    for (i = 0; i < 6; ++i) {
        Com_sprintf(nameBuf, 64, "flameSmoke%i", i + 1);
        cg_flameSmokeMaterials[i] = CG_RegisterMaterial(nameBuf, 4);
    }

    /* 0x30027a65 / 0x30027a75: register the two flamethrower smoke effects.
     * Args pushed name then 0xe2, so the id is 0xe2 and the payload is the name. */
    cg_flameSmokeEffect = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, "fx/smoke/smoke_flamethrower.efx"));
    cg_flameSmokeEffectLarge = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, "fx/smoke/smoke_flamethrower_lg.efx"));

    /* 0x30027a99: MOV dword ptr [0x300851e0],0 — flame-subsystem dword reset to 0 as
     * the final init step (role provisional; single write, no reader recovered). */
    cg_flameInitStateReset = 0;
}
