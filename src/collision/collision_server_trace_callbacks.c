#include "collision_server_trace.h"

#include "collision_box_trace.h"
#include "collision_point_contents.h"
#include "collision_server_entity.h"
#include "collision_trace_bounds.h"
#include "collision_world_sector.h"
#include "animation/dobj.h"
#include "qcommon/game_module_abi_types.h"
#include "math/q_math.h"
#include "server/engine/server_game_data.h"
#include "qcommon/vm_runtime.h"

#include <stddef.h>
#include <stdint.h>

extern vm_t *sv_gameVM;

enum {
    SV_SIGHT_TRACE_ENTITY_HIT = -1
};

/*
 * Complete per-entity server collision callback bank shared by the Windows
 * client/listen server and Linux dedicated server:
 *
 *   CoDUOMP.exe   0x00467550..0x00467c1d
 *   coduo_lnxded  0x0809a594..0x0809afc8
 *
 * The callbacks make the same contents, ownership, collision-model, solid
 * byte, and result-replacement decisions.  The stripped Linux recovery's
 * former helper names and MatrixInverseTransformVector43 spelling were not
 * separate contracts: the canonical Mac symbol is
 * MatrixTransposeTransformVector43, whose shared implementation already
 * retains each platform's proven x87 operation graph.
 */

void SV_ClipMoveToEntity(cmClipMoveWork_t *work, svEntity_t *serverEntity)
{
    const int32_t entityNum = (int32_t)(serverEntity - sv_entities);
    sharedEntity_t *const gentity = SV_GentityNum(entityNum);

    if ((gentity->contents & work->contentsMask) == 0) {
        return;
    }
    if (work->passEntityNum != ENTITYNUM_NONE &&
        (entityNum == work->passEntityNum || gentity->ownerNum == work->passEntityNum || gentity->ownerNum == work->passOwnerNum)) {
        return;
    }

    const float *const angles = gentity->bmodel != qfalse ? gentity->currentAngles : vec3_origin;
    trace_t trace;
    trace.fraction = work->bestTrace.fraction;

    CM_TransformedBoxTrace(&trace, work->start, work->end, work->mins, work->maxs, SV_ClipHandleForEntity(gentity), work->contentsMask,
                           gentity->currentOrigin, angles, work->capsule);

    /* The x87 unordered case follows the replacement path. */
    if (trace.fraction >= work->bestTrace.fraction) {
        work->bestTrace.allsolid |= trace.allsolid;
        work->bestTrace.startsolid |= trace.startsolid;
        return;
    }

    trace.allsolid |= work->bestTrace.allsolid;
    trace.startsolid |= work->bestTrace.startsolid;
    trace.entityNum = (uint16_t)gentity->entityState.number;
    work->bestTrace = trace;
}

