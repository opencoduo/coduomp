// Source: uo_cgame_mp_x86.dll 0x30032fe0..0x30033b68
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30032fe0_30033b68.mcode
//
// CG_PlayerVehiclePositionAndBlend — position a mounted rider/gunner model on its vehicle
// and drive the vehicle's turret/gun bone angles.
//
// NAMING: the .mcode header's mechanical "# name PM_CheckDuck" is a pure win/corpus
// SIZE match (0xb88 vs 0xb58) and is REJECTED — the body performs no duck/crouch
// player-move logic. It resolves the client's manned-turret DObj, smooth-blends the
// turret gun bone angles toward the current animation frame at a per-frame
// angular-velocity limit, writes AxisToAngles(worldBoneMatrix) into both the rider
// cent's lerpAngles (+0x214) and the rider anim record's turret override angle
// triple (clientInfo_t +0x3f4), and finally transforms the rider's seat offset
// by the bone matrix and stores it as the rider's lerpOrigin origin (+0x208). The
// proven AxisToAngles(worldBoneMatrix, anim->turretOverrideAngles) call at 0x30033a77
// and the "aborting player positioning on turret" warning string prove the role.
// The Mac cgame symbol CG_PlayerVehiclePositionAndBlend shares eight distinctive
// direct callees with this body, resolving the exact source name.
//
// ARGUMENT (EBX): the rider/passenger client entity, a centity_t*:
//   +0x74 vehicleEntityNum : the vehicle entity number the rider is mounted on
//                            (gated: >= 0x40 and != 0x3ff, i.e. a real player-vehicle).
//   +0x88 stateFilter      : low 3 bits select the rider's seat role (1..6); 0 and 7
//                            are rejected (0 < (stateFilter & 7) < 7).
//   +0x94 clientNum        : row into bgs.clientinfo[] (stride 0x4d0, base
//                            0x305e1f34) for the rider's per-client anim/turret state.
// EBX is a register argument (the callers hold the rider cent in EBX). No
// calling-convention attribute is added (syntax-only build). Returns qboolean:
// qtrue only when the rider was actually positioned; qfalse on every early-out.
//
// STRUCT NOTE: the 0x4d0-stride table at 0x305e1f34 is the live
// bgs.clientinfo[] storage, not the per-corpse clientInfo_t storage at
// 0x3044cb00. This function's EDI accesses map to clientInfo_t fields:
// +0x390 legsAnimWord, +0x394 legsAnimEntryWord, +0x3f4 turretOverrideAngles,
// +0x4c4 animTree.
//
// CONSTANTS (verified via objdump -s -j .rdata / .data):
//   0x3007bce0 = 1.0f   0x3007bce8 = 0.5f   0x3007bcec = 0.0f   0x3007bcf8 = 1.0 (dbl)
//   0x3007be88 = 1000.0f (angular-rate scale; NOT 0.85f — see below)  0x3007c2c0 = 1.0/60 (dbl)
//   0x3007c2c8 = 1.0f/60   0x304831ac = cg_frametime (int ms, FILD)
//   0x304831a8 = cg_frameInterpolation (float)   0x304832a4 = cg_predictedPlayerState.adsFraction
//   0x304831c0 = the cg_thirdPerson view/render gate
//   0x30459160 = cg_snap (cg_snap->ps.psClientNum at +0xe0)
//   0x305e1f08 = bgs.animationTable.animTreeHandle   0x3048c6e0 = cg_entities base
//
// ANGULAR-RATE CORRECTION: an earlier partial spec claimed the per-bone blend rate was
// |diff| * (0.85f / cg_frametime) with 0.85f at .rdata 0x3007be88. That is WRONG on both
// counts. objdump -s -j .rdata 0x3007be80..0x3007be90 gives 0x3007be84 = 0.8500000238f
// and 0x3007be88 = 1000.0f. Every one of the nine blend sites is `FDIVR [0x3007be88]`
// (divide-reverse by 1000.0f), and 0.85f (0x3007be84) is never referenced in this
// function. The proven rate is |diff| * (1000.0f / cg_frametime); see the
// compatibility spelling below.
//
// FULLY RECONSTRUCTED. The four per-mode turret-bone-angle blend blocks (Block A
// 0x30033083, Block B 0x30033366, Block C 0x3003355e, Block D 0x3003374c) are
// byte-transcribed from the mcode/objdump: the DObj anim-node bisection (trap_XAnimGetNumChildren/186),
// the rate-limited blend step, and every per-channel trap_XAnimSetGoalWeight override with its
// exact cg-frame stack-slot bindings (each [ESP+X] resolved to an absolute frame offset
// under the interleaved pushes). ESP-tracked facts (F = frame base after the
// SUB ESP,0x84; the tail baseline T = F-0xc under the three prologue pushes):
//   - every block packs the Scr_GetAnimsIndex return into the HIGH u16 of the anim
//     handle: MOV word [ESP+0x16],AX (F+0x6) then MOV word [ESP+0x18],AX/DX (F+0x4)
//     and MOV EBP,[ESP+0x18] loads the (treeHandle<<16)|(legsAnimWord&0xfdff) dword
//     (0x300330db/f0, 0x300333bd/ca, 0x300335a3/b0, 0x30033788/9d) — the same
//     rootNodePacked idiom as CG_PlayerTurretPositionAndBlend;
//   - Block A's sub-frame fraction is computed FROM the LerpAngle result (both
//     0x3003316a FLD [ESP+0x18] at ESP=T-4 and 0x3003317b FLD [ESP+0x14] at ESP=T
//     resolve to F+0x8, the 0x3003309c FSTP slot) — the result is NOT dead;
//   - Block A channel 0 samples AND writes the sampleNode bone (F+0x4, reloaded at
//     0x30033267); the resolved frameNode lives at F+0xc (0x30033200) and is used
//     only by channel 1 (0x30033293);
//   - the common tail's local-player skip compares rider->currentState.clientNum (the
//     prologue-seeded slot F+0x18 that [ESP+0x24] aliases at the tail ESP depth).
// All four blocks and the tail are byte-verified.

