// Source: uo_cgame_mp_x86.dll 0x30035030..0x300350c2
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30035030_300350c2.mcode

#include "client/cgame/client_recovered.h"

#include <stddef.h>

/* Layout guards for the collision fields this builder reads, proven at the i386
 * target width against the machine code (CMP [EAX+0x4],3; TEST byte [EAX+0xfc],2;
 * CMP [EAX+0x194],0xffffff). These are fields of the canonical per-entity
 * effect slot (base 0x3048c6e0, stride 0x288 = 648); 0x194 is nextState.solid. */
_Static_assert(offsetof(centity_t, currentState.eType) == 0x04,
               "centity_t.eType at +0x04");
_Static_assert(offsetof(centity_t, nextState) + offsetof(entityState_t, eFlags) == 0xfc,
               "centity_t.nextState.eFlags at +0xfc");
_Static_assert(offsetof(centity_t, nextState) + offsetof(entityState_t, solid) == 0x194,
               "centity_t.nextState.solid at +0x194");
_Static_assert(sizeof(centity_t) == 0x288,
               "mark-source array stride 0x288 (IMUL EAX,EAX,0x288)");
_Static_assert(offsetof(snapshot_t, numEntities) == 0x4510,
               "cg_nextSnap->numEntities at +0x4510 ([EDI+0x4510])");
_Static_assert(offsetof(snapshot_t, entities) == 0x4518,
               "cg_nextSnap->entities at +0x4518 (LEA EDX,[EDI+0x4518])");
_Static_assert(sizeof(entityState_t) == 0xf4,
               "snapshot entity stride 0xf4 (ADD EDX,0xf4)");

/*
 * CG_BuildSolidList (0x30035030)
 *
 * Rebuild the per-frame solid and trigger lists from the incoming snapshot
 * (cg_nextSnap, 0x30459164). For every entity in the snapshot it looks up that
 * entity's centity record — element [entity->number] of the
 * centity_t array at 0x3048c6e0 (stride 0x288) and classifies it:
 *   - a SOLID_BMODEL record whose nextState eFlags non-solid bit is set is dropped;
 *   - otherwise, an ET_ITEM record is appended to cg_triggerEntities[];
 *   - otherwise, a record with nonzero nextState.solid is appended to
 *     cg_solidEntities[].
 * The two running indices become the list counts (cg_numTriggerEntities and
 * cg_numSolidEntities). Both counts are cleared up front and stored again on exit.
 *
 * Name: the .mcode size-guess "Concussive_fx" is REJECTED — this function issues
 * no effect/damage traps and does no concussion math. The replacement name is
 * confirmed by the same-module symbol-bearing Mac binary.
 *
 * ABI: no arguments (reads globals only); plain RET, EBX/EBP/ESI/EDI callee-saved.
 *
 * Machine-code facts proven for every behavior-affecting statement:
 *   30035034  EDI = cg_nextSnap        (MOV EDI,[0x30459164])
 *   3003503a  EAX = snap->numEntities  (MOV EAX,[EDI+0x4510])
 *   30035040  EBP = 0  (solidEntities index)
 *   30035042  EBX = 0  (triggerEntities index)
 *   30035044  ESI = 0  (entity loop counter i)
 *   30035048  cg_numSolidEntities = EBP(0)   (MOV [0x300dc08c],EBP)
 *   3003504e  cg_numTriggerEntities = EBX(0) (MOV [0x300dc088],EBX)
 *   30035054  JLE past loop when numEntities <= 0  (TEST EAX,EAX / JLE, signed)
 *   30035056  EDX = &snap->entities[0]  (LEA EDX,[EDI+0x4518])
 *   30035060  EAX = entities[i].number  (MOV EAX,[EDX]; first dword of entityState)
 *   30035062  EAX = number * 0x288
 *   30035068  EAX += 0x3048c6e0         => cent = &cg_entities[number]
 *   3003506d  ECX = cent->nextState.solid (MOV ECX,[EAX+0x194])
 *   30035073  CMP ECX,0xffffff ; JNZ .checkType
 *   3003507b  TEST byte [EAX+0xfc],0x2 ; JNZ .next
 *   30035084  .checkType: CMP [EAX+0x4],3 ; JNZ .checkSolid => eType == ET_ITEM?
 *   3003508a  cg_triggerEntities[EBX] = cent ; INC EBX ; JMP .next
 *   30035094  .checkSolid: TEST ECX,ECX ; JZ .next => nextState.solid == 0
 *   30035098  cg_solidEntities[EBP] = cent ; INC EBP
 *   300350a0  .next: EAX = snap->numEntities (reload) ; INC ESI ; EDX += 0xf4
 *   300350ad  CMP ESI,EAX ; JL loop  (signed)
 *   300350b1  cg_numSolidEntities = EBP ; cg_numTriggerEntities = EBX (final store)
 *   300350c1  RET
 *
 * The 0xffffff sentinel is SOLID_BMODEL, not a color.
 */
void CG_BuildSolidList(void)
{
    snapshot_t *snap = cg_nextSnap;

    /* The centity array begins at 0x3048c6e0. */
    centity_t *entitySource =
        cg_entities;

    int32_t solidCount = 0;   /* EBP: cg_solidEntities[] fill index */
    int32_t triggerCount = 0; /* EBX: cg_triggerEntities[] fill index */

    /* Cleared up front (30035048/3003504e) so an early-out leaves empty lists. */
    cg_numSolidEntities = solidCount;
    cg_numTriggerEntities = triggerCount;

    /* Signed loop bound: JLE / JL against snap->numEntities. */
    for (int32_t i = 0; i < snap->numEntities; i++) {
        centity_t *cent = &entitySource[snap->entities[i].number];

        uint32_t solid = cent->nextState.solid;

        /* Exclude inline models whose next state marks them non-solid. */
        if (solid == SOLID_BMODEL &&
            (cent->nextState.eFlags & EF_NONSOLID_BMODEL) != 0) {
            continue;
        }

        if (cent->currentState.eType == ET_ITEM) {
            cg_triggerEntities[triggerCount++] = cent;
        } else if (solid != 0) {
            cg_solidEntities[solidCount++] = cent;
        }
    }

    cg_numSolidEntities = solidCount;
    cg_numTriggerEntities = triggerCount;
}
