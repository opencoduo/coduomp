// Source: uo_cgame_mp_x86.dll 0x30022170..0x30022228
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30022170_30022228.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_AddCEntity — add one client entity (centity_t) to the current render/sound
 * frame, dispatching on its currentState.eType. This is the canonical Quake3/CoD
 * per-entity switch: run the shared per-entity setup, then route the entity to
 * the eType-specific handler (general model, player, item, missile, mover, ...),
 * with an out-of-range diagnostic for a bad eType.
 *
 * Rejected .mcode header name `BG_SetupTransitionTimes`: pure size guess (win
 * size 0xb8 == matched size 0xb8, per the .mcode name_evidence) and contradicted
 * by the machine code, which does no transition-time / player-state work. The
 * function is proven to be CG_AddCEntity by:
 *   - a jump table over currentState.eType at +0x4 (CODUO_HOST_ENTITY_STATE_
 *     ETYPE_OFFSET = 0x004; the entity object is the stride-0x288
 *     centity_t whose currentState is at +0);
 *   - an unsigned range gate (CMP eType,0xf; JA -> default) and a 16-entry table
 *     at 0x30022228, the classic switch(eType) fan-out;
 *   - the default diagnostic "Bad entity type: %i\n" (global 0x30077138) issued
 *     via Com_ErrorMessage — an entity-type-specific message that nails the id.
 *
 * ABI: cent is passed in EAX (register convention). The prologue PUSH ECX is a
 * scratch/alignment slot (balanced by the epilogue POP ECX); no stack local is
 * used. EBX holds cent across the body. Each case tail-calls its handler in the
 * register the handler expects (EAX / ECX / ESI), or pushes cent as a stack arg
 * for the three cdecl handlers (ADD ESP,0x4 cleanup), then falls through to the
 * common POP ESI / POP EBX / POP ECX / RET epilogue. No return value.
 *
 * Instruction-level proof:
 *   30022170  PUSH ECX/EBX/ESI ; MOV EBX,EAX       cent = EAX (kept in EBX)
 *   30022175  CALL 0x3001e7f0                       CG_CalcEntityLerpOrigin(cent) (EAX=cent)
 *   3002217a  MOV EAX,[EBX+0x4]                     eType = cent->currentState.eType
 *   3002217d  CMP EAX,0xf ; JA 0x30022216           if ((uint)eType > 15) -> default
 *   30022186  JMP [EAX*4 + 0x30022228]              switch(eType) via jump table
 *
 * Jump table @0x30022228 (16 dwords, little-endian, verified from .text):
 *   eType  0 -> 0x30022196  CALL 0x3001e430               CG_AddCEntity_General(cent)      (EAX=cent)
 *   eType  1 -> 0x3002219f  MOV ECX,EBX; CALL 0x300343e0   CG_AddCEntity_Player(cent)       (ECX=cent)
 *   eType  2 -> 0x300221aa  PUSH EBX; CALL 0x300346c0      CG_AddCEntity_PlayerCorpse(cent) (stack)
 *   eType  3 -> 0x300221b7  CALL 0x3001e680               CG_AddCEntity_Item(cent)         (EAX=cent)
 *   eType  4 -> 0x300221c0  CALL 0x3001edb0               CG_AddCEntity_Missile(cent)      (EAX=cent)
 *   eType  5 -> 0x300221c9  PUSH EBX; CALL 0x3001f120      CG_Mover(cent)                   (stack)
 *   eType  6 -> 0x3002220b  MOV ESI,EBX; CALL 0x3001f470   CG_Portal(cent)                  (ESI=cent)
 *   eType  7 -> 0x30022224  (bare epilogue: no handler, no-op)
 *   eType  8 -> 0x300221d6  PUSH EBX; CALL 0x3001f260      CG_ScriptMover(cent)             (stack)
 *   eType  9 -> 0x300221f7  CALL 0x30021860               CG_AddCEntity_SoundBlend(cent)   (EAX=cent)
 *   eType 10 -> 0x30022200  MOV EAX,EBX; CALL 0x30021a30   CG_AddCEntity_LoopedFx(cent)     (EAX=cent)
 *   eType 11 -> 0x3002218d  CALL 0x3001eca0               CG_AddCEntity_Turret(cent)       (EAX=cent)
 *   eType 12 -> 0x300221e3  CALL 0x30021660               CG_AddCEntity_Vehicle(cent)      (EAX=cent)
 *   eType 13 -> 0x300221e3  CALL 0x30021660               CG_AddCEntity_Vehicle(cent)      (shared with 12)
 *   eType 14 -> 0x30022216  (default: "Bad entity type: %i\n")
 *   eType 15 -> 0x300221ec  MOV ESI,EBX; CALL 0x30021540   CG_VehicleOwnerIcon(cent)        (ESI=cent)
 *   default   -> 0x30022216  PUSH eType; PUSH fmt; CALL Com_ErrorMessage; ADD ESP,0x8
 *
 * At the entry the handler receives cent in a specific register (EAX/ECX/ESI) or
 * as a pushed stack dword; source-wise this is the same argument in every case
 * (cent), so each is written as a call taking cent. The register-vs-stack
 * distinction is a lowering detail recorded per callee decl, not source behavior.
 * Note slot 14 falls into the same default as any out-of-range eType, and slot 7
 * is a genuine no-op (its table entry is the shared epilogue address).
 */
void CG_AddCEntity(centity_t *cent)
{
    /* Shared per-entity setup run before the eType dispatch (EAX=cent). */
    CG_EntityEffects(cent);

    /* switch over currentState.eType; the range gate is unsigned (CMP,JA), so a
     * value > 15 (0xf) takes the default. eType is read as a 32-bit dword. */
    int32_t eType = cent->currentState.eType;
    switch ((entityType_t)eType) {
    case ET_GENERAL:
        CG_General(cent);
        break;
    case ET_PLAYER:
        CG_Player(cent);
        break;
    case ET_PLAYER_CORPSE:
        CG_AddPlayerCorpseEntity(cent);
        break;
    case ET_ITEM:
        CG_Item(cent);
        break;
    case ET_MISSILE:
        CG_Missile(cent);
        break;
    case ET_MOVER:
        CG_Mover(cent);
        break;
    case ET_PORTAL:
        CG_Portal(cent);
        break;
    case ET_INVISIBLE:
        /* Jump-table slot 7 is the bare epilogue: nothing is done. */
        break;
    case ET_SCRIPTMOVER:
        CG_ScriptMover(cent);
        break;
    case ET_SOUND_BLEND:
        CG_AddLoopedEntitySound(cent);
        break;
    case ET_LOOPED_FX:
        CG_AddCEntity_LoopedFx(cent);
        break;
    case ET_TURRET:
        CG_AddCEntity_ET11(cent);
        break;
    case ET_VEHICLE:
    case ET_VEHICLE_CORPSE:
        /* Vehicle and vehicle-corpse share table entry 0x300221e3. */
        CG_AddCEntity_Vehicle(cent);
        break;
    case ET_VEHICLE_OWNER_ICON:
        CG_VehicleOwnerIcon(cent);
        break;
    default:
        /* ET_VEHICLE_COLLMAP and any value > 15 land here. The type is passed
         * as the %i argument (PUSH eType; PUSH fmt; caller-cleaned 8 bytes). */
        Com_ErrorMessage(cg_badEntityTypeErrorFormat, eType);
        break;
    }
}
