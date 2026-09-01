#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3001e9f0..0x3001ec9e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001e9f0_3001ec9e.mcode
//
// CG_mg42_DoControllers — the type-11 MG42/turret arm of
// CG_DoControllers (0x30021fe0). It resolves
// the part's DObj (cgame syscall CG_DOBJ_GET_HANDLE) and drives three local view
// tags on that model — "tag_aim", "tag_aim_animated", and "tag_flash" — from the
// part's per-frame interpolated angles, then resolves its runtime animation tree
// and issues one
// CG_XANIM_SET_COMPLETE_GOAL_WEIGHT_KNOB_ALL overlay update.
//
// NAME ADJUDICATION: the .mcode's `# name fire_artillery` is REJECTED. It is a pure
// size guess (win 0x2ae ~ matched 0x2b0) from the game_mp_uo server bank; server
// fire_artillery is a gentity spawner with signature
// `gentity_t *fire_artillery(gentity_t*, float*, int)`. This function lives in
// uo_cgame_mp_x86.dll, takes no gentity, spawns nothing, and instead issues cgame
// DObj tag traps (0xa5/0xb2/0xa3/0xb5) and a CG_XANIM_SET_COMPLETE_GOAL_WEIGHT_KNOB_ALL overlay — it is a cgame
// render-side DObj controller. Its identity is fixed by the dispatcher
// (0x30021fe0), which routes part->currentState.eType == ET_TURRET (11) here. The Mac
// CG_mg42_DoControllers shares the aim-angle callees and drives the corresponding
// control tags and XAnim goal weight, resolving the source name.
//
// ---- ABI (proven from machine code / caller) ---------------------------------------
// The sole caller is CG_DoControllers (0x30022005: PUSH ECX; CALL; ADD ESP,4):
//   part    (ESI)  -- the centity, inherited in the shared ESI register
//                     (the dispatcher never reloads ESI, only PUSHes partBits in ECX).
//   partBits (stack, [ESP+0x2c] after the prologue) -- the DObj selection bitset the
//                     dispatcher passed through in ECX; forwarded verbatim as the middle
//                     argument of the CG_DOBJ_SET_CONTROL_ROT_TRANS_INDEX (0xa3) traps.
// Prologue SUB ESP,0x20 + PUSH EBX/EBP/EDI reserves the scratch angle slots and saves
// callee-saved regs; the RET at 0x3001ec9d + the caller's ADD ESP,4 are i386 cdecl
// stack cleanup, not source-level behavior. Returns void.
//
// ---- Local scratch layout (frame base S0 = ESP at entry, retaddr at [S0]) ----------
// The four aim-bound LerpAngles and the two/one aim/flash LerpAngles all land in a
// small block of scratch floats. Tracked absolute offsets from S0:
//   B[0] @ [S0-0x14]  B[1] @ [S0-0x10]   -- aim upper bounds (max)
//   A[0] @ [S0-0x1c]  A[1] @ [S0-0x18]   -- aim lower bounds (min)
//   C[0] @ [S0-0x0c]  C[1] @ [S0-0x08]   -- the working aim angles handed to the tags
//                       (as a 3-float vector {C[0], C[1], [S0-0x04]}, [S0-0x04] zeroed)
// The tag setters read &C[0] (LEA EBX,[ESP+0x24]) as the `angles` vec3 of
// CG_DObjSetLocalTag. Modeled here as plain local floats/vectors.

