#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x30022660..0x300226b9
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30022660_300226b9.mcode
//
// Role: CG_SetSelectedWeapon — commit an auto/initial weapon selection. Given a
// weapon index (in EAX), it latches the index and three cg.time snapshots, then
// — only when the index names a real weapon row of the HUD ammo-item table AND no
// weapon is currently selected — writes the "cg_weaponSelect" cvar to the weapon
// that row belongs to, via trap_Cvar_Set("cg_weaponSelect", va("%i", weapon)).
//
// Single caller (0x30022da1, in the spawn/HUD-event path): it loads the weapon
// index from [self+0xa4] into EBX, then `MOV EAX,EBX` before the plain CALL, so
// the argument arrives in EAX with no stack args and the callee ends in a plain
// RET. Expressed here as one `int32_t weaponIndex` parameter.
//
// Name adjudication: the .mcode header's assigned VectorDistance2D is a size-only
// guess (win size 0x59) and is REJECTED — this function contains no x87 at all
// (no FLD/FSUB/FMUL/FSQRT), does no vector math, and instead indexes bg_itemlist,
// tests a discriminant, and sets a cvar. The mechanical globals it first-touches
// (0x3048ae38/3c/40/44) carry that same wrong owner=vectordistance2d label. The
// behavior (latch a weapon index + time, gate on a valid weapon row and "nothing
// selected", set cg_weaponSelect) is a weapon-select commit; named
// CG_SetSelectedWeapon by proven role. Distinct from CG_SelectWeaponIndex
// (0x30047390), which sets the same cvar unconditionally from the requested index
// (no ammo-item/kind gate and no "cg_weaponSelect_vmCvar.integer==0" guard) and also asserts
// cl_run. The id-to-name binding rests on behavior/call-graph, not a symbol table.
//
// bg_itemlist[i] is a gitem_t (0x30 bytes): +0x20 type (itemType_t), +0x24
// weapon (the weapon index the row belongs to). The mechanical exporter
// split those two fields out as the bogus "vectordistance2d" globals 0x300827c0/
// 0x300827c4; they are element-0-relative field accesses into this one array.
void CG_SetSelectedWeapon(int32_t weaponIndex)
{
    /* 0x30022660 MOV ECX,[0x304831b0]: read the current game time (cg.time) once
     * into ECX; it is stamped into three snapshot slots below. */
    int32_t now = coduo_int32_from_bits(cg_time);

    /* 0x30022666 MOV [0x3048ae38],EAX: latch the requested weapon index
     * unconditionally (this happens before any gate). */
    cg_lastRequestedWeapon = weaponIndex;

    /* 0x3002266b LEA EAX,[EAX+EAX*2] / 0x3002266e SHL EAX,0x4: EAX = index*48,
     * the byte offset of bg_itemlist[weaponIndex] (sizeof(gitem_t)==0x30).
     * 0x30022671 CMP [EAX+0x300827c0],0x1: 0x300827c0 is bg_itemlist[0].type, so
     * with the *48 offset this reads bg_itemlist[weaponIndex].type, compared
     * against IT_WEAPON. */

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    gitem_t *item = &bg_itemlist[weaponIndex];
    /* 0x30022671 reads the type before either timer publication. */
    itemType_t itemType = item->type;

    /* 0x30022678 MOV [0x3048ae3c],ECX / 0x3002267e MOV [0x3048ae40],ECX: two
     * cg.time snapshots written unconditionally, alongside cg_weaponSelectTime
     * below. Write-only timers in this DLL (see globals.h). */
    cg_weaponSelectTimeA = now;
    cg_weaponSelectTimeB = now;

    /* 0x30022684 JNZ 0x300226b8: bail unless this row is a real weapon row. */
    if (itemType != IT_WEAPON) {
        return;
    }

    /* 0x30022686 MOV EDX,[0x3044034c] / 0x3002268c TEST EDX,EDX /
     * 0x30022694 JNZ 0x300226b8: bail if a weapon is already selected; this path
     * only applies the selection when cg_weaponSelect_vmCvar.integer == 0.
     * 0x3002268e MOV EAX,[EAX+0x300827c4]: read bg_itemlist[weaponIndex].weapon
     * (the weapon index the row belongs to) as the value to publish. */
    int32_t currentSelection = cg_weaponSelect_vmCvar.integer;
    /* 0x3002268e reads the item weapon before branching on currentSelection. */
    int32_t selectWeapon = item->weapon;
    if (currentSelection != 0) {
        return;
    }

    /* 0x3002269c MOV [0x3048ae44],ECX: stamp cg_weaponSelectTime = cg.time only on
     * the committing path (drives the on-screen weapon-name overlay fade). */
    cg_weaponSelectTime = now;

    /* 0x30022696..0x300226b5: trap_Cvar_Set("cg_weaponSelect", va("%i", weapon)).
     * va (0x3004e8a0) formats the weapon index; the two pushed calls share one
     * combined `ADD ESP,0x14` cleanup (va's 2 args + the trap's id/name/value). */
    trap_Cvar_Set("cg_weaponSelect", va("%i", selectWeapon));
}
