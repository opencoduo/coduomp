#include "collision_server_trace.h"

#include "collision_box_trace.h"
#include "collision_entity_traversal.h"
#include "collision_server_entity.h"
#include "collision_static_model_trace.h"
#include "math/q_math.h"
#include "server/engine/server_game_data.h"

#include <string.h>

enum {
    SV_TRACE_OWNER_NONE = -1,
    SV_SIGHT_TRACE_ENTITY_HIT = -1
};

/*
 * Complete public server trace-query cluster shared by the Windows
 * client/listen server and Linux dedicated server:
 *
 *   CoDUOMP.exe   0x00467c20..0x0046857c
 *   coduo_lnxded  0x0809afc9..0x0809bc1e
 *
 * World/static-model ordering, owner suppression, point/swept selection,
 * linked-entity traversal, and result construction agree.  The swept-work
 * setup blocks retain their proven platform spill patterns.  That is a
 * floating-point realization difference inside one original subsystem, not
 * separate client and dedicated-server ownership.
 */

void SV_Trace(trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int32_t passEntityNum,
              int32_t contentMask, qboolean capsule, qboolean useDObj, const uint8_t *dobjTracePartState, qboolean locational)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (passEntityNum != ENTITYNUM_NONE && (passEntityNum < 0 || passEntityNum >= sv_numGentities || passEntityNum >= MAX_GENTITIES)) {
        passEntityNum = ENTITYNUM_NONE;
    }

    const float *const traceMins = mins != NULL ? mins : vec3_origin;
    const float *const traceMaxs = maxs != NULL ? maxs : vec3_origin;
    trace_t worldTrace;

    memset(&worldTrace, 0, sizeof(worldTrace));
    worldTrace.fraction = 1.0f;
    CM_BoxTrace(&worldTrace, start, end, traceMins, traceMaxs, 0, contentMask, capsule);
    worldTrace.entityNum = worldTrace.fraction == 1.0f ? ENTITYNUM_NONE : ENTITYNUM_WORLD;

    if (worldTrace.fraction == 0.0f) {
        *trace = worldTrace;
        return;
    }

    if (locational != qfalse) {
        CM_PointTraceStaticModels(&worldTrace, start, end, contentMask);
        if (worldTrace.fraction == 0.0f) {
            *trace = worldTrace;
            return;
        }
    }

    /* Both originals evaluate one flat x87 chain, rather than three
     * separately rounded axis differences. */
    const long double traceExtentRaw = ((((long double)traceMaxs[0] - (long double)traceMins[0]) + (long double)traceMaxs[1]) -
                                        (long double)traceMins[1] + (long double)traceMaxs[2]) -
                                       (long double)traceMins[2];
    if (traceExtentRaw == 0.0L) {
        cmPointTraceWork_t work;

        work.start[0] = start[0];
        work.start[1] = start[1];
        work.start[2] = start[2];
        work.end[0] = end[0];
        work.end[1] = end[1];
        work.end[2] = end[2];
        work.bestTrace = worldTrace;
        work.passEntityNum = passEntityNum;
        work.passOwnerNum = SV_TRACE_OWNER_NONE;
        if (passEntityNum != ENTITYNUM_NONE) {
            const int32_t ownerNum = SV_GentityNum(passEntityNum)->ownerNum;
            if (ownerNum != ENTITYNUM_NONE) {
                work.passOwnerNum = ownerNum;
            }
        }
        work.contentsMask = contentMask;
        work.useDObj = useDObj;
        work.dobjTracePartState = dobjTracePartState;

        CM_PointTraceToEntities(&work);
        *trace = work.bestTrace;
        return;
    }

    cmClipMoveWork_t work;
#if defined(WINDOWS_BEHAVIOR)
    /* CoDUOMP.exe 0x00467d2f..0x00467f91 retains selected PC=53
     * intermediates; the X half-size and Y/Z center paths are not the same
     * sequence of binary32 spills used by the dedicated binary. */
    const long double halfXRaw = ((long double)traceMaxs[0] - (long double)traceMins[0]) * 0.5L;
    const float halfX = (float)halfXRaw;
    const float halfY = (float)(((long double)traceMaxs[1] - (long double)traceMins[1]) * 0.5L);
    const float deltaZ = (float)((long double)traceMaxs[2] - (long double)traceMins[2]);
    const long double halfZRaw = (long double)deltaZ * 0.5L;
    const float halfZ = (float)halfZRaw;
    work.mins[0] = (float)-halfXRaw;
    work.mins[1] = -halfY;
    work.mins[2] = (float)-halfZRaw;
    work.maxs[0] = halfX;
    work.maxs[1] = halfY;
    work.maxs[2] = halfZ;
    work.expandedHalfSize[0] = (float)(halfXRaw + 1.0L);
    work.expandedHalfSize[1] = halfY + 1.0f;
    work.expandedHalfSize[2] = (float)(halfZRaw + 1.0L);

    const float centerX = (float)(((long double)traceMaxs[0] + (long double)traceMins[0]) * 0.5L);
    const long double centerYRaw = ((long double)traceMaxs[1] + (long double)traceMins[1]) * 0.5L;
    const float centerZSum = (float)((long double)traceMaxs[2] + (long double)traceMins[2]);
    const long double centerZRaw = (long double)centerZSum * 0.5L;
    work.start[0] = start[0] + centerX;
    work.start[1] = (float)((long double)start[1] + centerYRaw);
    work.start[2] = (float)((long double)start[2] + centerZRaw);
    work.end[0] = end[0] + centerX;
    work.end[1] = (float)((long double)end[1] + centerYRaw);
    work.end[2] = (float)((long double)end[2] + centerZRaw);
