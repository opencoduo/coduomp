// Source: uo_cgame_mp_x86.dll 0x3001a4d0..0x3001a5ab
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001a4d0_3001a5ab.mcode
//
// CG_ScanForCrosshairEntity — trace a "shot" ray from the camera origin
// (cg_refdef.vieworg) 8192 units along the current aim/view forward direction
// (cg_refdef.viewaxis[0]) and, if it hits a hittable-contents entity that is below
// MAX_CLIENTS and is not the local player, latch that entity as the crosshair
// target (cg_crosshairEntNum) together with the current cg_time
// (cg_crosshairEntTime). When the trace hits a vehicle (currentState.eType ==
// ET_VEHICLE, 12), the target is redirected to the vehicle's occupant
// (currentState.vehicleEntityNum). The crosshair-name HUD drawer at 0x3001a604
// (this function's sole caller) then fades in the player's name via CG_FadeColor.
//
// Name adjudication: the .mcode header names this CG_DrawScoreboard (a pure size
// match, win 0xdb vs matched 0xdc). REJECTED per the no-size rule: the real
// CG_DrawScoreboard is at 0x30037d90 (already noted in globals.h), and this
// function draws nothing — it issues a trace projection and writes the crosshair
// entity/time globals. Behavior + call graph prove the classic Q3/CoD
// CG_ScanForCrosshairEntity (camera-to-8192 shot trace -> crosshairClientNum/Time).
// Provisional-by-role; exact CoD symbol unconfirmed.
//
// Machine-code facts proven for every behavior-affecting statement:
//   Endpoint (a4d3..a535): for each axis i, endpoint[i] = cg_refdef.viewaxis[0][i]
//     * 8192.0f + cg_refdef.vieworg[i]  (FLD dir[i]; FMUL [0x3007c14c]==8192.0f;
//     FADD viewOrg[i]; FSTP endpoint[i]). Start point = cg_refdef.vieworg copied to
//     the stack (a4f0/a4fb/a4ff store viewOrg.x/y/z).
//   Trace call (a539..a554): CG_Trace(0x30035310) with the custom ABI
//     EAX=handle, ECX=origin, EBX=flags, then stack (out, arg1, arg2, arg3):
//       handle = 0x2000211 (a4b) ; origin = &endpoint (ECX=LEA[ESP+0x20], a550)
//       flags  = 0x30071f58 (EBX/pushed, the &vec3_origin address used as a flag/arg)
//       out    = &trace buffer (EAX=LEA[ESP+0x28], a546/a54a)
//       arg1   = &start point (EDX=LEA[ESP+0x4], a50e/a545)
//       arg2   = 0x30071f58 (a540) ; arg3 = cg_snap->ps.psClientNum ([EAX+0xe0], a539)
//   Result read (a559/a55d): contents = trace.contents (dword @+0x20);
//     entityNum = (uint16)trace.entityNum (MOVZX word @+0x28).
//   a565 TEST contents,0x211 ; a56b JZ -> skip the vehicle redirect when none of
//     the hittable-contents bits (0x1|0x10|0x200) are set.
//   a56d..a582 vehicle redirect: idx = entityNum; if cg_entities[idx].eType == 12
//     (ET_VEHICLE) then entityNum = cg_entities[idx].vehicleEntityNum
//     (CMP [idx*0x288 + 0x3048c6e4],0xc ; MOV ECX,[idx*0x288 + 0x3048c754], where
//     0x3048c6e0 is the cg_entities[] base, +0x4 == eType, +0x74 == vehicleEntityNum).
//   a584..a595 gate: if entityNum == cg_snap->ps.psClientNum -> skip (JZ); if
//     entityNum >= 0x40 (MAX_CLIENTS, signed JGE) -> skip.
//   a597..a5a2 latch: cg_crosshairEntNum = entityNum ; cg_crosshairEntTime = cg_time.
//   a5a7 ADD ESP,0x48 ; RET (no return value; EBX callee-saved via PUSH/POP).
//
// TRACE-BUFFER NOTE: the buffer is the shared 48-byte trace_t. Recomputing the
// live-push stack offsets proves CG_Trace stamps entityNum at +0x28;
// this function then reads contents at +0x20 and that same entityNum field.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stddef.h>
#include <stdint.h>

/* Q3/CoD MAX_CLIENTS: the crosshair name is shown only for real players. The trace
 * entityNum is latched only when it is below this (signed CMP entityNum,0x40 / JGE
 * at 0x3001a592). The shared limit retains that retail default. */

/* Trace/aim distance: the endpoint is cg_refdef.vieworg + CG_CROSSHAIR_TRACE_DIST *
 * cg_refdef.viewaxis[0]. Read from the .rdata float at 0x3007c14c (8192.0f), FMUL'd
 * into each direction component at 0x3001a4de/512/52b. */
