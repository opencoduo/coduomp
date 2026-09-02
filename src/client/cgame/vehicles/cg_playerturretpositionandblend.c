// Source: uo_cgame_mp_x86.dll 0x30033b70..0x300343d4
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30033b70_300343d4.mcode
//
// CG_PlayerTurretPositionAndBlend (0x30033b70) — position and orient a player who is
// manning a vehicle turret, by blending the player's aim along the turret weapon's
// DObj bone tree, then writing the blended barrel angles back into the player's
// centity lerpOrigin/lerpAngles.
//
// The .mcode header's mechanical pre-hint "PM_UpdateAimDownSightFlag" is REJECTED: it
// came from a pure size match (win 0x864 vs corpus 0x863), which the naming rules
// forbid, and the body proves turret-positioning behavior, not a playerState ADS-flag
// write. Proof (all against the bytes / objdump):
//   * Strings: "WARNING: aborting player positioning on turret since 'tag_weapon'
//     does not exist" (0x30079a28), "tag_weapon" (0x30079a7c), "tag_aim" (0x300772e0),
//     "WARNING: aborting player positioning on turret since 'tag_aim' does not exist"
//     (0x300799d8 — dumped byte-exact; the tag name is BAKED into the literal, there
//     is no '%s' in this string), "Player anim '%s' has no children" (0x30079ad5).
//   * It reads the manning client's clientInfo_t turret fields
//     (legsAnimWord +0x390, legsAnimEntryWord +0x394,
//      animTree +0x4c4)
//     and the turret weaponInfo_t (turretTagOffsetX/Y +0x30/+0x34, animHorRotateInc
//     +0x48c) — all pre-named across the shared header as belonging to exactly this
//     function (0x30033b70).
// Exact CoD source symbol unproven; named by proven behavior. The header already
// declares this exact name.
//
// Argument: the PLAYER centity (centity_t *) as the sole stack arg at [ESP+4].
//   * +0x74 vehicleEntityNum names the turret vehicle entity (gated >= 0x40 and
//     != ENTITYNUM_NONE).
//   * +0x94 clientNum rows into bgs.clientinfo[] (stride 0x4d0).
//   * +0x208 lerpOrigin / +0x214 lerpAngles are read (source aim) and written
//     back (blended barrel aim). Proven the arg is a centity (not the corpse
//     entityState_t) because it dereferences +0x208 (== centity_t
//     lerpOrigin), which lies beyond the 0xf4-byte entityState_t.
//
// Frame / ABI: SUB ESP,0xfc plus PUSH EBP/EDI/ESI/EBX (callee-saved). The function
// pushes trap arguments throughout and cleans them per call site; those ESP shuffles
// are i386 calling-convention detail. Returns void.
//
// x87: every FLD/FST/FADD/FSUB/FMUL/FDIV is single precision (DWORD ptr).
//
// .rdata float pool (dumped EXACTLY via `objdump -s -j .rdata`, NOT inferred from a
// neighbor — 0x3007bce0 is a known adjacent-constant trap):
//   0x3007bce0 = 1.0f, 0x3007bce4 = 2.0f, 0x3007bce8 = 0.5f, 0x3007bcec = 0.0f
//   0x3007be88 = 1000.0f (bytes 00 00 7a 44)
// cg_frametime (0x304831ac, int32 ms delta) is FILD'd and combined with 1000.0f to
// form the per-frame blend-rate denominator.

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Layout guards (4-byte i386 pointer width; pointer-free fields safe at both ABIs). */
_Static_assert(offsetof(centity_t, currentState.number) == 0x00, "player.currentState.number +0x00");
_Static_assert(offsetof(centity_t, currentState.vehicleEntityNum) == 0x74, "player.currentState.vehicleEntityNum +0x74");
_Static_assert(offsetof(centity_t, currentState.clientNum) == 0x94, "player.currentState.clientNum +0x94");
_Static_assert(offsetof(centity_t, currentState.weapon) == 0xcc, "vehicle.currentState.weapon +0xcc");
_Static_assert(offsetof(centity_t, currentValid) == 0x1e8,
               "vehicle.currentValid +0x1e8");
_Static_assert(offsetof(centity_t, lerpOrigin) == 0x208, "player/vehicle.lerpOrigin +0x208");
_Static_assert(offsetof(centity_t, lerpAngles) == 0x214, "player/vehicle.lerpAngles +0x214");
_Static_assert(offsetof(clientInfo_t, infoValid) == 0x00, "animState.infoValid +0x00");
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(offsetof(clientInfo_t, legsAnimWord) == 0x390, "animState.legsAnimWord +0x390");
_Static_assert(offsetof(clientInfo_t, legsAnimEntryWord) == 0x394,
               "animState.legsAnimEntryWord +0x394");
_Static_assert(offsetof(clientInfo_t, animTree) == 0x4c4,
               "animState.animTree +0x4c4");
