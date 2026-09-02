// Source: uo_cgame_mp_x86.dll 0x30005d70..0x30005df7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30005d70_30005df7.mcode
//
// CG_TrajectoryPointInBounds - evaluate an entity's position trajectory at a
// given time and report whether a reference point lies inside an axis-aligned
// box centred on that position (x,y in [-36,36], z in [-88,18]).
//
// Naming: the .mcode header carries the SIZE-GUESS name "PM_StartWeaponAnim"
// (matched only on byte size 0x87). REJECTED: this routine starts no animation
// and touches no weapon/pmove state. It calls BG_EvaluateTrajectory (identified
// by that callee's own "BG_EvaluateTrajectory: unknown trType: %i" diagnostic
// string) and performs three axis bounds tests. The provisional-by-role name
// CG_TrajectoryPointInBounds reflects the proven behavior; the exact original
// source name is unresolved (the sole caller 0x30035680 uses the result to gate
// setting entity flag 0x80, i.e. an interaction/proximity gate).
//
// ABI (custom register calling convention, proven from caller 0x30035680):
//   ECX = entity           -> EBX = ECX + 0x0c = &entity->currentState.pos (position trajectory)
//   ESI = reference base   -> ps->psOrigin is the vec3 at ESI+0x14
//   EAX = atTime           -> forwarded UNTOUCHED into BG_EvaluateTrajectory
// The function reserves a 12-byte local (one vec3), never touches stack args,
// and ends in a plain RET (no callee stack cleanup).

#include <stddef.h>

#include "client/cgame/client_recovered.h"

// Offsets this function relies on (i386 target proves 4-byte pointer width):
//   entity->currentState.pos at +0x0c  (EBX = ECX + 0x0c handed to BG_EvaluateTrajectory)
//   ps->psOrigin at +0x14 (ESI+0x14/+0x18/+0x1c comparisons)
_Static_assert(offsetof(centity_t, currentState.pos) == 0x0c, "entity position trajectory must sit at +0x0c");
_Static_assert(offsetof(playerState_t, psOrigin) == 0x14, "reference point must sit at +0x14");

// Bounding half-extents / limits proven from the .rdata float constants compared
// against each axis delta:
//   x,y high limit  = +36.0 (float ptr [0x3007bfc0])
//   x,y low  limit  = -36.0 (float ptr [0x3007bfbc])
//   z   high limit  = +18.0 (float ptr [0x3007bf10])
//   z   low  limit  = -88.0 (float ptr [0x3007bfb8])
#define CG_TRAJBOX_XY_MAX 36.0f
#define CG_TRAJBOX_XY_MIN (-36.0f)
#define CG_TRAJBOX_Z_MAX 18.0f
#define CG_TRAJBOX_Z_MIN (-88.0f)

qboolean CG_TrajectoryPointInBounds(const centity_t *entity, const playerState_t *ps, int32_t atTime)
{
    vec3_t pos;
    /* The DLL leaves each axis delta (FLD psOrigin[i]; FSUB pos[i]) in ST0 and
     * FCOM/FCOMPs it against the box limits WITHOUT ever FSTP-DWORDing it to a
     * float slot (0x30005d82..0x30005de4). A `float d` would round the difference
     * to float before the boundary compares; long double keeps the 80-bit delta
     * the compares actually run on. */
    long double d;

    // 30005d76: EBX = ECX + 0x0c = &entity->currentState.pos
    // 30005d7d: ECX = &pos (result buffer)
    // 30005d7d..30005d81: BG_EvaluateTrajectory(&entity->currentState.pos, atTime, pos)
    //                     (atTime arrives in EAX and is passed through unchanged)
    BG_EvaluateTrajectory(&entity->currentState.pos, atTime, pos);

    // --- X axis ---------------------------------------------------------
    // 30005d82: FLD ps->psOrigin[0]   (ESI+0x14)
    // 30005d85: FSUB pos[0]
    // 30005d8a/8d90/8d92/8d95: FCOM +36.0; JZ out-of-box when delta > +36.0
    // 30005d97/9d9d/9d9f/9da2: FCOMP -36.0; delta < -36.0 -> out-of-box
    d = ps->psOrigin[0] - pos[0];
    if (d > CG_TRAJBOX_XY_MAX || d < CG_TRAJBOX_XY_MIN) {
        return qfalse;
    }

    // --- Y axis ---------------------------------------------------------
    // 30005da4: FLD ps->psOrigin[1]   (ESI+0x18)
    // 30005da7: FSUB pos[1]
    // 30005dab..30005dc3: same +36.0 / -36.0 window
    d = ps->psOrigin[1] - pos[1];
    if (d > CG_TRAJBOX_XY_MAX || d < CG_TRAJBOX_XY_MIN) {
        return qfalse;
    }

    // --- Z axis ---------------------------------------------------------
    // 30005dc5: FLD ps->psOrigin[2]   (ESI+0x1c)
    // 30005dc8: FSUB pos[2]
    // 30005dcc..30005de4: FCOM +18.0 (JZ out when > +18.0); FCOMP -88.0 (out when < -88.0)
    d = ps->psOrigin[2] - pos[2];
    if (d > CG_TRAJBOX_Z_MAX || d < CG_TRAJBOX_Z_MIN) {
        return qfalse;
    }

    // 30005de6: MOV EAX,1 -> all three axes inside the box
    return qtrue;
}