#include "../client_recovered.h"
#include "../globals.h"

#include <string.h>

#if defined(_MSC_VER)
#define CG_VEHICLE_COMPAT_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define CG_VEHICLE_COMPAT_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define CG_VEHICLE_COMPAT_ALWAYS_INLINE inline
#endif

/*
 * cgame_syscall — the cgame VM trap entry (function pointer at .data 0x30085e9c),
 * used directly here to query the vehicle's DObj handle (trap CG_DOBJ_GET_HANDLE).
 * Its canonical cgame_syscall_t declaration is in globals.h.
 */

/*
 * NOT_FROM_ORIGINAL_SOURCE: source spelling of the inline rate-limited per-bone
 * blend fraction shared by all four
 * blocks (idiom at 0x3003322a..0x3003324f and eight sibling sites):
 *
 *   rate   = |sample - reference| * (1000.0f / cg_frametime);   // FABS; FILD ft;
 *                                                               // FDIVR 1000.0f; FMULP
 *   result = (rate > 0.0f) ? (1.0f / rate) : 0.0f;              // FCOM 0.0; TEST AH,0x41
 *
 * The divisor is .rdata 0x3007be88 = 1000.0f (verified by objdump; 0.85f at 0x3007be84
 * is never referenced here). The reciprocal branch is FLD 1.0f; FDIV st0,st1 (1.0/rate);
 * the zero branch is a stored 0.0f. cg_frametime is FILD'd from .data 0x304831ac.
 */
static CG_VEHICLE_COMPAT_ALWAYS_INLINE float cgame_compat_turret_blend_step(float sample, float reference)
{
    /* diff/mag/rate live in 80-bit x87 registers in the DLL: FSUB reference; FABS;
     * FILD ft; FDIVR 1000.0f; FMULP (0x30033223..0x30033238) never store to a float
     * slot. Only the reciprocal result is FSTP'd once (0x3003324f). long double keeps
     * them unrounded; float locals would round three intermediates the DLL does not.
     * cg_frametime enters via a bare FILD (0x3003322c) with no FSTP DWORD before the
     * FDIVR, so no (float) cast (Class 4). */
    long double diff = (long double)sample - (long double)reference;      /* FSUB reference */
    long double mag = __builtin_fabsl(diff);             /* FABS */
    long double rate = mag * (1000.0L / (long double)cg_frametime);       /* FILD ft; FDIVR 1000.0f; FMULP */
    return rate > 0.0L ? (float)(1.0L / rate) : 0.0f;     /* FCOM 0.0; TEST AH,0x41; JNZ */
}

/*
 * NOT_FROM_ORIGINAL_SOURCE: source spelling of the DObj anim-node bisection shared
 * by all four blocks
 * (0x300330f9-region and its B/C/D twins). It clears the emitter subtree strictly,
 * then
 * bisects the current animation node twice:
 *   childCountA = trap_XAnimGetNumChildren(animNumber);   node = trap_XAnimGetChildAt(animNumber, childCountA/2);
 *   childCountB = trap_XAnimGetNumChildren(node);         sample = trap_XAnimGetChildAt(node, childCountB/2);
 * erroring via Com_Error if either node has no children. It returns the intermediate
 * mid-grandchild frame node (the "sample" node used as the trap_XAnimGetWeight bone index and as
 * the base-reset trap_XAnimSetGoalWeight index) and writes the mid-child node through *outNode (the
 * node used for the final channel). Blocks B/C/D use exactly this and stop here; Block A
 * calls CG_TurretBoneFrame, which extends it with the sub-frame fraction.
 *
 *   animNumber = anim->legsAnimWord & ~ANIM_TOGGLEBIT (& 0xfdff).
 *   animTree   = anim->animTree (forwarded to the XAnim traps).
 */