#endif
/* weaponInfo_t carries pointer fields (name/gunModel/...), so its offsets only match
 * the target ABI at 32-bit pointer width; guard these to be a no-op on the host. */
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(offsetof(weaponInfo_t, animHorRotateInc) == 0x48c, "weaponInfo_t.animHorRotateInc +0x48c");
#endif

/* Vehicle-owner gate: the manning vehicle must be a non-client entity (>= 0x40) and
 * not the 10-bit "none" sentinel. Proven at 0x30033b7e/0x30033b87. */
enum { CG_TURRET_FIRST_VEHICLE_ENTITYNUM = MAX_CLIENTS_IN_SNAPSHOT };

/* cg_debuganim_vmCvar.integer (0x3052efec) exact verbosity at which the blended
 * barrel angles are NOT written back (a debug freeze). Proven at 0x30034323
 * (CMP,0x5; JZ skip). */
enum { CG_ANIMDEBUG_FREEZE_TURRET = 5 };

/* World content mask CG_Trace is issued with at 0x30034392
 * (MOV EAX,0x2810011). Same value the flame damage trace uses. */
#define CG_TURRET_TRACE_CONTENTMASK 0x02810011u

/* Shared cgame syscall ids used here: trap 0xa5 resolves the entity's DObj and
 * trap 0x9d calculates one runtime XAnim node's absolute rotation/move delta. */

#if defined(_MSC_VER)
#define CG_TURRET_COMPAT_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define CG_TURRET_COMPAT_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define CG_TURRET_COMPAT_ALWAYS_INLINE inline
#endif

/*
 * NOT_FROM_ORIGINAL_SOURCE: source spelling of the repeated per-bone blend-rate
 * clamp (six near-identical x87 clusters, e.g.
 * 0x30033f28, 0x30033fa2, 0x3003402d, 0x300340a8, 0x30034120, 0x300341a5,
 * 0x3003422f). Given a signed delta `x`, compute
 *     rate  = fabsf(x) * (1000.0f / cg_frametime);   // FABS; FILD frametime; FDIVR 1000; FMULP
 *     step  = (rate > 0.0f) ? (1.0f / rate) : 0.0f;  // FCOM 0; TEST 0x41 (ZF|CF => <=0)
 * i.e. the reciprocal of the frame-scaled magnitude, or 0 when the magnitude is 0.
 * The two float inputs stay separate so their subtraction remains an unrounded
 * x87 value just as it does in the original clusters. */
static CG_TURRET_COMPAT_ALWAYS_INLINE float
cgame_compat_turret_blend_step(float sample, float reference)
{
    long double delta = (long double)sample - (long double)reference;
    long double magnitude = __builtin_fabsl(delta);
    long double rate = magnitude * (1000.0L / (long double)cg_frametime);
    if (rate > 0.0L) {
        return (float)(1.0L / rate);
    }
    return 0.0f;
}

