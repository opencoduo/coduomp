// Source: uo_cgame_mp_x86.dll 0x30025cd0..0x30025d94
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30025cd0_30025d94.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stddef.h>
#include <string.h>

/* Layout guards for the fields this function individually touches on the local
 * body-trace result and on the caller's world-trace result: fraction (+0x00),
 * entityNum (+0x28), allsolid (+0x2e), and the 48-byte total the final REP MOVSD
 * (ECX=12 dwords) copies out. Proven at the i386 target width. */
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(offsetof(trace_t, fraction) == 0x00, "trace_t.fraction at +0x00");
_Static_assert(offsetof(trace_t, entityNum) == 0x28, "trace_t.entityNum at +0x28");
_Static_assert(offsetof(trace_t, allsolid) == 0x2e, "trace_t.allsolid at +0x2e");
_Static_assert(sizeof(trace_t) == 0x30, "trace_t is 48 bytes (12 dwords copied by REP MOVSD)");
#endif

/*
 * CG_FlamethrowerTrace (0x30025cd0) — see the header comment on the declaration in
 * client_recovered.h. A flame-cluster trace helper: run the world trace, then (for
 * a non-local-player target) refine it against the local player's own body.
 *
 * Machine-code facts proven for every behavior-affecting statement:
 *
 *   Prologue / incoming ABI (register args EAX/ECX/EDX, four cdecl stack dwords):
 *     30025cd5  EBP = [entry+0x0c]  = origin        (MOV EBP,[ESP+0x44])
 *     30025cdb  ESI = incoming EAX  = contentMask/handle
 *     30025cdd  EAX = [entry+0x10]  = entityNum     (MOV EAX,[ESP+0x50])
 *     30025ce2  EDI = incoming EDX  = arg2
 *     30025ce4  EDX = [entry+0x04]  = out           (MOV EDX,[ESP+0x48])
 *     30025ce8  EBX = incoming ECX  = flags
 *     30025cea  ECX = [entry+0x08]  = arg1          (MOV ECX,[ESP+0x4c])
 *
 *   30025ce1..cf0  pushes (program order): EAX(entityNum), EDI(arg2), ECX(arg1),
 *                  EDX(out); then EAX=ESI(handle), ECX=EBP(origin).
 *   30025cf5  CALL CG_Trace(handle=ESI, origin=EBP, flags=EBX,
 *                  out=EDX, arg1=ECX, arg2=EDI, arg3=EAX)   -> C-arg order after
 *                  the reversed pushes is (out, arg1, arg2, arg3).
 *   30025cfa  EAX = cg_snap  (MOV EAX,[0x30459160])
 *   30025cff  ECX = [entry+0x10] = entityNum        (MOV ECX,[ESP+0x60])
 *   30025d03  EDX = cg_snap->ps.psClientNum              (MOV EDX,[EAX+0xe0])
 *   30025d0c  CMP ECX,EDX ; 30025d0e JZ epilogue     -> if entityNum == clientNum,
 *                  keep the world result and return (skip the whole body refine).
 *
 *   Body-setup trap CG_CM_TEMP_CAPSULE_MODEL (0x2a):
 *     30025d10  ECX = &cg_snap->ps.playerMaxs (cg_snap+0x574)
 *     30025d16  PUSH 0x2000000 (CONTENTS_BODY)
 *     30025d1b  PUSH ECX (&psFlameTraceB)
 *     30025d1c  EAX = cg_snap+0x568 = &cg_snap->ps.playerMins
 *     30025d21  PUSH EAX (&psFlameTraceA)
 *     30025d22  PUSH 0x2a
 *     30025d24  CALL [cgame_syscall]  ->
 *                  handle42 = cgame_syscall(CG_CM_TEMP_CAPSULE_MODEL,
 *                                           &psFlameTraceA, &psFlameTraceB,
 *                                           CONTENTS_BODY)
 *                  (EAX holds handle42; no stack cleanup here — folded into the
 *                   ADD ESP,0x38 that follows the CG_CM_TRANSFORMED_CAPSULE_TRACE call)
 *
 *   Body trace CG_CM_TRANSFORMED_CAPSULE_TRACE (0x29) into a local trace_t at [ESP+0x10]:
 *     30025d2a  EDX = cg_snap ; 30025d35 EDX = cg_snap+0x20 = &cg_snap->ps.psOrigin
 *     30025d30  PUSH 0x30071f58 = &vec3_origin {0,0,0}
 *     30025d38  PUSH EDX (&localSoundObj)
 *     30025d39  PUSH ESI (handle)
 *     30025d3a  PUSH EAX (handle42, the CG_CM_TEMP_CAPSULE_MODEL return)
 *     30025d3b  EAX = [entry+0x08] = start             (MOV EAX,[ESP+0x68])
 *     30025d3f  PUSH EBX (maxs)
 *     30025d40  PUSH EDI (mins)
 *     30025d41  PUSH EBP (end)
 *     30025d42  PUSH EAX (start)
 *     30025d43  ECX = &result (LEA [ESP+0x40], the 0x30-byte local)
 *     30025d47  PUSH ECX (&result)
 *     30025d48  PUSH 0x29
 *     30025d4a  CALL [cgame_syscall]  ->  C-arg order after reversed pushes:
 *                  cgame_syscall(CG_CM_TRANSFORMED_CAPSULE_TRACE, &result,
 *                                start, end, mins, maxs, handle42, contentMask,
 *                                &localSoundObj, &vec3_origin)
 *     30025d50  AL = result.allsolid  (MOV AL,[ESP+0x76] -> &result + 0x2e)
 *     30025d54  ADD ESP,0x38  (cdecl cleanup of both syscalls' pushed dwords)
 *     30025d57  TEST AL,AL ; 30025d59 JNZ copy       -> hit != 0 => keep body result
 *
 *   Nearer-fraction test (only when hit == 0):
 *     30025d5b  FLD  float [ESP+0x10] = result.fraction     (ST0 = body fraction)
 *     30025d5f  EDX = [entry+0x04] = out
 *     30025d63  FCOMP float [EDX] = out->fraction           (ST0 - out->fraction)
 *     30025d65  FNSTSW AX ; 30025d67 TEST AH,0x5 ; 30025d6a JP epilogue
 *                  PF is set (JP taken -> discard body result) when body.fraction
 *                  >= out->fraction or unordered; JP falls through (keep body
 *                  result) only when body.fraction < out->fraction.
 *
 *   Copy-out (30025d6c): stamp entityNum with the local client number, then copy the
 *   whole 48-byte body result over *out:
 *     30025d6c  EAX = cg_snap
 *     30025d71  CX  = (uint16_t)cg_snap->ps.psClientNum   (MOV CX, word [EAX+0xe0])
 *     30025d78  EDI = [entry+0x04] = out
 *     30025d7c  result.entityNum = CX (MOV word [ESP+0x38] -> &result+0x28)
 *     30025d81  ECX = 12 ; 30025d86 ESI = &result ; 30025d8a REP MOVSD (DF=0)
 *                  copies 12 dwords = 48 bytes from result to *out.
 *
 *   30025d8c  epilogue: POP EDI/ESI/EBP/EBX ; ADD ESP,0x30 ; RET (caller-cleaned
 *             stack args).
 */
