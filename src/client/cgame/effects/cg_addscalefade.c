// Source: uo_cgame_mp_x86.dll 0x3002ac20..0x3002acf2
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002ac20_3002acf2.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "compat/coduo_native_x87.h"

/*
 * CG_AddScaleFade — the leType == LE_SCALE_FADE (case 1) local-entity render
 * handler dispatched by CG_AddLocalEntities (0x3002ad00). Register ABI: the
 * localEntity_t * arrives in ESI (the dispatch loop holds the current node in
 * ESI), exactly like its siblings CG_AddFadeRGB (case 0, 0x3002abc0) and
 * CG_AddMovingTracer (case 2, 0x3002ab00). No stack args, no return value.
 *
 * NAME: the .mcode size-guess name "PM_InteruptWeaponWithProneMove" is REJECTED.
 * It was matched only by byte size (win 0xd2 ~ corpus 0xd4), which the naming
 * rules forbid, and it is contradicted by the machine code: this function does no
 * pmove / weapon / prone state work. It reads a localEntity_t fade trajectory,
 * packs a faded alpha into the embedded refEntity, computes a growing radius, and
 * distance-culls or renders the sprite. The adopted name CG_AddScaleFade comes
 * from the same-module PPC bank (cgame_mp!CG_AddScaleFade), which lists the three
 * local-entity handlers in source order CG_AddScaleFade / CG_AddFadeRGB /
 * CG_AddMovingTracer — the same trio the Windows client dispatches from
 * CG_AddLocalEntities (this is the one still unnamed), corroborated field-for-field
 * by the behavior below (life-fraction fade of the alpha channel, an expanding
 * radius, and a view-distance cull), the canonical Quake3 CG_AddScaleFade.
 *
 * Fade/round math is identical in shape to the sibling CG_AddFadeRGB (0x3002abc0):
 *   P = (float)(le->endTime - cg_time) * le->lifeRate              (0x3002ac2f-0x3002ac36)
 * Only the ALPHA channel is packed here (CG_AddFadeRGB packs all four):
 *   le->refEntity.shaderRGBA[3] = (uint8_t)_ftol2(P * le->color[3] * 255.0f)
 *   (FLD ST0 copy of P kept in ST1; the alpha branch multiplies the copy)
 *   (0x3002ac39-0x3002ac49; the surviving P is popped at 0x3002ac66).
 *
 * Instruction-level proof of every statement:
 *   3002ac21  ECX = cg_time                         [uint32 global 0x304831b0]
 *   3002ac27  EAX = le->endTime          (int32 @ +0x10)
 *   3002ac2a  EAX = EAX - ECX            (endTime - cg_time), 32-bit wrap
 *   3002ac2f  FILD [ESP]                 -> P0 = (float) of that signed int32
 *   3002ac33  EDI = &le->refEntity       (LEA ESI+0x50; live through the tail)
 *   3002ac36  FMUL le->lifeRate          (float @ +0x14)  => P (ST0)
 *   3002ac39  FLD ST0                     ST0=P, ST1=P (keep a copy)
 *   3002ac3b  FMUL le->color[3]          (float @ +0x48, alpha multiplier)
 *   3002ac3e  FMUL 255.0f                (float const 0x3007bd64)
 *   3002ac44  CALL _ftol2 (0x3006be3c)   truncate ST0 -> EAX; pops its ST0 arg (ST1=P survives)
 *   3002ac49  MOV [EDI+0x6f],AL          refEntity.shaderRGBA[3] = low byte
 *
 *   3002ac4c  TEST le->leFlags,0x1       (LEF_SCALE_FADE_NO_RADIUS)
 *   3002ac50  JNZ 0x3002ac66             if bit set, skip the radius write
 *     3002ac52  FLD 1.0f                 (const 0x3007bce0)  ST0=1.0, ST1=P
 *     3002ac58  FSUB ST0,ST1             ST0 = 1.0 - P
 *     3002ac5a  FMUL le->radius          (float @ +0x4c)
 *     3002ac5d  FADD 8.0f                (const 0x3007be08)
 *     3002ac63  FSTP [EDI+0x7c]          refEntity.radius = (1 - P) * le->radius + 8
 *   3002ac66  FSTP ST0                   pop the surviving P (x87 stack now empty)
 *
 *   -- view-distance of the sprite from the camera --
 *   3002ac68  FLD refEntity.origin[0] (EDI+0x44)
 *   3002ac6b  FSUB cg_refdef.vieworg[0] (0x30487a90)     => dx
 *   3002ac71  FLD refEntity.origin[1] (EDI+0x48)
 *   3002ac74  FSUB cg_refdef.vieworg[1] (0x30487a94)     => dy
 *   3002ac7a  FLD refEntity.origin[2] (EDI+0x4c)
 *   3002ac7d  FSUB cg_refdef.vieworg[2] (0x30487a98)     => dz
 *   3002ac83-93  dist = sqrt(dx*dx + dy*dy + dz*dz)     (FLD/FMUL/FADDP x2, FSQRT)
 *   3002ac95-99  FSTP ST3/FSTP ST0/FSTP ST0            drop the dx/dy/dz temporaries
 *   3002ac9b  FCOMP le->radius (ESI+0x4c) ; FNSTSW AX ; TEST AH,5 ; JP 0x3002ace3
 *             After FCOMP, TEST AH,5 masks C0|C2; JP (parity) is taken iff
 *             dist >= le->radius (or unordered): dist < radius sets C0 alone
 *             (one masked bit => PF=0 => fall through); dist >= radius clears both
 *             (PF=1 => jump). So render when dist >= radius, free when dist < radius.
 *
 *   -- dist < radius: sprite has shrunk below its threshold -> free it --
 *   -- (inlined CG_FreeLocalEntity(le), identical to CG_AddLocalEntities' path) --
 *   3002aca5  CMP le->prev,0 ; JNZ           if le->prev == 0 report list-integrity error
 *   3002acaa  PUSH "CG_FreeLocalEntity: not active" ; CALL Com_ErrorMessage ; ADD ESP,4
 *   3002acb7  EAX = cg_numLocalEntities   [0x30134cfc]
 *   3002acbc  EDX = le->next ; ECX = le->prev
 *   3002acc1  le->prev->next = le->next
 *   3002acc4  ECX = le->prev ; EDX = cg_freeLocalEntities [0x30537d80]
 *   3002accc  DEC EAX ; cg_numLocalEntities = EAX
 *   3002acd2  EAX = le->next ; le->next->prev = le->prev
 *   3002acd7  le->next = cg_freeLocalEntities ; cg_freeLocalEntities = le
 *   3002ace2  RET
 *
 *   -- dist >= radius: still visible -> submit to the render scene --
 *   3002ace3  PUSH EDI(&refEntity) ; PUSH 0x3d ; CALL [cgame_syscall] ; ADD ESP,8
 *             => trap_R_AddRefEntityToScene(&le->refEntity)
 *   3002acf1  RET
 */
