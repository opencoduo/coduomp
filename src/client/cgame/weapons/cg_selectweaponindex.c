#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x30047390..0x300473f6
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30047390_300473f6.mcode
//
// Role: CG_SelectWeaponIndex — commit a weapon selection by writing the
// "cg_weaponSelect" cvar to the requested weapon index, and, unless the request
// merely re-selects the currently-held weapon's alt-weapon, also assert the
// "cl_run" cvar. Called from the weapon-selection command cluster: the
// "weaponselect" console handler (0x3003ac90) and the next/prev/slot iterators
// (0x300475f0, 0x30047820, 0x300478a0, 0x30047960), each of which loads a weapon
// index from the per-slot byte table at 0x30483708 into EAX and the currently
// selected weapon index cg_weaponSelect_vmCvar.integer (0x3044034c) into ECX before the call.
//
// ABI (non-default register convention, proven from all five call sites, e.g.
// 0x3004788e MOVSX EAX,[..+0x30483708] then 0x30047885 MOV ECX,[0x3044034c]):
// the requested weapon index arrives in EAX and the current weapon index in ECX;
// there are no stack arguments and the callee ends in a plain RET (cdecl-clean of
// the callee's own pushed call args). Expressed here with EAX first / ECX second.
//
// Name adjudication: the .mcode header's assigned VectorPolar is a size-only
// match (win size 0x66) and is rejected — this function performs no vector math;
// it formats an integer and sets cvars. The behavior (take a weapon index, write
// it to cg_weaponSelect) matches the same-module PPC name CG_SelectWeaponIndex,
// which is adopted with the reservation that the id-to-name binding rests on
// call-graph/behavior, not a recovered symbol table.
//
// Struct: bg_weaponInfos[i] is a weaponInfo_t*; +0x36c is altWeapon (int32_t),
// corroborated against the recovered server weaponInfo_s (+0x36c altWeapon).
void CG_SelectWeaponIndex(int32_t weapon, int32_t currentWeapon)
{
    /* 0x30047392 MOV EDX,[0x304831b0] (cg_time) / 0x30047398 MOV
     * [0x3048ae44],EDX: snapshot the current game time as the weapon-(re)select
     * timestamp cg_weaponSelectTime, which the selected-weapon-name HUD overlay
     * (0x3002ec10 and siblings) reads as CG_FadeColor's startMsec to fade the
     * name out ~1800 ms after a switch. (Resolved during that overlay's
     * reconstruction; the mechanical owner=vectordistance2d label was a
     * first-touch artifact.) */
    cg_weaponSelectTime = cg_time;

    /* 0x30047390 CMP ECX,EAX / 0x3004739e JZ 0x300473f5: re-selecting the weapon
     * that is already current is a no-op. */
    if (currentWeapon == weapon) {
        return;
    }

    /* isAlt: is the requested weapon the alt-weapon of the currently-held one?
     * 0x300473a0 TEST EAX,EAX / 0x300473a3 JZ 0x300473bd: weapon 0 is never an
     * alt-weapon (skip the lookup, isAlt=0).
     * 0x300473a5 MOV EDX,[0x30134cd8] (bg_weaponInfos) / 0x300473ab MOV
     * ECX,[EDX+ECX*4]: fetch bg_weaponInfos[currentWeapon].
     * 0x300473ae CMP EAX,[ECX+0x36c] / 0x300473b4 JNZ 0x300473bd: isAlt is set
     * only when the request equals that weapon's altWeapon. */
    qboolean isAlt = qfalse;
    if (weapon != 0 && bg_weaponInfos[currentWeapon]->altWeapon == weapon) {
        isAlt = qtrue;
    }

    /* 0x300473bf..0x300473d8: trap_Cvar_Set("cg_weaponSelect", va("%i", weapon)).
     * va formats the requested index; the two pushed calls share one combined
     * `ADD ESP,0x14` cleanup (5 dwords: va's 2 args + the trap's id/name/value). */
    trap_Cvar_Set("cg_weaponSelect", va("%i", weapon));

    /* 0x300473db TEST ESI,ESI / 0x300473de JNZ 0x300473f5: only when this was
     * not an alt-weapon re-selection.
     * 0x300473e0..0x300473f2: trap_Cvar_Set("cl_run", "1"). */
    if (!isAlt) {
        trap_Cvar_Set("cl_run", "1");
    }
}
