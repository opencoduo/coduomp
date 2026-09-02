// Source: uo_cgame_mp_x86.dll 0x30040810..0x30041541
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30040810_30041541.mcode
//
// CG_CalcVehicleViewPos — compute the local player's vehicle-view camera origin
// (cg_refdef.vieworg) and view angles by resolving the vehicle's DObj bone tags and
// world-tracing the camera. The ghidra size-guess name "PM_SlideMove" is REJECTED
// (this has no velocity/gravity slide-clip loop; it is a camera-placement routine
// driven by cgame trap 0xa5 / DObj bone matrices / CG_Trace).
//
// x87 FIDELITY: the dense blend arms are transcribed one C statement per
// FLD/FMUL/FADD/FSTP so the exact x87 evaluation order and per-operation rounding are
// preserved. Named scalar temporaries mirror the x87 stack slots; this is the literal
// machine computation, not a simplification. Multiplies by 0.0f (the .rdata constant
// at 0x3007bcec) are kept because they are what the machine executes.
//
// The view-angle output at 0x30487ac8..0x30487ad0 is the contiguous
// cg_refdefViewAngles vec3. The pointer passed to AnglesToAxisNegRight at 0x30040f1c
// proves the three components are one source-level object.
//
// The formerly unresolved 0x30040eef..0x300411c6 arm is now proven with the
// reconstructed AnglesToAxisNegRight callee at 0x3004c200: it projects the tag delta
// into the tag basis, rebuilds that displacement in the current view basis, applies
// the mode/ADS vertical adjustments, and clamps the displacement length to the
// interval [0.5, 2.0] times the source tag-delta length.

#include <math.h>
#include <string.h>

#include "client/cgame/client_recovered.h"
#include "compat/coduo_native_x87.h"
#include "client/cgame/globals.h"

/* cgame trap id issued with the adjusted view-angle triple in the gunner arm is the
 * shared CG_VEH_VIEW_ANGLE_DELTA (0xf7) declared in client_recovered.h. */

enum {
    VEHICLE_VIEW_MODE_SEAT = 1,
    VEHICLE_VIEW_MODE_GUNNER = 2,
    VEHICLE_VIEW_MODE_PASSENGER = 3,
    VEHICLE_VIEW_MODE_CHASE_ADS = 5,
    VEHICLE_TYPE_PRIMARY = 1,
    CG_VEHICLE_VIEW_TRACE_MODEL = 0x211,
    CG_VEHICLE_VIEW_RETRACE_MODEL = 0x11
};

#define CG_VEHICLE_VIEW_SWAY_FLAG ((uint32_t)0x00000200)
#define CG_VEHICLE_VIEW_TRACE_BODY_FLAG ((uint32_t)0x00000200)


