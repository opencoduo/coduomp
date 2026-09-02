// Source: uo_cgame_mp_x86.dll 0x30044a80..0x30044ab3
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30044a80_30044ab3.mcode

#include "../client_recovered.h"
#include "../globals.h"

/*
 * CG_FreeWeaponDObjHandles (0x30044a80)
 *
 * Shutdown/reset helper: release the per-weapon DObj/model registration for every
 * registered weapon. For each weapon index i in 1..bg_numWeapons, ask the
 * engine to release the DObj registration keyed by (i + 0x400) via the release
 * trap CG_SAFE_CLIENT_DOBJ_FREE (0xa8), with flag arg 1. The 0x400 shift places the
 * weapon handles in a dedicated key band based at 0x400 (distinct from the
 * 0..1022 registration table freed by the Low/High sibling helpers). No table is
 * nulled here — this is a pure release loop.
 *
 * Called from the effect/HUD shutdown-reset path CG_ShutdownEffectsAndHud
 * (0x3002e390), alongside CG_FreeRegisteredHandlesLow/High.
 *
 * Count identity: the count global 0x30134cd4 is bg_numWeapons — the signed
 * registered-weapon count read across the corpus (BG_GetWeaponIndexForName,
 * CG_RefreshWeaponInfosForConfigString at the adjacent 0x30044a10, etc.), with
 * bg_weaponInfos[] at the adjacent 0x30134cd8. It is NOT a "cg_hudElemCount": the
 * earlier caller-observed header note that read it as a HUD-element count is
 * corrected here. The loop iterates 1..count inclusive (1-based weapon index),
 * matching bg_weaponInfos indexing.
 *
 * NAMING: the .mcode size-match guess "Scr_Init" (win size 0x33) is REJECTED —
 * there is no script VM, no init work; the body is a release-trap loop over the
 * registered weapons. Named by proven role (free the per-weapon DObj handle band)
 * and the proven count identity. Exact original engine/source symbol unproven (no
 * cgame syscall-id name table recovered); role-named from behavior + call graph.
 *
 * ABI: void(void), no arguments. Callee saves ESI (loop counter i); plain RET.
 * The three trap arguments (id, key, flag) are pushed and caller-cleaned per
 * iteration (ADD ESP,0xc), i.e. the cdecl cleanup for the *cgame_syscall variadic
 * trap call. bg_numWeapons is re-read each iteration (two 0x30134cd4 loads:
 * the pre-loop guard and the end-of-loop bound), and all compares are signed
 * (JL / JLE), so a count < 1 does nothing.
 *
 * Machine-code trace (0x30044a80..0x30044ab2):
 *   mov  eax,[bg_numWeapons]            ; count
 *   push esi                               ; save ESI
 *   mov  esi,1                             ; i = 1
 *   cmp  eax,esi ; jl exit                 ; if count < 1, return (signed)
 * loop (0x30044a90):
 *   push 1                                 ; flag
 *   lea  eax,[esi+0x400] ; push eax        ; key = i + 0x400
 *   push 0xa8            ; call *cgame_syscall   ; release trap
 *   mov  eax,[bg_numWeapons] ; add esp,0xc   ; reload count, cdecl cleanup
 *   inc  esi                               ; ++i
 *   cmp  esi,eax ; jle loop                ; while i <= count (signed)
 * exit (0x30044ab1): pop esi ; ret
 */

void CG_FreeWeaponDObjHandles(void)
{
    uint32_t indexBits = 1u;

    if (bg_numWeapons < 1)
        return;
    do {
        const int32_t key = coduo_int32_from_bits(indexBits + (uint32_t)CG_VIEW_WEAPON_DOBJ_HANDLE_BASE);
        /* 0x30044a90..0x30044aa3: PUSH 1; LEA EAX,[i+0x400]; PUSH EAX; PUSH 0xa8;
         * CALL [cgame_syscall]; ADD ESP,0xc. Release the DObj registration keyed
         * by (i + 0x400) for weapon i, with flag 1. */
        cgame_syscall(CG_SAFE_CLIENT_DOBJ_FREE, key, 1);
        indexBits += 1u;
    } while (coduo_int32_from_bits(indexBits) <= bg_numWeapons);
}
