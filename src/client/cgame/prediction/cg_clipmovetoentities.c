// Source: uo_cgame_mp_x86.dll 0x300350d0..0x3003530e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300350d0_3003530e.mcode
//
// CG_ClipMoveToEntities - refine a movement trace against the per-frame
// cg_solidEntities list. Inline brush models use CG_CM_INLINE_MODEL; encoded box
// and capsule solids use temporary collision models. The transformed collision
// trace is folded into the caller's running-best trace_t.
//
// NAME: the .mcode size-guess "CG_RegisterImpactEffects" is REJECTED - this
// registers no assets into an asset table. The replacement name is confirmed by
// the same-module symbol-bearing Mac binary.
//
// ABI: plain RET at 0x3003530d (caller-cleaned cdecl); EIGHT 32-bit stack params,
// all read at fixed [ESP+disp] offsets proven by an ESP trace of the body. The
// first four (arg1..arg4) are forwarded verbatim into the per-entity trace trap;
// arg5 is the exclude id, arg6 a contents mask, arg7 selects the trace variant,
// and arg8/out is the caller's result buffer that this function refines in place.
//
// The canonical centity layout and collision-model syscall ids live in client_recovered.h.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* Local per-entity scratch used to build the two vec3 buffers the trace trap consumes
 * and to receive its 48-byte trace_t before it is folded into *out. */