void CG_CalcVehicleViewPos(void)
{
    /* trace box mins/maxs at [ESP+0xec..0x100] = {-8,-8,-8}/{8,8,8}. */
    vec3_t traceMins = {-8.0f, -8.0f, -8.0f};
    vec3_t traceMaxs = {8.0f, 8.0f, 8.0f};

    /* Entry gate: draw the vehicle view only when the local player's entity-state
     * flags carry EF_IN_VEHICLE (0x100000). */
    if ((cg_predictedPlayerState.entityStateFlags & EF_IN_VEHICLE) == 0) {
        return;
    }

    int32_t entnum = cg_predictedPlayerState.viewLockedEntityNum;
    if (entnum == ENTITYNUM_NONE) {
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)entnum >= (uint32_t)MAX_GENTITIES) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "CG_CalcVehicleViewPos: invalid view-lock entity %i",
                  entnum);
        return;
    }
    centity_t *cent = &cg_entities[entnum];

    /* dobj = (int32_t)cgame_syscall(CG_DOBJ_GET_HANDLE, cent->currentState.number). */
    void *dobj = (void *)(intptr_t)cgame_syscall(CG_DOBJ_GET_HANDLE, cent->currentState.number);
    if (dobj == NULL) {
        return;
    }

    CG_CalcEntityLerpPositions(cent);

    int32_t viewMode = cent->currentState.stateFilter; /* +0x88: vehicle view/seat mode */

    /* Two DObj bone matrices live on the stack. Each is the float[16] world matrix
     * CG_DObjGetWorldTagMatrix writes; only the first three components of each
     * row are used, and row 3 is the translation. */
    DObjSkelMat primaryMat;   /* [ESP+0x38..0x74]: axis rows then origin */
    DObjSkelMat secondaryMat; /* [ESP+0xa8..0xe4]: axis rows then origin */

    /* -------------------------------------------------------------------- *
     * Dispatch (1): viewMode == 5 && adsFraction == 0 -> tag_chasecam copy  *
     * 0x300408d2: FUCOMPP vs 0.0 with TEST AH,0x44 / JP 0x30040932 — the JP *
     * (exactly-one-of-C3/C2) skip fires on any NONZERO or NaN adsFraction,  *
     * so the chasecam copy runs only when adsFraction is exactly 0.0.       *
     * -------------------------------------------------------------------- */
    if (viewMode == VEHICLE_VIEW_MODE_CHASE_ADS && cg_predictedPlayerState.adsFraction == 0.0f) {
        if (CG_DObjGetWorldTagMatrix(dobj, "tag_chasecam", cent, &secondaryMat)) {
            cg_refdef.vieworg[0] = secondaryMat.origin[0];
            cg_refdef.vieworg[1] = secondaryMat.origin[1];
            cg_refdef.vieworg[2] = secondaryMat.origin[2];
        }
        goto trace_tail;
    }

    /* --------------------------------------------------------------- *
     * Dispatch (2): cg_predictedPlayerState.vehiclePosition == 2 (gunner)   *
     * blend arm 0x30040941..0x30040d2c — transcribed faithfully.      *
     * --------------------------------------------------------------- */
    if (cg_predictedPlayerState.vehiclePosition == VEHICLE_VIEW_MODE_GUNNER) {
        /* Copy the whole predicted player state (0x1141 dwords from
         * cg_predictedPlayerState) into a local scratch that the sway core mutates; the
         * "thompson_MP" weapon index overwrites one slot (copy+0xe4). */
        playerState_t psCopy;
        memcpy(&psCopy, &cg_predictedPlayerState, sizeof psCopy);

        int32_t weaponIndex = BG_GetWeaponIndexForName("thompson_MP");
        /* MOV [ESP+0x1f4],ECX at 0x30040983 with three args pushed (ESP=frame-0xc)
         * lands at frame+0x1e8 == copy base (frame+0x110) + 0xd8 == currentWeapon. */
        psCopy.currentWeapon = weaponIndex;

        /* BG_CalculateWeaponPosition_Sway: ESI=&psCopy, EDI=out_angles,
         * EAX=out_position; the remaining three logical parameters are pushed as
         * (previous_view_angles, 2.0f, cg_frametime). */
        BG_CalculateWeaponPosition_Sway(&psCopy, cg_vehicleViewSwayPreviousViewAngles, cg_vehicleViewSwayOffset,
                                        cg_vehicleViewSwayViewAngles, 2.0f, cg_frametime);

        /* oscInt = cg_time % 100 folded to <=50; oscFactor = oscInt * 0.02f (0x3007c2b8). */
        int32_t oscInt = coduo_int32_from_bits(cg_time) % 100;
        if (oscInt > 50) {
            oscInt = 100 - oscInt;
        }
        float oscFactor = (float)((long double)oscInt * (long double)0.019999999552965164f);
        /* Bare FILD @0x300409b0 feeds FMUL 0x3007c2b8 directly; the only
         * binary32 rounding is the FSTP into the oscFactor stack slot. */

        const char *originTag = CG_GetVehicleViewPosOriginTag(cg_predictedPlayerState.vehiclePosition);
        if (!CG_DObjGetWorldTagMatrix(dobj, originTag, cent, &primaryMat)) {
            return;
        }

        /* cg_refdef.vieworg = primaryMat.trans. */
        cg_refdef.vieworg[0] = primaryMat.origin[0];
        cg_refdef.vieworg[1] = primaryMat.origin[1];
        cg_refdef.vieworg[2] = primaryMat.origin[2];

        /* offs[] = rotated view offset. Two producing branches, then a common blend. */
        long double offsX;
        long double offsY;
        float offsZ;
        if (!CG_DObjGetWorldTagMatrix(dobj, "tag_secondary_view", cent, &secondaryMat)) {
            /* Branch A (no tag_secondary_view): offs[i] =
             *   up[i]*6.5 + (right[i]*0.0 + fwd[i]*(-20.0)).
             *   0x3007c2b4=-20.0, 0x3007c2b0=6.5, 0x3007bcec=0.0. */
            long double txm20 = (long double)primaryMat.axis[0][0] * -20.0L;
            long double tym20 = (long double)primaryMat.axis[0][1] * -20.0L;
            float tzm20 = (float)((long double)primaryMat.axis[0][2] * -20.0L);
            float cx = (float)((long double)primaryMat.axis[1][0] * 0.0L + txm20);
            float cy = (float)((long double)primaryMat.axis[1][1] * 0.0L + tym20);
            float cz = (float)((long double)primaryMat.axis[1][2] * 0.0L + (long double)tzm20);
            offsX = (long double)primaryMat.axis[2][0] * 6.5L + (long double)cx;
            offsY = (long double)primaryMat.axis[2][1] * 6.5L + (long double)cy;
            offsZ = (float)((long double)primaryMat.axis[2][2] * 6.5L + (long double)cz);
        } else {
            /* Branch B (tag_secondary_view present): offs[i] = secondary.trans[i] -
             * primary.trans[i]. */
            offsX = (long double)secondaryMat.origin[0] - (long double)primaryMat.origin[0];
            offsY = (long double)secondaryMat.origin[1] - (long double)primaryMat.origin[1];
            offsZ = (float)((long double)secondaryMat.origin[2] - (long double)primaryMat.origin[2]);
        }

        /* Optional lean/recoil: when cg_snap->ps.entityStateFlags bit 0x200 is set
         * AND cent->currentState.hudTagMask == 0, add fwd[i]*(oscFactor*-0.5) to each offs[i].
         * 0x3007bf50 = -0.5. */
        if ((cg_snap->ps.entityStateFlags & CG_VEHICLE_VIEW_SWAY_FLAG) != 0 && cent->currentState.hudTagMask == 0) {
            long double half = (long double)oscFactor * -0.5L;
            offsX += (long double)primaryMat.axis[0][0] * half;
            offsY += (long double)primaryMat.axis[0][1] * half;
            offsZ = (float)((long double)offsZ + (long double)primaryMat.axis[0][2] * half);
        }

        /* Rotate offs by the smoothed weapon-sway rotation scalars and add to
         * cg_refdef.vieworg. matCopy holds the primary fwd/right/up rows (fed to
         * AxisToAngles). spa scalars: 0x300dc9dc/e0, and -0x300dc9e4 (FCHS). */
        float matCopy[3][3];
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                matCopy[r][c] = primaryMat.axis[r][c];
            }
        }

        float spa0 = cg_vehicleViewSwayOffset[0];
        float spa1 = cg_vehicleViewSwayOffset[1];
        float nspa2 = -cg_vehicleViewSwayOffset[2];

        /* Pass 1: fwd*spa0 + offs (FADD into the offs stack slots). */
        float rx = (float)((long double)primaryMat.axis[0][0] * (long double)spa0 + offsX);
        float ry = (float)((long double)primaryMat.axis[0][1] * (long double)spa0 + offsY);
        long double rz = (long double)primaryMat.axis[0][2] * (long double)spa0 + (long double)offsZ;
        /* Pass 2: + right*spa1. */
        float sx = (float)((long double)primaryMat.axis[1][0] * (long double)spa1 + (long double)rx);
        float sy = (float)((long double)primaryMat.axis[1][1] * (long double)spa1 + (long double)ry);
        long double sz = (long double)primaryMat.axis[1][2] * (long double)spa1 + rz;
        /* Pass 3: + up*(-spa2). */
        float dispX = (float)((long double)primaryMat.axis[2][0] * (long double)nspa2 + (long double)sx);
        float dispY = (float)((long double)primaryMat.axis[2][1] * (long double)nspa2 + (long double)sy);
        long double dispZ = (long double)primaryMat.axis[2][2] * (long double)nspa2 + sz;

        cg_refdef.vieworg[0] = (float)((long double)dispX + (long double)cg_refdef.vieworg[0]);
        cg_refdef.vieworg[1] = (float)((long double)dispY + (long double)cg_refdef.vieworg[1]);
        cg_refdef.vieworg[2] = (float)(dispZ + (long double)cg_refdef.vieworg[2]);

        /* View angles from the primary basis. */
        vec3_t viewAngles;
        AxisToAngles((const vec_t(*)[3])matCopy, viewAngles);

        /* cg_refdefViewAngles[0] = viewAngles.pitch;
         * cg_refdefViewAngles[2] = viewAngles.roll. */
        cg_refdefViewAngles[0] = viewAngles[0];
        cg_refdefViewAngles[2] = viewAngles[2];
        /* MOV [ESP+0x34],0 at 0x30040c64 (ESP=frame-4 after the PUSH) zeroes
         * frame+0x30 == viewAngles[2] itself, right after the roll copy above. */
        viewAngles[2] = 0.0f;

        /* angleDelta = AngleNormalize180(cg_predictedPlayerState.viewAngles.pitch - viewAngles.pitch).
         * 0x304832ac is FLD'd as a float (cg_predictedPlayerState.viewAngles[0]). */
        float angleDelta = cg_predictedPlayerState.viewAngles[0] - viewAngles[0];
        angleDelta = AngleNormalize180(angleDelta);

        /* When |angleDelta| > 0.4 (0x3007c2a8 double), fold the shared BAMS
         * deltaAngles into viewAngles IN PLACE (the FSUBR/FSTP targets at
         * 0x30040c99..0x30040cc3 are frame+0x28/0x2c/0x30 with ESP=frame-8 —
         * exactly the viewAngles triple) and issue the view-angle trap with
         * &viewAngles (the LEA EAX,[ESP+0x28] pushed at 0x30040c8d). */
        {
            float fabsDelta = fabsf(angleDelta);
            if (fabsDelta > (double)0.4f) { /* 0x3007c2a8 == 0x3fd99999a0000000 == (double)0.4f, NOT double 0.4 */
                const float bams = 0.0054931640625f; /* 0x3007bd5c = 0x3bb40000 (360/65536) */
                viewAngles[0] =
                    (float)((long double)viewAngles[0] - (long double)coduo_int32_from_bits(cg_predictedPlayerState.deltaAngles[0]) *
                                                             (long double)bams); /* bare FILD @0x30040c83 -> FMUL bams */
                viewAngles[1] =
                    (float)((long double)viewAngles[1] - (long double)coduo_int32_from_bits(cg_predictedPlayerState.deltaAngles[1]) *
                                                             (long double)bams); /* bare FILD @0x30040ca1 */
                viewAngles[2] = (float)(-((long double)coduo_int32_from_bits(cg_predictedPlayerState.deltaAngles[2]) *
                                          (long double)bams)); /* bare FILD @0x30040cb5; FCHS @0x30040cc1 */
                cgame_syscall(CG_VEH_VIEW_ANGLE_DELTA, (intptr_t)viewAngles);
            }
        }

        /* Second lean-driven view-angle blend, gated on the same snap bit / hudTagMask. */
        if ((cg_snap->ps.entityStateFlags & CG_VEHICLE_VIEW_SWAY_FLAG) != 0 && cent->currentState.hudTagMask == 0) {
            /* 0x30040ce8: FLD [ESP+0x34] (ESP=frame) == frame+0x34 == the oscFactor
             * slot (FSTP'd at 0x300409c4 with ESP=frame-4); cg_refdefViewAngles[0] -=
             * oscFactor*0.5 (0x3007bce8 == 0.5). */
            cg_refdefViewAngles[0] = cg_refdefViewAngles[0] - oscFactor * 0.5f;
            /* cg_refdefViewAngles[1] += Q_SwayRand(2.334f,3.198f,cg_time)
             * * 0.25f (0x3007be58 = 0x3e800000; 0.140625f is the PRECEDING dword
             * 0x3007be54). Arg order arg1=2.334 (0x40156417), arg2=3.198 (0x404ca9d3),
             * arg3=cg_time. */
            float osc = Q_SwayRand(2.3342339992523193f, 3.1978652477264404f, (float)coduo_int32_from_bits(cg_time));
            cg_refdefViewAngles[1] = osc * 0.25f + cg_refdefViewAngles[1];
        }

        goto trace_tail;
    }

    /* --------------------------------------------------------------- *
     * Dispatch (3): seat / turret / aimdown selector                  *
     * --------------------------------------------------------------- */
    {
        /* seat_selector gate (0x30040d79..0x30040db3):
         *   if (vehicleType==1 && ps.vehiclePosition==3) -> seat_selector
         *     (CMP ECX,0x3 at 0x30040d82 — ECX is ps.vehiclePosition, loaded from
         *      0x304837d8 at 0x30040932 and never reloaded; NOT cent->currentState.stateFilter)
         *   else adsFlag = (adsFraction==0)?1:0; seatFlag = (viewMode!=1)?1:0;
         *        if (adsFlag != seatFlag) -> seat_selector.
         *   (0x30040d8b FUCOMPP/TEST AH,0x44: JP 0x30040da7 -> EAX=0 on ads!=0/NaN,
         *    fallthrough MOV EAX,1 on ads==0.) */
        qboolean gotoSeatSelector = qfalse;
        if (cg_predictedPlayerState.vehicleType == VEHICLE_TYPE_PRIMARY &&
            cg_predictedPlayerState.vehiclePosition == VEHICLE_VIEW_MODE_PASSENGER) {
            gotoSeatSelector = qtrue;
        } else {
            int32_t adsFlag = (cg_predictedPlayerState.adsFraction == 0.0f) ? 1 : 0;
            int32_t seatFlag = (viewMode != VEHICLE_VIEW_MODE_SEAT) ? 1 : 0;
            if (adsFlag != seatFlag) {
                gotoSeatSelector = qtrue;
            }
        }

        if (!gotoSeatSelector) {
            /* Seatless (turret/chasecam) arm 0x30040db9..0x300411c6. */
            cg_refdefViewAngles[2] = 0.0f; /* MOV [0x30487ad0],0 */
            /* 0x30040dbe: MOV EAX,ECX — the selector is ps.vehiclePosition
             * (still live in ECX from 0x30040932), not viewMode. */
            const char *originTag = CG_GetVehicleViewPosOriginTag(cg_predictedPlayerState.vehiclePosition);
            if (!CG_DObjGetWorldTagMatrix(dobj, originTag, cent, &primaryMat)) {
                return;
            }

            vec3_t deltaVec;
            int32_t stFilter = cent->currentState.stateFilter;
            if (CG_DObjGetWorldTagMatrix(dobj, "tag_chasecam", cent, &secondaryMat)) {
                /* delta[i] = secondary.trans[i] - primary.trans[i]; if stFilter==1
                 * scale by 2.5 (0x3007be68 = 0x40200000; 127.0f is the earlier
                 * dword 0x3007be60). */
                deltaVec[0] = secondaryMat.origin[0] - primaryMat.origin[0];
                deltaVec[1] = secondaryMat.origin[1] - primaryMat.origin[1];
                deltaVec[2] = secondaryMat.origin[2] - primaryMat.origin[2];
                if (stFilter == VEHICLE_VIEW_MODE_SEAT) {
                    deltaVec[0] = deltaVec[0] * 2.5f;
                    deltaVec[1] = deltaVec[1] * 2.5f;
                    deltaVec[2] = deltaVec[2] * 2.5f;
                }
            } else {
                /* delta[i] = up[i]*32.0 + (right[i]*0.0 + fwd[i]*(-200.0)).
                 * 0x3007c2a4=-200.0, 0x3007bcec=0.0, 0x3007bdd0=32.0. */
                float txm = primaryMat.axis[0][0] * -200.0f;
                float tym = primaryMat.axis[0][1] * -200.0f;
                float tzm = primaryMat.axis[0][2] * -200.0f;
                float cx = primaryMat.axis[1][0] * 0.0f + txm;
                float cy = primaryMat.axis[1][1] * 0.0f + tym;
                float cz = primaryMat.axis[1][2] * 0.0f + tzm;
                deltaVec[0] = primaryMat.axis[2][0] * 32.0f + cx;
                deltaVec[1] = primaryMat.axis[2][1] * 32.0f + cy;
                deltaVec[2] = primaryMat.axis[2][2] * 32.0f + cz;
            }

            /* 0x30040eef..0x30040f71: express deltaVec in the primary tag basis.
             * Each dot product is evaluated z, y, x on x87, with the two FADDPs
             * retaining extended precision until the final FSTP. */
            vec3_t tagSpaceDelta;
            tagSpaceDelta[0] = (float)((long double)deltaVec[2] * (long double)primaryMat.axis[0][2] +
                                       (long double)deltaVec[1] * (long double)primaryMat.axis[0][1] +
                                       (long double)deltaVec[0] * (long double)primaryMat.axis[0][0]);
            tagSpaceDelta[1] = (float)((long double)deltaVec[2] * (long double)primaryMat.axis[1][2] +
                                       (long double)deltaVec[1] * (long double)primaryMat.axis[1][1] +
                                       (long double)deltaVec[0] * (long double)primaryMat.axis[1][0]);
            tagSpaceDelta[2] = (float)((long double)deltaVec[2] * (long double)primaryMat.axis[2][2] +
                                       (long double)deltaVec[1] * (long double)primaryMat.axis[2][1] +
                                       (long double)deltaVec[0] * (long double)primaryMat.axis[2][0]);

            /* 0x30040f0b/11/25 installs the primary tag translation before the
             * orientation conversion and displacement rebuild. */
            cg_refdef.vieworg[0] = primaryMat.origin[0];
            cg_refdef.vieworg[1] = primaryMat.origin[1];
            cg_refdef.vieworg[2] = primaryMat.origin[2];

            /* 0x30040f75: EAX=&viewAxis, EDX=&cg.refdefViewAngles. The callee's
             * machine code proves axis[0]=forward, axis[1]=-right, axis[2]=up. */
            axis_t viewAxis;
            AnglesToAxisNegRight(viewAxis, cg_refdefViewAngles);

            /* 0x30040f7a..0x30041004: rebuild the displacement in the current
             * view basis. */
            vec3_t displacement;
            float displacementXBase = (float)((long double)viewAxis[1][0] * (long double)tagSpaceDelta[1] +
                                              (long double)viewAxis[0][0] * (long double)tagSpaceDelta[0]);
            float displacementYBase = (float)((long double)viewAxis[1][1] * (long double)tagSpaceDelta[1] +
                                              (long double)viewAxis[0][1] * (long double)tagSpaceDelta[0]);
            float displacementZBase = (float)((long double)viewAxis[0][2] * (long double)tagSpaceDelta[0]);
            displacement[0] = (float)((long double)viewAxis[2][0] * (long double)tagSpaceDelta[2] + (long double)displacementXBase);
            displacement[1] = (float)((long double)viewAxis[2][1] * (long double)tagSpaceDelta[2] + (long double)displacementYBase);
            displacement[2] = (float)((long double)viewAxis[2][2] * (long double)tagSpaceDelta[2] +
                                      (long double)viewAxis[1][2] * (long double)tagSpaceDelta[1] + (long double)displacementZBase);

            if (stFilter == VEHICLE_VIEW_MODE_GUNNER) {
                displacement[2] = displacement[2] * 0.75f;
            }

            long double candidate[3];
            candidate[0] = (long double)displacement[0] + (long double)cg_refdef.vieworg[0];
            candidate[1] = (long double)displacement[1] + (long double)cg_refdef.vieworg[1];
            candidate[2] = (long double)displacement[2] + (long double)cg_refdef.vieworg[2];

            /* 0x30041046..0x30041088: the mode-1 ADS camera is lowered along
             * viewAxis[2] by 135.0f (0x43070000 at 0x3007c2a0). Gate is the
             * FUCOMPP/TEST AH,0x44/JNP-skip-on-equal idiom: lowered whenever
             * adsFraction != 0.0 (NaN included), not only when positive. */
            if (stFilter == VEHICLE_VIEW_MODE_SEAT && cg_predictedPlayerState.adsFraction != 0.0f) {
                candidate[0] -= (long double)viewAxis[2][0] * 135.0L;
                candidate[1] -= (long double)viewAxis[2][1] * 135.0L;
                candidate[2] -= (long double)viewAxis[2][2] * 135.0L;
            }

            cg_vehicleViewSwayOrigin[0] = (float)candidate[0];
            cg_vehicleViewSwayOrigin[1] = (float)candidate[1];
            cg_vehicleViewSwayOrigin[2] = (float)candidate[2];
            cg_vehicleViewSwayPrevTime = coduo_int32_from_bits(cg_time);

            /* 0x300410b1..0x3004119e: clamp the candidate displacement length
             * against the source tag-delta length. VectorNormalize is called only
             * outside the inclusive [0.5*sourceLength, 2*sourceLength] interval,
             * exactly matching the FCOMP branches. */
            displacement[0] = (float)(candidate[0] - (long double)primaryMat.origin[0]);
            displacement[1] = (float)(candidate[1] - (long double)primaryMat.origin[1]);
            displacement[2] = (float)(candidate[2] - (long double)primaryMat.origin[2]);

            long double displacementLength = coduo_x87_sqrtl((long double)displacement[2] * (long double)displacement[2] +
                                                             (long double)displacement[1] * (long double)displacement[1] +
                                                             (long double)displacement[0] * (long double)displacement[0]);
            long double sourceLength =
                coduo_x87_sqrtl((long double)deltaVec[2] * (long double)deltaVec[2] + (long double)deltaVec[1] * (long double)deltaVec[1] +
                                (long double)deltaVec[0] * (long double)deltaVec[0]);
            float clampedLength = (float)(sourceLength + sourceLength);

            if (displacementLength > clampedLength) {
                (void)VectorNormalize(displacement);
            } else {
                clampedLength = (float)(sourceLength * 0.5L);
                /* 0x3004114e..0x30041157 also takes this path for unordered
                 * x87 comparisons, which `!(a < b)` preserves. */
                if (!(displacementLength < clampedLength)) {
                    goto copy_sway_origin;
                }
                (void)VectorNormalize(displacement);
            }

            cg_vehicleViewSwayOrigin[0] =
                (float)((long double)primaryMat.origin[0] + (long double)clampedLength * (long double)displacement[0]);
            cg_vehicleViewSwayOrigin[1] =
                (float)((long double)primaryMat.origin[1] + (long double)clampedLength * (long double)displacement[1]);
            cg_vehicleViewSwayOrigin[2] =
                (float)((long double)primaryMat.origin[2] + (long double)clampedLength * (long double)displacement[2]);

        copy_sway_origin:
            cg_refdef.vieworg[0] = cg_vehicleViewSwayOrigin[0];
            cg_refdef.vieworg[1] = cg_vehicleViewSwayOrigin[1];
            cg_refdef.vieworg[2] = cg_vehicleViewSwayOrigin[2];
            goto trace_tail;
        }

        /* seat_selector (0x300411cb): */
        if (viewMode == VEHICLE_VIEW_MODE_SEAT) {
            /* seat-1 blend (0x300411d4..0x300412ba):
             * CG_GetRiderTagName(ps.vehiclePosition) — 0x300411dc MOV EAX,ECX with
             * ECX still holding 0x304837d8 — into secondaryMat, then a
             * 12.0 (0x3007bdc4 = 0x41400000; 432.0f is the PRECEDING dword
             * 0x3007bdc0) / 32.0 (0x3007bdd0) blend. 0x3007bcec=0.0. */
            const char *riderTag = CG_GetRiderTagName(cg_predictedPlayerState.vehiclePosition);
            if (!CG_DObjGetWorldTagMatrix(dobj, riderTag, cent, &secondaryMat)) {
                /* 0x300411f0 JZ 0x300412bf: a failed rider-tag lookup FALLS INTO
                 * the tag_aimdownbarrel/tag_barrel arm, it does not trace. */
                goto aimdownbarrel_arm;
            }
            {
                /* Machine order:
                 *   ax = secondary.trans.x - secondary.fwd.x*12 (held ST)
                 *   ay = secondary.trans.y - secondary.fwd.y*12
                 *   z0 = secondary.trans.z - secondary.fwd.z*12  -> FSTP 0x30487a98
                 *   bx = right.x*0 + ax; by = right.y*0 + ay
                 *   .z = right.z*0 + z0 ; then + up*32 into each. */
                float az = secondaryMat.origin[2] - secondaryMat.axis[0][2] * 12.0f;
                cg_refdef.vieworg[2] = az; /* FSTP 0x30487a98 (z first) */
                float ax = secondaryMat.origin[0] - secondaryMat.axis[0][0] * 12.0f;
                float ay = secondaryMat.origin[1] - secondaryMat.axis[0][1] * 12.0f;
                float bx = secondaryMat.axis[1][0] * 0.0f + ax;
                float by = secondaryMat.axis[1][1] * 0.0f + ay;
                float bz = secondaryMat.axis[1][2] * 0.0f + cg_refdef.vieworg[2];
                cg_refdef.vieworg[0] = secondaryMat.axis[2][0] * 32.0f + bx; /* up.x*32 + bx */
                cg_refdef.vieworg[1] = secondaryMat.axis[2][1] * 32.0f + by;
                cg_refdef.vieworg[2] = secondaryMat.axis[2][2] * 32.0f + bz;
            }
            goto trace_tail;
        }

    aimdownbarrel_arm:
        /* viewMode != 1 (0x300412bf): tag_aimdownbarrel copy, else tag_barrel blend. */
        if (CG_DObjGetWorldTagMatrix(dobj, "tag_aimdownbarrel", cent, &secondaryMat)) {
            cg_refdef.vieworg[0] = secondaryMat.origin[0];
            cg_refdef.vieworg[1] = secondaryMat.origin[1];
            cg_refdef.vieworg[2] = secondaryMat.origin[2];
            goto trace_tail;
        }

        if (!CG_DObjGetWorldTagMatrix(dobj, "tag_barrel", cent, &secondaryMat)) {
            return;
        }
        /* tag_barrel blend (0x30041326..0x300413ea):
         *   c[i] = secondary.fwd[i]*30.0 + secondary.trans[i]    (0x3007becc =
         *          0x41f00000 = 30.0; 140.0f is the PRECEDING dword 0x3007bec8)
         *   d[i] = c[i] - secondary.right[i]*3.0                 (0x3007be5c=3.0, FSUBR)
         *   out[i] = secondary.up[i]*10.0 + d[i]                 (0x3007bda4=10.0)
         * Store order: z stored to 0x30487a98 first (FSTP), then x, then the up terms. */
        {
            float cx = secondaryMat.axis[0][0] * 30.0f + secondaryMat.origin[0];
            float cy = secondaryMat.axis[0][1] * 30.0f + secondaryMat.origin[1];
            float cz = secondaryMat.axis[0][2] * 30.0f + secondaryMat.origin[2];
            cg_refdef.vieworg[2] = cz; /* FSTP 0x30487a98 */

            float dx = cx - secondaryMat.axis[1][0] * 3.0f; /* FSUBR ST,ST(2): c - right*3 */
            float dy = cy - secondaryMat.axis[1][1] * 3.0f;
            cg_refdef.vieworg[0] = dx; /* FSTP 0x30487a90 */
            float dz = cg_refdef.vieworg[2] - secondaryMat.axis[1][2] * 3.0f; /* FSUBR 0x30487a98 */

            cg_refdef.vieworg[0] = secondaryMat.axis[2][0] * 10.0f + cg_refdef.vieworg[0]; /* up.x*10 + dx */
            cg_refdef.vieworg[1] = secondaryMat.axis[2][1] * 10.0f + dy;                  /* up.y*10 + dy */
            cg_refdef.vieworg[2] = secondaryMat.axis[2][2] * 10.0f + dz;                  /* up.z*10 + dz */
        }
        goto trace_tail;
    }

