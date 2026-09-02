// Source: uo_cgame_mp_x86.dll 0x30025990..0x30025c5c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30025990_30025c5c.mcode
//
// CG_ComputeFlameChunkOrigin (0x30025990) — per-frame update of a flame chunk's
// world position. The .mcode mechanical pre-hint "ClientConnect" (a game_mp_uo
// SERVER function, size-matched) is REJECTED: there is no connection/userinfo work
// here. This function is cgame flame-effect math — it resolves an entity's DObj
// bone matrix (traps 0xa5/0xa0), composes a placement matrix, and transforms the
// chunk's local offset into world space; free (non-bone) chunks are extrapolated
// with an x87 drift/turbulence term (CG_pow, clamp). Name is a proven role name;
// the exact CoD source symbol is unresolved (no cgame syscall-id table recovered).
//
// Call sites all pass the flame chunk in EBX (register), and two caller-cleaned
// stack args: cg_flameTime (int, FILD'd) and the output vec3 pointer. The primary
// caller is CG_SpawnFlameChunkOnBone (0x30023faf: PUSH &out; PUSH cg_flameTime;
// CALL; ADD ESP,8). Modeled as ordered parameters — the EBX register argument is a
// custom-regparm calling-convention detail, not source-level behavior, so no
// calling-convention attribute is added (syntax-only build).
//
// Machine-code facts pinned during reconstruction:
//   - EBX = flameChunk_t *f.
//   - out = arg1 (EBP), read via [ESP+0x90] after SUB ESP,0x84 + PUSH EBP.
//   - cg_flameTime = arg0, read via FILD [ESP+0x8c] (an int timestamp).
//   - float pool at 0x3007bce0=1.0f, 0x3007bce8=0.5f, 0x3007bcec=0.0f (dumped with
//     objdump -s -j .rdata); the clamp uses 1.0f (0x3007bce0) and 0.0f (0x3007bcec)
//     directly — NOT the adjacent 0.5f. Scale constants dumped exactly:
//     0x3007c29c=-1.5f, 0x3007bd94=0.001f, 0x3007c298=0.0034482758f, 0x3007c0e4=0.35f,
//     0x3007be00=0.0011111111f, 0x3007bde8=2.0 (double), 0x3007c1fc=0.65f.
//   - CG_pow (0x3006bb20) is the MSVC x87 `pow` intrinsic: base on ST(1), exp ST(0).
//     Here base = field_94*0.0011111111f, exp = 2.0, i.e. (field_94*0.0011111111)^2.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <math.h>

/* trap(0xa5, entityNum) -> DObj context handle; trap(0xa0, self, 0) -> bone-matrix
 * table base (stride 0x40 per bone). cgame_syscall is declared in globals.h. */

enum { CG_FLAME_CHUNK_STATIC_MODE_LIMIT = 2 }; /* +0x2c < 2 selects the vertical-drift term */

