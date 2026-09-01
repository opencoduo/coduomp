// Sources: uo_cgame_mp_x86.dll 0x3000a140..0x3000a29c,
//          uo_game_mp_x86.dll  0x20009f00..0x2000a05b,
//          game.mp.uo.i386.so  0x000262b7..0x0002656c
//
// PM_CorrectAllSolid — pmove "unstick" helper. When the player origin is inside a
// solid brush, probe the 26 unit-cube neighbour offsets (all {-1,0,1}^3 offsets
// except {0,0,0}) in a fixed order; the first offset whose point trace does NOT
// start in solid is accepted: the origin is moved there, a 1-unit downward ground
// trace is run and cached in pml.groundTrace, and the origin snaps to that trace's
// endpos. If every offset is solid, write the Quake3 ground-miss epilogue
// (groundEntityNum = ENTITYNUM_NONE; pml.groundPlane / pml.groundLiftFlag /
// pml.walking = 0; clear the jump timer bit; jumpOriginZ = 0) and return qfalse.
//
// NAME: the .mcode's mechanical pre-hint "Display_MouseMove" (a ui_shared.c mouse
// dispatcher, size-guessed 0x15c==0x15c) is REJECTED. This function is pure pmove:
// it lives inside the PM_* cluster (callee 0x30008280 == PM_trace; sibling
// 0x3000a2a0 == PM_GroundTrace; the pml.walking/groundPlane/groundLiftFlag locals
// at 0x305395ac/b0/b4), reads pm (the BG pmove context), and iterates
// the classic Quake3/CoD PM_CorrectAllSolid corner-offset table (pm_correctSolidOffsets).
// The cgame_mp PPC bank lists PM_CorrectAllSolid in this same PM_* cluster.
//
// Register ABI (proven from the caller at 0x3000a51b): the trace_t* results buffer
// arrives in EAX; the caller cleans nothing extra (a plain-arg helper). Modeled as a
// single trace_t* parameter. EBX/ESI are callee-saved.

#include "bg_pmove.h"

#include <stddef.h>
#include <stdint.h>

/*
 * The complete 26-entry unit-cube neighbor table agrees byte-for-byte in the
 * Windows cgame/game modules and the Linux game module.  It is owned by this
 * helper; no other original function addresses the table.
 */
static const vec3_t pm_correctSolidOffsets[26] = {
    {  0.0f,  0.0f,  1.0f },
    { -1.0f,  0.0f,  1.0f },
    {  0.0f, -1.0f,  1.0f },
    {  1.0f,  0.0f,  1.0f },
    {  0.0f,  1.0f,  1.0f },
    { -1.0f,  0.0f,  0.0f },
    {  0.0f, -1.0f,  0.0f },
    {  1.0f,  0.0f,  0.0f },
    {  0.0f,  1.0f,  0.0f },
    {  0.0f,  0.0f, -1.0f },
    { -1.0f,  0.0f, -1.0f },
    {  0.0f, -1.0f, -1.0f },
    {  1.0f,  0.0f, -1.0f },
    {  0.0f,  1.0f, -1.0f },
    { -1.0f, -1.0f,  1.0f },
    {  1.0f, -1.0f,  1.0f },
    {  1.0f,  1.0f,  1.0f },
    { -1.0f,  1.0f,  1.0f },
    { -1.0f, -1.0f,  0.0f },
    {  1.0f, -1.0f,  0.0f },
    {  1.0f,  1.0f,  0.0f },
    { -1.0f,  1.0f,  0.0f },
    { -1.0f, -1.0f, -1.0f },
    {  1.0f, -1.0f, -1.0f },
    {  1.0f,  1.0f, -1.0f },
    { -1.0f,  1.0f, -1.0f },
};

/*
 * PM_CorrectAllSolid.
 *
 * `results` is the caller's scratch trace_t (EAX on entry, kept in EBX across the
 * body and passed straight through as the PM_trace results pointer, so PM_trace
 * fills it in place).
 */
qboolean PM_CorrectAllSolid(trace_t *results)
{
    pmove_t *move;
    vec3_t point;
    int i;

    /* Probe each unit-cube neighbour offset until one is not startsolid. ESI walks
     * the offset table by 0xc bytes (one vec3_t) from 0 to 0x138 (26 entries). */
    for (i = 0; i < 26; i++) {
        move = pm; /* 0x3000a150 reload for every probe. */

        /* point = ps->origin + pm_correctSolidOffsets[i]  (each component FADD'd) */
        playerState_t *pointXPs = move->ps;
        point[0] = pm_correctSolidOffsets[i][0] + pointXPs->psOrigin[0];
        playerState_t *pointYPs = move->ps;
        point[1] = pm_correctSolidOffsets[i][1] + pointYPs->psOrigin[1];
        playerState_t *pointZPs = move->ps;
        point[2] = pm_correctSolidOffsets[i][2] + pointZPs->psOrigin[2];

        /* Zero-length point trace at the nudged spot (start == end == point). */
        playerState_t *tracePs = move->ps;
        int32_t traceClientNum = tracePs->psClientNum;
        int32_t traceMask = move->traceMask;
        PM_trace(results, point, move->mins, move->maxs, point,
                 traceClientNum, traceMask);

        /* [results+0x2f] == trace_t.startsolid. A neighbour that is not startsolid
         * is an acceptable non-solid position. */
        if (!results->startsolid) {
            move = pm; /* 0x3000a1f8 success-path reload. */

            /* Accept: move the origin to the good neighbour point. */
            playerState_t *originXPs = move->ps;
            originXPs->psOrigin[0] = point[0];
            playerState_t *originYPs = move->ps;
            originYPs->psOrigin[1] = point[1];
            playerState_t *originZPs = move->ps;
            originZPs->psOrigin[2] = point[2];

            /* Downward ground snap: trace 1 unit straight down from the new origin. */
            playerState_t *startXPs = move->ps;
            point[0] = startXPs->psOrigin[0];
            playerState_t *startYPs = move->ps;
            point[1] = startYPs->psOrigin[1];
            playerState_t *startZPs = move->ps;
            point[2] = startZPs->psOrigin[2] - 1.0f;

            playerState_t *groundTracePs = move->ps;
            int32_t groundTraceClientNum = groundTracePs->psClientNum;
            int32_t groundTraceMask = move->traceMask;
            PM_trace(results, groundTracePs->psOrigin, move->mins,
                     move->maxs, point, groundTraceClientNum,
                     groundTraceMask);

            /* Cache the whole trace_t (rep movsd, 0xc dwords == 48 bytes). */
            move = pm; /* 0x3000a262 reload before the copy. */
            pml.groundTrace = *results;

            /* Snap the origin to the ground trace endpos. */
            playerState_t *endXPs = move->ps;
            endXPs->psOrigin[0] = results->endpos[0];
            playerState_t *endYPs = move->ps;
            endYPs->psOrigin[1] = results->endpos[1];
            playerState_t *endZPs = move->ps;
            endZPs->psOrigin[2] = results->endpos[2];
            return qtrue;
        }
    }

    /* Every neighbour was solid: Quake3 ground-miss epilogue. */
    move = pm; /* 0x3000a1c0 failure-path reload. */
    playerState_t *groundPs = move->ps;
    groundPs->groundEntityNum = ENTITYNUM_NONE;
    pml.groundPlane = 0;
    pml.groundLiftFlag = 0;
    pml.walking = 0;
    playerState_t *flagPs = move->ps;
    flagPs->playerStateFlags &= ~(uint32_t)PMF_WALLJUMP;
    playerState_t *originPs = move->ps;
    originPs->jumpOriginZ = 0.0f;
    return qfalse;
}