/* This handler receives a centity_t and the DObj selection bitset. Its
 * first 0xf4 bytes are currentState and +0xf4..+0x1e7 are nextState. The former
 * six-float runs are therefore current/next origin2 followed by angles2; +0xd8
 * and +0x1cc are current/next leanf. The wire-schema name of +0xe4 is
 * animMovetype, while the server turret producers use that slot specifically as
 * turretOverheatState. Keeping the two entityState_t views eliminates the false
 * abiGap runs and makes the logical field accesses valid when pointer-bearing
 * centity fields widen on 64-bit hosts. */
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(offsetof(entityState_t, origin2) == 0x5c, "currentState.origin2 +0x5c");
_Static_assert(offsetof(entityState_t, angles2) == 0x68, "currentState.angles2 +0x68");
_Static_assert(offsetof(entityState_t, leanf) == 0xd8, "currentState.leanf +0xd8");
_Static_assert(offsetof(entityState_t, turretOverheatState) == 0xe4,
               "currentState.turretOverheatState +0xe4");
_Static_assert(offsetof(centity_t, nextState) + offsetof(entityState_t, origin2) == 0x150,
               "nextState.origin2 +0x150");
_Static_assert(offsetof(centity_t, nextState) + offsetof(entityState_t, angles2) == 0x15c,
               "nextState.angles2 +0x15c");
_Static_assert(offsetof(centity_t, nextState) + offsetof(entityState_t, leanf) == 0x1cc,
               "nextState.leanf +0x1cc");
_Static_assert(offsetof(centity_t, lerpAngles) == 0x214,
               "cent.lerpAngles +0x214");
#endif

/* Bit 9 (0x200) of currentState.eFlags is authored by the fixed-turret server
 * path while the turret fires and cleared when it stops. Win32 TEST CH,0x2 and
 * PowerPC rlwinm both prove this exact bit and role. */
enum { CG_MG42_STATE_FIRING = 0x200 };

/* CG_XANIM_SET_COMPLETE_GOAL_WEIGHT_KNOB_ALL overlay sub-mode. When the aim gate holds (or the part is not overlay-
 * flagged) the tail uses mode 1; only a firing, non-aim turret whose overheat state
 * is clear selects mode 2. Provisional enum names by proven role (exact CoD
 * enumerators unresolved). */
enum {
    STATE_PART_OVERLAY_MODE_1 = 1,
    STATE_PART_OVERLAY_MODE_2 = 2
};

/* The DObj runtime-tree trap (0xb5) return is forwarded as
 * CG_XANIM_SET_COMPLETE_GOAL_WEIGHT_KNOB_ALL arg0. The tag strings are declared
 * with the other recovered globals. */
/* &vec3_origin (0x30071f58) is the shared read-only zero vec3 {0,0,0} used as the
 * tag origin here; declared in globals.h. (Previously mis-modeled as the scalar
 * cg_floatOne / a {1.0f,1.0f,0.0f} vec3 -- the .rdata bytes are three zeros.) */

/* NOT_FROM_ORIGINAL_SOURCE: source-only factoring of the inlined two-comparison
 * clamp at 0x3001eaaf..0x3001ead8.
 *
 * Clamp x into [lo, hi] using the exact FCOM ordering of the machine code:
 * if x > hi -> hi; else if x < lo -> lo; else x. Unordered/NaN falls through as x
 * (the code never re-stores on the unordered branches), matching FCOM's C0/C3 test. */
static float clamp_angle(float x, float lo, float hi)
{
    /* 0x3001eaaf FCOM hi; TEST AH,0x41; JNZ -> (x <= hi || unordered): check lo.
     * fallthrough (x > hi): result = hi. */
    if (x > hi) {
        return hi;
    }
    /* 0x3001eac9 FCOMP lo; TEST AH,0x5; JP skip. JP not-taken only when x < lo. */
    if (x < lo) {
        return lo;
    }
    return x;
}

