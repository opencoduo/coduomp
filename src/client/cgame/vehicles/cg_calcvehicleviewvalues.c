// Source: uo_cgame_mp_x86.dll 0x30040580..0x300407b1
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30040580_300407b1.mcode
//
// CG_CalcVehicleViewValues (0x30040580) — derive the local player's view/refdef
// angles while riding a vehicle/turret, by attaching to the vehicle centity's
// "tag_player" DObj bone, converting that world orientation to Euler angles,
// smoothing it against the previous frame, applying the engine's per-frame angle
// deltas, subtracting the ADS view-error wander, and committing the result into
// cg_predictedPlayerState.viewAngles. It is the angle-producing sibling of CG_CalcVehicleViewPos
// (0x30040810, which builds the vehicle view ORIGIN); the two share the identical
// entry gate (cg_predictedPlayerState.entityStateFlags & EF_IN_VEHICLE and a valid
// cg_predictedPlayerState.viewLockedEntityNum) and the same vehicle centity, angle-delta int globals, and the
// CG_VEH_VIEW_ANGLE_DELTA (0xf7).
//
// NAMING (adjudication vs the .mcode "ConvertQuatToMat" guess): the header name is a
// pure SIZE match (win 0x231 ~ 0x232) with ZERO behavioral basis and is REJECTED.
// The real ConvertQuatToMat is a different address (0x3004b7c0, reconstructed last
// batch — a leaf quaternion->matrix routine). This function converts NO quaternion:
// it reads the local player's predicted vehicle/entity state, resolves a DObj bone
// via cgame_syscall(CG_DOBJ_GET_HANDLE)/the "tag_player" world-matrix builder, runs
// the shared Matrix*/Axis*/Angles* math family, and writes the interpolated view
// angles. The symbolized Mac cgame places CG_CalcVehicleViewValues immediately
// beside CG_CalcVehicleViewPos, matching this Windows pair's order and roles.
//
// ABI: void(void). No arguments; callee-saved ESI is PUSH/POP'd around the body
// (SUB ESP,0x94 frame). Plain RET on every path.
//
// Register-argument ABI of the callees (proven at each call site and in their own
// reconstructions): CG_DObjGetWorldTagMatrix(self=ECX, tagName=EAX,
// entity=stack0, out=stack1) -> qboolean; MatrixTranspose(in=ECX, out=EAX);
// MatrixMultiply(in1=ECX, in2=EAX, out=EDX); AnglesToAxisNegRight(outAxis=EAX,
// angles=EDX); AxisToAngles(axis=ECX, angles=EAX); AnglesSubtract(a=ESI, b=EDX,
// out=ECX).
//
// Constants (exact .rdata dwords, objdump -s -j .rdata verified):
//   0x3007bcec = 0x00000000 = 0.0f          (FUCOMP sign compare of adsFraction)
//   0x3007bd5c = 0x3bb40000 = 0.0054931641f (SHORT2ANGLE = 360/65536; scales the
//                                            three FILD'd int angle-delta globals)
//   "tag_player" string @0x30072d68 (objdump: 74 61 67 5f 70 6c 61 79 65 72 00).
//
// Machine-code walk (all against the .mcode):
//   0x30040580 gate 1: cg_predictedPlayerState.entityStateFlags & EF_IN_VEHICLE
//              (0x100000) == 0 -> return.
//   0x30040596 gate 2: cg_predictedPlayerState.viewLockedEntityNum == 0x3ff (no vehicle entity) -> return.
//   0x300405a6 vehicle = &cg_entities[cg_predictedPlayerState.viewLockedEntityNum] (IMUL *0x288 + base).
//   0x300405b4 dobjHandle = cgame_syscall(CG_DOBJ_GET_HANDLE, vehicle->number).
//   0x300405c4 require vehicle->currentState.stateFilter (+0x88) == 1, else return.
//   0x300405d6 require cg_predictedPlayerState.vehiclePosition != 2, else return.
//   0x300405e3 require dobjHandle != 0, else return.
//   0x300405eb build the "tag_player" bone world DObjSkelMat into worldMatrix;
//              CG_DObjGetWorldTagMatrix(self=vehicle, "tag_player", vehicle,
//              worldMatrix). On failure -> return.
//   0x30040606 CG_CalcEntityLerpPositions(vehicle) — refresh its interpolated
//              lerpAngles (+0x214) block.
//   0x3004060f tagAxis = AnglesToAxisNegRight(vehicle->lerpAngles) into
//              [ESP+0x10] — the vehicle tag orientation basis this frame.
//   0x3004061e second gate cluster (bail out to the "save axis" path if any fail):
//              flags & EF_IN_VEHICLE clear; cg_predictedPlayerState.viewLockedEntityNum == 0x3ff;
//              vehicle->currentState.stateFilter == 5; BG_AllowPlayerWeaponAtVehiclePos(
//              cg_predictedPlayerState.vehicleType, cg_predictedPlayerState.vehiclePosition) != 0;
//              cg_predictedPlayerState.currentWeapon == 0.
//   0x30040675 smoothing: if cg_vehicleViewReset, seed cg_vehicleViewPrevAxis =
//              transpose(tagAxis) and clear the latch.
//   0x30040696 smoothedAxis[ESP+0x58] = cg_vehicleViewPrevAxis * tagAxis;
//              then cg_vehicleViewPrevAxis = transpose(tagAxis).
//   0x300406b3 viewAxis[ESP+0x34] = AnglesToAxisNegRight(cg_predictedPlayerState.viewAngles).
//   0x300406c1 composed[ESP+0x10] = viewAxis * smoothedAxis.
//   0x300406d2 derived[ESP+0x4] = AxisToAngles(composed).
//   0x300406dd if adsFraction (cg_predictedPlayerState.adsFraction) == 0.0f, commit the
//              derived pitch: cg_predictedPlayerState.viewAngles[0] = derived[0].
//   0x300406fc if flags & EF_VEHICLE_ACTIVE (0x200000), commit the derived yaw:
//              cg_predictedPlayerState.viewAngles[1] = derived[1].
//   0x30040711 delta[i] = (float)(int32_t)angleDeltaGlobal[i] * SHORT2ANGLE.
//   0x3004074c delta = AnglesSubtract(cg_predictedPlayerState.viewAngles, delta)  (viewAngles-delta).
//   0x30040758 delta = AnglesSubtract(delta, cg_adsViewErrorAngles).
//   0x3004075d cgame_syscall(CG_VEH_VIEW_ANGLE_DELTA /*0xf7*/, &delta).
//   0x3004076c finalAxis[ESP+0x10] = AnglesToAxisNegRight? — NO: MatrixMultiply(
//              in1=[ESP+0x34]=viewAxis, in2=cg_vehicleViewPrevAxis, out=[ESP+0x10]).
//   0x3004077e derived[ESP+0x4] = AxisToAngles(finalAxis).  (angles written into
//              `delta`, the same [ESP+4] buffer, via EAX=ESI=&delta.)
//   0x30040787 cg_predictedPlayerState.viewAngles[2] = -derived[2] (FLD [ESP+0xc]; FCHS; FSTP).
//              return.
//   The mid-function bailouts (0x3004079b) transpose the current tagAxis into
//   cg_vehicleViewPrevAxis before returning, so the next frame has a reference basis.
//
// NOTE on the two derive->AxisToAngles blocks: both write their three Euler outputs
// into the SAME [ESP+4] scratch (`derived`). The first block's outputs are consumed
// immediately for the pitch/yaw commit and to seed `delta`; only the SECOND block's
// roll ([ESP+0xc]) survives to the final cg_predictedPlayerState.viewAngles[2] store. Modeled as one
// reused `derived` vec3 to match the byte-exact stack reuse.

