// Source: uo_cgame_mp_x86.dll 0x30023d50..0x30023fce
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30023d50_30023fce.mcode
//
// CG_SpawnFlameChunkOnBone (0x30023d50) — allocate one flame chunk from the pool
// and attach it to a named bone/tag of a client entity's DObj skeleton, stamping
// its trajectory and lifetime and computing its initial world position.
//
// Identity / name adjudication:
//   The .mcode header carries the SIZE-match guess "SpectatorClientEndFrame"
//   (win 0x27e ~ 0x280). It is REJECTED: this routine allocates a flame chunk
//   (CG_SpawnFlameChunk 0x30025600), prints "Out of flame chunks\n" (0x300777b8)
//   on pool exhaustion, resolves a bone/tag index via the DObj traps
//   0xa5/0xb2, and stamps a flame-chunk record; it is the flame-chunk-on-bone
//   spawner, not a spectator end-of-frame routine. The existing shared decl in
//   client_recovered.h already names it CG_SpawnFlameChunkOnBone; this
//   reconstruction confirms that identity and the declared signature.
//
// Entry ABI (proven from the machine code and the sole caller at 0x30024031):
//   * slot         -> EAX          (register-passed centity_t*, may be NULL)
//   * pos          -> stack arg0   (E+4;  an optional explicit vec3 spawn point, NULL => bone tag)
//   * boneName     -> stack arg1   (E+8;  const char*, the tag/bone name)
//   * durationMsec -> stack arg2   (E+0xc; int, e.g. 3500 at the caller)
//   * startSpeed   -> stack arg3   (E+0x10; float, e.g. 60.0f (0x42700000) at the caller)
//   * count        -> stack arg4   (E+0x14; int, e.g. 20 at the caller)
//   Caller-cleaned cdecl (plain RET, no immediate). This exactly matches the
//   existing decl `void CG_SpawnFlameChunkOnBone(centity_t *slot,
//   const vec3_t pos, const char *boneName, int32_t durationMsec,
//   float startSpeed, int32_t count)`; no decl change was needed.
//
// Machine-code facts preserved:
//   * All time stamps (field_48/50/68/80/130/138) are x87 DOUBLE (FILD/FST/FSTP
//     QWORD); the two chunk-local coordinate runs and the sprite radius are
//     singles. cg_flameTime (0x300ab718) is read via FILD as a signed int.
//   * The bone-attached path transforms `pos` into bone-local space:
//     d = pos - worldBone.translation(row3); field_70/74/78 = worldBone.basis · d.
//   * The final EDX store of field_78 carries the third dot-product component as
//     raw float bits (MOV, not FSTP); modeled as an ordinary float assignment.

#include "client/cgame/client_recovered.h"

/* Flame-chunk kind stored in flameChunk_t.kind at spawn (0x30023db8:
 * MOV [EBX+0x2c],3). Provisional named value: CG_AddFlameSpriteToScene
 * (0x300268e0) / CG_UpdateFlamethrowerSounds (0x30029210) dispatch on this field with
 * the set {1,2,3,5}; the exact source enum name is unresolved. */
enum {
    FLAME_CHUNK_KIND_ON_BONE = 3,
    FLAME_CHUNK_WORLD_OWNER_INFO_INDEX = 1022
};

/* cg_entities[] view over the centity array base (stride 0x288 ==
 * sizeof(centity_t)); the established client convention. */