void CG_mg42_DoControllers(centity_t *part, uint32_t *partBits)
{
    /* centity_t names the canonical currentState overlays directly, while
     * nextState has its complete entityState_t type at +0xf4. */
    const entityState_t *nextState = &part->nextState;

    /* 0x3001e9f0..0x3001ea08: self = cgame_syscall(CG_DOBJ_GET_HANDLE,
     * part->currentState.number). */
    intptr_t self = cgame_syscall(
        CG_DOBJ_GET_HANDLE, part->currentState.number);

    /* 0x3001ea0a..0x3001ea2f: aim-tag gate. Take the clamped-aim path only when this
     * predicted-player entity is in the scope/zoom-FOV state (entityStateFlags & 0x6000,
     * TEST AH,0x60), is the entity the effect pass is currently drawing
     * (cg_predictedPlayerState.viewLockedEntityNum), and the view-state flag at 0x304831c0 is clear (its exact
     * identity is disputed between consumers — see globals.h — so it stays mechanical).
     * Otherwise (else block) the aim angles come straight from a single interpolated
     * pair with no clamp. */
    float aim[2]; /* C[0], C[1] */
    qboolean aimGate =
        (cg_predictedPlayerState.entityStateFlags & EF_ZOOM_FOV_MASK) != 0 &&
        cg_predictedPlayerState.viewLockedEntityNum ==
            part->currentState.number &&
        cg_thirdPerson == 0;

    if (aimGate) {
        /* 0x3001ea35..0x3001ea8e: four LerpAngle bounds, blended by cg_frameInterpolation.
         * Upper bounds B[] and lower bounds A[] for the two aim-cone components. */
        float frac = cg_frameInterpolation;
        float maxAngle[2];
        float minAngle[2];
        maxAngle[0] = LerpAngle(part->currentState.leanf,      nextState->leanf,      frac); /* B[0] */
        maxAngle[1] = LerpAngle(part->currentState.origin2[0], nextState->origin2[0], frac); /* B[1] */
        minAngle[0] = LerpAngle(part->currentState.origin2[1], nextState->origin2[1], frac); /* A[0] */
        minAngle[1] = LerpAngle(part->currentState.origin2[2], nextState->origin2[2], frac); /* A[1] */

        /* 0x3001ea95..0x3001eae5: for each of the two aim components, take the signed
         * angular difference between the animated spin angle and the model's aim basis,
         * then clamp it into [minAngle, maxAngle]. spinAngles[] is the contiguous
         * {pitch, yaw} spin-angle pair at 0x30487ac8 (indexed by the ECX byte offset). */
        const float spinAngles[2] = { cg_refdefViewAngles[0], cg_refdefViewAngles[1] };
        for (int k = 0; k < 2; ++k) {
            float d = AngleSubtract(spinAngles[k], part->lerpAngles[k]);
            aim[k] = clamp_angle(d, minAngle[k], maxAngle[k]);
        }
    } else {
        /* 0x3001eae9..0x3001eb19: the non-aim path — the two aim angles are just the
         * interpolated current pair (no bounds, no clamp). */
        float frac = cg_frameInterpolation;
        aim[0] = LerpAngle(part->currentState.angles2[0], nextState->angles2[0], frac);
        aim[1] = LerpAngle(part->currentState.angles2[1], nextState->angles2[1], frac);
    }

    /* The three tag setters share the same `angles` vector: {aim[0], aim[1], 0} for the
     * two aim tags, then {flashAngle, 0, 0} for tag_flash. Origin is the shared .rdata
     * zero vec3 &vec3_origin == {0, 0, 0} in every case. */
    const vec3_t *tagOrigin = &vec3_origin;

    /* 0x3001eb1c..0x3001eb65: tag_aim. Resolve the bone index (CG_DOBJ_GET_BONE_INDEX);
     * if it exists (>= 0) and binding a control rot/trans slot succeeds, set the local
     * tag orientation to the aim vector. */
    {
        vec3_t aimAngles = { aim[0], aim[1], 0.0f };
        int32_t boneIndex = coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_DOBJ_GET_BONE_INDEX, self, (intptr_t)cg_aimTagName));
        if (boneIndex >= 0 &&
            cgame_syscall(CG_DOBJ_SET_CONTROL_ROT_TRANS_INDEX, self,
                          (intptr_t)partBits, boneIndex) != 0) {
            CG_DObjSetLocalTagInternal((void *)(intptr_t)self, boneIndex, aimAngles, *tagOrigin);
        }

        /* 0x3001eb68..0x3001eba9: tag_aim_animated — same aim vector and bone-bind flow. */
        boneIndex = coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_DOBJ_GET_BONE_INDEX, self,
            (intptr_t)cg_animatedAimTagName));
        if (boneIndex >= 0 &&
            cgame_syscall(CG_DOBJ_SET_CONTROL_ROT_TRANS_INDEX, self,
                          (intptr_t)partBits, boneIndex) != 0) {
            CG_DObjSetLocalTagInternal((void *)(intptr_t)self, boneIndex, aimAngles, *tagOrigin);
        }
    }

    /* 0x3001ebac..0x3001ec0f: tag_flash. Its angle is a fresh single interpolated value;
     * the vector is {flashAngle, 0, 0}. Same bone-resolve / control-bind / set flow. */
    {
        float flashAngle = LerpAngle(part->currentState.angles2[2], nextState->angles2[2],
                                     cg_frameInterpolation);
        vec3_t flashAngles = { flashAngle, 0.0f, 0.0f };
        int32_t boneIndex = coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_DOBJ_GET_BONE_INDEX, self,
            (intptr_t)cg_muzzleFlashTagName));
        if (boneIndex >= 0 &&
            cgame_syscall(CG_DOBJ_SET_CONTROL_ROT_TRANS_INDEX, self,
                          (intptr_t)partBits, boneIndex) != 0) {
            CG_DObjSetLocalTagInternal((void *)(intptr_t)self, boneIndex, flashAngles, *tagOrigin);
        }
    }

    /* 0x3001ec12..0x3001ec1c: EAX receives DObjGetTree(self), and
     * 0x3001ec8e pushes that same value as the runtime-tree argument. intptr_t
     * preserves the original pointer-width contract on the native host. */
    intptr_t runtimeTree = cgame_syscall(CG_DOBJ_GET_TREE, self);

    /* 0x3001ec1e..0x3001ec5b: pick the overlay sub-mode. Mode 1 unless this is a non-aim,
     * firing turret whose overheat state is clear, in which case mode 2. The aim gate
     * here re-tests the same first-person / current-effect-entity condition as above. */
    qboolean aimGate2 =
        (cg_predictedPlayerState.entityStateFlags & EF_ZOOM_FOV_MASK) != 0 &&
        cg_predictedPlayerState.viewLockedEntityNum ==
            part->currentState.number &&
        cg_thirdPerson == 0;

    int32_t overlayMode;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (!aimGate2 &&
        (part->currentState.eFlags & CG_MG42_STATE_FIRING) != 0 &&
        part->currentState.turretOverheatState == 0) {
        overlayMode = STATE_PART_OVERLAY_MODE_2;
    } else {
        overlayMode = STATE_PART_OVERLAY_MODE_1;
    }

    /* 0x3001ec5d..0x3001ec9a: cgame_syscall(CG_XANIM_SET_COMPLETE_GOAL_WEIGHT_KNOB_ALL, runtimeTree, overlayMode, 0,
     * 1.0f, 0.1f, 1.0f, 0, 0). The three float args are pushed as their bit patterns
     * (0x3f800000 == 1.0f, 0x3dcccccd == 0.1f). overlayMode is MOVZX-widened from a
     * 16-bit value (always 1 or 2 here). */
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (runtimeTree != 0) {
        cgame_syscall(CG_XANIM_SET_COMPLETE_GOAL_WEIGHT_KNOB_ALL,
                      runtimeTree,
                      overlayMode,
                      0,
                      CG_FloatBits(1.0f),
                      CG_FloatBits(0.1f),
                      CG_FloatBits(1.0f),
                      0,
                      0);
    }
}
