// Source: uo_cgame_mp_x86.dll 0x30035390..0x30035417
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30035390_30035417.mcode

#include "client/cgame/client_recovered.h"

#include <stddef.h>

/* Layout guards for the trap-filled result: the two individually-accessed fields
 * (fraction at +0x00, entityNum at +0x28) and the 48-byte total that the final REP
 * MOVSD (ECX=12 dwords) copies out. Proven at the i386 target width. */
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(offsetof(trace_t, fraction) == 0x00, "trace_t.fraction at +0x00");
_Static_assert(offsetof(trace_t, entityNum) == 0x28, "trace_t.entityNum at +0x28");
_Static_assert(sizeof(trace_t) == 0x30, "trace_t is 48 bytes (12 dwords copied by REP MOVSD)");
#endif

/*
 * CG_TraceCapsule (0x30035390) — the capsule-trace twin of CG_Trace
 * (0x30035310). It is used as a CALLBACK: the enumerator at 0x300354b0 pushes this
 * function's address as a function pointer to the iterator 0x3000c8e0, so every
 * argument arrives on the stack (there is no register-argument ABI here). It runs
 * the collision trap CG_CM_CAPSULE_TRACE (0x28) into a local 48-byte
 * trace_t, tags the result by whether the trace reached the far end
 * (fraction == 1.0f), refines it against solid centities with
 * CG_ClipMoveToEntities, then copies the whole 48-byte struct out to the caller.
 *
 * Name: the .mcode-assigned "Scr_Vehicle_Use" is REJECTED — that is a server Scr_*
 * symbol, while this function is pure client cgame code (it calls through
 * cgame_syscall at 0x30085e9c and the client collision helper CG_ClipMoveToEntities at
 * 0x300350d0). Provisional-by-role name; exact source symbol unconfirmed.
 *
 * Divergence from CG_Trace (both proven from the machine code):
 *   - it issues trap CG_CM_CAPSULE_TRACE (0x28) rather than CG_CM_BOX_TRACE (0x26);
 *   - it passes 1 (not 0) for CG_ClipMoveToEntities's next-to-last argument.
 *
 * Stack argument ABI (all cdecl, plain RET => caller-cleaned; entry stack slots are
 * numbered from the return address at [ESP0+0]):
 *   [ESP0+0x04] arg0 = out    (trace_t *, destination of the copy)
 *   [ESP0+0x08] arg1 = int    (forwarded to both calls)
 *   [ESP0+0x0c] arg2 = int    (forwarded to both calls)
 *   [ESP0+0x10] arg3 = flags  (int; matches CG_Trace's `flags` slot)
 *   [ESP0+0x14] arg4 = origin (vec3_t *; matches CG_Trace's `origin` slot)
 *   [ESP0+0x18] arg5 = int    (only forwarded to CG_ClipMoveToEntities)
 *   [ESP0+0x1c] arg6 = handle (int; matches CG_Trace's `handle` slot)
 *   [ESP0+0x20] arg7 = unused
 * EBX/EBP/ESI/EDI are callee-saved and restored.
 *
 * Machine-code facts proven for every behavior-affecting statement:
 *   30035393  EAX = arg1   ([ESP+0x38] -> ESP0+0x08)
 *   30035398  EBX = arg2   ([ESP+0x40] -> ESP0+0x0c)
 *   3003539d  EBP = arg4   ([ESP+0x4c] -> ESP0+0x14, origin)
 *   300353a2  ESI = arg6   ([ESP+0x58] -> ESP0+0x1c, handle)
 *   300353a7  EDI = arg3   ([ESP+0x50] -> ESP0+0x10, flags)
 *   300353ab..300353b7  pushes (program order): ESI, 0, EDI, EBX, EBP, EAX,
 *               &result (LEA [ESP+0x28] -> ESP0-0x30), 0x28
 *   300353b9  CALL [cgame_syscall] with C-arg order (reversed pushes):
 *               cgame_syscall(0x28, &result, arg1, origin, arg2, flags, 0, handle)
 *   300353bf  FLD  float [ESP+0x30]            ST0 = result.fraction
 *   300353c3  FLD  double [0x3007bcf8]         ST0 = 1.0, ST1 = fraction
 *   300353c9  ADD ESP,0x20  (cdecl cleanup of the 8 pushed syscall dwords)
 *   300353cc  FUCOMPP  compares ST0(1.0) vs ST1(fraction), pops both
 *   300353ce  MOV word [result+0x28], 0x3fe   default 1022
 *   300353d5  FNSTSW AX ; TEST AH,0x44 ; JP  -> parity of the C3|C2 bits:
 *               fraction == 1.0  => C3=1,C2=0 => PF=0 => fall through
 *               otherwise/NaN    => even bits => PF=1 => JP taken (skip)
 *   300353dc  MOV word [result+0x28], 0x3ff   1023, only when fraction == 1.0
 *             => result.entityNum = (fraction == 1.0f) ? ENTITYNUM_NONE : ENTITYNUM_WORLD
 *   300353e3  EAX = arg5   ([ESP+0x58] -> ESP0+0x18)
 *   300353e7  ECX = arg1   ([ESP+0x48] -> ESP0+0x08)
 *   300353eb  EDX = &result (LEA [ESP+0x10] -> ESP0-0x30)
 *   300353ef..300353f7  pushes (program order): EDX(&result), 1, ESI(arg6/handle),
 *               EAX(arg5), EBP(arg4/origin), EDI(arg3/flags), EBX(arg2), ECX(arg1)
 *   300353f8  CALL 0x300350d0 with C-arg order (reversed pushes):
 *               CG_ClipMoveToEntities(arg1, arg2, flags, origin, arg5, handle, 1, &result)
 *   300353fd  EDI = out ([ESP+0x64] -> ESP0+0x04)
 *   30035401  ADD ESP,0x20  (cdecl cleanup of the 8 pushed helper dwords)
 *   30035404  ECX = 12
 *   30035409  ESI = &result
 *   3003540d  REP MOVSD  (DF=0) copies 12 dwords = 48 bytes from result to *out
 */
void CG_TraceCapsule(trace_t *out, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int32_t passEntityNum,
                     int32_t contentMask)
{
    trace_t result;

    cgame_syscall(CG_CM_CAPSULE_TRACE, (intptr_t)&result, (intptr_t)start, (intptr_t)end, (intptr_t)mins, (intptr_t)maxs, 0, contentMask);

    if (result.fraction == doubleOne) {
        result.entityNum = ENTITYNUM_NONE;
    } else {
        result.entityNum = ENTITYNUM_WORLD;
    }

    /* This twin passes 1 (not 0) in the marks helper's next-to-last slot. */
    CG_ClipMoveToEntities(start, mins, maxs, end, passEntityNum, contentMask, 1, (trace_t *)&result);

    *out = result;
}
