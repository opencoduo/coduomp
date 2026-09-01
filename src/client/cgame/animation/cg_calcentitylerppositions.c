// Source: uo_cgame_mp_x86.dll 0x30021d30..0x30021e80
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30021d30_30021e80.mcode
//
// CG_CalcEntityLerpPositions (0x30021d30) — compute one client entity's
// interpolated render/"weapon" angles into its centity +0x208 lerpOrigin /
// +0x214 lerpAngles block, and mirror the derived view angles into the
// entity's per-client / per-corpse 0x4d0-stride state record.
//
// NAMING: the .mcode header assigns the SIZE-GUESS name "BG_CalculateWeaponAngles"
// (matched only on byte size 0x150). REJECTED: this is not the shared BG
// weapon-position math family (BG_CalculateWeaponPosition / _AddIdleSway /
// _AddViewKick, 0x30014.. / 0x30015..); this function reads a centity's
// currentState trajectories, evaluates them at cg_time via BG_EvaluateTrajectory,
// writes the entity's centity-level lerpOrigin block, and mover-lag-adjusts it.
// The address already had TWO divergent provisional names in the shared header —
// CG_CalcEntityLerpPositions (from the HUD-tag draw callers) and
// CG_UpdateEntityDObjRenderState (from the CG_AddPacketEntities callers). Those are
// the same function; this reconstruction consolidates them onto
// CG_CalcEntityLerpPositions. The Mac symbol of that name has the identical two
// named direct callees, BG_EvaluateTrajectory and CG_AdjustPositionForMover;
// "DObjRenderState" is disproven by the absence of DObj access. See
// client_recovered.h.
//
// ABI: one caller-cleaned int32 cdecl stack arg — the centity pointer (loaded
// MOV EBP,[ESP+8] after PUSH EBP). Plain RET. No register arguments.
//
// The centity combines the client-side interpolation state with the embedded
// network entity state at the same base address:
//   - centity-level lerpOrigin(+0x208) / lerpAngles(+0x214), which this writes;
//   - embedded currentState number(+0x0),
//                              eType(+0x4), pos(+0xc) / apos(+0x30) trajectories,
//                              vehicleEntityNum(+0x74), moverEntityNum(+0x7c),
//                              otherEntityNum(+0x94), and the two scalars
//                              leanValue(+0x6c)/leanf(+0xd8).
//
// Machine-code facts preserved:
//   - 0x30021d35: eType == ET_SOUND_BLEND(9) AND vehicleEntityNum(+0x74) != 0x3ff
//     -> copy the six dwords +0x208..+0x21c straight from
//     cg_entities[vehicleEntityNum] (proxy/attach entity) and return.
//   - 0x30021d9a: switch on currentState.pos.trType(+0xc): TR_INTERPOLATE(1), OR
//     TR_LINEAR_STOP(3) with currentState.number(+0x0) < 0x40 (below MAX_CLIENTS),
//     tail-call the sibling smoothing routine 0x30021bb0 and return.
//   - 0x30021dbd: otherwise evaluate the two trajectories at cg_time
//     (BG_EvaluateTrajectory, register ABI EAX=atTime, ECX=out, EBX=tr):
//        lerpOrigin     = eval(&currentState.pos , cg_time)
//        lerpAngles = eval(&currentState.apos, cg_time)
//   - 0x30021de5: on currentState.eType(+0x4): ET_PLAYER(1) selects
//     bgs.clientinfo[otherEntityNum(+0x94)]; ET_PLAYER_CORPSE(2) selects
//     cg_corpseInfo[number(+0x0) - 0x40]; any other eType skips the block
//     write. Both stride-0x4d0 tables carry the shared cgLerpAngleBlock_t at +0x3e0.
//   - 0x30021e18..0x30021e4f: write the block { leanAmount=leanValue,
//     viewPitch=lerpAngles[0], viewYaw=lerpAngles[1],
//     viewRoll=lerpAngles[2], leanFraction=leanf }, THEN zero
//     lerpAngles[0] and lerpAngles[2] (NOT [1]). The five block dwords
//     are plain int/float MOV copies; modeled as float copies (the source fields are
//     angles/trajectory outputs).
//   - 0x30021e55: unless the entity IS cg_predictedEventEntity (0x304876c8), apply
//     CG_AdjustPositionForMover to lerpOrigin: in=out=&lerpOrigin,
//     moverNum=moverEntityNum(+0x7c), fromTime=cg_snap->serverTime(+0x8), toTime=cg_time,
//     angleDelta=NULL (ECX==0). cdecl; caller `add esp,0xc`.

#include <stddef.h>
#include <string.h>
#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* cg_entities[] — the DLL's centity array (stride 0x288 == sizeof centity_t),
 * base cg_entities. */

