#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x30023fd0..0x30024049
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30023fd0_30024049.mcode
//
// CG_StartFlameDamageEffect (provisional name) — mark a client as taking flame
// damage and ignite the visible burning effect on its model. Called with the
// client number in ECX (__fastcall-style, single register argument, no stack
// args; the function balances ESP by hand and ends with a plain RET).
//
// Proven behavior:
//   * cg_flameInfo[clientNum].damageActive = 1  (0x3002400c:
//       MOV dword ptr [EDX + 0x300ab7f4],0x1  with EDX = clientNum*0xb8). This is
//     the per-client flame-damage flag consumed by the pain-effect updater
//     FUN_300291c0 (which reads slot +0x04/+0x08 against cg_time and plays the
//     "fdc %i" sound).
//   * For each of the four limb bones "Bip01 L Hand", "Bip01 R Hand",
//     "Bip01 L Calf", "Bip01 R Calf" (a NUL-terminated local pointer array), spawn
//     a flame chunk attached to that bone of this client's effect slot:
//       CG_SpawnFlameChunkOnBone(&cg_effectSlots[clientNum], NULL, boneName,
//                                3500, 60.0f, 20).
//     The slot base is 0x3048c6e0 (stride 0x288 = 648; ECX*0x288 + 0x3048c6e0);
//     see centity_t. `pos` is NULL so the chunk uses the bone tag; 3500 is
//     the chunk lifetime in ms, 60.0f a speed/scale, 20 a count.
//
// The .mcode size-matched guess "Scr_GetClientField" is REJECTED: that server
// routine is a script-VM field getter; this function ignites limb flame chunks
// (bone-name .rdata strings + the flame-chunk spawner + the per-client
// flame-damage flag). The name was a pure size match (win 0x79), which the
// project rules forbid.
//
// Machine-code self-check: both array indices are clientNum scaled by 0xb8 (flag
// array) and 0x288 (centity array) with the same input register ECX; the flag
// store is an unconditional dword=1 before the loop; the loop is a NUL-terminated
// forward walk of a 4-element bone-pointer array (5 dwords incl. terminator,
// SUB ESP,0x14), passing each bone with the fixed (NULL,name,3500,60.0f,20)
// argument tuple; TEST EAX,EAX / JNZ continues while the next pointer is non-NULL.
void CG_StartFlameDamageEffect(int32_t clientNum)
{
    // 0x30024005 area: local NUL-terminated array of the four limb bone names.
    // The order (L Hand, R Hand, L Calf, R Calf) matches the stored pointers at
    // [ESP+0x8..0x14]; [ESP+0x18] is the NULL terminator.
    const char *const limbBoneNames[] = {
        "Bip01 L Hand",   // 0x300777a8
        "Bip01 R Hand",   // 0x30077798
        "Bip01 L Calf",   // 0x30077788
        "Bip01 R Calf",   // 0x30077778
        NULL,
    };

    // 0x3002400c: cg_flameInfo[clientNum].damageActive = 1 (EDX = clientNum*0xb8).
    cg_flameInfo[clientNum].damageActive = 1;

    // 0x30024016: EDI = &cg_effectSlots[clientNum] (ECX*0x288 + 0x3048c6e0). The
    // Index the typed cg_entities[] base directly by client number.
    // The spawner reaches the entity's +0x1e8 state field through self+0x1e8
    // (see centity_t / CG_SpawnFlameChunkOnBone in the header).
    centity_t *slot = cg_entities + clientNum;

    // 0x30024020..0x30024041: for each limb bone, spawn a flame chunk on it.
    for (const char *const *name = limbBoneNames; *name != NULL; ++name) {
        CG_SpawnFlameChunkOnBone(slot, NULL, *name,
                                 3500,     // 0x30024027 PUSH 0xdac  (lifetime, ms)
                                 60.0f,    // 0x30024022 PUSH 0x42700000 (speed/scale)
                                 20);      // 0x30024020 PUSH 0x14  (count)
    }
}
