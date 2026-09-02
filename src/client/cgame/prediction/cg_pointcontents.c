// Source: uo_cgame_mp_x86.dll 0x30035420..0x300354a8
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30035420_300354a8.mcode

#include "client/cgame/client_recovered.h"

#include <stddef.h>

/* Layout guards for the collision-entity fields this function reads. Proven at the
 * i386 target width against the machine code (CMP [ESI], MOV [ESI+0x8c],
 * CMP [ESI+0xa0], LEA [ESI+0x214], ADD ESI,0x208). */
_Static_assert(offsetof(centity_t, currentState.number) == 0x00,
               "centity_t.entityNum at +0x00");
_Static_assert(offsetof(centity_t, currentState.itemIndex) == 0x8c,
               "centity_t.itemIndex at +0x8c");
_Static_assert(offsetof(centity_t, currentState.solid) == 0xa0,
               "centity_t.solid at +0xa0");
_Static_assert(offsetof(centity_t, lerpOrigin) == 0x208,
               "centity_t.lerpOrigin at +0x208");
_Static_assert(offsetof(centity_t, lerpAngles) == 0x214,
               "centity_t.lerpAngles at +0x214");

/*
 * CG_PointContents (0x30035420)
 *
 * Name: the .mcode-assigned "Q_CleanStr" is REJECTED. Q_CleanStr is a pure
 * ASCII/color-code string filter with no system calls and no global array walk;
 * this function issues three cgame system calls (CG_CM_POINT_CONTENTS/33/37 through the trap
 * pointer at 0x30085e9c) and iterates the client solid-entity list, so the size-based
 * guess is provably wrong. The replacement name is confirmed by the Mac symbol bank.
 * It shares the collision array
 * (cg_solidEntities / cg_numSolidEntities) and the centity_t struct with
 * CG_ClipMoveToEntities (0x300350d0), which walks the identical fields.
 *
 * ABI: three cdecl stack dwords; ends in a plain RET (caller-cleaned). Returns EAX.
 * EBX/EBP/ESI/EDI are callee-saved and restored.
 *   [ESP+0x04] arg0 = point      (forwarded to CG_CM_POINT_CONTENTS and CG_CM_TRANSFORMED_POINT_CONTENTS)
 *   [ESP+0x08] arg1 = passEntityNum (compared against each cent->currentState.number)
 *   [ESP+0x0c] arg2 = mask       (ANDed into the accumulated result at the end)
 *
 * Machine-code facts proven for every behavior-affecting statement:
 *   30035422  EBP = point  (MOV EBP,[ESP+0xc] before any push; entry arg0)
 *   30035427..3003542a  pushes (program order): 0, EBP(point), 0x24
 *   3003542c  CALL [cgame_syscall]; ADD ESP,0xc  => acc = cgame_syscall(0x24, point, 0)
 *   30035432  EBX = acc  (the OR-accumulator seed)
 *   30035434  EAX = cg_numSolidEntities ([0x300dc08c])
 *   3003543c  EDI = 0  (loop index i)
 *   30035440  JLE past the loop when count <= 0  (signed: TEST EAX,EAX / JLE)
 *   30035443  ESI = cg_solidEntities[i]  (MOV ESI,[EDI*4 + 0x300dc090])
 *   3003544a  EAX = excludeId  ([ESP+0x18] resolves to entry arg1 after PUSH ESI)
 *   3003544e  CMP [ESI],EAX ; JZ skip      => skip passEntityNum
 *   30035452  CMP [ESI+0xa0],0xffffff ; JNZ skip => require SOLID_BMODEL
 *   3003545e  EAX = cent->currentState.itemIndex ([ESI+0x8c])
 *   30035464..30035465  pushes: EAX(itemIndex), 0x21
 *   30035467  CALL [cgame_syscall]; ADD ESP,8 => inline collision model
 *   30035470  TEST EAX,EAX ; JZ skip        => skip when the register returned 0
 *   30035474  ECX = &cent->lerpAngles   (LEA [ESI+0x214])
 *   3003547b  ESI = &cent->lerpOrigin (ADD ESI,0x208)
 *   3003547a..30035484  pushes (program order): ECX(&angles), ESI(&origin), EAX(h),
 *               EBP(point), 0x25
 *   30035486  CALL [cgame_syscall]; ADD ESP,0x14
 *               => transformed point-contents query against the inline model
 *   3003548f  OR EBX,EAX   => acc |= r
 *   30035491  EAX = cg_numSolidEntities ; INC EDI ; CMP EDI,EAX ; JL loop  (signed)
 *   3003549c  ECX = mask ([ESP+0x18] -> entry arg2 after ESI popped)
 *   300354a4  EAX = acc & mask
 *   300354a7  RET
 */
int32_t CG_PointContents(const vec3_t point, int32_t passEntityNum,
                         int32_t contentMask)
{
    /* World contents seed the accumulator before inline-model contributions. */
    int32_t acc = (int32_t)cgame_syscall(CG_CM_POINT_CONTENTS,
                                (intptr_t)point, 0);

    /* Signed loop bound: JLE / JL against cg_numSolidEntities. */
    for (int32_t i = 0; i < cg_numSolidEntities; i++) {
        centity_t *cent = cg_solidEntities[i];

        if (cent->currentState.number == passEntityNum) {
            continue;
        }
        if (cent->currentState.solid != SOLID_BMODEL) {
            continue;
        }

        int32_t inlineModel = (int32_t)cgame_syscall(CG_CM_INLINE_MODEL, cent->currentState.itemIndex);
        if (inlineModel == 0) {
            continue;
        }

        acc |= (int32_t)cgame_syscall(CG_CM_TRANSFORMED_POINT_CONTENTS,
                             (intptr_t)point, inlineModel,
                             (intptr_t)cent->lerpOrigin,
                             (intptr_t)cent->lerpAngles);
    }

    return acc & contentMask;
}
