#include "bg_pmove.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Sources: uo_cgame_mp_x86.dll 0x3000e840..0x3000e923,
//          uo_game_mp_x86.dll  0x2000e5f0..0x2000e6d2,
//          game.mp.uo.i386.so  RVA 0x0002e3b0..0x0002e462
//
// PM_VerifyPronePosition(const vec3_t origin, const vec3_t velocity)
//
// Prone-position verifier in the cgame pmove core (0x3000e.. is PmoveSingle
// 0x3000e050 / Pmove 0x3000e740). It reads the current pmove context
// (pm, the shared pmove_t; pm->ps is really the
// playerState_t -- see the DIVERGENCE note in globals.h). If the player is not
// prone (playerStateFlags bit PMF_PRONE clear) it does nothing and returns 1
// (trivially accepted). Otherwise it hands the candidate origin/velocity plus the
// player's bounds, prone orientation and lean payload to BG_CheckProneValid along
// with the pmove trace/entity-type callbacks. The machine-code return gate is
// preserved exactly: a zero result commits the supplied origin and velocity,
// while a nonzero result leaves the state untouched.
//
// Naming: the .mcode header's "PM_Accelerate" is a SIZE-ONLY guess
// (win size 0xe3 == matched 0xe4) and is REJECTED. This function performs no
// acceleration/friction math -- it has no x87 arithmetic at all; it is a
// flag-gated trace call that conditionally writes back an origin/velocity pair.
// The name PM_VerifyPronePosition is adopted from the server movement.c name bank
// entry `int PM_VerifyPronePosition(const float *origin, const float *velocity)`,
// whose exact 2-vec3-pointer signature, int return, and prone role match the
// machine code (two caller pointer args at [ESP+0x20]/[ESP+0x24], proneDirection
// forwarded, prone flag gate).
//
// ABI (proven from the sole caller FUN_3000f220_3000fa05 at 0x3000f863):
//   caller: LEA ECX,[ESP+0x58]; PUSH ECX; LEA EDX,[ESP+0x2c]; PUSH EDX;
//           CALL 0x3000e840; ADD ESP,0x8   -> cdecl, 2 stack args, caller-cleaned.
//   arg0 = origin   (const vec3 *)  -> read at [ESP+0x20] after the callee frame
//   arg1 = velocity (const vec3 *)  -> read at [ESP+0x24]
// Returns int (EAX). The early-out path returns 1; the taken path returns EAX from
// FUN_30006e10 (the callee test result), which is 0 on the commit path.
//
// The middle CALL pushes 17 dwords; ADD ESP,0x44 (68) cleans exactly those 17
// arguments (cdecl). The recovered BG_CheckProneValid declaration expresses the
// float, vector, callback, and output-pointer source types directly.
//
// BG_CheckProneValid argument mapping (arg0 = last PUSH, lowest stack address):
//   arg0  = ps->psClientNum        [EAX+0xd4]
//   arg1  = &ps->psOrigin          EAX+0x14
//   arg2  = ps->playerMaxs[0]      [EAX+0x568]        (float bits)
//   arg3  = 30.0f                  0x41f00000
//   arg4  = ps->proneDirection     [EAX+0x5a4]        (float bits)
//   arg5  = &ps->torsoHeight     EAX+0x608
//   arg6  = &ps->torsoPitch     EAX+0x60c
//   arg7  = &ps->waistPitch     EAX+0x610
//   arg8  = 1                      PUSH 0x1
//   arg9  = 1                      PUSH 0x1
//   arg10 = 0                      PUSH 0x0
//   arg11 = move->trace3             [ECX+0x10c]        (overhead trace callback)
//   arg12 = move->trace2             [ECX+0x108]        (ADS/wall trace callback)
//   arg13 = 0                      PUSH 0x0
//   arg14 = 60.0f                  0x42700000
//   arg15 = 0                      PUSH 0x0
//   arg16 = move->entityType         [ECX+0x114]        (entity-type callback)
//
int32_t PM_VerifyPronePosition(const vec3_t origin, const vec3_t velocity)
{
    /* pm is the current pmove context; ->anim is the playerState_t it
     * points at (globals.h DIVERGENCE note). MOV ECX,[0x30539850]; MOV EAX,[ECX];
     * MOV DL,[EAX+0xc]; TEST DL,0x1. */
    pmove_t *move = pm;
    playerState_t *ps = move->ps;

    /* Not prone: return 1 without touching anything (0x3000e91a). */
    if ((ps->playerStateFlags & PMF_PRONE) == 0) {
        return 1;
    }

    /* 0x3000e857..0x3000e866 snapshots these three callbacks from the entry
     * pmove before constructing the 17-argument call packet. */
    pm_entity_type_fn_t entityType = move->entityType;
    pm_trace_fn_t trace2 = move->trace2;
    pm_trace_fn_t trace3 = move->trace3;

    /* Linux retains this BG_CheckProne call.  Both Windows optimizers inline
     * that one-line wrapper into the direct BG_CheckProneValid call described
     * above, including its checkForwardClearance=qfalse argument. */
    int32_t result = BG_CheckProne(ps->psClientNum,                        /* arg0  = [EAX+0xd4]  */
                                   ps->psOrigin,                           /* arg1  = EAX+0x14    */
                                   ps->playerMaxs[0],                      /* arg2  = [EAX+0x568] */
                                   30.0f,                                  /* arg3  = 0x41f00000  */
                                   ps->proneDirection,                     /* arg4  = [EAX+0x5a4] */
                                   &ps->torsoHeight,                     /* arg5  = EAX+0x608   */
                                   &ps->torsoPitch,                     /* arg6  = EAX+0x60c   */
                                   &ps->waistPitch,                     /* arg7  = EAX+0x610   */
                                   qtrue,                                  /* arg8  = PUSH 0x1    */
                                   qtrue,                                  /* arg9  = PUSH 0x1    */
                                   NULL,                                   /* arg10 = PUSH 0x0    */
                                   trace3,                                 /* arg11 = saved [ECX+0x10c] */
                                   trace2,                                 /* arg12 = saved [ECX+0x108] */
                                   qfalse,                                 /* arg13 = PUSH 0x0    */
                                   60.0f,                                  /* arg14 = 0x42700000  */
                                   entityType);                            /* wrapper arg15 */

    /* A nonzero result leaves the state untouched and is returned directly
     * (0x3000e8d4 JNZ -> tail). */
    if (result != 0) {
        return result;
    }

    /* A zero result reloads pm once, then reloads move->ps at every
     * destination store. All six transfers are raw MOV dwords, not x87 loads;
     * memcpy preserves every float payload bit, including signaling NaNs. */
    uint32_t word;
    move = pm;                         /* MOV ECX,[0x30539850] */
    ps = move->ps;
    memcpy(&word, &origin[0], sizeof(word));
    memcpy(&ps->psOrigin[0], &word, sizeof(word));
    memcpy(&word, &origin[1], sizeof(word));
    ps = move->ps;
    memcpy(&ps->psOrigin[1], &word, sizeof(word));
    ps = move->ps;
    memcpy(&word, &origin[2], sizeof(word));
    memcpy(&ps->psOrigin[2], &word, sizeof(word));
    ps = move->ps;
    memcpy(&word, &velocity[0], sizeof(word));
    memcpy(&ps->velocity[0], &word, sizeof(word));
    ps = move->ps;
    memcpy(&word, &velocity[1], sizeof(word));
    memcpy(&ps->velocity[1], &word, sizeof(word));
    ps = move->ps;
    memcpy(&word, &velocity[2], sizeof(word));
    memcpy(&ps->velocity[2], &word, sizeof(word));

    return 0;
}