static CG_VEHICLE_COMPAT_ALWAYS_INLINE int32_t cgame_compat_turret_bisect_sample_node(XAnimTree *animTree, int32_t animNumber,
                                                                                      int32_t *outNode, int32_t *outChildCountB)
{
    int32_t childCountA, midChild, node, childCountB, midGrandchild, sampleNode;

    trap_XAnimClearTreeGoalWeightsStrict(animTree, animNumber, 0.0f); /* 0x300330fb setup */

    childCountA = trap_XAnimGetNumChildren((uint32_t)animNumber);    /* 0x30033101 */
    if (childCountA == 0) {                             /* 0x3003310b..0x30033122 */
        Com_Error(1, cg_playerAnimNoChildrenError, trap_XAnimGetAnimName((uint32_t)animNumber));
    }
    midChild = childCountA / 2;                         /* 0x30033125 (SAR after CDQ) */
    node = trap_XAnimGetChildAt((uint32_t)animNumber, midChild); /* 0x3003312d */

    childCountB = trap_XAnimGetNumChildren((uint32_t)node);          /* 0x30033135 */
    if (childCountB == 0) {                             /* 0x3003313d..0x30033158 */
        Com_Error(1, cg_playerAnimNoChildrenError, trap_XAnimGetAnimName((uint32_t)node));
    }
    midGrandchild = childCountB / 2;                    /* 0x3003315b */
    sampleNode = trap_XAnimGetChildAt((uint32_t)node, midGrandchild); /* 0x30033164 */

    *outNode = node;
    *outChildCountB = childCountB;
    return sampleNode;
}

/*
 * NOT_FROM_ORIGINAL_SOURCE: source spelling of Block A's full bisection
 * (0x300330f9..0x300331f9): the shared
 * bisection above, then a sub-frame blend fraction derived from lerpValue — the
 * caller's LerpAngle(vehicle->currentState.iconBaseYaw, lerpYawTarget, frac) result — and
 * returning the FINAL resolved frame node. Also reports the intermediate sampleNode
 * (used as Block A's channel-0 bone index and trap_XAnimGetWeight sample) and the mid-child node.
 *
 * The two FLDs feeding the fraction (0x3003316a FLD [ESP+0x18] at ESP=frame-4+tail
 * depth and 0x3003317b FLD [ESP+0x14] at the popped depth) BOTH resolve to the same
 * absolute slot F+0x8 — the 0x3003309c FSTP target of the LerpAngle result. The
 * sampleNode int (F+0x4) is never FLD'd; no int-bit reinterpretation happens.
 */
static CG_VEHICLE_COMPAT_ALWAYS_INLINE int32_t cgame_compat_turret_bone_frame(XAnimTree *animTree, int32_t animNumber, float lerpValue,
                                                                              float *outFraction, int32_t *outNode, int32_t *outSampleNode)
{
    int32_t node, childCountB, frameIndex;
    int32_t sampleNode;
    float fraction;

    sampleNode = cgame_compat_turret_bisect_sample_node(animTree, animNumber, &node, &childCountB);

    if (lerpValue > 0.0f) {              /* 0x30033172 FCOMP 0.0; TEST AH,0x41; JNZ b5 */
        float frac = lerpValue * (1.0f / 60.0f); /* FMUL 0x3007c2c8 = 0x3c888889 */
        /* 0x3003318c FCOM 1.0; TEST AH,5; JP -> clamp leg: fraction = MIN(frac, 1). */
        fraction = (frac < 1.0f) ? frac : 1.0f;
        frameIndex = childCountB - 1;    /* DEC EAX (both legs, 0x300331a1/0x300331b2) */
    } else if (!(lerpValue < 0.0f)) {    /* 0x300331b5 FCOMP 0.0; TEST AH,5; JP e9:
                                          * the == 0.0 (and NaN) leg zeroes the fraction. */
        fraction = 0.0f;                 /* 0x300331e9 */
        frameIndex = 0;                  /* 0x300331f1 XOR EAX,EAX */
    } else {                             /* lerpValue < 0.0 (0x300331c2) */
        float mag = -lerpValue;          /* FABS (lerpValue < 0 here) */
        /* The DLL keeps the product in an 80-bit register from FMUL double 0x3007c2c0
         * (0x300331c8) through FCOM double 1.0 (0x300331ce) to the single FSTP float
         * (0x300331e3). A `double d` would round to double first and then to float
         * (double rounding); long double rounds once, at the (float) store (Class 1). */
        long double d = (long double)mag * (1.0 / 60.0); /* FMUL double 0x3007c2c0 */
        /* 0x300331ce FCOM double 1.0 (0x3007bcf8); JNP -> fraction = MIN(d, 1). */
        fraction = (d < 1.0) ? (float)d : 1.0f;
        frameIndex = 0;                  /* 0x300331f1 XOR EAX,EAX */
    }

    *outFraction = fraction;
    *outNode = node;
    *outSampleNode = sampleNode;
    return trap_XAnimGetChildAt((uint32_t)node, frameIndex);      /* 0x300331f4 resolved frame */
}