void CG_FlamethrowerTrace(int32_t contentMask, const vec3_t maxs, const vec3_t mins, trace_t *out, const vec3_t start, const vec3_t end,
                          int32_t entityNum)
{
    trace_t result;
    int32_t handle42;

    /* World trace/projection into the caller's out buffer. */
    CG_Trace(contentMask, end, maxs, out, start, mins, entityNum);

    /* Refine against the local player's own body, unless the traced entity IS the
     * local player. */
    if (entityNum == cg_snap->ps.psClientNum) {
        return;
    }

    handle42 = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_CM_TEMP_CAPSULE_MODEL, (intptr_t)cg_snap->ps.playerMins,
                                                             (intptr_t)cg_snap->ps.playerMaxs, (int32_t)CONTENTS_BODY));

    cgame_syscall(CG_CM_TRANSFORMED_CAPSULE_TRACE, (intptr_t)&result, (intptr_t)start, (intptr_t)end, (intptr_t)mins, (intptr_t)maxs,
                  handle42, contentMask, (intptr_t)&cg_snap->ps.psOrigin, (intptr_t)&vec3_origin);

    /* Keep the body result when it actually hit, or when it came back nearer than
     * the world trace. Otherwise leave the world result in *out. */
    if (result.allsolid == 0 && !(result.fraction < out->fraction)) {
        return;
    }

    result.entityNum = (uint16_t)cg_snap->ps.psClientNum;
    memcpy(out, &result, sizeof(result)); /* REP MOVSD, 12 dwords = 48 bytes */
}