/* Offsets proven against the machine-code instruction streams. */
#define BG_VERIFY_PRONE_LAYOUT_ASSERT(name_, expression_) typedef char name_[(expression_) ? 1 : -1]

BG_VERIFY_PRONE_LAYOUT_ASSERT(bg_verify_prone_flags_offset, offsetof(playerState_t, playerStateFlags) == 0x0c);
BG_VERIFY_PRONE_LAYOUT_ASSERT(bg_verify_prone_origin_offset, offsetof(playerState_t, psOrigin) == 0x14);
BG_VERIFY_PRONE_LAYOUT_ASSERT(bg_verify_prone_velocity_offset, offsetof(playerState_t, velocity) == 0x20);
BG_VERIFY_PRONE_LAYOUT_ASSERT(bg_verify_prone_client_num_offset, offsetof(playerState_t, psClientNum) == 0xd4);
BG_VERIFY_PRONE_LAYOUT_ASSERT(bg_verify_prone_maxs_offset, offsetof(playerState_t, playerMaxs) == 0x568);
BG_VERIFY_PRONE_LAYOUT_ASSERT(bg_verify_prone_direction_offset, offsetof(playerState_t, proneDirection) == 0x5a4);
BG_VERIFY_PRONE_LAYOUT_ASSERT(bg_verify_prone_torso_height_offset, offsetof(playerState_t, torsoHeight) == 0x608);

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
BG_VERIFY_PRONE_LAYOUT_ASSERT(bg_verify_prone_trace2_offset, offsetof(pmove_t, trace2) == 0x108);
BG_VERIFY_PRONE_LAYOUT_ASSERT(bg_verify_prone_trace3_offset, offsetof(pmove_t, trace3) == 0x10c);
BG_VERIFY_PRONE_LAYOUT_ASSERT(bg_verify_prone_entity_type_offset, offsetof(pmove_t, entityType) == 0x114);
#endif

#undef BG_VERIFY_PRONE_LAYOUT_ASSERT