/*
 * NOT_FROM_ORIGINAL_SOURCE: source spelling of the turret-state gate shared by
 * all four blend blocks (e.g. 0x300330a0..0x300330c3):
 * the client must have a live turret render-flags word (legsAnimWord != 0), a
 * resolved legs animation entry (legsAnimEntryWord != 0), whose flags at +0x50 must
 * carry BG_ANIM_ENTRY_TURRET. The original stores a pointer in this i386 slot;
 * native builds resolve the table-relative offset stored in the same word.
 */
static CG_VEHICLE_COMPAT_ALWAYS_INLINE qboolean cgame_compat_turret_state_positioned(const clientInfo_t *anim)
{
    const bg_static_animation_t *entry;

    if (anim->legsAnimWord == 0 || anim->legsAnimEntryWord == 0) { /* 0x300330a0..b9 */
        return qfalse;
    }
    entry = cgame_compat_anim_entry_from_word(anim->legsAnimEntryWord);
    return (entry->flags & BG_ANIM_ENTRY_TURRET) != 0 ? qtrue : qfalse;
}

/*
 * The blend blocks pass blend fractions / 1.0f / (1.0f - frac) as floats;
 * trap_XAnimSetGoalWeight forwards their raw dword payloads after narrowing the
 * two unsigned-short arguments proved by the wrapper.
 */

/* .rdata 0x3007bce0 = 1.0f. */
#define CG_TURRET_ONE 1.0f

enum {
    CG_VEHICLE_FIRST_ENTITYNUM = MAX_CLIENTS_IN_SNAPSHOT,
    CG_VEHICLE_SEAT_ROLE_MASK = 7,
    CG_VEHICLE_SEAT_ROLE_FIRST = 1,
    CG_VEHICLE_SEAT_ROLE_LAST = 6
};