void CG_ClipMoveToEntities(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int32_t excludeId, int32_t flagsMask,
                           int32_t useVariant, trace_t *out)
{
    int32_t i;

    /* XOR EBX,EBX / MOV [loopIndex],EBX then the JLE guard: if the entity count is
     * <= 0 the whole body is skipped (signed compare against cg_numSolidEntities). */
    for (i = 0; i < cg_numSolidEntities; i++) {
        centity_t *cent = cg_solidEntities[i];

        /* Skip the vehicle temporarily removed from predicted-player collision
         * (+0x238 != 0) and the explicitly excluded entity (entityNum at +0x00 ==
         * arg5). Both jump to the loop tail. */
        if (cent->predictionCollisionActive != 0)
            continue;
        if (cent->currentState.number == excludeId)
            continue;

        int32_t emitHandle;             /* EDI: handle returned by the setup trap */
        vec3_t angles;                 /* [entry-0x60]: transformed-model angles */
        vec3_t origin;                 /* [entry-0x54]: transformed-model origin */

        if (cent->currentState.solid == SOLID_BMODEL) {
            /* Inline-brush path: resolve the entity's inline collision model and
             * evaluate its angle (+0x30 apos) and origin (+0x0c pos) trajectories
             * directly into the two transformed-trace buffers. */
            emitHandle = (int32_t)cgame_syscall(CG_CM_INLINE_MODEL, cent->currentState.itemIndex);

            /* BG_EvaluateTrajectory uses the client register ABI (result in ECX,
             * trajectory in EBX, atTime in EAX); modeled here as a normal call.
             * The apos trajectory is evaluated into emitBuf0, the pos trajectory
             * into origin, both at cg_latestSnapshotTime. */
            BG_EvaluateTrajectory(&cent->currentState.apos, cg_latestSnapshotTime, angles);
            BG_EvaluateTrajectory(&cent->currentState.pos, cg_latestSnapshotTime, origin);
        } else {
            /* Decode the packed entityState.solid bounds. */
            uint32_t encodedSolid = cent->currentState.solid;

            /* Three dimensions extracted from packed solid, each converted
             * to float via FILD (matching the exact MOVZX/DEC/SHR/SUB extractions):
             *   c0 = (rgba      ) & 0xff                    (FILD [+0x14])
             *   c1 = ((rgba >> 8) & 0xff) - 1               (MOVZX AH; DEC; FILD [+0x18])
             *   c2 = ((rgba >>16) & 0xff) - 0x20            (SHR 16; MOVZX AL; SUB 0x20; FILD [+0x10]) */
            int32_t c0 = (int32_t)(encodedSolid & 0xffu);
            int32_t c1 = (int32_t)((encodedSolid >> 8) & 0xffu) - 1;
            int32_t c2 = (int32_t)((encodedSolid >> 16) & 0xffu) - 0x20;
            float fc0 = (float)c0;
            float fc1 = (float)c1;
            float fc2 = (float)c2;

            /* Two adjacent vec3 buffers built by the x87 sequence at 0x30035172..
             * 0x300351b1 (traced FST/FSTP by ST-stack order and store offset):
             *   FILD c0; FLD ST0; FCHS -> ST0=-fc0, ST1=+fc0
             *   FST [+0x44]=-fc0; FSTP [+0x40]=-fc0 (pop); ST0=+fc0
             *   FST [+0x38]=+fc0; FSTP [+0x34]=+fc0 (pop)
             *   FILD c1; FCHS; FSTP [+0x48]=-fc1
             *   FILD c2;       FSTP [+0x3c]=+fc2
             * By ascending stack address the two buffers passed to the setup trap are
             *   maxs (at [+0x34..+0x3c] words) = { +fc0, +fc0, +fc2 }
             *   mins (at [+0x40..+0x48] words) = { -fc0, -fc0, -fc1 } */
            vec3_t maxs;
            vec3_t mins;
            maxs[0] = fc0;     /* [+0x34] */
            maxs[1] = fc0;     /* [+0x38] */
            maxs[2] = fc2;     /* [+0x3c] */
            mins[0] = -fc0;    /* [+0x40] */
            mins[1] = -fc0;    /* [+0x44] */
            mins[2] = -fc1;    /* [+0x48] */

            /* Content flag pushed as the trap's third argument: CONTENTS_BODY
             * (0x2000000) for ET_PLAYER, otherwise 1 (0x300351b8 DEC EAX sets ZF
             * on eType==1; JNZ keeps the preloaded 1, the eType==1 fall-through
             * loads 0x2000000). Also AND-tested against the flags mask; a zero
             * result skips the whole entry. */
            int32_t contentFlag = (cent->currentState.eType == ET_PLAYER) ? (int32_t)CONTENTS_BODY : (int32_t)CONTENTS_SOLID;
            if ((flagsMask & contentFlag) == 0)
                continue;

            /* eFlags bit 0x10 selects a temporary capsule rather than a box. */
            if (cent->currentState.eFlags & EF_CAPSULE)
                emitHandle = (int32_t)cgame_syscall(CG_CM_TEMP_CAPSULE_MODEL, (intptr_t)mins, (intptr_t)maxs, contentFlag);
            else
                emitHandle = (int32_t)cgame_syscall(CG_CM_TEMP_BOX_MODEL, (intptr_t)mins, (intptr_t)maxs, contentFlag);

            /* Encoded box/capsule solids have no rotation; their transformed-trace
             * origin is the centity's interpolated origin at +0x208. */
            angles[0] = 0.0f;
            angles[1] = 0.0f;
            angles[2] = 0.0f;
            origin[0] = cent->lerpOrigin[0];
            origin[1] = cent->lerpOrigin[1];
            origin[2] = cent->lerpOrigin[2];
        }

        /* Per-entity transformed trace into a local trace_t. arg7 selects the
         * variant: zero -> CG_CM_TRANSFORMED_BOX_TRACE (0x27), non-zero -> CG_CM_TRANSFORMED_CAPSULE_TRACE (0x29). Both
         * take the identical nine-argument shape (proven byte-for-byte from the two
         * mirror push sequences): the forwarded caller args, collision-model
         * handle, contents mask, origin, and angles. */
        trace_t local;
        int32_t emitId = (useVariant != 0) ? CG_CM_TRANSFORMED_CAPSULE_TRACE : CG_CM_TRANSFORMED_BOX_TRACE;
        cgame_syscall(emitId, (intptr_t)&local, (intptr_t)start, (intptr_t)end, (intptr_t)mins, (intptr_t)maxs, emitHandle, flagsMask,
                      (intptr_t)origin, (intptr_t)angles);

        /* Fold the local result into the caller's running-best buffer *out.
         * Replace *out with the local result when the local trace registered a hit
         * (+0x2e byte != 0) OR when its fraction < out->fraction (the exact
         * FCOMP/FNSTSW/TEST AH,0x5/JNP condition: after masking C0|C2, only the
         * ST0 < mem case leaves the single C0 bit set = parity-odd, so JNP takes
         * the copy path exactly for local.fraction < out->fraction; greater,
         * equal, and unordered all fall through — the classic Q3
         * "trace.fraction < tr->fraction" nearest-hit fold). Otherwise, if the
         * local +0x2f byte is set, mark out+0x2f instead. */
        if (local.allsolid != 0 || local.fraction < out->fraction) {
            /* Stamp the local result's entityNum (+0x28) with the entity number's low
             * word (MOV CX,[ESI]) then copy all 48 bytes (REP MOVSD, 12 dwords)
             * into *out. */
            local.entityNum = (uint16_t)cent->currentState.number;
            *out = local;
        } else if (local.startsolid != 0) {
            out->startsolid = 1;
        }

        /* After the fold, if out is now all-solid (out+0x2e != 0) the walk stops
         * early (the machine POPs and returns from inside the loop). */
        if (out->allsolid != 0)
            break;
    }
}
