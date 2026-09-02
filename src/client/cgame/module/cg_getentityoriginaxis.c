// Source: uo_cgame_mp_x86.dll 0x3002adb0..0x3002ae70
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002adb0_3002ae70.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_GetEntityOriginAxis (provisional-by-role) — a cgame script/VM builtin
 * (sibling of CG_GetEffectOriginAxis at 0x3002ae70; both dispatched from the
 * builtin table in 0x3002af00..0x3002b148, this one from the arm at 0x3002b06c
 * which loads ECX=[EBP+0xc], EDX=[EBP+0x10], EAX=[EBP+0x14]). Given an entity/tag
 * index it fills an outOrigin vec3 and an outAxis 3x3 basis:
 *
 *   - index in [0, MAX_GENTITIES)  -> a real client entity, cg_entities[index]
 *     (the stride-0x288 array at 0x3048c6e0): outOrigin = entity.lerpOrigin
 *     (+0x208), and the placement axis is built from entity.lerpAngles
 *     (+0x214) via AnglesToAxisNegRight (tail-called at 0x3004c200, which writes
 *     outAxis in EAX).
 *   - index in [MAX_GENTITIES, MAX_GENTITIES+0x80) -> one fixed pseudo-entity
 *     placement, cg_specialTagPlacement (orientation_t @ 0x3048b0e4): outOrigin =
 *     its origin, outAxis = its axis (copied directly, no AnglesToAxis). Note the
 *     sub-index (0..0x7f) is only range-checked, not used to index — all 128
 *     values return the same placement.
 *   - index < 0 or index >= MAX_GENTITIES+0x80 -> no output (early return).
 *
 * The .mcode size-guess name "PM_Weapon_FinishFiring" is rejected: this does no
 * weapon-firing / predicted-movement work; it is an entity/tag origin+axis fetch.
 *
 * Register-argument ABI (custom regparm), from the caller at 0x3002b06c:
 *   index in ECX, outOrigin in EDX, outAxis in EAX. Modeled as ordered parameters;
 *   no calling-convention attribute is added (syntax-only build does not need one).
 *
 * Machine-code facts proven for every branch/statement:
 *   3002adb0  TEST ECX,ECX ; JL 0x3002adef      index < 0 -> negative arm (returns)
 *   3002adb4  CMP ECX,0x400 ; JGE 0x3002adf7     index >= MAX_GENTITIES -> special arm
 *   3002adbc  IMUL ECX,ECX,0x288 ; ADD ECX,0x3048c6e0   ECX = &cg_entities[index]
 *   3002adc8  PUSH ESI (callee-saved scratch)
 *   3002adc9  MOV ESI,[ECX+0x208] ; MOV [EDX],ESI        outOrigin[0] = entity.lerpOrigin[0]
 *   3002add1  MOV ESI,[ECX+0x20c] ; MOV [EDX+0x4],ESI     outOrigin[1] = .lerpOrigin[1]
 *   3002adda  MOV ESI,[ECX+0x210] ; MOV [EDX+0x8],ESI     outOrigin[2] = .lerpOrigin[2]
 *   3002ade3  LEA EDX,[ECX+0x214]                          EDX = &entity.lerpAngles
 *   3002ade9  POP ESI
 *   3002adea  JMP 0x3004c200  tail-call AnglesToAxisNegRight(outAxis=EAX,
 *                                          angles=EDX=&lerpAngles)
 *   3002adef  CMP ECX,0x400 ; JL 0x3002ae6f   (negative index: always < 0x400 -> RET)
 *   3002adf7  ADD ECX,0xfffffc00              ECX = index - MAX_GENTITIES
 *   3002adfd  CMP ECX,0x80 ; JGE 0x3002ae6f    sub-index >= 0x80 -> RET (no output)
 *   3002ae05  MOV ECX,[0x3048b0e4] ; MOV [EDX],ECX         outOrigin[0] = special.origin[0]
 *   3002ae0d  ...[0x3048b0e8]->[EDX+0x4], [0x3048b0ec]->[EDX+0x8]   outOrigin[1..2]
 *   3002ae1f  MOV EDX,[0x3048b0f0] ; MOV [EAX],EDX          outAxis[0][0] = special.axis[0][0]
 *   3002ae27  [0x3048b0f4]->[EAX+0x4], [0x3048b0f8]->[EAX+0x8]      outAxis[0][1..2]
 *   3002ae39  [0x3048b0fc]->[EAX+0xc],[0x3048b100]->[EAX+0x10],[0x3048b104]->[EAX+0x14]  outAxis[1]
 *   3002ae54  [0x3048b108]->[EAX+0x18],[0x3048b10c]->[EAX+0x1c],[0x3048b110]->[EAX+0x20] outAxis[2]
 *   3002ae6f  RET
 *
 * All copies are 32-bit MOVs (plain dword copies), so the fields carry through as
 * their float bit patterns; represented as float vector assignments.
 */
void CG_GetEntityOriginAxis(int32_t index, vec3_t outOrigin, axis_t outAxis)
{
    if (index < 0) {
        /* 0x3002adef: negative index falls to the always-taken JL and returns. */
        return;
    }

    if (index < MAX_GENTITIES) {
        /* 0x3002adbc: entity = &cg_entities[index] (stride 0x288). */
        centity_t *entity = cg_entities + index;

        outOrigin[0] = entity->lerpOrigin[0];
        outOrigin[1] = entity->lerpOrigin[1];
        outOrigin[2] = entity->lerpOrigin[2];

        /* 0x3002adea: tail-call — derive the placement axis from the entity's
         * lerpAngles (+0x214). AnglesToAxisNegRight writes outAxis. */
        AnglesToAxisNegRight(outAxis, entity->lerpAngles);
        return;
    }

    /* 0x3002adf7: special pseudo-entity band [MAX_GENTITIES, MAX_GENTITIES+0x80). */
    if ((uint32_t)(index - MAX_GENTITIES) >= 0x80u) {
        return;
    }

    /* 0x3002ae05: copy the fixed placement directly; no AnglesToAxis. */
    outOrigin[0] = cg_specialTagPlacement.origin[0];
    outOrigin[1] = cg_specialTagPlacement.origin[1];
    outOrigin[2] = cg_specialTagPlacement.origin[2];

    outAxis[0][0] = cg_specialTagPlacement.axis[0][0];
    outAxis[0][1] = cg_specialTagPlacement.axis[0][1];
    outAxis[0][2] = cg_specialTagPlacement.axis[0][2];

    outAxis[1][0] = cg_specialTagPlacement.axis[1][0];
    outAxis[1][1] = cg_specialTagPlacement.axis[1][1];
    outAxis[1][2] = cg_specialTagPlacement.axis[1][2];

    outAxis[2][0] = cg_specialTagPlacement.axis[2][0];
    outAxis[2][1] = cg_specialTagPlacement.axis[2][1];
    outAxis[2][2] = cg_specialTagPlacement.axis[2][2];
}