#include <stddef.h>

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* cg_entities[] — the DLL's centity array (stride 0x288 == sizeof centity_t),
 * base cg_entities (IMUL n,0x288 + 0x3048c6e0 at 0x300405a6). */

/* 0x3007bd5c = 0x3bb40000 = 360.0f / 65536.0f — the id-Tech SHORT2ANGLE (BAMS) scale
 * applied to the three FILD'd signed-int angle-delta globals. */
#define SHORT2ANGLE 0.0054931640625f

/* MAX_GENTITIES-1 sentinel: cg_predictedPlayerState.viewLockedEntityNum == 0x3ff means "no vehicle entity". */
enum {
    CG_EFFECT_ENTITY_NONE = 0x3ff
};

/* The three contiguous per-frame view-angle delta inputs (signed ints, FILD'd then
 * scaled by SHORT2ANGLE). Owned across the vehicle-view family; two carry the
 * mechanical convertquattomat owner label, one is shared with pmovesingle. */

void CG_CalcVehicleViewValues(void)
{
    /* 0x30040580 / 0x30040596: vehicle/turret view gate. */
    if ((cg_predictedPlayerState.entityStateFlags & EF_IN_VEHICLE) == 0) {
        return;
    }
    int32_t entityNum = cg_predictedPlayerState.viewLockedEntityNum;
    if (entityNum == CG_EFFECT_ENTITY_NONE) {
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)entityNum >= (uint32_t)MAX_GENTITIES) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "CG_CalcVehicleViewValues: invalid view-lock "
                  "entity %i",
                  entityNum);
        return;
    }
    centity_t *vehicle = &cg_entities[entityNum];

    /* 0x300405b4: the vehicle's DObj skeleton handle (0 => no skeleton this frame). */
    intptr_t dobjHandle = cgame_syscall(CG_DOBJ_GET_HANDLE, vehicle->currentState.number);

    /* 0x300405c4 / 0x300405d6 / 0x300405e3: require the gunner state, a
     * non-transitional vehicle position, and a valid DObj handle. */
    if (vehicle->currentState.stateFilter != 1) {
        return;
    }
    if (cg_predictedPlayerState.vehiclePosition == 2) {
        return;
    }
    if (dobjHandle == 0) {
        return;
    }

    /* 0x300405eb: attach to the vehicle's "tag_player" bone and build its world
     * matrix; the composer returns qfalse when the entity has no usable bone. */
    DObjSkelMat worldMatrix;
    /* 0x300405f6: ECX (the `self`/handle arg) = dobjHandle (0x300405c2 mov ecx,eax
     * from the CG_DOBJ_GET_HANDLE return), NOT the vehicle pointer. Inside
     * CG_DObjGetWorldTagMatrix ECX->ESI feeds the bone-index/bone-matrix traps as
     * the DObj handle. A prior pass passed `vehicle` as self; every sibling
     * (cg_calcturretviewvalues/cg_calcvehicleviewpos/cg_calcmuzzlepoint) passes the
     * handle. */
    if (!CG_DObjGetWorldTagMatrix((void *)dobjHandle, "tag_player", vehicle, &worldMatrix)) {
        return;
    }

    /* 0x30040606: refresh the vehicle's interpolated lerpAngles (+0x214). */
    CG_CalcEntityLerpPositions(vehicle);

    /* 0x3004060f: this frame's vehicle tag orientation basis. */
    axis_t tagAxis;
    AnglesToAxisNegRight(tagAxis, vehicle->lerpAngles);

    /* 0x3004061e..0x30040662: second gate cluster. On any failure, save the current
     * tagAxis (transposed) into the previous-frame basis and return. */
    if ((cg_predictedPlayerState.entityStateFlags & EF_IN_VEHICLE) == 0 ||
        cg_predictedPlayerState.viewLockedEntityNum == CG_EFFECT_ENTITY_NONE || vehicle->currentState.stateFilter == 5 ||
        BG_AllowPlayerWeaponAtVehiclePos(cg_predictedPlayerState.vehicleType, cg_predictedPlayerState.vehiclePosition) != 0 ||
        cg_predictedPlayerState.currentWeapon == 0) {
        /* 0x3004079b: MatrixTranspose(in=tagAxis, out=cg_vehicleViewPrevAxis). */
        MatrixTranspose(tagAxis, cg_vehicleViewPrevAxis);
        return;
    }

    /* 0x30040675: on a snapshot reset, reseed the previous-frame basis from the
     * current tag orientation so the first smoothed frame is a no-op. */
    if (cg_vehicleViewReset != 0) {
        MatrixTranspose(tagAxis, cg_vehicleViewPrevAxis);
        cg_vehicleViewReset = 0;
    }

    /* 0x30040696: smooth this frame's tag orientation against the previous frame's,
     * then latch the current orientation (transposed) for the next frame. */
    axis_t smoothedAxis;
    MatrixMultiply(cg_vehicleViewPrevAxis, tagAxis, smoothedAxis);
    MatrixTranspose(tagAxis, cg_vehicleViewPrevAxis);

    /* 0x300406b3: orientation basis of the current committed view angles. */
    axis_t viewAxis;
    AnglesToAxisNegRight(viewAxis, cg_predictedPlayerState.viewAngles);

    /* 0x300406c1: compose the view basis with the smoothed tag basis, then recover
     * the Euler angles of the composition. */
    axis_t composedAxis;
    MatrixMultiply(viewAxis, smoothedAxis, composedAxis);

    vec3_t derived;
    AxisToAngles(composedAxis, derived);

    /* 0x300406dd: when not aiming down sights (adsFraction == 0), commit the derived
     * pitch. FLD 0.0f; FLD adsFraction; FUCOMPP; TEST AH,0x44 — the store runs on
     * the equal (== 0.0f) case. */
    if (cg_predictedPlayerState.adsFraction == 0.0f) {
        cg_predictedPlayerState.viewAngles[0] = derived[0];
    }

    /* 0x300406fc: when EF_VEHICLE_ACTIVE is set, commit the derived yaw. */
    if ((cg_predictedPlayerState.entityStateFlags & EF_VEHICLE_ACTIVE) != 0) {
        cg_predictedPlayerState.viewAngles[1] = derived[1];
    }

    /* 0x30040711: this frame's engine-supplied angle deltas, SHORT2ANGLE-scaled.
     * FILD reads them as signed int32; the middle global is shared with pmovesingle. */
    vec3_t angleDelta;
    angleDelta[0] = (float)((long double)coduo_int32_from_bits(cg_predictedPlayerState.deltaAngles[0]) * SHORT2ANGLE);
    angleDelta[1] = (float)((long double)coduo_int32_from_bits(cg_predictedPlayerState.deltaAngles[1]) * SHORT2ANGLE);
    angleDelta[2] = (float)((long double)coduo_int32_from_bits(cg_predictedPlayerState.deltaAngles[2]) * SHORT2ANGLE);

    /* 0x3004074c: angleDelta = cg_predictedPlayerState.viewAngles - angleDelta (per-axis short-way).
     * 0x30040758: angleDelta -= cg_adsViewErrorAngles (the idle aim-wander offset). */
    AnglesSubtract(cg_predictedPlayerState.viewAngles, angleDelta, angleDelta);
    AnglesSubtract(angleDelta, cg_adsViewErrorAngles, angleDelta);

    /* 0x3004075d: hand the adjusted angle triple to the engine. */
    cgame_syscall(CG_VEH_VIEW_ANGLE_DELTA, (intptr_t)angleDelta);

    /* 0x3004076c: finalAxis = viewAxis * cg_vehicleViewPrevAxis (the just-latched
     * transposed tag basis), then recover its Euler angles. AxisToAngles writes into
     * the SAME [ESP+4] scratch as `angleDelta` (ESI still points there), so its roll
     * output is what the final store reads. */
    axis_t finalAxis;
    MatrixMultiply(viewAxis, cg_vehicleViewPrevAxis, finalAxis);
    AxisToAngles(finalAxis, angleDelta);

    /* 0x30040787: commit the negated recovered roll. FLD [ESP+0xc]; FCHS; FSTP. */
    cg_predictedPlayerState.viewAngles[2] = -angleDelta[2];
}