#else
    /* coduo_lnxded 0x0809b20c..0x0809b3d4 stores every raw extent and
     * center sum to binary32 before halving it. */
    for (int32_t axis = 0; axis < 3; ++axis) {
        float halfSize = traceMaxs[axis] - traceMins[axis];
        float offset = traceMaxs[axis] + traceMins[axis];

        halfSize *= 0.5f;
        offset *= 0.5f;
        work.mins[axis] = -halfSize;
        work.maxs[axis] = halfSize;
        work.expandedHalfSize[axis] = halfSize + 1.0f;
        work.start[axis] = start[axis] + offset;
        work.end[axis] = end[axis] + offset;
    }
#endif

    work.bestTrace = worldTrace;
    work.passEntityNum = passEntityNum;
    work.passOwnerNum = SV_TRACE_OWNER_NONE;
    if (passEntityNum != ENTITYNUM_NONE) {
        const int32_t ownerNum = SV_GentityNum(passEntityNum)->ownerNum;
        if (ownerNum != ENTITYNUM_NONE) {
            work.passOwnerNum = ownerNum;
        }
    }
    work.contentsMask = contentMask;
    work.capsule = capsule;

    CM_ClipMoveToEntities(&work);

    if (work.bestTrace.fraction < worldTrace.fraction) {
        /* Both originals spill end-start to binary32 before interpolation. */
        const vec3_t delta = {end[0] - start[0], end[1] - start[1], end[2] - start[2]};
        work.bestTrace.endpos[0] = start[0] + delta[0] * work.bestTrace.fraction;
        work.bestTrace.endpos[1] = start[1] + delta[1] * work.bestTrace.fraction;
        work.bestTrace.endpos[2] = start[2] + delta[2] * work.bestTrace.fraction;
    }

    *trace = work.bestTrace;
}

void SV_SightTrace(int32_t *traceResult, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int32_t passEntityNum,
                   int32_t passOwnerNum, int32_t contentMask, qboolean capsule)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (passEntityNum != ENTITYNUM_NONE && (passEntityNum < 0 || passEntityNum >= sv_numGentities || passEntityNum >= MAX_GENTITIES)) {
        passEntityNum = ENTITYNUM_NONE;
    }
    if (passOwnerNum != ENTITYNUM_NONE && (passOwnerNum < 0 || passOwnerNum >= sv_numGentities || passOwnerNum >= MAX_GENTITIES)) {
        passOwnerNum = ENTITYNUM_NONE;
    }

    const float *const traceMins = mins != NULL ? mins : vec3_origin;
    const float *const traceMaxs = maxs != NULL ? maxs : vec3_origin;

    *traceResult = CM_BoxSightTrace(*traceResult, start, end, traceMins, traceMaxs, 0, contentMask, capsule);
    if (*traceResult != 0) {
        return;
    }

    int32_t passEntityOwnerNum = SV_TRACE_OWNER_NONE;
    if (passEntityNum != ENTITYNUM_NONE) {
        const int32_t ownerNum = SV_GentityNum(passEntityNum)->ownerNum;
        if (ownerNum != ENTITYNUM_NONE) {
            passEntityOwnerNum = ownerNum;
        }
    }

    int32_t passOwnerOwnerNum = SV_TRACE_OWNER_NONE;
    if (passOwnerNum != ENTITYNUM_NONE) {
        const int32_t ownerNum = SV_GentityNum(passOwnerNum)->ownerNum;
        if (ownerNum != ENTITYNUM_NONE) {
            passOwnerOwnerNum = ownerNum;
        }
    }

    const long double traceExtentRaw = ((((long double)traceMaxs[0] - (long double)traceMins[0]) + (long double)traceMaxs[1]) -
                                        (long double)traceMins[1] + (long double)traceMaxs[2]) -
                                       (long double)traceMins[2];
    if (traceExtentRaw == 0.0L) {
        cmPointSightTraceWork_t work;

        work.start[0] = start[0];
        work.start[1] = start[1];
        work.start[2] = start[2];
        work.end[0] = end[0];
        work.end[1] = end[1];
        work.end[2] = end[2];
        work.passEntityNum = passEntityNum;
        work.passOwnerNum = passOwnerNum;
        work.passEntityOwnerNum = passEntityOwnerNum;
        work.passOwnerOwnerNum = passOwnerOwnerNum;
        work.contentsMask = contentMask;

        *traceResult = CM_PointSightTraceToEntities(&work);
        return;
    }

    cmClipSightTraceWork_t work;