void CG_ComputeFlameChunkOrigin(flameChunk_t *f, int32_t cg_flameTime, vec3_t out)
{
    /* 0x30025990..0x300259de: three x87 temporaries computed up front for both paths.
     *   driftRadius   = f->expansionRate * -1.5f
     *   elapsedDrift  = (cg_flameTime - f->driftStartTime) * 0.001f   [seconds since drift start]
     *   elapsedSpawn  = (cg_flameTime - f->spawnTime) * 0.001f   [seconds since spawn]
     * (cg_flameTime is FILD'd as an int; the subtrahends are doubles. The 0.001
     * scale is FMUL float ptr [0x3007bd94] = 0x3a83126f, the FLOAT 0.001f, not
     * the double 0.001.) */
    float driftRadius = (float)(
        (long double)f->expansionRate * (long double)-1.5f);
    float elapsedDrift = (float)(
        ((long double)cg_flameTime -
         (long double)f->driftStartTime) *
        (long double)0.001f);
    float elapsedSpawn = (float)(
        ((long double)cg_flameTime -
         (long double)f->spawnTime) *
        (long double)0.001f);

    if (f->boneHandle != 0) {
        /* ------- 0x300259e4..0x30025b39: bone-attached chunk -------
         * Resolve the owning entity's DObj skeleton and transform the chunk's local
         * offset (field_70/74/78) by the world-space bone matrix. */
        int32_t entityNum = f->ownerInfoIndex;
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if ((uint32_t)entityNum >= (uint32_t)MAX_GENTITIES) {
            return;
        }
        centity_t *entity =
            cg_entities + entityNum;

        /* 0x300259ef: skip the whole transform if the entity has no DObj model
         * (cg_entities[entityNum].currentValid == 0 -> the pool value at base+0x1e8). */
        if (entity->currentValid == 0) {
            return;
        }

        /* 0x30025a04: self = trap(CG_DOBJ_GET_HANDLE, entityNum). 0 -> no DObj. */
        intptr_t self = cgame_syscall(CG_DOBJ_GET_HANDLE, entityNum);
        if (self == 0) {
            return;
        }

        /* 0x30025a1a..0x30025a2c: calculate this DObj bone hierarchy.
         * ABI: self in ESI, boneIndex (boneHandle-1) in EDI, and
         * &cg_entities[entityNum] as the owning-entity stack arg. */
        const int32_t boneIndex = coduo_int32_from_bits(
            (uint32_t)f->boneHandle - 1u);
        int32_t boneCount = coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_DOBJ_NUM_BONES, self));
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if ((uint32_t)boneIndex >= (uint32_t)boneCount) {
            return;
        }
        CG_DObjCalcBone((void *)self, boneIndex,
                       (centity_t *)entity);

        /* 0x30025a31: boneTable = trap(CG_DOBJ_GET_BONE_MATRICES, self, 0). */
        DObjSkelMat *boneTable = (DObjSkelMat *)(intptr_t)
            cgame_syscall(CG_DOBJ_GET_BONE_MATRICES, self, 0);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (boneTable == NULL) {
            return;
        }

        /* 0x30025a4d..0x30025a91: build the local placement matrix — axis rows from
         * the entity's lerpAngles (+0x214), translation row = lerpOrigin
         * (+0x208). AnglesToAxisNegRight writes rows[0..2]; rows[3] = origin. */
        matrix43_t local;
        AnglesToAxisNegRight(local.axis, entity->lerpAngles);
        local.origin[0] = entity->lerpOrigin[0];
        local.origin[1] = entity->lerpOrigin[1];
        local.origin[2] = entity->lerpOrigin[2];

        /* 0x30025a7e..0x30025a9e: compose into the engine bone matrix for this bone
         * (boneTable + (boneHandle-1)*0x40, i.e. (boneHandle<<6)-0x40). Output is a
         * padded 4x4 (row3 = world translation). */
        const DObjSkelMat *boneMatrix = &boneTable[boneIndex];
        DObjSkelMat world;
        CG_ComposeBoneMatrix(boneMatrix, &local, &world);

        /* 0x30025aa3..0x30025b2e: transform the chunk's local offset by the composed
         * matrix. world is row-major 4x4 (stride 4): rows[0..2] are the basis, row3
         * is the translation. out = translation + row0*field_70 + row1*field_74 +
         * row2*field_78 (each written component-wise across three FLD/FMUL/FADD runs).
         *   row0 = {world[0],world[1],world[2]}
         *   row1 = {world[4],world[5],world[6]}
         *   row2 = {world[8],world[9],world[10]}
         *   translation = {world[12],world[13],world[14]}
         * (The initial dword MOVs of the translation into out[] are dead — every
         * component is overwritten by the accumulated FSTP below.) */
        out[0] = (float)((long double)world.axis[0][0] *
                         (long double)f->localPos[0] +
                         (long double)world.origin[0]);
        out[1] = (float)((long double)world.axis[0][1] *
                         (long double)f->localPos[0] +
                         (long double)world.origin[1]);
        out[2] = (float)((long double)world.axis[0][2] *
                         (long double)f->localPos[0] +
                         (long double)world.origin[2]);

        out[0] = (float)((long double)world.axis[1][0] *
                         (long double)f->localPos[1] +
                         (long double)out[0]);
        out[1] = (float)((long double)world.axis[1][1] *
                         (long double)f->localPos[1] +
                         (long double)out[1]);
        out[2] = (float)((long double)world.axis[1][2] *
                         (long double)f->localPos[1] +
                         (long double)out[2]);

        out[0] = (float)((long double)world.axis[2][0] *
                         (long double)f->localPos[2] +
                         (long double)out[0]);
        out[1] = (float)((long double)world.axis[2][1] *
                         (long double)f->localPos[2] +
                         (long double)out[1]);
        out[2] = (float)((long double)world.axis[2][2] *
                         (long double)f->localPos[2] +
                         (long double)out[2]);
        return;
    }

    /* ------- 0x30025b3a..0x30025c5b: free (non-bone) chunk -------
     * Linear extrapolation of the stored position by the drift velocity, plus a
     * vertical turbulence term on out[2]. */

    /* 0x30025b3a..0x30025b7d: out = position + elapsedDrift * field_94 * dir. */
    out[0] = (float)(
        (long double)elapsedDrift * (long double)f->driftSpeed *
            (long double)f->driftDir[0] +
        (long double)f->localPos[0]);
    out[1] = (float)(
        (long double)elapsedDrift * (long double)f->driftSpeed *
            (long double)f->driftDir[1] +
        (long double)f->localPos[1]);
    long double outZWide =
        (long double)elapsedDrift * (long double)f->driftSpeed *
            (long double)f->driftDir[2] +
        (long double)f->localPos[2];
    float outZBase = (float)outZWide; /* 0x30025b79 FST retained; 0x30025b7d FSTP */
    out[2] = outZBase;

    /* 0x30025b80..0x30025b98: sizeBias = (1.0f - field_e4*(1/290.0f)) * 0.35f.
     * [0x3007c298] = 0x3b61fc78 = 0.0034482758f (== 1/290). The old literal
     * 0.003448276f rounds to 0x3b61fc79 — one ULP high, a different float. */
    float sizeBias = (float)(
        ((long double)1.0f -
         (long double)f->radius * (long double)0.0034482758f) *
        (long double)0.35f);

    /* 0x30025b9c..0x30025c0c: w = 0.65f * (field_94*0.0011111111f)^2 + sizeBias,
     * clamped to [0.0f, 1.0f]. The pow term is recomputed on the in-range branch
     * (0x30025bd6) exactly as on the first evaluation (0x30025b9c); both calls
     * are retained below.
     * The whole chain — the pow base, the raw CG_pow st0 result, the 0.65f
     * multiply, the sizeBias add and BOTH clamp compares — stays in st registers
     * with NO float store, so w is long double and the pow call is inlined
     * a double pow base/return would each add a rounding the DLL does not
     * perform, so powl widens the base OPERAND and keeps the result 80-bit). */
    long double w =
        0.65f *
            powl((long double)f->driftSpeed * 0.0011111111f, 2.0L) +
        sizeBias;
    if (w < 0.0f) {                 /* 0x30025bbf FCOMP 0.0f, JP-clamp low */
        w = 0.0f;
    } else {
        /* The retail path repeats the complete pow/scale/bias chain at
         * 0x30025bd6 before the upper clamp instead of reusing its first value. */
        w = 0.65f *
                powl((long double)f->driftSpeed * 0.0011111111f, 2.0L) +
            sizeBias;
        if (w > 1.0f) {             /* 0x30025bf7 FCOM 1.0f, JNZ-clamp high */
            w = 1.0f;
        }
    }

    /* 0x30025c0c..0x30025c4f: modeScale = (field_2c < 2) ? 1 : 0; then
     * out[2] = outZBase
     *          - ( modeScale*(1.0f - w)*elapsedDrift*elapsedSpawn*driftRadius
     *              + elapsedDrift*elapsedSpawn*field_a8 ) * field_b8.
     * The tail (0x30025c1b..0x30025c4f) is ONE st-register chain — both product
     * terms, the FADDP, the soundAmpRate multiply and the FSUBR — with a single
     * FSTP to out[2], so it is written as one expression (float termA/termB
     * temporaries would round where the DLL does not). */
    float modeScale = (f->kind < CG_FLAME_CHUNK_STATIC_MODE_LIMIT) ? 1.0f : 0.0f;
    out[2] = (float)(
        (long double)outZBase -
        ((long double)modeScale * ((long double)1.0f - w) *
             (long double)elapsedDrift * (long double)elapsedSpawn *
             (long double)driftRadius +
         (long double)elapsedDrift * (long double)elapsedSpawn *
             (long double)f->expansionRate) *
            (long double)f->soundAmpRate);
}