#define CG_CROSSHAIR_TRACE_DIST 8192.0f

/* Trace mask handed to CG_Trace (EAX = 0x2000211 at 0x3001a54b). A
 * shot/aim contents mask (CONTENTS_SOLID | body/hittable content bits); the exact
 * CoD CONTENTS_* decomposition is unconfirmed, so the full mask is kept as a named
 * machine-width constant. */
#define CG_CROSSHAIR_TRACE_MASK ((int32_t)0x2000211)

/* Subset of contents bits (0x1 | 0x10 | 0x200) the result is retested against at
 * 0x3001a565 (TEST contents,0x211) to decide whether a hit warrants the vehicle
 * occupant redirect. Named by role; exact CONTENTS_* names unconfirmed. */
#define CG_CROSSHAIR_HITTABLE_CONTENTS ((uint32_t)0x211)

/* CG_Trace receives the address of the shared zero vec3 vec3_origin
 * (0x30071f58, {0,0,0}) both as its `flags` register arg (EBX = 0x30071f58 at
 * 0x3001a518) and as one stack arg (PUSH 0x30071f58 at 0x3001a540). */

void CG_ScanForCrosshairEntity(void)
{
    vec3_t start;
    vec3_t end;
    trace_t trace;
    snapshot_t *snap;
    long double endX;
    long double endY;
    long double endZ;
    float originX;
    float originY;
    float originZ;
    int32_t passEntityNum;
    int32_t contents;
    int32_t entityNum;

    /* 0x3001a4d3..0x3001a535: preserve the staggered source reads and three
     * independent x87 chains; the initial cg_snap pointer is captured while
     * end.x is live, before the y/z endpoint work. */
    endX = (long double)cg_refdef.viewaxis[0][0];
    originX = cg_refdef.vieworg[0];
    endX *= (long double)CG_CROSSHAIR_TRACE_DIST;
    originY = cg_refdef.vieworg[1];
    originZ = cg_refdef.vieworg[2];
    start[0] = originX;
    endX += (long double)start[0];
    snap = cg_snap;
    start[1] = originY;
    start[2] = originZ;
    end[0] = (float)endX;

    endY = (long double)cg_refdef.viewaxis[0][1];
    endY *= (long double)CG_CROSSHAIR_TRACE_DIST;
    endY += (long double)start[1];
    end[1] = (float)endY;

    endZ = (long double)cg_refdef.viewaxis[0][2];
    endZ *= (long double)CG_CROSSHAIR_TRACE_DIST;
    endZ += (long double)start[2];
    end[2] = (float)endZ;
    passEntityNum = snap->ps.psClientNum;

    /* 0x3001a539..0x3001a554: CG_Trace into the local trace buffer.
     * Register/stack ABI (see header): handle=CG_CROSSHAIR_TRACE_MASK, origin=end,
     * flags=&vec3_origin, out=&trace, arg1=&start, arg2=&vec3_origin,
     * arg3=cg_snap->ps.psClientNum. */
    CG_Trace(CG_CROSSHAIR_TRACE_MASK, end,
                       vec3_origin,
                       &trace,
                       start,
                       vec3_origin,
                       passEntityNum);

    /* 0x3001a559/0x3001a55d: read the hit contents and the traced entity number. */
    contents = trace.contents;
    entityNum = (int32_t)(uint16_t)trace.entityNum;

    /* 0x3001a565..0x3001a582: when the hit is against hittable contents and the
     * hit entity is a vehicle (eType == ET_VEHICLE, 12), redirect the crosshair
     * target to the vehicle's occupant (currentState.vehicleEntityNum). The index
     * is the raw trace entity number into cg_entities[] (base 0x3048c6e0,
     * stride 0x288). */
    if ((uint32_t)contents & CG_CROSSHAIR_HITTABLE_CONTENTS) {
        const centity_t *cent =
            cg_entities + entityNum;
        if (cent->currentState.eType == ET_VEHICLE) {
            entityNum = cent->currentState.vehicleEntityNum;
        }
    }

    /* 0x3001a584..0x3001a5a2: latch the crosshair target unless it is the local
     * player (entityNum == cg_snap->ps.psClientNum) or a non-client entity
     * (entityNum >= MAX_CLIENTS, signed compare). */
    snap = cg_snap;
    if (entityNum != snap->ps.psClientNum && entityNum < MAX_CLIENTS) {
        uint32_t crosshairTime = cg_time;

        cg_crosshairEntNum = entityNum;
        cg_crosshairEntTime = coduo_int32_from_bits(crosshairTime);
    }
    /* 0x3001a5a7: RET (no return value). */
}