void CG_SpawnFlameChunkOnBone(centity_t *slot, const vec3_t pos,
                              const char *boneName, int32_t durationMsec,
                              float startSpeed, int32_t count)
{
    /* 0x30023d63: local DObj self-handle, initialized to 0; set only when a bone
     * name is present and resolved (E-0x80 in the machine frame). */
    struct DObj_s *dobjSelf = NULL;

    /* 0x30023d61..0x30023d75: when the slot exists but its DObj is absent
     * (slot->currentValid == 0), there is nothing to attach to — bail before allocating.
     * A NULL slot is allowed and skips this guard (world/free flame chunk). */
    if (slot != NULL && slot->currentValid == 0) {
        return;
    }

    /* 0x30023d7d..0x30023d84: allocate a root flame chunk (parent = NULL, passed
     * in ESI as XOR ESI,ESI). */
    flameChunk_t *chunk = CG_SpawnFlameChunk(NULL);

    /* 0x30023d86..0x30023da1: pool exhausted -> report and return. */
    if (chunk == NULL) {
        Com_PrintMessage("Out of flame chunks\n");
        return;
    }

    /* 0x30023da2..0x30023dd0: stamp the fixed spawn fields. startSpeed (a float)
     * is copied bit-for-bit into +0x58, +0x5c and +0xe4 (field_e4 is the chunk's
     * radius/expansion magnitude). */
    chunk->spawnTimeCopy =
        (double)coduo_int32_from_bits(cg_flameTime); /* FILD/FSTP QWORD */
    chunk->kind = FLAME_CHUNK_KIND_ON_BONE;
    chunk->startSpeed = startSpeed;
    chunk->radius = startSpeed;
    /* field_5c receives the same raw 32-bit startSpeed dword; it is typed
     * uint32_t (int gate elsewhere), so store the reinterpreted bits. */
    {
        chunk->startSpeedBits = CG_FloatBits(startSpeed);
    }
    chunk->smokeDensityRate = 0.0f;                    /* 0x30023dcb: MOV [EBX+0x60],0 */

    /* 0x30023dd2..0x30023e0a: resolve the flame-info / owner index (field_34) and,
     * when a bone name is given, the bone/tag handle (field_40 = boneIndex + 1). */
    if (slot != NULL) {
        int32_t entityNum = slot->currentState.number;   /* 0x30023ddd: MOV EAX,[EDI] */
        chunk->ownerInfoIndex = entityNum;

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if ((uint32_t)entityNum >= (uint32_t)MAX_GENTITIES) {
            chunk->ownerInfoIndex = FLAME_CHUNK_WORLD_OWNER_INFO_INDEX;
            chunk->boneHandle = 0;
        } else if (boneName != NULL) {          /* 0x30023ddb/de2: TEST ESI,ESI; JZ */
            /* 0x30023de4..0x30023dfb: query the entity's DObj handle then resolve
             * the named bone/tag on it. */
            dobjSelf = (struct DObj_s *)(uintptr_t)cgame_syscall(
                CG_DOBJ_GET_HANDLE, entityNum);
            int32_t boneIndex = coduo_int32_from_bits((uint32_t)cgame_syscall(
                CG_DOBJ_GET_BONE_INDEX, (intptr_t)dobjSelf,
                (intptr_t)boneName));
            chunk->boneHandle = coduo_int32_from_bits(
                (uint32_t)boneIndex + 1u); /* INC EAX; MOV [EBX+0x40] */
        }
    } else {
        /* 0x30023e0a: no slot -> a fixed sentinel flame-info index (1022). */
        chunk->ownerInfoIndex = FLAME_CHUNK_WORLD_OWNER_INFO_INDEX;
    }

    /* 0x30023e11..0x30023e30: copy the flame-info index into +0x38 and stamp the
     * trajectory timestamps. field_48 = spawn time; field_50 = spawn time +
     * 2*durationMsec (the chunk's end/expire time). Both doubles. */
    chunk->ownerClientNum = chunk->ownerInfoIndex;
    long double spawnTimeWide =
        (long double)coduo_int32_from_bits(cg_flameTime);
    chunk->spawnTime = (double)spawnTimeWide;
    chunk->endTime = (double)(
        (long double)durationMsec * 2.0L + spawnTimeWide);

    /* 0x30023e32: with no explicit position, skip the position-transform entirely
     * (field_70/74/78 are left as CG_SpawnFlameChunk initialized them). */
    if (pos != NULL) {
        /* 0x30023e38..0x30023e3d: a bone-attached chunk (boneHandle != 0)
         * transforms `pos` into bone-local space; otherwise the raw position is
         * stored. */
        if (chunk->boneHandle != 0) {
            /* 0x30023e43..0x30023e5a: calculate this DObj bone hierarchy.
             * ABI: self in ESI (dobjSelf), bone index (boneHandle-1) in EDI,
             * &cg_entities[field_34] as the owning-entity stack arg. */
            centity_t *ent = &cg_entities[chunk->ownerInfoIndex];
            int32_t calcBoneIndex = coduo_int32_from_bits(
                (uint32_t)chunk->boneHandle - 1u);
            int32_t boneCount = coduo_int32_from_bits((uint32_t)cgame_syscall(
                CG_DOBJ_NUM_BONES, (intptr_t)dobjSelf));
            if ((uint32_t)calcBoneIndex >= (uint32_t)boneCount) {
                /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                chunk->boneHandle = 0;
                chunk->localPos[0] = pos[0];
                chunk->localPos[1] = pos[1];
                chunk->localPos[2] = pos[2];
                goto position_ready;
            }
            CG_DObjCalcBone(dobjSelf, calcBoneIndex, ent);

            /* 0x30023e5f..0x30023e78: fetch the entity's per-bone matrix table. */
            DObjSkelMat *boneTable = (DObjSkelMat *)(intptr_t)
                cgame_syscall(CG_DOBJ_GET_BONE_MATRICES,
                              (intptr_t)dobjSelf, 0);
            if (boneTable == NULL) {
                /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                chunk->boneHandle = 0;
                chunk->localPos[0] = pos[0];
                chunk->localPos[1] = pos[1];
                chunk->localPos[2] = pos[2];
                goto position_ready;
            }

            /* 0x30023e6f..0x30023ec5: build the local placement matrix from the
             * entity's lerpAngles (basis rows) and lerpOrigin (translation
             * row), then compose it into the engine bone matrix for this bone
             * (boneTable + (boneHandle-1)*0x40). Output is a padded 4x4. */
            matrix43_t local;
            AnglesToAxisNegRight(local.axis, ent->lerpAngles);
            local.origin[0] = ent->lerpOrigin[0];
            local.origin[1] = ent->lerpOrigin[1];
            local.origin[2] = ent->lerpOrigin[2];

            int32_t matrixBoneIndex = coduo_int32_from_bits(
                (uint32_t)chunk->boneHandle - 1u);
            const DObjSkelMat *boneMatrix = &boneTable[matrixBoneIndex];
            DObjSkelMat world;
            CG_ComposeBoneMatrix(boneMatrix, &local, &world);

            /* 0x30023eca..0x30023f61: express `pos` in the bone's local frame.
             * d = pos - worldTranslation (world row 3, at world[12..14]);
             * field_70/74/78 = worldBasis (rows 0..2) dotted with d. The third
             * component is carried through EDX (raw float bits) and stored last.
             * Asymmetric spill: dy/dz are rounded to float slots (FSTP 30023ede /
             * 30023eec) but dx is NEVER stored — it stays on the x87 stack and is
             * consumed unrounded via FMUL ST2/ST3/ST4, so it must not be a float
             * local. Each dot product's FADDP chain order is
             * (col2*dz + col1*dy) + col0*dx, not row-major. */
            long double dx =
                (long double)pos[0] - (long double)world.origin[0];
            float dy = (float)(
                (long double)pos[1] - (long double)world.origin[1]);
            float dz = (float)(
                (long double)pos[2] - (long double)world.origin[2]);

            long double localX =
                (long double)world.axis[0][2] * (long double)dz +
                (long double)world.axis[0][1] * (long double)dy +
                (long double)world.axis[0][0] * dx;
            long double localY =
                (long double)world.axis[1][2] * (long double)dz +
                (long double)world.axis[1][1] * (long double)dy +
                (long double)world.axis[1][0] * dx;
            long double localZ =
                (long double)world.axis[2][2] * (long double)dz +
                (long double)world.axis[2][1] * (long double)dy +
                (long double)world.axis[2][0] * dx;

            /* 0x30023f3e rounds Z into a temporary first; X and Y are then
             * stored from their still-extended x87 values, followed by Z's raw
             * temporary dword at 0x30023f61. */
            float localZStored = (float)localZ;
            chunk->localPos[0] = (float)localX;
            chunk->localPos[1] = (float)localY;
            chunk->localPos[2] = localZStored;
        } else {
            /* 0x30023f52..0x30023f61: no bone -> store the raw world position. */
            chunk->localPos[0] = pos[0];
            chunk->localPos[1] = pos[1];
            chunk->localPos[2] = pos[2];
        }
    }

position_ready:
    ;
    /* 0x30023f64..0x30023fa9: finalize the lifetime/scale fields and stamp the
     * drift-start timestamp (field_80). field_138 is the chunk life-rate
     * (2*count) / (field_50 - field_48); field_130 is stamped with cg_flameTime. */
    long double driftStartWide =
        (long double)coduo_int32_from_bits(cg_flameTime);
    int32_t originFlameTime = coduo_int32_from_bits(cg_flameTime);
    chunk->driftStartTime = (double)driftStartWide;    /* 0x30023f77: FST QWORD (non-pop) */
    chunk->soundAmpRate = 1.0f;                       /* 0x30023f85: MOV [EBX+0xb8],0x3f800000 */
    chunk->lifeFraction = 0.0f;                       /* 0x30023f8f: MOV [EBX+0xe8],0 */
    chunk->lifeRate = (double)(
        ((long double)count + (long double)count) /
        ((long double)chunk->endTime -
         (long double)chunk->spawnTime));
    chunk->lifeStartTime = (double)driftStartWide;

    /* 0x30023faf: compute the chunk's initial world origin (into field_d8 vec3),
     * using cg_flameTime and the trajectory just stamped. */
    CG_ComputeFlameChunkOrigin(chunk, originFlameTime, &chunk->worldPos[0]);

    /* 0x30023fb4..0x30023fbe: re-stamp the drift-start timestamp after the origin
     * compute (the compute may have advanced internal state). */
    chunk->driftStartTime = (double)coduo_int32_from_bits(cg_flameTime);
}