void CG_CalcEntityLerpPositions(centity_t *entity)
{
    /* Same physical centity, currentState/trajectory view. */
    centity_t *state = (centity_t *)entity;

    /* 0x30021d35: ET_SOUND_BLEND proxy — copy a referenced entity's angle block. */
    if (state->currentState.eType == ET_SOUND_BLEND) {
        int32_t proxyNum = state->currentState.vehicleEntityNum;                 /* +0x74 */
        if (proxyNum != ENTITYNUM_NONE) {                           /* 0x3ff */
            /* 0x30021d45: cg_entities[proxyNum] (stride 0x288). */
            centity_t *proxy = cg_entities + proxyNum;
            /* 0x30021d50..0x30021d92: six raw dword copies of lerpOrigin and
             * lerpAngles. The two vec3s are contiguous in centity_t. */
            memcpy(entity->lerpOrigin, proxy->lerpOrigin,
                   sizeof(entity->lerpOrigin) + sizeof(entity->lerpAngles));
            return;
        }
        /* proxyNum == 0x3ff falls through to the derive path (0x30021d43 JZ). */
    }

    /* 0x30021d9a: on the position trajectory type, some entities use a separate
     * smoothing routine (0x30021bb0) instead of the direct trajectory evaluation. */
    trType_t posTrType = state->currentState.pos.trType;                         /* +0xc */
    if (posTrType == TR_INTERPOLATE ||
        (posTrType == TR_LINEAR_STOP && state->currentState.number < 0x40)) {    /* number < MAX_CLIENTS */
        /* 0x30021db2: ESI = entity; 0x30021db4 tail-call the sibling. */
        CG_InterpolateEntityPosition(entity);
        return;
    }

    /* 0x30021dbd..0x30021de0: evaluate the pos/apos trajectories at cg_time.
     * BG_EvaluateTrajectory register ABI: EAX=atTime(cg_time), ECX=out, EBX=tr. */
    BG_EvaluateTrajectory(&state->currentState.pos,
                          coduo_int32_from_bits(cg_time), entity->lerpOrigin);
    BG_EvaluateTrajectory(&state->currentState.apos,
                          coduo_int32_from_bits(cg_time), entity->lerpAngles);

    /* 0x30021de5: select the per-entity 0x4d0-stride state record whose shared
     * angle block (+0x3e0) receives the derived view angles. */
    cgLerpAngleBlock_t *block = NULL;
    if (state->currentState.eType == ET_PLAYER) {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        const int32_t clientNum = state->currentState.clientNum;
        if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
            Com_Error(ERR_DROP,
                      "\x15" "CG_CalcEntityLerpPositions: "
                      "invalid client number %i",
                      clientNum);
            return;
        }
        clientInfo_t *clientInfo = &bgs.clientinfo[clientNum];
        block = (cgLerpAngleBlock_t *)(void *)&clientInfo->leanAmount;
    } else if (state->currentState.eType == ET_PLAYER_CORPSE) {
        /* 0x30021e07: cg_corpseInfo[number - 0x40] (stride 0x4d0). */
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        uint32_t corpseIndex =
            (uint32_t)state->currentState.number -
            (uint32_t)PLAYER_CLONE_ENTITYNUM_BASE;
        if (corpseIndex >= (uint32_t)PLAYER_CLONE_COUNT) {
            Com_Error(ERR_DROP,
                      "\x15" "CG_CalcEntityLerpPositions: "
                      "invalid player clone entity %i",
                      state->currentState.number);
            return;
        }
        block = (cgLerpAngleBlock_t *)(void *)
            &cg_corpseInfo[corpseIndex].leanAmount;
    }

    if (block != NULL) {
        /* 0x30021e18..0x30021e4f: publish the interpolated view angles into the
         * shared block, then zero lerpAngles[0] and [2] on the centity. */
        memcpy(&block->leanAmount, &state->currentState.leanValue, sizeof(block->leanAmount));
        memcpy(&block->viewPitch, entity->lerpAngles, sizeof(entity->lerpAngles));
        /* 0x30021e41 loads leanf before the zero stores and publishes it after. */
        float leanFraction;
        memcpy(&leanFraction, &state->currentState.leanf, sizeof(leanFraction));
        /* 0x30021e47 / 0x30021e4d: zero lerpAngles[2] then [0] (not [1]). */
        entity->lerpAngles[2] = 0.0f;
        entity->lerpAngles[0] = 0.0f;
        memcpy(&block->leanFraction, &leanFraction, sizeof(block->leanFraction));
    }

    /* 0x30021e55: the singleton cg_predictedEventEntity skips the mover-lag pass. */
    if (entity != &cg_predictedEventEntity) {
        /* 0x30021e5d..0x30021e73: mover-lag-correct lerpOrigin in place.
         * fromTime = cg_snap->serverTime, toTime = cg_time, angleDelta = NULL. */
        CG_AdjustPositionForMover(entity->lerpOrigin, state->currentState.groundEntityNum,
                                  cg_snap->serverTime, coduo_int32_from_bits(cg_time),
                                  entity->lerpOrigin, NULL);
    }
}
