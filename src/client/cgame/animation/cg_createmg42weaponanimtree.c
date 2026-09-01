// Source: uo_cgame_mp_x86.dll 0x3001e960..0x3001e9eb
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001e960_3001e9eb.mcode
//
// CG_CreateMG42WeaponAnimTree — build the "MG42" weapon XAnim tree for the
// weapon owned by the given effect slot and return its engine tree handle.
//
// Naming: the .mcode header name "ObjectiveStateIndexFromString" is a size-only
// guess (win size 0x8b matched a same-size server symbol) and is REJECTED. This
// function parses no string and returns no state index; it looks up a weaponInfo_t
// record and issues the XAnim tree-construction traps with the compiled-in
// literals "MG42" and "root", returning the created tree handle. Role name from
// that behavior (MG42 weapon + XAnim tree construction). The sole caller
// (0x30021f27) guards on the weapon type == 11 (MG42) and feeds the returned
// handle to CG_XANIM_CREATE_TREE (trap 134) to instantiate it.
//
// Argument ABI: the object pointer arrives in EAX (register argument; the caller
// forms EAX = 0x3048c6e0 + i*0x288 = &cg_effectSlots[i], a centity_t*). No
// stack arguments; two callee-saved registers (ESI, EDI) are preserved. RET with
// no immediate (caller/register-arg convention).

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

intptr_t CG_CreateMG42WeaponAnimTree(centity_t *slot)
{
    // 3001e960 MOV ECX,[EAX+0xcc]      -> weapon index held in the effect slot
    // 3001e966 MOV EDX,[0x30134cd8]    -> bg_weaponInfos base (weaponInfo_t**)
    // 3001e96d MOV ESI,[EDX+ECX*4]     -> weaponInfo = bg_weaponInfos[weaponIndex]
    weaponInfo_t *weaponInfo = bg_weaponInfos[slot->currentState.weapon];

    // 3001e971 PUSH 3 / PUSH "MG42" / PUSH 0x84 / CALL [0x30085e9c]
    //   trap(132, "MG42", 3) -> master tree handle (kept in EDI, returned below).
    intptr_t treeHandle =
        cgame_syscall(CG_XANIM_CREATE_ANIMS,
                      (intptr_t)bg_mg42WeaponName, 3);

    // 3001e983 PUSH 0 / PUSH 2 / PUSH 1 / PUSH "root" / PUSH 0 / PUSH EDI / PUSH 0x87 / CALL
    //   trap(135, treeHandle, 0, "root", 1, 2, 0) -> attach the root node (result discarded).
    cgame_syscall(CG_XANIM_BLEND, treeHandle, 0,
                  (intptr_t)bg_rootAnimationName, 1, 2, 0);

    // 3001e99e MOV EAX,[ESI+0x1c] / PUSH EAX / PUSH 0x83 / CALL
    //   trap(131, animBoneId0) (result discarded).
    cgame_syscall(CG_XANIM_PRECACHE, (intptr_t)weaponInfo->idleAnim);
    // 3001e9ad MOV ECX,[ESI+0x1c] / PUSH ECX / PUSH 1 / PUSH EDI / PUSH 0x85 / CALL
    //   trap(133, treeHandle, 1, animBoneId0).
    cgame_syscall(CG_XANIM_CREATE, treeHandle, 1,
                  (intptr_t)weaponInfo->idleAnim);

    // 3001e9bf MOV EDX,[ESI+0x24] / PUSH EDX / PUSH 0x83 / CALL
    //   trap(131, animBoneId1) (result discarded).
    cgame_syscall(CG_XANIM_PRECACHE, (intptr_t)weaponInfo->fireAnim);
    // 3001e9d1 MOV EAX,[ESI+0x24] / PUSH EAX / PUSH 2 / PUSH EDI / PUSH 0x85 / CALL
    //   trap(133, treeHandle, 2, animBoneId1).
    cgame_syscall(CG_XANIM_CREATE, treeHandle, 2,
                  (intptr_t)weaponInfo->fireAnim);

    // 3001e9e6 MOV EAX,EDI ; RET  -> return the created master tree handle.
    return treeHandle;
}
