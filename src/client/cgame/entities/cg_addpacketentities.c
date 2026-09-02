// Source: uo_cgame_mp_x86.dll 0x3001f810..0x3001fb71
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001f810_3001fb71.mcode

#include <string.h>

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_AddPacketEntities (0x3001f810) — the cgame per-frame entity add pass.
 *
 * Named by behavior + call graph, NOT by size. The .mcode header's guessed name
 * `FinishSpawningItem` is a pure size match (win size 0x361 == matched 0x360) and
 * is REJECTED: this function does no item spawning; it is the once-per-frame pass
 * that (1) advances every incoming-snapshot entity's DObj animation, (2) rebuilds
 * the local player's predicted-event centity from the predicted playerState, and
 * (3) adds every snapshot entity — plus that predicted-event centity — to the
 * render/sound scene via CG_AddCEntity (0x30022170). Proven from:
 *   - iterating cg_nextSnap->entities[0..numEntities) twice (stride 0xf4 = the
 *     CoD entityState_t size), the classic CG_AddPacketEntities loop shape;
 *   - CG_AddCEntity (0x30022170) being invoked on each entity and on
 *     cg_predictedEventEntity, i.e. adding client entities to the scene;
 *   - BG_PlayerStateToEntityState (0x30006590) projecting cg.predictedPlayerState
 *     into the local-player entityState template, then MOVSD.REP-copying that
 *     0x3d-dword (244-byte = entityState_t) template into cg_predictedEventEntity;
 *   - the DObj begin/anim-advance/notetrack traps (0x97/0x98/0x99) that drive
 *     each entity's skeletal animation.
 * Its sole caller is the top-level frame builder at 0x30042160.
 *
 * ABI: no arguments (the caller `CALL 0x3001f810` with nothing set up), void
 * return. Callee-saved EBX/EBP/ESI/EDI pushed; a 0x14-byte stack frame holds the
 * FILD/FSTP float scratch ([esp+0x14]) and the three AngleVectors forward-vector
 * temporaries ([esp+0x18..0x20]). EBP is used as the constant 0 (XOR EBP,EBP)
 * throughout — reflected below as the literal 0 / qfalse it holds.
 *
 * Instruction-level self-check performed against the .mcode for every branch,
 * memory access, call order/args, constant, x87 op, and loop bound.
 */

/* cg_frametime (int ms) -> seconds; the machine code FILDs cg_frametime, rounds it
 * to float (FSTP/FLD [esp+0x14]), then FMULs by the FLOAT constant at 0x3007bd94
 * (0x3a83126f == 0.001f). The multiplier is written as the literal 0.001f: a
 * `1.0f / 1000.0f` expression would evaluate in extended precision under x87
 * excess-precision rules and multiply by a different (64-bit) 1/1000. */

/* The snapshot-flag test at 0x3001fa57 masks ps.playerStateFlags with 0xc0000:
 * two adjacent status bits (bits 0x40000 | 0x80000). Only the combined mask is
 * proven-consumed here (nonzero -> the local player has a live first-person DObj to
 * animate); the individual bit meanings are not resolved by this function. */
/* cg_dumpAnims_vmCvar.integer must be in [0, MAX_GENTITIES) (0x400); MAX_GENTITIES is the
 * shared macro from client_recovered.h. */

