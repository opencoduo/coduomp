// Source: uo_cgame_mp_x86.dll 0x3001f710..0x3001f75f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001f710_3001f75f.mcode
//
// CG_SetFrameInterpolation — compute cg.frameInterpolation, the [0,1) fraction
// used to interpolate entity/view state between the current snapshot (cg_snap)
// and the incoming snapshot (cg_nextSnap) for the current client time.
//
//   frameInterpolation = (float)(cg.time - cg_snap->serverTime)
//                        / (cg_nextSnap->serverTime - cg_snap->serverTime);
//
// with two clamps proven by the machine code:
//   - if the snapshot span (nextSnap - snap serverTime) is 0, result = 0;
//   - if the computed fraction is negative, result = 0.
//
// The mechanical name guess `Com_ParseFloat` (from a size-only 0x4f corpus match)
// is REJECTED: this reads cg_snap / cg_nextSnap serverTimes and cg.time and writes
// the float cg_frameInterpolation datum; it does not parse a float from text.
// The Mac CG_SetFrameInterpolation computes and stores this same snapshot fraction,
// resolving the source name.
//
// Machine-code facts:
//   - EAX = cg_snap->serverTime (MOV EAX,[0x30459160]; MOV EAX,[EAX+0x8]).
//   - ECX = cg_nextSnap->serverTime (MOV ECX,[0x30459164]; MOV EDX,[ECX+0x8]).
//   - EDX = nextSnap->serverTime - snap->serverTime, spilled to [ESP+4] (divisor).
//     JZ (SUB sets ZF) short-circuits straight to the zeroing path when span == 0.
//   - ECX = cg.time (cg_time, [0x304831b0]) - snap->serverTime, spilled to [ESP]
//     (dividend).
//   - FILD [ESP] (signed 32-bit integer load) / FIDIV [ESP+4] (divide by signed
//     32-bit integer): both operands are treated as signed ints, so use int32_t
//     subtraction and an int->float divide.
//   - FST [0x304831a8]: store the single-precision fraction to cg_frameInterpolation
//     WITHOUT popping (value stays on the x87 stack for the compare).
//   - FCOMP [0x3007bcec] then FNSTSW AX; TEST AH,0x5; JP. 0x3007bcec is the shared
//     .rdata 0.0f (bytes 00 00 00 00; the adjacent 0x3007bce8 is 0.5f — not used).
//     TEST AH,0x5 masks C0 (bit0) and C2 (bit2); JP jumps when their popcount is
//     even. For a real (non-NaN) fraction vs 0.0: fraction >= 0 -> C0=0 -> mask=0
//     -> even -> JP taken (skip zeroing, keep the stored fraction); fraction < 0
//     -> C0=1 -> mask=1 -> odd -> fall through and overwrite with 0. FIDIV of a
//     finite int by a nonzero finite int cannot produce NaN, so the net rule is
//     simply: clamp a negative fraction to 0.

#include "client/cgame/globals.h"

#include <stdint.h>
#include <string.h>

void CG_SetFrameInterpolation(void)
{
    /* SUB is a modulo-2^32 operation. Interpret the resulting bits as signed
     * only at the FILD/FIDIV boundary, matching Win32 x86 even when the
     * mathematical subtraction is outside the int32_t range. */
    uint32_t snapSpanBits =
        (uint32_t)cg_nextSnap->serverTime - (uint32_t)cg_snap->serverTime;
    int32_t snapSpan;
    memcpy(&snapSpan, &snapSpanBits, sizeof(snapSpan));

    if (snapSpan == 0) {
        /* SUB set ZF -> JZ to the zeroing store (MOV [0x304831a8],0). */
        cg_frameInterpolation = 0.0f;
        return;
    }

    uint32_t elapsedBits =
        (uint32_t)cg_time - (uint32_t)cg_snap->serverTime;
    int32_t elapsed;
    memcpy(&elapsed, &elapsedBits, sizeof(elapsed));

    /* FILD elapsed / FIDIV snapSpan, then FST (non-popping) into the datum. */
    double fraction = (double)elapsed / (double)snapSpan;
    cg_frameInterpolation = (float)fraction;

    /* FCOMP against 0.0f + TEST AH,0x5 + JP: clamp a negative fraction to 0. */
    if (fraction < 0.0f) {
        cg_frameInterpolation = 0.0f;
    }
}