trace_tail:
    /* Resolve tag_body (0x300771f8) into secondaryMat; if absent, fall back to the
     * vehicle entity's lerpOrigin (cg_entities[cg_predictedPlayerState.viewLockedEntityNum].lerpOrigin at
     * base+0x208). The anchor's z is raised by 20.0 (0x3007be04) and traced. */
    {
        vec3_t anchor;
        if (CG_DObjGetWorldTagMatrix(dobj, "tag_body", cent, &secondaryMat)) {
            anchor[0] = secondaryMat.origin[0];
            anchor[1] = secondaryMat.origin[1];
            anchor[2] = secondaryMat.origin[2];
        } else {
            int32_t fallbackEntityNum = cg_predictedPlayerState.viewLockedEntityNum;
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
            if ((uint32_t)fallbackEntityNum >= (uint32_t)MAX_GENTITIES) {
                Com_Error(ERR_DROP,
                          "\x15"
                          "CG_CalcVehicleViewPos: invalid reloaded "
                          "view-lock entity %i",
                          fallbackEntityNum);
                return;
            }
            centity_t *vent = &cg_entities[fallbackEntityNum];
            anchor[0] = vent->lerpOrigin[0];
            anchor[1] = vent->lerpOrigin[1];
            anchor[2] = vent->lerpOrigin[2];
        }

        vec3_t traceAnchor;
        traceAnchor[0] = anchor[0];
        traceAnchor[1] = anchor[1];
        traceAnchor[2] = anchor[2] + 20.0f; /* 0x3007be04 */

        /* First projection: handle 0x211, origin = cg_refdef.vieworg, EBX = &traceMaxs.
         * The four stack arguments at 0x3004140e..0x3004142b are, in source order,
         * out=&traceResult, arg1=&traceAnchor, arg2=&traceMins, arg3=entnum. */
        trace_t traceResult;
        CG_Trace(CG_VEHICLE_VIEW_TRACE_MODEL, cg_refdef.vieworg, traceMaxs, &traceResult, traceAnchor, traceMins, entnum);

        /* Status re-trace gate: after accounting for the four live wrapper arguments,
         * [ESP+0x98] is traceResult.contents (+0x20), not normal (+0x10). */
        if (traceResult.startsolid != 0) {
            if (((uint32_t)traceResult.contents & CG_VEHICLE_VIEW_TRACE_BODY_FLAG) != 0) {
                CG_Trace(CG_VEHICLE_VIEW_RETRACE_MODEL, cg_refdef.vieworg, traceMaxs, &traceResult, traceAnchor, traceMins, entnum);
            }
        }

        /* If the trace did NOT reach the far end (fraction != 1.0, 0x3007bcf8 double),
         * clip cg_refdef.vieworg to the endpoint and re-trace. */
        if (traceResult.fraction != 1.0) {
            cg_refdef.vieworg[0] = traceResult.endpos[0];
            cg_refdef.vieworg[1] = traceResult.endpos[1];
            /* (1.0 - fraction)*32 + endpos.z is one 80-bit chain in the DLL
             * (FLD 0x3007bce0=1.0; FSUB; FMUL 0x3007bdd0=32.0; FADD; FSTP
             * @0x300414a0..e8) — no float temp for the subtract. */
            cg_refdef.vieworg[2] = (float)((1.0L - (long double)traceResult.fraction) * 32.0L + (long double)traceResult.endpos[2]);

            CG_Trace(CG_VEHICLE_VIEW_RETRACE_MODEL, cg_refdef.vieworg, traceMaxs, &traceResult, traceAnchor, traceMins, entnum);

            /* 0x30041505..0x30041529 copies traceResult.endpos (+0x04..+0x0c). */
            cg_refdef.vieworg[0] = traceResult.endpos[0];
            cg_refdef.vieworg[1] = traceResult.endpos[1];
            cg_refdef.vieworg[2] = traceResult.endpos[2];
        }
    }
}