void CG_PlayerTurretPositionAndBlend(centity_t *player)
{
    int32_t              vehicleEntityNum;
    clientInfo_t *animState;
    const bg_static_animation_t *legsAnimEntry;
    centity_t      *vehicle;      /* the turret vehicle centity */
    struct DObj_s       *turretSelf;   /* DObj pointer: trap(0xa5, vehicle->currentState.number) */
    DObjSkelMat         *weaponMatrix; /* "tag_weapon" world skeleton matrix */
    weaponInfo_t          *weapInfo;     /* bg_weaponInfos[vehicle->currentState.weapon] */
    XAnimTree           *animTree;     /* animState->animTree (self for XAnim traps) */
    uint16_t             turretFlags;  /* animState->legsAnimWord & 0xfdff */
    uint16_t             treeHandle;   /* Scr_GetAnimsIndex(bgs.animationTable.animTreeHandle) */

    float   weaponYaw;       /* vectosignedyaw(weaponMatrix forward) — tag_weapon yaw */
    /* aimTransform is one contiguous 4-row frame matrix at [ESP+0x80] (slot 0x7c):
     *   rows[0..2] = AnglesToAxisNegRight(vehicle->lerpAngles)  (the aim basis)
     *   rows[3]    = vehicle->lerpOrigin (dword copies at 0x30033cab/cbf/ccd);
     *                the player-vehicle aim DELTA exists only on the x87 stack,
     *                feeds the rows[2] dot product, and is popped (FSTP ST0 x3 at
     *                0x30033d15..0x30033d19) — rows[3] is never overwritten.
     * The translation row (slot 0x7c+0x24 = 0xa0) is read as MatrixMultiply43's rhs
     * translation row in the writeback (0x3004a9bf FADD [rhs+0x24]), so the two must
     * share one 0x30-byte object. */
    matrix43_t aimTransform;
    float   aimAlongUp;      /* dot(player-vehicle lerpOrigin delta, rows[2]) */

    vehicleEntityNum = player->currentState.vehicleEntityNum;

    /* Gate 1: the vehicle owner must be a real (non-client, non-sentinel) entity. */
    if (vehicleEntityNum < CG_TURRET_FIRST_VEHICLE_ENTITYNUM ||
        vehicleEntityNum == ENTITYNUM_NONE) {
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    const int32_t clientNum = player->currentState.clientNum;
    if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_PlayerTurretPositionAndBlend: "
                  "invalid client number %i",
                  clientNum);
        return;
    }
    animState = &bgs.clientinfo[clientNum];

    /* Gate 2: the anim state slot must be live and carry a valid turret state. */
    if (animState->infoValid == 0) {
        return;
    }
    /* +0x390 is a uint16_t (legsAnimWord) but the gate loads the full dword and
     * null-tests it (0x30033baf: MOV EAX,[anim+0x390]; TEST EAX,EAX). */
    if (animState->legsAnimWord == 0) {
        return;
    }
    legsAnimEntry =
        cgame_compat_anim_entry_from_word(animState->legsAnimEntryWord);
    if (legsAnimEntry == NULL) {
        return;
    }
    if ((legsAnimEntry->flags & BG_ANIM_ENTRY_TURRET) == 0) {
        return;
    }

    /* vehicle = &cg_entities[vehicleEntityNum]; must have an animated DObj. The
     * established cg_entities[] view is the 0x288-stride array based at
     * cg_entities (0x3048c6e0). */
    vehicle = cgame_compat_unchecked_cgentity(vehicleEntityNum);
    if (vehicle->currentValid == 0) {
        return;
    }

    /* self = trap(0xa5, vehicle->currentState.number) — the DObj handle for tag resolution. */
    turretSelf = (struct DObj_s *)(intptr_t)cgame_syscall(
        CG_DOBJ_GET_HANDLE, vehicle->currentState.number);
    if (turretSelf == NULL) {
        return;
    }

    /* Resolve the "tag_weapon" bone/world matrix on the vehicle DObj. */
    weaponMatrix = CG_DObjGetEntityBoneMatrix(turretSelf, "tag_weapon",
                                              (centity_t *)vehicle);
    if (weaponMatrix == NULL) {
        /* WARNING: aborting player positioning on turret since 'tag_weapon' does not
         * exist (0x30079a28). Early return. */
        Com_Printf("WARNING: aborting player positioning on turret since 'tag_weapon' does not exist\n");
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    {
        const int32_t weaponIndex = vehicle->currentState.weapon;
        if (weaponIndex <= 0 || weaponIndex > bg_numWeapons ||
            (uint32_t)weaponIndex >= (uint32_t)MAX_WEAPONS ||
            bg_weaponInfos[weaponIndex] == NULL) {
            Com_Printf(
                "WARNING: CG_PlayerTurretPositionAndBlend: "
                "invalid weapon index %i\n",
                weaponIndex);
            return;
        }
        weapInfo = bg_weaponInfos[weaponIndex];
    }

    /* Convert the MP anim-tree object to its engine index (only AX is kept). */
    treeHandle =
        (uint16_t)Scr_GetAnimsIndex(bgs.animationTable.animTreeHandle);

    /* Runtime tree used by every XAnim trap in the walk. */
    animTree = animState->animTree;

    /* Masked animation word forwarded as trap_XAnimSetGoalWeight's `flags` arg:
     * ANIM_TOGGLEBIT is cleared by AND 0xfdff at 0x30033c6c. */
    turretFlags = (uint16_t)(animState->legsAnimWord &
                             (uint16_t)~ANIM_TOGGLEBIT);

    /* weaponYaw = yaw of the tag_weapon forward vector (matrix[0],matrix[1]). */
    weaponYaw = vectosignedyaw(weaponMatrix->axis[0]);

    /* Build the aim axis (rows[0..2]) from the vehicle's lerpAngles. */
    AnglesToAxisNegRight(aimTransform.axis, vehicle->lerpAngles);

    /* aimTransform.origin = vehicle->lerpOrigin: the dword copies at
     * 0x30033cab/0x30033cbf/0x30033ccd store the VEHICLE angles into the
     * translation row, and nothing ever overwrites them — so MatrixMultiply43 at
     * 0x3003430a composes with translation = vehicle->lerpOrigin. The
     * player-vehicle deltas (FLD [player+0x208/20c/210]; FSUB rows[3][i]) are
     * formed only on the x87 stack for the dot product below and then popped
     * (FSTP ST0 x3 at 0x30033d15..0x30033d19). */
    aimTransform.origin[0] = vehicle->lerpOrigin[0];
    aimTransform.origin[1] = vehicle->lerpOrigin[1];
    aimTransform.origin[2] = vehicle->lerpOrigin[2];

    /* aimAlongUp = dot(player->lerpOrigin - vehicle->lerpOrigin, aimAxis[2]).
     * Machine (0x30033cf2..0x30033d11) FSTPs the RAW dot product into the
     * [ESP+0x44] slot (abs-204) with NO subtraction; that slot is later reused
     * verbatim as barrelOrigin's Z (translation row) in the writeback, and only
     * the walk's local blend target (targetAlongUp = dot - weaponMatrix[+0x38])
     * subtracts the matrix Z. So the slot must remain the pure dot here. */
    {
        /* The three (player - vehicle) angle deltas stay on the x87 stack
         * (FLD/FSUB @0x30033cb9..0x30033ceb) and feed the dot product from
         * registers, with a single FSTP @0x30033d11 — they are NOT rounded to
         * float slots, so no float temporaries here. */
        long double dx = (long double)player->lerpOrigin[0]
                       - (long double)vehicle->lerpOrigin[0];
        long double dy = (long double)player->lerpOrigin[1]
                       - (long double)vehicle->lerpOrigin[1];
        long double dz = (long double)player->lerpOrigin[2]
                       - (long double)vehicle->lerpOrigin[2];
        long double dot = dz * (long double)aimTransform.axis[2][2];
        dot = dot + dy * (long double)aimTransform.axis[2][1];
        dot = dot + dx * (long double)aimTransform.axis[2][0];
        aimAlongUp = (float)dot;
    }

    /*
     * ---- Bone-tree walk + per-bone weight blend (0x30033d1b..0x3003427e) ----
     *
     * The turret barrel is posed by walking the weapon DObj bone tree and setting
     * per-node interpolation weights (via trap_XAnimSetGoalWeight), so the DObj skeleton itself
     * carries the blended result; the walk keeps no local barrel origin. The final
     * barrel origin/orientation are read back from the posed skeleton by the
     * tag-placement trap (0x9d) in the writeback below.
     *
     * Node handles are packed dwords: high 16 bits = tree handle, low 16 bits =
     * bone/node index (exactly how trap_XAnimGetNumChildren/186 and the DObj name/placement traps
     * unpack them). `rootNodePacked` combines the resolved MP tree handle (high) with
     * the masked render-flags word (low): the setup at 0x30033c74/0x30033c79 wrote
     * treeHandle and turretFlags into the two halves of the same frame dword, which
     * the walk reloads whole.
     *
     * The DObj traps used here (proven ABIs from their own wrappers):
     *   trap_XAnimClearTreeGoalWeightsStrict(self, packedNode, 0.0f)  0x3003e780  reset subtree
     *   trap_XAnimGetNumChildren(packedNode)                         0x3003ede0  child count
     *   trap_XAnimGetChildAt(packedNode, childIndex) -> childPk  0x3003ee00  nth child
     *   trap_XAnimGetWeight(self, boneIndex16) -> float sample  0x3003e9f0  bone value (ECX=self)
     *   trap_XAnimSetGoalWeight(self, node, w0bits, w1bits, w2, 0,0) 0x3003e890 set node weights
     *   cgame_syscall(0x9d, self, boneIndex16, &outAngles, &outOrigin)  tag placement
     * The three trap_XAnimSetGoalWeight weight args are floats; the wrapper
     * forwards their raw dword payloads, matching the machine's PUSHes.
     */
    {
        int32_t  rootNodePacked;    /* (treeHandle<<16)|turretFlags — walk root */
        int32_t  rootChildCount;    /* trap_XAnimGetNumChildren(rootNodePacked) */
        int32_t  childIndex;        /* loop counter over root's children */
        int32_t  childPacked;       /* current child node handle (packed) */
        int32_t  childCount;        /* trap_XAnimGetNumChildren(childPacked) */
        int32_t  levelIndex;        /* integer bracket index within the child */
        int32_t  grandChild0;       /* bracketing grandchild (packed) */
        int32_t  grandChild1;       /* adjacent grandchild for the fractional split */
        float    targetAlongUp;     /* s_10: aimAlongUp - weaponMatrix->translationZ */
        long double bracketPos;     /* unspilled x87 bracket position within the child */
        float    frac;              /* fractional remainder bracketPos - levelIndex */
        float    matchedAlongUp;    /* s_20: along-up value of the last bracketing child */
        float    matchedFrac;       /* s_28: `frac` carried from the last bracketing child.
                                     * The machine stores frac's raw float bits here (FSUB'd
                                     * as a float downstream in the H/I refinement), NOT an
                                     * integer child index. */
        int32_t  matchedLevelIndex; /* s_34: levelIndex of the bracketing child */
        float    moveDelta[3];      /* s_44: trap 0x9d movement output (Z tests aim) */
        float    rotationDelta[3];  /* s_5c: trap 0x9d rotation vec2 in a 3-float slot */
        float    nodeSample;        /* trap_XAnimGetWeight bone-value sample */
        float    blendStep;         /* per-node rate-limited blend result */

        /* rootNodePacked = high:treeHandle, low:turretFlags (one packed dword). */
        rootNodePacked = coduo_int32_from_bits(
            ((uint32_t)treeHandle << SCR_ANIM_TREE_INDEX_SHIFT) |
            (uint32_t)turretFlags);

        /* targetAlongUp = aimAlongUp - weaponMatrix row-3 Z (FLD s_40 / FSUB
         * [weaponMatrix+0x38]). The machine stores the pure dot to s_40 first,
         * then subtracts the matrix Z once and spills this target as binary32. */
        targetAlongUp = (float)((long double)aimAlongUp -
                                (long double)weaponMatrix->origin[2]);

        /* Reset the root node, then count its children. */
        trap_XAnimClearTreeGoalWeightsStrict(animTree, rootNodePacked, 0.0f);
        rootChildCount = trap_XAnimGetNumChildren((uint32_t)rootNodePacked);

        /* Loop-carried bookkeeping seeded to zero (the zero stores at
         * 0x30033d3f..0x30033d50: matchedAlongUp, matchedFrac, matchedLevelIndex,
         * and the grandChild1 word slot). */
        matchedAlongUp    = 0.0f;
        matchedFrac       = 0.0f;
        matchedLevelIndex = 0;
        grandChild1       = 0;

        if (rootChildCount == 0) {
            Com_Error(1, "\x15Player anim '%s' has no children",
                               trap_XAnimGetAnimName(
                                   (uint32_t)rootNodePacked));
        }

        childIndex = 0;
        for (;;) {
            /* child = root's childIndex'th child. */
            childPacked = trap_XAnimGetChildAt((uint32_t)rootNodePacked, childIndex);

            /* Reset the child transform to identity (weights 1,1,1). */
            trap_XAnimSetGoalWeight(animTree, childPacked,
                                    1.0f, 1.0f, 1.0f, 0, qfalse);

            childCount = trap_XAnimGetNumChildren((uint32_t)childPacked);
            if (childCount == 0) {
                Com_Error(1, "\x15Player anim '%s' has no children",
                                   trap_XAnimGetAnimName(
                                       (uint32_t)childPacked));
            }

            /* bracketPos = childCount*0.5 - weaponYaw/animHorRotateInc, clamped to
             * [0, childCount-1] (an interpolated bone index). 0x30033dcd: FILD
             * childCount; FMUL 0.5 (0x3007bce8); FLD weaponYaw; FDIV
             * [weapInfo+0x48c]; FSUBP ST(1),ST0 (bytes de e9: ST1 <- ST1 - ST0) —
             * the half-count is the MINUEND. */
            bracketPos = (long double)childCount * 0.5f
                       - ((long double)weaponYaw /
                          (long double)weapInfo->animHorRotateInc);
            /* 0x30033de7..0x30033dfc clamps only ordered-negative values;
             * ordered zero and unordered values stay on the original ST0 path. */
            if (bracketPos < 0.0L) {
                bracketPos = 0.0L;
            } else {
                int32_t lastChild =
                    coduo_int32_from_bits((uint32_t)childCount - 1u);
                float lastChildAsFloat = (float)lastChild;
                if (bracketPos >= (long double)lastChildAsFloat) {
                    bracketPos = (long double)lastChildAsFloat;
                }
            }

            /* 0x3006be3c = MSVC CRT _ftol2 (Q_rint), truncates toward zero:
             * levelIndex = (int)bracketPos. */
            levelIndex = coduo_fp_to_i32_extended(bracketPos);
            frac = (float)(bracketPos - (long double)levelIndex);

            /* Weight the bracketing grandchild by (1 - frac). */
            grandChild0 = trap_XAnimGetChildAt((uint32_t)childPacked, levelIndex);
            trap_XAnimSetGoalWeight(animTree, grandChild0,
                                    1.0f - frac, 1.0f, 1.0f, 0, qfalse);

            /* When frac != 0, weight the adjacent grandchild by frac. */
            if (frac != 0.0f) {
                grandChild1 = trap_XAnimGetChildAt(
                    (uint32_t)childPacked,
                    coduo_int32_from_bits((uint32_t)levelIndex + 1u));
                trap_XAnimSetGoalWeight(animTree, grandChild1,
                                        frac, 1.0f, 1.0f, 0, qfalse);
            }

            /* Sample this child's absolute XAnim rotation/movement delta. */
            (void)cgame_syscall(CG_XANIM_CALC_ABS_DELTA, (intptr_t)animTree,
                                (uint16_t)childPacked,
                                rotationDelta, moveDelta);

            /* If the child's along-up (movement Z) is still below the target,
             * record it as the current bracket and advance; else it brackets.
             * The three carried stores are exactly (0x30033edd/ee8/eec):
             *   matchedFrac       = frac        (EAX = [s_14] = frac)
             *   matchedAlongUp    = moveDelta[2](EDX = [s_4c])
             *   matchedLevelIndex = levelIndex  (EBX)
             * There is NO store of grandChild0/grandChild1 into a "matched" slot
             * here; grandChild0 (s_54) and grandChild1 (s_38) simply retain the
             * last iteration's handles for the post-loop refinement. */
            /* 0x30033ec2: FLD moveDelta[2]; FCOMP targetAlongUp; TEST AH,0x1;
             * JZ exit — the record-and-continue leg fires on C0, i.e. on
             * less-than OR unordered; `!(a >= b)` preserves the NaN behavior. */
            if (!(moveDelta[2] >= targetAlongUp)) {
                matchedFrac       = frac;
                matchedAlongUp    = moveDelta[2];
                matchedLevelIndex = levelIndex;
                childIndex = coduo_int32_from_bits((uint32_t)childIndex + 1u);
                if (childIndex < rootChildCount) {
                    continue;
                }
            }
            break;
        }

        /*
         * ---- Post-loop blend refinement (0x30033efa..0x3003427e) ----
         *
         * The bracket-search loop left these carried values (all re-derived from a
         * full ESP frame trace; slots are R-relative, R = ESP after the four
         * callee-saved pushes):
         *   frac              (s_14): the last bracketing child's fractional split
         *   grandChild0       (s_54): last-iteration bracketing grandchild handle
         *   grandChild1       (s_38): last-iteration adjacent grandchild handle
         *   childPacked (EDI, s_24 root): the last-iteration child node handle
         *   targetAlongUp     (s_10): aimAlongUp - weaponMatrix Z
         *   matchedAlongUp    (s_20), matchedFrac (s_28), matchedLevelIndex (s_34)
         *
         * Each cluster: reset/sample a node with trap_XAnimGetWeight, form a per-frame blend
         * step with the compatibility blend spelling, and re-weight the node via trap_XAnimSetGoalWeight. The
         * trap_XAnimSetGoalWeight arg order proven from the wrapper (0x3003e890) and the pushes
         * is (self, node, w0, w1, w2, 0, 0); the three weights are raw float bit
         * patterns pushed as integer dwords (CG_FloatBits). node/self are the same
         * animTree self used by the whole walk.
         */
        /* Reset the root node once, then run clusters D and E unconditionally
         * (0x30033efa: the loop falls straight through to here). */
        trap_XAnimClearTreeGoalWeightsStrict(animTree, rootNodePacked, 0.0f);

        /* Cluster D (0x30033efa..0x30033f72): re-weight grandChild0 by (1-frac),
         * blended by the per-frame step toward its own sample.
         *   sample = trap_XAnimGetWeight(grandChild0)
         *   step   = CG_TurretBlendStep(sample - (1.0f - frac))
         *   trap_XAnimSetGoalWeight(dObj, grandChild0, w0=1.0f-frac, w1=step, w2=1.0f) */
        nodeSample = trap_XAnimGetWeight(animTree, (uint16_t)grandChild0);
        blendStep = cgame_compat_turret_blend_step(nodeSample, 1.0f - frac);
        trap_XAnimSetGoalWeight(animTree, grandChild0,
                                1.0f - frac, blendStep, 1.0f, 0, qfalse);

        /* Cluster E (0x30033f8d..0x30033fef): only when frac is non-zero (FUCOMPP
         * frac vs 0.0 at 0x30033f7d(FLD s_14)..0x30033f8b, jnp skips) — i.e. the
         * last bracket had a fractional split, so the adjacent grandchild also needs
         * weighting. Re-weight grandChild1 by frac, blended by its own per-frame step.
         *   sample = trap_XAnimGetWeight(grandChild1)
         *   step   = CG_TurretBlendStep(sample - frac)
         *   trap_XAnimSetGoalWeight(dObj, grandChild1, w0=frac, w1=step, w2=1.0f) */
        if (frac != 0.0f) {
            nodeSample = trap_XAnimGetWeight(animTree, (uint16_t)grandChild1);
            blendStep = cgame_compat_turret_blend_step(nodeSample, frac);
            trap_XAnimSetGoalWeight(animTree, grandChild1,
                                    frac, blendStep, 1.0f, 0, qfalse);
        }

        /* Branch (0x30033ff2..0x30034002): interior childIndex → clusters F..I;
         * boundary childIndex (0 or rootChildCount) → tag_aim fallback. */
        if (childIndex != 0 && childIndex != rootChildCount) {
            int32_t child2;      /* trap_XAnimGetChildAt(rootNodePacked, childIndex-1) — EDI */
            int32_t grandChildH; /* trap_XAnimGetChildAt(child2, matchedLevelIndex) — EBX */
            int32_t grandChildI; /* trap_XAnimGetChildAt(child2, matchedLevelIndex+1) — EDI */
            float   factor;      /* along-up interpolation factor between brackets */

            /* factor = (targetAlongUp - matchedAlongUp) / (moveDelta[2] - matchedAlongUp)
             * (0x30034008..0x3003401d). moveDelta[2] is the last bracketing child's
             * along-up sample. */
            factor = (float)(
                ((long double)targetAlongUp - (long double)matchedAlongUp) /
                ((long double)moveDelta[2] - (long double)matchedAlongUp));

            /* Cluster F (0x30034008..0x3003407b): re-weight the last child node by
             * `factor`, blended by its per-frame step.
             *   sample = trap_XAnimGetWeight(childPacked)
             *   step   = CG_TurretBlendStep(sample - factor)
             *   trap_XAnimSetGoalWeight(dObj, childPacked, w0=factor, w1=step, w2=1.0f) */
            nodeSample = trap_XAnimGetWeight(animTree, (uint16_t)childPacked);
            blendStep = cgame_compat_turret_blend_step(nodeSample, factor);
            trap_XAnimSetGoalWeight(animTree, childPacked,
                                    factor, blendStep, 1.0f, 0, qfalse);

            /* Cluster G (0x3003407c..0x300340f6): child2 = the (childIndex-1)'th
             * child of the ROOT node; re-weight it by (1.0f - factor).
             *   child2 = trap_XAnimGetChildAt(rootNodePacked, childIndex - 1)
             *   sample = trap_XAnimGetWeight(child2)
             *   step   = CG_TurretBlendStep(sample - (1.0f - factor))
             *   trap_XAnimSetGoalWeight(dObj, child2, w0=1.0f-factor, w1=step, w2=1.0f) */
            child2 = trap_XAnimGetChildAt(
                (uint32_t)rootNodePacked,
                coduo_int32_from_bits((uint32_t)childIndex - 1u));
            nodeSample = trap_XAnimGetWeight(animTree, (uint16_t)child2);
            blendStep = cgame_compat_turret_blend_step(nodeSample, 1.0f - factor);
            trap_XAnimSetGoalWeight(animTree, child2,
                                    1.0f - factor, blendStep, 1.0f, 0, qfalse);

            /* Cluster H (0x300340f7..0x3003416e): grandChildH = the
             * matchedLevelIndex'th child of child2; re-weight it by
             * (1.0f - matchedFrac).
             *   grandChildH = trap_XAnimGetChildAt(child2, matchedLevelIndex)
             *   sample      = trap_XAnimGetWeight(grandChildH)
             *   step        = CG_TurretBlendStep(sample - (1.0f - matchedFrac))
             *   trap_XAnimSetGoalWeight(dObj, grandChildH, w0=1.0f-matchedFrac, w1=step, w2=1.0f) */
            grandChildH = trap_XAnimGetChildAt((uint32_t)child2, matchedLevelIndex);
            nodeSample  = trap_XAnimGetWeight(animTree, (uint16_t)grandChildH);
            blendStep = cgame_compat_turret_blend_step(
                nodeSample, 1.0f - matchedFrac);
            trap_XAnimSetGoalWeight(animTree, grandChildH,
                                    1.0f - matchedFrac, blendStep,
                                    1.0f, 0, qfalse);

            /* Cluster I (0x30034189..0x300341eb, merges into the trap_XAnimSetGoalWeight tail at
             * 0x30034275): only when matchedFrac != 0.0f (FUCOMPP matchedFrac vs 0.0
             * at 0x3003416f..0x30034183, jnp skips to the writeback). grandChildI =
             * the (matchedLevelIndex+1)'th child of child2; re-weight it by
             * matchedFrac.
             *   grandChildI = trap_XAnimGetChildAt(child2, matchedLevelIndex + 1)
             *   sample      = trap_XAnimGetWeight(grandChildI)
             *   step        = CG_TurretBlendStep(sample - matchedFrac)
             *   trap_XAnimSetGoalWeight(dObj, grandChildI, w0=matchedFrac, w1=step, w2=1.0f) */
            if (matchedFrac != 0.0f) {
                grandChildI = trap_XAnimGetChildAt(
                    (uint32_t)child2,
                    coduo_int32_from_bits((uint32_t)matchedLevelIndex + 1u));
                nodeSample  = trap_XAnimGetWeight(animTree, (uint16_t)grandChildI);
                blendStep = cgame_compat_turret_blend_step(nodeSample, matchedFrac);
                trap_XAnimSetGoalWeight(animTree, grandChildI,
                                        matchedFrac, blendStep,
                                        1.0f, 0, qfalse);
            }
        } else {
            /*
             * tag_aim fallback (0x300341f0..0x3003427e): the walk found no interior
             * bracketing child. Resolve "tag_aim" on the vehicle DObj (self =
             * turretSelf); if it does not exist, warn and abort. Otherwise blend the
             * last child node one final step toward tag_aim (weight 1.0f).
             *   sample = trap_XAnimGetWeight(childPacked)
             *   step   = CG_TurretBlendStep(sample - 1.0f)
             *   trap_XAnimSetGoalWeight(dObj, childPacked, w0=1.0f, w1=step, w2=1.0f) */
            if (CG_DObjGetEntityBoneMatrix(turretSelf, "tag_aim",
                                           (centity_t *)vehicle) == NULL) {
                Com_Printf("WARNING: aborting player positioning on turret since "
                           "'tag_aim' does not exist\n");
                return;
            }

            nodeSample = trap_XAnimGetWeight(animTree, (uint16_t)childPacked);
            blendStep = cgame_compat_turret_blend_step(nodeSample, 1.0f);
            trap_XAnimSetGoalWeight(animTree, childPacked,
                                    1.0f, blendStep, 1.0f, 0, qfalse);
        }

        /*
         * ---- Writeback (0x30034281..0x300343d3) ----
         *
         * Read the posed root-node delta, fold it through the tag_weapon world
         * matrix to a barrel transform, compose it with the aim transform, and store
         * the blended barrel angles/origin back into the player centity.
         */
        {
            float   rootMoveDelta[3]; /* s_44 out-buffer (0x9d arg4): RotatePoint2D'd
                                       * to a position, then + tag_weapon translation */
            float   rootRotationDelta[3]; /* s_5c out-buffer (0x9d arg3): its
                                            * leading vec2 is fed to RotationToYaw */
            matrix43_t barrelAxis; /* s_dc: YawToAxis basis + barrelOrigin */
            matrix43_t composedAxis; /* s_ac: MatrixMultiply43(barrelAxis, aimTransform) */
            float   barrelYaw;
            vec3_t  traceStart;    /* s_70: composed translation row (barrel origin) */
            vec3_t  traceEnd;      /* s_64: (T0, T1, vehicle->lerpOrigin[2]) */
            trace_t traceOut;   /* s_dc trace out-buffer */

            /* Read the root node's absolute rotation/movement delta. */
            (void)cgame_syscall(CG_XANIM_CALC_ABS_DELTA, (intptr_t)animTree,
                                (uint16_t)rootNodePacked,
                                rootRotationDelta, rootMoveDelta);

            /* Rotate the s_44 buffer's XY by weaponYaw, then offset by the tag_weapon
             * world translation → the barrel world origin XY. The Z of the barrel
             * origin is the raw aimAlongUp dot (see setup). (0x300342a2..0x300342d3) */
            RotatePoint2D(rootMoveDelta, weaponYaw);
            barrelAxis.origin[0] = rootMoveDelta[0] + weaponMatrix->origin[0];
            barrelAxis.origin[1] = rootMoveDelta[1] + weaponMatrix->origin[1];
            barrelAxis.origin[2] = aimAlongUp;

            /* barrelYaw = RotationToYaw(s_5c buffer) + weaponYaw (FADD the weaponYaw
             * slot at 0x300342df), folded into the barrel basis rows via YawToAxis. */
            barrelYaw = RotationToYaw(rootRotationDelta) + weaponYaw;
            YawToAxis(barrelYaw, barrelAxis.axis);

            /* composedAxis = barrelAxis (lhs=ECX) composed with the setup aim
             * transform (rhs=EAX), out=EDX (0x3003430a). MatrixMultiply43 is a full
             * affine 4x3 compose, so both operands' translation rows participate. */
            MatrixMultiply43(&barrelAxis, &aimTransform, &composedAxis);
            AxisToAngles(composedAxis.axis, player->lerpAngles);

            if (cg_debuganim_vmCvar.integer == CG_ANIMDEBUG_FREEZE_TURRET) {
                return;
            }

            /* player->lerpOrigin = composedAxis.origin; the
             * machine writes X,Z,Y in that store order (0x30034345..0x30034359). */
            player->lerpOrigin[0] = composedAxis.origin[0];
            player->lerpOrigin[2] = composedAxis.origin[2];
            player->lerpOrigin[1] = composedAxis.origin[1];

            /* Trace-project along the barrel: start = the blended barrel origin
             * (composed translation row), end = (X, Y, vehicle->lerpOrigin[2]).
             * ABI (0x300343a0): EAX=contentMask, ECX=&traceStart, EBX=0(flags);
             * stack (out, arg1, arg2, arg3) = (&traceOut, &traceEnd, 0,
             * player->currentState.number). If the trace was blocked, clamp lerpOrigin[2] to
             * the trace endpoint Z (traceOut.endpos[2] at +0x0c). */
            traceStart[0] = composedAxis.origin[0];
            traceStart[1] = composedAxis.origin[1];
            traceStart[2] = composedAxis.origin[2];
            traceEnd[0]   = composedAxis.origin[0];
            traceEnd[1]   = composedAxis.origin[1];
            traceEnd[2]   = vehicle->lerpOrigin[2];
            CG_Trace((int32_t)CG_TURRET_TRACE_CONTENTMASK, traceStart, 0,
                               &traceOut, traceEnd, 0,
                               player->currentState.number);
            if (traceOut.fraction < 1.0f) {
                /* MOV EAX,[traceOut+0xc]; MOV [player+0x210],EAX — a raw dword copy
                 * of traceOut.endpos[2] (the clipped endpoint Z) into lerpOrigin[2]. */
                player->lerpOrigin[2] = traceOut.endpos[2];
            }
        }
    }
}

#undef CG_TURRET_COMPAT_ALWAYS_INLINE
