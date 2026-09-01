// Source: uo_cgame_mp_x86.dll at the RVAs noted below.
// Evidence: corresponding cgame_mp/mcode/uo_cgame_mp_x86/FUN_*.mcode records.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <string.h>

void CG_SelectWeapon(int32_t weapon) /* 0x300474b0 */
{
    CG_SelectWeaponIndex(weapon, cg_weaponSelect_vmCvar.integer);
}

void *CG_RegisterDObjModel(const char *name) /* 0x30004d60 */
{
    enum { DOBJ_MODEL_CATEGORY = 7 };
    qhandle_t handle = CG_RegisterModel(name, DOBJ_MODEL_CATEGORY);
    return (void *)(intptr_t)cgame_syscall(CG_DOBJ_WRAP_MODEL, handle);
}

void CG_AddCEntityIfValid(centity_t *cent) /* 0x3001f6f0 */
{
    if (cent->currentState.eType < 16) {
        CG_CalcEntityLerpPositions(cent);
        CG_AddCEntity(cent);
    }
}

intptr_t CG_CreateTurretAnimTree(int32_t eType, int32_t entityNum) /* 0x30021e80 */
{
    if (eType != ET_TURRET) {
        return 0;
    }
    /* 0x30021e8c IMUL and 0x30021e92 ADD are target-dword effective-address
     * arithmetic. Use the native element stride while retaining the wrapped
     * Win32 displacement and avoiding C array-bound assumptions. */
    uint32_t offsetBits =
        (uint32_t)entityNum * (uint32_t)sizeof(cg_entities[0]);
    intptr_t displacement = (intptr_t)coduo_int32_from_bits(offsetBits);
    centity_t *entity = (centity_t *)(
        (uintptr_t)(void *)cg_entities + (uintptr_t)displacement);
    return CG_CreateMG42WeaponAnimTree(entity);
}

void CG_AddBufferedVoiceChat(const cgVoiceChatMsg_t *msg) /* 0x3003a130 */
{
    unsigned char *destination = (unsigned char *)&cgVoiceChatScratch;
    const unsigned char *source = (const unsigned char *)msg;

    /* REP MOVSD copies the target's 0x52 dwords in increasing-address order.
     * Snapshot each four-byte lane before its write so overlapping inputs keep
     * the instruction's granularity as well as its direction. Copying the
     * native logical extent retains the widened soundName pointer on 64-bit;
     * the i386 size assert proves the exact target 0x148. */
    size_t offset = 0;
    for (; offset + sizeof(uint32_t) <= sizeof(cgVoiceChatMsg_t);
         offset += sizeof(uint32_t)) {
        uint32_t word;
        memcpy(&word, source + offset, sizeof(word));
        memcpy(destination + offset, &word, sizeof(word));
    }
    for (; offset < sizeof(cgVoiceChatMsg_t); ++offset) {
        destination[offset] = source[offset];
    }
    CG_PlayVoiceChat(&cgVoiceChatScratch);
}