void SV_PointTraceToEntity(cmPointTraceWork_t *work, svEntity_t *serverEntity)
{
    const int32_t entityNum = (int32_t)(serverEntity - sv_entities);
    sharedEntity_t *const gentity = SV_GentityNum(entityNum);

    if ((gentity->contents & work->contentsMask) == 0) {
        return;
    }
    if (work->passEntityNum != ENTITYNUM_NONE &&
        (entityNum == work->passEntityNum || gentity->ownerNum == work->passEntityNum || gentity->ownerNum == work->passOwnerNum)) {
        return;
    }

    trace_t trace;
    DObj *const dobj = work->useDObj != qfalse
#if defined(WINDOWS_BEHAVIOR)
                           ? Com_GetServerDObj(gentity->entityState.number)
#else
                           ? Com_GetServerDObj(entityNum)
#endif
                           : NULL;

    if (dobj != NULL && (gentity->svFlags & SVF_DOBJ_BOUNDS_MASK) != 0U) {
        vec3_t dobjMins;
        vec3_t dobjMaxs;

        if ((gentity->svFlags & SVF_DOBJ_USE_MODEL_BOUNDS) != 0U) {
            if (DObjHasContents(dobj, work->contentsMask) == qfalse) {
                return;
            }
            DObjGetBounds(dobj, dobjMins, dobjMaxs);
        } else {
            dobjMins[0] = sv_defaultEntityClipMins[0];
            dobjMins[1] = sv_defaultEntityClipMins[1];
            dobjMins[2] = sv_defaultEntityClipMins[2];
            dobjMaxs[0] = sv_defaultEntityClipMaxs[0];
            dobjMaxs[1] = sv_defaultEntityClipMaxs[1];
            dobjMaxs[2] = sv_defaultEntityClipMaxs[2];
        }

        for (int32_t axis = 0; axis < 3; ++axis) {
            dobjMins[axis] += gentity->currentOrigin[axis];
            dobjMaxs[axis] += gentity->currentOrigin[axis];
        }

        if (CM_TraceLineSkipsBox(work->start, work->end, dobjMins, dobjMaxs, work->bestTrace.fraction) != qfalse) {
            return;
        }

        /* The original Windows VM bridge exposes twelve argument slots.
         * intptr_t casts keep the shared native varargs boundary valid on
         * 64-bit hosts; Linux consumes only the command's one declared arg. */
        (void)(VM_Call)(sv_gameVM, GAME_DOBJ_CALC_POSE,
#if defined(WINDOWS_BEHAVIOR)
                        (intptr_t)gentity->entityState.number,
#else
                        (intptr_t)entityNum,
#endif
                        (intptr_t)0, (intptr_t)0, (intptr_t)0, (intptr_t)0, (intptr_t)0, (intptr_t)0, (intptr_t)0, (intptr_t)0, (intptr_t)0,
                        (intptr_t)0, (intptr_t)0);

        matrix43_t entityMatrix;
        AnglesToAxis(gentity->currentAngles, entityMatrix.axis);
        entityMatrix.origin[0] = gentity->currentOrigin[0];
        entityMatrix.origin[1] = gentity->currentOrigin[1];
        entityMatrix.origin[2] = gentity->currentOrigin[2];

        vec3_t localStart;
        vec3_t localEnd;
        MatrixTransposeTransformVector43(work->start, &entityMatrix, localStart);
        MatrixTransposeTransformVector43(work->end, &entityMatrix, localEnd);

        dobj_trace_result_t dobjTrace;
        dobjTrace.fraction = work->bestTrace.fraction;
        if ((gentity->svFlags & SVF_DOBJ_USE_MODEL_BOUNDS) != 0U) {
            DObjTraceModelParts(dobj, localStart, localEnd, work->contentsMask, &dobjTrace);
        } else {
            DObjTraceParts(dobj, localStart, localEnd, work->dobjTracePartState, &dobjTrace);
        }

        /* The x87 unordered case follows the replacement path. */
        if (dobjTrace.fraction >= work->bestTrace.fraction) {
            work->bestTrace.allsolid |= dobjTrace.allsolid;
            work->bestTrace.startsolid |= dobjTrace.startsolid;
            return;
        }

        trace.fraction = dobjTrace.fraction;
        trace.surfaceFlags = dobjTrace.surfaceFlags;
        trace.partName = dobjTrace.hitPartNameHandle;
        trace.partGroup = dobjTrace.hitPartStateIndex;
        trace.startsolid = dobjTrace.startsolid;
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        trace.allsolid = 0;
        const matrix43_t *const matrixView = &entityMatrix;
        MatrixTransformVector(dobjTrace.normal, matrixView->axis, trace.normal);

        const vec3_t delta = {work->end[0] - work->start[0], work->end[1] - work->start[1], work->end[2] - work->start[2]};
        trace.endpos[0] = work->start[0] + delta[0] * trace.fraction;
        trace.endpos[1] = work->start[1] + delta[1] * trace.fraction;
        trace.endpos[2] = work->start[2] + delta[2] * trace.fraction;
    } else {
        const float *const angles = gentity->bmodel != qfalse ? gentity->currentAngles : vec3_origin;

        trace.fraction = work->bestTrace.fraction;
        CM_TransformedBoxTrace(&trace, work->start, work->end, vec3_origin, vec3_origin, SV_ClipHandleForEntity(gentity),
                               work->contentsMask, gentity->currentOrigin, angles, qfalse);

        /* The x87 unordered case follows the replacement path. */
        if (trace.fraction >= work->bestTrace.fraction) {
            work->bestTrace.allsolid |= trace.allsolid;
            work->bestTrace.startsolid |= trace.startsolid;
            return;
        }
    }

    /* On the closer-DObj path trace.allsolid is the determinized zero above
     * (stock ORs unwritten stack residue here: CoDUOMP.exe 0x004679c4,
     * coduo_lnxded 0x0809ac1b..0x0809ac25); on the box path it is the
     * CM_TransformedBoxTrace result, as in stock. */
    trace.allsolid |= work->bestTrace.allsolid;
    trace.startsolid |= work->bestTrace.startsolid;
#if defined(WINDOWS_BEHAVIOR)
    trace.entityNum = (uint16_t)gentity->entityState.number;
#else
    trace.entityNum = (uint16_t)entityNum;
#endif
    trace.contents = gentity->contents;
    trace.material = NULL;
    work->bestTrace = trace;
}