#if defined(WINDOWS_BEHAVIOR)
    const long double halfXRaw = ((long double)traceMaxs[0] - (long double)traceMins[0]) * 0.5L;
    const float halfX = (float)halfXRaw;
    const float halfY = (float)(((long double)traceMaxs[1] - (long double)traceMins[1]) * 0.5L);
    const float deltaZ = (float)((long double)traceMaxs[2] - (long double)traceMins[2]);
    const long double halfZRaw = (long double)deltaZ * 0.5L;
    const float halfZ = (float)halfZRaw;
    work.mins[0] = (float)-halfXRaw;
    work.mins[1] = -halfY;
    work.mins[2] = (float)-halfZRaw;
    work.maxs[0] = halfX;
    work.maxs[1] = halfY;
    work.maxs[2] = halfZ;
    work.expandedHalfSize[0] = (float)(halfXRaw + 1.0L);
    work.expandedHalfSize[1] = halfY + 1.0f;
    work.expandedHalfSize[2] = (float)(halfZRaw + 1.0L);

    const float centerX = (float)(((long double)traceMaxs[0] + (long double)traceMins[0]) * 0.5L);
    const long double centerYRaw = ((long double)traceMaxs[1] + (long double)traceMins[1]) * 0.5L;
    const float centerZSum = (float)((long double)traceMaxs[2] + (long double)traceMins[2]);
    const long double centerZRaw = (long double)centerZSum * 0.5L;
    work.start[0] = start[0] + centerX;
    work.start[1] = (float)((long double)start[1] + centerYRaw);
    work.start[2] = (float)((long double)start[2] + centerZRaw);
    work.end[0] = end[0] + centerX;
    work.end[1] = (float)((long double)end[1] + centerYRaw);
    work.end[2] = (float)((long double)end[2] + centerZRaw);
#else
    for (int32_t axis = 0; axis < 3; ++axis) {
        float halfSize = traceMaxs[axis] - traceMins[axis];
        float offset = traceMaxs[axis] + traceMins[axis];

        halfSize *= 0.5f;
        offset *= 0.5f;
        work.mins[axis] = -halfSize;
        work.maxs[axis] = halfSize;
        work.expandedHalfSize[axis] = halfSize + 1.0f;
        work.start[axis] = start[axis] + offset;
        work.end[axis] = end[axis] + offset;
    }
#endif

    work.passEntityNum = passEntityNum;
    work.passOwnerNum = passOwnerNum;
    work.passEntityOwnerNum = passEntityOwnerNum;
    work.passOwnerOwnerNum = passOwnerOwnerNum;
    work.contentsMask = contentMask;
    work.capsule = capsule;

    *traceResult = CM_ClipSightTraceToEntities(&work);
}

int32_t SV_SightTraceToEntity(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int32_t entityNum,
                              int32_t contentMask, qboolean capsule)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (entityNum < 0 || entityNum >= sv_numGentities || entityNum >= MAX_GENTITIES) {
        return 0;
    }

    sharedEntity_t *const gentity = SV_GentityNum(entityNum);
    if ((gentity->contents & contentMask) == 0) {
        return 0;
    }

    vec3_t traceAbsMin;
    vec3_t traceAbsMax;
    for (int32_t axis = 0; axis < 3; ++axis) {
        /* CoDUOMP selects this arm for start < end and unordered input. The
         * Linux port spells the test as ordered start < end; finite inputs
         * agree, so retain the main-platform Windows behavior. */
        if (!(start[axis] >= end[axis])) {
            traceAbsMin[axis] = start[axis] + mins[axis] - 1.0f;
            traceAbsMax[axis] = end[axis] + maxs[axis] + 1.0f;
        } else {
            traceAbsMin[axis] = end[axis] + mins[axis] - 1.0f;
            traceAbsMax[axis] = start[axis] + maxs[axis] + 1.0f;
        }
    }

    if (traceAbsMax[0] < gentity->absMin[0] || traceAbsMax[1] < gentity->absMin[1] || traceAbsMax[2] < gentity->absMin[2] ||
        gentity->absMax[0] < traceAbsMin[0] || gentity->absMax[1] < traceAbsMin[1] || gentity->absMax[2] < traceAbsMin[2]) {
        return 0;
    }

    const float *const angles = gentity->bmodel != qfalse ? gentity->currentAngles : vec3_origin;
    const int32_t hit = CM_TransformedBoxSightTrace(0, start, end, mins, maxs, SV_ClipHandleForEntity(gentity), contentMask,
                                                    gentity->currentOrigin, angles, capsule);
    return hit != 0 ? SV_SIGHT_TRACE_ENTITY_HIT : 0;
}