void CG_AddScaleFade(localEntity_t *le)
{
    refEntity_t *re = &le->refEntity;

    /* Remaining-life fraction P scaled by lifeRate (a 1/lifespan rate).
     * 0x3002ac2f..0x3002ac39: FILD; FMUL lifeRate; FLD ST0 — the product is
     * never stored, it stays in an x87 register for BOTH consumers (the alpha
     * _ftol2 chain and the FSUB ST0,ST1 at 0x3002ac58), so it is long double.
     * The ms delta carries no (float) cast: the DLL FILDs the integer straight
     * into the FMUL with no FSTP DWORD, so it enters exact. Under -std=c11 an
     * explicit (float) really rounds (fildl/fstps/flds) and would diverge once
     * |endTime - cg_time| exceeds 2^24 ms (~4.66 h of uptime). */
    int32_t remaining = coduo_int32_from_bits((uint32_t)le->endTime - (uint32_t)cg_time);
    long double phase = (long double)remaining * (long double)le->lifeRate;

    /* Only the alpha channel is packed from the fade (0..255, wrapped by the byte store).
     * The 255.0f multiplier is the shared .rdata float at 0x3007bd64. */
    re->shaderRGBA[3] = (uint8_t)coduo_fp_to_i32_extended(phase * le->color[3] * colorByteScale);

    /* Grow the sprite radius from 8 toward le->radius + 8, unless the no-radius flag is set.
     * FLD 1.0f (const 0x3007bce0) and FADD 8.0f (const 0x3007be08) — both are shared
     * .rdata float constants, written here in natural form. */
    if ((le->leFlags & LEF_SCALE_FADE_NO_RADIUS) == 0) {
        re->radius = (1.0f - phase) * le->radius + 8.0f;
    }

    /* Distance from the camera (cg_refdef.vieworg) to the sprite origin.
     * 0x3002ac68..0x3002ac9b: the whole chain — the three subtractions, the
     * squares, the FADDPs (summed z,y,x), the FSQRT and the FCOMP against
     * le->radius — stays in 80-bit st registers with NO float store, so the
     * locals are long double (BoxOnPlaneSide precedent); float locals would
     * round before the cull compare. */
    long double dx = (long double)re->origin[0] - (long double)cg_refdef.vieworg[0];
    long double dy = (long double)re->origin[1] - (long double)cg_refdef.vieworg[1];
    long double dz = (long double)re->origin[2] - (long double)cg_refdef.vieworg[2];
    long double dist = coduo_x87_sqrtl(dz * dz + dy * dy + dx * dx);

    if (!(dist < (long double)le->radius)) {
        /* Camera is outside the sprite radius: submit it to the scene. */
        trap_R_AddRefEntityToScene(re);
        return;
    }

    /*
     * dist < le->radius: retire the sprite. Inlined CG_FreeLocalEntity(le) —
     * unlink from the active list and push onto the free list. The list-integrity
     * assert fires when le->prev is NULL (the entity is not actually linked).
     */
    if (le->prev == 0) {
        Com_ErrorMessage(cg_freeLocalEntityInactiveErrorMessage);
    }
    {
        uint32_t countBits = (uint32_t)cg_numLocalEntities; /* 0x3002acb7 */
        localEntity_t *linkNext = le->next;                 /* 0x3002acbc */
        localEntity_t *linkPrev = le->prev;                 /* 0x3002acbf */
        localEntity_t *freeHead;

        linkPrev->next = linkNext;                          /* 0x3002acc1 */
        linkPrev = le->prev;                                /* 0x3002acc4 */
        freeHead = cg_freeLocalEntities;                     /* 0x3002acc6 */
        cg_numLocalEntities = coduo_int32_from_bits(countBits - 1u); /* accc..cd */
        linkNext = le->next;                                /* 0x3002acd2 */
        linkNext->prev = linkPrev;                          /* 0x3002acd5 */
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        le->prev = NULL;
        le->next = freeHead;
    }
    cg_freeLocalEntities = le;
}
