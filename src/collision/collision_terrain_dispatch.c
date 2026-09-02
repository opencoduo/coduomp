#include "collision_terrain_dispatch.h"
#include "collision_terrain_trace.h"

#include <string.h>

void Com_DPrintf(const char *format, ...);
/*
 * Complete triangle-soup dispatch and state-preservation cluster:
 *
 *   CoDUOMP.exe  CM_TraceSquareThroughTerrainCollide 0x00425510..0x0042558e
 *                CM_TraceThroughTerrainCollide       0x00425590..0x004255ef
 *                CM_SightTraceThroughTerrainCollide  0x004255f0..0x00425647
 *                CM_PositionTestInTerrainCollide     0x00425650..0x00425690
 *   coduo_lnxded corresponding bodies                0x08056f57..0x08057180
 *
 * Both binaries make the same point/sphere/square decisions, preserve the
 * same temporary z/radius state, and use quiet x87 equality for the sight
 * result.  Calling-convention and structure-copy instruction selection do not
 * define separate behavior.
 */
void CM_TraceSquareThroughTerrainCollide(traceWork_t *traceWork, const collisionTriangleSoup_t *terrainCollide)
{
    static qboolean warned;

    if (warned == qfalse) {
        warned = qtrue;
        /* CoDUOMP.exe 0x0042552b calls the original Com_DPrintf entry, so
         * this diagnostic remains developer-gated rather than unconditional. */
        Com_DPrintf("^1Box collision on terrain currently being faked with capsule collision\n");
    }

    const float savedStartZ = traceWork->start[2];
    const float savedEndZ = traceWork->end[2];
    const float savedRadius = traceWork->sphere.radius;

    traceWork->sphere.use = qtrue;
    traceWork->sphere.radius = traceWork->maxs[0];
    CM_TraceSphereThroughTerrainCollide(traceWork, terrainCollide);
    traceWork->sphere.use = qfalse;
    traceWork->start[2] = savedStartZ;
    traceWork->end[2] = savedEndZ;
    traceWork->sphere.radius = savedRadius;
}

void CM_TraceThroughTerrainCollide(traceWork_t *traceWork, const collisionTriangleSoup_t *terrainCollide)
{
    if (traceWork->isPoint != qfalse) {
        CM_TracePointThroughTerrainCollide(traceWork, terrainCollide);
        return;
    }

    if (traceWork->sphere.use != qfalse) {
        const float savedStartZ = traceWork->start[2];
        const float savedEndZ = traceWork->end[2];

        CM_TraceSphereThroughTerrainCollide(traceWork, terrainCollide);
        traceWork->start[2] = savedStartZ;
        traceWork->end[2] = savedEndZ;
        return;
    }

    CM_TraceSquareThroughTerrainCollide(traceWork, terrainCollide);
}

qboolean CM_SightTraceThroughTerrainCollide(const traceWork_t *traceWork, const collisionTriangleSoup_t *terrainCollide)
{
    traceWork_t sightWork;

    memcpy(&sightWork, traceWork, sizeof(sightWork));
    sightWork.trace.fraction = 1.0f;
    CM_TraceThroughTerrainCollide(&sightWork, terrainCollide);
    return sightWork.trace.fraction == 1.0f ? qtrue : qfalse;
}

qboolean CM_PositionTestInTerrainCollide(traceWork_t *traceWork, const collisionTriangleSoup_t *terrainCollide)
{
    if (traceWork->isPoint != qfalse) {
        return qfalse;
    }

    const float savedStartZ = traceWork->start[2];
    const float savedEndZ = traceWork->end[2];
    const qboolean intersects = CM_PositionTestSphereWithTerrainCollide(traceWork, terrainCollide);

    traceWork->start[2] = savedStartZ;
    traceWork->end[2] = savedEndZ;
    return intersects;
}