void CG_AddPacketEntities(void)
{
    int i;

    /* 3001f817: trap(0x97) — invalidate the client DObj skeleton cache for the
     * new frame (id only pushed, ADD ESP,4). */
    cgame_syscall(CG_DOBJ_INVALIDATE_SKELS);

    /* First pass: advance each incoming-snapshot entity's DObj animation.
     * 3001f822: EAX = cg_nextSnap; ECX = cg_nextSnap->numEntities. EBX = i (loop
     * counter), ESI = i*0xf4 byte offset into entities[]. */
    for (i = 0; i < cg_nextSnap->numEntities; i++) {
        /* 3001f840: entityNum = cg_nextSnap->entities[i].number. */
        int32_t entityNum = cg_nextSnap->entities[i].number;

        /* 3001f847: dObjHandle = trap(0xa5, entityNum) (ADD ESP,8). */
        intptr_t dObjHandle = cgame_syscall(CG_DOBJ_GET_HANDLE, entityNum);
        if (dObjHandle != 0) {
            /* 3001f85a: advance the animation by cg_frametime seconds. FILD
             * cg_frametime; FMUL 0.001f; trap(0x98, dObjHandle, dtSeconds). */
            float frameMilliseconds = (float)cg_frametime;
            float dtSeconds = (float)((long double)frameMilliseconds * (long double)0.001f);
            cgame_syscall(CG_DOBJ_ADVANCE_SERVER_TIME, dObjHandle, CG_FloatBits(dtSeconds));

            /* 3001f883: re-read cg_nextSnap->entities[i].number into EDI, then
             * CG_SetGunHandFromNotetracks(entityNum) (CALL 0x3001f760). The reload
             * is the compiler re-fetching the entity number across the trap call. */
            CG_SetGunHandFromNotetracks(cg_nextSnap->entities[i].number);
        }
    }

    /* Second stage: rebuild the local player's predicted-event centity.
     *
     * 3001f8ae..3001f965: build the three per-frame preview orientations from
     * cg.time. Each record's spin yaw = ((cg_time & mask) * 360) / divisor, with
     * (mask, divisor) = (0xfff, 4095.0f), (0x7ff, 2048.0f), (0x3ff, 1024.0f); the
     * 0x168 (=360) IMUL runs on the masked integer, which is then FILD'd and FDIV'd
     * by the (float) divisor. pitch and roll are 0 (EBP). */
    {
        uint32_t t = cg_time;
        cgDObjPreviewOrientation_t *o0 = &cg_dobjPreviewOrientations[0];
        cgDObjPreviewOrientation_t *o1 = &cg_dobjPreviewOrientations[1];
        cgDObjPreviewOrientation_t *o2 = &cg_dobjPreviewOrientations[2];

        /* Integer masked-and-scaled products (0x168 == 360). Written before the
         * FILD/FDIV so the exact 32-bit multiply is preserved. */
        int32_t spin0 = (int32_t)(t & 0xfffu) * 360;
        int32_t spin1 = (int32_t)(t & 0x7ffu) * 360;
        int32_t spin2 = (int32_t)(t & 0x3ffu) * 360;

        /* Each FILD result is first rounded through the shared float scratch.
         * The first FDIV result remains live on x87 through the six zero stores. */
        float spin0AsFloat = (float)spin0;
        long double yaw0Carrier = (long double)spin0AsFloat / (long double)4095.0f;

        /* 3001f901: cg_shakeExternAmplitude cleared to 0 (MOV [0x3048b5c0],EBP with
         * EBP==0). Consumed by CG_CalcViewShake (0x3001b550), which merges it as an
         * external amplitude candidate into the camera-shake maximum. */
        cg_shakeExternAmplitude = 0.0f;

        /* pitch=roll=0 for every record (EBP stores at +0x00 and +0x08). */
        o0->angles[0] = 0.0f;
        o0->angles[2] = 0.0f;
        o1->angles[0] = 0.0f;
        o1->angles[2] = 0.0f;
        o2->angles[0] = 0.0f;
        o2->angles[2] = 0.0f;

        /* 3001f8db/3001f92b: publish the retained first FDIV result. */
        o0->angles[1] = (float)yaw0Carrier;
        /* 3001f93f/3001f94d: o1.angles.yaw = spin1 / 2048.0f. */
        {
            float spin1AsFloat = (float)spin1;
            o1->angles[1] = (float)((long double)spin1AsFloat / (long double)2048.0f);
        }
        /* 3001f95b/3001f965: o2.angles.yaw = spin2 / 1024.0f. */
        {
            float spin2AsFloat = (float)spin2;
            o2->angles[1] = (float)((long double)spin2AsFloat / (long double)1024.0f);
        }

        /* 3001f96b: AngleVectors(o0.angles, forward=o0.forward, right=tmp,
         * up=o0.up), then store -tmp (the `right` output) into o0.negRight
         * (fld 0.0f; fsub tmp). AngleVectors' register outputs are forward=ESI,
         * right=EDI, up=EBX; here ESI=o0.forward, EDI=[esp+0x18] tmp, EBX=o0.up. */
        {
            vec3_t right;
            AngleVectors(o0->angles, o0->forward, right, o0->up);
            o0->negRight[0] = (float)(0.0L - (long double)right[0]);
            o0->negRight[1] = (float)(0.0L - (long double)right[1]);
            o0->negRight[2] = (float)(0.0L - (long double)right[2]);
        }
        /* 3001f9b3: same for record 1. */
        {
            vec3_t right;
            AngleVectors(o1->angles, o1->forward, right, o1->up);
            o1->negRight[0] = (float)(0.0L - (long double)right[0]);
            o1->negRight[1] = (float)(0.0L - (long double)right[1]);
            o1->negRight[2] = (float)(0.0L - (long double)right[2]);
        }
        /* 3001f9fb: same for record 2. */
        {
            vec3_t right;
            AngleVectors(o2->angles, o2->forward, right, o2->up);
            o2->negRight[0] = (float)(0.0L - (long double)right[0]);
            o2->negRight[1] = (float)(0.0L - (long double)right[1]);
            o2->negRight[2] = (float)(0.0L - (long double)right[2]);
        }
    }

    /* 3001fa3c: project the predicted playerState into the local-player entityState
     * template. Register ABI: es=ESI=&cg_predictedEventEntity.nextState,
     * ps=EDI=cg.predictedPlayerState (0x304831c4), snap=EAX=0. The template lives at
     * 0x304877bc, the source of the MOVSD.REP copy just below. */
    BG_PlayerStateToEntityState(&cg_predictedPlayerState, &cg_predictedEventEntity.nextState, qfalse);

    /* 3001fa41: MOVSD.REP of 0x3d (=61) dwords from the template (0x304877bc) into
     * cg_predictedEventEntity (0x304876c8). 61 dwords == 244 bytes == one
     * entityState_t / the leading currentState of the centity. */
    memcpy(&cg_predictedEventEntity, &cg_predictedEventEntity.nextState, sizeof(cg_predictedEventEntity.nextState));

    /* 3001fa52..3001fad6: this whole stage runs ONLY when the local player has a
     * live first-person DObj (ps.playerStateFlags & 0xc0000). The JZ at 0x3001fa5e
     * jumps past the entire block (including the predicted-entity add below) to the
     * second entity loop at 0x3001fadb. cg_nextSnap->ps.playerStateFlags is
     * ps.playerStateFlags at snapshot+0x18; clientNum is at +0xe0. */
    if ((cg_nextSnap->ps.playerStateFlags & PSF_PLAYER_ENTITY_MASK) != 0) {
        /* 3001fa60: dObjHandle = trap(0xa5, cg_nextSnap->ps.psClientNum). */
        intptr_t dObjHandle = cgame_syscall(CG_DOBJ_GET_HANDLE, cg_nextSnap->ps.psClientNum);
        /* 3001fa77: JZ 0x3001fab6 — a zero handle skips only the anim-advance and
         * falls into the predicted-entity add. */
        if (dObjHandle != 0) {
            /* 3001fa79: advance by cg_frametime seconds (same idiom as loop 1). */
            float frameMilliseconds = (float)cg_frametime;
            float dtSeconds = (float)((long double)frameMilliseconds * (long double)0.001f);
            cgame_syscall(CG_DOBJ_ADVANCE_SERVER_TIME, dObjHandle, CG_FloatBits(dtSeconds));
            /* 3001faa2: CG_SetGunHandFromNotetracks(cg_nextSnap->ps.psClientNum). */
            CG_SetGunHandFromNotetracks(cg_nextSnap->ps.psClientNum);
        }

        /* 3001fab6: add the predicted-event centity to the scene when its eType
         * is below ET_EVENTS, the ordinary CG_AddCEntity switch domain. */
        {
            centity_t *predicted = &cg_predictedEventEntity;
            if (predicted->currentState.eType < ET_EVENTS) {
                /* 3001fabf: CG_CalcEntityLerpPositions(predicted) (stack arg). */
                CG_CalcEntityLerpPositions(predicted);
                /* 3001facc: CG_AddCEntity(predicted) (EAX=cent). */
                CG_AddCEntity(predicted);
            }
        }
    }

    /* Second pass: add every snapshot entity to the scene.
     * 3001fadb: reload EAX=cg_nextSnap, ECX=numEntities. EBX = i, EDI = i*0xf4. */
    for (i = 0; i < cg_nextSnap->numEntities; i++) {
        /* 3001faf0: cent = &cg_entities[ cg_nextSnap->entities[i].number ]
         * (stride 0x288, base 0x3048c6e0 — the established cg_entities[] view). */
        centity_t *cent = cg_entities + cg_nextSnap->entities[i].number;

        /* 3001fb03: only ordinary entity types below ET_EVENTS are added. */
        if (cent->currentState.eType < ET_EVENTS) {
            /* 3001fb09: CG_CalcEntityLerpPositions(cent) (stack arg). */
            CG_CalcEntityLerpPositions(cent);
            /* 3001fb14: CG_AddCEntity(cent) (EAX=cent). */
            CG_AddCEntity(cent);
        }
    }

    /* 3001fb2f: finalize the special view/local DObj. Requires the index in
     * [0, MAX_GENTITIES) and the draw-inhibit gate clear; then fetch its handle and
     * commit it if valid. The signed compares JL/JGE prove a signed index range. */
    if (cg_dumpAnims_vmCvar.integer >= 0 && cg_dumpAnims_vmCvar.integer < MAX_GENTITIES && cl_paused_vmCvar.integer == 0) {
        /* 3001fb47: handle = trap(0xa5, cg_dumpAnims_vmCvar.integer). */
        intptr_t dObjHandle = cgame_syscall(CG_DOBJ_GET_HANDLE, cg_dumpAnims_vmCvar.integer);
        if (dObjHandle != 0) {
            /* 3001fb5a: trap(0x9b, handle) — display the selected DObj's
             * animation diagnostics. */
            cgame_syscall(CG_DOBJ_DISPLAY_ANIM, dObjHandle);
        }
    }
}
