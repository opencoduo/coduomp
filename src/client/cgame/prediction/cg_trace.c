// Source: uo_cgame_mp_x86.dll 0x30035310..0x3003538d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30035310_3003538d.mcode

#include "client/cgame/client_recovered.h"

#include <stddef.h>

/* Layout guards for the trap-filled result: the two individually-accessed fields
 * (fraction at +0x00, entityNum at +0x28) and the 48-byte total the final REP MOVSD
 * (ECX=12 dwords) copies out. Proven at the i386 target width. */
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(offsetof(trace_t, fraction) == 0x00,
               "trace_t.fraction at +0x00");
_Static_assert(offsetof(trace_t, entityNum) == 0x28,
               "trace_t.entityNum at +0x28");
_Static_assert(sizeof(trace_t) == 0x30,
               "trace_t is 48 bytes (12 dwords copied by REP MOVSD)");
#endif

/*
 * CG_Trace (0x30035310) — a thin cgame wrapper around the trace/
 * collision trap CG_CM_BOX_TRACE (0x26). It runs the trap into a local 48-byte
 * trace_t, tags the result by whether the trace reached the far end
 * (fraction == 1.0f), refines it against solid centities with
 * CG_ClipMoveToEntities, then copies the whole 48-byte struct out to the caller.
 *
 * Name: the .mcode-assigned "G_UpdateHudElemsToClients" is REJECTED — that is a
 * server G_* symbol, while this function is pure client cgame code (it calls
 * through cgame_syscall at 0x30085e9c and the client collision helper at 0x300350d0).
 * The name here is provisional-by-role; the exact source symbol is unconfirmed.
 *
 * Custom register ABI (proven from the call site at 0x30020e29):
 *   EAX = model handle (int)      -> saved to ESI at 0x3003531a
 *   ECX = origin (vec3_t *)       -> saved to EDI at 0x30035325
 *   EBX = flags (int)             -> used as-is (an incoming register arg; never
 *                                    written in this function)
 *   [ESP+4]  arg0 = out           (trace_t *, destination of the copy)
 *   [ESP+8]  arg1 = int
 *   [ESP+0xc] arg2 = int
 *   [ESP+0x10] arg3 = int
 * The function ends with a plain RET, so the four stack dwords are caller-cleaned
 * (cdecl for the stack portion). EBP/ESI/EDI are callee-saved and restored.
 *
 * Machine-code facts proven for every behavior-affecting statement:
 *   3003531a  ESI = handle (incoming EAX)
 *   30035325  EDI = origin (incoming ECX)
 *   30035314  EBP = arg2   ([ESP+0x40] resolves to entry+0x0c)
 *   3003531c  EAX = arg1   ([ESP+0x44] resolves to entry+0x08)
 *   30035329  ECX = &result (LEA of the 0x30-byte stack local, entry-0x30)
 *   30035320..3003532e  pushes (program order): ESI, 0, EBX, EBP, EDI, EAX, ECX, 0x26
 *   30035330  CALL [cgame_syscall] with, in C-arg order after the reversed pushes:
 *               cgame_syscall(0x26, &result, arg1, origin, arg2, flags, 0, handle)
 *   30035336  FLD  float [result+0]            ST0 = result.fraction
 *   3003533a  FLD  double [0x3007bcf8]         ST0 = 1.0, ST1 = fraction
 *   30035340  ADD ESP,0x20  (cdecl cleanup of the 8 pushed syscall dwords)
 *   30035343  FUCOMPP  compares ST0(1.0) vs ST1(fraction), pops both
 *   30035345  MOV word [result+0x28], 0x3fe   default 1022
 *   3003534c  FNSTSW AX ; TEST AH,0x44 ; JP  -> parity of the C3|C2 bits:
 *               fraction == 1.0  => C3=1,C2=0 => PF=0 => fall through
 *               otherwise/NaN    => even bits => PF=1 => JP taken (skip)
 *   30035353  MOV word [result+0x28], 0x3ff   1023, only when fraction == 1.0
 *             => result.entityNum = (fraction == 1.0f) ? ENTITYNUM_NONE : ENTITYNUM_WORLD
 *   3003535a  EAX = passEntityNum ([ESP+0x4c] -> entry+0x10)
 *   3003535e  ECX = start ([ESP+0x44] -> entry+0x08)
 *   30035362  EDX = &result (LEA entry-0x30)
 *   30035366..3003536e  pushes (program order): EDX, 0, ESI, EAX, EDI, EBX, EBP, ECX
 *   3003536f  CALL 0x300350d0 with C-arg order (reversed pushes):
 *               CG_ClipMoveToEntities(start, mins, maxs, end,
 *                                     passEntityNum, contentMask, 0, &result)
 *   30035374  EDI = out ([ESP+0x60] -> entry+0x04)
 *   30035378  ADD ESP,0x20  (cdecl cleanup of the 8 pushed helper dwords)
 *   3003537b  ECX = 12
 *   30035380  ESI = &result
 *   30035384  REP MOVSD  (DF=0) copies 12 dwords = 48 bytes from result to *out
 */
void CG_Trace(int32_t contentMask, const vec3_t end, const vec3_t maxs,
              trace_t *out, const vec3_t start, const vec3_t mins,
              int32_t passEntityNum)
{
    trace_t result;

    cgame_syscall(CG_CM_BOX_TRACE, (intptr_t)&result, (intptr_t)start,
                  (intptr_t)end, (intptr_t)mins, (intptr_t)maxs,
                  0, contentMask);

    if (result.fraction == doubleOne) {
        result.entityNum = ENTITYNUM_NONE;  /* fraction == 1.0f: reached far end */
    } else {
        result.entityNum = ENTITYNUM_WORLD;
    }

    CG_ClipMoveToEntities(start, mins, maxs, end, passEntityNum,
                          contentMask, 0, &result);

    *out = result;
}