int32_t SV_ClipSightTraceToEntity(cmClipSightTraceWork_t *work, svEntity_t *serverEntity)
{
    const int32_t entityNum = (int32_t)(serverEntity - sv_entities);
    sharedEntity_t *const gentity = SV_GentityNum(entityNum);

    if ((gentity->contents & work->contentsMask) == 0) {
        return 0;
    }
    if (work->passEntityNum != ENTITYNUM_NONE &&
        (entityNum == work->passEntityNum || gentity->ownerNum == work->passEntityNum || gentity->ownerNum == work->passEntityOwnerNum)) {
        return 0;
    }
    if (work->passOwnerNum != ENTITYNUM_NONE &&
        (entityNum == work->passOwnerNum || gentity->ownerNum == work->passOwnerNum || gentity->ownerNum == work->passOwnerOwnerNum)) {
        return 0;
    }

    const float *const angles = gentity->bmodel != qfalse ? gentity->currentAngles : vec3_origin;
    const int32_t hit = CM_TransformedBoxSightTrace(0, work->start, work->end, work->mins, work->maxs, SV_ClipHandleForEntity(gentity),
                                                    work->contentsMask, gentity->currentOrigin, angles, work->capsule);
    return hit != 0 ? SV_SIGHT_TRACE_ENTITY_HIT : 0;
}

int32_t SV_PointSightTraceToEntity(cmPointSightTraceWork_t *work, svEntity_t *serverEntity)
{
    const int32_t entityNum = (int32_t)(serverEntity - sv_entities);
    sharedEntity_t *const gentity = SV_GentityNum(entityNum);

    if ((gentity->contents & work->contentsMask) == 0) {
        return 0;
    }
    if (work->passEntityNum != ENTITYNUM_NONE &&
        (entityNum == work->passEntityNum || gentity->ownerNum == work->passEntityNum || gentity->ownerNum == work->passEntityOwnerNum)) {
        return 0;
    }
    if (work->passOwnerNum != ENTITYNUM_NONE &&
        (entityNum == work->passOwnerNum || gentity->ownerNum == work->passOwnerNum || gentity->ownerNum == work->passOwnerOwnerNum)) {
        return 0;
    }

    const float *const angles = gentity->bmodel != qfalse ? gentity->currentAngles : vec3_origin;
    const int32_t hit = CM_TransformedBoxSightTrace(0, work->start, work->end, vec3_origin, vec3_origin, SV_ClipHandleForEntity(gentity),
                                                    work->contentsMask, gentity->currentOrigin, angles, qfalse);
    return hit != 0 ? SV_SIGHT_TRACE_ENTITY_HIT : 0;
}

int32_t SV_PointContents(const vec3_t point, int32_t passEntityNum, int32_t contentMask)
{
    int32_t contents = CM_PointContents(point, 0);
    int32_t entityList[MAX_GENTITIES];
    const int32_t entityCount = CM_AreaEntities(point, point, entityList, MAX_GENTITIES, contentMask);

    for (int32_t entityIndex = 0; entityIndex < entityCount; ++entityIndex) {
        const int32_t entityNum = entityList[entityIndex];
        if (entityNum == passEntityNum) {
            continue;
        }

        sharedEntity_t *const gentity = SV_GentityNum(entityNum);
        contents |= CM_TransformedPointContents(point, SV_ClipHandleForEntity(gentity), gentity->currentOrigin, gentity->currentAngles);
    }

    return contents & contentMask;
}