qboolean CG_PlayerVehiclePositionAndBlend(centity_t *rider)
{
    int32_t vehicleEntityNum;
    int32_t riderClientNum;
    int32_t seatRole;                 /* rider->currentState.stateFilter & 7 (ECX) */
    clientInfo_t *anim;        /* &bgs.clientinfo[rider->currentState.clientNum] (EDI) */
    centity_t *vehicle;          /* &cg_entities[vehicleEntityNum] (EBP) */
    int32_t mode;                     /* vehicle->currentState.stateFilter (+0x88) */

    /* ---- 0x30032fe0..0x30033011: entry gate ---- */
    vehicleEntityNum = (int32_t)rider->currentState.vehicleEntityNum;
    if (vehicleEntityNum < CG_VEHICLE_FIRST_ENTITYNUM) {
        return qfalse;                                   /* JL 0x30033b5f */
    }
    if (vehicleEntityNum == ENTITYNUM_NONE) {
        return qfalse;                                   /* JZ 0x30033b5f */
    }
    seatRole = rider->currentState.stateFilter & CG_VEHICLE_SEAT_ROLE_MASK;
    if (seatRole < CG_VEHICLE_SEAT_ROLE_FIRST || seatRole > CG_VEHICLE_SEAT_ROLE_LAST) { /* JLE / JGE 0x30033b5f */
        return qfalse;
    }

    /* ---- 0x30033016..0x30033033: anim record must have advanced at least once ---- */
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    riderClientNum = rider->currentState.clientNum;
    if ((uint32_t)riderClientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "CG_PlayerVehiclePositionAndBlend: "
                  "invalid client number %i",
                  riderClientNum);
        return qfalse;
    }
    anim = &bgs.clientinfo[riderClientNum];
    if (anim->infoValid == 0) { /* CMP [EDI],0 / JZ 0x30033035 */
        return qfalse;
    }

    /* ---- 0x3003303f..0x30033065: vehicle owns a live DObj ---- */
    vehicle = cgame_compat_unchecked_cgentity(vehicleEntityNum);
    if (vehicle->currentValid == 0) { /* TEST EAX,EAX / JZ */
        return qfalse;
    }

    /* ---- 0x30033066..0x30033078: dispatch on vehicle mode + rider seat role ---- */
    mode = vehicle->currentState.stateFilter;

    if (mode == 1 && (seatRole == 1 || seatRole == 3)) {
        /* ============= Block A (0x30033083) — driver/main gunner ============= */
        /* LerpAngle(vehicle->currentState.iconBaseYaw, lerpYawTarget, cg_frameInterpolation)
         * (0x30033083..0x3003309c, FSTP [ESP+0x20] -> F+0x8) is LIVE: it is the
         * exact value the sub-frame-fraction logic in CG_TurretBoneFrame FLDs
         * (0x3003316a / 0x3003317b both resolve to F+0x8). */
        float lerpYaw = LerpAngle(vehicle->currentState.iconBaseYaw, vehicle->corpseModelInfo.leanf, cg_frameInterpolation);

        if (cgame_compat_turret_state_positioned(anim)) {
            XAnimTree *animTree = anim->animTree;
            int32_t animNumber, node, sampleNode;
            int32_t frameNode; /* F+0xc: the resolved final frame node
                                            *   (trap_XAnimGetChildAt @0x300331f4, stored @0x30033200);
                                            *   Block A's channel-1 override bone. Consumed
                                            *   only inside this block — NOT the tail compare
                                            *   (that reads clientNum; see the tail note). */
            float frameFrac = 0.0f;
            float ch0Reference, blend;

            /* 0x300330d5..0x300330f5: the Scr_GetAnimsIndex return (AX) is stored
             * into the HIGH u16 (F+0x6) over the masked legsAnimWord u16 (F+0x4),
             * and the packed dword is the anim-node handle every trap receives —
             * the same rootNodePacked idiom as CG_PlayerTurretPositionAndBlend. */
            animNumber = coduo_int32_from_bits(
                ((uint32_t)(uint16_t)Scr_GetAnimsIndex(bgs.animationTable.animTreeHandle) << SCR_ANIM_TREE_INDEX_SHIFT) |
                (anim->legsAnimWord & (uint32_t)~ANIM_TOGGLEBIT));

            frameNode = cgame_compat_turret_bone_frame(animTree, animNumber, lerpYaw, &frameFrac, &node, &sampleNode);

            /* ---- 0x300331fc..0x30033361: three channel overrides + tail blend ---- */
            trap_XAnimClearTreeGoalWeightsStrict(animTree, animNumber, 0.0f); /* 0x30033204 emitter re-setup */

            /* Channel 0 — samples AND writes the sampleNode bone: trap_XAnimGetWeight
             * arg2 = [ESP+0x1c] = F+0x4 = sampleNode at 0x30033217, and the
             * trap_XAnimSetGoalWeight bone arg ECX = [ESP+0x10] = F+0x4 = sampleNode
             * again at 0x30033267/0x30033276 (frameNode lives at F+0xc and is used
             * only by channel 1). reference = (1.0f - frameFrac). */
            ch0Reference = 1.0f - frameFrac; /* FLD 1.0f; FSUB [ESP+0x20] */
            blend = cgame_compat_turret_blend_step(trap_XAnimGetWeight(animTree, (uint16_t)sampleNode),
                                                   ch0Reference); /* 0x3003321e sample=sampleNode */
            (void)trap_XAnimSetGoalWeight(animTree, sampleNode, ch0Reference, blend, CG_TURRET_ONE, 0,
                                          qfalse); /* 0x30033278 bone=sampleNode */

            /* Channel 1 — frameNode bone, reference frameFrac. Runs only when the
             * sub-frame fraction is non-zero (0x3003327d FLD 0.0f; 0x30033283 FLD
             * frameFrac; FUCOMPP; TEST AH,0x44; JNP skip). */
            if (frameFrac != 0.0f) { /* 0x3003328e..0x30033291 */
                blend = cgame_compat_turret_blend_step(trap_XAnimGetWeight(animTree, (uint16_t)frameNode),
                                                       frameFrac); /* 0x3003329a sample; 0x300332a6 */
                (void)trap_XAnimSetGoalWeight(animTree, frameNode, frameFrac, blend, CG_TURRET_ONE, 0, qfalse); /* 0x300332f0 */
            }

            /* Channel 2 — node bone, base reset to identity (1.0f,1.0f,1.0f). 0x3003330d. */
            (void)trap_XAnimSetGoalWeight(animTree, node, CG_TURRET_ONE, CG_TURRET_ONE, CG_TURRET_ONE, 0, qfalse); /* 0x3003330d */

            /* Tail blend — node bone, reference 1.0f. 0x30033312 sample; tail Trap143 at
             * 0x3003391b via the shared 0x30033908 entry (arg1=node, arg2=1.0f). */
            blend = cgame_compat_turret_blend_step(trap_XAnimGetWeight(animTree, (uint16_t)node), 1.0f); /* 0x30033315 sample; 0x30033323 */
            (void)trap_XAnimSetGoalWeight(animTree, node, CG_TURRET_ONE, blend, CG_TURRET_ONE, 0, qfalse); /* 0x3003391b */
        }
    } else if (mode == 1 && seatRole == 2) {
        /* ============= Block B (0x30033366) — secondary passenger ============= */
        if (cgame_compat_turret_state_positioned(anim)) {
            /* B re-fetches anim from [ESP+0x1c] (0x300333a3) — identical pointer to the
             * clientNum-indexed record already in `anim`. No LerpAngle and no sub-frame
             * fraction: B has no frameNode, so channel 0 samples/writes sampleNode and both
             * blend channels use reference 1.0f. */
            XAnimTree *animTree = anim->animTree;
            int32_t animNumber, node, sampleNode, childCountB;
            float blend;

            /* 0x300333bd/0x300333ca: packed handle, (treeHandle<<16)|(word&0xfdff). */
            animNumber = coduo_int32_from_bits(
                ((uint32_t)(uint16_t)Scr_GetAnimsIndex(bgs.animationTable.animTreeHandle) << SCR_ANIM_TREE_INDEX_SHIFT) |
                (anim->legsAnimWord & (uint32_t)~ANIM_TOGGLEBIT)); /* 0x300333ae */

            sampleNode = cgame_compat_turret_bisect_sample_node(animTree, animNumber, &node, &childCountB);

            /* Channel 0 — sampleNode bone, base reset to identity. 0x3003345d. */
            (void)trap_XAnimSetGoalWeight(animTree, sampleNode, CG_TURRET_ONE, CG_TURRET_ONE, CG_TURRET_ONE, 0, qfalse); /* 0x3003345d */

            /* Channel 1 — sampleNode bone, reference 1.0f. 0x30033466 re-setup; sample. */
            trap_XAnimClearTreeGoalWeightsStrict(animTree, animNumber, 0.0f); /* 0x30033466 */
            blend = cgame_compat_turret_blend_step(trap_XAnimGetWeight(animTree, (uint16_t)sampleNode),
                                                   1.0f); /* 0x30033472 sample; 0x30033480 */
            (void)trap_XAnimSetGoalWeight(animTree, sampleNode, CG_TURRET_ONE, blend, CG_TURRET_ONE, 0, qfalse); /* 0x300334ce */

            /* Channel 2 — node bone, base reset to identity. 0x300334e8. */
            (void)trap_XAnimSetGoalWeight(animTree, node, CG_TURRET_ONE, CG_TURRET_ONE, CG_TURRET_ONE, 0, qfalse); /* 0x300334e8 */

            /* Tail blend — node bone, reference 1.0f. 0x300334f0 sample; tail Trap143 at
             * 0x3003391b via the shared 0x30033912 entry (arg1=node, arg2=1.0f). */
            blend = cgame_compat_turret_blend_step(trap_XAnimGetWeight(animTree, (uint16_t)node), 1.0f); /* 0x300334f0 sample; 0x300334fe */
            (void)trap_XAnimSetGoalWeight(animTree, node, CG_TURRET_ONE, blend, CG_TURRET_ONE, 0, qfalse); /* 0x3003391b */
        }
    } else if (mode == 5) {
        /* ============= Block C (0x3003355e) — mode-5 vehicle ============= */
        if (cgame_compat_turret_state_positioned(anim)) {
            /* Byte-for-byte the same shape as Block B. */
            XAnimTree *animTree = anim->animTree;
            int32_t animNumber, node, sampleNode, childCountB;
            float blend;

            /* 0x300335a3/0x300335b0: packed handle, (treeHandle<<16)|(word&0xfdff). */
            animNumber = coduo_int32_from_bits(
                ((uint32_t)(uint16_t)Scr_GetAnimsIndex(bgs.animationTable.animTreeHandle) << SCR_ANIM_TREE_INDEX_SHIFT) |
                (anim->legsAnimWord & (uint32_t)~ANIM_TOGGLEBIT)); /* 0x30033594 */

            sampleNode = cgame_compat_turret_bisect_sample_node(animTree, animNumber, &node, &childCountB);

            (void)trap_XAnimSetGoalWeight(animTree, sampleNode, CG_TURRET_ONE, CG_TURRET_ONE, CG_TURRET_ONE, 0, qfalse); /* 0x30033643 */

            trap_XAnimClearTreeGoalWeightsStrict(animTree, animNumber, 0.0f); /* 0x3003364c */
            blend = cgame_compat_turret_blend_step(trap_XAnimGetWeight(animTree, (uint16_t)sampleNode),
                                                   1.0f); /* 0x30033658 sample; 0x30033666 */
            (void)trap_XAnimSetGoalWeight(animTree, sampleNode, CG_TURRET_ONE, blend, CG_TURRET_ONE, 0, qfalse); /* 0x300336b4 */

            (void)trap_XAnimSetGoalWeight(animTree, node, CG_TURRET_ONE, CG_TURRET_ONE, CG_TURRET_ONE, 0, qfalse); /* 0x300336ce */

            blend = cgame_compat_turret_blend_step(trap_XAnimGetWeight(animTree, (uint16_t)node), 1.0f); /* 0x300336d6 sample; 0x300336e4 */
            (void)trap_XAnimSetGoalWeight(animTree, node, CG_TURRET_ONE, blend, CG_TURRET_ONE, 0, qfalse); /* 0x3003391b */
        }
    } else if (mode == 2 && seatRole == 2) {
        /* ============= Block D (0x3003374c) — mode-2 secondary ============= */
        if (cgame_compat_turret_state_positioned(anim)) {
            /* Same idiom as Blocks B/C; the final blend routes through the 0x30033908
             * tail entry (like Block A) rather than 0x30033912, but the resulting tail
             * trap_XAnimSetGoalWeight arguments are identical. */
            XAnimTree *animTree = anim->animTree;
            int32_t animNumber, node, sampleNode, childCountB;
            float blend;

            /* 0x30033788/0x3003379d: packed handle, (treeHandle<<16)|(word&0xfdff). */
            animNumber = coduo_int32_from_bits(
                ((uint32_t)(uint16_t)Scr_GetAnimsIndex(bgs.animationTable.animTreeHandle) << SCR_ANIM_TREE_INDEX_SHIFT) |
                (anim->legsAnimWord & (uint32_t)~ANIM_TOGGLEBIT)); /* 0x30033782 */

            sampleNode = cgame_compat_turret_bisect_sample_node(animTree, animNumber, &node, &childCountB);

            (void)trap_XAnimSetGoalWeight(animTree, sampleNode, CG_TURRET_ONE, CG_TURRET_ONE, CG_TURRET_ONE, 0, qfalse); /* 0x30033830 */

            trap_XAnimClearTreeGoalWeightsStrict(animTree, animNumber, 0.0f); /* 0x30033839 */
            blend = cgame_compat_turret_blend_step(trap_XAnimGetWeight(animTree, (uint16_t)sampleNode),
                                                   1.0f); /* 0x30033845 sample; 0x30033853 */
            (void)trap_XAnimSetGoalWeight(animTree, sampleNode, CG_TURRET_ONE, blend, CG_TURRET_ONE, 0, qfalse); /* 0x300338a1 */

            (void)trap_XAnimSetGoalWeight(animTree, node, CG_TURRET_ONE, CG_TURRET_ONE, CG_TURRET_ONE, 0, qfalse); /* 0x300338bb */

            blend = cgame_compat_turret_blend_step(trap_XAnimGetWeight(animTree, (uint16_t)node), 1.0f); /* 0x300338c3 sample; 0x300338d1 */
            (void)trap_XAnimSetGoalWeight(animTree, node, CG_TURRET_ONE, blend, CG_TURRET_ONE, 0, qfalse); /* 0x3003391b */
        }
    }
    /* every block falls through to the common tail at 0x3003392b */

    /* ==================== common tail (0x3003392b) ====================
     * Decide whether to actually position the rider: for some vehicle modes the LOCAL
     * player's own model must be skipped (rider->currentState.clientNum == cg_snap->ps.psClientNum and the
     * view gate 0x304831c0 clear), and the mode-1 driver seat additionally skips while
     * not aiming down sights (cg_predictedPlayerState.adsFraction == 0).
     *
     * The compare operand is [ESP+0x24], which — with ESP at the tail baseline (the three
     * prologue pushes still live, esp == F-0xc, proven by the POP ESI/EBP/EDI epilogue) —
     * resolves to the SAME absolute slot F+0x18 that the prologue seeded with rider->currentState.clientNum
     * (MOV [ESP+0x1c],EDI @0x3003301c, EDI = [EBX+0x94]). It is NOT the Block-A frameNode slot
     * (F+0x10, written via [ESP+0x24] at the deeper esp=F-0x20 @0x30033200); the shared
     * displacement 0x24 aliases two different absolute slots at the two ESP depths. So the
     * tail compares the rider's own client number, and no uninitialized read occurs. */
    mode = vehicle->currentState.stateFilter; /* retail reload at 0x3003392b */
    if (mode == 1) { /* 0x30033971 */
        if (riderClientNum == cg_snap->ps.psClientNum && cg_thirdPerson == 0) {
            int32_t r = rider->currentState.stateFilter & CG_VEHICLE_SEAT_ROLE_MASK;
            if (r == 2 || r == 3) {
                return qfalse; /* 0x30033998 / 0x3003399d */
            }
            /* 0x3003399f FUCOMPP vs 0.0; TEST AH,0x44; JNP 0x30033965 — the JNP
             * (exactly-one-bit) leg is the ORDERED-EQUAL case: return qfalse when
             * adsFraction == 0.0; proceed to position when != 0.0 (or NaN). */
            if (cg_predictedPlayerState.adsFraction == 0.0f) {
                return qfalse; /* 0x300339b2 -> 0x30033965 */
            }
        }
    } else if (mode == 2) { /* 0x30033937 */
        int32_t r = rider->currentState.stateFilter & CG_VEHICLE_SEAT_ROLE_MASK;
        if (r == 1) {
            return qfalse; /* 0x30033943 -> 0x30033965 */
        }
        if (r == 2) { /* 0x30033948 */
            if (riderClientNum == cg_snap->ps.psClientNum && cg_thirdPerson == 0) {
                return qfalse; /* 0x30033963 -> 0x30033965 */
            }
        }
    }
    /* mode 5 (and any other) always proceeds to build. */

    /* ---- 0x300339b4: resolve the vehicle DObj + rider seat tag, build bone matrix ---- */
    {
        intptr_t dObjHandle = cgame_syscall(CG_DOBJ_GET_HANDLE, vehicle->currentState.number);
        if (dObjHandle == 0) { /* TEST ECX,ECX / JZ 0x30033965 */
            return qfalse;
        }

        const char *tagName = CG_GetRiderTagName(rider->currentState.stateFilter & CG_VEHICLE_SEAT_ROLE_MASK); /* 0x300339d5 */

        DObjSkelMat worldMatrix; /* [ESP+0x2c] */
        if (CG_DObjGetWorldTagMatrix((void *)dObjHandle, tagName, vehicle, &worldMatrix) == 0) { /* 0x300339e4 */
            /* 0x300339f0: tag bone does not exist — warn and abort. */
            Com_Printf(cg_missingTurretTagWarning, tagName);
            return qfalse;
        }

        /* ---- 0x30033a0a..0x30033a77: recover Euler angles from the bone rotation ----
         * worldMatrix.axis holds the padded 3x3 rotation and worldMatrix.origin
         * holds the bone origin. AxisToAngles takes the compact 3x3 copy. */
        axis_t boneAxis;
        boneAxis[0][0] = worldMatrix.axis[0][0];
        boneAxis[0][1] = worldMatrix.axis[0][1];
        boneAxis[0][2] = worldMatrix.axis[0][2];
        boneAxis[1][0] = worldMatrix.axis[1][0];
        boneAxis[1][1] = worldMatrix.axis[1][1];
        boneAxis[1][2] = worldMatrix.axis[1][2];
        boneAxis[2][0] = worldMatrix.axis[2][0];
        boneAxis[2][1] = worldMatrix.axis[2][1];
        boneAxis[2][2] = worldMatrix.axis[2][2];

        AxisToAngles(boneAxis, rider->lerpAngles); /* 0x30033a68 -> rider +0x214 */
        AxisToAngles(boneAxis, anim->turretOverrideAngles); /* 0x30033a77 -> anim +0x3f4 */

        /* ---- 0x30033a7c..end: lerpOrigin = boneOrigin + boneRotation * seatOffset ----
         * seatOffset is selected by (vehicle->currentState.stateFilter, rider->currentState.stateFilter & 7). The
         * three POPs at 0x30033ab4 shift the stack slots mid-chain; semantically this is
         * the standard matrix*vector + origin transform below. */
        /* 0x30033a7c..0x30033aa9 publishes the raw bone origin before the
         * offset lookup, with the role reload interleaved between the stores. */
        rider->lerpOrigin[0] = worldMatrix.origin[0];
        int32_t offsetSeatRole = rider->currentState.stateFilter;
        rider->lerpOrigin[2] = worldMatrix.origin[2];
        rider->lerpOrigin[1] = worldMatrix.origin[1];
        vehicle_type_t offsetVehicleType = (vehicle_type_t)vehicle->currentState.stateFilter;
        offsetSeatRole &= CG_VEHICLE_SEAT_ROLE_MASK;
        const float *seatOffset = BG_GetVehiclePosOffset(offsetVehicleType, offsetSeatRole);
        float ox = seatOffset[0], oy = seatOffset[1], oz = seatOffset[2];

        /* The DLL accumulates each component in THREE rounding steps, spilling the
         * partial to the lerpOrigin float slot and reloading it (store 0x30033abb /
         * reload 0x30033ae8, store 0x30033aee / reload 0x30033b21). A single rvalue
         * would keep the sum 80-bit and round only once; force the DLL's per-step
         * float rounding with staged accumulation (Class 1). */
        rider->lerpOrigin[0] = worldMatrix.axis[0][0] * ox + worldMatrix.origin[0]; /* 0x30033aae..abb */
        rider->lerpOrigin[0] += worldMatrix.axis[1][0] * oy; /* 0x30033ae1..aee */
        rider->lerpOrigin[0] += worldMatrix.axis[2][0] * oz; /* 0x30033b1a..b27 */
        rider->lerpOrigin[1] = worldMatrix.axis[0][1] * ox + worldMatrix.origin[1]; /* 0x30033ac1..acb */
        rider->lerpOrigin[1] += worldMatrix.axis[1][1] * oy; /* 0x30033af4..b01 */
        rider->lerpOrigin[1] += worldMatrix.axis[2][1] * oz; /* 0x30033b2d..b3a */
        rider->lerpOrigin[2] = worldMatrix.axis[0][2] * ox + worldMatrix.origin[2]; /* 0x30033ad1..adb */
        rider->lerpOrigin[2] += worldMatrix.axis[1][2] * oy; /* 0x30033b07..b14 */
        rider->lerpOrigin[2] += worldMatrix.axis[2][2] * oz; /* 0x30033b40..b4c */
    }

    return qtrue; /* MOV EAX,1 (0x30033b47) */
}

#undef CG_VEHICLE_COMPAT_ALWAYS_INLINE
